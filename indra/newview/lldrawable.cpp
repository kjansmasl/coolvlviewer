/**
 * @file lldrawable.cpp
 * @brief LLDrawable class implementation
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

#include "lldrawable.h"

#include "imageids.h"
#include "llfasttimer.h"
#include "llmatrix4a.h"
#include "llvolume.h"

#include "llagent.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llsurfacepatch.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvoavatarpuppet.h"
#include "llvocache.h"
#include "llvovolume.h"
#include "llworld.h"

constexpr F32 MIN_INTERPOLATE_DISTANCE_SQUARED = 0.001f * 0.001f;
constexpr F32 MAX_INTERPOLATE_DISTANCE_SQUARED = 10.f * 10.f;
constexpr F32 OBJECT_DAMPING_TIME_CONSTANT = 0.06f;

//////////////////////////////
//
// Drawable code
//
//

// static
U32 LLDrawable::sNumZombieDrawables = 0;
F32 LLDrawable::sCurPixelAngle = 0;
#if LL_DEBUG
std::vector<LLPointer<LLDrawable> > LLDrawable::sDeadList;
#endif

// static
void LLDrawable::incrementVisible()
{
	LLViewerOctreeEntryData::incrementVisible();
	sCurPixelAngle = (F32)gViewerWindowp->getWindowDisplayHeight() / gViewerCamera.getView();
}

LLDrawable::LLDrawable(LLViewerObject* vobj, bool new_entry)
:	LLViewerOctreeEntryData(LLViewerOctreeEntry::LLDRAWABLE),
	mVObjp(vobj)
{
	init(new_entry);
}

void LLDrawable::init(bool new_entry)
{
	// mXform
	mParent = NULL;
	mRenderType = 0;
	mCurrentScale = LLVector3(1.f, 1.f, 1.f);
	mDistanceWRTCamera = 0.f;
	mState = 0;

	// mFaces
	mRadius = 0.f;
	mGeneration = -1;
	mSpatialBridge = NULL;

	LLViewerOctreeEntry* entry = NULL;
	LLVOCacheEntry* vo_entry = NULL;
	LLViewerRegion* region = getRegion();
	if (!new_entry && mVObjp && region)
	{
		vo_entry = region->getCacheEntryForOctree(mVObjp->getLocalID());
		if (vo_entry)
		{
			entry = vo_entry->getEntry();
		}
	}
	setOctreeEntry(entry);
	if (vo_entry)
	{
		if (!entry)
		{
			vo_entry->setOctreeEntry(mEntry);
		}

		if (region)
		{
			region->addActiveCacheEntry(vo_entry);

			if (vo_entry->getNumOfChildren() > 0)
			{
				// to load all children.
				region->addVisibleChildCacheEntry(vo_entry, NULL);
			}
		}

		llassert(!vo_entry->getGroup()); // not in the object cache octree.
	}

	llassert(!vo_entry || vo_entry->getEntry() == mEntry);

	// invisible for the current frame and the last frame.
	initVisible(sCurVisible - 2);
}

//virtual
LLDrawable::~LLDrawable()
{
#if LL_DEBUG
	if (gDebugGL)
	{
		gPipeline.checkReferences(this);
	}
#endif

	if (LLSpatialGroup::sNoDelete)
	{
		llwarns << "Illegal deletion of LLDrawable !" << llendl;
		llassert(false);
	}

	if (isDead())
	{
		--sNumZombieDrawables;
	}

	if (!mFaces.empty())
	{
		std::for_each(mFaces.begin(), mFaces.end(), DeletePointer());
		mFaces.clear();
	}

#if LL_DEBUG
	if (!(sNumZombieDrawables % 10))
	{
		llinfos << "Zombie drawables: " << sNumZombieDrawables << llendl;
	}
#endif
}

void LLDrawable::markDead()
{
	if (isDead())
	{
		llwarns << "Marking dead multiple times !" << llendl;
		return;
	}

	setState(DEAD);

	if (mSpatialBridge)
	{
		mSpatialBridge->markDead();
		mSpatialBridge = NULL;
	}

	++sNumZombieDrawables;

	// We are dead. Free up all of our references to other objects.
	cleanupReferences();
#if LL_DEBUG
	sDeadList.push_back(this);
#endif
}

LLVOVolume* LLDrawable::getVOVolume() const
{
	LLViewerObject* objectp = mVObjp.get();
	if (!isDead() && objectp && objectp->getPCode() == LL_PCODE_VOLUME)
	{
		return (LLVOVolume*)objectp;
	}
	return NULL;
}

const LLMatrix4& LLDrawable::getRenderMatrix() const
{
	return isRoot() ? getWorldMatrix() : getParent()->getWorldMatrix();
}

bool LLDrawable::isLight() const
{
	LLViewerObject* objectp = mVObjp.get();
	if (objectp && objectp->getPCode() == LL_PCODE_VOLUME && !isDead())
	{
		return ((LLVOVolume*)objectp)->getIsLight();
	}
	return false;
}

void LLDrawable::cleanupReferences()
{
	LL_FAST_TIMER(FTM_CLEANUP_DRAWABLE);

	std::for_each(mFaces.begin(), mFaces.end(), DeletePointer());
	mFaces.clear();

	gPipeline.unlinkDrawable(this);
	removeFromOctree();

	// Cleanup references to other objects
	mVObjp = NULL;
	mParent = NULL;
}

void LLDrawable::removeFromOctree()
{
	if (!mEntry)
	{
		return;
	}
	mEntry->removeData(this);
	if (mEntry->hasVOCacheEntry())
	{
		LLViewerRegion* regionp = getRegion();
		if (regionp)
		{
			regionp->removeActiveCacheEntry((LLVOCacheEntry*)mEntry->getVOCacheEntry(),
											this);
		}
	}
	mEntry = NULL;
}

#if LL_DEBUG && 0
void LLDrawable::cleanupDeadDrawables()
{
	for (S32 i = 0, count = sDeadList.size(); i < count; ++i)
	{
		if (sDeadList[i]->getNumRefs() > 1)
		{
			llwarns << "Dead drawable has " << sDeadList[i]->getNumRefs()
					<< " remaining refs" << llendl;
			gPipeline.findReferences(sDeadList[i]);
		}
	}
	sDeadList.clear();
}

S32 LLDrawable::findReferences(LLDrawable* drawablep)
{
	if (mParent == drawablep)
	{
		llinfos << this << ": parent reference" << llendl;
		return 1;
	}
	return 0;
}
#endif

LLFace* LLDrawable::getFace(S32 i) const
{
	if ((U32)i >= mFaces.size())
	{
		llwarns << "Invalid face index: " << i << " for a number of: "
				<< mFaces.size() << " faces." << llendl;
		return NULL;
	}

	if (!mFaces[i])
	{
		llwarns << "Null face found." << llendl;
		return NULL;
	}

	return mFaces[i];
}

LLFace* LLDrawable::addFace(LLFacePool* poolp, LLViewerTexture* texturep)
{
	LL_TRACY_TIMER(TRC_ALLOCATE_FACE);

	LLFace* face = new LLFace(this, mVObjp);
	if (!face)
	{
		llerrs << "Allocating new face: " << mFaces.size() << llendl;
	}

	mFaces.push_back(face);

	if (poolp)
	{
		face->setPool(poolp, texturep);
	}

	if (isState(UNLIT))
	{
		face->setState(LLFace::FULLBRIGHT);
	}

	return face;
}

LLFace* LLDrawable::addFace(const LLTextureEntry* te,
							LLViewerTexture* texturep)
{
	LL_TRACY_TIMER(TRC_ALLOCATE_FACE);

	LLFace* face = new LLFace(this, mVObjp);
	if (!face)
	{
		llerrs << "Allocating new face: " << mFaces.size() << llendl;
	}

	face->setTEOffset(mFaces.size());
	face->setDiffuseMap(texturep);
	face->setPoolType(gPipeline.getPoolTypeFromTE(te, texturep));

	mFaces.push_back(face);

	if (isState(UNLIT))
	{
		face->setState(LLFace::FULLBRIGHT);
	}

	return face;

}

LLFace* LLDrawable::addFace(const LLTextureEntry* te,
							LLViewerTexture* texturep,
							LLViewerTexture* normalp)
{
	LL_TRACY_TIMER(TRC_ALLOCATE_FACE);

	LLFace* face = new LLFace(this, mVObjp);
	if (!face)
	{
		llerrs << "Allocating new face: " << mFaces.size() << llendl;
	}

	face->setTEOffset(mFaces.size());
	face->setDiffuseMap(texturep);
	face->setNormalMap(normalp);
	face->setPoolType(gPipeline.getPoolTypeFromTE(te, texturep));

	mFaces.push_back(face);

	if (isState(UNLIT))
	{
		face->setState(LLFace::FULLBRIGHT);
	}

	return face;
}

LLFace* LLDrawable::addFace(const LLTextureEntry* te,
							LLViewerTexture* texturep,
							LLViewerTexture* normalp,
							LLViewerTexture* specularp)
{
	LL_TRACY_TIMER(TRC_ALLOCATE_FACE);

	LLFace* face = new LLFace(this, mVObjp);
	if (!face)
	{
		llerrs << "Allocating new face: " << mFaces.size() << llendl;
	}

	face->setTEOffset(mFaces.size());
	face->setDiffuseMap(texturep);
	face->setNormalMap(normalp);
	face->setSpecularMap(specularp);
	face->setPoolType(gPipeline.getPoolTypeFromTE(te, texturep));

	mFaces.push_back(face);

	if (isState(UNLIT))
	{
		face->setState(LLFace::FULLBRIGHT);
	}

	return face;
}

void LLDrawable::setNumFaces(S32 new_faces, LLFacePool* poolp,
							 LLViewerTexture* texturep)
{
	S32 cur_faces = mFaces.size();
	if (new_faces == cur_faces)
	{
		return;
	}
	if (new_faces < cur_faces)
	{
		std::for_each(mFaces.begin() + new_faces, mFaces.end(),
					  DeletePointer());
		mFaces.erase(mFaces.begin() + new_faces, mFaces.end());
	}
	else // (new_faces > cur_faces)
	{
		mFaces.reserve(new_faces);
		for (S32 i = mFaces.size(); i < new_faces; ++i)
		{
			addFace(poolp, texturep);
		}
	}

	llassert_always((S32)mFaces.size() == new_faces);
}

void LLDrawable::setNumFacesFast(S32 new_faces, LLFacePool* poolp,
								 LLViewerTexture* texturep)
{
	S32 cur_faces = mFaces.size();
	if (new_faces <= cur_faces && new_faces >= cur_faces / 2)
	{
		return;
	}
	if (new_faces < cur_faces)
	{
		std::for_each(mFaces.begin() + new_faces, mFaces.end(),
					  DeletePointer());
		mFaces.erase(mFaces.begin() + new_faces, mFaces.end());
	}
	else // (new_faces > mFaces.size())
	{
		mFaces.reserve(new_faces);
		for (S32 i = mFaces.size(); i < new_faces; ++i)
		{
			addFace(poolp, texturep);
		}
	}

	llassert_always((S32)mFaces.size() == new_faces);
}

void LLDrawable::mergeFaces(LLDrawable* src)
{
	U32 face_count = mFaces.size() + src->mFaces.size();

	mFaces.reserve(face_count);
	for (U32 i = 0, count = src->mFaces.size(); i < count; ++i)
	{
		LLFace* facep = src->mFaces[i];
		facep->setDrawable(this);
		mFaces.push_back(facep);
	}
	src->mFaces.clear();
}

void LLDrawable::deleteFaces(S32 offset, S32 count)
{
	face_list_t::iterator face_begin = mFaces.begin() + offset;
	face_list_t::iterator face_end = face_begin + count;

	std::for_each(face_begin, face_end, DeletePointer());
	mFaces.erase(face_begin, face_end);
}

LLDrawable* LLDrawable::getRoot()
{
	LLDrawable* drawablep = this;
	while (!drawablep->isRoot())
	{
		drawablep = drawablep->getParent();
	}
	return drawablep;
}

void LLDrawable::update()
{
	llerrs << "This should not be called !" << llendl;
}

void LLDrawable::makeActive()
{
#if LL_DEBUG
	if (mVObjp.notNull())
	{
		U32 pcode = mVObjp->getPCode();
		if (pcode == LLViewerObject::LL_VO_WATER ||
			pcode == LLViewerObject::LL_VO_VOID_WATER ||
			pcode == LLViewerObject::LL_VO_SURFACE_PATCH ||
			pcode == LLViewerObject::LL_VO_PART_GROUP ||
			pcode == LLViewerObject::LL_VO_HUD_PART_GROUP ||
			pcode == LLViewerObject::LL_VO_CLOUDS ||
			pcode == LLViewerObject::LL_VO_SKY)
		{
			llerrs << "Static viewer object has active drawable !" << llendl;
		}
	}
#endif

	if (!isState(ACTIVE)) // && mGeneration > 0)
	{
		setState(ACTIVE);

		// Parent must be made active first
		if (!isRoot() && !mParent->isActive())
		{
			mParent->makeActive();
			// NOTE: linked set will now NEVER become static
			mParent->setState(LLDrawable::ACTIVE_CHILD);
		}

		// All child objects must also be active
		llassert_always(mVObjp);

		LLViewerObject::const_child_list_t& child_list = mVObjp->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter = child_list.begin(), end = child_list.end();
			 iter != end; ++iter)
		{
			LLViewerObject* child = *iter;
			LLDrawable* drawable = child->mDrawable;
			if (drawable)
			{
				drawable->makeActive();
			}
		}

		if (mVObjp->getPCode() == LL_PCODE_VOLUME)
		{
			gPipeline.markRebuild(this, LLDrawable::REBUILD_VOLUME);
		}
		updatePartition();
	}
	// This should not happen, but occasionally it does...
	else if (!isRoot() && !mParent->isActive())
	{
		mParent->makeActive();
		// NOTE: linked set will now NEVER become static
		mParent->setState(LLDrawable::ACTIVE_CHILD);
	}

	if (!isAvatar() && !isRoot() && !mParent->isActive())
	{
		llwarns << "failed !" << llendl;
	}
}

void LLDrawable::makeStatic(bool warning_enabled)
{
	if (isState(ACTIVE) && !isState(ACTIVE_CHILD) && mVObjp.notNull() &&
		!mVObjp->isAttachment() && !mVObjp->isFlexible() &&
		!mVObjp->isAnimatedObject())
	{
		clearState(ACTIVE | ANIMATED_CHILD);

		if (mParent.notNull() && mParent->isActive() && warning_enabled)
		{
			llwarns_sparse << "Drawable becomes static with active parent !"
						   << llendl;
		}

		LLViewerObject::const_child_list_t& child_list = mVObjp->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter = child_list.begin(), end = child_list.end();
			 iter != end; ++iter)
		{
			LLViewerObject* child = *iter;
			LLDrawable* child_drawable = child->mDrawable;
			if (child_drawable)
			{
				if (child_drawable->getParent() != this)
				{
					llwarns << "Child drawable has unknown parent." << llendl;
				}
				child_drawable->makeStatic(warning_enabled);
			}
		}

		if (mVObjp->getPCode() == LL_PCODE_VOLUME)
		{
			gPipeline.markRebuild(this, LLDrawable::REBUILD_VOLUME);
		}

		if (mSpatialBridge)
		{
			mSpatialBridge->markDead();
			setSpatialBridge(NULL);
		}
		updatePartition();
	}

	llassert(isAvatar() || isRoot() || mParent->isStatic());
}

// Returns "distance" between target destination and resulting xfrom
F32 LLDrawable::updateXform(bool undamped)
{
	bool damped = !undamped;

	// Position
	LLVector3 old_pos(mXform.getPosition());
	LLVector3 target_pos;
	if (mXform.isRoot())
	{
		// get root position in your agent's region
		target_pos = mVObjp->getPositionAgent();
	}
	else
	{
		// parent-relative position
		target_pos = mVObjp->getPosition();
	}

	// Rotation
	LLQuaternion old_rot = mXform.getRotation();
	LLQuaternion target_rot = mVObjp->getRotation();
	bool no_target_omega = mVObjp->getAngularVelocity().isExactlyZero();

	// Scaling
	LLVector3 target_scale = mVObjp->getScale();
	LLVector3 old_scale = mCurrentScale;

	// Damping
	F32 dist_squared = 0.f;

	if (damped && isVisible())
	{
		F32 lerp_amt =
			llclamp(LLCriticalDamp::getInterpolant(OBJECT_DAMPING_TIME_CONSTANT),
												   0.f, 1.f);
		LLVector3 new_pos = lerp(old_pos, target_pos, lerp_amt);
		dist_squared = dist_vec_squared(new_pos, target_pos);

		LLQuaternion new_rot = nlerp(lerp_amt, old_rot, target_rot);
		dist_squared += (1.f - dot(new_rot, target_rot)) * 10.f;

		LLVector3 new_scale = lerp(old_scale, target_scale, lerp_amt);
		dist_squared += dist_vec_squared(new_scale, target_scale);
		if (dist_squared <= MAX_INTERPOLATE_DISTANCE_SQUARED &&
			dist_squared >= MIN_INTERPOLATE_DISTANCE_SQUARED *
							mDistanceWRTCamera * mDistanceWRTCamera)
		{
			// Interpolate
			target_pos = new_pos;
			target_rot = new_rot;
			target_scale = new_scale;
		}
		else if (no_target_omega)
		{
			// Snap to final position (only if no target omega is applied)
			dist_squared = 0.0f;
			if (getVOVolume() && !isRoot())
			{
				// Child prim snapping to some position, needs a rebuild
				gPipeline.markRebuild(this, LLDrawable::REBUILD_POSITION);
			}
		}
	}

	bool is_root = isRoot();

	if (old_scale != target_scale)
	{
		// Scale change requires immediate rebuild
		mCurrentScale = target_scale;
		gPipeline.markRebuild(this, LLDrawable::REBUILD_POSITION);
	}
	else if (!is_root && (dist_squared > 0.f || !no_target_omega))
	{
		// Child prim moving relative to parent, tag as needing to be rendered
		// atomically and rebuild
		dist_squared = 1.f; // keep this object on the move list
		if (!isState(LLDrawable::ANIMATED_CHILD))
		{
			setState(LLDrawable::ANIMATED_CHILD);
			gPipeline.markRebuild(this);
			mVObjp->dirtySpatialGroup();
		}
	}
	else if (!is_root && (old_pos != target_pos || target_rot != old_rot))
	{
		mVObjp->shrinkWrap();
		gPipeline.markRebuild(this);
	}
	else if (!getVOVolume() && !isAvatar())
	{
		movePartition();
	}

	// Update
	mXform.setPosition(target_pos);
	mXform.setRotation(target_rot);
	// No scale in drawable transforms: IT IS A RULE !
	mXform.setScale(LLVector3(1.f, 1.f, 1.f));
	mXform.updateMatrix();

	if (is_root && mVObjp->isAnimatedObject())
	{
		LLVOAvatarPuppet* puppet = mVObjp->getPuppetAvatar();
		if (puppet)
		{
			puppet->matchVolumeTransform();
		}
	}

	if (mSpatialBridge)
	{
		gPipeline.markMoved(mSpatialBridge, false);
	}

	return dist_squared;
}

void LLDrawable::moveUpdatePipeline(bool moved)
{
	if (moved)
	{
		makeActive();
	}

	// Update the face centers.
	for (S32 i = 0, count = getNumFaces(); i < count; ++i)
	{
		LLFace* face = getFace(i);
		if (face)
		{
			face->updateCenterAgent();
		}
	}
}

void LLDrawable::movePartition()
{
	LLSpatialPartition* part = getSpatialPartition();
	if (part)
	{
		part->move(this, getSpatialGroup());
	}
}

bool LLDrawable::updateMove()
{
	if (isDead())
	{
		llwarns << "Update move on dead drawable !" << llendl;
		return true;
	}

	if (mVObjp.isNull())
	{
		return false;
	}

	makeActive();

	return isState(MOVE_UNDAMPED) ? updateMoveUndamped() : updateMoveDamped();
}

bool LLDrawable::updateMoveUndamped()
{
	F32 dist_squared = updateXform(true);

	++mGeneration;

	if (!isState(LLDrawable::INVISIBLE))
	{
		bool moved = dist_squared > 0.001f && dist_squared < 255.99f;
		moveUpdatePipeline(moved);
		mVObjp->updateText();
	}

	mVObjp->clearChanged(LLXform::MOVED);

	return true;
}

void LLDrawable::updatePartition()
{
	if (!getVOVolume())
	{
		movePartition();
	}
	else if (mSpatialBridge)
	{
		gPipeline.markMoved(mSpatialBridge, false);
	}
	else
	{
		// A child prim moved and needs its verts regenerated
		gPipeline.markRebuild(this, LLDrawable::REBUILD_POSITION);
	}
}

bool LLDrawable::updateMoveDamped()
{
	F32 dist_squared = updateXform(false);

	++mGeneration;

	if (!isState(LLDrawable::INVISIBLE))
	{
		bool moved = dist_squared > 0.001f && dist_squared < 128.0f;
		moveUpdatePipeline(moved);
		mVObjp->updateText();
	}

	bool done_moving = dist_squared == 0.0f;
	if (done_moving)
	{
		mVObjp->clearChanged(LLXform::MOVED);
	}

	return done_moving;
}

void LLDrawable::updateDistance(LLCamera& camera, bool force_update)
{
	if (LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
	{
		llwarns << "Attempted to update distance for non-world camera."
				<< llendl;
		return;
	}

	if (gShiftFrame)
	{
		return;
	}

#if 0
	// Switch LOD with the spatial group to avoid artifacts
	LLSpatialGroup* sg = getSpatialGroup();
	if (sg && !sg->changeLOD())
	{
		return;
	}
#endif

	LLVector3 pos;

	LLVOVolume* volume = getVOVolume();
	if (volume)
	{
		if (getGroup())
		{
			pos.set(getPositionGroup().getF32ptr());
		}
		else
		{
			pos = getPositionAgent();
		}

		if (isState(LLDrawable::HAS_ALPHA))
		{
			LLVector4a box;
			LLVector3 v;
			for (S32 i = 0, count = getNumFaces(); i < count; ++i)
			{
				LLFace* facep = getFace(i);
				if (facep && (force_update || facep->isInAlphaPool()))
				{
					box.setSub(facep->mExtents[1], facep->mExtents[0]);
					box.mul(0.25f);
					v = facep->mCenterLocal - camera.getOrigin();
					const LLVector3& at = camera.getAtAxis();
					for (U32 j = 0; j < 3; ++j)
					{
						v.mV[j] -= box[j] * at.mV[j];
					}
					facep->mDistance = v * camera.getAtAxis();
				}
			}
		}
		// Handle volumes in an animated object as a special case
		LLVOAvatar* av = volume->getAvatar();
		LLViewerRegion* region = volume->getRegion();
#if 0	// SL-937: add dynamic box handling for rigged mesh on regular avs
		if (av && av->isPuppetAvatar() && region)
#else
		if (av && region)
#endif
		{
			const LLVector3* av_box = av->getLastAnimExtents();
			LLVector3 cam_offset =
				LLVector3::pointToBoxOffset(gViewerCamera.getOrigin(), av_box);
			mDistanceWRTCamera = llmax(ll_round(cam_offset.length(), 0.01f),
									   0.01f);
			mVObjp->updateLOD();
			return;
		}
	}
	else if (getGroup())
	{
		pos = LLVector3(getPositionGroup().getF32ptr());
	}

	pos -= camera.getOrigin();
	mDistanceWRTCamera = ll_round(pos.length(), 0.01f);
	mVObjp->updateLOD();
}

void LLDrawable::updateTexture()
{
	if (isDead())
	{
		llwarns << "Dead drawable updating texture!" << llendl;
		return;
	}

	if (getNumFaces() != mVObjp->getNumTEs())
	{
		// Drawable is transitioning its face count
		return;
	}

	if (getVOVolume())
	{
		gPipeline.markRebuild(this, LLDrawable::REBUILD_MATERIAL);
	}
}

bool LLDrawable::updateGeometry()
{
	return mVObjp.notNull() && mVObjp->updateGeometry(this);
}

void LLDrawable::shiftPos(const LLVector4a& shift_vector)
{
	if (isDead() || mVObjp.isNull())
	{
		llwarns << "Shifting dead drawable" << llendl;
		return;
	}

	if (mParent)
	{
		mXform.setPosition(mVObjp->getPosition());
	}
	else
	{
		mXform.setPosition(mVObjp->getPositionAgent());
	}

	mXform.updateMatrix();

	if (isStatic())
	{
		LLVOVolume* volume = getVOVolume();
		bool rebuild = (!volume &&
						mRenderType != LLPipeline::RENDER_TYPE_TREE &&
						mRenderType != LLPipeline::RENDER_TYPE_TERRAIN &&
						mRenderType != LLPipeline::RENDER_TYPE_SKY);
		if (rebuild)
		{
			gPipeline.markRebuild(this);
		}

		for (S32 i = 0, count = getNumFaces(); i < count; ++i)
		{
			LLFace* facep = getFace(i);
			if (facep)
			{
				facep->mCenterAgent += LLVector3(shift_vector.getF32ptr());
				facep->mExtents[0].add(shift_vector);
				facep->mExtents[1].add(shift_vector);

				if (rebuild && facep->hasGeometry())
				{
					facep->clearVertexBuffer();
				}
			}
		}

		shift(shift_vector);
	}
	else if (mSpatialBridge)
	{
		mSpatialBridge->shiftPos(shift_vector);
	}
	else if (isAvatar())
	{
		shift(shift_vector);
	}

	mVObjp->onShift(shift_vector);
}

const LLVector3& LLDrawable::getBounds(LLVector3& min, LLVector3& max) const
{
	mXform.getMinMax(min,max);
	return mXform.getPositionW();
}

void LLDrawable::updateSpatialExtents()
{
	if (mVObjp.notNull())
	{
		const LLVector4a* exts = getSpatialExtents();
		LLVector4a extents[2] = { exts[0], exts[1] };
		mVObjp->updateSpatialExtents(extents[0], extents[1]);
		setSpatialExtents(extents[0], extents[1]);
	}

	updateBinRadius();

	if (mSpatialBridge.notNull())
	{
		getGroupPosition().splat(0.f);
	}
}

void LLDrawable::updateBinRadius()
{
	if (mVObjp.notNull())
	{
		setBinRadius(llmin(mVObjp->getBinRadius(), 256.f));
	}
	else
	{
		setBinRadius(llmin(getRadius() * 4.f, 256.f));
	}
}

F32 LLDrawable::getVisibilityRadius() const
{
	if (isDead())
	{
		return 0.f;
	}
	else if (isLight())
	{
		const LLVOVolume* vov = getVOVolume();
		if (vov)
		{
			return llmax(getRadius(), vov->getLightRadius());
		}
		//else  llwarns ?
	}
	return getRadius();
}

bool LLDrawable::isVisible() const
{
	if (LLViewerOctreeEntryData::isVisible())
	{
		return true;
	}

	LLViewerOctreeGroup* group = mEntry->getGroup();
#if 1
	LLSpatialGroup* sgroup = getSpatialGroup();
	if ((group && group->isVisible()) || (sgroup && sgroup->isHUDGroup()))
#else
	if (group && group->isVisible())
#endif
	{
		LLViewerOctreeEntryData::setVisible();
		return true;
	}

	return false;
}

bool LLDrawable::isRecentlyVisible() const
{
	// currently visible or visible in the previous frame.
	bool vis = LLViewerOctreeEntryData::isRecentlyVisible();
	if (!vis)
	{
		// two frames:the current one and the last one.
		constexpr U32 MIN_VIS_FRAME_RANGE = 2;
		vis = sCurVisible - getVisible() < MIN_VIS_FRAME_RANGE;
	}

	return vis;
}

void LLDrawable::setGroup(LLViewerOctreeGroup* groupp)
{
	LLSpatialGroup* cur_groupp = (LLSpatialGroup*)getGroup();

#if 0
	// Precondition: mGroupp MUST be null or DEAD or mGroupp MUST NOT contain
	// this
	llassert(!cur_groupp || cur_groupp->isDead() ||
			 !cur_groupp->hasElement(this));
#endif

	// Precondition: groupp MUST be null or groupp MUST contain this
	llassert(!groupp || (LLSpatialGroup*)groupp->hasElement(this));

	if (cur_groupp != groupp && getVOVolume())
	{
		// NULL out vertex buffer references for volumes on spatial group
		// change to maintain requirement that every face vertex buffer is
		// either NULL or points to a vertex buffer contained by its drawable's
		// spatial group
		for (S32 i = 0, count = getNumFaces(); i < count; ++i)
		{
			LLFace* facep = getFace(i);
			if (facep)
			{
				facep->clearVertexBuffer();
			}
		}
	}

#if 0
	// Postcondition: if next group is NULL, previous group must be dead OR
	// NULL OR binIndex must be -1. If next group is NOT NULL, binIndex must
	// not be -1
	llassert(groupp == NULL ?
				cur_groupp == NULL || cur_groupp->isDead() || !getEntry() ||
				getEntry()->getBinIndex() == -1
							:
				getEntry() && getEntry()->getBinIndex() != -1);
#endif

	LLViewerOctreeEntryData::setGroup(groupp);
}

LLSpatialPartition* LLDrawable::getSpatialPartition()
{
	LLSpatialPartition* retval = NULL;

	if (!mVObjp || !getVOVolume() || isStatic())
	{
		retval = gPipeline.getSpatialPartition((LLViewerObject*)mVObjp);
	}
	else if (isRoot())
	{
		if (mSpatialBridge.notNull())
		{
			bool obsolete = false;
			U32 type = mSpatialBridge->asPartition()->mPartitionType;
			bool is_hud = mVObjp->isHUDAttachment();
			// Was/became a HUD attachment ?
			if ((type == LLViewerRegion::PARTITION_HUD) != is_hud)
			{
				obsolete = true;
			}
			else
			{
				bool is_animesh = mVObjp->isAnimatedObject() &&
								  mVObjp->getPuppetAvatar() != NULL;
				// Was/became an animesh ?
				if ((type == LLViewerRegion::PARTITION_PUPPET) != is_animesh)
				{
					obsolete = true;
				}
				// Was/became another kind of avatar attachment ?
				else if ((type == LLViewerRegion::PARTITION_AVATAR) !=
							(!is_hud && !is_animesh && mVObjp->isAttachment()))
				{
					obsolete = true;				
				}
			}
			if (obsolete)
			{
				// Remove obsolete bridge
				mSpatialBridge->markDead();
				setSpatialBridge(NULL);
			}
		}
		// Must be an active volume
		if (mSpatialBridge.isNull())
		{
			// The order is important here, since HUDs and puppets are or
			// can be attachments...
			if (mVObjp->isHUDAttachment())
			{
				setSpatialBridge(new LLHUDBridge(this, getRegion()));
			}
			else if (mVObjp->isAnimatedObject() && mVObjp->getPuppetAvatar())
			{
				setSpatialBridge(new LLPuppetBridge(this, getRegion()));
			}
			else if (mVObjp->isAttachment())
			{
				setSpatialBridge(new LLAvatarBridge(this, getRegion()));
			}
			else
			{
				setSpatialBridge(new LLVolumeBridge(this, getRegion()));
			}
		}

		return mSpatialBridge->asPartition();
	}
	else
	{
		retval = getParent()->getSpatialPartition();
	}

	if (retval && mSpatialBridge.notNull())
	{
		mSpatialBridge->markDead();
		setSpatialBridge(NULL);
	}

	return retval;
}

void LLDrawable::setVisible(LLCamera& camera,
							std::vector<LLDrawable*>* results,
							bool for_select)
{
	LLViewerOctreeEntryData::setVisible();

#if 0
	// Force visibility on all children as well
	LLViewerObject::const_child_list_t& child_list = mVObjp->getChildren();
	for (LLViewerObject::child_list_t::const_iterator
			iter = child_list.begin(), end = child_list.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		LLDrawable* child_drawable = child->mDrawable;
		if (child_drawable)
		{
			child_drawable->LLViewerOctreeEntryData::setVisible();
		}
	}
#endif

#if 0 && LL_DEBUG
	// crazy paranoid rules checking
	if (getVOVolume())
	{
		if (!isRoot())
		{
			if (isActive() && !mParent->isActive())
			{
				llerrs << "Active drawable has static parent !" << llendl;
			}

			if (isStatic() && !mParent->isStatic())
			{
				llerrs << "Static drawable has active parent !" << llendl;
			}

			if (mSpatialBridge)
			{
				llerrs << "Child drawable has spatial bridge !" << llendl;
			}
		}
		else if (isActive() && !mSpatialBridge)
		{
			llerrs << "Active root drawable has no spatial bridge !" << llendl;
		}
		else if (isStatic() && mSpatialBridge.notNull())
		{
			llerrs << "Static drawable has spatial bridge !" << llendl;
		}
	}
#endif
}


const LLVector3	LLDrawable::getPositionAgent() const
{
	if (getVOVolume())
	{
		if (isActive())
		{
			LLVector3 pos;
			if (!isRoot())
			{
				pos = mVObjp->getPosition();
			}
			return pos * getRenderMatrix();
		}
		return mVObjp->getPositionAgent();
	}
	return getWorldPosition();
}

bool LLDrawable::isAnimating() const
{
	if (!getVObj())
	{
		return true;
	}

	if (getScale() != mVObjp->getScale())
	{
		return true;
	}

	U32 pcode = mVObjp->getPCode();
	if (pcode == LLViewerObject::LL_VO_PART_GROUP)
	{
		return true;
	}
	if (pcode == LLViewerObject::LL_VO_HUD_PART_GROUP)
	{
		return true;
	}
	if (pcode == LLViewerObject::LL_VO_CLOUDS)
	{
		return true;
	}

#if 0
	if (!isRoot() && !mVObjp->getAngularVelocity().isExactlyZero())
	{
		// Target omega
		return true;
	}
#endif

	return false;
}

//virtual
void LLDrawable::updateFaceSize(S32 idx)
{
	if (mVObjp.notNull())
	{
		mVObjp->updateFaceSize(idx);
	}
}
