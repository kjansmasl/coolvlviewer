/**
 * @file llviewercamera.cpp
 * @brief LLViewerCamera class implementation
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

#include "llviewercamera.h"

#include "llgl.h"
#include "llmatrix4a.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llface.h"
//MK
#include "mkrlinterface.h"
//mk
#include "lltoolmgr.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For gCubeSnapshot
#include "llviewerjoystick.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "llworld.h"

LLViewerCamera gViewerCamera;

// Static members
LLStat LLViewerCamera::sVelocityStat;
LLStat LLViewerCamera::sAngularVelocityStat;
S32 LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

LLViewerCamera::LLViewerCamera()
:	LLCamera()
{
	calcProjection(getFar());
	mCameraFOVDefault = DEFAULT_FIELD_OF_VIEW;
	mPrevCameraFOVDefault = DEFAULT_FIELD_OF_VIEW;
	mCosHalfCameraFOV = cosf(mCameraFOVDefault * 0.5f);
	mPixelMeterRatio = 0.f;
	mScreenPixelArea = 0;
	mZoomFactor = 1.f;
	mZoomSubregion = 1;
	mAverageSpeed = 0.f;
	mAverageAngularSpeed = 0.f;
}

void LLViewerCamera::initClass()
{
	gSavedSettings.getControl("CameraAngle")->getSignal()->connect(boost::bind(&LLViewerCamera::updateCameraAngle, this, _2));
	llinfos << "Viewer camera initialized." << llendl;
}

void LLViewerCamera::updateCameraLocation(const LLVector3& center,
										  const LLVector3& up_direction,
										  const LLVector3& point_of_interest)
{
	// Do not update if we are in build mode AND avatar did not move
	if (gToolMgr.inBuildMode() &&
		!LLViewerJoystick::getInstance()->getCameraNeedsUpdate())
	{
		return;
	}

	LLVector3 last_position;
	LLVector3 last_axis;
	last_position = getOrigin();
	last_axis = getAtAxis();

	mLastPointOfInterest = point_of_interest;

	// Constrain to max distance from avatar

	LLViewerRegion* regionp = gWorld.getRegionFromPosAgent(getOrigin());
	if (!regionp)
	{
		regionp = gAgent.getRegion();
	}
	F32 water_height = regionp ? regionp->getWaterHeight() : 0.f;

	LLVector3 origin = center;
	if (origin.mV[2] > water_height)
	{
		origin.mV[2] = llmax(origin.mV[2], water_height + 0.20f);
	}
	else
	{
		origin.mV[2] = llmin(origin.mV[2], water_height - 0.20f);
	}

	setOriginAndLookAt(origin, up_direction, point_of_interest);

	mVelocityDir = center - last_position;
	F32 dpos = mVelocityDir.normalize();
	LLQuaternion rotation;
	rotation.shortestArc(last_axis, getAtAxis());

	F32 x, y, z;
	F32 drot;
	rotation.getAngleAxis(&drot, &x, &y, &z);

	sVelocityStat.addValue(dpos);
	sAngularVelocityStat.addValue(drot);

	mAverageSpeed = sVelocityStat.getMeanPerSec();
	mAverageAngularSpeed = sAngularVelocityStat.getMeanPerSec();
	mCosHalfCameraFOV = cosf(0.5f * getView() * llmax(1.0f, getAspect()));

	// update pixel meter ratio using default fov, not modified one
	mPixelMeterRatio = getViewHeightInPixels() / (2.f * tanf(mCameraFOVDefault * 0.5));
	// update screen pixel area
	mScreenPixelArea = (S32)((F32)getViewHeightInPixels() *
							 ((F32)getViewHeightInPixels() * getAspect()));
}

const LLMatrix4& LLViewerCamera::getProjection() const
{
	calcProjection(getFar());
	return mProjectionMatrix;
}

const LLMatrix4& LLViewerCamera::getModelview() const
{
	LLMatrix4 cfr(OGL_TO_CFR_ROTATION);
	getMatrixToLocal(mModelviewMatrix);
	mModelviewMatrix *= cfr;
	return mModelviewMatrix;
}

void LLViewerCamera::calcProjection(F32 far_distance) const
{
	F32 fov_y = getView();
	F32 z_far = far_distance;
	F32 z_near = getNear();
	F32 aspect = getAspect();
	F32 f = 1.f / tanf(fov_y * 0.5f);

	mProjectionMatrix.setZero();
	mProjectionMatrix.mMatrix[0][0] = f / aspect;
	mProjectionMatrix.mMatrix[1][1] = f;
	mProjectionMatrix.mMatrix[2][2] = (z_far + z_near) / (z_near - z_far);
	mProjectionMatrix.mMatrix[3][2] = 2 * z_far * z_near / (z_near - z_far);
	mProjectionMatrix.mMatrix[2][3] = -1;
}

//static
void LLViewerCamera::updateFrustumPlanes(LLCamera& camera, bool ortho,
										 bool zflip, bool no_hacks)
{
	LLVector3 frust[8];
	LLRect view_port(gGLViewport[0], gGLViewport[1] + gGLViewport[3],
					 gGLViewport[0] + gGLViewport[2], gGLViewport[1]);
	if (no_hacks)
	{
		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mBottom, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[0]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mBottom, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[1]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mTop, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[2]);
		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mTop, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[3]);


		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mBottom, 1.f),
					   gGLModelView, gGLProjection, view_port, frust[4]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mBottom, 1.f),
					   gGLModelView, gGLProjection, view_port, frust[5]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mTop, 1.f),
					   gGLModelView, gGLProjection, view_port, frust[6]);
		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mTop, 1.f),
					   gGLModelView, gGLProjection, view_port, frust[7]);
	}
	else if (zflip)
	{
		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mTop, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[0]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mTop, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[1]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mBottom, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[2]);
		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mBottom, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[3]);

		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mTop, 1.f),
					   gGLModelView, gGLProjection, view_port, frust[4]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mTop, 1.f),
					   gGLModelView, gGLProjection, view_port, frust[5]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mBottom, 1.f),
					   gGLModelView, gGLProjection, view_port, frust[6]);
		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mBottom, 1.f),
					   gGLModelView, gGLProjection, view_port, frust[7]);

		for (U32 i = 0; i < 4; ++i)
		{
			frust[i + 4] = frust[i + 4] - frust[i];
			frust[i + 4].normalize();
			frust[i + 4] = frust[i] + frust[i + 4] * camera.getFar();
		}
	}
	else
	{
		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mBottom, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[0]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mBottom, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[1]);
		gGL.unprojectf(LLVector3(view_port.mRight, view_port.mTop, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[2]);
		gGL.unprojectf(LLVector3(view_port.mLeft, view_port.mTop, 0.f),
					   gGLModelView, gGLProjection, view_port, frust[3]);

		if (ortho)
		{
			LLVector3 far_shift = camera.getAtAxis() * camera.getFar() * 2.f;
			for (U32 i = 0; i < 4; ++i)
			{
				frust[i + 4] = frust[i] + far_shift;
			}
		}
		else
		{
			for (U32 i = 0; i < 4; ++i)
			{
				LLVector3 vec = frust[i] - camera.getOrigin();
				vec.normalize();
				frust[i + 4] = camera.getOrigin() + vec * camera.getFar();
			}
		}
	}

	camera.calcAgentFrustumPlanes(frust);
}

void LLViewerCamera::setPerspective(bool for_selection, S32 x, S32 y_from_bot,
									S32 width, S32 height, bool limit_sel_dist,
									F32 z_near, F32 z_far)
{
	F32 fov_y = RAD_TO_DEG * getView();
	bool z_default_far = false;
	if (z_far <= 0)
	{
		z_default_far = true;
		z_far = getFar();
	}
	if (z_near <= 0)
	{
		z_near = getNear();
	}
	F32 aspect = getAspect();

	// Load camera view matrix
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.loadIdentity();

	LLMatrix4a proj_mat;
	proj_mat.setIdentity();

	if (for_selection)
	{
		// Make a tiny little viewport; anything drawn into this viewport will
		// be "selected"
		const LLRect& rect = gViewerWindowp->getWindowRect();
		F32 scale_x = rect.getWidth() / F32(width);
		F32 scale_y = rect.getHeight() / F32(height);
		F32 trans_x = scale_x + 2.f * (rect.mLeft - x) / (F32)width - 1.f;
		F32 trans_y = scale_y + 2.f * (rect.mBottom - y_from_bot) /
					  (F32)height - 1.f;
		// Generate a pick matrix
		proj_mat.applyScaleAffine(scale_x, scale_y, 1.f);
		proj_mat.setTranslateAffine(LLVector3(trans_x, trans_y, 0.f));

		if (limit_sel_dist)
		{
			// Select distance from control
			static LLCachedControl<F32> max_select_distance(gSavedSettings,
															"MaxSelectDistance");
			z_far = llclamp((F32)max_select_distance, 32.f, 512.f);
		}
		else
		{
			z_far = gAgent.mDrawDistance;
		}
	}
	else
	{
		// Only override the far clip if it's not passed in explicitly.
		if (z_default_far)
		{
			z_far = MAX_FAR_CLIP;
		}
		glViewport(x, y_from_bot, width, height);
		gGLViewport[0] = x;
		gGLViewport[1] = y_from_bot;
		gGLViewport[2] = width;
		gGLViewport[3] = height;
	}

	if (mZoomFactor > 1.f)
	{
		F32 offset = mZoomFactor - 1.f;
		S32 pos_y = mZoomSubregion / llceil(mZoomFactor);
		S32 pos_x = mZoomSubregion - pos_y * llceil(mZoomFactor);

		proj_mat.applyTranslationAffine(offset - (F32)pos_x * 2.f,
										offset - (F32)pos_y * 2.f, 0.f);
		proj_mat.applyScaleAffine(mZoomFactor, mZoomFactor, 1.f);
	}

	calcProjection(z_far); // Update the projection matrix cache

	proj_mat.mul(gl_perspective(fov_y, aspect, z_near, z_far));

	gGL.loadMatrix(proj_mat);

	gGLProjection = proj_mat;

	gGL.matrixMode(LLRender::MM_MODELVIEW);

	LLMatrix4a ogl_matrix;
	getOpenGLTransform(ogl_matrix.getF32ptr());

	LLMatrix4a modelview;
	modelview.setMul(OGL_TO_CFR_ROT4A, ogl_matrix);
	gGL.loadMatrix(modelview);

	if (for_selection && (width > 1 || height > 1))
	{
		F32 wwidth = (F32)gViewerWindowp->getWindowWidth();
		F32 wheight = (F32)gViewerWindowp->getWindowHeight();
		calculateFrustumPlanesFromWindow((F32)(x - width / 2) / wwidth - 0.5f,
										 (F32)(y_from_bot - height / 2) / wheight - 0.5f,
										 (F32)(x + width / 2) / wwidth - 0.5f,
										 (F32)(y_from_bot + height / 2) / wheight - 0.5f);

	}

	// if not picking and not doing a snapshot, cache various GL matrices
	if (!for_selection && mZoomFactor == 1.f)
	{
		// Save GL matrices for access elsewhere in code, especially
		// project_world_to_screen
		gGLModelView = modelview;
	}

	updateFrustumPlanes(*this);
}

// Uses the last GL matrices set in set_perspective to project a point from
// screen coordinates to the agent's region.
void LLViewerCamera::projectScreenToPosAgent(S32 screen_x, S32 screen_y,
											 LLVector3* pos_agent) const
{
	gGL.unprojectf(LLVector3(screen_x, screen_y, 0.f),
				   gGLModelView, gGLProjection,
				   LLRect(gGLViewport[0], gGLViewport[1] + gGLViewport[3],
						  gGLViewport[0] + gGLViewport[2], gGLViewport[1]),
				   *pos_agent);
}

// Uses the last GL matrices set in set_perspective to project a point from the
// agent's region space to screen coordinates. Returns true if point in within
// the current window.
bool LLViewerCamera::projectPosAgentToScreen(const LLVector3& pos_agent,
											 LLCoordGL& out_point,
											 bool clamp) const
{
	bool in_front = true;
	LLVector3 dir_to_point = pos_agent - getOrigin();
	dir_to_point /= dir_to_point.length();
	if (dir_to_point * getAtAxis() < 0.f)
	{
		if (clamp)
		{
			return false;
		}
		else
		{
			in_front = false;
		}
	}

	LLVector3 window_coordinates;
	if (gGL.projectf(pos_agent, gGLModelView, gGLProjection,
					 LLRect(gGLViewport[0], gGLViewport[1] + gGLViewport[3],
						    gGLViewport[0] + gGLViewport[2], gGLViewport[1]),
					 window_coordinates))
	{
		F32& x = window_coordinates.mV[VX];
		F32& y = window_coordinates.mV[VY];
		// convert screen coordinates to virtual UI coordinates
		x /= gViewerWindowp->getDisplayScale().mV[VX];
		y /= gViewerWindowp->getDisplayScale().mV[VY];

		// should now have the x,y coords of grab_point in screen space
		const LLRect& window_rect = gViewerWindowp->getWindowRect();

		// ...sanity check
		S32 int_x = lltrunc(x);
		S32 int_y = lltrunc(y);

		bool valid = true;

		if (clamp)
		{
			if (int_x < window_rect.mLeft)
			{
				out_point.mX = window_rect.mLeft;
				valid = false;
			}
			else if (int_x > window_rect.mRight)
			{
				out_point.mX = window_rect.mRight;
				valid = false;
			}
			else
			{
				out_point.mX = int_x;
			}

			if (int_y < window_rect.mBottom)
			{
				out_point.mY = window_rect.mBottom;
				valid = false;
			}
			else if (int_y > window_rect.mTop)
			{
				out_point.mY = window_rect.mTop;
				valid = false;
			}
			else
			{
				out_point.mY = int_y;
			}
			return valid;
		}
		else
		{
			out_point.mX = int_x;
			out_point.mY = int_y;

			if (int_x < window_rect.mLeft)
			{
				valid = false;
			}
			else if (int_x > window_rect.mRight)
			{
				valid = false;
			}
			if (int_y < window_rect.mBottom)
			{
				valid = false;
			}
			else if (int_y > window_rect.mTop)
			{
				valid = false;
			}

			return in_front && valid;
		}
	}
	else
	{
		return false;
	}
}

// Uses the last GL matrices set in set_perspective to project a point from the
// agent's region space to the nearest edge in screen coordinates. Returns true
// if the projection succeeded.
bool LLViewerCamera::projectPosAgentToScreenEdge(const LLVector3& pos_agent,
												 LLCoordGL& out_point) const
{
	LLVector3 dir_to_point = pos_agent - getOrigin();
	dir_to_point /= dir_to_point.length();

	bool in_front = true;
	if (dir_to_point * getAtAxis() < 0.f)
	{
		in_front = false;
	}

	LLVector3 window_coordinates;
	if (gGL.projectf(pos_agent, gGLModelView, gGLProjection,
					 LLRect(gGLViewport[0], gGLViewport[1] + gGLViewport[3],
						    gGLViewport[0] + gGLViewport[2], gGLViewport[1]),
					 window_coordinates))
	{
		F32& x = window_coordinates.mV[VX];
		F32& y = window_coordinates.mV[VY];
		x /= gViewerWindowp->getDisplayScale().mV[VX];
		y /= gViewerWindowp->getDisplayScale().mV[VY];
		// should now have the x,y coords of grab_point in screen space
		const LLRect& window_rect = gViewerWindowp->getVirtualWindowRect();

		// ...sanity check
		S32 int_x = lltrunc(x);
		S32 int_y = lltrunc(y);

		// find the center
		GLdouble center_x = (GLdouble)(0.5f * (window_rect.mLeft + window_rect.mRight));
		GLdouble center_y = (GLdouble)(0.5f * (window_rect.mBottom + window_rect.mTop));

		if (x == center_x  &&  y == center_y)
		{
			// can't project to edge from exact center
			return false;
		}

		// find the line from center to local
		GLdouble line_x = x - center_x;
		GLdouble line_y = y - center_y;

		int_x = lltrunc(center_x);
		int_y = lltrunc(center_y);

		if (0.f == line_x)
		{
			// the slope of the line is undefined
			if (line_y > 0.f)
			{
				int_y = window_rect.mTop;
			}
			else
			{
				int_y = window_rect.mBottom;
			}
		}
		else if (0 == window_rect.getWidth())
		{
			// the diagonal slope of the view is undefined
			if (y < window_rect.mBottom)
			{
				int_y = window_rect.mBottom;
			}
			else if (y > window_rect.mTop)
			{
				int_y = window_rect.mTop;
			}
		}
		else
		{
			F32 line_slope = (F32)(line_y / line_x);
			F32 rect_slope = (F32)window_rect.getHeight() / (F32)window_rect.getWidth();

			if (fabs(line_slope) > rect_slope)
			{
				if (line_y < 0.f)
				{
					// bottom
					int_y = window_rect.mBottom;
				}
				else
				{
					// top
					int_y = window_rect.mTop;
				}
				int_x = lltrunc(((GLdouble)int_y - center_y) / line_slope + center_x);
			}
			else if (fabs(line_slope) < rect_slope)
			{
				if (line_x < 0.f)
				{
					// left
					int_x = window_rect.mLeft;
				}
				else
				{
					// right
					int_x = window_rect.mRight;
				}
				int_y = lltrunc(((GLdouble)int_x - center_x) * line_slope + center_y);
			}
			else
			{
				// exactly parallel ==> push to the corners
				if (line_x > 0.f)
				{
					int_x = window_rect.mRight;
				}
				else
				{
					int_x = window_rect.mLeft;
				}
				if (line_y > 0.0f)
				{
					int_y = window_rect.mTop;
				}
				else
				{
					int_y = window_rect.mBottom;
				}
			}
		}
		if (!in_front)
		{
			int_x = window_rect.mLeft + window_rect.mRight - int_x;
			int_y = window_rect.mBottom + window_rect.mTop - int_y;
		}

		out_point.mX = int_x;
		out_point.mY = int_y;
		return true;
	}

	return false;
}

void LLViewerCamera::getPixelVectors(const LLVector3& pos_agent, LLVector3& up,
									 LLVector3& right)
{
	LLVector3 to_vec = pos_agent - getOrigin();

	F32 at_dist = to_vec * getAtAxis();

	F32 height_meters = at_dist * tanf(getView() * 0.5f);
	F32 height_pixels = getViewHeightInPixels() * 0.5f;

	F32 pixel_aspect = gWindowp->getPixelAspectRatio();

	F32 meters_per_pixel = height_meters / height_pixels;
	up = getUpAxis() * meters_per_pixel * gViewerWindowp->getDisplayScale().mV[VY];
	right = -1.f * pixel_aspect * meters_per_pixel * getLeftAxis() *
			gViewerWindowp->getDisplayScale().mV[VX];
}

LLVector3 LLViewerCamera::roundToPixel(const LLVector3& pos_agent)
{
	F32 dist = (pos_agent - getOrigin()).length();
	// Convert to screen space and back, preserving the depth.
	LLCoordGL screen_point;
	if (!projectPosAgentToScreen(pos_agent, screen_point, false))
	{
		// Off the screen, just return the original position.
		return pos_agent;
	}

	LLVector3 ray_dir;

	projectScreenToPosAgent(screen_point.mX, screen_point.mY, &ray_dir);
	ray_dir -= getOrigin();
	ray_dir.normalize();

	LLVector3 pos_agent_rounded = getOrigin() + ray_dir*dist;

#if 0
	LLVector3 pixel_x, pixel_y;
	getPixelVectors(pos_agent_rounded, pixel_y, pixel_x);
	pos_agent_rounded += 0.5f*pixel_x, 0.5f*pixel_y;
#endif

	return pos_agent_rounded;
}

bool LLViewerCamera::cameraUnderWater() const
{
	LLViewerRegion* regionp = gWorld.getRegionFromPosAgent(getOrigin());
	if (!regionp)
	{
		regionp = gAgent.getRegion();
	}
	return regionp && getOrigin().mV[VZ] < regionp->getWaterHeight();
}

bool LLViewerCamera::areVertsVisible(LLViewerObject* volumep, bool all_verts)
{
	LLDrawable* drawablep = volumep->mDrawable;
	if (!drawablep)
	{
		return false;
	}

	LLVolume* volume = volumep->getVolume();
	if (!volume)
	{
		return false;
	}

	LLVOVolume* vo_volume = (LLVOVolume*)volumep;

	vo_volume->updateRelativeXform();

	LLMatrix4 render_mat(vo_volume->getRenderRotation(),
						 LLVector4(vo_volume->getRenderPosition()));
	LLMatrix4a render_mata;
	render_mata.loadu(render_mat);

	LLMatrix4a mata;
	mata.loadu(vo_volume->getRelativeXform());

	S32 num_faces = volume->getNumVolumeFaces();
	for (S32 i = 0; i < num_faces; ++i)
	{
		const LLVolumeFace& face = volume->getVolumeFace(i);
		for (U32 v = 0, count = face.mNumVertices; v < count; ++v)
		{
			const LLVector4a& src_vec = face.mPositions[v];
			LLVector4a vec;
			mata.affineTransform(src_vec, vec);

			if (drawablep->isActive())
			{
				LLVector4a t = vec;
				render_mata.affineTransform(t, vec);
			}

			bool in_frustum = pointInFrustum(LLVector3(vec.getF32ptr())) > 0;
			if ((!in_frustum && all_verts) || (in_frustum && !all_verts))
			{
				return !all_verts;
			}
		}
	}

	return all_verts;
}

// Changes local camera and broadcasts change
//virtual
void LLViewerCamera::setView(F32 vertical_fov_rads)
{
	if (gCubeSnapshot)	// Should not happen
	{
		llassert(false);
		return;
	}

	F32 old_fov = getView();

	// Cap the FoV
	vertical_fov_rads = llclamp(vertical_fov_rads, getMinView(), getMaxView());

	if (vertical_fov_rads == old_fov) return;

	// Send the new value to the simulator
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AgentFOV);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addU32Fast(_PREHASH_CircuitCode, msg->mOurCircuitCode);

	msg->nextBlockFast(_PREHASH_FOVBlock);
	msg->addU32Fast(_PREHASH_GenCounter, 0);
	msg->addF32Fast(_PREHASH_VerticalAngle, vertical_fov_rads);

	gAgent.sendReliableMessage();

	// Sync the camera with the new value
	LLCamera::setView(vertical_fov_rads); // Call base implementation
}

void LLViewerCamera::setDefaultFOV(F32 vertical_fov_rads)
{
	vertical_fov_rads = llclamp(vertical_fov_rads, getMinView(), getMaxView());
//MK
	if (gRLenabled)
	{
		if (gRLInterface.mCamZoomMax < EXTREMUM &&
			DEFAULT_FIELD_OF_VIEW / vertical_fov_rads > gRLInterface.mCamZoomMax)
		{
			vertical_fov_rads = DEFAULT_FIELD_OF_VIEW / gRLInterface.mCamZoomMax;
		}
		if (gRLInterface.mCamZoomMin > -EXTREMUM &&
			DEFAULT_FIELD_OF_VIEW / vertical_fov_rads < gRLInterface.mCamZoomMin)
		{
			vertical_fov_rads = DEFAULT_FIELD_OF_VIEW / gRLInterface.mCamZoomMin;
		}
	}
//mk
	setView(vertical_fov_rads);
	mCameraFOVDefault = vertical_fov_rads;
	mCosHalfCameraFOV = cosf(mCameraFOVDefault * 0.5f);
}

bool LLViewerCamera::isDefaultFOVChanged()
{
	static LLCachedControl<bool> ignore_fov_zoom(gSavedSettings,
												 "IgnoreFOVZoomForLODs");
	if (mCameraFOVDefault != mPrevCameraFOVDefault)
	{
		mPrevCameraFOVDefault = mCameraFOVDefault;
		return !ignore_fov_zoom;
	}
	return false;
}

// static
void LLViewerCamera::updateCameraAngle(void* user_data, const LLSD& value)
{
	LLViewerCamera* self = (LLViewerCamera*)user_data;
	if (self)
	{
		self->setDefaultFOV(value.asReal());
	}
}
