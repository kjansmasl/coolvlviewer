/**
 * @file llgltfmaterial.cpp
 * @brief LLGLTFMaterial implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewergpl$
 *
 * Copyright (c) 2022, Linden Research, Inc.
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

// *NOTE: this should be the one and only place tiny_gltf.h is included
#include "tinygltf/tiny_gltf.h"

#include "llgltfmaterial.h"

#include "hbxxh.h"

static const char* const GLTF_FILE_EXT_TF = "KHR_texture_transform";
static const char* const GLTF_FILE_EXT_TF_SCALE = "scale";
static const char* const GLTF_FILE_EXT_TF_OFFSET = "offset";
static const char* const GLTF_FILE_EXT_TF_ROTATION = "rotation";

// Special UUID that indicates a null UUID in override data
static const LLUUID GLTF_OVERRIDE_NULL_UUID =
	LLUUID("ffffffff-ffff-ffff-ffff-ffffffffffff");

const char* const LLGLTFMaterial::ASSET_TYPE = "GLTF 2.0";
const char* const LLGLTFMaterial::ASSET_VERSION = "1.1";
// Make a static default material for accessors
const LLGLTFMaterial LLGLTFMaterial::sDefault;

LLGLTFMaterial::LLGLTFMaterial()
{
	// IMPORTANT: since we use the hash of the member variables memory block of
	// this class to detect changes, we must ensure that all its padding bytes
	// have been zeroed out. But of course, we must leave the LLRefCount member
	// variable untouched (and skip it when hashing), and we cannot either
	// touch the local texture overrides map (else we destroy pointers, and
	// sundry private data, which would lead to a crash when using that map).
	// The variable members have therefore been arranged so that anything,
	// starting at mLocalTexDataDigest and up to the end of the members, can be
	// safely zeroed. HB
	const size_t offset = intptr_t(&mLocalTexDataDigest) - intptr_t(this);
	memset((void*)((const char*)this + offset), 0, sizeof(*this) - offset);

	// Now that we zeroed out our member variables, we can set the ones that
	// should not be zero to their default value. HB
	mBaseColor.set(1.f, 1.f, 1.f, 1.f);
	mMetallicFactor = mRoughnessFactor = 1.f;
	mAlphaCutoff = 0.5f;
	for (U32 i = 0; i < GLTF_TEXTURE_INFO_COUNT; ++i)
	{
		mTextureTransform[i].mScale.set(1.f, 1.f);
#if 0
		mTextureTransform[i].mOffset.clear();
		mTextureTransform[i].mRotation = 0.f;
#endif	
	}
#if 0
	mLocalTexDataDigest = 0;
	mAlphaMode = ALPHA_MODE_OPAQUE;	// This is 0
	mOverrideDoubleSided = mOverrideAlphaMode = false;
#endif
}

LLGLTFMaterial& LLGLTFMaterial::operator=(const LLGLTFMaterial& rhs)
{
	// We have to do a manual operator= because of LLRefCount
	mTextureId = rhs.mTextureId;
	mTextureTransform = rhs.mTextureTransform;
	mBaseColor = rhs.mBaseColor;
	mEmissiveColor = rhs.mEmissiveColor;
	mMetallicFactor = rhs.mMetallicFactor;
	mRoughnessFactor = rhs.mRoughnessFactor;
	mAlphaCutoff = rhs.mAlphaCutoff;
	mDoubleSided = rhs.mDoubleSided;
	mAlphaMode = rhs.mAlphaMode;
	mOverrideDoubleSided = rhs.mOverrideDoubleSided;
	mOverrideAlphaMode = rhs.mOverrideAlphaMode;
	if (rhs.mTrackingIdToLocalTexture.empty())
	{
		mTrackingIdToLocalTexture.clear();
		mLocalTexDataDigest = 0;
	}
	else
	{
		mTrackingIdToLocalTexture = rhs.mTrackingIdToLocalTexture;
		updateLocalTexDataDigest();
		updateTextureTracking();
	}
	return *this;
}

void LLGLTFMaterial::updateLocalTexDataDigest()
{
	mLocalTexDataDigest = 0;
	if (!mTrackingIdToLocalTexture.empty())
	{
		for (local_tex_map_t::const_iterator
				it = mTrackingIdToLocalTexture.begin(),
				end = mTrackingIdToLocalTexture.end();
			 it != end; ++it)
		{
			mLocalTexDataDigest ^= it->first.getDigest64() ^
								   it->second.getDigest64();
		}
	}
}

LLUUID LLGLTFMaterial::getHash() const
{
	// *HACK: hash the bytes of this object but do not include the ref count
	// neither the local texture overrides (which is a map, with pointers to
	// key/value pairs that would change from one LLGLTFMaterial instance to
	// the other, even though the key/value pairs could be the same, and stored
	// elsewhere in the memory heap or on the stack).
	// Note: this does work properly to compare two LLGLTFMaterial instances
	// only because the padding bytes between their member variables have been
	// dutifully zeroed in the constructor. HB
	const size_t offset = intptr_t(&mLocalTexDataDigest) - intptr_t(this);
	return HBXXH128::digest((const void*)((const char*)this + offset),
							sizeof(*this) - offset);
}

bool LLGLTFMaterial::fromJSON(const std::string& json, std::string& warn_msg,
							  std::string& error_msg)
{
	tinygltf::TinyGLTF gltf;
	tinygltf::Model model_in;
	if (gltf.LoadASCIIFromString(&model_in, &error_msg, &warn_msg,
								 json.c_str(), json.length(), ""))
	{
		setFromModel(model_in, 0);
		return true;
	}
	return false;
}

std::string LLGLTFMaterial::asJSON(bool prettyprint) const
{
	tinygltf::TinyGLTF gltf;
	tinygltf::Model model_out;
	writeToModel(model_out, 0);

	// To ensure consistency in asset upload, this should be the only reference
	// to WriteGltfSceneToStream in the viewer.
	std::ostringstream str;
	gltf.WriteGltfSceneToStream(&model_out, str, prettyprint, false);
	return str.str();
}

void LLGLTFMaterial::setFromModel(const tinygltf::Model& model, S32 mat_index)
{
	if (mat_index < 0 || mat_index > (S32)model.materials.size())
	{
		return;
	}

	const tinygltf::Material& mat = model.materials[mat_index];

	// Apply base color texture
	setFromTexture(model, mat.pbrMetallicRoughness.baseColorTexture,
				   GLTF_TEXTURE_INFO_BASE_COLOR);
	// Apply normal map
	setFromTexture(model, mat.normalTexture, GLTF_TEXTURE_INFO_NORMAL);
	// Apply metallic-roughness texture
	setFromTexture(model, mat.pbrMetallicRoughness.metallicRoughnessTexture,
				   GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS);
	// Apply emissive texture
	setFromTexture(model, mat.emissiveTexture, GLTF_TEXTURE_INFO_EMISSIVE);

	setAlphaMode(mat.alphaMode);
	mAlphaCutoff = llclamp(mat.alphaCutoff, 0.f, 1.f);

	mBaseColor.set(mat.pbrMetallicRoughness.baseColorFactor);
	mEmissiveColor.set(mat.emissiveFactor);

	mMetallicFactor = llclamp(mat.pbrMetallicRoughness.metallicFactor, 0.f,
							  1.f);
	mRoughnessFactor = llclamp(mat.pbrMetallicRoughness.roughnessFactor, 0.f,
							   1.f);

	mDoubleSided = mat.doubleSided;

	if (mat.extras.IsObject())
	{
		tinygltf::Value::Object extras =
			mat.extras.Get<tinygltf::Value::Object>();
		const auto& alpha_mode = extras.find("override_alpha_mode");
		if (alpha_mode != extras.end())
		{
			mOverrideAlphaMode = alpha_mode->second.Get<bool>();
		}

		const auto& double_sided = extras.find("override_double_sided");
		if (double_sided != extras.end())
		{
			mOverrideDoubleSided = double_sided->second.Get<bool>();
		}
	}
}

static LLVector2 vec2_from_json(const tinygltf::Value::Object& object,
								const char* key, const LLVector2& dflt_value)
{
	const auto it = object.find(key);
	if (it == object.end())
	{
		return dflt_value;
	}

	const tinygltf::Value& vec2_json = std::get<1>(*it);
	if (!vec2_json.IsArray() || vec2_json.ArrayLen() < LENGTHOFVECTOR2)
	{
		return dflt_value;
	}

	LLVector2 value;
	for (U32 i = 0; i < LENGTHOFVECTOR2; ++i)
	{
		const tinygltf::Value& real_json = vec2_json.Get(i);
		if (!real_json.IsReal())
		{
			return dflt_value;
		}
		value.mV[i] = (F32)real_json.Get<double>();
	}
	return value;
}

static F32 float_from_json(const tinygltf::Value::Object& object,
						   const char* key, const F32 dflt_value)
{
	const auto it = object.find(key);
	if (it == object.end())
	{
		return dflt_value;
	}

	const tinygltf::Value& real_json = std::get<1>(*it);
	if (!real_json.IsReal())
	{
		return dflt_value;
	}

	return (F32)real_json.GetNumberAsDouble();
}

template<typename T>
std::string gltf_get_texture_image(const tinygltf::Model& model,
								   const T& tex_info)
{
	S32 texture_idx = tex_info.index;
	if (texture_idx < 0 || texture_idx >= (S32)model.textures.size())
	{
		return "";
	}
	const tinygltf::Texture& texture = model.textures[texture_idx];

	// Ignore texture.sampler for now

	S32 image_idx = texture.source;
	if (image_idx < 0 || image_idx >= (S32)model.images.size())
	{
		return "";
	}
	const tinygltf::Image& image = model.images[image_idx];

	return image.uri;
}

// NOTE: we use template here as workaround for the different similar texture
// info classes
template<typename T>
void LLGLTFMaterial::setFromTexture(const tinygltf::Model& model,
									const T& tex_info, TextureInfo tex_info_id)
{
	const std::string uri = gltf_get_texture_image(model, tex_info);
	mTextureId[tex_info_id].set(uri);

	const tinygltf::Value::Object& ext_obj = tex_info.extensions;
	const auto transform_it = ext_obj.find(GLTF_FILE_EXT_TF);
	if (transform_it == ext_obj.end())
	{
		return;
	}

	const tinygltf::Value& tf_json = std::get<1>(*transform_it);
	if (!tf_json.IsObject())
	{
		return;
	}

	const tinygltf::Value::Object& tf_obj =
		tf_json.Get<tinygltf::Value::Object>();
	TextureTransform& transform = mTextureTransform[tex_info_id];
	transform.mOffset = vec2_from_json(tf_obj, GLTF_FILE_EXT_TF_OFFSET,
									   getDefaultTextureOffset());
	transform.mScale = vec2_from_json(tf_obj, GLTF_FILE_EXT_TF_SCALE,
									  getDefaultTextureScale());
	transform.mRotation = float_from_json(tf_obj, GLTF_FILE_EXT_TF_ROTATION,
										  getDefaultTextureRotation());
}

void LLGLTFMaterial::writeToModel(tinygltf::Model& model, S32 mat_index) const
{
	if (model.materials.size() < size_t(mat_index + 1))
	{
		model.materials.resize(mat_index + 1);
	}

	tinygltf::Material& mat = model.materials[mat_index];

	// Set base color texture
	writeToTexture(model, mat.pbrMetallicRoughness.baseColorTexture,
				   GLTF_TEXTURE_INFO_BASE_COLOR);
	// Set normal texture
	writeToTexture(model, mat.normalTexture, GLTF_TEXTURE_INFO_NORMAL);
	// Set metallic-roughness texture
	writeToTexture(model, mat.pbrMetallicRoughness.metallicRoughnessTexture,
				   GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS);
	// Set emissive texture
	writeToTexture(model, mat.emissiveTexture, GLTF_TEXTURE_INFO_EMISSIVE);
	// Set occlusion texture. Note: this is required for ORM materials for GLTF
	// compliance. See:
	// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_occlusiontexture
	writeToTexture(model, mat.occlusionTexture, GLTF_TEXTURE_INFO_OCCLUSION);

	mat.alphaMode = getAlphaMode();
	mat.alphaCutoff = mAlphaCutoff;

	mBaseColor.write(mat.pbrMetallicRoughness.baseColorFactor);

	if (mEmissiveColor != LLGLTFMaterial::getDefaultEmissiveColor())
	{
		mat.emissiveFactor.resize(3);
		mEmissiveColor.write(mat.emissiveFactor);
	}

	mat.pbrMetallicRoughness.metallicFactor = mMetallicFactor;
	mat.pbrMetallicRoughness.roughnessFactor = mRoughnessFactor;

	mat.doubleSided = mDoubleSided;

	// Generate "extras" string
	tinygltf::Value::Object extras;
	bool write_extras = false;
	if (mOverrideAlphaMode && mAlphaMode == getDefaultAlphaMode())
	{
		extras["override_alpha_mode"] = tinygltf::Value(mOverrideAlphaMode);
		write_extras = true;
	}
	if (mOverrideDoubleSided && mDoubleSided == getDefaultDoubleSided())
	{
		extras["override_double_sided"] =
			tinygltf::Value(mOverrideDoubleSided);
		write_extras = true;
	}
	if (write_extras)
	{
		mat.extras = tinygltf::Value(extras);
	}

	model.asset.version = "2.0";
}

template<typename T>
void gltf_allocate_texture_image(tinygltf::Model& model, T& tex_info,
								 const std::string& uri)
{
	const S32 image_idx = model.images.size();
	model.images.emplace_back();
	model.images[image_idx].uri = uri;

	// The texture, not to be confused with the texture info
	const S32 texture_idx = model.textures.size();
	model.textures.emplace_back();
	tinygltf::Texture& texture = model.textures[texture_idx];
	texture.source = image_idx;

	tex_info.index = texture_idx;
}

template<typename T>
void LLGLTFMaterial::writeToTexture(tinygltf::Model& model, T& tex_info,
									TextureInfo tex_info_id, bool force) const
{
	const LLUUID& texture_id = mTextureId[tex_info_id];
	const TextureTransform& transform = mTextureTransform[tex_info_id];
	const bool is_blank_transform = transform == sDefault.mTextureTransform[0];
	// Check if this material matches all the fallback values, and if so, then
	// skip including it to reduce material size
	if (!force && texture_id.isNull() && is_blank_transform)
	{
		return;
	}

	// tinygltf will discard this texture info if there is no valid texture,
	// causing potential loss of information for overrides, so ensure one is
	// defined. -Cosmic,2023-01-30
	gltf_allocate_texture_image(model, tex_info, texture_id.asString());

	if (!is_blank_transform)
	{
		tinygltf::Value::Object transform_map;
		transform_map[GLTF_FILE_EXT_TF_OFFSET] =
			tinygltf::Value(tinygltf::Value::Array({
				tinygltf::Value(transform.mOffset.mV[VX]),
				tinygltf::Value(transform.mOffset.mV[VY])
			}));
		transform_map[GLTF_FILE_EXT_TF_SCALE] =
			tinygltf::Value(tinygltf::Value::Array({
				tinygltf::Value(transform.mScale.mV[VX]),
				tinygltf::Value(transform.mScale.mV[VY])
			}));
		transform_map[GLTF_FILE_EXT_TF_ROTATION] =
			tinygltf::Value(transform.mRotation);
		tex_info.extensions[GLTF_FILE_EXT_TF] = tinygltf::Value(transform_map);
	}
}

bool LLGLTFMaterial::setBaseMaterial()
{
	const LLGLTFMaterial old_override = *this;
	*this = sDefault;
	setBaseMaterial(old_override);
	return *this != old_override;
}

bool LLGLTFMaterial::isClearedForBaseMaterial() const
{
	LLGLTFMaterial cleared_override = sDefault;
	cleared_override.setBaseMaterial(*this);
	return *this == cleared_override;
}

//static
void LLGLTFMaterial::hackOverrideUUID(LLUUID& id)
{
	if (id.isNull())
	{
		id = GLTF_OVERRIDE_NULL_UUID;
	}
}

void LLGLTFMaterial::setBaseColorFactor(const LLColor4& baseColor,
										bool for_override)
{
	mBaseColor.set(baseColor);
	mBaseColor.clamp();
	if (for_override)
	{
		// *HACK: nudge off of default value
		if (mBaseColor == getDefaultBaseColor())
		{
			mBaseColor.mV[3] -= FLT_EPSILON;
		}
	}
}

void LLGLTFMaterial::setAlphaCutoff(F32 cutoff, bool for_override)
{
	mAlphaCutoff = llclamp(cutoff, 0.f, 1.f);
	if (for_override)
	{
		// *HACK: nudge off of default value
		if (mAlphaCutoff == getDefaultAlphaCutoff())
		{
			mAlphaCutoff -= FLT_EPSILON;
		}
	}
}

void LLGLTFMaterial::setEmissiveColorFactor(const LLColor3& emissiveColor,
											bool for_override)
{
	mEmissiveColor = emissiveColor;
	mEmissiveColor.clamp();
	if (for_override)
	{
		// *HACK: nudge off of default value
		if (mEmissiveColor == getDefaultEmissiveColor())
		{
			mEmissiveColor.mV[0] += FLT_EPSILON;
		}
	}
}

void LLGLTFMaterial::setMetallicFactor(F32 metallic, bool for_override)
{
	mMetallicFactor = llclamp(metallic, 0.f,
							  for_override ? 1.f - FLT_EPSILON : 1.f);
}

void LLGLTFMaterial::setRoughnessFactor(F32 roughness, bool for_override)
{
	mRoughnessFactor = llclamp(roughness, 0.f,
							   for_override ? 1.f - FLT_EPSILON : 1.f);
}

void LLGLTFMaterial::setAlphaMode(const std::string& mode, bool for_override)
{
	S32 m = ALPHA_MODE_OPAQUE; //getDefaultAlphaMode();
	if (mode == "MASK")
	{
		m = ALPHA_MODE_MASK;
	}
	else if (mode == "BLEND")
	{
		m = ALPHA_MODE_BLEND;
	}
	setAlphaMode(m, for_override);
}

const char* LLGLTFMaterial::getAlphaMode() const
{
	switch (mAlphaMode)
	{
		default:
		case ALPHA_MODE_OPAQUE:
			return "OPAQUE";

		case ALPHA_MODE_BLEND:
			return "BLEND";

		case ALPHA_MODE_MASK:
			return "MASK";
	}
}

void LLGLTFMaterial::setAlphaMode(U32 mode, bool for_override)
{
	mAlphaMode = llclamp(mode, ALPHA_MODE_OPAQUE, ALPHA_MODE_MASK);
	mOverrideAlphaMode = for_override && mAlphaMode == getDefaultAlphaMode();
}

void LLGLTFMaterial::setDoubleSided(bool double_sided, bool for_override)
{
	// Sure, no clamping will ever be needed for a bool, but include the
	// setter for consistency with the clamping API
	mDoubleSided = double_sided;
	mOverrideDoubleSided = for_override &&
						   mDoubleSided == getDefaultDoubleSided();
}

LLSD LLGLTFMaterial::getOverrideLLSD() const
{
	LLSD data;

	// Make every effort to shave off bytes here
	for (U32 i = 0; i < GLTF_TEXTURE_INFO_COUNT; ++i)
	{
		const LLUUID& texture_id = mTextureId[i];
		const LLUUID& override_texture_id = mTextureId[i];
		if (override_texture_id.notNull() && texture_id != override_texture_id)
		{
			data["tex"][i] = LLSD::UUID(override_texture_id);
		}
	}
	if (mBaseColor != sDefault.mBaseColor)
	{
		data["bc"] = mBaseColor.getValue();
	}
	if (mEmissiveColor != sDefault.mEmissiveColor)
	{
		data["ec"] = mEmissiveColor.getValue();
	}
	if (mMetallicFactor != sDefault.mMetallicFactor)
	{
		data["mf"] = mMetallicFactor;
	}
	if (mRoughnessFactor != sDefault.mRoughnessFactor)
	{
		data["rf"] = mRoughnessFactor;
	}
	if (mAlphaMode != sDefault.mAlphaMode || mOverrideAlphaMode)
	{
		data["am"] = mAlphaMode;
	}
	if (mAlphaCutoff != sDefault.mAlphaCutoff)
	{
		data["ac"] = mAlphaCutoff;
	}
	if (mDoubleSided != sDefault.mDoubleSided || mOverrideDoubleSided)
	{
		data["ds"] = mDoubleSided;
	}
	const LLVector2& def_tex_offset = getDefaultTextureOffset();
	const LLVector2& def_tex_scale = getDefaultTextureScale();
	const F32 def_tex_rot = getDefaultTextureRotation();
	for (U32 i = 0; i < GLTF_TEXTURE_INFO_COUNT; ++i)
	{
		if (mTextureTransform[i].mOffset != def_tex_offset)
		{
			data["ti"][i]["o"] = mTextureTransform[i].mOffset.getValue();
		}
		if (mTextureTransform[i].mScale != def_tex_scale)
		{
			data["ti"][i]["s"] = mTextureTransform[i].mScale.getValue();
		}
		if (mTextureTransform[i].mRotation != def_tex_rot)
		{
			data["ti"][i]["r"] = mTextureTransform[i].mRotation;
		}
	}

	return data;
}

void LLGLTFMaterial::applyOverrideLLSD(const LLSD& data)
{
	const LLSD& tex = data["tex"];
	if (tex.isArray())
	{
		for (U32 i = 0, count = tex.size(); i < count; ++i)
		{
			mTextureId[i] = tex[i].asUUID();
		}
	}
	const LLSD& bc = data["bc"];
	if (bc.isDefined())
	{
		mBaseColor.setValue(bc);
		if (mBaseColor == getDefaultBaseColor())
		{
			// *HACK: nudge by epsilon if we receive a default value (indicates
			// override to default).
			mBaseColor.mV[3] -= FLT_EPSILON;
		}
	}
	const LLSD& ec = data["ec"];
	if (ec.isDefined())
	{
		mEmissiveColor.setValue(ec);
		if (mEmissiveColor == getDefaultEmissiveColor())
		{
			// *HACK: nudge by epsilon if we receive a default value (indicates
			// override to default).
			mEmissiveColor.mV[0] -= FLT_EPSILON;
		}
	}
	const LLSD& mf = data["mf"];
	if (mf.isReal())
	{
		mMetallicFactor = mf.asReal();
		if (mMetallicFactor == getDefaultMetallicFactor())
		{
			// *HACK: nudge by epsilon if we receive a default value (indicates
			// override to default).
			mMetallicFactor -= FLT_EPSILON;
		}
	}
	const LLSD& rf = data["rf"];
	if (rf.isReal())
	{
		mRoughnessFactor = rf.asReal();
		if (mRoughnessFactor == getDefaultRoughnessFactor())
		{
			// *HACK: nudge by epsilon if we receive a default value (indicates
			// override to default).
			mRoughnessFactor -= FLT_EPSILON;
		}
	}
	const LLSD& am = data["am"];
	if (am.isInteger())
	{
		mAlphaMode = am.asInteger();
		mOverrideAlphaMode = true;
	}
	const LLSD& ac = data["ac"];
	if (ac.isReal())
	{
		mAlphaCutoff = ac.asReal();
		if (mAlphaCutoff == getDefaultAlphaCutoff())
		{
			// *HACK: nudge by epsilon if we receive a default value (indicates
			// override to default).
			mAlphaCutoff -= FLT_EPSILON;
		}
	}
	const LLSD& ds = data["ds"];
	if (ds.isBoolean())
	{
		mDoubleSided = ds.asBoolean();
		mOverrideDoubleSided = true;
	}
	const LLSD& ti = data["ti"];
	if (ti.isArray())
	{
		for (U32 i = 0; i < GLTF_TEXTURE_INFO_COUNT; ++i)
		{
			const LLSD& o = ti[i]["o"];
			if (o.isDefined())
			{
				mTextureTransform[i].mOffset.setValue(o);
			}
			const LLSD& s = ti[i]["s"];
			if (s.isDefined())
			{
				mTextureTransform[i].mScale.setValue(s);
			}
			const LLSD& r = ti[i]["r"];
			if (r.isReal())
			{
				mTextureTransform[i].mRotation = r.asReal();
			}
		}
	}
}

static void apply_override_id(LLUUID& dst_id, const LLUUID& over_id)
{
	if (over_id == GLTF_OVERRIDE_NULL_UUID)
	{
		dst_id.setNull();
	}
	else if (over_id.notNull())
	{
		dst_id = over_id;
	}
}

void LLGLTFMaterial::applyOverride(const LLGLTFMaterial& override_mat)
{
	for (U32 i = 0; i < GLTF_TEXTURE_INFO_COUNT; ++i)
	{
		LLUUID& texture_id = mTextureId[i];
		const LLUUID& override_texture_id = override_mat.mTextureId[i];
		apply_override_id(texture_id, override_texture_id);
	}

	if (override_mat.mBaseColor != getDefaultBaseColor())
	{
		mBaseColor = override_mat.mBaseColor;
	}

	if (override_mat.mEmissiveColor != getDefaultEmissiveColor())
	{
		mEmissiveColor = override_mat.mEmissiveColor;
	}

	if (override_mat.mMetallicFactor != getDefaultMetallicFactor())
	{
		mMetallicFactor = override_mat.mMetallicFactor;
	}

	if (override_mat.mRoughnessFactor != getDefaultRoughnessFactor())
	{
		mRoughnessFactor = override_mat.mRoughnessFactor;
	}

	if (override_mat.mAlphaMode != getDefaultAlphaMode() ||
		override_mat.mOverrideAlphaMode)
	{
		mAlphaMode = override_mat.mAlphaMode;
	}
	if (override_mat.mAlphaCutoff != getDefaultAlphaCutoff())
	{
		mAlphaCutoff = override_mat.mAlphaCutoff;
	}

	if (override_mat.mDoubleSided != getDefaultDoubleSided() ||
		override_mat.mOverrideDoubleSided)
	{
		mDoubleSided = override_mat.mDoubleSided;
	}

	for (U32 i = 0; i < GLTF_TEXTURE_INFO_COUNT; ++i)
	{
		if (override_mat.mTextureTransform[i].mOffset !=
				getDefaultTextureOffset())
		{
			mTextureTransform[i].mOffset =
				override_mat.mTextureTransform[i].mOffset;
		}

		if (override_mat.mTextureTransform[i].mScale !=
				getDefaultTextureScale())
		{
			mTextureTransform[i].mScale =
				override_mat.mTextureTransform[i].mScale;
		}

		if (override_mat.mTextureTransform[i].mRotation !=
				getDefaultTextureRotation())
		{
			mTextureTransform[i].mRotation =
				override_mat.mTextureTransform[i].mRotation;
		}
	}

	if (!override_mat.mTrackingIdToLocalTexture.empty())
	{
		auto it = override_mat.mTrackingIdToLocalTexture.begin();
		mTrackingIdToLocalTexture.insert(it, it);
		updateLocalTexDataDigest();
		updateTextureTracking();
	}
}

void LLGLTFMaterial::addLocalTextureTracking(const LLUUID& tracking_id,
											 const LLUUID& tex_id)
{
	mTrackingIdToLocalTexture.emplace(tracking_id, tex_id);
	updateLocalTexDataDigest();
}

void LLGLTFMaterial::removeLocalTextureTracking(const LLUUID& tracking_id)
{
	mTrackingIdToLocalTexture.erase(tracking_id);
	updateLocalTexDataDigest();
}

//virtual
bool LLGLTFMaterial::replaceLocalTexture(const LLUUID& tracking_id,
										 const LLUUID& old_id,
										 const LLUUID& new_id)
{
	bool seen = false;

	for (U32 i = 0; i < GLTF_TEXTURE_INFO_COUNT; ++i)
	{
		if (mTextureId[i] == old_id)
		{
			mTextureId[i] = new_id;
			seen = true;
		}
		else if (mTextureId[i] == new_id)
		{
			seen = true;
		}
	}
	if (seen)
	{
		mTrackingIdToLocalTexture.emplace(tracking_id, new_id);
	}
	else
	{
		mTrackingIdToLocalTexture.erase(tracking_id);
	}
	updateLocalTexDataDigest();

	return seen;
}
