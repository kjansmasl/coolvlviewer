/**
 * @file llavatarappearancedefines.h
 * @brief Various LLAvatarAppearance related definitions
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_AVATARAPPEARANCE_DEFINES_H
#define LL_AVATARAPPEARANCE_DEFINES_H

#include <vector>

#include "llavatarjoint.h"
#include "lldictionary.h"
#include "lluuid.h"
#include "llwearabletype.h"

namespace LLAvatarAppearanceDefines
{

constexpr S32 IMPOSTOR_PERIOD = 2;
constexpr U32 AVATAR_HOVER = 11001;

//--------------------------------------------------------------------
// Enums
//--------------------------------------------------------------------
enum ETextureIndex
{
	TEX_INVALID = -1,
	TEX_HEAD_BODYPAINT = 0,
	TEX_UPPER_SHIRT,
	TEX_LOWER_PANTS,
	TEX_EYES_IRIS,
	TEX_HAIR,
	TEX_UPPER_BODYPAINT,
	TEX_LOWER_BODYPAINT,
	TEX_LOWER_SHOES,
	TEX_HEAD_BAKED,		// Pre-composited
	TEX_UPPER_BAKED,	// Pre-composited
	TEX_LOWER_BAKED,	// Pre-composited
	TEX_EYES_BAKED,		// Pre-composited
	TEX_LOWER_SOCKS,
	TEX_UPPER_JACKET,
	TEX_LOWER_JACKET,
	TEX_UPPER_GLOVES,
	TEX_UPPER_UNDERSHIRT,
	TEX_LOWER_UNDERPANTS,
	TEX_SKIRT,
	TEX_SKIRT_BAKED,	// Pre-composited
	TEX_HAIR_BAKED,     // Pre-composited
	TEX_LOWER_ALPHA,
	TEX_UPPER_ALPHA,
	TEX_HEAD_ALPHA,
	TEX_EYES_ALPHA,
	TEX_HAIR_ALPHA,
	TEX_HEAD_TATTOO,
	TEX_UPPER_TATTOO,
	TEX_LOWER_TATTOO,
	TEX_HEAD_UNIVERSAL_TATTOO,
	TEX_UPPER_UNIVERSAL_TATTOO,
	TEX_LOWER_UNIVERSAL_TATTOO,
	TEX_SKIRT_TATTOO,
	TEX_HAIR_TATTOO,
	TEX_EYES_TATTOO,
	TEX_LEFT_ARM_TATTOO,
	TEX_LEFT_LEG_TATTOO,
	TEX_AUX1_TATTOO,
	TEX_AUX2_TATTOO,
	TEX_AUX3_TATTOO,
	TEX_LEFT_ARM_BAKED,	// Pre-composited
	TEX_LEFT_LEG_BAKED,	// Pre-composited
	TEX_AUX1_BAKED,		// Pre-composited
	TEX_AUX2_BAKED,		// Pre-composited
	TEX_AUX3_BAKED,		// Pre-composited
	TEX_NUM_INDICES
};

enum EBakedTextureIndex
{
	BAKED_HEAD = 0,
	BAKED_UPPER,
	BAKED_LOWER,
	BAKED_EYES,
	BAKED_SKIRT,
	BAKED_HAIR,
	BAKED_LEFT_ARM,
	BAKED_LEFT_LEG,
	BAKED_AUX1,
	BAKED_AUX2,
	BAKED_AUX3,
	BAKED_NUM_INDICES
};

// Reference IDs for each mesh. Used as indices for vector of joints
enum EMeshIndex
{
	MESH_ID_HAIR = 0,
	MESH_ID_HEAD,
	MESH_ID_EYELASH,
	MESH_ID_UPPER_BODY,
	MESH_ID_LOWER_BODY,
	MESH_ID_EYEBALL_LEFT,
	MESH_ID_EYEBALL_RIGHT,
	MESH_ID_SKIRT,
	MESH_ID_NUM_INDICES
};

//-----------------------------------------------------------------------------
// Vector Types
//-----------------------------------------------------------------------------
typedef std::vector<ETextureIndex> texture_vec_t;
typedef std::vector<EBakedTextureIndex> bakedtexture_vec_t;
typedef std::vector<EMeshIndex> mesh_vec_t;
typedef std::vector<LLWearableType::EType> wearables_vec_t;

//-----------------------------------------------------------------------------
// LLAvatarAppearanceDictionary
//
// Holds dictionary static entries for textures, baked textures, meshes, etc,
// i.e. information that is common to all avatars.
//
// This holds const data - it is initialized once and the contents never change
// after that.
//-----------------------------------------------------------------------------
class LLAvatarAppearanceDictionary
{
public:
	LLAvatarAppearanceDictionary();
	~LLAvatarAppearanceDictionary();

private:
	void createAssociations();

public:
	//-------------------------------------------------------------------------
	// Local and baked textures
	//-------------------------------------------------------------------------
	struct TextureEntry : public LLDictionaryEntry
	{
		// 'name' must match the xml name used by LLTexLayerInfo::parseXml
		TextureEntry(const std::string& name,
					 bool is_local_texture,
					 EBakedTextureIndex baked_texture_index = BAKED_NUM_INDICES,
					 const std::string& default_image_name = "",
					 LLWearableType::EType wearable_type = LLWearableType::WT_INVALID);

		const std::string			mDefaultImageName;
		const LLWearableType::EType	mWearableType;

		// It is either a local texture or a baked one
		bool 						mIsLocalTexture;
		bool 						mIsBakedTexture;
		// If it is a local texture, it may be used by a baked texture
		bool 						mIsUsedByBakedTexture;
		EBakedTextureIndex			mBakedTextureIndex;
	};

	struct Textures : public LLDictionary<ETextureIndex, TextureEntry>
	{
		Textures();
	} mTextures;

	LL_INLINE const TextureEntry* getTexture(ETextureIndex index) const
	{
		return mTextures.lookup(index);
	}

	LL_INLINE const Textures& getTextures() const
	{
		return mTextures;
	}

	//-------------------------------------------------------------------------
	// Meshes
	//-------------------------------------------------------------------------
	struct MeshEntry : public LLDictionaryEntry
	{
		MeshEntry(EBakedTextureIndex baked_index,
				  // Name of mesh types as they are used in avatar_lad.xml
				  const std::string& name, U8 level, LLJointPickName pick);
		// Levels of Detail for each mesh. Must match levels of detail present
		// in avatar_lad.xml, otherwise meshes will be unable to be found, or
		// levels of detail will be ignored
		const U8					mLOD;
		const EBakedTextureIndex	mBakedID;
		const LLJointPickName		mPickName;
	};

	struct MeshEntries : public LLDictionary<EMeshIndex, MeshEntry>
	{
		MeshEntries();
	} mMeshEntries;

	LL_INLINE const MeshEntry* getMeshEntry(EMeshIndex index) const
	{
		return mMeshEntries.lookup(index);
	}

	LL_INLINE const MeshEntries& getMeshEntries() const
	{
		return mMeshEntries;
	}

	//-------------------------------------------------------------------------
	// Baked Textures
	//-------------------------------------------------------------------------
	struct BakedEntry : public LLDictionaryEntry
	{
		BakedEntry(ETextureIndex tex_index,
				   const std::string& name, // unused but needed for template
				   const std::string& hash_name,
				   // # local textures, local texture list, # wearables,
				   // wearable list
				   U32 num_local_textures, ... );
		// Local Textures
		const ETextureIndex mTextureIndex;
		texture_vec_t 		mLocalTextures;
		// Wearables
		const LLUUID 		mWearablesHashID;
		wearables_vec_t 	mWearables;
	};

	struct BakedTextures: public LLDictionary<EBakedTextureIndex, BakedEntry>
	{
		BakedTextures();
	} mBakedTextures;

	LL_INLINE const BakedEntry* getBakedTexture(EBakedTextureIndex index) const
	{
		return mBakedTextures.lookup(index);
	}

	LL_INLINE const BakedTextures& getBakedTextures() const
	{
		return mBakedTextures;
	}

	//-------------------------------------------------------------------------
	// Convenience Functions
	//-------------------------------------------------------------------------
	// Convert from baked texture to associated texture; e.g.
	// BAKED_HEAD -> TEX_HEAD_BAKED
	static ETextureIndex bakedToLocalTextureIndex(EBakedTextureIndex t);

	// find a baked texture index based on its name
	static EBakedTextureIndex findBakedByRegionName(const std::string& name);
	static EBakedTextureIndex findBakedByImageName(const std::string& name);

	// Given a texture entry, determine which wearable type owns it.
	static LLWearableType::EType getTEWearableType(ETextureIndex index);

	// Attachments baking
	static bool isBakedImageId(const LLUUID& id);
	static EBakedTextureIndex assetIdToBakedTextureIndex(const LLUUID& id);
#if 0	// Not used
	static const LLUUID& localTextureIndexToMagicId(ETextureIndex t);
#endif
}; // End LLAvatarAppearanceDictionary

} // End namespace LLAvatarAppearanceDefines

extern LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary* gAvatarAppDictp;

#endif //LL_AVATARAPPEARANCE_DEFINES_H
