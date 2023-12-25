/**
 * @file llkeyframewalkmotion.cpp
 * @brief Implementation of LLKeyframeWalkMotion class.
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

#include "linden_common.h"

#include "llkeyframewalkmotion.h"

#include "llcharacter.h"
#include "llcriticaldamp.h"
#include "llmath.h"
#include "llmatrix3.h"

// Max speed (m/s) for which we adjust walk cycle speed
constexpr F32 MAX_WALK_PLAYBACK_SPEED = 8.f;
// Maximum two seconds a frame for calculating interpolation
constexpr F32 MAX_TIME_DELTA = 2.f;
// Maximum adjustment of walk animation playback speed
constexpr F32 SPEED_ADJUST_MAX = 2.5f;
// Maximum adjustment to walk animation playback speed for a second
constexpr F32 SPEED_ADJUST_MAX_SEC = 3.f;
// Final scaling for walk animation
constexpr F32 SPEED_FINAL_SCALING = 0.5f;
// Maximum drift compensation overall, in any direction
constexpr F32 DRIFT_COMP_MAX_TOTAL = 0.07f;	// 0.55f;
// Speed at which drift compensation total maxes out
constexpr F32 DRIFT_COMP_MAX_SPEED = 4.f;
constexpr F32 MAX_ROLL = 0.6f;

//-----------------------------------------------------------------------------
// LLKeyframeWalkMotion() class
//-----------------------------------------------------------------------------

LLKeyframeWalkMotion::LLKeyframeWalkMotion(const LLUUID& id)
:	LLKeyframeMotion(id),
	mCharacter(NULL),
	mRealTimeLast(0.f),
	mAdjTimeLast(0.f)
{
}

LLMotion::LLMotionInitStatus LLKeyframeWalkMotion::onInitialize(LLCharacter* character)
{
	mCharacter = character;
	return LLKeyframeMotion::onInitialize(character);
}

bool LLKeyframeWalkMotion::onActivate()
{
	mRealTimeLast = mAdjTimeLast = 0.f;
	return LLKeyframeMotion::onActivate();
}

void LLKeyframeWalkMotion::onDeactivate()
{
	mCharacter->removeAnimationData("Down Foot");
	LLKeyframeMotion::onDeactivate();
}

bool LLKeyframeWalkMotion::onUpdate(F32 time, U8* joint_mask)
{
	// compute time since last update
	F32 delta_time = time - mRealTimeLast;

	void* speed_ptr = mCharacter->getAnimationData("Walk Speed");
	F32 speed = speed_ptr ? *((F32*)speed_ptr) : 1.f;

	// Adjust the passage of time accordingly
	F32 adjusted_time = mAdjTimeLast + delta_time * speed;

	// Save time for next update
	mRealTimeLast = time;
	mAdjTimeLast = adjusted_time;

	// handle wrap around
	if (adjusted_time < 0.f)
	{
		adjusted_time = getDuration() + fmod(adjusted_time, getDuration());
	}

	// Let the base class update the cycle
	return LLKeyframeMotion::onUpdate(adjusted_time, joint_mask);
}

//-----------------------------------------------------------------------------
// LLWalkAdjustMotion() class
//-----------------------------------------------------------------------------

LLWalkAdjustMotion::LLWalkAdjustMotion(const LLUUID& id)
:	LLMotion(id),
	mLastTime(0.f),
	mAvgCorrection(0.f),
	mSpeedAdjust(0.f),
	mAnimSpeed(0.f),
	mAvgSpeed(0.f),
	mRelativeDir(0.f),
	mAnkleOffset(0.f)
{
	mName = "walk_adjust";
	mPelvisState = new LLJointState;
}

LLMotion::LLMotionInitStatus LLWalkAdjustMotion::onInitialize(LLCharacter* character)
{
	mCharacter = character;
	mLeftAnkleJoint = mCharacter->getJoint(LL_JOINT_KEY_ANKLELEFT);
	mRightAnkleJoint = mCharacter->getJoint(LL_JOINT_KEY_ANKLERIGHT);

	mPelvisJoint = mCharacter->getJoint(LL_JOINT_KEY_PELVIS);
	mPelvisState->setJoint(mPelvisJoint);
	if (!mPelvisJoint)
	{
		llwarns << getName() << ": cannot get pelvis joint." << llendl;
		return STATUS_FAILURE;
	}

	mPelvisState->setUsage(LLJointState::POS);
	addJointState(mPelvisState);

	return STATUS_SUCCESS;
}

bool LLWalkAdjustMotion::onActivate()
{
	mAvgCorrection = 0.f;
	mSpeedAdjust = 0.f;
	mAnimSpeed = 0.f;
	mAvgSpeed = 0.f;
	mRelativeDir = 1.f;
	mPelvisState->setPosition(LLVector3::zero);
	// store ankle positions for next frame
	mLastLeftAnklePos = mCharacter->getPosGlobalFromAgent(mLeftAnkleJoint->getWorldPosition());
	mLastRightAnklePos = mCharacter->getPosGlobalFromAgent(mRightAnkleJoint->getWorldPosition());

	F32 left_ankle_offset = (mLeftAnkleJoint->getWorldPosition() -
							 mCharacter->getCharacterPosition()).length();
	F32 right_ankle_offset = (mRightAnkleJoint->getWorldPosition() -
							 mCharacter->getCharacterPosition()).length();
	mAnkleOffset = llmax(left_ankle_offset, right_ankle_offset);

	return true;
}

bool LLWalkAdjustMotion::onUpdate(F32 time, U8* joint_mask)
{
	LLVector3 foot_corr;
	LLVector3 vel = mCharacter->getCharacterVelocity() * mCharacter->getTimeDilation();
	F32 delta_time = llclamp(time - mLastTime, 0.f, MAX_TIME_DELTA);
	mLastTime = time;

	LLQuaternion inv_rotation = ~mPelvisJoint->getWorldRotation();

	// get speed and normalize velocity vector
	LLVector3 ang_vel = mCharacter->getCharacterAngularVelocity() *
						mCharacter->getTimeDilation();
	F32 speed = llmin(vel.normalize(), MAX_WALK_PLAYBACK_SPEED);
	mAvgSpeed = lerp(mAvgSpeed, speed, LLCriticalDamp::getInterpolant(0.2f));

	// calculate facing vector in pelvis-local space
	// (either straight forward or back, depending on velocity)
	LLVector3 local_vel = vel * inv_rotation;
	if (local_vel.mV[VX] > 0.f)
	{
		mRelativeDir = 1.f;
	}
	else if (local_vel.mV[VX] < 0.f)
	{
		mRelativeDir = -1.f;
	}

	// calculate world-space foot drift
	LLVector3 left_ft_world_pos = mLeftAnkleJoint->getWorldPosition();
	LLVector3d left_ft_global_pos = mCharacter->getPosGlobalFromAgent(left_ft_world_pos);
	LLVector3 left_ft_delta(mLastLeftAnklePos - left_ft_global_pos);
	mLastLeftAnklePos = left_ft_global_pos;

	LLVector3 right_ft_world_pos = mRightAnkleJoint->getWorldPosition();
	LLVector3d right_ft_global_pos = mCharacter->getPosGlobalFromAgent(right_ft_world_pos);
	LLVector3 right_ft_delta(mLastRightAnklePos - right_ft_global_pos);
	mLastRightAnklePos = right_ft_global_pos;

	// find foot drift along velocity vector
	if (mAvgSpeed > 0.1)
	{
		// walking/running
		F32 leftFootDriftAmt = left_ft_delta * vel;
		F32 rightFootDriftAmt = right_ft_delta * vel;

		if (rightFootDriftAmt > leftFootDriftAmt)
		{
			foot_corr = right_ft_delta;
		} else
		{
			foot_corr = left_ft_delta;
		}
	}
	else
	{
		mAvgSpeed = ang_vel.length() * mAnkleOffset;
		mRelativeDir = 1.f;

		// standing/turning; find the lower foot
		if (left_ft_world_pos.mV[VZ] < right_ft_world_pos.mV[VZ])
		{
			// pivot on left foot
			foot_corr = left_ft_delta;
		}
		else
		{
			// pivot on right foot
			foot_corr = right_ft_delta;
		}
	}

	// rotate into avatar coordinates
	foot_corr = foot_corr * inv_rotation;

	// calculate ideal pelvis offset so that foot is glued to ground and damp
	// towards it the amount of foot slippage this frame + the offset applied
	// last frame
	mPelvisOffset = mPelvisState->getPosition() +
					lerp(LLVector3::zero, foot_corr,
						 LLCriticalDamp::getInterpolant(0.2f));

	// pelvis drift (along walk direction)
	mAvgCorrection = lerp(mAvgCorrection, foot_corr.mV[VX] * mRelativeDir,
						  LLCriticalDamp::getInterpolant(0.1f));

	// calculate average velocity of foot slippage
	F32 foot_slip_velocity = delta_time != 0.f ? -mAvgCorrection / delta_time
											   : 0.f;

	// Modulate speed by dot products of facing and velocity so that if we are
	// moving sideways, we slow down the animation and if we are moving
	// backward, we walk backward.
	F32 directional_factor = local_vel.mV[VX] * mRelativeDir;
	F32 new_speed_adj = 0.f;
	if (speed > 0.1f)
	{
		// calculate ratio of desired foot velocity to detected foot velocity
		new_speed_adj = llclamp(foot_slip_velocity -
								mAvgSpeed * (1.f - directional_factor),
								-SPEED_ADJUST_MAX, SPEED_ADJUST_MAX);
		new_speed_adj = lerp(mSpeedAdjust, new_speed_adj,
							 LLCriticalDamp::getInterpolant(0.2f));

		F32 speed_delta = new_speed_adj - mSpeedAdjust;
		speed_delta = llclamp(speed_delta, -SPEED_ADJUST_MAX_SEC * delta_time,
							  SPEED_ADJUST_MAX_SEC * delta_time);
		mSpeedAdjust = mSpeedAdjust + speed_delta;
	}
	else
	{
		mSpeedAdjust = lerp(mSpeedAdjust, 0.f,
							LLCriticalDamp::getInterpolant(0.2f));
	}

	mAnimSpeed = (mAvgSpeed + mSpeedAdjust) * mRelativeDir;
	mAnimSpeed = mAnimSpeed * SPEED_FINAL_SCALING;
	mCharacter->setAnimationData("Walk Speed", &mAnimSpeed);

	// clamp pelvis offset to a 90 degree arc behind the nominal position
	constexpr F32 drift_factor = DRIFT_COMP_MAX_TOTAL / DRIFT_COMP_MAX_SPEED;
	F32 drift_comp_max = drift_factor *
						 llclamp(speed, 0.f, DRIFT_COMP_MAX_SPEED);

	LLVector3 currentPelvisPos = mPelvisState->getJoint()->getPosition();

	// NB: this is an ADDITIVE amount that is accumulated every frame, so
	// clamping it alone won't do the trick must clamp with absolute position
	// of pelvis in mind
	mPelvisOffset.mV[VX] = llclamp(mPelvisOffset.mV[VX],
								   -drift_comp_max - currentPelvisPos.mV[VX],
								   drift_comp_max - currentPelvisPos.mV[VX]);
	mPelvisOffset.mV[VY] = llclamp(mPelvisOffset.mV[VY],
								   -drift_comp_max - currentPelvisPos.mV[VY],
								   drift_comp_max - currentPelvisPos.mV[VY]);
	mPelvisOffset.mV[VZ] = 0.f;

	// set position
	mPelvisState->setPosition(mPelvisOffset);

	mCharacter->setAnimationData("Pelvis Offset", &mPelvisOffset);

	return true;
}

void LLWalkAdjustMotion::onDeactivate()
{
	mCharacter->removeAnimationData("Walk Speed");
}

//-----------------------------------------------------------------------------
// LLFlyAdjustMotion() class
//-----------------------------------------------------------------------------

LLFlyAdjustMotion::LLFlyAdjustMotion(const LLUUID& id)
:	LLMotion(id),
	mRoll(0.f)
{
	mName = "fly_adjust";
	mPelvisState = new LLJointState;
}

LLMotion::LLMotionInitStatus LLFlyAdjustMotion::onInitialize(LLCharacter* character)
{
	mCharacter = character;

	LLJoint* pelvisJoint = mCharacter->getJoint(LL_JOINT_KEY_PELVIS);
	mPelvisState->setJoint(pelvisJoint);
	if (!pelvisJoint)
	{
		llwarns << getName() << ": cannot get pelvis joint." << llendl;
		return STATUS_FAILURE;
	}

	mPelvisState->setUsage(LLJointState::POS | LLJointState::ROT);
	addJointState(mPelvisState);

	return STATUS_SUCCESS;
}

bool LLFlyAdjustMotion::onActivate()
{
	mPelvisState->setPosition(LLVector3::zero);
	mPelvisState->setRotation(LLQuaternion::DEFAULT);
	mRoll = 0.f;
	return true;
}

bool LLFlyAdjustMotion::onUpdate(F32 time, U8* joint_mask)
{
	LLVector3 ang_vel = mCharacter->getCharacterAngularVelocity() *
						mCharacter->getTimeDilation();
	F32 speed = mCharacter->getCharacterVelocity().length();

	F32 roll_factor = clamp_rescale(speed, 7.f, 15.f, 0.f, -MAX_ROLL);
	F32 target_roll = llclamp(ang_vel.mV[VZ], -4.f, 4.f) * roll_factor;

	// Roll is critically damped interpolation between current roll and angular
	// velocity-derived target roll
	mRoll = lerp(mRoll, target_roll, LLCriticalDamp::getInterpolant(0.1f));

	LLQuaternion roll(mRoll, LLVector3(0.f, 0.f, 1.f));
	mPelvisState->setRotation(roll);

	return true;
}
