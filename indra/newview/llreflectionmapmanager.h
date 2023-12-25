/**
 * @file llreflectionmapmanager.h
 * @brief LLReflectionMap and LLReflectionMapManager classes declaration
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

#ifndef LL_LLREFLECTIONMAPMANAGER_H
#define LL_LLREFLECTIONMAPMANAGER_H

#include <list>
#include <vector>

#include "llcubemap.h"
#include "llcubemaparray.h"
#include "llrendertarget.h"

class LLSpatialGroup;
class LLViewerObject;

// Number of reflection probes to keep in VRAM
#define LL_MAX_REFLECTION_PROBE_COUNT 256

// Reflection probe resolution
#define LL_IRRADIANCE_MAP_RESOLUTION 64

// Reflection probe mininum scale
#define LL_REFLECTION_PROBE_MINIMUM_SCALE 1.f;

///////////////////////////////////////////////////////////////////////////////
// LLReflectionMap class
// This used to be in its own llreflectionmap.h/cpp module, but the header was
// only included by llreflectionmapmanager.h, so... HB
///////////////////////////////////////////////////////////////////////////////

class alignas(16) LLReflectionMap : public LLRefCount
{
protected:
	LOG_CLASS(LLReflectionMap);

public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

    enum DetailLevel : U32
    {
        STATIC_ONLY = 0,
        STATIC_AND_DYNAMIC = 1,
        REALTIME = 2
    };

	typedef std::vector<LLReflectionMap*> reflmap_vec_t;

	// Allocates an environment map of the given resolution
	LLReflectionMap();
	~LLReflectionMap();

	// Updates this environment map resolution
	void update(U32 resolution, U32 face);

	// For volume partition probes, tries to place this probe in the best spot
	void autoAdjustOrigin();

	// Returns true if given reflection map's influence volume intersects with
	// this one's.
	bool intersects(LLReflectionMap* otherp);

	// Gets the ambiance value to use for this probe
	F32 getAmbiance();

	// Gets the near clip plane distance to use for this probe
	F32 getNearClip();

	// Returns true if this probe should include avatars in its reflection map
	bool getIsDynamic();

	// Gets the encoded bounding box of this probe's influence volume; will
	// only return a box if this probe is associated with a LLVOVolume with its
	// reflection probe influence volume to to VOLUME_TYPE_BOX. Returns false
	// if no bounding box (treat as sphere influence volume).
	bool getBox(LLMatrix4& box);

	// Returns true if this probe is active for rendering
	LL_INLINE bool isActive()					{ return mCubeIndex != -1; }

	// Performs occlusion query/readback
	void doOcclusion(const LLVector4a& eye);

	// Returns false if this probe is not currently relevant (for example,
	// disabled due to graphics preferences).
	bool isRelevant();

public:
	// Index into array packed by LLReflectionMapManager::getReflectionMaps()
	// WARNING: only valid immediately after call to getReflectionMaps().
	// Note
	S32							mProbeIndex;

	// Spatial group this probe is tracking (if any).
	LLSpatialGroup*				mGroup;

	// Point at which environment map was last generated from (in agent space).
	// Note: placed 16 bytes (counting the LLRefCount S32) from variable
	// members start, to avoid padding issues. HB
	LLVector4a					mOrigin;

	// Viewer object this probe is tracking (if any).
	LLViewerObject*				mViewerObject;

	// Set of any LLReflectionMaps that intersect this map (maintained by
	// LLReflectionMapManager).
	reflmap_vec_t				mNeighbors;

	// Cube map used to sample this environment map.
	LLPointer<LLCubeMapArray>	mCubeArray;
	// Index into cube map array or -1 if not currently stored in a cube map.
	S32 mCubeIndex = -1;

	// Distance from main viewer camera.
	F32							mDistance;

	// Minimum and maximum depth in current render camera.
	F32							mMinDepth;
	F32							mMaxDepth;

	// Radius of this probe's affected area
	F32							mRadius;

	// Last time this probe was updated (or when its update timer got reset).
	F32							mLastUpdateTime;
	// Last time this probe was bound for rendering.
	F32							mLastBindTime;

	// fade in parameter for this probe
	F32							mFadeIn;

	// What priority should this probe have (higher is higher priority)
	// currently only 0 or 1
	// 0 - automatic probe
	// 1 - manual probe
	U32							mPriority;

	// Occlusion culling state
	U32							mOcclusionQuery;
	U32							mOcclusionPendingFrames;
	bool						mOccluded;

	// true when probe has had at least one full update and is ready to render.
	bool						mComplete;
};

///////////////////////////////////////////////////////////////////////////////
// LLReflectionMapManager class proper
///////////////////////////////////////////////////////////////////////////////

class alignas(16) LLReflectionMapManager
{
	friend class LLPipeline;

protected:
	LOG_CLASS(LLReflectionMapManager);

public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	// Allocates an environment map of the given resolution
	LLReflectionMapManager();

	// Releases any GL state
	void cleanup();

	// Maintains reflection probes
	void update();

	// Adds a probe for the given spatial group.
	LLReflectionMap* addProbe(LLSpatialGroup* groupp = NULL);

	// Populate "maps" with the N most relevant reflection maps where N is no
	// more than maps.size(). If less than maps.size() reflection maps are
	// available, will assign trailing elements to NULL. 'maps' must be an
	// adequately sized array of reflection map pointers.
	void getReflectionMaps(std::vector<LLReflectionMap*>& maps);

	// Called by LLSpatialGroup constructor. If spatial group should receive a
	// reflection probe, creates one for the specified spatial group.
	LLReflectionMap* registerSpatialGroup(LLSpatialGroup* groupp);

	// Presently hacked into LLViewerObject::setTE(). Used by LLViewerObjects
	// which are reflection probes. 'vobjp' must not be NULL. Guaranteed to not
	// return NULL.
	LLReflectionMap* registerViewerObject(LLViewerObject* vobjp);

	// Resets all state on the next update
	LL_INLINE void reset()						{ mReset = true; }

	// Pauses/resumes all updates other than the default probe
	LL_INLINE void pause(bool b)				{ mPaused = b; }
	LL_INLINE bool paused() const				{ return mPaused; }

	// Called on region crossing to "shift" probes into new coordinate frame.
	void shift(const LLVector4a& offset);

	// Called from LLSpatialPartition when reflection probe debug display is
	// active.
	void renderDebug();

	// Called once at startup to allocate cubemap arrays
	void initReflectionMaps();

	// Returns true if currently updating a radiance map, false if currently
	// updating an irradiance map.
	LL_INLINE bool isRadiancePass()				{ return mRadiancePass; }

	// Performs occlusion culling on all active reflection probes
	void doOcclusion();

	typedef std::vector<LLPointer<LLReflectionMap> > prmap_vec_t;

private:
	// Initializes mCubeFree array to default values
	void initCubeFree();

	// Deletes the probe with the given index in mProbes
	void deleteProbe(U32 i);

	// Gets a free cube index
	// returns -1 if allocation failed
	S32 allocateCubeIndex();

	// Updates the neighbors of the given probe
	void updateNeighbors(LLReflectionMap* probep);

	// Updates UBO used for rendering (call only once per render pipe flush)
	void updateUniforms();

	// Binds UBO used for rendering
	void setUniforms();

	// Performs an update on the currently updating probe
	void doProbeUpdate();

	// Updates the specified face of the specified probe
	void updateProbeFace(LLReflectionMap* probep, U32 face);

private:
	// Render target for cube snapshots; used to generate mipmaps without
	// doing a copy-to-texture.
	LLRenderTarget				mRenderTarget;

	std::vector<LLRenderTarget>	mMipChain;

	// List of free cubemap indices
	std::list<S32>				mCubeFree;

	// Storage for reflection probe radiance maps (plus two scratch space
	// cubemaps)
	LLPointer<LLCubeMapArray>	mTexture;

	// Vertex buffer for pushing verts to filter shaders
	LLPointer<LLVertexBuffer>	mVertexBuffer;

	// Storage for reflection probe irradiance maps
	LLPointer<LLCubeMapArray>	mIrradianceMaps;

	// Default reflection probe to fall back to for pixels with no probe
	// influences (should always be at cube index 0).
	LLPointer<LLReflectionMap>	mDefaultProbe;

	LLReflectionMap*			mUpdatingProbe;

	// List of maps being used for rendering
	typedef std::vector<LLReflectionMap*> rmap_vec_t;
	rmap_vec_t					mReflectionMaps;

	// List of active reflection maps
	prmap_vec_t					mProbes;
	// List of reflection maps to kill
	prmap_vec_t					mKillList;
	// List of reflection maps to create
	prmap_vec_t					mCreateList;

	// Handle to UBO
	U32							mUBO;

	U32							mUpdatingFace;

	// Number of reflection probes to use for rendering.
	U32							mReflectionProbeCount;

	// Resolution of reflection probes
	U32							mProbeResolution;
	// Previous resolution of reflection probes (used to detect resolution
	// changes and reset render target and mip chains when they happen). HB
	U32							mOldProbeResolution;

	// Maximum LoD of reflection probes (mip levels - 1)
	F32							mMaxProbeLOD;

	// amount to scale local lights during an irradiance map update (set during
	// updateProbeFace() and used by LLPipeline).
	F32							mLightScale;

	// If true, we are generating the radiance map for the current probe,
	// otherwise we are generating the irradiance map. Update sequence should
	// be to generate the irradiance map from render of the world that has no
	// irradiance, then generate the radiance map from a render of the world
	// that includes irradiance. This should avoid feedback loops and ensure
	// that the colors in the radiance maps match the colors in the
	// environment.
	bool						mRadiancePass;

	// Same as above, but for the realtime probe. Realtime probes should update
	// all six sides of the irradiance map on "odd" frames and all six sides of
	// the radiance map on "even" frames.
	bool						mRealtimeRadiancePass;

	// If true, reset all probe render state on the next update (for teleports
	// and sky changes).
	bool						mReset;

	// If true, only update the default probe
	bool						mPaused;
};

#endif	// LL_LLREFLECTIONMAPMANAGER_H
