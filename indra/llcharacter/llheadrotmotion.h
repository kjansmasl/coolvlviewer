/**
 * @file llheadrotmotion.h
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

#ifndef LL_LLHEADROTMOTION_H
#define LL_LLHEADROTMOTION_H

#include "llframetimer.h"
#include "llmotion.h"

#define MIN_REQUIRED_PIXEL_AREA_HEAD_ROT 500.f;
#define MIN_REQUIRED_PIXEL_AREA_EYE 25000.f;

class LLHeadRotMotion final : public LLMotion
{
protected:
	LOG_CLASS(LLHeadRotMotion);

public:
	LLHeadRotMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)		{ return new LLHeadRotMotion(id); }

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override						{ return true; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override					{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override				{ return 1.f; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override				{ return 1.f; }

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage
	LL_INLINE F32 getMinPixelArea() override				{ return MIN_REQUIRED_PIXEL_AREA_HEAD_ROT; }

	// Motions must report their priority
	LL_INLINE LLJoint::JointPriority getPriority() override	{ return LLJoint::MEDIUM_PRIORITY; }

	LL_INLINE LLMotionBlendType getBlendType() override		{ return NORMAL_BLEND; }

	// Run-time (post constructor) initialization, called after parameters have
	// been set. Must return true to indicate success and be available for
	// activation.
	LLMotionInitStatus onInitialize(LLCharacter* character) override;

	// Called when a motion is activated. Must return true to indicate success,
	// or else it will be deactivated.
	LL_INLINE bool onActivate() override					{ return true; }

	// Called per time step. Must return true while it is active, and must
	// return false when the motion is completed.
	bool onUpdate(F32 time, U8* joint_mask) override;

	// Called when a motion is deactivated
	LL_INLINE void onDeactivate() override					{}

	// Expose enabled status so the effects of this motion can be turned on/off
	// independently of its active state.
	LL_INLINE void enable() override						{ mEnabled = true; }
	LL_INLINE void disable() override						{ mEnabled = false; }
	LL_INLINE bool isEnabled() const override				{ return mEnabled; }

public:
	LLCharacter*			mCharacter;

	LLJoint*				mTorsoJoint;
	LLJoint*				mHeadJoint;
	LLJoint*				mRootJoint;
	LLJoint*				mPelvisJoint;

	// Joint states to be animated
	LLPointer<LLJointState>	mTorsoState;
	LLPointer<LLJointState>	mNeckState;
	LLPointer<LLJointState>	mHeadState;

	LLQuaternion			mLastHeadRot;

	bool					mEnabled;
};

class LLEyeMotion final : public LLMotion
{
protected:
	LOG_CLASS(LLEyeMotion);

public:
	LLEyeMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)		{ return new LLEyeMotion(id); }

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override						{ return true; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override					{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override				{ return 0.5f; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override				{ return 0.5f; }

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage
	LL_INLINE F32 getMinPixelArea() override				{ return MIN_REQUIRED_PIXEL_AREA_EYE; }

	// Motions must report their priority
	LL_INLINE LLJoint::JointPriority getPriority() override	{ return LLJoint::MEDIUM_PRIORITY; }

	LL_INLINE LLMotionBlendType getBlendType() override		{ return NORMAL_BLEND; }

	// Run-time (post constructor) initialization, called after parameters have
	// been set. Must return true to indicate success and be available for
	// activation.
	LLMotionInitStatus onInitialize(LLCharacter* character) override;

	// Called when a motion is activated. Must return true to indicate success,
	// or else it will be deactivated.
	LL_INLINE bool onActivate() override					{ return true; }

	void adjustEyeTarget(LLVector3* target_pos, LLJointState& left_eye_state,
						 LLJointState& right_eye_state);

	// Called per time step must return true while it is active, and must
	// return false when the motion is completed.
	bool onUpdate(F32 time, U8* joint_mask) override;

	// Called when a motion is deactivated
	void onDeactivate() override;

public:
	LLCharacter*			mCharacter;

	LLJoint*				mHeadJoint;

	// Joint states to be animated
	LLPointer<LLJointState>	mLeftEyeState;
	LLPointer<LLJointState>	mAltLeftEyeState;
	LLPointer<LLJointState>	mRightEyeState;
	LLPointer<LLJointState>	mAltRightEyeState;

	LLFrameTimer			mEyeJitterTimer;
	F32						mEyeJitterTime;
	F32						mEyeJitterYaw;
	F32						mEyeJitterPitch;
	F32						mEyeLookAwayTime;
	F32						mEyeLookAwayYaw;
	F32						mEyeLookAwayPitch;

	// Eye blinking
	LLFrameTimer			mEyeBlinkTimer;
	F32						mEyeBlinkTime;
	bool					mEyesClosed;
};

#endif // LL_LLHEADROTMOTION_H
