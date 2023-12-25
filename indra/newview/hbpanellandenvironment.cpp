/**
 * @file hbpanellandenvironment.cpp
 * @brief Configuration of environmemt settings for land (parcel or region).
 *
 * $LicenseInfo:firstyear=2019&license=viewergpl$
 *
 * Copyright (c) 2019-2023 Henri Beauchamp
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

#include "boost/pointer_cast.hpp"

#include "hbpanellandenvironment.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lldate.h"
#include "llmultisliderctrl.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"				// For gFrameTimeSeconds
#include "llenvsettings.h"
#include "hbfloatereditenvsettings.h"
#include "hbfloaterinvitemspicker.h"
#include "llfloaterregioninfo.h"		// For LLEstateInfoModel
#include "llinventorymodel.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llworld.h"
#include "roles_constants.h"

//-----------------------------------------------------------------------------
// HBSettingsDropTarget class - UI element for settings drop targets. It also
// handles automatically click-selection via the inventory items picker.
//-----------------------------------------------------------------------------

class HBSettingsDropTarget final : public LLView
{
protected:
	LOG_CLASS(HBSettingsDropTarget);

public:
	HBSettingsDropTarget(LLView* parentp, HBPanelLandEnvironment* panelp,
						 S32 track)
	:	LLView(parentp->getName() + "_area", false),
		mLandEnvironmentPanel(panelp),
		mTrack(track)
	{
		setFollows(FOLLOWS_LEFT | FOLLOWS_TOP);
		LLRect rect = parentp->getRect();
		setRect(rect);

		// Adjust rect so to be within the parent view (usually a view border)
		++rect.mBottom;
		--rect.mTop;
		rect.mLeft += 2;
		rect.mRight -= 2;

		// Create a text box associated with our drop target view
		mDropTargetText = new LLTextBox(parentp->getName() + "_text", rect, "",
										LLFontGL::getFontSansSerifSmall(),
										true); // Opaque text box
		// Add as a child of our owner panel
		panelp->addChild(mDropTargetText);

		// Add ourselves as a child of the panel: this must be done *after* the
		// text box was added, so that the drop target view is on top (note
		// that it is however not opaque to mouse: tool tip hovers and clicks
		// do get to the underlying text box).
		panelp->addChild(this);

		// Prettify the text box with centered text and an adequate tool tip
		mDropTargetText->setHAlign(LLFontGL::HCENTER);
		std::string tooltip = panelp->getString(track ? "sky_tool_tip"
													  : "water_tool_tip");
		mDropTargetText->setToolTip(tooltip);

		// Setup click-action on the text of the drop target (inventory picker
		// call)
		mDropTargetText->setClickedCallback(onTextClicked, this);
	}

	void setEnabled(bool enabled) override
	{
		mDropTargetText->setEnabled(enabled);
		LLView::setEnabled(enabled);
	}

	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type,
						   void* cargo_data, EAcceptance* accept,
						   std::string& tooltip_msg) override
	{
		// Careful: pointInView() gets f*cked up whenever the panel is embedded
		// inside a layout stack.
		if (!getEnabled() || !pointInView(x, y))
		{
			return false;
		}

		*accept = ACCEPT_NO;
		if (cargo_type == DAD_SETTINGS)
		{
			LLSettingsType::EType type = mTrack ? LLSettingsType::ST_SKY
												: LLSettingsType::ST_WATER;
			LLViewerInventoryItem* itemp = (LLViewerInventoryItem*)cargo_data;
			if (itemp && gInventory.getItem(itemp->getUUID()) &&
				itemp->getSettingsType() == type)
			{
				*accept = ACCEPT_YES_COPY_SINGLE;
				if (drop)
				{
					// Auto-update our text with the dropped item name
					mDropTargetText->setText(itemp->getName());
					// Inform our owner about the user choice
					mLandEnvironmentPanel->onChoosenItem(itemp, mTrack);
				}
			}
		}
		return true;
	}

	LL_INLINE void setText(const std::string& text)
	{
		mDropTargetText->setText(text);
	}

	LL_INLINE const std::string& getText()
	{
		return mDropTargetText->getText();
	}

private:
	static void invItemsPickerCallback(const std::vector<std::string>&,
									   const uuid_vec_t& ids, void* userdata,
									   bool)
	{
		HBSettingsDropTarget* self = (HBSettingsDropTarget*)userdata;
		if (!self || ids.empty())
		{
			return;
		}
		LLUUID inv_id = ids[0];
		// Make sure we are not trying to use a link and get the linked item
		// Id in that case.
		if (inv_id.notNull())
		{
			inv_id = gInventory.getLinkedItemID(inv_id);
		}
		LLViewerInventoryItem* itemp = gInventory.getItem(inv_id);
		if (itemp)
		{
			// Auto-update our text with the dropped item name
			self->mDropTargetText->setText(itemp->getName());
			// Inform our owner about the user choice
			self->mLandEnvironmentPanel->onChoosenItem(itemp, self->mTrack);
		}
	}

	static void onTextClicked(void* userdata)
	{
		HBSettingsDropTarget* self = (HBSettingsDropTarget*)userdata;
		if (!self || !self->getEnabled())
		{
			return;
		}
		HBFloaterInvItemsPicker* pickerp =
			new HBFloaterInvItemsPicker(self, invItemsPickerCallback, self);
		if (pickerp)
		{
			S32 sub_type = self->mTrack ? LLSettingsType::ST_SKY
										: LLSettingsType::ST_WATER;
			pickerp->setAssetType(LLAssetType::AT_SETTINGS, sub_type);
		}
	}

private:
	HBPanelLandEnvironment*	mLandEnvironmentPanel;
	LLTextBox*				mDropTargetText;
	S32						mTrack;
};

//-----------------------------------------------------------------------------
// HBPanelLandEnvironment class proper
//-----------------------------------------------------------------------------

static LLSafeHandle<LLParcelSelection> sDummyParcelHandle;

HBPanelLandEnvironment::HBPanelLandEnvironment(LLParcelSelectionHandle& parcel)
:	mParcel(parcel),
	mRegionHandle(0),
	mIsRegion(false),
	mDayParametersDirty(false),
	mEnvOverrideCheck(false),
	mLastEnabledState(false),
	mLastParametersChange(0.f),
	mLastTimeOfDayUpdate(0.f),
	mCurEnvVersion(INVALID_PARCEL_ENVIRONMENT_VERSION),
	mLastParcelId(INVALID_PARCEL_ID)
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_land_environment.xml");
}

HBPanelLandEnvironment::HBPanelLandEnvironment(U64 region_handle)
:	mParcel(sDummyParcelHandle),
	mRegionHandle(region_handle),
	mIsRegion(true),
	mDayParametersDirty(false),
	mLastParametersChange(0.f),
	mCurEnvVersion(INVALID_PARCEL_ENVIRONMENT_VERSION),
	mLastParcelId(INVALID_PARCEL_ID)
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_land_environment.xml");
}

//virtual
HBPanelLandEnvironment::~HBPanelLandEnvironment()
{
	if (mChangeConnection.connected())
	{
		mChangeConnection.disconnect();
	}
	closeEditFloater(true);
}

//virtual
bool HBPanelLandEnvironment::postBuild()
{
	mUseDefaultBtn = getChild<LLButton>("use_default_btn");
	mUseDefaultBtn->setClickedCallback(onBtnDefault, this);
	if (!mIsRegion)
	{
		mUseDefaultBtn->setLabel(getString("region_settings_label"));
	}

	mUseInventoryBtn = getChild<LLButton>("use_inventory_btn");
	mUseInventoryBtn->setClickedCallback(onBtnInventory, this);

	mUseCustomBtn = getChild<LLButton>("use_custom_btn");
	mUseCustomBtn->setClickedCallback(onBtnCustom, this);

	mResetAltitudesBtn = getChild<LLButton>("reset_alts_btn");
	mResetAltitudesBtn->setClickedCallback(onBtnReset, this);

	mAllowOverrideCheck = getChild<LLCheckBoxCtrl>("allow_override_chk");
	if (mIsRegion)
	{
		mAllowOverrideCheck->setCommitCallback(onAllowOverride);
		mAllowOverrideCheck->setCallbackUserData(this);
	}
	else
	{
		mAllowOverrideCheck->setVisible(false);
	}

	mDayLengthSlider = getChild<LLSliderCtrl>("day_length_sld");
	mDayLengthSlider->setCommitCallback(onDayParametersChanged);
	mDayLengthSlider->setCallbackUserData(this);

	mDayOffsetSlider = getChild<LLSliderCtrl>("day_offset_sld");
	mDayOffsetSlider->setCommitCallback(onDayParametersChanged);
	mDayOffsetSlider->setCallbackUserData(this);

	mApparentDayLengthText = getChild<LLTextBox>("day_time_value_txt");
	mAltitude2ValueText = getChild<LLTextBox>("alt2_value_txt");
	mAltitude3ValueText = getChild<LLTextBox>("alt3_value_txt");
	mAltitude4ValueText = getChild<LLTextBox>("alt4_value_txt");

	mAltitudesSlider = getChild<LLMultiSliderCtrl>("altitudes_sld");
	mAltitudesSlider->setCommitCallback(onAltSliderCommit);
	mAltitudesSlider->setCallbackUserData(this);
	mAltitudesSlider->setSliderMouseUpCallback(onAltSliderMouseUp);
	mAltitudesSlider->addSlider(1000.f, "sld1");
	mAltitudesSlider->addSlider(2000.f, "sld2");
	mAltitudesSlider->addSlider(3000.f, "sld3");

	LLView* parent_viewp = getChild<LLView>("water_drop_tgt");
	HBSettingsDropTarget* targetp = new HBSettingsDropTarget(parent_viewp,
															 this, 0);
	mDropTargets.push_back(targetp);

	parent_viewp = getChild<LLView>("alt1_drop_tgt");
	targetp = new HBSettingsDropTarget(parent_viewp, this, 1);
	mDropTargets.push_back(targetp);

	parent_viewp = getChild<LLView>("alt2_drop_tgt");
	targetp = new HBSettingsDropTarget(parent_viewp, this, 2);
	mDropTargets.push_back(targetp);

	parent_viewp = getChild<LLView>("alt3_drop_tgt");
	targetp = new HBSettingsDropTarget(parent_viewp, this, 3);
	mDropTargets.push_back(targetp);

	parent_viewp = getChild<LLView>("alt4_drop_tgt");
	targetp = new HBSettingsDropTarget(parent_viewp, this, 4);
	mDropTargets.push_back(targetp);

	refresh();

	mChangeConnection =
		gEnvironment.setEnvironmentChanged([this](LLEnvironment::EEnvSelection env,
												  S32 version)
										   {
											onEnvironmentChanged(env, version);
										   });

	if (mIsRegion)
	{
		refreshFromRegion();
	}

	return true;
}

//virtual
void HBPanelLandEnvironment::setEnabled(bool enabled)
{
	bool inv_ok = enabled && gAgent.hasInventorySettings();

	mUseDefaultBtn->setEnabled(enabled);
	mUseInventoryBtn->setEnabled(inv_ok);
	mUseCustomBtn->setEnabled(enabled);
	mAllowOverrideCheck->setEnabled(enabled);

	mDayLengthSlider->setEnabled(enabled);
	mDayOffsetSlider->setEnabled(enabled);

	mAltitudesSlider->setEnabled(enabled && mIsRegion);
	mResetAltitudesBtn->setEnabled(enabled && mIsRegion);
	for (U32 i = 0, count = mDropTargets.size(); i < count; ++i)
	{
		mDropTargets[i]->setEnabled(inv_ok);
	}

	LLPanel::setEnabled(enabled);
}

//virtual
void HBPanelLandEnvironment::draw()
{
	if (!mIsRegion && mLastParcelId != getParcelId())
	{
		refreshFromParcel();
	}

	if (mDayParametersDirty && gFrameTimeSeconds - mLastParametersChange > 1.f)
	{
		mDayParametersDirty = false;
		commitDayParametersChanges();
	}

	// While the editor floater is opened, disable all other ways to change
	// the land settings...
	bool enable = getEnabled() && mEditFloaterHandle.isDead();
	if (mLastEnabledState != enable)
	{
		mUseDefaultBtn->setEnabled(enable);
		mUseCustomBtn->setEnabled(enable);
		mUseInventoryBtn->setEnabled(enable);
		for (U32 i = 0, count = mDropTargets.size(); i < count; ++i)
		{
			mDropTargets[i]->setEnabled(enable);
		}
		mLastEnabledState = enable;
	}

	// Update the apparent time of day text every 5 seconds (meaning every 30
	// seconds of apparent day time when the day length is set to the minimum
	// of 4 hours), which is more than enough.
	if (gFrameTimeSeconds - mLastTimeOfDayUpdate > 5.f)
	{
		updateApparentTimeOfDay();
	}

	LLPanel::draw();
}

//virtual
void HBPanelLandEnvironment::refresh()
{
	bool edit_ok = false;
	if (isAgentRegion())
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		bool can_override_region = regionp->getAllowEnvironmentOverride();
		if (mIsRegion)
		{
			edit_ok = LLEnvironment::canAgentUpdateRegionEnvironment();
			mEnvOverrideCheck =
				can_override_region ||
				LLEstateInfoModel::getAllowEnvironmentOverride();
			mAllowOverrideCheck->set(mEnvOverrideCheck);
		}
		else
		{
			LLParcel* parcelp = mParcel->getParcel();
			edit_ok = parcelp && can_override_region &&
					  LLEnvironment::canAgentUpdateParcelEnvironment(parcelp);
		}
	}
	setEnabled(edit_ok);

	// Update day length and offset sliders
	if (mCurrentEnvironment)
	{
		constexpr F32 SEC2HOURS = 1.f / 3600.f;
		F32 day_length = mCurrentEnvironment->mDayLength * SEC2HOURS;
		mDayLengthSlider->setValue(day_length);
		F32 day_offset = mCurrentEnvironment->mDayOffset * SEC2HOURS;
		if (day_offset > 12.f)
		{
			day_offset -= 24.f;
		}
		mDayOffsetSlider->setValue(day_offset);
	}

	updateApparentTimeOfDay();

	if (mCurrentEnvironment)
	{
		F32 min = mAltitudesSlider->getMinValue();
		F32 max = mAltitudesSlider->getMaxValue();
		LLEnvironment::altitude_list_t alts = mCurrentEnvironment->mAltitudes;
		// Manually unrolled loop to avoid llformat() usage for slider names
		F32 altitude = llclamp(alts[1], min, max);
		mAltitudesSlider->setSliderValue("sld1", altitude);
		altitude = llclamp(alts[2], min, max);
		mAltitudesSlider->setSliderValue("sld2", altitude);
		altitude = llclamp(alts[3], min, max);
		mAltitudesSlider->setSliderValue("sld3", altitude);
	}
	updateAltitudeLabels();

	updateTrackNames();
}

void HBPanelLandEnvironment::resetOverride()
{
	mAllowOverrideCheck->set(mEnvOverrideCheck);
}

void HBPanelLandEnvironment::updateApparentTimeOfDay()
{
	mLastTimeOfDayUpdate = gFrameTimeSeconds;

	if (!mCurrentEnvironment || mCurrentEnvironment->mDayLength < 1 ||
		mCurrentEnvironment->mDayOffset < 1)
	{
		mApparentDayLengthText->setVisible(false);
		return;
	}
	mApparentDayLengthText->setVisible(true);

	S64 now = LLTimer::getEpochSeconds() + mCurrentEnvironment->mDayOffset;
	F32 percent = F32(now % mCurrentEnvironment->mDayLength) /
				  F32(mCurrentEnvironment->mDayLength);
	
	S32 seconds = S32(86400.f * percent);
	S32 hours = seconds / 3600;
	S32 minutes = (seconds - 3600 * hours) / 60;
	if (minutes == 60)
	{
		++hours;
		minutes = 0;
	}
	std::string time;
	if (minutes < 10)
	{
		time = llformat("%d:0%d", hours, minutes);
	}
	else
	{
		time = llformat("%d:%d", hours, minutes);
	}
	if (hours < 10)
	{
		time = llformat("%d%% (0%s)", (S32)(percent * 100.f),
						time.c_str());
	}
	else
	{
		time = llformat("%d%% (%s)", (S32)(percent * 100.f), time.c_str());
	}
	mApparentDayLengthText->setText(time);
}

void HBPanelLandEnvironment::updateAltitudeLabels()
{
	std::set<S32> alts;	// Using a std::set to auto-sort by value on insertion
	// Manually unrolled loop to avoid llformat() usage for slider names
	alts.insert((S32)mAltitudesSlider->getSliderValue("sld1"));
	alts.insert((S32)mAltitudesSlider->getSliderValue("sld2"));
	alts.insert((S32)mAltitudesSlider->getSliderValue("sld3"));
	std::set<S32>::iterator it = alts.begin();
	mAltitude2ValueText->setTextArg("[ALT]", llformat("%d", *it));
	mAltitude3ValueText->setTextArg("[ALT]", llformat("%d", *(++it)));
	mAltitude4ValueText->setTextArg("[ALT]", llformat("%d", *(++it)));
}

void HBPanelLandEnvironment::updateTrackNames()
{
	for (S32 i = 0, count = mDropTargets.size(); i < count; ++i)
	{
		HBSettingsDropTarget* targetp = mDropTargets[i];
		targetp->setText(getNameForTrack(i));
	}
}

std::string HBPanelLandEnvironment::getNameForTrack(S32 track)
{
	if (!mCurrentEnvironment || track < LLSettingsDay::TRACK_WATER ||
		track >= LLSettingsDay::TRACK_MAX)
	{
		return getString("empty");
	}

	std::string name;
	if (mCurrentEnvironment->mDayCycleName.empty())
	{
		name = mCurrentEnvironment->mNameList[track];
		if (name.empty() && track <= LLSettingsDay::TRACK_GROUND_LEVEL)
		{
			name = getString(mIsRegion ? "empty" : "region_env");
		}
	}
	else if (!mCurrentEnvironment->mDayCycle->isTrackEmpty(track))
	{
		name = mCurrentEnvironment->mDayCycleName;
	}
	if (name.empty())
	{
		name = getNameForTrack(track - 1);
	}
	return name;
}

void HBPanelLandEnvironment::setRegionHandle(U64 handle)
{
	mRegionHandle = handle;
	refresh();	// Refresh unconditionnally
}

LLViewerRegion* HBPanelLandEnvironment::getRegion()
{
	return mIsRegion ? gWorld.getRegionFromHandle(mRegionHandle) : NULL;
}

LLParcel* HBPanelLandEnvironment::getParcel()
{
	return !mIsRegion && mParcel ? mParcel->getParcel() : NULL;
}

S32 HBPanelLandEnvironment::getParcelId()
{
	LLParcel* parcelp = getParcel();
	return parcelp ? parcelp->getLocalID() : INVALID_PARCEL_ID;
}

bool HBPanelLandEnvironment::isAgentRegion()
{
	LLViewerRegion* selected_regionp = gViewerParcelMgr.getSelectionRegion();
	LLViewerRegion* agent_regionp = gAgent.getRegion();
	return agent_regionp &&
		   (!selected_regionp ||
			selected_regionp->getRegionID() == agent_regionp->getRegionID());
}

void HBPanelLandEnvironment::refreshFromRegion()
{
	LLHandle<LLPanel> handle = getHandle();
	gEnvironment.requestRegion([handle](S32 parcel_id,
									    LLEnvironment::envinfo_ptr_t info)
							   {
									HBPanelLandEnvironment* self =
										(HBPanelLandEnvironment*)handle.get();
									if (!self) return;
									self->onEnvironmentReceived(parcel_id, info);
							   });
}

void HBPanelLandEnvironment::refreshFromParcel()
{
	LLParcel* parcelp = getParcel();
	if (!parcelp || !isAgentRegion())
	{
		mLastParcelId = INVALID_PARCEL_ID;
		mCurrentEnvironment.reset();
		mCurEnvVersion = INVALID_PARCEL_ENVIRONMENT_VERSION;
		refresh();
		return;
	}

	// Parcel is valid; poceeed...
	mLastParcelId = parcelp->getLocalID();
	if (mCurEnvVersion < UNSET_PARCEL_ENVIRONMENT_VERSION)
	{
		// Mark as pending
		mCurEnvVersion = parcelp->getParcelEnvironmentVersion();
	}

	LLHandle<LLPanel> handle = getHandle();
	gEnvironment.requestParcel(mLastParcelId,
							   [handle](S32 parcel_id,
										LLEnvironment::envinfo_ptr_t info)
							   {
								HBPanelLandEnvironment* self =
									(HBPanelLandEnvironment*)handle.get();
								if (!self) return;
								self->onEnvironmentReceived(parcel_id, info);
							   });
}

void HBPanelLandEnvironment::onEnvironmentChanged(LLEnvironment::EEnvSelection env,
												  S32 version)
{
	if (version < INVALID_PARCEL_ENVIRONMENT_VERSION)
	{
		// This is likely a cleanup or local change; we are only interested in
		// changes sent by server, so ignore that.
		return;
	}

	// Environment comes from different sources: update callbacks, hovers
	// (causes callbacks on version change) and from personal requests.
	// Filter out duplicates and out of order packets by checking parcel
	// environment version.

	if (mIsRegion)
	{
		// 'version' should be always growing. UNSET_PARCEL_ENVIRONMENT_VERSION
		// is the backup case.
		if (env == LLEnvironment::ENV_REGION && version > mCurEnvVersion &&
			mCurEnvVersion >= UNSET_PARCEL_ENVIRONMENT_VERSION)
		{
			if (version >= UNSET_PARCEL_ENVIRONMENT_VERSION)
			{
				// Set a "pending state" to prevent re-request on following
				// onEnvironmentChanged should there be any...
				mCurEnvVersion = version;
			}
			mCurrentEnvironment.reset();
			refreshFromRegion();
		}
	}
	else if (env == LLEnvironment::ENV_PARCEL &&
			 gViewerParcelMgr.getAgentParcelId() == getParcelId())
	{
		LLParcel* parcelp = getParcel();
		if (!parcelp)
		{
			return;
		}
		// First test for parcel own settings, second for when the parcel uses
		// the region settings
		if (version > mCurEnvVersion ||
			version == UNSET_PARCEL_ENVIRONMENT_VERSION)
		{
			// Set a "pending state" to prevent re-request on following
			// onEnvironmentChanged should there be any...
			mCurEnvVersion = version;

			mCurrentEnvironment.reset();
			refreshFromParcel();
		}
		else if (mCurrentEnvironment)
		{
			refresh();	// Update the UI anyway
		}
	}
}

void HBPanelLandEnvironment::onEnvironmentReceived(S32 parcel_id,
												   LLEnvironment::envinfo_ptr_t info)
{
	if (parcel_id != getParcelId())
	{
		llwarns << "Got environment for parcel " << parcel_id
				<< " while expecting " << getParcelId() << ". Discarding."
				<< llendl;
		return;
	}
	mCurrentEnvironment = info;
	if (mCurrentEnvironment->mEnvVersion > INVALID_PARCEL_ENVIRONMENT_VERSION)
	{
		mCurEnvVersion = mCurrentEnvironment->mEnvVersion;
	}
	else
	{
		llwarns << "Environment version was not provided for " << parcel_id
				<< ". Retaining old version: " << mCurEnvVersion << llendl;
	}
	refresh();
}

void HBPanelLandEnvironment::commitDayParametersChanges()
{
	if (!mCurrentEnvironment)
	{
		return;
	}
	LLHandle<LLPanel> handle = getHandle();
	gEnvironment.updateParcel(getParcelId(), LLSettingsDay::ptr_t(),
							  mCurrentEnvironment->mDayLength,
							  mCurrentEnvironment->mDayOffset,
							  LLEnvironment::altitudes_vect_t(),
							  [handle](S32 parcel_id,
									   LLEnvironment::envinfo_ptr_t info)
							  {
								HBPanelLandEnvironment* self =
									(HBPanelLandEnvironment*)handle.get();
								if (!self) return;
								self->onEnvironmentReceived(parcel_id, info);
							  });
}

void HBPanelLandEnvironment::onChoosenItem(LLViewerInventoryItem* itemp,
										   S32 track)
{
	LLHandle<LLPanel> handle = getHandle();
	S32 day_length = -1;
	S32 day_offset = -1;
	if (mCurrentEnvironment)
	{
		day_length = mCurrentEnvironment->mDayLength;
		day_offset = mCurrentEnvironment->mDayOffset;
	}

	U32 flags = 0;
	const LLPermissions& perms = itemp->getPermissions();
	if (!perms.allowModifyBy(gAgentID))
	{
		flags |= LLSettingsBase::FLAG_NOMOD;
	}
	if (!perms.allowTransferBy(gAgentID))
	{
		flags |= LLSettingsBase::FLAG_NOTRANS;
	}

	gEnvironment.updateParcel(getParcelId(), itemp->getAssetUUID(),
							  itemp->getName(), track, 
							  day_length, day_offset, flags,
							  LLEnvironment::altitudes_vect_t(),
							  [handle](S32 parcel_id,
									   LLEnvironment::envinfo_ptr_t info)
							  {
								HBPanelLandEnvironment* self =
									(HBPanelLandEnvironment*)handle.get();
								if (!self) return;
								self->onEnvironmentReceived(parcel_id, info);
							  });
}

void HBPanelLandEnvironment::applyDayCycle(LLSettingsDay::ptr_t dayp,
										   bool edited)
{
	if (edited)
	{
		gEnvironment.clearEnvironment(LLEnvironment::ENV_EDIT);
		gEnvironment.updateEnvironment();
	}

	if (!dayp)
	{
		llwarns << "No day cycle to apply." << llendl;
		return;
	}

	if (mCurrentEnvironment && mCurrentEnvironment->mDayCycle &&
		mCurrentEnvironment->mDayCycle->getHash() == dayp->getHash())
	{
		llinfos << "No change in environment. Nothing to do." << llendl;
		// Nothing changed.
		return;
	}

	if (edited)
	{
		dayp->setName(getString("custom"));
#if 0	// This does not work, alas...
		// *FIXME: environment track names are not displaying properly after
		// using "Customized settings" when the former settings were set on
		// a per-track basis (via drag and drop or water/sky settings picker).
		for (S32 i = 0; i < LLSettingsDay::TRACK_MAX; ++i)
		{
			LLSettingsDay::cycle_track_t& track = dayp->getCycleTrack(i);
			for (LLSettingsDay::cycle_track_it_t it = track.begin(),
												 end = track.end();
				 it != end; ++it)
			{
				it->second->setName("");
			}
		}
#endif
	}

	LLHandle<LLPanel> handle = getHandle();
	S32 day_length = -1;
	S32 day_offset = -1;
	if (mCurrentEnvironment)
	{
		day_length = mCurrentEnvironment->mDayLength;
		day_offset = mCurrentEnvironment->mDayOffset;
	}
	gEnvironment.updateParcel(getParcelId(), dayp, day_length, day_offset,
							  LLEnvironment::altitudes_vect_t(),
							  [handle](S32 parcel_id,
									   LLEnvironment::envinfo_ptr_t info)
							  {
								HBPanelLandEnvironment* self =
									(HBPanelLandEnvironment*)handle.get();
								if (!self) return;
								self->onEnvironmentReceived(parcel_id, info);
							  });
}

void HBPanelLandEnvironment::loadInventoryItem(LLUUID inv_item_id)
{
	// Make sure we are not trying to edit a link and get the linked item Id
	// in that case.
	if (inv_item_id.notNull())
	{
		inv_item_id = gInventory.getLinkedItemID(inv_item_id);
	}

	if (inv_item_id.isNull())
	{
		llwarns << "Null UUID for inventory item. Aborted." << llendl;
		return;
	}

	LLViewerInventoryItem* itemp = gInventory.getItem(inv_item_id);
	if (!itemp || itemp->getIsBrokenLink())
	{
		llwarns << "Could not find inventory item: " << inv_item_id
				<< ". Aborted." << llendl;
		return;
	}

	if (!itemp->isSettingsType())
	{
		llwarns << "Inventory item " << inv_item_id
				<< " is not an environment settings item. Aborted."
				<< llendl;
		return;
	}

	LLSettingsType::EType type = itemp->getSettingsType();
	if (type != LLSettingsType::ST_DAYCYCLE)
	{
		llwarns << "Bad environment settings type for inventory item: "
				<< inv_item_id 
				<< ". Was expecting day cycle type and got type " << type
				<< ". Aborted." << llendl;
		return;
	}

	const LLUUID& asset_id = itemp->getAssetUUID();
	if (asset_id.isNull())
	{
		llwarns << "Null asset Id for inventory item: " << inv_item_id
				<< ". Aborted." << llendl;
		return;
	}

	std::string name = itemp->getName();
	LLHandle<LLPanel> handle = getHandle();
	LLEnvSettingsBase::getSettingsAsset(asset_id,
										[handle, name](LLUUID id,
													  LLSettingsBase::ptr_t settings,
													  S32 status, LLExtStat)
										{
											HBPanelLandEnvironment* self =
												(HBPanelLandEnvironment*)handle.get();
											if (!self) return;
											self->onAssetLoaded(name, id,
																settings,
																status);
										});
}

void HBPanelLandEnvironment::onAssetLoaded(const std::string& name,
										   const LLUUID& asset_id,
										   LLSettingsBase::ptr_t settings,
										   S32 status)
{
	if (!settings || status)
	{
		gNotifications.add("CantFindInvItem");
		return;
	}

	LLHandle<LLPanel> handle = getHandle();
	LLEnvSettingsDay::buildFromOtherSetting(settings,
											[handle, name](LLSettingsDay::ptr_t dayp)
											{
												HBPanelLandEnvironment* self =
													(HBPanelLandEnvironment*)handle.get();
												if (!self || !dayp) return;
												dayp->setName(name);
												self->applyDayCycle(dayp);
											});
}

bool HBPanelLandEnvironment::closeEditFloater(bool force)
{
	HBFloaterEditEnvSettings* floaterp =
		(HBFloaterEditEnvSettings*)mEditFloaterHandle.get();
	if (floaterp)
	{
		if (!force && floaterp->isDirty())
		{
			return false;
		}
		if (mCommitConnection.connected())
		{
			mCommitConnection.disconnect();
		}
		floaterp->close();
	}
	return true;
}

//static
void HBPanelLandEnvironment::onBtnDefault(void* userdata)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	if (!self) return;

	LLHandle<LLPanel> handle = self->getHandle();
	gEnvironment.resetParcel(self->getParcelId(),
							 [handle](S32 parcel_id,
									  LLEnvironment::envinfo_ptr_t info)
							  {
								HBPanelLandEnvironment* panelp =
									(HBPanelLandEnvironment*)handle.get();
								if (!panelp) return;
								panelp->onEnvironmentReceived(parcel_id, info);
							  });
}

//static
void HBPanelLandEnvironment::invPickerCallback(const std::vector<std::string>&,
											   const uuid_vec_t& ids,
											   void* userdata, bool)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	if (self && !ids.empty())
	{
		self->loadInventoryItem(ids[0]);
	}
}

//static
void HBPanelLandEnvironment::onBtnInventory(void* userdata)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	if (!self) return;

	HBFloaterInvItemsPicker* pickerp =
		new HBFloaterInvItemsPicker(self, invPickerCallback, self);
	if (pickerp)
	{
		pickerp->setAssetType(LLAssetType::AT_SETTINGS,
							  LLSettingsType::ST_DAYCYCLE);
	}
}

//static
void HBPanelLandEnvironment::onBtnCustom(void* userdata)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	if (!self) return;

	if (!self->closeEditFloater())
	{
		llwarns << "Editing in progress with unsaved changes. Aborting."
				<< llendl;
		return;
	}

	HBFloaterEditEnvSettings* floaterp =
		HBFloaterEditEnvSettings::create(LLSettingsType::ST_DAYCYCLE);
	if (!floaterp) return;

	self->mEditFloaterHandle = floaterp->getHandle();

	if (self->mIsRegion)
	{
		floaterp->setEditContextRegion();
	}
	else
	{
		floaterp->setEditContextParcel();
	}
	if (self->mCurrentEnvironment && self->mCurrentEnvironment->mDayCycle)
	{
		floaterp->setSettings(self->mCurrentEnvironment->mDayCycle);
	}
	else
	{
		floaterp->loadDefaultSettings();
	}

	S32 day_length = 4 * 3600;	// Four hours by default
	if (self->mCurrentEnvironment)
	{
		day_length = self->mCurrentEnvironment->mDayLength;
	}
	floaterp->setDayLength(day_length);

	LLHandle<LLPanel> handle = self->getHandle();
	self->mCommitConnection =
		floaterp->setCommitCB([handle](LLSettingsBase::ptr_t settings)
							  {
								HBPanelLandEnvironment* panelp =
									(HBPanelLandEnvironment*)handle.get();
								if (!panelp || !settings) return;
								LLSettingsDay::ptr_t dayp =
									boost::static_pointer_cast<LLSettingsDay>(settings);
								panelp->applyDayCycle(dayp, true);
							  });
}

//static
void HBPanelLandEnvironment::onBtnReset(void* userdata)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	if (!self) return;

	LLHandle<LLPanel> handle = self->getHandle();
	LLEnvironment::altitudes_vect_t alts;
	alts.push_back(1000.f);
	alts.push_back(2000.f);
	alts.push_back(3000.f);
	S32 day_length = -1;
	S32 day_offset = -1;
	if (self->mCurrentEnvironment)
	{
		day_length = self->mCurrentEnvironment->mDayLength;
		day_offset = self->mCurrentEnvironment->mDayOffset;
	}
	gEnvironment.updateParcel(self->getParcelId(), LLSettingsDay::ptr_t(),
							  day_length, day_offset, alts,
							  [handle](S32 parcel_id,
									   LLEnvironment::envinfo_ptr_t info)
							  {
								HBPanelLandEnvironment* panelp =
									(HBPanelLandEnvironment*)handle.get();
								if (!panelp) return;
								panelp->onEnvironmentReceived(parcel_id, info);
							  });
}

//static
void HBPanelLandEnvironment::onAllowOverride(LLUICtrl* ctrl, void* userdata)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!ctrl || !self)
	{
		return;
	}
	LLViewerRegion* regionp = self->getRegion();
	if (regionp)
	{
		bool allow = check->get();
		self->mEnvOverrideCheck = !allow;	// Old value
		LLEstateInfoModel::setAllowEnvironmentOverride(allow);
		LLPanelEstateInfo* panelp = LLFloaterRegionInfo::getPanelEstate();
		if (panelp)
		{
			panelp->sendUpdate();
		}
	}
}

//static
void HBPanelLandEnvironment::onDayParametersChanged(LLUICtrl*, void* userdata)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	if (self)
	{
		self->mDayParametersDirty = true;
		self->mLastParametersChange = gFrameTimeSeconds;
		if (self->mCurrentEnvironment)
		{
			self->mCurrentEnvironment->mDayLength =
				self->mDayLengthSlider->getValueF32() * 3600.f;
			F32 day_offset = self->mDayOffsetSlider->getValueF32();
			if (day_offset <= 0.f)
			{
				day_offset += 24.f;
			}
			self->mCurrentEnvironment->mDayOffset = day_offset * 3600.f;
			self->updateApparentTimeOfDay();
		}
	}
}

//static
void HBPanelLandEnvironment::onAltSliderCommit(LLUICtrl* ctrl, void* userdata)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	if (self)
	{
		self->updateAltitudeLabels();
	}
}

//static
void HBPanelLandEnvironment::onAltSliderMouseUp(S32, S32, void* userdata)
{
	HBPanelLandEnvironment* self = (HBPanelLandEnvironment*)userdata;
	if (!self || !self->mIsRegion)
	{
		return;
	}

	std::set<S32> alts;	// Using a std::set to auto-sort by value on insertion
	// Manually unrolled loop to avoid llformat() usage for slider names
	alts.insert((S32)self->mAltitudesSlider->getSliderValue("sld1"));
	alts.insert((S32)self->mAltitudesSlider->getSliderValue("sld2"));
	alts.insert((S32)self->mAltitudesSlider->getSliderValue("sld3"));

	// Push the sorted values into the altitudes vector
	std::set<S32>::iterator it = alts.begin();
	LLEnvironment::altitudes_vect_t alts_vec;
	alts_vec.push_back(*it);
	alts_vec.push_back(*(++it));
	alts_vec.push_back(*(++it));

	S32 day_length = -1;
	S32 day_offset = -1;
	if (self->mCurrentEnvironment)
	{
		day_length = self->mCurrentEnvironment->mDayLength;
		day_offset = self->mCurrentEnvironment->mDayOffset;
	}

	gEnvironment.updateParcel(self->getParcelId(), LLSettingsDay::ptr_t(),
							  day_length, day_offset, alts_vec);
}
