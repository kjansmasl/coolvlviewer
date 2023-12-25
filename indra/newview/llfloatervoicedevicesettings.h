/** 
 * @file llpanelvoicedevicesettings.h
 * @author Richard Nelson
 * @brief Voice communication set-up wizard
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

#ifndef LL_LLFLOATERVOICEDEVICESETTINGS_H
#define LL_LLFLOATERVOICEDEVICESETTINGS_H

#include "llfloater.h"

class LLColor4;
class LLComboBox;
class LLPrefsVoiceLogic;

class LLPanelVoiceDeviceSettings final : public LLPanel
{
public:
	LLPanelVoiceDeviceSettings();

	void draw() override;
	bool postBuild() override;
	void refresh() override;
	void onOpen();
	void onClose(bool app_quitting);

	void apply();
	void cancel();

protected:
	static void onCommitInputDevice(LLUICtrl* ctrl, void* user_data);
	static void onCommitOutputDevice(LLUICtrl* ctrl, void* user_data);

protected:
	LLComboBox*	mCtrlInputDevices;
	LLComboBox*	mCtrlOutputDevices;
	std::string	mInputDevice;
	std::string	mOutputDevice;
	LLColor4	mSpeakingColor;
	LLColor4	mOverdrivenColor;
	F32			mMicVolume;
	bool		mDevicesUpdated;
};

class LLFloaterVoiceDeviceSettings final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterVoiceDeviceSettings>
{
public:
	LLFloaterVoiceDeviceSettings(const LLSD& seed);

	void onOpen() override;
	void onClose(bool app_quitting) override;
	void draw() override;

	void apply();
	void cancel();

protected:
	static void* createPanelVoiceDeviceSettings(void* user_data);

protected:
	LLPanelVoiceDeviceSettings* mDevicePanel;
};

#endif // LL_LLFLOATERVOICEDEVICESETTINGS_H
