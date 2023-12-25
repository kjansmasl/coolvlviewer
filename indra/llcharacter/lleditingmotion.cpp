/**
 * @file lleditingmotion.cpp
 * @brief Implementation of LLEditingMotion class.
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

#include "lleditingmotion.h"

#include "llcharacter.h"
#include "llcriticaldamp.h"
#include "llhandmotion.h"

constexpr F32 TARGET_LAG_HALF_LIFE = 0.1f;	// Half-life of IK targeting

S32 LLEditingMotion::sHandPose = LLHandMotion::HAND_POSE_RELAXED_R;
S32 LLEditingMotion::sHandPosePriority = 3;

LLEditingMotion::LLEditingMotion(const LLUUID& id)
:	LLMotion(id),
	mCharacter(NULL),
	mParentState(new LLJointState),
	mShoulderState(new LLJointState),
	mElbowState(new LLJointState),
	mWristState(new LLJointState),
	mTorsoState(new LLJointState)
{
	mName = "editing";

	// Create kinematic chain
	mParentJoint.addChild(&mShoulderJoint);
	mShoulderJoint.addChild(&mElbowJoint);
	mElbowJoint.addChild(&mWristJoint);
}

LLMotion::LLMotionInitStatus LLEditingMotion::onInitialize(LLCharacter* character)
{
	mCharacter = character;	// Save character for future use

	LLJoint* shoulder_left = mCharacter->getJoint(LL_JOINT_KEY_SHOULDERLEFT);
	LLJoint* elbow_left = mCharacter->getJoint(LL_JOINT_KEY_ELBOWLEFT);
	LLJoint* wrist_left = mCharacter->getJoint(LL_JOINT_KEY_WRISTLEFT);

	// Make sure character skeleton is copacetic
	if (!shoulder_left || !elbow_left || !wrist_left)
	{
		llwarns << "Invalid skeleton for editing motion !" << llendl;
		return STATUS_FAILURE;
	}

	// Get the shoulder, elbow, wrist joints from the character
	mParentState->setJoint(shoulder_left->getParent());
	mShoulderState->setJoint(shoulder_left);
	mElbowState->setJoint(elbow_left);
	mWristState->setJoint(wrist_left);
	mTorsoState->setJoint(mCharacter->getJoint(LL_JOINT_KEY_TORSO));

	if (!mParentState->getJoint())
	{
		llinfos << getName() << ": Can't get parent joint." << llendl;
		return STATUS_FAILURE;
	}

	mWristOffset = LLVector3(0.f, 0.2f, 0.f);

	// Add joint states to the pose
	mShoulderState->setUsage(LLJointState::ROT);
	mElbowState->setUsage(LLJointState::ROT);
	mTorsoState->setUsage(LLJointState::ROT);
	mWristState->setUsage(LLJointState::ROT);
	addJointState(mShoulderState);
	addJointState(mElbowState);
	addJointState(mTorsoState);
	addJointState(mWristState);

	// Propagate joint positions to kinematic chain
	mParentJoint.setPosition(mParentState->getJoint()->getWorldPosition());
	mShoulderJoint.setPosition(mShoulderState->getJoint()->getPosition());
	mElbowJoint.setPosition(mElbowState->getJoint()->getPosition());
	mWristJoint.setPosition(mWristState->getJoint()->getPosition() +
							mWristOffset);

	// Propagate current joint rotations to kinematic chain
	mParentJoint.setRotation(mParentState->getJoint()->getWorldRotation());
	mShoulderJoint.setRotation(mShoulderState->getJoint()->getRotation());
	mElbowJoint.setRotation(mElbowState->getJoint()->getRotation());

	// Connect the ikSolver to the chain
	mIKSolver.setPoleVector(LLVector3(-1.f, 1.f, 0.f));
	// Specifying the elbow's axis will prevent bad IK for the more
	// singular configurations, but the axis is limb-specific -- Leviathan
	mIKSolver.setBAxis(LLVector3(-0.682683f, 0.f, -0.730714f));
	mIKSolver.setupJoints(&mShoulderJoint, &mElbowJoint, &mWristJoint, &mTarget);

	return STATUS_SUCCESS;
}

bool LLEditingMotion::onActivate()
{
	// Propagate joint positions to kinematic chain
	mParentJoint.setPosition(mParentState->getJoint()->getWorldPosition());
	mShoulderJoint.setPosition(mShoulderState->getJoint()->getPosition());
	mElbowJoint.setPosition(mElbowState->getJoint()->getPosition());
	mWristJoint.setPosition(mWristState->getJoint()->getPosition() +
							mWristOffset);

	// Propagate current joint rotations to kinematic chain
	mParentJoint.setRotation(mParentState->getJoint()->getWorldRotation());
	mShoulderJoint.setRotation(mShoulderState->getJoint()->getRotation());
	mElbowJoint.setRotation(mElbowState->getJoint()->getRotation());

	return true;
}

bool LLEditingMotion::onUpdate(F32, U8* joint_mask)
{
	bool result = true;

	LLVector3 focus_pt;
	static const std::string point_at_point = "PointAtPoint";
	LLVector3* point_at_pt =
		(LLVector3*)mCharacter->getAnimationData(point_at_point);
	if (!point_at_pt)
	{
		focus_pt = mLastSelectPt;
		result = false;
	}
	else
	{
		focus_pt = *point_at_pt;
		mLastSelectPt = focus_pt;
	}

	focus_pt += mCharacter->getCharacterPosition();

	// Propagate joint positions to kinematic chain
	mParentJoint.setPosition(mParentState->getJoint()->getWorldPosition());
	mShoulderJoint.setPosition(mShoulderState->getJoint()->getPosition());
	mElbowJoint.setPosition(mElbowState->getJoint()->getPosition());
	mWristJoint.setPosition(mWristState->getJoint()->getPosition() +
							mWristOffset);

	// Propagate current joint rotations to kinematic chain
	mParentJoint.setRotation(mParentState->getJoint()->getWorldRotation());
	mShoulderJoint.setRotation(mShoulderState->getJoint()->getRotation());
	mElbowJoint.setRotation(mElbowState->getJoint()->getRotation());

	// Update target position from character
	LLVector3 target = focus_pt - mParentJoint.getPosition();
	F32 target_dist = target.normalize();

	LLVector3 edit_plane_normal(1.f / F_SQRT2, 1.f / F_SQRT2, 0.f);
	edit_plane_normal.normalize();

	edit_plane_normal.rotVec(mTorsoState->getJoint()->getWorldRotation());

	F32 dot = edit_plane_normal * target;

	if (dot < 0.f)
	{
		target = target + (edit_plane_normal * (dot * 2.f));
		target.mV[VZ] += clamp_rescale(dot, 0.f, -1.f, 0.f, 5.f);
		target.normalize();
	}

	target = target * target_dist;
	if (!target.isFinite())
	{
		llwarns << "Non finite target in editing motion with target distance of "
				<< target_dist << " and focus point " << focus_pt << llendl;
		target.set(1.f, 1.f, 1.f);
	}

	mTarget.setPosition(target + mParentJoint.getPosition());

#if 0
	llinfos << "Point at: " << mTarget.getPosition() << llendl;
#endif

	// Update the ikSolver
	if (!mTarget.getPosition().isExactlyZero())
	{
		LLQuaternion shoulder_rot = mShoulderJoint.getRotation();
		LLQuaternion elbow_rot = mElbowJoint.getRotation();
		mIKSolver.solve();

		// Use blending...
		F32 slerp_amt = LLCriticalDamp::getInterpolant(TARGET_LAG_HALF_LIFE);
		shoulder_rot = slerp(slerp_amt, mShoulderJoint.getRotation(),
							 shoulder_rot);
		elbow_rot = slerp(slerp_amt, mElbowJoint.getRotation(), elbow_rot);

		// Now put blended values back into joints
		llassert(shoulder_rot.isFinite());
		llassert(elbow_rot.isFinite());
		mShoulderState->setRotation(shoulder_rot);
		mElbowState->setRotation(elbow_rot);
		mWristState->setRotation(LLQuaternion::DEFAULT);
	}

	static const std::string hand_pose = "Hand Pose";
	static const std::string hand_pose_priority = "Hand Pose Priority";
	mCharacter->setAnimationData(hand_pose, &sHandPose);
	mCharacter->setAnimationData(hand_pose_priority, &sHandPosePriority);

	return result;
}
