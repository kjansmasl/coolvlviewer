/**
 * @file llfloaterjoystick.cpp
 * @brief Joystick preferences panel
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterjoystick.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llstat.h"
#include "llstatview.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llappviewer.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewerjoystick.h"

LLFloaterJoystick::LLFloaterJoystick(const LLSD& data)
:	LLFloater("joystick"),
	mAxisStatsView(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_joystick.xml");
	center();
}

//virtual
bool LLFloaterJoystick::postBuild()
{
	mCheckJoystickEnabled = getChild<LLCheckBoxCtrl>("enable_joystick");
	childSetCommitCallback("enable_joystick", onCommitJoystickEnabled, this);

	mJoystickType = getChild<LLTextBox>("joystick_type");

	mCheckFlycamEnabled = getChild<LLCheckBoxCtrl>("JoystickFlycamEnabled");
	mCheckFlycamEnabled->setCommitCallback(onCommitJoystickEnabled);
	mCheckFlycamEnabled->setCallbackUserData(this);

	mInitButon = getChild<LLButton>("init_btn");
	mInitButon->setClickedCallback(onClickInit, this);

	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("ok_btn", onClickOK, this);

	childSetCommitCallback("defaults_btn", onClickRestoreDefaults, this);

	refresh();

	if (!LLStartUp::isLoggedIn())
	{
		// If not logged in, the joystick is not read in the main loop and the
		// monitor cannot be updated, so do not build the latter at all !
		return true;
	}

	childSetVisible("no_monitor", false);

	LLUIString joystick = getString("JoystickMonitor");
	// Use this child to get relative positioning info; we will place the
	// joystick monitor on its right, vertically aligned to it.
	LLRect rect;
	LLView* child = getChild<LLView>("FlycamAxisScale1");
	LLRect r = child->getRect();
	// Note: the stats view height is automatically adjusted (thus the final 0)
	rect = LLRect(r.mRight + 10, r.mTop, r.mRight + 170, 0);
	mAxisStatsView = new LLStatView("axis values", joystick, "", rect);
	mAxisStatsView->setDisplayChildren(true);
	mAxisStatsView->setCanCollapse(false);

	F32 range = gSavedSettings.getBool("Cursor3D") ? 1024.f : 2.f;
	LLUIString axis = getString("Axis");
	for (U32 i = 0; i < 6; ++i)
	{
		axis.setArg("[NUM]", llformat("%d", i));
		mAxisStats[i] = new LLStat(4);
		mAxisStatsBar[i] = mAxisStatsView->addStat(axis, mAxisStats[i], "",
												   true);	// Display the bar
		mAxisStatsBar[i]->mNoResize = true;
		mAxisStatsBar[i]->mMinBar = -range;
		mAxisStatsBar[i]->mMaxBar = range;
		mAxisStatsBar[i]->mLabelSpacing = range > 100.f ? range : range * 0.5f;
		mAxisStatsBar[i]->mTickSpacing = range * 0.25f;
	}

	addChild(mAxisStatsView);

	for (S32 i = 0; i < 16; ++i)
	{
		mJoystickButtons[i] =
			getChild<LLTextBox>(llformat("btn%d_text", i).c_str());
		mJoystickButtons[i]->setVisible(true);
	}

	return true;
}

//virtual
void LLFloaterJoystick::draw()
{
	LLViewerJoystick* joystick = LLViewerJoystick::getInstance();

	bool init_done = joystick->isJoystickInitialized();

	mCheckJoystickEnabled->setEnabled(init_done);

	std::string desc = joystick->getDescription();
	if (desc.empty())
	{
		desc = getString("NoDevice");
	}
	mJoystickType->setValue(desc);
	mJoystickType->setEnabled(init_done);

	mInitButon->setEnabled(!init_done);

	if (mAxisStatsView)
	{
		static LLCachedControl<bool> cursor_3d(gSavedSettings, "Cursor3D");
		F32 range = cursor_3d ? 1024.f : 2.f;
		static F32 old_range = 0.f;
		for (S32 i = 0; i < 6; ++i)
		{
			F32 value = joystick->getJoystickAxis(i);
			mAxisStats[i]->addValue(value * gFrameIntervalSeconds);
			if (mAxisStatsBar[i]->mMinBar > value)
			{
				mAxisStatsBar[i]->mMinBar = value;
			}
			if (mAxisStatsBar[i]->mMaxBar < value)
			{
				mAxisStatsBar[i]->mMaxBar = value;
			}
			if (old_range != range)
			{
				mAxisStatsBar[i]->mMinBar = -range;
				mAxisStatsBar[i]->mMaxBar = range;
				mAxisStatsBar[i]->mLabelSpacing = range > 100.f ? range
																: range * 0.5f;
				mAxisStatsBar[i]->mTickSpacing = range * 0.25f;
			}
		}
		old_range = range;
		for (S32 i = 0; i < 16; ++i)
		{
			bool pressed = joystick->getJoystickButton(i);
			if (pressed)
			{
				mJoystickButtons[i]->setColor(LLColor4::white);
			}
			else
			{
				mJoystickButtons[i]->setColor(LLColor4::grey4);
			}
		}
	}

	LLFloater::draw();
}

//virtual
void LLFloaterJoystick::refresh()
{
	LLFloater::refresh();

	mJoystickEnabled = gSavedSettings.getBool("JoystickEnabled");

	mJoystickAxis[0] = gSavedSettings.getS32("JoystickAxis0");
	mJoystickAxis[1] = gSavedSettings.getS32("JoystickAxis1");
	mJoystickAxis[2] = gSavedSettings.getS32("JoystickAxis2");
	mJoystickAxis[3] = gSavedSettings.getS32("JoystickAxis3");
	mJoystickAxis[4] = gSavedSettings.getS32("JoystickAxis4");
	mJoystickAxis[5] = gSavedSettings.getS32("JoystickAxis5");
	mJoystickAxis[6] = gSavedSettings.getS32("JoystickAxis6");

	mJoystickButtonFlyCam = gSavedSettings.getS32("JoystickButtonFlyCam");
	mJoystickButtonJump = gSavedSettings.getS32("JoystickButtonJump");

	m3DCursor = gSavedSettings.getBool("Cursor3D");
	mAutoLeveling = gSavedSettings.getBool("AutoLeveling");
	mZoomDirect  = gSavedSettings.getBool("ZoomDirect");

	mAvatarEnabled = gSavedSettings.getBool("JoystickAvatarEnabled");
	mBuildEnabled = gSavedSettings.getBool("JoystickBuildEnabled");
	mFlycamEnabled = gSavedSettings.getBool("JoystickFlycamEnabled");

	mAvatarAxisScale[0] = gSavedSettings.getF32("AvatarAxisScale0");
	mAvatarAxisScale[1] = gSavedSettings.getF32("AvatarAxisScale1");
	mAvatarAxisScale[2] = gSavedSettings.getF32("AvatarAxisScale2");
	mAvatarAxisScale[3] = gSavedSettings.getF32("AvatarAxisScale3");
	mAvatarAxisScale[4] = gSavedSettings.getF32("AvatarAxisScale4");
	mAvatarAxisScale[5] = gSavedSettings.getF32("AvatarAxisScale5");

	mBuildAxisScale[0] = gSavedSettings.getF32("BuildAxisScale0");
	mBuildAxisScale[1] = gSavedSettings.getF32("BuildAxisScale1");
	mBuildAxisScale[2] = gSavedSettings.getF32("BuildAxisScale2");
	mBuildAxisScale[3] = gSavedSettings.getF32("BuildAxisScale3");
	mBuildAxisScale[4] = gSavedSettings.getF32("BuildAxisScale4");
	mBuildAxisScale[5] = gSavedSettings.getF32("BuildAxisScale5");

	mFlycamAxisScale[0] = gSavedSettings.getF32("FlycamAxisScale0");
	mFlycamAxisScale[1] = gSavedSettings.getF32("FlycamAxisScale1");
	mFlycamAxisScale[2] = gSavedSettings.getF32("FlycamAxisScale2");
	mFlycamAxisScale[3] = gSavedSettings.getF32("FlycamAxisScale3");
	mFlycamAxisScale[4] = gSavedSettings.getF32("FlycamAxisScale4");
	mFlycamAxisScale[5] = gSavedSettings.getF32("FlycamAxisScale5");
	mFlycamAxisScale[6] = gSavedSettings.getF32("FlycamAxisScale6");

	mAvatarAxisDeadZone[0] = gSavedSettings.getF32("AvatarAxisDeadZone0");
	mAvatarAxisDeadZone[1] = gSavedSettings.getF32("AvatarAxisDeadZone1");
	mAvatarAxisDeadZone[2] = gSavedSettings.getF32("AvatarAxisDeadZone2");
	mAvatarAxisDeadZone[3] = gSavedSettings.getF32("AvatarAxisDeadZone3");
	mAvatarAxisDeadZone[4] = gSavedSettings.getF32("AvatarAxisDeadZone4");
	mAvatarAxisDeadZone[5] = gSavedSettings.getF32("AvatarAxisDeadZone5");

	mBuildAxisDeadZone[0] = gSavedSettings.getF32("BuildAxisDeadZone0");
	mBuildAxisDeadZone[1] = gSavedSettings.getF32("BuildAxisDeadZone1");
	mBuildAxisDeadZone[2] = gSavedSettings.getF32("BuildAxisDeadZone2");
	mBuildAxisDeadZone[3] = gSavedSettings.getF32("BuildAxisDeadZone3");
	mBuildAxisDeadZone[4] = gSavedSettings.getF32("BuildAxisDeadZone4");
	mBuildAxisDeadZone[5] = gSavedSettings.getF32("BuildAxisDeadZone5");

	mFlycamAxisDeadZone[0] = gSavedSettings.getF32("FlycamAxisDeadZone0");
	mFlycamAxisDeadZone[1] = gSavedSettings.getF32("FlycamAxisDeadZone1");
	mFlycamAxisDeadZone[2] = gSavedSettings.getF32("FlycamAxisDeadZone2");
	mFlycamAxisDeadZone[3] = gSavedSettings.getF32("FlycamAxisDeadZone3");
	mFlycamAxisDeadZone[4] = gSavedSettings.getF32("FlycamAxisDeadZone4");
	mFlycamAxisDeadZone[5] = gSavedSettings.getF32("FlycamAxisDeadZone5");
	mFlycamAxisDeadZone[6] = gSavedSettings.getF32("FlycamAxisDeadZone6");

	mAvatarFeathering = gSavedSettings.getF32("AvatarFeathering");
	mBuildFeathering = gSavedSettings.getF32("BuildFeathering");
	mFlycamFeathering = gSavedSettings.getF32("FlycamFeathering");
	mRunThreshold = gSavedSettings.getF32("JoystickRunThreshold");
}

void LLFloaterJoystick::cancel()
{
	gSavedSettings.setBool("JoystickEnabled", mJoystickEnabled);

	gSavedSettings.setS32("JoystickAxis0", mJoystickAxis[0]);
	gSavedSettings.setS32("JoystickAxis1", mJoystickAxis[1]);
	gSavedSettings.setS32("JoystickAxis2", mJoystickAxis[2]);
	gSavedSettings.setS32("JoystickAxis3", mJoystickAxis[3]);
	gSavedSettings.setS32("JoystickAxis4", mJoystickAxis[4]);
	gSavedSettings.setS32("JoystickAxis5", mJoystickAxis[5]);
	gSavedSettings.setS32("JoystickAxis6", mJoystickAxis[6]);

	gSavedSettings.setS32("JoystickButtonFlyCam", mJoystickButtonFlyCam);
	gSavedSettings.setS32("JoystickButtonJump", mJoystickButtonJump);

	gSavedSettings.setBool("Cursor3D", m3DCursor);
	gSavedSettings.setBool("AutoLeveling", mAutoLeveling);
	gSavedSettings.setBool("ZoomDirect", mZoomDirect );

	gSavedSettings.setBool("JoystickAvatarEnabled", mAvatarEnabled);
	gSavedSettings.setBool("JoystickBuildEnabled", mBuildEnabled);
	gSavedSettings.setBool("JoystickFlycamEnabled", mFlycamEnabled);

	gSavedSettings.setF32("AvatarAxisScale0", mAvatarAxisScale[0]);
	gSavedSettings.setF32("AvatarAxisScale1", mAvatarAxisScale[1]);
	gSavedSettings.setF32("AvatarAxisScale2", mAvatarAxisScale[2]);
	gSavedSettings.setF32("AvatarAxisScale3", mAvatarAxisScale[3]);
	gSavedSettings.setF32("AvatarAxisScale4", mAvatarAxisScale[4]);
	gSavedSettings.setF32("AvatarAxisScale5", mAvatarAxisScale[5]);

	gSavedSettings.setF32("BuildAxisScale0", mBuildAxisScale[0]);
	gSavedSettings.setF32("BuildAxisScale1", mBuildAxisScale[1]);
	gSavedSettings.setF32("BuildAxisScale2", mBuildAxisScale[2]);
	gSavedSettings.setF32("BuildAxisScale3", mBuildAxisScale[3]);
	gSavedSettings.setF32("BuildAxisScale4", mBuildAxisScale[4]);
	gSavedSettings.setF32("BuildAxisScale5", mBuildAxisScale[5]);

	gSavedSettings.setF32("FlycamAxisScale0", mFlycamAxisScale[0]);
	gSavedSettings.setF32("FlycamAxisScale1", mFlycamAxisScale[1]);
	gSavedSettings.setF32("FlycamAxisScale2", mFlycamAxisScale[2]);
	gSavedSettings.setF32("FlycamAxisScale3", mFlycamAxisScale[3]);
	gSavedSettings.setF32("FlycamAxisScale4", mFlycamAxisScale[4]);
	gSavedSettings.setF32("FlycamAxisScale5", mFlycamAxisScale[5]);
	gSavedSettings.setF32("FlycamAxisScale6", mFlycamAxisScale[6]);

	gSavedSettings.setF32("AvatarAxisDeadZone0", mAvatarAxisDeadZone[0]);
	gSavedSettings.setF32("AvatarAxisDeadZone1", mAvatarAxisDeadZone[1]);
	gSavedSettings.setF32("AvatarAxisDeadZone2", mAvatarAxisDeadZone[2]);
	gSavedSettings.setF32("AvatarAxisDeadZone3", mAvatarAxisDeadZone[3]);
	gSavedSettings.setF32("AvatarAxisDeadZone4", mAvatarAxisDeadZone[4]);
	gSavedSettings.setF32("AvatarAxisDeadZone5", mAvatarAxisDeadZone[5]);

	gSavedSettings.setF32("BuildAxisDeadZone0", mBuildAxisDeadZone[0]);
	gSavedSettings.setF32("BuildAxisDeadZone1", mBuildAxisDeadZone[1]);
	gSavedSettings.setF32("BuildAxisDeadZone2", mBuildAxisDeadZone[2]);
	gSavedSettings.setF32("BuildAxisDeadZone3", mBuildAxisDeadZone[3]);
	gSavedSettings.setF32("BuildAxisDeadZone4", mBuildAxisDeadZone[4]);
	gSavedSettings.setF32("BuildAxisDeadZone5", mBuildAxisDeadZone[5]);

	gSavedSettings.setF32("FlycamAxisDeadZone0", mFlycamAxisDeadZone[0]);
	gSavedSettings.setF32("FlycamAxisDeadZone1", mFlycamAxisDeadZone[1]);
	gSavedSettings.setF32("FlycamAxisDeadZone2", mFlycamAxisDeadZone[2]);
	gSavedSettings.setF32("FlycamAxisDeadZone3", mFlycamAxisDeadZone[3]);
	gSavedSettings.setF32("FlycamAxisDeadZone4", mFlycamAxisDeadZone[4]);
	gSavedSettings.setF32("FlycamAxisDeadZone5", mFlycamAxisDeadZone[5]);
	gSavedSettings.setF32("FlycamAxisDeadZone6", mFlycamAxisDeadZone[6]);

	gSavedSettings.setF32("AvatarFeathering", mAvatarFeathering);
	gSavedSettings.setF32("BuildFeathering", mBuildFeathering);
	gSavedSettings.setF32("FlycamFeathering", mFlycamFeathering);
	gSavedSettings.setF32("JoystickRunThreshold", mRunThreshold);
}

//static
void LLFloaterJoystick::onCommitJoystickEnabled(LLUICtrl*, void* userdata)
{
	LLFloaterJoystick* self = (LLFloaterJoystick*)userdata;
	if (!self) return;

	bool joystick_enabled = self->mCheckJoystickEnabled->get();
	bool flycam_enabled = self->mCheckFlycamEnabled->get();
	if (!joystick_enabled || !flycam_enabled)
	{
		// Turn off flycam
		LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
		if (joystick->getOverrideCamera())
		{
			joystick->toggleFlycam();
		}
	}
}

//static
void LLFloaterJoystick::onClickInit(void*)
{
	if (!LLViewerJoystick::getInstance()->isJoystickInitialized())
	{
		LLViewerJoystick::getInstance()->init(true);
	}
}

//static
void LLFloaterJoystick::onClickCancel(void* userdata)
{
	LLFloaterJoystick* self = (LLFloaterJoystick*)userdata;
	if (self)
	{
		self->cancel();
		self->close();
	}
}

//static
void LLFloaterJoystick::onClickOK(void* userdata)
{
	LLFloaterJoystick* self = (LLFloaterJoystick*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterJoystick::onClickRestoreDefaults(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterJoystick* self = (LLFloaterJoystick*)userdata;
	if (!self || !ctrl)
	{
		return;
	}

	std::string setting = ctrl->getValue().asString();
	if (setting == "previous")
	{
		self->cancel();
	}
	else if (setting == "spacenavigator")
	{
		LLViewerJoystick::getInstance()->setSNDefaults();
	}
	else
	{
		LLViewerJoystick::getInstance()->setToDefaults();
	}
}
