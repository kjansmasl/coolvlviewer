/**
 * @file llpathfindingobject.h
 * @brief LLPathfindingObject class declaration
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLPATHFINDINGOBJECT_H
#define LL_LLPATHFINDINGOBJECT_H

#include <memory>

#include "boost/function.hpp"
#include "boost/signals2.hpp"

#include "llavatarnamecache.h"
#include "hbfastmap.h"
#include "lluuid.h"
#include "llvector3.h"

class LLPathfindingCharacter;
class LLPathfindingLinkset;
class LLSD;

class LLPathfindingObject
{
protected:
	LOG_CLASS(LLPathfindingObject);

public:
	typedef std::shared_ptr<LLPathfindingObject> ptr_t;
	typedef fast_hmap<LLUUID, ptr_t> map_t;

	LLPathfindingObject();
	LLPathfindingObject(const LLUUID& id, const LLSD& obj_data);
	LLPathfindingObject(const LLPathfindingObject& obj);
	virtual ~LLPathfindingObject();

	LLPathfindingObject& operator=(const LLPathfindingObject& obj);

	LL_INLINE virtual LLPathfindingLinkset* asLinkset()
	{
		return NULL;
	}

	LL_INLINE virtual const LLPathfindingLinkset* asLinkset() const
	{
		return NULL;
	}

	LL_INLINE virtual LLPathfindingCharacter* asCharacter()
	{
		return NULL;
	}

	LL_INLINE virtual const LLPathfindingCharacter* asCharacter() const
	{
		return NULL;
	}

	LL_INLINE const LLUUID& getUUID() const				{ return mUUID; }
	LL_INLINE const std::string& getName() const		{ return mName; }
	LL_INLINE const std::string& getDescription() const	{ return mDescription; }
	LL_INLINE bool hasOwner() const						{ return mOwnerUUID.notNull(); }
	LL_INLINE bool hasOwnerName() const					{ return mHasOwnerName; }
	std::string getOwnerName() const;
	LL_INLINE bool isGroupOwned() const					{ return mIsGroupOwned; }
	LL_INLINE const LLVector3& getLocation() const		{ return mLocation; }

	typedef boost::function<void(const LLPathfindingObject*)> name_callback_t;
	typedef boost::signals2::signal<void(const LLPathfindingObject*)> name_signal_t;
	typedef boost::signals2::connection name_connection_t;

	name_connection_t registerOwnerNameListener(name_callback_t callback);

private:
	void parseObjectData(const LLSD& obj_data);

	void fetchOwnerName();

	static void handleGroupNameFetch(const LLUUID& group_id,
									 const std::string& name, bool is_group,
									 LLPathfindingObject* self);

	void handleAvatarNameFetch(const LLUUID& av_id,
							   const LLAvatarName& av_name);

	void disconnectAvatarNameCacheConnection();

private:
	LLVector3									mLocation;
	LLUUID										mUUID;
	LLUUID										mOwnerUUID;
	LLAvatarName								mOwnerName;
	LLAvatarNameCache::callback_connection_t	mAvatarNameCacheConnection;
	name_signal_t								mOwnerNameSignal;
	bool										mIsGroupOwned;
	bool										mHasOwnerName;
	std::string									mName;
	std::string									mDescription;
	std::string									mGroupName;

	static uuid_list_t							sPendingGroupUUIDs;

	typedef fast_hset<LLPathfindingObject*> pathfinding_obj_list_t;
	static pathfinding_obj_list_t				sGroupQueriesList;
};

#endif // LL_LLPATHFINDINGOBJECT_H
