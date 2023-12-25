/**
 * @file llsettingssky.cpp
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

#include "linden_common.h"

#include "llsettingssky.h"

const std::string LLSettingsSky::SETTING_AMBIENT = "ambient";
const std::string LLSettingsSky::SETTING_BLUE_DENSITY = "blue_density";
const std::string LLSettingsSky::SETTING_BLUE_HORIZON = "blue_horizon";
const std::string LLSettingsSky::SETTING_DENSITY_MULTIPLIER =
	"density_multiplier";
const std::string LLSettingsSky::SETTING_DISTANCE_MULTIPLIER =
	"distance_multiplier";
const std::string LLSettingsSky::SETTING_HAZE_DENSITY = "haze_density";
const std::string LLSettingsSky::SETTING_HAZE_HORIZON = "haze_horizon";

const std::string LLSettingsSky::SETTING_BLOOM_TEXTUREID = "bloom_id";
const std::string LLSettingsSky::SETTING_RAINBOW_TEXTUREID = "rainbow_id";
const std::string LLSettingsSky::SETTING_HALO_TEXTUREID = "halo_id";
const std::string LLSettingsSky::SETTING_CLOUD_COLOR = "cloud_color";
const std::string LLSettingsSky::SETTING_CLOUD_POS_DENSITY1 =
	"cloud_pos_density1";
const std::string LLSettingsSky::SETTING_CLOUD_POS_DENSITY2 =
	"cloud_pos_density2";
const std::string LLSettingsSky::SETTING_CLOUD_SCALE = "cloud_scale";
const std::string LLSettingsSky::SETTING_CLOUD_SCROLL_RATE =
	"cloud_scroll_rate";
const std::string LLSettingsSky::SETTING_CLOUD_SHADOW = "cloud_shadow";
const std::string LLSettingsSky::SETTING_CLOUD_TEXTUREID = "cloud_id";
const std::string LLSettingsSky::SETTING_CLOUD_VARIANCE = "cloud_variance";

const std::string LLSettingsSky::SETTING_DOME_OFFSET = "dome_offset";
const std::string LLSettingsSky::SETTING_DOME_RADIUS = "dome_radius";
const std::string LLSettingsSky::SETTING_GAMMA = "gamma";
const std::string LLSettingsSky::SETTING_GLOW = "glow";

const std::string LLSettingsSky::SETTING_LIGHT_NORMAL = "lightnorm";
const std::string LLSettingsSky::SETTING_MAX_Y = "max_y";
const std::string LLSettingsSky::SETTING_MOON_ROTATION = "moon_rotation";
const std::string LLSettingsSky::SETTING_MOON_SCALE = "moon_scale";
const std::string LLSettingsSky::SETTING_MOON_TEXTUREID = "moon_id";
const std::string LLSettingsSky::SETTING_MOON_BRIGHTNESS = "moon_brightness";

const std::string LLSettingsSky::SETTING_STAR_BRIGHTNESS = "star_brightness";
const std::string LLSettingsSky::SETTING_SUNLIGHT_COLOR = "sunlight_color";
const std::string LLSettingsSky::SETTING_SUN_ROTATION = "sun_rotation";
const std::string LLSettingsSky::SETTING_SUN_SCALE = "sun_scale";
const std::string LLSettingsSky::SETTING_SUN_TEXTUREID = "sun_id";

const std::string LLSettingsSky::SETTING_LEGACY_EAST_ANGLE = "east_angle";
const std::string LLSettingsSky::SETTING_LEGACY_ENABLE_CLOUD_SCROLL =
	"enable_cloud_scroll";
const std::string LLSettingsSky::SETTING_LEGACY_SUN_ANGLE = "sun_angle";

const std::string LLSettingsSky::SETTING_LEGACY_HAZE = "legacy_haze";

const std::string LLSettingsSky::KEY_DENSITY_PROFILE = "density";
const std::string LLSettingsSky::SETTING_DENSITY_PROFILE_WIDTH = "width";
const std::string LLSettingsSky::SETTING_DENSITY_PROFILE_EXP_TERM = "exp_term";
const std::string LLSettingsSky::SETTING_DENSITY_PROFILE_EXP_SCALE_FACTOR =
	"exp_scale";
const std::string LLSettingsSky::SETTING_DENSITY_PROFILE_LINEAR_TERM =
	"linear_term";
const std::string LLSettingsSky::SETTING_DENSITY_PROFILE_CONSTANT_TERM =
	"constant_term";

const std::string LLSettingsSky::SETTING_SKY_MOISTURE_LEVEL	= "moisture_level";
const std::string LLSettingsSky::SETTING_SKY_DROPLET_RADIUS	= "droplet_radius";
const std::string LLSettingsSky::SETTING_SKY_ICE_LEVEL = "ice_level";

const std::string LLSettingsSky::SETTING_REFLECTION_PROBE_AMBIANCE = "reflection_probe_ambiance";

// These are settings for an advanced atmospherics model (which was never fully
// implemented) and that are not currently in use for rendering... HB
const std::string LLSettingsSky::SETTING_PLANET_RADIUS = "planet_radius";
const std::string LLSettingsSky::SETTING_SKY_BOTTOM_RADIUS =
	"sky_bottom_radius";
const std::string LLSettingsSky::SETTING_SKY_TOP_RADIUS = "sky_top_radius";
const std::string LLSettingsSky::SETTING_SUN_ARC_RADIANS = "sun_arc_radians";
const std::string LLSettingsSky::SETTING_RAYLEIGH_CONFIG = "rayleigh_config";
const std::string LLSettingsSky::SETTING_MIE_CONFIG = "mie_config";
const std::string LLSettingsSky::SETTING_MIE_ANISOTROPY_FACTOR = "anisotropy";
const std::string LLSettingsSky::SETTING_ABSORPTION_CONFIG =
	"absorption_config";

// Default (fixed) sky settings asset
const LLUUID LLSettingsSky::DEFAULT_ASSET_ID("eb3a7080-831f-9f37-10f0-7b1f9ea4043c");

//static
F32 LLSettingsSky::sAutoAdjustProbeAmbiance = 1.f;

// Helper functions

static LLQuaternion convert_azimuth_and_altitude_to_quat(F32 azimuth,
														 F32 altitude)
{
	F32 sin_theta = sinf(azimuth);
	F32 cos_theta = cosf(azimuth);
	F32 sin_phi = sinf(altitude);
	F32 cos_phi = cosf(altitude);

	// +x right, +z up, +y at...
	LLVector3 dir(cos_theta * cos_phi, sin_theta * cos_phi, sin_phi);

	LLVector3 axis = LLVector3::x_axis % dir;
	axis.normalize();

	F32 angle = acosf(LLVector3::x_axis * dir);

	LLQuaternion quat;
	quat.setAngleAxis(angle, axis);
	return quat;
}

///////////////////////////////////////////////////////////////////////////////
// LLSettingsSky class proper
///////////////////////////////////////////////////////////////////////////////

LLSettingsSky::LLSettingsSky(const LLSD& data)
:	LLSettingsBase(data),
	mDensityMultiplier(0.0001f),
	mDistanceMultiplier(0.8f),
	mGamma(1.f),
	mMaxY(0.f),
	mSunScale(1.f),
	mMoonScale(1.f),
	mMoonBrightness(1.f),
	mStarBrightness(1.f),
	mSkyMoistureLevel(0.f),
	mSkyDropletRadius(0.f),
	mSkyIceLevel(0.f),
	mCloudShadow(0.f),
	mCloudVariance(0.f),
	mReflectionProbeAmbiance(0.f),
	mCanAutoAdjust(true)
{
}

LLSettingsSky::LLSettingsSky()
:	LLSettingsBase(),
	mDensityMultiplier(0.0001f),
	mDistanceMultiplier(0.8f),
	mGamma(1.f),
	mMaxY(0.f),
	mSunScale(1.f),
	mMoonScale(1.f),
	mMoonBrightness(1.f),
	mStarBrightness(1.f),
	mSkyMoistureLevel(0.f),
	mSkyDropletRadius(0.f),
	mSkyIceLevel(0.f),
	mCloudShadow(0.f),
	mCloudVariance(0.f),
	mCanAutoAdjust(true)
{
}

void LLSettingsSky::replaceSettings(const LLSD& settings)
{
	LLSettingsBase::replaceSettings(settings);
	mNextSunTextureId.setNull();
	mNextMoonTextureId.setNull();
	mNextCloudTextureId.setNull();
	mNextBloomTextureId.setNull();
	mNextRainbowTextureId.setNull();
	mNextHaloTextureId.setNull();
	mCanAutoAdjust = !settings.has(SETTING_REFLECTION_PROBE_AMBIANCE);
}

void LLSettingsSky::replaceWithSky(LLSettingsSky::ptr_t otherp)
{
	replaceWith(otherp);
	mNextSunTextureId = otherp->mNextSunTextureId;
	mNextMoonTextureId = otherp->mNextMoonTextureId;
	mNextCloudTextureId = otherp->mNextCloudTextureId;
	mNextBloomTextureId = otherp->mNextBloomTextureId;
	mNextRainbowTextureId = otherp->mNextRainbowTextureId;
	mNextHaloTextureId = otherp->mNextHaloTextureId;
	mCanAutoAdjust = otherp->mCanAutoAdjust;
}

void LLSettingsSky::blend(const LLSettingsBase::ptr_t& end, F64 blendf)
{
	llassert(getSettingsType() == end->getSettingsType());

	LLSettingsSky::ptr_t other = std::dynamic_pointer_cast<LLSettingsSky>(end);
	if (other)
	{
		if (other->mSettings.has(SETTING_LEGACY_HAZE))
		{
			if (!mSettings.has(SETTING_LEGACY_HAZE) ||
				!mSettings[SETTING_LEGACY_HAZE].has(SETTING_AMBIENT))
			{
				// Special case since SETTING_AMBIENT is both in outer and
				// legacy maps, we prioritize legacy one.
				// See getAmbientColor(), we are about to replaceSettings(), so
				// we are free to set it
				setAmbientColor(getAmbientColor());
			}
		}
		else if (mSettings.has(SETTING_LEGACY_HAZE) &&
				 mSettings[SETTING_LEGACY_HAZE].has(SETTING_AMBIENT))
		{
			// Special case due to ambient's duality. We need to match
			// 'other's' structure for interpolation. We are free to change
			// mSettings, since we are about to reset it
			mSettings[SETTING_AMBIENT] = getAmbientColor().getValue();
			mSettings[SETTING_LEGACY_HAZE].erase(SETTING_AMBIENT);
		}

		LLUUID cloud_noise_id = getCloudNoiseTextureId();
		LLUUID cloud_noise_id_next = other->getCloudNoiseTextureId();
		F64 cloud_shadow = 0.0;
		if (!cloud_noise_id.isNull() && cloud_noise_id_next.isNull())
		{
			// If there is no cloud texture in destination, reduce coverage to
			// imitate disappearance. See LLDrawPoolWLSky::renderSkyClouds...
			// We do not blend present texture with null Note: probably can be
			// done by shader
			cloud_shadow = lerp(mSettings[SETTING_CLOUD_SHADOW].asReal(), 0.0,
								blendf);
			cloud_noise_id_next = cloud_noise_id;
		}
		else if (cloud_noise_id.isNull() && !cloud_noise_id_next.isNull())
		{
			// Source has no cloud texture, reduce initial coverage to imitate
			// appearance use same texture as destination.
			cloud_shadow =
				lerp(0.0, other->mSettings[SETTING_CLOUD_SHADOW].asReal(),
					 blendf);
			setCloudNoiseTextureId(cloud_noise_id_next);
		}
		else
		{
			cloud_shadow =
				lerp(mSettings[SETTING_CLOUD_SHADOW].asReal(),
					 other->mSettings[SETTING_CLOUD_SHADOW].asReal(), blendf);
		}

		LLSD blenddata = interpolateSDMap(mSettings, other->mSettings,
										  other->getParameterMap(), blendf);
		blenddata[SETTING_CLOUD_SHADOW] = LLSD::Real(cloud_shadow);
		replaceSettings(blenddata);
		mNextSunTextureId = other->getSunTextureId();
		mNextMoonTextureId = other->getMoonTextureId();
		mNextCloudTextureId = cloud_noise_id_next;
		mNextBloomTextureId = other->getBloomTextureId();
		mNextRainbowTextureId = other->getRainbowTextureId();
		mNextHaloTextureId = other->getHaloTextureId();
	}
	else
	{
		llwarns << "Could not cast end settings to sky. No blend performed."
				<< llendl;
	}

	setBlendFactor(blendf);
}

const LLSettingsSky::stringset_t& LLSettingsSky::getSkipInterpolateKeys() const
{
	static stringset_t skip_set;
	if (skip_set.empty())
	{
		skip_set = LLSettingsBase::getSkipInterpolateKeys();
		skip_set.insert(SETTING_RAYLEIGH_CONFIG);
		skip_set.insert(SETTING_MIE_CONFIG);
		skip_set.insert(SETTING_ABSORPTION_CONFIG);
		skip_set.insert(SETTING_CLOUD_SHADOW);
	}
	return skip_set;
}

const LLSettingsSky::stringset_t& LLSettingsSky::getSlerpKeys() const
{
	static stringset_t slerp_set;
	if (slerp_set.empty())
	{
		slerp_set.insert(SETTING_SUN_ROTATION);
		slerp_set.insert(SETTING_MOON_ROTATION);
	}
	return slerp_set;
}

LLSettingsSky::validation_list_t LLSettingsSky::legacyHazeValidationList()
{
	static validation_list_t legacy_haze;
	if (legacy_haze.empty())
	{
		legacy_haze.emplace_back(SETTING_AMBIENT, false, LLSD::TypeArray,
								 boost::bind(&Validator::verifyVectorMinMax,
											 _1, _2,
											 llsd::array(0.0, 0.0, 0.0, "*"),
											 llsd::array(3.0, 3.0, 3.0, "*")));
		legacy_haze.emplace_back(SETTING_BLUE_DENSITY, false, LLSD::TypeArray,
								 boost::bind(&Validator::verifyVectorMinMax,
											 _1, _2,
											 llsd::array(0.0, 0.0, 0.0, "*"),
											 llsd::array(3.0, 3.0, 3.0, "*")));
		legacy_haze.emplace_back(SETTING_BLUE_HORIZON, false, LLSD::TypeArray,
								 boost::bind(&Validator::verifyVectorMinMax,
											 _1, _2,
											 llsd::array(0.0, 0.0, 0.0, "*"),
											 llsd::array(3.0, 3.0, 3.0, "*")));
		legacy_haze.emplace_back(SETTING_HAZE_DENSITY, false, LLSD::TypeReal,
								 boost::bind(&Validator::verifyFloatRange,
											 _1, _2, llsd::array(0.0, 5.0)));
		legacy_haze.emplace_back(SETTING_HAZE_HORIZON, false, LLSD::TypeReal,
								 boost::bind(&Validator::verifyFloatRange,
											 _1, _2, llsd::array(0.0, 5.0)));
		legacy_haze.emplace_back(SETTING_DENSITY_MULTIPLIER, false,
								 LLSD::TypeReal,
								 boost::bind(&Validator::verifyFloatRange,
											 _1, _2,
											 llsd::array(0.0000001, 2.0)));
		legacy_haze.emplace_back(SETTING_DISTANCE_MULTIPLIER, false,
								 LLSD::TypeReal,
								 boost::bind(&Validator::verifyFloatRange,
											 _1, _2,
											 llsd::array(0.0001, 1000.0)));
	}
	return legacy_haze;
}

LLSettingsSky::validation_list_t LLSettingsSky::rayleighValidationList()
{
	static validation_list_t rayleigh;
	if (rayleigh.empty())
	{
		rayleigh.emplace_back(SETTING_DENSITY_PROFILE_WIDTH, false,
							 LLSD::TypeReal,
							 boost::bind(&Validator::verifyFloatRange, _1, _2,
										 llsd::array(0.0, 32768.0)));
		rayleigh.emplace_back(SETTING_DENSITY_PROFILE_EXP_TERM, false,
							  LLSD::TypeReal,
							  boost::bind(&Validator::verifyFloatRange, _1, _2,
										  llsd::array(0.0, 2.0)));
		rayleigh.emplace_back(SETTING_DENSITY_PROFILE_EXP_SCALE_FACTOR, false,
							  LLSD::TypeReal,
							  boost::bind(&Validator::verifyFloatRange, _1, _2,
										  llsd::array(-1.0, 1.0)));
		rayleigh.emplace_back(SETTING_DENSITY_PROFILE_LINEAR_TERM, false,
							  LLSD::TypeReal,
							  boost::bind(&Validator::verifyFloatRange, _1, _2,
										  llsd::array(0.0, 2.0)));
		rayleigh.emplace_back(SETTING_DENSITY_PROFILE_CONSTANT_TERM, false,
							  LLSD::TypeReal,
							  boost::bind(&Validator::verifyFloatRange, _1, _2,
										  llsd::array(0.0, 1.0)));
	}
	return rayleigh;
}

LLSettingsSky::validation_list_t LLSettingsSky::absorptionValidationList()
{
	static validation_list_t absorption;
	if (absorption.empty())
	{
		absorption.emplace_back(SETTING_DENSITY_PROFILE_WIDTH, false,
								LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2,
											llsd::array(0.0, 32768.0)));
		absorption.emplace_back(SETTING_DENSITY_PROFILE_EXP_TERM, false,
								LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 2.0)));
		absorption.emplace_back(SETTING_DENSITY_PROFILE_EXP_SCALE_FACTOR,
								false, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(-1.0, 1.0)));
		absorption.emplace_back(SETTING_DENSITY_PROFILE_LINEAR_TERM, false,
								LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 2.0)));
		absorption.emplace_back(SETTING_DENSITY_PROFILE_CONSTANT_TERM, false,
								LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
	}
	return absorption;
}

LLSettingsSky::validation_list_t LLSettingsSky::mieValidationList()
{
	static validation_list_t mie;
	if (mie.empty())
	{
		mie.emplace_back(SETTING_DENSITY_PROFILE_WIDTH, false, LLSD::TypeReal,
						 boost::bind(&Validator::verifyFloatRange, _1, _2,
									 llsd::array(0.0, 32768.0)));
		mie.emplace_back(SETTING_DENSITY_PROFILE_EXP_TERM, false,
						 LLSD::TypeReal,
						 boost::bind(&Validator::verifyFloatRange, _1, _2,
									 llsd::array(0.0, 2.0)));
		mie.emplace_back(SETTING_DENSITY_PROFILE_EXP_SCALE_FACTOR, false,
						 LLSD::TypeReal,
						 boost::bind(&Validator::verifyFloatRange, _1, _2,
									 llsd::array(-1.0, 1.0)));
		mie.emplace_back(SETTING_DENSITY_PROFILE_LINEAR_TERM, false,
						 LLSD::TypeReal,
						 boost::bind(&Validator::verifyFloatRange, _1, _2,
									 llsd::array(0.0, 2.0)));
		mie.emplace_back(SETTING_DENSITY_PROFILE_CONSTANT_TERM, false,
						 LLSD::TypeReal,
						 boost::bind(&Validator::verifyFloatRange, _1, _2,
									 llsd::array(0.0, 1.0)));
		mie.emplace_back(SETTING_MIE_ANISOTROPY_FACTOR, false, LLSD::TypeReal,
						 boost::bind(&Validator::verifyFloatRange, _1, _2,
									 llsd::array(0.0, 1.0)));
	}
	return mie;
}

bool LLSettingsSky::validateLegacyHaze(LLSD& value, U32 flags)
{
	llassert(value.type() == LLSD::TypeMap);

	validation_list_t validations = legacyHazeValidationList();
	LLSD result = settingValidation(value, validations, flags);
	if (result["errors"].size() > 0)
	{
		llwarns << "Legacy haze config validation errors: " << result["errors"]
				<< llendl;
		return false;
	}
	if (result["warnings"].size() > 0)
	{
		llwarns << "Legacy haze config validation warnings: "
				<< result["warnings"] << llendl;
		return false;
	}
	return true;
}

bool LLSettingsSky::validateRayleighLayers(LLSD& value, U32 flags)
{
	validation_list_t validations = rayleighValidationList();
	if (value.isArray())
	{
		bool all_good = true;
		for (LLSD::array_iterator it = value.beginArray(),
								  end = value.endArray();
			 it != end; ++it)
		{
			LLSD& layerConfig = *it;
			if (layerConfig.type() == LLSD::TypeMap)
			{
				if (!validateRayleighLayers(layerConfig, flags))
				{
					all_good = false;
				}
			}
			else if (layerConfig.type() == LLSD::TypeArray)
			{
				return validateRayleighLayers(layerConfig, flags);
			}
			else
			{
				return settingValidation(value, validations, flags);
			}
		}
		return all_good;
	}

	llassert(value.type() == LLSD::TypeMap);

	LLSD result = settingValidation(value, validations, flags);
	if (result["errors"].size() > 0)
	{
		llwarns << "Rayleigh config validation errors: " << result["errors"]
				<< llendl;
		return false;
	}
	if (result["warnings"].size() > 0)
	{
		llwarns << "Rayleigh config validation warnings: " << result["errors"]
				<< llendl;
		return false;
	}
	return true;
}

bool LLSettingsSky::validateAbsorptionLayers(LLSD& value, U32 flags)
{
	validation_list_t validations = absorptionValidationList();
	if (value.isArray())
	{
		bool all_good = true;
		for (LLSD::array_iterator it = value.beginArray(),
								  end = value.endArray();
			 it != end; ++it)
		{
			LLSD& layerConfig = *it;
			if (layerConfig.type() == LLSD::TypeMap)
			{
				if (!validateAbsorptionLayers(layerConfig, flags))
				{
					all_good = false;
				}
			}
			else if (layerConfig.type() == LLSD::TypeArray)
			{
				return validateAbsorptionLayers(layerConfig, flags);
			}
			else
			{
				return settingValidation(value, validations, flags);
			}
		}
		return all_good;
	}

	llassert(value.type() == LLSD::TypeMap);

	LLSD result = settingValidation(value, validations, flags);
	if (result["errors"].size() > 0)
	{
		llwarns << "Absorption config validation errors: " << result["errors"]
				<< llendl;
		return false;
	}
	if (result["warnings"].size() > 0)
	{
		llwarns << "Absorption config validation warnings: "
				<< result["errors"] << llendl;
		return false;
	}
	return true;
}

bool LLSettingsSky::validateMieLayers(LLSD& value, U32 flags)
{
	validation_list_t validations = mieValidationList();
	if (value.isArray())
	{
		bool all_good = true;
		for (LLSD::array_iterator it = value.beginArray(),
								  end = value.endArray();
			 it != end; ++it)
		{
			LLSD& layerConfig = *it;
			if (layerConfig.type() == LLSD::TypeMap)
			{
				if (!validateMieLayers(layerConfig, flags))
				{
					all_good = false;
				}
			}
			else if (layerConfig.type() == LLSD::TypeArray)
			{
				return validateMieLayers(layerConfig, flags);
			}
			else
			{
				return settingValidation(value, validations, flags);
			}
		}
		return all_good;
	}

	LLSD result = settingValidation(value, validations, flags);
	if (result["errors"].size() > 0)
	{
		llwarns << "Mie config validation errors: " << result["errors"]
				<< llendl;
		return false;
	}
	if (result["warnings"].size() > 0)
	{
		llwarns << "Mie config validation warnings: " << result["warnings"]
				<< llendl;
		return false;
	}
	return true;
}

//virtual
const LLSettingsSky::validation_list_t& LLSettingsSky::getValidationList() const
{
	return validationList();
}

//static
const LLSettingsSky::validation_list_t& LLSettingsSky::validationList()
{
	static validation_list_t validation;

	if (validation.empty())
	{
		validation.emplace_back(SETTING_BLOOM_TEXTUREID, true, LLSD::TypeUUID);
		validation.emplace_back(SETTING_RAINBOW_TEXTUREID, false,
								LLSD::TypeUUID);
		validation.emplace_back(SETTING_HALO_TEXTUREID, false, LLSD::TypeUUID);

		validation.emplace_back(SETTING_CLOUD_COLOR, true, LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2,
											llsd::array(0.0, 0.0, 0.0, "*"),
											llsd::array(1.0, 1.0, 1.0, "*")));
		validation.emplace_back(SETTING_CLOUD_POS_DENSITY1, true,
								LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2,
											llsd::array(0.0, 0.0, 0.0, "*"),
											llsd::array(1.0, 1.0, 3.0, "*")));
		validation.emplace_back(SETTING_CLOUD_POS_DENSITY2, true,
								LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2,
											llsd::array(0.0, 0.0, 0.0, "*"),
											llsd::array(1.0, 1.0, 1.0, "*")));
		validation.emplace_back(SETTING_CLOUD_SCALE, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.001f, 3.0)));
		validation.emplace_back(SETTING_CLOUD_SCROLL_RATE, true,
								LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2, llsd::array(-50.0, -50.0),
											llsd::array(50.0, 50.0)));
		validation.emplace_back(SETTING_CLOUD_SHADOW, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_CLOUD_TEXTUREID, false,
								LLSD::TypeUUID);
		validation.emplace_back(SETTING_CLOUD_VARIANCE, false, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_DOME_OFFSET, false, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_DOME_RADIUS, false, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2,
											llsd::array(1000.0, 2000.0)));
		validation.emplace_back(SETTING_GAMMA, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 20.0)));
		validation.emplace_back(SETTING_GLOW, true, LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2,
											llsd::array(0.2, "*", -10.0, "*"),
											llsd::array(40.0, "*", 10.0, "*")));
		validation.emplace_back(SETTING_MAX_Y, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2,
											llsd::array(0.0, 10000.0)));
		validation.emplace_back(SETTING_MOON_ROTATION, true, LLSD::TypeArray,
								&Validator::verifyQuaternionNormal);
		validation.emplace_back(SETTING_MOON_SCALE, false, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.25, 20.0)),
											LLSD::Real(1.0));
		validation.emplace_back(SETTING_MOON_TEXTUREID, false, LLSD::TypeUUID);
		validation.emplace_back(SETTING_MOON_BRIGHTNESS, false, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_STAR_BRIGHTNESS, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 500.0)));
		validation.emplace_back(SETTING_SUNLIGHT_COLOR, true, LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2,
											llsd::array(0.0, 0.0, 0.0, "*"),
											llsd::array(3.0, 3.0, 3.0, "*")));
		validation.emplace_back(SETTING_SUN_ROTATION, true, LLSD::TypeArray,
								&Validator::verifyQuaternionNormal);
		validation.emplace_back(SETTING_SUN_SCALE, false, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.25, 20.0)),
											LLSD::Real(1.0));
		validation.emplace_back(SETTING_SUN_TEXTUREID, false, LLSD::TypeUUID);
		validation.emplace_back(SETTING_PLANET_RADIUS, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2,
											llsd::array(1000.0, 32768.0)));
		validation.emplace_back(SETTING_SKY_BOTTOM_RADIUS, true,
								LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2,
											llsd::array(1000.0, 32768.0)));
		validation.emplace_back(SETTING_SKY_TOP_RADIUS, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2,
											llsd::array(1000.0, 32768.0)));
		validation.emplace_back(SETTING_SUN_ARC_RADIANS, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 0.1)));
		validation.emplace_back(SETTING_SKY_MOISTURE_LEVEL, false,
								LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_SKY_DROPLET_RADIUS, false,
								LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(5.0, 1000.0)));
		validation.emplace_back(SETTING_SKY_ICE_LEVEL, false, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_REFLECTION_PROBE_AMBIANCE, false,
								LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_RAYLEIGH_CONFIG, true, LLSD::TypeArray,
								&validateRayleighLayers);
		validation.emplace_back(SETTING_ABSORPTION_CONFIG, true,
								LLSD::TypeArray, &validateAbsorptionLayers);
		validation.emplace_back(SETTING_MIE_CONFIG, true, LLSD::TypeArray,
								&validateMieLayers);
		validation.emplace_back(SETTING_LEGACY_HAZE, false, LLSD::TypeMap,
								&validateLegacyHaze);
	}
	return validation;
}

LLSD LLSettingsSky::createDensityProfileLayer(F32 width, F32 exponential_term,
											  F32 exponential_scale_factor,
											  F32 linear_term,
											  F32 constant_term,
											  F32 aniso_factor)
{
	LLSD dflt_layer;

	 // width = 0 -> the entire atmosphere
	dflt_layer[SETTING_DENSITY_PROFILE_WIDTH] = width;
	dflt_layer[SETTING_DENSITY_PROFILE_EXP_TERM] = exponential_term;
	dflt_layer[SETTING_DENSITY_PROFILE_EXP_SCALE_FACTOR] =
		exponential_scale_factor;
	dflt_layer[SETTING_DENSITY_PROFILE_LINEAR_TERM] = linear_term;
	dflt_layer[SETTING_DENSITY_PROFILE_CONSTANT_TERM] = constant_term;

	if (aniso_factor != 0.f)
	{
		dflt_layer[SETTING_MIE_ANISOTROPY_FACTOR] = aniso_factor;
	}

	return dflt_layer;
}

LLSD LLSettingsSky::createSingleLayerDensityProfile(F32 width,
													F32 exponential_term,
													F32 exponential_scale_factor,
													F32 linear_term,
													F32 constant_term,
													F32 aniso_factor)
{
	LLSD dflt;
	LLSD dflt_layer = createDensityProfileLayer(width, exponential_term,
												exponential_scale_factor,
												linear_term, constant_term,
												aniso_factor);
	dflt.append(dflt_layer);
	return dflt;
}

LLSD LLSettingsSky::rayleighConfigDefault()
{
	return createSingleLayerDensityProfile(0.f,  1.f, -1.f / 8000.f, 0.f, 0.f);
}

LLSD LLSettingsSky::absorptionConfigDefault()
{
	// Absorption (ozone) has two linear ramping zones
	LLSD dflt_absorption_layer_a = createDensityProfileLayer(25000.f, 0.f, 0.f,
															 -1.f / 25000.f,
															 -2.f / 3.f);
	LLSD dflt_absorption_layer_b = createDensityProfileLayer(0.f, 0.f, 0.f,
															 -1.f / 15000.f,
															 8.f / 3.f);
	LLSD dflt_absorption;
	dflt_absorption.append(dflt_absorption_layer_a);
	dflt_absorption.append(dflt_absorption_layer_b);
	return dflt_absorption;
}

LLSD LLSettingsSky::mieConfigDefault()
{
	LLSD dflt_mie = createSingleLayerDensityProfile(0.f,  1.f, -1.f / 1200.f,
													0.f, 0.f, 0.8f);
	return dflt_mie;
}

LLSD LLSettingsSky::defaults(F32 position)
{
	static LLSD dfltsetting;

	if (dfltsetting.size() == 0)
	{
		// Give the Sun and Moon slightly different tracks through the sky
		// instead of positioning them at opposite poles from each other...
		F32 azimuth = F_PI * position + 80.f * DEG_TO_RAD;
		F32 altitude = F_PI * position;
		LLQuaternion sunquat =
			convert_azimuth_and_altitude_to_quat(altitude, azimuth);
		LLQuaternion moonquat =
			convert_azimuth_and_altitude_to_quat(altitude + F_PI * 0.125f,
												 azimuth + F_PI * 0.125f);

		// Magic constants copied from Default.xml
		dfltsetting[SETTING_CLOUD_COLOR] =
			LLColor4(0.4099f, 0.4099f, 0.4099f, 0.f).getValue();
		dfltsetting[SETTING_CLOUD_POS_DENSITY1] =
			LLColor4(1.f, 0.526f, 1.f, 0.f).getValue();
		dfltsetting[SETTING_CLOUD_POS_DENSITY2] =
			LLColor4(1.f, 0.526f, 0.12f, 0.f).getValue();
		dfltsetting[SETTING_CLOUD_SCALE] = LLSD::Real(0.4199);
		dfltsetting[SETTING_CLOUD_SCROLL_RATE] = llsd::array(0.2, 0.01);
		dfltsetting[SETTING_CLOUD_SHADOW] = LLSD::Real(0.2699);
		dfltsetting[SETTING_CLOUD_VARIANCE] = LLSD::Real(0.0);

		dfltsetting[SETTING_DOME_OFFSET] = LLSD::Real(0.96);
		dfltsetting[SETTING_DOME_RADIUS] = LLSD::Real(15000.0);
		dfltsetting[SETTING_GAMMA]  = LLSD::Real(1.0);
		dfltsetting[SETTING_GLOW] =
			LLColor4(5.f, 0.001f, -0.4799f, 1.f).getValue();

		dfltsetting[SETTING_MAX_Y] = LLSD::Real(1605);
		dfltsetting[SETTING_MOON_ROTATION] = moonquat.getValue();
		dfltsetting[SETTING_MOON_BRIGHTNESS] = LLSD::Real(0.5);

		dfltsetting[SETTING_STAR_BRIGHTNESS] = LLSD::Real(250.0000);
		dfltsetting[SETTING_SUNLIGHT_COLOR] =
			LLColor4(0.7342f, 0.7815f, 0.8999f, 0.f).getValue();
		dfltsetting[SETTING_SUN_ROTATION] = sunquat.getValue();

		dfltsetting[SETTING_BLOOM_TEXTUREID] = getDefaultBloomTextureId();
		dfltsetting[SETTING_CLOUD_TEXTUREID] = getDefaultCloudNoiseTextureId();
		dfltsetting[SETTING_MOON_TEXTUREID] = getDefaultMoonTextureId();
		dfltsetting[SETTING_SUN_TEXTUREID] = getDefaultSunTextureId();
		dfltsetting[SETTING_RAINBOW_TEXTUREID] = getDefaultRainbowTextureId();
		dfltsetting[SETTING_HALO_TEXTUREID] = getDefaultHaloTextureId();

		dfltsetting[SETTING_TYPE] = "sky";

		// Defaults are for earth...
		dfltsetting[SETTING_PLANET_RADIUS] = 6360.0;
		dfltsetting[SETTING_SKY_BOTTOM_RADIUS] = 6360.0;
		dfltsetting[SETTING_SKY_TOP_RADIUS] = 6420.0;
		dfltsetting[SETTING_SUN_ARC_RADIANS] = 0.00045;

		dfltsetting[SETTING_SKY_MOISTURE_LEVEL] = 0.0;
		dfltsetting[SETTING_SKY_DROPLET_RADIUS] = 800.0;
		dfltsetting[SETTING_SKY_ICE_LEVEL] = 0.0;

#if 0	// By default (legacy sky), this setting does not exist !  HB
		dfltsetting[SETTING_REFLECTION_PROBE_AMBIANCE] = 0.0;
#endif

		dfltsetting[SETTING_RAYLEIGH_CONFIG] = rayleighConfigDefault();
		dfltsetting[SETTING_MIE_CONFIG]  = mieConfigDefault();
		dfltsetting[SETTING_ABSORPTION_CONFIG] = absorptionConfigDefault();
	}

	return dfltsetting;
}

LLSD LLSettingsSky::translateLegacyHazeSettings(const LLSD& legacy)
{
	LLSD legacyhazesettings;

	// *TODO: AdvancedAtmospherics. These need to be translated into density
	// profile info in the new settings format...
	// LEGACY_ATMOSPHERICS
	if (legacy.has(SETTING_AMBIENT))
	{
		legacyhazesettings[SETTING_AMBIENT] =
			LLColor3(legacy[SETTING_AMBIENT]).getValue();
	}
	if (legacy.has(SETTING_BLUE_DENSITY))
	{
		legacyhazesettings[SETTING_BLUE_DENSITY] =
			LLColor3(legacy[SETTING_BLUE_DENSITY]).getValue();
	}
	if (legacy.has(SETTING_BLUE_HORIZON))
	{
		legacyhazesettings[SETTING_BLUE_HORIZON] =
			LLColor3(legacy[SETTING_BLUE_HORIZON]).getValue();
	}
	if (legacy.has(SETTING_DENSITY_MULTIPLIER))
	{
		legacyhazesettings[SETTING_DENSITY_MULTIPLIER] =
			LLSD::Real(legacy[SETTING_DENSITY_MULTIPLIER][0].asReal());
	}
	if (legacy.has(SETTING_DISTANCE_MULTIPLIER))
	{
		legacyhazesettings[SETTING_DISTANCE_MULTIPLIER] =
			LLSD::Real(legacy[SETTING_DISTANCE_MULTIPLIER][0].asReal());
	}
	if (legacy.has(SETTING_HAZE_DENSITY))
	{
		legacyhazesettings[SETTING_HAZE_DENSITY] =
			LLSD::Real(legacy[SETTING_HAZE_DENSITY][0].asReal());
	}
	if (legacy.has(SETTING_HAZE_HORIZON))
	{
		legacyhazesettings[SETTING_HAZE_HORIZON] =
			LLSD::Real(legacy[SETTING_HAZE_HORIZON][0].asReal());
	}

	return legacyhazesettings;
}

LLSD LLSettingsSky::translateLegacySettings(const LLSD& legacy)
{
	bool converted_something = false;
	LLSD newsettings(defaults());

	// Move legacy haze parameters to an inner map allowing backward
	// compatibility and simple conversion to legacy format
	LLSD legacyhazesettings = translateLegacyHazeSettings(legacy);
	if (legacyhazesettings.size() > 0)
	{
		newsettings[SETTING_LEGACY_HAZE] = legacyhazesettings;
		converted_something = true;
	}

	if (legacy.has(SETTING_CLOUD_COLOR))
	{
		newsettings[SETTING_CLOUD_COLOR] =
			LLColor3(legacy[SETTING_CLOUD_COLOR]).getValue();
		converted_something = true;
	}
	if (legacy.has(SETTING_CLOUD_POS_DENSITY1))
	{
		newsettings[SETTING_CLOUD_POS_DENSITY1] =
			LLColor3(legacy[SETTING_CLOUD_POS_DENSITY1]).getValue();
		converted_something = true;
	}
	if (legacy.has(SETTING_CLOUD_POS_DENSITY2))
	{
		newsettings[SETTING_CLOUD_POS_DENSITY2] =
			LLColor3(legacy[SETTING_CLOUD_POS_DENSITY2]).getValue();
		converted_something = true;
	}
	if (legacy.has(SETTING_CLOUD_SCALE))
	{
		newsettings[SETTING_CLOUD_SCALE] =
			LLSD::Real(legacy[SETTING_CLOUD_SCALE][0].asReal());
		converted_something = true;
	}
	if (legacy.has(SETTING_CLOUD_SCROLL_RATE))
	{
		LLVector2 cloud_scroll(legacy[SETTING_CLOUD_SCROLL_RATE]);

		cloud_scroll -= LLVector2(10, 10);
		if (legacy.has(SETTING_LEGACY_ENABLE_CLOUD_SCROLL))
		{
			LLSD enabled = legacy[SETTING_LEGACY_ENABLE_CLOUD_SCROLL];
			if (!enabled[0].asBoolean())
			{
				cloud_scroll.mV[0] = 0.f;
			}
			if (!enabled[1].asBoolean())
			{
				cloud_scroll.mV[1] = 0.f;
			}
		}

		newsettings[SETTING_CLOUD_SCROLL_RATE] = cloud_scroll.getValue();
		converted_something = true;
	}
	if (legacy.has(SETTING_CLOUD_SHADOW))
	{
		newsettings[SETTING_CLOUD_SHADOW] =
			LLSD::Real(legacy[SETTING_CLOUD_SHADOW][0].asReal());
		converted_something = true;
	}

	if (legacy.has(SETTING_GAMMA))
	{
		newsettings[SETTING_GAMMA] = legacy[SETTING_GAMMA][0].asReal();
		converted_something = true;
	}
	if (legacy.has(SETTING_GLOW))
	{
		newsettings[SETTING_GLOW] = LLColor3(legacy[SETTING_GLOW]).getValue();
		converted_something = true;
	}

	if (legacy.has(SETTING_MAX_Y))
	{
		newsettings[SETTING_MAX_Y] =
			LLSD::Real(legacy[SETTING_MAX_Y][0].asReal());
		converted_something = true;
	}
	if (legacy.has(SETTING_STAR_BRIGHTNESS))
	{
		newsettings[SETTING_STAR_BRIGHTNESS] =
			LLSD::Real(legacy[SETTING_STAR_BRIGHTNESS].asReal() * 250.0);
		converted_something = true;
	}
	if (legacy.has(SETTING_SUNLIGHT_COLOR))
	{
		newsettings[SETTING_SUNLIGHT_COLOR] =
			LLColor4(legacy[SETTING_SUNLIGHT_COLOR]).getValue();
		converted_something = true;
	}

	if (legacy.has(SETTING_PLANET_RADIUS))
	{
		newsettings[SETTING_PLANET_RADIUS] =
			LLSD::Real(legacy[SETTING_PLANET_RADIUS].asReal());
		converted_something = true;
	}

	if (legacy.has(SETTING_SKY_BOTTOM_RADIUS))
	{
		newsettings[SETTING_SKY_BOTTOM_RADIUS] =
			LLSD::Real(legacy[SETTING_SKY_BOTTOM_RADIUS].asReal());
		converted_something = true;
	}

	if (legacy.has(SETTING_SKY_TOP_RADIUS))
	{
		newsettings[SETTING_SKY_TOP_RADIUS] =
			LLSD::Real(legacy[SETTING_SKY_TOP_RADIUS].asReal());
		converted_something = true;
	}

	if (legacy.has(SETTING_SUN_ARC_RADIANS))
	{
		newsettings[SETTING_SUN_ARC_RADIANS] =
			LLSD::Real(legacy[SETTING_SUN_ARC_RADIANS].asReal());
		converted_something = true;
	}

	if (legacy.has(SETTING_LEGACY_EAST_ANGLE) &&
		legacy.has(SETTING_LEGACY_SUN_ANGLE))
	{
		// Get counter-clockwise radian angle from clockwise legacy WL East
		// angle...
		F32 azimuth = -legacy[SETTING_LEGACY_EAST_ANGLE].asReal();
		F32 altitude = legacy[SETTING_LEGACY_SUN_ANGLE].asReal();

		LLQuaternion sunquat = convert_azimuth_and_altitude_to_quat(azimuth,
																	altitude);
		// Original WL Moon dir was diametrically opposed to the Sun dir
		LLQuaternion moonquat =
			convert_azimuth_and_altitude_to_quat(azimuth + F_PI, -altitude);

		newsettings[SETTING_SUN_ROTATION] = sunquat.getValue();
		newsettings[SETTING_MOON_ROTATION] = moonquat.getValue();
		converted_something = true;
	}

	return converted_something ? newsettings : LLSD();
}

void LLSettingsSky::updateSettings()
{
	// Base class clears dirty flag so as to not trigger recursive update.
	// NOTE: this *must* be invoked first in this method ! HB
	LLSettingsBase::updateSettings();

	calculateHeavenlyBodyPositions();
	calculateLightSettings();

	mCloudPosDensity1.setValue(mSettings[SETTING_CLOUD_POS_DENSITY1]);
	mSunlightColor.setValue(mSettings[SETTING_SUNLIGHT_COLOR]);
	mSunScale = mSettings[SETTING_SUN_SCALE].asReal();
	mMoonBrightness = mSettings[SETTING_MOON_BRIGHTNESS].asReal();
	mMoonScale = mSettings[SETTING_MOON_SCALE].asReal();
	mCloudColor.setValue(mSettings[SETTING_CLOUD_COLOR]);
	mCloudScrollRate.setValue(mSettings[SETTING_CLOUD_SCROLL_RATE]);
	mGamma = mSettings[SETTING_GAMMA].asReal();
	mGlow.setValue(mSettings[SETTING_GLOW]);
	mMaxY = mSettings[SETTING_MAX_Y].asReal();
	mDensityMultiplier = getFloat(SETTING_DENSITY_MULTIPLIER, 0.0001f);
	mDistanceMultiplier = getFloat(SETTING_DISTANCE_MULTIPLIER, 0.8f);
	mStarBrightness = mSettings[SETTING_STAR_BRIGHTNESS].asReal();
	mSkyMoistureLevel = mSettings[SETTING_SKY_MOISTURE_LEVEL].asReal();
	mSkyDropletRadius = mSettings[SETTING_SKY_DROPLET_RADIUS].asReal();
	mSkyIceLevel = mSettings[SETTING_SKY_ICE_LEVEL].asReal();
	mCloudShadow = mSettings[SETTING_CLOUD_SHADOW].asReal();
	mCloudVariance = mSettings[SETTING_CLOUD_VARIANCE].asReal();
	if (mSettings.has(SETTING_REFLECTION_PROBE_AMBIANCE))
	{
		mReflectionProbeAmbiance =
			mSettings[SETTING_REFLECTION_PROBE_AMBIANCE].asReal();
		mCanAutoAdjust = false;
	}
	else
	{
		mReflectionProbeAmbiance = 0.f;
		mCanAutoAdjust = true;
	}
}

F32 LLSettingsSky::getSunMoonGlowFactor() const
{
	return getIsSunUp() ? 1.f
						: (getIsMoonUp() ? getMoonBrightness() * 0.25f : 0.f);
}

bool LLSettingsSky::getIsSunUp() const
{
	LLVector3 sun_dir = getSunDirection();
	return sun_dir.mV[2] >= 0.f;
}

bool LLSettingsSky::getIsMoonUp() const
{
	LLVector3 moon_dir = getMoonDirection();
	return moon_dir.mV[2] >= 0.f;
}

void LLSettingsSky::calculateHeavenlyBodyPositions()  const
{
	LLQuaternion sunq = getSunRotation();
	LLQuaternion moonq = getMoonRotation();

	mSunDirection = LLVector3::x_axis * sunq;
	mMoonDirection = LLVector3::x_axis * moonq;

	mSunDirection.normalize();
	mMoonDirection.normalize();

	if (mSunDirection.lengthSquared() < 0.01f)
	{
		llwarns << "Zero length Sun direction." << llendl;
	}
	if (mMoonDirection.lengthSquared() < 0.01f)
	{
		llwarns << "Zero length moon direction." << llendl;
	}
}

LLVector3 LLSettingsSky::getLightDirection() const
{
	update();

	// Is the normal from the Sun or the Moon ?
	if (getIsSunUp())
	{
		return mSunDirection;
	}
	if (getIsMoonUp())
	{
		return mMoonDirection;
	}
	return LLVector3::z_axis_neg;
}

LLColor3 LLSettingsSky::getLightDiffuse() const
{
	update();

	// Is the normal from the Sun or the Moon ?
	if (getIsSunUp())
	{
		return getSunDiffuse();
	}
	if (getIsMoonUp())
	{
		return getMoonDiffuse();
	}
	return LLColor3::white;
}

LLColor3 LLSettingsSky::getColor(const std::string& key,
								 const LLColor3& default_value) const
{
	LLSD::map_const_iterator it = mSettings.find(SETTING_LEGACY_HAZE);
	LLSD::map_const_iterator end = mSettings.endMap();
	if (it != end)
	{
		LLSD::map_const_iterator it2 = it->second.find(key);
		if (it2 != it->second.endMap())
		{
			return LLColor3(it2->second);
		}
	}
	it = mSettings.find(key);
	if (it != end)
	{
		return LLColor3(it->second);
	}
	return default_value;
}

F32 LLSettingsSky::getFloat(const std::string& key, F32 default_value) const
{
	LLSD::map_const_iterator it = mSettings.find(SETTING_LEGACY_HAZE);
	LLSD::map_const_iterator end = mSettings.endMap();
	if (it != end)
	{
		LLSD::map_const_iterator it2 = it->second.find(key);
		if (it2 != it->second.endMap())
		{
			return it2->second.asReal();
		}
	}
	it = mSettings.find(key);
	if (it != end)
	{
		return it->second.asReal();
	}
	return default_value;
}

void LLSettingsSky::removeProbeAmbiance()
{
	if (mSettings.has(SETTING_REFLECTION_PROBE_AMBIANCE))
	{
		mSettings.erase(SETTING_REFLECTION_PROBE_AMBIANCE);
		setDirtyFlag(true);
		update();
	}
}

LLColor3 LLSettingsSky::getAmbientColorClamped() const
{
	LLColor3 ambient = getAmbientColor();
	F32 max_color = llmax(ambient.mV[0], ambient.mV[1], ambient.mV[2]);
	if (max_color > 1.f)
	{
		ambient *= 1.f / max_color;
	}
	return ambient;
}

// Get total from rayleigh and mie density values for normalization
LLColor3 LLSettingsSky::getTotalDensity() const
{
	return getBlueDensity() + smear(getHazeDensity());
}

// Sunlight attenuation effect (hue and brightness) due to atmosphere
// this is used later for sunlight modulation at various altitudes
LLColor3 LLSettingsSky::getLightAttenuation(F32 distance) const
{
	// Approximate line integral over requested distance
	return (getBlueDensity() + smear(getHazeDensity() * 0.25f)) *
		   getDensityMultiplier() * distance;
}

LLColor3 LLSettingsSky::getLightTransmittance(F32 distance) const
{
	// Transparency (-> density) from Beer's law
	return componentExp(getTotalDensity() *
						(-distance * getDensityMultiplier()));
}

// Performs soft scale clip and gamma correction following the shader
// implementation scales colors down to 0 - 1 range preserving relative ratios
//static
LLColor3 LLSettingsSky::gammaCorrect(const LLColor3& in, F32 gamma)
{
#if 0	// 'v' is not used, so why bothering ???
	LLColor3 v(in);
	// Scale down to 0 to 1 range preserving relative ratio (AKA homegenize)
	F32 max_color = llmax(llmax(in.mV[0], in.mV[1]), in.mV[2]);
	if (max_color > 1.f)
	{
		v *= 1.f / max_color;
	}
#endif
	LLColor3 color = in * 2.f;
	// Clamping after mul seems wrong, but prevents negative colors...
	color = smear(1.f) - componentSaturate(color);
	componentPow(color, gamma);
	return smear(1.f) - color;
}

void LLSettingsSky::calculateLightSettings() const
{
	// Sunlight attenuation effect (hue and brightness) due to atmosphere
	// this is used later for sunlight modulation at various altitudes
	F32 max_y = getMaxY();
	LLColor3 light_atten = getLightAttenuation(max_y);
	LLColor3 light_transmittance = getLightTransmittance(max_y);

	// And vary_sunlight will work properly with moon light
	LLVector3 lightnorm = getLightDirection();
	F32 lighty = fabs(lightnorm[2]);
	constexpr F32 LIMIT = FLT_EPSILON * 8.f;
	if (lighty >= LIMIT)
	{
		lighty = 1.f / lighty;
	}
	lighty = llmax(LIMIT, lighty);
	LLColor3 sunlight = getSunlightColor();
	componentMultBy(sunlight, componentExp(light_atten * -lighty));
	componentMultBy(sunlight, light_transmittance);

	// Increase ambient when there are more clouds
	F32 cloud_shadow = getCloudShadow();
	LLColor3 ambient = getAmbientColor();
	LLColor3 tmp_ambient = ambient +
						   (smear(1.f) - ambient) * cloud_shadow * 0.5f;

	// Brightness of surface both sunlight and ambient
	mSunDiffuse = sunlight;
	mSunAmbient = tmp_ambient;

	sunlight *= 1.f - cloud_shadow;
	sunlight += tmp_ambient;

	F32 haze_horizon = getHazeHorizon();
	mHazeColor = getBlueHorizon() * getBlueDensity() * sunlight;
	mHazeColor += LLColor4(haze_horizon, haze_horizon, haze_horizon,
						   haze_horizon) * getHazeDensity() * sunlight;

	F32 moon_brightness = getIsMoonUp() ? getMoonBrightness() : 0.001f;

	LLColor3 moonlight = getMoonlightColor();
	// Scotopic ambient value
	static const LLColor3 moonlight_b(0.66f, 0.66f, 1.2f);
	componentMultBy(moonlight, componentExp((light_atten * -1.f) * lighty));

	mMoonDiffuse = componentMult(moonlight, light_transmittance) *
				   moon_brightness;
	mMoonAmbient = moonlight_b * 0.0125f;

	mTotalAmbient = ambient;
}

LLColor3 LLSettingsSky::getSunlightColorClamped() const
{
	LLColor3 sunlight = getSunlightColor();
	F32 max_color = llmax(sunlight.mV[0], sunlight.mV[1], sunlight.mV[2]);
	if (max_color > 1.f)
	{
		sunlight *= 1.f / max_color;
	}
	return sunlight;
}

#if LL_VARIABLE_SKY_DOME_SIZE
F32 LLSettingsSky::getDomeOffset() const
{
	return mSettings[SETTING_DOME_OFFSET].asReal();
}

F32 LLSettingsSky::getDomeRadius() const
{
	return mSettings[SETTING_DOME_RADIUS].asReal();
}
#endif
