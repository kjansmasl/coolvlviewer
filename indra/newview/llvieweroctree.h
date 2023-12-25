/**
 * @file llvieweroctree.h
 * @brief LLViewerObjectOctree header file including definitions for supporting functions
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_VIEWEROCTREE_H
#define LL_VIEWEROCTREE_H

#include <map>
#include <vector>

#include "llvector2.h"
#include "llvector4.h"
#include "llmatrix4.h"
#include "lloctree.h"
#include "llrefcount.h"

#include "llviewercamera.h"

class LLVertexBuffer;
class LLViewerRegion;
class LLViewerOctreeEntry;
class LLViewerOctreeEntryData;
class LLViewerOctreeGroup;
class LLViewerOctreePartition;

typedef LLOctreeListener<LLViewerOctreeEntry>	OctreeListener;
typedef LLOctreeNode<LLViewerOctreeEntry>		OctreeNode;
typedef LLOctreeRoot<LLViewerOctreeEntry>		OctreeRoot;
typedef LLOctreeTraveler<LLViewerOctreeEntry>	OctreeTraveler;
typedef LLTreeNode<LLViewerOctreeEntry>			TreeNode;

// Gets index buffer for binary encoded axis vertex buffer given a box at
// center being viewed by given camera
U32 get_box_fan_indices(LLCamera* camera, const LLVector4a& center);
U8* get_box_fan_indices_ptr(LLCamera* camera, const LLVector4a& center);

S32 AABBSphereIntersect(const LLVector4a& min, const LLVector4a& max,
						const LLVector3& origin, const F32& rad);
S32 AABBSphereIntersectR2(const LLVector4a& min, const LLVector4a& max,
						  const LLVector3& origin, const F32& radius_squared);

S32 AABBSphereIntersect(const LLVector3& min, const LLVector3& max,
						const LLVector3& origin, const F32& rad);
S32 AABBSphereIntersectR2(const LLVector3& min, const LLVector3& max,
						  const LLVector3& origin, const F32& radius_squared);

// Defines data needed for octree of an entry
class alignas(16) LLViewerOctreeEntry : public LLRefCount
{
	friend class LLViewerOctreeEntryData;

public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	typedef enum
	{
		LLDRAWABLE = 0,
		LLVOCACHEENTRY,
		NUM_DATA_TYPE
	} eEntryDataType_t;

	LLViewerOctreeEntry();

	// Called by group handleDestruction() only
	LL_INLINE void nullGroup()						{ mGroup = NULL; }

	void setGroup(LLViewerOctreeGroup* group);
	void removeData(LLViewerOctreeEntryData* data);

	LL_INLINE LLViewerOctreeEntryData* getDrawable() const
	{
		return mData[LLDRAWABLE];
	}

	LL_INLINE bool hasDrawable() const				{ return mData[LLDRAWABLE] != NULL; }

	LL_INLINE LLViewerOctreeEntryData* getVOCacheEntry() const
	{
		return mData[LLVOCACHEENTRY];
	}

	LL_INLINE bool hasVOCacheEntry() const			{ return mData[LLVOCACHEENTRY] != NULL; }

	LL_INLINE const LLVector4a* getSpatialExtents() const
	{
		return mExtents;
	}

	LL_INLINE const LLVector4a& getPositionGroup() const
	{
		return mPositionGroup;
	}

	LL_INLINE LLViewerOctreeGroup* getGroup() const	{ return mGroup; }

	LL_INLINE F32 getBinRadius() const				{ return mBinRadius; }
	LL_INLINE S32 getBinIndex() const				{ return mBinIndex; }
	LL_INLINE void setBinIndex(S32 index) const		{ mBinIndex = index; }

protected:
	~LLViewerOctreeEntry() override;

private:
	void addData(LLViewerOctreeEntryData* data);

private:
	// Note: before these variables, we find the 32 bits counter from
	// LLRefCount... Since mExtents will be 16-bytes aligned, fill-up the gap
	// in the cache line with other member variables. HB

	F32							mBinRadius;
	mutable S32					mBinIndex;
	mutable U32					mVisible;

	// Aligned members
	alignas(16) LLVector4a		mExtents[2];
	LLVector4a					mPositionGroup;

	// Do not use LLPointer here:
	LLViewerOctreeEntryData*	mData[NUM_DATA_TYPE];

	LLViewerOctreeGroup*		mGroup;
};

// Defines an abstract class for entry data
class LLViewerOctreeEntryData : public LLRefCount
{
protected:
	~LLViewerOctreeEntryData() override;

public:
#if 0
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}
#endif

	LLViewerOctreeEntryData(const LLViewerOctreeEntryData& rhs)
	{
		*this = rhs;
	}

	LLViewerOctreeEntryData(LLViewerOctreeEntry::eEntryDataType_t data_type);

	LL_INLINE LLViewerOctreeEntry::eEntryDataType_t getDataType() const
	{
		return mDataType;
	}

	LL_INLINE LLViewerOctreeEntry* getEntry()		{ return mEntry; }

	virtual void setOctreeEntry(LLViewerOctreeEntry* entry);
	void removeOctreeEntry();

	LL_INLINE F32 getBinRadius() const				{ return mEntry->getBinRadius(); }
	const LLVector4a* getSpatialExtents() const;
	LLViewerOctreeGroup* getGroup()const;
	const LLVector4a& getPositionGroup() const;

	LL_INLINE void setBinRadius(F32 rad)			{ mEntry->mBinRadius = rad; }
	void setSpatialExtents(const LLVector3& min, const LLVector3& max);
	void setSpatialExtents(const LLVector4a& min, const LLVector4a& max);
	void setPositionGroup(const LLVector4a& pos);

	virtual void setGroup(LLViewerOctreeGroup* group);
	void shift(const LLVector4a& shift_vector);

	LL_INLINE U32 getVisible() const				{ return mEntry ? mEntry->mVisible : 0; }
	void setVisible() const;
	void resetVisible() const;
	virtual bool isVisible() const;
	virtual bool isRecentlyVisible() const;

	LL_INLINE static S32 getCurrentFrame()			{ return sCurVisible; }

protected:
	LL_INLINE LLVector4a& getGroupPosition()		{ return mEntry->mPositionGroup; }
	LL_INLINE void initVisible(U32 visible)			{ mEntry->mVisible = visible; }

	LL_INLINE static void incrementVisible()		{ ++sCurVisible; }

protected:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variable, counting the 32 bits counter from LLRefCount. HB

	LLViewerOctreeEntry::eEntryDataType_t	mDataType;

	LLPointer<LLViewerOctreeEntry>			mEntry;

	// Counter for what value of mVisible means currently visible
	static U32								sCurVisible;
};


// Defines an octree group for an octree node, which contains multiple entries.
class alignas(16) LLViewerOctreeGroup : public LLOctreeListener<LLViewerOctreeEntry>
{
	friend class LLViewerOctreeCull;

protected:
	LOG_CLASS(LLViewerOctreeGroup);

public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	enum
	{
		CLEAN				= 0x00000000,
		DIRTY				= 0x00000001,
		OBJECT_DIRTY		= 0x00000002,
		SKIP_FRUSTUM_CHECK	= 0x00000004,
		DEAD				= 0x00000008,
		INVALID_STATE		= 0x00000010,
	};

	typedef LLOctreeNode<LLViewerOctreeEntry>::element_iter element_iter;
	typedef LLOctreeNode<LLViewerOctreeEntry>::element_list element_list;

	LLViewerOctreeGroup(OctreeNode* node);

	LLViewerOctreeGroup(const LLViewerOctreeGroup& rhs)
	{
		*this = rhs;
	}

	bool removeFromGroup(LLViewerOctreeEntryData* data);
	bool removeFromGroup(LLViewerOctreeEntry* entry);

	virtual void unbound();
	virtual void rebound();

	LL_INLINE bool isDead()							{ return hasState(DEAD); }

	void setVisible();
	bool isVisible() const;

	LL_INLINE virtual bool isRecentlyVisible() const
	{
		return false;
	}

	LL_INLINE S32 getVisible(S32 id) const			{ return mVisible[id]; }

	LL_INLINE S32 getAnyVisible() const				{ return mAnyVisible; }
	LL_INLINE bool isEmpty() const					{ return mOctreeNode->isEmpty(); }

	LL_INLINE U32 getState()						{ return mState; }
	LL_INLINE bool isDirty() const					{ return (mState & DIRTY) != 0; }
	LL_INLINE bool hasState(U32 state) const		{ return (mState & state) != 0; }
	LL_INLINE void setState(U32 state)				{ mState |= state; }
	LL_INLINE void clearState(U32 state)			{ mState &= ~state; }

	// Listener functions
	virtual void handleInsertion(const TreeNode* node,
								 LLViewerOctreeEntry* obj);
	virtual void handleRemoval(const TreeNode* node,
							   LLViewerOctreeEntry* obj);
	virtual void handleDestruction(const TreeNode* node);
	virtual void handleStateChange(const TreeNode* node);
	virtual void handleChildAddition(const OctreeNode* parent,
									 OctreeNode* child);
	virtual void handleChildRemoval(const OctreeNode* parent,
									const OctreeNode* child);

	LL_INLINE OctreeNode* getOctreeNode()			{ return mOctreeNode; }
	LLViewerOctreeGroup* getParent();

	LL_INLINE const LLVector4a* getBounds() const	{ return mBounds; }
	LL_INLINE const LLVector4a* getExtents() const	{ return mExtents; }

	LL_INLINE const LLVector4a* getObjectBounds() const
	{
		return mObjectBounds;
	}

	LL_INLINE const LLVector4a* getObjectExtents() const
	{
		return mObjectExtents;
	}

	// Octree wrappers to make code more readable
	LL_INLINE const element_list& getData() const	{ return mOctreeNode->getData(); }
	LL_INLINE element_iter getDataBegin()			{ return mOctreeNode->getDataBegin(); }
	LL_INLINE element_iter getDataEnd()				{ return mOctreeNode->getDataEnd(); }
	LL_INLINE U32 getElementCount() const			{ return mOctreeNode->getElementCount(); }
	bool hasElement(LLViewerOctreeEntryData* data);

private:
	virtual bool boundObjects(bool empty, LLVector4a& minOut,
							  LLVector4a& maxOut);

protected:
	// Bounding box (center, size) of this node and all its children (tight
	// fit to objects):
	alignas(16) LLVector4a	mBounds[2];
	// Bounding box (center, size) of objects in this node:
	alignas(16) LLVector4a	mObjectBounds[2];
	// Extents (min, max) of this node and all its children:
	alignas(16) LLVector4a	mExtents[2];
	// Extents (min, max) of objects in this node:
	alignas(16) LLVector4a	mObjectExtents[2];

	OctreeNode*				mOctreeNode;
	S32						mVisible[LLViewerCamera::NUM_CAMERAS];
	U32						mState;
	S32						mAnyVisible;	// Latest visible to any camera
};

// Octree group which has capability to support occlusion culling
class LLOcclusionCullingGroup : public LLViewerOctreeGroup
{
public:
	typedef enum
	{
		OCCLUDED				= 0x00010000,
		QUERY_PENDING			= 0x00020000,
		ACTIVE_OCCLUSION		= 0x00040000,
		DISCARD_QUERY			= 0x00080000,
		EARLY_FAIL				= 0x00100000,
	} eOcclusionState;

	typedef enum
	{
		STATE_MODE_SINGLE = 0,		// Set one node
		STATE_MODE_BRANCH,			// Set entire branch
		// Set entire branch as long as current state is different
		STATE_MODE_DIFF,
		// Used for occlusion state, set state for all cameras
		STATE_MODE_ALL_CAMERAS,
	} eSetStateMode;

protected:
	~LLOcclusionCullingGroup() override;

public:
	LLOcclusionCullingGroup(OctreeNode* node, LLViewerOctreePartition* part);

	LLOcclusionCullingGroup(const LLOcclusionCullingGroup& rhs)
	:	LLViewerOctreeGroup(rhs)
	{
		*this = rhs;
	}

	void setOcclusionState(U32 state, S32 mode = STATE_MODE_SINGLE);
	void clearOcclusionState(U32 state, S32 mode = STATE_MODE_SINGLE);
	void checkOcclusion();		// Reads back last occlusion query (if any)
	// Issues occlusion query:
	void doOcclusion(LLCamera* camera, const LLVector4a* shift = NULL);

	LL_INLINE bool isOcclusionState(U32 state) const
	{
		return (mOcclusionState[LLViewerCamera::sCurCameraID] & state) != 0;
	}

	LL_INLINE U32 getOcclusionState() const	
	{
		return mOcclusionState[LLViewerCamera::sCurCameraID];
	}

	bool needsUpdate();
	U32 getLastOcclusionIssuedTime();

	void handleChildAddition(const OctreeNode* parent,
							 OctreeNode* child) override;

	bool isRecentlyVisible() const override;
	bool isAnyRecentlyVisible() const;

	LL_INLINE LLViewerOctreePartition* getSpatialPartition() const
	{
		return mSpatialPartition;
	}

	static U32 getNewOcclusionQueryObjectName();
	static void releaseOcclusionQueryObjectName(U32 name);

	LL_INLINE static U32 getTimeouts()				{ return sOcclusionTimeouts; }

protected:
	void releaseOcclusionQueryObjectNames();

private:
	bool earlyFail(LLCamera* camera, const LLVector4a* bounds);

protected:
	U32							mOcclusionState[LLViewerCamera::NUM_CAMERAS];
	U32							mOcclusionIssued[LLViewerCamera::NUM_CAMERAS];
	U32							mOcclusionChecks[LLViewerCamera::NUM_CAMERAS];
	U32							mOcclusionQuery[LLViewerCamera::NUM_CAMERAS];
	LLViewerOctreePartition*	mSpatialPartition;
	S32							mLODHash;

	static U32					sOcclusionTimeouts;
};

class LLViewerOctreePartition
{
public:
	LLViewerOctreePartition();
	virtual ~LLViewerOctreePartition();

	// Cull on arbitrary frustum
	virtual S32 cull(LLCamera& camera, bool do_occlusion = false) = 0;
	bool			isOcclusionEnabled();

protected:
	// *Must* be called from destructors of any derived classes (SL-17276)
	void cleanup();

public:
	OctreeNode*		mOctree;

	// The region this partition belongs to:
	LLViewerRegion*	mRegionp;

	U32				mPartitionType;
	U32				mDrawableType;

	U32				mLODSeed;

	// Number of frames between LOD updates for a given spatial group
	// (staggered by mLODSeed)
	U32				mLODPeriod;

	// If true, occlusion culling is performed:
	bool			mOcclusionEnabled;
};

class LLViewerOctreeCull : public OctreeTraveler
{
protected:
	LOG_CLASS(LLViewerOctreeCull);

public:
	LLViewerOctreeCull(LLCamera* camera)
	:	mCamera(camera),
		mRes(0)
	{
	}

	void traverse(const OctreeNode* n) override;

protected:
	virtual bool earlyFail(LLViewerOctreeGroup* group);

	// Agent space group cull
	S32 AABBInFrustumNoFarClipGroupBounds(const LLViewerOctreeGroup* group);
	S32 AABBSphereIntersectGroupExtents(const LLViewerOctreeGroup* group);
	S32 AABBInFrustumGroupBounds(const LLViewerOctreeGroup* group);

	// Agent space object set cull
	S32 AABBInFrustumNoFarClipObjectBounds(const LLViewerOctreeGroup* group);
	S32 AABBSphereIntersectObjectExtents(const LLViewerOctreeGroup* group);
	S32 AABBInFrustumObjectBounds(const LLViewerOctreeGroup* group);

	// Local region space group cull
	S32 AABBInRegionFrustumNoFarClipGroupBounds(const LLViewerOctreeGroup* group);
	S32 AABBInRegionFrustumGroupBounds(const LLViewerOctreeGroup* group);
	S32 AABBRegionSphereIntersectGroupExtents(const LLViewerOctreeGroup* group,
											  const LLVector3& shift);

	// Local region space object set cull
	S32 AABBInRegionFrustumNoFarClipObjectBounds(const LLViewerOctreeGroup* group);
	S32 AABBInRegionFrustumObjectBounds(const LLViewerOctreeGroup* group);
	S32 AABBRegionSphereIntersectObjectExtents(const LLViewerOctreeGroup* group,
											   const LLVector3& shift);

	virtual S32 frustumCheck(const LLViewerOctreeGroup* group) = 0;
	virtual S32 frustumCheckObjects(const LLViewerOctreeGroup* group) = 0;

	bool checkProjectionArea(const LLVector4a& center, const LLVector4a& size,
							 const LLVector3& shift, F32 pixel_threshold,
							 F32 near_radius);
	virtual bool checkObjects(const OctreeNode* branch,
							  const LLViewerOctreeGroup* group);
	virtual void preprocess(LLViewerOctreeGroup* group);
	virtual void processGroup(LLViewerOctreeGroup* group);
	void visit(const OctreeNode* branch) override;

protected:
	LLCamera*	mCamera;
	S32			mRes;
};

// Scan the octree, output the info of each node for debug use.
class LLViewerOctreeDebug : public OctreeTraveler
{
public:
	virtual void processGroup(LLViewerOctreeGroup* group);
	void visit(const OctreeNode* branch) override;

public:
	static bool sInDebug;
};

// Called from LLPipeline
bool ll_setup_cube_vb(LLVertexBuffer* vb);

#endif
