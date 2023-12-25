/**
 * @file llmotion.cpp
 * @brief Implementation of LLMotion class.
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

#include "llmotion.h"

#include "llcriticaldamp.h"

LLMotion::LLMotion(const LLUUID &id)
:	mStopped(true),
	mActive(false),
	mID(id),
	mActivationTimestamp(0.f),
	mStopTimestamp(0.f),
	mSendStopTimestamp(F32_MAX),
	mResidualWeight(0.f),
	mFadeWeight(1.f),
	mDeactivateCallback(NULL),
	mDeactivateCallbackUserData(NULL)
{
	for (S32 i = 0; i < 3; ++i)
	{
		memset(&mJointSignature[i][0], 0,
			   sizeof(U8) * LL_CHARACTER_MAX_ANIMATED_JOINTS);
	}
}

void LLMotion::fadeOut()
{
	if (mFadeWeight > 0.01f)
	{
		mFadeWeight = lerp(mFadeWeight, 0.f, LLCriticalDamp::getInterpolant(0.15f));
	}
	else
	{
		mFadeWeight = 0.f;
	}
}

void LLMotion::fadeIn()
{
	if (mFadeWeight < 0.99f)
	{
		mFadeWeight = lerp(mFadeWeight, 1.f,
						   LLCriticalDamp::getInterpolant(0.15f));
	}
	else
	{
		mFadeWeight = 1.f;
	}
}

void LLMotion::addJointState(const LLPointer<LLJointState>& joint_state)
{
	mPose.addJointState(joint_state);
	S32 priority = joint_state->getPriority();
	if (priority == LLJoint::USE_MOTION_PRIORITY)
	{
		priority = getPriority();
	}

	U32 usage = joint_state->getUsage();

	// For now, usage is everything
	S32 joint_num = joint_state->getJoint()->getJointNum();
	if (joint_num < 0 || joint_num >= (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS)
	{
		LL_DEBUGS("Avatar") << "Joint number (" << joint_num
							<< ") is outside of the legal range [0-"
							<< LL_CHARACTER_MAX_ANIMATED_JOINTS << "]"
							<< LL_ENDL;
	}

	mJointSignature[0][joint_num] = (usage & LLJointState::POS) ? (0xff >> (7 - priority)) : 0;
	mJointSignature[1][joint_num] = (usage & LLJointState::ROT) ? (0xff >> (7 - priority)) : 0;
	mJointSignature[2][joint_num] = (usage & LLJointState::SCALE) ? (0xff >> (7 - priority)) : 0;
}

void LLMotion::setDeactivateCallback(void (*cb)(void*), void* userdata)
{
	mDeactivateCallback = cb;
	mDeactivateCallbackUserData = userdata;
}

//virtual
void LLMotion::setStopTime(F32 time)
{
	mStopTimestamp = time;
	mStopped = true;
}

bool LLMotion::isBlending() const
{
	return mPose.getWeight() < 1.f;
}

void LLMotion::activate(F32 time)
{
	mActivationTimestamp = time;
	mStopped = false;
	mActive = true;
	onActivate();
}

void LLMotion::deactivate()
{
	mActive = false;
	mPose.setWeight(0.f);

	if (mDeactivateCallback)
	{
		(*mDeactivateCallback)(mDeactivateCallbackUserData);
		mDeactivateCallback = NULL; // only call callback once
		mDeactivateCallbackUserData = NULL;
	}

	onDeactivate();
}
