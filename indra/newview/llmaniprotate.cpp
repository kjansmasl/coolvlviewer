/**
 * @file llmaniprotate.cpp
 * @brief LLManipRotate class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llmaniprotate.h"

#include "llbutton.h"
#include "llgl.h"
#include "llprimitive.h"
#include "llrender.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llhoverview.h"
#include "llfloatertools.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For hud_render_text()
#include "llviewerobject.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llworld.h"

constexpr F32 RADIUS_PIXELS = 100.f;		// Size in screen space
constexpr F32 SQ_RADIUS = RADIUS_PIXELS * RADIUS_PIXELS;
constexpr F32 WIDTH_PIXELS = 8;
constexpr S32 CIRCLE_STEPS = 100;
constexpr F32 MAX_MANIP_SELECT_DISTANCE = 100.f;
constexpr F32 SNAP_ANGLE_INCREMENT = 5.625f;
constexpr F32 SNAP_ANGLE_DETENTE = SNAP_ANGLE_INCREMENT;
constexpr F32 SNAP_GUIDE_RADIUS_1 = 2.8f;
constexpr F32 SNAP_GUIDE_RADIUS_2 = 2.4f;
constexpr F32 SNAP_GUIDE_RADIUS_3 = 2.2f;
constexpr F32 SNAP_GUIDE_RADIUS_4 = 2.1f;
constexpr F32 SNAP_GUIDE_RADIUS_5 = 2.05f;
constexpr F32 SNAP_GUIDE_INNER_RADIUS = 2.f;
constexpr F32 SELECTED_MANIPULATOR_SCALE = 1.05f;
constexpr F32 MANIPULATOR_SCALE_HALF_LIFE = 0.07f;
// gcc accepts constexpr here, but not clang...
static const F32 AXIS_ONTO_CAM_TOLERANCE = cosf(80.f * DEG_TO_RAD);

LLManipRotate::LLManipRotate(LLToolComposite* composite)
: 	LLManip(std::string("Rotate"), composite),
	mRotationCenter(),
	mCenterScreen(),
	mRotation(),
	mMouseDown(),
	mMouseCur(),
	mRadiusMeters(0.f),
	mCenterToCam(),
	mCenterToCamNorm(),
	mCenterToCamMag(0.f),
	mCenterToProfilePlane(),
	mCenterToProfilePlaneMag(0.f),
	mSmoothRotate(false),
	mCamEdgeOn(false),
	mManipulatorScales(1.f, 1.f, 1.f, 1.f)
{
}

// static
bool LLManipRotate::getSnapEnabled()
{
	static LLCachedControl<bool> snap_enabled(gSavedSettings, "SnapEnabled");
	return snap_enabled;
}

void LLManipRotate::handleSelect()
{
	// *FIX: put this in mouseDown ?
	gSelectMgr.saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
	if (gFloaterToolsp)
	{
		gFloaterToolsp->setStatusText("rotate");
	}
	LLManip::handleSelect();
}

void LLManipRotate::render()
{
	LLGLSUIDefault gls_ui;
	gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);
	LLGLDepthTest gls_depth(GL_TRUE);
	LLGLEnable gl_blend(GL_BLEND);

	// You can rotate if you can move
	LLViewerObject* first_object = mObjectSelection->getFirstMoveableObject(true);
	if (!first_object)
	{
		return;
	}

	if (!updateVisiblity())
	{
		return;
	}

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		F32 zoom = gAgent.mHUDCurZoom;
		gGL.scalef(zoom, zoom, zoom);
	}

	LLVector3 center = gAgent.getPosAgentFromGlobal(mRotationCenter);

	LLColor4 highlight_outside(1.f, 1.f, 0.f, 1.f);
	LLColor4 highlight_inside(0.7f, 0.7f, 0.f, 0.5f);
	F32 width_meters = WIDTH_PIXELS * mRadiusMeters / RADIUS_PIXELS;

	gGL.pushMatrix();
	{
		// are we in the middle of a constrained drag?
		if (mManipPart >= LL_ROT_X && mManipPart <= LL_ROT_Z)
		{
			renderSnapGuides();
		}
		else
		{
			gDebugProgram.bind();

			LLGLEnable cull_face(GL_CULL_FACE);
			LLGLDepthTest gls_depth(GL_FALSE);
			gGL.pushMatrix();
			{
				// Draw "sphere" (intersection of sphere with tangent cone that
				// has apex at camera)
				gGL.translatef(mCenterToProfilePlane.mV[VX],
							   mCenterToProfilePlane.mV[VY],
							   mCenterToProfilePlane.mV[VZ]);
				gGL.translatef(center.mV[VX], center.mV[VY], center.mV[VZ]);

				// Inverse change of basis vectors
				LLVector3 forward = mCenterToCamNorm;
				LLVector3 left = gAgent.getUpAxis() % forward;
				left.normalize();
				LLVector3 up = forward % left;

				LLVector4 a(-forward);
				a.mV[3] = 0;
				LLVector4 b(up);
				b.mV[3] = 0;
				LLVector4 c(left);
				c.mV[3] = 0;
				LLMatrix4 mat;
				mat.initRows(a, b, c, LLVector4(0.f, 0.f, 0.f, 1.f));

				LLMatrix4a mata;
				mata.loadu(mat.getF32ptr());
				gGL.multMatrix(mata);

				static const LLMatrix4a rot = gl_gen_rot(-90, 0.f, 1.f, 0.f);
				gGL.rotatef(rot);

				LLColor4 color;
				if (mManipPart == LL_ROT_ROLL || mHighlightedPart == LL_ROT_ROLL)
				{
					color.set(0.8f, 0.8f, 0.8f, 0.8f);
					gGL.scalef(mManipulatorScales.mV[VW],
							   mManipulatorScales.mV[VW],
							   mManipulatorScales.mV[VW]);
				}
				else
				{
					color.set(0.7f, 0.7f, 0.7f, 0.6f);
				}
				gGL.diffuseColor4fv(color.mV);
				gl_washer_2d(mRadiusMeters + width_meters, mRadiusMeters,
							 CIRCLE_STEPS, color, color);

				if (mManipPart == LL_NO_PART)
				{
					gGL.color4f(0.7f, 0.7f, 0.7f, 0.3f);
					gGL.diffuseColor4f(0.7f, 0.7f, 0.7f, 0.3f);
					gl_circle_2d(0, 0,  mRadiusMeters, CIRCLE_STEPS, true);
				}

				gGL.flush();
			}
			gGL.popMatrix();

			gUIProgram.bind();
		}

		gGL.translatef(center.mV[VX], center.mV[VY], center.mV[VZ]);

		LLQuaternion rot;
		F32 angle_radians, x, y, z;

		LLVector3 grid_origin;
		LLVector3 grid_scale;
		LLQuaternion grid_rotation;

		gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);

		grid_rotation.getAngleAxis(&angle_radians, &x, &y, &z);
		gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

		gDebugProgram.bind();

		F32 critical_damp =
			LLCriticalDamp::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE);

		if (mManipPart == LL_ROT_Z)
		{
			static const LLVector4 rot_z_axis(1.f, 1.f,
											  SELECTED_MANIPULATOR_SCALE, 1.f);
			mManipulatorScales = lerp(mManipulatorScales, rot_z_axis,
									  critical_damp);
			gGL.pushMatrix();
			{
				// Selected part
				gGL.scalef(mManipulatorScales.mV[VZ],
						   mManipulatorScales.mV[VZ],
						   mManipulatorScales.mV[VZ]);
				renderActiveRing(mRadiusMeters, width_meters,
								 LLColor4(0.f, 0.f, 1.f, 1.f),
								 LLColor4(0.f, 0.f, 1.f, 0.3f));
			}
			gGL.popMatrix();
		}
		else if (mManipPart == LL_ROT_Y)
		{
			mManipulatorScales =
				lerp(mManipulatorScales,
					 LLVector4(1.f, SELECTED_MANIPULATOR_SCALE, 1.f, 1.f),
							   critical_damp);
			gGL.pushMatrix();
			{
				static const LLMatrix4a rot = gl_gen_rot(90.f, 1.f, 0.f, 0.f);
				gGL.rotatef(rot);
				gGL.scalef(mManipulatorScales.mV[VY],
						   mManipulatorScales.mV[VY],
						   mManipulatorScales.mV[VY]);
				renderActiveRing(mRadiusMeters, width_meters,
								 LLColor4(0.f, 1.f, 0.f, 1.f),
								 LLColor4(0.f, 1.f, 0.f, 0.3f));
			}
			gGL.popMatrix();
		}
		else if (mManipPart == LL_ROT_X)
		{
			mManipulatorScales = lerp(mManipulatorScales,
									  LLVector4(SELECTED_MANIPULATOR_SCALE,
												1.f, 1.f, 1.f),
									  critical_damp);
			gGL.pushMatrix();
			{
				static const LLMatrix4a rot = gl_gen_rot(90.f, 0.f, 1.f, 0.f);
				gGL.rotatef(rot);
				gGL.scalef(mManipulatorScales.mV[VX],
						   mManipulatorScales.mV[VX],
						   mManipulatorScales.mV[VX]);
				renderActiveRing(mRadiusMeters, width_meters,
								 LLColor4(1.f, 0.f, 0.f, 1.f),
								 LLColor4(1.f, 0.f, 0.f, 0.3f));
			}
			gGL.popMatrix();
		}
		else if (mManipPart == LL_ROT_ROLL)
		{
			mManipulatorScales =
				lerp(mManipulatorScales,
					 LLVector4(1.f, 1.f, 1.f, SELECTED_MANIPULATOR_SCALE),
					 critical_damp);
		}
		else if (mManipPart == LL_NO_PART)
		{
			if (mHighlightedPart == LL_NO_PART)
			{
				mManipulatorScales =
					lerp(mManipulatorScales,
						 LLVector4(1.f, 1.f, 1.f, 1.f),
						 critical_damp);
			}

			LLGLEnable cull_face(GL_CULL_FACE);
			LLGLEnable clip_plane0(GL_CLIP_PLANE0);
			LLGLDepthTest gls_depth(GL_FALSE);

			// First pass: centers. Second pass: sides.
			for (S32 i = 0; i < 2; i++)
			{
				gGL.pushMatrix();
				{
					if (mHighlightedPart == LL_ROT_Z)
					{
						mManipulatorScales =
							lerp(mManipulatorScales,
								 LLVector4(1.f, 1.f, SELECTED_MANIPULATOR_SCALE, 1.f),
								 critical_damp);
						gGL.scalef(mManipulatorScales.mV[VZ],
								   mManipulatorScales.mV[VZ],
								   mManipulatorScales.mV[VZ]);
						// Hovering over part
						gl_ring(mRadiusMeters, width_meters,
								LLColor4(0.f, 0.f, 1.f, 1.f),
								LLColor4(0.f, 0.f, 1.f, 0.5f),
								CIRCLE_STEPS, i);
					}
					else
					{
						// Default
						gl_ring(mRadiusMeters, width_meters,
								LLColor4(0.f, 0.f, 0.8f, 0.8f),
								LLColor4(0.f, 0.f, 0.8f, 0.4f),
								CIRCLE_STEPS, i);
					}
				}
				gGL.popMatrix();

				gGL.pushMatrix();
				{
					static const LLMatrix4a rot =
						gl_gen_rot(90.f, 1.f, 0.f, 0.f);
					gGL.rotatef(rot);
					if (mHighlightedPart == LL_ROT_Y)
					{
						mManipulatorScales =
							lerp(mManipulatorScales,
								 LLVector4(1.f, SELECTED_MANIPULATOR_SCALE, 1.f, 1.f),
								 critical_damp);
						gGL.scalef(mManipulatorScales.mV[VY],
								   mManipulatorScales.mV[VY],
								   mManipulatorScales.mV[VY]);
						// Hovering over part
						gl_ring(mRadiusMeters, width_meters,
								LLColor4(0.f, 1.f, 0.f, 1.f),
								LLColor4(0.f, 1.f, 0.f, 0.5f),
								CIRCLE_STEPS, i);
					}
					else
					{
						// Default
						gl_ring(mRadiusMeters, width_meters,
								LLColor4(0.f, 0.8f, 0.f, 0.8f),
								LLColor4(0.f, 0.8f, 0.f, 0.4f),
								CIRCLE_STEPS, i);
					}
				}
				gGL.popMatrix();

				gGL.pushMatrix();
				{
					static const LLMatrix4a rot =
						gl_gen_rot(90.f, 0.f, 1.f, 0.f);
					gGL.rotatef(rot);
					if (mHighlightedPart == LL_ROT_X)
					{
						mManipulatorScales =
							lerp(mManipulatorScales,
								 LLVector4(SELECTED_MANIPULATOR_SCALE, 1.f, 1.f, 1.f),
								 critical_damp);
						gGL.scalef(mManipulatorScales.mV[VX],
								   mManipulatorScales.mV[VX],
								   mManipulatorScales.mV[VX]);

						// Hovering over part
						gl_ring(mRadiusMeters, width_meters,
								LLColor4(1.f, 0.f, 0.f, 1.f),
								LLColor4(1.f, 0.f, 0.f, 0.5f),
								CIRCLE_STEPS, i);
					}
					else
					{
						// Default
						gl_ring(mRadiusMeters, width_meters,
								LLColor4(0.8f, 0.f, 0.f, 0.8f),
								LLColor4(0.8f, 0.f, 0.f, 0.4f),
								CIRCLE_STEPS, i);
					}
				}
				gGL.popMatrix();

				if (mHighlightedPart == LL_ROT_ROLL)
				{
					mManipulatorScales =
						lerp(mManipulatorScales,
							 LLVector4(1.f, 1.f, 1.f, SELECTED_MANIPULATOR_SCALE),
							 critical_damp);
				}
			}
		}

		gUIProgram.bind();
	}
	gGL.popMatrix();
	gGL.popMatrix();

	LLVector3 euler_angles;
	LLQuaternion object_rot = first_object->getRotationEdit();
	object_rot.getEulerAngles(&(euler_angles.mV[VX]), &(euler_angles.mV[VY]),
							  &(euler_angles.mV[VZ]));
	euler_angles *= RAD_TO_DEG;
	euler_angles.mV[VX] = ll_round(fmodf(euler_angles.mV[VX] + 360.f, 360.f),
								   0.05f);
	euler_angles.mV[VY] = ll_round(fmodf(euler_angles.mV[VY] + 360.f, 360.f),
								   0.05f);
	euler_angles.mV[VZ] = ll_round(fmodf(euler_angles.mV[VZ] + 360.f, 360.f),
								   0.05f);
	renderXYZ(euler_angles);
}

bool LLManipRotate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	LLViewerObject* first_object =
		mObjectSelection->getFirstMoveableObject(true);
	if (first_object)
	{
		if (mHighlightedPart != LL_NO_PART)
		{
			handled = handleMouseDownOnPart(x, y, mask);
		}
	}

	return handled;
}

// Assumes that one of the parts of the manipulator was hit.
bool LLManipRotate::handleMouseDownOnPart(S32 x, S32 y, MASK mask)
{
	if (!canAffectSelection())
	{
		return false;
	}

	highlightManipulators(x, y);
	S32 hit_part = mHighlightedPart;
	// We just started a drag, so save initial object positions
	gSelectMgr.saveSelectedObjectTransform(SELECT_ACTION_TYPE_ROTATE);

	// Save selection center
	mRotationCenter = gAgent.getPosGlobalFromAgent(getPivotPoint());

	mManipPart = (EManipPart)hit_part;
	LLVector3 center = gAgent.getPosAgentFromGlobal(mRotationCenter);

	if (mManipPart == LL_ROT_GENERAL)
	{
		mMouseDown = intersectMouseWithSphere(x, y, center, mRadiusMeters);
	}
	else
	{
		// Project onto the plane of the ring
		LLVector3 axis = getConstraintAxis();

		F32 axis_onto_cam = fabsf(axis * mCenterToCamNorm);
		if (axis_onto_cam < AXIS_ONTO_CAM_TOLERANCE)
		{
			LLVector3 up_from_axis = mCenterToCamNorm % axis;
			up_from_axis.normalize();
			LLVector3 cur_intersection;
			getMousePointOnPlaneAgent(cur_intersection, x, y, center,
									  mCenterToCam);
			cur_intersection -= center;
			mMouseDown = projected_vec(cur_intersection, up_from_axis);
			F32 mouse_depth = SNAP_GUIDE_INNER_RADIUS * mRadiusMeters;
			F32 mouse_dist_sqrd = mMouseDown.lengthSquared();
			if (mouse_dist_sqrd > 0.0001f)
			{
				mouse_depth = sqrtf(mouse_depth * mouse_depth -
									mouse_dist_sqrd);
			}
			LLVector3 projected_center_to_cam = mCenterToCamNorm -
												projected_vec(mCenterToCamNorm,
															 axis);
			mMouseDown += mouse_depth * projected_center_to_cam;
		}
		else
		{
			mMouseDown = findNearestPointOnRing(x, y, center, axis) - center;
			mMouseDown.normalize();
		}
	}

	mMouseCur = mMouseDown;

	// Route future Mouse messages here preemptively (release on mouse up).
	setMouseCapture(true);
	gSelectMgr.enableSilhouette(false);

	mHelpTextTimer.reset();
	++sNumTimesHelpTextShown;

	return true;
}

LLVector3 LLManipRotate::findNearestPointOnRing(S32 x, S32 y,
												const LLVector3& center,
												const LLVector3& axis)
{
	// Project the delta onto the ring and rescale it by the radius so that it
	// is _on_ the ring.
	LLVector3 proj_onto_ring;
	getMousePointOnPlaneAgent(proj_onto_ring, x, y, center, axis);
	proj_onto_ring -= center;
	proj_onto_ring.normalize();

	return center + proj_onto_ring * mRadiusMeters;
}

bool LLManipRotate::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// first, perform normal processing in case this was a quick-click
	handleHover(x, y, mask);

	if (hasMouseCapture())
	{
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
			LLViewerObject* root_object = object->getRootEdit();

			// have permission to move and object is root of selection or
			// individually selected
			if (object->permMove() && !object->isPermanentEnforced() &&
				(!root_object || !root_object->isPermanentEnforced()) &&
				(object->isRootEdit() || selectNode->mIndividualSelection))
			{
				object->mUnselectedChildrenPositions.clear() ;
			}
		}

		mManipPart = LL_NO_PART;

		// Might have missed last update due to timing.
		gSelectMgr.sendMultipleUpdate(UPD_ROTATION | UPD_POSITION);
		gSelectMgr.enableSilhouette(true);
#if 0
		gAgent.setObjectTracking(gSavedSettings.getBool("TrackFocusObject"));
#endif
		gSelectMgr.updateSelectionCenter();
		gSelectMgr.saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
	}

	return LLManip::handleMouseUp(x, y, mask);
}

bool LLManipRotate::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mObjectSelection->isEmpty())
		{
			// Somehow the object got deselected while we were dragging it.
			setMouseCapture(false);
		}
		else
		{
			drag(x, y);
		}

		LL_DEBUGS("UserInput") << "hover handled by LLManipRotate (active)"
							   << LL_ENDL;
	}
	else
	{
		highlightManipulators(x, y);
		LL_DEBUGS("UserInput") << "hover handled by LLManipRotate (inactive)"
							   << LL_ENDL;
	}

	gViewerWindowp->setCursor(UI_CURSOR_TOOLROTATE);
	return true;
}

LLVector3 LLManipRotate::projectToSphere(F32 x, F32 y, bool* on_sphere)
{
	F32 z = 0.f;
	F32 dist_squared = x*x + y*y;

	*on_sphere = dist_squared <= SQ_RADIUS;
    if (*on_sphere)
	{
        z = sqrtf(SQ_RADIUS - dist_squared);
    }
	return LLVector3(x, y, z);
}

// Freeform rotation
void LLManipRotate::drag(S32 x, S32 y)
{
	if (!updateVisiblity())
	{
		return;
	}

	if (mManipPart == LL_ROT_GENERAL)
	{
		mRotation = dragUnconstrained(x, y);
	}
	else
	{
		mRotation = dragConstrained(x, y);
	}

	bool damped = mSmoothRotate;
	mSmoothRotate = false;

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
		LLViewerObject* root_object = object->getRootEdit();

		// have permission to move and object is root of selection or
		// individually selected
		if (object->permMove() && !object->isPermanentEnforced() &&
			(!root_object || !root_object->isPermanentEnforced()) &&
			(object->isRootEdit() || selectNode->mIndividualSelection))
		{
			if (!object->isRootEdit())
			{
				// Child objects should not update if parent is selected
				LLViewerObject* editable_root =
					(LLViewerObject*)object->getParent();
				if (editable_root->isSelected())
				{
					// we will be moved properly by our parent, so skip
					continue;
				}
			}

			LLQuaternion new_rot = selectNode->mSavedRotation * mRotation;
			std::vector<LLVector3>& child_positions =
				object->mUnselectedChildrenPositions ;
			std::vector<LLQuaternion> child_rotations;
			if (object->isRootEdit() && selectNode->mIndividualSelection)
			{
				object->saveUnselectedChildrenRotation(child_rotations) ;
				object->saveUnselectedChildrenPosition(child_positions) ;
			}

			if (object->getParent() && object->mDrawable.notNull())
			{
				LLQuaternion invParentRotation =
					object->mDrawable->mXform.getParent()->getWorldRotation();
				invParentRotation.transpose();

				object->setRotation(new_rot * invParentRotation, damped);
				rebuild(object);
			}
			else
			{
				object->setRotation(new_rot, damped);
				rebuild(object);
			}

			// For individually selected roots, we need to counter-rotate all
			// the children
			if (object->isRootEdit() && selectNode->mIndividualSelection)
			{
				// RN: must do non-damped updates on these objects so relative
				// rotation appears constant instead of having two competing
				// slerps making the child objects appear to "wobble"
				object->resetChildrenRotationAndPosition(child_rotations,
														 child_positions);
			}
		}
	}

	// update positions
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
		LLViewerObject* root_object = object->getRootEdit();

		// to avoid cumulative position changes we calculate the objects new
		// position using its saved position
		if (object->permMove() && !object->isPermanentEnforced() &&
			(!root_object || !root_object->isPermanentEnforced()))
		{
			LLVector3 center   = gAgent.getPosAgentFromGlobal(mRotationCenter);

			LLVector3 old_position;
			LLVector3 new_position;

			if (object->isAttachment() && object->mDrawable.notNull())
			{
				// need to work in drawable space to handle selected items from
				// multiple attachments (which have no shared frame of
				// reference other than their render positions).
				LLXform* parent_xform = object->mDrawable->getXform()->getParent();
				new_position = selectNode->mSavedPositionLocal * parent_xform->getWorldRotation() + parent_xform->getWorldPosition();
				old_position = object->getPosition() * parent_xform->getWorldRotation() + parent_xform->getWorldPosition(); //object->getRenderPosition();
			}
			else
			{
				new_position = gAgent.getPosAgentFromGlobal(selectNode->mSavedPositionGlobal);
				old_position = object->getPositionAgent();
			}

			// New relative rotated position
			new_position = (new_position - center) * mRotation;
			new_position += center;

			if (object->isRootEdit() && !object->isAttachment())
			{
				LLVector3d new_pos_global = gAgent.getPosGlobalFromAgent(new_position);
				new_pos_global = gWorld.clipToVisibleRegions(selectNode->mSavedPositionGlobal,
															 new_pos_global);
				new_position = gAgent.getPosAgentFromGlobal(new_pos_global);
			}

			// For individually selected child objects
			if (!object->isRootEdit() && selectNode->mIndividualSelection)
			{
				LLViewerObject* parentp = (LLViewerObject*)object->getParent();
				if (!parentp->isSelected())
				{
					if (object->isAttachment() && object->mDrawable.notNull())
					{
						// Find position relative to render position of parent
						object->setPositionLocal((new_position -
												  parentp->getRenderPosition()) *
												  ~parentp->getRenderRotation());
						rebuild(object);
					}
					else
					{
						object->setPositionParent((new_position - parentp->getPositionAgent()) *
												   ~parentp->getRotationRegion());
						rebuild(object);
					}
				}
			}
			else if (object->isRootEdit())
			{
				if (object->isAttachment() && object->mDrawable.notNull())
				{
					LLXform* parent_xform =
						object->mDrawable->getXform()->getParent();
					object->setPositionLocal((new_position -
											  parent_xform->getWorldPosition()) *
											  ~parent_xform->getWorldRotation());
					rebuild(object);
				}
				else
				{
					object->setPositionAgent(new_position);
					rebuild(object);
				}
			}

			// For individually selected roots, we need to counter-translate
			// all unselected children
			if (object->isRootEdit() && selectNode->mIndividualSelection)
			{
				// Only offset by parent's translation as we have already
				// countered parent's rotation
				rebuild(object);
				object->resetChildrenPosition(old_position - new_position);
			}
		}
	}

	// store changes to override updates
	for (LLObjectSelection::iterator
			iter = gSelectMgr.getSelection()->begin(),
			end = gSelectMgr.getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* selectNode = *iter;
		LLViewerObject* object = selectNode->getObject();
		if (!object)
		{
			llwarns << "NULL selected object !" << llendl;
			continue;
		}
		LLViewerObject* root_object = object->getRootEdit();

		if (!object->isAvatar() && object->permModify() &&
			object->permMove() && !object->isPermanentEnforced() &&
			(!root_object || !root_object->isPermanentEnforced()))
		{
			selectNode->mLastRotation = object->getRotation();
			selectNode->mLastPositionLocal = object->getPosition();
		}
	}

	gSelectMgr.updateSelectionCenter();

	// RN: just clear focus so camera doesn't follow spurious object updates
	gAgent.clearFocusObject();
	dialog_refresh_all();
}

void LLManipRotate::renderActiveRing(F32 radius, F32 width,
									 const LLColor4& front_color,
									 const LLColor4& back_color)
{
	LLGLEnable cull_face(GL_CULL_FACE);

	gl_ring(radius, width, back_color, back_color * 0.5f, CIRCLE_STEPS, false);
	gl_ring(radius, width, back_color, back_color * 0.5f, CIRCLE_STEPS, true);

	{
		LLGLDepthTest gls_depth(GL_FALSE);
		gl_ring(radius, width, front_color, front_color * 0.5f, CIRCLE_STEPS,
				false);
		gl_ring(radius, width, front_color, front_color * 0.5f, CIRCLE_STEPS,
				true);
	}
}

void LLManipRotate::renderSnapGuides()
{
	if (!getSnapEnabled())
	{
		return;
	}

	LLVector3 grid_origin;
	LLVector3 grid_scale;
	LLQuaternion grid_rotation;

	gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale, true);

	LLVector3 constraint_axis = getConstraintAxis();

	LLVector3 center = gAgent.getPosAgentFromGlobal(mRotationCenter);
	LLVector3 cam_at_axis;
	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		cam_at_axis.set(1.f, 0.f, 0.f);
	}
	else
	{
		cam_at_axis = center - gAgent.getCameraPositionAgent();
		cam_at_axis.normalize();
	}

	LLVector3 world_snap_axis;
	LLVector3 test_axis = constraint_axis;

	bool constrain_to_ref_object = false;
	if (mObjectSelection->getSelectType() == SELECT_TYPE_ATTACHMENT &&
		isAgentAvatarValid())
	{
		test_axis = test_axis * ~grid_rotation;
	}
	else if (gSelectMgr.getGridMode() == GRID_MODE_REF_OBJECT)
	{
		test_axis = test_axis * ~grid_rotation;
		constrain_to_ref_object = true;
	}

	test_axis.abs();

	// Find closest global/reference axis to local constraint axis;
	if (test_axis.mV[VX] > test_axis.mV[VY] &&
		test_axis.mV[VX] > test_axis.mV[VZ])
	{
		world_snap_axis = LLVector3::y_axis;
	}
	else if (test_axis.mV[VY] > test_axis.mV[VZ])
	{
		world_snap_axis = LLVector3::z_axis;
	}
	else
	{
		world_snap_axis = LLVector3::x_axis;
	}

	LLVector3 projected_snap_axis = world_snap_axis;
	if (mObjectSelection->getSelectType() == SELECT_TYPE_ATTACHMENT &&
		isAgentAvatarValid())
	{
		projected_snap_axis = projected_snap_axis * grid_rotation;
	}
	else if (constrain_to_ref_object)
	{
		projected_snap_axis = projected_snap_axis * grid_rotation;
	}

	// Project world snap axis onto constraint plane
	projected_snap_axis -= projected_vec(projected_snap_axis, constraint_axis);
	projected_snap_axis.normalize();

	S32 num_rings = mCamEdgeOn ? 2 : 1;
	for (S32 ring_num = 0; ring_num < num_rings; ring_num++)
	{
		LLVector3 center = gAgent.getPosAgentFromGlobal(mRotationCenter);

		if (mCamEdgeOn)
		{
			// Draw two opposing rings
			if (ring_num == 0)
			{
				center += constraint_axis * mRadiusMeters * 0.5f;
			}
			else
			{
				center -= constraint_axis * mRadiusMeters * 0.5f;
			}
		}

		LLGLDepthTest gls_depth(GL_FALSE);
		for (S32 pass = 0; pass < 3; ++pass)
		{
			// Render snap guide ring
			gGL.pushMatrix();

			LLQuaternion snap_guide_rot;
			F32 angle_radians, x, y, z;
			snap_guide_rot.shortestArc(LLVector3::z_axis, getConstraintAxis());
			snap_guide_rot.getAngleAxis(&angle_radians, &x, &y, &z);
			gGL.translatef(center.mV[VX], center.mV[VY], center.mV[VZ]);
			gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

			LLColor4 line_color = setupSnapGuideRenderPass(pass);

			gGL.color4fv(line_color.mV);

			if (mCamEdgeOn)
			{
				// render an arc
				LLVector3 edge_normal = cam_at_axis % constraint_axis;
				edge_normal.normalize();
				LLVector3 x_axis_snap = LLVector3::x_axis * snap_guide_rot;
				LLVector3 y_axis_snap = LLVector3::y_axis * snap_guide_rot;

				F32 end_angle = atan2f(y_axis_snap * edge_normal, x_axis_snap *
									   edge_normal);
				F32 start_angle = end_angle - F_PI;
				gl_arc_2d(0.f, 0.f, mRadiusMeters * SNAP_GUIDE_INNER_RADIUS,
						  CIRCLE_STEPS, false, start_angle, end_angle);
			}
			else
			{
				gl_circle_2d(0.f, 0.f, mRadiusMeters * SNAP_GUIDE_INNER_RADIUS,
							 CIRCLE_STEPS, false);
			}
			gGL.popMatrix();

			// *TODO: translate
			static const std::string up_str("Up");
			static const std::string dwn_str("Down");
			static const std::string bck_str("Back");
			static const std::string fwd_str("Forward");
			static const std::string lft_str("Left");
			static const std::string rgt_str("Right");
			static const std::string nth_str("North");
			static const std::string sth_str("South");
			static const std::string wst_str("West");
			static const std::string est_str("East");
			bool is_attachment = mObjectSelection->isAttachment();
			const std::string& dir1_str = is_attachment ? fwd_str : est_str;
			const std::string& dir2_str = is_attachment ? lft_str : nth_str;
			const std::string& dir3_str = is_attachment ? rgt_str : sth_str;
			const std::string& dir4_str = is_attachment ? bck_str : wst_str;

			for (S32 i = 0; i < 64; i++)
			{
				bool render_text = true;
				F32 deg = 5.625f * (F32)i;
				LLVector3 inner_point;
				LLVector3 outer_point;
				LLVector3 text_point;
				LLQuaternion rot(deg * DEG_TO_RAD, constraint_axis);
				gGL.begin(LLRender::LINES);
				{
					inner_point = (projected_snap_axis * mRadiusMeters *
								   SNAP_GUIDE_INNER_RADIUS * rot) + center;
					F32 tick_length = 0.f;
					if (i % 16 == 0)
					{
						tick_length = mRadiusMeters *
									  (SNAP_GUIDE_RADIUS_1 - SNAP_GUIDE_INNER_RADIUS);
					}
					else if (i % 8 == 0)
					{
						tick_length = mRadiusMeters *
									  (SNAP_GUIDE_RADIUS_2 - SNAP_GUIDE_INNER_RADIUS);
					}
					else if (i % 4 == 0)
					{
						tick_length = mRadiusMeters *
									  (SNAP_GUIDE_RADIUS_3 - SNAP_GUIDE_INNER_RADIUS);
					}
					else if (i % 2 == 0)
					{
						tick_length = mRadiusMeters *
									  (SNAP_GUIDE_RADIUS_4 - SNAP_GUIDE_INNER_RADIUS);
					}
					else
					{
						tick_length = mRadiusMeters *
									  (SNAP_GUIDE_RADIUS_5 - SNAP_GUIDE_INNER_RADIUS);
					}

					if (mCamEdgeOn)
					{
						// don't draw ticks that are on back side of circle
						F32 dot = cam_at_axis * (projected_snap_axis * rot);
						if (dot > 0.f)
						{
							outer_point = inner_point;
							render_text = false;
						}
						else
						{
							if (ring_num == 0)
							{
								outer_point = inner_point +
											  (constraint_axis * tick_length) *
											  rot;
							}
							else
							{
								outer_point = inner_point -
											  (constraint_axis * tick_length) *
											  rot;
							}
						}
					}
					else
					{
						outer_point = inner_point +
									  (projected_snap_axis * tick_length) *
									  rot;
					}

					text_point = outer_point +
								 (projected_snap_axis * mRadiusMeters * 0.1f) *
								 rot;

					gGL.vertex3fv(inner_point.mV);
					gGL.vertex3fv(outer_point.mV);
				}
				gGL.end();

				// RN: text rendering does own shadow pass, so only render once
				if (pass == 1 && render_text && i % 16 == 0)
				{
					if (world_snap_axis.mV[VX])
					{
						if (i == 0)
						{
							renderTickText(text_point, dir1_str);
						}
						else if (i == 16)
						{
							if (constraint_axis.mV[VZ] > 0.f)
							{
								renderTickText(text_point, dir2_str);
							}
							else
							{
								renderTickText(text_point, dir3_str);
							}
						}
						else if (i == 32)
						{
							renderTickText(text_point, dir4_str);
						}
						else if (constraint_axis.mV[VZ] > 0.f)
						{
							renderTickText(text_point, dir3_str);
						}
						else
						{
							renderTickText(text_point, dir2_str);
						}
					}
					else if (world_snap_axis.mV[VY])
					{
						if (i == 0)
						{
							renderTickText(text_point, dir2_str);
						}
						else if (i == 16)
						{
							if (constraint_axis.mV[VX] > 0.f)
							{
								renderTickText(text_point, up_str);
							}
							else
							{
								renderTickText(text_point, dwn_str);
							}
						}
						else if (i == 32)
						{
							renderTickText(text_point, dir3_str);
						}
						else if (constraint_axis.mV[VX] > 0.f)
						{
							renderTickText(text_point, dwn_str);
						}
						else
						{
							renderTickText(text_point, up_str);
						}
					}
					else if (world_snap_axis.mV[VZ])
					{
						if (i == 0)
						{
							renderTickText(text_point, up_str);
						}
						else if (i == 16)
						{
							if (constraint_axis.mV[VY] > 0.f)
							{
								renderTickText(text_point, dir1_str);
							}
							else
							{
								renderTickText(text_point, dir4_str);
							}
						}
						else if (i == 32)
						{
							renderTickText(text_point, dwn_str);
						}
						else if (constraint_axis.mV[VY] > 0.f)
						{
							renderTickText(text_point, dir4_str);
						}
						else
						{
							renderTickText(text_point, dir1_str);
						}
					}
				}
				gGL.color4fv(line_color.mV);
			}

			// Now render projected object axis
			if (mInSnapRegime)
			{
				LLVector3 object_axis;
				getObjectAxisClosestToMouse(object_axis);

				// Project onto constraint plane
				LLSelectNode* first_node = mObjectSelection->getFirstMoveableNode(true);
				object_axis = object_axis * first_node->getObject()->getRenderRotation();
				object_axis = object_axis - (object_axis * getConstraintAxis()) * getConstraintAxis();
				object_axis.normalize();
				object_axis = object_axis * SNAP_GUIDE_INNER_RADIUS * mRadiusMeters + center;
				LLVector3 line_start = center;

				gGL.begin(LLRender::LINES);
				{
					gGL.vertex3fv(line_start.mV);
					gGL.vertex3fv(object_axis.mV);
				}
				gGL.end();

				// Draw snap guide arrow
				gGL.begin(LLRender::TRIANGLES);
				{
					LLVector3 arrow_dir;
					LLVector3 arrow_span = (object_axis - line_start) % getConstraintAxis();
					arrow_span.normalize();

					arrow_dir = mCamEdgeOn ? getConstraintAxis() : object_axis - line_start;
					arrow_dir.normalize();
					if (ring_num == 1)
					{
						arrow_dir *= -1.f;
					}
					gGL.vertex3fv((object_axis + arrow_dir * mRadiusMeters * 0.1f).mV);
					gGL.vertex3fv((object_axis + arrow_span * mRadiusMeters * 0.1f).mV);
					gGL.vertex3fv((object_axis - arrow_span * mRadiusMeters * 0.1f).mV);
				}
				gGL.end();

				{
					LLGLDepthTest gls_depth(GL_TRUE);
					gGL.begin(LLRender::LINES);
					{
						gGL.vertex3fv(line_start.mV);
						gGL.vertex3fv(object_axis.mV);
					}
					gGL.end();

					// Draw snap guide arrow
					gGL.begin(LLRender::TRIANGLES);
					{
						LLVector3 arrow_dir;
						LLVector3 arrow_span = (object_axis - line_start) % getConstraintAxis();
						arrow_span.normalize();

						arrow_dir = mCamEdgeOn ? getConstraintAxis() : object_axis - line_start;
						arrow_dir.normalize();
						if (ring_num == 1)
						{
							arrow_dir *= -1.f;
						}

						gGL.vertex3fv((object_axis + arrow_dir * mRadiusMeters * 0.1f).mV);
						gGL.vertex3fv((object_axis + arrow_span * mRadiusMeters * 0.1f).mV);
						gGL.vertex3fv((object_axis - arrow_span * mRadiusMeters * 0.1f).mV);
					}
					gGL.end();
				}
			}
		}
	}

	// Render help text
	if (mObjectSelection->getSelectType() != SELECT_TYPE_HUD)
	{
		if (mHelpTextTimer.getElapsedTimeF32() <
				sHelpTextVisibleTime + sHelpTextFadeTime &&
			sNumTimesHelpTextShown < sMaxTimesShowHelpText)
		{
			LLVector3 sel_center = gSelectMgr.getSavedBBoxOfSelection().getCenterAgent();
			LLVector3 offset_dir = gViewerCamera.getUpAxis();

			static LLCachedControl<F32> grid_alpha(gSavedSettings,
												   "GridOpacity");

			LLVector3 help_text_pos = sel_center + mRadiusMeters * 3.f * offset_dir;

			LLColor4 help_text_color = LLColor4::white;
			help_text_color.mV[VALPHA] =
				clamp_rescale(mHelpTextTimer.getElapsedTimeF32(),
							  sHelpTextVisibleTime,
							  sHelpTextVisibleTime + sHelpTextFadeTime,
							  grid_alpha, 0.f);

			const LLFontGL* big_fontp = LLFontGL::getFontSansSerif();

			static LLWString text1 =
				utf8str_to_wstring("Move mouse cursor over ruler");
			static F32 text1_width = -0.5f *
									 big_fontp->getWidthF32(text1.c_str());
			hud_render_text(text1, help_text_pos, *big_fontp, LLFontGL::NORMAL,
							text1_width, 3.f, help_text_color, false);

			static LLWString text2 = utf8str_to_wstring("to snap to grid");
			static F32 text2_width = -0.5f *
									 big_fontp->getWidthF32(text2.c_str());
			help_text_pos -= offset_dir * mRadiusMeters * 0.4f;
			hud_render_text(text2, help_text_pos, *big_fontp, LLFontGL::NORMAL,
							text2_width, 3.f, help_text_color, false);
		}
	}
}

// Returns true if center of sphere is visible. Also sets a bunch of member
// variables that are used later (e.g. mCenterToCam)
bool LLManipRotate::updateVisiblity()
{
	// We do not want to recalculate the center of the selection during a drag.
	// Due to packet delays, sometimes half the objects in the selection have
	// their new position and half have their old one. This creates subtle
	// errors in the computed center position for that frame. Unfortunately,
	// these errors accumulate.  The result is objects seem to "fly apart"
	// during rotations. JC - 03.26.2002
	if (!hasMouseCapture())
	{
		mRotationCenter = gAgent.getPosGlobalFromAgent(getPivotPoint());
	}

	bool visible = false;
	LLVector3 center = gAgent.getPosAgentFromGlobal(mRotationCenter);
	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		F32 zoom = gAgent.mHUDCurZoom;
		mCenterToCam = LLVector3(-1.f / zoom, 0.f, 0.f);
		mCenterToCamNorm = mCenterToCam;
		mCenterToCamMag = mCenterToCamNorm.normalize();

		mRadiusMeters = RADIUS_PIXELS / (F32)gViewerCamera.getViewHeightInPixels();
		mRadiusMeters /= zoom;

		mCenterToProfilePlaneMag = mRadiusMeters * mRadiusMeters / mCenterToCamMag;
		mCenterToProfilePlane = -mCenterToProfilePlaneMag * mCenterToCamNorm;

		mCenterScreen.set((S32)((0.5f - mRotationCenter.mdV[VY]) /
								zoom * gViewerWindowp->getWindowWidth()),
						  (S32)((mRotationCenter.mdV[VZ] + 0.5f) /
								zoom * gViewerWindowp->getWindowHeight()));
		visible = true;
	}
	else
	{
		visible = gViewerCamera.projectPosAgentToScreen(center, mCenterScreen);
		if (visible)
		{
			mCenterToCam = gAgent.getCameraPositionAgent() - center;
			mCenterToCamNorm = mCenterToCam;
			mCenterToCamMag = mCenterToCamNorm.normalize();
			LLVector3 cameraAtAxis = gViewerCamera.getAtAxis();
			cameraAtAxis.normalize();

			F32 z_dist = -1.f * (mCenterToCam * cameraAtAxis);

			// Do not drag manip if object too far away
			static LLCachedControl<bool> limit_select_distance(gSavedSettings,
															   "LimitSelectDistance");
			static LLCachedControl<F32> max_select_distance(gSavedSettings,
															"MaxSelectDistance");
			if (limit_select_distance &&
				dist_vec(gAgent.getPositionAgent(),
						 center) > max_select_distance)
			{
				visible = false;
			}

			if (mCenterToCamMag > 0.001f)
			{
				F32 fraction_of_fov = RADIUS_PIXELS /
									  (F32)gViewerCamera.getViewHeightInPixels();
				F32 apparent_angle = fraction_of_fov * gViewerCamera.getView();
				mRadiusMeters = z_dist * tanf(apparent_angle);

				mCenterToProfilePlaneMag = mRadiusMeters * mRadiusMeters /
										   mCenterToCamMag;
				mCenterToProfilePlane = -mCenterToProfilePlaneMag *
										mCenterToCamNorm;
			}
			else
			{
				visible = false;
			}
		}
	}

	mCamEdgeOn = false;
	F32 axis_onto_cam =
		mManipPart >= LL_ROT_X ? fabsf(getConstraintAxis() * mCenterToCamNorm)
							   : 0.f;
	if (axis_onto_cam < AXIS_ONTO_CAM_TOLERANCE)
	{
		mCamEdgeOn = true;
	}

	return visible;
}

LLQuaternion LLManipRotate::dragUnconstrained(S32 x, S32 y)
{
	LLVector3 cam = gAgent.getCameraPositionAgent();
	LLVector3 center =  gAgent.getPosAgentFromGlobal(mRotationCenter);

	mMouseCur = intersectMouseWithSphere(x, y, center, mRadiusMeters);

	F32 delta_x = (F32)(mCenterScreen.mX - x);
	F32 delta_y = (F32)(mCenterScreen.mY - y);

	F32 dist_from_sphere_center = sqrtf(delta_x * delta_x + delta_y * delta_y);

	LLVector3 axis = mMouseDown % mMouseCur;
	F32 angle = atan2f(sqrtf(axis * axis), mMouseDown * mMouseCur);
	axis.normalize();
	LLQuaternion sphere_rot(angle, axis);

	if (is_approx_zero(1.f - mMouseDown * mMouseCur))
	{
		return LLQuaternion::DEFAULT;
	}
	else if (dist_from_sphere_center < RADIUS_PIXELS)
	{
		return sphere_rot;
	}
	else
	{
		LLVector3 intersection;
		getMousePointOnPlaneAgent(intersection, x, y,
								  center + mCenterToProfilePlane,
								  mCenterToCamNorm);

		// Amount dragging in sphere from center to periphery would rotate
		// object
		F32 in_sphere_angle = F_PI_BY_TWO;
		F32 dist_to_tangent_point = mRadiusMeters;
		if (!is_approx_zero(mCenterToProfilePlaneMag))
		{
			dist_to_tangent_point = sqrtf(mRadiusMeters * mRadiusMeters -
										  mCenterToProfilePlaneMag *
										  mCenterToProfilePlaneMag);
			in_sphere_angle = atan2f(dist_to_tangent_point,
									 mCenterToProfilePlaneMag);
		}

		LLVector3 profile_center_to_intersection =
			intersection - (center + mCenterToProfilePlane);
		F32 dist_to_intersection = profile_center_to_intersection.normalize();
		F32 angle = (-1.f + dist_to_intersection / dist_to_tangent_point) *
					in_sphere_angle;

		LLVector3 axis;
		if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
		{
			axis = LLVector3(-1.f, 0.f, 0.f) % profile_center_to_intersection;
		}
		else
		{
			axis = (cam - center) % profile_center_to_intersection;
			axis.normalize();
		}
		return sphere_rot * LLQuaternion(angle, axis);
	}
}

LLVector3 LLManipRotate::getConstraintAxis()
{
	LLVector3 axis;
	if (LL_ROT_ROLL == mManipPart)
	{
		axis = mCenterToCamNorm;
	}
	else
	{
		S32 axis_dir = mManipPart - LL_ROT_X;
		if (axis_dir >= 0 && axis_dir < 3)
		{
			axis.mV[axis_dir] = 1.f;
		}
		else
		{
			llwarns << "Got bogus hit part " << mManipPart << llendl;
			llassert(false);
			axis.mV[0] = 1.f;
		}

		LLVector3 grid_origin;
		LLVector3 grid_scale;
		LLQuaternion grid_rotation;

		gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);

		LLSelectNode* first_node =
			mObjectSelection->getFirstMoveableNode(true);
		if (first_node)
		{
			// *FIX: get agent local attachment grid working
			// Put rotation into frame of first selected root object
			axis = axis * grid_rotation;
		}
	}

	return axis;
}

LLQuaternion LLManipRotate::dragConstrained(S32 x, S32 y)
{
	LLSelectNode* first_object_node =
		mObjectSelection->getFirstMoveableNode(true);
	LLVector3 constraint_axis = getConstraintAxis();
	LLVector3 center = gAgent.getPosAgentFromGlobal(mRotationCenter);

	F32 angle = 0.f;

	// build snap axes
	LLVector3 grid_origin;
	LLVector3 grid_scale;
	LLQuaternion grid_rotation;

	gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);

	LLVector3 axis1;
	LLVector3 axis2;

	LLVector3 test_axis = constraint_axis;
	if (mObjectSelection->getSelectType() == SELECT_TYPE_ATTACHMENT &&
		isAgentAvatarValid())
	{
		test_axis = test_axis * ~grid_rotation;
	}
	else if (gSelectMgr.getGridMode() == GRID_MODE_REF_OBJECT)
	{
		test_axis = test_axis * ~grid_rotation;
	}
	test_axis.abs();

	// Find closest global axis to constraint axis;
	if (test_axis.mV[VX] > test_axis.mV[VY] && test_axis.mV[VX] > test_axis.mV[VZ])
	{
		axis1 = LLVector3::y_axis;
	}
	else if (test_axis.mV[VY] > test_axis.mV[VZ])
	{
		axis1 = LLVector3::z_axis;
	}
	else
	{
		axis1 = LLVector3::x_axis;
	}

	if (mObjectSelection->getSelectType() == SELECT_TYPE_ATTACHMENT &&
		isAgentAvatarValid())
	{
		axis1 = axis1 * grid_rotation;
	}
	else if (gSelectMgr.getGridMode() == GRID_MODE_REF_OBJECT)
	{
		axis1 = axis1 * grid_rotation;
	}

	// Project axis onto constraint plane
	axis1 -= (axis1 * constraint_axis) * constraint_axis;
	axis1.normalize();

	// Calculate third and final axis
	axis2 = constraint_axis % axis1;

	const F32 snap_radius = SNAP_GUIDE_INNER_RADIUS * mRadiusMeters;

	if (mCamEdgeOn)
	{
		// We are looking at the ring edge-on.
		LLVector3 snap_plane_center = center +
									  (constraint_axis * mRadiusMeters * 0.5f);
		LLVector3 cam_to_snap_plane;
		if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
		{
			cam_to_snap_plane.set(1.f, 0.f, 0.f);
		}
		else
		{
			cam_to_snap_plane = snap_plane_center -
								gAgent.getCameraPositionAgent();
			cam_to_snap_plane.normalize();
		}

		LLVector3 projected_mouse;
		bool hit = getMousePointOnPlaneAgent(projected_mouse, x, y,
											 snap_plane_center,
											 constraint_axis);
		projected_mouse -= snap_plane_center;

		if (getSnapEnabled())
		{
			S32 snap_plane = 0;

			F32 dot = cam_to_snap_plane * constraint_axis;
			if (fabsf(dot) < 0.01f)
			{
				// Looking at ring edge on, project onto view plane and check
				// if mouse is past ring
				getMousePointOnPlaneAgent(projected_mouse, x, y,
										  snap_plane_center,
										  cam_to_snap_plane);
				projected_mouse -= snap_plane_center;
				dot = projected_mouse * constraint_axis;
				if (projected_mouse * constraint_axis > 0)
				{
					snap_plane = 1;
				}
				projected_mouse -= dot * constraint_axis;
			}
			else if (dot > 0.f)
			{
				// Look for mouse position outside and in front of snap circle
				if (hit && projected_mouse * cam_to_snap_plane < 0.f &&
					projected_mouse.length() > snap_radius)
				{
					snap_plane = 1;
				}
			}
			// Look for mouse position inside or in back of snap circle
			else if (!hit || projected_mouse * cam_to_snap_plane > 0.f ||
					 projected_mouse.length() < snap_radius)
			{
				snap_plane = 1;
			}

			if (snap_plane == 0)
			{
				// Try other plane
				snap_plane_center = center -
									constraint_axis * mRadiusMeters * 0.5f;
				if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
				{
					cam_to_snap_plane.set(1.f, 0.f, 0.f);
				}
				else
				{
					cam_to_snap_plane = snap_plane_center -
										gAgent.getCameraPositionAgent();
					cam_to_snap_plane.normalize();
				}

				hit = getMousePointOnPlaneAgent(projected_mouse, x, y,
												snap_plane_center,
												constraint_axis);
				projected_mouse -= snap_plane_center;

				dot = cam_to_snap_plane * constraint_axis;
				if (fabsf(dot) < 0.01f)
				{
					// Looking at ring edge on, project onto view plane and
					// check if mouse is past ring
					getMousePointOnPlaneAgent(projected_mouse, x, y,
											  snap_plane_center,
											  cam_to_snap_plane);
					projected_mouse -= snap_plane_center;
					dot = projected_mouse * constraint_axis;
					if (projected_mouse * constraint_axis < 0)
					{
						snap_plane = 2;
					}
					projected_mouse -= dot * constraint_axis;
				}
				else if (dot < 0.f)
				{
					// Look for mouse position outside and in front of snap
					// circle
					if (hit && projected_mouse * cam_to_snap_plane < 0.f &&
						projected_mouse.length() > snap_radius)
					{
						snap_plane = 2;
					}
				}
				// Look for mouse position inside or in back of snap circle
				else if (!hit || projected_mouse * cam_to_snap_plane > 0.f ||
						 projected_mouse.length() < snap_radius)
				{
					snap_plane = 2;
				}
			}

			if (snap_plane > 0)
			{
				LLVector3 cam_at_axis;
				if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
				{
					cam_at_axis.set(1.f, 0.f, 0.f);
				}
				else
				{
					cam_at_axis = snap_plane_center -
								  gAgent.getCameraPositionAgent();
					cam_at_axis.normalize();
				}

				// First, project mouse onto screen plane at point tangent to
				// rotation radius.
				getMousePointOnPlaneAgent(projected_mouse, x, y,
										  snap_plane_center, cam_at_axis);
				// Project that point onto rotation plane
				projected_mouse -= snap_plane_center;
				projected_mouse -= projected_vec(projected_mouse,
												 constraint_axis);

				F32 mouse_lateral_dist = llmin(snap_radius,
											   projected_mouse.length());
				F32 mouse_depth = snap_radius;
				if (fabsf(mouse_lateral_dist) > 0.01f)
				{
					mouse_depth = sqrtf(snap_radius * snap_radius -
										mouse_lateral_dist * mouse_lateral_dist);
				}
				LLVector3 projected_camera_at = cam_at_axis -
												projected_vec(cam_at_axis,
															  constraint_axis);
				projected_mouse -= mouse_depth * projected_camera_at;

				if (!mInSnapRegime)
				{
					mSmoothRotate = true;
				}
				mInSnapRegime = true;
				// 0 to 360 deg
				F32 mouse_angle = fmodf(atan2f(projected_mouse * axis1,
											   projected_mouse * axis2) *
										RAD_TO_DEG + 360.f, 360.f);

				F32 relative_mouse_angle = fmodf(mouse_angle + SNAP_ANGLE_DETENTE / 2,
												 SNAP_ANGLE_INCREMENT);

				LLVector3 object_axis;
				getObjectAxisClosestToMouse(object_axis);
				object_axis = object_axis * first_object_node->mSavedRotation;

				// Project onto constraint plane
				object_axis = object_axis -
							  object_axis * getConstraintAxis() * getConstraintAxis();
				object_axis.normalize();

				if (relative_mouse_angle < SNAP_ANGLE_DETENTE)
				{
					F32 quantized_mouse_angle = mouse_angle - relative_mouse_angle +
												SNAP_ANGLE_DETENTE * 0.5f;
					angle = quantized_mouse_angle * DEG_TO_RAD -
							atan2f(object_axis * axis1, object_axis * axis2);
				}
				else
				{
					angle = mouse_angle * DEG_TO_RAD -
							atan2f(object_axis * axis1, object_axis * axis2);
				}
				return LLQuaternion(-angle, constraint_axis);
			}
			else
			{
				if (mInSnapRegime)
				{
					mSmoothRotate = true;
				}
				mInSnapRegime = false;
			}
		}
		else
		{
			if (mInSnapRegime)
			{
				mSmoothRotate = true;
			}
			mInSnapRegime = false;
		}

		if (!mInSnapRegime)
		{
			LLVector3 up_from_axis = mCenterToCamNorm % constraint_axis;
			up_from_axis.normalize();
			LLVector3 cur_intersection;
			getMousePointOnPlaneAgent(cur_intersection, x, y, center,
									  mCenterToCam);
			cur_intersection -= center;
			mMouseCur = projected_vec(cur_intersection, up_from_axis);
			F32 mouse_depth = snap_radius;
			F32 mouse_dist_sqrd = mMouseCur.lengthSquared();
			if (mouse_dist_sqrd > 0.0001f)
			{
				mouse_depth = sqrtf(snap_radius * snap_radius -
									mouse_dist_sqrd);
			}
			LLVector3 projected_center_to_cam = mCenterToCamNorm -
												projected_vec(mCenterToCamNorm,
															  constraint_axis);
			mMouseCur += mouse_depth * projected_center_to_cam;

			F32 dist = cur_intersection * up_from_axis - mMouseDown * up_from_axis;
			angle = dist / snap_radius * -F_PI_BY_TWO;
		}
	}
	else
	{
		LLVector3 projected_mouse;
		getMousePointOnPlaneAgent(projected_mouse, x, y, center, constraint_axis);
		projected_mouse -= center;
		mMouseCur = projected_mouse;
		mMouseCur.normalize();

		if (!first_object_node)
		{
			return LLQuaternion::DEFAULT;
		}

		if (getSnapEnabled() && projected_mouse.length() > snap_radius)
		{
			if (!mInSnapRegime)
			{
				mSmoothRotate = true;
			}
			mInSnapRegime = true;
			// 0 to 360 deg
			F32 mouse_angle = fmodf(atan2f(projected_mouse * axis1,
										   projected_mouse * axis2) *
									RAD_TO_DEG + 360.f, 360.f);

			F32 relative_mouse_angle = fmodf(mouse_angle + SNAP_ANGLE_DETENTE / 2,
											 SNAP_ANGLE_INCREMENT);
			//fmodf(ll_round(mouse_angle * RAD_TO_DEG, 7.5f) + 360.f, 360.f);

			LLVector3 object_axis;
			getObjectAxisClosestToMouse(object_axis);
			object_axis = object_axis * first_object_node->mSavedRotation;

			// project onto constraint plane
			object_axis = object_axis - object_axis * getConstraintAxis() *
						  getConstraintAxis();
			object_axis.normalize();

			if (relative_mouse_angle < SNAP_ANGLE_DETENTE)
			{
				F32 quantized_mouse_angle = mouse_angle - relative_mouse_angle +
											SNAP_ANGLE_DETENTE * 0.5f;
				angle = quantized_mouse_angle * DEG_TO_RAD -
						atan2f(object_axis * axis1, object_axis * axis2);
			}
			else
			{
				angle = mouse_angle * DEG_TO_RAD -
						atan2f(object_axis * axis1, object_axis * axis2);
			}
			return LLQuaternion(-angle, constraint_axis);
		}
		else
		{
			if (mInSnapRegime)
			{
				mSmoothRotate = true;
			}
			mInSnapRegime = false;
		}

		LLVector3 axis = mMouseDown % mMouseCur;
		angle = atan2f(sqrtf(axis * axis), mMouseCur * mMouseDown);
		F32 dir = axis * constraint_axis;  // cross product
		if (dir < 0.f)
		{
			angle *= -1.f;
		}
	}

	static LLCachedControl<F32> rotation_step(gSavedSettings, "RotationStep");
	F32 step_size = DEG_TO_RAD * rotation_step;
	angle -= fmod(angle, step_size);

	return LLQuaternion(angle, constraint_axis);
}

LLVector3 LLManipRotate::intersectMouseWithSphere(S32 x, S32 y,
												  const LLVector3& sphere_center,
												  F32 sphere_radius)
{
	LLVector3 ray_pt;
	LLVector3 ray_dir;
	mouseToRay(x, y, &ray_pt, &ray_dir);
	return intersectRayWithSphere(ray_pt, ray_dir, sphere_center, sphere_radius);
}

LLVector3 LLManipRotate::intersectRayWithSphere(const LLVector3& ray_pt,
												const LLVector3& ray_dir,
												const LLVector3& sphere_center,
												F32 sphere_radius)
{
	LLVector3 ray_pt_to_center = sphere_center - ray_pt;
	F32 center_distance = ray_pt_to_center.normalize();

	F32 dot = ray_dir * ray_pt_to_center;

	if (dot == 0.f)
	{
		return LLVector3::zero;
	}

	// Point which ray hits plane centered on sphere origin, facing ray origin
	LLVector3 intersection_sphere_plane =
		ray_pt + ray_dir * center_distance / dot;
	// Vector from sphere origin to the point, normalized to sphere radius
	LLVector3 sphere_center_to_intersection =
		 (intersection_sphere_plane - sphere_center) / sphere_radius;

	LLVector3 result;
	F32 dist_squared = sphere_center_to_intersection.lengthSquared();
	if (dist_squared > 1.f)
	{
		result = sphere_center_to_intersection;
		result.normalize();
	}
	else
	{
		result = sphere_center_to_intersection -
				 ray_dir * sqrtf(1.f - dist_squared);
	}
	return result;
}

// Utility function.  Should probably be moved to another class.
//static
void LLManipRotate::mouseToRay(S32 x, S32 y, LLVector3* ray_pt,
							   LLVector3* ray_dir)
{
	if (gSelectMgr.getSelection()->getSelectType() == SELECT_TYPE_HUD)
	{
		F32 zoom = gAgent.mHUDCurZoom;
		F32 mouse_x = ((F32)x / gViewerWindowp->getWindowWidth() - 0.5f) / zoom;
		F32 mouse_y = ((F32)y / gViewerWindowp->getWindowHeight() - 0.5f) / zoom;

		*ray_pt = LLVector3(-1.f, -mouse_x, mouse_y);
		*ray_dir = LLVector3(1.f, 0.f, 0.f);
	}
	else
	{
		*ray_pt = gAgent.getCameraPositionAgent();
		gViewerCamera.projectScreenToPosAgent(x, y, ray_dir);
		*ray_dir -= *ray_pt;
		ray_dir->normalize();
	}
}

void LLManipRotate::highlightManipulators(S32 x, S32 y)
{
	mHighlightedPart = LL_NO_PART;

	//LLBBox bbox = gSelectMgr.getBBoxOfSelection();
	LLViewerObject* first_object = mObjectSelection->getFirstMoveableObject(true);

	if (!first_object)
	{
		return;
	}

	LLVector3 rotation_center = gAgent.getPosAgentFromGlobal(mRotationCenter);
	LLVector3 mouse_dir_x;
	LLVector3 mouse_dir_y;
	LLVector3 mouse_dir_z;
	LLVector3 intersection_roll;

	LLVector3 grid_origin;
	LLVector3 grid_scale;
	LLQuaternion grid_rotation;

	gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);

	LLVector3 rot_x_axis = LLVector3::x_axis * grid_rotation;
	LLVector3 rot_y_axis = LLVector3::y_axis * grid_rotation;
	LLVector3 rot_z_axis = LLVector3::z_axis * grid_rotation;

	F32 proj_rot_x_axis = fabsf(rot_x_axis * mCenterToCamNorm);
	F32 proj_rot_y_axis = fabsf(rot_y_axis * mCenterToCamNorm);
	F32 proj_rot_z_axis = fabsf(rot_z_axis * mCenterToCamNorm);

	F32 min_select_distance = 0.f;
	F32 cur_select_distance = 0.f;

	// test x
	getMousePointOnPlaneAgent(mouse_dir_x, x, y, rotation_center, rot_x_axis);
	mouse_dir_x -= rotation_center;
	// push intersection point out when working at obtuse angle to make ring
	// easier to hit
	mouse_dir_x *= 1.f + (1.f - fabsf(rot_x_axis * mCenterToCamNorm)) * 0.1f;

	// test y
	getMousePointOnPlaneAgent(mouse_dir_y, x, y, rotation_center, rot_y_axis);
	mouse_dir_y -= rotation_center;
	mouse_dir_y *= 1.f + (1.f - fabsf(rot_y_axis * mCenterToCamNorm)) * 0.1f;

	// test z
	getMousePointOnPlaneAgent(mouse_dir_z, x, y, rotation_center, rot_z_axis);
	mouse_dir_z -= rotation_center;
	mouse_dir_z *= 1.f + (1.f - fabsf(rot_z_axis * mCenterToCamNorm)) * 0.1f;

	// test roll
	getMousePointOnPlaneAgent(intersection_roll, x, y, rotation_center,
							  mCenterToCamNorm);
	intersection_roll -= rotation_center;

	F32 dist_x = mouse_dir_x.normalize();
	F32 dist_y = mouse_dir_y.normalize();
	F32 dist_z = mouse_dir_z.normalize();

	F32 distance_threshold = MAX_MANIP_SELECT_DISTANCE * mRadiusMeters /
							 gViewerWindowp->getWindowHeight();

	if (fabsf(dist_x - mRadiusMeters) * llmax(0.05f, proj_rot_x_axis) <
			distance_threshold)
	{
		// selected x
		cur_select_distance = dist_x * mouse_dir_x * mCenterToCamNorm;
		if (cur_select_distance >= -0.05f &&
			(min_select_distance == 0.f ||
			 cur_select_distance > min_select_distance))
		{
			min_select_distance = cur_select_distance;
			mHighlightedPart = LL_ROT_X;
		}
	}
	if (fabsf(dist_y - mRadiusMeters) * llmax(0.05f, proj_rot_y_axis) <
			distance_threshold)
	{
		// selected y
		cur_select_distance = dist_y * mouse_dir_y * mCenterToCamNorm;
		if (cur_select_distance >= -0.05f &&
			(min_select_distance == 0.f ||
			 cur_select_distance > min_select_distance))
		{
			min_select_distance = cur_select_distance;
			mHighlightedPart = LL_ROT_Y;
		}
	}
	if (fabsf(dist_z - mRadiusMeters) * llmax(0.05f, proj_rot_z_axis) <
			distance_threshold)
	{
		// selected z
		cur_select_distance = dist_z * mouse_dir_z * mCenterToCamNorm;
		if (cur_select_distance >= -0.05f &&
			(min_select_distance == 0.f ||
			 cur_select_distance > min_select_distance))
		{
			min_select_distance = cur_select_distance;
			mHighlightedPart = LL_ROT_Z;
		}
	}

	// Test for edge-on intersections
	if (proj_rot_x_axis < 0.05f)
	{
		if ((proj_rot_y_axis > 0.05f && dist_y < mRadiusMeters &&
			 dist_y * fabsf(mouse_dir_y * rot_x_axis) < distance_threshold) ||
			(proj_rot_z_axis > 0.05f && dist_z < mRadiusMeters &&
			 dist_z * fabsf(mouse_dir_z * rot_x_axis) < distance_threshold))
		{
			mHighlightedPart = LL_ROT_X;
		}
	}

	if (proj_rot_y_axis < 0.05f)
	{
		if ((proj_rot_x_axis > 0.05f && dist_x < mRadiusMeters &&
			 dist_x * fabsf(mouse_dir_x * rot_y_axis) < distance_threshold) ||
			(proj_rot_z_axis > 0.05f && dist_z < mRadiusMeters &&
			 dist_z * fabsf(mouse_dir_z * rot_y_axis) < distance_threshold))
		{
			mHighlightedPart = LL_ROT_Y;
		}
	}

	if (proj_rot_z_axis < 0.05f)
	{
		if ((proj_rot_x_axis > 0.05f && dist_x < mRadiusMeters &&
			 dist_x * fabsf(mouse_dir_x * rot_z_axis) < distance_threshold) ||
			(proj_rot_y_axis > 0.05f && dist_y < mRadiusMeters &&
			 dist_y * fabsf(mouse_dir_y * rot_z_axis) < distance_threshold))
		{
			mHighlightedPart = LL_ROT_Z;
		}
	}

	// Test for roll
	if (mHighlightedPart == LL_NO_PART)
	{
		F32 roll_distance = intersection_roll.length();
		F32 width_meters = WIDTH_PIXELS * mRadiusMeters / RADIUS_PIXELS;

		// use larger distance threshold for roll as it is checked only if
		// something else wasn't highlighted
		if (fabsf(roll_distance - mRadiusMeters - width_meters * 2.f) <
				distance_threshold * 2.f)
		{
			mHighlightedPart = LL_ROT_ROLL;
		}
		else if (roll_distance < mRadiusMeters)
		{
			mHighlightedPart = LL_ROT_GENERAL;
		}
	}
}

S32 LLManipRotate::getObjectAxisClosestToMouse(LLVector3& object_axis)
{
	LLSelectNode* first_object_node =
		mObjectSelection->getFirstMoveableNode(true);

	if (!first_object_node)
	{
		object_axis.clear();
		return -1;
	}

	LLQuaternion obj_rotation = first_object_node->mSavedRotation;
	LLVector3 mouse_down_object = mMouseDown * ~obj_rotation;
	LLVector3 mouse_down_abs = mouse_down_object;
	mouse_down_abs.abs();

	S32 axis_index = 0;
	if (mouse_down_abs.mV[VX] > mouse_down_abs.mV[VY] &&
		mouse_down_abs.mV[VX] > mouse_down_abs.mV[VZ])
	{
		if (mouse_down_object.mV[VX] > 0.f)
		{
			object_axis = LLVector3::x_axis;
		}
		else
		{
			object_axis = LLVector3::x_axis_neg;
		}
		axis_index = VX;
	}
	else if (mouse_down_abs.mV[VY] > mouse_down_abs.mV[VZ])
	{
		if (mouse_down_object.mV[VY] > 0.f)
		{
			object_axis = LLVector3::y_axis;
		}
		else
		{
			object_axis = LLVector3::y_axis_neg;
		}
		axis_index = VY;
	}
	else
	{
		if (mouse_down_object.mV[VZ] > 0.f)
		{
			object_axis = LLVector3::z_axis;
		}
		else
		{
			object_axis = LLVector3::z_axis_neg;
		}
		axis_index = VZ;
	}

	return axis_index;
}

//virtual
bool LLManipRotate::canAffectSelection()
{
	bool can_rotate = mObjectSelection->getObjectCount() != 0;
	if (can_rotate)
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
		can_rotate = mObjectSelection->applyToObjects(&func);
	}
	return can_rotate;
}
