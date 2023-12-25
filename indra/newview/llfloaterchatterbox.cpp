/** 
 * @file llfloaterchatterbox.cpp
 * @author Richard
 * @date 2007-05-08
 * @brief Implementation of the chatterbox integrated conversation ui
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterchatterbox.h"

#include "lluictrlfactory.h"

#include "llfloaterchat.h"
#include "llfloaterfriends.h"
#include "llfloatergroups.h"
#include "llfloaterim.h"
#include "llfloaternewim.h"
#include "llimmgr.h"
#include "llviewercontrol.h"
#include "llvoicechannel.h"

// Visibility policy for LLUISingleton

//static
bool LLFloaterChatterBox::visible(LLFloater* instance, const LLSD& key)
{
	LLFloater* floater_to_check;
	floater_to_check = ((LLFloaterChatterBox*)instance)->getFloater(key);
	if (floater_to_check)
	{
		return floater_to_check->isInVisibleChain();
	}

	// otherwise use default visibility rule for chatterbox
	return VisibilityPolicy<LLFloater>::visible(instance, key);
}

//static
void LLFloaterChatterBox::show(LLFloater* instance, const LLSD& key)
{
	LLFloater* floater_to_show = ((LLFloaterChatterBox*)instance)->getFloater(key);
	VisibilityPolicy<LLFloater>::show(instance, key);

	if (floater_to_show)
	{
		floater_to_show->open();
	}
}

//static
void LLFloaterChatterBox::hide(LLFloater* instance, const LLSD& key)
{
	VisibilityPolicy<LLFloater>::hide(instance, key);
}

LLFloater* LLFloaterChatterBox::getFloater(const LLSD& key)
{
	LLFloater* floater = NULL;

	// Try to show requested session
	LLUUID session_id = key.asUUID();
	if (session_id.notNull())
	{
		floater = LLFloaterIMSession::findInstance(session_id);
	}

	// If true, show tab for active voice channel, otherwise, just show last
	// tab
	if (key.asBoolean())
	{
		floater = getCurrentVoiceFloater();
	}

	return floater;
}

LLFloaterChatterBox::LLFloaterChatterBox(const LLSD& seed)
:	mActiveVoiceFloater(NULL),
	mFirstOpen(true)
{
	mAutoResize = false;

	if (gSavedSettings.getBool("IMTabsVerticalStacking"))
	{
		LLUICtrlFactory::getInstance()->buildFloater(this,
													 "floater_chatterbox2.xml",
													 NULL, false);
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildFloater(this,
													 "floater_chatterbox.xml",
													 NULL, false);
	}
	addFloater(mFloaterNewIM = new LLFloaterNewIM(), false);

	if (gSavedSettings.getBool("ChatHistoryTornOff"))
	{
		LLFloaterChat* floater_chat = LLFloaterChat::getInstance();
		// Add then remove to set up relationship for re-attach
		addFloater(floater_chat, false);
		removeFloater(floater_chat);
		// Reparent to floater view
		gFloaterViewp->addChild(floater_chat);
	}
	else
	{
		addFloater(LLFloaterChat::getInstance(LLSD()), false);
	}
	mTabContainer->lockTabs();
}

bool LLFloaterChatterBox::handleKeyHere(KEY key, MASK mask)
{
	if (key == 'W' && mask == MASK_CONTROL)
	{
		LLFloater* floater = getActiveFloater();
		// Is user closeable and is system closeable
		if (floater && floater->canClose())
		{
			if (floater->isCloseable())
			{
				floater->close();
			}
			else
			{
				// Close chatterbox window if frontmost tab is reserved,
				// non-closeable tab such as contacts or near me
				close();
			}
		}
		return true;
	}

	return LLMultiFloater::handleKeyHere(key, mask);
}

void LLFloaterChatterBox::draw()
{
	// Clear new im notifications when chatterbox is visible
	if (!isMinimized() && gIMMgrp) 
	{
		gIMMgrp->clearNewIMNotification();
	}
	LLFloater* current_active_floater = getCurrentVoiceFloater();
	// Set icon on tab for floater currently associated with active voice
	// channel
	if (mActiveVoiceFloater != current_active_floater)
	{
		// remove image from old floater's tab
		if (mActiveVoiceFloater)
		{
			mTabContainer->setTabImage(mActiveVoiceFloater, "");
		}
	}

	// Update image on current active tab
	if (current_active_floater)
	{
		LLColor4 icon_color = LLColor4::white;
		LLVoiceChannel* channelp = LLVoiceChannel::getCurrentVoiceChannel();
		if (channelp)
		{
			if (channelp->isActive())
			{
				icon_color = LLColor4::green;
			}
			else if (channelp->getState() == LLVoiceChannel::STATE_ERROR)
			{
				icon_color = LLColor4::red;
			}
			else // active, but not connected
			{
				icon_color = LLColor4::yellow;
			}
		}
		mTabContainer->setTabImage(current_active_floater,
								   "active_voice_tab.tga", icon_color);
	}

	mActiveVoiceFloater = current_active_floater;

	LLMultiFloater::draw();
}

void LLFloaterChatterBox::onOpen()
{
	gSavedSettings.setBool("ShowCommunicate", true);

	if (mFirstOpen)
	{
		mFirstOpen = false;
		// Reposition floater from saved settings
		LLRect rect = gSavedSettings.getRect("ChatterboxRect");
		reshape(rect.getWidth(), rect.getHeight(), false);
		setRect(rect);
	}

	// Force a refresh to get latest display names in the new IM panel.
	gAvatarTracker.dirtyBuddies();
}

void LLFloaterChatterBox::onClose(bool app_quitting)
{
	setVisible(false);
	gSavedSettings.setBool("ShowCommunicate", false);
}

void LLFloaterChatterBox::setMinimized(bool minimized)
{
	LLFloater::setMinimized(minimized);
	// *HACK: potentially need to toggle console
	LLFloaterChat::getInstance()->updateConsoleVisibility();
}

void LLFloaterChatterBox::removeFloater(LLFloater* floaterp)
{
    // Only my friends floater now locked
	mTabContainer->lockTabs(mTabContainer->getNumLockedTabs() - 1);
	if (floaterp->getName() == gChatFloaterName)
	{
		gSavedSettings.setBool("ChatHistoryTornOff", true);
	}
	floaterp->setCanClose(true);
	LLMultiFloater::removeFloater(floaterp);
}

void LLFloaterChatterBox::addFloater(LLFloater* floaterp, 
									bool select_added_floater, 
									LLTabContainer::eInsertionPoint insertion_point)
{
	S32 num_locked_tabs = mTabContainer->getNumLockedTabs();

	// Already here
	if (floaterp->getHost() == this) return;

	// Make sure chat history is locked when re-attaching it
	if (floaterp->getName() == gChatFloaterName)
	{
		mTabContainer->unlockTabs();
		// Add chat history as second tab if contact window is present, first
		// tab otherwise
		LLMultiFloater::addFloater(floaterp, select_added_floater,
								   LLTabContainer::START);

		// Make sure first two tabs are now locked
		mTabContainer->lockTabs(num_locked_tabs + 1);
		gSavedSettings.setBool("ChatHistoryTornOff", false);
		floaterp->setCanClose(false);
	}
	else
	{
		LLMultiFloater::addFloater(floaterp, select_added_floater,
								   insertion_point);
	}

	// Make sure active voice icon shows up for new tab
	if (floaterp == mActiveVoiceFloater)
	{
		mTabContainer->setTabImage(floaterp, "active_voice_tab.tga");
	}
}

//static 
LLFloater* LLFloaterChatterBox::getCurrentVoiceFloater()
{
	if (!LLVoiceClient::voiceEnabled())
	{
		return NULL;
	}
	LLVoiceChannel* cur_voicep = LLVoiceChannel::getCurrentVoiceChannel();
	if (LLVoiceChannelProximal::getInstance() == cur_voicep)
	{
		// Show near me tab if in proximal channel
		return LLFloaterChat::getInstance(LLSD());
	}

	LLFloaterChatterBox* floater = LLFloaterChatterBox::getInstance(LLSD());
	// Iterate over all IM tabs
	for (S32 i = 0; i < floater->getFloaterCount(); i++)
	{
		LLPanel* panelp = floater->mTabContainer->getPanelByIndex(i);
		if (panelp->getName() == gIMFloaterName)
		{
			// Only LLFloaterIMSessions are called gIMFloaterName
			LLFloaterIMSession* im_floaterp = (LLFloaterIMSession*)panelp;
			if (im_floaterp->getVoiceChannel() == cur_voicep)
			{
				return im_floaterp;
			}
		}
	}
	return NULL;
}
