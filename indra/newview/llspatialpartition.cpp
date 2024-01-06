/**
 * @file llspatialpartition.cpp
 * @brief LLSpatialGroup class implementation and supporting functions
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

#include "llspatialpartition.h"

#include "llfasttimer.h"
#include "llglslshader.h"
#include "lloctree.h"
#include "llphysshapebuilderutil.h"
#include "llrender.h"
#include "llvolume.h"
#include "llvolumeoctree.h"
#include "hbxxh.h"

#include "llappviewer.h"
#include "llface.h"
#include "llfloatertools.h"
#include "llmeshrepository.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For gCubeSnapshot
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"			// For gDebugRaycast*
#include "llvoavatar.h"
#include "llvolumemgr.h"
#include "llvovolume.h"

U32 LLSpatialGroup::sNodeCount = 0;
bool LLSpatialGroup::sNoDelete = false;

U32 gOctreeMaxCapacity;
F32 gOctreeMinSize;
// Must be adjusted upwards for OpenSim grids to avoid the dreaded
// "Element exceeds range of spatial partition" issue on TPs and its
// consequences (crashes or massive occlusion issues).
LLVector4a gOctreeMaxMag(1024.f * 1024.f);

spatial_groups_set_t gVisibleSelectedGroups;

static F32 sLastMaxTexPriority = 1.f;
static F32 sCurMaxTexPriority = 1.f;

bool LLSpatialPartition::sTeleportRequested = false;

// Returns:
//	0 if sphere and AABB are not intersecting
//	1 if they are
//	2 if AABB is entirely inside sphere
S32 LLSphereAABB(const LLVector3& center, const LLVector3& size,
				 const LLVector3& pos, const F32& rad)
{
	S32 ret = 2;

	LLVector3 min = center - size;
	LLVector3 max = center + size;
	for (U32 i = 0; i < 3; ++i)
	{
		if (min.mV[i] > pos.mV[i] + rad || max.mV[i] < pos.mV[i] - rad)
		{
			// Totally outside
			return 0;
		}

		if (min.mV[i] < pos.mV[i] - rad || max.mV[i] > pos.mV[i] + rad)
		{
			// Intersecting
			ret = 1;
		}
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
// LLOctreeIntersect class
///////////////////////////////////////////////////////////////////////////////

class alignas(16) LLOctreeIntersect final
:	public LLOctreeTraveler<LLViewerOctreeEntry>
{
protected:
	LOG_CLASS(LLOctreeIntersect);

public:
	LLOctreeIntersect(const LLVector4a& start, const LLVector4a& end,
					  bool pick_transparent, bool pick_rigged, S32* face_hit,
					  LLVector4a* intersection, LLVector2* tex_coord,
					  LLVector4a* normal, LLVector4a* tangent)
	:	mStart(start),
		mEnd(end),
		mFaceHit(face_hit),
		mIntersection(intersection),
		mTexCoord(tex_coord),
		mNormal(normal),
		mTangent(tangent),
		mHit(NULL),
		mPickTransparent(pick_transparent),
		mPickRigged(pick_rigged)
	{
	}

	void visit(const OctreeNode* branchp) override
	{
		for (OctreeNode::const_element_iter it = branchp->getDataBegin(),
											end = branchp->getDataEnd();
			 it != end; ++it)
		{
			check(*it);
		}
	}

	LLDrawable* check(const OctreeNode* nodep)
	{
		if (!nodep)
		{
			llwarns << "NULL node passed to LLOctreeIntersect::check()"
					<< llendl;
			return NULL;
		}

		nodep->accept(this);

		LLMatrix4a local_matrix4a;
		for (U32 i = 0; i < nodep->getChildCount(); ++i)
		{
			const OctreeNode* childp = nodep->getChild(i);
			if (!childp)
			{
				llwarns << "NULL spatial partition for node " << nodep
						<< llendl;
				continue;
			}

			LLSpatialGroup* groupp = (LLSpatialGroup*)childp->getListener(0);
			if (!groupp)
			{
				llwarns << "NULL spatial group for child " << childp
						<< " of node " << nodep << llendl;
				continue;
			}

			const LLVector4a* bounds = groupp->getBounds();
			LLVector4a size = bounds[1];
			LLVector4a center = bounds[0];

			LLVector4a local_start = mStart;
			LLVector4a local_end = mEnd;

			LLSpatialPartition* partp = groupp->getSpatialPartition();
			if (partp)
			{
				LLSpatialBridge* bridgep = partp->asBridge();
				if (bridgep)
				{
					LLDrawable* drawp = bridgep->mDrawable;
					if (drawp)
					{
						LLMatrix4 local_matrix = drawp->getRenderMatrix();
						local_matrix.invert();
						local_matrix4a.loadu(local_matrix);
						local_matrix4a.affineTransform(mStart, local_start);
						local_matrix4a.affineTransform(mEnd, local_end);
					}
					else
					{
						llwarns << "NULL drawable for spatial partition bridge of group "
								<< groupp << " of child " << childp
								<< " of node " << nodep << llendl;
					}
				}
			}
			else
			{
				llwarns << "NULL spatial partition for group " << groupp
						<< " of child " << childp << " of node " << nodep
						<< llendl;
			}

			if (LLLineSegmentBoxIntersect(local_start, local_end, center,
										  size))
			{
				check(childp);
			}
		}

		return mHit;
	}

	bool check(LLViewerOctreeEntry* entryp)
	{
		LLDrawable* drawp = (LLDrawable*)entryp->getDrawable();
		if (!drawp || !gPipeline.hasRenderType(drawp->getRenderType()) ||
			!drawp->isVisible())
		{
			return false;
		}

		if (drawp->isSpatialBridge())
		{
			LLSpatialPartition* partp = drawp->asPartition();
			if (partp)
			{
				LLSpatialBridge* bridgep = partp->asBridge();
				if (bridgep && gPipeline.hasRenderType(bridgep->mDrawableType))
				{
					check(partp->mOctree);
				}
			}
			else
			{
				llwarns << "NULL spatial partition for drawable " << drawp
						<< llendl;
			}

			return false;
		}

		LLViewerObject* vobjp = drawp->getVObj();
		if (!vobjp)
		{
			return false;
		}

		 // Forbid any interaction with HUDs when they are hidden. HB
		if (!LLPipeline::sShowHUDAttachments && vobjp->isHUDAttachment())
		{
			return false;
		}

		// The block of code below deals with selection behaviour changes when
		// the build floater is visible; some interactions are forbidden when
		// it is not. The "PickUnselectableInEdit" setting (non-persistent and
		// defaulting to TRUE) determines whether we do pick non-selectable
		// objects when the build floater is visible). HB
		static LLCachedControl<bool> edit_pick(gSavedSettings,
											   "PickUnselectableInEdit");
		bool not_building = !edit_pick || !LLFloaterTools::isVisible();
		// Forbid interaction when the build tools floater is not visible and
		// when this volume got an ignore click action set.
		if (not_building && vobjp->getClickAction() == CLICK_ACTION_IGNORE)
		{
			return false;
		}
		// Forbid interaction when this volume is a reflection probe and the
		// the build tools floater is not visible. I added this test here to
		// replace the 'pick_unselectable' hack as implemented in LL's PBR
		// viewer, which ruined it for us due to the differences between async
		// and sync picking in v1 and v2+ viewers; it is also much simpler this
		// way... HB
		if (not_building && vobjp->isReflectionProbe())
		{
			return false;
		}

		// We can interact with this volume: do check for intersection.

		LLVector4a intersection;
		bool skip_check = false;
		if (vobjp->isAvatar())
		{
			LLVOAvatar* avp = (LLVOAvatar*)vobjp;
			if (mPickRigged ||
				(avp->isSelf() && LLFloaterTools::isVisible()))
			{
				LLViewerObject* hitp =
					avp->lineSegmentIntersectRiggedAttachments(mStart, mEnd,
															   -1,
															   mPickTransparent,
															   mPickRigged,
															   mFaceHit,
															   &intersection,
															   mTexCoord,
															   mNormal,
															   mTangent);
				if (hitp)
				{
					mEnd = intersection;
					if (mIntersection)
					{
						*mIntersection = intersection;
					}
					mHit = hitp->mDrawable;
					skip_check = true;
				}
			}
		}
		if (!skip_check &&
			vobjp->lineSegmentIntersect(mStart, mEnd, -1, mPickTransparent,
										mPickRigged, mFaceHit, &intersection,
										mTexCoord, mNormal, mTangent))
		{
			// Shorten the ray so we only find CLOSER hits:
			mEnd = intersection;

			if (mIntersection)
			{
				*mIntersection = intersection;
			}
			mHit = vobjp->mDrawable;
		}

		return false;
	}

public:
	LLVector4a	mStart;
	LLVector4a	mEnd;
	S32*		mFaceHit;
	LLVector4a*	mIntersection;
	LLVector2*	mTexCoord;
	LLVector4a*	mNormal;
	LLVector4a*	mTangent;
	LLDrawable*	mHit;
	bool		mPickTransparent;
	bool		mPickRigged;
};

///////////////////////////////////////////////////////////////////////////////

LLSpatialGroup::~LLSpatialGroup()
{
#if LL_DEBUG
	if (gDebugGL)
	{
# if 0	// Note that this might actually "normally" happen, if to judge from
		// LL's latest viewer code... HB
		if (sNoDelete)
		{
			llerrs << "Deleted while in sNoDelete mode !" << llendl;
		}
# endif
		gPipeline.checkReferences(this);
	}
#endif

	--sNodeCount;

	clearDrawMap();
}

void LLSpatialGroup::clearDrawMap()
{
	mDrawMap.clear();
}

bool LLSpatialGroup::isHUDGroup()
{
	if (isDead())
	{
		return false;
	}
	LLSpatialPartition* partp = getSpatialPartition();
	return partp && partp->isHUDPartition();
}

bool LLSpatialGroup::updateInGroup(LLDrawable* drawablep, bool immediate)
{
	if (!drawablep)
	{
		llwarns << "NULL drawable !" << llendl;
		return false;
	}

	drawablep->updateSpatialExtents();

	OctreeNode* parentp = mOctreeNode->getOctParent();

	if (mOctreeNode->isInside(drawablep->getPositionGroup()) &&
		(mOctreeNode->contains(drawablep->getEntry()) ||
		 (drawablep->getBinRadius() > mOctreeNode->getSize()[0] &&
		  parentp && parentp->getElementCount() >= gOctreeMaxCapacity)))
	{
		unbound();
		setState(OBJECT_DIRTY);
#if 0
		setState(GEOM_DIRTY);
#endif
		return true;
	}

	return false;
}

bool LLSpatialGroup::addObject(LLDrawable* drawablep)
{
	if (!drawablep)
	{
		return false;
	}

	drawablep->setGroup(this);
	setState(OBJECT_DIRTY | GEOM_DIRTY);
	setOcclusionState(DISCARD_QUERY, STATE_MODE_ALL_CAMERAS);
	gPipeline.markRebuild(this);
	if (drawablep->isSpatialBridge())
	{
		mBridgeList.push_back((LLSpatialBridge*)drawablep);
	}
	if (drawablep->getRadius() > 1.f)
	{
		setState(IMAGE_DIRTY);
	}

	return true;
}

void LLSpatialGroup::rebuildGeom()
{
	if (!isDead())
	{
		getSpatialPartition()->rebuildGeom(this);

		if (hasState(MESH_DIRTY))
		{
			gPipeline.markMeshDirty(this);
		}
	}
}

void LLSpatialGroup::rebuildMesh()
{
	if (!isDead())
	{
		getSpatialPartition()->rebuildMesh(this);
	}
}

void LLSpatialPartition::rebuildGeom(LLSpatialGroup* groupp)
{
	if (groupp->isDead() || !groupp->hasState(LLSpatialGroup::GEOM_DIRTY))
	{
		return;
	}

	if (groupp->changeLOD())
	{
		groupp->mLastUpdateDistance = groupp->mDistance;
		groupp->mLastUpdateViewAngle = groupp->mViewAngle;
	}

	LL_FAST_TIMER(FTM_REBUILD_VBO);

	groupp->clearDrawMap();

	// Get geometry count
	U32 index_count = 0;
	U32 vertex_count = 0;
	{
		LL_FAST_TIMER(FTM_ADD_GEOMETRY_COUNT);
		addGeometryCount(groupp, vertex_count, index_count);
	}

	LLPointer<LLVertexBuffer>& vb = groupp->mVertexBuffer;
	if (vertex_count > 0 && index_count > 0)
	{
		// Create vertex buffer containing volume geometry for this node
		{
			LL_FAST_TIMER(FTM_CREATE_VB);
			groupp->mBuilt = 1.f;
			if (vb.isNull() || vb->getNumVerts() != vertex_count ||
				vb->getNumIndices() != index_count)
			{
				vb = createVertexBuffer(mVertexDataMask);
				if (!vb->allocateBuffer(vertex_count, index_count))
				{
					llwarns << "Failure to allocate a vertex buffer with "
							<< vertex_count << " vertices and "
							<< index_count << " indices" << llendl;
					vb = NULL;
					groupp->mBufferMap.clear();
					groupp->mLastUpdateTime = gFrameTimeSeconds;
					groupp->clearState(LLSpatialGroup::GEOM_DIRTY);
					return;
				}
			}
		}

		{
			LL_FAST_TIMER(FTM_GET_GEOMETRY);
			getGeometry(groupp);
		}
	}
	else
	{
		vb = NULL;
		groupp->mBufferMap.clear();
	}

	groupp->mLastUpdateTime = gFrameTimeSeconds;
	groupp->clearState(LLSpatialGroup::GEOM_DIRTY);
}

LLSpatialGroup* LLSpatialGroup::getParent()
{
	return (LLSpatialGroup*)LLViewerOctreeGroup::getParent();
}

bool LLSpatialGroup::removeObject(LLDrawable* drawablep, bool from_octree)
{
	if (!drawablep)
	{
		return false;
	}

	unbound();

	if (mOctreeNode && !from_octree)
	{
		drawablep->setGroup(NULL);
		return true;
	}

	drawablep->setGroup(NULL);
	setState(GEOM_DIRTY);
	gPipeline.markRebuild(this);

	if (drawablep->isSpatialBridge())
	{
		for (bridge_list_t::iterator it = mBridgeList.begin(),
									 end = mBridgeList.end();
			 it != end; ++it)
		{
			if (*it == drawablep)
			{
				mBridgeList.erase(it);
				break;
			}
		}
	}

	if (isEmpty())
	{
		// Delete draw map on last element removal since a rebuild might never
		// happen.
		clearDrawMap();
	}

	return true;
}

void LLSpatialGroup::shift(const LLVector4a& offset)
{
	LLVector4a t = mOctreeNode->getCenter();
	t.add(offset);
	mOctreeNode->setCenter(t);
	mOctreeNode->updateMinMax();
	mBounds[0].add(offset);
	mExtents[0].add(offset);
	mExtents[1].add(offset);
	mObjectBounds[0].add(offset);
	mObjectExtents[0].add(offset);
	mObjectExtents[1].add(offset);

	LLSpatialPartition* partition = getSpatialPartition();
	if (!partition)
	{
		llwarns_sparse << "NULL octree partition !" << llendl;
		llassert(false);
		return;
	}

	U32 type = partition->mPartitionType;
	if (!partition->mRenderByGroup &&
		type != LLViewerRegion::PARTITION_TREE &&
		type != LLViewerRegion::PARTITION_TERRAIN &&
		type != LLViewerRegion::PARTITION_AVATAR &&
		type != LLViewerRegion::PARTITION_PUPPET &&
		type != LLViewerRegion::PARTITION_BRIDGE)
	{
		setState(GEOM_DIRTY);
		gPipeline.markRebuild(this);
	}
}

class LLSpatialSetState : public OctreeTraveler
{
public:
	LLSpatialSetState(U32 state)
	:	mState(state)
	{
	}

	void visit(const OctreeNode* branchp) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)branchp->getListener(0);
		if (groupp)
		{
			groupp->setState(mState);
		}
	}

public:
	U32 mState;
};

class LLSpatialSetStateDiff final : public LLSpatialSetState
{
public:
	LLSpatialSetStateDiff(U32 state)
	:	LLSpatialSetState(state)
	{
	}

	void traverse(const OctreeNode* nodep) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)nodep->getListener(0);
		if (groupp && !groupp->hasState(mState))
		{
			OctreeTraveler::traverse(nodep);
		}
	}
};

void LLSpatialGroup::setState(U32 state, S32 mode)
{
	llassert(state <= LLSpatialGroup::STATE_MASK);

	if (mode <= STATE_MODE_SINGLE)
	{
		mState |= state;
	}
	else if (mode == STATE_MODE_DIFF)
	{
		LLSpatialSetStateDiff setter(state);
		setter.traverse(mOctreeNode);
	}
	else
	{
		LLSpatialSetState setter(state);
		setter.traverse(mOctreeNode);
	}
}

class LLSpatialClearState : public OctreeTraveler
{
public:
	LLSpatialClearState(U32 state)
	:	mState(state)
	{
	}

	void visit(const OctreeNode* branchp) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)branchp->getListener(0);
		if (groupp)
		{
			groupp->clearState(mState);
		}
	}

public:
	U32 mState;
};

class LLSpatialClearStateDiff final : public LLSpatialClearState
{
public:
	LLSpatialClearStateDiff(U32 state)
	:	LLSpatialClearState(state)
	{
	}

	void traverse(const OctreeNode* nodep) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)nodep->getListener(0);
		if (groupp && groupp->hasState(mState))
		{
			OctreeTraveler::traverse(nodep);
		}
	}
};

void LLSpatialGroup::clearState(U32 state, S32 mode)
{
	llassert(state <= LLSpatialGroup::STATE_MASK);

	if (mode > STATE_MODE_SINGLE)
	{
		if (mode == STATE_MODE_DIFF)
		{
			LLSpatialClearStateDiff clearer(state);
			clearer.traverse(mOctreeNode);
		}
		else
		{
			LLSpatialClearState clearer(state);
			clearer.traverse(mOctreeNode);
		}
	}
	else
	{
		mState &= ~state;
	}
}

//======================================
//		Octree Listener Implementation
//======================================

LLSpatialGroup::LLSpatialGroup(OctreeNode* node, LLSpatialPartition* part)
:	LLOcclusionCullingGroup(node, part),
	mObjectBoxSize(1.f),
	mGeometryBytes(0),
	mSurfaceArea(0.f),
	mAvatarp(NULL),
	mRenderOrder(0),
	mBuilt(0.f),
	mVertexBuffer(NULL),
	mDistance(0.f),
	mDepth(0.f),
	mLastUpdateDistance(-1.f),
	mLastUpdateTime(gFrameTimeSeconds)
{
	ll_assert_aligned(this, 16);

	++sNodeCount;

	mViewAngle.splat(0.f);
	mLastUpdateViewAngle.splat(-1.f);

	setState(SG_INITIAL_STATE_MASK);
	gPipeline.markRebuild(this);

	// Let the reflection map manager know about this spatial group
	mReflectionProbe =
		gPipeline.mReflectionMapManager.registerSpatialGroup(this);

	mRadius = 1;
	mPixelArea = 1024.f;
}

void LLSpatialGroup::updateDistance(LLCamera &camera)
{
	if (LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
	{
		llwarns << "Attempted to update distance for camera other than world camera !"
				<< llendl;
		return;
	}

	if (gShiftFrame)
	{
		return;
	}

#if LL_DEBUG
	if (hasState(OBJECT_DIRTY))
	{
		llerrs << "Spatial group dirty on distance update." << llendl;
	}
#endif

	if (!isEmpty())
	{
		LLSpatialPartition* partition = getSpatialPartition();
		if (!partition)
		{
			llwarns_sparse << "NULL octree partition !" << llendl;
			llassert(false);
			return;
		}
		mRadius = partition->mRenderByGroup ? mObjectBounds[1].getLength3().getF32()
											: mOctreeNode->getSize().getLength3().getF32();
		mDistance = partition->calcDistance(this, camera);
		mPixelArea = partition->calcPixelArea(this, camera);
	}
}

F32 LLSpatialPartition::calcDistance(LLSpatialGroup* groupp, LLCamera& camera)
{
	LLVector4a eye;
	LLVector4a origin;
	origin.load3(camera.getOrigin().mV);
	eye.setSub(groupp->mObjectBounds[0], origin);

	F32 dist = 0.f;

	if (groupp->mDrawMap.find(LLRenderPass::PASS_ALPHA) !=
			groupp->mDrawMap.end())
	{
		LLVector4a v = eye;
		dist = eye.getLength3().getF32();
		eye.normalize3fast();

		if (!groupp->hasState(LLSpatialGroup::ALPHA_DIRTY))
		{
			if (!groupp->getSpatialPartition()->isBridge())
			{
				LLVector4a view_angle = eye;

				LLVector4a diff;
				diff.setSub(view_angle, groupp->mLastUpdateViewAngle);

				if (diff.getLength3().getF32() > 0.64f)
				{
					groupp->mViewAngle = view_angle;
					groupp->mLastUpdateViewAngle = view_angle;
					// For occasional alpha sorting within the group.
					// NOTE: If there is a trivial way to detect that alpha
					// sorting here would not change the render order, not
					// setting this node to dirty would be a very good thing.
					groupp->setState(LLSpatialGroup::ALPHA_DIRTY);
					gPipeline.markRebuild(groupp);
				}
			}
		}

		// Calculate depth of node for alpha sorting

		LLVector3 at = camera.getAtAxis();

		LLVector4a ata;
		ata.load3(at.mV);

		LLVector4a t = ata;
		// front of bounding box
		t.mul(0.25f);
		t.mul(groupp->mObjectBounds[1]);
		v.sub(t);

		groupp->mDepth = v.dot3(ata).getF32();
	}
	else
	{
		dist = eye.getLength3().getF32();
	}

	if (dist < 16.f)
	{
		dist /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}

	return dist;
}

F32 LLSpatialPartition::calcPixelArea(LLSpatialGroup* groupp, LLCamera& camera)
{
	return LLPipeline::calcPixelArea(groupp->mObjectBounds[0],
									 groupp->mObjectBounds[1], camera);
}

bool LLSpatialGroup::changeLOD()
{
	if (hasState(ALPHA_DIRTY | OBJECT_DIRTY))
	{
		// A rebuild is going to happen, update distance and LoD
		return true;
	}

	if (getSpatialPartition()->mSlopRatio > 0.f)
	{
		F32 ratio = (mDistance - mLastUpdateDistance) /
					 llmax(mLastUpdateDistance, mRadius);

		if (fabsf(ratio) >= getSpatialPartition()->mSlopRatio)
		{
			return true;
		}
	}

	return needsUpdate();
}

void LLSpatialGroup::handleInsertion(const TreeNode* nodep,
									 LLViewerOctreeEntry* entryp)
{
	if (!entryp)
	{
		llwarns << "Tried to insert a NULL drawable in node " << nodep
				<< llendl;
		llassert(false);
		return;
	}

	addObject((LLDrawable*)entryp->getDrawable());
	unbound();
	setState(OBJECT_DIRTY);
}

void LLSpatialGroup::handleRemoval(const TreeNode* nodep,
								   LLViewerOctreeEntry* entryp)
{
	if (!entryp)
	{
		llwarns << "Tried to remove a NULL drawable from node " << nodep
				<< llendl;
		llassert(false);
		return;
	}

	removeObject((LLDrawable*)entryp->getDrawable(), true);
	LLViewerOctreeGroup::handleRemoval(nodep, entryp);
}

void LLSpatialGroup::handleDestruction(const TreeNode* nodep)
{
	if (isDead())
	{
		return;
	}
	setState(DEAD);

	for (element_iter it = getDataBegin(); it != getDataEnd(); ++it)
	{
		LLViewerOctreeEntry* entryp = *it;
		if (entryp && entryp->getGroup() == this && entryp->hasDrawable())
		{
			((LLDrawable*)entryp->getDrawable())->setGroup(NULL);
		}
	}

	// Clean up avatar attachment stats
	LLSpatialBridge* bridgep = getSpatialPartition()->asBridge();
	if (bridgep && bridgep->mAvatar.notNull())
	{
		bridgep->mAvatar->subtractAttachmentBytes(mGeometryBytes);
		bridgep->mAvatar->subtractAttachmentArea(mSurfaceArea);
	}

	clearDrawMap();
	mVertexBuffer = NULL;
	mBufferMap.clear();
	mOctreeNode = NULL;
}

void LLSpatialGroup::handleChildAddition(const OctreeNode*,
										 OctreeNode* childp)
{
	if (!childp)
	{
		llwarns << "Attempted to add a NULL child node" << llendl;
		llassert(false);
		return;
	}

	if (childp->getListenerCount())
	{
		llwarns << "Group redundancy detected." << llendl;
		llassert(false);
		return;
	}

	new LLSpatialGroup(childp, getSpatialPartition());
	unbound();
}

void LLSpatialGroup::destroyGL(bool keep_occlusion)
{
	setState(GEOM_DIRTY | IMAGE_DIRTY);

	if (!keep_occlusion)
	{
		// Going to need a rebuild
		gPipeline.markRebuild(this);
	}

	mLastUpdateTime = gFrameTimeSeconds;
	mVertexBuffer = NULL;
	mBufferMap.clear();

	clearDrawMap();

	if (!keep_occlusion)
	{
		releaseOcclusionQueryObjectNames();
	}

	const element_list& element_vec = getData();
	for (U32 i = 0, count = element_vec.size(); i < count; ++i)
	{
		LLDrawable* drawp = (LLDrawable*)element_vec[i]->getDrawable();
		if (drawp)
		{
			for (U32 j = 0, count = drawp->getNumFaces(); j < count; ++j)
			{
				LLFace* facep = drawp->getFace(j);
				if (facep)
				{
					facep->clearVertexBuffer();
				}
			}
		}
	}
}

LLDrawable* LLSpatialGroup::lineSegmentIntersect(const LLVector4a& start,
												 const LLVector4a& end,
												 bool pick_transparent,
												 bool pick_rigged,
												 S32* face_hit,
												 LLVector4a* intersection,
												 LLVector2* tex_coord,
												 LLVector4a* normal,
												 LLVector4a* tangent)
{
	LLOctreeIntersect intersect(start, end, pick_transparent, pick_rigged,
								face_hit, intersection, tex_coord, normal,
								tangent);
	return intersect.check(getOctreeNode());
}

///////////////////////////////////////////////////////////////////////////////

LLSpatialPartition::LLSpatialPartition(U32 data_mask,
									   bool render_by_group,
									   LLViewerRegion* regionp)
:	mRenderByGroup(render_by_group),
	mBridge(NULL)
{
	mRegionp = regionp;
	mVertexDataMask = data_mask;
	mDepthMask = false;
	mSlopRatio = 0.25f;
	mInfiniteFarClip = false;

	new LLSpatialGroup(mOctree, this);
}

//virtual
LLSpatialPartition::~LLSpatialPartition()
{
	cleanup();
}

LLSpatialGroup* LLSpatialPartition::put(LLDrawable* drawablep,
										bool was_visible)
{
	drawablep->updateSpatialExtents();

	// Keep drawable from being garbage collected
	LLPointer<LLDrawable> ptr = drawablep;

	if (!drawablep->getGroup())
	{
		mOctree->insert(drawablep->getEntry());
	}

	LLSpatialGroup* groupp = drawablep->getSpatialGroup();
	if (groupp && was_visible &&
		groupp->isOcclusionState(LLSpatialGroup::QUERY_PENDING))
	{
		groupp->setOcclusionState(LLSpatialGroup::DISCARD_QUERY,
								  LLSpatialGroup::STATE_MODE_ALL_CAMERAS);
	}

	return groupp;
}

bool LLSpatialPartition::remove(LLDrawable* drawablep, LLSpatialGroup* curp)
{
	if (curp->removeObject(drawablep))
	{
		drawablep->setGroup(NULL);
		return true;
	}

	llwarns << "Failed to remove drawable from octree !" << llendl;
	llassert(false);
	return false;
}

void LLSpatialPartition::move(LLDrawable* drawablep, LLSpatialGroup* curp,
							  bool immediate)
{
	// Sanity check submitted by open source user Bushing Spatula who was
	// seeing crashing here (see VWR-424 reported by Bunny Mayne)
	if (!drawablep)
	{
		llwarns << "Bad drawable !" << llendl;
		llassert(false);
		return;
	}

	bool was_visible = curp && curp->isVisible();

	if (curp && curp->getSpatialPartition() != this)
	{
		// Keep drawable from being garbage collected
		LLPointer<LLDrawable> ptr = drawablep;
		if (curp->getSpatialPartition()->remove(drawablep, curp))
		{
			put(drawablep, was_visible);
			return;
		}
		else
		{
			llwarns << "Drawable lost between spatial partitions on outbound transition."
					 << llendl;
			llassert(false);
		}
	}

	if (curp && curp->updateInGroup(drawablep, immediate))
	{
		// Already updated, do not need to do anything
		return;
	}

	// Keep drawable from being garbage collected
	LLPointer<LLDrawable> ptr = drawablep;
	if (curp && !remove(drawablep, curp))
	{
		llwarns << "Move could not find existing spatial group !" << llendl;
		llassert(false);
	}

	put(drawablep, was_visible);
}

class LLSpatialShift final : public OctreeTraveler
{
public:
	LLSpatialShift(const LLVector4a& offset)
	:	mOffset(offset)
	{
	}

	void visit(const OctreeNode* branchp) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)branchp->getListener(0);
		if (groupp)
		{
			groupp->shift(mOffset);
		}
	}

public:
	const LLVector4a& mOffset;
};

void LLSpatialPartition::shift(const LLVector4a& offset)
{
	// Shift octree node bounding boxes by offset
	LLSpatialShift shifter(offset);
	shifter.traverse(mOctree);
}

class LLOctreeCull : public LLViewerOctreeCull
{
protected:
	LOG_CLASS(LLOctreeCull);

public:
	LLOctreeCull(LLCamera* camerap)
	:	LLViewerOctreeCull(camerap)
	{
	}

	bool earlyFail(LLViewerOctreeGroup* base_groupp) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)base_groupp;
		if (!groupp)
		{
			llwarns_sparse << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return true;
		}
#if 1
		if (LLPipeline::sReflectionRender)
		{
			return false;
		}
#endif
		groupp->checkOcclusion();

			// Never occlusion cull the root node
		if (groupp->getOctreeNode()->getParent() &&
			// Ignore occlusion if disabled
		  	LLPipeline::sUseOcclusion &&
			groupp->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			gPipeline.markOccluder(groupp);
			return true;
		}

		return false;
	}

	S32 frustumCheck(const LLViewerOctreeGroup* groupp) override
	{
		S32 res = AABBInFrustumNoFarClipGroupBounds(groupp);
		if (res)
		{
			res = llmin(res, AABBSphereIntersectGroupExtents(groupp));
		}
		return res;
	}

	S32 frustumCheckObjects(const LLViewerOctreeGroup* groupp) override
	{
		S32 res = AABBInFrustumNoFarClipObjectBounds(groupp);
		if (res)
		{
			res = llmin(res, AABBSphereIntersectObjectExtents(groupp));
		}
		return res;
	}

	void processGroup(LLViewerOctreeGroup* base_groupp) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)base_groupp;
		if (!groupp)
		{
			llwarns_sparse << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		// Apparently, occlusion is still broken in the PBR renderer... HB
		if (!gUsePBRShaders)
		{
			if (groupp->needsUpdate() ||
				groupp->getVisible(LLViewerCamera::sCurCameraID) <
					LLViewerOctreeEntryData::getCurrentFrame() - 1)
			{
				groupp->doOcclusion(mCamera);
			}
		}
		gPipeline.markNotCulled(groupp, *mCamera);
	}
};

class LLOctreeCullNoFarClip final : public LLOctreeCull
{
public:
	LLOctreeCullNoFarClip(LLCamera* camerap)
	:	LLOctreeCull(camerap)
	{
	}

	S32 frustumCheck(const LLViewerOctreeGroup* groupp) override
	{
		return AABBInFrustumNoFarClipGroupBounds(groupp);
	}

	S32 frustumCheckObjects(const LLViewerOctreeGroup* groupp) override
	{
		return AABBInFrustumNoFarClipObjectBounds(groupp);
	}
};

class LLOctreeCullShadow : public LLOctreeCull
{
public:
	LLOctreeCullShadow(LLCamera* camerap)
	:	LLOctreeCull(camerap)
	{
	}

	S32 frustumCheck(const LLViewerOctreeGroup* groupp) override
	{
		return AABBInFrustumGroupBounds(groupp);
	}

	S32 frustumCheckObjects(const LLViewerOctreeGroup* groupp) override
	{
		return AABBInFrustumObjectBounds(groupp);
	}
};

class LLOctreeCullVisExtents final : public LLOctreeCullShadow
{
protected:
	LOG_CLASS(LLOctreeCullVisExtents);

public:
	LLOctreeCullVisExtents(LLCamera* camerap, LLVector4a& min, LLVector4a& max)
	:	LLOctreeCullShadow(camerap),
		mMin(min),
		mMax(max),
		mEmpty(true)
	{
	}

	bool earlyFail(LLViewerOctreeGroup* base_groupp) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)base_groupp;
		if (!groupp)
		{
			llwarns_sparse << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return true;
		}

			// Never occlusion cull the root node
		if (groupp->getOctreeNode()->getParent() &&
			// Ignore occlusion if disabled
		  	LLPipeline::sUseOcclusion &&
			groupp->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			return true;
		}

		return false;
	}

	void traverse(const OctreeNode* nodep) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)nodep->getListener(0);
		if (!groupp)
		{
			llwarns_once << "NULL spatial group for octree node "
						 << nodep << " !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		if (earlyFail(groupp))
		{
			return;
		}

		if (mRes == 2 ||
			(mRes && groupp->hasState(LLSpatialGroup::SKIP_FRUSTUM_CHECK)))
		{
			// Do not need to do frustum check
			OctreeTraveler::traverse(nodep);
		}
		else
		{
			mRes = frustumCheck(groupp);
			if (mRes)
			{
				// At least partially in, run on down
				OctreeTraveler::traverse(nodep);
			}
			mRes = 0;
		}
	}

	void processGroup(LLViewerOctreeGroup* base_groupp) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)base_groupp;
		if (!groupp)
		{
			llwarns_sparse << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		llassert(!groupp->hasState(LLSpatialGroup::DIRTY) &&
				 !groupp->isEmpty());

		if (mRes >= 2 || AABBInFrustumObjectBounds(groupp) > 0)
		{
			mEmpty = false;
			const LLVector4a* exts = groupp->getObjectExtents();
			update_min_max(mMin, mMax, exts[0]);
			update_min_max(mMin, mMax, exts[1]);
		}
	}

public:
	LLVector4a&	mMin;
	LLVector4a&	mMax;
	bool		mEmpty;
};

class LLOctreeSelect final : public LLOctreeCull
{
public:
	LLOctreeSelect(LLCamera* camerap, std::vector<LLDrawable*>* resultsp)
	:	LLOctreeCull(camerap),
		mResults(resultsp)
	{
	}

	LL_INLINE bool earlyFail(LLViewerOctreeGroup*) override
	{
		return false;
	}

	LL_INLINE void preprocess(LLViewerOctreeGroup*) override
	{
	}

	void processGroup(LLViewerOctreeGroup* base_group) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)base_group;
		if (!group)
		{
			llwarns_sparse << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		OctreeNode* branch = group->getOctreeNode();
		if (!branch)
		{
			llwarns_sparse << "NULL octree node !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		for (OctreeNode::const_element_iter i = branch->getDataBegin(),
											end = branch->getDataEnd();
			 i != end; ++i)
		{
			LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
			if (drawable && !drawable->isDead())
			{
				if (drawable->isSpatialBridge())
				{
					drawable->setVisible(*mCamera, mResults, true);
				}
				else
				{
					mResults->push_back(drawable);
				}
			}
		}
	}

public:
	std::vector<LLDrawable*>* mResults;
};

void drawBox(const LLVector3& c, const LLVector3& r)
{
	static const LLVector3 v1(-1.f, 1.f, -1.f);
	static const LLVector3 v2(-1.f, 1.f, 1.f);
	static const LLVector3 v3(1.f, 1.f, -1.f);
	static const LLVector3 v4(1.f, 1.f, 1.f);
	static const LLVector3 v5(1.f, -1.f, -1.f);
	static const LLVector3 v6(1.f, -1.f, 1.f);
	static const LLVector3 v7(-1.f, -1.f, -1.f);
	static const LLVector3 v8(-1.f, -1.f, 1.f);

	if (!c.isFinite() || !r.isFinite())
	{
		return;
	}

	LLVertexBuffer::unbind();

	gGL.begin(LLRender::TRIANGLE_STRIP);

	// Left front
	gGL.vertex3fv((c + r.scaledVec(v1)).mV);
	gGL.vertex3fv((c + r.scaledVec(v2)).mV);

	// Right front
	gGL.vertex3fv((c + r.scaledVec(v3)).mV);
	gGL.vertex3fv((c + r.scaledVec(v4)).mV);

	// Right back
 	gGL.vertex3fv((c + r.scaledVec(v5)).mV);
	gGL.vertex3fv((c + r.scaledVec(v6)).mV);

	// Left back
	gGL.vertex3fv((c + r.scaledVec(v7)).mV);
	gGL.vertex3fv((c + r.scaledVec(v8)).mV);

	// Left front
	gGL.vertex3fv((c + r.scaledVec(v1)).mV);
	gGL.vertex3fv((c + r.scaledVec(v2)).mV);

	gGL.end();

	// Bottom
	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.vertex3fv((c + r.scaledVec(v3)).mV);
	gGL.vertex3fv((c + r.scaledVec(v5)).mV);
	gGL.vertex3fv((c + r.scaledVec(v1)).mV);
	gGL.vertex3fv((c + r.scaledVec(v7)).mV);
	gGL.end();

	// Top
	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.vertex3fv((c + r.scaledVec(v4)).mV);
	gGL.vertex3fv((c + r.scaledVec(v2)).mV);
	gGL.vertex3fv((c + r.scaledVec(v6)).mV);
	gGL.vertex3fv((c + r.scaledVec(v8)).mV);
	gGL.end();
}

void drawBox(const LLVector4a& c, const LLVector4a& r)
{
	drawBox(reinterpret_cast<const LLVector3&>(c),
			reinterpret_cast<const LLVector3&>(r));
}

void drawBoxOutline(const LLVector3& pos, const LLVector3& size)
{
	if (!pos.isFinite() || !size.isFinite())
	{
		return;
	}

	LLVector3 v1 = size.scaledVec(LLVector3(1.f, 1.f, 1.f));
	LLVector3 v2 = size.scaledVec(LLVector3(-1.f, 1.f, 1.f));
	LLVector3 v3 = size.scaledVec(LLVector3(-1.f, -1.f, 1.f));
	LLVector3 v4 = size.scaledVec(LLVector3(1.f, -1.f, 1.f));

	gGL.begin(LLRender::LINES);

	// Top
	gGL.vertex3fv((pos + v1).mV);
	gGL.vertex3fv((pos + v2).mV);
	gGL.vertex3fv((pos + v2).mV);
	gGL.vertex3fv((pos + v3).mV);
	gGL.vertex3fv((pos + v3).mV);
	gGL.vertex3fv((pos + v4).mV);
	gGL.vertex3fv((pos + v4).mV);
	gGL.vertex3fv((pos + v1).mV);

	// Bottom
	gGL.vertex3fv((pos - v1).mV);
	gGL.vertex3fv((pos - v2).mV);
	gGL.vertex3fv((pos - v2).mV);
	gGL.vertex3fv((pos - v3).mV);
	gGL.vertex3fv((pos - v3).mV);
	gGL.vertex3fv((pos - v4).mV);
	gGL.vertex3fv((pos - v4).mV);
	gGL.vertex3fv((pos - v1).mV);

	// Right
	gGL.vertex3fv((pos + v1).mV);
	gGL.vertex3fv((pos - v3).mV);

	gGL.vertex3fv((pos + v4).mV);
	gGL.vertex3fv((pos - v2).mV);

	// Left
	gGL.vertex3fv((pos + v2).mV);
	gGL.vertex3fv((pos - v4).mV);

	gGL.vertex3fv((pos + v3).mV);
	gGL.vertex3fv((pos - v1).mV);

	gGL.end();
}

void drawBoxOutline(const LLVector4a& pos, const LLVector4a& size)
{
	drawBoxOutline(reinterpret_cast<const LLVector3&>(pos),
				   reinterpret_cast<const LLVector3&>(size));
}

class LLOctreeDirty : public OctreeTraveler
{
protected:
	LOG_CLASS(LLOctreeDirty);

public:
	LLOctreeDirty(bool no_rebuild)
	:	mNoRebuild(no_rebuild)
	{
	}

	void visit(const OctreeNode* state) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)state->getListener(0);
		if (!groupp)
		{
			llwarns_sparse << "NULL spatial group !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		groupp->destroyGL();

		if (!mNoRebuild)
		{
			for (LLSpatialGroup::element_iter i = groupp->getDataBegin(),
											  end = groupp->getDataEnd();
				 i != end; ++i)
			{
				LLDrawable* drawablep = (LLDrawable*)(*i)->getDrawable();
				if (!drawablep)
				{
					llwarns_once << "NULL drawable found in spatial group "
								 << std::hex << groupp << std::dec << llendl;
					continue;
				}
				LLViewerObject* vobjp = drawablep->getVObj().get();
				if (!vobjp)
				{
					continue;
				}
				vobjp->resetVertexBuffers();
				if (!groupp->getSpatialPartition()->mRenderByGroup)
				{
					gPipeline.markRebuild(drawablep);
				}
			}
		}

		for (LLSpatialGroup::bridge_list_t::iterator
				i = groupp->mBridgeList.begin(),
				end = groupp->mBridgeList.end();
			 i != end; ++i)
		{
			LLSpatialBridge* bridgep = *i;
			if (bridgep)
			{
				traverse(bridgep->mOctree);
			}
			else
			{
				llwarns_once << "NULL bridge found in spatial group "
							 << std::hex << groupp << std::dec << llendl;
			}
		}
	}

private:
	bool mNoRebuild;
};

void LLSpatialPartition::restoreGL()
{
}

void LLSpatialPartition::resetVertexBuffers()
{
	LLOctreeDirty dirty(sTeleportRequested);
	dirty.traverse(mOctree);
}

bool LLSpatialPartition::getVisibleExtents(LLCamera& camera, LLVector3& vis_min,
										   LLVector3& vis_max)
{
	LLVector4a vis_min_a, vis_max_a;
	vis_min_a.load3(vis_min.mV);
	vis_max_a.load3(vis_max.mV);

	{
		LL_FAST_TIMER(FTM_CULL_REBOUND);
		LLSpatialGroup* groupp = (LLSpatialGroup*)mOctree->getListener(0);
		if (groupp)
		{
			groupp->rebound();
		}
	}

	LLOctreeCullVisExtents vis(&camera, vis_min_a, vis_max_a);
	vis.traverse(mOctree);

	vis_min.set(vis_min_a.getF32ptr());
	vis_max.set(vis_max_a.getF32ptr());

	return vis.mEmpty;
}

S32 LLSpatialPartition::cull(LLCamera& camera,
							 std::vector<LLDrawable*>* resultsp,
							 bool for_select)
{
	{
		LL_FAST_TIMER(FTM_CULL_REBOUND);
		LLSpatialGroup* groupp = (LLSpatialGroup*)mOctree->getListener(0);
		if (groupp)
		{
			groupp->rebound();
		}
	}

#if 0
	if (for_select)
#endif
	{
		LLOctreeSelect selecter(&camera, resultsp);
		selecter.traverse(mOctree);
	}

	return 0;
}

S32 LLSpatialPartition::cull(LLCamera& camera, bool do_occlusion)
{
	{
		LL_FAST_TIMER(FTM_CULL_REBOUND);
		LLSpatialGroup* groupp = (LLSpatialGroup*)mOctree->getListener(0);
		if (groupp)
		{
			groupp->rebound();
		}
	}

	if (LLPipeline::sShadowRender)
	{
		LL_FAST_TIMER(FTM_FRUSTUM_CULL);
		LLOctreeCullShadow culler(&camera);
		culler.traverse(mOctree);
	}
	else if (mInfiniteFarClip || (!LLPipeline::sUseFarClip && !gCubeSnapshot))
	{
		LL_FAST_TIMER(FTM_FRUSTUM_CULL);
		LLOctreeCullNoFarClip culler(&camera);
		culler.traverse(mOctree);
	}
	else
	{
		LL_FAST_TIMER(FTM_FRUSTUM_CULL);
		LLOctreeCull culler(&camera);
		culler.traverse(mOctree);
	}

	return 0;
}

// Note: 'mask' is ignored for PBR rendering.
void pushVerts(LLDrawInfo* paramsp, U32 mask)
{
	LLRenderPass::applyModelMatrix(*paramsp);
	paramsp->mVertexBuffer->setBuffer(mask);
	paramsp->mVertexBuffer->drawRange(LLRender::TRIANGLES,
									  paramsp->mStart, paramsp->mEnd,
									  paramsp->mCount, paramsp->mOffset);
}

// Note: 'mask' is ignored for PBR rendering.
void pushVerts(LLSpatialGroup* groupp, U32 mask)
{
	for (LLSpatialGroup::draw_map_t::const_iterator
			it = groupp->mDrawMap.begin(), end = groupp->mDrawMap.end();
		 it != end; ++it)
	{
		const LLSpatialGroup::drawmap_elem_t& draw_info_vec = it->second;
		for (U32 i = 0, count = draw_info_vec.size(); i < count; ++i)
		{
			LLDrawInfo* paramsp = draw_info_vec[i].get();
			pushVerts(paramsp, mask);
		}
	}
}

// Note: 'mask' is ignored for PBR rendering.
void pushVerts(LLFace* facep, U32 mask)
{
	if (facep)
	{
		llassert(facep->verify());
		facep->renderIndexed(mask);
	}
}

// Note: 'mask' is ignored for PBR rendering.
void pushVerts(LLDrawable* drawablep, U32 mask)
{
	for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
	{
		pushVerts(drawablep->getFace(i), mask);
	}
}

void pushVerts(LLVolume* volp)
{
	if (!volp) return;

	LLVertexBuffer::unbind();
	for (S32 i = 0, count = volp->getNumVolumeFaces(); i < count; ++i)
	{
		const LLVolumeFace& face = volp->getVolumeFace(i);
		LLVertexBuffer::drawElements(face.mNumVertices, face.mPositions, NULL,
									 face.mNumIndices, face.mIndices);
	}
}

// Note: 'mask' is ignored for PBR rendering.
void pushBufferVerts(LLVertexBuffer* buffp, U32 mask)
{
	if (buffp)
	{
		buffp->setBuffer(mask);
		buffp->drawRange(LLRender::TRIANGLES, 0, buffp->getNumVerts() - 1,
						 buffp->getNumIndices(), 0);
	}
}

// Note: 'mask' is ignored for PBR rendering.
void pushBufferVerts(LLSpatialGroup* groupp, U32 mask, bool push_alpha = true)
{
	if (groupp->getSpatialPartition()->mRenderByGroup &&
		!groupp->mDrawMap.empty())
	{
		LLDrawInfo* paramsp = *(groupp->mDrawMap.begin()->second.begin());
		LLRenderPass::applyModelMatrix(*paramsp);

		if (push_alpha)
		{
			pushBufferVerts(groupp->mVertexBuffer, mask);
		}

		for (LLSpatialGroup::buffer_map_t::const_iterator
				i = groupp->mBufferMap.begin(), end = groupp->mBufferMap.end();
			 i != end; ++i)
		{
			for (LLSpatialGroup::buffer_texture_map_t::const_iterator
					j = i->second.begin(), end2 = i->second.end();
				 j != end2; ++j)
			{
				for (LLSpatialGroup::buffer_list_t::const_iterator
						k = j->second.begin(), end3 = j->second.end();
					 k != end3; ++k)
				{
					pushBufferVerts(*k, mask);
				}
			}
		}
	}
}

void pushVertsColorCoded(LLSpatialGroup* groupp, U32 mask)
{
	static const LLColor4 colors[] = {
		LLColor4::green,
		LLColor4::green1,
		LLColor4::green2,
		LLColor4::green3,
		LLColor4::green4,
		LLColor4::green5,
		LLColor4::green6
	};

	constexpr U32 col_count = LL_ARRAY_SIZE(colors);

	U32 col = 0;

	for (LLSpatialGroup::draw_map_t::const_iterator
			it = groupp->mDrawMap.begin(), end = groupp->mDrawMap.end();
		 it != end; ++it)
	{
		const LLSpatialGroup::drawmap_elem_t& draw_vec = it->second;
		for (U32 i = 0, count = draw_vec.size(); i < count; ++i)
		{
			LLDrawInfo* infop = draw_vec[i];
			LLRenderPass::applyModelMatrix(*infop);
			gGL.diffuseColor4f(colors[col].mV[0], colors[col].mV[1],
							   colors[col].mV[2], 0.5f);
			// Note: mask ignored in PBR rendering mode
			infop->mVertexBuffer->setBuffer(mask);
			infop->mVertexBuffer->drawRange(LLRender::TRIANGLES,
											infop->mStart, infop->mEnd,
											infop->mCount, infop->mOffset);
			col = (col + 1) % col_count;
		}
	}
}

// Renders solid object bounding box, color coded by buffer activity
void renderOctree(LLSpatialGroup* groupp)
{
	gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);
	LLVector4 col;
	if (groupp->mBuilt > 0.f)
	{
		groupp->mBuilt -= 2.f * gFrameIntervalSeconds;
		col.set(0.1f, 0.1f, 1.f, 0.1f);

		LLGLDepthTest gl_depth(false, false);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		gGL.diffuseColor4f(1.f, 0.f, 0.f, groupp->mBuilt);
		gGL.flush();
		gGL.lineWidth(5.f);
		const LLVector4a* bounds = groupp->getObjectBounds();
		drawBoxOutline(bounds[0], bounds[1]);
		gGL.flush();
		gGL.lineWidth(1.f);
		gGL.flush();

		LLVOAvatar* last_avatar = NULL;
		U64 last_hash = 0;

		for (LLSpatialGroup::element_iter i = groupp->getDataBegin(),
										  end = groupp->getDataEnd();
			 i != end; ++i)
		{
			LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
			if (!drawable || drawable->isDead()) continue;

			U32 count = drawable->getNumFaces();
			if (!count) continue;

			bool not_bridge = !groupp->getSpatialPartition()->isBridge();
			if (not_bridge)
			{
				gGL.pushMatrix();
				LLVector3 trans = drawable->getRegion()->getOriginAgent();
				gGL.translatef(trans.mV[0], trans.mV[1], trans.mV[2]);
			}

			LLFace* facep = drawable->getFace(0);
			bool rigged = facep->isState(LLFace::RIGGED);
			gDebugProgram.bind(rigged);
			gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f);
			U32 type = LLVertexBuffer::MAP_VERTEX;
			if (rigged)
			{
				if (facep->mAvatar != last_avatar ||
					facep->mSkinInfo->mHash != last_hash)
				{
					if (!LLRenderPass::uploadMatrixPalette(facep->mAvatar,
														   facep->mSkinInfo))
					{
						continue;
					}
					last_avatar = facep->mAvatar;
					last_hash = facep->mSkinInfo->mHash;
				}
				// Add the weights to the type for rigged faces
				type |= LLVertexBuffer::MAP_WEIGHT4;
				// Now that we got past the potential 'continue' above, we
				// can push our render matrix (bug in LL's code that pushes
				// it before the continue). HB
				gGL.pushMatrix();
				gGL.loadMatrix(gGLModelView);
			}

			for (U32 j = 0; j < count; ++j)
			{
				facep = drawable->getFace(j);
				if (!facep) continue;

				LLVertexBuffer* vb = facep->getVertexBuffer();
				if (!vb) continue;

				LLVOVolume* volp = drawable->getVOVolume();

				if (gFrameTimeSeconds - facep->mLastUpdateTime < 0.5f)
				{
					if (volp && volp->isShrinkWrapped())
					{
						gGL.diffuseColor4f(0.f, 1.f, 1.f, groupp->mBuilt);
					}
					else
					{
						gGL.diffuseColor4f(0.f, 1.f, 0.f, groupp->mBuilt);
					}
				}
				else if (gFrameTimeSeconds - facep->mLastMoveTime < 0.5f)
				{
					if (volp && volp->isShrinkWrapped())
					{
						gGL.diffuseColor4f(1.f, 1.f, 0.f, groupp->mBuilt);
					}
					else
					{
						gGL.diffuseColor4f(1.f, 0.f, 0.f, groupp->mBuilt);
					}
				}
				else
				{
					continue;
				}

				// Note: mask ignored in PBR rendering mode
				vb->setBuffer(type);
				vb->draw(LLRender::TRIANGLES, facep->getIndicesCount(),
						 facep->getIndicesStart());
			}

			if (rigged)
			{
				gGL.popMatrix();
			}

			if (not_bridge)
			{
				gGL.popMatrix();
			}
		}
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		gDebugProgram.bind(); // Make sure non-rigged variant is bound
		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else
	{
		col.set(0.1f, 0.1f, 1.f, 0.1f);
	}

	gGL.diffuseColor4fv(col.mV);
	LLVector4a fudge;
	fudge.splat(0.001f);

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	gGL.diffuseColor4f(0.f, 1.f, 1.f, 1.f);
	const LLVector4a* bounds = groupp->getBounds();
	drawBoxOutline(bounds[0], bounds[1]);

	stop_glerror();
}

void renderXRay(LLSpatialGroup* groupp)
{
	if (!groupp->isVisible() || groupp->isEmpty() ||
		(LLPipeline::sUseOcclusion &&
		 groupp->isOcclusionState(LLSpatialGroup::OCCLUDED)))
	{
		return;
	}

	pushBufferVerts(groupp, LLVertexBuffer::MAP_VERTEX, false);

	bool selected = false;
	for (LLSpatialGroup::element_iter iter = groupp->getDataBegin(),
									  end = groupp->getDataEnd();
		 iter != end; ++iter)
	{
		LLDrawable* drawable = (LLDrawable*)(*iter)->getDrawable();
		if (drawable && drawable->getVObj().notNull() &&
			drawable->getVObj()->isSelected())
		{
			selected = true;
			break;
		}
	}

	if (!selected)
	{
		return;
	}

	// Store for rendering occlusion volume as overlay
	LLSpatialBridge* bridgep = groupp->getSpatialPartition()->asBridge();
	if (bridgep)
	{
		gVisibleSelectedGroups.insert(bridgep->getSpatialGroup());
	}
	else
	{
		gVisibleSelectedGroups.insert(groupp);
	}
}

void renderCrossHairs(LLVector3 position, F32 size, LLColor4 color)
{
	gGL.color4fv(color.mV);
	gGL.begin(LLRender::LINES);
	{
		gGL.vertex3fv((position - LLVector3(size, 0.f, 0.f)).mV);
		gGL.vertex3fv((position + LLVector3(size, 0.f, 0.f)).mV);
		gGL.vertex3fv((position - LLVector3(0.f, size, 0.f)).mV);
		gGL.vertex3fv((position + LLVector3(0.f, size, 0.f)).mV);
		gGL.vertex3fv((position - LLVector3(0.f, 0.f, size)).mV);
		gGL.vertex3fv((position + LLVector3(0.f, 0.f, size)).mV);
	}
	gGL.end();
}

void renderUpdateType(LLDrawable* drawablep)
{
	LLViewerObject* vobj = drawablep->getVObj();
	if (!vobj || OUT_UNKNOWN == vobj->getLastUpdateType())
	{
		return;
	}

	LLGLEnable blend(GL_BLEND);

	switch (vobj->getLastUpdateType())
	{
		case OUT_FULL:
			gGL.diffuseColor4f(0.f, 1.f, 0.f, 0.5f);
			break;

		case OUT_TERSE_IMPROVED:
			gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);
			break;

		case OUT_FULL_COMPRESSED:
			if (vobj->getLastUpdateCached())
			{
				gGL.diffuseColor4f(1.f, 0.f, 0.f, 0.5f);
			}
			else
			{
				gGL.diffuseColor4f(1.f, 1.f, 0.f, 0.5f);
			}
			break;

		case OUT_FULL_CACHED:
			gGL.diffuseColor4f(0.f, 0.f, 1.f, 0.5f);
			break;

		default:
			llwarns << "Unknown update_type " << vobj->getLastUpdateType()
					<< llendl;
	}

	S32 num_faces = drawablep->getNumFaces();
	if (num_faces)
	{
		for (S32 i = 0; i < num_faces; ++i)
		{
			LLFace* facep = drawablep->getFace(i);
			if (facep)
			{
				pushVerts(facep, LLVertexBuffer::MAP_VERTEX);
			}
		}
	}
}

void renderBoundingBox(LLDrawable* drawable, bool set_color = true)
{
	if (set_color)
	{
		if (drawable->isSpatialBridge())
		{
			gGL.diffuseColor4f(1.f, 0.5f, 0.f, 1.f);
		}
		else if (drawable->getVOVolume())
		{
			if (drawable->isRoot())
			{
				gGL.diffuseColor4f(1.f, 1.f, 0.f, 1.f);
			}
			else
			{
				gGL.diffuseColor4f(0.f, 1.f, 0.f, 1.f);
			}
		}
		else if (drawable->getVObj())
		{
			switch (drawable->getVObj()->getPCode())
			{
				case LLViewerObject::LL_VO_SURFACE_PATCH:
					gGL.diffuseColor4f(0.f, 1.f, 1.f, 1.f);
					break;

				case LLViewerObject::LL_VO_CLOUDS:
					gGL.diffuseColor4f(0.5f, 0.5f, 0.5f, 1.f);
					break;

				case LLViewerObject::LL_VO_PART_GROUP:
				case LLViewerObject::LL_VO_HUD_PART_GROUP:
					gGL.diffuseColor4f(0.f, 0.f, 1.f, 1.f);
					break;

				case LLViewerObject::LL_VO_VOID_WATER:
				case LLViewerObject::LL_VO_WATER:
					gGL.diffuseColor4f(0.f, 0.5f, 1.f, 1.f);
					break;

				case LL_PCODE_LEGACY_TREE:
					gGL.diffuseColor4f(0.f, 0.5f, 0.f, 1.f);
					break;

				default:
					gGL.diffuseColor4f(1.f, 0.f, 1.f, 1.f);
			}
		}
		else
		{
			gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f);
		}
	}

	const LLVector4a* ext;
	LLVector4a pos, size;

	if (drawable->getVOVolume())
	{
		// Render face bounding boxes
		for (S32 i = 0, count = drawable->getNumFaces(); i < count; ++i)
		{
			LLFace* facep = drawable->getFace(i);
			if (facep)
			{
				ext = facep->mExtents;

				pos.setAdd(ext[0], ext[1]);
				pos.mul(0.5f);
				size.setSub(ext[1], ext[0]);
				size.mul(0.5f);

				drawBoxOutline(pos, size);
			}
		}
	}

	// Render drawable bounding box
	ext = drawable->getSpatialExtents();

	pos.setAdd(ext[0], ext[1]);
	pos.mul(0.5f);
	size.setSub(ext[1], ext[0]);
	size.mul(0.5f);

	LLViewerObject* vobj = drawable->getVObj();
	if (vobj && vobj->onActiveList())
	{
		gGL.flush();
		gGL.lineWidth(llmax(4.f * sinf(gFrameTimeSeconds * 2.f) + 1.f, 1.f));
		drawBoxOutline(pos,size);
		gGL.flush();
		gGL.lineWidth(1.f);
	}
	else
	{
		drawBoxOutline(pos, size);
	}

	stop_glerror();
}

void renderNormals(LLDrawable* drawablep)
{
	if (!drawablep->isVisible())
	{
		return;
	}

	LLVOVolume* vol = drawablep->getVOVolume();
	if (!vol)
	{
		return;
	}

	LLVolume* volp = vol->getVolume();
	if (!volp)
	{
		return;
	}

	LLVertexBuffer::unbind();

	// Drawable's normals & tangents are stored in model space, i.e. before any
	// scaling is applied. SL-13490: using pos + normal to compute the second
	// vertex of a normal line segment does not work when there is a non-
	// uniform scale in the mix. Normals require MVP-inverse-transpose
	// transform. We get that effect here by pre-applying the inverse scale
	// (twice, because one forward scale will be re-applied via the MVP in the
	// vertex shader)

	LLVector3 scale_v3 = vol->getScale();
	F32 scale_len = scale_v3.length();
	LLVector4a obj_scale(scale_v3.mV[VX], scale_v3.mV[VY], scale_v3.mV[VZ]);
	obj_scale.normalize3();

	// Normals & tangent line segments get scaled along with the object. Divide
	// by scale length to keep the as-viewed lengths (relatively) constant with
	// the debug setting length.
	static LLCachedControl<F32> norm_scale(gSavedSettings,
										   "RenderDebugNormalScale");
	F32 draw_length = norm_scale / scale_len;

	// Create inverse-scale vector for normals
	LLVector4a inv_scale(1.f / scale_v3.mV[VX], 1.f / scale_v3.mV[VY],
						 1.f / scale_v3.mV[VZ]);
	inv_scale.mul(inv_scale);  // Squared, to apply inverse scale twice
	inv_scale.normalize3fast();

	gGL.pushMatrix();
	gGL.multMatrix(vol->getRelativeXform().getF32ptr());

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	LLVector4a p, v;
	for (S32 i = 0, count = volp->getNumVolumeFaces(); i < count; ++i)
	{
		const LLVolumeFace& face = volp->getVolumeFace(i);

		gGL.flush();
		gGL.diffuseColor4f(1.f, 1.f, 0.f, 1.f);
		gGL.begin(LLRender::LINES);
		for (S32 j = 0; j < face.mNumVertices; ++j)
		{
			v.setMul(face.mNormals[j], 1.f);
			// Pre-scale normal, so it is left with an inverse-transpose xform
			// after MVP
			v.mul(inv_scale);
			v.normalize3fast();
			v.mul(draw_length);
			p.setAdd(face.mPositions[j], v);

			gGL.vertex3fv(face.mPositions[j].getF32ptr());
			gGL.vertex3fv(p.getF32ptr());
		}
		gGL.end();

		if (!face.mTangents)
		{
			continue;
		}

		// Tangents are simple vectors and do not require reorientation via
		// pre-scaling
		gGL.flush();
		gGL.diffuseColor4f(0.f, 1.f, 1.f, 1.f);
		gGL.begin(LLRender::LINES);
		for (S32 j = 0; j < face.mNumVertices; ++j)
		{
			v.setMul(face.mTangents[j], 1.f);
			v.normalize3fast();
			v.mul(draw_length);

			p.setAdd(face.mPositions[j], v);
			gGL.vertex3fv(face.mPositions[j].getF32ptr());
			gGL.vertex3fv(p.getF32ptr());
		}
		gGL.end();
	}

	gGL.popMatrix();
	stop_glerror();
}

void renderTexturePriority(LLDrawable* drawp)
{
	for (S32 face = 0, count = drawp->getNumFaces(); face < count; ++face)
	{
		LLFace* facep = drawp->getFace(face);
		if (!facep) continue;

		LLVector4 cold(0.f, 0.f, 0.25f);
		LLVector4 hot(1.f, 0.25f, 0.25f);

		LLVector4 boost_cold(0.f, 0.f, 0.f, 0.f);
		LLVector4 boost_hot(0.f, 1.f, 0.f, 1.f);

		LLGLDisable blend(GL_BLEND);

		F32 vsize = facep->getPixelArea();
		if (vsize > sCurMaxTexPriority)
		{
			sCurMaxTexPriority = vsize;
		}

		F32 t = vsize / sLastMaxTexPriority;
		LLVector4 col = lerp(cold, hot, t);
		gGL.diffuseColor4fv(col.mV);

		LLVector4a center;
		center.setAdd(facep->mExtents[1], facep->mExtents[0]);
		center.mul(0.5f);
		LLVector4a size;
		size.setSub(facep->mExtents[1], facep->mExtents[0]);
		size.mul(0.5f);
		size.add(LLVector4a(0.01f));
		drawBox(center, size);
	}
	stop_glerror();
}

void renderPoints(LLDrawable* drawablep)
{
	LLGLDepthTest depth(GL_FALSE, GL_FALSE);
	if (drawablep->getNumFaces())
	{
		gGL.begin(LLRender::POINTS);
		gGL.diffuseColor3f(1.f, 1.f, 1.f);
		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			LLFace* face = drawablep->getFace(i);
			if (face)
			{
				gGL.vertex3fv(face->mCenterLocal.mV);
			}
		}
		gGL.end();
		stop_glerror();
	}
}

void renderTextureAnim(LLDrawInfo* infop)
{
	if (!infop->mTextureMatrix)
	{
		return;
	}

	LLGLEnable blend(GL_BLEND);
	gGL.diffuseColor4f(1.f, 1.f, 0.f, 0.5f);
	pushVerts(infop, LLVertexBuffer::MAP_VERTEX);
	stop_glerror();
}

void renderBatchSize(LLDrawInfo* infop)
{
	if (infop->mTextureList.empty())
	{
		return;
	}
	LLGLEnable offset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.f, 1.f);
	LLGLSLShader* old_shader = LLGLSLShader::sCurBoundShaderPtr;
	// NOTE: does not impact PBR rendering (mask ignored). HB
	U32 mask = LLVertexBuffer::MAP_VERTEX;
	bool bind = false;
	if (infop->mAvatar && old_shader->mRiggedVariant)
	{
		bind = true;
		mask |= LLVertexBuffer::MAP_WEIGHT4;
		gGL.pushMatrix();
		gGL.loadMatrix(gGLModelView);
		old_shader->mRiggedVariant->bind();
		LLRenderPass::uploadMatrixPalette(*infop);
	}
	gGL.diffuseColor4ubv(infop->getDebugColor().mV);
	pushVerts(infop, mask);
	if (bind)
	{
		gGL.popMatrix();
		old_shader->bind();
	}
}

// Note: removed from the PBR renderer
void renderShadowFrusta(LLDrawInfo* infop)
{
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ADD);

	LLVector4a center;
	center.setAdd(infop->mExtents[1], infop->mExtents[0]);
	center.mul(0.5f);
	LLVector4a size;
	size.setSub(infop->mExtents[1],infop->mExtents[0]);
	size.mul(0.5f);

	if (gPipeline.mShadowCamera[4].AABBInFrustum(center, size))
	{
		gGL.diffuseColor3f(1.f, 0.f, 0.f);
		pushVerts(infop, LLVertexBuffer::MAP_VERTEX);
	}
	if (gPipeline.mShadowCamera[5].AABBInFrustum(center, size))
	{
		gGL.diffuseColor3f(0.f, 1.f, 0.f);
		pushVerts(infop, LLVertexBuffer::MAP_VERTEX);
	}
	if (gPipeline.mShadowCamera[6].AABBInFrustum(center, size))
	{
		gGL.diffuseColor3f(0.f, 0.f, 1.f);
		pushVerts(infop, LLVertexBuffer::MAP_VERTEX);
	}
	if (gPipeline.mShadowCamera[7].AABBInFrustum(center, size))
	{
		gGL.diffuseColor3f(1.f, 0.f, 1.f);
		pushVerts(infop, LLVertexBuffer::MAP_VERTEX);
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	stop_glerror();
}

void renderLights(LLDrawable* drawablep)
{
	if (!drawablep->isLight())
	{
		return;
	}

	if (drawablep->getNumFaces())
	{
		LLGLEnable blend(GL_BLEND);
		gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);

		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			LLFace* face = drawablep->getFace(i);
			if (face)
			{
				pushVerts(face, LLVertexBuffer::MAP_VERTEX);
			}
		}

		const LLVector4a* ext = drawablep->getSpatialExtents();

		LLVector4a pos;
		pos.setAdd(ext[0], ext[1]);
		pos.mul(0.5f);
		LLVector4a size;
		size.setSub(ext[1], ext[0]);
		size.mul(0.5f);

		{
			LLGLDepthTest depth(GL_FALSE, GL_TRUE);
			gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
			drawBoxOutline(pos, size);
		}

		gGL.diffuseColor4f(1.f, 1.f, 0.f, 1.f);
		F32 rad = drawablep->getVOVolume()->getLightRadius();
		drawBoxOutline(pos, LLVector4a(rad));
		stop_glerror();
	}
}

class LLRenderOctreeRaycast final
:	public LLOctreeTriangleRayIntersectNoOwnership
{
public:
	LLRenderOctreeRaycast(const LLVector4a& start, const LLVector4a& dir,
						  F32* closest_t)
	:	LLOctreeTriangleRayIntersectNoOwnership(start, dir, NULL, closest_t,
												NULL, NULL, NULL, NULL)
	{
	}

	void visit(const LLOctreeNodeNoOwnership<LLVolumeTriangle>* branch) override
	{
		LLVolumeOctreeListenerNoOwnership* vl =
			(LLVolumeOctreeListenerNoOwnership*)branch->getListener(0);

		LLVector3 center, size;

		if (branch->isEmpty())
		{
			gGL.diffuseColor3f(1.f, 0.2f, 0.f);
			center.set(branch->getCenter().getF32ptr());
			size.set(branch->getSize().getF32ptr());
		}
		else if (vl)
		{
			gGL.diffuseColor3f(0.75f, 1.f, 0.f);
			center.set(vl->mBounds[0].getF32ptr());
			size.set(vl->mBounds[1].getF32ptr());
		}

		drawBoxOutline(center, size);

		for (U32 i = 0; i < 2; ++i)
		{
			LLGLDepthTest depth(GL_TRUE, GL_FALSE,
								i == 1 ? GL_LEQUAL : GL_GREATER);

			if (i == 1)
			{
				gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);
			}
			else
			{
				gGL.diffuseColor4f(0.f, 0.5f, 0.5f, 0.25f);
				drawBoxOutline(center, size);
			}

			if (i == 1)
			{
				gGL.flush();
				gGL.lineWidth(3.f);
			}

			gGL.begin(LLRender::TRIANGLES);
			for (LLOctreeNodeNoOwnership<LLVolumeTriangle>::const_element_iter
					iter = branch->getDataBegin(), end = branch->getDataEnd();
				 iter != end; ++iter)
			{
				const LLVolumeTriangle* tri = *iter;

				gGL.vertex3fv(tri->mV[0]->getF32ptr());
				gGL.vertex3fv(tri->mV[1]->getF32ptr());
				gGL.vertex3fv(tri->mV[2]->getF32ptr());
			}
			gGL.end();

			if (i == 1)
			{
				gGL.flush();
				gGL.lineWidth(1.f);
			}
		}
	}
};

void renderRaycast(LLDrawable* drawablep)
{
	if (!drawablep->getNumFaces())
	{
		return;
	}

	LLGLEnable blend(GL_BLEND);
	gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);

	LLVOVolume* vobjp = drawablep->getVOVolume();
	if (vobjp && !vobjp->isDead())
	{
		LLVolume* volp = vobjp->getVolume();

		bool transform = true;
		if (drawablep->isState(LLDrawable::RIGGED))
		{
			volp = vobjp->getRiggedVolume();
			transform = false;
		}

		if (volp)
		{
			LLVector3 trans = drawablep->getRegion()->getOriginAgent();

			for (S32 i = 0, count = volp->getNumVolumeFaces(); i < count;
				 ++i)
			{
				const LLVolumeFace& face = volp->getVolumeFace(i);

				gGL.pushMatrix();
				gGL.translatef(trans.mV[0], trans.mV[1], trans.mV[2]);
				gGL.multMatrix(vobjp->getRelativeXform().getF32ptr());

				LLVector4a start, end;
				if (transform)
				{
					LLVector3 v_start(gDebugRaycastStart.getF32ptr());
					LLVector3 v_end(gDebugRaycastEnd.getF32ptr());

					v_start = vobjp->agentPositionToVolume(v_start);
					v_end = vobjp->agentPositionToVolume(v_end);

					start.load3(v_start.mV);
					end.load3(v_end.mV);
				}
				else
				{
					start = gDebugRaycastStart;
					end = gDebugRaycastEnd;
				}

				LLVector4a dir;
				dir.setSub(end, start);

				gGL.flush();
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				{
					// Render face positions
					gGL.diffuseColor4f(0.f, 1.f, 1.f, 0.5f);
					LLVertexBuffer::drawElements(face.mNumVertices,
												 face.mPositions, NULL,
												 face.mNumIndices,
												 face.mIndices);
				}

				if (!volp->isUnique())
				{
					F32 t = 1.f;

					if (!face.mOctree)
					{
						((LLVolumeFace*)&face)->createOctree();
					}

					LLRenderOctreeRaycast render(start, dir, &t);
					render.traverse(face.mOctree);
				}

				gGL.popMatrix();
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		}
	}
	else if (drawablep->isAvatar())
	{
		if (drawablep->getVObj() == gDebugRaycastObject)
		{
			LLGLDepthTest depth(GL_FALSE);
			LLVOAvatar* av = (LLVOAvatar*) drawablep->getVObj().get();
			av->renderCollisionVolumes();
		}
	}

	if (drawablep->getVObj() == gDebugRaycastObject)
	{
		// Draw intersection point
		gGL.pushMatrix();
		gGL.loadMatrix(gGLModelView);
		LLVector3 translate(gDebugRaycastIntersection.getF32ptr());
		gGL.translatef(translate.mV[0], translate.mV[1], translate.mV[2]);
		LLCoordFrame orient;
		LLVector4a debug_binormal;
		debug_binormal.setCross3(gDebugRaycastNormal, gDebugRaycastTangent);
		debug_binormal.mul(gDebugRaycastTangent.getF32ptr()[3]);
		LLVector3 normal(gDebugRaycastNormal.getF32ptr());
		LLVector3 binormal(debug_binormal.getF32ptr());
		orient.lookDir(normal, binormal);
		LLMatrix4 rotation;
		orient.getRotMatrixToParent(rotation);
		gGL.multMatrix(rotation.getF32ptr());

		gGL.diffuseColor4f(1.f, 0.f, 0.f, 0.5f);
		drawBox(LLVector3::zero, LLVector3(0.1f, 0.022f, 0.022f));
		gGL.diffuseColor4f(0.f, 1.f, 0.f, 0.5f);
		drawBox(LLVector3::zero, LLVector3(0.021f, 0.1f, 0.021f));
		gGL.diffuseColor4f(0.f, 0.f, 1.f, 0.5f);
		drawBox(LLVector3::zero, LLVector3(0.02f, 0.02f, 0.1f));
		gGL.popMatrix();

		// Draw bounding box of prim
		const LLVector4a* ext = drawablep->getSpatialExtents();

		LLVector4a pos;
		pos.setAdd(ext[0], ext[1]);
		pos.mul(0.5f);
		LLVector4a size;
		size.setSub(ext[1], ext[0]);
		size.mul(0.5f);

		LLGLDepthTest depth(GL_FALSE, GL_TRUE);
		gGL.diffuseColor4f(0.f, 0.5f, 0.5f, 1.f);
		drawBoxOutline(pos, size);
	}
}

void renderAgentTarget(LLVOAvatar* avatarp)
{
	// Render these for self only (why, I don't know)
	if (avatarp->isSelf())
	{
		renderCrossHairs(avatarp->getPositionAgent(), 0.2f,
						 LLColor4(1.f, 0.f, 0.f, 0.8f));
		renderCrossHairs(avatarp->mDrawable->getPositionAgent(), 0.2f,
						 LLColor4(1.f, 0.f, 0.f, 0.8f));
		renderCrossHairs(avatarp->mRoot->getWorldPosition(), 0.2f,
						 LLColor4(1.f, 1.f, 1.f, 0.8f));
		renderCrossHairs(avatarp->mPelvisp->getWorldPosition(), 0.2f,
						 LLColor4(0.f, 0.f, 1.f, 0.8f));
	}
}

class LLOctreeRenderNonOccluded final : public OctreeTraveler
{
public:
	LLOctreeRenderNonOccluded(LLCamera* camera)
	:	mCamera(camera)
	{
	}

	void traverse(const OctreeNode* node) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group) return;

		const LLVector4a* bounds = group->getBounds();
		if (!mCamera || mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1]))
		{
			node->accept(this);

			for (U32 i = 0, count = node->getChildCount(); i < count; ++i)
			{
				traverse(node->getChild(i));
			}

			// Draw tight fit bounding boxes for spatial group
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE))
			{
				group->rebuildGeom();
				group->rebuildMesh();
				renderOctree(group);
			}
		}
	}

	void visit(const OctreeNode* branch) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)branch->getListener(0);
		if (!group) return;

		const LLVector4a* bounds = group->getBounds();
		if (group->hasState(LLSpatialGroup::GEOM_DIRTY) ||
			(mCamera &&
			 !mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1])))
		{
			return;
		}

		LLGLDisable stencil(gUsePBRShaders ? 0 : GL_STENCIL_TEST);

		group->rebuildGeom();
		group->rebuildMesh();

		if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BBOXES))
		{
			if (!group->isEmpty())
			{
				gGL.diffuseColor3f(0.f, 0.f, 1.f);
				const LLVector4a* obj_bounds = group->getObjectBounds();
				drawBoxOutline(obj_bounds[0], obj_bounds[1]);
			}
		}

		static LLCachedControl<bool> for_self_only(gSavedSettings,
												   "ShowAvatarDebugForSelfOnly");
		for (OctreeNode::const_element_iter i = branch->getDataBegin(),
											end = branch->getDataEnd();
			 i != end; ++i)
		{
			LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
			if (!drawable || drawable->isDead()) continue;

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BBOXES))
			{
				renderBoundingBox(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_NORMALS))
			{
				renderNormals(drawable);
			}

			if (drawable->getVOVolume() &&
				gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
			{
				renderTexturePriority(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_POINTS))
			{
				renderPoints(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_LIGHTS))
			{
				renderLights(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_RAYCAST))
			{
				renderRaycast(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_UPDATE_TYPE))
			{
				renderUpdateType(drawable);
			}
			LLViewerObject* objectp = drawable->getVObj().get();
			LLVOAvatar* avatarp = objectp ? objectp->asAvatar() : NULL;
			if (avatarp && (!for_self_only || avatarp->isSelf()))
			{
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_AVATAR_VOLUME))
				{
					avatarp->renderCollisionVolumes();
				}

				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_AVATAR_JOINTS))
				{
					avatarp->renderJoints();
					avatarp->renderBones();
				}

				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_AGENT_TARGET))
				{
					renderAgentTarget(avatarp);
				}
			}

			if (gDebugGL && !gUsePBRShaders)
			{
				for (U32 i = 0, count = drawable->getNumFaces();
					 i < count; ++i)
				{
					LLFace* facep = drawable->getFace(i);
					if (facep && facep->mDrawInfo)
					{
						U8 index = facep->getTextureIndex();
						if (index < FACE_DO_NOT_BATCH_TEXTURES)
						{
							if (facep->mDrawInfo->mTextureList.size() <= index)
							{
								llwarns << "Face texture index out of bounds." << llendl;
							}
							else if (facep->mDrawInfo->mTextureList[index] != facep->getTexture())
							{
								llwarns << "Face texture index incorrect." << llendl;
							}
						}
					}
				}
			}
		}

		for (LLSpatialGroup::draw_map_t::iterator it = group->mDrawMap.begin(),
												  end = group->mDrawMap.end();
			 it != end; ++it)
		{
			LLSpatialGroup::drawmap_elem_t& draw_vec = it->second;
			for (U32 i = 0, count = draw_vec.size(); i < count; ++i)
			{
				LLDrawInfo* draw_infop = draw_vec[i];
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_ANIM))
				{
					renderTextureAnim(draw_infop);
				}
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BATCH_SIZE))
				{
					renderBatchSize(draw_infop);
				}
				if (!gUsePBRShaders &&
					gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA))
				{
					renderShadowFrusta(draw_infop);
				}
			}
		}
	}

public:
	LLCamera* mCamera;
};

class LLOctreeRenderXRay final : public OctreeTraveler
{
public:
	LLOctreeRenderXRay(LLCamera* camerap)
	:	mCamera(camerap)
	{
	}

	void traverse(const OctreeNode* nodep) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)nodep->getListener(0);
		if (!groupp) return;

		const LLVector4a* bounds = groupp->getBounds();
		if (mCamera && !mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1]))
		{
			return;
		}
		nodep->accept(this);

		for (U32 i = 0, count = nodep->getChildCount(); i < count; ++i)
		{
			traverse(nodep->getChild(i));
		}

		// Render visibility wireframe
		if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION))
		{
			groupp->rebuildGeom();
			groupp->rebuildMesh();

			gGL.flush();
			gGL.pushMatrix();
			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);
			renderXRay(groupp);
			gGLLastMatrix = NULL;
			gGL.popMatrix();
			stop_glerror();
		}
	}

	LL_INLINE void visit(const OctreeNode*) override
	{
	}

public:
	LLCamera* mCamera;
};

class LLOctreeStateCheck final : public OctreeTraveler
{
protected:
	LOG_CLASS(LLOctreeStateCheck);

public:
	LLOctreeStateCheck()
	{
		for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
		{
			mInheritedMask[i] = 0;
		}
	}

	void traverse(const OctreeNode* node) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)node->getListener(0);
		if (!group) return;

		node->accept(this);

		U32 temp[LLViewerCamera::NUM_CAMERAS];

		for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
		{
			temp[i] = mInheritedMask[i];
			mInheritedMask[i] |= group->mOcclusionState[i] &
								 LLSpatialGroup::OCCLUDED;
		}

		for (U32 i = 0; i < node->getChildCount(); ++i)
		{
			traverse(node->getChild(i));
		}

		for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
		{
			mInheritedMask[i] = temp[i];
		}
	}

	void visit(const OctreeNode* state) override
	{
		LLSpatialGroup* group = (LLSpatialGroup*)state->getListener(0);
		if (!group) return;

		for (U32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
		{
			if (mInheritedMask[i] &&
				!(group->mOcclusionState[i] & mInheritedMask[i]))
			{
				llerrs << "Spatial group failed inherited mask test."
					   << llendl;
			}
		}

		if (group->hasState(LLSpatialGroup::DIRTY))
		{
			assert_parent_state(group, LLSpatialGroup::DIRTY);
		}
	}

	void assert_parent_state(LLSpatialGroup* group, U32 state)
	{
		LLSpatialGroup* parent = group->getParent();
		while (parent)
		{
			if (!parent->hasState(state))
			{
				llerrs << "Spatial group failed parent state check." << llendl;
			}
			parent = parent->getParent();
		}
	}

public:
	U32 mInheritedMask[LLViewerCamera::NUM_CAMERAS];
};

S32 get_physics_detail(const LLVector3& scale)
{
	constexpr S32 DEFAULT_DETAIL = 1;
	constexpr F32 LARGE_THRESHOLD = 5.f;
	constexpr F32 MEGA_THRESHOLD = 25.f;

	S32 detail = DEFAULT_DETAIL;
	F32 avg_scale = (scale[0] + scale[1] + scale[2]) / 3.f;

	if (avg_scale > LARGE_THRESHOLD)
	{
		++detail;
		if (avg_scale > MEGA_THRESHOLD)
		{
			++detail;
		}
	}

	return detail;
}

void renderMeshBaseHull(LLVOVolume* volp, U32 data_mask, LLColor4& color,
						LLColor4& line_color)
{
	LLUUID mesh_id = volp->getVolume()->getParams().getSculptID();
	LLModel::Decomposition* decompp = gMeshRepo.getDecomposition(mesh_id);

	static const LLVector3 size(0.25f, 0.25f, 0.25f);

	if (decompp)
	{
		if (!decompp->mBaseHullMesh.empty())
		{
			gGL.diffuseColor4fv(color.mV);
			LLVertexBuffer::drawArrays(LLRender::TRIANGLES,
									   decompp->mBaseHullMesh.mPositions);
			if (gUsePBRShaders)
			{
				return;
			}
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			gGL.diffuseColor4fv(line_color.mV);
			LLVertexBuffer::drawArrays(LLRender::TRIANGLES,
									   decompp->mBaseHullMesh.mPositions);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		else
		{
			gMeshRepo.buildPhysicsMesh(*decompp);
			gGL.diffuseColor4f(0.f, 1.f, 1.f, 1.f);
			drawBoxOutline(LLVector3::zero, size);
		}
	}
	else
	{
		gGL.diffuseColor3f(1.f, 0.f, 1.f);
		drawBoxOutline(LLVector3::zero, size);
	}
}

void render_hull(LLModel::PhysicsMesh& mesh, const LLColor4& color,
				 const LLColor4& line_color)
{
	if (mesh.mPositions.empty() || mesh.mNormals.empty())
	{
		return;
	}
	gGL.diffuseColor4fv(color.mV);
	LLVertexBuffer::drawArrays(LLRender::TRIANGLES, mesh.mPositions);
	if (gUsePBRShaders)
	{
		// Outlines removed from LL's PBR viewer. *TODO: check and see if there
		// is any valid reason to remove them... HB
		return;
	}
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	gGL.lineWidth(3.f);
	gGL.diffuseColor4fv(line_color.mV);
	LLVertexBuffer::drawArrays(LLRender::TRIANGLES, mesh.mPositions);
	gGL.lineWidth(1.f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void renderPhysicsShape(LLVOVolume* volp, bool wireframe)
{
	U8 phys_type = volp->getPhysicsShapeType();
	if (phys_type == LLViewerObject::PHYSICS_SHAPE_NONE || volp->isFlexible())
	{
		return;
	}

	// Not allowed to return at this point without rendering *something*

	static LLCachedControl<F32> threshold(gSavedSettings,
										  "ObjectCostHighThreshold");
	F32 cost = volp->getObjectCost();

	static LLCachedControl<LLColor4> low(gSavedSettings, "ObjectCostLowColor");
	static LLCachedControl<LLColor4> mid(gSavedSettings, "ObjectCostMidColor");
	static LLCachedControl<LLColor4> high(gSavedSettings,
										  "ObjectCostHighColor");

	F32 normalized_cost = 1.f - expf(-cost / threshold);

	LLColor4 color;
	if (normalized_cost <= 0.5f)
	{
		color = lerp(low, mid, 2.f * normalized_cost);
	}
	else
	{
		color = lerp(mid, high, 2.f * (normalized_cost - 0.5f));
	}
	if (wireframe)
	{
		color *= 0.5f;
	}

	LLColor4 line_color = color * 0.5f;

	U32 data_mask = LLVertexBuffer::MAP_VERTEX;

	LLVolumeParams vol_params = volp->getVolume()->getParams();

	bool convex = phys_type == LLViewerObject::PHYSICS_SHAPE_CONVEX_HULL;
	LLPhysicsVolumeParams phys_params(vol_params, convex);

	bool has_decomp = false;
	static LLCachedControl<bool> hide_convex(gSavedSettings,
											 "HideConvexPhysShapes");
	if (hide_convex)
	{
		const LLUUID& mesh_id = vol_params.getSculptID();
		LLModel::Decomposition* decomp = gMeshRepo.getDecomposition(mesh_id);
		has_decomp = decomp && !decomp->mHull.empty();
	}

	LLPhysShapeBuilderUtil::ShapeSpec physics_spec;
	LLPhysShapeBuilderUtil::getPhysShape(phys_params, volp->getScale(),
											has_decomp, physics_spec);

	U32 type = physics_spec.getType();

	LLVector3 size(0.25f, 0.25f, 0.25f);

	gGL.pushMatrix();
	gGL.multMatrix(volp->getRelativeXform().getF32ptr());

	LLGLEnable(gUsePBRShaders ? 0 : GL_POLYGON_OFFSET_LINE);
	if (!gUsePBRShaders)
	{
		glPolygonOffset(3.f, 3.f);
	}

	if (type == LLPhysShapeBuilderUtil::ShapeSpec::USER_MESH)
	{
		const LLUUID& mesh_id = volp->getVolume()->getParams().getSculptID();
		LLModel::Decomposition* decomp = gMeshRepo.getDecomposition(mesh_id);
		if (decomp)
		{
			// Render a physics based mesh

			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

			if (!decomp->mHull.empty())
			{
				// Decomposition exists, use that
				if (decomp->mMesh.empty())
				{
					gMeshRepo.buildPhysicsMesh(*decomp);
				}

				for (U32 i = 0; i < decomp->mMesh.size(); ++i)
				{
					render_hull(decomp->mMesh[i], color, line_color);
				}
			}
			else if (!decomp->mPhysicsShapeMesh.empty())
			{
				// Decomp has physics mesh, render that mesh
				gGL.diffuseColor4fv(color.mV);
				const auto& positions = decomp->mPhysicsShapeMesh.mPositions;
				LLVertexBuffer::drawArrays(LLRender::TRIANGLES, positions);

				if (!gUsePBRShaders)
				{
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
					gGL.diffuseColor4fv(line_color.mV);
					LLVertexBuffer::drawArrays(LLRender::TRIANGLES, positions);
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				}
			}
			else
			{
				// No mesh or decomposition, render base hull
				renderMeshBaseHull(volp, data_mask, color, line_color);

				if (decomp->mPhysicsShapeMesh.empty())
				{
					// Attempt to fetch physics shape mesh if available
					gMeshRepo.fetchPhysicsShape(mesh_id);
				}
			}
		}
		else
		{
			gGL.diffuseColor3f(1.f, 1.f, 0.f);
			drawBoxOutline(LLVector3::zero, size);
		}
	}
	else if (type == LLPhysShapeBuilderUtil::ShapeSpec::USER_CONVEX ||
			 type == LLPhysShapeBuilderUtil::ShapeSpec::PRIM_CONVEX)
	{
		if (volp->isMesh())
		{
			renderMeshBaseHull(volp, data_mask, color, line_color);
		}
		else
		{
			LLVolumeParams vol_params = volp->getVolume()->getParams();
			S32 detail = get_physics_detail(volp->getScale());
			LLVolume* phys_volp = gVolumeMgrp->refVolume(vol_params, detail);
			if (!phys_volp->mHullPoints)
			{
				// Build convex hull
				std::vector<LLVector3> pos;
				std::vector<U16> index;

				S32 index_offset = 0;

				for (S32 i = 0, count = phys_volp->getNumVolumeFaces();
					 i < count; ++i)
				{
					const LLVolumeFace& face = phys_volp->getVolumeFace(i);
					if (index_offset + face.mNumVertices > 65535)
					{
						continue;
					}

					for (S32 j = 0; j < face.mNumVertices; ++j)
					{
						pos.emplace_back(face.mPositions[j].getF32ptr());
					}

					for (S32 j = 0; j < face.mNumIndices; ++j)
					{
						index.push_back(face.mIndices[j] + index_offset);
					}

					index_offset += face.mNumVertices;
				}

				LLConvexDecomposition* decomp =
					LLConvexDecomposition::getInstance();
				if (decomp && !pos.empty() && !index.empty())
				{
					LLCDMeshData mesh;
					mesh.mIndexBase = &index[0];
					mesh.mVertexBase = pos[0].mV;
					mesh.mNumVertices = pos.size();
					mesh.mVertexStrideBytes = 12;
					mesh.mIndexStrideBytes = 6;
					mesh.mIndexType = LLCDMeshData::INT_16;

					mesh.mNumTriangles = index.size() / 3;

					LLCDMeshData res;
					decomp->generateSingleHullMeshFromMesh(&mesh, &res);

					// Copy res into phys_volp
					phys_volp->mHullPoints =
						(LLVector4a*)allocate_volume_mem(sizeof(LLVector4a) *
														 res.mNumVertices);
					if (!phys_volp->mHullPoints)
					{
						gVolumeMgrp->unrefVolume(phys_volp);
						gGL.popMatrix();
						return;
					}
					phys_volp->mNumHullPoints = res.mNumVertices;

					S32 idx_size = (res.mNumTriangles * 3 * 2 + 0xF) & ~0xF;
					phys_volp->mHullIndices =
						(U16*)allocate_volume_mem(idx_size);
					if (!phys_volp->mHullIndices)
					{
						free_volume_mem(phys_volp->mHullPoints);
						gVolumeMgrp->unrefVolume(phys_volp);
						gGL.popMatrix();
						return;
					}
					phys_volp->mNumHullIndices = res.mNumTriangles * 3;

					const F32* v = res.mVertexBase;

					for (S32 i = 0; i < res.mNumVertices; ++i)
					{
						F32* p = (F32*)((U8*)v+i*res.mVertexStrideBytes);
						phys_volp->mHullPoints[i].load3(p);
					}

					if (res.mIndexType == LLCDMeshData::INT_16)
					{
						for (S32 i = 0; i < res.mNumTriangles; ++i)
						{
							U16* idx = (U16*)(((U8*)res.mIndexBase) +
											  i * res.mIndexStrideBytes);

							phys_volp->mHullIndices[i * 3] = idx[0];
							phys_volp->mHullIndices[i * 3 + 1] = idx[1];
							phys_volp->mHullIndices[i * 3 + 2] = idx[2];
						}
					}
					else
					{
						for (S32 i = 0; i < res.mNumTriangles; ++i)
						{
							U32* idx = (U32*)(((U8*)res.mIndexBase) +
											  i * res.mIndexStrideBytes);

							phys_volp->mHullIndices[i * 3] = (U16)idx[0];
							phys_volp->mHullIndices[i * 3 + 1] = (U16)idx[1];
							phys_volp->mHullIndices[i * 3 + 2] = (U16)idx[2];
						}
					}
				}
			}

			if (phys_volp->mHullPoints && phys_volp->mNumHullIndices &&
				phys_volp->mHullIndices && phys_volp->mNumHullPoints)
			{
				// Render hull
				if (!gUsePBRShaders)
				{
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				}
				gGL.diffuseColor4fv(line_color.mV);
				LLVertexBuffer::unbind();
				LLVertexBuffer::drawElements(phys_volp->mNumHullPoints,
											 phys_volp->mHullPoints, NULL,
											 phys_volp->mNumHullIndices,
											 phys_volp->mHullIndices);
				if (!gUsePBRShaders)
				{
					gGL.diffuseColor4fv(color.mV);
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
					LLVertexBuffer::drawElements(phys_volp->mNumHullPoints,
												 phys_volp->mHullPoints, NULL,
												 phys_volp->mNumHullIndices,
												 phys_volp->mHullIndices);
				}
			}
			else
			{
				gGL.diffuseColor4f(1.f, 0.1f, 1.f, 1.f);
				drawBoxOutline(LLVector3::zero, size);
			}

			free_volume_mem(phys_volp->mHullPoints);
			free_volume_mem(phys_volp->mHullIndices);
			gVolumeMgrp->unrefVolume(phys_volp);
		}
	}
	else if (type == LLPhysShapeBuilderUtil::ShapeSpec::BOX)
	{
		if (!wireframe)
		{
			LLVector3 center = physics_spec.getCenter();
			LLVector3 scale = physics_spec.getScale();
			LLVector3 vscale = volp->getScale() * 2.f;
			scale.set(scale[0] / vscale[0], scale[1] / vscale[1],
					  scale[2] / vscale[2]);

			gGL.diffuseColor4fv(color.mV);
			drawBox(center, scale);
		}
	}
	else if	(type == LLPhysShapeBuilderUtil::ShapeSpec::SPHERE)
	{
		if (!wireframe)
		{
			LLVolumeParams vol_params;
			vol_params.setType(LL_PCODE_PROFILE_CIRCLE_HALF,
							   LL_PCODE_PATH_CIRCLE);
			vol_params.setBeginAndEndS(0.f, 1.f);
			vol_params.setBeginAndEndT(0.f, 1.f);
			vol_params.setRatio(1.f, 1.f);
			vol_params.setShear(0.f, 0.f);
			LLVolume* spherep = gVolumeMgrp->refVolume(vol_params, 3);
			gGL.diffuseColor4fv(color.mV);
			pushVerts(spherep);
			gVolumeMgrp->unrefVolume(spherep);
		}
	}
	else if (type == LLPhysShapeBuilderUtil::ShapeSpec::CYLINDER)
	{
		if (!wireframe)
		{
			LLVolumeParams vol_params;
			vol_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE);
			vol_params.setBeginAndEndS(0.f, 1.f);
			vol_params.setBeginAndEndT(0.f, 1.f);
			vol_params.setRatio(1.f, 1.f);
			vol_params.setShear(0.f, 0.f);
			LLVolume* cylinderp = gVolumeMgrp->refVolume(vol_params, 3);
			gGL.diffuseColor4fv(color.mV);
			pushVerts(cylinderp);
			gVolumeMgrp->unrefVolume(cylinderp);
		}
	}
	else if (type == LLPhysShapeBuilderUtil::ShapeSpec::PRIM_MESH)
	{
		LLVolumeParams vol_params = volp->getVolume()->getParams();
		S32 detail = get_physics_detail(volp->getScale());

		LLVolume* phys_volp = gVolumeMgrp->refVolume(vol_params, detail);

		gGL.diffuseColor4fv(line_color.mV);
		pushVerts(phys_volp);

		if (!gUsePBRShaders)
		{
			gGL.diffuseColor4fv(color.mV);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			pushVerts(phys_volp);
		}

		gVolumeMgrp->unrefVolume(phys_volp);
	}
	else if (type == LLPhysShapeBuilderUtil::ShapeSpec::PRIM_CONVEX)
	{
		LLVolumeParams vol_params = volp->getVolume()->getParams();
		S32 detail = get_physics_detail(volp->getScale());

		LLVolume* phys_volp = gVolumeMgrp->refVolume(vol_params, detail);

		if (phys_volp->mHullPoints && phys_volp->mHullIndices)
		{
			if (gUsePBRShaders)
			{
				gGL.diffuseColor4fv(color.mV);
				LLVertexBuffer::unbind();
				glVertexPointer(3, GL_FLOAT, 16, phys_volp->mHullPoints);
				gGL.diffuseColor4fv(line_color.mV);
				gGL.syncMatrices();
				glDrawElements(GL_TRIANGLES, phys_volp->mNumHullIndices,
						 	   GL_UNSIGNED_SHORT, phys_volp->mHullIndices);
			}
			else
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				gGL.diffuseColor4fv(line_color.mV);
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				LLVertexBuffer::drawElements(phys_volp->mNumHullPoints,
											 phys_volp->mHullPoints, NULL,
											 phys_volp->mNumHullIndices,
 											 phys_volp->mHullIndices);

				gGL.diffuseColor4fv(color.mV);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				LLVertexBuffer::drawElements(phys_volp->mNumHullPoints,
											 phys_volp->mHullPoints, NULL,
											 phys_volp->mNumHullIndices,
 											 phys_volp->mHullIndices);
			}
		}
		else
		{
			gGL.diffuseColor3f(1.f, 0.f, 1.f);
			drawBoxOutline(LLVector3::zero, size);
			gMeshRepo.buildHull(vol_params, detail);
		}
		gVolumeMgrp->unrefVolume(phys_volp);
	}
	else if (type == LLPhysShapeBuilderUtil::ShapeSpec::SCULPT)
	{
		// *TODO: implement sculpted prim physics display
	}
	else
	{
		llerrs << "Unhandled type" << llendl;
	}

	gGL.popMatrix();
}

void renderPhysicsShapes(LLSpatialGroup* groupp, bool wireframe)
{
	const LLSpatialGroup::element_list& data_vec = groupp->getData();
	for (U32 i = 0, count = data_vec.size(); i < count; ++i)
	{
		LLDrawable* drawp = (LLDrawable*)data_vec[i]->getDrawable();
		if (!drawp || drawp->isDead()) continue;

		LLSpatialPartition* partp = drawp->asPartition();
		if (partp)
		{
			LLSpatialBridge* bridgep = drawp->asPartition()->asBridge();
			if (bridgep && bridgep->mDrawable)
			{
				gGL.pushMatrix();
				gGL.multMatrix(bridgep->mDrawable->getRenderMatrix().getF32ptr());
				bridgep->renderPhysicsShapes(wireframe);
				gGL.popMatrix();
			}
			continue;
		}

		LLVOVolume* volp = drawp->getVOVolume();
		if (volp && !volp->isAttachment() &&
			volp->getPhysicsShapeType() != LLViewerObject::PHYSICS_SHAPE_NONE)
		{
			if (groupp->getSpatialPartition()->isBridge())
			{
				renderPhysicsShape(volp, wireframe);
			}
			else
			{
				gGL.pushMatrix();
				LLVector3 trans = drawp->getRegion()->getOriginAgent();
				gGL.translatef(trans.mV[0], trans.mV[1], trans.mV[2]);
				renderPhysicsShape(volp, wireframe);
				gGL.popMatrix();
			}
			continue;
		}

		// Terrain physics shape not rendered by LL's PBR viewer
		if (gUsePBRShaders)
		{
			return;
		}

		LLViewerObject* objp = drawp->getVObj();
		if (objp && objp->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH)
		{
			gGL.pushMatrix();
			gGL.multMatrix(objp->getRegion()->mRenderMatrix.getF32ptr());
			// Push face vertices for terrain
			for (S32 j = 0, count = drawp->getNumFaces(); j < count; ++j)
			{
				LLFace* facep = drawp->getFace(j);
				if (!facep) continue;

				LLVertexBuffer* buffp = facep->getVertexBuffer();
				if (!buffp) continue;

				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				buffp->setBuffer(LLVertexBuffer::MAP_VERTEX);
				gGL.diffuseColor3f(0.2f, 0.5f, 0.3f);
				buffp->draw(LLRender::TRIANGLES, buffp->getNumIndices(), 0);
				gGL.diffuseColor3f(0.2f, 1.f, 0.3f);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				buffp->draw(LLRender::TRIANGLES, buffp->getNumIndices(), 0);
				gGL.popMatrix();
			}
		}
	}
}

class LLOctreeRenderPhysicsShapes final : public OctreeTraveler
{
public:
	LLOctreeRenderPhysicsShapes(LLCamera* camerap, bool wireframe)
	:	mCamera(camerap),
		mWireframe(wireframe)
	{
	}

	void traverse(const OctreeNode* nodep) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)nodep->getListener(0);
		if (!groupp) return;

		const LLVector4a* bounds = groupp->getBounds();
		if (!mCamera ||
			mCamera->AABBInFrustumNoFarClip(bounds[0], bounds[1]))
		{
			nodep->accept(this);

			for (U32 i = 0; i < nodep->getChildCount(); ++i)
			{
				traverse(nodep->getChild(i));
			}

			groupp->rebuildGeom();
			groupp->rebuildMesh();

			renderPhysicsShapes(groupp, mWireframe);
		}
	}

	LL_INLINE void visit(const OctreeNode*) override
	{
	}

public:
	LLCamera*	mCamera;
	bool		mWireframe;
};

void LLSpatialPartition::renderPhysicsShapes(bool wireframe)
{
	gGL.flush();
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	if (!gUsePBRShaders)
	{
		wireframe = false;
		gGL.lineWidth(3.f);
	}
	LLCamera* camerap = isBridge() ? NULL : &gViewerCamera;
	LLOctreeRenderPhysicsShapes render_physics(camerap, wireframe);
	render_physics.traverse(mOctree);
	gGL.flush();
	if (!gUsePBRShaders)
	{
		gGL.lineWidth(1.f);
	}
}

void LLSpatialPartition::renderDebug()
{
	if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE |
									  LLPipeline::RENDER_DEBUG_OCCLUSION |
									  LLPipeline::RENDER_DEBUG_LIGHTS |
									  LLPipeline::RENDER_DEBUG_BATCH_SIZE |
									  LLPipeline::RENDER_DEBUG_UPDATE_TYPE |
									  LLPipeline::RENDER_DEBUG_BBOXES |
									  LLPipeline::RENDER_DEBUG_NORMALS |
									  LLPipeline::RENDER_DEBUG_POINTS |
									  LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY |
									  LLPipeline::RENDER_DEBUG_TEXTURE_ANIM |
									  LLPipeline::RENDER_DEBUG_RAYCAST |
									  LLPipeline::RENDER_DEBUG_AVATAR_VOLUME |
									  LLPipeline::RENDER_DEBUG_AVATAR_JOINTS |
									  LLPipeline::RENDER_DEBUG_AGENT_TARGET |
									  LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA |
									  LLPipeline::RENDER_DEBUG_RENDER_COMPLEXITY))
	{
		return;
	}
//MK
	if (gRLenabled && gRLInterface.mVisionRestricted)
	{
		return;
	}
//mk

	gDebugProgram.bind();

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
	{
		sLastMaxTexPriority = (F32)gViewerCamera.getScreenPixelArea();
		sCurMaxTexPriority = 0.f;
	}

	LLGLDisable cullface(GL_CULL_FACE);
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gPipeline.disableLights();

	LLCamera* camerap = isBridge() ? NULL : &gViewerCamera;

	LLOctreeStateCheck checker;
	checker.traverse(mOctree);

	LLOctreeRenderNonOccluded render_debug(camerap);
	render_debug.traverse(mOctree);

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION))
	{
		LLGLEnable cull(GL_CULL_FACE);

		LLGLEnable blend(GL_BLEND);
		LLGLDepthTest depth_under(GL_TRUE, GL_FALSE, GL_GREATER);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		gGL.diffuseColor4f(0.5f, 0.f, 0.f, 0.25f);

		LLGLEnable offset(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(-1.f, -1.f);

		LLOctreeRenderXRay xray(camerap);
		xray.traverse(mOctree);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	gDebugProgram.unbind();

	stop_glerror();
}

void LLSpatialGroup::drawObjectBox(LLColor4 col)
{
	gGL.diffuseColor4fv(col.mV);
	LLVector4a size;
	size = mObjectBounds[1];
	size.mul(1.01f);
	size.add(LLVector4a(0.001f));
	drawBox(mObjectBounds[0], size);
}

bool LLSpatialPartition::isHUDPartition()
{
	return mPartitionType == LLViewerRegion::PARTITION_HUD;
}

bool LLSpatialPartition::isVisible(const LLVector3& v)
{
	return gViewerCamera.sphereInFrustum(v, 4.f) != 0;
}

LLDrawable* LLSpatialPartition::lineSegmentIntersect(const LLVector4a& start,
													 const LLVector4a& end,
													 bool pick_transparent,
													 bool pick_rigged,
													 S32* face_hit,
													 LLVector4a* intersection,
													 LLVector2* tex_coord,
													 LLVector4a* normal,
													 LLVector4a* tangent)
{
	LLOctreeIntersect intersect(start, end, pick_transparent, pick_rigged,
								face_hit, intersection, tex_coord, normal,
								tangent);
	return intersect.check(mOctree);
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawInfo class
///////////////////////////////////////////////////////////////////////////////

LLDrawInfo::LLDrawInfo(U16 start, U16 end, U32 count, U32 offset,
					   LLViewerTexture* texp, LLVertexBuffer* bufferp,
					   bool fullbright, U8 bump)
:	mVertexBuffer(bufferp),
	mTexture(texp),
	mTextureMatrix(NULL),
	mModelMatrix(NULL),
	mStart(start),
	mEnd(end),
	mCount(count),
	mOffset(offset),
	mFullbright(fullbright),
	mBump(bump),
	mVSize(0.f),
	mDistance(0.f),
	mBlendFuncSrc(LLRender::BF_SOURCE_ALPHA),
	mBlendFuncDst(LLRender::BF_ONE_MINUS_SOURCE_ALPHA),
	mHasGlow(false),
	mMaterial(NULL),
	mShaderMask(0),
	mSpecColor(1.f, 1.f, 1.f, 0.5f),
	mEnvIntensity(0.f),
	mDiffuseAlphaMode(0),
	mAlphaMaskCutoff(0.5f)
{
	if (gDebugGL)
	{
		mVertexBuffer->validateRange(mStart, mEnd, mCount, mOffset);
	}
}

LLDrawInfo::~LLDrawInfo()
{
#if LL_DEBUG
	if (gDebugGL)
	{
		gPipeline.checkReferences(this);
	}
#endif
}

void LLDrawInfo::validate()
{
	if (!mVertexBuffer->validateRange(mStart, mEnd, mCount, mOffset))
	{
		llwarns << "Invalid range !" << llendl;
	}
}

U64 LLDrawInfo::getSkinHash()
{
	return mSkinInfo.notNull() ? mSkinInfo->mHash : 0;
}

LLColor4U LLDrawInfo::getDebugColor()
{
	if (mDebugColor != LLColor4U::black)
	{
		// When the debug color has already been computed once, we use the
		// cached value to speed up rendering (even though this is just a
		// debug render feature) since we do not care whether the draw info
		// parameters changed or not (in the past, the debug color was picked
		// up at random in the LLDrawInfo() constructor). HB
		return mDebugColor;
	}

	constexpr F32 DEBUG_COLOR_ALPHA = 160.f;	// Used to be 200. HB
	// *HACK: hash the bytes of this object but do not include the ref count
	constexpr size_t offset = sizeof(LLRefCount);
	U64 digest = HBXXH64::digest((const void*)((const char*)this + offset),
								 sizeof(*this) - offset);
	*((U32*)mDebugColor.mV) = digest64to32(digest);
	mDebugColor.mV[3] = DEBUG_COLOR_ALPHA;
	return mDebugColor;
}

///////////////////////////////////////////////////////////////////////////////
// LLGeometryManager class
///////////////////////////////////////////////////////////////////////////////

LLVertexBuffer* LLGeometryManager::createVertexBuffer(U32 type_mask)
{
#if LL_DEBUG_VB_ALLOC
	LLVertexBuffer* vb = new LLVertexBuffer(type_mask);
	vb->setOwner(llformat("LLGeometryManager type %d", type_mask).c_str());
	return vb;
#else
	return new LLVertexBuffer(type_mask);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// LLCullResult class
///////////////////////////////////////////////////////////////////////////////

void LLCullResult::clear()
{
	mVisibleGroups.clear();
	mAlphaGroups.clear();
	mRiggedAlphaGroups.clear();
	mOcclusionGroups.clear();
	mDrawableGroups.clear();
	mVisibleList.clear();
	mVisibleBridge.clear();

	for (U32 i = 0; i < LLRenderPass::NUM_RENDER_TYPES; ++i)
	{
		mRenderMap[i].clear();
	}
}

void LLCullResult::pushDrawInfo(U32 type, LLDrawInfo* infop)
{
	if (infop && type < LLRenderPass::NUM_RENDER_TYPES)
	{
		mRenderMap[type].push_back(infop);
	}
}

void LLCullResult::assertDrawMapsEmpty()
{
	for (U32 i = 0; i < LLRenderPass::NUM_RENDER_TYPES; ++i)
	{
		if (hasRenderMap(i))
		{
			llerrs << "Stale LLDrawInfo's in LLCullResult !" << llendl;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Used by LLSpatialBridge

class LLOctreeMarkNotCulled final : public OctreeTraveler
{
public:
	LLOctreeMarkNotCulled(LLCamera* camerap)
	:	mCamera(camerap)
	{
	}

	void traverse(const OctreeNode* nodep) override
	{
		if (!nodep)
		{
			llwarns_sparse << "NULL node !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		LLSpatialGroup* groupp = (LLSpatialGroup*)nodep->getListener(0);
		if (!groupp)
		{
			llwarns_once << "NULL satial group for node " << nodep
						 << " !  Skipping..." << llendl;
			llassert(false);
			return;
		}

		groupp->setVisible();
		OctreeTraveler::traverse(nodep);
	}

	void visit(const OctreeNode* branchp) override
	{
		if (!branchp)
		{
			llwarns_sparse << "NULL branch !  Skipping..." << llendl;
			llassert(false);
			return;
		}
		gPipeline.markNotCulled((LLSpatialGroup*)branchp->getListener(0),
								*mCamera);
	}

public:
	LLCamera* mCamera;
};

///////////////////////////////////////////////////////////////////////////////
// LLSpatialBridge class. Spatial partition bridging drawable.
// Used to be in lldrawable.cpp, which was silly since it is declared in
// llspatialpartition.h. HB
///////////////////////////////////////////////////////////////////////////////

#define FORCE_INVISIBLE_AREA 16.f

LLSpatialBridge::LLSpatialBridge(LLDrawable* root, bool render_by_group,
								 U32 data_mask, LLViewerRegion* regionp)
:	LLDrawable(root->getVObj(), true),
	LLSpatialPartition(data_mask, render_by_group, regionp)
{
	llassert(root && root->getRegion());

	mOcclusionEnabled = false;
	mBridge = this;
	mDrawable = root;
	root->setSpatialBridge(this);

	mRenderType = mDrawable->mRenderType;
	mDrawableType = mDrawable->mRenderType;

	mPartitionType = LLViewerRegion::PARTITION_VOLUME;

	mOctree->balance();

	LLSpatialPartition* part =
		mDrawable->getRegion()->getSpatialPartition(mPartitionType);
	// PARTITION_VOLUME cannot be NULL
	llassert(part);
	part->put(this);
}

LLSpatialBridge::~LLSpatialBridge()
{
	if (mEntry)
	{
		LLSpatialGroup* groupp = getSpatialGroup();
		if (groupp)
		{
			groupp->getSpatialPartition()->remove(this, groupp);
		}
	}

	// Delete octree here so listeners will still be able to access bridge
	// specific state
	destroyTree();
}

void LLSpatialBridge::destroyTree()
{
	delete mOctree;
	mOctree = NULL;
}

//virtual
void LLSpatialBridge::updateBinRadius()
{
	setBinRadius(llmin(mOctree->getSize()[0] * 0.5f, 256.f));
}

//virtual
void LLSpatialBridge::updateSpatialExtents()
{
	LLSpatialGroup* root = (LLSpatialGroup*)mOctree->getListener(0);

	{
		LL_FAST_TIMER(FTM_CULL_REBOUND);
		root->rebound();
	}

	const LLVector4a* root_bounds = root->getBounds();
	static LLVector4a size;
	size = root_bounds[1];

	// VECTORIZE THIS
	static LLMatrix4a mat;
	mat.loadu(mDrawable->getXform()->getWorldMatrix());

	static const LLVector4a t(0.f, 0.f, 0.f, 0.f);
	LLVector4a center;
	mat.affineTransform(t, center);

	static LLVector4a offset;
	mat.rotate(root_bounds[0], offset);
	center.add(offset);

	// Get 4 corners of bounding box
	static LLVector4a v[4];
	mat.rotate(size, v[0]);

	static LLVector4a scale;
	scale.set(-1.f, -1.f, 1.f);
	scale.mul(size);
	mat.rotate(scale, v[1]);

	scale.set(1.f, -1.f, -1.f);
	scale.mul(size);
	mat.rotate(scale, v[2]);

	scale.set(-1.f, 1.f, -1.f);
	scale.mul(size);
	mat.rotate(scale, v[3]);

	static LLVector4a new_min, new_max, min, max, delta;
	new_min = new_max = center;
	for (U32 i = 0; i < 4; ++i)
	{
		delta.setAbs(v[i]);
		min.setSub(center, delta);
		max.setAdd(center, delta);

		new_min.setMin(new_min, min);
		new_max.setMax(new_max, max);
	}
	setSpatialExtents(new_min, new_max);

	static LLVector4a diagonal;
	diagonal.setSub(new_max, new_min);
	mRadius = diagonal.getLength3().getF32() * 0.5f;

	LLVector4a& pos = getGroupPosition();
	pos.setAdd(new_min, new_max);
	pos.mul(0.5f);
	updateBinRadius();
}

void LLSpatialBridge::transformExtents(const LLVector4a* src, LLVector4a* dst)
{
	LLMatrix4a mat;
	mat.loadu(mDrawable->getXform()->getWorldMatrix());
	mat.invert();
	mat.matMulBoundBox(src, dst);
}

LLCamera LLSpatialBridge::transformCamera(LLCamera& camera)
{
	LLXformMatrix* mat = mDrawable->getXform();
	LLVector3 center = LLVector3::zero * mat->getWorldMatrix();
	LLQuaternion rot = ~mat->getRotation();
	LLCamera ret = camera;

	LLVector3 delta = (ret.getOrigin() - center) * rot;
	if (!delta.isFinite())
	{
		delta.clear();
	}

	LLVector3 look_at = ret.getAtAxis() * rot;
	LLVector3 up_axis = ret.getUpAxis() * rot;
	LLVector3 left_axis = ret.getLeftAxis() * rot;

	ret.setOrigin(delta);
	ret.setAxes(look_at, left_axis, up_axis);
	return ret;
}

//virtual
void LLSpatialBridge::setVisible(LLCamera& camera_in,
								 std::vector<LLDrawable*>* results,
								 bool for_select)
{
	if (!gPipeline.hasRenderType(mDrawableType))
	{
		return;
	}

	// *HACK: do not draw attachments for avatars that have not been visible in
	// more than a frame
	LLViewerObject* vobjp = mDrawable->getVObj();
	if (vobjp && vobjp->isAttachment() && !vobjp->isHUDAttachment())
	{
		LLDrawable* parentp = mDrawable->getParent();
		if (parentp)
		{
			LLViewerObject* objparentp = parentp->getVObj();
			if (!objparentp || objparentp->isDead())
			{
				return;
			}
			if (objparentp->isAvatar())
			{
				LLVOAvatar* avatarp = (LLVOAvatar*)objparentp;
				if (!avatarp->isVisible() || avatarp->isImpostor() ||
					!avatarp->isFullyLoaded())
				{
					return;
				}
			}

			LLDrawable* drawablep = objparentp->mDrawable;
			LLSpatialGroup* groupp = drawablep->getSpatialGroup();
			if (!groupp ||
				LLViewerOctreeEntryData::getCurrentFrame() -
				drawablep->getVisible() > 1)
			{
				return;
			}
		}
	}

	LLSpatialGroup* groupp = (LLSpatialGroup*)mOctree->getListener(0);
	groupp->rebound();

	LLVector4a center;
	const LLVector4a* exts = getSpatialExtents();
	center.setAdd(exts[0], exts[1]);
	center.mul(0.5f);
	LLVector4a size;
	size.setSub(exts[1], exts[0]);
	size.mul(0.5f);

	if ((LLPipeline::sShadowRender && camera_in.AABBInFrustum(center, size)) ||
		LLPipeline::sImpostorRender ||
		(camera_in.AABBInFrustumNoFarClip(center, size) &&
		AABBSphereIntersect(exts[0], exts[1], camera_in.getOrigin(),
							camera_in.mFrustumCornerDist)))
	{
		if (!LLPipeline::sImpostorRender && !LLPipeline::sShadowRender &&
			LLPipeline::calcPixelArea(center, size, camera_in) < FORCE_INVISIBLE_AREA)
		{
			return;
		}

		LLDrawable::setVisible(camera_in);

		if (for_select)
		{
			results->push_back(mDrawable);
			if (mDrawable->getVObj())
			{
				LLViewerObject::const_child_list_t& child_list =
					mDrawable->getVObj()->getChildren();
				for (LLViewerObject::child_list_t::const_iterator
						iter = child_list.begin(), end = child_list.end();
					 iter != end; ++iter)
				{
					LLViewerObject* child = *iter;
					LLDrawable* drawable = child->mDrawable;
					if (drawable)
					{
						results->push_back(drawable);
					}
				}
			}
		}
		else
		{
			LLCamera trans_camera = transformCamera(camera_in);
			LLOctreeMarkNotCulled culler(&trans_camera);
			culler.traverse(mOctree);
		}
	}
}

//virtual
void LLSpatialBridge::updateDistance(LLCamera& camera_in, bool force_update)
{
	if (!mDrawable)
	{
		markDead();
		return;
	}

	if (gShiftFrame)
	{
		return;
	}

	if (mDrawable->getVObj())
	{
		if (mDrawable->getVObj()->isAttachment())
		{
			LLDrawable* parentp = mDrawable->getParent();
			if (parentp && parentp->getVObj())
			{
				LLVOAvatar* av = parentp->getVObj()->asAvatar();
				if (av && av->isImpostor())
				{
					return;
				}
			}
		}

		LLCamera camera = transformCamera(camera_in);

		mDrawable->updateDistance(camera, force_update);

		LLViewerObject::const_child_list_t& child_list =
			mDrawable->getVObj()->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter = child_list.begin(), end = child_list.end();
			 iter != end; ++iter)
		{
			LLViewerObject* childp = *iter;
			LLDrawable* drawablep = childp->mDrawable;
			if (drawablep && !drawablep->isAvatar())
			{
				drawablep->updateDistance(camera, force_update);
			}
		}
	}
}

//virtual
void LLSpatialBridge::makeActive()
{
	// It is an error to make a spatial bridge active (it is already active)
	llerrs << "makeActive called on spatial bridge" << llendl;
}

//virtual
void LLSpatialBridge::move(LLDrawable* drawablep, LLSpatialGroup* curp,
						   bool immediate)
{
	LLSpatialPartition::move(drawablep, curp, immediate);
	gPipeline.markMoved(this, false);
}

//virtual
bool LLSpatialBridge::updateMove()
{
	if (mDrawable && mDrawable->mVObjp && mDrawable->getRegion())
	{
		LLSpatialPartition* partp =
			mDrawable->getRegion()->getSpatialPartition(mPartitionType);
		mOctree->balance();
		if (partp)
		{
			partp->move(this, getSpatialGroup(), true);
		}
		return true;
	}

	llwarns_sparse << "Bad spatial bridge (NULL drawable or mVObjp or region)."
				   << llendl;
	return false;
}

//virtual
void LLSpatialBridge::shiftPos(const LLVector4a& vec)
{
	LLDrawable::shift(vec);
}

//virtual
void LLSpatialBridge::cleanupReferences()
{
	// Hold a LLPointer on mDrawable (which is a LLDrawable*) to prevent it
	// from getting destroyed during this method execution, should its refcount
	// fall down to 0. HB
	LLPointer<LLDrawable> drawablep = mDrawable;

	LLDrawable::cleanupReferences();

	if (drawablep.notNull())
	{
		LLViewerObject* vobjp = drawablep->getVObj().get();
		if (vobjp)
		{
			// In order to guard against modifications to the children list
			// that would result from setGroup(NULL) on them, build a vector
			// of LLPointer's on the drawables to operate upon, and then use
			// that vector's pointers to setGroup(NULL). HB
			LLViewerObject::const_child_list_t& child_list =
				vobjp->getChildren();
			std::vector<LLPointer<LLDrawable> > drawvec;
			drawvec.reserve(child_list.size());
			for (LLViewerObject::child_list_t::const_iterator
					iter = child_list.begin(), end = child_list.end();
				 iter != end; ++iter)
			{
				LLViewerObject* childp = iter->get();
				if (childp)
				{
					LLDrawable* drawp = childp->mDrawable.get();
					if (drawp)
					{
						drawvec.emplace_back(drawp);
					}
				}
			}
			// Now we can safely operate on children's drawables. HB
			for (U32 i = 0, count = drawvec.size(); i < count; ++i)
			{
				drawvec[i]->setGroup(NULL);
			}
		}
		// Do this *after* it got done on children. HB
		drawablep->setGroup(NULL);
		drawablep->setSpatialBridge(NULL);
		mDrawable = NULL;
	}
}

LLBridgePartition::LLBridgePartition(LLViewerRegion* regionp)
:	LLSpatialPartition(0, false, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_VOLUME;
	mPartitionType = LLViewerRegion::PARTITION_BRIDGE;
	mLODPeriod = 16;
	mSlopRatio = 0.25f;
}

LLAvatarPartition::LLAvatarPartition(LLViewerRegion* regionp)
:	LLBridgePartition(regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_AVATAR;
	mPartitionType = LLViewerRegion::PARTITION_AVATAR;
}

LLPuppetPartition::LLPuppetPartition(LLViewerRegion* regionp)
:	LLBridgePartition(regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_PUPPET;
	mPartitionType = LLViewerRegion::PARTITION_PUPPET;
}

LLHUDBridge::LLHUDBridge(LLDrawable* drawablep, LLViewerRegion* regionp)
:	LLVolumeBridge(drawablep, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_HUD;
	mPartitionType = LLViewerRegion::PARTITION_HUD;
	mSlopRatio = 0.f;
}
