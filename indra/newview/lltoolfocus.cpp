/**
 * @file lltoolfocus.cpp
 * @brief A tool to set the build focus point.
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

#include "lltoolfocus.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llhoverview.h"
#include "llhudmanager.h"
#include "llfloatertools.h"
#include "llmorphview.h"
#include "llpipeline.h"			// For LLPipeline::sFreezeTime
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

// Globals
bool gCameraBtnZoom = true;
bool gCameraBtnOrbit = false;
bool gCameraBtnPan = false;

constexpr S32 SLOP_RANGE = 4;

LLToolFocus gToolFocus;

LLToolFocus::LLToolFocus()
:	LLTool("Focus"),
	mAccumX(0),
	mAccumY(0),
	mMouseDownX(0),
	mMouseDownY(0),
	mOutsideSlopX(false),
	mOutsideSlopY(false),
	mValidClickPoint(false),
	mMouseSteering(false),
	mMouseUpX(0),
	mMouseUpY(0),
	mMouseUpMask(MASK_NONE)
{
}

//virtual
void LLToolFocus::handleSelect()
{
	if (gFloaterToolsp)
	{
		gFloaterToolsp->setStatusText("camera");
	}
}

//virtual
void LLToolFocus::handleDeselect()
{
}

bool LLToolFocus::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Ensure a mouseup
	setMouseCapture(true);

	// Call the base class to propogate info to sim
	LLTool::handleMouseDown(x, y, mask);

	mAccumX = mAccumY = 0;
	mOutsideSlopX = mOutsideSlopY = mValidClickPoint = false;

	// If mouse capture gets ripped away, claim we moused up at the point we
	// clicked down. JC
	mMouseUpX = x;
	mMouseUpY = y;
	mMouseUpMask = mask;

	gViewerWindowp->hideCursor();

	gViewerWindowp->pickAsync(x, y, mask, pickCallback);

	// Do not steal focus from UI
	return false;
}

//static
void LLToolFocus::pickCallback(const LLPickInfo& pick_info)
{
	LLToolFocus* self = &gToolFocus;

	if (!self->hasMouseCapture())
	{
		return;
	}

	self->mMouseDownX = pick_info.mMousePt.mX;
	self->mMouseDownY = pick_info.mMousePt.mY;

	gViewerWindowp->moveCursorToCenter();

	// Potentially recenter if click outside rectangle
	LLViewerObject* hit_obj = pick_info.getObject();

	// Check for hit the sky, or some other invalid point
	if (!hit_obj && pick_info.mPosGlobal.isExactlyZero())
	{
		self->mValidClickPoint = false;
		return;
	}

	// check for hud attachments
	if (hit_obj && hit_obj->isHUDAttachment())
	{
		LLObjectSelectionHandle selection = gSelectMgr.getSelection();
		if (!selection->getObjectCount() ||
			selection->getSelectType() != SELECT_TYPE_HUD)
		{
			self->mValidClickPoint = false;
			return;
		}
	}

	if (gAgent.getCameraMode() == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		bool good_customize_avatar_hit = false;
		if (hit_obj)
		{
			if (hit_obj == gAgentAvatarp)
			{
				// It's you
				good_customize_avatar_hit = true;
			}
			else if (hit_obj->isAttachment() && hit_obj->permYouOwner())
			{
				// It's an attachment that you're wearing
				good_customize_avatar_hit = true;
			}
		}

		if (!good_customize_avatar_hit)
		{
			self->mValidClickPoint = false;
			return;
		}

		if (gMorphViewp)
		{
			gMorphViewp->setCameraDrivenByKeys(false);
		}
	}
	// RN: check to see if this is mouse-driving as opposed to ALT-zoom or
	// Focus tool
	else if (pick_info.mKeyMask & MASK_ALT ||
			gToolMgr.getCurrentTool()->getName() == "Focus")
	{
//MK
		if (gRLenabled &&
			(gRLInterface.contains("camunlock") ||
			 gRLInterface.contains("setcam_unlock")))
		{
			if (!(pick_info.mKeyMask & MASK_ALT) &&
				gAgent.cameraThirdPerson() &&
				gViewerWindowp->getLeftMouseDown() &&
				!LLPipeline::sFreezeTime &&
				(hit_obj == gAgentAvatarp ||
				 (hit_obj && hit_obj->isAttachment() &&
				  LLVOAvatar::findAvatarFromAttachment(hit_obj)->isSelf())))
			{
				// Do nothing, we are steering with the mouse... but the
				// condition is so complex it is better to check for its
				// negative.
			}
			else
			{
				self->mValidClickPoint = false;
				return;
			}
		}
//mk
		LLViewerObject* hit_obj = pick_info.getObject();
		if (hit_obj)
		{
			// ...clicked on a world object, so focus at its position
			if (!hit_obj->isHUDAttachment())
			{
				gAgent.setFocusOnAvatar(false);
				gAgent.setFocusGlobal(pick_info);
			}
		}
		else if (!pick_info.mPosGlobal.isExactlyZero())
		{
			// Hit the ground
			gAgent.setFocusOnAvatar(false);
			gAgent.setFocusGlobal(pick_info);
		}

		if (!(pick_info.mKeyMask & MASK_ALT) && gAgent.cameraThirdPerson() &&
			gViewerWindowp->getLeftMouseDown() && !LLPipeline::sFreezeTime &&
			(hit_obj == gAgentAvatarp ||
			 (hit_obj && hit_obj->isAttachment() &&
			  LLVOAvatar::findAvatarFromAttachment(hit_obj)->isSelf())))
		{
			self->mMouseSteering = true;
		}
	}

	self->mValidClickPoint = true;

	if (CAMERA_MODE_CUSTOMIZE_AVATAR == gAgent.getCameraMode())
	{
		gAgent.setFocusOnAvatar(false, false);

		LLVector3d cam_pos = gAgent.getCameraPositionGlobal();
		cam_pos -= LLVector3d(gViewerCamera.getLeftAxis() *
				   gAgent.calcCustomizeAvatarUIOffset(cam_pos));

		gAgent.setCameraPosAndFocusGlobal(cam_pos, pick_info.mPosGlobal,
										  pick_info.mObjectID);
	}
}

// "Let go" of the mouse, for example on mouse up or when we loose mouse
// capture. This ensures that cursor becomes visible if a modal dialog pops up
// during Alt-Zoom. JC
void LLToolFocus::releaseMouse()
{
	// Need to tell the sim that the mouse button is up, since this tool is no
	// longer working and cursor is visible (despite actual mouse button status).
	LLTool::handleMouseUp(mMouseUpX, mMouseUpY, mMouseUpMask);

	gViewerWindowp->showCursor();

	gToolMgr.clearTransientTool();

	mMouseSteering = false;
	mValidClickPoint = false;
	mOutsideSlopX = false;
	mOutsideSlopY = false;
}

bool LLToolFocus::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// Claim that we're mousing up somewhere
	mMouseUpX = x;
	mMouseUpY = y;
	mMouseUpMask = mask;

	if (hasMouseCapture())
	{
		if (mValidClickPoint)
		{
			if (CAMERA_MODE_CUSTOMIZE_AVATAR == gAgent.getCameraMode())
			{
				LLCoordGL mouse_pos;
				LLVector3 focus_pos;
				focus_pos = gAgent.getPosAgentFromGlobal(gAgent.getFocusGlobal());
				bool success = gViewerCamera.projectPosAgentToScreen(focus_pos,
																	 mouse_pos);
				if (success)
				{
					LLUI::setCursorPositionScreen(mouse_pos.mX, mouse_pos.mY);
				}
			}
			else if (mMouseSteering)
			{
				LLUI::setCursorPositionScreen(mMouseDownX, mMouseDownY);
			}
			else
			{
				gViewerWindowp->moveCursorToCenter();
			}
		}
		else
		{
			// not a valid zoomable object
			LLUI::setCursorPositionScreen(mMouseDownX, mMouseDownY);
		}

		// calls releaseMouse() internally
		setMouseCapture(false);
	}
	else
	{
		releaseMouse();
	}

	return true;
}

bool LLToolFocus::handleHover(S32 x, S32 y, MASK mask)
{
	S32 dx = gViewerWindowp->getCurrentMouseDX();
	S32 dy = gViewerWindowp->getCurrentMouseDY();

	if (hasMouseCapture() && mValidClickPoint)
	{
		mAccumX += abs(dx);
		mAccumY += abs(dy);

		if (mAccumX >= SLOP_RANGE)
		{
			mOutsideSlopX = true;
		}

		if (mAccumY >= SLOP_RANGE)
		{
			mOutsideSlopY = true;
		}
	}

	if (mOutsideSlopX || mOutsideSlopY)
	{
		if (!mValidClickPoint)
		{
			LL_DEBUGS("UserInput") << "hover handled by LLToolFocus [invalid point]"
								   << LL_ENDL;
			gViewerWindowp->setCursor(UI_CURSOR_NO);
			gViewerWindowp->showCursor();
			return true;
		}

		if (gCameraBtnOrbit || mask == MASK_ORBIT ||
			mask == (MASK_ALT | MASK_ORBIT))
		{
			// Orbit tool
			if (hasMouseCapture())
			{
				const F32 radians_per_pixel = 360.f * DEG_TO_RAD /
											  gViewerWindowp->getWindowWidth();

				if (dx != 0)
				{
					gAgent.cameraOrbitAround(-dx * radians_per_pixel);
				}

				if (dy != 0)
				{
					gAgent.cameraOrbitOver(-dy * radians_per_pixel);
				}

				gViewerWindowp->moveCursorToCenter();
			}
			LL_DEBUGS("UserInput") << "hover handled by LLToolFocus [active]"
								  << LL_ENDL;
		}
		else if (gCameraBtnPan || mask == MASK_PAN ||
				 mask == (MASK_PAN | MASK_ALT))
		{
			// Pan tool
			if (hasMouseCapture())
			{
				LLVector3d camera_to_focus = gAgent.getCameraPositionGlobal();
				camera_to_focus -= gAgent.getFocusGlobal();
				F32 dist = (F32)camera_to_focus.normalize();

				// Fudge factor for pan
				F32 meters_per_pixel = 3.f * dist / gViewerWindowp->getWindowWidth();

				if (dx != 0)
				{
					gAgent.cameraPanLeft(dx * meters_per_pixel);
				}

				if (dy != 0)
				{
					gAgent.cameraPanUp(-dy * meters_per_pixel);
				}

				gViewerWindowp->moveCursorToCenter();
			}
			LL_DEBUGS("UserInput") << "hover handled by LLToolPan" << LL_ENDL;
		}
		else if (gCameraBtnZoom)
		{
			// Zoom tool
			if (hasMouseCapture())
			{
				const F32 radians_per_pixel = 360.f * DEG_TO_RAD /
											  gViewerWindowp->getWindowWidth();

				if (dx != 0)
				{
					gAgent.cameraOrbitAround(-dx * radians_per_pixel);
				}

				constexpr F32 IN_FACTOR = 0.99f;

				if (dy != 0 && mOutsideSlopY)
				{
					if (mMouseSteering)
					{
						gAgent.cameraOrbitOver(-dy * radians_per_pixel);
					}
					else
					{
						gAgent.cameraZoomIn(powf(IN_FACTOR, dy));
					}
				}

				gViewerWindowp->moveCursorToCenter();
			}

			LL_DEBUGS("UserInput") << "hover handled by LLToolZoom" << LL_ENDL;
		}
	}

	if (gCameraBtnOrbit || mask == MASK_ORBIT ||
		mask == (MASK_ALT | MASK_ORBIT))
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLCAMERA);
	}
	else if (gCameraBtnPan || mask == MASK_PAN ||
			 mask == (MASK_PAN | MASK_ALT))
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLPAN);
	}
	else
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLZOOMIN);
	}

	return true;
}

void LLToolFocus::onMouseCaptureLost()
{
	releaseMouse();
}
