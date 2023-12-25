/**
 * @file llmaterial.cpp
 * @brief Material definition
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include "llmaterial.h"

#include "hbxxh.h"

// Materials cap parameters

#define MAT_CAP_NORMAL_MAP_FIELD            "NormMap"
#define MAT_CAP_NORMAL_MAP_OFFSET_X_FIELD   "NormOffsetX"
#define MAT_CAP_NORMAL_MAP_OFFSET_Y_FIELD   "NormOffsetY"
#define MAT_CAP_NORMAL_MAP_REPEAT_X_FIELD   "NormRepeatX"
#define MAT_CAP_NORMAL_MAP_REPEAT_Y_FIELD   "NormRepeatY"
#define MAT_CAP_NORMAL_MAP_ROTATION_FIELD   "NormRotation"

#define MAT_CAP_SPECULAR_MAP_FIELD          "SpecMap"
#define MAT_CAP_SPECULAR_MAP_OFFSET_X_FIELD "SpecOffsetX"
#define MAT_CAP_SPECULAR_MAP_OFFSET_Y_FIELD "SpecOffsetY"
#define MAT_CAP_SPECULAR_MAP_REPEAT_X_FIELD "SpecRepeatX"
#define MAT_CAP_SPECULAR_MAP_REPEAT_Y_FIELD "SpecRepeatY"
#define MAT_CAP_SPECULAR_MAP_ROTATION_FIELD "SpecRotation"

#define MAT_CAP_SPECULAR_COLOR_FIELD        "SpecColor"
#define MAT_CAP_SPECULAR_EXP_FIELD          "SpecExp"
#define MAT_CAP_ENV_INTENSITY_FIELD         "EnvIntensity"
#define MAT_CAP_ALPHA_MASK_CUTOFF_FIELD     "AlphaMaskCutoff"
#define MAT_CAP_DIFFUSE_ALPHA_MODE_FIELD    "DiffuseAlphaMode"

const LLColor4U LLMaterial::DEFAULT_SPECULAR_LIGHT_COLOR(255, 255, 255, 255);
const LLMaterial LLMaterial::null;

constexpr F32 MAT_MULTIPLIER = 10000.f;

// Helper functions

template<typename T> T getMaterialField(const LLSD& data,
										const std::string& field,
										const LLSD::Type field_type)
{
	if (data.has(field) && field_type == data[field].type())
	{
		return (T)data[field];
	}
	llwarns << "Missing or mistyped field '" << field
			<< "' in material definition" << llendl;
	return (T)LLSD();
}

// GCC did not like the generic form above for some reason
template<> LLUUID getMaterialField(const LLSD& data, const std::string& field,
								   const LLSD::Type field_type)
{
	if (data.has(field) && field_type == data[field].type())
	{
		return data[field].asUUID();
	}
	llwarns << "Missing or mistyped field '" << field
			<< "' in material definition" << llendl;
	return LLUUID::null;
}

///////////////////////////////////////////////////////////////////////////////
// LLMaterial class proper
///////////////////////////////////////////////////////////////////////////////

LLMaterial::LLMaterial()
{
	// IMPORTANT: since we use the hash of the member variables memory block of
	// this class to detect changes, we must ensure that this block (and its
	// padding bytes) have been zeroed out. But of course, we must leave the
	// LLRefCount member variable untouched (and skip it when hashing). HB
	constexpr size_t offset = sizeof(LLRefCount);
	memset((void*)((const char*)this + offset), 0, sizeof(*this) - offset);

	// Now that we zeroed out our member variables, we can set the ones that
	// should not be zero to their default value. HB
	mNormalRepeatX = mNormalRepeatY = 1.f;
	mSpecularRepeatX = mSpecularRepeatY = 1.f;
	mSpecularLightColor = DEFAULT_SPECULAR_LIGHT_COLOR;
	mSpecularLightExponent = DEFAULT_SPECULAR_LIGHT_EXPONENT;
	mEnvironmentIntensity = DEFAULT_ENV_INTENSITY;
	mDiffuseAlphaMode = DIFFUSE_ALPHA_MODE_BLEND;
#if 0
	mNormalOffsetX = mNormalOffsetY = mNormalRotation = 0.f;
	mSpecularOffsetX = mSpecularOffsetY = mSpecularRotation = 0.f;
#endif
}

LLMaterial::LLMaterial(const LLSD& material_data)
{
	fromLLSD(material_data);
}

LLUUID LLMaterial::getHash() const
{
	// *HACK: hash the bytes of this object but do not include the ref count
	// Note: this does work properly only because the padding bytes between our
	// member variables have been zeroed in the constructor. HB
	constexpr size_t offset = sizeof(LLRefCount);
	return HBXXH128::digest((const void*)((const char*)this + offset),
							sizeof(*this) - offset);
}

LLSD LLMaterial::asLLSD() const
{
	LLSD material_data;

	material_data[MAT_CAP_NORMAL_MAP_FIELD] = mNormalID;
	material_data[MAT_CAP_NORMAL_MAP_OFFSET_X_FIELD] =
		ll_round(mNormalOffsetX * MAT_MULTIPLIER);
	material_data[MAT_CAP_NORMAL_MAP_OFFSET_Y_FIELD] =
		ll_round(mNormalOffsetY * MAT_MULTIPLIER);
	material_data[MAT_CAP_NORMAL_MAP_REPEAT_X_FIELD] =
		ll_round(mNormalRepeatX * MAT_MULTIPLIER);
	material_data[MAT_CAP_NORMAL_MAP_REPEAT_Y_FIELD] =
		ll_round(mNormalRepeatY * MAT_MULTIPLIER);
	material_data[MAT_CAP_NORMAL_MAP_ROTATION_FIELD] =
		ll_round(mNormalRotation * MAT_MULTIPLIER);

	material_data[MAT_CAP_SPECULAR_MAP_FIELD] = mSpecularID;
	material_data[MAT_CAP_SPECULAR_MAP_OFFSET_X_FIELD] =
		ll_round(mSpecularOffsetX * MAT_MULTIPLIER);
	material_data[MAT_CAP_SPECULAR_MAP_OFFSET_Y_FIELD] =
		ll_round(mSpecularOffsetY * MAT_MULTIPLIER);
	material_data[MAT_CAP_SPECULAR_MAP_REPEAT_X_FIELD] =
		ll_round(mSpecularRepeatX * MAT_MULTIPLIER);
	material_data[MAT_CAP_SPECULAR_MAP_REPEAT_Y_FIELD] =
		ll_round(mSpecularRepeatY * MAT_MULTIPLIER);
	material_data[MAT_CAP_SPECULAR_MAP_ROTATION_FIELD] =
		ll_round(mSpecularRotation * MAT_MULTIPLIER);

	material_data[MAT_CAP_SPECULAR_COLOR_FIELD] =
		mSpecularLightColor.getValue();
	material_data[MAT_CAP_SPECULAR_EXP_FIELD] = mSpecularLightExponent;
	material_data[MAT_CAP_ENV_INTENSITY_FIELD] = mEnvironmentIntensity;
	material_data[MAT_CAP_DIFFUSE_ALPHA_MODE_FIELD] = mDiffuseAlphaMode;
	material_data[MAT_CAP_ALPHA_MASK_CUTOFF_FIELD] = mAlphaMaskCutoff;

	return material_data;
}

void LLMaterial::fromLLSD(const LLSD& material_data)
{
	mNormalID = getMaterialField<LLSD::UUID>(material_data,
											 MAT_CAP_NORMAL_MAP_FIELD,
											 LLSD::TypeUUID);
	mNormalOffsetX =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_NORMAL_MAP_OFFSET_X_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mNormalOffsetY =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_NORMAL_MAP_OFFSET_Y_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mNormalRepeatX =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_NORMAL_MAP_REPEAT_X_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mNormalRepeatY =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_NORMAL_MAP_REPEAT_Y_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mNormalRotation =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_NORMAL_MAP_ROTATION_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;

	mSpecularID =
		getMaterialField<LLSD::UUID>(material_data,
									 MAT_CAP_SPECULAR_MAP_FIELD,
									 LLSD::TypeUUID);
	mSpecularOffsetX =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_SPECULAR_MAP_OFFSET_X_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mSpecularOffsetY =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_SPECULAR_MAP_OFFSET_Y_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mSpecularRepeatX =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_SPECULAR_MAP_REPEAT_X_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mSpecularRepeatY =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_SPECULAR_MAP_REPEAT_Y_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;
	mSpecularRotation =
		(F32)getMaterialField<LLSD::Integer>(material_data,
											 MAT_CAP_SPECULAR_MAP_ROTATION_FIELD,
											 LLSD::TypeInteger) / MAT_MULTIPLIER;

	mSpecularLightColor.setValue(getMaterialField<LLSD>(material_data,
														MAT_CAP_SPECULAR_COLOR_FIELD,
														LLSD::TypeArray));
	mSpecularLightExponent =
		(U8)getMaterialField<LLSD::Integer>(material_data,
											MAT_CAP_SPECULAR_EXP_FIELD,
											LLSD::TypeInteger);
	mEnvironmentIntensity =
		(U8)getMaterialField<LLSD::Integer>(material_data,
											MAT_CAP_ENV_INTENSITY_FIELD,
											LLSD::TypeInteger);
	mDiffuseAlphaMode =
		(U8)getMaterialField<LLSD::Integer>(material_data,
											MAT_CAP_DIFFUSE_ALPHA_MODE_FIELD,
											LLSD::TypeInteger);
	mAlphaMaskCutoff =
		(U8)getMaterialField<LLSD::Integer>(material_data,
											MAT_CAP_ALPHA_MASK_CUTOFF_FIELD,
											LLSD::TypeInteger);
}

bool LLMaterial::isNull() const
{
	return *this == null;
}

bool LLMaterial::operator==(const LLMaterial& rhs) const
{
	return mNormalID == rhs.mNormalID &&
		   mNormalOffsetX == rhs.mNormalOffsetX &&
		   mNormalOffsetY == rhs.mNormalOffsetY &&
		   mNormalRepeatX == rhs.mNormalRepeatX &&
		   mNormalRepeatY == rhs.mNormalRepeatY &&
		   mNormalRotation == rhs.mNormalRotation &&
		   mSpecularID == rhs.mSpecularID &&
		   mSpecularOffsetX == rhs.mSpecularOffsetX &&
		   mSpecularOffsetY == rhs.mSpecularOffsetY &&
		   mSpecularRepeatX == rhs.mSpecularRepeatX &&
		   mSpecularRepeatY == rhs.mSpecularRepeatY &&
		   mSpecularRotation == rhs.mSpecularRotation &&
		   mSpecularLightColor == rhs.mSpecularLightColor &&
		   mSpecularLightExponent == rhs.mSpecularLightExponent &&
		   mEnvironmentIntensity == rhs.mEnvironmentIntensity &&
		   mDiffuseAlphaMode == rhs.mDiffuseAlphaMode &&
		   mAlphaMaskCutoff == rhs.mAlphaMaskCutoff;
}

bool LLMaterial::operator!=(const LLMaterial& rhs) const
{
	return !(*this == rhs);
}

// NEVER incorporate this value into the message system: this function will
// vary depending on viewer implementation
U32 LLMaterial::getShaderMask(U32 alpha_mode, bool is_alpha)
{
	U32 ret = 0;

	// Two least significant bits are "diffuse alpha mode"
	if (alpha_mode != DIFFUSE_ALPHA_MODE_DEFAULT)
	{
		ret = alpha_mode;
	}
	else
	{
		ret = getDiffuseAlphaMode();
		if (ret == DIFFUSE_ALPHA_MODE_BLEND && !is_alpha)
		{
			ret = DIFFUSE_ALPHA_MODE_NONE;
		}
	}

	llassert(ret < SHADER_COUNT);

	// Next bit is whether or not specular map is present
	constexpr U32 SPEC_BIT = 0x4;

	if (getSpecularID().notNull())
	{
		ret |= SPEC_BIT;
	}

	llassert(ret < SHADER_COUNT);

	// Next bit is whether or not normal map is present
	constexpr U32 NORM_BIT = 0x8;
	if (getNormalID().notNull())
	{
		ret |= NORM_BIT;
	}

	llassert(ret < SHADER_COUNT);

	return ret;
}
