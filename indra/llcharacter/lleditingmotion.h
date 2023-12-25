/**
 * @file lleditingmotion.h
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

#ifndef LL_LLEDITINGMOTION_H
#define LL_LLEDITINGMOTION_H

#include "lljointsolverrp3.h"
#include "llmotion.h"
#include "llvector3d.h"

#define EDITING_EASEIN_DURATION	0.0f
#define EDITING_EASEOUT_DURATION 0.5f
#define EDITING_PRIORITY LLJoint::HIGH_PRIORITY
#define MIN_REQUIRED_PIXEL_AREA_EDITING 500.f

class LLEditingMotion final : public LLMotion
{
protected:
	LOG_CLASS(LLEditingMotion);

public:
	LLEditingMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)		{ return new LLEditingMotion(id); }

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override						{ return true; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override					{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override				{ return EDITING_EASEIN_DURATION; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override				{ return EDITING_EASEOUT_DURATION; }

	// Motions must report their priority
	LL_INLINE LLJoint::JointPriority getPriority() override	{ return EDITING_PRIORITY; }

	LL_INLINE LLMotionBlendType getBlendType() override		{ return NORMAL_BLEND; }

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage.
	LL_INLINE F32 getMinPixelArea() override				{ return MIN_REQUIRED_PIXEL_AREA_EDITING; }

	// Run-time (post constructor) initialization, called after parameters have
	// been set. Must return true to indicate success and be available for
	// activation.
	LLMotionInitStatus onInitialize(LLCharacter* character) override;

	// Called when a motion is activated must return true to indicate success,
	// or else it will be deactivated.
	bool onActivate() override;

	// Called per time step. Must return true while it is active, and must
	// return false when the motion is completed.
	bool onUpdate(F32 time, U8* joint_mask) override;

	// Called when a motion is deactivated
	LL_INLINE void onDeactivate() override					{}

public:
	LLJoint					mParentJoint;
	LLJoint					mShoulderJoint;
	LLJoint					mElbowJoint;
	LLJoint					mWristJoint;
	LLJoint					mTarget;

	// Joint states to be animated
	LLPointer<LLJointState>	mParentState;
	LLPointer<LLJointState>	mShoulderState;
	LLPointer<LLJointState>	mElbowState;
	LLPointer<LLJointState>	mWristState;
	LLPointer<LLJointState>	mTorsoState;

	LLCharacter*			mCharacter;

	LLVector3				mWristOffset;
	LLVector3				mLastSelectPt;

	LLJointSolverRP3		mIKSolver;

	static S32				sHandPose;
	static S32				sHandPosePriority;
};

#endif // LL_LLKEYFRAMEMOTION_H
