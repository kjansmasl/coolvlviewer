/**
 * @file lldrawpoolalpha.cpp
 * @brief LLDrawPoolAlpha class implementation
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

#include "lldrawpoolalpha.h"

#include "llcubemap.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "llmodel.h"
#include "llrender.h"

#include "lldrawable.h"
#include "llenvironment.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For gCubeSnapshot
#include "llviewerobjectlist.h" 	// For debugging
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"	// For debugging

constexpr F32 MINIMUM_ALPHA = 0.004f;	// ~ 1/255
constexpr F32 MINIMUM_IMPOSTOR_ALPHA = 0.1f;

static LLStaticHashedString sWaterSign("waterSign");

bool LLDrawPoolAlpha::sShowDebugAlpha = false;
static bool sDeferredRender = false;

// Helper functions

// EE renderer variant
static void prepare_alpha_shader(LLGLSLShader* shaderp, bool texture_gamma,
								 bool deferred)
{
	if (deferred)
	{
		gPipeline.bindDeferredShader(*shaderp);
	}
	else
	{
		shaderp->bind();
	}

	S32 no_atmo = 0;
	if (LLPipeline::sRenderingHUDs)
	{
		no_atmo = 1;
	}
	shaderp->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);

	F32 gamma = 1.f / LLPipeline::RenderDeferredDisplayGamma;
	shaderp->uniform1f(LLShaderMgr::DISPLAY_GAMMA, gamma);

	if (texture_gamma)
	{
		shaderp->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
	}

	if (LLPipeline::sImpostorRender)
	{
		shaderp->setMinimumAlpha(MINIMUM_IMPOSTOR_ALPHA);
	}
	else
	{
		shaderp->setMinimumAlpha(MINIMUM_ALPHA);
	}

	// Also prepare rigged variant
	if (shaderp->mRiggedVariant && shaderp->mRiggedVariant != shaderp)
	{
		prepare_alpha_shader(shaderp->mRiggedVariant, texture_gamma, deferred);
	}
}

// PBR renderer variant
static void prepare_alpha_shader(LLGLSLShader* shaderp, bool texture_gamma,
								 bool deferred, F32 water_sign)
{
	// Does this deferred shader need environment uniforms set such as sun_dir,
	// etc. ?  Note: we do not actually need a gbuffer since we are doing
	// forward rendering (for transparency) post deferred rendering.
	// *TODO: bindDeferredShader() probably should have the updating of the
	// environment uniforms factored out into updateShaderEnvironmentUniforms()
	// i.e. shaders/class1/deferred/alphaF.glsl.
	if (deferred)
	{
		shaderp->mCanBindFast = false;
	}

	shaderp->bind();

	F32 gamma = 1.f / LLPipeline::RenderDeferredDisplayGamma;
	shaderp->uniform1f(LLShaderMgr::DISPLAY_GAMMA, gamma);

	if (texture_gamma)
	{
		shaderp->uniform1f(LLShaderMgr::TEXTURE_GAMMA, 2.2f);
	}

	const LLVector4a* near_clip;
	if (LLPipeline::sRenderingHUDs)
	{
		// For HUD attachments, only the pre-water pass is executed and we
		// never want to clip anything.
		static const LLVector4a hud_near_clip(0.f, 0.f, -1.f, 0.f);
		water_sign = 1.f;
		near_clip = &hud_near_clip;
	}
	else
	{
		near_clip = &LLPipeline::sWaterPlane;
	}
	shaderp->uniform1f(sWaterSign, water_sign);
	shaderp->uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1,
						near_clip->getF32ptr());

	if (LLPipeline::sImpostorRender)
	{
		shaderp->setMinimumAlpha(MINIMUM_IMPOSTOR_ALPHA);
	}
	else
	{
		shaderp->setMinimumAlpha(MINIMUM_ALPHA);
	}

	// Also prepare rigged variant
	if (shaderp->mRiggedVariant && shaderp->mRiggedVariant != shaderp)
	{
		prepare_alpha_shader(shaderp->mRiggedVariant, texture_gamma, deferred,
							 water_sign);
	}
}

static void prepare_forward_shader(LLGLSLShader* shaderp, F32 minimum_alpha)
{
	shaderp->bind();
	shaderp->setMinimumAlpha(minimum_alpha);
	S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
	shaderp->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);

	// Also prepare rigged variant
	if (shaderp->mRiggedVariant && shaderp->mRiggedVariant != shaderp)
	{
		prepare_forward_shader(shaderp->mRiggedVariant, minimum_alpha);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolAlpha class
///////////////////////////////////////////////////////////////////////////////

LLDrawPoolAlpha::LLDrawPoolAlpha(U32 type)
:	LLRenderPass(type),
	mTargetShader(NULL),
	mSimpleShader(NULL),
	mFullbrightShader(NULL),
	mEmissiveShader(NULL),
	mPBRShader(NULL),
	mPBREmissiveShader(NULL),
	mColorSFactor(LLRender::BF_UNDEF),
	mColorDFactor(LLRender::BF_UNDEF),
	mAlphaSFactor(LLRender::BF_UNDEF),
	mAlphaDFactor(LLRender::BF_UNDEF)
{
}

//virtual
void LLDrawPoolAlpha::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

//virtual
void LLDrawPoolAlpha::renderPostDeferred(S32 pass)
{
	if (gUsePBRShaders)
	{
		renderPostDeferredPBR(pass);
		return;
	}

	sDeferredRender = true;

	// Prepare shaders
	bool impostors = LLPipeline::sImpostorRender;
	if (impostors)
	{
		mSimpleShader = &gDeferredAlphaImpostorProgram;
		mFullbrightShader = &gDeferredFullbrightProgram;
		mEmissiveShader = &gObjectEmissiveProgram;
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		mSimpleShader = &gDeferredAlphaWaterProgram;
		mFullbrightShader = &gDeferredFullbrightWaterProgram;
		mEmissiveShader = &gObjectEmissiveWaterProgram;
	}
	else
	{
		mSimpleShader = &gDeferredAlphaProgram;
		mFullbrightShader = &gDeferredFullbrightProgram;
		mEmissiveShader = &gObjectEmissiveProgram;
	}
	prepare_alpha_shader(mEmissiveShader, true, false);
	prepare_alpha_shader(mFullbrightShader, true, false);
	// Prime simple shader (loads shadow relevant uniforms)
	prepare_alpha_shader(mSimpleShader, false, true);

	LLGLSLShader* shader_array = LLPipeline::sUnderWaterRender ?
		gDeferredMaterialWaterProgram : gDeferredMaterialProgram;
	for (U32 i = 0; i < LLMaterial::SHADER_COUNT; ++i)
	{
		prepare_alpha_shader(&shader_array[i], false, false);
	}

	// First pass, render rigged objects only and drawn to depth buffer
	forwardRender(true);

	// Second pass, regular forward alpha rendering
	forwardRender();

	// Final pass, render to depth for depth of field effects
	if (!impostors && LLPipeline::RenderDepthOfField)
	{
		// Update depth buffer sampler
		gPipeline.mRT->mScreen.flush();
		LLRenderTarget& depth_rt = gPipeline.mDeferredDepth;
		LLRenderTarget& dscr_rt = gPipeline.mRT->mDeferredScreen;
		depth_rt.copyContents(dscr_rt,
							  0, 0, dscr_rt.getWidth(), dscr_rt.getHeight(),
							  0, 0, depth_rt.getWidth(), depth_rt.getHeight(),
							  GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		depth_rt.bindTarget();
		mSimpleShader = mFullbrightShader = &gObjectFullbrightAlphaMaskProgram;
		mSimpleShader->bind();
		mSimpleShader->setMinimumAlpha(0.33f);

		// Mask off color buffer writes as we are only writing to depth buffer
		gGL.setColorMask(false, false);

		constexpr U32 MIN_MASK = LLVertexBuffer::MAP_TEXTURE_INDEX |
								 LLVertexBuffer::MAP_TANGENT |
								 LLVertexBuffer::MAP_TEXCOORD1 |
								 LLVertexBuffer::MAP_TEXCOORD2;
		// If the face is more than 90% transparent, then do not update the
		// depth buffer for Dof since we not want nearly invisible objects to
		// cause DoF effects. Thus the 'true' below.
		renderAlpha(getVertexDataMask() | MIN_MASK, true);

		depth_rt.flush();
		gPipeline.mRT->mScreen.bindTarget();
		gGL.setColorMask(true, false);
	}

	sDeferredRender = false;
}

// Branched version for the PBR renderer
void LLDrawPoolAlpha::renderPostDeferredPBR(S32 pass)
{
	bool is_pre_water = mType == LLDrawPool::POOL_ALPHA_PRE_WATER;
	if (is_pre_water && LLPipeline::isWaterClip())
	{
		// Do not render alpha objects on the other side of the water plane if
		// water is opaque.
		return;
	}

	F32 water_sign = is_pre_water ? -1.f : 1.f;
	if (LLPipeline::sUnderWaterRender)
	{
		water_sign = -water_sign;
	}

	// Prepare shaders

	mEmissiveShader = &gDeferredEmissiveProgram;
	prepare_alpha_shader(mEmissiveShader, true, false, water_sign);

	mPBREmissiveShader = &gPBRGlowProgram;
	prepare_alpha_shader(mPBREmissiveShader, true, false, water_sign);

	bool impostors = LLPipeline::sImpostorRender;
	bool huds = LLPipeline::sRenderingHUDs;
	if (impostors)
	{
		mFullbrightShader = &gDeferredFullbrightAlphaMaskProgram;
		mSimpleShader = &gDeferredAlphaImpostorProgram;
		mPBRShader = &gDeferredPBRAlphaProgram;
	}
	else if (huds)
	{
		mFullbrightShader = &gHUDFullbrightAlphaMaskAlphaProgram;
		mSimpleShader = &gHUDAlphaProgram;
		mPBRShader = &gHUDPBRAlphaProgram;
	}
	else
	{
		mFullbrightShader = &gDeferredFullbrightAlphaMaskAlphaProgram;
		mSimpleShader = &gDeferredAlphaProgram;
		mPBRShader = &gDeferredPBRAlphaProgram;
	}
	prepare_alpha_shader(mFullbrightShader, true, true, water_sign);
	prepare_alpha_shader(mSimpleShader, false, true, water_sign);

	LLGLSLShader* mat_shaderp = gDeferredMaterialProgram;
	for (U32 i = 0; i < LLMaterial::SHADER_COUNT * 2; ++i)
	{
		prepare_alpha_shader(&mat_shaderp[i], false, true, water_sign);
	}

    prepare_alpha_shader(mPBRShader, false, true, water_sign);

	// Explicitly unbind here so render loop does not make assumptions about
	// the last shader already being setup for rendering.
	LLGLSLShader::unbind();

	if (!huds)
	{
		// First pass, render rigged objects only and drawn to depth buffer
		forwardRender(true);
	}

	// Second pass, regular forward alpha rendering.
	forwardRender();

	// Final pass, render to depth for depth of field effects
	if (!huds && !impostors && LLPipeline::RenderDepthOfField &&
		!gCubeSnapshot && mType == LLDrawPool::POOL_ALPHA_POST_WATER)
	{
		// Update depth buffer sampler
		mSimpleShader = mFullbrightShader =
						&gDeferredFullbrightAlphaMaskProgram;
		mSimpleShader->bind();
		mSimpleShader->setMinimumAlpha(0.33f);

		// Mask off color buffer writes as we are only writing to depth buffer
		gGL.setColorMask(false, false);

		constexpr U32 MIN_MASK = LLVertexBuffer::MAP_TEXTURE_INDEX |
								 LLVertexBuffer::MAP_TANGENT |
								 LLVertexBuffer::MAP_TEXCOORD1 |
								 LLVertexBuffer::MAP_TEXCOORD2;
		// If the face is more than 90% transparent, then do not update the
		// depth buffer for Dof since we not want nearly invisible objects to
		// cause DoF effects. Thus the 'true' below.
		renderAlpha(getVertexDataMask() | MIN_MASK, true);

		gGL.setColorMask(true, false);
	}
}

// Only for the EE renderer
//virtual
void LLDrawPoolAlpha::render(S32 pass)
{
	F32 minimum_alpha = 0.f;
	if (LLPipeline::sImpostorRender)
	{
		minimum_alpha = 0.5f;
		mSimpleShader = &gObjectSimpleImpostorProgram;
		mFullbrightShader = &gObjectFullbrightProgram;
		mEmissiveShader = &gObjectEmissiveProgram;
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		mSimpleShader = &gObjectSimpleWaterProgram;
		mFullbrightShader = &gObjectFullbrightWaterProgram;
		mEmissiveShader = &gObjectEmissiveWaterProgram;
	}
	else
	{
		mSimpleShader = &gObjectSimpleProgram;
		mFullbrightShader = &gObjectFullbrightProgram;
		mEmissiveShader = &gObjectEmissiveProgram;
	}

	 prepare_forward_shader(mFullbrightShader, minimum_alpha);
	 prepare_forward_shader(mSimpleShader, minimum_alpha);

	LLGLSLShader* shader_array = LLPipeline::sUnderWaterRender ?
		gDeferredMaterialWaterProgram : gDeferredMaterialProgram;
	for (U32 i = 0; i < LLMaterial::SHADER_COUNT; ++i)
	{
		prepare_alpha_shader(&shader_array[i], false, false);
	}

	// First pass, render rigged objects only and drawn to depth buffer
	forwardRender(true);

	// Second pass, non-rigged, no depth buffer writes
	forwardRender();
}

void LLDrawPoolAlpha::forwardRender(bool rigged)
{
	gPipeline.enableLightsDynamic();
	LLGLSPipelineAlpha gls_pipeline_alpha;

	// Enable writing to alpha for emissive effects
	gGL.setColorMask(true, true);

	bool write_depth = rigged || LLPipeline::sImpostorRenderAlphaDepthPass;
	if (!write_depth && gUsePBRShaders)
	{
		// Needed for accurate water fog
		write_depth = mType == LLDrawPoolAlpha::POOL_ALPHA_PRE_WATER;
	}
	LLGLDepthTest depth(GL_TRUE, write_depth ? GL_TRUE : GL_FALSE);

	// Regular alpha blend
	mColorSFactor = LLRender::BF_SOURCE_ALPHA;
	mColorDFactor = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
	// Glow suppression
	mAlphaSFactor = LLRender::BF_ZERO;
	mAlphaDFactor = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
	gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor, mAlphaDFactor);

	constexpr U32 MIN_MASK = LLVertexBuffer::MAP_TEXTURE_INDEX |
							 LLVertexBuffer::MAP_TANGENT |
							 LLVertexBuffer::MAP_TEXCOORD1 |
							 LLVertexBuffer::MAP_TEXCOORD2;
	renderAlpha(getVertexDataMask() | MIN_MASK, false, rigged);

	gGL.setColorMask(true, false);

	if (!rigged && sShowDebugAlpha)
	{
		// Render "highlight alpha" on final non-rigged pass.
		// *HACK: this call is protected by !rigged instead of alongside
		// forwardRender() so that renderDebugAlpha is executed while
		// gls_pipeline_alpha and depth GL state variables above are still in
		// scope.
		renderDebugAlpha();
	}
}

void LLDrawPoolAlpha::renderDebugAlpha()
{
	gHighlightProgram.bind();
	gGL.getTexUnit(0)->bindFast(LLViewerFetchedTexture::sSmokeImagep);

	if (gUsePBRShaders)
	{
		// Changed alpha from 1.f to 0.8f to avoid opaque highlighted textures
		// and get something closer to highlights seen in EE mode. HB
		constexpr F32 alpha = 0.8f;

		// Highlight (semi) transparent faces
		gGL.diffuseColor4f(1.f, 0.f, 0.f, alpha);
		renderAlphaHighlight();

		pushUntexturedBatches(PASS_ALPHA_MASK);
		pushUntexturedBatches(PASS_ALPHA_INVISIBLE);

		// Highlight alpha masking textures in blue
		gGL.diffuseColor4f(0.f, 0.f, 1.f, alpha);
		pushUntexturedBatches(PASS_MATERIAL_ALPHA_MASK);
		pushUntexturedBatches(PASS_NORMMAP_MASK);
		pushUntexturedBatches(PASS_SPECMAP_MASK);
		pushUntexturedBatches(PASS_NORMSPEC_MASK);
		pushUntexturedBatches(PASS_FULLBRIGHT_ALPHA_MASK);
		pushUntexturedBatches(PASS_MAT_PBR_ALPHA_MASK);

		// Highlight invisible faces in green
		gGL.diffuseColor4f(0.f, 1.f, 0.f, alpha);
		pushUntexturedBatches(PASS_INVISIBLE);

		// Bind the rigged shadder variant
		gHighlightProgram.mRiggedVariant->bind();
		
		// Highlight (semi) transparent faces
		gGL.diffuseColor4f(1.f, 0.f, 0.f, alpha);
		pushRiggedBatches(PASS_ALPHA_MASK_RIGGED, false);
		pushRiggedBatches(PASS_ALPHA_INVISIBLE_RIGGED, false);

		// Highlight alpha masking textures in blue
		gGL.diffuseColor4f(0.f, 0.f, 1.f, alpha);
		pushRiggedBatches(PASS_MATERIAL_ALPHA_MASK_RIGGED, false);
		pushRiggedBatches(PASS_NORMMAP_MASK_RIGGED, false);
		pushRiggedBatches(PASS_SPECMAP_MASK_RIGGED, false);
		pushRiggedBatches(PASS_NORMSPEC_MASK_RIGGED, false);
		pushRiggedBatches(PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, false);
		pushRiggedBatches(PASS_MAT_PBR_ALPHA_MASK_RIGGED, false);

		// Highlight invisible faces in green
		gGL.diffuseColor4f(0.f, 1.f, 0.f, alpha);
		pushRiggedBatches(PASS_INVISIBLE_RIGGED, false);

		LLGLSLShader::sCurBoundShaderPtr->unbind();
		return;
	}

	constexpr U32 mask = LLVertexBuffer::MAP_VERTEX |
						 LLVertexBuffer::MAP_TEXCOORD0;

	// Highlight (semi) transparent faces
	gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f);
	renderAlphaHighlight(mask);
	pushBatches(PASS_ALPHA_MASK, mask, false);
	pushBatches(PASS_ALPHA_INVISIBLE, mask, false);
	pushBatches(PASS_FULLBRIGHT_ALPHA_MASK, mask, false);

	// Highlight invisible faces in green
	gGL.diffuseColor4f(0.f, 1.f, 0.f, 1.f);
	pushBatches(PASS_INVISIBLE, mask, false);

	if (LLPipeline::sRenderDeferred)
	{
		// Highlight alpha masking textures in blue when in deferred rendering
		// mode.
		gGL.diffuseColor4f(0.f, 0.f, 1.f, 1.f);
		pushBatches(PASS_MATERIAL_ALPHA_MASK, mask, false);
		pushBatches(PASS_NORMMAP_MASK, mask, false);
		pushBatches(PASS_SPECMAP_MASK, mask, false);
		pushBatches(PASS_NORMSPEC_MASK, mask, false);
	}

	// Rigged variants now...
	gHighlightProgram.mRiggedVariant->bind();

	// Highlight (semi) transparent faces
	gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f);
	pushRiggedBatches(PASS_ALPHA_MASK_RIGGED, mask, false);
	pushRiggedBatches(PASS_ALPHA_INVISIBLE_RIGGED, mask, false);
	pushRiggedBatches(PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, mask, false);

	// Highlight invisible faces in green
	gGL.diffuseColor4f(0.f, 1.f, 0.f, 1.f);
	pushRiggedBatches(PASS_INVISIBLE_RIGGED, mask, false);

	if (LLPipeline::sRenderDeferred)
	{
		// Highlight alpha masking textures in blue when in deferred rendering
		// mode.
		gGL.diffuseColor4f(0.f, 0.f, 1.f, 1.f);
		pushRiggedBatches(PASS_MATERIAL_ALPHA_MASK_RIGGED, mask, false);
		pushRiggedBatches(PASS_NORMMAP_MASK_RIGGED, mask, false);
		pushRiggedBatches(PASS_SPECMAP_MASK_RIGGED, mask, false);
		pushRiggedBatches(PASS_NORMSPEC_MASK_RIGGED, mask, false);
	}

	LLGLSLShader::sCurBoundShaderPtr->unbind();
}

void LLDrawPoolAlpha::renderAlphaHighlight(U32 mask)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for get*AlphaGroups())
		return;
	}

	// Two passes: one not rigged and one rigged.
	for (U32 pass = 0; pass < 2; ++pass)
	{
		LLVOAvatar* last_avatarp = NULL;
		U64 last_hash = 0;

		LLCullResult::sg_list_t& alpha_groups =
			pass ? gPipeline.getRiggedAlphaGroups()
				 : gPipeline.getAlphaGroups();
		for (U32 i = 0, acount = alpha_groups.size(); i < acount; ++i)
		{
			LLSpatialGroup* groupp = alpha_groups[i];
			if (!groupp || groupp->isDead()) continue;

			LLSpatialPartition* partp = groupp->getSpatialPartition();
			if (!partp || !partp->mRenderByGroup) continue;

			// Use 'pass' to point on PASS_ALPHA_RIGGED on second pass
			LLSpatialGroup::drawmap_elem_t& draw_info =
				groupp->mDrawMap[PASS_ALPHA + pass];

			for (U32 j = 0, dcount = draw_info.size(); j < dcount; ++j)
			{
				LLDrawInfo& params = *draw_info[j];

				if (!params.mVertexBuffer)
				{
					continue;
				}

				bool rigged = params.mAvatar.notNull();
				gHighlightProgram.bind(rigged);

				if (rigged && params.mSkinInfo.notNull() &&
					(params.mAvatar.get() != last_avatarp ||
					 params.mSkinInfo->mHash != last_hash))
				{
					if (!uploadMatrixPalette(params))
					{
						continue;
					}
					last_avatarp = params.mAvatar.get();
					last_hash = params.mSkinInfo->mHash;
				}

				LLRenderPass::applyModelMatrix(params);

				// Note: mask is ignored for the PBR renderer
				params.mVertexBuffer->setBufferFast(mask);
				params.mVertexBuffer->drawRangeFast(params.mStart, params.mEnd,
													params.mCount,
													params.mOffset);
			}
			// Add weights to the mask for the second, rigged pass
			mask |= LLVertexBuffer::MAP_WEIGHT4;
		}
	}

	// Make sure static version of highlight shader is bound before returning
	gHighlightProgram.bind();
}

bool LLDrawPoolAlpha::texSetup(LLDrawInfo* drawp, bool use_material,
							   LLTexUnit* unitp)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_TEX_BINDS);

	if (gUsePBRShaders && drawp->mGLTFMaterial)
	{
		if (drawp->mTextureMatrix)
		{
			unitp->activate();
			gGL.matrixMode(LLRender::MM_TEXTURE);
			gGL.loadMatrix(drawp->mTextureMatrix->getF32ptr());
			++gPipeline.mTextureMatrixOps;
			return true;
		}
		return false;
	}

	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;

	bool tex_setup = false;

	bool render_mat = use_material && shaderp;
	if (render_mat)
	{
		render_mat = gUsePBRShaders ? !LLPipeline::sRenderingHUDs
									: sDeferredRender;
	}
	if (render_mat)
	{
		LL_TRACY_TIMER(TRC_RENDER_ALPHA_DEFERRED_TEX_BINDS);
		if (drawp->mNormalMap)
		{
			drawp->mNormalMap->addTextureStats(drawp->mVSize);
			shaderp->bindTexture(LLShaderMgr::BUMP_MAP, drawp->mNormalMap);
		}

		if (drawp->mSpecularMap)
		{
			drawp->mSpecularMap->addTextureStats(drawp->mVSize);
			shaderp->bindTexture(LLShaderMgr::SPECULAR_MAP,
								 drawp->mSpecularMap);
		}
	}
	else if (shaderp == mSimpleShader ||
			 shaderp == mSimpleShader->mRiggedVariant)
	{
		shaderp->bindTexture(LLShaderMgr::BUMP_MAP,
							 LLViewerFetchedTexture::sFlatNormalImagep);
		shaderp->bindTexture(LLShaderMgr::SPECULAR_MAP,
							 LLViewerFetchedTexture::sWhiteImagep);
	}
	U32 count = drawp->mTextureList.size();
	if (count > LL_NUM_TEXTURE_LAYERS)
	{
		llwarns << "We have only " << LL_NUM_TEXTURE_LAYERS
				<< " TexUnits and this batch contains " << count
				<< " textures. Rendering will be incomplete !" << llendl;
		count = LL_NUM_TEXTURE_LAYERS;
	}
	if (count > 1)
	{
		for (U32 i = 0; i < count; ++i)
		{
			LLViewerTexture* texp = drawp->mTextureList[i].get();
			if (texp)
			{
				gGL.getTexUnit(i)->bindFast(texp);
			}
		}
	}
	// Not batching textures or batch has only 1 texture; we might need a
	// texture matrix.
	else if (drawp->mTexture.notNull())
	{
		if (use_material)
		{
			shaderp->bindTexture(LLShaderMgr::DIFFUSE_MAP, drawp->mTexture);
		}
		else
		{
			unitp->bindFast(drawp->mTexture);
		}
		if (drawp->mTextureMatrix)
		{
			tex_setup = true;
			unitp->activate();
			gGL.matrixMode(LLRender::MM_TEXTURE);
			gGL.loadMatrix(drawp->mTextureMatrix->getF32ptr());
			++gPipeline.mTextureMatrixOps;
		}
	}
	else
	{
		unitp->unbindFast(LLTexUnit::TT_TEXTURE);
	}

	return tex_setup;
}

void LLDrawPoolAlpha::renderEmissives(U32 mask, const drawinfo_vec_t& ems)
{
	if (!mEmissiveShader) return;	// Paranoia

	mEmissiveShader->bind();
	mEmissiveShader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);

	// Note: mask is ignored for the PBR renderer
	mask = (mask & ~LLVertexBuffer::MAP_COLOR) | LLVertexBuffer::MAP_EMISSIVE;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	for (U32 i = 0, count = ems.size(); i < count; ++i)
	{
		LLDrawInfo* drawp = ems[i];
		bool tex_setup = texSetup(drawp, false, unit0);
		drawp->mVertexBuffer->setBufferFast(mask);
		drawp->mVertexBuffer->drawRangeFast(drawp->mStart, drawp->mEnd,
											drawp->mCount, drawp->mOffset);
		// Restore tex setup
		if (tex_setup)
		{
			unit0->activate();
			// Note: activate() did change matrix mode to MM_TEXTURE, so the
			// loadIdentity() call does apply to MM_TEXTURE. HB
			gGL.loadIdentity();
			gGL.matrixMode(LLRender::MM_MODELVIEW);
		}
	}
}

// PBR rendering only
void LLDrawPoolAlpha::renderPbrEmissives(const drawinfo_vec_t& ems)
{
	if (!mPBREmissiveShader) return;	// Paranoia

	mPBREmissiveShader->bind();

	for (U32 i = 0, count = ems.size(); i < count; ++i)
	{
		LLDrawInfo* drawp = ems[i];

		LLGLDisable cull_face(drawp->mGLTFMaterial->mDoubleSided ? GL_CULL_FACE
																 : 0);

		drawp->mGLTFMaterial->bind(drawp->mTexture, drawp->mVSize);
		drawp->mVertexBuffer->setBuffer();
		drawp->mVertexBuffer->drawRange(LLRender::TRIANGLES, drawp->mStart,
										drawp->mEnd, drawp->mCount,
										drawp->mOffset);
	}
}

void LLDrawPoolAlpha::renderRiggedEmissives(U32 mask,
											const drawinfo_vec_t& ems)
{
	LLGLSLShader* shaderp = mEmissiveShader->mRiggedVariant;
	if (!shaderp) return;	// Paranoia

	// Disable depth writes since "emissive" is additive so sorting does not
	// matter
	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	shaderp->bind();
	shaderp->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	// Note: mask is ignored for the PBR renderer
	mask = (mask & ~LLVertexBuffer::MAP_COLOR) | LLVertexBuffer::MAP_EMISSIVE |
		   LLVertexBuffer::MAP_WEIGHT4;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	for (U32 i = 0, count = ems.size(); i < count; ++i)
	{
		LLDrawInfo* drawp = ems[i];
		bool tex_setup = texSetup(drawp, false, unit0);
		if (drawp->mAvatar.notNull() && drawp->mSkinInfo.notNull() &&
			(drawp->mAvatar.get() != last_avatarp ||
			 drawp->mSkinInfo->mHash != last_hash))
		{
			if (!uploadMatrixPalette(*drawp))
			{
				continue;
			}
			last_avatarp = drawp->mAvatar.get();
			last_hash = drawp->mSkinInfo->mHash;
		}
		drawp->mVertexBuffer->setBufferFast(mask);
		drawp->mVertexBuffer->drawRangeFast(drawp->mStart, drawp->mEnd,
											drawp->mCount, drawp->mOffset);
		// Restore tex setup
		if (tex_setup)
		{
			unit0->activate();
			// Note: activate() did change matrix mode to MM_TEXTURE, so the
			// loadIdentity() call does apply to MM_TEXTURE. HB
			gGL.loadIdentity();
			gGL.matrixMode(LLRender::MM_MODELVIEW);
		}
	}
}

// PBR rendering only
void LLDrawPoolAlpha::renderRiggedPbrEmissives(const drawinfo_vec_t& ems)
{
	if (!mPBREmissiveShader) return;	// Paranoia

	// Disable depth writes since "emissive" is additive so sorting does not
	// matter
	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	mPBREmissiveShader->bind();

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	for (U32 i = 0, count = ems.size(); i < count; ++i)
	{
		LLDrawInfo* drawp = ems[i];
		if (drawp->mAvatar.notNull() && drawp->mSkinInfo.notNull() &&
			(drawp->mAvatar.get() != last_avatarp ||
			 drawp->mSkinInfo->mHash != last_hash))
		{
			if (!uploadMatrixPalette(*drawp))
			{
				continue;
			}
			last_avatarp = drawp->mAvatar.get();
			last_hash = drawp->mSkinInfo->mHash;
		}

		LLGLDisable cull_face(drawp->mGLTFMaterial->mDoubleSided ? GL_CULL_FACE
																 : 0);

		drawp->mGLTFMaterial->bind(drawp->mTexture, drawp->mVSize);
		drawp->mVertexBuffer->setBuffer();
		drawp->mVertexBuffer->drawRange(LLRender::TRIANGLES, drawp->mStart,
										drawp->mEnd, drawp->mCount,
										drawp->mOffset);
	}
}

static bool check_vb_mask(U32 mask, U32 expected_mask)
{
	U32 missing = expected_mask & ~mask;
	if (!missing)
	{
		return true;
	}

	if (gDebugGL)
	{
		llwarns << "Missing required components:"
				<< LLVertexBuffer::listMissingBits(missing) << llendl;
	}

	static LLCachedControl<bool> ignore(gSavedSettings,
										"RenderIgnoreBadVBMask");
	return ignore;
}

void LLDrawPoolAlpha::renderAlpha(U32 mask, bool depth_only, bool rigged)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for get*AlphaGroups())
		return;
	}

	bool initialized_lighting = false;
	bool light_enabled = true;
	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;
	LLGLSLShader* last_shaderp = NULL;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	static drawinfo_vec_t emissives, rigged_emissives;
	static drawinfo_vec_t pbr_emissives, pbr_rigged_emissives;

	// No shaders = no glow.
#if 1
	bool draw_glow = gUsePBRShaders || (!depth_only && mShaderLevel > 0);
#else
	bool draw_glow = mShaderLevel > 0;
#endif

	F32 water_height = 0.f;
	bool above_water = mType == POOL_ALPHA_POST_WATER;
	bool check_water = gUsePBRShaders && !LLPipeline::sRenderingHUDs;
	if (check_water)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			above_water = !above_water;
		}
		water_height = gPipeline.mWaterHeight;
	}
	bool is_pre_water = gUsePBRShaders && mType == POOL_ALPHA_PRE_WATER;

	bool underwater = LLPipeline::sUnderWaterRender && !gUsePBRShaders;

	LLCullResult::sg_list_t& alpha_groups =
		rigged ? gPipeline.getRiggedAlphaGroups() : gPipeline.getAlphaGroups();
	S32 map_idx = rigged ? PASS_ALPHA_RIGGED : PASS_ALPHA;
	for (U32 i = 0, acount = alpha_groups.size(); i < acount; ++i)
	{
		LLSpatialGroup* groupp = alpha_groups[i];
		if (!groupp || groupp->isDead())
		{
			continue;
		}

		LLSpatialPartition* partp = groupp->getSpatialPartition();
		if (!partp || !partp->mRenderByGroup)
		{
			llassert(partp);
			continue;
		}

		if (check_water)
		{
			LLSpatialBridge* bridgep = partp->asBridge();
			const LLVector4a* ext = bridgep ? bridgep->getSpatialExtents()
											: groupp->getExtents();
			if (above_water)
			{
				if (ext[1].getF32ptr()[2] < water_height)
				{
					// Reject spatial groups which have no part above water
					continue;
				}
			}
			else if (ext[0].getF32ptr()[2] > water_height)
			{
				// Reject spatial groups which have no part below water
				continue;
			}
		}

		emissives.clear();
		rigged_emissives.clear();
		pbr_emissives.clear();
		pbr_rigged_emissives.clear();

		U32 part_type = partp->mPartitionType;
		bool is_particle =
			part_type == LLViewerRegion::PARTITION_PARTICLE ||
			part_type == LLViewerRegion::PARTITION_HUD_PARTICLE ||
			part_type == LLViewerRegion::PARTITION_CLOUD;

		LLGLDisable cull(is_particle ? GL_CULL_FACE : 0);

		LLSpatialGroup::drawmap_elem_t& draw_info = groupp->mDrawMap[map_idx];

		for (U32 j = 0, dcount = draw_info.size(); j < dcount; ++j)
		{
			LLDrawInfo& params = *draw_info[j];
			if (bool(params.mAvatar) != rigged || !params.mVertexBuffer)
			{
				continue;
			}

			if (!gUsePBRShaders &&
				(!check_vb_mask(params.mVertexBuffer->getTypeMask(), mask)))
			{
				continue;
			}

			LLRenderPass::applyModelMatrix(params);

			LLMaterial* matp = NULL;
			LLFetchedGLTFMaterial* gltfp = NULL; 
			bool double_sided = false;
			bool gltf_alpha_blend = false;
			if (gUsePBRShaders)
			{
				gltfp = params.mGLTFMaterial.get();
				if (gltfp)
				{
					double_sided = gltfp->mDoubleSided;
					gltf_alpha_blend =
						gltfp->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND;
				}
				if (!gltf_alpha_blend && !LLPipeline::sRenderingHUDs)
				{
					matp = params.mMaterial.get();
				}
			}
			else if (sDeferredRender)
			{
				matp = params.mMaterial.get();
			}

			LLGLDisable cull_face(double_sided ? GL_CULL_FACE : 0);

			if (gltf_alpha_blend)
			{
				if (params.mAvatar.notNull())
				{
					mTargetShader = mPBRShader->mRiggedVariant;
				}
				else
				{
					mTargetShader = mPBRShader;
				}
				if (LLGLSLShader::sCurBoundShaderPtr != mTargetShader)
				{
					gPipeline.bindDeferredShaderFast(*mTargetShader);
				}
				gltfp->bind(params.mTexture, params.mVSize);
			}
			else
			{
				bool bind_deferred = gUsePBRShaders;

				if (params.mFullbright)
				{
					if (light_enabled || !initialized_lighting)
					{
						initialized_lighting = true;
						mTargetShader = mFullbrightShader;
						light_enabled = false;
					}
				}
				// Turn on lighting if it is not already.
				else if (!light_enabled || !initialized_lighting)
				{
					initialized_lighting = true;
					mTargetShader = mSimpleShader;
					light_enabled = true;
				}

				if (gUsePBRShaders && LLPipeline::sRenderingHUDs)
				{
					mTargetShader = mFullbrightShader;
				}
				else if (matp)
				{
					U32 mask = params.mShaderMask;
					llassert(mask < LLMaterial::SHADER_COUNT);
					if (underwater)
					{
						mTargetShader = &gDeferredMaterialWaterProgram[mask];
					}
					else
					{
						mTargetShader = &gDeferredMaterialProgram[mask];
					}
					bind_deferred = true;
				}
				else if (params.mFullbright)
				{
					mTargetShader = mFullbrightShader;
				}
				else
				{
					mTargetShader = mSimpleShader;
				}

				if (params.mAvatar.notNull() && mTargetShader->mRiggedVariant)
				{
					mTargetShader = mTargetShader->mRiggedVariant;
				}

				// If we are not ALREADY using the proper shader, then bind it
				// (this way we do not rebind shaders unnecessarily).
				bool needs_binding =
					LLGLSLShader::sCurBoundShaderPtr != mTargetShader;
				if (!bind_deferred)		// EE mode only, for non-materials. HB
				{
					if (needs_binding)
					{
						mTargetShader->bind();
					}
				}
				else if (needs_binding)
				{
					gPipeline.bindDeferredShaderFast(*mTargetShader);
					if (gUsePBRShaders && params.mFullbright)
					{
						// Make sure the bind the exposure map for fullbright
						// shaders so they can cancel out exposure.
						S32 chan =
							mTargetShader->enableTexture(LLShaderMgr::EXPOSURE_MAP);
						if (chan > -1)
						{
							gGL.getTexUnit(chan)->bind(&gPipeline.mExposureMap);
						}
					}
				}

				LLVector4 spec_color(1.f, 1.f, 1.f, 1.f);
				F32 env_intensity = 0.f;
				F32 brightness = 1.f;
				// If we have a material, supply the appropriate data here.
				if (matp)
				{
					spec_color = params.mSpecColor;
					env_intensity = params.mEnvIntensity;
					brightness = params.mFullbright ? 1.f : 0.f;
				}
				mTargetShader->uniform4f(LLShaderMgr::SPECULAR_COLOR,
										 spec_color.mV[0], spec_color.mV[1],
										 spec_color.mV[2], spec_color.mV[3]);
				mTargetShader->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY,
										 env_intensity);
				mTargetShader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS,
										 brightness);
			}

			if (params.mAvatar.notNull() && params.mSkinInfo.notNull() &&
				(params.mAvatar.get() != last_avatarp ||
				 params.mSkinInfo->mHash != last_hash ||
				 last_shaderp != mTargetShader))
			{
				if (!uploadMatrixPalette(params))
				{
					continue;
				}
				last_avatarp = params.mAvatar.get();
				last_hash = params.mSkinInfo->mHash;
				last_shaderp = mTargetShader;
			}

			bool tex_setup = texSetup(&params, matp != NULL, unit0);
			{
				LL_TRACY_TIMER(TRC_RENDER_ALPHA_DRAW);
				gGL.blendFunc(params.mBlendFuncSrc, params.mBlendFuncDst,
							  mAlphaSFactor, mAlphaDFactor);
				bool reset_minimum_alpha = false;
				if (!LLPipeline::sImpostorRender &&
					params.mBlendFuncDst != LLRender::BF_SOURCE_ALPHA &&
					params.mBlendFuncSrc != LLRender::BF_SOURCE_ALPHA)
				{
					// This draw call has a custom blend function that may
					// require rendering of "invisible" fragments
					mTargetShader->setMinimumAlpha(0.f);
					reset_minimum_alpha = true;
				}
				if (gUsePBRShaders)
				{
					params.mVertexBuffer->setBuffer();
					params.mVertexBuffer->drawRange(LLRender::TRIANGLES,
													params.mStart, params.mEnd,
													params.mCount,
													params.mOffset);
				}
				else
				{
					U32 draw_mask = mask;
					if (params.mFullbright)
					{
						constexpr U32 FB_MASK =
							~(LLVertexBuffer::MAP_TANGENT |
							  LLVertexBuffer::MAP_TEXCOORD1 |
							  LLVertexBuffer::MAP_TEXCOORD2);
						draw_mask &= FB_MASK;
					}
					if (params.mAvatar.notNull())
					{
						draw_mask |= LLVertexBuffer::MAP_WEIGHT4;
					}
					params.mVertexBuffer->setBufferFast(draw_mask);
					params.mVertexBuffer->drawRangeFast(params.mStart,
														params.mEnd,
														params.mCount,
														params.mOffset);
				}
				if (reset_minimum_alpha)
				{
					mTargetShader->setMinimumAlpha(MINIMUM_ALPHA);
				}
			}

			// If this alpha mesh has glow, then draw it a second time to add
			// the destination-alpha (=glow). Interleaving these state-changing
			// calls is expensive, but glow must be drawn Z-sorted with alpha.
			if (draw_glow && (!is_particle || params.mHasGlow) &&
				!is_pre_water &&
				params.mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE))
			{
				LL_TRACY_TIMER(TRC_RENDER_ALPHA_EMISSIVE);
				if (params.mAvatar.notNull())
				{
					if (gltfp)
					{
						pbr_rigged_emissives.push_back(&params);
					}
					else
					{
						rigged_emissives.push_back(&params);
					}
				}
				else if (gltfp)
				{
					pbr_emissives.push_back(&params);
				}
				else
				{
					emissives.push_back(&params);
				}
			}

			// Restore tex setup
			if (tex_setup)
			{
				unit0->activate();
				// Note: activate() did change matrix mode to MM_TEXTURE, so
				// the loadIdentity() call does apply to MM_TEXTURE. HB
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
			}
		}

		if (!depth_only)
		{
			gPipeline.enableLightsDynamic();

			// Install glow-accumulating blend mode and do not touch color, but
			// add to alpha (glow).
			gGL.blendFunc(LLRender::BF_ZERO, LLRender::BF_ONE,
						  LLRender::BF_ONE, LLRender::BF_ONE);

			bool rebind = false;
			LLGLSLShader* last_shaderp = LLGLSLShader::sCurBoundShaderPtr;
			if (!emissives.empty())
			{
				light_enabled = true;
				renderEmissives(mask, emissives);
				rebind = true;
			}
			if (!pbr_emissives.empty())
			{
				light_enabled = true;
				renderPbrEmissives(pbr_emissives);
				rebind = true;
			}
			if (!rigged_emissives.empty())
			{
				light_enabled = false;
				renderRiggedEmissives(mask, rigged_emissives);
				rebind = true;
			}
			if (!pbr_rigged_emissives.empty())
			{
				light_enabled = true;
				renderRiggedPbrEmissives(pbr_rigged_emissives);
				rebind = true;
			}
			// Restore our alpha blend mode
			gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor,
						  mAlphaDFactor);
			if (rebind && last_shaderp)
			{
				last_shaderp->bind();
			}
		}
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	LLVertexBuffer::unbind();

	if (!light_enabled)
	{
		gPipeline.enableLightsDynamic();
	}
}
