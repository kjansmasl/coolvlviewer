/**
 * @file llgroupmgr.h
 * @brief Manager for aggregating all client knowledge for specific groups
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

#ifndef LL_LLGROUPMGR_H
#define LL_LLGROUPMGR_H

#include <map>
#include <string>

#include "hbfastmap.h"
#include "lluuid.h"
#include "roles_constants.h"

constexpr U32 GB_MAX_BANNED_AGENTS = 500;
constexpr U32 MAX_GROUP_INVITES = 100;

class LLDate;
class LLGroupMgr;
class LLGroupRoleData;
class LLMessageSystem;

enum LLGroupChange
{
	GC_PROPERTIES,
	GC_MEMBER_DATA,
	GC_ROLE_DATA,
	GC_ROLE_MEMBER_DATA,
	GC_TITLES,
	GC_BANLIST,
	GC_ALL
};

class LLGroupMgrObserver
{
public:
	LLGroupMgrObserver(const LLUUID& id)
	:	mID(id)
	{
	}

	virtual ~LLGroupMgrObserver()
	{
	}

	virtual void changed(LLGroupChange gc) = 0;
	LL_INLINE const LLUUID& getID()							{ return mID; }

protected:
	LLUUID mID;
};

class LLGroupMemberData
{
friend class LLGroupMgrGroupData;

public:
	LLGroupMemberData(const LLUUID& id, S32 contribution, U64 agent_powers,
					  const std::string& title,
					  const std::string& online_status, bool is_owner);

	LL_INLINE const LLUUID& getID() const					{ return mID; }
	LL_INLINE S32 getContribution() const					{ return mContribution; }
	LL_INLINE U64 getAgentPowers() const					{ return mAgentPowers; }
	LL_INLINE bool isOwner() const							{ return mIsOwner; }
	LL_INLINE const std::string& getTitle() const			{ return mTitle; }
	LL_INLINE const std::string& getOnlineStatus() const	{ return mOnlineStatus; }
	void addRole(const LLUUID& role, LLGroupRoleData* rd);
	bool removeRole(const LLUUID& role);

	typedef fast_hmap<LLUUID, LLGroupRoleData*> role_list_t;
	LL_INLINE void clearRoles()								{ mRolesList.clear(); };
	LL_INLINE role_list_t::iterator roleBegin()				{ return mRolesList.begin(); }
	LL_INLINE role_list_t::iterator roleEnd()				{ return mRolesList.end(); }

	LL_INLINE bool isInRole(const LLUUID& role_id)			{ return mRolesList.count(role_id) != 0; }

private:
	std::string	mTitle;
	std::string	mOnlineStatus;
	role_list_t	mRolesList;
	U64			mAgentPowers;
	LLUUID		mID;
	S32			mContribution;
	bool		mIsOwner;
};

struct LLRoleData
{
	LLRoleData()
	:	mRolePowers(0),
		mChangeType(RC_UPDATE_NONE)
	{
	}

	LLRoleData(const LLRoleData& rd)
	: 	mRoleName(rd.mRoleName),
		mRoleTitle(rd.mRoleTitle),
		mRoleDescription(rd.mRoleDescription),
		mRolePowers(rd.mRolePowers),
		mChangeType(rd.mChangeType)
	{
	}

	std::string mRoleName;
	std::string mRoleTitle;
	std::string mRoleDescription;
	U64	mRolePowers;
	LLRoleChangeType mChangeType;
};

class LLGroupRoleData
{
friend class LLGroupMgrGroupData;

public:
	LLGroupRoleData(const LLUUID& role_id, const std::string& role_name,
					const std::string& title, const std::string& desc,
					U64 role_powers, S32 member_count);

	LLGroupRoleData(const LLUUID& role_id, LLRoleData role_data,
					S32 member_count);

	LL_INLINE const LLUUID& getID() const						{ return mRoleID; }

	LL_INLINE const uuid_vec_t& getRoleMembers() const			{ return mMemberIDs; }

	S32 getMembersInRole(uuid_vec_t members, bool needs_sort = true);

	LL_INLINE S32 getTotalMembersInRole()
	{
		// *FIXME: Returns 0 for Everyone role when Member list isn't yet
		// loaded, see MAINT-5225
		return mMemberCount ? mMemberCount : mMemberIDs.size();
	}

	LL_INLINE LLRoleData getRoleData() const					{ return mRoleData; }
	LL_INLINE void setRoleData(LLRoleData data)					{ mRoleData = data; }

	void addMember(const LLUUID& member);
	bool removeMember(const LLUUID& member);
	void clearMembers();

	LL_INLINE const uuid_vec_t::const_iterator getMembersBegin() const
	{
		return mMemberIDs.begin();
	}

	LL_INLINE const uuid_vec_t::const_iterator getMembersEnd() const
	{
		return mMemberIDs.end();
	}

protected:
	LLGroupRoleData()
	:	mMemberCount(0),
		mMembersNeedsSort(false)
	{
	}

	LLUUID		mRoleID;
	LLRoleData	mRoleData;

	uuid_vec_t	mMemberIDs;
	S32			mMemberCount;

private:
	bool mMembersNeedsSort;
};

struct LLRoleMemberChange
{
	LLRoleMemberChange()
	:	mChange(RMC_NONE)
	{
	}

	LLRoleMemberChange(const LLUUID& role, const LLUUID& member,
					   LLRoleMemberChangeType change)
	:	mRole(role),
		mMember(member),
		mChange(change)
	{
	}

	LLRoleMemberChange(const LLRoleMemberChange& rc)
	:	mRole(rc.mRole),
		mMember(rc.mMember),
		mChange(rc.mChange)
	{
	}

	LLUUID mRole;
	LLUUID mMember;
	LLRoleMemberChangeType mChange;
};

typedef std::pair<LLUUID,LLUUID> lluuid_pair;

struct lluuid_pair_less
{
	bool operator()(const lluuid_pair& lhs, const lluuid_pair& rhs) const
	{
		if (lhs.first == rhs.first)
		{
			return lhs.second < rhs.second;
		}
		else
		{
			return lhs.first < rhs.first;
		}
	}
};

class LLGroupBanData
{
public:
	LLGroupBanData()
	:	mBanDate()
	{
	}

	~LLGroupBanData()
	{
	}

	LLDate mBanDate;
	// *TODO: std:string ban_reason;
};

struct LLGroupTitle
{
	std::string mTitle;
	LLUUID		mRoleID;
	bool		mSelected;
};

class LLGroupMgrGroupData
{
	friend class LLGroupMgr;

public:
	LLGroupMgrGroupData(const LLUUID& id);
	~LLGroupMgrGroupData();

	LL_INLINE const LLUUID& getID()					{ return mID; }

	bool getRoleData(const LLUUID& role_id, LLRoleData& role_data);
	void setRoleData(const LLUUID& role_id, LLRoleData role_data);
	void createRole(const LLUUID& role_id, LLRoleData role_data);
	void deleteRole(const LLUUID& role_id);

	LL_INLINE bool pendingRoleChanges()				{ return !mRoleChanges.empty(); }

	void addRolePower(const LLUUID& role_id, U64 power);
	void removeRolePower(const LLUUID& role_id, U64 power);
	U64 getRolePowers(const LLUUID& role_id);

	void removeData();
	void removeRoleData();
	void removeMemberData();
	void removeRoleMemberData();

	bool changeRoleMember(const LLUUID& role_id, const LLUUID& member_id,
						  LLRoleMemberChangeType rmc);
	void recalcAllAgentPowers();
	void recalcAgentPowers(const LLUUID& agent_id);

	LL_INLINE bool isMemberDataComplete()			{ return mMemberDataComplete; }
	LL_INLINE bool isRoleDataComplete()				{ return mRoleDataComplete; }
	LL_INLINE bool isRoleMemberDataComplete()		{ return mRoleMemberDataComplete; }
	LL_INLINE bool isGroupPropertiesDataComplete()	{ return mGroupPropertiesDataComplete; }

	LL_INLINE bool hasGroupTitles()					{ return !mTitles.empty(); }

	LL_INLINE bool isMemberDataPending()			{ return mMemberRequestID.notNull(); }
	LL_INLINE bool isRoleDataPending()				{ return mRoleDataRequestID.notNull(); }
	LL_INLINE bool isRoleMemberDataPending()		{ return mPendingRoleMemberRequest ||
															 mRoleMembersRequestID.notNull(); }
	LL_INLINE bool isGroupTitlePending()			{ return mTitlesRequestID.notNull() && mTitles.empty(); }

	LL_INLINE F32 getAccessTime() const				{ return mAccessTime; }
	void setAccessed();

	LL_INLINE void clearBanList()					{ mBanList.clear(); }

	void getBanList(const LLUUID& group_id, LLGroupBanData& ban_data);

	LL_INLINE const LLGroupBanData& getBanEntry(const LLUUID& ban_id)
	{
		return mBanList[ban_id];
	}

	void createBanEntry(const LLUUID& ban_id,
						const LLGroupBanData& ban_data = LLGroupBanData());
	void removeBanEntry(const LLUUID& ban_id);

public:
	typedef	fast_hmap<LLUUID, LLGroupMemberData*> member_list_t;
	typedef	fast_hmap<LLUUID, LLGroupRoleData*> role_list_t;
	typedef std::map<lluuid_pair, LLRoleMemberChange, lluuid_pair_less> change_map_t;
	typedef fast_hmap<LLUUID, LLRoleData> role_data_map_t;
	typedef fast_hmap<LLUUID, LLGroupBanData> ban_list_t;

	member_list_t		mMembers;
	role_list_t			mRoles;
	change_map_t		mRoleMemberChanges;
	role_data_map_t		mRoleChanges;
	ban_list_t			mBanList;

	std::vector<LLGroupTitle> mTitles;

	LLUUID				mID;
	LLUUID				mOwnerRole;
	LLUUID				mInsigniaID;
	LLUUID				mFounderID;
	S32					mMembershipFee;
	S32					mMemberCount;
	S32					mRoleCount;
	std::string			mName;
	std::string			mCharter;
	bool				mShowInList;
	bool				mOpenEnrollment;
	bool				mAllowPublish;
	bool				mListInProfile;
	bool				mMaturePublish;
	bool				mChanged;

protected:
	void sendRoleChanges();
	void cancelRoleChanges();

private:
	LLUUID				mMemberRequestID;
	LLUUID				mRoleDataRequestID;
	LLUUID				mRoleMembersRequestID;
	LLUUID				mTitlesRequestID;
	U32					mReceivedRoleMemberPairs;
	F32					mAccessTime;
	bool				mMemberDataComplete;
	bool				mRoleDataComplete;
	bool				mRoleMemberDataComplete;
	bool				mGroupPropertiesDataComplete;
	bool				mPendingRoleMemberRequest;
};

struct LLRoleAction
{
	std::string mName;
	std::string mDescription;
	std::string mLongDescription;
	U64 mPowerBit;
};

struct LLRoleActionSet
{
	LLRoleActionSet();
	~LLRoleActionSet();
	LLRoleAction* mActionSetData;
	std::vector<LLRoleAction*> mActions;
};

class LLGroupMgr
{
protected:
	LOG_CLASS(LLGroupMgr);

public:
	enum EBanRequestType
	{
		REQUEST_GET = 0,
		REQUEST_POST,
		REQUEST_PUT,
		REQUEST_DEL
	};

	enum EBanRequestAction
	{
		BAN_NO_ACTION	= 0,
		BAN_CREATE		= 1,
		BAN_DELETE		= 2,
		BAN_UPDATE		= 4
	};

	LLGroupMgr();
	~LLGroupMgr();

	void addObserver(LLGroupMgrObserver* observer);
	void removeObserver(LLGroupMgrObserver* observer);
	LLGroupMgrGroupData* getGroupData(const LLUUID& id);

	// Used to be a local function in llpanelgrouproles.cpp. Moved here so that
	// it can be used elsewhere in the viewer (currently also used in for Lua
	// in hbviewerautomation.cpp). HB
	bool agentCanAddToRole(const LLUUID& group_id, const LLUUID& role_id);

	// Sends group data requests for any missing data in an agent group.
	// Returns true whenever a fetch was actually performed or false when
	// not an agent group or all data is already available. Used for Lua. HB
	bool fetchGroupMissingData(const LLUUID& group_id);

	void sendGroupPropertiesRequest(const LLUUID& group_id);
	void sendGroupRoleDataRequest(const LLUUID& group_id);
	void sendGroupRoleMembersRequest(const LLUUID& group_id);
	void sendGroupMembersRequest(const LLUUID& group_id);
	void sendGroupTitlesRequest(const LLUUID& group_id);
	void sendGroupTitleUpdate(const LLUUID& group_id,
							  const LLUUID& title_role_id);
	void sendUpdateGroupInfo(const LLUUID& group_id);
	void sendGroupRoleMemberChanges(const LLUUID& group_id);
	void sendGroupRoleChanges(const LLUUID& group_id);

	static void sendCreateGroupRequest(const std::string& name,
									   const std::string& charter,
									   U8 show_in_list,
									   const LLUUID& insignia,
									   S32 membership_fee,
									   bool open_enrollment,
									   bool allow_publish,
									   bool mature_publish);

	static void sendGroupMemberJoin(const LLUUID& group_id);
	typedef fast_hmap<LLUUID, LLUUID> role_member_pairs_t;
	static void sendGroupMemberInvites(const LLUUID& group_id,
									   role_member_pairs_t& role_member_pairs);
	static void sendGroupMemberEjects(const LLUUID& group_id,
									  uuid_vec_t& member_ids);

	static void sendGroupBanRequest(EBanRequestType request_type,
									const LLUUID& group_id,
									U32 ban_action = BAN_NO_ACTION,
									const uuid_vec_t& ban_list = uuid_vec_t());
	static void processGroupBanRequest(const LLSD& content);

	void sendCapGroupMembersRequest(const LLUUID& group_id);

	void cancelGroupRoleChanges(const LLUUID& group_id);

	static void processGroupPropertiesReply(LLMessageSystem* msg, void** data);
	static void processGroupMembersReply(LLMessageSystem* msg, void** data);
	static void processGroupRoleDataReply(LLMessageSystem* msg, void** data);
	static void processGroupRoleMembersReply(LLMessageSystem* msg, void** data);
	static void processGroupTitlesReply(LLMessageSystem* msg, void** data);
	static void processCreateGroupReply(LLMessageSystem* msg, void** data);
	static void processJoinGroupReply(LLMessageSystem* msg, void ** data);
	static void processEjectGroupMemberReply(LLMessageSystem* msg, void ** data);
	static void processLeaveGroupReply(LLMessageSystem* msg, void ** data);

	static bool parseRoleActions(const std::string& xml_filename);

	static void debugClearAllGroups(void*);
	void clearGroups();
	void clearGroupData(const LLUUID& group_id);

private:
	static void groupMembersRequestCoro(const std::string& url,
										const LLUUID& group_id);
	static void processCapGroupMembersRequest(const LLSD& content);

	static void getGroupBanRequestCoro(const std::string& url,
									   const LLUUID& group_id);
	static void postGroupBanRequestCoro(std::string url, LLUUID group_id,
										U32 action, const uuid_vec_t& ban_list,
										bool update);

	void notifyObservers(LLGroupChange gc);
	void addGroup(LLGroupMgrGroupData* group_datap);
	LLGroupMgrGroupData* createGroupData(const LLUUID &id);

public:
	std::vector<LLRoleActionSet*>	mRoleActionSets;

private:
	typedef std::multimap<LLUUID, LLGroupMgrObserver*> observer_multimap_t;
	observer_multimap_t				mObservers;

	typedef fast_hmap<LLUUID, LLGroupMgrGroupData*> group_map_t;
	group_map_t						mGroups;

	F32								mLastGroupMembersRequestTime;
	bool							mMemberRequestInFlight;
};

extern LLGroupMgr gGroupMgr;

#endif
