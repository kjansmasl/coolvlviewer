/**
 * @file lltoolgrab.cpp
 * @brief LLToolGrab class implementation
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

#include "lltoolgrab.h"

#include "llmessage.h"
#include "llwindow.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFPSClamped
#include "lldrawable.h"
#include "llfloatertools.h"
#include "llhudeffect.h"
#include "llhudmanager.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llworld.h"

constexpr S32 SLOP_DIST_SQ = 4;

LLToolGrab gToolGrab;
LLTool* gGrabTransientTool = NULL;
// Override modifier key behavior with these buttons
bool gGrabBtnVertical = false;
bool gGrabBtnSpin = false;

LLToolGrabBase::LLToolGrabBase(LLToolComposite* composite)
:	LLTool("Grab", composite),
	mMode(GRAB_INACTIVE),
	mVerticalDragging(false),
	mHasMoved(false),
	mOutsideSlop(false),
	mSpinGrabbing(false),
	mSpinRotation(),
	mClickedInMouselook(false)
{
}

// virtual
void LLToolGrabBase::handleSelect()
{
	if (gFloaterToolsp)
	{
		// Viewer can crash during startup if we do not check.
		gFloaterToolsp->setStatusText("grab");
	}
	gGrabBtnVertical = false;
	gGrabBtnSpin = false;
}

void LLToolGrabBase::handleDeselect()
{
	if (hasMouseCapture())
	{
		setMouseCapture(false);
	}
}

bool LLToolGrabBase::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (gDebugClicks)
	{
		llinfos << "Double click becoming mouse-down" << llendl;
	}

	return false;
}

bool LLToolGrabBase::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (gDebugClicks)
	{
		llinfos << "Mouse down" << llendl;
	}

	// Call the base class to propogate info to sim
	LLTool::handleMouseDown(x, y, mask);

	if (!gAgent.leftButtonGrabbed())
	{
		// Can grab transparent objects (how touch event propagates, scripters
		// rely on this) but not particles
		gViewerWindowp->pickAsync(x, y, mask, pickCallback, true, false, true);
	}

	mClickedInMouselook = gAgent.cameraMouselook();

	return true;
}

void LLToolGrabBase::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* objectp = pick_info.getObject();

	gToolGrab.mGrabPick = pick_info;

	bool extend_select = (pick_info.mKeyMask & MASK_SHIFT) != 0;
	if (!extend_select && !gSelectMgr.getSelection()->isEmpty())
	{
		gSelectMgr.deselectAll();
	}

	// If not over object, do nothing
	if (!objectp)
	{
		gToolGrab.setMouseCapture(true);
		gToolGrab.mMode = GRAB_NOOBJECT;
		gToolGrab.mGrabPick.mObjectID.setNull();
	}
	else
	{
		gToolGrab.handleObjectHit(gToolGrab.mGrabPick);
	}
}

bool LLToolGrabBase::handleObjectHit(const LLPickInfo& info)
{
	mGrabPick = info;
	LLViewerObject* objectp = mGrabPick.getObject();
//MK
	if (gRLenabled &&
		!gRLInterface.canTouch(objectp, mGrabPick.mIntersection))
	{
		// hide grab tool immediately
		if (gGrabTransientTool)
		{
			gBasicToolset->selectTool(gGrabTransientTool);
			gGrabTransientTool = NULL;
		}
		return true;
	}
//mk

	if (gDebugClicks)
	{
		llinfos << "Object hit at " << info.mMousePt.mX << ","
				<< info.mMousePt.mY << llendl;
	}

	if (!objectp) // unexpected
	{
		llwarns << "Objectp was NULL, aborting" << llendl;
		return false;
	}

	if (objectp->isAvatar())
	{
		if (gGrabTransientTool)
		{
			gBasicToolset->selectTool(gGrabTransientTool);
			gGrabTransientTool = NULL;
		}
		return true;
	}

	setMouseCapture(true);

	bool script_touch = objectp->flagHandleTouch();
	if (!script_touch)
	{
		LLViewerObject* parent = objectp->getRootEdit();
		script_touch = parent && parent->flagHandleTouch();
	}

	if (!objectp->flagUsePhysics())
	{
		if (script_touch)
		{
			// Script-touch object: let's touch it !
			mMode = GRAB_NONPHYSICAL;
		}
		else if (gAgent.cameraMouselook())
		{
			// In mouselook, we should not be able to grab non-physical,
			// non-touchable objects. If it has a touch handler, we do grab it
			// (so llDetectedGrab works), but movement is blocked on the server
			// side. JC
			mMode = GRAB_LOCKED;
			gViewerWindowp->hideCursor();
			gViewerWindowp->moveCursorToCenter();
		}
		else if (objectp->permMove() && !objectp->isPermanentEnforced())
		{
			mMode = GRAB_ACTIVE_CENTER;
			gViewerWindowp->hideCursor();
			gViewerWindowp->moveCursorToCenter();
		}
		else
		{
			mMode = GRAB_LOCKED;
		}
		// Do not bail out here, go on and grab so buttons can get their
		// "touched" event.
	}
	else if (!objectp->permMove() || objectp->flagCharacter() ||
			 objectp->isPermanentEnforced())
	{
		// If mouse is over a physical object without move permission, show
		// feedback if user tries to move it.
		mMode = GRAB_LOCKED;

		// Do not bail out here, go on and grab so buttons can get their
		// "touched" event.
	}
	else
	{
		// If mouse is over a physical object with move permission,
		// select it and enter "grab" mode (hiding cursor, etc.)
		mMode = GRAB_ACTIVE_CENTER;

		gViewerWindowp->hideCursor();
		gViewerWindowp->moveCursorToCenter();
	}

	// Always send "touched" message

	mLastMouseX = gViewerWindowp->getCurrentMouseX();
	mLastMouseY = gViewerWindowp->getCurrentMouseY();
	mAccumDeltaX = 0;
	mAccumDeltaY = 0;
	mHasMoved = false;
	mOutsideSlop = false;

	mVerticalDragging = info.mKeyMask == MASK_VERTICAL || gGrabBtnVertical;

	startGrab();

	if (info.mKeyMask == MASK_SPIN || gGrabBtnSpin)
	{
		startSpin();
	}

	gSelectMgr.updateSelectionCenter();	// Update selection beam

	// Update point at
	LLViewerObject* edit_object = info.getObject();
	if (edit_object && info.mPickType != LLPickInfo::PICK_FLORA)
	{
		LLVector3 local_edit_pt = gAgent.getPosAgentFromGlobal(info.mPosGlobal);
		local_edit_pt -= edit_object->getPositionAgent();
		local_edit_pt = local_edit_pt * ~edit_object->getRenderRotation();
		gAgent.setPointAt(POINTAT_TARGET_GRAB, edit_object, local_edit_pt);
		gAgent.setLookAt(LOOKAT_TARGET_SELECT, edit_object, local_edit_pt);
	}

	// On transient grabs (clicks on world objects), kill the grab immediately
	if (!gViewerWindowp->getLeftMouseDown() && gGrabTransientTool &&
		(mMode == GRAB_NONPHYSICAL || mMode == GRAB_LOCKED))
	{
		gBasicToolset->selectTool(gGrabTransientTool);
		gGrabTransientTool = NULL;
	}

	return true;
}

void LLToolGrabBase::startSpin()
{
	LLViewerObject* objectp = mGrabPick.getObject();
	if (!objectp)
	{
		return;
	}
	mSpinGrabbing = true;

	// Was saveSelectedObjectTransform()
	LLViewerObject* rootp = (LLViewerObject*)objectp->getRoot();
	mSpinRotation = rootp->getRotation();
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsEdit ||
		 !gRLInterface.canTouch(objectp, mGrabPick.mIntersection)))
	{
		return;
	}
//mk

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ObjectSpinStart);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addUUIDFast(_PREHASH_ObjectID, mGrabPick.mObjectID);
	msg->sendMessage(objectp->getRegion()->getHost());
}

void LLToolGrabBase::stopSpin()
{
	mSpinGrabbing = false;

	LLViewerObject* objectp = mGrabPick.getObject();
	if (!objectp)
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	switch (mMode)
	{
		case GRAB_ACTIVE_CENTER:
		case GRAB_NONPHYSICAL:
		case GRAB_LOCKED:
			msg->newMessageFast(_PREHASH_ObjectSpinStop);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addUUIDFast(_PREHASH_ObjectID, objectp->getID());
			msg->sendMessage(objectp->getRegion()->getHost());
			break;

		case GRAB_NOOBJECT:
		case GRAB_INACTIVE:
		default:
			// do nothing
			break;
	}
}

void LLToolGrabBase::startGrab()
{
	// Compute grab_offset in the OBJECT's root's coordinate frame
	// (sometimes root == object)
	LLViewerObject* objectp = mGrabPick.getObject();
	if (!objectp)
	{
		return;
	}

	LLViewerObject* rootp = (LLViewerObject*)objectp->getRoot();

	// drag from center
	LLVector3d grab_start_global = rootp->getPositionGlobal();

//MK
	if (gRLenabled &&
		(gRLInterface.mContainsEdit ||
		 !gRLInterface.canTouch(objectp, mGrabPick.mIntersection)))
	{
		return;
	}
//mk

	// Where the grab starts, relative to the center of the root object of the
	// set. JC - This code looks wonky, but I believe it does the right thing.
	// Otherwise, when you grab a linked object set, it "pops" on the start of
	// the drag.
	LLVector3d grab_offsetd = rootp->getPositionGlobal() -
							  objectp->getPositionGlobal();

	LLVector3 grab_offset;
	grab_offset.set(grab_offsetd);

	LLQuaternion rotation = rootp->getRotation();
	rotation.transpose();
	grab_offset = grab_offset * rotation;

	// This planar drag starts at the grab point
	mDragStartPointGlobal = grab_start_global;
	mDragStartFromCamera = grab_start_global - gAgent.getCameraPositionGlobal();

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ObjectGrab);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_LocalID, objectp->mLocalID);
	msg->addVector3Fast(_PREHASH_GrabOffset, grab_offset);
	msg->nextBlock("SurfaceInfo");
	msg->addVector3("UVCoord", LLVector3(mGrabPick.mUVCoords));
	msg->addVector3("STCoord", LLVector3(mGrabPick.mSTCoords));
	msg->addS32Fast(_PREHASH_FaceIndex, mGrabPick.mObjectFace);
	msg->addVector3("Position", mGrabPick.mIntersection);
	msg->addVector3("Normal", mGrabPick.mNormal);
	msg->addVector3("Binormal", mGrabPick.mBinormal);
	msg->sendMessage(objectp->getRegion()->getHost());

	mGrabOffsetFromCenterInitial = grab_offset;
	mGrabHiddenOffsetFromCamera = mDragStartFromCamera;

	mGrabTimer.reset();

	mLastUVCoords = mGrabPick.mUVCoords;
	mLastSTCoords = mGrabPick.mSTCoords;
	mLastFace = mGrabPick.mObjectFace;
	mLastIntersection = mGrabPick.mIntersection;
	mLastNormal = mGrabPick.mNormal;
	mLastBinormal = mGrabPick.mBinormal;
	mLastGrabPos = LLVector3(-1.f, -1.f, -1.f);
}

bool LLToolGrabBase::handleHover(S32 x, S32 y, MASK mask)
{
	if (!gViewerWindowp->getLeftMouseDown())
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLGRAB);
		setMouseCapture(false);
		return true;
	}

	// Do the right hover based on mode
	switch (mMode)
	{
		case GRAB_ACTIVE_CENTER:
			handleHoverActive(x, y, mask);	// cursor hidden
			break;

		case GRAB_NONPHYSICAL:
			handleHoverNonPhysical(x, y, mask);
			break;

		case GRAB_INACTIVE:
			handleHoverInactive(x, y, mask);  // cursor set here
			break;

		case GRAB_NOOBJECT:
		case GRAB_LOCKED:
			handleHoverFailed(x, y, mask);
	}

	mLastMouseX = x;
	mLastMouseY = y;

	return true;
}

constexpr F32 GRAB_SENSITIVITY_X = 0.0075f;
constexpr F32 GRAB_SENSITIVITY_Y = 0.0075f;

// Dragging.
void LLToolGrabBase::handleHoverActive(S32 x, S32 y, MASK mask)
{
	LLViewerObject* objectp = mGrabPick.getObject();
	if (!objectp || !hasMouseCapture()) return;
	if (objectp->isDead())
	{
		// Bail out of drag because object has been killed
		setMouseCapture(false);
		return;
	}

//MK
	if (gRLenabled &&
		(gRLInterface.mContainsEdit ||
		 !gRLInterface.canTouch(objectp, mGrabPick.mIntersection)))
	{
		return;
	}
//mk

	//--------------------------------------------------
	// Determine target mode
	//--------------------------------------------------
	bool vertical_dragging = false;
	bool spin_grabbing = false;
	if (mask == MASK_VERTICAL || (gGrabBtnVertical && mask != MASK_SPIN))
	{
		vertical_dragging = true;
	}
	else if (mask == MASK_SPIN || (gGrabBtnSpin && mask != MASK_VERTICAL))
	{
		spin_grabbing = true;
	}

	//--------------------------------------------------
	// Toggle spinning
	//--------------------------------------------------
	if (mSpinGrabbing && !spin_grabbing)
	{
		// User released or switched mask key(s), stop spinning
		stopSpin();
	}
	else if (!mSpinGrabbing && spin_grabbing)
	{
		// User pressed mask key(s), start spinning
		startSpin();
	}
	mSpinGrabbing = spin_grabbing;

	//--------------------------------------------------
	// Toggle vertical dragging
	//--------------------------------------------------
	if (mVerticalDragging && !vertical_dragging)
	{
		// Switch to horizontal dragging
		mDragStartPointGlobal =
			gViewerWindowp->clickPointInWorldGlobal(x, y, objectp);
		mDragStartFromCamera = mDragStartPointGlobal -
							   gAgent.getCameraPositionGlobal();
	}
	else if (!mVerticalDragging && vertical_dragging)
	{
		// Switch to vertical dragging
		mDragStartPointGlobal =
			gViewerWindowp->clickPointInWorldGlobal(x, y, objectp);
		mDragStartFromCamera = mDragStartPointGlobal -
							   gAgent.getCameraPositionGlobal();
	}
	mVerticalDragging = vertical_dragging;

	constexpr F32 RADIANS_PER_PIXEL_X = 0.01f;
	constexpr F32 RADIANS_PER_PIXEL_Y = 0.01f;

	S32 dx = x - gViewerWindowp->getWindowWidth() / 2;
	S32 dy = y - gViewerWindowp->getWindowHeight() / 2;
	if (dx != 0 || dy != 0)
	{
		mAccumDeltaX += dx;
		mAccumDeltaY += dy;
		S32 dist_sq = mAccumDeltaX * mAccumDeltaX +
					  mAccumDeltaY * mAccumDeltaY;
		if (dist_sq > SLOP_DIST_SQ)
		{
			mOutsideSlop = true;
		}

		// Mouse has moved outside center
		mHasMoved = true;

		if (mSpinGrabbing)
		{
			//------------------------------------------------------
			// Handle spinning
			//------------------------------------------------------

			// X motion maps to rotation around vertical axis
			LLQuaternion rot_around_vert(dx * RADIANS_PER_PIXEL_X,
										 LLVector3::z_axis);

			// Y motion maps to rotation around left axis
			const LLVector3& agent_left = gViewerCamera.getLeftAxis();
			LLQuaternion rot_around_left(dy * RADIANS_PER_PIXEL_Y, agent_left);

			// Compose with current rotation
			mSpinRotation = mSpinRotation * rot_around_vert;
			mSpinRotation = mSpinRotation * rot_around_left;

			// *TODO: throttle these
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_ObjectSpinUpdate);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addUUIDFast(_PREHASH_ObjectID, objectp->getID());
			msg->addQuatFast(_PREHASH_Rotation, mSpinRotation);
			msg->sendMessage(objectp->getRegion()->getHost());
		}
		else
		{
			//------------------------------------------------------
			// Handle grabbing
			//------------------------------------------------------

			LLVector3d x_part;
			x_part.set(gViewerCamera.getLeftAxis());
			x_part.mdV[VZ] = 0.0;
			x_part.normalize();

			LLVector3d y_part;
			if (mVerticalDragging)
			{
				y_part.set(gViewerCamera.getUpAxis());
			}
			else
			{
				// Drag toward camera
				y_part = x_part % LLVector3d::z_axis;
				y_part.mdV[VZ] = 0.0;
				y_part.normalize();
			}

			mGrabHiddenOffsetFromCamera = mGrabHiddenOffsetFromCamera +
										  x_part * (-dx * GRAB_SENSITIVITY_X) +
										  y_part * (dy * GRAB_SENSITIVITY_Y);

			// Send the message to the viewer.
			F32 dt = mGrabTimer.getElapsedTimeAndResetF32();
			U32 dt_milliseconds = (U32) (1000.f * dt);

			// Need to return offset from mGrabStartPoint
			LLVector3d grab_pt_global;

			grab_pt_global = gAgent.getCameraPositionGlobal() +
							 mGrabHiddenOffsetFromCamera;

#if 0		// Snap to grid disabled for grab tool: very confusing.
			// Handle snapping to grid, but only when the tool is formally
			// selected.
			if (!gGrabTransientTool && gSavedSettings.getBool("SnapEnabled")
			{
				F64	snap_size = gSavedSettings.getF32("GridResolution");
				U8 snap_dimensions = mVerticalDragging ? 3 : 2;

				for (U8 i = 0; i < snap_dimensions; ++i)
				{
					grab_pt_global.mdV[i] += snap_size * 0.5;
					grab_pt_global.mdV[i] -= fmod(grab_pt_global.mdV[i],
												  snap_size);
				}
			}
#endif
			// Do not let object centers go underground.
			F32 land_height = gWorld.resolveLandHeightGlobal(grab_pt_global);
			if (grab_pt_global.mdV[VZ] < land_height)
			{
				grab_pt_global.mdV[VZ] = land_height;
			}

			// For safety, cap heights where objects can be dragged
			if (grab_pt_global.mdV[VZ] > MAX_OBJECT_Z)
			{
				grab_pt_global.mdV[VZ] = MAX_OBJECT_Z;
			}

			grab_pt_global = gWorld.clipToVisibleRegions(mDragStartPointGlobal,
														 grab_pt_global);
			// Propagate constrained grab point back to grab offset
			mGrabHiddenOffsetFromCamera = grab_pt_global -
										  gAgent.getCameraPositionGlobal();

			// Handle auto-rotation at screen edge.
			LLVector3 grab_pos_agent =
				gAgent.getPosAgentFromGlobal(grab_pt_global);

			LLCoordGL grab_center_gl(gViewerWindowp->getWindowWidth() / 2,
									 gViewerWindowp->getWindowHeight() / 2);
			gViewerCamera.projectPosAgentToScreen(grab_pos_agent,
												  grab_center_gl);

			const S32 rotate_h_margin = gViewerWindowp->getWindowWidth() / 20;
			constexpr F32 ROTATE_ANGLE_PER_SECOND = 30.f * DEG_TO_RAD;
			const F32 rotate_angle = ROTATE_ANGLE_PER_SECOND / gFPSClamped;
			// Build mode moves camera about focus point
			if (grab_center_gl.mX < rotate_h_margin)
			{
				if (gAgent.getFocusOnAvatar())
				{
					gAgent.yaw(rotate_angle);
				}
				else
				{
					gAgent.cameraOrbitAround(rotate_angle);
				}
			}
			else if (grab_center_gl.mX >
						gViewerWindowp->getWindowWidth() - rotate_h_margin)
			{
				if (gAgent.getFocusOnAvatar())
				{
					gAgent.yaw(-rotate_angle);
				}
				else
				{
					gAgent.cameraOrbitAround(-rotate_angle);
				}
			}

			// Do not move above top of screen or below bottom
			if (grab_center_gl.mY < gViewerWindowp->getWindowHeight() - 6 &&
				grab_center_gl.mY > 24)
			{
				// Transmit update to simulator
				LLVector3 grab_pos_region =
					objectp->getRegion()->getPosRegionFromGlobal(grab_pt_global);

				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessageFast(_PREHASH_ObjectGrabUpdate);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addUUIDFast(_PREHASH_ObjectID, objectp->getID());
				msg->addVector3Fast(_PREHASH_GrabOffsetInitial,
									mGrabOffsetFromCenterInitial);
				msg->addVector3Fast(_PREHASH_GrabPosition, grab_pos_region);
				msg->addU32Fast(_PREHASH_TimeSinceLast, dt_milliseconds);
				msg->nextBlock("SurfaceInfo");
				msg->addVector3("UVCoord", LLVector3(mGrabPick.mUVCoords));
				msg->addVector3("STCoord", LLVector3(mGrabPick.mSTCoords));
				msg->addS32Fast(_PREHASH_FaceIndex, mGrabPick.mObjectFace);
				msg->addVector3("Position", mGrabPick.mIntersection);
				msg->addVector3("Normal", mGrabPick.mNormal);
				msg->addVector3("Binormal", mGrabPick.mBinormal);

				msg->sendMessage(objectp->getRegion()->getHost());
			}
		}

		gViewerWindowp->moveCursorToCenter();

		gSelectMgr.updateSelectionCenter();
	}

	// Once we have initiated a drag, lock the camera down
	if (mHasMoved)
	{
		if (!gAgent.cameraMouselook() && !objectp->isHUDAttachment() &&
			objectp->getRoot() == gAgentAvatarp->getRoot())
		{
			// Force focus to point in space where we were looking previously
			gAgent.setFocusGlobal(gAgent.calcFocusPositionTargetGlobal(),
								  LLUUID::null);
			gAgent.setFocusOnAvatar(false);
		}
		else
		{
			gAgent.clearFocusObject();
		}
	}

	// *HACK: to avoid assert: error checking system makes sure that the cursor
	// is set during every handleHover. This is actually a no-op since the
	// cursor is hidden.
	gViewerWindowp->setCursor(UI_CURSOR_ARROW);

	LL_DEBUGS("UserInput") << "Hover handled by LLToolGrab (active) [cursor hidden]"
						   << LL_ENDL;
}

void LLToolGrabBase::handleHoverNonPhysical(S32 x, S32 y, MASK mask)
{
	LLViewerObject* objectp = mGrabPick.getObject();
	if (!objectp || !hasMouseCapture()) return;
	if (objectp->isDead())
	{
		// Bail out of drag because object has been killed
		setMouseCapture(false);
		return;
	}

	LLPickInfo pick = mGrabPick;
	pick.mMousePt = LLCoordGL(x, y);
	pick.getSurfaceInfo();

	// Compute elapsed time
	F32 dt = mGrabTimer.getElapsedTimeAndResetF32();
	U32 dt_milliseconds = (U32) (1000.f * dt);

	// I am not a big fan of the following code - it has been culled from the
	// physical grab case. Ideally these two would be nicely integrated - but
	// the code in that method is a serious mess of spaghetti. So here we go:
#if 1	// *TODO: remove ?  See BUG-100806.
	//--------------------------------------------------
	// Toggle vertical dragging
	//--------------------------------------------------
	if (!gGrabBtnVertical && mask != MASK_VERTICAL)
	{
		mVerticalDragging = false;
	}
	else if (gGrabBtnVertical || mask == MASK_VERTICAL)
	{
		mVerticalDragging = true;
	}

	S32 dx = x - mLastMouseX;
	S32 dy = y - mLastMouseY;
	if (dx != 0 || dy != 0)
	{
		mAccumDeltaX += dx;
		mAccumDeltaY += dy;

		S32 dist_sq = mAccumDeltaX * mAccumDeltaX +
					  mAccumDeltaY * mAccumDeltaY;
		if (dist_sq > SLOP_DIST_SQ)
		{
			mOutsideSlop = true;
		}

		// Mouse has moved
		mHasMoved = true;

		//------------------------------------------------------
		// Handle grabbing
		//------------------------------------------------------

		LLVector3d x_part;
		x_part.set(gViewerCamera.getLeftAxis());
		x_part.mdV[VZ] = 0.0;
		x_part.normalize();

		LLVector3d y_part;
		if (mVerticalDragging)
		{
			y_part.set(gViewerCamera.getUpAxis());
		}
		else
		{
			// Drag toward camera
			y_part = x_part % LLVector3d::z_axis;
			y_part.mdV[VZ] = 0.0;
			y_part.normalize();
		}

		mGrabHiddenOffsetFromCamera = mGrabHiddenOffsetFromCamera +
									  x_part * (-dx * GRAB_SENSITIVITY_X) +
									  y_part * (dy * GRAB_SENSITIVITY_Y);
	}

	// Need to return offset from mGrabStartPoint
	LLVector3d grab_pt_global = gAgent.getCameraPositionGlobal() +
								mGrabHiddenOffsetFromCamera;
	LLVector3 grab_pos_region =
		objectp->getRegion()->getPosRegionFromGlobal(grab_pt_global);
#else
	LLVector3 grab_pos_region = mLastGrabPos;
#endif

	// Only send message if something has changed since last message
	if (grab_pos_region != mLastGrabPos || pick.mObjectFace != mLastFace ||
		pick.mUVCoords != mLastUVCoords || pick.mSTCoords != mLastSTCoords ||
		pick.mNormal != mLastNormal || pick.mBinormal != mLastBinormal ||
		pick.mIntersection != mLastIntersection)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ObjectGrabUpdate);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addUUIDFast(_PREHASH_ObjectID, objectp->getID());
		msg->addVector3Fast(_PREHASH_GrabOffsetInitial,
							mGrabOffsetFromCenterInitial);
		msg->addVector3Fast(_PREHASH_GrabPosition, grab_pos_region);
		msg->addU32Fast(_PREHASH_TimeSinceLast, dt_milliseconds);
		msg->nextBlock("SurfaceInfo");
		msg->addVector3("UVCoord", LLVector3(pick.mUVCoords));
		msg->addVector3("STCoord", LLVector3(pick.mSTCoords));
		msg->addS32Fast(_PREHASH_FaceIndex, pick.mObjectFace);
		msg->addVector3("Position", pick.mIntersection);
		msg->addVector3("Normal", pick.mNormal);
		msg->addVector3("Binormal", pick.mBinormal);

		msg->sendMessage(objectp->getRegion()->getHost());

		mLastUVCoords = pick.mUVCoords;
		mLastSTCoords = pick.mSTCoords;
		mLastFace = pick.mObjectFace;
		mLastIntersection = pick.mIntersection;
		mLastNormal = pick.mNormal;
		mLastBinormal = pick.mBinormal;
		mLastGrabPos = grab_pos_region;
	}

	// Update point-at / look-at
	// If the intersection was on the surface of the object:
	if (pick.mObjectFace != -1)
	{
		LLVector3 local_edit_pt = pick.mIntersection;
		local_edit_pt -= objectp->getPositionAgent();
		local_edit_pt = local_edit_pt * ~objectp->getRenderRotation();
		gAgent.setPointAt(POINTAT_TARGET_GRAB, objectp, local_edit_pt);
		gAgent.setLookAt(LOOKAT_TARGET_SELECT, objectp, local_edit_pt);
	}

	gViewerWindowp->setCursor(UI_CURSOR_HAND);
}

// Not dragging, just showing affordances
void LLToolGrabBase::handleHoverInactive(S32 x, S32 y, MASK mask)
{
	constexpr F32 ROTATE_ANGLE_PER_SECOND = 40.f * DEG_TO_RAD;
	const F32 rotate_angle = ROTATE_ANGLE_PER_SECOND / gFPSClamped;

	// Look for cursor against the edge of the screen. Only works in fullscreen
	if (gWindowp && gWindowp->getFullscreen())
	{
		if (gAgent.cameraThirdPerson())
		{
			if (x == 0)
			{
				gAgent.yaw(rotate_angle);
			}
			else if (x == gViewerWindowp->getWindowWidth() - 1)
			{
				gAgent.yaw(-rotate_angle);
			}
		}
	}

	// JC - *TODO: change cursor based on gGrabBtnVertical, gGrabBtnSpin
	LL_DEBUGS("UserInput") << "Hover handled by LLToolGrab (inactive-not over editable object)"
						   << LL_ENDL;
	gViewerWindowp->setCursor(UI_CURSOR_TOOLGRAB);
}

// User is trying to do something that is not allowed.
void LLToolGrabBase::handleHoverFailed(S32 x, S32 y, MASK mask)
{
	if (mMode == GRAB_NOOBJECT)
	{
		gViewerWindowp->setCursor(UI_CURSOR_NO);
		LL_DEBUGS("UserInput") << "Hover handled by LLToolGrab (not on object)"
							   << LL_ENDL;
	}
	else
	{
		S32 dist_sq = (x - mGrabPick.mMousePt.mX) *
					  (x - mGrabPick.mMousePt.mX) +
					  (y - mGrabPick.mMousePt.mY) *
					  (y - mGrabPick.mMousePt.mY);
		if (mOutsideSlop || dist_sq > SLOP_DIST_SQ)
		{
			mOutsideSlop = true;

			switch (mMode)
			{
			case GRAB_LOCKED:
				gViewerWindowp->setCursor(UI_CURSOR_GRABLOCKED);
				LL_DEBUGS("UserInput") << "Hover handled by LLToolGrab (grab failed, no move permission)"
									   << LL_ENDL;
				break;

#if 0		// Non physical now handled by handleHoverActive - CRO
			case GRAB_NONPHYSICAL:
				gViewerWindowp->setCursor(UI_CURSOR_ARROW);
				LL_DEBUGS("UserInput") << "Hover handled by LLToolGrab (grab failed, non-physical)"
									   << LL_ENDL;
				break;
#endif
			default:
				llassert(false);
			}
		}
		else
		{
			gViewerWindowp->setCursor(UI_CURSOR_ARROW);
			LL_DEBUGS("UserInput") << "Hover handled by LLToolGrab (grab failed but within slop)"
								   << LL_ENDL;
		}
	}
}

bool LLToolGrabBase::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// Call the base class to propogate info to sim
	LLTool::handleMouseUp(x, y, mask);

	if (hasMouseCapture())
	{
		setMouseCapture(false);
	}
	mMode = GRAB_INACTIVE;

	if (mClickedInMouselook && !gAgent.cameraMouselook())
	{
		mClickedInMouselook = false;
	}
	else
	{
		// *HACK: Make some grabs temporary
		if (gGrabTransientTool)
		{
			gBasicToolset->selectTool(gGrabTransientTool);
			gGrabTransientTool = NULL;
		}
	}

#if 0
	gAgent.setObjectTracking(gSavedSettings.getBool("TrackFocusObject"));
#endif

	return true;
}

void LLToolGrabBase::stopEditing()
{
	if (hasMouseCapture())
	{
		setMouseCapture(false);
	}
}

void LLToolGrabBase::onMouseCaptureLost()
{
	LLViewerObject* objectp = mGrabPick.getObject();
	if (!objectp)
	{
		gViewerWindowp->showCursor();
		return;
	}
	// First, fix cursor placement
	if (!gAgent.cameraMouselook() && GRAB_ACTIVE_CENTER == mMode)
	{
		if (objectp->isHUDAttachment())
		{
			// Move cursor "naturally", as if it had moved when hidden
			S32 x = mGrabPick.mMousePt.mX + mAccumDeltaX;
			S32 y = mGrabPick.mMousePt.mY + mAccumDeltaY;
			LLUI::setCursorPositionScreen(x, y);
		}
		else if (mHasMoved)
		{
			// Move cursor back to the center of the object
			LLVector3 grab_pt_agent = objectp->getRenderPosition();

			LLCoordGL gl_point;
			if (gViewerCamera.projectPosAgentToScreen(grab_pt_agent, gl_point))
			{
				LLUI::setCursorPositionScreen(gl_point.mX, gl_point.mY);
			}
		}
		else
		{
			// Move cursor back to click position
			LLUI::setCursorPositionScreen(mGrabPick.mMousePt.mX,
										  mGrabPick.mMousePt.mY);
		}

		gViewerWindowp->showCursor();
	}

	stopGrab();
	if (mSpinGrabbing)
	{
		stopSpin();
	}

	mMode = GRAB_INACTIVE;

	mGrabPick.mObjectID.setNull();

	gSelectMgr.updateSelectionCenter();
	gAgent.setPointAt(POINTAT_TARGET_CLEAR);
	gAgent.setLookAt(LOOKAT_TARGET_CLEAR);

	dialog_refresh_all();
}

void LLToolGrabBase::stopGrab()
{
	LLViewerObject* objectp = mGrabPick.getObject();
	if (!objectp)
	{
		return;
	}

	LLPickInfo pick = mGrabPick;

	if (mMode == GRAB_NONPHYSICAL)
	{
		// For non-physical (touch) grabs, gather surface info for this de-grab
		// (mouse-up)
		S32 x = gViewerWindowp->getCurrentMouseX();
		S32 y = gViewerWindowp->getCurrentMouseY();
		pick.mMousePt = LLCoordGL(x, y);
		pick.getSurfaceInfo();
	}

	// Next, send messages to simulator
	LLMessageSystem* msg = gMessageSystemp;
	switch (mMode)
	{
		case GRAB_ACTIVE_CENTER:
		case GRAB_NONPHYSICAL:
		case GRAB_LOCKED:
			msg->newMessageFast(_PREHASH_ObjectDeGrab);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addU32Fast(_PREHASH_LocalID, objectp->mLocalID);
			msg->nextBlock("SurfaceInfo");
			msg->addVector3("UVCoord", LLVector3(pick.mUVCoords));
			msg->addVector3("STCoord", LLVector3(pick.mSTCoords));
			msg->addS32Fast(_PREHASH_FaceIndex, pick.mObjectFace);
			msg->addVector3("Position", pick.mIntersection);
			msg->addVector3("Normal", pick.mNormal);
			msg->addVector3("Binormal", pick.mBinormal);

			msg->sendMessage(objectp->getRegion()->getHost());

			mVerticalDragging = false;
			break;

		case GRAB_NOOBJECT:
		case GRAB_INACTIVE:
		default:
			// Do nothing
			break;
	}
}

bool LLToolGrabBase::isEditing()
{
	return mGrabPick.getObject().notNull();
}

LLViewerObject* LLToolGrabBase::getEditingObject()
{
	return mGrabPick.getObject();
}

LLVector3d LLToolGrabBase::getEditingPointGlobal()
{
	return getGrabPointGlobal();
}

LLVector3d LLToolGrabBase::getGrabPointGlobal()
{
	switch (mMode)
	{
		case GRAB_ACTIVE_CENTER:
		case GRAB_NONPHYSICAL:
		case GRAB_LOCKED:
			return gAgent.getCameraPositionGlobal() +
				   mGrabHiddenOffsetFromCamera;

		case GRAB_NOOBJECT:
		case GRAB_INACTIVE:
		default:
			return gAgent.getPositionGlobal();
	}
}
