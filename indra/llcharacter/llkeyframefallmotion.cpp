/**
 * @file llkeyframefallmotion.cpp
 * @brief Implementation of LLKeyframeFallMotion class.
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

#include "llkeyframefallmotion.h"

#include "llcharacter.h"
#include "llmatrix3.h"

LLKeyframeFallMotion::LLKeyframeFallMotion(const LLUUID& id)
:	LLKeyframeMotion(id),
	mCharacter(NULL),
	mVelocityZ(0.f)
{
}

LLMotion::LLMotionInitStatus LLKeyframeFallMotion::onInitialize(LLCharacter* chrp)
{
	// Save character pointer for later use
	mCharacter = chrp;

	// Load keyframe data, setup pose and joint states
	LLMotion::LLMotionInitStatus result = LLKeyframeMotion::onInitialize(chrp);
	if (result != LLMotion::STATUS_SUCCESS)
	{
		return result;
	}

	for (U32 jm = 0, count = mJointMotionList->getNumJointMotions();
		 jm < count; ++jm)
	{
		LLJointState* jstate = mJointStates[jm];
		if (jstate)
		{
			LLJoint* joint = jstate->getJoint();
			if (joint && joint->getName() == "mPelvis")
			{
				mPelvisState = jstate;
				return result;
			}
		}
	}

	return result;
}

bool LLKeyframeFallMotion::onActivate()
{
	mVelocityZ = -mCharacter->getCharacterVelocity().mV[VZ];

	LLVector3 ground_pos;
	LLVector3 ground_normal;
	mCharacter->getGround(mCharacter->getCharacterPosition(), ground_pos,
						  ground_normal);
	ground_normal.normalize();

	LLQuaternion inverse_pelvis_rot = mCharacter->getCharacterRotation();
	inverse_pelvis_rot.transpose();

	// Find ground normal in pelvis space
	ground_normal = ground_normal * inverse_pelvis_rot;

	// Calculate new foward axis
	LLVector3 fwd_axis = LLVector3::x_axis -
						 ground_normal * (ground_normal * LLVector3::x_axis);
	fwd_axis.normalize();
	mRotationToGroundNormal = LLQuaternion(fwd_axis, ground_normal % fwd_axis,
										   ground_normal);

	return LLKeyframeMotion::onActivate();
}

bool LLKeyframeFallMotion::onUpdate(F32 time, U8* joint_mask)
{
	bool result = LLKeyframeMotion::onUpdate(time, joint_mask);
	F32 slerp_amt = clamp_rescale(time / getDuration(), 0.5f, 0.75f, 0.f, 1.f);

	if (mPelvisState.notNull())
	{
		mPelvisState->setRotation(mPelvisState->getRotation() *
								  slerp(slerp_amt, mRotationToGroundNormal,
										LLQuaternion()));
	}

	return result;
}
