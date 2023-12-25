/**
 * @file llfloaterjoystick.h
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

#ifndef LL_LLFLOATERJOYSTICK_H
#define LL_LLFLOATERJOYSTICK_H

#include "llfloater.h"

class LLButton;
class LLCheckBoxCtrl;
class LLTextBox;
class LLStat;
class LLStatBar;
class LLStatView;

class LLFloaterJoystick final : public LLFloater,
								public LLFloaterSingleton<LLFloaterJoystick>
{
public:
	LLFloaterJoystick(const LLSD& data);

	bool postBuild() override;
	void refresh() override;
	void draw() override;

private:
	void cancel();	// Cancel the changed values.

	static void onCommitJoystickEnabled(LLUICtrl*, void* userdata);
	static void onClickInit(void*);
	static void onClickCancel(void* userdata);
	static void onClickOK(void* userdata);
	static void onClickRestoreDefaults(LLUICtrl* ctrl, void* userdata);

private:
	LLCheckBoxCtrl*	mCheckJoystickEnabled;
	LLCheckBoxCtrl*	mCheckFlycamEnabled;

	LLButton*		mInitButon;
	LLTextBox*		mJoystickType;
	LLTextBox*		mJoystickButtons[16];

	LLStatView*		mAxisStatsView;
	LLStat*			mAxisStats[6];
	LLStatBar*		mAxisStatsBar[6];

	F32				mAvatarAxisScale[6];
	F32				mBuildAxisScale[6];
	F32				mFlycamAxisScale[7];
	F32				mAvatarAxisDeadZone[6];
	F32				mBuildAxisDeadZone[6];
	F32				mFlycamAxisDeadZone[7];
	F32				mAvatarFeathering;
	F32				mBuildFeathering;
	F32				mFlycamFeathering;
	F32				mRunThreshold;

	S32				mJoystickAxis[7];
	S32				mJoystickButtonFlyCam;
	S32				mJoystickButtonJump;

	bool			mAvatarEnabled;
	bool			mBuildEnabled;
	bool			mFlycamEnabled;

	bool			mJoystickEnabled;
	bool			m3DCursor;
	bool			mAutoLeveling;
	bool			mZoomDirect;
};

#endif
