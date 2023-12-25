/**
 * @file llkeyframestandmotion.h
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

#ifndef LL_LLKEYFRAMESTANDMOTION_H
#define LL_LLKEYFRAMESTANDMOTION_H

#include "lljointsolverrp3.h"
#include "llkeyframemotion.h"

class LLKeyframeStandMotion final : public LLKeyframeMotion
{
protected:
	LOG_CLASS(LLKeyframeStandMotion);

public:
	LLKeyframeStandMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)
	{
		return new LLKeyframeStandMotion(id);
	}

	LLMotionInitStatus onInitialize(LLCharacter* character) override;
	bool onActivate() override;
	bool onUpdate(F32 time, U8* joint_mask) override;
	void onDeactivate() override;

public:
	LLJoint					mPelvisJoint;

	LLJoint					mHipLeftJoint;
	LLJoint					mKneeLeftJoint;
	LLJoint					mAnkleLeftJoint;
	LLJoint					mTargetLeft;

	LLJoint					mHipRightJoint;
	LLJoint					mKneeRightJoint;
	LLJoint					mAnkleRightJoint;
	LLJoint					mTargetRight;

	LLCharacter*			mCharacter;

	LLPointer<LLJointState>	mPelvisState;

	LLPointer<LLJointState>	mHipLeftState;
	LLPointer<LLJointState>	mKneeLeftState;
	LLPointer<LLJointState>	mAnkleLeftState;

	LLPointer<LLJointState>	mHipRightState;
	LLPointer<LLJointState>	mKneeRightState;
	LLPointer<LLJointState>	mAnkleRightState;

	LLJointSolverRP3		mIKLeft;
	LLJointSolverRP3		mIKRight;

	LLVector3				mPositionLeft;
	LLVector3				mPositionRight;
	LLVector3				mNormalLeft;
	LLVector3				mNormalRight;
	LLQuaternion			mRotationLeft;
	LLQuaternion			mRotationRight;

	LLQuaternion			mLastGoodPelvisRotation;
	LLVector3				mLastGoodPosition;

	S32						mFrameNum;

	bool					mTrackAnkles;
	bool					mFlipFeet;
};

#endif // LL_LLKEYFRAMESTANDMOTION_H
