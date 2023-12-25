/**
 * @file llfloateractivespeakers.cpp
 * @brief Management interface for muting and controlling volume of residents
 * currently speaking
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloateractivespeakers.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcorehttputil.h"
#include "llscrolllistctrl.h"
#include "llsdutil.h"
#include "llslider.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloateravatarinfo.h"
#include "llfloaterim.h"
#include "llfloaterobjectiminfo.h"
#include "llimmgr.h"
#include "llmutelist.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvoicechannel.h"
#include "llworld.h"

using namespace LLOldEvents;

// Seconds of not being on voice channel before removed from list of active
// speakers
constexpr F32 SPEAKER_TIMEOUT = 10.f;
// Seconds of mouse inactivity before it's ok to sort regardless of
// mouse-in-view.
constexpr F32 RESORT_TIMEOUT = 5.f;
const LLColor4 INACTIVE_COLOR(0.3f, 0.3f, 0.3f, 0.5f);
const LLColor4 ACTIVE_COLOR(0.5f, 0.5f, 0.5f, 1.f);

LLSpeaker::LLSpeaker(const LLUUID& id, const std::string& name,
					 ESpeakerType type, ESpeakerStatus status)
:	mStatus(status),
	mLastSpokeTime(0.f),
	mSpeechVolume(0.f),
	mHasSpoken(false),
	mDotColor(LLColor4::white),
	mID(id),
	mTyping(false),
	mSortIndex(0),
	mType(type),
	mIsModerator(false),
	mModeratorMutedVoice(false),
	mModeratorMutedText(false),
	mNeedsResort(true)
{
	if (name.empty() && type == SPEAKER_AGENT)
	{
		lookupName();
	}
	else
	{
		mDisplayName = mLegacyName = name;
	}

	gVoiceClient.setUserVolume(id, LLMuteList::getSavedResidentVolume(id));

	mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
}

void LLSpeaker::lookupName()
{
	LLAvatarNameCache::get(mID,
						   boost::bind(&LLSpeaker::onAvatarNameLookup, _1, _2,
									   new LLHandle<LLSpeaker>(getHandle())));
}

//static
void LLSpeaker::onAvatarNameLookup(const LLUUID& id,
								   const LLAvatarName& avatar_name,
								   void* user_data)
{
	LLSpeaker* speaker_ptr = ((LLHandle<LLSpeaker>*)user_data)->get();

	if (speaker_ptr)
	{
		// Must keep "Resident" last names, thus the "true"
		speaker_ptr->mLegacyName = avatar_name.getLegacyName(true);
		if (!LLAvatarName::sLegacyNamesForSpeakers &&
			LLAvatarNameCache::useDisplayNames())
		{
			// Always show "Display Name [Legacy Name]" for security reasons
			speaker_ptr->setDisplayName(avatar_name.getNames());
		}
		else
		{
			// "Resident" last names stripped when appropriate
			speaker_ptr->setDisplayName(avatar_name.getLegacyName());
		}
	}

	delete (LLHandle<LLSpeaker>*)user_data;
}

LLSpeakerTextModerationEvent::LLSpeakerTextModerationEvent(LLSpeaker* source)
:	LLEvent(source, "Speaker text moderation event")
{
}

LLSD LLSpeakerTextModerationEvent::getValue()
{
	return std::string("text");
}

LLSpeakerVoiceModerationEvent::LLSpeakerVoiceModerationEvent(LLSpeaker* source)
:	LLEvent(source, "Speaker voice moderation event")
{
}

LLSD LLSpeakerVoiceModerationEvent::getValue()
{
	return std::string("voice");
}

LLSpeakerListChangeEvent::LLSpeakerListChangeEvent(LLSpeakerMgr* source,
												   const LLUUID& speaker_id)
:	LLEvent(source, "Speaker added/removed from speaker mgr"),
	mSpeakerID(speaker_id)
{
}

LLSD LLSpeakerListChangeEvent::getValue()
{
	return mSpeakerID;
}

// Helper sort class
struct LLSortRecentSpeakers
{
	bool operator()(const LLPointer<LLSpeaker> lhs,
					const LLPointer<LLSpeaker> rhs) const;
};

bool LLSortRecentSpeakers::operator()(const LLPointer<LLSpeaker> lhs,
									  const LLPointer<LLSpeaker> rhs) const
{
	// Sort first on status
	if (lhs->mStatus != rhs->mStatus)
	{
		return lhs->mStatus < rhs->mStatus;
	}

	// And then on last speaking time
	if (lhs->mLastSpokeTime != rhs->mLastSpokeTime)
	{
		return lhs->mLastSpokeTime > rhs->mLastSpokeTime;
	}

	// And finally (only if those are both equal), on name.
	return lhs->mDisplayName.compare(rhs->mDisplayName) < 0;
}

//
// LLFloaterActiveSpeakers
//

LLFloaterActiveSpeakers::LLFloaterActiveSpeakers(const LLSD& seed)
:	mPanel(NULL)
{
	mFactoryMap["active_speakers_panel"] = LLCallbackMap(createSpeakersPanel,
														 NULL);
	// Do not automatically open singleton floaters (as result of getInstance())
	bool no_open = false;
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_active_speakers.xml",
												 &getFactoryMap(), no_open);
#if 0
	// RN: for now, we poll voice client every frame to get voice amplitude
	// feedback
	gVoiceClient.addObserver(this);
#endif
	mPanel->refreshSpeakers(true);
}

void LLFloaterActiveSpeakers::onOpen()
{
	gSavedSettings.setBool("ShowActiveSpeakers", true);
}

void LLFloaterActiveSpeakers::onClose(bool app_quitting)
{
	if (!app_quitting)
	{
		gSavedSettings.setBool("ShowActiveSpeakers", false);
	}
	setVisible(false);
}

void LLFloaterActiveSpeakers::draw()
{
	// Update state every frame to get live amplitude feedback
	mPanel->refreshSpeakers();
	LLFloater::draw();
}

bool LLFloaterActiveSpeakers::postBuild()
{
	mPanel = getChild<LLPanelActiveSpeakers>("active_speakers_panel");
	return true;
}

//static
void* LLFloaterActiveSpeakers::createSpeakersPanel(void* data)
{
	// Do not show text only speakers
	return new LLPanelActiveSpeakers(LLActiveSpeakerMgr::getInstance(), false);
}

//
// LLPanelActiveSpeakers::SpeakerMuteListener
//
bool LLPanelActiveSpeakers::SpeakerMuteListener::handleEvent(LLPointer<LLEvent> event,
															 const LLSD& userdata)
{
	LLPointer<LLSpeaker> speakerp = (LLSpeaker*)event->getSource();
	if (speakerp.isNull()) return false;

	// Update UI on confirmation of moderator mutes
	if (mPanel->mModeratorAllowVoiceCtrl)
	{
		if (event->getValue().asString() == "voice")
		{
			mPanel->mModeratorAllowVoiceCtrl->setValue(!speakerp->mModeratorMutedVoice);
		}
	}
	if (mPanel->mModeratorAllowTextCtrl)
	{
		if (event->getValue().asString() == "text")
		{
			mPanel->mModeratorAllowTextCtrl->setValue(!speakerp->mModeratorMutedText);
		}
	}
	return true;
}

//
// LLPanelActiveSpeakers::SpeakerAddListener
//
bool LLPanelActiveSpeakers::SpeakerAddListener::handleEvent(LLPointer<LLEvent> event,
															const LLSD& userdata)
{
	mPanel->addSpeaker(event->getValue().asUUID());
	return true;
}

//
// LLPanelActiveSpeakers::SpeakerRemoveListener
//
bool LLPanelActiveSpeakers::SpeakerRemoveListener::handleEvent(LLPointer<LLEvent> event,
															   const LLSD& userdata)
{
	mPanel->removeSpeaker(event->getValue().asUUID());
	return true;
}

//
// LLPanelActiveSpeakers::SpeakerClearListener
//
bool LLPanelActiveSpeakers::SpeakerClearListener::handleEvent(LLPointer<LLEvent> event,
															  const LLSD& userdata)
{
	mPanel->mSpeakerList->clearRows();
	return true;
}

//
// LLPanelActiveSpeakers
//
LLPanelActiveSpeakers::LLPanelActiveSpeakers(LLSpeakerMgr* data_source,
											 bool show_text_chatters)
:	mSpeakerList(NULL),
	mModerationPanel(NULL),
	mModerationControls(NULL),
	mSpeakerVolumeSlider(NULL),
	mMuteVoiceCtrl(NULL),
	mMuteTextCtrl(NULL),
	mModeratorAllowVoiceCtrl(NULL),
	mModeratorAllowTextCtrl(NULL),
	mModerationModeCtrl(NULL),
	mModeratorControlsText(NULL),
	mNameText(NULL),
	mProfileBtn(NULL),
	mShowTextChatters(show_text_chatters),
	mSpeakerMgr(data_source)
{
	setMouseOpaque(false);
	mSpeakerMuteListener = new SpeakerMuteListener(this);
	mSpeakerAddListener = new SpeakerAddListener(this);
	mSpeakerRemoveListener = new SpeakerRemoveListener(this);
	mSpeakerClearListener = new SpeakerClearListener(this);

	mSpeakerMgr->addListener(mSpeakerAddListener, "add");
	mSpeakerMgr->addListener(mSpeakerRemoveListener, "remove");
	mSpeakerMgr->addListener(mSpeakerClearListener, "clear");
}

bool LLPanelActiveSpeakers::postBuild()
{
	std::string sort_column = gSavedSettings.getString("FloaterActiveSpeakersSortColumn");
	bool sort_ascending = gSavedSettings.getBool("FloaterActiveSpeakersSortAscending");

	mSpeakerList = getChild<LLScrollListCtrl>("speakers_list");
	mSpeakerList->sortByColumn(sort_column, sort_ascending);
	mSpeakerList->setDoubleClickCallback(onDoubleClickSpeaker);
	mSpeakerList->setCommitOnSelectionChange(true);
	mSpeakerList->setCommitCallback(onSelectSpeaker);
	mSpeakerList->setSortChangedCallback(onSortChanged);
	mSpeakerList->setCallbackUserData(this);

	mMuteTextCtrl = getChild<LLUICtrl>("mute_text_btn", true, false);
	if (mMuteTextCtrl)
	{
		childSetCommitCallback("mute_text_btn", onClickMuteTextCommit, this);
	}

	mMuteVoiceCtrl = getChild<LLUICtrl>("mute_check", true, false);
	if (mMuteVoiceCtrl)
	{
		// For the mute check box, in floater_chat_history.xml
		childSetCommitCallback("mute_check", onClickMuteVoiceCommit, this);
	}

	if (getChild<LLButton>("mute_btn", true, false))
	{
		// For the mute buttons, everywhere else
		childSetAction("mute_btn", onClickMuteVoice, this);
	}

	mSpeakerVolumeSlider = getChild<LLSlider>("speaker_volume", true, false);
	if (mSpeakerVolumeSlider)
	{
		mSpeakerVolumeSlider->setCommitCallback(onVolumeChange);
		mSpeakerVolumeSlider->setCallbackUserData(this);
	}

	mNameText = getChild<LLTextBox>("resident_name", true, false);

	mProfileBtn = getChild<LLButton>("profile_btn", true, false);
	if (mProfileBtn)
	{
		childSetAction("profile_btn", onClickProfile, this);
	}

	mModeratorAllowVoiceCtrl = getChild<LLUICtrl>("moderator_allow_voice",
												  true, false);
	if (mModeratorAllowVoiceCtrl)
	{
		mModeratorAllowVoiceCtrl->setCommitCallback(onModeratorMuteVoice);
		mModeratorAllowVoiceCtrl->setCallbackUserData(this);

		mModeratorAllowTextCtrl = getChild<LLUICtrl>("moderator_allow_text",
													 true, false);
		if (mModeratorAllowTextCtrl)
		{
			mModeratorAllowTextCtrl->setCommitCallback(onModeratorMuteText);
			mModeratorAllowTextCtrl->setCallbackUserData(this);
		}

		mModerationModeCtrl = getChild<LLUICtrl>("moderation_mode",
												 true, false);
		if (mModerationModeCtrl)
		{
			mModerationModeCtrl->setCommitCallback(onChangeModerationMode);
			mModerationModeCtrl->setCallbackUserData(this);
		}

		mModeratorControlsText = getChild<LLTextBox>("moderator_controls_label",
													 true, false);

		mModerationPanel = getChild<LLView>("moderation_mode_panel",
											true, false);
		mModerationControls = getChild<LLView>("moderator_controls",
											   true, false);
	}

	// Update speaker UI
	handleSpeakerSelect();

	return true;
}

void LLPanelActiveSpeakers::addSpeaker(const LLUUID& speaker_id, bool force)
{
	if (speaker_id.isNull() || mSpeakerList->getItemIndex(speaker_id) >= 0)
	{
		// Already have this speaker
		return;
	}

	LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(speaker_id);
	if (force && speakerp.isNull())
	{
		llinfos << "Force-adding absent speaker: " << speaker_id << llendl;
		speakerp = mSpeakerMgr->setSpeaker(speaker_id);
		// The "add" event that results from the above call will automatically
		// re-call this method.
		return;
	}
	if (speakerp.notNull())
	{
		// Since we are forced to sort by text, encode sort order as string
		std::string speaking_order_sort_string = llformat("%010d",
														  speakerp->mSortIndex);

		LLSD row;
		row["id"] = speaker_id;

		LLSD& columns = row["columns"];

		columns[0]["column"] = "icon_speaking_status";
		columns[0]["type"] = "icon";
		columns[0]["value"] = "icn_active-speakers-dot-lvl0.tga";

		std::string speaker_name;
		if (speakerp->mDisplayName.empty())
		{
			speaker_name = LLCacheName::getDefaultName();
		}
		else
		{
			speaker_name = speakerp->mDisplayName;
		}
		columns[1]["column"] = "speaker_name";
		columns[1]["type"] = "text";
		columns[1]["value"] = speaker_name;

		columns[2]["column"] = "speaking_status";
		columns[2]["type"] = "text";

		// Print speaking ordinal in a text-sorting friendly manner
		columns[2]["value"] = speaking_order_sort_string;

		mSpeakerList->addElement(row);
	}
}

void LLPanelActiveSpeakers::removeSpeaker(const LLUUID& speaker_id)
{
	mSpeakerList->deleteSingleItem(mSpeakerList->getItemIndex(speaker_id));
}

void LLPanelActiveSpeakers::handleSpeakerSelect()
{
	LLUUID speaker_id = mSpeakerList->getValue().asUUID();
	LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp.notNull())
	{
		// Since setting these values is delayed by a round trip to the Vivox
		// servers update them only when selecting a new speaker or
		// asynchronously when an update arrives
		if (mModeratorAllowVoiceCtrl)
		{
			mModeratorAllowVoiceCtrl->setValue(speakerp ?
											   !speakerp->mModeratorMutedVoice :
											   true);
		}
		if (mModeratorAllowTextCtrl)
		{
			mModeratorAllowTextCtrl->setValue(speakerp ?
											  !speakerp->mModeratorMutedText :
											  true);
		}

		mSpeakerMuteListener->clearDispatchers();
		speakerp->addListener(mSpeakerMuteListener);
	}
}

void LLPanelActiveSpeakers::refreshSpeakers(bool force)
{
	static const LLUIImagePtr icon_image_0 =
		LLUI::getUIImage("icn_active-speakers-dot-lvl0.tga");
	static const LLUIImagePtr icon_image_1 =
		LLUI::getUIImage("icn_active-speakers-dot-lvl1.tga");
	static const LLUIImagePtr icon_image_2 =
		LLUI::getUIImage("icn_active-speakers-dot-lvl2.tga");
	static const LLUIImagePtr mute_icon_image =
		LLUI::getUIImage("mute_icon.tga");

//MK
	if (gRLenabled && (gRLInterface.mContainsShownames ||
					   gRLInterface.mContainsShowNearby))
	{
		mSpeakerList->clearRows();
		return;
	}
//mk

	// Store off current selection and scroll state to preserve across list
	// rebuilds
	LLUUID selected_id = mSpeakerList->getSelectedValue().asUUID();
	S32 scroll_pos = mSpeakerList->getScrollPos();

	// Decide whether it is ok to resort the list then update the speaker
	// manager appropriately. Rapid resorting by activity makes it hard to
	// interact with speakers in the list so we freeze the sorting while the
	// user appears to be interacting with the control. We assume this is the
	// case whenever the mouse pointer is within the active speaker panel and
	// has not been motionless for more than a few seconds. see DEV-6655 -MG
	LLRect screen_rect;
	localRectToScreen(getLocalRect(), &screen_rect);
	bool mouse_in_view = screen_rect.pointInRect(gViewerWindowp->getCurrentMouseX(),
												 gViewerWindowp->getCurrentMouseY());
	F32 mouse_last_movement = gMouseIdleTimer.getElapsedTimeF32();
	bool sort_ok = force || !mouse_in_view ||
				   mouse_last_movement >= RESORT_TIMEOUT;
	mSpeakerMgr->update(sort_ok);

	std::vector<LLScrollListItem*> items = mSpeakerList->getAllData();

	LLSpeakerMgr::speaker_list_t speaker_list;
	mSpeakerMgr->getSpeakerList(&speaker_list, mShowTextChatters);
	for (std::vector<LLScrollListItem*>::iterator item_it = items.begin(),
												  end = items.end();
		 item_it != end; ++item_it)
	{
		LLScrollListItem* itemp = *item_it;
		LLUUID speaker_id = itemp->getUUID();

		LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(speaker_id);
		if (speakerp.isNull())
		{
			continue;
		}

		// Since we are forced to sort by text, encode sort order as string
		std::string speaking_order_sort_string = llformat("%010d",
														  speakerp->mSortIndex);

		LLScrollListIcon* icon_cell =
			dynamic_cast<LLScrollListIcon*>(itemp->getColumn(0));
		if (icon_cell)
		{
			LLUIImagePtr icon_image_id;

			S32 icon_image_idx = llmin(2,
									   llfloor(3.f * speakerp->mSpeechVolume /
											   OVERDRIVEN_POWER_LEVEL));
			switch (icon_image_idx)
			{
				case 0:
					icon_image_id = icon_image_0;
					break;

				case 1:
					icon_image_id = icon_image_1;
					break;

				case 2:
					icon_image_id = icon_image_2;
			}

			LLColor4 icon_color;
			if (speakerp->mStatus == LLSpeaker::STATUS_MUTED)
			{
				icon_cell->setImage(mute_icon_image);
				if (speakerp->mModeratorMutedVoice)
				{
					icon_color.set(0.5f, 0.5f, 0.5f, 1.f);
				}
				else
				{
					icon_color.set(1.f, 71.f / 255.f, 71.f / 255.f, 1.f);
				}
			}
			else
			{
				icon_cell->setImage(icon_image_id);
				icon_color = speakerp->mDotColor;

				// If voice is disabled for this speaker
				if (speakerp->mStatus > LLSpeaker::STATUS_VOICE_ACTIVE)
				{
					// Non voice speakers have hidden icons, render as
					// transparent
					icon_color.set(0.f, 0.f, 0.f, 0.f);
				}
			}

			icon_cell->setColor(icon_color);

			// If voice is disabled for this speaker
			if (speakerp->mStatus > LLSpeaker::STATUS_VOICE_ACTIVE &&
				speakerp->mStatus != LLSpeaker::STATUS_MUTED)
			{
				// Non voice speakers have hidden icons, render as transparent
				icon_cell->setColor(LLColor4::transparent);
			}
		}

		// Update name column
		LLScrollListCell* name_cell = itemp->getColumn(1);
		if (name_cell)
		{
			// *FIXME: remove hard coding of font colors
			if (speakerp->mStatus == LLSpeaker::STATUS_NOT_IN_CHANNEL)
			{
				// Draw inactive speakers in gray
				name_cell->setColor(LLColor4::grey4);
			}
			else
			{
				name_cell->setColor(LLColor4::black);
			}

			std::string speaker_name;
			if (speakerp->mDisplayName.empty())
			{
				speaker_name = LLCacheName::getDefaultName();
			}
			else
			{
				speaker_name = speakerp->mDisplayName;
			}

			if (speakerp->mIsModerator)
			{
				speaker_name += " " + getString("moderator_label");
			}

			name_cell->setValue(speaker_name);

			LLScrollListText* text_cell =
				dynamic_cast<LLScrollListText*>(name_cell);
			if (text_cell)
			{
				text_cell->setFontStyle(speakerp->mIsModerator ?
											LLFontGL::BOLD : LLFontGL::NORMAL);
			}
		}

		// Update speaking order column
		LLScrollListCell* speaking_status_cell = itemp->getColumn(2);
		if (speaking_status_cell)
		{
			// Print speaking ordinal in a text-sorting friendly manner
			speaking_status_cell->setValue(speaking_order_sort_string);
		}
	}

	// We potentially modified the sort order by touching the list items
	mSpeakerList->setSorted(false);

	LLPointer<LLSpeaker> selected_speakerp = mSpeakerMgr->findSpeaker(selected_id);
	// Update UI for selected participant
	bool valid_speaker = selected_id.notNull() && selected_id != gAgentID &&
						 selected_speakerp.notNull();
	bool speaker_on_voice = LLVoiceClient::voiceEnabled() &&
							gVoiceClient.getVoiceEnabled(selected_id);
	if (mMuteVoiceCtrl)
	{
		mMuteVoiceCtrl->setValue(LLMuteList::isMuted(selected_id,
													 LLMute::flagVoiceChat));
		mMuteVoiceCtrl->setEnabled(speaker_on_voice && valid_speaker &&
								   (selected_speakerp->mType == LLSpeaker::SPEAKER_AGENT ||
									selected_speakerp->mType == LLSpeaker::SPEAKER_EXTERNAL));

	}
	if (mMuteTextCtrl)
	{
		mMuteTextCtrl->setValue(LLMuteList::isMuted(selected_id,
													LLMute::flagTextChat));
		mMuteTextCtrl->setEnabled(valid_speaker &&
								  selected_speakerp->mType != LLSpeaker::SPEAKER_EXTERNAL &&
								  !LLMuteList::isLinden(selected_speakerp->mLegacyName));
	}

	if (mSpeakerVolumeSlider)
	{
		mSpeakerVolumeSlider->setValue(gVoiceClient.getUserVolume(selected_id));
		mSpeakerVolumeSlider->setEnabled(speaker_on_voice && valid_speaker &&
										 (selected_speakerp->mType == LLSpeaker::SPEAKER_AGENT ||
										 selected_speakerp->mType == LLSpeaker::SPEAKER_EXTERNAL));
	}

	if (mModeratorAllowVoiceCtrl)
	{
		mModeratorAllowVoiceCtrl->setEnabled(selected_id.notNull() &&
											 mSpeakerMgr->isVoiceActive() &&
											 gVoiceClient.getVoiceEnabled(selected_id));
	}
	if (mModeratorAllowTextCtrl)
	{
		mModeratorAllowTextCtrl->setEnabled(selected_id.notNull());
	}
	if (mModeratorControlsText)
	{
		mModeratorControlsText->setEnabled(selected_id.notNull());
	}

	if (mProfileBtn)
	{
		mProfileBtn->setEnabled(selected_id.notNull() &&
								selected_speakerp.notNull() &&
								selected_speakerp->mType != LLSpeaker::SPEAKER_EXTERNAL);
	}

	// Show selected user name in large font
	if (mNameText)
	{
		if (selected_speakerp)
		{
			mNameText->setValue(selected_speakerp->mDisplayName);
		}
		else
		{
			mNameText->setValue(LLStringUtil::null);
		}
	}

	if (mModeratorAllowVoiceCtrl)
	{
		// Update moderator capabilities
		LLPointer<LLSpeaker> self_speakerp = mSpeakerMgr->findSpeaker(gAgentID);
		if (self_speakerp.notNull())
		{
			bool moderator = self_speakerp->mIsModerator;
			if (mModerationPanel)
			{
				mModerationPanel->setVisible(moderator &&
											 mSpeakerMgr->isVoiceActive());
			}
			if (mModerationControls)
			{
				mModerationControls->setVisible(moderator);
			}
		}
	}

	// Keep scroll value stable
	mSpeakerList->setScrollPos(scroll_pos);
}

void LLPanelActiveSpeakers::setSpeaker(const LLUUID& id,
									   const std::string& name,
									   LLSpeaker::ESpeakerStatus status,
									   LLSpeaker::ESpeakerType type,
									   const LLUUID& owner_id)
{
	mSpeakerMgr->setSpeaker(id, name, status, type, owner_id);
}

void LLPanelActiveSpeakers::setVoiceModerationCtrlMode(const bool& moderated_voice)
{
	if (mModerationModeCtrl)
	{
		std::string value = moderated_voice ? "moderated" : "unmoderated";
		mModerationModeCtrl->setValue(value);
	}
}

//static
void LLPanelActiveSpeakers::onClickMuteTextCommit(LLUICtrl* ctrl,
												  void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	if (!panelp) return;

	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();
	bool is_muted = LLMuteList::isMuted(speaker_id, LLMute::flagTextChat);

	//fill in name using voice client's copy of name cache
	LLPointer<LLSpeaker> speakerp = panelp->mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp.isNull())
	{
		return;
	}

	std::string name = speakerp->mLegacyName;

	LLMute mute(speaker_id, name,
				speakerp->mType == LLSpeaker::SPEAKER_AGENT ? LLMute::AGENT
															: LLMute::OBJECT);

	if (!is_muted)
	{
		LLMuteList::add(mute, LLMute::flagTextChat);
	}
	else
	{
		LLMuteList::remove(mute, LLMute::flagTextChat);
	}
}

//static
void LLPanelActiveSpeakers::onClickMuteVoice(void* user_data)
{
	onClickMuteVoiceCommit(NULL, user_data);
}

//static
void LLPanelActiveSpeakers::onClickMuteVoiceCommit(LLUICtrl* ctrl, void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	if (!panelp) return;

	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();
	bool is_muted = LLMuteList::isMuted(speaker_id, LLMute::flagVoiceChat);

	LLPointer<LLSpeaker> speakerp = panelp->mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp.isNull())
	{
		return;
	}

	std::string name = speakerp->mLegacyName;

	// Muting voice means we're dealing with an agent
	LLMute mute(speaker_id, name, LLMute::AGENT);

	if (!is_muted)
	{
		LLMuteList::add(mute, LLMute::flagVoiceChat);
	}
	else
	{
		LLMuteList::remove(mute, LLMute::flagVoiceChat);
	}
}

//static
void LLPanelActiveSpeakers::onVolumeChange(LLUICtrl* source, void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	if (panelp && panelp->mSpeakerVolumeSlider)
	{
		LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();
		F32 new_volume = panelp->mSpeakerVolumeSlider->getValue().asReal();

		gVoiceClient.setUserVolume(speaker_id, new_volume);

		// Store this volume setting for future sessions
		LLMuteList::setSavedResidentVolume(speaker_id, new_volume);
	}
}

//static
void LLPanelActiveSpeakers::onClickProfile(void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	if (!panelp) return;

	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();
	LLPointer<LLSpeaker> speakerp = panelp->mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp.isNull()) return;

	if (speakerp->mType == LLSpeaker::SPEAKER_AGENT)
	{
		LLFloaterAvatarInfo::showFromDirectory(speaker_id);
	}
	else if (speakerp->mType == LLSpeaker::SPEAKER_OBJECT)
	{
		LLViewerObject* object = gObjectList.findObject(speaker_id);
		if (!object)
		{
			// Others' HUDs are not in our objects list: use the HUD owner
			// to find out their actual position...
			object = gObjectList.findObject(speakerp->mOwnerID);
		}
		if (object
//MK
			&& !(gRLenabled && gRLInterface.mContainsShowloc))
//mk
		{
			LLVector3 pos = object->getPositionRegion();
			S32 x = ll_round((F32)fmod((F64)pos.mV[VX],
									   (F64)REGION_WIDTH_METERS));
			S32 y = ll_round((F32)fmod((F64)pos.mV[VY],
									   (F64)REGION_WIDTH_METERS));
			S32 z = ll_round((F32)pos.mV[VZ]);
			std::ostringstream location;
			location << object->getRegion()->getName() << "/" << x << "/"
					 << y << "/" << z;
			LLObjectIMInfo::show(speaker_id, speakerp->mDisplayName,
								 location.str(), speakerp->mOwnerID, false);
		}
	}
}

//static
void LLPanelActiveSpeakers::onDoubleClickSpeaker(void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	if (!panelp) return;

	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();
	LLPointer<LLSpeaker> speakerp = panelp->mSpeakerMgr->findSpeaker(speaker_id);
	if (gIMMgrp && speaker_id != gAgentID && speakerp.notNull())
	{
		gIMMgrp->addSession(speakerp->mLegacyName, IM_NOTHING_SPECIAL, speaker_id);
	}
}

//static
void LLPanelActiveSpeakers::onSelectSpeaker(LLUICtrl* source, void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	if (panelp)
	{
		panelp->handleSpeakerSelect();
	}
}

//static
void LLPanelActiveSpeakers::onSortChanged(void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	if (panelp)
	{
		gSavedSettings.setString("FloaterActiveSpeakersSortColumn",
								 panelp->mSpeakerList->getSortColumnName());
		gSavedSettings.setBool("FloaterActiveSpeakersSortAscending",
							   panelp->mSpeakerList->getSortAscending());
	}
}

//static
void LLPanelActiveSpeakers::moderatorActionFailedCallback(const LLSD& result,
														  LLUUID session_id)
{
	if (gIMMgrp) return;	// Viewer is closing down !

	LLFloaterIMSession* floaterp =
		LLFloaterIMSession::findInstance(session_id);
	if (!floaterp)
	{
		llinfos << "Received a reply for closed session Id: " << session_id
				<< ". Ignored." << llendl;
		return;
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status == gStatusForbidden)
	{
		// 403 == you are not a mod: should be disabled then.
		floaterp->showSessionEventError("mute", "not_a_moderator");
	}
	else
	{
		floaterp->showSessionEventError("mute", "generic");
	}
}

//static
void LLPanelActiveSpeakers::onModeratorMuteVoice(LLUICtrl* ctrl,
												 void* user_data)
{
	LLPanelActiveSpeakers* self = (LLPanelActiveSpeakers*)user_data;
	if (!self || !self->mSpeakerList || !ctrl) return;

	const LLUUID& session_id = self->mSpeakerMgr->getSessionID();
	LLAgent::httpCallback_t
		fail = boost::bind(&LLPanelActiveSpeakers::moderatorActionFailedCallback,
						   _1, session_id);
	LLSD data;
	data["method"] = "mute update";
	data["session-id"] = session_id;
	data["params"] = LLSD::emptyMap();
	data["params"]["agent_id"] = self->mSpeakerList->getValue();
	data["params"]["mute_info"] = LLSD::emptyMap();
	// Ctrl value represents ability to type, so invert
	data["params"]["mute_info"]["voice"] = !ctrl->getValue();

	if (!gAgent.requestPostCapability("ChatSessionRequest", data, NULL, fail))
	{
		llwarns << "Cannot get the ChatSessionRequest capability !  Aborted."
				<< llendl;
	}
}

//static
void LLPanelActiveSpeakers::onModeratorMuteText(LLUICtrl* ctrl,
												void* user_data)
{
	LLPanelActiveSpeakers* self = (LLPanelActiveSpeakers*)user_data;
	if (!self || !self->mSpeakerList || !ctrl) return;

	const LLUUID& session_id = self->mSpeakerMgr->getSessionID();
	LLAgent::httpCallback_t
		fail = boost::bind(&LLPanelActiveSpeakers::moderatorActionFailedCallback,
						   _1, session_id);
	LLSD data;
	data["method"] = "mute update";
	data["session-id"] = session_id;
	data["params"] = LLSD::emptyMap();
	data["params"]["agent_id"] = self->mSpeakerList->getValue();
	data["params"]["mute_info"] = LLSD::emptyMap();
	// Ctrl value represents ability to type, so invert
	data["params"]["mute_info"]["text"] = !ctrl->getValue();

	if (!gAgent.requestPostCapability("ChatSessionRequest", data, NULL, fail))
	{
		llwarns << "Cannot get the ChatSessionRequest capability !  Aborted."
				<< llendl;
	}
}

//static
void LLPanelActiveSpeakers::onChangeModerationMode(LLUICtrl* ctrl,
												   void* user_data)
{
	LLPanelActiveSpeakers* self = (LLPanelActiveSpeakers*)user_data;
	if (!self || !ctrl) return;

	const std::string& url = gAgent.getRegionCapability("ChatSessionRequest");
	if (url.empty())
	{
		llwarns << "Cannot get the ChatSessionRequest capability !  Aborted."
				<< llendl;
		return;
	}

	LLSD data;
	data["method"] = "session update";
	data["session-id"] = self->mSpeakerMgr->getSessionID();
	data["params"] = LLSD::emptyMap();

	data["params"]["update_info"] = LLSD::emptyMap();

	data["params"]["update_info"]["moderated_mode"] = LLSD::emptyMap();
	if (ctrl->getValue().asString() == "unmoderated")
	{
		data["params"]["update_info"]["moderated_mode"]["voice"] = false;
	}
	else if (ctrl->getValue().asString() == "moderated")
	{
		data["params"]["update_info"]["moderated_mode"]["voice"] = true;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, data,
														  "Moderation mode changed",
														  "Failed to change moderation mode");
}

//
// LLSpeakerMgr
//

LLSpeakerMgr::LLSpeakerMgr(LLVoiceChannel* channelp)
:	mVoiceChannel(channelp)
{
}

LLPointer<LLSpeaker> LLSpeakerMgr::setSpeaker(const LLUUID& id,
											  const std::string& name,
											  LLSpeaker::ESpeakerStatus status,
											  LLSpeaker::ESpeakerType type,
											  const LLUUID& owner_id)
{
	if (id.isNull()) return NULL;

	LLPointer<LLSpeaker> speakerp = findSpeaker(id);
	if (speakerp.isNull())
	{
		speakerp = new LLSpeaker(id, name, type, status);
		speakerp->mOwnerID = owner_id;
		mSpeakers[id] = speakerp;
		mSpeakersSorted.emplace_back(speakerp);
		fireEvent(new LLSpeakerListChangeEvent(this, id), "add");
	}
	else
	{
		// Keep highest priority status (lowest value) instead of overriding
		// current value
		speakerp->setStatus(llmin(speakerp->mStatus, status));
		speakerp->mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
		// RN: due to a weird behavior where IMs from attached objects come
		// from the wearer's agent_id we need to override speakers that we
		// think are objects when we find out they are really residents
		if (type == LLSpeaker::SPEAKER_AGENT)
		{
			speakerp->mType = LLSpeaker::SPEAKER_AGENT;
			speakerp->lookupName();
		}
	}

	return speakerp;
}

void LLSpeakerMgr::update(bool resort_ok)
{
	static LLCachedControl<LLColor4> speaking(gSavedSettings,
											   "SpeakingColor");
	static LLCachedControl<LLColor4> overdriven(gSavedSettings,
												 "OverdrivenColor");
	LLColor4 speaking_color = speaking;
	LLColor4 overdriven_color = overdriven;

	bool dirty = false;

	// Only allow list changes when user is not interacting with it
	if (resort_ok)
	{
		updateSpeakerList();
		dirty = true;
	}

	// Update status of all current speakers
	bool voice_channel_active = (mVoiceChannel && mVoiceChannel->isActive()) ||
								(!mVoiceChannel &&
								 gVoiceClient.inProximalChannel());
	for (speaker_map_t::iterator it = mSpeakers.begin(), end = mSpeakers.end();
		 it != end; ++it)
	{
		LLUUID speaker_id = it->first;
		LLSpeaker* speakerp = it->second;

		if (voice_channel_active && gVoiceClient.getVoiceEnabled(speaker_id))
		{
			speakerp->mSpeechVolume = gVoiceClient.getCurrentPower(speaker_id);
			bool moderator_muted_voice;
			moderator_muted_voice = gVoiceClient.getIsModeratorMuted(speaker_id);
			if (moderator_muted_voice != speakerp->mModeratorMutedVoice)
			{
				speakerp->mModeratorMutedVoice = moderator_muted_voice;
				speakerp->fireEvent(new LLSpeakerVoiceModerationEvent(speakerp));
			}

			if (gVoiceClient.getOnMuteList(speaker_id) ||
				speakerp->mModeratorMutedVoice)
			{
				speakerp->setStatus(LLSpeaker::STATUS_MUTED);
			}
			else if (gVoiceClient.getIsSpeaking(speaker_id))
			{
				// Reset inactivity expiration
				if (speakerp->mStatus != LLSpeaker::STATUS_SPEAKING)
				{
					speakerp->setSpokenTime(mSpeechTimer.getElapsedTimeF32());
				}
				speakerp->setStatus(LLSpeaker::STATUS_SPEAKING);
				// Interpolate between active color and full speaking color
				// based on power of speech output
				speakerp->mDotColor = speaking_color;
				if (speakerp->mSpeechVolume > OVERDRIVEN_POWER_LEVEL)
				{
					speakerp->mDotColor = overdriven_color;
				}
			}
			else
			{
				speakerp->mSpeechVolume = 0.f;
				speakerp->mDotColor = ACTIVE_COLOR;

				if (speakerp->mHasSpoken)
				{
					// Has spoken once, not currently speaking
					speakerp->setStatus(LLSpeaker::STATUS_HAS_SPOKEN);
				}
				else
				{
					// Default state for being in voice channel
					speakerp->setStatus(LLSpeaker::STATUS_VOICE_ACTIVE);
				}
			}

			if (speakerp->mNeedsResort)
			{
				speakerp->mNeedsResort = false;
				dirty = true;
			}
		}
		// Speaker no longer registered in voice channel, demote to text only
		else if (speakerp->mStatus != LLSpeaker::STATUS_NOT_IN_CHANNEL)
		{
			if (speakerp->mType == LLSpeaker::SPEAKER_EXTERNAL)
			{
				// External speakers should be timed out when they leave the
				// voice channel (since they only exist via SLVoice)
				speakerp->setStatus(LLSpeaker::STATUS_NOT_IN_CHANNEL);
			}
			else
			{
				speakerp->setStatus(LLSpeaker::STATUS_TEXT_ONLY);
				speakerp->mSpeechVolume = 0.f;
				speakerp->mDotColor = ACTIVE_COLOR;
			}
		}
	}

	if (!dirty)
	{
		return;
	}

	// Sort by status then time last spoken
	std::sort(mSpeakersSorted.begin(), mSpeakersSorted.end(),
			  LLSortRecentSpeakers());

	// For recent speakers who are not currently speaking, show "recent" color
	// dot for most recent fading to "active" color

	S32 recent_speaker_count = 0;
	S32 sort_index = 0;
	for (speaker_list_t::iterator it = mSpeakersSorted.begin();
		 it != mSpeakersSorted.end(); )
	{
		LLPointer<LLSpeaker> speakerp = *it;

		// Color code recent speakers who are not currently speaking
		if (speakerp->mStatus == LLSpeaker::STATUS_HAS_SPOKEN)
		{
			speakerp->mDotColor = lerp(speaking_color, ACTIVE_COLOR,
									   clamp_rescale((F32)recent_speaker_count,
													 -2.f, 3.f, 0.f, 1.f));
			++recent_speaker_count;
		}

		// Stuff sort ordinal into speaker so the ui can sort by this value
		speakerp->mSortIndex = sort_index++;

		// Remove speakers that have been gone too long
		if (speakerp->mStatus == LLSpeaker::STATUS_NOT_IN_CHANNEL &&
			speakerp->mActivityTimer.hasExpired())
		{
			fireEvent(new LLSpeakerListChangeEvent(this, speakerp->mID),
					  "remove");

			mSpeakers.erase(speakerp->mID);
			it = mSpeakersSorted.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void LLSpeakerMgr::updateSpeakerList()
{
	// Are we bound to the currently active voice channel ?
	if ((mVoiceChannel && mVoiceChannel->isActive()) ||
		(!mVoiceChannel && gVoiceClient.inProximalChannel()))
	{
		LLVoiceClient::particip_map_t* participants =
			gVoiceClient.getParticipantList();
		if (participants)
		{
			// Add new participants to our list of known speakers
			for (LLVoiceClient::particip_map_t::iterator
					it = participants->begin(), end = participants->end();
				 it != end; ++it)
			{
				LLVoiceClient::participantState* participantp = it->second;
				setSpeaker(participantp->mAvatarID, participantp->mLegacyName,
						   LLSpeaker::STATUS_VOICE_ACTIVE,
						   (participantp->isAvatar() ?
								LLSpeaker::SPEAKER_AGENT :
								LLSpeaker::SPEAKER_EXTERNAL));
			}
		}
	}
}

const LLPointer<LLSpeaker> LLSpeakerMgr::findSpeaker(const LLUUID& speaker_id)
{
	speaker_map_t::iterator it = mSpeakers.find(speaker_id);
	if (it == mSpeakers.end())
	{
		return NULL;
	}
	return it->second;
}

void LLSpeakerMgr::getSpeakerList(speaker_list_t* speaker_list,
								  bool include_text)
{
	speaker_list->clear();
	for (speaker_map_t::iterator it = mSpeakers.begin(), end = mSpeakers.end();
		 it != end; ++it)
	{
		LLPointer<LLSpeaker> speakerp = it->second;
		// What about text only muted or inactive ?
		if (include_text || speakerp->mStatus != LLSpeaker::STATUS_TEXT_ONLY)
		{
			speaker_list->push_back(speakerp);
		}
	}
}

const LLUUID LLSpeakerMgr::getSessionID()
{
	return mVoiceChannel->getSessionID();
}

void LLSpeakerMgr::setSpeakerTyping(const LLUUID& speaker_id, bool typing)
{
	LLPointer<LLSpeaker> speakerp = findSpeaker(speaker_id);
	if (speakerp.notNull())
	{
		speakerp->mTyping = typing;
	}
}

// Speaker has chatted via either text or voice
void LLSpeakerMgr::speakerChatted(const LLUUID& speaker_id)
{
	LLPointer<LLSpeaker> speakerp = findSpeaker(speaker_id);
	if (speakerp.notNull())
	{
		speakerp->setSpokenTime(mSpeechTimer.getElapsedTimeF32());
	}
}

bool LLSpeakerMgr::isVoiceActive()
{
	// mVoiceChannel = NULL means current voice channel, whatever it is
	return LLVoiceClient::voiceEnabled() && mVoiceChannel &&
		   mVoiceChannel->isActive();
}

//
// LLIMSpeakerMgr
//
LLIMSpeakerMgr::LLIMSpeakerMgr(LLVoiceChannel* channel)
:	LLSpeakerMgr(channel)
{
}

void LLIMSpeakerMgr::updateSpeakerList()
{
	// Do not do normal updates which are pulled from voice channel: rely on
	// user list reported by sim.

	// We need to do this to allow PSTN callers into group chats to show in the
	// list.
	LLSpeakerMgr::updateSpeakerList();

	return;
}

void LLIMSpeakerMgr::setSpeakers(const LLSD& speakers)
{
	if (!speakers.isMap()) return;

	if (speakers.has("agent_info") && speakers["agent_info"].isMap())
	{
		for (LLSD::map_const_iterator it = speakers["agent_info"].beginMap(),
									  end = speakers["agent_info"].endMap();
			 it != end; ++it)
		{
			const LLUUID agent_id(it->first);
			LLPointer<LLSpeaker> speakerp = setSpeaker(agent_id);
			if (it->second.isMap())
			{
				speakerp->mIsModerator = it->second["is_moderator"];
				speakerp->mModeratorMutedText = it->second["mutes"]["text"];
			}
		}
	}
	else if (speakers.has("agents") && speakers["agents"].isArray())
	{
		// Older, more deprecated way. Needed for older server versions
		for (LLSD::array_const_iterator it = speakers["agents"].beginArray(),
										end = speakers["agents"].endArray();
			 it != end; ++it)
		{
			const LLUUID agent_id = it->asUUID();
			setSpeaker(agent_id);
		}
	}
}

void LLIMSpeakerMgr::updateSpeakers(const LLSD& update)
{
	if (!update.isMap()) return;

	if (update.has("agent_updates") && update["agent_updates"].isMap())
	{
		LLPointer<LLSpeaker> speakerp;
		for (LLSD::map_const_iterator it = update["agent_updates"].beginMap(),
									  end = update["agent_updates"].endMap();
			 it != end; ++it)
		{
			LLUUID agent_id(it->first);
			speakerp = findSpeaker(agent_id);

			LLSD agent_data = it->second;

			if (agent_data.isMap() && agent_data.has("transition"))
			{
				const std::string& trans = agent_data["transition"].asString();
				if (trans == "LEAVE")
				{
					if (speakerp.notNull())
					{
						speakerp->setStatus(LLSpeaker::STATUS_NOT_IN_CHANNEL);
						speakerp->mDotColor = INACTIVE_COLOR;
						speakerp->mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
					}
				}
				else if (trans == "ENTER")
				{
					// Add or update speaker
					speakerp = setSpeaker(agent_id);
				}
				else
				{
					llwarns << "bad membership list update "
							<< ll_print_sd(agent_data["transition"]) << llendl;
				}
			}

			if (speakerp.isNull()) continue;

			// Should have a valid speaker from this point on
			if (agent_data.isMap() && agent_data.has("info"))
			{
				const LLSD& agent_info = agent_data["info"];

				if (agent_info.has("is_moderator"))
				{
					speakerp->mIsModerator = agent_info["is_moderator"];
				}

				if (agent_info.has("mutes"))
				{
					speakerp->mModeratorMutedText = agent_info["mutes"]["text"];
				}
			}
		}
	}
	else if (update.has("updates") && update["updates"].isMap())
	{
		for (LLSD::map_const_iterator it = update["updates"].beginMap(),
									  end = update["updates"].endMap();
			 it != end; ++it)
		{
			const LLUUID agent_id(it->first);
			LLPointer<LLSpeaker> speakerp = findSpeaker(agent_id);

			std::string agent_transition = it->second.asString();
			if (agent_transition == "LEAVE" && speakerp.notNull())
			{
				speakerp->setStatus(LLSpeaker::STATUS_NOT_IN_CHANNEL);
				speakerp->mDotColor = INACTIVE_COLOR;
				speakerp->mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
			}
			else if (agent_transition == "ENTER")
			{
				// Add or update speaker
				speakerp = setSpeaker(agent_id);
			}
			else
			{
				llwarns << "bad membership list update " << agent_transition
						<< llendl;
			}
		}
	}
}

//
// LLActiveSpeakerMgr
//

LLActiveSpeakerMgr::LLActiveSpeakerMgr()
:	LLSpeakerMgr(NULL)
{
}

void LLActiveSpeakerMgr::updateSpeakerList()
{
	// Point to whatever the current voice channel is
	mVoiceChannel = LLVoiceChannel::getCurrentVoiceChannel();

	// Always populate from active voice channel
	if (LLVoiceChannel::getCurrentVoiceChannel() != mVoiceChannel)
	{
		fireEvent(new LLSpeakerListChangeEvent(this, LLUUID::null), "clear");
		mSpeakers.clear();
		mSpeakersSorted.clear();
		mVoiceChannel = LLVoiceChannel::getCurrentVoiceChannel();
	}
	LLSpeakerMgr::updateSpeakerList();

	// Clean up text only speakers
	for (speaker_map_t::iterator it = mSpeakers.begin(), end = mSpeakers.end();
		 it != end; ++it)
	{
		LLSpeaker* speakerp = it->second;
		if (speakerp->mStatus == LLSpeaker::STATUS_TEXT_ONLY)
		{
			// Automatically flag text only speakers for removal
			speakerp->setStatus(LLSpeaker::STATUS_NOT_IN_CHANNEL);
		}
	}
}

//
// LLLocalSpeakerMgr
//

LLLocalSpeakerMgr::LLLocalSpeakerMgr()
:	LLSpeakerMgr(LLVoiceChannelProximal::getInstance())
{
}

void LLLocalSpeakerMgr::updateSpeakerList()
{
	// Pull speakers from voice channel
	LLSpeakerMgr::updateSpeakerList();

	LLViewerRegion* regionp = gAgent.getRegion();
	if (gDisconnected || !regionp)
	{
		return;
	}

	// Pick up non-voice speakers in chat range
	uuid_vec_t avatar_ids;
	std::vector<LLVector3d> positions;
	F32 radius = regionp->getChatRange();
	gWorld.getAvatars(avatar_ids, &positions, NULL,
					  gAgent.getPositionGlobal(), radius);
	for (U32 i = 0, count = avatar_ids.size(); i < count; ++i)
	{
		setSpeaker(avatar_ids[i]);
	}

	// Check if text only speakers have moved out of chat range
	for (speaker_map_t::iterator it = mSpeakers.begin(), end = mSpeakers.end();
		 it != end; ++it)
	{
		LLUUID speaker_id = it->first;
		LLSpeaker* speakerp = it->second;
		if (speakerp->mStatus == LLSpeaker::STATUS_TEXT_ONLY)
		{
			LLVOAvatar* avatarp = gObjectList.findAvatar(speaker_id);
			if (!avatarp ||
				dist_vec(avatarp->getPositionAgent(),
						 gAgent.getPositionAgent()) > radius)
			{
				speakerp->setStatus(LLSpeaker::STATUS_NOT_IN_CHANNEL);
				speakerp->mDotColor = INACTIVE_COLOR;
				speakerp->mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
			}
		}
	}
}
