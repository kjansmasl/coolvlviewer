/**
 * @file llsettingssky.h
 * @brief The sky settings asset support class.
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

#ifndef LL_SETTINGSSKY_H
#define LL_SETTINGSSKY_H

#include "llsettingsbase.h"

#include "imageids.h"

// Change this to 1 whenever LL implements cariable sky domes for extended
// environment... For now (and this was the case with Windlight as well), the
// sky dome is of fixed size and the SKY_DOME_OFFSET and SKY_DOME_RADIUS below
// are used.
#define LL_VARIABLE_SKY_DOME_SIZE 0

constexpr F32 SKY_DOME_OFFSET = 0.96f;
constexpr F32 SKY_DOME_RADIUS = 15000.f;

class LLSettingsSky : public LLSettingsBase
{
protected:
	LOG_CLASS(LLSettingsSky);

	LLSettingsSky();

	const stringset_t& getSlerpKeys() const override;
	const stringset_t& getSkipInterpolateKeys() const override;

public:
	LLSettingsSky(const LLSD& data);
	~LLSettingsSky() override							{}

	typedef std::shared_ptr<LLSettingsSky> ptr_t;
	virtual ptr_t buildClone() const = 0;

	LL_INLINE LLSettingsBase::ptr_t buildDerivedClone() const override
	{
		return buildClone();
	}

	LL_INLINE std::string getSettingsType() const override
	{
		return "sky";
	}

	LL_INLINE LLSettingsType::EType getSettingsTypeValue() const override
	{
		return LLSettingsType::ST_SKY;
	}

	// Settings status
	void blend(const LLSettingsBase::ptr_t& end, F64 blendfactor) override;

	void replaceSettings(const LLSD& settings) override;

	void replaceWithSky(ptr_t otherp);
	static LLSD defaults(F32 position = 0.f);

	LL_INLINE F32 getSkyMoistureLevel() const
	{
		update();
		return mSkyMoistureLevel;
	}

	LL_INLINE F32 getSkyDropletRadius() const
	{
		update();
		return mSkyDropletRadius;
	}

	LL_INLINE F32 getSkyIceLevel() const
	{
		update();
		return mSkyIceLevel;
	}

	LL_INLINE LLUUID getBloomTextureId() const
	{
		return mSettings[SETTING_BLOOM_TEXTUREID].asUUID();
	}

	LL_INLINE LLUUID getRainbowTextureId() const
	{
		return mSettings[SETTING_RAINBOW_TEXTUREID].asUUID();
	}

	LL_INLINE LLUUID getHaloTextureId() const
	{
		return mSettings[SETTING_HALO_TEXTUREID].asUUID();
	}

	LL_INLINE void setSkyMoistureLevel(F32 moisture_level)
	{
		setValue(SETTING_SKY_MOISTURE_LEVEL, moisture_level);
	}

	LL_INLINE void setSkyDropletRadius(F32 radius)
	{
		setValue(SETTING_SKY_DROPLET_RADIUS, radius);
	}

	LL_INLINE void setSkyIceLevel(F32 ice_level)
	{
		setValue(SETTING_SKY_ICE_LEVEL, ice_level);
	}

	LL_INLINE void setReflectionProbeAmbiance(F32 val)
	{
		setValue(SETTING_REFLECTION_PROBE_AMBIANCE, val);
		setDirtyFlag(true);
	}

	LL_INLINE F32 getReflectionProbeAmbiance(bool auto_adjust = false) const
	{
		update();
		return auto_adjust && mCanAutoAdjust ? sAutoAdjustProbeAmbiance
											 : mReflectionProbeAmbiance;
	}

	// Removes entirely the probe ambiance parameter to turn the sky settings
	// back to a legacy (pre-PBR) sky and let the final users choose whether to
	// auto-adjust the probe ambiance for HDR display or not, depending on what
	// their monitor can manage. HB
	void removeProbeAmbiance();

	LL_INLINE bool canAutoAdjust() const
	{
		update();
		return mCanAutoAdjust;
	}

	LL_INLINE LLColor3 getAmbientColor() const
	{
		static const LLColor3 default_color(0.25f, 0.25f, 0.25f);
		return getColor(SETTING_AMBIENT, default_color);
	}

	LLColor3 getAmbientColorClamped() const;

	LL_INLINE void setAmbientColor(const LLColor3& val)
	{
		mSettings[SETTING_LEGACY_HAZE][SETTING_AMBIENT] = val.getValue();
		setDirtyFlag(true);
	}

	LL_INLINE LLColor3 getCloudColor() const
	{
		update();
		return mCloudColor;
	}

	LL_INLINE void setCloudColor(const LLColor3& val)
	{
		setValue(SETTING_CLOUD_COLOR, val);
	}

	LL_INLINE LLUUID getCloudNoiseTextureId() const
	{
		return mSettings[SETTING_CLOUD_TEXTUREID].asUUID();
	}

	LL_INLINE void setCloudNoiseTextureId(const LLUUID& id)
	{
		setValue(SETTING_CLOUD_TEXTUREID, LLSD(id));
	}

	LL_INLINE LLColor3 getCloudPosDensity1() const
	{
		update();
		return mCloudPosDensity1;
	}

	LL_INLINE void setCloudPosDensity1(const LLColor3& val)
	{
		setValue(SETTING_CLOUD_POS_DENSITY1, val);
	}

	// NOTE: getCloudPosDensity2() is not actually used for rendering... HB
	LL_INLINE LLColor3 getCloudPosDensity2() const
	{
		return LLColor3(mSettings[SETTING_CLOUD_POS_DENSITY2]);
	}

	// NOTE: setCloudPosDensity2() is not actually used for rendering... HB
	LL_INLINE void setCloudPosDensity2(const LLColor3& val)
	{
		setValue(SETTING_CLOUD_POS_DENSITY2, val);
	}

	// NOTE: getCloudScale() is not actually used for rendering... HB
	LL_INLINE F32 getCloudScale() const
	{
		return mSettings[SETTING_CLOUD_SCALE].asReal();
	}

	// NOTE: setCloudScale() is not actually used for rendering... HB
	LL_INLINE void setCloudScale(F32 val)
	{
		setValue(SETTING_CLOUD_SCALE, val);
	}

	LL_INLINE LLVector2 getCloudScrollRate() const
	{
		update();
		return mCloudScrollRate;
	}

	LL_INLINE void setCloudScrollRate(const LLVector2& val)
	{
		setValue(SETTING_CLOUD_SCROLL_RATE, val);
	}

	LL_INLINE void setCloudScrollRateX(F32 val)
	{
		mSettings[SETTING_CLOUD_SCROLL_RATE][0] = val;
		setDirtyFlag(true);
	}

	LL_INLINE void setCloudScrollRateY(F32 val)
	{
		mSettings[SETTING_CLOUD_SCROLL_RATE][1] = val;
		setDirtyFlag(true);
	}

	LL_INLINE F32 getCloudShadow() const
	{
		update();
		return mCloudShadow;
	}

	LL_INLINE void setCloudShadow(F32 val)
	{
		setValue(SETTING_CLOUD_SHADOW, val);
	}

	LL_INLINE F32 getCloudVariance() const
	{
		update();
		return mCloudVariance;
	}

	LL_INLINE void setCloudVariance(F32 val)
	{
		setValue(SETTING_CLOUD_VARIANCE, val);
	}

#if LL_VARIABLE_SKY_DOME_SIZE
	F32 getDomeOffset() const;
	F32 getDomeRadius() const;
#else
	LL_INLINE F32 getDomeOffset() const					{ return SKY_DOME_OFFSET; }
	LL_INLINE F32 getDomeRadius() const					{ return SKY_DOME_RADIUS; }
#endif

	LL_INLINE F32 getGamma() const
	{
		update();
		return mGamma;
	}

	LL_INLINE void setGamma(F32 val)
	{
		mSettings[SETTING_GAMMA] = LLSD::Real(val);
		setDirtyFlag(true);
	}

	LL_INLINE LLColor3 getGlow() const
	{
		update();
		return mGlow;
	}

	LL_INLINE void setGlow(const LLColor3& val)
	{
		setValue(SETTING_GLOW, val);
	}

	LL_INLINE F32 getMaxY() const
	{
		update();
		return mMaxY;
	}

	LL_INLINE void setMaxY(F32 val)
	{
		setValue(SETTING_MAX_Y, val);
	}

	LL_INLINE LLQuaternion getMoonRotation() const
	{
		return LLQuaternion(mSettings[SETTING_MOON_ROTATION]);
	}

	LL_INLINE void setMoonRotation(const LLQuaternion& val)
	{
		setValue(SETTING_MOON_ROTATION, val);
	}

	LL_INLINE F32 getMoonScale() const
	{
		update();
		return mMoonScale;
	}

	LL_INLINE void setMoonScale(F32 val)
	{
		setValue(SETTING_MOON_SCALE, val);
	}

	LL_INLINE LLUUID getMoonTextureId() const
	{
		return mSettings[SETTING_MOON_TEXTUREID].asUUID();
	}

	LL_INLINE void setMoonTextureId(const LLUUID& id)
	{
		setValue(SETTING_MOON_TEXTUREID, LLSD(id));
	}

	LL_INLINE F32 getMoonBrightness() const
	{
		update();
		return mMoonBrightness;
	}

	LL_INLINE void setMoonBrightness(F32 brightness_factor)
	{
		setValue(SETTING_MOON_BRIGHTNESS, brightness_factor);
	}

	// Color based on brightness
	LL_INLINE LLColor3 getMoonlightColor() const
	{
		return getSunlightColor();	// The Moon reflects the Sun light...
	}

	LL_INLINE F32 getStarBrightness() const
	{
		update();
		return mStarBrightness;
	}

	LL_INLINE void setStarBrightness(F32 val)
	{
		setValue(SETTING_STAR_BRIGHTNESS, val);
	}

	LL_INLINE LLColor3 getSunlightColor() const
	{
		update();
		return mSunlightColor;
	}

	LLColor3 getSunlightColorClamped() const;

	LL_INLINE void setSunlightColor(const LLColor3& val)
	{
		setValue(SETTING_SUNLIGHT_COLOR, val);
	}

	LL_INLINE LLQuaternion getSunRotation() const
	{
		return LLQuaternion(mSettings[SETTING_SUN_ROTATION]);
	}

	LL_INLINE void setSunRotation(const LLQuaternion& val)
	{
		setValue(SETTING_SUN_ROTATION, val);
	}

	LL_INLINE F32 getSunScale() const
	{
		update();
		return mSunScale;
	}

	LL_INLINE void setSunScale(F32 val)
	{
		setValue(SETTING_SUN_SCALE, val);
	}

	LL_INLINE LLUUID getSunTextureId() const
	{
		return mSettings[SETTING_SUN_TEXTUREID].asUUID();
	}

	LL_INLINE void setSunTextureId(const LLUUID& id)
	{
		setValue(SETTING_SUN_TEXTUREID, LLSD(id));
	}

	// Transient properties used in animations.

	LL_INLINE LLUUID getNextSunTextureId() const		{ return mNextSunTextureId; }
	LL_INLINE LLUUID getNextMoonTextureId() const		{ return mNextMoonTextureId; }
	LL_INLINE LLUUID getNextBloomTextureId() const		{ return mNextBloomTextureId; }

	LL_INLINE LLUUID getNextCloudNoiseTextureId() const
	{
		return mNextCloudTextureId;
	}

	const validation_list_t& getValidationList() const override;
	static const validation_list_t& validationList();

	static LLSD translateLegacySettings(const LLSD& legacy);

	// Legacy_atmospherics
	static LLSD translateLegacyHazeSettings(const LLSD& legacy);

	static LLColor3 gammaCorrect(const LLColor3& in, F32 gamma);

	LLColor3 getTotalDensity() const;
	// Fast version to avoid calling slow getColor() and getFloat() many times
	LL_INLINE static LLColor3 totalDensity(const LLColor3& bd, F32 hd)
	{
		return bd + smear(hd);
	}

	LLColor3 getLightAttenuation(F32 distance) const;
	// Fast version to avoid calling slow getColor() and getFloat() many times
	LL_INLINE static LLColor3 lightAttenuation(const LLColor3& bd, F32 hd,
											   F32 dm, F32 dist)
	{
		return (bd + smear(hd * 0.25f)) * (dm * dist);
	}

	LLColor3 getLightTransmittance(F32 distance) const;
	// Fast version to avoid calling slow getColor() and getFloat() many times
	LL_INLINE static LLColor3 lightTransmittance(const LLColor3& td, F32 dm,
												 F32 dist)
	{
		return componentExp(td * (-dm * dist));
	}

	LL_INLINE LLColor3 getBlueDensity() const
	{
		static const LLColor3 default_color(0.2447f, 0.4487f, 0.7599f);
		return getColor(SETTING_BLUE_DENSITY, default_color);
	}

	LL_INLINE LLColor3 getBlueHorizon() const
	{
		static const LLColor3 default_color(0.4954f, 0.4954f, 0.6399f);
		return getColor(SETTING_BLUE_HORIZON, default_color);
	}

	LL_INLINE F32 getHazeDensity() const
	{
		return getFloat(SETTING_HAZE_DENSITY, 0.7f);
	}

	LL_INLINE F32 getHazeHorizon() const
	{
		return getFloat(SETTING_HAZE_HORIZON, 0.19f);
	}

	LL_INLINE F32 getDensityMultiplier() const
	{
		update();
		return mDensityMultiplier;
	}

	LL_INLINE F32 getDistanceMultiplier() const
	{
		update();
		return mDistanceMultiplier;
	}

	LL_INLINE void setBlueDensity(const LLColor3& val)
	{
		mSettings[SETTING_LEGACY_HAZE][SETTING_BLUE_DENSITY] = val.getValue();
		setDirtyFlag(true);
	}

	LL_INLINE void setBlueHorizon(const LLColor3& val)
	{
		mSettings[SETTING_LEGACY_HAZE][SETTING_BLUE_HORIZON] = val.getValue();
		setDirtyFlag(true);
	}

	LL_INLINE void setDensityMultiplier(F32 val)
	{
		mSettings[SETTING_LEGACY_HAZE][SETTING_DENSITY_MULTIPLIER] = val;
		setDirtyFlag(true);
	}

	LL_INLINE void setDistanceMultiplier(F32 val)
	{
		mSettings[SETTING_LEGACY_HAZE][SETTING_DISTANCE_MULTIPLIER] = val;
		setDirtyFlag(true);
	}

	LL_INLINE void setHazeDensity(F32 val)
	{
		mSettings[SETTING_LEGACY_HAZE][SETTING_HAZE_DENSITY] = val;
		setDirtyFlag(true);
	}

	LL_INLINE void setHazeHorizon(F32 val)
	{
		mSettings[SETTING_LEGACY_HAZE][SETTING_HAZE_HORIZON] = val;
		setDirtyFlag(true);
	}

	// Internal/calculated settings
	bool getIsSunUp() const;
	bool getIsMoonUp() const;

	// Determines how much the haze glow effect occurs in rendering
	F32 getSunMoonGlowFactor() const;

	LLVector3 getLightDirection() const;
	LLColor3 getLightDiffuse() const;

	LL_INLINE const LLVector3& getSunDirection() const
	{
		update();
		return mSunDirection;
	}

	LL_INLINE const LLVector3& getMoonDirection() const
	{
		update();
		return mMoonDirection;
	}

	LL_INLINE const LLColor4& getMoonAmbient() const
	{
		update();
		return mMoonAmbient;
	}

	LL_INLINE const LLColor3& getMoonDiffuse() const
	{
		update();
		return mMoonDiffuse;
	}

	LL_INLINE const LLColor4& getSunAmbient() const
	{
		update();
		return mSunAmbient;
	}

	LL_INLINE const LLColor3& getSunDiffuse() const
	{
		update();
		return mSunDiffuse;
	}

	LL_INLINE const LLColor4& getTotalAmbient() const
	{
		update();
		return mTotalAmbient;
	}

	LL_INLINE const LLColor4& getHazeColor() const
	{
		update();
		return mHazeColor;
	}

	LL_INLINE static const LLUUID& getDefaultAssetId()	{ return DEFAULT_ASSET_ID; }

	LL_INLINE static const LLUUID& getDefaultSunTextureId()
	{
		return LLUUID::null;
	}

	LL_INLINE static const LLUUID& getBlankSunTextureId()
	{
		return DEFAULT_SUN_ID;
	}

	LL_INLINE static const LLUUID& getDefaultMoonTextureId()
	{
		return DEFAULT_MOON_ID;
	}

	LL_INLINE static const LLUUID& getDefaultCloudNoiseTextureId()
	{
		return DEFAULT_CLOUD_ID;
	}

	LL_INLINE static const LLUUID& getDefaultBloomTextureId()
	{
		return IMG_BLOOM1;
	}

	LL_INLINE static const LLUUID& getDefaultRainbowTextureId()
	{
		return IMG_RAINBOW;
	}

	LL_INLINE static const LLUUID& getDefaultHaloTextureId()
	{
		return IMG_HALO;
	}

	static LLSD createDensityProfileLayer(F32 width, F32 exponential_term,
										  F32 exponential_scale_factor,
										  F32 linear_term, F32 constant_term,
										  F32 aniso_factor = 0.f);

	static LLSD createSingleLayerDensityProfile(F32 width,
												F32 exponential_term,
												F32 exponential_scale_factor,
												F32 linear_term,
												F32 constant_term,
												F32 aniso_factor = 0.f);

	void updateSettings() override;

private:
	LLColor3 getColor(const std::string& key,
					  const LLColor3& default_value) const;
	F32 getFloat(const std::string& key, F32 default_value) const;

	void calculateHeavenlyBodyPositions() const;
	void calculateLightSettings() const;

	static LLSD rayleighConfigDefault();
	static LLSD absorptionConfigDefault();
	static LLSD mieConfigDefault();

	static validation_list_t legacyHazeValidationList();
	static validation_list_t rayleighValidationList();
	static validation_list_t absorptionValidationList();
	static validation_list_t mieValidationList();

	static bool validateLegacyHaze(LLSD& value, U32 flags);
	static bool validateRayleighLayers(LLSD& value, U32 flags);
	static bool validateAbsorptionLayers(LLSD& value, U32 flags);
	static bool validateMieLayers(LLSD& value, U32 flags);

public:
	static const std::string	SETTING_AMBIENT;
	static const std::string	SETTING_BLOOM_TEXTUREID;
	static const std::string	SETTING_RAINBOW_TEXTUREID;
	static const std::string	SETTING_HALO_TEXTUREID;
	static const std::string	SETTING_BLUE_DENSITY;
	static const std::string	SETTING_BLUE_HORIZON;
	static const std::string	SETTING_DENSITY_MULTIPLIER;
	static const std::string	SETTING_DISTANCE_MULTIPLIER;
	static const std::string	SETTING_HAZE_DENSITY;
	static const std::string	SETTING_HAZE_HORIZON;
	static const std::string	SETTING_CLOUD_COLOR;
	static const std::string	SETTING_CLOUD_POS_DENSITY1;
	static const std::string	SETTING_CLOUD_POS_DENSITY2;
	static const std::string	SETTING_CLOUD_SCALE;
	static const std::string	SETTING_CLOUD_SCROLL_RATE;
	static const std::string	SETTING_CLOUD_SHADOW;
	static const std::string	SETTING_CLOUD_TEXTUREID;
	static const std::string	SETTING_CLOUD_VARIANCE;

	static const std::string	SETTING_DOME_OFFSET;
	static const std::string	SETTING_DOME_RADIUS;
	static const std::string	SETTING_GAMMA;
	static const std::string	SETTING_GLOW;
	static const std::string	SETTING_LIGHT_NORMAL;
	static const std::string	SETTING_MAX_Y;
	static const std::string	SETTING_MOON_ROTATION;
	static const std::string	SETTING_MOON_SCALE;
	static const std::string	SETTING_MOON_TEXTUREID;
	static const std::string	SETTING_MOON_BRIGHTNESS;

	static const std::string	SETTING_STAR_BRIGHTNESS;
	static const std::string	SETTING_SUNLIGHT_COLOR;
	static const std::string	SETTING_SUN_ROTATION;
	static const std::string	SETTING_SUN_SCALE;
	static const std::string	SETTING_SUN_TEXTUREID;

	static const std::string	SETTING_PLANET_RADIUS;
	static const std::string	SETTING_SKY_BOTTOM_RADIUS;
	static const std::string	SETTING_SKY_TOP_RADIUS;
	static const std::string	SETTING_SUN_ARC_RADIANS;
	static const std::string	SETTING_MIE_ANISOTROPY_FACTOR;

	static const std::string	SETTING_RAYLEIGH_CONFIG;
	static const std::string	SETTING_MIE_CONFIG;
	static const std::string	SETTING_ABSORPTION_CONFIG;

	static const std::string	KEY_DENSITY_PROFILE;
	static const std::string	SETTING_DENSITY_PROFILE_WIDTH;
	static const std::string	SETTING_DENSITY_PROFILE_EXP_TERM;
	static const std::string	SETTING_DENSITY_PROFILE_EXP_SCALE_FACTOR;
	static const std::string	SETTING_DENSITY_PROFILE_LINEAR_TERM;
	static const std::string	SETTING_DENSITY_PROFILE_CONSTANT_TERM;

	static const std::string	SETTING_SKY_MOISTURE_LEVEL;
	static const std::string	SETTING_SKY_DROPLET_RADIUS;
	static const std::string	SETTING_SKY_ICE_LEVEL;

	static const std::string	SETTING_REFLECTION_PROBE_AMBIANCE;

	static const std::string	SETTING_LEGACY_HAZE;

	static const LLUUID			DEFAULT_ASSET_ID;

	static F32					sAutoAdjustProbeAmbiance;

protected:
	static const std::string	SETTING_LEGACY_EAST_ANGLE;
	static const std::string	SETTING_LEGACY_ENABLE_CLOUD_SCROLL;
	static const std::string	SETTING_LEGACY_SUN_ANGLE;

	LLUUID	  					mNextSunTextureId;
	LLUUID	  					mNextMoonTextureId;
	LLUUID	  					mNextCloudTextureId;
	LLUUID	  					mNextBloomTextureId;
	LLUUID	  					mNextRainbowTextureId;
	LLUUID	  					mNextHaloTextureId;

private:
	mutable LLColor4			mMoonAmbient;
	mutable LLColor4			mSunAmbient;
	mutable LLColor4			mTotalAmbient;
	mutable LLColor4			mHazeColor;
	mutable LLVector2			mCloudScrollRate;
	mutable LLVector3			mSunDirection;
	mutable LLVector3			mMoonDirection;
	mutable LLVector3			mLightDirection;
	mutable LLColor3			mMoonDiffuse;
	mutable LLColor3			mSunDiffuse;
	mutable LLColor3			mCloudPosDensity1;
	mutable LLColor3			mSunlightColor;
	mutable LLColor3			mCloudColor;
	mutable LLColor3			mGlow;
	mutable F32					mDensityMultiplier;
	mutable F32					mDistanceMultiplier;
	mutable F32					mGamma;
	mutable F32					mMaxY;
	mutable F32					mSunScale;
	mutable F32					mMoonScale;
	mutable F32					mMoonBrightness;
	mutable F32					mStarBrightness;
	mutable F32					mSkyMoistureLevel;
	mutable F32					mSkyDropletRadius;
	mutable F32					mSkyIceLevel;
	mutable F32					mCloudShadow;
	mutable F32					mCloudVariance;
	mutable F32					mReflectionProbeAmbiance;
	// If true, this sky is a candidate for auto-adjustment in PBR viewers.
	mutable bool				mCanAutoAdjust;
};

#endif	// LL_SETTINGSSKY_H
