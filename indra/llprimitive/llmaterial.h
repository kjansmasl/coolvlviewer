/**
 * @file llmaterial.h
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

#ifndef LL_LLMATERIAL_H
#define LL_LLMATERIAL_H

#include "llmaterialid.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llsd.h"
#include "llcolor4u.h"

class LLMaterial : public LLRefCount
{
public:
	typedef enum
	{
		DIFFUSE_ALPHA_MODE_NONE = 0,
		DIFFUSE_ALPHA_MODE_BLEND = 1,
		DIFFUSE_ALPHA_MODE_MASK = 2,
		DIFFUSE_ALPHA_MODE_EMISSIVE = 3,
		DIFFUSE_ALPHA_MODE_DEFAULT = 4,
	} eDiffuseAlphaMode;

	enum eShaderCount : U32
	{
		SHADER_COUNT = 16,
		ALPHA_SHADER_COUNT = 4
	};

	static const LLColor4U	DEFAULT_SPECULAR_LIGHT_COLOR;
	static constexpr U8		DEFAULT_SPECULAR_LIGHT_EXPONENT = (U8)(0.2f * 255);
	static constexpr U8		DEFAULT_ENV_INTENSITY = 0;

	LLMaterial();
	LLMaterial(const LLSD& material_data);

	LLSD asLLSD() const;
	void fromLLSD(const LLSD& material_data);

	LL_INLINE const LLUUID& getNormalID() const			{ return mNormalID; }
	LL_INLINE void setNormalID(const LLUUID& id)		{ mNormalID = id; }

	LL_INLINE void getNormalOffset(F32& offset_x, F32& offset_y) const
	{
		offset_x = mNormalOffsetX;
		offset_y = mNormalOffsetY;
	}

	LL_INLINE F32 getNormalOffsetX() const				{ return mNormalOffsetX; }
	LL_INLINE F32 getNormalOffsetY() const				{ return mNormalOffsetY; }

	LL_INLINE void setNormalOffset(F32 offset_x, F32 offset_y)
	{
		mNormalOffsetX = offset_x;
		mNormalOffsetY = offset_y;
	}

	LL_INLINE void setNormalOffsetX(F32 offset_x)		{ mNormalOffsetX = offset_x; }
	LL_INLINE void setNormalOffsetY(F32 offset_y)		{ mNormalOffsetY = offset_y; }

	LL_INLINE void getNormalRepeat(F32& repeat_x, F32& repeat_y) const
	{
		repeat_x = mNormalRepeatX;
		repeat_y = mNormalRepeatY;
	}

	LL_INLINE F32 getNormalRepeatX() const				{ return mNormalRepeatX; }
	LL_INLINE F32 getNormalRepeatY() const				{ return mNormalRepeatY; }

	LL_INLINE void setNormalRepeat(F32 repeat_x, F32 repeat_y)
	{
		mNormalRepeatX = repeat_x;
		mNormalRepeatY = repeat_y;
	}

	LL_INLINE void setNormalRepeatX(F32 repeat_x)		{ mNormalRepeatX = repeat_x; }
	LL_INLINE void setNormalRepeatY(F32 repeat_y)		{ mNormalRepeatY = repeat_y; }

	LL_INLINE F32 getNormalRotation() const				{ return mNormalRotation; }
	LL_INLINE void setNormalRotation(F32 rot)			{ mNormalRotation = rot; }

	LL_INLINE const LLUUID& getSpecularID() const		{ return mSpecularID; }
	LL_INLINE void setSpecularID(const LLUUID& id)		{ mSpecularID = id; }

	LL_INLINE void getSpecularOffset(F32& offset_x, F32& offset_y) const
	{
		offset_x = mSpecularOffsetX;
		offset_y = mSpecularOffsetY;
	}

	LL_INLINE F32 getSpecularOffsetX() const			{ return mSpecularOffsetX; }
	LL_INLINE F32 getSpecularOffsetY() const			{ return mSpecularOffsetY; }

	LL_INLINE void setSpecularOffset(F32 offset_x, F32 offset_y)
	{
		mSpecularOffsetX = offset_x;
		mSpecularOffsetY = offset_y;
	}

	LL_INLINE void setSpecularOffsetX(F32 offset_x)		{ mSpecularOffsetX = offset_x; }
	LL_INLINE void setSpecularOffsetY(F32 offset_y)		{ mSpecularOffsetY = offset_y; }

	LL_INLINE void getSpecularRepeat(F32& repeat_x, F32& repeat_y) const
	{
		repeat_x = mSpecularRepeatX;
		repeat_y = mSpecularRepeatY;
	}

	LL_INLINE F32 getSpecularRepeatX() const			{ return mSpecularRepeatX; }
	LL_INLINE F32 getSpecularRepeatY() const			{ return mSpecularRepeatY; }

	LL_INLINE void setSpecularRepeat(F32 repeat_x, F32 repeat_y)
	{
		mSpecularRepeatX = repeat_x;
		mSpecularRepeatY = repeat_y;
	}

	LL_INLINE void setSpecularRepeatX(F32 repeat_x)		{ mSpecularRepeatX = repeat_x; }
	LL_INLINE void setSpecularRepeatY(F32 repeat_y)		{ mSpecularRepeatY = repeat_y; }

	LL_INLINE F32 getSpecularRotation() const			{ return mSpecularRotation; }
	LL_INLINE void setSpecularRotation(F32 rot)			{ mSpecularRotation = rot; }

	LL_INLINE const LLColor4U& getSpecularLightColor() const
	{
		return mSpecularLightColor;
	}

	LL_INLINE void setSpecularLightColor(const LLColor4U& color)
	{
		mSpecularLightColor = color;
	}

	LL_INLINE U8 getSpecularLightExponent() const		{ return mSpecularLightExponent; }
	LL_INLINE void setSpecularLightExponent(U8 e)		{ mSpecularLightExponent = e; }
	LL_INLINE U8 getEnvironmentIntensity() const		{ return mEnvironmentIntensity; }
	LL_INLINE void setEnvironmentIntensity(U8 i)		{ mEnvironmentIntensity = i; }
	LL_INLINE U8 getDiffuseAlphaMode() const			{ return mDiffuseAlphaMode; }
	LL_INLINE void setDiffuseAlphaMode(U8 mode)			{ mDiffuseAlphaMode = mode; }
	LL_INLINE U8 getAlphaMaskCutoff() const				{ return mAlphaMaskCutoff; }
	LL_INLINE void setAlphaMaskCutoff(U8 cutoff)		{ mAlphaMaskCutoff = cutoff; }

	bool isNull() const;

	bool operator ==(const LLMaterial& rhs) const;
	bool operator !=(const LLMaterial& rhs) const;

	U32 getShaderMask(U32 alpha_mode, bool is_alpha);

	LLUUID getHash() const;

public:
	static const LLMaterial null;

protected:
	// Note: before these variables, we find the 32 bits counter from
	// LLRefCount... Placing five 32 bits floats first ensures the UUIDs
	// are aligned on 64 bits (where they are faster). HB

	F32			mNormalOffsetX;
	F32			mNormalOffsetY;
	F32			mNormalRepeatX;
	F32			mNormalRepeatY;
	F32			mNormalRotation;
	LLUUID		mNormalID;

	LLUUID		mSpecularID;
	F32			mSpecularOffsetX;
	F32			mSpecularOffsetY;
	F32			mSpecularRepeatX;
	F32			mSpecularRepeatY;
	F32			mSpecularRotation;

	LLColor4U	mSpecularLightColor;
	U8			mSpecularLightExponent;
	U8			mEnvironmentIntensity;
	U8			mDiffuseAlphaMode;
	U8			mAlphaMaskCutoff;
};

typedef LLPointer<LLMaterial> LLMaterialPtr;

#endif // LL_LLMATERIAL_H
