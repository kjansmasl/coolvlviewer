/**
 * @file lldrawpoolsimple.cpp
 * @brief LLDrawPoolSimple class implementation
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

#include "lldrawpoolsimple.h"

#include "llfasttimer.h"
#include "llrender.h"

#include "lldrawable.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewershadermgr.h"

// Only used by the EE renderer.
static LLGLSLShader* sSimpleShader = NULL;

// Helper functions

static void setup_simple_shader(LLGLSLShader* shaderp)
{
	shaderp->bind();
	if (!gUsePBRShaders)
	{
		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		shaderp->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
	}
}

static void setup_glow_shader(LLGLSLShader* shaderp)
{
	shaderp->bind();

	if (gUsePBRShaders)
	{
		if (LLPipeline::sRenderingHUDs)
		{
			shaderp->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 1.f);
		}
		else
		{
			shaderp->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
		}
		return;
	}

	if (LLPipeline::sRenderingHUDs)
	{
		shaderp->uniform1i(LLShaderMgr::NO_ATMO, 1);
		shaderp->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 1.f);
		return;
	}
	shaderp->uniform1i(LLShaderMgr::NO_ATMO, 0);
	if (LLPipeline::sRenderDeferred)
	{
		shaderp->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
	}
	else
	{
		shaderp->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 1.f);
	}
}

static void setup_fullbright_shader(LLGLSLShader* shaderp)
{
	setup_glow_shader(shaderp);

	if (gUsePBRShaders)
	{
		S32 channel = shaderp->enableTexture(LLShaderMgr::EXPOSURE_MAP);
		if (channel > -1)
		{
			gGL.getTexUnit(channel)->bind(&gPipeline.mExposureMap);
		}
	}

	shaderp->uniform1f(LLViewerShaderMgr::FULLBRIGHT, 1.f);
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolSimple class
///////////////////////////////////////////////////////////////////////////////

LLDrawPoolSimple::LLDrawPoolSimple()
:	LLRenderPass(POOL_SIMPLE)
{
}

//virtual
void LLDrawPoolSimple::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

// Only for use with the EE renderer
//virtual
void LLDrawPoolSimple::render(S32)
{
	LL_FAST_TIMER(FTM_RENDER_SIMPLE);

	LLGLDisable blend(GL_BLEND);

	LLGLSLShader* shaderp;
	if (LLPipeline::sImpostorRender)
	{
		shaderp = &gObjectSimpleImpostorProgram;
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		shaderp = &gObjectSimpleWaterProgram;
	}
	else
	{
		shaderp = &gObjectSimpleProgram;
	}

	gPipeline.enableLightsDynamic();
	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;

	// First pass: static objects
	setup_simple_shader(shaderp);
	pushBatches(PASS_SIMPLE, mask, true, true);
	if (LLPipeline::sRenderDeferred)
	{
		// If deferred rendering is enabled, bump faces are not registered as
		// simple render bump faces here as simple so bump faces will appear
		// under water
		pushBatches(PASS_BUMP, mask, true, true);
		pushBatches(PASS_MATERIAL, mask, true, true);
		pushBatches(PASS_SPECMAP, mask, true, true);
		pushBatches(PASS_NORMMAP, mask, true, true);
		pushBatches(PASS_NORMSPEC, mask, true, true);
	}

	// Second pass: rigged objects
	if (!shaderp->mRiggedVariant)	// Paranoia
	{
		return;
	}
	setup_simple_shader(shaderp->mRiggedVariant);
	pushRiggedBatches(PASS_SIMPLE_RIGGED, mask, true, true);
	if (LLPipeline::sRenderDeferred)
	{
		// If deferred rendering is enabled, bump faces are not registered as
		// simple render bump faces here as simple so bump faces will appear
		// under water
		pushRiggedBatches(PASS_BUMP_RIGGED, mask, true, true);
		pushRiggedBatches(PASS_MATERIAL_RIGGED, mask, true, true);
		pushRiggedBatches(PASS_SPECMAP_RIGGED, mask, true, true);
		pushRiggedBatches(PASS_NORMMAP_RIGGED, mask, true, true);
		pushRiggedBatches(PASS_NORMSPEC_RIGGED, mask, true, true);
	}
}

//virtual
void LLDrawPoolSimple::renderDeferred(S32)
{
	LL_FAST_TIMER(FTM_RENDER_SIMPLE);

	LLGLDisable blend(GL_BLEND);

	LLGLSLShader* shaderp = &gDeferredDiffuseProgram;
	// Note: mask ignored by the PBR renderer
	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;

	// Render static
	setup_simple_shader(shaderp);
	pushBatches(PASS_SIMPLE, mask, true, true);

	// Render rigged
	if (!shaderp->mRiggedVariant)	// Paranoia
	{
		return;
	}
	setup_simple_shader(shaderp->mRiggedVariant);
	pushRiggedBatches(PASS_SIMPLE_RIGGED, mask, true, true);
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolGrass class
///////////////////////////////////////////////////////////////////////////////

// Grass drawpool
LLDrawPoolGrass::LLDrawPoolGrass()
:	LLRenderPass(POOL_GRASS)
{
}

//virtual
void LLDrawPoolGrass::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

//virtual
void LLDrawPoolGrass::beginRenderPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);

	stop_glerror();

	if (LLPipeline::sUnderWaterRender)
	{
		sSimpleShader = &gObjectAlphaMaskNonIndexedWaterProgram;
	}
	else
	{
		sSimpleShader = &gObjectAlphaMaskNonIndexedProgram;
	}

	if (mShaderLevel > 0)
	{
		sSimpleShader->bind();
		sSimpleShader->setMinimumAlpha(0.5f);
		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		sSimpleShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
	}
	else
	{
		// Do not use shaders !
		LLGLSLShader::unbind();	// Also calls gGL.flush()
	}
}

//virtual
void LLDrawPoolGrass::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);

	LLRenderPass::endRenderPass(pass);

	if (mShaderLevel > 0)
	{
		sSimpleShader->unbind();
	}
	else
	{
		gGL.flush();
	}
}

// Only for use with the EE renderer
//virtual
void LLDrawPoolGrass::render(S32)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);

	LLGLDisable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	// Render grass
	LLRenderPass::pushBatches(PASS_GRASS, getVertexDataMask());
}

//virtual
void LLDrawPoolGrass::renderDeferred(S32)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);
	LLGLSLShader* shaderp = &gDeferredNonIndexedDiffuseAlphaMaskProgram;
	shaderp->bind();
	shaderp->setMinimumAlpha(0.5f);
	if (!gUsePBRShaders)
	{
		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		shaderp->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
	}
	// Render grass
	LLRenderPass::pushBatches(PASS_GRASS, getVertexDataMask());
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolAlphaMask class
///////////////////////////////////////////////////////////////////////////////

LLDrawPoolAlphaMask::LLDrawPoolAlphaMask()
:	LLRenderPass(POOL_ALPHA_MASK)
{
}

//virtual
void LLDrawPoolAlphaMask::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

// Only for use with the EE renderer
//virtual
void LLDrawPoolAlphaMask::render(S32 pass)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK);

	LLGLDisable blend(GL_BLEND);

	LLGLSLShader* shaderp = LLPipeline::sUnderWaterRender ?
								&gObjectSimpleWaterAlphaMaskProgram :
								&gObjectSimpleAlphaMaskProgram;

	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;

	// Render static
	setup_simple_shader(shaderp);
	pushMaskBatches(PASS_ALPHA_MASK, mask, true, true);
	pushMaskBatches(PASS_MATERIAL_ALPHA_MASK, mask, true, true);
	pushMaskBatches(PASS_SPECMAP_MASK, mask, true, true);
	pushMaskBatches(PASS_NORMMAP_MASK, mask, true, true);
	pushMaskBatches(PASS_NORMSPEC_MASK, mask, true, true);

	// Render rigged
	if (!shaderp->mRiggedVariant)	// Paranoia
	{
		return;
	}
	setup_simple_shader(shaderp->mRiggedVariant);
	pushRiggedMaskBatches(PASS_ALPHA_MASK_RIGGED, mask, true, true);
	pushRiggedMaskBatches(PASS_MATERIAL_ALPHA_MASK_RIGGED, mask, true, true);
	pushRiggedMaskBatches(PASS_SPECMAP_MASK_RIGGED, mask, true, true);
	pushRiggedMaskBatches(PASS_NORMMAP_MASK_RIGGED, mask, true, true);
	pushRiggedMaskBatches(PASS_NORMSPEC_MASK_RIGGED, mask, true, true);
}

//virtual
void LLDrawPoolAlphaMask::renderDeferred(S32)
{
	LL_FAST_TIMER(FTM_RENDER_GRASS);

	LLGLSLShader* shaderp = &gDeferredDiffuseAlphaMaskProgram;
	// Note: mask ignored by the PBR renderer
	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;

	// Render static
	setup_simple_shader(shaderp);
	pushMaskBatches(PASS_ALPHA_MASK, mask, true, true);

	// Render rigged
	if (!shaderp->mRiggedVariant)	// Paranoia
	{
		return;
	}
	setup_simple_shader(shaderp->mRiggedVariant);
	pushRiggedMaskBatches(PASS_ALPHA_MASK_RIGGED, mask, true, true);
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolFullbrightAlphaMask class
///////////////////////////////////////////////////////////////////////////////

LLDrawPoolFullbrightAlphaMask::LLDrawPoolFullbrightAlphaMask()
:	LLRenderPass(POOL_FULLBRIGHT_ALPHA_MASK)
{
}

//virtual
void LLDrawPoolFullbrightAlphaMask::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

// Only for use with the EE renderer
//virtual
void LLDrawPoolFullbrightAlphaMask::render(S32)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_MASK);

	LLGLSLShader* shaderp = LLPipeline::sUnderWaterRender ?
								&gObjectFullbrightWaterAlphaMaskProgram :
								&gObjectFullbrightAlphaMaskProgram;

	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;

	// Render static
	setup_fullbright_shader(shaderp);
	pushMaskBatches(PASS_FULLBRIGHT_ALPHA_MASK, mask, true, true);

	// Render rigged
	if (!shaderp->mRiggedVariant)	// Paranoia
	{
		return;
	}
	setup_fullbright_shader(shaderp->mRiggedVariant);
	pushRiggedMaskBatches(PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, mask, true, true);
}

//virtual
void LLDrawPoolFullbrightAlphaMask::renderPostDeferred(S32)
{
	LL_TRACY_TIMER(TRC_RENDER_FULLBRIGHT);

	LLGLSLShader* shaderp;
	if (gUsePBRShaders)
	{
		if (LLPipeline::sRenderingHUDs)
		{
			shaderp = &gHUDFullbrightAlphaMaskProgram;
		}
		else
		{
			shaderp = &gDeferredFullbrightAlphaMaskProgram;
		}
	}
	else if (LLPipeline::sRenderingHUDs || !LLPipeline::sRenderDeferred)
	{
		shaderp = &gObjectFullbrightAlphaMaskProgram;
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		shaderp = &gDeferredFullbrightAlphaMaskWaterProgram;
	}
	else
	{
		shaderp = &gDeferredFullbrightAlphaMaskProgram;
	}

	LLGLDisable blend(GL_BLEND);

	// Note: mask ignored by the PBR renderer
	constexpr U32 mask = LLVertexBuffer::MAP_VERTEX |
						 LLVertexBuffer::MAP_TEXCOORD0 |
						 LLVertexBuffer::MAP_COLOR |
						 LLVertexBuffer::MAP_TEXTURE_INDEX;

	// Render static
	setup_fullbright_shader(shaderp);
	pushMaskBatches(PASS_FULLBRIGHT_ALPHA_MASK, mask, true, true);

	// Render rigged
	if (!shaderp->mRiggedVariant ||		// Paranoia
		(gUsePBRShaders && LLPipeline::sRenderingHUDs))
	{
		return;
	}
	setup_fullbright_shader(shaderp->mRiggedVariant);
	pushRiggedMaskBatches(PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, mask, true, true);
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolFullbright class
///////////////////////////////////////////////////////////////////////////////

LLDrawPoolFullbright::LLDrawPoolFullbright()
:	LLRenderPass(POOL_FULLBRIGHT)
{
}

//virtual
void LLDrawPoolFullbright::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

// Only for use with the EE renderer
//virtual
void LLDrawPoolFullbright::render(S32)
{
	LL_FAST_TIMER(FTM_RENDER_FULLBRIGHT);

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	stop_glerror();

	LLGLSLShader* shaderp = LLPipeline::sUnderWaterRender ?
								&gObjectFullbrightWaterProgram :
								&gObjectFullbrightProgram;

	constexpr U32 mask = LLVertexBuffer::MAP_VERTEX |
						 LLVertexBuffer::MAP_TEXCOORD0 |
						 LLVertexBuffer::MAP_COLOR |
						 LLVertexBuffer::MAP_TEXTURE_INDEX;

	// Render static
	setup_fullbright_shader(shaderp);
	pushBatches(PASS_FULLBRIGHT, mask, true, true);
	pushBatches(PASS_MATERIAL_ALPHA_EMISSIVE, mask, true, true);
	pushBatches(PASS_SPECMAP_EMISSIVE, mask, true, true);
	pushBatches(PASS_NORMMAP_EMISSIVE, mask, true, true);
	pushBatches(PASS_NORMSPEC_EMISSIVE, mask, true, true);

	// Render rigged
	if (!shaderp->mRiggedVariant)	// Paranoia
	{
		return;
	}
	setup_fullbright_shader(shaderp->mRiggedVariant);
	pushRiggedBatches(PASS_FULLBRIGHT_RIGGED, mask, true, true);
	pushRiggedBatches(PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED, mask, true, true);
	pushRiggedBatches(PASS_SPECMAP_EMISSIVE_RIGGED, mask, true, true);
	pushRiggedBatches(PASS_NORMMAP_EMISSIVE_RIGGED, mask, true, true);
	pushRiggedBatches(PASS_NORMSPEC_EMISSIVE_RIGGED, mask, true, true);
}

//virtual
void LLDrawPoolFullbright::renderPostDeferred(S32)
{
	LL_FAST_TIMER(FTM_RENDER_FULLBRIGHT);

	LLGLSLShader* shaderp;
	if (gUsePBRShaders && LLPipeline::sRenderingHUDs)
	{
		shaderp = &gHUDFullbrightProgram;
	}
	else if (LLPipeline::sUnderWaterRender && !gUsePBRShaders)
	{
		shaderp = &gDeferredFullbrightWaterProgram;
	}
	else
	{
		shaderp = &gDeferredFullbrightProgram;
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	// Note: mask ignored by the PBR renderer
	constexpr U32 mask = LLVertexBuffer::MAP_VERTEX |
						 LLVertexBuffer::MAP_TEXCOORD0 |
						 LLVertexBuffer::MAP_COLOR |
						 LLVertexBuffer::MAP_TEXTURE_INDEX;

	// Render static
	setup_fullbright_shader(shaderp);
	pushBatches(PASS_FULLBRIGHT, mask, true, true);

	// Render rigged
	if (!shaderp->mRiggedVariant ||		// Paranoia
		(gUsePBRShaders && LLPipeline::sRenderingHUDs))
	{
		return;
	}
	setup_fullbright_shader(shaderp->mRiggedVariant);
	pushRiggedBatches(PASS_FULLBRIGHT_RIGGED, mask, true, true);
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolGlow class
///////////////////////////////////////////////////////////////////////////////

// Only for use with the EE renderer
//virtual
void LLDrawPoolGlow::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_GLOW);

	LLGLSLShader* shaderp =
		LLPipeline::sUnderWaterRender ? &gObjectEmissiveWaterProgram
									  : &gObjectEmissiveProgram;
	render(shaderp);
}

//virtual
void LLDrawPoolGlow::renderPostDeferred(S32 pass)
{
	render(&gDeferredEmissiveProgram);
}

void LLDrawPoolGlow::render(LLGLSLShader* shaderp)
{
	LL_FAST_TIMER(FTM_RENDER_GLOW);

	LLGLEnable blend(GL_BLEND);
	gGL.flush();
	// Get rid of Z-fighting with non-glow pass.
	LLGLEnable poly_offset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.f, -1.f);
	gGL.setSceneBlendType(LLRender::BT_ADD);

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);
	gGL.setColorMask(false, true);

	// Note: mask ignored by the PBR renderer
	U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;

	// First pass: static objects
	setup_glow_shader(shaderp);
	pushBatches(PASS_GLOW, mask, true, true);

	// Second pass: rigged objects
	if (shaderp->mRiggedVariant)	// Paranoia
	{
		setup_glow_shader(shaderp->mRiggedVariant);
		pushRiggedBatches(PASS_GLOW_RIGGED, mask, true, true);
	}

	gGL.setColorMask(true, false);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}
