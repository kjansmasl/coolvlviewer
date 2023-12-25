/**
 * @file lldrawpoolsky.cpp
 * @brief LLDrawPoolSky class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolsky.h"

#include "lldrawable.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewershadermgr.h"
#include "llviewertexture.h"
#include "llvosky.h"

LLDrawPoolSky::LLDrawPoolSky()
:	LLFacePool(POOL_SKY),
	mSkyTex(NULL)
{
}

//virtual
void LLDrawPoolSky::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_ENVIRONMENT);
	if (!gUsePBRShaders)
	{
		LLDrawable* drawablep = gSky.mVOSkyp->mDrawable;
		if (drawablep)
		{
			gSky.mVOSkyp->updateGeometry(drawablep);
		}
	}
}

//virtual
void LLDrawPoolSky::render(S32)
{
	if (mDrawFace.empty() ||
		// Do not draw the sky box if we can and are rendering the WL sky dome.
		gPipeline.canUseWindLightShaders() ||
		// Do not render sky under water (background just gets cleared to fog
		// color).
		(mShaderLevel > 0 && LLPipeline::sUnderWaterRender))
	{
		return;
	}

	gGL.flush();

	// Just use the UI shader (generic single texture no lighting)
	gOneTextureNoColorProgram.bind();

	LLVector3 origin = gViewerCamera.getOrigin();
	U32 face_count = mDrawFace.size();

	LLGLSPipelineDepthTestSkyBox gls_skybox(GL_TRUE, GL_FALSE);

	gGL.pushMatrix();
	gGL.translatef(origin.mV[0], origin.mV[1], origin.mV[2]);

	LLVertexBuffer::unbind();
	gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);

	for (U32 i = 0; i < face_count; ++i)
	{
		renderSkyFace(i);
	}

	gGL.popMatrix();
}

void LLDrawPoolSky::renderSkyFace(U8 index)
{
	LLFace* facep = mDrawFace[index];
	if (!facep || !facep->getGeomCount())
	{
		return;
	}

	if (index < LLVOSky::FACE_SUN)			// Sky texture, interpolate
	{
		mSkyTex[index].bindTexture(true);	// Bind the current texture
		facep->renderIndexed();
	}
	else if (index == LLVOSky::FACE_MOON)	// Moon
	{
		// SL-14113: write depth for Moon so stars can test if behind it
		LLGLSPipelineDepthTestSkyBox gls_skybox(GL_TRUE, GL_TRUE);
		
		LLGLEnable blend(GL_BLEND);

		LLViewerTexture* texp = facep->getTexture(LLRender::DIFFUSE_MAP);
		if (texp)
		{
			gMoonProgram.bind(); // SL-14113 was gOneTextureNoColorProgram
			gGL.getTexUnit(0)->bind(texp);
			facep->renderIndexed();
		}
	}
	else									// Heavenly body faces, no interp.
	{
		// Reset to previous
		LLGLSPipelineDepthTestSkyBox gls_skybox(GL_TRUE, GL_FALSE);

		LLGLEnable blend(GL_BLEND);

		LLViewerTexture* texp = facep->getTexture(LLRender::DIFFUSE_MAP);
		if (texp)
		{
			gOneTextureNoColorProgram.bind();
			gGL.getTexUnit(0)->bind(texp);
			facep->renderIndexed();
		}
	}
}
