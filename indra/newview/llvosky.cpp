/**
 * @file llvosky.cpp
 * @brief LLVOSky class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llvosky.h"

#include "imageids.h"
#include "llcubemap.h"
#include "llfasttimer.h"
#include "llgl.h"

#include "llagent.h"
#include "lldrawable.h"
#include "lldrawpoolsky.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolwlsky.h"
#include "llenvironment.h"
#include "llface.h"
#include "llfeaturemanager.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llworld.h"

// Constants

// Exported
const F32 NIGHTTIME_ELEVATION_COS = sinf(NIGHTTIME_ELEVATION * DEG_TO_RAD);

constexpr S32 NUM_TILES_X = 8;
constexpr S32 NUM_TILES_Y = 4;
constexpr S32 NUM_TILES = NUM_TILES_X * NUM_TILES_Y;
// Amortize updating face; see sTileResX
constexpr S32 UPDATE_TILES = NUM_TILES / 8;
constexpr S32 NUM_CUBEMAP_FACES = 6;
constexpr S32 TOTAL_TILES = NUM_CUBEMAP_FACES * NUM_TILES;
constexpr S32 MAX_TILES = TOTAL_TILES + 1;

// Heavenly body constants
constexpr F32 SUN_DISK_RADIUS = 0.5f;
constexpr F32 MOON_DISK_RADIUS = SUN_DISK_RADIUS * 0.9f;
constexpr F32 SUN_INTENSITY = 1e5f;

// Texture coordinates:
const LLVector2 TEX00 = LLVector2(0.f, 0.f);
const LLVector2 TEX01 = LLVector2(0.f, 1.f);
const LLVector2 TEX10 = LLVector2(1.f, 0.f);
const LLVector2 TEX11 = LLVector2(1.f, 1.f);

constexpr F32 UPDATE_EXPIRY = 0.05f;
constexpr F32 UPDATE_MIN_DELTA_THRESHOLD = 0.001f;

// Exported globals
LLUUID gSunTextureID = IMG_SUN;
LLUUID gMoonTextureID = IMG_MOON;

// Helpers

bool almost_equal(const F32& a, const F32& b)
{
	F32 diff = fabs(a - b);
	if (diff < F_APPROXIMATELY_ZERO)
	{
		return true;
	}
	if (diff < llmax(fabs(a), fabs(b)) * UPDATE_MIN_DELTA_THRESHOLD)
	{
		return true;
	}
	return false;
}

bool almost_equal(const LLColor3& a, const LLColor3& b)
{
	return almost_equal((F32)a.mV[0], (F32)b.mV[0]) &&
		   almost_equal((F32)a.mV[1], (F32)b.mV[1]) &&
		   almost_equal((F32)a.mV[2], (F32)b.mV[2]);
}

bool almost_equal(const LLVector4& a, const LLVector4& b)
{
	return almost_equal((F32)a.mV[0], (F32)b.mV[0]) &&
		   almost_equal((F32)a.mV[1], (F32)b.mV[1]) &&
		   almost_equal((F32)a.mV[2], (F32)b.mV[2]) &&
		   almost_equal((F32)a.mV[3], (F32)b.mV[3]);
}

// Clip quads with top and bottom sides parallel to horizon.

F32 clip_side_to_horizon(const LLVector3& V0, const LLVector3& V1,
						 F32 cos_max_angle)
{
	const LLVector3 V = V1 - V0;
	const F32 k2 = 1.f / (cos_max_angle * cos_max_angle) - 1.f;
	const F32 A = V.mV[0] * V.mV[0] + V.mV[1] * V.mV[1] -
				  k2 * V.mV[2] * V.mV[2];
	const F32 B = V0.mV[0] * V.mV[0] + V0.mV[1] * V.mV[1] -
				  k2 * V0.mV[2] * V.mV[2];
	const F32 C = V0.mV[0] * V0.mV[0] + V0.mV[1] * V0.mV[1] -
				  k2 * V0.mV[2] * V0.mV[2];

	if (fabsf(A) < 1e-7f)
	{
		return -0.1f; // v0 is cone origin and v1 is on the surface of the cone
	}

	const F32 det = sqrtf(B * B - A * C);
	const F32 t1 = (-B - det) / A;
	const F32 t2 = (-B + det) / A;
	const F32 z1 = V0.mV[2] + t1 * V.mV[2];
	const F32 z2 = V0.mV[2] + t2 * V.mV[2];
	if (z1 * cos_max_angle < 0.f)
	{
		return t2;
	}
	else if (z2 * cos_max_angle < 0.f)
	{
		return t1;
	}
	else if (t1 < 0.f || t1 > 1.f)
	{
		return t2;
	}
	else
	{
		return t1;
	}
}

class LLFastLn
{
public:
	LLFastLn()
	{
		mTable[0] = 0.f;
		for (S32 i = 1; i < 257; ++i)
		{
			mTable[i] = logf((F32)i);
		}
	}

	F32 ln(F32 x)
	{
		constexpr F32 OO_255 = 0.003921568627450980392156862745098f;
		constexpr F32 LN_255 = 5.5412635451584261462455391880218f;

		if (x < OO_255)
		{
			return logf(x);
		}
		else if (x < 1.f)
		{
			x *= 255.f;
			S32 index = llfloor(x);
			F32 t = x - (F32)index;
			F32 low = mTable[index];
			F32 high = mTable[index + 1];
			return low + t * (high - low) - LN_255;
		}
		else if (x <= 255.f)
		{
			S32 index = llfloor(x);
			F32 t = x - (F32)index;
			F32 low = mTable[index];
			F32 high = mTable[index + 1];
			return low + t * (high - low);
		}
		else
		{
			return logf(x);
		}
	}

	F32 pow(F32 x, F32 y)
	{
		return (F32)LL_FAST_EXP(y * ln(x));
	}

private:
	F32 mTable[257]; // index 0 is unused
};

static LLFastLn gFastLn;

// Functions used a lot.

LL_INLINE void color_pow(LLColor3& col, F32 e)
{
	col.mV[0] = gFastLn.pow(col.mV[0], e);
	col.mV[1] = gFastLn.pow(col.mV[1], e);
	col.mV[2] = gFastLn.pow(col.mV[2], e);
}

LL_INLINE LLColor3 componentPowF(const LLColor3& v, F32 exponent)
{
	return LLColor3(gFastLn.pow(v.mV[0], exponent),
					gFastLn.pow(v.mV[1], exponent),
					gFastLn.pow(v.mV[2], exponent));
}

LL_INLINE LLColor3 color_norm(const LLColor3& col)
{
	const F32 m = color_max(col);
	if (m > 1.f)
	{
		return 1.f / m * col;
	}
	else return col;
}

LL_INLINE void color_gamma_correct(LLColor3& col)
{
	constexpr F32 gamma_inv = 1.f / 1.2f;
	if (col.mV[0] != 0.f)
	{
		col.mV[0] = gFastLn.pow(col.mV[0], gamma_inv);
	}
	if (col.mV[1] != 0.f)
	{
		col.mV[1] = gFastLn.pow(col.mV[1], gamma_inv);
	}
	if (col.mV[2] != 0.f)
	{
		col.mV[2] = gFastLn.pow(col.mV[2], gamma_inv);
	}
}

/***************************************
		SkyTex
***************************************/

S32 LLSkyTex::sComponents = 4;
S32 LLSkyTex::sResolution = 64;
F32 LLSkyTex::sInterpVal = 0.f;
S32 LLSkyTex::sCurrent = 0;

LLSkyTex::LLSkyTex()
:	mSkyData(NULL),
	mSkyDirs(NULL),
	mIsShiny(false)
{
}

void LLSkyTex::init(bool shiny)
{
	mIsShiny = shiny;
	mSkyData = new LLColor4U[sResolution * sResolution];
	mSkyDirs = new LLVector3[sResolution * sResolution];

	for (S32 i = 0; i < 2; ++i)
	{
		mTexture[i] = LLViewerTextureManager::getLocalTexture(false);
		mTexture[i]->setAddressMode(LLTexUnit::TAM_CLAMP);
		mImageRaw[i] = new LLImageRaw(sResolution, sResolution, sComponents);

		initEmpty(i);
	}
}

void LLSkyTex::cleanupGL()
{
	mTexture[0] = NULL;
	mTexture[1] = NULL;
}

void LLSkyTex::restoreGL()
{
	for (S32 i = 0; i < 2; ++i)
	{
		mTexture[i] = LLViewerTextureManager::getLocalTexture(false);
		mTexture[i]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}
}

LLSkyTex::~LLSkyTex()
{
	delete[] mSkyData;
	mSkyData = NULL;

	delete[] mSkyDirs;
	mSkyDirs = NULL;
}

void LLSkyTex::initEmpty(S32 tex)
{
	U8* data = mImageRaw[tex]->getData();
	if (data)
	{
		for (S32 i = 0; i < sResolution; ++i)
		{
			for (S32 j = 0; j < sResolution; ++j)
			{
				const S32 basic_offset = i * sResolution + j;
				S32 offset = basic_offset * sComponents;
				data[offset] = 0;
				data[++offset] = 0;
				data[++offset] = 0;
				data[++offset] = 255;

				mSkyData[basic_offset].setToBlack();
			}
		}
	}

	createGLImage(tex);
}

void LLSkyTex::create()
{
	U8* data = mImageRaw[sCurrent]->getData();
	if (data)
	{
		for (S32 i = 0; i < sResolution; ++i)
		{
			for (S32 j = 0; j < sResolution; ++j)
			{
				const S32 basic_offset = i * sResolution + j;
				S32 offset = basic_offset * sComponents;
				U32* pix = (U32*)(data + offset);
				*pix = mSkyData[basic_offset].asRGBA();
			}
		}
	}

	createGLImage(sCurrent);
}

void LLSkyTex::createGLImage(S32 which)
{
	mTexture[which]->setExplicitFormat(GL_RGBA8, GL_RGBA);
	mTexture[which]->createGLTexture(0, mImageRaw[which], 0, true);
	mTexture[which]->setAddressMode(LLTexUnit::TAM_CLAMP);
}

void LLSkyTex::bindTexture(bool curr)
{
	gGL.getTexUnit(0)->bind(mTexture[getWhich(curr)]);
}

/***************************************
		Sky
***************************************/

F32	LLHeavenBody::sInterpVal = 0.f;

S32 LLVOSky::sResolution = LLSkyTex::getResolution();
S32 LLVOSky::sTileResX = sResolution / NUM_TILES_X;
S32 LLVOSky::sTileResY = sResolution / NUM_TILES_Y;

LLVOSky::LLVOSky(const LLUUID& id, LLViewerRegion* regionp)
:	LLStaticViewerObject(id, LL_VO_SKY, regionp, true),
	mSun(SUN_DISK_RADIUS),
	mMoon(MOON_DISK_RADIUS),
	mBrightnessScale(1.f),
	mBrightnessScaleNew(0.f),
	mBrightnessScaleGuess(1.f),
	mCloudDensity(0.2f),
	mWind(0.f),
	mWorldScale(1.f),
	mSunScale(1.f),
	mMoonScale(1.f),
	mInterpVal(0.f),
	mAtmHeight(ATM_HEIGHT),
	mBumpSunDir(0.f, 0.f, 1.f),
	mDrawRefl(0),
	mDomeRadius(SKY_DOME_RADIUS),
	mDomeOffset(SKY_DOME_OFFSET),
	mGamma(1.f),
	mHazeDensity(0.f),
	mDensityMultiplier(0.f),
	mCloudShadow(0.f),
	mMaxY(0.f),
	mHazeHorizon(1.f),
	mCubeMapUpdateStage(-1),
	mCubeMapUpdateTile(0),
	mWeatherChange(false),
	mForceUpdate(false),
	mNeedUpdate(true),
	mHeavenlyBodyUpdated(false),
	mInitialized(false)
{
	mCanSelect = false;

	mUpdateTimer.reset();
	mForceUpdateThrottle.setTimerExpirySec(UPDATE_EXPIRY);
	mForceUpdateThrottle.reset();

	for (S32 i = 0; i < NUM_CUBEMAP_FACES; ++i)
	{
		mSkyTex[i].init(false);
		mShinyTex[i].init(true);
	}
	for (S32 i = 0; i < FACE_COUNT; ++i)
	{
		mFace[i] = NULL;
	}

	mCameraPosAgent = gAgent.getCameraPositionAgent();
	mEarthCenter = LLVector3(mCameraPosAgent.mV[0], mCameraPosAgent.mV[1],
							 -EARTH_RADIUS);

	mSun.setIntensity(SUN_INTENSITY);
	mMoon.setIntensity(SUN_INTENSITY *
					   gSavedSettings.getF32("RenderMoonLightIntensity"));

	mAmbientScale = gSavedSettings.getF32("SkyAmbientScale");
	mNightColorShift = gSavedSettings.getColor3("SkyNightColorShift");
	mFogColor.mV[VRED] = mFogColor.mV[VGREEN] = mFogColor.mV[VBLUE] = 0.5f;
	mFogColor.mV[VALPHA] = 0.f;

	if (gSunTextureID != IMG_SUN ||
		LLViewerFetchedTexture::sDefaultSunImagep.isNull())
	{
		mSunTexturep[0] =
			LLViewerTextureManager::getFetchedTexture(gSunTextureID,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_UI);
		mSunTexturep[0]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}
	else
	{
		mSunTexturep[0] = LLViewerFetchedTexture::sDefaultSunImagep;
	}

	if (gMoonTextureID != IMG_MOON ||
		LLViewerFetchedTexture::sDefaultMoonImagep.isNull())
	{
		mMoonTexturep[0] =
			LLViewerTextureManager::getFetchedTexture(gMoonTextureID,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_UI);
		mMoonTexturep[0]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}
	else
	{
		mMoonTexturep[0] = LLViewerFetchedTexture::sDefaultMoonImagep;
	}

	mBloomTexturep[0] = LLViewerFetchedTexture::sBloomImagep;

	mCloudNoiseTexturep[0] = LLViewerFetchedTexture::sDefaultCloudNoiseImagep;
}

LLVOSky::~LLVOSky()
{
	// Do not delete images: they will get deleted by gTextureList on shutdown
	// This needs to be done for each texture
	mCubeMap = NULL;
}

void LLVOSky::init()
{
	LLSettingsSky::ptr_t skyp = gEnvironment.getCurrentSky();
	bool has_sky = false;
	if (skyp)	// Paranoia
	{
		has_sky = true;
		skyp->update();
		updateDirections(skyp);
		initAtmospherics(skyp);
	}

	// Initialize the cached normalized direction vectors
	for (S32 side = 0; side < NUM_CUBEMAP_FACES; ++side)
	{
		for (S32 tile = 0; tile < NUM_TILES; ++tile)
		{
			initSkyTextureDirs(side, tile);
			if (has_sky)	// Paranoia
			{
				createSkyTexture(skyp, side, tile);
			}
		}
		mSkyTex[side].create();
		mShinyTex[side].create();
	}

	initCubeMap();

	mInitialized = true;
	mHeavenlyBodyUpdated = false;
}

void LLVOSky::initCubeMap()
{
	std::vector<LLPointer<LLImageRaw> > images;
	for (S32 side = 0; side < NUM_CUBEMAP_FACES; ++side)
	{
		images.emplace_back(mShinyTex[side].getImageRaw());
	}
	if (mCubeMap)
	{
		mCubeMap->init(images);
	}
	else if (LLPipeline::sRenderWater)
	{
		mCubeMap = new LLCubeMap();
		mCubeMap->init(images);
	}
	gGL.getTexUnit(0)->disable();
}

void LLVOSky::cleanupGL()
{
	for (S32 i = 0; i < NUM_CUBEMAP_FACES; ++i)
	{
		mSkyTex[i].cleanupGL();
	}
	if (getCubeMap())
	{
		getCubeMap()->destroyGL();
	}
}

void LLVOSky::restoreGL()
{
	for (S32 i = 0; i < NUM_CUBEMAP_FACES; ++i)
	{
		mSkyTex[i].restoreGL();
	}

	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (skyp)	// Paranoia
	{
		setSunTextures(skyp->getSunTextureId(), skyp->getNextSunTextureId());
		setMoonTextures(skyp->getMoonTextureId(),
						skyp->getNextMoonTextureId());
		updateDirections(skyp);
	}

	if (LLPipeline::sRenderWater)
	{
		initCubeMap();
	}

	mForceUpdate = mNeedUpdate = true;
	mCubeMapUpdateStage = -1;
	mCubeMapUpdateTile = 0;

	if (mDrawable)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
	}
}

void LLVOSky::initSkyTextureDirs(S32 side, S32 tile)
{
	S32 tile_x = tile % NUM_TILES_X;
	S32 tile_y = tile / NUM_TILES_X;

	S32 tile_x_pos = tile_x * sTileResX;
	S32 tile_y_pos = tile_y * sTileResY;

	F32 coeff[3] = { 0.f, 0.f, 0.f };
	const S32 curr_coef = side >> 1; // 0/1 = Z axis, 2/3 = Y, 4/5 = X
	const S32 side_dir = ((side & 1) << 1) - 1;  // even = -1, odd = 1
	const S32 x_coef = (curr_coef + 1) % 3;
	const S32 y_coef = (x_coef + 1) % 3;

	coeff[curr_coef] = (F32)side_dir;

	F32 inv_res = 1.f / sResolution;
	for (S32 y = tile_y_pos; y < tile_y_pos + sTileResY; ++y)
	{
		for (S32 x = tile_x_pos; x < tile_x_pos + sTileResX; ++x)
		{
			coeff[x_coef] = F32((x << 1) + 1) * inv_res - 1.f;
			coeff[y_coef] = F32((y << 1) + 1) * inv_res - 1.f;
			LLVector3 dir(coeff[0], coeff[1], coeff[2]);
			dir.normalize();
			mSkyTex[side].setDir(dir, x, y);
			mShinyTex[side].setDir(dir, x, y);
		}
	}
}

void LLVOSky::createSkyTexture(const LLSettingsSky::ptr_t& skyp, S32 side,
							   S32 tile)
{
	S32 tile_x = tile % NUM_TILES_X;
	S32 tile_y = tile / NUM_TILES_X;

	S32 tile_x_pos = tile_x * sTileResX;
	S32 tile_y_pos = tile_y * sTileResY;

	for (S32 y = tile_y_pos; y < tile_y_pos + sTileResY; ++y)
	{
		for (S32 x = tile_x_pos; x < tile_x_pos + sTileResX; ++x)
		{
			const LLVector3& sky_dir = mSkyTex[side].getDir(x, y);
			mSkyTex[side].setPixel(calcSkyColorInDir(skyp, sky_dir), x, y);
			const LLVector3& shiny_dir = mShinyTex[side].getDir(x, y);
			mShinyTex[side].setPixel(calcSkyColorInDir(skyp, shiny_dir, true),
									 x, y);
		}
	}
}

void LLVOSky::initAtmospherics(const LLSettingsSky::ptr_t& skyp)
{
	mGamma = skyp->getGamma();

	mBlueDensity = skyp->getBlueDensity();
	mBlueHorizon = skyp->getBlueHorizon();
	mHazeDensity = skyp->getHazeDensity();
	mHazeHorizon = skyp->getHazeHorizon();
	mDensityMultiplier = skyp->getDensityMultiplier();
	mMaxY = skyp->getMaxY();
	mSunNorm = gEnvironment.getClampedSunNorm();
	mSunlight = skyp->getIsSunUp() ? skyp->getSunlightColor()
								   : skyp->getMoonlightColor();
	mAmbient = skyp->getAmbientColor();
	mGlow = skyp->getGlow();
	mCloudShadow = skyp->getCloudShadow();

	// Note: the following components are derived from the already fetched
	// settings above; the (simple) formulae to compute them have been kept as
	// inlined static methods of LLSettingsSky, so that should they get changed
	// it will be easy to find them rather than scattering them among the rest
	// of the viewer sources). HB
	mTotalDensity = LLSettingsSky::totalDensity(mBlueDensity, mHazeDensity);
	mLightAttenuation = LLSettingsSky::lightAttenuation(mBlueDensity,
														mHazeDensity,
														mDensityMultiplier,
														mMaxY);
	mLightTransmittance = LLSettingsSky::lightTransmittance(mTotalDensity,
															mDensityMultiplier,
															mMaxY);

	LLUUID tex_id = skyp->getRainbowTextureId();
	if (mRainbowMap.isNull() || mRainbowMap->getID() != tex_id)
	{
		mRainbowMap =
			LLViewerTextureManager::getFetchedTexture(tex_id, FTT_DEFAULT,
													  true,
													  LLGLTexture::BOOST_UI);
	}
	tex_id = skyp->getHaloTextureId();
	if (mHaloMap.isNull() || mHaloMap->getID() != tex_id)
	{
		mHaloMap =
			LLViewerTextureManager::getFetchedTexture(tex_id, FTT_DEFAULT,
													  true,
													  LLGLTexture::BOOST_UI);
	}
#if LL_VARIABLE_SKY_DOME_SIZE
	// NOTE: this is for now a constant equal to SKY_DOME_RADIUS.
	mDomeRadius = skyp->getDomeRadius();
	// NOTE: this is for now a constant equal to SKY_DOME_OFFSET.
	mDomeOffset = skyp->getDomeOffset();
#endif
}

LLColor4 LLVOSky::calcSkyColorInDir(const LLSettingsSky::ptr_t& skyp,
									const LLVector3& dir, bool is_shiny)
{
	constexpr F32 sky_saturation = 0.25f;
	constexpr F32 land_saturation = 0.1f;

	if (is_shiny && dir.mV[VZ] < -0.02f)
	{
		LLColor4 col;
		LLColor3 desat_fog(mFogColor);
		F32 brightness = desat_fog.brightness();
		// So that shiny somewhat shows up at night.
		if (brightness < 0.15f)
		{
			brightness = 0.15f;
			desat_fog = smear(0.15f);
		}
		F32 greyscale_sat = brightness * (1.f - land_saturation);
		desat_fog = desat_fog * land_saturation + smear(greyscale_sat);
		if (gPipeline.canUseWindLightShaders())
		{
			col = LLColor4(desat_fog * 0.5f, 0.f);
		}
		else
		{
			col = LLColor4(desat_fog, 0.f);
		}
		F32 x = 1.f - fabsf(-0.1f - dir.mV[VZ]);
		x *= x;
		col.mV[0] *= x * x;
		col.mV[1] *= powf(x, 2.5f);
		col.mV[2] *= x * x * x;
		return col;
	}

	// Undo OGL_TO_CFR_ROTATION and negate vertical direction.
	LLVector3 pn = LLVector3(-dir[1] , -dir[2], -dir[0]);
	// Calculate mHazeColor
	calcSkyColorVert(pn);

	LLColor3 sky_color;
	if (is_shiny)
	{
		F32 brightness = mHazeColor.brightness();
		F32 greyscale_sat = brightness * (1.f - sky_saturation);
		sky_color = mHazeColor * sky_saturation + smear(greyscale_sat);
#if 0	// SL-12574 EEP sky is being attenuated too much
		sky_color *= 0.5f + 0.5f * brightness;
#endif
	}
	else if (gPipeline.canUseWindLightShaders())
	{
		sky_color = LLSettingsSky::gammaCorrect(mHazeColor * 2.f, mGamma);
	}
	else
	{
		sky_color = mHazeColor * 2.f;
	}

	return LLColor4(sky_color, 0.f);
}

void LLVOSky::calcSkyColorVert(LLVector3& pn)
{
	// Project the direction ray onto the sky dome.
	F32 phi = acosf(pn[1]);
	F32 sin_a = sinf(F_PI - phi);
	if (fabsf(sin_a) < 0.01f)
	{
		// Avoid division by zero
		sin_a = 0.01f;
	}
	F32 p_len = mDomeRadius *
				sinf(F_PI + phi + asinf(mDomeOffset * sin_a)) / sin_a;

	pn *= p_len;

	// Set altitude
	if (pn[1] > 0.f)
	{
		pn *= mMaxY / pn[1];
	}
	else
	{
		pn *= -32000.f / pn[1];
	}

	p_len = pn.length();
	pn /= p_len;

	// Initialize temp variables
	LLColor3 sunlight = mSunlight;

	// Calculate relative weights
	LLColor3 temp1 = mTotalDensity;
	LLColor3 blue_factor = mBlueHorizon *
						   componentDiv(mBlueDensity, temp1);
	LLColor3 haze_factor = mHazeHorizon *
						   componentDiv(smear(mHazeDensity), temp1);

	// Compute sunlight from P & lightnorm (for long rays like sky)
	LLColor3 temp2;
	temp2.mV[1] = llmax(F_APPROXIMATELY_ZERO,
						llmax(0.f, pn[1]) + mSunNorm.mV[1]);

	temp2.mV[1] = 1.f / temp2.mV[1];
	componentMultBy(sunlight, componentExp(mLightAttenuation * -temp2.mV[1]));
	componentMultBy(sunlight, mLightTransmittance);

	// Distance
	temp2.mV[2] = p_len * mDensityMultiplier;

	// Transparency (-> temp1)
	temp1 = componentExp(temp1 * -temp2.mV[2]);

	// Compute haze glow
	temp2.mV[0] = pn * LLVector3(mSunNorm);

	// temp2.x is 0 at the sun and increases away from sun
	temp2.mV[0] = 1.f - temp2.mV[0];
	// Set a minimum "angle" (smaller glow.y allows tighter, brighter hotspot)
	temp2.mV[0] = llmax(temp2.mV[0], 0.001f);
	// Higher glow.x gives dimmer glow (because next step is 1 / "angle")
	temp2.mV[0] *= mGlow.mV[0];
	// glow.z should be negative, so we are doing a sort of (1 / "angle")
	// function
	temp2.mV[0] = powf(temp2.mV[0], mGlow.mV[2]);

	// Add "minimum anti-solar illumination"
	temp2.mV[0] += 0.25f;

	// Haze color above cloud
	mHazeColor = blue_factor * (sunlight + mAmbient) +
				 componentMult(haze_factor, sunlight * temp2.mV[0] + mAmbient);

#if 0	// 'hazeColorBelowCloud' in LL's EEP viewer (which would be
		// mHazeColorBelowCloud for the Cool VL Viewer) is never used...
	// Increase ambient when there are more clouds
	LLColor3 ambient = mAmbient + (LLColor3::white - mAmbient) *
					   mCloudShadow * 0.5f;

	// Dim sunlight by cloud shadow percentage
	sunlight *= 1.f - mCloudShadow;

	// Haze color below cloud
	mHazeColorBelowCloud = blue_factor * (sunlight + ambient) +
						   componentMult(mHazeColor,
										 sunlight * temp2.mV[0] + ambient);
	// Final atmosphere additive
	componentMultBy(mHazeColor, LLColor3::white - temp1);

	// Attenuate cloud color by atmosphere (less atmos opacity/more
	// transparency below clouds)
	temp1 = componentSqrt(temp1);

	// At horizon, blend high altitude sky color towards the darker color below
	// the clouds
	mHazeColor += componentMult(mHazeColorBelowCloud - mHazeColor,
								LLColor3::white - componentSqrt(temp1));
#else
	// Final atmosphere additive
	componentMultBy(mHazeColor, LLColor3::white - temp1);
#endif
}

void LLVOSky::calcAtmospherics()
{
	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (skyp)	// Paranoia
	{
		initAtmospherics(skyp);
		mSun.setColor(skyp->getSunDiffuse());
	}
	mMoon.setColor(LLColor3::white);

	mSun.renewDirection();
	mSun.renewColor();
	mMoon.renewDirection();
	mMoon.renewColor();
}

bool LLVOSky::updateSky()
{
	if (mDead || gGLManager.mIsDisabled ||
		!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
	{
		return true;
	}
	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (!skyp)	// Paranoia
	{
		return true;
	}

	static S32 next_frame = 0;

	mNeedUpdate = mForceUpdate;

	++next_frame;
	next_frame = next_frame % MAX_TILES;

	mInterpVal = mInitialized ? (F32)next_frame / (F32)MAX_TILES : 1.f;
	LLSkyTex::setInterpVal(mInterpVal);
	LLHeavenBody::setInterpVal(mInterpVal);
	updateDirections(skyp);

	if (!mCubeMap || LLPipeline::sReflectionProbesEnabled)
	{
		mCubeMapUpdateStage = NUM_CUBEMAP_FACES;
		mForceUpdate = false;
		return true;
	}

	if (mCubeMapUpdateStage < 0)
	{
		LL_TRACY_TIMER(TRC_VOSKY_CALC);
		calcAtmospherics();
		if (!mNeedUpdate)
		{
			mNeedUpdate = haveValuesChanged();
		}

		if (mNeedUpdate && (mForceUpdate || mForceUpdateThrottle.hasExpired()))
		{
			// Start updating cube map sides
			updateFog(gViewerCamera.getFar());
			mCubeMapUpdateStage = 0;
			mCubeMapUpdateTile = 0;
			mForceUpdate = false;
		}
	}
	else if (mCubeMapUpdateStage >= NUM_CUBEMAP_FACES &&
			 !LLPipeline::sReflectionProbesEnabled)
	{
		LL_TRACY_TIMER(TRC_VOSKY_UPDATEFORCED);
		LLSkyTex::stepCurrent();

		bool cannot_use_wl = !gPipeline.canUseWindLightShaders();

		S32 tex = mSkyTex[0].getWhich(true);
		LLImageRaw* raw1;
		LLImageRaw* raw2;
		for (S32 side = 0; side < NUM_CUBEMAP_FACES; ++side)
		{
			if (cannot_use_wl)
			{
				raw1 = mSkyTex[side].getImageRaw(true);
				raw2 = mSkyTex[side].getImageRaw(false);
				raw2->copy(raw1);
				mSkyTex[side].createGLImage(tex);
			}
			raw1 = mShinyTex[side].getImageRaw(true);
			raw2 = mShinyTex[side].getImageRaw(false);
			raw2->copy(raw1);
			mShinyTex[side].createGLImage(tex);
		}
		next_frame = 0;

		// Update the sky texture
		if (cannot_use_wl)
		{
			for (S32 side = 0; side < NUM_CUBEMAP_FACES; ++side)
			{
				mSkyTex[side].create();
			}
		}
		for (S32 side = 0; side < NUM_CUBEMAP_FACES; ++side)
		{
			mShinyTex[side].create();
		}

		// Update the environment map
		initCubeMap();

		saveCurrentValues();

		mNeedUpdate = mForceUpdate = false;

		mForceUpdateThrottle.setTimerExpirySec(UPDATE_EXPIRY);
		if (mDrawable.notNull() && mDrawable->getFace(0) &&
			!mDrawable->getFace(0)->getVertexBuffer())
		{
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
		}
		mCubeMapUpdateStage = -1;
		mCubeMapUpdateTile = 0;
	}
	// mCubeMapUpdateStage >= 0 && mCubeMapUpdateStage < NUM_CUBEMAP_FACES
	else if (!LLPipeline::sReflectionProbesEnabled)
	{
		LL_TRACY_TIMER(TRC_VOSKY_CREATETEXTURES);
		S32 side = mCubeMapUpdateStage;
		S32 start_tile = mCubeMapUpdateTile;
		for (S32 tile = 0; tile < UPDATE_TILES; ++tile)
		{
			createSkyTexture(skyp, side, start_tile + tile);
		}
		mCubeMapUpdateTile += UPDATE_TILES;
		if (mCubeMapUpdateTile >= NUM_TILES)
		{
			++mCubeMapUpdateStage;
			mCubeMapUpdateTile = 0;
		}
	}

	return true;
}

bool LLVOSky::haveValuesChanged()
{
	if (!almost_equal(mOldCloudShadow, mCloudShadow))
	{
		return true;
	}
	if (!almost_equal(mOldHazeDensity, mHazeDensity))
	{
		return true;
	}
	if (!almost_equal(mOldHazeHorizon, mHazeHorizon))
	{
		return true;
	}
	if (!almost_equal(mOldSunNorm, mSunNorm))
	{
		return true;
	}
	if (!almost_equal(mOldSunlight, mSunlight))
	{
		return true;
	}
	if (!almost_equal(mOldAmbient, mAmbient))
	{
		return true;
	}
	if (!almost_equal(mOldBlueDensity, mBlueDensity))
	{
		return true;
	}
	if (!almost_equal(mOldBlueHorizon, mBlueHorizon))
	{
		return true;
	}
	if (!almost_equal(mOldDensityMultiplier, mDensityMultiplier))
	{
		return true;
	}
	if (!almost_equal(mOldGlow, mGlow))
	{
		return true;
	}
	if (!almost_equal(mOldMaxY, mMaxY))
	{
		return true;
	}
	if (!almost_equal(mOldGamma, mGamma))
	{
		return true;
	}
#if 0	// These components are derived from the above ones, so need to save
		// and test them... HB
	if (!almost_equal(mOldLightAttenuation, mLightAttenuation))
	{
		return true;
	}
	if (!almost_equal(mOldLightTransmittance, mLightTransmittance))
	{
		return true;
	}
	if (!almost_equal(mOldTotalDensity, mTotalDensity))
	{
		return true;
	}
#endif
	return false;
}

void LLVOSky::saveCurrentValues()
{
	mOldGamma = mGamma;
	mOldHazeDensity = mHazeDensity;
	mOldHazeHorizon = mHazeHorizon;
	mOldDensityMultiplier = mDensityMultiplier;
	mOldMaxY = mMaxY;
	mOldCloudShadow = mCloudShadow;
	mOldSunNorm = mSunNorm;
	mOldGlow = mGlow;
	mOldSunlight = mSunlight;
	mOldAmbient = mAmbient;
	mOldBlueDensity = mBlueDensity;
	mOldBlueHorizon = mBlueHorizon;
#if 0	// These components are derived from the above ones, so need to save
		// and test them... HB
	mOldLightAttenuation = mLightAttenuation;
	mOldLightTransmittance = mLightTransmittance;
	mOldTotalDensity = mTotalDensity;
#endif
}

void LLVOSky::updateTextures()
{
	constexpr F32 max_area = (F32)MAX_IMAGE_AREA;
	if (mSunTexturep[0])
	{
		mSunTexturep[0]->addTextureStats(max_area);
	}
	if (mMoonTexturep[0])
	{
		mMoonTexturep[0]->addTextureStats(max_area);
	}
	if (mBloomTexturep[0])
	{
		mBloomTexturep[0]->addTextureStats(max_area);
	}
	if (mCloudNoiseTexturep[0])
	{
		mCloudNoiseTexturep[0]->addTextureStats(max_area);
	}
	if (mSunTexturep[1])
	{
		mSunTexturep[1]->addTextureStats(max_area);
	}
	if (mMoonTexturep[1])
	{
		mMoonTexturep[1]->addTextureStats(max_area);
	}
	if (mBloomTexturep[1])
	{
		mBloomTexturep[1]->addTextureStats(max_area);
	}
	if (mCloudNoiseTexturep[1])
	{
		mCloudNoiseTexturep[1]->addTextureStats(max_area);
	}
}

LLDrawable* LLVOSky::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setLit(false);

	LLDrawPoolSky* poolp =
		(LLDrawPoolSky*)gPipeline.getPool(LLDrawPool::POOL_SKY);
	poolp->setSkyTex(mSkyTex);
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_SKY);

	for (S32 i = 0; i < NUM_CUBEMAP_FACES; ++i)
	{
		mFace[FACE_SIDE0 + i] = mDrawable->addFace(poolp, NULL);
	}

	mFace[FACE_SUN] = mDrawable->addFace(poolp, NULL);
	mFace[FACE_MOON] = mDrawable->addFace(poolp, NULL);
	mFace[FACE_BLOOM] = mDrawable->addFace(poolp, NULL);

	mFace[FACE_SUN]->setMediaAllowed(false);
	mFace[FACE_MOON]->setMediaAllowed(false);
	mFace[FACE_BLOOM]->setMediaAllowed(false);

	return mDrawable;
}

bool LLVOSky::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_GEO_SKY);
	if (mFace[FACE_REFLECTION] == NULL)
	{
		LLDrawPoolWater* poolp =
			(LLDrawPoolWater*)gPipeline.getPool(LLDrawPool::POOL_WATER);
		if (gPipeline.getPool(LLDrawPool::POOL_WATER)->getShaderLevel())
		{
			mFace[FACE_REFLECTION] = drawable->addFace(poolp, NULL);
		}
	}

	mCameraPosAgent = drawable->getPositionAgent();
	mEarthCenter.mV[0] = mCameraPosAgent.mV[0];
	mEarthCenter.mV[1] = mCameraPosAgent.mV[1];

	LLVector3 v_agent[8];
	for (S32 i = 0; i < 8; ++i)
	{
		F32 x_sgn = (i & 1) ? 1.f : -1.f;
		F32 y_sgn = (i & 2) ? 1.f : -1.f;
		F32 z_sgn = (i & 4) ? 1.f : -1.f;
		v_agent[i] = HORIZON_DIST * SKY_BOX_MULT *
					 LLVector3(x_sgn, y_sgn, z_sgn);
	}

	LLStrider<LLVector3> verticesp;
	LLStrider<LLVector3> normalsp;
	LLStrider<LLVector2> texcoordsp;
	LLStrider<U16> indicesp;
	for (S32 side = 0; side < NUM_CUBEMAP_FACES; ++side)
	{
		LLFace* face = mFace[FACE_SIDE0 + side];
		if (!face || face->getVertexBuffer())
		{
			continue;
		}

		face->setSize(4, 6);
		face->setGeomIndex(0);
		face->setIndicesIndex(0);
		LLVertexBuffer* buff =
			new LLVertexBuffer(LLDrawPoolSky::VERTEX_DATA_MASK);
		buff->allocateBuffer(4, 6);
		face->setVertexBuffer(buff);

		U16 index_offset = face->getGeometry(verticesp, normalsp, texcoordsp,
											 indicesp);

		S32 vtx = 0;
		S32 curr_bit = side >> 1; // 0/1 = Z axis, 2/3 = Y, 4/5 = X
		S32 side_dir = side & 1;  // even - 0, odd - 1
		S32 i_bit = (curr_bit + 2) % 3;
		S32 j_bit = (i_bit + 2) % 3;

		LLVector3 axis;
		axis.mV[curr_bit] = 1.f;
		face->mCenterAgent = (F32)((side_dir << 1) - 1) * axis * HORIZON_DIST;

		vtx = side_dir << curr_bit;
		*verticesp++ = v_agent[vtx];
		*verticesp++ = v_agent[vtx | 1 << j_bit];
		*verticesp++ = v_agent[vtx | 1 << i_bit];
		*verticesp++ = v_agent[vtx | 1 << i_bit | 1 << j_bit];

		*texcoordsp++ = TEX00;
		*texcoordsp++ = TEX01;
		*texcoordsp++ = TEX10;
		*texcoordsp++ = TEX11;

		// Triangles for each side
		*indicesp++ = index_offset;
		*indicesp++ = index_offset + 1;
		*indicesp++ = index_offset + 3;

		*indicesp++ = index_offset;
		*indicesp++ = index_offset + 3;
		*indicesp++ = index_offset + 2;

		buff->unmapBuffer();
	}

	const LLVector3& look_at = gViewerCamera.getAtAxis();
	LLVector3 right = look_at % LLVector3::z_axis;
	LLVector3 up = right % look_at;
	right.normalize();
	up.normalize();

	constexpr F32 cos_max_angle = 1.f;
	bool draw_sun = updateHeavenlyBodyGeometry(drawable, true, mSun,
											   cos_max_angle, up, right);
	bool draw_moon = updateHeavenlyBodyGeometry(drawable, false, mMoon,
												cos_max_angle, up, right);
	draw_sun &= gEnvironment.getIsSunUp();
	draw_moon &= gEnvironment.getIsMoonUp();
	mSun.setDraw(draw_sun);
	mMoon.setDraw(draw_moon);

	F32 water_height = 0.01f;
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		water_height += regionp->getWaterHeight();
	}
	F32 camera_height = mCameraPosAgent.mV[2];
	F32 height_above_water = camera_height - water_height;

	bool sun_flag = !mMoon.isVisible() || look_at * mSun.getDirection() > 0.f;

	if (height_above_water > 0)
	{
		bool render_ref =
			gPipeline.getPool(LLDrawPool::POOL_WATER)->getShaderLevel() == 0;

		if (sun_flag)
		{
			setDrawRefl(0);
			if (render_ref)
			{
				updateReflectionGeometry(drawable, height_above_water, mSun);
			}
		}
		else
		{
			setDrawRefl(1);
			if (render_ref)
			{
				updateReflectionGeometry(drawable, height_above_water, mMoon);
			}
		}
	}
	else
	{
		setDrawRefl(-1);
	}

	return true;
}

bool LLVOSky::updateHeavenlyBodyGeometry(LLDrawable* drawable, bool is_sun,
										 LLHeavenBody& hb, F32 cos_max_angle,
										 const LLVector3& up,
										 const LLVector3& right)
{
	mHeavenlyBodyUpdated = true;

	LLVector3 to_dir, hb_right, hb_up;
	LLQuaternion rot = hb.getRotation();
	to_dir = LLVector3::x_axis * rot;
	hb_right = to_dir % LLVector3::z_axis;
	hb_up = hb_right % to_dir;
	// At zenith so math below fails spectacularly
	if (to_dir * LLVector3::z_axis > 0.99f)
	{
		hb_right = LLVector3::y_axis_neg * rot;
		hb_up = LLVector3::z_axis * rot;
	}
	hb_right.normalize();
	hb_up.normalize();

	LLVector3 draw_pos = to_dir * HEAVENLY_BODY_DIST;

	const F32 enlargm_factor = 1.f - to_dir.mV[2];
	F32 horiz_enlargement = 1.f + enlargm_factor * 0.3f;
	F32 vert_enlargement = 1.f + enlargm_factor * 0.2f;

	LLVector3 v_clipped[4];
	F32 scale = is_sun ? mSunScale : mMoonScale;
	scale *= HEAVENLY_BODY_DIST * HEAVENLY_BODY_FACTOR;
	const LLVector3 scaled_right = horiz_enlargement * scale *
								   hb.getDiskRadius() * hb_right;
	const LLVector3 scaled_up = vert_enlargement * scale *
								hb.getDiskRadius() * hb_up;
	v_clipped[0] = draw_pos - scaled_right + scaled_up;
	v_clipped[1] = draw_pos - scaled_right - scaled_up;
	v_clipped[2] = draw_pos + scaled_right + scaled_up;
	v_clipped[3] = draw_pos + scaled_right - scaled_up;

	hb.setVisible(true);

	const S32 f = is_sun ? FACE_SUN : FACE_MOON;
	LLFace* facep = mFace[f];
	if (!facep) return false;

	if (!facep->getVertexBuffer())
	{
		facep->setSize(4, 6);
		LLVertexBuffer* buff =
			new LLVertexBuffer(LLDrawPoolSky::VERTEX_DATA_MASK);
		if (!buff->allocateBuffer(facep->getGeomCount(),
								  facep->getIndicesCount()))
		{
			llwarns << "Failure to allocate a vertex buffer with "
					<< facep->getGeomCount() << " vertices and "
					<< facep->getIndicesCount() << " indices" << llendl;
			return true;
		}
		facep->setGeomIndex(0);
		facep->setIndicesIndex(0);
		facep->setVertexBuffer(buff);
	}

	LLStrider<LLVector3> verticesp;
	LLStrider<LLVector3> normalsp;
	LLStrider<LLVector2> texcoordsp;
	LLStrider<U16> indicesp;
	S32 index_offset = facep->getGeometry(verticesp, normalsp, texcoordsp,
										  indicesp);
	if (index_offset == -1)
	{
		return true;
	}

	for (S32 vtx = 0; vtx < 4; ++vtx)
	{
		hb.corner(vtx) = v_clipped[vtx];
		*(verticesp++)  = hb.corner(vtx) + mCameraPosAgent;
	}

	*texcoordsp++ = TEX01;
	*texcoordsp++ = TEX00;
	*texcoordsp++ = TEX11;
	*texcoordsp++ = TEX10;

	*indicesp++ = index_offset;
	*indicesp++ = index_offset + 2;
	*indicesp++ = index_offset + 1;

	*indicesp++ = index_offset + 1;
	*indicesp++ = index_offset + 2;
	*indicesp++ = index_offset + 3;

	facep->getVertexBuffer()->unmapBuffer();

	return true;
}

F32 dtReflection(const LLVector3& p, F32 cos_dir_from_top,
				 F32 sin_dir_from_top, F32 diff_angl_dir)
{
	LLVector3 P = p;
	P.normalize();

	const F32 cos_dir_angle = -P.mV[VZ];
	const F32 sin_dir_angle = sqrtf(1.f - cos_dir_angle * cos_dir_angle);

	F32 cos_diff_angles = cos_dir_angle * cos_dir_from_top +
						  sin_dir_angle * sin_dir_from_top;

	F32 diff_angles;
	if (cos_diff_angles > 1.f - 1e-7f)
	{
		diff_angles = 0.f;
	}
	else
	{
		diff_angles = acosf(cos_diff_angles);
	}

	const F32 rel_diff_angles = diff_angles / diff_angl_dir;
	const F32 dt = 1.f - rel_diff_angles;

	return dt >= 0.f ? dt : 0.f;
}

F32 dtClip(const LLVector3& v0, const LLVector3& v1, F32 far_clip2)
{
	const LLVector3 otrezok = v1 - v0;
	const F32 A = otrezok.lengthSquared();
	const F32 B = v0 * otrezok;
	const F32 C = v0.lengthSquared() - far_clip2;
	const F32 det = sqrtf(B * B - A * C);
	F32 dt_clip = (-B - det) / A;
	if (dt_clip < 0.f || dt_clip > 1.f)
	{
		dt_clip = (-B + det) / A;
	}
	return dt_clip;
}

void LLVOSky::updateReflectionGeometry(LLDrawable* drawable, F32 h,
									   const LLHeavenBody& hb)
{
	const LLVector3& look_at = gViewerCamera.getAtAxis();

	LLVector3 to_dir = hb.getDirection();
	LLVector3 hb_pos = to_dir * (HORIZON_DIST - 10.f);
	LLVector3 to_dir_proj = to_dir;
	to_dir_proj.mV[VZ] = 0.f;
	to_dir_proj.normalize();

	LLVector3 right = to_dir % LLVector3::z_axis;
	LLVector3 up = right % to_dir;
	right.normalize();
	up.normalize();

	// Finding angle between look direction and sprite.
	LLVector3 look_at_right = look_at % LLVector3::z_axis;
	look_at_right.normalize();

	const F32 enlargm_factor = 1.f - to_dir.mV[2];
	F32 horiz_enlargement = 1.f + enlargm_factor * 0.3f;
	F32 vert_enlargement = 1.f + enlargm_factor * 0.2f;

	F32 vert_size =
		vert_enlargement * HEAVENLY_BODY_SCALE * hb.getDiskRadius();
	right *= horiz_enlargement * HEAVENLY_BODY_SCALE * hb.getDiskRadius();
	up *= vert_size;

	LLVector3 v_corner[2];
	LLVector3 stretch_corner[2];

	LLVector3 top_hb = v_corner[0] = stretch_corner[0] = hb_pos - right + up;
	v_corner[1] = stretch_corner[1] = hb_pos - right - up;

	LLVector2 TEX0t = TEX00;
	LLVector2 TEX1t = TEX10;
	LLVector3 lower_corner = v_corner[1];

	top_hb.normalize();

	const F32 cos_angle_of_view = fabsf(top_hb.mV[VZ]);
	const F32 extension = llmin(5.f, 1.f / cos_angle_of_view);

	constexpr S32 cols = 1;
	const S32 raws = lltrunc(16 * extension);
	S32 quads = cols * raws;

	stretch_corner[0] = lower_corner + extension *
						(stretch_corner[0] - lower_corner);
	stretch_corner[1] = lower_corner + extension *
						(stretch_corner[1] - lower_corner);

	F32 cos_dir_from_top[2];
	LLVector3 dir = stretch_corner[0];
	dir.normalize();
	cos_dir_from_top[0] = dir.mV[VZ];
	dir = stretch_corner[1];
	dir.normalize();
	cos_dir_from_top[1] = dir.mV[VZ];

	const F32 sin_dir_from_top = sqrtf(1.f - cos_dir_from_top[0] *
									   cos_dir_from_top[0]);
	const F32 sin_dir_from_top2 = sqrtf(1.f - cos_dir_from_top[1] *
										cos_dir_from_top[1]);
	const F32 cos_diff_dir = cos_dir_from_top[0] * cos_dir_from_top[1] +
							 sin_dir_from_top * sin_dir_from_top2;
	const F32 diff_angl_dir = acosf(cos_diff_dir);

	v_corner[0] = stretch_corner[0];
	v_corner[1] = lower_corner;

	LLVector2 TEX0tt = TEX01;
	LLVector2 TEX1tt = TEX11;

	LLVector3 v_refl_corner[4];
	LLVector3 v_sprite_corner[4];

	for (S32 vtx = 0; vtx < 2; ++vtx)
	{
		LLVector3 light_proj = v_corner[vtx];
		light_proj.normalize();

		const F32 z = light_proj.mV[VZ];
		const F32 sin_angle = sqrtf(1.f - z * z);
		light_proj *= 1.f / sin_angle;
		light_proj.mV[VZ] = 0.f;
		const F32 to_refl_point = h * sin_angle / fabsf(z);

		v_refl_corner[vtx] = to_refl_point * light_proj;
	}

	for (S32 vtx = 2; vtx < 4; ++vtx)
	{
		const LLVector3 to_dir_vec = (to_dir_proj * v_refl_corner[vtx - 2]) *
									 to_dir_proj;
		v_refl_corner[vtx] = v_refl_corner[vtx - 2] +
							 2.f * (to_dir_vec - v_refl_corner[vtx - 2]);
	}

	for (S32 vtx = 0; vtx < 4; ++vtx)
	{
		v_refl_corner[vtx].mV[VZ] -= h;
	}

	LLVector3 refl_corn_norm[2];
	refl_corn_norm[0] = v_refl_corner[1];
	refl_corn_norm[0].normalize();
	refl_corn_norm[1] = v_refl_corner[3];
	refl_corn_norm[1].normalize();

	F32 cos_refl_look_at[2];
	cos_refl_look_at[0] = refl_corn_norm[0] * look_at;
	cos_refl_look_at[1] = refl_corn_norm[1] * look_at;

	S32 side = 0;
	if (cos_refl_look_at[1] > cos_refl_look_at[0])
	{
		side = 2;
	}

#if 0
	const F32 far_clip = (gViewerCamera.getFar() - 0.01f) / far_clip_factor;
	const F32 far_clip2 = far_clip * far_clip;
#else
	constexpr F32 far_clip = 512;
	constexpr F32 far_clip2 = far_clip * far_clip;
#endif

	F32 dt_clip;
	if (v_refl_corner[side].lengthSquared() > far_clip2)
	{
		// Whole thing is sprite: reflection is beyond far clip plane.
		dt_clip = 1.1f;
		quads = 1;
	}
	else if (v_refl_corner[side + 1].lengthSquared() > far_clip2)
	{
		// Part is reflection, the rest is sprite.
		dt_clip = dtClip(v_refl_corner[side + 1], v_refl_corner[side],
						 far_clip2);
		const LLVector3 p = (1.f - dt_clip) * v_refl_corner[side + 1] +
							dt_clip * v_refl_corner[side];

		F32 dt_tex = dtReflection(p, cos_dir_from_top[0], sin_dir_from_top,
								  diff_angl_dir);
		TEX0tt = LLVector2(0.f, dt_tex);
		TEX1tt = LLVector2(1.f, dt_tex);
		++quads;
	}
	else
	{
		// Whole thing is correct reflection.
		dt_clip = -0.1f;
	}

	LLFace* face = mFace[FACE_REFLECTION];
	if (!face) return;

	if (!face->getVertexBuffer() || face->getGeomCount() != quads * 4)
	{
		face->setSize(quads * 4, quads * 6);
		LLVertexBuffer* buff =
			new LLVertexBuffer(LLDrawPoolWater::VERTEX_DATA_MASK);
		if (!buff->allocateBuffer(face->getGeomCount(),
								  face->getIndicesCount()))
		{
			llwarns << "Failure to allocate a vertex buffer with "
					<< face->getGeomCount() << " vertices and "
					<< face->getIndicesCount() << " indices" << llendl;
			return;
		}
		face->setIndicesIndex(0);
		face->setGeomIndex(0);
		face->setVertexBuffer(buff);
	}

	LLStrider<LLVector3> verticesp;
	LLStrider<LLVector3> normalsp;
	LLStrider<LLVector2> texcoordsp;
	LLStrider<U16> indicesp;
	S32 index_offset = face->getGeometry(verticesp, normalsp, texcoordsp,
										 indicesp);
	if (index_offset == -1)
	{
		return;
	}

	LLColor3 hb_col3 = hb.getInterpColor();
	hb_col3.clamp();
	const LLColor4 hb_col = LLColor4(hb_col3);

	constexpr F32 min_attenuation = 0.4f;
	constexpr F32 max_attenuation = 0.7f;
	const F32 attenuation = min_attenuation +
							cos_angle_of_view *
							(max_attenuation - min_attenuation);

	LLColor4 hb_refl_col = (1.f - attenuation) * hb_col + attenuation *
						   mFogColor;
	face->setFaceColor(hb_refl_col);

	LLVector3 v_far[2];
	v_far[0] = v_refl_corner[1];
	v_far[1] = v_refl_corner[3];

	if (dt_clip > 0.f)
	{
		if (dt_clip >= 1.f)
		{
			for (S32 vtx = 0; vtx < 4; ++vtx)
			{
				F32 ratio = far_clip / v_refl_corner[vtx].length();
				*verticesp++ = v_refl_corner[vtx] = ratio *
													v_refl_corner[vtx] +
													mCameraPosAgent;
			}
			const LLVector3 draw_pos = 0.25 * (v_refl_corner[0] +
											   v_refl_corner[1] +
											   v_refl_corner[2] +
											   v_refl_corner[3]);
			face->mCenterAgent = draw_pos;
		}
		else
		{
			F32 ratio = far_clip / v_refl_corner[1].length();
			v_sprite_corner[1] = v_refl_corner[1] * ratio;

			ratio = far_clip / v_refl_corner[3].length();
			v_sprite_corner[3] = v_refl_corner[3] * ratio;

			v_refl_corner[1] = (1.f - dt_clip) * v_refl_corner[1] +
							   dt_clip * v_refl_corner[0];
			v_refl_corner[3] = (1.f - dt_clip) * v_refl_corner[3] +
							   dt_clip * v_refl_corner[2];
			v_sprite_corner[0] = v_refl_corner[1];
			v_sprite_corner[2] = v_refl_corner[3];

			for (S32 vtx = 0; vtx < 4; ++vtx)
			{
				*verticesp++ = v_sprite_corner[vtx] + mCameraPosAgent;
			}

			const LLVector3 draw_pos = 0.25 * (v_refl_corner[0] +
											   v_sprite_corner[1] +
											   v_refl_corner[2] +
											   v_sprite_corner[3]);
			face->mCenterAgent = draw_pos;
		}

		*texcoordsp++ = TEX0tt;
		*texcoordsp++ = TEX0t;
		*texcoordsp++ = TEX1tt;
		*texcoordsp++ = TEX1t;

		*indicesp++ = index_offset;
		*indicesp++ = index_offset + 2;
		*indicesp++ = index_offset + 1;

		*indicesp++ = index_offset + 1;
		*indicesp++ = index_offset + 2;
		*indicesp++ = index_offset + 3;

		index_offset += 4;
	}

	if (dt_clip < 1.f)
	{
		if (dt_clip <= 0.f)
		{
			const LLVector3 draw_pos = 0.25 * (v_refl_corner[0] +
											   v_refl_corner[1] +
											   v_refl_corner[2] +
											   v_refl_corner[3]);
			face->mCenterAgent = draw_pos;
		}

		const F32 raws_inv = 1.f / raws;
		const F32 cols_inv = 1.f / cols;
		LLVector3 left	= v_refl_corner[0] - v_refl_corner[1];
		LLVector3 right = v_refl_corner[2] - v_refl_corner[3];
		left *= raws_inv;
		right *= raws_inv;

		F32 dt_v0, dt_v1;
		for (S32 raw = 0; raw < raws; ++raw)
		{
			const LLVector3 bl = v_refl_corner[1] + (F32)raw * left;
			const LLVector3 br = v_refl_corner[3] + (F32)raw * right;
			const LLVector3 el = bl + left;
			const LLVector3 er = br + right;
			dt_v1 = dtReflection(el, cos_dir_from_top[0], sin_dir_from_top,
								 diff_angl_dir);
			dt_v0 = dt_v1;
			for (S32 col = 0; col < cols; ++col)
			{
				F32 dt_h0 = col * cols_inv;
				*verticesp++ = (1.f - dt_h0) * el + dt_h0 * er +
							   mCameraPosAgent;
				*verticesp++ = (1.f - dt_h0) * bl + dt_h0 * br +
							   mCameraPosAgent;
				F32 dt_h1 = (col + 1) * cols_inv;
				*verticesp++ = (1.f - dt_h1) * el + dt_h1 * er +
							   mCameraPosAgent;
				*verticesp++ = (1.f - dt_h1) * bl + dt_h1 * br +
							   mCameraPosAgent;

				*texcoordsp++ = LLVector2(dt_h0, dt_v1);
				*texcoordsp++ = LLVector2(dt_h0, dt_v0);
				*texcoordsp++ = LLVector2(dt_h1, dt_v1);
				*texcoordsp++ = LLVector2(dt_h1, dt_v0);

				*indicesp++ = index_offset;
				*indicesp++ = index_offset + 2;
				*indicesp++ = index_offset + 1;

				*indicesp++ = index_offset + 1;
				*indicesp++ = index_offset + 2;
				*indicesp++ = index_offset + 3;

				index_offset += 4;
			}
		}
	}

	face->getVertexBuffer()->unmapBuffer();
}

void LLVOSky::updateFog(F32 distance)
{
	if (!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FOG))
	{
		return;
	}

	F32 water_height = 0.01f;
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		water_height += regionp->getWaterHeight();
	}

	F32 camera_height = gAgent.getCameraPositionAgent().mV[2];

	F32 near_clip_height = gViewerCamera.getAtAxis().mV[VZ] *
						   gViewerCamera.getNear();
	camera_height += near_clip_height;

	LLColor3 res_color[3];
	LLColor3 sky_fog_color = LLColor3::white;
	LLColor3 render_fog_color = LLColor3::white;

	LLVector3 tosun;
	tosun.set(gEnvironment.getClampedLightNorm());

	const F32 tosun_z = tosun.mV[VZ];
	tosun.mV[VZ] = 0.f;
	tosun.normalize();

	LLVector3 perp_tosun;
	perp_tosun.mV[VX] = -tosun.mV[VY];
	perp_tosun.mV[VY] = tosun.mV[VX];

	LLVector3 tosun_45 = tosun + perp_tosun;
	tosun_45.normalize();

	constexpr F32 delta = 0.06f;
	tosun.mV[VZ] = perp_tosun.mV[VZ] = tosun_45.mV[VZ] = delta;
	tosun.normalize();
	perp_tosun.normalize();
	tosun_45.normalize();

	// Sky colors, just slightly above the horizon in the direction of the sun,
	// perpendicular to the sun, and at a 45 degree angle to the sun.
	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	initAtmospherics(skyp);
	res_color[0] = calcSkyColorInDir(skyp, tosun);
	res_color[1] = calcSkyColorInDir(skyp, perp_tosun);
	res_color[2] = calcSkyColorInDir(skyp, tosun_45);

	sky_fog_color = color_norm(res_color[0] + res_color[1] + res_color[2]);

	constexpr F32 full_off = -0.25f;
	constexpr F32 full_on = 0.f;
	F32 on = llclamp((tosun_z - full_off) / (full_on - full_off), 0.01f, 1.f);
	sky_fog_color *= 0.5f * on;

	// We need to clamp these to non-zero, in order for the gamma correction to
	// work. 0^y = ???
	for (S32 i = 0; i < 3; ++i)
	{
		sky_fog_color.mV[i] = llmax(0.0001f, sky_fog_color.mV[i]);
	}

	color_gamma_correct(sky_fog_color);

	render_fog_color = sky_fog_color;

	if (camera_height > water_height)
	{
		LLColor4 fog(render_fog_color);
		mGLFogCol = fog;
	}
	else
	{
		F32 depth = water_height - camera_height;

		// Adjust the color based on depth. We are doing linear approximations.
		static LLCachedControl<F32> gldepthscale(gSavedSettings,
												 "WaterGLFogDepthScale");
		F32 depth_scale = gldepthscale > 0.f ? gldepthscale : 1.f;
		static LLCachedControl<F32> gldepthfloor(gSavedSettings,
												 "WaterGLFogDepthFloor");
		F32 depth_floor = gldepthfloor > 0.f ? gldepthfloor : 0.f;
		F32 depth_modifier = 1.f - llmin(llmax(depth / depth_scale, 0.01f),
										 depth_floor);

		LLColor4 fog_col = LLDrawPoolWater::sWaterFogColor * depth_modifier;
		fog_col.setAlpha(1.f);

		// Set the gl fog color
		mGLFogCol = fog_col;
	}

	mFogColor = sky_fog_color;
	mFogColor.setAlpha(1.f);

	stop_glerror();
}

void LLVOSky::initSunDirection(const LLVector3& sun_dir)
{
	LLVector3 sun_direction = sun_dir.length() != 0.f ? sun_dir
													  : LLVector3::x_axis;
	sun_direction.normalize();
	mSun.setDirection(sun_direction);
	mSun.renewDirection();
	mSun.setAngularVelocity(LLVector3::zero);
	mMoon.setDirection(-mSun.getDirection());
	mMoon.renewDirection();
	mLastLightingDirection = mSun.getDirection();

	if (!mInitialized)
	{
		init();
		LLSkyTex::stepCurrent();
	}
}

void LLVOSky::setSunDirection(const LLVector3& sun_dir,
							  const LLVector3& sun_ang_velocity)
{
	LLVector3 sun_direction = sun_dir.length() != 0.f ? sun_dir
													  : LLVector3::x_axis;
	sun_direction.normalize();

	// Push the sun "South" as it approaches directly overhead so that we can
	// always see bump mapping on the upward facing faces of cubes.
	LLVector3 new_dir = sun_direction;

	// Same as dot product with the up direction + clamp.
	F32 sun_dot = llmax(0.f, new_dir.mV[2]);
	sun_dot *= sun_dot;

	// Create normalized vector that has the sun_dir pushed south about an hour
	// and change.
	LLVector3 adjusted_dir = (new_dir + LLVector3(0.f, -0.70711f, 0.70711f)) *
							 0.5f;
	// Blend between normal sun dir and adjusted sun dir based on how close we
	// are to having the sun overhead.
	mBumpSunDir = adjusted_dir * sun_dot + new_dir * (1.f - sun_dot);
	mBumpSunDir.normalize();

	mSun.setDirection(sun_direction);
	mSun.setAngularVelocity(sun_ang_velocity);
	mMoon.setDirection(-sun_direction);
	F32 dp = mLastLightingDirection * sun_direction;
	if (dp < 0.995f)
	{
		// The sun jumped a great deal, update immediately
		mForceUpdate = true;
	}
}

void LLVOSky::setSunDirectionCFR(const LLVector3& sun_dir_cfr)
{
	mSun.setDirection(sun_dir_cfr);
	mSun.setAngularVelocity(LLVector3::zero);

	// Push the sun "South" as it approaches directly overhead so that we can
	// always see bump mapping on the upward facing faces of cubes.
		
	// Same as dot product with the up direction + clamp.
	F32 sun_dot = llmax(0.f, sun_dir_cfr.mV[2]);
	sun_dot *= sun_dot;

	// Create normalized vector that has the sun_dir pushed south about an hour
	// and change.
	LLVector3 adjusted_dir = (sun_dir_cfr +
							  LLVector3(0.f, -0.70711f, 0.70711f)) * 0.5f;
	
	// Blend between normal sun dir and adjusted sun dir based on how close we
	// are to having the sun overhead.
	mBumpSunDir = adjusted_dir * sun_dot + sun_dir_cfr * (1.f - sun_dot);
	mBumpSunDir.normalize();

	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (skyp)	// Paranoia
	{
		updateDirections(skyp);
	}
}

void LLVOSky::setMoonDirectionCFR(const LLVector3& moon_dir)
{
	mMoon.setDirection(moon_dir);
	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (skyp)	// Paranoia
	{
		updateDirections(skyp);
	}
}

void LLVOSky::updateDirections(const LLSettingsSky::ptr_t& skyp)
{
	mSun.setDirection(skyp->getSunDirection());
	mSun.setAngularVelocity(LLVector3::zero);
	mSun.setRotation(skyp->getSunRotation());
	mMoon.setDirection(skyp->getMoonDirection());
	mMoon.setRotation(skyp->getMoonRotation());
	mSun.renewDirection();
	mMoon.renewDirection();
}

void LLVOSky::setSunTextures(const LLUUID& sun_tex1, const LLUUID& sun_tex2)
{
	if (sun_tex1.isNull())
	{
		if (gSunTextureID != IMG_SUN)
		{
			mSunTexturep[0] =
				LLViewerTextureManager::getFetchedTexture(gSunTextureID,
														  FTT_DEFAULT, true,
														  LLGLTexture::BOOST_UI);
			mSunTexturep[0]->setAddressMode(LLTexUnit::TAM_CLAMP);
		}
		else
		{
			mSunTexturep[0] = LLViewerFetchedTexture::sDefaultSunImagep;
		}
	}
	else
	{
		mSunTexturep[0] =
			LLViewerTextureManager::getFetchedTexture(sun_tex1,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_UI);
		mSunTexturep[0]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}

	if (sun_tex2.isNull())
	{
		mSunTexturep[1] = NULL;
	}
	else
	{
		mSunTexturep[1] =
			LLViewerTextureManager::getFetchedTexture(sun_tex2,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_UI);
		mSunTexturep[1]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}

	LLFace* facep = mFace[FACE_SUN];
	if (!facep)
	{
		return;
	}

	LLViewerTexture* tex = facep->getTexture(LLRender::DIFFUSE_MAP);
	if (tex && tex != mSunTexturep[0] && tex->isViewerMediaTexture())
	{
		((LLViewerMediaTexture*)tex)->removeMediaFromFace(facep);
	}

	tex = facep->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);
	if (tex && tex != mSunTexturep[1] && tex->isViewerMediaTexture())
	{
		((LLViewerMediaTexture*)tex)->removeMediaFromFace(facep);
	}

	facep->setTexture(LLRender::DIFFUSE_MAP, mSunTexturep[0]);
	if (mSunTexturep[1] && gPipeline.canUseWindLightShaders())
	{
		facep->setTexture(LLRender::ALTERNATE_DIFFUSE_MAP, mSunTexturep[1]);
	}
}

void LLVOSky::setMoonTextures(const LLUUID& moon_tex1, const LLUUID& moon_tex2)
{
	if (moon_tex1.isNull())
	{
		if (gMoonTextureID != IMG_MOON)
		{
			mMoonTexturep[0] =
				LLViewerTextureManager::getFetchedTexture(gMoonTextureID,
														  FTT_DEFAULT, true,
														  LLGLTexture::BOOST_UI);
			mMoonTexturep[0]->setAddressMode(LLTexUnit::TAM_CLAMP);
		}
		else
		{
			mMoonTexturep[0] = LLViewerFetchedTexture::sDefaultMoonImagep;
		}
	}
	else
	{
		mMoonTexturep[0] =
			LLViewerTextureManager::getFetchedTexture(moon_tex1,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_UI);
		mMoonTexturep[0]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}

	if (moon_tex2.isNull())
	{
		mMoonTexturep[1] = NULL;
	}
	else
	{
		mMoonTexturep[1] =
			LLViewerTextureManager::getFetchedTexture(moon_tex2,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_UI);
		mMoonTexturep[1]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}

	LLFace* facep = mFace[FACE_MOON];
	if (!facep)
	{
		return;
	}

	LLViewerTexture* tex = facep->getTexture(LLRender::DIFFUSE_MAP);
	if (tex && tex != mMoonTexturep[0] && tex->isViewerMediaTexture())
	{
		((LLViewerMediaTexture*)tex)->removeMediaFromFace(facep);
	}

	tex = facep->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);
	if (tex && tex != mMoonTexturep[1] && tex->isViewerMediaTexture())
	{
		((LLViewerMediaTexture*)tex)->removeMediaFromFace(facep);
	}

	facep->setTexture(LLRender::DIFFUSE_MAP, mMoonTexturep[0]);
	if (mMoonTexturep[1] && gPipeline.canUseWindLightShaders())
	{
		facep->setTexture(LLRender::ALTERNATE_DIFFUSE_MAP, mMoonTexturep[1]);
	}
}

void LLVOSky::setCloudNoiseTextures(const LLUUID& tex1, const LLUUID& tex2)
{
	if (tex1.isNull())
	{
		mCloudNoiseTexturep[0] =
			LLViewerFetchedTexture::sDefaultCloudNoiseImagep;
	}
	else
	{
		mCloudNoiseTexturep[0] =
			LLViewerTextureManager::getFetchedTexture(tex1, FTT_DEFAULT, true,
													  LLGLTexture::BOOST_UI);
		mCloudNoiseTexturep[0]->setAddressMode(LLTexUnit::TAM_WRAP);
	}

	if (tex2.isNull())
	{
		mCloudNoiseTexturep[1] = NULL;
		return;
	}

	mCloudNoiseTexturep[1] =
		LLViewerTextureManager::getFetchedTexture(tex2, FTT_DEFAULT, true,
												  LLGLTexture::BOOST_UI);
	mCloudNoiseTexturep[1]->setAddressMode(LLTexUnit::TAM_WRAP);
}

void LLVOSky::setBloomTextures(const LLUUID& tex1, const LLUUID& tex2)
{
	if (tex1.isNull())
	{
		mBloomTexturep[0] = LLViewerFetchedTexture::sBloomImagep;
	}
	else
	{
		mBloomTexturep[0] =
			LLViewerTextureManager::getFetchedTexture(tex1, FTT_DEFAULT, true,
													  LLGLTexture::BOOST_UI);
		if (mBloomTexturep[0])
		{
			mBloomTexturep[0]->setAddressMode(LLTexUnit::TAM_CLAMP);
		}
	}

	if (tex2.isNull())
	{
		mBloomTexturep[1] = mBloomTexturep[0];
		return;
	}

	mBloomTexturep[1] =
		LLViewerTextureManager::getFetchedTexture(tex2, FTT_DEFAULT, true,
												  LLGLTexture::BOOST_UI);
	mBloomTexturep[1]->setAddressMode(LLTexUnit::TAM_CLAMP);
}
