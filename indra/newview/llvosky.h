/**
 * @file llvosky.h
 * @brief LLVOSky class header file
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

#ifndef LL_LLVOSKY_H
#define LL_LLVOSKY_H

#include "stdtypes.h"

#include "llcolor3.h"
#include "llcolor4u.h"
#include "llframetimer.h"
#include "llimage.h"
#include "llquaternion.h"
#include "llsettingssky.h"

#include "llviewerobject.h"
#include "llviewertexture.h"

class LLCubeMap;
class LLFace;

///////////////////////////////////////////////////////////////////////////////
// Lots of constants. Will clean these up at some point...

constexpr F32 HORIZON_DIST			= 1024.0f;
constexpr F32 SKY_BOX_MULT			= 16.0f;
constexpr F32 HEAVENLY_BODY_DIST	= HORIZON_DIST - 10.f;
constexpr F32 HEAVENLY_BODY_FACTOR	= 0.1f;
constexpr F32 HEAVENLY_BODY_SCALE	= HEAVENLY_BODY_DIST * HEAVENLY_BODY_FACTOR;
constexpr F32 EARTH_RADIUS			= 6.4e6f;	// Eact radius = 6.37 x 10^6 m
constexpr F32 ATM_EXP_FALLOFF		= 0.000126f;
constexpr F32 ATM_SEA_LEVEL_NDENS	= 2.55e25f;
// Somewhat arbitrary:
constexpr F32 ATM_HEIGHT			= 100000.f;

constexpr F32 FIRST_STEP			= 5000.f;
constexpr F32 INV_FIRST_STEP		= 1.f / FIRST_STEP;
constexpr S32 NO_STEPS				= 15;
constexpr F32 INV_NO_STEPS			= 1.f / NO_STEPS;

// Constants used in calculation of scattering coeff of clear air
constexpr F32 sigma					= 0.035f;
constexpr F32 fsigma				= (6.f + 3.f * sigma) / (6.f - 7.f * sigma);
constexpr F64 Ndens					= 2.55e25;
constexpr F64 Ndens2				= Ndens * Ndens;

constexpr F32 NIGHTTIME_ELEVATION = -8.f; // Degrees
// While gcc allows to compute sinf(NIGHTTIME_ELEVATION * DEG_TO_RAD) at
// compile time, clang does not, so we cannot use constexpr here... :-(
extern const F32 NIGHTTIME_ELEVATION_COS;

// *HACK: allow server to change Sun and Moon Ids. I cannot figure out how to
// pass the appropriate information into the LLVOSky constructor. JC
extern LLUUID gSunTextureID;
extern LLUUID gMoonTextureID;

class LLSkyTex
{
	friend class LLVOSky;

public:
	LL_INLINE static F32 getInterpVal()						{ return sInterpVal; }
	LL_INLINE static void setInterpVal(F32 v)				{ sInterpVal = v; }
	LL_INLINE static bool doInterpolate()					{ return sInterpVal > 0.001f; }

	void bindTexture(bool curr = true);

protected:
	LLSkyTex();
	void init(bool shiny);
	void cleanupGL();
	void restoreGL();

	~LLSkyTex();

	LL_INLINE static S32 getResolution()					{ return sResolution; }

	LL_INLINE static S32 getCurrent()						{ return sCurrent; }

	LL_INLINE static S32 stepCurrent()
	{
		++sCurrent;
		sCurrent &= 1;
		return sCurrent;
	}

	LL_INLINE static S32 getNext()							{ return ((sCurrent + 1) & 1); }
	LL_INLINE static S32 getWhich(bool curr)				{ return curr ? sCurrent : getNext(); }

	void initEmpty(S32 tex);

	void create();

	LL_INLINE void setDir(const LLVector3& dir, S32 i, S32 j)
	{
		S32 offset = i * sResolution + j;
		mSkyDirs[offset] = dir;
	}

	LL_INLINE const LLVector3& getDir(S32 i, S32 j) const
	{
		S32 offset = i * sResolution + j;
		return mSkyDirs[offset];
	}

	LL_INLINE void setPixel(const LLColor4& col, S32 i, S32 j)
	{
		S32 offset = i * sResolution + j;
		mSkyData[offset] = LLColor4U(col);
	}

	LL_INLINE void setPixel(const LLColor4U& col, S32 i, S32 j)
	{
		S32 offset = (i * sResolution + j) * sComponents;
		U32* pix = (U32*)&(mImageRaw[sCurrent]->getData()[offset]);
		*pix = col.asRGBA();
	}

	LL_INLINE LLColor4U getPixel(S32 i, S32 j)
	{
		LLColor4U col;
		S32 offset = (i * sResolution + j) * sComponents;
		U32* pix = (U32*)&(mImageRaw[sCurrent]->getData()[offset]);
		col.fromRGBA(*pix);
		return col;
	}

	LL_INLINE LLImageRaw* getImageRaw(bool curr = true)		{ return mImageRaw[getWhich(curr)]; }

	void createGLImage(S32 tex);

private:
	LLPointer<LLViewerTexture>	mTexture[2];
	LLPointer<LLImageRaw>		mImageRaw[2];
	LLColor4U*					mSkyData;
	LLVector3*					mSkyDirs;	// Cache of sky direction vectors
	bool						mIsShiny;

	static S32					sResolution;
	static S32					sComponents;
	static S32					sCurrent;
	static F32					sInterpVal;
};

/// TODO Move into the stars draw pool (and rename them appropriately).
class LLHeavenBody
{
public:
	LLHeavenBody(F32 rad)
	:	mIntensity(0.f),
		mDiskRadius(rad),
		mHorizonVisibility(1.f),
		mVisibility(1.f),
		mVisible(false),
		mDraw(false)
	{
		mColor.setToBlack();
		mColorCached.setToBlack();
	}

	LL_INLINE const LLVector3& getDirection() const			{ return mDirection; }
	LL_INLINE void setDirection(const LLVector3& direction)	{ mDirection = direction; }
	LL_INLINE void setAngularVelocity(const LLVector3& av)	{ mAngularVelocity = av; }
	LL_INLINE const LLVector3& getAngularVelocity() const	{ return mAngularVelocity; }
	LL_INLINE void setRotation(const LLQuaternion& rot)		{ mRotation = rot; }
	LL_INLINE const LLQuaternion& getRotation() const		{ return mRotation; }

	LL_INLINE const LLVector3& getDirectionCached() const	{ return mDirectionCached; }
	LL_INLINE void renewDirection()							{ mDirectionCached = mDirection; }

	LL_INLINE const LLColor3& getColorCached() const		{ return mColorCached; }
	LL_INLINE void setColorCached(const LLColor3& c)		{ mColorCached = c; }
	LL_INLINE const LLColor3& getColor() const				{ return mColor; }
	LL_INLINE void setColor(const LLColor3& c)				{ mColor = c; }

	LL_INLINE void renewColor()								{ mColorCached = mColor; }

	LL_INLINE static F32 interpVal()						{ return sInterpVal; }
	LL_INLINE static void setInterpVal(F32 v)				{ sInterpVal = v; }

	LL_INLINE LLColor3 getInterpColor() const
	{
		return sInterpVal * mColor + (1 - sInterpVal) * mColorCached;
	}

	LL_INLINE const F32& getHorizonVisibility() const		{ return mHorizonVisibility; }
	LL_INLINE void setHorizonVisibility(F32 c = 1)			{ mHorizonVisibility = c; }
	LL_INLINE const F32& getVisibility() const				{ return mVisibility; }
	LL_INLINE void setVisibility(F32 c = 1)					{ mVisibility = c; }

	LL_INLINE F32 getHaloBrighness() const
	{
		return llmax(0.f, llmin(0.9f, mHorizonVisibility)) * mVisibility;
	}

	LL_INLINE bool isVisible() const						{ return mVisible; }
	LL_INLINE void setVisible(bool v)						{ mVisible = v; }

	LL_INLINE const F32& getIntensity() const				{ return mIntensity; }
	LL_INLINE void setIntensity(F32 c)						{ mIntensity = c; }

	LL_INLINE void setDiskRadius(F32 radius)				{ mDiskRadius = radius; }
	LL_INLINE F32 getDiskRadius() const						{ return mDiskRadius; }

	LL_INLINE void setDraw(bool draw)						{ mDraw = draw; }
	LL_INLINE bool getDraw() const							{ return mDraw; }

	LL_INLINE const LLVector3& corner(S32 n) const			{ return mQuadCorner[n]; }
	LL_INLINE LLVector3& corner(S32 n)						{ return mQuadCorner[n]; }
	LL_INLINE const LLVector3* corners() const				{ return mQuadCorner; }

	LL_INLINE const LLVector3& getU() const					{ return mU; }
	LL_INLINE const LLVector3& getV() const					{ return mV; }
	LL_INLINE void setU(const LLVector3& u)					{ mU = u; }
	LL_INLINE void setV(const LLVector3& v)					{ mV = v; }

protected:
	// *HACK: for events that should not happen every frame
	LLVector3		mDirectionCached;

	LLColor3		mColor;
	LLColor3		mColorCached;
	F32				mIntensity;
	LLVector3		mDirection;			// Direction of the local heavenly body
	LLVector3		mAngularVelocity;	// Velocity of the local heavenly body
	LLQuaternion	mRotation;

	F32				mDiskRadius;

	// Number [0, 1] due to how horizon
	F32				mHorizonVisibility;
	// Same but due to other objects being in throng.
	F32				mVisibility;

	LLVector3		mQuadCorner[4];
	LLVector3		mU;
	LLVector3		mV;
	LLVector3		mO;

	bool			mDraw;				// When false, do not draw.
	bool			mVisible;

	static F32		sInterpVal;
};

class LLVOSky final : public LLStaticViewerObject
{
public:
	enum
	{
		FACE_SIDE0,
		FACE_SIDE1,
		FACE_SIDE2,
		FACE_SIDE3,
		FACE_SIDE4,
		FACE_SIDE5,
		FACE_SUN,			// was 6
		FACE_MOON,			// was 7
		FACE_BLOOM,			// was 8
		FACE_REFLECTION,	// was 10
		FACE_COUNT
	};

	LLVOSky(const LLUUID& id, LLViewerRegion* regionp);

	// Initialize/delete data that is only inited once per class.
	void init();

	void cleanupGL();
	void restoreGL();

	// Nothing to do.
	LL_INLINE void idleUpdate(F64) override					{}

	bool updateSky();

	void updateTextures() override;
	LLDrawable* createDrawable() override;
	bool updateGeometry(LLDrawable* drawable) override;

	LL_INLINE const LLHeavenBody& getSun() const			{ return mSun; }
	LL_INLINE const LLHeavenBody& getMoon() const			{ return mMoon; }

	LL_INLINE const LLVector3& getToSunLast() const			{ return mSun.getDirectionCached(); }
	LL_INLINE const LLVector3& getToSun() const				{ return mSun.getDirection(); }
	LL_INLINE const LLVector3& getToMoon() const			{ return mMoon.getDirection(); }
	LL_INLINE const LLVector3& getToMoonLast() const		{ return mMoon.getDirectionCached(); }

	LL_INLINE LLColor3 getSunDiffuseColor() const			{ return mSunDiffuse; }
	LL_INLINE LLColor3 getMoonDiffuseColor() const			{ return mMoonDiffuse; }
	LL_INLINE LLColor4 getSunAmbientColor() const			{ return mSunAmbient; }
	LL_INLINE LLColor4 getMoonAmbientColor() const			{ return mMoonAmbient; }
	LL_INLINE const LLColor4& getTotalAmbientColor() const	{ return mTotalAmbient; }
	LL_INLINE LLColor4 getSkyFogColor() const				{ return mFogColor; }
	LL_INLINE LLColor4 getGLFogColor() const				{ return mGLFogCol; }

	void initSunDirection(const LLVector3& sun_dir);
	void setSunDirection(const LLVector3& sun_dir,
						 const LLVector3& sun_ang_velocity);

	// Directions provided should already be in CFR coord sys (+x at, +z up,
	// +y right)
	void setSunDirectionCFR(const LLVector3& sun_direction);

	void setMoonDirectionCFR(const LLVector3& moon_dir);

	LL_INLINE void setSunAndMoonDirectionsCFR(const LLVector3& sun_dir,
											  const LLVector3& moon_dir)
	{
		mMoon.setDirection(moon_dir);
		setSunDirectionCFR(sun_dir);		
	}

	LL_INLINE F32 getWorldScale() const						{ return mWorldScale; }
	LL_INLINE void setWorldScale(F32 s)						{ mWorldScale = s; }
	void updateFog(F32 distance);
	LL_INLINE LLColor4U getFadeColor() const				{ return mFadeColor; }
	LL_INLINE void setCloudDensity(F32 cloud_density)		{ mCloudDensity = cloud_density; }
	LL_INLINE void setWind(const LLVector3& wind)			{ mWind = wind.length(); }

	LL_INLINE const LLVector3& getCameraPosAgent() const	{ return mCameraPosAgent; }
	LL_INLINE LLVector3 getEarthCenter() const				{ return mEarthCenter; }

	LL_INLINE LLCubeMap* getCubeMap() const					{ return mCubeMap; }
	LL_INLINE S32 getDrawRefl() const						{ return mDrawRefl; }
	LL_INLINE void setDrawRefl(S32 r)						{ mDrawRefl = r; }
	LL_INLINE bool isReflFace(LLFace* face) const			{ return face == mFace[FACE_REFLECTION]; }
	LL_INLINE LLFace* getReflFace() const					{ return mFace[FACE_REFLECTION]; }

	LL_INLINE void setSunScale(F32 scale)					{ mSunScale = scale; }
	LL_INLINE void setMoonScale(F32 scale)					{ mMoonScale = scale; }

	LL_INLINE LLViewerTexture* getSunTex() const			{ return mSunTexturep[0].get(); }
	LL_INLINE LLViewerTexture* getMoonTex() const			{ return mMoonTexturep[0].get(); }
	LL_INLINE LLViewerTexture* getBloomTex() const			{ return mBloomTexturep[0].get(); }
	LL_INLINE LLViewerTexture* getCloudNoiseTex() const		{ return mCloudNoiseTexturep[0].get(); }

	LL_INLINE LLViewerTexture* getSunTexNext() const		{ return mSunTexturep[1].get(); }
	LL_INLINE LLViewerTexture* getMoonTexNext() const		{ return mMoonTexturep[1].get(); }
	LL_INLINE LLViewerTexture* getBloomTexNext() const		{ return mBloomTexturep[1].get(); }
	LL_INLINE LLViewerTexture* getCloudNoiseTexNext() const	{ return mCloudNoiseTexturep[1].get(); }

	void setSunTextures(const LLUUID& sun_tex1, const LLUUID& sun_tex2);
	void setMoonTextures(const LLUUID& moon_tex1, const LLUUID& moon_tex2);
	void setCloudNoiseTextures(const LLUUID& cld_tex1, const LLUUID& cld_tex2);
	void setBloomTextures(const LLUUID& bloom_tex1, const LLUUID& bloom_tex2);

	LL_INLINE LLViewerTexture* getRainbowTex() const		{ return mRainbowMap.get(); }
	LL_INLINE LLViewerTexture* getHaloTex() const			{ return mHaloMap.get(); }

	LL_INLINE void forceSkyUpdate()							{ mForceUpdate = true; }

	LL_INLINE F32 getInterpVal() const						{ return mInterpVal; }

protected:
	~LLVOSky() override;

private:
	void initCubeMap();
	void initAtmospherics(const LLSettingsSky::ptr_t& skyp);
	void calcAtmospherics();	// AKA calc() in LL's viewer

	void updateDirections(const LLSettingsSky::ptr_t& skyp);

	void initSkyTextureDirs(S32 side, S32 tile);
	void createSkyTexture(const LLSettingsSky::ptr_t& skyp, S32 side,
						  S32 tile);

	LLColor4 calcSkyColorInDir(const LLSettingsSky::ptr_t& skyp,
							   const LLVector3& dir, bool is_shiny = false);

	void calcSkyColorVert(LLVector3& pn);

	bool haveValuesChanged();
	void saveCurrentValues();

	LL_INLINE F32 cosHorizon() const
	{
		const F32 sin_angle = EARTH_RADIUS /
							  (EARTH_RADIUS + mCameraPosAgent.mV[2]);
		return -sqrtf(1.f - sin_angle * sin_angle);
	}

	bool updateHeavenlyBodyGeometry(LLDrawable* drawable, bool is_sun,
									LLHeavenBody& hb, F32 sin_max_angle,
									const LLVector3& up,
									const LLVector3& right);

	void updateReflectionGeometry(LLDrawable* drawable, F32 h,
								  const LLHeavenBody& hb);
protected:
	typedef LLPointer<LLViewerFetchedTexture> tex_ptr_t;
	tex_ptr_t			mSunTexturep[2];
	tex_ptr_t			mMoonTexturep[2];
	tex_ptr_t			mBloomTexturep[2];
	tex_ptr_t			mCloudNoiseTexturep[2];
	tex_ptr_t			mRainbowMap;
	tex_ptr_t			mHaloMap;

	static S32			sResolution;
	static S32			sTileResX;
	static S32			sTileResY;

	LLSkyTex			mSkyTex[6];
	LLSkyTex			mShinyTex[6];
	LLHeavenBody		mSun;
	LLHeavenBody		mMoon;
	F32					mSunScale;
	F32					mMoonScale;
	LLVector3			mSunAngVel;
	LLVector3			mEarthCenter;
	LLVector3			mCameraPosAgent;
	LLColor3			mBrightestPoint;
	LLColor3			mBrightestPointNew;
	LLColor3			mBrightestPointGuess;
	F32					mBrightnessScale;
	F32					mBrightnessScaleNew;
	F32					mBrightnessScaleGuess;
	F32					mCloudDensity;
	F32					mWind;
	F32					mAtmHeight;

	LLVector3			mLastLightingDirection;
	LLColor3			mLastTotalAmbient;
	LLColor3			mNightColorShift;
	F32					mAmbientScale;
	F32					mInterpVal;

	LLColor4			mFogColor;
	LLColor4			mGLFogCol;

	F32					mWorldScale;

	LLColor4			mSunAmbient;
	LLColor4			mMoonAmbient;
	LLColor4			mTotalAmbient;
	LLColor3			mSunDiffuse;
	LLColor3			mMoonDiffuse;
	LLColor4U			mFadeColor;		// Color to fade in from

	LLPointer<LLCubeMap> mCubeMap;		// Cube map for the environment
	// State of cubemap uodate: -1 idle; 0-5 per-face updates; 6 finalizing
	S32					mCubeMapUpdateStage;
	// Do partial work to amortize cost of updating
	S32					mCubeMapUpdateTile;

	S32					mDrawRefl;

	LLFrameTimer		mUpdateTimer;
	LLTimer				mForceUpdateThrottle;

	// Windlight parameters
	F32					mDomeRadius;
	F32					mDomeOffset;
	F32					mGamma;
	F32					mHazeDensity;
	F32					mHazeHorizon;
	F32					mDensityMultiplier;
	F32					mMaxY;
	F32					mCloudShadow;
	LLVector4			mSunNorm;
	LLVector4			mUnClampedSunNorm;
	LLColor3			mGlow;
	LLColor3			mSunlight;
	LLColor3			mAmbient;
	LLColor3			mBlueDensity;
	LLColor3			mBlueHorizon;

	// Extended environment parameters
	LLColor3			mHazeColor;
	LLColor3			mLightAttenuation;
	LLColor3			mLightTransmittance;
	LLColor3			mTotalDensity;
#if 0	// 'hazeColorBelowCloud' in LL's EEP viewer (which would be
		// mHazeColorBelowCloud for the Cool VL Viewer) is computed but never
		// truly used (excepted in what would be in haveValuesChanged() for
		// us)...
	LLColor3			mHazeColorBelowCloud;
#endif

	// Old values of above parameters, used to detect a needed update
	F32					mOldGamma;
	F32					mOldHazeDensity;
	F32					mOldHazeHorizon;
	F32					mOldDensityMultiplier;
	F32					mOldMaxY;
	F32					mOldCloudShadow;
	LLVector4			mOldSunNorm;
	LLColor3			mOldGlow;
	LLColor3			mOldSunlight;
	LLColor3			mOldAmbient;
	LLColor3			mOldBlueDensity;
	LLColor3			mOldBlueHorizon;
#if 0	// These components are derived from the above ones, so need to save
		// and test them... HB
	LLColor3			mOldLightAttenuation;
	LLColor3			mOldLightTransmittance;
	LLColor3			mOldTotalDensity;
#endif

	// Various flags

	bool				mWeatherChange;

	bool				mInitialized;
	bool				mForceUpdate;	// Flag to force update of cubemap
	bool				mNeedUpdate;	// Flag to update of cubemap

	bool				mHeavenlyBodyUpdated;

public:
	LLFace*				mFace[FACE_COUNT];
	LLVector3			mBumpSunDir;
};

#endif
