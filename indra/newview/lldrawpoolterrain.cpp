/**
 * @file lldrawpoolterrain.cpp
 * @brief LLDrawPoolTerrain class implementation
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

#include "lldrawpoolterrain.h"

#include "imageids.h"
#include "llfasttimer.h"
#include "llrender.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llface.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llsky.h"
#include "llsurface.h"
#include "llsurfacepatch.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"			// For gRenderParcelOwnership
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"		// To get alpha gradients
#include "llvlcomposition.h"
#include "llvosurfacepatch.h"
#include "llworld.h"

constexpr F32 DETAIL_SCALE = 1.f / 16.f;
int DebugDetailMap = 0;

S32 LLDrawPoolTerrain::sDetailMode = 1;
F32 LLDrawPoolTerrain::sDetailScale = DETAIL_SCALE;
static LLGLSLShader* sShader = NULL;

LLDrawPoolTerrain::LLDrawPoolTerrain(LLViewerTexture* texturep)
:	LLFacePool(POOL_TERRAIN),
	mTexturep(texturep)
{
	// *HACK
	static LLCachedControl<F32> terrain_scale(gSavedSettings,
											  "RenderTerrainScale");
	static LLCachedControl<S32> terrain_detail(gSavedSettings,
											   "RenderTerrainDetail");
	sDetailScale = 1.f / llmax(0.1f, (F32)terrain_scale);
	sDetailMode = llclamp((S32)terrain_detail, 0, 2);

	mAlphaRampImagep =
		LLViewerTextureManager::getFetchedTexture(IMG_ALPHA_GRAD);
	mAlphaRampImagep->setAddressMode(LLTexUnit::TAM_CLAMP);
#if 0
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bind(mAlphaRampImagep.get());
#endif

	m2DAlphaRampImagep =
		LLViewerTextureManager::getFetchedTexture(IMG_ALPHA_GRAD_2D);
	m2DAlphaRampImagep->setAddressMode(LLTexUnit::TAM_CLAMP);
#if 0
	unit0->bind(m2DAlphaRampImagep.get());
#endif

	if (mTexturep)
	{
		mTexturep->setBoostLevel(LLGLTexture::BOOST_TERRAIN);
	}

#if 0
	unit0->unbind(LLTexUnit::TT_TEXTURE);
#endif
}

//virtual
U32 LLDrawPoolTerrain::getVertexDataMask()
{
	if (LLPipeline::sShadowRender)
	{
		return LLVertexBuffer::MAP_VERTEX;
	}
	if (LLGLSLShader::sCurBoundShaderPtr)
	{
		return VERTEX_DATA_MASK & ~(LLVertexBuffer::MAP_TEXCOORD2 |
									LLVertexBuffer::MAP_TEXCOORD3);
	}
	return VERTEX_DATA_MASK;
}

//virtual
void LLDrawPoolTerrain::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_ENVIRONMENT);
	static LLCachedControl<S32> terrain_detail(gSavedSettings,
											   "RenderTerrainDetail");
	sDetailMode = llclamp((S32)terrain_detail, 0, 2);
}

// For use by the EE renderer only
//virtual
void LLDrawPoolTerrain::beginRenderPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_TERRAIN);

	sShader = LLPipeline::sUnderWaterRender ? &gTerrainWaterProgram
											: &gTerrainProgram;
	if (mShaderLevel > 1 && sShader->mShaderLevel > 0)
	{
		sShader->bind();
	}
}

// For use by the EE renderer only
//virtual
void LLDrawPoolTerrain::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_TERRAIN);

	if (mShaderLevel > 1 && sShader->mShaderLevel > 0)
	{
		sShader->unbind();
	}
}

// For use by the EE renderer only
//virtual
void LLDrawPoolTerrain::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_TERRAIN);

	if (mDrawFace.empty())
	{
		return;
	}

	// HB. Useless to do it at every render pass...
	// FIXME: see to move this to LLVLComposition so to avoid re-doing it at
	// each frame !!!
	if (pass == 0)
	{
		boostTerrainDetailTextures();
	}

	LLOverrideFaceColor color_override(this, 1.f, 1.f, 1.f, 1.f);

	// Render simplified land if video card cannot do sufficient multitexturing
	if (gGLManager.mNumTextureImageUnits < 2)
	{
		renderSimple(); // Render without multitexture
		return;
	}

	LLGLSPipeline gls;

	if (mShaderLevel > 1 && sShader->mShaderLevel > 0)
	{
		gPipeline.enableLightsDynamic();
		renderFullShader();
	}
	else
	{
		gPipeline.enableLightsStatic();

		if (sDetailMode == 0)
		{
			renderSimple();
		}
		else if (gGLManager.mNumTextureImageUnits < 4)
		{
			renderFull2TU();
		}
		else
		{
			renderFull4TU();
		}
	}

	// Special case for land ownership feedback
	static LLCachedControl<bool> show_parcel_owners(gSavedSettings,
													"ShowParcelOwners");
	if (show_parcel_owners)
	{
		hilightParcelOwners();
	}
}

//virtual
void LLDrawPoolTerrain::beginDeferredPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_TERRAIN);

	if (!gUsePBRShaders && LLPipeline::sUnderWaterRender)
	{
		sShader = &gDeferredTerrainWaterProgram;
	}
	else
	{
		sShader = &gDeferredTerrainProgram;
	}
	sShader->bind();
}

//virtual
void LLDrawPoolTerrain::endDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_TERRAIN);
	LLFacePool::endRenderPass(pass);
	sShader->unbind();
}

//virtual
void LLDrawPoolTerrain::renderDeferred(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_TERRAIN);

	if (mDrawFace.empty())
	{
		return;
	}

	if (pass == 0 && gUsePBRShaders)
	{
		boostTerrainDetailTextures();
	}

	renderFullShader();

	// Special case for land ownership feedback
	static LLCachedControl<bool> show_parcel_owners(gSavedSettings,
													"ShowParcelOwners");
	if (show_parcel_owners)
	{
		hilightParcelOwners();
	}
}

//virtual
void LLDrawPoolTerrain::beginShadowPass(S32)
{
	LL_FAST_TIMER(FTM_SHADOW_TERRAIN);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gDeferredShadowProgram.bind();
	gDeferredShadowProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR,
									 gEnvironment.getIsSunUp() ? 1 : 0);
}

//virtual
void LLDrawPoolTerrain::endShadowPass(S32 pass)
{
	LL_FAST_TIMER(FTM_SHADOW_TERRAIN);
	LLFacePool::endRenderPass(pass);
	gDeferredShadowProgram.unbind();
}

//virtual
void LLDrawPoolTerrain::renderShadow(S32 pass)
{
	LL_FAST_TIMER(FTM_SHADOW_TERRAIN);
	if (mDrawFace.empty())
	{
		return;
	}
#if 0
	LLGLEnable offset(GL_POLYGON_OFFSET);
	glCullFace(GL_FRONT);
#endif
	drawLoop();
#if 0
	glCullFace(GL_BACK);
#endif
}

void LLDrawPoolTerrain::drawLoop()
{
	for (U32 i = 0, count = mDrawFace.size(); i < count; ++i)
	{
		LLFace* facep = mDrawFace[i];
		if (!facep) continue;	// Paranoia

		LLDrawable* drawablep = facep->getDrawable();
		if (!drawablep) continue;

		LLViewerRegion* regionp = drawablep->getRegion();
		if (!regionp) continue;

		LLMatrix4* model_matrix = &(regionp->mRenderMatrix);
		if (model_matrix != gGLLastMatrix)
		{
			llassert(gGL.getMatrixMode() == LLRender::MM_MODELVIEW);
			gGLLastMatrix = model_matrix;
			gGL.loadMatrix(gGLModelView);
			if (model_matrix)
			{
				gGL.multMatrix(model_matrix->getF32ptr());
			}
			++gPipeline.mMatrixOpCount;
		}

		facep->renderIndexed();
	}
}

void LLDrawPoolTerrain::renderFullShader()
{
//MK
	if (gRLenabled && gRLInterface.mContainsCamTextures)
	{
		renderSimple();
		return;
	}
//mk
	// *HACK: get the region that this draw pool is rendering from !
	LLViewerRegion* regionp =
		mDrawFace[0]->getDrawable()->getVObj()->getRegion();
	LLViewerRegion* agent_regionp = gAgent.getRegion();
	if (!regionp || !agent_regionp) return;	// Paranoia

	LLVLComposition* compp = regionp->getComposition();
	LLViewerTexture* detail_texture0p = compp->mDetailTextures[0];
	LLViewerTexture* detail_texture1p = compp->mDetailTextures[1];
	LLViewerTexture* detail_texture2p = compp->mDetailTextures[2];
	LLViewerTexture* detail_texture3p = compp->mDetailTextures[3];

	const F64 divisor = 1.0 / (F64)sDetailScale;
	LLVector3d region_origin_global = agent_regionp->getOriginGlobal();
	F32 offset_x = (F32)fmod(region_origin_global.mdV[VX], divisor) *
				   sDetailScale;
	F32 offset_y = (F32)fmod(region_origin_global.mdV[VY], divisor) *
				   sDetailScale;

	static LLVector4 tp0, tp1;
	tp0.set(sDetailScale, 0.f, 0.f, offset_x);
	tp1.set(0.f, sDetailScale, 0.f, offset_y);

	// Detail texture 0
	S32 detail0 = sShader->enableTexture(LLShaderMgr::TERRAIN_DETAIL0);
	LLTexUnit* unitdet0 = gGL.getTexUnit(detail0);
	unitdet0->bind(detail_texture0p);
	// *BUG: why LL's EEP viewer needs this while it ruins it for us ?
	//unitdet0->setTextureAddressMode(LLTexUnit::TAM_WRAP);
	unitdet0->activate();

	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(shader);

	shader->uniform4fv(LLShaderMgr::OBJECT_PLANE_S, 1, tp0.mV);
	shader->uniform4fv(LLShaderMgr::OBJECT_PLANE_T, 1, tp1.mV);

	// Detail texture 1
	S32 detail1 = sShader->enableTexture(LLShaderMgr::TERRAIN_DETAIL1);
	LLTexUnit* unitdet1 = gGL.getTexUnit(detail1);
	unitdet1->bind(detail_texture1p);
	// *BUG: why LL's EEP viewer needs this while it ruins it for us ?
	//unitdet1->setTextureAddressMode(LLTexUnit::TAM_WRAP);
	unitdet1->activate();

	S32 detail2 = sShader->enableTexture(LLShaderMgr::TERRAIN_DETAIL2);
	LLTexUnit* unitdet2 = gGL.getTexUnit(detail2);
	unitdet2->bind(detail_texture2p);
	// *BUG: why LL's EEP viewer needs this while it ruins it for us ?
	//unitdet2->setTextureAddressMode(LLTexUnit::TAM_WRAP);
	unitdet2->activate();

	S32 detail3 = sShader->enableTexture(LLShaderMgr::TERRAIN_DETAIL3);
	LLTexUnit* unitdet3 = gGL.getTexUnit(detail3);
	unitdet3->bind(detail_texture3p);
	// *BUG: why LL's EEP viewer needs this while it ruins it for us ?
	//unitdet3->setTextureAddressMode(LLTexUnit::TAM_WRAP);
	unitdet3->activate();

	S32 alpha_ramp = sShader->enableTexture(LLShaderMgr::TERRAIN_ALPHARAMP);
	LLTexUnit* unitalpha = gGL.getTexUnit(alpha_ramp);
	unitalpha->bind(m2DAlphaRampImagep);
	// *BUG: why LL's EEP viewer needs this while it ruins it for us ?
	//unitalpha->setTextureAddressMode(LLTexUnit::TAM_WRAP);
	unitalpha->activate();

	// GL_BLEND disabled by default
	drawLoop();

	// Disable multitexture
	sShader->disableTexture(LLShaderMgr::TERRAIN_ALPHARAMP);
	sShader->disableTexture(LLShaderMgr::TERRAIN_DETAIL0);
	sShader->disableTexture(LLShaderMgr::TERRAIN_DETAIL1);
	sShader->disableTexture(LLShaderMgr::TERRAIN_DETAIL2);
	sShader->disableTexture(LLShaderMgr::TERRAIN_DETAIL3);

	unitalpha->unbind(LLTexUnit::TT_TEXTURE);
	unitalpha->disable();
	unitalpha->activate();

	unitdet3->unbind(LLTexUnit::TT_TEXTURE);
	unitdet3->disable();
	unitdet3->activate();

	unitdet2->unbind(LLTexUnit::TT_TEXTURE);
	unitdet2->disable();
	unitdet2->activate();

	unitdet1->unbind(LLTexUnit::TT_TEXTURE);
	unitdet1->disable();
	unitdet1->activate();

	// Restore texture unit detail0 defaults
	unitdet0->unbind(LLTexUnit::TT_TEXTURE);
	unitdet0->enable(LLTexUnit::TT_TEXTURE);
	unitdet0->activate();
}

void LLDrawPoolTerrain::hilightParcelOwners()
{
	if (gUsePBRShaders || mShaderLevel > 1)
	{
		// Use fullbright shader for highlighting
		LLGLSLShader* old_shaderp = sShader;
		sShader->unbind();
		sShader = gUsePBRShaders ? &gDeferredHighlightProgram
								 : &gHighlightProgram;
		sShader->bind();
		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
		LLGLEnable polyOffset(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.f, -1.f);
		renderOwnership();
		sShader = old_shaderp;
		sShader->bind();
	}
	else
	{
		gPipeline.disableLights();
		renderOwnership();
	}
}

void LLDrawPoolTerrain::renderFull4TU()
{
//MK
	if (gRLenabled && gRLInterface.mContainsCamTextures)
	{
		renderSimple();
		return;
	}
//mk
	// *HACK: get the region that this draw pool is rendering from !
	LLViewerRegion* regionp =
		mDrawFace[0]->getDrawable()->getVObj()->getRegion();
	LLViewerRegion* agent_regionp = gAgent.getRegion();
	if (!regionp || !agent_regionp) return;	// Paranoia

	LLVLComposition* compp = regionp->getComposition();
	LLViewerTexture* detail_texture0p = compp->mDetailTextures[0];
	LLViewerTexture* detail_texture1p = compp->mDetailTextures[1];
	LLViewerTexture* detail_texture2p = compp->mDetailTextures[2];
	LLViewerTexture* detail_texture3p = compp->mDetailTextures[3];

	const F64 divisor = 1.0 / (F64)sDetailScale;
	LLVector3d region_origin_global = agent_regionp->getOriginGlobal();
	F32 offset_x = (F32)fmod(region_origin_global.mdV[VX], divisor) *
				   sDetailScale;
	F32 offset_y = (F32)fmod(region_origin_global.mdV[VY], divisor) *
				   sDetailScale;

	LLVector4 tp0, tp1;

	tp0.set(sDetailScale, 0.f, 0.f, offset_x);
	tp1.set(0.f, sDetailScale, 0.f, offset_y);

	gGL.blendFunc(LLRender::BF_ONE_MINUS_SOURCE_ALPHA,
				  LLRender::BF_SOURCE_ALPHA);

	//-------------------------------------------------------------------------
	// First pass

	//
	// Stage 0: detail texture 0
	//
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->activate();
	unit0->bind(detail_texture0p);

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	// Stage 1: generate alpha ramp for detail0/detail1 transition
	//
	LLTexUnit* unit1 = gGL.getTexUnit(1);
	unit1->bind(m2DAlphaRampImagep.get());
	unit1->enable(LLTexUnit::TT_TEXTURE);
	unit1->activate();

	//
	// Stage 2: Interpolate detail1 with existing based on ramp
	//
	LLTexUnit* unit2 = gGL.getTexUnit(2);
	unit2->bind(detail_texture1p);
	unit2->enable(LLTexUnit::TT_TEXTURE);
	unit2->activate();

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	//
	// Stage 3: Modulate with primary (vertex) color for lighting
	//
	LLTexUnit* unit3 = gGL.getTexUnit(3);
	unit3->bind(detail_texture1p);
	unit3->enable(LLTexUnit::TT_TEXTURE);
	unit3->activate();

	unit0->activate();

	// GL_BLEND disabled by default
	drawLoop();

	//-------------------------------------------------------------------------
	// Second pass

	// Stage 0: write detail3 into base
	//
	unit0->activate();
	unit0->bind(detail_texture3p);

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	//
	// Stage 1: generate alpha ramp for detail2/detail3 transition
	//
	unit1->bind(m2DAlphaRampImagep);
	unit1->enable(LLTexUnit::TT_TEXTURE);
	unit1->activate();

	// Set the texture matrix
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.translatef(-2.f, 0.f, 0.f);

	//
	// Stage 2: Interpolate detail2 with existing based on ramp
	//
	unit2->bind(detail_texture2p);
	unit2->enable(LLTexUnit::TT_TEXTURE);
	unit2->activate();

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	//
	// Stage 3: generate alpha ramp for detail1/detail2 transition
	//
	unit3->bind(m2DAlphaRampImagep);
	unit3->enable(LLTexUnit::TT_TEXTURE);
	unit3->activate();

	// Set the texture matrix
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.translatef(-1.f, 0.f, 0.f);
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	unit0->activate();
	{
		LLGLEnable blend(GL_BLEND);
		drawLoop();
	}

	LLVertexBuffer::unbind();
	// Disable multitexture
	unit3->unbind(LLTexUnit::TT_TEXTURE);
	unit3->disable();
	unit3->activate();

	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	unit2->unbind(LLTexUnit::TT_TEXTURE);
	unit2->disable();
	unit2->activate();

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	unit1->unbind(LLTexUnit::TT_TEXTURE);
	unit1->disable();
	unit1->activate();

	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	// Restore blend state
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	//-------------------------------------------------------------------------
	// Restore texture unit 0 defaults

	unit0->activate();
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

void LLDrawPoolTerrain::renderFull2TU()
{
//MK
	if (gRLenabled && gRLInterface.mContainsCamTextures)
	{
		renderSimple();
		return;
	}
//mk
	// *HACK: get the region that this draw pool is rendering from !
	LLViewerRegion* regionp =
		mDrawFace[0]->getDrawable()->getVObj()->getRegion();
	LLViewerRegion* agent_regionp = gAgent.getRegion();
	if (!regionp || !agent_regionp) return;	// Paranoia

	LLVLComposition* compp = regionp->getComposition();
	LLViewerTexture* detail_texture0p = compp->mDetailTextures[0];
	LLViewerTexture* detail_texture1p = compp->mDetailTextures[1];
	LLViewerTexture* detail_texture2p = compp->mDetailTextures[2];
	LLViewerTexture* detail_texture3p = compp->mDetailTextures[3];

	const F64 divisor = 1.0 / (F64)sDetailScale;
	LLVector3d region_origin_global = agent_regionp->getOriginGlobal();
	F32 offset_x = (F32)fmod(region_origin_global.mdV[VX], divisor) *
				   sDetailScale;
	F32 offset_y = (F32)fmod(region_origin_global.mdV[VY], divisor) *
				   sDetailScale;

	LLVector4 tp0, tp1;

	tp0.set(sDetailScale, 0.f, 0.f, offset_x);
	tp1.set(0.f, sDetailScale, 0.f, offset_y);

	gGL.blendFunc(LLRender::BF_ONE_MINUS_SOURCE_ALPHA,
				  LLRender::BF_SOURCE_ALPHA);

	//-------------------------------------------------------------------------
	// Pass 1/4

	//
	// Stage 0: render detail 0 into base
	//
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bind(detail_texture0p);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	drawLoop();

	//-------------------------------------------------------------------------
	// Pass 2/4

	//
	// Stage 0: generate alpha ramp for detail0/detail1 transition
	//
	unit0->bind(m2DAlphaRampImagep);

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);

	//
	// Stage 1: write detail1
	//
	LLTexUnit* unit1 = gGL.getTexUnit(1);
	unit1->bind(detail_texture1p);
	unit1->enable(LLTexUnit::TT_TEXTURE);
	unit1->activate();

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	unit0->activate();
	{
		LLGLEnable blend(GL_BLEND);
		drawLoop();
	}
	//-------------------------------------------------------------------------
	// Pass 3/4

	//
	// Stage 0: generate alpha ramp for detail1/detail2 transition
	//
	unit0->bind(m2DAlphaRampImagep);

	// Set the texture matrix
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.translatef(-1.f, 0.f, 0.f);
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	//
	// Stage 1: write detail2
	//
	unit1->bind(detail_texture2p);
	unit1->enable(LLTexUnit::TT_TEXTURE);
	unit1->activate();

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	{
		LLGLEnable blend(GL_BLEND);
		drawLoop();
	}

	//-------------------------------------------------------------------------
	// Pass 4/4

	//
	// Stage 0: generate alpha ramp for detail2/detail3 transition
	//
	unit0->activate();
	unit0->bind(m2DAlphaRampImagep);
	// Set the texture matrix
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.translatef(-2.f, 0.f, 0.f);
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	// Stage 1: write detail3
	unit1->bind(detail_texture3p);
	unit1->enable(LLTexUnit::TT_TEXTURE);
	unit1->activate();

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	unit0->activate();
	{
		LLGLEnable blend(GL_BLEND);
		drawLoop();
	}

	// Restore blend state
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	// Disable multitexture

	unit1->unbind(LLTexUnit::TT_TEXTURE);
	unit1->disable();
	unit1->activate();

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	//-------------------------------------------------------------------------
	// Restore texture unit 0 defaults

	unit0->activate();
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

void LLDrawPoolTerrain::renderSimple()
{
	LLVector4 tp0, tp1;
	LLVector3 origin_agent =
		mDrawFace[0]->getDrawable()->getVObj()->getRegion()->getOriginAgent();
	const F32 tscale = 1.f / 256.f;
	tp0.set(tscale, 0.f, 0.f, origin_agent.mV[0] * -tscale);
	tp1.set(0.f, tscale, 0.f, origin_agent.mV[1] * -tscale);

	//-------------------------------------------------------------------------
	// Pass 1/1

	// Stage 0: base terrain texture pass
	if (mTexturep)
	{
		mTexturep->addTextureStats(1024.f * 1024.f);
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->activate();
	unit0->enable(LLTexUnit::TT_TEXTURE);
//MK
	if (gRLenabled && gRLInterface.mContainsCamTextures &&
		gRLInterface.mCamTexturesCustom)
	{
		unit0->bind(gRLInterface.mCamTexturesCustom);
	}
	else
//mk
	if (mTexturep)
	{
		unit0->bind(mTexturep);
	}

	sShader->uniform4fv(LLShaderMgr::OBJECT_PLANE_S, 1, tp0.mV);
	sShader->uniform4fv(LLShaderMgr::OBJECT_PLANE_T, 1, tp1.mV);

	drawLoop();

	//-------------------------------------------------------------------------
	// Restore texture unit 0 defaults

	unit0->activate();
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

void LLDrawPoolTerrain::renderOwnership()
{
	LLGLSPipelineAlpha gls_pipeline_alpha;

	llassert(!mDrawFace.empty());

	// Each terrain pool is associated with a single region. We need to peek
	// back into the viewer's data to find out which ownership overlay texture
	// to use.
	LLFace* facep = mDrawFace[0];
	if (!facep) return;	// Paranoia
	LLDrawable* drawablep = facep->getDrawable();
	if (!drawablep) return;	// Paranoia
	const LLViewerObject* objectp = drawablep->getVObj();
	if (!objectp) return;	// Paranoia
	const LLVOSurfacePatch* vo_surface_patchp = (LLVOSurfacePatch*)objectp;
	LLSurfacePatch* surface_patchp = vo_surface_patchp->getPatch();
	if (!surface_patchp) return;	// Paranoia
	LLSurface* surfacep = surface_patchp->getSurface();
	if (!surfacep) return;	// Paranoia
	LLViewerRegion* regionp = surfacep->getRegion();
	if (!regionp) return;	// Paranoia
	LLViewerParcelOverlay* overlayp = regionp->getParcelOverlay();
	if (!overlayp) return;	// Paranoia
	LLViewerTexture* texturep = overlayp->getTexture();
	if (!texturep) return;	// Paranoia

	gGL.getTexUnit(0)->bind(texturep);

	// *NOTE: because the region is 256 meters wide, but has 257 pixels, the
	// texture coordinates for pixel 256x256 is not 1, 1. This makes the
	// ownership map not line up with the selection. We address this with
	// a texture matrix multiply.
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.pushMatrix();

	constexpr F32 TEXTURE_FUDGE = 257.f / 256.f;
	gGL.scalef(TEXTURE_FUDGE, TEXTURE_FUDGE, 1.f);
	for (U32 i = 0, count = mDrawFace.size(); i < count; ++i)
	{
		LLFace* facep = mDrawFace[i];
		if (facep)	// Paranoia
		{
			// Note: mask is ignored for the PBR renderer
			facep->renderIndexed(LLVertexBuffer::MAP_VERTEX |
								 LLVertexBuffer::MAP_TEXCOORD0);
		}
	}

	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

void LLDrawPoolTerrain::dirtyTextures(const LLViewerTextureList::dirty_list_t& textures)
{
	LLViewerFetchedTexture* texp =
		LLViewerTextureManager::staticCast(mTexturep);
	if (texp && textures.count(texp))
	{
		for (U32 i = 0, count = mReferences.size(); i < count; ++i)
		{
			LLFace* facep = mReferences[i];
			if (facep)	// Paranoia
			{
				gPipeline.markTextured(facep->getDrawable());
			}
		}
	}
}

void LLDrawPoolTerrain::boostTerrainDetailTextures()
{
	LLFace* facep = mDrawFace[0];
	if (!facep) return;		// Paranoia 1

	LLDrawable* drawablep =  mDrawFace[0]->getDrawable().get();
	if (!drawablep) return;	// Paranoia 2

	LLViewerObject* objectp = drawablep->getVObj().get();
	if (!objectp) return;	// Paranoia 3

	// *HACK: get the region that this draw pool is rendering from !
	LLViewerRegion* regionp = objectp->getRegion();
	if (!regionp) return;	// Paranoia 4

	LLVLComposition* compp = regionp->getComposition();
	if (!compp) return;		// Paranoia 5

	for (S32 i = 0; i < 4; ++i)
	{
		LLViewerFetchedTexture* texp = compp->mDetailTextures[i].get();
		if (texp)			// Paranoia 6
		{
			texp->setBoostLevel(LLGLTexture::BOOST_TERRAIN);
			// Assume large pixel area
			texp->addTextureStats(1024.f * 1024.f);
		}
	}
}

// Failed attempt at properly restoring terrain after GL restart with core GL
// profile enabled. HB
#if 0
void LLDrawPoolTerrain::rebuildPatches()
{
	for (U32 i = 0, count = mDrawFace.size(); i < count; ++i)
	{
		LLFace* facep = mDrawFace[i];
		if (!facep) continue;	// Paranoia
		LLDrawable* drawablep = facep->getDrawable();
		if (drawablep)			// Paranoia
		{
			gPipeline.markRebuild(drawablep);
			LLViewerObject* objectp = drawablep->getVObj();
			gPipeline.markGLRebuild(objectp);
			LL_DEBUGS("MarkGLRebuild") << "Marked for GL rebuild: " << std::hex
									   << (intptr_t)objectp << std::dec
									   << LL_ENDL;
		}
	}
}
#endif
