/**
 * @file llmultigesture.cpp
 * @brief Gestures that are asset-based and can have multiple steps.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <algorithm>

#include "stdio.h"

#include "llmultigesture.h"

#include "lldatapacker.h"
#include "llstl.h"

constexpr S32 GESTURE_VERSION = 2;

//---------------------------------------------------------------------------
// LLMultiGesture
//---------------------------------------------------------------------------
LLMultiGesture::LLMultiGesture()
:	mKey(),
	mMask(),
	mTrigger(),
	mReplaceText(),
	mSteps(),
	mPlaying(false),
	mCurrentStep(0),
	mDoneCallback(NULL),
	mCallbackData(NULL)
{
	reset();
}

LLMultiGesture::~LLMultiGesture()
{
	std::for_each(mSteps.begin(), mSteps.end(), DeletePointer());
	mSteps.clear();
}

void LLMultiGesture::reset()
{
	mPlaying = false;
	mCurrentStep = 0;
	mWaitTimer.reset();
	mWaitingTimer = false;
	mWaitingAnimations = false;
	mWaitingAtEnd = false;
	mRequestedAnimIDs.clear();
	mPlayingAnimIDs.clear();
}

S32 LLMultiGesture::getMaxSerialSize() const
{
	S32 max_size = 0;

	// ASCII format, being very conservative about possible label lengths.
	max_size += 64;		// Version S32
	max_size += 64;		// Key U8
	max_size += 64;		// Mask U32
	max_size += 256;	// Trigger string
	max_size += 256;	// Replace string

	max_size += 64;		// Step count S32

	std::vector<LLGestureStep*>::const_iterator it;
	for (it = mSteps.begin(); it != mSteps.end(); ++it)
	{
		LLGestureStep* step = *it;
		max_size += 64;	// type S32
		max_size += step->getMaxSerialSize();
	}

	return max_size;
}

bool LLMultiGesture::serialize(LLDataPacker& dp) const
{
	dp.packS32(GESTURE_VERSION, "version");
	dp.packU8(mKey, "key");
	dp.packU32(mMask, "mask");
	dp.packString(mTrigger, "trigger");
	dp.packString(mReplaceText, "replace");

	S32 count = (S32)mSteps.size();
	dp.packS32(count, "step_count");
	for (S32 i = 0; i < count; ++i)
	{
		LLGestureStep* step = mSteps[i];

		dp.packS32(step->getType(), "step_type");
		if (!step->serialize(dp))
		{
			return false;
		}
	}
	return true;
}

bool LLMultiGesture::deserialize(LLDataPacker& dp)
{
	S32 version;
	dp.unpackS32(version, "version");
	if (version != GESTURE_VERSION)
	{
		llwarns << "Bad LLMultiGesture version " << version << " should be "
				<< GESTURE_VERSION << llendl;
		return false;
	}

	dp.unpackU8(mKey, "key");
	dp.unpackU32(mMask, "mask");

	dp.unpackString(mTrigger, "trigger");

	dp.unpackString(mReplaceText, "replace");

	S32 count;
	dp.unpackS32(count, "step_count");
	if (count < 0)
	{
		llwarns << "Bad LLMultiGesture step count " << count << llendl;
		return false;
	}

	for (S32 i = 0; i < count; ++i)
	{
		S32 type;
		dp.unpackS32(type, "step_type");

		EStepType step_type = (EStepType)type;
		switch (step_type)
		{
			case STEP_ANIMATION:
			{
				LLGestureStepAnimation* step = new LLGestureStepAnimation();
				if (!step->deserialize(dp))
				{
					delete step;
					return false;
				}
				mSteps.push_back(step);
				break;
			}

			case STEP_SOUND:
			{
				LLGestureStepSound* step = new LLGestureStepSound();
				if (!step->deserialize(dp))
				{
					delete step;
					return false;
				}
				mSteps.push_back(step);
				break;
			}

			case STEP_CHAT:
			{
				LLGestureStepChat* step = new LLGestureStepChat();
				if (!step->deserialize(dp))
				{
					delete step;
					return false;
				}
				mSteps.push_back(step);
				break;
			}

			case STEP_WAIT:
			{
				LLGestureStepWait* step = new LLGestureStepWait();
				if (!step->deserialize(dp))
				{
					delete step;
					return false;
				}
				mSteps.push_back(step);
				break;
			}

			default:
			{
				llwarns << "Bad LLMultiGesture step type " << type << llendl;
				return false;
			}
		}
	}

	return true;
}

void LLMultiGesture::dump()
{
	llinfos << "key " << S32(mKey) << " mask " << U32(mMask) << " trigger "
			<< mTrigger << " replace " << mReplaceText << llendl;
	for (U32 i = 0, count = mSteps.size(); i < count; ++i)
	{
		LLGestureStep* step = mSteps[i];
		if (step)
		{
			step->dump();
		}
		else
		{
			llwarns << "NULL step #" << i << llendl;
		}
	}
}

//---------------------------------------------------------------------------
// LLGestureStepAnimation
//---------------------------------------------------------------------------
LLGestureStepAnimation::LLGestureStepAnimation()
:	LLGestureStep(),
	mAnimName("None"),
	mAnimAssetID(),
	mFlags(0x0)
{
}

S32 LLGestureStepAnimation::getMaxSerialSize() const
{
#if 1
	// ASCII
	// 256 for anim name, 64 for asset Id, 64 for flags
	return 256 + 64 + 64;
#else
	// Binary
	S32 max_size = mAnimName.length() + 1;
	max_size += sizeof(mAnimAssetID);
	max_size += sizeof(mFlags);
	return max_size;
#endif
}

bool LLGestureStepAnimation::serialize(LLDataPacker& dp) const
{
	dp.packString(mAnimName, "anim_name");
	dp.packUUID(mAnimAssetID, "asset_id");
	dp.packU32(mFlags, "flags");
	return true;
}

bool LLGestureStepAnimation::deserialize(LLDataPacker& dp)
{
	dp.unpackString(mAnimName, "anim_name");

	// Apparently an earlier version of the gesture code added \r to the end
	// of the animation names.  Get rid of it.  JC
	if (!mAnimName.empty() && mAnimName[mAnimName.length() - 1] == '\r')
	{
		// Chop the last character
		mAnimName.resize(mAnimName.length() - 1);
	}

	dp.unpackUUID(mAnimAssetID, "asset_id");
	dp.unpackU32(mFlags, "flags");
	return true;
}

std::string LLGestureStepAnimation::getLabel() const
{
	std::string label;
	if (mFlags & ANIM_FLAG_STOP)
	{
		label = "Stop Animation: ";
	}
	else
	{
		label = "Start Animation: ";
	}
	label += mAnimName;
	return label;
}

void LLGestureStepAnimation::dump()
{
	llinfos << "step animation " << mAnimName << " id " << mAnimAssetID
			<< " flags " << mFlags << llendl;
}

//---------------------------------------------------------------------------
// LLGestureStepSound
//---------------------------------------------------------------------------
LLGestureStepSound::LLGestureStepSound()
:	LLGestureStep(),
	mSoundName("None"),
	mSoundAssetID(),
	mFlags(0x0)
{
}

S32 LLGestureStepSound::getMaxSerialSize() const
{
	S32 max_size = 0;
	max_size += 256;	// sound name
	max_size += 64;		// sound asset id
	max_size += 64;		// flags
	/* binary
	max_size += mSoundName.length() + 1;
	max_size += sizeof(mSoundAssetID);
	max_size += sizeof(mFlags);
	*/
	return max_size;
}

bool LLGestureStepSound::serialize(LLDataPacker& dp) const
{
	dp.packString(mSoundName, "sound_name");
	dp.packUUID(mSoundAssetID, "asset_id");
	dp.packU32(mFlags, "flags");
	return true;
}

bool LLGestureStepSound::deserialize(LLDataPacker& dp)
{
	dp.unpackString(mSoundName, "sound_name");

	dp.unpackUUID(mSoundAssetID, "asset_id");
	dp.unpackU32(mFlags, "flags");
	return true;
}

std::string LLGestureStepSound::getLabel() const
{
	std::string label("Sound: ");
	label += mSoundName;
	return label;
}

void LLGestureStepSound::dump()
{
	llinfos << "step sound " << mSoundName << " id " << mSoundAssetID
			<< " flags " << mFlags << llendl;
}

//---------------------------------------------------------------------------
// LLGestureStepChat
//---------------------------------------------------------------------------
LLGestureStepChat::LLGestureStepChat()
:	LLGestureStep(),
	mChatText(),
	mFlags(0x0)
{
}

S32 LLGestureStepChat::getMaxSerialSize() const
{
	S32 max_size = 0;
	max_size += 256;	// chat text
	max_size += 64;		// flags
	/* binary
	max_size += mChatText.length() + 1;
	max_size += sizeof(mFlags);
	*/
	return max_size;
}

bool LLGestureStepChat::serialize(LLDataPacker& dp) const
{
	dp.packString(mChatText, "chat_text");
	dp.packU32(mFlags, "flags");
	return true;
}

bool LLGestureStepChat::deserialize(LLDataPacker& dp)
{
	dp.unpackString(mChatText, "chat_text");

	dp.unpackU32(mFlags, "flags");
	return true;
}

std::string LLGestureStepChat::getLabel() const
{
	std::string label("Chat: ");
	label += mChatText;
	return label;
}

void LLGestureStepChat::dump()
{
	llinfos << "step chat " << mChatText << " flags " << mFlags << llendl;
}

//---------------------------------------------------------------------------
// LLGestureStepWait
//---------------------------------------------------------------------------
LLGestureStepWait::LLGestureStepWait()
:	LLGestureStep(),
	mWaitSeconds(0.f),
	mFlags(0x0)
{
}

S32 LLGestureStepWait::getMaxSerialSize() const
{
	S32 max_size = 0;
	max_size += 64;		// wait seconds
	max_size += 64;		// flags
	/* binary
	max_size += sizeof(mWaitSeconds);
	max_size += sizeof(mFlags);
	*/
	return max_size;
}

bool LLGestureStepWait::serialize(LLDataPacker& dp) const
{
	dp.packF32(mWaitSeconds, "wait_seconds");
	dp.packU32(mFlags, "flags");
	return true;
}

bool LLGestureStepWait::deserialize(LLDataPacker& dp)
{
	dp.unpackF32(mWaitSeconds, "wait_seconds");
	dp.unpackU32(mFlags, "flags");
	return true;
}

std::string LLGestureStepWait::getLabel() const
{
	std::string label("--- Wait: ");
	if (mFlags & WAIT_FLAG_TIME)
	{
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%.1f seconds", (double)mWaitSeconds);
		label += buffer;
	}
	else if (mFlags & WAIT_FLAG_ALL_ANIM)
	{
		label += "until animations are done";
	}

	return label;
}

void LLGestureStepWait::dump()
{
	llinfos << "step wait " << mWaitSeconds << " flags " << mFlags << llendl;
}
