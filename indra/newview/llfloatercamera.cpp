/**
 * @file llfloatercamera.cpp
 * @brief Container for camera control buttons (zoom, pan, orbit)
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

#include "llfloatercamera.h"

#include "llcheckboxctrl.h"
#include "llspinctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "lljoystickbutton.h"
#include "llviewercontrol.h"

// Constants
constexpr F32 CAMERA_BUTTON_DELAY = 0.0f;

//
// Member functions
//

LLFloaterCamera::LLFloaterCamera(const LLSD& val)
:	LLFloater("camera controls") // Uses "FloaterCameraRect3a"
{
	setIsChrome(true);

	// For now, only used for size and tooltip strings
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_camera.xml",
												 NULL, false); // Do not open
}

//virtual
bool LLFloaterCamera::postBuild()
{
	S32 top = getRect().getHeight();
	S32 bottom = 0;
	S32 left = 4;

	constexpr S32 ROTATE_WIDTH = 64;
	mRotate = new LLJoystickCameraRotate("cam rotate stick",
										 LLRect(left, top,
												left + ROTATE_WIDTH, bottom),
										 "cam_rotate_out.tga",
										 "cam_rotate_in.tga");
	mRotate->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
	mRotate->setHeldDownDelay(CAMERA_BUTTON_DELAY);
	mRotate->setToolTip(getString("rotate_tooltip"));
	mRotate->setSoundFlags(MOUSE_DOWN | MOUSE_UP);
	addChild(mRotate);

	left += ROTATE_WIDTH;

	constexpr S32 ZOOM_WIDTH = 16;
	mZoom = new LLJoystickCameraZoom("zoom",
									 LLRect(left, top,
											left + ZOOM_WIDTH, bottom),
									 "cam_zoom_out.tga",
									 "cam_zoom_plus_in.tga",
									 "cam_zoom_minus_in.tga");
	mZoom->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
	mZoom->setHeldDownDelay(CAMERA_BUTTON_DELAY);
	mZoom->setToolTip(getString("zoom_tooltip"));
	mZoom->setSoundFlags(MOUSE_DOWN | MOUSE_UP);
	addChild(mZoom);

	left += ZOOM_WIDTH;

	constexpr S32 TRACK_WIDTH = 64;
	mTrack = new LLJoystickCameraTrack("cam track stick",
									   LLRect(left, top, left + TRACK_WIDTH, bottom),
									   "cam_tracking_out.tga",
									   "cam_tracking_in.tga");
	mTrack->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
	mTrack->setHeldDownDelay(CAMERA_BUTTON_DELAY);
	mTrack->setToolTip(getString("move_tooltip"));
	mTrack->setSoundFlags(MOUSE_DOWN | MOUSE_UP);
	addChild(mTrack);

	mFrontViewCheck = getChild<LLCheckBoxCtrl>("front_view");

	return true;
}

//virtual
void LLFloaterCamera::onOpen()
{
	LLFloater::onOpen();
	gSavedSettings.setBool("ShowCameraControls", true);
}

//virtual
void LLFloaterCamera::onClose(bool app_quitting)
{
	LLFloater::onClose(app_quitting);
	if (!app_quitting)
	{
		gSavedSettings.setBool("ShowCameraControls", false);
	}
}

//virtual
void LLFloaterCamera::draw()
{
	static ECameraMode mode = CAMERA_MODE_THIRD_PERSON;
	ECameraMode current_mode = gAgent.getCameraMode();
	if (current_mode != mode)
	{
		mode = current_mode;
		mFrontViewCheck->setEnabled(mode != CAMERA_MODE_MOUSELOOK &&
									mode != CAMERA_MODE_CUSTOMIZE_AVATAR);
	}

	LLFloater::draw();
}
