/**
 * @file llpanelenvsettings.cpp
 * @brief Environment settings panel classes implementation
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 *
 * Copyright (c) 2018, Linden Research, Inc, (c) 2019-2023 Henri Beauchamp.
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

#include "llpanelenvsettings.h"

#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "llfloater.h"
#include "llmultisliderctrl.h"
#include "llnotifications.h"
#include "llradiogroup.h"
#include "llsliderctrl.h"
#include "lluictrlfactory.h"
#include "llvirtualtrackball.h"
#include "llxyvector.h"

#include "llagent.h"				// For gAgentID
#include "llappviewer.h"			// For gDisconnected and gFrameTimeSeconds
#include "llcolorswatch.h"
#include "llenvsettings.h"
#include "hbfloaterinvitemspicker.h"
#include "llinventorymodel.h"
#include "lllocalbitmaps.h"
#include "lltexturectrl.h"
#include "llviewercontrol.h"
#include "llwlskyparammgr.h"

///////////////////////////////////////////////////////////////////////////////
// LLPanelEnvSettingsSky class
///////////////////////////////////////////////////////////////////////////////

constexpr F32 SLIDER_SCALE_SUN_AMBIENT = 3.f;
constexpr F32 SLIDER_SCALE_BLUE_HORIZON_DENSITY = 2.f;
constexpr F32 SLIDER_SCALE_GLOW_R = 20.f;
constexpr F32 SLIDER_SCALE_GLOW_B = -5.f;
constexpr F32 SLIDER_SCALE_DENSITY_MULTIPLIER = 0.001f;

LLPanelEnvSettingsSky::LLPanelEnvSettingsSky()
:	LLPanelEnvSettings()
{
	LLEnvironment::addBeaconsUser();
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_settings_sky.xml");
}

//virtual
LLPanelEnvSettingsSky::~LLPanelEnvSettingsSky()
{
	LLEnvironment::delBeaconsUser();
}

//virtual
bool LLPanelEnvSettingsSky::postBuild()
{
	mAmbientColorCtrl = getChild<LLColorSwatchCtrl>("ambient_light");
	mAmbientColorCtrl->setCommitCallback(onAmbientLightChanged);
	mAmbientColorCtrl->setCallbackUserData(this);

	mBlueHorizonColorCtrl = getChild<LLColorSwatchCtrl>("blue_horizon");
	mBlueHorizonColorCtrl->setCommitCallback(onBlueHorizonChanged);
	mBlueHorizonColorCtrl->setCallbackUserData(this);

	mBlueDensityColorCtrl = getChild<LLColorSwatchCtrl>("blue_density");
	mBlueDensityColorCtrl->setCommitCallback(onBlueDensityChanged);
	mBlueDensityColorCtrl->setCallbackUserData(this);

	mHazeHorizonCtrl = getChild<LLSliderCtrl>("haze_horizon");
	mHazeHorizonCtrl->setCommitCallback(onHazeHorizonChanged);
	mHazeHorizonCtrl->setCallbackUserData(this);

	mHazeDensityCtrl = getChild<LLSliderCtrl>("haze_density");
	mHazeDensityCtrl->setCommitCallback(onHazeDensityChanged);
	mHazeDensityCtrl->setCallbackUserData(this);

	mMoistureLevelCtrl = getChild<LLSliderCtrl>("moisture_level");
	mMoistureLevelCtrl->setCommitCallback(onMoistureLevelChanged);
	mMoistureLevelCtrl->setCallbackUserData(this);

	mDropletRadiusCtrl = getChild<LLSliderCtrl>("droplet_radius");
	mDropletRadiusCtrl->setCommitCallback(onDropletRadiusChanged);
	mDropletRadiusCtrl->setCallbackUserData(this);

	mIceLevelCtrl = getChild<LLSliderCtrl>("ice_level");
	mIceLevelCtrl->setCommitCallback(onIceLevelChanged);
	mIceLevelCtrl->setCallbackUserData(this);

	mSceneGammaCtrl = getChild<LLSliderCtrl>("scene_gamma");
	mSceneGammaCtrl->setCommitCallback(onSceneGammaChanged);
	mSceneGammaCtrl->setCallbackUserData(this);

	mDensityMultCtrl = getChild<LLSliderCtrl>("density_mult");
	mDensityMultCtrl->setCommitCallback(onDensityMultipChanged);
	mDensityMultCtrl->setCallbackUserData(this);

	mDistanceMultCtrl = getChild<LLSliderCtrl>("distance_mult");
	mDistanceMultCtrl->setCommitCallback(onDistanceMultipChanged);
	mDistanceMultCtrl->setCallbackUserData(this);

	mMaxAltitudeCtrl = getChild<LLSliderCtrl>("max_alt");
	mMaxAltitudeCtrl->setCommitCallback(onMaxAltChanged);
	mMaxAltitudeCtrl->setCallbackUserData(this);

	mUseProbeAmbianceCtrl = getChild<LLCheckBoxCtrl>("probe_ambiance_enable");
	mUseProbeAmbianceCtrl->setCommitCallback(onProbeAmbianceChanged);
	mUseProbeAmbianceCtrl->setCallbackUserData(this);

	mProbeAmbianceCtrl = getChild<LLSliderCtrl>("probe_ambiance");
	mProbeAmbianceCtrl->setCommitCallback(onProbeAmbianceChanged);
	mProbeAmbianceCtrl->setCallbackUserData(this);

	mHdrAutoText = getChild<LLTextBox>("hdr_auto_text");
	mHdrOffText = getChild<LLTextBox>("hdr_never_text");
	mHdrOnText = getChild<LLTextBox>("hdr_always_text");

	mCloudColorCtrl = getChild<LLColorSwatchCtrl>("cloud_color");
	mCloudColorCtrl->setCommitCallback(onCloudColorChanged);
	mCloudColorCtrl->setCallbackUserData(this);

	mCloudMapCtrl = getChild<LLTextureCtrl>("cloud_map");
	mCloudMapCtrl->setDefaultImageAssetID(LLSettingsSky::getDefaultCloudNoiseTextureId());
	mCloudMapCtrl->setCommitCallback(onCloudMapChanged);
	mCloudMapCtrl->setCallbackUserData(this);

	mCloudCoverageCtrl = getChild<LLSliderCtrl>("cloud_coverage");
	mCloudCoverageCtrl->setCommitCallback(onCloudCoverageChanged);
	mCloudCoverageCtrl->setCallbackUserData(this);

	mCloudScaleCtrl = getChild<LLSliderCtrl>("cloud_scale");
	mCloudScaleCtrl->setCommitCallback(onCloudScaleChanged);
	mCloudScaleCtrl->setCallbackUserData(this);

	mCloudVarianceCtrl = getChild<LLSliderCtrl>("cloud_variance");
	mCloudVarianceCtrl->setCommitCallback(onCloudVarianceChanged);
	mCloudVarianceCtrl->setCallbackUserData(this);

	mCloudScrollCtrl = getChild<LLXYVector>("cloud_scroll_xy");
	mCloudScrollCtrl->setCommitCallback(onCloudScrollChanged);
	mCloudScrollCtrl->setCallbackUserData(this);

	mCloudDensityXCtrl = getChild<LLSliderCtrl>("cloud_density_x");
	mCloudDensityXCtrl->setCommitCallback(onCloudDensityChanged);
	mCloudDensityXCtrl->setCallbackUserData(this);

	mCloudDensityYCtrl = getChild<LLSliderCtrl>("cloud_density_y");
	mCloudDensityYCtrl->setCommitCallback(onCloudDensityChanged);
	mCloudDensityYCtrl->setCallbackUserData(this);

	mCloudDensityDCtrl = getChild<LLSliderCtrl>("cloud_density_d");
	mCloudDensityDCtrl->setCommitCallback(onCloudDensityChanged);
	mCloudDensityDCtrl->setCallbackUserData(this);

	mCloudDetailXCtrl = getChild<LLSliderCtrl>("cloud_detail_x");
	mCloudDetailXCtrl->setCommitCallback(onCloudDetailChanged);
	mCloudDetailXCtrl->setCallbackUserData(this);

	mCloudDetailYCtrl = getChild<LLSliderCtrl>("cloud_detail_y");
	mCloudDetailYCtrl->setCommitCallback(onCloudDetailChanged);
	mCloudDetailYCtrl->setCallbackUserData(this);

	mCloudDetailDCtrl = getChild<LLSliderCtrl>("cloud_detail_d");
	mCloudDetailDCtrl->setCommitCallback(onCloudDetailChanged);
	mCloudDetailDCtrl->setCallbackUserData(this);

	mSunRotationCtrl = getChild<LLVirtualTrackball>("sun_rotation");
	mSunRotationCtrl->setCommitCallback(onSunRotationChanged);
	mSunRotationCtrl->setCallbackUserData(this);

	mMoonRotationCtrl = getChild<LLVirtualTrackball>("moon_rotation");
	mMoonRotationCtrl->setCommitCallback(onMoonRotationChanged);
	mMoonRotationCtrl->setCallbackUserData(this);

	mSunImageCtrl = getChild<LLTextureCtrl>("sun_image");
	const LLUUID& blank_sun_id = LLSettingsSky::getBlankSunTextureId();
	mSunImageCtrl->setBlankImageAssetID(blank_sun_id);
	mSunImageCtrl->setDefaultImageAssetID(blank_sun_id);
	mSunImageCtrl->setCommitCallback(onSunImageChanged);
	mSunImageCtrl->setCallbackUserData(this);

	mMoonImageCtrl = getChild<LLTextureCtrl>("moon_image");
	const LLUUID& default_moon_id = LLSettingsSky::getDefaultMoonTextureId();
	mMoonImageCtrl->setBlankImageAssetID(default_moon_id);
	mMoonImageCtrl->setDefaultImageAssetID(default_moon_id);
	mMoonImageCtrl->setCommitCallback(onMoonImageChanged);
	mMoonImageCtrl->setCallbackUserData(this);

	mSunLightColorCtrl = getChild<LLColorSwatchCtrl>("sun_light_color");
	mSunLightColorCtrl->setCommitCallback(onSunlightColorChanged);
	mSunLightColorCtrl->setCallbackUserData(this);

	mSunScaleCtrl = getChild<LLSliderCtrl>("sun_scale");
	mSunScaleCtrl->setCommitCallback(onSunScaleChanged);
	mSunScaleCtrl->setCallbackUserData(this);

	mMoonScaleCtrl = getChild<LLSliderCtrl>("moon_scale");
	mMoonScaleCtrl->setCommitCallback(onMoonScaleChanged);
	mMoonScaleCtrl->setCallbackUserData(this);

	mGlowFocusCtrl = getChild<LLSliderCtrl>("glow_focus");
	mGlowFocusCtrl->setCommitCallback(onGlowChanged);
	mGlowFocusCtrl->setCallbackUserData(this);

	mGlowSizeCtrl = getChild<LLSliderCtrl>("glow_size");
	mGlowSizeCtrl->setCommitCallback(onGlowChanged);
	mGlowSizeCtrl->setCallbackUserData(this);

	mMoonBrightnessCtrl = getChild<LLSliderCtrl>("moon_brightness");
	mMoonBrightnessCtrl->setCommitCallback(onMoonBrightnessChanged);
	mMoonBrightnessCtrl->setCallbackUserData(this);

	mStarBrightnessCtrl = getChild<LLSliderCtrl>("star_brightness");
	mStarBrightnessCtrl->setCommitCallback(onStarBrightnessChanged);
	mStarBrightnessCtrl->setCallbackUserData(this);

	refresh();

	return true;
}

//virtual
void LLPanelEnvSettingsSky::setEnabled(bool enabled)
{
	mAmbientColorCtrl->setEnabled(enabled);
	mBlueHorizonColorCtrl->setEnabled(enabled);
	mBlueDensityColorCtrl->setEnabled(enabled);
	mCloudColorCtrl->setEnabled(enabled);
	mSunLightColorCtrl->setEnabled(enabled);
	mHazeHorizonCtrl->setEnabled(enabled);
	mHazeDensityCtrl->setEnabled(enabled);
	mMoistureLevelCtrl->setEnabled(enabled);
	mDropletRadiusCtrl->setEnabled(enabled);
	mIceLevelCtrl->setEnabled(enabled);
	mSceneGammaCtrl->setEnabled(enabled);
	mDensityMultCtrl->setEnabled(enabled);
	mDistanceMultCtrl->setEnabled(enabled);
	mMaxAltitudeCtrl->setEnabled(enabled);
	mUseProbeAmbianceCtrl->setEnabled(enabled);
	mProbeAmbianceCtrl->setEnabled(enabled);
	mCloudCoverageCtrl->setEnabled(enabled);
	mCloudScaleCtrl->setEnabled(enabled);
	mCloudVarianceCtrl->setEnabled(enabled);
	mCloudDensityXCtrl->setEnabled(enabled);
	mCloudDensityYCtrl->setEnabled(enabled);
	mCloudDensityDCtrl->setEnabled(enabled);
	mCloudDetailXCtrl->setEnabled(enabled);
	mCloudDetailYCtrl->setEnabled(enabled);
	mCloudDetailDCtrl->setEnabled(enabled);
	mSunScaleCtrl->setEnabled(enabled);
	mMoonScaleCtrl->setEnabled(enabled);
	mGlowFocusCtrl->setEnabled(enabled);
	mGlowSizeCtrl->setEnabled(enabled);
	mMoonBrightnessCtrl->setEnabled(enabled);
	mStarBrightnessCtrl->setEnabled(enabled);
	mCloudMapCtrl->setEnabled(enabled);
	mSunImageCtrl->setEnabled(enabled);
	mMoonImageCtrl->setEnabled(enabled);
	mSunRotationCtrl->setEnabled(enabled);
	mMoonRotationCtrl->setEnabled(enabled);
	mCloudScrollCtrl->setEnabled(enabled);
	mHdrAutoText->setEnabled(enabled);
	mHdrOffText->setEnabled(enabled);
	mHdrOnText->setEnabled(enabled);

	LLPanel::setEnabled(enabled);
}

//virtual
void LLPanelEnvSettingsSky::refresh()
{
	if (!mSkySettings || !canEdit())
	{
		setEnabled(false);
		return;
	}

	setEnabled(true);

	mAmbientColorCtrl->set(mSkySettings->getAmbientColor() /
						   SLIDER_SCALE_SUN_AMBIENT);
	mBlueHorizonColorCtrl->set(mSkySettings->getBlueHorizon() /
							   SLIDER_SCALE_BLUE_HORIZON_DENSITY);
	mBlueDensityColorCtrl->set(mSkySettings->getBlueDensity() /
							   SLIDER_SCALE_BLUE_HORIZON_DENSITY);
	mHazeHorizonCtrl->setValue(mSkySettings->getHazeHorizon());
	mHazeDensityCtrl->setValue(mSkySettings->getHazeDensity());
	mMoistureLevelCtrl->setValue(mSkySettings->getSkyMoistureLevel());
	mDropletRadiusCtrl->setValue(mSkySettings->getSkyDropletRadius());
	mIceLevelCtrl->setValue(mSkySettings->getSkyIceLevel());
	mSceneGammaCtrl->setValue(mSkySettings->getGamma());
	mDensityMultCtrl->setValue(mSkySettings->getDensityMultiplier() /
							   SLIDER_SCALE_DENSITY_MULTIPLIER);
	mDistanceMultCtrl->setValue(mSkySettings->getDistanceMultiplier());
	mMaxAltitudeCtrl->setValue(mSkySettings->getMaxY());

	if (mSkySettings->canAutoAdjust())
	{
		mUseProbeAmbianceCtrl->set(false);
		mProbeAmbianceCtrl->setValue(0.f);
		mProbeAmbianceCtrl->setEnabled(false);
		mHdrAutoText->setVisible(true);
		mHdrOffText->setVisible(false);
		mHdrOnText->setVisible(false);
	}
	else
	{
		mUseProbeAmbianceCtrl->set(true);
		F32 probe_ambiance = mSkySettings->getReflectionProbeAmbiance();
		mProbeAmbianceCtrl->setValue(probe_ambiance);
		mProbeAmbianceCtrl->setEnabled(true);
		bool hdr_off = probe_ambiance == 0.f;
		mHdrAutoText->setVisible(false);
		mHdrOffText->setVisible(hdr_off);
		mHdrOnText->setVisible(!hdr_off);
	}

	mCloudColorCtrl->set(mSkySettings->getCloudColor());
	mCloudCoverageCtrl->setValue(mSkySettings->getCloudShadow());
	mCloudScaleCtrl->setValue(mSkySettings->getCloudScale());
	mCloudVarianceCtrl->setValue(mSkySettings->getCloudVariance());
	LLVector2 scroll = mSkySettings->getCloudScrollRate();
	mCloudScrollCtrl->setValue(scroll.mV[VX], scroll.mV[VY]);
	mCloudMapCtrl->setImageAssetID(mSkySettings->getCloudNoiseTextureId());

	LLColor3 density = mSkySettings->getCloudPosDensity1();
	mCloudDensityXCtrl->setValue(density.mV[0]);
	mCloudDensityYCtrl->setValue(density.mV[1]);
	mCloudDensityDCtrl->setValue(density.mV[2]);

	LLColor3 detail = mSkySettings->getCloudPosDensity2();
	mCloudDetailXCtrl->setValue(detail.mV[0]);
	mCloudDetailYCtrl->setValue(detail.mV[1]);
	mCloudDetailDCtrl->setValue(detail.mV[2]);

	mSunRotationCtrl->setRotation(mSkySettings->getSunRotation());
	mMoonRotationCtrl->setRotation(mSkySettings->getMoonRotation());
	mSunImageCtrl->setImageAssetID(mSkySettings->getSunTextureId());
	mMoonImageCtrl->setImageAssetID(mSkySettings->getMoonTextureId());
	mSunLightColorCtrl->set(mSkySettings->getSunlightColor() /
							SLIDER_SCALE_SUN_AMBIENT);
	mSunScaleCtrl->setValue(mSkySettings->getSunScale());
	mMoonScaleCtrl->setValue(mSkySettings->getMoonScale());
	LLColor3 glow = mSkySettings->getGlow();
	mGlowFocusCtrl->setValue(glow.mV[2] / SLIDER_SCALE_GLOW_B);
	mGlowSizeCtrl->setValue(2.f - glow.mV[0] / SLIDER_SCALE_GLOW_R);
	mMoonBrightnessCtrl->setValue(mSkySettings->getMoonBrightness());
	mStarBrightnessCtrl->setValue(mSkySettings->getStarBrightness());

	LLPanel::refresh();
}

//virtual
void LLPanelEnvSettingsSky::setSettings(const LLSettingsBase::ptr_t& settings,
										bool reset_dirty)
{
	setSky(std::static_pointer_cast<LLSettingsSky>(settings), reset_dirty);
}

void LLPanelEnvSettingsSky::setSky(const LLSettingsSky::ptr_t& settings,
								   bool reset_dirty)
{
	mSkySettings = settings;
	if (reset_dirty)
	{
		setDirty(false);
	}
	refresh();
}

void LLPanelEnvSettingsSky::updateSettings()
{
	if (mSkySettings)
	{
		mSkySettings->update();
		setDirty();
		gWLSkyParamMgr.setDirty(); 
	}
}

//virtual
LLSettingsBase::ptr_t LLPanelEnvSettingsSky::getSettingsClone()
{
	LLSettingsBase::ptr_t clone;
	if (mSkySettings)
	{
		clone = mSkySettings->buildClone();
	}
	return clone;
}

//virtual
bool LLPanelEnvSettingsSky::hasLocalTextures(LLSD& args)
{
	if (mSkySettings)
	{
		if (LLLocalBitmap::isLocal(mSkySettings->getSunTextureId()))
		{
			args["FIELD"] = getString("sun");
			return true;
		}
		if (LLLocalBitmap::isLocal(mSkySettings->getMoonTextureId()))
		{
			args["FIELD"] = getString("moon");
			return true;
		}
		if (LLLocalBitmap::isLocal(mSkySettings->getCloudNoiseTextureId()))
		{
			args["FIELD"] = getString("cloudnoise");
			return true;
		}
		if (LLLocalBitmap::isLocal(mSkySettings->getBloomTextureId()))
		{
			args["FIELD"] = getString("bloom");
			return true;
		}
	}
	return false;
}

//virtual
void LLPanelEnvSettingsSky::updateEditEnvironment()
{
	if (mSkySettings)
	{
		gEnvironment.setEnvironment(LLEnvironment::ENV_EDIT, mSkySettings);
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_FAST);
	}
}

//virtual
void LLPanelEnvSettingsSky::updateLocal()
{
	if (mSkySettings)
	{
		gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, mSkySettings);
	}
}

//virtual
void LLPanelEnvSettingsSky::updateParcel(S32 parcel_id)
{
	if (mSkySettings)
	{
		gEnvironment.updateParcel(parcel_id, mSkySettings, -1, -1);
	}
}

//virtual
void LLPanelEnvSettingsSky::updateRegion()
{
	if (mSkySettings)
	{
		gEnvironment.updateRegion(mSkySettings, -1, -1);
	}
}

//static
void LLPanelEnvSettingsSky::onAmbientLightChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setAmbientColor(LLColor3(self->mAmbientColorCtrl->get() *
												 SLIDER_SCALE_SUN_AMBIENT));
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onBlueHorizonChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setBlueHorizon(LLColor3(self->mBlueHorizonColorCtrl->get() *
												SLIDER_SCALE_BLUE_HORIZON_DENSITY));
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onBlueDensityChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setBlueDensity(LLColor3(self->mBlueDensityColorCtrl->get() *
												SLIDER_SCALE_BLUE_HORIZON_DENSITY));
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onHazeHorizonChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setHazeHorizon(self->mHazeHorizonCtrl->getValueF32());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onHazeDensityChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setHazeDensity(self->mHazeDensityCtrl->getValueF32());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onMoistureLevelChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 value = self->mMoistureLevelCtrl->getValueF32();
	self->mSkySettings->setSkyMoistureLevel(value);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onDropletRadiusChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 value = self->mDropletRadiusCtrl->getValueF32();
	self->mSkySettings->setSkyDropletRadius(value);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onIceLevelChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setSkyIceLevel(self->mIceLevelCtrl->getValueF32());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onSceneGammaChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setGamma(self->mSceneGammaCtrl->getValueF32());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onDensityMultipChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 value = self->mDensityMultCtrl->getValueF32() *
				SLIDER_SCALE_DENSITY_MULTIPLIER;
	self->mSkySettings->setDensityMultiplier(value);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onDistanceMultipChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 value = self->mDistanceMultCtrl->getValueF32();
	self->mSkySettings->setDistanceMultiplier(value);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onMaxAltChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setMaxY(self->mMaxAltitudeCtrl->getValueF32());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onProbeAmbianceChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	if (self->mUseProbeAmbianceCtrl->get())
	{
		F32 probe_ambiance = self->mProbeAmbianceCtrl->getValueF32();
		self->mSkySettings->setReflectionProbeAmbiance(probe_ambiance);
	}
	else
	{
		self->mSkySettings->removeProbeAmbiance();
	}
	self->updateSettings();
	self->refresh();
}

//static
void LLPanelEnvSettingsSky::onCloudColorChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setCloudColor(LLColor3(self->mCloudColorCtrl->get()));
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onCloudMapChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	const LLUUID& map_id = self->mCloudMapCtrl->getImageAssetID();
	self->mSkySettings->setCloudNoiseTextureId(map_id);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onCloudCoverageChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 value = self->mCloudCoverageCtrl->getValueF32();
	self->mSkySettings->setCloudShadow(value);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onCloudScaleChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setCloudScale(self->mCloudScaleCtrl->getValueF32());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onCloudVarianceChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 value = self->mCloudVarianceCtrl->getValueF32();
	self->mSkySettings->setCloudVariance(value);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onCloudScrollChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	LLVector2 vect(self->mCloudScrollCtrl->getValue());
	self->mSkySettings->setCloudScrollRate(vect);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onCloudDensityChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 x = self->mCloudDensityXCtrl->getValueF32();
	F32 y = self->mCloudDensityYCtrl->getValueF32();
	F32 z = self->mCloudDensityDCtrl->getValueF32();
	self->mSkySettings->setCloudPosDensity1(LLColor3(x, y, z));
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onCloudDetailChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 x = self->mCloudDetailXCtrl->getValueF32();
	F32 y = self->mCloudDetailYCtrl->getValueF32();
	F32 z = self->mCloudDetailDCtrl->getValueF32();
	self->mSkySettings->setCloudPosDensity2(LLColor3(x, y, z));
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onSunRotationChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setSunRotation(self->mSunRotationCtrl->getRotation());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onMoonRotationChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setMoonRotation(self->mMoonRotationCtrl->getRotation());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onSunImageChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	const LLUUID& image_id = self->mSunImageCtrl->getImageAssetID();
	self->mSkySettings->setSunTextureId(image_id);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onMoonImageChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	const LLUUID& image_id = self->mMoonImageCtrl->getImageAssetID();
	self->mSkySettings->setMoonTextureId(image_id);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onSunlightColorChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	LLColor3 color(self->mSunLightColorCtrl->get());
	self->mSkySettings->setSunlightColor(color * SLIDER_SCALE_SUN_AMBIENT);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onSunScaleChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setSunScale(self->mSunScaleCtrl->getValueF32());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onMoonScaleChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	self->mSkySettings->setMoonScale(self->mMoonScaleCtrl->getValueF32());
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onGlowChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	// Turns [0.0-1.99] UI range to [40.0-0.2] range
	F32 x = (2.f - self->mGlowSizeCtrl->getValueF32()) * SLIDER_SCALE_GLOW_R;
	F32 z = self->mGlowFocusCtrl->getValueF32() * SLIDER_SCALE_GLOW_B;
	self->mSkySettings->setGlow(LLColor3(x, 0.f, z));
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onMoonBrightnessChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 value = self->mMoonBrightnessCtrl->getValueF32();
	self->mSkySettings->setMoonBrightness(value);
	self->updateSettings();
}

//static
void LLPanelEnvSettingsSky::onStarBrightnessChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsSky* self = (LLPanelEnvSettingsSky*)userdata;
	if (!self || !self->mSkySettings) return;	// Paranoia

	F32 value = self->mStarBrightnessCtrl->getValueF32();
	self->mSkySettings->setStarBrightness(value);
	self->updateSettings();
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelEnvSettingsWater class
///////////////////////////////////////////////////////////////////////////////

LLPanelEnvSettingsWater::LLPanelEnvSettingsWater()
:	LLPanelEnvSettings()
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_settings_water.xml");
}

//virtual
bool LLPanelEnvSettingsWater::postBuild()
{
	mFogColorCtrl = getChild<LLColorSwatchCtrl>("water_fog_color");
	mFogColorCtrl->setCommitCallback(onFogColorChanged);
	mFogColorCtrl->setCallbackUserData(this);

	mFogDensityCtrl = getChild<LLSliderCtrl>("water_fog_density");
	mFogDensityCtrl->setCommitCallback(onFogDensityChanged);
	mFogDensityCtrl->setCallbackUserData(this);

	mUnderwaterModCtrl = getChild<LLSliderCtrl>("water_underwater_mod");
	mUnderwaterModCtrl->setCommitCallback(onFogUnderWaterChanged);
	mUnderwaterModCtrl->setCallbackUserData(this);

	mFresnelScaleCtrl = getChild<LLSliderCtrl>("water_fresnel_scale");
	mFresnelScaleCtrl->setCommitCallback(onFresnelScaleChanged);
	mFresnelScaleCtrl->setCallbackUserData(this);

	mFresnelOffsetCtrl = getChild<LLSliderCtrl>("water_fresnel_offset");
	mFresnelOffsetCtrl->setCommitCallback(onFresnelOffsetChanged);
	mFresnelOffsetCtrl->setCallbackUserData(this);

	mNormalMapCtrl = getChild<LLTextureCtrl>("water_normal_map");
	mNormalMapCtrl->setDefaultImageAssetID(LLSettingsWater::getDefaultWaterNormalAssetId());
	mNormalMapCtrl->setCommitCallback(onNormalMapChanged);
	mNormalMapCtrl->setCallbackUserData(this);

	mNormalScaleXCtrl = getChild<LLSliderCtrl>("water_normal_scale_x");
	mNormalScaleXCtrl->setCommitCallback(onNormalScaleChanged);
	mNormalScaleXCtrl->setCallbackUserData(this);

	mNormalScaleYCtrl = getChild<LLSliderCtrl>("water_normal_scale_y");
	mNormalScaleYCtrl->setCommitCallback(onNormalScaleChanged);
	mNormalScaleYCtrl->setCallbackUserData(this);

	mNormalScaleZCtrl = getChild<LLSliderCtrl>("water_normal_scale_z");
	mNormalScaleZCtrl->setCommitCallback(onNormalScaleChanged);
	mNormalScaleZCtrl->setCallbackUserData(this);

	mLargeWaveCtrl = getChild<LLXYVector>("large_wave_xy");
	mLargeWaveCtrl->setCommitCallback(onLargeWaveChanged);
	mLargeWaveCtrl->setCallbackUserData(this);

	mSmallWaveCtrl = getChild<LLXYVector>("small_wave_xy");
	mSmallWaveCtrl->setCommitCallback(onSmallWaveChanged);
	mSmallWaveCtrl->setCallbackUserData(this);

	mRefractionAboveCtrl = getChild<LLSliderCtrl>("water_scale_above");
	mRefractionAboveCtrl->setCommitCallback(onScaleAboveChanged);
	mRefractionAboveCtrl->setCallbackUserData(this);

	mRefractionBelowCtrl = getChild<LLSliderCtrl>("water_scale_below");
	mRefractionBelowCtrl->setCommitCallback(onScaleBelowChanged);
	mRefractionBelowCtrl->setCallbackUserData(this);

	mBlurMultiplierCtrl = getChild<LLSliderCtrl>("water_blur_mult");
	mBlurMultiplierCtrl->setCommitCallback(onBlurMultChanged);
	mBlurMultiplierCtrl->setCallbackUserData(this);

	refresh();

	return true;
}

//virtual
void LLPanelEnvSettingsWater::setEnabled(bool enabled)
{
	mFogColorCtrl->setEnabled(enabled);
	mFogDensityCtrl->setEnabled(enabled);
	mUnderwaterModCtrl->setEnabled(enabled);
	mFresnelScaleCtrl->setEnabled(enabled);
	mFresnelOffsetCtrl->setEnabled(enabled);
	mNormalMapCtrl->setEnabled(enabled);
	mNormalScaleXCtrl->setEnabled(enabled);
	mNormalScaleYCtrl->setEnabled(enabled);
	mNormalScaleZCtrl->setEnabled(enabled);
	mLargeWaveCtrl->setEnabled(enabled);
	mSmallWaveCtrl->setEnabled(enabled);
	mRefractionAboveCtrl->setEnabled(enabled);
	mRefractionBelowCtrl->setEnabled(enabled);
	mBlurMultiplierCtrl->setEnabled(enabled);

	LLPanel::setEnabled(enabled);
}

//virtual
void LLPanelEnvSettingsWater::refresh()
{
	if (!mWaterSettings || !canEdit())
	{
		setEnabled(false);
		return;
	}

	setEnabled(true);

	mFogColorCtrl->set(mWaterSettings->getWaterFogColor());
	mFogDensityCtrl->setValue(mWaterSettings->getWaterFogDensity());
	mUnderwaterModCtrl->setValue(mWaterSettings->getFogMod());
	mFresnelScaleCtrl->setValue(mWaterSettings->getFresnelScale());
	mFresnelOffsetCtrl->setValue(mWaterSettings->getFresnelOffset());
	mNormalMapCtrl->setImageAssetID(mWaterSettings->getNormalMapID());

	LLVector3 normal_scale = mWaterSettings->getNormalScale();
	mNormalScaleXCtrl->setValue(normal_scale[VX]);
	mNormalScaleYCtrl->setValue(normal_scale[VY]);
	mNormalScaleZCtrl->setValue(normal_scale[VZ]);

	// Flipped so that North and East are positive in UI
	LLVector2 dir = mWaterSettings->getWave1Dir();
	mLargeWaveCtrl->setValue(-dir.mV[VX], -dir.mV[VY]);
	dir = mWaterSettings->getWave2Dir();
	mSmallWaveCtrl->setValue(-dir.mV[VX], -dir.mV[VY]);

	mRefractionAboveCtrl->setValue(mWaterSettings->getScaleAbove());
	mRefractionBelowCtrl->setValue(mWaterSettings->getScaleBelow());
	mBlurMultiplierCtrl->setValue(mWaterSettings->getBlurMultiplier());

	LLPanel::refresh();
}

//virtual
void LLPanelEnvSettingsWater::setSettings(const LLSettingsBase::ptr_t& settings,
										  bool reset_dirty)
{
	setWater(std::static_pointer_cast<LLSettingsWater>(settings), reset_dirty);
}

void LLPanelEnvSettingsWater::setWater(const LLSettingsWater::ptr_t& settings,
									   bool reset_dirty)
{
	mWaterSettings = settings;
	if (reset_dirty)
	{
		setDirty(false);
	}
	refresh();
}

//virtual
LLSettingsBase::ptr_t LLPanelEnvSettingsWater::getSettingsClone()
{
	LLSettingsBase::ptr_t clone;
	if (mWaterSettings)
	{
		clone = mWaterSettings->buildClone();
	}
	return clone;
}

//virtual
bool LLPanelEnvSettingsWater::hasLocalTextures(LLSD& args)
{
	if (mWaterSettings)
	{
		if (LLLocalBitmap::isLocal(mWaterSettings->getNormalMapID()))
		{
			args["FIELD"] = getString("normalmap");
			return true;
		}
		if (LLLocalBitmap::isLocal(mWaterSettings->getTransparentTextureID()))
		{
			args["FIELD"] = getString("transparent");
			return true;
		}
	}
	return false;
}

//virtual
void LLPanelEnvSettingsWater::updateEditEnvironment()
{
	if (mWaterSettings)
	{
		gEnvironment.setEnvironment(LLEnvironment::ENV_EDIT, mWaterSettings);
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_FAST);
	}
}

//virtual
void LLPanelEnvSettingsWater::updateLocal()
{
	if (mWaterSettings)
	{
		gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, mWaterSettings);
	}
}

//virtual
void LLPanelEnvSettingsWater::updateParcel(S32 parcel_id)
{
	if (mWaterSettings)
	{
		gEnvironment.updateParcel(parcel_id, mWaterSettings, -1, -1);
	}
}

//virtual
void LLPanelEnvSettingsWater::updateRegion()
{
	if (mWaterSettings)
	{
		gEnvironment.updateRegion(mWaterSettings, -1, -1);
	}
}

//static
void LLPanelEnvSettingsWater::onFogColorChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	self->mWaterSettings->setWaterFogColor(LLColor3(self->mFogColorCtrl->get()));
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onNormalMapChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	const LLUUID& map_id = self->mNormalMapCtrl->getImageAssetID();
	self->mWaterSettings->setNormalMapID(map_id);
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onFogDensityChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	F32 value = self->mFogDensityCtrl->getValueF32();
	self->mWaterSettings->setWaterFogDensity(value);
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onFogUnderWaterChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	self->mWaterSettings->setFogMod(self->mUnderwaterModCtrl->getValueF32());
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onLargeWaveChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	LLVector2 vect(self->mLargeWaveCtrl->getValue());
	// Vector flipped so that North and East are negative in settings
	self->mWaterSettings->setWave1Dir(-vect);
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onSmallWaveChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	LLVector2 vect(self->mSmallWaveCtrl->getValue());
	// Vector flipped so that North and East are negative in settings
	self->mWaterSettings->setWave2Dir(-vect);
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onNormalScaleChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	F32 x = self->mNormalScaleXCtrl->getValueF32();
	F32 y = self->mNormalScaleYCtrl->getValueF32();
	F32 z = self->mNormalScaleZCtrl->getValueF32();
	self->mWaterSettings->setNormalScale(LLVector3(x, y, z));
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onFresnelScaleChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	F32 value = self->mFresnelScaleCtrl->getValueF32();
	self->mWaterSettings->setFresnelScale(value);
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onFresnelOffsetChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	F32 value = self->mFresnelOffsetCtrl->getValueF32();
	self->mWaterSettings->setFresnelOffset(value);
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onScaleAboveChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	F32 value = self->mRefractionAboveCtrl->getValueF32();
	self->mWaterSettings->setScaleAbove(value);
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onScaleBelowChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	F32 value = self->mRefractionBelowCtrl->getValueF32();
	self->mWaterSettings->setScaleBelow(value);
	self->mWaterSettings->update();
	self->setDirty();
}

//static
void LLPanelEnvSettingsWater::onBlurMultChanged(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsWater* self = (LLPanelEnvSettingsWater*)userdata;
	if (!self || !self->mWaterSettings) return;	// Paranoia

	F32 value = self->mBlurMultiplierCtrl->getValueF32();
	self->mWaterSettings->setBlurMultiplier(value);
	self->mWaterSettings->update();
	self->setDirty();
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterTrackPicker class
///////////////////////////////////////////////////////////////////////////////

class LLFloaterTrackPicker final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterTrackPicker);

public:
	LLFloaterTrackPicker(LLPanelEnvSettingsDay* owner, const LLSD& args);
	~LLFloaterTrackPicker() override;

	bool postBuild() override;
	void onFocusLost() override;

private:
	static void onButtonCancel(void* userdata);
	static void onButtonSelect(void* userdata);

private:
	LLPanelEnvSettingsDay*	mOwner;
	LLRadioGroup*			mRadioGroup;
	const LLSD				mArgs;
};

LLFloaterTrackPicker::LLFloaterTrackPicker(LLPanelEnvSettingsDay* owner,
										   const LLSD& args)
:	mOwner(owner),
	mArgs(args)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_pick_day_track.xml");
}

//virtual
LLFloaterTrackPicker::~LLFloaterTrackPicker()
{
	gFocusMgr.releaseFocusIfNeeded(this);
}

//virtual
bool LLFloaterTrackPicker::postBuild()
{
	childSetAction("select_btn", onButtonSelect, this);
	childSetAction("cancel_btn", onButtonCancel, this);

	mRadioGroup = getChild<LLRadioGroup>("track_selection");

	std::string altitude;
	bool select_item = true;
	for (LLSD::array_const_iterator it = mArgs.beginArray(),
									end = mArgs.endArray();
		 it != end; ++it)
	{
		const LLSD& element = *it;
		S32 track_id = element["id"].asInteger();
		bool enabled = element["enabled"].asBoolean();
		if (element.has("altitude"))
		{
			altitude = element["altitude"].asString() + "m";
		}
		else
		{
			altitude = " ";
		}
		LLCheckBoxCtrl* checkboxp =
			getChild<LLCheckBoxCtrl>(llformat("%d", track_id).c_str());
		checkboxp->setEnabled(enabled);
		checkboxp->setLabelArg("[ALT]", altitude);
		if (enabled && select_item)
		{
			select_item = false;
			mRadioGroup->setSelectedByValue(LLSD(track_id), true);
		}
	}

	// Search for our owner's parent floater and register as dependent of it
	// if found.
	LLView* parentp = mOwner;
	while (parentp)
	{
		LLFloater* floaterp = parentp->asFloater();
		if (floaterp)
		{
			floaterp->addDependentFloater(this);
			break;
		}
		parentp = parentp->getParent();
	}

	return true;
}

//virtual
void LLFloaterTrackPicker::onFocusLost()
{
	close();
}

//static
void LLFloaterTrackPicker::onButtonCancel(void* userdata)
{
	LLFloaterTrackPicker* self = (LLFloaterTrackPicker*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterTrackPicker::onButtonSelect(void* userdata)
{
	LLFloaterTrackPicker* self = (LLFloaterTrackPicker*)userdata;
	if (!self) return;	// Paranoia

	self->mOwner->onPickerCommitTrackId(self->mRadioGroup->getSelectedValue());
	self->close();
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelEnvSettingsDay class
///////////////////////////////////////////////////////////////////////////////

constexpr F32 DAY_CYCLE_PLAY_TIME_SECONDS = 60.f;

//static
void* LLPanelEnvSettingsDay::createSkySettingsPanel(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	self->mSkyPanel = new LLPanelEnvSettingsSky();
	return self->mSkyPanel;
}

//static
void* LLPanelEnvSettingsDay::createWaterSettingsPanel(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	self->mWaterPanel = new LLPanelEnvSettingsWater();
	return self->mWaterPanel;
}

LLPanelEnvSettingsDay::LLPanelEnvSettingsDay()
:	LLPanelEnvSettings(),
	mScratchSky(LLEnvSettingsSky::buildDefaultSky()),
	mScratchWater(LLEnvSettingsWater::buildDefaultWater()),
	mCurrentTrack(1),
	mCurrentFrame(0.f),
	mOnOpenRefreshTime(-1.f),
	mPlayStartFrame(0.f),
	mDayLength(0),
	mIsPlaying(false)
{
	LLCallbackMap::map_t factory_map;
	factory_map["sky_panel"] = LLCallbackMap(createSkySettingsPanel, this);
	factory_map["water_panel"] = LLCallbackMap(createWaterSettingsPanel, this);
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_settings_day.xml",
											   &factory_map);
}

//virtual
LLPanelEnvSettingsDay::~LLPanelEnvSettingsDay()
{
	stopPlay();
}

//virtual
bool LLPanelEnvSettingsDay::postBuild()
{
	// If LLSettingsDay::TRACK_MAX ever changes, we will have to adjust the
	// number of track buttons !
	llassert_always(LLSettingsDay::TRACK_MAX == 5);

	mWaterTrackBtn = getChild<LLButton>("water_track");
	mWaterTrackBtn->setClickedCallback(onTrack0Button, this);
	mTrackButtons.push_back(mWaterTrackBtn);

	mSky1TrackBtn = getChild<LLButton>("sky1_track");
	mSky1TrackBtn->setClickedCallback(onTrack1Button, this);
	mTrackButtons.push_back(mSky1TrackBtn);

	mSky2TrackBtn = getChild<LLButton>("sky2_track");
	mSky2TrackBtn->setClickedCallback(onTrack2Button, this);
	mTrackButtons.push_back(mSky2TrackBtn);

	mSky3TrackBtn = getChild<LLButton>("sky3_track");
	mSky3TrackBtn->setClickedCallback(onTrack3Button, this);
	mTrackButtons.push_back(mSky3TrackBtn);

	mSky4TrackBtn = getChild<LLButton>("sky4_track");
	mSky4TrackBtn->setClickedCallback(onTrack4Button, this);
	mTrackButtons.push_back(mSky4TrackBtn);

	mCloneTrackBtn = getChild<LLButton>("clone_track");
	mCloneTrackBtn->setClickedCallback(onCloneTrack, this);

	mLoadTrackBtn = getChild<LLButton>("load_track");
	mLoadTrackBtn->setClickedCallback(onLoadTrack, this);

	mClearTrackBtn = getChild<LLButton>("clear_track");
	mClearTrackBtn->setClickedCallback(onClearTrack, this);

	mAddFrameBtn = getChild<LLButton>("add_frame");
	mAddFrameBtn->setClickedCallback(onAddFrame, this);

	mLoadFrameBtn = getChild<LLButton>("load_frame");
	mLoadFrameBtn->setClickedCallback(onLoadFrame, this);

	mDeleteFrameBtn = getChild<LLButton>("delete_frame");
	mDeleteFrameBtn->setClickedCallback(onRemoveFrame, this);

	mPlayBtn = getChild<LLButton>("play_btn");
	mPlayBtn->setClickedCallback(onPlay, this);

	mStopBtn = getChild<LLButton>("stop_btn");
	mStopBtn->setClickedCallback(onStop, this);

	mForwardBtn = getChild<LLButton>("forward_btn");
	mForwardBtn->setClickedCallback(onForward, this);

	mBackwardBtn = getChild<LLButton>("backward_btn");
	mBackwardBtn->setClickedCallback(onBackward, this);

	mTimeSlider = getChild<LLMultiSliderCtrl>("time_slider");
	mTimeSlider->addSlider(0);
	mTimeSlider->setCommitCallback(onTimeSliderCallback);
	mTimeSlider->setCallbackUserData(this);

	mFramesSlider = getChild<LLMultiSliderCtrl>("frames_slider");
	mFramesSlider->setCommitCallback(onFrameSliderCallback);
	mFramesSlider->setCallbackUserData(this);
#if 0	// *TODO: implement double-click callback support in LLMultiSliderCtrl
	mFramesSlider->setDoubleClickCallback(onFrameSliderDoubleClick);
#endif
	mFramesSlider->setSliderMouseDownCallback(onFrameSliderMouseDown);
	mFramesSlider->setSliderMouseUpCallback(onFrameSliderMouseUp);

	mEditLockedText = getChild<LLTextBox>("lock_edit");
	mCurrentTimeText = getChild<LLTextBox>("current_time");

	mWaterLabel = getString("water_label");
	mSkyLabel = getString("sky_label");

	selectTrack(LLSettingsDay::TRACK_GROUND_LEVEL, true);
	refresh();

	return true;
}

//virtual
void LLPanelEnvSettingsDay::setEnabled(bool enabled)
{
	mWaterTrackBtn->setEnabled(enabled);
	mSky1TrackBtn->setEnabled(enabled);
	mSky2TrackBtn->setEnabled(enabled);
	mSky3TrackBtn->setEnabled(enabled);
	mSky4TrackBtn->setEnabled(enabled);
	mTimeSlider->setEnabled(enabled);
	mFramesSlider->setEnabled(enabled);
	mCloneTrackBtn->setEnabled(enabled);
	mClearTrackBtn->setEnabled(enabled);
	mAddFrameBtn->setEnabled(enabled);
	mDeleteFrameBtn->setEnabled(enabled);
	mLoadTrackBtn->setEnabled(enabled);
	mLoadFrameBtn->setEnabled(enabled);
	bool got_frames = !mSliderKeyMap.empty();
	mPlayBtn->setEnabled(enabled && got_frames);
	mStopBtn->setEnabled(enabled && got_frames);
	mForwardBtn->setEnabled(enabled && got_frames);
	mBackwardBtn->setEnabled(enabled && got_frames);
	mEditLockedText->setEnabled(enabled);
	mCurrentTimeText->setEnabled(enabled);
	bool show_sky = mCurrentTrack != LLSettingsDay::TRACK_WATER;
	mWaterPanel->setVisible(enabled && !show_sky);
	mSkyPanel->setVisible(enabled && show_sky);

	LLPanel::setEnabled(enabled);
}

//virtual
bool LLPanelEnvSettingsDay::isDirty() const
{
	// Propagate dirty state from panels as well...
	return mIsDirty || mWaterPanel->isDirty() || mSkyPanel->isDirty();
}

//virtual
void LLPanelEnvSettingsDay::setDirty(bool dirty)
{
	mIsDirty = dirty;
	// Propagate dirty state to panels as well...
	mWaterPanel->setDirty(dirty);
	mSkyPanel->setDirty(dirty);
}

//virtual
void LLPanelEnvSettingsDay::draw()
{
	// *HACK: to work around a race condition on asset loading at panel
	// creation (and initial refresh) time, in order to get the water and sky
	// settings refreshed properly...
	if (mOnOpenRefreshTime > 0.f && gFrameTimeSeconds > mOnOpenRefreshTime)
	{
		mOnOpenRefreshTime = 0.f;
		selectTrack(LLSettingsDay::TRACK_GROUND_LEVEL, true);
	}
	LLPanel::draw();
}

//virtual
void LLPanelEnvSettingsDay::refresh()
{
	if (!mDaySettings || !canEdit())
	{
		setEnabled(false);
		return;
	}

	setEnabled(true);

	mPlayBtn->setVisible(!mIsPlaying);
	mStopBtn->setVisible(mIsPlaying);

	bool show_sky = mCurrentTrack != LLSettingsDay::TRACK_WATER;
	bool can_manipulate = !mIsPlaying && canEdit();
	mLoadTrackBtn->setEnabled(can_manipulate);
	mLoadFrameBtn->setEnabled(can_manipulate);
	mLoadFrameBtn->setLabelArg("[FRAME]", show_sky ? mSkyLabel : mWaterLabel);
	mAddFrameBtn->setEnabled(can_manipulate && isAddingFrameAllowed());
	mAddFrameBtn->setLabelArg("[FRAME]", show_sky ? mSkyLabel : mWaterLabel);
	mDeleteFrameBtn->setEnabled(can_manipulate && isRemovingFrameAllowed());
	mDeleteFrameBtn->setLabelArg("[FRAME]",
								 show_sky ? mSkyLabel : mWaterLabel);

	bool can_clone = false;
	bool can_clear = false;
	if (can_manipulate)
	{
		if (show_sky)
		{
			for (S32 i = 1; i < LLSettingsDay::TRACK_MAX; ++i)
			{
				if (i != mCurrentTrack &&
					!mDaySettings->getCycleTrack(i).empty())
				{
					can_clone = true;
					break;
				}
			}
		}

		if (mCurrentTrack > 1)
		{
			can_clear = !mDaySettings->getCycleTrack(mCurrentTrack).empty();
		}
		else
		{
			can_clear = mDaySettings->getCycleTrack(mCurrentTrack).size() > 1;
		}
	}
	mCloneTrackBtn->setEnabled(can_clone);
	mClearTrackBtn->setEnabled(can_clear);

	bool env_available = gAgent.hasExtendedEnvironment();
	for (S32 i = 0; i < LLSettingsDay::TRACK_MAX; ++i)
	{
		LLButton* button = mTrackButtons[i];
		button->setEnabled(env_available);
		button->setToggleState(i == mCurrentTrack);
	}

	mWaterPanel->refresh();
	mSkyPanel->refresh();
	LLPanel::refresh();
}

//virtual
void LLPanelEnvSettingsDay::setFileLoadingAvailable(bool available)
{
	bool enabled = available && !mIsPlaying && canEdit() && getEnabled();
	mLoadTrackBtn->setEnabled(enabled);
	mLoadFrameBtn->setEnabled(enabled);
}

//virtual
void LLPanelEnvSettingsDay::setSettings(const LLSettingsBase::ptr_t& settings,
										bool reset_dirty)
{
	setDay(std::static_pointer_cast<LLSettingsDay>(settings), reset_dirty);
}

void LLPanelEnvSettingsDay::setDay(const LLSettingsDay::ptr_t& settings,
								   bool reset_dirty)
{
	if (!settings)
	{
		mDaySettings = settings;
		return;
	}

	mDaySettings = settings->buildDeepCloneAndUncompress();
	if (mDaySettings->isTrackEmpty(LLSettingsDay::TRACK_WATER))
	{
		llwarns << "No water frame found, generating replacement." << llendl;
		mDaySettings->setWaterAtKeyframe(LLEnvSettingsWater::buildDefaultWater(),
										 0.5f);
	}
	if (mDaySettings->isTrackEmpty(LLSettingsDay::TRACK_GROUND_LEVEL))
	{
		llwarns << "No sky frame found, generating replacement." << llendl;
		mDaySettings->setSkyAtKeyframe(LLEnvSettingsSky::buildDefaultSky(),
									   0.5f,
									   LLSettingsDay::TRACK_GROUND_LEVEL);
	}
	updateEditEnvironment();
	gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_EDIT,
										LLEnvironment::TRANSITION_INSTANT);
	gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	updatePanels();
	if (reset_dirty)
	{
		setDirty(false);
		mWaterPanel->setDirty(false);
		mSkyPanel->setDirty(false);
	}

	// *HACK: to work around a race condition on asset loading at panel
	// creation (and initial refresh) time, in order to get the water and sky
	// settings refreshed properly...
	if (mOnOpenRefreshTime != 0.f)
	{
		mOnOpenRefreshTime = gFrameTimeSeconds + 1.f;
	}
}

void LLPanelEnvSettingsDay::synchronizePanels()
{
	if (!mDaySettings)
	{
		return;
	}

	bool can_edit = false;
	LLSettingsBase::ptr_t waterp = mScratchWater;
	LLSettingsBase::ptr_t skyp = mScratchSky;
	std::string cur_slider = mFramesSlider->getCurSlider();
	if (!cur_slider.empty())
	{
		keymap_t::iterator it = mSliderKeyMap.find(cur_slider);
		if (it != mSliderKeyMap.end())
		{
			if (mCurrentTrack == LLSettingsDay::TRACK_WATER)
			{
				waterp = it->second.mSettings;
			}
			else
			{
				skyp = it->second.mSettings;
			}
			can_edit = !mIsPlaying;
		}
	}
	mEditLockedText->setVisible(!can_edit);
	mCurrentTimeText->setVisible(can_edit);

	// Set can edit status first, then set settings
	bool can_actually_edit = can_edit && canEdit();
	mWaterPanel->setCanEdit(can_actually_edit);
	mSkyPanel->setCanEdit(can_actually_edit);

	mWaterPanel->setSettings(waterp, false);
	mSkyPanel->setSettings(skyp, false);

	gEnvironment.setEnvironment(LLEnvironment::ENV_EDIT, mSkyPanel->getSky(),
								mWaterPanel->getWater());
	gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
}

void LLPanelEnvSettingsDay::updatePanels()
{
	reblendSettings();
	synchronizePanels();
	updateTimeText();
	refresh();
}

//virtual
LLSettingsBase::ptr_t LLPanelEnvSettingsDay::getSettingsClone()
{
	LLSettingsBase::ptr_t clone;
	if (mDaySettings)
	{
		clone = mDaySettings->buildClone();
	}
	return clone;
}

//virtual
bool LLPanelEnvSettingsDay::hasLocalTextures(LLSD& args)
{
	if (!mDaySettings)
	{
		return false;
	}
	LLSettingsDay::ptr_t dayclone = mDaySettings->buildClone();
	if (!dayclone)
	{
		return false;
	}

	// Brute-force local texture scan
	std::string field;
	F32 percent = 0.f;
	for (S32 i = 0; i < LLSettingsDay::TRACK_MAX; ++i)
	{
		S32 frame = 0;
		LLSettingsDay::cycle_track_t& track = dayclone->getCycleTrack(i);
		for (LLSettingsDay::cycle_track_it_t it = track.begin(),
											 end = track.end();
			 it != end; ++it)
		{
			++frame;

			if (i == LLSettingsDay::TRACK_WATER)
			{
				LLSettingsWater::ptr_t waterp =
					std::static_pointer_cast<LLSettingsWater>(it->second);
				if (LLLocalBitmap::isLocal(waterp->getNormalMapID()))
				{
					field = mWaterPanel->getString("normalmap");
					percent = it->first;
					break;
				}
				if (LLLocalBitmap::isLocal(waterp->getTransparentTextureID()))
				{
					field = mWaterPanel->getString("transparent");
					percent = it->first;
					break;
				}
			}
			else
			{
				LLSettingsSky::ptr_t skyp =
					std::static_pointer_cast<LLSettingsSky>(it->second);
				if (LLLocalBitmap::isLocal(skyp->getSunTextureId()))
				{
					field = mSkyPanel->getString("sun");
					percent = it->first;
					break;
				}
				if (LLLocalBitmap::isLocal(skyp->getMoonTextureId()))
				{
					field = mSkyPanel->getString("moon");
					percent = it->first;
					break;
				}
				if (LLLocalBitmap::isLocal(skyp->getCloudNoiseTextureId()))
				{
					field = mSkyPanel->getString("cloudnoise");
					percent = it->first;
					break;
				}
				if (LLLocalBitmap::isLocal(skyp->getBloomTextureId()))
				{
					field = mSkyPanel->getString("bloom");
					percent = it->first;
					break;
				}
			}
		}
		if (!field.empty())
		{
			args["TRACK"] = mTrackButtons[i]->getCurrentLabel();
			args["FRAME"] = LLSD::Integer(percent * 100.f); // %
			args["FIELD"] = field;
			args["FRAMENO"] = frame;
			return true;
		}
	}

	return false;
}

//virtual
void LLPanelEnvSettingsDay::updateEditEnvironment()
{
	if (!mDaySettings)
	{
		return;
	}

	S32 skytrack = mCurrentTrack ? mCurrentTrack : 1;
	mSkyBlender = std::make_shared<LLTrackBlenderLoopingManual>(mScratchSky,
																mDaySettings,
																skytrack);
	mWaterBlender =
		std::make_shared<LLTrackBlenderLoopingManual>(mScratchWater,
													  mDaySettings,
													  LLSettingsDay::TRACK_WATER);
	if (gAgent.hasExtendedEnvironment())
	{
		selectTrack(LLSettingsDay::TRACK_MAX, true);
	}
	else
	{
		selectTrack(LLSettingsDay::TRACK_GROUND_LEVEL, true);
	}

	reblendSettings();

	gEnvironment.setEnvironment(LLEnvironment::ENV_EDIT, mSkyPanel->getSky(),
								mWaterPanel->getWater());
	gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_FAST);
}

void LLPanelEnvSettingsDay::reblendSettings()
{
	F32 position = mTimeSlider->getCurSliderValue();
	if (mSkyBlender)
	{
		if (mCurrentTrack != LLSettingsDay::TRACK_WATER &&
			mSkyBlender->getTrack() != mCurrentTrack)
		{
			mSkyBlender->switchTrack(mCurrentTrack, position);
		}
		else
		{
			mSkyBlender->setPosition(position);
		}
	}
	if (mWaterBlender)
	{
		mWaterBlender->setPosition(position);
	}
}

//virtual
void LLPanelEnvSettingsDay::updateLocal()
{
	if (mDaySettings)
	{
		gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, mDaySettings);
	}
}

//virtual
void LLPanelEnvSettingsDay::updateParcel(S32 parcel_id)
{
	if (mDaySettings)
	{
		gEnvironment.updateParcel(parcel_id, mDaySettings, -1, -1);
	}
}

//virtual
void LLPanelEnvSettingsDay::updateRegion()
{
	if (mDaySettings)
	{
		gEnvironment.updateRegion(mDaySettings, -1, -1);
	}
}

bool LLPanelEnvSettingsDay::isAddingFrameAllowed()
{
	if (!mDaySettings || !mFramesSlider->getCurSlider().empty())
	{
		return false;
	}
	F32 frame = mTimeSlider->getCurSliderValue();
	if ((mDaySettings->getSettingsNearKeyframe(frame, mCurrentTrack,
											   LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR)).second)
	{
		return false;
	}
	return mFramesSlider->canAddSliders();
}

bool LLPanelEnvSettingsDay::isRemovingFrameAllowed()
{
	if (!mDaySettings || mFramesSlider->getCurSlider().empty())
	{
		return false;
	}
	if (mCurrentTrack <= LLSettingsDay::TRACK_GROUND_LEVEL)
	{
		return mSliderKeyMap.size() > 1;
	}
	else
	{
		return mSliderKeyMap.size() != 0;
	}
}

void LLPanelEnvSettingsDay::addSliderFrame(F32 frame,
										   const LLSettingsBase::ptr_t& setting,
										   bool update_ui)
{
	std::string new_slider = mFramesSlider->addSlider(frame);
	if (!new_slider.empty())
	{
		mSliderKeyMap[new_slider] = FrameData(frame, setting);
		if (update_ui)
		{
			mTimeSlider->setCurSliderValue(frame);
			updatePanels();
		}
	}
}

void LLPanelEnvSettingsDay::removeCurrentSliderFrame()
{
	std::string slider = mFramesSlider->getCurSlider();
	if (slider.empty() || !mDaySettings)
	{
		return;
	}
	mFramesSlider->deleteCurSlider();
	keymap_t::iterator iter = mSliderKeyMap.find(slider);
	if (iter != mSliderKeyMap.end())
	{
		F32 frame = iter->second.mFrame;
		mDaySettings->removeTrackKeyframe(mCurrentTrack, frame);
		mSliderKeyMap.erase(iter);
	}
	mTimeSlider->setCurSliderValue(mFramesSlider->getCurSliderValue());
	updatePanels();
}

void LLPanelEnvSettingsDay::removeSliderFrame(F32 frame)
{
	keymap_t::iterator it =
		std::find_if(mSliderKeyMap.begin(), mSliderKeyMap.end(),
					 [frame](const keymap_t::value_type& value)
					 {
						return fabsf(value.second.mFrame - frame) <
								 LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR;
					 });
	if (it != mSliderKeyMap.end())
	{
		mFramesSlider->deleteSlider(it->first);
		mSliderKeyMap.erase(it);
	}
}

void LLPanelEnvSettingsDay::updateSlider()
{
	// Remember our current frame if any
	F32 frame = mTimeSlider->getCurSliderValue();

	mFramesSlider->clear();
	mSliderKeyMap.clear();

	if (!mDaySettings)
	{
		return;
	}

	LLSettingsDay::cycle_track_t track =
		mDaySettings->getCycleTrack(mCurrentTrack);
	for (auto& track_frame : track)
	{
		addSliderFrame(track_frame.first, track_frame.second, false);
	}
	if (mSliderKeyMap.empty())
	{
		// Disable panels. Set can edit status first, then set settings
		mWaterPanel->setCanEdit(false);
		mSkyPanel->setCanEdit(false);
		mWaterPanel->setWater(LLSettingsWater::ptr_t(NULL));
		mSkyPanel->setSky(LLSettingsSky::ptr_t(NULL));
	}

	selectFrame(frame, LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR);
}

void LLPanelEnvSettingsDay::updateTimeText()
{
	if (!mCurrentTimeText->getVisible())
	{
		return;
	}
	if (!mCurrentTimeText->getEnabled())
	{
		mCurrentTimeText->setText("");
		return;
	}
	F32 percent = mTimeSlider->getCurSliderValue();
	std::string time;
	if (mDayLength > 0)
	{
		S32 seconds = S32((F32)mDayLength * percent);
		S32 hours = seconds / 3600;
		S32 minutes = (seconds - 3600 * hours) / 60;
		if (minutes == 60)
		{
			++hours;
			minutes = 0;
		}
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
	}
	else
	{
		time = llformat("%d%%", (S32)(percent * 100.f));
	}
	mCurrentTimeText->setText(time);
}

void LLPanelEnvSettingsDay::selectFrame(F32 frame, F32 slop_factor)
{
	mFramesSlider->resetCurSlider();

	keymap_t::iterator it = mSliderKeyMap.begin();
	keymap_t::iterator end = mSliderKeyMap.end();
	while (it != end)
	{
		F32 keyframe = it->second.mFrame;
		F32 delta = fabsf(keyframe - frame);
		if (delta <= slop_factor)
		{
			keymap_t::iterator next_it = std::next(it);
			if (delta != 0.f && next_it != end &&
				fabsf(next_it->second.mFrame - frame) < delta)
			{
				mFramesSlider->setCurSlider(next_it->first);
				frame = next_it->second.mFrame;
				break;
			}
			mFramesSlider->setCurSlider(it->first);
			frame = it->second.mFrame;
			break;
		}
		++it;
	}

	mTimeSlider->setCurSliderValue(frame);
	updatePanels();
}

void LLPanelEnvSettingsDay::onPickerCommitTrackId(S32 track_id)
{
	if (mDaySettings && mSourceSettings)
	{
		cloneTrack(mSourceSettings, track_id, mCurrentTrack);
		reblendSettings();
		synchronizePanels();
	}
}

void LLPanelEnvSettingsDay::cloneTrack(const LLSettingsDay::ptr_t& src_day,
									   S32 src_idx, S32 dst_idx)
{
	if (!mDaySettings || !src_day)
	{
		return;
	}

	if (src_idx < 0 || dst_idx < 0 || src_idx >= LLSettingsDay::TRACK_MAX ||
		dst_idx >= LLSettingsDay::TRACK_MAX)
	{
		llwarns << "Track index out of range. Aborted. src_idx=" << src_idx
				<< " - dst_idx=" << dst_idx << llendl;
		return;
	}

	if (src_idx != dst_idx &&
		(src_idx == LLSettingsDay::TRACK_WATER ||
		 dst_idx == LLSettingsDay::TRACK_WATER))
	{
		// One of the tracks is a water track and the other is not
		LLSD args;
		args["TRACK1"] = mTrackButtons[src_idx]->getCurrentLabel();
		args["TRACK2"] = mTrackButtons[dst_idx]->getCurrentLabel();
		gNotifications.add("TrackLoadMismatch", args);
		return;
	}

	// Keep a copy of the current track in case of failure.
	LLSettingsDay::cycle_track_t& backup_track =
		mDaySettings->getCycleTrack(dst_idx);

	mDaySettings->clearCycleTrack(dst_idx);	// Because source may be empty
	LLSettingsDay::cycle_track_t& source_track =
		src_day->getCycleTrack(src_idx);
	S32 additions = 0;
	for (auto& track_frame : source_track)
	{
		LLSettingsBase::ptr_t framep = track_frame.second;
		LLSettingsBase::ptr_t clonep = framep->buildDerivedClone();
		if (clonep)
		{
			++additions;
			mDaySettings->setSettingsAtKeyframe(clonep, track_frame.first,
												dst_idx);
		}
	}
	if (!additions)
	{
		// Nothing was actually added; restore the old track and issue a
		// warning.
		mDaySettings->replaceCycleTrack(dst_idx, backup_track);
		LLSD args;
		args["TRACK"] = mTrackButtons[dst_idx]->getCurrentLabel();
		gNotifications.add("TrackLoadFailed", args);
	}
	setDirty();
	updatePanels();
}

void LLPanelEnvSettingsDay::selectTrack(S32 track_index, bool force)
{
	if (track_index >= 0 && track_index < LLSettingsDay::TRACK_MAX)
	{
		mCurrentTrack = track_index;
	}

	LLButton* buttonp = mTrackButtons[track_index];
	if (buttonp->getToggleState() && !force)
	{
		return;
	}

	refresh();

	updateSlider();
}

void LLPanelEnvSettingsDay::onAssetLoaded(const LLUUID& item_id,
										  LLSettingsBase::ptr_t settings,
										  S32 status)
{
	if (!mDaySettings)
	{
		return;
	}

	if (!settings || status)
	{
		gNotifications.add("CantFindInvItem");
		return;
	}

	LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
	// This should not happen (already cheched in loadInventoryItem()), but
	// just in case...
	if (!itemp || itemp->getIsBrokenLink())
	{
		llwarns << "Could not find inventory item for Id: " << item_id
				<< llendl;
		return;
	}

	// Do not allow at all to import no-trans settings in a transfer-ok item.
	// LL's code allows it by changing the settings (not the item) to no-trans,
	// thus relying on the item getting saved and marked no-transfer at that
	// stage, but I suspect this could be worked around or be the victim of
	// race conditions (between inventory item and settings permissions)... HB
	if (!itemp->getPermissions().allowTransferBy(gAgentID) &&
		!mDaySettings->getFlag(LLSettingsBase::FLAG_NOTRANS))
	{
		gNotifications.add("SettingsMakeNoTrans");
		return;
	}

	std::string type = settings->getSettingsType();
	bool is_water = type == "water";
	if (type != "daycycle")
	{
		if (mCurrentTrack == LLSettingsDay::TRACK_WATER)
		{
			if (!is_water)
			{
				llwarns << "Attempt to load a sky frame in the water track. Aborted."
						<< llendl;
				return;
			}
		}
		else if (is_water)
		{
			llwarns << "Attempt to load a water frame in a sky track. Aborted."
					<< llendl;
			return;
		}
		if (!mFramesSlider->canAddSliders())
		{
			llwarns << "Attempt to add new frame when slider is full. Aborted."
					<< llendl;
			return;
		}
		F32 frame = mTimeSlider->getCurSliderValue();
		LLSettingsDay::cycle_track_t::value_type nearest =
			mDaySettings->getSettingsNearKeyframe(frame, mCurrentTrack,
												  LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR);
		if (nearest.first != INVALID_TRACKPOS)
		{
			// There is already a frame near the target location. Remove it so
			// we can put the new one in its place.
			mDaySettings->removeTrackKeyframe(mCurrentTrack, nearest.first);
			removeSliderFrame(nearest.first);
		}
		// Do not forget to clone (we might reuse/load it a couple of times)
		mDaySettings->setSettingsAtKeyframe(settings->buildDerivedClone(), frame,
											mCurrentTrack);
		addSliderFrame(frame, settings, false);
	}
	else if (mCurrentTrack == LLSettingsDay::TRACK_WATER)
	{
		// Clone the water track
		LLSettingsDay::ptr_t dayp =
			std::dynamic_pointer_cast<LLSettingsDay>(settings);
		cloneTrack(dayp, mCurrentTrack, mCurrentTrack);
	}
	else
	{
		// We want to copy a sky track, but we first need to know which track
		// among the four sky tracks we want copied...
		LLSettingsDay::ptr_t dayp =
			std::dynamic_pointer_cast<LLSettingsDay>(settings);
		const LLEnvironment::altitude_list_t& altitudes =
			gEnvironment.getRegionAltitudes();
		bool use_altitudes = !altitudes.empty() &&
							 mEditContext >= CONTEXT_PARCEL;
		LLSD args = LLSD::emptyArray();
		S32 counter = 0;
		S32 last_non_empty_track = 0;
		for (S32 i = 1; i < LLSettingsDay::TRACK_MAX; ++i)
		{
			LLSD track;
			track["id"] = LLSD::Integer(i);
			bool populated = !dayp->isTrackEmpty(i);
			track["enabled"] = populated;
			if (populated)
			{
				++counter;
				last_non_empty_track = i;
			}
			if (use_altitudes)
			{
				track["altitude"] = altitudes[i - 1];
			}
			args.append(track);
		}
		if (!counter)
		{
			// This should not happen
			llwarns << "Tried to copy tracks, but there are no available sources."
					<< llendl;
			return;
		}
		else if (counter > 1)
		{
			// Let the user choose the track to clone...
			mSourceSettings = dayp;
			new LLFloaterTrackPicker(this, args);
			return;
		}
		// Only one sky track available (normaly the first, but just in case
		// we did remember the actual track number in last_non_empty_track).
		cloneTrack(dayp, last_non_empty_track, mCurrentTrack);
	}
	reblendSettings();
	synchronizePanels();
}

void LLPanelEnvSettingsDay::loadInventoryItem(LLUUID item_id)
{
	if (!mDaySettings || item_id.isNull())
	{
		return;
	}

	// Make sure we are not trying to load a link and get the linked item Id
	// in that case.
	item_id = gInventory.getLinkedItemID(item_id);
	LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
	if (!itemp || itemp->getIsBrokenLink())
	{
		llwarns << "Could not find inventory item for Id: " << item_id
				<< llendl;
		return;
	}
	const LLUUID& asset_id = itemp->getAssetUUID();
	if (asset_id.isNull())
	{
		llwarns << "Null asset Id for inventory item: " << item_id
				<< ". Not loading it." << llendl;
		return;
	}
	LLEnvSettingsBase::getSettingsAsset(asset_id,
										[this, item_id](LLUUID,
														LLSettingsBase::ptr_t settings,
														S32 status, LLExtStat)
										{
											onAssetLoaded(item_id, settings,
														  status);
										});
}

void LLPanelEnvSettingsDay::startPlay()
{
	if (!mIsPlaying)
	{
		mIsPlaying = true;
		mFramesSlider->resetCurSlider();
		mPlayTimer.reset();
		mPlayTimer.start();
		gIdleCallbacks.addFunction(onIdlePlay, this);
		mPlayStartFrame = mTimeSlider->getCurSliderValue();
		mPlayBtn->setVisible(false);
		mStopBtn->setVisible(true);
	}
}

void LLPanelEnvSettingsDay::stopPlay()
{
	if (mIsPlaying)
	{
		mIsPlaying = false;
		mFramesSlider->resetCurSlider();
		gIdleCallbacks.deleteFunction(onIdlePlay, this);
		mPlayTimer.stop();
		F32 frame = mTimeSlider->getCurSliderValue();
		selectFrame(frame, LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR);
		mPlayBtn->setVisible(true);
		mStopBtn->setVisible(false);
	}
}

//static
void LLPanelEnvSettingsDay::onIdlePlay(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self && !gDisconnected)
	{
		F32 prcnt_played = self->mPlayTimer.getElapsedTimeF32() /
						   DAY_CYCLE_PLAY_TIME_SECONDS;
		F32 new_frame = fmodf(self->mPlayStartFrame + prcnt_played, 1.f);
		self->mTimeSlider->setCurSliderValue(new_frame);
		if (self->mSkyBlender)
		{
			self->mSkyBlender->setPosition(new_frame);
		}
		if (self->mWaterBlender)
		{
			self->mWaterBlender->setPosition(new_frame);
		}
		self->synchronizePanels();
		self->refresh();
	}
}

//static
void LLPanelEnvSettingsDay::onTrack0Button(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self)
	{
		self->stopPlay();
		self->selectTrack(0);
	}
}

//static
void LLPanelEnvSettingsDay::onTrack1Button(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self)
	{
		self->stopPlay();
		self->selectTrack(1);
	}
}

//static
void LLPanelEnvSettingsDay::onTrack2Button(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self)
	{
		self->stopPlay();
		self->selectTrack(2);
	}
}

//static
void LLPanelEnvSettingsDay::onTrack3Button(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self)
	{
		self->stopPlay();
		self->selectTrack(3);
	}
}

//static
void LLPanelEnvSettingsDay::onTrack4Button(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self)
	{
		self->stopPlay();
		self->selectTrack(4);
	}
}

//static
void LLPanelEnvSettingsDay::onCloneTrack(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (!self || !self->mDaySettings)
	{
		return;
	}

	self->stopPlay();

	const LLEnvironment::altitude_list_t& altitudes =
		gEnvironment.getRegionAltitudes();
	bool use_altitudes = !altitudes.empty() &&
						 self->mEditContext >= CONTEXT_PARCEL;
	LLSD args = LLSD::emptyArray();
	S32 counter = 0;
	for (S32 i = 1; i < LLSettingsDay::TRACK_MAX; ++i)
	{
		LLSD track;
		track["id"] = LLSD::Integer(i);
		bool populated = i != self->mCurrentTrack &&
						 !self->mDaySettings->isTrackEmpty(i);
		track["enabled"] = populated;
		if (populated)
		{
			++counter;
		}
		if (use_altitudes)
		{
			track["altitude"] = altitudes[i - 1];
		}
		args.append(track);
	}
	if (!counter)
	{
		// This should not happen
		llwarns << "Tried to copy tracks, but there are no available sources."
				<< llendl;
		return;
	}
	self->mSourceSettings = self->mDaySettings;
	new LLFloaterTrackPicker(self, args);
}

static void inv_items_picker_cb(const std::vector<std::string>&,
								const uuid_vec_t& ids, void* userdata, bool)
{
	LLPanelEnvSettingsDay* panelp = (LLPanelEnvSettingsDay*)userdata;
	if (panelp && !ids.empty())
	{
		panelp->loadInventoryItem(ids[0]);
	}
}

//static
void LLPanelEnvSettingsDay::onLoadTrack(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self && self->mDaySettings)
	{
		self->stopPlay();

		HBFloaterInvItemsPicker* pickerp =
			new HBFloaterInvItemsPicker(self, inv_items_picker_cb, self);
		if (pickerp)
		{
			pickerp->setExcludeLibrary();
			pickerp->setAssetType(LLAssetType::AT_SETTINGS,
								  (S32)LLSettingsType::ST_DAYCYCLE);
		}
	}
}

//static
void LLPanelEnvSettingsDay::onClearTrack(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (!self || !self->mDaySettings)
	{
		return;
	}

	self->stopPlay();

	if (self->mCurrentTrack > 1)
	{
		self->mDaySettings->getCycleTrack(self->mCurrentTrack).clear();
	}
	else
	{
		LLSettingsDay::cycle_track_t& track =
			self->mDaySettings->getCycleTrack(self->mCurrentTrack);
		track.erase(++track.begin(), track.end());
	}

	self->updateEditEnvironment();
	gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_EDIT,
									LLEnvironment::TRANSITION_INSTANT);
	gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	self->setDirty();
	self->synchronizePanels();
	self->updatePanels();
}

//static
void LLPanelEnvSettingsDay::onAddFrame(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (!self || !self->mDaySettings)
	{
		return;
	}

	self->stopPlay();

	if (!self->mFramesSlider->canAddSliders())
	{
		// This should not happen since the button should be disabled
		llwarns << "Attempt to add new frame when slider is full." << llendl;
		return;
	}

	F32 frame = self->mTimeSlider->getCurSliderValue();
	if ((self->mDaySettings->getSettingsNearKeyframe(frame,
													 self->mCurrentTrack,
													 LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR)).second)
	{
		// This should not happen since the button should be disabled
		llwarns << "Attempt to add new frame too close to an existing frame."
				<< llendl;
		return;
	}
	LLSettingsBase::ptr_t settingsp;
	if (self->mCurrentTrack == LLSettingsDay::TRACK_WATER)
	{
		// Scratch water should always have the current water settings.
		LLSettingsWater::ptr_t waterp = self->mScratchWater->buildClone();
		settingsp = waterp;
		self->mDaySettings->setWaterAtKeyframe(waterp, frame);
	}
	else
	{
		// Scratch sky should always have the current sky settings.
		LLSettingsSky::ptr_t skyp = self->mScratchSky->buildClone();
		settingsp = skyp;
		self->mDaySettings->setSkyAtKeyframe(skyp, frame, self->mCurrentTrack);
	}
	self->setDirty();
	self->addSliderFrame(frame, settingsp);
}

//static
void LLPanelEnvSettingsDay::onLoadFrame(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self && self->mDaySettings)
	{
		self->stopPlay();

		HBFloaterInvItemsPicker* pickerp =
			new HBFloaterInvItemsPicker(self, inv_items_picker_cb, self);
		if (pickerp)
		{
			S32 type;
			if (self->mCurrentTrack == LLSettingsDay::TRACK_WATER)
			{
				type = LLSettingsType::ST_WATER;
			}
			else
			{
				type = LLSettingsType::ST_SKY;
			}
			pickerp->setExcludeLibrary();
			pickerp->setAssetType(LLAssetType::AT_SETTINGS, type);
		}
	}
}

//static
void LLPanelEnvSettingsDay::onRemoveFrame(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self && self->mDaySettings)
	{
		self->stopPlay();

		std::string cur_slider = self->mFramesSlider->getCurSlider();
		if (!cur_slider.empty())
		{
			self->setDirty();
			self->removeCurrentSliderFrame();
		}
	}
}

//static
void LLPanelEnvSettingsDay::onTimeSliderCallback(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self && self->mDaySettings)
	{
		self->stopPlay();
		self->selectFrame(self->mTimeSlider->getCurSliderValue(),
						  LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR);
	}
}

//static
void LLPanelEnvSettingsDay::onFrameSliderCallback(LLUICtrl*, void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (!self || !self->mDaySettings)
	{
		return;
	}

	std::string cur_slider = self->mFramesSlider->getCurSlider();
	if (cur_slider.empty())
	{
		return;
	}

	F32 frame = self->mFramesSlider->getCurSliderValue();
	keymap_t::iterator it = self->mSliderKeyMap.find(cur_slider);
	if (it == self->mSliderKeyMap.end())
	{
		llwarns << "Cannot find current slider value in slider map !"
				<< llendl;
		return;
	}

	// *TODO: implement SHIFT-copy feature ?

	if (self->canEdit() &&
		self->mDaySettings->moveTrackKeyframe(self->mCurrentTrack,
											  it->second.mFrame, frame))
	{
		it->second.mFrame = frame;
	}
	else
	{
		self->mFramesSlider->setCurSliderValue(it->second.mFrame);
	}
}

//static
void LLPanelEnvSettingsDay::onFrameSliderMouseDown(S32 x, S32 y,
												   void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (!self || !self->mDaySettings)
	{
		return;
	}

	self->stopPlay();

	F32 frame = self->mFramesSlider->getSliderValueFromPos(x, y);
	self->mCurrentFrame = frame;
	std::string cur_slider = self->mFramesSlider->getCurSlider();

	// *TODO: implement SHIFT-copy feature ?

	if (!cur_slider.empty())
	{
		F32 cur_frame = self->mFramesSlider->getSliderValue(cur_slider);
		if (fabsf(cur_frame - frame) > LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR)
		{
			self->mFramesSlider->resetCurSlider();
		}
	}
	self->mTimeSlider->setCurSliderValue(frame);

	self->updatePanels();

	gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
}

//static
void LLPanelEnvSettingsDay::onFrameSliderMouseUp(S32 x, S32 y, void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self && self->mDaySettings)
	{
		F32 frame = self->mFramesSlider->getSliderValueFromPos(x, y);
		self->mTimeSlider->setCurSliderValue(frame);
		self->selectFrame(frame, LLSettingsDay::DEFAULT_FRAME_SLOP_FACTOR);
		// Set dirty only if we actually changed the current frame position
		// since last onFrameSliderMouseDown() event
		if (fabsf(frame - self->mCurrentFrame) >=
					0.75f * self->mFramesSlider->getIncrement())
		{
			self->setDirty();
		}
	}
}

//static
void LLPanelEnvSettingsDay::onPlay(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self)
	{
		self->startPlay();
	}
}

//static
void LLPanelEnvSettingsDay::onStop(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (self)
	{
		self->stopPlay();
	}
}

//static
void LLPanelEnvSettingsDay::onForward(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (!self || !self->mDaySettings || self->mSliderKeyMap.empty())
	{
		return;
	}
	F32 inc_frame = self->mTimeSlider->getCurSliderValue() +
					self->mTimeSlider->getIncrement() * 0.5f;
	F32 frame = self->mDaySettings->getUpperBoundFrame(self->mCurrentTrack,
													   inc_frame);
	self->selectFrame(frame, 0.f);
	self->stopPlay();
}

//static
void LLPanelEnvSettingsDay::onBackward(void* userdata)
{
	LLPanelEnvSettingsDay* self = (LLPanelEnvSettingsDay*)userdata;
	if (!self || !self->mDaySettings || self->mSliderKeyMap.empty())
	{
		return;
	}
	F32 dec_frame = self->mTimeSlider->getCurSliderValue() -
					self->mTimeSlider->getIncrement() * 0.5f;
	F32 frame = self->mDaySettings->getLowerBoundFrame(self->mCurrentTrack,
													   dec_frame);
	self->selectFrame(frame, 0.f);
	self->stopPlay();
}
