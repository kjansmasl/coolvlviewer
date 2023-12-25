/**
 * @file llavatartracker.h
 * @brief Definition of the LLAvatarTracker and associated classes
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

#ifndef LL_LLAVATARTRACKER_H
#define LL_LLAVATARTRACKER_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "hbfastmap.h"
#include "lluserrelations.h"
#include "llvector3d.h"

#define TRACK_POWER 0	// Not implemented

class LLAvatarName;
class LLMessageSystem;
class LLTrackingData;

class LLFriendObserver
{
public:
	// This enumeration is a way to refer to what changed in a more human
	// readable format. You can mask the value provided by changed() to see if
	// the observer is interested in the change.
	enum
	{
		NONE = 0,
		ADD = 1,
		REMOVE = 2,
		ONLINE = 4,
		POWERS = 8,
		ALL = 0xffffffff
	};

	virtual ~LLFriendObserver() = default;

	virtual void changed(U32 mask) = 0;

	LL_INLINE virtual void changedBuddies(const uuid_list_t& buddies)
	{
	}
};

#if TRACK_POWER
struct LLBuddyInfo
{
	LLBuddyInfo()
	:	mIsOnline(false),
		mIsEmpowered(false)
	{
	}

	bool mIsOnline;
	bool mIsEmpowered;
};
#endif

// This is used as a base class for doing operations on all buddies.
class LLRelationshipFunctor
{
public:
	virtual ~LLRelationshipFunctor() = default;
	virtual bool operator()(const LLUUID& buddy_id, LLRelationship* buddy) = 0;
};

class LLAvatarTracker
{
protected:
	LOG_CLASS(LLAvatarTracker);

private:
	// do not implement
	LLAvatarTracker(const LLAvatarTracker&);
	bool operator==(const LLAvatarTracker&);

public:
	// Do not you dare create or delete this object
	LLAvatarTracker();
	~LLAvatarTracker();

	void track(const LLUUID& avatar_id, const std::string& name);
	void untrack(const LLUUID& avatar_id);
	LL_INLINE bool isTrackedAgentValid()		{ return mTrackedAgentValid; }
	LL_INLINE void setTrackedAgentValid(bool b)	{ mTrackedAgentValid = b; }
	void findAgent();

	// Coarse update information
	void setTrackedCoarseLocation(const LLVector3d& global_pos);

	// Dealing with the tracked agent location
	bool haveTrackingInfo();
	void getDegreesAndDist(F32& rot, F64& horiz_dist, F64& vert_dist);
	LLVector3d getGlobalPos();

	// Get the name passed in, returns null string if uninitialized.
	const std::string& getName();

	// Get the avatar being tracked, returns LLUUID::null if uninitialized
	const LLUUID& getAvatarID();

	// Add or remove agents from buddy list. Each method takes a set of buddies
	// and returns how many were actually added or removed.
	typedef fast_hmap<LLUUID, LLRelationship*> buddy_map_t;

	S32 addBuddyList(const buddy_map_t& buddies);
	//S32 removeBuddyList(const buddy_list_t& exes);
	void copyBuddyList(buddy_map_t& buddies) const;

	// Deal with termination of friendhsip
	void terminateBuddy(const LLUUID& id);

	// Flag the buddy list dirty to force an update
	void dirtyBuddies();

	// Get full info
	const LLRelationship* getBuddyInfo(const LLUUID& id) const;

	// Online status
	void setBuddyOnline(const LLUUID& id, bool is_online);
	bool isBuddyOnline(const LLUUID& id) const;

	LL_INLINE bool isBuddy(const LLUUID& id) const
	{
		return mBuddyInfo.count(id) > 0;
	}

#if TRACK_POWER
	// Simple empowered status
	void setBuddyEmpowered(const LLUUID& id, bool is_empowered);
	bool isBuddyEmpowered(const LLUUID& id) const;

	// Set the empower bit & message the server.
	void empowerList(const buddy_map_t& list, bool grant);
	void empower(const LLUUID& id, bool grant); // wrapper for above
#endif

	// Register callbacks
	void registerCallbacks(LLMessageSystem* msg);

	// Add/remove an observer. If the observer is destroyed, be sure to remove
	// it. On destruction of the tracker, it will delete any observers left
	// behind.
	void addObserver(LLFriendObserver* observer);
	void removeObserver(LLFriendObserver* observer);
	void idleNotifyObservers();
	void notifyObservers();

	// Apply the functor to every buddy. Do not actually modify the buddy list
	// in the functor or bad things will happen.
	void applyFunctor(LLRelationshipFunctor& f);

	// Stores flag for change and optionnally the Id of the buddy the change
	// applies to.
	void addChangedMask(U32 mask, const LLUUID& buddy_id = LLUUID::null);

	static void formFriendship(const LLUUID& friend_id);

	static bool isAgentFriend(const LLUUID& agent_id);
	static bool isAgentMappable(const LLUUID& agent_id);

protected:
	void deleteTrackingData();
	void agentFound(const LLUUID& prey,
					const LLVector3d& estimated_global_pos);

	// Message system functionality
	static void processAgentFound(LLMessageSystem* msg, void**);
	static void processOnlineNotification(LLMessageSystem* msg, void**);
	static void processOfflineNotification(LLMessageSystem* msg, void**);
	static void processTerminateFriendship(LLMessageSystem* msg, void**);
	static void processChangeUserRights(LLMessageSystem* msg, void**);

	static void callbackLoadAvatarName(const LLUUID& id, bool online,
									   const LLAvatarName& avatar_name);

	void processNotify(LLMessageSystem* msg, bool online);
	void processChange(LLMessageSystem* msg);

protected:
	LLTrackingData*	mTrackingData;
	U32				mModifyMask;

	buddy_map_t		mBuddyInfo;

	uuid_list_t		mChangedBuddyIDs;

	typedef std::vector<LLFriendObserver*> observer_list_t;
	observer_list_t	mObservers;

	bool mTrackedAgentValid;
	bool mIsNotifyObservers;
};

// Collect set of LLUUIDs we are a proxy for
class LLCollectProxyBuddies final : public LLRelationshipFunctor
{
public:
	LL_INLINE LLCollectProxyBuddies() = default;
	bool operator()(const LLUUID& buddy_id, LLRelationship* buddy) override;

public:
	typedef std::set<LLUUID> buddy_list_t;
	buddy_list_t mProxy;
};

// Collect dictionary sorted map of name -> agent_id for every mappable buddy
class LLCollectMappableBuddies final : public LLRelationshipFunctor
{
public:
	LL_INLINE LLCollectMappableBuddies() = default;
	bool operator()(const LLUUID& buddy_id, LLRelationship* buddy) override;

public:
	typedef std::map<std::string, LLUUID, LLDictionaryLess> buddy_map_t;
	buddy_map_t mMappable;
	std::string mFirst;
	std::string mLast;
};

// Collect dictionary sorted map of name -> agent_id for every online buddy
class LLCollectOnlineBuddies final : public LLRelationshipFunctor
{
public:
	LL_INLINE LLCollectOnlineBuddies() = default;
	bool operator()(const LLUUID& buddy_id, LLRelationship* buddy) override;

public:
	typedef std::map<std::string, LLUUID, LLDictionaryLess> buddy_map_t;
	buddy_map_t mOnline;
	std::string mFirst;
	std::string mLast;
};

// Collect dictionary sorted map of name -> agent_id for every buddy, one map
// is offline and the other map is online.
class LLCollectAllBuddies final : public LLRelationshipFunctor
{
public:
	LL_INLINE LLCollectAllBuddies() = default;
	bool operator()(const LLUUID& buddy_id, LLRelationship* buddy) override;

public:
	typedef std::map<std::string, LLUUID, LLDictionaryLess> buddy_map_t;
	buddy_map_t mOnline;
	buddy_map_t mOffline;
	std::string mFirst;
	std::string mLast;
};

extern LLAvatarTracker gAvatarTracker;

#endif // LL_LLAVATARTRACKER_H
