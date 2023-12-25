/**
 * @file llfloaterwindlight.cpp
 * @brief LLFloaterWindlight class implementation
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

#include "boost/tokenizer.hpp"

#include "llfloaterwindlight.h"

#include "imageids.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llmultisliderctrl.h"
#include "llnotifications.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lluictrlfactory.h"

#include "llcolorswatch.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "lltexturectrl.h"
#include "llviewercontrol.h"
#include "llwlskyparammgr.h"
#include "llwlwaterparammgr.h"

constexpr F32 HOUR_PER_DAY = 24.f;

///////////////////////////////////////////////////////////////////////////////
// LLPanelWLDayCycle class (used to be a separate floater in LL's viewer)
///////////////////////////////////////////////////////////////////////////////

// Convenience structure for holding keys mapped to sliders
struct LLWLSkyKey
{
public:
	std::string	mPresetName;
	F32			mTime;
};

class LLPanelWLDayCycle final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelWLDayCycle);

public:
	LLPanelWLDayCycle(const std::string& name)
	:	LLPanel(name)
	{
	}

	~LLPanelWLDayCycle() override
	{
	}

	bool postBuild() override;
	void refresh() override;

	// Adds a new preset to the combo
	void addPreset(const std::string& name);

	// Deletes any and all reference to a preset in keys
	void deletePreset(const std::string& name);

private:
	static LLPanelWLDayCycle* getSelf();

	void refreshNames(std::string new_selection = LLStringUtil::null);

	// Adds a slider to the track
	void addSliderKey(F32 time, const std::string& name);

	// Makes sure key slider has what is in day cycle
	void syncSliderTrack();

	// Makes sure day cycle data structure has what is in menu
	void syncTrack();

	static void onClickHelp(void* userdata);
	static void onTimeSliderMoved(LLUICtrl* ctrl, void* userdata);
	static void onKeyTimeMoved(LLUICtrl* ctrl, void* userdata);
	static void onKeyTimeChanged(LLUICtrl* ctrl, void* userdata);
	static void onKeyPresetChanged(LLUICtrl* ctrl, void* userdata);
	static void onRunAnimSky(void* userdata);
	static void onStopAnimSky(void*);
	static void onTimeRateChanged(LLUICtrl* ctrl, void* userdata);
	static void onAddKey(void* userdata);
	static void onDeleteKey(void* userdata);
	static void onNewDayCycle(void* userdata);
	static void onSaveDayCycle(void* userdata);
	static void onDeleteDayCycle(void* userdata);
	static void onChangeDayCycle(LLUICtrl* ctrl, void* userdata);

	static bool newPromptCallback(const LLSD& notification,
								  const LLSD& response);
	static bool saveAlertCallback(const LLSD& notification,
								  const LLSD& response);
	static bool deleteAlertCallback(const LLSD& notification,
									const LLSD& response);

private:
	LLComboBox*			mWLDayCycleCombo;
	LLComboBox*			mWLKeyPresets;
	LLMultiSliderCtrl*	mWLTimeSlider;
	LLMultiSliderCtrl*	mWLDayCycleKeys;
	LLSpinCtrl*			mWLCurKeyHour;
	LLSpinCtrl*			mWLCurKeyMin;
	LLSpinCtrl*			mWLLengthOfDayHour;
	LLSpinCtrl*			mWLLengthOfDayMin;
	LLSpinCtrl*			mWLLengthOfDaySec;
	LLButton*			mPlayBtn;
	LLButton*			mStopBtn;
	LLTextBox*			mNoLivePreviewText;

	// Map of sliders to parameters
	typedef std::map<std::string, LLWLSkyKey> skykey_map_t;
	static skykey_map_t	sSliderToKey;
};

LLPanelWLDayCycle::skykey_map_t LLPanelWLDayCycle::sSliderToKey;

//static
LLPanelWLDayCycle* LLPanelWLDayCycle::getSelf()
{
	LLFloaterWindlight* dcp = LLFloaterWindlight::findInstance();
	return dcp ? dcp->mPanelDayCycle : NULL;
}

//virtual
bool LLPanelWLDayCycle::postBuild()
{
	mWLDayCycleCombo = getChild<LLComboBox>("WLDayCycleCombo");
	refreshNames();
	mWLDayCycleCombo->setCommitCallback(onChangeDayCycle);
	mWLDayCycleCombo->setCallbackUserData(this);

	mWLKeyPresets = getChild<LLComboBox>("WLKeyPresets");
	for (LLWLSkyParamMgr::paramset_map_t::iterator
			it = gWLSkyParamMgr.mParamList.begin(),
			end = gWLSkyParamMgr.mParamList.end();
		 it != end; ++it)
	{
		const std::string& name = it->first;
		if (name != "current parcel environment")
		{
			mWLKeyPresets->add(name);
		}
	}
	mWLKeyPresets->selectFirstItem();
	mWLKeyPresets->setCommitCallback(onKeyPresetChanged);
	mWLKeyPresets->setCallbackUserData(this);

	mWLTimeSlider = getChild<LLMultiSliderCtrl>("WLTimeSlider");
	mWLTimeSlider->addSlider();
	mWLTimeSlider->setCommitCallback(onTimeSliderMoved);
	mWLTimeSlider->setCallbackUserData(this);

	mWLDayCycleKeys = getChild<LLMultiSliderCtrl>("WLDayCycleKeys");
	mWLDayCycleKeys->setCommitCallback(onKeyTimeMoved);
	mWLDayCycleKeys->setCallbackUserData(this);

	mWLCurKeyHour = getChild<LLSpinCtrl>("WLCurKeyHour");
	mWLCurKeyHour->setCommitCallback(onKeyTimeChanged);
	mWLCurKeyHour->setCallbackUserData(this);

	mWLCurKeyMin = getChild<LLSpinCtrl>("WLCurKeyMin");
	mWLCurKeyMin->setCommitCallback(onKeyTimeChanged);
	mWLCurKeyMin->setCallbackUserData(this);

	mWLLengthOfDayHour = getChild<LLSpinCtrl>("WLLengthOfDayHour");
	mWLLengthOfDayHour->setCommitCallback(onTimeRateChanged);
	mWLLengthOfDayHour->setCallbackUserData(this);

	mWLLengthOfDayMin = getChild<LLSpinCtrl>("WLLengthOfDayMin");
	mWLLengthOfDayMin->setCommitCallback(onTimeRateChanged);
	mWLLengthOfDayMin->setCallbackUserData(this);

	mWLLengthOfDaySec = getChild<LLSpinCtrl>("WLLengthOfDaySec");
	mWLLengthOfDaySec->setCommitCallback(onTimeRateChanged);
	mWLLengthOfDaySec->setCallbackUserData(this);

	mPlayBtn = getChild<LLButton>("WLAnimSky");
	mPlayBtn->setClickedCallback(onRunAnimSky, this);

	mStopBtn = getChild<LLButton>("WLStopAnimSky");
	mStopBtn->setClickedCallback(onStopAnimSky, this);

	mNoLivePreviewText = getChild<LLTextBox>("no_live_preview_text");

	childSetAction("WLNewDayCycle", onNewDayCycle, this);
	childSetAction("WLSaveDayCycle", onSaveDayCycle, this);
	childSetAction("WLDeleteDayCycle", onDeleteDayCycle, this);

	childSetAction("WLAddKey", onAddKey, this);
	childSetAction("WLDeleteKey", onDeleteKey, this);

	childSetAction("WLDayCycleHelp", onClickHelp, this);

	refresh();
	syncSliderTrack();

	return true;
}

//virtual
void LLPanelWLDayCycle::refresh()
{
	// Set time
	mWLTimeSlider->setCurSliderValue((F32)gWLSkyParamMgr.mAnimator.getDayTime() *
									 HOUR_PER_DAY);

	// Get the current rate
	F32 seconds = gWLSkyParamMgr.mDay.mDayLenth;
	F32 hours = (F32)((S32)(seconds / 3600.f));
	seconds -= hours * 3600.f;
	F32 min = (F32)((S32)(seconds / 60));
	seconds -= min * 60.f;

	mWLLengthOfDayHour->setValue(hours);
	mWLLengthOfDayMin->setValue(min);
	mWLLengthOfDaySec->setValue(seconds);

	// Preview. *TODO: no more available: remove !
	mPlayBtn->setEnabled(false);
	mStopBtn->setEnabled(false);
	mNoLivePreviewText->setVisible(false);
	onStopAnimSky(this);
}

void LLPanelWLDayCycle::refreshNames(std::string new_selection)
{
	// Refresh the available day cycles presets list
	LLWLDayCycle::findPresets();

	mWLDayCycleCombo->removeall();

	for (LLWLDayCycle::names_list_t::iterator
			it = LLWLDayCycle::sPresetNames.begin(),
			end = LLWLDayCycle::sPresetNames.end();
		 it != end; ++it)
	{
		mWLDayCycleCombo->add(*it);
	}

	// Set (possibly new) selected entry in combo box
	if (new_selection.empty())
	{
		new_selection = "Default";
	}
	mWLDayCycleCombo->selectByValue(LLSD(new_selection));
}

void LLPanelWLDayCycle::addPreset(const std::string& name)
{
	mWLKeyPresets->add(name);
	mWLKeyPresets->sortByName();
}

void LLPanelWLDayCycle::deletePreset(const std::string& name)
{
	// Remove from combo
	mWLKeyPresets->remove(name);

	// Delete any reference
	for (skykey_map_t::iterator it = sSliderToKey.begin(),
								end = sSliderToKey.end();
		 it != end; )
	{
		skykey_map_t::iterator cur = it++;
		if (cur->second.mPresetName == name)
		{
			mWLDayCycleKeys->deleteSlider(cur->first);
			sSliderToKey.erase(cur);
		}
	}
}

void LLPanelWLDayCycle::syncSliderTrack()
{
	// Clear the slider
	mWLDayCycleKeys->clear();
	sSliderToKey.clear();

	// Add sliders
	for (std::map<F32, std::string>::iterator
			it = gWLSkyParamMgr.mDay.mTimeMap.begin(),
			end = gWLSkyParamMgr.mDay.mTimeMap.end();
		 it != end; ++it)
	{
		addSliderKey(it->first * HOUR_PER_DAY, it->second);
	}
}

void LLPanelWLDayCycle::syncTrack()
{
	// If no keys, do nothing
	if (!sSliderToKey.size())
	{
		return;
	}

	// Create a new animation track
	gWLSkyParamMgr.mDay.clearKeys();

	// Add the keys one by one
	for (skykey_map_t::iterator it = sSliderToKey.begin(),
								end = sSliderToKey.end();
		 it != end; ++it)
	{
		gWLSkyParamMgr.mDay.addKey(it->second.mTime / HOUR_PER_DAY,
								   it->second.mPresetName);
	}

	// Set the param manager's track to the new one
	gWLSkyParamMgr.resetAnimator(mWLTimeSlider->getCurSliderValue() /
								 HOUR_PER_DAY,
								 false);
	gWLSkyParamMgr.mAnimator.update(gWLSkyParamMgr.mCurParams);
}

//static
void LLPanelWLDayCycle::onClickHelp(void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (self)
	{
		LLFloater* parent = self->getParentFloater();
		if (parent)
		{
			gNotifications.add(parent->contextualNotification("HelpDayCycle"));
		}
	}
}

//static
void LLPanelWLDayCycle::addSliderKey(F32 time, const std::string& name)
{
	// Make a slider
	const std::string& slider_name = mWLDayCycleKeys->addSlider(time);
	if (slider_name.empty())
	{
		return;
	}

	// Set the key
	LLWLSkyKey new_key;
	new_key.mPresetName = name;
	new_key.mTime = mWLDayCycleKeys->getCurSliderValue();

	// Add to map
	sSliderToKey[slider_name] = new_key;
}

//static
void LLPanelWLDayCycle::onRunAnimSky(void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self || sSliderToKey.size() == 0)
	{
		return;
	}

	// Turn off estate time
	gSavedSettings.setBool("UseWLEstateTime", false);

	// Set the param manager's track to the new one
	gWLSkyParamMgr.resetAnimator(self->mWLTimeSlider->getCurSliderValue() /
								 HOUR_PER_DAY,
								 true);
}

//static
void LLPanelWLDayCycle::onStopAnimSky(void*)
{
	// If no keys, do nothing
	if (sSliderToKey.size() > 0)
	{
		// Turn off animation and using linden time
		gWLSkyParamMgr.animate(false);
	}
}

//static
void LLPanelWLDayCycle::onChangeDayCycle(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	LLComboBox* combo = (LLComboBox*)ctrl;
	if (!self || !combo) return;

	std::string name = combo->getSelectedValue().asString();
	if (name.empty())
	{
		return;
	}

	gWLSkyParamMgr.mDay.loadDayCycle(name);

	// Sync it all up
	self->syncSliderTrack();
	self->refresh();

	// Set the param manager's track to the new one
	gWLSkyParamMgr.resetAnimator(self->mWLTimeSlider->getCurSliderValue() /
								 HOUR_PER_DAY,
								 false);
	// And draw it
	gWLSkyParamMgr.mAnimator.update(gWLSkyParamMgr.mCurParams);
}

//static
bool LLPanelWLDayCycle::newPromptCallback(const LLSD& notification,
										  const LLSD& response)
{
	LLPanelWLDayCycle* self = getSelf();
	if (!self || LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	std::string name = response["message"].asString();
	if (name.empty())
	{
		return false;
	}

	// Refresh the available day cycles presets list
	LLWLDayCycle::findPresets();
	if (LLWLDayCycle::sPresetNames.count(name))
	{
		// Otherwise, send a message to the user
		gNotifications.add("ExistsPresetAlert");
		return false;
	}

	gWLSkyParamMgr.mDay.saveDayCycle(name);

	self->refreshNames(name);
	// Sync it all up
	self->onChangeDayCycle(self->mWLDayCycleCombo, self);

	return false;
}

//static
void LLPanelWLDayCycle::onNewDayCycle(void* userdata)
{
	gNotifications.add("NewDayCycle", LLSD(), LLSD(), newPromptCallback);
}

//static
bool LLPanelWLDayCycle::saveAlertCallback(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		std::string name = notification["payload"]["name"].asString();
		gWLSkyParamMgr.mDay.saveDayCycle(name + ".xml");
	}
	return false;
}

//static
void LLPanelWLDayCycle::onSaveDayCycle(void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self) return;

	// Get the name
	std::string name = self->mWLDayCycleCombo->getSelectedItemLabel();
	if (!name.empty())	// Do not save with an empty name
	{
		LLSD payload;
		payload["name"] = name;
		gNotifications.add("WLSavePresetAlert", LLSD(), payload,
						   saveAlertCallback);
	}
}

//static
bool LLPanelWLDayCycle::deleteAlertCallback(const LLSD& notification,
											const LLSD& response)
{
	LLPanelWLDayCycle* self = getSelf();
	if (!self || LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	LLComboBox* combo = self->mWLDayCycleCombo;
	std::string name = combo->getSelectedValue().asString();
	if (name.empty())
	{
		return false;
	}

#if 0	// We do not care: only the user settings get deleted
	// Check to see if it is a default and should not be deleted
	if (name == "Default")
	{
		gNotifications.add("PresetNoEditDefault");
		return false;
	}
#endif

	LLWLDayCycle::removeDayCycle(name);

	self->refreshNames();
	// Sync it all up
	self->onChangeDayCycle(self->mWLDayCycleCombo, self);

	return false;
}

//static
void LLPanelWLDayCycle::onDeleteDayCycle(void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self) return;

	std::string name = self->mWLDayCycleCombo->getSelectedValue().asString();
	if (!name.empty())
	{
		LLSD args;
		args["NAME"] = name;
		gNotifications.add("WLDeletePresetAlert", args, LLSD(),
						   deleteAlertCallback);
	}
}

//static
void LLPanelWLDayCycle::onTimeSliderMoved(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self) return;

	F32 val = self->mWLTimeSlider->getCurSliderValue() / HOUR_PER_DAY;
	LLFloaterWindlight::setDayTime(val);
}

//static
void LLPanelWLDayCycle::onKeyTimeMoved(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self) return;

	LLComboBox* combo = self->mWLKeyPresets;
	LLMultiSliderCtrl* slider = self->mWLDayCycleKeys;

	if (slider->getValue().size() == 0)
	{
		return;
	}

	// Make sure we have a slider
	const std::string& cur_slider = slider->getCurSlider();
	if (cur_slider.empty())
	{
		return;
	}

	F32 time = slider->getCurSliderValue();

	// Check to see if a key exists
	std::string name = sSliderToKey[cur_slider].mPresetName;
	sSliderToKey[cur_slider].mTime = time;

	// If it exists, turn on check box
	combo->selectByValue(name);

	// Now set the spinners
	F32 hour = (F32)((S32)time);
	F32 min = (time - hour) * 60;
	// handle imprecision
	if (min >= 59.f)
	{
		min = 0.f;
		hour += 1.f;
	}

	self->mWLCurKeyHour->set(hour);
	self->mWLCurKeyMin->set(min);

	self->syncTrack();
}

//static
void LLPanelWLDayCycle::onKeyTimeChanged(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self) return;

	// If no keys, skipped
	if (sSliderToKey.size() == 0)
	{
		return;
	}

	LLMultiSliderCtrl* slider = self->mWLDayCycleKeys;

	F32 hour = self->mWLCurKeyHour->get();
	F32 min = self->mWLCurKeyMin->get();
	F32 val = hour + min / 60.0f;

	const std::string& cur_slider = slider->getCurSlider();
	slider->setCurSliderValue(val, true);
	F32 time = slider->getCurSliderValue() / HOUR_PER_DAY;

	// Now set the key's time in the sliderToKey map
	sSliderToKey[cur_slider].mTime = time;

	self->syncTrack();
}

//static
void LLPanelWLDayCycle::onKeyPresetChanged(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self) return;

	// Get the time
	LLComboBox* combo = self->mWLKeyPresets;
	LLMultiSliderCtrl* slider = self->mWLDayCycleKeys;

	// Do nothing if no sliders
	if (slider->getValue().size() == 0)
	{
		return;
	}

	// Change the map
	const std::string& cur_slider = slider->getCurSlider();
	// If empty, do not use
	if (cur_slider.empty())
	{
		return;
	}
	std::string name = combo->getSelectedValue().asString();
	sSliderToKey[cur_slider].mPresetName = name;

	self->syncTrack();
}

//static
void LLPanelWLDayCycle::onTimeRateChanged(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self) return;

	// Get the time
	F32 hour = self->mWLLengthOfDayHour->getValue().asReal();
	F32 min = self->mWLLengthOfDayMin->getValue().asReal();
	F32 sec = self->mWLLengthOfDaySec->getValue().asReal();
	F32 time = 3600.f * hour + 60.f * min + sec;
	if (time <= 0.f)
	{
		time = 1.f;
	}

	gWLSkyParamMgr.mDay.mDayLenth = time;

	self->syncTrack();
}

//static
void LLPanelWLDayCycle::onAddKey(void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self) return;

	// Get the values
	std::string name = self->mWLKeyPresets->getSelectedValue().asString();
	// Add the slider key
	self->addSliderKey(self->mWLTimeSlider->getCurSliderValue(), name);

	self->syncTrack();
}

//static
void LLPanelWLDayCycle::onDeleteKey(void* userdata)
{
	LLPanelWLDayCycle* self = (LLPanelWLDayCycle*)userdata;
	if (!self || sSliderToKey.size() == 0)
	{
		return;
	}

	LLMultiSliderCtrl* slider = self->mWLDayCycleKeys;

	// Delete from map
	const std::string& slider_name = slider->getCurSlider();
	skykey_map_t::iterator it = sSliderToKey.find(slider_name);
	if (it != sSliderToKey.end())
	{
		sSliderToKey.erase(it);
	}

	slider->deleteCurSlider();

	if (sSliderToKey.size() == 0)
	{
		return;
	}

	const std::string& name = slider->getCurSlider();
	self->mWLKeyPresets->selectByValue(sSliderToKey[name].mPresetName);
	F32 time = sSliderToKey[name].mTime;
	F32 hour = (F32)((S32)time);
	F32 min = (time - hour) / 60;

	// Now set the spinners
	self->mWLCurKeyHour->set(hour);
	self->mWLCurKeyMin->set(min);

	self->syncTrack();
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelWLSky class (used to be a separate floater in LL's viewer)
///////////////////////////////////////////////////////////////////////////////

class LLPanelWLSky final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelWLSky);

public:
	LLPanelWLSky(const std::string& name)
	:	LLPanel(name)
	{
	}

	~LLPanelWLSky() override
	{
	}

	bool postBuild() override;
	void refresh() override;

private:
	static LLPanelWLSky* getSelf();

	static void onColorControlRMoved(LLUICtrl* ctrl, void* userdata);
	static void onColorControlGMoved(LLUICtrl* ctrl, void* userdata);
	static void onColorControlBMoved(LLUICtrl* ctrl, void* userdata);
	static void onColorControlIMoved(LLUICtrl* ctrl, void* userdata);
	static void onFloatControlMoved(LLUICtrl* ctrl, void* userdata);
	static void onGlowRMoved(LLUICtrl* ctrl, void* userdata);
	static void onGlowBMoved(LLUICtrl* ctrl, void* userdata);
	static void onSunMoved(LLUICtrl* ctrl, void* userdata);
	static void onStarAlphaMoved(LLUICtrl* ctrl, void*);
	static void onCloudScrollXMoved(LLUICtrl* ctrl, void*);
	static void onCloudScrollYMoved(LLUICtrl* ctrl, void*);
	static void onCloudScrollXToggled(LLUICtrl* ctrl, void* userdata);
	static void onCloudScrollYToggled(LLUICtrl* ctrl, void* userdata);
	static void onNewPreset(void*);
	static void onSavePreset(void* userdata);
	static void onDeletePreset(void* userdata);
	static void onChangePresetName(LLUICtrl* ctrl, void* userdata);

	static bool newPromptCallback(const LLSD& notification,
								  const LLSD& response);
	static bool saveAlertCallback(const LLSD& notification,
								  const LLSD& response);
	static bool deleteAlertCallback(const LLSD& notification,
									const LLSD& response);

private:
	LLCheckBoxCtrl*			mWLCloudLockX;
	LLCheckBoxCtrl*			mWLCloudLockY;
	LLComboBox*				mWLPresetsCombo;
	LLSliderCtrl*			mWLBlueHorizonR;
	LLSliderCtrl*			mWLBlueHorizonG;
	LLSliderCtrl*			mWLBlueHorizonB;
	LLSliderCtrl*			mWLBlueHorizonI;
	LLSliderCtrl*			mWLHazeDensity;
	LLSliderCtrl*			mWLHazeHorizon;
	LLSliderCtrl*			mWLDensityMult;
	LLSliderCtrl*			mWLMaxAltitude;
	LLSliderCtrl*			mWLBlueDensityR;
	LLSliderCtrl*			mWLBlueDensityG;
	LLSliderCtrl*			mWLBlueDensityB;
	LLSliderCtrl*			mWLBlueDensityI;
	LLSliderCtrl*			mWLSunlightR;
	LLSliderCtrl*			mWLSunlightG;
	LLSliderCtrl*			mWLSunlightB;
	LLSliderCtrl*			mWLSunlightI;
	LLSliderCtrl*			mWLGlowR;
	LLSliderCtrl*			mWLGlowB;
	LLSliderCtrl*			mWLAmbientR;
	LLSliderCtrl*			mWLAmbientG;
	LLSliderCtrl*			mWLAmbientB;
	LLSliderCtrl*			mWLAmbientI;
	LLSliderCtrl*			mWLSunAngle;
	LLSliderCtrl*			mWLEastAngle;
	LLSliderCtrl*			mWLCloudColorR;
	LLSliderCtrl*			mWLCloudColorG;
	LLSliderCtrl*			mWLCloudColorB;
	LLSliderCtrl*			mWLCloudColorI;
	LLSliderCtrl*			mWLCloudX;
	LLSliderCtrl*			mWLCloudY;
	LLSliderCtrl*			mWLCloudDensity;
	LLSliderCtrl*			mWLCloudDetailX;
	LLSliderCtrl*			mWLCloudDetailY;
	LLSliderCtrl*			mWLCloudDetailDensity;
	LLSliderCtrl*			mWLCloudCoverage;
	LLSliderCtrl*			mWLCloudScale;
	LLSliderCtrl*			mWLCloudScrollX;
	LLSliderCtrl*			mWLCloudScrollY;
	LLSliderCtrl*			mWLDistanceMult;
	LLSliderCtrl*			mWLGamma;
	LLSliderCtrl*			mWLStarAlpha;

	typedef std::set<std::string> presets_list_t;
	static presets_list_t	sDefaultPresets;
};

//static
LLPanelWLSky* LLPanelWLSky::getSelf()
{
	LLFloaterWindlight* wlp = LLFloaterWindlight::findInstance();
	return wlp ? wlp->mPanelSky : NULL;
}

constexpr F32 SUN_AMBIENT_SLIDER_FACTOR = 1.f / 3.f;

LLPanelWLSky::presets_list_t LLPanelWLSky::sDefaultPresets;

//virtual
bool LLPanelWLSky::postBuild()
{
	// Presets

	std::string def_days = getString("WLDefaultSkyNames");
	// No editing or deleting of the blank string
	sDefaultPresets.emplace("");
	typedef boost::tokenizer<boost::char_separator<char> > boost_tokenizer;
	boost_tokenizer tokens(def_days, boost::char_separator<char>(":"));
	for (boost_tokenizer::iterator token_iter = tokens.begin();
		 token_iter != tokens.end(); ++token_iter)
	{
		std::string tok(*token_iter);
		sDefaultPresets.emplace(tok);
	}

	mWLPresetsCombo = getChild<LLComboBox>("WLPresetsCombo");
	for (LLWLSkyParamMgr::paramset_map_t::iterator
			it = gWLSkyParamMgr.mParamList.begin(),
			end = gWLSkyParamMgr.mParamList.end();
		 it != end; ++it)
	{
		mWLPresetsCombo->add(it->first);
	}
	// Entry for when we are in estate time
	mWLPresetsCombo->add(LLStringUtil::null);
	// Set default on combo box
	mWLPresetsCombo->selectByValue(LLSD("Default"));
	mWLPresetsCombo->setCommitCallback(onChangePresetName);
	mWLPresetsCombo->setCallbackUserData(this);

	// Blue horizon

	mWLBlueHorizonR = getChild<LLSliderCtrl>("WLBlueHorizonR");
	mWLBlueHorizonR->setCommitCallback(onColorControlRMoved);
	mWLBlueHorizonR->setCallbackUserData(&gWLSkyParamMgr.mBlueHorizon);

	mWLBlueHorizonG = getChild<LLSliderCtrl>("WLBlueHorizonG");
	mWLBlueHorizonG->setCommitCallback(onColorControlGMoved);
	mWLBlueHorizonG->setCallbackUserData(&gWLSkyParamMgr.mBlueHorizon);

	mWLBlueHorizonB = getChild<LLSliderCtrl>("WLBlueHorizonB");
	mWLBlueHorizonB->setCommitCallback(onColorControlBMoved);
	mWLBlueHorizonB->setCallbackUserData(&gWLSkyParamMgr.mBlueHorizon);

	mWLBlueHorizonI = getChild<LLSliderCtrl>("WLBlueHorizonI");
	mWLBlueHorizonI->setCommitCallback(onColorControlIMoved);
	mWLBlueHorizonI->setCallbackUserData(&gWLSkyParamMgr.mBlueHorizon);

	// Haze: density, horizon, multiplier and max altitude

	mWLHazeDensity = getChild<LLSliderCtrl>("WLHazeDensity");
	mWLHazeDensity->setCommitCallback(onColorControlRMoved);
	mWLHazeDensity->setCallbackUserData(&gWLSkyParamMgr.mHazeDensity);

	mWLHazeHorizon = getChild<LLSliderCtrl>("WLHazeHorizon");
	mWLHazeHorizon->setCommitCallback(onColorControlRMoved);
	mWLHazeHorizon->setCallbackUserData(&gWLSkyParamMgr.mHazeHorizon);

	mWLDensityMult = getChild<LLSliderCtrl>("WLDensityMult");
	mWLDensityMult->setCommitCallback(onFloatControlMoved);
	mWLDensityMult->setCallbackUserData(&gWLSkyParamMgr.mDensityMult);

	mWLMaxAltitude = getChild<LLSliderCtrl>("WLMaxAltitude");
	mWLMaxAltitude->setCommitCallback(onFloatControlMoved);
	mWLMaxAltitude->setCallbackUserData(&gWLSkyParamMgr.mMaxAlt);

	// Blue density

	mWLBlueDensityR = getChild<LLSliderCtrl>("WLBlueDensityR");
	mWLBlueDensityR->setCommitCallback(onColorControlRMoved);
	mWLBlueDensityR->setCallbackUserData(&gWLSkyParamMgr.mBlueDensity);

	mWLBlueDensityG = getChild<LLSliderCtrl>("WLBlueDensityG");
	mWLBlueDensityG->setCommitCallback(onColorControlGMoved);
	mWLBlueDensityG->setCallbackUserData(&gWLSkyParamMgr.mBlueDensity);

	mWLBlueDensityB = getChild<LLSliderCtrl>("WLBlueDensityB");
	mWLBlueDensityB->setCommitCallback(onColorControlBMoved);
	mWLBlueDensityB->setCallbackUserData(&gWLSkyParamMgr.mBlueDensity);

	mWLBlueDensityI = getChild<LLSliderCtrl>("WLBlueDensityI");
	mWLBlueDensityI->setCommitCallback(onColorControlIMoved);
	mWLBlueDensityI->setCallbackUserData(&gWLSkyParamMgr.mBlueDensity);

	// Sunlight

	mWLSunlightR = getChild<LLSliderCtrl>("WLSunlightR");
	mWLSunlightR->setCommitCallback(onColorControlRMoved);
	mWLSunlightR->setCallbackUserData(&gWLSkyParamMgr.mSunlight);

	mWLSunlightG = getChild<LLSliderCtrl>("WLSunlightG");
	mWLSunlightG->setCommitCallback(onColorControlGMoved);
	mWLSunlightG->setCallbackUserData(&gWLSkyParamMgr.mSunlight);

	mWLSunlightB = getChild<LLSliderCtrl>("WLSunlightB");
	mWLSunlightB->setCommitCallback(onColorControlBMoved);
	mWLSunlightB->setCallbackUserData(&gWLSkyParamMgr.mSunlight);

	mWLSunlightI = getChild<LLSliderCtrl>("WLSunlightI");
	mWLSunlightI->setCommitCallback(onColorControlIMoved);
	mWLSunlightI->setCallbackUserData(&gWLSkyParamMgr.mSunlight);

	// Glow

	mWLGlowR = getChild<LLSliderCtrl>("WLGlowR");
	mWLGlowR->setCommitCallback(onGlowRMoved);
	mWLGlowR->setCallbackUserData(&gWLSkyParamMgr.mGlow);

	mWLGlowB = getChild<LLSliderCtrl>("WLGlowB");
	mWLGlowB->setCommitCallback(onGlowBMoved);
	mWLGlowB->setCallbackUserData(&gWLSkyParamMgr.mGlow);

	// Ambient

	mWLAmbientR = getChild<LLSliderCtrl>("WLAmbientR");
	mWLAmbientR->setCommitCallback(onColorControlRMoved);
	mWLAmbientR->setCallbackUserData(&gWLSkyParamMgr.mAmbient);

	mWLAmbientG = getChild<LLSliderCtrl>("WLAmbientG");
	mWLAmbientG->setCommitCallback(onColorControlGMoved);
	mWLAmbientG->setCallbackUserData(&gWLSkyParamMgr.mAmbient);

	mWLAmbientB = getChild<LLSliderCtrl>("WLAmbientB");
	mWLAmbientB->setCommitCallback(onColorControlBMoved);
	mWLAmbientB->setCallbackUserData(&gWLSkyParamMgr.mAmbient);

	mWLAmbientI = getChild<LLSliderCtrl>("WLAmbientI");
	mWLAmbientI->setCommitCallback(onColorControlIMoved);
	mWLAmbientI->setCallbackUserData(&gWLSkyParamMgr.mAmbient);

	// Time of day

	mWLSunAngle = getChild<LLSliderCtrl>("WLSunAngle");
	mWLSunAngle->setCommitCallback(onSunMoved);
	mWLSunAngle->setCallbackUserData(&gWLSkyParamMgr.mLightnorm);

	mWLEastAngle = getChild<LLSliderCtrl>("WLEastAngle");
	mWLEastAngle->setCommitCallback(onSunMoved);
	mWLEastAngle->setCallbackUserData(&gWLSkyParamMgr.mLightnorm);

	// Clouds color

	mWLCloudColorR = getChild<LLSliderCtrl>("WLCloudColorR");
	mWLCloudColorR->setCommitCallback(onColorControlRMoved);
	mWLCloudColorR->setCallbackUserData(&gWLSkyParamMgr.mCloudColor);

	mWLCloudColorG = getChild<LLSliderCtrl>("WLCloudColorG");
	mWLCloudColorG->setCommitCallback(onColorControlGMoved);
	mWLCloudColorG->setCallbackUserData(&gWLSkyParamMgr.mCloudColor);

	mWLCloudColorB = getChild<LLSliderCtrl>("WLCloudColorB");
	mWLCloudColorB->setCommitCallback(onColorControlBMoved);
	mWLCloudColorB->setCallbackUserData(&gWLSkyParamMgr.mCloudColor);

	mWLCloudColorI = getChild<LLSliderCtrl>("WLCloudColorI");
	mWLCloudColorI->setCommitCallback(onColorControlIMoved);
	mWLCloudColorI->setCallbackUserData(&gWLSkyParamMgr.mCloudColor);

	// Cloud main: speed and density

	mWLCloudX = getChild<LLSliderCtrl>("WLCloudX");
	mWLCloudX->setCommitCallback(onColorControlRMoved);
	mWLCloudX->setCallbackUserData(&gWLSkyParamMgr.mCloudMain);

	mWLCloudY = getChild<LLSliderCtrl>("WLCloudY");
	mWLCloudY->setCommitCallback(onColorControlGMoved);
	mWLCloudY->setCallbackUserData(&gWLSkyParamMgr.mCloudMain);

	mWLCloudDensity = getChild<LLSliderCtrl>("WLCloudDensity");
	mWLCloudDensity->setCommitCallback(onColorControlBMoved);
	mWLCloudDensity->setCallbackUserData(&gWLSkyParamMgr.mCloudMain);

	// Cloud detail: speed and density

	mWLCloudDetailX = getChild<LLSliderCtrl>("WLCloudDetailX");
	mWLCloudDetailX->setCommitCallback(onColorControlRMoved);
	mWLCloudDetailX->setCallbackUserData(&gWLSkyParamMgr.mCloudDetail);

	mWLCloudDetailY = getChild<LLSliderCtrl>("WLCloudDetailY");
	mWLCloudDetailY->setCommitCallback(onColorControlGMoved);
	mWLCloudDetailY->setCallbackUserData(&gWLSkyParamMgr.mCloudDetail);

	mWLCloudDetailDensity = getChild<LLSliderCtrl>("WLCloudDetailDensity");
	mWLCloudDetailDensity->setCommitCallback(onColorControlBMoved);
	mWLCloudDetailDensity->setCallbackUserData(&gWLSkyParamMgr.mCloudDetail);

	// Cloud misc: coverage, scale, locking, scrolling and distance multipler

	mWLCloudCoverage = getChild<LLSliderCtrl>("WLCloudCoverage");
	mWLCloudCoverage->setCommitCallback(onFloatControlMoved);
	mWLCloudCoverage->setCallbackUserData(&gWLSkyParamMgr.mCloudCoverage);

	mWLCloudScale = getChild<LLSliderCtrl>("WLCloudScale");
	mWLCloudScale->setCommitCallback(onFloatControlMoved);
	mWLCloudScale->setCallbackUserData(&gWLSkyParamMgr.mCloudScale);

	mWLCloudLockX = getChild<LLCheckBoxCtrl>("WLCloudLockX");
	mWLCloudLockX->setCommitCallback(onCloudScrollXToggled);
	mWLCloudLockX->setCallbackUserData(this);

	mWLCloudLockY = getChild<LLCheckBoxCtrl>("WLCloudLockY");
	mWLCloudLockY->setCommitCallback(onCloudScrollYToggled);
	mWLCloudLockY->setCallbackUserData(this);

	mWLCloudScrollX = getChild<LLSliderCtrl>("WLCloudScrollX");
	mWLCloudScrollX->setCommitCallback(onCloudScrollXMoved);
	mWLCloudScrollX->setCallbackUserData(this);

	mWLCloudScrollY = getChild<LLSliderCtrl>("WLCloudScrollY");
	mWLCloudScrollY->setCommitCallback(onCloudScrollYMoved);
	mWLCloudScrollY->setCallbackUserData(this);

	mWLDistanceMult = getChild<LLSliderCtrl>("WLDistanceMult");
	mWLDistanceMult->setCommitCallback(onFloatControlMoved);
	mWLDistanceMult->setCallbackUserData(&gWLSkyParamMgr.mDistanceMult);

	// Dome

	mWLGamma = getChild<LLSliderCtrl>("WLGamma");
	mWLGamma->setCommitCallback(onFloatControlMoved);
	mWLGamma->setCallbackUserData(&gWLSkyParamMgr.mWLGamma);

	mWLStarAlpha = getChild<LLSliderCtrl>("WLStarAlpha");
	mWLStarAlpha->setCommitCallback(onStarAlphaMoved);
	mWLStarAlpha->setCallbackUserData(this);

	// Load/save/delete
	childSetAction("WLNewPreset", onNewPreset, this);
	childSetAction("WLSavePreset", onSavePreset, this);
	childSetAction("WLDeletePreset", onDeletePreset, this);

	refresh();

	return true;
}

//virtual
void LLPanelWLSky::refresh()
{
	LLWLParamSet& cur_params = gWLSkyParamMgr.mCurParams;
	bool err;

	// Blue horizon
	gWLSkyParamMgr.mBlueHorizon =
		cur_params.getVector(gWLSkyParamMgr.mBlueHorizon.mName, err);
	F32 red = gWLSkyParamMgr.mBlueHorizon.r * 0.5f;
	F32 green = gWLSkyParamMgr.mBlueHorizon.g * 0.5f;
	F32 blue = gWLSkyParamMgr.mBlueHorizon.b * 0.5f;
	mWLBlueHorizonR->setValue(red);
	mWLBlueHorizonG->setValue(green);
	mWLBlueHorizonB->setValue(blue);
	mWLBlueHorizonI->setValue(llmax(red, green, blue));

	// Haze: density, horizon, multiplier and altitude

	gWLSkyParamMgr.mHazeDensity =
		cur_params.getVector(gWLSkyParamMgr.mHazeDensity.mName, err);
	mWLHazeDensity->setValue(gWLSkyParamMgr.mHazeDensity.r);

	gWLSkyParamMgr.mHazeHorizon =
		cur_params.getVector(gWLSkyParamMgr.mHazeHorizon.mName, err);
	mWLHazeHorizon->setValue(gWLSkyParamMgr.mHazeHorizon.r);

	gWLSkyParamMgr.mDensityMult =
		cur_params.getVector(gWLSkyParamMgr.mDensityMult.mName, err);
	mWLDensityMult->setValue(gWLSkyParamMgr.mDensityMult.x *
							 gWLSkyParamMgr.mDensityMult.mult);

	gWLSkyParamMgr.mMaxAlt = cur_params.getVector(gWLSkyParamMgr.mMaxAlt.mName,
												  err);
	mWLMaxAltitude->setValue(gWLSkyParamMgr.mMaxAlt.x);

	// Blue density

	gWLSkyParamMgr.mBlueDensity =
		cur_params.getVector(gWLSkyParamMgr.mBlueDensity.mName, err);
	red = gWLSkyParamMgr.mBlueDensity.r * 0.5f;
	green = gWLSkyParamMgr.mBlueDensity.g * 0.5f;
	blue = gWLSkyParamMgr.mBlueDensity.b * 0.5f;
	mWLBlueDensityR->setValue(red);
	mWLBlueDensityG->setValue(green);
	mWLBlueDensityB->setValue(blue);
	mWLBlueDensityI->setValue(llmax(red, green, blue));

	// Lighting

	// Sunlight
	gWLSkyParamMgr.mSunlight =
		cur_params.getVector(gWLSkyParamMgr.mSunlight.mName, err);
	red = gWLSkyParamMgr.mSunlight.r * SUN_AMBIENT_SLIDER_FACTOR;
	green = gWLSkyParamMgr.mSunlight.g * SUN_AMBIENT_SLIDER_FACTOR;
	blue = gWLSkyParamMgr.mSunlight.b * SUN_AMBIENT_SLIDER_FACTOR;
	mWLSunlightR->setValue(red);
	mWLSunlightG->setValue(green);
	mWLSunlightB->setValue(blue);
	mWLSunlightI->setValue(llmax(red, green, blue));

	// Glow
	gWLSkyParamMgr.mGlow = cur_params.getVector(gWLSkyParamMgr.mGlow.mName,
												err);
	mWLGlowR->setValue(2.f - gWLSkyParamMgr.mGlow.r / 20.f);
	mWLGlowB->setValue(gWLSkyParamMgr.mGlow.b / -5.f);

	// Ambient
	gWLSkyParamMgr.mAmbient =
		cur_params.getVector(gWLSkyParamMgr.mAmbient.mName, err);
	red = gWLSkyParamMgr.mAmbient.r * SUN_AMBIENT_SLIDER_FACTOR;
	green = gWLSkyParamMgr.mAmbient.g * SUN_AMBIENT_SLIDER_FACTOR;
	blue = gWLSkyParamMgr.mAmbient.b * SUN_AMBIENT_SLIDER_FACTOR;
	mWLAmbientR->setValue(red);
	mWLAmbientG->setValue(green);
	mWLAmbientB->setValue(blue);
	mWLAmbientI->setValue(llmax(red, green, blue));

	// Sun angles
	constexpr F32 TWO_PI_INV = 1.f / F_TWO_PI;
	F32 value = cur_params.getFloat("sun_angle", err);
	mWLSunAngle->setValue(value * TWO_PI_INV);
	value = cur_params.getFloat("east_angle", err);
	mWLEastAngle->setValue(value * TWO_PI_INV);

	// Clouds color
	gWLSkyParamMgr.mCloudColor =
		cur_params.getVector(gWLSkyParamMgr.mCloudColor.mName, err);
	red = gWLSkyParamMgr.mCloudColor.r;
	green = gWLSkyParamMgr.mCloudColor.g;
	blue = gWLSkyParamMgr.mCloudColor.b;
	mWLCloudColorR->setValue(red);
	mWLCloudColorG->setValue(green);
	mWLCloudColorB->setValue(blue);
	mWLCloudColorI->setValue(llmax(red, green, blue));

	// Cloud main
	gWLSkyParamMgr.mCloudMain =
		cur_params.getVector(gWLSkyParamMgr.mCloudMain.mName, err);
	mWLCloudX->setValue(gWLSkyParamMgr.mCloudMain.r);
	mWLCloudY->setValue(gWLSkyParamMgr.mCloudMain.g);
	mWLCloudDensity->setValue(gWLSkyParamMgr.mCloudMain.b);

	// Cloud detail
	gWLSkyParamMgr.mCloudDetail =
		cur_params.getVector(gWLSkyParamMgr.mCloudDetail.mName, err);
	mWLCloudDetailX->setValue(gWLSkyParamMgr.mCloudDetail.r);
	mWLCloudDetailY->setValue(gWLSkyParamMgr.mCloudDetail.g);
	mWLCloudDetailDensity->setValue(gWLSkyParamMgr.mCloudDetail.b);

	// Cloud coverage
	gWLSkyParamMgr.mCloudCoverage =
		cur_params.getVector(gWLSkyParamMgr.mCloudCoverage.mName, err);
	mWLCloudCoverage->setValue(gWLSkyParamMgr.mCloudCoverage.x);

	// Cloud scale
	gWLSkyParamMgr.mCloudScale =
		cur_params.getVector(gWLSkyParamMgr.mCloudScale.mName, err);
	mWLCloudScale->setValue(gWLSkyParamMgr.mCloudScale.x);

	// Cloud scrolling. BEWARE: Windlight uses an offset of 10 for these.
	bool lock_x = !cur_params.getEnableCloudScrollX();
	mWLCloudLockX->set(lock_x);
	mWLCloudScrollX->setEnabled(!lock_x);
	mWLCloudScrollX->setValue(cur_params.getCloudScrollX() - 10.f);

	bool lock_y = !cur_params.getEnableCloudScrollY();
	mWLCloudLockY->set(lock_y);
	mWLCloudScrollY->setEnabled(!lock_y);
	mWLCloudScrollY->setValue(cur_params.getCloudScrollY() - 10.f);

	gWLSkyParamMgr.mDistanceMult =
		cur_params.getVector(gWLSkyParamMgr.mDistanceMult.mName, err);
	mWLDistanceMult->setValue(gWLSkyParamMgr.mDistanceMult.x);

	// Dome

	gWLSkyParamMgr.mWLGamma =
		cur_params.getVector(gWLSkyParamMgr.mWLGamma.mName, err);
	mWLGamma->setValue(gWLSkyParamMgr.mWLGamma.x);

	mWLStarAlpha->setValue(cur_params.getStarBrightness());
}

//static
void LLPanelWLSky::onColorControlRMoved(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLSky* self = getSelf();
	if (!self || !ctrl || !userdata) return;

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	WLColorControl* color_ctrl = (WLColorControl*)userdata;

	gWLSkyParamMgr.animate(false);

	color_ctrl->r = slider->getValueF32();
	if (color_ctrl->isSunOrAmbientColor)
	{
		color_ctrl->r *= 3.f;
	}
	if (color_ctrl->isBlueHorizonOrDensity)
	{
		color_ctrl->r *= 2.f;
	}

	// Move i if it is the max
	if (color_ctrl->r >= color_ctrl->g &&
		color_ctrl->r >= color_ctrl->b &&
		color_ctrl->hasSliderName)
	{
		color_ctrl->i = color_ctrl->r;
		std::string name = color_ctrl->mSliderName;
		name.append("I");

		if (color_ctrl->isSunOrAmbientColor)
		{
			self->childSetValue(name.c_str(), color_ctrl->r / 3.f);
		}
		else if (color_ctrl->isBlueHorizonOrDensity)
		{
			self->childSetValue(name.c_str(), color_ctrl->r * 0.5f);
		}
		else
		{
			self->childSetValue(name.c_str(), color_ctrl->r);
		}
	}

	color_ctrl->update(gWLSkyParamMgr.mCurParams);
	gWLSkyParamMgr.propagateParameters();
}

//static
void LLPanelWLSky::onColorControlGMoved(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLSky* self = getSelf();
	if (!self || !ctrl || !userdata) return;

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	WLColorControl* color_ctrl = (WLColorControl*)userdata;

	gWLSkyParamMgr.animate(false);

	color_ctrl->g = slider->getValueF32();
	if (color_ctrl->isSunOrAmbientColor)
	{
		color_ctrl->g *= 3.f;
	}
	if (color_ctrl->isBlueHorizonOrDensity)
	{
		color_ctrl->g *= 2.f;
	}

	// Move i if it is the max
	if (color_ctrl->g >= color_ctrl->r &&
		color_ctrl->g >= color_ctrl->b &&
		color_ctrl->hasSliderName)
	{
		color_ctrl->i = color_ctrl->g;
		std::string name = color_ctrl->mSliderName;
		name.append("I");

		if (color_ctrl->isSunOrAmbientColor)
		{
			self->childSetValue(name.c_str(), color_ctrl->g / 3.f);
		}
		else if (color_ctrl->isBlueHorizonOrDensity)
		{
			self->childSetValue(name.c_str(), color_ctrl->g * 0.5f);
		}
		else
		{
			self->childSetValue(name.c_str(), color_ctrl->g);
		}
	}

	color_ctrl->update(gWLSkyParamMgr.mCurParams);
	gWLSkyParamMgr.propagateParameters();
}

//static
void LLPanelWLSky::onColorControlBMoved(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLSky* self = getSelf();
	if (!self || !ctrl || !userdata) return;

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	WLColorControl* color_ctrl = (WLColorControl*)userdata;

	gWLSkyParamMgr.animate(false);

	color_ctrl->b = slider->getValueF32();
	if (color_ctrl->isSunOrAmbientColor)
	{
		color_ctrl->b *= 3.f;
	}
	if (color_ctrl->isBlueHorizonOrDensity)
	{
		color_ctrl->b *= 2.f;
	}

	// Move i if it is the max
	if (color_ctrl->hasSliderName && color_ctrl->b >= color_ctrl->r &&
		color_ctrl->b >= color_ctrl->g)
	{
		color_ctrl->i = color_ctrl->b;
		std::string name = color_ctrl->mSliderName;
		name.append("I");

		if (color_ctrl->isSunOrAmbientColor)
		{
			self->childSetValue(name.c_str(), color_ctrl->b / 3.f);
		}
		else if (color_ctrl->isBlueHorizonOrDensity)
		{
			self->childSetValue(name.c_str(), color_ctrl->b * 0.5f);
		}
		else
		{
			self->childSetValue(name.c_str(), color_ctrl->b);
		}
	}

	color_ctrl->update(gWLSkyParamMgr.mCurParams);
	gWLSkyParamMgr.propagateParameters();
}

//static
void LLPanelWLSky::onColorControlIMoved(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLSky* self = getSelf();
	if (!self || !ctrl || !userdata) return;

	gWLSkyParamMgr.animate(false);

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	WLColorControl* color_ctrl = (WLColorControl*)userdata;

	color_ctrl->i = slider->getValueF32();

	// Only for sliders where we pass a name
	if (color_ctrl->hasSliderName)
	{
		// Set it to the top
		F32 max_val = llmax(color_ctrl->r, color_ctrl->g, color_ctrl->b);
		F32 i_val;
		if (color_ctrl->isSunOrAmbientColor)
		{
			i_val = color_ctrl->i * 3.f;
		}
		else if (color_ctrl->isBlueHorizonOrDensity)
		{
			i_val = color_ctrl->i * 2.f;
		}
		else
		{
			i_val = color_ctrl->i;
		}

		// Get the names of the other sliders
		std::string r_name = color_ctrl->mSliderName;
		r_name.append("R");
		std::string g_name = color_ctrl->mSliderName;
		g_name.append("G");
		std::string b_name = color_ctrl->mSliderName;
		b_name.append("B");

		if (i_val == 0.f)
		{
			color_ctrl->r = color_ctrl->g = color_ctrl->b = 0.f;
		}
		else if (max_val == 0.f)
		{
			// If all at the start, set them all to the intensity
			color_ctrl->r = color_ctrl->g = color_ctrl->b = i_val;
		}
		else
		{
			// Add delta amounts to each
			F32 factor = 1.f + (i_val - max_val) / max_val;
			color_ctrl->r *= factor;
			color_ctrl->g *= factor;
			color_ctrl->b *= factor;
		}

		// Divide sun color vals by three
		if (color_ctrl->isSunOrAmbientColor)
		{
			constexpr F32 ONETHIRD = 1.f / 3.f;
			self->childSetValue(r_name.c_str(), color_ctrl->r * ONETHIRD);
			self->childSetValue(g_name.c_str(), color_ctrl->g * ONETHIRD);
			self->childSetValue(b_name.c_str(), color_ctrl->b * ONETHIRD);

		}
		else if (color_ctrl->isBlueHorizonOrDensity)
		{
			self->childSetValue(r_name.c_str(), color_ctrl->r * 0.5f);
			self->childSetValue(g_name.c_str(), color_ctrl->g * 0.5f);
			self->childSetValue(b_name.c_str(), color_ctrl->b * 0.5f);

		}
		else
		{
			// Set the sliders to the new vals
			self->childSetValue(r_name.c_str(), color_ctrl->r);
			self->childSetValue(g_name.c_str(), color_ctrl->g);
			self->childSetValue(b_name.c_str(), color_ctrl->b);
		}
	}

	// Now update the current parameters and send them to shaders
	color_ctrl->update(gWLSkyParamMgr.mCurParams);
	gWLSkyParamMgr.propagateParameters();
}

//static
void LLPanelWLSky::onGlowRMoved(LLUICtrl* ctrl, void* userdata)
{
	if (!ctrl || !userdata) return;

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	WLColorControl* color_ctrl = (WLColorControl*)userdata;

	gWLSkyParamMgr.animate(false);

	// Scaled by 20
	color_ctrl->r = (2.f - slider->getValueF32()) * 20.f;
	color_ctrl->update(gWLSkyParamMgr.mCurParams);
	gWLSkyParamMgr.propagateParameters();
}

//static
void LLPanelWLSky::onGlowBMoved(LLUICtrl* ctrl, void* userdata)
{
	if (!ctrl || !userdata) return;

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	WLColorControl* color_ctrl = (WLColorControl*)userdata;

	gWLSkyParamMgr.animate(false);

	// NOTE that we want NEGATIVE (-) B and NOT by 20 as 20 is too big
	color_ctrl->b = -slider->getValueF32() * 5.f;

	color_ctrl->update(gWLSkyParamMgr.mCurParams);
	gWLSkyParamMgr.propagateParameters();
}

//static
void LLPanelWLSky::onFloatControlMoved(LLUICtrl* ctrl, void* userdata)
{
	if (!ctrl || !userdata) return;

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	WLFloatControl* float_ctrl = (WLFloatControl*)userdata;

	gWLSkyParamMgr.animate(false);

	float_ctrl->x = slider->getValueF32() / float_ctrl->mult;
	float_ctrl->update(gWLSkyParamMgr.mCurParams);
	gWLSkyParamMgr.propagateParameters();
}

//static
void LLPanelWLSky::onSunMoved(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLSky* self = getSelf();
	if (!self || !ctrl || !userdata) return;

	WLColorControl* color_ctrl = (WLColorControl*)userdata;

	gWLSkyParamMgr.animate(false);

	LLWLParamSet& cur_params = gWLSkyParamMgr.mCurParams;

	// Get the two angles
	cur_params.setSunAngle(F_TWO_PI * self->mWLSunAngle->getValueF32());
	cur_params.setEastAngle(F_TWO_PI * self->mWLEastAngle->getValueF32());
	F32 sun_angle = cur_params.getSunAngle();
	F32 east_angle = cur_params.getEastAngle();
	// Set the sun vector
	F32 cos_sun_angle = cosf(sun_angle);
	color_ctrl->r = -sinf(east_angle) * cos_sun_angle;
	color_ctrl->g = sinf(sun_angle);
	color_ctrl->b = cosf(east_angle) * cos_sun_angle;
	color_ctrl->i = 1.f;

	color_ctrl->update(cur_params);
	gWLSkyParamMgr.propagateParameters();
}

//static
void LLPanelWLSky::onStarAlphaMoved(LLUICtrl* ctrl, void*)
{
	if (ctrl)
	{
		gWLSkyParamMgr.animate(false);

		LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
		gWLSkyParamMgr.mCurParams.setStarBrightness(slider->getValueF32());
	}
}

//static
bool LLPanelWLSky::newPromptCallback(const LLSD& notification,
									 const LLSD& response)
{
	LLPanelWLSky* self = getSelf();
	if (!self || LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	std::string text = response["message"].asString();
	if (text.empty())
	{
		return false;
	}

	LLComboBox* combo = self->mWLPresetsCombo;

	if (gWLSkyParamMgr.mParamList.find(text) ==
			gWLSkyParamMgr.mParamList.end())
	{
		// Add the current parameters to the list if not there
		gWLSkyParamMgr.addParamSet(text, gWLSkyParamMgr.mCurParams);
		combo->add(text);
		combo->sortByName();

		// Add a blank to the bottom
		combo->selectFirstItem();
		if (combo->getSimple().empty())
		{
			combo->remove(0);
		}
		combo->add(LLStringUtil::null);

		combo->setSelectedByValue(text, true);

		LLFloaterWindlight* floaterp =
			dynamic_cast<LLFloaterWindlight*>(self->getParentFloater());
		if (floaterp)
		{
			floaterp->mPanelDayCycle->addPreset(text);
		}
		gWLSkyParamMgr.savePreset(text);
	}
	else
	{
		// Otherwise, send a message to the user
		gNotifications.add("ExistsPresetAlert");
	}

	return false;
}

//static
void LLPanelWLSky::onNewPreset(void*)
{
	gNotifications.add("NewSkyPreset", LLSD(), LLSD(), newPromptCallback);
}

//static
void LLPanelWLSky::onSavePreset(void* userdata)
{
	LLPanelWLSky* self = (LLPanelWLSky*)userdata;
	if (!self) return;

	// Get the name
	std::string name = self->mWLPresetsCombo->getSelectedItemLabel();
	if (name.empty())
	{
		// Do not save with an empty name
		return;
	}

	// Check to see if it is a default and should not be overwritten
	if (sDefaultPresets.count(name) &&
		!gSavedSettings.getBool("SkyEditPresets"))
	{
		gNotifications.add("PresetNoEditDefault");
		return;
	}

	gWLSkyParamMgr.mCurParams.mName = name;

	gNotifications.add("WLSavePresetAlert", LLSD(), LLSD(), saveAlertCallback);
}

//static
bool LLPanelWLSky::saveAlertCallback(const LLSD& notification,
									 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		gWLSkyParamMgr.setParamSet(gWLSkyParamMgr.mCurParams.mName,
								   gWLSkyParamMgr.mCurParams);
		gWLSkyParamMgr.savePreset(gWLSkyParamMgr.mCurParams.mName);
	}
	return false;
}

//static
void LLPanelWLSky::onDeletePreset(void* userdata)
{
	LLPanelWLSky* self = (LLPanelWLSky*)userdata;
	if (!self) return;

	std::string name = self->mWLPresetsCombo->getSelectedValue().asString();
	if (!name.empty())
	{
		LLSD args;
		args["NAME"] = name;
		gNotifications.add("WLDeletePresetAlert", args, LLSD(),
						   deleteAlertCallback);
	}
}

//static
bool LLPanelWLSky::deleteAlertCallback(const LLSD& notification,
									   const LLSD& response)
{
	LLPanelWLSky* self = getSelf();
	if (!self || LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	LLComboBox* combo = self->mWLPresetsCombo;
	std::string name = combo->getSelectedValue().asString();
	if (name.empty())
	{
		return false;
	}

	// Check to see if it is a default and should not be deleted
	std::set<std::string>::iterator it = sDefaultPresets.find(name);
	if (it != sDefaultPresets.end())
	{
		gNotifications.add("PresetNoEditDefault");
		return false;
	}

	gWLSkyParamMgr.removeParamSet(name, true);

	// Remove and choose another
	S32 new_index = combo->getCurrentIndex();

	LLFloaterWindlight* floaterp =
		dynamic_cast<LLFloaterWindlight*>(self->getParentFloater());
	if (floaterp)
	{
		floaterp->mPanelDayCycle->deletePreset(name);
	}

	// Pick the previously selected index after delete
	if (new_index > 0)
	{
		--new_index;
	}
	if (combo->getItemCount() > 0)
	{
		combo->setCurrentByIndex(new_index);
	}

	return false;
}

//static
void LLPanelWLSky::onChangePresetName(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLSky* self = (LLPanelWLSky*)userdata;
	if (!self || !ctrl) return;

	gWLSkyParamMgr.animate(false);

	LLComboBox* combo = (LLComboBox*)ctrl;
	std::string name = combo->getSelectedValue().asString();
	if (!name.empty())
	{
		gWLSkyParamMgr.loadPreset(name);
		self->refresh();
	}
}

//static
void LLPanelWLSky::onCloudScrollXMoved(LLUICtrl* ctrl, void*)
{
	if (!ctrl) return;

	gWLSkyParamMgr.animate(false);
	// BEWARE: Windlight cloud scrolling value is offset by 10.
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	gWLSkyParamMgr.mCurParams.setCloudScrollX(slider->getValueF32() + 10.f);
}

//static
void LLPanelWLSky::onCloudScrollYMoved(LLUICtrl* ctrl, void*)
{
	if (!ctrl) return;

	gWLSkyParamMgr.animate(false);
	// BEWARE: Windlight cloud scrolling value is offset by 10.
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	gWLSkyParamMgr.mCurParams.setCloudScrollY(slider->getValueF32() + 10.f);
}

//static
void LLPanelWLSky::onCloudScrollXToggled(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLSky* self = (LLPanelWLSky*)userdata;
	if (!self || !ctrl) return;

	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	bool lock = check->get();
	self->mWLCloudScrollX->setEnabled(!lock);

	gWLSkyParamMgr.animate(false);
	gWLSkyParamMgr.mCurParams.setEnableCloudScrollX(!lock);
}

//static
void LLPanelWLSky::onCloudScrollYToggled(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLSky* self = (LLPanelWLSky*)userdata;
	if (!self || !ctrl) return;

	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	bool lock = check->get();
	self->mWLCloudScrollY->setEnabled(!lock);

	gWLSkyParamMgr.animate(false);
	gWLSkyParamMgr.mCurParams.setEnableCloudScrollY(!lock);
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelWLWater class (used to be a separate floater in LL's viewer)
///////////////////////////////////////////////////////////////////////////////

class LLPanelWLWater final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelWLWater);

public:
	LLPanelWLWater(const std::string& name)
	:	LLPanel(name)
	{
	}

	~LLPanelWLWater() override
	{
	}

	bool postBuild() override;
	void refresh() override;

private:
	static LLPanelWLWater* getSelf();

	static void onVector3ControlXMoved(LLUICtrl* ctrl, void* userdata);
	static void onVector3ControlYMoved(LLUICtrl* ctrl, void* userdata);
	static void onVector3ControlZMoved(LLUICtrl* ctrl, void* userdata);
	static void onVector2ControlXMoved(LLUICtrl* ctrl, void* userdata);
	static void onVector2ControlYMoved(LLUICtrl* ctrl, void* userdata);
	static void onFloatControlMoved(LLUICtrl* ctrl, void* userdata);
	static void onExpFloatControlMoved(LLUICtrl* ctrl, void* userdata);
	static void onWaterFogColorMoved(LLUICtrl* ctrl, void* userdata);
	static void onNormalMapPicked(LLUICtrl* ctrl, void*);
	static void onNewPreset(void*);
	static void onSavePreset(void* userdata);
	static void onDeletePreset(void* userdata);
	static void onChangePresetName(LLUICtrl* ctrl, void* userdata);

	static bool newPromptCallback(const LLSD& notification,
								  const LLSD& response);
	static bool saveAlertCallback(const LLSD& notification,
								  const LLSD& response);
	static bool deleteAlertCallback(const LLSD& notification,
									const LLSD& response);

private:
	LLColorSwatchCtrl*		mWaterFogColor;
	LLComboBox*				mWaterPresetsCombo;
	LLSliderCtrl*			mWaterFogDensity;
	LLSliderCtrl*			mWaterUnderWaterFogMod;
	LLSliderCtrl*			mWaterNormalScaleX;
	LLSliderCtrl*			mWaterNormalScaleY;
	LLSliderCtrl*			mWaterNormalScaleZ;
	LLSliderCtrl*			mWaterFresnelScale;
	LLSliderCtrl*			mWaterFresnelOffset;
	LLSliderCtrl*			mWaterScaleAbove;
	LLSliderCtrl*			mWaterScaleBelow;
	LLSliderCtrl*			mWaterBlurMult;
	LLSliderCtrl*			mWaterWave1DirX;
	LLSliderCtrl*			mWaterWave1DirY;
	LLSliderCtrl*			mWaterWave2DirX;
	LLSliderCtrl*			mWaterWave2DirY;
	LLTextureCtrl*			mWaterNormalMap;

	typedef std::set<std::string> presets_list_t;
	static presets_list_t	sDefaultPresets;
};

LLPanelWLWater::presets_list_t LLPanelWLWater::sDefaultPresets;

//static
LLPanelWLWater* LLPanelWLWater::getSelf()
{
	LLFloaterWindlight* wlp = LLFloaterWindlight::findInstance();
	return wlp ? wlp->mPanelWater : NULL;
}

//virtual
bool LLPanelWLWater::postBuild()
{
	// Presets

	std::string def_water = getString("WLDefaultWaterNames");
	// No editing or deleting of the blank string
	sDefaultPresets.emplace("");
	typedef boost::tokenizer<boost::char_separator<char> > boost_tokenizer;
	boost_tokenizer tokens(def_water, boost::char_separator<char>(":"));
	for (boost_tokenizer::iterator it = tokens.begin();
		 it != tokens.end(); ++it)
	{
		const std::string& tok = *it;
		sDefaultPresets.emplace(tok);
	}

	mWaterPresetsCombo = getChild<LLComboBox>("WaterPresetsCombo");
	for (LLWLWaterParamMgr::paramset_map_t::iterator
			it = gWLWaterParamMgr.mParamList.begin(),
			end = gWLWaterParamMgr.mParamList.end();
		 it != end; ++it)
	{
		mWaterPresetsCombo->add(it->first);
	}
	mWaterPresetsCombo->selectByValue(LLSD("Default"));
	mWaterPresetsCombo->setCommitCallback(onChangePresetName);
	mWaterPresetsCombo->setCallbackUserData(this);

	// Fog color
	mWaterFogColor = getChild<LLColorSwatchCtrl>("WaterFogColor");
	mWaterFogColor->setCommitCallback(onWaterFogColorMoved);
	mWaterFogColor->setCallbackUserData(&gWLWaterParamMgr.mFogColor);

	// Fog density

	mWaterFogDensity = getChild<LLSliderCtrl>("WaterFogDensity");
	mWaterFogDensity->setCommitCallback(onExpFloatControlMoved);
	mWaterFogDensity->setCallbackUserData(&gWLWaterParamMgr.mFogDensity);

	mWaterUnderWaterFogMod = getChild<LLSliderCtrl>("WaterUnderWaterFogMod");
	mWaterUnderWaterFogMod->setCommitCallback(onFloatControlMoved);
	mWaterUnderWaterFogMod->setCallbackUserData(&gWLWaterParamMgr.mUnderWaterFogMod);

	// Blue density

	mWaterNormalScaleX = getChild<LLSliderCtrl>("WaterNormalScaleX");
	mWaterNormalScaleX->setCommitCallback(onVector3ControlXMoved);
	mWaterNormalScaleX->setCallbackUserData(&gWLWaterParamMgr.mNormalScale);

	mWaterNormalScaleY = getChild<LLSliderCtrl>("WaterNormalScaleY");
	mWaterNormalScaleY->setCommitCallback(onVector3ControlYMoved);
	mWaterNormalScaleY->setCallbackUserData(&gWLWaterParamMgr.mNormalScale);

	mWaterNormalScaleZ = getChild<LLSliderCtrl>("WaterNormalScaleZ");
	mWaterNormalScaleZ->setCommitCallback(onVector3ControlZMoved);
	mWaterNormalScaleZ->setCallbackUserData(&gWLWaterParamMgr.mNormalScale);

	// Fresnel

	mWaterFresnelScale = getChild<LLSliderCtrl>("WaterFresnelScale");
	mWaterFresnelScale->setCommitCallback(onFloatControlMoved);
	mWaterFresnelScale->setCallbackUserData(&gWLWaterParamMgr.mFresnelScale);

	mWaterFresnelOffset = getChild<LLSliderCtrl>("WaterFresnelOffset");
	mWaterFresnelOffset->setCommitCallback(onFloatControlMoved);
	mWaterFresnelOffset->setCallbackUserData(&gWLWaterParamMgr.mFresnelOffset);

	// Scale above/below

	mWaterScaleAbove = getChild<LLSliderCtrl>("WaterScaleAbove");
	mWaterScaleAbove->setCommitCallback(onFloatControlMoved);
	mWaterScaleAbove->setCallbackUserData(&gWLWaterParamMgr.mScaleAbove);

	mWaterScaleBelow = getChild<LLSliderCtrl>("WaterScaleBelow");
	mWaterScaleBelow->setCommitCallback(onFloatControlMoved);
	mWaterScaleBelow->setCallbackUserData(&gWLWaterParamMgr.mScaleBelow);

	// Blur multiplier
	mWaterBlurMult = getChild<LLSliderCtrl>("WaterBlurMult");
	mWaterBlurMult->setCommitCallback(onFloatControlMoved);
	mWaterBlurMult->setCallbackUserData(&gWLWaterParamMgr.mBlurMultiplier);

	// Waves directions

	mWaterWave1DirX = getChild<LLSliderCtrl>("WaterWave1DirX");
	mWaterWave1DirX->setCommitCallback(onVector2ControlXMoved);
	mWaterWave1DirX->setCallbackUserData(&gWLWaterParamMgr.mWave1Dir);

	mWaterWave1DirY = getChild<LLSliderCtrl>("WaterWave1DirY");
	mWaterWave1DirY->setCommitCallback(onVector2ControlYMoved);
	mWaterWave1DirY->setCallbackUserData(&gWLWaterParamMgr.mWave1Dir);

	mWaterWave2DirX = getChild<LLSliderCtrl>("WaterWave2DirX");
	mWaterWave2DirX->setCommitCallback(onVector2ControlXMoved);
	mWaterWave2DirX->setCallbackUserData(&gWLWaterParamMgr.mWave2Dir);

	mWaterWave2DirY = getChild<LLSliderCtrl>("WaterWave2DirY");
	mWaterWave2DirY->setCommitCallback(onVector2ControlYMoved);
	mWaterWave2DirY->setCallbackUserData(&gWLWaterParamMgr.mWave2Dir);

	// Water normal map texture
	mWaterNormalMap = getChild<LLTextureCtrl>("WaterNormalMap");
	mWaterNormalMap->setDefaultImageAssetID(DEFAULT_WATER_NORMAL);
	mWaterNormalMap->setCommitCallback(onNormalMapPicked);
	mWaterNormalMap->setCallbackUserData(this);

	childSetAction("WaterNewPreset", onNewPreset, this);
	childSetAction("WaterSavePreset", onSavePreset, this);
	childSetAction("WaterDeletePreset", onDeletePreset, this);

	refresh();

	return true;
}

//virtual
void LLPanelWLWater::refresh()
{
	LLWaterParamSet& cur_params = gWLWaterParamMgr.mCurParams;
	bool err;

	// Blue horizon
	gWLWaterParamMgr.mFogColor =
		cur_params.getVector4(gWLWaterParamMgr.mFogColor.mName, err);
	LLColor4 col = gWLWaterParamMgr.getFogColor();
	col.mV[3] = 1.f;
	mWaterFogColor->set(col);

	// Fog and wavelets

	F32 value =
		logf(cur_params.getFloat(gWLWaterParamMgr.mFogDensity.mName, err)) /
		logf(gWLWaterParamMgr.mFogDensity.mBase);
	gWLWaterParamMgr.mFogDensity.mExp = value;
	gWLWaterParamMgr.setDensitySliderValue(value);
	mWaterFogDensity->setValue(value);

	value = cur_params.getFloat(gWLWaterParamMgr.mUnderWaterFogMod.mName, err);
	gWLWaterParamMgr.mUnderWaterFogMod.mX = value;
	mWaterUnderWaterFogMod->setValue(value);

	gWLWaterParamMgr.mNormalScale =
		cur_params.getVector3(gWLWaterParamMgr.mNormalScale.mName, err);
	mWaterNormalScaleX->setValue(gWLWaterParamMgr.mNormalScale.mX);
	mWaterNormalScaleY->setValue(gWLWaterParamMgr.mNormalScale.mY);
	mWaterNormalScaleZ->setValue(gWLWaterParamMgr.mNormalScale.mZ);

	// Fresnel

	value = cur_params.getFloat(gWLWaterParamMgr.mFresnelScale.mName, err);
	gWLWaterParamMgr.mFresnelScale.mX = value;
	mWaterFresnelScale->setValue(value);

	value = cur_params.getFloat(gWLWaterParamMgr.mFresnelOffset.mName, err);
	gWLWaterParamMgr.mFresnelOffset.mX = value;
	mWaterFresnelOffset->setValue(value);

	// Scale Above/Below

	value = cur_params.getFloat(gWLWaterParamMgr.mScaleAbove.mName, err);
	gWLWaterParamMgr.mScaleAbove.mX = value;
	mWaterScaleAbove->setValue(value);

	value = cur_params.getFloat(gWLWaterParamMgr.mScaleBelow.mName, err);
	gWLWaterParamMgr.mScaleBelow.mX = value;
	mWaterScaleBelow->setValue(value);

	// Blur multiplier
	value = cur_params.getFloat(gWLWaterParamMgr.mBlurMultiplier.mName, err);
	gWLWaterParamMgr.mBlurMultiplier.mX = value;
	mWaterBlurMult->setValue(value);

	// Waves directions

	gWLWaterParamMgr.mWave1Dir =
		cur_params.getVector2(gWLWaterParamMgr.mWave1Dir.mName, err);
	mWaterWave1DirX->setValue(gWLWaterParamMgr.mWave1Dir.mX);
	mWaterWave1DirY->setValue(gWLWaterParamMgr.mWave1Dir.mY);

	gWLWaterParamMgr.mWave2Dir =
		cur_params.getVector2(gWLWaterParamMgr.mWave2Dir.mName, err);
	mWaterWave2DirX->setValue(gWLWaterParamMgr.mWave2Dir.mX);
	mWaterWave2DirY->setValue(gWLWaterParamMgr.mWave2Dir.mY);

	// Normal map texture
	mWaterNormalMap->setImageAssetID(gWLWaterParamMgr.getNormalMapID());
}

//static
void LLPanelWLWater::onVector3ControlXMoved(LLUICtrl* ctrl, void* userdata)
{
	if (ctrl && userdata)
	{
		LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
		WaterVector3Control* vec_ctrl = (WaterVector3Control*)userdata;
		vec_ctrl->mX = slider->getValueF32();
		vec_ctrl->update(gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.propagateParameters();
	}
}

//static
void LLPanelWLWater::onVector3ControlYMoved(LLUICtrl* ctrl, void* userdata)
{
	if (ctrl && userdata)
	{
		LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
		WaterVector3Control* vec_ctrl = (WaterVector3Control*)userdata;
		vec_ctrl->mY = slider->getValueF32();
		vec_ctrl->update(gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.propagateParameters();
	}
}

//static
void LLPanelWLWater::onVector3ControlZMoved(LLUICtrl* ctrl, void* userdata)
{
	if (ctrl && userdata)
	{
		LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
		WaterVector3Control* vec_ctrl = (WaterVector3Control*)userdata;
		vec_ctrl->mZ = slider->getValueF32();
		vec_ctrl->update(gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.propagateParameters();
	}
}

//static
void LLPanelWLWater::onVector2ControlXMoved(LLUICtrl* ctrl, void* userdata)
{
	if (ctrl && userdata)
	{
		LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
		WaterVector2Control* vec_ctrl = (WaterVector2Control*)userdata;
		vec_ctrl->mX = slider->getValueF32();
		vec_ctrl->update(gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.propagateParameters();
	}
}

//static
void LLPanelWLWater::onVector2ControlYMoved(LLUICtrl* ctrl, void* userdata)
{
	if (ctrl && userdata)
	{
		LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
		WaterVector2Control* vec_ctrl = (WaterVector2Control*)userdata;
		vec_ctrl->mY = slider->getValueF32();
		vec_ctrl->update(gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.propagateParameters();
	}
}

//static
void LLPanelWLWater::onExpFloatControlMoved(LLUICtrl* ctrl, void* userdata)
{
	if (ctrl && userdata)
	{
		LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
		WaterExpFloatControl* float_ctrl = (WaterExpFloatControl*)userdata;

		F32 val = slider->getValueF32();
		float_ctrl->mExp = val;

		gWLWaterParamMgr.setDensitySliderValue(val);

		float_ctrl->update(gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.propagateParameters();
	}
}

//static
void LLPanelWLWater::onFloatControlMoved(LLUICtrl* ctrl, void* userdata)
{
	if (ctrl && userdata)
	{
		LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
		WaterFloatControl* float_ctrl = (WaterFloatControl*)userdata;
		float_ctrl->mX = slider->getValueF32() / float_ctrl->mMult;
		float_ctrl->update(gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.propagateParameters();
	}
}

//static
void LLPanelWLWater::onWaterFogColorMoved(LLUICtrl* ctrl, void* userdata)
{
	if (ctrl && userdata)
	{
		LLColorSwatchCtrl* swatch = (LLColorSwatchCtrl*)ctrl;
		WaterColorControl* color_ctrl = (WaterColorControl*)userdata;
		*color_ctrl = swatch->get();
		color_ctrl->update(gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.propagateParameters();
	}
}

//static
void LLPanelWLWater::onNormalMapPicked(LLUICtrl* ctrl, void*)
{
	if (ctrl)
	{
		LLTextureCtrl* texture = (LLTextureCtrl*)ctrl;
		const LLUUID& id = texture->getImageAssetID();
		gWLWaterParamMgr.setNormalMapID(id);
	}
}

//static
bool LLPanelWLWater::newPromptCallback(const LLSD& notification,
									   const LLSD& response)
{
	LLPanelWLWater* self = getSelf();
	if (!self) return false;

	std::string text = response["message"].asString();
	if (text.empty())
	{
		return false;
	}

	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		if (gWLWaterParamMgr.mParamList.find(text) ==
				gWLWaterParamMgr.mParamList.end())
		{
			// Add the current parameters to the list see if it is there first
			gWLWaterParamMgr.addParamSet(text, gWLWaterParamMgr.mCurParams);
			LLComboBox* combo = self->mWaterPresetsCombo;
			combo->add(text);
			combo->sortByName();
			combo->setSelectedByValue(text, true);
			gWLWaterParamMgr.savePreset(text);
		}
		else
		{
			// Otherwise, send a message to the user
			gNotifications.add("ExistsPresetAlert");
		}
	}
	return false;
}

//static
void LLPanelWLWater::onNewPreset(void*)
{
	gNotifications.add("NewWaterPreset", LLSD(), LLSD(), newPromptCallback);
}

//static
void LLPanelWLWater::onSavePreset(void* userdata)
{
	LLPanelWLWater* self = (LLPanelWLWater*)userdata;
	if (!self) return;

	// Get the name
	std::string name = self->mWaterPresetsCombo->getSelectedItemLabel();
	if (name.empty())
	{
		// Do not save with an empty name
		return;
	}

	// Check to see if it is a default and should not be overwritten
	if (sDefaultPresets.find(name) != sDefaultPresets.end() &&
		!gSavedSettings.getBool("WaterEditPresets"))
	{
		gNotifications.add("PresetNoEditDefault");
		return;
	}

	gWLWaterParamMgr.mCurParams.mName = name;

	gNotifications.add("WLSavePresetAlert", LLSD(), LLSD(), saveAlertCallback);
}

//static
bool LLPanelWLWater::saveAlertCallback(const LLSD& notification,
									   const LLSD& response)
{
	// If user chose save, do it. Otherwise, do not do anything.
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		gWLWaterParamMgr.setParamSet(gWLWaterParamMgr.mCurParams.mName,
									 gWLWaterParamMgr.mCurParams);
		gWLWaterParamMgr.savePreset(gWLWaterParamMgr.mCurParams.mName);
	}
	return false;
}

//static
void LLPanelWLWater::onDeletePreset(void* userdata)
{
	LLPanelWLWater* self = (LLPanelWLWater*)userdata;
	if (!self) return;

	// Get the name
	std::string name = self->mWaterPresetsCombo->getSelectedValue().asString();
	if (name.empty())
	{
		// Do not save with an empty name
		return;
	}

	LLSD args;
	args["NAME"] = name;
	gNotifications.add("WLDeletePresetAlert", args, LLSD(),
					   deleteAlertCallback);
}

//static
bool LLPanelWLWater::deleteAlertCallback(const LLSD& notification,
										 const LLSD& response)
{
	LLPanelWLWater* self = getSelf();
	if (!self || LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	LLComboBox* combo = self->mWaterPresetsCombo;
	std::string name = combo->getSelectedValue().asString();
	if (name.empty())
	{
		return false;
	}

	// Check to see if it's a default and shouldn't be deleted
	presets_list_t::iterator it = sDefaultPresets.find(name);
	if (it != sDefaultPresets.end())
	{
		gNotifications.add("PresetNoEditDefault");
		return false;
	}

	gWLWaterParamMgr.removeParamSet(name, true);

	// Remove and choose another
	S32 new_index = combo->getCurrentIndex();

	combo->remove(name);

	// Pick the previously selected index after delete
	if (new_index > 0)
	{
		--new_index;
	}

	if (combo->getItemCount() > 0)
	{
		combo->setCurrentByIndex(new_index);
	}

	return false;
}

//static
void LLPanelWLWater::onChangePresetName(LLUICtrl* ctrl, void* userdata)
{
	LLPanelWLWater* self = (LLPanelWLWater*)userdata;
	if (!self || !ctrl) return;

	LLComboBox* combo = (LLComboBox*)ctrl;
	std::string name = combo->getSelectedValue().asString();
	if (!name.empty())
	{
		gWLWaterParamMgr.loadPreset(name);
		self->refresh();
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterWindlight class proper
///////////////////////////////////////////////////////////////////////////////

//static
void* LLFloaterWindlight::createPanelDayCycle(void* data)
{
	LLFloaterWindlight* self = (LLFloaterWindlight*)data;
	self->mPanelDayCycle = new LLPanelWLDayCycle("day_cycle");
	return self->mPanelDayCycle;
}

//static
void* LLFloaterWindlight::createPanelSky(void* data)
{
	LLFloaterWindlight* self = (LLFloaterWindlight*)data;
	self->mPanelSky = new LLPanelWLSky("sky");
	return self->mPanelSky;
}

//static
void* LLFloaterWindlight::createPanelWater(void* data)
{
	LLFloaterWindlight* self = (LLFloaterWindlight*)data;
	self->mPanelWater = new LLPanelWLWater("water");
	return self->mPanelWater;
}

LLFloaterWindlight::LLFloaterWindlight(const LLSD&)
{
	LLCallbackMap::map_t factory_map;
	factory_map["day_cycle"] = LLCallbackMap(createPanelDayCycle, this);
	factory_map["sky"] = LLCallbackMap(createPanelSky, this);
	factory_map["water"] = LLCallbackMap(createPanelWater, this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_windlight.xml",
												 &factory_map);
}

//virtual
bool LLFloaterWindlight::postBuild()
{
	mEnvTimeText = getChild<LLTextBox>("EnvTimeText");
	mEnvWaterColorText = getChild<LLTextBox>("EnvWaterColorText");

	mEnvTimeSlider = getChild<LLSliderCtrl>("EnvTimeSlider");
	mEnvTimeSlider->setCommitCallback(onChangeDayTime);
	mEnvTimeSlider->setCallbackUserData(this);

	mEnvCloudSlider = getChild<LLSliderCtrl>("EnvCloudSlider");
	mEnvCloudSlider->setCommitCallback(onChangeCloudCoverage);
	mEnvCloudSlider->setCallbackUserData(this);

	mEnvWaterColor = getChild<LLColorSwatchCtrl>("EnvWaterColor");
	mEnvWaterColor->setCommitCallback(onChangeWaterColor);
	mEnvWaterColor->setCallbackUserData(&gWLWaterParamMgr.mFogColor);

	mEnvWaterFogSlider = getChild<LLSliderCtrl>("EnvWaterFogSlider");
	mEnvWaterFogSlider->setCommitCallback(onChangeWaterFogDensity);
	mEnvWaterFogSlider->setCallbackUserData(&gWLWaterParamMgr.mFogDensity);

	mPreviewBtn = getChild<LLButton>("preview_btn");
	mPreviewBtn->setClickedCallback(onPreviewAsEE, this);

	refresh();

	return true;
}

//virtual
void LLFloaterWindlight::refresh()
{
	// Sync the clock
	F32 val = (F32)gWLSkyParamMgr.mAnimator.getDayTime();
	std::string time_str = timeToString(val);

	mEnvTimeText->setValue(time_str);

	// Sync time slider which starts at 06:00
	val -= 0.25f;
	if (val < 0.f)
	{
		val += 1.f;
	}
	mEnvTimeSlider->setValue(val);

	// Sync cloud coverage
	bool err;
	mEnvCloudSlider->setValue(gWLSkyParamMgr.mCurParams.getFloat("cloud_shadow",
																 err));

	// Sync water params

	LLColor4 col = gWLWaterParamMgr.getFogColor();
	col.mV[3] = 1.f;
	mEnvWaterColor->set(col);

	mEnvWaterFogSlider->setValue(gWLWaterParamMgr.mFogDensity.mExp);
	gWLWaterParamMgr.setDensitySliderValue(gWLWaterParamMgr.mFogDensity.mExp);

	// Only allow access to these if we are using vertex shaders
	bool enable = gPipeline.shadersLoaded();
	mEnvWaterColor->setEnabled(enable);
	mEnvWaterColorText->setEnabled(enable);

	// Only allow access to this if we are using Windlight
	mEnvCloudSlider->setEnabled(gPipeline.canUseWindLightShaders());

	// Show the "Preview frame" button. *TODO: make always visible.
	mPreviewBtn->setVisible(true);

	// Ask our panels to refresh themselves
	mPanelDayCycle->refresh();
	mPanelSky->refresh();
	mPanelWater->refresh();
}

//virtual
void LLFloaterWindlight::draw()
{
//MK
	if (gRLenabled && gRLInterface.mContainsSetenv)
	{
		close();
		return;
	}
//mk

	LLFloater::draw();
}

//static
void LLFloaterWindlight::setDayTime(F32 time)
{
	// Turn off animator...
	gWLSkyParamMgr.animate(false);
	// Set the new time...
	gWLSkyParamMgr.mAnimator.setDayTime((F64)time);
	// Then call update once.
	gWLSkyParamMgr.mAnimator.update(gWLSkyParamMgr.mCurParams);

	// Since we now always render in EE mode, the WL animator does not refresh
	// our floater, so we must do it here in order to get the time of day
	// sliders synced...
	LLFloaterWindlight* self = LLFloaterWindlight::findInstance();
	if (self)
	{
		self->refresh();
	}
}

//static
void LLFloaterWindlight::onChangeDayTime(LLUICtrl* ctrl, void*)
{
	if (!ctrl) return;

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	F32 val = slider->getValueF32() + 0.25f;
	if (val > 1.f)
	{
		--val;
	}
	setDayTime(val);
}

//static
void LLFloaterWindlight::onChangeCloudCoverage(LLUICtrl* ctrl, void*)
{
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	if (!slider) return;

	gWLSkyParamMgr.mCurParams.set("cloud_shadow", slider->getValueF32());
}

//static
void LLFloaterWindlight::onChangeWaterFogDensity(LLUICtrl* ctrl,
												   void* userdata)
{
	if (!ctrl || !userdata) return;

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;

	WaterExpFloatControl* exp_float_ctrl = (WaterExpFloatControl*)userdata;

	F32 val = slider->getValueF32();
	exp_float_ctrl->mExp = val;

	gWLWaterParamMgr.setDensitySliderValue(val);
	exp_float_ctrl->update(gWLWaterParamMgr.mCurParams);
	gWLWaterParamMgr.propagateParameters();
}

//static
void LLFloaterWindlight::onChangeWaterColor(LLUICtrl* ctrl, void* userdata)
{
	if (!ctrl || !userdata) return;

	LLColorSwatchCtrl* swatch = (LLColorSwatchCtrl*)ctrl;
	WaterColorControl* color_ctrl = (WaterColorControl*)userdata;
	*color_ctrl = swatch->get();

	color_ctrl->update(gWLWaterParamMgr.mCurParams);
	gWLWaterParamMgr.propagateParameters();
}

//static
void LLFloaterWindlight::onPreviewAsEE(void*)
{
	gWLSkyParamMgr.propagateParameters();
	gWLWaterParamMgr.propagateParameters();
}

//static
std::string LLFloaterWindlight::timeToString(F32 cur_time)
{
	// Get hours and minutes
	S32 hours = (S32)(HOUR_PER_DAY * cur_time);
	cur_time -= (F32)hours / HOUR_PER_DAY;
	S32 min = ll_roundp(1440.f * cur_time);

	// Handle case where it is 60
	if (min == 60)
	{
		++hours;
		min = 0;
	}
	if (hours >= 24)
	{
		hours = 0;
	}

	// Make the string
	std::stringstream new_time;
	new_time << hours << ":";

	// Double 0
	if (min < 10)
	{
		new_time << 0;
	}

	// Finish it
	new_time << min;

	return new_time.str();
}
