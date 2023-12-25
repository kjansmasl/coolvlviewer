/**
 * @file llvoicechannel.cpp
 * @brief Voice Channel related classes
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

#include "llvoicechannel.h"

#include "llcachename.h"
#include "llcorehttputil.h"
#include "llnotifications.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llimmgr.h"
#include "llmediactrl.h"
#include "llviewercontrol.h"
#include "llvoiceclient.h"

constexpr U32 DEFAULT_RETRIES_COUNT = 3;

// Static variables
LLVoiceChannel::voice_channel_map_t LLVoiceChannel::sVoiceChannelMap;
LLVoiceChannel::voice_channel_map_uri_t LLVoiceChannel::sVoiceChannelURIMap;
LLVoiceChannel* LLVoiceChannel::sCurrentVoiceChannel = NULL;
LLVoiceChannel* LLVoiceChannel::sSuspendedVoiceChannel = NULL;
bool LLVoiceChannel::sSuspended = false;

///////////////////////////////////////////////////////////////////////////////
// Global command handler for voicecallavatar
///////////////////////////////////////////////////////////////////////////////

class LLVoiceCallAvatarHandler final : public LLCommandHandler
{
public:
	LLVoiceCallAvatarHandler()
	:	LLCommandHandler("voicecallavatar", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		// Make sure we have some parameters
		if (params.size() == 0)
		{
			return false;
		}

		// Get the ID
		LLUUID id;
		if (!id.set(params[0], false))
		{
			return false;
		}

		std::string name;
		if (gIMMgrp && gCacheNamep && gCacheNamep->getFullName(id, name))
		{
			// Once the IM panel will be opened, and provided that both the
			// caller and the recipient are voice-enabled, the user will be
			// only one click away from an actual voice call...
			// When no voice is available, this action is still consistent
			// With the "Call" link it is associated with in web profiles.
			gIMMgrp->setFloaterOpen(true);
			gIMMgrp->addSession(name, IM_NOTHING_SPECIAL, id);
			make_ui_sound("UISndStartIM");
		}

		return true;
	}
};

LLVoiceCallAvatarHandler gVoiceCallAvatarHandler;

///////////////////////////////////////////////////////////////////////////////
// LLVoiceChannel class
///////////////////////////////////////////////////////////////////////////////

LLVoiceChannel::LLVoiceChannel(const LLUUID& session_id,
							   const std::string& session_name)
:	mSessionID(session_id),
	mState(STATE_NO_CHANNEL_INFO),
	mSessionName(session_name),
	mIgnoreNextSessionLeave(false)
{
	mNotifyArgs["VOICE_CHANNEL_NAME"] = mSessionName;

	if (!sVoiceChannelMap.emplace(session_id, this).second)
	{
		// A voice channel already exists for this session id, so this instance
		// will be orphaned the end result should simply be the failure to make
		// voice calls
		llwarns << "Duplicate voice channels registered for session_id "
				<< session_id << llendl;
	}
}

LLVoiceChannel::~LLVoiceChannel()
{
	if (LLVoiceClient::sInitDone)	// be sure to keep this !
	{
		gVoiceClient.removeObserver(this);
	}
	sVoiceChannelMap.erase(mSessionID);
	sVoiceChannelURIMap.erase(mURI);
}

void LLVoiceChannel::setChannelInfo(const std::string& uri,
									const std::string& credentials)
{
	setURI(uri);

	mCredentials = credentials;

	if (mState == STATE_NO_CHANNEL_INFO)
	{
		if (mURI.empty())
		{
			gNotifications.add("VoiceChannelJoinFailed", mNotifyArgs);
			llwarns << "Received empty URI for channel " << mSessionName
					<< llendl;
			deactivate();
		}
		else if (mCredentials.empty())
		{
			gNotifications.add("VoiceChannelJoinFailed", mNotifyArgs);
			llwarns << "Received empty credentials for channel "
					<< mSessionName << llendl;
			deactivate();
		}
		else
		{
			setState(STATE_READY);

			// If we are supposed to be active, reconnect. This will happen on
			// initial connect, as we request credentials on first use
			if (sCurrentVoiceChannel == this)
			{
				// just in case we got new channel info while active
				// should move over to new channel
				activate();
			}
		}
	}
}

void LLVoiceChannel::onChange(EStatusType type, const std::string& channel_uri,
							  bool)
{
	if (channel_uri == mURI)
	{
		if (type < BEGIN_ERROR_STATUS)
		{
			handleStatusChange(type);
		}
		else
		{
			handleError(type);
		}
	}
}

void LLVoiceChannel::handleStatusChange(EStatusType type)
{
	// status updates
	switch (type)
	{
		case STATUS_LOGIN_RETRY:
			gNotifications.add("VoiceLoginRetry");
			break;

		case STATUS_LOGGED_IN:
			break;

		case STATUS_LEFT_CHANNEL:
			if (callStarted() && !mIgnoreNextSessionLeave && !sSuspended)
			{
				// if forcibly removed from channel update the UI and revert to
				// default channel
				gNotifications.add("VoiceChannelDisconnected", mNotifyArgs);
				deactivate();
			}
			mIgnoreNextSessionLeave = false;
			break;

		case STATUS_JOINING:
			if (callStarted())
			{
				setState(STATE_RINGING);
			}
			break;

		case STATUS_JOINED:
			if (callStarted())
			{
				setState(STATE_CONNECTED);
			}

		default:
			break;
	}
}

// Default behavior is to just deactivate channel derived classes provide
// specific error messages
void LLVoiceChannel::handleError(EStatusType type)
{
	deactivate();
	setState(STATE_ERROR);
}

bool LLVoiceChannel::isActive()
{
	// Only considered active when currently bound channel matches what our
	// channel
	return callStarted() && gVoiceClient.getCurrentChannel() == mURI;
}

bool LLVoiceChannel::callStarted()
{
	return mState >= STATE_CALL_STARTED;
}

void LLVoiceChannel::deactivate()
{
	if (mState >= STATE_RINGING)
	{
		// ignore session leave event
		mIgnoreNextSessionLeave = true;
	}

	if (callStarted())
	{
		setState(STATE_HUNG_UP);
		// mute the microphone if required when returning to the proximal
		// channel
		if (sCurrentVoiceChannel == this &&
			gSavedSettings.getBool("AutoDisengageMic"))
		{
			gSavedSettings.setBool("PTTCurrentlyEnabled", true);
		}
	}
	gVoiceClient.removeObserver(this);

	if (sCurrentVoiceChannel == this)
	{
		// default channel is proximal channel
		sCurrentVoiceChannel = LLVoiceChannelProximal::getInstance();
		sCurrentVoiceChannel->activate();
	}
}

void LLVoiceChannel::activate()
{
	if (callStarted())
	{
		return;
	}

	// deactivate old channel and mark ourselves as the active one
	if (sCurrentVoiceChannel != this)
	{
		// mark as current before deactivating the old channel to prevent
		// activating the proximal channel between IM calls
		LLVoiceChannel* old_channel = sCurrentVoiceChannel;
		sCurrentVoiceChannel = this;
		if (old_channel)
		{
			old_channel->deactivate();
		}
	}

	if (mState == STATE_NO_CHANNEL_INFO)
	{
		// responsible for setting status to active
		getChannelInfo();
	}
	else
	{
		setState(STATE_CALL_STARTED);
	}

	gVoiceClient.addObserver(this);
}

void LLVoiceChannel::getChannelInfo()
{
	// pretend we have everything we need
	if (sCurrentVoiceChannel == this)
	{
		setState(STATE_CALL_STARTED);
	}
}

//static
LLVoiceChannel* LLVoiceChannel::getChannelByID(const LLUUID& session_id)
{
	voice_channel_map_t::iterator found_it = sVoiceChannelMap.find(session_id);
	if (found_it == sVoiceChannelMap.end())
	{
		return NULL;
	}
	return found_it->second;
}

//static
LLVoiceChannel* LLVoiceChannel::getChannelByURI(std::string uri)
{
	voice_channel_map_uri_t::iterator found_it = sVoiceChannelURIMap.find(uri);
	if (found_it == sVoiceChannelURIMap.end())
	{
		return NULL;
	}
	else
	{
		return found_it->second;
	}
}

void LLVoiceChannel::updateSessionID(const LLUUID& new_session_id)
{
	sVoiceChannelMap.erase(sVoiceChannelMap.find(mSessionID));
	mSessionID = new_session_id;
	sVoiceChannelMap.emplace(mSessionID, this);
}

void LLVoiceChannel::setURI(std::string uri)
{
	sVoiceChannelURIMap.erase(mURI);
	mURI = uri;
	sVoiceChannelURIMap.insert(std::make_pair(mURI, this));
}

void LLVoiceChannel::setState(EState state)
{
	if (!gIMMgrp) return;

	switch (state)
	{
		case STATE_RINGING:
			gIMMgrp->addSystemMessage(mSessionID, "ringing", mNotifyArgs);
			break;

		case STATE_CONNECTED:
			gIMMgrp->addSystemMessage(mSessionID, "connected", mNotifyArgs);
			break;

		case STATE_HUNG_UP:
			gIMMgrp->addSystemMessage(mSessionID, "hang_up", mNotifyArgs);
			break;

		default:
			break;
	}

	mState = state;
}

//static
void LLVoiceChannel::initClass()
{
	sCurrentVoiceChannel = LLVoiceChannelProximal::getInstance();
}

//static
void LLVoiceChannel::suspend()
{
	if (!sSuspended)
	{
		sSuspendedVoiceChannel = sCurrentVoiceChannel;
		sSuspended = true;
	}
}

//static
void LLVoiceChannel::resume()
{
	if (sSuspended)
	{
		if (gVoiceClient.voiceEnabled())
		{
			if (sSuspendedVoiceChannel)
			{
				sSuspendedVoiceChannel->activate();
			}
			else
			{
				LLVoiceChannelProximal::getInstance()->activate();
			}
		}
		sSuspended = false;
	}
}

//
// LLVoiceChannelGroup
//

LLVoiceChannelGroup::LLVoiceChannelGroup(const LLUUID& session_id,
										 const std::string& session_name)
:	LLVoiceChannel(session_id, session_name)
{
	mRetries = DEFAULT_RETRIES_COUNT;
	mIsRetrying = false;
}

void LLVoiceChannelGroup::deactivate()
{
	if (callStarted())
	{
		gVoiceClient.leaveNonSpatialChannel();
	}
	LLVoiceChannel::deactivate();
}

void LLVoiceChannelGroup::activate()
{
	if (callStarted()) return;

	LLVoiceChannel::activate();

	if (callStarted())
	{
		// we have the channel info, just need to use it now
		gVoiceClient.setNonSpatialChannel(mURI, mCredentials);
	}
}

//static
void LLVoiceChannelGroup::voiceCallCapCoro(const std::string& url,
										   LLUUID session_id)
{
	LLSD data;
	data["method"] = "call";
	data["session-id"] = session_id;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("voiceCallCapCoro");
	LLSD result = adapter.postAndSuspend(url, data);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);

	// Verify that the channel is still open on server reply, and bail if not.
	LLVoiceChannel* channelp = LLVoiceChannel::getChannelByID(session_id);
	if (!channelp)
	{
		llinfos << "Got reply for closed session Id: " << session_id
				<< ". Ignored." << llendl;
		return;
	}

	if (!status)
	{
		if (status == gStatusForbidden)
		{
			// 403 == no ability
			gNotifications.add("VoiceNotAllowed", channelp->getNotifyArgs());
		}
		else
		{
			gNotifications.add("VoiceCallGenericError",
							   channelp->getNotifyArgs());
		}
		channelp->deactivate();
		return;
	}

	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
	for (LLSD::map_const_iterator iter = result.beginMap(),
								  end = result.endMap();
		 iter != end; ++iter)
	{
		llinfos << "Got " << iter->first << llendl;
	}
	const LLSD& credentials = result["voice_credentials"];
	channelp->setChannelInfo(credentials["channel_uri"].asString(),
							 credentials["channel_credentials"].asString());
}

void LLVoiceChannelGroup::getChannelInfo()
{
	const std::string& url = gAgent.getRegionCapability("ChatSessionRequest");
	if (url.empty())
	{
		return;
	}
	gCoros.launch("LLVoiceChannelGroup::voiceCallCapCoro",
				  boost::bind(&LLVoiceChannelGroup::voiceCallCapCoro, url,
							  mSessionID));
}

void LLVoiceChannelGroup::setChannelInfo(const std::string& uri,
										 const std::string& credentials)
{
	setURI(uri);

	mCredentials = credentials;

	if (mState == STATE_NO_CHANNEL_INFO)
	{
		if (!mURI.empty() && !mCredentials.empty())
		{
			setState(STATE_READY);

			// If we are supposed to be active, reconnect. This will happen on
			// initial connect, as we request credentials on first use
			if (sCurrentVoiceChannel == this)
			{
				// just in case we got new channel info while active
				// should move over to new channel
				activate();
			}
		}
		else
		{
			// *TODO: notify the user
			llwarns << "Received invalid credentials for channel "
					<< mSessionName << llendl;
			deactivate();
		}
	}
	else if (mIsRetrying)
	{
		// We have the channel info, just need to use it now
		gVoiceClient.setNonSpatialChannel(mURI, mCredentials);
	}
}

void LLVoiceChannelGroup::handleStatusChange(EStatusType type)
{
	// Status updates
	if (type == STATUS_JOINED)
	{
		mRetries = 3;
		mIsRetrying = false;
	}

	LLVoiceChannel::handleStatusChange(type);
}

void LLVoiceChannelGroup::handleError(EStatusType status)
{
	std::string notify;
	switch (status)
	{
		case ERROR_CHANNEL_LOCKED:
		case ERROR_CHANNEL_FULL:
			notify = "VoiceChannelFull";
			break;

		case ERROR_NOT_AVAILABLE:
			// Clear URI and credentials, set the state to be no info and
			// activate
			if (mRetries > 0)
			{
				--mRetries;
				mIsRetrying = true;
				mIgnoreNextSessionLeave = true;
				getChannelInfo();
				return;
			}
			else
			{
				notify = "VoiceChannelJoinFailed";
				mRetries = DEFAULT_RETRIES_COUNT;
				mIsRetrying = false;
			}
			break;

		case ERROR_UNKNOWN:
		default:
			break;
	}

	// Notification
	if (!notify.empty())
	{
		LLNotificationPtr notification;
		notification = gNotifications.add(notify, mNotifyArgs);
		// Echo to IM window
		if (gIMMgrp)
		{
			gIMMgrp->addMessage(mSessionID, LLUUID::null, SYSTEM_FROM,
							    notification->getMessage());
		}
	}

	LLVoiceChannel::handleError(status);
}

void LLVoiceChannelGroup::setState(EState state)
{
	if (state == STATE_RINGING)
	{
		if (!mIsRetrying && gIMMgrp)
		{
			gIMMgrp->addSystemMessage(mSessionID, "ringing", mNotifyArgs);
		}
		mState = state;
	}
	else
	{
		LLVoiceChannel::setState(state);
	}
}

//
// LLVoiceChannelProximal
//
LLVoiceChannelProximal::LLVoiceChannelProximal()
:	LLVoiceChannel(LLUUID::null, LLStringUtil::null)
{
	activate();
}

bool LLVoiceChannelProximal::isActive()
{
	return callStarted() && gVoiceClient.inProximalChannel();
}

void LLVoiceChannelProximal::activate()
{
	if (callStarted()) return;

	LLVoiceChannel::activate();

	if (callStarted())
	{
		// this implicitly puts you back in the spatial channel
		gVoiceClient.leaveNonSpatialChannel();
	}
}

void LLVoiceChannelProximal::onChange(EStatusType type, const std::string&,
									  bool proximal)
{
	if (proximal)
	{
		if (type < BEGIN_ERROR_STATUS)
		{
			handleStatusChange(type);
		}
		else
		{
			handleError(type);
		}
	}
}

void LLVoiceChannelProximal::handleStatusChange(EStatusType status)
{
	// status updates
	switch (status)
	{
		case STATUS_LEFT_CHANNEL:
			// do not notify user when leaving proximal channel
			return;
		case STATUS_VOICE_DISABLED:
			if (gIMMgrp)
			{
				gIMMgrp->addSystemMessage(LLUUID::null, "unavailable",
										 mNotifyArgs);
			}
			return;
		default:
			break;
	}
	LLVoiceChannel::handleStatusChange(status);
}

void LLVoiceChannelProximal::handleError(EStatusType status)
{
	if (status == ERROR_CHANNEL_LOCKED || status == ERROR_CHANNEL_FULL)
	{
		gNotifications.add("ProximalVoiceChannelFull", mNotifyArgs);
	}

	LLVoiceChannel::handleError(status);
}

void LLVoiceChannelProximal::deactivate()
{
	if (callStarted())
	{
		setState(STATE_HUNG_UP);
	}
}

//
// LLVoiceChannelP2P
//
LLVoiceChannelP2P::LLVoiceChannelP2P(const LLUUID& session_id,
									 const std::string& session_name,
									 const LLUUID& other_user_id)
:	LLVoiceChannelGroup(session_id, session_name),
	mOtherUserID(other_user_id),
	mReceivedCall(false)
{
	// make sure URI reflects encoded version of other user's agent id
	setURI(gVoiceClient.sipURIFromID(other_user_id));
}

void LLVoiceChannelP2P::handleStatusChange(EStatusType type)
{
	// status updates
	if (type == STATUS_LEFT_CHANNEL)
	{
		if (callStarted() && !mIgnoreNextSessionLeave && !sSuspended)
		{
			if (mState == STATE_RINGING)
			{
				// other user declined call
				gNotifications.add("P2PCallDeclined", mNotifyArgs);
			}
			else
			{
				// other user hung up
				gNotifications.add("VoiceChannelDisconnectedP2P", mNotifyArgs);
			}
			deactivate();
		}
		mIgnoreNextSessionLeave = false;
		return;
	}

	LLVoiceChannel::handleStatusChange(type);
}

void LLVoiceChannelP2P::handleError(EStatusType type)
{
	if (type == ERROR_NOT_AVAILABLE)
	{
		gNotifications.add("P2PCallNoAnswer", mNotifyArgs);
	}

	LLVoiceChannel::handleError(type);
}

void LLVoiceChannelP2P::activate()
{
	if (callStarted()) return;

	LLVoiceChannel::activate();

	if (callStarted())
	{
		// No session handle yet, we're starting the call
		if (mSessionHandle.empty())
		{
			mReceivedCall = false;
			gVoiceClient.callUser(mOtherUserID);
		}
		// Otherwise answering the call
		else
		{
			gVoiceClient.answerInvite(mSessionHandle);

			// Using the session handle invalidates it.
			// Clear it out here so we can't reuse it by accident.
			mSessionHandle.clear();
		}
	}
}

void LLVoiceChannelP2P::getChannelInfo()
{
	// Pretend we have everything we need, since P2P does not use channel info.
	if (sCurrentVoiceChannel == this)
	{
		setState(STATE_CALL_STARTED);
	}
}

// Receiving session from other user who initiated call
void LLVoiceChannelP2P::setSessionHandle(const std::string& handle,
										 const std::string& in_uri)
{
	bool needs_activate = false;
	if (callStarted())
	{
		// defer to lower agent id when already active
		if (mOtherUserID < gAgentID)
		{
			// Pretend we have not started the call yet, so we can connect to
			// this session instead
			deactivate();
			needs_activate = true;
		}
		else
		{
			// We are active and have priority, invite the other user again
			// under the assumption they will join this new session
			mSessionHandle.clear();
			gVoiceClient.callUser(mOtherUserID);
			return;
		}
	}

	mSessionHandle = handle;

	// The URI of a P2P session should always be the other end SIP URI.
	if (!in_uri.empty())
	{
		setURI(in_uri);
	}
	else
	{
		setURI(gVoiceClient.sipURIFromID(mOtherUserID));
	}

	mReceivedCall = true;

	if (needs_activate)
	{
		activate();
	}
}

void LLVoiceChannelP2P::setState(EState state)
{
	// You only "answer" voice invites in P2P mode so provide a special purpose
	// message here
	if (mReceivedCall && state == STATE_RINGING)
	{
		if (gIMMgrp)
		{
			gIMMgrp->addSystemMessage(mSessionID, "answering", mNotifyArgs);
		}
		mState = state;
		return;
	}
	LLVoiceChannel::setState(state);
}
