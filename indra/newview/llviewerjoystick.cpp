/**
 * @file llviewerjoystick.cpp
 * @brief Joystick / NDOF device functionality.
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

#include "llviewerjoystick.h"

#include "llfocusmgr.h"
#include "llkeyboard.h"
#include "llsys.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFrameIntervalSeconds
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolmgr.h"
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerwindow.h"			// For gAwayTimer

// Constants

#define  X_I	1
#define  Y_I	2
#define  Z_I	0
#define RX_I	4
#define RY_I	5
#define RZ_I	3

// Flycam translations in build mode should be reduced
constexpr F32 BUILDMODE_FLYCAM_T_SCALE = 3.f;

// Minimum time after setting away state before coming back
constexpr F32 MIN_AFK_TIME = 2.f;

F32  LLViewerJoystick::sLastDelta[] = { 0, 0, 0, 0, 0, 0, 0 };
F32  LLViewerJoystick::sDelta[] = { 0, 0, 0, 0, 0, 0, 0 };

// These constants specify the maximum absolute value coming in from the
// device.
// *HACK: the value of MAX_JOYSTICK_INPUT_VALUE is not arbitrary as it should
// be. It has to be equal to 3000 because the SpaceNavigator on Windows refuses
// to respond to the DirectInput SetProperty call; it always returns values in
// the [-3000, 3000] range.
#define MAX_SPACENAVIGATOR_INPUT 3000.0f
#define MAX_JOYSTICK_INPUT_VALUE MAX_SPACENAVIGATOR_INPUT

void LLViewerJoystick::updateEnabled(bool autoenable)
{
	if (mDriverState == JDS_UNINITIALIZED)
	{
		gSavedSettings.setBool("JoystickEnabled", false);
	}
	else if (isLikeSpaceNavigator() && autoenable)
	{
		gSavedSettings.setBool("JoystickEnabled", true);
	}
	if (!mJoystickEnabled)
	{
		mOverrideCamera = false;
	}
}

void LLViewerJoystick::setOverrideCamera(bool val)
{
	if (!mJoystickEnabled)
	{
		mOverrideCamera = false;
	}
	else
	{
		mOverrideCamera = val;
	}

	if (mOverrideCamera)
	{
		gAgent.changeCameraToDefault();
	}
}

//static
NDOF_HotPlugResult LLViewerJoystick::hotPlugAddCallback(NDOF_Device* dev)
{
	NDOF_HotPlugResult res = NDOF_DISCARD_HOTPLUGGED;
	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	if (joystick->mDriverState == JDS_UNINITIALIZED)
	{
        llinfos << "Will use device: " << llendl;
		ndof_dump(stderr, dev);
		joystick->mNdofDev = dev;
        joystick->mDriverState = JDS_INITIALIZED;
        res = NDOF_KEEP_HOTPLUGGED;
	}
	joystick->updateEnabled(true);
    return res;
}

//static
void LLViewerJoystick::hotPlugRemovalCallback(NDOF_Device* dev)
{
	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	if (joystick->mNdofDev == dev)
	{
        llinfos << "joystick->mNdofDev=" << joystick->mNdofDev
				<< "; removed device:" << llendl;
		ndof_dump(stderr, dev);
		joystick->mDriverState = JDS_UNINITIALIZED;
	}
	joystick->updateEnabled(true);
}

LLViewerJoystick::LLViewerJoystick()
:	mDriverState(JDS_UNINITIALIZED),
	mNdofDev(NULL),
	mResetFlag(false),
	mCameraUpdated(true),
	mOverrideCamera(false),
	mJoystickRun(0),
	mJoystickEnabled(LLCachedControl<bool>(gSavedSettings, "JoystickEnabled")),
	mJoystickAvatarEnabled(LLCachedControl<bool>(gSavedSettings,
												 "JoystickAvatarEnabled")),
	mJoystickFlycamEnabled(LLCachedControl<bool>(gSavedSettings,
												 "JoystickFlycamEnabled")),
	mJoystickBuildEnabled(LLCachedControl<bool>(gSavedSettings,
												"JoystickBuildEnabled")),
	mCursor3D(LLCachedControl<bool>(gSavedSettings, "Cursor3D")),
	mJoystickAxis0(LLCachedControl<S32>(gSavedSettings, "JoystickAxis0")),
	mJoystickAxis1(LLCachedControl<S32>(gSavedSettings, "JoystickAxis1")),
	mJoystickAxis2(LLCachedControl<S32>(gSavedSettings, "JoystickAxis2")),
	mJoystickAxis3(LLCachedControl<S32>(gSavedSettings, "JoystickAxis3")),
	mJoystickAxis4(LLCachedControl<S32>(gSavedSettings, "JoystickAxis4")),
	mJoystickAxis5(LLCachedControl<S32>(gSavedSettings, "JoystickAxis5")),
	mJoystickAxis6(LLCachedControl<S32>(gSavedSettings, "JoystickAxis6"))
{
	for (U32 i = 0; i < 6; ++i)
	{
		mAxes[i] = sDelta[i] = sLastDelta[i] = 0.f;
	}

	for (U32 i = 0; i < 16; ++i)
	{
		mBtn[i] = false;
	}

	// Factor in bandwidth ?  bandwidth = gViewerStats->mKBitStat
	mPerfScale = 4000.f / LLCPUInfo::getInstance()->getMHz();
}

LLViewerJoystick::~LLViewerJoystick()
{
	if (mDriverState == JDS_INITIALIZED)
	{
		terminate();
	}
}

void LLViewerJoystick::init(bool autoenable)
{
	if (gSavedSettings.getBool("JoystickNeverEnable"))
	{
		return;
	}
	static bool libinit = false;
	mDriverState = JDS_INITIALIZING;

	if (!libinit)
	{
		// Note: the HotPlug callbacks are not actually getting called on
		// Windows
		if (ndof_libinit(hotPlugAddCallback,
						 hotPlugRemovalCallback,
						 NULL))
		{
			mDriverState = JDS_UNINITIALIZED;
		}
		else
		{
			// NB: ndof_libinit succeeds when there is no device
			libinit = true;

			// Allocate memory once for an eventual device
			mNdofDev = ndof_create();
		}
	}

	if (libinit)
	{
		if (mNdofDev)
		{
			// Different joysticks will return different ranges of raw values.
			// Since we want to handle every device in the same uniform way,
			// we initialize the mNdofDev struct and we set the range
			// of values we would like to receive.
			//
			// *HACK: on Windows, libndofdev passes our range to DI with a
			// SetProperty call. This works but with one notable exception, the
			// SpaceNavigator which does not seem to care about the SetProperty
			// call. In theory, we should handle this case inside libndofdev.
			// However, the range we are setting here is arbitrary anyway, so
			// let's just use the SpaceNavigator range for our purposes.
			mNdofDev->axes_min = (long)-MAX_JOYSTICK_INPUT_VALUE;
			mNdofDev->axes_max = (long)+MAX_JOYSTICK_INPUT_VALUE;

			// libndofdev could be used to return deltas. Here we choose to
			// just have the absolute values instead.
			mNdofDev->absolute = 1;

			// Init & use the first suitable NDOF device found on the USB chain
			if (ndof_init_first(mNdofDev, NULL))
			{
				mDriverState = JDS_UNINITIALIZED;
				if (mJoystickEnabled)
				{
					llwarns << "No NDOF device found. Joystick control unavailable."
							<< llendl;
				}
			}
			else
			{
				mDriverState = JDS_INITIALIZED;
			}
		}
		else
		{
			mDriverState = JDS_UNINITIALIZED;
		}
	}

	// Autoenable the joystick for recognized devices if nothing was connected
	// previously
	if (!autoenable)
	{
		autoenable = gSavedSettings.getString("JoystickInitialized").empty();
	}
	updateEnabled(autoenable);

	if (mDriverState == JDS_INITIALIZED)
	{
		// A Joystick device is plugged in
		if (isLikeSpaceNavigator())
		{
			// It is a space navigator, we have defaults for it.
			if (gSavedSettings.getString("JoystickInitialized") !=
					"SpaceNavigator")
			{
				// Only set the defaults if we have not already (in case they
				// were overridden)
				setSNDefaults();
				gSavedSettings.setString("JoystickInitialized",
										 "SpaceNavigator");
			}
		}
		else
		{
			std::string device = getDescription();
			if (device.empty())
			{
				device = "UnknownDevice";
			}
			// It is not a Space Navigator
			gSavedSettings.setString("JoystickInitialized", device);
		}
	}

	llinfos << "ndof: mDriverState=" << mDriverState << "; mNdofDev="
			<< mNdofDev << "; libinit=" << libinit << llendl;
}

void LLViewerJoystick::terminate()
{
	if (mNdofDev)
	{
		mNdofDev = NULL;
		llinfos << "Terminating connection with NDOF device..." << llendl;
		ndof_libcleanup();
		mDriverState = JDS_UNINITIALIZED;
		llinfos << "NDOF device freed." << llendl;
	}
}

void LLViewerJoystick::updateStatus()
{
	if (!mNdofDev)
	{
		return;
	}

	ndof_update(mNdofDev);

	for (U32 i = 0; i < 6; ++i)
	{
		mAxes[i] = (F32)mNdofDev->axes[i] / mNdofDev->axes_max;
	}

	S32 new_state = 0;	// Bitmap of the currently pressed buttons
	for (S32 i = 15; i >= 0; --i)
	{
		bool active = mBtn[i] = mNdofDev->buttons[i] != 0;
		new_state <<= 1;
		if (active)
		{
			++new_state;
		}
	}

	static S32 old_state = 0;	// Bitmap of the formerly pressed buttons
	if (new_state != old_state && gAutomationp)
	{
		gAutomationp->onJoystickButtons(old_state, new_state);
		old_state = new_state;
	}
}

F32 LLViewerJoystick::getJoystickAxis(S32 axis) const
{
	if (axis >= 0 && axis < 6)
	{
		return mAxes[axis];
	}
	return 0.f;
}

bool LLViewerJoystick::getJoystickButton(S32 button) const
{
	if (button >= 0 && button < 16)
	{
		return mBtn[button];
	}
	return false;
}

void LLViewerJoystick::handleRun(F32 inc)
{
//MK
	if (gRLenabled && gRLInterface.mContainsRun)
	{
		mJoystickRun = 0;
		if (gAgent.getRunning())
		{
			gAgent.clearRunning();
			gAgent.sendWalkRun(false);
		}
		return;
	}
//mk
	// Decide whether to walk or run by applying a threshold, with slight
	// hysteresis to avoid oscillating between the two with input spikes.
	// Analog speed control would be better, but not likely any time soon.
	static LLCachedControl<F32> run_threshold(gSavedSettings,
											  "JoystickRunThreshold");
	if (inc > run_threshold)
	{
		if (mJoystickRun == 1)
		{
			++mJoystickRun;
			gAgent.setRunning();
			gAgent.sendWalkRun(true);
		}
		else if (mJoystickRun == 0)
		{
			// Hysteresis: respond NEXT frame
			++mJoystickRun;
		}
	}
	else if (mJoystickRun > 0)
	{
		if (--mJoystickRun == 0)
		{
			gAgent.clearRunning();
			gAgent.sendWalkRun(false);
		}
	}
}

void LLViewerJoystick::agentJump()
{
    gAgent.moveUp(1);
}

void LLViewerJoystick::agentSlide(F32 inc)
{
	if (inc < 0.f)
	{
		gAgent.moveLeft(1);
	}
	else if (inc > 0.f)
	{
		gAgent.moveLeft(-1);
	}
}

void LLViewerJoystick::agentPush(F32 inc)
{
	if (inc < 0.f)                            // forward
	{
		gAgent.moveAt(1, false);
	}
	else if (inc > 0.f)                       // backward
	{
		gAgent.moveAt(-1, false);
	}
}

void LLViewerJoystick::agentFly(F32 inc)
{
	static LLCachedControl<bool> automatic_fly(gSavedSettings, "AutomaticFly");
	if (inc < 0.f)
	{
		if (automatic_fly && !gAgent.getFlying() && gAgent.canFly() &&
			!gAgent.upGrabbed())
		{
			gAgent.setFlying(true);
		}
		gAgent.moveUp(1);
	}
	else if (inc > 0.f)
	{
		// Crouch
		gAgent.moveUp(-1);
	}
}

void LLViewerJoystick::agentRotate(F32 pitch_inc, F32 yaw_inc)
{
	LLQuaternion new_rot;
	pitch_inc = gAgent.clampPitchToLimits(-pitch_inc);
	const LLQuaternion qx(pitch_inc, gAgent.getLeftAxis());
	const LLQuaternion qy(-yaw_inc, gAgent.getReferenceUpVector());
	new_rot.set(qx * qy);
	gAgent.rotate(new_rot);
}

void LLViewerJoystick::resetDeltas(S32 axis[])
{
	for (U32 i = 0; i < 6; ++i)
	{
		sLastDelta[i] = -mAxes[axis[i]];
		sDelta[i] = 0.f;
	}

	sLastDelta[6] = sDelta[6] = 0.f;
	mResetFlag = false;
}

void LLViewerJoystick::moveObjects(bool reset)
{
	static bool toggle_send_to_sim = false;

	if (mDriverState != JDS_INITIALIZED || !gFocusMgr.getAppHasFocus() ||
		!mJoystickEnabled || !mJoystickBuildEnabled)
	{
		return;
	}

	S32 axis[] =
	{
		(S32)mJoystickAxis0,
		(S32)mJoystickAxis1,
		(S32)mJoystickAxis2,
		(S32)mJoystickAxis3,
		(S32)mJoystickAxis4,
		(S32)mJoystickAxis5
	};

	if (reset || mResetFlag)
	{
		resetDeltas(axis);
		return;
	}

	static LLCachedControl<F32> axis_scale0(gSavedSettings,
											 "BuildAxisScale0");
	static LLCachedControl<F32> axis_scale1(gSavedSettings,
											 "BuildAxisScale1");
	static LLCachedControl<F32> axis_scale2(gSavedSettings,
											 "BuildAxisScale2");
	static LLCachedControl<F32> axis_scale3(gSavedSettings,
											 "BuildAxisScale3");
	static LLCachedControl<F32> axis_scale4(gSavedSettings,
											 "BuildAxisScale4");
	static LLCachedControl<F32> axis_scale5(gSavedSettings,
											 "BuildAxisScale5");
	F32 axis_scale[] =
	{
		(F32)axis_scale0,
		(F32)axis_scale1,
		(F32)axis_scale2,
		(F32)axis_scale3,
		(F32)axis_scale4,
		(F32)axis_scale5
	};

	static LLCachedControl<F32> dead_zone0(gSavedSettings,
											"BuildAxisDeadZone0");
	static LLCachedControl<F32> dead_zone1(gSavedSettings,
											"BuildAxisDeadZone1");
	static LLCachedControl<F32> dead_zone2(gSavedSettings,
											"BuildAxisDeadZone2");
	static LLCachedControl<F32> dead_zone3(gSavedSettings,
											"BuildAxisDeadZone3");
	static LLCachedControl<F32> dead_zone4(gSavedSettings,
											"BuildAxisDeadZone4");
	static LLCachedControl<F32> dead_zone5(gSavedSettings,
											"BuildAxisDeadZone5");
	F32 dead_zone[] =
	{
		(F32)dead_zone0,
		(F32)dead_zone1,
		(F32)dead_zone2,
		(F32)dead_zone3,
		(F32)dead_zone4,
		(F32)dead_zone5
	};

	F32 cur_delta[6];
	F32 time = gFrameIntervalSeconds;

	// Avoid making ridicously big movements if there is a big drop in fps
	if (time > .2f)
	{
		time = .2f;
	}

	// Max feather is 32
	static LLCachedControl<F32> feather(gSavedSettings, "BuildFeathering");
	bool absolute = mCursor3D;
	bool is_zero = true;

	for (U32 i = 0; i < 6; ++i)
	{
		cur_delta[i] = -mAxes[axis[i]];
		F32 tmp = cur_delta[i];
		if (absolute)
		{
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
		}
		sLastDelta[i] = tmp;
		is_zero = is_zero && (cur_delta[i] == 0.f);

		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i] - dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i] + dead_zone[i], 0.f);
		}
		cur_delta[i] *= axis_scale[i];

		if (!absolute)
		{
			cur_delta[i] *= time;
		}

		sDelta[i] = sDelta[i] + (cur_delta[i] - sDelta[i]) * time * feather;
	}

	U32 upd_type = UPD_NONE;
	LLVector3 v;

	if (!is_zero)
	{
		// Clear AFK state if moved beyond the deadzone
		if (gAwayTimer.getElapsedTimeF32() > MIN_AFK_TIME)
		{
			gAgent.clearAFK();
		}

		if (sDelta[0] || sDelta[1] || sDelta[2])
		{
			upd_type |= UPD_POSITION;
			v.set(sDelta[0], sDelta[1], sDelta[2]);
		}

		if (sDelta[3] || sDelta[4] || sDelta[5])
		{
			upd_type |= UPD_ROTATION;
		}

		// The selection update could fail, so we would not send
		if (gSelectMgr.selectionMove(v, sDelta[3],sDelta[4],sDelta[5], upd_type))
		{
			toggle_send_to_sim = true;
		}
	}
	else if (toggle_send_to_sim)
	{
		gSelectMgr.sendSelectionMove();
		toggle_send_to_sim = false;
	}
}

void LLViewerJoystick::moveAvatar(bool reset)
{
	if (mDriverState != JDS_INITIALIZED || !gFocusMgr.getAppHasFocus() ||
		!mJoystickEnabled || !mJoystickAvatarEnabled)
	{
		return;
	}

	S32 axis[] =
	{
		// [1 0 2 4  3  5]
		// [Z X Y RZ RX RY]
		(S32)mJoystickAxis0,
		(S32)mJoystickAxis1,
		(S32)mJoystickAxis2,
		(S32)mJoystickAxis3,
		(S32)mJoystickAxis4,
		(S32)mJoystickAxis5
	};

	if (reset || mResetFlag)
	{
		resetDeltas(axis);
		if (reset)
		{
			// Note: moving the agent triggers agent camera mode; do not do
			// this every time we set mResetFlag (e.g. because we gained focus)
			gAgent.moveAt(0, true);
		}
		return;
	}

	bool is_zero = true;

	static LLCachedControl<S32> jump_button(gSavedSettings,
											"JoystickButtonJump");
	if (getJoystickButton(jump_button))
	{
		agentJump();
		is_zero = false;
	}

	static LLCachedControl<F32> axis_scale0(gSavedSettings,
											"AvatarAxisScale0");
	static LLCachedControl<F32> axis_scale1(gSavedSettings,
											"AvatarAxisScale1");
	static LLCachedControl<F32> axis_scale2(gSavedSettings,
											"AvatarAxisScale2");
	static LLCachedControl<F32> axis_scale3(gSavedSettings,
											"AvatarAxisScale3");
	static LLCachedControl<F32> axis_scale4(gSavedSettings,
											"AvatarAxisScale4");
	static LLCachedControl<F32> axis_scale5(gSavedSettings,
											"AvatarAxisScale5");
	F32 axis_scale[] =
	{
		(F32)axis_scale0,
		(F32)axis_scale1,
		(F32)axis_scale2,
		(F32)axis_scale3,
		(F32)axis_scale4,
		(F32)axis_scale5
	};

	static LLCachedControl<F32> dead_zone0(gSavedSettings,
										   "AvatarAxisDeadZone0");
	static LLCachedControl<F32> dead_zone1(gSavedSettings,
										   "AvatarAxisDeadZone1");
	static LLCachedControl<F32> dead_zone2(gSavedSettings,
										   "AvatarAxisDeadZone2");
	static LLCachedControl<F32> dead_zone3(gSavedSettings,
										   "AvatarAxisDeadZone3");
	static LLCachedControl<F32> dead_zone4(gSavedSettings,
										   "AvatarAxisDeadZone4");
	static LLCachedControl<F32> dead_zone5(gSavedSettings,
										   "AvatarAxisDeadZone5");
	F32 dead_zone[] =
	{
		(F32)dead_zone0,
		(F32)dead_zone1,
		(F32)dead_zone2,
		(F32)dead_zone3,
		(F32)dead_zone4,
		(F32)dead_zone5
	};

	// Time interval in seconds between this frame and the previous
	F32 time = gFrameIntervalSeconds;

	// Avoid making ridicously big movements if there is a big drop in fps
	if (time > .2f)
	{
		time = .2f;
	}

	// Note: max feather is 32.0
	static LLCachedControl<F32> feather(gSavedSettings, "AvatarFeathering");

	F32 cur_delta[6];
	F32 val, dom_mov = 0.f;
	U32 dom_axis = Z_I;
    bool absolute = mCursor3D && mNdofDev->absolute;
	// Remove dead zones and determine biggest movement on the joystick
	for (U32 i = 0; i < 6; ++i)
	{
		cur_delta[i] = -mAxes[axis[i]];
		if (absolute)
		{
			F32 tmp = cur_delta[i];
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
			sLastDelta[i] = tmp;
		}

		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
		}

		// We do not care about Roll (RZ) and Z is calculated after the loop
        if (i != Z_I && i != RZ_I)
		{
			// find out the axis with the biggest joystick motion
			val = fabs(cur_delta[i]);
			if (val > dom_mov)
			{
				dom_axis = i;
				dom_mov = val;
			}
		}

		is_zero = is_zero && (cur_delta[i] == 0.f);
	}

	if (!is_zero)
	{
		// Clear AFK state if moved beyond the deadzone
		if (gAwayTimer.getElapsedTimeF32() > MIN_AFK_TIME)
		{
			gAgent.clearAFK();
		}

		setCameraNeedsUpdate(true);
	}

	// Forward|backward movements overrule the real dominant movement if they
	// are bigger than its 20%. This is what you want 'cos moving forward is
	// what you do most. We also added a special (even more lenient) case for
	// RX|RY to allow walking while pitching and turning
	if (fabs(cur_delta[Z_I]) > .2f * dom_mov ||
		((dom_axis == RX_I || dom_axis == RY_I) &&
		 fabs(cur_delta[Z_I]) > .05f * dom_mov))
	{
		dom_axis = Z_I;
	}

	sDelta[X_I] = -cur_delta[X_I] * axis_scale[X_I];
	sDelta[Y_I] = -cur_delta[Y_I] * axis_scale[Y_I];
	sDelta[Z_I] = -cur_delta[Z_I] * axis_scale[Z_I];
	cur_delta[RX_I] *= -axis_scale[RX_I] * mPerfScale;
	cur_delta[RY_I] *= -axis_scale[RY_I] * mPerfScale;

	if (!absolute)
	{
		cur_delta[RX_I] *= time;
		cur_delta[RY_I] *= time;
	}
	sDelta[RX_I] += (cur_delta[RX_I] - sDelta[RX_I]) * time * feather;
	sDelta[RY_I] += (cur_delta[RY_I] - sDelta[RY_I]) * time * feather;

	handleRun(sqrtf(sDelta[Z_I] * sDelta[Z_I] + sDelta[X_I] * sDelta[X_I]));

	// Allow forward/backward movement some priority
	if (dom_axis == Z_I)
	{
		agentPush(sDelta[Z_I]);			// forward/back

		if (fabs(sDelta[X_I])  > .1f)
		{
			agentSlide(sDelta[X_I]);	// move sideways
		}

		if (fabs(sDelta[Y_I])  > .1f)
		{
			agentFly(sDelta[Y_I]);		// up/down & crouch
		}

		// too many rotations during walking can be confusing, so apply
		// the deadzones one more time (quick & dirty), at 50%|30% power
		F32 eff_rx = .3f * dead_zone[RX_I];
		F32 eff_ry = .3f * dead_zone[RY_I];

		if (sDelta[RX_I] > 0)
		{
			eff_rx = llmax(sDelta[RX_I] - eff_rx, 0.f);
		}
		else
		{
			eff_rx = llmin(sDelta[RX_I] + eff_rx, 0.f);
		}

		if (sDelta[RY_I] > 0)
		{
			eff_ry = llmax(sDelta[RY_I] - eff_ry, 0.f);
		}
		else
		{
			eff_ry = llmin(sDelta[RY_I] + eff_ry, 0.f);
		}

		if (fabs(eff_rx) > 0.f || fabs(eff_ry) > 0.f)
		{
			if (gAgent.getFlying())
			{
				agentRotate(eff_rx, eff_ry);
			}
			else
			{
				agentRotate(eff_rx, 2.f * eff_ry);
			}
		}
	}
	else
	{
		agentSlide(sDelta[X_I]);		// move sideways
		agentFly(sDelta[Y_I]);			// up/down & crouch
		agentPush(sDelta[Z_I]);			// forward/back
		agentRotate(sDelta[RX_I], sDelta[RY_I]);	// pitch & turn
	}
}

void LLViewerJoystick::moveFlycam(bool reset)
{
	static LLQuaternion sFlycamRotation;
	static LLVector3 sFlycamPosition;
	static F32 sFlycamZoom;

	if (mDriverState != JDS_INITIALIZED || !gFocusMgr.getAppHasFocus() ||
		!mJoystickEnabled || !mJoystickFlycamEnabled)
	{
		return;
	}

	S32 axis[] =
	{
		(S32)mJoystickAxis0,
		(S32)mJoystickAxis1,
		(S32)mJoystickAxis2,
		(S32)mJoystickAxis3,
		(S32)mJoystickAxis4,
		(S32)mJoystickAxis5,
		(S32)mJoystickAxis6
	};

	if (reset || mResetFlag)
	{
		sFlycamPosition = gViewerCamera.getOrigin();
		sFlycamRotation = gViewerCamera.getQuaternion();
		sFlycamZoom = gViewerCamera.getView();

		resetDeltas(axis);

		return;
	}

	static LLCachedControl<F32> axis_scale0(gSavedSettings,
											"FlycamAxisScale0");
	static LLCachedControl<F32> axis_scale1(gSavedSettings,
											"FlycamAxisScale1");
	static LLCachedControl<F32> axis_scale2(gSavedSettings,
											"FlycamAxisScale2");
	static LLCachedControl<F32> axis_scale3(gSavedSettings,
											"FlycamAxisScale3");
	static LLCachedControl<F32> axis_scale4(gSavedSettings,
											"FlycamAxisScale4");
	static LLCachedControl<F32> axis_scale5(gSavedSettings,
											"FlycamAxisScale5");
	static LLCachedControl<F32> axis_scale6(gSavedSettings,
											"FlycamAxisScale6");
	F32 axis_scale[] =
	{
		(F32)axis_scale0,
		(F32)axis_scale1,
		(F32)axis_scale2,
		(F32)axis_scale3,
		(F32)axis_scale4,
		(F32)axis_scale5,
		(F32)axis_scale6
	};

	static LLCachedControl<F32> dead_zone0(gSavedSettings,
										   "FlycamAxisDeadZone0");
	static LLCachedControl<F32> dead_zone1(gSavedSettings,
										   "FlycamAxisDeadZone1");
	static LLCachedControl<F32> dead_zone2(gSavedSettings,
										   "FlycamAxisDeadZone2");
	static LLCachedControl<F32> dead_zone3(gSavedSettings,
										   "FlycamAxisDeadZone3");
	static LLCachedControl<F32> dead_zone4(gSavedSettings,
										   "FlycamAxisDeadZone4");
	static LLCachedControl<F32> dead_zone5(gSavedSettings,
										   "FlycamAxisDeadZone5");
	static LLCachedControl<F32> dead_zone6(gSavedSettings,
										   "FlycamAxisDeadZone6");
	F32 dead_zone[] =
	{
		(F32)dead_zone0,
		(F32)dead_zone1,
		(F32)dead_zone2,
		(F32)dead_zone3,
		(F32)dead_zone4,
		(F32)dead_zone5,
		(F32)dead_zone6
	};

	F32 time = gFrameIntervalSeconds;

	// Avoid making ridiculously big movements if there is a big drop in fps
	if (time > .2f)
	{
		time = .2f;
	}

	F32 cur_delta[7];
	static LLCachedControl<F32> feather(gSavedSettings, "FlycamFeathering");
	bool absolute = mCursor3D;
	bool is_zero = true;

	for (U32 i = 0; i < 7; ++i)
	{
		cur_delta[i] = -getJoystickAxis(axis[i]);


		F32 tmp = cur_delta[i];
		if (absolute)
		{
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
		}
		sLastDelta[i] = tmp;

		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
		}

		// We need smaller camera movements in build mode.
		// NOTE: this needs to remain after the deadzone calculation, otherwise
		// we have issues with flycam "jumping" when the build dialog is
		// opened/closed. -Nyx
		if (gToolMgr.inBuildMode())
		{
			if (i == X_I || i == Y_I || i == Z_I)
			{
				cur_delta[i] /= BUILDMODE_FLYCAM_T_SCALE;
			}
		}

		cur_delta[i] *= axis_scale[i];

		if (!absolute)
		{
			cur_delta[i] *= time;
		}

		sDelta[i] = sDelta[i] + (cur_delta[i] - sDelta[i]) * time * feather;

		is_zero = is_zero && (cur_delta[i] == 0.f);

	}

	// Clear AFK state if moved beyond the deadzone
	if (!is_zero && gAwayTimer.getElapsedTimeF32() > MIN_AFK_TIME)
	{
		gAgent.clearAFK();
	}

	sFlycamPosition += LLVector3(sDelta) * sFlycamRotation;

	LLMatrix3 rot_mat(sDelta[3], sDelta[4], sDelta[5]);
	sFlycamRotation = LLQuaternion(rot_mat) * sFlycamRotation;

	static LLCachedControl<bool> auto_leveling(gSavedSettings, "AutoLeveling");
	if (auto_leveling)
	{
		LLMatrix3 level(sFlycamRotation);

		LLVector3 x = LLVector3(level.mMatrix[0]);
		LLVector3 y = LLVector3(level.mMatrix[1]);
		LLVector3 z = LLVector3(level.mMatrix[2]);

		y.mV[2] = 0.f;
		y.normalize();

		level.setRows(x,y,z);
		level.orthogonalize();

		LLQuaternion quat(level);
		sFlycamRotation = nlerp(llmin(feather * time, 1.f), sFlycamRotation,
								quat);
	}

	static LLCachedControl<bool> zoom_direct(gSavedSettings, "ZoomDirect");
	if (zoom_direct)
	{
		sFlycamZoom = sLastDelta[6] * axis_scale[6] + dead_zone[6];
	}
	else
	{
		sFlycamZoom += sDelta[6];
	}

	LLMatrix3 mat(sFlycamRotation);

	gViewerCamera.setView(sFlycamZoom);
	gViewerCamera.setOrigin(sFlycamPosition);
	gViewerCamera.mXAxis = LLVector3(mat.mMatrix[0]);
	gViewerCamera.mYAxis = LLVector3(mat.mMatrix[1]);
	gViewerCamera.mZAxis = LLVector3(mat.mMatrix[2]);
}

bool LLViewerJoystick::toggleFlycam()
{
	if (!mJoystickEnabled || !mJoystickFlycamEnabled)
	{
		mOverrideCamera = false;
		return false;
	}

	if (!mOverrideCamera)
	{
		gAgent.changeCameraToDefault();
	}

	if (gAwayTimer.getElapsedTimeF32() > MIN_AFK_TIME)
	{
		gAgent.clearAFK();
	}

	mOverrideCamera = !mOverrideCamera;
	if (mOverrideCamera)
	{
		moveFlycam(true);

	}
	else if (!gToolMgr.inBuildMode())
	{
		moveAvatar(true);
	}
	else
	{
		// We are in build mode, exiting from the flycam mode: since we are
		// going to keep the flycam POV for the main camera until the avatar
		// moves, we need to track this situation.
		setCameraNeedsUpdate(false);
		setNeedsReset(true);
	}
	return true;
}

void LLViewerJoystick::scanJoystick()
{
	if (mDriverState != JDS_INITIALIZED || !mJoystickEnabled)
	{
		return;
	}

#if LL_WINDOWS
	// On windows, the flycam is updated syncronously with a timer, so there is
	// no need to update the status of the joystick here.
	if (!mOverrideCamera)
#endif
	{
		updateStatus();
	}

	static bool toggle_flycam = false;
	static LLCachedControl<S32> fly_cam_button(gSavedSettings,
											   "JoystickButtonFlyCam");
	bool fly_cam = getJoystickButton(fly_cam_button);
	if (fly_cam)
    {
		if (fly_cam != toggle_flycam)
		{
			toggle_flycam = toggleFlycam();
		}
	}
	else
	{
		toggle_flycam = false;
	}

	if (!mOverrideCamera &&
		!(gToolMgr.inBuildMode() && mJoystickBuildEnabled))
	{
		moveAvatar();
	}
}

std::string LLViewerJoystick::getDescription()
{
	std::string res;
	if (mDriverState == JDS_INITIALIZED && mNdofDev)
	{
		res = ll_safe_string(mNdofDev->product);
		LLStringUtil::replaceNonstandardASCII(res, ' ');
		LLStringUtil::replaceChar(res, '\n', ' ');
		LLStringUtil::trim(res);
	}
	return res;
}

bool LLViewerJoystick::isLikeSpaceNavigator() const
{
	return isJoystickInitialized() && strlen(mNdofDev->product) > 5 &&
		   strncmp(mNdofDev->product, "Space", 5) == 0;
}

void LLViewerJoystick::setToDefaults()
{
	llinfos << "Restoring defaults." << llendl;

	LLControlVariable* controlp = gSavedSettings.getControl("JoystickAxis0");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("JoystickAxis1");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("JoystickAxis2");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("JoystickAxis3");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("JoystickAxis4");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("JoystickAxis5");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("JoystickAxis6");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("Cursor3D");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AutoLeveling");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("ZoomDirect");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisScale0");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisScale1");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisScale2");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisScale3");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisScale4");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisScale5");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisScale0");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisScale1");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisScale2");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisScale3");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisScale4");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisScale5");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisScale0");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisScale1");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisScale2");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisScale3");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisScale4");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisScale5");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisDeadZone0");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisDeadZone1");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisDeadZone2");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisDeadZone3");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisDeadZone4");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarAxisDeadZone5");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisDeadZone0");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisDeadZone1");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisDeadZone2");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisDeadZone3");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisDeadZone4");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildAxisDeadZone5");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisDeadZone0");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisDeadZone1");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisDeadZone2");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisDeadZone3");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisDeadZone4");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisDeadZone5");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamAxisDeadZone6");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("AvatarFeathering");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("BuildFeathering");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("FlycamFeathering");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("JoystickButtonFlyCam");
	controlp->resetToDefault(true);
	controlp = gSavedSettings.getControl("JoystickButtonJump");
	controlp->resetToDefault(true);
}

void LLViewerJoystick::setSNDefaults()
{
#if LL_DARWIN || LL_LINUX
	constexpr float platform_scale = 20.f;
	constexpr float platform_scale_av_xz = 1.f;
	// The SpaceNavigator does not act as a 3D cursor on OS X / Linux.
	constexpr bool is_3d_cursor = false;
#else
	constexpr float platform_scale = 1.f;
	constexpr float platform_scale_av_xz = 2.f;
	constexpr bool is_3d_cursor = true;
#endif

	llinfos << "Setting to SpaceNavigator defaults." << llendl;

	gSavedSettings.setS32("JoystickAxis0", 1); // z (at)
	gSavedSettings.setS32("JoystickAxis1", 0); // x (slide)
	gSavedSettings.setS32("JoystickAxis2", 2); // y (up)
	gSavedSettings.setS32("JoystickAxis3", 4); // pitch
	gSavedSettings.setS32("JoystickAxis4", 3); // roll
	gSavedSettings.setS32("JoystickAxis5", 5); // yaw
	gSavedSettings.setS32("JoystickAxis6", -1);

	gSavedSettings.setBool("Cursor3D", is_3d_cursor);
	gSavedSettings.setBool("AutoLeveling", true);
	gSavedSettings.setBool("ZoomDirect", false);

	gSavedSettings.setF32("AvatarAxisScale0", 1.f * platform_scale_av_xz);
	gSavedSettings.setF32("AvatarAxisScale1", 1.f * platform_scale_av_xz);
	gSavedSettings.setF32("AvatarAxisScale2", 1.f);
	gSavedSettings.setF32("AvatarAxisScale4", 0.1f * platform_scale);
	gSavedSettings.setF32("AvatarAxisScale5", 0.1f * platform_scale);
	gSavedSettings.setF32("AvatarAxisScale3", 0.f * platform_scale);
	gSavedSettings.setF32("BuildAxisScale1", 0.3f * platform_scale);
	gSavedSettings.setF32("BuildAxisScale2", 0.3f * platform_scale);
	gSavedSettings.setF32("BuildAxisScale0", 0.3f * platform_scale);
	gSavedSettings.setF32("BuildAxisScale4", 0.3f * platform_scale);
	gSavedSettings.setF32("BuildAxisScale5", 0.3f * platform_scale);
	gSavedSettings.setF32("BuildAxisScale3", 0.3f * platform_scale);
	gSavedSettings.setF32("FlycamAxisScale1", 2.f * platform_scale);
	gSavedSettings.setF32("FlycamAxisScale2", 2.f * platform_scale);
	gSavedSettings.setF32("FlycamAxisScale0", 2.1f * platform_scale);
	gSavedSettings.setF32("FlycamAxisScale4", 0.1f * platform_scale);
	gSavedSettings.setF32("FlycamAxisScale5", 0.15f * platform_scale);
	gSavedSettings.setF32("FlycamAxisScale3", 0.f * platform_scale);
	gSavedSettings.setF32("FlycamAxisScale6", 0.f * platform_scale);

	gSavedSettings.setF32("AvatarAxisDeadZone0", 0.1f);
	gSavedSettings.setF32("AvatarAxisDeadZone1", 0.1f);
	gSavedSettings.setF32("AvatarAxisDeadZone2", 0.1f);
	gSavedSettings.setF32("AvatarAxisDeadZone3", 1.f);
	gSavedSettings.setF32("AvatarAxisDeadZone4", 0.02f);
	gSavedSettings.setF32("AvatarAxisDeadZone5", 0.01f);
	gSavedSettings.setF32("BuildAxisDeadZone0", 0.01f);
	gSavedSettings.setF32("BuildAxisDeadZone1", 0.01f);
	gSavedSettings.setF32("BuildAxisDeadZone2", 0.01f);
	gSavedSettings.setF32("BuildAxisDeadZone3", 0.01f);
	gSavedSettings.setF32("BuildAxisDeadZone4", 0.01f);
	gSavedSettings.setF32("BuildAxisDeadZone5", 0.01f);
	gSavedSettings.setF32("FlycamAxisDeadZone0", 0.01f);
	gSavedSettings.setF32("FlycamAxisDeadZone1", 0.01f);
	gSavedSettings.setF32("FlycamAxisDeadZone2", 0.01f);
	gSavedSettings.setF32("FlycamAxisDeadZone3", 0.01f);
	gSavedSettings.setF32("FlycamAxisDeadZone4", 0.01f);
	gSavedSettings.setF32("FlycamAxisDeadZone5", 0.01f);
	gSavedSettings.setF32("FlycamAxisDeadZone6", 1.f);

	gSavedSettings.setF32("AvatarFeathering", 6.f);
	gSavedSettings.setF32("BuildFeathering", 12.f);
	gSavedSettings.setF32("FlycamFeathering", 5.f);

	gSavedSettings.setS32("JoystickButtonFlyCam", 0);
	gSavedSettings.setS32("JoystickButtonJump", 1);
}
