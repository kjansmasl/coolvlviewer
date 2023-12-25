/**
 * @file llcubemaparray.cpp
 * @brief LLCubeMapArray class implementation
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

#include "llcubemaparray.h"

#include "llrender.h"
#include "llvector3.h"

// Defined in llimagegl.cpp
extern void image_bound(U32 width, U32 height, U32 pixformat, U32 count = 1);
extern void image_unbound(U32 tex_name);

// MUST match order of OpenGL face-layers
GLenum LLCubeMapArray::sTargets[6] =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};

LLVector3 LLCubeMapArray::sLookVecs[6] =
{
	LLVector3(1.f, 0.f, 0.f),
	LLVector3(-1.f, 0.f, 0.f),
	LLVector3(0.f, 1.f, 0.f),
	LLVector3(0.f, -1.f, 0.f),
	LLVector3(0.f, 0.f, 1.f),
	LLVector3(0.f, 0.f, -1.f)
};

LLVector3 LLCubeMapArray::sUpVecs[6] =
{
	LLVector3(0.f, -1.f, 0.f),
	LLVector3(0.f, -1.f, 0.f),
	LLVector3(0.f, 0.f, 1.f),
	LLVector3(0.f, 0.f, -1.f),
	LLVector3(0.f, -1.f, 0.f),
	LLVector3(0.f, -1.f, 0.f)
};

LLVector3 LLCubeMapArray::sClipToCubeLookVecs[6] =
{
	LLVector3(0.f, 0.f, -1.f),
	LLVector3(0.f, 0.f, 1.f),
	LLVector3(1.f, 0.f, 0.f),
	LLVector3(1.f, 0.f, 0.f),
	LLVector3(1.f, 0.f, 0.f),
	LLVector3(-1.f, 0.f, 0.f)
};

LLVector3 LLCubeMapArray::sClipToCubeUpVecs[6] =
{
	LLVector3(-1.f, 0.f, 0.f),
	LLVector3(1.f, 0.f, 0.f),
	LLVector3(0.f, 1.f, 0.f),
	LLVector3(0.f, -1.f, 0.f),
	LLVector3(0.f, 0.f, -1.f),
	LLVector3(0.f, 0.f, 1.f)
};

void LLCubeMapArray::allocate(U32 resolution, U32 components, U32 count,
							  bool use_mips)
{
	mResolution = resolution;
	mCount = count;

	LLImageGL::generateTextures(1, &mTexName);

	mImage = new LLImageGL(resolution, resolution, components, use_mips);
	mImage->setTexName(mTexName);
	mImage->setTarget(sTargets[0], LLTexUnit::TT_CUBE_MAP_ARRAY);

	mImage->setUseMipMaps(use_mips);
	mImage->setHasMipMaps(use_mips);

	bind(0);

	U32 format = components == 4 ? GL_RGBA16F : GL_RGB16F;

	image_bound(resolution, resolution, format, 6 * count);

	U32 mip = 0;
	while (resolution >= 1)
	{
		glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, mip, format, resolution,
					 resolution, count * 6, 0, GL_RGBA, GL_UNSIGNED_BYTE,
					 NULL);
		if (!use_mips)
		{
			break;
		}
		resolution /= 2;
		++mip;
	}

	mImage->setAddressMode(LLTexUnit::TAM_CLAMP);

	if (use_mips)
	{
		mImage->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
#if 0	// Latest AMD drivers do not appreciate this method of allocating
		// mipmaps
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP_ARRAY);
#endif
	}
	else
	{
		mImage->setFilteringOption(LLTexUnit::TFO_BILINEAR);
	}

	unbind();
}

void LLCubeMapArray::destroyGL()
{
	mImage = NULL;
	if (mTexName)
	{
		image_unbound(mTexName);
		mTexName = 0;
	}
}

void LLCubeMapArray::bind(S32 stage)
{
	mTextureStage = stage;
	gGL.getTexUnit(stage)->bindManual(LLTexUnit::TT_CUBE_MAP_ARRAY,
									  getGLName(), mImage->getUseMipMaps());
}

void LLCubeMapArray::unbind()
{
	gGL.getTexUnit(mTextureStage)->unbind(LLTexUnit::TT_CUBE_MAP_ARRAY);
	mTextureStage = -1;
}
