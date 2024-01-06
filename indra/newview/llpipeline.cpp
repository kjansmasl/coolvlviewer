/**
 * @file llpipeline.cpp
 * @brief Rendering pipeline.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "GL/smaa.h"

#include "llpipeline.h"

#include "imageids.h"
#include "llaudioengine.h"			// For sound beacons
#include "llcubemap.h"
#include "llfasttimer.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolterrain.h"
#include "lldrawpooltree.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolwlsky.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llface.h"
#include "llfeaturemanager.h"
#include "hbfloatersoundslist.h"
#include "llfloaterstats.h"
#include "llfloatertelehub.h"
#include "llhudmanager.h"
#include "llhudtext.h"
#include "llmeshrepository.h"
#include "llpanelface.h"
#include "llprefsgraphics.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llstartup.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "lltracker.h"
#include "lltool.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerjoystick.h"
#include "llviewermediafocus.h"
#include "llviewerobjectlist.h"
#include "llvieweroctree.h"			// For ll_setup_cube_vb()
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llvopartgroup.h"
#include "llvosurfacepatch.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llvowater.h"
#include "llvowlsky.h"
#include "llworld.h"

// Set to 0 to disable optimized deferred shaders uniforms setting
#define OPTIMIZED_UNIFORMS 1
// Set to 1 to debug deferred shaders uniforms setting
#define DEBUG_OPTIMIZED_UNIFORMS 0

LLPipeline gPipeline;
const LLMatrix4* gGLLastMatrix = NULL;
bool gShiftFrame = false;

// Cached settings
bool LLPipeline::RenderDeferred;
F32 LLPipeline::RenderDeferredSunWash;
F32 LLPipeline::RenderDeferredDisplayGamma;
U32 LLPipeline::RenderFSAASamples;
S32 LLPipeline::RenderDeferredAAQuality;
bool LLPipeline::RenderDeferredAASharpen;
U32 LLPipeline::RenderResolutionDivisor;
U32 LLPipeline::RenderShadowDetail;
bool LLPipeline::RenderDeferredSSAO;
F32 LLPipeline::RenderShadowResolutionScale;
U32 LLPipeline::RenderLocalLightCount;
bool LLPipeline::RenderDelayCreation;
bool LLPipeline::RenderAnimateRes;
bool LLPipeline::RenderSpotLightsInNondeferred;
LLColor4 LLPipeline::PreviewAmbientColor;
LLColor4 LLPipeline::PreviewDiffuse0;
LLColor4 LLPipeline::PreviewSpecular0;
LLColor4 LLPipeline::PreviewDiffuse1;
LLColor4 LLPipeline::PreviewSpecular1;
LLColor4 LLPipeline::PreviewDiffuse2;
LLColor4 LLPipeline::PreviewSpecular2;
LLVector3 LLPipeline::PreviewDirection0;
LLVector3 LLPipeline::PreviewDirection1;
LLVector3 LLPipeline::PreviewDirection2;
bool LLPipeline::RenderGlow;
F32 LLPipeline::RenderGlowMinLuminance;
F32 LLPipeline::RenderGlowMaxExtractAlpha;
F32 LLPipeline::RenderGlowWarmthAmount;
LLVector3 LLPipeline::RenderGlowLumWeights;
LLVector3 LLPipeline::RenderGlowWarmthWeights;
U32 LLPipeline::RenderGlowResolutionPow;
U32 LLPipeline::RenderGlowIterations;
F32 LLPipeline::RenderGlowWidth;
F32 LLPipeline::RenderGlowStrength;
bool LLPipeline::RenderDepthOfField;
bool LLPipeline::RenderDepthOfFieldInEditMode;
F32 LLPipeline::RenderShadowNoise;
F32 LLPipeline::RenderShadowBlurSize;
F32 LLPipeline::RenderSSAOScale;
U32 LLPipeline::RenderSSAOMaxScale;
F32 LLPipeline::RenderSSAOFactor;
LLVector3 LLPipeline::RenderSSAOEffect;
F32 LLPipeline::RenderShadowBiasError;
F32 LLPipeline::RenderShadowOffset;
F32 LLPipeline::RenderShadowOffsetNoSSAO;
F32 LLPipeline::RenderShadowBias;
F32 LLPipeline::RenderSpotShadowOffset;
F32 LLPipeline::RenderSpotShadowBias;
LLVector3 LLPipeline::RenderShadowGaussian;
F32 LLPipeline::RenderShadowBlurDistFactor;
bool LLPipeline::RenderDeferredAtmospheric;
U32 LLPipeline::RenderWaterReflectionType;
bool LLPipeline::RenderTransparentWater = true;
LLVector3 LLPipeline::RenderShadowClipPlanes;
LLVector3 LLPipeline::RenderShadowOrthoClipPlanes;
F32 LLPipeline::RenderFarClip;
LLVector3 LLPipeline::RenderShadowSplitExponent;
F32 LLPipeline::RenderShadowErrorCutoff;
F32 LLPipeline::RenderShadowFOVCutoff;
bool LLPipeline::CameraOffset;
F32 LLPipeline::CameraMaxCoF;
F32 LLPipeline::CameraDoFResScale;
U32 LLPipeline::RenderAutoHideGeometryMemoryLimit;
F32 LLPipeline::RenderAutoHideSurfaceAreaLimit;
S32 LLPipeline::RenderBufferVisualization;
U32 LLPipeline::RenderScreenSpaceReflectionIterations;
F32 LLPipeline::RenderScreenSpaceReflectionRayStep;
F32 LLPipeline::RenderScreenSpaceReflectionDistanceBias;
F32 LLPipeline::RenderScreenSpaceReflectionDepthRejectBias;
F32 LLPipeline::RenderScreenSpaceReflectionAdaptiveStepMultiplier;
U32 LLPipeline::RenderScreenSpaceReflectionGlossySamples;
bool LLPipeline::RenderScreenSpaceReflections;
bool LLPipeline::sRenderScriptedBeacons = false;
bool LLPipeline::sRenderScriptedTouchBeacons = false;
bool LLPipeline::sRenderPhysicalBeacons = false;
bool LLPipeline::sRenderPermanentBeacons = false;
bool LLPipeline::sRenderCharacterBeacons = false;
bool LLPipeline::sRenderSoundBeacons = false;
bool LLPipeline::sRenderInvisibleSoundBeacons = false;
bool LLPipeline::sRenderParticleBeacons = false;
bool LLPipeline::sRenderMOAPBeacons = false;
bool LLPipeline::sRenderHighlight = true;
bool LLPipeline::sRenderBeacons = false;
bool LLPipeline::sRenderAttachments = false;
bool LLPipeline::sRenderingHUDs = false;
U32	LLPipeline::sRenderByOwner = 0;
U32 LLPipeline::DebugBeaconLineWidth;
LLRender::eTexIndex LLPipeline::sRenderHighlightTextureChannel = LLRender::DIFFUSE_MAP;

LLVector4a LLPipeline::sWaterPlane;

constexpr F32 LIGHT_FADE_TIME = 0.2f;
constexpr F32 ALPHA_BLEND_CUTOFF = 0.598f;
constexpr U32 AUX_VB_MASK = LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_TEXCOORD1;

static LLDrawable* sRenderSpotLight = NULL;

static LLStaticHashedString sDelta("delta");
static LLStaticHashedString sDistFactor("dist_factor");
static LLStaticHashedString sKern("kern");
static LLStaticHashedString sKernScale("kern_scale");
static LLStaticHashedString sSmaaRTMetrics("SMAA_RT_METRICS");
static LLStaticHashedString sSharpness("sharpen_params");
static LLStaticHashedString sMipLevel("mipLevel");
static LLStaticHashedString sDT("dt");
static LLStaticHashedString sNoiseVec("noiseVec");
static LLStaticHashedString sExpParams("dynamic_exposure_params");
static LLStaticHashedString sExposure("exposure");
static LLStaticHashedString sIrradianceScale("ssao_irradiance_scale");
static LLStaticHashedString sIrradianceMax("ssao_irradiance_max");
static LLStaticHashedString sAboveWater("above_water");

std::string gPoolNames[LLDrawPool::NUM_POOL_TYPES] =
{
	// Correspond to LLDrawpool enum render type
	"NONE",
	"POOL_SIMPLE",
	"POOL_FULLBRIGHT",
	"POOL_BUMP",
	"POOL_TERRAIN,"
	"POOL_MATERIALS",
	"POOL_MAT_PBR",
	"POOL_GRASS",
	"POOL_MAT_PBR_ALPHA_MASK",
	"POOL_TREE",
	"POOL_ALPHA_MASK",
	"POOL_FULLBRIGHT_ALPHA_MASK",
	"POOL_SKY",
	"POOL_WL_SKY",
	"POOL_INVISIBLE",
	"POOL_AVATAR",
	"POOL_PUPPET",
	"POOL_GLOW",
	"POOL_ALPHA_PRE_WATER",
	"POOL_VOIDWATER",
	"POOL_WATER",
	"POOL_ALPHA_POST_WATER",
	"POOL_ALPHA"
};

bool LLPipeline::sFreezeTime = false;
bool LLPipeline::sPickAvatar = true;
bool LLPipeline::sDynamicLOD = true;
bool LLPipeline::sShowHUDAttachments = true;
bool LLPipeline::sRenderBeaconsFloaterOpen = false;
bool LLPipeline::sAutoMaskAlphaDeferred = true;
bool LLPipeline::sAutoMaskAlphaNonDeferred = false;
bool LLPipeline::sUseFarClip = true;
bool LLPipeline::sShadowRender = false;
bool LLPipeline::sCanRenderGlow = false;
bool LLPipeline::sReflectionRender = false;
bool LLPipeline::sImpostorRender = false;
bool LLPipeline::sImpostorRenderAlphaDepthPass = false;
bool LLPipeline::sAvatarPreviewRender = false;
bool LLPipeline::sUnderWaterRender = false;
bool LLPipeline::sRenderFrameTest = false;
bool LLPipeline::sRenderAttachedLights = true;
bool LLPipeline::sRenderAttachedParticles = true;
bool LLPipeline::sRenderDeferred = false;
bool LLPipeline::sRenderWater = true;
bool LLPipeline::sReflectionProbesEnabled = false;
S32 LLPipeline::sUseOcclusion = 0;
S32 LLPipeline::sVisibleLightCount = 0;

LLCullResult* LLPipeline::sCull = NULL;

static const LLMatrix4a TRANS_MAT(LLVector4a(0.5f, 0.f, 0.f, 0.f),
								  LLVector4a(0.f, 0.5f, 0.f, 0.f),
								  LLVector4a(0.f, 0.f, 0.5f, 0.f),
								  LLVector4a(0.5f, 0.5f, 0.5f, 1.f));

// Utility functions only used here

static LLMatrix4a look_proj(const LLVector3& pos_in, const LLVector3& dir_in,
							const LLVector3& up_in)
{
	const LLVector4a pos(pos_in.mV[VX], pos_in.mV[VY], pos_in.mV[VZ], 1.f);
	LLVector4a dir(dir_in.mV[VX], dir_in.mV[VY], dir_in.mV[VZ]);
	const LLVector4a up(up_in.mV[VX], up_in.mV[VY], up_in.mV[VZ]);

	LLVector4a left_norm;
	left_norm.setCross3(dir, up);
	left_norm.normalize3fast();
	LLVector4a up_norm;
	up_norm.setCross3(left_norm, dir);
	up_norm.normalize3fast();
	LLVector4a& dir_norm = dir;
	dir.normalize3fast();

	LLVector4a left_dot;
	left_dot.setAllDot3(left_norm, pos);
	left_dot.negate();
	LLVector4a up_dot;
	up_dot.setAllDot3(up_norm, pos);
	up_dot.negate();
	LLVector4a dir_dot;
	dir_dot.setAllDot3(dir_norm, pos);

	dir_norm.negate();

	LLMatrix4a ret;
	ret.setRow<0>(left_norm);
	ret.setRow<1>(up_norm);
	ret.setRow<2>(dir_norm);
	ret.setRow<3>(LLVector4a(0.f, 0.f, 0.f, 1.f));

	ret.getRow<0>().copyComponent<3>(left_dot);
	ret.getRow<1>().copyComponent<3>(up_dot);
	ret.getRow<2>().copyComponent<3>(dir_dot);

	ret.transpose();

	return ret;
}

static bool addDeferredAttachments(LLRenderTarget& target)
{
	if (gUsePBRShaders)
	{
			   // frag-data[1] specular OR PBR ORM
		return target.addColorAttachment(GL_RGBA) &&
			   // frag_data[2] normal+z+fogmask,
			   // See: class1/deferred/materialF.glsl & softenlight
			   target.addColorAttachment(GL_RGBA16F) &&
			   // frag_data[3] PBR emissive
			   target.addColorAttachment(GL_RGB16F);
	}
	return target.addColorAttachment(GL_SRGB8_ALPHA8) &&	// Specular
		   target.addColorAttachment(GL_RGBA12);			// Normal + z
}

LLPipeline::LLPipeline()
:	mBackfaceCull(false),
	mNeedsDrawStats(false),
	mPoissonOffset(0),
	mBatchCount(0),
	mMatrixOpCount(0),
	mTextureMatrixOps(0),
	mMaxBatchSize(0),
	mMinBatchSize(0),
	mTrianglesDrawn(0),
	mNumVisibleNodes(0),
	mInitialized(false),
	mVertexShadersLoaded(-1),
	mRenderDebugFeatureMask(0),
	mRenderDebugMask(0),
	mOldRenderDebugMask(0),
	mMeshDirtyQueryObject(0),
	mGroupQLocked(false),
	mResetVertexBuffers(false),
	mLastRebuildPool(NULL),
	mAlphaPool(NULL),
	mAlphaPoolPreWater(NULL),
	mAlphaPoolPostWater(NULL),
	mSkyPool(NULL),
	mTerrainPool(NULL),
	mWaterPool(NULL),
	mSimplePool(NULL),
	mGrassPool(NULL),
	mAlphaMaskPool(NULL),
	mFullbrightAlphaMaskPool(NULL),
	mFullbrightPool(NULL),
	mInvisiblePool(NULL),
	mGlowPool(NULL),
	mBumpPool(NULL),
	mMaterialsPool(NULL),
	mWLSkyPool(NULL),
	mPBROpaquePool(NULL),
	mPBRAlphaMaskPool(NULL),
	mLightMask(0),
	mNoiseMap(0),
	mTrueNoiseMap(0),
	mAreaMap(0),
	mSearchMap(0),
	mLightFunc(0),
	mProbeAmbiance(0.f),
	mSkyGamma(1.f),
	mEyeAboveWater(0.f),
	mWaterHeight(0.f),
	mIsSunUp(true),
	mIsMoonUp(false)
{
}

void LLPipeline::connectRefreshCachedSettingsSafe(const char* name)
{
	LLPointer<LLControlVariable> cvp = gSavedSettings.getControl(name);
	if (cvp.isNull())
	{
		llwarns << "Global setting name not found: " << name << llendl;
		return;
	}
	cvp->getSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
}

void LLPipeline::createAuxVBs()
{
	mCubeVB = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
	if (!ll_setup_cube_vb(mCubeVB))
	{
		llwarns << "Could not setup a cube VB !" << llendl;
		mCubeVB = NULL;
	}

	mDeferredVB = new LLVertexBuffer(AUX_VB_MASK);
	mDeferredVB->allocateBuffer(8, 0);

	mScreenTriangleVB = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
	mScreenTriangleVB->allocateBuffer(3, 0);
	LLStrider<LLVector3> v;
	if (mScreenTriangleVB->getVertexStrider(v))
	{
		v[0].set(-1.f, 1.f, 0.f);
		v[1].set(-1.f, -3.f, 0.f);
		v[2].set(3.f, 1.f, 0.f);
	}
	else
	{
		llwarns << "Could not initialize mScreenTriangleVB strider !"
				<< llendl;
	}
	mScreenTriangleVB->unmapBuffer();

	if (!gUsePBRShaders)
	{
		mGlowCombineVB = new LLVertexBuffer(AUX_VB_MASK);
		mGlowCombineVB->allocateBuffer(3, 0);
		LLStrider<LLVector3> v;
		LLStrider<LLVector2> uv1;
		if (mGlowCombineVB->getVertexStrider(v) &&
			mGlowCombineVB->getTexCoord0Strider(uv1))
		{
			uv1[0].clear();
			uv1[1].set(0.f, 2.f);
			uv1[2].set(2.f, 0.f);

			v[0].set(-1.f, -1.f, 0.f);
			v[1].set(-1.f, 3.f, 0.f);
			v[2].set(3.f, -1.f, 0.f);
		}
		else
		{
			llwarns << "Could not initialize mGlowCombineVB striders !"
					<< llendl;
		}
		mGlowCombineVB->unmapBuffer();
	}

#if LL_DEBUG_VB_ALLOC
	if (mCubeVB)
	{
		mCubeVB->setOwner("LLPipeline cube VB");
	}
	mDeferredVB->setOwner("LLPipeline deferred VB");
	if (mGlowCombineVB)
	{
		mGlowCombineVB->setOwner("LLPipeline glow combine VB");
	}
	if (mScreenTriangleVB)
	{
		mScreenTriangleVB->setOwner("LLPipeline screen triangle VB");
	}
#endif
}

void LLPipeline::init()
{
	mRT = &mMainRT;

	// The following three lines used to be in llappviewer.cpp, in
	// settings_to_globals(). HB
	sRenderDeferred = gUsePBRShaders ||
					  gSavedSettings.getBool("RenderDeferred");
	LLRenderTarget::sUseFBO = sRenderDeferred;

#if 1	// This should only taken into account after a restart, thus why it
		// is set here. HB
	RenderFSAASamples = llmin((U32)gSavedSettings.getU32("RenderFSAASamples"),
							  16U);
#endif
	refreshCachedSettings();

	gOctreeMaxCapacity = gSavedSettings.getU32("OctreeMaxNodeCapacity");
	gOctreeMinSize = gSavedSettings.getF32("OctreeMinimumNodeSize");

	sDynamicLOD = gSavedSettings.getBool("RenderDynamicLOD");

	sRenderAttachedLights = gSavedSettings.getBool("RenderAttachedLights");
	sRenderAttachedParticles =
		gSavedSettings.getBool("RenderAttachedParticles");
	sAutoMaskAlphaDeferred =
		gSavedSettings.getBool("RenderAutoMaskAlphaDeferred");
	sAutoMaskAlphaNonDeferred =
		gSavedSettings.getBool("RenderAutoMaskAlphaNonDeferred");

	if (gFeatureManager.isFeatureAvailable("RenderCompressTextures"))
	{
		LLImageGL::sCompressTextures =
			gGLManager.mGLVersion >= 2.1f &&
			gSavedSettings.getBool("RenderCompressTextures");
		LLImageGL::sCompressThreshold =
			gSavedSettings.getU32("RenderCompressThreshold");
	}

	// Create render pass pools
	if (gUsePBRShaders)
	{
		getPool(LLDrawPool::POOL_ALPHA_PRE_WATER);
		getPool(LLDrawPool::POOL_ALPHA_POST_WATER);
	}
	else
	{
		getPool(LLDrawPool::POOL_ALPHA);
	}
	getPool(LLDrawPool::POOL_SIMPLE);
	getPool(LLDrawPool::POOL_ALPHA_MASK);
	getPool(LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK);
	getPool(LLDrawPool::POOL_GRASS);
	getPool(LLDrawPool::POOL_FULLBRIGHT);
	if (!gUsePBRShaders)
	{
		getPool(LLDrawPool::POOL_INVISIBLE);
	}
	getPool(LLDrawPool::POOL_BUMP);
	getPool(LLDrawPool::POOL_MATERIALS);
	getPool(LLDrawPool::POOL_GLOW);
	if (gUsePBRShaders)
	{
		getPool(LLDrawPool::POOL_MAT_PBR);
		getPool(LLDrawPool::POOL_MAT_PBR_ALPHA_MASK);
	}

	mTrianglesDrawnStat.reset();
	resetFrameStats();

	setAllRenderTypes();	// All rendering types start enabled

	mRenderDebugFeatureMask = 0xffffffff; // All debugging features on
	mRenderDebugMask = 0;	// All debug starts off

	mOldRenderDebugMask = mRenderDebugMask;

	mBackfaceCull = true;

	for (U32 i = 0; i < 2; ++i)
	{
		mSpotLightFade[i] = 1.f;
	}

	createAuxVBs();

	// Enable features

	// Must be set before calling setShaders().
	mInitialized = true;

	// Note: this will set mVertexShadersLoaded to 1 if basic shaders get
	// successfully loaded, or to -1 on failure.
	gViewerShaderMgrp->setShaders();

	stop_glerror();

	if (!gSavedSettings.getBool("SkipStaticVectorSizing"))
	{
		// Reserve some space in permanent vectors to avoid fragmentation,
		// based on the statistics we got for real sessions.
		// By setting SkipStaticVectorSizing to true (and restarting the
		// viewer), you may skip this pre-sizing of vectors so to verify (via
		// "Advanced" -> "Consoles" -> "Info to Debug Console" ->
		// "Memory Stats") what capacity is naturally reached during a session
		// and check it against the capacity reserved below (this is how I
		// determined the suitable values). HB
		mMovedList.reserve(1024);
		mMovedBridge.reserve(1024);
		mGroupQ.reserve(8192);
		mMeshDirtyGroup.reserve(2048);
		mShiftList.reserve(65536);
	}

	// Register settings callbacks

	connectRefreshCachedSettingsSafe("RenderAutoMaskAlphaDeferred");
	connectRefreshCachedSettingsSafe("RenderAutoMaskAlphaNonDeferred");
	connectRefreshCachedSettingsSafe("RenderUseFarClip");
	connectRefreshCachedSettingsSafe("UseOcclusion");
	connectRefreshCachedSettingsSafe("RenderDeferred");
	connectRefreshCachedSettingsSafe("RenderDeferredSunWash");
	connectRefreshCachedSettingsSafe("RenderDeferredAASharpen");
	connectRefreshCachedSettingsSafe("RenderResolutionDivisor");
	connectRefreshCachedSettingsSafe("RenderShadowResolutionScale");
	connectRefreshCachedSettingsSafe("RenderDelayCreation");
	connectRefreshCachedSettingsSafe("RenderAnimateRes");
	connectRefreshCachedSettingsSafe("RenderLocalLightCount");
	connectRefreshCachedSettingsSafe("RenderSpotLightsInNondeferred");
	connectRefreshCachedSettingsSafe("PreviewAmbientColor");
	connectRefreshCachedSettingsSafe("PreviewDiffuse0");
	connectRefreshCachedSettingsSafe("PreviewSpecular0");
	connectRefreshCachedSettingsSafe("PreviewDiffuse1");
	connectRefreshCachedSettingsSafe("PreviewSpecular1");
	connectRefreshCachedSettingsSafe("PreviewDiffuse2");
	connectRefreshCachedSettingsSafe("PreviewSpecular2");
	connectRefreshCachedSettingsSafe("PreviewDirection0");
	connectRefreshCachedSettingsSafe("PreviewDirection1");
	connectRefreshCachedSettingsSafe("PreviewDirection2");
	connectRefreshCachedSettingsSafe("RenderGlowMinLuminance");
	connectRefreshCachedSettingsSafe("RenderGlowMaxExtractAlpha");
	connectRefreshCachedSettingsSafe("RenderGlowWarmthAmount");
	connectRefreshCachedSettingsSafe("RenderGlowLumWeights");
	connectRefreshCachedSettingsSafe("RenderGlowWarmthWeights");
	connectRefreshCachedSettingsSafe("RenderGlowIterations");
	connectRefreshCachedSettingsSafe("RenderGlowWidth");
	connectRefreshCachedSettingsSafe("RenderGlowStrength");
	connectRefreshCachedSettingsSafe("RenderDepthOfFieldInEditMode");
	connectRefreshCachedSettingsSafe("RenderShadowNoise");
	connectRefreshCachedSettingsSafe("RenderShadowBlurSize");
	connectRefreshCachedSettingsSafe("RenderSSAOScale");
	connectRefreshCachedSettingsSafe("RenderSSAOMaxScale");
	connectRefreshCachedSettingsSafe("RenderSSAOFactor");
	connectRefreshCachedSettingsSafe("RenderSSAOEffect");
	connectRefreshCachedSettingsSafe("RenderShadowBiasError");
	connectRefreshCachedSettingsSafe("RenderShadowOffset");
	connectRefreshCachedSettingsSafe("RenderShadowOffsetNoSSAO");
	connectRefreshCachedSettingsSafe("RenderShadowBias");
	connectRefreshCachedSettingsSafe("RenderSpotShadowOffset");
	connectRefreshCachedSettingsSafe("RenderSpotShadowBias");
	connectRefreshCachedSettingsSafe("RenderShadowGaussian");
	connectRefreshCachedSettingsSafe("RenderShadowBlurDistFactor");
	connectRefreshCachedSettingsSafe("RenderDeferredAtmospheric");
	connectRefreshCachedSettingsSafe("RenderShadowClipPlanes");
	connectRefreshCachedSettingsSafe("RenderShadowOrthoClipPlanes");
	connectRefreshCachedSettingsSafe("RenderFarClip");
	connectRefreshCachedSettingsSafe("RenderShadowSplitExponent");
	connectRefreshCachedSettingsSafe("RenderShadowErrorCutoff");
	connectRefreshCachedSettingsSafe("RenderShadowFOVCutoff");
	connectRefreshCachedSettingsSafe("CameraOffset");
	connectRefreshCachedSettingsSafe("CameraMaxCoF");
	connectRefreshCachedSettingsSafe("CameraDoFResScale");
	connectRefreshCachedSettingsSafe("RenderAutoHideGeometryMemoryLimit");
	connectRefreshCachedSettingsSafe("RenderAutoHideSurfaceAreaLimit");
	connectRefreshCachedSettingsSafe("RenderWater");

	// PBR related settings
	connectRefreshCachedSettingsSafe("RenderBufferVisualization");
	connectRefreshCachedSettingsSafe("RenderScreenSpaceReflections");
	connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionIterations");
	connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionRayStep");
	connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionDistanceBias");
	connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionDepthRejectBias");
	connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionAdaptiveStepMultiplier");
	connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionGlossySamples");

	// Beacons stuff
	connectRefreshCachedSettingsSafe("scriptsbeacon");
	connectRefreshCachedSettingsSafe("scripttouchbeacon");
	connectRefreshCachedSettingsSafe("physicalbeacon");
	connectRefreshCachedSettingsSafe("permanentbeacon");
	connectRefreshCachedSettingsSafe("characterbeacon");
	connectRefreshCachedSettingsSafe("soundsbeacon");
	connectRefreshCachedSettingsSafe("invisiblesoundsbeacon");
	connectRefreshCachedSettingsSafe("particlesbeacon");
	connectRefreshCachedSettingsSafe("moapbeacon");
	connectRefreshCachedSettingsSafe("renderhighlights");
	connectRefreshCachedSettingsSafe("renderbeacons");
	connectRefreshCachedSettingsSafe("renderattachment");
	connectRefreshCachedSettingsSafe("renderbyowner");
	connectRefreshCachedSettingsSafe("DebugBeaconLineWidth");
}

// This must be called at the very start of a render frame. HB
void LLPipeline::toggleRenderer()
{
	// Force a GL states check here.
	bool old_debug_gl = gDebugGL;
	gDebugGL = true;
	LL_GL_CHECK_STATES;
	gDebugGL = old_debug_gl;

	// First, cleanup everything.

	GLbitfield mask = GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT;
	if (!gUsePBRShaders)
	{
		mask |= GL_STENCIL_BUFFER_BIT;
	}
	glClear(mask);

	mResetVertexBuffers = true;
	doResetVertexBuffers(true);

	releaseGLBuffers();

	LLRenderTarget::reset();

	gCubeSnapshot = false;		// Paranoia (should already be false).

	while (true)	// Avoids a goto.
	{
		// Toggle now.
		gUsePBRShaders = !gUsePBRShaders;
		llinfos << "Toggling the renderer to the "
				<< (gUsePBRShaders ? "PBR" : "EE") << " mode..." << llendl;

		// Resync render pass pools
		if (gUsePBRShaders)
		{
			removePool(mAlphaPool);
			removePool(mInvisiblePool);
			getPool(LLDrawPool::POOL_ALPHA_PRE_WATER);
			getPool(LLDrawPool::POOL_ALPHA_POST_WATER);
			getPool(LLDrawPool::POOL_MAT_PBR);
			getPool(LLDrawPool::POOL_MAT_PBR_ALPHA_MASK);
		}
		else
		{
			mReflectionMapManager.cleanup();
			removePool(mAlphaPoolPreWater);
			removePool(mAlphaPoolPostWater);
			removePool(mPBROpaquePool);
			removePool(mPBRAlphaMaskPool);
			getPool(LLDrawPool::POOL_ALPHA);
			getPool(LLDrawPool::POOL_INVISIBLE);
		}

		// Reload/resync the shaders (also refreshes the pipeline cached
		// settings and recreates GL buffers on success).
		gViewerShaderMgrp->setShaders();

		if (gUsePBRShaders && !LLViewerShaderMgr::sInitialized)
		{
			// We failed to load the mandatory PBR shaders, and we need to
			// switch back to the EE renderer immediately (otherwise we would
			// crash later on while rendering). HB
			gSavedSettings.setBool("RenderUsePBR", false); // Resync setting
			continue;	// Loop back...
		}

		break;	// Success. Exit the "loop".
	}

	// Refresh again graphics preferences (when open) after shader loading
	// since the status of some check boxes depends on what got actually
	// loaded. HB
	LLPrefsGraphics::refresh();

	// Rebuild everything

	mResetVertexBuffers = true;
	doResetVertexBuffers(true);

	// Force-rebuild all objects in the render pipeline.
	for (S32 i = 0, count = gObjectList.getNumObjects(); i < count; ++i)
	{
		LLViewerObject* objectp = gObjectList.getObject(i);
		if (objectp && !objectp->isDead())
		{
			objectp->markForUpdate(true);
		}
	}

	// Force a GL states check here.
	gDebugGL = true;
	LL_GL_CHECK_STATES;
	gDebugGL = old_debug_gl;
}

void LLPipeline::cleanup()
{
	llinfos << "Total number of occlusion query timeouts: "
			<< LLOcclusionCullingGroup::getTimeouts() << llendl;

	mGroupQ.clear();
	mGroupSaveQ.clear();

	for (pool_set_t::iterator iter = mPools.begin(),
							  end = mPools.end();
		 iter != end; )
	{
		pool_set_t::iterator curiter = iter++;
		LLDrawPool* poolp = *curiter;

		if (!poolp)	// Paranoia
		{
			llwarns << "Found a NULL pool !" << llendl;
			continue;
		}

		if (poolp->isFacePool())
		{
			LLFacePool* face_pool = (LLFacePool*)poolp;
			if (face_pool->mReferences.empty())
			{
				mPools.erase(curiter);
				removeFromQuickLookup(poolp);
				delete poolp;
			}
		}
		else
		{
			mPools.erase(curiter);
			removeFromQuickLookup(poolp);
			delete poolp;
		}
	}

	if (!mTerrainPools.empty())
	{
		llwarns << "Terrain pools not cleaned up" << llendl;
	}
	if (!mTreePools.empty())
	{
		llwarns << "Tree pools not cleaned up" << llendl;
	}

	delete mAlphaPool;
	mAlphaPool = NULL;
	delete mAlphaPoolPreWater;
	mAlphaPoolPreWater = NULL;
	delete mAlphaPoolPostWater;
	mAlphaPoolPostWater = NULL;
	delete mSkyPool;
	mSkyPool = NULL;
	delete mTerrainPool;
	mTerrainPool = NULL;
	delete mWaterPool;
	mWaterPool = NULL;
	delete mSimplePool;
	mSimplePool = NULL;
	delete mFullbrightPool;
	mFullbrightPool = NULL;
	delete mInvisiblePool;
	mInvisiblePool = NULL;
	delete mGlowPool;
	mGlowPool = NULL;
	delete mBumpPool;
	mBumpPool = NULL;
#if 0	// Do not delete WL sky pool: already done above in the for loop.
	delete mWLSkyPool;
#endif
	mWLSkyPool = NULL;
	delete mPBROpaquePool;
	mPBROpaquePool = NULL;
	delete mPBRAlphaMaskPool;
	mPBRAlphaMaskPool = NULL;

	releaseGLBuffers();

	mFaceSelectImagep = NULL;

	mMovedList.clear();
	mMovedBridge.clear();
	mShiftList.clear();

	mMeshDirtyGroup.clear();

	mInitialized = false;

	mDeferredVB = mGlowCombineVB = mCubeVB = mScreenTriangleVB = NULL;

	mReflectionMapManager.cleanup();
}

void LLPipeline::dumpStats()
{
	llinfos << "mMovedList vector capacity reached: " << mMovedList.capacity()
			<< " - mMovedBridge vector capacity reached: "
			<< mMovedBridge.capacity()
			<< " - mShiftList vector capacity reached: "
			<< mShiftList.capacity()
			<< " - mGroupQ vector capacity reached: " << mGroupQ.capacity()
			<< " - mMeshDirtyGroup vector capacity reached: "
			<< mMeshDirtyGroup.capacity() << llendl;
}

void LLPipeline::destroyGL()
{
	unloadShaders();
	mHighlightFaces.clear();

	resetDrawOrders();

	resetVertexBuffers();

	releaseGLBuffers();

	if (mMeshDirtyQueryObject)
	{
		glDeleteQueries(1, &mMeshDirtyQueryObject);
		mMeshDirtyQueryObject = 0;
	}
	stop_glerror();
}

void LLPipeline::resizeShadowTexture()
{
	gResizeShadowTexture = false;
	releaseShadowTargets();
	allocateShadowBuffer(mRT->mWidth, mRT->mHeight);
}

void LLPipeline::resizeScreenTexture()
{
	static U32 res_divisor = 0;

	LL_FAST_TIMER(FTM_RESIZE_SCREEN_TEXTURE);

	gResizeScreenTexture = false;

	if (shadersLoaded())
	{
		GLuint res_x = gViewerWindowp->getWindowDisplayWidth();
		GLuint res_y = gViewerWindowp->getWindowDisplayHeight();
		if (res_x != mRT->mScreen.getWidth() ||
			res_y != mRT->mScreen.getHeight() ||
			res_divisor != RenderResolutionDivisor)
		{
			res_divisor = RenderResolutionDivisor;
			releaseScreenBuffers();
			allocateScreenBuffer(res_x, res_y);
		}
	}
}

void LLPipeline::allocatePhysicsBuffer()
{
	GLuint res_x = gViewerWindowp->getWindowDisplayWidth();
	GLuint res_y = gViewerWindowp->getWindowDisplayHeight();

	if (mPhysicsDisplay.getWidth() != res_x ||
		mPhysicsDisplay.getHeight() != res_y)
	{
		mPhysicsDisplay.release();
		mPhysicsDisplay.allocate(res_x, res_y, GL_RGBA, true, false,
								 LLTexUnit::TT_RECT_TEXTURE);
	}
}

void LLPipeline::allocateScreenBuffer(U32 res_x, U32 res_y)
{
	refreshCachedSettings();

	U32 samples = RenderFSAASamples;

	// Try to allocate screen buffers at requested resolution and samples:
	// - on failure, shrink number of samples and try again
	// - if not multisampled, shrink resolution and try again (favor X
	//   resolution over Y)
	// Make sure to call "releaseScreenBuffers" after each failure to cleanup
	// the partially loaded state

	if (!allocateScreenBuffer(res_x, res_y, samples))
	{
		releaseScreenBuffers();
		// Reduce number of samples
		while (samples > 0)
		{
			samples /= 2;
			if (allocateScreenBuffer(res_x, res_y, samples))
			{
				return;	// success
			}
			releaseScreenBuffers();
		}

		samples = 0;

		// Reduce resolution
		while (res_y > 0 && res_x > 0)
		{
			res_y /= 2;
			if (allocateScreenBuffer(res_x, res_y, samples))
			{
				return;
			}
			releaseScreenBuffers();

			res_x /= 2;
			if (allocateScreenBuffer(res_x, res_y, samples))
			{
				return;
			}
			releaseScreenBuffers();
		}

		llwarns << "Unable to allocate screen buffer at any resolution !"
				<< llendl;
	}
}

bool LLPipeline::allocateScreenBuffer(U32 res_x, U32 res_y, U32 samples)
{
	refreshCachedSettings();

	llinfos << "Allocating ";
	if (gCubeSnapshot)
	{
		llcont << "auxillary ";
	}
	llcont << "target buffers at size " << res_x << "x" << res_y
			<< " with " << samples << " samples..." << llendl;

	if (sReflectionProbesEnabled && mRT == &mMainRT)
	{
		// *HACK: allocate auxillary buffers.
		// Note: gCubeSnapshot acts as a flag for the auxilliary buffers
		// allocation step. HB
		gCubeSnapshot = true;
		mReflectionMapManager.initReflectionMaps();
		mRT = &mAuxillaryRT;
		// Multiply by 4 because probes will be 16x super sampled
		U32 res = mReflectionMapManager.mProbeResolution * 4;
		allocateScreenBuffer(res, res, samples);
		mRT = &mMainRT;
		gCubeSnapshot = false;
	}

	U32 res_mod = RenderResolutionDivisor;
	if (res_mod > 1 && res_mod < res_x && res_mod < res_y)
	{
//MK
		// *HACK: avoids issues and cheating when drawing cloud spheres around
		// the avatar and RenderResolutionDivisor is larger than 1
		if (res_mod < 256 && gRLenabled && gRLInterface.mVisionRestricted)
		{
			res_mod = 256;
		}
//mk
		res_x /= res_mod;
		res_y /= res_mod;
	}

	// Remember these dimensions
	mRT->mWidth = res_x;
	mRT->mHeight = res_y;

	if (!sRenderDeferred && !gUsePBRShaders)	// Forward rendering
	{
		mRT->mDeferredLight.release();

		releaseShadowTargets();

		mRT->mFXAABuffer.release();
		mRT->mSMAABlendBuffer.release();
		mRT->mSMAAEdgeBuffer.release();
		mRT->mScratchBuffer.release();
		mRT->mScreen.release();
		// Make sure to release any render targets that share a depth buffer
		// with mDeferredScreen first:
		mRT->mDeferredScreen.release();
		if (!gUsePBRShaders)
		{
			mDeferredDepth.release();
			mOcclusionDepth.release();
		}

		if (!mRT->mScreen.allocate(res_x, res_y, GL_RGBA, true, true,
								   LLTexUnit::TT_RECT_TEXTURE))
		{
			llwarns << "Failed to allocate the screen buffer." << llendl;
			return false;
		}

		gGL.getTexUnit(0)->disable();
		stop_glerror();

		llinfos << "Allocation successful." << llendl;
		return true;
	}

	if (!gUsePBRShaders)
	{
		// Set this flag in case we crash while resizing window or allocating
		// space for deferred rendering targets
		gSavedSettings.setBool("RenderInitError", true);
		gAppViewerp->saveGlobalSettings();
	}

	constexpr U32 occlusion_divisor = 3;

	// Allocate deferred rendering color buffers
	if (gUsePBRShaders)
	{
		if (!mRT->mDeferredScreen.allocate(res_x, res_y, GL_RGBA, true))
		{
			llwarns << "Failed to allocate the deferred screen buffer."
					<< llendl;
			return false;
		}
	}
	else if (!mRT->mDeferredScreen.allocate(res_x, res_y, GL_SRGB8_ALPHA8,
											true, true,
											LLTexUnit::TT_RECT_TEXTURE))
	{
		llwarns << "Failed to allocate the deferred screen buffer."
				<< llendl;
		return false;
	}
	if (!addDeferredAttachments(mRT->mDeferredScreen))
	{
		llwarns << "Failed to attach the deferred screen buffer." << llendl;
		return false;
	}

	if (!gUsePBRShaders)
	{
		if (!mDeferredDepth.allocate(res_x, res_y, 0, true, false,
									 LLTexUnit::TT_RECT_TEXTURE))
		{
			llwarns << "Failed to allocate the deferred depth buffer."
					<< llendl;
			return false;
		}
		if (!mOcclusionDepth.allocate(res_x / occlusion_divisor,
									  res_y / occlusion_divisor, 0, true,
									  false, LLTexUnit::TT_RECT_TEXTURE))
		{
			llwarns << "Failed to allocate the occlusion depth buffer."
					<< llendl;
			return false;
		}
	}

	if (gUsePBRShaders)
	{
		if (!mRT->mScreen.allocate(res_x, res_y, GL_RGBA16F))
		{
			llwarns << "Failed to allocate the screen buffer." << llendl;
			return false;
		}
	}
	else
	{
		GLuint screen_format = GL_RGBA16;
		if (gGLManager.mIsAMD)
		{
			static LLCachedControl<bool> use_rgba16(gSavedSettings,
													"RenderUseRGBA16ATI");
			if (!use_rgba16 || gGLManager.mGLVersion < 4.f)
			{
				screen_format = GL_RGBA12;
			}
		}
		else if (gGLManager.mIsNVIDIA && gGLManager.mGLVersion < 4.f)
		{
			screen_format = GL_RGBA16F;
		}
		if (!mRT->mScreen.allocate(res_x, res_y, screen_format, false, false,
								   LLTexUnit::TT_RECT_TEXTURE))
		{
			llwarns << "Failed to allocate the screen buffer." << llendl;
			return false;
		}
	}

	// Share depth buffer between deferred targets
	mRT->mDeferredScreen.shareDepthBuffer(mRT->mScreen);

	if (samples > 0)
	{
		if (gUsePBRShaders)
		{
			if (!mRT->mFXAABuffer.allocate(res_x, res_y, GL_RGBA))
			{
				llwarns << "Failed to allocate the FXAA buffer." << llendl;
				return false;
			}
#if HB_PBR_SMAA_AND_CAS
			if (!mRT->mSMAAEdgeBuffer.allocate(res_x, res_y, GL_RGBA, true))
			{
				llwarns << "Failed to allocate the SMAA edge buffer."
						<< llendl;
				return false;
			}
			if (!mRT->mSMAABlendBuffer.allocate(res_x, res_y, GL_RGBA))
			{
				llwarns << "Failed to allocate the SMAA blend buffer."
						<< llendl;
				return false;
			}
			mRT->mSMAAEdgeBuffer.shareDepthBuffer(mRT->mSMAABlendBuffer);
#endif
		}
		else
		{
			if (!mRT->mFXAABuffer.allocate(res_x, res_y, GL_RGBA, false,
										   false))
			{
				llwarns << "Failed to allocate the FXAA buffer." << llendl;
				return false;
			}
			if (!mRT->mSMAAEdgeBuffer.allocate(res_x, res_y, GL_RGBA, true,
											   true))
			{
				llwarns << "Failed to allocate the SMAA edge buffer."
						<< llendl;
				return false;
			}
			if (!mRT->mSMAABlendBuffer.allocate(res_x, res_y, GL_RGBA, false,
												false))
			{
				llwarns << "Failed to allocate the SMAA blend buffer."
						<< llendl;
				return false;
			}
			mRT->mSMAAEdgeBuffer.shareDepthBuffer(mRT->mSMAABlendBuffer);
			if (!mRT->mScratchBuffer.allocate(res_x, res_y, GL_RGBA, false,
											  false))
			{
				llwarns << "Failed to allocate the scratch buffer."
						<< llendl;
				return false;
			}
		}
	}
	else
	{
		mRT->mFXAABuffer.release();
		mRT->mSMAABlendBuffer.release();
		mRT->mSMAAEdgeBuffer.release();
		mRT->mScratchBuffer.release();
	}

	if (samples > 0 || RenderShadowDetail || RenderDeferredSSAO ||
		RenderDepthOfField)
	{
		// Only need mDeferredLight for shadows or SSAO or DOF or FXAA
		if (gUsePBRShaders)
		{
			if (!mRT->mDeferredLight.allocate(res_x, res_y, GL_RGBA16F))
			{
				llwarns << "Failed to allocate the deferred light buffer."
						<< llendl;
				return false;
			}
		}
		else if (!mRT->mDeferredLight.allocate(res_x, res_y, GL_RGBA,
											   false, false,
											   LLTexUnit::TT_RECT_TEXTURE))
		{
			llwarns << "Failed to allocate the deferred light buffer."
					<< llendl;
			return false;
		}
	}
	else
	{
		mRT->mDeferredLight.release();
	}

	allocateShadowBuffer(res_x, res_y);

	if (gUsePBRShaders && !gCubeSnapshot)
	{
		if (RenderScreenSpaceReflections &&
			!mSceneMap.allocate(res_x, res_y, GL_RGB, true))
		{
			llwarns << "Failed to allocate the scene map buffer." << llendl;
			return false;
		}
		if (!mPostMap.allocate(res_x, res_y, GL_RGBA))
		{
			llwarns << "Failed to allocate the post map buffer." << llendl;
			return false;
		}
	}

	if (!gUsePBRShaders)
	{
		// Clear the flag set to disable shaders on next session
		gSavedSettings.setBool("RenderInitError", false);
		gAppViewerp->saveGlobalSettings();
	}

	gGL.getTexUnit(0)->disable();
	stop_glerror();

	if (!gCubeSnapshot)
	{
		llinfos << "Allocation successful." << llendl;
	}
	return true;
}

// Must be even to avoid a stripe in the horizontal shadow blur
LL_INLINE static U32 blur_happy_size(U32 x, F32 scale)
{
	return (U32((F32)x * scale) + 16) & ~0xF;
}

bool LLPipeline::allocateShadowBuffer(U32 res_x, U32 res_y)
{
	refreshCachedSettings();

	if (!sRenderDeferred && !gUsePBRShaders)
	{
		return true;
	}

	constexpr U32 occlusion_divisor = 3;
	F32 scale = RenderShadowResolutionScale;

	if (RenderShadowDetail)
	{
		// Allocate 4 sun shadow maps

		U32 sun_shadow_map_width = blur_happy_size(res_x, scale);
		U32 sun_shadow_map_height = blur_happy_size(res_y, scale);
		if (gUsePBRShaders)
		{
			for (U32 i = 0; i < 4; ++i)
			{
				if (!mRT->mSunShadow[i].allocate(sun_shadow_map_width,
												 sun_shadow_map_height, 0,
												 true))
				{
					llwarns << "Failed to allocate the Sun shadows buffer."
							<< llendl;
					return false;
				}
			}
		}
		else
		{
			for (U32 i = 0; i < 4; ++i)
			{
				if (!mShadow[i].allocate(sun_shadow_map_width,
										 sun_shadow_map_height, 0, true,
										 false))
				{
					llwarns << "Failed to allocate the Sun shadows buffers."
							<< llendl;
					return false;
				}
				if (!mShadowOcclusion[i].allocate(sun_shadow_map_width /
												  occlusion_divisor,
												  sun_shadow_map_height /
												  occlusion_divisor,
												  0, true, false))
				{
					llwarns << "Failed to allocate the Sun shadow occlusions buffers."
							<< llendl;
					return false;
				}
			}
		}
	}
	else if (gUsePBRShaders)
	{
		releaseSunShadowTargets();
	}
	else
	{
		for (U32 i = 0; i < 4; ++i)
		{
			releaseShadowTarget(i);
		}
	}

	if (RenderShadowDetail > 1)
	{
		// Allocate two spot shadow maps
		U32 size = res_x * scale;
		if (!gUsePBRShaders)
		{
			for (U32 i = 4; i < 6; ++i)
			{
				if (!mShadow[i].allocate(size, size, 0, true, false))
				{
					llwarns << "Failed to allocate the spot shadows buffers."
							<< llendl;
					return false;
				}
				if (!mShadowOcclusion[i].allocate(size / occlusion_divisor,
												  size / occlusion_divisor,
												  0, true, false))
				{
					llwarns << "Failed to allocate the spot shadow occlusions buffers."
							<< llendl;
					return false;
				}
			}
		}
		// *HACK: !gCubeSnapshot to prevent allocating spot shadow maps during
		// ReflectionMapManager init.
		else if (!gCubeSnapshot)
		{
			for (U32 i = 0; i < 2; ++i)
			{
				if (!mSpotShadow[i].allocate(size, size, 0, true))
				{
					llwarns << "Failed to allocate the spot shadows buffers."
							<< llendl;
					return false;
				}
			}
		}
	}
	else if (!gUsePBRShaders)
	{
		for (U32 i = 4; i < 6; ++i)
		{
			releaseShadowTarget(i);
		}
	}
	// *HACK: !gCubeSnapshot to prevent touching spot shadow maps during
	// ReflectionMapManager init.
	else if (!gCubeSnapshot)
	{
		releaseSpotShadowTargets();
	}

	if (!gUsePBRShaders || !RenderShadowDetail)
	{
		return true;
	}

	// Set up shadow map filtering and compare modes

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (RenderShadowDetail)
	{
		for (U32 i = 0; i < 4; ++i)
		{
			LLRenderTarget* targetp = &mRT->mSunShadow[i];
			if (!targetp) continue;

			unit0->bind(targetp, true);
			unit0->setTextureFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
			unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
							GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}
	}

	if (RenderShadowDetail > 1 && !gCubeSnapshot)
	{
		for (U32 i = 0; i < 2; ++i)
		{
			LLRenderTarget* targetp = &mSpotShadow[i];
			if (!targetp) continue;

			unit0->bind(targetp, true);
			unit0->setTextureFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
			unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
							GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}
	}

	return true;
}

//static
U32 LLPipeline::waterReflectionType()
{
	if (gUsePBRShaders)
	{
		if (!RenderTransparentWater)
		{
			return 0;
		}
	}
	else if (RenderWaterReflectionType <= 1)
	{
		return RenderWaterReflectionType;
	}
	static LLCachedControl<F32> far_clip(gSavedSettings, "RenderFarClip");
	if (gPipeline.mEyeAboveWater > F32(far_clip))
	{
		// Draw minimal water (opaque for PBR or sky reflections only for EE)
		// when farther than draw distance.
		return gUsePBRShaders ? 0 : 1;
	}
	return gUsePBRShaders ? 1 : RenderWaterReflectionType;
}

//static
void LLPipeline::updateRenderDeferred()
{
	if (gUsePBRShaders)
	{
		// If we could switch to the PBR renderer, then we obviously can render
		// in deferred mode. HB
		sRenderDeferred = LLRenderTarget::sUseFBO = true;
		return;
	}
	sRenderDeferred = RenderDeferred && !gUseWireframe &&
					  gFeatureManager.isFeatureAvailable("RenderDeferred");
	LLRenderTarget::sUseFBO = sRenderDeferred;
}

// IMPORTANT: this method shall not reallocate buffers or call another method
// that could trigger a change in the settings it gets called for (since this
// would incur a recursion). It may also be called several times as the result
// of a single callback (e.g. when reloading shaders). HB
//static
void LLPipeline::refreshCachedSettings()
{
	static LLCachedControl<bool> scriptseacon(gSavedSettings, "scriptsbeacon");
	sRenderScriptedBeacons = scriptseacon;

	static LLCachedControl<bool> scripttouchbeacon(gSavedSettings,
												   "scripttouchbeacon");
	sRenderScriptedTouchBeacons = scripttouchbeacon;

	static LLCachedControl<bool> physbeacon(gSavedSettings, "physicalbeacon");
	sRenderPhysicalBeacons = physbeacon;

	static LLCachedControl<bool> permbeacon(gSavedSettings, "permanentbeacon");
	sRenderPermanentBeacons = permbeacon;

	static LLCachedControl<bool> charbeacon(gSavedSettings, "characterbeacon");
	sRenderCharacterBeacons = charbeacon;

	static LLCachedControl<bool> soundsbeacon(gSavedSettings, "soundsbeacon");
	sRenderSoundBeacons = soundsbeacon;

	static LLCachedControl<bool> invisisoundsbeacon(gSavedSettings,
													"invisiblesoundsbeacon");
	sRenderInvisibleSoundBeacons = invisisoundsbeacon;

	static LLCachedControl<bool> particlesbeacon(gSavedSettings,
												 "particlesbeacon");
	sRenderParticleBeacons = particlesbeacon;

	static LLCachedControl<bool> moapbeacon(gSavedSettings, "moapbeacon");
	sRenderMOAPBeacons = moapbeacon;

	static LLCachedControl<bool> renderbeacons(gSavedSettings,
											   "renderbeacons");
	sRenderBeacons = renderbeacons;

	static LLCachedControl<bool> renderhighlights(gSavedSettings,
												  "renderhighlights");
	sRenderHighlight = renderhighlights;

	static LLCachedControl<bool> renderattachment(gSavedSettings,
												  "renderattachment");
	sRenderAttachments = renderattachment;

	static LLCachedControl<U32> renderbyowner(gSavedSettings, "renderbyowner");
	sRenderByOwner = renderbyowner;

	static LLCachedControl<U32> beacon_width(gSavedSettings,
											 "DebugBeaconLineWidth");
	DebugBeaconLineWidth = beacon_width;

	static LLCachedControl<bool> mask_alpha_def(gSavedSettings,
												"RenderAutoMaskAlphaDeferred");
	sAutoMaskAlphaDeferred = mask_alpha_def;

	static LLCachedControl<bool> mask_alpha(gSavedSettings,
											"RenderAutoMaskAlphaNonDeferred");
	sAutoMaskAlphaNonDeferred = mask_alpha;

	static LLCachedControl<bool> use_far_clip(gSavedSettings,
											  "RenderUseFarClip");
	sUseFarClip = use_far_clip;

	static LLCachedControl<bool> use_occlusion(gSavedSettings, "UseOcclusion");
	sUseOcclusion = use_occlusion && !gUseWireframe &&
					gFeatureManager.isFeatureAvailable("UseOcclusion") ? 2 : 0;

	static LLCachedControl<bool> render_water(gSavedSettings, "RenderWater");
	sRenderWater = render_water;

	static LLCachedControl<bool> render_deferred(gSavedSettings,
												 "RenderDeferred");
	RenderDeferred = render_deferred;

	static LLCachedControl<F32> sun_wash(gSavedSettings,
										 "RenderDeferredSunWash");
	RenderDeferredSunWash = sun_wash;

	static LLCachedControl<F32> gamma(gSavedSettings,
									  "RenderDeferredDisplayGamma");
	RenderDeferredDisplayGamma = gamma > 0.1f ? gamma : 2.2f;

#if 0	// This should only taken into account after a restart. HB
	static LLCachedControl<U32> fsaa_samp(gSavedSettings, "RenderFSAASamples");
	RenderFSAASamples = llmin((U32)fsaa_samp, 16U);
#endif

	static LLCachedControl<S32> aa_quality(gSavedSettings,
										   "RenderDeferredAAQuality");
	if (aa_quality >= 0)
	{
		RenderDeferredAAQuality = llmin((S32)aa_quality, 3);
	}
	else if (RenderFSAASamples > 8)
	{
		RenderDeferredAAQuality = 3;
	}
	else if (RenderFSAASamples > 4)
	{
		RenderDeferredAAQuality = 2;
	}
	else if (RenderFSAASamples > 2)
	{
		RenderDeferredAAQuality = 1;
	}
	else
	{
		RenderDeferredAAQuality = 0;
	}

	static LLCachedControl<bool> sharpen(gSavedSettings,
										 "RenderDeferredAASharpen");
	RenderDeferredAASharpen = sharpen;

	static LLCachedControl<U32> res_divisor(gSavedSettings,
											"RenderResolutionDivisor");
	RenderResolutionDivisor = llmax((U32)res_divisor, 1U);

	static LLCachedControl<U32> shadow_detail(gSavedSettings,
											  "RenderShadowDetail");
	RenderShadowDetail = shadow_detail;

	static LLCachedControl<U32> deferred_ssao(gSavedSettings,
											  "RenderDeferredSSAO");
	RenderDeferredSSAO = deferred_ssao > 1 ||
						 (deferred_ssao == 1 && RenderShadowDetail);

	static LLCachedControl<F32> shadow_res_scl(gSavedSettings,
											   "RenderShadowResolutionScale");
	RenderShadowResolutionScale = llclamp((F32)shadow_res_scl, 0.25f, 2.5f);

	static LLCachedControl<U32> local_lights(gSavedSettings,
											 "RenderLocalLightCount");
	RenderLocalLightCount = llmin(U32(local_lights), 1024);

	static LLCachedControl<bool> delay_creation(gSavedSettings,
												"RenderDelayCreation");
	RenderDelayCreation = delay_creation;

	static LLCachedControl<bool> anim_res(gSavedSettings, "RenderAnimateRes");
	RenderAnimateRes = anim_res;

	static LLCachedControl<bool> spot_lights_nd(gSavedSettings,
												"RenderSpotLightsInNondeferred");
	RenderSpotLightsInNondeferred = spot_lights_nd;

	static LLCachedControl<LLColor4> preview_amb_col(gSavedSettings,
													 "PreviewAmbientColor");
	PreviewAmbientColor = preview_amb_col;

	static LLCachedControl<LLColor4> preview_diff0(gSavedSettings,
												   "PreviewDiffuse0");
	PreviewDiffuse0 = preview_diff0;

	static LLCachedControl<LLColor4> preview_spec0(gSavedSettings,
												   "PreviewSpecular0");
	PreviewSpecular0 = preview_spec0;

	static LLCachedControl<LLVector3> preview_dir0(gSavedSettings,
												   "PreviewDirection0");
	PreviewDirection0 = preview_dir0;
	PreviewDirection0.normalize();

	static LLCachedControl<LLColor4> preview_diff1(gSavedSettings,
												   "PreviewDiffuse1");
	PreviewDiffuse1 = preview_diff1;

	static LLCachedControl<LLColor4> preview_spec1(gSavedSettings,
												   "PreviewSpecular1");
	PreviewSpecular1 = preview_spec1;

	static LLCachedControl<LLVector3> preview_dir1(gSavedSettings,
												   "PreviewDirection1");
	PreviewDirection1 = preview_dir1;
	PreviewDirection1.normalize();

	static LLCachedControl<LLColor4> preview_diff2(gSavedSettings,
												   "PreviewDiffuse2");
	PreviewDiffuse2 = preview_diff2;

	static LLCachedControl<LLColor4> preview_spec2(gSavedSettings,
												   "PreviewSpecular2");
	PreviewSpecular2 = preview_spec2;

	static LLCachedControl<LLVector3> preview_dir2(gSavedSettings,
												   "PreviewDirection2");
	PreviewDirection2 = preview_dir2;
	PreviewDirection2.normalize();

	static LLCachedControl<bool> render_glow(gSavedSettings, "RenderGlow");
	RenderGlow = sCanRenderGlow && render_glow;

	static LLCachedControl<F32> glow_min_lum(gSavedSettings,
											 "RenderGlowMinLuminance");
	RenderGlowMinLuminance = llmax((F32)glow_min_lum, 0.f);

	static LLCachedControl<F32> glow_max_alpha(gSavedSettings,
											   "RenderGlowMaxExtractAlpha");
	RenderGlowMaxExtractAlpha = glow_max_alpha;

	static LLCachedControl<F32> glow_warmth_a(gSavedSettings,
											  "RenderGlowWarmthAmount");
	RenderGlowWarmthAmount = glow_warmth_a;

	static LLCachedControl<LLVector3> glow_lum_w(gSavedSettings,
												 "RenderGlowLumWeights");
	RenderGlowLumWeights = glow_lum_w;

	static LLCachedControl<LLVector3> glow_warmth_w(gSavedSettings,
													"RenderGlowWarmthWeights");
	RenderGlowWarmthWeights = glow_warmth_w;

	static LLCachedControl<U32> glow_res_pow(gSavedSettings,
											 "RenderGlowResolutionPow");
	RenderGlowResolutionPow = glow_res_pow;

	static LLCachedControl<U32> glow_iterations(gSavedSettings,
												"RenderGlowIterations");
	RenderGlowIterations = glow_iterations;

	static LLCachedControl<F32> glow_width(gSavedSettings, "RenderGlowWidth");
	RenderGlowWidth = glow_width;

	static LLCachedControl<F32> glow_strength(gSavedSettings,
											  "RenderGlowStrength");
	RenderGlowStrength = llmax(0.f, (F32)glow_strength);

	static LLCachedControl<bool> depth_of_field(gSavedSettings,
												"RenderDepthOfField");
	RenderDepthOfField = depth_of_field;

	static LLCachedControl<bool> dof_in_edit(gSavedSettings,
											 "RenderDepthOfFieldInEditMode");
	RenderDepthOfFieldInEditMode = dof_in_edit;

	static LLCachedControl<F32> shadow_noise(gSavedSettings,
											 "RenderShadowNoise");
	RenderShadowNoise = shadow_noise;

	static LLCachedControl<F32> shadow_blur_size(gSavedSettings,
												 "RenderShadowBlurSize");
	RenderShadowBlurSize = shadow_blur_size;

	static LLCachedControl<F32> ssao_scale(gSavedSettings, "RenderSSAOScale");
	RenderSSAOScale = ssao_scale;

	static LLCachedControl<U32> ssao_max_scale(gSavedSettings,
											   "RenderSSAOMaxScale");
	RenderSSAOMaxScale = ssao_max_scale;

	static LLCachedControl<F32> ssao_fact(gSavedSettings, "RenderSSAOFactor");
	RenderSSAOFactor = ssao_fact;

	static LLCachedControl<LLVector3> ssao_effect(gSavedSettings,
												  "RenderSSAOEffect");
	RenderSSAOEffect = ssao_effect;

	static LLCachedControl<F32> shadow_bias_error(gSavedSettings,
												  "RenderShadowBiasError");
	RenderShadowBiasError = shadow_bias_error;

	static LLCachedControl<F32> shadow_offset(gSavedSettings,
											  "RenderShadowOffset");
	RenderShadowOffset = shadow_offset;

	static LLCachedControl<F32> shadow_off_no_ssao(gSavedSettings,
												   "RenderShadowOffsetNoSSAO");
	RenderShadowOffsetNoSSAO = shadow_off_no_ssao;

	static LLCachedControl<F32> shadow_bias(gSavedSettings,
											"RenderShadowBias");
	RenderShadowBias = shadow_bias;

	static LLCachedControl<F32> spot_shadow_offset(gSavedSettings,
												   "RenderSpotShadowOffset");
	RenderSpotShadowOffset = spot_shadow_offset;

	static LLCachedControl<F32> spot_shadow_bias(gSavedSettings,
												 "RenderSpotShadowBias");
	RenderSpotShadowBias = spot_shadow_bias;

	static LLCachedControl<LLVector3> shadow_gaussian(gSavedSettings,
													  "RenderShadowGaussian");
	RenderShadowGaussian = shadow_gaussian;

	static LLCachedControl<F32> shadow_blur_dist(gSavedSettings,
												 "RenderShadowBlurDistFactor");
	RenderShadowBlurDistFactor = shadow_blur_dist;

	static LLCachedControl<bool> deferred_atmos(gSavedSettings,
												"RenderDeferredAtmospheric");
	RenderDeferredAtmospheric = deferred_atmos;

	static LLCachedControl<U32> reflection_type(gSavedSettings,
												"RenderWaterReflectionType");
	RenderWaterReflectionType = reflection_type;

	static LLCachedControl<bool> transparent_water(gSavedSettings,
												   "RenderTransparentWater");
	RenderTransparentWater = transparent_water;

	static LLCachedControl<LLVector3> shadow_planes(gSavedSettings,
													"RenderShadowClipPlanes");
	RenderShadowClipPlanes = shadow_planes;

	static LLCachedControl<LLVector3> shadow_ortho(gSavedSettings,
												   "RenderShadowOrthoClipPlanes");
	RenderShadowOrthoClipPlanes = shadow_ortho;

	static LLCachedControl<F32> far_clip(gSavedSettings, "RenderFarClip");
	RenderFarClip = far_clip;

	static LLCachedControl<LLVector3> shadow_split_exp(gSavedSettings,
													   "RenderShadowSplitExponent");
	RenderShadowSplitExponent = shadow_split_exp;

	static LLCachedControl<F32> shadow_error_cutoff(gSavedSettings,
													"RenderShadowErrorCutoff");
	RenderShadowErrorCutoff = shadow_error_cutoff;

	static LLCachedControl<F32> shadow_fov_cutoff(gSavedSettings,
												  "RenderShadowFOVCutoff");
	RenderShadowFOVCutoff = llmin((F32)shadow_fov_cutoff, 1.4f);

	static LLCachedControl<bool> camera_offset(gSavedSettings, "CameraOffset");
	CameraOffset = camera_offset;

	static LLCachedControl<F32> camera_max_cof(gSavedSettings, "CameraMaxCoF");
	CameraMaxCoF = camera_max_cof;

	static LLCachedControl<F32> camera_dof_scale(gSavedSettings,
												 "CameraDoFResScale");
	CameraDoFResScale = camera_dof_scale;

	static LLCachedControl<U32> auto_hide_mem(gSavedSettings,
											  "RenderAutoHideGeometryMemoryLimit");
	RenderAutoHideGeometryMemoryLimit = auto_hide_mem;

	static LLCachedControl<F32> auto_hide_area(gSavedSettings,
											   "RenderAutoHideSurfaceAreaLimit");
	RenderAutoHideSurfaceAreaLimit = auto_hide_area;

	sRenderSpotLight = NULL;

	static LLCachedControl<S32> pbr_visu(gSavedSettings,
										 "RenderBufferVisualization");
	RenderBufferVisualization = llclamp(S32(pbr_visu), -1, 4);

	static LLCachedControl<bool> refl_space(gSavedSettings,
											"RenderScreenSpaceReflections");
	RenderScreenSpaceReflections = refl_space;

	static LLCachedControl<U32> refl_iter(gSavedSettings,
										  "RenderScreenSpaceReflectionIterations");
	RenderScreenSpaceReflectionIterations = refl_iter;

	static LLCachedControl<F32> refl_step(gSavedSettings,
										  "RenderScreenSpaceReflectionRayStep");
	RenderScreenSpaceReflectionRayStep = llmax(0.f, refl_step);

	static LLCachedControl<F32> refl_bias(gSavedSettings,
										  "RenderScreenSpaceReflectionDistanceBias");
	RenderScreenSpaceReflectionDistanceBias = llmax(0.f, refl_bias);

	static LLCachedControl<F32> refl_rej(gSavedSettings,
										 "RenderScreenSpaceReflectionDepthRejectBias");
	RenderScreenSpaceReflectionDepthRejectBias = llmax(0.f, refl_rej);

	static LLCachedControl<F32> refl_mult(gSavedSettings,
										  "RenderScreenSpaceReflectionAdaptiveStepMultiplier");
	RenderScreenSpaceReflectionDepthRejectBias = llmax(0.f, refl_mult);

	static LLCachedControl<U32> refl_glos(gSavedSettings,
										  "RenderScreenSpaceReflectionGlossySamples");
	RenderScreenSpaceReflectionGlossySamples = refl_glos;

	static LLCachedControl<bool> refl_enable(gSavedSettings,
											 "RenderReflectionsEnabled");
	sReflectionProbesEnabled =
		gUsePBRShaders && refl_enable &&
		gFeatureManager.isFeatureAvailable("RenderReflectionsEnabled");

	updateRenderDeferred();

	LLPrefsGraphics::refresh();
}

void LLPipeline::releaseGLBuffers()
{
	if (mNoiseMap)
	{
		LLImageGL::deleteTextures(1, &mNoiseMap);
		mNoiseMap = 0;
	}

	if (mTrueNoiseMap)
	{
		LLImageGL::deleteTextures(1, &mTrueNoiseMap);
		mTrueNoiseMap = 0;
	}

	if (mAreaMap)
	{
		LLImageGL::deleteTextures(1, &mAreaMap);
		mAreaMap = 0;
	}

	if (mSearchMap)
	{
		LLImageGL::deleteTextures(1, &mSearchMap);
		mSearchMap = 0;
	}

	releaseLUTBuffers();

	if (gUsePBRShaders)
	{
		mSceneMap.release();
		mPostMap.release();
	}
	else
	{
		mWaterRef.release();
	}
	mWaterDis.release();

	for (U32 i = 0; i < 3; ++i)
	{
		mGlow[i].release();
	}

	releaseScreenBuffers();

	LLVOAvatar::resetImpostors();
}

void LLPipeline::releaseLUTBuffers()
{
	if (mLightFunc)
	{
		LLImageGL::deleteTextures(1, &mLightFunc);
		mLightFunc = 0;
	}

	if (gUsePBRShaders)
	{
		mPbrBrdfLut.release();
		mExposureMap.release();
		mLuminanceMap.release();
		mLastExposure.release();
	}
}

void LLPipeline::releasePackBuffers(RenderTargetPack* packp)
{
	mRT = packp;
	releaseShadowTargets();

	mRT->mScreen.release();
	mRT->mFXAABuffer.release();
	mRT->mSMAAEdgeBuffer.release();
	mRT->mSMAABlendBuffer.release();
	mRT->mDeferredScreen.release();
	mRT->mDeferredLight.release();
}

void LLPipeline::releaseScreenBuffers()
{
	if (gUsePBRShaders)
	{
		releasePackBuffers(&mAuxillaryRT);
	}
	else
	{
		mPhysicsDisplay.release();
		mDeferredDepth.release();
		mOcclusionDepth.release();
	}
	releasePackBuffers(&mMainRT);
}

void LLPipeline::releaseShadowTarget(U32 index)
{
	llassert(!gUsePBRShaders);	// EE rendering only
	mShadow[index].release();
	mShadowOcclusion[index].release();
}

void LLPipeline::releaseSunShadowTargets()
{
	llassert(gUsePBRShaders); // PBR rendering only
	mRT->mSunShadow[0].release();
	mRT->mSunShadow[1].release();
	mRT->mSunShadow[2].release();
	mRT->mSunShadow[3].release();
}

void LLPipeline::releaseSpotShadowTargets()
{
	llassert(gUsePBRShaders); // PBR rendering only
	// *HACK: do not release during auxiliary target allocation
	if (!gCubeSnapshot)
	{
		mSpotShadow[0].release();
		mSpotShadow[1].release();
	}
}

void LLPipeline::releaseShadowTargets()
{
	if (gUsePBRShaders)
	{
		releaseSunShadowTargets();
		releaseSpotShadowTargets();
	}
	else
	{
		for (U32 i = 0; i < 6; ++i)
		{
			mShadow[i].release();
			mShadowOcclusion[i].release();
		}
	}
}

void LLPipeline::createGLBuffers()
{
	updateRenderDeferred();

	U32 res = llmax(gSavedSettings.getU32("RenderWaterRefResolution"), 512);
	if (!gUsePBRShaders)
	{
		// Water reflection texture
		// Set up SRGB targets if we are doing deferred-path reflection rendering
		mWaterRef.allocate(res, res, GL_RGBA, true, false);
		mWaterDis.allocate(res, res, GL_RGBA, true, false);
	}
	else if (RenderTransparentWater)
	{
		// Used only in LLDrawPoolWater
		mWaterDis.allocate(res, res, GL_RGBA16F, true);
	}

	GLuint res_x = gViewerWindowp->getWindowDisplayWidth();
	GLuint res_y = gViewerWindowp->getWindowDisplayHeight();

	// Screen space glow buffers
	U32 glow_pow = gSavedSettings.getU32("RenderGlowResolutionPow");
	// Limited between 16 and 512
	const U32 glow_res = 1 << llclamp(glow_pow, 4U, 9U);

	for (U32 i = 0; i < 3; ++i)
	{
		if (gUsePBRShaders)
		{
			mGlow[i].allocate(512, glow_res, GL_RGBA);
		}
		else
		{
			mGlow[i].allocate(512, glow_res, GL_RGBA, false, false);
		}
	}

	allocateScreenBuffer(res_x, res_y);

	if (!sRenderDeferred)	// Forward rendering
	{
		stop_glerror();
		return;
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (!mNoiseMap)
	{
		constexpr U32 noise_res = 128;
		LLVector3 noise[noise_res * noise_res];

		F32 scaler = gSavedSettings.getF32("RenderDeferredNoise") / 100.f;
		for (U32 i = 0; i < noise_res * noise_res; ++i)
		{
			noise[i].set(ll_frand() - 0.5f, ll_frand() - 0.5f, 0.f);
			noise[i].normalize();
			noise[i].mV[2] = ll_frand() * scaler + 1.f - scaler * 0.5f;
		}

		LLImageGL::generateTextures(1, &mNoiseMap);

		unit0->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap);
		LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),
								  0, GL_RGB16F, noise_res, noise_res, GL_RGB,
								  GL_FLOAT, noise, false);
		unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		unit0->disable();
	}

	if (!mTrueNoiseMap)
	{
		constexpr U32 noise_res = 128;
		F32 noise[noise_res * noise_res * 3];
		for (U32 i = 0; i < noise_res * noise_res * 3; ++i)
		{
			noise[i] = ll_frand() * 2.f - 1.f;
		}

		LLImageGL::generateTextures(1, &mTrueNoiseMap);
		unit0->bindManual(LLTexUnit::TT_TEXTURE, mTrueNoiseMap);
		LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),
								  0, GL_RGB16F, noise_res, noise_res, GL_RGB,
								  GL_FLOAT, noise, false);
		unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		unit0->disable();
	}

	if (!mAreaMap)
	{
		std::vector<unsigned char> temp_buff(AREATEX_SIZE);
		for (size_t y = 0; y < AREATEX_HEIGHT; ++y)
		{
			size_t src_y = AREATEX_HEIGHT - 1 - y;
			memcpy(&temp_buff[y * AREATEX_PITCH],
				   areaTexBytes + src_y * AREATEX_PITCH, AREATEX_PITCH);
		}

		LLImageGL::generateTextures(1, &mAreaMap);
		unit0->bindManual(LLTexUnit::TT_TEXTURE, mAreaMap);
		LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),
								  0, GL_RG8, AREATEX_WIDTH, AREATEX_HEIGHT,
								  GL_RG, GL_UNSIGNED_BYTE, temp_buff.data(),
								  false);
		unit0->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
		unit0->disable();
	}

	if (!mSearchMap)
	{
		std::vector<unsigned char> temp_buff(SEARCHTEX_SIZE);
		for (size_t y = 0; y < SEARCHTEX_HEIGHT; ++y)
		{
			size_t src_y = SEARCHTEX_HEIGHT - 1 - y;
			memcpy(&temp_buff[y * SEARCHTEX_PITCH],
				   searchTexBytes + src_y * SEARCHTEX_PITCH, SEARCHTEX_PITCH);
		}

		LLImageGL::generateTextures(1, &mSearchMap);
		unit0->bindManual(LLTexUnit::TT_TEXTURE, mSearchMap);
		LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),
								  0, GL_RG8, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT,
								  GL_RED, GL_UNSIGNED_BYTE, temp_buff.data(),
								  false);
		unit0->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
		unit0->disable();
	}

	createLUTBuffers();

	stop_glerror();
}

void LLPipeline::createLUTBuffers()
{
	if (!sRenderDeferred || mLightFunc)
	{
		return;
	}

	static LLCachedControl<U32> light_res_x(gSavedSettings,
											"RenderSpecularResX");
	static LLCachedControl<U32> light_res_y(gSavedSettings,
											"RenderSpecularResY");
	static LLCachedControl<F32> spec_exp(gSavedSettings,
										 "RenderSpecularExponent");

	F32* ls = new F32[light_res_x * light_res_y];
	// Calculate the (normalized) blinn-phong specular lookup texture (with a
	// few tweaks).
	for (U32 y = 0; y < light_res_y; ++y)
	{
		for (U32 x = 0; x < light_res_x; ++x)
		{
			ls[y * light_res_x + x] = 0.f;
			F32 sa = (F32)x / (F32)(light_res_x - 1);
			F32 spec = (F32)y / (F32)(light_res_y - 1);
			F32 n = spec * spec * spec_exp;

			// Nothing special here, just your typical blinn-phong term
			spec = powf(sa, n);

			// Apply our normalization function.
			// Note: This is the full equation that applies the full
			// normalization curve, not an approximation. This is fine, given
			// we only need to create our LUT once per buffer initialization.
			// The only trade off is we have a really low dynamic range. This
			// means we have to account for things not being able to exceed 0
			// to 1 in our shaders.
			spec *= (n + 2.f) * (n + 4.f) /
					(8.f * F_PI * (powf(2.f, -0.5f * n) + n));
			// Since we use R16F, we no longer have a dynamic range issue we
			// need to work around here. Though some older drivers may not like
			// this, newer drivers should not have this problem.
			ls[y * light_res_x + x] = spec;
		}
	}

#if LL_DARWIN
	// Work around for limited precision with 10.6.8 and older drivers
	constexpr U32 pix_format = GL_R32F;
#else
	constexpr U32 pix_format = GL_R16F;
#endif
	LLImageGL::generateTextures(1, &mLightFunc);
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc);
	LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),
							  0, pix_format, light_res_x, light_res_y,
							  GL_RED, GL_FLOAT, ls, false);
	unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	unit0->setTextureFilteringOption(LLTexUnit::TFO_TRILINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	delete[] ls;

	if (!gUsePBRShaders)
	{
		return;
	}

	mPbrBrdfLut.allocate(512, 512, GL_RG16F);
	mPbrBrdfLut.bindTarget();
	gDeferredGenBrdfLutProgram.bind();
	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.vertex2f(-1.f, -1.f);
	gGL.vertex2f(-1.f, 1.f);
	gGL.vertex2f(1.f, -1.f);
	gGL.vertex2f(1.f, 1.f);
	gGL.end(true);
	gDeferredGenBrdfLutProgram.unbind();
	mPbrBrdfLut.flush();

	mExposureMap.allocate(1, 1, GL_R16F);
	mExposureMap.bindTarget();
	glClearColor(1.f, 1.f, 1.f, 0.f);
	mExposureMap.clear();
	glClearColor(0.f, 0.f, 0.f, 0.f);
	mExposureMap.flush();

	mLuminanceMap.allocate(256, 256, GL_R16F, false, LLTexUnit::TT_TEXTURE,
						   LLTexUnit::TMG_AUTO);

	mLastExposure.allocate(1, 1, GL_R16F);
}

void LLPipeline::restoreGL()
{
	gViewerShaderMgrp->setShaders();

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; ++i)
		{
			LLSpatialPartition* partp = regionp->getSpatialPartition(i);
			if (partp)
			{
				partp->restoreGL();
			}
		}
	}
#if 0	// Failed attempt at properly restoring terrain after GL restart with
		// core GL profile enabled. HB
	LLViewerTextureList::dirty_list_t textures;
	for (pool_tex_map_t::iterator it = mTerrainPools.begin(),
								  end = mTerrainPools.end();
		 it != end; ++it)
	{
		textures.insert((LLViewerFetchedTexture*)it->first);
	}
	dirtyPoolObjectTextures(textures);
#elif 0	// Second failed attempt at properly restoring terrain after GL
		// restart with core GL profile enabled. HB
	for (pool_tex_map_t::iterator it = mTerrainPools.begin(),
								  end = mTerrainPools.end();
		 it != end; ++it)
	{
		LLDrawPool* poolp = it->second;
		if (poolp->isTerrainPool())	// Paranoia
		{
			((LLDrawPoolTerrain*)poolp)->rebuildPatches();
		}
	}
#endif
}

bool LLPipeline::canUseWindLightShaders() const
{
	if (gUsePBRShaders)
	{
		// If we could switch to PBR rendering, then we can do Windlight. HB
		return true;
	}
	return gWLSkyProgram.mProgramObject &&
		   gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_WINDLIGHT) > 1;
}

void LLPipeline::unloadShaders()
{
	gViewerShaderMgrp->unloadShaders();
	if (mVertexShadersLoaded != -1)
	{
		mVertexShadersLoaded = 0;
	}
}

class LLOctreeDirtyTexture final : public OctreeTraveler
{
public:
	LLOctreeDirtyTexture(const LLViewerTextureList::dirty_list_t& textures)
	:	mTextures(textures)
	{
	}

	void visit(const OctreeNode* nodep) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)nodep->getListener(0);
		if (!groupp) return;

		if (!groupp->isEmpty() &&
			!groupp->hasState(LLSpatialGroup::GEOM_DIRTY))
		{
			LLViewerTextureList::dirty_list_t::const_iterator textures_end =
				mTextures.end();
			for (LLSpatialGroup::draw_map_t::const_iterator
					it = groupp->mDrawMap.begin(), end = groupp->mDrawMap.end();
				 it != end; ++it)
			{
				const LLSpatialGroup::drawmap_elem_t& draw_info = it->second;
				for (U32 i = 0, count = draw_info.size(); i < count; ++i)
				{
					LLDrawInfo* infop = draw_info[i].get();
					LLViewerFetchedTexture* texp =
						LLViewerTextureManager::staticCast(infop->mTexture);
					if (texp && mTextures.find(texp) != textures_end)
					{
						groupp->setState(LLSpatialGroup::GEOM_DIRTY);
					}
				}
			}
		}

		LLSpatialGroup::bridge_list_t& blist = groupp->mBridgeList;
		for (U32 i = 0, count = blist.size(); i < count; ++i)
		{
			traverse(blist[i]->mOctree);
		}
	}

public:
	const LLViewerTextureList::dirty_list_t& mTextures;
};

// Called when a texture changes # of channels (causes faces to move to alpha
// pool)
void LLPipeline::dirtyPoolObjectTextures(const LLViewerTextureList::dirty_list_t& textures)
{
	// *TODO: This is inefficient and causes frame spikes; need a better way to
	//		  do this. Most of the time is spent in dirty.traverse.

	for (pool_set_t::iterator iter = mPools.begin(), end = mPools.end();
		 iter != end; ++iter)
	{
		LLDrawPool* poolp = *iter;
		// Only LLDrawPoolTerrain was using dirtyTextures() so I de-virtualized
		// the latter and added the isTerrainPool() virtual, which is tested
		// instead of isFacePool() with a useless call to an empty virtual
		// dirtyTextures() mesthod for all other face pools... HB
		if (poolp->isTerrainPool())
		{
			((LLDrawPoolTerrain*)poolp)->dirtyTextures(textures);
		}
	}

	LLOctreeDirtyTexture dirty(textures);
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* partp = regionp->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			dirty.traverse(partp->mOctree);
		}
	}
}

LLDrawPool* LLPipeline::findPool(U32 type, LLViewerTexture* tex0)
{
	LLDrawPool* poolp = NULL;
	switch (type)
	{
		case LLDrawPool::POOL_SIMPLE:
			poolp = mSimplePool;
			break;

		case LLDrawPool::POOL_GRASS:
			poolp = mGrassPool;
			break;

		case LLDrawPool::POOL_ALPHA_MASK:
			poolp = mAlphaMaskPool;
			break;

		case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
			poolp = mFullbrightAlphaMaskPool;
			break;

		case LLDrawPool::POOL_FULLBRIGHT:
			poolp = mFullbrightPool;
			break;

		case LLDrawPool::POOL_INVISIBLE:
			poolp = mInvisiblePool;
			break;

		case LLDrawPool::POOL_GLOW:
			poolp = mGlowPool;
			break;

		case LLDrawPool::POOL_TREE:
			poolp = get_ptr_in_map(mTreePools, (uintptr_t)tex0);
			break;

		case LLDrawPool::POOL_TERRAIN:
			poolp = get_ptr_in_map(mTerrainPools, (uintptr_t)tex0);
			break;

		case LLDrawPool::POOL_BUMP:
			poolp = mBumpPool;
			break;

		case LLDrawPool::POOL_MATERIALS:
			poolp = mMaterialsPool;
			break;

		case LLDrawPool::POOL_ALPHA_PRE_WATER:
			poolp = mAlphaPoolPreWater;
			break;

		case LLDrawPool::POOL_ALPHA_POST_WATER:
			poolp = mAlphaPoolPostWater;
			break;

		case LLDrawPool::POOL_ALPHA:
			poolp = mAlphaPool;
			break;

		case LLDrawPool::POOL_AVATAR:
		case LLDrawPool::POOL_PUPPET:
			break; // Do nothing

		case LLDrawPool::POOL_SKY:
			poolp = mSkyPool;
			break;

		case LLDrawPool::POOL_WATER:
			poolp = mWaterPool;
			break;

		case LLDrawPool::POOL_WL_SKY:
			poolp = mWLSkyPool;
			break;

		case LLDrawPool::POOL_MAT_PBR:
			poolp = mPBROpaquePool;
			break;

		case LLDrawPool::POOL_MAT_PBR_ALPHA_MASK:
			poolp = mPBRAlphaMaskPool;
			break;

		default:
			llerrs << "Invalid Pool Type: " << type << llendl;
	}

	return poolp;
}

LLDrawPool* LLPipeline::getPool(U32 type, LLViewerTexture* tex0)
{
	LLDrawPool* poolp = findPool(type, tex0);
	if (poolp)
	{
		return poolp;
	}
	poolp = LLDrawPool::createPool(type, tex0);
	addPool(poolp);
	return poolp;
}

//static
LLDrawPool* LLPipeline::getPoolFromTE(const LLTextureEntry* tep,
									  LLViewerTexture* imagep)
{
	return gPipeline.getPool(getPoolTypeFromTE(tep, imagep), imagep);
}

//static
U32 LLPipeline::getPoolTypeFromTE(const LLTextureEntry* tep,
								  LLViewerTexture* imagep)
{
	if (!tep || !imagep)
	{
		return 0;
	}

	LLMaterial* matp = tep->getMaterialParams().get();
	LLGLTFMaterial* gltfp = NULL;
	if (gUsePBRShaders)
	{
		gltfp = tep->getGLTFRenderMaterial();
	}

	bool color_alpha = tep->getAlpha() < 0.999f;
	bool alpha = color_alpha;
	if (!alpha && imagep)
	{
		S32 components = imagep->getComponents();
		alpha = components == 2 ||
				(components == 4 &&
				 imagep->getType() != LLViewerTexture::MEDIA_TEXTURE);
	}
	if (alpha && matp)
	{
		if (matp->getDiffuseAlphaMode() == 1)
		{
			// Material's alpha mode is set to blend. Toss it into the alpha
			// draw pool.
			return LLDrawPool::POOL_ALPHA;
		}
		// 0 -> Material alpha mode set to none, never go to alpha pool
		// 3 -> Material alpha mode set to emissive, never go to alpha pool
		// other -> Material alpha mode set to "mask", go to alpha pool if
		// fullbright, or material alpha mode is set to none, mask or
		// emissive. Toss it into the opaque material draw pool.
		alpha = color_alpha;	// Use the pool matching the te alpha
	}
	if (alpha ||
		(gltfp && gltfp->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND))
	{
		return LLDrawPool::POOL_ALPHA;
	}

	if ((tep->getBumpmap() || tep->getShiny()) &&
		(!matp || matp->getNormalID().isNull()))
	{
		return LLDrawPool::POOL_BUMP;
	}

	if (gltfp)
	{
		return LLDrawPool::POOL_MAT_PBR;
	}

	return matp ? LLDrawPool::POOL_MATERIALS : LLDrawPool::POOL_SIMPLE;
}

void LLPipeline::addPool(LLDrawPool* new_poolp)
{
	mPools.insert(new_poolp);
	addToQuickLookup(new_poolp);
}

void LLPipeline::allocDrawable(LLViewerObject* objp)
{
	LLDrawable* drawablep = new LLDrawable(objp);
	objp->mDrawable = drawablep;

	// Encompass completely sheared objects by taking the most extreme
	// point possible (<1.0, 1.0, 0.5>)
	F32 radius = LLVector3(1.f, 1.f, 0.5f).scaleVec(objp->getScale()).length();
	drawablep->setRadius(radius);
	if (objp->isOrphaned())
	{
		drawablep->setState(LLDrawable::FORCE_INVISIBLE);
	}
	drawablep->updateXform(true);
}

void LLPipeline::unlinkDrawable(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UNLINK);

	// Make sure the drawable does not get deleted before we are done
	LLPointer<LLDrawable> drawablep = drawable;

	// Based on flags, remove the drawable from the queues that it is on.
	if (drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		LL_FAST_TIMER(FTM_REMOVE_FROM_MOVE_LIST);
		for (U32 i = 0, count = mMovedList.size(); i < count; ++i)
		{
			if (mMovedList[i] == drawablep)
			{
				if (i < count - 1)
				{
					mMovedList[i] = std::move(mMovedList.back());
				}
				mMovedList.pop_back();
				break;
			}
		}
	}

	LLSpatialGroup* groupp = drawablep->getSpatialGroup();
	if (groupp && groupp->getSpatialPartition())
	{
		LL_FAST_TIMER(FTM_REMOVE_FROM_SPATIAL_PARTITION);
		if (!groupp->getSpatialPartition()->remove(drawablep, groupp))
		{
			llwarns << "Could not remove object from spatial group" << llendl;
			llassert(false);
		}
	}

	{
		LL_FAST_TIMER(FTM_REMOVE_FROM_LIGHT_SET);
		mLights.erase(drawablep);

		for (light_set_t::iterator iter = mNearbyLights.begin(),
								   end = mNearbyLights.end();
			 iter != end; ++iter)
		{
			if (iter->drawable == drawablep)
			{
				mNearbyLights.erase(iter);
				break;
			}
		}
	}

	for (U32 i = 0; i < 2; ++i)
	{
		if (mShadowSpotLight[i] == drawablep)
		{
			mShadowSpotLight[i] = NULL;
		}

		if (mTargetShadowSpotLight[i] == drawablep)
		{
			mTargetShadowSpotLight[i] = NULL;
		}
	}
}

U32 LLPipeline::addObject(LLViewerObject* objp)
{
	if (RenderDelayCreation)
	{
		mCreateQ.emplace_back(objp);
	}
	else
	{
		createObject(objp);
	}
	return 1;
}

void LLPipeline::createObjects(F32 max_dtime)
{
	LL_FAST_TIMER(FTM_PIPELINE_CREATE);

	LLTimer update_timer;
	while (!mCreateQ.empty() && update_timer.getElapsedTimeF32() < max_dtime)
	{
		LLViewerObject* objp = mCreateQ.front();
		if (objp && !objp->isDead())
		{
			createObject(objp);
		}
		mCreateQ.pop_front();
	}
}

void LLPipeline::createObject(LLViewerObject* objp)
{
	LLDrawable* drawablep = objp->mDrawable;
	if (!drawablep)
	{
		drawablep = objp->createDrawable();
		llassert(drawablep);
	}
	else
	{
		llerrs << "Redundant drawable creation !" << llendl;
	}

	LLViewerObject* parentp = (LLViewerObject*)objp->getParent();
	if (parentp)
	{
		// LLPipeline::addObject 1
		objp->setDrawableParent(parentp->mDrawable);
	}
	else
	{
		// LLPipeline::addObject 2
		objp->setDrawableParent(NULL);
	}

	markRebuild(drawablep);

	if (RenderAnimateRes && drawablep->getVOVolume())
	{
		// Fun animated res
		drawablep->updateXform(true);
		drawablep->clearState(LLDrawable::MOVE_UNDAMPED);
		drawablep->setScale(LLVector3::zero);
		drawablep->makeActive();
	}
}

void LLPipeline::resetFrameStats()
{
	static LLCachedControl<bool> render_info(gSavedSettings,
											 "DebugShowRenderInfo");
	if (render_info)
	{
		mNeedsDrawStats = true;
	}
	else if (mNeedsDrawStats && !LLFloaterStats::findInstance())
	{
		mNeedsDrawStats = false;
	}
	if (mNeedsDrawStats)
	{
		mTrianglesDrawnStat.addValue(F32(mTrianglesDrawn) * .001f);
		mTrianglesDrawn = 0;
	}

	if (mOldRenderDebugMask != mRenderDebugMask)
	{
		gObjectList.clearDebugText();
		mOldRenderDebugMask = mRenderDebugMask;
	}
}

// External functions for asynchronous updating
void LLPipeline::updateMoveDampedAsync(LLDrawable* drawablep)
{
	if (sFreezeTime)
	{
		return;
	}
	if (!drawablep)
	{
		llerrs << "Called with NULL drawable" << llendl;
		return;
	}
	if (drawablep->isState(LLDrawable::EARLY_MOVE))
	{
		return;
	}

	// Update drawable now
	drawablep->clearState(LLDrawable::MOVE_UNDAMPED); // Force to DAMPED
	drawablep->updateMove(); // Returns done
	// Flag says we already did an undamped move this frame:
	drawablep->setState(LLDrawable::EARLY_MOVE);
	// Put on move list so that EARLY_MOVE gets cleared
	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		mMovedList.emplace_back(drawablep);
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
}

void LLPipeline::updateMoveNormalAsync(LLDrawable* drawablep)
{
	if (sFreezeTime)
	{
		return;
	}
	if (!drawablep)
	{
		llerrs << "Called with NULL drawable" << llendl;
	}
	if (drawablep->isState(LLDrawable::EARLY_MOVE))
	{
		return;
	}

	// Update drawable now
	drawablep->setState(LLDrawable::MOVE_UNDAMPED); // Force to UNDAMPED
	drawablep->updateMove();
	// Flag says we already did an undamped move this frame:
	drawablep->setState(LLDrawable::EARLY_MOVE);
	// Put on move list so that EARLY_MOVE gets cleared
	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		mMovedList.emplace_back(drawablep);
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
}

void LLPipeline::updateMovedList(LLDrawable::draw_vec_t& moved_list)
{
	LL_TRACY_TIMER(TRC_MOVED_LIST);

	for (U32 i = 0, count = moved_list.size(); i < count; )
	{
		LLDrawable* drawablep = moved_list[i];
		bool done = true;
		if (!drawablep->isDead() &&
			!drawablep->isState(LLDrawable::EARLY_MOVE))
		{
			done = drawablep->updateMove();
		}
		drawablep->clearState(LLDrawable::EARLY_MOVE |
							  LLDrawable::MOVE_UNDAMPED);
		if (done)
		{
			if (drawablep->isRoot() && !drawablep->isState(LLDrawable::ACTIVE))
			{
				drawablep->makeStatic();
			}
			drawablep->clearState(LLDrawable::ON_MOVE_LIST);
			if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
			{
				// Will likely not receive any future world matrix updates;
				// this keeps attachments from getting stuck in space and
				// falling off your avatar
				drawablep->clearState(LLDrawable::ANIMATED_CHILD);
				markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
				LLViewerObject* objp = drawablep->getVObj().get();
				if (objp)
				{
					objp->dirtySpatialGroup();
				}
			}
			if (i < --count)
			{
				moved_list[i] = std::move(moved_list.back());
			}
			moved_list.pop_back();
		}
		else
		{
			++i;
		}
	}
}

void LLPipeline::updateMove(bool balance_vo_cache)
{
	LL_FAST_TIMER(FTM_UPDATE_MOVE);

	if (sFreezeTime)
	{
		return;
	}

	for (LLDrawable::draw_set_t::iterator iter = mRetexturedList.begin(),
										  end = mRetexturedList.end();
		 iter != end; ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (drawablep && !drawablep->isDead())
		{
			drawablep->updateTexture();
		}
	}
	mRetexturedList.clear();

	updateMovedList(mMovedList);

	// Balance octrees
	{
		LL_FAST_TIMER(FTM_OCTREE_BALANCE);

		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; ++i)
			{
				if (i == LLViewerRegion::PARTITION_VO_CACHE &&
					!balance_vo_cache)
				{
					continue;
				}
				LLSpatialPartition* partp = regionp->getSpatialPartition(i);
				if (partp)
				{
					partp->mOctree->balance();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Culling and occlusion testing
/////////////////////////////////////////////////////////////////////////////

//static
F32 LLPipeline::calcPixelArea(LLVector3 center, LLVector3 size,
							  LLCamera& camera)
{
	LLVector3 lookAt = center - camera.getOrigin();
	F32 dist = lookAt.length();

	// Ramp down distance for nearby objects; shrink dist by dist / 16.
	if (dist < 16.f)
	{
		dist /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}
	if (dist <= 0.f)
	{
		dist = F32_MIN;
	}

	// Get area of circle around node
	F32 app_angle = atanf(size.length() / dist);
	F32 radius = app_angle * LLDrawable::sCurPixelAngle;
	return radius * radius * F_PI;
}

//static
F32 LLPipeline::calcPixelArea(const LLVector4a& center, const LLVector4a& size,
							  LLCamera& camera)
{
	LLVector4a origin;
	origin.load3(camera.getOrigin().mV);

	LLVector4a lookAt;
	lookAt.setSub(center, origin);
	F32 dist = lookAt.getLength3().getF32();

	// Ramp down distance for nearby objects.
	// Shrink dist by dist/16.
	if (dist < 16.f)
	{
		dist *= 0.0625f; // 1/16
		dist *= dist;
		dist *= 16.f;
	}
	if (dist <= 0.f)
	{
		dist = F32_MIN;
	}

	// Get area of circle around node
	F32 app_angle = atanf(size.getLength3().getF32() / dist);
	F32 radius = app_angle * LLDrawable::sCurPixelAngle;
	return radius * radius * F_PI;
}

void LLPipeline::grabReferences(LLCullResult& result)
{
	sCull = &result;
}

void LLPipeline::clearReferences()
{
	sCull = NULL;
	mGroupSaveQ.clear();
}

#if LL_DEBUG
static void check_references(LLSpatialGroup* groupp, LLDrawable* drawablep)
{
	const LLSpatialGroup::element_list& data = groupp->getData();
	for (U32 i = 0, count = data.size(); i < count; ++i)
	{
		if (drawablep == (LLDrawable*)data[i]->getDrawable())
		{
			llerrs << "LLDrawable deleted while actively reference by LLPipeline."
				   << llendl;
		}
	}
}

static void check_references(LLDrawable* drawablep, LLFace* facep)
{
	for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
	{
		if (drawablep->getFace(i) == facep)
		{
			llerrs << "LLFace deleted while actively referenced by LLPipeline."
				   << llendl;
		}
	}
}

static void check_references(LLSpatialGroup* groupp, LLFace* facep)
{
	const LLSpatialGroup::element_list& data = groupp->getData();
	for (U32 i = 0, count = data.size(); i < count; ++i)
	{
		LLDrawable* drawablep = (LLDrawable*)data[i]->getDrawable();
		if (drawablep)
		{
			check_references(drawablep, facep);
		}
	}
}

static void check_references(LLSpatialGroup* groupp, LLDrawInfo* draw_infop)
{
	for (LLSpatialGroup::draw_map_t::const_iterator
			it = groupp->mDrawMap.begin(), end = groupp->mDrawMap.end();
		 it != end; ++it)
	{
		const LLSpatialGroup::drawmap_elem_t& draw_vec = it->second;
		for (U32 i = 0, count = draw_vec.size(); i < count; ++i)
		{
			LLDrawInfo* paramsp = draw_vec[i].get();
			if (paramsp == draw_infop)
			{
				llerrs << "LLDrawInfo deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}
	}
}

void LLPipeline::checkReferences(LLFace* facep)
{
	if (sCull)
	{
		LLCullResult::sg_list_t& visible_groups = sCull->getVisibleGroups();
		for (U32 i = 0, count = visible_groups.size(); i < count; ++i)
		{
			check_references(visible_groups[i], facep);
		}

		LLCullResult::sg_list_t& alpha_groups = sCull->getAlphaGroups();
		for (U32 i = 0, count = alpha_groups.size(); i < count; ++i)
		{
			check_references(alpha_groups[i], facep);
		}

		LLCullResult::sg_list_t& rig_groups = sCull->getRiggedAlphaGroups();
		for (U32 i = 0, count = rig_groups.size(); i < count; ++i)
		{
			check_references(rig_groups[i], facep);
		}

		LLCullResult::sg_list_t& draw_groups = sCull->getDrawableGroups();
		for (U32 i = 0, count = draw_groups.size(); i < count; ++i)
		{
			check_references(draw_groups[i], facep);
		}

		LLCullResult::drawable_list_t& visible = sCull->getVisibleList();
		for (U32 i = 0, count = visible.size(); i < count; ++i)
		{
			LLDrawable* drawablep = visible[i];
			check_references(drawablep, facep);
		}
	}
}

void LLPipeline::checkReferences(LLDrawable* drawablep)
{
	if (sCull)
	{
		LLCullResult::sg_list_t& visible_groups = sCull->getVisibleGroups();
		for (U32 i = 0, count = visible_groups.size(); i < count; ++i)
		{
			check_references(visible_groups[i], drawablep);
		}

		LLCullResult::sg_list_t& alpha_groups = sCull->getAlphaGroups();
		for (U32 i = 0, count = alpha_groups.size(); i < count; ++i)
		{
			check_references(alpha_groups[i], drawablep);
		}

		LLCullResult::sg_list_t& rig_groups = sCull->getRiggedAlphaGroups();
		for (U32 i = 0, count = rig_groups.size(); i < count; ++i)
		{
			check_references(rig_groups[i], drawablep);
		}

		LLCullResult::sg_list_t& draw_groups = sCull->getDrawableGroups();
		for (U32 i = 0, count = draw_groups.size(); i < count; ++i)
		{
			check_references(draw_groups[i], drawablep);
		}

		LLCullResult::drawable_list_t& visible = sCull->getVisibleList();
		for (U32 i = 0, count = visible.size(); i < count; ++i)
		{
			if (drawablep == visible[i])
			{
				llerrs << "LLDrawable deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}
	}
}

void LLPipeline::checkReferences(LLDrawInfo* draw_infop)
{
	if (sCull)
	{
		LLCullResult::sg_list_t& visible_groups = sCull->getVisibleGroups();
		for (U32 i = 0, count = visible_groups.size(); i < count; ++i)
		{
			check_references(visible_groups[i], draw_infop);
		}

		LLCullResult::sg_list_t& alpha_groups = sCull->getAlphaGroups();
		for (U32 i = 0, count = alpha_groups.size(); i < count; ++i)
		{
			check_references(alpha_groups[i], draw_infop);
		}

		LLCullResult::sg_list_t& rig_groups = sCull->getRiggedAlphaGroups();
		for (U32 i = 0, count = rig_groups.size(); i < count; ++i)
		{
			check_references(rig_groups[i], draw_infop);
		}

		LLCullResult::sg_list_t& draw_groups = sCull->getDrawableGroups();
		for (U32 i = 0, count = draw_groups.size(); i < count; ++i)
		{
			check_references(draw_groups[i], draw_infop);
		}
	}
}

void LLPipeline::checkReferences(LLSpatialGroup* groupp)
{
	if (sCull)
	{
		LLCullResult::sg_list_t& visible_groups = sCull->getVisibleGroups();
		for (U32 i = 0, count = visible_groups.size(); i < count; ++i)
		{
			if (groupp == visible_groups[i])
			{
				llerrs << "LLSpatialGroup deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}

		LLCullResult::sg_list_t& alpha_groups = sCull->getAlphaGroups();
		for (U32 i = 0, count = alpha_groups.size(); i < count; ++i)
		{
			if (groupp == alpha_groups[i])
			{
				llerrs << "LLSpatialGroup deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}

		LLCullResult::sg_list_t& rig_groups = sCull->getRiggedAlphaGroups();
		for (U32 i = 0, count = rig_groups.size(); i < count; ++i)
		{
			if (groupp == rig_groups[i])
			{
				llerrs << "LLSpatialGroup deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}

		LLCullResult::sg_list_t& draw_groups = sCull->getDrawableGroups();
		for (U32 i = 0, count = draw_groups.size(); i < count; ++i)
		{
			if (groupp == draw_groups[i])
			{
				llerrs << "LLSpatialGroup deleted while actively referenced by LLPipeline."
					   << llendl;
			}
		}
	}
}
#endif

bool LLPipeline::getVisibleExtents(LLCamera& camera, LLVector3& min,
								   LLVector3& max)
{
	constexpr F32 max_val = 65536.f;
	constexpr F32 min_val = -65536.f;
	min.set(max_val, max_val, max_val);
	max.set(min_val, min_val, min_val);

	S32 saved_camera_id = LLViewerCamera::sCurCameraID;
	LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

	bool res = true;

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;

		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* partp = regionp->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (hasRenderType(partp->mDrawableType) &&
				!partp->getVisibleExtents(camera, min, max))
			{
				res = false;
			}
		}
	}

	LLViewerCamera::sCurCameraID = saved_camera_id;

	return res;
}

//static
bool LLPipeline::isWaterClip()
{
	return (!RenderTransparentWater || gCubeSnapshot) && !sRenderingHUDs;
}

// Branched version for the PBR renderer
void LLPipeline::updateCullPBR(LLCamera& camera, LLCullResult& result)
{
	if (isWaterClip())
	{
		LLVector3 pnorm;
		if (sUnderWaterRender)
		{
			// Camera is below water, cull above water
			pnorm.set(0.f, 0.f, 1.f);
		}
		else
		{
			// Camera is above water, cull below water
			pnorm.set(0.f, 0.f, -1.f);
		}

		LLPlane plane(LLVector3(0.f, 0.f, mWaterHeight), pnorm);
		camera.setUserClipPlane(plane);
	}
	else
	{
		camera.disableUserClipPlane();
	}

	grabReferences(result);

	sCull->clear();

	bool do_occlusion_cull = sUseOcclusion > 0;
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;

		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* partp = regionp->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (hasRenderType(partp->mDrawableType))
			{
				partp->cull(camera);
			}
		}

		// Scan the VO Cache tree
		LLVOCachePartition* vo_partp = regionp->getVOCachePartition();
		if (vo_partp)
		{
			vo_partp->cull(camera, do_occlusion_cull);
		}
	}

	if (hasRenderType(RENDER_TYPE_SKY) &&
	    gSky.mVOSkyp.notNull() && gSky.mVOSkyp->mDrawable.notNull())
	{
		gSky.mVOSkyp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOSkyp->mDrawable);
		gSky.updateCull();
	}

	if (hasRenderType(RENDER_TYPE_WL_SKY) &&
		gSky.mVOWLSkyp.notNull() && gSky.mVOWLSkyp->mDrawable.notNull())
	{
		gSky.mVOWLSkyp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOWLSkyp->mDrawable);
	}
}

void LLPipeline::updateCull(LLCamera& camera, LLCullResult& result,
							LLPlane* planep, bool hud_attachments)
{
	LL_FAST_TIMER(FTM_CULL);

	if (gUsePBRShaders)
	{
		updateCullPBR(camera, result);
		return;
	}

	if (planep)
	{
		camera.setUserClipPlane(*planep);
	}
	else
	{
		camera.disableUserClipPlane();
	}

	grabReferences(result);

	sCull->clear();

	bool to_texture = sUseOcclusion > 1 && shadersLoaded();
	if (to_texture)
	{
		if (sRenderDeferred && sUseOcclusion > 1)
		{
			mOcclusionDepth.bindTarget();
		}
		else
		{
			mRT->mScreen.bindTarget();
		}
	}

	if (sUseOcclusion > 1)
	{
		gGL.setColorMask(false, false);
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(gGLLastProjection);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLLastModelView);

	LLGLDisable blend(GL_BLEND);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	bool bound_shader = false;
	if (shadersLoaded() && !LLGLSLShader::sCurBoundShader)
	{
		// If no shader is currently bound, use the occlusion shader instead of
		// fixed function if we can (shadow render uses a special shader that
		// clamps to clip planes)
		bound_shader = true;
		gOcclusionCubeProgram.bind();
	}

	if (sUseOcclusion > 1)
	{
		if (mCubeVB.isNull())
		{
			sUseOcclusion = 0;
			llwarns << "No available Cube VB, disabling occlusion" << llendl;
		}
		else
		{
			mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
		}
	}

	bool do_occlusion_cull = sUseOcclusion > 1 && !gUseWireframe;
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;

		for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
		{
			LLSpatialPartition* partp = regionp->getSpatialPartition(i);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (hasRenderType(partp->mDrawableType) ||
				(!hud_attachments && i == LLViewerRegion::PARTITION_BRIDGE))
			{
				partp->cull(camera);
			}
		}

		// Scan the VO Cache tree
		LLVOCachePartition* vo_partp = regionp->getVOCachePartition();
		if (vo_partp)
		{
			vo_partp->cull(camera, do_occlusion_cull);
		}
	}

	if (bound_shader)
	{
		gOcclusionCubeProgram.unbind();
	}

	if (hasRenderType(RENDER_TYPE_SKY) &&
	    gSky.mVOSkyp.notNull() && gSky.mVOSkyp->mDrawable.notNull())
	{
		gSky.mVOSkyp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOSkyp->mDrawable);
		gSky.updateCull();
	}

	bool can_use_wl_shaders = canUseWindLightShaders();

	if (can_use_wl_shaders &&
		hasRenderType(RENDER_TYPE_WL_SKY) &&
		gSky.mVOWLSkyp.notNull() && gSky.mVOWLSkyp->mDrawable.notNull())
	{
		gSky.mVOWLSkyp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOWLSkyp->mDrawable);
	}

	if (!sReflectionRender &&
		(hasRenderType(RENDER_TYPE_WATER) ||
		 hasRenderType(RENDER_TYPE_VOIDWATER)))
	{
		gWorld.precullWaterObjects(camera, sCull);
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	if (sUseOcclusion > 1)
	{
		gGL.setColorMask(true, false);
	}

	if (to_texture)
	{
		if (sRenderDeferred && sUseOcclusion > 1)
		{
			mOcclusionDepth.flush();
		}
		else
		{
			mRT->mScreen.flush();
		}
	}
}

void LLPipeline::markNotCulled(LLSpatialGroup* groupp, LLCamera& camera)
{
	if (groupp->isEmpty())
	{
		return;
	}

	groupp->setVisible();

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
		!gCubeSnapshot)
	{
		groupp->updateDistance(camera);
	}

	if (!groupp->getSpatialPartition()->mRenderByGroup)
	{
		// Render by drawable
		sCull->pushDrawableGroup(groupp);
	}
	else
	{
		// Render by group
		sCull->pushVisibleGroup(groupp);
	}

	++mNumVisibleNodes;

	if (!gUsePBRShaders)
	{
		return;
	}

	S32 frame = LLViewerOctreeEntryData::getCurrentFrame() - 1;
	if (groupp->needsUpdate() ||
		groupp->getVisible(LLViewerCamera::sCurCameraID) < frame)
	{
		// Include this group in occlusion groups, not because we know it is an
		// occluder, but because we want to run an occlusion query to find out
		// if it is an occluder.
		markOccluder(groupp);
	}
}

void LLPipeline::markOccluder(LLSpatialGroup* groupp)
{
	if (sUseOcclusion > 1 && groupp &&
		!groupp->isOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION))
	{
		LLSpatialGroup* parentp = groupp->getParent();
		if (!parentp || !parentp->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			// Only mark top most occluders as active occlusion
			sCull->pushOcclusionGroup(groupp);
			groupp->setOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);

			if (parentp &&
				!parentp->isOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION) &&
				parentp->getElementCount() == 0 && parentp->needsUpdate())
			{
				sCull->pushOcclusionGroup(groupp);
				parentp->setOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
			}
		}
	}
}

// Used only by the EE renderer
void LLPipeline::downsampleDepthBuffer(LLRenderTarget& source,
									   LLRenderTarget& dest,
									   LLRenderTarget* scratch_space)
{
	LLGLSLShader* last_shaderp = LLGLSLShader::sCurBoundShaderPtr;

	if (scratch_space)
	{
		scratch_space->copyContents(source, 0, 0,
									source.getWidth(), source.getHeight(),
									0, 0, scratch_space->getWidth(),
									scratch_space->getHeight(),
									GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	dest.bindTarget();
	dest.clear(GL_DEPTH_BUFFER_BIT);

	LLStrider<LLVector3> vert;
	if (mDeferredVB.isNull() || !mDeferredVB->getVertexStrider(vert))
	{
		return;
	}
	vert[0].set(-1.f, 1.f, 0.f);
	vert[1].set(-1.f, -3.f, 0.f);
	vert[2].set(3.f, 1.f, 0.f);

	LLGLSLShader* shaderp;
	if (source.getUsage() == LLTexUnit::TT_RECT_TEXTURE)
	{
		shaderp = &gDownsampleDepthRectProgram;
		shaderp->bind();
		shaderp->uniform2f(sDelta, 1.f, 1.f);
		shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
						   source.getWidth(), source.getHeight());
	}
	else
	{
		shaderp = &gDownsampleDepthProgram;
		shaderp->bind();
		shaderp->uniform2f(sDelta, 1.f / source.getWidth(),
						   1.f / source.getHeight());
		shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, 1.f, 1.f);
	}

	gGL.getTexUnit(0)->bind(scratch_space ? scratch_space : &source, true);

	{
		LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
		mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
		mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	}

	dest.flush();

	if (last_shaderp)
	{
		last_shaderp->bind();
	}
	else
	{
		shaderp->unbind();
	}
}

// Used only by the EE renderer
void LLPipeline::doOcclusion(LLCamera& camera, LLRenderTarget& source,
							 LLRenderTarget& dest,
							 LLRenderTarget* scratch_space)
{
	downsampleDepthBuffer(source, dest, scratch_space);
	dest.bindTarget();
	doOcclusion(camera);
	dest.flush();
}

// Branched version for the PBR renderer
void LLPipeline::doOcclusionPBR(LLCamera& camera)
{
	if (sReflectionProbesEnabled && !sShadowRender && !gCubeSnapshot)
	{
		gGL.setColorMask(false, false);
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		LLGLDisable cull(GL_CULL_FACE);

		gOcclusionCubeProgram.bind();
		mCubeVB->setBuffer();
		mReflectionMapManager.doOcclusion();
		gOcclusionCubeProgram.unbind();

		gGL.setColorMask(true, true);
	}

	if (!sCull->hasOcclusionGroups() &&
		!LLVOCachePartition::sNeedsOcclusionCheck)
	{
		return;
	}

	LLVertexBuffer::unbind();
	gGL.setColorMask(false, false);

	LLGLDisable blend(GL_BLEND);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	LLGLDepthTest depth(GL_TRUE, GL_FALSE);
	LLGLDisable cull(GL_CULL_FACE);

	gOcclusionCubeProgram.bind();
	mCubeVB->setBuffer();

	LLCullResult::sg_list_t& occ_groups = sCull->getOcclusionGroups();
	for (U32 i = 0, count = occ_groups.size(); i < count; ++i)
	{
		LLSpatialGroup* groupp = occ_groups[i];
		if (!groupp->isDead())
		{
			groupp->doOcclusion(&camera);
			groupp->clearOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
		}
	}

	// Apply occlusion culling to object cache tree
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLVOCachePartition* partp = (*iter)->getVOCachePartition();
		if (partp)
		{
			partp->processOccluders(&camera);
		}
	}

	gGL.setColorMask(true, true);
}

void LLPipeline::doOcclusion(LLCamera& camera)
{
	if (sUseOcclusion <= 1 || LLSpatialPartition::sTeleportRequested)
	{
		return;
	}

	if (mCubeVB.isNull())
	{
		sUseOcclusion = 0;
		llwarns << "No available Cube VB, disabling occlusion" << llendl;
		return;
	}

	if (gUsePBRShaders)
	{
		doOcclusionPBR(camera);
		return;
	}

	if (!sCull->hasOcclusionGroups() &&
		!LLVOCachePartition::sNeedsOcclusionCheck)
	{
		return;
	}

	LLVertexBuffer::unbind();

	if (hasRenderDebugMask(RENDER_DEBUG_OCCLUSION))
	{
		gGL.setColorMask(true, false, false, false);
	}
	else
	{
		gGL.setColorMask(false, false);
	}
	LLGLDisable blend(GL_BLEND);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	LLGLDisable cull(GL_CULL_FACE);

	bool bind_shader = !LLGLSLShader::sCurBoundShader;
	if (bind_shader)
	{
		if (sShadowRender)
		{
			gDeferredShadowCubeProgram.bind();
		}
		else
		{
			gOcclusionCubeProgram.bind();
		}
	}

	mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

	LLCullResult::sg_list_t& occ_groups = sCull->getOcclusionGroups();
	for (U32 i = 0, count = occ_groups.size(); i < count; ++i)
	{
		LLSpatialGroup* groupp = occ_groups[i];
		if (!groupp->isDead())
		{
			groupp->doOcclusion(&camera);
			groupp->clearOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
		}
	}

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
	{
		// Apply occlusion culling to object cache tree
		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLVOCachePartition* partp = (*iter)->getVOCachePartition();
			if (partp)
			{
				partp->processOccluders(&camera);
			}
		}
	}

	if (bind_shader)
	{
		if (sShadowRender)
		{
			gDeferredShadowCubeProgram.unbind();
		}
		else
		{
			gOcclusionCubeProgram.unbind();
		}
	}

	gGL.setColorMask(true, false);
}

bool LLPipeline::updateDrawableGeom(LLDrawable* drawablep)
{
	bool update_complete = drawablep->updateGeometry();
	if (update_complete)
	{
		drawablep->setState(LLDrawable::BUILT);
	}
	return update_complete;
}

void LLPipeline::updateGL()
{
	LL_FAST_TIMER(FTM_UPDATE_GL);
	while (!LLGLUpdate::sGLQ.empty())
	{
		LLGLUpdate* glup = LLGLUpdate::sGLQ.front();
		glup->updateGL();
		glup->mInQ = false;
		LLGLUpdate::sGLQ.pop_front();
		LL_DEBUGS("MarkGLRebuild") << "Rebuilt GL for: " << std::hex
								   << (intptr_t)glup << std::dec << LL_ENDL;
	}
}

// Iterates through all groups on the build queue and remove all the groups
// that do not correspond to HUD objects.
void LLPipeline::clearRebuildGroups()
{
	mGroupQLocked = true;
	for (S32 i = 0, count = mGroupQ.size(); i < count; )
	{
		LLSpatialGroup* groupp = mGroupQ[i].get();
		if (!groupp->isHUDGroup())
		{
			// Not a HUD object, so clear the build state.
			// NOTE: since isHUDGroup() also tests for isDead() and returns
			// false when the group is dead, dead groups will also be properly
			// removed from mGroupQ. HB
			groupp->clearState(LLSpatialGroup::IN_BUILD_QUEUE);
			if (i < --count)
			{
				mGroupQ[i] = std::move(mGroupQ.back());
			}
			mGroupQ.pop_back();
		}
		else
		{
			++i;
		}
	}
	mGroupQLocked = false;
}

void LLPipeline::clearRebuildDrawables()
{
	// Clear all drawables on the build queue,
	for (LLDrawable::draw_list_t::iterator iter = mBuildQ.begin(),
										   end = mBuildQ.end();
		 iter != end; ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (drawablep && !drawablep->isDead())
		{
			drawablep->clearState(LLDrawable::IN_REBUILD_QUEUE);
		}
	}
	mBuildQ.clear();

	// Clear all moving bridges
	U32 bits = LLDrawable::EARLY_MOVE | LLDrawable::MOVE_UNDAMPED |
			   LLDrawable::ON_MOVE_LIST | LLDrawable::ANIMATED_CHILD;
	for (U32 i = 0, count = mMovedBridge.size(); i < count; ++i)
	{
		LLDrawable* drawablep = mMovedBridge[i];
		if (drawablep)
		{
			drawablep->clearState(bits);
		}
	}
	mMovedBridge.clear();

	// Clear all moving drawables
	for (U32 i = 0, count = mMovedList.size(); i < count; ++i)
	{
		LLDrawable* drawablep = mMovedList[i];
		if (drawablep)
		{
			drawablep->clearState(bits);
		}
	}
	mMovedList.clear();

	// Clear all shifting drawables
	bits |= LLDrawable::ON_SHIFT_LIST;
	for (U32 i = 0, count = mShiftList.size(); i < count; ++i)
	{
		LLDrawable* drawablep = mShiftList[i];
		if (drawablep)
		{
			drawablep->clearState(bits);
		}
	}
	mShiftList.clear();
}

void LLPipeline::rebuildPriorityGroups()
{
	LL_FAST_TIMER(FTM_REBUILD_PRIORITY_GROUPS);

	{
		LL_FAST_TIMER(FTM_REBUILD_MESH);
		gMeshRepo.notifyLoadedMeshes();
	}

	mGroupQLocked = true;
	// Iterate through all drawables on the build queue
	for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ.begin();
		 iter != mGroupQ.end(); ++iter)
	{
		LLSpatialGroup* groupp = iter->get();
		groupp->rebuildGeom();
		groupp->clearState(LLSpatialGroup::IN_BUILD_QUEUE);
	}

	mGroupSaveQ.clear();
	mGroupSaveQ.swap(mGroupQ);	// This clears mGroupQ
	mGroupQLocked = false;
}

void LLPipeline::updateGeom(F32 max_dtime)
{
	if (gCubeSnapshot)
	{
		return;
	}

	LL_FAST_TIMER(FTM_GEO_UPDATE);

	// Notify various object types to reset internal cost metrics, etc.
	// for now, only LLVOVolume does this to throttle LOD changes
	LLVOVolume::preUpdateGeom();

	LLPointer<LLDrawable> drawablep;
	// Iterate through all drawables on the priority build queue,
	for (LLDrawable::draw_list_t::iterator iter = mBuildQ.begin(),
										   end = mBuildQ.end();
		 iter != end; )
	{
		LLDrawable::draw_list_t::iterator curiter = iter++;
		drawablep = *curiter;
		bool remove = drawablep.isNull() || drawablep->isDead();
		if (!remove)
		{
			remove = updateDrawableGeom(drawablep);
		}
		if (remove)
		{
			if (drawablep)
			{
				drawablep->clearState(LLDrawable::IN_REBUILD_QUEUE);
			}
			mBuildQ.erase(curiter);
		}
	}

	updateMovedList(mMovedBridge);
}

void LLPipeline::markVisible(LLDrawable* drawablep, LLCamera& camera)
{
	if (!drawablep || drawablep->isDead())
	{
		return;
	}

	if (drawablep->isSpatialBridge())
	{
		const LLDrawable* rootp = ((LLSpatialBridge*)drawablep)->mDrawable;
		if (rootp && rootp->getVObj()->isAttachment())
		{
			LLDrawable* parentp = rootp->getParent();
			if (parentp) // This IS sometimes NULL
			{
				LLViewerObject* objp = parentp->getVObj();
				if (objp)
				{
					LLVOAvatar* avp = objp->asAvatar();
					if (avp &&
						(avp->isImpostor() || avp->isInMuteList() ||
						 (avp->getVisualMuteSettings() ==
							LLVOAvatar::AV_DO_NOT_RENDER &&
						  !avp->needsImpostorUpdate())))
					{
						return;
					}
				}
			}
		}
		sCull->pushBridge((LLSpatialBridge*)drawablep);
	}
	else
	{
		sCull->pushDrawable(drawablep);
	}

	drawablep->setVisible(camera);
}

void LLPipeline::markMoved(LLDrawable* drawablep, bool damped_motion)
{
	if (!drawablep || drawablep->isDead())
	{
		return;
	}

	LLDrawable* parentp = drawablep->getParent();
	if (parentp)
	{
		// Ensure that parent drawables are moved first
		markMoved(parentp, damped_motion);
	}

	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		if (drawablep->isSpatialBridge())
		{
			mMovedBridge.emplace_back(drawablep);
		}
		else
		{
			mMovedList.emplace_back(drawablep);
		}
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
	if (!damped_motion)
	{
		// UNDAMPED trumps DAMPED
		drawablep->setState(LLDrawable::MOVE_UNDAMPED);
	}
	else if (drawablep->isState(LLDrawable::MOVE_UNDAMPED))
	{
		drawablep->clearState(LLDrawable::MOVE_UNDAMPED);
	}
}

void LLPipeline::markShift(LLDrawable* drawablep)
{
	if (!drawablep || drawablep->isDead())
	{
		return;
	}

	if (!drawablep->isState(LLDrawable::ON_SHIFT_LIST))
	{
		LLViewerObject* objp = drawablep->getVObj().get();
		if (objp)
		{
			objp->setChanged(LLXform::SHIFTED | LLXform::SILHOUETTE);
		}
		LLDrawable* parentp = drawablep->getParent();
		if (parentp)
		{
			markShift(parentp);
		}
		mShiftList.emplace_back(drawablep);
		drawablep->setState(LLDrawable::ON_SHIFT_LIST);
	}
}

void LLPipeline::shiftObjects(const LLVector3& offset)
{
	glClear(GL_DEPTH_BUFFER_BIT);
	gDepthDirty = true;

	LLVector4a offseta;
	offseta.load3(offset.mV);

	{
		LL_FAST_TIMER(FTM_SHIFT_DRAWABLE);
		for (U32 i = 0, count = mShiftList.size(); i < count; ++i)
		{
			LLDrawable* drawablep = mShiftList[i].get();
			if (drawablep && !drawablep->isDead())
			{
				drawablep->shiftPos(offseta);
				drawablep->clearState(LLDrawable::ON_SHIFT_LIST);
			}
		}
		mShiftList.clear();
	}

	{
		LL_FAST_TIMER(FTM_SHIFT_OCTREE);
		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; ++i)
			{
				LLSpatialPartition* partp = regionp->getSpatialPartition(i);
				if (partp)
				{
					partp->shift(offseta);
				}
			}
		}
	}

	if (gUsePBRShaders)
	{
		mReflectionMapManager.shift(offseta);
	}

	{
		LL_FAST_TIMER(FTM_SHIFT_HUD);
		LLHUDText::shiftAll(offset);
	}

	display_update_camera();
}

void LLPipeline::markTextured(LLDrawable* drawablep)
{
	if (drawablep && !drawablep->isDead())
	{
		mRetexturedList.emplace(drawablep);
	}
}

void LLPipeline::markGLRebuild(LLGLUpdate* glup)
{
	if (glup && !glup->mInQ)
	{
		LLGLUpdate::sGLQ.push_back(glup);
		glup->mInQ = true;
	}
}

void LLPipeline::markPartitionMove(LLDrawable* drawablep)
{
	if (!drawablep->isState(LLDrawable::PARTITION_MOVE) &&
		!drawablep->getPositionGroup().equals3(LLVector4a::getZero()))
	{
		drawablep->setState(LLDrawable::PARTITION_MOVE);
		mPartitionQ.emplace_back(drawablep);
	}
}

void LLPipeline::processPartitionQ()
{
	LL_FAST_TIMER(FTM_PROCESS_PARTITIONQ);
	for (U32 i = 0, count = mPartitionQ.size(); i < count; ++i)
	{
		LLDrawable* drawablep = mPartitionQ[i].get();
		if (!drawablep) continue;	// Paranoia

		if (!drawablep->isDead())
		{
			drawablep->updateBinRadius();
			drawablep->movePartition();
		}
		drawablep->clearState(LLDrawable::PARTITION_MOVE);
	}
	mPartitionQ.clear();
}

void LLPipeline::markMeshDirty(LLSpatialGroup* groupp)
{
	mMeshDirtyGroup.emplace_back(groupp);
}

void LLPipeline::markRebuild(LLSpatialGroup* groupp)
{
	if (groupp && !groupp->isDead() && groupp->getSpatialPartition() &&
		!groupp->hasState(LLSpatialGroup::IN_BUILD_QUEUE))
	{
		mGroupQ.emplace_back(groupp);
		groupp->setState(LLSpatialGroup::IN_BUILD_QUEUE);
	}
}

void LLPipeline::markRebuild(LLDrawable* drawablep,
							 LLDrawable::EDrawableFlags flag)
{
	if (drawablep && !drawablep->isDead())
	{
		if (!drawablep->isState(LLDrawable::IN_REBUILD_QUEUE))
		{
			mBuildQ.emplace_back(drawablep);
			// Mark drawable as being in build queue
			drawablep->setState(LLDrawable::IN_REBUILD_QUEUE);
		}
		if (flag & (LLDrawable::REBUILD_VOLUME | LLDrawable::REBUILD_POSITION))
		{
			LLViewerObject* objp = drawablep->getVObj().get();
			if (objp)
			{
				objp->setChanged(LLXform::SILHOUETTE);
			}
		}
		drawablep->setState(flag);
	}
}

void LLPipeline::stateSort(LLCamera& camera, LLCullResult& result)
{
	if (hasAnyRenderType(RENDER_TYPE_AVATAR, RENDER_TYPE_PUPPET,
						 RENDER_TYPE_TERRAIN, RENDER_TYPE_TREE,
						 RENDER_TYPE_SKY, RENDER_TYPE_VOIDWATER,
						 RENDER_TYPE_WATER, END_RENDER_TYPES))
	{
		// Clear faces from face pools
		LL_FAST_TIMER(FTM_RESET_DRAWORDER);
		resetDrawOrders();
	}

	LL_FAST_TIMER(FTM_STATESORT);

	grabReferences(result);

	LLCullResult::sg_list_t& draw_groups = sCull->getDrawableGroups();
	for (U32 i = 0, count = draw_groups.size(); i < count; ++i)
	{
		LLSpatialGroup* groupp = draw_groups[i];
		if (!groupp || groupp->isDead()) continue;

		groupp->checkOcclusion();
		if (sUseOcclusion > 1 &&
			groupp->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			markOccluder(groupp);
			continue;
		}

		groupp->setVisible();

		const LLSpatialGroup::element_list& data = groupp->getData();
		for (U32 j = 0, count2 = data.size(); j < count2; ++j)
		{
			LLDrawable* drawablep = (LLDrawable*)data[j]->getDrawable();
			markVisible(drawablep, camera);
		}

		// Rebuild mesh as soon as we know it is visible
		groupp->rebuildMesh();
	}

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
		!gCubeSnapshot)
	{
		bool fov_changed = gViewerCamera.isDefaultFOVChanged();
		LLSpatialGroup* last_groupp = NULL;

		LLCullResult::bridge_list_t& bridges = sCull->getVisibleBridge();
		for (U32 i = 0, count = bridges.size(); i < count; ++i)
		{
			LLSpatialBridge* bridgep = bridges[i];
			if (!bridgep) continue;

			LLSpatialGroup* groupp = bridgep->getSpatialGroup();
			if (groupp->isDead()) continue;

			if (!last_groupp)
			{
				last_groupp = groupp;
			}

			if (groupp && !bridgep->isDead() &&
				!groupp->isOcclusionState(LLSpatialGroup::OCCLUDED))
			{
				stateSort(bridgep, camera, fov_changed);
			}

			if (last_groupp && last_groupp != groupp &&
				last_groupp->changeLOD())
			{
				last_groupp->mLastUpdateDistance = last_groupp->mDistance;
			}

			last_groupp = groupp;
		}

		if (last_groupp && last_groupp->changeLOD())
		{
			last_groupp->mLastUpdateDistance = last_groupp->mDistance;
		}
	}

	LLCullResult::sg_list_t& visible_groups = sCull->getVisibleGroups();
	for (U32 i = 0, count = visible_groups.size(); i < count; ++i)
	{
		LLSpatialGroup* groupp = visible_groups[i];
		if (!groupp || groupp->isDead()) continue;

		groupp->checkOcclusion();
		if (sUseOcclusion > 1 &&
			groupp->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			markOccluder(groupp);
		}
		else
		{
			groupp->setVisible();
			stateSort(groupp, camera);

			// Rebuild mesh as soon as we know it is visible
			groupp->rebuildMesh();
		}
	}

	{
		LL_FAST_TIMER(FTM_STATESORT_DRAWABLE);

		LLCullResult::drawable_list_t& visible = sCull->getVisibleList();
		for (U32 i = 0, count = visible.size(); i < count; ++i)
		{
			LLDrawable* drawablep = visible[i];
			if (drawablep && !drawablep->isDead())
			{
				stateSort(drawablep, camera);
			}
		}
	}

	postSort(camera);
}

void LLPipeline::stateSort(LLSpatialGroup* groupp, LLCamera& camera)
{
	if (groupp->changeLOD())
	{
		const LLSpatialGroup::element_list& data = groupp->getData();
		for (U32 i = 0, count = data.size(); i < count; ++i)
		{
			LLDrawable* drawablep = (LLDrawable*)data[i]->getDrawable();
			stateSort(drawablep, camera);
		}

		if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
			!gCubeSnapshot)
		{
			// Avoid redundant stateSort calls
			groupp->mLastUpdateDistance = groupp->mDistance;
		}
	}
}

void LLPipeline::stateSort(LLSpatialBridge* bridgep, LLCamera& camera,
						   bool fov_changed)
{
	if (fov_changed || bridgep->getSpatialGroup()->changeLOD())
	{
		// false = do not force update
		bridgep->updateDistance(camera, false);
	}
}

void LLPipeline::stateSort(LLDrawable* drawablep, LLCamera& camera)
{
	if (!drawablep || drawablep->isDead() ||
		!hasRenderType(drawablep->getRenderType()))
	{
		return;
	}

	// SL-11353: ignore our own geometry when rendering spotlight shadowmaps...
	if (drawablep == sRenderSpotLight)
	{
		return;
	}

	LLViewerObject* objp = drawablep->getVObj().get();
	if (gSelectMgr.mHideSelectedObjects && objp && objp->isSelected() &&
//MK
		(!gRLenabled || !gRLInterface.mContainsEdit))
//mk
	{
		return;
	}

	if (drawablep->isAvatar())
	{
		// Do not draw avatars beyond render distance or if we do not have a
		// spatial group.
		LLSpatialGroup* groupp = drawablep->getSpatialGroup();
		if (!groupp || groupp->mDistance > LLVOAvatar::sRenderDistance)
		{
			return;
		}

		LLVOAvatar* avatarp = (LLVOAvatar*)objp;
		if (avatarp && !avatarp->isVisible())
		{
			return;
		}
	}

	if (hasRenderType(drawablep->mRenderType) &&
		!drawablep->isState(LLDrawable::INVISIBLE |
							LLDrawable::FORCE_INVISIBLE))
	{
		drawablep->setVisible(camera, NULL, false);
	}

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
		!gCubeSnapshot)
	{
#if 0	// isVisible() check here is redundant, if it was not visible, it would
		// not be here
		if (drawablep->isVisible())
#endif
		{
			if (!drawablep->isActive())
			{
				// false = do not force update = false;
				drawablep->updateDistance(camera, false);
			}
			else if (drawablep->isAvatar())
			{
				// Calls objp->updateLOD() which calls
				// LLVOAvatar::updateVisibility()
				drawablep->updateDistance(camera, false);
			}
		}
	}

	if (!drawablep->getVOVolume())
	{
		for (U32 i = 0, count = drawablep->mFaces.size(); i < count; ++i)
		{
			LLFace* facep = drawablep->mFaces[i];
			if (facep->hasGeometry())
			{
				LLFacePool* poolp = facep->getPool();
				if (!poolp)
				{
					break;
				}
				poolp->enqueue(facep);
			}
		}
	}
}

void forAllDrawables(LLCullResult::sg_list_t& group_data,
					 void (*func)(LLDrawable*))
{
	for (U32 i = 0, count = group_data.size(); i < count; ++i)
	{
		LLSpatialGroup* groupp = group_data[i];
		if (groupp->isDead()) continue;

		const LLSpatialGroup::element_list& octree_data = groupp->getData();
		for (U32 j = 0, count2 = octree_data.size(); j < count2; ++j)
		{
			if (octree_data[j]->hasDrawable())
			{
				func((LLDrawable*)octree_data[j]->getDrawable());
			}
		}
	}
}

void LLPipeline::forAllVisibleDrawables(void (*func)(LLDrawable*))
{
	forAllDrawables(sCull->getDrawableGroups(), func);
	forAllDrawables(sCull->getVisibleGroups(), func);
}

//static
U32 LLPipeline::highlightable(const LLViewerObject* objp)
{
	if (!objp || objp->isAvatar()) return 0;
	if (sRenderByOwner == 1 && !objp->permYouOwner()) return 0;
	if (sRenderByOwner == 2 && objp->permYouOwner()) return 0;
	LLViewerObject* parentp = (LLViewerObject*)objp->getParent();
	if (!parentp) return 1;
	if (!sRenderAttachments) return 0;

	// Attachments can be highlighted but are not marked with beacons since
	// it would mark the avatar itself... But, we highlight all the
	// primitives of the attachments instead of just the root primitive
	// (which could be buried into the avatar or be too small to be
	// visible). HB
	if (parentp->isAvatar())
	{
		return 2;
	}
	LLViewerObject* rootp = (LLViewerObject*)objp->getRoot();
	if (rootp && rootp->isAvatar())
	{
		return 2;
	}

	return 0;
}

// Function for creating scripted beacons
void renderScriptedBeacons(LLDrawable* drawablep)
{
	LLViewerObject* objp = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(objp);
	if (type != 0 && objp->flagScripted())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(objp->getPositionAgent(), "",
									   LLColor4(1.f, 0.f, 0.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderScriptedTouchBeacons(LLDrawable* drawablep)
{
	LLViewerObject* objp = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(objp);
	if (type != 0 && objp->flagScripted() && objp->flagHandleTouch())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(objp->getPositionAgent(), "",
									   LLColor4(1.f, 0.f, 0.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderPhysicalBeacons(LLDrawable* drawablep)
{
	LLViewerObject* objp = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(objp);
	if (type != 0 && objp->flagUsePhysics())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(objp->getPositionAgent(), "",
									   LLColor4(0.f, 1.f, 0.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderPermanentBeacons(LLDrawable* drawablep)
{
	LLViewerObject* objp = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(objp);
	if (type != 0 && objp->flagObjectPermanent())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(objp->getPositionAgent(), "",
									   LLColor4(0.f, 1.f, 1.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderCharacterBeacons(LLDrawable* drawablep)
{
	LLViewerObject* objp = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(objp);
	if (type != 0 && objp->flagCharacter())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(objp->getPositionAgent(), "",
									   LLColor4(0.5f, 0.5f, 0.5f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderSoundBeacons(LLDrawable* drawablep)
{
	// Look for attachments, objects, etc.
	LLViewerObject* objp = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(objp);
	if (type != 0 && objp->isAudioSource())
	{
		if ((LLPipeline::sRenderBeacons ||
			 !LLPipeline::sRenderInvisibleSoundBeacons) && type != 2)
		{
			gObjectList.addDebugBeacon(objp->getPositionAgent(), "",
									   LLColor4(1.f, 1.f, 0.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderParticleBeacons(LLDrawable* drawablep)
{
	// Look for attachments, objects, etc.
	LLViewerObject* objp = drawablep->getVObj().get();
	U32 type = LLPipeline::highlightable(objp);
	if (type != 0 && objp->isParticleSource())
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(objp->getPositionAgent(), "",
									   LLColor4(0.5f, 0.5f, 1.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderMOAPBeacons(LLDrawable* drawablep)
{
	LLViewerObject* objp = drawablep->getVObj().get();
	if (!objp || objp->isAvatar()) return;

	U32 type = LLPipeline::highlightable(objp);
	if (type == 0) return;

	bool beacon = false;
	U8 tecount = objp->getNumTEs();
	for (S32 x = 0; x < tecount; ++x)
	{
		LLTextureEntry* tep = objp->getTE(x);
		if (tep && tep->hasMedia())
		{
			beacon = true;
			break;
		}
	}
	if (beacon)
	{
		if (LLPipeline::sRenderBeacons && type != 2)
		{
			gObjectList.addDebugBeacon(objp->getPositionAgent(), "",
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLColor4(1.f, 1.f, 1.f, 0.5f),
									   LLPipeline::DebugBeaconLineWidth);
		}

		if (LLPipeline::sRenderHighlight)
		{
			for (S32 face_id = 0, count = drawablep->getNumFaces();
				 face_id < count; ++face_id)
			{
				LLFace* facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void LLPipeline::touchTexture(LLViewerTexture* tex, F32 vsize)
{
	if (tex)
	{
		LLImageGL* gl_tex = tex->getGLImage();
		if (gl_tex && gl_tex->updateBindStats())
		{
			tex->addTextureStats(vsize);
		}
	}
}

void LLPipeline::touchTextures(LLDrawInfo* infop)
{
	for (U32 i = 0, count = infop->mTextureList.size(); i < count; ++i)
	{
		touchTexture(infop->mTextureList[i], infop->mTextureListVSize[i]);
	}

	F32 vsize = infop->mVSize;

	touchTexture(infop->mTexture, vsize);
	if (sRenderDeferred)
	{
		touchTexture(infop->mSpecularMap, vsize);
		touchTexture(infop->mNormalMap, vsize);
	}

	if (!gUsePBRShaders)
	{
		return;
	}

	LLFetchedGLTFMaterial* gltfp = infop->mGLTFMaterial.get();
	if (gltfp)
	{
		touchTexture(gltfp->mBaseColorTexture, vsize);
		touchTexture(gltfp->mNormalTexture, vsize);
		touchTexture(gltfp->mMetallicRoughnessTexture, vsize);
		touchTexture(gltfp->mEmissiveTexture, vsize);
	}
}

void LLPipeline::postSort(LLCamera& camera)
{
	LL_FAST_TIMER(FTM_STATESORT_POSTSORT);

	if (!gCubeSnapshot)
	{
		// Rebuild drawable geometry
		LLCullResult::sg_list_t& draw_groups = sCull->getDrawableGroups();
		for (U32 i = 0, count = draw_groups.size(); i < count; ++i)
		{
			LLSpatialGroup* groupp = draw_groups[i];
			if (groupp->isDead())
			{
				continue;
			}
			if (!sUseOcclusion ||
				!groupp->isOcclusionState(LLSpatialGroup::OCCLUDED))
			{
				groupp->rebuildGeom();
			}
		}

		// Rebuild groups
		sCull->assertDrawMapsEmpty();

		rebuildPriorityGroups();
	}

	// Build render map
	bool has_type_pass_alpha = hasRenderType(RENDER_TYPE_PASS_ALPHA);
	bool has_alpha_type = hasRenderType(LLDrawPool::POOL_ALPHA);
	bool is_world_camera =
		LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD;
	U32 bytes_limit = RenderAutoHideGeometryMemoryLimit * 1048576;
	bool limit_surf_area = RenderAutoHideSurfaceAreaLimit > 0.f;
	LLVector4a bounds;
	LLCullResult::sg_list_t& visible_groups = sCull->getVisibleGroups();
	for (U32 i = 0, vcount = visible_groups.size(); i < vcount; ++i)
	{
		LLSpatialGroup* groupp = visible_groups[i];
		if (groupp->isDead())
		{
			continue;
		}
		if (sUseOcclusion &&
			groupp->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			continue;
		}

		if (bytes_limit && groupp->mGeometryBytes > bytes_limit)
		{
			continue;
		}

		if (limit_surf_area &&
			groupp->mSurfaceArea >
				RenderAutoHideSurfaceAreaLimit * groupp->mObjectBoxSize)
		{
			continue;
		}

		bool needs_rebuild = !gCubeSnapshot &&
							 groupp->hasState(LLSpatialGroup::NEW_DRAWINFO) &&
							 groupp->hasState(LLSpatialGroup::GEOM_DIRTY);
		if (needs_rebuild)
		{
			// No way this group is going to be drawable without a rebuild
			groupp->rebuildGeom();
		}

		bool needs_touch;
		if (gUsePBRShaders)
		{
			needs_touch = !gCubeSnapshot && !sShadowRender;
		}
		else
		{
			// Note: sReflectionRender is true while rendering impostors in EE
			// non-deferred mode, but then we still want impostors textures to
			// be touched so that they would not vanish. HB
			needs_touch = !sShadowRender &&
						  (!sReflectionRender || sImpostorRender);
		}
		for (LLSpatialGroup::draw_map_t::iterator j = groupp->mDrawMap.begin(),
												  end = groupp->mDrawMap.end();
			 j != end; ++j)
		{
			LLSpatialGroup::drawmap_elem_t& src_vec = j->second;
			if (!hasRenderType(j->first))
			{
				continue;
			}
			for (U32 k = 0, dcount = src_vec.size(); k < dcount; ++k)
			{
				LLDrawInfo* infop = src_vec[k].get();
				sCull->pushDrawInfo(j->first, infop);
				if (needs_touch)
				{
					if (!needs_rebuild)
					{
						touchTextures(infop);
					}
					addTrianglesDrawn(infop->mCount);
				}
			}
		}

		if (has_type_pass_alpha)
		{
			if (groupp->mDrawMap.count(LLRenderPass::PASS_ALPHA))
			{
				// Store alpha groups for sorting
				if (is_world_camera && !gCubeSnapshot)
				{
					LLSpatialBridge* bridgep =
						groupp->getSpatialPartition()->asBridge();
					if (bridgep)
					{
						LLCamera trans_camera =
							bridgep->transformCamera(camera);
						groupp->updateDistance(trans_camera);
					}
					else
					{
						groupp->updateDistance(camera);
					}
				}

				if (has_alpha_type)
				{
					sCull->pushAlphaGroup(groupp);
				}
			}

			if (has_alpha_type &&
				groupp->mDrawMap.count(LLRenderPass::PASS_ALPHA_RIGGED))
			{
				// Store rigged alpha groups for LLDrawPoolAlpha prepass
				// (skip distance update, rigged attachments use depth buffer)
				sCull->pushRiggedAlphaGroup(groupp);
			}
		}
	}

	// Pack vertex buffers for groups that chose to delay their updates
	for (U32 i = 0, count = mMeshDirtyGroup.size(); i < count; ++i)
	{
		LLSpatialGroup* groupp = mMeshDirtyGroup[i].get();
		if (groupp)
		{
			groupp->rebuildMesh();
		}
	}
	mMeshDirtyGroup.clear();

	if (!sShadowRender)
	{
		// Order alpha groups by distance
		std::sort(sCull->beginAlphaGroups(), sCull->endAlphaGroups(),
				  LLSpatialGroup::CompareDepthGreater());
		// Order rigged alpha groups by avatar attachment order
		std::sort(sCull->beginRiggedAlphaGroups(),
				  sCull->endRiggedAlphaGroups(),
				  LLSpatialGroup::CompareRenderOrder());
	}

	if (gCubeSnapshot)
	{
		// Do not render beacons or highlights during cube snapshot.
		return;
	}

	// This is the position for the sounds list floater beacon:
	LLVector3d selected_pos = HBFloaterSoundsList::selectedLocation();

	// Only render if the flag is set. The flag is only set if we are in edit
	// mode or the toggle is set in the menus.
	static LLCachedControl<bool> beacons_always_on(gSavedSettings,
												   "BeaconAlwaysOn");
	if ((sRenderBeaconsFloaterOpen || beacons_always_on) &&
//MK
		!(gRLenabled &&
		  (gRLInterface.mContainsEdit || gRLInterface.mVisionRestricted)) &&
//mk
		!sShadowRender)
	{
		if (sRenderScriptedTouchBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderScriptedTouchBeacons);
		}
		else if (sRenderScriptedBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderScriptedBeacons);
		}

		if (sRenderPhysicalBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderPhysicalBeacons);
		}

		if (sRenderPermanentBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderPermanentBeacons);
		}

		if (sRenderCharacterBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderCharacterBeacons);
		}

		if (sRenderSoundBeacons && gAudiop)
		{
			if (sRenderInvisibleSoundBeacons && sRenderBeacons)
			{
				static const LLColor4 semi_yellow(1.f, 1.f, 0.f, 0.5f);
				static const LLColor4 semi_white(1.f, 1.f, 0.f, 0.5f);
				// Walk all sound sources and render out beacons for them.
				// Note, this is not done in the ForAllVisibleDrawables
				// function, because some are not visible.
				for (LLAudioEngine::source_map_t::const_iterator
						iter = gAudiop->mAllSources.begin(),
						end = gAudiop->mAllSources.end();
					 iter != end; ++iter)
				{
					const LLAudioSource* sourcep = iter->second;
					if (!sourcep) continue;	// Paranoia

					// Verify source owner and match against renderbyowner
					const LLUUID& owner_id = sourcep->getOwnerID();
					if ((sRenderByOwner == 1 && owner_id != gAgentID) ||
						(sRenderByOwner == 2 && owner_id == gAgentID))
					{
						continue;
					}

					LLVector3d pos_global = sourcep->getPositionGlobal();
					if (selected_pos.isExactlyZero() ||
						pos_global != selected_pos)
					{
						LLVector3 pos =
							gAgent.getPosAgentFromGlobal(pos_global);
						gObjectList.addDebugBeacon(pos, "", semi_yellow,
												   semi_white,
												   DebugBeaconLineWidth);
					}
				}
			}
			// Now deal with highlights for all those seeable sound sources
			forAllVisibleDrawables(renderSoundBeacons);
		}

		if (sRenderParticleBeacons)
		{
			forAllVisibleDrawables(renderParticleBeacons);
		}

		if (sRenderMOAPBeacons)
		{
			forAllVisibleDrawables(renderMOAPBeacons);
		}
	}

	// Render the sound beacon for the sounds list floater, if needed.
	if (!selected_pos.isExactlyZero())
	{
		gObjectList.addDebugBeacon(gAgent.getPosAgentFromGlobal(selected_pos),
								   "",
								   // Oranger yellow than sound normal beacons
								   LLColor4(1.f, 0.8f, 0.f, 0.5f),
								   LLColor4(1.f, 1.f, 1.f, 0.5f),
								   DebugBeaconLineWidth);
	}

	// If managing your telehub, draw beacons at telehub and currently selected
	// spawnpoint.
	if (LLFloaterTelehub::renderBeacons())
	{
		LLFloaterTelehub::addBeacons();
	}

	if (!sShadowRender)
	{
		mSelectedFaces.clear();

		sRenderHighlightTextureChannel =
			LLPanelFace::getTextureChannelToEdit();

		// Draw face highlights for selected faces.
		if (gSelectMgr.getTEMode())
		{
			struct f final : public LLSelectedTEFunctor
			{
				bool apply(LLViewerObject* object, S32 te) override
				{
					LLDrawable* drawablep = object->mDrawable;
					if (drawablep)
					{
						LLFace* facep = drawablep->getFace(te);
						if (facep)
						{
							gPipeline.mSelectedFaces.push_back(facep);
						}
					}
					return true;
				}
			} func;
			gSelectMgr.getSelection()->applyToTEs(&func);
		}
	}
}

void render_hud_elements()
{
	gPipeline.disableLights();

	LLGLSUIDefault gls_ui;

	LLGLEnable stencil(gUsePBRShaders ? 0 : GL_STENCIL_TEST);
	if (!gUsePBRShaders)
	{
		glStencilFunc(GL_ALWAYS, 255, 0xFFFFFFFF);
		glStencilMask(0xFFFFFFFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	}

	gUIProgram.bind();

	gGL.color4f(1.f, 1.f, 1.f, 1.f);

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	if (!LLPipeline::sReflectionRender &&
		gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
	{
		bool sample = !gUsePBRShaders && LLPipeline::RenderFSAASamples > 0;
		LLGLEnable multisample(sample ? GL_MULTISAMPLE : 0);
		// For HUD version in render_ui_3d()
		gViewerWindowp->renderSelections(false, false, false);

		// Draw the tracking overlays
		gTracker.render3D();

//MK
		if (!gRLenabled || !gRLInterface.mVisionRestricted)
//mk
		{
			// Show the property lines
			gWorld.renderPropertyLines();
			gViewerParcelMgr.render();
			gViewerParcelMgr.renderParcelCollision();
		}

		// Note: for PBR this is done in render_ui() (llviewerdisplay.cpp) so
		// to avoid seeing the text anti-aliased.
		if (!gUsePBRShaders)
		{
			// Render name tags and hover texts.
			LLHUDObject::renderAll();
		}
	}
	else if (gForceRenderLandFence)
	{
		// This is only set when not rendering the UI, for parcel snapshots
		gViewerParcelMgr.render();
	}
	else if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
	{
		LLHUDText::renderAllHUD();
	}

	gUIProgram.unbind();

	gGL.flush();
}

void LLPipeline::renderHighlights()
{
	S32 selected_count = mSelectedFaces.size();
	S32 highlighted_count = mHighlightFaces.size();
	if ((!selected_count && !highlighted_count) ||
		!hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED))
	{
		// Nothing to draw
		return;
	}

	LLGLSPipelineAlpha gls_pipeline_alpha;
	disableLights();

	bool shader_interface =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_INTERFACE) > 0;

	std::vector<LLFace*>::iterator sel_start, sel_end;
	if (selected_count)
	{
		sel_start = mSelectedFaces.begin();
		sel_end = mSelectedFaces.end();
	}

	if (highlighted_count)
	{
		// Beacons face highlights
		if (shader_interface)
		{
			gHighlightProgram.bind();
		}
		LLColor4 color(1.f, 0.f, 0.f, .5f);
		for (S32 i = 0; i < highlighted_count; ++i)
		{
			LLFace* facep = mHighlightFaces[i];
			if (facep && !facep->getDrawable()->isDead())
			{
				if (!selected_count ||
					// Exclude selected faces from beacon highlights
					std::find(sel_start, sel_end, facep) == sel_end)
				{
					facep->renderSelected(LLViewerTexture::sNullImagep, color);
				}
			}
			else if (gDebugGL)
			{
				llwarns << "Bad face in beacons highlights" << llendl;
			}
		}
		if (shader_interface)
		{
			gHighlightProgram.unbind();
		}
		mHighlightFaces.clear();
	}

	if (selected_count)
	{
		// Selection image initialization if needed
		if (!mFaceSelectImagep)
		{
			mFaceSelectImagep =
				LLViewerTextureManager::getFetchedTexture(IMG_FACE_SELECT);
		}
		// Make sure the selection image gets downloaded and decoded
		mFaceSelectImagep->addTextureStats((F32)MAX_IMAGE_AREA);

		// Use the color matching the channel we are editing
		LLColor4 color;
		LLRender::eTexIndex active_channel = sRenderHighlightTextureChannel;
		switch (active_channel)
		{
			case LLRender::NORMAL_MAP:
				color = LLColor4(1.f, .5f, .5f, .5f);
				break;

			case LLRender::SPECULAR_MAP:
				color = LLColor4(0.f, .3f, 1.f, .8f);
				break;

			case LLRender::DIFFUSE_MAP:
			default:
				color = LLColor4(1.f, 1.f, 1.f, .5f);
		}

		LLGLSLShader* prev_shaderp = NULL;

		for (S32 i = 0; i < selected_count; ++i)
		{
			LLFace* facep = mSelectedFaces[i];
			if (facep && !facep->getDrawable()->isDead())
			{
				LLMaterial* matp = NULL;
				if (sRenderDeferred && active_channel != LLRender::DIFFUSE_MAP)
				{
					// Fetch the material info, if any
					const LLTextureEntry* tep = facep->getTextureEntry();
					if (tep)
					{
						matp = tep->getMaterialParams().get();
					}
				}
				if (shader_interface)
				{
					// Default to diffuse map highlighting
					LLGLSLShader* new_shaderp = &gHighlightProgram;

					// Use normal or specular maps highlighting where possible
					// (i.e. material exists and got a corresponding map)
					if (active_channel == LLRender::NORMAL_MAP && matp &&
						matp->getNormalID().notNull())
					{
						new_shaderp = &gHighlightNormalProgram;
					}
					else if (active_channel == LLRender::SPECULAR_MAP &&
							 matp && matp->getSpecularID().notNull())
					{
						new_shaderp = &gHighlightSpecularProgram;
					}

					// Change the shader if not already the one in use
					if (shader_interface && new_shaderp != prev_shaderp)
					{
						if (prev_shaderp)
						{
							prev_shaderp->unbind();
						}
						new_shaderp->bind();
						prev_shaderp = new_shaderp;
					}
				}

				// Draw the selection on the face.
				facep->renderSelected(mFaceSelectImagep, color);
			}
			else if (gDebugGL)
			{
				llwarns << "Bad face in selection" << llendl;
			}
		}

		// Unbind the last shader, if any
		if (prev_shaderp)
		{
			prev_shaderp->unbind();
		}
	}
}

// Debug use
U32 LLPipeline::sCurRenderPoolType = 0;

// Only for use by the EE renderer (in forward rendering mode)
void LLPipeline::renderGeom(LLCamera& camera)
{
	LL_FAST_TIMER(FTM_RENDER_GEOMETRY);

	// HACK: preserve/restore matrices around HUD render
	LLMatrix4a saved_modelview;
	LLMatrix4a saved_projection;
	bool hud_render = hasRenderType(RENDER_TYPE_HUD);
	if (hud_render)
	{
		saved_modelview = gGLModelView;
		saved_projection = gGLProjection;
	}

	///////////////////////////////////////////
	// Sync and verify GL state

	LLVertexBuffer::unbind();

	// Do verification of GL state
	LL_GL_CHECK_STATES;
	if (mRenderDebugMask & RENDER_DEBUG_VERIFY)
	{
		if (!verify())
		{
			llerrs << "Pipeline verification failed !" << llendl;
		}
	}

	// Initialize lots of GL state to "safe" values
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	LLGLSPipeline gls_pipeline;
	LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE : 0);

	// Toggle backface culling for debugging
	LLGLEnable cull_face(mBackfaceCull ? GL_CULL_FACE : 0);
	// Set fog
	gSky.updateFog(camera.getFar());
	if (!hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_FOG))
	{
		sUnderWaterRender = false;
	}

	if (LLViewerFetchedTexture::sDefaultImagep)
	{
		unit0->bind(LLViewerFetchedTexture::sDefaultImagep);
		LLViewerFetchedTexture::sDefaultImagep->setAddressMode(LLTexUnit::TAM_WRAP);
	}

	//////////////////////////////////////////////
	// Actually render all of the geometry

	pool_set_t::iterator pools_end = mPools.end();
	for (pool_set_t::iterator iter = mPools.begin(); iter != pools_end; ++iter)
	{
		LLDrawPool* poolp = *iter;
		if (hasRenderType(poolp->getType()))
		{
			poolp->prerender();
		}
	}

	{
		LL_FAST_TIMER(FTM_POOLS);

		// *HACK: do not calculate local lights if we are rendering the HUD !
		// Removing this check will cause bad flickering when there are HUD
		// elements being rendered AND the user is in flycam mode. -nyx
		if (!hud_render)
		{
			calcNearbyLights(camera);
			setupHWLights();
		}

		bool occlude = sUseOcclusion > 1;
		U32 cur_type = 0;

		pool_set_t::iterator iter1 = mPools.begin();
		while (iter1 != pools_end)
		{
			LLDrawPool* poolp = *iter1;

			cur_type = poolp->getType();

			// Debug use
			sCurRenderPoolType = cur_type;

			if (occlude && cur_type >= LLDrawPool::POOL_GRASS)
			{
				occlude = false;
				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);
				LLGLSLShader::unbind();
				doOcclusion(camera);
			}

			pool_set_t::iterator iter2 = iter1;
			S32 passes;
			if (hasRenderType(poolp->getType()) &&
				(passes = poolp->getNumPasses()) > 0)
			{
				LL_FAST_TIMER(FTM_POOLRENDER);

				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);

				for (S32 i = 0; i < passes; ++i)
				{
					LLVertexBuffer::unbind();
					poolp->beginRenderPass(i);
					for (iter2 = iter1; iter2 != pools_end; ++iter2)
					{
						LLDrawPool* p = *iter2;
						if (p->getType() != cur_type)
						{
							break;
						}

						p->render(i);
					}
					poolp->endRenderPass(i);
					LLVertexBuffer::unbind();
					if (gDebugGL && iter2 != pools_end)
					{
						std::string msg = llformat("%s pass %d",
												   gPoolNames[cur_type].c_str(),
												   i);
						LLGLState::checkStates(msg);
					}
				}
			}
			else
			{
				// Skip all pools of this type
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
				}
			}
			iter1 = iter2;
		}

		LLVertexBuffer::unbind();

		gGLLastMatrix = NULL;
		gGL.loadMatrix(gGLModelView);

		if (occlude)
		{
			occlude = false;
			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);
			LLGLSLShader::unbind();
			doOcclusion(camera);
		}
	}

	LLVertexBuffer::unbind();
	LL_GL_CHECK_STATES;

	if (!sImpostorRender)
	{
		if (!sReflectionRender)
		{
			renderHighlights();
		}

		// Contains a list of the faces of beacon-targeted objects that are
		// also to be highlighted.
		mHighlightFaces.clear();

		renderDebug();

		LLVertexBuffer::unbind();

		if (!sReflectionRender && !sRenderDeferred)
		{
			if (hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_UI))
			{
				// Render debugging beacons.
				gObjectList.renderObjectBeacons();
				gObjectList.resetObjectBeacons();
				gSky.addSunMoonBeacons();
			}
			else
			{
				// Make sure particle effects disappear
				LLHUDObject::removeExpired();
			}
		}
		else
		{
			// Make sure particle effects disappear
			LLHUDObject::removeExpired();
		}

		// HACK: preserve/restore matrices around HUD render
		if (hud_render)
		{
			gGLModelView = saved_modelview;
			gGLProjection = saved_projection;
		}
	}

	LLVertexBuffer::unbind();

	LL_GL_CHECK_STATES;
}

// Version for use only by the PBR renderer
void LLPipeline::renderGeomDeferred(LLCamera& camera, bool do_occlusion)
{
	LL_FAST_TIMER(FTM_RENDER_GEOMETRY);

	if (gUseWireframe)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	if (&camera == &gViewerCamera && !sAvatarPreviewRender)
	{
		// A bit hacky: this is the start of the main render frame, figure out
		// delta between last modelview matrix and current modelview matrix.
		LLMatrix4a mat(gGLLastModelView);
		// The goal is to have a matrix here that goes from the last frame's
		// camera space to the current frame's camera space
		mat.invert();
		mat.setMul(gGLModelView, mat);
		gGLDeltaModelView = mat;
		mat.invert();
		gGLInverseDeltaModelView = mat;
	}

	bool occlude = do_occlusion && sUseOcclusion > 1 &&
				   !LLGLSLShader::sProfileEnabled;
	setupHWLights();

	{
		LL_FAST_TIMER(FTM_POOLS);

		LLGLEnable cull(GL_CULL_FACE);

		pool_set_t::iterator pools_end = mPools.end();

		for (pool_set_t::iterator it = mPools.begin(); it != pools_end; ++it)
		{
			LLDrawPool* poolp = *it;
			if (hasRenderType(poolp->getType()))
			{
				poolp->prerender();
			}
		}

		LLVertexBuffer::unbind();
		LL_GL_CHECK_STATES;

		if (gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_DEFERRED) > 1)
		{
			// Update reflection probe uniform
			 mReflectionMapManager.updateUniforms();
		}

		U32 cur_type = 0;

		gGL.setColorMask(true, true);

		pool_set_t::iterator iter1 = mPools.begin();

		while (iter1 != pools_end)
		{
			LLDrawPool* poolp = *iter1;

			cur_type = poolp->getType();

			if (occlude && cur_type >= LLDrawPool::POOL_GRASS)
			{
				occlude = false;
				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);
				doOcclusion(camera);
			}

			pool_set_t::iterator iter2 = iter1;
			S32 passes;
			if (hasRenderType(poolp->getType()) &&
				(passes = poolp->getNumDeferredPasses()) > 0)
			{
				LL_FAST_TIMER(FTM_POOLRENDER);

				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);

				for (S32 i = 0; i < passes; ++i)
				{
					LLVertexBuffer::unbind();
					poolp->beginDeferredPass(i);
					for (iter2 = iter1; iter2 != pools_end; ++iter2)
					{
						LLDrawPool* p = *iter2;
						if (p->getType() != cur_type)
						{
							break;
						}

						p->renderDeferred(i);
					}
					poolp->endDeferredPass(i);
					LLVertexBuffer::unbind();

					LL_GL_CHECK_STATES;
				}
			}
			else
			{
				// Skip all pools of this type
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
				}
			}
			iter1 = iter2;
		}

		gGLLastMatrix = NULL;
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.loadMatrix(gGLModelView);

		gGL.setColorMask(true, false);

		stop_glerror();
	}

	if (gUseWireframe)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}

// Version for use only by the EE renderer
void LLPipeline::renderGeomDeferred(LLCamera& camera)
{
	// *HACK: branch to the PBR version when used without 'do_occlusion'
	// parameter, so to simplify the dual renderer code. HB
	if (gUsePBRShaders)
	{
		renderGeomDeferred(camera, false);
		return;
	}

	LL_FAST_TIMER(FTM_RENDER_GEOMETRY);

	{
		LL_FAST_TIMER(FTM_POOLS);

		LLGLEnable cull(GL_CULL_FACE);

		LLGLEnable stencil(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		pool_set_t::iterator pools_end = mPools.end();

		for (pool_set_t::iterator it = mPools.begin(); it != pools_end; ++it)
		{
			LLDrawPool* poolp = *it;
			if (hasRenderType(poolp->getType()))
			{
				poolp->prerender();
			}
		}

		LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE : 0);

		LLVertexBuffer::unbind();

		LL_GL_CHECK_STATES;

		U32 cur_type = 0;

		gGL.setColorMask(true, true);

		pool_set_t::iterator iter1 = mPools.begin();

		while (iter1 != pools_end)
		{
			LLDrawPool* poolp = *iter1;

			cur_type = poolp->getType();

			pool_set_t::iterator iter2 = iter1;
			S32 passes;
			if (hasRenderType(poolp->getType()) &&
				(passes = poolp->getNumDeferredPasses()) > 0)
			{
				LL_FAST_TIMER(FTM_POOLRENDER);

				gGLLastMatrix = NULL;
				gGL.loadMatrix(gGLModelView);

				for (S32 i = 0; i < passes; ++i)
				{
					LLVertexBuffer::unbind();
					poolp->beginDeferredPass(i);
					for (iter2 = iter1; iter2 != pools_end; ++iter2)
					{
						LLDrawPool* p = *iter2;
						if (p->getType() != cur_type)
						{
							break;
						}

						p->renderDeferred(i);
					}
					poolp->endDeferredPass(i);
					LLVertexBuffer::unbind();

					LL_GL_CHECK_STATES;
				}
			}
			else
			{
				// Skip all pools of this type
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
				}
			}
			iter1 = iter2;
		}

		gGLLastMatrix = NULL;
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.loadMatrix(gGLModelView);

		gGL.setColorMask(true, false);

		stop_glerror();
	}
}

void LLPipeline::renderGeomPostDeferred(LLCamera& camera, bool do_occlusion)
{
	LL_FAST_TIMER(FTM_POOLS);

	bool occlude;
	bool sample;
	if (gUsePBRShaders)
	{
		sample = occlude = false;
		if (gUseWireframe)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
	}
	else
	{
		occlude = do_occlusion && sUseOcclusion > 1;
		sample = LLPipeline::RenderFSAASamples > 0;
	}
	LLGLEnable cull(GL_CULL_FACE);
	LLGLEnable multisample(sample ? GL_MULTISAMPLE : 0);

	// Skip PBR atmospherics/haze when in EE rendering, or when rendering HUDs,
	// or when rendering impostors, or when not rendering atmospherics.
	bool done_atmospherics = !gUsePBRShaders || sRenderingHUDs ||
							 sImpostorRender || !RenderDeferredAtmospheric;
	bool done_water_haze = done_atmospherics;
	// Do water haze just before pre water alpha or atmospheric haze just
	// before post water alpha.
	U32 atm_pass = sUnderWaterRender ? LLDrawPool::POOL_WATER
									 : LLDrawPool::POOL_ALPHA_POST_WATER;
	// Do water haze just before pre water alpha.
	constexpr U32 water_haze_pass = LLDrawPool::POOL_ALPHA_PRE_WATER;

	calcNearbyLights(camera);
	setupHWLights();

	if (gUsePBRShaders)
	{
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}
	gGL.setColorMask(true, false);

	pool_set_t::iterator iter1 = mPools.begin();
	pool_set_t::iterator pools_end = mPools.end();
	while (iter1 != pools_end)
	{
		LLDrawPool* poolp = *iter1;
		U32 cur_type = poolp->getType();

		// Possibly used only in EE rendering mode. HB
		if (occlude && cur_type >= LLDrawPool::POOL_GRASS)
		{
			occlude = false;
			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);
			LLGLSLShader::unbind();
			doOcclusion(camera, mRT->mScreen, mOcclusionDepth,
						&mDeferredDepth);
			gGL.setColorMask(true, false);
		}

		// Possibly used only in PBR rendering mode. HB
		if (!done_atmospherics && cur_type >= atm_pass)
		{
			doAtmospherics();
			done_atmospherics = true;
		}
		if (!done_water_haze && cur_type >= water_haze_pass)
		{
			doWaterHaze();
			done_water_haze = true;
		}

		pool_set_t::iterator iter2 = iter1;
		S32 passes;
		if (hasRenderType(poolp->getType()) &&
			(passes = poolp->getNumPostDeferredPasses()) > 0)
		{
			LL_FAST_TIMER(FTM_POOLRENDER);

			// In PBR rendering mode, some draw pools do not use at all the
			// deferred pass, so we must ensure prerender() has been called for
			// them ! HB
			if (gUsePBRShaders && !poolp->getNumDeferredPasses())
			{
				poolp->prerender();
			}

			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);

			for (S32 i = 0; i < passes; ++i)
			{
				LLVertexBuffer::unbind();
				poolp->beginPostDeferredPass(i);
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}

					p->renderPostDeferred(i);
				}
				poolp->endPostDeferredPass(i);
				LLVertexBuffer::unbind();

				LL_GL_CHECK_STATES;
			}
		}
		else
		{
			// Skip all pools of this type
			for (iter2 = iter1; iter2 != pools_end; ++iter2)
			{
				LLDrawPool* p = *iter2;
				if (p->getType() != cur_type)
				{
					break;
				}
			}
		}
		iter1 = iter2;
	}

	gGLLastMatrix = NULL;
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.loadMatrix(gGLModelView);

	if (gUsePBRShaders)
	{
		if (!gCubeSnapshot)
		{
			// Render highlights, etc.
			renderHighlights();
			mHighlightFaces.clear();

			renderDebug();
		}

		if (gUseWireframe)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}
	else if (occlude)
	{
		LLGLSLShader::unbind();
		doOcclusion(camera);
		gGLLastMatrix = NULL;
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.loadMatrix(gGLModelView);
	}

	stop_glerror();
}

// PBR only
void LLPipeline::doAtmospherics()
{
	LL_TRACY_TIMER(TRC_DO_ATMOSPHERICS);

	LLGLEnable blend(GL_BLEND);
	gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_SOURCE_ALPHA,
				  LLRender::BF_ZERO, LLRender::BF_SOURCE_ALPHA);

	gGL.setColorMask(true, true);

	// Apply haze
	bindDeferredShader(gHazeProgram);

	gHazeProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR, mIsSunUp ? 1 : 0);
	gHazeProgram.uniform3fv(LLShaderMgr::LIGHTNORM, 1,
							gEnvironment.getClampedLightNorm().mV);
	gHazeProgram.uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1,
							sWaterPlane.getF32ptr());

	LLGLDepthTest depth(GL_FALSE);
	// Full screen blit
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	unbindDeferredShader(gHazeProgram);

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

// PBR only
void LLPipeline::doWaterHaze()
{
	LL_TRACY_TIMER(TRC_DO_WATER_HAZE);

	LLGLEnable blend(GL_BLEND);
	gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_SOURCE_ALPHA,
				  LLRender::BF_ZERO, LLRender::BF_SOURCE_ALPHA);

	gGL.setColorMask(true, true);

	// Apply haze
	bindDeferredShader(gHazeWaterProgram);

	gHazeWaterProgram.uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1,
								 sWaterPlane.getF32ptr());
	gHazeWaterProgram.uniform1i(sAboveWater, sUnderWaterRender ? -1 : 1);
	if (sUnderWaterRender)
	{
		gHazeWaterProgram.uniform1i(sAboveWater, -1);
		LLGLDepthTest depth(GL_FALSE);
		// Full screen blit
		mScreenTriangleVB->setBuffer();
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	}
	else
	{
		gHazeWaterProgram.uniform1i(sAboveWater, 1);
		// Render water patches like LLDrawPoolWater does
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		LLGLDisable cull(GL_CULL_FACE);

		gGLLastMatrix = NULL;
		gGL.loadMatrix(gGLModelView);

		if (mWaterPool)
		{
			mWaterPool->pushFaceGeometry();
		}
	}

	unbindDeferredShader(gHazeWaterProgram);

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

void LLPipeline::renderGeomShadow(LLCamera& camera)
{
	U32 cur_type = 0;

	LLGLEnable cull(GL_CULL_FACE);

	LLVertexBuffer::unbind();

	pool_set_t::iterator iter1 = mPools.begin();
	pool_set_t::iterator pools_end = mPools.end();

	while (iter1 != pools_end)
	{
		LLDrawPool* poolp = *iter1;

		cur_type = poolp->getType();

		pool_set_t::iterator iter2 = iter1;
		S32 passes;
		if (hasRenderType(poolp->getType()) &&
			(passes = poolp->getNumShadowPasses()) > 0)
		{
			poolp->prerender();

			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);

			for (S32 i = 0; i < passes; ++i)
			{
				LLVertexBuffer::unbind();
				poolp->beginShadowPass(i);
				for (iter2 = iter1; iter2 != pools_end; ++iter2)
				{
					LLDrawPool* p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}

					p->renderShadow(i);
				}
				poolp->endShadowPass(i);
				LLVertexBuffer::unbind();

				LL_GL_CHECK_STATES;
			}
		}
		else
		{
			// Skip all pools of this type
			for (iter2 = iter1; iter2 != pools_end; ++iter2)
			{
				LLDrawPool* p = *iter2;
				if (p->getType() != cur_type)
				{
					break;
				}
			}
		}
		iter1 = iter2;
	}

	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLModelView);

	stop_glerror();
}

void LLPipeline::addTrianglesDrawn(U32 index_count)
{
	if (mNeedsDrawStats)
	{
		U32 count = index_count / 3;
		mTrianglesDrawn += count;
		if (count > mMaxBatchSize)
		{
			mMaxBatchSize = count;
		}
		if (count < mMinBatchSize)
		{
			mMinBatchSize = count;
		}
		++mBatchCount;
	}

	if (sRenderFrameTest)
	{
		gWindowp->swapBuffers();
		ms_sleep(16);
	}
}

void LLPipeline::renderPhysicsDisplay()
{
	if (!gUsePBRShaders)
	{
		allocatePhysicsBuffer();

		gGL.flush();
		mPhysicsDisplay.bindTarget();
		glClearColor(0.f, 0.f, 0.f, 1.f);
		gGL.setColorMask(true, true);
		mPhysicsDisplay.clear();
		glClearColor(0.f, 0.f, 0.f, 0.f);

		gGL.setColorMask(true, false);

		gDebugProgram.bind();

		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* partp = regionp->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				if (hasRenderType(partp->mDrawableType))
				{
					partp->renderPhysicsShapes();
				}
			}
		}

		gGL.flush();

		gDebugProgram.unbind();

		mPhysicsDisplay.flush();
		return;
	}

	gGL.flush();
	gDebugProgram.bind();

	LLGLEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(3.f, 3.f);
	glLineWidth(3.f);
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	for (U32 pass = 0; pass < 3; ++pass)
	{
		// Pass 0 - depth write enabled, color write disabled, fill
		// Pass 1 - depth write disabled, color write enabled, fill
		// Pass 2 - depth write disabled, color write enabled, wireframe
		gGL.setColorMask(pass >= 1, false);
		LLGLDepthTest depth(GL_TRUE, pass == 0);
		bool wireframe = pass == 2;

		if (wireframe)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}

		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* region = *iter;
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* partp = region->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				if (hasRenderType(partp->mDrawableType))
				{
					partp->renderPhysicsShapes(wireframe);
				}
			}
		}
		gGL.flush();

		if (wireframe)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}

	glLineWidth(1.f);
	gDebugProgram.unbind();
}

void LLPipeline::renderDebug()
{
	bool hud_only = hasRenderType(RENDER_TYPE_HUD);
	bool render_blips = !hud_only && !mDebugBlips.empty();

	// If no debug feature is on and there's no blip to render, return now
	if (mRenderDebugMask == 0 && !render_blips) return;

	gGL.color4f(1.f, 1.f, 1.f, 1.f);

	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLModelView);
	gGL.setColorMask(true, false);

	if (render_blips)
	{
		// Render debug blips
		gUIProgram.bind();

		gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep, true);

		glPointSize(8.f);
		LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);

		gGL.begin(LLRender::POINTS);
		for (std::list<DebugBlip>::iterator iter = mDebugBlips.begin(),
											end = mDebugBlips.end();
			 iter != end; )
		{
			DebugBlip& blip = *iter;

			blip.mAge += gFrameIntervalSeconds;
			if (blip.mAge > 2.f)
			{
				iter = mDebugBlips.erase(iter);
			}
			else
			{
				++iter;
			}

			blip.mPosition.mV[2] += gFrameIntervalSeconds * 2.f;

			gGL.color4fv(blip.mColor.mV);
			gGL.vertex3fv(blip.mPosition.mV);
		}
		gGL.end(true);
		glPointSize(1.f);

		gUIProgram.unbind();

		stop_glerror();
	}

	// If no debug feature is on, return now
	if (mRenderDebugMask == 0) return;

	// This is a no-op when gUsePBRShaders is true. HB
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_LEQUAL, gUsePBRShaders);

	// Debug stuff.
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (hud_only)
		{
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* partp = regionp->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				U32 type = partp->mDrawableType;
				if (type == RENDER_TYPE_HUD ||
					type == RENDER_TYPE_HUD_PARTICLES)
				{
					partp->renderDebug();
				}
			}
		}
		else
		{
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* partp = regionp->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				if (hasRenderType(partp->mDrawableType))
				{
					partp->renderDebug();
				}
			}
		}
	}

	LLCullResult::bridge_list_t& bridges = sCull->getVisibleBridge();
	for (U32 i = 0, count = bridges.size(); i < count; ++i)
	{
		LLSpatialBridge* bridgep = bridges[i];
		if (bridgep && !bridgep->isDead() &&
			hasRenderType(bridgep->mDrawableType))
		{
			gGL.pushMatrix();
			gGL.multMatrix(bridgep->mDrawable->getRenderMatrix().getF32ptr());
			bridgep->renderDebug();
			gGL.popMatrix();
		}
	}

	if (hasRenderDebugMask(RENDER_DEBUG_OCCLUSION) &&
		!gVisibleSelectedGroups.empty())
	{
		// Render visible selected group occlusion geometry
		gDebugProgram.bind();
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		gGL.diffuseColor3f(1.f, 0.f, 1.f);
		LLVector4a fudge, size;
		for (spatial_groups_set_t::iterator
				iter = gVisibleSelectedGroups.begin(),
				end = gVisibleSelectedGroups.end();
			 iter != end; ++iter)
		{
			LLSpatialGroup* group = *iter;

			fudge.splat(0.25f); // SG_OCCLUSION_FUDGE

			const LLVector4a* bounds = group->getBounds();
			size.setAdd(fudge, bounds[1]);

			drawBox(bounds[0], size);
		}
	}

	gVisibleSelectedGroups.clear();

	bool check_probes = gUsePBRShaders && !hud_only;
	if (check_probes &&
		gPipeline.hasRenderDebugMask(RENDER_DEBUG_REFLECTION_PROBES))
	{
		mReflectionMapManager.renderDebug();
	}
	static LLCachedControl<bool> render_probes(gSavedSettings,
											   "RenderReflectionProbeVolumes");
	if (check_probes && render_probes)
	{
		bindDeferredShader(gReflectionProbeDisplayProgram);
		mScreenTriangleVB->setBuffer();
		LLGLEnable blend(GL_BLEND);
		LLGLDepthTest depth(GL_FALSE);
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
		unbindDeferredShader(gReflectionProbeDisplayProgram);
	}

	gUIProgram.bind();

	if (!hud_only && gDebugRaycastParticle &&
		hasRenderDebugMask(RENDER_DEBUG_RAYCAST))
	{
		// Draw crosshairs on particle intersection
		gDebugProgram.bind();

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		LLVector3 center(gDebugRaycastParticleIntersection.getF32ptr());
		LLVector3 size(0.1f, 0.1f, 0.1f);

		LLVector3 p[6];
		p[0] = center + size.scaledVec(LLVector3(1.f, 0.f, 0.f));
		p[1] = center + size.scaledVec(LLVector3(-1.f, 0.f, 0.f));
		p[2] = center + size.scaledVec(LLVector3(0.f, 1.f, 0.f));
		p[3] = center + size.scaledVec(LLVector3(0.f, -1.f, 0.f));
		p[4] = center + size.scaledVec(LLVector3(0.f, 0.f, 1.f));
		p[5] = center + size.scaledVec(LLVector3(0.f, 0.f, -1.f));

		gGL.begin(LLRender::LINES);
		gGL.diffuseColor3f(1.f, 1.f, 0.f);
		for (U32 i = 0; i < 6; ++i)
		{
			gGL.vertex3fv(p[i].mV);
		}
		gGL.end(true);

		gDebugProgram.unbind();
		stop_glerror();
	}

	if (!hud_only && hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
	{
		LLVertexBuffer::unbind();

		LLGLEnable blend(GL_BLEND);
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		LLGLDisable cull(GL_CULL_FACE);

		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		static const F32 colors[] =
		{
			1.f, 0.f, 0.f, 0.1f,
			0.f, 1.f, 0.f, 0.1f,
			0.f, 0.f, 1.f, 0.1f,
			1.f, 0.f, 1.f, 0.1f,

			1.f, 1.f, 0.f, 0.1f,
			0.f, 1.f, 1.f, 0.1f,
			1.f, 1.f, 1.f, 0.1f,
			1.f, 0.f, 1.f, 0.1f,
		};

		for (U32 i = 0; i < 8; ++i)
		{
			LLVector3* frust = mShadowCamera[i].mAgentFrustum;

			if (i > 3)
			{
				// Render shadow frusta as volumes
				if (mShadowFrustPoints[i - 4].empty())
				{
					continue;
				}

				gGL.color4fv(colors + (i - 4) * 4);

				gGL.begin(LLRender::TRIANGLE_STRIP);
				{
					gGL.vertex3fv(frust[0].mV);
					gGL.vertex3fv(frust[4].mV);

					gGL.vertex3fv(frust[1].mV);
					gGL.vertex3fv(frust[5].mV);

					gGL.vertex3fv(frust[2].mV);
					gGL.vertex3fv(frust[6].mV);

					gGL.vertex3fv(frust[3].mV);
					gGL.vertex3fv(frust[7].mV);

					gGL.vertex3fv(frust[0].mV);
					gGL.vertex3fv(frust[4].mV);
				}
				gGL.end();

				gGL.begin(LLRender::TRIANGLE_STRIP);
				{
					gGL.vertex3fv(frust[0].mV);
					gGL.vertex3fv(frust[1].mV);
					gGL.vertex3fv(frust[3].mV);
					gGL.vertex3fv(frust[2].mV);
				}
				gGL.end();

				gGL.begin(LLRender::TRIANGLE_STRIP);
				{
					gGL.vertex3fv(frust[4].mV);
					gGL.vertex3fv(frust[5].mV);
					gGL.vertex3fv(frust[7].mV);
					gGL.vertex3fv(frust[6].mV);
				}
				gGL.end();
			}

			if (i < 4)
			{
#if 0
				if (i == 0 || !mShadowFrustPoints[i].empty())
#endif
				{
					// Render visible point cloud
					gGL.flush();
					glPointSize(8.f);
					gGL.begin(LLRender::POINTS);

					gGL.color3fv(colors + i * 4);

					for (U32 j = 0, size = mShadowFrustPoints[i].size();
						 j < size; ++j)
					{
						gGL.vertex3fv(mShadowFrustPoints[i][j].mV);
					}
					gGL.end(true);

					glPointSize(1.f);

					LLVector3* ext = mShadowExtents[i];
					LLVector3 pos = (ext[0] + ext[1]) * 0.5f;
					LLVector3 size = (ext[1] - ext[0]) * 0.5f;
					drawBoxOutline(pos, size);

					// Render camera frustum splits as outlines
					gGL.begin(LLRender::LINES);
					{
						gGL.vertex3fv(frust[0].mV);
						gGL.vertex3fv(frust[1].mV);

						gGL.vertex3fv(frust[1].mV);
						gGL.vertex3fv(frust[2].mV);

						gGL.vertex3fv(frust[2].mV);
						gGL.vertex3fv(frust[3].mV);

						gGL.vertex3fv(frust[3].mV);
						gGL.vertex3fv(frust[0].mV);

						gGL.vertex3fv(frust[4].mV);
						gGL.vertex3fv(frust[5].mV);

						gGL.vertex3fv(frust[5].mV);
						gGL.vertex3fv(frust[6].mV);

						gGL.vertex3fv(frust[6].mV);
						gGL.vertex3fv(frust[7].mV);

						gGL.vertex3fv(frust[7].mV);
						gGL.vertex3fv(frust[4].mV);

						gGL.vertex3fv(frust[0].mV);
						gGL.vertex3fv(frust[4].mV);

						gGL.vertex3fv(frust[1].mV);
						gGL.vertex3fv(frust[5].mV);

						gGL.vertex3fv(frust[2].mV);
						gGL.vertex3fv(frust[6].mV);

						gGL.vertex3fv(frust[3].mV);
						gGL.vertex3fv(frust[7].mV);
					}
					gGL.end();
				}
			}
			gGL.flush();
		}
		stop_glerror();
	}

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp && (mRenderDebugMask & RENDER_DEBUG_WIND_VECTORS))
	{
		regionp->mWind.renderVectors();
	}

	if (regionp && (mRenderDebugMask & RENDER_DEBUG_COMPOSITION))
	{
		// Debug composition layers
		F32 x, y;

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		gGL.begin(LLRender::POINTS);
		// Draw the composition layer for the region that I am in.
		for (x = 0; x <= 260; ++x)
		{
			for (y = 0; y <= 260; ++y)
			{
				if (x > 255 || y > 255)
				{
					gGL.color4f(1.f, 0.f, 0.f, 1.f);
				}
				else
				{
					gGL.color4f(0.f, 0.f, 1.f, 1.f);
				}
				F32 z = regionp->getCompositionXY((S32)x, (S32)y);
				z *= 5.f;
				z += 50.f;
				gGL.vertex3f(x, y, z);
			}
		}
		gGL.end();
		stop_glerror();
	}

	gGL.flush();

	gUIProgram.unbind();
}

void LLPipeline::rebuildPools()
{
	S32 max_count = mPools.size();
	pool_set_t::iterator iter1 = mPools.upper_bound(mLastRebuildPool);
	while (max_count > 0 && mPools.size() > 0) // && num_rebuilds < MAX_REBUILDS)
	{
		if (iter1 == mPools.end())
		{
			iter1 = mPools.begin();
		}
		LLDrawPool* poolp = *iter1;

		if (poolp->isDead())
		{
			mPools.erase(iter1++);
			removeFromQuickLookup(poolp);
			if (poolp == mLastRebuildPool)
			{
				mLastRebuildPool = NULL;
			}
			delete poolp;
		}
		else
		{
			mLastRebuildPool = poolp;
			++iter1;
		}
		--max_count;
	}
}

void LLPipeline::addToQuickLookup(LLDrawPool* new_poolp)
{
	switch (new_poolp->getType())
	{
		case LLDrawPool::POOL_SIMPLE:
			if (mSimplePool)
			{
				llwarns << "Ignoring duplicate simple pool." << llendl;
				llassert(false);
			}
			else
			{
				mSimplePool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_ALPHA_MASK:
			if (mAlphaMaskPool)
			{
				llwarns << "Ignoring duplicate alpha mask pool." << llendl;
				llassert(false);
			}
			else
			{
				mAlphaMaskPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
			if (mFullbrightAlphaMaskPool)
			{
				llwarns << "Ignoring duplicate alpha mask pool." << llendl;
				llassert(false);
			}
			else
			{
				mFullbrightAlphaMaskPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_GRASS:
			if (mGrassPool)
			{
				llwarns << "Ignoring duplicate grass pool." << llendl;
				llassert(false);
			}
			else
			{
				mGrassPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_FULLBRIGHT:
			if (mFullbrightPool)
			{
				llwarns << "Ignoring duplicate simple pool." << llendl;
				llassert(false);
			}
			else
			{
				mFullbrightPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_INVISIBLE:
			if (mInvisiblePool)
			{
				llwarns << "Ignoring duplicate simple pool." << llendl;
				llassert(false);
			}
			else
			{
				mInvisiblePool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_GLOW:
			if (mGlowPool)
			{
				llwarns << "Ignoring duplicate glow pool." << llendl;
				llassert(false);
			}
			else
			{
				mGlowPool = (LLRenderPass*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_TREE:
			mTreePools.emplace(uintptr_t(new_poolp->getTexture()), new_poolp);
			break;

		case LLDrawPool::POOL_TERRAIN:
			mTerrainPools.emplace(uintptr_t(new_poolp->getTexture()),
								  new_poolp);
			break;

		case LLDrawPool::POOL_BUMP:
			if (mBumpPool)
			{
				llwarns << "Ignoring duplicate bump pool." << llendl;
				llassert(false);
			}
			else
			{
				mBumpPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_MATERIALS:
			if (mMaterialsPool)
			{
				llwarns << "Ignoring duplicate materials pool." << llendl;
				llassert(false);
			}
			else
			{
				mMaterialsPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_ALPHA_PRE_WATER:
			if (mAlphaPoolPreWater)
			{
				llwarns << "Ignoring duplicate pre-water alpha pool" << llendl;
				llassert(false);
			}
			else
			{
				mAlphaPoolPreWater = (LLDrawPoolAlpha*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_ALPHA_POST_WATER:
			if (mAlphaPoolPostWater)
			{
				llwarns << "Ignoring duplicate post-water alpha pool" << llendl;
				llassert(false);
			}
			else
			{
				mAlphaPoolPostWater = (LLDrawPoolAlpha*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_ALPHA:
			if (mAlphaPool)
			{
				llwarns << "Ignoring duplicate alpha pool" << llendl;
				llassert(false);
			}
			else
			{
				mAlphaPool = (LLDrawPoolAlpha*)new_poolp;
			}
			break;

		case LLDrawPool::POOL_AVATAR:
		case LLDrawPool::POOL_PUPPET:
			break; // Do nothing

		case LLDrawPool::POOL_SKY:
			if (mSkyPool)
			{
				llwarns << "Ignoring duplicate sky pool" << llendl;
				llassert(false);
			}
			else
			{
				mSkyPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_WATER:
			if (mWaterPool)
			{
				llwarns << "Ignoring duplicate water pool" << llendl;
				llassert(false);
			}
			else
			{
				mWaterPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_WL_SKY:
			if (mWLSkyPool)
			{
				llwarns << "Ignoring duplicate Windlight sky pool" << llendl;
				llassert(false);
			}
			else
			{
				mWLSkyPool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_MAT_PBR:
			if (mPBROpaquePool)
			{
				llwarns << "Ignoring duplicate PBR opaque pool" << llendl;
				llassert(false);
			}
			else
			{
				mPBROpaquePool = new_poolp;
			}
			break;

		case LLDrawPool::POOL_MAT_PBR_ALPHA_MASK:
			if (mPBRAlphaMaskPool)
			{
				llwarns << "Ignoring duplicate PBR alpha mask pool" << llendl;
				llassert(false);
			}
			else
			{
				mPBRAlphaMaskPool = new_poolp;
			}
			break;

		default:
			llerrs << "Invalid pool type: " << new_poolp->getType() << llendl;
	}
}

void LLPipeline::removePool(LLDrawPool* poolp)
{
	removeFromQuickLookup(poolp);
	mPools.erase(poolp);
	delete poolp;
}

void LLPipeline::removeFromQuickLookup(LLDrawPool* poolp)
{
	switch (poolp->getType())
	{
		case LLDrawPool::POOL_SIMPLE:
			llassert(mSimplePool == poolp);
			mSimplePool = NULL;
			break;

		case LLDrawPool::POOL_ALPHA_MASK:
			llassert(mAlphaMaskPool == poolp);
			mAlphaMaskPool = NULL;
			break;

		case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
			llassert(mFullbrightAlphaMaskPool == poolp);
			mFullbrightAlphaMaskPool = NULL;
			break;

		case LLDrawPool::POOL_GRASS:
			llassert(mGrassPool == poolp);
			mGrassPool = NULL;
			break;

		case LLDrawPool::POOL_FULLBRIGHT:
			llassert(mFullbrightPool == poolp);
			mFullbrightPool = NULL;
			break;

		case LLDrawPool::POOL_INVISIBLE:
			llassert(mInvisiblePool == poolp);
			mInvisiblePool = NULL;
			break;

		case LLDrawPool::POOL_WL_SKY:
			llassert(mWLSkyPool == poolp);
			mWLSkyPool = NULL;
			break;

		case LLDrawPool::POOL_GLOW:
			llassert(mGlowPool == poolp);
			mGlowPool = NULL;
			break;

		case LLDrawPool::POOL_TREE:
		{
#if LL_DEBUG
			bool found = mTreePools.erase((uintptr_t)poolp->getTexture());
			llassert(found);
#else
			mTreePools.erase((uintptr_t)poolp->getTexture());
#endif
			break;
		}

		case LLDrawPool::POOL_TERRAIN:
		{
#if LL_DEBUG
			bool found = mTerrainPools.erase((uintptr_t)poolp->getTexture());
			llassert(found);
#else
			mTerrainPools.erase((uintptr_t)poolp->getTexture());
#endif
			break;
		}

		case LLDrawPool::POOL_BUMP:
			llassert(poolp == mBumpPool);
			mBumpPool = NULL;
			break;

		case LLDrawPool::POOL_MATERIALS:
			llassert(poolp == mMaterialsPool);
			mMaterialsPool = NULL;
			break;

		case LLDrawPool::POOL_ALPHA_PRE_WATER:
			llassert(poolp == mAlphaPoolPreWater);
			mAlphaPoolPreWater = NULL;
			break;

		case LLDrawPool::POOL_ALPHA_POST_WATER:
			llassert(poolp == mAlphaPoolPostWater);
			mAlphaPoolPostWater = NULL;
			break;

		case LLDrawPool::POOL_ALPHA:
			llassert(poolp == mAlphaPool);
			mAlphaPool = NULL;
			break;

		case LLDrawPool::POOL_AVATAR:
		case LLDrawPool::POOL_PUPPET:
			break; // Do nothing

		case LLDrawPool::POOL_SKY:
			llassert(poolp == mSkyPool);
			mSkyPool = NULL;
			break;

		case LLDrawPool::POOL_WATER:
			llassert(poolp == mWaterPool);
			mWaterPool = NULL;
			break;

		case LLDrawPool::POOL_MAT_PBR:
			llassert(poolp == mPBROpaquePool);
			mPBROpaquePool = NULL;
			break;

		case LLDrawPool::POOL_MAT_PBR_ALPHA_MASK:
			llassert(poolp == mPBRAlphaMaskPool);
			mPBRAlphaMaskPool = NULL;
			break;

		default:
			llerrs << "Invalid pool type: " << poolp->getType() << llendl;
			break;
	}
}

void LLPipeline::resetDrawOrders()
{
	// Iterate through all of the draw pools and rebuild them.
	for (pool_set_t::iterator iter = mPools.begin(), end = mPools.end();
		 iter != end; ++iter)
	{
		LLDrawPool* poolp = *iter;
		poolp->resetDrawOrders();
	}
}

//============================================================================
// Once-per-frame setup of hardware lights, including sun/moon, avatar
// backlight, and up to 6 local lights

void LLPipeline::setupAvatarLights(bool for_edit)
{
	LLLightState* lightp = gGL.getLight(1);
	if (for_edit)
	{
		static const LLColor4 white_transparent(1.f, 1.f, 1.f, 0.f);
		mHWLightColors[1] = white_transparent;

		LLMatrix4 camera_mat = gViewerCamera.getModelview();
		LLMatrix4 camera_rot(camera_mat.getMat3());
		camera_rot.invert();

		// w = 0 => directional light
		static const LLVector4 light_pos_cam(-8.f, 0.25f, 10.f, 0.f);
		LLVector4 light_pos = light_pos_cam * camera_rot;
		light_pos.normalize();

		lightp->setDiffuse(white_transparent);
		lightp->setAmbient(LLColor4::black);
		lightp->setSpecular(LLColor4::black);
		lightp->setPosition(light_pos);
		lightp->setConstantAttenuation(1.f);
		lightp->setLinearAttenuation(0.f);
		lightp->setQuadraticAttenuation(0.f);
		lightp->setSpotExponent(0.f);
		lightp->setSpotCutoff(180.f);
	}
	else
	{
		mHWLightColors[1] = LLColor4::black;

		lightp->setDiffuse(LLColor4::black);
		lightp->setAmbient(LLColor4::black);
		lightp->setSpecular(LLColor4::black);
	}
}

static F32 calc_light_dist(LLVOVolume* lightvolp, const LLVector3& cam_pos,
						   F32 max_dist)
{
	if (lightvolp->getLightIntensity() < .001f)
	{
		return max_dist;
	}
	if (lightvolp->isSelected())
	{
		return 0.f; // Selected lights get highest priority
	}
	F32 radius = lightvolp->getLightRadius();
	F32 dist = dist_vec(lightvolp->getRenderPosition(), cam_pos) - radius;
	if (lightvolp->mDrawable.notNull() &&
		lightvolp->mDrawable->isState(LLDrawable::ACTIVE))
	{
		// Moving lights get a little higher priority (too much causes
		// artifacts)
		dist -= radius * 0.25f;
	}
	return llclamp(dist, 0.f, max_dist);
}

void LLPipeline::calcNearbyLights(LLCamera& camera)
{
	if (sReflectionRender || sRenderingHUDs || sAvatarPreviewRender || gCubeSnapshot)
	{
		return;
	}

	if (RenderLocalLightCount)
	{
		// mNearbyLight (and all light_set_t's) are sorted such that
		// begin() == the closest light and rbegin() == the farthest light
		constexpr S32 MAX_LOCAL_LIGHTS = 6;
 		LLVector3 cam_pos = camera.getOrigin();

		F32 max_dist;
		if (sRenderDeferred)
		{
			max_dist = RenderFarClip;
		}
		else
		{
			max_dist = llmax(RenderFarClip, LIGHT_MAX_RADIUS * 4.f);
		}

		// UPDATE THE EXISTING NEARBY LIGHTS
		light_set_t cur_nearby_lights;
		for (light_set_t::iterator iter = mNearbyLights.begin(),
								   end = mNearbyLights.end();
			 iter != end; ++iter)
		{
			const Light* lightp = &(*iter);
			if (!lightp) continue;		// Paranoia

			LLDrawable* drawablep = lightp->drawable;
			if (!drawablep) continue;	// Paranoia

			LLVOVolume* lightvolp = drawablep->getVOVolume();
			if (!lightvolp || !drawablep->isState(LLDrawable::LIGHT))
			{
				drawablep->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			if (lightp->fade <= -LIGHT_FADE_TIME)
			{
				drawablep->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			if (!sRenderAttachedLights && lightvolp->isAttachment())
			{
				drawablep->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			LLVOAvatar* avp = lightvolp->getAvatar();
			if (avp && avp->isVisuallyMuted())
			{
				drawablep->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}

			F32 dist = calc_light_dist(lightvolp, cam_pos, max_dist);
			F32 fade = lightp->fade;
			// Note: actual fade gets decreased/increased by setupHWLights()
			// and lightp->fade value is 'time' (positive for light to become
			// visible as value increases, negative for fading out).
			if (dist < max_dist)
			{
				if (fade < 0.f)
				{
					// Mark light to fade in: if fade was -LIGHT_FADE_TIME,
					// it was fully invisible, if negative it was fully visible
					// and visibility goes up from 0 to LIGHT_FADE_TIME.
					fade += LIGHT_FADE_TIME;
				}
			}
			// Mark light to fade out. Visibility goes down from -0 to
			// -LIGHT_FADE_TIME.
			else if (fade >= LIGHT_FADE_TIME)
			{
				fade = -0.0001f; // Was fully visible
			}
			else if (fade >= 0.f)
			{
				// 0.75 visible light should stay 0.75 visible, but should
				// reverse direction.
				fade -= LIGHT_FADE_TIME;
			}
			cur_nearby_lights.emplace(drawablep, dist, fade);
		}
		mNearbyLights.swap(cur_nearby_lights);

		// FIND NEW LIGHTS THAT ARE IN RANGE
		light_set_t new_nearby_lights;
		for (LLDrawable::draw_set_t::iterator iter = mLights.begin(),
											  end = mLights.end();
			 iter != end; ++iter)
		{
			LLDrawable* drawablep = *iter;
			LLVOVolume* lightvolp = drawablep->getVOVolume();
			if (!lightvolp || drawablep->isState(LLDrawable::NEARBY_LIGHT) ||
				lightvolp->isHUDAttachment())
			{
				continue;
			}
			if (!sRenderAttachedLights && lightvolp->isAttachment())
			{
				continue;
			}
			LLVOAvatar* avp = lightvolp->getAvatar();
			if (avp && avp->isVisuallyMuted())
			{
				drawablep->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			F32 dist = calc_light_dist(lightvolp, cam_pos, max_dist);
			if (dist >= max_dist)
			{
				continue;
			}
			new_nearby_lights.emplace(drawablep, dist, 0.f);
			if (!sRenderDeferred &&
				new_nearby_lights.size() > (U32)MAX_LOCAL_LIGHTS)
			{
				new_nearby_lights.erase(--new_nearby_lights.end());
				const Light& last = *new_nearby_lights.rbegin();
				max_dist = last.dist;
			}
		}

		// INSERT ANY NEW LIGHTS
		for (light_set_t::iterator iter = new_nearby_lights.begin(),
								   end = new_nearby_lights.end();
			 iter != end; ++iter)
		{
			const Light* lightp = &(*iter);
			if (sRenderDeferred ||
				mNearbyLights.size() < (U32)MAX_LOCAL_LIGHTS)
			{
				mNearbyLights.emplace(*lightp);
				((LLDrawable*)lightp->drawable)->setState(LLDrawable::NEARBY_LIGHT);
				continue;
			}

			// Crazy cast so that we can overwrite the fade value even though
			// gcc enforces sets as const (fade value does not affect sort so
			// this is safe)
			Light* farthestp = const_cast<Light*>(&(*(mNearbyLights.rbegin())));
			if (lightp->dist >= farthestp->dist)
			{
				break; // None of the other lights are closer
			}
			// This is a mess, but for now it needs to be in sync with fade
			// code above. Ex: code above detects distance < max, sets fade
			// time to positive, this code then detects closer lights and sets
			// fade time negative, fully compensating for the code above.
			if (farthestp->fade >= LIGHT_FADE_TIME)
			{
				farthestp->fade = -0.0001f; // Was fully visible
			}
			else if (farthestp->fade >= 0.f)
			{
				farthestp->fade -= LIGHT_FADE_TIME;
			}
		}

		// Mark nearby lights not-removable.
		for (light_set_t::iterator iter = mNearbyLights.begin(),
								   end = mNearbyLights.end();
			 iter != end; ++iter)
		{
			const Light* lightp = &(*iter);
			((LLViewerOctreeEntryData*)lightp->drawable)->setVisible();
		}
	}
}

// This method allows to read the sky values only once per frame and caches
// them for use by the various pipeline methods called during that frame. HB
void LLPipeline::cacheEnvironment()
{
	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (!skyp) return;	// Paranoia

	// Ambient
	static LLCachedControl<bool> adjust(gSavedSettings,
										"RenderSkyAutoAdjustLegacy");
	static LLCachedControl<F32> adj_scale(gSavedSettings,
										  "RenderSkyAutoAdjustSunColorScale");

	mTotalAmbient = skyp->getTotalAmbient();
	if (gUsePBRShaders)
	{
		// Cache the sky settings values for the reflection probes ambiance and
		// the gamma. HB
		mProbeAmbiance = skyp->getReflectionProbeAmbiance(adjust);
		mSkyGamma = skyp->getGamma();
	}

	mIsSunUp = skyp->getIsSunUp();
	mIsMoonUp = skyp->getIsMoonUp();
	// Prevent underlighting from having neither lightsource facing us
	if (!mIsSunUp && !mIsMoonUp)
	{
		mSunDir.set(0.f, 1.f, 0.f, 0.f);
		mMoonDir.set(0.f, 1.f, 0.f, 0.f);
		mSunDiffuse.setToBlack();
		mMoonDiffuse.setToBlack();
	}
	else
	{
		mSunDir.set(skyp->getSunDirection(), 0.f);
		mMoonDir.set(skyp->getMoonDirection(), 0.f);
		mSunDiffuse.set(skyp->getSunlightColor());
		mMoonDiffuse.set(skyp->getMoonlightColor());
		if (gUsePBRShaders && adjust && skyp->canAutoAdjust())
		{
			mSunDiffuse *= adj_scale;
		}
	}

	// Sun or Moon (All objects)
	F32 max_color = llmax(mSunDiffuse.mV[0], mSunDiffuse.mV[1],
						  mSunDiffuse.mV[2]);
	if (max_color > 1.f)
	{
		mSunDiffuse *= 1.f / max_color;
	}
	mSunDiffuse.clamp();

	max_color = llmax(mMoonDiffuse.mV[0], mMoonDiffuse.mV[1],
					  mMoonDiffuse.mV[2]);
	if (max_color > 1.f)
	{
		mMoonDiffuse *= 1.f / max_color;
	}
	mMoonDiffuse.clamp();

	// Also cache this for use by render pipeline and draw pools.
	LLViewerRegion* regionp = gAgent.getRegion();
	mWaterHeight = regionp ? regionp->getWaterHeight() : 0.f;
	mEyeAboveWater = gViewerCamera.getOrigin().mV[VZ] - mWaterHeight;
}

void LLPipeline::setupHWLights()
{
	if (sRenderingHUDs || sAvatarPreviewRender)
	{
		return;
	}

	gGL.setAmbientLightColor(mTotalAmbient);

	// Darken local lights when probe ambiance is above 1
	F32 light_scale = gCubeSnapshot ? mReflectionMapManager.mLightScale : 1.f;

	LLLightState* lightp = gGL.getLight(0);
	lightp->setPosition(mIsSunUp ? mSunDir : mMoonDir);
	LLColor4 light_diffuse = mIsSunUp ? mSunDiffuse : mMoonDiffuse;
	mHWLightColors[0] = light_diffuse;
	lightp->setDiffuse(light_diffuse);
	lightp->setSunPrimary(mIsSunUp);
	lightp->setDiffuseB(mMoonDiffuse);
	lightp->setAmbient(mTotalAmbient);
	lightp->setSpecular(LLColor4::black);
	lightp->setConstantAttenuation(1.f);
	lightp->setLinearAttenuation(0.f);
	lightp->setQuadraticAttenuation(0.f);
	lightp->setSpotExponent(0.f);
	lightp->setSpotCutoff(180.f);

	// Nearby lights = LIGHT 2-7
	S32 cur_light = 2;

	if (RenderLocalLightCount)
	{
		for (light_set_t::iterator iter = mNearbyLights.begin(),
								   end = mNearbyLights.end();
			 iter != end; ++iter)
		{
			LLDrawable* drawablep = iter->drawable;
			LLVOVolume* volp = drawablep->getVOVolume();
			if (!volp)
			{
				continue;
			}

			bool is_attachment = volp->isAttachment();
			if (is_attachment && !sRenderAttachedLights)
			{
				continue;
			}

			const LLViewerObject* objp = drawablep->getVObj();
			if (objp)
			{
				LLVOAvatar* avp = is_attachment ? objp->getAvatar()
												: NULL;
				if (avp && !avp->isSelf() &&
					(avp->isInMuteList() || avp->isTooComplex()))
				{
					continue;
				}
			}

			// Send linear light color to shader
			LLColor4 light_color = volp->getLightLinearColor() * light_scale;
			light_color.mV[3] = 0.f;

			F32 fade = iter->fade;
			if (fade < LIGHT_FADE_TIME)
			{
				constexpr F32 LIGHT_FADE_TIME_INV = 1.f / LIGHT_FADE_TIME;
				// Fade in/out light
				if (fade >= 0.f)
				{
					fade *= LIGHT_FADE_TIME_INV;
					((Light*)(&(*iter)))->fade += gFrameIntervalSeconds;
				}
				else
				{
					fade = 1.f + fade * LIGHT_FADE_TIME_INV;
					((Light*)(&(*iter)))->fade -= gFrameIntervalSeconds;
				}
				fade = llclamp(fade, 0.f, 1.f);
				light_color *= fade;
			}

			if (light_color.lengthSquared() < 0.001f)
			{
				continue;
			}

			F32 adjusted_radius = volp->getLightRadius();
			if (sRenderDeferred)
			{
				 adjusted_radius *= 1.5f;
			}
			if (adjusted_radius <= 0.001f)
			{
				continue;
			}

			LLVector4 light_pos_gl(volp->getRenderPosition(), 1.f);

			// Why this magic ?  Probably trying to match a historic behavior:
			F32 x = 3.f * (1.f + volp->getLightFalloff(2.f));
			F32 linatten = x / adjusted_radius;

			mHWLightColors[cur_light] = light_color;
			lightp = gGL.getLight(cur_light);

			lightp->setPosition(light_pos_gl);
			lightp->setDiffuse(light_color);
			lightp->setAmbient(LLColor4::black);
			lightp->setConstantAttenuation(0.f);
			lightp->setLinearAttenuation(linatten);
			lightp->setSize(volp->getLightRadius() * 1.5f);
			F32 fall_off = volp->getLightFalloff(0.5f);
			lightp->setFalloff(fall_off);
			if (sRenderDeferred)
			{
				// Get falloff to match for forward deferred rendering lights
				lightp->setQuadraticAttenuation(1.f + fall_off);
			}
			else
			{
				lightp->setQuadraticAttenuation(0);
			}

			if (volp->isLightSpotlight() &&	// Directional (spot-)light
				// These are only rendered as GL spotlights if we are in
				// deferred rendering mode *or* the setting forces them on:
				(sRenderDeferred || RenderSpotLightsInNondeferred))
			{
				LLQuaternion quat = volp->getRenderRotation();
				// This matches deferred rendering's object light direction:
				LLVector3 at_axis(0.f, 0.f, -1.f);
				at_axis *= quat;

				lightp->setSpotDirection(at_axis);
				lightp->setSpotCutoff(90.f);
				lightp->setSpotExponent(2.f);
				LLVector3 spot_params = volp->getSpotLightParams();
				const LLColor4 specular(0.f, 0.f, 0.f, spot_params[2]);
				lightp->setSpecular(specular);
			}
			else // Omnidirectional (point) light
			{
				lightp->setSpotExponent(0.f);
				lightp->setSpotCutoff(180.f);
				// We use z = 1.f as a cheap hack for the shaders to know that
				// this is omnidirectional rather than a spotlight
				lightp->setSpecular(LLColor4(0.f, 0.f, 1.f, 0.f));
			}
			if (++cur_light >= 8)
			{
				break; // safety
			}
		}
	}
	for ( ; cur_light < 8; ++cur_light)
	{
		mHWLightColors[cur_light] = LLColor4::black;
		lightp = gGL.getLight(cur_light);
		lightp->setSunPrimary(true);
		lightp->setDiffuse(LLColor4::black);
		lightp->setAmbient(LLColor4::black);
		lightp->setSpecular(LLColor4::black);
	}

	static LLCachedControl<bool> customize_lighting(gSavedSettings,
													"AvatarCustomizeLighting");
	if (customize_lighting && isAgentAvatarValid() &&
		gAgentAvatarp->mSpecialRenderMode == 3)
	{
		LLVector3 light_pos(gViewerCamera.getOrigin());
		LLVector4 light_pos_gl(light_pos, 1.f);

		F32 light_radius = 16.f;
		F32 x = 3.f;
		F32 linatten = x / light_radius; // % of brightness at radius

		lightp = gGL.getLight(2);

		LLColor4 light_color;
		light_color = LLColor4::white;
		lightp->setDiffuseB(light_color * 0.25f);
		mHWLightColors[2] = light_color;
		lightp->setPosition(light_pos_gl);
		lightp->setDiffuse(light_color);
		lightp->setDiffuseB(light_color * 0.25f);
		lightp->setAmbient(LLColor4::black);
		lightp->setSpecular(LLColor4::black);
		lightp->setQuadraticAttenuation(0.f);
		lightp->setConstantAttenuation(0.f);
		lightp->setLinearAttenuation(linatten);
		lightp->setSpotExponent(0.f);
		lightp->setSpotCutoff(180.f);
	}

	for (S32 i = 0; i < 8; ++i)
	{
		gGL.getLight(i)->disable();
	}
	mLightMask = 0;
}

void LLPipeline::enableLights(U32 mask)
{
	if (!RenderLocalLightCount)
	{
		mask &= 0xf003; // Sun and backlight only (and fullbright bit)
	}
	if (mLightMask != mask)
	{
		if (mask)
		{
			for (S32 i = 0; i < 8; ++i)
			{
				LLLightState* lightp = gGL.getLight(i);
				if (mask & (1 << i))
				{
					lightp->enable();
					lightp->setDiffuse(mHWLightColors[i]);
				}
				else
				{
					lightp->disable();
					lightp->setDiffuse(LLColor4::black);
				}
			}
		}
		mLightMask = mask;
	}
}

void LLPipeline::enableLightsStatic()
{
	constexpr U32 mask = 0xff & ~2;
	enableLights(mask);
}

void LLPipeline::enableLightsDynamic()
{
	U32 mask = 0xff & (~2); // Local lights
	enableLights(mask);

	if (isAgentAvatarValid() && RenderLocalLightCount)
	{
		if (gAgentAvatarp->mSpecialRenderMode == 0)			// Normal
		{
			enableLightsAvatar();
		}
		else if (gAgentAvatarp->mSpecialRenderMode >= 1)	// Anim preview
		{
			enableLightsAvatarEdit();
		}
	}
}

void LLPipeline::enableLightsAvatar()
{
	setupAvatarLights(false);
	enableLights(0xff);		// All lights
}

void LLPipeline::enableLightsPreview()
{
	disableLights();

	gGL.setAmbientLightColor(PreviewAmbientColor);

	LLLightState* lightp = gGL.getLight(1);

	lightp->enable();
	lightp->setPosition(LLVector4(PreviewDirection0, 0.f));
	lightp->setDiffuse(PreviewDiffuse0);
	lightp->setAmbient(PreviewAmbientColor);
	lightp->setSpecular(PreviewSpecular0);
	lightp->setSpotExponent(0.f);
	lightp->setSpotCutoff(180.f);

	lightp = gGL.getLight(2);
	lightp->enable();
	lightp->setPosition(LLVector4(PreviewDirection1, 0.f));
	lightp->setDiffuse(PreviewDiffuse1);
	lightp->setAmbient(PreviewAmbientColor);
	lightp->setSpecular(PreviewSpecular1);
	lightp->setSpotExponent(0.f);
	lightp->setSpotCutoff(180.f);

	lightp = gGL.getLight(3);
	lightp->enable();
	lightp->setPosition(LLVector4(PreviewDirection2, 0.f));
	lightp->setDiffuse(PreviewDiffuse2);
	lightp->setAmbient(PreviewAmbientColor);
	lightp->setSpecular(PreviewSpecular2);
	lightp->setSpotExponent(0.f);
	lightp->setSpotCutoff(180.f);
}

void LLPipeline::enableLightsAvatarEdit()
{
	U32 mask = 0x2002;	// Avatar backlight only, set ambient
	setupAvatarLights(true);
	enableLights(mask);

	gGL.setAmbientLightColor(LLColor4(0.7f, 0.6f, 0.3f, 1.f));
}

void LLPipeline::enableLightsFullbright()
{
	U32 mask = 0x1000;	// Non-0 mask, set ambient
	enableLights(mask);
}

void LLPipeline::disableLights()
{
	enableLights(0);	// No lighting (full bright)
}

#if LL_DEBUG && 0
void LLPipeline::findReferences(LLDrawable* drawablep)
{
	if (mLights.find(drawablep) != mLights.end())
	{
		llinfos << "In mLights" << llendl;
	}
	if (std::find(mMovedList.begin(), mMovedList.end(),
				  drawablep) != mMovedList.end())
	{
		llinfos << "In mMovedList" << llendl;
	}
	if (std::find(mShiftList.begin(), mShiftList.end(),
				  drawablep) != mShiftList.end())
	{
		llinfos << "In mShiftList" << llendl;
	}
	if (mRetexturedList.find(drawablep) != mRetexturedList.end())
	{
		llinfos << "In mRetexturedList" << llendl;
	}

	if (std::find(mBuildQ.begin(), mBuildQ.end(), drawablep) != mBuildQ.end())
	{
		llinfos << "In mBuildQ" << llendl;
	}

	S32 count;

	count = gObjectList.findReferences(drawablep);
	if (count)
	{
		llinfos << "In other drawables: " << count << " references" << llendl;
	}
}
#endif

bool LLPipeline::verify()
{
	bool ok = true;
	for (pool_set_t::iterator iter = mPools.begin(), end = mPools.end();
		 iter != end; ++iter)
	{
		LLDrawPool* poolp = *iter;
		if (!poolp->verify())
		{
			ok = false;
		}
	}
	if (!ok)
	{
		llwarns << "Pipeline verify failed !" << llendl;
	}
	return ok;
}

//////////////////////////////
// Collision detection

///////////////////////////////////////////////////////////////////////////////
/**
 *	A method to compute a ray-AABB intersection.
 *	Original code by Andrew Woo, from "Graphics Gems", Academic Press, 1990
 *	Optimized code by Pierre Terdiman, 2000 (~20-30% faster on my Celeron 500)
 *	Epsilon value added by Klaus Hartmann. (discarding it saves a few cycles only)
 *
 *	Hence this version is faster as well as more robust than the original one.
 *
 *	Should work provided:
 *	1) the integer representation of 0.f is 0x00000000
 *	2) the sign bit of the float is the most significant one
 *
 *	Report bugs: p.terdiman@codercorner.com
 *
 *	\param		aabb		[in] the axis-aligned bounding box
 *	\param		origin		[in] ray origin
 *	\param		dir			[in] ray direction
 *	\param		coord		[out] impact coordinates
 *	\return		true if ray intersects AABB
 */
///////////////////////////////////////////////////////////////////////////////
//#define RAYAABB_EPSILON 0.00001f
#define IR(x)	((U32&)x)

bool LLRayAABB(const LLVector3& center, const LLVector3& size,
			   const LLVector3& origin, const LLVector3& dir,
			   LLVector3& coord, F32 epsilon)
{
	bool inside = true;
	LLVector3 MinB = center - size;
	LLVector3 MaxB = center + size;
	LLVector3 MaxT;
	MaxT.mV[VX] = MaxT.mV[VY] = MaxT.mV[VZ] = -1.f;

	// Find candidate planes.
	for (U32 i = 0; i < 3; ++i)
	{
		if (origin.mV[i] < MinB.mV[i])
		{
			coord.mV[i]	= MinB.mV[i];
			inside = false;

			// Calculate T distances to candidate planes
			if (IR(dir.mV[i]))
			{
				MaxT.mV[i] = (MinB.mV[i] - origin.mV[i]) / dir.mV[i];
			}
		}
		else if (origin.mV[i] > MaxB.mV[i])
		{
			coord.mV[i]	= MaxB.mV[i];
			inside = false;

			// Calculate T distances to candidate planes
			if (IR(dir.mV[i]))
			{
				MaxT.mV[i] = (MaxB.mV[i] - origin.mV[i]) / dir.mV[i];
			}
		}
	}

	// Ray origin inside bounding box
	if (inside)
	{
		coord = origin;
		return true;
	}

	// Get largest of the maxT's for final choice of intersection
	U32 WhichPlane = 0;
	if (MaxT.mV[1] > MaxT.mV[WhichPlane])
	{
		WhichPlane = 1;
	}
	if (MaxT.mV[2] > MaxT.mV[WhichPlane])
	{
		WhichPlane = 2;
	}

	// Check final candidate actually inside box
	if (IR(MaxT.mV[WhichPlane]) & 0x80000000)
	{
		return false;
	}

	for (U32 i = 0; i < 3; ++i)
	{
		if (i != WhichPlane)
		{
			coord.mV[i] = origin.mV[i] + MaxT.mV[WhichPlane] * dir.mV[i];
			if (epsilon > 0)
			{
				if (coord.mV[i] < MinB.mV[i] - epsilon ||
					coord.mV[i] > MaxB.mV[i] + epsilon)
				{
					return false;
				}
			}
			else if (coord.mV[i] < MinB.mV[i] || coord.mV[i] > MaxB.mV[i])
			{
				return false;
			}
		}
	}

	return true;	// ray hits box
}

void LLPipeline::setLight(LLDrawable* drawablep, bool is_light)
{
	if (drawablep)
	{
		if (is_light)
		{
			mLights.emplace(drawablep);
			drawablep->setState(LLDrawable::LIGHT);
		}
		else
		{
			drawablep->clearState(LLDrawable::LIGHT);
			mLights.erase(drawablep);
		}
	}
}

//static
void LLPipeline::toggleRenderType(U32 type)
{
//MK
	// Force the render type to true if our vision is restricted
	if (gRLenabled &&
		(type == RENDER_TYPE_AVATAR || type == RENDER_TYPE_PUPPET) &&
		gRLInterface.mVisionRestricted)
	{
		gPipeline.mRenderTypeEnabled[type] = true;
		return;
	}
//mk
	gPipeline.mRenderTypeEnabled[type] = !gPipeline.mRenderTypeEnabled[type];
	if (type == RENDER_TYPE_WATER)
	{
		gPipeline.mRenderTypeEnabled[RENDER_TYPE_VOIDWATER] =
			!gPipeline.mRenderTypeEnabled[RENDER_TYPE_VOIDWATER];
	}
}

//static
void LLPipeline::toggleRenderTypeControl(void* data)
{
	U32 type = (U32)(intptr_t)data;
	U32 bit = 1 << type;
	if (gPipeline.hasRenderType(type))
	{
		llinfos << "Toggling render type mask " << std::hex << bit << " off"
				<< std::dec << llendl;
	}
	else
	{
		llinfos << "Toggling render type mask " << std::hex << bit << " on"
				<< std::dec << llendl;
	}
	gPipeline.toggleRenderType(type);
}

//static
bool LLPipeline::hasRenderTypeControl(void* data)
{
	U32 type = (U32)(intptr_t)data;
	return gPipeline.hasRenderType(type);
}

// Allows UI items labeled "Hide foo" instead of "Show foo"
//static
bool LLPipeline::toggleRenderTypeControlNegated(void* data)
{
	U32 type = (U32)(intptr_t)data;
	return !gPipeline.hasRenderType(type);
}

//static
void LLPipeline::toggleRenderDebug(void* data)
{
	U32 bit = (U32)(intptr_t)data;
//MK
	if (gRLenabled && gRLInterface.mVisionRestricted)
	{
		bit = 0;
	}
//mk
	if (gPipeline.hasRenderDebugMask(bit))
	{
		llinfos << "Toggling render debug mask " << std::hex << bit << " off"
				<< std::dec << llendl;
	}
	else
	{
		llinfos << "Toggling render debug mask " << std::hex << bit << " on"
				<< std::dec << llendl;
	}
	gPipeline.mRenderDebugMask ^= bit;
}

//static
bool LLPipeline::toggleRenderDebugControl(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	return gPipeline.hasRenderDebugMask(bit);
}

//static
void LLPipeline::toggleRenderDebugFeature(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	gPipeline.mRenderDebugFeatureMask ^= bit;
}

//static
bool LLPipeline::toggleRenderDebugFeatureControl(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	return gPipeline.hasRenderDebugFeatureMask(bit);
}

//static
void LLPipeline::setRenderDebugFeatureControl(U32 bit, bool value)
{
	if (value)
	{
		gPipeline.mRenderDebugFeatureMask |= bit;
	}
	else
	{
		gPipeline.mRenderDebugFeatureMask &= !bit;
	}
}

LLVOPartGroup* LLPipeline::lineSegmentIntersectParticle(const LLVector4a& start,
														const LLVector4a& end,
														LLVector4a* intersectp,
														S32* face_hitp)
{
	LLVector4a local_end = end;
	LLVector4a position;
	LLDrawable* drawablep = NULL;

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			wend = gWorld.getRegionList().end();
			iter != wend; ++iter)
	{
		LLViewerRegion* region = *iter;
		if (!region) continue;	// Paranoia

		LLSpatialPartition* partp =
			region->getSpatialPartition(LLViewerRegion::PARTITION_PARTICLE);
		// PARTITION_PARTICLE cannot be NULL
		if (hasRenderType(partp->mDrawableType))
		{
			LLDrawable* hitp = partp->lineSegmentIntersect(start, local_end,
														   true, false,
														   face_hitp,
														   &position);
			if (hitp)
			{
				drawablep = hitp;
				local_end = position;
			}
		}
	}

	LLVOPartGroup* partp = NULL;
	if (drawablep)
	{
		// Make sure we are returning an LLVOPartGroup
		partp = drawablep->getVObj().get()->asVOPartGroup();
	}

	if (intersectp)
	{
		*intersectp = position;
	}

	return partp;
}

LLViewerObject* LLPipeline::lineSegmentIntersectInWorld(const LLVector4a& start,
														const LLVector4a& end,
														bool pick_transparent,
														bool pick_rigged,
														S32* face_hit,
														// intersection point
														LLVector4a* intersection,
														// texcoords of intersection
														LLVector2* tex_coord,
														// normal at intersection
														LLVector4a* normal,
														// tangent at intersection
														LLVector4a* tangent)
{
	LLDrawable* drawablep = NULL;
	LLVector4a local_end = end;
	LLVector4a position;

	sPickAvatar = false;	// !gToolMgr.inBuildMode();

	// Only check these non-avatar partitions in a first step
	static const U32 non_avatars[] =
	{
		LLViewerRegion::PARTITION_TERRAIN,
		LLViewerRegion::PARTITION_TREE,
		LLViewerRegion::PARTITION_GRASS,
		LLViewerRegion::PARTITION_VOLUME,
		LLViewerRegion::PARTITION_BRIDGE,
		LLViewerRegion::PARTITION_PUPPET,
	};
	constexpr U32 non_avatars_count = LL_ARRAY_SIZE(non_avatars);

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			wend = gWorld.getRegionList().end();
		 iter != wend; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (!regionp) continue;	// Paranoia

		for (U32 j = 0; j < non_avatars_count; ++j)
		{
			U32 type = non_avatars[j];
			LLSpatialPartition* partp = regionp->getSpatialPartition(type);
			// None of the partitions under PARTITION_VO_CACHE can be NULL
			if (!hasRenderType(partp->mDrawableType))
			{
				continue;
			}
			LLDrawable* hitp =
				partp->lineSegmentIntersect(start, local_end,
											// Terrain, tree and grass cannot
											// be transparent neither rigged !
											// HB
											pick_transparent && j >= 3,
											pick_rigged && j >= 3,
											face_hit, &position,
											tex_coord, normal, tangent);
			if (hitp)
			{
				drawablep = hitp;
				local_end = position;
			}
		}
	}

	if (!sPickAvatar)
	{
		// Save hit info in case we need to restore due to attachment override
		LLVector4a local_normal;
		LLVector4a local_tangent;
		LLVector2 local_texcoord;
		S32 local_face_hit = -1;

		if (face_hit)
		{
			local_face_hit = *face_hit;
		}
		if (tex_coord)
		{
			local_texcoord = *tex_coord;
		}
		if (tangent)
		{
			local_tangent = *tangent;
		}
		else
		{
			local_tangent.clear();
		}
		if (normal)
		{
			local_normal = *normal;
		}
		else
		{
			local_normal.clear();
		}

		constexpr F32 ATTACHMENT_OVERRIDE_DIST = 0.1f;

		// Check against avatars
		sPickAvatar = true;
		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				wend = gWorld.getRegionList().end();
			 iter != wend; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			if (!regionp) continue;	// Paranoia

			LLSpatialPartition* partp =
				regionp->getSpatialPartition(LLViewerRegion::PARTITION_AVATAR);
			// Note: PARTITION_AVATAR cannot be NULL
			if (!hasRenderType(partp->mDrawableType))
			{
				continue;
			}
			LLDrawable* hitp =
				partp->lineSegmentIntersect(start, local_end, pick_transparent,
											pick_rigged, face_hit, &position,
											tex_coord, normal, tangent);
			if (hitp)
			{
				LLVector4a delta;
				delta.setSub(position, local_end);

				if (!drawablep || !drawablep->getVObj()->isAttachment() ||
					delta.getLength3().getF32() > ATTACHMENT_OVERRIDE_DIST)
				{
					// Avatar overrides if previously hit drawable is not
					// an attachment or attachment is far enough away from
					// detected intersection
					drawablep = hitp;
					local_end = position;
				}
				else
				{
					// Prioritize attachments over avatars
					position = local_end;
					if (face_hit)
					{
						*face_hit = local_face_hit;
					}
					if (tex_coord)
					{
						*tex_coord = local_texcoord;
					}
					if (tangent)
					{
						*tangent = local_tangent;
					}
					if (normal)
					{
						*normal = local_normal;
					}
				}
			}
		}
	}

	// Check all avatar name tags
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* avp = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (avp && avp->mNameText.notNull() &&
			avp->mNameText->lineSegmentIntersect(start, local_end, position))
		{
			drawablep = avp->mDrawable;
			local_end = position;
		}
	}

	if (intersection)
	{
		*intersection = position;
	}

	return drawablep ? drawablep->getVObj().get() : NULL;
}

LLViewerObject* LLPipeline::lineSegmentIntersectInHUD(const LLVector4a& start,
													  const LLVector4a& end,
													  bool pick_transparent,
													  S32* face_hitp,
													  LLVector4a* intersection,
													  LLVector2* tex_coord,
													  LLVector4a* normal,
													  LLVector4a* tangent)
{
	LLDrawable* drawablep = NULL;

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			wend = gWorld.getRegionList().end();
		 iter != wend; ++iter)
	{
		LLViewerRegion* region = *iter;

		bool toggle = false;
		if (!hasRenderType(RENDER_TYPE_HUD))
		{
			toggleRenderType(RENDER_TYPE_HUD);
			toggle = true;
		}

		LLSpatialPartition* partp =
			region->getSpatialPartition(LLViewerRegion::PARTITION_HUD);
		// PARTITION_HUD cannot be NULL
		LLDrawable* hitp = partp->lineSegmentIntersect(start, end,
													   pick_transparent, false,
													   face_hitp, intersection,
													   tex_coord, normal,
													   tangent);
		if (hitp)
		{
			drawablep = hitp;
		}

		if (toggle)
		{
			toggleRenderType(RENDER_TYPE_HUD);
		}
	}
	return drawablep ? drawablep->getVObj().get() : NULL;
}

LLSpatialPartition* LLPipeline::getSpatialPartition(LLViewerObject* objp)
{
	if (objp)
	{
		LLViewerRegion* regionp = objp->getRegion();
		if (regionp)
		{
			return regionp->getSpatialPartition(objp->getPartitionType());
		}
	}
	return NULL;
}

void LLPipeline::resetVertexBuffers(LLDrawable* drawablep)
{
	if (drawablep)
	{
		for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
		{
			LLFace* facep = drawablep->getFace(i);
			if (facep)
			{
				facep->clearVertexBuffer();
			}
		}
	}
}

void LLPipeline::resetVertexBuffers()
{
	mResetVertexBuffers = true;
	updateRenderDeferred();
}

void LLPipeline::doResetVertexBuffers(bool forced)
{
	if (!mResetVertexBuffers)
	{
		return;
	}

	// Wait for teleporting to finish
	if (!forced && LLSpatialPartition::sTeleportRequested)
	{
		if (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)
		{
			// Teleporting aborted
			LLSpatialPartition::sTeleportRequested = false;
			mResetVertexBuffers = false;
		}
		return;
	}

	LL_FAST_TIMER(FTM_RESET_VB);
	mResetVertexBuffers = false;

	gGL.flush();
	glFinish();

	LLVertexBuffer::unbind();

	// Delete our utility buffers
	mDeferredVB = mGlowCombineVB = mCubeVB = mScreenTriangleVB = NULL;

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; ++i)
		{
			LLSpatialPartition* partp = regionp->getSpatialPartition(i);
			if (partp)
			{
				partp->resetVertexBuffers();
			}
		}
	}

	if (LLSpatialPartition::sTeleportRequested)
	{
		LLSpatialPartition::sTeleportRequested = false;
		gWorld.clearAllVisibleObjects();
		clearRebuildDrawables();
	}

	resetDrawOrders();

	gSky.resetVertexBuffers();

	gGL.resetVertexBuffer();

	LLVertexBuffer::cleanupClass();

#if LL_DEBUG_VB_ALLOC
	if (LLVertexBuffer::getGLCount())
	{
		llwarns << "VBO wipe failed: " << LLVertexBuffer::getGLCount()
				<< " buffers remaining." << llendl;
		LLVertexBuffer::dumpInstances();
	}
#endif

	updateRenderDeferred();

	LLVertexBuffer::initClass();
	gGL.initVertexBuffer();

	createAuxVBs();	// Recreate our utility buffers...

	LLDrawPoolWater::restoreGL();
	LLDrawPoolWLSky::restoreGL();
}

void LLPipeline::renderObjects(U32 type, U32 mask, bool texture,
							   bool batch_texture, bool rigged)
{
	LL_TRACY_TIMER(TRC_RENDER_OBJECTS);
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
	if (rigged)
	{
		mSimplePool->pushRiggedBatches(type + 1, mask, texture, batch_texture);
	}
	else
	{
		mSimplePool->pushBatches(type, mask, texture, batch_texture);
	}
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

// Used only by the PBR renderer
void LLPipeline::renderGLTFObjects(U32 type, bool texture, bool rigged)
{
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;

	if (rigged)
	{
		mSimplePool->pushRiggedGLTFBatches(type + 1, texture);
	}
	else
	{
		mSimplePool->pushGLTFBatches(type, texture);
	}

	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

// Branched version for the PBR renderer
void LLPipeline::renderAlphaObjectsPBR(bool rigged)
{
	LL_TRACY_TIMER(TRC_RENDER_ALPHA_OBJECTS);

	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;

	constexpr U32 type = LLRenderPass::PASS_ALPHA;
	const F32 width = LLRenderTarget::sCurResX;

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	LLCullResult::drawinfo_list_t& draw_list = getRenderMap(type);
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

		bool has_avatar = paramsp->mAvatar.notNull();
		if (rigged != has_avatar)
		{
			// This pool contains both rigged and non-rigged DrawInfos. Only
			// draw the objects we are interested in in this pass.
			continue;
		}

		bool has_pbr_mat = paramsp->mGLTFMaterial.notNull();
		LLGLSLShader* shaderp =
			has_pbr_mat ? &gDeferredShadowGLTFAlphaBlendProgram
						: &gDeferredShadowAlphaMaskProgram;
		shaderp->bind(rigged);
		shaderp = LLGLSLShader::sCurBoundShaderPtr;
		shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, mIsSunUp);
		shaderp->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, width);
		shaderp->setMinimumAlpha(ALPHA_BLEND_CUTOFF);

		if (rigged && paramsp->mSkinInfo &&
			(paramsp->mAvatar.get() != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0
			if (!mSimplePool->uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			mSimplePool->uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar.get();
			last_hash = paramsp->mSkinInfo->mHash;
		}

		if (has_pbr_mat)
		{
			mSimplePool->pushGLTFBatch(*paramsp);
		}
		else
		{
			mSimplePool->pushBatch(*paramsp, 0, true, true);
		}
	}

	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

void LLPipeline::renderAlphaObjects(bool rigged)
{
	if (gUsePBRShaders)
	{
		renderAlphaObjectsPBR(rigged);
		return;
	}

	LL_TRACY_TIMER(TRC_RENDER_ALPHA_OBJECTS);

	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;

	constexpr U32 type = LLRenderPass::PASS_ALPHA;
	constexpr U32 mask = LLVertexBuffer::MAP_VERTEX |
						 LLVertexBuffer::MAP_TEXCOORD0 |
						 LLVertexBuffer::MAP_COLOR |
						 LLVertexBuffer::MAP_TEXTURE_INDEX;

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	LLCullResult::drawinfo_list_t& draw_list = getRenderMap(type);
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

		bool has_avatar = paramsp->mAvatar.notNull();
		if (rigged != has_avatar)
		{
			// This pool contains both rigged and non-rigged DrawInfos. Only
			// draw the objects we are interested in in this pass.
			continue;
		}

		if (!rigged)
		{
			mSimplePool->pushBatch(*paramsp, mask, true, true);
			continue;
		}

		if (paramsp->mSkinInfo &&
			(paramsp->mAvatar.get() != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0
			if (!mSimplePool->uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			mSimplePool->uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar.get();
			last_hash = paramsp->mSkinInfo->mHash;
		}
		mSimplePool->pushBatch(*paramsp, mask | LLVertexBuffer::MAP_WEIGHT4,
							   true, true);
	}

	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

void LLPipeline::renderMaskedObjects(U32 type, U32 mask, bool texture,
									 bool batch_texture, bool rigged)
{
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
	if (rigged)
	{
		mAlphaMaskPool->pushRiggedMaskBatches(type + 1, mask, texture,
											  batch_texture);
	}
	else
	{
		mAlphaMaskPool->pushMaskBatches(type, mask, texture, batch_texture);
	}
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

void LLPipeline::renderFullbrightMaskedObjects(U32 type, U32 mask,
											   bool texture,
											   bool batch_texture, bool rigged)
{
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
	if (rigged)
	{
		mFullbrightAlphaMaskPool->pushRiggedMaskBatches(type + 1, mask,
														texture,
														batch_texture);
	}
	else
	{
		mFullbrightAlphaMaskPool->pushMaskBatches(type, mask, texture,
												  batch_texture);
	}
	gGL.loadMatrix(gGLModelView);
	gGLLastMatrix = NULL;
}

// PBR renderer only
void LLPipeline::visualizeBuffers(LLRenderTarget* srcp, LLRenderTarget* dstp,
								  U32 buff_idx)
{
	dstp->bindTarget();

	LLGLSLShader* shaderp = &gDeferredBufferVisualProgram;
	shaderp->bind();
	shaderp->bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, srcp, false,
						 LLTexUnit::TFO_BILINEAR, buff_idx);
	shaderp->uniform1f(sMipLevel, RenderBufferVisualization == 4 ? 8.f : 0.f);
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	shaderp->unbind();

	dstp->flush();
}

// PBR renderer only
void LLPipeline::generateLuminance(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_LUMINANCE);

	dstp->bindTarget();

	LLGLDepthTest depth(GL_FALSE, GL_FALSE);

	gLuminanceProgram.bind();

	S32 chan = gLuminanceProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE);
	if (chan > -1)
	{
		srcp->bindTexture(0, chan, LLTexUnit::TFO_POINT);
	}

	chan = gLuminanceProgram.enableTexture(LLShaderMgr::DEFERRED_EMISSIVE);
	if (chan > -1)
	{
		mGlow[1].bindTexture(0, chan);
	}

	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	dstp->flush();

	gLuminanceProgram.unbind();
}

// PBR renderer only
void LLPipeline::generateExposure(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_EXPOSURE);

	static LLCachedControl<F32> exp_coeff(gSavedSettings,
										  "RenderDynamicExposureCoefficient");

	// Copy last frame's exposure into mLastExposure
	mLastExposure.bindTarget();
	gCopyProgram.bind();
	gGL.getTexUnit(0)->bind(dstp);
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	mLastExposure.flush();

	dstp->bindTarget();

	LLGLDepthTest depth(GL_FALSE, GL_FALSE);

	gExposureProgram.bind();

	S32 chan = gExposureProgram.enableTexture(LLShaderMgr::DEFERRED_EMISSIVE);
	if (chan > -1)
	{
		mLuminanceMap.bindTexture(0, chan, LLTexUnit::TFO_TRILINEAR);
	}

	chan = gExposureProgram.enableTexture(LLShaderMgr::EXPOSURE_MAP);
	if (chan > -1)
	{
		mLastExposure.bindTexture(0, chan);
	}

	gExposureProgram.uniform1f(sDT, gFrameIntervalSeconds);
	gExposureProgram.uniform2f(sNoiseVec, ll_frand() * 2.f - 1.f,
							   ll_frand() * 2.f - 1.f);

	F32 exp_min = 1.f;
	F32 exp_max = 1.f;
	if (mProbeAmbiance > 0.f)
	{
		F32 hdr_scale = 2.f * sqrtf(mSkyGamma);
		if (hdr_scale > 1.f)
		{
			exp_min = 1.f / hdr_scale;
			exp_max = hdr_scale;
		}
	}
	gExposureProgram.uniform3f(sExpParams, exp_coeff, exp_min, exp_max);

	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	gGL.getTexUnit(chan)->unbind(mLastExposure.getUsage());
	gExposureProgram.unbind();

	dstp->flush();
}

// PBR renderer only
void LLPipeline::generateGlow(LLRenderTarget* srcp)
{
	LL_TRACY_TIMER(TRC_RENDER_GLOW);

	if (!RenderGlow)
	{
		mGlow[1].bindTarget();
		mGlow[1].clear();
		mGlow[1].flush();
		return;
	}

	mGlow[2].bindTarget();
	mGlow[2].clear();

	gGlowExtractProgram.bind();
	gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MIN_LUMINANCE, 9999);
	gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MAX_EXTRACT_ALPHA,
								  RenderGlowMaxExtractAlpha);
	gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_LUM_WEIGHTS,
								  RenderGlowLumWeights.mV[0],
								  RenderGlowLumWeights.mV[1],
								  RenderGlowLumWeights.mV[2]);
	gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_WARMTH_WEIGHTS,
								  RenderGlowWarmthWeights.mV[0],
								  RenderGlowWarmthWeights.mV[1],
								  RenderGlowWarmthWeights.mV[2]);
	gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_WARMTH_AMOUNT,
								  RenderGlowWarmthAmount);

	static LLCachedControl<bool> with_noise(gSavedSettings, "RenderGlowNoise");
	if (with_noise)
	{
		S32 channel =
			gGlowExtractProgram.enableTexture(LLShaderMgr::GLOW_NOISE_MAP);
		if (channel > -1)
		{
			LLTexUnit* unitp = gGL.getTexUnit(channel);
			unitp->bindManual(LLTexUnit::TT_TEXTURE, mTrueNoiseMap);
			unitp->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		}
		gGlowExtractProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
									  mGlow[2].getWidth(),
									  mGlow[2].getHeight());
	}

	{
		LLGLEnable blend_on(GL_BLEND);
		gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);
		gGlowExtractProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, srcp);
		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		enableLightsFullbright();
		mScreenTriangleVB->setBuffer();
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
		mGlow[2].flush();
	}
	gGlowExtractProgram.unbind();

	// Power of two between 1 and 1024
	const U32 glow_res = llclamp(1 << RenderGlowResolutionPow, 1, 1024);

	S32 kernel = RenderGlowIterations * 2;
	F32 delta = RenderGlowWidth / (F32)glow_res;
	// Use half the glow width if we have the res set to less than 9 so that it
	// looks almost the same in either case.
	if (RenderGlowResolutionPow < 9)
	{
		delta *= 0.5f;
	}

	gGlowProgram.bind();
	gGlowProgram.uniform1f(LLShaderMgr::GLOW_STRENGTH, RenderGlowStrength);

	for (S32 i = 0; i < kernel; ++i)
	{
		mGlow[i % 2].bindTarget();
		mGlow[i % 2].clear();

		if (i == 0)
		{
			gGlowProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, &mGlow[2]);
		}
		else
		{
			gGlowProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP,
									 &mGlow[(i - 1) % 2]);
		}

		if (i % 2 == 0)
		{
			gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, delta, 0.f);
		}
		else
		{
			gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, 0.f, delta);
		}

		mScreenTriangleVB->setBuffer();
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

		mGlow[i % 2].flush();
	}

	gGlowProgram.unbind();
}

// PBR renderer only
void LLPipeline::combineGlow(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_GLOW_COMBINE);

	dstp->bindTarget();

	gGlowCombineProgram.bind();
	gGlowCombineProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, srcp);
	gGlowCombineProgram.bindTexture(LLShaderMgr::DEFERRED_EMISSIVE, &mGlow[1]);
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	dstp->flush();
}

// PBR renderer only
void LLPipeline::gammaCorrect(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_GAMMA_CORRECT);

	static LLCachedControl<bool> no_post(gSavedSettings,
										 "RenderDisablePostProcessing");
	static LLCachedControl<F32> exposure(gSavedSettings, "RenderExposure");

	dstp->bindTarget();

	LLGLDepthTest depth(GL_FALSE, GL_FALSE);

	static LLGLSLShader* last_shaderp = NULL;

	LLGLSLShader* shaderp;
	if (gSnapshotNoPost || (no_post && gToolMgr.inBuildMode()))
	{
		shaderp = &gNoPostGammaCorrectProgram;
		if (shaderp != last_shaderp)
		{
			llinfos << "Gamma shader in use: gNoPostGammaCorrectProgram"
					<< llendl;
		}
	}
	else if (mProbeAmbiance <= 0.f)
	{
		shaderp = &gLegacyPostGammaCorrectProgram;
		if (shaderp != last_shaderp)
		{
			llinfos << "Gamma shader in use: gLegacyPostGammaCorrectProgram"
					<< llendl;
		}
	}
	else
	{
		shaderp = &gDeferredPostGammaCorrectProgram;
		if (shaderp != last_shaderp)
		{
			llinfos << "Gamma shader in use: gDeferredPostGammaCorrectProgram"
					<< llendl;
		}
	}
	last_shaderp = shaderp;

	shaderp->bind();

	shaderp->bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, srcp, false,
						 LLTexUnit::TFO_POINT);
	shaderp->bindTexture(LLShaderMgr::EXPOSURE_MAP, &mExposureMap);
	shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, srcp->getWidth(),
					   srcp->getHeight());
	shaderp->uniform1f(sExposure,  llclamp(F32(exposure), 0.5f, 4.f));

	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	gGL.getTexUnit(0)->unbind(srcp->getUsage());
	shaderp->unbind();

	dstp->flush();
}

// PBR renderer only
void LLPipeline::copyRenderTarget(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_COPY_TARGET);

	dstp->bindTarget();

	LLGLSLShader* shaderp = &gDeferredPostNoDoFProgram;
	shaderp->bind();
	shaderp->bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, srcp);
	shaderp->bindTexture(LLShaderMgr::DEFERRED_DEPTH, &mRT->mDeferredScreen,
						 true);
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	shaderp->unbind();

	dstp->flush();
}

// PBR renderer only. Returns true when FXAA got actually applied. HB
bool LLPipeline::applyFXAA(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_FXAA);

	if (!LLViewerShaderMgr::sHasFXAA || RenderFSAASamples <= 1 ||
		!mRT->mFXAABuffer.isComplete())
	{
		copyRenderTarget(srcp, dstp);
		return false;
	}

	// Bake out texture2D with RGBL for FXAA shader
	mRT->mFXAABuffer.bindTarget();

	LLGLSLShader* shaderp = &gGlowCombineFXAAProgram;
	shaderp->bind();
	LLTexUnit::eTextureType mode = srcp->getUsage();
	S32 channel = shaderp->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mode);
	if (channel > -1)
	{
		srcp->bindTexture(0, channel, LLTexUnit::TFO_BILINEAR);
	}

	LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);

	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	shaderp->disableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mode);
	shaderp->unbind();

	mRT->mFXAABuffer.flush();

	dstp->bindTarget();
	shaderp = &gFXAAProgram[RenderDeferredAAQuality];
	shaderp->bind();

	channel = shaderp->enableTexture(LLShaderMgr::DIFFUSE_MAP,
									 mRT->mFXAABuffer.getUsage());
	if (channel > -1)
	{
		mRT->mFXAABuffer.bindTexture(0, channel, LLTexUnit::TFO_BILINEAR);
	}

	gViewerWindowp->setupViewport();

	F32 inv_width = 1.f / F32(mRT->mFXAABuffer.getWidth());
	F32 inv_height = 1.f / F32(mRT->mFXAABuffer.getHeight());
	F32 scale_x = F32(dstp->getWidth()) * inv_width;
	F32 scale_y = F32(dstp->getHeight()) * inv_height;
	shaderp->uniform2f(LLShaderMgr::FXAA_TC_SCALE, scale_x, scale_y);
	shaderp->uniform2f(LLShaderMgr::FXAA_RCP_SCREEN_RES, inv_width,
					   inv_height);
	shaderp->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT,
					   -0.5f * inv_width, -0.5f * inv_height,
					   0.5f * inv_width, 0.5f * inv_height);
	shaderp->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT2,
					   -2.f * inv_width, -2.f * inv_height,
					   2.f * inv_width, 2.f * inv_height);


	channel = shaderp->getTextureChannel(LLShaderMgr::DEFERRED_DEPTH);
	gGL.getTexUnit(channel)->bind(&mRT->mDeferredScreen, true);
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	shaderp->unbind();
	dstp->flush();

	return true;
}

#if HB_PBR_SMAA_AND_CAS
// PBR renderer only. Returns true when SMAA got actually applied. HB
bool LLPipeline::applySMAA(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_SMAA);

	if (!LLViewerShaderMgr::sHasSMAA || RenderFSAASamples <= 1 ||
		!mAreaMap || !mSearchMap || !mRT->mSMAAEdgeBuffer.isComplete() ||
		!mRT->mSMAABlendBuffer.isComplete())
	{
		copyRenderTarget(srcp, dstp);
		return false;
	}

	LLGLSLShader* shaderp;

	// Note: all buffers got the same size.
	S32 width = srcp->getWidth();
	S32 height = srcp->getHeight();
#if 0
	mRT->mFXAABuffer.copyContents(*srcp, 0, 0, width, height, 0, 0, width,
								  height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
#else
	// Bake out texture2D with RGBL for SMAA shader
	mRT->mFXAABuffer.bindTarget();

	shaderp = &gGlowCombineFXAAProgram;
	shaderp->bind();
	LLTexUnit::eTextureType mode = srcp->getUsage();
	S32 channel = shaderp->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mode);
	if (channel > -1)
	{
		srcp->bindTexture(0, channel, LLTexUnit::TFO_BILINEAR);
	}
	{
		LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);
		mScreenTriangleVB->setBuffer();
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	}
	shaderp->disableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mode);
	shaderp->unbind();

	mRT->mFXAABuffer.flush();
#endif

	// OK above, KO below...

	glClearColor(0.f, 0.f, 0.f, 0.f);

	glViewport(0, 0, width, height);
	F32 rt_metrics[] = { 1.f / width, 1.f / height, (F32)width, (F32)height };

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	LLTexUnit* unit1 = gGL.getTexUnit(1);
	LLTexUnit* unit2 = gGL.getTexUnit(2);

	mRT->mSMAAEdgeBuffer.bindTarget();
	mRT->mSMAAEdgeBuffer.clear(GL_COLOR_BUFFER_BIT);

	shaderp = &gPostSMAAEdgeDetect[RenderDeferredAAQuality];
	shaderp->bind();
	shaderp->uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

	mRT->mFXAABuffer.bindTexture(0, 0, LLTexUnit::TFO_BILINEAR);
	unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	unit0->setTextureColorSpace(LLTexUnit::TCS_LINEAR);

	{
		LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);
		channel = shaderp->getTextureChannel(LLShaderMgr::DEFERRED_DEPTH);
		gGL.getTexUnit(channel)->bind(&mRT->mDeferredScreen, true);
		mScreenTriangleVB->setBuffer();
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	}

	unit0->disable();

	shaderp->unbind();

	mRT->mSMAAEdgeBuffer.flush();

	mRT->mSMAABlendBuffer.bindTarget();
	mRT->mSMAABlendBuffer.clear(GL_COLOR_BUFFER_BIT);

	shaderp = &gPostSMAABlendWeights[RenderDeferredAAQuality];
	shaderp->bind();
	shaderp->uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

	mRT->mSMAAEdgeBuffer.bindTexture(0, 0, LLTexUnit::TFO_BILINEAR);
	unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	unit0->setTextureColorSpace(LLTexUnit::TCS_LINEAR);
	unit1->bindManual(LLTexUnit::TT_TEXTURE, mAreaMap);
	unit1->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
	unit1->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	unit1->setTextureColorSpace(LLTexUnit::TCS_LINEAR);
	unit2->bindManual(LLTexUnit::TT_TEXTURE, mSearchMap);
	unit2->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
	unit2->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	unit2->setTextureColorSpace(LLTexUnit::TCS_LINEAR);

	{
		LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);
		channel = shaderp->getTextureChannel(LLShaderMgr::DEFERRED_DEPTH);
		gGL.getTexUnit(channel)->bind(&mRT->mDeferredScreen, true);
		mScreenTriangleVB->setBuffer();
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	}

	unit0->disable();
	unit1->disable();
	unit2->disable();

	shaderp->unbind();

	mRT->mSMAABlendBuffer.flush();

	dstp->bindTarget();

	shaderp = &gPostSMAANeighborhoodBlend[RenderDeferredAAQuality];
	shaderp->bind();
	shaderp->uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

	mRT->mFXAABuffer.bindTexture(0, 0, LLTexUnit::TFO_BILINEAR);
	unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	unit0->setTextureColorSpace(LLTexUnit::TCS_LINEAR);
	mRT->mSMAABlendBuffer.bindTexture(0, 1, LLTexUnit::TFO_BILINEAR);
	unit1->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	unit1->setTextureColorSpace(LLTexUnit::TCS_LINEAR);

	gViewerWindowp->setupViewport();

	{
		LLGLDepthTest depth(GL_FALSE, GL_FALSE);
		mScreenTriangleVB->setBuffer();
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	}

	unit0->disable();
	unit1->disable();

	shaderp->unbind();

	dstp->flush();

	return true;
}

// PBR renderer only.
void LLPipeline::applyCAS(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_CAS);

	dstp->bindTarget();

	gPostCASProgram.bind();

	static LLCachedControl<LLVector3> cas_params(gSavedSettings,
												 "RenderDeferredCASParams");
	LLVector3 params = LLVector3(cas_params);
	params.clamp(0.f, 1.f);
	gPostCASProgram.uniform3fv(sSharpness, 1, params.mV);

	gPostCASProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, srcp, false,
								LLTexUnit::TFO_POINT);

	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	gPostCASProgram.unbind();

	dstp->flush();
}
#endif

// Helper function used to factorize common code in EE and PBR renderers. HB
static void calc_doff_params(F32& subject_dist, F32& blur_constant,
							 F32& magnification)
{
	static F32 current_dist = 16.f;
	static F32 start_dist = 16.f;
	static F32 transition_time = 1.f;

	static LLCachedControl<F32> cam_trans_time(gSavedSettings,
											   "CameraFocusTransitionTime");
	static LLCachedControl<F32> camera_fnum(gSavedSettings, "CameraFNumber");

	static LLCachedControl<F32> cam_default_focal(gSavedSettings,
												  "CameraFocalLength");
	static LLCachedControl<F32> camera_fov(gSavedSettings,
										   "CameraFieldOfView");

	LLVector3 focus_point;
	LLViewerMediaFocus* mfocusp = LLViewerMediaFocus::getInstance();
	LLViewerObject* objp = mfocusp->getFocusedObject();
	if (objp && objp->mDrawable && objp->isSelected())
	{
		// Focus on selected media object
		S32 face_idx = mfocusp->getFocusedFace();
		if (objp && objp->mDrawable)
		{
			LLFace* facep = objp->mDrawable->getFace(face_idx);
			if (facep)
			{
				focus_point = facep->getPositionAgent();
			}
		}
	}
	if (focus_point.isExactlyZero())
	{
		if (LLViewerJoystick::getInstance()->getOverrideCamera())
		{
			// Focus on point under cursor
			focus_point.set(gDebugRaycastIntersection.getF32ptr());
		}
		else if (gAgent.cameraMouselook())
		{
			// Focus on point under mouselook crosshairs
			LLVector4a result;
			result.clear();
			gViewerWindowp->cursorIntersect(-1, -1, 512.f, NULL, -1,
											false, false, NULL, &result);
			focus_point.set(result.getF32ptr());
		}
		else
		{
			// Focus on alt-zoom target
			LLViewerRegion* regionp = gAgent.getRegion();
			if (regionp)
			{
				focus_point = LLVector3(gAgent.getFocusGlobal() -
										regionp->getOriginGlobal());
			}
		}
	}

	LLVector3 eye = gViewerCamera.getOrigin();
	F32 target_dist = 16.f;
	if (!focus_point.isExactlyZero())
	{
		target_dist = gViewerCamera.getAtAxis() * (focus_point - eye);
	}

	if (transition_time >= 1.f &&
		fabsf(current_dist - target_dist) / current_dist > 0.01f)
	{
		// Large shift happened, interpolate smoothly to new target distance
		transition_time = 0.f;
		start_dist = current_dist;
	}
	else if (transition_time < 1.f)
	{
		// Currently in a transition, continue interpolating
		transition_time += 1.f / llmax(F32_MIN,
									   cam_trans_time * gFrameIntervalSeconds);
		transition_time = llmin(transition_time, 1.f);

		F32 t = cosf(transition_time * F_PI + F_PI) * 0.5f + 0.5f;
		current_dist = start_dist + (target_dist - start_dist) * t;
	}
	else
	{
		// Small or no change, just snap to target distance
		current_dist = target_dist;
	}

	// From wikipedia -- c = |s2-s1|/s2 * f^2/(N(S1-f))
	// where	 N = CameraFNumber
	//			 s2 = dot distance
	//			 s1 = subject distance
	//			 f = focal length
	subject_dist = current_dist * 1000.f;	// In mm
	F32 dv = 2.f * cam_default_focal * tanf(camera_fov * DEG_TO_RAD * 0.5f);
	F32 focal = dv / (2.f * tanf(gViewerCamera.getView() * 0.5f));
	blur_constant = focal * focal / (camera_fnum * (subject_dist - focal));
	blur_constant *= 0.001f; // Convert to meters for shader
	if (subject_dist == focal)
	{
		magnification = F32_MAX;
	}
	else
	{
		magnification = focal / (subject_dist - focal);
	}
	subject_dist *= 0.001f; // Convert back to meters for shader
}

// PBR renderer only. Returns true when DoF got actually applied. HB
bool LLPipeline::renderDoF(LLRenderTarget* srcp, LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_DOF);

	if (!RenderDepthOfField || gCubeSnapshot ||
		(!RenderDepthOfFieldInEditMode && gToolMgr.inBuildMode()))
	{
		copyRenderTarget(srcp, dstp);
		return false;
	}

	F32 subject_dist, blur_constant, magnification;
	calc_doff_params(subject_dist, blur_constant, magnification);

	LLGLDisable blend(GL_BLEND);

	// Build diffuse + bloom + CoF
	mRT->mDeferredLight.bindTarget();
	LLGLSLShader* shaderp = &gDeferredCoFProgram;
	shaderp->bind();
	shaderp->bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, srcp,
						 LLTexUnit::TFO_POINT);
	shaderp->bindTexture(LLShaderMgr::DEFERRED_DEPTH, &mRT->mDeferredScreen,
						 true);
	shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, dstp->getWidth(),
					   dstp->getHeight());
	shaderp->uniform1f(LLShaderMgr::DOF_FOCAL_DISTANCE, -subject_dist);
	shaderp->uniform1f(LLShaderMgr::DOF_BLUR_CONSTANT, blur_constant);
	shaderp->uniform1f(LLShaderMgr::DOF_TAN_PIXEL_ANGLE,
					   tanf(1.f / LLDrawable::sCurPixelAngle));
	shaderp->uniform1f(LLShaderMgr::DOF_MAGNIFICATION, magnification);
	shaderp->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
	shaderp->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	shaderp->unbind();
	mRT->mDeferredLight.flush();

	U32 dof_width = U32(mRT->mScreen.getWidth() * CameraDoFResScale);
	U32 dof_height = U32(mRT->mScreen.getHeight() * CameraDoFResScale);

	// Perform DoF sampling at half-res (preserve alpha channel)
	srcp->bindTarget();
	glViewport(0, 0, dof_width, dof_height);
	gGL.setColorMask(true, false);

	shaderp = &gDeferredPostProgram;
	shaderp->bind();
	shaderp->bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, &mRT->mDeferredLight,
						 LLTexUnit::TFO_POINT);
	shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, dstp->getWidth(),
						dstp->getHeight());
	shaderp->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
	shaderp->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	shaderp->unbind();
	srcp->flush();
	gGL.setColorMask(true, true);

	// Combine result based on alpha
	dstp->bindTarget();
	if (RenderFSAASamples > 1 && mRT->mFXAABuffer.isComplete())
	{
		glViewport(0, 0, dstp->getWidth(), dstp->getHeight());
	}
	else
	{
		gViewerWindowp->setupViewport();
	}
	shaderp = &gDeferredDoFCombineProgram;
	shaderp->bind();
	shaderp->bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, srcp,
						 LLTexUnit::TFO_POINT);
	shaderp->bindTexture(LLShaderMgr::DEFERRED_LIGHT, &mRT->mDeferredLight,
						 LLTexUnit::TFO_POINT);
	shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, dstp->getWidth(),
						dstp->getHeight());
	shaderp->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
	shaderp->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);
	shaderp->uniform1f(LLShaderMgr::DOF_WIDTH,
					   (dof_width - 1) / (F32)srcp->getWidth());
	shaderp->uniform1f(LLShaderMgr::DOF_HEIGHT,
					   (dof_height - 1) / (F32)srcp->getHeight());
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	shaderp->unbind();
	dstp->flush();

	return true;
}

// PBR renderer only
void LLPipeline::copyScreenSpaceReflections(LLRenderTarget* srcp,
											LLRenderTarget* dstp)
{
	LL_TRACY_TIMER(TRC_RENDER_SSR_COPY);

	if (!RenderScreenSpaceReflections || gCubeSnapshot)
	{
		return;
	}

	LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);

	dstp->bindTarget();
	dstp->clear();

	LLGLSLShader* shaderp = &gCopyDepthProgram;
	shaderp->bind();
	S32 diff_chan = shaderp->getTextureChannel(LLShaderMgr::DIFFUSE_MAP);
	S32 depth_chan = shaderp->getTextureChannel(LLShaderMgr::DEFERRED_DEPTH);
	gGL.getTexUnit(diff_chan)->bind(srcp);
	gGL.getTexUnit(depth_chan)->bind(&mRT->mDeferredScreen, true);
	mScreenTriangleVB->setBuffer();
	mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

	dstp->flush();
}

// Branched version for the PBR renderer
void LLPipeline::renderFinalizePBR()
{
	LLVertexBuffer::unbind();
	LL_GL_CHECK_STATES;

	LL_FAST_TIMER(FTM_RENDER_BLOOM);

	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	LLGLDepthTest depth(GL_FALSE);
	LLGLDisable blend(GL_BLEND);
	LLGLDisable cull(GL_CULL_FACE);

	enableLightsFullbright();

	gGL.setColorMask(true, true);
	glClearColor(0.f, 0.f, 0.f, 0.f);

	copyScreenSpaceReflections(&mRT->mScreen, &mSceneMap);

	generateLuminance(&mRT->mScreen, &mLuminanceMap);
	generateExposure(&mLuminanceMap, &mExposureMap);
	gammaCorrect(&mRT->mScreen, &mPostMap);

	LLVertexBuffer::unbind();

	generateGlow(&mPostMap);
	combineGlow(&mPostMap, &mRT->mScreen);

	gViewerWindowp->setupViewport();

	renderDoF(&mRT->mScreen, &mPostMap);

	LLRenderTarget* final_targetp = &mRT->mScreen;
	LLRenderTarget* work_targetp = &mPostMap;

#if HB_PBR_SMAA_AND_CAS
	static LLCachedControl<bool> use_smaa(gSavedSettings,
										  "RenderDeferredUseSMAA");
	if (use_smaa)
	{
		applySMAA(work_targetp, final_targetp);

		static LLCachedControl<U32> debug_smaa(gSavedSettings,
											   "RenderDebugSMAA");
		switch (U32(debug_smaa))
		{
			case 0:
			default:
				break;

			case 1:
				final_targetp = &mRT->mFXAABuffer;
				break;

			case 2:
				final_targetp = &mRT->mSMAAEdgeBuffer;
				break;

			case 3:
				final_targetp = &mRT->mSMAABlendBuffer;
				break;
		}
	}
	else
#endif
	{
		applyFXAA(work_targetp, final_targetp);
	}

#if HB_PBR_SMAA_AND_CAS
	if (RenderDeferredAASharpen && LLViewerShaderMgr::sHasCAS)
	{
		LLRenderTarget* tempp = work_targetp;
		work_targetp = final_targetp;
		final_targetp = tempp;
		applyCAS(work_targetp, final_targetp);
	}
#endif

	if (RenderBufferVisualization > -1)
	{
		final_targetp = work_targetp;
		if (RenderBufferVisualization == 4)
		{
			visualizeBuffers(&mLuminanceMap, final_targetp, 0);
		}
		else
		{
			visualizeBuffers(&mRT->mDeferredScreen, final_targetp,
							 RenderBufferVisualization);
		}
	}

	// Present the screen target

	gDeferredPostNoDoFProgram.bind();

	// Whatever is last in the above post processing chain should _always_ be
	// rendered directly here. If not, expect problems.
	gDeferredPostNoDoFProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE,
										  final_targetp);
	gDeferredPostNoDoFProgram.bindTexture(LLShaderMgr::DEFERRED_DEPTH,
										  &mRT->mDeferredScreen, true);
	{
		LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);
		mScreenTriangleVB->setBuffer();
		mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	}
	gDeferredPostNoDoFProgram.unbind();

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	if (hasRenderDebugMask(RENDER_DEBUG_PHYSICS_SHAPES))
	{
		renderPhysicsDisplay();
	}

	LLVertexBuffer::unbind();
	LL_GL_CHECK_STATES;
}

void LLPipeline::renderFinalize()
{
	if (gUsePBRShaders)
	{
		renderFinalizePBR();
		return;
	}

	LLVertexBuffer::unbind();
	LL_GL_CHECK_STATES;

	if (gUseWireframe)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	LLVector2 tc1;
	LLVector2 tc2((F32)(mRT->mScreen.getWidth() * 2),
				  (F32)(mRT->mScreen.getHeight() * 2));

	LL_FAST_TIMER(FTM_RENDER_BLOOM);

	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	LLGLDepthTest depth(GL_FALSE);
	LLGLDisable blend(GL_BLEND);
	LLGLDisable cull(GL_CULL_FACE);

	enableLightsFullbright();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	gGL.setColorMask(true, true);
	glClearColor(0.f, 0.f, 0.f, 0.f);

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (RenderGlow)
	{
		mGlow[2].bindTarget();
		mGlow[2].clear();

		gGlowExtractProgram.bind();
		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MIN_LUMINANCE,
									  RenderGlowMinLuminance);
		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MAX_EXTRACT_ALPHA,
									  RenderGlowMaxExtractAlpha);
		gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_LUM_WEIGHTS,
									  RenderGlowLumWeights.mV[0],
									  RenderGlowLumWeights.mV[1],
									  RenderGlowLumWeights.mV[2]);
		gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_WARMTH_WEIGHTS,
									  RenderGlowWarmthWeights.mV[0],
									  RenderGlowWarmthWeights.mV[1],
									  RenderGlowWarmthWeights.mV[2]);
		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_WARMTH_AMOUNT,
									  RenderGlowWarmthAmount);
		{
			LLGLEnable blend_on(GL_BLEND);

			gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);

			mRT->mScreen.bindTexture(0, 0, LLTexUnit::TFO_POINT);

			gGL.color4f(1.f, 1.f, 1.f, 1.f);

			enableLightsFullbright();

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);

			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);

			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);

			gGL.end();

			unit0->unbind(mRT->mScreen.getUsage());

			mGlow[2].flush();
		}

		tc1.set(0, 0);
		tc2.set(2, 2);

		// Power of two between 1 and 1024
		const U32 glow_res = llclamp(1 << RenderGlowResolutionPow, 1, 1024);

		S32 kernel = RenderGlowIterations * 2;
		F32 delta = RenderGlowWidth / (F32)glow_res;
		// Use half the glow width if we have the res set to less than 9 so
		// that it looks almost the same in either case.
		if (RenderGlowResolutionPow < 9)
		{
			delta *= 0.5f;
		}

		gGlowProgram.bind();
		gGlowProgram.uniform1f(LLShaderMgr::GLOW_STRENGTH, RenderGlowStrength);

		for (S32 i = 0; i < kernel; ++i)
		{
			mGlow[i % 2].bindTarget();
			mGlow[i % 2].clear();

			if (i == 0)
			{
				unit0->bind(&mGlow[2]);
			}
			else
			{
				unit0->bind(&mGlow[(i - 1) % 2]);
			}

			if (i % 2 == 0)
			{
				gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, delta, 0.f);
			}
			else
			{
				gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, 0.f, delta);
			}

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);

			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);

			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);

			gGL.end();

			mGlow[i % 2].flush();
		}

		gGlowProgram.unbind();
	}
	else	// Skip the glow ping-pong and just clear the result target
	{
		mGlow[1].bindTarget();
		mGlow[1].clear();
		mGlow[1].flush();
	}

	gViewerWindowp->setupViewport();

	tc2.set((F32)mRT->mScreen.getWidth(), (F32)mRT->mScreen.getHeight());

	gGL.flush();

	LLVertexBuffer::unbind();

	stop_glerror();

	if (sRenderDeferred)
	{
		bool dof_enabled = RenderDepthOfField &&
						   (RenderDepthOfFieldInEditMode ||
							!gToolMgr.inBuildMode()) &&
						   !gViewerCamera.cameraUnderWater();

		bool multisample = RenderFSAASamples > 1 &&
						   mRT->mFXAABuffer.isComplete();

		if (dof_enabled)
		{
			// Depth of field focal plane calculations
			F32 subject_dist, blur_constant, magnification;
			calc_doff_params(subject_dist, blur_constant, magnification);

			LLGLDisable blend(GL_BLEND);

			// Build diffuse + bloom + CoF
			mRT->mDeferredLight.bindTarget();
			LLGLSLShader* shaderp = &gDeferredCoFProgram;
			bindDeferredShader(*shaderp);
			S32 channel = shaderp->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
												 mRT->mScreen.getUsage());
			if (channel > -1)
			{
				mRT->mScreen.bindTexture(0, channel);
			}
			shaderp->uniform1f(LLShaderMgr::DOF_FOCAL_DISTANCE, -subject_dist);
			shaderp->uniform1f(LLShaderMgr::DOF_BLUR_CONSTANT, blur_constant);
			shaderp->uniform1f(LLShaderMgr::DOF_TAN_PIXEL_ANGLE,
							  tanf(1.f / LLDrawable::sCurPixelAngle));
			shaderp->uniform1f(LLShaderMgr::DOF_MAGNIFICATION, magnification);
			shaderp->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
			shaderp->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);
			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);
			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);
			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);
			gGL.end();
			unbindDeferredShader(*shaderp);
			mRT->mDeferredLight.flush();

			U32 dof_width = U32(mRT->mScreen.getWidth() * CameraDoFResScale);
			U32 dof_height = U32(mRT->mScreen.getHeight() * CameraDoFResScale);

			// Perform DoF sampling at half-res (preserve alpha channel)
			mRT->mScreen.bindTarget();
			glViewport(0, 0, dof_width, dof_height);
			gGL.setColorMask(true, false);
			shaderp = &gDeferredPostProgram;
			bindDeferredShader(*shaderp);
			channel = shaderp->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
											 mRT->mDeferredLight.getUsage());
			if (channel > -1)
			{
				mRT->mDeferredLight.bindTexture(0, channel);
			}
			shaderp->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
			shaderp->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);
			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);
			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);
			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);
			gGL.end();
			unbindDeferredShader(*shaderp);
			mRT->mScreen.flush();
			gGL.setColorMask(true, true);

			// Combine result based on alpha
			if (multisample)
			{
				mRT->mDeferredLight.bindTarget();
				glViewport(0, 0, mRT->mDeferredScreen.getWidth(),
						   mRT->mDeferredScreen.getHeight());
			}
			else
			{
				gViewerWindowp->setupViewport();
			}
			shaderp = &gDeferredDoFCombineProgram;
			bindDeferredShader(*shaderp);
			channel = shaderp->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
											 mRT->mScreen.getUsage());
			if (channel > -1)
			{
				mRT->mScreen.bindTexture(0, channel);
			}
			shaderp->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
			shaderp->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);
			shaderp->uniform1f(LLShaderMgr::DOF_WIDTH, dof_width - 1);
			shaderp->uniform1f(LLShaderMgr::DOF_HEIGHT, dof_height - 1);
			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);
			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);
			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);
			gGL.end();
			unbindDeferredShader(*shaderp);
			if (multisample)
			{
				mRT->mDeferredLight.flush();
			}
		}
		else
		{
			if (multisample)
			{
				mRT->mDeferredLight.bindTarget();
			}
			LLGLSLShader* shaderp = &gDeferredPostNoDoFProgram;

			bindDeferredShader(*shaderp);

			S32 channel = shaderp->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
												 mRT->mScreen.getUsage());
			if (channel > -1)
			{
				mRT->mScreen.bindTexture(0, channel);
			}

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
			gGL.vertex2f(-1.f, -1.f);

			gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
			gGL.vertex2f(-1.f, 3.f);

			gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
			gGL.vertex2f(3.f, -1.f);

			gGL.end();

			unbindDeferredShader(*shaderp);

			if (multisample)
			{
				mRT->mDeferredLight.flush();
			}
		}

		if (multisample)
		{
			static LLCachedControl<bool> use_smaa(gSavedSettings,
												  "RenderDeferredUseSMAA");
			static LLCachedControl<bool> use_stencil(gSavedSettings,
												 	 "RenderDeferredUseSMAAStencil");
			if (use_smaa && LLViewerShaderMgr::sHasSMAA && mAreaMap &&
				mSearchMap && mRT->mSMAAEdgeBuffer.isComplete() &&
				mRT->mSMAABlendBuffer.isComplete() &&
				mRT->mScratchBuffer.isComplete())
			{
				// Note: all buffers got the same size.
				S32 width = mRT->mScreen.getWidth();
				S32 height = mRT->mScreen.getHeight();
				mRT->mFXAABuffer.copyContents(mRT->mDeferredLight, 0, 0, width,
											  height, 0, 0, width, height,
											  GL_COLOR_BUFFER_BIT, GL_NEAREST);

				LLGLDepthTest depth(GL_FALSE, GL_FALSE);
				glClearColor(0.f, 0.f, 0.f, 0.f);

				glViewport(0, 0, width, height);
				F32 rt_metrics[] = { 1.f / width, 1.f / height,
									 (F32)width, (F32)height };

				LLTexUnit* unit1 = gGL.getTexUnit(1);
				LLTexUnit* unit2 = gGL.getTexUnit(2);

				LLGLSLShader* shaderp;
				LLRenderTarget* targetp;

				{
					LLGLState stencil(GL_STENCIL_TEST, use_stencil);

					shaderp = &gPostSMAAEdgeDetect[RenderDeferredAAQuality];
					shaderp->bind();
					shaderp->uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

					mRT->mFXAABuffer.bindTexture(0, 0,
												 LLTexUnit::TFO_BILINEAR);
					unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
					unit0->setTextureColorSpace(LLTexUnit::TCS_LINEAR);

					targetp = &mRT->mSMAAEdgeBuffer;
					targetp->bindTarget();
					targetp->clear(GL_COLOR_BUFFER_BIT |
								   GL_STENCIL_BUFFER_BIT);

					if (use_stencil)
					{
						glStencilFunc(GL_ALWAYS, 1, 0xFF);
						glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
						glStencilMask(0xFF);
					}

					mScreenTriangleVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
					mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

					shaderp->unbind();

					targetp->flush();

					unit0->disable();
				}

				{
					LLGLState stencil(GL_STENCIL_TEST, use_stencil);

					shaderp = &gPostSMAABlendWeights[RenderDeferredAAQuality];
					shaderp->bind();
					shaderp->uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

					mRT->mSMAAEdgeBuffer.bindTexture(0, 0,
													 LLTexUnit::TFO_BILINEAR);
					unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
					unit0->setTextureColorSpace(LLTexUnit::TCS_LINEAR);
					unit1->bindManual(LLTexUnit::TT_TEXTURE, mAreaMap);
					unit1->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
					unit1->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
					unit1->setTextureColorSpace(LLTexUnit::TCS_LINEAR);
					unit2->bindManual(LLTexUnit::TT_TEXTURE, mSearchMap);
					unit2->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
					unit2->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
					unit2->setTextureColorSpace(LLTexUnit::TCS_LINEAR);

					targetp = &mRT->mSMAABlendBuffer;
					targetp->bindTarget();
					targetp->clear(GL_COLOR_BUFFER_BIT);

					if (use_stencil)
					{
						glStencilFunc(GL_EQUAL, 1, 0xFF);
						glStencilMask(0x00);
					}
					mScreenTriangleVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
					mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
					if (use_stencil)
					{
						glStencilFunc(GL_ALWAYS, 0, 0xFF);
					}

					shaderp->unbind();

					targetp->flush();

					unit0->disable();
					unit1->disable();
					unit2->disable();
				}

				{
					LLGLDisable stencil(GL_STENCIL_TEST);

					shaderp =
						&gPostSMAANeighborhoodBlend[RenderDeferredAAQuality];
					shaderp->bind();
					shaderp->uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

					mRT->mFXAABuffer.bindTexture(0, 0, LLTexUnit::TFO_BILINEAR);
					unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
					unit0->setTextureColorSpace(LLTexUnit::TCS_LINEAR);
					mRT->mSMAABlendBuffer.bindTexture(0, 1,
													  LLTexUnit::TFO_BILINEAR);
					unit1->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
					unit1->setTextureColorSpace(LLTexUnit::TCS_LINEAR);

					if (RenderDeferredAASharpen)
					{
						targetp = &mRT->mScratchBuffer;
						targetp->bindTarget();
						targetp->clear(GL_COLOR_BUFFER_BIT);
					}
					else
					{
						gViewerWindowp->setupViewport();
					}

					mScreenTriangleVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
					mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

					shaderp->unbind();

					if (RenderDeferredAASharpen)
					{
						targetp->flush();
					}

					unit0->disable();
					unit1->disable();
				}
			}
			else if (LLViewerShaderMgr::sHasFXAA)
			{
				// Bake out texture2D with RGBL for FXAA shader
				mRT->mFXAABuffer.bindTarget();

				S32 width = mRT->mScreen.getWidth();
				S32 height = mRT->mScreen.getHeight();
				glViewport(0, 0, width, height);

				LLGLSLShader* shaderp = &gGlowCombineFXAAProgram;

				shaderp->bind();
				shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, width,
								   height);

				S32 channel =
					shaderp->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
										   mRT->mDeferredLight.getUsage());
				if (channel > -1)
				{
					mRT->mDeferredLight.bindTexture(0, channel);
				}

				mScreenTriangleVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

				shaderp->disableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
									    mRT->mDeferredLight.getUsage());
				shaderp->unbind();

				mRT->mFXAABuffer.flush();

				shaderp = &gFXAAProgram[RenderDeferredAAQuality];
				shaderp->bind();

				channel = shaderp->enableTexture(LLShaderMgr::DIFFUSE_MAP,
												 mRT->mFXAABuffer.getUsage());
				if (channel > -1)
				{
					mRT->mFXAABuffer.bindTexture(0, channel,
												 LLTexUnit::TFO_BILINEAR);
				}

				if (RenderDeferredAASharpen)
				{
					mRT->mScratchBuffer.bindTarget();
					glClearColor(0.f, 0.f, 0.f, 0.f);
					mRT->mScratchBuffer.clear(GL_COLOR_BUFFER_BIT);
				}
				else
				{
					gViewerWindowp->setupViewport();
				}

				F32 inv_width = 1.f / F32(mRT->mFXAABuffer.getWidth());
				F32 inv_height = 1.f / F32(mRT->mFXAABuffer.getHeight());
				F32 scale_x = F32(width) * inv_width;
				F32 scale_y = F32(height) * inv_height;
				shaderp->uniform2f(LLShaderMgr::FXAA_TC_SCALE, scale_x, scale_y);
				shaderp->uniform2f(LLShaderMgr::FXAA_RCP_SCREEN_RES, inv_width,
								   inv_height);
				shaderp->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT,
								   -0.5f * inv_width, -0.5f * inv_height,
								   0.5f * inv_width, 0.5f * inv_height);
				shaderp->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT2,
								   -2.f * inv_width, -2.f * inv_height,
								   2.f * inv_width, 2.f * inv_height);

				mScreenTriangleVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

				shaderp->unbind();

				if (RenderDeferredAASharpen)
				{
					mRT->mScratchBuffer.flush();
				}

				unit0->disable();
			}

			if (RenderDeferredAASharpen && LLViewerShaderMgr::sHasCAS)
			{
				LLGLDepthTest depth(GL_FALSE, GL_FALSE);

				gPostCASProgram.bind();
				static LLCachedControl<LLVector3>
					cas_params(gSavedSettings, "RenderDeferredCASParams");
				LLVector3 params = LLVector3(cas_params);
				params.clamp(0.f, 1.f);
				gPostCASProgram.uniform3fv(sSharpness, 1, params.mV);

				mRT->mScratchBuffer.bindTexture(0, 0, LLTexUnit::TFO_BILINEAR);
				unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
				unit0->setTextureColorSpace(LLTexUnit::TCS_LINEAR);

				gViewerWindowp->setupViewport();

				mScreenTriangleVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

				gPostCASProgram.unbind();

				unit0->disable();
			}
		}
	}
	else
	{
		LLStrider<LLVector2> uv2;
		if (mGlowCombineVB.isNull() ||
			!mGlowCombineVB->getTexCoord1Strider(uv2))
		{
			return;
		}
		uv2[0].clear();
		uv2[1] = LLVector2(0.f, tc2.mV[1] * 2.f);
		uv2[2] = LLVector2(tc2.mV[0] * 2.f, 0.f);

		LLGLDisable blend(GL_BLEND);

		LLTexUnit* unit1 = gGL.getTexUnit(1);

		gGlowCombineProgram.bind();

		unit0->bind(&mGlow[1]);
		unit1->bind(&mRT->mScreen);

		LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE : 0);

		mGlowCombineVB->setBuffer(AUX_VB_MASK);
		mGlowCombineVB->drawArrays(LLRender::TRIANGLE_STRIP, 0, 3);

		gGlowCombineProgram.unbind();
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	if (hasRenderDebugMask(RENDER_DEBUG_PHYSICS_SHAPES))
	{
		gSplatTextureRectProgram.bind();

		gGL.setColorMask(true, false);

		LLVector2 tc1;
		LLVector2 tc2((F32)gViewerWindowp->getWindowDisplayWidth() * 2,
					  (F32)gViewerWindowp->getWindowDisplayHeight() * 2);

		LLGLEnable blend(GL_BLEND);
		gGL.color4f(1.f, 1.f, 1.f, 0.75f);

		unit0->bind(&mPhysicsDisplay);

		gGL.begin(LLRender::TRIANGLES);
		gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
		gGL.vertex2f(-1.f, -1.f);

		gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
		gGL.vertex2f(-1.f, 3.f);

		gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
		gGL.vertex2f(3.f, -1.f);

		gGL.end(true);

		gSplatTextureRectProgram.unbind();
	}

	if (LLRenderTarget::sUseFBO && mRT->mScreen.getFBO())
	{
		// Copy depth buffer from mScreen to framebuffer
		LLRenderTarget::copyContentsToFramebuffer(mRT->mScreen, 0, 0,
												  mRT->mScreen.getWidth(),
												  mRT->mScreen.getHeight(),
												  0, 0,
												  mRT->mScreen.getWidth(),
												  mRT->mScreen.getHeight(),
												  GL_DEPTH_BUFFER_BIT,
												  GL_NEAREST);
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	LLVertexBuffer::unbind();

	LL_GL_CHECK_STATES;
}

void LLPipeline::bindLightFunc(LLGLSLShader& shader)
{
	S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);
	if (channel > -1)
	{
		gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc);
	}
	if (gUsePBRShaders)
	{
		channel = shader.enableTexture(LLShaderMgr::DEFERRED_BRDF_LUT,
									   LLTexUnit::TT_TEXTURE);
		if (channel > -1)
		{
			mPbrBrdfLut.bindTexture(0, channel);
		}
	}
	stop_glerror();
}

void LLPipeline::bindShadowMaps(LLGLSLShader& shader)
{
	if (gUsePBRShaders)
	{
		for (U32 i = 0; i < 4; ++i)
		{
			LLRenderTarget* shadow_targetp = &mRT->mSunShadow[i];
			if (!shadow_targetp) continue;

			S32 chan = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i,
											LLTexUnit::TT_TEXTURE);
			if (chan > -1)
			{
				gGL.getTexUnit(chan)->bind(shadow_targetp, true);
			}
		}
		for (U32 i = 4; i < 6; ++i)
		{
			S32 chan = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i);
			if (chan > -1)
			{
				LLRenderTarget* shadow_targetp = &mSpotShadow[i - 4];
				if (shadow_targetp)
				{
					gGL.getTexUnit(chan)->bind(shadow_targetp, true);
				}
			}
		}
		stop_glerror();
		return;
	}

	for (U32 i = 0; i < 4; ++i)
	{
		LLRenderTarget* shadow_targetp = &mShadow[i];
		if (!shadow_targetp) continue;

		S32 chan = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i,
										LLTexUnit::TT_TEXTURE);
		if (chan <= -1) continue;

		LLTexUnit* unitp = gGL.getTexUnit(chan);
		unitp->bind(shadow_targetp, true);
		unitp->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		unitp->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
						GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	}
	for (U32 i = 4; i < 6; ++i)
	{
		S32 chan = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i);
		if (chan <= -1) continue;

		LLRenderTarget* shadow_targetp = &mShadow[i];
		if (!shadow_targetp) continue;

		LLTexUnit* unitp = gGL.getTexUnit(chan);
		unitp->bind(shadow_targetp);
		unitp->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		unitp->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
						GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	}
	stop_glerror();
}

void LLPipeline::setEnvMat(LLGLSLShader& shader)
{
	F32* m = gGLModelView.getF32ptr();
	F32 mat[] =
	{
		m[0], m[1], m[2],
		m[4], m[5], m[6],
		m[8], m[9], m[10]
	};
	shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_ENV_MAT, 1, GL_TRUE, mat);
}

void LLPipeline::bindReflectionProbes(LLGLSLShader& shader)
{
	if (!sReflectionProbesEnabled)
	{
		return;
	}

	bool bound = false;

	S32 chan = shader.enableTexture(LLShaderMgr::REFLECTION_PROBES,
									LLTexUnit::TT_CUBE_MAP_ARRAY);
	if (chan > -1 && mReflectionMapManager.mTexture.notNull())
	{
		mReflectionMapManager.mTexture->bind(chan);
		bound = true;
	}

	chan = shader.enableTexture(LLShaderMgr::IRRADIANCE_PROBES,
								LLTexUnit::TT_CUBE_MAP_ARRAY);
	if (chan > -1 && mReflectionMapManager.mIrradianceMaps.notNull())
	{
		mReflectionMapManager.mIrradianceMaps->bind(chan);
		bound = true;
	}

	if (bound)
	{
		mReflectionMapManager.setUniforms();
		setEnvMat(shader);
	}

	// Reflection probe shaders generally sample the scene map as well for SSR
	chan = shader.enableTexture(LLShaderMgr::SCENE_MAP);
	if (chan > -1)
	{
		gGL.getTexUnit(chan)->bind(&mSceneMap);
	}

	shader.uniform1f(LLShaderMgr::DEFERRED_SSR_ITR_COUNT,
					 RenderScreenSpaceReflectionIterations);
	shader.uniform1f(LLShaderMgr::DEFERRED_SSR_DIST_BIAS,
					 RenderScreenSpaceReflectionDistanceBias);
	shader.uniform1f(LLShaderMgr::DEFERRED_SSR_RAY_STEP,
					 RenderScreenSpaceReflectionRayStep);
	shader.uniform1f(LLShaderMgr::DEFERRED_SSR_GLOSSY_SAMPLES,
					 RenderScreenSpaceReflectionGlossySamples);
	shader.uniform1f(LLShaderMgr::DEFERRED_SSR_REJECT_BIAS,
					 RenderScreenSpaceReflectionDepthRejectBias);
	if (++mPoissonOffset + RenderScreenSpaceReflectionGlossySamples > 128)
	{
		mPoissonOffset = 0;
	}
	shader.uniform1f(LLShaderMgr::DEFERRED_SSR_NOISE_SINE,
					 F32(mPoissonOffset));
	shader.uniform1f(LLShaderMgr::DEFERRED_SSR_ADAPTIVE_STEP_MULT,
					 RenderScreenSpaceReflectionAdaptiveStepMultiplier);

	chan = shader.enableTexture(LLShaderMgr::SCENE_DEPTH);
	if (chan > -1)
	{
		gGL.getTexUnit(chan)->bind(&mSceneMap, true);
	}
}

void LLPipeline::unbindReflectionProbes(LLGLSLShader& shader)
{
	S32 chan = shader.disableTexture(LLShaderMgr::REFLECTION_PROBES,
									 LLTexUnit::TT_CUBE_MAP);
	if (chan > -1 && mReflectionMapManager.mTexture.notNull())
	{
		mReflectionMapManager.mTexture->unbind();
		if (chan == 0)
		{
			gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
		}
	}
}

void LLPipeline::bindDeferredShaderFast(LLGLSLShader& shader)
{
	// Note: what happens if a shader is flagged mCanBindFast with mMainRT,
	// then reused just after with mAuxillaryRT ?... The wrong render target
	// would be bound and used. I therefore added the is_main_rt tests. HB
	bool is_main_rt = mRT == &mMainRT;
	if (is_main_rt && shader.mCanBindFast)
	{
		shader.bind();
		bindLightFunc(shader);
		bindShadowMaps(shader);
		if (gUsePBRShaders)
		{
			bindReflectionProbes(shader);
		}
	}
	else
	{
		// Was not previously bound, use slow path
		bindDeferredShader(shader);
		shader.mCanBindFast = is_main_rt;
	}
}

void LLPipeline::bindDeferredShader(LLGLSLShader& shader,
									LLRenderTarget* light_targetp)
{
	LL_FAST_TIMER(FTM_BIND_DEFERRED);

	LLRenderTarget* deferred_targetp = &mRT->mDeferredScreen;
	// Note: the EE renderer uses a different buffer for depth target.
	LLRenderTarget* depth_targetp = gUsePBRShaders ? deferred_targetp
												   : &mDeferredDepth;

	shader.bind();

	LLTexUnit::eTextureType usage = deferred_targetp->getUsage();
	S32 chan = shader.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, usage);
	if (chan > -1)
	{
		deferred_targetp->bindTexture(0, chan, LLTexUnit::TFO_POINT);
		if (gUsePBRShaders)
		{
			gGL.getTexUnit(chan)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
		}
	}

	chan = shader.enableTexture(LLShaderMgr::DEFERRED_SPECULAR, usage);
	if (chan > -1)
	{
		deferred_targetp->bindTexture(1, chan, LLTexUnit::TFO_POINT);
		if (gUsePBRShaders)
		{
			gGL.getTexUnit(chan)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
		}
	}

	chan = shader.enableTexture(LLShaderMgr::DEFERRED_NORMAL, usage);
	if (chan > -1)
	{
		deferred_targetp->bindTexture(2, chan, LLTexUnit::TFO_POINT);
		if (gUsePBRShaders)
		{
			gGL.getTexUnit(chan)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
		}
	}

	if (gUsePBRShaders)
	{
		chan = shader.enableTexture(LLShaderMgr::DEFERRED_EMISSIVE, usage);
		if (chan > -1)
		{
			deferred_targetp->bindTexture(3, chan, LLTexUnit::TFO_POINT);
			gGL.getTexUnit(chan)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
		}
	}

	chan = shader.enableTexture(LLShaderMgr::DEFERRED_DEPTH,
								depth_targetp->getUsage());
	if (chan > -1)
	{

		gGL.getTexUnit(chan)->bind(depth_targetp, true);
		stop_glerror();
	}

	if (gUsePBRShaders)
	{
		chan = shader.enableTexture(LLShaderMgr::EXPOSURE_MAP);
		if (chan > -1)
		{
			gGL.getTexUnit(chan)->bind(&mExposureMap);
		}
	}
	else if (shader.getUniformLocation(LLShaderMgr::INVERSE_PROJECTION_MATRIX) != -1)
	{
		// This is done in LLRender::syncMatrices() for PBR shaders, but would
		// not be enough for EE rendering, due to a bug involving the deferred
		// renderer regular pushing and popping of matrices.
		LLMatrix4a inv_proj = gGLProjection;
		inv_proj.invert();
		shader.uniformMatrix4fv(LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1,
								GL_FALSE, inv_proj.getF32ptr());
	}

	if (shader.getUniformLocation(LLShaderMgr::VIEWPORT) != -1)
	{
		shader.uniform4f(LLShaderMgr::VIEWPORT, (F32)gGLViewport[0],
						 (F32)gGLViewport[1], (F32)gGLViewport[2],
						 (F32)gGLViewport[3]);
	}

	// Note: mReflectionModelView is never calculated for PBR rendering, and
	// sReflectionRender is normally never true, but I added !gUsePBRShaders
	// for good measure. HB
	if (!gUsePBRShaders && sReflectionRender &&
		shader.getUniformLocation(LLShaderMgr::MODELVIEW_MATRIX) != -1)
	{
		shader.uniformMatrix4fv(LLShaderMgr::MODELVIEW_MATRIX, 1, GL_FALSE,
								mReflectionModelView.getF32ptr());
	}

	chan = shader.enableTexture(LLShaderMgr::DEFERRED_NOISE);
	if (chan > -1)
	{
		LLTexUnit* unitp = gGL.getTexUnit(chan);
		unitp->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap);
		unitp->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	bindLightFunc(shader);

	if (!light_targetp)
	{
		light_targetp = &mRT->mDeferredLight;
	}
	chan = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHT,
								light_targetp->getUsage());
	if (chan > -1)
	{
		if (!gUsePBRShaders || light_targetp->isComplete())
		{
			light_targetp->bindTexture(0, chan, LLTexUnit::TFO_POINT);
		}
		else
		{
			gGL.getTexUnit(chan)->bindFast(LLViewerFetchedTexture::sWhiteImagep);
		}
	}

	if (!gUsePBRShaders)
	{
		chan = shader.enableTexture(LLShaderMgr::DEFERRED_BLOOM);
		if (chan > -1)
		{
			mGlow[1].bindTexture(0, chan);
		}
	}

	stop_glerror();

#if OPTIMIZED_UNIFORMS
	if (shader.mFeatures.hasShadows)
#endif
	{
		bindShadowMaps(shader);

		F32 mat[16 * 6];
		constexpr size_t total_bytes = sizeof(F32) * 16 * 6;
		memcpy((void*)mat, (void*)mSunShadowMatrix, total_bytes);
		shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_SHADOW_MATRIX, 6,
								GL_FALSE, mat);

		shader.uniform4fv(LLShaderMgr::DEFERRED_SHADOW_CLIP, 1,
						  mSunClipPlanes.mV);
		if (gUsePBRShaders)
		{
			shader.uniform2f(LLShaderMgr::DEFERRED_SHADOW_RES,
							 mRT->mSunShadow[0].getWidth(),
							 mRT->mSunShadow[0].getHeight());
			shader.uniform2f(LLShaderMgr::DEFERRED_PROJ_SHADOW_RES,
							 mSpotShadow[0].getWidth(),
							 mSpotShadow[0].getHeight());
		}
		else
		{
			shader.uniform2f(LLShaderMgr::DEFERRED_SHADOW_RES,
							 mShadow[0].getWidth(),
							 mShadow[0].getHeight());
			shader.uniform2f(LLShaderMgr::DEFERRED_PROJ_SHADOW_RES,
							 mShadow[4].getWidth(),
							 mShadow[4].getHeight());
		}

		shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_OFFSET,
						 RenderDeferredSSAO ? RenderShadowOffset
											: RenderShadowOffsetNoSSAO);

		constexpr F32 ONEBYTHREETHOUSANDS = 1.f / 3000.f;
		F32 shadow_bias_error = RenderShadowBiasError * ONEBYTHREETHOUSANDS *
								fabsf(gViewerCamera.getOrigin().mV[2]);
		shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_BIAS,
						 RenderShadowBias + shadow_bias_error);

		shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_OFFSET,
						 RenderSpotShadowOffset);
		shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_BIAS,
						 RenderSpotShadowBias);
	}
#if DEBUG_OPTIMIZED_UNIFORMS && OPTIMIZED_UNIFORMS
	else if (shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW0) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_MATRIX) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_CLIP) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_RES) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_OFFSET) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_BIAS) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SPOT_SHADOW_OFFSET) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SPOT_SHADOW_BIAS) >= 0)
	{
		llwarns_once << "Shader: " << shader.mName
					 << " shall be marked as hasShadows !" << llendl;
	}
#endif

	if (!sReflectionProbesEnabled)
	{
		chan = shader.enableTexture(LLShaderMgr::ENVIRONMENT_MAP,
									LLTexUnit::TT_CUBE_MAP);
		if (chan > -1)
		{
			LLCubeMap* cube_mapp = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap()
												: NULL;
			if (cube_mapp)
			{
				cube_mapp->enableTexture(chan);
				cube_mapp->bind();
				const F32* m = gGLModelView.getF32ptr();

				F32 mat[] = { m[0], m[1], m[2],
							  m[4], m[5], m[6],
							  m[8], m[9], m[10] };

				shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_ENV_MAT, 1,
										GL_TRUE, mat);
			}
		}
	}

	if (gUsePBRShaders)
	{
		bindReflectionProbes(shader);
		shader.uniformMatrix4fv(LLShaderMgr::MODELVIEW_DELTA_MATRIX, 1,
								GL_FALSE, gGLDeltaModelView.getF32ptr());
		shader.uniformMatrix4fv(LLShaderMgr::INVERSE_MODELVIEW_DELTA_MATRIX, 1,
								GL_FALSE,
								gGLInverseDeltaModelView.getF32ptr());
		shader.uniform1i(LLShaderMgr::CUBE_SNAPSHOT, gCubeSnapshot ? 1 : 0);
	}

	shader.uniform1f(LLShaderMgr::DEFERRED_SUN_WASH, RenderDeferredSunWash);
	shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_NOISE, RenderShadowNoise);
	shader.uniform1f(LLShaderMgr::DEFERRED_BLUR_SIZE, RenderShadowBlurSize);

#if OPTIMIZED_UNIFORMS
	if (shader.mFeatures.hasAmbientOcclusion)
#endif
	{
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_RADIUS, RenderSSAOScale);
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_MAX_RADIUS,
						 RenderSSAOMaxScale);
		shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_FACTOR, RenderSSAOFactor);
	}
#if DEBUG_OPTIMIZED_UNIFORMS && OPTIMIZED_UNIFORMS
	else if (shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_RADIUS) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_MAX_RADIUS) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::DEFERRED_SSAO_FACTOR) >= 0)
	{
		llwarns_once << "Shader: " << shader.mName
					 << " shall be marked as hasAmbientOcclusion !" << llendl;
	}
#endif

	constexpr F32 ONETHIRD = 1.f / 3.f;
	F32 matrix_diag = (RenderSSAOEffect[0] + 2.f * RenderSSAOEffect[1]) *
					  ONETHIRD;
	F32 matrix_nondiag = (RenderSSAOEffect[0] - RenderSSAOEffect[1]) *
						 ONETHIRD;
	// This matrix scales (proj of color onto <1/rt(3),1/rt(3),1/rt(3)>) by
	// value factor, and scales remainder by saturation factor
	F32 ssao_effect_mat[] =
	{
		matrix_diag, matrix_nondiag, matrix_nondiag,
		matrix_nondiag, matrix_diag, matrix_nondiag,
		matrix_nondiag, matrix_nondiag, matrix_diag
	};
	shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_SSAO_EFFECT_MAT, 1, GL_FALSE,
							ssao_effect_mat);

	shader.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
					 deferred_targetp->getWidth(),
					 deferred_targetp->getHeight());
	shader.uniform1f(LLShaderMgr::DEFERRED_NEAR_CLIP,
					 gViewerCamera.getNear() * 2.f);

	shader.uniform3fv(LLShaderMgr::DEFERRED_SUN_DIR, 1,
					  mTransformedSunDir.getF32ptr());
	shader.uniform3fv(LLShaderMgr::DEFERRED_MOON_DIR, 1,
					  mTransformedMoonDir.getF32ptr());

	if (shader.getUniformLocation(LLShaderMgr::DEFERRED_NORM_MATRIX) > -1)
	{
		LLMatrix4a norm_mat = gGLModelView;
		norm_mat.invert();
		norm_mat.transpose();
		shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_NORM_MATRIX, 1, GL_FALSE,
								norm_mat.getF32ptr());
	}

	if (gUsePBRShaders)
	{
		shader.uniform3fv(LLShaderMgr::SUNLIGHT_COLOR, 1, mSunDiffuse.mV);
		shader.uniform3fv(LLShaderMgr::MOONLIGHT_COLOR, 1, mMoonDiffuse.mV);
		shader.uniform1f(LLShaderMgr::REFLECTION_PROBE_MAX_LOD,
						 mReflectionMapManager.mMaxProbeLOD);
	}
	else
	{
		shader.uniform4fv(LLShaderMgr::SUNLIGHT_COLOR, 1, mSunDiffuse.mV);
		shader.uniform4fv(LLShaderMgr::MOONLIGHT_COLOR, 1, mMoonDiffuse.mV);
	}

	gEnvironment.updateShaderSkyUniforms(&shader);
}

// Branched version for the PBR renderer
void LLPipeline::renderDeferredLightingPBR()
{
	F32 light_scale = gCubeSnapshot ? mReflectionMapManager.mLightScale : 1.f;

	LLRenderTarget* screen_targetp = &mRT->mScreen;
	LLRenderTarget* light_targetp = &mRT->mDeferredLight;
	LLGLSLShader* shaderp;

	{
		LL_FAST_TIMER(FTM_RENDER_DEFERRED);

		if (hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{
			toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		}

		gGL.setColorMask(true, true);

		// Draw a cube around every light

		LLVertexBuffer::unbind();

		LLGLEnable cull(GL_CULL_FACE);
		LLGLEnable blend(GL_BLEND);

		setupHWLights();
		mTransformedSunDir.loadua(mSunDir.mV);
		gGLModelView.rotate(mTransformedSunDir, mTransformedSunDir);
		mTransformedMoonDir.loadua(mMoonDir.mV);
		gGLModelView.rotate(mTransformedMoonDir, mTransformedMoonDir);

		if (RenderDeferredSSAO || RenderShadowDetail > 0)
		{
			// Paint shadow/SSAO light map (direct lighting lightmap)
			LL_FAST_TIMER(FTM_SUN_SHADOW);

			light_targetp->bindTarget();
			shaderp = &gDeferredSunProgram;

			bindDeferredShader(*shaderp, light_targetp);
			mScreenTriangleVB->setBuffer();
			glClearColor(1.f, 1.f, 1.f, 1.f);
			light_targetp->clear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.f, 0.f, 0.f, 0.f);

#if 0		// The "offset" uniform does not appear to be used... HB
			static LLStaticHashedString sOffset("offset");
			LLMatrix4a inv_trans = gGLModelView;
			inv_trans.invert();
			inv_trans.transpose();
			constexpr U32 slices = 32;
			F32 offset[slices * 3];
			constexpr F32 indice_to_rad = F_PI / 4.f;
			for (U32 i = 0; i < 4; ++i)
			{
				F32 z = -i;
				for (U32 j = 0; j < 8; ++j)
				{
					F32 angle = indice_to_rad * j;
					LLVector4a v(sinf(angle), cosf(angle), z);
					inv_trans.affineTransform(v, v);
					v.normalize3();
					U32 idx = 3 * (8 * i + j);
					offset[idx++] = v[0];
					offset[idx++] = v[2];
					offset[idx] = v[1];
				}
			}
			shaderp->uniform3fv(sOffset, slices, offset);
#endif

			shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
							   light_targetp->getWidth(),
							   light_targetp->getHeight());
			{
				LLGLDisable blend(GL_BLEND);
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
			}
			unbindDeferredShader(*shaderp);
			light_targetp->flush();
		}

		if (RenderDeferredSSAO)
		{
			// Soften direct lighting lightmap
			LL_FAST_TIMER(FTM_SOFTEN_SHADOW);

			// Blur lightmap
			screen_targetp->bindTarget();
			glClearColor(1.f, 1.f, 1.f, 1.f);
			screen_targetp->clear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.f, 0.f, 0.f, 0.f);

			shaderp = &gDeferredBlurLightProgram;
			bindDeferredShader(*shaderp);

			// Sample symmetrically with the middle sample falling exactly on
			// 0.0
			F32 x = 0.f;
			F32 shadow_gaussian_x = RenderShadowGaussian.mV[0];
			F32 shadow_gaussian_y = RenderShadowGaussian.mV[1];
			constexpr U32 kern_length = 4;
			LLVector3 gauss[kern_length]; // xweight, yweight, offset
			for (U32 i = 0; i < kern_length; ++i)
			{
				gauss[i].mV[0] = llgaussian(x, shadow_gaussian_x);
				gauss[i].mV[1] = llgaussian(x, shadow_gaussian_y);
				gauss[i].mV[2] = x;
				x += 1.f;
			}

			shaderp->uniform2f(sDelta, 1.f, 0.f);
			shaderp->uniform1f(sDistFactor, RenderShadowBlurDistFactor);
			shaderp->uniform3fv(sKern, kern_length, gauss[0].mV);
			shaderp->uniform1f(sKernScale,
							   RenderShadowBlurSize *
							   (kern_length * 0.5f - 0.5f));
			{
				LLGLDisable blend(GL_BLEND);
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				mScreenTriangleVB->setBuffer();
				mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
			}

			screen_targetp->flush();
			unbindDeferredShader(*shaderp);

			bindDeferredShader(*shaderp, screen_targetp);

			light_targetp->bindTarget();

			shaderp->uniform2f(sDelta, 0.f, 1.f);

			{
				LLGLDisable blend(GL_BLEND);
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				mScreenTriangleVB->setBuffer();
				mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
			}
			light_targetp->flush();
			unbindDeferredShader(*shaderp);
		}

		screen_targetp->bindTarget();
		// Clear color buffer here; zeroing alpha (glow) is important or it
		// will accumulate against sky
		glClearColor(0.f, 0.f, 0.f, 0.f);
		screen_targetp->clear(GL_COLOR_BUFFER_BIT);

		if (RenderDeferredAtmospheric)
		{
			// Apply sunlight contribution
			LL_FAST_TIMER(FTM_ATMOSPHERICS);

			shaderp = &gDeferredSoftenProgram;
			bindDeferredShader(*shaderp);

			static LLCachedControl<F32> ssao_scale(gSavedSettings,
												   "RenderSSAOIrradianceScale");
			static LLCachedControl<F32> ssao_max(gSavedSettings,
												 "RenderSSAOIrradianceMax");
			shaderp->uniform1f(sIrradianceScale, ssao_scale);
			shaderp->uniform1f(sIrradianceMax, ssao_max);

			shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, mIsSunUp ? 1 : 0);
			shaderp->uniform3fv(LLShaderMgr::LIGHTNORM, 1,
								gEnvironment.getClampedLightNorm().mV);
			shaderp->uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1,
								sWaterPlane.getF32ptr());

			{
				LLGLDepthTest depth(GL_FALSE);
				LLGLDisable blend(GL_BLEND);
				// Full screen blit
				mScreenTriangleVB->setBuffer();
				mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
			}

			unbindDeferredShader(*shaderp);
		}

		if (RenderLocalLightCount && mCubeVB.notNull())
		{
			gGL.setSceneBlendType(LLRender::BT_ADD);
			std::list<LLVector4> fullscreen_lights, light_colors;
			LLDrawable::draw_list_t spot_lights, fullscreen_spot_lights;

			if (!gCubeSnapshot)
			{
				for (U32 i = 0; i < 2; ++i)
				{
					mTargetShadowSpotLight[i] = NULL;
				}
			}

			LLVertexBuffer::unbind();

			{
				shaderp = &gDeferredLightProgram;
				bindDeferredShader(*shaderp);

				mCubeVB->setBuffer();

				const LLVector3& cam_origin = gViewerCamera.getOrigin();
				F32 cam_x = cam_origin.mV[0];
				F32 cam_y = cam_origin.mV[1];
				F32 cam_z = cam_origin.mV[2];
				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				LLVector4a center, sa;
				LLColor3 col;
				U32 rendered_count = 0;
				// mNearbyLights already includes distance calculation and
				// excludes muted avatars. It is calculated from mLights and
				// also provides fade value to gracefully fade-out out of range
				// lights
				for (light_set_t::iterator iter = mNearbyLights.begin(),
										   end = mNearbyLights.end();
					 iter != end; ++iter)
				{
					if (++rendered_count > RenderLocalLightCount)
					{
						break;
					}

					LLDrawable* drawablep = iter->drawable;

					LLVOVolume* volp = drawablep->getVOVolume();
					if (!volp)
					{
						continue;
					}

					bool is_attachment = volp->isAttachment();
					if (is_attachment && !sRenderAttachedLights)
					{
						continue;
					}

					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volp->getLightRadius() * 1.5f;
					if (s <= 0.001f)
					{
						continue;
					}

					// Send light color to shader in linear space
					col = volp->getLightLinearColor() * light_scale;
					if (col.lengthSquared() < 0.001f)
					{
						continue;
					}

					sa.splat(s);
					if (gViewerCamera.AABBInFrustumNoFarClip(center, sa) == 0)
					{
						continue;
					}

					++sVisibleLightCount;

					if (cam_x > c[0] + s + 0.2f || cam_x < c[0] - s - 0.2f ||
						cam_y > c[1] + s + 0.2f || cam_y < c[1] - s - 0.2f ||
						cam_z > c[2] + s + 0.2f || cam_z < c[2] - s - 0.2f)
					{
						// Draw box if camera is outside box
						if (RenderLocalLightCount)
						{
							if (volp->isLightSpotlight())
							{
								drawablep->getVOVolume()->updateSpotLightPriority();
								spot_lights.emplace_back(drawablep);
								continue;
							}

							LL_FAST_TIMER(FTM_LOCAL_LIGHTS);
							shaderp->uniform3fv(LLShaderMgr::LIGHT_CENTER, 1,
												c);
							shaderp->uniform1f(LLShaderMgr::LIGHT_SIZE, s);
							shaderp->uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1,
												col.mV);
							shaderp->uniform1f(LLShaderMgr::LIGHT_FALLOFF,
											   volp->getLightFalloff(0.5f));
							gGL.syncMatrices();

							mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8,
											   get_box_fan_indices(&gViewerCamera,
																   center));
						}
					}
					else
					{
						if (volp->isLightSpotlight())
						{
							drawablep->getVOVolume()->updateSpotLightPriority();
							fullscreen_spot_lights.emplace_back(drawablep);
							continue;
						}

						gGLModelView.affineTransform(center, center);
						LLVector4 tc(center.getF32ptr());
						tc.mV[VW] = s;
						fullscreen_lights.emplace_back(tc);

						light_colors.emplace_back(col.mV[0], col.mV[1],
												  col.mV[2],
												  volp->getLightFalloff(0.5f));
					}
				}

				// When editting appearance, we need to add the corresponding
				// light at the camera position.
				static LLCachedControl<bool> custlight(gSavedSettings,
													   "AvatarCustomizeLighting");
				if (custlight && isAgentAvatarValid() &&
					gAgentAvatarp->mSpecialRenderMode == 3)
				{
					fullscreen_lights.emplace_back(0.f, 0.f, 0.f, 15.f);
					light_colors.emplace_back(1.f, 1.f, 1.f, 0.f);
				}

				unbindDeferredShader(*shaderp);
			}

			if (!spot_lights.empty())
			{
				LL_FAST_TIMER(FTM_PROJECTORS);

				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				shaderp = &gDeferredSpotLightProgram;
				bindDeferredShader(*shaderp);

				mCubeVB->setBuffer();

				shaderp->enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				for (LLDrawable::draw_list_t::iterator
						iter = spot_lights.begin(), end = spot_lights.end();
					 iter != end; ++iter)
				{
					LLDrawable* drawablep = *iter;

					LLVOVolume* volp = drawablep->getVOVolume();

					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volp->getLightRadius() * 1.5f;

					++sVisibleLightCount;

					setupSpotLight(*shaderp, drawablep);

					LLColor3 col = volp->getLightLinearColor() * light_scale;
					shaderp->uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, c);
					shaderp->uniform1f(LLShaderMgr::LIGHT_SIZE, s);
					shaderp->uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
					shaderp->uniform1f(LLShaderMgr::LIGHT_FALLOFF,
									   volp->getLightFalloff(0.5f));
					gGL.syncMatrices();

					mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8,
									   get_box_fan_indices(&gViewerCamera,
														   center));
				}
				shaderp->disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				unbindDeferredShader(*shaderp);
			}

			{
				LLGLDepthTest depth(GL_FALSE);

				constexpr U32 max_count = LL_DEFERRED_MULTI_LIGHT_COUNT;
				LLVector4 light[max_count];
				LLVector4 col[max_count];
				U32 count = 0;
				F32 far_z = 0.f;

				{
					LL_FAST_TIMER(FTM_FULLSCREEN_LIGHTS);
					while (count < max_count && !fullscreen_lights.empty())
					{
						light[count] = fullscreen_lights.front();
						fullscreen_lights.pop_front();
						col[count] = light_colors.front();
						light_colors.pop_front();
						far_z = llmin(light[count].mV[2] - light[count].mV[3],
									  far_z);

						if (++count == max_count || fullscreen_lights.empty())
						{
							U32 idx = count - 1;
							shaderp = &gDeferredMultiLightProgram[idx];
							bindDeferredShader(*shaderp);
							shaderp->uniform1i(LLShaderMgr::MULTI_LIGHT_COUNT,
											   count);
							shaderp->uniform4fv(LLShaderMgr::MULTI_LIGHT,
												count, (F32*)light);
							shaderp->uniform4fv(LLShaderMgr::MULTI_LIGHT_COL,
												count, (F32*)col);
							shaderp->uniform1f(LLShaderMgr::MULTI_LIGHT_FAR_Z,
											   far_z);
							mScreenTriangleVB->setBuffer();
							mScreenTriangleVB->drawArrays(LLRender::TRIANGLES,
														  0, 3);
							unbindDeferredShader(*shaderp);
							far_z = 0.f;
							count = 0;
						}
					}
				}

				shaderp = &gDeferredMultiSpotLightProgram;
				bindDeferredShader(*shaderp);

				shaderp->enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				mScreenTriangleVB->setBuffer();

				{
					LL_FAST_TIMER(FTM_PROJECTORS);
					for (LLDrawable::draw_list_t::iterator
							iter = fullscreen_spot_lights.begin(),
							end = fullscreen_spot_lights.end();
						 iter != end; ++iter)
					{
						LLDrawable* drawablep = *iter;

						LLVOVolume* volp = drawablep->getVOVolume();

						LLVector4a center;
						center.load3(drawablep->getPositionAgent().mV);
						F32 s = volp->getLightRadius() * 1.5f;

						++sVisibleLightCount;

						gGLModelView.affineTransform(center, center);

						setupSpotLight(*shaderp, drawablep);

						LLColor3 col = volp->getLightLinearColor() *
									   light_scale;
						shaderp->uniform3fv(LLShaderMgr::LIGHT_CENTER, 1,
											center.getF32ptr());
						shaderp->uniform1f(LLShaderMgr::LIGHT_SIZE, s);
						shaderp->uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1,
											col.mV);
						shaderp->uniform1f(LLShaderMgr::LIGHT_FALLOFF,
										   volp->getLightFalloff(0.5f));
						mScreenTriangleVB->drawArrays(LLRender::TRIANGLES,
													  0, 3);
					}
				}

				shaderp->disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				unbindDeferredShader(*shaderp);
			}
		}

		gGL.setColorMask(true, true);
	}

	{
		// Render non-deferred geometry (alpha, fullbright, glow)
		LLGLDisable blend(GL_BLEND);

		pushRenderTypeMask();
		andRenderTypeMask(RENDER_TYPE_ALPHA,
						  RENDER_TYPE_ALPHA_PRE_WATER,
						  RENDER_TYPE_ALPHA_POST_WATER,
						  RENDER_TYPE_FULLBRIGHT,
						  RENDER_TYPE_VOLUME,
						  RENDER_TYPE_GLOW,
						  RENDER_TYPE_BUMP,
						  RENDER_TYPE_MAT_PBR,
						  RENDER_TYPE_PASS_SIMPLE,
						  RENDER_TYPE_PASS_ALPHA,
						  RENDER_TYPE_PASS_ALPHA_MASK,
						  RENDER_TYPE_PASS_BUMP,
						  RENDER_TYPE_PASS_POST_BUMP,
						  RENDER_TYPE_PASS_FULLBRIGHT,
						  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
						  RENDER_TYPE_PASS_GLOW,
						  RENDER_TYPE_PASS_PBR_GLOW,
						  RENDER_TYPE_PASS_GRASS,
						  RENDER_TYPE_PASS_SHINY,
						  RENDER_TYPE_PASS_INVISIBLE,
						  RENDER_TYPE_PASS_INVISI_SHINY,
						  RENDER_TYPE_AVATAR,
						  RENDER_TYPE_PUPPET,
						  RENDER_TYPE_ALPHA_MASK,
						  RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_WATER,
						  END_RENDER_TYPES);

		renderGeomPostDeferred(gViewerCamera);
		popRenderTypeMask();
	}

	screen_targetp->flush();

	if (!gCubeSnapshot)
	{
		// This is the end of the 3D scene render, grab a copy of the model
		// view and projection matrix for use in off-by-one-frame effects in
		// the next frame.
		gGLLastModelView = gGLModelView;
		gGLLastProjection = gGLProjection;
	}

	gGL.setColorMask(true, true);
}

void LLPipeline::renderDeferredLighting()
{
	if (!sCull)
	{
		return;
	}

	if (gUsePBRShaders)
	{
		renderDeferredLightingPBR();
		return;
	}

	LLGLSLShader* shaderp;
	{
		LL_FAST_TIMER(FTM_RENDER_DEFERRED);

		{
			LLGLDepthTest depth(GL_TRUE);
			mDeferredDepth.copyContents(mRT->mDeferredScreen, 0, 0,
										mRT->mDeferredScreen.getWidth(),
										mRT->mDeferredScreen.getHeight(),
										0, 0,
										mDeferredDepth.getWidth(),
										mDeferredDepth.getHeight(),
										GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		}

		LLGLEnable multisample(RenderFSAASamples > 0 ? GL_MULTISAMPLE : 0);

		if (hasRenderType(RENDER_TYPE_HUD))
		{
			toggleRenderType(RENDER_TYPE_HUD);
		}

		// ATI does not seem to love actually using the stencil buffer on FBOs
		LLGLDisable stencil(GL_STENCIL_TEST);
#if 0
		glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
#endif
		gGL.setColorMask(true, true);

		// Draw a cube around every light
		LLVertexBuffer::unbind();

		LLGLEnable cull(GL_CULL_FACE);
		LLGLEnable blend(GL_BLEND);

		LLStrider<LLVector3> vert;
		if (mDeferredVB.isNull() || !mDeferredVB->getVertexStrider(vert))
		{
			return;
		}
		vert[0].set(-1.f, 1.f, 0.f);
		vert[1].set(-1.f, -3.f, 0.f);
		vert[2].set(3.f, 1.f, 0.f);

		setupHWLights();	// To set mSunDir/mMoonDir;
		mTransformedSunDir.loadua(mSunDir.mV);
		gGLModelView.rotate(mTransformedSunDir, mTransformedSunDir);
		mTransformedMoonDir.loadua(mMoonDir.mV);
		gGLModelView.rotate(mTransformedMoonDir, mTransformedMoonDir);

		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();

		if (RenderDeferredSSAO || RenderShadowDetail > 0)
		{
			mRT->mDeferredLight.bindTarget();
			{
				// Paint shadow/SSAO light map (direct lighting lightmap)
				LL_FAST_TIMER(FTM_SUN_SHADOW);

				shaderp = &gDeferredSunProgram;
				bindDeferredShader(*shaderp, &mRT->mDeferredLight);
				glClearColor(1.f, 1.f, 1.f, 1.f);
				mRT->mDeferredLight.clear(GL_COLOR_BUFFER_BIT);
				glClearColor(0.f, 0.f, 0.f, 0.f);

				shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
								   mRT->mDeferredLight.getWidth(),
								   mRT->mDeferredLight.getHeight());

				{
					LLGLDisable blend(GL_BLEND);
					LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
					mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
					mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
				}

				unbindDeferredShader(*shaderp);
			}
			mRT->mDeferredLight.flush();
		}

		if (RenderDeferredSSAO)
		{
			// Soften direct lighting lightmap

			LL_FAST_TIMER(FTM_SOFTEN_SHADOW);

			// Blur lightmap
			mRT->mScreen.bindTarget();
			glClearColor(1.f, 1.f, 1.f, 1.f);
			mRT->mScreen.clear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.f, 0.f, 0.f, 0.f);

			shaderp = &gDeferredBlurLightProgram;
			bindDeferredShader(*shaderp);
			mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

			// Sample symmetrically with the middle sample falling exactly on
			// 0.0
			F32 x = 0.f;
			F32 shadow_gaussian_x = RenderShadowGaussian.mV[0];
			F32 shadow_gaussian_y = RenderShadowGaussian.mV[1];
			constexpr U32 kern_length = 4;
			LLVector3 gauss[kern_length]; // xweight, yweight, offset
			for (U32 i = 0; i < kern_length; ++i)
			{
				gauss[i].mV[0] = llgaussian(x, shadow_gaussian_x);
				gauss[i].mV[1] = llgaussian(x, shadow_gaussian_y);
				gauss[i].mV[2] = x;
				x += 1.f;
			}

			shaderp->uniform2f(sDelta, 1.f, 0.f);
			shaderp->uniform1f(sDistFactor, RenderShadowBlurDistFactor);
			shaderp->uniform3fv(sKern, kern_length, gauss[0].mV);
			shaderp->uniform1f(sKernScale,
							   RenderShadowBlurSize *
							   (kern_length * 0.5f - 0.5f));

			{
				LLGLDisable blend(GL_BLEND);
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
			}

			mRT->mScreen.flush();
			unbindDeferredShader(*shaderp);

			bindDeferredShader(*shaderp, &mRT->mScreen);
			mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
			mRT->mDeferredLight.bindTarget();

			shaderp->uniform2f(sDelta, 0.f, 1.f);

			{
				LLGLDisable blend(GL_BLEND);
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
			}
			mRT->mDeferredLight.flush();
			unbindDeferredShader(*shaderp);
		}

		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();

		mRT->mScreen.bindTarget();
		// Clear color buffer here; zeroing alpha (glow) is important or it
		// will accumulate against sky
		glClearColor(0.f, 0.f, 0.f, 0.f);
		mRT->mScreen.clear(GL_COLOR_BUFFER_BIT);

		if (RenderDeferredAtmospheric)
		{
			// Apply sunlight contribution

			LL_FAST_TIMER(FTM_ATMOSPHERICS);

			shaderp = sUnderWaterRender ? &gDeferredSoftenWaterProgram
										: &gDeferredSoftenProgram;
			bindDeferredShader(*shaderp);

			shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, mIsSunUp ? 1 : 0);
			shaderp->uniform4fv(LLShaderMgr::LIGHTNORM, 1,
								gEnvironment.getClampedLightNorm().mV);

			{
				LLGLDepthTest depth(GL_FALSE);
				LLGLDisable blend(GL_BLEND);

				// Full screen blit
				gGL.pushMatrix();
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.pushMatrix();
				gGL.loadIdentity();

				mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);

				gGL.popMatrix();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.popMatrix();
			}

			unbindDeferredShader(*shaderp);
		}

		{
			// Render non-deferred geometry (fullbright, alpha, etc)
			LLGLDisable blend(GL_BLEND);
			LLGLDisable stencil(GL_STENCIL_TEST);
			gGL.setSceneBlendType(LLRender::BT_ALPHA);

			pushRenderTypeMask();

			andRenderTypeMask(RENDER_TYPE_SKY,
							  RENDER_TYPE_CLOUDS,
							  RENDER_TYPE_WL_SKY,
							  END_RENDER_TYPES);

			renderGeomPostDeferred(gViewerCamera, false);
			popRenderTypeMask();
		}

		if (RenderLocalLightCount)
		{
			gGL.setSceneBlendType(LLRender::BT_ADD);
			std::list<LLVector4> fullscreen_lights, light_colors;
			LLDrawable::draw_list_t spot_lights, fullscreen_spot_lights;

			for (U32 i = 0; i < 2; ++i)
			{
				mTargetShadowSpotLight[i] = NULL;
			}

			LLVertexBuffer::unbind();

			{
				shaderp = &gDeferredLightProgram;
				bindDeferredShader(*shaderp);

				if (mCubeVB.notNull())
				{
					mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				}

				const LLVector3& cam_origin = gViewerCamera.getOrigin();
				F32 cam_x = cam_origin.mV[0];
				F32 cam_y = cam_origin.mV[1];
				F32 cam_z = cam_origin.mV[2];
				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				LLVector4a center, sa;
				LLColor3 col;
				U32 rendered_count = 0;
				// mNearbyLights already includes distance calculation and
				// excludes muted avatars. It is calculated from mLights and
				// also provides fade value to gracefully fade-out out of range
				// lights
				for (light_set_t::iterator iter = mNearbyLights.begin(),
										   end = mNearbyLights.end();
					 iter != end; ++iter)
				{
					if (++rendered_count > RenderLocalLightCount)
					{
						break;
					}

					LLDrawable* drawablep = iter->drawable;

					LLVOVolume* volp = drawablep->getVOVolume();
					if (!volp)
					{
						continue;
					}

					bool is_attachment = volp->isAttachment();
					if (is_attachment && !sRenderAttachedLights)
					{
						continue;
					}

					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volp->getLightRadius() * 1.5f;
					if (s <= 0.001f)
					{
						continue;
					}

					// Send light color to shader in linear space
					col = volp->getLightLinearColor();
					if (col.lengthSquared() < 0.001f)
					{
						continue;
					}

					sa.splat(s);
					if (gViewerCamera.AABBInFrustumNoFarClip(center, sa) == 0)
					{
						continue;
					}

					++sVisibleLightCount;

					if (cam_x > c[0] + s + 0.2f || cam_x < c[0] - s - 0.2f ||
						cam_y > c[1] + s + 0.2f || cam_y < c[1] - s - 0.2f ||
						cam_z > c[2] + s + 0.2f || cam_z < c[2] - s - 0.2f)
					{
						// Draw box if camera is outside box
						if (RenderLocalLightCount && mCubeVB.notNull())
						{
							if (volp->isLightSpotlight())
							{
								drawablep->getVOVolume()->updateSpotLightPriority();
								spot_lights.emplace_back(drawablep);
								continue;
							}

							LL_FAST_TIMER(FTM_LOCAL_LIGHTS);
							shaderp->uniform3fv(LLShaderMgr::LIGHT_CENTER, 1,
												c);
							shaderp->uniform1f(LLShaderMgr::LIGHT_SIZE, s);
							shaderp->uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1,
												col.mV);
							shaderp->uniform1f(LLShaderMgr::LIGHT_FALLOFF,
											   volp->getLightFalloff(0.5f));
							gGL.syncMatrices();

							mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8,
											   get_box_fan_indices(&gViewerCamera,
																   center));
						}
					}
					else
					{
						if (volp->isLightSpotlight())
						{
							drawablep->getVOVolume()->updateSpotLightPriority();
							fullscreen_spot_lights.emplace_back(drawablep);
							continue;
						}

						gGLModelView.affineTransform(center, center);
						LLVector4 tc(center.getF32ptr());
						tc.mV[VW] = s;
						fullscreen_lights.emplace_back(tc);

						light_colors.emplace_back(col.mV[0], col.mV[1],
												  col.mV[2],
												  volp->getLightFalloff(0.5f));
					}
				}
				stop_glerror();

				// When editting appearance, we need to add the corresponding
				// light at the camera position.
				static LLCachedControl<bool> custlight(gSavedSettings,
													   "AvatarCustomizeLighting");
				if (custlight && isAgentAvatarValid() &&
					gAgentAvatarp->mSpecialRenderMode == 3)
				{
					fullscreen_lights.emplace_back(0.f, 0.f, 0.f, 15.f);
					light_colors.emplace_back(1.f, 1.f, 1.f, 0.f);
				}

				unbindDeferredShader(*shaderp);
			}

			if (!spot_lights.empty() && mCubeVB.notNull())
			{
				LL_FAST_TIMER(FTM_PROJECTORS);

				LLGLDepthTest depth(GL_TRUE, GL_FALSE);

				shaderp = &gDeferredSpotLightProgram;
				bindDeferredShader(*shaderp);

				mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

				shaderp->enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				for (LLDrawable::draw_list_t::iterator
						iter = spot_lights.begin(), end = spot_lights.end();
					 iter != end; ++iter)
				{
					LLDrawable* drawablep = *iter;

					LLVOVolume* volp = drawablep->getVOVolume();

					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volp->getLightRadius() * 1.5f;

					++sVisibleLightCount;

					setupSpotLight(*shaderp, drawablep);

					LLColor3 col = volp->getLightLinearColor();
					shaderp->uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, c);
					shaderp->uniform1f(LLShaderMgr::LIGHT_SIZE, s);
					shaderp->uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
					shaderp->uniform1f(LLShaderMgr::LIGHT_FALLOFF,
									   volp->getLightFalloff(0.5f));
					gGL.syncMatrices();

					mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8,
									   get_box_fan_indices(&gViewerCamera,
														   center));
				}
				shaderp->disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				unbindDeferredShader(*shaderp);
			}

			// Reset mDeferredVB to fullscreen triangle
			if (!mDeferredVB->getVertexStrider(vert))
			{
				return;
			}
			vert[0].set(-1.f, 1.f, 0.f);
			vert[1].set(-1.f, -3.f, 0.f);
			vert[2].set(3.f, 1.f, 0.f);

			{
				LLGLDepthTest depth(GL_FALSE);

				// Full screen blit
				gGL.pushMatrix();
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.pushMatrix();
				gGL.loadIdentity();

				constexpr U32 max_count = LL_DEFERRED_MULTI_LIGHT_COUNT;
				LLVector4 light[max_count];
				LLVector4 col[max_count];
				U32 count = 0;
				F32 far_z = 0.f;

				{
					LL_FAST_TIMER(FTM_FULLSCREEN_LIGHTS);
					while (count < max_count && !fullscreen_lights.empty())
					{
						light[count] = fullscreen_lights.front();
						fullscreen_lights.pop_front();
						col[count] = light_colors.front();
						light_colors.pop_front();
						far_z = llmin(light[count].mV[2] - light[count].mV[3],
									  far_z);

						if (++count == max_count || fullscreen_lights.empty())
						{
							U32 idx = count - 1;
							shaderp = &gDeferredMultiLightProgram[idx];
							bindDeferredShader(*shaderp);
							shaderp->uniform1i(LLShaderMgr::MULTI_LIGHT_COUNT,
											   count);
							shaderp->uniform4fv(LLShaderMgr::MULTI_LIGHT,
												count, (F32*)light);
							shaderp->uniform4fv(LLShaderMgr::MULTI_LIGHT_COL,
												count, (F32*)col);
							shaderp->uniform1f(LLShaderMgr::MULTI_LIGHT_FAR_Z,
											   far_z);
							mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
							mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
							unbindDeferredShader(*shaderp);
							far_z = 0.f;
							count = 0;
						}
					}
				}

				shaderp = &gDeferredMultiSpotLightProgram;
				bindDeferredShader(*shaderp);

				shaderp->enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				mDeferredVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

				{
					LL_FAST_TIMER(FTM_PROJECTORS);
					for (LLDrawable::draw_list_t::iterator
							iter = fullscreen_spot_lights.begin(),
							end = fullscreen_spot_lights.end();
						 iter != end; ++iter)
					{
						LLDrawable* drawablep = *iter;

						LLVOVolume* volp = drawablep->getVOVolume();

						LLVector4a center;
						center.load3(drawablep->getPositionAgent().mV);
						F32 s = volp->getLightRadius() * 1.5f;

						++sVisibleLightCount;

						gGLModelView.affineTransform(center, center);

						setupSpotLight(*shaderp, drawablep);

						LLColor3 col = volp->getLightLinearColor();
						shaderp->uniform3fv(LLShaderMgr::LIGHT_CENTER, 1,
											center.getF32ptr());
						shaderp->uniform1f(LLShaderMgr::LIGHT_SIZE, s);
						shaderp->uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1,
											col.mV);
						shaderp->uniform1f(LLShaderMgr::LIGHT_FALLOFF,
										   volp->getLightFalloff(0.5f));
						mDeferredVB->drawArrays(LLRender::TRIANGLES, 0, 3);
					}
				}

				shaderp->disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				unbindDeferredShader(*shaderp);

				gGL.popMatrix();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.popMatrix();
			}
			gGL.setSceneBlendType(LLRender::BT_ALPHA);
		}

		gGL.setColorMask(true, true);
	}

	mRT->mScreen.flush();

	// Gamma-correct lighting

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	{
		LLGLDepthTest depth(GL_FALSE, GL_FALSE);

		LLVector2 tc1;
		LLVector2 tc2((F32)(mRT->mScreen.getWidth() * 2),
					  (F32)(mRT->mScreen.getHeight() * 2));

		mRT->mScreen.bindTarget();
		// Apply gamma correction to the frame here.
		shaderp = &gDeferredPostGammaCorrectProgram;
		shaderp->bind();
		S32 channel = shaderp->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
											 mRT->mScreen.getUsage());
		if (channel > -1)
		{
			mRT->mScreen.bindTexture(0, channel, LLTexUnit::TFO_POINT);
		}

		shaderp->uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
						   mRT->mScreen.getWidth(), mRT->mScreen.getHeight());

		shaderp->uniform1f(LLShaderMgr::DISPLAY_GAMMA,
						   1.f / RenderDeferredDisplayGamma);

		gGL.begin(LLRender::TRIANGLE_STRIP);
		gGL.texCoord2f(tc1.mV[0], tc1.mV[1]);
		gGL.vertex2f(-1.f, -1.f);

		gGL.texCoord2f(tc1.mV[0], tc2.mV[1]);
		gGL.vertex2f(-1.f, 3.f);

		gGL.texCoord2f(tc2.mV[0], tc1.mV[1]);
		gGL.vertex2f(3.f, -1.f);

		gGL.end();

		if (channel > -1)
		{
			gGL.getTexUnit(channel)->unbind(mRT->mScreen.getUsage());
		}
		shaderp->unbind();
		mRT->mScreen.flush();
		stop_glerror();
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	mRT->mScreen.bindTarget();

	{
		// Render non-deferred geometry (alpha, fullbright, glow)
		LLGLDisable blend(GL_BLEND);
		LLGLDisable stencil(GL_STENCIL_TEST);

		pushRenderTypeMask();
		andRenderTypeMask(RENDER_TYPE_ALPHA,
						  RENDER_TYPE_FULLBRIGHT,
						  RENDER_TYPE_VOLUME,
						  RENDER_TYPE_GLOW,
						  RENDER_TYPE_BUMP,
						  RENDER_TYPE_PASS_SIMPLE,
						  RENDER_TYPE_PASS_ALPHA,
						  RENDER_TYPE_PASS_ALPHA_MASK,
						  RENDER_TYPE_PASS_BUMP,
						  RENDER_TYPE_PASS_POST_BUMP,
						  RENDER_TYPE_PASS_FULLBRIGHT,
						  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
						  RENDER_TYPE_PASS_GLOW,
						  RENDER_TYPE_PASS_GRASS,
						  RENDER_TYPE_PASS_SHINY,
						  RENDER_TYPE_PASS_INVISIBLE,
						  RENDER_TYPE_PASS_INVISI_SHINY,
						  RENDER_TYPE_AVATAR,
						  RENDER_TYPE_PUPPET,
						  RENDER_TYPE_ALPHA_MASK,
						  RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
						  END_RENDER_TYPES);

		renderGeomPostDeferred(gViewerCamera);
		popRenderTypeMask();
	}

	{
		// Render highlights, etc.
		renderHighlights();
		mHighlightFaces.clear();

		renderDebug();

		LLVertexBuffer::unbind();

		if (hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_UI))
		{
			// Render debugging beacons.
			gObjectList.renderObjectBeacons();
			gObjectList.resetObjectBeacons();
			gSky.addSunMoonBeacons();
		}
	}

	mRT->mScreen.flush();
}

void LLPipeline::setupSpotLight(LLGLSLShader& shader, LLDrawable* drawablep)
{
	// Construct frustum
	LLVOVolume* volp = drawablep->getVOVolume();
	LLVector3 params = volp->getSpotLightParams();

	F32 fov = params.mV[0];
	F32 focus = params.mV[1];

	LLVector3 pos = drawablep->getPositionAgent();
	LLQuaternion quat = volp->getRenderRotation();
	LLVector3 scale = volp->getScale();

	// Get near clip plane
	LLVector3 at_axis(0.f, 0.f, -scale.mV[2] * 0.5f);
	at_axis *= quat;

	LLVector3 np = pos + at_axis;
	at_axis.normalize();

	// Get origin that has given fov for plane np, at_axis, and given scale
	F32 dist = scale.mV[1] * 0.5f / tanf(fov * 0.5f);

	LLVector3 origin = np - at_axis * dist;

	// Matrix from volume space to agent space
	LLMatrix4 light_mat4(quat, LLVector4(origin,1.f));
	LLMatrix4a light_mat;
	light_mat.loadu(light_mat4.getF32ptr());
	LLMatrix4a light_to_screen;
	light_to_screen.setMul(gGLModelView, light_mat);
	LLMatrix4a screen_to_light = light_to_screen;
	screen_to_light.invert();

	F32 s = volp->getLightRadius() * 1.5f;
	F32 near_clip = dist;
	F32 width = scale.mV[VX];
	F32 height = scale.mV[VY];
	F32 far_clip = s + dist - scale.mV[VZ];

	F32 fovy = fov * RAD_TO_DEG;
	F32 aspect = width / height;

	LLVector4a p1(0.f, 0.f, -(near_clip + 0.01f));
	LLVector4a p2(0.f, 0.f, -(near_clip + 1.f));

	LLVector4a screen_origin;
	screen_origin.clear();

	light_to_screen.affineTransform(p1, p1);
	light_to_screen.affineTransform(p2, p2);
	light_to_screen.affineTransform(screen_origin, screen_origin);

	LLVector4a n;
	n.setSub(p2, p1);
	n.normalize3fast();

	F32 proj_range = far_clip - near_clip;
	LLMatrix4a light_proj = gl_perspective(fovy, aspect, near_clip, far_clip);
	light_proj.setMul(TRANS_MAT, light_proj);
	screen_to_light.setMul(light_proj, screen_to_light);

	shader.uniformMatrix4fv(LLShaderMgr::PROJECTOR_MATRIX, 1, GL_FALSE,
							screen_to_light.getF32ptr());
	shader.uniform3fv(LLShaderMgr::PROJECTOR_P, 1, p1.getF32ptr());
	shader.uniform3fv(LLShaderMgr::PROJECTOR_N, 1, n.getF32ptr());
	shader.uniform3fv(LLShaderMgr::PROJECTOR_ORIGIN, 1,
					  screen_origin.getF32ptr());
	shader.uniform1f(LLShaderMgr::PROJECTOR_RANGE, proj_range);
	shader.uniform1f(LLShaderMgr::PROJECTOR_AMBIANCE, params.mV[2]);

#if OPTIMIZED_UNIFORMS
	if (shader.mFeatures.hasShadows)
#endif
	{
		S32 s_idx = -1;
		for (U32 i = 0; i < 2; ++i)
		{
			if (mShadowSpotLight[i] == drawablep)
			{
				s_idx = i;
			}
		}

		shader.uniform1i(LLShaderMgr::PROJECTOR_SHADOW_INDEX, s_idx);

		if (s_idx >= 0)
		{
			shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE,
							 1.f - mSpotLightFade[s_idx]);
		}
		else
		{
			shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE, 1.f);
		}

		if (!gCubeSnapshot)
		{
			LLDrawable* potentialp = drawablep;
			// Determine if this is a good light for casting shadows
			F32 m_pri = volp->getSpotLightPriority();

			for (U32 i = 0; i < 2; ++i)
			{
				F32 pri = 0.f;
				LLDrawable* slightp = mTargetShadowSpotLight[i].get();
				if (slightp)
				{
					pri = slightp->getVOVolume()->getSpotLightPriority();
				}
				if (m_pri > pri)
				{
					LLDrawable* tempp = mTargetShadowSpotLight[i];
					mTargetShadowSpotLight[i] = potentialp;
					potentialp = tempp;
					m_pri = pri;
				}
			}
		}
	}
#if DEBUG_OPTIMIZED_UNIFORMS && OPTIMIZED_UNIFORMS
	else if (shader.getUniformLocation(LLShaderMgr::PROJECTOR_SHADOW_INDEX) >= 0 ||
			 shader.getUniformLocation(LLShaderMgr::PROJECTOR_SHADOW_FADE) >= 0)
	{
		llwarns_once << "Shader: " << shader.mName
					 << " shall be marked as hasShadows !" << llendl;
	}
#endif

	LLViewerTexture* texp = volp->getLightTexture();
	if (!texp)
	{
		texp = LLViewerFetchedTexture::sWhiteImagep;
	}

	S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);
	if (channel > -1)
	{
		if (texp)
		{
			gGL.getTexUnit(channel)->bind(texp);
			shader.uniform1f(LLShaderMgr::PROJECTOR_FOCUS, focus);
			static const F32 INVLOG2 = 1.f / logf(2.f);
			F32 lod_range = logf(texp->getWidth()) * INVLOG2;
			shader.uniform1f(LLShaderMgr::PROJECTOR_LOD, lod_range);
		}
	}
	stop_glerror();
}

void LLPipeline::unbindDeferredShader(LLGLSLShader& shader)
{
	LLTexUnit::eTextureType usage = mRT->mDeferredScreen.getUsage();
	shader.disableTexture(LLShaderMgr::DEFERRED_NORMAL, usage);
	shader.disableTexture(LLShaderMgr::DEFERRED_DIFFUSE, usage);
	shader.disableTexture(LLShaderMgr::DEFERRED_SPECULAR, usage);
	if (gUsePBRShaders)
	{
		shader.disableTexture(LLShaderMgr::DEFERRED_EMISSIVE, usage);
		shader.disableTexture(LLShaderMgr::DEFERRED_BRDF_LUT);
	}
	else
	{
		// Not the same buffer used for DEFERRED_DEPTH in EE rendering, unlike
		// what happens for PBR rendering. HB
		usage = mDeferredDepth.getUsage();
	}
	shader.disableTexture(LLShaderMgr::DEFERRED_DEPTH, usage);
	shader.disableTexture(LLShaderMgr::DEFERRED_LIGHT,
						  mRT->mDeferredLight.getUsage());
	shader.disableTexture(LLShaderMgr::DIFFUSE_MAP);
	if (!gUsePBRShaders) // "bloomMap" not in use any more in PBR shaders. HB
	{
		shader.disableTexture(LLShaderMgr::DEFERRED_BLOOM);
	}

#if OPTIMIZED_UNIFORMS
	if (shader.mFeatures.hasShadows)
#endif
	{
		for (U32 i = 0; i < 6; ++i)
		{
			if (shader.disableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i) > -1)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
								GL_NONE);
			}
		}
	}

	shader.disableTexture(LLShaderMgr::DEFERRED_NOISE);
	shader.disableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);

	if (!sReflectionProbesEnabled)
	{
		S32 channel = shader.disableTexture(LLShaderMgr::ENVIRONMENT_MAP,
											LLTexUnit::TT_CUBE_MAP);
		if (channel > -1)
		{
			LLCubeMap* cube_mapp = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap()
												: NULL;
			if (cube_mapp)
			{
				cube_mapp->disableTexture();
			}
		}
	}

	if (gUsePBRShaders)
	{
		unbindReflectionProbes(shader);
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);
	unit0->activate();
	shader.unbind();
	stop_glerror();
}

// For EE rendering only
void LLPipeline::generateWaterReflection()
{
	if (!LLDrawPoolWater::sNeedsReflectionUpdate)
	{
		if (!gViewerCamera.cameraUnderWater())
		{
			// Initial sky pass is still needed even if water reflection is not
			// rendering.
			pushRenderTypeMask();
			andRenderTypeMask(RENDER_TYPE_SKY, RENDER_TYPE_WL_SKY,
							  RENDER_TYPE_CLOUDS, END_RENDER_TYPES);
			LLCamera camera = gViewerCamera;
			camera.setFar(camera.getFar() * 0.75f);
			updateCull(camera, mSky);
			stateSort(camera, mSky);
			renderGeom(camera);
			popRenderTypeMask();
		}
		return;
	}

	// Disable occlusion culling for reflection/refraction passes.
	LLDisableOcclusionCulling no_occlusion;

	bool skip_avatar_update = false;
	if (!isAgentAvatarValid() || gAgent.getCameraAnimating() ||
		gAgent.getCameraMode() != CAMERA_MODE_MOUSELOOK ||
		!LLVOAvatar::sVisibleInFirstPerson)
	{
		skip_avatar_update = true;
	}

	LLCamera camera = gViewerCamera;
	camera.setFar(camera.getFar() * 0.75f);

	sReflectionRender = true;

	pushRenderTypeMask();

	LLMatrix4a current = gGLModelView;
	LLMatrix4a projection = gGLProjection;

	F32 camera_height = gViewerCamera.getOrigin().mV[VZ];
	LLVector3 reflection_offset(0.f, 0.f,
								fabsf(camera_height - mWaterHeight) * 2.f);
	LLVector3 reflect_origin = gViewerCamera.getOrigin() - reflection_offset;
	const LLVector3& camera_look_at = gViewerCamera.getAtAxis();
	LLVector3 reflection_look_at(camera_look_at.mV[VX], camera_look_at.mV[VY],
								 -camera_look_at.mV[VZ]);
	LLVector3 reflect_interest_point = reflect_origin +
									   reflection_look_at * 5.f;
	camera.setOriginAndLookAt(reflect_origin, LLVector3::z_axis,
							  reflect_interest_point);

	// Plane params
	LLVector3 pnorm;
	bool camera_is_underwater = gViewerCamera.cameraUnderWater();
	if (camera_is_underwater)
	{
		// Camera is below water, cull above water
		pnorm.set(0.f, 0.f, 1.f);
	}
	else
	{
		// Camera is above water, cull below water
		pnorm.set(0.f, 0.f, -1.f);
	}
	LLPlane plane(LLVector3(0.f, 0.f, mWaterHeight), pnorm);

	if (!camera_is_underwater)
	{
		// Generate planar reflection map
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WATER0;

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();

		LLMatrix4a mat;
		mat.setIdentity();
		mat.getRow<2>().negate();
		mat.setTranslateAffine(LLVector3(0.f, 0.f, mWaterHeight * 2.f));
		mat.setMul(current, mat);

		mReflectionModelView = mat;

		gGLModelView = mat;
		gGL.loadMatrix(mat);

		LLViewerCamera::updateFrustumPlanes(camera, false, true);

		LLVector4a origin;
		origin.clear();
		LLMatrix4a inv_mat = mat;
		inv_mat.invert();
		inv_mat.affineTransform(origin, origin);
		camera.setOrigin(origin.getF32ptr());

		glCullFace(GL_FRONT);

		if (LLDrawPoolWater::sNeedsReflectionUpdate)
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			glClearColor(0.f, 0.f, 0.f, 0.f);
			mWaterRef.bindTarget();

			gGL.setColorMask(true, true);
			mWaterRef.clear();
			gGL.setColorMask(true, false);
			mWaterRef.getViewport(gGLViewport);

			// Initial sky pass (no user clip plane)
			// Mask out everything but the sky
			pushRenderTypeMask();
			U32 reflection_type = waterReflectionType();
			if (reflection_type < 5)	// Render sky without clouds
			{
				andRenderTypeMask(RENDER_TYPE_SKY, RENDER_TYPE_WL_SKY,
								  END_RENDER_TYPES);
			}
			else								// Render sky with clouds
			{
				andRenderTypeMask(RENDER_TYPE_SKY, RENDER_TYPE_WL_SKY,
								  RENDER_TYPE_CLOUDS, END_RENDER_TYPES);
			}
			updateCull(camera, mSky);
			stateSort(camera, mSky);
			renderGeom(camera);
			popRenderTypeMask();

			if (reflection_type > 1)	// If not just sky to render
			{
				pushRenderTypeMask();
				// These have just been rendered above: remove them now.
				clearRenderTypeMask(RENDER_TYPE_WATER, RENDER_TYPE_VOIDWATER,
									RENDER_TYPE_SKY, RENDER_TYPE_CLOUDS,
									END_RENDER_TYPES);

				// Mask out selected geometry based on reflection type
				if (reflection_type < 5)			// Remove particles
				{
					clearRenderTypeMask(RENDER_TYPE_PARTICLES,
										END_RENDER_TYPES);
					if (reflection_type < 4)		// Remove avatars
					{
						clearRenderTypeMask(RENDER_TYPE_AVATAR,
											RENDER_TYPE_PUPPET,
											END_RENDER_TYPES);
						if (reflection_type < 3)	// Remove objects
						{
							clearRenderTypeMask(RENDER_TYPE_VOLUME,
												END_RENDER_TYPES);
						}
					}
				}

				LLGLUserClipPlane clip_plane(plane, mReflectionModelView,
											 projection);
				LLGLDisable cull(GL_CULL_FACE);
				updateCull(camera, mReflectedObjects, &plane);
				stateSort(camera, mReflectedObjects);
				renderGeom(camera);
				popRenderTypeMask();
			}
			mWaterRef.flush();
		}

		glCullFace(GL_BACK);
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();
		gGLModelView = current;
	}

	camera.setOrigin(gViewerCamera.getOrigin());

	// Render distortion map
	static bool last_update = true;
	if (last_update)
	{
		pushRenderTypeMask();

		camera.setFar(gViewerCamera.getFar());
		clearRenderTypeMask(RENDER_TYPE_WATER, RENDER_TYPE_VOIDWATER,
							END_RENDER_TYPES);

		// Intentionally inverted so that distortion map contents (objects
		// under the water when we are above it) will properly include water
		// fog effects.
		sUnderWaterRender = !camera_is_underwater;

		if (sUnderWaterRender)
		{
			clearRenderTypeMask(RENDER_TYPE_SKY, RENDER_TYPE_CLOUDS,
								RENDER_TYPE_WL_SKY, END_RENDER_TYPES);
		}
		LLViewerCamera::updateFrustumPlanes(camera);

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		if (sUnderWaterRender || LLDrawPoolWater::sNeedsReflectionUpdate)
		{
			LLColor4& col = LLDrawPoolWater::sWaterFogColor;
			glClearColor(col.mV[0], col.mV[1], col.mV[2], 0.f);
			// *HACK: pretend underwater camera is the world camera to fix
			// weird visibility artifacts during distortion render (does not
			// break main render because the camera is the same perspective as
			// world camera and occlusion culling is disabled for this pass).
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

			mWaterDis.bindTarget();
			mWaterDis.getViewport(gGLViewport);

			gGL.setColorMask(true, true);
			mWaterDis.clear();
			gGL.setColorMask(true, false);

			// Clip out geometry on the same side of water as the camera with
			// enough margin to not include the water geo itself, but not so
			// much as to clip out parts of avatars that should be seen under
			// the water in the distortion map.
			constexpr F32 nudge_factor = 1.0125f;
			F32 water_dist = mWaterHeight;
			if (camera_is_underwater)
			{
				// Nudge clip plane below water to avoid visible holes in
				// objects intersecting the water surface.
				water_dist *= 1.f / nudge_factor;
				// Camera is below water, clip plane points up
				pnorm.set(0.f, 0.f, -1.f);
			}
			else
			{
				// Nudge clip plane above water to avoid visible holes in
				// objects intersecting the water surface.
				water_dist *= nudge_factor;
				// Camera is above water, clip plane points down
				pnorm.set(0.f, 0.f, 1.f);
			}
			LLPlane plane(LLVector3(0.f, 0.f, water_dist), pnorm);

			LLGLUserClipPlane clip_plane(plane, current, projection);

			gGL.setColorMask(true, true);
			mWaterDis.clear();
			gGL.setColorMask(true, false);

			if (RenderWaterReflectionType)
			{
				updateCull(camera, mRefractedObjects, &plane);
				stateSort(camera, mRefractedObjects);
				renderGeom(camera);
			}

			gUIProgram.bind();
			gWorld.renderPropertyLines();
			gUIProgram.unbind();

			mWaterDis.flush();
		}

		popRenderTypeMask();
	}
	last_update = LLDrawPoolWater::sNeedsReflectionUpdate;
	LLDrawPoolWater::sNeedsReflectionUpdate = false;

	popRenderTypeMask();

	sUnderWaterRender = false;
	sReflectionRender = false;

	if (!LLRenderTarget::sUseFBO)
	{
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	glClearColor(0.f, 0.f, 0.f, 0.f);
	gViewerWindowp->setupViewport();

	LL_GL_CHECK_STATES;

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
	}

	LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
}

// For PBR rendering only
void LLPipeline::renderShadow(const LLMatrix4a& view, const LLMatrix4a& proj,
							  LLCamera& shadow_cam, LLCullResult& result,
							  bool depth_clamp)
{
	LL_FAST_TIMER(FTM_SHADOW_RENDER);

	// Disable occlusion culling during shadow render
	LLDisableOcclusionCulling no_occlusion;

	sShadowRender = true;

	static const U32 types[] =
	{
		LLRenderPass::PASS_SIMPLE,
		LLRenderPass::PASS_FULLBRIGHT,
		LLRenderPass::PASS_SHINY,
		LLRenderPass::PASS_BUMP,
		LLRenderPass::PASS_FULLBRIGHT_SHINY,
		LLRenderPass::PASS_MATERIAL,
		LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
		LLRenderPass::PASS_SPECMAP,
		LLRenderPass::PASS_SPECMAP_EMISSIVE,
		LLRenderPass::PASS_NORMMAP,
		LLRenderPass::PASS_NORMMAP_EMISSIVE,
		LLRenderPass::PASS_NORMSPEC,
		LLRenderPass::PASS_NORMSPEC_EMISSIVE
	};

	LLGLEnable cull(GL_CULL_FACE);

#if 0	// The OpenGL version required by PBR should have depth clamp. HB
	if (depth_clamp)
	{
		static LLCachedControl<bool> dclamp(gSavedSettings,
											"RenderDepthClampShadows");
		if (!dclamp || !gGLManager.mUseDepthClamp)
		{
			depth_clamp = false;
		}
	}
#endif
	LLGLEnable clamp_depth(depth_clamp ? GL_DEPTH_CLAMP : 0);

	LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_LESS);

	updateCull(shadow_cam, result);

	stateSort(shadow_cam, result);

	// Generate shadow map
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(proj);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadMatrix(view);

	gGLLastMatrix = NULL;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	stop_glerror();

	LLVertexBuffer::unbind();

	for (U32 rigged = 0; rigged < 2; ++rigged)
	{
		gDeferredShadowProgram.bind(rigged);

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);

		// If not using VSM, disable color writes
		if (RenderShadowDetail <= 2)
		{
			gGL.setColorMask(false, false);
		}

		LL_FAST_TIMER(FTM_SHADOW_SIMPLE);
		unit0->disable();
		constexpr U32 count = sizeof(types) / sizeof(U32);
		for (U32 i = 0; i < count; ++i)
		{
			renderObjects(types[i], 0, false, false, rigged);
		}

		renderGLTFObjects(LLRenderPass::PASS_MAT_PBR, false, rigged);

		unit0->enable(LLTexUnit::TT_TEXTURE);
	}

	{
		LL_TRACY_TIMER(TRC_SHADOW_GEOM);
		renderGeomShadow(shadow_cam);
	}

	S32 sun_up = mIsSunUp ? 1 : 0;
	const F32 width = LLRenderTarget::sCurResX;
	LLGLSLShader* shaderp;
	{
		LL_FAST_TIMER(FTM_SHADOW_ALPHA);

		for (U32 rigged = 0; rigged < 2; ++rigged)
		{
			gDeferredShadowAlphaMaskProgram.bind(rigged);
			shaderp = LLGLSLShader::sCurBoundShaderPtr;
			shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
			shaderp->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH,
							   width);
			renderMaskedObjects(LLRenderPass::PASS_ALPHA_MASK, 0, true, true,
								rigged);
			renderAlphaObjects(rigged);

			gDeferredShadowFullbrightAlphaMaskProgram.bind(rigged);
			shaderp = LLGLSLShader::sCurBoundShaderPtr;
			shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
			shaderp->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH,
							   width);
			renderFullbrightMaskedObjects(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
										  0, true, true, rigged);

			gDeferredTreeShadowProgram.bind(rigged);
			shaderp = LLGLSLShader::sCurBoundShaderPtr;
			shaderp->setMinimumAlpha(0.598f);
			if (!rigged)
			{
				renderObjects(LLRenderPass::PASS_GRASS, 0, true);
			}
			renderMaskedObjects(LLRenderPass::PASS_NORMSPEC_MASK,
								0, true, false, rigged);
			renderMaskedObjects(LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
								0, true, false, rigged);
			renderMaskedObjects(LLRenderPass::PASS_SPECMAP_MASK,
								0, true, false, rigged);
			renderMaskedObjects(LLRenderPass::PASS_NORMMAP_MASK,
								0, true, false, rigged);
		}
	}

	for (U32 rigged = 0; rigged < 2; ++rigged)
	{
		gDeferredShadowGLTFAlphaMaskProgram.bind(rigged);
		shaderp = LLGLSLShader::sCurBoundShaderPtr;
		shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
		shaderp->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, width);
		gGL.loadMatrix(gGLModelView);
		gGLLastMatrix = NULL;
		constexpr U32 type = LLRenderPass::PASS_MAT_PBR_ALPHA_MASK;
		if (rigged)
		{
			mAlphaMaskPool->pushRiggedGLTFBatches(type + 1);
		}
		else
		{
			mAlphaMaskPool->pushGLTFBatches(type);
		}
		gGL.loadMatrix(gGLModelView);
		gGLLastMatrix = NULL;
	}

	gDeferredShadowCubeProgram.bind();
	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLModelView);

	gGL.setColorMask(true, true);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
	gGLLastMatrix = NULL;

	sShadowRender = false;
}

// For EE rendering only
void LLPipeline::renderShadow(const LLMatrix4a& view, const LLMatrix4a& proj,
							  LLCamera& shadow_cam, LLCullResult& result,
							  bool use_shader, bool use_occlusion,
							  U32 target_width)
{
	LL_FAST_TIMER(FTM_SHADOW_RENDER);

	// Clip out geometry on the same side of water as the camera
	S32 occlude = sUseOcclusion;
	if (!use_occlusion)
	{
		sUseOcclusion = 0;
	}
	sShadowRender = true;

	static const U32 types[] =
	{
		LLRenderPass::PASS_SIMPLE,
		LLRenderPass::PASS_FULLBRIGHT,
		LLRenderPass::PASS_SHINY,
		LLRenderPass::PASS_BUMP,
		LLRenderPass::PASS_FULLBRIGHT_SHINY,
		LLRenderPass::PASS_MATERIAL,
		LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
		LLRenderPass::PASS_SPECMAP,
		LLRenderPass::PASS_SPECMAP_EMISSIVE,
		LLRenderPass::PASS_NORMMAP,
		LLRenderPass::PASS_NORMMAP_EMISSIVE,
		LLRenderPass::PASS_NORMSPEC,
		LLRenderPass::PASS_NORMSPEC_EMISSIVE,
	};

	LLGLEnable cull(GL_CULL_FACE);

	// Enable depth clamping if available and in use for shaders.
	U32 depth_clamp_state = 0;
	if (gGLManager.mUseDepthClamp)
	{
		// Added a debug setting to see if it makes any difference for
		// projectors with some GPU/drivers (no difference seen by me
		// for NVIDIA GPU + proprietary drivers). HB
		static LLCachedControl<bool> dclamp(gSavedSettings,
											"RenderDepthClampShadows");
		if (dclamp)
		{
			depth_clamp_state = GL_DEPTH_CLAMP;
		}
	}
	LLGLEnable depth_clamp(depth_clamp_state);

	if (use_shader)
	{
		gDeferredShadowCubeProgram.bind();
	}

	LLRenderTarget& occlusion_target =
		mShadowOcclusion[LLViewerCamera::sCurCameraID - 1];
	occlusion_target.bindTarget();
	updateCull(shadow_cam, result);
	occlusion_target.flush();

	stateSort(shadow_cam, result);

	// Generate shadow map
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(proj);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadMatrix(view);

	gGLLastMatrix = NULL;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	stop_glerror();

	LLVertexBuffer::unbind();

	S32 sun_up = mIsSunUp ? 1 : 0;
	for (U32 rigged = 0; rigged < 2; ++rigged)
	{
		if (!use_shader)
		{
			// Occlusion program is general purpose depth-only no-textures
			gOcclusionProgram.bind(rigged);
		}
		else
		{
			gDeferredShadowProgram.bind(rigged);
			gDeferredShadowProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR,
											 sun_up);
		}

		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);

		// If not using VSM, disable color writes
		if (RenderShadowDetail <= 2)
		{
			gGL.setColorMask(false, false);
		}

		LL_FAST_TIMER(FTM_SHADOW_SIMPLE);
		unit0->disable();
		constexpr U32 count = sizeof(types) / sizeof(U32);
		for (U32 i = 0; i < count; ++i)
		{
			renderObjects(types[i], LLVertexBuffer::MAP_VERTEX, false, false,
						  rigged);
		}
		unit0->enable(LLTexUnit::TT_TEXTURE);
		if (!use_shader)
		{
			gOcclusionProgram.unbind();
		}
		stop_glerror();
	}

	if (use_shader)
	{
		LL_TRACY_TIMER(TRC_SHADOW_GEOM);
		gDeferredShadowProgram.unbind();
		renderGeomShadow(shadow_cam);
		gDeferredShadowProgram.bind();
		gDeferredShadowProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
	}
	else
	{
		LL_TRACY_TIMER(TRC_SHADOW_GEOM);
		renderGeomShadow(shadow_cam);
	}

	stop_glerror();

	{
		LL_FAST_TIMER(FTM_SHADOW_ALPHA);

		constexpr U32 NO_IDX_MASK = LLVertexBuffer::MAP_VERTEX |
									LLVertexBuffer::MAP_TEXCOORD0 |
									LLVertexBuffer::MAP_COLOR;
		constexpr U32 IDX_MASK = NO_IDX_MASK |
								 LLVertexBuffer::MAP_TEXTURE_INDEX;
		LLGLSLShader* shaderp;
		for (U32 rigged = 0; rigged < 2; ++rigged)
		{
			gDeferredShadowAlphaMaskProgram.bind(rigged);
			shaderp = LLGLSLShader::sCurBoundShaderPtr;
			shaderp->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH,
							   (F32)target_width);
			shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);

			renderMaskedObjects(LLRenderPass::PASS_ALPHA_MASK, IDX_MASK,
								true, true, rigged);

			shaderp->setMinimumAlpha(0.598f);
			renderAlphaObjects(rigged);

			gDeferredShadowFullbrightAlphaMaskProgram.bind(rigged);
			shaderp = LLGLSLShader::sCurBoundShaderPtr;
			shaderp->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH,
							   (F32)target_width);
			shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
			renderFullbrightMaskedObjects(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
										  IDX_MASK, true, true, rigged);

			gDeferredTreeShadowProgram.bind(rigged);
			shaderp = LLGLSLShader::sCurBoundShaderPtr;
			if (!rigged)
			{
				shaderp->setMinimumAlpha(0.598f);
				renderObjects(LLRenderPass::PASS_GRASS,
							  LLVertexBuffer::MAP_VERTEX |
							  LLVertexBuffer::MAP_TEXCOORD0,
							  true);
			}
			shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
			renderMaskedObjects(LLRenderPass::PASS_NORMSPEC_MASK,
								NO_IDX_MASK, true, false, rigged);
			renderMaskedObjects(LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
								NO_IDX_MASK, true, false, rigged);
			renderMaskedObjects(LLRenderPass::PASS_SPECMAP_MASK,
								NO_IDX_MASK, true, false, rigged);
			renderMaskedObjects(LLRenderPass::PASS_NORMMAP_MASK,
								NO_IDX_MASK, true, false, rigged);
		}
		stop_glerror();
	}

#if 0
	glCullFace(GL_BACK);
#endif

	gDeferredShadowCubeProgram.bind();
	gGLLastMatrix = NULL;
	gGL.loadMatrix(gGLModelView);

	LLRenderTarget& occlusion_source =
		mShadow[LLViewerCamera::sCurCameraID - 1];
	doOcclusion(shadow_cam, occlusion_source, occlusion_target);

	if (use_shader)
	{
		gDeferredShadowProgram.unbind();
	}

	gGL.setColorMask(true, true);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
	gGLLastMatrix = NULL;

	sUseOcclusion = occlude;
	sShadowRender = false;
	stop_glerror();
}

// Gets point cloud of intersection of frust and min, max
bool LLPipeline::getVisiblePointCloud(LLCamera& camera,
									  LLVector3& min, LLVector3& max,
									  std::vector<LLVector3>& fp,
									  LLVector3 light_dir)
{
	LL_FAST_TIMER(FTM_VISIBLE_CLOUD);

	if (getVisibleExtents(camera, min, max))
	{
		return false;
	}

	// Get set of planes on bounding box
	LLPlane bp[] =
	{
		LLPlane(min, LLVector3(-1.f, 0.f, 0.f)),
		LLPlane(min, LLVector3(0.f, -1.f, 0.f)),
		LLPlane(min, LLVector3(0.f, 0.f, -1.f)),
		LLPlane(max, LLVector3(1.f, 0.f, 0.f)),
		LLPlane(max, LLVector3(0.f, 1.f, 0.f)),
		LLPlane(max, LLVector3(0.f, 0.f, 1.f))
	};

	// Potential points
	std::vector<LLVector3> pp;

	// Add corners of AABB
	pp.emplace_back(min.mV[0], min.mV[1], min.mV[2]);
	pp.emplace_back(max.mV[0], min.mV[1], min.mV[2]);
	pp.emplace_back(min.mV[0], max.mV[1], min.mV[2]);
	pp.emplace_back(max.mV[0], max.mV[1], min.mV[2]);
	pp.emplace_back(min.mV[0], min.mV[1], max.mV[2]);
	pp.emplace_back(max.mV[0], min.mV[1], max.mV[2]);
	pp.emplace_back(min.mV[0], max.mV[1], max.mV[2]);
	pp.emplace_back(max.mV[0], max.mV[1], max.mV[2]);

	// Add corners of camera frustum
	for (U32 i = 0; i < LLCamera::AGENT_FRUSTRUM_NUM; ++i)
	{
		pp.emplace_back(camera.mAgentFrustum[i]);
	}

	// bounding box line segments
	static const U32 bs[] =
	{
		0, 1,
		1, 3,
		3, 2,
		2, 0,

		4, 5,
		5, 7,
		7, 6,
		6, 4,

		0, 4,
		1, 5,
		3, 7,
		2, 6
	};

	for (U32 i = 0; i < 12; ++i)	// for each line segment in bounding box
	{
		const LLVector3& v1 = pp[bs[i * 2]];
		const LLVector3& v2 = pp[bs[i * 2 + 1]];
		LLVector3 n, line;
		// For each plane in camera frustum
		for (U32 j = 0; j < LLCamera::AGENT_PLANE_NO_USER_CLIP_NUM; ++j)
		{
			const LLPlane& cp = camera.getAgentPlane(j);
			cp.getVector3(n);

			line = v1 - v2;

			F32 d1 = line * n;
			F32 d2 = -cp.dist(v2);

			F32 t = d2 / d1;

			if (t > 0.f && t < 1.f)
			{
				pp.emplace_back(v2 + line * t);
			}
		}
	}

	// Camera frustum line segments
	static const U32 fs[] =
	{
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		4, 5,
		5, 6,
		6, 7,
		7, 4,

		0, 4,
		1, 5,
		2, 6,
		3, 7
	};

	for (U32 i = 0; i < 12; ++i)
	{
		const LLVector3& v1 = pp[fs[i * 2] + 8];
		const LLVector3& v2 = pp[fs[i * 2 + 1] + 8];
		LLVector3 n, line;
		for (U32 j = 0; j < 6; ++j)
		{
			const LLPlane& cp = bp[j];
			cp.getVector3(n);

			line = v1 - v2;

			F32 d1 = line * n;
			F32 d2 = -cp.dist(v2);

			F32 t = d2 / d1;

			if (t > 0.f && t < 1.f)
			{
				pp.emplace_back(v2 + line * t);
			}
		}
	}

	LLVector3 ext[] = { min - LLVector3(0.05f, 0.05f, 0.05f),
						max + LLVector3(0.05f, 0.05f, 0.05f) };

	for (U32 i = 0, count = pp.size(); i < count; ++i)
	{
		bool found = true;

		const F32* p = pp[i].mV;

		for (U32 j = 0; j < 3; ++j)
		{
			if (p[j] < ext[0].mV[j] || p[j] > ext[1].mV[j])
			{
				found = false;
				break;
			}
		}

		for (U32 j = 0; j < LLCamera::AGENT_PLANE_NO_USER_CLIP_NUM; ++j)
		{
			const LLPlane& cp = camera.getAgentPlane(j);
			F32 dist = cp.dist(pp[i]);
			if (dist > 0.05f)
			{
				// point is above some plane, not contained
				found = false;
				break;
			}
		}
		if (found)
		{
			fp.emplace_back(pp[i]);
		}
	}

	return !fp.empty();
}

void LLPipeline::renderHighlight(const LLViewerObject* objp, F32 fade)
{
	if (!objp || objp->isDead() || !objp->getVolume())
	{
		return;
	}

	for (LLViewerObject::child_list_t::const_iterator
			it = objp->getChildren().begin(), end = objp->getChildren().end();
		 it != end; ++it)
	{
		renderHighlight(*it, fade);
	}

	LLDrawable* drawablep = objp->mDrawable;
	if (!drawablep)
	{
		return;
	}

	LLColor4 color(1.f, 1.f, 1.f, fade);
	for (S32 i = 0, count = drawablep->getNumFaces(); i < count; ++i)
	{
		LLFace* facep = drawablep->getFace(i);
		if (facep)
		{
			facep->renderSelected(LLViewerTexture::sNullImagep, color);
		}
	}
}

// Branched version for the PBR renderer
void LLPipeline::generateSunShadowPBR()
{
	LLDisableOcclusionCulling no_occlusion;

	bool skip_avatar_update = false;
	if (!isAgentAvatarValid() || gAgent.getCameraAnimating() ||
		gAgent.getCameraMode() != CAMERA_MODE_MOUSELOOK ||
		!LLVOAvatar::sVisibleInFirstPerson)
	{
		skip_avatar_update = true;
	}

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(CAMERA_MODE_THIRD_PERSON);
	}

	// Store last_modelview of world camera
	LLMatrix4a last_modelview = gGLLastModelView;
	LLMatrix4a last_projection = gGLLastProjection;

	pushRenderTypeMask();
	andRenderTypeMask(RENDER_TYPE_SIMPLE,
					  RENDER_TYPE_ALPHA,
					  RENDER_TYPE_ALPHA_PRE_WATER,
					  RENDER_TYPE_ALPHA_POST_WATER,
					  RENDER_TYPE_GRASS,
					  RENDER_TYPE_MAT_PBR,
					  RENDER_TYPE_FULLBRIGHT,
					  RENDER_TYPE_BUMP,
					  RENDER_TYPE_VOLUME,
					  RENDER_TYPE_AVATAR,
					  RENDER_TYPE_PUPPET,
					  RENDER_TYPE_TREE,
					  RENDER_TYPE_TERRAIN,
					  RENDER_TYPE_WATER,
					  RENDER_TYPE_VOIDWATER,
					  RENDER_TYPE_PASS_ALPHA,
					  RENDER_TYPE_PASS_ALPHA_MASK,
					  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
					  RENDER_TYPE_PASS_GRASS,
					  RENDER_TYPE_PASS_SIMPLE,
					  RENDER_TYPE_PASS_BUMP,
					  RENDER_TYPE_PASS_FULLBRIGHT,
					  RENDER_TYPE_PASS_SHINY,
					  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
					  RENDER_TYPE_PASS_MATERIAL,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
					  RENDER_TYPE_PASS_SPECMAP,
					  RENDER_TYPE_PASS_SPECMAP_BLEND,
					  RENDER_TYPE_PASS_SPECMAP_MASK,
					  RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
					  RENDER_TYPE_PASS_NORMMAP,
					  RENDER_TYPE_PASS_NORMMAP_BLEND,
					  RENDER_TYPE_PASS_NORMMAP_MASK,
					  RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
					  RENDER_TYPE_PASS_NORMSPEC,
					  RENDER_TYPE_PASS_NORMSPEC_BLEND,
					  RENDER_TYPE_PASS_NORMSPEC_MASK,
					  RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
					  RENDER_TYPE_PASS_ALPHA_MASK_RIGGED,
					  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK_RIGGED,
					  RENDER_TYPE_PASS_SIMPLE_RIGGED,
					  RENDER_TYPE_PASS_BUMP_RIGGED,
					  RENDER_TYPE_PASS_FULLBRIGHT_RIGGED,
					  RENDER_TYPE_PASS_SHINY_RIGGED,
					  RENDER_TYPE_PASS_FULLBRIGHT_SHINY_RIGGED,
					  RENDER_TYPE_PASS_MATERIAL_RIGGED,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_RIGGED,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK_RIGGED,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED,
					  RENDER_TYPE_PASS_SPECMAP_RIGGED,
					  RENDER_TYPE_PASS_SPECMAP_BLEND_RIGGED,
					  RENDER_TYPE_PASS_SPECMAP_MASK_RIGGED,
					  RENDER_TYPE_PASS_SPECMAP_EMISSIVE_RIGGED,
					  RENDER_TYPE_PASS_NORMMAP_RIGGED,
					  RENDER_TYPE_PASS_NORMMAP_BLEND_RIGGED,
					  RENDER_TYPE_PASS_NORMMAP_MASK_RIGGED,
					  RENDER_TYPE_PASS_NORMMAP_EMISSIVE_RIGGED,
					  RENDER_TYPE_PASS_NORMSPEC_RIGGED,
					  RENDER_TYPE_PASS_NORMSPEC_BLEND_RIGGED,
					  RENDER_TYPE_PASS_NORMSPEC_MASK_RIGGED,
					  RENDER_TYPE_PASS_NORMSPEC_EMISSIVE_RIGGED,
					  RENDER_TYPE_PASS_MAT_PBR,
					  RENDER_TYPE_PASS_MAT_PBR_RIGGED,
					  RENDER_TYPE_PASS_MAT_ALPHA_MASK_PBR,
					  RENDER_TYPE_PASS_MAT_ALPHA_MASK_PBR_RIGGED,
					  END_RENDER_TYPES);

	gGL.setColorMask(false, false);

	// Get sun view matrix

	// Store current projection/modelview matrix
	const LLMatrix4a saved_proj = gGLProjection;
	const LLMatrix4a saved_view = gGLModelView;
	LLMatrix4a inv_view(saved_view);
	inv_view.invert();

	LLMatrix4a view[6];
	LLMatrix4a proj[6];

	LLVector3 caster_dir(mIsSunUp ? mSunDir : mMoonDir);

	// Put together a universal "near clip" plane for shadow frusta
	LLPlane shadow_near_clip;
	LLVector3 p = gViewerCamera.getOrigin();
	p += caster_dir * RenderFarClip * 2.f;
	shadow_near_clip.setVec(p, caster_dir);

	LLVector3 light_dir = -caster_dir;
	light_dir.normalize();

	// Create light space camera matrix

	LLVector3 at = light_dir;

	LLVector3 up = gViewerCamera.getAtAxis();

	if (fabsf(up * light_dir) > 0.75f)
	{
		up = gViewerCamera.getUpAxis();
	}

	up.normalize();
	at.normalize();

	LLCamera main_camera = gViewerCamera;

	bool no_shadow_frustra = !gCubeSnapshot &&
							 !hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA);

	F32 near_clip = 0.f;

	// Get visible point cloud
	main_camera.calcAgentFrustumPlanes(main_camera.mAgentFrustum);
	LLVector3 min, max;
	std::vector<LLVector3> fp;
	getVisiblePointCloud(main_camera, min, max, fp);
	if (fp.empty())
	{
		if (no_shadow_frustra)
		{
			mShadowCamera[0] = main_camera;
			mShadowExtents[0][0] = min;
			mShadowExtents[0][1] = max;

			mShadowFrustPoints[0].clear();
			mShadowFrustPoints[1].clear();
			mShadowFrustPoints[2].clear();
			mShadowFrustPoints[3].clear();
		}
		popRenderTypeMask();

		if (!skip_avatar_update)
		{
			gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
		}

		return;
	}

	LLVector4a v;
	// Get good split distances for frustum
	for (U32 i = 0; i < fp.size(); ++i)
	{
		v.load3(fp[i].mV);
		saved_view.affineTransform(v, v);
		fp[i].set(v.getF32ptr());
	}

	min = fp[0];
	max = fp[0];

	// Get camera space bounding box
	for (U32 i = 1; i < fp.size(); ++i)
	{
		update_min_max(min, max, fp[i]);
	}

	near_clip = llclamp(-max.mV[2], 0.01f, 4.f);
	F32 far_clip = llclamp(-min.mV[2] * 2.f, 16.f, 512.f);
	far_clip = llmin(far_clip, gViewerCamera.getFar());

	F32 range = far_clip - near_clip;

	LLVector3 split_exp = RenderShadowSplitExponent;

	F32 da = 1.f - llmax(fabsf(light_dir * up),
						 fabsf(light_dir * gViewerCamera.getLeftAxis()));
	da = powf(da, split_exp.mV[2]);

	F32 sxp = split_exp.mV[1] + (split_exp.mV[0] - split_exp.mV[1]) * da;

	for (U32 i = 0; i < 4; ++i)
	{
		F32 x = (F32)(i + 1) * 0.25f;
		x = powf(x, sxp);
		mSunClipPlanes.mV[i] = near_clip + range * x;
	}

	// Bump back first split for transition padding
	mSunClipPlanes.mV[0] *= 1.25f;

	if (gCubeSnapshot)
	{
		// Stretch clip planes for reflection probe renders to reduce number
		// of shadow passes
		mSunClipPlanes.mV[1] = mSunClipPlanes.mV[2];
		mSunClipPlanes.mV[2] = mSunClipPlanes.mV[3];
		mSunClipPlanes.mV[3] *= 1.5f;
	}

	// Convenience array of 4 near clip plane distances
	F32 dist[] = { near_clip,
				   mSunClipPlanes.mV[0],
				   mSunClipPlanes.mV[1],
				   mSunClipPlanes.mV[2],
				   mSunClipPlanes.mV[3] };

	if (mSunDiffuse == LLColor4::black)
	{
		// Sun diffuse is totally black, shadows do not matter
		LLGLDepthTest depth(GL_TRUE);

		for (S32 j = 0; j < 4; ++j)
		{
			mRT->mSunShadow[j].bindTarget();
			mRT->mSunShadow[j].clear();
			mRT->mSunShadow[j].flush();
		}
	}
	else
	{
		static LLCachedControl<U32> splits(gSavedSettings,
										   "RenderShadowSplits");
		U32 max_splits = llclamp((U32)splits, 0, 3);
		for (U32 j = 0, count = gCubeSnapshot ? 2 : 4; j < count; ++j)
		{
			if (no_shadow_frustra)
			{
				mShadowFrustPoints[j].clear();
			}

			LLViewerCamera::sCurCameraID =
				LLViewerCamera::CAMERA_SUN_SHADOW0 + j;

			// Restore render matrices
			gGLModelView = saved_view;
			gGLProjection = saved_proj;

			LLVector3 eye = gViewerCamera.getOrigin();

			// Camera used for shadow cull/render
			LLCamera shadow_cam = gViewerCamera;
			shadow_cam.setFar(16.f);

			// Create world space camera frustum for this split
			LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
												true);

			LLVector3* frust = shadow_cam.mAgentFrustum;

			LLVector3 pn = shadow_cam.getAtAxis();

			// Construct 8 corners of split frustum section
			for (U32 i = 0; i < 4; ++i)
			{
				LLVector3 delta = frust[i + 4] - eye;
				delta += (frust[i + 4] - frust[(i + 2) % 4 + 4]) * 0.05f;
				delta.normalize();
				F32 dp = delta * pn;
				frust[i] = eye + (delta * dist[j] * 0.75f) / dp;
				frust[i + 4] = eye + (delta * dist[j + 1] * 1.25f) / dp;
			}

			shadow_cam.calcAgentFrustumPlanes(frust);
			shadow_cam.mFrustumCornerDist = 0.f;

			if (no_shadow_frustra)
			{
				mShadowCamera[j] = shadow_cam;
			}

			std::vector<LLVector3> fp;
			LLVector3 min, max;
			if (!getVisiblePointCloud(shadow_cam, min, max, fp, light_dir) ||
				j > max_splits)
			{
				// No possible shadow receivers
				if (no_shadow_frustra)
				{
					mShadowExtents[j][0].clear();
					mShadowExtents[j][1].clear();
					mShadowCamera[j + 4] = shadow_cam;
				}

				mRT->mSunShadow[j].bindTarget();
				{
					LLGLDepthTest depth(GL_TRUE);
					mRT->mSunShadow[j].clear();
				}
				mRT->mSunShadow[j].flush();

				continue;
			}

			if (no_shadow_frustra)
			{
				mShadowExtents[j][0] = min;
				mShadowExtents[j][1] = max;
				mShadowFrustPoints[j] = fp;
			}

			// Find a good origin for shadow projection
			LLVector3 origin;

			// Get a temporary view projection
			view[j] = look_proj(gViewerCamera.getOrigin(), light_dir, -up);

			std::vector<LLVector3> wpf;
			LLVector4a p;
			for (U32 i = 0; i < fp.size(); ++i)
			{
				p.load3(fp[i].mV);
				view[j].affineTransform(p, p);
				wpf.emplace_back(LLVector3(p.getF32ptr()));
			}

			min = max = wpf[0];

			for (U32 i = 0; i < fp.size(); ++i)
			{
				// Get AABB in camera space
				update_min_max(min, max, wpf[i]);
			}

			// Construct a perspective transform with perspective along y-axis
			// that contains points in wpf
			// Known:
			// - far clip plane
			// - near clip plane
			// - points in frustum
			// Find:
			// - origin

			// Get some "interesting" points of reference
			LLVector3 center = (min + max) * 0.5f;
			LLVector3 size = (max - min) * 0.5f;
			LLVector3 near_center = center;
			near_center.mV[1] += size.mV[1] * 2.f;

			// Put all points in wpf in quadrant 0, reletive to center of
			// min/max get the best fit line using least squares

			for (U32 i = 0; i < wpf.size(); ++i)
			{
				wpf[i] -= center;
				wpf[i].mV[0] = fabsf(wpf[i].mV[0]);
				wpf[i].mV[2] = fabsf(wpf[i].mV[2]);
			}

			F32 bfm = 0.f;
			F32 bfb = 0.f;
			if (!wpf.empty())
			{
				F32 sx = 0.f;
				F32 sx2 = 0.f;
				F32 sy = 0.f;
				F32 sxy = 0.f;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					sx += wpf[i].mV[0];
					sx2 += wpf[i].mV[0] * wpf[i].mV[0];
					sy += wpf[i].mV[1];
					sxy += wpf[i].mV[0] * wpf[i].mV[1];
				}

				bfm = (sy * sx - wpf.size() * sxy) /
					  (sx * sx - wpf.size() * sx2);
				bfb = (sx * sxy - sy * sx2) / (sx * sx - bfm * sx2);
			}
			if (llisnan(bfm) || llisnan(bfb))
			{
				LL_DEBUGS("Pipeline") << "NaN found. Corresponding shadow rendering aborted. Camera ID: "
									  << LLViewerCamera::sCurCameraID
									  << LL_ENDL;
				continue;
			}

			{
				// Best fit line is y = bfm * x + bfb

				// Find point that is furthest to the right of line
				F32 off_x = -1.f;
				LLVector3 lp;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					// y = bfm * x + bfb
					// x = (y - bfb) / bfm
					F32 lx = (wpf[i].mV[1] - bfb) / bfm;

					lx = wpf[i].mV[0] - lx;

					if (off_x < lx)
					{
						off_x = lx;
						lp = wpf[i];
					}
				}

				// Get line with slope bfm through lp
				// bfb = y - bfm * x
				bfb = lp.mV[1] - bfm * lp.mV[0];

				// Calculate error
				F32 shadow_error = 0.f;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					F32 lx = (wpf[i].mV[1] - bfb) / bfm;
					shadow_error += fabsf(wpf[i].mV[0] - lx);
				}

				shadow_error /= wpf.size() * size.mV[0];

				if (llisnan(shadow_error) ||
					shadow_error > RenderShadowErrorCutoff)
				{
					// Just use ortho projection
					origin.clear();
					proj[j] = gl_ortho(min.mV[0], max.mV[0], min.mV[1],
									   max.mV[1], -max.mV[2], -min.mV[2]);
				}
				else
				{
					// Origin is where line x = 0;
					origin.set(0, bfb, 0);

					F32 fovz = 1.f;
					F32 fovx = 1.f;

					LLVector3 zp;
					LLVector3 xp;

					for (U32 i = 0; i < wpf.size(); ++i)
					{
						LLVector3 atz = wpf[i] - origin;
						atz.mV[0] = 0.f;
						atz.normalize();
						if (fovz > -atz.mV[1])
						{
							zp = wpf[i];
							fovz = -atz.mV[1];
						}

						LLVector3 atx = wpf[i] - origin;
						atx.mV[2] = 0.f;
						atx.normalize();
						if (fovx > -atx.mV[1])
						{
							fovx = -atx.mV[1];
							xp = wpf[i];
						}
					}

					fovx = acosf(fovx);
					fovz = acosf(fovz);

					F32 cutoff = RenderShadowFOVCutoff;

					if (fovx < cutoff && fovz > cutoff)
					{
						// x is a good fit, but z is too big, move away from zp
						// enough so that fovz matches cutoff
						F32 d = zp.mV[2] / tanf(cutoff);
						F32 ny = zp.mV[1] + fabsf(d);

						origin.mV[1] = ny;

						fovz = fovx = 1.f;

						for (U32 i = 0; i < wpf.size(); ++i)
						{
							LLVector3 atz = wpf[i] - origin;
							atz.mV[0] = 0.f;
							atz.normalize();
							fovz = llmin(fovz, -atz.mV[1]);

							LLVector3 atx = wpf[i] - origin;
							atx.mV[2] = 0.f;
							atx.normalize();
							fovx = llmin(fovx, -atx.mV[1]);
						}

						fovx = acosf(fovx);
						fovz = acosf(fovz);
					}

					origin += center;

					F32 ynear = origin.mV[1] - max.mV[1];
					F32 yfar = origin.mV[1] - min.mV[1];

					if (ynear < 0.1f) // keep a sensible near clip plane
					{
						F32 diff = 0.1f - ynear;
						origin.mV[1] += diff;
						ynear += diff;
						yfar += diff;
					}

					if (fovx > cutoff)
					{
						// Just use ortho projection
						origin.clear();
						proj[j] = gl_ortho(min.mV[0], max.mV[0], min.mV[1],
										   max.mV[1], -max.mV[2], -min.mV[2]);
					}
					else
					{
						// Get perspective projection
						view[j].invert();

						// Translate view to origin
						LLVector4a origin_agent;
						origin_agent.load3(origin.mV);
						view[j].affineTransform(origin_agent, origin_agent);

						eye = LLVector3(origin_agent.getF32ptr());

						view[j] = look_proj(LLVector3(origin_agent.getF32ptr()),
											light_dir, -up);
						F32 fx = 1.f / tanf(fovx);
						F32 fz = 1.f / tanf(fovz);
						const F32 y1 = (yfar + ynear) / (ynear - yfar);
						const F32 y3 = 2.f * yfar * ynear / (ynear - yfar);
						proj[j].setRow<0>(LLVector4a(-fx, 0.f, 0.f, 0.f));
						proj[j].setRow<1>(LLVector4a(0.f, y1, 0.f, -1.f));
						proj[j].setRow<2>(LLVector4a(0.f, 0.f, -fz, 0.f));
						proj[j].setRow<3>(LLVector4a(0.f, y3, 0.f, 0.f));				
					}
				}
			}

#if 0
			shadow_cam.setFar(128.f);
#endif
			if (llisnan(eye.mV[VX]) || llisnan(eye.mV[VY]) ||
				llisnan(eye.mV[VZ]))
			{
				LL_DEBUGS("Pipeline") << "NaN found in eye origin. Corresponding shadow rendering aborted. Camera ID: "
									  << LLViewerCamera::sCurCameraID
									  << LL_ENDL;
				continue;
			}
			shadow_cam.setOriginAndLookAt(eye, up, center);

			shadow_cam.setOrigin(0.f, 0.f, 0.f);

			gGLModelView = view[j];
			gGLProjection = proj[j];

			LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
												true);

			// shadow_cam.ignoreAgentFrustumPlane(LLCamera::AGENT_PLANE_NEAR);
			shadow_cam.agentPlane(LLCamera::AGENT_PLANE_NEAR).set(shadow_near_clip);

			gGLModelView = view[j];
			gGLProjection = proj[j];

			gGLLastModelView = mShadowModelview[j];
			gGLLastProjection = mShadowProjection[j];

			mShadowModelview[j] = view[j];
			mShadowProjection[j] = proj[j];

			mSunShadowMatrix[j].setMul(TRANS_MAT, proj[j]);
			mSunShadowMatrix[j].mulAffine(view[j]);
			mSunShadowMatrix[j].mulAffine(inv_view);

			mRT->mSunShadow[j].bindTarget();
			mRT->mSunShadow[j].getViewport(gGLViewport);
			mRT->mSunShadow[j].clear();

			static LLCullResult result[4];
			renderShadow(view[j], proj[j], shadow_cam, result[j], true);

			mRT->mSunShadow[j].flush();

			if (no_shadow_frustra)
			{
				mShadowCamera[j + 4] = shadow_cam;
			}
		}
	}

	// HACK to disable projector shadows
	bool gen_shadow = RenderShadowDetail > 1;
	// Note: skip updating spot shadow maps during cubemap updates
	if (gen_shadow && !gCubeSnapshot)
	{
		F32 fade_amt =
			gFrameIntervalSeconds *
			llmax(LLViewerCamera::getVelocityStat().getCurrentPerSec(), 1.f);

		// Update shadow targets
		for (U32 i = 0; i < 2; ++i)
		{
			// For each current shadow
			LLViewerCamera::sCurCameraID =
				LLViewerCamera::CAMERA_SPOT_SHADOW0 + i;

			if (mShadowSpotLight[i].notNull() &&
				(mShadowSpotLight[i] == mTargetShadowSpotLight[0] ||
				 mShadowSpotLight[i] == mTargetShadowSpotLight[1]))
			{
				// Keep this spotlight
				mSpotLightFade[i] = llmin(mSpotLightFade[i] + fade_amt, 1.f);
			}
			else
			{
				// Fade out this light
				mSpotLightFade[i] = llmax(mSpotLightFade[i] - fade_amt, 0.f);

				if (mSpotLightFade[i] == 0.f || mShadowSpotLight[i].isNull())
				{
					// Faded out, grab one of the pending spots (whichever one
					// is not already taken)
					if (mTargetShadowSpotLight[0] != mShadowSpotLight[(i + 1) % 2])
					{
						mShadowSpotLight[i] = mTargetShadowSpotLight[0];
					}
					else
					{
						mShadowSpotLight[i] = mTargetShadowSpotLight[1];
					}
				}
			}
		}
	}

	if (gen_shadow)
	{
		for (S32 i = 0; i < 2; ++i)
		{
			gGLModelView = saved_view;
			gGLProjection = saved_proj;

			if (mShadowSpotLight[i].isNull())
			{
				continue;
			}

			LLVOVolume* volp = mShadowSpotLight[i]->getVOVolume();
			if (!volp)
			{
				mShadowSpotLight[i] = NULL;
				continue;
			}

			LLDrawable* drawablep = mShadowSpotLight[i];

			LLVector3 params = volp->getSpotLightParams();
			F32 fov = params.mV[0];

			// Get agent->light space matrix (modelview)
			LLVector3 center = drawablep->getPositionAgent();
			LLQuaternion quat = volp->getRenderRotation();

			// Get near clip plane
			LLVector3 scale = volp->getScale();
			LLVector3 at_axis(0.f, 0.f, -scale.mV[2] * 0.5f);
			at_axis *= quat;

			LLVector3 np = center + at_axis;
			at_axis.normalize();

			// Get origin that has given fov for plane np, at_axis, and given
			// scale
			F32 divisor = tanf(fov * 0.5f);
			// Seen happening and causing NaNs in setOrigin() below. HB
			if (divisor == 0.f) continue;
			F32 dist = (scale.mV[1] * 0.5f) / divisor;

			LLVector3 origin = np - at_axis * dist;

			LLMatrix4 mat(quat, LLVector4(origin, 1.f));

			view[i + 4].loadu(mat.getF32ptr());
			view[i + 4].invert();

			// Get perspective matrix
			F32 near_clip = dist + 0.01f;
			F32 width = scale.mV[VX];
			F32 height = scale.mV[VY];
			F32 far_clip = dist + volp->getLightRadius() * 1.5f;

			F32 fovy = fov * RAD_TO_DEG;
			F32 aspect = width / height;

			proj[i + 4] = gl_perspective(fovy, aspect, near_clip, far_clip);

			// Translate and scale from [-1, 1] to [0, 1]

			gGLModelView = view[i + 4];
			gGLProjection = proj[i + 4];

			mSunShadowMatrix[i + 4].setMul(TRANS_MAT, proj[i + 4]);
			mSunShadowMatrix[i + 4].mulAffine(view[i + 4]);
			mSunShadowMatrix[i + 4].mulAffine(inv_view);

			gGLLastModelView = mShadowModelview[i + 4];
			gGLLastProjection = mShadowProjection[i + 4];

			mShadowModelview[i + 4] = view[i + 4];
			mShadowProjection[i + 4] = proj[i + 4];

			// Skip updating spot shadow maps during cubemap updates
			if (!gCubeSnapshot)
			{
				LLCamera shadow_cam = gViewerCamera;
				shadow_cam.setFar(far_clip);
				shadow_cam.setOrigin(origin);

				LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
													true);

				mSpotShadow[i].bindTarget();
				mSpotShadow[i].getViewport(gGLViewport);
				mSpotShadow[i].clear();

				LLViewerCamera::sCurCameraID =
					LLViewerCamera::CAMERA_SPOT_SHADOW0 + i;

				sRenderSpotLight = drawablep;
				static LLCullResult result[2];
				renderShadow(view[i + 4], proj[i + 4], shadow_cam, result[i],
							 false);
				sRenderSpotLight = NULL;

				mSpotShadow[i].flush();
			}
 		}
	}
	else
	{
		// No spotlight shadows
		mShadowSpotLight[0] = mShadowSpotLight[1] = NULL;
	}

	if (!CameraOffset)
	{
		gGLModelView = saved_view;
		gGLProjection = saved_proj;
	}
	else
	{
		gGLModelView = view[1];
		gGLProjection = proj[1];
		gGL.loadMatrix(view[1]);
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.loadMatrix(proj[1]);
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
	gGL.setColorMask(true, true);

	gGLLastModelView = last_modelview;
	gGLLastProjection = last_projection;

	popRenderTypeMask();

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
	}
}

void LLPipeline::generateSunShadow()
{
	if (!sRenderDeferred || !RenderShadowDetail)
	{
		return;
	}

	LL_FAST_TIMER(FTM_GEN_SUN_SHADOW);

	if (gUsePBRShaders)
	{
		generateSunShadowPBR();
		return;
	}

	bool skip_avatar_update = false;
	if (!isAgentAvatarValid() || gAgent.getCameraAnimating() ||
		gAgent.getCameraMode() != CAMERA_MODE_MOUSELOOK ||
		!LLVOAvatar::sVisibleInFirstPerson)
	{
		skip_avatar_update = true;
	}

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(CAMERA_MODE_THIRD_PERSON);
	}

	// Store last_modelview of world camera
	LLMatrix4a last_modelview = gGLLastModelView;
	LLMatrix4a last_projection = gGLLastProjection;

	pushRenderTypeMask();
	andRenderTypeMask(RENDER_TYPE_SIMPLE,
					  RENDER_TYPE_ALPHA,
					  RENDER_TYPE_GRASS,
					  RENDER_TYPE_FULLBRIGHT,
					  RENDER_TYPE_BUMP,
					  RENDER_TYPE_VOLUME,
					  RENDER_TYPE_AVATAR,
					  RENDER_TYPE_PUPPET,
					  RENDER_TYPE_TREE,
					  RENDER_TYPE_TERRAIN,
					  RENDER_TYPE_WATER,
					  RENDER_TYPE_VOIDWATER,
					  RENDER_TYPE_PASS_ALPHA,
					  RENDER_TYPE_PASS_ALPHA_MASK,
					  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
					  RENDER_TYPE_PASS_GRASS,
					  RENDER_TYPE_PASS_SIMPLE,
					  RENDER_TYPE_PASS_BUMP,
					  RENDER_TYPE_PASS_FULLBRIGHT,
					  RENDER_TYPE_PASS_SHINY,
					  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
					  RENDER_TYPE_PASS_MATERIAL,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
					  RENDER_TYPE_PASS_SPECMAP,
					  RENDER_TYPE_PASS_SPECMAP_BLEND,
					  RENDER_TYPE_PASS_SPECMAP_MASK,
					  RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
					  RENDER_TYPE_PASS_NORMMAP,
					  RENDER_TYPE_PASS_NORMMAP_BLEND,
					  RENDER_TYPE_PASS_NORMMAP_MASK,
					  RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
					  RENDER_TYPE_PASS_NORMSPEC,
					  RENDER_TYPE_PASS_NORMSPEC_BLEND,
					  RENDER_TYPE_PASS_NORMSPEC_MASK,
					  RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
					  RENDER_TYPE_PASS_ALPHA_MASK_RIGGED,
					  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK_RIGGED,
					  RENDER_TYPE_PASS_SIMPLE_RIGGED,
					  RENDER_TYPE_PASS_BUMP_RIGGED,
					  RENDER_TYPE_PASS_FULLBRIGHT_RIGGED,
					  RENDER_TYPE_PASS_SHINY_RIGGED,
					  RENDER_TYPE_PASS_FULLBRIGHT_SHINY_RIGGED,
					  RENDER_TYPE_PASS_MATERIAL_RIGGED,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_RIGGED,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK_RIGGED,
					  RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED,
					  RENDER_TYPE_PASS_SPECMAP_RIGGED,
					  RENDER_TYPE_PASS_SPECMAP_BLEND_RIGGED,
					  RENDER_TYPE_PASS_SPECMAP_MASK_RIGGED,
					  RENDER_TYPE_PASS_SPECMAP_EMISSIVE_RIGGED,
					  RENDER_TYPE_PASS_NORMMAP_RIGGED,
					  RENDER_TYPE_PASS_NORMMAP_BLEND_RIGGED,
					  RENDER_TYPE_PASS_NORMMAP_MASK_RIGGED,
					  RENDER_TYPE_PASS_NORMMAP_EMISSIVE_RIGGED,
					  RENDER_TYPE_PASS_NORMSPEC_RIGGED,
					  RENDER_TYPE_PASS_NORMSPEC_BLEND_RIGGED,
					  RENDER_TYPE_PASS_NORMSPEC_MASK_RIGGED,
					  RENDER_TYPE_PASS_NORMSPEC_EMISSIVE_RIGGED,
					  END_RENDER_TYPES);

	gGL.setColorMask(false, false);

	// Get sun view matrix

	// Store current projection/modelview matrix
	const LLMatrix4a saved_proj = gGLProjection;
	const LLMatrix4a saved_view = gGLModelView;
	LLMatrix4a inv_view(saved_view);
	inv_view.invert();

	LLMatrix4a view[6];
	LLMatrix4a proj[6];

	// Clip contains parallel split distances for 3 splits
	LLVector3 clip = RenderShadowClipPlanes;

	LLVector3 caster_dir(mIsSunUp ? mSunDir : mMoonDir);

	// Far clip on last split is minimum of camera view distance and 128
	mSunClipPlanes = LLVector4(clip, clip.mV[2] * clip.mV[2] / clip.mV[1]);

	// Put together a universal "near clip" plane for shadow frusta
	LLPlane shadow_near_clip;
	{
		LLVector3 p = gAgent.getPositionAgent();
		p += caster_dir * RenderFarClip * 2.f;
		shadow_near_clip.setVec(p, caster_dir);
	}

	LLVector3 light_dir = -caster_dir;
	light_dir.normalize();

	// Create light space camera matrix

	LLVector3 at = light_dir;

	LLVector3 up = gViewerCamera.getAtAxis();

	if (fabsf(up * light_dir) > 0.75f)
	{
		up = gViewerCamera.getUpAxis();
	}

	up.normalize();
	at.normalize();

	LLCamera main_camera = gViewerCamera;

	bool no_shadow_frustra = !hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA);

	F32 near_clip = 0.f;
	{
		// Get visible point cloud
		main_camera.calcAgentFrustumPlanes(main_camera.mAgentFrustum);
		LLVector3 min, max;
		std::vector<LLVector3> fp;
		getVisiblePointCloud(main_camera, min, max, fp);
		if (fp.empty())
		{
			if (no_shadow_frustra)
			{
				mShadowCamera[0] = main_camera;
				mShadowExtents[0][0] = min;
				mShadowExtents[0][1] = max;

				mShadowFrustPoints[0].clear();
				mShadowFrustPoints[1].clear();
				mShadowFrustPoints[2].clear();
				mShadowFrustPoints[3].clear();
			}
			popRenderTypeMask();

			if (!skip_avatar_update)
			{
				gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
			}

			return;
		}

		LLVector4a v;
		// Get good split distances for frustum
		for (U32 i = 0; i < fp.size(); ++i)
		{
			v.load3(fp[i].mV);
			saved_view.affineTransform(v, v);
			fp[i].set(v.getF32ptr());
		}

		min = fp[0];
		max = fp[0];

		// Get camera space bounding box
		for (U32 i = 1; i < fp.size(); ++i)
		{
			update_min_max(min, max, fp[i]);
		}

		near_clip = llclamp(-max.mV[2], 0.01f, 4.0f);
		F32 far_clip = llclamp(-min.mV[2] * 2.f, 16.0f, 512.0f);
		far_clip = llmin(far_clip, gViewerCamera.getFar());

		F32 range = far_clip - near_clip;

		LLVector3 split_exp = RenderShadowSplitExponent;

		F32 da = 1.f - llmax(fabsf(light_dir * up),
							 fabsf(light_dir * gViewerCamera.getLeftAxis()));

		da = powf(da, split_exp.mV[2]);

		F32 sxp = split_exp.mV[1] + (split_exp.mV[0] - split_exp.mV[1]) * da;

		for (U32 i = 0; i < 4; ++i)
		{
			F32 x = (F32)(i + 1) * 0.25f;
			x = powf(x, sxp);
			mSunClipPlanes.mV[i] = near_clip + range * x;
		}

		// Bump back first split for transition padding
		mSunClipPlanes.mV[0] *= 1.25f;
	}

	// Convenience array of 4 near clip plane distances
	F32 dist[] = { near_clip,
				   mSunClipPlanes.mV[0],
				   mSunClipPlanes.mV[1],
				   mSunClipPlanes.mV[2],
				   mSunClipPlanes.mV[3] };

	if (mSunDiffuse == LLColor4::black)
	{
		// Sun diffuse is totally black, shadows do not matter
		LLGLDepthTest depth(GL_TRUE);

		for (S32 j = 0; j < 4; ++j)
		{
			mShadow[j].bindTarget();
			mShadow[j].clear();
			mShadow[j].flush();
		}
	}
	else
	{
		for (S32 j = 0; j < 4; ++j)
		{
			if (no_shadow_frustra)
			{
				mShadowFrustPoints[j].clear();
			}

			LLViewerCamera::sCurCameraID =
				LLViewerCamera::CAMERA_SUN_SHADOW0 + j;

			// Restore render matrices
			gGLModelView = saved_view;
			gGLProjection = saved_proj;

			LLVector3 eye = gViewerCamera.getOrigin();

			// Camera used for shadow cull/render
			LLCamera shadow_cam = gViewerCamera;
			shadow_cam.setFar(16.f);

			// Create world space camera frustum for this split
			LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
												true);

			LLVector3* frust = shadow_cam.mAgentFrustum;

			LLVector3 pn = shadow_cam.getAtAxis();

			// Construct 8 corners of split frustum section
			for (U32 i = 0; i < 4; ++i)
			{
				LLVector3 delta = frust[i + 4] - eye;
				delta += (frust[i + 4] - frust[(i + 2) % 4 + 4]) * 0.05f;
				delta.normalize();
				F32 dp = delta * pn;
				frust[i] = eye + (delta * dist[j] * 0.75f) / dp;
				frust[i + 4] = eye + (delta * dist[j + 1] * 1.25f) / dp;
			}

			shadow_cam.calcAgentFrustumPlanes(frust);
			shadow_cam.mFrustumCornerDist = 0.f;

			if (no_shadow_frustra)
			{
				mShadowCamera[j] = shadow_cam;
			}

			std::vector<LLVector3> fp;
			LLVector3 min, max;
			if (!getVisiblePointCloud(shadow_cam, min, max, fp, light_dir))
			{
				// No possible shadow receivers
				if (no_shadow_frustra)
				{
					mShadowExtents[j][0].clear();
					mShadowExtents[j][1].clear();
					mShadowCamera[j + 4] = shadow_cam;
				}

				mShadow[j].bindTarget();
				{
					LLGLDepthTest depth(GL_TRUE);
					mShadow[j].clear();
				}
				mShadow[j].flush();

				continue;
			}

			if (no_shadow_frustra)
			{
				mShadowExtents[j][0] = min;
				mShadowExtents[j][1] = max;
				mShadowFrustPoints[j] = fp;
			}

			// Find a good origin for shadow projection
			LLVector3 origin;

			// Get a temporary view projection
			view[j] = look_proj(gViewerCamera.getOrigin(), light_dir, -up);

			std::vector<LLVector3> wpf;

			LLVector4a p;
			for (U32 i = 0; i < fp.size(); ++i)
			{
				p.load3(fp[i].mV);
				view[j].affineTransform(p, p);
				wpf.emplace_back(LLVector3(p.getF32ptr()));
			}

			min = max = wpf[0];

			for (U32 i = 0; i < fp.size(); ++i)
			{
				// Get AABB in camera space
				update_min_max(min, max, wpf[i]);
			}

			// Construct a perspective transform with perspective along y-axis
			// that contains points in wpf
			// Known:
			// - far clip plane
			// - near clip plane
			// - points in frustum
			// Find:
			// - origin

			// Get some "interesting" points of reference
			LLVector3 center = (min + max) * 0.5f;
			LLVector3 size = (max - min) * 0.5f;
			LLVector3 near_center = center;
			near_center.mV[1] += size.mV[1] * 2.f;

			// Put all points in wpf in quadrant 0, reletive to center of
			// min/max get the best fit line using least squares

			for (U32 i = 0; i < wpf.size(); ++i)
			{
				wpf[i] -= center;
				wpf[i].mV[0] = fabsf(wpf[i].mV[0]);
				wpf[i].mV[2] = fabsf(wpf[i].mV[2]);
			}

			F32 bfm = 0.f;
			F32 bfb = 0.f;
			if (!wpf.empty())
			{
				F32 sx = 0.f;
				F32 sx2 = 0.f;
				F32 sy = 0.f;
				F32 sxy = 0.f;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					sx += wpf[i].mV[0];
					sx2 += wpf[i].mV[0] * wpf[i].mV[0];
					sy += wpf[i].mV[1];
					sxy += wpf[i].mV[0] * wpf[i].mV[1];
				}

				bfm = (sy * sx - wpf.size() * sxy) /
					  (sx * sx - wpf.size() * sx2);
				bfb = (sx * sxy - sy * sx2) / (sx * sx - bfm * sx2);
			}
			if (llisnan(bfm) || llisnan(bfb))
			{
				LL_DEBUGS("Pipeline") << "NaN found. Corresponding shadow rendering aborted. Camera ID: "
									  << LLViewerCamera::sCurCameraID
									  << LL_ENDL;
				continue;
			}

			{
				// Best fit line is y = bfm * x + bfb

				// Find point that is furthest to the right of line
				F32 off_x = -1.f;
				LLVector3 lp;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					// y = bfm * x + bfb
					// x = (y - bfb) / bfm
					F32 lx = (wpf[i].mV[1] - bfb) / bfm;

					lx = wpf[i].mV[0] - lx;

					if (off_x < lx)
					{
						off_x = lx;
						lp = wpf[i];
					}
				}

				// Get line with slope bfm through lp
				// bfb = y - bfm * x
				bfb = lp.mV[1] - bfm * lp.mV[0];

				// Calculate error
				F32 shadow_error = 0.f;
				for (U32 i = 0; i < wpf.size(); ++i)
				{
					F32 lx = (wpf[i].mV[1] - bfb) / bfm;
					shadow_error += fabsf(wpf[i].mV[0] - lx);
				}

				shadow_error /= wpf.size() * size.mV[0];

				if (llisnan(shadow_error) ||
					shadow_error > RenderShadowErrorCutoff)
				{
					// Just use ortho projection
					origin.clear();
					proj[j] = gl_ortho(min.mV[0], max.mV[0], min.mV[1],
									   max.mV[1], -max.mV[2], -min.mV[2]);
				}
				else
				{
					// Origin is where line x = 0;
					origin.set(0, bfb, 0);

					F32 fovz = 1.f;
					F32 fovx = 1.f;

					LLVector3 zp;
					LLVector3 xp;

					for (U32 i = 0; i < wpf.size(); ++i)
					{
						LLVector3 atz = wpf[i] - origin;
						atz.mV[0] = 0.f;
						atz.normalize();
						if (fovz > -atz.mV[1])
						{
							zp = wpf[i];
							fovz = -atz.mV[1];
						}

						LLVector3 atx = wpf[i] - origin;
						atx.mV[2] = 0.f;
						atx.normalize();
						if (fovx > -atx.mV[1])
						{
							fovx = -atx.mV[1];
							xp = wpf[i];
						}
					}

					fovx = acosf(fovx);
					fovz = acosf(fovz);

					F32 cutoff = RenderShadowFOVCutoff;

					if (fovx < cutoff && fovz > cutoff)
					{
						// x is a good fit, but z is too big, move away from zp
						// enough so that fovz matches cutoff
						F32 d = zp.mV[2] / tanf(cutoff);
						F32 ny = zp.mV[1] + fabsf(d);

						origin.mV[1] = ny;

						fovz = fovx = 1.f;

						for (U32 i = 0; i < wpf.size(); ++i)
						{
							LLVector3 atz = wpf[i] - origin;
							atz.mV[0] = 0.f;
							atz.normalize();
							fovz = llmin(fovz, -atz.mV[1]);

							LLVector3 atx = wpf[i] - origin;
							atx.mV[2] = 0.f;
							atx.normalize();
							fovx = llmin(fovx, -atx.mV[1]);
						}

						fovx = acosf(fovx);
						fovz = acosf(fovz);
					}

					origin += center;

					F32 ynear = origin.mV[1] - max.mV[1];
					F32 yfar = origin.mV[1] - min.mV[1];

					if (ynear < 0.1f) // keep a sensible near clip plane
					{
						F32 diff = 0.1f - ynear;
						origin.mV[1] += diff;
						ynear += diff;
						yfar += diff;
					}

					if (fovx > cutoff)
					{
						// Just use ortho projection
						origin.clear();
						proj[j] = gl_ortho(min.mV[0], max.mV[0], min.mV[1],
										   max.mV[1], -max.mV[2], -min.mV[2]);
					}
					else
					{
						// Get perspective projection
						view[j].invert();

						// Translate view to origin
						LLVector4a origin_agent;
						origin_agent.load3(origin.mV);
						view[j].affineTransform(origin_agent, origin_agent);

						eye = LLVector3(origin_agent.getF32ptr());

						view[j] = look_proj(LLVector3(origin_agent.getF32ptr()),
											light_dir, -up);

						F32 fx = 1.f / tanf(fovx);
						F32 fz = 1.f / tanf(fovz);
						const F32 y1 = (yfar + ynear) / (ynear - yfar);
						const F32 y3 = 2.f * yfar * ynear / (ynear - yfar);
						proj[j].setRow<0>(LLVector4a(-fx, 0.f, 0.f, 0.f));
						proj[j].setRow<1>(LLVector4a(0.f, y1, 0.f, -1.f));
						proj[j].setRow<2>(LLVector4a(0.f, 0.f, -fz, 0.f));
						proj[j].setRow<3>(LLVector4a(0.f, y3, 0.f, 0.f));				
					}
				}
			}

#if 0
			shadow_cam.setFar(128.f);
#endif
			if (llisnan(eye.mV[VX]) || llisnan(eye.mV[VY]) ||
				llisnan(eye.mV[VZ]))
			{
				LL_DEBUGS("Pipeline") << "NaN found in eye origin. Corresponding shadow rendering aborted. Camera ID: "
									  << LLViewerCamera::sCurCameraID
									  << LL_ENDL;
				continue;
			}
			shadow_cam.setOriginAndLookAt(eye, up, center);

			shadow_cam.setOrigin(0.f, 0.f, 0.f);

			gGLModelView = view[j];
			gGLProjection = proj[j];

			LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
												true);

			// shadow_cam.ignoreAgentFrustumPlane(LLCamera::AGENT_PLANE_NEAR);
			shadow_cam.agentPlane(LLCamera::AGENT_PLANE_NEAR).set(shadow_near_clip);

			gGLModelView = view[j];
			gGLProjection = proj[j];

			gGLLastModelView = mShadowModelview[j];
			gGLLastProjection = mShadowProjection[j];

			mShadowModelview[j] = view[j];
			mShadowProjection[j] = proj[j];

			mSunShadowMatrix[j].setMul(TRANS_MAT, proj[j]);
			mSunShadowMatrix[j].mulAffine(view[j]);
			mSunShadowMatrix[j].mulAffine(inv_view);

			mShadow[j].bindTarget();
			mShadow[j].getViewport(gGLViewport);
			mShadow[j].clear();

			static LLCullResult result[4];
			renderShadow(view[j], proj[j], shadow_cam, result[j], true, false,
						 mShadow[j].getWidth());

			mShadow[j].flush();

			if (no_shadow_frustra)
			{
				mShadowCamera[j + 4] = shadow_cam;
			}
		}
	}

	// HACK to disable projector shadows
	bool gen_shadow = RenderShadowDetail > 1;
	if (gen_shadow)
	{
		F32 fade_amt =
			gFrameIntervalSeconds *
			llmax(LLViewerCamera::getVelocityStat().getCurrentPerSec(), 1.f);

		// Update shadow targets
		for (U32 i = 0; i < 2; ++i)
		{
			// For each current shadow
			LLViewerCamera::sCurCameraID =
				LLViewerCamera::CAMERA_SPOT_SHADOW0 + i;

			if (mShadowSpotLight[i].notNull() &&
				(mShadowSpotLight[i] == mTargetShadowSpotLight[0] ||
				 mShadowSpotLight[i] == mTargetShadowSpotLight[1]))
			{
				// Keep this spotlight
				mSpotLightFade[i] = llmin(mSpotLightFade[i] + fade_amt, 1.f);
			}
			else
			{
				// Fade out this light
				mSpotLightFade[i] = llmax(mSpotLightFade[i] - fade_amt, 0.f);

				if (mSpotLightFade[i] == 0.f || mShadowSpotLight[i].isNull())
				{
					// Faded out, grab one of the pending spots (whichever one
					// is not already taken)
					if (mTargetShadowSpotLight[0] != mShadowSpotLight[(i + 1) % 2])
					{
						mShadowSpotLight[i] = mTargetShadowSpotLight[0];
					}
					else
					{
						mShadowSpotLight[i] = mTargetShadowSpotLight[1];
					}
				}
			}
		}

		for (S32 i = 0; i < 2; ++i)
		{
			gGLModelView = saved_view;
			gGLProjection = saved_proj;

			if (mShadowSpotLight[i].isNull())
			{
				continue;
			}

			LLVOVolume* volp = mShadowSpotLight[i]->getVOVolume();
			if (!volp)
			{
				mShadowSpotLight[i] = NULL;
				continue;
			}

			LLDrawable* drawablep = mShadowSpotLight[i];

			LLVector3 params = volp->getSpotLightParams();
			F32 fov = params.mV[0];

			// Get agent->light space matrix (modelview)
			LLVector3 center = drawablep->getPositionAgent();
			LLQuaternion quat = volp->getRenderRotation();

			// Get near clip plane
			LLVector3 scale = volp->getScale();
			LLVector3 at_axis(0.f, 0.f, -scale.mV[2] * 0.5f);
			at_axis *= quat;

			LLVector3 np = center + at_axis;
			at_axis.normalize();

			// Get origin that has given fov for plane np, at_axis, and given
			// scale
			F32 divisor = tanf(fov * 0.5f);
			// Seen happening and causing NaNs in setOrigin() below. HB
			if (divisor == 0.f) continue;
			F32 dist = (scale.mV[1] * 0.5f) / divisor;

			LLVector3 origin = np - at_axis * dist;

			LLMatrix4 mat(quat, LLVector4(origin, 1.f));

			view[i + 4].loadu(mat.getF32ptr());
			view[i + 4].invert();

			// Get perspective matrix
			F32 near_clip = dist + 0.01f;
			F32 width = scale.mV[VX];
			F32 height = scale.mV[VY];
			F32 far_clip = dist + volp->getLightRadius() * 1.5f;

			F32 fovy = fov * RAD_TO_DEG;
			F32 aspect = width / height;

			proj[i + 4] = gl_perspective(fovy, aspect, near_clip, far_clip);

			// Translate and scale from [-1, 1] to [0, 1]

			gGLModelView = view[i + 4];
			gGLProjection = proj[i + 4];

			mSunShadowMatrix[i + 4].setMul(TRANS_MAT, proj[i + 4]);
			mSunShadowMatrix[i + 4].mulAffine(view[i + 4]);
			mSunShadowMatrix[i + 4].mulAffine(inv_view);

			gGLLastModelView = mShadowModelview[i + 4];
			gGLLastProjection = mShadowProjection[i + 4];

			mShadowModelview[i + 4] = view[i + 4];
			mShadowProjection[i + 4] = proj[i + 4];

			LLCamera shadow_cam = gViewerCamera;
			shadow_cam.setFar(far_clip);
			shadow_cam.setOrigin(origin);

			LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false,
												true);

			mShadow[i + 4].bindTarget();
			mShadow[i + 4].getViewport(gGLViewport);
			mShadow[i + 4].clear();

			LLViewerCamera::sCurCameraID =
				LLViewerCamera::CAMERA_SPOT_SHADOW0 + i;

			sRenderSpotLight = drawablep;
			static LLCullResult result[2];
			renderShadow(view[i + 4], proj[i + 4], shadow_cam, result[i],
						 false, false, mShadow[i + 4].getWidth());
			sRenderSpotLight = NULL;

			mShadow[i + 4].flush();
 		}
	}
	else
	{
		// No spotlight shadows
		mShadowSpotLight[0] = mShadowSpotLight[1] = NULL;
	}

	if (!CameraOffset)
	{
		gGLModelView = saved_view;
		gGLProjection = saved_proj;
	}
	else
	{
		gGLModelView = view[1];
		gGLProjection = proj[1];
		gGL.loadMatrix(view[1]);
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.loadMatrix(proj[1]);
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
	gGL.setColorMask(true, false);

	gGLLastModelView = last_modelview;
	gGLLastProjection = last_projection;

	popRenderTypeMask();

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(gAgent.getCameraMode());
	}

	stop_glerror();
}

void LLPipeline::renderGroups(LLRenderPass* pass, U32 type, U32 mask,
							  bool texture)
{
	LLCullResult::sg_list_t& visible_groups = sCull->getVisibleGroups();
	for (U32 i = 0, count = visible_groups.size(); i < count; ++i)
	{
		LLSpatialGroup* group = visible_groups[i];
		if (group && !group->isDead() &&
			(!sUseOcclusion ||
			 !group->isOcclusionState(LLSpatialGroup::OCCLUDED)) &&
			hasRenderType(group->getSpatialPartition()->mDrawableType) &&
			group->mDrawMap.count(type))
		{
			pass->renderGroup(group, type, mask, texture);
		}
	}
}

void LLPipeline::renderRiggedGroups(LLRenderPass* pass, U32 type, U32 mask,
									bool texture)
{
	LLCullResult::sg_list_t& visible_groups = sCull->getVisibleGroups();
	for (U32 i = 0, count = visible_groups.size(); i < count; ++i)
	{
		LLSpatialGroup* group = visible_groups[i];
		if (group && !group->isDead() &&
			(!sUseOcclusion ||
			 !group->isOcclusionState(LLSpatialGroup::OCCLUDED)) &&
			hasRenderType(group->getSpatialPartition()->mDrawableType) &&
			group->mDrawMap.count(type))
		{
			pass->renderRiggedGroup(group, type, mask, texture);
		}
	}
}

void LLPipeline::generateImpostor(LLVOAvatar* avatarp)
{
	if (!avatarp || avatarp->isDead() || !avatarp->mDrawable)
	{
		return;
	}

	LL_GL_CHECK_STATES;

	static LLCullResult result;
	result.clear();
	grabReferences(result);

	pushRenderTypeMask();

	bool visually_muted = avatarp->isVisuallyMuted();
//MK
	bool vision_restricted = gRLenabled && gRLInterface.mVisionRestricted;
	if (vision_restricted)
	{
		// Render everything on impostors
		andRenderTypeMask(RENDER_TYPE_ALPHA,
						  RENDER_TYPE_FULLBRIGHT,
						  RENDER_TYPE_VOLUME,
						  RENDER_TYPE_GLOW,
						  RENDER_TYPE_BUMP,
						  RENDER_TYPE_PASS_SIMPLE,
						  RENDER_TYPE_PASS_ALPHA,
						  RENDER_TYPE_PASS_ALPHA_MASK,
						  RENDER_TYPE_PASS_BUMP,
						  RENDER_TYPE_PASS_POST_BUMP,
						  RENDER_TYPE_PASS_FULLBRIGHT,
						  RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
						  RENDER_TYPE_PASS_GLOW,
						  RENDER_TYPE_PASS_GRASS,
						  RENDER_TYPE_PASS_SHINY,
						  RENDER_TYPE_PASS_MATERIAL,
						  RENDER_TYPE_PASS_MATERIAL_ALPHA,
						  RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
						  RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
						  RENDER_TYPE_PASS_SPECMAP,
						  RENDER_TYPE_PASS_SPECMAP_BLEND,
						  RENDER_TYPE_PASS_SPECMAP_MASK,
						  RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
						  RENDER_TYPE_PASS_NORMMAP,
						  RENDER_TYPE_PASS_NORMMAP_BLEND,
						  RENDER_TYPE_PASS_NORMMAP_MASK,
						  RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
						  RENDER_TYPE_PASS_NORMSPEC,
						  RENDER_TYPE_PASS_NORMSPEC_BLEND,
						  RENDER_TYPE_PASS_NORMSPEC_MASK,
						  RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
						  RENDER_TYPE_AVATAR,
						  RENDER_TYPE_PUPPET,
						  RENDER_TYPE_ALPHA_MASK,
						  RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
						  RENDER_TYPE_SIMPLE,
						  RENDER_TYPE_MATERIALS,
						  END_RENDER_TYPES);
	}
	else
//mk
	if (visually_muted)
	{
		// Only show jelly doll geometry
		andRenderTypeMask(RENDER_TYPE_AVATAR, RENDER_TYPE_PUPPET,
						  END_RENDER_TYPES);
	}
	else if (gUsePBRShaders)
	{
		clearRenderTypeMask(RENDER_TYPE_SKY,
							RENDER_TYPE_WL_SKY,
							RENDER_TYPE_TERRAIN,
							RENDER_TYPE_GRASS,
							RENDER_TYPE_PUPPET, // Animesh
							RENDER_TYPE_TREE,
							RENDER_TYPE_VOIDWATER,
							RENDER_TYPE_WATER,
							RENDER_TYPE_ALPHA_PRE_WATER,
							RENDER_TYPE_PASS_GRASS,
							RENDER_TYPE_HUD,
							RENDER_TYPE_PARTICLES,
							RENDER_TYPE_CLOUDS,
							RENDER_TYPE_HUD_PARTICLES,
							END_RENDER_TYPES);
	}
	else
	{
		// Hide world geometry
		clearRenderTypeMask(RENDER_TYPE_SKY,
							RENDER_TYPE_WL_SKY,
							RENDER_TYPE_TERRAIN,
							RENDER_TYPE_GRASS,
							RENDER_TYPE_PUPPET, // Animesh
							RENDER_TYPE_TREE,
							RENDER_TYPE_VOIDWATER,
							RENDER_TYPE_WATER,
							RENDER_TYPE_PASS_GRASS,
							RENDER_TYPE_HUD,
							RENDER_TYPE_PARTICLES,
							RENDER_TYPE_CLOUDS,
							RENDER_TYPE_HUD_PARTICLES,
							END_RENDER_TYPES);
	}

	LLDisableOcclusionCulling no_occlusion;

	sReflectionRender = !sRenderDeferred;
	sShadowRender = sImpostorRender = true;

	{
		LL_FAST_TIMER(FTM_IMPOSTOR_MARK_VISIBLE);
		markVisible(avatarp->mDrawable, gViewerCamera);
		LLVOAvatar::sUseImpostors = false;

		for (S32 i = 0, count = avatarp->mAttachedObjectsVector.size();
			 i < count; ++i)
		{
			LLViewerObject* object = avatarp->mAttachedObjectsVector[i].first;
			if (object)
			{
				markVisible(object->mDrawable->getSpatialBridge(),
							gViewerCamera);
			}
		}
	}

	stateSort(gViewerCamera, result);

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	LLCamera camera = gViewerCamera;
	LLVector2 tdim;
	U32 res_y = 0;
	U32 res_x = 0;

	{
		LL_FAST_TIMER(FTM_IMPOSTOR_SETUP);
		const LLVector4a* ext = avatarp->mDrawable->getSpatialExtents();
		LLVector3 pos = avatarp->getRenderPosition() +
						avatarp->getImpostorOffset();
		camera.lookAt(gViewerCamera.getOrigin(), pos,
					  gViewerCamera.getUpAxis());

		LLVector4a half_height;
		half_height.setSub(ext[1], ext[0]);
		half_height.mul(0.5f);

		LLVector4a left;
		left.load3(camera.getLeftAxis().mV);
		left.mul(left);
		left.normalize3fast();

		LLVector4a up;
		up.load3(camera.getUpAxis().mV);
		up.mul(up);
		up.normalize3fast();

		tdim.mV[0] = fabsf(half_height.dot3(left).getF32());
		tdim.mV[1] = fabsf(half_height.dot3(up).getF32());

		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();

		F32 distance = (pos - camera.getOrigin()).length();
		F32 fov = atanf(tdim.mV[1] / distance) * 2.f * RAD_TO_DEG;
		F32 aspect = tdim.mV[0] / tdim.mV[1];
		LLMatrix4a persp = gl_perspective(fov, aspect, .001f, 256.f);
		gGLProjection = persp;
		gGL.loadMatrix(persp);

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();
		LLMatrix4a mat;
		camera.getOpenGLTransform(mat.getF32ptr());

		mat.setMul(OGL_TO_CFR_ROT4A, mat);

		gGL.loadMatrix(mat);
		gGLModelView = mat;

		glClearColor(0.f, 0.f, 0.f, 0.f);
		gGL.setColorMask(true, true);

		// Get the number of pixels per angle
		F32 pa = gViewerWindowp->getWindowDisplayHeight() /
				 (RAD_TO_DEG * gViewerCamera.getView());

		// Get resolution based on angle width and height of impostor (double
		// desired resolution to prevent aliasing)
		res_y = llmin(nhpo2((U32)(fov * pa)), 512U);
		res_x = llmin(nhpo2((U32)(atanf(tdim.mV[0] / distance) * 2.f *
								 RAD_TO_DEG * pa)),
					  512U);

		if (!avatarp->mImpostor.isComplete())
		{
			LL_FAST_TIMER(FTM_IMPOSTOR_ALLOCATE);
			if (gUsePBRShaders)
			{
				avatarp->mImpostor.allocate(res_x, res_y, GL_RGBA, true);
			}
			else
			{
				U32 format = sRenderDeferred ? GL_SRGB8_ALPHA8 : GL_RGBA;
				avatarp->mImpostor.allocate(res_x, res_y, format, true, false);
			}
			if (sRenderDeferred)
			{
				addDeferredAttachments(avatarp->mImpostor);
			}
			unit0->bind(&avatarp->mImpostor);
			unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
		}
		else if (res_x != avatarp->mImpostor.getWidth() ||
				 res_y != avatarp->mImpostor.getHeight())
		{
			LL_FAST_TIMER(FTM_IMPOSTOR_RESIZE);
			avatarp->mImpostor.resize(res_x, res_y);
		}

		avatarp->mImpostor.bindTarget();

		stop_glerror();
	}

	F32 old_alpha = LLDrawPoolAvatar::sMinimumAlpha;

	if (visually_muted)
	{
		// Disable alpha masking for muted avatars (get whole skin silhouette)
		LLDrawPoolAvatar::sMinimumAlpha = 0.f;
	}

	if (sRenderDeferred)
	{
		avatarp->mImpostor.clear();
		renderGeomDeferred(camera);
		renderGeomPostDeferred(camera);

		// Shameless hack time: render it all again, this time writing the
		// depth values we need to generate the alpha mask below while
		// preserving the alpha-sorted color rendering from the previous pass.

		sImpostorRenderAlphaDepthPass = true;

		// Depth-only here...
		gGL.setColorMask(false, false);
		renderGeomPostDeferred(camera);

		sImpostorRenderAlphaDepthPass = false;
	}
	else
	{
		LLGLEnable scissor(GL_SCISSOR_TEST);
		glScissor(0, 0, res_x, res_y);
		avatarp->mImpostor.clear();
		renderGeom(camera);

		// Shameless hack time: render it all again, this time writing the
		// depth values we need to generate the alpha mask below while
		// preserving the alpha-sorted color rendering from the previous pass.

		sImpostorRenderAlphaDepthPass = true;

		// Depth-only here...
		gGL.setColorMask(false, false);
		renderGeom(camera);

		sImpostorRenderAlphaDepthPass = false;
	}

	LLDrawPoolAvatar::sMinimumAlpha = old_alpha;

	{
		// Create alpha mask based on depth buffer (grey out if muted)
		LL_FAST_TIMER(FTM_IMPOSTOR_BACKGROUND);
		if (sRenderDeferred)
		{
			GLuint buff = GL_COLOR_ATTACHMENT0;
			glDrawBuffers(1, &buff);
		}

		LLGLDisable blend(vision_restricted ? 0 : GL_BLEND); // mk

		if (visually_muted)
		{
			gGL.setColorMask(true, true);
		}
		else
		{
			gGL.setColorMask(false, true);
		}

		unit0->unbind(LLTexUnit::TT_TEXTURE);

		LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);

		gGL.flush();

		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();

		constexpr F32 clip_plane = 0.99999f;

		gDebugProgram.bind();

		LLColor4 muted_color(avatarp->getMutedAVColor());
		gGL.diffuseColor4fv(muted_color.mV);

		gGL.begin(LLRender::TRIANGLES);
		{
			gGL.vertex3f(-1.f, -1.f, clip_plane);
			gGL.vertex3f(1.f, -1.f, clip_plane);
			gGL.vertex3f(1.f, 1.f, clip_plane);
			gGL.vertex3f(-1.f, -1.f, clip_plane);
			gGL.vertex3f(1.f, 1.f, clip_plane);
			gGL.vertex3f(-1.f, 1.f, clip_plane);
		}
		gGL.end(true);

		gDebugProgram.unbind();

		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();

		stop_glerror();
	}

	avatarp->mImpostor.flush();

	avatarp->setImpostorDim(tdim);

	LLVOAvatar::sUseImpostors = LLVOAvatar::sMaxNonImpostors != 0;
	sReflectionRender = false;
	sImpostorRender = false;
	sShadowRender = false;
	popRenderTypeMask();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	avatarp->mNeedsImpostorUpdate = false;
	avatarp->cacheImpostorValues();

	LLVertexBuffer::unbind();
	LL_GL_CHECK_STATES;
}

void LLPipeline::previewAvatar(LLVOAvatar* avatarp)
{
	LL_TRACY_TIMER(TRC_PREVIEW_AVATAR);

	if (!avatarp || avatarp->isDead() || !avatarp->mDrawable)
	{
		llwarns_once << "Avatar is " << (avatarp ? "not drawable" : "NULL")
					 << llendl;
		return;
	}

	gGL.flush();
	stop_glerror();

	LLGLDepthTest gls_depth(GL_TRUE);

	LLGLSDefault gls_default;
	gGL.setSceneBlendType(LLRender::BT_REPLACE);

	LL_GL_CHECK_STATES;

	static LLCullResult result;
	result.clear();
	grabReferences(result);

	pushRenderTypeMask();
	// Hide world geometry
	if (gUsePBRShaders)
	{
		clearRenderTypeMask(RENDER_TYPE_SKY,
							RENDER_TYPE_WL_SKY,
							RENDER_TYPE_TERRAIN,
							RENDER_TYPE_GRASS,
							RENDER_TYPE_PUPPET, // Animesh
							RENDER_TYPE_TREE,
							RENDER_TYPE_VOIDWATER,
							RENDER_TYPE_WATER,
							RENDER_TYPE_ALPHA_PRE_WATER,
							RENDER_TYPE_PASS_GRASS,
							RENDER_TYPE_HUD,
							RENDER_TYPE_PARTICLES,
							RENDER_TYPE_CLOUDS,
							RENDER_TYPE_HUD_PARTICLES,
							END_RENDER_TYPES);
	}
	else
	{
		clearRenderTypeMask(RENDER_TYPE_SKY,
							RENDER_TYPE_WL_SKY,
							RENDER_TYPE_TERRAIN,
							RENDER_TYPE_GRASS,
							RENDER_TYPE_PUPPET, // Animesh
							RENDER_TYPE_TREE,
							RENDER_TYPE_VOIDWATER,
							RENDER_TYPE_WATER,
							RENDER_TYPE_PASS_GRASS,
							RENDER_TYPE_HUD,
							RENDER_TYPE_PARTICLES,
							RENDER_TYPE_CLOUDS,
							RENDER_TYPE_HUD_PARTICLES,
							END_RENDER_TYPES);
	}

	LLDisableOcclusionCulling no_occlusion;

	sReflectionRender = sImpostorRender = sShadowRender = false;

	markVisible(avatarp->mDrawable, gViewerCamera);

	static LLCachedControl<bool> with_rigged_meshes(gSavedSettings,
													"PreviewAvatarWithRigged");
	if (with_rigged_meshes)
	{
		// Only show rigged attachments for preview, for the sake of
		// performance, and so that static objects would not obstruct
		// previewing changes.
		for (U32 i = 0, count = avatarp->mAttachedObjectsVector.size();
			 i < count; ++i)
		{
			LLViewerObject* objectp = avatarp->mAttachedObjectsVector[i].first;
			if (!objectp || objectp->isDead())
			{
				continue;
			}
			bool is_rigged_mesh = objectp->isRiggedMesh();
			if (!is_rigged_mesh)
			{
				// Sometimes object is a linkset and rigged mesh is a child
				LLViewerObject::const_child_list_t& clist =
					objectp->getChildren();
				for (LLViewerObject::const_child_list_t::const_iterator
						it = clist.begin(), end = clist.end();
					 it != end; ++it)
				{
					const LLViewerObject* childp = *it;
					if (childp && childp->isRiggedMesh())
					{
						is_rigged_mesh = true;
						break;
					}
				}
			}
			if (is_rigged_mesh)
			{
				LLDrawable* drawablep = objectp->mDrawable;
				if (drawablep && !drawablep->isDead())
				{
					markVisible(drawablep->getSpatialBridge(), gViewerCamera);
				}
			}
		}
	}

	stateSort(gViewerCamera, result);

	F32 old_alpha = LLDrawPoolAvatar::sMinimumAlpha;
	LLDrawPoolAvatar::sMinimumAlpha = 0.f;
	if (sRenderDeferred)
	{
		renderGeomDeferred(gViewerCamera);
		renderGeomPostDeferred(gViewerCamera);
	}
	else
	{
		renderGeom(gViewerCamera);
	}
	LLDrawPoolAvatar::sMinimumAlpha = old_alpha;

	// Create an alpha mask based on depth buffer
	{
		if (sRenderDeferred && !gUsePBRShaders)
		{
			GLuint buff = GL_COLOR_ATTACHMENT0;
			glDrawBuffers(1, &buff);
		}

		LLGLDisable blend(GL_BLEND);
		gGL.setColorMask(false, true);

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);

		gGL.flush();

		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();

		gDebugProgram.bind();
		gGL.begin(LLRender::TRIANGLES);
		constexpr F32 clip_plane = 0.99999f;
		gGL.vertex3f(-1.f, -1.f, clip_plane);
		gGL.vertex3f(-1.f, 1.f, clip_plane);
		gGL.vertex3f(1.f, -1.f, clip_plane);
		gGL.vertex3f(1.f, -1.f, clip_plane);
		gGL.vertex3f(-1.f, 1.f, clip_plane);
		gGL.vertex3f(1.f, 1.f, clip_plane);
		gGL.end(true);
		gDebugProgram.unbind();

		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();
	}

	popRenderTypeMask();

	LLVertexBuffer::unbind();
	LL_GL_CHECK_STATES;

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	gGL.flush();
}

void LLPipeline::setRenderTypeMask(U32 type, ...)
{
	va_list args;

	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		mRenderTypeEnabled[type] = true;
		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		llerrs << "Invalid render type." << llendl;
	}
}

bool LLPipeline::hasAnyRenderType(U32 type, ...) const
{
	va_list args;
	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		if (mRenderTypeEnabled[type])
		{
			va_end(args);
			return true;
		}
		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		llerrs << "Invalid render type." << llendl;
	}

	return false;
}

void LLPipeline::pushRenderTypeMask()
{
	mRenderTypeEnableStack.emplace((const char*)mRenderTypeEnabled,
								   sizeof(mRenderTypeEnabled));
}

void LLPipeline::popRenderTypeMask()
{
	if (mRenderTypeEnableStack.empty())
	{
		llerrs << "Depleted render type stack." << llendl;
	}

	memcpy(mRenderTypeEnabled, mRenderTypeEnableStack.top().data(),
		   sizeof(mRenderTypeEnabled));
	mRenderTypeEnableStack.pop();
}

void LLPipeline::andRenderTypeMask(U32 type, ...)
{
	bool tmp[NUM_RENDER_TYPES];
	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		tmp[i] = false;
	}

	va_list args;
	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		if (mRenderTypeEnabled[type])
		{
			tmp[type] = true;
		}

		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		llerrs << "Invalid render type." << llendl;
	}

	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		mRenderTypeEnabled[i] = tmp[i];
	}
}

void LLPipeline::clearRenderTypeMask(U32 type, ...)
{
	va_list args;
	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		mRenderTypeEnabled[type] = false;

		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		llerrs << "Invalid render type." << llendl;
	}
}

void LLPipeline::setAllRenderTypes()
{
	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		mRenderTypeEnabled[i] = true;
	}
}

void LLPipeline::addDebugBlip(const LLVector3& position, const LLColor4& color)
{
	mDebugBlips.emplace_back(position, color);
}

class LLOctreeDirtyInfo : public OctreeTraveler
{
public:
	void visit(const OctreeNode* nodep) override
	{
		LLSpatialGroup* groupp = (LLSpatialGroup*)nodep->getListener(0);
		if (groupp->getSpatialPartition()->mRenderByGroup)
		{
			groupp->setState(LLSpatialGroup::GEOM_DIRTY);
			gPipeline.markRebuild(groupp);

			LLSpatialGroup::bridge_list_t& blist = groupp->mBridgeList;
			for (U32 i = 0, count = blist.size(); i < count; ++i)
			{
				traverse(blist[i]->mOctree);
			}
		}
	}
};

void LLPipeline::rebuildDrawInfo()
{
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;

		LLOctreeDirtyInfo dirty;

		LLSpatialPartition* partp =
			regionp->getSpatialPartition(LLViewerRegion::PARTITION_VOLUME);
		dirty.traverse(partp->mOctree);

		partp = regionp->getSpatialPartition(LLViewerRegion::PARTITION_BRIDGE);
		dirty.traverse(partp->mOctree);
	}
}
