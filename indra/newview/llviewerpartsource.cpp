/**
 * @file llviewerpartsource.cpp
 * @brief LLViewerPartSource class implementation
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

#include "llviewerpartsource.h"

#include "llrender.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llpipeline.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewertexturelist.h"
#include "llworld.h"

static LLVOAvatar* find_avatar(const LLUUID& id)
{
	LLViewerObject* obj = gObjectList.findObject(id);
	while (obj && obj->isAttachment())
	{
		obj = (LLViewerObject*)obj->getParent();
	}
	if (obj && obj->isAvatar())
	{
		return (LLVOAvatar*)obj;
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerPartSource base class
///////////////////////////////////////////////////////////////////////////////

LLViewerPartSource::LLViewerPartSource(U32 type)
:	mType(type),
	mOwnerUUID(LLUUID::null),
	mLastPart(NULL),
	mPartFlags(0),
	mIsDead(false),
	mIsSuspended(false),
	mLastUpdateTime(0.f),
	mLastPartTime(0.f),
	mDistFromCamera(0.f),
	mDelay(0),
	mPartCount(0),
	mPartUpdates(1)
{
	static U32 id_seed = 0;
	mID = ++id_seed;
}

void LLViewerPartSource::setDead()
{
	mIsDead = true;
}

const LLUUID& LLViewerPartSource::getImageUUID() const
{
	return mImagep.notNull() ? mImagep->getID() : LLUUID::null;
}

void LLViewerPartSource::incPartCount()
{
	if (++mPartCount > U32_MAX)
	{
		mPartCount /= mPartUpdates;
		mPartUpdates = 1;
	}
}

U64 LLViewerPartSource::getAveragePartCount()
{
	return mPartCount / mPartUpdates;
}

//static
void LLViewerPartSource::updatePart(LLViewerPart& part, F32 dt)
{
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerPartSourceScript sub-class
///////////////////////////////////////////////////////////////////////////////

LLViewerPartSourceScript::LLViewerPartSourceScript(LLViewerObject* source_objp)
:	LLViewerPartSource(LL_PART_SOURCE_SCRIPT)
{
	llassert(source_objp);
	mSourceObjectp = source_objp;
	mPosAgent = mSourceObjectp->getPositionAgent();
	mImagep = gImgPixieSmall;
	mImagep->setAddressMode(LLTexUnit::TAM_CLAMP);
	LLMuteList::addObserver(this);
}

LLViewerPartSourceScript::~LLViewerPartSourceScript()
{
	LLMuteList::removeObserver(this);
}

void LLViewerPartSourceScript::setDead()
{
	mIsDead = true;
	mSourceObjectp = NULL;
	mTargetObjectp = NULL;
}

//virtual
void LLViewerPartSourceScript::onChange()
{
	if ((mOwnerUUID.notNull() &&
		 LLMuteList::isMuted(mOwnerUUID, LLMute::flagParticles)) ||
		(mSourceObjectp.notNull() &&
		 LLMuteList::isMuted(mSourceObjectp->getID())))
	{
		// Kill particle source because it has been muted !
		setDead();
	}
}

void LLViewerPartSourceScript::update(F32 dt)
{
	if (mIsSuspended)
	{
		return;
	}

	// By default (for particles that will not be updated), set a far distance
	mDistFromCamera = 1024.f;

	if (mOwnerAvatarp.isNull() && mOwnerUUID.notNull())
	{
		mOwnerAvatarp = find_avatar(mOwnerUUID);
	}
	if (mOwnerAvatarp.notNull() &&
		mOwnerAvatarp->getVisualMuteSettings() == LLVOAvatar::AV_DO_NOT_RENDER)
	{
		return;
	}

	F32 old_update_time = mLastUpdateTime;
	mLastUpdateTime += dt;

	F32 travelspeed = llmin(gViewerPartSim.getRefRate(), 1.f);

	F32 dt_update = mLastUpdateTime - mLastPartTime;

	// Update this for objects which have the follow flag set...
	if (mSourceObjectp.notNull())
	{
		if (mSourceObjectp->isDead())
		{
			mSourceObjectp = NULL;
		}
		else if (mSourceObjectp->mDrawable.notNull())
		{
			mPosAgent = mSourceObjectp->getRenderPosition();
		}
	}

	if (mTargetObjectp.isNull() && mPartSysData.mTargetUUID.notNull())
	{
		// Hmm, missing object, let's see if we can find it...
		LLViewerObject* target_objp =
			gObjectList.findObject(mPartSysData.mTargetUUID);
		setTargetObject(target_objp);
	}

	if (mTargetObjectp.notNull())
	{
		if (mTargetObjectp->isDead())
		{
			mTargetObjectp = NULL;
		}
		else if (mTargetObjectp->mDrawable.notNull())
		{
			mTargetPosAgent = mTargetObjectp->getRenderPosition();
		}
	}

	if (mTargetObjectp.isNull())
	{
		mTargetPosAgent = mPosAgent;
	}

	if (mPartSysData.mMaxAge &&
		mPartSysData.mStartAge + mLastUpdateTime + dt_update > mPartSysData.mMaxAge)
	{
		// Kill particle source because it has outlived its max age...
		setDead();
		return;
	}

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PARTICLES))
	{
		if (mSourceObjectp.notNull())
		{
			std::ostringstream ostr;
			ostr << mPartSysData;
			mSourceObjectp->setDebugText(ostr.str());
		}
	}

	bool first_run = false;
	if (old_update_time <= 0.f)
	{
		first_run = true;
		// Check we are not already muted...
		onChange();
		if (mIsDead) return;
	}

	// Distance from camera
	static LLCachedControl<F32> far_clip(gSavedSettings, "RenderFarClip");
	mDistFromCamera = (mPosAgent - gViewerCamera.getOrigin()).length();
	if (mDistFromCamera > far_clip)
	{
		// Do not even bother !
		LL_DEBUGS("Particles") << "Particle source " << mID
							   << " skipped because it is too far away."
							   << LL_ENDL;
		return;
	}

	F32 pixel_meter_ratio = gViewerCamera.getPixelMeterRatio();

	F32 max_time = llmax(1.f, 10.f * mPartSysData.mBurstRate);
	dt_update = llmin(max_time, dt_update);
	while (dt_update > mPartSysData.mBurstRate || first_run)
	{
		first_run = false;

		// Update the rotation of the particle source by the angular velocity
		// First check to see if there is still an angular velocity.
		F32 angular_velocity_mag = mPartSysData.mAngularVelocity.length();
		if (angular_velocity_mag != 0.f)
		{
			F32 av_angle = dt * angular_velocity_mag;
			LLQuaternion dquat(av_angle, mPartSysData.mAngularVelocity);
			mRotation *= dquat;
		}
		else
		{
			// No angular velocity. Reset our rotation.
			mRotation.setEulerAngles(0.f, 0.f, 0.f);
		}

		if (LLViewerPartSim::aboveParticleLimit())
		{
			// Do not bother doing any more updates if we are above the
			// particle limit, just give up.
			mLastPartTime = mLastUpdateTime;
            break;
		}

		// Find the greatest length that the shortest side of a system particle
		// is expected to have
		F32 max_short_side =
			llmax(llmax(llmin(mPartSysData.mPartData.mStartScale[0],
							  mPartSysData.mPartData.mStartScale[1]),
						llmin(mPartSysData.mPartData.mEndScale[0],
							  mPartSysData.mPartData.mEndScale[1])),
						llmin((mPartSysData.mPartData.mStartScale[0] +
							   mPartSysData.mPartData.mEndScale[0]) * 0.5f,
							  (mPartSysData.mPartData.mStartScale[1] +
							   mPartSysData.mPartData.mEndScale[1]) * 0.5f));

		// Maximum distance at which spawned particles will be viewable
		F32 max_dist = max_short_side * pixel_meter_ratio;

		if (max_dist < 0.25f)
		{
			// < 1 pixel wide at a distance of >= 25cm. Particles this tiny are
			// useless and mostly spawned by buggy sources
			mLastPartTime = mLastUpdateTime;
			LL_DEBUGS("Particles") << "Particle source " << mID
								   << " skipped because it is too small."
								   << LL_ENDL;
			break;
		}

		// Particle size vs distance vs maxage throttling

		F32 limited_rate = 0.f;
		if (mDistFromCamera - max_dist > 0.f)
		{
			F32 max_age = mPartSysData.mPartData.mMaxAge;
			if ((mDistFromCamera - max_dist) * travelspeed > max_age - 0.2f)
			{
				// You need to travel faster than 1 divided by reference rate
				// m/s directly towards these particles to see them at least
				// 0.2s.
				LL_DEBUGS("Particles") << "Particle source " << mID
									   << " skipped because it won't have time to show up."
									   << LL_ENDL;
				mLastPartTime = mLastUpdateTime;
				break;
			}
			limited_rate = (mDistFromCamera - max_dist) * travelspeed / max_age;
		}

		if (mDelay)
		{
			limited_rate = llmax(limited_rate, 0.01f * mDelay--);
		}

		bool ribbon = (mPartSysData.mPartData.mFlags &
					   LLPartData::LL_PART_RIBBON_MASK) != 0;
		for (S32 i = 0; i < mPartSysData.mBurstPartCount; ++i)
		{
			F32 burst_rate = gViewerPartSim.getBurstRate();
			if (burst_rate == 0.f)
			{
				LL_DEBUGS("Particles") << "Particle source " << mID
									   << " skipped because MAX_PART_COUNT was reached."
									   << LL_ENDL;
				break;	// Do not insist, we reached MAX_PART_COUNT...
			}
			// Always create at least one particle
			if (i > 0 && ll_frand() < llmax(1.f - burst_rate, limited_rate))
			{
				// Limit particle generation
				continue;
			}

			if (ribbon && mLastPart &&
				(mLastPart->mPosAgent - mPosAgent).length() <= .005f)
			{
				// Do not generate a new ribbon particle if its length is too
				// small to be visible
				continue;
			}

			LLViewerPart* part = new LLViewerPart();

			part->init(this, mImagep, NULL);
			part->mFlags = mPartSysData.mPartData.mFlags;
			if (mSourceObjectp.notNull() && mSourceObjectp->isHUDAttachment())
			{
				part->mFlags |= LLPartData::LL_PART_HUD;
			}

			if (part->mFlags & LLPartData::LL_PART_RIBBON_MASK && mLastPart)
			{
				// Set previous particle parent to this particle to chain
				// ribbons together
				mLastPart->mParent = part;
				part->mChild = mLastPart;
				part->mAxis = LLVector3::z_axis;

				if (mSourceObjectp.notNull())
				{
					LLQuaternion rot = mSourceObjectp->getRenderRotation();
					part->mAxis *= rot;
				}
			}
			mLastPart = part;

			part->mMaxAge = mPartSysData.mPartData.mMaxAge;
			part->mStartColor = mPartSysData.mPartData.mStartColor;
			part->mEndColor = mPartSysData.mPartData.mEndColor;
			part->mColor = part->mStartColor;

			part->mStartScale = mPartSysData.mPartData.mStartScale;
			part->mEndScale = mPartSysData.mPartData.mEndScale;
			part->mScale = part->mStartScale;

			part->mAccel = mPartSysData.mPartAccel;

			part->mBlendFuncDest = mPartSysData.mPartData.mBlendFuncDest;
			part->mBlendFuncSource = mPartSysData.mPartData.mBlendFuncSource;

			part->mStartGlow = mPartSysData.mPartData.mStartGlow;
			part->mEndGlow = mPartSysData.mPartData.mEndGlow;
			part->mGlow = LLColor4U(0, 0, 0,
									(U8)ll_roundp(part->mStartGlow * 255.f));

			if (mPartSysData.mPattern & LLPartSysData::LL_PART_SRC_PATTERN_DROP)
			{
				part->mPosAgent = mPosAgent;
				part->mVelocity.clear();
			}
			else if (mPartSysData.mPattern &
					 LLPartSysData::LL_PART_SRC_PATTERN_EXPLODE)
			{
				part->mPosAgent = mPosAgent;
				LLVector3 part_dir_vector;

				F32 mvs;
				do
				{
					part_dir_vector.mV[VX] = ll_frand(2.f) - 1.f;
					part_dir_vector.mV[VY] = ll_frand(2.f) - 1.f;
					part_dir_vector.mV[VZ] = ll_frand(2.f) - 1.f;
					mvs = part_dir_vector.lengthSquared();
				}
				while (mvs > 1.f || mvs < 0.01f);

				part_dir_vector.normalize();
				part->mPosAgent += mPartSysData.mBurstRadius * part_dir_vector;
				part->mVelocity = part_dir_vector;
				F32 speed = mPartSysData.mBurstSpeedMin +
							ll_frand(mPartSysData.mBurstSpeedMax -
									 mPartSysData.mBurstSpeedMin);
				part->mVelocity *= speed;
			}
			else if ((mPartSysData.mPattern &
					  LLPartSysData::LL_PART_SRC_PATTERN_ANGLE) ||
					 (mPartSysData.mPattern &
					  LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE))
			{
				part->mPosAgent = mPosAgent;

				// Original implemenetation for part_dir_vector was just:
				LLVector3 part_dir_vector = LLVector3::z_axis;
				// params from the script...
				// outer = outer cone angle
				// inner = inner cone angle
				//		between outer and inner there will be particles
				F32 inner_angle = mPartSysData.mInnerAngle;
				F32 outer_angle = mPartSysData.mOuterAngle;

				// Generate a random angle within the given space...
				F32 angle = inner_angle + ll_frand(outer_angle - inner_angle);
				// Split which side it will go on randomly...
				if (ll_frand() < 0.5)
				{
					angle = -angle;
				}
				// Both patterns rotate around the x-axis first:
				part_dir_vector.rotVec(angle, 1.f, 0.f, 0.f);

				// If this is a cone pattern, rotate again to create the cone.
				if (mPartSysData.mPattern &
					LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE)
				{
					part_dir_vector.rotVec(ll_frand(4 * F_PI), 0.f, 0.f, 1.f);
				}

				// Only apply this rotation if using the deprecated angles.
				if (!(mPartSysData.mFlags &
					  LLPartSysData::LL_PART_USE_NEW_ANGLE))
				{
					// Deprecated...
					part_dir_vector.rotVec(outer_angle, 1.f, 0.f, 0.f);
				}

				if (mSourceObjectp)
				{
					part_dir_vector = part_dir_vector *
									  mSourceObjectp->getRenderRotation();
				}

				part_dir_vector = part_dir_vector * mRotation;

				part->mPosAgent += mPartSysData.mBurstRadius*part_dir_vector;

				part->mVelocity = part_dir_vector;

				F32 speed = mPartSysData.mBurstSpeedMin +
							ll_frand(mPartSysData.mBurstSpeedMax -
									 mPartSysData.mBurstSpeedMin);
				part->mVelocity *= speed;
			}
			else
			{
				part->mPosAgent = mPosAgent;
				part->mVelocity.set(0.f, 0.f, 0.f);
				LL_DEBUGS("Particles") << "Unknown source pattern: "
									   << (S32)mPartSysData.mPattern
									   << LL_ENDL;
			}

			if (part->mFlags & LLPartData::LL_PART_FOLLOW_SRC_MASK ||
				part->mFlags & LLPartData::LL_PART_TARGET_LINEAR_MASK)
			{
				mPartSysData.mBurstRadius = 0;
			}

			gViewerPartSim.addPart(part);
		}

		mLastPartTime = mLastUpdateTime;
		dt_update -= mPartSysData.mBurstRate;
	}
}

//static
LLPointer<LLViewerPartSourceScript>
	LLViewerPartSourceScript::unpackPSS(LLViewerObject* source_objp,
										LLPointer<LLViewerPartSourceScript> pssp,
										S32 block_num)
{
	if (LLPartSysData::isNullPS(block_num))
	{
		return NULL;
	}

	if (!pssp)
	{
		LLPointer<LLViewerPartSourceScript> new_pssp =
			new LLViewerPartSourceScript(source_objp);
		if (!new_pssp->mPartSysData.unpackBlock(block_num))
		{
			return NULL;
		}
		if (new_pssp->mPartSysData.mTargetUUID.notNull())
		{
			LLViewerObject* target_objp =
				gObjectList.findObject(new_pssp->mPartSysData.mTargetUUID);
			new_pssp->setTargetObject(target_objp);
		}
		return new_pssp;
	}

	if (!pssp->mPartSysData.unpackBlock(block_num))
	{
		return NULL;
	}

	F32 prev_max_age = pssp->mPartSysData.mMaxAge;
	F32 prev_start_age = pssp->mPartSysData.mStartAge;
	if (pssp->mPartSysData.mMaxAge &&
		(prev_max_age != pssp->mPartSysData.mMaxAge ||
		 prev_start_age != pssp->mPartSysData.mStartAge))
	{
		// Reusing existing pss, so reset time to allow particles to start
		// again
		pssp->mLastUpdateTime = pssp->mLastPartTime = 0.f;
	}

	if (pssp->mPartSysData.mTargetUUID.notNull())
	{
		LLViewerObject* target_objp =
			gObjectList.findObject(pssp->mPartSysData.mTargetUUID);
		pssp->setTargetObject(target_objp);
	}

	return pssp;
}

LLPointer<LLViewerPartSourceScript>
	LLViewerPartSourceScript::unpackPSS(LLViewerObject* source_objp,
										LLPointer<LLViewerPartSourceScript> pssp,
										LLDataPacker& dp, bool legacy)
{
	if (!pssp)
	{
		LLPointer<LLViewerPartSourceScript> new_pssp =
			new LLViewerPartSourceScript(source_objp);
		bool res = legacy ? new_pssp->mPartSysData.unpackLegacy(dp)
						  : new_pssp->mPartSysData.unpack(dp);
		if (!res)
		{
			return NULL;
		}

		if (new_pssp->mPartSysData.mTargetUUID.notNull())
		{
			LLViewerObject* target_objp =
				gObjectList.findObject(new_pssp->mPartSysData.mTargetUUID);
			new_pssp->setTargetObject(target_objp);
		}
		return new_pssp;
	}

	bool res = legacy ? pssp->mPartSysData.unpackLegacy(dp)
					  : pssp->mPartSysData.unpack(dp);
	if (!res)
	{
		return NULL;
	}

	if (pssp->mPartSysData.mTargetUUID.notNull())
	{
		LLViewerObject* target_objp =
			gObjectList.findObject(pssp->mPartSysData.mTargetUUID);
		pssp->setTargetObject(target_objp);
	}
	return pssp;
}

//static
LLPointer<LLViewerPartSourceScript>
	LLViewerPartSourceScript::createPSS(LLViewerObject* source_objp,
										const LLPartSysData& part_params)
{
	LLPointer<LLViewerPartSourceScript> new_pssp =
		new LLViewerPartSourceScript(source_objp);

	new_pssp->mPartSysData = part_params;

	if (new_pssp->mPartSysData.mTargetUUID.notNull())
	{
		LLViewerObject* target_objp =
			gObjectList.findObject(new_pssp->mPartSysData.mTargetUUID);
		new_pssp->setTargetObject(target_objp);
	}

	return new_pssp;
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerPartSourceSpiral sub-class
///////////////////////////////////////////////////////////////////////////////

LLViewerPartSourceSpiral::LLViewerPartSourceSpiral(const LLVector3& pos)
:	LLViewerPartSource(LL_PART_SOURCE_CHAT)
{
	mPosAgent = pos;
}

void LLViewerPartSourceSpiral::setDead()
{
	mIsDead = true;
	mSourceObjectp = NULL;
}

//static
void LLViewerPartSourceSpiral::updatePart(LLViewerPart& part, F32 dt)
{
	F32 frac = part.mLastUpdateTime / part.mMaxAge;

	LLVector3 center_pos;
	LLViewerPartSourceSpiral* pss =
		(LLViewerPartSourceSpiral*)part.mPartSourcep.get();
	if (pss->mSourceObjectp.notNull() &&
		pss->mSourceObjectp->mDrawable.notNull())
	{
		part.mPosAgent = pss->mSourceObjectp->getRenderPosition();
	}
	else
	{
		part.mPosAgent = pss->mPosAgent;
	}
	F32 x = sinf(F_TWO_PI * frac + part.mParameter);
	F32 y = cosf(F_TWO_PI * frac + part.mParameter);

	part.mPosAgent.mV[VX] += x;
	part.mPosAgent.mV[VY] += y;
	part.mPosAgent.mV[VZ] += -0.5f + frac;
}

void LLViewerPartSourceSpiral::update(F32 dt)
{
	if (!mImagep)
	{
		mImagep = gImgPixieSmall;
	}

	constexpr F32 RATE = 0.025f;

	mLastUpdateTime += dt;

	F32 dt_update = mLastUpdateTime - mLastPartTime;
	F32 max_time = llmax(1.f, 10.f * RATE);
	dt_update = llmin(max_time, dt_update);

	if (dt_update > RATE)
	{
		mLastPartTime = mLastUpdateTime;
		if (!LLViewerPartSim::shouldAddPart())
		{
			// Particle simulation says we have too many particles, skip all
			// this
			return;
		}

		if (mSourceObjectp.notNull() && mSourceObjectp->mDrawable.notNull())
		{
			mPosAgent = mSourceObjectp->getRenderPosition();
		}
		LLViewerPart* part = new LLViewerPart();
		part->init(this, mImagep, updatePart);
		part->mStartColor = mColor;
		part->mEndColor = mColor;
		part->mEndColor.mV[3] = 0.f;
		part->mPosAgent = mPosAgent;
		part->mMaxAge = 1.f;
		part->mFlags = LLViewerPart::LL_PART_INTERP_COLOR_MASK;
		part->mLastUpdateTime = 0.f;
		part->mScale.mV[0] = part->mScale.mV[1] = 0.25f;
		part->mParameter = ll_frand(F_TWO_PI);
		part->mBlendFuncDest = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
		part->mBlendFuncSource = LLRender::BF_SOURCE_ALPHA;
		part->mStartGlow = part->mEndGlow = 0.f;
		part->mGlow = LLColor4U(0, 0, 0, 0);

		gViewerPartSim.addPart(part);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerPartSourceBeam sub-class
///////////////////////////////////////////////////////////////////////////////

LLViewerPartSourceBeam::LLViewerPartSourceBeam()
:	LLViewerPartSource(LL_PART_SOURCE_BEAM)
{
}

void LLViewerPartSourceBeam::setDead()
{
	mIsDead = true;
	mSourceObjectp = NULL;
	mTargetObjectp = NULL;
}

//static
void LLViewerPartSourceBeam::updatePart(LLViewerPart& part, F32 dt)
{
	F32 frac = part.mLastUpdateTime/part.mMaxAge;

	LLViewerPartSourceBeam* psb =
		(LLViewerPartSourceBeam*)part.mPartSourcep.get();
	if (psb->mSourceObjectp.isNull())
	{
		part.mFlags = LLPartData::LL_PART_DEAD_MASK;
		return;
	}

	LLVector3 source_pos_agent;
	LLVector3 target_pos_agent;
	if (psb->mSourceObjectp.notNull() &&
		psb->mSourceObjectp->mDrawable.notNull())
	{
		if (psb->mSourceObjectp->isAvatar())
		{
			LLViewerObject* objp = psb->mSourceObjectp;
			LLVOAvatar* avp = (LLVOAvatar*)objp;
			source_pos_agent = avp->mWristLeftp->getWorldPosition();
		}
		else
		{
			source_pos_agent = psb->mSourceObjectp->getRenderPosition();
		}
	}
	if (psb->mTargetObjectp.notNull() &&
		psb->mTargetObjectp->mDrawable.notNull())
	{
		target_pos_agent = psb->mTargetObjectp->getRenderPosition();
	}

	part.mPosAgent = (1.f - frac) * source_pos_agent;
	if (psb->mTargetObjectp.isNull())
	{
		part.mPosAgent += frac *
						  gAgent.getPosAgentFromGlobal(psb->mLKGTargetPosGlobal);
	}
	else
	{
		part.mPosAgent += frac * target_pos_agent;
	}
}

void LLViewerPartSourceBeam::update(F32 dt)
{
	mLastUpdateTime += dt;

	if (mSourceObjectp.notNull() && mSourceObjectp->mDrawable.notNull())
	{
		if (mSourceObjectp->isAvatar())
		{
			LLViewerObject* objp = mSourceObjectp;
			LLVOAvatar* avp = (LLVOAvatar*)objp;
			mPosAgent = avp->mWristLeftp->getWorldPosition();
		}
		else
		{
			mPosAgent = mSourceObjectp->getRenderPosition();
		}
	}

	if (mTargetObjectp.notNull() && mTargetObjectp->mDrawable.notNull())
	{
		mTargetPosAgent = mTargetObjectp->getRenderPosition();
	}
	else if (!mLKGTargetPosGlobal.isExactlyZero())
	{
		mTargetPosAgent = gAgent.getPosAgentFromGlobal(mLKGTargetPosGlobal);
	}

	F32 dt_update = mLastUpdateTime - mLastPartTime;
	constexpr F32 RATE = 0.025f;
	F32 max_time = llmax(1.f, 10.f * RATE);
	dt_update = llmin(max_time, dt_update);
	if (dt_update <= RATE)
	{
		return;
	}

	mLastPartTime = mLastUpdateTime;
	if (!LLViewerPartSim::shouldAddPart())
	{
		// Particle simulation says we have too many particles, skip all this
		return;
	}

	if (!mImagep)
	{
		mImagep = gImgPixieSmall;
	}

	LLViewerPart* part = new LLViewerPart();
	part->init(this, mImagep, updatePart);

	part->mFlags = LLPartData::LL_PART_INTERP_COLOR_MASK |
				   LLPartData::LL_PART_INTERP_SCALE_MASK |
				   LLPartData::LL_PART_TARGET_POS_MASK |
				   LLPartData::LL_PART_FOLLOW_VELOCITY_MASK;
	part->mMaxAge = 0.5f;
	part->mStartColor = mColor;
	part->mEndColor = part->mStartColor;
	part->mEndColor.mV[3] = 0.4f;
	part->mColor = part->mStartColor;

	part->mStartScale = LLVector2(0.1f, 0.1f);
	part->mEndScale = LLVector2(0.1f, 0.1f);
	part->mScale = part->mStartScale;

	part->mPosAgent = mPosAgent;
	part->mVelocity = mTargetPosAgent - mPosAgent;

	part->mBlendFuncDest = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
	part->mBlendFuncSource = LLRender::BF_SOURCE_ALPHA;
	part->mStartGlow = 0.f;
	part->mEndGlow = 0.f;
	part->mGlow = LLColor4U(0, 0, 0, 0);

	gViewerPartSim.addPart(part);
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerPartSourceChat sub-class
///////////////////////////////////////////////////////////////////////////////

LLViewerPartSourceChat::LLViewerPartSourceChat(const LLVector3& pos)
:	LLViewerPartSource(LL_PART_SOURCE_SPIRAL)
{
	mPosAgent = pos;
}

void LLViewerPartSourceChat::setDead()
{
	mIsDead = true;
	mSourceObjectp = NULL;
}

//static
void LLViewerPartSourceChat::updatePart(LLViewerPart& part, F32 dt)
{
	F32 frac = part.mLastUpdateTime / part.mMaxAge;

	LLVector3 center_pos;
	LLViewerPartSourceChat* pss =
		(LLViewerPartSourceChat*)part.mPartSourcep.get();
	if (pss->mSourceObjectp.notNull() &&
		pss->mSourceObjectp->mDrawable.notNull())
	{
		part.mPosAgent = pss->mSourceObjectp->getRenderPosition();
	}
	else
	{
		part.mPosAgent = pss->mPosAgent;
	}
	F32 x = sinf(F_TWO_PI * frac + part.mParameter);
	F32 y = cosf(F_TWO_PI * frac + part.mParameter);

	part.mPosAgent.mV[VX] += x;
	part.mPosAgent.mV[VY] += y;
	part.mPosAgent.mV[VZ] += -0.5f + frac;
}

void LLViewerPartSourceChat::update(F32 dt)
{
	if (!mImagep)
	{
		mImagep = gImgPixieSmall;
	}

	mLastUpdateTime += dt;

	if (mLastUpdateTime > 2.f)
	{
		// Kill particle source because it has outlived its max age...
		setDead();
		return;
	}

	F32 dt_update = mLastUpdateTime - mLastPartTime;

	// Clamp us to generating at most one second's worth of particles on a
	// frame
	constexpr F32 RATE = 0.025f;
	F32 max_time = llmax(1.f, 10.f * RATE);
	dt_update = llmin(max_time, dt_update);
	if (dt_update > RATE)
	{
		mLastPartTime = mLastUpdateTime;
		if (!LLViewerPartSim::shouldAddPart())
		{
			// Particle simulation says we have too many particles, skip all this
			return;
		}

		if (mSourceObjectp.notNull() && mSourceObjectp->mDrawable.notNull())
		{
			mPosAgent = mSourceObjectp->getRenderPosition();
		}
		LLViewerPart* part = new LLViewerPart();
		part->init(this, mImagep, updatePart);
		part->mStartColor = mColor;
		part->mEndColor = mColor;
		part->mEndColor.mV[3] = 0.f;
		part->mPosAgent = mPosAgent;
		part->mMaxAge = 1.f;
		part->mFlags = LLViewerPart::LL_PART_INTERP_COLOR_MASK;
		part->mLastUpdateTime = 0.f;
		part->mScale.mV[0] = part->mScale.mV[1] = 0.25f;
		part->mParameter = ll_frand(F_TWO_PI);
		part->mBlendFuncDest = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
		part->mBlendFuncSource = LLRender::BF_SOURCE_ALPHA;
		part->mStartGlow = part->mEndGlow = 0.f;
		part->mGlow = LLColor4U(0, 0, 0, 0);

		gViewerPartSim.addPart(part);
	}
}
