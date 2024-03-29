/**
 * @file llhandmotion.h
 * @brief Implementation of LLHandMotion class.
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

#ifndef LL_LLHANDMOTION_H
#define LL_LLHANDMOTION_H

#include "llmotion.h"
#include "lltimer.h"

#define MIN_REQUIRED_PIXEL_AREA_HAND 10000.f;

class LLHandMotion final : public LLMotion
{
protected:
	LOG_CLASS(LLHandMotion);

public:
	typedef enum e_hand_pose
	{
		HAND_POSE_SPREAD,
		HAND_POSE_RELAXED,
		HAND_POSE_POINT,
		HAND_POSE_FIST,
		HAND_POSE_RELAXED_L,
		HAND_POSE_POINT_L,
		HAND_POSE_FIST_L,
		HAND_POSE_RELAXED_R,
		HAND_POSE_POINT_R,
		HAND_POSE_FIST_R,
		HAND_POSE_SALUTE_R,
		HAND_POSE_TYPING,
		HAND_POSE_PEACE_R,
		HAND_POSE_PALM_R,
		NUM_HAND_POSES
	} eHandPose;

	LLHandMotion(const LLUUID& id);

	LL_INLINE static LLMotion* create(const LLUUID& id)		{ return new LLHandMotion(id); }

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override						{ return true; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override					{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override				{ return 0.f; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override				{ return 0.f; }

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage
	LL_INLINE F32 getMinPixelArea() override				{ return MIN_REQUIRED_PIXEL_AREA_HAND; }

	// Motions must report their priority
	LL_INLINE LLJoint::JointPriority getPriority() override	{ return LLJoint::MEDIUM_PRIORITY; }

	LL_INLINE LLMotionBlendType getBlendType() override		{ return NORMAL_BLEND; }

	// Run-time (post constructor) initialization, called after parameters have
	// been set.
	LLMotionInitStatus onInitialize(LLCharacter* character) override
	{
		mCharacter = character;
		return STATUS_SUCCESS;
	}

	// Called when a motion is activated. Must return true to indicate success,
	// or else it will be deactivated.
	bool onActivate() override;

	// Called per time step. Must return true while it is active, and must
	// return false when the motion is completed.
	bool onUpdate(F32 time, U8* joint_mask) override;

	// Called when a motion is deactivated
	LL_INLINE void onDeactivate() override					{}

	LL_INLINE bool canDeprecate() override					{ return false; }

	static std::string getHandPoseName(eHandPose pose);
	static eHandPose getHandPose(const std::string& posename);

public:
	LLCharacter*	mCharacter;
	F32				mLastTime;
	eHandPose		mCurrentPose;
	eHandPose		mNewPose;
};

#endif // LL_LLHANDMOTION_H
