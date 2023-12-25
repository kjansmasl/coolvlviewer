/**
 * @file llpathfindingmanager.cpp
 * @brief Implementation of llpathfindingmanager
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llpathfindingmanager.h"

#include "llapp.h"
#include "llcorehttputil.h"
#include "llhttpnode.h"
#if HAVE_PATHINGLIB
#include "llpathinglib.h"
#endif
#include "llsd.h"
#include "lltrans.h"

#include "llagent.h"
#include "llpathfindingcharacterlist.h"
#include "llpathfindinglinkset.h"
#include "llpathfindinglinksetlist.h"
#include "llpathfindingnavmesh.h"
#include "llpathfindingnavmeshstatus.h"
#include "llstartup.h"						// For getStartupState()
#include "llviewerregion.h"
#include "llweb.h"
#include "llworld.h"

//---------------------------------------------------------------------------
// LLNavMeshSimStateChangeNode
//---------------------------------------------------------------------------

class LLNavMeshSimStateChangeNode final : public LLHTTPNode
{
protected:
	LOG_CLASS(LLNavMeshSimStateChangeNode);

public:
	void post(ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override;
};
LLHTTPRegistration<LLNavMeshSimStateChangeNode>
	gHTTPRegistrationNavMeshSimStateChangeNode("/message/NavMeshStatusUpdate");

//---------------------------------------------------------------------------
// LLAgentStateChangeNode
//---------------------------------------------------------------------------

class LLAgentStateChangeNode final : public LLHTTPNode
{
protected:
	LOG_CLASS(LLAgentStateChangeNode);

public:
	void post(ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override;
};
LLHTTPRegistration<LLAgentStateChangeNode>
	gHTTPRegistrationAgentStateChangeNode("/message/AgentStateUpdate");

//---------------------------------------------------------------------------
// LinksetsResponder
//---------------------------------------------------------------------------

class LinksetsResponder
{
public:
	LinksetsResponder(LLPathfindingManager::request_id_t req_id,
					  LLPathfindingManager::object_request_cb_t cb,
					  bool object_requested, bool terrain_requested);

	void handleObjectLinksetsResult(const LLSD& content);
	void handleObjectLinksetsError();
	void handleTerrainLinksetsResult(const LLSD& content);
	void handleTerrainLinksetsError();

	typedef std::shared_ptr<LinksetsResponder> ptr_t;

private:
	void sendCallback();

private:
	LLPathfindingManager::request_id_t 			mRequestId;
	LLPathfindingManager::object_request_cb_t	mLinksetsCallback;

	LLPathfindingObjectList::ptr_t				mObjectLinksetListPtr;
	LLPathfindingObject::ptr_t					mTerrainLinksetPtr;

	typedef enum
	{
		kNotRequested,
		kWaiting,
		kReceivedGood,
		kReceivedError
	} EMessagingState;

	EMessagingState								mObjectMessagingState;
	EMessagingState								mTerrainMessagingState;
};

//---------------------------------------------------------------------------
// LLPathfindingManager
//---------------------------------------------------------------------------

LLPathfindingManager::LLPathfindingManager()
:	LLSingleton<LLPathfindingManager>(),
	mAgentStateSignal(),
	// NOTE: by using these instead of omitting the corresponding
	// xxxAndSuspend() parameters, we avoid seeing such classes constructed
	// and destroyed each time...
	mHttpOptions(DEFAULT_HTTP_OPTIONS),
	mHttpHeaders(DEFAULT_HTTP_HEADERS)
{
#if HAVE_PATHINGLIB
	if (!LLPathingLib::getInstance())
	{
		LLPathingLib::initSystem();
	}
#endif
}

LLPathfindingManager::~LLPathfindingManager()
{
	mHttpOptions.reset();
	mHttpHeaders.reset();
#if HAVE_PATHINGLIB
	if (LLPathingLib::getInstance())
	{
		LLPathingLib::quitSystem();
	}
#endif
}

#if HAVE_PATHINGLIB
bool LLPathfindingManager::isPathfindingViewEnabled() const
{
	return LLPathingLib::getInstance() != NULL;
}
#endif

bool LLPathfindingManager::isPathfindingEnabledForCurrentRegion() const
{
	return isPathfindingEnabledForRegion(gAgent.getRegion());
}

bool LLPathfindingManager::isPathfindingEnabledForRegion(LLViewerRegion* region) const
{
	return region && !region->getCapability("RetrieveNavMeshSrc").empty();
}

bool LLPathfindingManager::isAllowViewTerrainProperties() const
{
	LLViewerRegion* region = gAgent.getRegion();
	return gAgent.isGodlike() || (region && region->canManageEstate());
}

LLPathfindingNavMesh::navmesh_slot_t
	LLPathfindingManager::registerNavMeshListenerForRegion(LLViewerRegion* region,
														   LLPathfindingNavMesh::navmesh_cb_t cb)
{
	LLPathfindingNavMesh::ptr_t navmeshp = getNavMeshForRegion(region);
	return navmeshp->registerNavMeshListener(cb);
}

void LLPathfindingManager::requestGetNavMeshForRegion(LLViewerRegion* region,
													  bool get_status_only)
{
	LLPathfindingNavMesh::ptr_t navmeshp = getNavMeshForRegion(region);

	if (!region)
	{
		navmeshp->handleNavMeshNotEnabled();
		return;
	}

	if (!region->capabilitiesReceived())
	{
		navmeshp->handleNavMeshWaitForRegionLoad();
		region->setCapsReceivedCB(boost::bind(&LLPathfindingManager::handleDeferredGetNavMeshForRegion,
											  this, _1, get_status_only));
		return;
	}

	if (!isPathfindingEnabledForRegion(region))
	{
		navmeshp->handleNavMeshNotEnabled();
		return;
	}

	std::string status_url = getNavMeshStatusURLForRegion(region);
	if (status_url.empty())
	{
		llassert(false);
		return;
	}

	navmeshp->handleNavMeshCheckVersion();

	gCoros.launch("LLPathfindingManager::navMeshStatusRequestCoro",
				  boost::bind(&LLPathfindingManager::navMeshStatusRequestCoro,
							  this, status_url, region->getHandle(),
							  get_status_only));
}

void LLPathfindingManager::requestGetLinksets(request_id_t req_id,
											  object_request_cb_t cb) const
{
	LLPathfindingObjectList::ptr_t empty_linkset_list;

	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
	{
		cb(req_id, kRequestNotEnabled, empty_linkset_list);
		return;
	}

	if (!region->capabilitiesReceived())
	{
		cb(req_id, kRequestStarted, empty_linkset_list);
		region->setCapsReceivedCB(boost::bind(&LLPathfindingManager::handleDeferredGetLinksetsForRegion,
											  this, _1, req_id, cb));
		return;
	}

	std::string object_url = getRetrieveObjectLinksetsURLForCurrentRegion();
	std::string terrain_url = getTerrainLinksetsURLForCurrentRegion();
	if (object_url.empty() || terrain_url.empty())
	{
		cb(req_id, kRequestNotEnabled, empty_linkset_list);
		return;
	}

	cb(req_id, kRequestStarted, empty_linkset_list);

	bool with_terrain = isAllowViewTerrainProperties();
	LinksetsResponder::ptr_t responder(new LinksetsResponder(req_id, cb, true,
															 with_terrain));
	gCoros.launch("LLPathfindingManager::linksetObjectsCoro",
				  boost::bind(&LLPathfindingManager::linksetObjectsCoro, this,
							  object_url, responder, LLSD()));
	if (with_terrain)
	{
		gCoros.launch("LLPathfindingManager::linksetTerrainCoro",
					  boost::bind(&LLPathfindingManager::linksetTerrainCoro,
								  this, terrain_url, responder, LLSD()));
	}
}

void LLPathfindingManager::requestSetLinksets(request_id_t req_id,
											  const LLPathfindingObjectList::ptr_t& pobjects,
											  LLPathfindingLinkset::ELinksetUse use,
											  S32 a, S32 b, S32 c, S32 d,
											  object_request_cb_t cb) const
{
	LLPathfindingObjectList::ptr_t empty_linkset_list;

	std::string object_url = getChangeObjectLinksetsURLForCurrentRegion();
	std::string terrain_url = getTerrainLinksetsURLForCurrentRegion();
	if (object_url.empty() || terrain_url.empty())
	{
		cb(req_id, kRequestNotEnabled, empty_linkset_list);
		return;
	}

	if (!pobjects || pobjects->isEmpty())
	{
		cb(req_id, kRequestCompleted, empty_linkset_list);
		return;
	}

	const LLPathfindingLinksetList* list = pobjects.get()->asLinksetList();
	if (!list)
	{
		llassert(false);
		return;
	}

	LLSD object_data = list->encodeObjectFields(use, a, b, c, d);

	LLSD terrain_data;
	if (isAllowViewTerrainProperties())
	{
		terrain_data = list->encodeTerrainFields(use, a, b, c, d);
	}

	bool got_object = !object_data.isUndefined();
	bool got_terrain = !terrain_data.isUndefined();
	if (!got_object && !got_terrain)
	{
		cb(req_id, kRequestCompleted, empty_linkset_list);
		return;
	}

	cb(req_id, kRequestStarted, empty_linkset_list);

	LinksetsResponder::ptr_t responder(new LinksetsResponder(req_id, cb,
															 got_object,
															 got_terrain));
	if (got_object)
	{
		gCoros.launch("LLPathfindingManager::linksetObjectsCoro",
					  boost::bind(&LLPathfindingManager::linksetObjectsCoro,
								  this, object_url, responder, object_data));
	}

	if (got_terrain)
	{
		gCoros.launch("LLPathfindingManager::linksetTerrainCoro",
					  boost::bind(&LLPathfindingManager::linksetTerrainCoro,
								  this, terrain_url, responder, terrain_data));
	}
}

void LLPathfindingManager::requestGetCharacters(request_id_t req_id,
												object_request_cb_t cb) const
{
	LLPathfindingObjectList::ptr_t empty_char_list;

	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
	{
		cb(req_id, kRequestNotEnabled, empty_char_list);
		return;
	}

	if (!region->capabilitiesReceived())
	{
		cb(req_id, kRequestStarted, empty_char_list);
		region->setCapsReceivedCB(boost::bind(&LLPathfindingManager::handleDeferredGetCharactersForRegion,
											  this, _1, req_id, cb));
		return;
	}

	std::string char_url = getCharactersURLForCurrentRegion();
	if (char_url.empty())
	{
		cb(req_id, kRequestNotEnabled, empty_char_list);
		return;
	}

	cb(req_id, kRequestStarted, empty_char_list);

	gCoros.launch("LLPathfindingManager::charactersCoro",
				  boost::bind(&LLPathfindingManager::charactersCoro,
							  this, char_url, req_id, cb));
}

LLPathfindingManager::agent_state_slot_t LLPathfindingManager::registerAgentStateListener(agent_state_cb_t cb)
{
	return mAgentStateSignal.connect(cb);
}

void LLPathfindingManager::requestGetAgentState()
{
	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
	{
		mAgentStateSignal(false);
		return;
	}

	if (!region->capabilitiesReceived())
	{
		region->setCapsReceivedCB(boost::bind(&LLPathfindingManager::handleDeferredGetAgentStateForRegion,
											  this, _1));
		return;
	}

	if (!isPathfindingEnabledForRegion(region))
	{
		mAgentStateSignal(false);
		return;
	}

	std::string agent_url = getAgentStateURLForRegion(region);
	if (agent_url.empty())
	{
		llassert(false);
		llwarns << "Missing agent state capability !" << llendl;
		return;
	}

	gCoros.launch("LLPathfindingManager::navAgentStateRequestCoro",
				  boost::bind(&LLPathfindingManager::navAgentStateRequestCoro,
							  this, agent_url));
}

void LLPathfindingManager::requestRebakeNavMesh(rebake_navmesh_cb_t cb)
{
	LLViewerRegion* region = gAgent.getRegion();
	if (!region || !isPathfindingEnabledForRegion(region))
	{
		cb(false);
		return;
	}

	std::string status_url = getNavMeshStatusURLForCurrentRegion();
	if (status_url.empty())
	{
		return;
	}

	gCoros.launch("LLPathfindingManager::navMeshRebakeCoro",
				  boost::bind(&LLPathfindingManager::navMeshRebakeCoro,
							  this, status_url, cb));
}

void LLPathfindingManager::handleDeferredGetAgentStateForRegion(const LLUUID& region_id)
{
	LLViewerRegion* region = gAgent.getRegion();
	if (region && region->getRegionID() == region_id)
	{
		requestGetAgentState();
	}
}

void LLPathfindingManager::handleDeferredGetNavMeshForRegion(const LLUUID& region_id,
															 bool get_status_only)
{
	LLViewerRegion* region = gAgent.getRegion();
	if (region && region->getRegionID() == region_id)
	{
		requestGetNavMeshForRegion(region, get_status_only);
	}
}

void LLPathfindingManager::handleDeferredGetLinksetsForRegion(const LLUUID& region_id,
															  request_id_t req_id,
															  object_request_cb_t cb) const
{
	LLViewerRegion* region = gAgent.getRegion();
	if (region && region->getRegionID() == region_id)
	{
		requestGetLinksets(req_id, cb);
	}
}

void LLPathfindingManager::handleDeferredGetCharactersForRegion(const LLUUID& region_id,
																request_id_t req_id,
																object_request_cb_t cb) const
{
	LLViewerRegion* region = gAgent.getRegion();
	if (region && region->getRegionID() == region_id)
	{
		requestGetCharacters(req_id, cb);
	}
}

void LLPathfindingManager::navMeshStatusRequestCoro(const std::string& url,
													U64 region_handle,
													bool get_status_only)
{
	LLViewerRegion* region = gWorld.getRegionFromHandle(region_handle);
	if (!region || !region->isAlive())
	{
		// No agent region is set before the STATE_WORLD_INIT step has been
		// completed, and login region goes "live" only at SATE_STARTED, so
		// only emit a warning when fully logged in.
		if (LLStartUp::isLoggedIn())
		{
			llwarns << "Region is gone. Navmesh status request aborted."
					<< llendl;
		}
		return;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("NavMeshStatusRequest");
	LLSD result = adapter.getAndSuspend(url, mHttpOptions, mHttpHeaders);

	if (!instanceExists() || LLApp::isExiting())
	{
		return;	// Viewer is being closed down !
	}

	region = gWorld.getRegionFromHandle(region_handle);
	if (!region || !region->isAlive())
	{
		llwarns << "Region is gone. Ignoring response." << llendl;
		return;
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Navmesh status request failed (1): " << status.toString()
				<< llendl;
		return;
	}

	LLUUID region_id = region->getRegionID();
	LLPathfindingNavMeshStatus nmstatus(region_id);
	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
	LL_DEBUGS("NavMesh") << "Results (1): " << result << LL_ENDL;

	nmstatus = LLPathfindingNavMeshStatus(region_id, result);
	LLPathfindingNavMesh::ptr_t navmeshp = getNavMeshForRegion(region_id);

	if (!nmstatus.isValid())
	{
		navmeshp->handleNavMeshError();
		return;
	}
	if (navmeshp->hasNavMeshVersion(nmstatus))
	{
		navmeshp->handleRefresh(nmstatus);
		return;
	}
	if (get_status_only)
	{
		navmeshp->handleNavMeshNewVersion(nmstatus);
		return;
	}

	std::string nav_mesh_url = getRetrieveNavMeshURLForRegion(region);
	if (nav_mesh_url.empty())
	{
		navmeshp->handleNavMeshNotEnabled();
		return;
	}

	navmeshp->handleNavMeshStart(nmstatus);

	LLSD post_data;
	result = adapter.postAndSuspend(nav_mesh_url, post_data, mHttpOptions,
									mHttpHeaders);

	if (!instanceExists() || LLApp::isExiting())
	{
		return;	// Viewer is being closed down !
	}

	status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);

	region = gWorld.getRegionFromHandle(region_handle);
	if (!region || !region->isAlive())
	{
		llwarns << "Region is gone (2). Flaging navmesh as disabled."
				<< llendl;
		navmeshp->handleNavMeshNotEnabled();
		return;
	}

	U32 version = nmstatus.getVersion();
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("NavMesh") << "Results (2): " << result << LL_ENDL;
		navmeshp->handleNavMeshResult(result, version);
	}
	else
	{
		llwarns << "Navmesh status request failed (2): " << status.toString()
				<< llendl;
		navmeshp->handleNavMeshError(version);
	}
}

void LLPathfindingManager::navAgentStateRequestCoro(const std::string& url)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("NavAgentStateRequest");
	LLSD result = adapter.getAndSuspend(url, mHttpOptions, mHttpHeaders);

	if (!instanceExists() || LLApp::isExiting())
	{
		return;	// Viewer is being closed down !
	}

	bool can_rebake = false;

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		if (result.has("can_modify_navmesh") &&
			result.get("can_modify_navmesh").isBoolean())
		{
			can_rebake = result["can_modify_navmesh"].asBoolean();
		}
		else
		{
			llwarns << "Malformed agent state response: " << result << llendl;
		}
	}
	else
	{
		llwarns << "Agent state request failed: " << status.toString()
				<< llendl;
	}

	handleAgentState(can_rebake);
}

void LLPathfindingManager::navMeshRebakeCoro(const std::string& url,
											 rebake_navmesh_cb_t cd)
{
	LLSD post_data = LLSD::emptyMap();
	post_data["command"] = "rebuild";

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("NavMeshRebake");
	LLSD result = adapter.postAndSuspend(url, post_data, mHttpOptions,
										 mHttpHeaders);

	if (!instanceExists() || LLApp::isExiting())
	{
		return;	// Viewer is being closed down !
	}

	bool success = true;

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Navmesh rebake request failed: " << status.toString()
				<< llendl;
		success = false;
	}

	cd(success);
}

// If called with put_data undefined this coroutine will issue a get. If there
// is data in put_data it will be PUT to the URL.
void LLPathfindingManager::linksetObjectsCoro(const std::string& url,
											  LinksetsResponder::ptr_t responder,
											  LLSD put_data) const
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("LinksetObjects");

	LLSD result;
	if (put_data.isUndefined())
	{
		result = adapter.getAndSuspend(url, mHttpOptions, mHttpHeaders);
	}
	else
	{
		result = adapter.putAndSuspend(url, put_data, mHttpOptions,
									   mHttpHeaders);
	}

	if (!instanceExists() || LLApp::isExiting())
	{
		return;	// Viewer is being closed down !
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("NavMesh") << "Results: " << result << LL_ENDL;
		responder->handleObjectLinksetsResult(result);
	}
	else
	{
		llwarns << "Linkset objects request failed: " << status.toString()
				<< llendl;
		responder->handleObjectLinksetsError();
	}
}

// If called with put_data undefined this coroutine will issue a GET. If there
// is data in put_data it will be PUT to the URL.
void LLPathfindingManager::linksetTerrainCoro(const std::string& url,
											  LinksetsResponder::ptr_t responder,
											  LLSD put_data) const
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("LinksetTerrain");

	LLSD result;
	if (put_data.isUndefined())
	{
		result = adapter.getAndSuspend(url, mHttpOptions, mHttpHeaders);
	}
	else
	{
		result = adapter.putAndSuspend(url, put_data, mHttpOptions,
									   mHttpHeaders);
	}

	if (!instanceExists() || LLApp::isExiting())
	{
		return;	// Viewer is being closed down !
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("NavMesh") << "Results: " << result << LL_ENDL;
		responder->handleTerrainLinksetsResult(result);
	}
	else
	{
		llwarns << "Linkset terrain request failed: " << status.toString()
				<< llendl;
		responder->handleTerrainLinksetsError();
	}
}

void LLPathfindingManager::charactersCoro(const std::string& url,
										  request_id_t req_id,
										  object_request_cb_t cb) const
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("Characters");
	LLSD result = adapter.getAndSuspend(url, mHttpOptions, mHttpHeaders);

	if (!instanceExists() || LLApp::isExiting())
	{
		return;	// Viewer is being closed down !
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("NavMesh") << "Results: " << result << LL_ENDL;
		LLPathfindingObjectList::ptr_t char_list =
			LLPathfindingObjectList::ptr_t(new LLPathfindingCharacterList(result));
		cb(req_id, LLPathfindingManager::kRequestCompleted, char_list);
	}
	else
	{
		llwarns << "Characters request failed: " << status.toString()
				<< llendl;
		LLPathfindingObjectList::ptr_t char_list =
			LLPathfindingObjectList::ptr_t(new LLPathfindingCharacterList());
		cb(req_id, LLPathfindingManager::kRequestError, char_list);
	}
}

void LLPathfindingManager::handleNavMeshStatusUpdate(const LLPathfindingNavMeshStatus& status)
{
	LLPathfindingNavMesh::ptr_t navmeshp =
		getNavMeshForRegion(status.getRegionUUID());

	if (!status.isValid())
	{
		navmeshp->handleNavMeshError();
	}
	else
	{
		navmeshp->handleNavMeshNewVersion(status);
	}
}

void LLPathfindingManager::handleAgentState(bool can_rebake_region)
{
	mAgentStateSignal(can_rebake_region);
}

LLPathfindingNavMesh::ptr_t LLPathfindingManager::getNavMeshForRegion(const LLUUID& region_id)
{
	LLPathfindingNavMesh::ptr_t navmeshp;

	map_t::iterator iter = mNavMeshMap.find(region_id);
	if (iter == mNavMeshMap.end())
	{
		navmeshp =
			LLPathfindingNavMesh::ptr_t(new LLPathfindingNavMesh(region_id));
		mNavMeshMap[region_id] = navmeshp;
	}
	else
	{
		navmeshp = iter->second;
	}

	return navmeshp;
}

LLPathfindingNavMesh::ptr_t LLPathfindingManager::getNavMeshForRegion(LLViewerRegion* region)
{
	LLUUID region_id;
	if (region)
	{
		region_id = region->getRegionID();
	}
	return getNavMeshForRegion(region_id);
}

const std::string LLPathfindingManager::getNavMeshStatusURLForCurrentRegion() const
{
	return getRegionCapability(gAgent.getRegion(), "NavMeshGenerationStatus");
}

const std::string LLPathfindingManager::getNavMeshStatusURLForRegion(LLViewerRegion* region) const
{
	return getRegionCapability(region, "NavMeshGenerationStatus");
}

const std::string LLPathfindingManager::getRetrieveNavMeshURLForRegion(LLViewerRegion* region) const
{
	return getRegionCapability(region, "RetrieveNavMeshSrc");
}

const std::string LLPathfindingManager::getRetrieveObjectLinksetsURLForCurrentRegion() const
{
	return gAgent.getRegionCapability("RegionObjects");
}

const std::string LLPathfindingManager::getChangeObjectLinksetsURLForCurrentRegion() const
{
	return gAgent.getRegionCapability("ObjectNavMeshProperties");
}

const std::string LLPathfindingManager::getTerrainLinksetsURLForCurrentRegion() const
{
	return getRegionCapability(gAgent.getRegion(), "TerrainNavMeshProperties");
}

const std::string LLPathfindingManager::getCharactersURLForCurrentRegion() const
{
	return getRegionCapability(gAgent.getRegion(), "CharacterProperties");
}

const std::string LLPathfindingManager::getAgentStateURLForRegion(LLViewerRegion* region) const
{
	return getRegionCapability(region, "AgentState");
}

const std::string LLPathfindingManager::getRegionCapability(LLViewerRegion* region,
															const char* cap_name) const
{
	const std::string& url = region ? region->getCapability(cap_name)
									: LLStringUtil::null;
	if (url.empty())
	{
		llwarns << "Cannot find capability '" << cap_name
				<< "' for current region '"
				<< (region ? region->getIdentity() : "<null>") << "'"
				<< llendl;
	}
	return url;
}

//---------------------------------------------------------------------------
// LLNavMeshSimStateChangeNode
//---------------------------------------------------------------------------

void LLNavMeshSimStateChangeNode::post(ResponsePtr response,
									   const LLSD& context,
									   const LLSD& input) const
{
	if (!input.has("body") || !input.get("body").isMap())
	{
		llwarns << "Invalid response !" << llendl;
		return;
	}

	LLPathfindingNavMeshStatus nmstatus(input.get("body"));
	LLPathfindingManager::getInstance()->handleNavMeshStatusUpdate(nmstatus);
}

//---------------------------------------------------------------------------
// LLAgentStateChangeNode
//---------------------------------------------------------------------------

void LLAgentStateChangeNode::post(ResponsePtr response, const LLSD& context,
								  const LLSD& input) const
{
	if (input.has("body") && input.get("body").isMap() &&
		input.get("body").has("can_modify_navmesh") &&
		input.get("body").get("can_modify_navmesh").isBoolean())
	{
		bool rebake_ok =
			input.get("body").get("can_modify_navmesh").asBoolean();
		LLPathfindingManager::getInstance()->handleAgentState(rebake_ok);
	}
	else
	{
		llwarns << "Invalid response !" << llendl;
	}
}

//---------------------------------------------------------------------------
// LinksetsResponder
//---------------------------------------------------------------------------

LinksetsResponder::LinksetsResponder(LLPathfindingManager::request_id_t req_id,
									 LLPathfindingManager::object_request_cb_t cb,
									 bool object_requested,
									 bool terrain_requested)
:	mRequestId(req_id),
	mLinksetsCallback(cb),
	mObjectMessagingState(object_requested ? kWaiting : kNotRequested),
	mTerrainMessagingState(terrain_requested ? kWaiting : kNotRequested),
	mObjectLinksetListPtr(),
	mTerrainLinksetPtr()
{
}

void LinksetsResponder::handleObjectLinksetsResult(const LLSD& content)
{
	mObjectLinksetListPtr =
		LLPathfindingObjectList::ptr_t(new LLPathfindingLinksetList(content));

	mObjectMessagingState = kReceivedGood;
	if (mTerrainMessagingState != kWaiting)
	{
		sendCallback();
	}
}

void LinksetsResponder::handleObjectLinksetsError()
{
	mObjectMessagingState = kReceivedError;
	if (mTerrainMessagingState != kWaiting)
	{
		sendCallback();
	}
}

void LinksetsResponder::handleTerrainLinksetsResult(const LLSD& content)
{
	mTerrainLinksetPtr =
		LLPathfindingObject::ptr_t(new LLPathfindingLinkset(content));

	mTerrainMessagingState = kReceivedGood;
	if (mObjectMessagingState != kWaiting)
	{
		sendCallback();
	}
}

void LinksetsResponder::handleTerrainLinksetsError()
{
	mTerrainMessagingState = kReceivedError;
	if (mObjectMessagingState != kWaiting)
	{
		sendCallback();
	}
}

void LinksetsResponder::sendCallback()
{
	llassert(mObjectMessagingState != kWaiting &&
			 mTerrainMessagingState != kWaiting);

	LLPathfindingManager::ERequestStatus req_status;
	if ((mObjectMessagingState == kReceivedGood ||
		 mObjectMessagingState == kNotRequested) &&
		(mTerrainMessagingState == kReceivedGood ||
		  mTerrainMessagingState == kNotRequested))
	{
		req_status = LLPathfindingManager::kRequestCompleted;
	}
	else
	{
		req_status = LLPathfindingManager::kRequestError;
	}

	if (mObjectMessagingState != kReceivedGood)
	{
		mObjectLinksetListPtr =
			LLPathfindingObjectList::ptr_t(new LLPathfindingLinksetList());
	}

	if (mTerrainMessagingState == kReceivedGood)
	{
		mObjectLinksetListPtr->update(mTerrainLinksetPtr);
	}

	mLinksetsCallback(mRequestId, req_status, mObjectLinksetListPtr);
}
