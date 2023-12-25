/**
 * @file llsettingswater.cpp
 * @brief The water settings asset support class.
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

#include "llsettingswater.h"

#include "imageids.h"

const std::string LLSettingsWater::SETTING_BLUR_MULTIPLIER = "blur_multiplier";
const std::string LLSettingsWater::SETTING_FOG_COLOR = "water_fog_color";
const std::string LLSettingsWater::SETTING_FOG_DENSITY = "water_fog_density";
const std::string LLSettingsWater::SETTING_FOG_MOD = "underwater_fog_mod";
const std::string LLSettingsWater::SETTING_FRESNEL_OFFSET = "fresnel_offset";
const std::string LLSettingsWater::SETTING_FRESNEL_SCALE = "fresnel_scale";
const std::string LLSettingsWater::SETTING_TRANSPARENT_TEXTURE = "transparent_texture";
const std::string LLSettingsWater::SETTING_NORMAL_MAP = "normal_map";
const std::string LLSettingsWater::SETTING_NORMAL_SCALE = "normal_scale";
const std::string LLSettingsWater::SETTING_SCALE_ABOVE = "scale_above";
const std::string LLSettingsWater::SETTING_SCALE_BELOW = "scale_below";
const std::string LLSettingsWater::SETTING_WAVE1_DIR = "wave1_direction";
const std::string LLSettingsWater::SETTING_WAVE2_DIR = "wave2_direction";

const std::string LLSettingsWater::SETTING_LEGACY_BLUR_MULTIPLIER = "blurMultiplier";
const std::string LLSettingsWater::SETTING_LEGACY_FOG_COLOR	= "waterFogColor";
const std::string LLSettingsWater::SETTING_LEGACY_FOG_DENSITY = "waterFogDensity";
const std::string LLSettingsWater::SETTING_LEGACY_FOG_MOD = "underWaterFogMod";
const std::string LLSettingsWater::SETTING_LEGACY_FRESNEL_OFFSET = "fresnelOffset";
const std::string LLSettingsWater::SETTING_LEGACY_FRESNEL_SCALE = "fresnelScale";
const std::string LLSettingsWater::SETTING_LEGACY_NORMAL_MAP = "normalMap";
const std::string LLSettingsWater::SETTING_LEGACY_NORMAL_SCALE = "normScale";
const std::string LLSettingsWater::SETTING_LEGACY_SCALE_ABOVE = "scaleAbove";
const std::string LLSettingsWater::SETTING_LEGACY_SCALE_BELOW = "scaleBelow";
const std::string LLSettingsWater::SETTING_LEGACY_WAVE1_DIR = "wave1Dir";
const std::string LLSettingsWater::SETTING_LEGACY_WAVE2_DIR = "wave2Dir";

const LLUUID LLSettingsWater::DEFAULT_ASSET_ID("59d1a851-47e7-0e5f-1ed7-6b715154f41a");

LLSettingsWater::LLSettingsWater(const LLSD& data)
:	LLSettingsBase(data),
	mBlurMultiplier(1.f),
	mFresnelOffset(0.f),
	mFresnelScale(1.f),
	mScaleAbove(1.f),
	mScaleBelow(1.f),
	mWaterFogDensity(1.f)
{
}

LLSettingsWater::LLSettingsWater()
:	LLSettingsBase(),
	mBlurMultiplier(1.f),
	mFresnelOffset(0.f),
	mFresnelScale(1.f),
	mScaleAbove(1.f),
	mScaleBelow(1.f),
	mWaterFogDensity(1.f)
{
}

LLSD LLSettingsWater::defaults(F32 position)
{
	static LLSD defaults;
	if (defaults.size() == 0)
	{
		// Give the normal scale offset some variability over track time...
		F32 norm_scale_offset = position * 0.5f - 0.25f;
		// Magic constants copied form defaults.xml
		defaults[SETTING_BLUR_MULTIPLIER] = LLSD::Real(0.04);
		defaults[SETTING_FOG_COLOR] = LLColor3(0.0156f, 0.149f,
											   0.2509f).getValue();
		defaults[SETTING_FOG_DENSITY] = LLSD::Real(2.0);
		defaults[SETTING_FOG_MOD] = LLSD::Real(0.25);
		defaults[SETTING_FRESNEL_OFFSET] = LLSD::Real(0.5);
		defaults[SETTING_FRESNEL_SCALE] = LLSD::Real(0.3999);
		defaults[SETTING_TRANSPARENT_TEXTURE] =
			getDefaultTransparentTextureAssetId();
		defaults[SETTING_NORMAL_MAP] = getDefaultWaterNormalAssetId();
		defaults[SETTING_NORMAL_SCALE] =
			LLVector3(2.f + norm_scale_offset, 2.f + norm_scale_offset,
					  2.f + norm_scale_offset).getValue();
		defaults[SETTING_SCALE_ABOVE] = LLSD::Real(0.0299);
		defaults[SETTING_SCALE_BELOW] = LLSD::Real(0.2);
		defaults[SETTING_WAVE1_DIR] = LLVector2(1.04999f, -0.42f).getValue();
		defaults[SETTING_WAVE2_DIR] = LLVector2(1.10999f, -1.16f).getValue();
		defaults[SETTING_TYPE] = "water";
	}
	return defaults;
}

LLSD LLSettingsWater::translateLegacySettings(const LLSD& legacy)
{
	bool converted_something(false);
	LLSD newsettings(defaults());

	if (legacy.has(SETTING_LEGACY_BLUR_MULTIPLIER))
	{
		newsettings[SETTING_BLUR_MULTIPLIER] =
			LLSD::Real(legacy[SETTING_LEGACY_BLUR_MULTIPLIER].asReal());
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_FOG_COLOR))
	{
		newsettings[SETTING_FOG_COLOR] =
			LLColor3(legacy[SETTING_LEGACY_FOG_COLOR]).getValue();
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_FOG_DENSITY))
	{
		newsettings[SETTING_FOG_DENSITY] =
			LLSD::Real(legacy[SETTING_LEGACY_FOG_DENSITY]);
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_FOG_MOD))
	{
		newsettings[SETTING_FOG_MOD] =
			LLSD::Real(legacy[SETTING_LEGACY_FOG_MOD].asReal());
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_FRESNEL_OFFSET))
	{
		newsettings[SETTING_FRESNEL_OFFSET] =
			LLSD::Real(legacy[SETTING_LEGACY_FRESNEL_OFFSET].asReal());
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_FRESNEL_SCALE))
	{
		newsettings[SETTING_FRESNEL_SCALE] =
			LLSD::Real(legacy[SETTING_LEGACY_FRESNEL_SCALE].asReal());
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_NORMAL_MAP))
	{
		newsettings[SETTING_NORMAL_MAP] =
			LLSD::UUID(legacy[SETTING_LEGACY_NORMAL_MAP].asUUID());
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_NORMAL_SCALE))
	{
		newsettings[SETTING_NORMAL_SCALE] =
			LLVector3(legacy[SETTING_LEGACY_NORMAL_SCALE]).getValue();
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_SCALE_ABOVE))
	{
		newsettings[SETTING_SCALE_ABOVE] =
			LLSD::Real(legacy[SETTING_LEGACY_SCALE_ABOVE].asReal());
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_SCALE_BELOW))
	{
		newsettings[SETTING_SCALE_BELOW] =
			LLSD::Real(legacy[SETTING_LEGACY_SCALE_BELOW].asReal());
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_WAVE1_DIR))
	{
		newsettings[SETTING_WAVE1_DIR] =
			LLVector2(legacy[SETTING_LEGACY_WAVE1_DIR]).getValue();
		converted_something = true;
	}
	if (legacy.has(SETTING_LEGACY_WAVE2_DIR))
	{
		newsettings[SETTING_WAVE2_DIR] =
			LLVector2(legacy[SETTING_LEGACY_WAVE2_DIR]).getValue();
		converted_something = true;
	}

	return converted_something ? newsettings : LLSD();
}

void LLSettingsWater::updateSettings()
{
	// Base class clears dirty flag so as to not trigger recursive update.
	// NOTE: this *must* be invoked first in this method ! HB
	LLSettingsBase::updateSettings();

	mBlurMultiplier = mSettings[SETTING_BLUR_MULTIPLIER].asReal();
	mFresnelOffset = mSettings[SETTING_FRESNEL_OFFSET].asReal();
	mFresnelScale = mSettings[SETTING_FRESNEL_SCALE].asReal();
	mNormalScale.setValue(mSettings[SETTING_NORMAL_SCALE]);
	mScaleAbove = mSettings[SETTING_SCALE_ABOVE].asReal();
	mScaleBelow = mSettings[SETTING_SCALE_BELOW].asReal();
	mWave1Dir.setValue(mSettings[SETTING_WAVE1_DIR]);
	mWave2Dir.setValue(mSettings[SETTING_WAVE2_DIR]);
	mWaterFogColor.setValue(mSettings[SETTING_FOG_COLOR]);
	mWaterFogDensity = mSettings[SETTING_FOG_DENSITY].asReal();
}

void LLSettingsWater::blend(const LLSettingsBase::ptr_t& end, F64 blendf)
{
	LLSettingsWater::ptr_t other =
		std::static_pointer_cast<LLSettingsWater>(end);
	if (other)
	{
		LLSD blenddata = interpolateSDMap(mSettings, other->mSettings,
										  other->getParameterMap(), blendf);
		replaceSettings(blenddata);
		mNextNormalMapID = other->getNormalMapID();
		mNextTransparentTextureID = other->getTransparentTextureID();
	}
	else
	{
		llwarns << "Could not cast end settings to water. No blend performed."
				<< llendl;
	}
	setBlendFactor(blendf);
}

void LLSettingsWater::replaceSettings(const LLSD& settings)
{
	LLSettingsBase::replaceSettings(settings);
	mNextNormalMapID.setNull();
	mNextTransparentTextureID.setNull();
}

void LLSettingsWater::replaceWithWater(LLSettingsWater::ptr_t other)
{
	replaceWith(other);
	mNextNormalMapID = other->mNextNormalMapID;
	mNextTransparentTextureID = other->mNextTransparentTextureID;
}

//virtual
const LLSettingsWater::validation_list_t& LLSettingsWater::getValidationList() const
{
	return LLSettingsWater::validationList();
}

//static
const LLSettingsWater::validation_list_t& LLSettingsWater::validationList()
{
	static validation_list_t validation;

	if (validation.empty())
	{
		validation.emplace_back(SETTING_BLUR_MULTIPLIER, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(-0.5f, 0.5f)));
		validation.emplace_back(SETTING_FOG_COLOR, true, LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2,
											llsd::array(0.0, 0.0, 0.0, 1.0),
											llsd::array(1.0, 1.0, 1.0, 1.0)));
		validation.emplace_back(SETTING_FOG_DENSITY, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(-10.0, 10.0)));
		validation.emplace_back(SETTING_FOG_MOD, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 20.0)));
		validation.emplace_back(SETTING_FRESNEL_OFFSET, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_FRESNEL_SCALE, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 1.0)));
		validation.emplace_back(SETTING_NORMAL_MAP, true, LLSD::TypeUUID);
		validation.emplace_back(SETTING_NORMAL_SCALE, true, LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2, llsd::array(0.0, 0.0, 0.0),
											llsd::array(10.0, 10.0, 10.0)));
		validation.emplace_back(SETTING_SCALE_ABOVE, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 3.0)));
		validation.emplace_back(SETTING_SCALE_BELOW, true, LLSD::TypeReal,
								boost::bind(&Validator::verifyFloatRange,
											_1, _2, llsd::array(0.0, 3.0)));
		validation.emplace_back(SETTING_WAVE1_DIR, true, LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2, llsd::array(-20.0, -20.0),
											llsd::array(20.0, 20.0)));
		validation.emplace_back(SETTING_WAVE2_DIR, true, LLSD::TypeArray,
								boost::bind(&Validator::verifyVectorMinMax,
											_1, _2, llsd::array(-20.0, -20.0),
											llsd::array(20.0, 20.0)));
	}

	return validation;
}

const LLUUID& LLSettingsWater::getDefaultAssetId()
{
	return DEFAULT_ASSET_ID;
}

const LLUUID& LLSettingsWater::getDefaultWaterNormalAssetId()
{
	return DEFAULT_WATER_NORMAL;
}

const LLUUID& LLSettingsWater::getDefaultTransparentTextureAssetId()
{
	return DEFAULT_WATER_TEXTURE;
}

const LLUUID& LLSettingsWater::getDefaultOpaqueTextureAssetId()
{
	return DEFAULT_WATER_OPAQUE;
}

F32 LLSettingsWater::getModifiedWaterFogDensity(bool underwater) const
{
	F32 fog_density = getWaterFogDensity();
	F32 underwater_fog_mod = getFogMod();
	if (underwater && underwater_fog_mod > 0.f)
	{
		underwater_fog_mod = llclamp(underwater_fog_mod, 0.f, 10.f);
		// BUG-233797/BUG-233798: negative underwater fog density can cause
		// (unrecoverable) blackout; raising a negative number to a non-
		// integral power results in a non-real result (which NaN for our
		// purposes). So we force density to be an arbitrary non-negative
		// (i.e. 1) when underwater and modifier is not an integer (1 was
		// chosen as it gives at least some notion of fog in the transition).
		if (fog_density < 0.f &&
			underwater_fog_mod != (F32)(S32(underwater_fog_mod)))
		{
			fog_density = 1.f;
		}
		else
		{
			fog_density = powf(fog_density, underwater_fog_mod);
		}
	}
	return fog_density;
}
