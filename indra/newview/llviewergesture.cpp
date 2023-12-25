/**
 * @file llviewergesture.cpp
 * @brief LLViewerGesture class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llviewergesture.h"

#include "llanimationstates.h"
#include "llaudioengine.h"
#include "lldir.h"
#include "sound_ids.h"			// For testing

#include "llagent.h"
#include "llchatbar.h"
#include "llkeyboard.h"			// For key shortcuts for testing
#include "llgridmanager.h"
#include "llinventorymodel.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewerinventory.h"
#include "llviewermessage.h"	// For send_sound_trigger()
#include "llvoavatar.h"
#include "llxfermanager.h"

// Globals
LLViewerGestureList gGestureList;

constexpr F32 SOUND_VOLUME = 1.f;

LLViewerGesture::LLViewerGesture()
:	LLGesture()
{
}

LLViewerGesture::LLViewerGesture(KEY key, MASK mask, const std::string& trigger,
								 const LLUUID& sound_item_id,
								 const std::string &animation,
								 const std::string &output_string)
:	LLGesture(key, mask, trigger, sound_item_id, animation, output_string)
{
}

LLViewerGesture::LLViewerGesture(U8** buffer, S32 max_size)
:	LLGesture(buffer, max_size)
{
}

LLViewerGesture::LLViewerGesture(const LLViewerGesture& rhs)
:	LLGesture((LLGesture)rhs)
{
}

bool LLViewerGesture::trigger(KEY key, MASK mask)
{
	if (mKey == key && mMask == mask)
	{
		doTrigger(true);
		return true;
	}
	return false;
}

bool LLViewerGesture::trigger(const std::string& trigger_string)
{
	// Assumes trigger_string is lowercase
	if (mTriggerLower == trigger_string)
	{
		doTrigger(false);
		return true;
	}
	return false;
}

void LLViewerGesture::doTrigger(bool send_chat)
{
	if (mSoundItemID.notNull())
	{
		LLViewerInventoryItem* item;
		item = gInventory.getItem(mSoundItemID);
		if (item)
		{
			send_sound_trigger(item->getAssetUUID(), SOUND_VOLUME);
		}
	}

	if (!mAnimation.empty())
	{
		// AFK animations trigger the special "away" state, which
		// includes agent control settings. JC
		if (mAnimation == "enter_away_from_keyboard_state" || mAnimation == "away")
		{
			gAgent.setAFK();
		}
		else
		{
			LLUUID anim_id = gAnimLibrary.stringToAnimState(mAnimation);
			gAgent.sendAnimationRequest(anim_id, ANIM_REQUEST_START);
		}
	}

	if (send_chat && !mOutputString.empty())
	{
		// Don't play nodding animation, since that might not blend
		// with the gesture animation.
//MK
		if (!gRLenabled || !gRLInterface.contains("sendchat"))
		{
//mk
			if (gChatBarp)
			{
				gChatBarp->sendChatFromViewer(mOutputString, CHAT_TYPE_NORMAL,
											  false);
			}
//MK
		}
//mk
	}
}

LLViewerGestureList::LLViewerGestureList()
:	LLGestureList()
{
}

// helper for deserialize that creates the right LLGesture subclass
LLGesture* LLViewerGestureList::create_gesture(U8** buffer, S32 max_size)
{
	return new LLViewerGesture(buffer, max_size);
}

// Sees if the prefix matches any gesture. If so, returns true and place the
// full text of the gesture trigger into output_str
bool LLViewerGestureList::matchPrefix(const std::string& in_str,
									  std::string* out_str)
{
	S32 in_len = in_str.length();

	std::string in_str_lc = in_str;
	LLStringUtil::toLower(in_str_lc);

	for (S32 i = 0; i < count(); ++i)
	{
		LLGesture* gesture = get(i);
		const std::string &trigger = gesture->getTrigger();

		if (in_len > (S32)trigger.length())
		{
			// too short, bail out
			continue;
		}

		std::string trigger_trunc = utf8str_truncate(trigger, in_len);
		LLStringUtil::toLower(trigger_trunc);
		if (in_str_lc == trigger_trunc)
		{
			*out_str = trigger;
			return true;
		}
	}
	return false;
}

// static
void LLViewerGestureList::xferCallback(void* data, S32 size, void**,
									   S32 status)
{
	if (status == LL_ERR_NOERR)
	{
		U8* buffer = (U8*)data;
		U8* end = gGestureList.deserialize(buffer, size);
		if (end - buffer > size)
		{
			llerrs << "Read off of end of array, error in serialization"
				   << llendl;
		}
	}
	else
	{
		llwarns << "Unable to load gesture list !" << llendl;
	}
}
