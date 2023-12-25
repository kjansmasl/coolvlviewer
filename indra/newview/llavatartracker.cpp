/**
 * @file llavatartracker.cpp
 * @brief Implementation of the LLAvatarTracker and associated classes
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

#if LL_WINDOWS
# pragma warning(disable : 4800)	// Performance warning in <functional>
#endif

#include "llavatartracker.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llnotifications.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfloaterim.h"
#include "llimmgr.h"
#include "llinventorymodel.h"
#include "llgridmanager.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

class LLTrackingData
{
public:
	LLTrackingData(const LLUUID& avatar_id, const std::string& name);
	bool haveTrackingInfo();
	void setTrackedCoarseLocation(const LLVector3d& global_pos);
	void agentFound(const LLUUID& prey,
					const LLVector3d& estimated_global_pos);

public:
	LLUUID		mAvatarID;
	std::string	mName;
	LLVector3d	mGlobalPositionEstimate;
	LLTimer		mCoarseLocationTimer;
	LLTimer		mUpdateTimer;
	LLTimer		mAgentGone;
	bool		mHaveInfo;
	bool		mHaveCoarseInfo;
};

constexpr F32 COARSE_FREQUENCY = 2.2f;
// This results in a database query, so cut these back:
constexpr F32 FIND_FREQUENCY = 29.7f;
constexpr F32 OFFLINE_SECONDS = FIND_FREQUENCY + 8.f;

LLAvatarTracker gAvatarTracker;

///----------------------------------------------------------------------------
/// Class LLAvatarTracker
///----------------------------------------------------------------------------

LLAvatarTracker::LLAvatarTracker()
:	mTrackingData(NULL),
	mTrackedAgentValid(false),
	mModifyMask(0x0),
	mIsNotifyObservers(false)
{
}

LLAvatarTracker::~LLAvatarTracker()
{
	deleteTrackingData();

	std::for_each(mObservers.begin(), mObservers.end(), DeletePointer());
	mObservers.clear();

	for (auto it = mBuddyInfo.begin(), end = mBuddyInfo.end(); it != end; ++it)
	{
		delete it->second;
	}
	mBuddyInfo.clear();
}

void LLAvatarTracker::track(const LLUUID& avatar_id, const std::string& name)
{
	deleteTrackingData();
	mTrackedAgentValid = false;
	mTrackingData = new LLTrackingData(avatar_id, name);
	findAgent();

	// We track here because findAgent() is called on a timer (for now).
	if (avatar_id.notNull())
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_TrackAgent);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_TargetData);
		msg->addUUIDFast(_PREHASH_PreyID, avatar_id);
		gAgent.sendReliableMessage();
	}
}

void LLAvatarTracker::untrack(const LLUUID& avatar_id)
{
	if (mTrackingData && mTrackingData->mAvatarID == avatar_id)
	{
		deleteTrackingData();
		mTrackedAgentValid = false;
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_TrackAgent);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_TargetData);
		msg->addUUIDFast(_PREHASH_PreyID, LLUUID::null);
		gAgent.sendReliableMessage();
	}
}

void LLAvatarTracker::setTrackedCoarseLocation(const LLVector3d& global_pos)
{
	if (mTrackingData)
	{
		mTrackingData->setTrackedCoarseLocation(global_pos);
	}
}

bool LLAvatarTracker::haveTrackingInfo()
{
	if (mTrackingData)
	{
		return mTrackingData->haveTrackingInfo();
	}
	return false;
}

LLVector3d LLAvatarTracker::getGlobalPos()
{
	if (!mTrackedAgentValid || !mTrackingData) return LLVector3d();
	LLVector3d global_pos;

	LLVOAvatar* avatarp = gObjectList.findAvatar(mTrackingData->mAvatarID);
	if (avatarp && !avatarp->isDead())
	{
		global_pos = avatarp->getPositionGlobal();
		// *HACK: for making the tracker point above the avatar's head rather
		// than to its groin
		global_pos.mdV[VZ] += 0.7f * (avatarp->mBodySize.mV[VZ] +
									  avatarp->mAvatarOffset.mV[VZ]);
		mTrackingData->mGlobalPositionEstimate = global_pos;
	}
	else
	{
		global_pos = mTrackingData->mGlobalPositionEstimate;
	}

	return global_pos;
}

void LLAvatarTracker::getDegreesAndDist(F32& rot, F64& horiz_dist,
										F64& vert_dist)
{
	if (!mTrackingData) return;

	LLVector3d global_pos;

	LLVOAvatar* avatarp = gObjectList.findAvatar(mTrackingData->mAvatarID);
	if (avatarp && !avatarp->isDead())
	{
		global_pos = avatarp->getPositionGlobal();
		mTrackingData->mGlobalPositionEstimate = global_pos;
	}
	else
	{
		global_pos = mTrackingData->mGlobalPositionEstimate;
	}
	LLVector3d to_vec = global_pos - gAgent.getPositionGlobal();
	horiz_dist = sqrtf(to_vec.mdV[VX] * to_vec.mdV[VX] +
					   to_vec.mdV[VY] * to_vec.mdV[VY]);
	vert_dist = to_vec.mdV[VZ];
	rot = RAD_TO_DEG * atan2f(to_vec.mdV[VY], to_vec.mdV[VX]);
}

const std::string& LLAvatarTracker::getName()
{
	return mTrackingData ? mTrackingData->mName : LLStringUtil::null;
}

const LLUUID& LLAvatarTracker::getAvatarID()
{
	return mTrackingData ? mTrackingData->mAvatarID : LLUUID::null;
}

S32 LLAvatarTracker::addBuddyList(const LLAvatarTracker::buddy_map_t& buds)
{
	U32 new_buddy_count = 0;
	std::string first, last;
	LLUUID agent_id;
	for (buddy_map_t::const_iterator itr = buds.begin(); itr != buds.end();
		 ++itr)
	{
		agent_id = itr->first;
		buddy_map_t::const_iterator existing_buddy = mBuddyInfo.find(agent_id);
		if (existing_buddy == mBuddyInfo.end())
		{
			++new_buddy_count;
			mBuddyInfo[agent_id] = itr->second;
			if (gCacheNamep)
			{
				gCacheNamep->getName(agent_id, first, last);
			}
			addChangedMask(LLFriendObserver::ADD, agent_id);
			LL_DEBUGS("AvatarTracker") << "Added buddy " << agent_id << ", "
									   << (mBuddyInfo[agent_id]->isOnline() ? "Online"
																			: "Offline")
									   << ", TO: "
									   << mBuddyInfo[agent_id]->getRightsGrantedTo()
									   << ", FROM: "
									   << mBuddyInfo[agent_id]->getRightsGrantedFrom()
									   << LL_ENDL;
		}
		else
		{
			LLRelationship* e_r = existing_buddy->second;
			LLRelationship* n_r = itr->second;
			llwarns << "Add buddy for existing buddy: " << agent_id
					<< " [" << (e_r->isOnline() ? "Online" : "Offline")
					<< "->" << (n_r->isOnline() ? "Online" : "Offline")
					<< ", " <<  e_r->getRightsGrantedTo()
					<< "->" << n_r->getRightsGrantedTo()
					<< ", " <<  e_r->getRightsGrantedTo()
					<< "->" << n_r->getRightsGrantedTo() << "]" << llendl;
		}
	}
	// Do not notify observers here: list can be large so let it be done on
	// idle instead (via a call to gAvatarTracker.idleNotifyObservers() in
	// llappviewer.cpp)

	return new_buddy_count;
}

void LLAvatarTracker::copyBuddyList(buddy_map_t& buddies) const
{
	for (buddy_map_t::const_iterator it = mBuddyInfo.begin(),
									 end = mBuddyInfo.end();
		 it != end; ++it)
	{
		buddies[it->first] = it->second;
	}
}

void LLAvatarTracker::terminateBuddy(const LLUUID& id)
{
	LL_DEBUGS("AvatarTracker") << "Terminating friendship with avatar Id: "
							   << id << LL_ENDL;
	LLRelationship* buddy = get_ptr_in_map(mBuddyInfo, id);
	if (buddy)
	{
		mBuddyInfo.erase(id);
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage("TerminateFriendship");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgentID);
		msg->addUUID("SessionID", gAgentSessionID);
		msg->nextBlock("ExBlock");
		msg->addUUID("OtherID", id);
		gAgent.sendReliableMessage();

		addChangedMask(LLFriendObserver::REMOVE, id);
		delete buddy;

		LLVOAvatar* avatarp = gObjectList.findAvatar(id);
		if (avatarp)
		{
			static LLCachedControl<LLColor4U> map_avatar(gColors, "MapAvatar");
			avatarp->setMinimapColor(LLColor4(map_avatar));
		}
	}
}

// Get all buddy info
const LLRelationship* LLAvatarTracker::getBuddyInfo(const LLUUID& id) const
{
	if (id.isNull())
	{
		return NULL;
	}
	return get_ptr_in_map(mBuddyInfo, id);
}

// Online status
void LLAvatarTracker::setBuddyOnline(const LLUUID& id, bool is_online)
{
	LLRelationship* info = get_ptr_in_map(mBuddyInfo, id);
	if (!info)
	{
		llwarns << "No buddy info found for " << id << ", setting to "
				<< (is_online ? "online" : "offline") << llendl;
		return;
	}

	info->online(is_online);
	addChangedMask(LLFriendObserver::ONLINE, id);
	LL_DEBUGS("AvatarTracker") << "Set buddy " << id
							   << (is_online ? " online" : " offline")
							   << LL_ENDL;
}

bool LLAvatarTracker::isBuddyOnline(const LLUUID& id) const
{
	LLRelationship* info = get_ptr_in_map(mBuddyInfo, id);
	return info && info->isOnline();
}

#if TRACK_POWER
// empowered status
void LLAvatarTracker::setBuddyEmpowered(const LLUUID& id, bool is_empowered)
{
	LLRelationship* info = get_ptr_in_map(mBuddyInfo, id);
	if (info)
	{
		info->grantRights(LLRelationship::GRANT_MODIFY_OBJECTS, 0);
		addChangedMask(LLFriendObserver::POWERS, id);
	}
}

bool LLAvatarTracker::isBuddyEmpowered(const LLUUID& id) const
{
	LLRelationship* info = get_ptr_in_map(mBuddyInfo, id);
	if (info)
	{
		return info->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS);
	}
	return false;
}

// Wrapper for ease of use in some situations.
void LLAvatarTracker::empower(const LLUUID& id, bool grant)
{
	buddy_map_t list;
	list.insert(id);
	empowerList(list, grant);
}

void LLAvatarTracker::empowerList(const buddy_map_t& list, bool grant)
{
	const char* message_name;
	const char* block_name;
	const char* field_name;
	if (grant)
	{
		message_name = _PREHASH_GrantModification;
		block_name = _PREHASH_EmpoweredBlock;
		field_name = _PREHASH_EmpoweredID;
	}
	else
	{
		message_name = _PREHASH_RevokeModification;
		block_name = _PREHASH_RevokedBlock;
		field_name = _PREHASH_RevokedID;
	}

	std::string name;
	gAgent.buildFullnameAndTitle(name);

	LLMessageSystem* msg = gMessageSystemp;
	bool start_new_message = true;
	buddy_list_t::const_iterator it = list.begin();
	buddy_list_t::const_iterator end = list.end();
	for ( ; it != end; ++it)
	{
		if (!get_ptr_in_map(mBuddyInfo, *it)) continue;
		setBuddyEmpowered((*it), grant);
		if (start_new_message)
		{
			start_new_message = false;
			msg->newMessageFast(message_name);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->addStringFast(_PREHASH_GranterName, name);
		}
		msg->nextBlockFast(block_name);
		msg->addUUIDFast(field_name, (*it));
		if (msg->isSendFullFast(block_name))
		{
			start_new_message = true;
			gAgent.sendReliableMessage();
		}
	}
	if (!start_new_message)
	{
		gAgent.sendReliableMessage();
	}
}
#endif

void LLAvatarTracker::deleteTrackingData()
{
	if (mTrackingData)
	{
		delete mTrackingData;
		// Make sure mTrackingData never points to freed memory
		mTrackingData = NULL;
	}
}

void LLAvatarTracker::findAgent()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg || !mTrackingData || mTrackingData->mAvatarID.isNull())
	{
		return;
	}

	msg->newMessageFast(_PREHASH_FindAgent); // Request
	msg->nextBlockFast(_PREHASH_AgentBlock);
	msg->addUUIDFast(_PREHASH_Hunter, gAgentID);
	msg->addUUIDFast(_PREHASH_Prey, mTrackingData->mAvatarID);
	msg->addIPAddrFast(_PREHASH_SpaceIP, 0); // will get filled in by simulator
	msg->nextBlockFast(_PREHASH_LocationBlock);
	constexpr F64 NO_LOCATION = 0.0;
	msg->addF64Fast(_PREHASH_GlobalX, NO_LOCATION);
	msg->addF64Fast(_PREHASH_GlobalY, NO_LOCATION);
	gAgent.sendReliableMessage();
}

void LLAvatarTracker::addObserver(LLFriendObserver* observer)
{
	if (observer)
	{
		mObservers.push_back(observer);
	}
}

void LLAvatarTracker::removeObserver(LLFriendObserver* observer)
{
	mObservers.erase(std::remove(mObservers.begin(), mObservers.end(),
								 observer),
					 mObservers.end());
}

void LLAvatarTracker::idleNotifyObservers()
{
	if (mModifyMask != LLFriendObserver::NONE)
	{
		notifyObservers();
	}
}

void LLAvatarTracker::notifyObservers()
{
	if (mIsNotifyObservers)
	{
		// Do not allow recursive calls; new masks and Ids will be processed
		// later from idle.
		return;
	}
	mIsNotifyObservers = true;

	LL_DEBUGS("Friends") << "Notifying observers with mask=" << mModifyMask
						 << LL_ENDL;

	// Copy the list, in case an observer would remove itself on changed event
	observer_list_t observers(mObservers);

	for (observer_list_t::iterator it = observers.begin(),
								   end = observers.end();
		 it != end; ++it)
	{
		LLFriendObserver* observer = *it;
		if (observer)
		{
			observer->changed(mModifyMask);
			observer->changedBuddies(mChangedBuddyIDs);
		}
	}

	mModifyMask = LLFriendObserver::NONE;
	mChangedBuddyIDs.clear();

	mIsNotifyObservers = false;
}

void LLAvatarTracker::addChangedMask(U32 mask, const LLUUID& buddy_id)
{
	mModifyMask |= mask; 
	if (buddy_id.notNull())
	{
		mChangedBuddyIDs.emplace(buddy_id);
	}
}

void LLAvatarTracker::applyFunctor(LLRelationshipFunctor& f)
{
	for (buddy_map_t::iterator it = mBuddyInfo.begin(), end = mBuddyInfo.end();
		 it != end; ++it)
	{
		f(it->first, it->second);
	}
}

void LLAvatarTracker::registerCallbacks(LLMessageSystem* msg)
{
	msg->setHandlerFuncFast(_PREHASH_FindAgent, processAgentFound);
	msg->setHandlerFuncFast(_PREHASH_OnlineNotification,
							processOnlineNotification);
	msg->setHandlerFuncFast(_PREHASH_OfflineNotification,
							processOfflineNotification);
	msg->setHandlerFunc(_PREHASH_TerminateFriendship,
						processTerminateFriendship);
	msg->setHandlerFunc(_PREHASH_ChangeUserRights, processChangeUserRights);
}

//static
void LLAvatarTracker::processAgentFound(LLMessageSystem* msg, void**)
{
	LLUUID id;
	msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_Hunter, id);
	msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_Prey, id);
	// *FIX: should make sure prey id matches.
	LLVector3d estimated_global_pos;
	msg->getF64Fast(_PREHASH_LocationBlock, _PREHASH_GlobalX,
					estimated_global_pos.mdV[VX]);
	msg->getF64Fast(_PREHASH_LocationBlock, _PREHASH_GlobalY,
					estimated_global_pos.mdV[VY]);
	gAvatarTracker.agentFound(id, estimated_global_pos);
}

void LLAvatarTracker::agentFound(const LLUUID& prey,
								 const LLVector3d& estimated_global_pos)
{
	// If we get a valid reply from the server, that means the agent is our
	// friend and mappable, so enable interest list based updates
	if (mTrackingData)
	{
		gAvatarTracker.setTrackedAgentValid(true);
		mTrackingData->agentFound(prey, estimated_global_pos);
	}
}

//static
void LLAvatarTracker::processOnlineNotification(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("AvatarTracker") << "called" << LL_ENDL;
	gAvatarTracker.processNotify(msg, true);
}

//static
void LLAvatarTracker::processOfflineNotification(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("AvatarTracker") << "called" << LL_ENDL;
	gAvatarTracker.processNotify(msg, false);
}

void LLAvatarTracker::processChange(LLMessageSystem* msg)
{
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_Rights);
	LLUUID agent_id, agent_related;
	S32 new_rights;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_Rights, _PREHASH_AgentRelated, agent_related,
						 i);
		msg->getS32Fast(_PREHASH_Rights,_PREHASH_RelatedRights, new_rights, i);
		if (agent_id == gAgentID)
		{
			if (mBuddyInfo.find(agent_related) != mBuddyInfo.end())
			{
				(mBuddyInfo[agent_related])->setRightsTo(new_rights);
			}
		}
		else if (mBuddyInfo.find(agent_id) != mBuddyInfo.end())
		{
			if (((mBuddyInfo[agent_id]->getRightsGrantedFrom() ^ new_rights) &
				 LLRelationship::GRANT_MODIFY_OBJECTS) && !gAgent.getBusy())
			{
				std::string name;
				LLSD args;
				if (gCacheNamep && gCacheNamep->getFullName(agent_id, name))
				{
					if (!LLAvatarName::sLegacyNamesForFriends &&
						LLAvatarNameCache::useDisplayNames())
					{
						LLAvatarName avatar_name;
						if (LLAvatarNameCache::get(agent_id, &avatar_name))
						{
							// Always show "Display Name [Legacy Name]" for
							// security reasons
							name = avatar_name.getNames();
						}
					}
					args["NAME"] = name;
				}
				if (LLRelationship::GRANT_MODIFY_OBJECTS & new_rights)
				{
					gNotifications.add("GrantedModifyRights", args);
				}
				else
				{
					gNotifications.add("RevokedModifyRights", args);
				}
			}
			(mBuddyInfo[agent_id])->setRightsFrom(new_rights);
		}
	}
	addChangedMask(LLFriendObserver::POWERS, agent_id);

	notifyObservers();
}

void LLAvatarTracker::processChangeUserRights(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("AvatarTracker") << "called" << LL_ENDL;
	gAvatarTracker.processChange(msg);
}

//static
void LLAvatarTracker::callbackLoadAvatarName(const LLUUID& id, bool online,
											 const LLAvatarName& avatar_name)
{
	std::string name;
	if (!LLAvatarName::sLegacyNamesForFriends &&
		LLAvatarNameCache::useDisplayNames())
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
	else
	{
		name = avatar_name.getLegacyName();
	}

	// Popup a notify box with online status of this agent
	LLSD args;
	args["NAME"] = name;
	LLNotificationPtr n = gNotifications.add(online ? "FriendOnline"
													: "FriendOffline", args);

	// If there is an open IM session with this agent, send a notification
	// there too.
	LLUUID session_id = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, id);
	LLFloaterIMSession* floater = LLFloaterIMSession::findInstance(session_id);
	if (floater)
	{
		std::string mesg = n->getMessage();
		if (!mesg.empty())
		{
			floater->addHistoryLine(mesg,
									gSavedSettings.getColor4("SystemChatColor"));
		}
	}
}

void LLAvatarTracker::processNotify(LLMessageSystem* msg, bool online)
{
	static LLCachedControl<bool> chat_notify(gSavedSettings,
											 "ChatOnlineNotification");

	S32 count = msg->getNumberOfBlocksFast(_PREHASH_AgentBlock);
	LL_DEBUGS("AvatarTracker") << "Received " << count
							   << " online notifications **** " << LL_ENDL;
	if (count <= 0)
	{
		return;
	}

	LLUUID tracking_id;
	if (mTrackingData)
	{
		tracking_id = mTrackingData->mAvatarID;
	}

	LLUUID agent_id;
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_AgentID, agent_id, i);
		const LLRelationship* info = getBuddyInfo(agent_id);
		if (info)
		{
			setBuddyOnline(agent_id, online);
			if (chat_notify)
			{
				LLAvatarNameCache::get(agent_id,
									   boost::bind(&callbackLoadAvatarName,
												   _1, online, _2));
			}
		}
		else
		{
			llwarns << "Received online notification for unknown buddy: "
					<< agent_id << " is "
					<< (online ? "online" : "offline") << llendl;
		}

		if (tracking_id == agent_id)
		{
			// We were tracking someone who went offline
			deleteTrackingData();
		}

		addChangedMask(LLFriendObserver::ONLINE, agent_id);

		// *TODO: get actual inventory Id
		gInventory.addChangedMask(LLInventoryObserver::CALLING_CARD,
								  LLUUID::null);
	}
	gAvatarTracker.notifyObservers();
	gInventory.notifyObservers();
}

//static
void LLAvatarTracker::formFriendship(const LLUUID& id)
{
	if (id.notNull())
	{
		LLRelationship* buddy_info = get_ptr_in_map(gAvatarTracker.mBuddyInfo,
													id);
		if (!buddy_info)
		{
			// The default for relationship establishment is to have both
			// parties visible online to each other.
			buddy_info = new LLRelationship(LLRelationship::GRANT_ONLINE_STATUS,
											LLRelationship::GRANT_ONLINE_STATUS,
											false);
			gAvatarTracker.mBuddyInfo[id] = buddy_info;
			gAvatarTracker.addChangedMask(LLFriendObserver::ADD, id);
			gAvatarTracker.notifyObservers();
		}
		LLVOAvatar* avatarp = gObjectList.findAvatar(id);
		if (avatarp)
		{
			static LLCachedControl<LLColor4U> map_friend(gColors, "MapFriend");
			avatarp->setMinimapColor(LLColor4(map_friend));
		}
	}
}

//static
bool LLAvatarTracker::isAgentFriend(const LLUUID& agent_id)
{
	return gAvatarTracker.isBuddy(agent_id);
}

//static
bool LLAvatarTracker::isAgentMappable(const LLUUID& agent_id)
{
	const LLRelationship* buddy = gAvatarTracker.getBuddyInfo(agent_id);
	return buddy && buddy->isOnline() &&
		   buddy->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION);
}

//static
void LLAvatarTracker::processTerminateFriendship(LLMessageSystem* msg, void**)
{
	LLUUID id;
	msg->getUUID("ExBlock", "OtherID", id);
	if (id.isNull())
	{
		return;
	}

	LLRelationship* buddy = get_ptr_in_map(gAvatarTracker.mBuddyInfo, id);
	if (buddy)
	{
		gAvatarTracker.mBuddyInfo.erase(id);
		gAvatarTracker.addChangedMask(LLFriendObserver::REMOVE, id);
		delete buddy;
		gAvatarTracker.notifyObservers();
	}

	LLVOAvatar* avatarp = gObjectList.findAvatar(id);
	if (avatarp)
	{
		static LLCachedControl<LLColor4U> map_avatar(gColors, "MapAvatar");
		avatarp->setMinimapColor(LLColor4(map_avatar));
	}
}

void LLAvatarTracker::dirtyBuddies()
{
	mModifyMask |= LLFriendObserver::REMOVE | LLFriendObserver::ADD;
	notifyObservers();
}

///----------------------------------------------------------------------------
/// Tracking Data
///----------------------------------------------------------------------------

LLTrackingData::LLTrackingData(const LLUUID& avatar_id,
							   const std::string& name)
:	mAvatarID(avatar_id),
	mHaveInfo(false),
	mHaveCoarseInfo(false)
{
	mCoarseLocationTimer.setTimerExpirySec(COARSE_FREQUENCY);
	mUpdateTimer.setTimerExpirySec(FIND_FREQUENCY);
	mAgentGone.setTimerExpirySec(OFFLINE_SECONDS);
	if (!name.empty())
	{
		mName = name;
	}
}

void LLTrackingData::agentFound(const LLUUID& prey,
								const LLVector3d& estimated_global_pos)
{
	if (prey != mAvatarID)
	{
		llwarns << "LLTrackingData::agentFound() - found " << prey
				<< " but looking for " << mAvatarID << llendl;
	}
	mHaveInfo = true;
	mAgentGone.setTimerExpirySec(OFFLINE_SECONDS);
	mGlobalPositionEstimate = estimated_global_pos;
}

bool LLTrackingData::haveTrackingInfo()
{
	LLVOAvatar* avatarp = gObjectList.findAvatar(mAvatarID);
	if (avatarp && !avatarp->isDead())
	{
		mCoarseLocationTimer.checkExpirationAndReset(COARSE_FREQUENCY);
		mUpdateTimer.setTimerExpirySec(FIND_FREQUENCY);
		mAgentGone.setTimerExpirySec(OFFLINE_SECONDS);
		mHaveInfo = true;
		return true;
	}
	if (mHaveCoarseInfo &&
	    !mCoarseLocationTimer.checkExpirationAndReset(COARSE_FREQUENCY))
	{
		// if we reach here, then we have a 'recent' coarse update
		mUpdateTimer.setTimerExpirySec(FIND_FREQUENCY);
		mAgentGone.setTimerExpirySec(OFFLINE_SECONDS);
		return true;
	}
	if (mUpdateTimer.checkExpirationAndReset(FIND_FREQUENCY))
	{
		gAvatarTracker.findAgent();
		mHaveCoarseInfo = false;
	}
	if (mAgentGone.checkExpirationAndReset(OFFLINE_SECONDS))
	{
		mHaveInfo = false;
		mHaveCoarseInfo = false;
	}
	return mHaveInfo;
}

void LLTrackingData::setTrackedCoarseLocation(const LLVector3d& global_pos)
{
	mCoarseLocationTimer.setTimerExpirySec(COARSE_FREQUENCY);
	mGlobalPositionEstimate = global_pos;
	mHaveInfo = true;
	mHaveCoarseInfo = true;
}

///----------------------------------------------------------------------------
// various buddy functors
///----------------------------------------------------------------------------

bool LLCollectProxyBuddies::operator()(const LLUUID& buddy_id,
									   LLRelationship* buddy)
{
	if (buddy->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS))
	{
		mProxy.emplace(buddy_id);
	}
	return true;
}

bool LLCollectMappableBuddies::operator()(const LLUUID& buddy_id,
										  LLRelationship* buddy)
{
	if (gCacheNamep)
	{
		gCacheNamep->getName(buddy_id, mFirst, mLast);
	}
	std::ostringstream fullname;
	fullname << mFirst << " " << mLast;
	buddy_map_t::value_type value(fullname.str(), buddy_id);
	if (buddy->isOnline() &&
		buddy->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION))
	{
		mMappable.emplace(value);
	}
	return true;
}

bool LLCollectOnlineBuddies::operator()(const LLUUID& buddy_id,
										LLRelationship* buddy)
{
	if (gCacheNamep)
	{
		gCacheNamep->getName(buddy_id, mFirst, mLast);
	}
	std::ostringstream fullname;
	fullname << mFirst << " " << mLast;
	buddy_map_t::value_type value(fullname.str(), buddy_id);
	if (buddy->isOnline())
	{
		mOnline.emplace(value);
	}
	return true;
}

bool LLCollectAllBuddies::operator()(const LLUUID& buddy_id,
									 LLRelationship* buddy)
{
	if (gCacheNamep)
	{
		gCacheNamep->getName(buddy_id, mFirst, mLast);
	}
	std::ostringstream fullname;
	fullname << mFirst << " " << mLast;
	buddy_map_t::value_type value(fullname.str(), buddy_id);
	if (buddy->isOnline())
	{
		mOnline.emplace(value);
	}
	else
	{
		mOffline.emplace(value);
	}
	return true;
}
