/**
 * @file llvlcomposition.h
 * @brief Viewer-side representation of a composition layer...
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

#ifndef LL_LLVLCOMPOSITION_H
#define LL_LLVLCOMPOSITION_H

#include "llviewertexture.h"

class LLSurface;

// Viewer-side representation of a layer...
class LLViewerLayer
{
public:
	LLViewerLayer(S32 width, F32 scale = 1.f);
	virtual ~LLViewerLayer();

	F32 getValueScaled(F32 x, F32 y) const;

protected:
	LL_INLINE F32 getValue(S32 x, S32 y) const
	{
		return *(mDatap + x + y * mWidth);
	}

protected:
	F32*	mDatap;
	S32		mWidth;
	F32		mScale;
	F32		mScaleInv;
};

class LLVLComposition final : public LLViewerLayer
{
	friend class LLVOSurfacePatch;
	friend class LLDrawPoolTerrain;

protected:
	LOG_CLASS(LLVLComposition);

public:
	LLVLComposition(LLSurface* surfacep, U32 width, F32 scale);

	LL_INLINE void setSurface(LLSurface* s)		{ mSurfacep = s; }

	void forceRebuild();

	// Viewer side hack to generate composition values
	bool generateHeights(F32 x, F32 y, F32 width, F32 height);

	bool detailTexturesReady();

	LL_INLINE bool compositionGenerated()		{ return mTexturesLoaded; }

	// GenerateS texture from composition values.
	bool generateTexture(F32 x, F32 y, F32 width, F32 height);

	// Use these as indices into the get/setters below that use 'terrain'
	enum ETerrain
	{
		TERRAIN_DIRT = 0,
		TERRAIN_GRASS = 1,
		TERRAIN_MOUNTAIN = 2,
		TERRAIN_ROCK = 3,
		TERRAIN_COUNT = 4
	};

	LL_INLINE LLViewerFetchedTexture* getDetailTexture(S32 terrain)
	{
		return mDetailTextures[terrain];
	}

	LL_INLINE LLUUID getDetailTextureID(S32 terrain)
	{
		return mDetailTextures[terrain]->getID();
	}

	void setDetailTextureID(S32 terrain, const LLUUID& id);

	LL_INLINE F32 getStartHeight(S32 t)			{ return mStartHeight[t]; }
	LL_INLINE void setStartHeight(S32 t, F32 h)	{ mStartHeight[t] = h; }

	LL_INLINE F32 getHeightRange(S32 t)			{ return mHeightRange[t]; }
	LL_INLINE void setHeightRange(S32 t, F32 r)	{ mHeightRange[t] = r; }

	LL_INLINE void setParamsReady()				{ mParamsReady = true; }
	LL_INLINE bool getParamsReady() const		{ return mParamsReady; }

protected:
	LLSurface*							mSurfacep;

	LLPointer<LLViewerFetchedTexture>	mDetailTextures[TERRAIN_COUNT];
	LLPointer<LLImageRaw>				mRawImages[TERRAIN_COUNT];

	F32									mStartHeight[TERRAIN_COUNT];
	F32									mHeightRange[TERRAIN_COUNT];

	F32									mTexScaleX;
	F32									mTexScaleY;

	bool								mParamsReady;
	bool								mTexturesLoaded;
};

#endif //LL_LLVLCOMPOSITION_H
