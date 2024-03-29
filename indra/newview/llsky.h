/**
 * @file llsky.h
 * @brief It's, uh, the sky!
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

#ifndef LL_LLSKY_H
#define LL_LLSKY_H

#include "llmath.h"
#include "llpointer.h"
#include "llvector3.h"
#include "llvector4.h"
#include "llcolor4.h"
#include "llcolor4u.h"
#include "llvosky.h"

class LLVOWLSky;

class LLSky
{
public:
	LLSky();

	void init();

	void cleanup();

	void destroyGL();
	void restoreGL();
	void resetVertexBuffers();

	// *TODO: do culling for WL sky properly -Brad
	LL_INLINE void updateCull()						{}

	void updateSky();

	void addSunMoonBeacons();

	LLColor4 getSkyFogColor() const;

	void setCloudDensityAtAgent(F32 cloud_density);
	void setWind(const LLVector3& wind);

	void updateFog(F32 distance);

	// Windlight specific methods

	void setSunDirection(const LLVector3& sun_direction,
						 const LLVector3& sun_ang_velocity);

	void setOverrideSun(bool override_sun);
	LL_INLINE bool getOverrideSun()					{ return mOverrideSimSunPosition; }

	LL_INLINE void setSunTargetDirection(const LLVector3& sun_direction,
										 const LLVector3& sun_ang_velocity)
	{
		mSunTargDir = sun_direction;
	}

	void propagateHeavenlyBodies(F32 dt);	// dt = seconds

	LLColor4U getFadeColor() const;

	LLVector3 getSunDirection() const;
	LLVector3 getMoonDirection() const;
	LLColor4 getSunDiffuseColor() const;
	LLColor4 getMoonDiffuseColor() const;
	LLColor4 getSunAmbientColor() const;
	LLColor4 getMoonAmbientColor() const;
	LLColor4 getTotalAmbientColor() const;
	bool sunUp() const;

	// Extended environment specific methods

	void setSunScale(F32 sun_scale);
	void setMoonScale(F32 moon_scale);

	// These directions should be in CFR coord sys (+x at, +z up, +y right)
	void setSunAndMoonDirectionsCFR(const LLVector3& sun_direction,
									const LLVector3& moon_direction);
	void setSunDirectionCFR(const LLVector3& sun_direction);
	void setMoonDirectionCFR(const LLVector3& moon_direction);

	void setSunTextures(const LLUUID& sun_tex1,
						const LLUUID& sun_tex2 = LLUUID::null);
	void setMoonTextures(const LLUUID& moon_tex1,
						 const LLUUID& moon_tex2 = LLUUID::null);
	void setCloudNoiseTextures(const LLUUID& cld_tex1,
							   const LLUUID& cld_tex2 = LLUUID::null);
	void setBloomTextures(const LLUUID& bloom_tex1,
						  const LLUUID& bloom_tex2 = LLUUID::null);

public:
	// Pointer to the LLVOSky object (only one, ever!)
	LLPointer<LLVOSky>		mVOSkyp;
	LLPointer<LLVOWLSky>	mVOWLSkyp;

	LLVector3				mSunTargDir;

	S32						mLightingGeneration;

	bool					mUpdatedThisFrame;

protected:
	bool					mOverrideSimSunPosition;

	LLColor4				mFogColor;	// Color to use for fog and haze

	LLVector3				mLastSunDirection;
};

extern LLSky gSky;
#endif
