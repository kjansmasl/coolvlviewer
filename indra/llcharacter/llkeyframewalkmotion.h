/**
 * @file llkeyframewalkmotion.h
 * @brief Implementation of LLKeframeWalkMotion class.
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

#ifndef LL_LLKEYFRAMEWALKMOTION_H
#define LL_LLKEYFRAMEWALKMOTION_H

#include "llcharacter.h"
#include "llkeyframemotion.h"
#include "llpreprocessor.h"
#include "llvector3d.h"

#define MIN_REQUIRED_PIXEL_AREA_WALK_ADJUST (20.f)
#define MIN_REQUIRED_PIXEL_AREA_FLY_ADJUST (20.f)

class LLKeyframeWalkMotion final : public LLKeyframeMotion
{
	friend class LLWalkAdjustMotion;

protected:
	LOG_CLASS(LLKeyframeWalkMotion);

public:
	LLKeyframeWalkMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)
	{
		return new LLKeyframeWalkMotion(id);
	}

	LLMotionInitStatus onInitialize(LLCharacter* character) override;
	bool onActivate() override;
	void onDeactivate() override;
	bool onUpdate(F32 time, U8* joint_mask) override;

public:
	LLCharacter*	mCharacter;
	F32				mRealTimeLast;
	F32				mAdjTimeLast;
};

class LLWalkAdjustMotion final : public LLMotion
{
protected:
	LOG_CLASS(LLWalkAdjustMotion);

public:
	LLWalkAdjustMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)		{ return new LLWalkAdjustMotion(id); }

public:
	LLMotionInitStatus onInitialize(LLCharacter* character) override;
	bool onActivate() override;
	void onDeactivate() override;
	bool onUpdate(F32 time, U8* joint_mask) override;

	LL_INLINE LLJoint::JointPriority getPriority() override	{ return LLJoint::HIGH_PRIORITY; }
	LL_INLINE bool getLoop() override						{ return true; }
	LL_INLINE F32 getDuration() override					{ return 0.f; }
	LL_INLINE F32 getEaseInDuration() override				{ return 0.f; }
	LL_INLINE F32 getEaseOutDuration() override				{ return 0.f; }
	LL_INLINE F32 getMinPixelArea() override				{ return MIN_REQUIRED_PIXEL_AREA_WALK_ADJUST; }
	LL_INLINE LLMotionBlendType getBlendType() override		{ return ADDITIVE_BLEND; }

public:
	LLCharacter*			mCharacter;
	LLJoint*				mLeftAnkleJoint;
	LLJoint*				mRightAnkleJoint;
	LLPointer<LLJointState>	mPelvisState;
	LLJoint*				mPelvisJoint;
	LLVector3d				mLastLeftAnklePos;
	LLVector3d				mLastRightAnklePos;
	F32						mLastTime;
	F32						mAvgCorrection;
	F32						mSpeedAdjust;
	F32						mAnimSpeed;
	F32						mAvgSpeed;
	F32						mRelativeDir;
	LLVector3				mPelvisOffset;
	F32						mAnkleOffset;
};

class LLFlyAdjustMotion final : public LLMotion
{
protected:
	LOG_CLASS(LLFlyAdjustMotion);

public:
	LLFlyAdjustMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)		{ return new LLFlyAdjustMotion(id); }

public:
	LLMotionInitStatus onInitialize(LLCharacter* character) override;
	bool onActivate() override;
	LL_INLINE void onDeactivate() override					{}
	bool onUpdate(F32 time, U8* joint_mask) override;

	LL_INLINE LLJoint::JointPriority getPriority() override	{ return LLJoint::HIGHER_PRIORITY; }
	LL_INLINE bool getLoop() override						{ return true; }
	LL_INLINE F32 getDuration() override					{ return 0.f; }
	LL_INLINE F32 getEaseInDuration() override				{ return 0.f; }
	LL_INLINE F32 getEaseOutDuration() override				{ return 0.f; }
	LL_INLINE F32 getMinPixelArea() override				{ return MIN_REQUIRED_PIXEL_AREA_FLY_ADJUST; }
	LL_INLINE LLMotionBlendType getBlendType() override		{ return ADDITIVE_BLEND; }

protected:
	LLCharacter*			mCharacter;
	LLPointer<LLJointState>	mPelvisState;
	F32						mRoll;
};

#endif // LL_LLKeyframeWalkMotion_H
