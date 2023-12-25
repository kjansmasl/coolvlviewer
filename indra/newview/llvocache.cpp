/**
 * @file llvocache.cpp
 * @brief Cache of objects on the viewer.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2016 (original implementation), Linden Research, Inc.
 * Copyright (c) 2022 (PBR extra data implementation), Linden Research, Inc.
 * Copyright (c) 2016-2023, Henri Beauchamp (debugging, cache parameters
 * biasing based on memory usage, removal of APR dependency, threading).
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

#include "llvocache.h"

#include "lldir.h"
#include "lldiriterator.h"
#include "llfasttimer.h"
#include "llregionhandle.h"
#include "llsdserialize.h"
#include "llsdutil.h"			// For ll_pretty_print_sd()
#include "hbtracy.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawable.h"
#include "llgridmanager.h"
#include "llpipeline.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexture.h"	// For sDesiredDiscardBias

// Version of our object cache: increment each time its structure changes.
// Note: we use an unusually large number, which should ensure that no cache
// written by another viewer than the Cool VL Viewer would be considered valid
// (even though the cache directory is normally already different).
constexpr U32 OBJECT_CACHE_VERSION = 10002U;
constexpr U32 ADDRESS_SIZE = 64U;

#if HB_AJUSTED_VOCACHE_PARAMETERS
// This is a target FPS rate that is used as a scaler but that is normalized
// with the actual frame rate (1.f / gFrameIntervalSeconds).
constexpr S32 TARGET_FPS = 30;
#endif

// Static variables
U32 LLVOCacheEntry::sMinFrameRange = 0;
F32 LLVOCacheEntry::sNearRadius = 1.f;
F32 LLVOCacheEntry::sRearFarRadius = 1.f;
F32 LLVOCacheEntry::sFrontPixelThreshold = 1.f;
F32 LLVOCacheEntry::sRearPixelThreshold = 1.f;
#if HB_AJUSTED_VOCACHE_PARAMETERS
bool LLVOCacheEntry::sBiasedRetention = false;
#endif
bool LLVOCachePartition::sNeedsOcclusionCheck = false;

bool check_read(LLFile* infile, U8* src, S32 bytes)
{
	// Note: eof() is true when getStream() is NULL, so there is no need to
	// test for the latter. HB
	return src && !infile->eof() && infile->read(src, bytes) == bytes;
}

bool check_write(LLFile* outfile, U8* dst, S32 bytes)
{
	return outfile->getStream() && dst && outfile->write(dst, bytes) == bytes;
}

//---------------------------------------------------------------------------
// LLGLTFOverrideCacheEntry
//---------------------------------------------------------------------------

bool LLGLTFOverrideCacheEntry::fromLLSD(const LLSD& data)
{
	if (!data.has("local_id") || !data.has("region_handle_x") ||
		!data.has("region_handle_y"))
	{
		LL_DEBUGS("ObjectCache") << "Missing data. local_id: "
								 << data.has("local_id")
								 << " - region_handle_x: "
								 << data.has("region_handle_x")
								 << " - region_handle_y: "
								 << data.has("region_handle_y")
								 << LL_ENDL;
		return false;
	}

	mLocalId = data["local_id"].asInteger();

	U32 region_x = data["region_handle_x"].asInteger();
	U32 region_y = data["region_handle_y"].asInteger();
	mRegionHandle = to_region_handle(region_x, region_y);

	// Data format for GLTF materials as follow:
	//  - "sides" is a list of face indices;
	//  - "gltf_llsd" is a list of corresponding GLTF override LLSD.
	// Any side not represented in "sides" has no override.
	if (!data.has("sides") || !data.has("gltf_llsd"))
	{
		return true;	// No GLTF material for this object. It is fine !
	}

	const LLSD& sides = data.get("sides");
	const LLSD& gltf_llsd = data.get("gltf_llsd");
	size_t num_sides = sides.size();
	if (!num_sides || num_sides != gltf_llsd.size() || !sides.isArray() ||
		!gltf_llsd.isArray())
	{
		llwarns << "Invalid data for object with local id: " << mLocalId
				<< llendl;
		return false;
	}

	for (size_t i = 0; i < num_sides; ++i)
	{
		S32 side_idx = sides[i].asInteger();
		const LLSD& gltf_mat_llsd = gltf_llsd[i];
		mSides.emplace(side_idx, gltf_mat_llsd);
		LLGLTFMaterial* matp = new LLGLTFMaterial();
		matp->applyOverrideLLSD(gltf_mat_llsd);
		mGLTFMaterial.emplace(side_idx, matp);
	}

	return true;
}

LLSD LLGLTFOverrideCacheEntry::toLLSD() const
{
	LLSD data;
	data["local_id"] = (LLSD::Integer)mLocalId;

	U32 region_x, region_y;
	from_region_handle(mRegionHandle, &region_x, &region_y);
	data["region_handle_x"] = (LLSD::Integer)region_x;
	data["region_handle_y"] = (LLSD::Integer)region_y;

	for (data_map_t::const_iterator it = mSides.begin(), end = mSides.end();
		 it != end; ++it)
	{
		data["sides"].append(LLSD::Integer(it->first));
		data["gltf_llsd"].append(it->second);
	}

	return data;
}

//---------------------------------------------------------------------------
// LLVOCacheEntry
//---------------------------------------------------------------------------

LLVOCacheEntry::LLVOCacheEntry(U32 local_id, U32 crc,
							   LLDataPackerBinaryBuffer& dp)
:	LLViewerOctreeEntryData(LLViewerOctreeEntry::LLVOCACHEENTRY),
	mLocalID(local_id),
	mCRC(crc),
	mUpdateFlags(-1),
	mHitCount(0),
	mDupeCount(0),
	mCRCChangeCount(0),
	mState(INACTIVE),
	mSceneContrib(0.f),
	mValid(true),
	mParentID(0),
	mBSphereRadius(-1.0f)
{
	mBuffer = new U8[dp.getBufferSize()];
	mDP.assignBuffer(mBuffer, dp.getBufferSize());
	mDP = dp;
}

LLVOCacheEntry::LLVOCacheEntry()
:	LLViewerOctreeEntryData(LLViewerOctreeEntry::LLVOCACHEENTRY),
	mLocalID(0),
	mCRC(0),
	mUpdateFlags(-1),
	mHitCount(0),
	mDupeCount(0),
	mCRCChangeCount(0),
	mBuffer(NULL),
	mState(INACTIVE),
	mSceneContrib(0.f),
	mValid(true),
	mParentID(0),
	mBSphereRadius(-1.0f)
{
	mDP.assignBuffer(mBuffer, 0);
}

LLVOCacheEntry::LLVOCacheEntry(LLFile* infile)
:	LLViewerOctreeEntryData(LLViewerOctreeEntry::LLVOCACHEENTRY),
	mBuffer(NULL),
	mUpdateFlags(-1),
	mState(INACTIVE),
	mSceneContrib(0.f),
	mValid(false),
	mParentID(0),
	mBSphereRadius(-1.0f)
{
	mDP.assignBuffer(mBuffer, 0);

	S32 size = -1;
	static U32 data_buffer[6];
	bool success = check_read(infile, (U8*)data_buffer, 6 * sizeof(U32));
	if (success)
	{
		U32* ptr = data_buffer;
		mLocalID = *ptr++;
		mCRC = *ptr++;
		mHitCount = (S32)*ptr++;
		mDupeCount = (S32)*ptr++;
		mCRCChangeCount = (S32)*ptr++;
		size = (S32)*ptr;
		if (size > 10000 || size < 1)	// Corruption in the cache entries ?
		{
			// We have got a bogus size, skip reading it. We will not bother
			// seeking, because the rest of this file is likely bogus, and will
			// be tossed anyway.
			llwarns << "Bogus cache entry, size " << size << ", aborting !"
					<< llendl;
			success = false;
		}
	}
	if (success && size > 0)
	{
		mBuffer = new U8[size];
		success = check_read(infile, mBuffer, size);
		if (success)
		{
			mDP.assignBuffer(mBuffer, size);
		}
		else
		{
			delete[] mBuffer;
			mBuffer = NULL;
		}
	}

	if (!success)
	{
		mLocalID = 0;
		mCRC = 0;
		mHitCount = 0;
		mDupeCount = 0;
		mCRCChangeCount = 0;
		mBuffer = NULL;
		mEntry = NULL;
		mState = INACTIVE;
	}
}

LLVOCacheEntry::~LLVOCacheEntry()
{
	mDP.freeBuffer();
}

void LLVOCacheEntry::updateEntry(U32 crc, LLDataPackerBinaryBuffer& dp)
{
	if (mCRC != crc)
	{
		mCRC = crc;
		++mCRCChangeCount;
	}

	mDP.freeBuffer();

	llassert_always(dp.getBufferSize() > 0);
	mBuffer = new U8[dp.getBufferSize()];
	mDP.assignBuffer(mBuffer, dp.getBufferSize());
	mDP = dp;
}

void LLVOCacheEntry::setParentID(U32 id)
{
	if (mParentID != id)
	{
		removeAllChildren();
		mParentID = id;
	}
}

void LLVOCacheEntry::removeAllChildren()
{
	if (mChildrenList.empty())
	{
		return;
	}
	for (set_t::iterator iter = mChildrenList.begin(),
						 end = mChildrenList.end();
		 iter != end; ++iter)
	{
		LLVOCacheEntry* entry = *iter;
		if (entry)	// Paranoia
		{
			entry->setParentID(0);
		}
	}
	mChildrenList.clear();
}

//virtual
void LLVOCacheEntry::setOctreeEntry(LLViewerOctreeEntry* entry)
{
	if (!entry && mDP.getBufferSize() > 0)
	{
		LLUUID fullid;
		LLViewerObject::unpackUUID(&mDP, fullid, "ID");

		LLViewerObject* obj = gObjectList.findObject(fullid);
		if (obj && obj->mDrawable)
		{
			entry = obj->mDrawable->getEntry();
		}
	}

	LLViewerOctreeEntryData::setOctreeEntry(entry);
}

void LLVOCacheEntry::setState(U32 state)
{
	if (state > LOW_BITS) // special states
	{
		mState |= HIGH_BITS & state;
		return;
	}

	// Otherwise LOW_BITS states
	clearState(LOW_BITS);
	mState |= (LOW_BITS & state);

	if (getState() == ACTIVE)
	{
#if HB_AJUSTED_VOCACHE_PARAMETERS
		F32 fps_ratio_to_target = F32_MAX;
		if (gFrameIntervalSeconds > 0.f)
		{
			fps_ratio_to_target = 1.f / (F32)TARGET_FPS / gFrameIntervalSeconds;
		}
		S32 min_interval = llmin((S32)((64.f + (F32)sMinFrameRange) *
									   fps_ratio_to_target), 10);
#else
		S32 min_interval = 64 + sMinFrameRange;
#endif
		S32 last_visible = getVisible();

		setVisible();

		S32 cur_visible = getVisible();
		if (cur_visible - last_visible > min_interval ||
			cur_visible < min_interval)
		{
			mLastCameraUpdated = 0; // Reset
		}
		else
		{
			mLastCameraUpdated = LLViewerRegion::sLastCameraUpdated;
		}
	}
}

void LLVOCacheEntry::addChild(LLVOCacheEntry* entry)
{
	if (!entry || !entry->getEntry() || entry->getParentID() != mLocalID)
	{
		llassert(false);
		return;
	}

	mChildrenList.insert(entry);

	// Update parent bbox
	if (getEntry() && isState(INACTIVE))
	{
		updateParentBoundingInfo(entry);
		resetVisible();
	}
}

void LLVOCacheEntry::removeChild(LLVOCacheEntry* entry)
{
	entry->setParentID(0);

	set_t::iterator iter = mChildrenList.find(entry);
	if (iter != mChildrenList.end())
	{
		mChildrenList.erase(iter);
	}
}

// Removes the first child and returns it.
LLVOCacheEntry* LLVOCacheEntry::getChild()
{
	LLVOCacheEntry* child = NULL;
	set_t::iterator iter = mChildrenList.begin();
	if (iter != mChildrenList.end())
	{
		child = *iter;
		mChildrenList.erase(iter);
	}
	return child;
}

LLDataPackerBinaryBuffer* LLVOCacheEntry::getDP()
{
	return mDP.getBufferSize() ? &mDP : NULL;
}

void LLVOCacheEntry::dump() const
{
	llinfos << "local " << mLocalID << " crc " << mCRC << " hits " << mHitCount
			<< " dupes " << mDupeCount << " change " << mCRCChangeCount
			<< llendl;
}

bool LLVOCacheEntry::writeToFile(LLFile* outfile) const
{
	if (!mBuffer)
	{
		llwarns << "NULL buffer for id " << mLocalID << llendl;
		return false;
	}

	S32 size = mDP.getBufferSize();
	if (size > 10000 || size < 1)
	{
		llwarns << "Invalid object cache entry size (" << size << ") for id "
				<< mLocalID << llendl;
		return false;
	}

	static U32 data_buffer[6];
	U32* ptr = data_buffer;
	*ptr++ = mLocalID;
	*ptr++ = mCRC;
	*ptr++ = (U32)mHitCount;
	*ptr++ = (U32)mDupeCount;
	*ptr++ = (U32)mCRCChangeCount;
	*ptr = (U32)size;
	if (!check_write(outfile, (U8*)data_buffer, 6 * sizeof(U32)))
	{
		llwarns << "Failed to write cache entry header for id " << mLocalID
				<< llendl;
		return false;
	}


	if (!check_write(outfile, (U8*)mBuffer, size))
	{
		llwarns << "Failed to write cache entry body for id " << mLocalID
				<< llendl;
		return false;

	}

	return outfile->flush();
}

//static
void LLVOCacheEntry::updateSettings()
{
	F32 draw_distance = gAgent.mDrawDistance;

	// The number of frames invisible objects stay in memory
	static LLCachedControl<U32> inv_obj_time(gSavedSettings,
											 "NonVisibleObjectsInMemoryTime");
#if HB_AJUSTED_VOCACHE_PARAMETERS
	// Whether or not we use the texture discard bias to bias the objects
	// retention, thus lowering the memory consumption by cached objects when
	// the textures memory usage gets higher.
	static LLCachedControl<bool> biased(gSavedSettings,
										"BiasedObjectRetention");
	sBiasedRetention = biased;

	// Make 0 to be the maximum
	sMinFrameRange = (inv_obj_time * TARGET_FPS) - 1;
#else
	sMinFrameRange = inv_obj_time - 1;
#endif
	LL_DEBUGS("ObjectCache") << "Min frame range = " << sMinFrameRange
							 << " frames." << LL_ENDL;

	// Min radius: all objects within this radius remain loaded in memory
	static LLCachedControl<F32> min_radius(gSavedSettings,
										   "SceneLoadMinRadius");
	// Cannot exceed the draw distance
	sNearRadius = llmin((F32)min_radius, draw_distance);
	sNearRadius = llmax(sNearRadius, 1.f); // Minimum value is 1m
	LL_DEBUGS("ObjectCache") << "Near radius = " << sNearRadius << "m."
							 << LL_ENDL;

	// Objects within the view frustum whose visible area is greater than this
	// threshold will be loaded
	static LLCachedControl<F32> front_pixel_threshold(gSavedSettings,
													  "SceneLoadFrontPixelThreshold");
	sFrontPixelThreshold = front_pixel_threshold;
	LL_DEBUGS("ObjectCache") << "Front objects threshold = "
							 << sFrontPixelThreshold << " pixels." << LL_ENDL;

	// Objects out of the view frustum whose visible area is greater than this
	// threshold will remain loaded
	static LLCachedControl<F32> rear_pixel_threshold(gSavedSettings,
													 "SceneLoadRearPixelThreshold");
	sRearPixelThreshold = rear_pixel_threshold;
	// Cannot be smaller than sFrontPixelThreshold.
	sRearPixelThreshold = llmax(sRearPixelThreshold, sFrontPixelThreshold);
	LL_DEBUGS("ObjectCache") << "Rear objects threshold = "
							 << sRearPixelThreshold << " pixels." << LL_ENDL;

	// A percentage of draw distance beyond which all objects outside of view
	// frustum will be unloaded, regardless of pixel threshold
	static LLCachedControl<F32> rear_max_radius_frac(gSavedSettings,
													 "SceneLoadRearMaxRadiusFraction");
	// Minimum value is 1m
	sRearFarRadius = llmax((F32)rear_max_radius_frac * draw_distance / 100.f,
						   1.f);
	// Cannot be less than "SceneLoadMinRadius".
	sRearFarRadius = llmax(sRearFarRadius, (F32)min_radius);
	// Cannot be more than the draw distance.
	sRearFarRadius = llmin(sRearFarRadius, draw_distance);
	LL_DEBUGS("ObjectCache") << "Rear far radius = " << sRearFarRadius << "m."
							 << LL_ENDL;
}

//static
F32 LLVOCacheEntry::getSquaredPixelThreshold(bool is_front)
{
	F32 threshold;
	if (is_front)
	{
		threshold = sFrontPixelThreshold;
	}
	else
	{
		threshold = sRearPixelThreshold;
	}
	// Object projected area threshold
	F32 pixel_meter_ratio = gViewerCamera.getPixelMeterRatio();
	F32 projection_threshold =
		pixel_meter_ratio > 0.f ? threshold / pixel_meter_ratio : 0.f;
	return projection_threshold * projection_threshold;
}

bool LLVOCacheEntry::isAnyVisible(const LLVector4a& camera_origin,
								  const LLVector4a& local_camera_origin,
								  F32 dist_threshold)
{
	LLOcclusionCullingGroup* group = (LLOcclusionCullingGroup*)getGroup();
	if (!group)
	{
		return false;
	}

	// Any visible
	bool vis = group->isAnyRecentlyVisible();
	if (!vis)	// Not ready to remove
	{
		S32 cur_vis = llmax(group->getAnyVisible(), (S32)getVisible());
#if HB_AJUSTED_VOCACHE_PARAMETERS
		// Adjust the delta based on the actual frame rate so that it
		// translates into seconds.
		F32 fps_ratio_to_target = F32_MAX;
		if (gFrameIntervalSeconds > 0.f)
		{
			fps_ratio_to_target = 1.f / (F32)TARGET_FPS / gFrameIntervalSeconds;
		}
		S32 delta = (S32)((F32)sMinFrameRange * fps_ratio_to_target);
		if (sBiasedRetention)
		{
			// Adjust the delta time depending on the discard bias (the higher
			// the latter, the lower the former). This means that we release
			// the non-visible objects sooner when the memory consumption gets
			// higher.
			delta = (S32)((F32)delta / (LLViewerTexture::sDesiredDiscardBias + 1.f));
		}
#else
		S32 delta = sMinFrameRange;
#endif
		vis = cur_vis + delta > LLViewerOctreeEntryData::getCurrentFrame();
	}

	// Within the back sphere
	if (!vis && !mParentID &&
		!group->isOcclusionState(LLOcclusionCullingGroup::OCCLUDED))
	{
		LLVector4a look_at;
		if (mBSphereRadius > 0.f)
		{
			look_at.setSub(mBSphereCenter, local_camera_origin);
			dist_threshold += mBSphereRadius;
		}
		else
		{
			look_at.setSub(getPositionGroup(), camera_origin);
			dist_threshold += getBinRadius();
		}

		vis = look_at.dot3(look_at).getF32() < dist_threshold * dist_threshold;
	}

	return vis;
}

void LLVOCacheEntry::calcSceneContribution(const LLVector4a& camera_origin,
										   bool needs_update, U32 last_update,
										   F32 max_dist)
{
	if (!needs_update && getVisible() >= last_update)
	{
		return; // no need to update
	}

	LLVector4a look_at;
	look_at.setSub(getPositionGroup(), camera_origin);
	F32 near_radius = sNearRadius;
#if HB_AJUSTED_VOCACHE_PARAMETERS
	if (sBiasedRetention)
	{
		near_radius /= LLViewerTexture::sDesiredDiscardBias / 3.f + 1.f;
	}
#endif
	F32 distance = look_at.getLength3().getF32() - near_radius;
	if (distance <= 0.f)
	{
		// Nearby objects, set a large number to force to load the object.
		constexpr F32 LARGE_SCENE_CONTRIBUTION = 1000.f;
		mSceneContrib = LARGE_SCENE_CONTRIBUTION;
	}
	else
	{
		F32 rad = getBinRadius();
		max_dist += rad;

		if (distance + near_radius < max_dist)
		{
			mSceneContrib = rad * rad / distance;
		}
		else
		{
			mSceneContrib = 0.f; // out of draw distance, not to load
		}
	}

	setVisible();
}

void LLVOCacheEntry::saveBoundingSphere()
{
	mBSphereCenter = getPositionGroup();
	mBSphereRadius = getBinRadius();
}

void LLVOCacheEntry::setBoundingInfo(const LLVector3& pos,
									 const LLVector3& scale)
{
	LLVector4a center, new_min, new_max;
	center.load3(pos.mV);
	LLVector4a size;
	size.load3(scale.mV);
	new_min.setSub(center, size);
	new_max.setAdd(center, size);

	setPositionGroup(center);
	setSpatialExtents(new_min, new_max);

	if (getNumOfChildren() > 0) // has children
	{
		updateParentBoundingInfo();
	}
	else
	{
		setBinRadius(llmin(size.getLength3().getF32() * 4.f, 256.f));
	}
}

// Make the parent bounding box to include all children
void LLVOCacheEntry::updateParentBoundingInfo()
{
	if (mChildrenList.empty())
	{
		return;
	}

	for (set_t::iterator iter = mChildrenList.begin(),
						 end = mChildrenList.end();
		 iter != end; ++iter)
	{
		updateParentBoundingInfo(*iter);
	}
	resetVisible();
}

// Make the parent bounding box to include this child
void LLVOCacheEntry::updateParentBoundingInfo(const LLVOCacheEntry* child)
{
	const LLVector4a* child_exts = child->getSpatialExtents();
	LLVector4a new_min, new_max;
	new_min = child_exts[0];
	new_max = child_exts[1];

	// Move to regional space.
	const LLVector4a& parent_pos = getPositionGroup();
	new_min.add(parent_pos);
	new_max.add(parent_pos);

	// Update parent's bbox(min, max)
	const LLVector4a* parent_exts = getSpatialExtents();
	update_min_max(new_min, new_max, parent_exts[0]);
	update_min_max(new_min, new_max, parent_exts[1]);

	// Clamping
	static const LLVector4a min_vector(-65536.f);
	static const LLVector4a max_vector(65536.f);
	new_min.clamp(min_vector, max_vector);
	new_max.clamp(min_vector, max_vector);

	setSpatialExtents(new_min, new_max);

	// Update parent's bbox center
	LLVector4a center;
	center.setAdd(new_min, new_max);
	center.mul(0.5f);
	setPositionGroup(center);

	// Update parent's bbox size vector
	LLVector4a size;
	size.setSub(new_max, new_min);
	size.mul(0.5f);
	setBinRadius(llmin(size.getLength3().getF32() * 4.f, 256.f));
}

//-------------------------------------------------------------------
// LLVOCacheGroup
//-------------------------------------------------------------------

LLVOCacheGroup::~LLVOCacheGroup()
{
	if (mOcclusionState[LLViewerCamera::CAMERA_WORLD] & ACTIVE_OCCLUSION)
	{
		((LLVOCachePartition*)mSpatialPartition)->removeOccluder(this);
	}
}

//virtual
void LLVOCacheGroup::handleChildAddition(const OctreeNode* parent,
										 OctreeNode* child)
{
	if (child->getListenerCount() == 0)
	{
		new LLVOCacheGroup(child, mSpatialPartition);
	}
	else
	{
		llwarns << "Redundancy detected." << llendl;
		llassert(false);
	}

	unbound();

	((LLViewerOctreeGroup*)child->getListener(0))->unbound();
}

//-------------------------------------------------------------------
// LLVOCachePartition
//-------------------------------------------------------------------

LLVOCachePartition::LLVOCachePartition(LLViewerRegion* regionp)
{
	mLODPeriod = 16;
	mRegionp = regionp;
	mPartitionType = LLViewerRegion::PARTITION_VO_CACHE;
	mBackSlectionEnabled = -1;
	mIdleHash = 0;

	for (S32 i = 0; i < LLViewerCamera::NUM_CAMERAS; ++i)
	{
		mCulledTime[i] = 0;
	}
	mCullHistory = -1;

	new LLVOCacheGroup(mOctree, this);
}

LLVOCachePartition::~LLVOCachePartition()
{
	// SL-17276 make sure to do base class cleanup while this instance can
	// can still be treated as an LLVOCachePartition.
	cleanup();
}

bool LLVOCachePartition::addEntry(LLViewerOctreeEntry* entry)
{
	llassert(entry->hasVOCacheEntry());
	if (!llfinite(entry->getBinRadius()) ||
		!entry->getPositionGroup().isFinite3())
	{
		return false;	// data is corrupted
	}

	mOctree->insert(entry);
	return true;
}

void LLVOCachePartition::removeEntry(LLViewerOctreeEntry* entry)
{
	entry->getVOCacheEntry()->setGroup(NULL);
	llassert(!entry->getGroup());
}

class LLVOCacheOctreeCull final : public LLViewerOctreeCull
{
public:
	LLVOCacheOctreeCull(LLCamera* camera, LLViewerRegion* regionp,
						const LLVector3& shift, bool use_cache_occlusion,
						F32 pixel_threshold, F32 near_radius,
						LLVOCachePartition* part)
	:	LLViewerOctreeCull(camera),
		mRegionp(regionp),
		mUseObjectCacheOcclusion(use_cache_occlusion),
		mPixelThreshold(pixel_threshold),
		mNearRadius(near_radius),
		mPartition(part)
	{
		mLocalShift = shift;
	}

	bool earlyFail(LLViewerOctreeGroup* base_group) override
	{
		if (mUseObjectCacheOcclusion &&
			// never occlusion-cull the root node
			base_group->getOctreeNode()->getParent())
		{
			LLOcclusionCullingGroup* group =
				(LLOcclusionCullingGroup*)base_group;
			if (group->needsUpdate())
			{
				// Needs to issue new occlusion culling check, perform view
				// culling check first.
				return false;
			}

			group->checkOcclusion();

			if (group->isOcclusionState(LLOcclusionCullingGroup::OCCLUDED))
			{
				return true;
			}
		}

		return false;
	}

	S32 frustumCheck(const LLViewerOctreeGroup* group) override
	{
#if 0
		S32 res = AABBInRegionFrustumGroupBounds(group);
#else
		S32 res = AABBInRegionFrustumNoFarClipGroupBounds(group);
		if (res != 0)
		{
			res = llmin(res,
						AABBRegionSphereIntersectGroupExtents(group,
															  mLocalShift));
		}
#endif

		return res;
	}

	S32 frustumCheckObjects(const LLViewerOctreeGroup* group) override
	{
#if 0
		S32 res = AABBInRegionFrustumObjectBounds(group);
#else
		S32 res = AABBInRegionFrustumNoFarClipObjectBounds(group);
		if (res != 0)
		{
			res = llmin(res,
						AABBRegionSphereIntersectObjectExtents(group,
															   mLocalShift));
		}
#endif

		if (res != 0)
		{
			// Check if the objects projection large enough
			const LLVector4a* exts = group->getObjectExtents();
			res = checkProjectionArea(exts[0], exts[1], mLocalShift,
									  mPixelThreshold, mNearRadius);
		}

		return res;
	}

	void processGroup(LLViewerOctreeGroup* base_group) override
	{
		if (!mUseObjectCacheOcclusion ||
			!base_group->getOctreeNode()->getParent())
		{
			// No occlusion check
			if (mRegionp->addVisibleGroup(base_group))
			{
				base_group->setVisible();
			}
			return;
		}

		LLOcclusionCullingGroup* group = (LLOcclusionCullingGroup*)base_group;
		if (group->needsUpdate() || !group->isRecentlyVisible())
		{
			// Needs to issue new occlusion culling check.
			mPartition->addOccluders(group);
			group->setVisible();
			return ; // wait for occlusion culling result
		}

		if (group->isOcclusionState(LLOcclusionCullingGroup::QUERY_PENDING) ||
			group->isOcclusionState(LLOcclusionCullingGroup::ACTIVE_OCCLUSION))
		{
			// Keep waiting
			group->setVisible();
		}
		else if (mRegionp->addVisibleGroup(base_group))
		{
			base_group->setVisible();
		}
	}

private:
	LLVOCachePartition*	mPartition;
	LLViewerRegion*		mRegionp;
	// Shift vector from agent space to local region space:
	LLVector3			mLocalShift;
	F32					mPixelThreshold;
	F32					mNearRadius;
	bool				mUseObjectCacheOcclusion;
};

// Select objects behind camera
class LLVOCacheOctreeBackCull final : public LLViewerOctreeCull
{
public:
	LL_INLINE LLVOCacheOctreeBackCull(LLCamera* camera, const LLVector3& shift,
									  LLViewerRegion* regionp, F32 threshold,
									  F32 radius, bool use_occlusion)
	:	LLViewerOctreeCull(camera),
		mRegionp(regionp),
		mPixelThreshold(threshold),
		mSphereRadius(radius),
		mUseObjectCacheOcclusion(use_occlusion)
	{
		mLocalShift = shift;
	}

	bool earlyFail(LLViewerOctreeGroup* base_group) override
	{
		if (mUseObjectCacheOcclusion &&
			// Never occlusion-cull the root node
			base_group->getOctreeNode()->getParent())
		{
			LLOcclusionCullingGroup* group =
				(LLOcclusionCullingGroup*)base_group;
			if (group->getOcclusionState() > 0)
			{
				// Occlusion state is not clear.
				return true;
			}
		}
		return false;
	}

	LL_INLINE S32 frustumCheck(const LLViewerOctreeGroup* group) override
	{
		const LLVector4a* exts = group->getExtents();
		return backSphereCheck(exts[0], exts[1]);
	}

	S32 frustumCheckObjects(const LLViewerOctreeGroup* group) override
	{
		const LLVector4a* exts = group->getObjectExtents();
		if (backSphereCheck(exts[0], exts[1]))
		{
			// Check if the objects projection large enough
			const LLVector4a* exts = group->getObjectExtents();
			return checkProjectionArea(exts[0], exts[1],
									   mLocalShift, mPixelThreshold,
									   mSphereRadius);
		}
		return false;
	}

	LL_INLINE void processGroup(LLViewerOctreeGroup* base_group) override
	{
		mRegionp->addVisibleGroup(base_group);
	}

private:
	// A sphere around the camera origin, including objects behind camera.
	LL_INLINE S32 backSphereCheck(const LLVector4a& min, const LLVector4a& max)
	{
		return AABBSphereIntersect(min, max,
								   mCamera->getOrigin() - mLocalShift,
								   mSphereRadius);
	}

private:
	F32				mSphereRadius;
	LLViewerRegion*	mRegionp;
	// shift vector from agent space to local region space.
	LLVector3		mLocalShift;
	F32				mPixelThreshold;
	bool			mUseObjectCacheOcclusion;
};

void LLVOCachePartition::selectBackObjects(LLCamera& camera,
										   F32 pixel_threshold,
										   bool use_occlusion)
{
	if (LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
	{
		return;
	}

	if (mBackSlectionEnabled < 0)
	{
		mBackSlectionEnabled = LLVOCacheEntry::sMinFrameRange - 1;
		mBackSlectionEnabled = llmax(mBackSlectionEnabled, (S32)1);
	}

	if (!mBackSlectionEnabled)
	{
		return;
	}

	// Localize the camera
	LLVector3 region_agent = mRegionp->getOriginAgent();

	F32 radius = LLVOCacheEntry::sRearFarRadius;
#if HB_AJUSTED_VOCACHE_PARAMETERS
	if (LLVOCacheEntry::sBiasedRetention)
	{
		radius /= LLViewerTexture::sDesiredDiscardBias / 3.f + 1.f;
	}
#endif
	LLVOCacheOctreeBackCull culler(&camera, region_agent, mRegionp,
								   pixel_threshold, radius, use_occlusion);
	culler.traverse(mOctree);

	if (mRegionp->getNumOfVisibleGroups())
	{
		--mBackSlectionEnabled;
	}
	else
	{
		mBackSlectionEnabled = 0;
	}
}

S32 LLVOCachePartition::cull(LLCamera& camera, bool do_occlusion)
{
	LL_FAST_TIMER(FTM_CULL_VOCACHE);

	static LLCachedControl<bool> use_cache_occlusion(gSavedSettings,
													 "UseObjectCacheOcclusion");
	if (!use_cache_occlusion)
	{
		do_occlusion = false;
	}

	if (!LLViewerRegion::sVOCacheCullingEnabled || mRegionp->isPaused())
	{
		return 0;
	}

	((LLViewerOctreeGroup*)mOctree->getListener(0))->rebound();

	if (LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
	{
		// No need for those cameras.
		return 0;
	}

	S32 frame = LLViewerOctreeEntryData::getCurrentFrame();
	if ((S32)mCulledTime[LLViewerCamera::sCurCameraID] == frame)
	{
		return 0; // Already culled
	}
	mCulledTime[LLViewerCamera::sCurCameraID] = frame;

	if (!mCullHistory && LLViewerRegion::isViewerCameraStatic())
	{
		U32 seed = llmax(mLODPeriod >> 1, 4U);
		if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
		{
			if (frame % seed == 0)
			{
				mIdleHash = (mIdleHash + 1) % seed;
			}
		}
		if (frame % seed != mIdleHash)
		{
			mFrontCull = false;

			// Process back objects selection
			selectBackObjects(camera,
							  LLVOCacheEntry::getSquaredPixelThreshold(mFrontCull),
							  do_occlusion);
			return 0; // Nothing changed, reduce frequency of culling
		}
	}
	else
	{
		mBackSlectionEnabled = -1; // Reset it.
	}

	// Localize the camera
	LLVector3 region_agent = mRegionp->getOriginAgent();
	camera.calcRegionFrustumPlanes(region_agent, gAgent.mDrawDistance);

	mFrontCull = true;

	F32 near_radius = LLVOCacheEntry::sNearRadius;
#if HB_AJUSTED_VOCACHE_PARAMETERS
	if (LLVOCacheEntry::sBiasedRetention)
	{
		near_radius /= LLViewerTexture::sDesiredDiscardBias / 3.f + 1.f;
	}
#endif
	LLVOCacheOctreeCull culler(&camera, mRegionp, region_agent,
							   do_occlusion,
							   LLVOCacheEntry::getSquaredPixelThreshold(mFrontCull),
							   near_radius, this);
	culler.traverse(mOctree);

	if (!sNeedsOcclusionCheck)
	{
		sNeedsOcclusionCheck = !mOccludedGroups.empty();
	}

	return 1;
}

void LLVOCachePartition::setCullHistory(bool has_new_object)
{
	mCullHistory <<= 1;
	mCullHistory |= has_new_object ? 1 : 0;
}

void LLVOCachePartition::addOccluders(LLViewerOctreeGroup* gp)
{
	LLVOCacheGroup* group = (LLVOCacheGroup*)gp;
	if (!group->isOcclusionState(LLOcclusionCullingGroup::ACTIVE_OCCLUSION))
	{
		group->setOcclusionState(LLOcclusionCullingGroup::ACTIVE_OCCLUSION);
		mOccludedGroups.insert(group);
	}
}

void LLVOCachePartition::processOccluders(LLCamera* camera)
{
	if (mOccludedGroups.empty() ||
		// No need for those cameras
		LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
	{
		return;
	}

	LLVector3 region_agent = mRegionp->getOriginAgent();
	LLVector4a shift(region_agent[0], region_agent[1], region_agent[2]);
	for (std::set<LLVOCacheGroup*>::iterator iter = mOccludedGroups.begin(),
											 end = mOccludedGroups.end();
		 iter != end; ++iter)
	{
		LLVOCacheGroup* group = *iter;
		if (group->isOcclusionState(LLOcclusionCullingGroup::ACTIVE_OCCLUSION))
		{
			group->doOcclusion(camera, &shift);
			group->clearOcclusionState(LLOcclusionCullingGroup::ACTIVE_OCCLUSION);
		}
	}

	// Safe to clear mOccludedGroups here because only the world camera
	// accesses it.
	mOccludedGroups.clear();
	sNeedsOcclusionCheck = false;
}

void LLVOCachePartition::resetOccluders()
{
	if (mOccludedGroups.empty())
	{
		return;
	}
	for (std::set<LLVOCacheGroup*>::iterator
			iter = mOccludedGroups.begin(), end = mOccludedGroups.end();
		 iter != end; ++iter)
	{
		LLVOCacheGroup* group = *iter;
		group->clearOcclusionState(LLOcclusionCullingGroup::ACTIVE_OCCLUSION);
	}
	mOccludedGroups.clear();
	sNeedsOcclusionCheck = false;
}

void LLVOCachePartition::removeOccluder(LLVOCacheGroup* group)
{
	if (mOccludedGroups.empty())
	{
		return;
	}
	mOccludedGroups.erase(group);
}

///////////////////////////////////////////////////////////////////////////////
// LLVOCache
///////////////////////////////////////////////////////////////////////////////

// Format string used to construct filename for the object cache
static const char* OBJECT_CACHE_FILENAME = "%sobjects_%d_%d.slc";
static const char* OBJECT_CACHE_EXTRAS_FILENAME = "%sobjects_%d_%d_extras.slc";
static const char* HEADER_FILENAME = "%sobject.cache";
static const char* OBJECT_CACHE_DIRNAME = "objectcache";

constexpr U32 MAX_NUM_OBJECT_ENTRIES = 128;
constexpr U32 MIN_ENTRIES_TO_PURGE = 16;
constexpr U32 INVALID_TIME = 0;

LLVOCache::LLVOCache()
:	mInitialized(false),
	mReadOnly(true),
	mNumEntries(0),
	mCacheSize(1)
{
	mEnabled = gSavedSettings.getBool("ObjectDiskCacheEnabled");
	if (mEnabled)
	{
		llinfos << "Initializing with 1 worker thread." << llendl;
		mThreadPoolp.reset(new LLThreadPool("Object cache", 1));
		mThreadPoolp->start();
	}
	llinfos << "Objects cache created." << llendl;
}

LLVOCache::~LLVOCache()
{
	if (mThreadPoolp)
	{
		mThreadPoolp->close();
		mThreadPoolp.reset(nullptr);
		llinfos << "Thread pool destroyed." << llendl;
	}
	if (mEnabled)
	{
		writeCacheHeader();
		clearCacheInMemory();
	}
	llinfos << "Objects cache destroyed." << llendl;
}

void LLVOCache::setDirNames(ELLPath location)
{
	std::string grid;
	if (!gIsInSecondLife)
	{
		LLGridManager* gridmgrp = LLGridManager::getInstance();
		grid = LLDir::getScrubbedFileName(gridmgrp->getGridLabel()) + "_";
	}
	else if (!gIsInSecondLifeProductionGrid)
	{
		grid = "beta_";
	}
	mHeaderFileName = gDirUtilp->getExpandedFilename(location,
													 OBJECT_CACHE_DIRNAME,
													 llformat(HEADER_FILENAME,
															  grid.c_str()));
	mObjectCacheDirName = gDirUtilp->getExpandedFilename(location,
														 OBJECT_CACHE_DIRNAME);
}

void LLVOCache::initCache(ELLPath location, U32 size)
{
	if (!mEnabled)
	{
		llinfos << "Not initializing cache: cache is currently disabled."
				<< llendl;
		return;
	}

	if (mInitialized)
	{
		llwarns << "Cache already initialized." << llendl;
		return;
	}
	mInitialized = true;

	setDirNames(location);
	if (!mReadOnly)
	{
		LLFile::mkdir(mObjectCacheDirName);
	}
	mCacheSize = llclamp(size, MIN_ENTRIES_TO_PURGE, MAX_NUM_OBJECT_ENTRIES);
	readCacheHeader();

	if (mMetaInfo.mVersion != OBJECT_CACHE_VERSION ||
		mMetaInfo.mAddressSize != ADDRESS_SIZE)
	{
		mMetaInfo.mVersion = OBJECT_CACHE_VERSION;
		mMetaInfo.mAddressSize = ADDRESS_SIZE;
		if (mReadOnly) // Disable cache
		{
			clearCacheInMemory();
		}
		else // Delete the current cache if the format does not match.
		{
			removeCache();
		}
	}

	llinfos << "Cache initialized in directory: " << mObjectCacheDirName
			<< " - with cache header file name: " << mHeaderFileName
			<< " - cache in read" << (mReadOnly ? " only" : "-write")
			<< " mode." << llendl;
}

void LLVOCache::removeCache(ELLPath location)
{
	if (mReadOnly)
	{
		llinfos << "Not removing cache at " << location
				<< ": cache is currently in read-only mode." << llendl;
		return;
	}

	llinfos << "About to remove the object cache due to settings." << llendl;

	std::string cache_dir = gDirUtilp->getExpandedFilename(location,
														   OBJECT_CACHE_DIRNAME);
	llinfos << "Removing object cache at " << cache_dir << llendl;
	LLDirIterator::deleteFilesInDir(cache_dir); // Delete all files
	LLFile::rmdir(cache_dir);

	clearCacheInMemory();
	mInitialized = false;
}

void LLVOCache::removeCache()
{
	if (mReadOnly)
	{
		llinfos << "Not clearing object cache which is currently in read-only mode."
				<< llendl;
		return;
	}

	if (!mInitialized)
	{
		// OK to remove cache even it is not initialized.
		llwarns << "Object cache is not initialized yet." << llendl;
	}

	llinfos << "Removing object cache at " << mObjectCacheDirName << llendl;
	LLDirIterator::deleteFilesInDir(mObjectCacheDirName);

	clearCacheInMemory();
	writeCacheHeader();
}

// Note: may be occasionnally called (indirectly, via removeEntry(U64 handle)
// below) from the cache workers, whenever a bad cache entry is found or the
// cache file cannot be read or written. HB
void LLVOCache::removeEntry(HeaderEntryInfo* entry)
{
	mMutex.lock();	// See above as for why we need a mutex here... HB

	if (entry && mInitialized && !mReadOnly)
	{
		header_entry_queue_t::iterator iter = mHeaderEntryQueue.find(entry);
		if (iter != mHeaderEntryQueue.end())
		{
			mHandleEntryMap.erase(entry->mHandle);
			mHeaderEntryQueue.erase(iter);
			removeFromCache(entry);
			delete entry;

			mNumEntries = mHandleEntryMap.size();
		}
	}

	mMutex.unlock();
}

// Note: may be occasionnally called from the cache workers, whenever a bad
// cache entry is found or the cache file cannot be read or written. HB
void LLVOCache::removeEntry(U64 handle)
{
	mMutex.lock();	// See above as for why we need a mutex here... HB

	handle_entry_map_t::iterator iter = mHandleEntryMap.find(handle);
	if (iter != mHandleEntryMap.end())
	{
		// Note: mMutex will again be locked, but it is OK since LLMutex
		// is recursive. HB
		removeEntry(iter->second);
	}

	mMutex.unlock();
}

void LLVOCache::clearCacheInMemory()
{
	if (!mHeaderEntryQueue.empty())
	{
		for (header_entry_queue_t::iterator iter = mHeaderEntryQueue.begin(),
											end = mHeaderEntryQueue.end();
			 iter != end; ++iter)
		{
			delete *iter;
		}
		mHeaderEntryQueue.clear();
		mHandleEntryMap.clear();
		mNumEntries = 0;
	}
}

void LLVOCache::getObjectCacheFilename(U64 handle, std::string& filename,
									   bool extra_entries)
{
	std::string grid;
	if (!gIsInSecondLife)
	{
		LLGridManager* gridmgrp = LLGridManager::getInstance();
		grid = LLDir::getScrubbedFileName(gridmgrp->getGridLabel()) + "_";
	}
	else if (!gIsInSecondLifeProductionGrid)
	{
		grid = "beta_";
	}
	U32 region_x, region_y;
	grid_from_region_handle(handle, &region_x, &region_y);
	const char* name_format = extra_entries ? OBJECT_CACHE_EXTRAS_FILENAME
											: OBJECT_CACHE_FILENAME;
	filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
											  OBJECT_CACHE_DIRNAME,
											  llformat(name_format,
													   grid.c_str(),
													   region_x, region_y));
}

void LLVOCache::removeFromCache(HeaderEntryInfo* entry)
{
	if (mReadOnly)
	{
		llinfos << "Not removing cache for handle " << entry->mHandle
				<< ": cache is currently in read-only mode." << llendl;
		return;
	}

	std::string filename;
	getObjectCacheFilename(entry->mHandle, filename);
	LLFile::remove(filename);
	entry->mTime = INVALID_TIME;
	updateEntry(entry);	// Update the head file.
}

void LLVOCache::readCacheHeader()
{
	// Initialize meta info, in case there is no cache to read
	mMetaInfo.mVersion = OBJECT_CACHE_VERSION;
	mMetaInfo.mAddressSize = ADDRESS_SIZE;

	if (!mEnabled)
	{
		LL_DEBUGS("ObjectCache") << "Not reading cache header: cache is currently disabled."
								 << LL_ENDL;
		return;
	}

	// Clear stale info.
	clearCacheInMemory();

	bool success = true;
	if (LLFile::exists(mHeaderFileName))
	{
		LLFile infile(mHeaderFileName, "rb");

		// Read the meta element
		success = check_read(&infile, (U8*)&mMetaInfo, sizeof(HeaderMetaInfo));
		if (success)
		{
			HeaderEntryInfo* entry = NULL;
			mNumEntries = 0;
			U32 num_read = 0;
			while (num_read++ < MAX_NUM_OBJECT_ENTRIES)
			{
				if (!entry)
				{
					entry = new HeaderEntryInfo();
				}
				success = check_read(&infile, (U8*)entry,
									 sizeof(HeaderEntryInfo));
				if (!success)
				{
					llwarns << "Error reading cache header entry. (entry_index="
							<< mNumEntries << ")" << llendl;
					delete entry;
					entry = NULL;
					break;
				}
				if (entry->mTime == INVALID_TIME)
				{
					continue; // An empty entry
				}

				entry->mIndex = mNumEntries++;
				mHeaderEntryQueue.insert(entry);
				mHandleEntryMap.emplace(entry->mHandle, entry);
				entry = NULL;
			}
			if (entry)
			{
				delete entry;
			}
		}
	}
	else
	{
		writeCacheHeader();
	}

	if (success && mNumEntries >= mCacheSize)
	{
		purgeEntries(mCacheSize);
	}
}

void LLVOCache::writeCacheHeader()
{
	if (mReadOnly || !mEnabled)
	{
		LL_DEBUGS("ObjectCache") << "Not writing cache header: cache is currently "
								 << (mEnabled ? "disabled." : "in read-only mode.")
								 << LL_ENDL;
		return;
	}

	bool success = true;
	{
		// Write the header file. Note that we are using "wb" (which overwrites
		// any existing file; this is essential to avoid writing a smaller
		// amount of data in a larger file, which would result in a "corrupted"
		// error on next read to EOF). HB
		LLFile outfile(mHeaderFileName, "wb");

		// Write the meta element
		success = check_write(&outfile, (U8*)&mMetaInfo,
							  sizeof(HeaderMetaInfo));

		mNumEntries = 0;
		for (header_entry_queue_t::iterator iter = mHeaderEntryQueue.begin(),
											end = mHeaderEntryQueue.end();
			 success && iter != end; ++iter)
		{
			(*iter)->mIndex = mNumEntries++;
			success = check_write(&outfile, (U8*)*iter,
								  sizeof(HeaderEntryInfo));
		}

		mNumEntries = mHeaderEntryQueue.size();
		if (success && mNumEntries < MAX_NUM_OBJECT_ENTRIES)
		{
			HeaderEntryInfo* entry = new HeaderEntryInfo();
			entry->mTime = INVALID_TIME;
			for (S32 i = mNumEntries;
				 success && i < (S32)MAX_NUM_OBJECT_ENTRIES; ++i)
			{
				// Fill the cache with the default entry.
				success = check_write(&outfile, (U8*)entry,
									  sizeof(HeaderEntryInfo));
			}
			delete entry;
		}
	}

	if (!success)
	{
		clearCacheInMemory();
		mReadOnly = true;	// Disable the cache on failure to write it.
	}
}

bool LLVOCache::updateEntry(const HeaderEntryInfo* entry)
{
	// NOT using "wb" here, since we seek to an entry to update it.
	LLFile outfile(mHeaderFileName, "r+b");
	S64 offset = entry->mIndex * sizeof(HeaderEntryInfo) +
				 sizeof(HeaderMetaInfo);
	if (outfile.seek(offset) != offset)
	{
		llwarns << "Failed to seek to entry index " << entry->mIndex << llendl;
		return false;
	}

	if (!check_write(&outfile, (U8*)entry, sizeof(HeaderEntryInfo)))
	{
		llwarns << "Failed to write entry at index " << entry->mIndex
				<< llendl;
		return false;
	}

	return outfile.flush();
}

void LLVOCache::readFromCache(U64 handle, const std::string& region_name,
							  const LLUUID& id)
{
	LL_TRACY_TIMER(TRC_OBJ_CACHE_READ);

	static LLCachedControl<bool> allow_read(gSavedSettings,
											"ObjectDiskCacheReads");
	if (!mEnabled || !allow_read)
	{
		LLViewerRegion::cacheLoadedCallback(handle, NULL, NULL);
		return;
	}

	if (!mInitialized)
	{
		llwarns << "Call done while not initialized !" << llendl;
		llassert(false);
		LLViewerRegion::cacheLoadedCallback(handle, NULL, NULL);
		return;
	}

	LLTimer read_timer;
	read_timer.reset();

	handle_entry_map_t::iterator iter = mHandleEntryMap.find(handle);
	if (iter == mHandleEntryMap.end()) // No cache
	{
		llinfos << "Cache miss for region: " << region_name << llendl;
		LLViewerRegion::cacheLoadedCallback(handle, NULL, NULL);
		return;
	}

	static LLCachedControl<bool> use_thread_pool(gSavedSettings,
												 "ThreadedObjectCacheReads");
	// Note: cannot queue when shutting down (it would crash). HB
	if (!use_thread_pool || LLApp::isExiting() || !mThreadPoolp)
 	{
		ReadWorker worker(handle, id, region_name);
		worker.readCacheFile();
		llinfos << "Object cache read for region '" << region_name << "' in "
				<< read_timer.getElapsedTimeF32() * 1000.f << "ms." << llendl;
 		return;
 	}

	std::string filename;
	getObjectCacheFilename(handle, filename);
	if (!LLFile::exists(filename))
	{
		llwarns << "Could not find: " << filename << " - Region: "
				<< region_name << ". Removing entry." << llendl;
		removeEntry(iter->second);
		return;
	}

	// Queue the cache file read
	mThreadPoolp->getQueue().post(
		[worker = ReadWorker(handle, id, region_name)]() mutable
 		{
			LL_TRACY_TIMER(TRC_OBJ_CACHE_THREAD_READ);
			// Queued reads are aborted on shutdown to prevent crashes (because
			// LLThreadPool did already shut down on LLApp::isExiting()); this
			// it not a problem at all (too late to rez objects). HB
			if (!LLApp::isExiting())
 			{
				worker.readCacheFile();
 			}
		});

	llinfos << "Queued cache read operation for region '" << region_name
			<< "' in " << read_timer.getElapsedTimeF32() * 1000.f << "ms."
			<< llendl;
}

void LLVOCache::purgeEntries(U32 size)
{
	while (mHeaderEntryQueue.size() > size)
	{
		header_entry_queue_t::iterator iter = mHeaderEntryQueue.begin();
		HeaderEntryInfo* entry = *iter;
		mHandleEntryMap.erase(entry->mHandle);
		mHeaderEntryQueue.erase(iter);
		removeFromCache(entry);
		delete entry;
	}
	mNumEntries = mHandleEntryMap.size();
}

void LLVOCache::writeToCache(U64 handle, const std::string& region_name,
							 const LLUUID& id,
							 LLVOCacheEntry::map_t& entry_map,
							 bool dirty_cache,
							 LLVOCacheEntry::emap_t& extras_map,
							 bool removal_enabled)
{
	LL_TRACY_TIMER(TRC_OBJ_CACHE_WRITE);

	static LLCachedControl<bool> allow_write(gSavedSettings,
											"ObjectDiskCacheWrites");
	if (mReadOnly || !mEnabled || !allow_write)
	{
		return;
	}
	if (!mInitialized)
	{
		llwarns << "Call done while not initialized !" << llendl;
		llassert(false);
		return;
	}
	if (entry_map.empty())
	{
		llinfos << "Empty cache map data for region: " << region_name
				<< ". Not writing an object cache file." << llendl;
		return;
	}

	LLTimer write_timer;
	write_timer.reset();

	if (removal_enabled)
	{
		bool has_valid_entry = false;
		for (LLVOCacheEntry::map_t::const_iterator
				iter = entry_map.begin(), end = entry_map.end();
			 iter != end; ++iter)
		{
			if (iter->second->isValid())
			{
				has_valid_entry = true;
				break;
			}
		}
		if (!has_valid_entry)
		{
			LL_DEBUGS("ObjectCache") << "Skipping write to cache for region: "
									 << region_name
									 << ". No valid cache entry." << LL_ENDL;
			return;
		}
	}

	HeaderEntryInfo* entry;
	handle_entry_map_t::iterator iter = mHandleEntryMap.find(handle);
	if (iter == mHandleEntryMap.end()) // New entry
	{
		if (mNumEntries >= mCacheSize - 1)
		{
			purgeEntries(mCacheSize - 1);
		}

		entry = new HeaderEntryInfo();
		entry->mHandle = handle;
		entry->mTime = time(NULL);
		entry->mIndex = mNumEntries++;
		mHeaderEntryQueue.insert(entry);
		mHandleEntryMap.emplace(handle, entry);
	}
	else
	{
		// Update access time.
		entry = iter->second;

		// Resort
		mHeaderEntryQueue.erase(entry);

		entry->mTime = time(NULL);
		mHeaderEntryQueue.insert(entry);
	}

	// Update cache header
	if (!updateEntry(entry))
	{
		llwarns << "Failed to update cache header index " << entry->mIndex
				<< " for region: " << region_name << " - handle = " << handle
				<< " - Time taken: "
				<< write_timer.getElapsedTimeF32() * 1000.f << "ms." << llendl;
		return; // Update failed.
	}

	if (!dirty_cache)
	{
		LL_DEBUGS("ObjectCache") << "Skipping write to cache for region: "
								 << region_name << ". Cache not dirty."
								 << LL_ENDL;
		return; // Nothing changed, no need to update.
	}

	static LLCachedControl<bool> use_thread_pool(gSavedSettings,
												 "ThreadedObjectCacheWrites");
	// Note: cannot queue when shutting down (it would crash). HB
	if (!use_thread_pool || LLApp::isExiting() || !mThreadPoolp)
	{
		WriteWorker worker(handle, id, region_name, entry_map, extras_map,
						   removal_enabled);
		worker.writeCacheFile();
		llinfos << "Saved objects for region '" << region_name << "' in "
				<< write_timer.getElapsedTimeF32() * 1000.f << "ms." << llendl;
		return;
	}

	// Queue the cache file write
	mThreadPoolp->getQueue().post(
		[worker = WriteWorker(handle, id, region_name, entry_map, extras_map,
						 	  removal_enabled)]() mutable
		{
			LL_TRACY_TIMER(TRC_OBJ_CACHE_THREAD_WRITE);
			// Queued saves are aborted on shutdown to prevent crashes (because
			// LLThreadPool did already shut down on LLApp::isExiting()); this
			// it not a big deal, since it is a *rare* condition (the user must
			// quit or crash just after a LLViewerRegion got deleted and before
			// its queued cache write is performed, meaning within 500ms or so
			// at worst), and it simply means that the corresponding cache file
			// would not be updated. HB
			if (!LLApp::isExiting())
			{
				worker.writeCacheFile();
			}
		});

	llinfos << "Queued cache save operation for region '" << region_name
			<< "' in " << write_timer.getElapsedTimeF32() * 1000.f << "ms."
			<< llendl;
}

///////////////////////////////////////////////////////////////////////////////
// LLVOCache::ReadWorker sub-class
///////////////////////////////////////////////////////////////////////////////

LLVOCache::ReadWorker::ReadWorker(U64 handle, const LLUUID& id,
								  const std::string& region_name)
:	mId(id),
	mHandle(handle),
	mRegionName(region_name)
{
}

void LLVOCache::ReadWorker::readCacheFile()
{
	bool success;
	LLVOCacheEntry::map_t* entry_map = NULL;
	// Read from cache file
	std::string filename;
	LLVOCache::getInstance()->getObjectCacheFilename(mHandle, filename);
	bool file_exists = LLFile::exists(filename);
	if (!file_exists)
	{
		success = false;
		llwarns << "Could not find: " << filename << " - Region: "
				<< mRegionName << llendl;
	}
	else
	{
		LLFile infile(filename, "rb");

		LLUUID cache_id;
		success = check_read(&infile, cache_id.mData, UUID_BYTES);
		if (success && cache_id != mId)
		{
			success = false;
			llinfos << "Cache Id does not match region: " << mRegionName
					<< ". Discarding." << llendl;
		}
		if (success)
		{
			entry_map = new LLVOCacheEntry::map_t();
			S32 num_entries;
			success = check_read(&infile, (U8*)&num_entries, sizeof(S32));
			if (success)
			{
				for (S32 i = 0; i < num_entries && !infile.eof(); ++i)
				{
					LLPointer<LLVOCacheEntry> entry =
						new LLVOCacheEntry(&infile);
					if (!entry->getLocalID())
					{
						success = infile.eof() && !entry_map->empty();
						if (!success)
						{
							llwarns << "Aborting cache file load for "
									<< filename
									<< ": cache file corruption detected."
									<< llendl;
						}
						break;
					}
					entry_map->emplace(entry->getLocalID(), entry);
				}
			}
		}
	}
	if (success)
	{
		llinfos << "Cache hit for region " << mRegionName << " on file: "
				<< filename << llendl;
	}
	else
	{
		if (entry_map)
		{
			delete entry_map;
			entry_map = NULL;
		}
		llinfos << "Removing cache entry for region: " << mRegionName
				<< llendl;
		LLVOCache::getInstance()->removeEntry(mHandle);
		if (file_exists)
		{
			llinfos << "Removing cache file: " << filename << llendl;
			LLFile::remove(filename);
		}
	}

	// Read extras GLTF materials data, if any and desired.
	LLVOCacheEntry::emap_t* extras_map = NULL;
	while (true)
	{
		LLVOCache::getInstance()->getObjectCacheFilename(mHandle, filename,
													     true);
		if (!LLFile::exists(filename))
		{
			// Since not all grids support GLTF, do not spam the log file, unless
			// we do need debugging.
			LL_DEBUGS("ObjectCache") << "No extras cache file: " << filename
									 << " - Region: " << mRegionName << llendl;
			break;
		}

		llifstream infile(filename, std::ios::in | std::ios::binary);
		std::string line;

		std::getline(infile, line);
		LLUUID cache_id;
		if (infile.good())
		{
			cache_id.set(line, false);
		}
		bool success = cache_id.notNull();
		if (success && cache_id != mId)
		{
			success = false;
			llwarns << "Extra data cache Id does not match region: "
					<< mRegionName << ". Discarding." << llendl;
		}

		while (success)
		{
			S32 num_entries = 0;
			std::getline(infile, line);
			if (infile.good())
			{
				num_entries = atoi(line.c_str());
			}
			if (num_entries <= 0)
			{
				success = false;
				break;
			}

			extras_map = new LLVOCacheEntry::emap_t();
			for (S32 i = 0; i < num_entries && !infile.eof(); ++i)
			{
				LLSD entry_llsd;
				std::getline(infile, line);
				std::stringstream sstream(line);
				success = LLSDSerialize::fromNotation(entry_llsd, sstream,
													  line.size()) > 0;
				if (!success)
				{
					llwarns << "Failure to parse entry " << i
							<< " in extras cache for region: " << mRegionName
							<< llendl;
					success = false;
					break;
				}
				U32 local_id = entry_llsd["local_id"].asInteger();
				if (!local_id)
				{
					llwarns << "Null local id for entry " << i
							<< " in extras cache for region: " << mRegionName
							<< llendl;
					continue;
				}
				LLGLTFOverrideCacheEntry entry;
				if (entry.fromLLSD(entry_llsd))
				{
					extras_map->emplace(local_id, entry);
				}
				else
				{
					llwarns << "Failed to read entry for local id " << local_id
							<< " in extras cache for region: " << mRegionName
							<< ". Data was:\n"
							<< ll_pretty_print_sd(entry_llsd) << llendl;
					// Do not keep the corresponding entry in the entry map:
					// it would cause a failure to rez materials since the
					// missing data would not be requested to the server. HB
					if (entry_map)
					{
						entry_map->erase(local_id);
					}
				}
			}
			break;
		}

		infile.close();
		if (success)
		{
			llinfos << "Extra data cache hit for region " << mRegionName
					<< " on file: " << filename << llendl;
			break;
		}
		llwarns << "Aborted extra data cache load for region '" << mRegionName
				<< ". Removing bad cache file: " << filename << llendl;
		LLFile::remove(filename);
		if (extras_map)
		{
			delete extras_map;
			extras_map = NULL;
		}
		break;
	}

	// Important: the callback shall be called from the main thread. HB
	if (is_main_thread())
	{
		LLViewerRegion::cacheLoadedCallback(mHandle, entry_map, extras_map);
	}
	else if (gMainloopWorkp)
	{
		LL_DEBUGS("ObjectCache") << "Queuing loaded callback for region "
								 << mRegionName << " (handle " << mHandle
								 << ")" << LL_ENDL;
		// We *MUST* copy the handle on the stack and capture the latter, else
		// we do not capture this->mHandle, but instead get the one of a new
		// worker getting created via LLVOCache::ReadWorker::operator() in the
		// lambda.
		U64 handle = mHandle;
		gMainloopWorkp->post([=]()
							 {
								LLViewerRegion::cacheLoadedCallback(handle,
																	entry_map,
																	extras_map);
							 });
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLVOCache::WriteWorker sub-class
///////////////////////////////////////////////////////////////////////////////

LLVOCache::WriteWorker::WriteWorker(U64 handle, const LLUUID& id,
									const std::string& region_name,
									LLVOCacheEntry::map_t& entry_map,
									LLVOCacheEntry::emap_t& extras_map,
									bool removal_enabled)
:	mId(id),
	mHandle(handle),
	mRegionName(region_name),
	mRemovalEnabled(removal_enabled)
{
	// We swap the maps passed by reference, for speed. It means the referenced
	// maps are emptied, but this is OK; see LLViewerRegion::saveObjectCache()
	// which is currently the only caller for LLVOCache::writeToCache(). HB
	mEntryMap.swap(entry_map);
	mExtraMap.swap(extras_map);
}

void LLVOCache::WriteWorker::writeCacheFile()
{
	// Write to cache file
	std::string filename;
	LLVOCache* cachep = LLVOCache::getInstance();
	cachep->getObjectCacheFilename(mHandle, filename);

	// Write the cache file. Note that we are using "wb" (which overwrites any
	// existing file; this is essential to avoid writing a smaller amount of
	// data in a larger file, which would result in a "corrupted" error on next
	// read to EOF). HB
	LLFile outfile(filename, "wb");

	bool success = check_write(&outfile, (U8*)mId.mData, UUID_BYTES);
	if (success)
	{
		S32 num_entries = mEntryMap.size();
		success = check_write(&outfile, (U8*)&num_entries, sizeof(S32));

		for (LLVOCacheEntry::map_t::const_iterator iter = mEntryMap.begin(),
												   end = mEntryMap.end();
			 success && iter != end; ++iter)
		{
			if (!mRemovalEnabled || iter->second->isValid())
			{
				success = iter->second->writeToFile(&outfile);
				if (!success)
				{
					break;
				}
			}
		}
	}

	if (!success)
	{
		llwarns << "Aborted cache file write for region " << mRegionName
				<< " (failure to write to file: " << filename << ")."
				<< llendl;
		cachep->removeEntry(mHandle);
		LLFile::remove(filename);
	}

	// This used to be in a separate LLVOCache::writeExtrasToCache() method.
	// I integrated it here, so to thread this operation as well. HB

	cachep->getObjectCacheFilename(mHandle, filename, true);
	if (mExtraMap.empty())
	{
		if (LLFile::exists(filename))
		{
			llinfos << "Empty extra data for '" << mRegionName
					<< ". Removing stale file: " << filename << llendl;
			LLFile::remove(filename);
		}
		return;
	}

	cachep->getObjectCacheFilename(mHandle, filename, true);
	llofstream out_file(filename, std::ios::trunc | std::ios::binary);
	success = out_file.good();
	while (success)
	{
		out_file << mId << '\n' << U32(mExtraMap.size()) << '\n';
		if (!out_file.good())
		{
			success = false;
			break;
		}

		LLSD entry_llsd;
		for (auto const& entry : mExtraMap)
		{
			entry_llsd = entry.second.toLLSD();
			entry_llsd["local_id"] = (LLSD::Integer)entry.first;
			out_file << entry_llsd << '\n';
			if (!out_file.good())
			{
				success = false;
				break;
			}
		}
		break;
	}

	if (success)
	{
		llinfos << "Saved extra data for region '" << mRegionName
				<< " to file: " << filename << llendl;
		return;
	}
	llwarns << "Aborted extra cache write for region '" << mRegionName
			<< " (failure to write to file: " << filename << ")." << llendl;
}
