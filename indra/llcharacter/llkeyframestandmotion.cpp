/**
 * @file llkeyframestandmotion.cpp
 * @brief Implementation of LLKeyframeStandMotion class.
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

#include "llkeyframestandmotion.h"

#include "llcharacter.h"

#define GO_TO_KEY_POSE	1
#define MIN_TRACK_SPEED 0.01f

constexpr F32 ROTATION_THRESHOLD = 0.6f;
constexpr F32 POSITION_THRESHOLD = 0.1f;

LLKeyframeStandMotion::LLKeyframeStandMotion(const LLUUID& id)
:	LLKeyframeMotion(id),
	mCharacter(NULL),
	mPelvisState(NULL),
	mHipLeftState(NULL),
	mKneeLeftState(NULL),
	mAnkleLeftState(NULL),
	mHipRightState(NULL),
	mKneeRightState(NULL),
	mAnkleRightState(NULL),
	mFrameNum(0),
	mFlipFeet(false),
	mTrackAnkles(true)
{
	// Create kinematic hierarchy
	mPelvisJoint.addChild(&mHipLeftJoint);
	mHipLeftJoint.addChild(&mKneeLeftJoint);
	mKneeLeftJoint.addChild(&mAnkleLeftJoint);
	mPelvisJoint.addChild(&mHipRightJoint);
	mHipRightJoint.addChild(&mKneeRightJoint);
	mKneeRightJoint.addChild(&mAnkleRightJoint);
}

LLMotion::LLMotionInitStatus LLKeyframeStandMotion::onInitialize(LLCharacter* character)
{
	// Save character pointer for later use
	mCharacter = character;

	mFlipFeet = false;

	// Load keyframe data, setup pose and joint states
	LLMotion::LLMotionInitStatus status =
		LLKeyframeMotion::onInitialize(character);
	if (status == STATUS_FAILURE)
	{
		return status;
	}

	// Find the necessary joint states
	LLPose* pose = getPose();
	mPelvisState = pose->findJointState(LL_JOINT_KEY_PELVIS);

	mHipLeftState = pose->findJointState(LL_JOINT_KEY_HIPLEFT);
	mKneeLeftState = pose->findJointState(LL_JOINT_KEY_KNEELEFT);
	mAnkleLeftState = pose->findJointState(LL_JOINT_KEY_ANKLELEFT);

	mHipRightState = pose->findJointState(LL_JOINT_KEY_HIPRIGHT);
	mKneeRightState = pose->findJointState(LL_JOINT_KEY_KNEERIGHT);
	mAnkleRightState = pose->findJointState(LL_JOINT_KEY_ANKLERIGHT);

	if (!mPelvisState || !mHipLeftState || !mKneeLeftState ||
		!mAnkleLeftState || !mHipRightState || !mKneeRightState ||
		!mAnkleRightState)
	{
		llinfos << getName() << ": cannot find necessary joint states."
				<< llendl;
		return STATUS_FAILURE;
	}

	return STATUS_SUCCESS;
}

bool LLKeyframeStandMotion::onActivate()
{
	// Setup the IK solvers
	mIKLeft.setPoleVector(LLVector3(1.f, 0.f, 0.f));
	mIKRight.setPoleVector(LLVector3(1.f, 0.f, 0.f));
	mIKLeft.setBAxis(LLVector3(0.05f, 1.f, 0.f));
	mIKRight.setBAxis(LLVector3(-0.05f, 1.f, 0.f));

	mLastGoodPelvisRotation.loadIdentity();
	mLastGoodPosition.clear();

	mFrameNum = 0;

	return LLKeyframeMotion::onActivate();
}

void LLKeyframeStandMotion::onDeactivate()
{
	LLKeyframeMotion::onDeactivate();
}

bool LLKeyframeStandMotion::onUpdate(F32 time, U8* joint_mask)
{
	// Let the base class update the cycle
	bool status = LLKeyframeMotion::onUpdate(time, joint_mask);
	if (!status)
	{
		return false;
	}

	LLJoint* pelvisp = mPelvisState->getJoint();
	if (!pelvisp)
	{
		// Something is wrong. Pretend update is done and abort !
		return true;
	}

	LLJoint* parentp = pelvisp->getParent();
	if (!parentp)
	{
		// Something is wrong. Pretend update is done and abort !
		return true;
	}

	LLVector3 root_world_pos = parentp->getWorldPosition();
	// Have we received a valid world position for this avatar ?
	if (root_world_pos.isExactlyZero())
	{
		return true;
	}

	// Stop tracking (start locking) ankles once ease in is done.
	// Setting this here ensures we track until we get valid foot position.
	if (dot(pelvisp->getWorldRotation(),
			mLastGoodPelvisRotation) < ROTATION_THRESHOLD)
	{
		mLastGoodPelvisRotation = pelvisp->getWorldRotation();
		mLastGoodPelvisRotation.normalize();
		mTrackAnkles = true;
	}
	else if ((mCharacter->getCharacterPosition() -
			  mLastGoodPosition).lengthSquared() > POSITION_THRESHOLD)
	{
		mLastGoodPosition = mCharacter->getCharacterPosition();
		mTrackAnkles = true;
	}
	else if (mPose.getWeight() < 1.f)
	{
		mTrackAnkles = true;
	}

	// Propagate joint positions to internal versions

	mPelvisJoint.setPosition(root_world_pos + mPelvisState->getPosition());

	mHipLeftJoint.setPosition(mHipLeftState->getJoint()->getPosition());
	mKneeLeftJoint.setPosition(mKneeLeftState->getJoint()->getPosition());
	mAnkleLeftJoint.setPosition(mAnkleLeftState->getJoint()->getPosition());

	mHipLeftJoint.setScale(mHipLeftState->getJoint()->getScale());
	mKneeLeftJoint.setScale(mKneeLeftState->getJoint()->getScale());
	mAnkleLeftJoint.setScale(mAnkleLeftState->getJoint()->getScale());

	mHipRightJoint.setPosition(mHipRightState->getJoint()->getPosition());
	mKneeRightJoint.setPosition(mKneeRightState->getJoint()->getPosition());
	mAnkleRightJoint.setPosition(mAnkleRightState->getJoint()->getPosition());

	mHipRightJoint.setScale(mHipRightState->getJoint()->getScale());
	mKneeRightJoint.setScale(mKneeRightState->getJoint()->getScale());
	mAnkleRightJoint.setScale(mAnkleRightState->getJoint()->getScale());

	// Propagate joint rotations to internal versions

	mPelvisJoint.setRotation(pelvisp->getWorldRotation());

#if GO_TO_KEY_POSE
	mHipLeftJoint.setRotation(mHipLeftState->getRotation());
	mKneeLeftJoint.setRotation(mKneeLeftState->getRotation());
	mAnkleLeftJoint.setRotation(mAnkleLeftState->getRotation());

	mHipRightJoint.setRotation(mHipRightState->getRotation());
	mKneeRightJoint.setRotation(mKneeRightState->getRotation());
	mAnkleRightJoint.setRotation(mAnkleRightState->getRotation());
#else
	mHipLeftJoint.setRotation(mHipLeftState->getJoint()->getRotation());
	mKneeLeftJoint.setRotation(mKneeLeftState->getJoint()->getRotation());
	mAnkleLeftJoint.setRotation(mAnkleLeftState->getJoint()->getRotation());

	mHipRightJoint.setRotation(mHipRightState->getJoint()->getRotation());
	mKneeRightJoint.setRotation(mKneeRightState->getJoint()->getRotation());
	mAnkleRightJoint.setRotation(mAnkleRightState->getJoint()->getRotation());
#endif

	// Need to wait for underlying keyframe motion to affect the skeleton
	if (mFrameNum == 2)
	{
		mIKLeft.setupJoints(&mHipLeftJoint, &mKneeLeftJoint,
							&mAnkleLeftJoint, &mTargetLeft);
		mIKRight.setupJoints(&mHipRightJoint, &mKneeRightJoint,
							 &mAnkleRightJoint, &mTargetRight);
	}
	else if (mFrameNum < 2)
	{
		++mFrameNum;
		return true;
	}

	++mFrameNum;

	// Compute target position by projecting ankles to the ground
	if (mTrackAnkles)
	{
		mCharacter->getGround(mAnkleLeftJoint.getWorldPosition(),
							  mPositionLeft, mNormalLeft);
		mCharacter->getGround(mAnkleRightJoint.getWorldPosition(),
							  mPositionRight, mNormalRight);

		mTargetLeft.setPosition(mPositionLeft);
		mTargetRight.setPosition(mPositionRight);
	}

	// Update solvers
	mIKLeft.solve();
	mIKRight.solve();

	// Make ankle rotation conform to the ground
	if (mTrackAnkles)
	{
		LLVector4 dir_left4 = mAnkleLeftJoint.getWorldMatrix().getFwdRow4();
		LLVector4 dir_right4 = mAnkleRightJoint.getWorldMatrix().getFwdRow4();
		LLVector3 dir_left = vec4to3(dir_left4);
		LLVector3 dir_right = vec4to3(dir_right4);

		LLVector3 up;
		LLVector3 dir;
		LLVector3 left;

		up = mNormalLeft;
		up.normalize();
		if (mFlipFeet)
		{
			up *= -1.f;
		}
		dir = dir_left;
		dir.normalize();
		left = up % dir;
		left.normalize();
		dir = left % up;
		mRotationLeft = LLQuaternion(dir, left, up);

		up = mNormalRight;
		up.normalize();
		if (mFlipFeet)
		{
			up *= -1.f;
		}
		dir = dir_right;
		dir.normalize();
		left = up % dir;
		left.normalize();
		dir = left % up;
		mRotationRight = LLQuaternion(dir, left, up);
	}
	mAnkleLeftJoint.setWorldRotation(mRotationLeft);
	mAnkleRightJoint.setWorldRotation(mRotationRight);

	// Propagate joint rotations to joint states

	mHipLeftState->setRotation(mHipLeftJoint.getRotation());
	mKneeLeftState->setRotation(mKneeLeftJoint.getRotation());
	mAnkleLeftState->setRotation(mAnkleLeftJoint.getRotation());

	mHipRightState->setRotation(mHipRightJoint.getRotation());
	mKneeRightState->setRotation(mKneeRightJoint.getRotation());
	mAnkleRightState->setRotation(mAnkleRightJoint.getRotation());

	return true;
}
