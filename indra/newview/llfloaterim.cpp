/**
 * @file llfloaterim.cpp
 * @brief LLFloaterIM and LLFloaterIMSession classes definition
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llfloaterim.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcombobox.h"				// For class LLFlyoutButton
#include "llconsole.h"
#include "llcorehttputil.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llmessage.h"
#include "llnotifications.h"
#include "llslider.h"
#include "llstylemap.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llchat.h"
#include "llfloateractivespeakers.h"
#include "llfloateravatarinfo.h"
#include "llfloaterchat.h"
#include "llfloatergroupinfo.h"
#include "llfloatermediabrowser.h"
#include "hbfloatertextinput.h"
#include "llimmgr.h"
#include "llinventory.h"
#include "llinventorymodel.h"
#include "lllogchat.h"
#include "llmutelist.h"
//MK
#include "mkrlinterface.h"
//mk
#include "lltooldraganddrop.h"
#include "llviewertexteditor.h"
#include "llviewerstats.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llvoicechannel.h"
#include "llweb.h"

// Static variables
static std::string sTypingStartString;
static std::string sSessionStartString;
static std::string sDefaultTextString;
static std::string sUnavailableTextString;
static std::string sMutedTextString;

LLFloaterIMSession::instances_list_t LLFloaterIMSession::sFloaterIMSessions;

std::string LLFloaterIM::sOnlyUserMessage;
std::string LLFloaterIM::sOfflineMessage;
std::string LLFloaterIM::sMutedMessage;

LLFloaterIM::strings_map_t LLFloaterIM::sMsgStringsMap;

///////////////////////////////////////////////////////////////////////////////
// LLFloaterIMSession class
///////////////////////////////////////////////////////////////////////////////

//static
LLFloaterIMSession* LLFloaterIMSession::findInstance(const LLUUID& session_id)
{
	for (instances_list_t::const_iterator it = sFloaterIMSessions.begin(),
										  end = sFloaterIMSessions.end();
		 it != end; ++it)
	{
		LLFloaterIMSession* inst = *it;
		if (inst && inst->mSessionUUID == session_id)
		{
			return inst;
		}
	}

	return NULL;
}

//static
void LLFloaterIMSession::closeAllInstances()
{
	instances_list_t instances_copy = sFloaterIMSessions;
	while (!instances_copy.empty())
	{
		LLFloaterIMSession* inst = *instances_copy.begin();
		if (inst)
		{
			inst->setEnabled(false);
			inst->close(true);
			instances_copy.erase(inst);
		}
	}
}

LLFloaterIMSession::LLFloaterIMSession(const std::string& session_label,
									   const LLUUID& session_id,
									   const LLUUID& other_participant_id,
									   EInstantMessage dialog)
:	LLFloater(session_label, LLRect(), session_label),
	mSendButton(NULL),
	mOpenTextEditorButton(NULL),
	mStartCallButton(NULL),
	mEndCallButton(NULL),
	mSnoozeButton(NULL),
	mViewLogButton(NULL),
	mToggleSpeakersButton(NULL),
	mSpeakerVolumeSlider(NULL),
	mMuteButton(NULL),
	mSessionUUID(session_id),
	mVoiceChannel(NULL),
	mSessionInitialized(false),
	mOtherParticipantUUID(other_participant_id),
	mDialog(dialog),
	mIsGroupSession(false),
	mHasScrolledOnce(false),
	mTyping(false),
	mOtherTyping(false),
	mTypingLineStartIndex(0),
	mSentTypingState(true),
	mNumUnreadMessages(0),
	mShowSpeakersOnConnect(true),
	mAutoConnect(false),
	mTextIMPossible(true),
	mProfileButtonEnabled(true),
	mCallBackEnabled(true),
	mFetchingLog(false),
	mSpeakers(NULL),
	mSpeakerPanel(NULL)
{
	sFloaterIMSessions.insert(this);
	init(session_label);
}

LLFloaterIMSession::LLFloaterIMSession(const std::string& session_label,
									   const LLUUID& session_id,
									   const LLUUID& other_participant_id,
									   const uuid_vec_t& ids,
									   EInstantMessage dialog)
:	LLFloater(session_label, LLRect(), session_label),
	mSessionUUID(session_id),
	mOtherParticipantUUID(other_participant_id),
	mSendButton(NULL),
	mOpenTextEditorButton(NULL),
	mStartCallButton(NULL),
	mEndCallButton(NULL),
	mSnoozeButton(NULL),
	mToggleSpeakersButton(NULL),
	mSpeakerVolumeSlider(NULL),
	mMuteButton(NULL),
	mVoiceChannel(NULL),
	mSpeakers(NULL),
	mSpeakerPanel(NULL),
	mSessionInitialized(false),
	mSnoozeDuration(0),
	mDialog(dialog),
	mIsGroupSession(false),
	mHasScrolledOnce(false),
	mTyping(false),
	mOtherTyping(false),
	mTypingLineStartIndex(0),
	mSentTypingState(true),
	mShowSpeakersOnConnect(true),
	mAutoConnect(false),
	mTextIMPossible(true),
	mProfileButtonEnabled(true),
	mCallBackEnabled(true)
{
	sFloaterIMSessions.insert(this);
	mSessionInitialTargetIDs = ids;
	init(session_label);
}

void LLFloaterIMSession::init(const std::string& session_label)
{
	mSessionLabel = mSessionLog = session_label;
	mProfileButtonEnabled = false;

	std::string xml_filename;
	switch (mDialog)
	{
		case IM_SESSION_GROUP_START:
			mFactoryMap["active_speakers_panel"] =
				LLCallbackMap(createSpeakersPanel, this);
			xml_filename = "floater_instant_message_group.xml";
			mIsGroupSession = true;
			mVoiceChannel = new LLVoiceChannelGroup(mSessionUUID,
													mSessionLabel);
			break;

		case IM_SESSION_INVITE:
			mFactoryMap["active_speakers_panel"] =
				LLCallbackMap(createSpeakersPanel, this);
			if (gAgent.isInGroup(mSessionUUID, true))
			{
				xml_filename = "floater_instant_message_group.xml";
				mIsGroupSession = true;
			}
			else // Must be invite to ad hoc IM
			{
				xml_filename = "floater_instant_message_ad_hoc.xml";
			}
			mVoiceChannel = new LLVoiceChannelGroup(mSessionUUID,
													mSessionLabel);
			break;

		case IM_SESSION_P2P_INVITE:
			xml_filename = "floater_instant_message.xml";
			mProfileButtonEnabled = true;
			if (LLAvatarName::sOmitResidentAsLastName)
			{
				mSessionLabel = LLCacheName::cleanFullName(mSessionLabel);
			}
			mVoiceChannel = new LLVoiceChannelP2P(mSessionUUID, mSessionLabel,
												  mOtherParticipantUUID);
			break;

		case IM_SESSION_CONFERENCE_START:
			mFactoryMap["active_speakers_panel"] =
				LLCallbackMap(createSpeakersPanel, this);
			xml_filename = "floater_instant_message_ad_hoc.xml";
			mVoiceChannel = new LLVoiceChannelGroup(mSessionUUID,
													mSessionLabel);
			break;

		case IM_NOTHING_SPECIAL:	// Just received text from another user
			xml_filename = "floater_instant_message.xml";
			mTextIMPossible =
				gVoiceClient.isSessionTextIMPossible(mSessionUUID);
			mProfileButtonEnabled =
				gVoiceClient.isParticipantAvatar(mSessionUUID);
			if (mProfileButtonEnabled && LLAvatarName::sOmitResidentAsLastName)
			{
				mSessionLabel = LLCacheName::cleanFullName(mSessionLabel);
			}
			mCallBackEnabled =
				gVoiceClient.isSessionCallBackPossible(mSessionUUID);
			mVoiceChannel = new LLVoiceChannelP2P(mSessionUUID, mSessionLabel,
												  mOtherParticipantUUID);
			break;

		default:
			llwarns << "Unknown session type" << llendl;
			xml_filename = "floater_instant_message.xml";
	}

	mSpeakers = new LLIMSpeakerMgr(mVoiceChannel);

	LLUICtrlFactory::getInstance()->buildFloater(this, xml_filename,
												 &getFactoryMap(), false);

	if (mProfileButtonEnabled && mSessionLog.find(' ') == std::string::npos)
	{
		// Make sure the IM log file will be unique (avoid getting both
		// "JohnDoe.txt" and "JohnDoe Resident.txt", depending on how the IM
		// session was started)
		mSessionLog += " Resident";
	}

	setTitle(mSessionLabel);
	if (mProfileButtonEnabled)
	{
		lookupName();
	}

	mInputEditor->setMaxTextLength(DB_IM_MSG_STR_LEN);
	// Enable line history support for instant message bar
	mInputEditor->setEnableLineHistory(true);

	if (mViewLogButton)
	{
		// This button is visible only if a log file exists
		mViewLogButton->setVisible(false);
	}

	if (gSavedPerAccountSettings.getBool("LogShowHistory"))
	{
		LLUUID log_session_id;
		if (mIsGroupSession)
		{
			log_session_id = mSessionUUID;
		}
		LLLogChat::loadHistory(mSessionLog, &chatFromLog, (void*)this,
							   log_session_id);
	}

	if (!mSessionInitialized)
	{
		if (!LLIMMgr::sendStartSessionMessages(mSessionUUID,
											   mOtherParticipantUUID,
						 					   mSessionInitialTargetIDs,
											   mDialog))
		{
			// We do not need to need to wait for any responses so we are
			// already initialized
			mSessionInitialized = true;
		}
		else
		{
			// Use the starting session message as the input line editor label.
			// We used to echo a temporary message in the text editor, but it
			// relied on the fact that it would stay the last printed message,
			// so that we could remove it later; this is no more true with the
			// server logs asynchronous fetching. HB
			LLUIString session_start = sSessionStartString;
			session_start.setArg("[NAME]", getTitle());
			mInputEditor->setEnabled(false);
			mInputEditor->setLabel(session_start);
		}
	}
}

void LLFloaterIMSession::lookupName()
{
	LLAvatarNameCache::get(mOtherParticipantUUID,
						   boost::bind(&LLFloaterIMSession::onAvatarNameLookup,
									   _1, _2, this));
}

//static
void LLFloaterIMSession::onAvatarNameLookup(const LLUUID& id,
											const LLAvatarName& avatar_name,
											void* user_data)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)user_data;

	if (self && sFloaterIMSessions.count(self) != 0)
	{
		// Always show "Display Name [Legacy Name]" for security reasons
		std::string title = avatar_name.getNames();
		if (!title.empty())
		{
			self->setTitle(title);
		}
	}
}

LLFloaterIMSession::~LLFloaterIMSession()
{
	sFloaterIMSessions.erase(this);

	delete mSpeakers;
	mSpeakers = NULL;

#if 0	// Vivox text IMs are not in use.
	// End the text IM session if necessary
	if (mOtherParticipantUUID.notNull())
	{
		if (mDialog == IM_NOTHING_SPECIAL || mDialog == IM_SESSION_P2P_INVITE)
		{
			gVoiceClient.endUserIMSession(mOtherParticipantUUID);
		}
	}
#endif

	// Kicks you out of the voice channel if it is currently active

	// HAVE to do this here: if it happens in the LLVoiceChannel destructor it
	// will call the wrong version (since the object's partially deconstructed
	// at that point).
	mVoiceChannel->deactivate();

	delete mVoiceChannel;
	mVoiceChannel = NULL;

	// Delete focus lost callback
	mInputEditor->setFocusLostCallback(NULL);
}

bool LLFloaterIMSession::postBuild()
{
	if (sDefaultTextString.empty())
	{
		sDefaultTextString = getString("default_text_label");
		sSessionStartString = getString("session_start_string");
		sTypingStartString = getString("typing_start_string");
		sUnavailableTextString = getString("unavailable_text_label");
		sMutedTextString = getString("muted_text_label");
	}

	mInputEditor = getChild<LLLineEditor>("chat_editor");
	mInputEditor->setFocusReceivedCallback(onInputEditorFocusReceived, this);
	mInputEditor->setFocusLostCallback(onInputEditorFocusLost, this);
	mInputEditor->setKeystrokeCallback(onInputEditorKeystroke);
	mInputEditor->setScrolledCallback(onInputEditorScrolled, this);
	mInputEditor->setCommitCallback(onCommitChat);
	mInputEditor->setCallbackUserData(this);
	mInputEditor->setCommitOnFocusLost(false);
	mInputEditor->setRevertOnEsc(false);
	mInputEditor->setReplaceNewlinesWithSpaces(false);
	mInputEditor->setCustomMenuType("im_input");

	if (getChild<LLFlyoutButton>("avatar_btn", true, false))
	{
		childSetCommitCallback("avatar_btn", onCommitAvatar, this);
		if (!mProfileButtonEnabled)
		{
			childSetEnabled("avatar_btn", false);
		}
	}
	if (getChild<LLButton>("group_info_btn", true, false))
	{
		childSetAction("group_info_btn", onClickGroupInfo, this);
	}

	mStartCallButton = getChild<LLButton>("start_call_btn", true, false);
	if (mStartCallButton)
	{
		mStartCallButton->setClickedCallback(onClickStartCall, this);
		mEndCallButton = getChild<LLButton>("end_call_btn");
		mEndCallButton->setClickedCallback(onClickEndCall, this);
	}

	mViewLogButton = getChild<LLButton>("view_log_btn", true, false);
	if (mViewLogButton)
	{
		mViewLogButton->setClickedCallback(onClickViewLog, this);
	}

	mSendButton = getChild<LLButton>("send_btn", true, false);
	if (mSendButton)
	{
		mSendButton->setClickedCallback(onClickSend, this);
	}

	mOpenTextEditorButton = getChild<LLButton>("open_text_editor_btn", true,
											   false);
	if (mOpenTextEditorButton)
	{
		mOpenTextEditorButton->setClickedCallback(onClickOpenTextEditor, this);
	}

	mToggleSpeakersButton = getChild<LLButton>("toggle_active_speakers_btn",
											   true, false);
	if (mToggleSpeakersButton)
	{
		mToggleSpeakersButton->setClickedCallback(onClickToggleActiveSpeakers,
												  this);
	}
#if 0
	LLButton* close_btn = getChild<LLButton>("close_btn");
	close_btn->setClickedCallback(&LLFloaterIMSession::onClickClose, this);
#endif
	mHistoryEditor = getChild<LLViewerTextEditor>("im_history");
	mHistoryEditor->setParseHTML(true);
	mHistoryEditor->setCustomMenuType("im_history");

	if (mIsGroupSession)
	{
		mSnoozeButton = getChild<LLButton>("snooze_btn");
		mSnoozeButton->setClickedCallback(onClickSnooze, this);
		childSetEnabled("profile_btn", false);
	}

	if (mSpeakerPanel)
	{
		mSpeakerPanel->refreshSpeakers();
	}

	if (mDialog == IM_NOTHING_SPECIAL)
	{
		mMuteButton = getChild<LLButton>("mute_btn", true, false);
		if (mMuteButton)
		{
			mMuteButton->setClickedCallback(onClickMuteVoice, this);
			mSpeakerVolumeSlider = getChild<LLSlider>("speaker_volume");
			childSetCommitCallback("speaker_volume", onVolumeChange, this);
		}
	}

	setDefaultBtn("send_btn");

	return true;
}

bool LLFloaterIMSession::setSnoozeDuration(U32 duration)
{
	if (mIsGroupSession)
	{
		mSnoozeDuration = duration;
		return true;
	}
	return false;
}

void* LLFloaterIMSession::createSpeakersPanel(void* data)
{
	LLFloaterIMSession* floaterp = (LLFloaterIMSession*)data;
	if (floaterp && floaterp->mSpeakers)
	{
		floaterp->mSpeakerPanel =
			new LLPanelActiveSpeakers(floaterp->mSpeakers, true);
		return floaterp->mSpeakerPanel;
	}
	else
	{
		if (floaterp)
		{
			llwarns << "NULL LLIMSpeakerMgr object" << llendl;
		}
		else
		{
			llwarns  << "Called with a NULL pointer" << llendl;
			llassert(false);
		}
		return NULL;
	}
}

//static
void LLFloaterIMSession::onClickMuteVoice(void* user_data)
{
	LLFloaterIMSession* floaterp = (LLFloaterIMSession*)user_data;
	if (floaterp)
	{
		bool is_muted = LLMuteList::isMuted(floaterp->mOtherParticipantUUID,
											LLMute::flagVoiceChat);

		LLMute mute(floaterp->mOtherParticipantUUID, floaterp->getTitle(),
					LLMute::AGENT);
		if (!is_muted)
		{
			LLMuteList::add(mute, LLMute::flagVoiceChat);
		}
		else
		{
			LLMuteList::remove(mute, LLMute::flagVoiceChat);
		}
	}
}

//static
void LLFloaterIMSession::onVolumeChange(LLUICtrl* source, void* user_data)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)user_data;
	if (self)
	{
		gVoiceClient.setUserVolume(self->mOtherParticipantUUID,
								   (F32)source->getValue().asReal());
	}
}

// virtual
void LLFloaterIMSession::draw()
{
	bool voice_enabled = LLVoiceClient::voiceEnabled();
	bool enable_connect = mCallBackEnabled && mSessionInitialized &&
						  voice_enabled;

	if (mStartCallButton)
	{
		// Hide/show start call and end call buttons
		bool call_started =
			mVoiceChannel &&
			mVoiceChannel->getState() >= LLVoiceChannel::STATE_CALL_STARTED;

		mStartCallButton->setVisible(voice_enabled && !call_started);
		mStartCallButton->setEnabled(enable_connect);
		mEndCallButton->setVisible(voice_enabled && call_started);
	}

	if (mSnoozeButton)
	{
		static LLCachedControl<U32> snooze_duration(gSavedSettings,
													"GroupIMSnoozeDuration");
		mSnoozeButton->setVisible(snooze_duration > 0);
	}

	bool has_text_editor = HBFloaterTextInput::hasFloaterFor(mInputEditor);
	bool empty = mInputEditor->getText().size() == 0;
	if (empty && !has_text_editor)
	{
		// Reset this flag if the chat input line is empty
		mHasScrolledOnce = false;
	}
	if (mSendButton)
	{
		mSendButton->setEnabled(!empty && !has_text_editor);
	}

	// Test mSessionInitialized to keep "Starting session..." when not yet
	// ready. HB
	if (mSessionInitialized)
	{
		LLPointer<LLSpeaker> self_speaker = mSpeakers->findSpeaker(gAgentID);
		if (!mTextIMPossible)
		{
			mInputEditor->setEnabled(false);
			mInputEditor->setLabel(sUnavailableTextString);
		}
		else if (self_speaker.notNull() && self_speaker->mModeratorMutedText)
		{
			mInputEditor->setEnabled(false);
			mInputEditor->setLabel(sMutedTextString);
		}
		else
		{
			mInputEditor->setEnabled(!has_text_editor);
			mInputEditor->setLabel(sDefaultTextString);
		}
	}

	if (mAutoConnect && enable_connect)
	{
		onClickStartCall(this);
		mAutoConnect = false;
	}

	// Show speakers window when voice first connects
	if (mShowSpeakersOnConnect && mSpeakerPanel && mVoiceChannel->isActive())
	{
		mSpeakerPanel->setVisible(true);
		mShowSpeakersOnConnect = false;
	}
	if (mToggleSpeakersButton)
	{
		mToggleSpeakersButton->setValue(mSpeakerPanel &&
										mSpeakerPanel->getVisible());
	}

	if (mTyping)
	{
		// Time out if user has not typed for a while.
		if (mLastKeystrokeTimer.getElapsedTimeF32() >
				LLAgent::TYPING_TIMEOUT_SECS)
		{
			setTyping(false);
		}

		// If we are typing, and it has been a little while, send the
		// typing indicator
		if (!mSentTypingState &&
			mFirstKeystrokeTimer.getElapsedTimeF32() > 1.f)
		{
			sendTypingState(true);
			mSentTypingState = true;
		}
	}

	// Use embedded panel if available
	if (mSpeakerPanel)
	{
		if (mSpeakerPanel->getVisible())
		{
			mSpeakerPanel->refreshSpeakers();
		}
	}
	else if (mMuteButton)
	{
		// Refresh volume and mute
		mSpeakerVolumeSlider->setVisible(voice_enabled &&
										 mVoiceChannel->isActive());
		mSpeakerVolumeSlider->setValue(gVoiceClient.getUserVolume(mOtherParticipantUUID));

		mMuteButton->setValue(LLMuteList::isMuted(mOtherParticipantUUID,
												  LLMute::flagVoiceChat));
		mMuteButton->setVisible(voice_enabled && mVoiceChannel->isActive());
	}
	LLFloater::draw();
}

bool LLFloaterIMSession::inviteToSession(const uuid_vec_t& ids)
{
	const std::string& url = gAgent.getRegionCapability("ChatSessionRequest");
	if (url.empty())
	{
		return false;
	}

	S32 count = ids.size();
	if (isInviteAllowed() && count > 0)
	{
		llinfos << "Inviting participants" << llendl;

		LLSD data;
		data["params"] = LLSD::emptyArray();
		for (S32 i = 0; i < count; ++i)
		{
			data["params"].append(ids[i]);
		}
		data["method"] = "invite";
		data["session-id"] = mSessionUUID;
		LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, data,
															  "Session invite sent",
															  "Session invite failed");
	}
	else
	{
		llinfos << "No need to invite agents for " << mDialog << llendl;
		// Successful add, because everyone that needed to get added was added.
	}

	return true;
}

void LLFloaterIMSession::addQueuedMessages()
{
	mFetchingLog = false;
	for (U32 i = 0, count = mMessagesBuffer.size(); i < count; ++i)
	{
		const QueuedMessage& data = mMessagesBuffer[i];
		addHistoryLine(data.mText, data.mColor, data.mLog, data.mSourceId,
					   data.mFrom);
	}
	mMessagesBuffer.clear();
}

void LLFloaterIMSession::addHistoryLine(const std::string& utf8msg,
										const LLColor4& color,
										bool log_to_file,
										const LLUUID& source,
										const std::string& const_name)
{
	if (mFetchingLog)
	{
		// We must queue this message until the log is fully recovered. HB
		mMessagesBuffer.emplace_back(source, const_name, utf8msg, color,
									 log_to_file);
		return;
	}

	std::string name = const_name;
	// Start tab flashing when receiving im for background session from user
	if (source.notNull())
	{
		LLMultiFloater* hostp = getHost();
		if (!isInVisibleChain() && hostp && source != gAgentID)
		{
			hostp->setFloaterFlashing(this, true);
		}
	}

	// Now we are adding the actual line of text, so erase the "Foo is
	// typing..." text segment, and the optional timestamp if it was present.
	// JC
	removeTypingIndicator();

	// Actually add the line
	std::string timestring;
	bool prepend_newline = true;
	static LLCachedControl<bool> show_time(gSavedSettings, "IMShowTimestamps");
	if (show_time)
	{
		timestring = mHistoryEditor->appendTime(prepend_newline);
		prepend_newline = false;
	}

	// 'name' is a sender name that we want to hotlink so that clicking on it
	// opens a profile. If name exists then add it to the front of the message.
	if (!name.empty())
	{
		// Do not hotlink any messages from the system (e.g. "Second Life:"),
		// so just add those in plain text.
		if (name == SYSTEM_FROM)
		{
			mHistoryEditor->appendColoredText(name, false, prepend_newline,
											  color);
		}
		else
		{
			LLUUID av_id = source;
			if (av_id.isNull())
			{
				std::string self_name;
				gAgent.buildFullname(self_name);
				if (name == self_name)
				{
					av_id = gAgentID;
				}
			}
			else if (LLAvatarNameCache::useDisplayNames())
			{
				LLAvatarName avatar_name;
				if (LLAvatarNameCache::get(av_id, &avatar_name))
				{
					if (LLAvatarNameCache::useDisplayNames() == 2)
					{
						name = avatar_name.mDisplayName;
					}
					else
					{
						name = avatar_name.getNames();
					}
				}
			}
			// Convert the name to a hotlink and add to message.
			const LLStyleSP& source_style = gStyleMap.lookupAgent(source);
			mHistoryEditor->appendStyledText(name, false, prepend_newline,
											 source_style);
		}
		prepend_newline = false;
	}
	mHistoryEditor->appendColoredText(utf8msg, false, prepend_newline, color);

	if (log_to_file)
	{
		logToFile(name + utf8msg);
	}

	if (!isInVisibleChain())
	{
		++mNumUnreadMessages;
	}

	if (source.notNull())
	{
		mSpeakers->speakerChatted(source);
		mSpeakers->setSpeakerTyping(source, false);
		if (mSpeakerPanel)
		{
			// Make sure this speaker is listed...
			mSpeakerPanel->addSpeaker(source, true);
			if (source != gAgentID)
			{
				// And that we are here too !
				mSpeakerPanel->addSpeaker(gAgentID, true);
			}
		}
	}
}

void LLFloaterIMSession::logToFile(const std::string& line,
								   bool allow_timestamp)
{
	static LLCachedControl<bool> log_im(gSavedPerAccountSettings,
										"LogInstantMessages");
	if (!log_im)
	{
		return;
	}

	static LLCachedControl<bool> stamp(gSavedPerAccountSettings,
									   "IMLogTimestamp");
	if (allow_timestamp && stamp)
	{
		LLLogChat::saveHistory(mSessionLog, LLLogChat::timestamp() + line);
	}
	else
	{
		LLLogChat::saveHistory(mSessionLog, line);
	}
}

void LLFloaterIMSession::setVisible(bool b)
{
	LLPanel::setVisible(b);

	LLMultiFloater* hostp = getHost();
	if (b && hostp)
	{
		hostp->setFloaterFlashing(this, false);
	}
}

void LLFloaterIMSession::setInputFocus(bool b)
{
	mInputEditor->setFocus(b);
}

void LLFloaterIMSession::selectAll()
{
	mInputEditor->selectAll();
}

void LLFloaterIMSession::selectNone()
{
	mInputEditor->deselect();
}

bool LLFloaterIMSession::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;
	if (key == KEY_RETURN)
	{
		if (HBFloaterTextInput::hasFloaterFor(mInputEditor))
		{
			HBFloaterTextInput::show(mInputEditor);
			return true;
		}

		if (mask == MASK_NONE || mask == MASK_CONTROL || mask == MASK_SHIFT)
		{
			sendMsg();
			handled = true;

			// Close talk panels on hitting return but not shift-return or
			// control-return
			if (gIMMgrp && !gSavedSettings.getBool("PinTalkViewOpen") &&
				!(mask & MASK_CONTROL) && !(mask & MASK_SHIFT))
			{
				gIMMgrp->toggle(NULL);
			}
		}
		else if (mask == (MASK_SHIFT | MASK_CONTROL))
		{
			S32 cursor = mInputEditor->getCursor();
			std::string text = mInputEditor->getText();
			// For some reason, the event is triggered twice: let's insert only
			// one newline character.
			if (cursor == 0 || text[cursor - 1] != '\n')
			{
				text = text.insert(cursor, "\n");
				mInputEditor->setText(text);
				mInputEditor->setCursor(cursor + 1);
			}
			handled = true;
		}
	}
	else if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		handled = true;
		gFocusMgr.setKeyboardFocus(NULL);

		// Close talk panel with escape
		if (gIMMgrp && !gSavedSettings.getBool("PinTalkViewOpen"))
		{
			gIMMgrp->toggle(NULL);
		}
	}

	// May need to call base class LLPanel::handleKeyHere if not handled in
	// order to tab between buttons. JNC 1.2.2002
	return handled;
}

bool LLFloaterIMSession::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
										   EDragAndDropType cargo_type,
										   void* cargo_data,
										   EAcceptance* accept,
										   std::string& tooltip_msg)
{

	if (mDialog == IM_NOTHING_SPECIAL)
	{
		LLToolDragAndDrop::handleGiveDragAndDrop(mOtherParticipantUUID,
												 mSessionUUID, drop,
												 cargo_type, cargo_data,
												 accept);
	}
	else if (isInviteAllowed())
	{
		// Handle case for dropping calling cards (and folders of calling
		// cards) onto invitation panel for invites

		*accept = ACCEPT_NO;
		if (cargo_type == DAD_CALLINGCARD)
		{
			if (dropCallingCard((LLInventoryItem*)cargo_data, drop))
			{
				*accept = ACCEPT_YES_MULTI;
			}
		}
		else if (cargo_type == DAD_CATEGORY)
		{
			if (dropCategory((LLInventoryCategory*)cargo_data, drop))
			{
				*accept = ACCEPT_YES_MULTI;
			}
		}
	}

	return true;
}

bool LLFloaterIMSession::dropCallingCard(LLInventoryItem* item, bool drop)
{
	bool rv = isInviteAllowed();
	if (rv && item && item->getCreatorUUID().notNull())
	{
		if (drop)
		{
			uuid_vec_t ids;
			ids.emplace_back(item->getCreatorUUID());
			inviteToSession(ids);
		}
	}
	else
	{
		// Set to false if creator uuid is null.
		rv = false;
	}
	return rv;
}

bool LLFloaterIMSession::dropCategory(LLInventoryCategory* category, bool drop)
{
	bool rv = isInviteAllowed();
	if (rv && category)
	{
		LLInventoryModel::cat_array_t cats;
		LLInventoryModel::item_array_t items;
		LLUniqueBuddyCollector buddies;
		gInventory.collectDescendentsIf(category->getUUID(), cats, items,
										LLInventoryModel::EXCLUDE_TRASH,
										buddies);
		U32 count = items.size();
		if (!count)
		{
			return false;
		}

		if (drop)
		{
			uuid_vec_t ids;
			for (U32 i = 0; i < count; ++i)
			{
				ids.emplace_back(items[i]->getCreatorUUID());
			}
			inviteToSession(ids);
		}
	}
	return rv;
}

bool LLFloaterIMSession::isInviteAllowed() const
{
	return mDialog == IM_SESSION_CONFERENCE_START ||
		   mDialog == IM_SESSION_INVITE;
}

//static
void LLFloaterIMSession::onTabClick(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	if (self)
	{
		self->setInputFocus(true);
	}
}

//static
void LLFloaterIMSession::onCommitAvatar(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	if (!self || !ctrl) return;

	LLUUID id = self->mOtherParticipantUUID;
	if (id.notNull())
	{
		std::string valstr = ctrl->getValue().asString();
		if (valstr == "offer_tp")
		{
			LLAvatarActions::offerTeleport(id);
		}
		else if (valstr == "request_tp")
		{
			LLAvatarActions::teleportRequest(id);
		}
		else
		{
			// Bring up the Profile window
			LLFloaterAvatarInfo::showFromDirectory(id);
		}
	}
}

//static
void LLFloaterIMSession::onClickViewLog(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	if (self && !self->mLogFileName.empty() && gViewerWindowp)
	{
#if LL_WINDOWS
		std::string url = "file:///";
#else
		std::string url = "file://";
#endif
		url += LLWeb::escapeURL(self->mLogFileName);
		if (gSavedPerAccountSettings.getBool("OpenIMLogsInBuiltInBrowser"))
		{
			LLFloaterMediaBrowser::showInstance(url);
		}
		else
		{
			gWindowp->spawnWebBrowser(url, true);
		}
	}
}

//static
void LLFloaterIMSession::onClickGroupInfo(void* userdata)
{
	//  Bring up the Profile window
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;

	LLFloaterGroupInfo::showFromUUID(self->mSessionUUID);
}

//static
void LLFloaterIMSession::onClickClose(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterIMSession::onClickSnooze(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	if (self)
	{
		if (self->mIsGroupSession)
		{
			self->mSnoozeDuration =
				gSavedSettings.getU32("GroupIMSnoozeDuration");
		}
		self->close();
	}
}

//static
void LLFloaterIMSession::onClickStartCall(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	self->mVoiceChannel->activate();
}

//static
void LLFloaterIMSession::onClickEndCall(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	self->getVoiceChannel()->deactivate();
}

//static
void LLFloaterIMSession::onClickSend(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	self->sendMsg();
}

//static
void LLFloaterIMSession::onClickOpenTextEditor(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	if (self && !self->mSessionLabel.empty())
	{
		self->mHasScrolledOnce = true;
		HBFloaterTextInput::show(self->mInputEditor, self->mSessionLabel,
								 &setIMTyping, self);
	}
}

//static
void LLFloaterIMSession::onClickToggleActiveSpeakers(void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	if (self && self->mSpeakerPanel)
	{
		self->mSpeakerPanel->setVisible(!self->mSpeakerPanel->getVisible());
	}
}

//static
void LLFloaterIMSession::onCommitChat(LLUICtrl* caller, void* userdata)
{
	LLFloaterIMSession* self= (LLFloaterIMSession*)userdata;
	self->sendMsg();
}

//static
void LLFloaterIMSession::onInputEditorFocusReceived(LLFocusableElement* caller,
													void* userdata)
{
	LLFloaterIMSession* self= (LLFloaterIMSession*)userdata;
	self->mHistoryEditor->setCursorAndScrollToEnd();
}

//static
void LLFloaterIMSession::onInputEditorFocusLost(LLFocusableElement* caller,
												void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	self->setTyping(false);
}

//static
void LLFloaterIMSession::onInputEditorKeystroke(LLLineEditor* caller,
												void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	std::string text = self->mInputEditor->getText();
	if (!text.empty())
	{
		self->setTyping(true);
	}
	else
	{
		// Deleting all text counts as stopping typing.
		self->setTyping(false);
	}
}

//static
void LLFloaterIMSession::onInputEditorScrolled(LLLineEditor* caller,
											   void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;
	if (!self || !caller) return;

	if (!self->mHasScrolledOnce && !self->mSessionLabel.empty() &&
		gSavedSettings.getBool("AutoOpenTextInput"))
	{
		self->mHasScrolledOnce = true;
		HBFloaterTextInput::show(caller, self->mSessionLabel, &setIMTyping,
								 self);
	}
}

void LLFloaterIMSession::onClose(bool app_quitting)
{
	HBFloaterTextInput::abort(mInputEditor);

	setTyping(false);

	if (gIMMgrp)
	{
		gIMMgrp->removeSession(mSessionUUID, mOtherParticipantUUID,
							   mSnoozeDuration);
	}

	destroy();
}

void LLFloaterIMSession::onVisibilityChange(bool new_visibility)
{
	if (new_visibility)
	{
		mNumUnreadMessages = 0;
	}
}

void LLFloaterIMSession::sendText(LLWString text)
{
	if (!gAgent.isGodlike() && mDialog == IM_NOTHING_SPECIAL &&
		mOtherParticipantUUID.isNull())
	{
		llinfos << "Cannot send IM to everyone unless you are a god."
				<< llendl;
		return;
	}

//MK
	if (gRLenabled)
	{
		bool allowed;
		if (mIsGroupSession)
		{
			allowed = gRLInterface.canSendGroupIM(mSessionLabel);
		}
		else
		{
			allowed = gRLInterface.canSendIM(mOtherParticipantUUID);
		}
		if (!allowed)
		{
			// User is forbidden to send IMs and the receiver is no exception
			// signal both the sender and the receiver
			text = utf8str_to_wstring(RLInterface::sSendimMessage);
		}
	}
//mk
	if (!text.empty())
	{
		// Store sent line in history, duplicates will get filtered
		mInputEditor->updateHistory();

		// Convert to UTF8 for transport
		std::string utf8_text = wstring_to_utf8str(text);

		if (utf8_text.length() > 3)
		{
			if (gSavedSettings.getBool("AutoCloseOOC"))
			{
				// Try to find any unclosed OOC chat (i.e. an opening double
				// parenthesis without a matching closing double parenthesis.
				size_t i = utf8_text.find("((");
				if (i != std::string::npos)
				{
					size_t j = utf8_text.rfind("))");
					if (j == std::string::npos || j < i)
					{
						if (utf8_text.back() == ')')
						{
							// Cosmetic: add a space first to avoid a closing
							// triple parenthesis
							utf8_text += ' ';
						}
						// Add the missing closing double parenthesis.
						utf8_text += "))";
					}
				}
			}

			// Convert MU*s style poses into IRC emotes here.
			if (utf8_text[0] == ':'&& gSavedSettings.getBool("AllowMUpose"))
			{
				if (utf8_text.compare(0, 2, ":'") == 0)
				{
					utf8_text.replace(0, 1, "/me");
				}
				// Do not prevent smileys and such.
				else if (isalpha(utf8_text[1]))
				{
					utf8_text.replace(0, 1, "/me ");
				}
			}
		}

		// Truncate
		utf8_text = utf8str_truncate(utf8_text, MAX_MSG_BUF_SIZE - 1);

		if (mSessionInitialized)
		{
			LLIMMgr::deliverMessage(utf8_text, mSessionUUID,
									mOtherParticipantUUID, mDialog);

			// Local echo
			if (mDialog == IM_NOTHING_SPECIAL &&
				mOtherParticipantUUID.notNull())
			{
				std::string history_echo;
				gAgent.buildFullname(history_echo);
				if (LLAvatarNameCache::useDisplayNames())
				{
					LLAvatarName avatar_name;
					if (LLAvatarNameCache::get(gAgentID, &avatar_name))
					{
						if (LLAvatarNameCache::useDisplayNames() == 2)
						{
							history_echo = avatar_name.mDisplayName;
						}
						else
						{
							history_echo = avatar_name.getNames();
						}
					}
				}

				// Look for IRC-style emotes here.
				std::string prefix = utf8_text.substr(0, 4);
				if (prefix == "/me " || prefix == "/me'")
				{
					utf8_text.replace(0, 3, "");
				}
				else
				{
					history_echo += ": ";
				}
				history_echo += utf8_text;

				bool other_was_typing = mOtherTyping;

				addHistoryLine(history_echo,
							   gSavedSettings.getColor("IMChatColor"), true,
							   gAgentID);

				if (other_was_typing)
				{
					addTypingIndicator(mOtherParticipantUUID,
									   mOtherTypingName);
				}
			}
		}
		else
		{
			// Queue up the message to send once the session is initialized
			mQueuedMsgsForInit.append(utf8_text);
		}
	}

	gViewerStats.incStat(LLViewerStats::ST_IM_COUNT);

	// We do not need to actually send the typing stop message, the other
	// client will infer it from receiving the message.
	mTyping = false;
	mSentTypingState = true;
}

void LLFloaterIMSession::sendMsg()
{
	sendText(mInputEditor->getConvertedText());
	mInputEditor->setText(LLStringUtil::null);
}

void LLFloaterIMSession::updateSpeakersList(const LLSD& speaker_updates)
{
	mSpeakers->updateSpeakers(speaker_updates);
}

void LLFloaterIMSession::processSessionUpdate(const LLSD& session_update)
{
	if (session_update.has("moderated_mode") &&
		session_update["moderated_mode"].has("voice"))
	{
		bool voice_moderated = session_update["moderated_mode"]["voice"];
		if (voice_moderated)
		{
			setTitle(mSessionLabel + " " + getString("moderated_chat_label"));
		}
		else
		{
			setTitle(mSessionLabel);
		}

		// Update the speakers drop-down too
		mSpeakerPanel->setVoiceModerationCtrlMode(voice_moderated);
	}
}

void LLFloaterIMSession::setSpeakers(const LLSD& speaker_list)
{
	mSpeakers->setSpeakers(speaker_list);
}

void LLFloaterIMSession::sessionInitReplyReceived(const LLUUID& session_id)
{
	mSessionUUID = session_id;
	mVoiceChannel->updateSessionID(session_id);
	mSessionInitialized = true;

	// Reenable now that the session has started. The "Starting session..."
	// label will be reset to what it should finally be in draw(). HB
	mInputEditor->setEnabled(true);

	// And now, send the queued message.
	LLSD::array_iterator iter;
	for (iter = mQueuedMsgsForInit.beginArray();
		 iter != mQueuedMsgsForInit.endArray(); ++iter)
	{
		LLIMMgr::deliverMessage(iter->asString(), mSessionUUID,
								mOtherParticipantUUID, mDialog);
	}
}

void LLFloaterIMSession::requestAutoConnect()
{
	mAutoConnect = true;
}

void LLFloaterIMSession::setTyping(bool typing)
{
	if (typing)
	{
		// Every time the user types something, reset this timer
		mLastKeystrokeTimer.reset();

		if (!mTyping)
		{
			// The user just started typing.
			mFirstKeystrokeTimer.reset();

			// Will send typing state after a short delay.
			mSentTypingState = false;
		}

		mSpeakers->setSpeakerTyping(gAgentID, true);
	}
	else
	{
		if (mTyping)
		{
			// The user just stopped typing, send state immediately
			sendTypingState(false);
			mSentTypingState = true;
		}
		mSpeakers->setSpeakerTyping(gAgentID, false);
	}

	mTyping = typing;
}

void LLFloaterIMSession::sendTypingState(bool typing)
{
	// Do not want to send typing indicators to multiple people, potentially
	// too much network traffic. Only send in person-to-person IMs.
	if (mDialog != IM_NOTHING_SPECIAL) return;

	std::string name;
	gAgent.buildFullname(name);

	pack_instant_message(gAgentID, false, gAgentSessionID,
						 mOtherParticipantUUID, name, std::string("typing"),
						 IM_ONLINE, typing ? IM_TYPING_START : IM_TYPING_STOP,
						 mSessionUUID);
	gAgent.sendReliableMessage();
}

void LLFloaterIMSession::processIMTyping(const LLUUID& from_id,
										 const std::string& name, bool typing)
{
	if (typing)
	{
		// Other user started typing
		addTypingIndicator(from_id, name);
	}
	else
	{
		// Other user stopped typing
		removeTypingIndicator(from_id);
	}
}

void LLFloaterIMSession::addTypingIndicator(const LLUUID& from_id,
											const std::string& from_name)
{
	// We may have lost a "stop-typing" packet, do not add it twice.
	// Also, do not add any indicator while fetching the server log; for
	// now, this should never happen since the server log is so far reserved
	// to group IM sessions, for which the typing state is never sent. HB
	if (!mOtherTyping && !mFetchingLog)
	{
		mTypingLineStartIndex = mHistoryEditor->getWText().length();
		LLUIString typing_start = sTypingStartString;
		typing_start.setArg("[NAME]", from_name);
		addHistoryLine(typing_start,
					   gSavedSettings.getColor4("SystemChatColor"), false);
		mOtherTypingName = from_name;
		mOtherTyping = true;

		if (from_id.notNull())
		{
			mSpeakers->setSpeakerTyping(from_id, true);
		}
	}
}

void LLFloaterIMSession::removeTypingIndicator(const LLUUID& from_id)
{
	if (mOtherTyping)
	{
		// Must do this first, otherwise addHistoryLine calls us again.
		mOtherTyping = false;

		S32 chars_to_remove = mHistoryEditor->getWText().length() -
							  mTypingLineStartIndex;
		mHistoryEditor->removeTextFromEnd(chars_to_remove);
	}

	if (from_id.notNull())
	{
		mSpeakers->setSpeakerTyping(from_id, false);
	}
}

//static
void LLFloaterIMSession::setIMTyping(void* caller, bool typing)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)caller;
	if (self)
	{
		self->setTyping(typing);
	}
}

//static
void LLFloaterIMSession::chatFromLog(S32 type, const LLSD& data,
									 void* userdata)
{
	LLFloaterIMSession* self = (LLFloaterIMSession*)userdata;

	std::string message;
	LLColor4 color = LLColor4::grey;
	switch (type)
	{
		case LLLogChat::LOG_FILENAME:
			self->mLogFileName = data["filename"].asString();
			// Nothing to print in the IM window.
			return;

		case LLLogChat::LOG_SERVER_FETCH:
			// The server log is being fetched: at this point we must queue all
			// incoming messages until the full log has been (asynchronously)
			// recovered and printed into mHistoryEditor, or else we would get
			// out of order messages in the latter. HB
			self->mFetchingLog = true;
			// Nothing to print in the IM window.
			return;

		case LLLogChat::LOG_SERVER:
		{
			// Compare this log message against queued messages; the server
			// will have the first message of an opening sesssion already
			// logged when that message arrives on our side, for example. HB
			U32 count = self->mMessagesBuffer.size();
			if (count)
			{
				LLUUID src_id(data["from_id"].asString());
				message = data["message"].asString();
				for (U32 i = 0; i < count; ++i)
				{
					// Here, we can compare the message originator, based on
					// their UUID. HB
					const QueuedMessage& msg = self->mMessagesBuffer[i];
					if (src_id.notNull() && msg.mSourceId != src_id)
					{
						continue;
					}
					// Match the text. HB
					size_t pos = msg.mText.find(message);
					if (pos == std::string::npos)
					{
						continue;
					}
					if (pos + message.size() == msg.mText.size())
					{
						// This message is already queued. Skip from log. HB
						LL_DEBUGS("ServerIMLog") << "Skipping log server message that we did receive: "
												 << message << LL_ENDL;
						return;
					}
				}
			}
			// Add an IM chat log line, with a slighty lighter grey color. HB
			color = LLColor4::grey2;
			message = data["line"].asString();
			// Add it to our log file too, if configured for logging, but do
			// not attempt to add a timestamp, which was already added if the
			// user does want it, in LLLogChat::fetchHistoryCoro(). HB
			self->logToFile(message, false);
			break;
		}

		case LLLogChat::LOG_LINE:
			// Add an IM chat log line.
			message = data["line"].asString();
			break;


		case LLLogChat::LOG_END:
			// Add log end message
			if (gSavedPerAccountSettings.getBool("LogInstantMessages"))
			{
				message =
					LLFloaterChat::getInstance()->getString("IM_logging_string");
			}
			// Enable the View log button only when the file exists; it may
			// have been created as the result of the server log retreival,
			// thus why we waited until now to take this action. HB
			if (self->mViewLogButton && !self->mLogFileName.empty() &&
				LLFile::exists(self->mLogFileName))
			{
				self->mViewLogButton->setVisible(true);
			}
			break;

		default:
			llerrs << "Unknown callback response type: " << type << llendl;
	}

	if (!message.empty())
	{
		self->mHistoryEditor->appendColoredText(message, false, true, color);
	}
	// If the log is fully printed and messages got queued, add them now. HB
	if (type == LLLogChat::LOG_END && self->mFetchingLog)
	{
		self->addQueuedMessages();	// Note: this resets mFetchingLog. HB
	}
}

void LLFloaterIMSession::showSessionStartError(const std::string& error_string)
{
	// The error strings etc should really be static and local to this file
	// instead of in the LLFloaterIM, but they were in llimmgr.cpp first and
	// unfortunately some translations into non English languages already
	// occurred thus making it a tad harder to change over to a "correct"
	// solution. The best solution would be to store all of the misc strings
	// into their own XML file which would be read in by any LLIMPanel post
	// build function instead of repeating the same info in the group, adhoc
	// and normal IM xml files.
	LLSD args;
	args["REASON"] = LLFloaterIM::sMsgStringsMap[error_string];
	std::string recipient = getTitle();
	args["RECIPIENT"] = recipient.empty() ? mSessionUUID.asString()
										  : recipient;

	LLSD payload;
	payload["session_id"] = mSessionUUID;

	gNotifications.add("ChatterBoxSessionStartError", args, payload,
					   onConfirmForceCloseError);
}

void LLFloaterIMSession::showSessionEventError(const std::string& event_string,
											   const std::string& error_string)
{
	LLSD args;
	args["REASON"] = LLFloaterIM::sMsgStringsMap[error_string];
	LLUIString event_str = LLFloaterIM::sMsgStringsMap[event_string];
	std::string recipient = getTitle();
	event_str.setArg("[RECIPIENT]",
					 recipient.empty() ? mSessionUUID.asString() : recipient);
	args["EVENT"] = event_str;

	gNotifications.add("ChatterBoxSessionEventError", args);
}

void LLFloaterIMSession::showSessionForceClose(const std::string& reason_string)
{
	LLSD args;

	args["NAME"] = getTitle();
	args["REASON"] = LLFloaterIM::sMsgStringsMap[reason_string];

	LLSD payload;
	payload["session_id"] = mSessionUUID;

	gNotifications.add("ForceCloseChatterBoxSession", args, payload,
					   onConfirmForceCloseError);
}

bool LLFloaterIMSession::onConfirmForceCloseError(const LLSD& notification,
												  const LLSD& response)
{
	LLUUID session_id = notification["payload"]["session_id"];
	LLFloaterIMSession* floaterp = findInstance(session_id);
	if (floaterp)
	{
		floaterp->close();
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterIM class
///////////////////////////////////////////////////////////////////////////////

LLFloaterIM::LLFloaterIM()
{
	// autoresize=false is necessary to avoid resizing of the IM window
	// whenever a session is opened or closed (it would otherwise resize the
	// window to match the size of the im-sesssion when they were created.
	// This happens in LLMultiFloater::resizeToContents() when called through
	// LLMultiFloater::addFloater())
	this->mAutoResize = false;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_im.xml");
}

bool LLFloaterIM::postBuild()
{
	if (sOnlyUserMessage.empty())
	{
		sOnlyUserMessage = getString("only_user_message");
		sOfflineMessage = getString("offline_message");
		sMutedMessage = getString("muted_message");
		sMsgStringsMap["generic"] = getString("generic_request_error");
		sMsgStringsMap["unverified"] = getString("insufficient_perms_error");
		sMsgStringsMap["no_ability"] = getString("no_ability_error");
		sMsgStringsMap["muted"] = getString("muted_error");
		sMsgStringsMap["not_a_moderator"] = getString("not_a_mod_error");
		sMsgStringsMap["does not exist"] = getString("session_does_not_exist_error");
		sMsgStringsMap["add"] = getString("add_session_event");
		sMsgStringsMap["message"] = getString("message_session_event");
		sMsgStringsMap["removed"] = getString("removed_from_group");
		sMsgStringsMap["no ability"] = getString("close_on_no_ability");
	}

	return true;
}
