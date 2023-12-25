/**
 * @file lltargetingmotion.h
 * @brief Implementation of LLTargetingMotion class.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLTARGETINGMOTION_H
#define LL_LLTARGETINGMOTION_H

#include "llmotion.h"

#define TARGETING_EASEIN_DURATION	0.3f
#define TARGETING_EASEOUT_DURATION 0.5f
#define TARGETING_PRIORITY LLJoint::HIGH_PRIORITY
#define MIN_REQUIRED_PIXEL_AREA_TARGETING 1000.f;

class LLTargetingMotion final : public LLMotion
{
protected:
	LOG_CLASS(LLTargetingMotion);

public:
	LLTargetingMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)		{ return new LLTargetingMotion(id); }

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override						{ return true; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override					{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override				{ return TARGETING_EASEIN_DURATION; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override				{ return TARGETING_EASEOUT_DURATION; }

	// Motions must report their priority
	LL_INLINE LLJoint::JointPriority getPriority() override	{ return TARGETING_PRIORITY; }

	LL_INLINE LLMotionBlendType getBlendType() override		{ return ADDITIVE_BLEND; }

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage
	LL_INLINE F32 getMinPixelArea() override				{ return MIN_REQUIRED_PIXEL_AREA_TARGETING; }

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

public:
	LLCharacter*			mCharacter;
	LLPointer<LLJointState>	mTorsoState;
	LLJoint*				mPelvisJoint;
	LLJoint*				mTorsoJoint;
	LLJoint*				mRightHandJoint;
};

#endif // LL_LLTARGETINGMOTION_H

