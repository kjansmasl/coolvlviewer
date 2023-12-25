/**
 * @file llpathfindingobject.cpp
 * @brief Implementation of llpathfindingobject
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

#include "llviewerprecompiledheaders.h"

#include "llpathfindingobject.h"

#include "llcachename.h"
#include "llsd.h"

#define PATHFINDING_OBJECT_NAME_FIELD           "name"
#define PATHFINDING_OBJECT_DESCRIPTION_FIELD    "description"
#define PATHFINDING_OBJECT_OWNER_FIELD          "owner"
#define PATHFINDING_OBJECT_POSITION_FIELD       "position"
#define PATHFINDING_OBJECT_IS_GROUP_OWNED_FIELD "owner_is_group"

//static
LLPathfindingObject::pathfinding_obj_list_t LLPathfindingObject::sGroupQueriesList;
uuid_list_t LLPathfindingObject::sPendingGroupUUIDs;

LLPathfindingObject::LLPathfindingObject()
:	mHasOwnerName(false),
	mIsGroupOwned(false),
	mAvatarNameCacheConnection(),
	mOwnerNameSignal()
{
}

LLPathfindingObject::LLPathfindingObject(const LLUUID& id, const LLSD& obj_data)
:	mUUID(id),
	mHasOwnerName(false),
	mIsGroupOwned(false),
	mAvatarNameCacheConnection(),
	mOwnerNameSignal()
{
	parseObjectData(obj_data);
}

LLPathfindingObject::LLPathfindingObject(const LLPathfindingObject& obj)
:	mUUID(obj.mUUID),
	mName(obj.mName),
	mDescription(obj.mDescription),
	mOwnerUUID(obj.mOwnerUUID),
	mHasOwnerName(false),
	mIsGroupOwned(obj.mIsGroupOwned),
	mLocation(obj.mLocation),
	mAvatarNameCacheConnection(),
	mOwnerNameSignal()
{
	fetchOwnerName();
}

LLPathfindingObject::~LLPathfindingObject()
{
	disconnectAvatarNameCacheConnection();
	sGroupQueriesList.erase(this);
}

LLPathfindingObject& LLPathfindingObject::operator=(const LLPathfindingObject& obj)
{
	mUUID = obj.mUUID;
	mName = obj.mName;
	mDescription = obj.mDescription;
	mOwnerUUID = obj.mOwnerUUID;
	fetchOwnerName();
	mIsGroupOwned = obj.mIsGroupOwned;
	mLocation = obj.mLocation;

	return *this;
}

std::string LLPathfindingObject::getOwnerName() const
{
	if (!hasOwner())
	{
		return "";
	}
	return mIsGroupOwned ? mGroupName : mOwnerName.getLegacyName();
}

void LLPathfindingObject::parseObjectData(const LLSD& obj_data)
{
	if (obj_data.has(PATHFINDING_OBJECT_NAME_FIELD) &&
		obj_data.get(PATHFINDING_OBJECT_NAME_FIELD).isString())
	{
		mName = obj_data.get(PATHFINDING_OBJECT_NAME_FIELD).asString();
	}
	else
	{
		llwarns << "Malformed pathfinding object data: no name" << llendl;
	}

	if (obj_data.has(PATHFINDING_OBJECT_DESCRIPTION_FIELD) &&
		obj_data.get(PATHFINDING_OBJECT_DESCRIPTION_FIELD).isString())
	{
		mDescription =
			obj_data.get(PATHFINDING_OBJECT_DESCRIPTION_FIELD).asString();
	}
	else
	{
		llwarns << "Malformed pathfinding object data: no description"
				<< llendl;
	}

	if (obj_data.has(PATHFINDING_OBJECT_IS_GROUP_OWNED_FIELD))
	{
		if (obj_data.get(PATHFINDING_OBJECT_IS_GROUP_OWNED_FIELD).isBoolean())
		{
			mIsGroupOwned =
				obj_data.get(PATHFINDING_OBJECT_IS_GROUP_OWNED_FIELD).asBoolean();
		}
		else
		{
			llwarns << "Malformed pathfinding object data: bad group flag"
					<< llendl;
		}
	}

	if (obj_data.has(PATHFINDING_OBJECT_OWNER_FIELD) &&
		obj_data.get(PATHFINDING_OBJECT_OWNER_FIELD).isUUID())
	{
		mOwnerUUID = obj_data.get(PATHFINDING_OBJECT_OWNER_FIELD).asUUID();
		fetchOwnerName();
	}
	else
	{
		llwarns << "Malformed pathfinding object data: no owner UUID"
				<< llendl;
	}

	if (obj_data.has(PATHFINDING_OBJECT_POSITION_FIELD) &&
		obj_data.get(PATHFINDING_OBJECT_POSITION_FIELD).isArray())
	{
		mLocation.setValue(obj_data.get(PATHFINDING_OBJECT_POSITION_FIELD));
	}
	else
	{
		llwarns << "Malformed pathfinding object data: no position"
				<< llendl;
	}
}

LLPathfindingObject::name_connection_t LLPathfindingObject::registerOwnerNameListener(name_callback_t callback)
{
	llassert(hasOwner());

	name_connection_t connection;

	if (hasOwnerName())
	{
		callback(this);
	}
	else
	{
		connection = mOwnerNameSignal.connect(callback);
	}

	return connection;
}

void LLPathfindingObject::fetchOwnerName()
{
	mHasOwnerName = false;
	if (!hasOwner())
	{
		return;
	}

	if (mIsGroupOwned)
	{
		if (!gCacheNamep) return;	// Paranoia

		mHasOwnerName = gCacheNamep->getGroupName(mOwnerUUID, mGroupName);
		if (!mHasOwnerName)
		{
			sGroupQueriesList.insert(this);
			gCacheNamep->get(mOwnerUUID, true,
							 boost::bind(&LLPathfindingObject::handleGroupNameFetch,
										 _1, _2, _3, this));
		}
	}
	else
	{
		mHasOwnerName = LLAvatarNameCache::get(mOwnerUUID, &mOwnerName);
		if (!mHasOwnerName)
		{
			disconnectAvatarNameCacheConnection();
			mAvatarNameCacheConnection =
				LLAvatarNameCache::get(mOwnerUUID,
									   boost::bind(&LLPathfindingObject::handleAvatarNameFetch,
												   this, _1, _2));
		}
	}

	if (mHasOwnerName)
	{
		mOwnerNameSignal(this);
	}
}

//static
void LLPathfindingObject::handleGroupNameFetch(const LLUUID& group_id,
											   const std::string& name,
											   bool is_group,
											   LLPathfindingObject* self)
{
	if (sGroupQueriesList.count(self))
	{
		sGroupQueriesList.erase(self);
		self->mGroupName = name;
		self->mHasOwnerName = true;
		self->mOwnerNameSignal(self);
	}
}

void LLPathfindingObject::handleAvatarNameFetch(const LLUUID& av_id,
												const LLAvatarName& av_name)
{
	if (mOwnerUUID == av_id)
	{
		mOwnerName = av_name;
		mHasOwnerName = true;
		disconnectAvatarNameCacheConnection();
		mOwnerNameSignal(this);
	}
	else
	{
		llwarns << "Incorrect UUID in avatar name request reply" << llendl;
	}
}

void LLPathfindingObject::disconnectAvatarNameCacheConnection()
{
	if (mAvatarNameCacheConnection.connected())
	{
		mAvatarNameCacheConnection.disconnect();
	}
}
