/**
 * @file llfloatervoicedevicesettings.cpp
 * @author Richard Nelson
 * @brief Voice communication set-up
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

#include "llfloatervoicedevicesettings.h"

#include "llbutton.h"
#include "llcombobox.h"
#include "llsliderctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llprefsvoice.h"
#include "llviewercontrol.h"
#include "llvoicechannel.h"
#include "llvoiceclient.h"

LLPanelVoiceDeviceSettings::LLPanelVoiceDeviceSettings()
{
	mCtrlInputDevices = NULL;
	mCtrlOutputDevices = NULL;
	mInputDevice = gSavedSettings.getString("VoiceInputAudioDevice");
	mOutputDevice = gSavedSettings.getString("VoiceOutputAudioDevice");
	mDevicesUpdated = false;

	// grab "live" mic volume level
	mMicVolume = gSavedSettings.getF32("AudioLevelMic");

	mSpeakingColor = gSavedSettings.getColor4("SpeakingColor");
	mOverdrivenColor = gSavedSettings.getColor4("OverdrivenColor");
	// ask for new device enumeration
	// now do this in onOpen() instead...
	//gVoiceClient.refreshDeviceLists();
}

bool LLPanelVoiceDeviceSettings::postBuild()
{
	LLSlider* volume_slider = getChild<LLSlider>("mic_volume_slider");
	// set mic volume tuning slider based on last mic volume setting
	volume_slider->setValue(mMicVolume);

	childSetCommitCallback("voice_input_device", onCommitInputDevice, this);
	childSetCommitCallback("voice_output_device", onCommitOutputDevice, this);

	return true;
}

void LLPanelVoiceDeviceSettings::draw()
{
	// let user know that volume indicator is not yet available
	bool is_in_tuning_mode = gVoiceClient.inTuningMode();
	childSetVisible("wait_text", !is_in_tuning_mode);

	LLPanel::draw();

	F32 voice_power = gVoiceClient.tuningGetEnergy();
	S32 discrete_power = 0;

	if (!is_in_tuning_mode)
	{
		discrete_power = 0;
	}
	else
	{
		discrete_power =
			llmin(4, S32(voice_power * 4.f / OVERDRIVEN_POWER_LEVEL));
	}

	if (is_in_tuning_mode)
	{
		for (S32 power_bar_idx = 0; power_bar_idx < 5; power_bar_idx++)
		{
			std::string view_name = llformat("%s%d", "bar", power_bar_idx);
			LLView* bar_view = getChild<LLView>(view_name.c_str());
			if (bar_view)
			{
				if (power_bar_idx < discrete_power)
				{
					LLColor4 color = power_bar_idx >= 3 ? mOverdrivenColor
														: mSpeakingColor;
					gl_rect_2d(bar_view->getRect(), color, true);
				}
				gl_rect_2d(bar_view->getRect(), LLColor4::grey, false);
			}
		}
	}
}

void LLPanelVoiceDeviceSettings::apply()
{
	std::string s;
	if (mCtrlInputDevices)
	{
		s = mCtrlInputDevices->getSimple();
		gSavedSettings.setString("VoiceInputAudioDevice", s);
		mInputDevice = s;
	}

	if (mCtrlOutputDevices)
	{
		s = mCtrlOutputDevices->getSimple();
		gSavedSettings.setString("VoiceOutputAudioDevice", s);
		mOutputDevice = s;
	}

	// Assume we are being destroyed by closing our embedding window
	LLSlider* volume_slider = getChild<LLSlider>("mic_volume_slider");
	if (volume_slider)
	{
		F32 slider_value = (F32)volume_slider->getValue().asReal();
		gSavedSettings.setF32("AudioLevelMic", slider_value);
		mMicVolume = slider_value;
	}
}

void LLPanelVoiceDeviceSettings::cancel()
{
	gSavedSettings.setString("VoiceInputAudioDevice", mInputDevice);
	gSavedSettings.setString("VoiceOutputAudioDevice", mOutputDevice);

	if (mCtrlInputDevices)
	{
		mCtrlInputDevices->setSimple(mInputDevice);
	}

	if (mCtrlOutputDevices)
	{
		mCtrlOutputDevices->setSimple(mOutputDevice);
	}

	gSavedSettings.setF32("AudioLevelMic", mMicVolume);
	LLSlider* volume_slider = getChild<LLSlider>("mic_volume_slider");
	if (volume_slider)
	{
		volume_slider->setValue(mMicVolume);
	}
}

void LLPanelVoiceDeviceSettings::refresh()
{
	//grab current volume
	LLSlider* volume_slider = getChild<LLSlider>("mic_volume_slider");
	// set mic volume tuning slider based on last mic volume setting
	F32 current_volume = (F32)volume_slider->getValue().asReal();
	gVoiceClient.tuningSetMicVolume(current_volume);

	// Fill in popup menus
	mCtrlInputDevices = getChild<LLComboBox>("voice_input_device");
	mCtrlOutputDevices = getChild<LLComboBox>("voice_output_device");

	if (!gVoiceClient.deviceSettingsAvailable())
	{
		// The combo boxes are disabled, since we can't get the device settings
		// from the daemon just now. Put the currently set default (ONLY) in
		// the box, and select it.
		if (mCtrlInputDevices)
		{
			mCtrlInputDevices->removeall();
			mCtrlInputDevices->add(mInputDevice, ADD_BOTTOM);
			mCtrlInputDevices->setSimple(mInputDevice);
		}
		if (mCtrlOutputDevices)
		{
			mCtrlOutputDevices->removeall();
			mCtrlOutputDevices->add(mOutputDevice, ADD_BOTTOM);
			mCtrlOutputDevices->setSimple(mOutputDevice);
		}
	}
	else if (!mDevicesUpdated)
	{
		LLVoiceClient::device_list_t* devices;
		LLVoiceClient::device_list_t::iterator iter;

		if (mCtrlInputDevices)
		{
			mCtrlInputDevices->removeall();
			mCtrlInputDevices->add(getString("default_text"), ADD_BOTTOM);

			devices = gVoiceClient.getCaptureDevices();
			for (iter = devices->begin(); iter != devices->end(); ++iter)
			{
				mCtrlInputDevices->add(*iter, ADD_BOTTOM);
			}

			if (!mCtrlInputDevices->setSimple(mInputDevice))
			{
				mCtrlInputDevices->setSimple(getString("default_text"));
			}
		}

		if (mCtrlOutputDevices)
		{
			mCtrlOutputDevices->removeall();
			mCtrlOutputDevices->add(getString("default_text"), ADD_BOTTOM);

			devices = gVoiceClient.getRenderDevices();
			for (iter = devices->begin(); iter != devices->end(); ++iter)
			{
				mCtrlOutputDevices->add(*iter, ADD_BOTTOM);
			}

			if (!mCtrlOutputDevices->setSimple(mOutputDevice))
			{
				mCtrlOutputDevices->setSimple(getString("default_text"));
			}
		}
		mDevicesUpdated = true;
	}
}

void LLPanelVoiceDeviceSettings::onOpen()
{
	mInputDevice = gSavedSettings.getString("VoiceInputAudioDevice");
	mOutputDevice = gSavedSettings.getString("VoiceOutputAudioDevice");
	mMicVolume = gSavedSettings.getF32("AudioLevelMic");
	mDevicesUpdated = false;

	// ask for new device enumeration
	gVoiceClient.refreshDeviceLists();

	// put voice client in "tuning" mode
	gVoiceClient.tuningStart();
	LLVoiceChannel::suspend();
}

void LLPanelVoiceDeviceSettings::onClose(bool app_quitting)
{
	gVoiceClient.tuningStop();
	LLVoiceChannel::resume();
}

// static
void LLPanelVoiceDeviceSettings::onCommitInputDevice(LLUICtrl* ctrl, void* user_data)
{
	gVoiceClient.setCaptureDevice(ctrl->getValue().asString());
}

// static
void LLPanelVoiceDeviceSettings::onCommitOutputDevice(LLUICtrl* ctrl, void* user_data)
{
	gVoiceClient.setRenderDevice(ctrl->getValue().asString());
}

//
// LLFloaterVoiceDeviceSettings
//

LLFloaterVoiceDeviceSettings::LLFloaterVoiceDeviceSettings(const LLSD& seed)
:	LLFloater("voice settings"),
	mDevicePanel(NULL)
{
	mFactoryMap["device_settings"] = LLCallbackMap(createPanelVoiceDeviceSettings, this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_device_settings.xml",
												 &mFactoryMap,
												 // do not automatically open
												 // singleton floaters (as
												 // result of getInstance())
												 false);
	center();
}

void LLFloaterVoiceDeviceSettings::onOpen()
{
	if (mDevicePanel)
	{
		mDevicePanel->onOpen();
	}

	LLFloater::onOpen();
}

void LLFloaterVoiceDeviceSettings::onClose(bool app_quitting)
{
	if (mDevicePanel)
	{
		mDevicePanel->onClose(app_quitting);
	}

	setVisible(false);
}

void LLFloaterVoiceDeviceSettings::apply()
{
	if (mDevicePanel)
	{
		mDevicePanel->apply();
	}
}

void LLFloaterVoiceDeviceSettings::cancel()
{
	if (mDevicePanel)
	{
		mDevicePanel->cancel();
	}
}

void LLFloaterVoiceDeviceSettings::draw()
{
	if (mDevicePanel)
	{
		mDevicePanel->refresh();
	}
	LLFloater::draw();
}

// static
void* LLFloaterVoiceDeviceSettings::createPanelVoiceDeviceSettings(void* user_data)
{
	LLFloaterVoiceDeviceSettings* floaterp = (LLFloaterVoiceDeviceSettings*)user_data;
	floaterp->mDevicePanel = new LLPanelVoiceDeviceSettings();
	return floaterp->mDevicePanel;
}
