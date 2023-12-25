/**
 * @file llviewerjoystick.h
 * @brief Viewer joystick / NDOF device functionality.
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

#ifndef LL_LLVIEWERJOYSTICK_H
#define LL_LLVIEWERJOYSTICK_H

#include "ndofdev_external.h"

#include "llcontrol.h"

typedef enum e_joystick_driver_state
{
	JDS_UNINITIALIZED,
	JDS_INITIALIZED,
	JDS_INITIALIZING
} EJoystickDriverState;

class LLViewerJoystick final : public LLSingleton<LLViewerJoystick>
{
	friend class LLSingleton<LLViewerJoystick>;

protected:
	LOG_CLASS(LLViewerJoystick);

public:
	LLViewerJoystick();
	~LLViewerJoystick() override;

	void init(bool autoenable);
	void terminate();

	void updateStatus();
	void scanJoystick();
	void moveObjects(bool reset = false);
	void moveAvatar(bool reset = false);
	void moveFlycam(bool reset = false);

	std::string getDescription();
	F32 getJoystickAxis(S32 axis) const;
	bool getJoystickButton(S32 button) const;

	LL_INLINE bool isJoystickInitialized() const	{ return mDriverState == JDS_INITIALIZED; }
	bool isLikeSpaceNavigator() const;

	LL_INLINE void setNeedsReset(bool reset = true)	{ mResetFlag = reset; }

	LL_INLINE void setCameraNeedsUpdate(bool b)		{ mCameraUpdated = b; }
	LL_INLINE bool getCameraNeedsUpdate() const		{ return mCameraUpdated; }

	LL_INLINE bool getOverrideCamera()				{ return mOverrideCamera; }
	void setOverrideCamera(bool val);
	bool toggleFlycam();

	void setToDefaults();
	void setSNDefaults();

protected:
	void updateEnabled(bool autoenable);
	void handleRun(F32 inc);
	void agentSlide(F32 inc);
	void agentPush(F32 inc);
	void agentFly(F32 inc);
	void agentRotate(F32 pitch_inc, F32 turn_inc);
    void agentJump();
	void resetDeltas(S32 axis[]);

	static NDOF_HotPlugResult hotPlugAddCallback(NDOF_Device* dev);
	static void hotPlugRemovalCallback(NDOF_Device* dev);

private:
	LLCachedControl<bool>	mJoystickEnabled;
	LLCachedControl<bool>	mJoystickAvatarEnabled;
	LLCachedControl<bool>	mJoystickFlycamEnabled;
	LLCachedControl<bool>	mJoystickBuildEnabled;

	LLCachedControl<bool>	mCursor3D;

	LLCachedControl<S32>	mJoystickAxis0;
	LLCachedControl<S32>	mJoystickAxis1;
	LLCachedControl<S32>	mJoystickAxis2;
	LLCachedControl<S32>	mJoystickAxis3;
	LLCachedControl<S32>	mJoystickAxis4;
	LLCachedControl<S32>	mJoystickAxis5;
	LLCachedControl<S32>	mJoystickAxis6;

	NDOF_Device*			mNdofDev;

	EJoystickDriverState	mDriverState;

	F32						mPerfScale;
	U32						mJoystickRun;

	F32						mAxes[6];
	bool					mBtn[16];

	bool					mResetFlag;
	bool					mCameraUpdated;
	bool 					mOverrideCamera;

	static F32				sLastDelta[7];
	static F32				sDelta[7];
};

#endif
