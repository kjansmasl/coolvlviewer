/**
 * @file llviewerpartsim.cpp
 * @brief LLViewerPart class implementation
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

#include <utility>

#include "llviewerpartsim.h"

#include "llfasttimer.h"

#include "llagent.h"
#include "llappviewer.h"			// gFPSClamped
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerpartsource.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvopartgroup.h"
#include "llvovolume.h"
#include "llworld.h"

constexpr F32 PART_SIM_BOX_SIDE = 16.f;

// Global
LLViewerPartSim gViewerPartSim;

//static
U32 LLViewerPart::sNextPartID = 1;
S32 LLViewerPartSim::sMaxParticleCount = 0;
S32 LLViewerPartSim::sParticleCount = 0;
#if LL_DEBUG
S32 LLViewerPartSim::sParticleCount2 = 0;
#endif
// This controls how greedy individual particle burst sources are allowed to
// be, and adapts according to how near the particle-count limit we are.
F32 LLViewerPartSim::sParticleAdaptiveRate = 0.0625f;
F32 LLViewerPartSim::sParticleBurstRate = 0.5f;

F32 calc_desired_size(LLVector3 pos, LLVector2 scale)
{
	F32 desired_size = (pos - gViewerCamera.getOrigin()).length() * 0.25f;
	return llclamp(desired_size, scale.length() * 0.5f,
				   PART_SIM_BOX_SIDE * 2.f);
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerPart class
///////////////////////////////////////////////////////////////////////////////

LLViewerPart::LLViewerPart()
:	mPartID(0),
	mLastUpdateTime(0.f),
	mSkipOffset(0.f),
	mVPCallback(NULL),
	mParent(NULL),
	mChild(NULL)
{
#if LL_DEBUG
	++LLViewerPartSim::sParticleCount2;
#endif
}

LLViewerPart::~LLViewerPart()
{
	if (mPartSourcep.notNull() && mPartSourcep->mLastPart == this)
	{
		mPartSourcep->mLastPart = NULL;
	}

	// Patch up holes in the ribbon
	if (mParent)
	{
		llassert(mParent->mChild == this);
		mParent->mChild = mChild;
	}

	if (mChild)
	{
		llassert (mChild->mParent == this);
		mChild->mParent = mParent;
	}

	mPartSourcep = NULL;
#if LL_DEBUG
	--LLViewerPartSim::sParticleCount2;
#endif
}

void LLViewerPart::init(LLPointer<LLViewerPartSource> sourcep,
						LLViewerTexture* imagep, LLVPCallback cb)
{
	mPartID = sNextPartID++;
	mFlags = 0x00f;
	mLastUpdateTime = mSkipOffset = 0.f;
	mMaxAge = 10.f;
	mVPCallback = cb;
	mPartSourcep = std::move(sourcep);
	mImagep = imagep;
	if (mImagep.notNull())
	{
		mImagep->setBoostLevel(LLGLTexture::BOOST_SUPER_HIGH);
		// Do not allow to discard the texture: fast changing particle systems
		// often see their cycling textures de-rez if we do. *TODO: only apply
		// to fast changing particle systems ? HB
		LLViewerFetchedTexture* texp = mImagep->asFetched();
		if (texp)
		{
			texp->setMinDiscardLevel(1);
		}
		mImagep->dontDiscard();
#if !LL_IMPLICIT_SETNODELETE
		// Also set NO_DELETE since the changing textures might otherwise get
		// removed from memory. HB
		mImagep->setNoDelete();
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerPartGroup class
///////////////////////////////////////////////////////////////////////////////

LLViewerPartGroup::LLViewerPartGroup(const LLVector3& center_agent,
									 F32 box_side, bool hud)
:	mVOPartGroupp(NULL),
	mHud(hud),
	mUniformParticles(true),
	mCenterAgent(center_agent),
	mBoxSide(box_side),
	mBoxRadius(F_SQRT3 * 0.5f * box_side),
	mSkippedTime(0.f)
{
	static U32 id_seed = 0;
	mID = ++id_seed;

	llassert_always(center_agent.isFinite());
	mRegionp = gWorld.getRegionFromPosAgent(center_agent);
	if (!mRegionp)
	{
		LL_DEBUGS("Particles") << "No region at position, using agent region"
							   << LL_ENDL;
		mRegionp = gAgent.getRegion();
	}

	if (mHud)
	{
		mVOPartGroupp =
			(LLVOPartGroup*)gObjectList.createObjectViewer(LLViewerObject::LL_VO_HUD_PART_GROUP,
														   getRegion());
	}
	else
	{
		mVOPartGroupp =
			(LLVOPartGroup*)gObjectList.createObjectViewer(LLViewerObject::LL_VO_PART_GROUP,
														   getRegion());
	}
	mVOPartGroupp->setViewerPartGroup(this);
	mVOPartGroupp->setPositionAgent(getCenterAgent());
	F32 scale = box_side * 0.5f;
	mVOPartGroupp->setScale(LLVector3(scale, scale, scale));

	gPipeline.createObject(mVOPartGroupp);

	LLSpatialGroup* group = mVOPartGroupp->mDrawable->getSpatialGroup();
	if (group)
	{
		LLVector3 center(group->getOctreeNode()->getCenter().getF32ptr());
		LLVector3 size(group->getOctreeNode()->getSize().getF32ptr());
		size += LLVector3(0.01f, 0.01f, 0.01f);
		mMinObjPos = center - size;
		mMaxObjPos = center + size;
	}
	else
	{
		// Not sure what else to set the obj bounds to when the drawable has no
		// spatial group.
		LLVector3 extents(mBoxRadius, mBoxRadius, mBoxRadius);
		mMinObjPos = center_agent - extents;
		mMaxObjPos = center_agent + extents;
	}
}

LLViewerPartGroup::~LLViewerPartGroup()
{
	cleanup();

	S32 count = (S32)mParticles.size();
	for (S32 i = 0; i < count; ++i)
	{
		delete mParticles[i];
	}
	mParticles.clear();

	LLViewerPartSim::decPartCount(count);
}

void LLViewerPartGroup::cleanup()
{
	if (mVOPartGroupp)
	{
		if (!mVOPartGroupp->isDead())
		{
			gObjectList.killObject(mVOPartGroupp);
		}
		mVOPartGroupp = NULL;
	}
}

bool LLViewerPartGroup::posInGroup(const LLVector3& pos, F32 desired_size)
{
	if (pos.mV[VX] < mMinObjPos.mV[VX] || pos.mV[VY] < mMinObjPos.mV[VY] ||
		pos.mV[VZ] < mMinObjPos.mV[VZ])
	{
		return false;
	}

	if (pos.mV[VX] > mMaxObjPos.mV[VX] || pos.mV[VY] > mMaxObjPos.mV[VY] ||
		pos.mV[VZ] > mMaxObjPos.mV[VZ])
	{
		return false;
	}

	if (desired_size > 0 &&
		(desired_size < mBoxRadius * 0.5f || desired_size > mBoxRadius * 2.f))
	{
		return false;
	}

	return true;
}

bool LLViewerPartGroup::addPart(LLViewerPart* part, F32 desired_size)
{
	if (!mHud && (part->mFlags & LLPartData::LL_PART_HUD))
	{
		return false;
	}

	bool uniform_part = part->mScale.mV[0] == part->mScale.mV[1] &&
						!(part->mFlags &
						  LLPartData::LL_PART_FOLLOW_VELOCITY_MASK);

	if (mUniformParticles != uniform_part ||
		!posInGroup(part->mPosAgent, desired_size))
	{
		return false;
	}

	gPipeline.markRebuild(mVOPartGroupp->mDrawable);

	mParticles.push_back(part);
	part->mSkipOffset = mSkippedTime;
	LLViewerPartSim::incPartCount(1);
	return true;
}

void LLViewerPartGroup::updateParticles(F32 lastdt)
{
	F32 dt;

	LLVector3 gravity(0.f, 0.f, GRAVITY);

#if LL_DEBUG
	LLViewerPartSim::checkParticleCount(mParticles.size());
#endif

	S32 end = (S32)mParticles.size();
	for (S32 i = 0, count = mParticles.size(); i < count; )
	{
		LLVector3 a;
		LLViewerPart* part = mParticles[i];

		dt = lastdt + mSkippedTime - part->mSkipOffset;
		part->mSkipOffset = 0.f;

		// Update current time
		const F32 cur_time = part->mLastUpdateTime + dt;
		const F32 frac = cur_time / part->mMaxAge;

		// "Drift" the object based on the source object
		if (part->mFlags & LLPartData::LL_PART_FOLLOW_SRC_MASK)
		{
			part->mPosAgent = part->mPartSourcep->mPosAgent;
			part->mPosAgent += part->mPosOffset;
		}

		// Do a custom callback if we have one...
		if (part->mVPCallback)
		{
			(*part->mVPCallback)(*part, dt);
		}

		if (part->mFlags & LLPartData::LL_PART_WIND_MASK)
		{
			part->mVelocity *= 1.f - 0.1f * dt;
			part->mVelocity += 0.1f * dt *
							   mRegionp->mWind.getVelocity(mRegionp->getPosRegionFromAgent(part->mPosAgent));
		}

		// Now do interpolation towards a target
		if (part->mFlags & LLPartData::LL_PART_TARGET_POS_MASK)
		{
			F32 remaining = part->mMaxAge - part->mLastUpdateTime;
			F32 step = dt / remaining;

			step = llclamp(step, 0.f, 0.1f);
			step *= 5.f;
			// we want a velocity that will result in reaching the target in the
			// Interpolate towards the target.
			LLVector3 delta_pos = part->mPartSourcep->mTargetPosAgent -
								  part->mPosAgent;

			delta_pos /= remaining;

			part->mVelocity *= 1.f - step;
			part->mVelocity += step * delta_pos;
		}

		if (part->mFlags & LLPartData::LL_PART_TARGET_LINEAR_MASK)
		{
			LLVector3 delta_pos = part->mPartSourcep->mTargetPosAgent -
								  part->mPartSourcep->mPosAgent;
			part->mPosAgent = part->mPartSourcep->mPosAgent;
			part->mPosAgent += frac * delta_pos;
			part->mVelocity = delta_pos;
		}
		else
		{
			// Do velocity interpolation
			part->mPosAgent += dt * part->mVelocity;
			part->mPosAgent += 0.5f * dt * dt * part->mAccel;
			part->mVelocity += part->mAccel * dt;
		}

		// Do a bounce test
		if (part->mFlags & LLPartData::LL_PART_BOUNCE_MASK)
		{
			// Need to do point vs. plane check...
			// For now, just check relative to object height...
			F32 dz = part->mPosAgent.mV[VZ] -
					 part->mPartSourcep->mPosAgent.mV[VZ];
			if (dz < 0)
			{
				part->mPosAgent.mV[VZ] += -2.f * dz;
				part->mVelocity.mV[VZ] *= -0.75f;
			}
		}

		// Reset the offset from the source position
		if (part->mFlags & LLPartData::LL_PART_FOLLOW_SRC_MASK)
		{
			part->mPosOffset = part->mPosAgent;
			part->mPosOffset -= part->mPartSourcep->mPosAgent;
		}

		// Do color interpolation
		if (part->mFlags & LLPartData::LL_PART_INTERP_COLOR_MASK)
		{
			part->mColor.set(part->mStartColor);
			// Note: LLColor4's v%k means multiply-alpha-only,
			//       LLColor4's v*k means multiply-rgb-only
			part->mColor *= 1.f - frac; // rgb*k
			part->mColor %= 1.f - frac; // alpha*k
			part->mColor += frac % (frac * part->mEndColor); // rgb,alpha
		}

		// Do scale interpolation
		if (part->mFlags & LLPartData::LL_PART_INTERP_SCALE_MASK)
		{
			part->mScale.set(part->mStartScale);
			part->mScale *= 1.f - frac;
			part->mScale += frac * part->mEndScale;
		}

		// Do glow interpolation
		part->mGlow.mV[3] = (U8)ll_roundp(lerp(part->mStartGlow,
											   part->mEndGlow, frac) * 255.f);

		// Set the last update time to now.
		part->mLastUpdateTime = cur_time;

		// Kill dead particles (either flagged dead, or too old)
		if (part->mLastUpdateTime > part->mMaxAge ||
			LLViewerPart::LL_PART_DEAD_MASK == part->mFlags)
		{
			delete part;
			if (i < --count)
			{
				mParticles[i] = mParticles.back();
			}
			mParticles.pop_back();
		}
		else
		{
			// Increment the active particles count for the source
			part->mPartSourcep->incPartCount();

			F32 desired_size = calc_desired_size(part->mPosAgent,
												 part->mScale);
			if (!posInGroup(part->mPosAgent, desired_size))
			{
				// Transfer particles between groups
				gViewerPartSim.put(part);
				if (i < --count)
				{
					mParticles[i] = mParticles.back();
				}
				mParticles.pop_back();
			}
			else
			{
				++i;
			}
		}
	}

	S32 removed = end - (S32)mParticles.size();
	if (removed > 0)
	{
		// We removed one or more particles, so flag this group for update
		if (mVOPartGroupp.notNull())
		{
			gPipeline.markRebuild(mVOPartGroupp->mDrawable);
		}
		LLViewerPartSim::decPartCount(removed);
	}

	// Kill the viewer object if this particle group is empty
	if (mParticles.empty())
	{
		gObjectList.killObject(mVOPartGroupp);
		mVOPartGroupp = NULL;
	}

#if LL_DEBUG
	LLViewerPartSim::checkParticleCount(mParticles.size());
#endif
}

void LLViewerPartGroup::shift(const LLVector3& offset)
{
	mCenterAgent += offset;
	mMinObjPos += offset;
	mMaxObjPos += offset;

	for (S32 i = 0, count = mParticles.size(); i < count; ++i)
	{
		mParticles[i]->mPosAgent += offset;
	}
}

void LLViewerPartGroup::removeParticlesByID(U32 source_id)
{
	for (S32 i = 0, count = mParticles.size(); i < count; ++i)
	{
		if (mParticles[i]->mPartSourcep->getID() == source_id)
		{
			mParticles[i]->mFlags = LLViewerPart::LL_PART_DEAD_MASK;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerPartSim class
///////////////////////////////////////////////////////////////////////////////

void LLViewerPartSim::initClass()
{
	setMaxPartCount(gSavedSettings.getS32("RenderMaxPartCount"));
}

void LLViewerPartSim::cleanupClass()
{
	// Kill all of the groups (and particles)
	llinfos << "Destroying all particle groups..." << llendl;
	for (S32 i = 0, count = mViewerPartGroups.size(); i < count; ++i)
	{
		delete mViewerPartGroups[i];
	}
	mViewerPartGroups.clear();

	// Kill all of the sources
	llinfos << "Destroying all particle sources..." << llendl;
	mViewerPartSources.clear();

	llinfos << "Particles destroyed." << llendl;
}

//static
void LLViewerPartSim::setMaxPartCount(S32 max)
{
	sMaxParticleCount = llclamp(max, 2, 8192);
}

#if LL_DEBUG
//static
void LLViewerPartSim::checkParticleCount(U32 size)
{
	if (sParticleCount2 != sParticleCount)
	{
		llerrs << "sParticleCount: " << sParticleCount
			   << " - sParticleCount2: " << sParticleCount2 << llendl;
	}

	if (size > (U32)sParticleCount2)
	{
		llerrs << "Current particle size: " << sParticleCount2
			   << " - array size: " << size << llendl;
	}
}
#endif

//static
bool LLViewerPartSim::shouldAddPart()
{
	if (sParticleCount >= MAX_PART_COUNT)
	{
		return false;
	}
	if (sParticleCount > PART_THROTTLE_THRESHOLD * sMaxParticleCount)
	{
		F32 frac = (F32)sParticleCount / (F32)sMaxParticleCount;
		frac -= PART_THROTTLE_THRESHOLD;
		frac *= PART_THROTTLE_RESCALE;
		if (ll_frand() < frac)
		{
			// Skip...
			return false;
		}
	}

	// Check frame rate, and do not add more if the viewer is really slow
	constexpr F32 MIN_FRAME_RATE_FOR_NEW_PARTICLES = 5.f;
	return gFPSClamped >= MIN_FRAME_RATE_FOR_NEW_PARTICLES;
}

void LLViewerPartSim::addPart(LLViewerPart* part)
{
	if (sParticleCount < MAX_PART_COUNT)
	{
		put(part);
	}
	else
	{
		// Delete the particle if we cannot add it in
		delete part;
		part = NULL;
	}
}

LLViewerPartGroup* LLViewerPartSim::put(LLViewerPart* part)
{
	constexpr F32 MAX_MAG = 1000000.f * 1000000.f;
	LLViewerPartGroup* return_group = NULL;
	if (part->mPosAgent.lengthSquared() > MAX_MAG ||
		!part->mPosAgent.isFinite())
	{
		LL_DEBUGS("Particles") << "Particle out of range !  Position: "
							   << part->mPosAgent << LL_ENDL;
	}
	else
	{
		F32 desired_size = calc_desired_size(part->mPosAgent, part->mScale);

		for (S32 i = 0, count = mViewerPartGroups.size(); i < count; ++i)
		{
			if (mViewerPartGroups[i]->addPart(part, desired_size))
			{
				// We found a spatial group that we fit into, add us and exit
				return_group = mViewerPartGroups[i];
				break;
			}
		}

		// Hmm, we did not fit in any of the existing spatial groups
		// Create a new one...
		if (!return_group)
		{
			llassert_always(part->mPosAgent.isFinite());
			LLViewerPartGroup* groupp =
				createViewerPartGroup(part->mPosAgent, desired_size,
									  part->mFlags & LLPartData::LL_PART_HUD);
			groupp->mUniformParticles =
				part->mScale.mV[0] == part->mScale.mV[1] &&
				!(part->mFlags & LLPartData::LL_PART_FOLLOW_VELOCITY_MASK);
			if (!groupp->addPart(part))
			{
				llwarns << "Particle did not go into its box !  Particle group center: "
						<< groupp->getCenterAgent() << " - mPosAgent = "
						<< part->mPosAgent << llendl;
				mViewerPartGroups.pop_back();
				delete groupp;
				groupp = NULL;
			}
			return_group = groupp;
		}
	}

	if (!return_group) // Failed to insert the particle
	{
		delete part;
	}

	return return_group;
}

LLViewerPartGroup* LLViewerPartSim::createViewerPartGroup(const LLVector3& pos_agent,
														  F32 desired_size,
														  bool hud)
{
	// Find a box that has a center position divisible by PART_SIM_BOX_SIDE
	// that encompasses pos_agent
	LLViewerPartGroup* groupp = new LLViewerPartGroup(pos_agent, desired_size,
													  hud);
	mViewerPartGroups.push_back(groupp);
	return groupp;
}

void LLViewerPartSim::shift(const LLVector3& offset)
{
	for (S32 i = 0, count = mViewerPartSources.size(); i < count; ++i)
	{
		mViewerPartSources[i]->mPosAgent += offset;
		mViewerPartSources[i]->mTargetPosAgent += offset;
		mViewerPartSources[i]->mLastUpdatePosAgent += offset;
	}

	for (S32 i = 0, count = mViewerPartGroups.size(); i < count; ++i)
	{
		mViewerPartGroups[i]->shift(offset);
	}
}

void LLViewerPartSim::updateSimulation()
{
	static LLFrameTimer update_timer;

	const F32 dt = llmin(update_timer.getElapsedTimeAndResetF32(), 0.1f);

 	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES))
	{
		return;
	}

	LL_FAST_TIMER(FTM_SIMULATE_PARTICLES);

//MK
	// ref_joint will also be used as a flag for restricted vision in the loop
	// (when NULL, there is no restriction in force).
	LLJoint* ref_joint = NULL;
	LLVector3 joint_pos;
	if (gRLenabled && gRLInterface.mVisionRestricted)
	{
		ref_joint = gRLInterface.getCamDistDrawFromJoint();
		joint_pos = ref_joint->getWorldPosition();
	}
//mk

	// Note: to avoid starvation of the particles allotment by high particle
	// count sources, the sources are updated in growing order of active
	// (rezzed) particles. The sorting is done at the end of this method. HB
	for (S32 i = 0, count = mViewerPartSources.size(); i < count; )
	{
		LLViewerPartSource* psrc = mViewerPartSources[i].get();

		if (psrc && !psrc->isDead())
		{
			bool upd = true;
			LLViewerObject* vobj = psrc->mSourceObjectp;
			bool is_volume = vobj && vobj->getPCode() == LL_PCODE_VOLUME;
			if (is_volume && !LLPipeline::sRenderAttachedParticles &&
				((LLVOVolume*)vobj)->isAttachment())
			{
				upd = false;
			}
			if (upd && vobj && vobj->isAvatar() &&
				((LLVOAvatar*)vobj)->isInMuteList())
			{
				upd = false;
			}
			if (upd && is_volume)
			{
				LLVOAvatar* avatar = vobj->getAvatar();
				if (avatar && avatar->isInMuteList())
				{
					upd = false;
				}
			}
//MK
			// If our vision is obscured enough, particles in world and worn by
			// other avatars may give away their position (because of a
			// rendering issue) => hide them if their source object is too far.
			if (upd && is_volume && ref_joint)
			{
				LLVector3 offset = ((LLVOVolume*)vobj)->getPositionRegion() -
								   joint_pos;
				if (offset.length() > gRLInterface.mCamDistDrawMax)
				{
					upd = false;
				}
			}
//mk
			if (upd)
			{
				psrc->update(dt);
			}
			else
			{
				// Pretend the source is too far away (used in sorting)
				psrc->mDistFromCamera = 1024.f;
			}

			// Increment the updates count for this source
			psrc->incPartUpdates();
		}

		if (!psrc || psrc->isDead())
		{
			if (i < --count)
			{
				mViewerPartSources[i] = mViewerPartSources.back();
			}
			mViewerPartSources.pop_back();
		}
		else
        {
			++i;
        }
	}

	S32 current_frame = LLViewerOctreeEntryData::getCurrentFrame();
	for (S32 i = 0, count = mViewerPartGroups.size(); i < count; ++i)
	{
		LLViewerPartGroup* pgroup = mViewerPartGroups[i];
		if (!pgroup) continue;	// Paranoia !

		LLViewerObject* vobj = pgroup->mVOPartGroupp;
		LLDrawable* drawable = NULL;
		if (vobj && !vobj->isDead())
		{
			drawable = vobj->mDrawable;
			if (drawable && drawable->isDead())
			{
				drawable = NULL;
			}
		}

		S32 visirate = 1;
		if (drawable)
		{
			LLSpatialGroup* group = drawable->getSpatialGroup();
			if (group && !group->isVisible() /* &&
				!group->isState(LLSpatialGroup::OBJECT_DIRTY)*/)
			{
				if (!vobj || vobj->getPCode() != LL_PCODE_VOLUME ||
					!(LLPipeline::sRenderAttachedParticles &&
					  ((LLVOVolume*)vobj)->isAttachment()))
				{
					visirate = 8;
					if (vobj)
					{
						LL_DEBUGS("Particles") << "Object " << vobj->getID()
											   << " gets its particles refresh sparsed because its group is not visible."
											   << LL_ENDL;
					}
				}
			}
		}

		if ((current_frame + pgroup->mID) % visirate == 0)
		{
			if (drawable)
			{
				gPipeline.markRebuild(drawable);
			}
			pgroup->updateParticles(dt * visirate);
			pgroup->mSkippedTime = 0.f;
			if (!pgroup->getCount())
			{
				delete pgroup;
				mViewerPartGroups[i--] = mViewerPartGroups.back();
				mViewerPartGroups.pop_back();
				--count;
			}
		}
		else
		{
			pgroup->mSkippedTime += dt;
		}
	}

	if (current_frame % 16 == 0)
	{
		if (sParticleCount > sMaxParticleCount * 0.875f &&
			sParticleAdaptiveRate < 2.f)
		{
			sParticleAdaptiveRate *= PART_ADAPT_RATE_MULT;
		}
		else
		{
			if (sParticleCount < sMaxParticleCount * 0.5f &&
				sParticleAdaptiveRate > 0.03125f)
			{
				sParticleAdaptiveRate *= PART_ADAPT_RATE_MULT_RECIP;
			}
		}
#if 0	// Way too spammy...
		LL_DEBUGS("Particles") << "Particles: " << sParticleCount
							   << " Adaptive Rate: " << sParticleAdaptiveRate
							   << LL_ENDL;
#endif
	}

	updatePartBurstRate();

	// Let's now sort the particle sources following the average number of
	// active (rezzed) particles they got per update, so that the sources
	// with less particles are updated first on next updateSimulation() run.
	// We also take into account the distance to the camera in our priorizing.
	// We use a std::map with a long integer as the key to perform a quick
	// sorting, by using the averaged particles count multiplied by a distance
	// factor as the MSLW of the key with a counter as the LSLW of the key, and
	// the source object address as the map data. We then std::move the smart
	// pointers to the sources from that map back into the sources vector. HB
	{
		LL_FAST_TIMER(FTM_SIM_PART_SORT);

		// Using an union to avoid costly bit shifting...
		union key_value
		{
			U64 key;
			U32 word[2];
		} kv;
#if LL_BIG_ENDIAN
# define LSW 1
# define MSW 0
#else
# define LSW 0
# define MSW 1
#endif
		constexpr F32 ONE32TH = 1.f / 32.f;
		U32 count = (U32)mViewerPartSources.size();
		llassert(count < 1 << 24);
		typedef std::map<U64, LLPointer<LLViewerPartSource> > sorting_map_t;
		sorting_map_t sources;
		for (U32 i = 0; i < count; ++i)
		{
			LLViewerPartSource* psrc = mViewerPartSources[i].get();

			// Compute the key we will sort the particles with
			U64 dist_ratio = psrc->mDistFromCamera * ONE32TH;
			if (dist_ratio == 0)
			{
				dist_ratio = 1;
			}
			kv.word[LSW] = i;
			kv.word[MSW] = psrc->getAveragePartCount() * dist_ratio;
			LL_DEBUGS("Particles") << "Particle source #" << i << " "
								   << psrc->getID();
			LLViewerObject* vobj = psrc->mSourceObjectp;
			if (vobj)
			{
				LL_CONT << " on object " << vobj->getID();
				if (vobj && vobj->getPCode() == LL_PCODE_VOLUME)
				{
					LLVOVolume* vvo = (LLVOVolume*)vobj;
					if (vvo && vvo->isAttachment())
					{
						LL_CONT << " (attachment)";
					}
				}
			}
			LL_CONT << " got an average particles count * distance factor of: "
					<< kv.word[MSW] << " - Resulting key: 0x" << std::hex
					<< kv.key << std::dec << LL_ENDL;

			// Do this last in the loop since mViewerPartSources[i] gets
			// invalidated (NULLed) by the move constructor of LLPointer.
			sources[kv.key] = std::move(mViewerPartSources[i]);
		}
		llassert_always(sources.size() == count);
		sorting_map_t::iterator it = sources.begin();
		for (U32 i = 0; i < count; ++i, ++it)
		{
			mViewerPartSources[i] = std::move(it->second);
		}

		LL_DEBUGS("Particles") << "Sorted particles sources:";
		for (U32 i = 0; i < count; ++i, ++it)
		{
			LL_CONT << "\n  #" << i + 1 << ", source "
					<< mViewerPartSources[i]->getID();
		}
		LL_CONT << LL_ENDL;
	}
}

void LLViewerPartSim::updatePartBurstRate()
{
	if (LLViewerOctreeEntryData::getCurrentFrame() & 0xf)
	{
		return;
	}
	if (sParticleCount >= MAX_PART_COUNT)
	{
		// Set rate to zero if above max particles count
		sParticleBurstRate = 0.f;
		return;
	}
	if (sParticleCount <= 0)
	{
		sParticleBurstRate += 0.00125f;
		return;
	}

	if (sParticleBurstRate <= 0.0000001f)
	{
		sParticleBurstRate += 0.0000001f;
		return;
	}

	F32 total_particles = sParticleCount / sParticleBurstRate; // Estimated
	F32 new_rate = llmin(0.9f * sMaxParticleCount / total_particles, 1.f);
	F32 delta_rate_threshold =
		llmin(0.1f * llmax(new_rate, sParticleBurstRate), 0.1f);
	F32 delta_rate = llclamp(new_rate - sParticleBurstRate,
							 -delta_rate_threshold, delta_rate_threshold);

	sParticleBurstRate = llclamp(sParticleBurstRate + 0.5f * delta_rate,
								 0.f, 1.f);
}

void LLViewerPartSim::addPartSource(LLPointer<LLViewerPartSource> sourcep)
{
	if (sourcep.isNull())
	{
		llwarns << "Null particle source !" << llendl;
		return;
	}
	sourcep->setStart();
	// Do this last since sourcep gets invalidated (NULLed) by the move
	// constructor of LLPointer. HB
	mViewerPartSources.emplace_back(std::move(sourcep));
}

void LLViewerPartSim::removeLastCreatedSource()
{
	mViewerPartSources.pop_back();
}

void LLViewerPartSim::cleanupRegion(LLViewerRegion* regionp)
{
	for (U32 i = 0, count = mViewerPartGroups.size(); i < count; )
	{
		if (mViewerPartGroups[i]->getRegion() == regionp)
		{
			delete mViewerPartGroups[i];
			if (i < --count)
			{
				mViewerPartGroups[i] = mViewerPartGroups.back();
			}
			mViewerPartGroups.pop_back();
		}
		else
		{
			++i;
		}
	}
}

void LLViewerPartSim::clearParticlesByID(U32 system_id)
{
	for (group_list_t::iterator g = mViewerPartGroups.begin(),
								end = mViewerPartGroups.end();
		 g != end; ++g)
	{
		(*g)->removeParticlesByID(system_id);
	}
	for (source_list_t::iterator i = mViewerPartSources.begin(),
								 end = mViewerPartSources.end();
		 i != end; ++i)
	{
		if ((*i)->getID() == system_id)
		{
			(*i)->setDead();
			break;
		}
	}
}

void LLViewerPartSim::clearParticlesByOwnerID(const LLUUID& task_id)
{
	for (source_list_t::iterator iter = mViewerPartSources.begin(),
								 end = mViewerPartSources.end();
		 iter != end; ++iter)
	{
		if ((*iter)->getOwnerUUID() == task_id)
		{
			clearParticlesByID((*iter)->getID());
		}
	}
}

void LLViewerPartSim::clearParticlesByRootObjectID(const LLUUID& object_id)
{
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (!objectp)
	{
		llwarns << "Tried to clear particles for non-existent object "
				<< object_id << llendl;
	}
	else if (objectp->isAvatar())
	{
		clearParticlesByOwnerID(object_id);
	}
	else
	{
		if (objectp->getPartSource())
		{
			clearParticlesByID(objectp->getPartSource()->getID());
		}
		for (LLViewerObject::child_list_t::const_iterator
				it = objectp->getChildren().begin(),
				end = objectp->getChildren().end();
			 it != end; ++it)
		{
			LLViewerObject* childp = *it;
			if (childp && childp->getPartSource())
			{
				clearParticlesByID(childp->getPartSource()->getID());
			}
		}
	}
}
