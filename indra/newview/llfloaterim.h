/**
 * @file llfloaterim.h
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

#ifndef LL_FLOATERIM_H
#define LL_FLOATERIM_H

#include "llavatarnamecache.h"
#include "hbfastset.h"
#include "llfloater.h"
#include "llinstantmessage.h"
#include "llstyle.h"
#include "lluistring.h"

class LLButton;
class LLLineEditor;
class LLViewerTextEditor;
class LLInventoryItem;
class LLInventoryCategory;
class LLIMSpeakerMgr;
class LLPanelActiveSpeakers;
class LLSlider;
class LLVoiceChannel;

class LLFloaterIMSession final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterIMSession);

public:
	// The session id is the id of the session this is for. The target refers
	// to the user (or group) that where this session serves as the default.
	// For example, if you open a session though a calling card, a new session
	// id will be generated, but the target_id will be the agent referenced by
	// the calling card.
	LLFloaterIMSession(const std::string& session_label,
					   const LLUUID& session_id, const LLUUID& target_id,
					   EInstantMessage dialog);
	LLFloaterIMSession(const std::string& session_label,
					   const LLUUID& session_id, const LLUUID& target_id,
					   const uuid_vec_t& ids, EInstantMessage dialog);
	~LLFloaterIMSession() override;

	void lookupName();
	static void onAvatarNameLookup(const LLUUID& id,
								   const LLAvatarName& avatar_name,
								   void* user_data);

	bool postBuild() override;

	void draw() override;
	void onClose(bool app_quitting = false) override;

	void setVisible(bool b) override;
	void onVisibilityChange(bool new_visibility) override;

	// Add target ids to the session.
	// Return true if successful, otherwise false.
	bool inviteToSession(const uuid_vec_t& agent_ids);

	void addHistoryLine(const std::string& utf8msg,
						const LLColor4& color = LLColor4::white,
						bool log_to_file = true,
						const LLUUID& source = LLUUID::null,
						const std::string& name = LLStringUtil::null);

	void setInputFocus(bool b);

	void selectAll();
	void selectNone();

	LL_INLINE S32 getNumUnreadMessages()				{ return mNumUnreadMessages; }

	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask,
						   bool drop, EDragAndDropType cargo_type,
						   void* cargo_data, EAcceptance* accept,
						   std::string& tooltip_msg) override;

	// Callbacks for P2P muting and volume control
	static void onClickMuteVoice(void* user_data);
	static void onVolumeChange(LLUICtrl* source, void* user_data);

	LL_INLINE const LLUUID& getSessionID() const		{ return mSessionUUID; }

	LL_INLINE const LLUUID& getOtherParticipantID() const
	{
		return mOtherParticipantUUID;
	}

	void updateSpeakersList(const LLSD& speaker_updates);
	void processSessionUpdate(const LLSD& update);
	void setSpeakers(const LLSD& speaker_list);

	LL_INLINE LLVoiceChannel* getVoiceChannel()			{ return mVoiceChannel; }
	LL_INLINE EInstantMessage getDialogType() const		{ return mDialog; }
	LL_INLINE bool isGroupSession() const				{ return mIsGroupSession; }
	LL_INLINE const std::string& getSessionName() const	{ return mSessionLabel; }

	void requestAutoConnect();

	void sessionInitReplyReceived(const LLUUID& im_session_id);

	// Handle other participant in the session typing.
	void processIMTyping(const LLUUID& from_id, const std::string& name,
						 bool typing);
	static void chatFromLog(S32 type, const LLSD& data, void* userdata);

	//show error statuses to the user
	void showSessionStartError(const std::string& error_string);
	void showSessionEventError(const std::string& event_string,
							   const std::string& error_string);
	void showSessionForceClose(const std::string& reason);

	static bool onConfirmForceCloseError(const LLSD& notification,
										 const LLSD& response);

	static LLFloaterIMSession* findInstance(const LLUUID& session_id);
	static void closeAllInstances();

	// Used before invoking close() in order to snooze a group IM session
	// instead of leaving it for good. 'duration' is in minutes. If the IM
	// session is not a group one, the method aborts and returns false.
	bool setSnoozeDuration(U32 duration);

	void sendText(LLWString text);

	// Used by llimmgr.cpp
	static void onClickStartCall(void* userdata);

private:
	// Called by constructors
	void init(const std::string& session_label);

	// Called by UI methods.
	void sendMsg();

	void addQueuedMessages();
	void logToFile(const std::string& line, bool allow_timestamp = true);

	// For adding agents via the UI. Return true if possible, do it if
	bool dropCallingCard(LLInventoryItem* item, bool drop);
	bool dropCategory(LLInventoryCategory* category, bool drop);

	// Test if local agent can add agents.
	bool isInviteAllowed() const;

	// Called whenever the user starts or stops typing.
	// Sends the typing state to the other user if necessary.
	void setTyping(bool typing);
	// Static, callback version
	static void setIMTyping(void* caller, bool typing);

	// Add the "User is typing..." indicator.
	void addTypingIndicator(const LLUUID& from_id,
							const std::string& from_name);

	// Remove the "User is typing..." indicator.
	void removeTypingIndicator(const LLUUID& from_id = LLUUID::null);

	void sendTypingState(bool typing);

	void disableWhileSessionStarting();

	static void onInputEditorFocusReceived(LLFocusableElement* caller,
										   void* userdata);
	static void onInputEditorFocusLost(LLFocusableElement* caller,
									   void* userdata);
	static void onInputEditorKeystroke(LLLineEditor* caller, void* userdata);
	static void onInputEditorScrolled(LLLineEditor* caller, void* userdata);
	static void onCommitChat(LLUICtrl* caller, void* userdata);
	static void onTabClick(void* userdata);

	static void onCommitAvatar(LLUICtrl* ctrl, void* userdata);

	static void onClickViewLog(void* userdata);
	static void onClickGroupInfo(void* userdata);
	static void onClickClose(void* userdata);
	static void onClickSnooze(void* userdata);
	static void onClickEndCall(void* userdata);
	static void onClickSend(void* userdata);
	static void onClickOpenTextEditor(void* userdata);
	static void onClickToggleActiveSpeakers(void* userdata);
	static void* createSpeakersPanel(void* data);

private:
	LLLineEditor*			mInputEditor;
	LLViewerTextEditor*		mHistoryEditor;
	LLButton*				mOpenTextEditorButton;
	LLButton*				mSendButton;
	LLButton*				mStartCallButton;
	LLButton*				mEndCallButton;
	LLButton*				mSnoozeButton;
	LLButton*				mViewLogButton;
	LLButton*				mToggleSpeakersButton;
	LLButton*				mMuteButton;
	LLSlider*				mSpeakerVolumeSlider;

	// The value of the mSessionUUID depends on how the IM session was started:
	//   one-on-one  ==> random id
	//   group ==> group_id
	//   inventory folder ==> folder item_id
	//   911 ==> Gaurdian_Angel_Group_ID ^ gAgentID
	LLUUID					mSessionUUID;

	std::string				mSessionLabel;
	std::string				mSessionLog;
	std::string				mLogFileName;

	LLVoiceChannel*			mVoiceChannel;

	LLSD					mQueuedMsgsForInit;

	// The value mOtherParticipantUUID depends on how the IM session was
	// started:
	//   one-on-one = recipient's id
	//   group ==> group_id
	//   inventory folder ==> first target id in list
	//   911 ==> sender
	LLUUID					mOtherParticipantUUID;
	uuid_vec_t				mSessionInitialTargetIDs;

	EInstantMessage			mDialog;

	// Name of other user who is currently typing
	std::string				mOtherTypingName;

	// Where does the "User is typing..." line start ?
	S32						mTypingLineStartIndex;

	S32						mNumUnreadMessages;

	U32 					mSnoozeDuration;

	LLIMSpeakerMgr*			mSpeakers;
	LLPanelActiveSpeakers*	mSpeakerPanel;

	// Optimization:  Don't send "User is typing..." until the user has
	// actually been typing for a little while. Prevents extra IMs for brief
	// "lol" type utterences.
	LLFrameTimer			mFirstKeystrokeTimer;

	// Timer to detect when user has stopped typing.
	LLFrameTimer			mLastKeystrokeTimer;

	// Used while fetching the log from the server, to queue incoming mesages
	// and avoid out of order displaying in mHistoryEditor. HB
	struct QueuedMessage
	{
		LL_INLINE QueuedMessage(const LLUUID& src_id, const std::string& from,
								const std::string& text, const LLColor4& color,
								bool log)
		:	mSourceId(src_id),
			mFrom(from),
			mText(text),
			mColor(color),
			mLog(log)
		{
		}

		LLUUID		mSourceId;
		std::string	mFrom;
		std::string	mText;
		LLColor4	mColor;
		bool		mLog;
	};
	typedef std::vector<QueuedMessage> messages_buffer_t;
	messages_buffer_t		mMessagesBuffer;

	// Set to true when fetching the server log and needing to queue incoming
	// messages till done. HB
	bool					mFetchingLog;

	bool					mSessionInitialized;
	bool 					mIsGroupSession;

	bool					mHasScrolledOnce;

	// Are you currently typing ?
	bool					mTyping;

	// Is other user currently typing ?
	bool					mOtherTyping;

	bool					mSentTypingState;

	bool					mShowSpeakersOnConnect;

	bool					mAutoConnect;

	bool					mTextIMPossible;
	bool					mProfileButtonEnabled;
	bool					mCallBackEnabled;

	typedef fast_hset<LLFloaterIMSession*> instances_list_t;
	static instances_list_t	sFloaterIMSessions;
};

class LLFloaterIM final : public LLMultiFloater
{
public:
	LLFloaterIM();

	bool postBuild() override;

public:
	// Used as well by llimmgr.cpp
	static std::string		sOfflineMessage;
	static std::string		sOnlyUserMessage;
	static std::string		sMutedMessage;

	typedef std::map<std::string, std::string> strings_map_t;
	static strings_map_t	sMsgStringsMap;
};

#endif  // LL_FLOATERIM_H
