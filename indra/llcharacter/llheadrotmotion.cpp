/**
 * @file llheadrotmotion.cpp
 * @brief Implementation of LLHeadRotMotion class.
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

#include "llheadrotmotion.h"

#include "llcharacter.h"
#include "llrand.h"
#include "llmatrix3.h"
#include "llvector3d.h"
#include "llcriticaldamp.h"

// Torso rotation factor
constexpr F32 TORSO_LAG	= 0.35f;
// Neck rotation factor
constexpr F32 NECK_LAG = 0.5f;
// Half-life of lookat targeting for head
constexpr F32 HEAD_LOOKAT_LAG_HALF_LIFE	= 0.15f;
// Half-life of lookat targeting for torso
constexpr F32 TORSO_LOOKAT_LAG_HALF_LIFE = 0.27f;
// Limit angle for head rotation
constexpr F32 HEAD_ROTATION_CONSTRAINT = F_PI_BY_TWO * 0.8f;

// Minimum distance from head before we turn to look at it
constexpr F32 MIN_HEAD_LOOKAT_DISTANCE = 0.3f;
// Minimum amount of time between eye "jitter" motions
constexpr F32 EYE_JITTER_MIN_TIME = 0.3f;
// Maximum amount of time between eye "jitter" motions
constexpr F32 EYE_JITTER_MAX_TIME = 2.5f;
// Maximum yaw of eye jitter motion
constexpr F32 EYE_JITTER_MAX_YAW = 0.08f;
// Maximum pitch of eye jitter motion
constexpr F32 EYE_JITTER_MAX_PITCH = 0.015f;
// Minimum amount of time between eye "look away" motions
constexpr F32 EYE_LOOK_AWAY_MIN_TIME = 5.f;
// Maximum amount of time between eye "look away" motions
constexpr F32 EYE_LOOK_AWAY_MAX_TIME = 15.f;
// Minimum amount of time before looking back after looking away
constexpr F32 EYE_LOOK_BACK_MIN_TIME = 1.f;
// Maximum amount of time before looking back after looking away
constexpr F32 EYE_LOOK_BACK_MAX_TIME = 5.f;
// Maximum yaw of eye look away motion
constexpr F32 EYE_LOOK_AWAY_MAX_YAW = 0.15f;
// Maximum pitch of look away motion
constexpr F32 EYE_LOOK_AWAY_MAX_PITCH = 0.12f;
// Maximum angle in radians for eye rotation
constexpr F32 EYE_ROT_LIMIT_ANGLE = F_PI_BY_TWO * 0.3f;
 // Minimum amount of time between blinks
constexpr F32 EYE_BLINK_MIN_TIME = 0.5f;
// Maximum amount of time between blinks
constexpr F32 EYE_BLINK_MAX_TIME = 8.f;
// How long the eye stays closed in a blink
constexpr F32 EYE_BLINK_CLOSE_TIME = 0.03f;
// Seconds it takes for a eye open/close movement
constexpr F32 EYE_BLINK_SPEED = 0.015f;
// Time between one eye starting a blink and the other following
constexpr F32 EYE_BLINK_TIME_DELTA = 0.005f;

//-----------------------------------------------------------------------------
// LLHeadRotMotion() class
//-----------------------------------------------------------------------------

LLHeadRotMotion::LLHeadRotMotion(const LLUUID& id)
:	LLMotion(id),
	mCharacter(NULL),
	mTorsoJoint(NULL),
	mHeadJoint(NULL),
	mEnabled(true)
{
	mName = "head_rot";

	mTorsoState = new LLJointState;
	mNeckState = new LLJointState;
	mHeadState = new LLJointState;
}

LLMotion::LLMotionInitStatus LLHeadRotMotion::onInitialize(LLCharacter* character)
{
	if (!character)
	{
		return STATUS_FAILURE;
	}
	mCharacter = character;

	mPelvisJoint = character->getJoint(LL_JOINT_KEY_PELVIS);
	if (!mPelvisJoint)
	{
		llinfos << getName() << ": cannot get pelvis joint." << llendl;
		return STATUS_FAILURE;
	}

	mRootJoint = character->getJoint(LL_JOINT_KEY_ROOT);
	if (!mRootJoint)
	{
		llinfos << getName() << ": cannot get root joint." << llendl;
		return STATUS_FAILURE;
	}

	mTorsoJoint = character->getJoint(LL_JOINT_KEY_TORSO);
	if (!mTorsoJoint)
	{
		llinfos << getName() << ": cannot get torso joint." << llendl;
		return STATUS_FAILURE;
	}

	mHeadJoint = character->getJoint(LL_JOINT_KEY_HEAD);
	if (!mHeadJoint)
	{
		llinfos << getName() << ": cannot get head joint." << llendl;
		return STATUS_FAILURE;
	}

	mTorsoState->setJoint(mTorsoJoint);
	if (!mTorsoState->getJoint())
	{
		llinfos << getName() << ": cannot set torso joint." << llendl;
		return STATUS_FAILURE;
	}

	mNeckState->setJoint(character->getJoint(LL_JOINT_KEY_NECK));
	if (!mNeckState->getJoint())
	{
		llinfos << getName() << ": cannot set neck joint." << llendl;
		return STATUS_FAILURE;
	}

	mHeadState->setJoint(character->getJoint(LL_JOINT_KEY_HEAD));
	if (!mHeadState->getJoint())
	{
		llinfos << getName() << ": cannot set head joint." << llendl;
		return STATUS_FAILURE;
	}

	mTorsoState->setUsage(LLJointState::ROT);
	mNeckState->setUsage(LLJointState::ROT);
	mHeadState->setUsage(LLJointState::ROT);

	addJointState(mTorsoState);
	addJointState(mNeckState);
	addJointState(mHeadState);

	mLastHeadRot.loadIdentity();

	return STATUS_SUCCESS;
}

bool LLHeadRotMotion::onUpdate(F32 time, U8* joint_mask)
{
	if (!mEnabled)
	{
		// Yes, return true even when not enabled since this motion relays the
		// target position to code that moves the eyes and such; we want to
		// keep the targeting working but bypass the head motion effects.
		return true;
	}

	LLQuaternion target_head_rot;
	LLQuaternion cur_root_rot = mRootJoint->getWorldRotation();
	LLQuaternion cur_inv_root_rot = ~cur_root_rot;

	F32 head_slerp_amt = LLCriticalDamp::getInterpolant(HEAD_LOOKAT_LAG_HALF_LIFE);
	F32 torso_slerp_amt = LLCriticalDamp::getInterpolant(TORSO_LOOKAT_LAG_HALF_LIFE);

	LLVector3* target_pos = (LLVector3*)mCharacter->getAnimationData("LookAtPoint");
	if (target_pos)
	{
		LLVector3 headLookAt = *target_pos;

		F32 lookatDistance = headLookAt.normalize();
		if (lookatDistance < MIN_HEAD_LOOKAT_DISTANCE)
		{
			target_head_rot = mPelvisJoint->getWorldRotation();
		}
		else
		{
			LLVector3 root_up = LLVector3(0.f, 0.f, 1.f) * cur_root_rot;
			LLVector3 left(root_up % headLookAt);
			// If look_at has zero length, fail; if look_at and skyward are
			// parallel, fail. Test both of these conditions with a cross
			// product.
			if (left.lengthSquared() < 0.15f)
			{
				LLVector3 root_at = LLVector3(1.f, 0.f, 0.f) *
									cur_root_rot;
				root_at.mV[VZ] = 0.f;
				root_at.normalize();

				headLookAt = lerp(headLookAt, root_at, 0.4f);
				headLookAt.normalize();

				left = root_up % headLookAt;
			}

			// Make sure look_at and skyward and not parallel and neither are
			// zero length
			LLVector3 up(headLookAt % left);

			target_head_rot = LLQuaternion(headLookAt, left, up);
		}
	}
	else
	{
		target_head_rot = cur_root_rot;
	}

	LLQuaternion head_rot_local = target_head_rot * cur_inv_root_rot;
	head_rot_local.constrain(HEAD_ROTATION_CONSTRAINT);

	// Set final torso rotation and torso target rotation such that it lags
	// behind the head rotation by a fixed amount.
	LLQuaternion torso_rot_local = nlerp(TORSO_LAG, LLQuaternion::DEFAULT,
										 head_rot_local);
	mTorsoState->setRotation(nlerp(torso_slerp_amt, mTorsoState->getRotation(),
								   torso_rot_local));

	head_rot_local = nlerp(head_slerp_amt, mLastHeadRot, head_rot_local);
	mLastHeadRot = head_rot_local;

	// Set the head rotation.
	if (mNeckState->getJoint() && mNeckState->getJoint()->getParent())
	{
		LLQuaternion torsoRotLocal =
			mNeckState->getJoint()->getParent()->getWorldRotation() *
			cur_inv_root_rot;
		head_rot_local = head_rot_local * ~torsoRotLocal;
		mNeckState->setRotation(nlerp(NECK_LAG, LLQuaternion::DEFAULT,
									  head_rot_local));
		mHeadState->setRotation(nlerp(1.f - NECK_LAG, LLQuaternion::DEFAULT,
									  head_rot_local));
	}

	return true;
}

//-----------------------------------------------------------------------------
// LLEyeMotion() class
//-----------------------------------------------------------------------------

LLEyeMotion::LLEyeMotion(const LLUUID& id)
:	LLMotion(id)
{
	mCharacter = NULL;
	mEyeJitterTime = 0.f;
	mEyeJitterYaw = 0.f;
	mEyeJitterPitch = 0.f;

	mEyeLookAwayTime = 0.f;
	mEyeLookAwayYaw = 0.f;
	mEyeLookAwayPitch = 0.f;

	mEyeBlinkTime = 0.f;
	mEyesClosed = false;

	mHeadJoint = NULL;

	mName = "eye_rot";

	mLeftEyeState = new LLJointState;
	mAltLeftEyeState = new LLJointState;

	mRightEyeState = new LLJointState;
	mAltRightEyeState = new LLJointState;
}

LLMotion::LLMotionInitStatus LLEyeMotion::onInitialize(LLCharacter* character)
{
	if (!character)
	{
		return STATUS_FAILURE;
	}
	mCharacter = character;

	mHeadJoint = character->getJoint(LL_JOINT_KEY_HEAD);
	if (!mHeadJoint)
	{
		llinfos << getName() << ": cannot get head joint." << llendl;
		return STATUS_FAILURE;
	}

	mLeftEyeState->setJoint(character->getJoint(LL_JOINT_KEY_EYELEFT));
	if (!mLeftEyeState->getJoint())
	{
		llinfos << getName() << ": cannot get left eyeball joint." << llendl;
		return STATUS_FAILURE;
	}

	mAltLeftEyeState->setJoint(character->getJoint(LL_JOINT_KEY_EYEALTLEFT));
	if (!mLeftEyeState->getJoint())
	{
		llinfos << getName() << ": cannot get alt left eyeball joint."
				<< llendl;
		return STATUS_FAILURE;
	}

	mRightEyeState->setJoint(character->getJoint(LL_JOINT_KEY_EYERIGHT));
	if (!mRightEyeState->getJoint())
	{
		llinfos << getName() << ": cannot set right eyeball joint." << llendl;
		return STATUS_FAILURE;
	}

	mAltRightEyeState->setJoint(character->getJoint(LL_JOINT_KEY_EYEALTRIGHT));
	if (!mRightEyeState->getJoint())
	{
		llinfos << getName() << ": cannot get alt right eyeball joint."
				<< llendl;
		return STATUS_FAILURE;
	}

	mLeftEyeState->setUsage(LLJointState::ROT);
	mAltLeftEyeState->setUsage(LLJointState::ROT);

	mRightEyeState->setUsage(LLJointState::ROT);
	mAltRightEyeState->setUsage(LLJointState::ROT);

	addJointState(mLeftEyeState);
	addJointState(mAltLeftEyeState);

	addJointState(mRightEyeState);
	addJointState(mAltRightEyeState);

	return STATUS_SUCCESS;
}

void LLEyeMotion::adjustEyeTarget(LLVector3* target_pos,
								  LLJointState& left_eye_state,
								  LLJointState& right_eye_state)
{
	// Compute eye rotation.
	bool has_eye_target = false;
	LLQuaternion target_eye_rot;
	LLVector3 eye_look_at;
	F32 vergence;

	if (target_pos)
	{
		LLVector3 skyward(0.f, 0.f, 1.f);
		LLVector3 left, up;

		eye_look_at = *target_pos;
		has_eye_target = true;
		F32 look_at_dist = eye_look_at.normalize();

		left.set(skyward % eye_look_at);
		up.set(eye_look_at % left);

		target_eye_rot = LLQuaternion(eye_look_at, left, up);
		// Convert target rotation to head-local coordinates
		target_eye_rot *= ~mHeadJoint->getWorldRotation();
		// Eliminate any Euler roll - we are lucky that roll is applied last.
		F32 roll, pitch, yaw;
		target_eye_rot.getEulerAngles(&roll, &pitch, &yaw);
		target_eye_rot.setEulerAngles(0.f, pitch, yaw);
		// constrain target orientation to be in front of avatar's face
		target_eye_rot.constrain(EYE_ROT_LIMIT_ANGLE);

		// Calculate vergence
		F32 interocular_dist = (left_eye_state.getJoint()->getWorldPosition() -
								right_eye_state.getJoint()->getWorldPosition()).length();
		vergence = -atan2f((interocular_dist / 2.f), look_at_dist);
		llclamp(vergence, -F_PI_BY_TWO, 0.f);
	}
	else
	{
		target_eye_rot = LLQuaternion::DEFAULT;
		vergence = 0.f;
	}

	// RN: subtract 4 degrees to account for foveal angular offset relative to
	// pupil
	vergence += 4.f * DEG_TO_RAD;

	// calculate eye jitter
	LLQuaternion eye_jitter_rot;

	// vergence not too high...
	if (vergence > -0.05f)
	{
		//...go ahead and jitter
		eye_jitter_rot.setEulerAngles(0.f, mEyeJitterPitch + mEyeLookAwayPitch,
									  mEyeJitterYaw + mEyeLookAwayYaw);
	}
	else
	{
		//...or don't
		eye_jitter_rot.loadIdentity();
	}

	// calculate vergence of eyes as an object gets closer to the avatar's head
	LLQuaternion vergence_quat;

	if (has_eye_target)
	{
		vergence_quat.setAngleAxis(vergence, LLVector3(0.f, 0.f, 1.f));
	}
	else
	{
		vergence_quat.loadIdentity();
	}

	// calculate eye rotations
	LLQuaternion left_eye_rot = target_eye_rot;
	left_eye_rot = vergence_quat * eye_jitter_rot * left_eye_rot;

	LLQuaternion right_eye_rot = target_eye_rot;
	vergence_quat.transpose();
	right_eye_rot = vergence_quat * eye_jitter_rot * right_eye_rot;

	left_eye_state.setRotation(left_eye_rot);
	right_eye_state.setRotation(right_eye_rot);
}

bool LLEyeMotion::onUpdate(F32 time, U8* joint_mask)
{
	// Calculate jitter
	if (mEyeJitterTimer.getElapsedTimeF32() > mEyeJitterTime)
	{
		mEyeJitterTime = EYE_JITTER_MIN_TIME +
						 ll_frand(EYE_JITTER_MAX_TIME - EYE_JITTER_MIN_TIME);
		mEyeJitterYaw = (ll_frand(2.f) - 1.f) * EYE_JITTER_MAX_YAW;
		mEyeJitterPitch = (ll_frand(2.f) - 1.f) * EYE_JITTER_MAX_PITCH;
		// Make sure lookaway time count gets updated, because we are resetting
		// the timer
		mEyeLookAwayTime -= llmax(0.f, mEyeJitterTimer.getElapsedTimeF32());
		mEyeJitterTimer.reset();
	}
	else if (mEyeJitterTimer.getElapsedTimeF32() > mEyeLookAwayTime)
	{
		if (ll_frand() > 0.1f)
		{
			// Blink while moving eyes some percentage of the time
			mEyeBlinkTime = mEyeBlinkTimer.getElapsedTimeF32();
		}
		if (mEyeLookAwayYaw == 0.f && mEyeLookAwayPitch == 0.f)
		{
			mEyeLookAwayYaw = (ll_frand(2.f) - 1.f) * EYE_LOOK_AWAY_MAX_YAW;
			mEyeLookAwayPitch = (ll_frand(2.f) - 1.f) * EYE_LOOK_AWAY_MAX_PITCH;
			mEyeLookAwayTime = EYE_LOOK_BACK_MIN_TIME +
							   ll_frand(EYE_LOOK_BACK_MAX_TIME -
										EYE_LOOK_BACK_MIN_TIME);
		}
		else
		{
			mEyeLookAwayYaw = 0.f;
			mEyeLookAwayPitch = 0.f;
			mEyeLookAwayTime = EYE_LOOK_AWAY_MIN_TIME +
							   ll_frand(EYE_LOOK_AWAY_MAX_TIME -
										EYE_LOOK_AWAY_MIN_TIME);
		}
	}

	// Do blinking
	if (!mEyesClosed && mEyeBlinkTimer.getElapsedTimeF32() >= mEyeBlinkTime)
	{
		F32 leftEyeBlinkMorph = mEyeBlinkTimer.getElapsedTimeF32() - mEyeBlinkTime;
		F32 rightEyeBlinkMorph = leftEyeBlinkMorph - EYE_BLINK_TIME_DELTA;

		leftEyeBlinkMorph = llclamp(leftEyeBlinkMorph / EYE_BLINK_SPEED, 0.f, 1.f);
		rightEyeBlinkMorph = llclamp(rightEyeBlinkMorph / EYE_BLINK_SPEED, 0.f, 1.f);
		mCharacter->setVisualParamWeight("Blink_Left", leftEyeBlinkMorph);
		mCharacter->setVisualParamWeight("Blink_Right", rightEyeBlinkMorph);
		mCharacter->updateVisualParams();

		if (rightEyeBlinkMorph == 1.f)
		{
			mEyesClosed = true;
			mEyeBlinkTime = EYE_BLINK_CLOSE_TIME;
			mEyeBlinkTimer.reset();
		}
	}
	else if (mEyesClosed)
	{
		if (mEyeBlinkTimer.getElapsedTimeF32() >= mEyeBlinkTime)
		{
			F32 leftEyeBlinkMorph = mEyeBlinkTimer.getElapsedTimeF32() - mEyeBlinkTime;
			F32 rightEyeBlinkMorph = leftEyeBlinkMorph - EYE_BLINK_TIME_DELTA;

			leftEyeBlinkMorph = 1.f - llclamp(leftEyeBlinkMorph / EYE_BLINK_SPEED, 0.f, 1.f);
			rightEyeBlinkMorph = 1.f - llclamp(rightEyeBlinkMorph / EYE_BLINK_SPEED, 0.f, 1.f);
			mCharacter->setVisualParamWeight("Blink_Left", leftEyeBlinkMorph);
			mCharacter->setVisualParamWeight("Blink_Right", rightEyeBlinkMorph);
			mCharacter->updateVisualParams();

			if (rightEyeBlinkMorph == 0.f)
			{
				mEyesClosed = false;
				mEyeBlinkTime = EYE_BLINK_MIN_TIME +
								ll_frand(EYE_BLINK_MAX_TIME -
										 EYE_BLINK_MIN_TIME);
				mEyeBlinkTimer.reset();
			}
		}
	}

	LLVector3* target_pos =
		(LLVector3*)mCharacter->getAnimationData("LookAtPoint");
	adjustEyeTarget(target_pos, *mLeftEyeState, *mRightEyeState);
	adjustEyeTarget(target_pos, *mAltLeftEyeState, *mAltRightEyeState);

	return true;
}

void LLEyeMotion::onDeactivate()
{
	LLJoint* joint = mLeftEyeState->getJoint();
	if (joint)
	{
		joint->setRotation(LLQuaternion::DEFAULT);
	}

	joint = mAltLeftEyeState->getJoint();
	if (joint)
	{
		joint->setRotation(LLQuaternion::DEFAULT);
	}

	joint = mRightEyeState->getJoint();
	if (joint)
	{
		joint->setRotation(LLQuaternion::DEFAULT);
	}

	joint = mAltRightEyeState->getJoint();
	if (joint)
	{
		joint->setRotation(LLQuaternion::DEFAULT);
	}
}
