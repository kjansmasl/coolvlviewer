/**
 * @file llviewerregion.h
 * @brief Description of the LLViewerRegion class.
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

#ifndef LL_LLVIEWERREGION_H
#define LL_LLVIEWERREGION_H

// A ViewerRegion is a class that contains a bunch of objects and surfaces
// that are in to a particular region.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "boost/signals2.hpp"

#include "llcorehttpheaders.h"
#include "llcorehttpoptions.h"
#include "llframetimer.h"
#include "llhost.h"
#include "llregionflags.h"
#include "llstat.h"
#include "llstring.h"
#include "lluuid.h"
#include "llwind.h"
#include "llmatrix4.h"

#include "llcloud.h"
#include "llreflectionmapmanager.h"
#include "llvocache.h"

// Surface id's
#define LAND  1
#define WATER 2

constexpr U32 MAX_OBJECT_CACHE_ENTRIES = 50000;

// Region handshake flags
constexpr U32 REGION_HANDSHAKE_SUPPORTS_SELF_APPEARANCE = 1U << 2;

class LLBBox;
class LLDrawable;
class LLEventPoll;
class LLMessageSystem;
class LLSpatialGroup;
class LLSpatialPartition;
class LLSurface;
class LLViewerObject;
class LLViewerOctreeGroup;
class LLViewerParcelOverlay;
class LLVLComposition;
class LLVOCache;
class LLVOCacheEntry;
class LLVOCachePartition;

class LLViewerRegion
{
protected:
	LOG_CLASS(LLViewerRegion);

public:
	// MUST MATCH THE ORDER OF DECLARATION IN CONSTRUCTOR
	typedef enum
	{
		PARTITION_HUD				= 0,
		PARTITION_TERRAIN,
		PARTITION_VOIDWATER,
		PARTITION_WATER,
		PARTITION_TREE,
		PARTITION_PARTICLE,
		PARTITION_CLOUD,
		PARTITION_GRASS,
		PARTITION_VOLUME,
		PARTITION_BRIDGE,
		PARTITION_AVATAR,
		PARTITION_PUPPET,
		PARTITION_HUD_PARTICLE,
		PARTITION_VO_CACHE,
		PARTITION_NONE,
		NUM_PARTITIONS
	} eObjectPartitions;

	typedef boost::signals2::signal<void(const LLUUID& region_id)> caps_received_sig_t;
	typedef caps_received_sig_t::slot_type caps_received_cb_t;

	LLViewerRegion(const U64& handle, const LLHost& host,
				   U32 surface_grid_width, U32 patch_grid_width,
				   F32 region_width_meters);
	~LLViewerRegion();

	// Sends the current message to this region's simulator
	void sendMessage();
	// Sends the current message to this region's simulator
	void sendReliableMessage();

	// Sends a EstateCovenantInfoRequest request message for this region.
	void sendEstateCovenantRequest();

	void setOriginGlobal(const LLVector3d& origin);
	void updateRenderMatrix();

	LL_INLINE void setAllowDamage(bool b)					{ setRegionFlag(REGION_FLAGS_ALLOW_DAMAGE, b); }
	LL_INLINE void setAllowLandmark(bool b)					{ setRegionFlag(REGION_FLAGS_ALLOW_LANDMARK, b); }
	LL_INLINE void setAllowSetHome(bool b)					{ setRegionFlag(REGION_FLAGS_ALLOW_SET_HOME, b); }
	LL_INLINE void setResetHomeOnTeleport(bool b)			{ setRegionFlag(REGION_FLAGS_RESET_HOME_ON_TELEPORT, b); }
	LL_INLINE void setSunFixed(bool b)						{ setRegionFlag(REGION_FLAGS_SUN_FIXED, b); }
	LL_INLINE void setAllowDirectTeleport(bool b)			{ setRegionFlag(REGION_FLAGS_ALLOW_DIRECT_TELEPORT, b); }

	LL_INLINE bool getAllowDamage() const					{ return (mRegionFlags & REGION_FLAGS_ALLOW_DAMAGE) != 0; }
	LL_INLINE bool getAllowLandmark() const					{ return (mRegionFlags & REGION_FLAGS_ALLOW_LANDMARK) != 0; }
	LL_INLINE bool getAllowSetHome() const					{ return (mRegionFlags & REGION_FLAGS_ALLOW_SET_HOME) != 0; }
	LL_INLINE bool getResetHomeOnTeleport() const			{ return (mRegionFlags & REGION_FLAGS_RESET_HOME_ON_TELEPORT) != 0; }
	LL_INLINE bool getSunFixed() const						{ return (mRegionFlags & REGION_FLAGS_SUN_FIXED) != 0; }
	LL_INLINE bool getBlockFly() const						{ return (mRegionFlags & REGION_FLAGS_BLOCK_FLY) != 0; }
	LL_INLINE bool getAllowDirectTeleport() const			{ return (mRegionFlags & REGION_FLAGS_ALLOW_DIRECT_TELEPORT) != 0; }
	LL_INLINE bool isPrelude() const						{ return is_prelude(mRegionFlags); }
	LL_INLINE bool getAllowTerraform() const				{ return (mRegionFlags & REGION_FLAGS_BLOCK_TERRAFORM) == 0; }
	LL_INLINE bool getRestrictPushObject() const			{ return (mRegionFlags & REGION_FLAGS_RESTRICT_PUSHOBJECT) != 0; }
	LL_INLINE bool getAllowEnvironmentOverride() const		{ return (mRegionFlags & REGION_FLAGS_ALLOW_ENVIRONMENT_OVERRIDE) != 0; }

	// Can become false if circuit disconnects
	LL_INLINE bool isAlive()								{ return mAlive; }

	void setWaterHeight(F32 water_level);
	F32 getWaterHeight() const;

	LL_INLINE bool isVoiceEnabled() const					{ return getRegionFlag(REGION_FLAGS_ALLOW_VOICE); }

	LL_INLINE void setBillableFactor(F32 billable_factor)	{ mBillableFactor = billable_factor; }
	LL_INLINE F32 getBillableFactor() const 				{ return mBillableFactor; }

	// Maximum number of primitives allowed, regardless of object bonus factor.
	LL_INLINE U32 getMaxTasks() const						{ return mMaxTasks; }
	LL_INLINE void setMaxTasks(U32 max_tasks)				{ mMaxTasks = max_tasks; }

	// Draw lines in the dirt showing ownership. Return number of
	// vertices drawn.
	void renderPropertyLines() const;
	// 'scale' is in pixels_per_meter. 'color' is a pointer on a LLColor4
	void renderParcelBorders(F32 scale, const F32* color) const;
	// Returns true when at least one banned parcel got drawn
	bool renderBannedParcels(F32 scale, const F32* color) const;

	// Call this whenever you change the height data in the region.
	// (Automatically called by LLSurfacePatch's update routine)
	void dirtyHeights();

	LL_INLINE LLViewerParcelOverlay* getParcelOverlay() const
	{
		return mParcelOverlay;
	}

	LL_INLINE void setRegionFlag(U64 flag, bool on)
	{
		if (on)
		{
			mRegionFlags |= flag;
		}
		else
		{
			mRegionFlags &= ~flag;
		}
	}

	LL_INLINE bool getRegionFlag(U64 flag) const			{ return (mRegionFlags & flag) != 0; }

	LL_INLINE void setRegionFlags(U64 flags)				{ mRegionFlags = flags; }
	LL_INLINE U64 getRegionFlags() const					{ return mRegionFlags; }

	LL_INLINE void setRegionProtocol(U64 protocol, bool on)
	{
		if (on)
		{
			mRegionProtocols |= protocol;
		}
		else
		{
			mRegionProtocols &= ~protocol;
		}
	}

	bool getRegionProtocol(U64 protocol) const				{ return (mRegionProtocols & protocol) != 0; }

	LL_INLINE void setRegionProtocols(U64 protocols)		{ mRegionProtocols = protocols; }
	LL_INLINE U64 getRegionProtocols() const				{ return mRegionProtocols; }

	LL_INLINE void setTimeDilation(F32 time_dilation)		{ mTimeDilation = time_dilation; }

	LL_INLINE F32 getTimeDilation() const					{ return mTimeDilation; }

	// Origin height is at zero.
	LL_INLINE const LLVector3d& getOriginGlobal() const		{ return mOriginGlobal; }
	LLVector3 getOriginAgent() const;

	// Center is at the height of the water table.
	LL_INLINE const LLVector3d& getCenterGlobal() const		{ return mCenterGlobal; }
	LLVector3 getCenterAgent() const;

	void setRegionNameAndZone(const std::string& name_and_zone);
	LL_INLINE const std::string& getName() const			{ return mName; }
	LL_INLINE const std::string& getZoning() const			{ return mZoning; }

	LL_INLINE void setOwner(const LLUUID& owner_id)			{ mOwnerID = owner_id; }
	LL_INLINE const LLUUID& getOwner() const				{ return mOwnerID; }

	// Is the current agent on the estate manager list for this region?
	LL_INLINE void setIsEstateManager(bool b)				{ mIsEstateManager = b; }
	LL_INLINE bool isEstateManager() const					{ return mIsEstateManager; }
	bool canManageEstate() const;

	LL_INLINE void setSimAccess(U8 sim_access)				{ mSimAccess = sim_access; }
	LL_INLINE U8 getSimAccess() const						{ return mSimAccess; }
	LL_INLINE const std::string getSimAccessString() const	{ return accessToString(mSimAccess); }

	// Homestead-related getters; there are no setters as nobody should be
	// setting them other than the individual message handler which is a member
	LL_INLINE S32 getSimClassID() const						{ return mClassID; }
	LL_INLINE S32 getSimCPURatio() const					{ return mCPURatio; }
	LL_INLINE const std::string& getSimColoName() const		{ return mColoName; }
	LL_INLINE const std::string& getSimProductSKU() const	{ return mProductSKU; }
	LL_INLINE const std::string& getSimProductName() const	{ return mProductName; }

	// Returns translated version of "Mature", "PG", "Adult", etc.
	static std::string accessToString(U8 sim_access);

	// Returns "M", "PG", "A" etc.
	static std::string accessToShortString(U8 sim_access);

	// Returns the maturity rating icon name for sim_access
	static const std::string& getMaturityIconName(U8 sim_access);

	// Used by LLVOCache once the cache has been read, to populate the cache
	// and signal that the handshake reply can be sent. HB
	static void cacheLoadedCallback(U64 region_handle,
									LLVOCacheEntry::map_t* cachep,
									LLVOCacheEntry::emap_t* extrasp);

	// Callback for the "RegionInfo" sim message. Fills up LLRegionInfoModel
	// variable members and calls interested parties (hard-coded "observers").
	static void processRegionInfo(LLMessageSystem* msg, void**);

	// Check if the viewer camera is static
	static bool isViewerCameraStatic();
	static void calcNewObjectCreationThrottle();

	LL_INLINE void setCacheID(const LLUUID& id)				{ mCacheID = id; }

	// Variable region size support
	LL_INLINE F32 getWidth() const							{ return mWidth; }

	LL_INLINE S32 getLastUpdate() const						{ return mLastUpdate; }

	void idleUpdate(F32 max_update_time);
	void lightIdleUpdate();
	bool addVisibleGroup(LLViewerOctreeGroup* group);
	void addVisibleChildCacheEntry(LLVOCacheEntry* parent,
								   LLVOCacheEntry* child);
	void addActiveCacheEntry(LLVOCacheEntry* entry);
	void removeActiveCacheEntry(LLVOCacheEntry* entry, LLDrawable* drawablep);
	// Physically delete the cache entry
	void killCacheEntry(U32 local_id);

	// Updates the land surface and parcel.
	void forceUpdate();

	void connectNeighbor(LLViewerRegion* neighborp, U32 direction);

	void updateNetStats();

	U32	getPacketsLost() const;
	LL_INLINE U32 getHttpResponderID() const				{ return mHttpResponderID; }

	// Get/set named capability URLs for this region.
	void setSeedCapability(const std::string& url);
	void setCapability(const std::string& name, const std::string& url);
	const std::string& getCapability(const char* name) const;
	const std::string& getTextureUrl() const;
	LL_INLINE const std::string& getViewerAssetUrl() const	{ return mViewerAssetUrl; }
	const std::string& getMeshUrl(bool* is_mesh2 = NULL) const;
	LL_INLINE size_t getCapabilitiesCount() const			{ return mCapabilities.size(); }

	// Has region received its final (not seed) capability list ?
	LL_INLINE bool capabilitiesReceived() const				{ return mCapabilitiesState == CAPABILITIES_STATE_RECEIVED; }
	void setCapabilitiesReceived(bool received);
	LL_INLINE void setCapabilitiesError()					{ mCapabilitiesState = CAPABILITIES_STATE_ERROR; }
	LL_INLINE bool capabilitiesError() const				{ return mCapabilitiesState == CAPABILITIES_STATE_ERROR; }
	LL_INLINE U32 getNumSeedCapRetries() const				{ return mSeedCapAttempts; }
	void onCapabilitiesReceived();
	boost::signals2::connection setCapsReceivedCB(const caps_received_cb_t& cb);
	boost::signals2::connection setFeaturesReceivedCB(const caps_received_cb_t& cb);

	static bool isSpecialCapabilityName(const std::string& name);
	void logActiveCapabilities() const;

	// When the "InterestList" capability is available, this method switches
	// the interest list mode to 360° or default for 'this' region, whenever it
	// happens to be the current agent region, and based on the corresponding
	// debug setting. Returns true on success to call the capability. When
	// set_default is true, the default interest list mode is forced (used when
	// leaving a region for another with 360° mode debug setting on).
	bool setInterestListMode(bool set_default = false) const;

	std::string getSimHostName();

	LL_INLINE const LLHost&	getHost() const					{ return mHost; }
	LL_INLINE U64 getHandle() const 						{ return mHandle; }

	LL_INLINE LLSurface& getLand() const					{ return *mLandp; }

	// Gets the region Id
	LL_INLINE const LLUUID& getRegionID() const				{ return mRegionID; }
	// Sets the region Id
	LL_INLINE void setRegionID(const LLUUID& region_id)		{ mRegionID = region_id; }

	// Returns a string with the region name (or the sim host IP when the
	// region name is empty) and, when not null, the region Id enclosed in
	// parentheses. Useful for log messages... HB
	std::string getIdentity() const;

	bool pointInRegionGlobal(const LLVector3d& point_global) const;

	LL_INLINE LLVector3 getPosRegionFromGlobal(const LLVector3d& pos) const
	{
		return LLVector3(pos - mOriginGlobal);
	}

	LL_INLINE LLVector3d getPosGlobalFromRegion(const LLVector3& offset) const
	{
		return LLVector3d(offset) + mOriginGlobal;
	}

	LL_INLINE LLVector3 getPosRegionFromAgent(const LLVector3& pos_agent) const
	{
		return pos_agent - getOriginAgent();
	}

	LLVector3 getPosAgentFromRegion(const LLVector3& region_pos) const;

	LL_INLINE LLVLComposition* getComposition() const		{ return mCompositionp; }
	F32 getCompositionXY(S32 x, S32 y) const;

	bool isOwnedSelf(const LLVector3& pos);

	// Owned by a group you belong to (as officer OR member) ?
	bool isOwnedGroup(const LLVector3& pos);

	// Deals with map object updates in the world.
	void updateCoarseLocations(LLMessageSystem* msg);

	F32 getLandHeightRegion(const LLVector3& region_pos);

	LL_INLINE U8 getCentralBakeVersion()					{ return mCentralBakeVersion; }

	void getInfo(LLSD& info);

	LL_INLINE bool getFeaturesReceived() const				{ return mFeaturesReceived; }

	LL_INLINE bool meshRezEnabled() const					{ return mMeshRezEnabled; }
	LL_INLINE bool meshUploadEnabled() const				{ return mMeshUploadEnabled; }
	LL_INLINE bool physicsShapeTypes() const				{ return mPhysicsShapeTypes; }
	LL_INLINE bool hasDynamicPathfinding() const			{ return mDynamicPathfinding; }
	LL_INLINE bool dynamicPathfindingEnabled() const		{ return mDynamicPathfindingEnabled; }
	LL_INLINE bool bakesOnMeshEnabled() const				{ return mBakesOnMeshEnabled; }
	LL_INLINE bool isOSExportPermSupported() const			{ return mOSExportPermSupported; }
	LL_INLINE bool avatarHoverHeightEnabled() const			{ return mHoverHeigthFeature; }
	LL_INLINE U32 getWhisperRange() const					{ return mWhisperRange; }
	LL_INLINE U32 getChatRange() const						{ return mChatRange; }
	LL_INLINE U32 getShoutRange() const						{ return mShoutRange; }

	LL_INLINE const LLSD& getSimulatorFeatures()			{ return mSimulatorFeatures; }
	void setSimulatorFeatures(const LLSD& info);

	typedef enum
	{
		CACHE_MISS_TYPE_FULL = 0,
		CACHE_MISS_TYPE_CRC,
		CACHE_MISS_TYPE_NONE
	} eCacheMissType;

	typedef enum
	{
		CACHE_UPDATE_DUPE = 0,
		CACHE_UPDATE_CHANGED,
		CACHE_UPDATE_ADDED,
		CACHE_UPDATE_REPLACED
	} eCacheUpdateResult;

	// Handles a full update message
	eCacheUpdateResult cacheFullUpdate(LLDataPackerBinaryBuffer& dp,
									   U32 flags);

	LL_INLINE eCacheUpdateResult cacheFullUpdate(LLViewerObject* objectp,
												 LLDataPackerBinaryBuffer& dp,
												 U32 flags)
	{
		return cacheFullUpdate(dp, flags);
	}

	void cacheFullUpdateGLTFOverride(const LLGLTFOverrideCacheEntry& data);

	LLVOCacheEntry* getCacheEntryForOctree(U32 local_id);
	LLVOCacheEntry* getCacheEntry(U32 local_id, bool valid = true);
	bool probeCache(U32 local_id, U32 crc, U32 flags, U8& cache_miss_type);
	void requestCacheMisses();
	void addCacheMissFull(U32 local_id);
	// Updates object cache if the object receives a full-update or terse
	// update:
	LLViewerObject* updateCacheEntry(U32 local_id, LLViewerObject* objectp);
	void findOrphans(U32 parent_id);
	void clearCachedVisibleObjects();
	void dumpCache();

	void unpackRegionHandshake();

	void calculateCenterGlobal();
	void calculateCameraDistance();

	friend std::ostream& operator<<(std::ostream& s, const LLViewerRegion& region);

	LL_INLINE U32 getNumOfVisibleGroups() const				{ return mVisibleGroups.size(); }
	LL_INLINE U32 getNumOfActiveCachedObjects() const		{ return mActiveSet.size(); }

	LLSpatialPartition* getSpatialPartition(U32 type);
	LLVOCachePartition* getVOCachePartition();

	bool objectIsReturnable(const LLVector3& pos,
							const std::vector<LLBBox>& boxes) const;
	bool childrenObjectReturnable(const std::vector<LLBBox>& boxes) const;

	void getNeighboringRegions(std::vector<LLViewerRegion*>& regions);
	void getNeighboringRegionsStatus(std::vector<S32>& regions);

	// Implements the materials capability throttle
	LL_INLINE bool materialsCapThrottled() const
	{
		return !mMaterialsCapThrottleTimer.hasExpired();
	}

	LL_INLINE void resetMaterialsCapThrottle()
	{
		mMaterialsCapThrottleTimer.resetWithExpiry(mRenderMaterialsCapability);
	}

	LL_INLINE U32 getMaxMaterialsPerTransaction() const
	{
		return mMaxMaterialsPerTransaction;
	}

	void removeFromCreatedList(U32 local_id);
	void addToCreatedList(U32 local_id);

	LL_INLINE bool isPaused() const							{ return mPaused; }

	LL_INLINE static bool isNewObjectCreationThrottleDisabled()
	{
		return sNewObjectCreationThrottle < 0;
	}

	// Rebuilds the reflection probe list
	void updateReflectionProbes();

	void dumpSettings();

	bool isEventPollInFlight() const;
	F32 getEventPollRequestAge() const;

private:
	// Call this after you have the region name and handle.
	void loadObjectCache();
	void saveObjectCache();

	void sendHandshakeReply();

	void addToVOCacheTree(LLVOCacheEntry* entry);
	LLViewerObject* addNewObject(LLVOCacheEntry* entry);
	void killObject(LLVOCacheEntry* entry,
					std::vector<LLDrawable*>& delete_list);
	void removeFromVOCacheTree(LLVOCacheEntry* entry);
	// Physically deletes the cache entry:
	void killCacheEntry(LLVOCacheEntry* entry, bool for_rendering = false);
	void killInvisibleObjects(F32 max_time);
	void createVisibleObjects(F32 max_time);
	void updateVisibleEntries();

	void addCacheMiss(U32 id, LLViewerRegion::eCacheMissType miss_type);
	void decodeBoundingInfo(LLVOCacheEntry* entry);
	bool isNonCacheableObjectCreated(U32 local_id);

	static void buildCapabilityNames(LLSD& capability_names);
	static void requestBaseCapabilitiesCoro(U64 region_handle);
	static void requestBaseCapabilitiesCompleteCoro(U64 region_handle);
	static void requestSimulatorFeatureCoro(std::string url,
											U64 region_handle);

public:
	void loadCacheMiscExtras(LLViewerObject* objp);
	void applyCacheMiscExtras(LLViewerObject* objp);

	struct CompareDistance
	{
		LL_INLINE bool operator()(const LLViewerRegion* const& lhs,
								  const LLViewerRegion* const& rhs) const
		{
			return lhs->mCameraDistanceSquared < rhs->mCameraDistanceSquared;
		}
	};

	struct CompareRegionByLastUpdate
	{
		LL_INLINE bool operator()(const LLViewerRegion* const& lhs,
								  const LLViewerRegion* const& rhs) const
		{
			S32 lpa = lhs->getLastUpdate();
			S32 rpa = rhs->getLastUpdate();
			// Small (older) mLastUpdate first
			if (lpa < rpa)
			{
				return true;
			}
			if (lpa > rpa)
			{
				return false;
			}
			return lhs < rhs;
		}
	};
	typedef std::set<LLViewerRegion*, CompareRegionByLastUpdate> prio_list_t;

	// *HACK: allow public access to these two methods in order to deal with
	// long teleports causing octree insertion failures (these are called from
	// the process_agent_movement_complete() function in llviewermessage.cpp).
	void initPartitions();
	void deletePartitions();

protected:
	void disconnectAllNeighbors();
	void initStats();

public:
	LLMatrix4				mRenderMatrix;

	LLViewerParcelOverlay*	mParcelOverlay;

	// These vectors are maintained in parallel. Ideally they would be combined
	// into a single array of an aggregate data type but for compatibility with
	// the old messaging system in which the previous message only sends and
	// parses the positions stored in the first array, they are maintained
	// separately until we stop supporting the old CoarseLocationUpdate message
	std::vector<U32>		mMapAvatars;
	uuid_vec_t				mMapAvatarIDs;

	LLWind					mWind;
	LLCloudLayer			mCloudLayer;

	LLStat					mBitStat;
	LLStat					mPacketsStat;
	LLStat					mPacketsLostStat;

	F32						mFirstWindLayerReceivedTime;
	bool					mGotClouds;

	static S32				sLastCameraUpdated;
	static bool				sVOCacheCullingEnabled;	// vo cache culling enabled or not.

private:
	LLEventPoll*			mEventPoll;

	// The surfaces and other layers
	LLSurface*				mLandp;

	// Composition layer for the surface
	LLVLComposition*		mCompositionp;

	// The materials capability throttle
	LLFrameTimer			mMaterialsCapThrottleTimer;

	// Simulator name
	std::string				mName;
	std::string				mZoning;

	// Region geometry data

	// Location of southwest corner of region (meters)
	LLVector3d				mOriginGlobal;
	// Location of center in world space (meters)
	LLVector3d				mCenterGlobal;
	// Variable region size support; width of region on a side (meters)
	F32						mWidth;

	// Simulator host data
	U64						mHandle;
	LLHost					mHost;
	std::string				mHostName;	// Taken from sim features data

	// The unique ID for this region.
	LLUUID					mRegionID;

	// Region/estate owner - usually null.
	LLUUID					mOwnerID;

	// Cache ID is unique per-region, across renames, moving locations, etc.
	LLUUID					mCacheID;

	F32						mCreationTime;

	// Time dilation of physics simulation on simulator
	F32						mTimeDilation;
	S32						mLastUpdate;	// Last call to idleUpdate()

	LLVOCacheEntry*			mLastVisitedEntry;
	U32						mInvisibilityCheckHistory;

	// Network statistics for the region's circuit...
	LLTimer					mLastNetUpdate;
	U32						mPacketsIn;
	U32						mBitsIn;
	U32						mLastBitsIn;
	U32						mLastPacketsIn;
	U32						mPacketsOut;
	U32						mLastPacketsOut;
	S32						mPacketsLost;
	S32						mLastPacketsLost;
	U32						mPingDelay;
	// Time since last measurement of lastPackets, Bits, etc
	F32						mDeltaTime;

	// Misc

	U64						mRegionFlags;		// Includes damage flags
	// Protocols supported by this region
	U64						mRegionProtocols;
	F32 					mBillableFactor;
	U32						mMaxTasks;				// Max prim count
	F32						mCameraDistanceSquared;	// Updated once per frame
	U8						mSimAccess;
	U8						mCentralBakeVersion;

	LLVOCacheEntry::emap_t	mGLTFOverrides;	// For GLTF materials
	// Maps local ids to cache entries.
	// Regions can have order 10,000 objects, so assume
	// a structure of size 2^14 = 16,384
	LLVOCacheEntry::map_t	mCacheMap;	// All cached entries
	LLVOCacheEntry::set_t	mActiveSet;	// All active entries
	// Entries waiting for LLDrawable to be generated:
	LLVOCacheEntry::set_t	mWaitingSet;
	LLVOCachePartition*		mVOCachePartition;
	// Must-be-created visible entries wait for objects creation:
	LLVOCacheEntry::set_t	mVisibleEntries;
	// Transient list storing sorted visible entries waiting for object
	// creation:
	LLVOCacheEntry::prio_list_t mWaitingList;
	// Visible groups
	typedef std::vector<LLPointer<LLViewerOctreeGroup> > visible_groups_vec_t;
	visible_groups_vec_t	mVisibleGroups;
	// List of local ids of all non-cacheable objects:
	typedef fast_hset<U32> non_cacheable_list_t;
	non_cacheable_list_t	mNonCacheableCreatedList;

	// List of reflection maps being managed by this region.
	LLReflectionMapManager::prmap_vec_t mReflectionMaps;

	typedef std::map<std::string, std::string> capability_map_t;
	capability_map_t		mCapabilities;

	U32						mSeedCapAttempts;
	U32						mHttpResponderID;

	caps_received_sig_t		mCapabilitiesReceivedSignal;
	caps_received_sig_t		mFeaturesReceivedSignal;

	typedef enum
	{
		CAPABILITIES_STATE_INIT = 0,
		CAPABILITIES_STATE_ERROR,
		CAPABILITIES_STATE_RECEIVED
	} eCababilitiesState;

	eCababilitiesState		mCapabilitiesState;

	U32						mPendingHandshakes;

	bool					mFeaturesReceived;

	// Is this agent on the estate managers list for this region ?
	bool					mIsEstateManager;

	bool					mCacheLoading;
	bool					mCacheLoaded;
	bool					mCacheDirty;

	// Can become false if circuit disconnects
	bool					mAlive;

	bool					mDead;
	bool					mPaused;

	// Variables mirroring the data from mSimulatorFeatures, for quick access

	bool					mMeshRezEnabled;
	bool					mMeshUploadEnabled;
	bool					mPhysicsShapeTypes;
	bool					mDynamicPathfinding;
	bool					mDynamicPathfindingEnabled;
	bool					mBakesOnMeshEnabled;
	bool					mOSExportPermSupported;
	bool					mHoverHeigthFeature;

	U32						mWhisperRange;
	U32						mChatRange;
	U32						mShoutRange;
	U32						mMaxMaterialsPerTransaction;
	F32						mRenderMaterialsCapability;

	LLVector3				mLastCameraOrigin;
	U32						mLastCameraUpdate;

	// Information for Homestead / CR-53
	S32						mClassID;
	S32						mCPURatio;
	std::string				mColoName;
	std::string				mProductSKU;
	std::string				mProductName;

	// Validated (trailing "/" added if missing from the caps), cached caps
	std::string				mGetTextureUrl;
	std::string				mGetMeshUrl;
	std::string				mGetMesh2Url;
	std::string				mViewerAssetUrl;

	typedef std::map<U32, std::vector<U32> > orphan_list_t;
	orphan_list_t			mOrphanMap;

	class CacheMissItem
	{
	public:
		CacheMissItem(U32 id, LLViewerRegion::eCacheMissType miss_type)
		:	mID(id),
			mType(miss_type)
		{
		}

		typedef std::list<CacheMissItem> cache_miss_list_t;

	public:
		U32								mID;     // Local object id
		LLViewerRegion::eCacheMissType	mType;   // Cache miss type
	};
	CacheMissItem::cache_miss_list_t		mCacheMissList;

	// Spatial partitions for objects in this region
	std::vector<LLViewerOctreePartition*>	mObjectPartition;

	LLSD									mSimulatorFeatures;

	static LLCore::HttpHeaders::ptr_t		sHttpHeaders;
	static LLCore::HttpOptions::ptr_t		sHttpOptions;
	static S32								sNewObjectCreationThrottle;
};

// Purely static class (a singleton in LL's viewer, but LL coders love to
// complicate and slow down things for no reason)... It is used to store data
// for the agent region (as such, it could as well be part of LLAgent) and its
// member variables are filled up by LLViewerRegion::processRegionInfo(). HB
class LLRegionInfoModel
{
	LLRegionInfoModel() = delete;
	~LLRegionInfoModel() = delete;

public:
	LL_INLINE static void setUseFixedSun(bool fixed)
	{
		if (fixed)
		{
			sRegionFlags |= REGION_FLAGS_SUN_FIXED;
		}
		else
		{
			sRegionFlags &= ~REGION_FLAGS_SUN_FIXED;
		}
	}

	LL_INLINE static bool getUseFixedSun()
	{
		return (sRegionFlags & REGION_FLAGS_SUN_FIXED) != 0;
	}

public:
	static U64			sRegionFlags;
	static U32			sEstateID;
	static U32			sParentEstateID;

	static S32			sPricePerMeter;

	static S32			sRedirectGridX;
	static S32			sRedirectGridY;

	static std::string	sSimName;
	static std::string	sSimType;

	static F32			sBillableFactor;
	static F32			sObjectBonusFactor;
	static F32			sWaterHeight;
	static F32			sTerrainRaiseLimit;
	static F32			sTerrainLowerLimit;
	static F32			sSunHour;			// 6... 30

	static S32			sHardAgentLimit;
	static U8			sSimAccess;
	static U8			sAgentLimit;

	static bool			sUseEstateSun;
};

#endif
