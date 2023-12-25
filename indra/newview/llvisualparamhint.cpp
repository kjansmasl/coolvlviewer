/**
 * @file llvisualparamhint.cpp
 * @brief A dynamic texture class for displaying avatar visual params effects
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

// Note: probably because of obscure pre-historical reasons, this file is
// named "lltoolmorph.cpp" in LL's viewer sources. I renamed it based on the
// class it implements instead. HB

#include "llviewerprecompiledheaders.h"

#include "llvisualparamhint.h"

#include "llrender.h"
#include "llwearable.h"

#include "llagent.h"
#include "lldrawable.h"
#include "lldrawpoolavatar.h"
#include "llface.h"
#include "llmorphview.h"
#include "llpipeline.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llvoavatarself.h"

//static
LLVisualParamHint::instance_list_t LLVisualParamHint::sInstances;
bool LLVisualParamReset::sDirty = false;

//-----------------------------------------------------------------------------
// LLVisualParamHint() class
//-----------------------------------------------------------------------------

// static
LLVisualParamHint::LLVisualParamHint(S32 pos_x, S32 pos_y,
									 S32 width, S32 height,
									 LLViewerJointMesh* mesh,
									 LLViewerVisualParam* param,
									 LLWearable* wearable,
									 F32 param_weight,
									 LLJoint* jointp)
:	LLViewerDynamicTexture(width, height, 3,
						   LLViewerDynamicTexture::ORDER_MIDDLE, true),
	mNeedsUpdate(true),
	mAllowsUpdates(true),
	mIsVisible(false),
	mJointMesh(mesh),
	mVisualParam(param),
	mWearablePtr(wearable),
	mVisualParamWeight(param_weight),
	mDelayFrames(0),
	mRect(pos_x, pos_y + height, pos_x + width, pos_y),
	mLastParamWeight(0.f),
	mCamTargetJoint(jointp)
{
	LLVisualParamHint::sInstances.insert(this);
	mBackgroundp = LLUI::getUIImage("avatar_thumb_bkgrnd.j2c");

	if (!mCamTargetJoint)
	{
		llwarns << "Missing camera target joint !" << llendl;
	}

	llassert(mCamTargetJoint);
	llassert(width != 0);
	llassert(height != 0);
}

LLVisualParamHint::~LLVisualParamHint()
{
	LLVisualParamHint::sInstances.erase(this);
}

//virtual
S8 LLVisualParamHint::getType() const
{
	return LLViewerDynamicTexture::LL_VISUAL_PARAM_HINT;
}

// Requests updates for all instances (excluding two possible exceptions).
// Grungy but efficient.
//static
void LLVisualParamHint::requestHintUpdates(LLVisualParamHint* exception1,
										   LLVisualParamHint* exception2)
{
	S32 delay_frames = 0;
	for (instance_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); ++iter)
	{
		LLVisualParamHint* instance = *iter;
		if (instance != exception1 && instance != exception2)
		{
			if (instance->mAllowsUpdates)
			{
				instance->mNeedsUpdate = true;
				instance->mDelayFrames = delay_frames++;
			}
			else
			{
				instance->mNeedsUpdate = true;
				instance->mDelayFrames = 0;
			}
		}
	}
}

bool LLVisualParamHint::needsRender()
{
	return mNeedsUpdate && mDelayFrames-- <= 0 && mAllowsUpdates &&
		   isAgentAvatarValid() && !gAgentAvatarp->getIsAppearanceAnimating();
}

void LLVisualParamHint::preRender(bool clear_depth)
{
	if (isAgentAvatarValid())
	{
		mLastParamWeight = mVisualParam->getWeight();
		if (mWearablePtr)
		{
			mWearablePtr->setVisualParamWeight(mVisualParam->getID(),
											   mVisualParamWeight, false);
			LLViewerWearable* wearable = mWearablePtr->asViewerWearable();
			if (wearable)
			{
				wearable->setVolatile(true);
			}
		}
		else
		{
			llwarns << "mWearablePtr is NULL: cannot set wearable visual param weight."
					<< llendl;
		}
		gAgentAvatarp->setVisualParamWeight(mVisualParam->getID(),
											mVisualParamWeight, false);
		gAgentAvatarp->setVisualParamWeight("Blink_Left", 0.f);
		gAgentAvatarp->setVisualParamWeight("Blink_Right", 0.f);
		gAgentAvatarp->updateComposites();
		gAgentAvatarp->updateVisualParams();
#if 0	// This is a NOP !
		gAgentAvatarp->updateGeometry(gAgentAvatarp->mDrawable);
#endif
		gAgentAvatarp->updateLOD();

		LLViewerDynamicTexture::preRender(clear_depth);
	}
}

bool LLVisualParamHint::render()
{
	if (!isAgentAvatarValid()) return true;

	LLVisualParamReset::sDirty = true;

	gGL.pushUIMatrix();
	gGL.loadUIIdentity();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho(0.f, mFullWidth, 0.f, mFullHeight, -1.f, 1.f);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	gUIProgram.bind();

	LLGLSUIDefault gls_ui;
	//LLGLState::verify(true);
	mBackgroundp->draw(0, 0, mFullWidth, mFullHeight);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	mNeedsUpdate = false;
	mIsVisible = true;

	LLQuaternion avatar_rot;
	LLJoint* rootp = gAgentAvatarp->getRootJoint();
	if (rootp)
	{
		avatar_rot = rootp->getWorldRotation();
	}

	LLVector3 target_joint_pos;
	if (mCamTargetJoint)
	{
		target_joint_pos = mCamTargetJoint->getWorldPosition();
	}
	LLVector3 target_offset(0.f, 0.f, mVisualParam->getCameraElevation());
	LLVector3 target_pos = target_joint_pos + (target_offset * avatar_rot);

	F32 cam_angle_radians = mVisualParam->getCameraAngle() * DEG_TO_RAD;
	LLVector3 snapshot_offset(mVisualParam->getCameraDistance() *
							  cosf(cam_angle_radians),
							  mVisualParam->getCameraDistance() *
							  sinf(cam_angle_radians),
							  mVisualParam->getCameraElevation());
	LLVector3 camera_pos = target_joint_pos + snapshot_offset * avatar_rot;

	gGL.flush();

	gViewerCamera.setAspect((F32)mFullWidth / (F32)mFullHeight);
	gViewerCamera.setOriginAndLookAt(camera_pos,		// Camera
									 LLVector3::z_axis,	// Up
									 target_pos);		// Point of interest

	gViewerCamera.setPerspective(false, mOrigin.mX, mOrigin.mY, mFullWidth,
								  mFullHeight, false);

	// Do not let environment settings influence our scene lighting.
	LLPreviewLighting preview_light;

	gPipeline.previewAvatar(gAgentAvatarp);

	gAgentAvatarp->setVisualParamWeight(mVisualParam->getID(),
										mLastParamWeight);
	if (mWearablePtr)
	{
		mWearablePtr->setVisualParamWeight(mVisualParam->getID(),
										   mLastParamWeight, false);
	}
	else
	{
		llwarns << "mWearablePtr is NULL: cannot set wearable visual param weight."
				<< llendl;
	}

	LLViewerWearable* wearablep = mWearablePtr->asViewerWearable();
	if (wearablep)
	{
		wearablep->setVolatile(false);
	}

	gAgentAvatarp->updateVisualParams();

	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	mImageGLp->setGLTextureCreated(true);

	gGL.popUIMatrix();

	return true;
}

void LLVisualParamHint::draw()
{
	if (!mIsVisible) return;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bind(this);

	gGL.color4f(1.f, 1.f, 1.f, 1.f);

	LLGLSUIDefault gls_ui;
	gGL.begin(LLRender::TRIANGLES);
	{
		gGL.texCoord2i(0, 1);
		gGL.vertex2i(0, mFullHeight);
		gGL.texCoord2i(0, 0);
		gGL.vertex2i(0, 0);
		gGL.texCoord2i(1, 0);
		gGL.vertex2i(mFullWidth, 0);
		gGL.texCoord2i(0, 1);
		gGL.vertex2i(0, mFullHeight);
		gGL.texCoord2i(1, 0);
		gGL.vertex2i(mFullWidth, 0);
		gGL.texCoord2i(1, 1);
		gGL.vertex2i(mFullWidth, mFullHeight);
	}
	gGL.end();

	unit0->unbind(LLTexUnit::TT_TEXTURE);
}

void LLVisualParamHint::setWearable(LLWearable* wearable,
									LLViewerVisualParam* param)
{
	mWearablePtr = wearable;
	mVisualParam = param;
}

//-----------------------------------------------------------------------------
// LLVisualParamReset() class
//-----------------------------------------------------------------------------

LLVisualParamReset::LLVisualParamReset()
:	LLViewerDynamicTexture(1, 1, 1, ORDER_RESET, false)
{
}

//virtual
S8 LLVisualParamReset::getType() const
{
	return LLViewerDynamicTexture::LL_VISUAL_PARAM_RESET;
}

bool LLVisualParamReset::render()
{
	if (sDirty && isAgentAvatarValid())
	{
		gAgentAvatarp->updateComposites();
		gAgentAvatarp->updateVisualParams();
#if 0	// This is a NOP !
		gAgentAvatarp->updateGeometry(gAgentAvatarp->mDrawable);
#endif
		sDirty = false;
	}

	return false;
}
