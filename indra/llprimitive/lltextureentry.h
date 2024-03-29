/**
 * @file lltextureentry.h
 * @brief LLTextureEntry base class
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

#ifndef LL_LLTEXTUREENTRY_H
#define LL_LLTEXTUREENTRY_H

#include "llgltfmaterial.h"
#include "llmaterial.h"
#include "llmaterialid.h"

class LLMediaEntry;

// These bits are used while unpacking TEM messages to tell which aspects of
// the texture entry changed.
constexpr S32 TEM_CHANGE_NONE = 0x0;
constexpr S32 TEM_CHANGE_COLOR = 0x1;
constexpr S32 TEM_CHANGE_TEXTURE = 0x2;
constexpr S32 TEM_CHANGE_MEDIA = 0x4;
constexpr S32 TEM_INVALID = 0x8;

constexpr S32 TEM_BUMPMAP_COUNT = 32;

// The Bump Shiny Fullbright values are bits in an eight bit field:
// +----------+
// | SSFBBBBB | S = Shiny, F = Fullbright, B = Bumpmap
// | 76543210 |
// +----------+
constexpr S32 TEM_BUMP_MASK 		= 0x1f; // 5 bits
constexpr S32 TEM_FULLBRIGHT_MASK 	= 0x01; // 1 bit
constexpr S32 TEM_SHINY_MASK 		= 0x03; // 2 bits
constexpr S32 TEM_BUMP_SHINY_MASK 	= (0xc0 | 0x1f);
constexpr S32 TEM_FULLBRIGHT_SHIFT 	= 5;
constexpr S32 TEM_SHINY_SHIFT 		= 6;

// The Media Tex Gen values are bits in a bit field:
// +----------+
// | .....TTM | M = Media Flags (web page), T = LLTextureEntry::eTexGen, . = unused
// | 76543210 |
// +----------+
constexpr S32 TEM_MEDIA_MASK		= 0x01;
constexpr S32 TEM_TEX_GEN_MASK		= 0x06;
constexpr S32 TEM_TEX_GEN_SHIFT		= 1;

constexpr F32 ONE255TH = 1.f / 255.f;

class LLTextureEntry final
{
protected:
	LOG_CLASS(LLTextureEntry);

public:
	static LLTextureEntry* newTextureEntry();

	typedef enum e_texgen
	{
		TEX_GEN_DEFAULT			= 0x00,
		TEX_GEN_PLANAR			= 0x02,
#if 0	// Not used
		TEX_GEN_SPHERICAL		= 0x04,
		TEX_GEN_CYLINDRICAL		= 0x06,
#endif
	} eTexGen;

	LLTextureEntry();
	LLTextureEntry(const LLUUID& tex_id);
	LLTextureEntry(const LLTextureEntry& rhs);

	~LLTextureEntry();

	LLTextureEntry& operator=(const LLTextureEntry& rhs);

	bool operator==(const LLTextureEntry& rhs) const;
	bool operator!=(const LLTextureEntry& rhs) const;

	LLSD asLLSD() const;
	void asLLSD(LLSD& sd) const;
	LL_INLINE operator LLSD() const					{ return asLLSD(); }
	bool fromLLSD(const LLSD& sd);

	LL_INLINE LLTextureEntry* newCopy() const		{ return new LLTextureEntry(*this); }

	LL_INLINE bool hasPendingMaterialUpdate() const	{ return mMaterialUpdatePending; }
	LL_INLINE bool isSelected() const				{ return mSelected; }
	LL_INLINE bool setSelected(bool sel)
	{
		bool prev_sel = mSelected;
		mSelected = sel;
		return prev_sel;
	}

	// These return a TEM_ flag from above to indicate if something changed.
	S32 setID(const LLUUID& tex_id);
	S32 setColor(const LLColor4& color);
	S32 setColor(const LLColor3& color);
	S32 setAlpha(F32 alpha);
	S32 setScale(F32 s, F32 t);
	S32 setScaleS(F32 s);
	S32 setScaleT(F32 t);
	S32 setOffset(F32 s, F32 t);
	S32 setOffsetS(F32 s);
	S32 setOffsetT(F32 t);
	S32 setRotation(F32 theta);

	S32 setBumpmap(U8 bump);
	S32 setFullbright(U8 bump);
	S32 setShiny(U8 bump);
	S32 setBumpShiny(U8 bump);
 	S32 setBumpShinyFullbright(U8 bump);
	S32 setGlow(F32 glow);

	S32 setTexGen(U8 tex_gen);
	S32 setMediaTexGen(U8 media);
	S32 setMediaFlags(U8 media_flags);

	S32	setMaterialID(const LLMaterialID& mat_id);
	S32 setMaterialParams(const LLMaterialPtr mat_parms);

	LL_INLINE const LLUUID& getID() const			{ return mID; }
	LL_INLINE bool isBlank() const					{ return mIsBlankTexture; }
	LL_INLINE bool isDefault() const				{ return mIsDefaultTexture; }

	LL_INLINE const LLColor4& getColor() const		{ return mColor; }

	LL_INLINE F32 getAlpha() const					{ return mColor.mV[VALPHA]; }
	LL_INLINE F32 isTransparent() const				{ return mColor.mV[VALPHA] < 0.001f; }
	LL_INLINE F32 isOpaque() const					{ return mColor.mV[VALPHA] >= 0.999f; }

	LL_INLINE void getScale(F32* s, F32* t) const	{ *s = mScaleS; *t = mScaleT; }
	LL_INLINE F32 getScaleS() const					{ return mScaleS; }
	LL_INLINE F32 getScaleT() const					{ return mScaleT; }

	LL_INLINE void getOffset(F32* s, F32* t) const	{ *s = mOffsetS; *t = mOffsetT; }
	LL_INLINE F32 getOffsetS() const				{ return mOffsetS; }
	LL_INLINE F32 getOffsetT() const				{ return mOffsetT; }

	LL_INLINE F32 getRotation() const				{ return mRotation; }
	LL_INLINE void getRotation(F32* theta) const	{ *theta = mRotation; }

	LL_INLINE U8 getBumpmap() const					{ return mBump & TEM_BUMP_MASK; }
	LL_INLINE U8 getFullbright() const				{ return (mBump >> TEM_FULLBRIGHT_SHIFT) & TEM_FULLBRIGHT_MASK; }
	LL_INLINE U8 getShiny() const					{ return (mBump >> TEM_SHINY_SHIFT) & TEM_SHINY_MASK; }
	LL_INLINE U8 getBumpShiny() const				{ return mBump & TEM_BUMP_SHINY_MASK; }
	LL_INLINE U8 getBumpShinyFullbright() const		{ return mBump; }

	LL_INLINE F32 getGlow() const					{ return mGlow; }
	LL_INLINE bool hasGlow() const					{ return mGlow >= ONE255TH; }

	LL_INLINE U8 getMediaFlags() const				{ return mMediaFlags & TEM_MEDIA_MASK; }

	LL_INLINE LLTextureEntry::e_texgen getTexGen() const
	{
		return LLTextureEntry::e_texgen(mMediaFlags & TEM_TEX_GEN_MASK);
	}

	LL_INLINE U8 getMediaTexGen() const				{ return mMediaFlags; }

	LL_INLINE const LLMaterialID& getMaterialID() const
	{
		return mMaterialID;
	}

	LL_INLINE const LLMaterialPtr& getMaterialParams() const
	{
		return mMaterial;
	}

	// *NOTE: it is possible for hasMedia() to return true, but getMediaData()
	// to return NULL. CONVERSELY, it is also possible for hasMedia() to return
	// false, but getMediaData() to NOT return NULL.
	LL_INLINE bool hasMedia() const					{ return (mMediaFlags & MF_HAS_MEDIA) != 0; }
	LL_INLINE LLMediaEntry* getMediaData() const	{ return mMediaEntry; }

	// Completely change the media data on this texture entry.
	void setMediaData(const LLMediaEntry& media_entry);
	// Returns true if media data was updated, false if it was cleared
	bool updateMediaData(const LLSD& media_data);
	// Clears media data, and sets the media flags bit to 0
	void clearMediaData();
	// Merges the given LLSD of media fields with this media entry. Only those
	// fields that are set that match the keys in LLMediaEntry will be
	// affected. If no fields are set or if the LLSD is undefined, this is a
	// no-op.
	void mergeIntoMediaData(const LLSD& media_fields);

	// Takes a media version string (an empty string or a previously-returned string)
	// and returns a "touched" string, touched by agent_id
	static std::string touchMediaVersionString(const std::string& in_version,
											   const LLUUID& agent_id);
	// Given a media version string, return the version
	static U32 getVersionFromMediaVersionString(const std::string& version_string);
	// Given a media version string, return the UUID of the agent
	static LLUUID getAgentIDFromMediaVersionString(const std::string& version_string);
	// Return whether or not the given string is actually a media version
	static bool isMediaVersionString(const std::string& version_string);

	// Media flags
	enum { MF_NONE = 0x0, MF_HAS_MEDIA = 0x1 };

	// GLTF support

	void setGLTFMaterial(LLGLTFMaterial* matp, bool local_origin = true);

	LL_INLINE LLGLTFMaterial* getGLTFMaterial() const
	{
		return mGLTFMaterial.get();
	}

	S32 setGLTFMaterialOverride(LLGLTFMaterial* matp);

	LL_INLINE LLGLTFMaterial* getGLTFMaterialOverride() const
	{
		return mGLTFMaterialOverrides.get();
	}

	// Clears most overrides so the render material better matches the material
	// Id (preserves transforms). If the overrides become passthrough, sets the
	// overrides to NULL.
	S32 setBaseMaterial();

	S32 setGLTFRenderMaterial(LLGLTFMaterial* matp);
	// Nuanced behavior here: if there is no render material, fall back to
	// getGLTFMaterial().
	LLGLTFMaterial* getGLTFRenderMaterial() const;

private:
	void init(const LLUUID& tex_id, F32 scale_s, F32 scale_t, F32 offset_s,
			  F32 offset_t, F32 rotation, U8 bump);

public:
	static const LLTextureEntry null;

	// LLSD key defines
	static const char*	OBJECT_ID_KEY;
	static const char*	OBJECT_MEDIA_DATA_KEY;
	static const char*	MEDIA_VERSION_KEY;
	static const char*	TEXTURE_INDEX_KEY;
	static const char*	TEXTURE_MEDIA_DATA_KEY;

private:
	// Note the media data is not sent via the same message structure as the
	// rest of the TE.
	LLMediaEntry*		mMediaEntry;	// The media data for the face

	// NOTE: when adding new data to this class, in addition to adding it to
	// the serializers asLLSD/fromLLSD and the message packers (e.g.
	// LLPrimitive::packTEMessage) you must also implement its copy in
	// LLPrimitive::copyTEs()
	LLUUID				mID;			// Texture UUID
	LLColor4			mColor;
	LLMaterialID		mMaterialID;
	LLMaterialPtr		mMaterial;

	typedef LLPointer<LLGLTFMaterial> glft_ptr_t;

	// Reference to GLTF material asset state; this should be the same
	// LLGLTFMaterial instance that exists in LLGLTFMaterialList.
	glft_ptr_t			mGLTFMaterial;

	// GLTF material parameter overrides: the viewer will use this data to
	// override material parameters.
	glft_ptr_t			mGLTFMaterialOverrides;

	// GLTF material to use for rendering: always an LLFetchedGLTFMaterial
	glft_ptr_t			mGLTFRenderMaterial;

	F32					mScaleS;	// S, T offset
	F32					mScaleT;	// S, T offset
	F32					mOffsetS;	// S, T offset
	F32					mOffsetT;	// S, T offset
	// Anti-clockwise rotation in rad about the bottom left corner
	F32					mRotation;

	F32					mGlow;
	U8					mBump;			// Bump map, shiny, and fullbright
	U8					mMediaFlags;	// replace with web page, movie, etc.
	bool				mMaterialUpdatePending;

	// Set to true when mID is null or equal to either the plywood or the blank
	// default textures. Used to decide whether to override the diffuse texture
	// with the base color texture when we have a GLTF material set. HB
	bool				mIsDefaultTexture;
	// Set to true when mID is equal to the blank default texture Id. Used to
	// avoid bothering with settings offsets, scales and rotation at render
	// time. HB
	bool				mIsBlankTexture;

	bool				mSelected;
};

#endif
