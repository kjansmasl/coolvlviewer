/**
 * @file llaudiosourcevo.cpp
 * @author Douglas Soo, James Cook
 * @brief Audio sources attached to viewer objects
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llaudiosourcevo.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "llmutelist.h"
#include "llviewercontrol.h"		// For gSavedSettings
#include "llviewerparcelmgr.h"

// Update mutes at most every half of a second
constexpr F32 UPDATE_INTERVAL = 0.5f;

LLAudioSourceVO::LLAudioSourceVO(const LLUUID& sound_id,
								 const LLUUID& owner_id, F32 gain,
								 LLViewerObject* objectp)
:	LLAudioSource(sound_id, owner_id, gain, LLAudioEngine::AUDIO_TYPE_SFX),
	mObjectp(objectp),
	mLastUpdate(0.f)
{
	update();
}

LLAudioSourceVO::~LLAudioSourceVO()
{
	if (mObjectp)
	{
		mObjectp->clearAttachedSound();
	}
	mObjectp = NULL;
}

void LLAudioSourceVO::setGain(F32 gain)
{
	mGain = llclamp(gain, 0.f, 1.f);
}

bool LLAudioSourceVO::isInCutOffRadius(const LLVector3d& pos_global,
									   F32 cutoff) const
{
	static LLCachedControl<S32> ear_mode(gSavedSettings, "VoiceEarLocation");

	LLVector3d to_vec;
	if (ear_mode == 1 || ear_mode == 2)
	{
		to_vec = pos_global - gAgent.getPositionGlobal();
	}
	else
	{
		to_vec = pos_global - gAgent.getCameraPositionGlobal();
	}
	return (F32)to_vec.lengthSquared() < cutoff * cutoff;
}

void LLAudioSourceVO::checkCutOffRadius()
{
	if (mSourceMuted || !mObjectp) return;

	F32 cutoff = mObjectp->getSoundCutOffRadius();
	if (cutoff < 0.1f)
	{
		// Consider cutoff below 0.1m as off (to avoid near zero comparison)
		return;
	}

	LLViewerObject* objectp = mObjectp;
	if (objectp->isAttachment())
	{
		while (objectp && !objectp->isAvatar())
		{
			objectp = (LLViewerObject*)objectp->getParent();
		}
	}
	if (objectp && !isInCutOffRadius(objectp->getPositionGlobal(), cutoff))
	{
		mSourceMuted = true;
	}
}

void LLAudioSourceVO::updateMute()
{
	if (!mObjectp) return;	// Paranoia

	bool is_attachment = mObjectp->isAttachment();
	LLViewerObject* parent = mObjectp;
	if (is_attachment)
	{
		while (parent && !parent->isAvatar())
		{
			parent = (LLViewerObject*)parent->getParent();
		}
	}

	bool mute = mCurrentDatap && mCurrentDatap->isBlocked();

	static LLCachedControl<bool> play_attached(gSavedSettings,
											   "EnableAttachmentSounds");
	if (!mute && is_attachment && !play_attached && parent &&
		parent->getID() != gAgentID)
	{
		mute = true;
	}

	if (!mute)
	{
		LLVector3d pos_global;
		if (parent)
		{
			pos_global = parent->getPositionGlobal();
		}
		else
		{
			pos_global = mObjectp->getPositionGlobal();
		}
		if (!gViewerParcelMgr.canHearSound(pos_global))
		{
			mute = true;
		}
		else
		{
			F32 cutoff = mObjectp->getSoundCutOffRadius();
			if (cutoff > 0.1f && !isInCutOffRadius(pos_global, cutoff))
			{
				mute = true;
			}
		}
	}

	if (!mute)
	{
		if (LLMuteList::isMuted(mObjectp->getID()))
		{
			mute = true;
		}
		else if (LLMuteList::isMuted(mOwnerID, LLMute::flagObjectSounds))
		{
			mute = true;
		}
		else if (is_attachment && parent &&
				 LLMuteList::isMuted(parent->getID()))
		{
			mute = true;
		}
	}

	if (mute != mSourceMuted)
	{
		mSourceMuted = mute;
		if (mSourceMuted)
		{
		  	// Stop the sound.
			play(LLUUID::null);
		}
		else
		{
		  	// Muted sounds keep their data at all times, because it is the
			// place where the audio UUID is stored. However, it is possible
			// that mCurrentDatap is NULL when this source did only preload
			// sounds.
			if (mCurrentDatap)
			{
		  		// Restart the sound.
				play(mCurrentDatap->getID());
			}
		}
	}
}

void LLAudioSourceVO::update()
{
	if (!mObjectp || mObjectp->isDead())
	{
		mObjectp = NULL;
	  	mSourceMuted = true;
		return;
	}

	if (mLastUpdate + UPDATE_INTERVAL < gFrameTimeSeconds)
	{
		updateMute();
		mLastUpdate = gFrameTimeSeconds;
	}

	if (mSourceMuted)
	{
	  	return;
	}

	if (mObjectp->isHUDAttachment())
	{
		mPositionGlobal = gAgent.getCameraPositionGlobal();
	}
	else
	{
		mPositionGlobal = mObjectp->getPositionGlobal();
	}
	if (mObjectp->getSubParent())
	{
		mVelocity = mObjectp->getSubParent()->getVelocity();
	}
	else
	{
		mVelocity = mObjectp->getVelocity();
	}

	LLAudioSource::update();
}
