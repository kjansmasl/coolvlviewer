/**
 * @file llimmgr.h
 * @brief Instant Messaging management
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

#ifndef LL_LLIMMGR_H
#define LL_LLIMMGR_H

#include "llfloater.h"
#include "llinstantmessage.h"

class LLFloaterChatterBox;
class LLFloaterIM;
class LLFloaterIMSession;
class LLFriendObserver;
class LLHost;

class LLIMMgr
{
	friend class LLViewerChatterBoxInvitation;

protected:
	LOG_CLASS(LLIMMgr);

public:
	enum EInvitationType
	{
		INVITATION_TYPE_INSTANT_MESSAGE = 0,
		INVITATION_TYPE_VOICE = 1,
		INVITATION_TYPE_IMMEDIATE = 2
	};

	LLIMMgr();
	virtual ~LLIMMgr();

	// Adds a message to a session. The session can be keyed to a session or
	// agent id. If link_name is true, then we insert the name and link to the
	// profile at the start of the message.
	void addMessage(const LLUUID& session_id, const LLUUID& target_id,
					const std::string& from, const std::string& msg,
					const std::string& session_name = LLStringUtil::null,
					EInstantMessage dialog = IM_NOTHING_SPECIAL,
					U32 parent_estate_id = 0,
					const LLUUID& region_id = LLUUID::null,
					const LLVector3& position = LLVector3::zero,
					bool link_name = false);

	void addSystemMessage(const LLUUID& session_id,
						  const std::string& message_name, const LLSD& args);

	// Adds a session to the talk view. The name is the local name of the
	// session, dialog specifies the type of session. Since sessions can be
	// keyed off of first recipient or initiator, the session can be matched
	// against the id provided. If the session exists, it is brought forward.
	// This method accepts a group id or an agent id. Specifying id = NULL
	// results in an IM session to everyone. Returns the uuid of the session.
	LLUUID addSession(const std::string& name, EInstantMessage dialog,
					  const LLUUID& other_participant_id);

	// Adds a session using a specific group of starting agents the dialog type
	// is assumed correct. Returns the uuid of the session.
	LLUUID addSession(const std::string& name, EInstantMessage dialog,
					  const LLUUID& other_participant_id,
					  const uuid_vec_t& ids);

	// Creates a P2P session with the requisite handle for responding to voice
	// calls
	LLUUID addP2PSession(const std::string& name,
						 const LLUUID& other_participant_id,
						 const std::string& voice_session_handle,
						 const std::string& caller_uri = LLStringUtil::null);

	// This leaves the session (by sending a message to the server, unless
	// snooze_duration is not zero), removes the panel referenced by session_id
	// and then restores internal consistency.
	void removeSession(const LLUUID& session_id, const LLUUID& other_part_id,
					   U32 snooze_duration);

	void inviteToSession(const LLUUID& session_id,
						 const std::string& session_name, const LLUUID& caller,
						 const std::string& caller_name, EInstantMessage type,
						 EInvitationType inv_type,
						 const std::string& session_handle = LLStringUtil::null,
						 const std::string& session_uri = LLStringUtil::null);

	// Updates a given session's session IDs. Does not open, create or do
	// anything new. If the old session does not exist, then nothing happens.
	void updateFloaterSessionID(const LLUUID& old_session_id,
								const LLUUID& new_session_id);

	// Rebuild stuff
	void refresh();

	LL_INLINE void clearNewIMNotification()
	{
		mIMsReceived = 0;
		mPrivateIMReceived = false;
	}

	// IM received that you haven't seen yet
	LL_INLINE U32 getIMsReceived() const		{ return mIMsReceived; }
	LL_INLINE bool isPrivateIMReceived() const	{ return mPrivateIMReceived; }

	void setFloaterOpen(bool open);
	bool getFloaterOpen();

	LLFloaterChatterBox* getFloater();

	// This method is used to go through all active sessions and disable all of
	// them. This method is usally called when you are forced to log out or
	// similar situations where you do not have a good connection.
	void disconnectAllSessions();

	static void	toggle(void*);

	// This is a helper function to determine what kind of IM session should be
	// used for the given agent.
	static EInstantMessage defaultIMTypeForAgent(const LLUUID& agent_id);

	static LLUUID computeSessionID(EInstantMessage dialog,
								   const LLUUID& other_participant_id);

	void clearPendingInvitation(const LLUUID& session_id);

	LLSD getPendingAgentListUpdates(const LLUUID& session_id);
	void addPendingAgentListUpdates(const LLUUID& sessioN_id,
									const LLSD& updates);
	void clearPendingAgentListUpdates(const LLUUID& session_id);

	void processNewMessage(const LLUUID& from_id, bool from_group,
						   const LLUUID& to_id, U8 offline,
						   EInstantMessage dialog,
						   const LLUUID& session_id, U32 timestamp,
						   std::string name, std::string message,
						   U32 parent_estate_id,const LLUUID& region_id,
						   const LLVector3& position, U8* binary_bucket,
						   S32 bucket_size, const LLHost& sender,
						   const LLUUID& aux_id = LLUUID::null);

	static void deliverMessage(const std::string& utf8_text,
							   const LLUUID& im_session_id,
							   const LLUUID& other_participant_id,
							   EInstantMessage dialog);

	static bool sendStartSessionMessages(const LLUUID& temp_session_id,
										 const LLUUID& other_participant_id,
										 const uuid_vec_t& ids,
										 EInstantMessage dialog);

	// Used by llappviewer.cpp to request stored IMs on login
	static bool requestOfflineMessages();

private:
	// Creates a panel and update internal representation for consistency.
	// Returns the pointer, caller (the class instance since it is a private
	// method) is not responsible for deleting the pointer.
	LLFloaterIMSession* createFloater(const LLUUID& session_id,
									  const LLUUID& target_id,
									  const std::string& name,
									  EInstantMessage dialog,
									  bool user_initiated = false);

	LLFloaterIMSession* createFloater(const LLUUID& session_id,
									  const LLUUID& target_id,
									  const std::string& name,
									  const uuid_vec_t& ids,
									  EInstantMessage dialog,
									  bool user_initiated = false);

	// This simple method just iterates through all of the ids, and prints a
	// simple message if they are not online. Used to help reduce 'hello'
	// messages to the Linden employees unlucky enough to have their calling
	// card in the default inventory.
	void noteOfflineUsers(LLFloaterIMSession* panel, const uuid_vec_t& ids);
	void noteMutedUsers(LLFloaterIMSession* panel, const uuid_vec_t& ids);

	// Returns true when the session for from_id does exist already.
	bool processIMTypingCore(EInstantMessage dialog, const LLUUID& from_id,
							 const std::string& from_name, bool typing);

	static void onInviteNameLookup(const LLUUID& id,
								   const std::string& full_name,
								   bool is_group, LLSD payload);

	static bool inviteUserResponse(const LLSD& notification,
								   const LLSD& response);

	static void chatterBoxInvitationCoro(const std::string& url,
										 LLUUID session_id,
										 LLIMMgr::EInvitationType type);

	static void startDeprecatedConference(const LLUUID& temp_session_id,
										  const LLUUID& creator_id,
										  const LLUUID& other_participant_id,
										  const LLSD& agents_to_invite);

	static void startConferenceCoro(const std::string& url,
									LLUUID temp_session_id, LLUUID creator_id,
									LLUUID other_participant_id, LLSD agents);

	static bool requestOfflineMessagesLegacy();
	static void requestOfflineMessagesCoro(const std::string& url);

private:
	LLFriendObserver*	mFriendObserver;

	typedef fast_hmap<LLUUID, F32> snoozed_map_t;
	snoozed_map_t		mSnoozedSessions;

	LLSD				mPendingInvitations;
	LLSD				mPendingAgentListUpdates;

	// IMs have been received that you have not seen yet.
	U32					mIMsReceived;
	bool 				mPrivateIMReceived;
};

// Globals
extern LLIMMgr* gIMMgrp;
extern const std::string gIMFloaterName;

#endif  // LL_LLIMMGR_H
