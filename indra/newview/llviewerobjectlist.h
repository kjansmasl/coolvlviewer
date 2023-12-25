/**
 * @file llviewerobjectlist.h
 * @brief Description of LLViewerObjectList class.
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

#ifndef LL_LLVIEWEROBJECTLIST_H
#define LL_LLVIEWEROBJECTLIST_H

#include "llstat.h"
#include "llstring.h"

#include "llviewerobject.h"

class LLPanelMiniMap;
class LLVOAvatar;
class LLVOCacheEntry;

constexpr U32 CLOSE_BIN_SIZE = 10;
constexpr U32 NUM_BINS = 128;

// GL name = position in object list + GL_NAME_INDEX_OFFSET so that we can have
// special numbers like zero.
constexpr U32 GL_NAME_LAND = 0;
constexpr U32 GL_NAME_PARCEL_WALL = 1;

constexpr U32 GL_NAME_INDEX_OFFSET = 10;

class LLDebugBeacon
{
public:
	LL_INLINE LLDebugBeacon()
	:	mLineWidth(1)
	{
	}

	LL_INLINE LLDebugBeacon(const LLVector3& pos_agent, const std::string& text,
							const LLColor4& text_col, const LLColor4& color,
							S32 line_width)
	:	mPositionAgent(pos_agent),
		mString(text),
		mTextColor(text_col),
		mColor(color),
		mLineWidth(line_width)
	{
	}

	~LLDebugBeacon();

public:
	LLPointer<LLHUDObject>	mHUDObject;
	std::string				mString;
	LLColor4				mColor;
	LLColor4				mTextColor;
	LLVector3				mPositionAgent;
	S32						mLineWidth;
};

class LLViewerObjectList
{
	friend class LLViewerObject;

protected:
	LOG_CLASS(LLViewerObjectList);

public:
	typedef std::vector<LLPointer<LLViewerObject> > vobj_list_t;
	typedef fast_hmap<LLUUID, LLPointer<LLViewerObject> > objs_map_t;
	typedef fast_hmap<LLUUID, LLPointer<LLVOAvatar> > avatars_map_t;

	LLViewerObjectList();

	void cleanupClass();	// Called from LLWorld::cleanupClass()

	// For internal use only. Does NOT take a local Id, but takes an index into
	// an internal dynamic array.
	LL_INLINE LLViewerObject* getObject(S32 index) const
	{
		if (index < 0 || index >= (S32)mObjects.size())
		{
			return NULL;
		}
		LLViewerObject* objectp = mObjects[index];
		return !objectp || objectp->isDead() ? NULL : objectp;
	}

	LL_INLINE LLViewerObject* findObject(const LLUUID& id) const
	{
		objs_map_t::const_iterator iter = mUUIDObjectMap.find(id);
		return iter != mUUIDObjectMap.end() ? iter->second.get() : NULL;
	}

	LL_INLINE LLVOAvatar* findAvatar(const LLUUID& id) const
	{
		avatars_map_t::const_iterator iter = mUUIDAvatarMap.find(id);
		return iter != mUUIDAvatarMap.end() ? iter->second.get() : NULL;
	}

	// Create a viewer-side object:
	LLViewerObject* createObjectViewer(LLPCode pcode, LLViewerRegion* regionp,
									   S32 flags = 0);
	LLViewerObject* createObjectFromCache(LLPCode pcode,
										  LLViewerRegion* regionp,
										  const LLUUID& uuid,
										  U32 local_id);
	LLViewerObject* createObject(LLPCode pcode, LLViewerRegion* regionp,
								 const LLUUID& uuid, U32 local_id,
								 const LLHost& sender);

	// TomY: hack to switch VO instances on the fly:
	LLViewerObject* replaceObject(const LLUUID& id, LLPCode pcode,
								  LLViewerRegion* regionp);

	bool killObject(LLViewerObject* objectp);
	// Kills all objects owned by a particular region:
	void killObjects(LLViewerRegion* regionp);
	void killAllObjects();

	// Clean up the dead objects list.
	void cleanDeadObjects();

	// Simulator and viewer side object updates...
	void processUpdateCore(LLViewerObject* objectp, void** data, U32 block,
						   EObjectUpdateType update_type,
						   LLDataPacker* dpp,
						   bool just_created, bool from_cache = false);
	LLViewerObject* processObjectUpdateFromCache(LLVOCacheEntry* entry,
												 LLViewerRegion* regionp);
	void processObjectUpdate(LLMessageSystem* msg, void** user_data,
							 EObjectUpdateType update_type,
							 bool compressed = false);
	void processCompressedObjectUpdate(LLMessageSystem* msg, void** user_data,
									   EObjectUpdateType update_type);
	void processCachedObjectUpdate(LLMessageSystem* msg, void** user_data,
								   EObjectUpdateType update_type);
	void updateApparentAngles();
	void update();

	void fetchObjectCosts();
	void fetchPhysicsFlags();

	bool gotObjectPhysicsFlags(LLViewerObject* objectp);

	void updateObjectCost(LLViewerObject* object);
	void updateObjectCost(const LLUUID& object_id, F32 object_cost,
						   F32 link_cost, F32 physics_cost,
						   F32 link_physics_cost);
	void onObjectCostFetchFailure(const LLUUID& object_id);

	void updatePhysicsFlags(const LLViewerObject* object);
	void onPhysicsFlagsFetchFailure(const LLUUID& object_id);
	void updatePhysicsShapeType(const LLUUID& object_id, S32 type);
	void updatePhysicsProperties(const LLUUID& object_id, F32 density,
								 F32 friction, F32 restitution,
								 F32 gravity_multiplier);

	void shiftObjects(const LLVector3& offset);
	void repartitionObjects();

	void clearAllMapObjectsInRegion(LLViewerRegion* regionp);
	void renderObjectsForMap(LLPanelMiniMap* map);

	void addDebugBeacon(const LLVector3& pos_agent, const std::string& text,
						const LLColor4& color = LLColor4(1.f, 0.f, 0.f, 0.5f),
						const LLColor4& text_color = LLColor4(1.f, 1.f, 1.f, 1.f),
						S32 line_width = 1);
	void renderObjectBeacons();
	void resetObjectBeacons();

	void dirtyAllObjectInventory();

	void removeFromActiveList(LLViewerObject* objectp);
	void updateActive(LLViewerObject* objectp);

	void updateAvatarVisibility();

	LL_INLINE S32 getNumObjects()				{ return (S32)mObjects.size(); }
	LL_INLINE S32 getNumActiveObjects()			{ return (S32)mActiveObjects.size(); }
	LL_INLINE S32 getNumDeadObjects()			{ return (S32)mDeadObjects.size(); }

	LL_INLINE void addToMap(LLViewerObject* objectp)
	{
		mMapObjects.emplace_back(objectp);
	}

	LL_INLINE void removeFromMap(LLViewerObject* objectp)
	{
		vobj_list_t::iterator iter = std::find(mMapObjects.begin(),
											   mMapObjects.end(), objectp);
		if (iter != mMapObjects.end())
		{
			mMapObjects.erase(iter);
		}
	}

	void clearDebugText();

	// Only accessed by markDead in LLViewerObject
	void cleanupReferences(LLViewerObject* objectp);

#if LL_DEBUG && 0
	// Debugging method.
	// Find references to drawable in all objects, and return value.
	S32 findReferences(LLDrawable* drawablep) const;
#endif

	LL_INLINE S32 getOrphanParentCount() const	{ return (S32)mOrphanParents.size(); }
	LL_INLINE S32 getOrphanCount() const		{ return mNumOrphans; }

	void orphanize(LLViewerObject* childp, U32 parent_id, U32 ip, U32 port);
	void findOrphans(LLViewerObject* objectp, U32 ip, U32 port);

	static void getUUIDFromLocal(LLUUID& id, U32 local_id, U32 ip, U32 port);
	static void setUUIDAndLocal(const LLUUID& id, U32 local_id, U32 ip,
								// Requires knowledge of message system info !
								U32 port);

	static bool removeFromLocalIDTable(const LLViewerObject* objectp);

	// Used ONLY by the orphaned object code.
	static U64 getIndex(U32 local_id, U32 ip, U32 port);

	static void registerKilledAttachment(const LLUUID& id);

private:
	void fetchObjectCostsCoro(const std::string& url);
	void fetchPhysicsFlagsCoro(const std::string& url);

public:
	// Class for keeping track of orphaned objects
	class OrphanInfo
	{
	public:
		OrphanInfo();
		OrphanInfo(U64 parent_info, const LLUUID& child_info);

		bool operator==(const OrphanInfo& rhs) const;
		bool operator!=(const OrphanInfo& rhs) const;

	public:
		U64		mParentInfo;
		LLUUID	mChildInfo;
	};

	U32							mCurBin; // Current bin we are working on.

	// Statistics data
	S32							mNumNewObjects;
	S32							mNumSizeCulled;
	S32							mNumVisCulled;

	S32							mNumUnknownUpdates;
	S32							mNumDeadObjectUpdates;

	LLStat						mNumObjectsStat;
	LLStat						mNumActiveObjectsStat;
	LLStat						mNumNewObjectsStat;
	LLStat						mNumSizeCulledStat;
	LLStat						mNumVisCulledStat;

	// If we paused in the last frame used to discount stats from this frame
	bool						mWasPaused;

	static uuid_list_t			sBlackListedObjects;

protected:
	S32							mCurLazyUpdateIndex;

	S32							mNumOrphans;

	S32							mIdleListSlots;

	// LocalID/ip, port of orphaned objects
	std::vector<U64>			mOrphanParents;

	// UUID's of orphaned objects
	std::vector<OrphanInfo>		mOrphanChildren;

	vobj_list_t					mObjects;
	vobj_list_t					mActiveObjects;
	vobj_list_t					mMapObjects;

	uuid_list_t					mDeadObjects;

	objs_map_t					mUUIDObjectMap;
	avatars_map_t				mUUIDAvatarMap;

	// Set of objects that need to update their cost
	uuid_list_t					mStaleObjectCost;
	uuid_list_t					mPendingObjectCost;

	// Set of objects that need to update their physics flags
	uuid_list_t					mStalePhysicsFlags;
	uuid_list_t					mPendingPhysicsFlags;

	typedef std::vector<LLViewerObject*> vobj_vec_t;
	vobj_vec_t					mIdleList;
	vobj_vec_t					mDeadList;

	std::vector<LLDebugBeacon>	mDebugBeacons;

	static U32					sSimulatorMachineIndex;

	typedef fast_hmap<U64, U32> ip_to_idx_map_t;
	static ip_to_idx_map_t		sIPAndPortToIndex;

	typedef fast_hmap<U64, LLUUID> idx_to_uuid_map_t;
	static idx_to_uuid_map_t	sIndexAndLocalIDToUUID;

	static uuid_list_t			sKilledAttachments;
	static U64					sKilledAttachmentsStamp;
};

// Global object list
extern LLViewerObjectList gObjectList;

#endif // LL_VIEWER_OBJECT_LIST_H
