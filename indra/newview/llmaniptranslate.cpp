/**
 * @file llmaniptranslate.cpp
 * @brief LLManipTranslate class implementation
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

/**
 * Positioning tool
 */

#include "llviewerprecompiledheaders.h"

#include "llmaniptranslate.h"

#include "llgl.h"
#include "llrender.h"
#include "llrenderutils.h"			// For gCone

#include "llagent.h"
#include "llappviewer.h"			// For gFPSClamped
#include "lldrawable.h"
#include "llfloatertools.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For hud_render_text()
#include "llviewerjoint.h"
#include "llviewerobject.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llworld.h"

constexpr S32 NUM_AXES = 3;
constexpr S32 MOUSE_DRAG_SLOP = 2;       	// In pixels
constexpr F32 SELECTED_ARROW_SCALE = 1.3f;
constexpr F32 MANIP_HOTSPOT_START = 0.2f;
constexpr F32 MANIP_HOTSPOT_END = 1.2f;
constexpr F32 SNAP_GUIDE_SCREEN_SIZE = 0.7f;
constexpr F32 MIN_PLANE_MANIP_DOT_PRODUCT = 0.25f;
constexpr F32 PLANE_TICK_SIZE = 0.4f;
constexpr F32 QUARTER_TICK_SIZE = PLANE_TICK_SIZE * 0.25f;
constexpr F32 MANIP_SCALE_HALF_LIFE = 0.07f;
constexpr F32 SNAP_ARROW_SCALE = 0.7f;

static LLPointer<LLViewerTexture> sGridTex = NULL;

const LLManip::EManipPart MANIPULATOR_IDS[9] =
{
	LLManip::LL_X_ARROW,
	LLManip::LL_Y_ARROW,
	LLManip::LL_Z_ARROW,
	LLManip::LL_X_ARROW,
	LLManip::LL_Y_ARROW,
	LLManip::LL_Z_ARROW,
	LLManip::LL_YZ_PLANE,
	LLManip::LL_XZ_PLANE,
	LLManip::LL_XY_PLANE
};

static const U32 ARROW_TO_AXIS[4] =
{
	VX,
	VX,
	VY,
	VZ
};

// Sort manipulator handles by their screen-space projection
struct ClosestToCamera
{
	bool operator()(const LLManipTranslate::ManipulatorHandle& a,
					const LLManipTranslate::ManipulatorHandle& b) const
	{
		return a.mEndPosition.mV[VZ] < b.mEndPosition.mV[VZ];
	}
};

LLManipTranslate::LLManipTranslate(LLToolComposite* composite)
:	LLManip("Move", composite),
	mLastHoverMouseX(-1),
	mLastHoverMouseY(-1),
	mMouseOutsideSlop(false),
	mCopyMadeThisDrag(false),
	mMouseDownX(-1),
	mMouseDownY(-1),
	mAxisArrowLength(50),
	mConeSize(0),
	mArrowLengthMeters(0.f),
	mPlaneManipOffsetMeters(0.f),
	mUpdateTimer(),
	mSnapOffsetMeters(0.f),
	mSubdivisions(10.f),
	mInSnapRegime(false),
	mArrowScales(1.f, 1.f, 1.f),
	mPlaneScales(1.f, 1.f, 1.f),
	mPlaneManipPositions(1.f, 1.f, 1.f, 1.f)
{
}

// static
bool LLManipTranslate::getSnapEnabled()
{
	static LLCachedControl<bool> snap_enabled(gSavedSettings, "SnapEnabled");
	return snap_enabled;
}

// static
bool LLManipTranslate::getSnapToMouseCursor()
{
	static LLCachedControl<bool> snap_to_mouse_cursor(gSavedSettings,
													  "SnapToMouseCursor");
	return snap_to_mouse_cursor;
}

// static
F32 LLManipTranslate::getGridDrawSize()
{
	static LLCachedControl<F32> grid_draw_size(gSavedSettings, "GridDrawSize");
	return grid_draw_size;
}

//static
U32 LLManipTranslate::getGridTexName()
{
	if (sGridTex.isNull())
	{
		restoreGL();
	}

	return sGridTex.isNull() ? 0 : sGridTex->getTexName();
}

//static
void LLManipTranslate::destroyGL()
{
	if (sGridTex)
	{
		sGridTex = NULL;
	}
}

//static
void LLManipTranslate::restoreGL()
{
	// Generate grid texture
	U32 rez = 512;
	U32 mip = 0;

	destroyGL();
	sGridTex = LLViewerTextureManager::getLocalTexture();
	if (!sGridTex->createGLTexture())
	{
		sGridTex = NULL;
		return;
	}

	GLuint* d = new GLuint[rez * rez];

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bindManual(LLTexUnit::TT_TEXTURE, sGridTex->getTexName(), true);
	unit0->setTextureFilteringOption(LLTexUnit::TFO_TRILINEAR);

	while (rez >= 1)
	{
		for (U32 i = 0; i < rez * rez; i++)
		{
			d[i] = 0x00FFFFFF;
		}

		U32 subcol = 0xFFFFFFFF;
		if (rez >= 4)
		{
			// Large grain grid
			for (U32 i = 0; i < rez; ++i)
			{
				if (rez <= 16)
				{
					if (rez == 16)
					{
						subcol = 0xA0FFFFFF;
					}
					else if (rez == 8)
					{
						subcol = 0x80FFFFFF;
					}
					else
					{
						subcol = 0x40FFFFFF;
					}
				}
				else
				{
					subcol = 0xFFFFFFFF;
				}
				d[i * rez] = subcol;
				d[i] = subcol;
				if (rez >= 32)
				{
					d[i * rez + rez - 1] = subcol;
					d[(rez - 1)	* rez + i] = subcol;
				}

				if (rez >= 64)
				{
					subcol = 0xFFFFFFFF;

					if (i > 0 && i < rez - 1)
					{
						d[i * rez + 1] = subcol;
						d[i * rez + rez - 2] = subcol;
						d[rez + i] = subcol;
						d[(rez - 2)	* rez + i] = subcol;
					}
				}
			}
		}

		subcol = 0x50A0A0A0;
		if (rez >= 128)
		{
			// Small grain grid
			for (U32 i = 8; i < rez; i += 8)
			{
				for (U32 j = 2; j < rez - 2; ++j)
				{
					d[i	* rez + j] = subcol;
					d[j	* rez + i] = subcol;
				}
			}
		}
		if (rez >= 64)
		{
			// Medium grain grid
			if (rez == 64)
			{
				subcol = 0x50A0A0A0;
			}
			else
			{
				subcol = 0xA0D0D0D0;
			}

			for (U32 i = 32; i < rez; i += 32)
			{
				U32 pi = i - 1;
				for (U32 j = 2; j < rez - 2; ++j)
				{
					d[i * rez + j] = subcol;
					d[j * rez + i] = subcol;

					if (rez > 128)
					{
						d[pi * rez + j] = subcol;
						d[j * rez + pi] = subcol;
					}
				}
			}
		}
		LLImageGL::setManualImage(GL_TEXTURE_2D, mip, GL_RGBA, rez, rez,
								  GL_RGBA, GL_UNSIGNED_BYTE, d);
		rez = rez >> 1;
		++mip;
	}
	delete[] d;
}

void LLManipTranslate::handleSelect()
{
	gSelectMgr.saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
	if (gFloaterToolsp)
	{
		gFloaterToolsp->setStatusText("move");
	}
	LLManip::handleSelect();
}

bool LLManipTranslate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	// Did not click in any UI object, so must have clicked in the world
	if (mHighlightedPart == LL_X_ARROW || mHighlightedPart == LL_Y_ARROW ||
		mHighlightedPart == LL_Z_ARROW || mHighlightedPart == LL_YZ_PLANE ||
		mHighlightedPart == LL_XZ_PLANE || mHighlightedPart == LL_XY_PLANE)
	{
		handled = handleMouseDownOnPart(x, y, mask);
	}

	return handled;
}

// Assumes that one of the arrows on an object was hit.
bool LLManipTranslate::handleMouseDownOnPart(S32 x, S32 y, MASK mask)
{
	if (!canAffectSelection())
	{
		return false;
	}

	highlightManipulators(x, y);
	S32 hit_part = mHighlightedPart;

	if (hit_part != LL_X_ARROW && hit_part != LL_Y_ARROW &&
		hit_part != LL_Z_ARROW && hit_part != LL_YZ_PLANE &&
		hit_part != LL_XZ_PLANE && hit_part != LL_XY_PLANE)
	{
		return true;
	}

	mHelpTextTimer.reset();
	sNumTimesHelpTextShown++;

	gSelectMgr.getGrid(mGridOrigin, mGridRotation, mGridScale);

	gSelectMgr.enableSilhouette(false);

	// We just started a drag, so save initial object positions
	gSelectMgr.saveSelectedObjectTransform(SELECT_ACTION_TYPE_MOVE);

	mManipPart = (EManipPart)hit_part;
	mMouseDownX = x;
	mMouseDownY = y;
	mMouseOutsideSlop = false;

	LLVector3 axis;

	LLSelectNode* selectNode = mObjectSelection->getFirstMoveableNode(true);

	if (!selectNode)
	{
		// Did not find the object in our selection... Oh well...
		llwarns << "Trying to translate an unselected object" << llendl;
		return true;
	}

	LLViewerObject* selected_object = selectNode->getObject();
	if (!selected_object)
	{
		// Somehow we lost the object !
		llwarns << "Translate manip lost the object, no selected object"
				<< llendl;
		gViewerWindowp->setCursor(UI_CURSOR_TOOLTRANSLATE);
		return true;
	}

	// Compute unit vectors for arrow hit and a plane through that vector
	bool axis_exists = getManipAxis(selected_object, mManipPart, axis);
	getManipNormal(selected_object, mManipPart, mManipNormal);

	LLVector3 select_center_agent = getPivotPoint();
	mSubdivisions = llclamp(getSubdivisionLevel(select_center_agent,
												axis_exists ? axis
															: LLVector3::z_axis,
												getMinGridScale()),
							sGridMinSubdivisionLevel,
							sGridMaxSubdivisionLevel);

	// If we clicked on a planar manipulator, recenter mouse cursor
	if (mManipPart >= LL_YZ_PLANE && mManipPart <= LL_XY_PLANE)
	{
		LLCoordGL mouse_pos;
		if (!gViewerCamera.projectPosAgentToScreen(select_center_agent, mouse_pos))
		{
			// mouse_pos may be nonsense
			llwarns << "Failed to project object center to screen" << llendl;
		}
		else if (getSnapToMouseCursor())
		{
			LLUI::setCursorPositionScreen(mouse_pos.mX, mouse_pos.mY);
			x = mouse_pos.mX;
			y = mouse_pos.mY;
		}
	}

	gSelectMgr.updateSelectionCenter();
	LLVector3d object_start_global =
		gAgent.getPosGlobalFromAgent(getPivotPoint());
	getMousePointOnPlaneGlobal(mDragCursorStartGlobal, x, y,
							   object_start_global, mManipNormal);
	mDragSelectionStartGlobal = object_start_global;
	mCopyMadeThisDrag = false;

	// Route future Mouse messages here preemptively (release on mouse up).
	setMouseCapture(true);

	return true;
}

bool LLManipTranslate::handleHover(S32 x, S32 y, MASK mask)
{
	// Translation tool only works if mouse button is down.
	// Bail out if mouse not down.
	if (!hasMouseCapture())
	{
		LL_DEBUGS("UserInput") << "hover handled by LLManipTranslate (inactive)"
							   << LL_ENDL;
		// Always show cursor
		gViewerWindowp->setCursor(UI_CURSOR_TOOLTRANSLATE);

		highlightManipulators(x, y);
		return true;
	}

	// Handle auto-rotation if necessary.
	constexpr F32 ROTATE_ANGLE_PER_SECOND = 30.f * DEG_TO_RAD;
	const S32 rotate_h_margin = gViewerWindowp->getWindowWidth() / 20;
	const F32 rotate_angle = ROTATE_ANGLE_PER_SECOND / gFPSClamped;
	bool rotated = false;

	// ...build mode moves camera about focus point
	if (mObjectSelection->getSelectType() != SELECT_TYPE_HUD)
	{
		if (x < rotate_h_margin)
		{
			gAgent.cameraOrbitAround(rotate_angle);
			rotated = true;
		}
		else if (x > gViewerWindowp->getWindowWidth() - rotate_h_margin)
		{
			gAgent.cameraOrbitAround(-rotate_angle);
			rotated = true;
		}
	}

	// Suppress processing if mouse hasn't actually moved. This may cause
	// problems if the camera moves outside of the rotation above.
	if (x == mLastHoverMouseX && y == mLastHoverMouseY && !rotated)
	{
		LL_DEBUGS("UserInput") << "hover handled by LLManipTranslate (mouse unmoved)"
							   << LL_ENDL;
		gViewerWindowp->setCursor(UI_CURSOR_TOOLTRANSLATE);
		return true;
	}
	mLastHoverMouseX = x;
	mLastHoverMouseY = y;

	// Suppress if mouse hasn't moved past the initial slop region. Reset once
	// we start moving
	if (!mMouseOutsideSlop)
	{
		if (abs(mMouseDownX - x) < MOUSE_DRAG_SLOP &&
			abs(mMouseDownY - y) < MOUSE_DRAG_SLOP)
		{
			LL_DEBUGS("UserInput") << "hover handled by LLManipTranslate (mouse inside slop)"
								   << LL_ENDL;
			gViewerWindowp->setCursor(UI_CURSOR_TOOLTRANSLATE);
			return true;
		}
		else	// ...Just went outside the slop region
		{
			mMouseOutsideSlop = true;
			// If holding down shift, leave behind a copy.
			if (mask == MASK_COPY)
			{
				// ...we are trying to make a copy
//MK
				if (!gRLenabled || !gRLInterface.mContainsRez)
				{
//mk
					gSelectMgr.selectDuplicate(LLVector3::zero, false);
					mCopyMadeThisDrag = true;

					// When we make the copy, we don't want to do any other
					// processing. If so, the object will also be moved, and
					// the copy will be offset.
					LL_DEBUGS("UserInput") << "hover handled by LLManipTranslate (made copy)"
										   << LL_ENDL;
					gViewerWindowp->setCursor(UI_CURSOR_TOOLTRANSLATE);
				}
			}
		}
	}

	LLVector3 axis_f;
	LLVector3d axis_d;

	// Pick the first object to constrain to grid w/ common origin. This is so
	// we do not screw up groups
	LLSelectNode* selectNode = mObjectSelection->getFirstMoveableNode(true);
	if (!selectNode)
	{
		// somehow we lost the object!
		llwarns << "Translate manip lost the object, no selectNode" << llendl;
		gViewerWindowp->setCursor(UI_CURSOR_TOOLTRANSLATE);
		return true;
	}

	LLViewerObject* object = selectNode->getObject();
	if (!object)
	{
		// somehow we lost the object!
		llwarns << "Translate manip lost the object, no object in selectNode"
				<< llendl;
		gViewerWindowp->setCursor(UI_CURSOR_TOOLTRANSLATE);
		return true;
	}

	// Compute unit vectors for arrow hit and a plane through that vector
	bool axis_exists = getManipAxis(object, mManipPart, axis_f);

	axis_d.set(axis_f);

	gSelectMgr.updateSelectionCenter();
	LLVector3d current_pos_global = gAgent.getPosGlobalFromAgent(getPivotPoint());

	mSubdivisions = llclamp(getSubdivisionLevel(getPivotPoint(), axis_f,
												getMinGridScale()),
							sGridMinSubdivisionLevel,
							sGridMaxSubdivisionLevel);

	// Project the cursor onto that plane
	LLVector3d relative_move;
	getMousePointOnPlaneGlobal(relative_move, x, y, current_pos_global,
							   mManipNormal);
	relative_move -= mDragCursorStartGlobal;

	// You can't move more than some distance from your original mousedown point.
	static LLCachedControl<bool> limit_drag_distance(gSavedSettings,
													 "LimitDragDistance");
	static LLCachedControl<F32> max_drag_dist(gSavedSettings,
											  "MaxDragDistance");
	if (limit_drag_distance &&
		relative_move.lengthSquared() > max_drag_dist * max_drag_dist)
	{
		LL_DEBUGS("UserInput") << "hover handled by LLManipTranslate (too far)"
							   << LL_ENDL;
		gViewerWindowp->setCursor(UI_CURSOR_NOLOCKED);
		return true;
	}

	F64 axis_magnitude = relative_move * axis_d;	// dot product
	LLVector3d cursor_point_snap_line;

	getMousePointOnPlaneGlobal(cursor_point_snap_line, x, y,
							   current_pos_global, mSnapOffsetAxis % axis_f);
	F64 off_axis_magnitude =
		axis_exists ? fabs((cursor_point_snap_line - current_pos_global) *
							LLVector3d(mSnapOffsetAxis))
					: 0.f;

	if (getSnapEnabled())
	{
		if (off_axis_magnitude > mSnapOffsetMeters)
		{
			mInSnapRegime = true;
			LLVector3 mouse_down_offset(mDragCursorStartGlobal -
										mDragSelectionStartGlobal);
			LLVector3 cursor_snap_agent =
				gAgent.getPosAgentFromGlobal(cursor_point_snap_line);
			if (!getSnapToMouseCursor())
			{
				cursor_snap_agent -= mouse_down_offset;
			}

			F32 cursor_grid_dist = (cursor_snap_agent - mGridOrigin) * axis_f;

			F32 snap_dist = getMinGridScale() / (2.f * mSubdivisions);
			F32 relative_snap_dist =
				fmodf(fabsf(cursor_grid_dist) + snap_dist,
					  getMinGridScale() / mSubdivisions);
			if (relative_snap_dist < snap_dist * 2.f)
			{
				if (cursor_grid_dist > 0.f)
				{
					cursor_grid_dist -= relative_snap_dist - snap_dist;
				}
				else
				{
					cursor_grid_dist += relative_snap_dist - snap_dist;
				}
			}

			F32 object_start_on_axis =
				(gAgent.getPosAgentFromGlobal(mDragSelectionStartGlobal) -
				 mGridOrigin) * axis_f;
			axis_magnitude = cursor_grid_dist - object_start_on_axis;
		}
		else if (mManipPart >= LL_YZ_PLANE && mManipPart <= LL_XY_PLANE)
		{
			// Subtract offset from object center
			LLVector3d cursor_point_global;
			getMousePointOnPlaneGlobal(cursor_point_global, x, y,
									   current_pos_global, mManipNormal);
			cursor_point_global -= mDragCursorStartGlobal -
								   mDragSelectionStartGlobal;

			// Snap to planar grid
			LLVector3 cursor_point_agent =
				gAgent.getPosAgentFromGlobal(cursor_point_global);
			LLVector3 camera_plane_projection = gViewerCamera.getAtAxis();
			camera_plane_projection -= projected_vec(camera_plane_projection,
													 mManipNormal);
			camera_plane_projection.normalize();
			LLVector3 camera_projected_dir = camera_plane_projection;
			camera_plane_projection.rotVec(~mGridRotation);
			camera_plane_projection.scaleVec(mGridScale);
			camera_plane_projection.abs();
			F32 max_grid_scale;
			if (camera_plane_projection.mV[VX] > camera_plane_projection.mV[VY] &&
				camera_plane_projection.mV[VX] > camera_plane_projection.mV[VZ])
			{
				max_grid_scale = mGridScale.mV[VX];
			}
			else if (camera_plane_projection.mV[VY] > camera_plane_projection.mV[VZ])
			{
				max_grid_scale = mGridScale.mV[VY];
			}
			else
			{
				max_grid_scale = mGridScale.mV[VZ];
			}

			F32 num_subdivisions = llclamp(getSubdivisionLevel(getPivotPoint(),
															   camera_projected_dir,
															   max_grid_scale),
										   sGridMinSubdivisionLevel,
										   sGridMaxSubdivisionLevel);

			F32 grid_scale_a;
			F32 grid_scale_b;
			LLVector3 cursor_point_grid = (cursor_point_agent - mGridOrigin) *
										  ~mGridRotation;

			switch (mManipPart)
			{
				case LL_YZ_PLANE:
					grid_scale_a = mGridScale.mV[VY] / num_subdivisions;
					grid_scale_b = mGridScale.mV[VZ] / num_subdivisions;
					cursor_point_grid.mV[VY] -= fmod(cursor_point_grid.mV[VY] +
													 grid_scale_a * 0.5f,
													 grid_scale_a) -
												grid_scale_a * 0.5f;
					cursor_point_grid.mV[VZ] -= fmod(cursor_point_grid.mV[VZ] +
													 grid_scale_b * 0.5f,
													 grid_scale_b) -
											    grid_scale_b * 0.5f;
					break;

				case LL_XZ_PLANE:
					grid_scale_a = mGridScale.mV[VX] / num_subdivisions;
					grid_scale_b = mGridScale.mV[VZ] / num_subdivisions;
					cursor_point_grid.mV[VX] -= fmod(cursor_point_grid.mV[VX] +
													 grid_scale_a * 0.5f,
													 grid_scale_a) -
												grid_scale_a * 0.5f;
					cursor_point_grid.mV[VZ] -= fmod(cursor_point_grid.mV[VZ] +
													 grid_scale_b * 0.5f,
													 grid_scale_b) -
												grid_scale_b * 0.5f;
					break;

				case LL_XY_PLANE:
					grid_scale_a = mGridScale.mV[VX] / num_subdivisions;
					grid_scale_b = mGridScale.mV[VY] / num_subdivisions;
					cursor_point_grid.mV[VX] -= fmod(cursor_point_grid.mV[VX] +
													 grid_scale_a * 0.5f,
													 grid_scale_a) -
												grid_scale_a * 0.5f;
					cursor_point_grid.mV[VY] -= fmod(cursor_point_grid.mV[VY] +
													 grid_scale_b * 0.5f,
													 grid_scale_b) -
												grid_scale_b * 0.5f;
					break;

				default:
					break;
			}
			cursor_point_agent = cursor_point_grid * mGridRotation +
								 mGridOrigin;
			relative_move.set(cursor_point_agent -
							  gAgent.getPosAgentFromGlobal(mDragSelectionStartGlobal));
			mInSnapRegime = true;
		}
		else
		{
			mInSnapRegime = false;
		}
	}
	else
	{
		mInSnapRegime = false;
	}

	// Clamp to arrow direction
	// *FIX: does this apply anymore?
	if (!axis_exists)
	{
		axis_magnitude = relative_move.normalize();
		axis_d.set(relative_move);
		axis_d.normalize();
		axis_f.set(axis_d);
	}

	// Scalar multiplications
	LLVector3d clamped_relative_move = axis_magnitude * axis_d;
	LLVector3 clamped_relative_move_f = (F32)axis_magnitude * axis_f;

	for (LLObjectSelection::iterator iter = mObjectSelection->begin(),
									 end = mObjectSelection->end();
		 iter != end; ++iter)
	{
		LLSelectNode* selectNode = *iter;
		LLViewerObject* object = selectNode->getObject();
		if (!object)
		{
			llwarns << "NULL selected object !" << llendl;
			continue;
		}

		// Only apply motion to root objects and objects selected as
		// "individual".
		if (!object->isRootEdit() && !selectNode->mIndividualSelection)
		{
			continue;
		}

		if (!object->isRootEdit())
		{
			// child objects should not update if parent is selected
			LLViewerObject* editable_root = (LLViewerObject*)object->getParent();
			if (editable_root->isSelected())
			{
				// we will be moved properly by our parent, so skip
				continue;
			}
		}

		LLViewerObject* root_object = object->getRootEdit();
		if (object->permMove() && !object->isPermanentEnforced() &&
			(!root_object || !root_object->isPermanentEnforced()))
		{
			// handle attachments in local space
			if (object->isAttachment() && object->mDrawable.notNull())
			{
				// calculate local version of relative move
				LLQuaternion obj_world_rot =
					object->mDrawable->mXform.getParent()->getWorldRotation();
				obj_world_rot.transpose();

				LLVector3 old_position_local = object->getPosition();
				LLVector3 new_position_local = selectNode->mSavedPositionLocal +
											   clamped_relative_move_f * obj_world_rot;

				// RN: I forget, but we need to do this because of snapping
				// which doesn't often result in position changes even when the
				// mouse moves
				object->setPositionLocal(new_position_local);
				rebuild(object);
				gAgentAvatarp->clampAttachmentPositions();
				new_position_local = object->getPosition();

				if (selectNode->mIndividualSelection)
				{
					// Counter-translate child objects if we are moving the
					// root as an individual
					object->resetChildrenPosition(old_position_local -
												  new_position_local, true);
				}
			}
			else
			{
				// Compute new position to send to simulators, but don't set
				// it yet. We need the old position to know which simulator to
				// send the move message to.
				LLVector3d new_pos_global = selectNode->mSavedPositionGlobal +
											clamped_relative_move;

				// Do not let object centers go too far underground
				F64 min_height = gWorld.getMinAllowedZ(object);
				if (new_pos_global.mdV[VZ] < min_height)
				{
					new_pos_global.mdV[VZ] = min_height;
				}

				// For safety, cap heights where objects can be dragged
				if (new_pos_global.mdV[VZ] > MAX_OBJECT_Z)
				{
					new_pos_global.mdV[VZ] = MAX_OBJECT_Z;
				}

				// Grass is always drawn on the ground, so clamp its position
				// to the ground
				if (object->getPCode() == LL_PCODE_LEGACY_GRASS)
				{
					new_pos_global.mdV[VZ] =
						gWorld.resolveLandHeightGlobal(new_pos_global) + 1.f;
				}

				if (object->isRootEdit())
				{
					new_pos_global =
						gWorld.clipToVisibleRegions(object->getPositionGlobal(),
													new_pos_global);
				}

				// PR: Only update if changed
				LLVector3 old_position_agent = object->getPositionAgent();
				LLVector3 new_position_agent =
					gAgent.getPosAgentFromGlobal(new_pos_global);
				if (object->isRootEdit())
				{
					// Finally, move parent object after children have
					// calculated new offsets
					object->setPositionAgent(new_position_agent);
					rebuild(object);
				}
				else
				{
					LLViewerObject* root_object = object->getRootEdit();
					new_position_agent -= root_object->getPositionAgent();
					new_position_agent = new_position_agent *
										 ~root_object->getRotation();
					object->setPositionParent(new_position_agent, false);
					rebuild(object);
				}

				if (selectNode->mIndividualSelection)
				{
					// Counter-translate child objects if we are moving the
					// root as an individual
					object->resetChildrenPosition(old_position_agent -
												  new_position_agent, true);
				}
			}
			selectNode->mLastPositionLocal  = object->getPosition();
		}
	}

	gSelectMgr.updateSelectionCenter();
	gAgent.clearFocusObject();
	dialog_refresh_all();		// is this necessary ?

	LL_DEBUGS("UserInput") << "Hover handled by LLManipTranslate (active)"
						   << LL_ENDL;
	gViewerWindowp->setCursor(UI_CURSOR_TOOLTRANSLATE);
	return true;
}

void LLManipTranslate::highlightManipulators(S32 x, S32 y)
{
	mHighlightedPart = LL_NO_PART;

	if (!mObjectSelection->getObjectCount())
	{
		return;
	}

	LLMatrix4 proj_mat = gViewerCamera.getProjection();
	LLMatrix4 model_view = gViewerCamera.getModelview();

	LLVector3 object_position = getPivotPoint();

	LLVector3 grid_origin;
	LLVector3 grid_scale;
	LLQuaternion grid_rotation;

	gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);

	LLVector3 relative_camera_dir;

	LLMatrix4 transform;

	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		relative_camera_dir = LLVector3::x_axis * ~grid_rotation;
		LLVector4 translation(object_position);
		transform.initRotTrans(grid_rotation, translation);
		LLMatrix4 cfr(OGL_TO_CFR_ROTATION);
		transform *= cfr;
		LLMatrix4 window_scale;
		F32 zoom_level = 2.f * gAgent.mHUDCurZoom;
		window_scale.initAll(LLVector3(zoom_level / gViewerCamera.getAspect(),
									   zoom_level, 0.f),
							 LLQuaternion::DEFAULT, LLVector3::zero);
		transform *= window_scale;
	}
	else
	{
		relative_camera_dir = (object_position - gViewerCamera.getOrigin()) *
							  ~grid_rotation;
		relative_camera_dir.normalize();

		transform.initRotTrans(grid_rotation, LLVector4(object_position));
		transform *= model_view;
		transform *= proj_mat;
	}

	S32 num_manips = 0;

	// edges
	mManipulatorVertices[num_manips++] =
		LLVector4(mArrowLengthMeters * MANIP_HOTSPOT_START, 0.f, 0.f, 1.f);
	mManipulatorVertices[num_manips++] =
		LLVector4(mArrowLengthMeters * MANIP_HOTSPOT_END, 0.f, 0.f, 1.f);

	mManipulatorVertices[num_manips++] =
		LLVector4(0.f, mArrowLengthMeters * MANIP_HOTSPOT_START, 0.f, 1.f);
	mManipulatorVertices[num_manips++] =
		LLVector4(0.f, mArrowLengthMeters * MANIP_HOTSPOT_END, 0.f, 1.f);

	mManipulatorVertices[num_manips++] =
		LLVector4(0.f, 0.f, mArrowLengthMeters * MANIP_HOTSPOT_START, 1.f);
	mManipulatorVertices[num_manips++] =
		LLVector4(0.f, 0.f, mArrowLengthMeters * MANIP_HOTSPOT_END, 1.f);

	mManipulatorVertices[num_manips++] =
		LLVector4(mArrowLengthMeters * -MANIP_HOTSPOT_START, 0.f, 0.f, 1.f);
	mManipulatorVertices[num_manips++] =
		LLVector4(mArrowLengthMeters * -MANIP_HOTSPOT_END, 0.f, 0.f, 1.f);

	mManipulatorVertices[num_manips++] =
		LLVector4(0.f, mArrowLengthMeters * -MANIP_HOTSPOT_START, 0.f, 1.f);
	mManipulatorVertices[num_manips++] =
		LLVector4(0.f, mArrowLengthMeters * -MANIP_HOTSPOT_END, 0.f, 1.f);

	mManipulatorVertices[num_manips++] =
		LLVector4(0.f, 0.f, mArrowLengthMeters * -MANIP_HOTSPOT_START, 1.f);
	mManipulatorVertices[num_manips++] =
		LLVector4(0.f, 0.f, mArrowLengthMeters * -MANIP_HOTSPOT_END, 1.f);

	S32 num_arrow_manips = num_manips;

	// planar manipulators
	bool planar_manip_yz_visible = false;
	bool planar_manip_xz_visible = false;
	bool planar_manip_xy_visible = false;

	constexpr F32 PLANE_FACTOR1 = 1.f - PLANE_TICK_SIZE * 0.5f;
	constexpr F32 PLANE_FACTOR2 = 1.f + PLANE_TICK_SIZE * 0.5f;

	mManipulatorVertices[num_manips] =
		LLVector4(0.f, mPlaneManipOffsetMeters * PLANE_FACTOR1,
				 mPlaneManipOffsetMeters * PLANE_FACTOR1, 1.f);
	mManipulatorVertices[num_manips++].scaleVec(mPlaneManipPositions);

	mManipulatorVertices[num_manips] =
		LLVector4(0.f, mPlaneManipOffsetMeters * PLANE_FACTOR2,
				  mPlaneManipOffsetMeters * PLANE_FACTOR2, 1.f);
	mManipulatorVertices[num_manips++].scaleVec(mPlaneManipPositions);

	if (fabsf(relative_camera_dir.mV[VX]) > MIN_PLANE_MANIP_DOT_PRODUCT)
	{
		planar_manip_yz_visible = true;
	}

	mManipulatorVertices[num_manips] =
		LLVector4(mPlaneManipOffsetMeters * PLANE_FACTOR1, 0.f,
				  mPlaneManipOffsetMeters * PLANE_FACTOR1, 1.f);
	mManipulatorVertices[num_manips++].scaleVec(mPlaneManipPositions);

	mManipulatorVertices[num_manips] =
		LLVector4(mPlaneManipOffsetMeters * PLANE_FACTOR2, 0.f,
				  mPlaneManipOffsetMeters * PLANE_FACTOR2, 1.f);
	mManipulatorVertices[num_manips++].scaleVec(mPlaneManipPositions);

	if (fabsf(relative_camera_dir.mV[VY]) > MIN_PLANE_MANIP_DOT_PRODUCT)
	{
		planar_manip_xz_visible = true;
	}

	mManipulatorVertices[num_manips] =
		LLVector4(mPlaneManipOffsetMeters * PLANE_FACTOR1,
				  mPlaneManipOffsetMeters * PLANE_FACTOR1, 0.f, 1.f);
	mManipulatorVertices[num_manips++].scaleVec(mPlaneManipPositions);

	mManipulatorVertices[num_manips] =
		LLVector4(mPlaneManipOffsetMeters * PLANE_FACTOR2,
				  mPlaneManipOffsetMeters * PLANE_FACTOR2, 0.f, 1.f);
	mManipulatorVertices[num_manips++].scaleVec(mPlaneManipPositions);

	if (fabsf(relative_camera_dir.mV[VZ]) > MIN_PLANE_MANIP_DOT_PRODUCT)
	{
		planar_manip_xy_visible = true;
	}

	// Project up to 9 manipulators to screen space 2*X, 2*Y, 2*Z, 3*planes
	std::vector<ManipulatorHandle> projected_manipulators;
	projected_manipulators.reserve(9);

	for (S32 i = 0; i < num_arrow_manips; i += 2)
	{
		LLVector4 projected_start = mManipulatorVertices[i] * transform;
		projected_start = projected_start / projected_start.mV[VW];

		LLVector4 projected_end = mManipulatorVertices[i + 1] * transform;
		projected_end = projected_end / projected_end.mV[VW];

		ManipulatorHandle projected_manip(LLVector3(projected_start.mV[VX],
													projected_start.mV[VY],
													projected_start.mV[VZ]),
										  LLVector3(projected_end.mV[VX],
													projected_end.mV[VY],
													projected_end.mV[VZ]),
										  MANIPULATOR_IDS[i / 2],
										  10.f); // 10 pixel hotspot for arrows
		projected_manipulators.push_back(projected_manip);
	}

	if (planar_manip_yz_visible)
	{
		S32 i = num_arrow_manips;
		LLVector4 projected_start = mManipulatorVertices[i] * transform;
		projected_start = projected_start / projected_start.mV[VW];

		LLVector4 projected_end = mManipulatorVertices[i + 1] * transform;
		projected_end = projected_end / projected_end.mV[VW];

		ManipulatorHandle projected_manip(LLVector3(projected_start.mV[VX],
													projected_start.mV[VY],
													projected_start.mV[VZ]),
										  LLVector3(projected_end.mV[VX],
													projected_end.mV[VY],
													projected_end.mV[VZ]),
										  MANIPULATOR_IDS[i / 2],
										  // 20 pixels for planar manipulators
										  20.f);
		projected_manipulators.push_back(projected_manip);
	}

	if (planar_manip_xz_visible)
	{
		S32 i = num_arrow_manips + 2;
		LLVector4 projected_start = mManipulatorVertices[i] * transform;
		projected_start = projected_start / projected_start.mV[VW];

		LLVector4 projected_end = mManipulatorVertices[i + 1] * transform;
		projected_end = projected_end / projected_end.mV[VW];

		ManipulatorHandle projected_manip(LLVector3(projected_start.mV[VX],
													projected_start.mV[VY],
													projected_start.mV[VZ]),
										  LLVector3(projected_end.mV[VX],
													projected_end.mV[VY],
													projected_end.mV[VZ]),
										  MANIPULATOR_IDS[i / 2],
										  // 20 pixels for planar manipulators
										  20.f);
		projected_manipulators.push_back(projected_manip);
	}

	if (planar_manip_xy_visible)
	{
		S32 i = num_arrow_manips + 4;
		LLVector4 projected_start = mManipulatorVertices[i] * transform;
		projected_start = projected_start / projected_start.mV[VW];

		LLVector4 projected_end = mManipulatorVertices[i + 1] * transform;
		projected_end = projected_end / projected_end.mV[VW];

		ManipulatorHandle projected_manip(LLVector3(projected_start.mV[VX],
													projected_start.mV[VY],
													projected_start.mV[VZ]),
										  LLVector3(projected_end.mV[VX],
													projected_end.mV[VY],
													projected_end.mV[VZ]),
										  MANIPULATOR_IDS[i / 2],
										  // 20 pixels for planar manipulators
										  20.f);
		projected_manipulators.push_back(projected_manip);
	}

	LLVector2 manip_start_2d;
	LLVector2 manip_end_2d;
	LLVector2 manip_dir;
	F32 half_width = gViewerWindowp->getWindowWidth() * 0.5f;
	F32 half_height = gViewerWindowp->getWindowHeight() * 0.5f;
	LLVector2 mousePos((F32)x - half_width, (F32)y - half_height);
	LLVector2 mouse_delta;

	// Keep order consistent with insertion via stable_sort
	std::stable_sort(projected_manipulators.begin(),
					 projected_manipulators.end(), ClosestToCamera());

	std::vector<ManipulatorHandle>::iterator it = projected_manipulators.begin();
	for ( ; it != projected_manipulators.end(); ++it)
	{
		ManipulatorHandle& manipulator = *it;
		{
			manip_start_2d.set(manipulator.mStartPosition.mV[VX] * half_width,
							   manipulator.mStartPosition.mV[VY] * half_height);
			manip_end_2d.set(manipulator.mEndPosition.mV[VX] * half_width,
							 manipulator.mEndPosition.mV[VY] * half_height);
			manip_dir = manip_end_2d - manip_start_2d;

			mouse_delta = mousePos - manip_start_2d;

			F32 manip_length = manip_dir.normalize();

			F32 mouse_pos_manip = mouse_delta * manip_dir;
			F32 mouse_dist_manip_squared = mouse_delta.lengthSquared() -
										   mouse_pos_manip * mouse_pos_manip;

			if (mouse_pos_manip > 0.f &&
				mouse_pos_manip < manip_length &&
				mouse_dist_manip_squared < manipulator.mHotSpotRadius *
										   manipulator.mHotSpotRadius)
			{
				mHighlightedPart = manipulator.mManipID;
				break;
			}
		}
	}
}

F32 LLManipTranslate::getMinGridScale()
{
	F32 scale;
	switch (mManipPart)
	{
		case LL_NO_PART:
		default:
			scale = 1.f;
			break;

		case LL_X_ARROW:
			scale = mGridScale.mV[VX];
			break;

		case LL_Y_ARROW:
			scale = mGridScale.mV[VY];
			break;

		case LL_Z_ARROW:
			scale = mGridScale.mV[VZ];
			break;

		case LL_YZ_PLANE:
			scale = llmin(mGridScale.mV[VY], mGridScale.mV[VZ]);
			break;

		case LL_XZ_PLANE:
			scale = llmin(mGridScale.mV[VX], mGridScale.mV[VZ]);
			break;

		case LL_XY_PLANE:
			scale = llmin(mGridScale.mV[VX], mGridScale.mV[VY]);
	}

	return scale;
}

bool LLManipTranslate::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// First, perform normal processing in case this was a quick-click
	handleHover(x, y, mask);

	if (hasMouseCapture())
	{
		// make sure arrow colors go back to normal
		mManipPart = LL_NO_PART;
		gSelectMgr.enableSilhouette(true);

		// Might have missed last update due to UPDATE_DELAY timing.
		gSelectMgr.sendMultipleUpdate(UPD_POSITION);

		mInSnapRegime = false;
		gSelectMgr.saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
#if 0
		gAgent.setObjectTracking(gSavedSettings.getBool("TrackFocusObject"));
#endif
	}

	return LLManip::handleMouseUp(x, y, mask);
}

void LLManipTranslate::render()
{
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		F32 zoom = gAgent.mHUDCurZoom;
		gGL.scalef(zoom, zoom, zoom);
	}
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		renderGuidelines();
	}
	{
		renderTranslationHandles();
		renderSnapGuides();
	}
	gGL.popMatrix();

	renderText();
}

void LLManipTranslate::renderSnapGuides()
{
	if (!getSnapEnabled())
	{
		return;
	}

	F32 max_subdivisions = sGridMaxSubdivisionLevel;
	static LLCachedControl<F32> grid_opacity(gSavedSettings, "GridOpacity");
	F32 line_alpha = grid_opacity;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	LLGLDepthTest gls_depth(GL_TRUE);
	LLGLDisable gls_cull(GL_CULL_FACE);
	LLVector3 translate_axis;

	if (mManipPart == LL_NO_PART)
	{
		return;
	}

	LLSelectNode *first_node = mObjectSelection->getFirstMoveableNode(true);
	if (!first_node)
	{
		return;
	}

	updateGridSettings();

	F32 smallest_grid_unit_scale = getMinGridScale() / max_subdivisions;
	LLVector3 grid_origin;
	LLVector3 grid_scale;
	LLQuaternion grid_rotation;

	gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);
	LLVector3 saved_selection_center = getSavedPivotPoint();
	LLVector3 selection_center = getPivotPoint();

	LLViewerObject* first_object = first_node->getObject();

	// Pick appropriate projection plane for snap rulers according to relative
	// camera position
	if (mManipPart >= LL_X_ARROW && mManipPart <= LL_Z_ARROW)
	{
		LLVector3 normal;
		LLColor4 inner_color;
		LLManip::EManipPart temp_manip = mManipPart;
		switch (mManipPart)
		{
			case LL_X_ARROW:
				normal.set(1, 0, 0);
				inner_color.set(0, 1, 1, line_alpha);
				mManipPart = LL_YZ_PLANE;
				break;
			case LL_Y_ARROW:
				normal.set(0, 1, 0);
				inner_color.set(1, 0, 1, line_alpha);
				mManipPart = LL_XZ_PLANE;
				break;
			case LL_Z_ARROW:
				normal.set(0, 0, 1);
				inner_color.set(1, 1, 0, line_alpha);
				mManipPart = LL_XY_PLANE;
				break;
			default:
				break;
		}

		highlightIntersection(normal, selection_center, grid_rotation,
							  inner_color);
		mManipPart = temp_manip;
		getManipAxis(first_object, mManipPart, translate_axis);

		LLVector3 at_axis_abs;
		if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
		{
			at_axis_abs = LLVector3::x_axis * ~grid_rotation;
		}
		else
		{
			at_axis_abs = saved_selection_center - gViewerCamera.getOrigin();
			at_axis_abs.normalize();

			at_axis_abs = at_axis_abs * ~grid_rotation;
		}
		at_axis_abs.abs();

		if (at_axis_abs.mV[VX] > at_axis_abs.mV[VY] &&
			at_axis_abs.mV[VX] > at_axis_abs.mV[VZ])
		{
			if (mManipPart == LL_Y_ARROW)
			{
				mSnapOffsetAxis = LLVector3::z_axis;
			}
			else if (mManipPart == LL_Z_ARROW)
			{
				mSnapOffsetAxis = LLVector3::y_axis;
			}
			else if (at_axis_abs.mV[VY] > at_axis_abs.mV[VZ])
			{
				mSnapOffsetAxis = LLVector3::z_axis;
			}
			else
			{
				mSnapOffsetAxis = LLVector3::y_axis;
			}
		}
		else if (at_axis_abs.mV[VY] > at_axis_abs.mV[VZ])
		{
			if (mManipPart == LL_X_ARROW)
			{
				mSnapOffsetAxis = LLVector3::z_axis;
			}
			else if (mManipPart == LL_Z_ARROW)
			{
				mSnapOffsetAxis = LLVector3::x_axis;
			}
			else if (at_axis_abs.mV[VX] > at_axis_abs.mV[VZ])
			{
				mSnapOffsetAxis = LLVector3::z_axis;
			}
			else
			{
				mSnapOffsetAxis = LLVector3::x_axis;
			}
		}
		else if (mManipPart == LL_X_ARROW)
		{
			mSnapOffsetAxis = LLVector3::y_axis;
		}
		else if (mManipPart == LL_Y_ARROW)
		{
			mSnapOffsetAxis = LLVector3::x_axis;
		}
		else if (at_axis_abs.mV[VX] > at_axis_abs.mV[VY])
		{
			mSnapOffsetAxis = LLVector3::y_axis;
		}
		else
		{
			mSnapOffsetAxis = LLVector3::x_axis;
		}

		mSnapOffsetAxis = mSnapOffsetAxis * grid_rotation;

		F32 guide_size_meters;

		if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
		{
			guide_size_meters = 1.f / gAgent.mHUDCurZoom;
			mSnapOffsetMeters = mArrowLengthMeters * 1.5f;
		}
		else
		{
			LLVector3 cam_to_selection = getPivotPoint() -
										 gViewerCamera.getOrigin();
			F32 current_range = cam_to_selection.normalize();
			guide_size_meters = SNAP_GUIDE_SCREEN_SIZE *
								gViewerWindowp->getWindowHeight() *
								current_range /
								gViewerCamera.getPixelMeterRatio();

			F32 fraction_of_fov = mAxisArrowLength /
								  (F32)gViewerCamera.getViewHeightInPixels();
			// in radians:
			F32 apparent_angle = fraction_of_fov * gViewerCamera.getView();
			F32 offset_at_camera = tanf(apparent_angle) * 1.5f;
			F32 range =
				dist_vec(gAgent.getPosAgentFromGlobal(first_node->mSavedPositionGlobal),
						 gViewerCamera.getOrigin());
			mSnapOffsetMeters = range * offset_at_camera;
		}

		LLVector3 tick_start;
		LLVector3 tick_end;

		// how far away from grid origin is the selection along the axis of
		// translation ?
		F32 dist_grid_axis = (selection_center - mGridOrigin) * translate_axis;
		// find distance to nearest smallest grid unit
		F32 offset_nearest_grid_unit = fmodf(dist_grid_axis,
											 smallest_grid_unit_scale);
		// how many smallest grid units are we away from largest grid scale?
		S32 sub_div_offset =
			ll_round(fmod(dist_grid_axis - offset_nearest_grid_unit,
						  getMinGridScale() / sGridMinSubdivisionLevel) /
						  smallest_grid_unit_scale);
		S32 num_ticks_per_side = llmax(1,
									   llfloor(0.5f * guide_size_meters /
											   smallest_grid_unit_scale));

		LLGLDepthTest gls_depth(GL_FALSE);

		for (S32 pass = 0; pass < 3; ++pass)
		{
			LLColor4 line_color = setupSnapGuideRenderPass(pass);

			gGL.begin(LLRender::LINES);
			{
				LLVector3 line_start = selection_center +
									   (mSnapOffsetMeters * mSnapOffsetAxis) +
									   (translate_axis *
										(guide_size_meters * 0.5f +
										 offset_nearest_grid_unit));
				LLVector3 line_end = selection_center +
									 mSnapOffsetMeters * mSnapOffsetAxis -
									 translate_axis *
									 (guide_size_meters * 0.5f +
									  offset_nearest_grid_unit);
				LLVector3 line_mid = (line_start + line_end) * 0.5f;

				gGL.color4f(line_color.mV[VX], line_color.mV[VY],
							line_color.mV[VZ], line_color.mV[VW] * 0.2f);
				gGL.vertex3fv(line_start.mV);
				gGL.color4f(line_color.mV[VX], line_color.mV[VY],
							line_color.mV[VZ], line_color.mV[VW]);
				gGL.vertex3fv(line_mid.mV);
				gGL.vertex3fv(line_mid.mV);
				gGL.color4f(line_color.mV[VX], line_color.mV[VY],
							line_color.mV[VZ], line_color.mV[VW] * 0.2f);
				gGL.vertex3fv(line_end.mV);

				line_start.set(selection_center +
							   mSnapOffsetAxis * -mSnapOffsetMeters +
							   translate_axis * guide_size_meters * 0.5f);
				line_end.set(selection_center +
							 mSnapOffsetAxis * -mSnapOffsetMeters -
							 translate_axis * guide_size_meters * 0.5f);
				line_mid = (line_start + line_end) * 0.5f;

				gGL.color4f(line_color.mV[VX], line_color.mV[VY],
							line_color.mV[VZ], line_color.mV[VW] * 0.2f);
				gGL.vertex3fv(line_start.mV);
				gGL.color4f(line_color.mV[VX], line_color.mV[VY],
							line_color.mV[VZ], line_color.mV[VW]);
				gGL.vertex3fv(line_mid.mV);
				gGL.vertex3fv(line_mid.mV);
				gGL.color4f(line_color.mV[VX], line_color.mV[VY],
							line_color.mV[VZ], line_color.mV[VW] * 0.2f);
				gGL.vertex3fv(line_end.mV);

				for (S32 i = -num_ticks_per_side; i <= num_ticks_per_side; ++i)
				{
					tick_start = selection_center +
								 translate_axis *
								 (smallest_grid_unit_scale * (F32)i -
								  offset_nearest_grid_unit);

#if 0				// No need to check this condition to prevent tick position
					// scaling (FIX MAINT-5207/5208)
					F32 cur_subdivisions =
						llclamp(getSubdivisionLevel(tick_start, translate_axis,
													getMinGridScale()),
								sGridMinSubdivisionLevel,
								sGridMaxSubdivisionLevel);

					if (fmodf((F32)(i + sub_div_offset),
							  max_subdivisions / cur_subdivisions) != 0.f)
					{
						continue;
					}
#endif

					// Add in off-axis offset
					tick_start += (mSnapOffsetAxis * mSnapOffsetMeters);

					F32 tick_scale = 1.f;
					for (F32 division_level = max_subdivisions;
						 division_level >= sGridMinSubdivisionLevel;
						 division_level *= 0.5f)
					{
						if (fmodf((F32)(i + sub_div_offset),
										division_level) == 0.f)
						{
							break;
						}
						tick_scale *= 0.7f;
					}

					tick_end = tick_start +
							   mSnapOffsetAxis * mSnapOffsetMeters *
							   tick_scale;

					gGL.color4f(line_color.mV[VX], line_color.mV[VY],
								line_color.mV[VZ], line_color.mV[VW]);
					gGL.vertex3fv(tick_start.mV);
					gGL.vertex3fv(tick_end.mV);

					tick_start = selection_center +
								 mSnapOffsetAxis * -mSnapOffsetMeters +
								 translate_axis *
								 (getMinGridScale() / (F32)(max_subdivisions) *
								  (F32)i - offset_nearest_grid_unit);
					tick_end = tick_start -
							   mSnapOffsetAxis * mSnapOffsetMeters *
							   tick_scale;

					gGL.vertex3fv(tick_start.mV);
					gGL.vertex3fv(tick_end.mV);
				}
			}
			gGL.end();

			if (mInSnapRegime)
			{
				LLVector3 line_start = selection_center -
									   mSnapOffsetAxis * mSnapOffsetMeters;
				LLVector3 line_end = selection_center +
									 mSnapOffsetAxis * mSnapOffsetMeters;

				gGL.begin(LLRender::LINES);
				{
					gGL.color4f(line_color.mV[VX], line_color.mV[VY],
								line_color.mV[VZ], line_color.mV[VW]);

					gGL.vertex3fv(line_start.mV);
					gGL.vertex3fv(line_end.mV);
				}
				gGL.end();

				// draw snap guide arrow
				gGL.begin(LLRender::TRIANGLES);
				{
					gGL.color4f(line_color.mV[VX], line_color.mV[VY],
								line_color.mV[VZ], line_color.mV[VW]);

					LLVector3 arrow_dir;
					LLVector3 arrow_span = translate_axis;

					arrow_dir = -mSnapOffsetAxis;
					gGL.vertex3fv((line_start + arrow_dir * mConeSize *
								   SNAP_ARROW_SCALE).mV);
					gGL.vertex3fv((line_start + arrow_span * mConeSize *
								   SNAP_ARROW_SCALE).mV);
					gGL.vertex3fv((line_start - arrow_span * mConeSize *
								   SNAP_ARROW_SCALE).mV);

					arrow_dir = mSnapOffsetAxis;
					gGL.vertex3fv((line_end + arrow_dir * mConeSize *
								   SNAP_ARROW_SCALE).mV);
					gGL.vertex3fv((line_end + arrow_span * mConeSize *
								   SNAP_ARROW_SCALE).mV);
					gGL.vertex3fv((line_end - arrow_span * mConeSize *
								   SNAP_ARROW_SCALE).mV);
				}
				gGL.end();
			}
		}

		sub_div_offset = ll_round(fmod(dist_grid_axis - offset_nearest_grid_unit,
									   getMinGridScale() * 32.f) /
								  smallest_grid_unit_scale);

		LLVector2 screen_translate_axis(fabsf(translate_axis *
											  gViewerCamera.getLeftAxis()),
										fabsf(translate_axis *
											  gViewerCamera.getUpAxis()));
		screen_translate_axis.normalize();

		S32 tick_label_spacing = ll_round(screen_translate_axis *
										  sTickLabelSpacing);

		// Render tickmark values
		for (S32 i = -num_ticks_per_side; i <= num_ticks_per_side; ++i)
		{
			LLVector3 tick_pos = selection_center +
								 translate_axis *
								 (smallest_grid_unit_scale * (F32)i -
								  offset_nearest_grid_unit);
			F32 alpha = line_alpha *
						(1.f - (F32)abs(i) / (F32)num_ticks_per_side * 0.5f);

			F32 tick_scale = 1.f;
			for (F32 division_level = max_subdivisions;
				 division_level >= sGridMinSubdivisionLevel;
				 division_level /= 2.f)
			{
				if (fmodf((F32)(i + sub_div_offset), division_level) == 0.f)
				{
					break;
				}
				tick_scale *= 0.7f;
			}

			if (fmodf((F32)(i + sub_div_offset),
					  max_subdivisions / llmin(sGridMaxSubdivisionLevel,
											   getSubdivisionLevel(tick_pos,
																   translate_axis,
																   getMinGridScale(),
																   tick_label_spacing))) == 0.f)
			{
				F32 snap_offset_meters;

				if (mSnapOffsetAxis * gViewerCamera.getUpAxis() > 0.f)
				{
					snap_offset_meters = mSnapOffsetMeters;
				}
				else
				{
					snap_offset_meters = -mSnapOffsetMeters;
				}
				LLVector3 text_origin = selection_center +
										translate_axis *
										(smallest_grid_unit_scale * (F32)i -
										 offset_nearest_grid_unit) +
										mSnapOffsetAxis * snap_offset_meters *
										(1.f + tick_scale);

				LLVector3 tick_offset = (tick_pos - mGridOrigin) * ~mGridRotation;
				F32 offset_val = 0.5f *
								 tick_offset.mV[ARROW_TO_AXIS[mManipPart]] /
								 getMinGridScale();
				EGridMode grid_mode = gSelectMgr.getGridMode();
				F32 text_highlight = 0.8f;
				if (mInSnapRegime &&
					i - ll_round(offset_nearest_grid_unit /
								 smallest_grid_unit_scale) == 0)
				{
					text_highlight = 1.f;
				}

				if (grid_mode == GRID_MODE_WORLD)
				{
					// Rescale units to meters from multiple of grid scale
					offset_val *= 2.f * grid_scale[ARROW_TO_AXIS[mManipPart]];
					renderTickValue(text_origin, offset_val, std::string("m"),
									LLColor4(text_highlight, text_highlight,
											 text_highlight, alpha));
				}
				else
				{
					renderTickValue(text_origin, offset_val, std::string("x"),
									LLColor4(text_highlight, text_highlight,
											 text_highlight, alpha));
				}
			}
		}
		if (mObjectSelection->getSelectType() != SELECT_TYPE_HUD)
		{
			// Render helpful text
			static const LLFontGL* big_fontp = LLFontGL::getFontSansSerif();
			static const std::string help_text1("Move mouse cursor over ruler to snap");
			static const std::string help_text2("to snap to grid");
			static const F32 text1_offset = -0.5f *
											big_fontp->getWidthF32(help_text1);
			static const F32 text2_offset = -0.5f *
											big_fontp->getWidthF32(help_text2);
			static const LLWString wtext1 = utf8str_to_wstring(help_text1);
			static const LLWString wtext2 = utf8str_to_wstring(help_text2);

			if (sNumTimesHelpTextShown < sMaxTimesShowHelpText &&
				mHelpTextTimer.getElapsedTimeF32() <
					sHelpTextVisibleTime + sHelpTextFadeTime)
			{
				F32 snap_offset_meters_up;
				if (mSnapOffsetAxis * gViewerCamera.getUpAxis() > 0.f)
				{
					snap_offset_meters_up = mSnapOffsetMeters;
				}
				else
				{
					snap_offset_meters_up = -mSnapOffsetMeters;
				}

				bool is_hud =
					mObjectSelection->getSelectType() == SELECT_TYPE_HUD;

				LLVector3 selection_center_start = getSavedPivotPoint();
				LLVector3 help_text_pos = selection_center_start +
										  snap_offset_meters_up * 3.f *
										  mSnapOffsetAxis;

				LLColor4 help_text_color = LLColor4::white;
				help_text_color.mV[VALPHA] =
					clamp_rescale(mHelpTextTimer.getElapsedTimeF32(),
								  sHelpTextVisibleTime,
								  sHelpTextVisibleTime + sHelpTextFadeTime,
								  line_alpha, 0.f);

				hud_render_text(wtext1, help_text_pos, *big_fontp,
								LLFontGL::NORMAL, text1_offset, 3.f,
								help_text_color, is_hud);

				help_text_pos -= gViewerCamera.getUpAxis() *
								 mSnapOffsetMeters * 0.2f;

				hud_render_text(wtext2, help_text_pos, *big_fontp,
								LLFontGL::NORMAL, text2_offset, 3.f,
								help_text_color, is_hud);
			}
		}
	}
	else	// Render gridlines for planar snapping
	{
		F32 u = 0.f;
		F32 v = 0.f;
		LLColor4 inner_color;
		LLVector3 normal;
		LLVector3 grid_center = selection_center - grid_origin;
		F32 usc = 1;
		F32 vsc = 1;

		grid_center *= ~grid_rotation;

		switch (mManipPart)
		{
			case LL_YZ_PLANE:
				u = grid_center.mV[VY];
				v = grid_center.mV[VZ];
				usc = grid_scale.mV[VY];
				vsc = grid_scale.mV[VZ];
				inner_color.set(0, 1, 1, line_alpha);
				normal = LLVector3::x_axis;
				break;

			case LL_XZ_PLANE:
				u = grid_center.mV[VX];
				v = grid_center.mV[VZ];
				usc = grid_scale.mV[VX];
				vsc = grid_scale.mV[VZ];
				inner_color.set(1, 0, 1, line_alpha);
				normal = LLVector3::y_axis;
				break;

			case LL_XY_PLANE:
				u = grid_center.mV[VX];
				v = grid_center.mV[VY];
				usc = grid_scale.mV[VX];
				vsc = grid_scale.mV[VY];
				inner_color.set(1, 1, 0, line_alpha);
				normal = LLVector3::z_axis;
				break;

			default:
				break;
		}

		unit0->unbind(LLTexUnit::TT_TEXTURE);
		highlightIntersection(normal, selection_center, grid_rotation,
							  inner_color);

		gGL.pushMatrix();

		F32 x, y, z, angle_radians;
		grid_rotation.getAngleAxis(&angle_radians, &x, &y, &z);
		gGL.translatef(selection_center.mV[VX], selection_center.mV[VY],
					   selection_center.mV[VZ]);
		gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

		F32 sz = getGridDrawSize();
		F32 tiles = sz;
		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.pushMatrix();
		usc = 1.f / usc;
		vsc = 1.f / vsc;

		while (usc > vsc * 4.f)
		{
			usc *= 0.5f;
		}
		while (vsc > usc * 4.f)
		{
			vsc *= 0.5f;
		}

		gGL.scalef(usc, vsc, 1.f);
		gGL.translatef(u, v, 0);

		float a = line_alpha;
		{
			// Draw grid behind objects
			LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

			{
				LLGLDisable stencil(gUsePBRShaders ? 0 : GL_STENCIL_TEST);
				{
					LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_GREATER);
					unit0->bindManual(LLTexUnit::TT_TEXTURE,
												  getGridTexName());
					gGL.flush();
					gGL.blendFunc(LLRender::BF_ZERO,
								  LLRender::BF_ONE_MINUS_SOURCE_ALPHA);
					renderGrid(u, v, tiles,0.9f, 0.9f, 0.9f, a * 0.15f);
					gGL.flush();
					gGL.setSceneBlendType(LLRender::BT_ALPHA);
				}

				{
					// Draw black overlay
					unit0->unbind(LLTexUnit::TT_TEXTURE);
					renderGrid(u, v, tiles, 0.f, 0.f, 0.f, a * 0.16f);

					// Draw grid top
					unit0->bindManual(LLTexUnit::TT_TEXTURE,
												  getGridTexName());
					renderGrid(u, v, tiles, 1.f, 1.f, 1.f, a);

					gGL.popMatrix();
					gGL.matrixMode(LLRender::MM_MODELVIEW);
					gGL.popMatrix();
				}

				{
					LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
					renderGuidelines();
				}

				{
					LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_GREATER);
					gGL.flush();

					switch (mManipPart)
					{
						case LL_YZ_PLANE:
							renderGuidelines(false, true, true);
							break;

						case LL_XZ_PLANE:
							renderGuidelines(true, false, true);
							break;

						case LL_XY_PLANE:
							renderGuidelines(true, true, false);
							break;

						default:
							break;
					}
					gGL.flush();
				}
			}
		}
	}
}

void LLManipTranslate::renderGrid(F32 x, F32 y, F32 size, F32 r, F32 g, F32 b,
								  F32 a)
{
//MK
	if (gRLenabled && gRLInterface.mVisionRestricted &&
		gRLInterface.mCamDistDrawAlphaMax >= 0.25f)
	{
		return;
	}
//mk

	float dx, dy, da;
	F32 d = size * 0.5f;

	for (F32 xx = -size-d; xx < size + d; xx += d)
	{
		gGL.begin(LLRender::TRIANGLE_STRIP);
		for (F32 yy = -size-d; yy < size + d; yy += d)
		{
			dx = xx;
			dy = yy;
			da = sqrtf(llmax(0.f, 1.f - sqrtf(dx * dx + dy * dy) / size)) * a;
			gGL.texCoord2f(dx, dy);
			renderGridVert(dx, dy, r, g, b, da);

			dx = xx + d;
			dy = yy;
			da = sqrtf(llmax(0.f, 1.f - sqrtf(dx * dx + dy * dy) / size)) * a;
			gGL.texCoord2f(dx, dy);
			renderGridVert(dx, dy, r, g, b, da);

			dx = xx;
			dy = yy + d;
			da = sqrtf(llmax(0.f, 1.f - sqrtf(dx * dx + dy * dy) / size)) * a;
			gGL.texCoord2f(dx, dy);
			renderGridVert(dx, dy, r, g, b, da);

			dx = xx + d;
			dy = yy + d;
			da = sqrtf(llmax(0.f, 1.f - sqrtf(dx * dx + dy * dy) / size)) * a;
			gGL.texCoord2f(dx, dy);
			renderGridVert(dx, dy, r, g, b, da);
		}
		gGL.end();
	}
}

void LLManipTranslate::highlightIntersection(LLVector3 normal,
											 LLVector3 selection_center,
											 LLQuaternion grid_rotation,
											 LLColor4 inner_color)
{
//MK
	if (gRLenabled && gRLInterface.mVisionRestricted &&
		gRLInterface.mCamDistDrawAlphaMax >= 0.25f)
	{
		return;
	}
//mk

	// Note: marked "deprecated" in LL's PBR viewer (likely because of heavy
	// stencil usage, the latter having been completely disabled in the PBR
	// code)... *TODO: see if we can de-deprecate, since this is a net loss in
	// functionality, and we still have the stencil buffer available (for the
	// EE renderer). HB
	static LLCachedControl<bool> grid_cross_sections(gSavedSettings,
													 "GridCrossSections");
	if (!grid_cross_sections || gUsePBRShaders)
	{
		return;
	}

	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

	static const U32 types[] =
	{
		LLRenderPass::PASS_SIMPLE,
		LLRenderPass::PASS_ALPHA,
		LLRenderPass::PASS_FULLBRIGHT,
		LLRenderPass::PASS_SHINY
	};
	constexpr U32 num_types = LL_ARRAY_SIZE(types);

	GLuint stencil_mask = 0xFFFFFFFF;

	// Stencil in volumes

	gGL.flush();

	if (shader)
	{
		gClipProgram.bind();
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	{
		glStencilMask(stencil_mask);
		glClearStencil(1);
		glClear(GL_STENCIL_BUFFER_BIT);
		glClearStencil(0);
		LLGLEnable cull_face(GL_CULL_FACE);
		LLGLEnable stencil(GL_STENCIL_TEST);
		LLGLDepthTest depth (GL_TRUE, GL_FALSE, GL_ALWAYS);
		glStencilFunc(GL_ALWAYS, 0, stencil_mask);
		gGL.setColorMask(false, false);
		unit0->unbind(LLTexUnit::TT_TEXTURE);
		gGL.diffuseColor4f(1, 1, 1, 1);

		// Setup clip plane
		normal = normal * grid_rotation;
		if (normal * (gViewerCamera.getOrigin() - selection_center) < 0)
		{
			normal = -normal;
		}
		F32 d = -(selection_center * normal);
		LLVector4a plane(normal.mV[0], normal.mV[1], normal.mV[2], d);

		LLMatrix4a inv_mat = gGL.getModelviewMatrix();
		inv_mat.invert();
		inv_mat.transpose();
		inv_mat.rotate4(plane, plane);

		static LLStaticHashedString sClipPlane("clip_plane");
		gClipProgram.uniform4fv(sClipPlane, 1, plane.getF32ptr());

		bool particles = gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES);
		if (particles)
		{
			LLPipeline::toggleRenderType(LLPipeline::RENDER_TYPE_PARTICLES);
		}
		bool clouds = gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS);
		if (clouds)
		{
			LLPipeline::toggleRenderType(LLPipeline::RENDER_TYPE_CLOUDS);
		}

		// stencil in volumes
		glStencilOp(GL_INCR, GL_INCR, GL_INCR);
		glCullFace(GL_FRONT);
		for (U32 i = 0; i < num_types; ++i)
		{
			gPipeline.renderObjects(types[i], LLVertexBuffer::MAP_VERTEX, false);
		}

		glStencilOp(GL_DECR, GL_DECR, GL_DECR);
		glCullFace(GL_BACK);
		for (U32 i = 0; i < num_types; ++i)
		{
			gPipeline.renderObjects(types[i], LLVertexBuffer::MAP_VERTEX, false);
		}

		if (particles)
		{
			LLPipeline::toggleRenderType(LLPipeline::RENDER_TYPE_PARTICLES);
		}
		if (clouds)
		{
			LLPipeline::toggleRenderType(LLPipeline::RENDER_TYPE_CLOUDS);
		}

		gGL.setColorMask(true, false);
	}
	gGL.color4f(1, 1, 1, 1);

	gGL.pushMatrix();

	F32 x, y, z, angle_radians;
	grid_rotation.getAngleAxis(&angle_radians, &x, &y, &z);
	gGL.translatef(selection_center.mV[VX], selection_center.mV[VY],
				   selection_center.mV[VZ]);
	gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

	F32 sz = getGridDrawSize();
	F32 tiles = sz;

	if (shader)
	{
		shader->bind();
	}

	// Draw volume/plane intersections
	{
		unit0->unbind(LLTexUnit::TT_TEXTURE);
		LLGLDepthTest depth(GL_FALSE);
		LLGLEnable stencil(GL_STENCIL_TEST);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glStencilFunc(GL_EQUAL, 0, stencil_mask);
		renderGrid(0, 0, tiles,inner_color.mV[0], inner_color.mV[1],
				   inner_color.mV[2], 0.25f);
	}

	glStencilFunc(GL_ALWAYS, 255, 0xFFFFFFFF);
	glStencilMask(0xFFFFFFFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	gGL.popMatrix();
}

void LLManipTranslate::renderText()
{
	if (mObjectSelection->getRootObjectCount() &&
		!mObjectSelection->isAttachment())
	{
		LLVector3 pos = getPivotPoint();
		renderXYZ(pos);
	}
	else
	{
		constexpr bool children_ok = true;
		LLViewerObject* objectp = mObjectSelection->getFirstRootObject(children_ok);
		if (objectp)
		{
			renderXYZ(objectp->getPositionEdit());
		}
	}
}

void LLManipTranslate::renderTranslationHandles()
{
	LLVector3 grid_origin;
	LLVector3 grid_scale;
	LLQuaternion grid_rotation;
	LLGLDepthTest gls_depth(GL_FALSE);

	gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);
	LLVector3 at_axis;
	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		at_axis = LLVector3::x_axis * ~grid_rotation;
	}
	else
	{
		at_axis = gViewerCamera.getAtAxis() * ~grid_rotation;
	}

	if (at_axis.mV[VX] > 0.f)
	{
		mPlaneManipPositions.mV[VX] = 1.f;
	}
	else
	{
		mPlaneManipPositions.mV[VX] = -1.f;
	}

	if (at_axis.mV[VY] > 0.f)
	{
		mPlaneManipPositions.mV[VY] = 1.f;
	}
	else
	{
		mPlaneManipPositions.mV[VY] = -1.f;
	}

	if (at_axis.mV[VZ] > 0.f)
	{
		mPlaneManipPositions.mV[VZ] = 1.f;
	}
	else
	{
		mPlaneManipPositions.mV[VZ] = -1.f;
	}

	LLViewerObject *first_object = mObjectSelection->getFirstMoveableObject(true);
	if (!first_object) return;

	LLVector3 selection_center = getPivotPoint();

	// Drag handles
	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		mArrowLengthMeters = mAxisArrowLength / gViewerWindowp->getWindowHeight();
		mArrowLengthMeters /= gAgent.mHUDCurZoom;
	}
	else
	{
		LLVector3 camera_pos_agent = gAgent.getCameraPositionAgent();
		F32 range = dist_vec(camera_pos_agent, selection_center);
		F32 range_from_agent = dist_vec(gAgent.getPositionAgent(), selection_center);

		// Don't draw handles if you're too far away
		static LLCachedControl<bool> limit_select_distance(gSavedSettings,
														   "LimitSelectDistance");
		static LLCachedControl<F32> max_select_distance(gSavedSettings,
														"MaxSelectDistance");
		if (limit_select_distance)
		{
			if (range_from_agent > max_select_distance)
			{
				return;
			}
		}

		if (range > 0.001f)
		{
			// range != zero
			F32 fraction_of_fov = mAxisArrowLength /
								  (F32)gViewerCamera.getViewHeightInPixels();
			// in radians:
			F32 apparent_angle = fraction_of_fov * gViewerCamera.getView();
			mArrowLengthMeters = range * tanf(apparent_angle);
		}
		else
		{
			// range == zero
			mArrowLengthMeters = 1.f;
		}
	}

	mPlaneManipOffsetMeters = mArrowLengthMeters * 1.8f;
	mConeSize = mArrowLengthMeters * 0.25f;

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	{
		gGL.translatef(selection_center.mV[VX], selection_center.mV[VY],
					   selection_center.mV[VZ]);

		F32 angle_radians, x, y, z;
		grid_rotation.getAngleAxis(&angle_radians, &x, &y, &z);

		gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

		LLQuaternion invRotation = grid_rotation;
		invRotation.transpose();

		LLVector3 relative_camera_dir;

		if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
		{
			relative_camera_dir = LLVector3::x_axis * invRotation;
		}
		else
		{
			relative_camera_dir = (selection_center -
								   gViewerCamera.getOrigin()) * invRotation;
		}
		relative_camera_dir.normalize();

		LLTexUnit* unit0 = gGL.getTexUnit(0);
		{
			unit0->unbind(LLTexUnit::TT_TEXTURE);
			LLGLDisable cull_face(GL_CULL_FACE);

			LLColor4 color1;
			LLColor4 color2;

			// Update manipulator sizes
			F32 critical_damp =
				LLCriticalDamp::getInterpolant(MANIP_SCALE_HALF_LIFE);
			for (S32 index = 0; index < 3; ++index)
			{
				if (index == mManipPart - LL_X_ARROW ||
					index == mHighlightedPart - LL_X_ARROW)
				{
					mArrowScales.mV[index] = lerp(mArrowScales.mV[index],
												  SELECTED_ARROW_SCALE,
												  critical_damp);
					mPlaneScales.mV[index] = lerp(mPlaneScales.mV[index], 1.f,
												  critical_damp);
				}
				else if (index == mManipPart - LL_YZ_PLANE ||
						 index == mHighlightedPart - LL_YZ_PLANE)
				{
					mArrowScales.mV[index] = lerp(mArrowScales.mV[index], 1.f,
						 						  critical_damp);
					mPlaneScales.mV[index] = lerp(mPlaneScales.mV[index],
												  SELECTED_ARROW_SCALE,
												  critical_damp);
				}
				else
				{
					mArrowScales.mV[index] = lerp(mArrowScales.mV[index], 1.f,
												  critical_damp);
					mPlaneScales.mV[index] = lerp(mPlaneScales.mV[index], 1.f,
												  critical_damp);
				}
			}

			if ((mManipPart == LL_NO_PART || mManipPart == LL_YZ_PLANE) &&
				fabsf(relative_camera_dir.mV[VX]) > MIN_PLANE_MANIP_DOT_PRODUCT)
			{
				// render YZ plane manipulator
				gGL.pushMatrix();
				gGL.scalef(mPlaneManipPositions.mV[VX],
						   mPlaneManipPositions.mV[VY],
						   mPlaneManipPositions.mV[VZ]);
				gGL.translatef(0.f, mPlaneManipOffsetMeters,
							   mPlaneManipOffsetMeters);
				gGL.scalef(mPlaneScales.mV[VX], mPlaneScales.mV[VX],
						   mPlaneScales.mV[VX]);
				if (mHighlightedPart == LL_YZ_PLANE)
				{
					color1.set(0.f, 1.f, 0.f, 1.f);
					color2.set(0.f, 0.f, 1.f, 1.f);
				}
				else
				{
					color1.set(0.f, 1.f, 0.f, 0.6f);
					color2.set(0.f, 0.f, 1.f, 0.6f);
				}
				gGL.begin(LLRender::TRIANGLES);
				{
					gGL.color4fv(color1.mV);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f));
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);

					gGL.color4fv(color2.mV);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f),
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
				}
				gGL.end();

				LLUI::setLineWidth(3.f);
				gGL.begin(LLRender::LINES);
				{
					gGL.color4f(0.f, 0.f, 0.f, 0.3f);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.1f),
								 mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.1f));
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.1f),
								 mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.4f));

					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.1f,
								 mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.1f));
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(0.f,
								 mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.4f,
								 mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.1f));
				}
				gGL.end();
				LLUI::setLineWidth(1.f);
				gGL.popMatrix();
			}

			if ((mManipPart == LL_NO_PART || mManipPart == LL_XZ_PLANE) &&
				 fabsf(relative_camera_dir.mV[VY]) > MIN_PLANE_MANIP_DOT_PRODUCT)
			{
				// render XZ plane manipulator
				gGL.pushMatrix();
				gGL.scalef(mPlaneManipPositions.mV[VX],
						   mPlaneManipPositions.mV[VY],
						   mPlaneManipPositions.mV[VZ]);
				gGL.translatef(mPlaneManipOffsetMeters, 0.f,
							   mPlaneManipOffsetMeters);
				gGL.scalef(mPlaneScales.mV[VY], mPlaneScales.mV[VY],
						   mPlaneScales.mV[VY]);
				if (mHighlightedPart == LL_XZ_PLANE)
				{
					color1.set(0.f, 0.f, 1.f, 1.f);
					color2.set(1.f, 0.f, 0.f, 1.f);
				}
				else
				{
					color1.set(0.f, 0.f, 1.f, 0.6f);
					color2.set(1.f, 0.f, 0.f, 0.6f);
				}

				gGL.begin(LLRender::TRIANGLES);
				{
					gGL.color4fv(color1.mV);
					gGL.vertex3f(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f),
								 0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);

					gGL.color4fv(color2.mV);
					gGL.vertex3f(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f));
					gGL.vertex3f(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
				}
				gGL.end();

				LLUI::setLineWidth(3.f);
				gGL.begin(LLRender::LINES);
				{
					gGL.color4f(0.f, 0.f, 0.f, 0.3f);
					gGL.vertex3f(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.1f),
								 0.f,
								 mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.1f));
					gGL.vertex3f(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.1f),
								 0.f,
								 mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.4f));

					gGL.vertex3f(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.1f),
								 0.f,
								 mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.1f));
					gGL.vertex3f(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								 0.f,
								 mPlaneManipOffsetMeters * QUARTER_TICK_SIZE);
					gGL.vertex3f(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.4f),
								 0.f,
								 mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.1f));
				}
				gGL.end();
				LLUI::setLineWidth(1.f);

				gGL.popMatrix();
			}

			if ((mManipPart == LL_NO_PART || mManipPart == LL_XY_PLANE) &&
				fabsf(relative_camera_dir.mV[VZ]) > MIN_PLANE_MANIP_DOT_PRODUCT)
			{
				// render XY plane manipulator
				gGL.pushMatrix();
				gGL.scalef(mPlaneManipPositions.mV[VX],
						   mPlaneManipPositions.mV[VY],
						   mPlaneManipPositions.mV[VZ]);

/*				 			  Y
				 			  ^
				 			  v1
				 			  |  \
				 			  |<- v0
				 			  |  /| \
				 			  v2__v__v3 > X
*/
					LLVector3 v0,v1,v2,v3;
#if 0
					// This should theoretically work but looks off; could be
					// tuned later -SJB
					gGL.translatef(-mPlaneManipOffsetMeters,
								   -mPlaneManipOffsetMeters, 0.f);
					v0 = LLVector3(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								   mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								   0.f);
					v1 = LLVector3(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								   mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.75f),
								   0.f);
					v2 = LLVector3(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								   mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								   0.f);
					v3 = LLVector3(mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.75f),
								   mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								   0.f);
#else
					gGL.translatef(mPlaneManipOffsetMeters,
								   mPlaneManipOffsetMeters, 0.f);
					v0 = LLVector3(mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								   mPlaneManipOffsetMeters * -QUARTER_TICK_SIZE,
								   0.f);
					v1 = LLVector3(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								   mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f),
								   0.f);
					v2 = LLVector3(mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								   mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								   0.f);
					v3 = LLVector3(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f),
								   mPlaneManipOffsetMeters * QUARTER_TICK_SIZE,
								   0.f);
#endif
					gGL.scalef(mPlaneScales.mV[VZ], mPlaneScales.mV[VZ],
							   mPlaneScales.mV[VZ]);
					if (mHighlightedPart == LL_XY_PLANE)
					{
						color1.set(1.f, 0.f, 0.f, 1.f);
						color2.set(0.f, 1.f, 0.f, 1.f);
					}
					else
					{
						color1.set(0.8f, 0.f, 0.f, 0.6f);
						color2.set(0.f, 0.8f, 0.f, 0.6f);
					}

					gGL.begin(LLRender::TRIANGLES);
					{
						gGL.color4fv(color1.mV);
						gGL.vertex3fv(v0.mV);
						gGL.vertex3fv(v1.mV);
						gGL.vertex3fv(v2.mV);

						gGL.color4fv(color2.mV);
						gGL.vertex3fv(v2.mV);
						gGL.vertex3fv(v3.mV);
						gGL.vertex3fv(v0.mV);
					}
					gGL.end();

					LLUI::setLineWidth(3.f);
					gGL.begin(LLRender::LINES);
					{
						gGL.color4f(0.f, 0.f, 0.f, 0.3f);
						LLVector3 v12 = (v1 + v2) * .5f;
						gGL.vertex3fv(v0.mV);
						gGL.vertex3fv(v12.mV);
						gGL.vertex3fv(v12.mV);
						gGL.vertex3fv((v12 + (v0 - v12) * .3f +
									  (v2 - v12) * .3f).mV);
						gGL.vertex3fv(v12.mV);
						gGL.vertex3fv((v12 + (v0 - v12) * .3f +
									  (v1 - v12) * .3f).mV);

						LLVector3 v23 = (v2 + v3) * .5f;
						gGL.vertex3fv(v0.mV);
						gGL.vertex3fv(v23.mV);
						gGL.vertex3fv(v23.mV);
						gGL.vertex3fv((v23 + (v0 - v23) * .3f +
									  (v3 - v23) * .3f).mV);
						gGL.vertex3fv(v23.mV);
						gGL.vertex3fv((v23 + (v0 - v23) * .3f +
									  (v2 - v23) * .3f).mV);
					}
					gGL.end();
					LLUI::setLineWidth(1.f);

				gGL.popMatrix();
			}
		}
		{
			unit0->unbind(LLTexUnit::TT_TEXTURE);

			// Since we draw handles with depth testing off, we need to draw
			// them in the proper depth order.

			// Copied from LLDrawable::updateGeometry
			LLVector3 pos_agent     = first_object->getPositionAgent();
			LLVector3 camera_agent	= gAgent.getCameraPositionAgent();
			LLVector3 headPos		= pos_agent - camera_agent;

			LLVector3 orientWRTHead	= headPos * invRotation;

			// Find nearest vertex
			U32 nearest = (orientWRTHead.mV[0] < 0.f ? 1 : 0) +
						  (orientWRTHead.mV[1] < 0.f ? 2 : 0) +
						  (orientWRTHead.mV[2] < 0.f ? 4 : 0);

			// opposite faces on Linden cubes:
			// 0 & 5
			// 1 & 3
			// 2 & 4

			// Table of order to draw faces, based on nearest vertex
			static U32 face_list[8][NUM_AXES * 2] = {
				{ 2,0,1, 4,5,3 }, // v6  F201 F453
				{ 2,0,3, 4,5,1 }, // v7  F203 F451
				{ 4,0,1, 2,5,3 }, // v5  F401 F253
				{ 4,0,3, 2,5,1 }, // v4  F403 F251
				{ 2,5,1, 4,0,3 }, // v2  F251 F403
				{ 2,5,3, 4,0,1 }, // v3  F253 F401
				{ 4,5,1, 2,0,3 }, // v1  F451 F203
				{ 4,5,3, 2,0,1 }, // v0  F453 F201
			};

			static const EManipPart which_arrow[6] = {
				LL_Z_ARROW,
				LL_X_ARROW,
				LL_Y_ARROW,
				LL_X_ARROW,
				LL_Y_ARROW,
				LL_Z_ARROW
			};

			// Draw arrows for deeper faces first, closer faces last
			LLVector3 camera_axis;
			if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
			{
				camera_axis = LLVector3::x_axis;
			}
			else
			{
				camera_axis.set(gAgent.getCameraPositionAgent() -
								first_object->getPositionAgent());
			}

			for (U32 i = 0; i < NUM_AXES * 2; ++i)
			{
				U32 face = face_list[nearest][i];

				LLVector3 arrow_axis;
				getManipAxis(first_object, which_arrow[face], arrow_axis);

				renderArrow(which_arrow[face],
							mManipPart,
							face >= 3 ? -mConeSize : mConeSize,
							face >= 3 ? -mArrowLengthMeters
									  : mArrowLengthMeters,
							mConeSize, false);
			}
		}
	}
	gGL.popMatrix();
}

void LLManipTranslate::renderArrow(S32 which_arrow, S32 selected_arrow,
								   F32 box_size, F32 arrow_size,
								   F32 handle_size, bool reverse_direction)
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	LLGLEnable gls_blend(GL_BLEND);

	for (S32 pass = 1; pass <= 2; pass++)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE,
								pass == 1 ? GL_LEQUAL : GL_GREATER);
		gGL.pushMatrix();

		S32 index = ARROW_TO_AXIS[which_arrow];

		// Assign a color for this arrow
		LLColor4 color;  // black
		if (which_arrow == selected_arrow || which_arrow == mHighlightedPart)
		{
			color.mV[index] = (pass == 1) ? 1.f : 0.5f;
		}
		else if (selected_arrow != LL_NO_PART)
		{
			color.mV[VALPHA] = 0.f;
		}
		else
		{
			color.mV[index] = pass == 1 ? .8f : .35f;	// red, green, or blue
			color.mV[VALPHA] = 0.6f;
		}
		gGL.color4fv(color.mV);

		LLVector3 vec;

		LLUI::setLineWidth(2.f);
		gGL.begin(LLRender::LINES);

		vec.mV[index] = box_size;
		gGL.vertex3f(vec.mV[0], vec.mV[1], vec.mV[2]);

		vec.mV[index] = arrow_size;
		gGL.vertex3f(vec.mV[0], vec.mV[1], vec.mV[2]);

		gGL.end();
		LLUI::setLineWidth(1.f);

		gGL.translatef(vec.mV[0], vec.mV[1], vec.mV[2]);
		gGL.scalef(handle_size, handle_size, handle_size);

		F32 rot = 0.f;
		LLVector3 axis;

		switch (which_arrow)
		{
			case LL_X_ARROW:
				rot = reverse_direction ? -90.f : 90.f;
				axis.mV[1] = 1.f;
				break;

			case LL_Y_ARROW:
				rot = reverse_direction ? 90.f : -90.f;
				axis.mV[0] = 1.f;
				break;

			case LL_Z_ARROW:
				rot = reverse_direction ? 180.f : 0.f;
				axis.mV[0] = 1.f;
				break;

			default:
				llerrs << "Unknown arrow type " << which_arrow << llendl;
		}

		gGL.diffuseColor4fv(color.mV);
		gGL.rotatef(rot, axis.mV[0], axis.mV[1], axis.mV[2]);
		gGL.scalef(mArrowScales.mV[index], mArrowScales.mV[index],
				   mArrowScales.mV[index] * 1.5f);

		gCone.render();

		gGL.popMatrix();
	}
}

void LLManipTranslate::renderGridVert(F32 x_trans, F32 y_trans,
									  F32 r, F32 g, F32 b, F32 alpha)
{
	gGL.color4f(r, g, b, alpha);
	switch (mManipPart)
	{
		case LL_YZ_PLANE:
			gGL.vertex3f(0.f, x_trans, y_trans);
			break;

		case LL_XZ_PLANE:
			gGL.vertex3f(x_trans, 0.f, y_trans);
			break;

		case LL_XY_PLANE:
			gGL.vertex3f(x_trans, y_trans, 0.f);
			break;

		default:
			gGL.vertex3f(0.f, 0.f, 0.f);
	}
}

//virtual
bool LLManipTranslate::canAffectSelection()
{
	bool can_move = mObjectSelection->getObjectCount() != 0;
	if (can_move)
	{
		struct f final : public LLSelectedObjectFunctor
		{
			bool apply(LLViewerObject* objectp) override
			{
				static LLCachedControl<bool> edit_linked_parts(gSavedSettings,
															   "EditLinkedParts");
				if (!objectp)
				{
					llwarns << "NULL object passed to functor !" << llendl;
					return false;
				}
				LLViewerObject* root_object = objectp->getRootEdit();
				return objectp->permMove() &&
					   !objectp->isPermanentEnforced() &&
					   (!root_object || !root_object->isPermanentEnforced()) &&
					   (objectp->permModify() || !edit_linked_parts);
			}
		} func;
		can_move = mObjectSelection->applyToObjects(&func);
	}
	return can_move;
}
