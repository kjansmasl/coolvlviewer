/**
 * @file llviewerregion.cpp
 * @brief Implementation of the LLViewerRegion class.
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

#include "llviewerprecompiledheaders.h"

#include "llviewerregion.h"

#include "llapp.h"
#include "llbbox.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llhost.h"
#include "llhttpnode.h"
#include "llregionhandle.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llsurface.h"
#include "lltrans.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"				// For gFrameTimeSeconds
#include "llavatartracker.h"
#include "llcommandhandler.h"
#include "llenvironment.h"
#include "lleventpoll.h"
#include "llfloatergodtools.h"
#include "llfloaterregioninfo.h"
#include "hbfloatersearch.h"
#include "llgltfmateriallist.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llselectmgr.h"				// For dialog_refresh_all()
#include "llspatialpartition.h"
#include "llstartup.h"
#include "llurldispatcher.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"			// For gTeleportDisplay
#include "llviewerobjectlist.h"
#include "llvieweroctree.h"
#include "llviewerparcelmgr.h"
#include "llviewerparceloverlay.h"
#include "llviewerwindow.h"				// For getProgressView()
#include "llvlcomposition.h"
#include "llvlmanager.h"
#include "llvoavatarself.h"
#include "llvoclouds.h"
#include "llworld.h"
#include "llworldmap.h"

// The server only keeps our pending agent info for 60 seconds. We want to
// allow for seed cap retry, but its not useful after that 60 seconds. Even
// when we gave up on login, keep trying for caps after we are logged in.
constexpr U32 MAX_CAP_REQUEST_ATTEMPTS = 30;
constexpr U32 DEFAULT_MAX_REGION_WIDE_PRIM_COUNT = 15000;

bool LLViewerRegion::sVOCacheCullingEnabled = false;
S32 LLViewerRegion::sLastCameraUpdated = 0;
S32 LLViewerRegion::sNewObjectCreationThrottle = -1;

// NOTE: by using these instead of omitting the corresponding xxxAndSuspend()
// parameters, we avoid seeing such classes constructed and destroyed each time
LLCore::HttpHeaders::ptr_t LLViewerRegion::sHttpHeaders(new LLCore::HttpHeaders());
LLCore::HttpOptions::ptr_t LLViewerRegion::sHttpOptions(new LLCore::HttpOptions());

U64 LLRegionInfoModel::sRegionFlags = 0;
U32 LLRegionInfoModel::sEstateID = 0;
U32 LLRegionInfoModel::sParentEstateID = 0;
S32 LLRegionInfoModel::sPricePerMeter = 0;
S32 LLRegionInfoModel::sRedirectGridX = 0;
S32 LLRegionInfoModel::sRedirectGridY = 0;
F32 LLRegionInfoModel::sBillableFactor = 0.f;
F32 LLRegionInfoModel::sObjectBonusFactor = 0.f;
F32 LLRegionInfoModel::sWaterHeight = 0.f;
F32 LLRegionInfoModel::sTerrainRaiseLimit = 0.f;
F32 LLRegionInfoModel::sTerrainLowerLimit = 0.f;
F32 LLRegionInfoModel::sSunHour = 0.f;
S32 LLRegionInfoModel::sHardAgentLimit = 0;
U8 LLRegionInfoModel::sSimAccess = 0;
U8 LLRegionInfoModel::sAgentLimit = 0;
bool LLRegionInfoModel::sUseEstateSun = false;
std::string LLRegionInfoModel::sSimName;
std::string LLRegionInfoModel::sSimType;

// Support for secondlife:///app/region/{REGION} SLapps
// N.B. this is defined to work exactly like the classic secondlife://{REGION}
// However, the later syntax cannot support spaces in the region name because
// spaces (and %20 chars) are illegal in the hostname of an http URL. Some
// browsers let you get away with this, but some do not.
// Hence we introduced the newer secondlife:///app/region alternative.
class LLRegionHandler final : public LLCommandHandler
{
public:
	// requests will be throttled from a non-trusted browser
	LLRegionHandler()
	:	LLCommandHandler("region", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl* web) override
	{
		// Make sure that we at least have a region name
		S32 num_params = params.size();
		if (num_params < 1)
		{
			return false;
		}

		// Build a secondlife://{PLACE} SLurl from this SLapp
		std::string url = "secondlife://";
		for (S32 i = 0; i < num_params; ++i)
		{
			if (i > 0)
			{
				url += "/";
			}
			url += params[i].asString();
		}

		// Process the SLapp as if it was a secondlife://{PLACE} SLurl
		LLURLDispatcher::dispatch(url, "clicked", web, true);
		return true;
	}
};
LLRegionHandler gRegionHandler;

LLViewerRegion::LLViewerRegion(const U64& handle, const LLHost& host,
							   U32 grids_per_region_edge,
							   U32 grids_per_patch_edge,
							   F32 region_width_meters)
:	mHandle(handle),
	mHost(host),
	mTimeDilation(1.0f),
	mLastUpdate(0),
	mCreationTime(gFrameTimeSeconds),
	mIsEstateManager(false),
	mRegionFlags(REGION_FLAGS_DEFAULT),
	mRegionProtocols(0),
	mCentralBakeVersion(0),
	mSimAccess(SIM_ACCESS_MIN),
	mBillableFactor(1.0),
	mMaxTasks(DEFAULT_MAX_REGION_WIDE_PRIM_COUNT),
	mClassID(0),
	mCPURatio(0),
	mColoName("unknown"),
	mProductSKU("unknown"),
	mProductName("unknown"),
	mCacheLoading(false),
	mCacheLoaded(false),
	mCacheDirty(false),
	mPendingHandshakes(0),
	mLastCameraUpdate(0),
	mLastCameraOrigin(),
	mEventPoll(NULL),
	mSeedCapAttempts(0),
	mHttpResponderID(0),
	mCapabilitiesState(CAPABILITIES_STATE_INIT),
	mFeaturesReceived(false),
	mDead(false),
	mLastVisitedEntry(NULL),
	mInvisibilityCheckHistory(-1),
	mPaused(false),
	mMeshRezEnabled(false),
	mMeshUploadEnabled(false),
	mPhysicsShapeTypes(false),
	mDynamicPathfinding(false),
	mDynamicPathfindingEnabled(false),
	mBakesOnMeshEnabled(false),
	mOSExportPermSupported(false),
	mHoverHeigthFeature(false),
	mWhisperRange((U32)CHAT_WHISPER_RADIUS),
	mChatRange((U32)CHAT_NORMAL_RADIUS),
	mShoutRange((U32)CHAT_SHOUT_RADIUS),
	mMaxMaterialsPerTransaction(50),			// Original hard coded default
	mRenderMaterialsCapability(1.f),			// Original hard coded default
	mFirstWindLayerReceivedTime(0.f),
	mGotClouds(false),
	mWidth(region_width_meters)					// Variable region size support
{
	mOriginGlobal = from_region_handle(handle);
	updateRenderMatrix();

	mLandp = new LLSurface('l', NULL);

	// Create the composition layer for the surface
	mCompositionp = new LLVLComposition(mLandp, grids_per_region_edge,
										region_width_meters / grids_per_region_edge);
	mCompositionp->setSurface(mLandp);

	// Create the surfaces
	mLandp->setRegion(this);
	mLandp->create(grids_per_region_edge, grids_per_patch_edge, mOriginGlobal,
				   mWidth);

	mParcelOverlay = new LLViewerParcelOverlay(this, region_width_meters);
	gViewerParcelMgr.setRegionWidth(region_width_meters);

	mWind.setRegionWidth(region_width_meters);

	mCloudLayer.create(this);
	mCloudLayer.setWindPointer(&mWind);

	setOriginGlobal(from_region_handle(handle));
	calculateCenterGlobal();

	// Create the object lists
	initStats();

	initPartitions();
}

void LLViewerRegion::initStats()
{
	mLastNetUpdate.reset();
	mPacketsIn = 0;
	mBitsIn = 0;
	mLastBitsIn = 0;
	mLastPacketsIn = 0;
	mPacketsOut = 0;
	mLastPacketsOut = 0;
	mPacketsLost = 0;
	mLastPacketsLost = 0;
	mPingDelay = 0;
	mAlive = false;					// can become false if circuit disconnects
}

// Creates object partitions. MUST MATCH declaration of eObjectPartitions
void LLViewerRegion::initPartitions()
{
	mObjectPartition.push_back(new LLHUDPartition(this));			//PARTITION_HUD
	mObjectPartition.push_back(new LLTerrainPartition(this));		//PARTITION_TERRAIN
	mObjectPartition.push_back(new LLVoidWaterPartition(this));		//PARTITION_VOIDWATER
	mObjectPartition.push_back(new LLWaterPartition(this));			//PARTITION_WATER
	mObjectPartition.push_back(new LLTreePartition(this));			//PARTITION_TREE
	mObjectPartition.push_back(new LLParticlePartition(this));		//PARTITION_PARTICLE
	mObjectPartition.push_back(new LLCloudPartition(this));			//PARTITION_CLOUD
	mObjectPartition.push_back(new LLGrassPartition(this));			//PARTITION_GRASS
	mObjectPartition.push_back(new LLVolumePartition(this));		//PARTITION_VOLUME
	mObjectPartition.push_back(new LLBridgePartition(this));		//PARTITION_BRIDGE
	mObjectPartition.push_back(new LLAvatarPartition(this));		//PARTITION_AVATAR
	mObjectPartition.push_back(new LLPuppetPartition(this));		//PARTITION_PUPPET
	mObjectPartition.push_back(new LLHUDParticlePartition(this));	//PARTITION_HUD_PARTICLE
	mObjectPartition.push_back(new LLVOCachePartition(this));		//PARTITION_VO_CACHE
	mObjectPartition.push_back(NULL);								//PARTITION_NONE
	mVOCachePartition = getVOCachePartition();
}

void LLViewerRegion::deletePartitions()
{
	std::for_each(mObjectPartition.begin(), mObjectPartition.end(),
				  DeletePointer());
	mObjectPartition.clear();
}

LLViewerRegion::~LLViewerRegion()
{
	mDead = true;
	mActiveSet.clear();
	mVisibleEntries.clear();
	mVisibleGroups.clear();
	mWaitingSet.clear();

	gVLManager.cleanupData(this);

	// Cannot do this on destruction, because the neighbor pointers might be
	// invalid. This should be reference counted...
	disconnectAllNeighbors();

	mCloudLayer.destroy();

	gViewerPartSim.cleanupRegion(this);

	gObjectList.killObjects(this);

	delete mCompositionp;
	delete mParcelOverlay;
	delete mLandp;
	delete mEventPoll;

	deletePartitions();

	saveObjectCache();
}

//static
void LLViewerRegion::cacheLoadedCallback(U64 region_handle,
										 LLVOCacheEntry::map_t* cachep,
										 LLVOCacheEntry::emap_t* extrasp)
{
	LLViewerRegion* self = gWorld.getRegionFromHandle(region_handle);
	if (self && !self->mDead && !LLApp::isExiting())
	{
		LL_DEBUGS("ObjectCache") << "Cache loaded callback for region: "
								 << self->mName << " (handle " << region_handle
								 << ")" << LL_ENDL;
		self->mCacheLoading = false;

		// Recover the cache data, if any.
		if (!cachep || cachep->empty())
		{
			self->mCacheDirty = true;
		}
		else
		{
			self->mCacheMap.swap(*cachep);
		}
		if (extrasp && !extrasp->empty())
		{
			self->mGLTFOverrides.swap(*extrasp);
		}

		// Reply to the pending handshake(s) now.
		while (self->mPendingHandshakes)
		{
			self->sendHandshakeReply();
		}
	}
	else if (!LLApp::isExiting())
	{
		llwarns << "Skipping for region handle " << region_handle
				<< (self ? ": region not found." : " region is dead.")
				<< llendl;
	}
	if (cachep)
	{
		delete cachep;
	}
	if (extrasp)
	{
		delete extrasp;
	}
}

void LLViewerRegion::loadObjectCache()
{
	if (!mCacheLoaded)
	{
		// Pretend it is already loaded so that it does not get queued twice
		// or more (we do get several handshakes for the same region in SL). HB
		mCacheLoaded = true;

		if (LLVOCache::instanceExists())
		{
			llinfos << "Loading object cache for region: " << mName << llendl;
			mCacheLoading = true;
			LLVOCache::getInstance()->readFromCache(mHandle, mName, mCacheID);
		}
	}
}

void LLViewerRegion::saveObjectCache()
{
	if (!mCacheLoaded)
	{
		LL_DEBUGS("ObjectCache") << "Cache map not loaded for region: "
								 << mName << ". Skiping." << LL_ENDL;
		return;
	}
	if (mCacheMap.empty())
	{
		LL_DEBUGS("ObjectCache") << "Cache map empty for region: " << mName
								 << ". Skiping." << LL_ENDL;
		return;
	}

	if (LLVOCache::instanceExists())
	{
		LL_DEBUGS("ObjectCache") << "Saving object cache for region: " << mName
								 << LL_ENDL;

		constexpr F32 THRESHOLD = 600.f;	// Seconds
		bool removal_enabled =
			sVOCacheCullingEnabled &&
			(LLApp::isExiting() ||
			 mCreationTime - gFrameTimeSeconds > THRESHOLD);
		// Note: mCacheMap and/or mGLTFOverrides may be wiped out (for
		// speed, they are actually swapped with an empty map on successful
		// cache write) by this call. So they cannot be reused afterwards, but
		// this is OK, since we are going to destroy it: saveObjectCache() is
		// for now only called at the end of ~LLViewerRegion() (should this
		// change, the swap optimization would have to be removed and replaced
		// with a copy). HB
		LLVOCache::getInstance()->writeToCache(mHandle, mName, mCacheID,
											   mCacheMap, mCacheDirty,
											   mGLTFOverrides,
											   removal_enabled);
	}
	mCacheDirty = false;
}

void LLViewerRegion::sendMessage()
{
	gMessageSystemp->sendMessage(mHost);
}

void LLViewerRegion::sendReliableMessage()
{
	gMessageSystemp->sendReliable(mHost);
}

void LLViewerRegion::sendEstateCovenantRequest()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg)	// Paranoia
	{
		msg->newMessage(_PREHASH_EstateCovenantRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID,	gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->sendReliable(mHost);
	}
}

void LLViewerRegion::setWaterHeight(F32 water_level)
{
	mLandp->setWaterHeight(water_level);
}

F32 LLViewerRegion::getWaterHeight() const
{
	return mLandp->getWaterHeight();
}

void LLViewerRegion::setOriginGlobal(const LLVector3d& origin_global)
{
	mOriginGlobal = origin_global;
	updateRenderMatrix();
	mLandp->setOriginGlobal(origin_global);
	mWind.setOriginGlobal(origin_global);
	mCloudLayer.setOriginGlobal(origin_global);
	calculateCenterGlobal();
}

void LLViewerRegion::updateRenderMatrix()
{
	mRenderMatrix.setTranslation(getOriginAgent());
}

LLVector3 LLViewerRegion::getOriginAgent() const
{
	return gAgent.getPosAgentFromGlobal(mOriginGlobal);
}

LLVector3 LLViewerRegion::getCenterAgent() const
{
	return gAgent.getPosAgentFromGlobal(mCenterGlobal);
}

void LLViewerRegion::setRegionNameAndZone(const std::string& name_zone)
{
	std::string::size_type pipe_pos = name_zone.find('|');
	if (pipe_pos != std::string::npos)
	{
		size_t length = name_zone.size();
		mName = name_zone.substr(0, pipe_pos);
		mZoning = name_zone.substr(pipe_pos + 1, length - pipe_pos - 1);
	}
	else
	{
		mName = name_zone;
		mZoning.clear();
	}

	LLStringUtil::stripNonprintable(mName);
	LLStringUtil::stripNonprintable(mZoning);

	if (mEventPoll)
	{
		mEventPoll->setRegionName(mName);
	}
}

bool LLViewerRegion::canManageEstate() const
{
	return gAgent.isGodlike() || isEstateManager() || gAgentID == getOwner();
}

//static
std::string LLViewerRegion::accessToString(U8 sim_access)
{
	static std::string access_pg	 = LLTrans::getString("SIM_ACCESS_PG");
	static std::string access_mature = LLTrans::getString("SIM_ACCESS_MATURE");
	static std::string access_adult	 = LLTrans::getString("SIM_ACCESS_ADULT");
	static std::string access_down	 = LLTrans::getString("SIM_ACCESS_DOWN");
	static std::string access_min	 = LLTrans::getString("unknown");

	switch (sim_access)
	{
		case SIM_ACCESS_PG:
			return access_pg;

		case SIM_ACCESS_MATURE:
			return access_mature;

		case SIM_ACCESS_ADULT:
			return access_adult;

		case SIM_ACCESS_DOWN:
			return access_down;

		case SIM_ACCESS_MIN:
		default:
			return access_min;
	}
}

//static
std::string LLViewerRegion::accessToShortString(U8 sim_access)
{
	switch (sim_access)
	{
		case SIM_ACCESS_PG:
			return "PG";

		case SIM_ACCESS_MATURE:
			return "M";

		case SIM_ACCESS_ADULT:
			return "A";

		case SIM_ACCESS_MIN:
		default:
			return "U";
	}
}

//static
const std::string& LLViewerRegion::getMaturityIconName(U8 sim_access)
{
	static const std::string pg = "access_pg.tga";
	static const std::string mature = "access_mature.tga";
	static const std::string adult = "access_adult.tga";

	if (sim_access <= SIM_ACCESS_PG)
	{
		return pg;
	}
	if (sim_access <= SIM_ACCESS_MATURE)
	{
		return mature;
	}
	return adult;
}

//static
void LLViewerRegion::processRegionInfo(LLMessageSystem* msg, void**)
{
	if (!msg) return;

	std::string sim_name;
	msg->getString(_PREHASH_RegionInfo, _PREHASH_SimName, sim_name);
	F32 water_height;
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_WaterHeight, water_height);
	if (msg->getSender() != gAgent.getRegionHost())
	{
		// Update is for a different region than the one we are in.
		// Just check for a waterheight change.
		gWorld.waterHeightRegionInfo(sim_name, water_height);
		return;
	}
	LLRegionInfoModel::sSimName = sim_name;
	LLRegionInfoModel::sWaterHeight = water_height;

	msg->getU32Fast(_PREHASH_RegionInfo, _PREHASH_EstateID,
					LLRegionInfoModel::sEstateID);
	msg->getU32Fast(_PREHASH_RegionInfo, _PREHASH_ParentEstateID,
					LLRegionInfoModel::sParentEstateID);
	msg->getU8Fast(_PREHASH_RegionInfo, _PREHASH_SimAccess,
				   LLRegionInfoModel::sSimAccess);
	msg->getU8Fast(_PREHASH_RegionInfo, _PREHASH_MaxAgents,
				   LLRegionInfoModel::sAgentLimit);
	LLRegionInfoModel::sHardAgentLimit = 0;
	msg->getS32(_PREHASH_RegionInfo2, _PREHASH_HardMaxAgents,
				LLRegionInfoModel::sHardAgentLimit);
	if (!LLRegionInfoModel::sHardAgentLimit)
	{
		LLRegionInfoModel::sHardAgentLimit = 100;
	}

	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_ObjectBonusFactor,
					LLRegionInfoModel::sObjectBonusFactor);
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_BillableFactor,
					LLRegionInfoModel::sBillableFactor);
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_TerrainRaiseLimit,
					LLRegionInfoModel::sTerrainRaiseLimit);
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_TerrainLowerLimit,
					LLRegionInfoModel::sTerrainLowerLimit);
	msg->getS32Fast(_PREHASH_RegionInfo, _PREHASH_PricePerMeter,
					LLRegionInfoModel::sPricePerMeter);
	msg->getS32Fast(_PREHASH_RegionInfo, _PREHASH_RedirectGridX,
					LLRegionInfoModel::sRedirectGridX);
	msg->getS32Fast(_PREHASH_RegionInfo, _PREHASH_RedirectGridY,
					LLRegionInfoModel::sRedirectGridY);

	msg->getBool(_PREHASH_RegionInfo, _PREHASH_UseEstateSun,
				 LLRegionInfoModel::sUseEstateSun);

	// Actually the "last set" Sun hour, not the current Sun hour.
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_SunHour,
				LLRegionInfoModel::sSunHour);

	// The only reasonable way to decide if we actually have any data is to
	// check to see if any of these fields have nonzero sizes
	std::string sim_type = LLTrans::getString("unknown");
	if (msg->getSize(_PREHASH_RegionInfo2, _PREHASH_ProductSKU) > 0 ||
		msg->getSize(_PREHASH_RegionInfo2, _PREHASH_ProductName) > 0)
	{
		msg->getString(_PREHASH_RegionInfo2, _PREHASH_ProductName,
					   LLRegionInfoModel::sSimType);
	}
	else
	{
		LLRegionInfoModel::sSimType.clear();
	}

	if (msg->has(_PREHASH_RegionInfo3))
	{
		msg->getU64(_PREHASH_RegionInfo3, _PREHASH_RegionFlagsExtended,
					LLRegionInfoModel::sRegionFlags);
	}
	else
	{
		U32 flags = 0;
		msg->getU32(_PREHASH_RegionInfo, _PREHASH_RegionFlags, flags);
		LLRegionInfoModel::sRegionFlags = flags;
	}

	// Push values to the agent region
	LLViewerRegion* self = gAgent.getRegion();
	if (self)
	{
		self->setRegionNameAndZone(sim_name);
		self->setRegionFlags(LLRegionInfoModel::sRegionFlags);
		self->setSimAccess(LLRegionInfoModel::sSimAccess);
		self->setWaterHeight(LLRegionInfoModel::sWaterHeight);
		self->setBillableFactor(LLRegionInfoModel::sBillableFactor);
	}

	// Send the region info update notification to interested parties.
	LLFloaterRegionInfo::updateFromRegionInfo();
	LLFloaterGodTools::updateFromRegionInfo();

	// *TODO: this frequently results in one more request than we need. It is
	// not breaking, but should be nicer. We need to know new env version to
	// fix this, without it we can only do full re-request. They happens on
	// updates, on opening LLFloaterRegionInfo, on region crossing if info
	// floater is open.
	LLEnvironment::requestRegion();
}

void LLViewerRegion::renderPropertyLines() const
{
	if (mParcelOverlay)
	{
		mParcelOverlay->renderPropertyLines();
	}
}

void LLViewerRegion::renderParcelBorders(F32 scale, const F32* color) const
{
	if (mParcelOverlay)
	{
		mParcelOverlay->renderParcelBorders(scale, color);
	}
}

bool LLViewerRegion::renderBannedParcels(F32 scale, const F32* color) const
{
	return mParcelOverlay && mParcelOverlay->renderBannedParcels(scale, color);
}

// This gets called when the height field changes.
void LLViewerRegion::dirtyHeights()
{
	// Property lines need to be reconstructed when the land changes.
	if (mParcelOverlay)
	{
		mParcelOverlay->setDirty();
	}
}

// Physically delete the cache entry
void LLViewerRegion::killCacheEntry(LLVOCacheEntry* entry, bool for_rendering)
{
	if (!entry|| !entry->isValid())
	{
		return;
	}

	if (for_rendering && !entry->isState(LLVOCacheEntry::ACTIVE))
	{
		addNewObject(entry);	// Force to add to rendering pipeline
	}

	// Remove from active list and waiting list
	if (entry->isState(LLVOCacheEntry::ACTIVE))
	{
		mActiveSet.erase(entry);
	}
	else
	{
		if (entry->isState(LLVOCacheEntry::WAITING))
		{
			mWaitingSet.erase(entry);
		}

		// Remove from mVOCachePartition
		removeFromVOCacheTree(entry);
	}

	// Remove from the forced visible list
	mVisibleEntries.erase(entry);

	// Disconnect from parent if it is a child
	if (entry->getParentID() > 0)
	{
		LLVOCacheEntry* parent = getCacheEntry(entry->getParentID());
		if (parent)
		{
			parent->removeChild(entry);
		}
	}
	else if (entry->getNumOfChildren() > 0)
	{
		LLVOCacheEntry* child = entry->getChild();
		while (child)
		{
			killCacheEntry(child, for_rendering);
			child = entry->getChild();
		}
	}

	// Will remove it from the object cache, real deletion
	entry->setState(LLVOCacheEntry::INACTIVE);
	entry->removeOctreeEntry();
	entry->setValid(false);
}

// Physically delete the cache entry
void LLViewerRegion::killCacheEntry(U32 local_id)
{
	killCacheEntry(getCacheEntry(local_id));
}

void LLViewerRegion::addActiveCacheEntry(LLVOCacheEntry* entry)
{
	if (!entry || mDead ||
		// Ignore if already inserted
		entry->isState(LLVOCacheEntry::ACTIVE))
	{
		return;
	}

	if (entry->isState(LLVOCacheEntry::WAITING))
	{
		mWaitingSet.erase(entry);
	}

	entry->setState(LLVOCacheEntry::ACTIVE);
	entry->setVisible();

	llassert(entry->getEntry()->hasDrawable());
	mActiveSet.insert(entry);
}

void LLViewerRegion::removeActiveCacheEntry(LLVOCacheEntry* entry,
											LLDrawable* drawablep)
{
	if (mDead || !entry || !entry->isValid() ||
		// Ignore if not an active entry
		!entry->isState(LLVOCacheEntry::ACTIVE))
	{
		return;
	}

	// Shift to the local regional space from agent space
	if (drawablep && drawablep->getVObj().notNull())
	{
		const LLVector3& pos = drawablep->getVObj()->getPositionRegion();
		LLVector4a shift;
		shift.load3(pos.mV);
		shift.sub(entry->getPositionGroup());
		entry->shift(shift);
	}

	if (entry->getParentID() > 0) // is a child
	{
		LLVOCacheEntry* parent = getCacheEntry(entry->getParentID());
		if (parent)
		{
			parent->addChild(entry);
		}
		else
		{
			// Parent not in cache (happens only when it is not cacheable)
			mOrphanMap[entry->getParentID()].push_back(entry->getLocalID());
		}
	}
	else
	{
		// Insert to vo cache tree.
		entry->updateParentBoundingInfo();
		entry->saveBoundingSphere();
		addToVOCacheTree(entry);
	}

	mVisibleEntries.erase(entry);
	mActiveSet.erase(entry);
	mWaitingSet.erase(entry);
	entry->setState(LLVOCacheEntry::INACTIVE);
}

bool LLViewerRegion::addVisibleGroup(LLViewerOctreeGroup* group)
{
	if (mDead || group->isEmpty())
	{
		return false;
	}

	mVisibleGroups.push_back(group);

	return true;
}

void LLViewerRegion::updateReflectionProbes()
{
	constexpr F32 HOVER_HEIGHT = 2.f;
	constexpr F32 PROBE_SPACING = 32.f;
	constexpr F32 HALF_PROBE_SPACING = PROBE_SPACING * 0.5f;
	constexpr F32 START = HALF_PROBE_SPACING;
	static const F32 PROBE_RADIUS = sqrtf(HALF_PROBE_SPACING *
										  HALF_PROBE_SPACING * 3.f);
	// Using mWidth here instead of REGION_WIDTH_METERS for Variable region
	// size support. HB
	U32 grid_width = mWidth / PROBE_SPACING;

	mReflectionMaps.resize(grid_width * grid_width);

	F32 water_height = getWaterHeight();
	LLVector3 origin = getOriginAgent();

	for (U32 i = 0; i < grid_width; ++i)
	{
		F32 x = i * PROBE_SPACING + START;
		for (U32 j = 0; j < grid_width; ++j)
		{
			F32 y = j * PROBE_SPACING + START;
			U32 idx = i * grid_width + j;
			if (mReflectionMaps[idx].isNull())
			{
				mReflectionMaps[idx] =
					gPipeline.mReflectionMapManager.addProbe();
			}
			LLVector3 pos(x, y,
						  HOVER_HEIGHT +
						  llmax(water_height,
								mLandp->resolveHeightRegion(x, y)));
			pos += origin;
			mReflectionMaps[idx]->mOrigin.load3(pos.mV);
			mReflectionMaps[idx]->mRadius = PROBE_RADIUS;
		}
	}
}

void LLViewerRegion::addToVOCacheTree(LLVOCacheEntry* entry)
{
	if (entry && sVOCacheCullingEnabled && !mDead &&
		entry->getEntry() && entry->isValid() &&
		entry->getParentID() == 0 &&					// Child in octree
		!entry->hasState(LLVOCacheEntry::IN_VO_TREE))	// Not in octree
	{
		llassert_always(!entry->getGroup());			// Not in octree.
		llassert(!entry->getEntry()->hasDrawable());	// No drawable

		if (mVOCachePartition->addEntry(entry->getEntry()))
		{
			entry->setState(LLVOCacheEntry::IN_VO_TREE);
		}
	}
}

void LLViewerRegion::removeFromVOCacheTree(LLVOCacheEntry* entry)
{
	if (entry && !mDead && entry->getEntry() &&
		// Must be in tree
		entry->hasState(LLVOCacheEntry::IN_VO_TREE))
	{
		entry->clearState(LLVOCacheEntry::IN_VO_TREE);
		mVOCachePartition->removeEntry(entry->getEntry());
	}
}

// Add child objects as visible entries
void LLViewerRegion::addVisibleChildCacheEntry(LLVOCacheEntry* parent,
											   LLVOCacheEntry* child)
{
	if (mDead)
	{
		return;
	}

 	if (parent &&
		(!parent->isValid() || !parent->isState(LLVOCacheEntry::ACTIVE)))
	{
		// Parent must be valid and in rendering pipeline
		return;
	}

	if (child &&
		(!child->getEntry() || !child->isValid() ||
		 !child->isState(LLVOCacheEntry::INACTIVE)))
	{
		// Child must be valid and not in the rendering pipeline
		return;
	}

	if (child)
	{
		child->setState(LLVOCacheEntry::IN_QUEUE);
		mVisibleEntries.insert(child);
	}
	else if (parent && parent->getNumOfChildren() > 0)
	{
		// Add all children
		while ((child = parent->getChild()))
		{
			addVisibleChildCacheEntry(NULL, child);
		}
	}
}

void LLViewerRegion::updateVisibleEntries()
{
	if (mDead || !sNewObjectCreationThrottle ||
		(mVisibleGroups.empty() && mVisibleEntries.empty()))
	{
		return;
	}

	// A large number to force to load the object:
	constexpr F32 LARGE_SCENE_CONTRIBUTION = 1000.f;
	const LLVector3 camera_origin = gViewerCamera.getOrigin();
	const U32 cur_frame = LLViewerOctreeEntryData::getCurrentFrame();
	bool needs_update = cur_frame - mLastCameraUpdate > 5 &&
						(camera_origin -
						 mLastCameraOrigin).lengthSquared() > 10.f;
	U32 last_update = mLastCameraUpdate;
	LLVector4a local_origin;
	local_origin.load3((camera_origin - getOriginAgent()).mV);

	// Process visible entries
	for (LLVOCacheEntry::set_t::iterator iter = mVisibleEntries.begin(),
										 end = mVisibleEntries.end();
		 iter != end; )
	{
		LLVOCacheEntry::set_t::iterator curiter = iter++;
		LLVOCacheEntry* entryp = *curiter;
		if (entryp && entryp->isValid() &&
			entryp->getState() < LLVOCacheEntry::WAITING)
		{
			// Set a large number to force to load this object.
			entryp->setSceneContribution(LARGE_SCENE_CONTRIBUTION);

			mWaitingList.insert(entryp);
		}
		else
		{
			mVisibleEntries.erase(curiter);
		}
	}

	// Process visible groups

	if (!mVOCachePartition)
	{
		return;
	}

	// Object projected area threshold
	F32 projection_threshold =
		LLVOCacheEntry::getSquaredPixelThreshold(mVOCachePartition->isFrontCull());
	F32 dist_threshold =
		mVOCachePartition->isFrontCull() ? gAgent.mDrawDistance
										 : LLVOCacheEntry::sRearFarRadius;

	for (U32 i = 0, count = mVisibleGroups.size(); i < count; ++i)
	{
		LLPointer<LLViewerOctreeGroup> group = mVisibleGroups[i];
		if (group.isNull() || group->getNumRefs() < 3 || // Group to be deleted
			!group->getOctreeNode() || group->isEmpty()) // Group empty
		{
			continue;
		}

		for (LLViewerOctreeGroup::element_iter it = group->getDataBegin(),
											   end2 = group->getDataEnd();
			 it != end2; ++it)
		{
			if (*it && (*it)->hasVOCacheEntry())
			{
				LLVOCacheEntry* entryp =
					(LLVOCacheEntry*)(*it)->getVOCacheEntry();
				if (entryp->getParentID() == 0 && entryp->isValid())
				{
					entryp->calcSceneContribution(local_origin, needs_update,
												  last_update, dist_threshold);
					if (entryp->getSceneContribution() > projection_threshold)
					{
						mWaitingList.insert(entryp);
					}
				}
			}
		}
	}

	if (needs_update)
	{
		mLastCameraOrigin = camera_origin;
		mLastCameraUpdate = cur_frame;
	}
}

void LLViewerRegion::createVisibleObjects(F32 max_time)
{
	if (mDead)
	{
		return;
	}
	if (mWaitingList.empty())
	{
		mVOCachePartition->setCullHistory(false);
		return;
	}

	S32 throttle = sNewObjectCreationThrottle;
	bool do_throttle = throttle > 0;
	bool has_new_obj = false;
	LLTimer update_timer;
	for (LLVOCacheEntry::prio_list_t::iterator iter = mWaitingList.begin(),
											   end = mWaitingList.end();
		 iter != end; ++iter)
	{
		LLVOCacheEntry* entryp = *iter;
		if (entryp->getState() < LLVOCacheEntry::WAITING)
		{
			addNewObject(entryp);
			has_new_obj = true;
			if ((do_throttle && --throttle <= 0) ||
				update_timer.getElapsedTimeF32() > max_time)
			{
				break;
			}
		}
	}

	mVOCachePartition->setCullHistory(has_new_obj);
}

void LLViewerRegion::clearCachedVisibleObjects()
{
	mWaitingList.clear();
	mVisibleGroups.clear();

	// Reset all occluders
	mVOCachePartition->resetOccluders();
	mPaused = true;

	// Clean visible entries
	for (LLVOCacheEntry::set_t::iterator iter = mVisibleEntries.begin();
		 iter != mVisibleEntries.end(); )
	{
		LLVOCacheEntry::set_t::iterator curiter = iter++;
		LLVOCacheEntry* entry = *curiter;
		LLVOCacheEntry* parent = getCacheEntry(entry->getParentID());

		// If no child or parent is cache-able
		if (!entry->getParentID() || parent)
		{
			if (parent) // Has a cache-able parent
			{
				parent->addChild(entry);
			}

			mVisibleEntries.erase(curiter);
		}
	}

	// Remove all visible entries.
	mLastVisitedEntry = NULL;
	std::vector<LLDrawable*> delete_list;
	for (LLVOCacheEntry::set_t::iterator iter = mActiveSet.begin(),
										 end = mActiveSet.end();
		 iter != end; ++iter)
	{
		LLVOCacheEntry* entryp = *iter;
		if (!entryp)
		{
			continue;
		}
		LLViewerOctreeEntry* octreep = entryp->getEntry();
		if (!octreep)
		{
			continue;
		}
		LLDrawable* drawablep = (LLDrawable*)octreep->getDrawable();
		if (drawablep && !drawablep->getParent())
		{
			delete_list.push_back(drawablep);
		}
	}

	if (!delete_list.empty())
	{
		for (S32 i = 0, count = delete_list.size(); i < count; ++i)
		{
			gObjectList.killObject(delete_list[i]->getVObj());
		}
		delete_list.clear();
	}
}

// Perform some necessary but very light updates to replace the function
// idleUpdate(...) in case there is not enough time.
void LLViewerRegion::lightIdleUpdate()
{
	if (sVOCacheCullingEnabled && !mCacheMap.empty())
	{
		LL_FAST_TIMER(FTM_UPD_CACHEDOBJECTS);
		// Reset all occluders
		mVOCachePartition->resetOccluders();
	}
}

void LLViewerRegion::idleUpdate(F32 max_update_time)
{
	LLTimer update_timer;

	mLastUpdate = LLViewerOctreeEntryData::getCurrentFrame();

	{
		LL_FAST_TIMER(FTM_UPD_LANDPATCHES);
		mLandp->idleUpdate(max_update_time);
	}

	if (mParcelOverlay)
	{
		LL_FAST_TIMER(FTM_UPD_PARCELOVERLAY);
		// Hopefully not a significant time sink...
		mParcelOverlay->idleUpdate();
	}

	if (sVOCacheCullingEnabled && !mCacheMap.empty())
	{
		LL_FAST_TIMER(FTM_UPD_CACHEDOBJECTS);

		mPaused = false;	// Un-pause

		S32 old_camera_id = LLViewerCamera::sCurCameraID;
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

		// Reset all occluders
		mVOCachePartition->resetOccluders();

		F32 max_time = max_update_time - update_timer.getElapsedTimeF32();
        killInvisibleObjects(max_time * 0.4f);

		updateVisibleEntries();

		max_time = max_update_time - update_timer.getElapsedTimeF32();
		createVisibleObjects(max_time);

		mWaitingList.clear();
		mVisibleGroups.clear();

		LLViewerCamera::sCurCameraID = old_camera_id;
	}
}

// Update the throttling number for new object creation
void LLViewerRegion::calcNewObjectCreationThrottle()
{
	static LLCachedControl<S32> creation_throttle(gSavedSettings,
												  "NewObjectCreationThrottle");
	static LLCachedControl<F32> throttle_delay(gSavedSettings,
											   "NewObjectCreationThrottleDelay");
	static LLFrameTimer timer;

	// sNewObjectCreationThrottle =
	//  -2 : throttle is disabled because either the screen is showing progress
	//       view, or immediate after the screen is not black
	//  -1 : throttle is disabled by the debug setting
	//   0 : no new object creation is allowed
	// > 0 : valid throttling number

	if (throttle_delay > 0.f &&
		(gTeleportDisplay ||
		 (gViewerWindowp && gViewerWindowp->getProgressView()->getVisible())))
	{
		sNewObjectCreationThrottle = -2; // cancel the throttling
		timer.reset();
	}
	else if (sNewObjectCreationThrottle < -1)
	{
		// Just recovered from the login/teleport screen... Wait for
		// throttle_delay to reset the throttle
		if (timer.getElapsedTimeF32() > throttle_delay)
		{
			sNewObjectCreationThrottle = creation_throttle;
			if (sNewObjectCreationThrottle < -1)
			{
				sNewObjectCreationThrottle = -1;
			}
		}
	}
}

bool LLViewerRegion::isViewerCameraStatic()
{
	return sLastCameraUpdated < LLViewerOctreeEntryData::getCurrentFrame();
}

void LLViewerRegion::killInvisibleObjects(F32 max_time)
{
	if (!sVOCacheCullingEnabled || sNewObjectCreationThrottle < -1 ||
		mActiveSet.empty())
	{
		return;
	}

	LLTimer update_timer;
	LLVector4a camera_origin;
	camera_origin.load3(gViewerCamera.getOrigin().mV);
	LLVector4a local_origin;
	local_origin.load3((gViewerCamera.getOrigin() - getOriginAgent()).mV);
	F32 back_threshold = LLVOCacheEntry::sRearFarRadius;

	size_t max_update = 64;
	if (!mInvisibilityCheckHistory && isViewerCameraStatic())
	{
		// History is clean, reduce number of checking
		max_update /= 2;
	}

	std::vector<LLDrawable*> delete_list;
	S32 update_counter = llmin(max_update, mActiveSet.size());
	LLVOCacheEntry::set_t::iterator iter;
	for (iter = mActiveSet.upper_bound(mLastVisitedEntry);
		 update_counter > 0; --update_counter, ++iter)
	{
		if (iter == mActiveSet.end())
		{
			iter = mActiveSet.begin();
		}
		if ((*iter)->getParentID() > 0)
		{
			// Skip child objects: they are removed with their parent.
			continue;
		}

		LLVOCacheEntry* entryp = *iter;
		if (!entryp->isAnyVisible(camera_origin, local_origin,
								  back_threshold) &&
			entryp->mLastCameraUpdated < sLastCameraUpdated)
		{
			killObject(entryp, delete_list);
		}

		if (max_time < update_timer.getElapsedTimeF32())
		{
			break;	// We timed out
		}
	}

	if (iter == mActiveSet.end())
	{
		mLastVisitedEntry = NULL;
	}
	else
	{
		mLastVisitedEntry = *iter;
	}

	mInvisibilityCheckHistory <<= 1;
	if (!delete_list.empty())
	{
		mInvisibilityCheckHistory |= 1;
		for (S32 i = 0, count = delete_list.size(); i < count; ++i)
		{
			gObjectList.killObject(delete_list[i]->getVObj());
		}
		delete_list.clear();
	}
}

void LLViewerRegion::killObject(LLVOCacheEntry* entry,
								std::vector<LLDrawable*>& delete_list)
{
	// kill the object.
	LLDrawable* drawablep = (LLDrawable*)entry->getEntry()->getDrawable();
	llassert(drawablep && drawablep->getRegion() == this);

	if (drawablep && !drawablep->getParent())
	{
		LLViewerObject* vobj = drawablep->getVObj();
		if (vobj)
		{
			if (vobj->isSelected() ||
				(vobj->flagAnimSource() && isAgentAvatarValid() &&
				 gAgentAvatarp->hasMotionFromSource(vobj->getID())))
			{
				// Do not remove objects user is interacting with
				((LLViewerOctreeEntryData*)drawablep)->setVisible();
				return;
			}

			LLViewerObject::const_child_list_t& child_list = vobj->getChildren();
			for (LLViewerObject::child_list_t::const_iterator
					iter = child_list.begin(), end = child_list.end();
				 iter != end; ++iter)
			{
				LLViewerObject* child = *iter;
				if (child && child->mDrawable)
				{
					if (!child->mDrawable->getEntry() ||
						!child->mDrawable->getEntry()->hasVOCacheEntry() ||
						child->isSelected() ||
						(child->flagAnimSource() &&
						 isAgentAvatarValid() &&
						 gAgentAvatarp->hasMotionFromSource(child->getID())))
					{
						// Do not remove parent if any of its children is non-
						// cacheable, animating or selected, especially for the
						// case when an avatar sits on a cacheable object.
						((LLViewerOctreeEntryData*)drawablep)->setVisible();
						return;
					}

					LLOcclusionCullingGroup* group =
						(LLOcclusionCullingGroup*)child->mDrawable->getGroup();
					if (group && group->isAnyRecentlyVisible())
					{
						// Set the parent visible if any of its children visible.
						((LLViewerOctreeEntryData*)drawablep)->setVisible();
						return;
					}
				}
			}
		}
		else
		{
			llwarns_once << "NULL viewer object for drawable: " << std::hex
						 << drawablep << std::dec << llendl;
			llassert(false);
		}
		delete_list.push_back(drawablep);
	}
}

LLViewerObject* LLViewerRegion::addNewObject(LLVOCacheEntry* entry)
{
	if (!entry || !entry->getEntry())
	{
		if (entry)
		{
			mVisibleEntries.erase(entry);
			entry->setState(LLVOCacheEntry::INACTIVE);
		}
		return NULL;
	}

	LLViewerObject* objp = NULL;
	if (!entry->getEntry()->hasDrawable())
	{
		 // Not yet added to the rendering pipeline... Add the object now.
		objp = gObjectList.processObjectUpdateFromCache(entry, this);
		if (objp && !entry->isState(LLVOCacheEntry::ACTIVE))
		{
			mWaitingSet.insert(entry);
			entry->setState(LLVOCacheEntry::WAITING);
		}
	}
	else
	{
		LLViewerRegion* old_regionp =
			((LLDrawable*)entry->getEntry()->getDrawable())->getRegion();
		if (old_regionp != this)
		{
			// This object exists in two regions at the same time; this case
			// can be safely ignored here because the server should soon send
			// update message to remove one region for this object.
			llwarns_once << "Entry: " << entry->getLocalID()
						 << " exists in two regions at the same time."
						 << llendl;
			return NULL;
		}

		llwarns_once << "Entry: " << entry->getLocalID()
					 << " in rendering pipeline but not set to be active."
					 << llendl;

		// Should not hit here any more, but does not hurt either, just put it
		// back to active list
		addActiveCacheEntry(entry);
	}

#if 0
	if (objp)
	{
		loadCacheMiscExtras(objp);
	}
#endif

	return objp;
}

void LLViewerRegion::loadCacheMiscExtras(LLViewerObject* objp)
{
	if (!objp)
	{
		return;
	}

	U32 local_id = objp->getLocalID();
	LLVOCacheEntry::emap_t::iterator iter = mGLTFOverrides.find(local_id);
	if (iter == mGLTFOverrides.end())
	{
		return;
	}

	LL_DEBUGS("ObjectCache") << "Applying cached data to object: "
							 << objp->getID() << LL_ENDL;

	bool has_te[MAX_TES] = { false };
	const LLGLTFOverrideCacheEntry& entry = iter->second;

	for (auto it = entry.mGLTFMaterial.begin(),
			  end = entry.mGLTFMaterial.end();
	 	it != end; ++it)
	{
		S32 te = it->first;
		objp->setTEGLTFMaterialOverride(te, it->second);
		if (objp->getTE(te) && objp->getTE(te)->isSelected())
		{
			LLGLTFMaterialList::doSelectionCallbacks(objp->getID(), te);
		}
		if (te < (S32)MAX_TES)
		{
			has_te[te] = true;
		}
	}
	// Null out overrides on TEs that should not have them
	for (U32 i = 0, count = llmin(objp->getNumTEs(), MAX_TES); i < count; ++i)
	{
		if (!has_te[i])
		{
			LLTextureEntry* tep = objp->getTE(i);
			if (tep && tep->getGLTFMaterialOverride())
			{
				objp->setTEGLTFMaterialOverride(i, NULL);
				LLGLTFMaterialList::doSelectionCallbacks(objp->getID(), i);
			}
		}
	}
}

void LLViewerRegion::applyCacheMiscExtras(LLViewerObject* objp)
{
	if (!objp) return;

	U32 local_id = objp->getLocalID();
	LLVOCacheEntry::emap_t::iterator it = mGLTFOverrides.find(local_id);
	if (it == mGLTFOverrides.end())
	{
		return;
	}
	for (auto& side : it->second.mGLTFMaterial)
	{
		objp->setTEGLTFMaterialOverride(side.first, side.second);
	}
}

// Updates object cache if the object receives a full-update or a terse update
LLViewerObject* LLViewerRegion::updateCacheEntry(U32 local_id,
												 LLViewerObject* objectp)
{
	LLVOCacheEntry* entry = getCacheEntry(local_id);
	if (!entry)
	{
		return objectp; // Not in the cache, do nothing.
	}

	if (!objectp) // Object not created
	{
		// Create a new object from cache.
		objectp = addNewObject(entry);
	}

	// Remove from cache
	killCacheEntry(entry, true);

	return objectp;
}

void LLViewerRegion::forceUpdate()
{
	mLandp->idleUpdate(0.f);

	if (mParcelOverlay)
	{
		mParcelOverlay->idleUpdate(true);
	}
}

bool LLViewerRegion::isEventPollInFlight() const
{
	return mEventPoll && mEventPoll->isPollInFlight();
}

F32 LLViewerRegion::getEventPollRequestAge() const
{
	return mEventPoll ? mEventPoll->getPollAge() : -1.f;
}

void LLViewerRegion::connectNeighbor(LLViewerRegion *neighborp, U32 direction)
{
	mLandp->connectNeighbor(neighborp->mLandp, direction);
	mCloudLayer.connectNeighbor(&(neighborp->mCloudLayer), direction);
}

void LLViewerRegion::disconnectAllNeighbors()
{
	mLandp->disconnectAllNeighbors();
	mCloudLayer.disconnectAllNeighbors();
}

F32 LLViewerRegion::getCompositionXY(S32 x, S32 y) const
{
	if (x >= mWidth)
	{
		if (y >= mWidth)
		{
			LLVector3d center = getCenterGlobal() +
								LLVector3d(mWidth, mWidth, 0.f);
			LLViewerRegion* regionp = gWorld.getRegionFromPosGlobal(center);
			if (regionp)
			{
				// OK, we need to do some hackery here - different simulators
				// no longer use the same composition values, necessarily.
				// If we're attempting to blend, then we want to make the
				// fractional part of this region match the fractional of the
				// adjacent. For now, just minimize the delta.
				F32 our_comp = mCompositionp->getValueScaled(mWidth - 1.f,
															 mWidth - 1.f);
				F32 adj_comp =
					regionp->mCompositionp->getValueScaled(x - regionp->mWidth,
														   y - regionp->mWidth);
				while (fabsf(our_comp - adj_comp) >= 1.f)
				{
					if (our_comp > adj_comp)
					{
						adj_comp += 1.f;
					}
					else
					{
						adj_comp -= 1.f;
					}
				}
				return adj_comp;
			}
		}
		else
		{
			LLVector3d center = getCenterGlobal() + LLVector3d(mWidth, 0, 0.f);
			LLViewerRegion* regionp = gWorld.getRegionFromPosGlobal(center);
			if (regionp)
			{
				// OK, we need to do some hackery here - different simulators
				// no longer use the same composition values, necessarily.
				// If we're attempting to blend, then we want to make the
				// fractional part of this region match the fractional of the
				// adjacent. For now, just minimize the delta.
				F32 our_comp = mCompositionp->getValueScaled(mWidth - 1.f,
															 (F32)y);
				F32 adj_comp =
					regionp->mCompositionp->getValueScaled(x - regionp->mWidth,
														   (F32)y);
				while (fabsf(our_comp - adj_comp) >= 1.f)
				{
					if (our_comp > adj_comp)
					{
						adj_comp += 1.f;
					}
					else
					{
						adj_comp -= 1.f;
					}
				}
				return adj_comp;
			}
		}
	}
	else if (y >= mWidth)
	{
		LLVector3d center = getCenterGlobal() + LLVector3d(0.f, mWidth, 0.f);
		LLViewerRegion* regionp = gWorld.getRegionFromPosGlobal(center);
		if (regionp)
		{
			// OK, we need to do some hackery here - different simulators no
			// longer use the same composition values, necessarily. If we're
			// attempting to blend, then we want to make the fractional part of
			// this region match the fractional of the adjacent. For now, just
			// minimize the delta.
			F32 our_comp = mCompositionp->getValueScaled((F32)x, mWidth - 1.f);
			F32 adj_comp =
				regionp->mCompositionp->getValueScaled((F32)x,
													   y - regionp->mWidth);
			while (fabsf(our_comp - adj_comp) >= 1.f)
			{
				if (our_comp > adj_comp)
				{
					adj_comp += 1.f;
				}
				else
				{
					adj_comp -= 1.f;
				}
			}
			return adj_comp;
		}
	}

	return mCompositionp->getValueScaled((F32)x, (F32)y);
}

void LLViewerRegion::calculateCenterGlobal()
{
	mCenterGlobal = mOriginGlobal;
	mCenterGlobal.mdV[VX] += 0.5 * mWidth;
	mCenterGlobal.mdV[VY] += 0.5 * mWidth;
	mCenterGlobal.mdV[VZ] = 0.5 * mLandp->getMinZ() + mLandp->getMaxZ();
}

void LLViewerRegion::calculateCameraDistance()
{
	mCameraDistanceSquared = (F32)(gAgent.getCameraPositionGlobal() -
								   getCenterGlobal()).lengthSquared();
}

std::ostream& operator<<(std::ostream& s, const LLViewerRegion& region)
{
	s << "{ " << region.mHost
	  << " mOriginGlobal = " << region.getOriginGlobal() << "\n";
	std::string name = region.getName();
	if (!name.empty())
	{
		s << " mName         = " << name << '\n';
	}
	name = region.getZoning();
	if (!name.empty())
	{
		s << " mZoning       = " << name << '\n';
	}
	s << "}";
	return s;
}

void LLViewerRegion::updateNetStats()
{
	F32 dt = mLastNetUpdate.getElapsedTimeAndResetF32();

	LLCircuitData* cdp = gMessageSystemp->mCircuitInfo.findCircuit(mHost);
	if (!cdp)
	{
		mAlive = false;
		return;
	}

	mAlive = true;
	mDeltaTime = dt;

	mLastPacketsIn = mPacketsIn;
	mLastBitsIn = mBitsIn;
	mLastPacketsOut = mPacketsOut;
	mLastPacketsLost = mPacketsLost;

	mPacketsIn = cdp->getPacketsIn();
	mBitsIn = 8 * cdp->getBytesIn();
	mPacketsOut = cdp->getPacketsOut();
	mPacketsLost = cdp->getPacketsLost();
	mPingDelay = cdp->getPingDelay();

	mBitStat.addValue(mBitsIn - mLastBitsIn);
	mPacketsStat.addValue(mPacketsIn - mLastPacketsIn);
	mPacketsLostStat.addValue(mPacketsLost);
}

U32 LLViewerRegion::getPacketsLost() const
{
	LLCircuitData* cdp = gMessageSystemp->mCircuitInfo.findCircuit(mHost);
	if (!cdp)
	{
		llinfos << "Could not find circuit for " << mHost << llendl;
		return 0;
	}
	return cdp->getPacketsLost();
}

bool LLViewerRegion::pointInRegionGlobal(const LLVector3d& point_global) const
{
	LLVector3 pos_region = getPosRegionFromGlobal(point_global);
	return pos_region.mV[VX] >= 0 && pos_region.mV[VX] < mWidth &&
		   pos_region.mV[VY] >= 0 && pos_region.mV[VY] <mWidth;
}

LLVector3 LLViewerRegion::getPosAgentFromRegion(const LLVector3& pos_region) const
{
	return gAgent.getPosAgentFromGlobal(getPosGlobalFromRegion(pos_region));
}

F32 LLViewerRegion::getLandHeightRegion(const LLVector3& region_pos)
{
	return mLandp->resolveHeightRegion(region_pos);
}

bool LLViewerRegion::isOwnedSelf(const LLVector3& pos)
{
	return mParcelOverlay && mParcelOverlay->isOwnedSelf(pos);
}

// Owned by a group you belong to?  (officer or member)
bool LLViewerRegion::isOwnedGroup(const LLVector3& pos)
{
	return mParcelOverlay && mParcelOverlay->isOwnedGroup(pos);
}

// Yhe new TCP coarse location handler node
class CoarseLocationUpdate final : public LLHTTPNode
{
public:
	void post(ResponsePtr responder, const LLSD& context,
			  const LLSD& input) const override
	{
		LLHost host(input["sender"].asString());
		LLViewerRegion* region = gWorld.getRegion(host);
		if (!region)
		{
			return;
		}

		S32 target_index = input["body"]["Index"][0]["Prey"].asInteger();
		S32 you_index    = input["body"]["Index"][0]["You" ].asInteger();

		std::vector<U32>* avatar_locs = &region->mMapAvatars;
		uuid_vec_t* avatar_ids = &region->mMapAvatarIDs;
		avatar_locs->clear();
		avatar_ids->clear();

		LLSD locs = input["body"]["Location"];
		LLSD agents = input["body"]["AgentData"];
		LLSD::array_iterator locs_it = locs.beginArray();
		LLSD::array_iterator locs_end = locs.endArray();
		LLSD::array_iterator agents_it = agents.beginArray();
		bool has_agent_data = input["body"].has("AgentData");

		// Variable region size support
		F64 scale_factor = (F64)region->getWidth() / REGION_WIDTH_METERS;

		for (S32 i = 0; locs_it != locs_end; ++i, ++locs_it)
		{
			U8 x = locs_it->get("X").asInteger();
			U8 y = locs_it->get("Y").asInteger();
			U8 z = locs_it->get("Z").asInteger();
			// treat the target specially for the map, and don't add you or the
			// target
			if (i == target_index)
			{
				LLVector3d global_pos(region->getOriginGlobal());
				global_pos.mdV[VX] += (F64)x * scale_factor;
				global_pos.mdV[VY] += (F64)y * scale_factor;
				global_pos.mdV[VZ] += (F64)z * 4.0;
				gAvatarTracker.setTrackedCoarseLocation(global_pos);
			}
			else if (i != you_index)
			{
				U32 pos = 0x0;
				pos |= x;
				pos <<= 8;
				pos |= y;
				pos <<= 8;
				pos |= z;
				avatar_locs->push_back(pos);
				if (has_agent_data)
				{
					// For backwards compatibility with old message format
					avatar_ids->emplace_back(agents_it->get("AgentID").asUUID());
				}
			}
			if (has_agent_data)
			{
				++agents_it;
			}
		}
	}
};

// build the coarse location HTTP node under the "/message" URL
LLHTTPRegistration<CoarseLocationUpdate>
   gHTTPRegistrationCoarseLocationUpdate("/message/CoarseLocationUpdate");

// the deprecated coarse location handler
void LLViewerRegion::updateCoarseLocations(LLMessageSystem* msg)
{
	mMapAvatars.clear();
	mMapAvatarIDs.clear();

	U8 x_pos = 0;
	U8 y_pos = 0;
	U8 z_pos = 0;
	U32 pos = 0x0;

	S16 agent_index;
	S16 target_index;
	msg->getS16Fast(_PREHASH_Index, _PREHASH_You, agent_index);
	msg->getS16Fast(_PREHASH_Index, _PREHASH_Prey, target_index);

	// Variable region size support
	F64 scale_factor = (F64)mWidth / REGION_WIDTH_METERS;

	bool has_agent_data = msg->has(_PREHASH_AgentData);
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_Location);
	LLUUID agent_id;
	for (S32 i = 0; i < count; ++i)
	{
		msg->getU8Fast(_PREHASH_Location, _PREHASH_X, x_pos, i);
		msg->getU8Fast(_PREHASH_Location, _PREHASH_Y, y_pos, i);
		msg->getU8Fast(_PREHASH_Location, _PREHASH_Z, z_pos, i);
		agent_id.setNull();
		if (has_agent_data)
		{
			msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID,
							 agent_id, i);
		}

		// Treat the target specially for the map
		if (i == target_index)
		{
			LLVector3d global_pos(mOriginGlobal);
			global_pos.mdV[VX] += (F64)x_pos * scale_factor;
			global_pos.mdV[VY] += (F64)y_pos * scale_factor;
			global_pos.mdV[VZ] += (F64)z_pos * 4.0;
			gAvatarTracker.setTrackedCoarseLocation(global_pos);
		}

		// Do not add self
		if (i != agent_index)
		{
			pos = 0x0;
			pos |= x_pos;
			pos <<= 8;
			pos |= y_pos;
			pos <<= 8;
			pos |= z_pos;
			mMapAvatars.push_back(pos);
			if (has_agent_data)
			{
				mMapAvatarIDs.emplace_back(agent_id);
			}
			else
			{
				// Maintain strict coherency in indices bewteen the mMapAvatars
				// and mMapAvatarIDs vectors, else things could get messy in
				// LLWorld::getAvatars()... HB
				mMapAvatarIDs.emplace_back(LLUUID::null);
			}
		}
	}
}

void LLViewerRegion::getInfo(LLSD& info)
{
	info["Region"]["Host"] = mHost.getIPandPort();
	info["Region"]["Name"] = mName;
	U32 x, y;
	from_region_handle(getHandle(), &x, &y);
	info["Region"]["Handle"]["x"] = (LLSD::Integer)x;
	info["Region"]["Handle"]["y"] = (LLSD::Integer)y;
}

//static
void LLViewerRegion::requestBaseCapabilitiesCoro(U64 region_handle)
{
	LLViewerRegion* self = gWorld.getRegionFromHandle(region_handle);
	if (!self)
	{
		// Region was since disconnected. Ignore and abort.
		return;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("BaseCapabilitiesRequest");
	LLSD result;

	// This loop is used for retrying a capabilities request.
	do
	{
		const std::string& url = self->getCapability("Seed");
		if (url.empty())
		{
			llwarns << "No Seed capability for region: "
					<< self->getIdentity() << ". Aborted." << llendl;
			self->setCapabilitiesError();
			break;
		}

		EStartupState state = LLStartUp::getStartupState();
		if (state < STATE_WORLD_INIT)
		{
			llinfos << "Aborting capabilities request, reason: returned to login screen"
					<< llendl;
			break;
		}

		if (++self->mSeedCapAttempts > MAX_CAP_REQUEST_ATTEMPTS)
		{
			// *TODO: Give a user pop-up about this error ?
			llwarns << "Failed to get seed capability from '" << url
					<< "' after " << self->mSeedCapAttempts
					<< " attempts, for region: " << self->getIdentity()
					<< ". Giving up !" << llendl;
			self->setCapabilitiesError();
			break;
		}

		U32 id = ++self->mHttpResponderID;

		LLSD cap_names = LLSD::emptyArray();
		buildCapabilityNames(cap_names);

		llinfos << "Attempt #" << self->mSeedCapAttempts
				<< " at requesting seed for region " << self->getIdentity()
				<< " from: " << url << llendl;

		result = adapter.postAndSuspend(url, cap_names, sHttpOptions,
										sHttpHeaders);

		state = LLStartUp::getStartupState();
		if (state < STATE_WORLD_INIT)
		{
			llinfos << "Aborting capabilities request, reason: returned to login screen"
					<< llendl;
			break;
		}

		// Abort now if we are quitting
		if (LLApp::isExiting() || gDisconnected)
		{
			break;
		}

		self = gWorld.getRegionFromHandle(region_handle);
		if (!self)
		{
			llwarns << "Received a capability response for a disconnected region. Ignored."
					<< llendl;
			break;
		}

		if (id != self->mHttpResponderID)
		{
			llwarns << "Received a staled capability response. Ignored."
					<< llendl;
			continue;
		}

		if (!result.isMap() || result.has("error"))
		{
			llwarns << "Malformed response. Ignored." << llendl;
			continue;
		}

		LLCore::HttpStatus status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
		if (!status)
		{
			llwarns << "HTTP error fetching capabilities for region: "
					<< self->getIdentity() << ". Will retry..."
					<< llendl;
			continue;
		}

		// Remove the http_result from the llsd
		result.erase("http_result");

		for (LLSD::map_const_iterator iter = result.beginMap(),
									  end = result.endMap();
			 iter != end; ++iter)
		{
			self->setCapability(iter->first, iter->second);
			LL_DEBUGS("Capabilities") << "Got capability '" << iter->first
									  << "' for region: "
									  << self->getIdentity() << LL_ENDL;
		}

		self->setCapabilitiesReceived(true);

		break;
	} 
	while (true);
}

//static
void LLViewerRegion::requestBaseCapabilitiesCompleteCoro(U64 region_handle)
{
	LLViewerRegion* self = gWorld.getRegionFromHandle(region_handle);
	if (!self)
	{
		// Region was since disconnected. Ignore and abort.
		return;
	}

	const std::string& url = self->getCapability("Seed");
	if (url.empty())
	{
		llwarns << "No 'Seed' capability for region: " << self->getIdentity()
				<< ". Aborted." << llendl;
		// Note: initial attempt failed to get this cap as well...
		self->setCapabilitiesError();
		return;
	}

	LLSD cap_names = LLSD::emptyArray();
	buildCapabilityNames(cap_names);

	llinfos << "Requesting second 'Seed' capability for region "
			<< self->getIdentity() << " from: " << url << llendl;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("BaseCapabilitiesRequest");
	LLSD result = adapter.postAndSuspend(url, cap_names, sHttpOptions,
										 sHttpHeaders);

	// Abort now if we are quitting
	if (LLApp::isExiting() || gDisconnected)
	{
		return;
	}

	self = gWorld.getRegionFromHandle(region_handle);
	if (!self)
	{
		llwarns << "Received a capability response for a disconnected region. Ignored."
				<< llendl;
		return;
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "HTTP error fetching second capabilities for region: "
				<< self->getIdentity() << llendl;
		return;
	}

	// Remove the http_result from the LLSD
	result.erase("http_result");

	bool set_cap = gSavedSettings.getBool("RegisterChangedCaps");
	U32 old_count = (U32)self->getCapabilitiesCount();
	U32 count = 0;
	std::string new_caps_list;
	std::string changed_caps_list;
	for (LLSD::map_const_iterator iter = result.beginMap(),
								  end = result.endMap();
		 iter != end; ++iter)
	{
		const std::string name = iter->first;
		const std::string url = iter->second;
		if (self->isSpecialCapabilityName(name))
		{
			// Do not count special capabilities that are not kept in the map.
			LL_DEBUGS("Capabilities") << "Got special capability: " << name
									  << " - " << url << LL_ENDL;
			continue;
		}
		const std::string& old_url = self->getCapability(name.c_str());
		if (old_url.empty())
		{
			if (!new_caps_list.empty())
			{
				new_caps_list += ", ";
			}
			new_caps_list += name;
			if (set_cap)
			{
				self->setCapability(name, url);
			}
			LL_DEBUGS("Capabilities") << "New capability '" << name
									  << "' - URL: " << url << LL_ENDL;
		}
		else if (old_url != url)
		{
			if (!changed_caps_list.empty())
			{
				changed_caps_list += ", ";
			}
			changed_caps_list += name;
			if (set_cap)
			{
				self->setCapability(name, url);
			}
			LL_DEBUGS("Capabilities") << "Changed capability '" << name
									  << "' - Old URL: " << old_url
									  << " - New URL: " << url << LL_ENDL;
		}
		else
		{
			LL_DEBUGS("Capabilities") << "Got duplicate capability (same url): "
									  << name << LL_ENDL;
		}
		++count;
	}
	self->onCapabilitiesReceived();

	if (!new_caps_list.empty())
	{
		if (set_cap)
		{
			llinfos << "Simulator " << self->getIdentity()
					<< " sent the following new capabilities: "
					<< new_caps_list << llendl;
		}
		else
		{
			llwarns_once << "Simulator " << self->getIdentity()
						 << " sent the following new capabilities: "
						 << new_caps_list << ". Ignoring." << llendl;
		}
	}
	if (!changed_caps_list.empty())
	{
		if (set_cap)
		{
			llinfos << "Simulator " << self->getIdentity()
					<< " sent new URLs for capabilities: "
					<< changed_caps_list << llendl;
		}
		else
		{
			llwarns_once << "Simulator " << self->getIdentity()
						 << " sent new URLs for capabilities: "
						 << changed_caps_list << ". Ignoring." << llendl;
		}
	}
	if (count != old_count + (U32)new_caps_list.size())
	{
		// There are some capabilities missing in the new set (which seems
		// to be a very common occurrence, thus the LL_DEBUGS instead of a
		// llwarns).
		LL_DEBUGS("Capabilities") << "Simulator " << self->getIdentity()
								  << " sent a new, smaller set of capabilities."
								  << LL_ENDL;
	}
}

//static
void LLViewerRegion::requestSimulatorFeatureCoro(std::string url,
												 U64 region_handle)
{
	LLViewerRegion* self = gWorld.getRegionFromHandle(region_handle);
	if (!self)
	{
		// Region was since disconnected. Ignore and abort.
		return;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("SimFeatureRequest");
	U32 attempt = 0;
	LLSD result;

	// This loop is used for retrying a capabilities request.
	do
	{
		if (++attempt > MAX_CAP_REQUEST_ATTEMPTS)
		{
			llwarns << "Retries count exceeded attempting to get simulator feature for region "
					<< self->getIdentity() << " from: " << url << llendl;
			break;
		}

		result = adapter.getAndSuspend(url, sHttpOptions, sHttpHeaders);

		// Abort now if we are quitting
		if (LLApp::isExiting() || gDisconnected)
		{
			break;
		}

		self = gWorld.getRegionFromHandle(region_handle);
		if (!self)
		{
			// Region was since disconnected. Ignore and abort.
			llwarns << "Received a simulator feature for a disconnected region. Ignored."
					<< llendl;
			break;
		}

		LLCore::HttpStatus status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
		if (!status)
		{
			llwarns << "HTTP error fetching simulator feature for region: "
					<< self->getIdentity() << ". Will retry..." << llendl;
			continue;
		}

		// Remove the http_result from the LLSD
		result.erase("http_result");

		self->setSimulatorFeatures(result);

		break;
	}
	while (true);
}

void LLViewerRegion::setSimulatorFeatures(const LLSD& sim_features)
{
	llinfos << "Received simulator features for region: " << getIdentity()
			<< llendl;

	LL_DEBUGS("SimulatorFeatures") << "\n";
	std::stringstream str;
	LLSDSerialize::toPrettyXML(sim_features, str);
	LL_CONT << str.str() << LL_ENDL;

	mSimulatorFeatures = sim_features;
	mFeaturesReceived = true;

	if (mSimulatorFeatures.has("HostName"))
	{
		mHostName = mSimulatorFeatures["HostName"].asString();
		if (!gIsInSecondLife &&
			// AWS server reporting the alias for SL sim hostname. HB
			mHostName.find("secondlife.io") != std::string::npos)
		{
			llwarns << "Second Life sim detected while supposedly logged in OpenSim !"
					<< llendl;
			gIsInSecondLife = true,
			gIsInSecondLifeBetaGrid =
				mHostName.find("aditi") != std::string::npos;
			gIsInProductionGrid = gIsInSecondLifeProductionGrid =
				!gIsInSecondLifeBetaGrid;
			if (gViewerWindowp)
			{
				gViewerWindowp->setMenuBackgroundColor();
			}
			llinfos << "Switched to Second Life mode/policy." << llendl;
		}
	}

	// Cache physics shape types availability flag. HB
	mPhysicsShapeTypes = mSimulatorFeatures.has("PhysicsShapeTypes");
	LL_DEBUGS("SimulatorFeatures") << "Physics shape types"
								   << (mPhysicsShapeTypes ? " " : " not ")
								   << "supported"
								   << LL_ENDL;

	// Avatar Hover Height support. HB
	mHoverHeigthFeature = !getCapability("AgentPreferences").empty() &&
						  mSimulatorFeatures.has("AvatarHoverHeightEnabled") &&
						  mSimulatorFeatures["AvatarHoverHeightEnabled"].asBoolean();

	// Cache mesh support data. HB
	mMeshRezEnabled = mSimulatorFeatures.has("MeshRezEnabled") &&
					  mSimulatorFeatures["MeshRezEnabled"].asBoolean();
	mMeshUploadEnabled = mSimulatorFeatures.has("MeshUploadEnabled") &&
						 mSimulatorFeatures["MeshUploadEnabled"].asBoolean();
	LL_DEBUGS("SimulatorFeatures") << "Mesh rezzing "
								   << (mMeshRezEnabled ? "enabled"
													   : " disabled")
								   << " - Mesh upload "
								   << (mMeshUploadEnabled ? "enabled"
													   : " disabled")
								   << LL_ENDL;

	// Materials-related features. HB
	if (mSimulatorFeatures.has("MaxMaterialsPerTransaction") &&
		mSimulatorFeatures["MaxMaterialsPerTransaction"].isInteger())
	{
		mMaxMaterialsPerTransaction =
			mSimulatorFeatures["MaxMaterialsPerTransaction"].asInteger();
	}
	else
	{
		LL_DEBUGS("Materials") << "Region " << getIdentity()
							   << " did not return MaxMaterialsPerTransaction, using default: 50"
							   << LL_ENDL;
	}
	if (mSimulatorFeatures.has("RenderMaterialsCapability") &&
		mSimulatorFeatures["RenderMaterialsCapability"].isReal())
	{
		F32 value = mSimulatorFeatures["RenderMaterialsCapability"].asReal();
		if (value > 0.f)
		{
			mRenderMaterialsCapability = 1.f / value;
			LL_DEBUGS("Materials") << "Region " << getIdentity()
								   << " RenderMaterialsCapability = " << value
								   << " req/s." << LL_ENDL;
		}
		else
		{
			llwarns << "Region " << getIdentity()
					<< " returned invalid RenderMaterialsCapability; using default (1 request/s)."
					<< llendl;
		}
	}
	else
	{
		LL_DEBUGS("Materials") << "Region " << getIdentity()
							   << " did not return RenderMaterialsCapability, using default (1 request/s)."
							   << LL_ENDL;
	}

	// Cache path finding support data. HB
	mDynamicPathfinding = mSimulatorFeatures.has("DynamicPathfindingEnabled");
	mDynamicPathfindingEnabled = mDynamicPathfinding &&
								 mSimulatorFeatures["DynamicPathfindingEnabled"].asBoolean();
	LL_DEBUGS("SimulatorFeatures") << "Dynamic pathfinding "
								   << (mDynamicPathfinding ?
											(mDynamicPathfindingEnabled ? "enabled"
																		: " disabled")
														   : " not supported")
								   << LL_ENDL;

	mBakesOnMeshEnabled = mSimulatorFeatures.has("BakesOnMeshEnabled") &&
						  mSimulatorFeatures["BakesOnMeshEnabled"].asBoolean();
	if (gAgent.getRegion() == this)
	{
		gAgent.setUploadedBakesLimit();
	}
	LL_DEBUGS("SimulatorFeatures") << "Bake on mesh "
								   << (mBakesOnMeshEnabled ? "enabled"
														   : "disabled")
								   << LL_ENDL;

	// We will not erase URLs passed on login (see below)...
	// This poses a problem for cross-grid TPs, when the arrival grid does not
	// provide the URLs for the corresponding services via the simulator
	// features and the departure grid did provide them via the login.cgi
	// script (and not via the simulator features either), but it is still
	// better than not erasing the sim-provided URLs when the arrival grid
	// does not provide the same services... HB
	// *TODO: detect cross-grid TPs and erase the URLs only when they occur...
	bool got_map_url = LLWorldMap::wasMapURLSetOnLogin();
	bool got_search_url = HBFloaterSearch::wasSearchURLSetOnLogin();

	// Cache OpenSim specific data. HB
	// See: http://opensimulator.org/wiki/SimulatorFeatures_Extras
	if (mSimulatorFeatures.has("OpenSimExtras"))
	{
		if (gIsInSecondLife)
		{
			llwarns << "OpenSim features detected while supposedly logged in Second Life !"
					<< llendl;
			gIsInSecondLife = false,
			gIsInProductionGrid = true;
			gIsInSecondLifeBetaGrid = gIsInSecondLifeProductionGrid = false;
			if (gViewerWindowp)
			{
				gViewerWindowp->setMenuBackgroundColor();
			}
			llinfos << "Switched to OpenSim mode/policy." << llendl;
		}

		const LLSD& extras = mSimulatorFeatures["OpenSimExtras"];

		// Export permission support
		bool old_value = mOSExportPermSupported;
		mOSExportPermSupported = extras.has("ExportSupported") &&
								 extras["ExportSupported"].asBoolean();
		if (mOSExportPermSupported != old_value)
		{
			dialog_refresh_all();
		}

		if (extras.has("map-server-url"))
		{
			LLWorldMap::gotMapServerURL(true);
			LLWorldMap::setMapServerURL(extras["map-server-url"].asString());
			LL_DEBUGS("SimulatorFeatures") << "Map server URL set to: "
										   << extras["map-server-url"].asString()
										   << LL_ENDL;
			got_map_url = true;
		}

		if (extras.has("search-server-url"))
		{
			std::string url = extras["search-server-url"].asString();
			HBFloaterSearch::setSearchURL(url);
			LL_DEBUGS("SimulatorFeatures") << "Search URL: " << url << LL_ENDL;
			got_search_url = true;
		}

		// Whisper/chat/shout ranges: seen in OS Grid...
		if (extras.has("whisper-range"))
		{
			mWhisperRange = (U32)extras["whisper-range"].asInteger();
			LL_DEBUGS("SimulatorFeatures") << "Whisper range: "
										   << mWhisperRange << "m"
										   << LL_ENDL;
		}
		if (extras.has("say-range"))
		{
			mChatRange = (U32)extras["say-range"].asInteger();
			gAgent.setNearChatRadius((F32)mChatRange * 0.5f);
			LL_DEBUGS("SimulatorFeatures") << "Chat range: " << mChatRange
										   << "m" << LL_ENDL;
		}
		if (extras.has("shout-range"))
		{
			mShoutRange = (U32)extras["shout-range"].asInteger();
			LL_DEBUGS("SimulatorFeatures") << "Shout range: " << mShoutRange
										   << "m" << LL_ENDL;
		}
	}

	if (mSimulatorFeatures.has("GridServices"))
	{
		const LLSD& services = mSimulatorFeatures["GridServices"];

		// Seen in Speculoos...
		if (services.has("search"))
		{
			std::string url = services["search"].asString();
			HBFloaterSearch::setSearchURL(url);
			LL_DEBUGS("SimulatorFeatures") << "Search URL: " << url << LL_ENDL;
			got_search_url = true;
		}
	}

	// When in OpenSim, erase the map and search URLs if not found in sim
	// features and not set at login time (see above). HB
	// *TODO: detect cross-grid TPs and erase the URLs only when they occur...
	if (!gIsInSecondLife)
	{
		if (!got_map_url)
		{
			LLWorldMap::gotMapServerURL(false);
		}
		if (!got_search_url)
		{
			HBFloaterSearch::setSearchURL("");
		}
	}

	mFeaturesReceivedSignal(getRegionID());
	// This is a single-shot signal. Forget callbacks to save resources.
	mFeaturesReceivedSignal.disconnect_all_slots();
}

boost::signals2::connection LLViewerRegion::setFeaturesReceivedCB(const caps_received_cb_t& cb)
{
	return mFeaturesReceivedSignal.connect(cb);
}

// This is called when the parent is not cacheable. Moves all orphan children
// out of the cache and inserts them into the rendering octree.
void LLViewerRegion::findOrphans(U32 parent_id)
{
	orphan_list_t::iterator iter = mOrphanMap.find(parent_id);
	if (iter != mOrphanMap.end())
	{
		std::vector<U32>* children = &mOrphanMap[parent_id];
		for (S32 i = 0, count = children->size(); i < count; ++i)
		{
			// Parent is visible, so is the child.
			addVisibleChildCacheEntry(NULL, getCacheEntry((*children)[i]));
		}
		children->clear();
		mOrphanMap.erase(parent_id);
	}
}

void LLViewerRegion::decodeBoundingInfo(LLVOCacheEntry* entry)
{
	if (!sVOCacheCullingEnabled)
	{
		gObjectList.processObjectUpdateFromCache(entry, this);
		return;
	}
	if (!entry || !entry->isValid())
	{
		return;
	}

	if (!entry->getEntry())
	{
		entry->setOctreeEntry(NULL);
	}

	if (!entry->getDP())
	{
		return;
	}

	if (entry->getEntry()->hasDrawable())
	{
		// Already in the rendering pipeline
		LLDrawable* drawablep = (LLDrawable*)entry->getEntry()->getDrawable();
		if (drawablep)
		{
			LLViewerRegion* old_regionp = drawablep->getRegion();
			if (old_regionp != this)
			{
				LLViewerObject* objp = drawablep->getVObj();
				if (objp)
				{
					// Remove from old region
					if (old_regionp)
					{
						old_regionp->killCacheEntry(objp->getLocalID());
					}

					// Change region
					objp->setRegion(this);
				}
			}
		}

		addActiveCacheEntry(entry);

		// Set parent id
		U32	parent_id = 0;
		LLViewerObject::unpackParentID(entry->getDP(), parent_id);
		if (parent_id != entry->getParentID())
		{
			entry->setParentID(parent_id);
		}

		// Update the object
		gObjectList.processObjectUpdateFromCache(entry, this);
		return; // done
	}

	// Must not be active.
	llassert_always(!entry->isState(LLVOCacheEntry::ACTIVE));
	removeFromVOCacheTree(entry); // remove from cache octree if it is in.

	LLVector3 pos, scale;
	LLQuaternion rot;

	// Decode spatial info and parent info
	U32 parent_id = LLViewerObject::extractSpatialExtents(entry->getDP(),
														  pos, scale, rot);
	U32 old_parent_id = entry->getParentID();
	bool same_old_parent = parent_id == old_parent_id;
	if (!same_old_parent) // Parent changed.
	{
		if (old_parent_id > 0) // Has an old parent, disconnect it
		{
			LLVOCacheEntry* old_parent = getCacheEntry(old_parent_id);
			if (old_parent)
			{
				old_parent->removeChild(entry);
				if (!old_parent->isState(LLVOCacheEntry::INACTIVE))
				{
					mVisibleEntries.erase(entry);
					entry->setState(LLVOCacheEntry::INACTIVE);
				}
			}
		}
		entry->setParentID(parent_id);
	}

	if (parent_id > 0) // Got a new parent
	{
		// 1.- Find the parent in cache
		LLVOCacheEntry* parent = getCacheEntry(parent_id);

		// 2.- Parent is not in the cache, put into the orphan list.
		if (!parent)
		{
			if (!same_old_parent)
			{
				// Check if parent is non-cacheable and already created
				if (isNonCacheableObjectCreated(parent_id))
				{
					// Parent is visible, so is the child.
					addVisibleChildCacheEntry(NULL, entry);
				}
				else
				{
					entry->setBoundingInfo(pos, scale);
					mOrphanMap[parent_id].push_back(entry->getLocalID());
				}
			}
			else
			{
				entry->setBoundingInfo(pos, scale);
			}
		}
		// Not in cache.
		else if (!parent->isState(LLVOCacheEntry::INACTIVE))
		{
			// Parent is visible, so is the child.
			addVisibleChildCacheEntry(parent, entry);
		}
		else
		{
			entry->setBoundingInfo(pos, scale);
			parent->addChild(entry);

			if (parent->getGroup())
			{
				// Re-insert parent to vo-cache tree because its bounding info
				// changed.
				removeFromVOCacheTree(parent);
				addToVOCacheTree(parent);
			}
		}

		return;
	}

	//
	// No parent
	//

	entry->setBoundingInfo(pos, scale);

	if (!parent_id) // a potential parent
	{
		// Find all children and update their bounding info
		orphan_list_t::iterator iter = mOrphanMap.find(entry->getLocalID());
		if (iter != mOrphanMap.end())
		{
			std::vector<U32>* orphans = &mOrphanMap[entry->getLocalID()];
			for (S32 i = 0, count = orphans->size(); i < count; ++i)
			{
				LLVOCacheEntry* child = getCacheEntry((*orphans)[i]);
				if (child)
				{
					entry->addChild(child);
				}
			}
			orphans->clear();
			mOrphanMap.erase(entry->getLocalID());
		}
	}

	if (!entry->getGroup() && entry->isState(LLVOCacheEntry::INACTIVE))
	{
		addToVOCacheTree(entry);
	}
}

LLViewerRegion::eCacheUpdateResult LLViewerRegion::cacheFullUpdate(LLDataPackerBinaryBuffer& dp,
																   U32 flags)
{
	eCacheUpdateResult result;

	U32 crc, local_id;
	LLViewerObject::unpackU32(&dp, local_id, "LocalID");
	LLViewerObject::unpackU32(&dp, crc, "CRC");

	LLVOCacheEntry* entry = getCacheEntry(local_id, false);
	if (entry)
	{
		entry->setValid();

		// We have seen this object before
		if (entry->getCRC() == crc)
		{
			// Record a hit
			entry->recordDupe();
			result = CACHE_UPDATE_DUPE;
		}
		else // CRC changed
		{
			// Update the cache entry
			entry->updateEntry(crc, dp);
			decodeBoundingInfo(entry);
			result = CACHE_UPDATE_CHANGED;
		}
	}
	else
	{
		// We have not seen this object before; create new entry and add to map
		LLPointer<LLVOCacheEntry> new_entry = new LLVOCacheEntry(local_id, crc,
																 dp);
		mCacheMap[local_id] = new_entry;
		decodeBoundingInfo(new_entry);
		entry = new_entry;
		result = CACHE_UPDATE_ADDED;
	}

	if (flags != 0xffffffff)
	{
		entry->setUpdateFlags(flags);
		LLUUID fullid;
		LLViewerObjectList::getUUIDFromLocal(fullid, local_id,
											 gMessageSystemp->getSenderIP(),
											 gMessageSystemp->getSenderPort());
		if (fullid.notNull())
		{
			LL_DEBUGS("ObjectCacheSpam") << "Set cache entry flags for object "
										 << fullid << " to: " << flags
										 << LL_ENDL;
			// We MUST also update the corresponding object when it exists ! HB
			LLViewerObject* objectp = gObjectList.findObject(fullid);
			if (objectp)
			{
				objectp->loadFlags(flags);
			}
		}
	}

	return result;
}

void LLViewerRegion::cacheFullUpdateGLTFOverride(const LLGLTFOverrideCacheEntry& data)
{
	mGLTFOverrides.emplace(data.mLocalId, data);
}

LLVOCacheEntry* LLViewerRegion::getCacheEntryForOctree(U32 local_id)
{
	if (!sVOCacheCullingEnabled)
	{
		return NULL;
	}

	LLVOCacheEntry* entry = getCacheEntry(local_id);
	removeFromVOCacheTree(entry);

	return entry;
}

LLVOCacheEntry* LLViewerRegion::getCacheEntry(U32 local_id, bool valid)
{
	LLVOCacheEntry::map_t::iterator iter = mCacheMap.find(local_id);
	if (iter != mCacheMap.end() && (!valid || iter->second->isValid()))
	{
		return iter->second;
	}
	return NULL;
}

void LLViewerRegion::addCacheMiss(U32 id,
								  LLViewerRegion::eCacheMissType miss_type)
{
#if 0
	mCacheMissList.emplace(id, miss_type);
#else
	mCacheMissList.emplace_back(id, miss_type);
#endif
}

// Checks if a non-cacheable object is already created.
bool LLViewerRegion::isNonCacheableObjectCreated(U32 local_id)
{
	return local_id > 0 && mNonCacheableCreatedList.count(local_id) != 0;
}

void LLViewerRegion::removeFromCreatedList(U32 local_id)
{
	if (local_id > 0)
	{
		non_cacheable_list_t::iterator iter =
			mNonCacheableCreatedList.find(local_id);
		if (iter != mNonCacheableCreatedList.end())
		{
			mNonCacheableCreatedList.hset_erase(iter);
		}
	}
}

void LLViewerRegion::addToCreatedList(U32 local_id)
{
	if (local_id > 0)
	{
		mNonCacheableCreatedList.insert(local_id);
	}
}

// Get data packer for this object, if we have cached data AND the CRC matches.
bool LLViewerRegion::probeCache(U32 local_id, U32 crc, U32 flags,
								U8& cache_miss_type)
{
	LLVOCacheEntry* entry = getCacheEntry(local_id, false);
	if (!entry)
	{
		addCacheMiss(local_id, CACHE_MISS_TYPE_FULL);
		return false;
	}

	if (entry->getCRC() != crc)
	{
		addCacheMiss(local_id, CACHE_MISS_TYPE_CRC);
		return false;
	}

	cache_miss_type = CACHE_MISS_TYPE_NONE;
	// We have seen this object before; record a hit.
	entry->recordHit();

	if (flags != 0xffffffff)
	{
		LL_DEBUGS("ObjectCacheSpam") << "Setting cache entry flags for object ";
		LLUUID fullid;
		LLViewerObjectList::getUUIDFromLocal(fullid, local_id,
											 gMessageSystemp->getSenderIP(),
											 gMessageSystemp->getSenderPort());
		if (fullid.notNull())
		{
			LL_CONT << fullid;
		}
		else
		{
			LL_CONT << " with local Id/from server " << local_id << "/"
					<< gMessageSystemp->getSender();
		}
		LL_CONT << " to: 0x" << std::hex << flags << std::dec << LL_ENDL;
	}
	entry->setUpdateFlags(flags);

	if (entry->isState(LLVOCacheEntry::ACTIVE))
	{
		LLDrawable* drawablep = (LLDrawable*)entry->getEntry()->getDrawable();
		if (drawablep)
		{
			LLViewerObject* obj = drawablep->getVObj();
			if (obj)
			{
				obj->loadFlags(flags);
			}
		}
	}
	else if (!entry->isValid())	// If not already probed
	{
		entry->setValid();
		decodeBoundingInfo(entry);
	}

	return true;
}

void LLViewerRegion::addCacheMissFull(U32 local_id)
{
	addCacheMiss(local_id, CACHE_MISS_TYPE_FULL);
}

void LLViewerRegion::requestCacheMisses()
{
	if (mCacheMissList.empty())
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	bool start_new_message = true;
	S32 blocks = 0;

	// Send requests for all cache-missed objects
	for (CacheMissItem::cache_miss_list_t::iterator
			iter = mCacheMissList.begin(), end = mCacheMissList.end();
		 iter != end; ++iter)
	{
		if (start_new_message)
		{
			msg->newMessageFast(_PREHASH_RequestMultipleObjects);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			start_new_message = false;
		}

		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU8Fast(_PREHASH_CacheMissType, iter->mType);
		msg->addU32Fast(_PREHASH_ID, iter->mID);

		if (++blocks >= 255)
		{
			sendReliableMessage();
			start_new_message = true;
			blocks = 0;
		}
	}

	// Finish any pending message
	if (!start_new_message)
	{
		sendReliableMessage();
	}

	mCacheDirty = true;
	mCacheMissList.clear();
}

void LLViewerRegion::dumpCache()
{
	constexpr S32 BINS = 4;
	S32 hit_bin[BINS];
	S32 change_bin[BINS];

	for (S32 i = 0; i < BINS; ++i)
	{
		hit_bin[i] = 0;
		change_bin[i] = 0;
	}

	for (LLVOCacheEntry::map_t::iterator iter = mCacheMap.begin(),
										 end = mCacheMap.end();
		 iter != end; ++iter)
	{
		LLVOCacheEntry* entry = iter->second;

		S32 hits = entry->getHitCount();
		S32 changes = entry->getCRCChangeCount();

		hits = llclamp(hits, 0, BINS - 1);
		changes = llclamp(changes, 0, BINS - 1);

		++hit_bin[hits];
		++change_bin[changes];
	}

	llinfos << "Count " << mCacheMap.size() << llendl;
	for (S32 i = 0; i < BINS; ++i)
	{
		llinfos << "Hits " << i << " " << hit_bin[i] << llendl;
	}
	for (S32 i = 0; i < BINS; ++i)
	{
		llinfos << "Changes " << i << " " << change_bin[i] << llendl;
	}
}

void LLViewerRegion::unpackRegionHandshake()
{
	LLMessageSystem* msg = gMessageSystemp;

	U8 sim_access;
	msg->getU8(_PREHASH_RegionInfo, _PREHASH_SimAccess, sim_access);

	std::string sim_name;
	msg->getString(_PREHASH_RegionInfo, _PREHASH_SimName, sim_name);

	LLUUID sim_owner;
	msg->getUUID(_PREHASH_RegionInfo, _PREHASH_SimOwner, sim_owner);

	bool manager;
	msg->getBool(_PREHASH_RegionInfo, _PREHASH_IsEstateManager, manager);

	F32 water_height;
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_WaterHeight, water_height);

	F32 billable_factor;
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_BillableFactor, billable_factor);

	LLUUID cache_id;
	msg->getUUID(_PREHASH_RegionInfo, _PREHASH_CacheID, cache_id);

	U64 region_flags = 0;
	U64 region_protocols = 0;
	if (msg->has(_PREHASH_RegionInfo4))
	{
		msg->getU64Fast(_PREHASH_RegionInfo4, _PREHASH_RegionFlagsExtended,
						region_flags);
		msg->getU64Fast(_PREHASH_RegionInfo4, _PREHASH_RegionProtocols,
						region_protocols);
	}
	else
	{
		U32 flags = 0;
		msg->getU32Fast(_PREHASH_RegionInfo, _PREHASH_RegionFlags, flags);
		region_flags = flags;
	}

	setRegionProtocols(region_protocols);
	setRegionFlags(region_flags);
	setSimAccess(sim_access);
	setRegionNameAndZone(sim_name);
	setOwner(sim_owner);
	setIsEstateManager(manager);
	setWaterHeight(water_height);
	setBillableFactor(billable_factor);
	setCacheID(cache_id);

	LL_DEBUGS("ObjectCache") << "Got hanshake message for region: " << mName
							 << " - Cache Id: " << mCacheID << LL_ENDL;

	LLUUID region_id;
	msg->getUUID(_PREHASH_RegionInfo2, _PREHASH_RegionID, region_id);
	setRegionID(region_id);

	// Retrieve the CR-53 (Homestead/Land SKU) information. The only reasonable
	// way to decide if we actually have any data is to check to see if any of
	// these fields have positive sizes.
	if (msg->getSize(_PREHASH_RegionInfo3, _PREHASH_ColoName) > 0 ||
		msg->getSize(_PREHASH_RegionInfo3, _PREHASH_ProductSKU) > 0 ||
		msg->getSize(_PREHASH_RegionInfo3, _PREHASH_ProductName) > 0)
	{
		msg->getS32(_PREHASH_RegionInfo3, _PREHASH_CPUClassID, mClassID);
		msg->getS32(_PREHASH_RegionInfo3, _PREHASH_CPURatio, mCPURatio);
		msg->getString(_PREHASH_RegionInfo3, _PREHASH_ColoName, mColoName);
		msg->getString(_PREHASH_RegionInfo3, _PREHASH_ProductSKU, mProductSKU);
		msg->getString(_PREHASH_RegionInfo3, _PREHASH_ProductName,
					   mProductName);
	}

	mCentralBakeVersion = region_protocols & 1;
	if (!gIsInSecondLife && !mBakesOnMeshEnabled)
	{
		mBakesOnMeshEnabled = region_protocols & 0x8000000000000000ll;
	}

	// Get the 4 textures for land
	LLUUID tmp_id;
	msg->getUUID(_PREHASH_RegionInfo, _PREHASH_TerrainDetail0, tmp_id);
	bool changed = tmp_id != mCompositionp->getDetailTextureID(0);
	mCompositionp->setDetailTextureID(0, tmp_id);
	msg->getUUID(_PREHASH_RegionInfo, _PREHASH_TerrainDetail1, tmp_id);
	changed |= tmp_id != mCompositionp->getDetailTextureID(1);
	mCompositionp->setDetailTextureID(1, tmp_id);
	msg->getUUID(_PREHASH_RegionInfo, _PREHASH_TerrainDetail2, tmp_id);
	changed |= tmp_id != mCompositionp->getDetailTextureID(2);
	mCompositionp->setDetailTextureID(2, tmp_id);
	msg->getUUID(_PREHASH_RegionInfo, _PREHASH_TerrainDetail3, tmp_id);
	changed |= tmp_id != mCompositionp->getDetailTextureID(3);
	mCompositionp->setDetailTextureID(3, tmp_id);

	// Get the start altitude and range values for land textures
	F32 tmp_f32;
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_TerrainStartHeight00, tmp_f32);
	changed |= tmp_f32 != mCompositionp->getStartHeight(0);
	mCompositionp->setStartHeight(0, tmp_f32);
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_TerrainStartHeight01, tmp_f32);
	changed |= tmp_f32 != mCompositionp->getStartHeight(1);
	mCompositionp->setStartHeight(1, tmp_f32);
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_TerrainStartHeight10, tmp_f32);
	changed |= tmp_f32 != mCompositionp->getStartHeight(2);
	mCompositionp->setStartHeight(2, tmp_f32);
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_TerrainStartHeight11, tmp_f32);
	changed |= tmp_f32 != mCompositionp->getStartHeight(3);
	mCompositionp->setStartHeight(3, tmp_f32);

	msg->getF32(_PREHASH_RegionInfo, _PREHASH_TerrainHeightRange00, tmp_f32);
	changed |= tmp_f32 != mCompositionp->getHeightRange(0);
	mCompositionp->setHeightRange(0, tmp_f32);
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_TerrainHeightRange01, tmp_f32);
	changed |= tmp_f32 != mCompositionp->getHeightRange(1);
	mCompositionp->setHeightRange(1, tmp_f32);
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_TerrainHeightRange10, tmp_f32);
	changed |= tmp_f32 != mCompositionp->getHeightRange(2);
	mCompositionp->setHeightRange(2, tmp_f32);
	msg->getF32(_PREHASH_RegionInfo, _PREHASH_TerrainHeightRange11, tmp_f32);
	changed |= tmp_f32 != mCompositionp->getHeightRange(3);
	mCompositionp->setHeightRange(3, tmp_f32);

	bool was_ready = mCompositionp->getParamsReady();
	if (!was_ready)
	{
		mCompositionp->setParamsReady();
	}
	else if (changed)
	{
		mLandp->dirtyAllPatches();
	}
	// Update if the land changed
	LL_DEBUGS("RegionTexture") << "Region: " << sim_name
							   << " - Composition did "
							   << (changed ? "" : "not ")
							   << "change and parameters were "
							   << (was_ready ? "" : "not " ) << "ready."
							   << LL_ENDL;

	// Now that we have the name, we can load the cache file off disk.
	loadObjectCache();

	// Reply to the handshake, but not while we already have a threaded cache
	// read in flight. In this latter case, just register a new pending
	// handshake reply until the cache got read. HB
	++mPendingHandshakes;
	if (!mCacheLoading)
	{
		sendHandshakeReply();
	}
}

// *TODO: Send all upstream viewer->sim handshake info here.
void LLViewerRegion::sendHandshakeReply()
{
	if (!mPendingHandshakes)
	{
		return;
	}
	--mPendingHandshakes;
	LL_DEBUGS("ObjectCache") << "Sending handshake reply for region: "
							 << mName << LL_ENDL;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_RegionHandshakeReply);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_RegionInfo);
	U32 flags = REGION_HANDSHAKE_SUPPORTS_SELF_APPEARANCE;
	if (sVOCacheCullingEnabled)
	{
		// Set the bit 0 to be 1 to ask sim to send all cacheable objects.
		flags |= 0x00000001;
	}
	if (mCacheMap.empty())
	{
		// Set the bit 1 to be 1 to tell sim the cache file is empty, no need
		// to send cache probes.
		flags |= 0x00000002;
	}
	msg->addU32(_PREHASH_Flags, flags);
	msg->sendReliable(mHost);
}

//static
void LLViewerRegion::buildCapabilityNames(LLSD& cap_names)
{
	cap_names.append("AbuseCategories");
	cap_names.append("AcceptFriendship");
	cap_names.append("AcceptGroupInvite");	// For use with ReadOfflineMsgs
	cap_names.append("AgentExperiences");
	cap_names.append("AgentPreferences");
	cap_names.append("AgentProfile");
	cap_names.append("AgentState");
#if LL_ANIMESH_VPARAMS
	// Added in LL's viewer-muscadine but not used at all in its code...
	// Is that a trigger like ObjectAnimation ?
	cap_names.append("AnimatedObjects");
#endif
#if 0	// *TODO: implement attachments scripts count ?
	cap_names.append("AttachmentResources");
#endif
	cap_names.append("AvatarPickerSearch");
	cap_names.append("AvatarRenderInfo");
	cap_names.append("CharacterProperties");
	cap_names.append("ChatSessionRequest");
	cap_names.append("CopyInventoryFromNotecard");
	cap_names.append("CreateInventoryCategory");
	cap_names.append("DeclineFriendship");	// For use with ReadOfflineMsgs
	cap_names.append("DeclineGroupInvite");
	cap_names.append("DirectDelivery");
	cap_names.append("DispatchRegionInfo");
#if 0	// Windlight region settings (deprecated, not used by this viewer)
	cap_names.append("EnvironmentSettings");
#endif
	cap_names.append("EstateAccess");
	cap_names.append("EstateChangeInfo");
	cap_names.append("EventQueueGet");
	if (gIsInSecondLife || gSavedSettings.getBool("UseHTTPInventory"))
	{
		cap_names.append("FetchInventory2");
		cap_names.append("FetchInventoryDescendents2");
		cap_names.append("FetchLib2");
		cap_names.append("FetchLibDescendents2");
		cap_names.append("InventoryAPIv3");
		cap_names.append("LibraryAPIv3");
	}
	cap_names.append("ExperiencePreferences");
	cap_names.append("ExperienceQuery");
	cap_names.append("ExtEnvironment");
	cap_names.append("FindExperienceByName");
	cap_names.append("GetAdminExperiences");
	cap_names.append("GetCreatorExperiences");
	cap_names.append("GetExperiences");
	cap_names.append("GetExperienceInfo");
	cap_names.append("GetDisplayNames");
	cap_names.append("GetMesh");
	cap_names.append("GetMesh2");
	cap_names.append("GetMetadata");
	cap_names.append("GetObjectCost");
	cap_names.append("GetObjectPhysicsData");
	cap_names.append("GetTexture");
	cap_names.append("GroupAPIv1");
	cap_names.append("GroupExperiences");
	cap_names.append("GroupMemberData");
	cap_names.append("HomeLocation");
	cap_names.append("InterestList");	// For 360° snapshots (not implemented)
	cap_names.append("InventoryThumbnailUpload");
	cap_names.append("IsExperienceAdmin");
	cap_names.append("IsExperienceContributor");
	cap_names.append("MapLayer");
	cap_names.append("MapLayerGod");
	cap_names.append("MeshUploadFlag");
	cap_names.append("ModifyMaterialParams");
	cap_names.append("NavMeshGenerationStatus");
	cap_names.append("NewFileAgentInventory");
	// Requesting this cap triggers the sending of UDP messages for puppets:
	cap_names.append("ObjectAnimation");
#if LL_ANIMESH_VPARAMS
	cap_names.append("ObjectExtendedAttributes");
#endif
	cap_names.append("ObjectMedia");
	cap_names.append("ObjectMediaNavigate");
	cap_names.append("ObjectNavMeshProperties");
	cap_names.append("ParcelNavigateMedia");
	cap_names.append("ParcelPropertiesUpdate");
	cap_names.append("ParcelVoiceInfoRequest");
	cap_names.append("ProductInfoRequest");
	cap_names.append("ProvisionVoiceAccountRequest");
	cap_names.append("Puppetry");
	// Needs AcceptGroupInvite & DeclineGroupInvite:
	cap_names.append("ReadOfflineMsgs");
	cap_names.append("RegionExperiences");
	cap_names.append("RegionObjects");	// Replaces "ObjectNavMeshProperties"
	cap_names.append("RemoteParcelRequest");
	cap_names.append("RenderMaterials");
	cap_names.append("RequestTextureDownload");
#if 0	// *TODO: implement in llfloaterobjectweights.cpp ?
	cap_names.append("ResourceCostSelected");
#endif
	cap_names.append("RetrieveNavMeshSrc");
	cap_names.append("SearchStatRequest");
	cap_names.append("SearchStatTracking");
	cap_names.append("SendPostcard");
	cap_names.append("SendUserReport");
	cap_names.append("SendUserReportWithScreenshot");
	cap_names.append("ServerReleaseNotes");
	cap_names.append("SetDisplayName");
	cap_names.append("SimConsole");
	cap_names.append("SimConsoleAsync");
	cap_names.append("SimulatorFeatures");
	cap_names.append("TerrainNavMeshProperties");
	cap_names.append("UntrustedSimulatorMessage");
	cap_names.append("UpdateAgentInformation");
	cap_names.append("UpdateAgentLanguage");
	cap_names.append("UpdateAvatarAppearance");
	cap_names.append("UpdateExperience");
	cap_names.append("UpdateGestureAgentInventory");
	cap_names.append("UpdateGestureTaskInventory");
	cap_names.append("UpdateMaterialAgentInventory");
	cap_names.append("UpdateMaterialTaskInventory");
	cap_names.append("UpdateNotecardAgentInventory");
	cap_names.append("UpdateNotecardTaskInventory");
	cap_names.append("UpdateScriptAgent");
	cap_names.append("UpdateScriptTask");
	cap_names.append("UpdateSettingsAgentInventory");
	cap_names.append("UpdateSettingsTaskInventory");
	cap_names.append("UploadBakedTexture");
	cap_names.append("UserInfo");
	cap_names.append("ViewerAsset");
	cap_names.append("ViewerBenefits");
	cap_names.append("ViewerStartAuction");
	cap_names.append("ViewerStats");
}

std::string LLViewerRegion::getIdentity() const
{
	std::string name;
	if (mName.empty())
	{
#if 0	// This can lead to long hiccups or "pauses" when the host is unknown
		// and the DNS resolves too slowly... HB
		name = mHost.getHostName();
#else
		name = mHost.getIPString();
#endif
	}
	else
	{
		name = mName;
	}
	if (mRegionID.isNull())
	{
		return name;
	}
	return name + " (" + mRegionID.asString() + ")";
}

void LLViewerRegion::setSeedCapability(const std::string& url)
{
	if (getCapability("Seed") == url)
	{
		std::string coro =
			gCoros.launch("LLViewerRegion::requestBaseCapabilitiesCompleteCoro",
						  boost::bind(&LLViewerRegion::requestBaseCapabilitiesCompleteCoro,
									  mHandle));
		llinfos << "Coroutine " << coro
				<< " launched for duplicate Seed for region " << getIdentity()
				<< llendl;
		return;
	}

	delete mEventPoll;
	mEventPoll = NULL;

	mFeaturesReceived = mHoverHeigthFeature = false;
	mSimulatorFeatures.clear();

	mCapabilities.clear();
	static const std::string seed_cap("Seed");
	setCapability(seed_cap, url);

	std::string coro =
		gCoros.launch("LLViewerRegion::requestBaseCapabilitiesCoro",
					  boost::bind(&LLViewerRegion::requestBaseCapabilitiesCoro,
								  mHandle));
	llinfos << "Coroutine " << coro
			<< " launched for capabilities request for region " << getIdentity()
			<< " to seed: " << url << llendl;
}

std::string add_trailing_slash_to_cap(const std::string& url)
{
	if (!url.empty() && *url.rbegin() != '/')
	{
		return url + "/";
	}
	return url;
}

void LLViewerRegion::setCapability(const std::string& name,
								   const std::string& url)
{
	// Guards against a crash seen when exiting the viewer shortly after a far
	// TP, during the shut down 'mainloop' pumping to exit coroutines: a
	// SimulatorFeatures reply may then arrive and trigger a call to gCoros
	// below, causing a crash due to the use of destroyed mutex. HB
	if (LLApp::isExiting() || gDisconnected)
	{
		return;
	}

	if (name == "EventQueueGet")
	{
		// *HACK: remember the event poll request URL to be able to relaunch it
		// for the TP race workaround. HB
		mCapabilities[name] = url;
		if (mEventPoll)
		{
			delete mEventPoll;
		}
		mEventPoll = new LLEventPoll(mHandle, mHost, url);
		// When restarting an event poll, we already have a sim name... HB
		if (!mName.empty())
		{
			mEventPoll->setRegionName(mName);
		}
	}
	else if (name == "UntrustedSimulatorMessage")
	{
		mHost.setUntrustedSimulatorCap(url);
	}
	else if (name == "SimulatorFeatures")
	{
		mCapabilities[name] = url;
		// Kick off a request for simulator features
		std::string coro_name =
			gCoros.launch("LLViewerRegion::requestSimulatorFeatureCoro",
						  boost::bind(&LLViewerRegion::requestSimulatorFeatureCoro,
									  url, mHandle));
		llinfos << "Coroutine " << coro_name
				<< " launched to request simulator features for region "
				<< getIdentity() << " from: " << url << llendl;
		// At this point, we should have a region name for this simulator. HB
		if (mEventPoll && !mName.empty())
		{
			mEventPoll->setRegionName(mName);
		}
	}
	else if (name == "Metadata")
	{
		LL_DEBUGS("Capabilities") << "Got special capability Metadata, content = "
								  << url << LL_ENDL;
	}
	// Some cached capabilities need to have a trailing slash. HB
	else if (name == "GetTexture")
	{
		mGetTextureUrl = add_trailing_slash_to_cap(url);
		mCapabilities[name] = mGetTextureUrl;
	}
	else if (name == "GetMesh")
	{
		mGetMeshUrl = add_trailing_slash_to_cap(url);
		mCapabilities[name] = mGetMeshUrl;
	}
	else if (name == "GetMesh2")
	{
		mGetMesh2Url = add_trailing_slash_to_cap(url);
		mCapabilities[name] = mGetMesh2Url;
	}
	else if (name == "ViewerAsset")
	{
		mViewerAssetUrl = add_trailing_slash_to_cap(url);
		mCapabilities[name] = mViewerAssetUrl;
	}
	else if (name == "GetDisplayNames" || name == "GetExperienceInfo")
	{
		mCapabilities[name] = add_trailing_slash_to_cap(url);
	}
	else
	{
		mCapabilities[name] = url;
	}
	if (name == "InterestList")
	{
		setInterestListMode();
	}
}

bool LLViewerRegion::setInterestListMode(bool set_default) const
{
	if (gAgent.getRegion() != this)
	{
		// Not our agent region: do not touch anything...
		return false;
	}

	const std::string& cap_url = getCapability("InterestList");
	if (cap_url.empty())
	{
		// No such capability, nothing to do.
		return false;
	}

	static LLCachedControl<bool> use_360(gSavedSettings, "Use360InterestList");
	LLSD body;
	body["mode"] = LLSD::String(!set_default && use_360  ? "360" : "default");
	return gAgent.requestPostCapability("InterestList", body);
}

// NOTE: do make sure to call this method only after the region capabilities
// have been received !
const std::string& LLViewerRegion::getTextureUrl() const
{
	bool no_asset_cap = mViewerAssetUrl.empty();
	if (no_asset_cap && gIsInSecondLife)
	{
		if (mCapabilitiesState == CAPABILITIES_STATE_RECEIVED)
		{
			llwarns_once << "Region '" << getIdentity()
						 << "' is missing ViewerAsset capability." << llendl;
		}
		else	// This should not happen if the note above is respected
		{
			llwarns_once << "Region '" << getIdentity()
						 << "' did not yet send the ViewerAsset capability."
						 << llendl;
			llassert(false);
		}
	}

	static LLCachedControl<bool> use_viewerasset(gSavedSettings,
												 "UseViewerAssetCap");
	if (use_viewerasset && !no_asset_cap)
	{
		LL_DEBUGS_ONCE("Capabilities") << "Using the ViewerAsset capability for region "
									   << getIdentity() << LL_ENDL;
		return mViewerAssetUrl;
	}
	LL_DEBUGS_ONCE("Capabilities") << "Using the GetTexture capability for region "
								   << getIdentity() << LL_ENDL;
	return mGetTextureUrl;
}

// NOTE: do make sure to call this method only after the region capabilities
// have been received !
const std::string& LLViewerRegion::getMeshUrl(bool* is_mesh2) const
{
	bool no_asset_cap = mViewerAssetUrl.empty();
	if (no_asset_cap && gIsInSecondLife)
	{
		if (mCapabilitiesState == CAPABILITIES_STATE_RECEIVED)
		{
			llwarns_once << "Region '" << getIdentity()
						 << "' is missing ViewerAsset capability." << llendl;
		}
		else	// This should not happen if the note above is respected
		{
			llwarns_once << "Region '" << getIdentity()
						 << "' did not yet send the ViewerAsset capability."
						 << llendl;
			llassert(false);
		}
	}

	if (is_mesh2)
	{
		*is_mesh2 = true;
	}
	static LLCachedControl<bool> use_getmesh2(gSavedSettings,
											  "UseGetMesh2Cap");
	static LLCachedControl<bool> use_viewerasset(gSavedSettings,
												 "UseViewerAssetCap");

	if (use_viewerasset && !no_asset_cap)
	{
		LL_DEBUGS_ONCE("Capabilities") << "Using the ViewerAsset capability for region "
									   << getIdentity() << LL_ENDL;
		return mViewerAssetUrl;
	}
	if ((gIsInSecondLife || use_getmesh2) && !mGetMesh2Url.empty())
	{
		LL_DEBUGS_ONCE("Capabilities") << "Using the GetMesh2Url capability for region "
									   << getIdentity() << LL_ENDL;
		return mGetMesh2Url;
	}

	if (is_mesh2)
	{
		*is_mesh2 = false;
	}
	LL_DEBUGS_ONCE("Capabilities") << "Using the GetMeshUrl capability for region "
								   << getIdentity() << LL_ENDL;
	return mGetMeshUrl;
}

bool LLViewerRegion::isSpecialCapabilityName(const std::string &name)
{
	return name == "EventQueueGet" || name == "UntrustedSimulatorMessage" ||
		   name == "Metadata" || name == "SimulatorFeatures";
}

const std::string& LLViewerRegion::getCapability(const char* name) const
{
	capability_map_t::const_iterator iter = mCapabilities.find(name);
	if (iter == mCapabilities.end())
	{
		return LLStringUtil::null;
	}
	return iter->second;
}

void LLViewerRegion::onCapabilitiesReceived()
{
	if (getCapability("SimulatorFeatures").empty())
	{
		llinfos << "No SimulatorFeatures capability for region: "
				<< getIdentity() << llendl;
		mMeshRezEnabled = !getCapability("GetMesh").empty() ||
						  !getCapability("GetMesh2").empty();
		mMeshUploadEnabled = !getCapability("MeshUploadFlag").empty();
		// There is no need holding back the sim features signal callbacks...
		// Just fire and disconnect them.
		mFeaturesReceived = true;
		mFeaturesReceivedSignal(getRegionID());
		mFeaturesReceivedSignal.disconnect_all_slots();
	}
}

void LLViewerRegion::setCapabilitiesReceived(bool received)
{
	mCapabilitiesState = received ? CAPABILITIES_STATE_RECEIVED
								  : CAPABILITIES_STATE_INIT;

	// Tell interested parties that we've received capabilities,
	// so that they can safely use getCapability().
	if (received)
	{
		onCapabilitiesReceived();

		mCapabilitiesReceivedSignal(getRegionID());
		// This is a single-shot signal. Forget callbacks to save resources.
		mCapabilitiesReceivedSignal.disconnect_all_slots();
	}
}

boost::signals2::connection LLViewerRegion::setCapsReceivedCB(const caps_received_cb_t& cb)
{
	return mCapabilitiesReceivedSignal.connect(cb);
}

void LLViewerRegion::logActiveCapabilities() const
{
	U32 count = 0;
	for (capability_map_t::const_iterator iter = mCapabilities.begin(),
										  end = mCapabilities.end();
		 iter != end; ++iter)
	{
		if (!iter->second.empty())
		{
			llinfos << iter->first << " URL is " << iter->second << llendl;
			++count;
		}
	}
	llinfos << "Dumped " << count << " entries." << llendl;
}

std::string LLViewerRegion::getSimHostName()
{
	return mHostName.empty() ? mHost.getHostName() : mHostName;
}

LLSpatialPartition* LLViewerRegion::getSpatialPartition(U32 type)
{
	if (type < mObjectPartition.size() && type < PARTITION_VO_CACHE)
	{
		return (LLSpatialPartition*)mObjectPartition[type];
	}
	return NULL;
}

LLVOCachePartition* LLViewerRegion::getVOCachePartition()
{
	// No need to check for mObjectPartition.size() against PARTITION_VO_CACHE
	// in release builds since mObjectPartition is always initialized in the
	// constructor of LLViewerRegion and not modified until the destructor is
	// called...
	llassert(mObjectPartition.size() > PARTITION_VO_CACHE);
	return (LLVOCachePartition*)mObjectPartition[PARTITION_VO_CACHE];
}

// The viewer can not yet distinquish between normal and estate-owned objects
// so we collapse these two bits and enable the UI if either are set
constexpr U64 ALLOW_RETURN_ENCROACHING_OBJECT =
	REGION_FLAGS_ALLOW_RETURN_ENCROACHING_OBJECT |
	REGION_FLAGS_ALLOW_RETURN_ENCROACHING_ESTATE_OBJECT;

bool LLViewerRegion::objectIsReturnable(const LLVector3& pos,
										const std::vector<LLBBox>& boxes) const
{
	return mParcelOverlay &&
		   (mParcelOverlay->isOwnedSelf(pos) ||
			mParcelOverlay->isOwnedGroup(pos) ||
			(getRegionFlag(ALLOW_RETURN_ENCROACHING_OBJECT) &&
			 mParcelOverlay->encroachesOwned(boxes)));
}

bool LLViewerRegion::childrenObjectReturnable(const std::vector<LLBBox>& boxes) const
{
	return mParcelOverlay && mParcelOverlay->encroachesOnUnowned(boxes);
}

void LLViewerRegion::getNeighboringRegions(std::vector<LLViewerRegion*>& regions)
{
	mLandp->getNeighboringRegions(regions);
}

void LLViewerRegion::getNeighboringRegionsStatus(std::vector<S32>& regions)
{
	mLandp->getNeighboringRegionsStatus(regions);
}

void LLViewerRegion::dumpSettings()
{
	llinfos << "Damage:         " << (getAllowDamage() ? "on" : "off")
								  << llendl;
	llinfos << "Landmark:       " << (getAllowLandmark() ? "on" : "off")
								  << llendl;
	llinfos << "SetHome:        " << (getAllowSetHome() ? "on" : "off")
								  << llendl;
	llinfos << "ResetHome:      " << (getResetHomeOnTeleport() ? "on" : "off")
								  << llendl;
	llinfos << "SunFixed:       " << (getSunFixed() ? "on" : "off") << llendl;
	llinfos << "Clouds updates: " << (mGotClouds ? "yes" : "no") << llendl;
	llinfos << "BlockFly:       " << (getBlockFly() ? "on" : "off") << llendl;
	llinfos << "AllowDirectTP:  " << (getAllowDirectTeleport() ? "on" : "off")
								  << llendl;
	llinfos << "Terraform:      " << (getAllowTerraform() ? "on" : "off")
								  << llendl;
	llinfos << "RestrictPush:   " << (getRestrictPushObject() ? "on" : "off")
								  << llendl;
	llinfos << "Voice:          " << (isVoiceEnabled() ? "on" : "off")
								  << llendl;
	llinfos << "Prelude:        " << (isPrelude() ? "on" : "off") << llendl;
	llinfos << "Water:          " << getWaterHeight() << "m" << llendl;
	llinfos << "Region size:    " << mWidth << "m" << llendl;
	llinfos << "Max primitives: " << mMaxTasks << llendl;
	llinfos << "MeshRezEnabled: " << (mMeshRezEnabled ? "yes" : "no") << llendl;
	llinfos << "MeshRezUpload:  " << (mMeshUploadEnabled ? "yes" : "no")
								  << llendl;
	llinfos << "PathFinding:    " << (mDynamicPathfinding ? "yes" : "no")
								  << llendl;
	llinfos << "HoverHeight:    " << (mHoverHeigthFeature ? "yes" : "no")
								  << llendl;
	llinfos << "OS export perm: " << (isOSExportPermSupported() ? "yes" : "no")
								  << llendl;
	llinfos << "WhisperRange:   " << mWhisperRange << "m" << llendl;
	llinfos << "ChatRange:      " << mChatRange << "m" << llendl;
	llinfos << "ShoutRange:     " << mShoutRange << "m" << llendl;
}
