/**
 * @file llworldmap.cpp
 * @brief Underlying data representation for map of the world
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
 * Copyright (c) 2009-2021, Henri Beauchamp.
 * Changes by Henri Beauchamp:
 *  - Cleaned up/reorganized the code (which was spread over three classes).
 *  - Backported web map tiles support from v2/3 viewers while keeping the old
 *    terrain-only map support.
 *  - Adapted the code for OpenSim variable region size support.
 *  - Allowed to keep both objects and terrain map tiles in memory (avoids
 *    seeing the map tiles fully reloaded at each world map tab change).
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

#include "llworldmap.h"

#include "llregionflags.h"
#include "llregionhandle.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfloaterworldmap.h"
#include "llgridmanager.h"
#include "lltracker.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llworld.h"

LLWorldMap gWorldMap;

bool LLWorldMap::sGotMapURL = false;
bool LLWorldMap::sMapURLSetOnLogin = false;
std::string LLWorldMap::sMapURL;
constexpr F32 REQUEST_ITEMS_TIMER = 600.f; // 10 minutes

//-----------------------------------------------------------------------------
// LLItemInfo class
//-----------------------------------------------------------------------------

LLItemInfo::LLItemInfo(F32 global_x, F32 global_y, const std::string& name,
					   const LLUUID& id, S32 extra, S32 extra2)
:	mName(name),
	mPosGlobal(global_x, global_y, 40.0),
	mID(id),
	mSelected(false),
	mExtra(extra),
	mExtra2(extra2)
{
	mRegionHandle = to_region_handle(mPosGlobal);
}

LLSimInfo::LLSimInfo()
:	mHandle(0),
	mAgentsUpdateTime(0.0),
	mAgentsCount(-1),
	mShowAgentLocations(false),
	mAccess(0x0),
	mRegionFlags(0x0),
	mAlpha(-1.f),
	// Variable region size support
    mSizeX(REGION_WIDTH_METERS),
	mSizeY(REGION_WIDTH_METERS)
{
}

LLVector3d LLSimInfo::getGlobalOrigin() const
{
	return from_region_handle(mHandle);
}

LLVector3d LLSimInfo::getGlobalPos(LLVector3 local_pos) const
{
	LLVector3d pos = from_region_handle(mHandle);
	pos.mdV[VX] += local_pos.mV[VX];
	pos.mdV[VY] += local_pos.mV[VY];
	pos.mdV[VZ] += local_pos.mV[VZ];
	return pos;
}

//-----------------------------------------------------------------------------
// LLWorldMap class
//-----------------------------------------------------------------------------

LLWorldMap::LLWorldMap()
:	mIsTrackingUnknownLocation(false),
	mInvalidLocation(false),
	mIsTrackingDoubleClick(false),
	mIsTrackingCommit(false),
	mUnknownLocation(0, 0, 0),
	mRequestLandForSale(true),
	mCurrentMap(0),
	mMinX(U32_MAX),
	mMaxX(U32_MIN),
	mMinY(U32_MAX),
	mMaxY(U32_MIN),
	mSLURLRegionName(),
	mSLURLRegionHandle(0),
	mSLURL(),
	mSLURLCallback(NULL),
	mSLURLTeleport(false)
{
	clearSimFlags();
	for (S32 map = 0; map < MAP_SIM_IMAGE_TYPES; ++map)
	{
		mMapLoaded[map] = false;
	}
}

LLWorldMap::~LLWorldMap()
{
	reset();
 	mAgentLocationsMap.clear();
}

void LLWorldMap::reset()
{
	for (sim_info_map_t::iterator it = mSimInfoMap.begin(),
								  end = mSimInfoMap.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mSimInfoMap.clear();

	eraseItems(true);

	for (S32 i = 0; i < MAP_SIM_IMAGE_TYPES; ++i)
	{
		mMapLoaded[i] = false;
		mMapLayers[i].clear();
		mMapBlockMap[i].clear();
	}

	mMinX = U32_MAX;
	mMaxX = U32_MIN;

	mMinY = U32_MAX;
	mMaxY = U32_MIN;
}

void LLWorldMap::eraseItems(bool force)
{
	if (force || mRequestTimer.getElapsedTimeF32() > REQUEST_ITEMS_TIMER)
	{
		mRequestTimer.reset();

		mTelehubs.clear();
		mInfohubs.clear();
		mPGEvents.clear();
		mMatureEvents.clear();
		mAdultEvents.clear();
		mLandForSale.clear();
	}

#if 0	// Persistent data
 	mAgentLocationsMap.clear();
#endif
}

void LLWorldMap::clearImageRefs(S32 layer)
{
	for (sim_info_map_t::iterator it = mSimInfoMap.begin(),
								  end = mSimInfoMap.end();
		 it != end; ++it)
	{
		LLSimInfo* info = it->second;
		if ((layer <= 0 || layer > 1) && info->mCurrentImage[0])
		{
			info->mCurrentImage[0]->setBoostLevel(0);
			info->mCurrentImage[0] = NULL;
		}
		if ((layer == 1 || layer < 0) && info->mCurrentImage[1])
		{
			info->mCurrentImage[1]->setBoostLevel(0);
			info->mCurrentImage[1] = NULL;
		}
		if (info->mOverlayImage)
		{
			info->mOverlayImage->setBoostLevel(0);
			info->mOverlayImage = NULL;
		}
	}
}

// Does not clear the already-loaded sim infos, just re-requests them
void LLWorldMap::clearSimFlags()
{
	for (S32 map = 0; map < MAP_SIM_IMAGE_TYPES; ++map)
	{
		mMapBlockMap[map].clear();
	}
}

LLSimInfo* LLWorldMap::simInfoFromPosGlobal(const LLVector3d& pos_global)
{
	return simInfoFromHandle(to_region_handle(pos_global));
}

LLSimInfo* LLWorldMap::simInfoFromHandle(U64 handle)
{
	sim_info_map_t::iterator it = mSimInfoMap.find(handle);
	sim_info_map_t::iterator end = mSimInfoMap.end();
	if (it != end)
	{
		LLSimInfo* sim_info = it->second;
		if (sim_info)
		{
			return sim_info;
		}
	}

	// Variable region size support
	U32 x = 0;
	U32 y = 0;
	from_region_handle(handle, &x, &y);
	for (it = mSimInfoMap.begin(); it != end; ++it)
	{
		U32 tx, ty;
		from_region_handle(it->first, &tx, &ty);

		LLSimInfo* info = it->second;
		if (info && x >= tx && x < tx + info->getSizeX() &&
			y >= ty && y < ty + info->getSizeY())
		{
			return info;
		}
	}

	return NULL;
}

LLSimInfo* LLWorldMap::simInfoFromName(std::string sim_name)
{
	if (sim_name.empty())
	{
		return NULL;
	}
	
	LLStringUtil::toUpper(sim_name);
	std::string sim_name2;
	for (sim_info_map_t::iterator it = mSimInfoMap.begin(),
								  end = mSimInfoMap.end();
		 it != end; ++it)
	{
		LLSimInfo* sim_info = it->second;
		if (!sim_info) continue;

		sim_name2 = sim_info->mName;
		LLStringUtil::toUpper(sim_name2);
		if (LLStringOps::collate(sim_name.c_str(), sim_name2.c_str()) == 0)
		{
			return sim_info;
		}
	}

	return NULL;
}

bool LLWorldMap::simNameFromPosGlobal(const LLVector3d& pos_global,
									  std::string& sim_name)
{
	U64 handle = to_region_handle(pos_global);
	sim_info_map_t::iterator it = mSimInfoMap.find(handle);
	if (it != mSimInfoMap.end())
	{
		LLSimInfo* info = it->second;
		sim_name = info->mName;
		return true;
	}

	sim_name = "(unknown region)";
	return false;
}

void LLWorldMap::setCurrentLayer(U32 layer, bool request_layer)
{
	if (layer > MAP_SIM_TERRAIN)
	{
		llwarns << "Bad layer number: " << layer << llendl;
		return;
	}

	if (mCurrentMap != layer)
	{
		mCurrentMap = layer;
#if 0	// Keep already loaded tiles ! HB
		clearImageRefs(layer);
#endif
		clearSimFlags();
	}

	if (!mMapLoaded[layer] || request_layer)
	{
		sendMapLayerRequest();
	}

	if (mTelehubs.empty() || mInfohubs.empty())
	{
		// Request for telehubs
		sendItemRequest(MAP_ITEM_TELEHUB);
	}

	if (mPGEvents.empty())
	{
		// Request for events
		sendItemRequest(MAP_ITEM_PG_EVENT);
	}

	if (mMatureEvents.empty())
	{
		// Request for events (mature)
		sendItemRequest(MAP_ITEM_MATURE_EVENT);
	}

	if (mAdultEvents.empty())
	{
		// Request for events (adult)
		sendItemRequest(MAP_ITEM_ADULT_EVENT);
	}

	if (mLandForSale.empty())
	{
		// Request for Land For Sale
		sendItemRequest(MAP_ITEM_LAND_FOR_SALE);
	}

	if (mLandForSaleAdult.empty())
	{
		// Request for Land For Sale
		sendItemRequest(MAP_ITEM_LAND_FOR_SALE_ADULT);
	}
}

void LLWorldMap::forceUpdateRegion(U64 handle)
{
	sim_info_map_t::iterator it = mSimInfoMap.find(handle);
	if (it != mSimInfoMap.end())
	{
		LLSimInfo* info = it->second;
		if (info->mCurrentImage[mCurrentMap])
		{
			info->mCurrentImage[mCurrentMap]->setBoostLevel(0);
			info->mCurrentImage[mCurrentMap] = NULL;
		}
		if (info->mOverlayImage)
		{
			info->mOverlayImage->setBoostLevel(0);
			info->mOverlayImage = NULL;
		}
		delete info;
		mSimInfoMap.erase(it);
	}

	U32 x, y;
	grid_from_region_handle(handle, &x, &y);
	updateRegions(x, y, x, y, true);
}

// Loads all regions in a given rectangle (in region grid coordinates, i.e.
// world / 256 meters). Returns the number of requested map blocks.
U32 LLWorldMap::updateRegions(S32 x0, S32 y0, S32 x1, S32 y1, bool force_upd)
{
	U32 blocks_requested = 0;
	// Convert those boundaries to the corresponding (MAP_BLOCK_SIZE x
	// MAP_BLOCK_SIZE) block coordinates
	U32 global_x0 = x0 / MAP_BLOCK_SIZE;
	U32 global_x1 = x1 / MAP_BLOCK_SIZE;
	U32 global_y0 = y0 / MAP_BLOCK_SIZE;
	U32 global_y1 = y1 / MAP_BLOCK_SIZE;

	// There is a bunch of extra logic here, as OpenSim grids support sim
	// coordinates that extend beyond the range used on the SL grid. We
	// basically just extend what LL had here by nesting the mMapBlockLoaded
	// array in a 'dynamic' grid, essentially making that array a 'block'
	// itself.

	// MapBlockRequest uses U16 for coordinate components. In order not to
	// exceed U16_MAX values, MAP_BLOCK_RES*MAP_BLOCK_SIZE*(i or j) cannot
	// exceed U16_MAX(65535)
	U32 max_range = (U16_MAX + 1) / MAP_BLOCK_RES/MAP_BLOCK_SIZE - 1;

	// Desired coordinate ranges in our 'dynamic' grid of 512x512 grids of 4x4
	// sim blocks.
	U32 map_block_x0 = global_x0 / MAP_BLOCK_RES;
	U32 map_block_x1 = llmin(global_x1 / MAP_BLOCK_RES, max_range);
	U32 map_block_y0 = global_y0 / MAP_BLOCK_RES;
	U32 map_block_y1 = llmin(global_y1 / MAP_BLOCK_RES, max_range);

	for (U32 i = map_block_x0; i <= map_block_x1; ++i)
	{
		for (U32 j = map_block_y0; j <= map_block_y1; ++j)
		{
			// Desired coordinate ranges in our 512x512 grids of 4x4 sim
			// blocks.
			x0 = global_x0 - i * MAP_BLOCK_RES;
			x1 = llmin(global_x1 - i * (U32)MAP_BLOCK_RES,
					   (U32)MAP_BLOCK_RES - 1);
			y0 = global_y0 - j * MAP_BLOCK_RES;
			y1 = llmin(global_y1 - j * (U32)MAP_BLOCK_RES,
					   (U32)MAP_BLOCK_RES - 1);

			std::vector<bool>& block = mMapBlockMap[mCurrentMap][(i << 16) | j];
			if (force_upd)
			{
				block.clear();
			}
			if (block.empty())
			{
				// New block. Allocate the array and set all entries to false
				// (seen as mMapBlockLoaded in v3)
				block.resize(MAP_BLOCK_RES * MAP_BLOCK_RES, false);
			}

			// Load the region info those blocks
			for (S32 block_x = llmax(x0, 0),
					 count_x = llmin(x1, MAP_BLOCK_RES - 1);
				 block_x <= count_x; ++block_x)
			{
				for (S32 block_y = llmax(y0, 0),
						 count_y = llmin(y1, MAP_BLOCK_RES - 1);
					 block_y <= count_y; ++block_y)
				{
					S32 offset = block_x | (block_y * MAP_BLOCK_RES);
					if (!block[offset])
					{
						U16 min_x = (block_x + i * MAP_BLOCK_RES) *
									MAP_BLOCK_SIZE;
						U16 max_x = min_x + MAP_BLOCK_SIZE - 1;
						U16 min_y = (block_y + j * MAP_BLOCK_RES) *
									MAP_BLOCK_SIZE;
						U16 max_y = min_y + MAP_BLOCK_SIZE - 1;

						sendMapBlockRequest(min_x, min_y, max_x, max_y);
						block[offset] = true;
						++blocks_requested;
					}
				}
			}
		}
	}

	return blocks_requested;
}

void LLWorldMap::sendItemRequest(U32 type, U64 handle)
{
	LLMessageSystem* msg = gMessageSystemp;

	msg->newMessageFast(_PREHASH_MapItemRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addU32Fast(_PREHASH_Flags, mCurrentMap);
	msg->addU32Fast(_PREHASH_EstateID, 0);		// Filled in on sim
	msg->addBoolFast(_PREHASH_Godlike, false);	// Filled in on sim

	msg->nextBlockFast(_PREHASH_RequestData);
	msg->addU32Fast(_PREHASH_ItemType, type);
	msg->addU64Fast(_PREHASH_RegionHandle, handle); // Filled in on sim if zero

	gAgent.sendReliableMessage();
}

bool LLWorldMap::isTrackedUnknownLocation(U32 x, U32 y) const
{
	return mIsTrackingUnknownLocation && mUnknownLocation.mdV[0] >= x &&
		   mUnknownLocation.mdV[0] < x + 256 && mUnknownLocation.mdV[1] >= y &&
		   mUnknownLocation.mdV[1] < y + 256;
}

//static
void LLWorldMap::mapLayerRequestCallback(LLSD result)
{
	if (!gFloaterWorldMapp) return;

	llinfos << "Got result from capability" << llendl;

	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);

	S32 agent_flags = result["AgentData"]["Flags"];
	if (agent_flags < 0 || agent_flags >= MAP_SIM_IMAGE_TYPES)
	{
		llwarns << "Invalid map image layer returned: " << agent_flags
				<< llendl;
		return;
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	LLUUID image_id;

	gWorldMap.mMapLayers[agent_flags].clear();

	bool use_web_map_tiles = useWebMapTiles();

	bool adjust = false;
	for (LLSD::array_const_iterator iter = result["LayerData"].beginArray(),
									end = result["LayerData"].endArray();
		 iter != end; ++iter)
	{
		const LLSD& layer_data = *iter;

		LLWorldMapLayer new_layer;
		new_layer.mLayerDefined = true;

		new_layer.mLayerExtents.mLeft = layer_data["Left"];
		new_layer.mLayerExtents.mRight = layer_data["Right"];
		new_layer.mLayerExtents.mBottom = layer_data["Bottom"];
		new_layer.mLayerExtents.mTop = layer_data["Top"];

		new_layer.mLayerImageID = layer_data["ImageID"];

#if 0	// No good... Maybe using of level 2 and higher web maps ?
		if (use_web_map_tiles)
		{
			new_layer.mLayerImage =
				LLWorldMap::loadObjectsTile((U32)new_layer.mLayerExtents.mLeft,
											(U32)new_layer.mLayerExtents.mBottom);
		}
		else
#endif
		new_layer.mLayerImage =
			LLViewerTextureManager::getFetchedTexture(new_layer.mLayerImageID);

		unit0->bind(new_layer.mLayerImage.get());
		new_layer.mLayerImage->setAddressMode(LLTexUnit::TAM_CLAMP);

		F32 x_meters = F32(new_layer.mLayerExtents.mLeft * REGION_WIDTH_UNITS);
		F32 y_meters = F32(new_layer.mLayerExtents.mBottom *
						   REGION_WIDTH_UNITS);
		if (gWorldMap.extendAABB((U32)x_meters, (U32)y_meters,
								 U32(x_meters + REGION_WIDTH_UNITS *
									 new_layer.mLayerExtents.getWidth()),
								 U32(y_meters + REGION_WIDTH_UNITS *
									 new_layer.mLayerExtents.getHeight())))
		{
			adjust = true;
		}

		gWorldMap.mMapLayers[agent_flags].push_back(new_layer);
	}

	gWorldMap.mMapLoaded[agent_flags] = true;
	if (adjust)
	{
		gFloaterWorldMapp->adjustZoomSliderBounds();
	}

	bool found_null_sim = false;

	adjust = false;
	if (result.has("MapBlocks"))
	{
		U32 cur_layer = gWorldMap.mCurrentMap;
		sim_info_map_t::iterator sit;
		const LLSD& map_blocks = result["MapBlocks"];
		for (LLSD::array_const_iterator iter = map_blocks.beginArray(),
										end = map_blocks.endArray();
			 iter != end; ++iter)
		{
			const LLSD& map_block = *iter;

			S32 x_regions = map_block["X"].asInteger();
			S32 y_regions = map_block["Y"].asInteger();
			std::string name = map_block["Name"].asString();
			S32 access = map_block["Access"].asInteger();
			U64 region_flags = map_block["RegionFlags"].asInteger();
			LLUUID image_id = map_block["MapImageID"].asUUID();

			U32 x_meters = x_regions * REGION_WIDTH_UNITS;
			U32 y_meters = y_regions * REGION_WIDTH_UNITS;

			if (access == 255)
			{
				// This region does not exist
				if (gWorldMap.isTrackedUnknownLocation(x_meters, y_meters))
				{
					// We were tracking this location, but it does not exist
					gWorldMap.mInvalidLocation = true;
				}

				found_null_sim = true;
				continue;
			}

			if (gWorldMap.extendAABB(x_meters, y_meters,
									 x_meters + REGION_WIDTH_UNITS,
									 y_meters + REGION_WIDTH_UNITS))
			{
				adjust = true;
			}

			U64 handle = to_region_handle(x_meters, y_meters);

			LLSimInfo* siminfo = new LLSimInfo();
			sit = gWorldMap.mSimInfoMap.find(handle);
			if (sit != gWorldMap.mSimInfoMap.end())
			{
				LLSimInfo* oldinfo = sit->second;
				siminfo->mAgentsUpdateTime = oldinfo->mAgentsUpdateTime;
				for (S32 image = 0; image < MAP_SIM_IMAGE_TYPES; ++image)
				{
					siminfo->mMapImageID[image] = oldinfo->mMapImageID[image];
				}
				delete oldinfo;
			}
			gWorldMap.mSimInfoMap[handle] = siminfo;

			siminfo->mHandle = handle;
			siminfo->mName.assign(name);
			siminfo->mAccess = access;
			siminfo->mRegionFlags = region_flags;
			siminfo->mMapImageID[agent_flags] = image_id;
			if (use_web_map_tiles)
			{
				siminfo->mCurrentImage[cur_layer] =
					loadObjectsTile((U32)x_regions, (U32)y_regions);
			}
			else
			{
				siminfo->mCurrentImage[cur_layer] =
					LLViewerTextureManager::getFetchedTexture(siminfo->mMapImageID[agent_flags]);
			}
			siminfo->mCurrentImage[cur_layer]->setAddressMode(LLTexUnit::TAM_CLAMP);
			unit0->bind(siminfo->mCurrentImage[cur_layer].get());

			if (siminfo->mMapImageID[2].notNull())
			{
				siminfo->mOverlayImage =
					LLViewerTextureManager::getFetchedTexture(siminfo->mMapImageID[2]);
			}
			else
			{
				siminfo->mOverlayImage = NULL;
			}

			if (gWorldMap.isTrackedUnknownLocation(x_meters, y_meters))
			{
				if (siminfo->mAccess == SIM_ACCESS_DOWN)
				{
					// We were tracking this location, but it does not exist
					gWorldMap.mInvalidLocation = true;
				}
				else
				{
					// We were tracking this location, and it does exist
					gFloaterWorldMapp->trackLocation(gWorldMap.mUnknownLocation);
					// Try another TP method...
					if (gWorldMap.mIsTrackingDoubleClick)
					{
						LLVector3d pos_global =
							gTracker.getTrackedPositionGlobal();
						gAgent.teleportViaLocation(pos_global);
					}
				}
			}
		}
	}
	if (adjust)
	{
		gFloaterWorldMapp->adjustZoomSliderBounds();
	}
	gFloaterWorldMapp->updateSims(found_null_sim);
}

void LLWorldMap::sendMapLayerRequest()
{
	LLSD body;
	body["Flags"] = (S32)mCurrentMap;

	LLAgent::httpCallback_t succ =
		boost::bind(&LLWorldMap::mapLayerRequestCallback, _1);
	const char* cap_name = gAgent.isGodlike() ? "MapLayerGod" : "MapLayer";
	if (gAgent.requestPostCapability(cap_name, body, succ))
	{
		llinfos << "Sent map layer request via capability: " << cap_name
				<< llendl;
	}
	else
	{
		llinfos << "Sending map layer request via message system" << llendl;
		LLMessageSystem* msg = gMessageSystemp;

		// Request for layer
		msg->newMessageFast(_PREHASH_MapLayerRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addU32Fast(_PREHASH_Flags, mCurrentMap);
		msg->addU32Fast(_PREHASH_EstateID, 0);		// Filled in on sim
		msg->addBoolFast(_PREHASH_Godlike, false);	// Filled in on sim
		gAgent.sendReliableMessage();

		if (mRequestLandForSale)
		{
			msg->newMessageFast(_PREHASH_MapLayerRequest);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->addU32Fast(_PREHASH_Flags, 2);
			msg->addU32Fast(_PREHASH_EstateID, 0);		// Filled in on sim
			msg->addBoolFast(_PREHASH_Godlike, false);	// Filled in on sim
			gAgent.sendReliableMessage();
		}
	}
}

void LLWorldMap::sendNamedRegionRequest(const std::string& region_name)
{
	// Request for region data
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_MapNameRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
#if 0
	msg->addU32Fast(_PREHASH_Flags, mCurrentMap);
#else
	// Note: in OpenSIM, send request with layer = 2, which is what v2+ viewers
	// (i.e. viewers without terrain-only layer support) always do... Doing
	// otherwise confuses the newest OpenSIM servers. :-( HB
	// *TODO: verify this is still needed. HB
	msg->addU32Fast(_PREHASH_Flags,
					gIsInSecondLife ? mCurrentMap : MAP_SIM_LAND_FOR_SALE);
#endif
	msg->addU32Fast(_PREHASH_EstateID, 0);		// Filled in on sim
	msg->addBoolFast(_PREHASH_Godlike, false);	// Filled in on sim
	msg->nextBlockFast(_PREHASH_NameData);
	msg->addStringFast(_PREHASH_Name, region_name);
	gAgent.sendReliableMessage();
}

void LLWorldMap::sendNamedRegionRequest(const std::string& region_name,
										url_callback_t callback,
										const std::string& callback_url,
										// Teleport when result returned
										bool teleport)
{
	mSLURLRegionName = region_name;
	mSLURLRegionHandle = 0;
	mSLURL = callback_url;
	mSLURLCallback = callback;
	mSLURLTeleport = teleport;

	sendNamedRegionRequest(mSLURLRegionName);
}

void LLWorldMap::sendHandleRegionRequest(U64 region_handle,
										 url_callback_t callback,
										 const std::string& url, bool teleport)
{
	mSLURLRegionName.clear();
	mSLURLRegionHandle = region_handle;
	mSLURL = url;
	mSLURLCallback = callback;
	mSLURLTeleport = teleport;

	U32 global_x, global_y;
	from_region_handle(region_handle, &global_x, &global_y);
	U16 grid_x = (U16)(global_x / REGION_WIDTH_UNITS);
	U16 grid_y = (U16)(global_y / REGION_WIDTH_UNITS);

	sendMapBlockRequest(grid_x, grid_y, grid_x, grid_y, true);
}

void LLWorldMap::sendMapBlockRequest(U16 min_x, U16 min_y, U16 max_x,
									 U16 max_y, bool return_nonexistent)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_MapBlockRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	U32 flags = mCurrentMap;
	flags |= (return_nonexistent ? 0x10000 : 0);
	msg->addU32Fast(_PREHASH_Flags, flags);
	msg->addU32Fast(_PREHASH_EstateID, 0);		// Filled in on sim
	msg->addBoolFast(_PREHASH_Godlike, false);	// Filled in on sim
	msg->nextBlockFast(_PREHASH_PositionData);
	msg->addU16Fast(_PREHASH_MinX, min_x);
	msg->addU16Fast(_PREHASH_MinY, min_y);
	msg->addU16Fast(_PREHASH_MaxX, max_x);
	msg->addU16Fast(_PREHASH_MaxY, max_y);
	gAgent.sendReliableMessage();

	if (mRequestLandForSale)
	{
		msg->newMessageFast(_PREHASH_MapBlockRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addU32Fast(_PREHASH_Flags, 2);
		msg->addU32Fast(_PREHASH_EstateID, 0);		// Filled in on sim
		msg->addBoolFast(_PREHASH_Godlike, false);	// Filled in on sim
		msg->nextBlockFast(_PREHASH_PositionData);
		msg->addU16Fast(_PREHASH_MinX, min_x);
		msg->addU16Fast(_PREHASH_MinY, min_y);
		msg->addU16Fast(_PREHASH_MaxX, max_x);
		msg->addU16Fast(_PREHASH_MaxY, max_y);
		gAgent.sendReliableMessage();
	}
}

//static
void LLWorldMap::processMapLayerReply(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("WorldMap") << "Processing map layer reply from message system"
						  << LL_ENDL;

	U32 agent_flags;
	msg->getU32Fast(_PREHASH_AgentData, _PREHASH_Flags, agent_flags);
	if (agent_flags >= (U32)MAP_SIM_IMAGE_TYPES)
	{
		llwarns << "Invalid map image layer returned: " << agent_flags
				<< llendl;
		return;
	}
	gWorldMap.mMapLayers[agent_flags].clear();

	LLTexUnit* unit0 = gGL.getTexUnit(0);
#if 0
	bool use_web_map_tiles = useWebMapTiles();
#endif
	bool adjust = false;
	S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_LayerData);
	for (S32 block = 0; block < num_blocks; ++block)
	{
		LLWorldMapLayer new_layer;
		new_layer.mLayerDefined = true;
		msg->getUUIDFast(_PREHASH_LayerData, _PREHASH_ImageID,
						 new_layer.mLayerImageID, block);

		U32 left, right, top, bottom;
		msg->getU32Fast(_PREHASH_LayerData, _PREHASH_Left, left, block);
		msg->getU32Fast(_PREHASH_LayerData, _PREHASH_Right, right, block);
		msg->getU32Fast(_PREHASH_LayerData, _PREHASH_Top, top, block);
		msg->getU32Fast(_PREHASH_LayerData, _PREHASH_Bottom, bottom, block);

#if 0	// No good... Maybe using of level 2 and higher web maps ?
		if (use_web_map_tiles)
		{
			new_layer.mLayerImage = loadObjectsTile(left, bottom);
		}
		else
#endif
		new_layer.mLayerImage =
			LLViewerTextureManager::getFetchedTexture(new_layer.mLayerImageID,
													  FTT_MAP_TILE, MIPMAP_YES,
													  LLGLTexture::BOOST_MAP,
													  LLViewerTexture::LOD_TEXTURE);

		unit0->bind(new_layer.mLayerImage.get());
		new_layer.mLayerImage->setAddressMode(LLTexUnit::TAM_CLAMP);

		new_layer.mLayerExtents.mLeft = left;
		new_layer.mLayerExtents.mRight = right;
		new_layer.mLayerExtents.mBottom = bottom;
		new_layer.mLayerExtents.mTop = top;

		F32 x_meters = F32(left * REGION_WIDTH_UNITS);
		F32 y_meters = F32(bottom * REGION_WIDTH_UNITS);
		if (gWorldMap.extendAABB((U32)x_meters, (U32)y_meters,
								 U32(x_meters + REGION_WIDTH_UNITS *
									 new_layer.mLayerExtents.getWidth()),
								 U32(y_meters + REGION_WIDTH_UNITS *
									 new_layer.mLayerExtents.getHeight())))
		{
			adjust = true;
		}

		gWorldMap.mMapLayers[agent_flags].push_back(new_layer);
	}

	gWorldMap.mMapLoaded[agent_flags] = true;
	if (adjust && gFloaterWorldMapp)
	{
		gFloaterWorldMapp->adjustZoomSliderBounds();
	}
}

//static
bool LLWorldMap::useWebMapTiles()
{
	static LLCachedControl<bool> use_web_map_tiles(gSavedSettings,
												   "UseWebMapTiles");
	return use_web_map_tiles && (gIsInSecondLife || sGotMapURL) &&
		   !sMapURL.empty() && gWorldMap.mCurrentMap == 0;
}

//static
LLPointer<LLViewerFetchedTexture> LLWorldMap::loadObjectsTile(U32 grid_x,
															  U32 grid_y)
{
	// Get the grid coordinates
	std::string imageurl = sMapURL + llformat("map-1-%d-%d-objects.jpg",
											  grid_x, grid_y);
	LLPointer<LLViewerFetchedTexture> texp =
		LLViewerTextureManager::getFetchedTextureFromUrl(imageurl,
														 FTT_MAP_TILE, true,
														 LLGLTexture::BOOST_MAP,
														 LLViewerTexture::LOD_TEXTURE);
	// Return the smart pointer
	return texp;
}

//static
void LLWorldMap::processMapBlockReply(LLMessageSystem* msg, void**)
{
	if (!gFloaterWorldMapp) return;

	U32 agent_flags;
	msg->getU32Fast(_PREHASH_AgentData, _PREHASH_Flags, agent_flags);
	if (agent_flags >= (U32)MAP_SIM_IMAGE_TYPES)
	{
		llwarns << "Invalid map image type returned, layer = " << agent_flags
				<< llendl;
		return;
	}

	S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_Data);

	bool found_null_sim = false;
	bool adjust = false;
	for (S32 block = 0; block < num_blocks; ++block)
	{
		U16 x_regions, y_regions;
		msg->getU16Fast(_PREHASH_Data, _PREHASH_X, x_regions, block);
		msg->getU16Fast(_PREHASH_Data, _PREHASH_Y, y_regions, block);
		std::string name;
		msg->getStringFast(_PREHASH_Data, _PREHASH_Name, name, block);
		U8 accesscode;
		msg->getU8Fast(_PREHASH_Data, _PREHASH_Access, accesscode, block);
		U32 region_flags;
		msg->getU32Fast(_PREHASH_Data, _PREHASH_RegionFlags, region_flags, block);
		LLUUID image_id;
		msg->getUUIDFast(_PREHASH_Data, _PREHASH_MapImageID, image_id, block);

		// OpenSim bug. BlockRequest can return sims without names with an
		// accesscode that is not 255. Skip if this has happened.
		if (name.empty() && accesscode != 255)
		{
			continue;
		}

		// Variable region size support
		U16 x_size = 0;
		U16 y_size = 0;
		if (msg->getNumberOfBlocksFast(_PREHASH_Size) > 0)
		{
			msg->getU16Fast(_PREHASH_Size, _PREHASH_SizeX, x_size, block);
			msg->getU16Fast(_PREHASH_Size, _PREHASH_SizeY, y_size, block);
			if (x_size == 0 || x_size % 16 != 0 || y_size % 16 != 0)
			{
				x_size = y_size = 0;
			}
		}

		U32 x_meters = (U32)x_regions * REGION_WIDTH_UNITS;
		U32 y_meters = (U32)y_regions * REGION_WIDTH_UNITS;
		U64 handle = to_region_handle(x_meters, y_meters);

		if (accesscode == 255)
		{
			// This region does not exist
			if (gWorldMap.isTrackedUnknownLocation(x_meters, y_meters))
			{
				// We were tracking this location, but it does not exist
				gWorldMap.mInvalidLocation = true;
			}

			found_null_sim = true;
		}
		else
		{
			if (gWorldMap.extendAABB(x_meters, y_meters,
									 x_meters + REGION_WIDTH_UNITS,
									 y_meters + REGION_WIDTH_UNITS))
			{
				adjust = true;
			}

			LLSimInfo* siminfo;
			sim_info_map_t::iterator iter = gWorldMap.mSimInfoMap.find(handle);
			if (iter != gWorldMap.mSimInfoMap.end())
			{
				siminfo = iter->second;
			}
			else
			{
				siminfo = new LLSimInfo();
				gWorldMap.mSimInfoMap[handle] = siminfo;
			}
			siminfo->mHandle = handle;
			siminfo->mName.assign(name);
			siminfo->mAccess = accesscode;
			siminfo->mRegionFlags = region_flags;
			siminfo->mMapImageID[agent_flags] = image_id;
			// Variable region size support
			if (x_size && y_size)
			{
				siminfo->setSize(x_size, y_size);
			}

			if (siminfo->mMapImageID[2].isNull())
			{
				siminfo->mOverlayImage = NULL;
			}

			if (gWorldMap.isTrackedUnknownLocation(x_meters, y_meters))
			{
				if (siminfo->mAccess == SIM_ACCESS_DOWN)
				{
					// We were tracking this location, but it does not exist
					gWorldMap.mInvalidLocation = true;
				}
				else
				{
					// We were tracking this location, and it does exist
					if (gFloaterWorldMapp)
					{
						gFloaterWorldMapp->trackLocation(gWorldMap.mUnknownLocation);
					}
					// Try another TP method...
					if (gWorldMap.mIsTrackingDoubleClick)
					{
						LLVector3d pos_global =
							gTracker.getTrackedPositionGlobal();
						gAgent.teleportViaLocation(pos_global);
					}
				}
			}
		}

		if (gWorldMap.mSLURLCallback)
		{
			// Server returns definitive capitalization, SLURL might not have
			// that.
			if (gWorldMap.mSLURLRegionHandle == handle ||
				LLStringUtil::compareInsensitive(gWorldMap.mSLURLRegionName,
												 name) == 0)
			{
				url_callback_t callback = gWorldMap.mSLURLCallback;

				gWorldMap.mSLURLCallback = NULL;
				gWorldMap.mSLURLRegionName.clear();
				gWorldMap.mSLURLRegionHandle = 0;

				callback(handle, gWorldMap.mSLURL, image_id,
						 gWorldMap.mSLURLTeleport);
			}
		}
	}

	if (adjust)
	{
		gFloaterWorldMapp->adjustZoomSliderBounds();
	}

	gFloaterWorldMapp->updateSims(found_null_sim);
}

//static
void LLWorldMap::processMapItemReply(LLMessageSystem* msg, void**)
{
	U32 type;
	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_ItemType, type);

	static LLCachedControl<std::string> dfmt(gSavedSettings,
											 "ShortDateFormat");
	static LLCachedControl<std::string> tfmt(gSavedSettings,
											 "ShortTimeFormat");
	std::string time_format = dfmt;
	time_format += ' ';
	time_format += tfmt;

	S32 num_blocks = msg->getNumberOfBlocks("Data");
	for (S32 block = 0; block < num_blocks; ++block)
	{
		U32 x, y;
		msg->getU32Fast(_PREHASH_Data, _PREHASH_X, x, block);
		msg->getU32Fast(_PREHASH_Data, _PREHASH_Y, y, block);
		std::string name;
		msg->getStringFast(_PREHASH_Data, _PREHASH_Name, name, block);
		LLUUID uuid;
		msg->getUUIDFast(_PREHASH_Data, _PREHASH_ID, uuid, block);
		S32 extra, extra2;
		msg->getS32Fast(_PREHASH_Data, _PREHASH_Extra, extra, block);
		msg->getS32Fast(_PREHASH_Data, _PREHASH_Extra2, extra2, block);

		F32 world_x = (F32)x;
		x /= REGION_WIDTH_UNITS;
		F32 world_y = (F32)y;
		y /= REGION_WIDTH_UNITS;

		LLItemInfo new_item(world_x, world_y, name, uuid, extra, extra2);
		U64 handle = new_item.mRegionHandle;
		LLSimInfo* siminfo = gWorldMap.simInfoFromHandle(handle);

		switch (type)
		{
			case MAP_ITEM_TELEHUB:
			{
				// Telehub color, store in extra as 4 U8's
				U8* color = (U8*)&new_item.mExtra;

				F32 red = fmodf((F32)x * 0.11f, 1.f) * 0.8f;
				F32 green = fmodf((F32)y * 0.11f, 1.f) * 0.8f;
				F32 blue = fmodf(1.5f * (F32)(x + y) * 0.11f, 1.f) * 0.8f;
				F32 add_amt = x % 2 ? 0.15f : -0.15f;
				add_amt += y % 2 ? -0.15f : 0.15f;
				color[0] = U8((red + add_amt) * 255);
				color[1] = U8((green + add_amt) * 255);
				color[2] = U8((blue + add_amt) * 255);
				color[3] = 255;

				// extra2 specifies whether this is an infohub or a telehub.
				if (extra2)
				{
					gWorldMap.mInfohubs.push_back(new_item);
				}
				else
				{
					gWorldMap.mTelehubs.push_back(new_item);
				}

				break;
			}

			case MAP_ITEM_PG_EVENT:
			case MAP_ITEM_MATURE_EVENT:
			case MAP_ITEM_ADULT_EVENT:
			{
				new_item.mToolTip =
					LLGridManager::getTimeStamp(extra, time_format);

				// *HACK: store Z in extra2
				new_item.mPosGlobal.mdV[VZ] = (F64)extra2;
				if (type == MAP_ITEM_PG_EVENT)
				{
					gWorldMap.mPGEvents.push_back(new_item);
				}
				else if (type == MAP_ITEM_MATURE_EVENT)
				{
					gWorldMap.mMatureEvents.push_back(new_item);
				}
				else if (type == MAP_ITEM_ADULT_EVENT)
				{
					gWorldMap.mAdultEvents.push_back(new_item);
				}

				break;
			}

			case MAP_ITEM_LAND_FOR_SALE:
			case MAP_ITEM_LAND_FOR_SALE_ADULT:
			{
				new_item.mToolTip = llformat("%d m2 L$%d", new_item.mExtra,
											 new_item.mExtra2);
				if (type == MAP_ITEM_LAND_FOR_SALE)
				{
					gWorldMap.mLandForSale.push_back(new_item);
				}
				else if (type == MAP_ITEM_LAND_FOR_SALE_ADULT)
				{
					gWorldMap.mLandForSaleAdult.push_back(new_item);
				}
				break;
			}

			case MAP_ITEM_AGENT_LOCATIONS:
			{
				if (!siminfo)
				{
					llinfos << "Sim info missing for "
							<< new_item.mPosGlobal.mdV[0]
							<< ", " << new_item.mPosGlobal.mdV[1]
							<< llendl;
					break;
				}
 				LL_DEBUGS("WorldMap") << "New Location: " << new_item.mName
									  << LL_ENDL;

				item_info_list_t& agentcounts =
					gWorldMap.mAgentLocationsMap[handle];

				// Find the last item in the list with a different name and
				// erase them
				item_info_list_t::iterator lastiter;
				item_info_list_t::iterator end = agentcounts.end();
				for (lastiter = agentcounts.begin(); lastiter != end;
					 ++lastiter)
				{
					const LLItemInfo& info = *lastiter;
					if (info.mName == new_item.mName)
					{
						break;
					}
				}
				if (lastiter != agentcounts.begin())
				{
					agentcounts.erase(agentcounts.begin(), lastiter);
				}
				// Now append the new location
				if (new_item.mExtra > 0)
				{
					agentcounts.push_back(new_item);
				}
				break;
			}

			case MAP_ITEM_CLASSIFIED:	// Deprecated, no longer used.
			default:
				break;
		}
	}
}

void LLWorldMap::dump()
{
	for (sim_info_map_t::iterator it = mSimInfoMap.begin(),
								  end = mSimInfoMap.end();
		 it != end; ++it)
	{
		U64 handle = it->first;
		LLSimInfo* info = it->second;

		U32 x_pos, y_pos;
		from_region_handle(handle, &x_pos, &y_pos);

		llinfos << info->mName << " (" << x_pos << "," << y_pos
				<< ") - Access: " << (S32)info->mAccess
				<< " - Flags: " << std::hex << info->mRegionFlags << std::dec;
		LLViewerFetchedTexture* texp = info->mCurrentImage[mCurrentMap].get();
		if (texp)
		{
			llcont << " - Image layer: " << mCurrentMap
				   << " - Image: discard: " << (S32)texp->getDiscardLevel()
				   << " - full width: " << texp->getWidth(0)
				   << " - full height: " << texp->getHeight(0)
				   << " - max virtual size: " << texp->getMaxVirtualSize()
				   << " - max discard: " << (S32)texp->getMaxDiscardLevel();
		}
		llcont << llendl;
	}
}

bool LLWorldMap::extendAABB(U32 min_x, U32 min_y, U32 max_x, U32 max_y)
{
	bool rv = false;
	if (min_x < mMinX)
	{
		rv = true;
		mMinX = min_x;
	}
	if (min_y < mMinY)
	{
		rv = true;
		mMinY = min_y;
	}
	if (max_x > mMaxX)
	{
		rv = true;
		mMaxX = max_x;
	}
	if (max_y > mMaxY)
	{
		rv = true;
		mMaxY = max_y;
	}
	LL_DEBUGS("WorldMap") << "World map AABB: (" << mMinX << ", " << mMinY
						  << "), (" << mMaxX << ", " << mMaxY << ")"
						  << LL_ENDL;
	return rv;
}
