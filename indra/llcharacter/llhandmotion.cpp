/**
 * @file llhandmotion.cpp
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

#include "linden_common.h"

#include "llhandmotion.h"

#include "llcharacter.h"

// Constants

const char* gHandPoseNames[LLHandMotion::NUM_HAND_POSES] =
{
	"",
	"Hands_Relaxed",
	"Hands_Point",
	"Hands_Fist",
	"Hands_Relaxed_L",
	"Hands_Point_L",
	"Hands_Fist_L",
	"Hands_Relaxed_R",
	"Hands_Point_R",
	"Hands_Fist_R",
	"Hands_Salute_R",
	"Hands_Typing",
	"Hands_Peace_R",
	"Hands_Spread_R"
};

constexpr F32 HAND_MORPH_BLEND_TIME = 0.2f;

LLHandMotion::LLHandMotion(const LLUUID& id)
:	LLMotion(id),
	mCharacter(NULL),
	mLastTime(0.f),
	mCurrentPose(HAND_POSE_RELAXED),
	mNewPose(HAND_POSE_RELAXED)
{
	mName = "hand_motion";

	// RN: flag hand joint as highest priority for now, until we implement a
	// proper animation track
	mJointSignature[0][LL_HAND_JOINT_NUM] = 0xff;
	mJointSignature[1][LL_HAND_JOINT_NUM] = 0xff;
	mJointSignature[2][LL_HAND_JOINT_NUM] = 0xff;
}

//virtual
bool LLHandMotion::onActivate()
{
	LLPolyMesh* upperBodyMesh = mCharacter->getUpperBodyMesh();

	if (upperBodyMesh)
	{
		// Note: 0 is the default
		for (S32 i = 1; i < LLHandMotion::NUM_HAND_POSES; ++i)
		{
			mCharacter->setVisualParamWeight(gHandPoseNames[i], 0.f);
		}
		mCharacter->setVisualParamWeight(gHandPoseNames[mCurrentPose], 1.f);
		mCharacter->updateVisualParams();
	}
	return true;
}

//virtual
bool LLHandMotion::onUpdate(F32 time, U8* joint_mask)
{
	F32 time_delta = time - mLastTime;
	if (time_delta < 0.f)
	{
		time_delta = 0.f;
		llwarns_sparse << "Negative time passed; zeroed." << llendl;
	}
	mLastTime = time;

	eHandPose* req_hand_pose =
		(eHandPose*)mCharacter->getAnimationData("Hand Pose");
	// check to see if requested pose has changed
	if (!req_hand_pose)
	{
		if (mNewPose != HAND_POSE_RELAXED && mNewPose != mCurrentPose)
		{
			// Only set param weight for poses other than the default
			// (HAND_POSE_SPREAD); HAND_POSE_SPREAD is not an animatable
			// morph !
			if (mNewPose != HAND_POSE_SPREAD)
			{
				mCharacter->setVisualParamWeight(gHandPoseNames[mNewPose],
												 0.f);
			}

			// Reset morph weight for current pose back to its full extend or
			// it might be stuck somewhere in the middle if a pose is requested
			// and the old pose is requested again shortly after while still
			// blending to the other pose !
			if (mCurrentPose != HAND_POSE_SPREAD)
			{
				mCharacter->setVisualParamWeight(gHandPoseNames[mCurrentPose],
												 1.f);
			}

			// Update visual params now if we won't blend
			if (mCurrentPose == HAND_POSE_RELAXED)
			{
				mCharacter->updateVisualParams();
			}
		}
		mNewPose = HAND_POSE_RELAXED;
	}
	else
	{
		// Sometimes we seem to get garbage here, with poses that are out of
		// bounds. So check for a valid pose first.
		if (*req_hand_pose >= 0 && *req_hand_pose < NUM_HAND_POSES)
		{
			// This is a new morph we didn't know about before: reset morph
			// weight for both current and new pose back their starting values
			// while still blending.
			if (*req_hand_pose != mNewPose && mNewPose != mCurrentPose)
			{
				if (mNewPose != HAND_POSE_SPREAD)
				{
					mCharacter->setVisualParamWeight(gHandPoseNames[mNewPose],
													 0.f);
				}

				// Reset morph weight for current pose back to its full extend
				// or it might be stuck somewhere in the middle if a pose is
				// requested and the old pose is requested again shortly after
				// while still blending to the other pose !
				if (mCurrentPose != HAND_POSE_SPREAD)
				{
					mCharacter->setVisualParamWeight(gHandPoseNames[mCurrentPose],
													 1.f);
				}

				// Update visual params now if we won't blend
				if (mCurrentPose == *req_hand_pose)
				{
					mCharacter->updateVisualParams();
				}
			}
			mNewPose = *req_hand_pose;
		}
		else
		{
			llwarns << "Invalid requested hand pose index; ignoring new hand pose."
					<< llendl;
			mNewPose = mCurrentPose;
		}
	}

	mCharacter->removeAnimationData("Hand Pose");
	mCharacter->removeAnimationData("Hand Pose Priority");

	// If we are still blending...
	if (mCurrentPose != mNewPose)
	{
		LL_DEBUGS("Animation") << "New Hand Pose: " << gHandPoseNames[mNewPose]
							   << LL_ENDL;
		F32 incoming_wght = 1.f;
		F32 outgoing_wght = 0.f;

		if (mNewPose != HAND_POSE_SPREAD)
		{
			incoming_wght =
				mCharacter->getVisualParamWeight(gHandPoseNames[mNewPose]);
			incoming_wght += (time_delta / HAND_MORPH_BLEND_TIME);
			incoming_wght = llclamp(incoming_wght, 0.f, 1.f);
			mCharacter->setVisualParamWeight(gHandPoseNames[mNewPose],
											 incoming_wght);
		}

		if (mCurrentPose != HAND_POSE_SPREAD)
		{
			outgoing_wght =
				mCharacter->getVisualParamWeight(gHandPoseNames[mCurrentPose]);
			outgoing_wght -= (time_delta / HAND_MORPH_BLEND_TIME);
			outgoing_wght = llclamp(outgoing_wght, 0.f, 1.f);
			mCharacter->setVisualParamWeight(gHandPoseNames[mCurrentPose],
											 outgoing_wght);
		}

		mCharacter->updateVisualParams();

		if (incoming_wght == 1.f && outgoing_wght == 0.f)
		{
			mCurrentPose = mNewPose;
		}
	}

	return true;
}

std::string LLHandMotion::getHandPoseName(eHandPose pose)
{
	if ((S32)pose < LLHandMotion::NUM_HAND_POSES && (S32)pose >= 0)
	{
		return std::string(gHandPoseNames[pose]);
	}
	return LLStringUtil::null;
}

LLHandMotion::eHandPose LLHandMotion::getHandPose(const std::string& posename)
{
	for (S32 pose = 0; pose < LLHandMotion::NUM_HAND_POSES; ++pose)
	{
		if (gHandPoseNames[pose] == posename)
		{
			return (eHandPose)pose;
		}
	}
	return (eHandPose)0;
}
