/**
 * @file lldrawpoolbump.cpp
 * @brief LLDrawPoolBump class implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include <utility>

#include "lldrawpoolbump.h"

#include "llcubemap.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llglheaders.h"
#include "llimagegl.h"
#include "llmodel.h"
#include "llrender.h"
#include "lltextureentry.h"

#include "llappviewer.h"			// For gFrameTimeSeconds
#include "lldrawable.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"

//static
LLStandardBumpmap gStandardBumpmapList[TEM_BUMPMAP_COUNT];

//static
U32 LLStandardBumpmap::sStandardBumpmapCount = 0;

//static
LLBumpImageList gBumpImageList;

constexpr S32 STD_BUMP_LATEST_FILE_VERSION = 1;

constexpr U32 VERTEX_MASK_SHINY = LLVertexBuffer::MAP_VERTEX |
								  LLVertexBuffer::MAP_NORMAL |
								  LLVertexBuffer::MAP_COLOR;
constexpr U32 VERTEX_MASK_BUMP = LLVertexBuffer::MAP_VERTEX |
								 LLVertexBuffer::MAP_TEXCOORD0 |
								 LLVertexBuffer::MAP_TEXCOORD1;

U32 LLDrawPoolBump::sVertexMask = VERTEX_MASK_SHINY;

static LLGLSLShader* sCurrentShader = NULL;
static S32 sCubeChannel = -1;
static S32 sDiffuseChannel = -1;
static S32 sBumpChannel = -1;

LLRenderTarget LLBumpImageList::sRenderTarget;

//static
void LLStandardBumpmap::add()
{
	if (!gTextureList.isInitialized())
	{
		// Note: loading pre-configuration sometimes triggers this call.
		// But it is safe to return here because bump images will be reloaded
		// during initialization later.
		return;
	}

	// Cannot assert; we destroyGL and restoreGL a lot during *first* startup,
	// which populates this list already, THEN we explicitly init the list as
	// part of *normal* startup.  Sigh.  So clear the list every time before we
	// (re-)add the standard bumpmaps.
	//llassert(LLStandardBumpmap::sStandardBumpmapCount == 0);
	clear();
	llinfos << "Adding standard bumpmaps." << llendl;
	gStandardBumpmapList[sStandardBumpmapCount++] =
		LLStandardBumpmap("None"); 				    	// BE_NO_BUMP
	gStandardBumpmapList[sStandardBumpmapCount++] =
		LLStandardBumpmap("Brightness");				// BE_BRIGHTNESS
	gStandardBumpmapList[sStandardBumpmapCount++] =
		LLStandardBumpmap("Darkness");					// BE_DARKNESS

	std::string file_name =
		gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "std_bump.ini");
	LLFILE* file = LLFile::open(file_name, "rt");
	if (!file)
	{
		llwarns << "Could not open std_bump <" << file_name << ">" << llendl;
		return;
	}

	S32 file_version = 0;

	S32 fields_read = fscanf(file, "LLStandardBumpmap version %d",
							 &file_version);
	if (fields_read != 1)
	{
		llwarns << "Bad LLStandardBumpmap header" << llendl;
		LLFile::close(file);
		return;
	}

	if (file_version > STD_BUMP_LATEST_FILE_VERSION)
	{
		llwarns << "LLStandardBumpmap has newer version (" << file_version
				<< ") than viewer (" << STD_BUMP_LATEST_FILE_VERSION << ")"
				<< llendl;
		LLFile::close(file);
		return;
	}

	while (!feof(file) &&
		   LLStandardBumpmap::sStandardBumpmapCount < (U32)TEM_BUMPMAP_COUNT)
	{
		// *NOTE: This buffer size is hard coded into scanf() below.
		char label[256] = "";
		char bump_image_id[256] = "";
		fields_read = fscanf(file, "\n%255s %255s", label, bump_image_id);
		bump_image_id[UUID_STR_LENGTH - 1] = 0;	// Truncate file name to UUID
		if (fields_read == EOF)
		{
			break;
		}
		if (fields_read != 2)
		{
			llwarns << "Bad LLStandardBumpmap entry" << llendl;
			break;
		}

		LLStandardBumpmap& bump =
			gStandardBumpmapList[sStandardBumpmapCount++];

		bump.mLabel = label;
		bump.mImage =
			LLViewerTextureManager::getFetchedTexture(LLUUID(bump_image_id));
		bump.mImage->setBoostLevel(LLGLTexture::BOOST_BUMP);
#if !LL_IMPLICIT_SETNODELETE
		bump.mImage->setNoDelete();
#endif
		bump.mImage->setLoadedCallback(LLBumpImageList::onSourceStandardLoaded,
									   0, true, false, NULL, NULL);
		bump.mImage->forceToSaveRawImage(0, 30.f);
	}

	LLFile::close(file);
}

//static
void LLStandardBumpmap::clear()
{
	if (sStandardBumpmapCount)
	{
		llinfos << "Clearing standard bumpmaps." << llendl;
		for (U32 i = 0; i < LLStandardBumpmap::sStandardBumpmapCount; ++i)
		{
			gStandardBumpmapList[i].mLabel.clear();
			gStandardBumpmapList[i].mImage = NULL;
		}
		sStandardBumpmapCount = 0;
	}
}

////////////////////////////////////////////////////////////////

//virtual
void LLDrawPoolBump::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

// For the EE renderer only
//virtual
void LLDrawPoolBump::render(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_BUMP);

	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_BUMP))
	{
		return;
	}

	for (U32 rigged = 0; rigged < 2; ++rigged)
	{
		mRigged = rigged;

		// First pass: shiny
		beginShiny();
		renderShiny();
		endShiny();

		// Second pass: fullbright shiny
		if (mShaderLevel > 1)
		{
			beginFullbrightShiny();
			renderFullbrightShiny();
			endFullbrightShiny();
		}

		// Third pass: bump
		beginBump();
		renderBump(PASS_BUMP);
		endBump();
	}
}

// For the EE renderer only
//static
void LLDrawPoolBump::bindCubeMap(LLGLSLShader* shaderp, S32 shader_level,
								 S32& diffuse_channel, S32& cube_channel)
{
	LLCubeMap* cubemapp = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
	if (!cubemapp)
	{
		return;
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (shaderp)
	{
		LLMatrix4 mat(gGLModelView.getF32ptr());
		LLVector3 vec = LLVector3(gShinyOrigin) * mat;
		LLVector4 vec4(vec, gShinyOrigin.mV[3]);
		shaderp->uniform4fv(LLShaderMgr::SHINY_ORIGIN, 1, vec4.mV);
		if (shader_level > 1)
		{
			cubemapp->setMatrix(1);
			// Make sure that texture coord generation happens for tex unit 1,
			// as this is the one we use for the cube map in the one pass shiny
			// shaders
			cube_channel = shaderp->enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
												  LLTexUnit::TT_CUBE_MAP);
			cubemapp->enableTexture(cube_channel);
			diffuse_channel = shaderp->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		}
		else
		{
			cubemapp->setMatrix(0);
			cube_channel = shaderp->enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
												  LLTexUnit::TT_CUBE_MAP);
			diffuse_channel = -1;
			cubemapp->enableTexture(cube_channel);
		}
		gGL.getTexUnit(cube_channel)->bind(cubemapp);
		unit0->activate();
	}
	else
	{
		cube_channel = 0;
		diffuse_channel = -1;
		unit0->disable();
		cubemapp->enableTexture(0);
		cubemapp->setMatrix(0);
		unit0->bind(cubemapp);
	}
}

// For the EE renderer only
//static
void LLDrawPoolBump::unbindCubeMap(LLGLSLShader* shaderp, S32 shader_level,
								   S32& diffuse_channel)
{
	LLCubeMap* cubemapp = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
	if (!cubemapp)
	{
		return;
	}

	if (shader_level > 1)
	{
		shaderp->disableTexture(LLShaderMgr::ENVIRONMENT_MAP,
								LLTexUnit::TT_CUBE_MAP);
#if 0	// 'shader_level' is in fact mShaderLevel, which is itself set to
		// getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) in prerender(), so
		// there no need to test this (which always true)... HB
		if (gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 0)
#endif
		{
			if (diffuse_channel)
			{
				shaderp->disableTexture(LLShaderMgr::DIFFUSE_MAP);
			}
		}
	}

	// Moved below shaderp->disableTexture call to avoid false alarms from
	// auto-re-enable of textures on stage 0 - MAINT-755
	cubemapp->disableTexture();
	cubemapp->restoreMatrix();
}

// For the EE renderer only
void LLDrawPoolBump::beginShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);

	mShiny = true;
	sVertexMask = VERTEX_MASK_SHINY;
	// Second pass: environment map
	if (mShaderLevel > 1)
	{
		sVertexMask = VERTEX_MASK_SHINY | LLVertexBuffer::MAP_TEXCOORD0;
	}

	if (LLPipeline::sUnderWaterRender)
	{
		sCurrentShader = &gObjectShinyWaterProgram;
	}
	else
	{
		sCurrentShader = &gObjectShinyProgram;
	}
	if (mRigged && sCurrentShader->mRiggedVariant)
	{
		sCurrentShader = sCurrentShader->mRiggedVariant;
	}
	sCurrentShader->bind();
	S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
	sCurrentShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);

	bindCubeMap(sCurrentShader, mShaderLevel, sDiffuseChannel, sCubeChannel);

	if (mShaderLevel > 1)
	{
		// Indexed texture rendering, channel 0 is always diffuse
		sDiffuseChannel = 0;
	}
}

// For the EE renderer only
void LLDrawPoolBump::renderShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);

	if (!gSky.mVOSkyp->getCubeMap())
	{
		return;
	}

	LLGLEnable blend_enable(GL_BLEND);

	if (mShaderLevel > 1)
	{
		U32 mask = sVertexMask | LLVertexBuffer::MAP_TEXTURE_INDEX;
		if (mRigged)
		{
			LLRenderPass::pushRiggedBatches(PASS_SHINY_RIGGED, mask, true,
											true);
		}
		else
		{
			LLRenderPass::pushBatches(PASS_SHINY, mask, true, true);
		}
	}
	else if (mRigged)
	{
		gPipeline.renderRiggedGroups(this, PASS_SHINY_RIGGED, sVertexMask,
									 true);
	}
	else
	{
		gPipeline.renderGroups(this, PASS_SHINY, sVertexMask, true);
	}
}

// For the EE renderer only
void LLDrawPoolBump::endShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);

	unbindCubeMap(sCurrentShader, mShaderLevel, sDiffuseChannel);
	if (sCurrentShader)
	{
		sCurrentShader->unbind();
	}

	sDiffuseChannel = -1;
	sCubeChannel = 0;
	mShiny = false;
}

void LLDrawPoolBump::beginFullbrightShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);

	sVertexMask = VERTEX_MASK_SHINY | LLVertexBuffer::MAP_TEXCOORD0;

	// Second pass: environment map

	if (gUsePBRShaders)
	{
		sCurrentShader =
			LLPipeline::sRenderingHUDs ? &gHUDFullbrightShinyProgram
									   : &gDeferredFullbrightShinyProgram;
	}
	else if (LLPipeline::sUnderWaterRender)
	{
		sCurrentShader = &gObjectFullbrightShinyWaterProgram;
	}
	else if (LLPipeline::sRenderDeferred)
	{
		sCurrentShader = &gDeferredFullbrightShinyProgram;
	}
	else
	{
		sCurrentShader = &gObjectFullbrightShinyProgram;
	}

	if (mRigged)
	{
		if (sCurrentShader->mRiggedVariant)
		{
			sCurrentShader = sCurrentShader->mRiggedVariant;
		}
		else
		{
			llwarns_once << "Missing rigged variant shader !" << llendl;
		}
	}

	if (gUsePBRShaders)
	{
		// Bind exposure map so fullbright shader can cancel out exposure
		S32 channel = sCurrentShader->enableTexture(LLShaderMgr::EXPOSURE_MAP);
		if (channel > -1)
		{
			gGL.getTexUnit(channel)->bind(&gPipeline.mExposureMap);
		}
	}

	LLCubeMap* cubemapp = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
	if (gUsePBRShaders)
	{
		if (cubemapp && !LLPipeline::sReflectionProbesEnabled)
		{
			// Make sure that texture coord generation happens for tex unit 1,
			// as this is the one we use for the cube map in the one pass shiny
			// shaders
			gGL.getTexUnit(1)->disable();
			sCubeChannel =
				sCurrentShader->enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
											  LLTexUnit::TT_CUBE_MAP);
			cubemapp->enableTexture(sCubeChannel);
			sDiffuseChannel =
				sCurrentShader->enableTexture(LLShaderMgr::DIFFUSE_MAP);

			gGL.getTexUnit(sCubeChannel)->bind(cubemapp);
			gGL.getTexUnit(0)->activate();
		}
		LLMatrix4 mat(gGLModelView.getF32ptr());
		sCurrentShader->bind();

		LLVector3 vec = LLVector3(gShinyOrigin) * mat;
		LLVector4 vec4(vec, gShinyOrigin.mV[3]);
		sCurrentShader->uniform4fv(LLShaderMgr::SHINY_ORIGIN, 1, vec4.mV);
		if (LLPipeline::sReflectionProbesEnabled)
		{
			gPipeline.bindReflectionProbes(*sCurrentShader);
		}
		else
		{
			gPipeline.setEnvMat(*sCurrentShader);
		}
	}
	else if (cubemapp)
	{
		LLMatrix4 mat(gGLModelView.getF32ptr());
		sCurrentShader->bind();

		S32 no_atmo = LLPipeline::sRenderingHUDs ? 1 : 0;
		sCurrentShader->uniform1i(LLShaderMgr::NO_ATMO, no_atmo);

		LLVector3 vec = LLVector3(gShinyOrigin) * mat;
		LLVector4 vec4(vec, gShinyOrigin.mV[3]);
		sCurrentShader->uniform4fv(LLShaderMgr::SHINY_ORIGIN, 1, vec4.mV);

		cubemapp->setMatrix(1);
		// Make sure that texture coord generation happens for tex unit 1, as
		// this is the one we use for the cube map in the one pass shiny
		// shaders.
		gGL.getTexUnit(1)->disable();
		sCubeChannel =
			sCurrentShader->enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
										  LLTexUnit::TT_CUBE_MAP);
		cubemapp->enableTexture(sCubeChannel);
		sDiffuseChannel =
			sCurrentShader->enableTexture(LLShaderMgr::DIFFUSE_MAP);

		gGL.getTexUnit(sCubeChannel)->bind(cubemapp);
		gGL.getTexUnit(0)->activate();
	}

	if (mShaderLevel > 1)
	{
		// Indexed texture rendering, channel 0 is always diffuse
		sDiffuseChannel = 0;
	}

	mShiny = true;
}

void LLDrawPoolBump::renderFullbrightShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);

	if (!gUsePBRShaders && !gSky.mVOSkyp->getCubeMap())
	{
		return;
	}

	LLGLEnable blend_enable(GL_BLEND);

	if (mShaderLevel > 1)
	{
		// Note: 'mask' is ignored for the PBR renderer
		U32 mask = sVertexMask | LLVertexBuffer::MAP_TEXTURE_INDEX;
		if (mRigged)
		{
			LLRenderPass::pushRiggedBatches(PASS_FULLBRIGHT_SHINY_RIGGED, mask,
											true, true);
		}
		else
		{
			LLRenderPass::pushBatches(PASS_FULLBRIGHT_SHINY, mask, true, true);
		}
	}
	else if (mRigged)
	{
		LLRenderPass::pushRiggedBatches(PASS_FULLBRIGHT_SHINY_RIGGED,
										sVertexMask);
	}
	else
	{
		LLRenderPass::pushBatches(PASS_FULLBRIGHT_SHINY, sVertexMask);
	}
}

void LLDrawPoolBump::endFullbrightShiny()
{
	LL_FAST_TIMER(FTM_RENDER_SHINY);

	LLCubeMap* cubemapp = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
	if (cubemapp && !LLPipeline::sReflectionProbesEnabled)
	{
		cubemapp->disableTexture();
		if (!gUsePBRShaders)
		{
			cubemapp->restoreMatrix();
		}
		else if (sCurrentShader->mFeatures.hasReflectionProbes)
		{
			gPipeline.unbindReflectionProbes(*sCurrentShader);
		}
		sCurrentShader->unbind();
	}

	sDiffuseChannel = -1;
	sCubeChannel = 0;
	mShiny = false;
}

void LLDrawPoolBump::renderGroup(LLSpatialGroup* groupp, U32 type, U32 mask,
								 bool texture)
{
	LLSpatialGroup::drawmap_elem_t& draw_info = groupp->mDrawMap[type];
	for (U32 i = 0, count = draw_info.size(); i < count; ++i)
	{
		LLDrawInfo& params = *draw_info[i];

		applyModelMatrix(params);

		// Note: mask is ignored by the PBR renderer.
		params.mVertexBuffer->setBuffer(mask);
		params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
										params.mEnd, params.mCount,
										params.mOffset);
		gPipeline.addTrianglesDrawn(params.mCount);
	}
}

//static
bool LLDrawPoolBump::bindBumpMap(LLDrawInfo& params, S32 channel)
{
	return bindBumpMap(params.mBump, params.mTexture, params.mVSize, channel);
}

//static
bool LLDrawPoolBump::bindBumpMap(LLFace* facep, S32 channel)
{
	const LLTextureEntry* tep = facep->getTextureEntry();
	return tep && bindBumpMap(tep->getBumpmap(), facep->getTexture(),
							  facep->getVirtualSize(), channel);
}

//static
bool LLDrawPoolBump::bindBumpMap(U8 bump_code, LLViewerTexture* texturep,
								 F32 vsize, S32 channel)
{
	LLViewerFetchedTexture* texp =
		LLViewerTextureManager::staticCast(texturep);
	if (!texp)
	{
		// If the texture is not a fetched texture
		return false;
	}

	LLViewerTexture* bumpp = NULL;

	switch (bump_code)
	{
		case BE_NO_BUMP:
			break;

		case BE_BRIGHTNESS:
		case BE_DARKNESS:
			bumpp = gBumpImageList.getBrightnessDarknessImage(texp, bump_code);
			break;

		default:
			if (bump_code < LLStandardBumpmap::sStandardBumpmapCount)
			{
				bumpp = gStandardBumpmapList[bump_code].mImage;
				gBumpImageList.addTextureStats(bump_code, texp->getID(),
											   vsize);
			}
	}

	if (!bumpp)
	{
		return false;
	}

	if (channel == -2)
	{
		gGL.getTexUnit(1)->bindFast(bumpp);
		gGL.getTexUnit(0)->bindFast(bumpp);
	}
	else
	{
		// NOTE: do not use bindFast here (see SL-16222)
		gGL.getTexUnit(channel)->bind(bumpp);
	}

	return true;
}

// Optional second pass: emboss bump map
//static
void LLDrawPoolBump::beginBump()
{
	LL_FAST_TIMER(FTM_RENDER_BUMP);

	// Optional second pass: emboss bump map
	sVertexMask = VERTEX_MASK_BUMP;

	sCurrentShader = &gObjectBumpProgram;
	if (mRigged)
	{
		if (sCurrentShader->mRiggedVariant)
		{
			sCurrentShader = sCurrentShader->mRiggedVariant;
		}
		else
		{
			llwarns_once << "Missing rigged variant shader !" << llendl;
		}
	}
	sCurrentShader->bind();

	gGL.setSceneBlendType(LLRender::BT_MULT_X2);
	stop_glerror();
}

void LLDrawPoolBump::renderBump(U32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_BUMP);
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
	LLGLEnable blend(GL_BLEND);
	gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	// Get rid of z-fighting with non-bump pass.
	LLGLEnable poly_offset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.f, -1.f);
	if (gUsePBRShaders)
	{
		pushBumpBatches(pass);
	}
	else
	{
		renderBump(pass, VERTEX_MASK_BUMP);
	}
}

//static
void LLDrawPoolBump::endBump()
{
	if (gUsePBRShaders)
	{
		 LLGLSLShader::unbind();
	}
	else
	{
		sCurrentShader->unbind();
	}
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

void LLDrawPoolBump::renderDeferred(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_BUMP);

	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	mShiny = true;

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	constexpr U32 mask = LLVertexBuffer::MAP_VERTEX |
						 LLVertexBuffer::MAP_TEXCOORD0 |
						 LLVertexBuffer::MAP_TANGENT |
						 LLVertexBuffer::MAP_NORMAL |
						 LLVertexBuffer::MAP_COLOR;

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	for (U32 rigged = 0; rigged < 2; ++rigged)
	{
		gDeferredBumpProgram.bind(rigged);
		LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
		sDiffuseChannel = shaderp->enableTexture(LLShaderMgr::DIFFUSE_MAP);
		sBumpChannel = shaderp->enableTexture(LLShaderMgr::BUMP_MAP);
		gGL.getTexUnit(sDiffuseChannel)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.getTexUnit(sBumpChannel)->unbind(LLTexUnit::TT_TEXTURE);

		U32 type = rigged ? PASS_BUMP_RIGGED : PASS_BUMP;
		LLCullResult::drawinfo_list_t& dlist = gPipeline.getRenderMap(type);
		for (U32 i = 0, count = dlist.size(); i < count; )
		{
			LLDrawInfo* paramsp = dlist[i++];

			// Draw info cache prefetching optimization.
			if (i < count)
			{
				_mm_prefetch((char*)dlist[i]->mVertexBuffer.get(),
							 _MM_HINT_NTA);
				if (i + 1 < count)
				{
					_mm_prefetch((char*)dlist[i + 1], _MM_HINT_NTA);
				}
			}

			shaderp->setMinimumAlpha(paramsp->mAlphaMaskCutoff);
			bindBumpMap(*paramsp, sBumpChannel);
			if (rigged)
			{
				if (paramsp->mAvatar && paramsp->mSkinInfo &&
					(paramsp->mAvatar != last_avatarp ||
					 paramsp->mSkinInfo->mHash != last_hash))
				{
#if 0
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
				pushBumpBatch(*paramsp, mask | LLVertexBuffer::MAP_WEIGHT4,
							  true, false);
			}
			else
			{
				pushBumpBatch(*paramsp, mask, true, false);
			}
		}

		shaderp->disableTexture(LLShaderMgr::DIFFUSE_MAP);
		shaderp->disableTexture(LLShaderMgr::BUMP_MAP);
		shaderp->unbind();
		unit0->activate();
	}

	mShiny = false;
}

void LLDrawPoolBump::renderPostDeferred(S32 pass)
{
	// Skip rigged pass when rendering HUDs
	U32 num_passes = LLPipeline::sRenderingHUDs ? 1 : 2;
	// Two passes: static and rigged
	for (U32 rigged = 0; rigged < num_passes; ++rigged)
	{
		mRigged = rigged;

		// Render shiny
		beginFullbrightShiny();
		renderFullbrightShiny();
		endFullbrightShiny();

		// Render bump
		beginBump();
		renderBump(PASS_POST_BUMP);
		endBump();
	}
}

////////////////////////////////////////////////////////////////
// List of bump-maps created from other textures.

void LLBumpImageList::destroyGL()
{
	// These will be re-populated on-demand
	if (!mBrightnessEntries.empty() && !mDarknessEntries.empty())
	{
		llinfos << "Clearing dynamic bumpmaps." << llendl;
		mBrightnessEntries.clear();
		mDarknessEntries.clear();
	}
	LLStandardBumpmap::shutdown();
}

void LLBumpImageList::restoreGL()
{
	if (!gTextureList.isInitialized())
	{
		// Safe to return here because bump images will be reloaded during
		// initialization later.
		return;
	}

	LLStandardBumpmap::init();
	// Images will be recreated as they are needed.
}

// Note: Does nothing for entries in gStandardBumpmapList that are not actually
// standard bump images (e.g. none, brightness, and darkness)
void LLBumpImageList::addTextureStats(U8 bump, const LLUUID& base_image_id,
									  F32 virtual_size)
{
	bump &= TEM_BUMP_MASK;
	LLViewerFetchedTexture* bump_image = gStandardBumpmapList[bump].mImage;
	if (bump_image)
	{
		bump_image->addTextureStats(virtual_size);
	}
}

void LLBumpImageList::updateImages()
{
	for (bump_image_map_t::iterator iter = mBrightnessEntries.begin(),
									end = mBrightnessEntries.end();
		 iter != end; )
	{
		bump_image_map_t::iterator curiter = iter++;
		LLViewerTexture* image = curiter->second;
		if (image)
		{
			bool destroy = true;
			if (image->hasGLTexture())
			{
				if (image->getBoundRecently())
				{
					destroy = false;
				}
				else
				{
					image->destroyGLTexture();
				}
			}

			if (destroy)
			{
				// Deletes the image thanks to reference counting
				mBrightnessEntries.erase(curiter);
			}
		}
	}

	for (bump_image_map_t::iterator iter = mDarknessEntries.begin(),
									end = mDarknessEntries.end();
		 iter != end; )
	{
		bump_image_map_t::iterator curiter = iter++;
		LLViewerTexture* image = curiter->second;
		if (image)
		{
			bool destroy = true;
			if (image->hasGLTexture())
			{
				if (image->getBoundRecently())
				{
					destroy = false;
				}
				else
				{
					image->destroyGLTexture();
				}
			}

			if (destroy)
			{
				// Deletes the image thanks to reference counting
				mDarknessEntries.erase(curiter);
			}
		}
	}
}

// Note: the caller SHOULD NOT keep the pointer that this function returns.
// It may be updated as more data arrives.
LLViewerTexture* LLBumpImageList::getBrightnessDarknessImage(LLViewerFetchedTexture* src_image,
															 U8 bump_code)
{
	llassert(bump_code == BE_BRIGHTNESS || bump_code == BE_DARKNESS);

	LLViewerTexture* bump = NULL;

	bump_image_map_t* entries_list = NULL;
	void (*callback_func)(bool success, LLViewerFetchedTexture* src_vi,
						  LLImageRaw* src, LLImageRaw* aux_src,
						  S32 discard_level, bool is_final,
						  void* userdata) = NULL;

	switch (bump_code)
	{
		case BE_BRIGHTNESS:
			entries_list = &mBrightnessEntries;
			callback_func = LLBumpImageList::onSourceBrightnessLoaded;
			break;

		case BE_DARKNESS:
			entries_list = &mDarknessEntries;
			callback_func = LLBumpImageList::onSourceDarknessLoaded;
			break;

		default:
			llassert(false);
			return NULL;
	}

	bump_image_map_t::iterator iter = entries_list->find(src_image->getID());
	if (iter != entries_list->end() && iter->second.notNull())
	{
		bump = iter->second;
	}
	else
	{
		(*entries_list)[src_image->getID()] =
			LLViewerTextureManager::getLocalTexture(true);
		// In case callback was called immediately and replaced the image:
		bump = (*entries_list)[src_image->getID()];
	}

	if (!src_image->hasCallbacks())
	{
		// If image has no callbacks but resolutions do not match, trigger
		// raw image loaded callback again
		if (src_image->getWidth() != bump->getWidth() ||
#if 0
			(LLPipeline::sRenderDeferred && bump->getComponents() != 4) ||
#endif
			src_image->getHeight() != bump->getHeight())
		{
			src_image->setBoostLevel(LLGLTexture::BOOST_BUMP);
			src_image->setLoadedCallback(callback_func, 0, true, false,
										 new LLUUID(src_image->getID()), NULL);
			src_image->forceToSaveRawImage(0);
		}
	}

	return bump;
}

//static
void LLBumpImageList::onSourceBrightnessLoaded(bool success,
											   LLViewerFetchedTexture* src_vi,
											   LLImageRaw* src,
											   LLImageRaw* aux_src,
											   S32 discard_level,
											   bool is_final,
											   void* userdata)
{
	LLUUID* source_asset_id = (LLUUID*)userdata;
	LLBumpImageList::onSourceLoaded(success, src_vi, src, *source_asset_id,
									BE_BRIGHTNESS);
	if (is_final)
	{
		delete source_asset_id;
	}
}

//static
void LLBumpImageList::onSourceDarknessLoaded(bool success,
											 LLViewerFetchedTexture* src_vi,
											 LLImageRaw* src,
											 LLImageRaw* aux_src,
											 S32 discard_level,
											 bool is_final,
											 void* userdata)
{
	LLUUID* source_asset_id = (LLUUID*)userdata;
	LLBumpImageList::onSourceLoaded(success, src_vi, src, *source_asset_id,
									BE_DARKNESS);
	if (is_final)
	{
		delete source_asset_id;
	}
}

void LLBumpImageList::onSourceStandardLoaded(bool success,
											 LLViewerFetchedTexture* src_vi,
											 LLImageRaw* src,
											 LLImageRaw* aux_src,
											 S32 discard_level,
											 bool, void*)
{
	if (success && LLPipeline::sRenderDeferred)
	{
		LL_FAST_TIMER(FTM_BUMP_SOURCE_STANDARD_LOADED);
		LLPointer<LLImageRaw> nrm_image = new LLImageRaw(src->getWidth(),
														 src->getHeight(), 4);
		{
			LL_FAST_TIMER(FTM_BUMP_GEN_NORMAL);
			generateNormalMapFromAlpha(src, nrm_image);
		}
		src_vi->setExplicitFormat(GL_RGBA, GL_RGBA);
		{
			LL_FAST_TIMER(FTM_BUMP_CREATE_TEXTURE);
			src_vi->createGLTexture(src_vi->getDiscardLevel(), nrm_image);
		}
	}
}

void LLBumpImageList::generateNormalMapFromAlpha(LLImageRaw* src,
												 LLImageRaw* nrm_image)
{
	U8* nrm_data = nrm_image->getData();
	S32 resx = src->getWidth();
	S32 resy = src->getHeight();

	U8* src_data = src->getData();
	S32 src_cmp = src->getComponents();

	static LLCachedControl<F32> norm_scale(gSavedSettings,
										   "RenderNormalMapScale");
	// Generate normal map from pseudo-heightfield
	LLVector3 up, down, left, right, norm;
	up.mV[VY] = -norm_scale;
	down.mV[VY] = norm_scale;
	left.mV[VX] = -norm_scale;
	right.mV[VX] = norm_scale;
	static const LLVector3 offset(0.5f, 0.5f ,0.5f);
	U32 idx = 0;
	for (S32 j = 0; j < resy; ++j)
	{
		for (S32 i = 0; i < resx; ++i)
		{
			S32 rx = (i + 1) % resx;
			S32 ry = (j + 1) % resy;

			S32 lx = (i - 1) % resx;
			if (lx < 0)
			{
				lx += resx;
			}

			S32 ly = (j - 1) % resy;
			if (ly < 0)
			{
				ly += resy;
			}

			F32 ch = (F32)src_data[(j * resx + i) * src_cmp + src_cmp - 1];

			right.mV[VZ] = (F32)src_data[(j * resx + rx + 1) * src_cmp - 1] - ch;
			left.mV[VZ] = (F32)src_data[(j * resx + lx + 1) * src_cmp - 1] - ch;
			up.mV[VZ] = (F32)src_data[(ly * resx + i + 1) * src_cmp - 1] - ch;
			down.mV[VZ] = (F32)src_data[(ry * resx + i + 1) * src_cmp - 1] - ch;

			norm = right % down + down % left + left % up + up % right;
			norm.normalize();
			norm *= 0.5f;
			norm += offset;

			idx = (j * resx + i) * 4;
			nrm_data[idx] = (U8)(norm.mV[0] * 255);
			nrm_data[idx + 1] = (U8)(norm.mV[1] * 255);
			nrm_data[idx + 2] = (U8)(norm.mV[2] * 255);
			nrm_data[idx + 3] = src_data[(j * resx + i) * src_cmp + src_cmp - 1];
		}
	}
}

//static
void LLBumpImageList::onSourceLoaded(bool success, LLViewerTexture* src_vi,
									 LLImageRaw* src, LLUUID& source_asset_id,
									 EBumpEffect bump_code)
{
	LL_FAST_TIMER(FTM_BUMP_SOURCE_LOADED);

	if (!success)
	{
		return;
	}

	if (!src || !src->getData())	// Paranoia
	{
		llwarns << "No image data for bump texture: " << source_asset_id
				<< llendl;
		return;
	}

	bump_image_map_t& entries_list(bump_code == BE_BRIGHTNESS ?
										gBumpImageList.mBrightnessEntries :
										gBumpImageList.mDarknessEntries);
	bump_image_map_t::iterator iter = entries_list.find(source_asset_id);
	bool needs_update = iter == entries_list.end() || iter->second.isNull() ||
						iter->second->getWidth() != src->getWidth() ||
						iter->second->getHeight() != src->getHeight();
	if (needs_update)
	{
		// If bump not cached yet or has changed resolution...
		LL_FAST_TIMER(FTM_BUMP_SOURCE_ENTRIES_UPDATE);
		// Make sure an entry exists for this image
		iter = entries_list.emplace(src_vi->getID(),
									LLViewerTextureManager::getLocalTexture(true)).first;
	}
	else
	{
		// Nothing to do
		return;
	}

	LLPointer<LLImageRaw> dst_image = new LLImageRaw(src->getWidth(),
													 src->getHeight(), 1);
	if (dst_image.isNull())
	{
		llwarns << "Could not create a new raw image for bump: "
				<< src_vi->getID() << ". Out of memory !" << llendl;
		return;
	}

	U8* dst_data = dst_image->getData();
	S32 dst_data_size = dst_image->getDataSize();

	U8* src_data = src->getData();
	S32 src_data_size = src->getDataSize();

	S32 src_components = src->getComponents();

	// Convert to luminance and then scale and bias that to get ready for
	// embossed bump mapping (0-255 maps to 127-255).

	// Convert to fixed point so we don't have to worry about precision or
	// clamping.
	constexpr S32 FIXED_PT = 8;
	constexpr S32 R_WEIGHT = S32(0.2995f * F32(1 << FIXED_PT));
	constexpr S32 G_WEIGHT = S32(0.5875f * F32(1 << FIXED_PT));
	constexpr S32 B_WEIGHT = S32(0.1145f * F32(1 << FIXED_PT));

	S32 minimum = 255;
	S32 maximum = 0;

	switch (src_components)
	{
		case 1:
		case 2:
		{
			LL_FAST_TIMER(FTM_BUMP_SOURCE_MIN_MAX);
			if (src_data_size == dst_data_size * src_components)
			{
				for (S32 i = 0, j = 0; i < dst_data_size;
					 i++, j += src_components)
				{
					dst_data[i] = src_data[j];
					if (dst_data[i] < minimum)
					{
						minimum = dst_data[i];
					}
					if (dst_data[i] > maximum)
					{
						maximum = dst_data[i];
					}
				}
			}
			else
			{
				llassert(false);
				dst_image->clear();
			}
			break;
		}

		case 3:
		case 4:
		{
			LL_FAST_TIMER(FTM_BUMP_SOURCE_RGB2LUM);
			if (src_data_size == dst_data_size * src_components)
			{
				for (S32 i = 0, j = 0; i < dst_data_size;
					 i++, j+= src_components)
				{
					// RGB to luminance
					dst_data[i] = (R_WEIGHT * src_data[j] +
								   G_WEIGHT * src_data[j + 1] +
								   B_WEIGHT * src_data[j + 2]) >> FIXED_PT;
					if (dst_data[i] < minimum)
					{
						minimum = dst_data[i];
					}
					if (dst_data[i] > maximum)
					{
						maximum = dst_data[i];
					}
				}
			}
			else
			{
				llassert(false);
				dst_image->clear();
			}
			break;
		}

		default:
			llassert(false);
			dst_image->clear();
	}

	if (maximum > minimum)
	{
		LL_FAST_TIMER(FTM_BUMP_SOURCE_RESCALE);
		U8 bias_and_scale_lut[256];
		F32 twice_one_over_range = 2.f / (maximum - minimum);
		S32 i;
		// Advantage: exaggerates the effect in midrange. Disadvantage: clamps
		// at the extremes.
		constexpr F32 ARTIFICIAL_SCALE = 2.f;
		if (bump_code == BE_DARKNESS)
		{
			for (i = minimum; i <= maximum; ++i)
			{
				F32 minus_one_to_one = F32(maximum - i) *
									   twice_one_over_range - 1.f;
				bias_and_scale_lut[i] = llclampb(ll_round(127 *
														  minus_one_to_one *
														  ARTIFICIAL_SCALE +
														  128));
			}
		}
		else
		{
			for (i = minimum; i <= maximum; ++i)
			{
				F32 minus_one_to_one = F32(i - minimum) *
									   twice_one_over_range - 1.f;
				bias_and_scale_lut[i] = llclampb(ll_round(127 *
														  minus_one_to_one *
														  ARTIFICIAL_SCALE +
														  128));
			}
		}

		for (i = 0; i < dst_data_size; ++i)
		{
			dst_data[i] = bias_and_scale_lut[dst_data[i]];
		}
	}

	//---------------------------------------------------
	// Immediately assign bump to a smart pointer in case some local smart
	// pointer accidentally releases it.
	LLPointer<LLViewerTexture> bump = iter->second;

	static LLCachedControl<bool> use_worker(gSavedSettings,
											"GLWorkerUseForBumpmap");
	bool can_queue = use_worker && LLImageGLThread::sEnabled && gMainloopWorkp;

	if (!LLPipeline::sRenderDeferred)
	{
		LL_FAST_TIMER(FTM_BUMP_SOURCE_CREATE);

		bump->setExplicitFormat(GL_ALPHA8, GL_ALPHA);
		auto texq = can_queue ? gImageQueuep.lock() : nullptr;
		if (texq)
		{
			// Dispatch creation to background thread
			LLImageRaw* dst_ptr = dst_image.get();
			LLViewerTexture* bump_ptr = bump.get();
			dst_ptr->ref();
			bump_ptr->ref();
			texq->post([=]()
					   {
							bump_ptr->createGLTexture(0, dst_ptr);
							bump_ptr->unref();
							dst_ptr->unref();
					   });
		}
		else
		{
			bump->createGLTexture(0, dst_image);
		}
	}
	else	// Convert to normal map
	{
		LLImageGL* img = bump->getGLImage();
		LLImageRaw* dst_ptr = dst_image.get();
		LLGLTexture* bump_ptr = bump.get();

		dst_ptr->ref();
		img->ref();
		bump_ptr->ref();
		auto create_func = [=]()
		{
			img->setUseMipMaps(true);
			// Upload dst_image to GPU (greyscale in red channel)
			img->setExplicitFormat(GL_RED, GL_RED);
			bump_ptr->createGLTexture(0, dst_ptr);
			dst_ptr->unref();
		};
		
		static LLCachedControl<F32> norm_scale(gSavedSettings,
											   "RenderNormalMapScale");
		auto generate_func = [=]()
		{
			// Allocate an empty RGBA texture at "tex_name" the same size as
			// bump. Note: bump will still point at GPU copy of dst_image.
			bump_ptr->setExplicitFormat(GL_RGBA, GL_RGBA);
			U32 tex_name;
			img->createGLTexture(0, NULL, false, 0, true, &tex_name);

			// Point render target at empty buffer
			sRenderTarget.setColorAttachment(img, tex_name);

			// Generate normal map in empty texture
			{
				sRenderTarget.bindTarget();

				LLGLDepthTest depth(GL_FALSE);
				LLGLDisable cull(GL_CULL_FACE);
				LLGLDisable blend(GL_BLEND);
				gGL.setColorMask(true, true);
				gNormalMapGenProgram.bind();

				static LLStaticHashedString sNormScale("norm_scale");
				static LLStaticHashedString sStepX("stepX");
				static LLStaticHashedString sStepY("stepY");
				gNormalMapGenProgram.uniform1f(sNormScale, norm_scale);
				gNormalMapGenProgram.uniform1f(sStepX,
											   1.f / bump_ptr->getWidth());
				gNormalMapGenProgram.uniform1f(sStepY,
											   1.f / bump_ptr->getHeight());

				gGL.getTexUnit(0)->bind(bump);

				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2f(0.f, 0.f);

				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex2f(0.f, 1.f);

				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex2f(1.f, 0.f);
				
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex2f(1.f, 1.f);
				
				gGL.end(true);

				gNormalMapGenProgram.unbind();

				sRenderTarget.flush();
				sRenderTarget.releaseColorAttachment();
			}

			// Point bump at normal map and free GPU copy of dst_image
			img->syncTexName(tex_name);

			// Generate mipmap
			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bind(img);
			glGenerateMipmap(GL_TEXTURE_2D);
			unit0->disable();

			bump_ptr->unref();
			img->unref();
		};

		// If possible, dispatch the texture upload to the background thread,
		// issue GPU commands to the generate normal map on the main thread.
		if (!can_queue ||
			!gMainloopWorkp->postTo(gImageQueuep, create_func, generate_func))
		{
			// If not possible or failed, immediately upload the texture and
			// generate the normal map
			{
				LL_FAST_TIMER(FTM_BUMP_SOURCE_CREATE);
				create_func();
			}
			{
				LL_FAST_TIMER(FTM_BUMP_SOURCE_CREATE);
				generate_func();
			}
		}
	}

	iter->second = std::move(bump); // Derefs (and deletes) old image
}

// For the EE renderer only
void LLDrawPoolBump::renderBump(U32 type, U32 mask)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	if (mRigged)
	{
		// Nudge type enum and include skinweights for rigged pass
		++type;
		mask |= LLVertexBuffer::MAP_WEIGHT4;
	}

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (!bindBumpMap(*paramsp))
		{
			continue;
		}

		if (mRigged && paramsp->mAvatar && paramsp->mSkinInfo &&
			(paramsp->mAvatar != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
			last_avatarp = paramsp->mAvatar;
			last_hash = paramsp->mSkinInfo->mHash;
		}

		pushBumpBatch(*paramsp, mask, false);
	}
}

//virtual
void LLDrawPoolBump::pushBatch(LLDrawInfo& params, U32 mask, bool texture,
							   bool batch_textures)
{
	if (gUsePBRShaders)
	{
		// In LL's PBR code, pushBatch() is not a virtual method any more (the
		// LLDrawPoolBump override was renamed as a non virtual pushBumpBatch()
		// method instead), so when pushBatch() gets called on a bump draw
		// pool, we must re-route it to the underlying LLRenderPass:pushBatch()
		// method. This makes it compatible at the API level for both EE and
		// PBR despite the virtual/non-virtual difference. HB
		LLRenderPass::pushBatch(params, mask, texture, batch_textures);
	}
	else
	{
		pushBumpBatch(params, mask, texture, batch_textures);
	}
}

void LLDrawPoolBump::pushBumpBatch(LLDrawInfo& params, U32 mask, bool texture,
								   bool batch_textures)
{
	applyModelMatrix(params);

	bool tex_setup = false;

	U32 count = 0;
	if (batch_textures && (count = params.mTextureList.size()) > 1)
	{
		for (U32 i = 0; i < count; ++i)
		{
			const LLPointer<LLViewerTexture>& tex = params.mTextureList[i];
			if (tex.notNull())
			{
				gGL.getTexUnit(i)->bindFast(tex);
			}
		}
	}
	else
	{
		// Not batching textures or batch has only 1 texture: might need a
		// texture matrix
		if (params.mTextureMatrix)
		{
			if (mShiny)
			{
				gGL.getTexUnit(0)->activate();
				gGL.matrixMode(LLRender::MM_TEXTURE);
			}
			else
			{
				gGL.getTexUnit(0)->activate();
				gGL.matrixMode(LLRender::MM_TEXTURE);
				gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
				++gPipeline.mTextureMatrixOps;
			}

			gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
			++gPipeline.mTextureMatrixOps;

			tex_setup = true;
		}

		if (mShiny && mShaderLevel > 1 && texture)
		{
			if (params.mTexture.notNull())
			{
				gGL.getTexUnit(sDiffuseChannel)->bindFast(params.mTexture);
			}
			else
			{
				gGL.getTexUnit(sDiffuseChannel)->unbind(LLTexUnit::TT_TEXTURE);
			}
		}
	}

	// Note: mask is ignored for the PBR renderer
	params.mVertexBuffer->setBufferFast(mask);
	params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
									params.mEnd, params.mCount,
									params.mOffset);

	if (tex_setup)
	{
		if (mShiny)
		{
			gGL.getTexUnit(0)->activate();
		}
		else
		{
			gGL.getTexUnit(0)->activate();
			gGL.matrixMode(LLRender::MM_TEXTURE);
		}
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

// For the PBR renderer only
void LLDrawPoolBump::pushBumpBatches(U32 type)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	if (mRigged)
	{
		// Nudge type enum and include skin weights for rigged pass.
		++type;
	}

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (!bindBumpMap(*paramsp))
		{
			continue;
		}

		if (mRigged && paramsp->mAvatar && paramsp->mSkinInfo &&
			(paramsp->mAvatar != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
			last_avatarp = paramsp->mAvatar;
			last_hash = paramsp->mSkinInfo->mHash;
		}

		pushBumpBatch(*paramsp, 0, false);
	}
}

// Renders invisiprims
void LLDrawPoolInvisible::render(S32)
{
	LL_FAST_TIMER(FTM_RENDER_INVISIBLE);

	bool has_shaders = gPipeline.shadersLoaded();
	if (has_shaders)
	{
		gOcclusionProgram.bind();
	}

	glStencilMask(0);
	gGL.setColorMask(false, false);
	pushBatches(PASS_INVISIBLE, VERTEX_DATA_MASK, false);
	gGL.setColorMask(true, false);	// false for alpha mask in direct rendering
	glStencilMask(0xFFFFFFFF);

	if (has_shaders)
	{
		gOcclusionProgram.unbind();
	}
}

void LLDrawPoolInvisible::renderDeferred(S32 pass)
{
	LL_FAST_TIMER(FTM_RENDER_INVISIBLE);

	// *TODO: since we kept the stencil for EE, see if we can re-implement this
	// in PBR rendering mode. HB
	static LLCachedControl<bool> deferred_invisible(gSavedSettings,
													"RenderDeferredInvisible");
	if (!deferred_invisible)
	{
		// This MUST be called nevertheless to restore the proper color masks.
		// HB
		gGL.setColorMask(true, true);
		return;
	}

	bool has_shaders = gPipeline.shadersLoaded();
	if (has_shaders)
	{
		gOcclusionProgram.bind();
	}

	glStencilMask(0);
	//glStencilOp(GL_ZERO, GL_KEEP, GL_REPLACE);
	gGL.setColorMask(false, false);
	pushBatches(PASS_INVISIBLE, VERTEX_DATA_MASK, false);
	gGL.setColorMask(true, true);	// true for alpha masking in deferred mode
	//glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0xFFFFFFFF);

	if (has_shaders)
	{
		gOcclusionProgram.unbind();
	}
}
