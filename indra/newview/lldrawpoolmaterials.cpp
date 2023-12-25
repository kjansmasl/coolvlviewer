/**
 * @file lldrawpoolmaterials.cpp
 * @brief LLDrawPoolMaterials and LLDrawPoolMatPBR class implementations
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012-2022, Linden Research, Inc.
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

#include "lldrawpoolmaterials.h"

#include "llfasttimer.h"
#include "llmodel.h"

#include "llpipeline.h"
#include "llviewershadermgr.h"
#include "llvoavatar.h"

// Only used by the EE renderer.
static S32 sDiffuseChannel = -1;

static const U32 sTypeList[] =
{
	LLRenderPass::PASS_MATERIAL,
	//LLRenderPass::PASS_MATERIAL_ALPHA,
	LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
	LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
	LLRenderPass::PASS_SPECMAP,
	//LLRenderPass::PASS_SPECMAP_BLEND,
	LLRenderPass::PASS_SPECMAP_MASK,
	LLRenderPass::PASS_SPECMAP_EMISSIVE,
	LLRenderPass::PASS_NORMMAP,
	//LLRenderPass::PASS_NORMMAP_BLEND,
	LLRenderPass::PASS_NORMMAP_MASK,
	LLRenderPass::PASS_NORMMAP_EMISSIVE,
	LLRenderPass::PASS_NORMSPEC,
	//LLRenderPass::PASS_NORMSPEC_BLEND,
	LLRenderPass::PASS_NORMSPEC_MASK,
	LLRenderPass::PASS_NORMSPEC_EMISSIVE,
};

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolMaterials class
///////////////////////////////////////////////////////////////////////////////

LLDrawPoolMaterials::LLDrawPoolMaterials()
:	LLRenderPass(LLDrawPool::POOL_MATERIALS)
{
}

//virtual
void LLDrawPoolMaterials::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

//virtual
void LLDrawPoolMaterials::beginDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_MATERIALS);

	bool rigged = pass >= 12;
	if (rigged)
	{
		pass -= 12;
	}

	U32 shader_idx[] =
	{
		0,		// PASS_MATERIAL,
		//1,	// PASS_MATERIAL_ALPHA,
		2,		// PASS_MATERIAL_ALPHA_MASK,
		3,		// PASS_MATERIAL_ALPHA_EMISSIVE,
		4,		// PASS_SPECMAP,
		//5,	// PASS_SPECMAP_BLEND,
		6,		// PASS_SPECMAP_MASK,
		7,		// PASS_SPECMAP_EMISSIVE,
		8,		// PASS_NORMMAP,
		//9,	// PASS_NORMMAP_BLEND,
		10,		// PASS_NORMMAP_MASK,
		11,		// PASS_NORMMAP_EMISSIVE,
		12,		// PASS_NORMSPEC,
		//13,	// PASS_NORMSPEC_BLEND,
		14,		// PASS_NORMSPEC_MASK,
		15,		// PASS_NORMSPEC_EMISSIVE,
	};

	if (LLPipeline::sUnderWaterRender && !gUsePBRShaders)
	{
		mShader = &gDeferredMaterialWaterProgram[shader_idx[pass]];
	}
	else
	{
		mShader = &gDeferredMaterialProgram[shader_idx[pass]];
	}
	if (rigged)
	{
		if (mShader->mRiggedVariant)
		{
			mShader = mShader->mRiggedVariant;
		}
		else
		{
			llwarns_once << "Missing rigged variant shader !" << llendl;
		}
	}
	if (gUsePBRShaders)
	{
		gPipeline.bindDeferredShader(*mShader);
	}
	else
	{
		mShader->bind();

		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		mShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);

		sDiffuseChannel = mShader->enableTexture(LLShaderMgr::DIFFUSE_MAP);
	}
}

//virtual
void LLDrawPoolMaterials::endDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_MATERIALS);

	if (gUsePBRShaders)
	{
		gPipeline.unbindDeferredShader(*mShader);
	}
	else
	{
		mShader->unbind();
	}

	LLRenderPass::endRenderPass(pass);
}

//virtual
void LLDrawPoolMaterials::renderDeferred(S32 pass)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	if (gUsePBRShaders)
	{
		renderDeferredPBR(pass);
		return;
	}

	bool rigged = pass >= 12;
	if (rigged)
	{
		pass -= 12;
	}

	llassert(pass < (S32)LL_ARRAY_SIZE(sTypeList));

	U32 type = sTypeList[pass];
	if (rigged)
	{
		++type;
	}

	U32 mask = mShader->mAttributeMask;
	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;
	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i != count)
		{
			LLVertexBuffer* vb = draw_list[i]->mVertexBuffer.get();
			if (vb)	// Paranoia
			{
				_mm_prefetch((char*)vb, _MM_HINT_NTA);
			}
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (!paramsp->mVertexBuffer) continue;	// Paranoia

		if (rigged && paramsp->mAvatar && paramsp->mSkinInfo &&
			(paramsp->mAvatar != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0		// Better seeing part of the avatar rather than nothing at all. HB
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar;
			last_hash = paramsp->mSkinInfo->mHash;
		}

		mShader->uniform4f(LLShaderMgr::SPECULAR_COLOR,
						   paramsp->mSpecColor.mV[0], paramsp->mSpecColor.mV[1],
						   paramsp->mSpecColor.mV[2], paramsp->mSpecColor.mV[3]);
		mShader->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY,
						   paramsp->mEnvIntensity);

		if (paramsp->mNormalMap)
		{
			paramsp->mNormalMap->addTextureStats(paramsp->mVSize);
			bindNormalMap(paramsp->mNormalMap);
		}

		if (paramsp->mSpecularMap)
		{
			paramsp->mSpecularMap->addTextureStats(paramsp->mVSize);
			bindSpecularMap(paramsp->mSpecularMap);
		}

		mShader->setMinimumAlpha(paramsp->mAlphaMaskCutoff);
		F32 brightness = paramsp->mFullbright ? 1.f : 0.f;
		mShader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, brightness);

		pushMaterialsBatch(*paramsp, mask);
	}
}

void LLDrawPoolMaterials::renderDeferredPBR(S32 pass)
{
	bool rigged = pass >= 12;
	if (rigged)
	{
		pass -= 12;
	}

	llassert(pass < (S32)LL_ARRAY_SIZE(sTypeList));

	U32 type = sTypeList[pass];
	if (rigged)
	{
		++type;
	}

	S32 intensity_loc =
		mShader->getUniformLocation(LLShaderMgr::ENVIRONMENT_INTENSITY);
	S32 brightness_loc =
		mShader->getUniformLocation(LLShaderMgr::EMISSIVE_BRIGHTNESS);
	S32 min_alpha_loc =
		mShader->getUniformLocation(LLShaderMgr::MINIMUM_ALPHA);
	S32 specular_loc =
		mShader->getUniformLocation(LLShaderMgr::SPECULAR_COLOR);

	LLTexUnit* diffunitp =
		gGL.getTexUnit(mShader->enableTexture(LLShaderMgr::DIFFUSE_MAP));

	S32 channel = mShader->enableTexture(LLShaderMgr::SPECULAR_MAP);
	LLTexUnit* specunitp = channel > -1 ? gGL.getTexUnit(channel) : NULL;

	channel = mShader->enableTexture(LLShaderMgr::BUMP_MAP);
	LLTexUnit* normunitp = channel > -1 ? gGL.getTexUnit(channel) : NULL;

	diffunitp->unbindFast(LLTexUnit::TT_TEXTURE);

	F32 last_intensity = 0.f;
	if (intensity_loc > -1)
	{
		glUniform1f(intensity_loc, last_intensity);
	}

	F32 last_fullbrigh = 0.f;
	if (brightness_loc > -1)
	{
		glUniform1f(brightness_loc, last_fullbrigh);
	}

	F32 last_min_alpha = 0.f;
	if (min_alpha_loc > -1)
	{
		glUniform1f(min_alpha_loc, last_min_alpha);
	}

	LLVector4 last_specular(0.f, 0.f, 0.f, 0.f);
	if (specular_loc > -1)
	{
		glUniform4fv(specular_loc, 1, last_specular.mV);
	}

	LLViewerTexture* last_diffp = NULL;
	LLViewerTexture* last_normp = NULL;
	LLViewerTexture* last_specp = NULL;

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;
	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i != count)
		{
			LLVertexBuffer* vb = draw_list[i]->mVertexBuffer.get();
			if (vb)	// Paranoia
			{
				_mm_prefetch((char*)vb, _MM_HINT_NTA);
			}
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (!paramsp->mVertexBuffer) continue;	// Paranoia

		if (specular_loc > -1 && paramsp->mSpecColor != last_specular)
		{
			last_specular = paramsp->mSpecColor;
			glUniform4fv(specular_loc, 1, last_specular.mV);
		}

		if (intensity_loc > -1 && paramsp->mEnvIntensity != last_intensity)
		{
			last_intensity = paramsp->mEnvIntensity;
			glUniform1f(intensity_loc, last_intensity);
		}

		if (min_alpha_loc > -1 && paramsp->mAlphaMaskCutoff != last_min_alpha)
		{
			last_min_alpha = paramsp->mAlphaMaskCutoff;
			glUniform1f(min_alpha_loc, last_min_alpha);
		}

		F32 fullbright = paramsp->mFullbright ? 1.f : 0.f;
		if (brightness_loc > -1 && fullbright != last_fullbrigh)
		{
			last_fullbrigh = fullbright;
			glUniform1f(brightness_loc, last_fullbrigh);
		}

		if (normunitp && paramsp->mNormalMap.get() != last_normp)
		{
			last_normp = paramsp->mNormalMap.get();
			if (last_normp)
			{
				normunitp->bindFast(last_normp);
				last_normp->addTextureStats(paramsp->mVSize);
			}
		}

		if (specunitp && paramsp->mSpecularMap.get() != last_specp)
		{
			last_specp = paramsp->mSpecularMap.get();
			if (last_specp)
			{
				specunitp->bindFast(last_specp);
				last_specp->addTextureStats(paramsp->mVSize);
			}
		}

		if (paramsp->mTexture.get() != last_diffp)
		{
			last_diffp = paramsp->mTexture.get();
			if (last_diffp)
			{
				diffunitp->bindFast(last_diffp);
				last_diffp->addTextureStats(paramsp->mVSize);
			}
			else
			{
				diffunitp->unbindFast(LLTexUnit::TT_TEXTURE);
			}
		}

		if (rigged && paramsp->mAvatar && paramsp->mSkinInfo &&
			(paramsp->mAvatar != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0		// Better seeing part of the avatar rather than nothing at all. HB
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar;
			last_hash = paramsp->mSkinInfo->mHash;
		}

		applyModelMatrix(*paramsp);

		bool tex_setup = false;
		if (paramsp->mTextureMatrix)
		{
			tex_setup = true;
			unit0->activate();
			gGL.matrixMode(LLRender::MM_TEXTURE);
			gGL.loadMatrix(paramsp->mTextureMatrix->getF32ptr());
			++gPipeline.mTextureMatrixOps;
		}

		paramsp->mVertexBuffer->setBuffer();
		paramsp->mVertexBuffer->drawRange(LLRender::TRIANGLES,
										  paramsp->mStart, paramsp->mEnd,
										  paramsp->mCount, paramsp->mOffset);

		if (tex_setup)
		{
			unit0->activate();
			gGL.loadIdentity();
			gGL.matrixMode(LLRender::MM_MODELVIEW);
		}
	}
}

// For EE rendering only
void LLDrawPoolMaterials::bindSpecularMap(LLViewerTexture* texp)
{
	mShader->bindTexture(LLShaderMgr::SPECULAR_MAP, texp);
}

void LLDrawPoolMaterials::bindNormalMap(LLViewerTexture* texp)
{
	mShader->bindTexture(LLShaderMgr::BUMP_MAP, texp);
}

// For EE rendering only
void LLDrawPoolMaterials::pushMaterialsBatch(LLDrawInfo& params, U32 mask)
{
	applyModelMatrix(params);

	bool tex_setup = false;

	if (params.mTextureMatrix)
	{
#if 0
		if (mShiny)
#endif
		{
			gGL.getTexUnit(0)->activate();
			gGL.matrixMode(LLRender::MM_TEXTURE);
		}

		gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
		++gPipeline.mTextureMatrixOps;

		tex_setup = true;
	}

	if (mShaderLevel > 1)
	{
		if (params.mTexture.notNull())
		{
			gGL.getTexUnit(sDiffuseChannel)->bindFast(params.mTexture);
		}
		else
		{
			gGL.getTexUnit(sDiffuseChannel)->unbindFast(LLTexUnit::TT_TEXTURE);
		}
	}

	params.mVertexBuffer->setBufferFast(mask);
	params.mVertexBuffer->drawRangeFast(params.mStart, params.mEnd,
										params.mCount, params.mOffset);
	if (tex_setup)
	{
		gGL.getTexUnit(0)->activate();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolMatPBR class
///////////////////////////////////////////////////////////////////////////////

LLDrawPoolMatPBR::LLDrawPoolMatPBR(U32 type)
:	LLRenderPass(type)
{
	if (type == POOL_MAT_PBR_ALPHA_MASK)
	{
		mRenderType = LLPipeline::RENDER_TYPE_PASS_MAT_ALPHA_MASK_PBR;
	}
	else
	{
		mRenderType = LLPipeline::RENDER_TYPE_PASS_MAT_PBR;
	}
}

//virtual
S32 LLDrawPoolMatPBR::getNumDeferredPasses()
{
	return gUsePBRShaders ? 1 : 0;
}

//virtual
void LLDrawPoolMatPBR::renderDeferred(S32 pass)
{
	if (LLPipeline::sRenderingHUDs)
	{
		return;
	}

	gDeferredPBROpaqueProgram.bind();
	pushGLTFBatches(mRenderType);

	gDeferredPBROpaqueProgram.bind(true);
	pushRiggedGLTFBatches(mRenderType + 1);
}

//virtual
void LLDrawPoolMatPBR::renderPostDeferred(S32 pass)
{
	if (LLPipeline::sRenderingHUDs)
	{
		gHUDPBROpaqueProgram.bind();
		pushGLTFBatches(mRenderType);
		return;
	}

	// *HACK: do not render glow except for the non-alpha masked implementation
	if (mRenderType == LLPipeline::RENDER_TYPE_PASS_MAT_PBR)
	{
		gGL.setColorMask(false, true);

		gPBRGlowProgram.bind();
		pushGLTFBatches(PASS_PBR_GLOW);

		gPBRGlowProgram.bind(true);
		pushRiggedGLTFBatches(PASS_PBR_GLOW_RIGGED);

		gGL.setColorMask(true, false);
	}
}
