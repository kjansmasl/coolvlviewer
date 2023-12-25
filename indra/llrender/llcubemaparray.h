/**
 * @file llcubemaparray.h
 * @brief LLCubeMapArray class definition
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

#ifndef LL_LLCUBEMAPARRAY_H
#define LL_LLCUBEMAPARRAY_H

#include "llgl.h"
#include "llimagegl.h"

class LLVector3;

class LLCubeMapArray : public LLRefCount
{
	friend class LLTexUnit;

protected:
	LL_INLINE ~LLCubeMapArray()
	{
		destroyGL();
	}

public:
	LL_INLINE LLCubeMapArray()
	:	mTextureStage(0),
		mTexName(0),
		mResolution(0),
		mCount(0)
	{
	}

	// Allocates a cube map array
	// res - resolution of each cube face
	// components - number of components per pixel
	// count - number of cube maps in the array
	// use_mips - if true, mipmaps will be allocated for this cube map array
	// and anisotropic filtering will be used.
	void allocate(U32 res, U32 components, U32 count, bool use_mips = true);

	void bind(S32 stage);
	void unbind();

	LL_INLINE U32 getGLName() const			{ return mImage->getTexName(); }

	void destroyGL();

	// Returns the resolution of the cubemaps in the array.
	LL_INLINE U32 getResolution() const		{ return mResolution; }
	// Returns the number of cubemaps in the array
	LL_INLINE U32 getCount() const			{ return mCount; }

protected:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variables, counting the 32 bits counter from LLRefCount. HB
	S32						mTextureStage;

	LLPointer<LLImageGL>	mImage;
	U32						mTexName;		// For GL image alloc tracking.

	U32						mResolution;
	U32						mCount;

public:
	static GLenum			sTargets[6];

	// Look and up vectors for each cube face (agent space)
	static LLVector3		sLookVecs[6];
	static LLVector3		sUpVecs[6];

	// Look and up vectors for each cube face (clip space)
	static LLVector3		sClipToCubeLookVecs[6];
	static LLVector3		sClipToCubeUpVecs[6];
};

#endif	// LL_LLCUBEMAPARRAY_H