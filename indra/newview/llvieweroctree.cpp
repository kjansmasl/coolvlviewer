/**
 * @file llvieweroctree.cpp
 * @brief LLViewerObjectOctree class implementation and supporting functions
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

#include "llviewerprecompiledheaders.h"

#include <queue>

#include "llvieweroctree.h"

#include "llfasttimer.h"
#include "llimagedecodethread.h"

#include "llappviewer.h"			// For gFrameCount
#include "lldrawpoolwater.h"
#include "llpipeline.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"

//-----------------------------------------------------------------------------
// Static & global variables and definitions
//-----------------------------------------------------------------------------

constexpr F32 SG_OCCLUSION_FUDGE = 0.25f;
#define SG_DISCARD_TOLERANCE 0.01f

// reserve the low numbers for special use.
U32 LLViewerOctreeEntryData::sCurVisible = 10;

bool LLViewerOctreeDebug::sInDebug = false;

//-----------------------------------------------------------------------------
// Some global functions definitions
//-----------------------------------------------------------------------------
typedef enum
{
	b000 = 0x00,
	b001 = 0x01,
	b010 = 0x02,
	b011 = 0x03,
	b100 = 0x04,
	b101 = 0x05,
	b110 = 0x06,
	b111 = 0x07,
} eLoveTheBits;

// Contact Runitai Linden for a copy of the SL object used to write this table
// basically, you give the table a bitmask of the look-at vector to a node and
// it gives you a triangle fan index array
static U16 sOcclusionIndices[] =
{
	 //000
		b111, b110, b010, b011, b001, b101, b100, b110,
	 //001
		b011, b010, b000, b001, b101, b111, b110, b010,
	 //010
		b101, b100, b110, b111, b011, b001, b000, b100,
	 //011
		b001, b000, b100, b101, b111, b011, b010, b000,
	 //100
		b110, b000, b010, b011, b111, b101, b100, b000,
	 //101
		b010, b100, b000, b001, b011, b111, b110, b100,
	 //110
		b100, b010, b110, b111, b101, b001, b000, b010,
	 //111
		b000, b110, b100, b101, b001, b011, b010, b110,
};

U32 get_box_fan_indices(LLCamera* camera, const LLVector4a& center)
{
	LLVector4a origin;
	origin.load3(camera->getOrigin().mV);

	S32 cypher = center.greaterThan(origin).getGatheredBits() & 0x7;

	return cypher * 8;
}

U8* get_box_fan_indices_ptr(LLCamera* camera, const LLVector4a& center)
{
	LLVector4a origin;
	origin.load3(camera->getOrigin().mV);

	S32 cypher = center.greaterThan(origin).getGatheredBits() & 0x7;

	return (U8*)(sOcclusionIndices + cypher * 8);
}

bool ll_setup_cube_vb(LLVertexBuffer* vb)
{
	if (!vb->allocateBuffer(8, 64))
	{
		return false;
	}

	LLStrider<LLVector3> pos;
	LLStrider<U16> idx;
	if (!vb->getVertexStrider(pos) || !vb->getIndexStrider(idx))
	{
		return false;
	}

	pos[0] = LLVector3(-1.f, -1.f, -1.f);
	pos[1] = LLVector3(-1.f, -1.f, 1.f);
	pos[2] = LLVector3(-1.f, 1.f, -1.f);
	pos[3] = LLVector3(-1.f, 1.f, 1.f);
	pos[4] = LLVector3(1.f, -1.f, -1.f);
	pos[5] = LLVector3(1.f, -1.f, 1.f);
	pos[6] = LLVector3(1.f, 1.f, -1.f);
	pos[7] = LLVector3(1.f, 1.f, 1.f);

	for (U32 i = 0; i < 64; ++i)
	{
		idx[i] = sOcclusionIndices[i];
	}

	vb->unmapBuffer();

	return true;
}

S32 AABBSphereIntersect(const LLVector3& min, const LLVector3& max,
						const LLVector3& origin, const F32& rad)
{
	return AABBSphereIntersectR2(min, max, origin, rad * rad);
}

S32 AABBSphereIntersectR2(const LLVector3& min, const LLVector3& max,
						  const LLVector3& origin, const F32& r)
{
	F32 d = 0.f;
	F32 t;

	if ((min - origin).lengthSquared() < r &&
		(max - origin).lengthSquared() < r)
	{
		return 2;
	}

	for (U32 i = 0; i < 3; ++i)
	{
		if (origin.mV[i] < min.mV[i])
		{
			t = min.mV[i] - origin.mV[i];
			d += t * t;
		}
		else if (origin.mV[i] > max.mV[i])
		{
			t = origin.mV[i] - max.mV[i];
			d += t * t;
		}

		if (d > r)
		{
			return 0;
		}
	}

	return 1;
}

S32 AABBSphereIntersect(const LLVector4a& min, const LLVector4a& max,
						const LLVector3& origin, const F32& rad)
{
	return AABBSphereIntersectR2(min, max, origin, rad * rad);
}

S32 AABBSphereIntersectR2(const LLVector4a& min, const LLVector4a& max,
						  const LLVector3& origin, const F32& r)
{
	F32 d = 0.f;
	F32 t;

	LLVector4a origina;
	origina.load3(origin.mV);

	LLVector4a v;
	v.setSub(min, origina);

	if (v.dot3(v) < r)
	{
		v.setSub(max, origina);
		if (v.dot3(v) < r)
		{
			return 2;
		}
	}

	for (U32 i = 0; i < 3; ++i)
	{
		if (origin.mV[i] < min[i])
		{
			t = min[i] - origin.mV[i];
			d += t * t;
		}
		else if (origin.mV[i] > max[i])
		{
			t = origin.mV[i] - max[i];
			d += t * t;
		}

		if (d > r)
		{
			return 0;
		}
	}

	return 1;
}

//-----------------------------------------------------------------------------
// LLViewerOctreeEntry class
//-----------------------------------------------------------------------------
LLViewerOctreeEntry::LLViewerOctreeEntry()
:	mGroup(NULL),
	mBinRadius(0.f),
	mBinIndex(-1),
	mVisible(0)
{
	mPositionGroup.clear();
	mExtents[0].clear();
	mExtents[1].clear();

	for (S32 i = 0; i < NUM_DATA_TYPE; ++i)
	{
		mData[i] = NULL;
	}
}

LLViewerOctreeEntry::~LLViewerOctreeEntry()
{
	llassert(!mGroup);
}

void LLViewerOctreeEntry::addData(LLViewerOctreeEntryData* data)
{
#if 0
	llassert(mData[data->getDataType()] == NULL);
#endif
	llassert(data != NULL);

	mData[data->getDataType()] = data;
}

void LLViewerOctreeEntry::removeData(LLViewerOctreeEntryData* data)
{
#if 0
	// Cannot remove LLVOCache entry
	llassert(data->getDataType() != LLVOCACHEENTRY);
#endif

	if (!mData[data->getDataType()] || mData[data->getDataType()] != data)
	{
		return;
	}

	mData[data->getDataType()] = NULL;

	if (mGroup != NULL && !mData[LLDRAWABLE])
	{
		LLViewerOctreeGroup* group = mGroup;
		mGroup = NULL;
		group->removeFromGroup(data);

		llassert(mBinIndex == -1);
	}
}

void LLViewerOctreeEntry::setGroup(LLViewerOctreeGroup* group)
{
	if (mGroup == group)
	{
		return;
	}

	if (mGroup)
	{
		LLViewerOctreeGroup* group = mGroup;
		mGroup = NULL;
		group->removeFromGroup(this);

		llassert(mBinIndex == -1);
	}

	mGroup = group;
}

//-----------------------------------------------------------------------------
// LLViewerOctreeEntryData class
//-----------------------------------------------------------------------------

LLViewerOctreeEntryData::~LLViewerOctreeEntryData()
{
	if (mEntry)
	{
		mEntry->removeData(this);
		mEntry = NULL;
	}
}

LLViewerOctreeEntryData::LLViewerOctreeEntryData(LLViewerOctreeEntry::eEntryDataType_t data_type)
:	mDataType(data_type),
	mEntry(NULL)
{
}

//virtual
void LLViewerOctreeEntryData::setOctreeEntry(LLViewerOctreeEntry* entry)
{
	if (mEntry.notNull())
	{
		llwarns << "This should not be called when mEntry is not NULL !"
				<< llendl;
		llassert(false);
		return;
	}

	if (!entry)
	{
		mEntry = new LLViewerOctreeEntry();
	}
	else
	{
		mEntry = entry;
	}

	mEntry->addData(this);
}

void LLViewerOctreeEntryData::removeOctreeEntry()
{
	if (mEntry)
	{
		mEntry->removeData(this);
		mEntry = NULL;
	}
}

void LLViewerOctreeEntryData::setSpatialExtents(const LLVector3& min,
												const LLVector3& max)
{
	mEntry->mExtents[0].load3(min.mV);
	mEntry->mExtents[1].load3(max.mV);
}

void LLViewerOctreeEntryData::setSpatialExtents(const LLVector4a& min,
												const LLVector4a& max)
{
	mEntry->mExtents[0] = min;
	mEntry->mExtents[1] = max;
}

void LLViewerOctreeEntryData::setPositionGroup(const LLVector4a& pos)
{
	mEntry->mPositionGroup = pos;
}

const LLVector4a* LLViewerOctreeEntryData::getSpatialExtents() const
{
	return mEntry->getSpatialExtents();
}

//virtual
void LLViewerOctreeEntryData::setGroup(LLViewerOctreeGroup* group)
{
	mEntry->setGroup(group);
}

void LLViewerOctreeEntryData::shift(const LLVector4a& shift_vector)
{
	mEntry->mExtents[0].add(shift_vector);
	mEntry->mExtents[1].add(shift_vector);
	mEntry->mPositionGroup.add(shift_vector);
}

LLViewerOctreeGroup* LLViewerOctreeEntryData::getGroup()const
{
	return mEntry.notNull() ? mEntry->mGroup : NULL;
}

const LLVector4a& LLViewerOctreeEntryData::getPositionGroup() const
{
	return mEntry->getPositionGroup();
}

//virtual
bool LLViewerOctreeEntryData::isVisible() const
{
	if (mEntry)
	{
		return mEntry->mVisible == sCurVisible;
	}
	return false;
}

//virtual
bool LLViewerOctreeEntryData::isRecentlyVisible() const
{
	if (!mEntry)
	{
		return false;
	}

	if (isVisible())
	{
		return true;
	}
	if (getGroup() && getGroup()->isRecentlyVisible())
	{
		setVisible();
		return true;
	}

	return false;
}

void LLViewerOctreeEntryData::setVisible() const
{
	if (mEntry)
	{
		mEntry->mVisible = sCurVisible;
	}
}

void LLViewerOctreeEntryData::resetVisible() const
{
	if (mEntry)
	{
		mEntry->mVisible = 0;
	}
}

//-----------------------------------------------------------------------------
// LLViewerOctreeGroup class
//-----------------------------------------------------------------------------

LLViewerOctreeGroup::LLViewerOctreeGroup(OctreeNode* node)
:	mOctreeNode(node),
	mAnyVisible(0),
	mState(CLEAN)
{
	LLVector4a tmp;
	tmp.splat(0.f);
	mExtents[0] = mExtents[1] = mObjectBounds[0] = mObjectBounds[1] =
				  mObjectExtents[0] = mObjectExtents[1] = tmp;

	mBounds[0] = node->getCenter();
	mBounds[1] = node->getSize();

	mOctreeNode->addListener(this);
}

bool LLViewerOctreeGroup::hasElement(LLViewerOctreeEntryData* data)
{
	if (!data->getEntry())
	{
		return false;
	}
	return std::find(getDataBegin(), getDataEnd(),
					 data->getEntry()) != getDataEnd();
}

bool LLViewerOctreeGroup::removeFromGroup(LLViewerOctreeEntryData* data)
{
	return removeFromGroup(data->getEntry());
}

bool LLViewerOctreeGroup::removeFromGroup(LLViewerOctreeEntry* entry)
{
	llassert(entry && !entry->getGroup());

	unbound();
	setState(OBJECT_DIRTY);

	if (isDead())
	{
		// Group is about to be destroyed: don't double delete the entry.
		entry->setBinIndex(-1);
		return true;
	}

	if (mOctreeNode)
	{
		// This could cause *this* pointer to be destroyed, so no more function
		// calls after this.
		if (!mOctreeNode->remove(entry))
		{
			llwarns_sparse << "Could not remove LLVOCacheEntry from LLVOCacheOctreeGroup"
						   << llendl;
			llassert(false);
			return false;
		}
	}

	return true;
}

//virtual
void LLViewerOctreeGroup::unbound()
{
	if (isDirty())
	{
		return;
	}

	setState(DIRTY);

	// All the parent nodes need to rebound this child
	if (mOctreeNode)
	{
		OctreeNode* parent = (OctreeNode*)mOctreeNode->getParent();
		while (parent)
		{
			LLViewerOctreeGroup* group =
				(LLViewerOctreeGroup*)parent->getListener(0);
			if (!group || group->isDirty())
			{
				return;
			}

			group->setState(DIRTY);
			parent = (OctreeNode*)parent->getParent();
		}
	}
}

//virtual
void LLViewerOctreeGroup::rebound()
{
	if (!isDirty())
	{
		return;
	}

	if (mOctreeNode->getChildCount() == 1 &&
		mOctreeNode->getElementCount() == 0)
	{
		LLViewerOctreeGroup* group =
			(LLViewerOctreeGroup*)mOctreeNode->getChild(0)->getListener(0);
		if (!group)
		{
			llwarns_sparse << "NULL group found !  Cannot rebound." << llendl;
			llassert(false);
			return;
		}

		group->rebound();

		// Copy single child's bounding box
		mBounds[0] = group->mBounds[0];
		mBounds[1] = group->mBounds[1];
		mExtents[0] = group->mExtents[0];
		mExtents[1] = group->mExtents[1];

		group->setState(SKIP_FRUSTUM_CHECK);
	}
	else if (!mOctreeNode->getChildCount())
	{
		// Copy object bounding box if this is a leaf
		boundObjects(true, mExtents[0], mExtents[1]);
		mBounds[0] = mObjectBounds[0];
		mBounds[1] = mObjectBounds[1];
	}
	else
	{
		LLVector4a& new_min = mExtents[0];
		LLVector4a& new_max = mExtents[1];

		LLViewerOctreeGroup* group =
			(LLViewerOctreeGroup*)mOctreeNode->getChild(0)->getListener(0);
		if (!group)
		{
			llwarns_sparse << "NULL group found !  Cannot rebound." << llendl;
			llassert(false);
			return;
		}
		group->clearState(SKIP_FRUSTUM_CHECK);
		group->rebound();

		// Initialize to first child
		new_min = group->mExtents[0];
		new_max = group->mExtents[1];

		// First, rebound children
		for (U32 i = 1; i < mOctreeNode->getChildCount(); ++i)
		{
			group =
				(LLViewerOctreeGroup*)mOctreeNode->getChild(i)->getListener(0);
			if (!group) continue;

			group->clearState(SKIP_FRUSTUM_CHECK);
			group->rebound();

			const LLVector4a& max = group->mExtents[1];
			const LLVector4a& min = group->mExtents[0];
			new_max.setMax(new_max, max);
			new_min.setMin(new_min, min);
		}

		boundObjects(false, new_min, new_max);

		mBounds[0].setAdd(new_min, new_max);
		mBounds[0].mul(0.5f);
		mBounds[1].setSub(new_max, new_min);
		mBounds[1].mul(0.5f);
	}

	clearState(DIRTY);

	return;
}

//virtual
void LLViewerOctreeGroup::handleInsertion(const TreeNode* node,
										  LLViewerOctreeEntry* obj)
{
	obj->setGroup(this);
	unbound();
	setState(OBJECT_DIRTY);
}

//virtual
void LLViewerOctreeGroup::handleRemoval(const TreeNode* node,
										LLViewerOctreeEntry* obj)
{
	unbound();
	setState(OBJECT_DIRTY);

	// This could cause *this* pointer to be destroyed. So no more function
	// calls after this.
	obj->setGroup(NULL);
}

//virtual
void LLViewerOctreeGroup::handleDestruction(const TreeNode* node)
{
	if (isDead())
	{
		return;
	}
	setState(DEAD);

	for (OctreeNode::element_iter i = mOctreeNode->getDataBegin(),
								  end = mOctreeNode->getDataEnd();
		 i != end; ++i)
	{
		LLViewerOctreeEntry* obj = *i;
		if (obj && obj->getGroup() == this)
		{
			obj->nullGroup();
		}
	}
	mOctreeNode = NULL;
}

//virtual
void LLViewerOctreeGroup::handleStateChange(const TreeNode* node)
{
	// Drop bounding box upon state change
	if (mOctreeNode != node)
	{
		mOctreeNode = (OctreeNode*)node;
	}
	unbound();
}

//virtual
void LLViewerOctreeGroup::handleChildAddition(const OctreeNode* parent,
											  OctreeNode* child)
{
	if (child->getListenerCount() == 0)
	{
		new LLViewerOctreeGroup(child);
	}
	else
	{
		llwarns_sparse << "LLViewerOctreeGroup redundancy detected." << llendl;
		llassert(false);
	}

	unbound();

	LLViewerOctreeGroup* group = (LLViewerOctreeGroup*)child->getListener(0);
	if (group)
	{
		group->unbound();
	}
}

//virtual
void LLViewerOctreeGroup::handleChildRemoval(const OctreeNode* parent,
											 const OctreeNode* child)
{
	unbound();
}

LLViewerOctreeGroup* LLViewerOctreeGroup::getParent()
{
	if (isDead() || !mOctreeNode)
	{
		return NULL;
	}

	OctreeNode* parent = mOctreeNode->getOctParent();
	if (parent)
	{
		return (LLViewerOctreeGroup*)parent->getListener(0);
	}

	return NULL;
}

//virtual
bool LLViewerOctreeGroup::boundObjects(bool empty, LLVector4a& min_out,
									   LLVector4a& max_out)
{
	const OctreeNode* node = mOctreeNode;

	if (node->isEmpty())
	{
		// Do not do anything if there is no object
		if (empty && mOctreeNode->getParent())
		{
			// Only root is allowed to be empty
			llwarns_sparse << "Empty leaf found in octree." << llendl;
			llassert(false);
		}
		return false;
	}

	LLVector4a& new_min = mObjectExtents[0];
	LLVector4a& new_max = mObjectExtents[1];

	if (hasState(OBJECT_DIRTY))
	{
		// Calculate new bounding box
		clearState(OBJECT_DIRTY);

		// Initialize bounding box to first element
		OctreeNode::const_element_iter i = node->getDataBegin();
		LLViewerOctreeEntry* entry = *i;
		const LLVector4a* minMax = entry->getSpatialExtents();

		new_min = minMax[0];
		new_max = minMax[1];

		for (++i; i != node->getDataEnd(); ++i)
		{
			entry = *i;
			minMax = entry->getSpatialExtents();

			update_min_max(new_min, new_max, minMax[0]);
			update_min_max(new_min, new_max, minMax[1]);
		}

		mObjectBounds[0].setAdd(new_min, new_max);
		mObjectBounds[0].mul(0.5f);
		mObjectBounds[1].setSub(new_max, new_min);
		mObjectBounds[1].mul(0.5f);
	}

	if (empty)
	{
		min_out = new_min;
		max_out = new_max;
	}
	else
	{
		min_out.setMin(min_out, new_min);
		max_out.setMax(max_out, new_max);
	}

	return true;
}

//virtual
bool LLViewerOctreeGroup::isVisible() const
{
	return mVisible[LLViewerCamera::sCurCameraID] >=
			LLViewerOctreeEntryData::getCurrentFrame();
}

void LLViewerOctreeGroup::setVisible()
{
	mVisible[LLViewerCamera::sCurCameraID] =
		LLViewerOctreeEntryData::getCurrentFrame();

	if (LLViewerCamera::sCurCameraID < LLViewerCamera::CAMERA_WATER0)
	{
		mAnyVisible = LLViewerOctreeEntryData::getCurrentFrame();
	}
}

//-----------------------------------------------------------------------------
// Occulsion culling functions and classes
//-----------------------------------------------------------------------------

//static
U32 LLOcclusionCullingGroup::sOcclusionTimeouts = 0;

#define QUERY_POOL_SIZE 1024
static std::queue<GLuint> sFreeQueries;

U32 LLOcclusionCullingGroup::getNewOcclusionQueryObjectName()
{
	if (sFreeQueries.empty())
	{
		GLuint queries[QUERY_POOL_SIZE];
		glGenQueries(1024, queries);
		for (S32 i = 0; i < 1024; ++i)
		{
			sFreeQueries.push(queries[i]);
		}
	}

	// Pull from pool
	GLuint ret = sFreeQueries.front();
	sFreeQueries.pop();
	return ret;
}

void LLOcclusionCullingGroup::releaseOcclusionQueryObjectName(GLuint name)
{
	if (name)
	{
		sFreeQueries.push(name);
	}
}

//=====================================
//		Occlusion State Set/Clear
//=====================================
class LLSpatialSetOcclusionState : public OctreeTraveler
{
public:
	LLSpatialSetOcclusionState(U32 state) : mState(state) {}

	void visit(const OctreeNode* branch) override
	{
		LLOcclusionCullingGroup* group =
			(LLOcclusionCullingGroup*)branch->getListener(0);
		if (group)
		{
			group->setOcclusionState(mState);
		}
	}

public:
	U32 mState;
};

class LLSpatialSetOcclusionStateDiff final : public LLSpatialSetOcclusionState
{
public:
	LLSpatialSetOcclusionStateDiff(U32 state)
	:	LLSpatialSetOcclusionState(state)
	{
	}

	void traverse(const OctreeNode* n) override
	{
		LLOcclusionCullingGroup* group =
			(LLOcclusionCullingGroup*)n->getListener(0);
		if (group && !group->isOcclusionState(mState))
		{
			OctreeTraveler::traverse(n);
		}
	}
};

LLOcclusionCullingGroup::LLOcclusionCullingGroup(OctreeNode* node,
												 LLViewerOctreePartition* part)
:	LLViewerOctreeGroup(node),
	mSpatialPartition(part)
{
	part->mLODSeed = (part->mLODSeed + 1) % part->mLODPeriod;
	mLODHash = part->mLODSeed;

	LLOcclusionCullingGroup* parent = NULL;
	OctreeNode* oct_parent = node->getOctParent();
	if (oct_parent)
	{
		parent = (LLOcclusionCullingGroup*)oct_parent->getListener(0);
	}

	for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
	{
		mOcclusionQuery[i] = 0;
		mOcclusionChecks[i] = 0;
		mOcclusionIssued[i] = 0;
		mOcclusionState[i] = parent ? SG_STATE_INHERIT_MASK &
									  parent->mOcclusionState[i]
									: 0;
		mVisible[i] = 0;
	}
}

LLOcclusionCullingGroup::~LLOcclusionCullingGroup()
{
	releaseOcclusionQueryObjectNames();
}

bool LLOcclusionCullingGroup::needsUpdate()
{
	return mSpatialPartition &&
		   (S32)(LLViewerOctreeEntryData::getCurrentFrame() %
				 mSpatialPartition->mLODPeriod) == mLODHash;
}

bool LLOcclusionCullingGroup::isRecentlyVisible() const
{
	constexpr S32 MIN_VIS_FRAME_RANGE = 2;
	return LLViewerOctreeEntryData::getCurrentFrame() -
		   mVisible[LLViewerCamera::sCurCameraID] < MIN_VIS_FRAME_RANGE;
}

bool LLOcclusionCullingGroup::isAnyRecentlyVisible() const
{
	constexpr S32 MIN_VIS_FRAME_RANGE = 2;
	return LLViewerOctreeEntryData::getCurrentFrame() -
		   mAnyVisible < MIN_VIS_FRAME_RANGE;
}

//virtual
void LLOcclusionCullingGroup::handleChildAddition(const OctreeNode* parent,
												  OctreeNode* child)
{
	if (child->getListenerCount() == 0)
	{
		new LLOcclusionCullingGroup(child, mSpatialPartition);
	}
	else
	{
		llwarns_sparse << "LLOcclusionCullingGroup redundancy detected."
					   << llendl;
		llassert(false);
	}

	unbound();

	LLViewerOctreeGroup* group = (LLViewerOctreeGroup*)child->getListener(0);
	if (group)
	{
		group->unbound();
	}
}

void LLOcclusionCullingGroup::releaseOcclusionQueryObjectNames()
{
	for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
	{
		if (mOcclusionQuery[i])
		{
			releaseOcclusionQueryObjectName(mOcclusionQuery[i]);
			mOcclusionQuery[i] = 0;
		}
	}
}

void LLOcclusionCullingGroup::setOcclusionState(U32 state, S32 mode)
{
	switch (mode)
	{
		case STATE_MODE_SINGLE:
		{
			mOcclusionState[LLViewerCamera::sCurCameraID] |= state;
			GLuint query = mOcclusionQuery[LLViewerCamera::sCurCameraID];
			if (query && (state & DISCARD_QUERY))
			{
				releaseOcclusionQueryObjectName(query);
				mOcclusionQuery[LLViewerCamera::sCurCameraID] = 0;
			}
			break;
		}

		case STATE_MODE_BRANCH:
		{
			if (mOctreeNode)
			{
				LLSpatialSetOcclusionState setter(state);
				setter.traverse(mOctreeNode);
			}
			break;
		}

		case STATE_MODE_DIFF:
		{
			if (mOctreeNode)
			{
				LLSpatialSetOcclusionStateDiff setter(state);
				setter.traverse(mOctreeNode);
			}
			break;
		}

		default:	// STATE_MODE_ALL_CAMERAS
		{
			for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
			{
				mOcclusionState[i] |= state;

				if ((state & DISCARD_QUERY) != 0 && mOcclusionQuery[i])
				{
					releaseOcclusionQueryObjectName(mOcclusionQuery[i]);
					mOcclusionQuery[i] = 0;
				}
			}
		}
	}
}

class LLSpatialClearOcclusionState : public OctreeTraveler
{
public:
	LLSpatialClearOcclusionState(U32 state)
	:	mState(state)
	{
	}

	void visit(const OctreeNode* branch) override
	{
		LLOcclusionCullingGroup* group =
			(LLOcclusionCullingGroup*)branch->getListener(0);
		if (group)
		{
			group->clearOcclusionState(mState);
		}
	}

public:
	U32 mState;
};

class LLSpatialClearOcclusionStateDiff final
:	public LLSpatialClearOcclusionState
{
public:
	LLSpatialClearOcclusionStateDiff(U32 state)
	:	LLSpatialClearOcclusionState(state)
	{
	}

	void traverse(const OctreeNode* n) override
	{
		LLOcclusionCullingGroup* group =
			(LLOcclusionCullingGroup*)n->getListener(0);
		if (group && group->isOcclusionState(mState))
		{
			OctreeTraveler::traverse(n);
		}
	}
};

void LLOcclusionCullingGroup::clearOcclusionState(U32 state, S32 mode)
{
	switch (mode)
	{
		case STATE_MODE_SINGLE:
		{
			mOcclusionState[LLViewerCamera::sCurCameraID] &= ~state;
			break;
		}

		case STATE_MODE_BRANCH:
		{
			if (mOctreeNode)
			{
				LLSpatialClearOcclusionState clearer(state);
				clearer.traverse(mOctreeNode);
			}
			break;
		}

		case STATE_MODE_DIFF:
		{
			if (mOctreeNode)
			{
				LLSpatialClearOcclusionStateDiff clearer(state);
				clearer.traverse(mOctreeNode);
			}
			break;
		}

		default:	// STATE_MODE_ALL_CAMERAS
		{
			for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
			{
				mOcclusionState[i] &= ~state;
			}
		}
	}
}

bool LLOcclusionCullingGroup::earlyFail(LLCamera* camera,
										const LLVector4a* bounds)
{
	if (camera->getOrigin().isExactlyZero())
	{
		return false;
	}

	constexpr F32 vel = SG_OCCLUSION_FUDGE * 2.f;
	LLVector4a fudge;
	fudge.splat(vel);

	const LLVector4a& c = bounds[0];
	LLVector4a r;
	r.setAdd(bounds[1], fudge);

#if 0
	if (r.lengthSquared() > 1024.0 * 1024.0)
	{
		return true;
	}
#endif

	LLVector4a e;
	e.load3(camera->getOrigin().mV);

	LLVector4a min;
	min.setSub(c, r);
	LLVector4a max;
	max.setAdd(c, r);

	S32 lt = e.lessThan(min).getGatheredBits() & 0x7;
	if (lt)
	{
		return false;
	}

	S32 gt = e.greaterThan(max).getGatheredBits() & 0x7;
	if (gt)
	{
		return false;
	}

	return true;
}

U32 LLOcclusionCullingGroup::getLastOcclusionIssuedTime()
{
	return mOcclusionIssued[LLViewerCamera::sCurCameraID];
}

void LLOcclusionCullingGroup::checkOcclusion()
{
	if (!mSpatialPartition || LLPipeline::sUseOcclusion <= 1)
	{
		return;
	}

	LL_FAST_TIMER(FTM_OCCLUSION_READBACK);

	LLOcclusionCullingGroup* parent = (LLOcclusionCullingGroup*)getParent();
	if (parent && parent->isOcclusionState(OCCLUDED))
	{
		// If the parent has been marked as occluded, the child is implicitly
		// occluded
		clearOcclusionState(QUERY_PENDING | DISCARD_QUERY);
		return;
	}

	GLuint query = mOcclusionQuery[LLViewerCamera::sCurCameraID];
	if (query && isOcclusionState(QUERY_PENDING))
	{
		if (isOcclusionState(DISCARD_QUERY))
		{
			// Delete the query to avoid holding onto hundreds of pending
			// queries
			releaseOcclusionQueryObjectName(query);
			mOcclusionQuery[LLViewerCamera::sCurCameraID] = 0;
			// Mark as not occluded
			clearOcclusionState(OCCLUDED, STATE_MODE_DIFF);
			clearOcclusionState(QUERY_PENDING | DISCARD_QUERY);
			return;
		}

		// Otherwise a query is pending; read it back
		GLuint available = 0;
		glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
		// If the result is not available, wait until next frame, but count the
		// number of frames we wait and timeout when above the configured
		// limit...
		static LLCachedControl<U32> max_count(gSavedSettings,
											  "RenderOcclusionTimeout");
		if (!available &&
			mOcclusionChecks[LLViewerCamera::sCurCameraID] < (U32)max_count)
		{
			++mOcclusionChecks[LLViewerCamera::sCurCameraID];
			return;
		}
		if (!available)
		{
			++sOcclusionTimeouts;
		}
		mOcclusionChecks[LLViewerCamera::sCurCameraID] = 0;

		// Read back the result
		GLuint res = 0;
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &res);
		if (res > 0)
		{
			clearOcclusionState(OCCLUDED, STATE_MODE_DIFF);
		}
		else
		{
			setOcclusionState(OCCLUDED, STATE_MODE_DIFF);
		}
		clearOcclusionState(QUERY_PENDING);
	}
	else if (!gUsePBRShaders && mSpatialPartition->isOcclusionEnabled() &&
			 isOcclusionState(OCCLUDED))
	{
		// Check occlusion has been issued for occluded node that has not had a
		// query issued
		clearOcclusionState(OCCLUDED, STATE_MODE_DIFF);
	}
}

void LLOcclusionCullingGroup::doOcclusion(LLCamera* camera,
										  const LLVector4a* shift)
{
	if (!mSpatialPartition || !mSpatialPartition->isOcclusionEnabled() ||
		LLPipeline::sUseOcclusion <= 1)
	{
		return;
	}

	// Move mBounds to the agent space if necessary
	LLVector4a bounds[2];
	bounds[0] = mBounds[0];
	bounds[1] = mBounds[1];
	if (shift)
	{
		bounds[0].add(*shift);
	}

	// Do not cull hole/edge water, unless we have the GL_ARB_depth_clamp
	// extension
	if (earlyFail(camera, bounds))
	{
		LL_FAST_TIMER(FTM_OCCLUSION_EARLY_FAIL);
		setOcclusionState(DISCARD_QUERY);
		clearOcclusionState(OCCLUDED, STATE_MODE_DIFF);
		return;
	}

	if (isOcclusionState(QUERY_PENDING) && !isOcclusionState(DISCARD_QUERY))
	{
		return;
	}

	// No query pending, or previous query to be discarded
	{
		LL_FAST_TIMER(FTM_RENDER_OCCLUSION);

		GLuint query = mOcclusionQuery[LLViewerCamera::sCurCameraID];
		if (!query)
		{
			LL_FAST_TIMER(FTM_OCCLUSION_ALLOCATE);
			query = getNewOcclusionQueryObjectName();
			mOcclusionQuery[LLViewerCamera::sCurCameraID] = query;
		}

		U32 type = mSpatialPartition->mDrawableType;

		// Depth clamp all water to avoid it being culled as a result of being
		// behind the far clip plane, and in the case of edge water to avoid it
		// being culled while still visible.
		bool use_depth_clamp = gGLManager.mHasDepthClamp &&
							   (type == LLPipeline::RENDER_TYPE_WATER ||
								type == LLPipeline::RENDER_TYPE_VOIDWATER);

		LLGLEnable clamp(use_depth_clamp ? GL_DEPTH_CLAMP : 0);

		U32 mode = gGLManager.mHasOcclusionQuery2 ? GL_ANY_SAMPLES_PASSED
												  : GL_SAMPLES_PASSED;
		{
			LL_FAST_TIMER(FTM_PUSH_OCCLUSION_VERTS);

			// Store which frame this query was issued on
			mOcclusionIssued[LLViewerCamera::sCurCameraID] = gFrameCount;

			{
				LL_FAST_TIMER(FTM_OCCLUSION_BEGIN_QUERY);
				 // Get an occlusion query that has not been used in a while
				releaseOcclusionQueryObjectName(query);
				query = getNewOcclusionQueryObjectName();
				mOcclusionQuery[LLViewerCamera::sCurCameraID] = query;
				glBeginQuery(mode, query);
			}

			LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
			if (shader)	// Paranoia
			{
				shader->uniform3fv(LLShaderMgr::BOX_CENTER, 1,
								   bounds[0].getF32ptr());

				F32 fudge_z;
				if (type == LLPipeline::RENDER_TYPE_VOIDWATER)
				{
					fudge_z = 1.f;
				}
				else
				{
					fudge_z = SG_OCCLUSION_FUDGE;
				}
				shader->uniform3f(LLShaderMgr::BOX_SIZE,
								  bounds[1][0] + SG_OCCLUSION_FUDGE,
								  bounds[1][1] + SG_OCCLUSION_FUDGE,
								  bounds[1][2] + fudge_z);
			}

			if (!use_depth_clamp && type == LLPipeline::RENDER_TYPE_VOIDWATER)
			{
				LL_FAST_TIMER(FTM_OCCLUSION_DRAW_WATER);

				LLGLSquashToFarClip squash;
				if (camera->getOrigin().isExactlyZero())
				{
					// Origin is invalid, draw entire box
					gPipeline.mCubeVB->drawRange(LLRender::TRIANGLE_FAN,
												 0, 7, 8, 0);
					gPipeline.mCubeVB->drawRange(LLRender::TRIANGLE_FAN,
												 0, 7, 8, b111 * 8);
				}
				else
				{
					gPipeline.mCubeVB->drawRange(LLRender::TRIANGLE_FAN,
												 0, 7, 8,
												 get_box_fan_indices(camera,
																	 bounds[0]));
				}
			}
			else
			{
				LL_FAST_TIMER(FTM_OCCLUSION_DRAW);

				if (camera->getOrigin().isExactlyZero())
				{
					// Origin is invalid, draw entire box
					gPipeline.mCubeVB->drawRange(LLRender::TRIANGLE_FAN,
												 0, 7, 8, 0);
					gPipeline.mCubeVB->drawRange(LLRender::TRIANGLE_FAN,
												 0, 7, 8, b111 * 8);
				}
				else
				{
					gPipeline.mCubeVB->drawRange(LLRender::TRIANGLE_FAN,
												 0, 7, 8,
												 get_box_fan_indices(camera,
																	 bounds[0]));
				}
			}

			{
				LL_FAST_TIMER(FTM_OCCLUSION_END_QUERY);
				glEndQuery(mode);
			}
		}
	}

	{
		LL_FAST_TIMER(FTM_SET_OCCLUSION_STATE);
		setOcclusionState(QUERY_PENDING);
		clearOcclusionState(DISCARD_QUERY);
	}
}

//-----------------------------------------------------------------------------
// LLViewerOctreePartition class
//-----------------------------------------------------------------------------

LLViewerOctreePartition::LLViewerOctreePartition()
:	mRegionp(NULL),
	mPartitionType(LLViewerRegion::PARTITION_NONE),
	mDrawableType(0),
	mOcclusionEnabled(true),
	mLODSeed(0),
	mLODPeriod(1)
{
	LLVector4a center, size;
	center.splat(0.f);
	size.splat(1.f);

	mOctree = new OctreeRoot(center, size, NULL);
}

LLViewerOctreePartition::~LLViewerOctreePartition()
{
	cleanup();
}

void LLViewerOctreePartition::cleanup()
{
	if (mOctree)
	{
		delete mOctree;
		mOctree = NULL;
	}
}

bool LLViewerOctreePartition::isOcclusionEnabled()
{
	return mOcclusionEnabled || LLPipeline::sUseOcclusion > 2;
}

//-----------------------------------------------------------------------------
// LLViewerOctreeCull class
//-----------------------------------------------------------------------------

//virtual
bool LLViewerOctreeCull::earlyFail(LLViewerOctreeGroup* group)
{
	return false;
}

//virtual
void LLViewerOctreeCull::traverse(const OctreeNode* n)
{
	if (!n)
	{
		llwarns_sparse << "NULL node was passed !  Skipping..." << llendl;
		llassert(false);
		return;
	}

	LLViewerOctreeGroup* group = (LLViewerOctreeGroup*)n->getListener(0);
	if (!group)
	{
		llwarns_once << "NULL spatial group for octree node "
					 << n << " !  Skipping..." << llendl;
		llassert(false);
		return;
	}

	if (earlyFail(group))
	{
		return;
	}

	if (mRes == 2 ||
		(mRes && group->hasState(LLViewerOctreeGroup::SKIP_FRUSTUM_CHECK)))
	{
		// Fully in, just add everything
		OctreeTraveler::traverse(n);
	}
	else
	{
		mRes = frustumCheck(group);

		if (mRes)
		{
			// At least partially in, run on down
			OctreeTraveler::traverse(n);
		}

		mRes = 0;
	}
}

//------------------------------------------
// Agent space group culling
//------------------------------------------

S32 LLViewerOctreeCull::AABBInFrustumNoFarClipGroupBounds(const LLViewerOctreeGroup* group)
{
	return mCamera->AABBInFrustumNoFarClip(group->mBounds[0],
										   group->mBounds[1]);
}

S32 LLViewerOctreeCull::AABBSphereIntersectGroupExtents(const LLViewerOctreeGroup* group)
{
	return AABBSphereIntersect(group->mExtents[0], group->mExtents[1],
							   mCamera->getOrigin(),
							   mCamera->mFrustumCornerDist);
}

S32 LLViewerOctreeCull::AABBInFrustumGroupBounds(const LLViewerOctreeGroup* group)
{
	return mCamera->AABBInFrustum(group->mBounds[0], group->mBounds[1]);
}

//------------------------------------------
// Agent space object set culling
//------------------------------------------

S32 LLViewerOctreeCull::AABBInFrustumNoFarClipObjectBounds(const LLViewerOctreeGroup* group)
{
	return mCamera->AABBInFrustumNoFarClip(group->mObjectBounds[0],
										   group->mObjectBounds[1]);
}

S32 LLViewerOctreeCull::AABBSphereIntersectObjectExtents(const LLViewerOctreeGroup* group)
{
	return AABBSphereIntersect(group->mObjectExtents[0],
							   group->mObjectExtents[1],
							   mCamera->getOrigin(),
							   mCamera->mFrustumCornerDist);
}

S32 LLViewerOctreeCull::AABBInFrustumObjectBounds(const LLViewerOctreeGroup* group)
{
	return mCamera->AABBInFrustum(group->mObjectBounds[0],
								  group->mObjectBounds[1]);
}

//------------------------------------------
// Local regional space group culling
//------------------------------------------

S32 LLViewerOctreeCull::AABBInRegionFrustumNoFarClipGroupBounds(const LLViewerOctreeGroup* group)
{
	return mCamera->AABBInRegionFrustumNoFarClip(group->mBounds[0],
												 group->mBounds[1]);
}

S32 LLViewerOctreeCull::AABBInRegionFrustumGroupBounds(const LLViewerOctreeGroup* group)
{
	return mCamera->AABBInRegionFrustum(group->mBounds[0], group->mBounds[1]);
}

S32 LLViewerOctreeCull::AABBRegionSphereIntersectGroupExtents(const LLViewerOctreeGroup* group,
															  const LLVector3& shift)
{
	return AABBSphereIntersect(group->mExtents[0], group->mExtents[1],
							   mCamera->getOrigin() - shift,
							   mCamera->mFrustumCornerDist);
}

//------------------------------------------
// Local regional space object culling
//------------------------------------------

S32 LLViewerOctreeCull::AABBInRegionFrustumObjectBounds(const LLViewerOctreeGroup* group)
{
	return mCamera->AABBInRegionFrustum(group->mObjectBounds[0],
										group->mObjectBounds[1]);
}

S32 LLViewerOctreeCull::AABBInRegionFrustumNoFarClipObjectBounds(const LLViewerOctreeGroup* group)
{
	return mCamera->AABBInRegionFrustumNoFarClip(group->mObjectBounds[0],
												 group->mObjectBounds[1]);
}

S32 LLViewerOctreeCull::AABBRegionSphereIntersectObjectExtents(const LLViewerOctreeGroup* group,
															   const LLVector3& shift)
{
	return AABBSphereIntersect(group->mObjectExtents[0],
							   group->mObjectExtents[1],
							   mCamera->getOrigin() - shift,
							   mCamera->mFrustumCornerDist);
}

//------------------------------------------

// Check if the objects projection large enough
bool LLViewerOctreeCull::checkProjectionArea(const LLVector4a& center,
											 const LLVector4a& size,
											 const LLVector3& shift,
											 F32 pixel_threshold,
											 F32 near_radius)
{
	LLVector3 local_orig = mCamera->getOrigin() - shift;
	LLVector4a origin;
	origin.load3(local_orig.mV);

	LLVector4a lookAt;
	lookAt.setSub(center, origin);
	F32 distance = lookAt.getLength3().getF32();
	if (distance <= near_radius)
	{
		return true;	// always load close-by objects
	}

	// Treat object as if it were near_radius meters closer than it actually
	// is. This allows us to get some temporal coherence on visibility...
	// Objects that can be reached quickly will tend to be visible.
	distance -= near_radius;

	F32 squared_rad = size.dot3(size).getF32();
	return squared_rad / distance > pixel_threshold;
}

//virtual
bool LLViewerOctreeCull::checkObjects(const OctreeNode* branch,
									  const LLViewerOctreeGroup* group)
{
	if (branch->getElementCount() == 0)
	{
		// No element
		return false;
	}
	else if (branch->getChildCount() == 0)
	{
		// Leaf state, already checked tightest bounding box
		return true;
	}
	else if (mRes == 1 && !frustumCheckObjects(group))
	{
		// No object in frustum
		return false;
	}

	return true;
}

//virtual
void LLViewerOctreeCull::preprocess(LLViewerOctreeGroup* group)
{
}

//virtual
void LLViewerOctreeCull::processGroup(LLViewerOctreeGroup* group)
{
}

//virtual
void LLViewerOctreeCull::visit(const OctreeNode* branch)
{
	LLViewerOctreeGroup* group = (LLViewerOctreeGroup*)branch->getListener(0);
	if (group)
	{
		preprocess(group);
		if (checkObjects(branch, group))
		{
			processGroup(group);
		}
	}
}

//--------------------------------------------------------------
// LLViewerOctreeDebug class
//--------------------------------------------------------------

//virtual
void LLViewerOctreeDebug::visit(const OctreeNode* branch)
{
#if 0
	llinfos << "Node: " << (U32)branch << " - Elements: "
			<< branch->getElementCount() << " - Children: "
			<< branch->getChildCount();
	for (U32 i = 0, count = branch->getChildCount(); i < count; ++i)
	{
		llcont << "\n - Child " << i << " : " << (U32)branch->getChild(i);
	}
	llcont << llendl;
#endif
	LLViewerOctreeGroup* group = (LLViewerOctreeGroup*)branch->getListener(0);
	if (group)
	{
		processGroup(group);
	}
}

//virtual
void LLViewerOctreeDebug::processGroup(LLViewerOctreeGroup* group)
{
#if 0
	const LLVector4a* vec4 = group->getBounds();
	LLVector3 vec[2];
	vec[0].set(vec4[0].getF32ptr());
	vec[1].set(vec4[1].getF32ptr());
	llinfos << "Bounds: " << vec[0] << " : " << vec[1] << llendl;

	vec4 = group->getExtents();
	vec[0].set(vec4[0].getF32ptr());
	vec[1].set(vec4[1].getF32ptr());
	llinfos << "Extents: " << vec[0] << " : " << vec[1] << llendl;

	vec4 = group->getObjectBounds();
	vec[0].set(vec4[0].getF32ptr());
	vec[1].set(vec4[1].getF32ptr());
	llinfos << "ObjectBounds: " << vec[0] << " : " << vec[1] << llendl;

	vec4 = group->getObjectExtents();
	vec[0].set(vec4[0].getF32ptr());
	vec[1].set(vec4[1].getF32ptr());
	llinfos << "ObjectExtents: " << vec[0] << " : " << vec[1] << llendl;
#endif
}
