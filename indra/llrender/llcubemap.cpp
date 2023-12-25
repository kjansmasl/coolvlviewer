/**
 * @file llcubemap.cpp
 * @brief LLCubeMap class implementation
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

#include "linden_common.h"

#include "llcubemap.h"

#include "llglslshader.h"
#include "llmatrix4a.h"
#include "llrender.h"
#include "llvector3.h"

constexpr U16 RESOLUTION = 64;

LLCubeMap::LLCubeMap(bool init_as_srgb)
:	mTextureStage(0),
	mMatrixStage(0),
	mIsSRGB(init_as_srgb)
{
	mTargets[0] = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
	mTargets[1] = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
	mTargets[2] = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
	mTargets[3] = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
	mTargets[4] = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
	mTargets[5] = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
}

void LLCubeMap::initGL()
{
	llassert(gGLManager.mInited);

	if (mImages[0].isNull())
	{
		U32 texname = 0;
		LLImageGL::generateTextures(1, &texname);

		LLTexUnit* unit0 = gGL.getTexUnit(0);
		for (S32 i = 0; i < 6; ++i)
		{
			mImages[i] = new LLImageGL(RESOLUTION, RESOLUTION, 4, false);
			mImages[i]->setTarget(mTargets[i], LLTexUnit::TT_CUBE_MAP);
			mRawImages[i] = new LLImageRaw(RESOLUTION, RESOLUTION, 4);
			mImages[i]->createGLTexture(0, mRawImages[i], texname);

			unit0->bindManual(LLTexUnit::TT_CUBE_MAP, texname);
			mImages[i]->setAddressMode(LLTexUnit::TAM_CLAMP);
			stop_glerror();
		}
		unit0->disable();
	}
	disableTexture();
}

void LLCubeMap::destroyGL()
{
	for (S32 i = 0; i < 6; ++i)
	{
		mImages[i] = NULL;
	}
}

void LLCubeMap::initRawData(const std::vector<LLPointer<LLImageRaw> >& rawimages)
{
	bool flip_x[6] =	{ false, true,  false, false, true,  false };
	bool flip_y[6] = 	{ true,  true,  true,  false, true,  true  };
	bool transpose[6] = { false, false, false, false, true,  true  };

	// Yes, I know that this is inefficient! - djs 08/08/02
	for (S32 i = 0; i < 6; ++i)
	{
		const U8* sd = rawimages[i]->getData();
		U8* td = mRawImages[i]->getData();
		if (!sd || !td)
		{
			continue;
		}

		S32 offset = 0;
		S32 sx, sy, so;
		for (S32 y = 0; y < RESOLUTION; ++y)
		{
			for (S32 x = 0; x < RESOLUTION; ++x)
			{
				sx = x;
				sy = y;
				if (flip_y[i])
				{
					sy = 63 - y;
				}
				if (flip_x[i])
				{
					sx = 63 - x;
				}
				if (transpose[i])
				{
					S32 temp = sx;
					sx = sy;
					sy = temp;
				}

				so = RESOLUTION * sy + sx;
				so *= 4;
				*(td + offset++) = *(sd + so++);
				*(td + offset++) = *(sd + so++);
				*(td + offset++) = *(sd + so++);
				*(td + offset++) = *(sd + so++);
			}
		}
	}
}

void LLCubeMap::initGLData()
{
	for (S32 i = 0; i < 6; ++i)
	{
		mImages[i]->setSubImage(mRawImages[i], 0, 0, RESOLUTION, RESOLUTION);
	}
}

void LLCubeMap::init(const std::vector<LLPointer<LLImageRaw> >& rawimages)
{
	if (!gGLManager.mIsDisabled)
	{
		initGL();
		initRawData(rawimages);
		initGLData();
	}
}

void LLCubeMap::bind()
{
	gGL.getTexUnit(mTextureStage)->bind(this);
}

void LLCubeMap::enableTexture(S32 stage)
{
	mTextureStage = stage;
	if (stage >= 0)
	{
		gGL.getTexUnit(stage)->enable(LLTexUnit::TT_CUBE_MAP);
	}
}

void LLCubeMap::disableTexture()
{
	if (mTextureStage >= 0)
	{
		LLTexUnit* unit = gGL.getTexUnit(mTextureStage);
		unit->disable();
		if (mTextureStage == 0)
		{
			unit->enable(LLTexUnit::TT_TEXTURE);
		}
	}
}

void LLCubeMap::setMatrix(S32 stage)
{
	mMatrixStage = stage;

	if (mMatrixStage < 0) return;

#if 0
	if (stage > 0)
#endif
	{
		gGL.getTexUnit(stage)->activate();
	}

	LLMatrix4a trans(gGLModelView);
	trans.setRow<3>(LLVector4a::getZero());
	trans.transpose();

	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.pushMatrix();
	gGL.loadMatrix(trans);
	gGL.matrixMode(LLRender::MM_MODELVIEW);

#if 0
	if (stage > 0)
	{
		gGL.getTexUnit(0)->activate();
	}
#endif
}

void LLCubeMap::restoreMatrix()
{
	if (mMatrixStage < 0) return;

#if 0
	if (mMatrixStage > 0)
#endif
	{
		gGL.getTexUnit(mMatrixStage)->activate();
	}
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

#if 0
	if (mMatrixStage > 0)
	{
		gGL.getTexUnit(0)->activate();
	}
#endif
}

void LLCubeMap::initReflectionMap(U32 resolution, U32 components)
{
	U32 texname = 0;
	LLImageGL::generateTextures(1, &texname);

	mImages[0] = new LLImageGL(resolution, resolution, components, true);
	mImages[0]->setTexName(texname);
	mImages[0]->setTarget(mTargets[0], LLTexUnit::TT_CUBE_MAP);
	gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_CUBE_MAP, texname);
	mImages[0]->setAddressMode(LLTexUnit::TAM_CLAMP);
}

void LLCubeMap::initEnvironmentMap(const std::vector<LLPointer<LLImageRaw> >& rawimages)
{
	llassert(rawimages.size() == 6);

	U32 texname = 0;
	LLImageGL::generateTextures(1, &texname);

	U32 resolution = rawimages[0]->getWidth();
	U32 components = rawimages[0]->getComponents();

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	for (U32 i = 0; i < 6; ++i)
	{
		llassert(rawimages[i]->getWidth() == resolution &&
				 rawimages[i]->getHeight() == resolution &&
				 (U32)rawimages[i]->getComponents() == components);

		mImages[i] = new LLImageGL(resolution, resolution, components, true);
		mImages[i]->setTarget(mTargets[i], LLTexUnit::TT_CUBE_MAP);
		mRawImages[i] = rawimages[i];
		mImages[i]->createGLTexture(0, mRawImages[i], texname);

		unit0->bindManual(LLTexUnit::TT_CUBE_MAP, texname);
		mImages[i]->setAddressMode(LLTexUnit::TAM_CLAMP);
		stop_glerror();
		mImages[i]->setSubImage(mRawImages[i], 0, 0, resolution, resolution);
	}
	enableTexture(0);
	bind();
	mImages[0]->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	unit0->disable();
	disableTexture();
}

void LLCubeMap::generateMipMaps()
{
	mImages[0]->setUseMipMaps(true);
	mImages[0]->setHasMipMaps(true);
	enableTexture(0);
	bind();
	mImages[0]->setFilteringOption(LLTexUnit::TFO_BILINEAR);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	gGL.getTexUnit(0)->disable();
	disableTexture();
}
