/**
 * @file llgesture.cpp
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "boost/tokenizer.hpp"

#include "indra_constants.h"
#include "llgesture.h"
#include "llstl.h"
#include "llmessage.h"

constexpr S32 MAX_GESTURES = 4096;

LLGesture::LLGesture()
:	mKey(KEY_NONE),
	mMask(MASK_NONE),
	mTrigger(),
	mTriggerLower(),
	mSoundItemID(),
	mAnimation(),
	mOutputString()
{
}

LLGesture::LLGesture(KEY key, MASK mask, const std::string& trigger,
					 const LLUUID& sound_item_id,
					 const std::string& animation,
					 const std::string& output_string)
:	mKey(key),
	mMask(mask),
	mTrigger(trigger),
	mTriggerLower(trigger),
	mSoundItemID(sound_item_id),
	mAnimation(animation),
	mOutputString(output_string)
{
	mTriggerLower = utf8str_tolower(mTriggerLower);
}

LLGesture::LLGesture(U8** buffer, S32 max_size)
{
	*buffer = deserialize(*buffer, max_size);
}

LLGesture::LLGesture(const LLGesture& rhs)
{
	mKey			= rhs.mKey;
	mMask			= rhs.mMask;
	mTrigger		= rhs.mTrigger;
	mTriggerLower	= rhs.mTriggerLower;
	mSoundItemID	= rhs.mSoundItemID;
	mAnimation		= rhs.mAnimation;
	mOutputString	= rhs.mOutputString;
}

const LLGesture& LLGesture::operator=(const LLGesture& rhs)
{
	mKey			= rhs.mKey;
	mMask			= rhs.mMask;
	mTrigger		= rhs.mTrigger;
	mTriggerLower	= rhs.mTriggerLower;
	mSoundItemID	= rhs.mSoundItemID;
	mAnimation		= rhs.mAnimation;
	mOutputString	= rhs.mOutputString;
	return (*this);
}

bool LLGesture::trigger(KEY key, MASK mask)
{
	llwarns << "Parent class trigger called: you probably didn't mean this."
			<< llendl;
	return false;
}

bool LLGesture::trigger(const std::string& trigger_string)
{
	llwarns << "Parent class trigger called: you probably didn't mean this."
			<< llendl;
	return false;
}

// NOT endian-neutral
U8* LLGesture::serialize(U8* buffer) const
{
	htonmemcpy(buffer, &mKey, MVT_S8, 1);
	buffer += sizeof(mKey);
	htonmemcpy(buffer, &mMask, MVT_U32, 4);
	buffer += sizeof(mMask);
	htonmemcpy(buffer, mSoundItemID.mData, MVT_LLUUID, 16);
	buffer += 16;

	memcpy(buffer, mTrigger.c_str(), mTrigger.length() + 1);
	buffer += mTrigger.length() + 1;
	memcpy(buffer, mAnimation.c_str(), mAnimation.length() + 1);
	buffer += mAnimation.length() + 1;
	memcpy(buffer, mOutputString.c_str(), mOutputString.length() + 1);
	buffer += mOutputString.length() + 1;

	return buffer;
}

U8* LLGesture::deserialize(U8* buffer, S32 max_size)
{
	U8* tmp = buffer;

	if (tmp + sizeof(mKey) + sizeof(mMask) + 16 > buffer + max_size)
	{
		llwarns << "Attempt to read past end of buffer, bad data !" << llendl;
		return buffer;
	}

	htonmemcpy(&mKey, tmp, MVT_S8, 1);
	tmp += sizeof(mKey);
	htonmemcpy(&mMask, tmp, MVT_U32, 4);
	tmp += sizeof(mMask);
	htonmemcpy(mSoundItemID.mData, tmp, MVT_LLUUID, 16);
	tmp += 16;

	mTrigger.assign((char *)tmp);
	mTriggerLower = mTrigger;
	mTriggerLower = utf8str_tolower(mTriggerLower);
	tmp += mTrigger.length() + 1;
	mAnimation.assign((char *)tmp);
	//RN: force animation names to lower case
	// must do this for backwards compatibility
	mAnimation = utf8str_tolower(mAnimation);
	tmp += mAnimation.length() + 1;
	mOutputString.assign((char *)tmp);
	tmp += mOutputString.length() + 1;

	if (tmp > buffer + max_size)
	{
		llwarns << "Read past end of buffer, bad data !" << llendl;
		return tmp;
	}

	return tmp;
}

S32 LLGesture::getMaxSerialSize()
{
	return MAX_SERIAL_SIZE;
}

//---------------------------------------------------------------------
// LLGestureList
//---------------------------------------------------------------------

LLGestureList::LLGestureList()
:	mList(0)
{
}

LLGestureList::~LLGestureList()
{
	deleteAll();
}

void LLGestureList::deleteAll()
{
	delete_and_clear(mList);
}

// Iterates through space delimited tokens in string, triggering any gestures
// found. Generates a revised string that has the found tokens replaced by
// their replacement strings and (as a minor side effect) has multiple spaces
// in a row replaced by single spaces.
bool LLGestureList::triggerAndReviseString(const std::string& string,
										   std::string* revised_string)
{
	std::string tokenized = string;

	bool found_gestures = false;
	bool first_token = true;

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");
	tokenizer tokens(string, sep);
	tokenizer::iterator token_iter;

	for (token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
	{
		LLGesture* gesture = NULL;

		if (!found_gestures) // Only pay attention to the first gesture in the string.
		{
			std::string cur_token_lower = *token_iter;
			LLStringUtil::toLower(cur_token_lower);

			for (S32 i = 0, count = mList.size(); i < count; ++i)
			{
				gesture = mList[i];
				if (gesture && gesture->trigger(cur_token_lower))
				{
					if (!gesture->getOutputString().empty())
					{
						if (!first_token)
						{
							revised_string->append(" ");
						}

						// Do not muck with the user's capitalization if we
						// do not have to.
						const std::string& output = gesture->getOutputString();
						std::string output_lower = std::string(output.c_str());
						LLStringUtil::toLower(output_lower);
						if (cur_token_lower == output_lower)
						{
							revised_string->append(*token_iter);
						}
						else
						{
							revised_string->append(output);
						}

					}
					found_gestures = true;
					break;
				}
				gesture = NULL;
			}
		}

		if (!gesture)
		{
			if (!first_token)
			{
				revised_string->append(" ");
			}
			revised_string->append(*token_iter);
		}

		first_token = false;
	}
	return found_gestures;
}

bool LLGestureList::trigger(KEY key, MASK mask)
{
	for (S32 i = 0, count = mList.size(); i < count; ++i)
	{
		LLGesture* gesture = mList[i];
		if (gesture)
		{
			if (gesture->trigger(key, mask))
			{
				return true;
			}
		}
		else
		{
			llwarns << "NULL gesture in gesture list (" << i << ")" << llendl;
		}
	}
	return false;
}

// NOT endian-neutral
U8* LLGestureList::serialize(U8* buffer) const
{
	// a single S32 serves as the header that tells us how many to read
	S32 count = mList.size();
	htonmemcpy(buffer, &count, MVT_S32, 4);
	buffer += sizeof(count);

	for (S32 i = 0; i < count; i++)
	{
		buffer = mList[i]->serialize(buffer);
	}

	return buffer;
}

U8* LLGestureList::deserialize(U8* buffer, S32 max_size)
{
	deleteAll();

	S32 count;
	U8* tmp = buffer;

	if (tmp + sizeof(count) > buffer + max_size)
	{
		llwarns << "Invalid max_size" << llendl;
		return buffer;
	}

	htonmemcpy(&count, tmp, MVT_S32, 4);

	if (count > MAX_GESTURES)
	{
		llwarns << "Unreasonably large gesture list count in deserialize: "
				<< count << llendl;
		return tmp;
	}

	tmp += sizeof(count);

	mList.resize(count);

	for (S32 i = 0; i < count; i++)
	{
		mList[i] = create_gesture(&tmp, max_size - (S32)(tmp - buffer));
		if (tmp - buffer > max_size)
		{
			llwarns << "Deserialization read past end of buffer, bad data !"
					<< llendl;
			return tmp;
		}
	}

	return tmp;
}

// This is a helper for deserialize: it gets overridden by LLViewerGestureList
// to create LLViewerGestures overridden by child class to use local LLGesture
// implementation
LLGesture* LLGestureList::create_gesture(U8** buffer, S32 max_size)
{
	return new LLGesture(buffer, max_size);
}

S32 LLGestureList::getMaxSerialSize()
{
	return SERIAL_HEADER_SIZE + count() * LLGesture::getMaxSerialSize();
}
