/**
 * @file llinventorymodelfetch.cpp
 * @brief Implementation of the inventory fetcher.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "llinventorymodelfetch.h"

#include "indra_constants.h"		// For ALEXANDRIA_LINDEN_ID
#include "llcallbacklist.h"
#include "llcorehttputil.h"
#include "llsdutil.h"				// For ll_pretty_print_sd()
#include "llnotifications.h"
#include "llsdserialize.h"

#include "llagent.h"
#include "llaisapi.h"
#include "llappviewer.h"
#include "llinventorymodel.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermessage.h"

// IMPORTANT NOTE: do *NOT* add calls to gInventory.notifyObservers() into
// *ANY* of the methods of this module: these would cause recursive calls to
// gInventory.notifyObservers() in observers callbacks, and result in failed
// inventory items status updates (such as worn items listed as not worn). Such
// calls are *USELESS* anyway, since gInventory.idleNotifyObservers() is called
// at *each frame* from llappviewer.cpp, after the idle callbacks invocation,
// and it itself calls gInventory.notifyObservers() at a point where recursion
// does not risk to happen. HB

//----------------------------------------------------------------------------
// Helper class BGItemHttpHandler
//----------------------------------------------------------------------------

// HTTP request handler class for single inventory item requests.
//
// We will use a handler-per-request pattern here rather than a shared handler.
// Mainly convenient as this was converted from a Responder class model.
//
// Derives from and is identical to the normal FetchItemHttpHandler except
// that: 1) it uses the background request object which is updated more slowly
// than the foreground and: 2) keeps a count of active requests on the
// LLInventoryModelFetch object to indicate outstanding operations are
// in-flight.

class BGItemHttpHandler : public LLInventoryModel::FetchItemHttpHandler
{
	LOG_CLASS(BGItemHttpHandler);

public:
	BGItemHttpHandler(const LLSD& request_sd)
	:	LLInventoryModel::FetchItemHttpHandler(request_sd)
	{
		LLInventoryModelFetch::getInstance()->incrFetchCount(1);
	}

	~BGItemHttpHandler() override
	{
		LLInventoryModelFetch::getInstance()->incrFetchCount(-1);
	}

	BGItemHttpHandler(const BGItemHttpHandler&) = delete;
	void operator=(const BGItemHttpHandler&) = delete;

	static void postRequest(const std::string& url, const LLSD& request_sd,
							bool is_library = false);
};

//static
void BGItemHttpHandler::postRequest(const std::string& url,
									const LLSD& request_sd, bool is_library)
{
	static const char* lib_item_str = "library item";
	static const char* inv_item_str = "inventory item";
	LLCore::HttpHandler::ptr_t handler(new BGItemHttpHandler(request_sd));
	gInventory.requestPost(false, url, request_sd, handler,
						   is_library ? lib_item_str : inv_item_str);
}

//----------------------------------------------------------------------------
// Helper class BGFolderHttpHandler
//----------------------------------------------------------------------------

// HTTP request handler class for folders.
//
// Handler for FetchInventoryDescendents2 and FetchLibDescendents2 caps
// requests for folders.

class BGFolderHttpHandler : public LLCore::HttpHandler
{
	LOG_CLASS(BGFolderHttpHandler);

public:
	BGFolderHttpHandler(const LLSD& request_sd,
						const uuid_vec_t& recursive_cats)
	:	LLCore::HttpHandler(),
		mRequestSD(request_sd),
		mRecursiveCatUUIDs(recursive_cats)
	{
		LLInventoryModelFetch::getInstance()->incrFetchCount(1);
	}

	~BGFolderHttpHandler() override
	{
		LLInventoryModelFetch::getInstance()->incrFetchCount(-1);
	}

	BGFolderHttpHandler(const BGFolderHttpHandler&) = delete;
	void operator=(const BGFolderHttpHandler&) = delete;

	void onCompleted(LLCore::HttpHandle handle,
					 LLCore::HttpResponse* responsep) override;

	bool getIsRecursive(const LLUUID& cat_id) const;

	static void postRequest(const std::string& url, const LLSD& request_sd,
							const uuid_vec_t& recursive_cats,
							bool is_library = false);

private:
	void processFailure(LLCore::HttpStatus status,
						LLCore::HttpResponse* responsep);
	void processFailure(const char* const reason,
						LLCore::HttpResponse* responsep);

private:
	LLSD				mRequestSD;
	// *HACK for storing away which cat fetches are recursive:
	const uuid_vec_t	mRecursiveCatUUIDs;
};

//static
void BGFolderHttpHandler::postRequest(const std::string& url,
									  const LLSD& request_sd,
									  const uuid_vec_t& recursive_cats,
									  bool is_library)
{
	static const char* lib_folder_str = "library folder";
	static const char* inv_folder_str = "inventory folder";
	LLCore::HttpHandler::ptr_t handler(new BGFolderHttpHandler(request_sd,
															   recursive_cats));
	gInventory.requestPost(false, url, request_sd, handler,
						   is_library ? lib_folder_str : inv_folder_str);
}

void BGFolderHttpHandler::onCompleted(LLCore::HttpHandle handle,
									  LLCore::HttpResponse* responsep)
{
	LLCore::HttpStatus status = responsep->getStatus();
	if (!status)
	{
		processFailure(status, responsep);
		return;
	}

	// Response body should be present.
	LLCore::BufferArray* bodyp = responsep->getBody();
	if (!bodyp || ! bodyp->size())
	{
		llwarns << "Missing data in inventory folder query." << llendl;
		processFailure("HTTP response missing expected body", responsep);
		return;
	}

	// Could test 'Content-Type' header but probably unreliable.

	// Convert response to LLSD
	LLSD body_llsd;
	if (!LLCoreHttpUtil::responseToLLSD(responsep, true, body_llsd))
	{
		// INFOS-level logging will occur on the parsed failure
		processFailure("HTTP response contained malformed LLSD", responsep);
		return;
	}

	// Expect top-level structure to be a map
	if (!body_llsd.isMap())
	{
		processFailure("LLSD response not a map", responsep);
		return;
	}

	// Check for 200-with-error failures. See comments in llinventorymodel.cpp
	// about this mode of error.
	if (body_llsd.has("error"))
	{
		processFailure("Inventory application error (200-with-error)",
					   responsep);
		return;
	}

	// Okay, process data if possible

	LLInventoryModelFetch* fetcherp = LLInventoryModelFetch::getInstance();

	const LLUUID& laf_id = gInventory.getLostAndFoundID();

	// API V2 and earlier should probably be testing for "error" map in
	// response as an application-level error. Instead, we assume success and
	// attempt to extract information.
	if (body_llsd.has("folders"))
	{
		const LLSD& folders = body_llsd["folders"];
		for (LLSD::array_const_iterator folder_it = folders.beginArray(),
										folder_end = folders.endArray();
			folder_it != folder_end; ++folder_it)
		{
			const LLSD& folder_sd = *folder_it;

			LLUUID parent_id = folder_sd["folder_id"];
			LLUUID owner_id = folder_sd["owner_id"];
			S32 version = folder_sd["version"].asInteger();
			S32 descendents = folder_sd["descendents"].asInteger();

			if (parent_id.isNull() && laf_id.notNull() &&
				folder_sd.has("items"))
			{
				const LLSD& items = folder_sd["items"];
				LLPointer<LLViewerInventoryItem> itemp =
					new LLViewerInventoryItem;

				for (LLSD::array_const_iterator item_it = items.beginArray(),
												item_end = items.endArray();
					 item_it != item_end; ++item_it)
				{
					const LLSD& item_llsd = *item_it;
					itemp->unpackMessage(item_llsd);

					LLInventoryModel::update_list_t update;
					update.emplace_back(laf_id, 1);
					gInventory.accountForUpdate(update);

					itemp->setParent(laf_id);
					itemp->updateParentOnServer(false);
					gInventory.updateItem(itemp);
				}
			}

			if (!gInventory.getCategory(parent_id))
			{
				continue;
			}

			if (folder_sd.has("categories"))
			{
				LLPointer<LLViewerInventoryCategory> catp =
					new LLViewerInventoryCategory(owner_id);

				const LLSD& categories = folder_sd["categories"];
				for (LLSD::array_const_iterator it = categories.beginArray(),
												end = categories.endArray();
					 it != end; ++it)
				{
					LLSD category = *it;
					catp->fromLLSD(category);

					if (getIsRecursive(catp->getUUID()))
					{
						fetcherp->addRequestAtBack(catp->getUUID(), true,
												   true);
					}
					else if (!gInventory.isCategoryComplete(catp->getUUID()))
					{
						gInventory.updateCategory(catp);
					}
				}
			}

			if (folder_sd.has("items"))
			{
				const LLSD& items = folder_sd["items"];
				LLPointer<LLViewerInventoryItem> itemp =
					new LLViewerInventoryItem;
				for (LLSD::array_const_iterator it = items.beginArray(),
												end = items.endArray();
					 it != end; ++it)
				{
					const LLSD item_llsd = *it;
					itemp->unpackMessage(item_llsd);
					gInventory.updateItem(itemp);
				}

				// Set version and descendentcount according to message.
				LLViewerInventoryCategory* catp =
					gInventory.getCategory(parent_id);
				if (catp)
				{
					catp->setVersion(version);
					catp->setDescendentCount(descendents);
				}
			}
		}
	}

	if (body_llsd.has("bad_folders"))
	{
		const LLSD& bad_folders = body_llsd["bad_folders"];
		LL_DEBUGS("InventoryFetch") << "Bad folders LLSD:\n"
									<< ll_pretty_print_sd(bad_folders)
									<< LL_ENDL;

		for (LLSD::array_const_iterator it = bad_folders.beginArray(),
										end = bad_folders.endArray();
			it != end; ++it)
		{
			const LLSD& folder_sd = *it;
			// These folders failed on the dataserver. We probably do not want
			// to retry them.
			if (folder_sd.has("folder_id"))
			{
				llwarns << "Folder: " << folder_sd["folder_id"].asString()
						<< " - Error: " << folder_sd["error"].asString()
						<< llendl;
			}
		}
	}

	if (fetcherp->isBulkFetchProcessingComplete())
	{
		fetcherp->setAllFoldersFetched();
	}
}

void BGFolderHttpHandler::processFailure(LLCore::HttpStatus status,
										 LLCore::HttpResponse* responsep)
{
	if (gDisconnected || LLApp::isExiting())
	{
		return;
	}

	const std::string& ct = responsep->getContentType();
	llwarns << "Inventory folder fetch failure - Status: "
			<< status.toTerseString() << " - Reason: " << status.toString()
			<< " - Content-type: " << ct << " - Content (abridged): "
			<< LLCoreHttpUtil::responseToString(responsep) << llendl;

	// Could use a 404 test here to try to detect revoked caps...

	if (status != gStatusForbidden)	// 403
	{
		LLInventoryModelFetch* fetcherp = LLInventoryModelFetch::getInstance();
		if (fetcherp->isBulkFetchProcessingComplete())
		{
			fetcherp->setAllFoldersFetched();
		}
		return;
	}

	// 403 error processing.

	const std::string& url =
		gAgent.getRegionCapability("FetchInventoryDescendents2");
	if (url.empty())
	{
		llwarns << "Fetch failed. No FetchInventoryDescendents2 capability."
				<< llendl;
		return;
	}

	size_t size = mRequestSD["folders"].size();
	if (!size)
	{
		static U32 warned = 0;
		const char* notification = warned++ ? "AISInventoryLimitReached"
											: "AISInventoryLimitReachedAlert";
		gNotifications.add(notification);
	}

	// We can split. Also assume that this is not the library
	LLSD folders;
	uuid_vec_t recursive_cats;
	LLUUID folder_id;
	for (LLSD::array_iterator it = mRequestSD["folders"].beginArray(),
							  end = mRequestSD["folders"].endArray();
		 it != end; )
	{
		folders.append(*it++);
		folder_id = it->get("folder_id").asUUID();
		if (getIsRecursive(folder_id))
		{
			recursive_cats.emplace_back(folder_id);
		}
		if (it == end || folders.size() == size / 2)
		{
			LLSD request_body;
			request_body["folders"] = folders;
			postRequest(url, request_body, recursive_cats);
			recursive_cats.clear();
			folders.clear();
		}
	}
}

void BGFolderHttpHandler::processFailure(const char* const reason,
										 LLCore::HttpResponse* responsep)
{
	llwarns << "Inventory folder fetch failure - Status: internal error - Reason: "
			<< reason << " - Content (abridged): "
			<< LLCoreHttpUtil::responseToString(responsep) << llendl;

	LLInventoryModelFetch* fetcherp = LLInventoryModelFetch::getInstance();

	// Reverse of previous processFailure() method, this is invoked when
	// response structure is found to be invalid. Original always re-issued the
	// request (without limit). This does the same but be aware that this may
	// be a source of problems. Philosophy is that inventory folders are so
	// essential to operation that this is a reasonable action.
	for (LLSD::array_const_iterator it = mRequestSD["folders"].beginArray(),
									end = mRequestSD["folders"].endArray();
		it != end; ++it)
	{
		LLSD folder_sd = *it;
		LLUUID cat_id = folder_sd["folder_id"].asUUID();
		fetcherp->addRequestAtFront(cat_id, getIsRecursive(cat_id), true);
	}
}

bool BGFolderHttpHandler::getIsRecursive(const LLUUID& cat_id) const
{
	return std::find(mRecursiveCatUUIDs.begin(), mRecursiveCatUUIDs.end(),
					 cat_id) != mRecursiveCatUUIDs.end();
}

///////////////////////////////////////////////////////////////////////////////
// LLInventoryModelFetch class proper
///////////////////////////////////////////////////////////////////////////////

//static
bool LLInventoryModelFetch::sUseAISFetching = true;

//static
bool LLInventoryModelFetch::useAISFetching()
{
	return sUseAISFetching && AISAPI::isAvailable();
}

LLInventoryModelFetch::LLInventoryModelFetch()
:	mBackgroundFetchActive(false),
	mFolderFetchActive(false),
	mFetchCount(0),
	mLastFetchCount(0),
	mFetchFolderCount(0),
	mAllRecursiveFoldersFetched(false),
	mRecursiveInventoryFetchStarted(false),
	mRecursiveLibraryFetchStarted(false)
{
}

bool LLInventoryModelFetch::isBulkFetchProcessingComplete() const
{
	return mFetchCount <= 0 && mFetchFolderQueue.empty() &&
		   mFetchItemQueue.empty();
}

bool LLInventoryModelFetch::isFolderFetchProcessingComplete() const
{
	return mFetchFolderCount <= 0 && mFetchFolderQueue.empty();
}

bool LLInventoryModelFetch::libraryFetchCompleted() const
{
	return mRecursiveLibraryFetchStarted &&
		   fetchQueueContainsNoDescendentsOf(gInventory.getLibraryRootFolderID());
}

bool LLInventoryModelFetch::inventoryFetchCompleted() const
{
	return mRecursiveInventoryFetchStarted &&
		   fetchQueueContainsNoDescendentsOf(gInventory.getRootFolderID());
}

void LLInventoryModelFetch::addRequestAtFront(const LLUUID& id, bool recursive,
											  bool is_category)
{
	U32 type = recursive ? FT_RECURSIVE : FT_DEFAULT;
	if (is_category)
	{
		mFetchFolderQueue.emplace_front(id, type, is_category);
	}
	else
	{
		mFetchItemQueue.emplace_front(id, type, is_category);
	}
}

void LLInventoryModelFetch::addRequestAtBack(const LLUUID& id, bool recursive,
											 bool is_category)
{
	U32 type = recursive ? FT_RECURSIVE : FT_DEFAULT;
	if (is_category)
	{
		mFetchFolderQueue.emplace_back(id, type, is_category);
	}
	else
	{
		mFetchItemQueue.emplace_back(id, type, is_category);
	}
}

void LLInventoryModelFetch::start(const LLUUID& id, bool recursive)
{
	bool is_cat = id.notNull() && gInventory.getCategory(id);

	if (is_cat || (!mAllRecursiveFoldersFetched && id.isNull()))
	{
		// It is a folder: do a bulk fetch.
		LL_DEBUGS("InventoryFetch") << "Start fetching category: " << id
									<< ", recursive: " << recursive << LL_ENDL;
		mBackgroundFetchActive = mFolderFetchActive = true;
		U32 fetch_type = recursive ? FT_RECURSIVE : FT_DEFAULT;
		if (id.isNull())	// Root folder fetch request
		{
			if (!mRecursiveInventoryFetchStarted)
			{
				mRecursiveInventoryFetchStarted |= recursive;
				const LLUUID& root_id = gInventory.getRootFolderID();
				if (recursive && useAISFetching())
				{
					// Not only root folder can be massive, but most system
					// folders will be requested independently, so request root
					// folder and content separately.

					mFetchFolderQueue.emplace_front(root_id,
													FT_FOLDER_AND_CONTENT,
													true);
				}
				else
				{
					mFetchFolderQueue.emplace_back(root_id, fetch_type, true);
				}
				gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
			}
			if (!mRecursiveLibraryFetchStarted)
			{
				mRecursiveLibraryFetchStarted |= recursive;
				const LLUUID& lib_id = gInventory.getLibraryRootFolderID();
				mFetchFolderQueue.emplace_back(lib_id, fetch_type, true);
				gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
			}
		}
		else
		{
			if (useAISFetching())
			{
				if (mFetchFolderQueue.empty() ||
					mFetchFolderQueue.back().mUUID != id)
				{
					// With AIS, make sure root goes to the top and follow up
					// recursive fetches, not individual requests.
					mFetchFolderQueue.emplace_back(id, fetch_type, true);
					gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
				}
			}
			else if (mFetchFolderQueue.empty() ||
					 mFetchFolderQueue.front().mUUID != id)
			{
				// Specific folder requests go to front of queue.
				mFetchFolderQueue.emplace_front(id, fetch_type, true);
				gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
			}
			if (id == gInventory.getLibraryRootFolderID())
			{
				mRecursiveLibraryFetchStarted |= recursive;
			}
			if (id == gInventory.getRootFolderID())
			{
				mRecursiveInventoryFetchStarted |= recursive;
			}
		}
	}
	else if (LLViewerInventoryItem* itemp = gInventory.getItem(id))
	{
		if (!itemp->isFinished())
		{
			scheduleItemFetch(id);
		}
	}
}

void LLInventoryModelFetch::scheduleFolderFetch(const LLUUID& id,
												bool force)
{
	if (mFetchFolderQueue.empty() || mFetchFolderQueue.front().mUUID != id)
	{
		mBackgroundFetchActive = true;
		U32 fetch_type = force ? FT_FORCED : FT_DEFAULT;
		// Specific folder requests go to front of queue.
		mFetchFolderQueue.emplace_front(id, fetch_type, true);
		gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
		LL_DEBUGS("InventoryFetch") << "Scheduled category " << id
									<< (force ? " for forced fetch."
											  : " for fetch.") << LL_ENDL;
	}
}

void LLInventoryModelFetch::scheduleItemFetch(const LLUUID& id, bool force)
{
	if (mFetchItemQueue.empty() || mFetchItemQueue.front().mUUID != id)
	{
		mBackgroundFetchActive = true;
		U32 fetch_type = force ? FT_FORCED : FT_DEFAULT;
		mFetchItemQueue.emplace_front(id, fetch_type, true);
		gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
		LL_DEBUGS("InventoryFetch") << "Scheduled item " << id
									<< (force ? " for forced fetch."
											  : " for fetch.") << LL_ENDL;
	}
}

void LLInventoryModelFetch::findLostItems()
{
	mBackgroundFetchActive = mFolderFetchActive = true;
	mFetchFolderQueue.emplace_back(LLUUID::null, FT_RECURSIVE, true);
	gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
}

void LLInventoryModelFetch::setAllFoldersFetched()
{
	if (mRecursiveInventoryFetchStarted && mRecursiveLibraryFetchStarted)
	{
		mAllRecursiveFoldersFetched = true;
	}

	mFolderFetchActive = false;
	if (isBulkFetchProcessingComplete())
	{
		mBackgroundFetchActive = false;
#if 1	// Avoids pointless idle callbacks when nothing is left to do. HB
		gIdleCallbacks.deleteFunction(backgroundFetchCB, NULL);
#endif
	}

	// Try and rebuild any broken links in the inventory now.
	gInventory.rebuildBrokenLinks();

	llinfos << "Inventory background fetch completed" << llendl;
}

//static
void LLInventoryModelFetch::backgroundFetchCB(void*)
{
	LLInventoryModelFetch::getInstance()->backgroundFetch();
}

void LLInventoryModelFetch::backgroundFetch()
{
	// Wait until we receive the agent region capabilities. HB
	if (!gAgent.regionCapabilitiesReceived())
	{
		return;
	}

	if (useAISFetching())
	{
		bulkFetchAIS();
		return;
	}

	const std::string& url =
		gAgent.getRegionCapability("FetchInventoryDescendents2");
	if (!url.empty())
	{
		bulkFetch(url);
		return;
	}

	// This should never happen any more, including in OpenSim (unless a grid
	// is running an antediluvian server version). HB
	llwarns_sparse << "Missing capability: cannot perform bulk fetch !"
				   << llendl;
}

void LLInventoryModelFetch::incrFetchCount(S32 fetching)
{
	mFetchCount += fetching;
	if (mFetchCount < 0)
	{
		llwarns_sparse << "Inventory fetch count fell below zero." << llendl;
		mFetchCount = 0;
	}
}

void LLInventoryModelFetch::incrFetchFolderCount(S32 fetching)
{
	incrFetchCount(fetching);
	mFetchFolderCount += fetching;
	if (mFetchFolderCount < 0)
	{
		llwarns_sparse << "Inventory categories fetch count fell below zero."
					   << llendl;
		mFetchFolderCount = 0;
	}
}

void LLInventoryModelFetch::onAISContentsCallback(const uuid_vec_t& ids_vec,
												  const LLUUID& response_id)
{
	// Do not push_front on failure: there is a chance it was fired from inside
	// bulkFetchAIS().
	incrFetchFolderCount(-1);

	for (U32 i = 0, count = ids_vec.size(); i < count; ++i)
	{
		const LLUUID& cat_id = ids_vec[i];
		mExpectedFolderIds.erase(cat_id);
		LLViewerInventoryCategory* catp = gInventory.getCategory(cat_id);
		if (catp)
		{
			catp->setFetching(LLViewerInventoryCategory::FETCH_NONE);
		}

		if (response_id.isNull())
		{
			// Failed to fetch; get it individually.
			mFetchFolderQueue.emplace_back(cat_id, FT_RECURSIVE, true);
			continue;
		}

		// Push descendant back to verify they are fetched fully (e.g. we did
		// not encounter depth limit).
		LLInventoryModel::cat_array_t* categories;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(cat_id, categories, items);
		if (categories)
		{
			for (LLInventoryModel::cat_array_t::const_iterator
					it = categories->begin(), end = categories->end();
				 it != end; ++it)
			{
				mFetchFolderQueue.emplace_back((*it)->getUUID(), FT_RECURSIVE,
											   true);
			}
		}
	}

	if (!mFetchFolderQueue.empty())
	{
		mBackgroundFetchActive = mFolderFetchActive = true;
		gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
	}
}

void LLInventoryModelFetch::onAISFolderCallback(const LLUUID& cat_id,
												const LLUUID& response_id,
												U32 fetch_type)
{
	if (!mExpectedFolderIds.erase(cat_id))
	{
		llwarns << "Unexpected folder response for: " << cat_id << llendl;
	}

	if (cat_id.isNull())
	{
		// Orphan: no other actions needed.
		// Note: return is done on purpose before incrFetchFolderCount(-1),
		// below since we did not incrFetchFolderCount(1) for orphans request,
		// to avoid requests number mismatch when no reply is received for
		// orphans. HB
		return;
	}

	// Do not push_front on failure: there is a chance it was fired from inside
	// bulkFetchAIS().
	incrFetchFolderCount(-1);

	if (response_id.isNull())	// Failed to fetch
	{
		if (fetch_type == FT_RECURSIVE)
		{
			// A full recursive request failed; try requesting folder and
			// nested contents separately.
			mFetchFolderQueue.emplace_back(cat_id, FT_CONTENT_RECURSIVE, true);
		}
		else if (fetch_type == FT_FOLDER_AND_CONTENT)
		{
			llwarns << "Failed to download folder: " << cat_id
					<< " - Requesting known content separately." << llendl;
			mFetchFolderQueue.emplace_back(cat_id, FT_CONTENT_RECURSIVE, true);
			// Set folder version to prevent viewer from trying to request
			// folder indefinetely.
			LLViewerInventoryCategory* catp = gInventory.getCategory(cat_id);
			if (catp && catp->isVersionUnknown())
			{
				catp->setVersion(0);
			}
		}
	}
	else if (fetch_type == FT_RECURSIVE)
	{
		// Got the folder and contents, now verify contents. Request contents
		// even for FT_RECURSIVE in case of changes, failures or if depth limit
		// gets imlemented. This should not re-download folders if they already
		// have a known version.
		LL_DEBUGS("InventoryFetch") << "Got folder: " << cat_id
									<< " - Requesting its contents."
									<< LL_ENDL;

		// Push descendant back to verify they are fetched fully (e.g. we did
		// not encounter depth limit).
		LLInventoryModel::cat_array_t* categories;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(cat_id, categories, items);
		if (categories)
		{
			for (LLInventoryModel::cat_array_t::const_iterator
					it = categories->begin(), end = categories->end();
				 it != end; ++it)
			{
				mFetchFolderQueue.emplace_back((*it)->getUUID(), FT_RECURSIVE,
											   true);
			}
		}
	}
	else if (fetch_type == FT_FOLDER_AND_CONTENT)
	{
		// Read folder for contents request.
		mFetchFolderQueue.emplace_front(cat_id, FT_CONTENT_RECURSIVE, true);
	}
	else
	{
		LL_DEBUGS("InventoryFetch") << "Got folder: " << cat_id << LL_ENDL;
	}

	if (!mFetchFolderQueue.empty())
	{
		mBackgroundFetchActive = mFolderFetchActive = true;
		gIdleCallbacks.addFunction(backgroundFetchCB, NULL);
	}

	LLViewerInventoryCategory* catp = gInventory.getCategory(cat_id);
	if (catp)
	{
		catp->setFetching(LLViewerInventoryCategory::FETCH_NONE);
	}
}

void LLInventoryModelFetch::bulkFetchAIS()
{
	if (gDisconnected || LLApp::isExiting())
	{
		gIdleCallbacks.deleteFunction(backgroundFetchCB, NULL);
		return;
	}

	static LLCachedControl<U32> ais_pool(gSavedSettings, "PoolSizeAIS");
	// Do not launch too many requests at once; AIS throttles. Also, reserve
	// one request for actions outside of fetch (like renames).
	S32 max_fetches = llclamp((S32)ais_pool, 2, 51) - 1;

	// Do not loop for too long (in case of large, fully loaded inventory)
	mFetchTimer.reset();
	bool short_timeout = LLStartUp::getStartupState() > STATE_WEARABLES_WAIT;
	mFetchTimer.setTimerExpirySec(short_timeout ? 0.005f : 1.f);

	S32 last_fetch_count = mFetchCount;

	while (!mFetchFolderQueue.empty() && mFetchCount < max_fetches &&
		   !mFetchTimer.hasExpired())
	{
		const FetchQueueInfo& fetch_info = mFetchFolderQueue.front();
		bulkFetchAIS(fetch_info);
		mFetchFolderQueue.pop_front();
	}
	// Ideally we should not fetch items if recursive fetch is not done, but
	// there is a chance some request will start timing out and recursive fetch
	// would then get stuck on a single folder, so we need to keep item fetches
	// going to avoid such an issue.
	while (!mFetchItemQueue.empty() && mFetchCount < max_fetches &&
		   !mFetchTimer.hasExpired())
	{
		const FetchQueueInfo& fetch_info = mFetchItemQueue.front();
		bulkFetchAIS(fetch_info);
		mFetchItemQueue.pop_front();
	}

	if (mFetchCount != last_fetch_count || mFetchCount != mLastFetchCount)
	{
		LL_DEBUGS("InventoryFetch") << "Total active fetches went from "
									<< mLastFetchCount << " to " << mFetchCount
									<< " with " << mFetchFolderQueue.size()
									<< " scheduled folder fetches and "
									<< mFetchItemQueue.size()
									<< " scheduled item fetches." << LL_ENDL;
		mLastFetchCount = mFetchCount;
	}

	if (mFolderFetchActive && isFolderFetchProcessingComplete())
	{
		setAllFoldersFetched();
	}
	if (isBulkFetchProcessingComplete())
	{
		mBackgroundFetchActive = false;
	}
}

static void ais_simple_item_cb(const LLUUID& response_id)
{
	LL_DEBUGS("InventoryFetch") << "Got simple response Id:" << response_id
								<< LL_ENDL;
	LLInventoryModelFetch::getInstance()->incrFetchCount(-1);
}

// I do not like lambdas... HB
static void fetch_orphans_cb(const LLUUID& response_id)
{
	if (gDisconnected || LLApp::isExiting())
	{
		return;
	}
	LL_DEBUGS("InventoryFetch") << "Got orphans reply Id: " << response_id
								<< LL_ENDL;
	// Note: the '0' below is for FT_DEFAULT; should it actually be recursive ?
	LLInventoryModelFetch::getInstance()->onAISFolderCallback(LLUUID::null,
															  response_id, 0);
}

static void fetch_contents_cb(uuid_vec_t children, const LLUUID& response_id)
{
	if (gDisconnected || LLApp::isExiting())
	{
		return;
	}
	LL_DEBUGS("InventoryFetch") << "Got contents reply Id: " << response_id
								<< LL_ENDL;
	LLInventoryModelFetch::getInstance()->onAISContentsCallback(children,
																response_id);
}

static void fetch_folder_cb(LLUUID cat_id, U32 type, const LLUUID& response_id)
{
	if (gDisconnected || LLApp::isExiting())
	{
		return;
	}
	LL_DEBUGS("InventoryFetch") << "Got folder reply Id: " << response_id
								<< LL_ENDL;
	LLInventoryModelFetch::getInstance()->onAISFolderCallback(cat_id,
															  response_id,
															  type);
}

void LLInventoryModelFetch::bulkFetchAIS(const FetchQueueInfo& fetch_info)
{
	const LLUUID& id = fetch_info.mUUID;

	if (!fetch_info.mIsCategory)	// If this is an inventory item...
	{
		bool needs_fetch = false;
		bool is_library = false;
		LLViewerInventoryItem* itemp = gInventory.getItem(id);
		if (!itemp)
		{
			// We do not know it at all, so assume it is incomplete.
			needs_fetch = true;
		}
		else if (!itemp->isFinished() || fetch_info.mFetchType == FT_FORCED)
		{
			needs_fetch = true;
			is_library = itemp->getPermissions().getOwner() != gAgentID;
		}
		if (needs_fetch)
		{
			++mFetchCount;
			AISAPI::fetchItem(id, is_library, ais_simple_item_cb);
		}
		return;
	}

	// Inventory category cases.

	if (id.isNull()) // Lost & found case.
	{
#if 0	// Do NOT increment the count for this request: it may not receive any
		// reply when there are no orphans and we would be left in indefinitely
		// "loading" inventory state. HB
		incrFetchFolderCount(1);
#endif
		mExpectedFolderIds.emplace(id);
		AISAPI::fetchOrphans(fetch_orphans_cb);
		return;
	}

	LLViewerInventoryCategory* catp = gInventory.getCategory(id);
	if (!catp)
	{
		return;	// Could try and fetch it in another way instead ?
	}

	bool is_library = catp->getOwnerID() == ALEXANDRIA_LINDEN_ID;

	U32 fetch_type = fetch_info.mFetchType;
	if (fetch_type == FT_CONTENT_RECURSIVE)
	{
		static LLCachedControl<U32> ais_batch(gSavedSettings, "BatchSizeAIS3");
		// Top limit is 'as many as you can put into an URL'.
		size_t batch_limit = llclamp((size_t)ais_batch, 1, 40);

		U32 target_state = LLViewerInventoryCategory::FETCH_RECURSIVE;
		bool content_done = true;

		// Fetch contents only, ignoring the category itself
		uuid_vec_t children;
		LLInventoryModel::cat_array_t* categories;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(id, categories, items);
		if (categories)
		{
			for (LLInventoryModel::cat_array_t::const_iterator
					it = categories->begin(), end = categories->end();
				 it != end; ++it)
			{
				LLViewerInventoryCategory* childp = *it;
				if (childp->getFetching() >= target_state ||
					!childp->isVersionUnknown())
				{
					continue;
				}

				if (childp->getPreferredType() ==
						LLFolderType::FT_MARKETPLACE_LISTINGS)
				{
					// Fetch marketplace alone; should it actually be
					// fetched as FT_FOLDER_AND_CONTENT ?
					if (!children.empty())
					{
						// Ignore it now so that it can instead be fetched
						// alone on next run(s).
						content_done = false;
						continue;
					}
					// This will cause to break from the loop below, after
					// registering this only marketplace folder for fetch. HB
					batch_limit = 0;
				}

				const LLUUID& child_id = childp->getUUID();
				children.emplace_back(child_id);
				mExpectedFolderIds.emplace(child_id);
				childp->setFetching(target_state);
				if (children.size() >= batch_limit)
				{
					content_done = false;
					break;
				}
			}
			if (!children.empty())
			{
				// Increment before call in case of immediate callback
				incrFetchFolderCount(1);
				AISAPI::fetchCategorySubset(id, children, is_library, true,
											boost::bind(fetch_contents_cb,
														children, _1));
			}
			if (content_done)
			{
				// This will have a bit of overlap with onAISContentCallback(),
				// but something else might have dowloaded folders, so verify
				// every child that is complete has its children done as well.
				for (LLInventoryModel::cat_array_t::const_iterator
						it = categories->begin(), end = categories->end();
					 it != end; ++it)
				{
					LLViewerInventoryCategory* childp = *it;
					if (!childp->isVersionUnknown())
					{
						mFetchFolderQueue.emplace_back(childp->getUUID(),
													   FT_RECURSIVE, true);
					}
				}
			}
			else
			{
				// Send it back to get the rest
				mFetchFolderQueue.emplace_back(id, FT_CONTENT_RECURSIVE, true);
			}
		}
	}
	else if (fetch_type == FT_FORCED || catp->isVersionUnknown())
	{
		U32 target_state;
		if (fetch_type > FT_CONTENT_RECURSIVE)
		{
			target_state = LLViewerInventoryCategory::FETCH_RECURSIVE;
		}
		else
		{
			target_state = LLViewerInventoryCategory::FETCH_NORMAL;
		}
		// Start again if we did a non-recursive fetch before to get all
		// children in a single request.
		if (catp->getFetching() < target_state)
		{
			// Increment before call in case of immediate callback
			incrFetchFolderCount(1);
			catp->setFetching(target_state);
			mExpectedFolderIds.emplace(id);
			bool recurse = fetch_type == FT_RECURSIVE;
			AISAPI::fetchCategoryChildren(id, is_library, recurse,
										  boost::bind(fetch_folder_cb, id,
													  fetch_type, _1));
		}
	}
	// Already fetched, check if anything inside needs fetching.
	else if (fetch_type == FT_RECURSIVE || fetch_type == FT_FOLDER_AND_CONTENT)
	{
		LLInventoryModel::cat_array_t* categories;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(id, categories, items);
		if (categories)
		{
			for (LLInventoryModel::cat_array_t::const_iterator
					it = categories->begin(), end = categories->end();
				 it != end; ++it)
			{
				// Send it back to get the rest (not front, to avoid an
				// infinite loop).
				mFetchFolderQueue.emplace_back((*it)->getUUID(), FT_RECURSIVE,
											   true);
			}
		}
	}
}

// Bundle up a bunch of requests to send all at once.
//static
void LLInventoryModelFetch::bulkFetch(const std::string& url)
{
	// Background fetch is called from gIdleCallbacks in a loop until
	// background fetch is stopped. If there are items in mFetch*Queue, we want
	// to check the time since the last bulkFetch was sent. If it exceeds our
	// retry time, go ahead and fire off another batch.

	// *TODO: these values could be tweaked at runtime to effect a fast/slow
	// fetch throttle. Once login is complete and the scene is mostly loaded,
	// we could turn up the throttle and fill missing inventory quicker.
	constexpr U32 max_batch_size = 10;
	// Outstanding requests, not connections
	constexpr S32 max_concurrent_fetches = 12;

	if (gDisconnected || LLApp::isExiting())
	{
		gIdleCallbacks.deleteFunction(backgroundFetchCB, NULL);
		return; // Just bail if we are disconnected
	}

	if (mFetchCount)
	{
		// Process completed background HTTP requests
		gInventory.handleResponses(false);
	}

	if (mFetchCount > max_concurrent_fetches)
	{
		return;
	}

	U32 item_count = 0;
	U32 folder_count = 0;

	static LLCachedControl<U32> inventory_sort_order(gSavedSettings,
													 "InventorySortOrder");
	U32 sort_order = (U32)inventory_sort_order & 0x1;

	uuid_vec_t recursive_cats;
	uuid_list_t all_cats;		// Duplicate avoidance.

	LLSD folder_request_body;
	LLSD folder_request_body_lib;
	LLSD item_request_body;
	LLSD item_request_body_lib;

	const LLUUID& lib_owner_id = gInventory.getLibraryOwnerID();

	while (!mFetchFolderQueue.empty() &&
		   item_count + folder_count < max_batch_size)
	{
		const FetchQueueInfo& fetch_info = mFetchFolderQueue.front();

		const LLUUID& cat_id = fetch_info.mUUID;
		if (all_cats.count(cat_id))
		{
			// Duplicate, skip.
			mFetchFolderQueue.pop_front();
			continue;
		}
		all_cats.emplace(cat_id);

		if (fetch_info.mFetchType >= FT_CONTENT_RECURSIVE)
		{
			recursive_cats.emplace_back(cat_id);
		}

		if (cat_id.isNull()) // DEV-17797
		{
			LLSD folder_sd;
			folder_sd["folder_id"] = LLUUID::null.asString();
			folder_sd["owner_id"] = gAgentID;
			folder_sd["sort_order"] = (LLSD::Integer)sort_order;
			folder_sd["fetch_folders"] = false;
			folder_sd["fetch_items"] = true;
			folder_request_body["folders"].append(folder_sd);
			++folder_count;
			mFetchFolderQueue.pop_front();
			continue;
		}

		const LLViewerInventoryCategory* catp = gInventory.getCategory(cat_id);
		if (!catp)
		{
			mFetchFolderQueue.pop_front();
			continue;
		}

		if (catp->isVersionUnknown())
		{
			LLSD folder_sd;
			folder_sd["folder_id"] = cat_id;
			folder_sd["owner_id"] = catp->getOwnerID();
			folder_sd["sort_order"] = (LLSD::Integer)sort_order;
			// (LLSD::Boolean)sFullFetchStarted;
			folder_sd["fetch_folders"] = true;
			folder_sd["fetch_items"] = true;

			if (catp->getOwnerID() == lib_owner_id)
			{
				folder_request_body_lib["folders"].append(folder_sd);
			}
			else
			{
				folder_request_body["folders"].append(folder_sd);
			}
			++folder_count;
		}
		else
		{
			// May already have this folder, but append child folders to list.
			if (fetch_info.mFetchType >= FT_CONTENT_RECURSIVE)
			{
				LLInventoryModel::cat_array_t* categories;
				LLInventoryModel::item_array_t* items;
				gInventory.getDirectDescendentsOf(cat_id, categories, items);
				if (categories)
				{
					for (LLInventoryModel::cat_array_t::const_iterator
							it = categories->begin(), end = categories->end();
						 it != end; ++it)
					{
						mFetchFolderQueue.emplace_back((*it)->getUUID(),
													   fetch_info.mFetchType,
													   true);
					}
				}
			}
		}

		mFetchFolderQueue.pop_front();
	}

	while (!mFetchItemQueue.empty() &&
		   item_count + folder_count < max_batch_size)
	{
		const FetchQueueInfo& fetch_info = mFetchItemQueue.front();
		const LLUUID& item_id = fetch_info.mUUID;

		LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
		if (itemp)
		{
			LLSD item_sd;
			item_sd["owner_id"] = itemp->getPermissions().getOwner();
			item_sd["item_id"] = item_id;
			if (itemp->getPermissions().getOwner() == gAgentID)
			{
				item_request_body.append(item_sd);
			}
			else
			{
				item_request_body_lib.append(item_sd);
			}
#if 0
			itemp->fetchFromServer();
#endif
			++item_count;
		}

		mFetchItemQueue.pop_front();
	}

	if (!item_count && !folder_count)
	{
		if (isBulkFetchProcessingComplete())
		{
			setAllFoldersFetched();
		}
		return;
	}

	// Issue HTTP POST requests to fetch folders and items

	if (folder_request_body["folders"].size())
	{
		BGFolderHttpHandler::postRequest(url, folder_request_body,
										 recursive_cats);
	}

	if (folder_request_body_lib["folders"].size())
	{
		const std::string& url =
			gAgent.getRegionCapability("FetchLibDescendents2");
		if (!url.empty())
		{
			BGFolderHttpHandler::postRequest(url, folder_request_body_lib,
											 recursive_cats, true);
		}
	}

	if (item_request_body.size())
	{
		const std::string& url = gAgent.getRegionCapability("FetchInventory2");
		if (!url.empty())
		{
			LLSD body;
			body["items"] = item_request_body;
			BGItemHttpHandler::postRequest(url, body);
		}
	}

	if (item_request_body_lib.size())
	{
		const std::string& url = gAgent.getRegionCapability("FetchLib2");
		if (!url.empty())
		{
			LLSD body;
			body["items"] = item_request_body_lib;
			BGItemHttpHandler::postRequest(url, body, true);
		}
	}
}

bool LLInventoryModelFetch::fetchQueueContainsNoDescendentsOf(const LLUUID& cat_id) const
{
	for (fetch_queue_t::const_iterator it = mFetchFolderQueue.begin(),
									   end = mFetchFolderQueue.end();
		 it != end; ++it)
	{
		const LLUUID& fetch_id = it->mUUID;
		if (gInventory.isObjectDescendentOf(fetch_id, cat_id))
		{
			return false;
		}
	}
	for (fetch_queue_t::const_iterator it = mFetchItemQueue.begin(),
									   end = mFetchItemQueue.end();
		 it != end; ++it)
	{
		const LLUUID& fetch_id = it->mUUID;
		if (gInventory.isObjectDescendentOf(fetch_id, cat_id))
		{
			return false;
		}
	}
	return true;
}

//static
void LLInventoryModelFetch::forceFetchFolder(const LLUUID& cat_id)
{
	LLInventoryModelFetch* self = getInstance();
	self->scheduleFolderFetch(cat_id, true);	// true = force
	self->start(cat_id, false);					// false = not recursive
}

//static
void LLInventoryModelFetch::forceFetchItem(const LLUUID& item_id)
{
	forceFetchItem(gInventory.getItem(item_id));
}

//static
void LLInventoryModelFetch::forceFetchItem(const LLInventoryItem* itemp)
{
	if (!itemp)
	{
		return;
	}
	LLInventoryModelFetch* self = getInstance();
	self->scheduleItemFetch(itemp->getUUID(), true);	// true = force
	if (useAISFetching())
	{
		// Scheduling is not enough with AIS3: we need to trigger the fetch on
		// the parent folder as well. HB
		const LLUUID& parent_id = itemp->getParentUUID();
		self->scheduleFolderFetch(parent_id, true);	// true = force
		self->start(parent_id, false);				// false = not recursive
	}
}
