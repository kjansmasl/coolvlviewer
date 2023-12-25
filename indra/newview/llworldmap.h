/**
 * @file llworldmap.h
 * @brief Underlying data storage for the map of the entire world.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
 * Copyright (c) 2009-2021, Henri Beauchamp.
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

#ifndef LL_LLWORLDMAP_H
#define LL_LLWORLDMAP_H

#include "boost/function.hpp"

#include "hbfastmap.h"
#include "llframetimer.h"
#include "llpointer.h"
#include "llrect.h"
#include "llstring.h"
#include "lluuid.h"
#include "llvector3d.h"

#include "llviewertexture.h"

// Region map layer numbers
constexpr S32 MAP_SIM_OBJECTS		= 0;	
constexpr S32 MAP_SIM_TERRAIN		= 1;
// Transparent alpha overlay of land for sale
constexpr S32 MAP_SIM_LAND_FOR_SALE = 2;
constexpr S32 MAP_SIM_IMAGE_TYPES	= 3; // Number of map layers

// map item types
constexpr U32 MAP_ITEM_TELEHUB				= 0x01;
constexpr U32 MAP_ITEM_PG_EVENT				= 0x02;
constexpr U32 MAP_ITEM_MATURE_EVENT			= 0x03;
//constexpr U32 MAP_ITEM_POPULAR			= 0x04;
//constexpr U32 MAP_ITEM_AGENT_COUNT		= 0x05;
constexpr U32 MAP_ITEM_AGENT_LOCATIONS		= 0x06;
constexpr U32 MAP_ITEM_LAND_FOR_SALE		= 0x07;
constexpr U32 MAP_ITEM_CLASSIFIED			= 0x08;
constexpr U32 MAP_ITEM_ADULT_EVENT			= 0x09;
constexpr U32 MAP_ITEM_LAND_FOR_SALE_ADULT	= 0x0a;

class LLMessageSystem;

class LLItemInfo
{
public:
	LLItemInfo(F32 global_x, F32 global_y, const std::string& name,
			   const LLUUID& id, S32 extra = 0, S32 extra2 = 0);

public:
	LLVector3d	mPosGlobal;
	LLUUID		mID;
	S32			mExtra;
	S32			mExtra2;
	U64			mRegionHandle;
	std::string mName;
	std::string mToolTip;
	bool		mSelected;
};

#define MAP_SIM_IMAGE_TYPES 3
// 0 - Prim
// 1 - Terrain Only
// 2 - Overlay: Land For Sale

class LLSimInfo
{
public:
	LLSimInfo();

	LLVector3d getGlobalPos(LLVector3 local_pos) const;

	// Get the world coordinates of the SW corner of that region
	LLVector3d getGlobalOrigin() const;

	LL_INLINE void setSize(U16 x, U16 y)			{ mSizeX = y; mSizeY = y; }
	LL_INLINE const U64& getHandle() const			{ return mHandle; }
	LL_INLINE U16 getSizeX() const					{ return mSizeX; }
	LL_INLINE U16 getSizeY() const					{ return mSizeY; }

public:
	U64									mHandle;

	U64									mRegionFlags;

	F64									mAgentsUpdateTime;

	// Hold a reference to the currently displayed image.
	LLPointer<LLViewerFetchedTexture>	mCurrentImage[2];
	LLPointer<LLViewerFetchedTexture>	mOverlayImage;

	std::string							mName;

	// Image ID for the current overlay mode.
	LLUUID								mMapImageID[MAP_SIM_IMAGE_TYPES];

	// Filled up when counting agents on the map: caching this number here
	// prevents from managing a separate std::map and speeds things up...
	S32									mAgentsCount;

	F32									mAlpha;

	// For variable region size support
	U16									mSizeX;
	U16									mSizeY;

	U8									mAccess;

	// Are agents visible ?
	bool								mShowAgentLocations;
};

#define MAP_BLOCK_RES 256
// We request region data on the world by "blocks" of (MAP_BLOCK_SIZE x
// MAP_BLOCK_SIZE) regions. This is to reduce the number of requests to the
// asset DB and get things in big "blocks"
#define MAP_BLOCK_SIZE 16

struct LLWorldMapLayer
{
	LLWorldMapLayer()
	:	mLayerDefined(false)
	{
	}

	LLPointer<LLViewerFetchedTexture>	mLayerImage;
	LLUUID								mLayerImageID;
	LLRect								mLayerExtents;
	bool								mLayerDefined;
};

class LLWorldMap
{
protected:
	LOG_CLASS(LLWorldMap);

public:
	typedef boost::function<void(U64 region_handle, const std::string& url,
								 const LLUUID& snapshot_id,
								 bool teleport)> url_callback_t;

	LLWorldMap();
	~LLWorldMap();

	// Clears the list
	void reset();

	// Clear the visible items
	void eraseItems(bool force = false);

	// Removes references to cached images. If layer is omitted or out of range
	// images for both layer 0 and 1 are unloaded. Else, only the requested
	// layer image is unloaded.
	void clearImageRefs(S32 layer = -1);

	// Clears the flags indicating that we've received sim infos. Causes a
	// re-request of the sim info without erasing existing info.
	void clearSimFlags();

	// Returns simulator information, or NULL if out of range
	LLSimInfo* simInfoFromHandle(U64 handle);

	// Returns simulator information, or NULL if out of range
	LLSimInfo* simInfoFromPosGlobal(const LLVector3d& pos_global);

	// Returns simulator information for named sim, or NULL if non-existent
	LLSimInfo* simInfoFromName(std::string sim_name);

	// Gets simulator name for a global position, returns true if it was found
	bool simNameFromPosGlobal(const LLVector3d& pos_global,
							  std::string& sim_name);

	// Sets the current layer
	void setCurrentLayer(U32 layer, bool request_layer = false);
	LL_INLINE U32 getCurrentLayer() const			{ return mCurrentMap; }

	void forceUpdateRegion(U64 handle);
	U32 updateRegions(S32 x0, S32 y0, S32 x1, S32 y1, bool force_upd = false);

	void sendMapLayerRequest();
	void sendMapBlockRequest(U16 min_x, U16 min_y, U16 max_x, U16 max_y,
							 bool return_nonexistent = false);
	void sendNamedRegionRequest(const std::string& region_name);
	void sendNamedRegionRequest(const std::string& region_name,
								url_callback_t callback,
								const std::string& callback_url,
								bool teleport);
	// When teleport is true, the callback should TP the agent
	void sendHandleRegionRequest(U64 region_handle,
								 url_callback_t callback = NULL,
								 const std::string& url = LLStringUtil::null,
								 bool teleport = false);
	void sendItemRequest(U32 type, U64 handle = 0);

	static void processMapLayerReply(LLMessageSystem*, void**);
	static void processMapBlockReply(LLMessageSystem*, void**);
	static void processMapItemReply(LLMessageSystem*, void**);

	LL_INLINE static void gotMapServerURL(bool b)	{ sGotMapURL = b; }

	LL_INLINE static void setMapServerURL(std::string url,
										  bool login = false)
	{
		sMapURL = url;
		sMapURLSetOnLogin = login;
	}

	LL_INLINE static bool wasMapURLSetOnLogin()		{ return sMapURLSetOnLogin; }

	static bool useWebMapTiles();
	static LLPointer<LLViewerFetchedTexture> loadObjectsTile(U32 grid_x,
															 U32 grid_y);

	void dump();

	// Bounds of the world, in meters
	LL_INLINE U32 getWorldWidth() const				{ return mMaxX - mMinX; }
	LL_INLINE U32 getWorldHeight() const			{ return mMaxY - mMinY; }

private:
	// Extend the bounding box of the list of simulators. Returns true if the
	// extents changed.
	bool extendAABB(U32 x_min, U32 y_min, U32 x_max, U32 y_max);

	bool isTrackedUnknownLocation(U32 x, U32 y) const;

	static void mapLayerRequestCallback(LLSD result);

public:
	LLVector3d			mUnknownLocation;

	// Map from region-handle to simulator info
	typedef fast_hmap<U64, LLSimInfo*> sim_info_map_t;
	sim_info_map_t		mSimInfoMap;

	typedef std::vector<LLItemInfo> item_info_list_t;
	item_info_list_t	mTelehubs;
	item_info_list_t	mInfohubs;
	item_info_list_t	mPGEvents;
	item_info_list_t	mMatureEvents;
	item_info_list_t	mAdultEvents;
	item_info_list_t	mLandForSale;
	item_info_list_t	mLandForSaleAdult;

	typedef fast_hmap<U64, item_info_list_t> agent_list_map_t;
	agent_list_map_t	mAgentLocationsMap;

	typedef fast_hmap<U32, std::vector<bool> > mapblock_map_t;
	mapblock_map_t		mMapBlockMap[MAP_SIM_IMAGE_TYPES];

	typedef std::vector<LLWorldMapLayer> map_layers_vec_t;
	map_layers_vec_t	mMapLayers[MAP_SIM_IMAGE_TYPES];
	bool				mMapLoaded[MAP_SIM_IMAGE_TYPES];

	bool 				mIsTrackingUnknownLocation;
	bool				mInvalidLocation;
	bool				mIsTrackingDoubleClick;
	bool				mIsTrackingCommit;

	bool				mRequestLandForSale;

private:
	U32					mCurrentMap;

	// AABB of the list of simulators
	U32					mMinX;
	U32					mMaxX;
	U32					mMinY;
	U32					mMaxY;

	LLTimer				mRequestTimer;

	U64					mSLURLRegionHandle;

	// Search for named region for url processing
	std::string			mSLURLRegionName;

	std::string			mSLURL;

	url_callback_t		mSLURLCallback;
	bool				mSLURLTeleport;

	static bool			sGotMapURL;
	static bool			sMapURLSetOnLogin;
	static std::string	sMapURL;
};

extern LLWorldMap gWorldMap;

#endif
