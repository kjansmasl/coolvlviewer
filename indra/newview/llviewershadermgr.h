/**
 * @file llviewershadermgr.h
 * @brief Viewer Shader Manager
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_VIEWER_SHADER_MGR_H
#define LL_VIEWER_SHADER_MGR_H

#include "llmaterial.h"
#include "llshadermgr.h"

#define LL_DEFERRED_MULTI_LIGHT_COUNT 16

class LLViewerShaderMgr : public LLShaderMgr
{
protected:
	LOG_CLASS(LLViewerShaderMgr);

private:
	// Use createInstance() and releaseInstance()
	LLViewerShaderMgr();
	~LLViewerShaderMgr() override;

	void init();

public:
	static void createInstance();
	static void releaseInstance();

	void initAttribsAndUniforms() override;
	void setShaders();
	void unloadShaders();
	bool loadBasicShaders();
	bool loadShadersEffects();
	bool loadShadersDeferred();
	bool loadShadersObject();
	bool loadShadersAvatar();
	bool loadShadersEnvironment();
	bool loadShadersWater();
	bool loadShadersInterface();
	bool loadShadersWindLight();

	enum EShaderClass
	{
		SHADER_LIGHTING,
		SHADER_OBJECT,
		SHADER_AVATAR,
		SHADER_ENVIRONMENT,
		SHADER_INTERFACE,
		SHADER_EFFECT,
		SHADER_WINDLIGHT,
		SHADER_WATER,
		SHADER_DEFERRED,
		SHADER_COUNT
	};

	LL_INLINE S32 getShaderLevel(S32 type) const
	{
		return mShaderLevel[type];
	}

	// Simple model of forward iterator
	// http://www.sgi.com/tech/stl/ForwardIterator.html
	class shader_iter
	{
	private:
		friend bool operator==(shader_iter const& a, shader_iter const& b);
		friend bool operator!=(shader_iter const& a, shader_iter const& b);

		typedef std::vector<LLGLSLShader*>::const_iterator base_iter_t;

	public:
		shader_iter() = default;

		LL_INLINE shader_iter(base_iter_t iter)
		:	mIter(iter)
		{
		}

		LL_INLINE LLGLSLShader& operator*() const
		{
			return **mIter;
		}

		LL_INLINE LLGLSLShader* operator->() const
		{
			return *mIter;
		}

		LL_INLINE shader_iter& operator++()
		{
			++mIter;
			return *this;
		}

		LL_INLINE shader_iter operator++(int)
		{
			return mIter++;
		}

	private:
		base_iter_t mIter;
	};

	typedef std::vector<LLGLSLShader*> shaders_list_t;

	LL_INLINE const shaders_list_t& getEnvShadersList() const
	{
		return mShaderList;
	}

	LL_INLINE const std::string& getShaderDirPrefix() const override
	{
		return mShaderDirPrefix;
	}

	void updateShaderUniforms(LLGLSLShader* shader) override;

private:
	std::string					mShaderDirPrefix;
	// The list of shaders we need to propagate parameters to.
	std::vector<LLGLSLShader*>	mShaderList;
	// This is the cached value of LLGLSLShader::sIndexedTextureChannels during
	// shaders creation (just there to make the code less verbose; not speeed
	// critical). HB
	S32							mTextureChannels;

public:
	S32							mMaxAvatarShaderLevel;
	std::vector<S32>			mShaderLevel;

	static bool					sInitialized;
	static bool					sSkipReload;
	static bool					sHasFXAA;
	static bool					sHasSMAA;
	static bool					sHasCAS;
	static bool					sHasIrrandiance;
	// Set when PBR shaders are loaded, depending on GL capability and settings
	static bool					sHasRP;	// Reflection probes
};

LL_INLINE bool operator==(LLViewerShaderMgr::shader_iter const& a,
						  LLViewerShaderMgr::shader_iter const& b)
{
	return a.mIter == b.mIter;
}

LL_INLINE bool operator!=(LLViewerShaderMgr::shader_iter const& a,
						  LLViewerShaderMgr::shader_iter const& b)
{
	return a.mIter != b.mIter;
}

extern LLViewerShaderMgr* gViewerShaderMgrp;

extern LLVector4 gShinyOrigin;

// Utility shaders
extern LLGLSLShader gOcclusionProgram;
extern LLGLSLShader gOcclusionCubeProgram;
extern LLGLSLShader gGlowCombineProgram;
extern LLGLSLShader gReflectionMipProgram;						// PBR only
extern LLGLSLShader gGaussianProgram;							// PBR only
extern LLGLSLShader gRadianceGenProgram;						// PBR only
extern LLGLSLShader gIrradianceGenProgram;						// PBR only
extern LLGLSLShader gSplatTextureRectProgram;					// EE/WL only
extern LLGLSLShader gGlowCombineFXAAProgram;
extern LLGLSLShader gDebugProgram;
extern LLGLSLShader gClipProgram;
extern LLGLSLShader gDownsampleDepthProgram;					// EE/WL only
extern LLGLSLShader gDownsampleDepthRectProgram;				// EE/WL only
extern LLGLSLShader gAlphaMaskProgram;
extern LLGLSLShader gBenchmarkProgram;
extern LLGLSLShader gOneTextureNoColorProgram;
extern LLGLSLShader gReflectionProbeDisplayProgram;				// PBR only
extern LLGLSLShader gCopyProgram;								// PBR only
extern LLGLSLShader gCopyDepthProgram;							// PBR only

// Object shaders
extern LLGLSLShader gObjectSimpleProgram;						// EE/WL only
extern LLGLSLShader gObjectSimpleImpostorProgram;				// EE/WL only
extern LLGLSLShader gObjectPreviewProgram;
extern LLGLSLShader gPhysicsPreviewProgram;						// PBR only
extern LLGLSLShader gObjectSimpleAlphaMaskProgram;				// EE/WL only
extern LLGLSLShader gObjectSimpleWaterProgram;					// EE/WL only
extern LLGLSLShader gObjectSimpleWaterAlphaMaskProgram;			// EE/WL only
extern LLGLSLShader gObjectSimpleNonIndexedTexGenProgram;		// EE/WL only
extern LLGLSLShader gObjectSimpleNonIndexedTexGenWaterProgram;	// EE/WL only
extern LLGLSLShader gObjectAlphaMaskNonIndexedProgram;			// EE/WL only
extern LLGLSLShader gObjectAlphaMaskNonIndexedWaterProgram;		// EE/WL only
extern LLGLSLShader gObjectAlphaMaskNoColorProgram;
extern LLGLSLShader gObjectAlphaMaskNoColorWaterProgram;		// EE/WL only
extern LLGLSLShader gObjectFullbrightProgram;					// EE/WL only
extern LLGLSLShader gObjectFullbrightWaterProgram;				// EE/WL only
extern LLGLSLShader gObjectFullbrightNoColorWaterProgram;		// EE/WL only
extern LLGLSLShader gObjectEmissiveProgram;						// EE/WL only
extern LLGLSLShader gObjectEmissiveWaterProgram;				// EE/WL only
extern LLGLSLShader gObjectFullbrightAlphaMaskProgram;			// EE/WL only
extern LLGLSLShader gObjectFullbrightWaterAlphaMaskProgram;		// EE/WL only
extern LLGLSLShader gObjectBumpProgram;
extern LLGLSLShader gTreeProgram;								// EE/WL only
extern LLGLSLShader gTreeWaterProgram;							// EE/WL only

extern LLGLSLShader gObjectFullbrightShinyProgram;				// EE/WL only
extern LLGLSLShader gObjectFullbrightShinyWaterProgram;			// EE/WL only

extern LLGLSLShader gObjectShinyProgram;						// EE/WL only
extern LLGLSLShader gObjectShinyWaterProgram;					// EE/WL only

// Environment shaders
extern LLGLSLShader gMoonProgram;								// EE/WL only
extern LLGLSLShader gStarsProgram;								// EE/WL only
extern LLGLSLShader gTerrainProgram;							// EE/WL only
extern LLGLSLShader gTerrainWaterProgram;						// EE/WL only
extern LLGLSLShader gWaterProgram;
extern LLGLSLShader gWaterEdgeProgram;
extern LLGLSLShader gUnderWaterProgram;

// Effects Shaders
extern LLGLSLShader gGlowProgram;
extern LLGLSLShader gGlowExtractProgram;
extern LLGLSLShader gPostScreenSpaceReflectionProgram;			// PBR only

// Interface shaders
extern LLGLSLShader gHighlightProgram;
extern LLGLSLShader gHighlightNormalProgram;
extern LLGLSLShader gHighlightSpecularProgram;

// Avatar shader handles
extern LLGLSLShader gAvatarProgram;
extern LLGLSLShader gAvatarWaterProgram;						// EE/WL only
extern LLGLSLShader gAvatarEyeballProgram;
extern LLGLSLShader gImpostorProgram;

// WindLight shader handles (EE/WL only)
extern LLGLSLShader gWLSkyProgram;
extern LLGLSLShader gWLCloudProgram;
extern LLGLSLShader gWLSunProgram;
extern LLGLSLShader gWLMoonProgram;

// Post processing shader handles (EE/WL only, for now)
extern LLGLSLShader gPostSMAAEdgeDetect[4];
extern LLGLSLShader gPostSMAABlendWeights[4];
extern LLGLSLShader gPostSMAANeighborhoodBlend[4];
extern LLGLSLShader gPostCASProgram;

// Deferred rendering shaders
extern LLGLSLShader gDeferredImpostorProgram;
extern LLGLSLShader gDeferredWaterProgram;						// EE/WL only
extern LLGLSLShader gDeferredUnderWaterProgram;					// EE/WL only
extern LLGLSLShader gDeferredHighlightProgram;					// PBR only
extern LLGLSLShader gDeferredDiffuseProgram;
extern LLGLSLShader gDeferredDiffuseAlphaMaskProgram;
extern LLGLSLShader gDeferredNonIndexedDiffuseAlphaMaskProgram;
extern LLGLSLShader gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
extern LLGLSLShader gDeferredBumpProgram;
extern LLGLSLShader gDeferredTerrainProgram;
extern LLGLSLShader gDeferredTerrainWaterProgram;				// EE/WL only
extern LLGLSLShader gDeferredTreeProgram;
extern LLGLSLShader gDeferredTreeShadowProgram;
extern LLGLSLShader gDeferredLightProgram;
extern LLGLSLShader gDeferredMultiLightProgram[LL_DEFERRED_MULTI_LIGHT_COUNT];
extern LLGLSLShader gDeferredSpotLightProgram;
extern LLGLSLShader gDeferredMultiSpotLightProgram;
extern LLGLSLShader gDeferredSunProgram;
extern LLGLSLShader gHazeProgram;								// PBR only
extern LLGLSLShader gHazeWaterProgram;							// PBR only
extern LLGLSLShader gDeferredBlurLightProgram;
extern LLGLSLShader gDeferredAvatarProgram;
extern LLGLSLShader gDeferredSoftenProgram;
extern LLGLSLShader gDeferredSoftenWaterProgram;				// EE/WL only
extern LLGLSLShader gDeferredShadowProgram;
extern LLGLSLShader gDeferredShadowCubeProgram;
extern LLGLSLShader gDeferredShadowAlphaMaskProgram;
extern LLGLSLShader gDeferredShadowGLTFAlphaMaskProgram;		// PBR only
extern LLGLSLShader gDeferredShadowGLTFAlphaBlendProgram;		// PBR only
extern LLGLSLShader gDeferredShadowFullbrightAlphaMaskProgram;
extern LLGLSLShader gDeferredPostProgram;
extern LLGLSLShader gDeferredCoFProgram;
extern LLGLSLShader gDeferredDoFCombineProgram;
extern LLGLSLShader gFXAAProgram[4];
extern LLGLSLShader gDeferredPostNoDoFProgram;
extern LLGLSLShader gDeferredPostGammaCorrectProgram;
extern LLGLSLShader gNoPostGammaCorrectProgram;					// PBR only
extern LLGLSLShader gLegacyPostGammaCorrectProgram;				// PBR only
extern LLGLSLShader gExposureProgram;							// PBR only
extern LLGLSLShader gLuminanceProgram;							// PBR only
extern LLGLSLShader gDeferredAvatarShadowProgram;
extern LLGLSLShader gDeferredAvatarAlphaShadowProgram;
extern LLGLSLShader gDeferredAvatarAlphaMaskShadowProgram;
extern LLGLSLShader gDeferredAlphaProgram;
extern LLGLSLShader gHUDAlphaProgram;							// PBR only
extern LLGLSLShader gDeferredAlphaImpostorProgram;
extern LLGLSLShader gDeferredFullbrightProgram;
extern LLGLSLShader gDeferredFullbrightAlphaMaskProgram;
extern LLGLSLShader gDeferredFullbrightAlphaMaskAlphaProgram;	// PBR only
extern LLGLSLShader gHUDFullbrightProgram;						// PBR only
extern LLGLSLShader gHUDFullbrightAlphaMaskProgram;				// PBR only
extern LLGLSLShader gHUDFullbrightAlphaMaskAlphaProgram;		// PBR only
extern LLGLSLShader gDeferredAlphaWaterProgram;					// EE/WL only
extern LLGLSLShader gDeferredFullbrightWaterProgram;			// EE/WL only
extern LLGLSLShader gDeferredFullbrightAlphaMaskWaterProgram;	// EE/WL only
extern LLGLSLShader gDeferredEmissiveProgram;
extern LLGLSLShader gDeferredAvatarEyesProgram;
extern LLGLSLShader gDeferredAvatarAlphaProgram;
extern LLGLSLShader gDeferredWLSkyProgram;
extern LLGLSLShader gDeferredWLCloudProgram;
extern LLGLSLShader gDeferredWLSunProgram;
extern LLGLSLShader gDeferredWLMoonProgram;
extern LLGLSLShader gDeferredStarProgram;
extern LLGLSLShader gDeferredFullbrightShinyProgram;
extern LLGLSLShader gHUDFullbrightShinyProgram;					// PBR only
extern LLGLSLShader gNormalMapGenProgram;
extern LLGLSLShader gDeferredGenBrdfLutProgram;					// PBR only
extern LLGLSLShader gDeferredBufferVisualProgram;				// PBR only

// Deferred materials shaders
extern LLGLSLShader gDeferredMaterialProgram[LLMaterial::SHADER_COUNT * 2];
extern LLGLSLShader gDeferredMaterialWaterProgram[LLMaterial::SHADER_COUNT * 2]; // EE/WL only
extern LLGLSLShader gHUDPBROpaqueProgram;						// PBR only
extern LLGLSLShader gPBRGlowProgram;							// PBR only
extern LLGLSLShader gDeferredPBROpaqueProgram;					// PBR only
extern LLGLSLShader gDeferredPBRAlphaProgram;					// PBR only
extern LLGLSLShader gHUDPBRAlphaProgram;						// PBR only

#endif	// LL_VIEWER_SHADER_MGR_H
