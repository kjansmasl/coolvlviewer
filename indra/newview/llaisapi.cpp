/**
 * @file llaisapi.cpp
 * @brief classes and functions implementation for interfacing with the v3+ ais
 * inventory service.
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

#include <utility>					// For std::move

#include "llaisapi.h"

#include "llcallbacklist.h"
#include "llnotifications.h"
#include "llsdutil.h"

#include "llagent.h"
#include "llappviewer.h"			// For gDisconnected
#include "llinventorymodel.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

// AIS3 allows '*' requests, but in reality those will be cut at some point.
// Specify our own depth to be able to anticipate it and mark folders as
// incomplete.
constexpr U32 MAX_FOLDER_DEPTH_REQUEST = 50;

//static
std::list<AISAPI::ais_query_item_t> AISAPI::sPostponedQuery;

//-----------------------------------------------------------------------------
// Classes for AISv3 support.
//-----------------------------------------------------------------------------

//static
bool AISAPI::isAvailable(bool override_setting)
{
	static LLCachedControl<bool> use_ais(gSavedSettings, "UseAISForInventory");
	bool available = (override_setting || use_ais) &&
					 gAgent.hasRegionCapability("InventoryAPIv3");

	static bool pool_created = false;
	if (available && !pool_created)
	{
		pool_created = true;
		LLCoprocedureManager::getInstance()->initializePool("AIS");
	}

	return available;
}

//static
bool AISAPI::getInvCap(std::string& cap)
{
	cap = gAgent.getRegionCapability("InventoryAPIv3");
	return !cap.empty();
}

//static
bool AISAPI::getLibCap(std::string& cap)
{
	cap = gAgent.getRegionCapability("LibraryAPIv3");
	return !cap.empty();
}

// I may be suffering from golden hammer here, but the first part of this bind
// is actually a static cast for &HttpCoroutineAdapter::postAndSuspend so that
// the compiler can identify the correct signature to select. Reads as follows:
// LLSD										: method returning LLSD
// (LLCoreHttpUtil::HttpCoroutineAdapter::*): pointer to member function of
//											  HttpCoroutineAdapter
// (const std::string&, const LLSD&, LLCore::HttpOptions::ptr_t,
//  LLCore::HttpHeaders::ptr_t)				: method signature
#define COROCAST(T)  static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, const LLSD&, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)
#define COROCAST2(T) static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)
#define COROCAST3(T) static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, const std::string, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)

//static
void AISAPI::createInventory(const LLUUID& parent_id, const LLSD& inventory,
							 completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}
	LLUUID tid;
	tid.generate();
	url += "/category/" + parent_id.asString() + "?tid=" + tid.asString();
	LL_DEBUGS("Inventory") << "url: " << url << " - New inventory:\n"
						   << ll_pretty_print_sd(inventory) << LL_ENDL;

	invokationFn_t postfn =
		boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::postAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _3, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, postfn, url,
						 parent_id, inventory, callback, CREATEINVENTORY));
	enqueueAISCommand("createInventory", proc);
}

//static
void AISAPI::slamFolder(const LLUUID& folder_id, const LLSD& new_inventory,
						completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}
	LLUUID tid;
	tid.generate();
	url += "/category/" + folder_id.asString() + "/links?tid=" +
		   tid.asString();
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t putfn =
		boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::putAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _3, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, putfn, url,
						 folder_id, new_inventory, callback, SLAMFOLDER));
	enqueueAISCommand("slamFolder", proc);
}

//static
void AISAPI::removeCategory(const LLUUID& cat_id, completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}
	url += "/category/" + cat_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t delfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::deleteAndSuspend),
							  // _1 -> adapter
							  // _2 -> url
							  // _3 -> body
							  // _4 -> options
							  // _5 -> headers
							  _1, _2, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, delfn, url,
						 cat_id, LLSD(), callback, REMOVECATEGORY));
	enqueueAISCommand("removeCategory", proc);
}

//static
void AISAPI::removeItem(const LLUUID& item_id, completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}
	url += "/item/" + item_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t delfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::deleteAndSuspend),
							  // _1 -> adapter
							  // _2 -> url
							  // _3 -> body
							  // _4 -> options
							  // _5 -> headers
							  _1, _2, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, delfn, url,
						 item_id, LLSD(), callback, REMOVEITEM));
	enqueueAISCommand("RemoveItem", proc);
}

//static
void AISAPI::copyLibraryCategory(const LLUUID& source_id,
								 const LLUUID& dest_id, bool copy_subfolders,
								 completion_t callback)
{
	std::string url;
	if (!getLibCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}
	LL_DEBUGS("Inventory") << "Copying library category: " << source_id
						   << " => " << dest_id << LL_ENDL;
	LLUUID tid;
	tid.generate();
	url += "/category/" + source_id.asString() + "?tid=" + tid.asString();
	if (!copy_subfolders)
	{
		url += ",depth=0";
	}
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	std::string destination = dest_id.asString();
	invokationFn_t copyfn =
		boost::bind(COROCAST3(&LLCoreHttpUtil::HttpCoroutineAdapter::copyAndSuspend),
							  // _1 -> adapter
							  // _2 -> url
							  // _3 -> body
							  // _4 -> options
							  // _5 -> headers
							  _1, _2, destination, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, copyfn, url,
						 dest_id, LLSD(), callback, COPYLIBRARYCATEGORY));
	enqueueAISCommand("copyLibraryCategory", proc);
}

//static
void AISAPI::purgeDescendents(const LLUUID& cat_id, completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}
	url += "/category/" + cat_id.asString() + "/children";
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t delfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::deleteAndSuspend),
							  // _1 -> adapter
							  // _2 -> url
							  // _3 -> body
							  // _4 -> options
							  // _5 -> headers
							  _1, _2, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, delfn, url,
						 cat_id, LLSD(), callback, PURGEDESCENDENTS));
	enqueueAISCommand("purgeDescendents", proc);
}

//static
void AISAPI::updateCategory(const LLUUID& cat_id, const LLSD& updates,
							completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}
	url += "/category/" + cat_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << " - Request:\n"
						   << ll_pretty_print_sd(updates) << LL_ENDL;

	invokationFn_t patchfn =
		boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::patchAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _3, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, patchfn, url,
						 cat_id, updates, callback, UPDATECATEGORY));
	enqueueAISCommand("updateCategory", proc);
}

//static
void AISAPI::updateItem(const LLUUID& item_id, const LLSD& updates,
						completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}
	url += "/item/" + item_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << " - Request:\n"
						   << ll_pretty_print_sd(updates) << LL_ENDL;

	invokationFn_t patchfn =
		boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::patchAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _3, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, patchfn, url,
						 item_id, updates, callback, UPDATEITEM));
	enqueueAISCommand("updateItem", proc);
}

//static
void AISAPI::fetchItem(const LLUUID& item_id, bool library,
					   completion_t callback)
{
	std::string url;
	bool has_cap = library ? getLibCap(url) : getInvCap(url);
	if (!has_cap)
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	url += "/item/" + item_id.asString();
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t getfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, getfn, url,
						 item_id, LLSD(), callback, FETCHITEM));
	enqueueAISCommand("fetchItem", proc);
}

//static
void AISAPI::fetchCategoryChildren(const LLUUID& cat_id, bool library,
								   bool recursive, completion_t callback,
								   U32 depth)
{
	std::string url;
	bool has_cap = library ? getLibCap(url) : getInvCap(url);
	if (!has_cap)
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	url += "/category/" + cat_id.asString() + "/children";

	if (recursive || depth > MAX_FOLDER_DEPTH_REQUEST)
	{
		// Can specify depth=*, but server side is going to cap requests and
		// reject everything "over the top".
		depth = MAX_FOLDER_DEPTH_REQUEST;
	}
	url += llformat("?depth=%u" , depth);

	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t getfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _4, _5);

	// getAndSuspend() does not use a body, so we can pass additional data.
	LLSD body;
	body["depth"] = depth;
	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, getfn, url,
						 cat_id, body, callback, FETCHCATEGORYCHILDREN));
	enqueueAISCommand("fetchCategoryChildren", proc);
}

//static
void AISAPI::fetchCategoryCategories(const LLUUID& cat_id, bool library,
									 bool recursive, completion_t callback,
									 U32 depth)
{
	std::string url;
	bool has_cap = library ? getLibCap(url) : getInvCap(url);
	if (!has_cap)
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	url += "/category/" + cat_id.asString() + "/categories";

	if (recursive || depth > MAX_FOLDER_DEPTH_REQUEST)
	{
		// Can specify depth=*, but server side is going to cap requests and
		// reject everything "over the top".
		depth = MAX_FOLDER_DEPTH_REQUEST;
	}
	url += llformat("?depth=%u" , depth);

	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t getfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _4, _5);

	// getAndSuspend() does not use a body, so we can pass additional data.
	LLSD body;
	body["depth"] = depth;
	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, getfn, url,
						 cat_id, body, callback, FETCHCATEGORYCATEGORIES));
	enqueueAISCommand("fetchCategoryCategories", proc);
}

//static
void AISAPI::fetchCategorySubset(const LLUUID& cat_id,
								 const uuid_vec_t& children, bool library,
								 bool recursive, completion_t callback,
								 U32 depth)
{
	if (children.empty())
	{
		llwarns << "Empty request" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	std::string url;
	bool has_cap = library ? getLibCap(url) : getInvCap(url);
	if (!has_cap)
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	url += "/category/" + cat_id.asString() + "/children";

	if (recursive || depth > MAX_FOLDER_DEPTH_REQUEST)
	{
		// Can specify depth=*, but server side is going to cap requests and
		// reject everything "over the top".
		depth = MAX_FOLDER_DEPTH_REQUEST;
	}
	url += llformat("?depth=%u&children=" , depth);

	for (U32 i = 0, count = children.size(); i < count; ++i)
	{
		if (i)
		{
			url.append(",");
		}
		url += children[i].asString();
	}

	// RFC documentation specifies a maximum length of 2048
	constexpr size_t MAX_URL_LENGTH = 2000;
	if (url.size() > MAX_URL_LENGTH)
	{
		llwarns << "Request url is too long, url: " << url << llendl;
	}
	else
	{
		LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;
	}

	invokationFn_t getfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _4, _5);

	// getAndSuspend() does not use a body, so we can pass additional data.
	LLSD body;
	body["depth"] = depth;
	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, getfn, url,
						 cat_id, body, callback, FETCHCATEGORYSUBSET));
	enqueueAISCommand("fetchCategorySubset", proc);
}

//static
void AISAPI::fetchCategoryLinks(const LLUUID& cat_id, completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	url += "/category/" + cat_id.asString() + "/links";
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t getfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _4, _5);

	// getAndSuspend() does not use a body, so we can pass additional data.
	LLSD body;
	body["depth"] = 0;
	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, getfn, url,
						 cat_id, body, callback, FETCHCATEGORYLINKS));
	enqueueAISCommand("fetchCategoryLinks", proc);
}

//static
void AISAPI::fetchCOF(completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	url += "/category/current/links";
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t getfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _4, _5);

	// getAndSuspend() does not use a body, so we can pass additional data.
	LLSD body;
	body["depth"] = 0;
	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, getfn, url,
						 LLUUID::null, body, callback, FETCHCOF));
	enqueueAISCommand("fetchCOF", proc);
}

//static
void AISAPI::fetchOrphans(completion_t callback)
{
	std::string url;
	if (!getInvCap(url))
	{
		llwarns << "No cap found" << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	url += "/orphans";
	LL_DEBUGS("Inventory") << "url: " << url << LL_ENDL;

	invokationFn_t getfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
							 // _1 -> adapter
							 // _2 -> url
							 // _3 -> body
							 // _4 -> options
							 // _5 -> headers
							 _1, _2, _4, _5);

	LLCoprocedureManager::coprocedure_t
		proc(boost::bind(&AISAPI::invokeAISCommandCoro, _1, getfn, url,
						 LLUUID::null, LLSD(), callback, FETCHORPHANS));
	enqueueAISCommand("fetchOrphans", proc);
}

//static
void AISAPI::enqueueAISCommand(const std::string& proc_name,
							   LLCoprocedureManager::coprocedure_t proc)
{
	if (!sPostponedQuery.empty())
	{
		sPostponedQuery.emplace_back("AIS(" + proc_name + ")", proc);
		llinfos << "Queue not empty. Postponing: " << proc_name << llendl;
		return;
	}

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	LLUUID id = cpmgr->enqueueCoprocedure("AIS", "AIS(" + proc_name + ")",
										  proc);
	if (id.isNull())	// Failure to enqueue !
	{
		llinfos << "Will retry: " << proc_name << llendl;
		sPostponedQuery.emplace_back("AIS(" + proc_name + ")", proc);
		gIdleCallbacks.addFunction(onIdle, NULL);
	}
}

//static
void AISAPI::onIdle(void*)
{
	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	while (!sPostponedQuery.empty())
	{
		ais_query_item_t& item = sPostponedQuery.front();
		LLUUID id = cpmgr->enqueueCoprocedure("AIS", item.first, item.second);
		if (id.isNull())	// Failure to enqueue !
		{
			llinfos << "Will retry: " << item.first << llendl;
			break;
		}
		sPostponedQuery.pop_front();
	}
	if (sPostponedQuery.empty())
	{
		gIdleCallbacks.deleteFunction(onIdle, NULL);
	}
}

//static
void AISAPI::invokeAISCommandCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter,
								  invokationFn_t invoke, std::string url,
								  LLUUID target_id, LLSD body,
								  completion_t callback, U32 type)
{
	if (gDisconnected)
	{
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	LLCore::HttpHeaders::ptr_t headers;

	constexpr U32 AIS_TIMEOUT = 180;
	options->setTimeout(AIS_TIMEOUT);

	LL_DEBUGS("Inventory") << "Target: " << target_id << " - Command type: "
						   << (S32)type << " - URL: " << url << LL_ENDL;

	LLSD result = invoke(adapter, url, body, options, headers);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result.isMap())
	{
		if (!result.isMap())
		{
			status = gStatusInternalError;
		}
		llwarns << "Inventory error: " << status.toString() << " - Result:\n"
				<< ll_pretty_print_sd(result) << llendl;

		if (status.getType() == 410) // Gone
		{
			// Item does not exist or was already deleted from server; parent
			// folder is out of sync.
			if (type == REMOVECATEGORY)
			{
				LLViewerInventoryCategory* catp =
					gInventory.getCategory(target_id);
				if (catp)
				{
					llwarns << "Purge failed (folder no longer exists on server) for: "
							<< catp->getName()
							<< " - Local version: " << catp->getVersion()
							<< " - Descendents count: server="
							<< catp->getDescendentCount() << " - viewer="
							<< catp->getViewerDescendentCount() << llendl;
					gInventory.fetchDescendentsOf(catp->getParentUUID());
				}
			}
			else if (type == REMOVEITEM)
			{
				LLViewerInventoryItem* itemp = gInventory.getItem(target_id);
				if (itemp)
				{
					llwarns << "Purge failed (item no longer exists on server) for: "
							<< itemp->getName() << llendl;
					gInventory.onObjectDeletedFromServer(target_id);
				}
			}
		}
		else if (status == gStatusForbidden)	// 403
		{
			if (type == FETCHCATEGORYCHILDREN)
			{
				if (body.has("depth") && !body["depth"].asInteger())
				{
					// Cannot fetch a single folder with depth 0; folder is too
					// big.
					llwarns << "Fetch failed, content is over limit, url: "
							<< url << llendl;
					static U32 warned = 0;
					const char* notification =
						warned++ ? "AISInventoryLimitReached"
								 : "AISInventoryLimitReachedAlert";
					gNotifications.add(notification);
				}
			}
		}
	}

	// Parse update LLSD into stuff to do.
	AISUpdate ais_update(result, type, body);
	// Execute the updates in the appropriate order.
	ais_update.doUpdate();

	if (!callback || callback.empty())
	{
		return;
	}

	switch (type)
	{
		case COPYLIBRARYCATEGORY:
		case FETCHCATEGORYCHILDREN:
		case FETCHCATEGORYCATEGORIES:
		case FETCHCATEGORYSUBSET:
		case FETCHCOF:
		case FETCHCATEGORYLINKS:
		{
			LLUUID id;
			if (result.has("category_id"))
			{
				id = result["category_id"];
			}
			callback(id);
			break;
		}

		case FETCHITEM:
		{
			LLUUID id;
			if (result.has("item_id"))
			{
				// Error message might contain an item_id !
				id = result["item_id"];
			}
			if (result.has("linked_id"))
			{
				id = result["linked_id"];
			}
			callback(id);
			break;
		}
			
		case CREATEINVENTORY:
		{
			if (result.has("_created_categories"))
			{
				const LLSD& cats = result["_created_categories"];
				for (LLSD::array_const_iterator it = cats.beginArray(),
												end = cats.endArray();
					 it != end; ++it)
				{
					callback(it->asUUID());
				}
			}
			if (result.has("_created_items"))
			{
				const LLSD& items = result["_created_items"];
				for (LLSD::array_const_iterator it = items.beginArray(),
												end = items.endArray();
					 it != end; ++it)
				{
					callback(it->asUUID());
				}
			}
		}

		default:	// No callback needed.
			break;
	}
}

constexpr F32 CORO_YIELD_SECONDS = 1.f / 120.f;

AISUpdate::AISUpdate(const LLSD& update, U32 type, const LLSD& body)
:	mType(type),
	mFetch(type >= AISAPI::FETCHITEM),
	mFetchDepth(MAX_FOLDER_DEPTH_REQUEST)
{
	LL_DEBUGS("Inventory") << "Applying updates for command type: " << type
						   << LL_ENDL;
	if (mFetch && body.has("depth"))
	{
		mFetchDepth = body["depth"].asInteger();
	}
	mTimer.setTimerExpirySec(CORO_YIELD_SECONDS);
	parseUpdate(update);
}

void AISUpdate::checkTimeout()
{
	if (mTimer.hasExpired())
	{
		llcoro::suspend();
		mTimer.setTimerExpirySec(CORO_YIELD_SECONDS);
	}
}

void AISUpdate::clearParseResults()
{
	mCatDescendentDeltas.clear();
	mCatDescendentsKnown.clear();
	mCatVersionsUpdated.clear();
	mItemsCreated.clear();
	mItemsUpdated.clear();
	mItemsLost.clear();
	mCategoriesCreated.clear();
	mCategoriesUpdated.clear();
	mObjectsDeletedIds.clear();
	mItemIds.clear();
	mCategoryIds.clear();
}

void AISUpdate::parseUpdate(const LLSD& update)
{
	clearParseResults();
	parseMeta(update);
	parseContent(update);
}

void AISUpdate::parseMeta(const LLSD& update)
{
	LL_DEBUGS("Inventory") << "Meta data:\n" << ll_pretty_print_sd(update)
						   << LL_ENDL;
	// Parse _categories_removed -> mObjectsDeletedIds
	uuid_list_t cat_ids;
	parseUUIDArray(update, "_categories_removed", cat_ids);
	for (uuid_list_t::const_iterator it = cat_ids.begin();
		 it != cat_ids.end(); ++it)
	{
		LLViewerInventoryCategory* catp = gInventory.getCategory(*it);
		if (catp)
		{
			--mCatDescendentDeltas[catp->getParentUUID()];
			mObjectsDeletedIds.emplace(*it);
		}
		else
		{
			llwarns << "Removed category " << *it << " not found." << llendl;
		}
	}

	// Parse _categories_items_removed -> mObjectsDeletedIds
	uuid_list_t item_ids;
	parseUUIDArray(update, "_category_items_removed", item_ids);
	parseUUIDArray(update, "_removed_items", item_ids);
	for (uuid_list_t::const_iterator it = item_ids.begin();
		 it != item_ids.end(); ++it)
	{
		LLViewerInventoryItem* itemp = gInventory.getItem(*it);
		if (itemp)
		{
			--mCatDescendentDeltas[itemp->getParentUUID()];
			mObjectsDeletedIds.emplace(*it);
		}
		else
		{
			llwarns << "Removed item " << *it << " not found." << llendl;
		}
	}

	// Parse _broken_links_removed -> mObjectsDeletedIds
	uuid_list_t broken_link_ids;
	parseUUIDArray(update, "_broken_links_removed", broken_link_ids);
	for (uuid_list_t::const_iterator it = broken_link_ids.begin();
		 it != broken_link_ids.end(); ++it)
	{
		LLViewerInventoryItem* itemp = gInventory.getItem(*it);
		if (itemp)
		{
			--mCatDescendentDeltas[itemp->getParentUUID()];
			mObjectsDeletedIds.emplace(*it);
		}
		else
		{
			llwarns << "Removed broken link " << *it << " not found."
					<< llendl;
		}
	}

	// Parse _created_items
	parseUUIDArray(update, "_created_items", mItemIds);

	// Parse _created_categories
	parseUUIDArray(update, "_created_categories", mCategoryIds);

	// Parse updated category versions.
	const std::string& ucv = "_updated_category_versions";
	if (update.has(ucv))
	{
		LLUUID cat_id;
		for (LLSD::map_const_iterator it = update[ucv].beginMap(),
				end = update[ucv].endMap();
			it != end; ++it)
		{
			cat_id.set(it->first, false);
			mCatVersionsUpdated.emplace(cat_id, it->second.asInteger());
		}
	}
}

void AISUpdate::parseContent(const LLSD& update)
{
	LL_DEBUGS("Inventory") << "Update data:\n" << ll_pretty_print_sd(update)
						   << LL_ENDL;
	// Errors from a fetch request might contain an item id without full item
	// or folder. *TODO: depending on error we might want to do something, like
	// removing the item on 404, or refetching the parent folder.
	if (update.has("parent_id"))
	{
		if (update.has("linked_id"))
		{
			parseLink(update, mFetchDepth);
		}
		else if (update.has("item_id"))
		{
			parseItem(update);
		}
	}

	if (mType == AISAPI::FETCHCATEGORYSUBSET)
	{
		// Initial category is incomplete, do not process it and go for
		// contents instead.
		if (update.has("_embedded"))
		{
			parseEmbedded(update["_embedded"], mFetchDepth - 1);
		}	
	}
	else if (update.has("category_id") && update.has("parent_id"))
	{
		parseCategory(update, mFetchDepth);
	}
	else if (update.has("_embedded"))
	{
		parseEmbedded(update["_embedded"], mFetchDepth);
	}
}

void AISUpdate::parseItem(const LLSD& item_map)
{
	LL_DEBUGS("Inventory") << "Item map:\n" << ll_pretty_print_sd(item_map)
						   << LL_ENDL;
	LLUUID item_id = item_map["item_id"].asUUID();
	LLPointer<LLViewerInventoryItem> new_itemp(new LLViewerInventoryItem);
	LLViewerInventoryItem* cur_itemp = gInventory.getItem(item_id);
	if (cur_itemp && new_itemp)
	{
		// Default to current values where not provided.
		new_itemp->copyViewerItem(cur_itemp);
	}

	if (!new_itemp || !new_itemp->unpackMessage(item_map))
	{
		llwarns << "Invalid data, cannot parse: " << item_map << llendl;
		gNotifications.add("AISFailure");
		return;
	}

	if (mFetch)
	{
		new_itemp->setComplete(true);
		if (new_itemp->getParentUUID().isNull())
		{
			mItemsLost.emplace(item_id, new_itemp);
		}
		// Do not use new_itemp after this ! HB
		mItemsCreated.emplace(item_id, std::move(new_itemp));
	}
	else if (cur_itemp)
	{
		// This statement is here to cause a new entry with 0 delta to be
		// created if it does not already exist; otherwise has no effect.
		mCatDescendentDeltas[new_itemp->getParentUUID()];
		// Do not use new_itemp after this ! HB
		mItemsUpdated.emplace(item_id, std::move(new_itemp));
	}
	else
	{
		new_itemp->setComplete(true);
		++mCatDescendentDeltas[new_itemp->getParentUUID()];
		// Do not use new_itemp after this ! HB
		mItemsCreated.emplace(item_id, std::move(new_itemp));
	}
}

void AISUpdate::parseLink(const LLSD& link_map, U32 depth)
{
	LL_DEBUGS("Inventory") << "Link map:\n" << ll_pretty_print_sd(link_map)
						   << LL_ENDL;
	LLUUID item_id = link_map["item_id"].asUUID();
	LLPointer<LLViewerInventoryItem> new_linkp(new LLViewerInventoryItem);
	LLViewerInventoryItem* cur_linkp = gInventory.getItem(item_id);
	if (cur_linkp && new_linkp)
	{
		// Default to current values where not provided.
		new_linkp->copyViewerItem(cur_linkp);
	}

	if (!new_linkp || !new_linkp->unpackMessage(link_map))
	{
		llwarns << "Invalid data, cannot parse: " << link_map << llendl;
		gNotifications.add("AISFailure");
		return;
	}

	const LLUUID& parent_id = new_linkp->getParentUUID();
	if (mFetch)
	{
		LLPermissions perms;
		perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
		perms.initMasks(PERM_NONE, PERM_NONE, PERM_NONE, PERM_NONE, PERM_NONE);
		new_linkp->setPermissions(perms);
		LLSaleInfo default_sale_info;
		new_linkp->setSaleInfo(default_sale_info);
		new_linkp->setComplete(true);
		if (new_linkp->getParentUUID().isNull())
		{
			mItemsLost.emplace(item_id, new_linkp);
		}
		// Do not use new_linkp after this ! HB
		mItemsCreated.emplace(item_id, std::move(new_linkp));
	}
	else if (cur_linkp)
	{
		// This statement is here to cause a new entry with 0 delta to be
		// created if it does not already exist; otherwise has no effect.
		mCatDescendentDeltas[parent_id];
		// Do not use new_linkp after this ! HB
		mItemsUpdated.emplace(item_id, std::move(new_linkp));
	}
	else
	{
		++mCatDescendentDeltas[parent_id];
		LLPermissions perms;
		perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
		perms.initMasks(PERM_NONE, PERM_NONE, PERM_NONE, PERM_NONE, PERM_NONE);
		new_linkp->setPermissions(perms);
		LLSaleInfo default_sale_info;
		new_linkp->setSaleInfo(default_sale_info);
		new_linkp->setComplete(true);
		// Do not use new_linkp after this ! HB
		mItemsCreated.emplace(item_id, std::move(new_linkp));
	}

	if (link_map.has("_embedded"))
	{
		parseEmbedded(link_map["_embedded"], depth);
	}
}

void AISUpdate::parseCategory(const LLSD& category_map, U32 depth)
{
	LLUUID cat_id = category_map["category_id"].asUUID();

	S32 version = LLViewerInventoryCategory::VERSION_UNKNOWN;
	if (category_map.has("version"))
	{
		version = category_map["version"].asInteger();
	}

	LLViewerInventoryCategory* catp = gInventory.getCategory(cat_id);
	if (catp && version > LLViewerInventoryCategory::VERSION_UNKNOWN &&
		catp->getVersion() > version && !catp->isDescendentCountUnknown())
	{
		llwarns << "Got stale folder data for " << cat_id
				<< ". Current version is " << catp->getVersion()
				<< " and received data version was " << version
				<< ". Ignoring." << llendl;
		return;
	}

	LLPointer<LLViewerInventoryCategory> new_catp;
	if (catp)
	{
		// Default to current values where not provided.
		new_catp = new LLViewerInventoryCategory(catp);
	}
	else if (category_map.has("agent_id"))
	{
		new_catp =
			new LLViewerInventoryCategory(category_map["agent_id"].asUUID());
	}
	else
	{
		new_catp = new LLViewerInventoryCategory(LLUUID::null);
		LL_DEBUGS("Inventory") << "No owner provided, folder "
							   << new_catp->getUUID()
							   << " might be assigned wrong owner" << LL_ENDL;
	}

	// Note: unpackMessage() does not unpack version or descendent count.
	if (!new_catp || !new_catp->unpackMessage(category_map))
	{
		gNotifications.add("AISFailure");
		return;
	}

	// Check descendent count first, as it may be needed to populate newly
	// created categories
	if (category_map.has("_embedded"))
	{
		LLFolderType::EType type = new_catp->getPreferredType();
		bool links_only = type == LLFolderType::FT_CURRENT_OUTFIT ||
						  type == LLFolderType::FT_OUTFIT;
		parseDescendentCount(cat_id, links_only, category_map["_embedded"]);
	}

	if (mFetch)
	{
		uuid_int_map_t::const_iterator it = mCatDescendentsKnown.find(cat_id);
		if (it != mCatDescendentsKnown.end())
		{
			S32 descendent_count = it->second;
			LL_DEBUGS("Inventory") << "Setting descendents count to "
								   << descendent_count << " for category "
								   << cat_id << LL_ENDL;
			new_catp->setDescendentCount(descendent_count);

			// Set the version only if we are sure this update has full data
			// and embeded items since the viewer uses version to decide if
			// folder and contents still need fetching.
			if (depth >= 0 &&
				version > LLViewerInventoryCategory::VERSION_UNKNOWN)
			{
				if (catp && catp->getVersion() > version)
				{
					llwarns << "Version for category " << cat_id << " was "
							<< catp->getVersion()
							<< ", but fetch returned version " << version
							<< llendl;
				}
				LL_DEBUGS("Inventory") << "Setting version to " << version
									   << " for category " << cat_id
									   << LL_ENDL;
				new_catp->setVersion(version);
			}
		}
		// Do not use new_catp after this ! HB
		mCategoriesCreated.emplace(cat_id, std::move(new_catp));
	}
	else if (catp)
	{
		// This statement is here to cause a new entry with 0 delta to be
		// created if it does not already exist; otherwise has no effect.
		mCatDescendentDeltas[new_catp->getParentUUID()];
		// Capture update for the category itself as well.
		mCatDescendentDeltas[cat_id];
		// Do not use new_catp after this ! HB
		mCategoriesUpdated.emplace(cat_id, std::move(new_catp));
	}
	else
	{
		uuid_int_map_t::const_iterator it = mCatDescendentsKnown.find(cat_id);
		if (it != mCatDescendentsKnown.end())
		{
			S32 descendent_count = it->second;
			LL_DEBUGS("Inventory") << "Setting descendents count to "
								   << descendent_count << " for new category "
								   << cat_id << LL_ENDL;
			new_catp->setDescendentCount(descendent_count);
			// Since we got a proper children count, we can set the version.
			if (version > LLViewerInventoryCategory::VERSION_UNKNOWN)
			{
				LL_DEBUGS("Inventory") << "Setting version to " << version
									   << " for category " << cat_id
									   << LL_ENDL;
				new_catp->setVersion(version);
			}
		}
		++mCatDescendentDeltas[new_catp->getParentUUID()];
		// Do not use new_catp after this ! HB
		mCategoriesCreated.emplace(cat_id, std::move(new_catp));
	}

	// Check for more embedded content.
	if (category_map.has("_embedded"))
	{
		parseEmbedded(category_map["_embedded"], depth - 1);
	}
}

void AISUpdate::parseDescendentCount(const LLUUID& cat_id, bool links_only,
									 const LLSD& embedded)
{
	// We can only determine true descendent count if this contains all
	// descendent types.
	if (embedded.has("categories") && embedded.has("links") &&
		embedded.has("items"))
	{
		S32 count = embedded["categories"].size() + embedded["links"].size() +
					embedded["items"].size();
		mCatDescendentsKnown.emplace(cat_id, count);
	}
	// For folders that *should* only contain links, such as the COF, we only
	// need to ensure links are present.
	else if (links_only && mFetch && embedded.has("links"))
	{
		mCatDescendentsKnown.emplace(cat_id, embedded["links"].size());
	}
}

void AISUpdate::parseEmbedded(const LLSD& embedded, U32 depth)
{
	checkTimeout();

#if 0
	if (embedded.has("link"))
	{
		parseEmbeddedLinks(embedded["link"], depth);
	}
#endif
	if (embedded.has("links"))			// _embedded in a category
	{
		parseEmbeddedLinks(embedded["links"], depth);
	}
	if (embedded.has("items"))			// _embedded in a category
	{
		parseEmbeddedItems(embedded["items"]);
	}
	if (embedded.has("item"))			// _embedded in a link
	{
		parseEmbeddedItem(embedded["item"]);
	}
	if (embedded.has("categories"))		// _embedded in a category
	{
		parseEmbeddedCategories(embedded["categories"], depth);
	}
	if (embedded.has("category"))		// _embedded in a link
	{
		parseEmbeddedCategory(embedded["category"], depth);
	}
}

void AISUpdate::parseUUIDArray(const LLSD& content, const std::string& name,
							   uuid_list_t& ids)
{
	if (content.has(name))
	{
		for (LLSD::array_const_iterator it = content[name].beginArray(),
										end = content[name].endArray();
			 it != end; ++it)
		{
			ids.emplace(it->asUUID());
		}
	}
}

void AISUpdate::parseEmbeddedLinks(const LLSD& links, U32 depth)
{
	for (LLSD::map_const_iterator it = links.beginMap(), end = links.endMap();
		 it != end; ++it)
	{
		const LLUUID id(it->first);
		if (mFetch || mItemIds.count(id))
		{
			parseLink(it->second, depth);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Ignoring link not in items list: " << id
								   << LL_ENDL;
		}
	}
}

void AISUpdate::parseEmbeddedItem(const LLSD& item)
{
	// A single item (_embedded in a link)
	if (item.has("item_id") &&
		(mFetch || mItemIds.count(item["item_id"].asUUID())))
	{
		parseItem(item);
	}
}

void AISUpdate::parseEmbeddedItems(const LLSD& items)
{
	// A map of items (_embedded in a category)
	for (LLSD::map_const_iterator it = items.beginMap(), end = items.endMap();
		 it != end; ++it)
	{
		const LLUUID id(it->first);
		if (mFetch || mItemIds.count(id))
		{
			parseItem(it->second);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Ignoring item not in items list: " << id
								   << LL_ENDL;
		}
	}
}

void AISUpdate::parseEmbeddedCategory(const LLSD& category, U32 depth)
{
	// A single category (_embedded in a link)
	if (category.has("category_id") &&
		(mFetch || mCategoryIds.count(category["category_id"].asUUID())))
	{
		parseCategory(category, depth);
	}
}

void AISUpdate::parseEmbeddedCategories(const LLSD& categories, U32 depth)
{
	// A map of categories (_embedded in a category)
	for (LLSD::map_const_iterator it = categories.beginMap(),
								  end = categories.endMap();
		 it != end; ++it)
	{
		const LLUUID id(it->first);
		if (mFetch || mCategoryIds.count(id))
		{
			parseCategory(it->second, depth);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Ignoring category not in categories list: "
								   << id << LL_ENDL;
		}
	}
}

void AISUpdate::doUpdate()
{
	checkTimeout();

	// Do version/descendent accounting.
	for (uuid_int_map_t::const_iterator it = mCatDescendentDeltas.begin(),
										end = mCatDescendentDeltas.end();
		 it != end; ++it)
	{
		const LLUUID& cat_id = it->first;
		LLViewerInventoryCategory* catp = gInventory.getCategory(cat_id);
		LL_DEBUGS("Inventory") << "Descendent accounting for category "
							   << (catp ? catp->getName() : "NOT FOUND")
							   << " (" << cat_id << ")" << LL_ENDL;

		// Do not account for update if we just created this category
		if (mCategoriesCreated.count(cat_id))
		{
			LL_DEBUGS("Inventory") << "Skipping version increment for new category "
								   << (catp ? catp->getName() : "NOT FOUND")
								   << " (" << cat_id << ")" << LL_ENDL;
			continue;
		}

		// Do not account for update unless AIS told us it updated that
		// category
		if (!mCatVersionsUpdated.count(cat_id))
		{
			LL_DEBUGS("Inventory") << "Skipping version increment for non-updated category "
								   << (catp ? catp->getName() : "NOT FOUND")
								   << " (" << cat_id << ")" << LL_ENDL;
			continue;
		}

		// If we have a known descendent count, set that now.
		if (catp)
		{
			S32 descendent_delta = it->second;
			LL_DEBUGS("Inventory") << "Updating descendent count for "
								   << catp->getName() << " (" << cat_id
								   << ") with delta " << descendent_delta;
			S32 old_count = catp->getDescendentCount();
			LL_CONT << " from " << old_count << " to "
					<< old_count + descendent_delta << LL_ENDL;
			LLInventoryModel::LLCategoryUpdate up(cat_id, descendent_delta);
			gInventory.accountForUpdate(up);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Skipping version accounting for unknown category "
								   << cat_id << LL_ENDL;
		}
	}

	// CREATE CATEGORIES
	for (deferred_category_map_t::const_iterator
			it = mCategoriesCreated.begin(), end = mCategoriesCreated.end();
		  it != end; ++it)
	{
		LL_DEBUGS("Inventory") << "Creating category " << it->first << LL_ENDL;
		LLPointer<LLViewerInventoryCategory> new_catp = it->second;
		gInventory.updateCategory(new_catp, LLInventoryObserver::CREATE);

		// Fetching can receive massive amount of items and folders
		if (gInventory.getChangedIDs().size() > MAX_FOLDER_DEPTH_REQUEST)
		{
			gInventory.notifyObservers();
			checkTimeout();
		}
	}

	// UPDATE CATEGORIES
	for (deferred_category_map_t::const_iterator
			it = mCategoriesUpdated.begin(), end = mCategoriesUpdated.end();
		 it != end; ++it)
	{
		const LLUUID& cat_id = it->first;
		LLPointer<LLViewerInventoryCategory> new_catp = it->second;
		// Since this is a copy of the category *before* the accounting update,
		// above, we need to transfer back the updated version/descendent
		// count.
		LLViewerInventoryCategory* cur_catp =
			gInventory.getCategory(new_catp->getUUID());
		if (cur_catp)
		{
			LL_DEBUGS("Inventory") << "Updating category: "
								   << new_catp->getName() << " - Id: "
								   << cat_id << LL_ENDL;
			new_catp->setVersion(cur_catp->getVersion());
			new_catp->setDescendentCount(cur_catp->getDescendentCount());
			gInventory.updateCategory(new_catp);
		}
		else
		{
			llwarns << "Failed to update unknown category "
					<< new_catp->getUUID() << llendl;
		}
	}

	// LOST ITEMS
	if (!mItemsLost.empty())
	{
		const LLUUID& laf = gInventory.getLostAndFoundID();
		for (deferred_item_map_t::const_iterator it = mItemsLost.begin(),
												 end = mItemsLost.end();
			 it != end; ++it)
		{
			LL_DEBUGS("Inventory") << "Lost item " << it->first << LL_ENDL;
			LLPointer<LLViewerInventoryItem> new_itemp = it->second;
			new_itemp->setParent(laf);
			new_itemp->updateParentOnServer(false);
		}
	}

	// CREATE ITEMS
	for (deferred_item_map_t::const_iterator it = mItemsCreated.begin(),
											 end = mItemsCreated.end();
		 it != end; ++it)
	{
		LL_DEBUGS("Inventory") << "Creating item " << it->first << LL_ENDL;
		LLPointer<LLViewerInventoryItem> new_itemp = it->second;
		// *FIXME: risky function since it calls updateServer() in some cases.
		// Maybe break out the update/create cases, in which case this is
		// create.
		gInventory.updateItem(new_itemp, LLInventoryObserver::CREATE);

		// Fetching can receive massive amount of items and folders
		if (gInventory.getChangedIDs().size() > MAX_FOLDER_DEPTH_REQUEST)
		{
			gInventory.notifyObservers();
			checkTimeout();
		}
	}

	// UPDATE ITEMS
	for (deferred_item_map_t::const_iterator it = mItemsUpdated.begin(),
											 end = mItemsUpdated.end();
		 it != end; ++it)
	{
		LL_DEBUGS("Inventory") << "Updating item " << it->first << LL_ENDL;
		LLPointer<LLViewerInventoryItem> new_itemp = it->second;
		// *FIXME: risky function since it calls updateServer() in some cases.
		// Maybe break out the update/create cases, in which case this is
		// update.
		gInventory.updateItem(new_itemp);
	}

	// DELETE OBJECTS
	for (uuid_list_t::const_iterator it = mObjectsDeletedIds.begin(),
									 end = mObjectsDeletedIds.end();
		 it != end; ++it)
	{
		const LLUUID& item_id = *it;
		LL_DEBUGS("Inventory") << "Deleting item " << item_id << LL_ENDL;
		gInventory.onObjectDeletedFromServer(item_id, false, false, false);
	}

	// *TODO: how can we use this version info ?  Need to be sure all changes
	// are going through AIS first, or at least through something with a
	// reliable responder. Notes by HB: this is mostly irrelevant: the AIS
	// updates can be mixed up with legacy UDP inventory updates, the latter
	// also causing version increments (UPDATE: as of 28/05/20016 there
	// *should* not be mixed-up AIS/UDP operations any more now: all inventory
	// ops should now have been enabled with AIS). Beside, several requests
	// launched in a raw can see their replies arriving in a different order
	// (because TCP/IP networking does not guarantee that bunches of packets
	// sent in sequence will arrive in the same order) and a race condition
	// ensues, falsely producing category versions mismatches (UPDATE: as of
	// 28/05/20016 this is still a problem).
	// It may however help tracking down bad version accounting in code and
	// was therefore kept as a debug feature (UPDATE as of 24/05/2017 this is
	// also used for the added 'catp->fetch()', backported from viewer-neko,
	// and therefore no more just a debug feature).
	LL_DEBUGS("Inventory") << "Checking updated category versions...";
	for (uuid_int_map_t::iterator it = mCatVersionsUpdated.begin(),
								  end = mCatVersionsUpdated.end();
		 it != end; ++it)
	{
		S32 version = it->second;
		LLViewerInventoryCategory* catp = gInventory.getCategory(it->first);
		if (catp && catp->getVersion() != version)
		{
			LL_CONT << "\nPossible version mismatch for category: "
					<< catp->getName()
					<< " - Viewer-side version: "
					<< catp->getVersion()
					<< " - Server-side version: "
					<< version;
			if (version == LLViewerInventoryCategory::VERSION_UNKNOWN)
			{
				catp->fetch();
			}
#if 1		// 02/10/2023: AIS has been revamped, with more operations moved to
			// it (meaning less potential occurrences for mixed UDP and HTTP
			// operations arriving/occurring out of order and messing up the
			// folders version and descendents count), serious version checks
			// and stale data detection. Let's now allow to set the folder
			// version according to AIS' idea of what it should be... HB
			else
			{
				catp->setVersion(version);
			}
#endif
		}
	}
	LL_CONT << "\nChecks done." << LL_ENDL;

	gInventory.notifyObservers();

	checkTimeout();
}
