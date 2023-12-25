/** 
 * @file llprefsinput.cpp
 * @brief Input preferences panel
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Adapted by Henri Beauchamp from llpanelinput.cpp
 * Copyright (c) 2004-2009 Linden Research, Inc. (c) 2011 Henri Beauchamp
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

#include "llprefsinput.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llnotifications.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"

#include "llfloaterjoystick.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"

class LLPrefsInputImpl final : public LLPanel
{
public:
	LLPrefsInputImpl();
	~LLPrefsInputImpl() override 	{}

	void refresh() override;
	void draw() override;

	void apply();
	void cancel();

private:
	static void onTabChanged(void* data, bool from_click);

	static void onClickJoystickSetup(void* data);
	static void onClickResetToDefault(void* data);
	static void onCommitCheckPrivateLookAt(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckPrivatePointAt(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckLimitSelectDistance(LLUICtrl* ctrl, void* data);
	static void onCommitRadioDoubleClickAction(LLUICtrl* ctrl, void* data);
	static void onRearOffsetAdjust(LLUICtrl* ctrl, void* data);
	static void onFrontOffsetAdjust(LLUICtrl* ctrl, void* data);
	static void onCommitCheckNoJoystick(LLUICtrl* ctrl, void* user_data);
	void refreshValues();

private:
	static bool		sDirty;

	LLTabContainer*	mTabContainer;

	LLSpinCtrl*		mSpinRearX;
	LLSpinCtrl*		mSpinRearY;
	LLSpinCtrl*		mSpinRearZ;
	LLSpinCtrl*		mSpinFrontX;
	LLSpinCtrl*		mSpinFrontY;
	LLSpinCtrl*		mSpinFrontZ;

	LLButton*		mJoystickButton;

	F32				mMouseSensitivity;
	F32				mMaxSelectDistance;
	F32				mCameraAngle;
	F32				mCameraOffsetScale;
	U32				mCameraToPelvisRotDeviation;
	U32				mPrivateLookAtLimit;
	U32				mPrivatePointAtLimit;
	U32				mDoubleClickAction;
	LLVector3		mCameraOffsetDefault;
	LLVector3		mCameraOffsetFrontView;
	bool			mDoubleClickScriptedObject;
	bool			mJoystickNeverEnable;
	bool			mMouseSmooth;
	bool			mPrivateLookAt;
	bool			mPrivatePointAt;
	bool			mLimitSelectDistance;
	bool			mInvertMouse;
	bool			mShowCrosshairs;
	bool			mFirstPersonAvatarVisible;
	bool			mMouselookRenderRigged;
	bool			mCameraIgnoreCollisions;
	bool			mDisableCameraConstraints;
	bool			mResetViewRotatesAvatar;
	bool			mEditCameraMovement;
	bool			mAppearanceCameraMovement;
	bool			mThumbnailSnapshotFrontView;
	bool			mSitCameraFrontView;
	bool			mAutomaticFly;
	bool			mArrowKeysMoveAvatar;
	bool			mMouseLookUseRotDeviation;
	bool			mEyesFollowMousePointer;
	bool			mLeftClickSteersAvatar;
	bool			mLeftClickToOpen;
	bool			mLeftClickToPay;
	bool			mLeftClickToPlay;
	bool			mLeftClickToSit;
	bool			mLeftClickToZoom;

	bool			mFirstRun;
};

bool LLPrefsInputImpl::sDirty = false;

LLPrefsInputImpl::LLPrefsInputImpl() 
:	LLPanel(std::string("Input and Camera Preferences")),
	mFirstRun(true)
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_input.xml");

	mTabContainer = getChild<LLTabContainer>("Input and Camera");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("Input Controls");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("Camera Controls");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	mJoystickButton = getChild<LLButton>("joystick_setup_button");
	mJoystickButton->setClickedCallback(onClickJoystickSetup, this);

	childSetAction("mouse_sensitivity_reset_button", onClickResetToDefault,
				   (void*)"MouseSensitivity");
	childSetAction("camera_angle_reset_button", onClickResetToDefault,
				   (void*)"CameraAngle");
	childSetAction("max_rot_reset_button", onClickResetToDefault,
				   (void*)"CameraToPelvisRotDeviation");
	childSetAction("offset_scale_reset_button", onClickResetToDefault,
				   (void*)"CameraOffsetScale");
	childSetAction("rear_offset_reset_button", onClickResetToDefault,
				   (void*)"CameraOffsetDefault");
	childSetAction("front_offset_reset_button", onClickResetToDefault,
				   (void*)"CameraOffsetFrontView");
	childSetCommitCallback("private_look_at_check",
						   onCommitCheckPrivateLookAt, this);
	childSetCommitCallback("private_point_at_check",
						   onCommitCheckPrivatePointAt, this);
	childSetCommitCallback("limit_select_distance",
						   onCommitCheckLimitSelectDistance, this);
	childSetCommitCallback("double_click_action",
						   onCommitRadioDoubleClickAction, this);

	childSetCommitCallback("no_joystick_check",
						   onCommitCheckNoJoystick, this);

	LLSliderCtrl* fov_slider = getChild<LLSliderCtrl>("camera_angle");
	fov_slider->setMinValue(gViewerCamera.getMinView());
	fov_slider->setMaxValue(gViewerCamera.getMaxView());
	fov_slider->setValue(gViewerCamera.getView());

	mSpinRearX = getChild<LLSpinCtrl>("rear_offset_x");
	mSpinRearX->setCommitCallback(onRearOffsetAdjust);
	mSpinRearX->setCallbackUserData(this);
	
	mSpinRearY = getChild<LLSpinCtrl>("rear_offset_y");
	mSpinRearY->setCommitCallback(onRearOffsetAdjust);
	mSpinRearY->setCallbackUserData(this);

	mSpinRearZ = getChild<LLSpinCtrl>("rear_offset_z");
	mSpinRearZ->setCommitCallback(onRearOffsetAdjust);
	mSpinRearZ->setCallbackUserData(this);

	mSpinFrontX = getChild<LLSpinCtrl>("front_offset_x");
	mSpinFrontX->setCommitCallback(onFrontOffsetAdjust);
	mSpinFrontX->setCallbackUserData(this);

	mSpinFrontY = getChild<LLSpinCtrl>("front_offset_y");
	mSpinFrontY->setCommitCallback(onFrontOffsetAdjust);
	mSpinFrontY->setCallbackUserData(this);

	mSpinFrontZ = getChild<LLSpinCtrl>("front_offset_z");
	mSpinFrontZ->setCommitCallback(onFrontOffsetAdjust);
	mSpinFrontZ->setCallbackUserData(this);

	refresh();
}

void LLPrefsInputImpl::draw()
{
	if (mFirstRun)
	{
		mFirstRun = false;
		mTabContainer->selectTab(gSavedSettings.getS32("LastInputPrefTab"));
	}

	if (sDirty)
	{
		sDirty = false;

		LLVector3 offset = gSavedSettings.getVector3("CameraOffsetDefault");
		mSpinRearX->set(offset.mV[VX]);
		mSpinRearY->set(offset.mV[VY]);
		mSpinRearZ->set(offset.mV[VZ]);

		offset = gSavedSettings.getVector3("CameraOffsetFrontView");
		mSpinFrontX->set(offset.mV[VX]);
		mSpinFrontY->set(offset.mV[VY]);
		mSpinFrontZ->set(offset.mV[VZ]);
	}

	LLPanel::draw();
}

void LLPrefsInputImpl::refreshValues()
{
	mCameraAngle				= gSavedSettings.getF32("CameraAngle");
	mCameraOffsetScale			= gSavedSettings.getF32("CameraOffsetScale");
	mMouseSensitivity			= gSavedSettings.getF32("MouseSensitivity");
	mMaxSelectDistance			= gSavedSettings.getF32("MaxSelectDistance");
	mCameraToPelvisRotDeviation	= gSavedSettings.getU32("CameraToPelvisRotDeviation");
	mPrivateLookAtLimit			= gSavedSettings.getU32("PrivateLookAtLimit");
	mPrivatePointAtLimit		= gSavedSettings.getU32("PrivatePointAtLimit");
	mDoubleClickAction			= gSavedSettings.getU32("DoubleClickAction");
	mDoubleClickScriptedObject	= gSavedSettings.getBool("DoubleClickScriptedObject");
	mJoystickNeverEnable		= gSavedSettings.getBool("JoystickNeverEnable");
	mMouseSmooth				= gSavedSettings.getBool("MouseSmooth");
	mPrivateLookAt				= gSavedSettings.getBool("PrivateLookAt");
	mPrivatePointAt				= gSavedSettings.getBool("PrivatePointAt");
	mLimitSelectDistance		= gSavedSettings.getBool("LimitSelectDistance");
	mInvertMouse				= gSavedSettings.getBool("InvertMouse");
	mShowCrosshairs				= gSavedSettings.getBool("ShowCrosshairs");
	mFirstPersonAvatarVisible	= gSavedSettings.getBool("FirstPersonAvatarVisible");
	mMouselookRenderRigged		= gSavedSettings.getBool("MouselookRenderRigged");
	mCameraIgnoreCollisions		= gSavedSettings.getBool("CameraIgnoreCollisions");
	mDisableCameraConstraints	= gSavedSettings.getBool("DisableCameraConstraints");
	mResetViewRotatesAvatar		= gSavedSettings.getBool("ResetViewRotatesAvatar");
	mEditCameraMovement			= gSavedSettings.getBool("EditCameraMovement");
	mAppearanceCameraMovement	= gSavedSettings.getBool("AppearanceCameraMovement");
	mThumbnailSnapshotFrontView	= gSavedSettings.getBool("ThumbnailSnapshotFrontView");
	mSitCameraFrontView			= gSavedSettings.getBool("SitCameraFrontView");
	mAutomaticFly				= gSavedSettings.getBool("AutomaticFly");
	mArrowKeysMoveAvatar		= gSavedSettings.getBool("ArrowKeysMoveAvatar");
	mMouseLookUseRotDeviation	= gSavedSettings.getBool("MouseLookUseRotDeviation");
	mEyesFollowMousePointer		= gSavedSettings.getBool("EyesFollowMousePointer");
	mLeftClickSteersAvatar		= gSavedSettings.getBool("LeftClickSteersAvatar");
	mLeftClickToOpen			= gSavedSettings.getBool("LeftClickToOpen");
	mLeftClickToPay				= gSavedSettings.getBool("LeftClickToPay");
	mLeftClickToPlay			= gSavedSettings.getBool("LeftClickToPlay");
	mLeftClickToSit				= gSavedSettings.getBool("LeftClickToSit");
	mLeftClickToZoom			= gSavedSettings.getBool("LeftClickToZoom");
	mCameraOffsetDefault		= gSavedSettings.getVector3("CameraOffsetDefault");
	mCameraOffsetFrontView		= gSavedSettings.getVector3("CameraOffsetFrontView");
}

void LLPrefsInputImpl::refresh()
{
	refreshValues();

	childSetEnabled("private_look_at_limit", mPrivateLookAt);
	childSetEnabled("private_look_at_limit_meters", mPrivateLookAt);
	childSetEnabled("private_point_at_limit", mPrivatePointAt);
	childSetEnabled("private_point_at_limit_meters", mPrivatePointAt);
	childSetEnabled("max_select_distance", mLimitSelectDistance);
	childSetEnabled("select_distance_meters", mLimitSelectDistance);
	childSetEnabled("scripted_object_check", mDoubleClickAction != 0);

	mSpinRearX->set(mCameraOffsetDefault.mV[VX]);
	mSpinRearY->set(mCameraOffsetDefault.mV[VY]);
	mSpinRearZ->set(mCameraOffsetDefault.mV[VZ]);

	mSpinFrontX->set(mCameraOffsetFrontView.mV[VX]);
	mSpinFrontY->set(mCameraOffsetFrontView.mV[VY]);
	mSpinFrontZ->set(mCameraOffsetFrontView.mV[VZ]);

	mJoystickButton->setEnabled(!mJoystickNeverEnable);
}

void LLPrefsInputImpl::apply()
{
	onRearOffsetAdjust(NULL, this);
	onFrontOffsetAdjust(NULL, this);

	refreshValues();
}

void LLPrefsInputImpl::cancel()
{
	gViewerCamera.setDefaultFOV(mCameraAngle);
	gSavedSettings.setF32("CameraAngle",				gViewerCamera.getView());
	gSavedSettings.setF32("CameraOffsetScale",			mCameraOffsetScale);
	gSavedSettings.setF32("MouseSensitivity",			mMouseSensitivity);
	gSavedSettings.setF32("MaxSelectDistance",			mMaxSelectDistance);
	gSavedSettings.setU32("CameraToPelvisRotDeviation",	mCameraToPelvisRotDeviation);
	gSavedSettings.setU32("PrivateLookAtLimit",			mPrivateLookAtLimit);
	gSavedSettings.setU32("PrivatePointAtLimit",		mPrivatePointAtLimit);
	gSavedSettings.setU32("DoubleClickAction",			mDoubleClickAction);
	gSavedSettings.setBool("DoubleClickScriptedObject",	mDoubleClickScriptedObject);
	gSavedSettings.setBool("JoystickNeverEnable",		mJoystickNeverEnable);
	gSavedSettings.setBool("MouseSmooth",				mMouseSmooth);
	gSavedSettings.setBool("PrivateLookAt",				mPrivateLookAt);
	gSavedSettings.setBool("PrivatePointAt",			mPrivatePointAt);
	gSavedSettings.setBool("LimitSelectDistance",		mLimitSelectDistance);
	gSavedSettings.setBool("InvertMouse",				mInvertMouse);
	gSavedSettings.setBool("ShowCrosshairs",			mShowCrosshairs);
	gSavedSettings.setBool("FirstPersonAvatarVisible",	mFirstPersonAvatarVisible);
	gSavedSettings.setBool("MouselookRenderRigged",		mMouselookRenderRigged);
	gSavedSettings.setBool("CameraIgnoreCollisions",	mCameraIgnoreCollisions);
	gSavedSettings.setBool("DisableCameraConstraints",	mDisableCameraConstraints);
	gSavedSettings.setBool("ResetViewRotatesAvatar",	mResetViewRotatesAvatar);
	gSavedSettings.setBool("EditCameraMovement",		mEditCameraMovement);
	gSavedSettings.setBool("AppearanceCameraMovement",	mAppearanceCameraMovement);
	gSavedSettings.setBool("ThumbnailSnapshotFrontView", mThumbnailSnapshotFrontView);
	gSavedSettings.setBool("SitCameraFrontView",		mSitCameraFrontView);
	gSavedSettings.setBool("AutomaticFly",				mAutomaticFly);
	gSavedSettings.setBool("ArrowKeysMoveAvatar",		mArrowKeysMoveAvatar);
	gSavedSettings.setBool("MouseLookUseRotDeviation",	mMouseLookUseRotDeviation);
	gSavedSettings.setBool("EyesFollowMousePointer",	mEyesFollowMousePointer);
	gSavedSettings.setBool("LeftClickSteersAvatar",		mLeftClickSteersAvatar);
	gSavedSettings.setBool("LeftClickToOpen",			mLeftClickToOpen);
	gSavedSettings.setBool("LeftClickToPay",			mLeftClickToPay);
	gSavedSettings.setBool("LeftClickToPlay",			mLeftClickToPlay);
	gSavedSettings.setBool("LeftClickToSit",			mLeftClickToSit);
	gSavedSettings.setBool("LeftClickToZoom",			mLeftClickToZoom);
	gSavedSettings.setVector3("CameraOffsetDefault",	mCameraOffsetDefault);
	gSavedSettings.setVector3("CameraOffsetFrontView",	mCameraOffsetFrontView);
}

//static
void LLPrefsInputImpl::onTabChanged(void* data, bool from_click)
{
	LLPrefsInputImpl* self = (LLPrefsInputImpl*)data;
	if (self && self->mTabContainer)
	{
		gSavedSettings.setS32("LastInputPrefTab",
							  self->mTabContainer->getCurrentPanelIndex());
	}
}

//static
void LLPrefsInputImpl::onClickJoystickSetup(void* data)
{
	LLPrefsInputImpl* prefs = (LLPrefsInputImpl*)data;
	LLFloaterJoystick* floaterp = LLFloaterJoystick::showInstance();
	LLFloater* parentp = gFloaterViewp->getParentFloater(prefs);
	if (parentp)
	{
		parentp->addDependentFloater(floaterp, false);
	}
}

//static
void LLPrefsInputImpl::onClickResetToDefault(void* data)
{
	std::string setting(static_cast<char*>(data));
	LLControlVariable* controlp = gSavedSettings.getControl(setting.c_str());
	if (controlp)
	{
		controlp->resetToDefault(true);
	}
	if (setting == "CameraAngle")
	{
		// Get the value that was just reset to default
		F32 fov = gSavedSettings.getF32("CameraAngle");
		// Attempt to set the camera with it
		gViewerCamera.setDefaultFOV(fov);
		// Recover the actual value
		fov = gViewerCamera.getView();
		// Store it
		gSavedSettings.setF32("CameraAngle", fov);
	}
	else if (setting == "CameraOffsetDefault" ||
			 setting == "CameraOffsetFrontView")
	{
		sDirty = true;
	}
}

//static
void LLPrefsInputImpl::onCommitCheckPrivateLookAt(LLUICtrl* ctrl,
													void* user_data)
{
	LLPrefsInputImpl* self = (LLPrefsInputImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		bool enabled = check->get();
		self->childSetEnabled("private_look_at_limit", enabled);
		self->childSetEnabled("private_look_at_limit_meters", enabled);
	}
}

//static
void LLPrefsInputImpl::onCommitCheckPrivatePointAt(LLUICtrl* ctrl,
													 void* user_data)
{
	LLPrefsInputImpl* self = (LLPrefsInputImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		bool enabled = check->get();
		self->childSetEnabled("private_point_at_limit", enabled);
		self->childSetEnabled("private_point_at_limit_meters", enabled);
	}
}

//static
void LLPrefsInputImpl::onCommitCheckLimitSelectDistance(LLUICtrl* ctrl,
														void* data)
{
	LLPrefsInputImpl* self = (LLPrefsInputImpl*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		bool enable = check->get();
		self->childSetEnabled("max_select_distance", enable);
		self->childSetEnabled("select_distance_meters", enable);
	}
}

//static
void LLPrefsInputImpl::onCommitRadioDoubleClickAction(LLUICtrl* ctrl,
													  void* data)
{
	LLPrefsInputImpl* self = (LLPrefsInputImpl*)data;
	if (!self) return;

	bool enable = gSavedSettings.getU32("DoubleClickAction") != 0;
	self->childSetEnabled("scripted_object_check", enable);
}

//static
void LLPrefsInputImpl::onRearOffsetAdjust(LLUICtrl* ctrl, void* data)
{
	LLPrefsInputImpl* self = (LLPrefsInputImpl*)data;
	if (self)
	{
		LLVector3 value = LLVector3(self->mSpinRearX->get(),
									self->mSpinRearY->get(),
									self->mSpinRearZ->get());
		gSavedSettings.setVector3("CameraOffsetDefault", value);
	}
}

//static
void LLPrefsInputImpl::onFrontOffsetAdjust(LLUICtrl* ctrl, void* data)
{
	LLPrefsInputImpl* self = (LLPrefsInputImpl*)data;
	if (self)
	{
		LLVector3 value = LLVector3(self->mSpinFrontX->get(),
									self->mSpinFrontY->get(),
									self->mSpinFrontZ->get());
		gSavedSettings.setVector3("CameraOffsetFrontView", value);
	}
}

//static
void LLPrefsInputImpl::onCommitCheckNoJoystick(LLUICtrl* ctrl,
											   void* user_data)
{
	LLPrefsInputImpl* self = (LLPrefsInputImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	self->mJoystickButton->setEnabled(!enabled);
 	if (self->mJoystickNeverEnable != enabled)
	{
		gNotifications.add("InEffectAfterRestart");
	}
}

//---------------------------------------------------------------------------

LLPrefsInput::LLPrefsInput()
:	impl(* new LLPrefsInputImpl())
{
}

LLPrefsInput::~LLPrefsInput()
{
	delete &impl;
}

void LLPrefsInput::apply()
{
	impl.apply();
}

void LLPrefsInput::cancel()
{
	impl.cancel();
}

LLPanel* LLPrefsInput::getPanel()
{
	return &impl;
}
