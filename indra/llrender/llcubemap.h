/**
 * @file llcubemap.h
 * @brief LLCubeMap class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLCUBEMAP_H
#define LL_LLCUBEMAP_H

#include <vector>

#include "llgl.h"
#include "llimagegl.h"

class LLVector3;

// Environment map hack !
class LLCubeMap : public LLRefCount
{
	friend class LLTexUnit;

protected:
	LOG_CLASS(LLCubeMap);

	~LLCubeMap() override = default;

public:
	LLCubeMap(bool init_as_srgb = false);

	void init(const std::vector<LLPointer<LLImageRaw> >& rawimages);

	void initGL();
	void destroyGL();

	void initRawData(const std::vector<LLPointer<LLImageRaw> >& rawimages);
	void initGLData();

	void bind();
	void enableTexture(S32 stage);
	void disableTexture();

	void setMatrix(S32 stage);
	void restoreMatrix();

	LL_INLINE U32 getGLName() const		{ return mImages[0]->getTexName(); }

	// The methods below are used by the PBR renderer only.

	LL_INLINE U32 getResolution() const
	{
		return mImages[0].notNull() ? mImages[0]->getWidth(0) : 0;
	}

	// Initializes as an undefined cubemap at the given resolution used for
	// render-to-cubemap operations. Avoids usage of LLImageRaw.
	void initReflectionMap(U32 resolution, U32 components = 3);

	// Initializes from environment map images. Similar to init(), but takes
	// ownership of rawimages and makes this cubemap respect the resolution of
	// rawimages. Raw images must point to array of six square images that are
	// all the same resolution.
	void initEnvironmentMap(const std::vector<LLPointer<LLImageRaw> >& images);

	// Generates mip maps for this Cube Map using GL. NOTE: the cube map MUST
	// already be resident in VRAM.
	void generateMipMaps();

protected:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variables, counting the 32 bits counter from LLRefCount. HB

	S32						mTextureStage;

	U32						mTargets[6];

	LLPointer<LLImageGL>	mImages[6];
	LLPointer<LLImageRaw>	mRawImages[6];

	S32						mMatrixStage;

	bool					mIsSRGB;
};

#endif
