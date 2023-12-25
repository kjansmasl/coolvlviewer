/**
 * @file llmorphview.cpp
 * @brief Container for Morph functionality
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

#include "llviewerprecompiledheaders.h"

#include "llmorphview.h"

#include "llanimationstates.h"
#include "lljoint.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llfirstuse.h"
#include "llfloatercustomize.h"
#include "llfloatertools.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llvisualparamhint.h"
#include "llvoavatarself.h"

// Instance created in LLViewerWindow::initWorldUI()
LLMorphView* gMorphViewp = NULL;

constexpr F32 MORPH_NEAR_CLIP = 0.1f;

LLMorphView::LLMorphView(const LLRect& rect)
:	LLView("morph view", rect, false, FOLLOWS_ALL),
	mCameraTargetJoint(NULL),
	mCameraOffset(-0.5f, 0.05f, 0.07f),
	mCameraTargetOffset(0.f, 0.f, 0.05f),
	mOldCameraNearClip(0.f),
	mCameraPitch(0.f),
	mCameraYaw(0.f),
	mCameraDrivenByKeys(false)
{
}

LLMorphView::~LLMorphView()
{
	gMorphViewp = NULL;
}

void LLMorphView::initialize()
{
	mCameraPitch = 0.f;
	mCameraYaw = 0.f;

	if (!isAgentAvatarValid())
	{
		gAgent.changeCameraToDefault();
		return;
	}

	gAgentAvatarp->stopMotion(ANIM_AGENT_BODY_NOISE);
	gAgentAvatarp->mSpecialRenderMode = 3;

	// Set up camera for close look at avatar
	mOldCameraNearClip = gViewerCamera.getNear();
	gViewerCamera.setNear(MORPH_NEAR_CLIP);
}

void LLMorphView::shutdown()
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->startMotion(ANIM_AGENT_BODY_NOISE);
		gAgentAvatarp->mSpecialRenderMode = 0;
		// Reset camera
		gViewerCamera.setNear(mOldCameraNearClip);
	}
}

void LLMorphView::setVisible(bool visible)
{
	if (visible &&
		(!gAgentWearables.getWearableCount(LLWearableType::WT_SHAPE) ||
		 !gAgentWearables.getWearableCount(LLWearableType::WT_HAIR) ||
		 !gAgentWearables.getWearableCount(LLWearableType::WT_EYES) ||
		 !gAgentWearables.getWearableCount(LLWearableType::WT_SKIN)))
	{
		// Do not let the user edit wearables if avatar is cloud due to missing
		// parts.
		visible = false;
		llwarns << "Cannot edit appearance while mandatory wearables are missing from outfit."
				<< llendl;
	}

	if (gFloaterViewp && visible != getVisible())
	{
		LLView::setVisible(visible);
		if (visible)
		{
			llassert(!gFloaterCustomizep);
			gFloaterCustomizep = new LLFloaterCustomize();
			gFloaterCustomizep->fetchInventory();
			gFloaterCustomizep->open();
			gFloaterCustomizep->switchToDefaultSubpart();

			initialize();

			// First run dialog
			LLFirstUse::useAppearance();
		}
		else
		{
			if (gFloaterCustomizep)
			{
				gFloaterViewp->removeChild(gFloaterCustomizep);
				delete gFloaterCustomizep;
				gFloaterCustomizep = NULL;
			}

			shutdown();
		}
	}
}

void LLMorphView::updateCamera()
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	if (!mCameraTargetJoint)
	{
		setCameraTargetJoint(gAgentAvatarp->getJoint(LL_JOINT_KEY_HEAD));
	}

	LLJoint* root_joint = gAgentAvatarp->getRootJoint();
	if (!root_joint)
	{
		return;
	}

	const LLQuaternion& avatar_rot = root_joint->getWorldRotation();

	LLVector3d joint_pos =
		gAgent.getPosGlobalFromAgent(mCameraTargetJoint->getWorldPosition());
	LLVector3d target_pos = joint_pos + mCameraTargetOffset * avatar_rot;

	LLQuaternion camera_rot_yaw(mCameraYaw, LLVector3::z_axis);
	LLQuaternion camera_rot_pitch(mCameraPitch, LLVector3::y_axis);

	LLVector3d camera_pos = joint_pos +
							mCameraOffset * camera_rot_pitch *
							camera_rot_yaw * avatar_rot;

	gAgent.setCameraPosAndFocusGlobal(camera_pos, target_pos, gAgentID);
}

void LLMorphView::setCameraDrivenByKeys(bool b)
{
	if (mCameraDrivenByKeys != b)
	{
		if (b)
		{
			// Reset to the default camera position specified by mCameraPitch,
			// mCameraYaw, etc.
			updateCamera();
		}
		mCameraDrivenByKeys = b;
	}
}
