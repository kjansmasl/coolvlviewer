/**
 * @file llreflectionmapmanager.cpp
 * @brief LLReflectionMap and LLReflectionMapManager classes implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewergpl$
 *
 * Copyright (c) 2022, Linden Research, Inc.
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

#include "llreflectionmapmanager.h"

#include "llappviewer.h"
#include "llenvironment.h"
#include "llpipeline.h"
#include "llspatialpartition.h"
#include "llstartup.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"			// For gTeleportDisplay, gCubeSnapshot
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "llworld.h"

///////////////////////////////////////////////////////////////////////////////
// LLReflectionMap class
///////////////////////////////////////////////////////////////////////////////

LLReflectionMap::LLReflectionMap()
:	mGroup(NULL),
	mViewerObject(NULL),
	mCubeIndex(-1),
	mDistance(-1.f),
	mMinDepth(-1.f),
	mMaxDepth(-1.f),
	mRadius(16.f),
	mLastUpdateTime(0.f),
	mLastBindTime(0.f),
	mFadeIn(0.f),
	mProbeIndex(-1),
	mPriority(0),
	mOcclusionQuery(0),
	mOcclusionPendingFrames(0),
	mOccluded(false),
	mComplete(false)
{
	mOrigin.clear();
}

LLReflectionMap::~LLReflectionMap()
{
	if (mOcclusionQuery)
	{
		glDeleteQueries(1, &mOcclusionQuery);
	}
	mViewerObject = NULL;
}

void LLReflectionMap::update(U32 resolution, U32 face)
{
	if (!gUsePBRShaders || mCubeIndex == -1 || mCubeArray.isNull())
	{
		return;
	}
	mLastUpdateTime = gFrameTimeSeconds;
	// Make sure we do not walk off the edge of the render target
	while (resolution > gPipeline.mRT->mDeferredScreen.getWidth() ||
		   resolution > gPipeline.mRT->mDeferredScreen.getHeight())
	{
		resolution /= 2;
	}
	gViewerWindowp->cubeSnapshot(LLVector3(mOrigin.getF32ptr()), mCubeArray,
								 face, getNearClip(), getIsDynamic());
}

void LLReflectionMap::autoAdjustOrigin()
{
	if (mComplete || !mGroup)
	{
		if (mViewerObject && !mViewerObject->isDead())
		{
			mPriority = 1;
			mOrigin.load3(mViewerObject->getPositionAgent().mV);
			bool got_radius = false;
			if (mViewerObject->getVolume())
			{
				LLVOVolume* vobjp = mViewerObject->asVolume();
				if (vobjp && vobjp->getReflectionProbeIsBox())
				{
					static const LLVector3 half(0.5f, 0.5f, 0.5f);
					mRadius = vobjp->getScale().scaledVec(half).length();
					got_radius = true;
				}
			}
			if (!got_radius)
			{
				mRadius = mViewerObject->getScale().mV[0] * 0.5f;
			}
		}
		return;
	}
	if (!mGroup->getOctreeNode())
	{
		return;
	}
	if (mGroup->getSpatialPartition()->mPartitionType !=
			LLViewerRegion::PARTITION_VOLUME)
	{
		return;
	}

	mPriority = 0;

	// Cast a ray towards 8 corners of bounding box nudge origin towards center
	// of empty space
	const LLVector4a* bounds = mGroup->getBounds();
	mOrigin = bounds[0];
	LLVector4a size = bounds[1];

	LLVector4a corners[] =
	{
		{ 1.f, 1.f, 1.f },
		{ -1.f, 1.f, 1.f },
		{ 1.f, -1.f, 1.f },
		{ -1.f, -1.f, 1.f },
		{ 1.f, 1.f, -1.f },
		{ -1.f, 1.f, -1.f },
		{ 1.f, -1.f, -1.f },
		{ -1.f, -1.f, -1.f }
	};
	for (U32 i = 0; i < 8; ++i)
	{
		corners[i].mul(size);
		corners[i].add(bounds[0]);
	}

	LLVector4a extents[2];
	extents[0].setAdd(bounds[0], bounds[1]);
	extents[1].setSub(bounds[0], bounds[1]);

	LLVector4a intersection;
	bool hit = false;
	for (U32 i = 0; i < 8; ++i)
	{
		S32 face = -1;
		LLDrawable* drawablep =
			mGroup->lineSegmentIntersect(bounds[0], corners[i], false, false,
										 &face, &intersection);
		if (drawablep)
		{
			hit = true;
			update_min_max(extents[0], extents[1], intersection);
		}
		else
		{
			update_min_max(extents[0], extents[1], corners[i]);
		}
	}

	if (hit)
	{
		mOrigin.setAdd(extents[0], extents[1]);
		mOrigin.mul(0.5f);
	}

	// Make sure origin is not under the ground
	F32* fp = mOrigin.getF32ptr();
	LLVector3 origin(fp);
	F32 height = gWorld.resolveLandHeightAgent(origin) + 2.f;
	fp[2] = llmax(fp[2], height);

	// Make sure radius encompasses all objects
	LLSimdScalar r2 = 0.0;
	for (S32 i = 0; i < 8; ++i)
	{
		LLVector4a v;
		v.setSub(corners[i], mOrigin);

		LLSimdScalar d = v.dot3(v);
		if (d > r2)
		{
			r2 = d;
		}
	}

	mRadius = llmax(sqrtf(r2.getF32()), 8.f);

	// Make sure near clip does not poke through ground
	fp[2] = llmax(fp[2], height + mRadius * 0.5f);
}

bool LLReflectionMap::intersects(LLReflectionMap* otherp)
{
	LLVector4a delta;
	delta.setSub(otherp->mOrigin, mOrigin);

	F32 r = mRadius + otherp->mRadius;
	return delta.dot3(delta).getF32() < r * r;
}

F32 LLReflectionMap::getAmbiance()
{
	F32 ret = 0.f;
	if (mViewerObject && !mViewerObject->isDead() &&
		mViewerObject->getVolume())
	{
		LLVOVolume* vobjp = mViewerObject->asVolume();
		if (vobjp)
		{
			ret = vobjp->getReflectionProbeAmbiance();
		}
	}
	return ret;
}

F32 LLReflectionMap::getNearClip()
{
	F32 ret = 1.f;	// Default to 1m for automatic terrain probes
	if (mViewerObject && !mViewerObject->isDead() &&
		mViewerObject->getVolume())
	{
		LLVOVolume* vobjp = mViewerObject->asVolume();
		if (vobjp)
		{
			ret = vobjp->getReflectionProbeNearClip();
		}
	}
	else if (mGroup)
	{
		// Default to half radius for automatic object probes
		ret = mRadius * 0.5f;
	}
	constexpr F32 MINIMUM_NEAR_CLIP = 0.1f;
	return llmax(ret, MINIMUM_NEAR_CLIP);
}

bool LLReflectionMap::getIsDynamic()
{
	static LLCachedControl<U32> probe_detail(gSavedSettings,
											 "RenderReflectionProbeDetail");
	if (!mViewerObject || mViewerObject->isDead() ||
		!mViewerObject->getVolume() || (U32)probe_detail < STATIC_AND_DYNAMIC)
	{
		return false;
	}
	LLVOVolume* vovolp = mViewerObject->asVolume();
	return vovolp && vovolp->getReflectionProbeIsDynamic();
}

bool LLReflectionMap::getBox(LLMatrix4& box)
{
	if (!mViewerObject || mViewerObject->isDead())
	{
		return false;
	}

	LLVolume* volp = mViewerObject->getVolume();
	if (!volp)
	{
		return false;
	}

	LLVOVolume* vobjp = mViewerObject->asVolume();
	if (!vobjp || !vobjp->getReflectionProbeIsBox())
	{
		return false;
	}

	static const LLVector3 half(0.5f, 0.5f, 0.5f);
	LLVector3 s = vobjp->getScale().scaledVec(half);
	mRadius = s.length();

	if (vobjp->mDrawable)
	{
		// Object to agent space (no scale)
		LLMatrix4a scale;
		scale.setIdentity();
		scale.applyScaleAffine(s);
		scale.transpose();

		// Construct object to camera space (with scale)
		LLMatrix4a mv = gGLModelView;
		LLMatrix4a rm(vobjp->mDrawable->getWorldMatrix());
		mv.mul(rm);
		mv.mul(scale);

		// Inverse is camera space to object unit cube
		mv.invert();
		box.set(mv.getF32ptr());
		return true;
	}

	return false;
}

bool LLReflectionMap::isRelevant()
{
	static LLCachedControl<S32> probe_level(gSavedSettings,
											"RenderReflectionProbeLevel");

	if (mViewerObject && !mViewerObject->isDead() && probe_level > 0)
	{
		// Not an automatic probe
		return true;
	}

	if (probe_level == 3)
	{
		// All automatics are relevant
		return true;
	}

	if (probe_level == 2)
	{
		// Terrain and water only, ignore probes that have a group
		return !mGroup;
	}

	// No automatic probes, yes manual probes
	return mViewerObject && !mViewerObject->isDead();
}

// Super sloppy, but we are doing an occlusion cull against a bounding cube of
// a bounding sphere, pad radius so we assume if the eye is within the bounding
// sphere of the bounding cube, the node is not culled.
void LLReflectionMap::doOcclusion(const LLVector4a& eye)
{
	if (LLGLSLShader::sProfileEnabled)
	{
		return;
	}

	F32 dist = mRadius * F_SQRT3 + 1.f;

	LLVector4a o;
	o.setSub(mOrigin, eye);

	bool do_query = false;

	if (o.getLength3().getF32() < dist)
	{
		// Eye is inside radius, do not attempt to occlude
		mOccluded = false;
		return;
	}

	if (mOcclusionQuery == 0)
	{
		// No query was previously issued, allocate one and issue
		glGenQueries(1, &mOcclusionQuery);
		do_query = true;
	}
	else
	{
		// Query was previously issued, check it and only issue a new query
		// if previous query is available
		GLuint result = 0;
		glGetQueryObjectuiv(mOcclusionQuery, GL_QUERY_RESULT_AVAILABLE, &result);

		if (result > 0)
		{
			do_query = true;
			glGetQueryObjectuiv(mOcclusionQuery, GL_QUERY_RESULT, &result);
			mOccluded = result == 0;
			mOcclusionPendingFrames = 0;
		}
		else
		{
			++mOcclusionPendingFrames;
		}
	}

	if (do_query)
	{
		glBeginQuery(GL_ANY_SAMPLES_PASSED, mOcclusionQuery);

		LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;

		shaderp->uniform3fv(LLShaderMgr::BOX_CENTER, 1, mOrigin.getF32ptr());
		shaderp->uniform3f(LLShaderMgr::BOX_SIZE, mRadius, mRadius, mRadius);

		gPipeline.mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8,
									 get_box_fan_indices(&gViewerCamera,
														 mOrigin));

		glEndQuery(GL_ANY_SAMPLES_PASSED);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLReflectionMapManager class proper
///////////////////////////////////////////////////////////////////////////////

// Uniform names
static LLStaticHashedString sDirection("direction");
static LLStaticHashedString sMipLevel("mipLevel");
static LLStaticHashedString sResScale("resScale");
static LLStaticHashedString sRoughness("roughness");
static LLStaticHashedString sSourceIdx("sourceIdx");
static LLStaticHashedString sWidth("u_width");

static U32 sUpdateCount = 0;

// Helper function
static void touch_default_probe(LLReflectionMap* probep)
{
	LLVector3 origin = gViewerCamera.getOrigin();
	origin.mV[2] += 64.f;
	probep->mOrigin.load3(origin.mV);
}

LLReflectionMapManager::LLReflectionMapManager()
:	mUpdatingProbe(NULL),
	mUBO(0),
	mUpdatingFace(0),
	mReflectionProbeCount(0),
	mProbeResolution(128),
	mOldProbeResolution(0),
	mMaxProbeLOD(6.f),
	mLightScale(1.f),
	mReset(false),
	mPaused(false),
	mRadiancePass(false),
	mRealtimeRadiancePass(false)
{
	initCubeFree();
}

void LLReflectionMapManager::initCubeFree()
{
	// Start at 1 because index 0 is reserved for mDefaultProbe
	for (U32 i = 1; i < LL_MAX_REFLECTION_PROBE_COUNT; ++i)
	{
		mCubeFree.push_back(i);
	}
}

struct CompareProbeDistance
{
	LL_INLINE bool operator()(const LLPointer<LLReflectionMap>& lhs,
							  const LLPointer<LLReflectionMap>& rhs)
	{
		return lhs->mDistance < rhs->mDistance;
	}
};

static F32 update_score(LLReflectionMap* probep)
{
	return gFrameTimeSeconds - probep->mLastUpdateTime -
		   probep->mDistance * 0.1f;
}

// Returns true if a is higher priority for an update than b
static bool check_priority(LLReflectionMap* a, LLReflectionMap* b)
{
	if (a->mCubeIndex == -1)
	{
		// Not a candidate for updating
		return false;
	}
	if (b->mCubeIndex == -1)
	{
		// b is not a candidate for updating, a is higher priority by default
		return true;
	}
	if (!a->mComplete && !b->mComplete)
	{
		// Neither probe is complete, use distance
		return a->mDistance < b->mDistance;
	}
	if (a->mComplete && b->mComplete)
	{
		// Both probes are complete, use update_score metric
		return update_score(a) > update_score(b);
	}
	if (sUpdateCount % 3 == 0)
	{
		// a or b  is not complete;every third update, allow complete probes to
		// cut in line in front of non-complete probes to avoid spammy probe
		// generators from deadlocking scheduler (SL-20258).
		return !b->mComplete;
	}
	// Prioritize incomplete probe
	return b->mComplete;
}

void LLReflectionMapManager::update()
{
	if (!LLPipeline::sReflectionProbesEnabled || gCubeSnapshot ||
		gTeleportDisplay || gDisconnected || !LLStartUp::isLoggedIn() ||
		gAppViewerp->logoutRequestSent())
	{
		llassert(!gCubeSnapshot); // Assert a snapshot is not in progress
		return;
	}

	initReflectionMaps();

	llassert(mProbes[0] == mDefaultProbe);

	LLVector4a camera_pos;
	camera_pos.load3(gViewerCamera.getOrigin().mV);

	// Process kill list
	for (U32 i = 0, count = mKillList.size(); i < count; ++i)
	{
		const auto& probep = mKillList[i];
		prmap_vec_t::const_iterator start = mProbes.begin();
		prmap_vec_t::const_iterator end = mProbes.end();
		prmap_vec_t::const_iterator iter = std::find(start, end, probep);
		if (iter != end)
		{
			deleteProbe(iter - start);
		}
	}
	mKillList.clear();

	// Process create list
	for (U32 i = 0, count = mCreateList.size(); i < count; ++i)
	{
		LLReflectionMap* probep = mCreateList[i].get();
		if (probep)	// Paranoia
		{
			mProbes.emplace_back(probep);
		}
	}
	mCreateList.clear();

	if (mProbes.empty())
	{
		return;
	}

	bool did_update = false;

	static LLCachedControl<U32> detail(gSavedSettings,
									   "RenderReflectionProbeDetail");
	static LLCachedControl<U32> level(gSavedSettings,
									  "RenderReflectionProbeLevel");

	bool realtime = (U32)detail >= LLReflectionMap::REALTIME;

	if (mUpdatingProbe)
	{
		did_update = true;
		doProbeUpdate();
	}

	// Update distance to camera for all probes
	std::sort(mProbes.begin() + 1, mProbes.end(), CompareProbeDistance());
	llassert(mProbes[0] == mDefaultProbe && mProbes[0]->mCubeIndex == 0 &&
			 mProbes[0]->mCubeArray == mTexture);

	// Make sure we are assigning cube slots to the closest probes

	// First free any cube indices for distant probes
	for (U32 i = mReflectionProbeCount, count = mProbes.size(); i < count; ++i)
	{
		LLReflectionMap* probep = mProbes[i].get();
		if (probep && probep->mCubeIndex != -1 && mUpdatingProbe != probep)
		{
			mCubeFree.push_back(probep->mCubeIndex);
			probep->mCubeArray = NULL;
			probep->mCubeIndex = -1;
			probep->mComplete = false;
		}
	}

	// Next distribute the free indices
	for (U32 i = 1, count = llmin(mReflectionProbeCount, mProbes.size());
		 i < count && !mCubeFree.empty(); ++i)
	{
		// Find the closest probe that needs a cube index
		LLReflectionMap* probep = mProbes[i].get();
		if (probep && probep->mCubeIndex == -1)
		{
			S32 idx = allocateCubeIndex();
			if (!idx)	// This should not happen
			{
				llwarns << "Could not allocate a new cube index." << llendl;
				llassert(false);
			}
			probep->mCubeArray = mTexture;
			probep->mCubeIndex = idx;
		}
	}

	LLReflectionMap* closest_dynamicp = NULL;
	LLReflectionMap* oldest_probep = NULL;
	LLReflectionMap* oldest_occludedp = NULL;
	LLVector4a d;
	for (U32 i = 0, count = mProbes.size(); i < count; ++i)
	{
		LLReflectionMap* probep = mProbes[i].get();
		if (probep->getNumRefs() == 1)
		{
			// No references held outside manager, delete this probe
			deleteProbe(i);
			--i;
			count = mProbes.size();
			continue;
		}

		if (probep != mDefaultProbe.get() &&
			(mPaused || !probep->isRelevant()))
		{
			// Skip irrelevant probes (or all non-default probes when paused).
			continue;
		}

		if (probep != mDefaultProbe.get())
		{
			LLViewerObject* objp = probep->mViewerObject;
			if (objp && !objp->isDead())
			{
				// Make sure probes track the object they are attached to.
				probep->mOrigin.load3(objp->getPositionAgent().mV);
			}
			d.setSub(camera_pos, probep->mOrigin);
			probep->mDistance = d.getLength3().getF32() - probep->mRadius;
		}
		else if (probep->mComplete)
		{
			// Make default probe have a distance of 64m for the purposes of
			// prioritization (if it is already been generated once).
			probep->mDistance = 64.f;
		}
		else
		{
			// Boost priority of default probe when it is not complete
			probep->mDistance = -4096.f;
		}

		if (probep->mComplete)
		{
			probep->autoAdjustOrigin();
			probep->mFadeIn = llmin(probep->mFadeIn + gFrameIntervalSeconds,
									1.f);
		}
		if (probep->mOccluded && probep->mComplete)
		{
			if (!oldest_occludedp)
			{
				oldest_occludedp = probep;
			}
			else if (probep->mLastUpdateTime <
						oldest_occludedp->mLastUpdateTime)
			{
				oldest_occludedp = probep;
			}
		}
		else if (!did_update && i < mReflectionProbeCount &&
				(!oldest_probep || check_priority(probep, oldest_probep)))
		{
		   oldest_probep = probep;
		}

		if (realtime && !closest_dynamicp && probep->mCubeIndex != -1 &&
			probep->getIsDynamic())
		{
			closest_dynamicp = probep;
		}
	}

	if (realtime && closest_dynamicp)
	{
		// Update the closest dynamic probe realtime; should do a full
		// irradiance pass on "odd" frames and a radiance pass on "even" frames
		closest_dynamicp->autoAdjustOrigin();

		// Store and override the value of "isRadiancePass"; parts of the
		// render pipeline rely on "isRadiancePass" to set lighting values etc.
		bool radiance_pass = isRadiancePass();
		mRadiancePass = mRealtimeRadiancePass;
		for (U32 i = 0; i < 6; ++i)
		{
			updateProbeFace(closest_dynamicp, i);
		}
		mRealtimeRadiancePass = !mRealtimeRadiancePass;

		// Restore "isRadiancePass"
		mRadiancePass = radiance_pass;
	}

	static LLCachedControl<U32> upd_period(gSavedSettings,
										   "RenderDefaultProbeUpdatePeriod");
	F32 update_period = llclamp(U32(upd_period), 1, 30);
	if (gFrameTimeSeconds - mDefaultProbe->mLastUpdateTime < update_period)
	{
		if (!level)
		{
			// When probes are disabled do not update the default probe more
			// often than the prescribed update period.
			oldest_probep = NULL;
		}
	}
	else if (level)
	{
		// Wen probes are enabled do not update the default probe less often
		// than the prescribed update period.
		oldest_probep = mDefaultProbe.get();
	}

	// Switch to updating the next oldest probe
	if (!did_update && oldest_probep)
	{
		LLReflectionMap* probep = oldest_probep;
		llassert(probep->mCubeIndex != -1);
		probep->autoAdjustOrigin();
		++sUpdateCount;
		mUpdatingProbe = probep;
		doProbeUpdate();
	}

	if (oldest_occludedp)
	{
		// As far as this occluded probe is concerned, an origin/radius update
		// is as good as a full update.
		oldest_occludedp->autoAdjustOrigin();
		oldest_occludedp->mLastUpdateTime = gFrameTimeSeconds;
	}
}

LLReflectionMap* LLReflectionMapManager::addProbe(LLSpatialGroup* groupp)
{
	LLReflectionMap* probep = new LLReflectionMap();
	probep->mGroup = groupp;

	if (mDefaultProbe.isNull())
	{ 
		// Safety check to make sure default probe is always first probe added
		mDefaultProbe = new LLReflectionMap();
		mProbes.push_back(mDefaultProbe);
	}
	llassert(mProbes[0] == mDefaultProbe);

	if (groupp)
	{
		probep->mOrigin = groupp->getOctreeNode()->getCenter();
	}

	if (gCubeSnapshot)
	{
		// Snapshot is in progress, mProbes is being iterated over: defer
		// insertion until next update.
		mCreateList.emplace_back(probep);
	}
	else
	{
		mProbes.emplace_back(probep);
	}

	return probep;
}

struct CompareProbeDepth
{
	bool operator()(const LLReflectionMap* lhs, const LLReflectionMap* rhs)
	{
		return lhs->mMinDepth < rhs->mMinDepth;
	}
};

void LLReflectionMapManager::getReflectionMaps(std::vector<LLReflectionMap*>& maps)
{
	LLMatrix4a modelview = gGLModelView;
	LLVector4a oa;	// Scratch space for transformed origin

	U32 count = 0;
	U32 last_idx = 0;
	const U32 maps_size = maps.size();
	for (U32 i = 0, probes = mProbes.size(); i < probes && count < maps_size;
		 ++i)
	{
		LLReflectionMap* probep = mProbes[i].get();
		if (!probep) continue;	// Paranoia ?

		// Something wants to use this probe, so let's indicate it has been
		// requested.
		probep->mLastBindTime = gFrameTimeSeconds;
		if (probep->mCubeIndex != -1)
		{
			if (!probep->mOccluded && probep->mComplete)
			{
				maps[count++] = probep;
				modelview.affineTransform(probep->mOrigin, oa);
				F32 radius = probep->mRadius;
				probep->mMinDepth = -oa.getF32ptr()[2] - radius;
				probep->mMaxDepth = -oa.getF32ptr()[2] + radius;
			}
		}
		else
		{
			probep->mProbeIndex = -1;
		}
		last_idx = i;
	}

	// Set remaining probe indices to -1
	for (U32 i = last_idx + 1, n = mProbes.size(); i < n; ++i)
	{
		LLReflectionMap* probep = mProbes[i].get();
		if (probep)	// Paranoia ?
		{
			probep->mProbeIndex = -1;
		}
	}

	if (count > 1)
	{
		std::sort(maps.begin(), maps.begin() + count, CompareProbeDepth());
	}

	for (U32 i = 0; i < count; ++i)
	{
		maps[i]->mProbeIndex = i;
	}

	// NULL-terminate list
	if (count < maps_size)
	{
		maps[count] = NULL;
	}
}

LLReflectionMap* LLReflectionMapManager::registerSpatialGroup(LLSpatialGroup* groupp)
{
	if (groupp &&
		groupp->getSpatialPartition()->mPartitionType ==
			LLViewerRegion::PARTITION_VOLUME)
	{
		OctreeNode* nodep = groupp->getOctreeNode();
		F32 size = nodep->getSize().getF32ptr()[0];
		if (size >= 15.f && size <= 17.f)
		{
			return addProbe(groupp);
		}
	}
	return NULL;
}

LLReflectionMap* LLReflectionMapManager::registerViewerObject(LLViewerObject* vobjp)
{
	llassert(vobjp != NULL);

	LLReflectionMap* probep = new LLReflectionMap();
	probep->mViewerObject = vobjp;
	probep->mOrigin.load3(vobjp->getPositionAgent().mV);

	if (gCubeSnapshot)
	{
		// Snapshot is in progress, mProbes is being iterated over, defer
		// insertion until next update
		mCreateList.emplace_back(probep);
	}
	else
	{
		mProbes.emplace_back(probep);
	}

	return probep;
}

S32 LLReflectionMapManager::allocateCubeIndex()
{
	if (mCubeFree.empty())
	{
		return -1;
	}
	S32 ret = mCubeFree.front();
	mCubeFree.pop_front();
	return ret;
}

void LLReflectionMapManager::deleteProbe(U32 i)
{
	LLReflectionMap* probep = mProbes[i].get();
	if (probep == mDefaultProbe.get())
	{
		llwarns << "Attempt to remove the default probe. Aborted." << llendl;
		return;
	}

	if (probep->mCubeIndex != -1)
	{
		// Mark the cube index used by this probe as being free
		mCubeFree.push_back(probep->mCubeIndex);
	}
	if (mUpdatingProbe == probep)
	{
		mUpdatingProbe = NULL;
		mUpdatingFace = 0;
	}

	// Remove from any neighbors lists
	for (auto& otherp : probep->mNeighbors)
	{
		LLReflectionMap::reflmap_vec_t::iterator ne = otherp->mNeighbors.end();
		LLReflectionMap::reflmap_vec_t::iterator it =
			std::find(otherp->mNeighbors.begin(), ne, probep);
		if (it != ne)
		{
			otherp->mNeighbors.erase(it);
		}
	}

	mProbes.erase(mProbes.begin() + i);
}

void LLReflectionMapManager::doProbeUpdate()
{
	if (!gUsePBRShaders)
	{
		return;
	}

	llassert(mUpdatingProbe != NULL);

	updateProbeFace(mUpdatingProbe, mUpdatingFace);

	if (++mUpdatingFace == 6)
	{
		updateNeighbors(mUpdatingProbe);
		mUpdatingFace = 0;
		if (isRadiancePass())
		{
			mUpdatingProbe->mComplete = true;
			mUpdatingProbe = NULL;
			mRadiancePass = false;
		}
		else
		{
			mRadiancePass = true;
		}
	}
}

// Do the reflection map update render passes. For every 12 calls to this
// method, one complete reflection probe radiance map and irradiance map is
// generated. First six passes render the scene with direct lighting only into
// a scratch space cube map at the end of the cube map array and generate a
// simple mip chain (not convolution filter). At the end of these passes, an
// irradiance map is generated for this probe and placed into the irradiance
// cube map array at the index for this probe. The next six passes render the
// scene with both radiance and irradiance into the same scratch space cube map
// and generate a simple mip chain. At the end of these passes, a radiance map
// is generated for this probe and placed into the radiance cube map array at
// the index for this probe. In effect this simulates single-bounce lighting.
void LLReflectionMapManager::updateProbeFace(LLReflectionMap* probep, U32 face)
{
	if (!gUsePBRShaders)
	{
		return;
	}

	mLightScale = 1.f;
	static LLCachedControl<F32> max_amb(gSavedSettings,
										"RenderReflectionProbeMaxAmbiance");
	if (!isRadiancePass() && probep->getAmbiance() > (F32)max_amb)
	{
		mLightScale = max_amb / probep->getAmbiance();
	}

	// Hacky hot-swap of camera specific render targets
	gPipeline.mRT = &gPipeline.mAuxillaryRT;

	if (probep == mDefaultProbe.get())
	{
		touch_default_probe(probep);

		gPipeline.pushRenderTypeMask();

		// Only render sky, water, terrain, and clouds
		gPipeline.andRenderTypeMask(LLPipeline::RENDER_TYPE_SKY,
									LLPipeline::RENDER_TYPE_WL_SKY,
									LLPipeline::RENDER_TYPE_WATER,
									LLPipeline::RENDER_TYPE_VOIDWATER,
									LLPipeline::RENDER_TYPE_CLOUDS,
									LLPipeline::RENDER_TYPE_TERRAIN,
									LLPipeline::END_RENDER_TYPES);

		probep->update(mRenderTarget.getWidth(), face);

		gPipeline.popRenderTypeMask();
	}
	else
	{
		probep->update(mRenderTarget.getWidth(), face);
	}

	gPipeline.mRT = &gPipeline.mMainRT;

	S32 source_idx = mReflectionProbeCount;
	if (probep != mUpdatingProbe)
	{
		// This is the "realtime" probe that is updating every frame, use the
		// secondary scratch space channel
		++source_idx;
	}

	gGL.setColorMask(true, true);
	LLGLDepthTest depth(GL_FALSE, GL_FALSE);
	LLGLDisable cull(GL_CULL_FACE);
	LLGLDisable blend(GL_BLEND);

	// Downsample to placeholder map

	gGL.matrixMode(gGL.MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	gGL.matrixMode(gGL.MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();

	gGL.flush();
	U32 res = mProbeResolution * 2;

	LLRenderTarget* screen_rt = &gPipeline.mAuxillaryRT.mScreen;

	// Perform a gaussian blur on the super sampled render before downsampling

	gGaussianProgram.bind();
	const F32 res_scale = 1.f / F32(mProbeResolution * 2);
	gGaussianProgram.uniform1f(sResScale, res_scale);
	S32 chan = gGaussianProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
											  LLTexUnit::TT_TEXTURE);

	// Horizontal
	gGaussianProgram.uniform2f(sDirection, 1.f, 0.f);
	gGL.getTexUnit(chan)->bind(screen_rt);
	mRenderTarget.bindTarget();
	gPipeline.mScreenTriangleVB->setBuffer();
	gPipeline.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	mRenderTarget.flush();

	// Vertical
	gGaussianProgram.uniform2f(sDirection, 0.f, 1.f);
	gGL.getTexUnit(chan)->bind(&mRenderTarget);
	screen_rt->bindTarget();
	gPipeline.mScreenTriangleVB->setBuffer();
	gPipeline.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
	screen_rt->flush();

	S32 mips = S32(log2f((F32)mProbeResolution) + 0.5f);

	gReflectionMipProgram.bind();
	chan = gReflectionMipProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE,
											   LLTexUnit::TT_TEXTURE);

	for (U32 i = 0, count = mMipChain.size(); i < count; ++i)
	{
		LLRenderTarget& target = mMipChain[i];
		target.bindTarget();
		if (i == 0)
		{
			gGL.getTexUnit(chan)->bind(screen_rt);
		}
		else
		{
			gGL.getTexUnit(chan)->bind(&(mMipChain[i - 1]));
		}

		gReflectionMipProgram.uniform1f(sResScale, res_scale);

		gPipeline.mScreenTriangleVB->setBuffer();
		gPipeline.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

		res /= 2;

		S32 mip = i + mips - count;

		if (mip >= 0)
		{
			mTexture->bind(0);
			glCopyTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, mip, 0, 0,
								source_idx * 6 + face, 0, 0, res, res);
			mTexture->unbind();
		}
		target.flush();
	}

	gGL.popMatrix();
	gGL.matrixMode(gGL.MM_MODELVIEW);
	gGL.popMatrix();

	gGL.getTexUnit(chan)->unbind(LLTexUnit::TT_TEXTURE);
	gReflectionMipProgram.unbind();

	if (face != 5)
	{
		return;	// We are done.
	}

	if (mMipChain.empty())	// Paranoia ?
	{
		llwarns_once << "mMipChain is empty !" << llendl;
		return;
	}

	if (!LLViewerShaderMgr::sHasIrrandiance)
	{
		// Cannot render this since the two gIrradianceGenProgram and
		// gRadianceGenProgram shaders have not loaded... HB
		return;
	}

	mMipChain[0].bindTarget();

	LLGLSLShader* shaderp;
	if (isRadiancePass())
	{
		// Generate radiance map (even if this is not the irradiance map, we
		// need the mip chain for the irradiance map).
		shaderp = &gRadianceGenProgram;
		shaderp->bind();

		mVertexBuffer->setBuffer();

		chan = shaderp->enableTexture(LLShaderMgr::REFLECTION_PROBES,
									  LLTexUnit::TT_CUBE_MAP_ARRAY);
		mTexture->bind(chan);
		shaderp->uniform1i(sSourceIdx, source_idx);
		shaderp->uniform1f(LLShaderMgr::REFLECTION_PROBE_MAX_LOD,
						   mMaxProbeLOD);

		U32 res = mMipChain[0].getWidth();

		LLCoordFrame frame;
		F32 mat[16];
		for (U32 i = 0, count = mMipChain.size(); i < count; ++i)
		{
			shaderp->uniform1f(sRoughness, F32(i) / F32(count - 1));
			shaderp->uniform1f(sMipLevel, i);
			shaderp->uniform1i(sWidth, mProbeResolution);

			for (U32 cf = 0; cf < 6; ++cf)	// For each cube face
			{
				frame.lookAt(LLVector3::zero,
							 LLCubeMapArray::sClipToCubeLookVecs[cf],
							 LLCubeMapArray::sClipToCubeUpVecs[cf]);

				frame.getOpenGLRotation(mat);
				gGL.loadMatrix(mat);

				mVertexBuffer->drawArrays(gGL.TRIANGLE_STRIP, 0, 4);

				glCopyTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, i, 0, 0,
									probep->mCubeIndex * 6 + cf, 0, 0,
									res, res);
			}

			if (i != count - 1)
			{
				res /= 2;
				glViewport(0, 0, res, res);
			}
		}
	}
	else
	{
		// Generate irradiance map
		shaderp = &gIrradianceGenProgram;
		shaderp->bind();
		chan = shaderp->enableTexture(LLShaderMgr::REFLECTION_PROBES,
									  LLTexUnit::TT_CUBE_MAP_ARRAY);
		mTexture->bind(chan);

		shaderp->uniform1i(sSourceIdx, source_idx);
		shaderp->uniform1f(LLShaderMgr::REFLECTION_PROBE_MAX_LOD,
						   mMaxProbeLOD);

		mVertexBuffer->setBuffer();

		// Find the mip target to start with based on irradiance map resolution
		U32 start_mip = 0;
		U32 count = mMipChain.size();
		while (start_mip < count &&
			   mMipChain[start_mip].getWidth() != LL_IRRADIANCE_MAP_RESOLUTION)
		{
			++start_mip;
		}

		if (start_mip < count)
		{
			LLRenderTarget& target = mMipChain[start_mip];
			glViewport(0, 0, target.getWidth(), target.getHeight());

			F32 mat[16];
			for (U32 cf = 0; cf < 6; ++cf)	// For each cube face
			{
				LLCoordFrame frame;
				frame.lookAt(LLVector3::zero,
							 LLCubeMapArray::sClipToCubeLookVecs[cf],
							 LLCubeMapArray::sClipToCubeUpVecs[cf]);

				frame.getOpenGLRotation(mat);
				gGL.loadMatrix(mat);

				mVertexBuffer->drawArrays(gGL.TRIANGLE_STRIP, 0, 4);

				S32 res = target.getWidth();
				mIrradianceMaps->bind(chan);
				glCopyTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0,
									probep->mCubeIndex * 6 + cf, 0, 0,
									res, res);
				mTexture->bind(chan);
			}
		}
	}

	mMipChain[0].flush();

	shaderp->unbind();
}

void LLReflectionMapManager::shift(const LLVector4a& offset)
{
	for (U32 i = 0, count = mProbes.size(); i < count; ++i)
	{
		LLReflectionMap* probep = mProbes[i].get();
		if (probep)	// Paranoia
		{
			probep->mOrigin.add(offset);
		}
	}
}

void LLReflectionMapManager::updateNeighbors(LLReflectionMap* probep)
{
	if (mDefaultProbe.get() == probep)
	{
		return;
	}

	// Remove from existing neighbors
	for (auto& otherp : probep->mNeighbors)
	{
		LLReflectionMap::reflmap_vec_t::iterator ne = otherp->mNeighbors.end();
		LLReflectionMap::reflmap_vec_t::iterator it =
			std::find(otherp->mNeighbors.begin(), ne, probep);
		if (it != ne)
		{
			otherp->mNeighbors.erase(it);
		}
	}
	probep->mNeighbors.clear();

	// Search for new neighbors
	if (probep->isRelevant())
	{
		for (U32 i = 0, count = mProbes.size(); i < count; ++i)
		{
			LLReflectionMap* otherp = mProbes[i].get();
			if (otherp != mDefaultProbe.get() && otherp != probep)
			{
				if (otherp->isRelevant() && probep->intersects(otherp))
				{
					probep->mNeighbors.push_back(otherp);
					otherp->mNeighbors.push_back(probep);
				}
			}
		}
	}
}

// Structure for packing uniform buffer object.
// See class3/deferred/reflectionProbeF.glsl
struct ReflectionProbeData
{
	// For box probes, matrix that transforms from camera space to a [-1, 1]
	// cube representing the bounding box of the box probe
	LLMatrix4 refBox[LL_MAX_REFLECTION_PROBE_COUNT];

	// For sphere probes, origin (xyz) and radius (w) of refmaps in clip space
	LLVector4 refSphere[LL_MAX_REFLECTION_PROBE_COUNT];

	// Extra parameters
	//  x - irradiance scale
	//  y - radiance scale
	//  z - fade in
	//  w - znear
	LLVector4 refParams[LL_MAX_REFLECTION_PROBE_COUNT];

	// Indices used by probe:
	//  [i][0] - cubemap array index for this probe
	//  [i][1] - index into "refNeighbor" for probes that intersect this probe
	//  [i][2] - number of probes  that intersect this probe, or -1 for no
	//			 neighbors
	//  [i][3] - priority (probe type stored in sign bit - positive for
	//			 spheres, negative for boxes)
	GLint refIndex[LL_MAX_REFLECTION_PROBE_COUNT][4];

	// List of neighbor indices
	GLint refNeighbor[4096];

	// Lookup table for which index to start with for the given Z depth
	GLint refBucket[256][4];
	// Numbrer of active refmaps
	GLint refmapCount;
};

void LLReflectionMapManager::updateUniforms()
{
	if (!LLPipeline::sReflectionProbesEnabled)
	{
		return;
	}

	mReflectionMaps.resize(mReflectionProbeCount);
	getReflectionMaps(mReflectionMaps);

	ReflectionProbeData rpd;

	static F32 min_depth[256];

	for (U32 i = 0; i < 256; ++i)
	{
		rpd.refBucket[i][0] = rpd.refBucket[i][1] = rpd.refBucket[i][2] =
							  rpd.refBucket[i][3] = mReflectionProbeCount;
		min_depth[i] = FLT_MAX;
	}

	LLMatrix4a modelview = gGLModelView;
	LLVector4a oa; // Scratch space for transformed origin

	S32 count = 0;
	// Neighbor "cursor": index into refNeighbor to start writing the next
	// probe's list of neighbors
	U32 nc = 0;

	static LLCachedControl<bool> auto_adjust(gSavedSettings,
											 "RenderSkyAutoAdjustLegacy");
	LLSettingsSky::ptr_t skyp = gEnvironment.getCurrentSky();
	F32 min_ambiance = skyp->getReflectionProbeAmbiance(auto_adjust);

	F32 ambscale, radscale;
	if (gCubeSnapshot && !isRadiancePass())	// Ambiance pass ?
	{
		ambscale = 0.f;
		radscale = 0.5f;
	}
	else
	{
		ambscale = radscale = 1.f;
	}

	for (U32 k = 0, nmaps = mReflectionMaps.size(); k < nmaps; ++k)
	{
		LLReflectionMap* refmapp = mReflectionMaps[k];
		if (!refmapp)
		{
			break;
		}

		if (refmapp != mDefaultProbe.get())
		{
			// Bucket search data. Theory of operation:
			// 1. Determine minimum and maximum depth of each influence volume
			//	  and store in mDepth (done in getReflectionMaps).
			// 2. Sort by minimum depth.
			// 3. Prepare a bucket for each 1m of depth out to 256m.
			// 4. For each bucket, store the index of the nearest probe that
			//    might influence pixels in that bucket.
			// 5. In the shader, lookup the bucket for the pixel depth to get
			//    the index of the first probe that could possibly influence
			//    the current pixel.
			U32 depth_min = U32(llclamp(S32(refmapp->mMinDepth), 0, 255));
			U32 depth_max = U32(llclamp(S32(refmapp->mMaxDepth), 0, 255));
			for (U32 i = depth_min; i <= depth_max; ++i)
			{
				if (refmapp->mMinDepth < min_depth[i])
				{
					min_depth[i] = refmapp->mMinDepth;
					rpd.refBucket[i][0] = refmapp->mProbeIndex;
				}
			}
		}

		llassert(refmapp->mProbeIndex == count && refmapp->mCubeIndex >= 0 &&
				 mReflectionMaps[refmapp->mProbeIndex] == refmapp);
		LLViewerObject* objp = refmapp->mViewerObject;
		if (objp && objp->getVolume())
		{
			// Have active manual probes live-track the object they are
			// associated with
			refmapp->mOrigin.load3(objp->getPositionAgent().mV);
			LLVOVolume* vobjp = objp->asVolume();
			if (vobjp && vobjp->getReflectionProbeIsBox())
			{
				static const LLVector3 half(0.5f, 0.5f, 0.5f);
				refmapp->mRadius = vobjp->getScale().scaledVec(half).length();
			}
			else
			{
				refmapp->mRadius = objp->getScale().mV[0] * 0.5f;
			}
		}
		modelview.affineTransform(refmapp->mOrigin, oa);
		rpd.refSphere[count].set(oa.getF32ptr());
		rpd.refSphere[count].mV[3] = refmapp->mRadius;

		rpd.refIndex[count][0] = refmapp->mCubeIndex;
		llassert(nc % 4 == 0);
		rpd.refIndex[count][1] = nc / 4;
		rpd.refIndex[count][3] = refmapp->mPriority;

		// For objects that are reflection probes, use the volume as the
		// influence volume of the probe only possibile influence volumes are
		// boxes and spheres, so detect boxes and treat everything else as
		// spheres
		if (refmapp->getBox(rpd.refBox[count]))
		{
			// Negate priority to indicate this probe has a box influence
			// volume
			rpd.refIndex[count][3] *= -1;
		}

		rpd.refParams[count].set(llmax(min_ambiance,
									   refmapp->getAmbiance()) * ambscale,
								 radscale,			// Radiance scale
								 refmapp->mFadeIn,	// Fade-in weight
								 // Z near
								 oa.getF32ptr()[2] - refmapp->mRadius);

		// Neighbor ("index"): index into refNeighbor to write indices for
		// current reflection probe's neighbors
		U32 ni = nc;
		// Pack neghbor list
		constexpr U32 MAX_NEIGHBORS = 64;
		U32 neighbor_count = 0;
		for (U32 n = 0, ncount = refmapp->mNeighbors.size();
			 n < ncount && ni < 4096 && neighbor_count < MAX_NEIGHBORS; ++n)
		{
			LLReflectionMap* neighborp = refmapp->mNeighbors[n];
			GLint idx = neighborp->mProbeIndex;
			if (idx != -1 && !neighborp->mOccluded &&
				neighborp->mCubeIndex != -1)
			{
				// This neighbor may be sampled
				rpd.refNeighbor[ni++] = idx;
				++neighbor_count;
			}
		}

		if (nc == ni)
		{
			// No neighbors, tag as empty
			rpd.refIndex[count][1] = -1;
		}
		else
		{
			rpd.refIndex[count][2] = ni - nc;

			// Move the cursor forward
			nc = ni;
			if (nc % 4 != 0)
			{
				// Jump to next power of 4 for compatibility with ivec4
				nc += 4 - (nc % 4);
			}
		}

		++count;
	}

	rpd.refmapCount = count;

	// Copy rpd into uniform buffer object
	if (mUBO == 0)
	{
		glGenBuffers(1, &mUBO);
	}

	glBindBuffer(GL_UNIFORM_BUFFER, mUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(ReflectionProbeData), &rpd,
				 GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void LLReflectionMapManager::setUniforms()
{
	if (LLPipeline::sReflectionProbesEnabled)
	{
		if (mUBO == 0)
		{
			updateUniforms();
		}
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, mUBO);
	}
}

void renderReflectionProbe(LLReflectionMap* probep)
{
	if (!probep || !probep->isRelevant())
	{
		return;
	}

	F32* po = probep->mOrigin.getF32ptr();

	// Draw orange line from probe to neighbors
	gGL.flush();
	gGL.diffuseColor4f(1.f, 0.5f, 0.f, 1.f);
	gGL.begin(gGL.LINES);
	for (U32 i = 0, count = probep->mNeighbors.size(); i < count; ++i)
	{
		LLReflectionMap* neighborp = probep->mNeighbors[i];
		if (!neighborp) continue;	// Paranoia ?

		if (!probep->mViewerObject || !neighborp->mViewerObject)
		{
			gGL.vertex3fv(po);
			gGL.vertex3fv(neighborp->mOrigin.getF32ptr());
		}
	}
	gGL.end(true);

	gGL.diffuseColor4f(1.f, 1.f, 0.f, 1.f);
	gGL.begin(gGL.LINES);
	for (U32 i = 0, count = probep->mNeighbors.size(); i < count; ++i)
	{
		LLReflectionMap* neighborp = probep->mNeighbors[i];
		if (!neighborp) continue;	// Paranoia ?

		if (probep->mViewerObject && neighborp->mViewerObject)
		{
			gGL.vertex3fv(po);
			gGL.vertex3fv(neighborp->mOrigin.getF32ptr());
		}
	}
	gGL.end(true);
}

void LLReflectionMapManager::renderDebug()
{
	gDebugProgram.bind();
	for (U32 i = 0, count = mProbes.size(); i < count; ++i)
	{
		renderReflectionProbe(mProbes[i].get());
	}
	gDebugProgram.unbind();
}

void LLReflectionMapManager::initReflectionMaps()
{
	if (!gUsePBRShaders)
	{
		return;
	}

	if (mReset || mTexture.isNull() ||
		mReflectionProbeCount != LL_MAX_REFLECTION_PROBE_COUNT)
	{
		mReset = false;
		static LLCachedControl<U32> res(gSavedSettings,
										"RenderReflectionProbeResolution");
		mProbeResolution = nhpo2(llclamp((U32)res, 64, 512));
		mReflectionProbeCount = LL_MAX_REFLECTION_PROBE_COUNT;
		mMaxProbeLOD = log2f(mProbeResolution) - 1.f; // Number of mips - 1

		if (mTexture.isNull() ||
			mTexture->getResolution() != mProbeResolution ||
			mTexture->getCount() != mReflectionProbeCount + 2)
		{
			mTexture = new LLCubeMapArray();
			// Store mReflectionProbeCount + 2 cube maps, final two cube maps
			// are used for render target and radiance map generation source).
			mTexture->allocate(mProbeResolution, 3, mReflectionProbeCount + 2);

			mIrradianceMaps = new LLCubeMapArray();
			mIrradianceMaps->allocate(LL_IRRADIANCE_MAP_RESOLUTION, 3,
									  mReflectionProbeCount, false);
		}

		// Reset probe state
		mUpdatingFace = 0;
		mUpdatingProbe = NULL;
		mRadiancePass = mRealtimeRadiancePass = false;

		// If default probe already exists, remember whether or not it is
		// complete (SL-20498)
		bool default_complete = mDefaultProbe.notNull() &&
								mDefaultProbe->mComplete;
		for (U32 i = 0, count = mProbes.size(); i < count; ++i)
		{
			LLReflectionMap* probep = mProbes[i].get();
			if (probep)	// Paranoia
			{
				probep->mLastUpdateTime = 0.f;
				probep->mComplete = false;
				probep->mProbeIndex = -1;
				probep->mCubeArray = NULL;
				probep->mCubeIndex = -1;
				probep->mNeighbors.clear();
			}
		}

		mCubeFree.clear();
		initCubeFree();

		if (mDefaultProbe.isNull())
		{
			// The default probe MUST be the first probe created
			llassert(mProbes.empty());
			mDefaultProbe = new LLReflectionMap();
			mProbes.push_back(mDefaultProbe);
		}

		llassert(mProbes[0] == mDefaultProbe);

		mDefaultProbe->mCubeIndex = 0;
		mDefaultProbe->mCubeArray = mTexture;
		mDefaultProbe->mDistance = 64.f;
		mDefaultProbe->mRadius = 4096.f;
		mDefaultProbe->mProbeIndex = 0;
		mDefaultProbe->mComplete = default_complete;
		touch_default_probe(mDefaultProbe);

		if (mProbeResolution != mOldProbeResolution)
		{
			mOldProbeResolution = mProbeResolution;
			mRenderTarget.release();
			mMipChain.clear();
		}
	}

	if (!mRenderTarget.isComplete())
	{
		U32 tgt_res = mProbeResolution * 4; // Super sample
		mRenderTarget.allocate(tgt_res, tgt_res, GL_RGB16F, true);
	}

	if (mMipChain.empty())
	{
		U32 res = mProbeResolution;
		U32 count = U32(log2f(F32(res)) + 0.5f);

		mMipChain.resize(count);
		for (U32 i = 0; i < count; ++i)
		{
			mMipChain[i].allocate(res, res, GL_RGB16F);
			res /= 2;
		}
	}

	if (mVertexBuffer.isNull())
	{
		constexpr U32 mask = LLVertexBuffer::MAP_VERTEX;
		mVertexBuffer = new LLVertexBuffer(mask);
		mVertexBuffer->allocateBuffer(4, 0);

		LLStrider<LLVector3> v;

		mVertexBuffer->getVertexStrider(v);

		v[0] = LLVector3(-1.f, -1.f, -1.f);
		v[1] = LLVector3(1.f, -1.f, -1.f);
		v[2] = LLVector3(-1.f, 1.f, -1.f);
		v[3] = LLVector3(1.f, 1.f, -1.f);

		mVertexBuffer->unmapBuffer();
	}
}

void LLReflectionMapManager::cleanup()
{
	mVertexBuffer = NULL;
	mRenderTarget.release();

	mMipChain.clear();

	mTexture = NULL;
	mIrradianceMaps = NULL;

	mProbes.clear();
	mKillList.clear();
	mCreateList.clear();

	mReflectionMaps.clear();
	mUpdatingFace = 0;

	mDefaultProbe = NULL;
	mUpdatingProbe = NULL;

	glDeleteBuffers(1, &mUBO);
	mUBO = 0;

	// Note: also called on teleport (not just shutdown), so make sure we are
	// in a good "starting" state.
	initCubeFree();
}

void LLReflectionMapManager::doOcclusion()
{
	if (!gUsePBRShaders)
	{
		return;
	}

	LLVector4a eye;
	eye.load3(gViewerCamera.getOrigin().mV);

	for (U32 i = 0, count = mProbes.size(); i < count; ++i)
	{
		LLReflectionMap* probep = mProbes[i].get();
		if (probep && probep != mDefaultProbe.get())
		{
			probep->doOcclusion(eye);
		}
	}
}
