/**
 * @file llsettingsdaycycle.h
 * @brief The day cycles settings asset support class.
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 *
 * Copyright (c) 2001-2019, Linden Research, Inc.
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

#ifndef LL_SETTINGSDAYCYCLE_H
#define LL_SETTINGSDAYCYCLE_H

#include "llsettingsbase.h"

class LLSettingsSky;
class LLSettingsWater;

class LLSettingsDay : public LLSettingsBase
{
protected:
	LOG_CLASS(LLSettingsDay);

	LLSettingsDay();

	void updateSettings() override						{}

public:
	LLSettingsDay(const LLSD& data);

	~LLSettingsDay() override							{}

	static const std::string SETTING_KEYID;
	static const std::string SETTING_KEYNAME;
	static const std::string SETTING_KEYKFRAME;
	static const std::string SETTING_KEYHASH;
	static const std::string SETTING_TRACKS;
	static const std::string SETTING_FRAMES;

	static constexpr S32 MINIMUM_DAYLENGTH = 14400;		// 4 hours
	static constexpr S32 DEFAULT_DAYLENGTH = 14400;		// 4 hours
	static constexpr S32 MAXIMUM_DAYLENGTH = 604800;	// 7 days

	static constexpr S32 MINIMUM_DAYOFFSET = 0;
	static constexpr S32 DEFAULT_DAYOFFSET = 57600;
	static constexpr S32 MAXIMUM_DAYOFFSET = 86400;		// 24 hours

	// Not constexpr because this allows to name more tracks more easily, and
	// possibly add new ones easily as well in the future.
	enum
	{
		TRACK_WATER 		= 0,
		TRACK_GROUND_LEVEL	= 1,
		TRACK_SKY_LEVEL1	= 2,
		TRACK_SKY_LEVEL2	= 3,
		TRACK_SKY_LEVEL3	= 4,
		TRACK_MAX			= 5		// 4 skys + 1 water
	};

	static constexpr S32 FRAME_MAX = 56;

	static constexpr F32 DEFAULT_FRAME_SLOP_FACTOR = 0.02501f;

	static const LLUUID DEFAULT_ASSET_ID;

	// These are alias for LLSettingsWater::ptr_t and LLSettingsSky::ptr_t
	// respectively. Here for definitions only.
	typedef std::shared_ptr<LLSettingsWater> settings_water_ptr_t;
	typedef std::shared_ptr<LLSettingsSky> settings_sky_ptr_t;

	typedef std::shared_ptr<LLSettingsDay> ptr_t;
	typedef std::weak_ptr<LLSettingsDay> wptr_t;

	typedef std::vector<F32> keyframe_list_t;
	typedef std::map<F32, LLSettingsBase::ptr_t> cycle_track_t;
	typedef cycle_track_t::iterator cycle_track_it_t;
	typedef cycle_track_t::reverse_iterator cycle_track_rit_t;
	typedef std::vector<cycle_track_t> cycle_list_t;
	typedef std::pair<cycle_track_it_t, cycle_track_it_t> track_bound_t;

	bool initialize(bool validate_frames = false);

	LL_INLINE void setInitialized(bool b = true)		{ mInitialized = b; }

	LLSD getSettings() const override;
	LL_INLINE LLSettingsType::EType getSettingsTypeValue() const override
	{
		return LLSettingsType::ST_DAYCYCLE;
	}

	LL_INLINE std::string getSettingsType() const override
	{
		return "daycycle";
	}

	// Settings status
	void blend(const LLSettingsBase::ptr_t&, F64) override;

	static LLSD defaults();

	keyframe_list_t getTrackKeyframes(S32 track);
	bool moveTrackKeyframe(S32 track, F32 old_frame, F32 new_frame);
	bool removeTrackKeyframe(S32 track, F32 frame);

	void setWaterAtKeyframe(const settings_water_ptr_t& water, F32 keyframe);
	settings_water_ptr_t getWaterAtKeyframe(F32 keyframe) const;

	void setSkyAtKeyframe(const settings_sky_ptr_t& sky, F32 keyframe,
						  S32 track);
	settings_sky_ptr_t getSkyAtKeyframe(F32 keyframe, S32 track) const;

	void setSettingsAtKeyframe(const LLSettingsBase::ptr_t& settings,
							   F32 keyframe, S32 track);
	LLSettingsBase::ptr_t getSettingsAtKeyframe(F32 keyframe, S32 track) const;
	cycle_track_t::value_type getSettingsNearKeyframe(F32 keyframe, S32 track,
													  F32 fudge) const;

	void startDayCycle();

	cycle_track_t& getCycleTrack(S32 track);
	const cycle_track_t& getCycleTrackConst(S32 track) const;
	bool clearCycleTrack(S32 track);
	bool replaceCycleTrack(S32 track, const cycle_track_t& source);
	bool isTrackEmpty(S32 track) const;

	virtual settings_sky_ptr_t getDefaultSky() const = 0;
	virtual settings_water_ptr_t getDefaultWater() const = 0;

	virtual settings_sky_ptr_t buildSky(const LLSD& settings) const = 0;
	virtual settings_water_ptr_t buildWater(const LLSD& settings) const = 0;

	virtual ptr_t buildClone() const = 0;
	virtual ptr_t buildDeepCloneAndUncompress() const = 0;

	LL_INLINE LLSettingsBase::ptr_t buildDerivedClone() const override
	{
		return buildClone();
	}

	const validation_list_t& getValidationList() const override;
	static const validation_list_t& validationList();

	F32 getUpperBoundFrame(S32 track, F32 keyframe);
	F32 getLowerBoundFrame(S32 track, F32 keyframe);

	static const LLUUID& getDefaultAssetId();

private:
	void parseFromLLSD(LLSD& data);
	static cycle_track_it_t getEntryAtOrBefore(cycle_track_t& track,
											   F32 keyframe);
	static cycle_track_it_t getEntryAtOrAfter(cycle_track_t& track,
											  F32 keyframe);
	track_bound_t getBoundingEntries(cycle_track_t& track, F32 keyframe);

	static bool validateDayCycleTrack(LLSD& value, U32 flags);
	static bool validateDayCycleFrames(LLSD& value, U32 flags);

private:
	F64				mLastUpdateTime;
	cycle_list_t	mDayTracks;

protected:
	bool			mInitialized;
};

#endif	// LL_SETTINGSDAYCYCLE_H
