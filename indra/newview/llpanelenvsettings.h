/**
 * @file llpanelenvsettings.h
 * @brief Environment settings panel classes definitions
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

#ifndef LL_LLPANELENVSETTINGS_H
#define LL_LLPANELENVSETTINGS_H

#include "llpanel.h"
#include "llframetimer.h"

#include "llenvironment.h"

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLMultiSliderCtrl;
class LLSliderCtrl;
class LLTextBox;
class LLTextureCtrl;
class LLVirtualTrackball;
class LLXYVector;

class LLPanelEnvSettings : public LLPanel
{
public:
	virtual void setEnabled(bool enabled) = 0;

	virtual void setSettings(const LLSettingsBase::ptr_t& settings,
							 bool reset_dirty = true) = 0;
	virtual LLSettingsBase::ptr_t getSettingsClone() = 0;

	virtual bool hasLocalTextures(LLSD& args) = 0;
	virtual void updateEditEnvironment() = 0;
	virtual void updateLocal() = 0;
	virtual void updateParcel(S32 parcel_id) = 0;
	virtual void updateRegion() = 0;

	virtual std::string getSettingsName() = 0;
	virtual void setSettingsName(const std::string& name) = 0;

	virtual bool settingsValid() const = 0;

	LL_INLINE virtual bool isDirty() const			{ return mIsDirty; }
	LL_INLINE virtual void setDirty(bool b = true)	{ mIsDirty = b; }
	LL_INLINE bool canEdit() const					{ return mCanEdit; }
	LL_INLINE void setCanEdit(bool b = true)		{ mCanEdit = b; refresh(); }

	// Used to refresh the enabled state of any button that needs the file
	// selector, depending whether the latter is already in use or not.
	// Currently, only the day cycle settings panel got such buttons.
	LL_INLINE virtual void setFileLoadingAvailable(bool available)
	{
	}

	enum EEditContext {
		CONTEXT_UNKNOWN,
		CONTEXT_INVENTORY,
		CONTEXT_PARCEL,
		CONTEXT_REGION
	};

	LL_INLINE void setEditContext(EEditContext ctx)	{ mEditContext = ctx; }
	LL_INLINE EEditContext getEditContext() const	{ return mEditContext; }

protected:
	LL_INLINE LLPanelEnvSettings()
	:	mEditContext(CONTEXT_UNKNOWN),
		mIsDirty(false),
		mCanEdit(false)
	{
	}

protected:
	EEditContext	mEditContext;
	bool			mIsDirty;
	bool			mCanEdit;
};

class LLPanelEnvSettingsSky final : public LLPanelEnvSettings
{
protected:
	LOG_CLASS(LLPanelEnvSettingsSky);

public:
	LLPanelEnvSettingsSky();
	~LLPanelEnvSettingsSky() override;

	bool postBuild() override;
	void refresh() override;

	void setEnabled(bool enabled) override;

	void setSettings(const LLSettingsBase::ptr_t& settings,
					 bool reset_dirty = true) override;
	LLSettingsBase::ptr_t getSettingsClone() override;

	bool hasLocalTextures(LLSD& args) override;
	void updateEditEnvironment() override;
	void updateLocal() override;
	void updateParcel(S32 parcel_id) override;
	void updateRegion() override;

	LL_INLINE std::string getSettingsName() override
	{
		return mSkySettings->getName();
	}

	LL_INLINE void setSettingsName(const std::string& name) override
	{
		mSkySettings->setName(name);
	}

	LL_INLINE bool settingsValid() const override
	{
		return mSkySettings != NULL;
	}

	void setSky(const LLSettingsSky::ptr_t& settings, bool reset_dirty = true);

	LL_INLINE LLSettingsSky::ptr_t getSky() const
	{
		return mSkySettings;
	}

private:
	void updateSettings();

	static void onAmbientLightChanged(LLUICtrl*, void* userdata);
	static void onBlueHorizonChanged(LLUICtrl*, void* userdata);
	static void onBlueDensityChanged(LLUICtrl*, void* userdata);
	static void onHazeHorizonChanged(LLUICtrl*, void* userdata);
	static void onHazeDensityChanged(LLUICtrl*, void* userdata);
	static void onMoistureLevelChanged(LLUICtrl*, void* userdata);
	static void onDropletRadiusChanged(LLUICtrl*, void* userdata);
	static void onIceLevelChanged(LLUICtrl*, void* userdata);
	static void onSceneGammaChanged(LLUICtrl*, void* userdata);
	static void onDensityMultipChanged(LLUICtrl*, void* userdata);
	static void onDistanceMultipChanged(LLUICtrl*, void* userdata);
	static void onMaxAltChanged(LLUICtrl*, void* userdata);
	static void onProbeAmbianceChanged(LLUICtrl*, void* userdata);
	static void onCloudColorChanged(LLUICtrl*, void* userdata);
	static void onCloudMapChanged(LLUICtrl*, void* userdata);
	static void onCloudCoverageChanged(LLUICtrl*, void* userdata);
	static void onCloudScaleChanged(LLUICtrl*, void* userdata);
	static void onCloudVarianceChanged(LLUICtrl*, void* userdata);
	static void onCloudScrollChanged(LLUICtrl*, void* userdata);
	static void onCloudDensityChanged(LLUICtrl*, void* userdata);
	static void onCloudDetailChanged(LLUICtrl*, void* userdata);
	static void onSunRotationChanged(LLUICtrl*, void* userdata);
	static void onMoonRotationChanged(LLUICtrl*, void* userdata);
	static void onSunImageChanged(LLUICtrl*, void* userdata);
	static void onMoonImageChanged(LLUICtrl*, void* userdata);
	static void onSunlightColorChanged(LLUICtrl*, void* userdata);
	static void onSunScaleChanged(LLUICtrl*, void* userdata);
	static void onMoonScaleChanged(LLUICtrl*, void* userdata);
	static void onGlowChanged(LLUICtrl*, void* userdata);
	static void onMoonBrightnessChanged(LLUICtrl*, void* userdata);
	static void onStarBrightnessChanged(LLUICtrl*, void* userdata);

protected:
	LLSettingsSky::ptr_t	mSkySettings;

private:
	LLCheckBoxCtrl*			mUseProbeAmbianceCtrl;
	LLColorSwatchCtrl*		mAmbientColorCtrl;
	LLColorSwatchCtrl*		mBlueHorizonColorCtrl;
	LLColorSwatchCtrl*		mBlueDensityColorCtrl;
	LLColorSwatchCtrl*		mCloudColorCtrl;
	LLColorSwatchCtrl*		mSunLightColorCtrl;
	LLSliderCtrl*			mHazeHorizonCtrl;
	LLSliderCtrl*			mHazeDensityCtrl;
	LLSliderCtrl*			mMoistureLevelCtrl;
	LLSliderCtrl*			mDropletRadiusCtrl;
	LLSliderCtrl*			mIceLevelCtrl;
	LLSliderCtrl*			mSceneGammaCtrl;
	LLSliderCtrl*			mDensityMultCtrl;
	LLSliderCtrl*			mDistanceMultCtrl;
	LLSliderCtrl*			mMaxAltitudeCtrl;
	LLSliderCtrl*			mProbeAmbianceCtrl;
	LLSliderCtrl*			mCloudCoverageCtrl;
	LLSliderCtrl*			mCloudScaleCtrl;
	LLSliderCtrl*			mCloudVarianceCtrl;
	LLSliderCtrl*			mCloudDensityXCtrl;
	LLSliderCtrl*			mCloudDensityYCtrl;
	LLSliderCtrl*			mCloudDensityDCtrl;
	LLSliderCtrl*			mCloudDetailXCtrl;
	LLSliderCtrl*			mCloudDetailYCtrl;
	LLSliderCtrl*			mCloudDetailDCtrl;
	LLSliderCtrl*			mSunScaleCtrl;
	LLSliderCtrl*			mMoonScaleCtrl;
	LLSliderCtrl*			mGlowFocusCtrl;
	LLSliderCtrl*			mGlowSizeCtrl;
	LLSliderCtrl*			mMoonBrightnessCtrl;
	LLSliderCtrl*			mStarBrightnessCtrl;
	LLTextBox*				mHdrAutoText;
	LLTextBox*				mHdrOffText;
	LLTextBox*				mHdrOnText;
	LLTextureCtrl*			mCloudMapCtrl;
	LLTextureCtrl*			mSunImageCtrl;
	LLTextureCtrl*			mMoonImageCtrl;
	LLVirtualTrackball*		mSunRotationCtrl;
	LLVirtualTrackball*		mMoonRotationCtrl;
	LLXYVector*				mCloudScrollCtrl;
};

class LLPanelEnvSettingsWater final : public LLPanelEnvSettings
{
protected:
	LOG_CLASS(LLPanelEnvSettingsWater);

public:
	LLPanelEnvSettingsWater();

	bool postBuild() override;
	void refresh() override;

	void setEnabled(bool enabled) override;

	void setSettings(const LLSettingsBase::ptr_t& settings,
					 bool reset_dirty = true) override;
	LLSettingsBase::ptr_t getSettingsClone() override;

	bool hasLocalTextures(LLSD& args) override;
	void updateEditEnvironment() override;
	void updateLocal() override;
	void updateParcel(S32 parcel_id) override;
	void updateRegion() override;

	LL_INLINE std::string getSettingsName() override
	{
		return mWaterSettings->getName();
	}

	LL_INLINE void setSettingsName(const std::string& name) override
	{
		mWaterSettings->setName(name);
	}

	LL_INLINE bool settingsValid() const override
	{
		return mWaterSettings != NULL;
	}

	void setWater(const LLSettingsWater::ptr_t& settings,
				  bool reset_dirty = true);

	LL_INLINE LLSettingsWater::ptr_t getWater() const
	{
		return mWaterSettings;
	}

private:
	static void onFogColorChanged(LLUICtrl*, void* userdata);
	static void onNormalMapChanged(LLUICtrl*, void* userdata);
	static void onFogDensityChanged(LLUICtrl*, void* userdata);
	static void onFogUnderWaterChanged(LLUICtrl*, void* userdata);
	static void onLargeWaveChanged(LLUICtrl*, void* userdata);
	static void onSmallWaveChanged(LLUICtrl*, void* userdata);
	static void onNormalScaleChanged(LLUICtrl*, void* userdata);
	static void onFresnelScaleChanged(LLUICtrl*, void* userdata);
	static void onFresnelOffsetChanged(LLUICtrl*, void* userdata);
	static void onScaleAboveChanged(LLUICtrl*, void* userdata);
	static void onScaleBelowChanged(LLUICtrl*, void* userdata);
	static void onBlurMultChanged(LLUICtrl*, void* userdata);

protected:
	LLSettingsWater::ptr_t	mWaterSettings;

private:
	LLColorSwatchCtrl*		mFogColorCtrl;
	LLSliderCtrl*			mFogDensityCtrl;
	LLSliderCtrl*			mUnderwaterModCtrl;
	LLSliderCtrl*			mFresnelScaleCtrl;
	LLSliderCtrl*			mFresnelOffsetCtrl;
	LLSliderCtrl*			mNormalScaleXCtrl;
	LLSliderCtrl*			mNormalScaleYCtrl;
	LLSliderCtrl*			mNormalScaleZCtrl;
	LLSliderCtrl*			mRefractionAboveCtrl;
	LLSliderCtrl*			mRefractionBelowCtrl;
	LLSliderCtrl*			mBlurMultiplierCtrl;
	LLTextureCtrl*			mNormalMapCtrl;
	LLXYVector*				mLargeWaveCtrl;
	LLXYVector*				mSmallWaveCtrl;
};

class LLPanelEnvSettingsDay final : public LLPanelEnvSettings
{
	friend class LLFloaterTrackPicker;

protected:
	LOG_CLASS(LLPanelEnvSettingsDay);

public:
	LLPanelEnvSettingsDay();
	~LLPanelEnvSettingsDay() override;

	bool postBuild() override;
	void draw() override;
	void refresh() override;

	void setEnabled(bool enabled) override;

	void setSettings(const LLSettingsBase::ptr_t& settings,
					 bool reset_dirty = true) override;
	LLSettingsBase::ptr_t getSettingsClone() override;

	bool hasLocalTextures(LLSD& args) override;
	void updateEditEnvironment() override;
	void updateLocal() override;
	void updateParcel(S32 parcel_id) override;
	void updateRegion() override;

	void setDirty(bool b = true) override;
	bool isDirty() const override;

	void setFileLoadingAvailable(bool available) override;

	LL_INLINE std::string getSettingsName() override
	{
		return mDaySettings->getName();
	}

	LL_INLINE void setSettingsName(const std::string& name) override
	{
		mDaySettings->setName(name);
	}

	LL_INLINE bool settingsValid() const override
	{
		return mDaySettings != NULL;
	}

	void setDay(const LLSettingsDay::ptr_t& settings, bool reset_dirty = true);

	LL_INLINE LLSettingsDay::ptr_t getDay() const
	{
		return mDaySettings;
	}

	LL_INLINE void setDayLength(S32 seconds)		{ mDayLength = seconds; }

	void loadInventoryItem(LLUUID item_id);

protected:
	void onPickerCommitTrackId(S32 track_id);

private:
	// For map of sliders to parameters
	class FrameData
	{
	public:
		FrameData()
		:	mFrame(0.f)
		{
		}

		FrameData(F32 frame, LLSettingsBase::ptr_t settings)
		:	mFrame(frame),
			mSettings(settings)
		{
		}

	public:
		LLSettingsBase::ptr_t	mSettings;
		F32						mFrame;
	};

	void synchronizePanels();
	void updatePanels();
	void closePickerFloaters();

	void addSliderFrame(F32 frame, const LLSettingsBase::ptr_t& setting,
						bool update_ui = true);
	void removeCurrentSliderFrame();
	void removeSliderFrame(F32 frame);
	void updateSlider();
	void selectFrame(F32 frame, F32 slop_factor);
	void selectTrack(S32 track_index, bool force = false);
	void cloneTrack(const LLSettingsDay::ptr_t& src_day, S32 src_idx,
					S32 dst_idx);

	bool isAddingFrameAllowed();
	bool isRemovingFrameAllowed();

	void updateTimeText();

	void reblendSettings();

	void onAssetLoaded(const LLUUID& item_id, LLSettingsBase::ptr_t settings,
					   S32 status);

	void startPlay();
	void stopPlay();

	static void* createSkySettingsPanel(void* userdata);
	static void* createWaterSettingsPanel(void* userdata);

	static void onIdlePlay(void* userdata);

	static void onTrack0Button(void* userdata);
	static void onTrack1Button(void* userdata);
	static void onTrack2Button(void* userdata);
	static void onTrack3Button(void* userdata);
	static void onTrack4Button(void* userdata);
	static void onCloneTrack(void* userdata);
	static void onLoadTrack(void* userdata);
	static void onClearTrack(void* userdata);
	static void onAddFrame(void* userdata);
	static void onLoadFrame(void* userdata);
	static void onRemoveFrame(void* userdata);
	static void onPlay(void* userdata);
	static void onStop(void* userdata);
	static void onForward(void* userdata);
	static void onBackward(void* userdata);
	static void onTimeSliderCallback(LLUICtrl*, void* userdata);
	static void onFrameSliderCallback(LLUICtrl*, void* userdata);
	static void onFrameSliderMouseDown(S32 x, S32 y, void* userdata);
	static void onFrameSliderMouseUp(S32 x, S32 y, void* userdata);

protected:
	LLSettingsDay::ptr_t				mDaySettings;

private:
	LLButton*							mWaterTrackBtn;
	LLButton*							mSky1TrackBtn;
	LLButton*							mSky2TrackBtn;
	LLButton*							mSky3TrackBtn;
	LLButton*							mSky4TrackBtn;
	LLButton*							mCloneTrackBtn;
	LLButton*							mLoadTrackBtn;
	LLButton*							mClearTrackBtn;
	LLButton*							mAddFrameBtn;
	LLButton*							mLoadFrameBtn;
	LLButton*							mDeleteFrameBtn;
	LLButton*							mPlayBtn;
	LLButton*							mStopBtn;
	LLButton*							mForwardBtn;
	LLButton*							mBackwardBtn;
	LLTextBox*							mEditLockedText;
	LLTextBox*							mCurrentTimeText;
	LLMultiSliderCtrl*					mTimeSlider;
	LLMultiSliderCtrl*					mFramesSlider;
	LLPanelEnvSettingsSky*				mSkyPanel;
	LLPanelEnvSettingsWater*			mWaterPanel;

	std::vector<LLButton*>				mTrackButtons;

	// Source settings copied on callback from to the track selector, used
	// by onPickerCommitTrackId().
	LLSettingsDay::ptr_t				mSourceSettings;

	LLSettingsSky::ptr_t				mScratchSky;
	LLSettingsWater::ptr_t				mScratchWater;

	LLTrackBlenderLoopingManual::ptr_t	mSkyBlender;
	LLTrackBlenderLoopingManual::ptr_t	mWaterBlender;

	std::string							mWaterLabel;
	std::string							mSkyLabel;

	typedef std::map<std::string, FrameData> keymap_t;
	keymap_t							mSliderKeyMap;

	LLFrameTimer						mPlayTimer;
	F32									mPlayStartFrame;

	// *HACK: to work around a race condition on asset loading at panel
	// creation (and initial refresh) time, in order to get the water and sky
	// settings refreshed properly...
	F32									mOnOpenRefreshTime;

	// Used only for onFrameSliderMouseDown() and onFrameSliderMouseUp()
	F32									mCurrentFrame;

	S32									mCurrentTrack;
	S32									mDayLength;

	bool								mIsPlaying;
};

#endif // LL_LLPANELENVSETTINGS_H
