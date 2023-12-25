/**
 * @file llimmgr.cpp
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

#include "llviewerprecompiledheaders.h"

#include "boost/tokenizer.hpp"

#include "llimmgr.h"

#include "llcachename.h"
#include "llcorehttputil.h"
#include "llfloater.h"
#include "llhttpnode.h"
#include "llnotifications.h"
#include "llsdserialize.h"			// For LLSDSerialize::toPrettyXML()
#include "llsdutil_math.h"			// For ll_vector3_from_sd()
#include "lltabcontainer.h"
#include "lltrans.h"
#include "lluistring.h"
#include "llwindow.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFrameTimeSeconds and gDisconnected
#include "llavatartracker.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llfloaterchatterbox.h"
#include "llfloatergroupinfo.h"
#include "llfloaterim.h"
#include "llfloaternewim.h"
#include "llinventorymodel.h"
#include "llmutelist.h"
#include "lloverlaybar.h"
//MK
#include "mkrlinterface.h"
//mk
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewermessage.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llvoicechannel.h"

LLIMMgr* gIMMgrp = NULL;

// This name is used by (and reserved for) the menus: floater_im.xml,
// floater_instant_message.xml, floater_instant_message_group.xml and
// floater_instant_message_ad_hoc.xml. If you change it here, change it
// there ! HB
const std::string gIMFloaterName = "im session";

typedef boost::tokenizer<boost::char_separator<char> > tok_t;
static const boost::char_separator<char> sSeparators("|", "",
													 boost::keep_empty_tokens);

///////////////////////////////////////////////////////////////////////////////
// Friendship offer callback (was formerly in llviewermessage.cpp, but since
// the OfferFriendship and OfferFriendshipNoMessage notifications are initiated
// from here, it makes more sense to keep the corresponding callback here too).
///////////////////////////////////////////////////////////////////////////////

bool accept_friendship_udp(const LLSD& payload)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg)	// Went offline ?
	{
		return false;
	}

	LL_DEBUGS("InstantMessaging") << "Accepting friendship offer via UDP messaging"
								  << LL_ENDL;

	LLAvatarTracker::formFriendship(payload["from_id"]);

	const LLUUID& fid =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	// This will also trigger an onlinenotification if the user is online
	msg->newMessageFast(_PREHASH_AcceptFriendship);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_TransactionBlock);
	msg->addUUIDFast(_PREHASH_TransactionID, payload["session_id"]);
	msg->nextBlockFast(_PREHASH_FolderData);
	msg->addUUIDFast(_PREHASH_FolderID, fid);
	msg->sendReliable(LLHost(payload["sender"].asString()));

	return true;
}

bool decline_friendship_udp(const LLSD& payload)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg)	// Went offline ?
	{
		return false;
	}

	LL_DEBUGS("InstantMessaging") << "Declining friendship offer via UDP messaging"
								  << LL_ENDL;

	// We no longer notify other viewers, but we DO still send the rejection to
	// the simulator to delete the pending userop.
	msg->newMessageFast(_PREHASH_DeclineFriendship);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_TransactionBlock);
	msg->addUUIDFast(_PREHASH_TransactionID, payload["session_id"]);
	msg->sendReliable(LLHost(payload["sender"].asString()));

	return true;
}

void accept_friendship_coro(std::string url, LLSD payload)
{
	LL_DEBUGS("InstantMessaging") << "Accepting friendship offer via capability"
								  << LL_ENDL;

	url += "?from=" + payload["from_id"].asString() + "&agent_name=\"" +
		   LLURI::escape(gAgentAvatarp->getFullname(true)) + "\"";
	LLSD data;
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("AcceptFriendshipOffer");
	LLSD result = adapter.postAndSuspend(url, data);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result.has("success") || !result["success"].asBoolean())
	{
		llwarns << "Error accepting frienship offer via capability. Error: "
				<< status.toString() << llendl;
		if (accept_friendship_udp(payload))
		{
			llinfos << "Sent frienship acceptance via legacy UDP messaging"
					<< llendl;
		}
		else
		{
			llwarns << "Failed to send frienship acceptance via legacy UDP messaging"
					<< llendl;
		}
		return;
	}

	LLAvatarTracker::formFriendship(payload["from_id"]);
}

void decline_friendship_coro(std::string url, LLSD payload)
{
	LL_DEBUGS("InstantMessaging") << "Declining friendship offer via capability"
								  << LL_ENDL;

	url += "?from=" + payload["from_id"].asString();

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("DeclineFriendshipOffer");
	LLSD result = adapter.deleteAndSuspend(url);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result.has("success") || !result["success"].asBoolean())
	{
		llwarns << "Error declining frienship offer via capability. Error: "
				<< status.toString() << llendl;
		if (decline_friendship_udp(payload))
		{
			llinfos << "Sent frienship declining via legacy UDP messaging"
					<< llendl;
		}
		else
		{
			llwarns << "Failed to send frienship declining via legacy UDP messaging"
					<< llendl;
		}
	}
}

bool friendship_offer_callback(const LLSD& notification, const LLSD& response)
{
	const LLSD& payload = notification["payload"];
	bool online = payload.has("online") && payload["online"].asBoolean();

	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)		// Accept
	{
		const std::string& url =
			gAgent.getRegionCapability("AcceptFriendship");
		if (url.empty() || online)
		{
			accept_friendship_udp(payload);
			return false;
		}
		gCoros.launch("acceptFriendshipOffer",
					  boost::bind(&accept_friendship_coro, url, payload));
	}
	else if (option == 1)	// Decline
	{
		const std::string& url =
			gAgent.getRegionCapability("DeclineFriendship");
		if (url.empty() || online)
		{
			decline_friendship_udp(payload);
			return false;
		}

		gCoros.launch("declineFriendshipOffer",
					  boost::bind(&decline_friendship_coro, url, payload));
	}

	return false;
}
static LLNotificationFunctorRegistration friend_offer_cb_reg("OfferFriendship",
															 friendship_offer_callback);
static LLNotificationFunctorRegistration friend_offer_nm_cb_reg("OfferFriendshipNoMessage",
																friendship_offer_callback);

///////////////////////////////////////////////////////////////////////////////
// LLIMMgrFriendObserver class
// Bridge to suport knowing when the friends list has changed.
///////////////////////////////////////////////////////////////////////////////

class LLIMMgrFriendObserver final : public LLFriendObserver
{
public:
	LLIMMgrFriendObserver()
	{
	}

	~LLIMMgrFriendObserver() override
	{
	}

	void changed(U32 mask) override
	{
		if (gIMMgrp &&
			(mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE |
					 LLFriendObserver::ONLINE)) != 0)
		{
			gIMMgrp->refresh();
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
// LLIMMgr class
///////////////////////////////////////////////////////////////////////////////

LLIMMgr::LLIMMgr()
:	mIMsReceived(0),
	mPrivateIMReceived(false)
{
	llassert_always(gIMMgrp == NULL);	// Only one instance allowed

	mFriendObserver = new LLIMMgrFriendObserver();
	gAvatarTracker.addObserver(mFriendObserver);

	// *HACK: use floater to initialize string constants from xml file then
	// delete it right away
	LLFloaterIM* dummy_floater = new LLFloaterIM();
	delete dummy_floater;

	mPendingInvitations = LLSD::emptyMap();
	mPendingAgentListUpdates = LLSD::emptyMap();

	gIMMgrp = this;
}

LLIMMgr::~LLIMMgr()
{
	gAvatarTracker.removeObserver(mFriendObserver);
	delete mFriendObserver;
	gIMMgrp = NULL;
}

// NOTE: the other_participant_id is either an agent_id, a group_id, or an
// inventory folder item_id (collection of calling cards)
//static
LLUUID LLIMMgr::computeSessionID(EInstantMessage dialog,
								 const LLUUID& other_participant_id)
{
	LLUUID session_id;
	if (dialog == IM_SESSION_GROUP_START || dialog == IM_SESSION_INVITE)
	{
		// Slam group session_id to the group_id (other_participant_id)
		// or the provided session id for invites (which includes group
		// session invites).
		session_id = other_participant_id;
	}
	else if (dialog == IM_SESSION_CONFERENCE_START)
	{
		session_id.generate();
	}
	else
	{
		LLUUID agent_id = gAgentID;
		if (other_participant_id == agent_id)
		{
			// If we try to send an IM to ourselves then the XOR would be null
			// so we just make the session_id the same as the agent_id
			session_id = agent_id;
		}
		else
		{
			// Peer-to-peer or peer-to-asset session_id is the XOR
			session_id = other_participant_id ^ agent_id;
		}
	}
	return session_id;
}

//static
void LLIMMgr::chatterBoxInvitationCoro(const std::string& url,
									   LLUUID session_id,
									   LLIMMgr::EInvitationType type)
{
	LLSD data;
	data["method"] = "accept invitation";
	data["session-id"] = session_id;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("ChatterBoxInvitation");
	LLSD result = adapter.postAndSuspend(url, data);

	if (!gIMMgrp) return;	// Viewer is closing down !
	gIMMgrp->clearPendingAgentListUpdates(session_id);
	gIMMgrp->clearPendingInvitation(session_id);

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
	if (!status)
	{
		llwarns << "Failed to start session Id: " << session_id
				<< ". Error: " << status.toString() << llendl;
		if (status == gStatusNotFound)
		{
			floaterp->showSessionStartError("does not exist");
		}
		return;
	}

	// We have accepted our invitation and received a list of agents that were
	// currently in the session when the reply was sent to us. Now, it is
	// possible that there were some agents to slip in/out between when that
	// message was sent to us and now.
	// The agent list updates we have received have been accurate from the time
	// we were added to the session but unfortunately our base that we are
	// receiving here may not be the most up to date. It was accurate at some
	// point in time though.
	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
	floaterp->setSpeakers(result);

	// We now have our base of users in the session that was accurate at some
	// point, but maybe not now so now we apply all of the udpates we have
	// received in case of race conditions
	floaterp->updateSpeakersList(gIMMgrp->getPendingAgentListUpdates(session_id));

	if (type == LLIMMgr::INVITATION_TYPE_VOICE)
	{
		floaterp->requestAutoConnect();
		LLFloaterIMSession::onClickStartCall(floaterp);
		// always open IM window when connecting to voice
		LLFloaterChatterBox::showInstance(true);
	}
	else if (type == LLIMMgr::INVITATION_TYPE_IMMEDIATE)
	{
		LLFloaterChatterBox::showInstance(true);
	}
}

//static
bool LLIMMgr::inviteUserResponse(const LLSD& notification,
								 const LLSD& response)
{
	if (!gIMMgrp) return false;

	const LLSD& payload = notification["payload"];
	LLUUID session_id = payload["session_id"].asUUID();
	EInstantMessage type = (EInstantMessage)payload["type"].asInteger();
	LLIMMgr::EInvitationType inv_type =
		(LLIMMgr::EInvitationType)payload["inv_type"].asInteger();
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0: // Accept
		{
			if (type == IM_SESSION_P2P_INVITE)
			{
				// Create a normal IM session
				session_id = gIMMgrp->addP2PSession(
					payload["session_name"].asString(),
					payload["caller_id"].asUUID(),
					payload["session_handle"].asString(),
					payload["session_uri"].asString());

				LLFloaterIMSession* im_floater =
					LLFloaterIMSession::findInstance(session_id);
				if (im_floater)
				{
					im_floater->requestAutoConnect();
					LLFloaterIMSession::onClickStartCall(im_floater);
					// always open IM window when connecting to voice
					LLFloaterChatterBox::showInstance(session_id);
				}

				gIMMgrp->clearPendingAgentListUpdates(session_id);
				gIMMgrp->clearPendingInvitation(session_id);
			}
			else
			{
				const std::string& url =
					gAgent.getRegionCapability("ChatSessionRequest");
				if (!url.empty())
				{
					gIMMgrp->addSession(payload["session_name"].asString(), type,
									   session_id);
					gCoros.launch("chatterBoxInvitationCoro",
								  boost::bind(&LLIMMgr::chatterBoxInvitationCoro,
											  url, session_id, inv_type));
				}
			}
			break;
		}

		// Mute (also implies ignore, so this falls through to the "ignore" case
		// below)
		case 2:	// Mute
		{
			// Mute the sender of this invite
			if (!LLMuteList::isMuted(payload["caller_id"].asUUID()))
			{
				LLMute mute(payload["caller_id"].asUUID(),
							payload["caller_name"].asString(), LLMute::AGENT);
				LLMuteList::add(mute);
			}
			// FALLTHROUGH to decline
		}

		case 1: // Decline
		{
			if (type == IM_SESSION_P2P_INVITE)
			{
				std::string s = payload["session_handle"].asString();
				gVoiceClient.declineInvite(s);
			}
			else
			{
				const std::string& url =
					gAgent.getRegionCapability("ChatSessionRequest");
				if (!url.empty())
				{
					LLSD data;
					data["method"] = "decline invitation";
					data["session-id"] = session_id;
					LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url,
																		  data,
																		  "Invitation declined.",
																		  "Failed to send decline invitation message.");
				}
			}

			gIMMgrp->clearPendingAgentListUpdates(session_id);
			gIMMgrp->clearPendingInvitation(session_id);
			break;
		}
	}

	return false;
}

// Helper function
void session_starter_helper(const LLUUID& temp_session_id,
							const LLUUID& other_participant_id,
							EInstantMessage im_type)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	msg->nextBlockFast(_PREHASH_MessageBlock);
	msg->addBoolFast(_PREHASH_FromGroup, false);
	msg->addUUIDFast(_PREHASH_ToAgentID, other_participant_id);
	msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
	msg->addU8Fast(_PREHASH_Dialog, im_type);
	msg->addUUIDFast(_PREHASH_ID, temp_session_id);
	// No timestamp necessary
	msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP);

	std::string name;
	gAgent.buildFullname(name);

	msg->addStringFast(_PREHASH_FromAgentName, name);
	msg->addStringFast(_PREHASH_Message, LLStringUtil::null);
	msg->addU32Fast(_PREHASH_ParentEstateID, 0);
	msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());
}

//static
void LLIMMgr::startConferenceCoro(const std::string& url,
								  LLUUID temp_session_id, LLUUID creator_id,
								  LLUUID other_participant_id, LLSD agents)
{
	LLSD data;
	data["method"] = "start conference";
	data["session-id"] = temp_session_id;
	data["params"] = agents;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("StartConference");
	LLSD result = adapter.postAndSuspend(url, data);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		if (status == gStatusBadRequest)
		{
			startDeprecatedConference(temp_session_id, creator_id,
									  other_participant_id, agents);
		}
		else
		{
			// Throw an error back to the client ?
			// In theory we should have just have these error strings set up in
			// this file as opposed to the IMMgr, but the error string were
			// unneeded here previously and it is not worth the effort
			// switching over all the possible different language translations.
			llwarns << "Failed to start conference: " << status.toString()
					<< llendl;
		}
	}
}

// Returns true if any messages were sent, false otherwise. Is sort of
// equivalent to "does the server need to do anything ?"
//static
bool LLIMMgr::sendStartSessionMessages(const LLUUID& temp_session_id,
									   const LLUUID& other_participant_id,
									   const uuid_vec_t& ids,
									   EInstantMessage dialog)
{
	if (dialog == IM_SESSION_GROUP_START)
	{
		session_starter_helper(temp_session_id, other_participant_id, dialog);
		gMessageSystemp->addBinaryDataFast(_PREHASH_BinaryBucket,
										   EMPTY_BINARY_BUCKET,
										   EMPTY_BINARY_BUCKET_SIZE);
		gAgent.sendReliableMessage();

		return true;
	}
	
	if (dialog != IM_SESSION_CONFERENCE_START)
	{
		return false;
	}

	LLSD agents;
	for (S32 i = 0, count = ids.size(); i < count; ++i)
	{
		agents.append(ids[i]);
	}

	// We have a new way of starting conference calls now
	const std::string& url = gAgent.getRegionCapability("ChatSessionRequest");
	if (!url.empty())
	{
		gCoros.launch("startConferenceCoro",
					  boost::bind(&LLIMMgr::startConferenceCoro, url,
								  temp_session_id, gAgentID,
								  other_participant_id, agents));
	}
	else
	{
		startDeprecatedConference(temp_session_id, gAgentID,
								  other_participant_id, agents);
	}

	// We also need to wait for reply from the server in case of ad-hoc
	// chat (we will get a new session id).
	return true;
}

//static
void LLIMMgr::startDeprecatedConference(const LLUUID& temp_session_id,
										const LLUUID& creator_id,
										const LLUUID& other_participant_id,
										const LLSD& agents_to_invite)
{
	// This method is also called on return of coroutines, and the viewer could
	// be closing down when it happens...
	if (!gMessageSystemp)
	{
		return;
	}

	S32 count = agents_to_invite.size();
	if (count == 0)
	{
		return;	// No one to invite...
	}

	S32 bucket_size = UUID_BYTES * count;
	U8* bucket = new U8[bucket_size];
	// *FIX: this could suffer from endian issues
	U8* pos = bucket;

	LLUUID agent_id;
	for (S32 i = 0; i < count; ++i)
	{
		agent_id = agents_to_invite[i].asUUID();
		memcpy(pos, &agent_id, UUID_BYTES);
		pos += UUID_BYTES;
	}

	session_starter_helper(temp_session_id, other_participant_id,
						   IM_SESSION_CONFERENCE_START);

	gMessageSystemp->addBinaryDataFast(_PREHASH_BinaryBucket, bucket,
									   bucket_size);

	gAgent.sendReliableMessage();

	delete[] bucket;
}

// This is a helper function to determine what kind of IM session should be
// used for the given agent.
//static
EInstantMessage LLIMMgr::defaultIMTypeForAgent(const LLUUID& agent_id)
{
	EInstantMessage type = IM_NOTHING_SPECIAL;
	if (LLAvatarTracker::isAgentFriend(agent_id))
	{
		if (gAvatarTracker.isBuddyOnline(agent_id))
		{
			type = IM_SESSION_CONFERENCE_START;
		}
	}
	return type;
}

//static
void LLIMMgr::toggle(void*)
{
	// Hide the button and show the floater or vice versa.
	if (gIMMgrp)
	{
		gIMMgrp->setFloaterOpen(!gIMMgrp->getFloaterOpen());
	}
}

// Helper function
static void get_extended_text_color(const LLUUID& session_id,
									const LLUUID& other_participant_id,
									const std::string& msg,
									LLColor4& color)
{
	if (other_participant_id.notNull() &&
		gSavedSettings.getBool("HighlightOwnNameInIM"))
	{
		for (std::vector<LLGroupData>::iterator i = gAgent.mGroups.begin(),
												end = gAgent.mGroups.end();
			 i != end; ++i)
		{
			if (i->mID == session_id)
			{
				if (LLFloaterChat::isOwnNameInText(msg))
				{
					color = gSavedSettings.getColor4("OwnNameChatColor");
				}
				break;
			}
		}
	}
}

// Add a message to a session.
void LLIMMgr::addMessage(const LLUUID& session_id, const LLUUID& target_id,
						 const std::string& from, const std::string& msg,
						 const std::string& session_name,
						 EInstantMessage dialog,  U32 parent_estate_id,
						 const LLUUID& region_id, const LLVector3& position,
						 bool link_name)
{
	LLUUID other_participant_id = target_id;

	bool private_im = from != SYSTEM_FROM &&
					  !gAgent.isInGroup(session_id, true);

	// Replace interactive system message marker with correct from string value
	std::string from_name = from;
	if (from == INCOMING_IM)
	{
		from_name = SYSTEM_FROM;
	}
	else if (from == INTERACTIVE_SYSTEM_FROM)
	{
		from_name = SYSTEM_FROM;
		private_im = false;
	}

	// Do not process muted IMs
	if (LLMuteList::isMuted(other_participant_id, LLMute::flagTextChat) &&
		!LLMuteList::isLinden(from_name))
	{
		return;
	}
	if (session_id.notNull() &&
		LLMuteList::isMuted(session_id, LLMute::flagTextChat))
	{
		// Muted group
		return;
	}
	size_t i = session_name.find(" Conference");
	if (i != std::string::npos)
	{
		std::string initiator = session_name.substr(0, i);
		if (LLMuteList::isMuted(LLUUID::null, initiator, LLMute::flagTextChat,
								LLMute::AGENT))
		{
			// Conference initiated by a muted agent
			return;
		}
	}

#if 1	// *TODO: check that this is still needed...
	// Not sure why...but if it is from ourselves we set the target_id to be
	// NULL
	if (other_participant_id == gAgentID)
	{
		other_participant_id.setNull();
	}
#endif

	LL_DEBUGS("InstantMessaging") << "IM type: " << dialog
								  << " - session name: " << session_name
								  << " - From: " << from_name << LL_ENDL;

	LLUUID new_session_id = session_id;
	if (new_session_id.isNull())
	{
		// No session ID... Compute a new one
		new_session_id = computeSessionID(dialog, other_participant_id);
	}
	LLFloaterIMSession* floater =
		LLFloaterIMSession::findInstance(new_session_id);
	if (!floater)
	{
		floater = LLFloaterIMSession::findInstance(other_participant_id);
		if (floater)
		{
			llinfos << "Found the IM session " << session_id
					<< " by participant " << other_participant_id << llendl;
		}
	}

	// Create IM window as necessary
	if (!floater)
	{
		LL_DEBUGS("InstantMessaging") << "Creating a new window" << LL_ENDL;

		std::string name = from_name;
		if (!session_name.empty() && session_name.size() > 1)
		{
			name = session_name;
		}
		if (LLAvatarName::sOmitResidentAsLastName)
		{
			name = LLCacheName::cleanFullName(name);
			from_name = LLCacheName::cleanFullName(from_name);
		}

		floater = createFloater(new_session_id, other_participant_id, name,
								dialog, false);

		// When we get a new IM, and if you are a god, display a bit of
		// information about the source. This is to help liaisons when
		// answering questions.
		if (gAgent.isGodlike())
		{
			// *TODO:translate (low priority, god ability)
			std::ostringstream bonus_info;
			bonus_info << "*** parent estate: " << parent_estate_id
					   << (parent_estate_id == 1 ? ", mainland" : "")
					   << (parent_estate_id == 5 ? ", teen" : "");

			// Once we have web-services (or something) which returns
			// information about a region id, we can print this out and even
			// have it link to map-teleport or something.
			// << "*** region_id: " << region_id << std::endl
			// << "*** position: " << position << std::endl;

			floater->addHistoryLine(bonus_info.str(),
									gSavedSettings.getColor4("SystemChatColor"));
		}

		if (private_im ||
			gSavedSettings.getBool("UISndNewIncomingPlayForGroup"))
		{
			make_ui_sound("UISndNewIncomingIMSession");
		}
	}

	// Now add message to floater
	bool is_from_system = target_id.isNull() || from_name == SYSTEM_FROM;
	LLColor4 color;
	if (is_from_system)
	{
		color = gSavedSettings.getColor4("SystemChatColor");
	}
	else
	{
		std::string new_line = std::string(msg);
		if (new_line.find(": ") == 0)
		{
			new_line = new_line.substr(2);
		}
		else
		{
			new_line = new_line.substr(1);
		}

		color = gSavedSettings.getColor("IMChatColor");
		get_extended_text_color(session_id, other_participant_id, new_line,
								color);
	}

	if (!link_name)
	{
		// No name to prepend, so just add the message normally
		floater->addHistoryLine(msg, color);
	}
	else
	{
		// Insert linked name to front of message
		floater->addHistoryLine(msg, color, true, other_participant_id,
								from_name);
	}

	LLFloaterChatterBox* chat_floater =
		LLFloaterChatterBox::getInstance(LLSD());
	if (!chat_floater->getVisible() && !floater->getVisible())
	{
		LL_DEBUGS("InstantMessaging") << "Adding the IM to the non-visible window"
									  << LL_ENDL;

		// If the IM window is not open and the floater is not visible (i.e.
		// not torn off)
		LLFloater* old_active = chat_floater->getActiveFloater();

		// Select the newly added floater (or the floater with the new line
		// added to it). It should be there.
		chat_floater->selectFloater(floater);

		// There was a previously unseen IM, make that old tab flashing it is
		// assumed that the most recently unseen IM tab is the one current
		// selected/active
		if (old_active && mIMsReceived > 0)
		{
			chat_floater->setFloaterFlashing(old_active, true);
		}

		// Notify of a new IM (for the overlay bar button)
		// *BUG: in fact, this counts the number of sessions that received new,
		// unread IMs, and not the number of unread IMs... The floater code
		// above is apparently changing the getVisible() flags even though the
		// corresponding windows are not visible...
		++mIMsReceived;
		if (private_im)
		{
			mPrivateIMReceived = true;
		}
		if (gOverlayBarp)
		{
			gOverlayBarp->setDirty();
		}
		LL_DEBUGS("InstantMessaging") << "Unread IMs: " << mIMsReceived
									  << LL_ENDL;
	}
}

void LLIMMgr::addSystemMessage(const LLUUID& session_id,
							   const std::string& message_name,
							   const LLSD& args)
{
	LLUIString message;

	// Null session id means near me (chat history)
	if (session_id.isNull())
	{
		LLFloaterChat* chat_floaterp = LLFloaterChat::getInstance();

		message = chat_floaterp->getString(message_name);
		message.setArgs(args);

		LLChat chat(message);
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		chat_floaterp->addChatHistory(chat);
	}
	else // Going to IM session
	{
		LLFloaterIMSession* im_floaterp =
			LLFloaterIMSession::findInstance(session_id);
		if (im_floaterp)
		{
			message = im_floaterp->getString(message_name);
			message.setArgs(args);
			addMessage(session_id, LLUUID::null, SYSTEM_FROM,
					   message.getString());
		}
	}
}

LLUUID LLIMMgr::addP2PSession(const std::string& name,
							  const LLUUID& other_participant_id,
							  const std::string& voice_session_handle,
							  const std::string& caller_uri)
{
	LLUUID session_id = addSession(name, IM_NOTHING_SPECIAL,
								   other_participant_id);

	LLFloaterIMSession* floater = LLFloaterIMSession::findInstance(session_id);
	if (floater)
	{
		LLVoiceChannelP2P* chanp =
			(LLVoiceChannelP2P*)floater->getVoiceChannel();
		if (chanp)
		{
			chanp->setSessionHandle(voice_session_handle, caller_uri);
		}
		else
		{
			llwarns << "NULL voice channel for session: " << session_id
					<< llendl;
		}
	}

	return session_id;
}

// This adds a session to the talk view. The name is the local name of the
// session, dialog specifies the type of session. If the session exists, it is
// brought forward. Specifying id = NULL results in an IM session to everyone.
// Returns the UUID of the session.
LLUUID LLIMMgr::addSession(const std::string& name, EInstantMessage dialog,
						   const LLUUID& other_participant_id)
{
	LLUUID session_id = computeSessionID(dialog, other_participant_id);

	LLFloaterIMSession* floater = LLFloaterIMSession::findInstance(session_id);
	if (!floater)
	{
		uuid_vec_t ids;
		ids.emplace_back(other_participant_id);
//MK
		if (gRLenabled && !gRLInterface.canStartIM(other_participant_id))
		{
			return LLUUID::null;
		}
//mk
		floater = createFloater(session_id, other_participant_id, name, ids,
								dialog, true);

		noteOfflineUsers(floater, ids);
		LLFloaterChatterBox::showInstance(session_id);

		// Only warn for regular IMs - not group IMs
		if (dialog == IM_NOTHING_SPECIAL)
		{
			noteMutedUsers(floater, ids);
		}
		else
		{
			snoozed_map_t::iterator it = mSnoozedSessions.find(session_id);
			if (it != mSnoozedSessions.end())
			{
				LL_DEBUGS("InstantMessaging") << "Removing session Id "
											  << session_id
											  << " from snoozes map."
											  << LL_ENDL;
				mSnoozedSessions.erase(it);
			}
		}
		LLFloaterChatterBox::getInstance(LLSD())->showFloater(floater);
	}
	else
	{
		floater->open();
	}

	floater->setInputFocus(true);

	return floater->getSessionID();
}

// Adds a session using the given session_id. If the session already exists the
// dialog type is assumed correct. Returns the uuid of the session.
LLUUID LLIMMgr::addSession(const std::string& name, EInstantMessage dialog,
						   const LLUUID& other_participant_id,
						   const uuid_vec_t& ids)
{
	if (ids.size() == 0)
	{
		return LLUUID::null;
	}

	LLUUID session_id = computeSessionID(dialog, other_participant_id);

	LLFloaterIMSession* floater = LLFloaterIMSession::findInstance(session_id);
	if (!floater)
	{
		// On creation, use the first element of Ids as the
		// "other_participant_id"
		floater = createFloater(session_id, other_participant_id,
								name, ids, dialog, true);

		if (!floater) return LLUUID::null;

		noteOfflineUsers(floater, ids);
		LLFloaterChatterBox::showInstance(session_id);

		// Only warn for regular IMs, not group IMs
		if (dialog == IM_NOTHING_SPECIAL)
		{
			noteMutedUsers(floater, ids);
		}
	}
	else
	{
		floater->open();
	}

	floater->setInputFocus(true);

	return floater->getSessionID();
}

void LLIMMgr::removeSession(const LLUUID& session_id,
							const LLUUID& other_participant_id,
							U32 snooze_duration)
{
	if (session_id.notNull())
	{
		if (snooze_duration)
		{
			F32 unsnooze_after = gFrameTimeSeconds +
								 (F32)snooze_duration * 60.f;
			LL_DEBUGS("InstantMessaging") << "Snoozing session Id: "
										  << session_id << LL_ENDL;
			mSnoozedSessions.emplace(session_id, unsnooze_after);
		}
		else	// Close the session server-side
		{
			std::string name;
			gAgent.buildFullname(name);
			pack_instant_message(gAgentID, false, gAgentSessionID,
								 other_participant_id, name,
								 LLStringUtil::null, IM_ONLINE,
								 IM_SESSION_LEAVE, session_id);
			gAgent.sendReliableMessage();
		}
	}

	LLFloaterIMSession* floater = LLFloaterIMSession::findInstance(session_id);
	if (floater)
	{
		LLFloaterChatterBox::getInstance(LLSD())->removeFloater(floater);
		clearPendingInvitation(session_id);
		clearPendingAgentListUpdates(session_id);
	}
}

void LLIMMgr::inviteToSession(const LLUUID& session_id,
							  const std::string& session_name,
							  const LLUUID& caller_id,
							  const std::string& caller_name,
							  EInstantMessage type,
							  EInvitationType inv_type,
							  const std::string& session_handle,
							  const std::string& session_uri)
{
	// Ignore invites from muted residents
	bool is_linden = LLMuteList::isLinden(caller_name);
	if (!is_linden && LLMuteList::isMuted(caller_id))
	{
		llinfos << "Ignoring session invite from fully muted resident: "
				<< caller_name << llendl;
		return;
	}

	std::string notify_box_type;
	bool ad_hoc_invite = false;
	bool voice_invite = false;
	if (type == IM_SESSION_P2P_INVITE)
	{
		// P2P is different... they only have voice invitations
		notify_box_type = "VoiceInviteP2P";
		voice_invite = true;
	}
	else if (gAgent.isInGroup(session_id, true))
	{
		// Only really old school groups have voice invitations
		notify_box_type = "VoiceInviteGroup";
		voice_invite = true;
	}
	else if (inv_type == INVITATION_TYPE_VOICE)
	{
		// Else it is an ad-hoc and a voice ad-hoc
		notify_box_type = "VoiceInviteAdHoc";
		ad_hoc_invite = true;
		voice_invite = true;
	}
	else if (inv_type == INVITATION_TYPE_IMMEDIATE)
	{
		notify_box_type = "InviteAdHoc";
		ad_hoc_invite = true;
	}

	if (voice_invite && LLMuteList::isMuted(caller_id, LLMute::flagVoiceChat))
	{
		llinfos << "Ignoring voice session invite from voice-muted resident: "
				<< caller_name << llendl;
		return;
	}

	LLSD payload;
	payload["session_id"] = session_id;
	payload["session_name"] = session_name;
	payload["caller_id"] = caller_id;
	payload["caller_name"] = caller_name;
	payload["type"] = type;
	payload["inv_type"] = inv_type;
	payload["session_handle"] = session_handle;
	payload["session_uri"] = session_uri;
	payload["notify_box_type"] = notify_box_type;

	LLVoiceChannel* channelp = LLVoiceChannel::getChannelByID(session_id);
	if (channelp && channelp->callStarted())
	{
		// You have already started a call to the other user, so just accept
		// the invite
		gNotifications.forceResponse(LLNotification::Params("VoiceInviteP2P").payload(payload),
									 0);
		return;
	}

	if (type == IM_SESSION_P2P_INVITE || ad_hoc_invite)
	{
		// Is the inviter a friend ?
		if (gAvatarTracker.getBuddyInfo(caller_id) == NULL)
		{
			// if not, and we are ignoring voice invites from non-friends
			// then silently decline
			if (gSavedSettings.getBool("VoiceCallsFriendsOnly"))
			{
				// Invite is not from a friend, so decline
				gNotifications.forceResponse(LLNotification::Params("VoiceInviteP2P").payload(payload),
											 1);
				return;
			}
		}
	}

	if (!mPendingInvitations.has(session_id.asString()))
	{
		if (caller_name.empty())
		{
			if (gCacheNamep)
			{
				gCacheNamep->get(caller_id, false,
								 boost::bind(&LLIMMgr::onInviteNameLookup,
											 _1, _2, _3, payload));
			}
		}
		else
		{
			LLSD args;
			args["NAME"] = caller_name;
			args["GROUP"] = session_name;
			gNotifications.add(notify_box_type, args, payload,
							   &inviteUserResponse);
		}
		mPendingInvitations[session_id.asString()] = LLSD();
	}
}

//static
void LLIMMgr::onInviteNameLookup(const LLUUID& id,
								 const std::string& full_name,
								 bool is_group, LLSD payload)
{
	std::string name = full_name;
	if (LLAvatarName::sOmitResidentAsLastName)
	{
		name = LLCacheName::cleanFullName(name);
	}
	payload["caller_name"] = name;
	payload["session_name"] = name;

	LLSD args;
	args["NAME"] = name;

	gNotifications.add(payload["notify_box_type"].asString(), args, payload,
					   &inviteUserResponse);
}

void LLIMMgr::refresh()
{
	static const EInstantMessage group_session = IM_SESSION_GROUP_START;
	static const EInstantMessage default_session = IM_NOTHING_SPECIAL;

	LLFloaterNewIM* floaterimp =
		LLFloaterChatterBox::getInstance(LLSD())->getFloaterNewIM();
	if (!floaterimp) return;

	S32 old_group_scroll_pos = floaterimp->getGroupScrollPos();
	S32 old_agent_scroll_pos = floaterimp->getAgentScrollPos();
	floaterimp->clearAllTargets();

	// Add groups
	for (S32 i = 0, count = gAgent.mGroups.size(); i < count; ++i)
	{
		LLGroupData* group = &(gAgent.mGroups[i]);
		floaterimp->addGroup(group->mID, (void*)(&group_session));
	}

	// Build a set of buddies in the current buddy list.
	LLCollectAllBuddies collector;
	gAvatarTracker.applyFunctor(collector);
	LLCollectAllBuddies::buddy_map_t::iterator it;
	LLCollectAllBuddies::buddy_map_t::iterator end;
	it = collector.mOnline.begin();
	end = collector.mOnline.end();
	for ( ; it != end; ++it)
	{
		floaterimp->addAgent(it->second, (void*)(&default_session), true);
	}
	it = collector.mOffline.begin();
	end = collector.mOffline.end();
	for ( ; it != end; ++it)
	{
		floaterimp->addAgent(it->second, (void*)(&default_session), false);
	}

	floaterimp->setGroupScrollPos(old_group_scroll_pos);
	floaterimp->setAgentScrollPos(old_agent_scroll_pos);
}

void LLIMMgr::setFloaterOpen(bool set_open)
{
	if (set_open)
	{
		LLFloaterChatterBox::showInstance();

		LLFloaterChatterBox* floater_chatterbox = getFloater();
		LLFloater* floater_current = floater_chatterbox->getActiveFloater();
		LLFloater* floater_new_im = floater_chatterbox->getFloaterNewIM();
		bool active_is_im = floater_current &&
							(floater_current->getName() == gIMFloaterName ||
							 floater_current == floater_new_im);
		LLFloater* floater_to_show = active_is_im ? floater_current : NULL;
		LLTabContainer* tabs =
			floater_chatterbox->getChild<LLTabContainer>("Preview Tabs");

		for (S32 i = 0; i < floater_chatterbox->getFloaterCount(); ++i)
		{
			LLPanel* panelp = tabs->getPanelByIndex(i);
			if (panelp->getName() == gIMFloaterName)
			{
				// This cast is safe here because in such tabs, only an
				// LLFloaterIMSessions can be called gIMFloaterName.
				LLFloaterIMSession* im_floaterp = (LLFloaterIMSession*)panelp;
				if (im_floaterp &&
					(!floater_to_show ||
					 floater_chatterbox->isFloaterFlashing(im_floaterp)))
				{
					 // The first im_floater or the flashing im_floater
					floater_to_show = im_floaterp;
				}
			}
		}

		if (floater_to_show)
		{
			floater_to_show->open();
		}
		else if (floater_chatterbox && floater_chatterbox->getFloaterNewIM())
		{
			floater_chatterbox->getFloaterNewIM()->open();
		}
	}
	else
	{
		LLFloaterChatterBox::hideInstance();
	}
}

bool LLIMMgr::getFloaterOpen()
{
	return LLFloaterChatterBox::instanceVisible(LLSD());
}

LLFloaterChatterBox* LLIMMgr::getFloater()
{
	return LLFloaterChatterBox::getInstance(LLSD());
}

void LLIMMgr::disconnectAllSessions()
{
	LLFloaterIMSession::closeAllInstances();
}

void LLIMMgr::clearPendingInvitation(const LLUUID& session_id)
{
	if (mPendingInvitations.has(session_id.asString()))
	{
		mPendingInvitations.erase(session_id.asString());
	}
}

LLSD LLIMMgr::getPendingAgentListUpdates(const LLUUID& session_id)
{
	if (mPendingAgentListUpdates.has(session_id.asString()))
	{
		return mPendingAgentListUpdates[session_id.asString()];
	}
	return LLSD();
}

void LLIMMgr::addPendingAgentListUpdates(const LLUUID& session_id,
										 const LLSD& updates)
{
	LLSD::map_const_iterator iter;

	if (!mPendingAgentListUpdates.has(session_id.asString()))
	{
		// This is a new agent list update for this session
		mPendingAgentListUpdates[session_id.asString()] = LLSD::emptyMap();
	}

	if (updates.has("agent_updates") && updates["agent_updates"].isMap() &&
		updates.has("updates") && updates["updates"].isMap())
	{
		// New school update
		LLSD update_types = LLSD::emptyArray();
		LLSD::array_iterator array_iter;

		update_types.append("agent_updates");
		update_types.append("updates");

		for (array_iter = update_types.beginArray();
			 array_iter != update_types.endArray();
			 ++array_iter)
		{
			// We only want to include the last update for a given agent
			for (iter = updates[array_iter->asString()].beginMap();
				 iter != updates[array_iter->asString()].endMap();
				 ++iter)
			{
				mPendingAgentListUpdates[session_id.asString()][array_iter->asString()][iter->first] =
					iter->second;
			}
		}
	}
	else if (updates.has("updates") && updates["updates"].isMap())
	{
		// Old school update where the SD contained just mappings of
		// agent_id -> "LEAVE"/"ENTER"

		// Only want to keep last update for each agent
		for (iter = updates["updates"].beginMap();
			 iter != updates["updates"].endMap(); ++iter)
		{
			mPendingAgentListUpdates[session_id.asString()]["updates"][iter->first] =
				iter->second;
		}
	}
}

void LLIMMgr::clearPendingAgentListUpdates(const LLUUID& session_id)
{
	if (mPendingAgentListUpdates.has(session_id.asString()))
	{
		mPendingAgentListUpdates.erase(session_id.asString());
	}
}

// Creates a floater and updates internal representation for consistency.
// Returns the pointer, caller (the class instance since it is a private
// method) is not responsible for deleting the pointer. Add the floater to
// this but do not select it.
LLFloaterIMSession* LLIMMgr::createFloater(const LLUUID& session_id,
										   const LLUUID& other_participant_id,
										   const std::string& session_label,
										   EInstantMessage dialog,
										   bool user_initiated)
{
	if (session_id.isNull())
	{
		llwarns << "Creating floater with null session Id" << llendl;
	}

	llinfos << "Created from " << other_participant_id << " in session "
			 << session_id << llendl;
	LLFloaterIMSession* floater = new LLFloaterIMSession(session_label,
														 session_id,
														 other_participant_id,
														 dialog);
	LLTabContainer::eInsertionPoint i_pt = user_initiated ?
										   LLTabContainer::RIGHT_OF_CURRENT :
										   LLTabContainer::END;
	LLFloaterChatterBox::getInstance(LLSD())->addFloater(floater, false, i_pt);
	return floater;
}

LLFloaterIMSession* LLIMMgr::createFloater(const LLUUID& session_id,
										   const LLUUID& other_participant_id,
										   const std::string& session_label,
										   const uuid_vec_t& ids,
										   EInstantMessage dialog,
										   bool user_initiated)
{
	if (session_id.isNull())
	{
		llwarns << "Creating with null session Id !" << llendl;
	}
	llinfos << "Creating floater for " << other_participant_id
			<< " in session " << session_id << llendl;

	LLFloaterIMSession* floater = new LLFloaterIMSession(session_label,
														 session_id,
														 other_participant_id,
														 ids, dialog);
	LLTabContainer::eInsertionPoint i_pt = user_initiated ?
										   LLTabContainer::RIGHT_OF_CURRENT :
										   LLTabContainer::END;
	LLFloaterChatterBox::getInstance(LLSD())->addFloater(floater, false, i_pt);
	return floater;
}

void LLIMMgr::noteOfflineUsers(LLFloaterIMSession* floater,
							   const uuid_vec_t& ids)
{
	S32 count = ids.size();
	if (!count)
	{
		floater->addHistoryLine(LLFloaterIM::sOnlyUserMessage,
								gSavedSettings.getColor4("SystemChatColor"));
		return;
	}

	const LLRelationship* info = NULL;
	LLAvatarTracker& at = gAvatarTracker;
	LLColor4 color = gSavedSettings.getColor4("SystemChatColor");
	for (S32 i = 0; i < count; ++i)
	{
		info = at.getBuddyInfo(ids[i]);
		std::string first, last;
		if (info && !info->isOnline() && gCacheNamep &&
			gCacheNamep->getName(ids[i], first, last))
		{
			LLUIString offline = LLFloaterIM::sOfflineMessage;
			offline.setArg("[FIRST]", first);
			offline.setArg("[LAST]", last);
			floater->addHistoryLine(offline, color);
		}
	}
}

void LLIMMgr::noteMutedUsers(LLFloaterIMSession* floater,
							 const uuid_vec_t& ids)
{
	S32 count = ids.size();
	if (count > 0)
	{
		for (S32 i = 0; i < count; ++i)
		{
			if (LLMuteList::isMuted(ids[i]))
			{
				LLUIString muted = LLFloaterIM::sMutedMessage;
				floater->addHistoryLine(muted);
				break;
			}
		}
	}
}

void LLIMMgr::processNewMessage(const LLUUID& from_id, bool from_group,
								const LLUUID& to_id, U8 offline,
								EInstantMessage dialog,
								const LLUUID& session_id, U32 timestamp,
								std::string name, std::string message,
								U32 parent_estate_id,const LLUUID& region_id,
								const LLVector3& position, U8* binary_bucket,
								S32 bucket_size, const LLHost& sender,
								const LLUUID& aux_id)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp)
	{
		return; // Viewer is likely disconnected or closing down !
	}

	// Make sure that we do not have an empty or all-whitespace name
	LLStringUtil::trim(name);
	if (name.empty())
	{
		name = LLTrans::getString("Unnamed");
	}

	bool is_busy = gAgent.getBusy();
	bool is_away = gAgent.getAFK() &&
				   gSavedPerAccountSettings.getBool("BusyResponseWhenAway");
	bool auto_reply = gAgent.getAutoReply();

	bool is_muted = LLMuteList::isMuted(from_id, name, LLMute::flagTextChat);
	bool is_linden = LLMuteList::isLinden(name);

	bool is_owned_by_me = false;
	// session_id is probably the wrong thing...
	LLViewerObject* source = gObjectList.findObject(session_id);
	if (source)
	{
		is_owned_by_me = source->permYouOwner();
	}

	LLChat chat;
	chat.mMuted = is_muted && !is_linden;
	chat.mFromID = from_id;
	chat.mFromName = name;
	if (from_id.isNull() || name == SYSTEM_FROM)
	{
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
	}
	else if (dialog == IM_FROM_TASK || dialog == IM_FROM_TASK_AS_ALERT)
	{
		chat.mSourceType = CHAT_SOURCE_OBJECT;
		// Keep track of the owner's Id for the source object.
		if (source && source->mOwnerID.isNull())
		{
			source->mOwnerID = from_id;
		}
	}
	else
	{
		chat.mSourceType = CHAT_SOURCE_AGENT;
	}

	std::string separator_string(": ");
	size_t message_offset = 0;

	// Handle IRC styled /me messages.
	std::string prefix = message.substr(0, 4);
	if (prefix == "/me " || prefix == "/me'")
	{
		separator_string = "";
		message_offset = 3;
	}

	LL_DEBUGS("InstantMessaging") << "IM type: " << dialog << " from: "
								  << (is_owned_by_me ?
									  "agent-owned object" :
									  (source ?
									   "other resident object" :
									   (from_group ? "group" : "resident")))
								  << LL_ENDL;

	std::string buffer;
	LLSD args;
	switch (dialog)
	{
		case IM_CONSOLE_AND_CHAT_HISTORY:
		{
			// These are used for system messages, hence do not need the name,
			// as it is always "Second Life". *TODO: translate
//MK
			if (gRLenabled)
			{
				if (gRLInterface.mContainsShowloc)
				{
					// Hide every occurrence of the Region and Parcel names if
					// the location restriction is active
					message = gRLInterface.getCensoredLocation(message);
				}

				if (gRLInterface.mContainsShownames ||
					gRLInterface.mContainsShownametags)
				{
					// Censor object IMs but not avatar IMs
					message = gRLInterface.getCensoredMessage(message);
				}
			}
//mk
			args["MESSAGE"] = message;

			// Note: don't put the message in the IM history, even though was
			// sent via the IM mechanism.
			gNotifications.add("SystemMessageTip", args);
			break;
		}

		case IM_NOTHING_SPECIAL:
		{
			// Do not show dialog, just do IM
			if (to_id.isNull() && !gAgent.isGodlike() && regionp &&
				regionp->isPrelude())
			{
				// Do not distract newbies in Prelude with global IMs
				break;
			}
//MK
			else if (gRLenabled && !is_muted &&
					 (message == "@version" || message == "@getblacklist" ||
					  message == "@list" || message == "@stopim"))
			{
				bool close_session = false;
				std::string my_name;
				gAgent.buildFullname(my_name);
				std::string response;
				if (message == "@version")
				{
					// Return the version message
					response = gRLInterface.getVersion();
				}
				else if (message == "@getblacklist")
				{
					// Return the list of the blacklisted RLV commands
					response = RLInterface::sBlackList;
				}
				else if (message == "@list")
				{
					// Return the list of the RLV restrictions in force
					response = gRLInterface.getRlvRestrictions();
				}
				else if (gRLInterface.canStartIM(from_id))
				{
					response =
						"*** The other party is not under @startim restriction.";
				}
				// @stopim
				else
				{
					close_session = true;
					response =
						"*** Session has been ended for the other party.";
				}

				// The message may be very long, so we might need to chop it
				// into chunks of 1023 characters and send several IMs in a row
				// or else it will be truncated by the server.
				std::string chunk;
				while (response.length())
				{
					if (response.length() > 1023)
					{
						chunk = response.substr(0, 1023);
						// Try to break out at the end of a text line, if
						// possible...
						size_t i = chunk.rfind('\n');
						if (i > 1)
						{
							chunk = chunk.substr(0, i);
						}
						else
						{
							i = 1023;
						}
						response = response.substr(i);
					}
					else
					{
						chunk = response;
						response.clear();
					}
					pack_instant_message(gAgentID, false, gAgentSessionID,
										 from_id, my_name.c_str(),
										 chunk.c_str(), IM_ONLINE,
										 IM_BUSY_AUTO_RESPONSE, session_id);
					gAgent.sendReliableMessage();
				}

				if (close_session)
				{
					LLFloaterIMSession* floater;
					floater = LLFloaterIMSession::findInstance(session_id);
					if (floater)
					{
						LLChat chat("*** IM session with " + name +
									" has been ended remotely.");
						LLFloaterChat::addChat(chat, true, false);
						floater->close();
					}
				}

				// Remove the "XXX is typing..." label from the IM window
				processIMTypingCore(dialog, from_id, name, false);
			}
//mk
			else if ((is_busy || is_away || auto_reply) &&
//MK
					(!gRLenabled ||
					 // Agent is not forbidden to receive IMs or the sender is
					 // an exception => send Busy response
					 gRLInterface.canReceiveIM(from_id)) &&
//mk
					 offline == IM_ONLINE && !is_linden && name != SYSTEM_FROM)
			{
				// Return a standard "busy" message, but only do it to online
				// IM (i.e. not other auto responses and not store-and-forward
				// IM)
				if (!LLFloaterIMSession::findInstance(session_id))
				{
					// There is no panel for this conversation (i.e. it is a
					// new IM conversation initiated by the other party)
					std::string my_name;
					gAgent.buildFullname(my_name);
					std::string response;
					if (is_away)
					{
						response = "Away mode auto-response: ";
					}
					else if (is_busy)
					{
						response = "Busy mode auto-response: ";
					}
					else
					{
						response = "Auto-response: ";
					}
					response +=
						gSavedPerAccountSettings.getText("BusyModeResponse");
					pack_instant_message(gAgentID, false, gAgentSessionID,
										 from_id, my_name, response, IM_ONLINE,
										 IM_BUSY_AUTO_RESPONSE, session_id);
					gAgent.sendReliableMessage();
				}

				// Now store incoming IM in chat history

				buffer = separator_string + message.substr(message_offset);

				llinfos << "IM_NOTHING_SPECIAL session_id(" << session_id
						<< "), from_id(" << from_id << ")" << llendl;

				// Add to IM panel, but do not bother the user
				addMessage(session_id, from_id, name, buffer,
						   LLStringUtil::null, dialog, parent_estate_id,
						   region_id, position, true);

				// Pretend this is chat generated by self, so it does not show
				// up on screen
				chat.mText = "IM: " + name + separator_string +
							 message.substr(message_offset);
				LLFloaterChat::addChat(chat, true, true);
				if (gAutomationp)
				{
					gAutomationp->onInstantMsg(session_id, from_id, name,
											   chat.mText);
				}
			}
			else if (from_id.isNull())
			{
				// Messages from "Second Life" ID don't go to IM history
				// messages which should be routed to IM window come from a
				// user ID with name = SYSTEM_NAME:
				chat.mText = name + ": " + message;
//MK
				if (gRLenabled)
				{
					if (gRLInterface.mContainsShowloc)
					{
						// Hide every occurrence of the Region and Parcel names if
						// the location restriction is active
						chat.mText = gRLInterface.getCensoredLocation(chat.mText);
					}

					if (gRLInterface.mContainsShownames)
					{
						// censor that message
						chat.mText = gRLInterface.getCensoredMessage(chat.mText);
					}
				}
//mk
				LLFloaterChat::addChat(chat, false, false);
				if (gAutomationp)
				{
					gAutomationp->onReceivedChat(chat.mChatType, from_id, name,
												 chat.mText);
				}
			}
			else if (to_id.isNull())
			{
				// Message to everyone from GOD
				args["NAME"] = name;
				args["MESSAGE"] = message;
				gNotifications.add("GodMessage", args);

				// Treat like a system message and put in chat history. Claim
				// to be from a local agent so it doesn't go into console.
				chat.mText = name + separator_string +
							 message.substr(message_offset);
				bool local_agent = true;
				LLFloaterChat::addChat(chat, false, local_agent);
				if (gAutomationp)
				{
					gAutomationp->onReceivedChat(chat.mChatType, from_id, name,
												 chat.mText);
				}
			}
			else
			{
				// Standard message, not from system
				std::string saved;
				if (offline == IM_OFFLINE)
				{
					saved = llformat("(Saved %s) ",
									 formatted_time(timestamp).c_str());
				}
				buffer = separator_string + saved +
						 message.substr(message_offset);

//MK
				bool forbid = gRLenabled &&
							  !gRLInterface.canReceiveIM(from_id);
				if (forbid)
				{
					// Agent is forbidden to receive IMs and the sender is no
					// exception
					buffer = separator_string + saved +
							 "*** IM blocked by your viewer";

					// Tell the sender the avatar could not read them
					std::string my_name;
					gAgent.buildFullname(my_name);
					my_name += " using viewer " +
							   gRLInterface.getVersion();
					std::string response = RLInterface::sRecvimMessage;
					pack_instant_message(gAgentID, false, gAgentSessionID,
										 from_id, my_name.c_str(),
										 response.c_str(), IM_ONLINE,
										 IM_BUSY_AUTO_RESPONSE, session_id);
					gAgent.sendReliableMessage();
				}
//mk

				llinfos << "IM_NOTHING_SPECIAL session_id(" << session_id
						<< "), from_id(" << from_id << ")" << llendl;

				if (!is_muted || is_linden)
				{
					addMessage(session_id, from_id, name, buffer,
							   LLStringUtil::null, dialog, parent_estate_id,
							   region_id, position, true);
					if (gAutomationp)
					{
						gAutomationp->onInstantMsg(session_id, from_id, name,
												   buffer);
					}
//MK
				 	// When agent is not forbidden to receive IMs or the sender
					// is an exception, duplicate in chat box.
					if (!forbid)
//mk
					{
						chat.mText = "IM: " + name + separator_string + saved +
									 message.substr(message_offset);
						bool local_agent = false;
						LLFloaterChat::addChat(chat, true, local_agent);
					}
				}
				else
				{
					// Muted user, so do not start an IM session, just record
					// line in chat history. Pretend the chat is from a local
					// agent, so it will go into the history but not be shown
					// on screen.
					chat.mText = buffer;
					bool local_agent = true;
					LLFloaterChat::addChat(chat, true, local_agent);
				}
			}
			break;
		}

		case IM_TYPING_START:
		case IM_TYPING_STOP:
		{
			bool typing_start = dialog == IM_TYPING_START;
			bool ok = processIMTypingCore(dialog, from_id, name, typing_start);
			if (!ok && typing_start && (!is_muted || is_linden) &&
				// Do not announce when busy/away/auto-replying
				!is_busy && !is_away && !auto_reply &&
				gSavedSettings.getBool("IMOpenSessionOnIncoming") &&
//MK
				(!gRLenabled || gRLInterface.canReceiveIM(from_id)))
//mk
			{
				addMessage(computeSessionID(dialog, from_id), from_id,
						   INCOMING_IM, LLTrans::getString("im_incoming"),
						   name, IM_NOTHING_SPECIAL, parent_estate_id,
						   region_id, position, false);
			}
			break;
		}

		case IM_MESSAGEBOX:
		{
			// This is a block, modeless dialog.
			//*TODO:translate
//MK
			if (gRLenabled)
			{
				if (gRLInterface.mContainsShowloc)
				{
					// Hide every occurrence of the Region and Parcel names if
					// thelocation restriction is active
					message = gRLInterface.getCensoredLocation(message);
				}

				if (gRLInterface.mContainsShownames ||
					gRLInterface.mContainsShownametags)
				{
					message = gRLInterface.getCensoredMessage(message);
				}
			}
//mk
			args["MESSAGE"] = message;
			gNotifications.add("SystemMessage", args);
			break;
		}

		case IM_GROUP_NOTICE:
		case IM_GROUP_NOTICE_REQUESTED:
		{
			llinfos << "Received IM_GROUP_NOTICE message." << llendl;

			U8 has_inventory = 0;
			U8 asset_type = 0;
			LLUUID group_id;
			std::string item_name;

			if (aux_id.notNull())
			{
				// aux_id contains group id, binary bucket contains name and
				// asset type
				from_group = true;
				group_id = aux_id;
				has_inventory = bucket_size > 1 ? 1 : 0;
				if (has_inventory)
				{
					std::string str_bucket =
						ll_safe_string((char*)binary_bucket, bucket_size);
					tok_t tokens(str_bucket, sSeparators);
					tok_t::iterator iter = tokens.begin();
					if (iter != tokens.end())
					{
						asset_type =
							(LLAssetType::EType)(atoi((iter++)->c_str()));
						if (++iter != tokens.end())
						{
							item_name.assign(iter->c_str());
						}
					}
				}
			}
			else
			{
				// Read the binary bucket for more information.
				struct notice_bucket_header_t
				{
					U8 has_inventory;
					U8 asset_type;
					LLUUID group_id;
				};

				struct notice_bucket_full_t
				{
					struct notice_bucket_header_t header;
					U8 item_name[DB_INV_ITEM_NAME_BUF_SIZE];
				}* notice_bin_bucket;

				// Make sure the binary bucket is big enough to hold the header
				// and a nul terminated item name.
				if (bucket_size <
						(S32)(sizeof(notice_bucket_header_t) + sizeof(U8)) ||
					binary_bucket[bucket_size - 1] != '\0')
				{
					llwarns << "Malformed group notice binary bucket"
							<< llendl;
					break;
				}

				notice_bin_bucket =
					(struct notice_bucket_full_t*)&binary_bucket[0];
				has_inventory = notice_bin_bucket->header.has_inventory;
				asset_type = notice_bin_bucket->header.asset_type;
				group_id = notice_bin_bucket->header.group_id;
				item_name =
					ll_safe_string((const char*)notice_bin_bucket->item_name);
			}

			// If there is inventory, give the user the inventory offer.
			LLOfferInfo* info = NULL;
			is_muted = LLMuteList::isMuted(LLUUID::null, name, 0,
										   LLMute::AGENT);
			if (has_inventory && !is_muted)
			{
				info = new LLOfferInfo;
				info->mIM = IM_GROUP_NOTICE;
				info->mFromID = from_id;
				info->mFromObject = false;
				info->mFromGroup = from_group;
				info->mTransactionID = session_id;
				info->mType = (LLAssetType::EType)asset_type;
				info->mFolderID =
					gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(info->mType));
				info->mFromName = "A group member named " + name;
				info->mDesc = item_name;
				info->mHost = sender;

				// For requested notices, we do not want a chat decline message
				// logged (it would appear each time you select another group
				// notice).
				if (dialog == IM_GROUP_NOTICE_REQUESTED || is_muted)
				{
					info->mLogInChat = false;
				}
			}

			std::string str = message;

			// Tokenize the string. *TODO: Support escaped tokens ("||" -> "|")
			tok_t tokens(str, sSeparators);
			tok_t::iterator iter = tokens.begin();

			std::string subj(*iter++);
			std::string mes(*iter++);

			// Send the notification down the new path.
			// For requested notices, we do not want to send the popups.
			if (dialog != IM_GROUP_NOTICE_REQUESTED && !is_muted)
			{
				LLSD payload;
				payload["subject"] = subj;
				payload["message"] = mes;
				payload["sender_name"] = name;
				payload["group_id"] = group_id;
				payload["inventory_name"] = item_name;
				payload["inventory_offer"] = info ? info->asLLSD() : LLSD();

				LLSD args;
				args["SUBJECT"] = subj;
				args["MESSAGE"] = mes;
				gNotifications.add(LLNotification::Params("GroupNotice").substitutions(args).payload(payload).timestamp(timestamp));
			}

			// Also send down the old path for now.
			if (IM_GROUP_NOTICE_REQUESTED == dialog)
			{
				LLFloaterGroupInfo::showNotice(subj, mes, group_id,
											   has_inventory, item_name, info);
			}
			break;
		}

		case IM_GROUP_INVITATION:
		{
			//if (!is_linden && (is_busy || is_muted))
			if (is_busy || is_muted)
			{
				busy_message(from_id);
			}
			else
			{
				llinfos << "Received IM_GROUP_INVITATION message." << llendl;
//MK
				if (gRLenabled && gRLInterface.contains("setgroup"))
				{
					llinfos << "Invitation ignored due to RestrainedLove restrictions."
							<< llendl;
					break;
				}
//mk
				// Read the binary bucket for more information.
				struct invite_bucket_t
				{
					S32 membership_fee;
					LLUUID role_id;
				}* invite_bucket;

				// Make sure the binary bucket is the correct size.
				if (bucket_size != sizeof(invite_bucket_t))
				{
					llwarns << "Malformed group invite binary bucket"
							<< llendl;
					break;
				}

				invite_bucket = (struct invite_bucket_t*)&binary_bucket[0];
				S32 membership_fee = ntohl(invite_bucket->membership_fee);

				LLSD payload;
				payload["transaction_id"] = session_id;
				payload["group_id"] = from_group ? from_id : aux_id;
				payload["name"] = name;
				payload["message"] = message;
				payload["fee"] = membership_fee;
				payload["use_offline_cap"] = session_id.isNull() &&
											 offline == IM_OFFLINE;

				LLSD args;
				args["MESSAGE"] = message;
				gNotifications.add("JoinGroup", args, payload);
			}
			break;
		}

		case IM_INVENTORY_OFFERED:
		case IM_TASK_INVENTORY_OFFERED:
		{
			// Someone has offered us some inventory.
			LLOfferInfo* info = new LLOfferInfo;

			if (dialog == IM_INVENTORY_OFFERED)
			{
				struct offer_agent_bucket_t
				{
					S8		asset_type;
					LLUUID	object_id;
				}* bucketp;

				if (bucket_size != sizeof(offer_agent_bucket_t))
				{
					llwarns << "Malformed inventory offer from agent"
							<< llendl;
					break;
				}
				bucketp = (struct offer_agent_bucket_t*)&binary_bucket[0];
				info->mType = (LLAssetType::EType)bucketp->asset_type;
				info->mObjectID = bucketp->object_id;
				info->mFromObject = false;
			}
			// IM_TASK_INVENTORY_OFFERED
			else
			{
				if (bucket_size == sizeof(S8))
				{
					info->mType = (LLAssetType::EType)binary_bucket[0];
				}
				else
				{
					// Rider - The previous version of the protocol returned
					// the wrong binary bucket... We still might be able to
					// figure out the type even though the offer is not
					// retrievable.
					std::string str(reinterpret_cast<char*>(binary_bucket));
					std::string str_type = str.substr(0, str.find('|'));
					std::stringstream type_convert(str_type);
					S32 type;
					type_convert >> type;
					// We could try AT_UNKNOWN which would be more accurate,
					// but it would cause an auto decline.
					info->mType = (LLAssetType::EType)type;
					// Do not break in the case of a bad binary bucket. Go
					// ahead and show the accept/decline popup even though it
					// will not do anything.
					llwarns << "Malformed inventory offer from object, type might be: "
							<< info->mType
							<< ". The offer will likely be impossible to process."
							<< llendl;
				}
				info->mObjectID.setNull();
				info->mFromObject = true;
			}

			// In the case of an offline message, the transaction Id is in
			// aux_id and the session_id is null
			info->mTransactionID = session_id.notNull() ? session_id : aux_id;
			
			info->mIM = dialog;
			info->mFromID = from_id;
			info->mFromGroup = from_group;
//MK
			std::string folder_name(message);
			if (gRLenabled &&
				!gSavedSettings.getBool("RestrainedLoveForbidGiveToRLV") &&
				info->mType == LLAssetType::AT_CATEGORY &&
				gRLInterface.getRlvShare() &&
				folder_name.find(RL_RLV_REDIR_FOLDER_PREFIX) == 1)
			{
				info->mFolderID = gRLInterface.getRlvShare()->getUUID();
			}
			else
//mk
			{
				info->mFolderID =
					gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(info->mType));
			}

			if (dialog == IM_TASK_INVENTORY_OFFERED)
			{
				info->mFromObject = true;
			}
			else
			{
				info->mFromObject = false;
			}
			info->mFromName = name;
//MK
			if (gRLenabled && gRLInterface.mContainsShowloc)
			{
				// Hide every occurrence of the Region and Parcel names if the
				// location restriction is active
				message = gRLInterface.getCensoredLocation(message);
			}
//mk
			info->mDesc = message;
			info->extractSLURL();
			info->mHost = sender;
			is_muted = LLMuteList::isMuted(from_id, name);
			if (is_muted)
			{
				static F32 last_notification = 0.f;
				// Do not spam with such messages...
				llinfos_once << "Declining inventory offer from muted object/agent: "
							 << info->mFromName << llendl;
				if (gFrameTimeSeconds - last_notification > 30.f)
				{
					LLSD args;
					args["NAME"] = info->mFromName;
					gNotifications.add("MutedObjectOfferDeclined", args);
					last_notification = gFrameTimeSeconds;
				}
				// Same as closing window
				info->forceResponse(IOR_MUTED);
			}
			else if (is_busy && dialog != IM_TASK_INVENTORY_OFFERED &&
					 gSavedSettings.getBool("RejectNewInventoryWhenBusy"))
			{
				// Until throttling is implemented, busy mode should reject
				// inventory instead of silently accepting it. SEE SL-39554
				info->forceResponse(IOR_BUSY);
			}
			else
			{
				info->inventoryOfferHandler();
			}
			break;
		}

		case IM_INVENTORY_ACCEPTED:
		{
			args["NAME"] = name;
			gNotifications.add("InventoryAccepted", args);
			break;
		}

		case IM_INVENTORY_DECLINED:
		{
			args["NAME"] = name;
			gNotifications.add("InventoryDeclined", args);
			break;
		}

		case IM_GROUP_VOTE:
		{
			llwarns << "Received deprecated IM event: IM_GROUP_VOTE" << llendl;
			break;
		}

		case IM_GROUP_ELECTION_DEPRECATED:
		{
			llwarns << "Received deprecated IM event: IM_GROUP_ELECTION_DEPRECATED"
					<< llendl;
			break;
		}

		case IM_SESSION_SEND:
		{
			if (!is_linden && is_busy)
			{
				return;
			}
			LLFloaterIMSession* floaterp =
				LLFloaterIMSession::findInstance(session_id);
			// Only show messages if we have a session open (which should
			// happen after you get an "invitation"
			if (!floaterp)
			{
				// Check to see if this was a snoozed session, and whether the
				// snooze delay expired or not.
				snoozed_map_t::iterator it = mSnoozedSessions.find(session_id);
				if (it == mSnoozedSessions.end())
				{
					return;	// Unexpected message for a closed session: ignore.
				}
				if (it->second > gFrameTimeSeconds)
				{
					LL_DEBUGS("InstantMessaging") << "Ignoring message for snoozed session Id: "
												  << session_id << LL_ENDL;
					return;
				}
				mSnoozedSessions.erase(it);
				LL_DEBUGS("InstantMessaging") << "Restoring snoozed session Id: "
											  << session_id << LL_ENDL;
			}
//MK
			if (gRLenabled)
			{
				if (!gRLInterface.canReceiveIM(from_id))
				{
					 // Agent is forbidden to receive IMs
					return;
				}
				// Group session ?
				if (floaterp && floaterp->isGroupSession() &&
					!gRLInterface.canSendGroupIM(floaterp->getSessionName()))
				{
					 // Agent is forbidden to receive group IMs
					return;
				}
			}
			if (gRLenabled && !gRLInterface.canReceiveIM(from_id))
			{
				 // Agent is forbidden to receive IMs
				return;
			}
//mk
			// Standard message, not from system
			std::string saved;
			if (offline == IM_OFFLINE)
			{
				saved = llformat("(Saved %s) ", formatted_time(timestamp).c_str());
			}
			buffer = separator_string + saved + message.substr(message_offset);
			addMessage(session_id, from_id, name, buffer,
					   ll_safe_string((char*)binary_bucket), IM_SESSION_INVITE,
					   parent_estate_id, region_id, position, true);
			if (gAutomationp)
			{
				gAutomationp->onInstantMsg(session_id, from_id, name, buffer);
			}

			chat.mText = "IM: " + name + separator_string + saved +
						 message.substr(message_offset);
			LLFloaterChat::addChat(chat, true, from_id == gAgentID);
			break;
		}

		case IM_FROM_TASK:
		{
			LL_DEBUGS("InstantMessaging") << "IM_FROM_TASK: owner: " << from_id
										  << " - Object name: " << name
										  << " - Object Id: " << session_id
										  << LL_ENDL;
			if (from_id == gAgentID)
			{
				is_owned_by_me = true;
			}
			if ((is_busy && !is_owned_by_me) ||
				LLMuteList::isMuted(from_id, LLMute::flagTextChat) ||
				LLMuteList::isMuted(session_id, name, LLMute::flagTextChat))
			{
				return;
			}

			chat.mFromName = name;

			// Build a link to open the object IM info window.
			std::string location = ll_safe_string((char*)binary_bucket,
												  bucket_size);
			LLSD query_string;
			query_string["owner"] = from_id;
			query_string["slurl"] = location.c_str();
			query_string["name"] = name;
			if (from_group)
			{
				query_string["groupowned"] = "true";
			}

			if (session_id.notNull())
			{
				chat.mFromID = session_id;
			}
			else
			{
				// This message originated on a region without the updated code
				// for task id and slurl information. We just need a unique ID
				// for this object that is not the owner ID. If it is the owner
				// ID, it will overwrite the style that contains the link to
				// that owner's profile. This is not ideal: it will make one
				// style for all objects owned by the the same person/group.
				// This works because the only thing we can really do in this
				// case is show the owner name and link to their profile.
				chat.mFromID = from_id ^ gAgentSessionID;
			}

			std::ostringstream link;
			link << "secondlife:///app/objectim/" << session_id
				 << LLURI::mapToQueryString(query_string);

			chat.mURL = link.str();
//MK
			if (gRLenabled)
			{
				if (gRLInterface.mContainsShowloc)
				{
					// hide the url
					chat.mURL = "";
					// Hide every occurrence of the Region and Parcel names if
					// the/ location restriction is active
					message = gRLInterface.getCensoredLocation(message);
				}

				if (gRLInterface.mContainsShownames)
				{
					message = gRLInterface.getCensoredMessage(message);
				}
			}
//mk
			chat.mText = name + separator_string +
						 message.substr(message_offset);

			// Note: lie to LLFloaterChat::addChat(), pretending that this is
			// NOT an IM, because IMs from objects do not open IM sessions.
			// However, display it like a direct chat from object.
			chat.mChatType = CHAT_TYPE_DIRECT;
			chat.mOwnerID = from_id;
			if (is_owned_by_me &&
				HBViewerAutomation::checkLuaCommand(message, from_id, name))
			{
				return;
			}
			if (gAutomationp)
			{
				gAutomationp->onReceivedChat(chat.mChatType, from_id, name,
											 chat.mText);
			}
			LLFloaterChat::addChat(chat, false, false);
			break;
		}

		case IM_FROM_TASK_AS_ALERT:
		{
			if (is_busy && !is_owned_by_me)
			{
				return;
			}

			// Construct a viewer alert for this message.
//MK
			if (gRLenabled)
			{
				if (gRLInterface.mContainsShowloc)
				{
					// Hide every occurrence of the Region and Parcel names if
					// the location restriction is active
					message = gRLInterface.getCensoredLocation(message);
				}

				if (gRLInterface.mContainsShownames ||
					gRLInterface.mContainsShownametags)
				{
					// Censor object IMs but not avatar IMs
					message = gRLInterface.getCensoredMessage(message);
				}
			}
//mk
			args["NAME"] = name;
			args["MESSAGE"] = message;
			gNotifications.add("ObjectMessage", args);
			break;
		}

		case IM_BUSY_AUTO_RESPONSE:
		{
			if (is_muted)
			{
				LL_DEBUGS("InstantMessaging") << "Ignoring busy response from "
											  << from_id << LL_ENDL;
				return;
			}
			else
			{
				// *TODO: translate.
				buffer = llformat("%s (%s): %s", name.c_str(), "busy response",
								  message.substr(message_offset).c_str());
				addMessage(session_id, from_id, name, buffer);
			}
			break;
		}

		case IM_LURE_USER:
		case IM_TELEPORT_REQUEST:
		{
			if (LLMuteList::isMuted(from_id, name))
			{
				return;
			}
//MK
			bool auto_accept = false;
			if (gRLenabled)
			{
				std::string behav = dialog == IM_LURE_USER ? "accepttp"
														   : "accepttprequest";
				auto_accept = gRLInterface.contains(behav) ||
							  gRLInterface.contains(behav + ":" +
													from_id.asString());
			}
//mk
			if (is_busy
//MK
				// Even in busy mode, accept if we are forced to
				&& !auto_accept)
//mk
			{
				busy_message(from_id);
			}
			else
			{
//MK
				if (gRLenabled && dialog == IM_LURE_USER)
				{
					if (gRLInterface.containsWithoutException("tplure",
															  from_id.asString()) ||
						(gRLInterface.mContainsUnsit &&
						 isAgentAvatarValid() && gAgentAvatarp->mIsSitting))
					{
						std::string response =
							"The Resident you invited is prevented from accepting teleport offers. Please try again later.";
						pack_instant_message(gAgentID, false, gAgentSessionID,
											 from_id, SYSTEM_FROM,
											 response.c_str(), IM_ONLINE,
											 IM_BUSY_AUTO_RESPONSE);
						gAgent.sendReliableMessage();
						return;
					}
				}

				if (gRLenabled && dialog == IM_TELEPORT_REQUEST)
				{
					if (gRLInterface.containsWithoutException("tprequest",
															  from_id.asString()))
					{
						std::string response =
							"The Resident you invited is prevented from accepting teleport requests. Please try again later.";
						pack_instant_message(gAgentID, false, gAgentSessionID,
											 from_id, SYSTEM_FROM,
											 response.c_str(), IM_ONLINE,
											 IM_BUSY_AUTO_RESPONSE);
						gAgent.sendReliableMessage();
						return;
					}
				}

				if (gRLenabled &&
					(gRLInterface.mContainsShowloc ||
					 !gRLInterface.canReceiveIM(from_id)))
				{
					message = "(Hidden)";
				}

				if (gRLenabled && dialog == IM_LURE_USER && auto_accept)
				{
					// accepttp => the viewer acts like it was teleported by a god
					gRLInterface.setAllowCancelTp(false);
					LLSD payload;
					payload["from_id"] = from_id;
					payload["lure_id"] = session_id;
					payload["godlike"] = true;
					// do not show a message box, because you're about to be
					// teleported.
					gNotifications.forceResponse(LLNotification::Params("TeleportOffered").payload(payload), 0);
				}
				else if (gRLenabled && dialog == IM_TELEPORT_REQUEST && auto_accept)
				{
					// accepttprequest => the viewer automatically sends the TP
					LLSD dummy_notification, dummy_response;
					dummy_notification["payload"]["ids"][0] = from_id;
					dummy_response["message"] = "Automatic teleport offer";
					send_lures(dummy_notification, dummy_response);
				}
				else
//mk
				{
					LLSD args;
					// *TODO:translate -> [FIRST] [LAST] (maybe)
					args["NAME"] = name;
					args["MESSAGE"] = message;
					LLSD payload;
					payload["from_id"] = from_id;
					payload["lure_id"] = session_id;
					payload["godlike"] = false;
					if (dialog == IM_TELEPORT_REQUEST)
					{
						gNotifications.add("TeleportRequest", args, payload);
					}
					else
					{
						gNotifications.add("TeleportOffered", args, payload);
					}
				}
			}
			break;
		}

		case IM_GODLIKE_LURE_USER:
		{
			LLSD payload;
			payload["from_id"] = from_id;
			payload["lure_id"] = session_id;
			payload["godlike"] = true;
			// Do not show a message box, because you're about to be
			// teleported.
			gNotifications.forceResponse(LLNotification::Params("TeleportOffered").payload(payload), 0);
			break;
		}

		case IM_GOTO_URL:
		{
			LLSD args;
			// N.B.: this is for URLs sent by the system, not for URLs sent by
			// scripts (i.e. llLoadURL)
			if (bucket_size <= 0)
			{
				llwarns << "bad bucket_size: " << bucket_size
						<< " - aborting function." << llendl;
				return;
			}

			std::string url;

			url.assign((char*)binary_bucket, bucket_size - 1);
			args["MESSAGE"] = message;
			args["URL"] = url;
			LLSD payload;
			payload["url"] = url;
			gNotifications.add("GotoURL", args, payload);
			break;
		}

		case IM_FRIENDSHIP_OFFERED:
		{
			LLSD payload;
			payload["from_id"] = from_id;
			payload["session_id"] = session_id;;
			payload["online"] = offline == IM_ONLINE;
			payload["sender"] = sender.getIPandPort();

			if (is_busy)
			{
				busy_message(from_id);
				gNotifications.forceResponse(LLNotification::Params("OfferFriendship").payload(payload), 1);
			}
			else if (LLMuteList::isMuted(from_id, name))
			{
				gNotifications.forceResponse(LLNotification::Params("OfferFriendship").payload(payload), 1);
			}
			else
			{
				args["[NAME]"] = name;
//MK
				if (gRLenabled && !gRLInterface.canReceiveIM(from_id))
				{
					message = "(Hidden)";
				}
//mk
				if (message.empty())
				{
					// Support for frienship offers from clients before 07/2008
					gNotifications.add("OfferFriendshipNoMessage", args,
									   payload);
				}
				else
				{
					args["[MESSAGE]"] = message;
					gNotifications.add("OfferFriendship", args, payload);
				}
			}
			break;
		}

		case IM_FRIENDSHIP_ACCEPTED:
		{
			// In the case of an offline IM, the formFriendship() may be
			// extraneous as the database should already include the
			// relationship. But it does not hurt for dupes.
			LLAvatarTracker::formFriendship(from_id);

			std::vector<std::string> strings;
			strings.emplace_back(from_id.asString());
			send_generic_message("requestonlinenotification", strings);

			args["NAME"] = name;
			gNotifications.add("FriendshipAccepted", args);
			break;
		}

		case IM_FRIENDSHIP_DECLINED_DEPRECATED:
		default:
		{
			llwarns << "Instant message calling for unknown dialog "
					<< (S32)dialog << llendl;
		}
	}

	if (gWindowp && gWindowp->getMinimized())
	{
		F32 flash_time = gSavedSettings.getF32("TaskBarButtonFlashTime");
		if (flash_time > 0.f)
		{
			gWindowp->flashIcon(flash_time);
		}
	}
}

bool LLIMMgr::processIMTypingCore(EInstantMessage dialog,
								  const LLUUID& from_id,
								  const std::string& from_name, bool typing)
{
	LLUUID session_id = computeSessionID(dialog, from_id);
	LLFloaterIMSession* floater = LLFloaterIMSession::findInstance(session_id);
	if (floater)
	{
		floater->processIMTyping(from_id, from_name, typing);
		return true;
	}

	return false;
}

void LLIMMgr::updateFloaterSessionID(const LLUUID& old_session_id,
									 const LLUUID& new_session_id)
{
	LLFloaterIMSession* floater =
		LLFloaterIMSession::findInstance(old_session_id);
	if (floater)
	{
		floater->sessionInitReplyReceived(new_session_id);
	}
}

// LLFloaterIMSession::sessionInitReplyReceived() above will call back this
// method:
//static
void LLIMMgr::deliverMessage(const std::string& utf8_text,
							 const LLUUID& im_session_id,
							 const LLUUID& other_participant_id,
							 EInstantMessage dialog)
{
	std::string name;
	gAgent.buildFullname(name);

	const LLRelationship* info =
		gAvatarTracker.getBuddyInfo(other_participant_id);
	U8 offline = (!info || info->isOnline()) ? IM_ONLINE : IM_OFFLINE;
	// Send message normally. Default to IM_SESSION_SEND unless it is nothing
	// special, in which case it is probably an IM to everyone.
	U8 new_dialog = dialog;
	if (dialog != IM_NOTHING_SPECIAL)
	{
		new_dialog = IM_SESSION_SEND;
	}
	pack_instant_message(gAgentID, false, gAgentSessionID,
						 other_participant_id, name.c_str(), utf8_text.c_str(),
						 offline, (EInstantMessage)new_dialog, im_session_id);
	gAgent.sendReliableMessage();

	// If there is a mute list and this is not a group chat the target should
	// not be in our mute list for some message types. Auto-remove them if
	// present.
	switch (dialog)
	{
#if 0	// Enabling this makes it impossible to mute permanently a resident who
		// initiated a group IM session (posting in the group chat would unmute
		// them)
		case IM_SESSION_INVITE:
#endif
		case IM_NOTHING_SPECIAL:
		case IM_GROUP_INVITATION:
		case IM_INVENTORY_OFFERED:
		case IM_SESSION_P2P_INVITE:
		case IM_SESSION_CONFERENCE_START:
		case IM_SESSION_SEND: // Marginal: erring on the side of hearing.
		case IM_LURE_USER:
		case IM_GODLIKE_LURE_USER:
		case IM_FRIENDSHIP_OFFERED:
			LLMuteList::autoRemove(other_participant_id, LLMuteList::AR_IM);
			break;
		default: ; // do nothing
	}
}

//static
bool LLIMMgr::requestOfflineMessages()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg || gDisconnected)
	{
		return false;
	}

	if (!gAgent.regionCapabilitiesReceived())
	{
		return false;
	}

	if (!gSavedSettings.getBool("UseOfflineIMsCapability"))
	{
		return requestOfflineMessagesLegacy();
	}

	const std::string& cap_url = gAgent.getRegionCapability("ReadOfflineMsgs");
	if (cap_url.empty() ||
		// NOTE: Offline messages capability provides no session/transaction
		// Ids for message AcceptFriendship and IM_GROUP_INVITATION to work,
		// so make sure we have the necessary caps before using it.
		!gAgent.hasRegionCapability("AcceptFriendship") ||
		!gAgent.hasRegionCapability("AcceptGroupInvite"))
	{
		return requestOfflineMessagesLegacy();
	}

	LL_DEBUGS("InstantMessaging") << "Using capability for offline instant messages request"
								  << LL_ENDL;
	gCoros.launch("requestOfflineMessagesCoro",
				  boost::bind(&LLIMMgr::requestOfflineMessagesCoro, cap_url));
	return true;
}

//static
bool LLIMMgr::requestOfflineMessagesLegacy()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg)
	{
		return false;
	}

	LL_DEBUGS("InstantMessaging") << "Using UDP messaging for offline instant messages request"
								  << LL_ENDL;

	msg->newMessageFast(_PREHASH_RetrieveInstantMessages);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	gAgent.sendReliableMessage();
	return true;
}

//static
void LLIMMgr::requestOfflineMessagesCoro(const std::string& url)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("requestOfflineMessages");
	LLSD result = adapter.getAndSuspend(url);

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp || !gIMMgrp)
	{
		return; // Viewer is likely disconnected or closing down !
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Error requesting offline messages via capability. Error: "
				<< status.toString() << llendl;
		if (requestOfflineMessagesLegacy())
		{
			llinfos << "Sent offline messages request via legacy UDP messaging"
					<< llendl;
		}
		else
		{
			llwarns << "Failed to send offline messages request via legacy UDP messaging"
					<< llendl;
		}
		return;
	}

	const LLSD& contents =
		result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_CONTENT];
	if (!contents.size())
	{
		llinfos << "No contents received for offline messages via capability"
				<< llendl;
		return;
	}

	LLSD messages;
	if (contents.isArray())
	{
		messages = *contents.beginArray();
	}
	else if (contents.has("messages"))
	{
		messages = contents["messages"];
	}
	else
	{
		llwarns << "Malformed contents received for offline messages via capability"
				<< llendl;
		return;
	}
	if (!messages.isArray())
	{
		llwarns << "Malformed contents received for offline messages via capability"
				<< llendl;
		return;
	}
	if (!messages.size())
	{
		// Nothing to process
		return;
	}

	std::vector<U8> data;
	LLVector3 position;
	std::string message, from_name;
	LLUUID session_id;
	LLHost sender = regionp->getHost();
	for (LLSD::array_iterator it = messages.beginArray(),
							  end = messages.endArray();
		 it != end; ++it)
	{
		const LLSD& message_data(*it);

		LL_DEBUGS("InstantMessaging") << "Processing offline message:\n";
		std::stringstream str;
		LLSDSerialize::toPrettyXML(message_data, str);
		LL_CONT << "\n" << str.str() << LL_ENDL;

		EInstantMessage dialog =
			(EInstantMessage)message_data["dialog"].asInteger();

		if (message_data.has("message"))
		{
			message = message_data["message"].asString();
			LL_DEBUGS("InstantMessaging") << "Found 'message'" << LL_ENDL;
		}
		else
		{
			message.clear();
			LL_DEBUGS("InstantMessaging") << "No message !" << LL_ENDL;
		}

		U32 parent_estate_id = 1; // 1 = Main land
		if (message_data.has("parent_estate_id"))
		{
			parent_estate_id = message_data["parent_estate_id"].asInteger();
			LL_DEBUGS("InstantMessaging") << "Found 'parent_estate_id': "
										  << parent_estate_id << LL_ENDL;
		}

		if (message_data.has("position"))
		{
			position.setValue(message_data["position"]);
			LL_DEBUGS("InstantMessaging") << "Found 'position'" << LL_ENDL;
		}
		else if (message_data.has("local_x"))
		{
			position.set(message_data["local_x"].asReal(),
						 message_data["local_y"].asReal(),
						 message_data["local_z"].asReal());
			LL_DEBUGS("InstantMessaging") << "Found 'local_x/y/z'" << LL_ENDL;
		}
		else
		{
			position.clear();
			LL_DEBUGS("InstantMessaging") << "No position !" << LL_ENDL;
		}

		data.clear();
		if (message_data.has("binary_bucket"))
		{
			data = message_data["binary_bucket"].asBinary();
			LL_DEBUGS("InstantMessaging") << "Found 'binary_bucket'"
										  << LL_ENDL;
		}
		else
		{
			data.push_back(0);
		}

		bool from_group;
		if (message_data["from_group"].isInteger())
		{
			from_group = message_data["from_group"].asInteger();
		}
		else
		{
			from_group = message_data["from_group"].asString() == "Y";
		}

		if (message_data.has("transaction-id"))
		{
			session_id = message_data["transaction-id"].asUUID();
			LL_DEBUGS("InstantMessaging") << "Found 'transaction-id': "
										  << session_id << LL_ENDL;
		}
		// Fallbacks, in case LL changes this field name for something more
		// coherent (no dash is ever used in other names but underline is) or
		// meaningful (this actually is a session Id) in the future... HB
		else if (message_data.has("transaction_id"))
		{
			session_id = message_data["transaction_id"].asUUID();
			LL_DEBUGS("InstantMessaging") << "Found 'transaction_id': "
										  << session_id << LL_ENDL;
		}
		else if (message_data.has("session_id"))
		{
			session_id = message_data["session_id"].asUUID();
			LL_DEBUGS("InstantMessaging") << "Found 'session_id': "
										  << session_id << LL_ENDL;
		}
		else
		{
			session_id.setNull();
			LL_DEBUGS("InstantMessaging") << "No session/transaction id !"
										  << LL_ENDL;
		}

		if (session_id.isNull() && dialog == IM_FROM_TASK)
		{
			session_id = message_data["asset_id"].asUUID();
			LL_DEBUGS("InstantMessaging") << "IM_FROM_TASK: using the asset Id for the session Id"
										  << LL_ENDL;
		}

		U8 im_type = IM_OFFLINE;
		if (message_data.has("offline"))
		{
			im_type = (U8)message_data["offline"].asInteger();
			LL_DEBUGS("InstantMessaging") << "Found 'offline': "
										  << (S32)im_type << LL_ENDL;
		}

		if (message_data.has("from_agent_name"))
		{
			from_name = message_data["from_agent_name"].asString();
			LL_DEBUGS("InstantMessaging") << "Found 'from_agent_name': "
										  << from_name << LL_ENDL;
		}
		else if (message_data.has("from_name"))
		{
			from_name = message_data["from_name"].asString();
			LL_DEBUGS("InstantMessaging") << "Found 'from_name': "
										  << from_name << LL_ENDL;
		}
		else
		{
			from_name.clear();
			LL_DEBUGS("InstantMessaging") << "No originator name !" << LL_ENDL;
		}

		gIMMgrp->processNewMessage(message_data["from_agent_id"].asUUID(),
								   from_group,
								   message_data["to_agent_id"].asUUID(),
								   im_type, dialog, session_id,
								   (U32)message_data["timestamp"].asInteger(),
								   from_name, message, parent_estate_id,
								   message_data["region_id"].asUUID(),
						  		   position, data.data(), data.size(), sender,
								   // Not necessarily an asset
								   message_data["asset_id"].asUUID());
	}
}

class LLViewerChatterBoxSessionStartReply final : public LLHTTPNode
{
public:
	void describe(Description& desc) const override
	{
		desc.shortInfo("Used for receiving a reply to a request to initialize an ChatterBox session");
		desc.postAPI();
		desc.input("{\"client_session_id\": UUID, \"session_id\": UUID, \"success\" boolean, \"reason\": string");
		desc.source(__FILE__, __LINE__);
	}

	void post(ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		LLSD body;
		LLUUID temp_session_id;
		LLUUID session_id;
		bool success;

		if (!gIMMgrp) return;

		body = input["body"];
		success = body["success"].asBoolean();
		temp_session_id = body["temp_session_id"].asUUID();

		if (success)
		{
			session_id = body["session_id"].asUUID();
			gIMMgrp->updateFloaterSessionID(temp_session_id, session_id);
			LLFloaterIMSession* floaterp =
				LLFloaterIMSession::findInstance(session_id);
			if (floaterp)
			{
				floaterp->setSpeakers(body);

				// Apply updates we have possibly received previously
				floaterp->updateSpeakersList(gIMMgrp->getPendingAgentListUpdates(session_id));

				if (body.has("session_info"))
				{
					floaterp->processSessionUpdate(body["session_info"]);
				}

				// Apply updates we have possibly received previously
				floaterp->updateSpeakersList(gIMMgrp->getPendingAgentListUpdates(session_id));
			}
			gIMMgrp->clearPendingAgentListUpdates(session_id);
		}
		else
		{
			// Throw an error dialog and close the temp session's floater
			LLFloaterIMSession* floater =
				LLFloaterIMSession::findInstance(temp_session_id);
			if (floater)
			{
				floater->showSessionStartError(body["error"].asString());
			}
		}

		gIMMgrp->clearPendingAgentListUpdates(session_id);
	}
};

class LLViewerChatterBoxSessionEventReply final : public LLHTTPNode
{
public:
	void describe(Description& desc) const override
	{
		desc.shortInfo("Used for receiving a reply to a ChatterBox session event");
		desc.postAPI();
		desc.input("{\"event\": string, \"reason\": string, \"success\": boolean, \"session_id\": UUID");
		desc.source(__FILE__, __LINE__);
	}

	void post(ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		LLUUID session_id;
		bool success;

		LLSD body = input["body"];
		success = body["success"].asBoolean();
		session_id = body["session_id"].asUUID();

		if (!success)
		{
			// Throw an error dialog
			LLFloaterIMSession* floater =
				LLFloaterIMSession::findInstance(session_id);
			if (floater)
			{
				floater->showSessionEventError(body["event"].asString(),
											   body["error"].asString());
			}
		}
	}
};

class LLViewerForceCloseChatterBoxSession final : public LLHTTPNode
{
public:
	void post(ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		LLUUID session_id = input["body"]["session_id"].asUUID();
		LLFloaterIMSession* floaterp =
			LLFloaterIMSession::findInstance(session_id);
		if (floaterp)
		{
			std::string reason = input["body"]["reason"].asString();
			floaterp->showSessionForceClose(reason);
		}
	}
};

class LLViewerChatterBoxSessionAgentListUpdates final : public LLHTTPNode
{
public:
	void post(ResponsePtr responder, const LLSD& context,
			  const LLSD& input) const override
	{
		if (!gIMMgrp) return;

		LLUUID session_id = input["body"]["session_id"].asUUID();
		LLFloaterIMSession* floaterp =
			LLFloaterIMSession::findInstance(session_id);
		if (floaterp)
		{
			floaterp->updateSpeakersList(input["body"]);
		}
		else
		{
			// We do not have a floater yet: something went wrong and we are
			// probably receiving an update here before a start or an
			// acceptance of an invitation. Race condition.
			gIMMgrp->addPendingAgentListUpdates(session_id, input["body"]);
		}
	}
};

class LLViewerChatterBoxSessionUpdate final : public LLHTTPNode
{
public:
	void post(ResponsePtr responder, const LLSD& context,
			  const LLSD& input) const override
	{
		LLFloaterIMSession* floaterp =
			LLFloaterIMSession::findInstance(input["body"]["session_id"].asUUID());
		if (floaterp)
		{
			floaterp->processSessionUpdate(input["body"]["info"]);
		}
	}
};

class LLViewerChatterBoxInvitation final : public LLHTTPNode
{
public:
	void post(ResponsePtr response,	const LLSD& context,
			  const LLSD& input) const override
	{
		if (!gIMMgrp) return;

		// For backwards compatiblity reasons... we need to still check for
		// 'text' or 'voice' invitations... bleh
		if (input["body"].has("instantmessage"))
		{
			LLSD message_params =
				input["body"]["instantmessage"]["message_params"];

			// Do something here to have the IM invite behave just like a
			// normal IM; this is just replicated code from process_improved_im
			// and should really go in its own function - jwolk

			std::string message = message_params["message"].asString();
			std::string name = message_params["from_name"].asString();
			if (LLAvatarName::sOmitResidentAsLastName)
			{
				name = LLCacheName::cleanFullName(name);
			}
			LLUUID from_id = message_params["from_id"].asUUID();
			LLUUID session_id = message_params["id"].asUUID();
			const LLSD::Binary& bin_bucket =
				message_params["data"]["binary_bucket"].asBinary();
			U8 offline = (U8)message_params["offline"].asInteger();

			time_t timestamp = (time_t)message_params["timestamp"].asInteger();

			bool is_busy = gAgent.getBusy();

			bool is_muted = LLMuteList::isMuted(from_id, name,
												LLMute::flagTextChat);
			bool is_linden = LLMuteList::isLinden(name);

			std::string separator_string(": ");
			size_t message_offset = 0;

			// Handle IRC styled /me messages.
			std::string prefix = message.substr(0, 4);
			if (prefix == "/me " || prefix == "/me'")
			{
				separator_string.clear();
				message_offset = 3;
			}

			LLChat chat;
			chat.mMuted = is_muted && !is_linden;
			chat.mFromID = from_id;
			chat.mFromName = name;

			if (!is_linden && (is_busy || is_muted))
			{
				return;
			}
//MK
			if (gRLenabled && !gRLInterface.canReceiveIM(from_id))
			{
				return;
			}
//mk
			// Standard message, not from system
			std::string saved;
			if (offline == IM_OFFLINE)
			{
				saved = llformat("(Saved %s) ",
								 formatted_time(timestamp).c_str());
			}
			std::string buffer = separator_string + saved +
								 message.substr(message_offset);

			bool is_this_agent = false;
			if (from_id == gAgentID)
			{
				is_this_agent = true;
			}

			// Do not process muted IMs
			if (!is_this_agent && !LLMuteList::isLinden(name) &&
				LLMuteList::isMuted(from_id, LLMute::flagTextChat))
			{
				// Muted agent
				return;
			}
			else if (session_id.notNull() &&
					 LLMuteList::isMuted(session_id, LLMute::flagTextChat))
			{
				// Muted group
				return;
			}

			gIMMgrp->addMessage(session_id, from_id, name, buffer,
								std::string((char*)&bin_bucket[0]),
								IM_SESSION_INVITE,
								message_params["parent_estate_id"].asInteger(),
								message_params["region_id"].asUUID(),
								ll_vector3_from_sd(message_params["position"]),
								true);
			if (gAutomationp)
			{
				gAutomationp->onInstantMsg(session_id, from_id, name, buffer);
				if (!LLFloaterIMSession::findInstance(session_id))
				{
					// If the automation script OnInstantMsg() callback closed
					// the session as a result of this IM, abort now.
					return;
				}
			}

			chat.mText = "IM: " + name + separator_string + saved +
						 message.substr(message_offset);
			LLFloaterChat::addChat(chat, true, is_this_agent);

			// OK, now we want to accept the invitation
			const std::string& url =
				gAgent.getRegionCapability("ChatSessionRequest");
			if (!url.empty())
			{
				gCoros.launch("chatterBoxInvitationCoro",
							  boost::bind(&LLIMMgr::chatterBoxInvitationCoro,
										  url, session_id,
										  LLIMMgr::INVITATION_TYPE_INSTANT_MESSAGE));
			}
		}
		else if (input["body"].has("voice"))
		{
			if (!LLVoiceClient::voiceEnabled())
			{
				// Do not display voice invites unless the user has voice
				// enabled
				return;
			}

			gIMMgrp->inviteToSession(input["body"]["session_id"].asUUID(),
									 input["body"]["session_name"].asString(),
									 input["body"]["from_id"].asUUID(),
									 input["body"]["from_name"].asString(),
									 IM_SESSION_INVITE,
									 LLIMMgr::INVITATION_TYPE_VOICE);
		}
		else if (input["body"].has("immediate"))
		{
			gIMMgrp->inviteToSession(input["body"]["session_id"].asUUID(),
									 input["body"]["session_name"].asString(),
									 input["body"]["from_id"].asUUID(),
									 input["body"]["from_name"].asString(),
									 IM_SESSION_INVITE,
									 LLIMMgr::INVITATION_TYPE_IMMEDIATE);
		}
	}
};

LLHTTPRegistration<LLViewerChatterBoxSessionStartReply>
   gHTTPRegistrationMessageChatterboxsessionstartreply(
	   "/message/ChatterBoxSessionStartReply");

LLHTTPRegistration<LLViewerChatterBoxSessionEventReply>
   gHTTPRegistrationMessageChatterboxsessioneventreply(
	   "/message/ChatterBoxSessionEventReply");

LLHTTPRegistration<LLViewerForceCloseChatterBoxSession>
    gHTTPRegistrationMessageForceclosechatterboxsession(
		"/message/ForceCloseChatterBoxSession");

LLHTTPRegistration<LLViewerChatterBoxSessionAgentListUpdates>
    gHTTPRegistrationMessageChatterboxsessionagentlistupdates(
	    "/message/ChatterBoxSessionAgentListUpdates");

LLHTTPRegistration<LLViewerChatterBoxSessionUpdate>
    gHTTPRegistrationMessageChatterBoxSessionUpdate(
	    "/message/ChatterBoxSessionUpdate");

LLHTTPRegistration<LLViewerChatterBoxInvitation>
    gHTTPRegistrationMessageChatterBoxInvitation(
		"/message/ChatterBoxInvitation");
