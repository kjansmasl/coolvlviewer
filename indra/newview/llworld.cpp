/**
 * @file llworld.cpp
 * @brief Initial test structure to organize viewer regions
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

#include <queue>

#include "llworld.h"

#include "llcorehttplibcurl.h"
#include "llglheaders.h"
#include "llhttpnode.h"
#include "llregionhandle.h"
#include "llrender.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawpool.h"
#include "llgridmanager.h"
#include "llpipeline.h"
#include "llsky.h"						// For gSky.cleanup()
#include "llsurface.h"
#include "llsurfacepatch.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"
#include "llvlcomposition.h"
#include "llvoavatar.h"
#include "llvowater.h"

//
// Globals
//
LLWorld gWorld;

U32 gAgentPauseSerialNum = 0;

// Magnitude along the x and y axis
const S32 gDirAxes[8][2] =
{
	{ 1, 0 },	// East
	{ 0, 1 },	// North
	{ -1, 0 },	// West
	{ 0, -1 },	// South
	{ 1, 1 },	// NE
	{ -1, 1 },	// NW
	{ -1, -1 },	// SW
	{ 1, -1 },	// SE
};

constexpr S32 WORLD_PATCH_SIZE = 16;

extern LLColor4U MAX_WATER_COLOR;

//
// Functions
//

// allocate the stack
LLWorld::LLWorld()
:	mLandFarClip(DEFAULT_FAR_PLANE),
	mLastPacketsIn(0),
	mLastPacketsOut(0),
	mLastPacketsLost(0),
	mLastCurlBytes(0),
	mLastRegionDisabling(0.f)
{
	for (S32 i = 0; i < EDGE_WATER_OBJECTS_COUNT; ++i)
	{
		mEdgeWaterObjects[i] = NULL;
	}
}

void LLWorld::initClass()
{
	LLPointer<LLImageRaw> raw = new LLImageRaw(1, 1, 4);
	U8* default_texture = raw->getData();
	if (default_texture)
	{
		*(default_texture++) = MAX_WATER_COLOR.mV[0];
		*(default_texture++) = MAX_WATER_COLOR.mV[1];
		*(default_texture++) = MAX_WATER_COLOR.mV[2];
		*(default_texture++) = MAX_WATER_COLOR.mV[3];
	}

	mDefaultWaterTexturep = LLViewerTextureManager::getLocalTexture(raw.get(),
																	false);
	if (mDefaultWaterTexturep)
	{
		gGL.getTexUnit(0)->bind(mDefaultWaterTexturep);
		mDefaultWaterTexturep->setAddressMode(LLTexUnit::TAM_CLAMP);
	}

	LLViewerRegion::sVOCacheCullingEnabled =
		gSavedSettings.getBool("RequestFullRegionCache");

	gViewerPartSim.initClass();

	llinfos << "World class initialized" << llendl;
}

void LLWorld::cleanupClass()
{
	llinfos << "Shutting down the World class..." << llendl;
	gObjectList.cleanupClass();
	gSky.cleanup();

	llinfos << "Removing regions..." << llendl;
	for (region_list_t::iterator region_it = mRegionList.begin();
		 region_it != mRegionList.end(); )
	{
		LLViewerRegion* region_to_delete = *region_it++;
		removeRegion(region_to_delete->getHost());
	}
	mRegionList.clear();
	mActiveRegionList.clear();
	mVisibleRegionList.clear();
	mCulledRegionList.clear();
	mDisabledRegionList.clear();

	gViewerPartSim.cleanupClass();

	llinfos << "Removing water edges..." << llendl;
	mDefaultWaterTexturep = NULL;
	for (S32 i = 0; i < EDGE_WATER_OBJECTS_COUNT; ++i)
	{
		mEdgeWaterObjects[i] = NULL;
	}

	// Make all visible drawables invisible.
	LLDrawable::incrementVisible();

	llinfos << "World class shut down." << llendl;
}

LLViewerRegion* LLWorld::addRegion(const U64& region_handle,
								   const LLHost& host, U32 width)
{
	LLViewerRegion* regionp = getRegionFromHandle(region_handle);
	std::string seed_url;
	if (regionp)
	{
		LLHost old_host = regionp->getHost();
		// region already exists!
		if (host == old_host && regionp->isAlive())
		{
			// This is a duplicate for the same host and it is alive, do not
			// bother.
			llinfos << "Region already exists and is alive, using existing region"
					<< llendl;
			mDisabledRegionList.remove(regionp); // cancel any delayed removal
			return regionp;
		}

		if (host != old_host)
		{
			llwarns << "Region exists, but old host " << old_host
					<< " does not match new host " << host
					<< ". Removing old region and creating a new one."
					<< llendl;
		}
		if (!regionp->isAlive())
		{
			llinfos << "Region exists, but is no more alive. Removing old region and creating a new one."
					<< llendl;
		}

		// Save capabilities seed URL
		seed_url = regionp->getCapability("Seed");

		// Kill the old host, and then we can continue on and add the new host.
		// We have to kill even if the host matches, because all the agent
		// state for the new camera is completely different.
		removeRegion(old_host);
	}
	else
	{
		LL_DEBUGS("World") << "Region does not exist, creating a new one."
						   << LL_ENDL;
	}

	U32 iindex = 0;
	U32 jindex = 0;
	from_region_handle(region_handle, &iindex, &jindex);
#if 1	// Variable region size support... Unintuitive to say the least...
	S32 x = (S32)(iindex / 256); // MegaRegion
	S32 y = (S32)(jindex / 256); // MegaRegion
#else
	S32 x = (S32)(iindex / width);
	S32 y = (S32)(jindex / width);
#endif
	llinfos << "Adding new region (" << x << ":" << y << ") on host: " << host
			<< " - Width: " << width << "m." << llendl;

	LLVector3d origin_global;

	origin_global = from_region_handle(region_handle);

	regionp = new LLViewerRegion(region_handle, host, width, WORLD_PATCH_SIZE,
								 width);
	if (!regionp)
	{
		llerrs << "Unable to create new region !" << llendl;
	}

	if (!seed_url.empty())
	{
		regionp->setCapability("Seed", seed_url);
	}

	mRegionList.push_back(regionp);
	mActiveRegionList.push_back(regionp);
	mCulledRegionList.push_back(regionp);
	mDisabledRegionList.remove(regionp); // cancel any delayed removal

	// Find all the adjacent regions, and attach them in the correct way.
	F32 region_x = 0.f;
	F32 region_y = 0.f;
	from_region_handle(region_handle, &region_x, &region_y);

	// Iterate through all directions, and connect neighbors if there.
	U64 adj_handle = 0;
	for (S32 dir = 0; dir < 8; ++dir)
	{
		F32 adj_x = region_x + (F32)width * gDirAxes[dir][0];
		F32 adj_y = region_y + (F32)width * gDirAxes[dir][1];
		to_region_handle(adj_x, adj_y, &adj_handle);

		LLViewerRegion* neighborp = getRegionFromHandle(adj_handle);
		if (neighborp)
		{
			LL_DEBUGS("World") << "Connecting " << region_x << ":"
							   << region_y << " -> " << adj_x << ":" << adj_y
							   << LL_ENDL;
			regionp->connectNeighbor(neighborp, dir);
		}
	}

	updateWaterObjects();

	return regionp;
}

void LLWorld::removeRegion(const LLHost& host)
{
	LLViewerRegion* regionp = getRegion(host);
	if (!regionp)
	{
		llwarns << "Trying to remove region that does not exist !" << llendl;
		return;
	}

	if (regionp == gAgent.getRegion())
	{
		llwarns << "Disabling agent region: " << regionp->getName()
				<< " - Agent positions: global = "
				<< gAgent.getPositionGlobal() << " / agent = "
				<< gAgent.getPositionAgent() << " - Regions visited: "
				<< gAgent.getRegionsVisited() << "\nRegions dump:";
		for (region_list_t::iterator iter = mRegionList.begin();
			 iter != mRegionList.end(); ++iter)
		{
			LLViewerRegion* reg = *iter;
			llcont << "\nRegion: " << reg->getName() << " " << reg->getHost()
				   << " " << reg->getOriginGlobal();
		}
		llcont << llendl;
		// *TODO: translate
		gAppViewerp->forceDisconnect("You have been disconnected from the region you were in.");
		return;
	}

	F32 x, y;
	from_region_handle(regionp->getHandle(), &x, &y);
	llinfos << "Removing region at " << x << ":" << y << " ("
			<< regionp->getIdentity() << ")" << llendl;

	mRegionList.remove(regionp);
	mActiveRegionList.remove(regionp);
	mCulledRegionList.remove(regionp);
	mVisibleRegionList.remove(regionp);
	mDisabledRegionList.remove(regionp);

	// Remove all objects in this region from the mapped objects list. Note
	// that this is normally automatically done whenever the objects get killed
	// via ~LLViewerRegion(), but better safe than sorry. Also, I moved this
	// call before the region is destroyed (avoids a "use after free" warning
	// by gcc v12+) and changed the method itself (made it simpler and more
	// efficient). HB
	gObjectList.clearAllMapObjectsInRegion(regionp);

	mRegionRemovedSignal(regionp);

	// We can now safely destroy the region.
	delete regionp;

	updateWaterObjects();
}

LLViewerRegion* LLWorld::getRegion(const LLHost& host)
{
	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp->getHost() == host)
		{
			return regionp;
		}
	}
	return NULL;
}

LLViewerRegion* LLWorld::getRegionFromPosAgent(const LLVector3& pos)
{
	return getRegionFromPosGlobal(gAgent.getPosGlobalFromAgent(pos));
}

LLViewerRegion* LLWorld::getRegionFromPosGlobal(const LLVector3d& pos)
{
	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp->pointInRegionGlobal(pos))
		{
			return regionp;
		}
	}
	return NULL;
}

LLVector3d	LLWorld::clipToVisibleRegions(const LLVector3d& start_pos,
										  const LLVector3d& end_pos)
{
	if (positionRegionValidGlobal(end_pos))
	{
		return end_pos;
	}

	LLViewerRegion* regionp = getRegionFromPosGlobal(start_pos);
	if (!regionp)
	{
		return start_pos;
	}

	LLVector3d delta_pos = end_pos - start_pos;
	LLVector3d delta_pos_abs;
	delta_pos_abs.set(delta_pos);
	delta_pos_abs.abs();

	LLVector3 region_coord = regionp->getPosRegionFromGlobal(end_pos);
	F64 clip_factor = 1.0;
	F32 region_width = regionp->getWidth();
	if (region_coord.mV[VX] < 0.f)
	{
		if (region_coord.mV[VY] < region_coord.mV[VX])
		{
			// Clip along y -
			clip_factor = -(region_coord.mV[VY] / delta_pos_abs.mdV[VY]);
		}
		else
		{
			// Clip along x -
			clip_factor = -(region_coord.mV[VX] / delta_pos_abs.mdV[VX]);
		}
	}
	else if (region_coord.mV[VX] > region_width)
	{
		if (region_coord.mV[VY] > region_coord.mV[VX])
		{
			// Clip along y +
			clip_factor = (region_coord.mV[VY] - region_width) /
						  delta_pos_abs.mdV[VY];
		}
		else
		{
			//Clip along x +
			clip_factor = (region_coord.mV[VX] - region_width) /
						  delta_pos_abs.mdV[VX];
		}
	}
	else if (region_coord.mV[VY] < 0.f)
	{
		// Clip along y -
		clip_factor = -(region_coord.mV[VY] / delta_pos_abs.mdV[VY]);
	}
	else if (region_coord.mV[VY] > region_width)
	{
		// Clip along y +
		clip_factor = (region_coord.mV[VY] - region_width) /
					  delta_pos_abs.mdV[VY];
	}

	// Clamp to within region dimensions
	LLVector3d final_region_pos = LLVector3d(region_coord) -
								  delta_pos * clip_factor;
	final_region_pos.mdV[VX] = llclamp(final_region_pos.mdV[VX], 0.0,
									   (F64)(region_width - F_ALMOST_ZERO));
	final_region_pos.mdV[VY] = llclamp(final_region_pos.mdV[VY], 0.0,
									   (F64)(region_width - F_ALMOST_ZERO));
	final_region_pos.mdV[VZ] = llclamp(final_region_pos.mdV[VZ], 0.0,
									   (F64)(MAX_OBJECT_Z - F_ALMOST_ZERO));

	return regionp->getPosGlobalFromRegion(LLVector3(final_region_pos));
}

LLViewerRegion* LLWorld::getRegionFromHandle(const U64& handle)
{
	// Variable region size support
	U32 x, y;
	from_region_handle(handle, &x, &y);

	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
#if 1	// Variable region size support
		U32 tw = (U32)regionp->getWidth();
		U32 tx, ty;
		from_region_handle(regionp->getHandle(), &tx, &ty);
		if (x >= tx && x < tx + tw &&
			y >= ty && y < ty + tw)
#else
		if (regionp->getHandle() == handle)
#endif
		{
			return regionp;
		}
	}
	return NULL;
}

LLViewerRegion* LLWorld::getRegionFromID(const LLUUID& region_id)
{
	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp->getRegionID() == region_id)
		{
			return regionp;
		}
	}
	return NULL;
}

bool LLWorld::positionRegionValidGlobal(const LLVector3d& pos_global)
{
	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp->pointInRegionGlobal(pos_global))
		{
			return true;
		}
	}
	return false;
}

// Allow objects to go up to their radius underground.
F32 LLWorld::getMinAllowedZ(LLViewerObject* object)
{
	F32 land_height = resolveLandHeightGlobal(object->getPositionGlobal());
	F32 radius = 0.5f * object->getScale().length();
	return land_height - radius;
}

F32 LLWorld::getMinAllowedZ(LLViewerObject* object,
							const LLVector3d& global_pos)
{
	F32 land_height = resolveLandHeightGlobal(global_pos);
	F32 radius = 0.5f * object->getScale().length();
	return land_height - radius;
}

LLViewerRegion* LLWorld::resolveRegionGlobal(LLVector3& pos_region,
											 const LLVector3d& pos_global)
{
	LLViewerRegion* regionp = getRegionFromPosGlobal(pos_global);

	if (regionp)
	{
		pos_region = regionp->getPosRegionFromGlobal(pos_global);
		return regionp;
	}

	return NULL;
}

LLViewerRegion* LLWorld::resolveRegionAgent(LLVector3& pos_region,
											const LLVector3& pos_agent)
{
	LLVector3d pos_global = gAgent.getPosGlobalFromAgent(pos_agent);
	LLViewerRegion* regionp = getRegionFromPosGlobal(pos_global);
	if (regionp)
	{
		pos_region = regionp->getPosRegionFromGlobal(pos_global);
		return regionp;
	}
	return NULL;
}

F32 LLWorld::resolveLandHeightAgent(const LLVector3& pos_agent)
{
	LLVector3d pos_global = gAgent.getPosGlobalFromAgent(pos_agent);
	return resolveLandHeightGlobal(pos_global);
}

F32 LLWorld::resolveLandHeightGlobal(const LLVector3d& pos_global)
{
	LLViewerRegion* regionp = getRegionFromPosGlobal(pos_global);
	if (regionp)
	{
		return regionp->getLand().resolveHeightGlobal(pos_global);
	}
	return 0.f;
}

// Takes a line defined by "pt_a" and "pt_b" and determines the closest
// (to pt_a) point where the the line intersects an object or the land
// surface. Stores the results in "intersection" and "intersect_norm" and
// returns a scalar value that represents the normalized distance along the
// line from "pt_a" to "intersection".
//
// Currently assumes pt_a and pt_b only differ in z-direction, but it may
// eventually become more general.
F32 LLWorld::resolveStepHeightGlobal(const LLVOAvatar* avatarp,
									 const LLVector3d& pt_a,
									 const LLVector3d& pt_b,
									 LLVector3d& intersection,
									 LLVector3& intersect_norm,
									 LLViewerObject** vobjp)
{
	// Initialize return value to null
	if (vobjp)
	{
		*vobjp = NULL;
	}

	LLViewerRegion* regionp = getRegionFromPosGlobal(pt_a);
	if (!regionp)
	{
		// We are outside the world
		intersection = 0.5f * (pt_a + pt_b);
		intersect_norm.set(0.f, 0.f, 1.f);
		return 0.5f;
	}

	// Calculate the length of the segment
	F32 segment_len = (F32)((pt_a - pt_b).length());
	if (segment_len == 0.f)
	{
		intersection = pt_a;
		intersect_norm.set(0.f, 0.f, 1.f);
		return segment_len;
	}

	// Get the land height. Note: we assume that the line is parallel to z-axis
	// here
	LLVector3d land_intersection = pt_a;
	land_intersection.mdV[VZ] = regionp->getLand().resolveHeightGlobal(pt_a);
	F32 normalized_land_dist = (F32)(pt_a.mdV[VZ] -
									 land_intersection.mdV[VZ]) / segment_len;
	intersection = land_intersection;
	intersect_norm = resolveLandNormalGlobal(land_intersection);

	if (avatarp && !avatarp->mFootPlane.isExactlyClear())
	{
		LLVector3 foot_plane_normal(avatarp->mFootPlane.mV);
		LLVector3 start_pt =
			avatarp->getRegion()->getPosRegionFromGlobal(pt_a);
		// Added 0.05 meters to compensate for error in foot plane reported by
		// Havok
		F32 norm_dist_from_plane = start_pt * foot_plane_normal -
								   avatarp->mFootPlane.mV[VW] + 0.05f;
		norm_dist_from_plane = llclamp(norm_dist_from_plane / segment_len,
									   0.f, 1.f);
		if (norm_dist_from_plane < normalized_land_dist)
		{
			// collided with object before land
			normalized_land_dist = norm_dist_from_plane;
			intersection = pt_a;
			intersection.mdV[VZ] -= norm_dist_from_plane * segment_len;
			intersect_norm = foot_plane_normal;
		}
		else
		{
			intersection = land_intersection;
			intersect_norm = resolveLandNormalGlobal(land_intersection);
		}
	}

	return normalized_land_dist;
}

// Returns a pointer to the patch at this location
LLSurfacePatch* LLWorld::resolveLandPatchGlobal(const LLVector3d& pos_global)
{
	LLViewerRegion* regionp = getRegionFromPosGlobal(pos_global);
	return regionp ? regionp->getLand().resolvePatchGlobal(pos_global) : NULL;
}

LLVector3 LLWorld::resolveLandNormalGlobal(const LLVector3d& pos_global)
{
	LLViewerRegion* regionp = getRegionFromPosGlobal(pos_global);
	return regionp ? regionp->getLand().resolveNormalGlobal(pos_global)
				   : LLVector3::z_axis;
}

void LLWorld::updateVisibilities()
{
	F32 cur_far_clip = gViewerCamera.getFar();
	gViewerCamera.setFar(mLandFarClip);

	// Go through the culled list and check for visible regions
	for (region_list_t::iterator iter = mCulledRegionList.begin(),
								 end = mCulledRegionList.end();
		 iter != end; )
	{
		region_list_t::iterator curiter = iter++;
		LLViewerRegion* regionp = *curiter;

		LLSpatialPartition* part =
			regionp->getSpatialPartition(LLViewerRegion::PARTITION_TERRAIN);
		// PARTITION_TERRAIN cannot be NULL
		llassert(part);
		LLSpatialGroup* group = (LLSpatialGroup*)part->mOctree->getListener(0);
		const LLVector4a* bounds = group->getBounds();
		if (gViewerCamera.AABBInFrustum(bounds[0], bounds[1]))
		{
			mCulledRegionList.erase(curiter);
			mVisibleRegionList.push_back(regionp);
		}
	}

	// Update all of the visible regions
	for (region_list_t::iterator iter = mVisibleRegionList.begin(),
								 end = mVisibleRegionList.end();
		 iter != end; )
	{
		region_list_t::iterator curiter = iter++;
		LLViewerRegion* regionp = *curiter;
		if (!regionp->getLand().hasZData())
		{
			continue;
		}

		LLSpatialPartition* part =
			regionp->getSpatialPartition(LLViewerRegion::PARTITION_TERRAIN);
		// PARTITION_TERRAIN cannot be NULL
		llassert(part);
		LLSpatialGroup* group = (LLSpatialGroup*)part->mOctree->getListener(0);
		const LLVector4a* bounds = group->getBounds();
		if (gViewerCamera.AABBInFrustum(bounds[0], bounds[1]))
		{
			regionp->calculateCameraDistance();
			regionp->getLand().updatePatchVisibilities();
		}
		else
		{
			mVisibleRegionList.erase(curiter);
			mCulledRegionList.push_back(regionp);
		}
	}

	// Sort visible regions
	mVisibleRegionList.sort(LLViewerRegion::CompareDistance());

	gViewerCamera.setFar(cur_far_clip);
}

void LLWorld::updateRegions(F32 max_update_time)
{
	LLTimer update_timer;

	if (gViewerCamera.isChanged())
	{
		LLViewerRegion::sLastCameraUpdated =
			LLViewerOctreeEntryData::getCurrentFrame() + 1;
	}
	LLViewerRegion::calcNewObjectCreationThrottle();
	static LLCachedControl<U32> region_update_fraction(gSavedSettings,
													   "RegionUpdateFraction");
	F32 fraction = (F32)llclamp((S32)region_update_fraction, 2, 20);

	if (LLViewerRegion::isNewObjectCreationThrottleDisabled())
	{
		// Loosen the time throttle.
		max_update_time = 10.f * max_update_time;
	}
	F32 max_time = llmin((F32)(max_update_time -
							   update_timer.getElapsedTimeF32()),
						 max_update_time / fraction);

	// Always perform an update on the agent region first.
	LLViewerRegion* self_regionp = gAgent.getRegion();
	if (self_regionp)
	{
		self_regionp->idleUpdate(max_time);
	}

	// Sort regions by its mLastUpdate: smaller mLastUpdate first to make sure
	// every region has a chance to get updated.
	LLViewerRegion::prio_list_t region_list;
	for (region_list_t::iterator iter = mActiveRegionList.begin(),
								 end = mActiveRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp != self_regionp)
		{
			region_list.insert(regionp);
		}
	}

	// Perform idle time updates for the regions (and associated surfaces)
	for (LLViewerRegion::prio_list_t::iterator iter = region_list.begin(),
											   end = region_list.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (max_time > 0.f)
		{
			max_time = llmin((F32)(max_update_time -
								   update_timer.getElapsedTimeF32()),
							 max_update_time / fraction);
		}
		if (max_time > 0.f)
		{
			regionp->idleUpdate(max_time);
		}
		else
		{
			// Perform some necessary but very light updates.
			regionp->lightIdleUpdate();
		}
	}
}

void LLWorld::clearAllVisibleObjects()
{
	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		// Clear all cached visible objects.
		(*iter)->clearCachedVisibleObjects();
	}
	clearHoleWaterObjects();
	clearEdgeWaterObjects();
}

void LLWorld::updateClouds(F32 dt)
{
	if (LLPipeline::sFreezeTime || !LLCloudLayer::needClassicClouds())
	{
		// Do not move clouds in snapshot mode and do not bother updating them
		// when not needed...
		return;
	}
	if (mActiveRegionList.size())
	{
		// Update all the cloud puff positions, and timer based stuff
		// such as death decay
		for (region_list_t::iterator iter = mActiveRegionList.begin(),
									 end = mActiveRegionList.end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			regionp->mCloudLayer.updatePuffs(dt);
		}

		// Reshuffle who owns which puffs
		for (region_list_t::iterator iter = mActiveRegionList.begin(),
									 end = mActiveRegionList.end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			regionp->mCloudLayer.updatePuffOwnership();
		}

		// Add new puffs
		for (region_list_t::iterator iter = mActiveRegionList.begin(),
									 end = mActiveRegionList.end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			regionp->mCloudLayer.updatePuffCount();
		}
	}
}

void LLWorld::killClouds()
{
	for (region_list_t::iterator iter = mActiveRegionList.begin(),
								 end = mActiveRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		regionp->mCloudLayer.reset();
	}
}

LLCloudGroup* LLWorld::findCloudGroup(const LLCloudPuff& puff)
{
	if (mActiveRegionList.size())
	{
		// Update all the cloud puff positions, and timer based stuff
		// such as death decay
		for (region_list_t::iterator iter = mActiveRegionList.begin(),
									 end = mActiveRegionList.end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			LLCloudGroup* groupp = regionp->mCloudLayer.findCloudGroup(puff);
			if (groupp)
			{
				return groupp;
			}
		}
	}
	return NULL;
}

void LLWorld::renderPropertyLines()
{
	static LLCachedControl<bool> show_lines(gSavedSettings,
											"ShowPropertyLines");
	if (!show_lines)
	{
		return;
	}
	for (region_list_t::iterator iter = mVisibleRegionList.begin(),
								 end = mVisibleRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		regionp->renderPropertyLines();
	}
}

void LLWorld::updateNetStats()
{
	F64 bits = 0.0;
	for (region_list_t::iterator iter = mActiveRegionList.begin(),
								 end = mActiveRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		regionp->updateNetStats();
		bits += (F64)regionp->mBitStat.getCurrent();
	}
	U64 curl_bytes = LLCore::HttpLibcurl::getDownloadedBytes();
	bits += 8.0 * (F64)(curl_bytes - mLastCurlBytes);
	mLastCurlBytes = curl_bytes;

	LLMessageSystem* msg = gMessageSystemp;
	S32 packets_in = msg->mPacketsIn - mLastPacketsIn;
	S32 packets_out = msg->mPacketsOut - mLastPacketsOut;
	S32 packets_lost = msg->mDroppedPackets - mLastPacketsLost;

	S32 actual_in_bits = msg->mPacketRing.getAndResetActualInBits();
	S32 actual_out_bits = msg->mPacketRing.getAndResetActualOutBits();
	gViewerStats.mActualInKBitStat.addValue(actual_in_bits * 0.001f);
	gViewerStats.mActualOutKBitStat.addValue(actual_out_bits * 0.001f);
	gViewerStats.mKBitStat.addValue((F32)(bits * 0.001));
	gViewerStats.mPacketsInStat.addValue(packets_in);
	gViewerStats.mPacketsOutStat.addValue(packets_out);
	gViewerStats.mPacketsLostStat.addValue(msg->mDroppedPackets);
	F32 packets_pct = packets_in ? (F32)(100 * packets_lost) / (F32)packets_in
								 : 0.f;
	gViewerStats.mPacketsLostPercentStat.addValue(packets_pct);

	mLastPacketsIn = msg->mPacketsIn;
	mLastPacketsOut = msg->mPacketsOut;
	mLastPacketsLost = msg->mDroppedPackets;
}

void LLWorld::printPacketsLost()
{
	llinfos << "Simulators:" << llendl;
	llinfos << "----------" << llendl;

	LLCircuitData* cdp = NULL;
	for (region_list_t::iterator iter = mActiveRegionList.begin();
		 iter != mActiveRegionList.end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		cdp = gMessageSystemp->mCircuitInfo.findCircuit(regionp->getHost());
		if (cdp)
		{
			LLVector3d range = regionp->getCenterGlobal() -
							   gAgent.getPositionGlobal();

			llinfos << regionp->getHost() << ", range: " << range.length()
					<< " packets lost: " << cdp->getPacketsLost() << llendl;
		}
	}

	llinfos << "----------" << llendl;
}

void LLWorld::processCoarseUpdate(LLMessageSystem* msg, void**)
{
	LLViewerRegion* region = gWorld.getRegion(msg->getSender());
	if (region)
	{
		region->updateCoarseLocations(msg);
	}
}

F32 LLWorld::getLandFarClip() const
{
	return mLandFarClip;
}

void LLWorld::setLandFarClip(F32 far_clip)
{
	// Variable region size support
	LLViewerRegion* region = gAgent.getRegion();
	F32 rwidth = (S32)(region ? region->getWidth() : REGION_WIDTH_METERS);

	const S32 n1 = (llceil(mLandFarClip) - 1) / rwidth;
	const S32 n2 = (llceil(far_clip) - 1) / rwidth;
	bool need_water_objects_update = n1 != n2;

	mLandFarClip = far_clip;

	if (need_water_objects_update)
	{
		updateWaterObjects();
	}
}

// Some region that we are connected to, but not the one we are in, gave us
// a (possibly) new water height. Update it in our local copy.
void LLWorld::waterHeightRegionInfo(std::string const& sim_name,
									F32 water_height)
{
	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		if ((*iter)->getName() == sim_name)
		{
			(*iter)->setWaterHeight(water_height);
			break;
		}
	}
}

void LLWorld::clearHoleWaterObjects()
{
	for (water_obj_list_t::iterator iter = mHoleWaterObjects.begin(),
									end = mHoleWaterObjects.end();
		 iter != end; ++iter)
	{
		LLVOWater* waterp = iter->get();
		if (waterp)	// Paranoia
		{
			gObjectList.killObject(waterp);
		}
	}
	mHoleWaterObjects.clear();
}

void LLWorld::clearEdgeWaterObjects()
{
	for (S32 i = 0; i < EDGE_WATER_OBJECTS_COUNT; ++i)
	{
		LLVOWater* waterp = mEdgeWaterObjects[i];
		if (waterp)	// Paranoia
		{
			gObjectList.killObject(waterp);
			mEdgeWaterObjects[i] = NULL;
		}
	}
}

void LLWorld::updateWaterObjects()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp || mRegionList.empty())
	{
		return;
	}

	// First, determine the min and max "box" of water objects

	// Variable region size support
	F32 rwidth = (S32)regionp->getWidth();

	// We only want to fill in water for stuff that is near us, say, within 256
	// or 512m
	S32 range = gViewerCamera.getFar() > 256.f ? 512 : 256;

	U32 region_x, region_y;
	from_region_handle(regionp->getHandle(), &region_x, &region_y);

	S32 min_x = (S32)region_x - range;
	S32 min_y = (S32)region_y - range;
	S32 max_x = (S32)region_x + range;
	S32 max_y = (S32)region_y + range;

	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regp = *iter;
		if (regp)	// Paranoia
		{
			LLVOWater* waterp = regp->getLand().getWaterObj();
			if (waterp)
			{
				gObjectList.updateActive(waterp);
			}
		}
	}

	clearHoleWaterObjects();

	// Now, get a list of the holes
	F32 water_height = regionp->getWaterHeight() + 256.f;
	for (S32 x = min_x; x <= max_x; x += rwidth)
	{
		for (S32 y = min_y; y <= max_y; y += rwidth)
		{
			U64 region_handle = to_region_handle(x, y);
			if (!getRegionFromHandle(region_handle))
			{
				LLViewerObject* vobj =
					gObjectList.createObjectViewer(LLViewerObject::LL_VO_WATER,
													  regionp);
				LLVOWater* waterp = (LLVOWater*)vobj;
				waterp->setUseTexture(false);
				waterp->setPositionGlobal(LLVector3d(x + rwidth / 2,
													 y + rwidth / 2,
													 water_height));
				waterp->setScale(LLVector3((F32)rwidth, (F32)rwidth, 512.f));
				gPipeline.createObject(waterp);
				mHoleWaterObjects.push_back(waterp);
			}
		}
	}

	// Update edge water objects
	S32 wx = (max_x - min_x) + rwidth;
	S32 wy = (max_y - min_y) + rwidth;
	S32 center_x = min_x + (wx >> 1);
	S32 center_y = min_y + (wy >> 1);

	S32 add_boundary[4] =
	{
		512 - (S32)(max_x - region_x),
		512 - (S32)(max_y - region_y),
		512 - (S32)(region_x - min_x),
		512 - (S32)(region_y - min_y)
	};

	S32 dim[2];
	for (S32 dir = 0; dir < 8; ++dir)
	{
		switch (gDirAxes[dir][0])
		{
			case -1:
				dim[0] = add_boundary[2];
				break;

			case 0:
				dim[0] = wx;
				break;

			default:
				dim[0] = add_boundary[0];
		}
		switch (gDirAxes[dir][1])
		{
			case -1:
				dim[1] = add_boundary[3];
				break;

			case 0:
				dim[1] = wy;
				break;

			default:
				dim[1] = add_boundary[1];
		}

		// Resize and reshape the water objects
		const S32 water_center_x = center_x +
								   ll_round((wx + dim[0]) * 0.5f *
										    gDirAxes[dir][0]);
		const S32 water_center_y = center_y +
								   ll_round((wy + dim[1]) * 0.5f *
										    gDirAxes[dir][1]);

		LLVOWater* waterp = mEdgeWaterObjects[dir];
		if (!waterp || waterp->isDead())
		{
			// The edge water objects can be dead because they are attached to
			// the region that the agent was in when they were originally
			// created.
			LLViewerObject* vobj =
				gObjectList.createObjectViewer(LLViewerObject::LL_VO_WATER,
											   regionp);
			mEdgeWaterObjects[dir] = (LLVOWater*)vobj;
			waterp = mEdgeWaterObjects[dir];
			waterp->setUseTexture(false);
			waterp->setIsEdgePatch(true);
			gPipeline.createObject(waterp);
		}

		waterp->setRegion(regionp);
		LLVector3d water_pos(water_center_x, water_center_y, water_height);
		LLVector3 water_scale((F32)dim[0], (F32)dim[1], 512.f);

		// Stretch out to horizon
		water_scale.mV[0] += fabsf(2048.f * gDirAxes[dir][0]);
		water_scale.mV[1] += fabsf(2048.f * gDirAxes[dir][1]);

		water_pos.mdV[0] += 1024.f * gDirAxes[dir][0];
		water_pos.mdV[1] += 1024.f * gDirAxes[dir][1];

		waterp->setPositionGlobal(water_pos);
		waterp->setScale(water_scale);

		gObjectList.updateActive(waterp);
	}
}

void LLWorld::precullWaterObjects(LLCamera& camera, LLCullResult* cullp)
{
	if (mRegionList.empty())
	{
		return;
	}

	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		LLVOWater* waterp = regionp->getLand().getWaterObj();
		if (waterp && !waterp->isDead() && waterp->mDrawable)
		{
			waterp->mDrawable->setVisible(camera);
			cullp->pushDrawable(waterp->mDrawable);
		}
	}

	for (water_obj_list_t::iterator iter = mHoleWaterObjects.begin(),
									end = mHoleWaterObjects.end();
		 iter != end; ++iter)
	{
		LLVOWater* waterp = iter->get();
		if (waterp && !waterp->isDead() && waterp->mDrawable)
		{
			waterp->mDrawable->setVisible(camera);
			cullp->pushDrawable(waterp->mDrawable);
		}
	}

	for (S32 i = 0; i < EDGE_WATER_OBJECTS_COUNT; ++i)
	{
		LLVOWater* waterp = mEdgeWaterObjects[i].get();
		if (waterp && !waterp->isDead() && waterp->mDrawable)
		{
			waterp->mDrawable->setVisible(camera);
			cullp->pushDrawable(waterp->mDrawable);
		}
	}
}

void LLWorld::shiftRegions(const LLVector3& offset)
{
	for (region_list_t::const_iterator i = getRegionList().begin(),
									   end = getRegionList().end();
		 i != end; ++i)
	{
		LLViewerRegion* regionp = *i;
		regionp->updateRenderMatrix();
	}

	gViewerPartSim.shift(offset);
}

void LLWorld::reloadAllSurfacePatches()
{
	llinfos << "Force-reloading all surface patches to rebuild failed textures."
			<< llendl;
	// This inserts a delay before a new automatic reload hack would get
	// triggered... HB
	LLSurfacePatch::allPatchesReloaded();

	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		LLVLComposition* compp = regionp->getComposition();
		if (compp)
		{
			compp->forceRebuild();
			regionp->getLand().dirtyAllPatches();
		}
	}
}

void LLWorld::requestCacheMisses()
{
	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		regionp->requestCacheMisses();
	}
}

void LLWorld::getInfo(LLSD& info)
{
	LLSD region_info;
	for (region_list_t::iterator iter = mRegionList.begin(),
								 end = mRegionList.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		regionp->getInfo(region_info);
		info["World"].append(region_info);
	}
}

void LLWorld::disconnectRegions()
{
	LLMessageSystem* msg = gMessageSystemp;
	for (region_list_t::iterator iter = mRegionList.begin();
		 iter != mRegionList.end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp == gAgent.getRegion())
		{
			// Skip the main agent
			continue;
		}

		llinfos << "Sending AgentQuitCopy to: " << regionp->getHost()
				<< llendl;
		msg->newMessageFast(_PREHASH_AgentQuitCopy);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_FuseBlock);
		msg->addU32Fast(_PREHASH_ViewerCircuitCode, msg->mOurCircuitCode);
		msg->sendMessage(regionp->getHost());
	}
}

// Enables the appropriate circuit for this simulator and adds its parameters
//static
void LLWorld::processEnableSimulator(LLMessageSystem* msg, void**)
{
	U64 handle;
	msg->getU64Fast(_PREHASH_SimulatorInfo, _PREHASH_Handle, handle);
	U32 ip_u32;
	msg->getIPAddrFast(_PREHASH_SimulatorInfo, _PREHASH_IP, ip_u32);
	U16 port;
	msg->getIPPortFast(_PREHASH_SimulatorInfo, _PREHASH_Port, port);

	// Which simulator should we modify ?
	LLHost sim(ip_u32, port);

	// Viewer trusts the simulator.
	msg->enableCircuit(sim, true);

	// Variable region size support
	U32 region_size_x = REGION_WIDTH_METERS;
	U32 region_size_y = REGION_WIDTH_METERS;
	if (!gIsInSecondLife)
	{
		msg->getU32Fast(_PREHASH_SimulatorInfo, _PREHASH_RegionSizeX,
						region_size_x);
		if (region_size_x == 0)
		{
			region_size_x = REGION_WIDTH_METERS;
		}
		msg->getU32Fast(_PREHASH_SimulatorInfo, _PREHASH_RegionSizeY,
						region_size_y);
		if (region_size_y == 0)
		{
			region_size_y = region_size_x;
		}
	}
	if (region_size_x != region_size_y)
	{
		llwarns << "RECTANGULAR REGIONS NOT SUPPORTED: expect a crash !"
				<< llendl;
		region_size_x = llmax(region_size_x, region_size_y);
	}

	gWorld.addRegion(handle, sim, region_size_x);

	// Give the simulator a message it can use to get ip and port
	U32 circuit_code = msg->getOurCircuitCode();
	static U32 last_ip_u32 = 0;
	static U32 last_circuit_code = 0;
	if (ip_u32 != last_ip_u32 || circuit_code != last_circuit_code)
	{
		last_ip_u32 = ip_u32;
		last_circuit_code = circuit_code;
		llinfos << "Enabling simulator " << sim << " (region handle " << handle
				<< ") with code " << circuit_code << llendl;
	}
	msg->newMessageFast(_PREHASH_UseCircuitCode);
	msg->nextBlockFast(_PREHASH_CircuitCode);
	msg->addU32Fast(_PREHASH_Code, circuit_code);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_ID, gAgentID);
	msg->sendReliable(sim);
}

class LLEstablishAgentCommunication final : public LLHTTPNode
{
protected:
	LOG_CLASS(LLEstablishAgentCommunication);

public:
 	void describe(Description& desc) const override
	{
		desc.shortInfo("seed capability info for a region");
		desc.postAPI();
		desc.input("{ seed-capability: ..., sim-ip: ..., sim-port }");
		desc.source(__FILE__, __LINE__);
	}

	void post(ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		if (LLApp::isExiting() || gDisconnected)
		{
			return;
		}

		if (!input["body"].has("agent-id") ||
			!input["body"].has("sim-ip-and-port") ||
			!input["body"].has("seed-capability"))
		{
			llwarns << "Missing parameters" << llendl;
            return;
		}

		LLHost sim(input["body"]["sim-ip-and-port"].asString());
		if (sim.isInvalid())
		{
			llwarns << "Got a response with an invalid host" << llendl;
			return;			
		}

		LLViewerRegion* regionp = gWorld.getRegion(sim);
		if (!regionp)
		{
			llwarns << "Got a response for an unknown region: " << sim
					<< llendl;
			return;
		}
		regionp->setSeedCapability(input["body"]["seed-capability"]);
	}
};

LLHTTPRegistration<LLEstablishAgentCommunication>
	gHTTPRegistrationEstablishAgentCommunication("/message/EstablishAgentCommunication");

// Disable the circuit to this simulator. Called in response to a
// "DisableSimulator" message. However, if the last sim disabling happened
// less than 1 second ago, queue the region for later disabling instead, so to
// avoid huge 'hiccups' when saving the corresponding objects cache to the hard
// drive (this is especially true with the new objects caching scheme, the full
// region objects getting cached, and that is a lot of data to save, which
// takes quite some time). HB
//static
void LLWorld::processDisableSimulator(LLMessageSystem* msg, void**)
{
	static LLCachedControl<bool> staged_sim_disabling(gSavedSettings,
													  "StagedSimDisabling");
	static LLCachedControl<U32> disabling_delay(gSavedSettings,
												"StagedSimDisablingDelay");
	F32 delay = disabling_delay > 0 ? (F32)disabling_delay : 1.f;

	if (!msg) return;

	LLHost host = msg->getSender();
	LLViewerRegion* regionp = gWorld.getRegion(host);
	if (!regionp || !staged_sim_disabling ||
		(gWorld.mDisabledRegionList.empty() &&
		 gFrameTimeSeconds - gWorld.mLastRegionDisabling > delay))
	{
		llinfos << "Disabling simulator " << host << llendl;
		gWorld.removeRegion(host);
		msg->disableCircuit(host);
		gWorld.mLastRegionDisabling = gFrameTimeSeconds;
	}
	else
	{
		llinfos << "Queuing simulator " << host << " for delayed removal."
				<< llendl;
		gWorld.mDisabledRegionList.push_back(regionp);
	}
}

//static
void LLWorld::idleDisableQueuedSim()
{
	static LLCachedControl<U32> disabling_delay(gSavedSettings,
												"StagedSimDisablingDelay");
	F32 delay = disabling_delay > 0 ? (F32)disabling_delay : 1.f;

	if (!gWorld.mDisabledRegionList.empty() &&
		gFrameTimeSeconds - gWorld.mLastRegionDisabling > delay)
	{
		LLViewerRegion* regionp = gWorld.mDisabledRegionList.front();
		LLHost host = regionp->getHost();
		llinfos << "Disabling simulator " << host << llendl;
		// Note: removeRegion() also removes the region from
		// mDisabledRegionList
		gWorld.removeRegion(host);
		if (gMessageSystemp)
		{
			gMessageSystemp->disableCircuit(host);
		}
		gWorld.mLastRegionDisabling = gFrameTimeSeconds;
	}
}

//static
void LLWorld::processRegionHandshake(LLMessageSystem* msg, void**)
{
	const LLHost& host = msg->getSender();
	LLViewerRegion* regionp = gWorld.getRegion(host);
	if (regionp)
	{
		regionp->unpackRegionHandshake();
	}
	else
	{
		llwarns << "Got region handshake for unknown region " << host
				<< llendl;
	}
}

//static
void LLWorld::sendAgentPause()
{
	// Note: used to check for LLWorld initialization before it became a
	// global. Rather than just remove this check I am changing it to assure
	// that the message system has been initialized. -MG
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg)
	{
		return;
	}

	msg->newMessageFast(_PREHASH_AgentPause);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addU32Fast(_PREHASH_SerialNum, ++gAgentPauseSerialNum);

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		msg->sendReliable(regionp->getHost());
	}

	gObjectList.mWasPaused = true;
}

//static
void LLWorld::sendAgentResume()
{
	// Note: used to check for LLWorld initialization before it became a
	// singleton. Rather than just remove this check I'm changing it to assure
	// that the message system has been initialized. -MG
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg)
	{
		return;
	}

	msg->newMessageFast(_PREHASH_AgentResume);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addU32Fast(_PREHASH_SerialNum, ++gAgentPauseSerialNum);

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		msg->sendReliable(regionp->getHost());
	}

	// Reset the FPS counter to avoid an invalid fps
	gViewerStats.mFPSStat.start();
}

void LLWorld::getAvatars(uuid_vec_t& avatar_ids,
						 std::vector<LLVector3d>* positions,
						 std::vector<LLColor4>* colors,
						 const LLVector3d& relative_to,
						 F32 radius) const
{
	avatar_ids.clear();

	S32 count = LLCharacter::sInstances.size();

	if (positions)
	{
		positions->clear();
		positions->reserve(count);
	}

	if (colors)
	{
		colors->clear();
		colors->reserve(count);
	}

	F32 radius_squared = radius * radius;

	// Get the list of avatars from the character list first, so distances are
	// correct when agent is above 1020m and other avatars are nearby
	for (S32 i = 0; i < count; ++i)
	{
		LLVOAvatar* avatarp = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (avatarp && !avatarp->isDead() && !avatarp->isSelf() &&
			!avatarp->mIsDummy && !avatarp->isOrphaned())
		{
			const LLUUID& id = avatarp->getID();
			if (id.isNull()) continue;	// Paranoia

			LLVector3d pos_global = avatarp->getPositionGlobal();
			if (dist_vec_squared(pos_global, relative_to) <= radius_squared)
			{
				avatar_ids.emplace_back(id);
				if (positions)
				{
					positions->emplace_back(pos_global);
				}
				if (colors)
				{
					colors->emplace_back(avatarp->getMinimapColor());
				}
			}
		}
	}

	// Region avatars added for situations where radius is greater than
	// RenderFarClip
	LLVector3d pos_global;
	for (LLWorld::region_list_t::const_iterator it = mActiveRegionList.begin(),
												end = mActiveRegionList.end();
		it != end; ++it)
	{
		LLViewerRegion* regionp = *it;
		if (!regionp) continue;	// Paranoia

		// Variable region size support
		F64 scale_factor = (F64)regionp->getWidth() / REGION_WIDTH_METERS;

		const LLVector3d& origin_global = regionp->getOriginGlobal();
		count = regionp->mMapAvatars.size();

		for (S32 i = 0; i < count; ++i)
		{
			const LLUUID& id = regionp->mMapAvatarIDs[i];
			if (id.isNull()) continue;	// This can happen !

			// Unpack the 32 bits encoded position and make it global
			U32 compact_local = regionp->mMapAvatars[i];
			pos_global = origin_global;
			pos_global.mdV[VZ] += (F64)((compact_local & 0xFFU) * 4);
			compact_local >>= 8;
			pos_global.mdV[VY] += (F64)(compact_local & 0xFFU) * scale_factor;
			compact_local >>= 8;
			pos_global.mdV[VX] += (F64)(compact_local & 0xFFU) * scale_factor;

			if (dist_vec_squared(pos_global, relative_to) > radius_squared)
			{
				continue;
			}
			bool not_listed = true;
			for (S32 j = 0, count = avatar_ids.size(); j < count; ++j)
			{
				if (id == avatar_ids[j])
				{
					not_listed = false;
					break;
				}
			}
			if (not_listed)
			{
				// If this avatar does not already exist in the list, add it
				avatar_ids.emplace_back(id);
				if (positions)
				{
					positions->emplace_back(pos_global);
				}
				if (colors)
				{
					colors->emplace_back(LLVOAvatar::getMinimapColor(id));
				}
			}
		}
	}
}

bool LLWorld::isRegionListed(const LLViewerRegion* region) const
{
	region_list_t::const_iterator it = find(mRegionList.begin(),
											mRegionList.end(), region);
	return it != mRegionList.end();
}

boost::signals2::connection LLWorld::setRegionRemovedCallback(const region_remove_signal_t::slot_type& cb)
{
	return mRegionRemovedSignal.connect(cb);
}
