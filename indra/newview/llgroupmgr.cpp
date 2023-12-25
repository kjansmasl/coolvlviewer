/**
 * @file llgroupmgr.cpp
 * @brief LLGroupMgr class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

/**
 * Manager for aggregating all client knowledge for specific groups
 * Keeps a cache of group information.
 */

#include "llviewerprecompiledheaders.h"

#include "llgroupmgr.h"

#include "llcorehttputil.h"
#include "lleconomy.h"
#include "llinstantmessage.h"
#include "llnotifications.h"
#include "lltransactiontypes.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "llfloatergroupinfo.h"
#include "hbfloatersearch.h"
#include "llstartup.h"				// For gMaxAgentGroups
#include "llstatusbar.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For send_improved_im()

LLGroupMgr gGroupMgr;

// Was 32, but we can now pertain to 70 groups in SL, so... HB
constexpr U32 MAX_CACHED_GROUPS = 72;

///////////////////////////////////////////////////////////////////////////////
// Group invitation callback (was formerly in llviewermessage.cpp).
///////////////////////////////////////////////////////////////////////////////

void join_group_response_coro(const std::string& url, LLUUID group_id,
							  bool accepted_invite)
{
	LLSD payload;
	payload["group"] = group_id;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("GroupInvitationResponse");
	LLSD result = adapter.postAndSuspend(url, payload);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result.has("success") || !result["success"].asBoolean())
	{
		llwarns << "Error responding via capability to invitation to group: "
				<< group_id << ". Error: " << status.toString() << llendl;
		// *TODO: implement UDP fallback ?
		return;
	}
	if (accepted_invite)
	{
		// Refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		gGroupMgr.clearGroupData(group_id);
		// Refresh the floater for this group, if any.
		LLFloaterGroupInfo::refreshGroup(group_id);
		// Refresh the group panel of the search window, if necessary.
		HBFloaterSearch::refreshGroup(group_id);
	}
}

bool join_group_response(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	bool accept_invite = false;

	const LLSD& payload = notification["payload"];
	LLUUID group_id = payload["group_id"].asUUID();
	LLUUID transaction_id = payload["transaction_id"].asUUID();
	std::string name = payload["name"].asString();
	std::string message = payload["message"].asString();
	S32 fee = payload["fee"].asInteger();
	bool use_cap = payload.has("use_offline_cap") &&
				   payload["use_offline_cap"].asBoolean();

	if (option == 2 && group_id.notNull())
	{
		LLFloaterGroupInfo::showFromUUID(group_id);
		LLSD args;
		args["MESSAGE"] = message;
		gNotifications.add("JoinGroup", args, payload);
		return false;
	}
	if (option == 0 && group_id.notNull())
	{
		// Check for promotion or demotion.
		S32 max_groups = gMaxAgentGroups;
		if (gAgent.isInGroup(group_id))
		{
			++max_groups;
		}

		if ((S32)gAgent.mGroups.size() < max_groups)
		{
			accept_invite = true;
		}
		else
		{
			LLSD args;
			args["NAME"] = name;
			args["INVITE"] = message;
			gNotifications.add("JoinedTooManyGroupsMember", args, payload);
		}
	}

	if (accept_invite && fee > 0)
	{
		// If there is a fee to join this group, make sure the user does want
		// to join.
		LLSD args;
		args["COST"] = llformat("%d", fee);
		// Set the fee for next time to 0, so that we do not keep asking about
		// a fee.
		LLSD next_payload = notification["payload"];
		next_payload["fee"] = 0;
		gNotifications.add("JoinGroupCanAfford", args, next_payload);
	}
	else if (use_cap)
	{
		const std::string& url =
			gAgent.getRegionCapability(accept_invite ? "AcceptGroupInvite"
													 : "DeclineGroupInvite");
		if (url.empty())
		{
			llwarns << "Missing capability, cannot reply to offline group invitation to group: "
					<< group_id << llendl;
			return false;
		}
		gCoros.launch("groupInvitationResponse",
					  boost::bind(&join_group_response_coro, url, group_id,
								  accept_invite));
	}
	else
	{
		send_improved_im(group_id, "name", "message", IM_ONLINE,
						 accept_invite ? IM_GROUP_INVITATION_ACCEPT
									   :IM_GROUP_INVITATION_DECLINE,
						 transaction_id);
	}

	return false;
}
static LLNotificationFunctorRegistration jgr_1("JoinGroup",
											   join_group_response);
static LLNotificationFunctorRegistration jgr_2("JoinedTooManyGroupsMember",
											   join_group_response);
static LLNotificationFunctorRegistration jgr_3("JoinGroupCanAfford",
											   join_group_response);

//
// LLRoleActionSet
//
LLRoleActionSet::LLRoleActionSet()
:	mActionSetData(NULL)
{
}

LLRoleActionSet::~LLRoleActionSet()
{
	delete mActionSetData;
	std::for_each(mActions.begin(), mActions.end(), DeletePointer());
	mActions.clear();
}

//
// LLGroupMemberData
//

LLGroupMemberData::LLGroupMemberData(const LLUUID& id,
									 S32 contribution,
									 U64 agent_powers,
									 const std::string& title,
									 const std::string& online_status,
									 bool is_owner)
:	mID(id),
	mContribution(contribution),
	mAgentPowers(agent_powers),
	mTitle(title),
	mOnlineStatus(online_status),
	mIsOwner(is_owner)
{
}

void LLGroupMemberData::addRole(const LLUUID& role, LLGroupRoleData* rd)
{
	mRolesList[role] = rd;
}

bool LLGroupMemberData::removeRole(const LLUUID& role)
{
	role_list_t::iterator it = mRolesList.find(role);
	if (it != mRolesList.end())
	{
		mRolesList.hmap_erase(it);
		return true;
	}

	return false;
}

//
// LLGroupRoleData
//

LLGroupRoleData::LLGroupRoleData(const LLUUID& role_id,
								 const std::string& role_name,
								 const std::string& role_title,
								 const std::string& role_desc,
								 U64 role_powers, S32 member_count)
:	mRoleID(role_id),
	mMemberCount(member_count),
	mMembersNeedsSort(false)
{
	mRoleData.mRoleName = role_name;
	mRoleData.mRoleTitle = role_title;
	mRoleData.mRoleDescription = role_desc;
	mRoleData.mRolePowers = role_powers;
	mRoleData.mChangeType = RC_UPDATE_NONE;
}

LLGroupRoleData::LLGroupRoleData(const LLUUID& role_id, LLRoleData role_data,
								 S32 member_count)
:	mRoleID(role_id),
	mRoleData(role_data),
	mMemberCount(member_count),
	mMembersNeedsSort(false)
{
}

S32 LLGroupRoleData::getMembersInRole(uuid_vec_t members, bool needs_sort)
{
	if (mRoleID.isNull())
	{
		// This is the everyone role, just return the size of members,
		// because everyone is in the everyone role.
		return members.size();
	}

	// Sort the members list, if needed.
	if (mMembersNeedsSort)
	{
		std::sort(mMemberIDs.begin(), mMemberIDs.end());
		mMembersNeedsSort = false;
	}
	if (needs_sort)
	{
		// Sort the members parameter.
		std::sort(members.begin(), members.end());
	}

	// Return the number of members in the intersection.
	S32 max_size = llmin(members.size(), mMemberIDs.size());
	uuid_vec_t in_role(max_size);
	uuid_vec_t::iterator in_role_end;
	in_role_end = std::set_intersection(mMemberIDs.begin(), mMemberIDs.end(),
										members.begin(), members.end(),
										in_role.begin());
	return in_role_end - in_role.begin();
}

void LLGroupRoleData::addMember(const LLUUID& member)
{
	mMembersNeedsSort = true;
	mMemberIDs.emplace_back(member);
}

bool LLGroupRoleData::removeMember(const LLUUID& member)
{
	uuid_vec_t::iterator it = std::find(mMemberIDs.begin(), mMemberIDs.end(),
										member);
	if (it != mMemberIDs.end())
	{
		mMembersNeedsSort = true;
		mMemberIDs.erase(it);
		return true;
	}

	return false;
}

void LLGroupRoleData::clearMembers()
{
	mMembersNeedsSort = false;
	mMemberIDs.clear();
}

//
// LLGroupMgrGroupData
//

LLGroupMgrGroupData::LLGroupMgrGroupData(const LLUUID& id)
:	mID(id),
	mShowInList(true),
	mOpenEnrollment(false),
	mMembershipFee(0),
	mAllowPublish(false),
	mListInProfile(false),
	mMaturePublish(false),
	mChanged(false),
	mMemberCount(0),
	mRoleCount(0),
	mReceivedRoleMemberPairs(0),
	mMemberDataComplete(false),
	mRoleDataComplete(false),
	mRoleMemberDataComplete(false),
	mGroupPropertiesDataComplete(false),
	mPendingRoleMemberRequest(false),
	mAccessTime(0.0f)
{
}

void LLGroupMgrGroupData::setAccessed()
{
	mAccessTime = (F32)LLFrameTimer::getTotalSeconds();
}

bool LLGroupMgrGroupData::getRoleData(const LLUUID& role_id,
									  LLRoleData& role_data)
{
	// Do we have changes for it ?
	role_data_map_t::const_iterator it = mRoleChanges.find(role_id);
	if (it != mRoleChanges.end())
	{
		if (it->second.mChangeType == RC_DELETE)
		{
			return false;
		}

		role_data = it->second;
		return true;
	}

	// Ok, no changes, has not been deleted, is not a new role, just find the
	// role.
	role_list_t::const_iterator rit = mRoles.find(role_id);
	if (rit != mRoles.end())
	{
		role_data = rit->second->getRoleData();
		return true;
	}

	// This role must not exist.
	return false;
}

void LLGroupMgrGroupData::setRoleData(const LLUUID& role_id,
									  LLRoleData role_data)
{
	// If this is a newly created group, we need to change the data in the
	// created list.
	role_data_map_t::iterator it = mRoleChanges.find(role_id);
	if (it != mRoleChanges.end())
	{
		if (it->second.mChangeType == RC_CREATE)
		{
			role_data.mChangeType = RC_CREATE;
			mRoleChanges[role_id] = role_data;
			return;
		}
		else if (it->second.mChangeType == RC_DELETE)
		{
			// Don't do anything for a role being deleted.
			return;
		}
	}

	// Not a new role, so put it in the changes list.
	LLRoleData old_role_data;
	role_list_t::iterator rit = mRoles.find(role_id);
	if (rit != mRoles.end())
	{
		bool data_change =
			rit->second->mRoleData.mRoleDescription !=
				role_data.mRoleDescription ||
			rit->second->mRoleData.mRoleName != role_data.mRoleName ||
			rit->second->mRoleData.mRoleTitle != role_data.mRoleTitle;
		bool powers_change =
			rit->second->mRoleData.mRolePowers != role_data.mRolePowers;

		if (!data_change && !powers_change)
		{
			// We are back to the original state, the changes have been
			// 'undone' so take out the change.
			mRoleChanges.erase(role_id);
			return;
		}

		if (data_change && powers_change)
		{
			role_data.mChangeType = RC_UPDATE_ALL;
		}
		else if (data_change)
		{
			role_data.mChangeType = RC_UPDATE_DATA;
		}
		else
		{
			role_data.mChangeType = RC_UPDATE_POWERS;
		}

		mRoleChanges[role_id] = role_data;
	}
	else
	{
		llwarns << "Change being made to non-existant role " << role_id
				<< llendl;
	}
}

// This is a no-op if the role has already been created.
void LLGroupMgrGroupData::createRole(const LLUUID& role_id,
									 LLRoleData role_data)
{
	if (mRoleChanges.count(role_id))
	{
		llwarns << "Attempt to create a role for existing role " << role_id
				<< ". Aborted." << llendl;
	}
	else
	{
		role_data.mChangeType = RC_CREATE;
		mRoleChanges[role_id] = role_data;
	}
}

void LLGroupMgrGroupData::deleteRole(const LLUUID& role_id)
{
	// If this was a new role, just discard it.
	role_data_map_t::iterator it = mRoleChanges.find(role_id);
	if (it != mRoleChanges.end() && it->second.mChangeType == RC_CREATE)
	{
		mRoleChanges.erase(it);
		return;
	}

	LLRoleData rd;
	rd.mChangeType = RC_DELETE;
	mRoleChanges[role_id] = rd;
}

void LLGroupMgrGroupData::addRolePower(const LLUUID& role_id, U64 power)
{
	LLRoleData rd;
	if (getRoleData(role_id, rd))
	{
		rd.mRolePowers |= power;
		setRoleData(role_id, rd);
	}
	else
	{
		llwarns << "addRolePower: no role data found for " << role_id
				<< llendl;
	}
}

void LLGroupMgrGroupData::removeRolePower(const LLUUID& role_id, U64 power)
{
	LLRoleData rd;
	if (getRoleData(role_id, rd))
	{
		rd.mRolePowers &= ~power;
		setRoleData(role_id, rd);
	}
	else
	{
		llwarns << "removeRolePower: no role data found for " << role_id
				<< llendl;
	}
}

U64 LLGroupMgrGroupData::getRolePowers(const LLUUID& role_id)
{
	LLRoleData rd;
	if (getRoleData(role_id, rd))
	{
		return rd.mRolePowers;
	}
	else
	{
		llwarns << "getRolePowers: no role data found for " << role_id
				<< llendl;
		return GP_NO_POWERS;
	}
}

void LLGroupMgrGroupData::removeData()
{
	// Remove member data first, because removeRoleData will walk the member
	// list
	removeMemberData();
	removeRoleData();
}

void LLGroupMgrGroupData::removeMemberData()
{
	for (member_list_t::iterator mi = mMembers.begin(), end = mMembers.end();
		 mi != end; ++mi)
	{
		delete mi->second;
	}
	mMembers.clear();
	mMemberDataComplete = false;
}

void LLGroupMgrGroupData::removeRoleData()
{
	for (member_list_t::iterator it = mMembers.begin(), end = mMembers.end();
		 it != end; ++it)
	{
		LLGroupMemberData* data = it->second;
		if (data)
		{
			data->clearRoles();
		}
	}

	for (role_list_t::iterator it = mRoles.begin(), end = mRoles.end();
		 it != end; ++it)
	{
		LLGroupRoleData* data = it->second;
		if (data)
		{
			delete data;
		}
	}

	mRoles.clear();
	mReceivedRoleMemberPairs = 0;
	mRoleDataComplete = false;
	mRoleMemberDataComplete = false;
}

void LLGroupMgrGroupData::removeRoleMemberData()
{
	for (member_list_t::iterator it = mMembers.begin(), end = mMembers.end();
		 it != end; ++it)
	{
		LLGroupMemberData* data = it->second;
		if (data)
		{
			data->clearRoles();
		}
	}

	for (role_list_t::iterator it = mRoles.begin(), end = mRoles.end();
		 it != end; ++it)
	{
		LLGroupRoleData* data = it->second;
		if (data)
		{
			data->clearMembers();
		}
	}

	mReceivedRoleMemberPairs = 0;
	mRoleMemberDataComplete = false;
}

LLGroupMgrGroupData::~LLGroupMgrGroupData()
{
	removeData();
}

bool LLGroupMgrGroupData::changeRoleMember(const LLUUID& role_id,
										   const LLUUID& member_id,
										   LLRoleMemberChangeType rmc)
{
	role_list_t::iterator ri = mRoles.find(role_id);
	if (ri == mRoles.end())
	{
		llwarns << "Could not find role " << role_id << llendl;
		return false;
	}

	member_list_t::iterator mi = mMembers.find(member_id);
	if (mi == mMembers.end())
	{
		llwarns << "Could not find member " << member_id << llendl;
		return false;
	}

	LLGroupRoleData* grd = ri->second;
	LLGroupMemberData* gmd = mi->second;
	if (!grd || !gmd)
	{
		llwarns << "Could not get member or role data." << llendl;
		return false;
	}

	if (rmc == RMC_ADD)
	{
		llinfos << "Adding member " << member_id << " to role " << role_id
				<< llendl;
		grd->addMember(member_id);
		gmd->addRole(role_id, grd);

		// TODO move this into addrole function
		// see if they added someone to the owner role and update isOwner
		gmd->mIsOwner = gmd->mIsOwner || role_id == mOwnerRole;
	}
	else if (rmc == RMC_REMOVE)
	{
		llinfos << "Removing member " << member_id << " from role " << role_id
				<< llendl;
		grd->removeMember(member_id);
		gmd->removeRole(role_id);

		// see if they removed someone from the owner role and update isOwner
		gmd->mIsOwner = gmd->mIsOwner && role_id != mOwnerRole;
	}

	lluuid_pair role_member;
	role_member.first = role_id;
	role_member.second = member_id;

	change_map_t::iterator it = mRoleMemberChanges.find(role_member);
	if (it != mRoleMemberChanges.end())
	{
		// There was already a role change for this role_member
		if (it->second.mChange == rmc)
		{
			// Already recorded this change?  Weird.
			llinfos << "Received duplicate change for "
					<< " role: " << role_id << " member " << member_id
					<< " change " << (rmc == RMC_ADD ? "ADD" : "REMOVE")
					<< llendl;
		}
		// The only two operations (add and remove) currently cancel each other
		// out. If that changes this will need more logic
		else if (rmc == RMC_NONE)
		{
			llwarns << "Existing entry with 'RMC_NONE' change !  This should not happen."
					<< llendl;
			LLRoleMemberChange rc(role_id, member_id, rmc);
			mRoleMemberChanges[role_member] = rc;
		}
		else
		{
			mRoleMemberChanges.erase(it);
		}
	}
	else
	{
		LLRoleMemberChange rc(role_id, member_id, rmc);
		mRoleMemberChanges[role_member] = rc;
	}

	recalcAgentPowers(member_id);

	mChanged = true;
	return true;
}

void LLGroupMgrGroupData::recalcAllAgentPowers()
{
	for (member_list_t::iterator mit = mMembers.begin(), end = mMembers.end();
		 mit != end; ++mit)
	{
		LLGroupMemberData* gmd = mit->second;
		if (!gmd) continue;

		gmd->mAgentPowers = 0;
		for (LLGroupMemberData::role_list_t::iterator it = gmd->mRolesList.begin(),
													  end2 = gmd->mRolesList.end();
			 it != end2; ++it)
		{
			LLGroupRoleData* grd = it->second;
			if (!grd) continue;

			gmd->mAgentPowers |= grd->mRoleData.mRolePowers;
		}
	}
}

void LLGroupMgrGroupData::recalcAgentPowers(const LLUUID& agent_id)
{
	member_list_t::iterator mi = mMembers.find(agent_id);
	if (mi == mMembers.end()) return;

	LLGroupMemberData* gmd = mi->second;
	if (!gmd) return;

	gmd->mAgentPowers = 0;
	for (LLGroupMemberData::role_list_t::iterator it = gmd->mRolesList.begin(),
												  end = gmd->mRolesList.end();
		 it != end; ++it)
	{
		LLGroupRoleData* grd = it->second;
		if (grd)
		{
			gmd->mAgentPowers |= grd->mRoleData.mRolePowers;
		}
	}
}

bool packRoleUpdateMessageBlock(LLMessageSystem* msg,
								const LLUUID& group_id, const LLUUID& role_id,
								const LLRoleData& role_data,
								bool start_message)
{
	if (start_message)
	{
		msg->newMessage(_PREHASH_GroupRoleUpdate);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID,gAgentID);
		msg->addUUID(_PREHASH_SessionID,gAgentSessionID);
		msg->addUUID(_PREHASH_GroupID,group_id);
		start_message = false;
	}

	msg->nextBlock(_PREHASH_RoleData);
	msg->addUUID(_PREHASH_RoleID, role_id);
	msg->addString(_PREHASH_Name, role_data.mRoleName);
	msg->addString(_PREHASH_Description, role_data.mRoleDescription);
	msg->addString(_PREHASH_Title, role_data.mRoleTitle);
	msg->addU64(_PREHASH_Powers, role_data.mRolePowers);
	msg->addU8(_PREHASH_UpdateType, (U8)role_data.mChangeType);

	if (msg->isSendFullFast())
	{
		gAgent.sendReliableMessage();
		start_message = true;
	}

	return start_message;
}

void LLGroupMgrGroupData::sendRoleChanges()
{
	// Commit changes locally
	bool start_message = true;
	bool need_role_cleanup = false;
	bool need_role_data = false;
	bool need_power_recalc = false;

	// Apply all changes
	role_list_t::iterator role_it, role_end;
	for (role_data_map_t::iterator iter = mRoleChanges.begin();
		 iter != mRoleChanges.end(); )
	{
		role_data_map_t::iterator it = iter++; // safely increment iter
		const LLUUID& role_id = it->first;
		const LLRoleData& role_data = it->second;

		// Commit to local data set
		role_it = mRoles.find(it->first);
		role_end = mRoles.end();
		if ((role_it == role_end && role_data.mChangeType != RC_CREATE) ||
			(role_it != role_end && role_data.mChangeType == RC_CREATE))
		{
			continue;
		}

		// NOTE: role_it is valid EXCEPT for the RC_CREATE case
		switch (role_data.mChangeType)
		{
			case RC_CREATE:
			{
				// NOTE: role_it is NOT valid in this case
				LLGroupRoleData* grd = new LLGroupRoleData(role_id, role_data,
														   0);
				mRoles[role_id] = grd;
				need_role_data = true;
				break;
			}

			case RC_DELETE:
			{
				LLGroupRoleData* group_role_data = role_it->second;
				delete group_role_data;
				mRoles.erase(role_it);
				need_role_cleanup = true;
				need_power_recalc = true;
				break;
			}

			case RC_UPDATE_ALL:
			case RC_UPDATE_POWERS:
				need_power_recalc = true;
			case RC_UPDATE_DATA:
			default:
			{
				LLGroupRoleData* group_role_data = role_it->second;
				// NOTE: might modify mRoleChanges !
				group_role_data->setRoleData(role_data);
			}
		}

		// Update dataserver
		start_message = packRoleUpdateMessageBlock(gMessageSystemp, getID(),
												   role_id, role_data,
												   start_message);
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}

	// If we delete a role then all the role-member pairs are invalid !
	if (need_role_cleanup)
	{
		removeRoleMemberData();
	}

	// If we create a new role, then we need to re-fetch all the role data.
	if (need_role_data)
	{
		gGroupMgr.sendGroupRoleDataRequest(getID());
	}

	// Clean up change lists
	mRoleChanges.clear();

	// Recalculate all the agent powers because role powers have now changed.
	if (need_power_recalc)
	{
		recalcAllAgentPowers();
	}
}

void LLGroupMgrGroupData::cancelRoleChanges()
{
	// Clear out all changes !
	mRoleChanges.clear();
}

void LLGroupMgrGroupData::createBanEntry(const LLUUID& ban_id,
										 const LLGroupBanData& ban_data)
{
	mBanList[ban_id] = ban_data;
}

void LLGroupMgrGroupData::removeBanEntry(const LLUUID& ban_id)
{
	mBanList.erase(ban_id);
}

//
// LLGroupMgr
//

LLGroupMgr::LLGroupMgr()
:	mLastGroupMembersRequestTime(0.f),
	mMemberRequestInFlight(false)
{
}

LLGroupMgr::~LLGroupMgr()
{
	clearGroups();
}

void LLGroupMgr::clearGroups()
{
	std::for_each(mRoleActionSets.begin(), mRoleActionSets.end(),
				  DeletePointer());
	mRoleActionSets.clear();

	for (auto it = mGroups.begin(), end = mGroups.end(); it != end; ++it)
	{
		delete it->second;
	}
	mGroups.clear();

	mObservers.clear();
}

void LLGroupMgr::clearGroupData(const LLUUID& group_id)
{
	group_map_t::iterator iter = mGroups.find(group_id);
	if (iter != mGroups.end())
	{
		delete iter->second;
		mGroups.hmap_erase(iter);
	}
}

void LLGroupMgr::addObserver(LLGroupMgrObserver* observer)
{
	if (observer && observer->getID().notNull())
	{
		mObservers.emplace(observer->getID(), observer);
	}
}

void LLGroupMgr::removeObserver(LLGroupMgrObserver* observer)
{
	if (observer)
	{
		observer_multimap_t::iterator it;
		it = mObservers.find(observer->getID());
		while (it != mObservers.end())
		{
			if (it->second == observer)
			{
				mObservers.erase(it);
				break;
			}
			else
			{
				++it;
			}
		}
	}
}

LLGroupMgrGroupData* LLGroupMgr::getGroupData(const LLUUID& id)
{
	group_map_t::iterator gi = mGroups.find(id);
	return gi != mGroups.end() ? gi->second : NULL;
}

bool LLGroupMgr::agentCanAddToRole(const LLUUID& group_id,
								   const LLUUID& role_id)
{
	LLGroupMgrGroupData* gdatap = getGroupData(group_id);
	if (!gdatap)
	{
		llinfos << "No group data for group Id: " << group_id
				<< " - Creating and fetching data now..." << llendl;
		fetchGroupMissingData(group_id);
		return false;
	}

	bool is_god = gAgent.isGodlikeWithoutAdminMenuFakery();

	// Make sure the agent is in the group
	LLGroupMgrGroupData::member_list_t::iterator mi =
		gdatap->mMembers.find(gAgentID);
	if (mi == gdatap->mMembers.end())
	{
		if (!gdatap->isMemberDataComplete())
		{
			llinfos << "No group member data received for group Id: "
					<< group_id << " - Fetching data now..." << llendl;
			fetchGroupMissingData(group_id);
		}
		return is_god;
	}

	bool needs_data_fetch = false;

	if (gdatap->isGroupPropertiesDataComplete())
	{
		// 'assign members' can add to non-owner roles.
		if (gAgent.hasPowerInGroup(group_id, GP_ROLE_ASSIGN_MEMBER) &&
			role_id != gdatap->mOwnerRole)
		{
			return true;
		}
	}
	else
	{
		llinfos << "No group properties data received for group Id: "
				<< group_id << llendl;
		needs_data_fetch = true;
		fetchGroupMissingData(group_id);
	}

	if (!gdatap->isRoleDataComplete())
	{
		llinfos << "No role data received for group Id: " << group_id
				<< llendl;
		needs_data_fetch = true;
	}

	LLGroupMemberData* member_data = mi->second;
	if (!member_data)
	{
		llwarns << "No member data for a known member in group Id: "
				<< group_id << llendl;
		needs_data_fetch = true;
	}

	if (needs_data_fetch)
	{
		llinfos << "Fetching data now..." << llendl;
		fetchGroupMissingData(group_id);
	}

	// Owners can add to any role.
	if (member_data && member_data->isInRole(gdatap->mOwnerRole))
	{
		return true;
	}

	// 'Limited assign members' can add to roles the user is in.
	if (gAgent.hasPowerInGroup(group_id, GP_ROLE_ASSIGN_MEMBER_LIMITED) &&
		member_data && member_data->isInRole(role_id))
	{
		return true;
	}

	return is_god;
}

//static
void LLGroupMgr::processGroupMembersReply(LLMessageSystem* msg, void** data)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got group members reply for another agent !" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_RequestID, request_id);

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(group_id);
#if 0
	if (!gdatap)
	{
		LL_DEBUGS("GroupMgr") << "Creating a new group data for group "
							  << group_id
							  << " during group member reply processing"
							  << LL_ENDL;
		gdatap = gGroupMgr.createGroupData(group_id);
	}
#endif
	if (!gdatap || gdatap->mMemberRequestID != request_id)
	{
		llwarns << "Received incorrect, possibly stale request Id" << llendl;
		return;
	}

	msg->getS32(_PREHASH_GroupData, "MemberCount", gdatap->mMemberCount);

	if (gdatap->mMemberCount > 0)
	{
		S32 contribution = 0;
		std::string online_status;
		std::string title;
		U64 agent_powers = 0;
		bool is_owner = false;

		S32 num_members = msg->getNumberOfBlocksFast(_PREHASH_MemberData);
		std::string date_format = gSavedSettings.getString("ShortDateFormat");
		for (S32 i = 0; i < num_members; ++i)
		{
			LLUUID member_id;

			msg->getUUIDFast(_PREHASH_MemberData, _PREHASH_AgentID, member_id,
							 i);
			msg->getS32(_PREHASH_MemberData, _PREHASH_Contribution,
						contribution, i);
			msg->getU64(_PREHASH_MemberData, _PREHASH_AgentPowers,
						agent_powers, i);
			msg->getStringFast(_PREHASH_MemberData, _PREHASH_OnlineStatus,
							   online_status, i);
			msg->getString(_PREHASH_MemberData, _PREHASH_Title, title, i);
			msg->getBool(_PREHASH_MemberData, _PREHASH_IsOwner, is_owner, i);

			if (member_id.notNull())
			{
				tm t;
				if (online_status != "Online" &&
					sscanf(online_status.c_str(), "%u/%u/%u", &t.tm_mon,
						   &t.tm_mday, &t.tm_year) == 3 && t.tm_year > 1900)
				{
					t.tm_year -= 1900;
					t.tm_mon--;
					t.tm_hour = t.tm_min = t.tm_sec = 0;
					timeStructToFormattedString(&t, date_format, online_status);
				}

				LL_DEBUGS("GroupMgr") << "Member " << member_id
									  << " has powers " << std::hex
									  << agent_powers << std::dec << LL_ENDL;
				LLGroupMemberData* newdata =
					new LLGroupMemberData(member_id, contribution,
										  agent_powers, title, online_status,
										  is_owner);

				LLGroupMgrGroupData::member_list_t::iterator mit =
					gdatap->mMembers.find(member_id);
				if (mit != gdatap->mMembers.end())
				{
					LL_DEBUGS("GroupMgr") << "Received duplicate member data for agent "
										  << member_id << LL_ENDL;
				}

				gdatap->mMembers[member_id] = newdata;
			}
			else
			{
				llinfos << "Received null group member data." << llendl;
			}
		}

		// If group members are loaded while titles are missing, load the
		// titles.
		if (gdatap->mTitles.empty())
		{
			gGroupMgr.sendGroupTitlesRequest(group_id);
		}
	}

	if (gdatap->mMembers.size() ==  (U32)gdatap->mMemberCount)
	{
		gdatap->mMemberDataComplete = true;
		gdatap->mMemberRequestID.setNull();
		// We do not want to make role-member data requests until we have all
		// the members
		if (gdatap->mPendingRoleMemberRequest)
		{
			gdatap->mPendingRoleMemberRequest = false;
			gGroupMgr.sendGroupRoleMembersRequest(gdatap->mID);
		}
	}

	gdatap->mChanged = true;
	gGroupMgr.notifyObservers(GC_MEMBER_DATA);
}

//static
void LLGroupMgr::processGroupPropertiesReply(LLMessageSystem* msg, void** data)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got group properties reply for another agent !" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);
	LLUUID founder_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_FounderID, founder_id);
	std::string	name;
	msg->getStringFast(_PREHASH_GroupData, _PREHASH_Name, name);
	std::string	charter;
	msg->getStringFast(_PREHASH_GroupData, _PREHASH_Charter, charter);
	bool show_in_list = false;
	msg->getBoolFast(_PREHASH_GroupData, _PREHASH_ShowInList, show_in_list);
	std::string	member_title;
	msg->getStringFast(_PREHASH_GroupData, _PREHASH_MemberTitle, member_title);
	LLUUID insignia_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_InsigniaID, insignia_id);
	U64 powers_mask = GP_NO_POWERS;
	msg->getU64Fast(_PREHASH_GroupData, _PREHASH_PowersMask, powers_mask);
	U32 membership_fee = 0;
	msg->getU32Fast(_PREHASH_GroupData, _PREHASH_MembershipFee,
					membership_fee);
	bool open_enrollment = false;
	msg->getBoolFast(_PREHASH_GroupData, _PREHASH_OpenEnrollment,
					 open_enrollment);
	S32 num_group_members = 0;
	msg->getS32Fast(_PREHASH_GroupData, _PREHASH_GroupMembershipCount,
					num_group_members);
	S32 num_group_roles = 0;
	msg->getS32(_PREHASH_GroupData, _PREHASH_GroupRolesCount, num_group_roles);
	S32 money = 0;
	msg->getS32Fast(_PREHASH_GroupData, _PREHASH_Money, money);
	bool allow_publish = false;
	msg->getBool(_PREHASH_GroupData, _PREHASH_AllowPublish, allow_publish);
	bool mature = false;
	msg->getBool(_PREHASH_GroupData, _PREHASH_MaturePublish, mature);
	LLUUID owner_role;
	msg->getUUID(_PREHASH_GroupData, _PREHASH_OwnerRole, owner_role);

	LLGroupMgrGroupData* gdatap = gGroupMgr.createGroupData(group_id);

	gdatap->mName = name;
	gdatap->mCharter = charter;
	gdatap->mShowInList = show_in_list;
	gdatap->mInsigniaID = insignia_id;
	gdatap->mFounderID = founder_id;
	gdatap->mMembershipFee = membership_fee;
	gdatap->mOpenEnrollment = open_enrollment;
	gdatap->mAllowPublish = allow_publish;
	gdatap->mMaturePublish = mature;
	gdatap->mOwnerRole = owner_role;
	gdatap->mMemberCount = num_group_members;
	gdatap->mRoleCount = num_group_roles + 1; // Add the everyone role.

	gdatap->mGroupPropertiesDataComplete = true;
	gdatap->mChanged = true;

	gGroupMgr.notifyObservers(GC_PROPERTIES);
}

//static
void LLGroupMgr::processGroupRoleDataReply(LLMessageSystem* msg, void** data)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got group roles reply for another agent !" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_RequestID, request_id);

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(group_id);
#if 0
	if (!gdatap)
	{
		LL_DEBUGS("GroupMgr") << "Creating a new group data for group "
							  << group_id
							  << " during group role reply processing"
							  << LL_ENDL;
		gdatap = gGroupMgr.createGroupData(group_id);
	}
#endif
	if (!gdatap || gdatap->mRoleDataRequestID != request_id)
	{
		llwarns << "Received incorrect, possibly stale request Id" << llendl;
		return;
	}

	msg->getS32(_PREHASH_GroupData, "RoleCount", gdatap->mRoleCount);

	std::string	name;
	std::string	title;
	std::string	desc;
	U64 powers = 0;
	U32 member_count = 0;
	LLUUID role_id;

	U32 num_blocks = msg->getNumberOfBlocks(_PREHASH_RoleData);
	U32 i = 0;
	for (i = 0; i < num_blocks; ++i)
	{
		msg->getUUID(_PREHASH_RoleData, _PREHASH_RoleID, role_id, i);

		msg->getString(_PREHASH_RoleData, _PREHASH_Name, name, i);
		msg->getString(_PREHASH_RoleData, _PREHASH_Title, title, i);
		msg->getString(_PREHASH_RoleData, _PREHASH_Description, desc, i);
		msg->getU64(_PREHASH_RoleData, _PREHASH_Powers, powers, i);
		msg->getU32(_PREHASH_RoleData, _PREHASH_Members, member_count, i);

		LL_DEBUGS("GroupMgr") << "Adding role data: " << name << " {"
							  << role_id << "}" << LL_ENDL;
		LLGroupRoleData* rd = new LLGroupRoleData(role_id, name, title, desc,
												  powers, member_count);
		gdatap->mRoles[role_id] = rd;
	}

	if (gdatap->mRoles.size() == (U32)gdatap->mRoleCount)
	{
		gdatap->mRoleDataComplete = true;
		gdatap->mRoleDataRequestID.setNull();
		// We do not want to make role-member data requests until we have all
		// the role data
		if (gdatap->mPendingRoleMemberRequest)
		{
			gdatap->mPendingRoleMemberRequest = false;
			gGroupMgr.sendGroupRoleMembersRequest(gdatap->mID);
		}
	}

	gdatap->mChanged = true;
	gGroupMgr.notifyObservers(GC_ROLE_DATA);
}

//static
void LLGroupMgr::processGroupRoleMembersReply(LLMessageSystem* msg, void** data)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got group role members reply for another agent !"
				<< llendl;
		return;
	}

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_RequestID, request_id);

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id);

	U32 total_pairs;
	msg->getU32(_PREHASH_AgentData, _PREHASH_TotalPairs, total_pairs);

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(group_id);
#if 0
	if (!gdatap)
	{
		LL_DEBUGS("GroupMgr") << "Creating a new group data for group "
							  << group_id
							  << " during role member reply processing"
							  << LL_ENDL;
		gdatap = gGroupMgr.createGroupData(group_id);
	}
#endif
	if (!gdatap || gdatap->mRoleMembersRequestID != request_id)
	{
		llwarns << "Received incorrect, possibly stale request Id" << llendl;
		return;
	}

	U32 num_blocks = msg->getNumberOfBlocks(_PREHASH_MemberData);
	U32 i;
	LLUUID member_id;
	LLUUID role_id;
	LLGroupRoleData* rd = NULL;
	LLGroupMemberData* md = NULL;

	LLGroupMgrGroupData::role_list_t::iterator ri;
	LLGroupMgrGroupData::member_list_t::iterator mi;

	// If total_pairs == 0, there are no members in any custom roles.
	if (total_pairs > 0)
	{
		for (i = 0;i < num_blocks; ++i)
		{
			msg->getUUID(_PREHASH_MemberData, _PREHASH_RoleID, role_id, i);
			msg->getUUID(_PREHASH_MemberData, _PREHASH_MemberID, member_id, i);

			if (role_id.notNull() && member_id.notNull())
			{
				rd = NULL;
				ri = gdatap->mRoles.find(role_id);
				if (ri != gdatap->mRoles.end())
				{
					rd = ri->second;
				}

				md = NULL;
				mi = gdatap->mMembers.find(member_id);
				if (mi != gdatap->mMembers.end())
				{
					md = mi->second;
				}

				if (rd && md)
				{
					LL_DEBUGS("GroupMgr") << "Adding role-member pair: "
										  << role_id << ", " << member_id
										  << LL_ENDL;
					rd->addMember(member_id);
					md->addRole(role_id, rd);
				}
				else
				{
					if (!rd)
					{
						llwarns << "Received role data for unkown role "
								<< role_id << " in group " << group_id
								<< llendl;
					}
					if (!md)
					{
						llwarns << "Received role data for unkown member "
								<< member_id << " in group " << group_id
								<< llendl;
					}
				}
			}
		}

		gdatap->mReceivedRoleMemberPairs += num_blocks;
	}

	if (gdatap->mReceivedRoleMemberPairs == total_pairs)
	{
		// Add role data for the 'everyone' role to all members
		LLGroupRoleData* everyone = gdatap->mRoles[LLUUID::null];
		if (!everyone)
		{
			llwarns << "Everyone role not found !" << llendl;
		}
		else
		{
			for (LLGroupMgrGroupData::member_list_t::iterator
					mi = gdatap->mMembers.begin(),
					end = gdatap->mMembers.end();
				 mi != end; ++mi)
			{
				LLGroupMemberData* data = mi->second;
				if (data)
				{
					data->addRole(LLUUID::null,everyone);
				}
			}
		}

        gdatap->mRoleMemberDataComplete = true;
		gdatap->mRoleMembersRequestID.setNull();
	}

	gdatap->mChanged = true;
	gGroupMgr.notifyObservers(GC_ROLE_MEMBER_DATA);
}

//static
void LLGroupMgr::processGroupTitlesReply(LLMessageSystem* msg, void** data)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got group titles reply for another agent !" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id);
	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_RequestID, request_id);

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(group_id);
#if 0
	if (!gdatap)
	{
		LL_DEBUGS("GroupMgr") << "Creating a new group data for group "
							  << group_id
							  << " during group titles reply processing"
							  << LL_ENDL;
		gdatap = gGroupMgr.createGroupData(group_id);
	}
#endif
	if (!gdatap || gdatap->mTitlesRequestID != request_id)
	{
		llwarns << "Received incorrect, possibly stale request Id" << llendl;
		return;
	}

	LLGroupTitle title;

	S32 blocks = msg->getNumberOfBlocksFast(_PREHASH_GroupData);
	for (S32 i = 0; i < blocks; ++i)
	{
		msg->getString(_PREHASH_GroupData, _PREHASH_Title, title.mTitle, i);
		msg->getUUID(_PREHASH_GroupData, _PREHASH_RoleID, title.mRoleID, i);
		msg->getBool(_PREHASH_GroupData, _PREHASH_Selected, title.mSelected,
					 i);

		if (!title.mTitle.empty())
		{
			LL_DEBUGS("GroupMgr") << "LLGroupMgr adding title: "
								  << title.mTitle << ", " << title.mRoleID
								  << ", " << (title.mSelected ? 'Y' : 'N')
								  << LL_ENDL;
			gdatap->mTitles.emplace_back(title);
		}
	}

	gdatap->mChanged = true;
	gGroupMgr.notifyObservers(GC_TITLES);
}

//static
void LLGroupMgr::processEjectGroupMemberReply(LLMessageSystem* msg, void ** data)
{
	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);
	bool success;
	msg->getBoolFast(_PREHASH_EjectData, _PREHASH_Success, success);

	// If we had a failure, the group panel needs to be updated.
	if (!success)
	{
		LLFloaterGroupInfo::refreshGroup(group_id);
	}
}

//static
void LLGroupMgr::processJoinGroupReply(LLMessageSystem* msg, void** data)
{
	LLUUID group_id;
	bool success;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);
	msg->getBoolFast(_PREHASH_GroupData, _PREHASH_Success, success);

	if (success)
	{
		// Refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		gGroupMgr.clearGroupData(group_id);
		// Refresh the floater for this group, if any.
		LLFloaterGroupInfo::refreshGroup(group_id);
		// Refresh the group panel of the search window, if necessary.
		HBFloaterSearch::refreshGroup(group_id);
	}
}

//static
void LLGroupMgr::processLeaveGroupReply(LLMessageSystem* msg, void** data)
{
	LLUUID group_id;
	bool success;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);
	msg->getBoolFast(_PREHASH_GroupData, _PREHASH_Success, success);

	if (success)
	{
		// Refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		gGroupMgr.clearGroupData(group_id);
		// close the floater for this group, if any.
		LLFloaterGroupInfo::closeGroup(group_id);
		// Refresh the group panel of the search window, if necessary.
		HBFloaterSearch::refreshGroup(group_id);
	}
}

//static
void LLGroupMgr::processCreateGroupReply(LLMessageSystem* msg, void ** data)
{
	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_ReplyData, _PREHASH_GroupID, group_id);
	bool success;
	msg->getBoolFast(_PREHASH_ReplyData, _PREHASH_Success,	success);
	std::string message;
	msg->getStringFast(_PREHASH_ReplyData, _PREHASH_Message, message);

	if (success)
	{
		// Refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		// *HACK: we have not gotten the agent group update yet, so fake it.
		// This is so when we go to modify the group we will be able to do so.
		// This is not actually too bad because real data will come down in 2
		// or 3 miliseconds and replace this.
		gAgent.mGroups.emplace_back(group_id, "new group", GP_ALL_POWERS);

		LLFloaterGroupInfo::closeCreateGroup();
		LLFloaterGroupInfo::showFromUUID(group_id,"roles_tab");
	}
	else
	{
		// *TODO:translate
		LLSD args;
		args["MESSAGE"] = message;
		gNotifications.add("UnableToCreateGroup", args);
	}
}

LLGroupMgrGroupData* LLGroupMgr::createGroupData(const LLUUID& id)
{
	LLGroupMgrGroupData* gdatap = NULL;

	group_map_t::iterator existing_group = mGroups.find(id);
	if (existing_group == mGroups.end())
	{
		gdatap = new LLGroupMgrGroupData(id);
		addGroup(gdatap);
	}
	else
	{
		gdatap = existing_group->second;
	}

	if (gdatap)
	{
		gdatap->setAccessed();
	}

	return gdatap;
}

void LLGroupMgr::notifyObservers(LLGroupChange gc)
{
	for (group_map_t::iterator gi = mGroups.begin(), end = mGroups.end();
		 gi != end; ++gi)
	{
		LLUUID group_id = gi->first;
		if (gi->second->mChanged)
		{
			// Copy the map because observers may remove themselves on update
			observer_multimap_t observers = mObservers;

			// Find all observers for this group id
			for (observer_multimap_t::iterator
					oi = observers.lower_bound(group_id),
					end2 = observers.upper_bound(group_id);
				 oi != end2; ++oi)
			{
				oi->second->changed(gc);
			}
			gi->second->mChanged = false;
		}
	}
}

void LLGroupMgr::addGroup(LLGroupMgrGroupData* gdatap)
{
	while (mGroups.size() >= MAX_CACHED_GROUPS)
	{
		// LRU: Remove the oldest un-observed group from cache until group size
		// is small enough

		F32 oldest_access = LLFrameTimer::getTotalSeconds();
		group_map_t::iterator oldest_gi = mGroups.end();

		for (group_map_t::iterator gi = mGroups.begin(); gi != mGroups.end();
			 ++gi)
		{
			observer_multimap_t::iterator oi = mObservers.find(gi->first);
			if (oi == mObservers.end())
			{
				if (gi->second && gi->second->getAccessTime() < oldest_access)
				{
					oldest_access = gi->second->getAccessTime();
					oldest_gi = gi;
				}
			}
		}
		if (oldest_gi != mGroups.end())
		{
			delete oldest_gi->second;
			mGroups.erase(oldest_gi);
		}
		else
		{
			// All groups must be currently open, none to remove.
			// Just add the new group anyway, but get out of this loop as it
			// will never drop below max_cached_groups.
			break;
		}
	}

	mGroups[gdatap->getID()] = gdatap;
}

bool LLGroupMgr::fetchGroupMissingData(const LLUUID& group_id)
{
	if (!gAgent.isInGroup(group_id))
	{
		return false;
	}

	bool fetching = false;

	// Start requesting member and role data if needed.
	LLGroupMgrGroupData* gdatap = getGroupData(group_id);
	// Check member data.
	if (!gdatap || !gdatap->isMemberDataComplete())
	{
		sendCapGroupMembersRequest(group_id);
		fetching = true;
	}
	// Check role data.
	if (!gdatap || !gdatap->isRoleDataComplete())
	{
		sendGroupRoleDataRequest(group_id);
		fetching = true;
	}
	// Check role-member mapping data.
	if (!gdatap || !gdatap->isRoleMemberDataComplete())
	{
		sendGroupRoleMembersRequest(group_id);
		fetching = true;
	}
	// Check group titles data.
	if (!gdatap || !gdatap->isGroupTitlePending())
	{
		sendGroupTitlesRequest(group_id);
		fetching = true;
	}
	// Need this to get base group member powers
	if (!gdatap || !gdatap->isGroupPropertiesDataComplete())
	{
		sendGroupPropertiesRequest(group_id);
		fetching = true;
	}

	return fetching;
}

void LLGroupMgr::sendGroupPropertiesRequest(const LLUUID& group_id)
{
#if 0	// This will happen when we get the reply
	LLGroupMgrGroupData* gdatap = createGroupData(group_id);
#endif
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_GroupProfileRequest);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_GroupData);
	msg->addUUID(_PREHASH_GroupID, group_id);
	gAgent.sendReliableMessage();
}

void LLGroupMgr::sendGroupMembersRequest(const LLUUID& group_id)
{
	LLGroupMgrGroupData* gdatap = createGroupData(group_id);
	if (gdatap && gdatap->mMemberRequestID.isNull())
	{
		gdatap->removeMemberData();
		gdatap->mMemberRequestID.generate();

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_GroupMembersRequest);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_GroupData);
		msg->addUUID(_PREHASH_GroupID, group_id);
		msg->addUUID(_PREHASH_RequestID, gdatap->mMemberRequestID);
		gAgent.sendReliableMessage();
	}
}

void LLGroupMgr::sendGroupRoleDataRequest(const LLUUID& group_id)
{
	LLGroupMgrGroupData* gdatap = createGroupData(group_id);
	if (gdatap && gdatap->mRoleDataRequestID.isNull())
	{
		gdatap->removeRoleData();
		gdatap->mRoleDataRequestID.generate();

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_GroupRoleDataRequest);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_GroupData);
		msg->addUUID(_PREHASH_GroupID, group_id);
		msg->addUUID(_PREHASH_RequestID, gdatap->mRoleDataRequestID);
		gAgent.sendReliableMessage();
	}
}

void LLGroupMgr::sendGroupRoleMembersRequest(const LLUUID& group_id)
{
	LLGroupMgrGroupData* gdatap = createGroupData(group_id);
	if (gdatap && gdatap->mRoleMembersRequestID.isNull())
	{
		// Do not send the request if we do not have all the member or role
		// data
		if (!gdatap->isMemberDataComplete() ||
			!gdatap->isRoleDataComplete())
		{
			// *FIXME: Should we start a member or role data request ?
			llinfos << " Pending: "
					<< (gdatap->mPendingRoleMemberRequest ? "Y" : "N")
					<< " MemberDataComplete: "
					<< (gdatap->mMemberDataComplete ? "Y" : "N")
					<< " RoleDataComplete: "
					<< (gdatap->mRoleDataComplete ? "Y" : "N") << llendl;
			gdatap->mPendingRoleMemberRequest = true;
			return;
		}

		gdatap->removeRoleMemberData();
		gdatap->mRoleMembersRequestID.generate();

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_GroupRoleMembersRequest);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_GroupData);
		msg->addUUID(_PREHASH_GroupID, group_id);
		msg->addUUID(_PREHASH_RequestID, gdatap->mRoleMembersRequestID);
		gAgent.sendReliableMessage();
	}
}

void LLGroupMgr::sendGroupTitlesRequest(const LLUUID& group_id)
{
	LLGroupMgrGroupData* gdatap = createGroupData(group_id);
	if (!gdatap) return;

	gdatap->mTitles.clear();
	gdatap->mTitlesRequestID.generate();

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_GroupTitlesRequest);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUID(_PREHASH_GroupID, group_id);
	msg->addUUID(_PREHASH_RequestID, gdatap->mTitlesRequestID);

	gAgent.sendReliableMessage();
}

void LLGroupMgr::sendGroupTitleUpdate(const LLUUID& group_id,
									  const LLUUID& title_role_id)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_GroupTitleUpdate);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUID(_PREHASH_GroupID, group_id);
	msg->addUUID(_PREHASH_TitleRoleID, title_role_id);

	gAgent.sendReliableMessage();

	// Save the change locally
	LLGroupMgrGroupData* gdatap = createGroupData(group_id);
	for (std::vector<LLGroupTitle>::iterator iter = gdatap->mTitles.begin(),
											 end = gdatap->mTitles.end();
		 iter != end; ++iter)
	{
		if (iter->mRoleID == title_role_id)
		{
			iter->mSelected = true;
		}
		else if (iter->mSelected)
		{
			iter->mSelected = false;
		}
	}
}

//static
void LLGroupMgr::sendCreateGroupRequest(const std::string& name,
										const std::string& charter,
										U8 show_in_list,
										const LLUUID& insignia,
										S32 membership_fee,
										bool open_enrollment,
										bool allow_publish,
										bool mature_publish)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_CreateGroupRequest);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);

	msg->nextBlock(_PREHASH_GroupData);
	msg->addString(_PREHASH_Name, name);
	msg->addString(_PREHASH_Charter, charter);
	msg->addBool(_PREHASH_ShowInList, show_in_list);
	msg->addUUID(_PREHASH_InsigniaID, insignia);
	msg->addS32(_PREHASH_MembershipFee, membership_fee);
	msg->addBool(_PREHASH_OpenEnrollment, open_enrollment);
	msg->addBool(_PREHASH_AllowPublish, allow_publish);
	msg->addBool(_PREHASH_MaturePublish, mature_publish);

	gAgent.sendReliableMessage();
}

void LLGroupMgr::sendUpdateGroupInfo(const LLUUID& group_id)
{
	LLGroupMgrGroupData* gdatap = createGroupData(group_id);
	if (!gdatap) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_UpdateGroupInfo);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	msg->nextBlockFast(_PREHASH_GroupData);
	msg->addUUIDFast(_PREHASH_GroupID, gdatap->getID());
	msg->addStringFast(_PREHASH_Charter, gdatap->mCharter);
	msg->addBoolFast(_PREHASH_ShowInList, gdatap->mShowInList);
	msg->addUUIDFast(_PREHASH_InsigniaID, gdatap->mInsigniaID);
	msg->addS32Fast(_PREHASH_MembershipFee, gdatap->mMembershipFee);
	msg->addBoolFast(_PREHASH_OpenEnrollment, gdatap->mOpenEnrollment);
	msg->addBoolFast(_PREHASH_AllowPublish, gdatap->mAllowPublish);
	msg->addBoolFast(_PREHASH_MaturePublish, gdatap->mMaturePublish);

	gAgent.sendReliableMessage();

	// Not expecting a response, so let anyone else watching know the data has
	// changed.
	gdatap->mChanged = true;
	notifyObservers(GC_PROPERTIES);
}

void LLGroupMgr::sendGroupRoleMemberChanges(const LLUUID& group_id)
{
	LLGroupMgrGroupData* gdatap = createGroupData(group_id);
	if (!gdatap || gdatap->mRoleMemberChanges.empty()) return;

	LLMessageSystem* msg = gMessageSystemp;
	bool start_message = true;
	for (LLGroupMgrGroupData::change_map_t::const_iterator
			citer = gdatap->mRoleMemberChanges.begin(),
			end = gdatap->mRoleMemberChanges.end();
		 citer != end; ++citer)
	{
		if (start_message)
		{
			msg->newMessage(_PREHASH_GroupRoleChanges);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->addUUIDFast(_PREHASH_GroupID, group_id);
			start_message = false;
		}
		msg->nextBlock(_PREHASH_RoleChange);
		msg->addUUID(_PREHASH_RoleID, citer->second.mRole);
		msg->addUUID(_PREHASH_MemberID, citer->second.mMember);
		msg->addU32(_PREHASH_Change, (U32)citer->second.mChange);

		if (msg->isSendFullFast())
		{
			gAgent.sendReliableMessage();
			start_message = true;
		}
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}

	gdatap->mRoleMemberChanges.clear();

	// Not expecting a response, so let anyone else watching know the data has
	// changed.
	gdatap->mChanged = true;
	notifyObservers(GC_ROLE_MEMBER_DATA);
}

//static
void LLGroupMgr::sendGroupMemberJoin(const LLUUID& group_id)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_JoinGroupRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_GroupData);
	msg->addUUIDFast(_PREHASH_GroupID, group_id);

	gAgent.sendReliableMessage();
}

// member_role_pairs is <member_id,role_id>
//static
void LLGroupMgr::sendGroupMemberInvites(const LLUUID& group_id,
										role_member_pairs_t& member_role_pairs)
{
	LLMessageSystem* msg = gMessageSystemp;
	bool start_message = true;

	for (role_member_pairs_t::iterator it = member_role_pairs.begin(),
									   end = member_role_pairs.end();
		 it != end; ++it)
	{
		if (start_message)
		{
			msg->newMessage(_PREHASH_InviteGroupRequest);
			msg->nextBlock(_PREHASH_AgentData);
			msg->addUUID(_PREHASH_AgentID, gAgentID);
			msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlock(_PREHASH_GroupData);
			msg->addUUID(_PREHASH_GroupID, group_id);
			start_message = false;
		}

		msg->nextBlock(_PREHASH_InviteData);
		msg->addUUID(_PREHASH_InviteeID, it->first);
		msg->addUUID(_PREHASH_RoleID, it->second);

		if (msg->isSendFull())
		{
			gAgent.sendReliableMessage();
			start_message = true;
		}
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}
}

//static
void LLGroupMgr::sendGroupMemberEjects(const LLUUID& group_id,
									   uuid_vec_t& member_ids)
{
	bool start_message = true;

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(group_id);
	if (!gdatap) return;

	LLMessageSystem* msg = gMessageSystemp;
	for (uuid_vec_t::iterator it = member_ids.begin(), end = member_ids.end();
		 it != end; ++it)
	{
		// Can't use 'eject' to leave a group.
		if (*it == gAgentID) continue;

		// Make sure they are in the group, and we need the member data
		LLGroupMgrGroupData::member_list_t::iterator mit = gdatap->mMembers.find(*it);
		if (mit != gdatap->mMembers.end())
		{
			// Add them to the message
			if (start_message)
			{
				msg->newMessage(_PREHASH_EjectGroupMemberRequest);
				msg->nextBlock(_PREHASH_AgentData);
				msg->addUUID(_PREHASH_AgentID, gAgentID);
				msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlock(_PREHASH_GroupData);
				msg->addUUID(_PREHASH_GroupID, group_id);
				start_message = false;
			}

			msg->nextBlock(_PREHASH_EjectData);
			msg->addUUID(_PREHASH_EjecteeID, *it);

			if (msg->isSendFull())
			{
				gAgent.sendReliableMessage();
				start_message = true;
			}

			// Clean up groupmgr
			for (LLGroupMemberData::role_list_t::iterator
					rit = mit->second->roleBegin(),
					rend = mit->second->roleEnd();
				 rit != rend; ++rit)
			{
				if (rit->first.notNull())
				{
					rit->second->removeMember(*it);
				}
			}
			delete mit->second;
			gdatap->mMembers.erase(*it);
		}
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}
}

//static
void LLGroupMgr::getGroupBanRequestCoro(const std::string& url,
										const LLUUID& group_id)
{
	std::string final_url = url + "?group_id=" + group_id.asString();

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("groupMembersRequest");
	LLSD result = adapter.getAndSuspend(final_url);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Error receiving group member data: " << status.toString()
				<< llendl;
	}
	else if (result.has("ban_list"))
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		// group ban data received
		processGroupBanRequest(result);
	}
}

//static
void LLGroupMgr::postGroupBanRequestCoro(std::string url, LLUUID group_id,
										 U32 action,
										 const uuid_vec_t& ban_list,
										 bool update)
{
	std::string final_url = url + "?group_id=" + group_id.asString();

	LLSD body = LLSD::emptyMap();
	body["ban_action"] = (LLSD::Integer)action;
	// Add our list of potential banned residents to the list
	body["ban_ids"] = LLSD::emptyArray();
	LLSD ban_entry;
	for (uuid_vec_t::const_iterator it = ban_list.begin(),
									end = ban_list.end();
		 it != end; ++it)
	{
		ban_entry = *it;
		body["ban_ids"].append(ban_entry);
	}

	LL_DEBUGS("GroupMgr") << "Posting data: " << body << LL_ENDL;

	LLCore::HttpHeaders::ptr_t headers(new LLCore::HttpHeaders);
	headers->append(HTTP_OUT_HEADER_CONTENT_TYPE, HTTP_CONTENT_LLSD_XML);

	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setFollowRedirects(false);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("groupMembersRequest");
	LLSD result = adapter.postAndSuspend(final_url, body, options, headers);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Error posting group member data: " << status.toString()
				<< llendl;
		return;
	}

	if (result.has("ban_list"))
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		// group ban data received
		processGroupBanRequest(result);
	}

	if (update)
	{
		getGroupBanRequestCoro(url, group_id);
	}
}

//static
void LLGroupMgr::sendGroupBanRequest(EBanRequestType request_type,
									 const LLUUID& group_id, U32 ban_action,
									 const uuid_vec_t& ban_list)
{
	// Get our capability
	const std::string& cap_url = gAgent.getRegionCapability("GroupAPIv1");
	if (cap_url.empty())
	{
		return;
	}

	U32 action = ban_action & ~BAN_UPDATE;
	bool update = (ban_action & BAN_UPDATE) == BAN_UPDATE;

	switch (request_type)
	{
		case REQUEST_GET:
			gCoros.launch("LLGroupMgr::getGroupBanRequestCoro",
						  boost::bind(&LLGroupMgr::getGroupBanRequestCoro,
									   cap_url, group_id));
			break;

		case REQUEST_POST:
			gCoros.launch("LLGroupMgr::postGroupBanRequestCoro",
						  boost::bind(&LLGroupMgr::postGroupBanRequestCoro,
									  cap_url, group_id, action, ban_list,
									  update));
			break;

		default:
			break;
	}
}

//static
void LLGroupMgr::processGroupBanRequest(const LLSD& content)
{
	// Did we get anything in content ?
	if (!content.size())
	{
		llwarns << "No group member data received." << llendl;
		return;
	}

	LLUUID group_id = content["group_id"].asUUID();
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(group_id);
	if (!gdatap) return;

	gdatap->clearBanList();
	for (LLSD::map_const_iterator iter = content["ban_list"].beginMap(),
								  end = content["ban_list"].endMap();
		 iter != end; ++iter)
	{
		const LLUUID ban_id(iter->first);
		LLSD ban_entry(iter->second);

		LLGroupBanData ban_data;
		if (ban_entry.has("ban_date"))
		{
			ban_data.mBanDate = ban_entry["ban_date"].asDate();
			// *TODO: Ban reason
		}

		gdatap->createBanEntry(ban_id, ban_data);
	}

	gdatap->mChanged = true;
	gGroupMgr.notifyObservers(GC_BANLIST);
}

//static
void LLGroupMgr::groupMembersRequestCoro(const std::string& url,
										 const LLUUID& group_id)
{
	gGroupMgr.mMemberRequestInFlight = true;

	LLSD body = LLSD::emptyMap();
	body["group_id"] = group_id;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("groupMembersRequest");
	LLSD result = adapter.postAndSuspend(url, body);

	const LLSD& http_results =
		result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(http_results);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		processCapGroupMembersRequest(result);
	}
	else
	{
		llwarns << "Error receiving group member data: " << status.toString()
				<< llendl;
	}

	gGroupMgr.mMemberRequestInFlight = false;
}

//static
void LLGroupMgr::sendCapGroupMembersRequest(const LLUUID& group_id)
{
	// Are we requesting the information already ?
	if (mMemberRequestInFlight ||
		// or did we request it in the last 0.5 seconds ?
		mLastGroupMembersRequestTime + 0.5f > gFrameTimeSeconds)
	{
		return;
	}
	mLastGroupMembersRequestTime = gFrameTimeSeconds;

	if (!gSavedSettings.getBool("UseHTTPGroupDataFetch"))
	{
		sendGroupMembersRequest(group_id);
		return;
	}

	// Get our capability
	const std::string& cap_url = gAgent.getRegionCapability("GroupMemberData");
	if (cap_url.empty())
	{
		LL_DEBUGS("GroupMgr") << "Region has no GroupMemberData capability. Falling back to UDP fetch."
							  << LL_ENDL;
		sendGroupMembersRequest(group_id);
		return;
	}

	LL_DEBUGS("GroupMgr") << "Region has GroupMemberData capability. Using it."
						  << LL_ENDL;

	// Make sure group exists
	LLGroupMgrGroupData* group_datap = createGroupData(group_id);
	group_datap->mMemberRequestID.generate();	// Mark as pending

	gCoros.launch("LLGroupMgr::groupMembersRequestCoro",
				  boost::bind(&LLGroupMgr::groupMembersRequestCoro, cap_url,
							  group_id));
}

//static
void LLGroupMgr::processCapGroupMembersRequest(const LLSD& content)
{
	// Did we get anything in content ?
	if (!content.size())
	{
		LL_DEBUGS("GroupMgr") << "No group member data received." << LL_ENDL;
		return;
	}

	LLUUID group_id = content["group_id"].asUUID();

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(group_id);
	if (!gdatap)
	{
		llwarns << "Received incorrect, possibly stale, group or request Id"
				<< llendl;
		return;
	}

	// If we have no members, there is no reason to do anything else
	S32	num_members	= content["member_count"];
	if (num_members < 1)
	{
		llinfos << "Received empty group members list for group id: " << group_id
				<< llendl;
		gdatap->mMemberDataComplete = true;
		gdatap->mChanged = true;
		gGroupMgr.notifyObservers(GC_MEMBER_DATA);
		return;
	}

	gdatap->mMemberCount = num_members;

	LLSD member_list = content["members"];
	LLSD titles = content["titles"];
	LLSD defaults = content["defaults"];

	std::string online_status;
	std::string title;
	S32 contribution;
	U64 member_powers;
	// If this is changed to a bool, make sure to change the LLGroupMemberData
	// constructor
	bool is_owner;

	// Compute this once, rather than every time.
	U64	default_powers =
		llstrtou64(defaults["default_powers"].asString().c_str(), NULL, 16);
	std::string date_format = gSavedSettings.getString("ShortDateFormat");

	LLSD::map_const_iterator member_iter_start = member_list.beginMap();
	LLSD::map_const_iterator member_iter_end = member_list.endMap();
	for ( ; member_iter_start != member_iter_end; ++member_iter_start)
	{
		// Reset defaults
		online_status = "unknown";
		title = titles[0].asString();
		contribution = 0;
		member_powers = default_powers;
		is_owner = false;

		const LLUUID member_id(member_iter_start->first);
		LLSD member_info = member_iter_start->second;

		if (member_info.has("last_login"))
		{
			tm t;
			online_status = member_info["last_login"].asString();
			if (online_status != "Online" &&
				sscanf(online_status.c_str(), "%u/%u/%u", &t.tm_mon,
					   &t.tm_mday, &t.tm_year) == 3 && t.tm_year > 1900)
			{
				t.tm_year -= 1900;
				--t.tm_mon;
				t.tm_hour = t.tm_min = t.tm_sec = 0;
				timeStructToFormattedString(&t, date_format, online_status);
			}
		}

		if (member_info.has("title"))
		{
			title = titles[member_info["title"].asInteger()].asString();
		}

		if (member_info.has("powers"))
		{
			member_powers = llstrtou64(member_info["powers"].asString().c_str(),
									   NULL, 16);
		}

		if (member_info.has("donated_square_meters"))
		{
			contribution = member_info["donated_square_meters"];
		}

		if (member_info.has("owner"))
		{
			is_owner = true;
		}

		LLGroupMemberData* data = new LLGroupMemberData(member_id,
														contribution,
														member_powers,
														title,
														online_status,
														is_owner);

		LLGroupMemberData* member_old = gdatap->mMembers[member_id];
		if (member_old && gdatap->mRoleMemberDataComplete)
		{
			for (LLGroupMemberData::role_list_t::iterator
					it = member_old->roleBegin(), end = member_old->roleEnd();
				 it != end; ++it)
			{
				data->addRole(it->first, it->second);
			}
		}
		else
		{
			gdatap->mRoleMemberDataComplete = false;
		}

		gdatap->mMembers[member_id] = data;
	}

	// Technically, we have this data, but to prevent completely overhauling
	// this entire system (it would be nice, but I do not have the time),
	// I am going to be dumb and just call services I most likely do not need
	// with the thought being that the system might need it to be done.
	//
	// TODO:
	// Refactor to reduce multiple calls for data we already have.
	if (gdatap->mTitles.empty())
	{
		gGroupMgr.sendGroupTitlesRequest(group_id);
	}

	gdatap->mMemberDataComplete = true;
	gdatap->mMemberRequestID.setNull();
	// Make the role-member data request
	if (gdatap->mPendingRoleMemberRequest ||
		!gdatap->mRoleMemberDataComplete)
	{
		gdatap->mPendingRoleMemberRequest = false;
		gGroupMgr.sendGroupRoleMembersRequest(group_id);
	}

	gdatap->mChanged = true;
	gGroupMgr.notifyObservers(GC_MEMBER_DATA);
}

void LLGroupMgr::sendGroupRoleChanges(const LLUUID& group_id)
{
	LLGroupMgrGroupData* gdatap = getGroupData(group_id);
	if (gdatap && gdatap->pendingRoleChanges())
	{
		gdatap->sendRoleChanges();

		// Not expecting a response, so let anyone else watching know the data
		// has changed.
		gdatap->mChanged = true;
		notifyObservers(GC_ROLE_DATA);
	}
}

void LLGroupMgr::cancelGroupRoleChanges(const LLUUID& group_id)
{
	LLGroupMgrGroupData* gdatap = getGroupData(group_id);
	if (gdatap)
	{
		gdatap->cancelRoleChanges();
	}
}

//static
bool LLGroupMgr::parseRoleActions(const std::string& xml_filename)
{
	LLXMLNodePtr root;

	bool success = LLUICtrlFactory::getLayeredXMLNode(xml_filename, root);
	if (!success || !root || !root->hasName("role_actions"))
	{
		llerrs << "Problem reading UI role_actions file: " << xml_filename
			   << llendl;
		return false;
	}

	LLXMLNodeList role_list;

	root->getChildren("action_set", role_list, false);

	for (LLXMLNodeList::iterator role_iter = role_list.begin();
		 role_iter != role_list.end(); ++role_iter)
	{
		LLXMLNodePtr action_set = role_iter->second;

		LLRoleActionSet* role_action_set = new LLRoleActionSet();
		LLRoleAction* role_action_data = new LLRoleAction();

		// name=
		std::string action_set_name;
		if (action_set->getAttributeString("name", action_set_name))
		{
			LL_DEBUGS("GroupMgr") << "Loading action set " << action_set_name
								  << LL_ENDL;
			role_action_data->mName = action_set_name;
		}
		else
		{
			llwarns << "Unable to parse action set with no name" << llendl;
			delete role_action_set;
			delete role_action_data;
			continue;
		}
		// description=
		std::string set_description;
		if (action_set->getAttributeString("description", set_description))
		{
			role_action_data->mDescription = set_description;
		}
		// long description=
		std::string set_longdescription;
		if (action_set->getAttributeString("longdescription",
										   set_longdescription))
		{
			role_action_data->mLongDescription = set_longdescription;
		}

		// power mask=
		U64 set_power_mask = 0;

		LLXMLNodeList action_list;
		LLXMLNodeList::iterator action_iter;

		action_set->getChildren("action", action_list, false);

		for (action_iter = action_list.begin();
			 action_iter != action_list.end(); ++action_iter)
		{
			LLXMLNodePtr action = action_iter->second;

			LLRoleAction* role_action = new LLRoleAction();

			// name=
			std::string action_name;
			if (action->getAttributeString("name", action_name))
			{
				LL_DEBUGS("GroupMgr") << "Loading action " << action_name
									  << LL_ENDL;
				role_action->mName = action_name;
			}
			else
			{
				llwarns << "Unable to parse action with no name" << llendl;
				delete role_action;
				continue;
			}
			// description=
			std::string description;
			if (action->getAttributeString("description", description))
			{
				role_action->mDescription = description;
			}
			// long description=
			std::string longdescription;
			if (action->getAttributeString("longdescription", longdescription))
			{
				role_action->mLongDescription = longdescription;
			}
			// description=
			S32 power_bit = 0;
			if (action->getAttributeS32("value", power_bit))
			{
				if (0 <= power_bit && power_bit < 64)
				{
					role_action->mPowerBit = 0x1LL << power_bit;
				}
			}

			set_power_mask |= role_action->mPowerBit;

			role_action_set->mActions.push_back(role_action);
		}

		role_action_data->mPowerBit = set_power_mask;
		role_action_set->mActionSetData = role_action_data;

		gGroupMgr.mRoleActionSets.push_back(role_action_set);
	}
	return true;
}

//static
void LLGroupMgr::debugClearAllGroups(void*)
{
	gGroupMgr.clearGroups();
	LLGroupMgr::parseRoleActions("role_actions.xml");
}
