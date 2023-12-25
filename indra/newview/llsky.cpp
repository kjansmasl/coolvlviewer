/**
 * @file llsky.cpp
 * @brief IndraWorld sky class
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

//	Ideas:
//		-haze should be controlled by global query from sims
//		-need secondary optical effects on sun (flare)
//		-stars should be brought down from sims
//		-star intensity should be driven by global ambient level from sims,
//		 so that eclipses, etc can be easily done.
//

#include "llviewerprecompiledheaders.h"

#include "llsky.h"

#include "llcubemap.h"
#include "llrenderutils.h"

#include "lldrawpool.h"
#include "llpipeline.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"
#include "llvowlsky.h"

LLSky gSky;

LLSky::LLSky()
:	mLightingGeneration(0),
	mUpdatedThisFrame(true),
	mOverrideSimSunPosition(false)
{
	// Set fog color
	mFogColor.mV[VRED] = mFogColor.mV[VGREEN] = mFogColor.mV[VBLUE] = 0.5f;
	mFogColor.mV[VALPHA] = 0.f;
}

void LLSky::cleanup()
{
	mVOSkyp = NULL;
	mVOWLSkyp = NULL;
}

void LLSky::destroyGL()
{
	if (mVOSkyp.notNull() && mVOSkyp->getCubeMap())
	{
		mVOSkyp->cleanupGL();
	}
	if (mVOWLSkyp.notNull())
	{
		mVOWLSkyp->cleanupGL();
	}
}

void LLSky::restoreGL()
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->restoreGL();
	}
	if (mVOWLSkyp.notNull())
	{
		mVOWLSkyp->restoreGL();
	}
}

void LLSky::resetVertexBuffers()
{
	if (gSky.mVOWLSkyp.notNull())
	{
		gSky.mVOWLSkyp->resetVertexBuffers();
		gPipeline.resetVertexBuffers(gSky.mVOWLSkyp->mDrawable);
		gPipeline.markRebuild(gSky.mVOWLSkyp->mDrawable);
	}
	if (gSky.mVOSkyp.notNull())
	{
		gPipeline.resetVertexBuffers(gSky.mVOSkyp->mDrawable);
		gPipeline.markRebuild(gSky.mVOSkyp->mDrawable);
	}
}

void LLSky::init()
{
	mVOWLSkyp =
		(LLVOWLSky*)gObjectList.createObjectViewer(LLViewerObject::LL_VO_WL_SKY,
												   NULL);

	gPipeline.createObject(mVOWLSkyp.get());

	mVOSkyp =
		(LLVOSky*)gObjectList.createObjectViewer(LLViewerObject::LL_VO_SKY,
												 NULL);

	mVOSkyp->initSunDirection(LLVector3::x_axis);

	gPipeline.createObject((LLViewerObject*)mVOSkyp);

	setSunDirection(LLVector3::x_axis, LLVector3::zero);

	mUpdatedThisFrame = true;
}

void LLSky::setCloudDensityAtAgent(F32 cloud_density)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setCloudDensity(cloud_density);
	}
}

void LLSky::setWind(const LLVector3& average_wind)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setWind(average_wind);
	}
}

LLColor4 LLSky::getSkyFogColor() const
{
	return mVOSkyp.notNull() ? mVOSkyp->getSkyFogColor() : LLColor4::white;
}

void LLSky::updateFog(F32 distance)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->updateFog(distance);
	}
}

void LLSky::updateSky()
{
	if (mVOSkyp.notNull() &&
		gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
	{
		mVOSkyp->updateSky();
	}
}

//-----------------------------------------------------------------------------
// Windlight renderer specific methods
//-----------------------------------------------------------------------------

void LLSky::setOverrideSun(bool override_sun)
{
	if (!mOverrideSimSunPosition && override_sun)
	{
		mLastSunDirection = getSunDirection();
	}
	else if (mOverrideSimSunPosition && !override_sun)
	{
		setSunDirection(mLastSunDirection, LLVector3::zero);
	}
	mOverrideSimSunPosition = override_sun;
}

void LLSky::setSunDirection(const LLVector3& sun_direction,
							const LLVector3& sun_ang_velocity)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setSunDirection(sun_direction, sun_ang_velocity);
	}
}

LLVector3 LLSky::getSunDirection() const
{
	return mVOSkyp.notNull() ? mVOSkyp->getToSun() : LLVector3::z_axis;
}

LLVector3 LLSky::getMoonDirection() const
{
	return mVOSkyp.notNull() ? mVOSkyp->getToMoon() : LLVector3::z_axis;
}

LLColor4 LLSky::getSunDiffuseColor() const
{
	return mVOSkyp.notNull() ? LLColor4(mVOSkyp->getSunDiffuseColor())
							 : LLColor4::white;
}

LLColor4 LLSky::getSunAmbientColor() const
{
	return mVOSkyp.notNull() ? mVOSkyp->getSunAmbientColor()
							 : LLColor4::black;
}

LLColor4 LLSky::getMoonDiffuseColor() const
{
	return mVOSkyp.notNull() ? LLColor4(mVOSkyp->getMoonDiffuseColor())
							 : LLColor4::white;
}

LLColor4 LLSky::getMoonAmbientColor() const
{
	return mVOSkyp.notNull() ? mVOSkyp->getMoonAmbientColor()
							 : LLColor4::transparent;
}

LLColor4 LLSky::getTotalAmbientColor() const
{
	return mVOSkyp.notNull() ? mVOSkyp->getTotalAmbientColor()
							 : LLColor4::white;
}

bool LLSky::sunUp() const
{
	return getSunDirection().mV[2] >= NIGHTTIME_ELEVATION_COS;
}

LLColor4U LLSky::getFadeColor() const
{
	return mVOSkyp.notNull() ? mVOSkyp->getFadeColor() : LLColor4U::white;
}

void LLSky::propagateHeavenlyBodies(F32 dt)
{
	if (!mOverrideSimSunPosition)
	{
		LLVector3 curr_dir = getSunDirection();
		LLVector3 diff = mSunTargDir - curr_dir;
		F32 dist = diff.normalize();
		if (dist > 0.f)
		{
			F32 step = llmin(dist, 0.00005f);
			diff *= step;
			curr_dir += diff;
			curr_dir.normalize();
			if (mVOSkyp.notNull())
			{
				mVOSkyp->setSunDirection(curr_dir, LLVector3::zero);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Extended environment specific methods
//-----------------------------------------------------------------------------
void LLSky::setSunScale(F32 sun_scale)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setSunScale(sun_scale);
	}
}

void LLSky::setMoonScale(F32 moon_scale)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setMoonScale(moon_scale);
	}
}

void LLSky::setSunTextures(const LLUUID& sun_tex1, const LLUUID& sun_tex2)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setSunTextures(sun_tex1, sun_tex2);
	}
}

void LLSky::setMoonTextures(const LLUUID& moon_tex1, const LLUUID& moon_tex2)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setMoonTextures(moon_tex1, moon_tex2);
	}
}

void LLSky::setCloudNoiseTextures(const LLUUID& noise_tex1,
								  const LLUUID& noise_tex2)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setCloudNoiseTextures(noise_tex1, noise_tex2);
	}
}

void LLSky::setBloomTextures(const LLUUID& bloom_tex1,
							 const LLUUID& bloom_tex2)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setBloomTextures(bloom_tex1, bloom_tex2);
	}
}

void LLSky::setSunAndMoonDirectionsCFR(const LLVector3& sun_direction,
									   const LLVector3& moon_direction)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setSunAndMoonDirectionsCFR(sun_direction, moon_direction);
	}
}

void LLSky::setSunDirectionCFR(const LLVector3& sun_direction)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setSunDirectionCFR(sun_direction);
	}
}

void LLSky::setMoonDirectionCFR(const LLVector3& moon_direction)
{
	if (mVOSkyp.notNull())
	{
		mVOSkyp->setMoonDirectionCFR(moon_direction);
	}
}

//-----------------------------------------------------------------------------

static void render_sun_moon_beacons(const LLVector3& pos_agent,
									const LLVector3& direction,
									const LLColor4& color)
{
	LLGLSUIDefault gls_ui;

	gUIProgram.bind();

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	LLVector3 pos_end;
	for (S32 i = 0; i < 3; ++i)
	{
		pos_end.mV[i] = pos_agent.mV[i] + (50 * direction.mV[i]);
	}

	gGL.lineWidth(LLPipeline::DebugBeaconLineWidth);
	gGL.begin(LLRender::LINES);
	gGL.color4fv(color.mV);
	gl_draw_3d_cross_lines(pos_agent, 0.5f, 0.5f, 0.5f);
	gl_draw_3d_cross_lines(pos_end, 2.f, 2.f, 2.f);
	gGL.vertex3fv(pos_agent.mV);
	gGL.vertex3fv(pos_end.mV);
	gGL.end(true);
	gGL.lineWidth(1.f);

	gUIProgram.unbind();

	stop_glerror();
}

void LLSky::addSunMoonBeacons()
{
	if (!isAgentAvatarValid() || mVOSkyp.isNull()) return;

	static LLCachedControl<bool> show_sun(gSavedSettings, "sunbeacon");
	if (show_sun)
	{
		static LLColor4 sun_beacon_color(1.f, 0.5f, 0.f, 0.5f);
		render_sun_moon_beacons(gAgentAvatarp->getPositionAgent(),
								mVOSkyp->getSun().getDirection(),
								sun_beacon_color);
	}

	static LLCachedControl<bool> show_moon(gSavedSettings, "moonbeacon");
	if (show_moon)
	{
		static LLColor4 moon_beacon_color(1.f, 0.f, 0.8f, 0.5f);
		render_sun_moon_beacons(gAgentAvatarp->getPositionAgent(),
								mVOSkyp->getMoon().getDirection(),
								moon_beacon_color);
	}
}
