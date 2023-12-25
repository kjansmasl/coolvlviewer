/**
 * @file lldrawpoolavatar.cpp
 * @brief LLDrawPoolAvatar class implementation
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

#include "lldrawpoolavatar.h"

#include "llfasttimer.h"
#include "llnoise.h"
#include "llrenderutils.h"			// For gSphere

#include "llagent.h"
#include "llface.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerpartsim.h"
#include "llviewershadermgr.h"
#include "llvoavatarself.h"
#include "llvovolume.h"

LLGLSLShader* LLDrawPoolAvatar::sVertexProgram = NULL;
bool LLDrawPoolAvatar::sSkipOpaque = false;
bool LLDrawPoolAvatar::sSkipTransparent = false;
S32 LLDrawPoolAvatar::sShadowPass = -1;
S32 LLDrawPoolAvatar::sDiffuseChannel = 0;
F32 LLDrawPoolAvatar::sMinimumAlpha = 0.2f;

static bool sIsDeferredRender = false;
static bool sIsPostDeferredRender = false;

extern bool gUseGLPick;

constexpr F32 CLOTHING_GRAVITY_EFFECT = 0.7f;

static bool sRenderingSkinned = false;
static S32 sNormalChannel = -1;
static S32 sSpecularChannel = -1;

LLDrawPoolAvatar::LLDrawPoolAvatar(U32 type)
:	LLFacePool(type)
{
}

//virtual
LLDrawPoolAvatar::~LLDrawPoolAvatar()
{
	if (!isDead())
	{
		llwarns << "Destroying a pool (" << std::hex << (intptr_t)this
				<< std::dec << ") still containing faces" << llendl;
	}
}

//virtual
void LLDrawPoolAvatar::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_AVATAR);
}

LLMatrix4& LLDrawPoolAvatar::getModelView()
{
	static LLMatrix4 ret;
	ret.set(gGLModelView.getF32ptr());
	return ret;
}

//virtual
void LLDrawPoolAvatar::beginDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);

	sSkipTransparent = true;
	sIsDeferredRender = true;

	if (LLPipeline::sImpostorRender)
	{
		// Impostor pass does not have impostor rendering
		++pass;
	}

	switch (pass)
	{
		case 0:
			beginDeferredImpostor();
			break;

		case 1:
			beginDeferredRigid();
			break;

		case 2:
			beginDeferredSkinned();

		default:
			break;
	}
}

//virtual
void LLDrawPoolAvatar::endDeferredPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);

	sSkipTransparent = false;
	sIsDeferredRender = false;

	if (LLPipeline::sImpostorRender)
	{
		++pass;
	}

	switch (pass)
	{
		case 0:
			endDeferredImpostor();
			break;

		case 1:
			endDeferredRigid();
			break;

		case 2:
			endDeferredSkinned();

		default:
			break;
	}
}

//virtual
void LLDrawPoolAvatar::beginPostDeferredPass(S32 pass)
{
	sSkipOpaque = true;
	sVertexProgram = &gDeferredAvatarAlphaProgram;
	sRenderingSkinned = true;

	gPipeline.bindDeferredShader(*sVertexProgram);

	sVertexProgram->setMinimumAlpha(sMinimumAlpha);

	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
}

//virtual
void LLDrawPoolAvatar::endPostDeferredPass(S32 pass)
{
	// If we are in software-blending, remember to set the fence _after_ we
	// draw so we wait till this rendering is done
	sRenderingSkinned = false;
	sSkipOpaque = false;

	gPipeline.unbindDeferredShader(*sVertexProgram);
	sDiffuseChannel = 0;
}

//virtual
void LLDrawPoolAvatar::renderPostDeferred(S32 pass)
{
	sIsPostDeferredRender = true;
	if (LLPipeline::sImpostorRender)
	{
		// *HACK: for impostors so actual pass ends up being proper pass
		render(0);
	}
	else
	{
		render(2);
	}
	sIsPostDeferredRender = false;
}

//virtual
void LLDrawPoolAvatar::beginShadowPass(S32 pass)
{
	LL_FAST_TIMER(FTM_SHADOW_AVATAR);

	if (pass == SHADOW_PASS_AVATAR_OPAQUE)
	{
		sVertexProgram = &gDeferredAvatarShadowProgram;

		if (mShaderLevel)  // For hardware blending
		{
			sRenderingSkinned = true;
			sVertexProgram->bind();
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else if (pass == SHADOW_PASS_AVATAR_ALPHA_BLEND)
	{
		sVertexProgram = &gDeferredAvatarAlphaShadowProgram;

		// Bind diffuse tex so we can reference the alpha channel...
		if (sVertexProgram->getUniformLocation(LLViewerShaderMgr::DIFFUSE_MAP) != -1)
		{
			sDiffuseChannel =
				sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			sDiffuseChannel = 0;
		}

		if (mShaderLevel)  // For hardware blending
		{
			sRenderingSkinned = true;
			sVertexProgram->bind();
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else if (pass == SHADOW_PASS_AVATAR_ALPHA_MASK)
	{
		sVertexProgram = &gDeferredAvatarAlphaMaskShadowProgram;

		// Bind diffuse tex so we can reference the alpha channel...
		if (sVertexProgram->getUniformLocation(LLViewerShaderMgr::DIFFUSE_MAP) != -1)
		{
			sDiffuseChannel =
				sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			sDiffuseChannel = 0;
		}

		if (mShaderLevel)  // For hardware blending
		{
			sRenderingSkinned = true;
			sVertexProgram->bind();
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
}

//virtual
void LLDrawPoolAvatar::endShadowPass(S32 pass)
{
	LL_FAST_TIMER(FTM_SHADOW_AVATAR);

	if (mShaderLevel)
	{
		sVertexProgram->unbind();
	}

	sVertexProgram = NULL;
	sRenderingSkinned = false;
	sShadowPass = -1;
}

//virtual
void LLDrawPoolAvatar::renderShadow(S32 pass)
{
	LL_FAST_TIMER(FTM_SHADOW_AVATAR);

	if (mDrawFace.empty())
	{
		return;
	}

	const LLFace* facep = mDrawFace[0];
	if (!facep || !facep->getDrawable())
	{
		return;
	}

	LLVOAvatar* avatarp = (LLVOAvatar*)facep->getDrawable()->getVObj().get();
	if (!avatarp || avatarp->isDead() || avatarp->isUIAvatar() ||
		avatarp->mDrawable.isNull() || avatarp->isVisuallyMuted() ||
		avatarp->isImpostor())
	{
		return;
	}

	sShadowPass = pass;

	if (pass == SHADOW_PASS_AVATAR_OPAQUE)
	{
		sSkipTransparent = true;
		avatarp->renderSkinned();
		sSkipTransparent = false;
		return;
	}

	if (pass == SHADOW_PASS_AVATAR_ALPHA_BLEND ||
		pass == SHADOW_PASS_AVATAR_ALPHA_MASK)
	{
		sSkipOpaque = true;
		avatarp->renderSkinned();
		sSkipOpaque = false;
	}
}

//virtual
void LLDrawPoolAvatar::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);
	if (LLPipeline::sImpostorRender)
	{
		++pass;
	}
	renderAvatars(NULL, pass); // Render all avatars
}

//virtual
void LLDrawPoolAvatar::beginRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);
	// Reset vertex buffer mappings
	LLVertexBuffer::unbind();

	if (LLPipeline::sImpostorRender)
	{
		// Impostor render does not have impostors rendering
		++pass;
	}

	switch (pass)
	{
		case 0:
			beginImpostor();
			// Make sure no stale colors are left over from a previous render
			gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
			break;

		case 1:
			beginRigid();
			break;

		case 2:
			beginSkinned();

		default:
			break;
	}
}

//virtual
void LLDrawPoolAvatar::endRenderPass(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_CHARACTERS);

	if (LLPipeline::sImpostorRender)
	{
		++pass;
	}

	switch (pass)
	{
		case 0:
			endImpostor();
			break;

		case 1:
			endRigid();
			break;

		case 2:
			endSkinned();

		default:
			break;
	}
}

void LLDrawPoolAvatar::beginImpostor()
{
	if (!LLPipeline::sReflectionRender)
	{
		LLVOAvatar::sRenderDistance = llclamp(LLVOAvatar::sRenderDistance,
											  16.f, 256.f);
		LLVOAvatar::sNumVisibleAvatars = 0;
	}

	gImpostorProgram.bind();
	gImpostorProgram.setMinimumAlpha(0.01f);

	gPipeline.enableLightsFullbright();
	sDiffuseChannel = 0;
}

void LLDrawPoolAvatar::endImpostor()
{
	gImpostorProgram.unbind();
	gPipeline.enableLightsDynamic();
}

void LLDrawPoolAvatar::beginRigid()
{
	if (!gPipeline.shadersLoaded())
	{
		sVertexProgram = NULL;
		return;
	}

	if (LLPipeline::sUnderWaterRender && !gUsePBRShaders)
	{
		sVertexProgram = &gObjectAlphaMaskNoColorWaterProgram;
	}
	else
	{
		sVertexProgram = &gObjectAlphaMaskNoColorProgram;
	}

	if (sVertexProgram)
	{
		// Eyeballs render with the specular shader
		sVertexProgram->bind();
		sVertexProgram->setMinimumAlpha(sMinimumAlpha);
		if (!gUsePBRShaders)
		{
			S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
			sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
		}
	}
}

void LLDrawPoolAvatar::endRigid()
{
	if (sVertexProgram)
	{
		sVertexProgram->unbind();
	}
}

void LLDrawPoolAvatar::beginDeferredImpostor()
{
	if (!LLPipeline::sReflectionRender)
	{
		LLVOAvatar::sRenderDistance = llclamp(LLVOAvatar::sRenderDistance,
											  16.f, 256.f);
		LLVOAvatar::sNumVisibleAvatars = 0;
	}

	sVertexProgram = &gDeferredImpostorProgram;
	sSpecularChannel =
		sVertexProgram->enableTexture(LLShaderMgr::SPECULAR_MAP);
	sNormalChannel =
		sVertexProgram->enableTexture(LLShaderMgr::DEFERRED_NORMAL);
	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
	sVertexProgram->bind();
	sVertexProgram->setMinimumAlpha(0.01f);
}

void LLDrawPoolAvatar::endDeferredImpostor()
{
	sVertexProgram->disableTexture(LLShaderMgr::DEFERRED_NORMAL);
	sVertexProgram->disableTexture(LLShaderMgr::SPECULAR_MAP);
	sVertexProgram->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	gPipeline.unbindDeferredShader(*sVertexProgram);
    sVertexProgram = NULL;
    sDiffuseChannel = 0;
}

void LLDrawPoolAvatar::beginDeferredRigid()
{
	sVertexProgram = &gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);
	sVertexProgram->bind();
	sVertexProgram->setMinimumAlpha(sMinimumAlpha);
	if (!gUsePBRShaders)
	{
		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
	}
}

void LLDrawPoolAvatar::endDeferredRigid()
{
	sVertexProgram->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	sVertexProgram->unbind();
	gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::beginSkinned()
{
	if (gUsePBRShaders)
	{
		sRenderingSkinned = true;

		sVertexProgram = &gAvatarProgram;
		sVertexProgram->bind();
		sVertexProgram->setMinimumAlpha(sMinimumAlpha);
		return;
	}

	if (mShaderLevel)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			sVertexProgram = &gAvatarWaterProgram;
		}
		else
		{
			sVertexProgram = &gAvatarProgram;
		}
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sVertexProgram = &gObjectAlphaMaskNoColorWaterProgram;
	}
	else
	{
		sVertexProgram = &gObjectAlphaMaskNoColorProgram;
	}

	if (mShaderLevel)  // For hardware blending
	{
		sRenderingSkinned = true;

		sVertexProgram->bind();
		sVertexProgram->enableTexture(LLShaderMgr::BUMP_MAP);
		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
		gGL.getTexUnit(0)->activate();
	}
	else if (gPipeline.shadersLoaded())
	{
		// Software skinning, use a basic shader for windlight.
		// *TODO: find a better fallback method for software skinning.
		sVertexProgram->bind();
		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
	}

	sVertexProgram->setMinimumAlpha(sMinimumAlpha);
}

void LLDrawPoolAvatar::endSkinned()
{
	LLTexUnit* unitp = gGL.getTexUnit(0);

	// If we are in software-blending, remember to set the fence _after_ we
	// draw so we wait till this rendering is done
	if (mShaderLevel)
	{
		sRenderingSkinned = false;
		if (!gUsePBRShaders) // BUMP_MAP not used by the PBR avatar shaders. HB
		{
			sVertexProgram->disableTexture(LLShaderMgr::BUMP_MAP);
		}
		unitp->activate();
		sVertexProgram->unbind();
	}
	else if (gPipeline.shadersLoaded())
	{
		// Software skinning, use a basic shader for windlight.
		// *TODO: find a better fallback method for software skinning.
		sVertexProgram->unbind();
	}

	unitp->activate();
}

void LLDrawPoolAvatar::beginDeferredSkinned()
{
	sRenderingSkinned = true;

	sVertexProgram = &gDeferredAvatarProgram;

	sVertexProgram->bind();
	sVertexProgram->setMinimumAlpha(sMinimumAlpha);
	if (!gUsePBRShaders)
	{
		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		sVertexProgram->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);
	}

	sDiffuseChannel = sVertexProgram->enableTexture(LLShaderMgr::DIFFUSE_MAP);

	gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::endDeferredSkinned()
{
	// If we are in software-blending, remember to set the fence _after_ we
	// draw so we wait till this rendering is done
	sRenderingSkinned = false;
	sVertexProgram->unbind();

	sVertexProgram->disableTexture(LLShaderMgr::DIFFUSE_MAP);

	gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::renderAvatars(LLVOAvatar* single_avatar, S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_AVATARS);

	if (pass == -1)
	{
		// Start with i = 1 to skip impostor pass
		for (S32 i = 1, count = getNumPasses(); i < count; ++i)
		{
			prerender();
			beginRenderPass(i);
			renderAvatars(single_avatar, i);
			endRenderPass(i);
		}
#if 0	// Failed attempt at getting UI avatars to render in PBR mode. HB
		beginPostDeferredPass(0);
		renderPostDeferred(0);
		endPostDeferredPass(0);
#endif
		return;
	}

	if (!single_avatar && mDrawFace.empty())
	{
		return;
	}

	LLVOAvatar* avatarp;
	if (single_avatar)
	{
		avatarp = single_avatar;
	}
	else
	{
		const LLFace* facep = mDrawFace[0];
		if (!facep || !facep->getDrawable())
		{
			return;
		}
		avatarp = (LLVOAvatar*)facep->getDrawable()->getVObj().get();
	}

    if (!avatarp || avatarp->isDead() || avatarp->mDrawable.isNull())
	{
		return;
	}

//MK
	// If this avatar is totally hidden by vision restriction spheres, then do
	// not render it at all...
	if (gRLenabled && !gRLInterface.avatarVisibility(avatarp))
	{
		return;
	}
//mk

	static LLCachedControl<bool> hit_box(gSavedSettings, "RenderDebugHitBox");
	if (pass == 1 && hit_box)
	{
		gDebugProgram.bind();

		// Set up drawing mode and remove any texture in use
		LLGLEnable blend(GL_BLEND);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		// Save current world matrix
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();

		const LLColor4& avatar_color = avatarp->getMinimapColor();
		gGL.diffuseColor4f(avatar_color.mV[VX], avatar_color.mV[VY],
						   avatar_color.mV[VZ], avatar_color.mV[VW]);

		const LLVector3& pos = avatarp->getPositionAgent();
		const LLVector3& size = avatarp->getScale();
		LLQuaternion rot = avatarp->getRotationRegion();

		// Set up and rotate the hit box to avatar orientation and half the
		// avatar size in either direction.
		static const LLVector3 sv1 = LLVector3(0.5f, 0.5f, 0.5f);
		static const LLVector3 sv2 = LLVector3(-0.5f, 0.5f, 0.5f);
		static const LLVector3 sv3 = LLVector3(-0.5f, -0.5f, 0.5f);
		static const LLVector3 sv4 = LLVector3(0.5f, -0.5f, 0.5f);
		LLVector3 v1 = size.scaledVec(sv1) * rot;
		LLVector3 v2 = size.scaledVec(sv2) * rot;
		LLVector3 v3 = size.scaledVec(sv3) * rot;
		LLVector3 v4 = size.scaledVec(sv4) * rot;

		// Box corners coordinates
		LLVector3 pospv1 = pos + v1;
		LLVector3 posmv1 = pos - v1;
		LLVector3 pospv2 = pos + v2;
		LLVector3 posmv2 = pos - v2;
		LLVector3 pospv3 = pos + v3;
		LLVector3 posmv3 = pos - v3;
		LLVector3 pospv4 = pos + v4;
		LLVector3 posmv4 = pos - v4;

		// Render the box

		gGL.begin(LLRender::LINES);

		// Top
		gGL.vertex3fv(pospv1.mV);
		gGL.vertex3fv(pospv2.mV);
		gGL.vertex3fv(pospv2.mV);
		gGL.vertex3fv(pospv3.mV);
		gGL.vertex3fv(pospv3.mV);
		gGL.vertex3fv(pospv4.mV);
		gGL.vertex3fv(pospv4.mV);
		gGL.vertex3fv(pospv1.mV);

		// Bottom
		gGL.vertex3fv(posmv1.mV);
		gGL.vertex3fv(posmv2.mV);
		gGL.vertex3fv(posmv2.mV);
		gGL.vertex3fv(posmv3.mV);
		gGL.vertex3fv(posmv3.mV);
		gGL.vertex3fv(posmv4.mV);
		gGL.vertex3fv(posmv4.mV);
		gGL.vertex3fv(posmv1.mV);
		
		// Right
		gGL.vertex3fv(pospv1.mV);
		gGL.vertex3fv(posmv3.mV);
		gGL.vertex3fv(pospv4.mV);
		gGL.vertex3fv(posmv2.mV);
		
		// Left
		gGL.vertex3fv(pospv2.mV);
		gGL.vertex3fv(posmv4.mV);
		gGL.vertex3fv(pospv3.mV);
		gGL.vertex3fv(posmv1.mV);

		gGL.end();

		// Restore world matrix
		gGL.popMatrix();

		gDebugProgram.unbind();
	}

	if (!single_avatar && !avatarp->isFullyLoaded())
	{
		if (pass == 0 &&
			(!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES) ||
			 LLViewerPartSim::getMaxPartCount() <= 0))
		{
			// Debug code to draw a sphere in place of avatar
			gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);

			gGL.setColorMask(true, true);

			LLVector3 pos = avatarp->getPositionAgent();
			gGL.color4f(1.f, 1.f, 1.f, 0.7f);

			gGL.pushMatrix();

			gGL.translatef((F32)pos.mV[VX], (F32)pos.mV[VY], (F32)pos.mV[VZ]);
			gGL.scalef(0.15f, 0.15f, 0.3f);
			gSphere.renderGGL();

			gGL.popMatrix();

			gGL.setColorMask(true, false);
		}
		// Do not render please
		return;
	}

	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_AVATAR))
	{
		return;
	}

	bool impostor = !single_avatar && avatarp->isImpostor();
	bool only_pass0 = impostor ||
					  (single_avatar && !avatarp->needsImpostorUpdate() &&
					   (avatarp->getVisualMuteSettings() ==
						 LLVOAvatar::AV_DO_NOT_RENDER ||
//MK
						 (gRLenabled &&
						  gRLInterface.avatarVisibility(avatarp) != 1)));
//mk
 	if (pass != 0 && only_pass0)
	{
		// Do not draw anything but the impostor for impostored avatars
		return;
	}

	if (pass == 0 && !impostor && LLPipeline::sUnderWaterRender)
	{
		// Do not draw foot shadows under water
		return;
	}

	if (single_avatar)
	{
		LLVOAvatar* attached_av = avatarp->getAttachedAvatar();
		// Do not render any animesh for visually muted avatars
		if (attached_av && attached_av->isVisuallyMuted())
		{
			return;
		}
	}

	if (pass == 0)
	{
		if (!LLPipeline::sReflectionRender)
		{
			++LLVOAvatar::sNumVisibleAvatars;
		}

		if (only_pass0)
		{
			if (LLPipeline::sRenderDeferred &&
				!LLPipeline::sReflectionRender &&
				avatarp->mImpostor.isComplete())
			{
				U32 num_tex = avatarp->mImpostor.getNumTextures();
				if (sNormalChannel > -1 && num_tex >= 3)
				{
					avatarp->mImpostor.bindTexture(2, sNormalChannel);
				}
				if (sSpecularChannel > -1 && num_tex >= 2)
				{
					avatarp->mImpostor.bindTexture(1, sSpecularChannel);
				}
			}
			avatarp->renderImpostor(avatarp->getMutedAVColor(),
									sDiffuseChannel);
		}
		return;
	}

	if (pass == 1)
	{
		// Render rigid meshes (eyeballs) first
		avatarp->renderRigid();
		return;
	}

	if (!gUsePBRShaders && mShaderLevel >= SHADER_LEVEL_CLOTH)
	{
		LLMatrix4 rot_mat;
		gViewerCamera.getMatrixToLocal(rot_mat);
		LLMatrix4 cfr(OGL_TO_CFR_ROTATION);
		rot_mat *= cfr;

		LLVector4 wind;
		wind.set(avatarp->mWindVec);
		wind.mV[VW] = 0.f;
		wind = wind * rot_mat;
		wind.mV[VW] = avatarp->mWindVec.mV[VW];

		sVertexProgram->uniform4fv(LLShaderMgr::AVATAR_WIND, 1, wind.mV);
		F32 phase = -(avatarp->mRipplePhase);

		F32 freq = 7.f + 2.f * noise1(avatarp->mRipplePhase);
		LLVector4 sin_params(freq, freq, freq, phase);
		sVertexProgram->uniform4fv(LLShaderMgr::AVATAR_SINWAVE, 1,
								   sin_params.mV);

		LLVector4 gravity(0.f, 0.f, -CLOTHING_GRAVITY_EFFECT, 0.f);
		gravity = gravity * rot_mat;
		sVertexProgram->uniform4fv(LLShaderMgr::AVATAR_GRAVITY, 1, gravity.mV);
	}

	if (!single_avatar || avatarp == single_avatar)
	{
		avatarp->renderSkinned();
//MK
		if (sIsPostDeferredRender && gRLenabled && avatarp->isSelf() &&
			!gRLInterface.mRenderLimitRenderedThisFrame &&
			gRLInterface.mVisionRestricted && avatarp->isFullyLoaded())
		{
			LL_TRACY_TIMER(TRC_RLV_RENDER_LIMITS);
			// Possibly draw a big black sphere around our avatar if the camera
			// render is limited
			gRLInterface.drawRenderLimit(false);
		}
//mk
	}
}
