/**
 * @file llmarketplacefunctions.cpp
 * @brief Implementation of assorted functions related to the marketplace
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include "llmarketplacefunctions.h"

#include "llcallbacklist.h"
#include "llcorehttputil.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "lltrans.h"

#include "llagent.h"
#include "llgridmanager.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llweb.h"

// static variable members
std::string LLMarketplace::sMessage;
LLUUID LLMarketplace::sMarketplaceListingId;

// Helpers

// Get the version folder: if there is only one subfolder, we will use it as a
// version folder
LLUUID getVersionFolderIfUnique(const LLUUID& folder_id)
{
	LLUUID version_id;
	LLInventoryModel::cat_array_t* categories;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(folder_id, categories, items);
	if (categories && categories->size() == 1)
	{
		version_id = categories->begin()->get()->getUUID();
	}
	else
	{
		gNotifications.add("AlertMerchantListingActivateRequired");
	}
	return version_id;
}

void log_SLM_warning(const std::string& request, U32 status,
					 const std::string& reason, const std::string& code,
					 std::string message)
{
	llwarns << "SLM API: Responder to: " << request << " - Status: " << status
			<< " - Reason: " << reason << " - Code: " << code
			<< " - Description: " << message << llendl;
	LLStringUtil::replaceString(message, std::string("["), std::string("- "));
	LLStringUtil::replaceString(message, std::string("\""), LLStringUtil::null);
	LLStringUtil::replaceString(message, std::string(","), "\n-");
	LLStringUtil::replaceString(message, std::string("]"), LLStringUtil::null);
	if (message.length() > 512)
	{
		// We do not show long messages in the alert (unlikely to be readable).
		// The full message string will be in the log though.
		message = message.substr(0, 504) + "\n.../...";
	}
	LLSD subs;
	subs["ERROR_REASON"] = reason;
	subs["ERROR_DESCRIPTION"] = message;
	gNotifications.add(status == 422 ? "MerchantUnprocessableEntity"
									 : "MerchantTransactionFailed", subs);
}

///////////////////////////////////////////////////////////////////////////////
// New Marketplace Listings API tuples and data

class LLMarketplaceInventoryObserver final : public LLInventoryObserver
{
protected:
	LOG_CLASS(LLMarketplaceInventoryObserver);

public:
	LLMarketplaceInventoryObserver()				{}
	~LLMarketplaceInventoryObserver() override		{}
	void changed(U32 mask) override;
};

void LLMarketplaceInventoryObserver::changed(U32 mask)
{
	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();

	// When things are added to the marketplace, we might need to re-validate
	// and fix the containing listings
	if (mask & LLInventoryObserver::ADD)
	{
		const uuid_list_t& changed_items = gInventory.getChangedIDs();

		// First, count the number of items in this list...
		S32 count = 0;
		for (uuid_list_t::const_iterator it = changed_items.begin(),
										 end = changed_items.end();
			 it != end; ++it)
		{
			LLInventoryObject* obj = gInventory.getObject(*it);
			if (obj && obj->getType() != LLAssetType::AT_CATEGORY)
			{
				++count;
			}
		}

		// Then, decrement the folders of that amount. Note that among all of
		// those, only one folder will be a listing folder (if at all), the
		// others will be ignored by the decrement method.
		for (uuid_list_t::const_iterator it = changed_items.begin(),
										 end = changed_items.end();
			 it != end; ++it)
		{
			LLInventoryObject* obj = gInventory.getObject(*it);
			if (obj && obj->getType() != LLAssetType::AT_CATEGORY)
			{
				marketdata->decrementValidationWaiting(obj->getUUID(), count);
			}
		}
	}

	// When things are changed in the inventory, this can trigger a host of
	// changes in the marketplace listings folder:
	// * stock counts changing: no copy items coming in and out will change
	//   the stock count on folders;
	// * version and listing folders: moving those might invalidate the
	//   marketplace data itself.
	// Since we cannot raise inventory change while the observer is called (the
	// list will be cleared once observers are called) we need to raise a flag
	// in the inventory to signal that things have been dirtied.

	if (mask & (LLInventoryObserver::INTERNAL | LLInventoryObserver::STRUCTURE))
	{
		const LLUUID& group_id = gAgent.getGroupID();
		const uuid_list_t& changed_items = gInventory.getChangedIDs();
		for (uuid_list_t::const_iterator it = changed_items.begin(),
										 end = changed_items.end();
			 it != end; ++it)
		{
			LLInventoryObject* objp = gInventory.getObject(*it);
			if (!objp) continue;

			if (objp->getType() == LLAssetType::AT_CATEGORY)
			{
				// If it is a folder known to the marketplace, let's check it
				// is in proper shape
				if (marketdata->isListed(*it) ||
					marketdata->isVersionFolder(*it))
				{
					marketdata->listForIdleValidation(*it);
				}
			}
			else
			{
				// If it is not a category, it is an item...
				LLViewerInventoryItem* itemp = gInventory.getItem(*it);
				// If it is a no copy item, we may need to update the label
				// count of marketplace listings
				if (itemp &&
					!itemp->getPermissions().allowCopyBy(gAgentID, group_id))
				{
					marketdata->setDirtyCount();
				}
			}
		}
	}
}

// Tuple == Item
LLMarketplaceTuple::LLMarketplaceTuple()
:	mListingId(0),
	mIsActive(false),
	mCountOnHand(0)
{
}

LLMarketplaceTuple::LLMarketplaceTuple(const LLUUID& folder_id)
:	mListingFolderId(folder_id),
	mListingId(0),
	mIsActive(false),
	mCountOnHand(0)
{
}

LLMarketplaceTuple::LLMarketplaceTuple(const LLUUID& folder_id, S32 listing_id,
									   const LLUUID& version_id, bool is_listed)
:	mListingFolderId(folder_id),
	mListingId(listing_id),
	mVersionFolderId(version_id),
	mIsActive(is_listed),
	mCountOnHand(0)
{
}

LLMarketplaceTuple::LLMarketplaceTuple(const LLUUID& folder_id, S32 listing_id,
									   const LLUUID& version_id, bool is_listed,
									   const std::string& edit_url, S32 count)
:	mListingFolderId(folder_id),
	mListingId(listing_id),
	mVersionFolderId(version_id),
	mIsActive(is_listed),
	mEditURL(edit_url),
	mCountOnHand(count)
{
}

// Data map
LLMarketplaceData::LLMarketplaceData()
:	mMarketPlaceStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED),
	mMarketPlaceDataFetched(MarketplaceFetchCodes::MARKET_FETCH_NOT_DONE),
	mStatusUpdatedSignal(NULL),
	mDirtyCount(false),
	// NOTE: by using these instead of omitting the corresponding
	// xxxAndSuspend() parameters, we avoid seeing such classes constructed
	// and destroyed each time...
	mHttpOptions(new LLCore::HttpOptions),
	mHttpHeaders(new LLCore::HttpHeaders)
{
	gIdleCallbacks.addFunction(idleCallback, this);
	mInventoryObserver = new LLMarketplaceInventoryObserver;
	gInventory.addObserver(mInventoryObserver);

	// NOTE: mHttpHeaders is used for Json requests only
	mHttpHeaders->append(HTTP_OUT_HEADER_ACCEPT, "application/json");
	mHttpHeaders->append(HTTP_OUT_HEADER_CONTENT_TYPE, "application/json");
}

LLMarketplaceData::~LLMarketplaceData()
{
	gIdleCallbacks.deleteFunction(idleCallback, this);
	if (mInventoryObserver)
	{
		gInventory.removeObserver(mInventoryObserver);
		mInventoryObserver = NULL;
	}
	mHttpOptions.reset();
	mHttpHeaders.reset();
}

void LLMarketplaceData::initializeSLM(const status_updated_signal_t::slot_type& cb)
{
	if (mStatusUpdatedSignal == NULL)
	{
		mStatusUpdatedSignal = new status_updated_signal_t();
	}
	mStatusUpdatedSignal->connect(cb);

	if (mMarketPlaceStatus == MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED ||
		mMarketPlaceStatus == MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE)
	{
		// Initiate SLM connection and set responder
		std::string url = getSLMConnectURL("/merchant");
		if (url.empty())
		{
			// No capability... Init failed.
			LL_DEBUGS("Marketplace") << "Marketplace capability empty, cannot initialize"
									 << LL_ENDL;
			setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE);
		}
		else
		{
			mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_INITIALIZING;
			llinfos << "Initializing the Marketplace Listings" << llendl;
			LL_DEBUGS("Marketplace") << "Sending resquest: " << url << LL_ENDL;
			gCoros.launch("getMerchantStatus",
						  boost::bind(&LLMarketplaceData::getMerchantStatusCoro,
									  this, url));
		}
	}
	else
	{
		// If already initialized or initializing, just confirm the status so
		// that the callback gets called
		LL_DEBUGS("Marketplace") << "Marketplace already initialized or initializing"
								 << LL_ENDL;
		setSLMStatus(mMarketPlaceStatus);
	}
}

void LLMarketplaceData::getMerchantStatusCoro(const std::string& url)
{
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setFollowRedirects(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("getMerchantStatusCoro");
	LLSD result = adapter.getAndSuspend(url, options);

	if (!instanceExists()) return;	// Viewer is being closed down !

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	S32 http_code = status.getType();
	if (status)
	{
		LL_DEBUGS("Marketplace") << "Status: " << http_code
								 << " - User is a merchant" << LL_ENDL;
		setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_MERCHANT);
	}
	else if (http_code == HTTP_NOT_FOUND)
	{
		LL_DEBUGS("Marketplace") << "Status: " << http_code
								 << " - User is not a merchant" << LL_ENDL;
		setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_MERCHANT);
	}
	else if (http_code == HTTP_SERVICE_UNAVAILABLE)
	{
		LL_DEBUGS("Marketplace") << "Status: " << http_code
								 << " - Merchant is not migrated"
								 << LL_ENDL;
		setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_MIGRATED_MERCHANT);
	}
	else if (http_code == HTTP_INTERNAL_ERROR)
	{
		// 499 includes timeout and ssl error - marketplace is down or having
		// issues, we do not show it in this request according to MAINT-5938
		llwarns << "Server internal error reported, reason: "
				<< status.toString() << " - Code: "
				<< result["error_code"].asString()
				<< " - Description: " << result["error_description"].asString()
				<< llendl;
		setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE);
	}
	else
	{
		log_SLM_warning("Get merchant", http_code, status.toString(),
						result["error_code"].asString(),
						result["error_description"].asString());
		setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE);
	}
}

// Get/Post/Put requests to the SLM Server using the SLM API

void LLMarketplaceData::getSLMListings()
{
	std::string url = getSLMConnectURL("/listings");
	if (url.empty()) return;

	// Send request
	const LLUUID& market_id = LLMarketplace::getMPL();
	if (market_id.notNull())
	{
		LL_DEBUGS("Marketplace") << "Sending resquest: " << url << LL_ENDL;
		setUpdating(market_id, true);
		gCoros.launch("getSLMListings",
					  boost::bind(&LLMarketplaceData::getSLMListingsCoro, this,
								  url, market_id));
	}
}

void LLMarketplaceData::getSLMListingsCoro(const std::string& url,
										   LLUUID expected_folder_id)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("getSLMListingsCoro");
	LLSD result = adapter.getJsonAndSuspend(url, mHttpOptions, mHttpHeaders);

	if (!instanceExists()) return;	// Viewer is being closed down !

	setUpdating(expected_folder_id, false);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("Marketplace") << "Body: " << result << LL_ENDL;

    	for (LLSD::array_iterator it = result["listings"].beginArray(),
								  end = result["listings"].endArray();
			 it != end; ++it)
		{
			const LLSD& listing = *it;
			S32 listing_id = listing["id"].asInteger();
			bool is_listed = listing["is_listed"].asBoolean();
			std::string edit_url = listing["edit_url"].asString();
			LLUUID folder_id = listing["inventory_info"]["listing_folder_id"].asUUID();
			LLUUID version_id = listing["inventory_info"]["version_folder_id"].asUUID();
			S32 count = listing["inventory_info"]["count_on_hand"].asInteger();
			if (folder_id.notNull())
			{
				addListing(folder_id, listing_id, version_id, is_listed,
						   edit_url, count);
			}
		}

		setSLMDataFetched(MarketplaceFetchCodes::MARKET_FETCH_DONE);
	}
	else
	{
		log_SLM_warning("Get listings", status.getType(), status.toString(),
						"", result.asString());
		setSLMDataFetched(MarketplaceFetchCodes::MARKET_FETCH_FAILED);
	}

	// Update all folders under the root
	LLMarketplace::updateCategory(expected_folder_id, false);
	gInventory.notifyObservers();
}

void LLMarketplaceData::getSLMListing(S32 listing_id)
{
	std::string url = getSLMConnectURL(llformat("/listing/%d", listing_id));
	if (url.empty()) return;

	// Send request
	LL_DEBUGS("Marketplace") << "Sending resquest: " << url << LL_ENDL;
	const LLUUID& folder_id = getListingFolder(listing_id);
	setUpdating(folder_id, true);
	gCoros.launch("getSLMListings",
				  boost::bind(&LLMarketplaceData::getSLMListingCoro, this, url,
							  folder_id));
}

void LLMarketplaceData::getSLMListingCoro(const std::string& url,
										  LLUUID expected_folder_id)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("getSLMListingCoro");
	LLSD result = adapter.getJsonAndSuspend(url, mHttpOptions, mHttpHeaders);

	if (!instanceExists()) return;	// Viewer is being closed down !

	setUpdating(expected_folder_id, false);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("Marketplace") << "Body: " << result << LL_ENDL;

    	for (LLSD::array_iterator it = result["listings"].beginArray(),
								  end = result["listings"].endArray();
			 it != end; ++it)
		{
			const LLSD& listing = *it;
			S32 listing_id = listing["id"].asInteger();
			bool is_listed = listing["is_listed"].asBoolean();
			std::string edit_url = listing["edit_url"].asString();
			LLUUID folder_id = listing["inventory_info"]["listing_folder_id"].asUUID();
			LLUUID version_id = listing["inventory_info"]["version_folder_id"].asUUID();
			S32 count = listing["inventory_info"]["count_on_hand"].asInteger();

			// Update that listing
			setListingID(folder_id, listing_id, false);
			setVersionFolderID(folder_id, version_id, false);
			setActivationState(folder_id, is_listed, false);
			setListingURL(folder_id, edit_url, false);
			setCountOnHand(folder_id, count, false);
			LLMarketplace::updateCategory(folder_id, false);
			gInventory.notifyObservers();
		}
	}
	else
	{
		S32 http_code = status.getType();
		if (http_code == HTTP_NOT_FOUND)
		{
			// That listing does not exist -> delete its record from the local
			// SLM data store
			deleteListing(expected_folder_id, false);
		}
		else
		{
			log_SLM_warning("Get listing", http_code, status.toString(), "",
							result.asString());
		}
		LLMarketplace::updateCategory(expected_folder_id, false);
		gInventory.notifyObservers();
	}
}

void LLMarketplaceData::createSLMListing(const LLUUID& folder_id,
										 const LLUUID& version_id, S32 count)
{
	std::string url = getSLMConnectURL("/listings");
	if (url.empty()) return;

	LLViewerInventoryCategory* category = gInventory.getCategory(folder_id);
	if (!category)
	{
		llwarns << "Cannot find category for folder Id: " << folder_id
				<< llendl;
		return;
	}

	// Build the message
	LLSD inventory_info;
	inventory_info["listing_folder_id"] = folder_id;
	inventory_info["version_folder_id"] = version_id;
	inventory_info["count_on_hand"] = count;

	LLSD listing;
	listing["name"] = category->getName();
	listing["inventory_info"] = inventory_info;

	LLSD data;
	data["listing"] = listing;

	// Send request
	LL_DEBUGS("Marketplace") << "Sending resquest: " << url << " - Body:"
							 << data << LL_ENDL;
	setUpdating(folder_id, true);
	gCoros.launch("createSLMListingCoro",
				  boost::bind(&LLMarketplaceData::createSLMListingCoro, this,
							  url, folder_id, data));
}

void LLMarketplaceData::createSLMListingCoro(const std::string& url,
											 LLUUID expected_folder_id,
											 const LLSD& data)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("getSLMListingCoro");
	LLSD result = adapter.postJsonAndSuspend(url, data, mHttpOptions,
											 mHttpHeaders);

	if (!instanceExists()) return;	// Viewer is being closed down !

	setUpdating(expected_folder_id, false);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("Marketplace") << "Body: " << result << LL_ENDL;

    	for (LLSD::array_iterator it = result["listings"].beginArray(),
								  end = result["listings"].endArray();
			 it != end; ++it)
		{
			const LLSD& listing = *it;
			S32 listing_id = listing["id"].asInteger();
			bool is_listed = listing["is_listed"].asBoolean();
			std::string edit_url = listing["edit_url"].asString();
			LLUUID folder_id = listing["inventory_info"]["listing_folder_id"].asUUID();
			LLUUID version_id = listing["inventory_info"]["version_folder_id"].asUUID();
			S32 count = listing["inventory_info"]["count_on_hand"].asInteger();

			addListing(folder_id, listing_id, version_id, is_listed, edit_url,
					   count);
			LLMarketplace::updateCategory(folder_id, false);
			gInventory.notifyObservers();
		}
	}
	else
	{
		log_SLM_warning("Post listing", status.getType(), status.toString(),
						"", result.asString());
		LLMarketplace::updateCategory(expected_folder_id, false);
		gInventory.notifyObservers();
	}
}

void LLMarketplaceData::updateSLMListing(const LLUUID& folder_id,
										 S32 listing_id,
										 const LLUUID& version_id,
										 bool is_listed, S32 count)
{
	std::string url = getSLMConnectURL(llformat("/listing/%d", listing_id));
	if (url.empty()) return;

	// Auto unlist if the count is 0 (out of stock)
	if (is_listed && count == 0)
	{
		is_listed = false;
		gNotifications.add("AlertMerchantStockFolderEmpty");
	}

	// Note: we are assuming that sending unchanged info would not break
	// anything server side...

	// Build the message
	LLSD inventory_info;
	inventory_info["listing_folder_id"] = folder_id;
	inventory_info["version_folder_id"] = version_id;
	inventory_info["count_on_hand"] = count;

	LLSD listing;
	listing["id"] = listing_id;
	listing["is_listed"] = is_listed;
	listing["inventory_info"] = inventory_info;

	LLSD data;
	data["listing"] = listing;

	// Send request
	LL_DEBUGS("Marketplace") << "Sending resquest: " << url << " - Body:"
							 << data << LL_ENDL;
	setUpdating(folder_id, true);
	gCoros.launch("updateSLMListingCoro",
				  boost::bind(&LLMarketplaceData::updateSLMListingCoro, this,
							  url, folder_id, version_id, is_listed, data));
}

// Notification callback for updateSLMListingCoro()
bool edit_listing_callback(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // yes
	{
		std::string url = notification["payload"]["url"].asString();
		if (!url.empty())
		{
			LLWeb::loadURL(url);
		}
	}
	return false;
}

void LLMarketplaceData::updateSLMListingCoro(const std::string& url,
											 LLUUID expected_folder_id,
											 LLUUID expected_version_id,
											 bool expected_listed,
											 const LLSD& data)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("getSLMListingCoro");
	LLSD result = adapter.putJsonAndSuspend(url, data, mHttpOptions,
											mHttpHeaders);

	if (!instanceExists()) return;	// Viewer is being closed down !

	setUpdating(expected_folder_id, false);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("Marketplace") << "Body: " << result << LL_ENDL;

    	for (LLSD::array_iterator it = result["listings"].beginArray(),
								  end = result["listings"].endArray();
			 it != end; ++it)
		{
			const LLSD& listing = *it;
			S32 listing_id = listing["id"].asInteger();
			bool is_listed = listing["is_listed"].asBoolean();
			std::string edit_url = listing["edit_url"].asString();
			LLUUID folder_id = listing["inventory_info"]["listing_folder_id"].asUUID();
			LLUUID version_id = listing["inventory_info"]["version_folder_id"].asUUID();
			S32 count = listing["inventory_info"]["count_on_hand"].asInteger();

			// Update that listing
			setListingID(folder_id, listing_id, false);
			setVersionFolderID(folder_id, version_id, false);
			setActivationState(folder_id, is_listed, false);
			setListingURL(folder_id, edit_url, false);
			setCountOnHand(folder_id, count, false);
			LLMarketplace::updateCategory(folder_id, false);
			gInventory.notifyObservers();

			// Show a notification alert if what we got is not what we expected
			// (this actually does not result in an error status from the SLM
			// API protocol)
			if (is_listed != expected_listed ||
				version_id != expected_version_id)
			{
				LLSD subs;
				LLViewerInventoryCategory* cat;
				cat = gInventory.getCategory(folder_id);
				if (cat)
				{
					subs["NAME"] = cat->getName();
				}
				else
				{
					subs["NAME"] = folder_id.asString();
				}
				LLSD payload;
				payload["url"] = edit_url;
				gNotifications.add("AlertMerchantListingNotUpdated", subs,
								   payload, edit_listing_callback);
			}
		}
	}
	else
	{
		log_SLM_warning("Put listing", status.getType(), status.toString(), "",
						result.asString());
		LLMarketplace::updateCategory(expected_folder_id, false);
		gInventory.notifyObservers();
	}
}

void LLMarketplaceData::associateSLMListing(const LLUUID& folder_id,
											S32 listing_id,
											const LLUUID& version_id,
											const LLUUID& source_folder_id)
{
	std::string url = getSLMConnectURL(llformat("/associate_inventory/%d",
												listing_id));
	if (url.empty()) return;

	// Note: we are assuming that sending unchanged info woould not break
	// anything server side...

	// Build the message
	LLSD inventory_info;
	inventory_info["listing_folder_id"] = folder_id;
	inventory_info["version_folder_id"] = version_id;

	LLSD listing;
	listing["id"] = listing_id;
	listing["inventory_info"] = inventory_info;

	LLSD data;
	data["listing"] = listing;

	// Send request
	LL_DEBUGS("Marketplace") << "Sending resquest: " << url << " - Body:"
							 << data << LL_ENDL;
	// Send request
	setUpdating(folder_id, true);
	setUpdating(source_folder_id, true);
	gCoros.launch("updateSLMListingCoro",
				  boost::bind(&LLMarketplaceData::associateSLMListingCoro,
							  this, url, folder_id, source_folder_id, data));
}

void LLMarketplaceData::associateSLMListingCoro(const std::string& url,
												LLUUID expected_folder_id,
												LLUUID source_folder_id,
												const LLSD& data)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("associateSLMListingCoro");
	LLSD result = adapter.putJsonAndSuspend(url, data, mHttpOptions,
											mHttpHeaders);

	if (!instanceExists()) return;	// Viewer is being closed down !

	setUpdating(expected_folder_id, false);
	setUpdating(source_folder_id, false);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("Marketplace") << "Body: " << result << LL_ENDL;

    	for (LLSD::array_iterator it = result["listings"].beginArray(),
								  end = result["listings"].endArray();
			 it != end; ++it)
		{
			const LLSD& listing = *it;
			S32 listing_id = listing["id"].asInteger();
			bool is_listed = listing["is_listed"].asBoolean();
			std::string edit_url = listing["edit_url"].asString();
			LLUUID folder_id = listing["inventory_info"]["listing_folder_id"].asUUID();
			LLUUID version_id = listing["inventory_info"]["version_folder_id"].asUUID();
			S32 count = listing["inventory_info"]["count_on_hand"].asInteger();

			// Check that the listing ID is not already associated to some
			// other record
			const LLUUID& old_listing = getListingFolder(listing_id);
			if (old_listing.notNull())
			{
				// If it is already used, unlist the old record (we cannot have
				// 2 listings with the same listing ID)
				deleteListing(old_listing);
			}

			// Add the new association
			addListing(folder_id, listing_id, version_id, is_listed, edit_url,
					   count);
			LLMarketplace::updateCategory(folder_id, false);
			gInventory.notifyObservers();

			// The stock count needs to be updated with the new local count now
			updateCountOnHand(folder_id, 1);
		}
	}
	else
	{
		log_SLM_warning("Put associate_inventory", status.getType(),
						status.toString(), "", result.asString());
		LLMarketplace::updateCategory(expected_folder_id, false);
		gInventory.notifyObservers();
	}

	// Always update the source folder so its widget updates
	LLMarketplace::updateCategory(source_folder_id, false);
	gInventory.notifyObservers();
}

void LLMarketplaceData::deleteSLMListing(S32 listing_id)
{
	std::string url = getSLMConnectURL(llformat("/listing/%d", listing_id));
	if (url.empty()) return;

	LLSD headers = LLSD::emptyMap();
	headers[HTTP_OUT_HEADER_ACCEPT] = "application/json";
	headers[HTTP_OUT_HEADER_CONTENT_TYPE] = "application/json";

	// Send request
	const LLUUID& folder_id = getListingFolder(listing_id);
	setUpdating(folder_id, true);
	LL_DEBUGS("Marketplace") << "Sending resquest: " << url << LL_ENDL;
	gCoros.launch("deleteSLMListingCoro",
				  boost::bind(&LLMarketplaceData::deleteSLMListingCoro, this,
							  url, folder_id));
}

void LLMarketplaceData::deleteSLMListingCoro(const std::string& url,
											 LLUUID expected_folder_id)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("deleteSLMListingCoro");
	LLSD result = adapter.deleteJsonAndSuspend(url, mHttpOptions,
											   mHttpHeaders);

	if (!instanceExists()) return;	// Viewer is being closed down !

	setUpdating(expected_folder_id, false);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		LL_DEBUGS("Marketplace") << "Body: " << result << LL_ENDL;

    	for (LLSD::array_iterator it = result["listings"].beginArray(),
								  end = result["listings"].endArray();
			 it != end; ++it)
		{
			const LLSD& listing = *it;
			S32 listing_id = listing["id"].asInteger();
			const LLUUID& folder_id = getListingFolder(listing_id);
			deleteListing(folder_id);
		}
	}
	else
	{
		log_SLM_warning("Delete listing", status.getType(), status.toString(),
						"", result.asString());
		LLMarketplace::updateCategory(expected_folder_id, false);
		gInventory.notifyObservers();
	}
}

std::string LLMarketplaceData::getSLMConnectURL(const std::string& route)
{
	std::string url = gAgent.getRegionCapability("DirectDelivery");
	if (!url.empty())
	{
		url += route;
	}
	return url;
}

void LLMarketplaceData::setSLMStatus(S32 status)
{
	if (mMarketPlaceStatus != status)
	{
		mMarketPlaceStatus = status;
		if (mStatusUpdatedSignal)
		{
			(*mStatusUpdatedSignal)();
		}
	}
}

// Creation / Deletion / Update
// Methods publicly called
bool LLMarketplaceData::createListing(const LLUUID& folder_id)
{
	if (isListed(folder_id))
	{
		// Listing already exists -> exit with error
		return false;
	}

	const LLUUID& version_id = getVersionFolderIfUnique(folder_id);
	S32 count = version_id.isNull() ? COMPUTE_STOCK_INFINITE
									: LLMarketplace::computeStockCount(version_id,
																	   true);
	// Validate the count on hand
	if (count == COMPUTE_STOCK_NOT_EVALUATED)
	{
		// If the count on hand cannot be evaluated, we will consider it empty
		// (out of stock) at creation time. It will get reevaluated and updated
		// once the items are fetched.
		count = 0;
	}

	// Post the listing creation request to SLM
	createSLMListing(folder_id, version_id, count);

	return true;
}

bool LLMarketplaceData::clearListing(const LLUUID& folder_id, S32 depth)
{
	if (folder_id.isNull())
	{
		// Folder does not exist -> exit with error
		return false;
	}

	// Folder id can be the root of the listing or not so we need to retrieve
	// the root first
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(folder_id);
	}
	const LLUUID& listing_uuid =
		isListed(folder_id) ? folder_id
							: LLMarketplace::nestedParentId(folder_id, depth);
	S32 listing_id = getListingID(listing_uuid);
	if (listing_id == 0)
	{
		// Listing does not exist -> exit with error
		return false;
	}

	// Update the SLM Server so that this listing is deleted (actually,
	// archived...)
	deleteSLMListing(listing_id);

	return true;
}

bool LLMarketplaceData::getListing(const LLUUID& folder_id, S32 depth)
{
	if (folder_id.isNull())
	{
		// Folder does not exist -> exit with error
		return false;
	}

	// Folder id can be the root of the listing or not so we need to retrieve
	// the root first
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(folder_id);
	}
	const LLUUID& listing_uuid =
		isListed(folder_id) ? folder_id
							: LLMarketplace::nestedParentId(folder_id, depth);
	S32 listing_id = getListingID(listing_uuid);
	if (listing_id == 0)
	{
		// Listing does not exist -> exit with error
		return false;
	}

	// Get listing data from SLM
	getSLMListing(listing_id);

	return true;
}

bool LLMarketplaceData::getListing(S32 listing_id)
{
	if (listing_id == 0)
	{
		return false;
	}

	// Get listing data from SLM
	getSLMListing(listing_id);
	return true;
}

bool LLMarketplaceData::activateListing(const LLUUID& folder_id, bool activate,
										S32 depth)
{
	// Folder id can be the root of the listing or not so we need to retrieve
	// the root first
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(folder_id);
	}
	const LLUUID& listing_uuid = LLMarketplace::nestedParentId(folder_id,
															   depth);
	S32 listing_id = getListingID(listing_uuid);
	if (listing_id == 0)
	{
		// Listing does not exist -> exit with error
		return false;
	}

	if (getActivationState(listing_uuid) == activate)
	{
		// If activation state is unchanged, no point spamming SLM with an
		// update
		return true;
	}

	const LLUUID& version_uuid = getVersionFolder(listing_uuid);

	// Also update the count on hand
	S32 count = LLMarketplace::computeStockCount(folder_id);
	if (count == COMPUTE_STOCK_NOT_EVALUATED)
	{
		// If the count on hand cannot be evaluated locally, we should not
		// change that SLM value. We are assuming that this issue is local and
		// should not modify server side values.
		count = getCountOnHand(listing_uuid);
	}

	// Post the listing update request to SLM
	updateSLMListing(listing_uuid, listing_id, version_uuid, activate, count);

	return true;
}

bool LLMarketplaceData::setVersionFolder(const LLUUID& folder_id,
										 const LLUUID& version_id, S32 depth)
{
	// Folder id can be the root of the listing or not so we need to retrieve
	// the root first
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(folder_id);
	}
	const LLUUID& listing_uuid = LLMarketplace::nestedParentId(folder_id,
															   depth);
	S32 listing_id = getListingID(listing_uuid);
	if (listing_id == 0)
	{
		// Listing does not exist -> exit with error
		return false;
	}

	if (getVersionFolder(listing_uuid) == version_id)
	{
		// If version folder is unchanged, no point spamming SLM with an update
		return true;
	}

	// Note: if the version_id is cleared, we need to unlist the listing,
	// otherwise, state unchanged
	bool is_listed = version_id.isNull() ? false
										 : getActivationState(listing_uuid);

	// Also update the count on hand
	S32 count = LLMarketplace::computeStockCount(version_id);
	if (count == COMPUTE_STOCK_NOT_EVALUATED)
	{
		// If the count on hand cannot be evaluated, we will consider it empty
		// (out of stock) at creation time. It will get reevaluated and updated
		// once the items are fetched.
		count = 0;
	}

	// Post the listing update request to SLM
	updateSLMListing(listing_uuid, listing_id, version_id, is_listed, count);

	return true;
}

bool LLMarketplaceData::updateCountOnHand(const LLUUID& folder_id, S32 depth)
{
	// Folder id can be the root of the listing or not so we need to retrieve
	// the root first
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(folder_id);
	}
	const LLUUID& listing_uuid = LLMarketplace::nestedParentId(folder_id,
															   depth);
	S32 listing_id = getListingID(listing_uuid);
	if (listing_id == 0)
	{
		// Listing does not exist -> exit with error
		return false;
	}

	// Compute the new count on hand
	S32 count = LLMarketplace::computeStockCount(folder_id);
	if (count == getCountOnHand(listing_uuid))
	{
		// If count on hand is unchanged, no point spamming SLM with an update
		return true;
	}
	if (count == COMPUTE_STOCK_NOT_EVALUATED)
	{
		// If local count on hand is not known at that point, do *not* force an
		// update to SLM
		return false;
	}

	// Get the unchanged values
	bool is_listed = getActivationState(listing_uuid);
	const LLUUID& version_uuid = getVersionFolder(listing_uuid);

	// Post the listing update request to SLM
	updateSLMListing(listing_uuid, listing_id, version_uuid, is_listed, count);

	// Force the local value as it prevents spamming (count update may occur in
	// burst when restocking). Note that if SLM has a good reason to return a
	// different value, it'll be updated by the responder
	setCountOnHand(listing_uuid, count, false);

	return true;
}

bool LLMarketplaceData::associateListing(const LLUUID& folder_id,
										 const LLUUID& source_folder_id,
										 S32 listing_id)
{
	if (isListed(folder_id))
	{
		// Listing already exists -> exit with error
		return false;
	}

	// Get the version folder: if there is only one subfolder, we will set it
	// as a version folder immediately
	const LLUUID& version_id = getVersionFolderIfUnique(folder_id);

	// Post the listing update request to SLM
	associateSLMListing(folder_id, listing_id, version_id, source_folder_id);

	return true;
}

// Methods privately called or called by SLM responders to perform changes
bool LLMarketplaceData::addListing(const LLUUID& folder_id, S32 listing_id,
								   const LLUUID& version_id, bool is_listed,
								   const std::string& edit_url, S32 count)
{
	mMarketplaceItems[folder_id] = LLMarketplaceTuple(folder_id, listing_id,
													  version_id, is_listed,
													  edit_url, count);
	if (version_id.notNull())
	{
		mVersionFolders[version_id] = folder_id;
	}

	return true;
}

bool LLMarketplaceData::deleteListing(const LLUUID& folder_id, bool update)
{
	if (mMarketplaceItems.erase(folder_id) != 1)
	{
		return false;
	}

	const LLUUID& vf_uuid = getVersionFolder(folder_id);
	if (vf_uuid.notNull())
	{
		mVersionFolders.erase(vf_uuid);
	}

	if (update)
	{
		LLMarketplace::updateCategory(folder_id, false);
		gInventory.notifyObservers();
	}

	return true;
}

bool LLMarketplaceData::deleteListing(S32 listing_id, bool update)
{
	if (listing_id == 0)
	{
		return false;
	}

	LLUUID folder_id = getListingFolder(listing_id);
	return deleteListing(folder_id, update);
}

// Accessors
bool LLMarketplaceData::getActivationState(const LLUUID& folder_id)
{
	// Listing folder case
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	if (it != mMarketplaceItems.end())
	{
		return (it->second).mIsActive;
	}

	// Version folder case
	version_folders_list_t::iterator vit = mVersionFolders.find(folder_id);
	if (vit != mVersionFolders.end())
	{
		it = mMarketplaceItems.find(vit->second);
		if (it != mMarketplaceItems.end())
		{
			return (it->second).mIsActive;
		}
	}

	return false;
}

S32 LLMarketplaceData::getListingID(const LLUUID& folder_id)
{
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	return it == mMarketplaceItems.end() ? 0 : (it->second).mListingId;
}

S32 LLMarketplaceData::getCountOnHand(const LLUUID& folder_id)
{
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	return it == mMarketplaceItems.end() ? -1 : (it->second).mCountOnHand;
}

LLUUID LLMarketplaceData::getVersionFolder(const LLUUID& folder_id)
{
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	return it == mMarketplaceItems.end() ? LLUUID::null
										 : (it->second).mVersionFolderId;
}

// Reverse lookup : find the listing folder id from the listing id
LLUUID LLMarketplaceData::getListingFolder(S32 listing_id)
{
	for (marketplace_items_list_t::iterator it = mMarketplaceItems.begin(),
											end = mMarketplaceItems.end();
		 it != end; ++it)
	{
		if ((it->second).mListingId == listing_id)
		{
			return (it->second).mListingFolderId;
		}
	}
	return LLUUID::null;
}

std::string LLMarketplaceData::getListingURL(const LLUUID& folder_id,
											 S32 depth)
{
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(folder_id);
	}
	const LLUUID& listing_uuid = LLMarketplace::nestedParentId(folder_id,
															   depth);
	marketplace_items_list_t::iterator it =
		mMarketplaceItems.find(listing_uuid);
	return it == mMarketplaceItems.end() ? "" : (it->second).mEditURL;
}

bool LLMarketplaceData::isListed(const LLUUID& folder_id)
{
	return mMarketplaceItems.count(folder_id) != 0;
}

bool LLMarketplaceData::isListedAndActive(const LLUUID& folder_id)
{
	return isListed(folder_id) && getActivationState(folder_id);
}

bool LLMarketplaceData::isVersionFolder(const LLUUID& folder_id)
{
	return mVersionFolders.count(folder_id) != 0;
}

bool LLMarketplaceData::isInActiveFolder(const LLUUID& obj_id, S32 depth)
{
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(obj_id);
	}
	const LLUUID& listing_uuid = LLMarketplace::nestedParentId(obj_id, depth);
	bool active = getActivationState(listing_uuid);
	if (!active)
	{
		return false;
	}

	const LLUUID& version_uuid = getVersionFolder(listing_uuid);
	return obj_id == version_uuid ||
		   gInventory.isObjectDescendentOf(obj_id, version_uuid);
}

LLUUID LLMarketplaceData::getActiveFolder(const LLUUID& obj_id, S32 depth)
{
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(obj_id);
	}
	const LLUUID& listing_uuid = LLMarketplace::nestedParentId(obj_id, depth);
	return getActivationState(listing_uuid) ? getVersionFolder(listing_uuid)
											: LLUUID::null;
}

bool LLMarketplaceData::isUpdating(const LLUUID& folder_id, S32 depth)
{
	if (depth < 0)
	{
		depth = LLMarketplace::depthNesting(folder_id);
	}
	if (depth < 0)
	{
		// Not a Marketplace folder
		return false;
	}

	if (depth == 0 &&
		getSLMStatus() <= MarketplaceStatusCodes::MARKET_PLACE_INITIALIZING)
	{
		// If the Marketplace is not yet initialized, then yes, we are
		// definitely updating...
		return true;
	}

	const LLUUID& market_id = LLMarketplace::getMPL();
	if (mPendingUpdateSet.find(market_id) != mPendingUpdateSet.end())
	{
		// If we are waiting for data for the marketplace listings root, we are
		// in the updating process for all
		return true;
	}

#if 0	// Stock folders too...
	if (depth > 2)
	{
		// Only listing and version folders though are concerned by that status
		return false;
	}
#endif

	// Check if the listing folder is waiting or data
	const LLUUID& listing_uuid = LLMarketplace::nestedParentId(folder_id,
															   depth);
	return mPendingUpdateSet.find(listing_uuid) != mPendingUpdateSet.end();
}

void LLMarketplaceData::setUpdating(const LLUUID& folder_id, bool is_updating)
{
	uuid_list_t::iterator it = mPendingUpdateSet.find(folder_id);
	if (it != mPendingUpdateSet.end())
	{
		mPendingUpdateSet.erase(it);
	}
	if (is_updating)
	{
		mPendingUpdateSet.emplace(folder_id);
	}
}

void LLMarketplaceData::listForIdleValidation(const LLUUID& folder_id)
{
	mPendingValidations.emplace(folder_id);
}

void LLMarketplaceData::setValidationWaiting(const LLUUID& folder_id,
											 S32 count)
{
	mValidationWaitingList[folder_id] = count;
}

void LLMarketplaceData::decrementValidationWaiting(const LLUUID& folder_id,
												   S32 count)
{
	waiting_list_t::iterator it = mValidationWaitingList.find(folder_id);
	if (it != mValidationWaitingList.end())
	{
		it->second -= count;
		if (it->second <= 0)
		{
			mValidationWaitingList.hmap_erase(it);
			mPendingValidations.emplace(folder_id);
		}
	}
}

//static
void LLMarketplaceData::idleCallback(void* userdata)
{
	LLMarketplaceData* self = (LLMarketplaceData*)userdata;

	if (!self || self->mPendingValidations.empty()) return;

	for (uuid_list_t::const_iterator it = self->mPendingValidations.begin(),
									 end = self->mPendingValidations.end();
		 it != end; ++it)
	{
		LLViewerInventoryCategory* cat = gInventory.getCategory(*it);
		if (cat)
		{
			LLMarketplace::validateListings(cat);
		}
	}

	self->mPendingValidations.clear();
}

// Private Modifiers
bool LLMarketplaceData::setListingID(const LLUUID& folder_id, S32 listing_id,
									 bool update)
{
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	if (it == mMarketplaceItems.end())
	{
		return false;
	}

	it->second.mListingId = listing_id;

	if (update)
	{
		LLMarketplace::updateCategory(folder_id, false);
		gInventory.notifyObservers();
	}

	return true;
}

bool LLMarketplaceData::setCountOnHand(const LLUUID& folder_id, S32 count,
									   bool update)
{
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	if (it == mMarketplaceItems.end())
	{
		return false;
	}

	it->second.mCountOnHand = count;

	return true;
}

bool LLMarketplaceData::setVersionFolderID(const LLUUID& folder_id,
										   const LLUUID& version_id,
										   bool update)
{
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	if (it == mMarketplaceItems.end())
	{
		return false;
	}

	// Note: do not use LLUUID& here since we need an actual copy of the old
	// UUID, not a pointer on (it->second).mVersionFolderId.
	LLUUID old_version_id = (it->second).mVersionFolderId;
	if (version_id == old_version_id)
	{
		return false;
	}
	it->second.mVersionFolderId = version_id;

	bool update_old = false;
	if (old_version_id.notNull())
	{
		mVersionFolders.erase(old_version_id);
		update_old = update;
	}

	bool update_new = false;
	if (version_id.notNull())
	{
		mVersionFolders[version_id] = folder_id;
		update_new = update;
	}

	// Now that the version folder has been changed, we can update the folders
	// hierarchy if needed.
	if (update_old)
	{
		LLMarketplace::updateCategory(old_version_id, false);
	}
	if (update_new)
	{
		LLMarketplace::updateCategory(version_id, false);
	}
	if (update_old || update_new)
	{
		gInventory.notifyObservers();
	}

	return true;
}

bool LLMarketplaceData::setActivationState(const LLUUID& folder_id,
										   bool activate, bool update)
{
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	if (it == mMarketplaceItems.end())
	{
		return false;
	}

	it->second.mIsActive = activate;

	if (update)
	{
		LLMarketplace::updateCategory((it->second).mListingFolderId, false);
		gInventory.notifyObservers();
	}

	return true;
}

bool LLMarketplaceData::setListingURL(const LLUUID& folder_id,
									  const std::string& edit_url, bool update)
{
	marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
	if (it == mMarketplaceItems.end())
	{
		return false;
	}

	it->second.mEditURL = edit_url;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// New Marketplace Listings API related functions

// Local helper
bool can_move_to_marketplace(LLViewerInventoryItem* inv_item,
							 std::string& tooltip_msg,
							 bool resolve_links = false)
{
	if (!inv_item)
	{
		tooltip_msg = "NULL inventory item";
		return false;
	}

	LLViewerInventoryItem* vitem = inv_item;
	LLViewerInventoryItem* linked_item = vitem->getLinkedItem();
	LLViewerInventoryCategory* linked_category = vitem->getLinkedCategory();
	if (resolve_links)
	{
		if (linked_item)
		{
			vitem = linked_item;
			linked_item = NULL; // Link resolved, so allow to pass next test
		}
		else if (linked_category)
		{
			vitem = (LLViewerInventoryItem*)linked_category;
			// Link resolved, so allow to pass next test
			linked_category = NULL;
		}
	}

	// Linked items and folders cannot be put for sale
	if (linked_category || linked_item)
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxLinked");
		return false;
	}

	const LLUUID& item_uuid = vitem->getUUID();
	// Check library status: library items cannot be put on the marketplace
	if (!gInventory.isObjectDescendentOf(item_uuid,
										 gInventory.getRootFolderID()))
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxNotInInventory");
		return false;
	}

	// Check type
	S32 type = vitem->getType();
	// A category is always considered as passing...
	if (type == LLAssetType::AT_CATEGORY)
	{
		return true;
	}
	// For the moment, calling cards cannot be put on the marketplace
	if (type == LLAssetType::AT_CALLINGCARD)
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxCallingCard");
		return false;
	}

	// Check that the agent has transfer permission on the item: this is
	// required as a resident cannot put on sale items they cannot transfer.
	// Proceed with move if we have permission.
	if (!vitem->getPermissions().allowTransferBy(gAgentID))
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxNoTransfer");
		return false;
	}

	// Check worn/not worn status: worn items cannot be put on the marketplace
	if (get_is_item_worn(item_uuid))
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxWorn");
		return false;
	}

	return true;
}

// Local helper
// Counts only the copyable items, i.e. skip the stock items (which are no
// copy)
S32 count_copyable_items(const LLInventoryModel::item_array_t& items)
{
	S32 count = 0;

	const LLUUID& group_id = gAgent.getGroupID();
	for (LLInventoryModel::item_array_t::const_iterator it = items.begin(),
														end = items.end();
		 it != end; ++it)
	{
		LLViewerInventoryItem* itemp = *it;
		if (itemp && itemp->getPermissions().allowCopyBy(gAgentID, group_id))
		{
			++count;
		}
	}

	return count;
}

// Local helper
// Count only the non-copyable items, i.e. the stock items, skip the others
S32 count_stock_items(const LLInventoryModel::item_array_t& items)
{
	S32 count = 0;

	const LLUUID& group_id = gAgent.getGroupID();
	for (LLInventoryModel::item_array_t::const_iterator it = items.begin(),
														end = items.end();
		 it != end; ++it)
	{
		LLViewerInventoryItem* itemp = *it;
		if (itemp && !itemp->getPermissions().allowCopyBy(gAgentID, group_id))
		{
			++count;
		}
	}

	return count;
}

// Local helper
// Counts the number of stock folders
S32 count_stock_folders(const LLInventoryModel::cat_array_t& cats)
{
	S32 count = 0;
	for (LLInventoryModel::cat_array_t::const_iterator it = cats.begin(),
													   end = cats.end();
		 it != end; ++it)
	{
		LLViewerInventoryCategory* cat = *it;
		if (cat &&
			cat->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
		{
			++count;
		}
	}
	return count;
}

//static
bool LLMarketplace::contains(const LLUUID& item_id)
{
	return sMarketplaceListingId.notNull() &&
		   gInventory.isObjectDescendentOf(item_id, sMarketplaceListingId);
}

// Get the marketplace listings root, exit with -1 (i.e. not under the
// marketplace listings root) if none
//static
S32 LLMarketplace::depthNesting(const LLUUID& item_id)
{
	if (sMarketplaceListingId.isNull() ||
		!gInventory.isObjectDescendentOf(item_id, sMarketplaceListingId))
	{
		return -1;
	}

	// Iterate through the parents till we hit the marketplace listings root
	// Note that the marketplace listings root itself will return 0
	S32 depth = 0;
	LLInventoryObject* cur_object = gInventory.getObject(item_id);
	if (cur_object)
	{
		LLUUID cur_uuid(item_id);
		while (cur_uuid != sMarketplaceListingId)
		{
			++depth;
			cur_uuid = cur_object->getParentUUID();
			cur_object = gInventory.getCategory(cur_uuid);
			if  (!cur_object)
			{
				return -1;
			}
		}
	}
	return depth;
}

// Returns the UUID of the marketplace listing this object is in
//static
LLUUID LLMarketplace::nestedParentId(const LLUUID& item_id, S32 depth)
{
	if (depth < 1)
	{
		// For objects outside the marketplace listings root (or root itself),
		// we return a NULL UUID
		return LLUUID::null;
	}
	else if (depth == 1)
	{
		// Just under the root, we return the passed UUID itself if it's a
		// folder, NULL otherwise (not a listing)
		LLViewerInventoryCategory* cat = gInventory.getCategory(item_id);
		return cat ? item_id : LLUUID::null;
	}

	// depth > 1
	LLInventoryObject* cur_object = gInventory.getObject(item_id);
	LLUUID cur_uuid(item_id);
	while (cur_object && depth-- > 1)
	{
		cur_uuid = cur_object->getParentUUID();
		cur_object = gInventory.getCategory(cur_uuid);
	}
	return cur_uuid;
}

//static
S32 LLMarketplace::computeStockCount(const LLUUID& cat_id, bool force_count)
{
	// Handle the case of the folder being a stock folder immediately
	LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
	if (!cat)
	{
		// Not a category so no stock count to speak of
		return COMPUTE_STOCK_INFINITE;
	}
	if (cat->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
	{
		if (cat->isVersionUnknown())
		{
			// COMPUTE_STOCK_NOT_EVALUATED denotes that a stock folder has a
			// count that cannot be evaluated at this time (folder not up to
			// date)
			return COMPUTE_STOCK_NOT_EVALUATED;
		}
		// Note: stock folders are *not* supposed to have nested subfolders so
		// we stop recursion here but we count only items (subfolders will be
		// ignored)
		// Note: we *always* give a stock count for stock folders, it's useful
		// even if the listing is unassociated
		LLInventoryModel::cat_array_t* cat_array;
		LLInventoryModel::item_array_t* item_array;
		gInventory.getDirectDescendentsOf(cat_id, cat_array, item_array);
		return item_array ? item_array->size() : COMPUTE_STOCK_NOT_EVALUATED;
	}

	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();

	// When force_count is true, we do not do any verification of the
	// marketplace status and simply compute the stock amount based on the
	// descendent hierarchy. This is used specifically when creating a listing.
	if (!force_count)
	{
		// Grab marketplace data for this folder
		S32 depth = depthNesting(cat_id);
		LLUUID listing_uuid = nestedParentId(cat_id, depth);
		if (!marketdata->isListed(listing_uuid))
		{
			// If not listed, the notion of stock is meaningless so it would
			// not be computed for any level
			return COMPUTE_STOCK_INFINITE;
		}

		const LLUUID& vf_uuid = marketdata->getVersionFolder(listing_uuid);
		// Handle the case of the first 2 levels : listing and version folders
		if (depth == 1)
		{
			if (vf_uuid.notNull())
			{
				// If there is a version folder, the stock value for the
				// listing is the version folder stock
				return computeStockCount(vf_uuid, true);
			}
			else
			{
				// If there's no version folder associated, the notion of stock
				// count has no meaning
				return COMPUTE_STOCK_INFINITE;
			}
		}
		else if (depth == 2)
		{
			if (vf_uuid.notNull() && vf_uuid != cat_id)
			{
				// If there is a version folder but we're not it, our stock
				// count is meaningless
				return COMPUTE_STOCK_INFINITE;
			}
		}
	}

	// In all other cases, the stock count is the min of stock folders count
	// found in the descendents
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat_id, cat_array, item_array);
	if (!cat_array || !item_array)
	{
		llwarns << "Failed to get descendents of: " << cat_id << llendl;
		return COMPUTE_STOCK_INFINITE;
	}

	// COMPUTE_STOCK_INFINITE denotes a folder that does not contain any stock
	// folder in its descendents
	S32 curr_count = COMPUTE_STOCK_INFINITE;

	// Note: marketplace listings have a maximum depth nesting of 4
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(),
												 end = cat_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryCategory* category = *iter;
		if (!category) continue;	// Paranoia

		S32 count = computeStockCount(category->getUUID(), true);
		if (curr_count == COMPUTE_STOCK_INFINITE ||
			(count != COMPUTE_STOCK_INFINITE && count < curr_count))
		{
			curr_count = count;
		}
	}

	return curr_count;
}

//static
bool LLMarketplace::processUpdateNotification(const LLSD& data)
{
	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	S32 listing_id = data["listing_id"].asInteger();
	std::string state = data["state"].asString();
	if (state == "deleted")
	{
		// Perform the deletion viewer side, no alert shown in this case
		marketdata->deleteListing(listing_id);
		return true;
	}
	else
	{
		// In general, no message will be displayed, all we want is to get the
		// listing updated in the inventory. If getListing() fails though, the
		// message of the alert will be shown by the caller
		return marketdata->getListing(listing_id);
	}
}

//static
bool LLMarketplace::updateIfListed(const LLUUID& folder_id,
								   const LLUUID& parent_id)
{
	S32 depth = LLMarketplace::depthNesting(folder_id);
	if (depth == 1 || depth == 2)
	{
		// Trigger an SLM listing update
		LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
		S32 listing_id = depth == 1 ? marketdata->getListingID(folder_id)
									: marketdata->getListingID(parent_id);
		marketdata->getListing(listing_id);
		return true;
	}
	return false;
}

//static
void LLMarketplace::inventoryContextMenu(LLFolderBridge* folder,
										 const LLUUID& id, U32 flags,
										 std::vector<std::string>& items,
										 std::vector<std::string>& disabled_items)
{
	if (!folder)
	{
		llwarns << "NULL folder bridge !" << llendl;
		llassert(false);
		return;
	}

	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	U32 status = marketdata->getSLMStatus();
	if (status != MarketplaceStatusCodes::MARKET_PLACE_MERCHANT &&
		status != MarketplaceStatusCodes::MARKET_PLACE_MIGRATED_MERCHANT)
	{
		// Disable everything that could harm the Marketplace listings while
		// we are not connected.
		disabled_items.emplace_back("Rename");
		disabled_items.emplace_back("Cut");
		disabled_items.emplace_back("Paste");
		disabled_items.emplace_back("Delete");
		if ((status == MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE ||
			 status == MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED))
		{
			items.emplace_back("Marketplace Connect");
		}
		return;
	}

	S32 depth = depthNesting(id);
	bool is_updating = marketdata->isUpdating(id, depth);

	// Non Marketplace-specific entries

	if (depth > 0 &&
		folder->getPreferredType() != LLFolderType::FT_MARKETPLACE_STOCK)
	{
		items.emplace_back("New Folder");
		if (is_updating)
		{
			disabled_items.emplace_back("New Folder");
		}
		else if (depth >= 2)
		{
			// Prevent creation of new folders if the max count has been
			// reached on this version folder (active or not)
			const LLUUID& local_listing_id = nestedParentId(id, depth - 1);
			LLInventoryModel::cat_array_t categories;
			LLInventoryModel::item_array_t items;
			gInventory.collectDescendents(local_listing_id, categories, items,
										  false);
			U32 max_count = gSavedSettings.getU32("InventoryOutboxMaxFolderCount");
			if (categories.size() >= max_count)
			{
				disabled_items.emplace_back("New Folder");
			}
		}
	}

	if (is_updating)
	{
		disabled_items.emplace_back("Rename");
		disabled_items.emplace_back("Cut");
		disabled_items.emplace_back("Copy");
		disabled_items.emplace_back("Paste");
		disabled_items.emplace_back("Delete");
	}

	// Marketplace-specific entries

	items.emplace_back("Marketplace Separator");

	if (depth == 0)
	{
		items.emplace_back("Marketplace Check Listing");
	}
	else if (depth == 1)
	{
		// Options available at the Listing Folder level
		items.emplace_back("Marketplace Create Listing");
		items.emplace_back("Marketplace Associate Listing");
		items.emplace_back("Marketplace Check Listing");
		items.emplace_back("Marketplace List");
		items.emplace_back("Marketplace Unlist");
		if (is_updating || (flags & FIRST_SELECTED_ITEM) == 0)
		{
			// During SLM update, disable all marketplace related options
			// Also disable all if multiple selected items
			disabled_items.emplace_back("Marketplace Create Listing");
			disabled_items.emplace_back("Marketplace Associate Listing");
			disabled_items.emplace_back("Marketplace Check Listing");
			disabled_items.emplace_back("Marketplace List");
			disabled_items.emplace_back("Marketplace Unlist");
		}
		else
		{
			bool listing_logging = false;
			LL_DEBUGS("Marketplace") << "Adding 'Get/refresh listing' for debug purpose";
			listing_logging = true;
			LL_CONT << LL_ENDL;
			if (listing_logging)
			{
				items.emplace_back("Marketplace Get Listing");
			}

			if (marketdata->isListed(id))
			{
				disabled_items.emplace_back("Marketplace Create Listing");
				disabled_items.emplace_back("Marketplace Associate Listing");
				if (marketdata->getVersionFolder(id).isNull())
				{
					disabled_items.emplace_back("Marketplace List");
					disabled_items.emplace_back("Marketplace Unlist");
				}
				else
				{
					if (marketdata->getActivationState(id))
					{
						disabled_items.emplace_back("Marketplace List");
					}
					else
					{
						disabled_items.emplace_back("Marketplace Unlist");
					}
				}
			}
			else
			{
				disabled_items.emplace_back("Marketplace List");
				disabled_items.emplace_back("Marketplace Unlist");
				if (listing_logging)
				{
					disabled_items.emplace_back("Marketplace Get Listing");
				}
			}
		}
	}
	else if (depth == 2)
	{
		// Options available at the Version Folder levels and only for folders
		LLViewerInventoryCategory* cat = gInventory.getCategory(id);
		if (cat && marketdata->isListed(cat->getParentUUID()))
		{
			items.emplace_back("Marketplace Activate");
			items.emplace_back("Marketplace Deactivate");
			if (is_updating || (flags & FIRST_SELECTED_ITEM) == 0)
			{
				// During SLM update, disable all marketplace related options
				// Also disable all if multiple selected items
				disabled_items.emplace_back("Marketplace Activate");
				disabled_items.emplace_back("Marketplace Deactivate");
			}
			else
			{
				if (marketdata->isVersionFolder(id))
				{
					disabled_items.emplace_back("Marketplace Activate");
					if (marketdata->getActivationState(id))
					{
						disabled_items.emplace_back("Marketplace Deactivate");
					}
				}
				else
				{
					disabled_items.emplace_back("Marketplace Deactivate");
				}
			}
		}
	}

	if (depth > 0)
	{
		// Options available at all sub-levels on items and categories
		items.emplace_back("Marketplace Edit Listing");
		const LLUUID& listing_id = nestedParentId(id, depth);
		const LLUUID& version_id = marketdata->getVersionFolder(listing_id);
		if (version_id.isNull() || !marketdata->isListed(listing_id))
		{
			disabled_items.emplace_back("Marketplace Edit Listing");
		}
	}
}

//static
std::string LLMarketplace::rootFolderLabelSuffix()
{
	std::string suffix;

	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	switch (marketdata->getSLMStatus())
	{
		case MarketplaceStatusCodes::MARKET_PLACE_INITIALIZING:
			suffix = LLTrans::getString("MarketplaceInitializing");
			break;

		case MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE:
			suffix = LLTrans::getString("MarketplaceFailure");
			break;

		case MarketplaceStatusCodes::MARKET_PLACE_MERCHANT:
		case MarketplaceStatusCodes::MARKET_PLACE_MIGRATED_MERCHANT:
		{
			switch (marketdata->getSLMDataFetched())
			{
				case MarketplaceFetchCodes::MARKET_FETCH_NOT_DONE:
				case MarketplaceFetchCodes::MARKET_FETCH_LOADING:
					suffix = LLTrans::getString("MarketplaceFetching");
					break;

				case MarketplaceFetchCodes::MARKET_FETCH_FAILED:
					suffix = LLTrans::getString("MarketplaceFetchFailed");
					break;

				case MarketplaceFetchCodes::MARKET_FETCH_DONE:
				default:
					suffix = LLTrans::getString("MarketplaceMerchant");
			}
			break;
		}

		case MarketplaceStatusCodes::MARKET_PLACE_NOT_MERCHANT:
			suffix = LLTrans::getString("MarketplaceNotMerchant");
			break;

		case MarketplaceStatusCodes::MARKET_PLACE_NOT_MIGRATED_MERCHANT:
			suffix = LLTrans::getString("MarketplaceNotMigrated");

		default:
			break;
	}

	if (!suffix.empty())
	{
		suffix = " (" + suffix + ")";
	}

	return suffix;
}

//static
std::string LLMarketplace::folderLabelSuffix(const LLUUID& cat_id)
{
	std::string suffix;
	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	U32 status = marketdata->getSLMStatus();
	if (status != MarketplaceStatusCodes::MARKET_PLACE_MERCHANT &&
		status != MarketplaceStatusCodes::MARKET_PLACE_MIGRATED_MERCHANT)
	{
		return suffix;
	}
	if (marketdata->isUpdating(cat_id))
	{
		// Skip expensive computations if we are waiting for an update
		suffix = LLTrans::getString("MarketplaceUpdating");
	}
	else
	{
		if (marketdata->isListed(cat_id))				// Listing folder case
		{
			S32 id = marketdata->getListingID(cat_id);
			if (id)
			{
				suffix = llformat("%d", id);
			}
			else
			{
				suffix = LLTrans::getString("MarketplaceNoID");
			}
			if (marketdata->getActivationState(cat_id))
			{
				suffix += " - " + LLTrans::getString("MarketplaceLive");
			}
		}
		else if (marketdata->isVersionFolder(cat_id))	// Version folder case
		{
			suffix = LLTrans::getString("MarketplaceActive");
		}

		// Add stock amount
		S32 stock_count = computeStockCount(cat_id);
		if (stock_count == COMPUTE_STOCK_NOT_EVALUATED)
		{
			// Add updating suffix
			if (!suffix.empty())
			{
				suffix += " - ";
			}
			suffix += LLTrans::getString("MarketplaceUpdating");
		}
		else if (stock_count == 0)
		{
			if (!suffix.empty())
			{
				suffix += " - ";
			}
			suffix += LLTrans::getString("MarketplaceNoStock");
		}
		else if (stock_count > 0)
		{
			if (!suffix.empty())
			{
				suffix += " - ";
			}
			LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
			if (cat &&
				cat->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
			{
				suffix += LLTrans::getString("MarketplaceStock") + "=" +
						  llformat("%d", stock_count);
			}
			else
			{
				suffix += LLTrans::getString("MarketplaceMax") + "=" +
						  llformat("%d", stock_count);
			}
		}
	}

	if (!suffix.empty())
	{
		suffix = " (" + suffix + ")";
	}

	return suffix;
}

//static
bool LLMarketplace::isFolderActive(const LLUUID& cat_id)
{
	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	U32 status = marketdata->getSLMStatus();
	return (status == MarketplaceStatusCodes::MARKET_PLACE_MIGRATED_MERCHANT ||
			status == MarketplaceStatusCodes::MARKET_PLACE_MERCHANT) &&
		   marketdata->getActivationState(cat_id);
}

//static
void LLMarketplace::getListing(const LLUUID& folder_id)
{
	LLMarketplaceData::getInstance()->getListing(folder_id);
}

//static
void LLMarketplace::createListing(const LLUUID& folder_id)
{
	LLViewerInventoryCategory* cat = gInventory.getCategory(folder_id);
	sMessage.clear();
	bool valid = validateListings(cat,
								  boost::bind(&LLMarketplace::gatherMessage,
											  _1, _2, _3),
								  false);
	if (!valid)
	{
		sMessage.clear();
		valid = validateListings(cat,
								 boost::bind(&LLMarketplace::gatherMessage,
											 _1, _2, _3));
		if (valid)
		{
			gNotifications.add("MerchantForceValidateListing");
		}
	}

	if (valid)
	{
		LLMarketplaceData::getInstance()->createListing(folder_id);
	}
	else
	{
		LLSD subs;
		subs["ERROR_CODE"] = sMessage;
		gNotifications.add("MerchantListingFailed", subs);
	}
}

//static
void LLMarketplace::clearListing(const LLUUID& folder_id)
{
	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	if (marketdata->isListed(folder_id))
	{
		marketdata->clearListing(folder_id);
	}
}

//static
void LLMarketplace::editListing(const LLUUID& folder_id)
{
	std::string url =
		LLMarketplaceData::getInstance()->getListingURL(folder_id);
	if (!url.empty())
	{
		LLWeb::loadURL(url);
	}
}

//static
void LLMarketplace::gatherMessage(std::string& message, S32 depth,
								  LLError::ELevel log_level)
{
	if (log_level > LLError::LEVEL_WARN && !sMessage.empty())
	{
		// Currently, we do not gather all messages as it creates very long
		// alerts. Users can get to the whole list of errors on a listing using
		// the "Check listing" right click menu
		return;
	}
	// Take the leading spaces out...
	std::string::size_type start = message.find_first_not_of(' ');
	// Append the message
	sMessage += message.substr(start, message.length() - start);
}

//static
void LLMarketplace::listFolder(const LLUUID& folder_id, bool list)
{
	if (depthNesting(folder_id) == 1)
	{
		LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
		if (list)
		{
			const LLUUID& version_id = marketdata->getVersionFolder(folder_id);
			LLViewerInventoryCategory* cat = gInventory.getCategory(version_id);
			sMessage.clear();
			if (!validateListings(cat,
								  boost::bind(&LLMarketplace::gatherMessage,
											  _1, _2, _3)))
			{
				LLSD subs;
				subs["ERROR_CODE"] = sMessage;
				gNotifications.add("MerchantListingFailed", subs);
			}
			else
			{
				marketdata->activateListing(folder_id, true, 1);
			}
		}
		else
		{
			marketdata->activateListing(folder_id, false, 1);
		}
	}
}

//static
void LLMarketplace::activateFolder(const LLUUID& folder_id, bool activate)
{
	if (depthNesting(folder_id) == 2)
	{
		LLViewerInventoryCategory* cat = gInventory.getCategory(folder_id);
		if (cat)
		{
			sMessage.clear();
			if (activate &&
				!validateListings(cat,
								  boost::bind(&LLMarketplace::gatherMessage,
											  _1, _2, _3),
								  false, 2))
			{
				LLSD subs;
				subs["ERROR_CODE"] = sMessage;
				gNotifications.add("MerchantFolderActivationFailed", subs);
				return;
			}
			LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
			const LLUUID& link_id = activate ? folder_id : LLUUID::null;
			marketdata->setVersionFolder(cat->getParentUUID(), link_id);
		}
	}
}

//static
void LLMarketplace::updateFolderHierarchy(const LLUUID& cat_id)
{
	// When changing the marketplace status of a folder, the only thing that
	// needs to happen is for all observers of the folder to, possibly, change
	// the display label of the folder so that's the only thing we change on
	// the update mask.
	gInventory.addChangedMask(LLInventoryObserver::LABEL, cat_id);

	// Update all descendent folders down
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat_id, cat_array, item_array);
	if (!cat_array || !item_array)
	{
		llwarns << "Failed to get descendents of: " << cat_id << llendl;
		return;
	}

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(),
												 end = cat_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryCategory* category = *iter;
		if (category)
		{
			updateFolderHierarchy(category->getUUID());
		}
	}
}

//static
void LLMarketplace::updateCategory(const LLUUID& cur_uuid,
								   bool perform_consistency_enforcement)
{
	// When changing the marketplace status of an item, we usually have to
	// change the status of all folders in the same listing. This is because
	// the display of each folder is affected by the overall status of the
	// whole listing. Consequently, the only way to correctly update an item
	// anywhere in the marketplace is to update the whole listing from its
	// listing root. This is not as bad as it seems as we only update folders,
	// not items, and the folder nesting depth is limited to 4.
	// We also take care of degenerated cases so we do not update all folders
	// in the inventory by mistake.

	if (cur_uuid.isNull())
	{
		return;
	}

	LLViewerInventoryCategory* cat = gInventory.getCategory(cur_uuid);
	if (!cat || cat->isVersionUnknown())
	{
		return;
	}

	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();

	// Grab marketplace listing data for this item
	S32 depth = depthNesting(cur_uuid);
	if (depth > 0)
	{
		// Retrieve the listing uuid this object is in
		const LLUUID& listing_uuid = nestedParentId(cur_uuid, depth);
		if (perform_consistency_enforcement)
		{
			cat = gInventory.getCategory(listing_uuid);
			if (!cat || cat->isVersionUnknown())
			{
				perform_consistency_enforcement = false;
			}
		}

		// Verify marketplace data consistency for this listing
		if (perform_consistency_enforcement &&
			 marketdata->isListed(listing_uuid))
		{
			const LLUUID& vf_uuid = marketdata->getVersionFolder(listing_uuid);
			if (vf_uuid.notNull())
			{
				S32 version_depth = depthNesting(vf_uuid);
				if (version_depth != 2 ||
					!gInventory.isObjectDescendentOf(vf_uuid, listing_uuid))
				{
					llinfos << "Unlisting and clearing the listing folder "
							<< listing_uuid
							<< " because the version folder " << vf_uuid
							<< " is not at the right place anymore" << llendl;
					marketdata->setVersionFolder(listing_uuid, LLUUID::null);
				}
				else if (gInventory.isCategoryComplete(vf_uuid) &&
						 marketdata->getActivationState(vf_uuid) &&
						 count_descendants_items(vf_uuid) == 0 &&
						 !marketdata->isUpdating(vf_uuid, depth))
				{
					llinfos << "Unlisting the listing folder " << listing_uuid
							<< " because the version folder " << vf_uuid
							<< " is empty" << llendl;
					marketdata->activateListing(listing_uuid, false);
				}
			}
		}

		// Check if the count on hand needs to be updated on SLM
		if (perform_consistency_enforcement &&
			computeStockCount(listing_uuid,
							  true) != marketdata->getCountOnHand(listing_uuid))
		{
			marketdata->updateCountOnHand(listing_uuid);
		}

		// Update all descendents starting from the listing root
		updateFolderHierarchy(listing_uuid);
	}
	else if (depth == 0)
	{
		// If this is the marketplace listings root itself, update all descendents
		if (gInventory.getCategory(cur_uuid))
		{
			updateFolderHierarchy(cur_uuid);
		}
	}
	else
	{
		// If the folder is outside the marketplace listings root, clear its
		// SLM data if needs be
		if (perform_consistency_enforcement && marketdata->isListed(cur_uuid))
		{
			llinfos << "Disassociating since the listing folder is not under the marketplace folder anymore"
					<< llendl;
			marketdata->clearListing(cur_uuid);
		}
		// Update all descendents if this is a category
		if (gInventory.getCategory(cur_uuid))
		{
			updateFolderHierarchy(cur_uuid);
		}
	}
}

// Iterate through the marketplace and flag for label change all categories
// that countain a stock folder (i.e. stock folders and embedding folders up
// the hierarchy)
//static
void LLMarketplace::updateAllCounts(const LLUUID& cat_id)
{
	// Get all descendent folders down
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat_id, cat_array, item_array);
	if (!cat_array || !item_array)
	{
		llwarns << "Failed to get descendents of: " << cat_id << llendl;
		return;
	}

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(),
												 end = cat_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryCategory* category = *iter;
		if (!category) continue;	// Paranoia

		if (category->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
		{
			// Listing containing stock folders needs to be updated but not
			// others. Note: we take advantage of the fact that stock folder
			// *do not* contain sub folders to avoid a recursive call here.
			updateCategory(category->getUUID());
			gInventory.notifyObservers();
		}
		else
		{
			// Explore the contained folders recursively
			updateAllCounts(category->getUUID());
		}
	}
}

//static
void LLMarketplace::updateAllCounts()
{
	if (LLMarketplaceData::getInstance()->checkDirtyCount())
	{
		// Get the marketplace root and launch the recursive exploration
		if (sMarketplaceListingId.notNull())
		{
			updateAllCounts(sMarketplaceListingId);
		}
	}
}

//static
void LLMarketplace::initializeCallback()
{
	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	U32 status = marketdata->getSLMStatus();
	if (status == MarketplaceStatusCodes::MARKET_PLACE_MERCHANT ||
		status == MarketplaceStatusCodes::MARKET_PLACE_MIGRATED_MERCHANT)
	{
		// Create the Marketplace Listings folder if missing
		sMarketplaceListingId =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS,
											   true);
		if (sMarketplaceListingId.isNull())
		{
			llwarns << "Failed to create the Marketplace Listings folder"
					<< llendl;
			marketdata->setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED);
		}
		else
		{
			marketdata->setSLMDataFetched(MarketplaceFetchCodes::MARKET_FETCH_LOADING);
			marketdata->getSLMListings();
		}
	}
	else
	{
		sMarketplaceListingId =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS,
											   false);
	}
	if (sMarketplaceListingId.isNull())
	{
		return;
	}

	// We should not have to do that but with a client/server system relying on
	// a "well known folder" convention, things get messy and conventions get
	// broken down eventually
	gInventory.consolidateForType(sMarketplaceListingId,
								  LLFolderType::FT_MARKETPLACE_LISTINGS);

	// Force an update of the market place items labels
	LL_DEBUGS("Marketplace") << "Updating Marketplace Listings folder items labels"
							 << LL_ENDL;
	gInventory.addChangedMask(LLInventoryObserver::LABEL,
							  sMarketplaceListingId);

#if 0	// We needed this during the SLM transition, because when crossing
		// borders with a non-SLM-aware region, the labels of the folders were
		// changed based on the 'not MARKET_PLACE_MERCHANT' status, instead of
		// being updated by the responders (which obviously could not respond
		// any more to anything...)
	LLInventoryModel::cat_array_t descendent_categories;
	LLInventoryModel::item_array_t descendent_items;
	gInventory.collectDescendents(sMarketplaceListingId,
								  descendent_categories, descendent_items,
								  false);
	for (S32 i = 0, count = descendent_categories.size(); i < count; ++i)
	{
		LLViewerInventoryCategory* cat = descendent_categories[i];
		if (cat)
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL,
									  cat->getUUID());
		}
	}
#endif

	gInventory.notifyObservers();
}

//static
void LLMarketplace::setup(bool warn)
{
	sMarketplaceListingId =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS,
										   false);
	if (sMarketplaceListingId.notNull())
	{
		LLInventoryModel::cat_array_t categories;
		LLInventoryModel::item_array_t items;
		gInventory.collectDescendents(sMarketplaceListingId, categories, items,
									  false);
		U32 max_count = gSavedSettings.getU32("MarketplaceLargeInventory");
		if (categories.size() >= max_count)
		{
			if (warn)
			{
				gNotifications.add("AlertLargeMarketplace");
			}
			return;
		}
	}
	LLMarketplaceData::getInstance()->initializeSLM(boost::bind(&LLMarketplace::initializeCallback));
}

//static
void LLMarketplace::checkMerchantStatus()
{
	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	marketdata->setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED);
	marketdata->initializeSLM(boost::bind(&LLMarketplace::initializeCallback));
}

//static
bool LLMarketplace::connected()
{
	U32 status = LLMarketplaceData::getInstance()->getSLMStatus();
	return status == MarketplaceStatusCodes::MARKET_PLACE_MERCHANT ||
		   status == MarketplaceStatusCodes::MARKET_PLACE_MIGRATED_MERCHANT;
}

bool sort_alpha(const LLViewerInventoryCategory* cat1,
				const LLViewerInventoryCategory* cat2)
{
	return cat1->getName().compare(cat2->getName()) < 0;
}

// Make all relevant business logic checks on the marketplace listings starting
// with the folder as argument. This function does no deletion of listings but
// a mere audit and raises issues to the user (through the optional callback).
// It also returns a boolean, true if things validate, false if issues are
// raised. The only inventory changes that are done is to move and sort folders
// containing no-copy items to stock folders.
//static
bool LLMarketplace::validateListings(LLViewerInventoryCategory* cat,
									 LLMarketplace::validation_callback_t cb,
									 bool fix_hierarchy, S32 depth)
{
	if (!cat) return false;

	// Folder is valid unless issue is raised
	bool result = true;

	// Get the type and the depth of the folder
	LLViewerInventoryCategory* viewer_cat = cat;
	const LLFolderType::EType folder_type = cat->getPreferredType();
	if (depth < 0)
	{
		// If the depth argument was not provided, evaluate the depth directly
		depth = depthNesting(cat->getUUID());
	}
	if (depth < 0)
	{
		// If the folder is not under the marketplace listings root, we run
		// validation as if it was a listing folder and prevent any hierarchy
		// fix. This allows the function to be used to pre-validate a folder
		// anywhere in the inventory.
		depth = 1;
		fix_hierarchy = false;
	}

	// Set the indentation for print output
	std::string indent;
	for (S32 i = 1; i < depth; ++i)
	{
		indent += "  ";
	}
	std::string message;

	// Check out that version folders are marketplace ready
	if (depth == 2)
	{
		if (!canMoveFolderInto(cat, cat, cat, message, 0))
		{
			result = false;
			if (cb)
			{
				message = indent + cat->getName() +
						  LLTrans::getString("Marketplace Validation Error") +
						  " " + message;
				cb(message, depth, LLError::LEVEL_ERROR);
			}
		}
	}

	// Check out that stock folders are at the right level
	if (folder_type == LLFolderType::FT_MARKETPLACE_STOCK && depth <= 2)
	{
		if (cb)
		{
			message = indent + cat->getName();
		}
		if (fix_hierarchy)
		{
			if (cb)
			{
				message +=
					LLTrans::getString("Marketplace Validation Warning") +
					" " +
					LLTrans::getString("Marketplace Validation Warning Stock");
				cb(message, depth, LLError::LEVEL_WARN);
			}
			// Nest the stock folder one level deeper in a normal folder and
			// restart from there
			const LLUUID& parent_id = cat->getParentUUID();
			const LLUUID& folder_id =
					gInventory.createCategoryUDP(parent_id,
												 LLFolderType::FT_NONE,
												 cat->getName());
			gInventory.notifyObservers();
			LLViewerInventoryCategory* new_cat =
				gInventory.getCategory(folder_id);
			gInventory.changeCategoryParent(viewer_cat, folder_id, false);
			gInventory.notifyObservers();
			result &= validateListings(new_cat, cb, fix_hierarchy, ++depth);
			return result;
		}

		result = false;
		if (cb)
		{
			message += LLTrans::getString("Marketplace Validation Error") +
					   " " +
					   LLTrans::getString("Marketplace Validation Warning Stock");
			cb(message, depth, LLError::LEVEL_ERROR);
		}
	}

	// Item sorting and validation: sorting and moving the various stock items
	// is complicated as the set of constraints is high. We need to:
	// * separate non stock items, stock items per types in different folders
	// * have stock items nested at depth 2 at least
	// * never ever move the non-stock items

	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat->getUUID(), cat_array, item_array);
	if (!cat_array || !item_array)
	{
		if (cb)
		{
			message = indent + cat->getName() +
					  LLTrans::getString("Marketplace Failed Descendents");
			cb(message, depth, LLError::LEVEL_ERROR);
		}
		return false;
	}

	// We use a composite (type, permissions) key on that map to store UUIDs of
	// items of same (type, permissions)
	std::map<U32, std::vector<LLUUID> > items_vector;

	// Parse the items and create vectors of item UUIDs sorting copyable items
	// and stock items of various types
	const LLUUID& group_id = gAgent.getGroupID();
	bool has_bad_items = false;
	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator iter = item_array_copy.begin();
		 iter != item_array_copy.end(); ++iter)
	{
		LLViewerInventoryItem* itemp = *iter;
		if (!itemp) continue;	// Paranoia

		// Test but skip items that should not be there to start with, raise
		// an error message for those
		std::string error_msg;
		if (!can_move_to_marketplace(itemp, error_msg))
		{
			has_bad_items = true;
			if (cb && fix_hierarchy)
			{
				message = indent + itemp->getName() +
						  LLTrans::getString("Marketplace Validation Error") +
						  " " + error_msg;
				cb(message, depth, LLError::LEVEL_ERROR);
			}
			continue;
		}
		// Update the appropriate vector item for that type

		// Default value for non stock items:
		LLInventoryType::EType type = LLInventoryType::IT_COUNT;
		U32 perms = 0;
		if (!itemp->getPermissions().allowCopyBy(gAgentID, group_id))
		{
			// Get the item type for stock items
			type = itemp->getInventoryType();
			perms = itemp->getPermissions().getMaskNextOwner();
		}
		U32 key = (((U32)(type) & 0xFF) << 24) | (perms & 0xFFFFFF);
		items_vector[key].emplace_back(itemp->getUUID());
	}

	// How many types of items ?  Which type is it if only one ?
	S32 count = items_vector.size();
	// This is the key for any normal copyable item:
	U32 default_key = (U32)(LLInventoryType::IT_COUNT) << 24;
	// The key in the case of one item type only:
	U32 unique_key = count == 1 ? items_vector.begin()->first : default_key;

	// If we have no items in there (only folders or empty), analyze a bit
	// further
	if (count == 0 && !has_bad_items)
	{
		if (cb)
		{
			message = indent + cat->getName();
			if (cat_array->size() == 0)
			{
				// So we have no item and no folder. That is a warning.
				if (depth == 2)
				{
					// If this is an empty version folder, warn only (listing
					// would not be delivered by AIS, but only AIS should
					// unlist)
					message +=
						LLTrans::getString("Marketplace Validation Error Empty Version");
					cb(message, depth, LLError::LEVEL_WARN);
				}
				else if (depth > 2 &&
						 folder_type == LLFolderType::FT_MARKETPLACE_STOCK)
				{
					// If this is a legit but empty stock folder, warn only
					// (listing must stay searchable when out of stock)
					message +=
						LLTrans::getString("Marketplace Validation Error Empty Stock");
					cb(message, depth, LLError::LEVEL_WARN);
				}
				else
				{
					// We warn if there's nothing in a regular folder (may be it's
					// an under construction listing)
					message +=
						LLTrans::getString("Marketplace Validation Warning Empty");
					cb(message, depth, LLError::LEVEL_WARN);
				}
			}
			else if (result && depth >= 1)
			{
				// Done with that folder: print out the folder name unless we
				// already found an error here
				message += LLTrans::getString("Marketplace Validation Log");
				cb(message, depth, LLError::LEVEL_INFO);
			}
		}
	}
	// If we have a single type of items of the right type in the right place,
	// we are done
	else if (count == 1 && !has_bad_items &&
			 ((unique_key == default_key && depth > 1) ||
			  (folder_type == LLFolderType::FT_MARKETPLACE_STOCK &&
			   depth > 2 && cat_array->size() == 0)))
	{
		// Done with that folder: print out the folder name unless we already
		// found an error here
		if (cb && result && depth >= 1)
		{
			message = indent + cat->getName() +
					  LLTrans::getString("Marketplace Validation Log");
			cb(message, depth, LLError::LEVEL_INFO);
		}
	}
	else
	{
		if (fix_hierarchy && !has_bad_items)
		{
			// Alert the user when an existing stock folder has to be split
			if (folder_type == LLFolderType::FT_MARKETPLACE_STOCK &&
				(count >= 2 || cat_array->size() > 0))
			{
				gNotifications.add("AlertMerchantStockFolderSplit");
			}
			// If we have more than 1 type of items or we are at the listing
			// level or we have stock/no stock type mismatch, wrap the items
			// in subfolders
			if (count > 1 || depth == 1 ||
				(folder_type == LLFolderType::FT_MARKETPLACE_STOCK &&
				 unique_key == default_key) ||
				(folder_type != LLFolderType::FT_MARKETPLACE_STOCK &&
				 unique_key != default_key))
			{
				// Create one folder per vector at the right depth and of the
				// right type
				for (std::map<U32, std::vector<LLUUID> >::iterator
						it = items_vector.begin(), end = items_vector.end();
					 it != end; ++it)
				{
					// Create a new folder
					const LLUUID& parent_uuid = depth > 2 ? viewer_cat->getParentUUID()
														  : viewer_cat->getUUID();
					LLViewerInventoryItem* item = gInventory.getItem(it->second.back());
					std::string folder_name = depth >= 1 ? viewer_cat->getName()
														 : item->getName();
					LLFolderType::EType new_folder_type =
						it->first == default_key ? LLFolderType::FT_NONE
												 : LLFolderType::FT_MARKETPLACE_STOCK;
					if (cb)
					{
						message = indent + folder_name;
						if (new_folder_type == LLFolderType::FT_MARKETPLACE_STOCK)
						{
							message +=
								LLTrans::getString("Marketplace Validation Warning Create Stock");
						}
						else
						{
							message +=
								LLTrans::getString("Marketplace Validation Warning Create Version");
						}
						cb(message, depth, LLError::LEVEL_WARN);
					}
					LLUUID folder_uuid = gInventory.createCategoryUDP(parent_uuid,
																	  new_folder_type,
																	  folder_name);
					gInventory.notifyObservers();

					// Move each item to the new folder
					while (!it->second.empty())
					{
						item = gInventory.getItem(it->second.back());
						if (cb)
						{
							message = indent + item->getName() +
									  LLTrans::getString("Marketplace Validation Warning Move");
							cb(message, depth, LLError::LEVEL_WARN);
						}
						gInventory.changeItemParent(item, folder_uuid, true);
						gInventory.notifyObservers();
						it->second.pop_back();
					}
					updateCategory(parent_uuid);
					gInventory.notifyObservers();
					updateCategory(folder_uuid);
					gInventory.notifyObservers();
				}
			}

			// Stock folder should have no sub folder so reparent those up
			if (folder_type == LLFolderType::FT_MARKETPLACE_STOCK)
			{
				const LLUUID& parent_uuid = cat->getParentUUID();
				gInventory.getDirectDescendentsOf(cat->getUUID(), cat_array,
												  item_array);
				if (!cat_array || !item_array)
				{
					if (cb)
					{
						message = indent + cat->getName() +
					 			 LLTrans::getString("Marketplace Failed Descendents");
						cb(message, depth, LLError::LEVEL_ERROR);
					}
					result = false;
				}
				else
				{
					LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
					for (LLInventoryModel::cat_array_t::iterator
							iter = cat_array_copy.begin();
						  iter != cat_array_copy.end(); ++iter)
					{
						LLViewerInventoryCategory* viewer_cat = *iter;
						if (!viewer_cat) continue;	// Paranoia

						gInventory.changeCategoryParent(viewer_cat, parent_uuid,
														false);
						gInventory.notifyObservers();
						result &= validateListings(viewer_cat, cb, fix_hierarchy,
												   depth);
					}
				}
			}
		}
		else if (cb)
		{
			// We are not fixing the hierarchy but reporting problems, report
			// everything we can find.
			// Print the folder name
			if (result && depth >= 1)
			{
				message = indent + cat->getName();
				if (folder_type == LLFolderType::FT_MARKETPLACE_STOCK)
				{
					if (count >= 2)
					{
						// Report if a stock folder contains a mix of items
						result = false;
						message +=
							LLTrans::getString("Marketplace Validation Error Mixed Stock");
						cb(message, depth, LLError::LEVEL_ERROR);
					}
					else if (cat_array->size())
					{
						// Report if a stock folder contains subfolders
						result = false;
						message +=
							LLTrans::getString("Marketplace Validation Error Subfolder In Stock");
						cb(message, depth, LLError::LEVEL_ERROR);
					}
				}
				if (result)
				{
					// Simply print the folder name
					message += LLTrans::getString("Marketplace Validation Log");
					cb(message, depth, LLError::LEVEL_INFO);
				}
			}

			// Scan each item and report if there's a problem
			LLInventoryModel::item_array_t item_array_copy = *item_array;
			for (LLInventoryModel::item_array_t::iterator
					iter = item_array_copy.begin();
				 iter != item_array_copy.end(); ++iter)
			{
				LLViewerInventoryItem* item = *iter;
				if (!item) continue;	// Paranoia

				message = indent + "  " + item->getName();
				std::string error_msg;
				if (!can_move_to_marketplace(item, error_msg))
				{
					// Report items that should not be there to start with
					result = false;
					message += LLTrans::getString("Marketplace Validation Error") +
							   " " + error_msg;
					cb(message, depth, LLError::LEVEL_ERROR);
				}
				else if (folder_type != LLFolderType::FT_MARKETPLACE_STOCK &&
						 !item->getPermissions().allowCopyBy(gAgentID,
															 group_id))
				{
					// Report stock items that are misplaced
					result = false;
					message +=
						LLTrans::getString("Marketplace Validation Error Stock Item");
					cb(message, depth, LLError::LEVEL_ERROR);
				}
				else if (depth == 1)
				{
					// Report items not wrapped in version folder
					result = false;
					message +=
						LLTrans::getString("Marketplace Validation Warning Unwrapped Item");
					cb(message, depth, LLError::LEVEL_ERROR);
				}
			}
		}

		// Clean up
		if (viewer_cat->getDescendentCount() == 0)
		{
			// Remove the current folder if it ends up empty
			if (cb)
			{
				message = indent + viewer_cat->getName() +
						  LLTrans::getString("Marketplace Validation Warning Delete");
				cb(message, depth, LLError::LEVEL_WARN);
			}
			gInventory.removeCategory(cat->getUUID());
			gInventory.notifyObservers();
			return result && !has_bad_items;
		}
	}

	// Recursion : Perform the same validation on each nested folder
	gInventory.getDirectDescendentsOf(cat->getUUID(), cat_array, item_array);
	if (!cat_array || !item_array)
	{
		if (cb)
		{
			message = indent + cat->getName() +
					  LLTrans::getString("Marketplace Failed Descendents");
			cb(message, depth, LLError::LEVEL_ERROR);
		}
		return false;
	}
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	// Sort the folders in alphabetical order first
	std::sort(cat_array_copy.begin(), cat_array_copy.end(), sort_alpha);

	for (LLInventoryModel::cat_array_t::iterator
			iter = cat_array_copy.begin();
		 iter != cat_array_copy.end(); ++iter)
	{
		LLViewerInventoryCategory* category = *iter;
		result &= validateListings(category, cb, fix_hierarchy, depth + 1);
	}

	// Update the current folder
	updateCategory(cat->getUUID(), fix_hierarchy);
	gInventory.notifyObservers();

	return result && !has_bad_items;
}

//static
bool LLMarketplace::hasPermissionsForSale(LLViewerInventoryCategory* cat,
										  std::string& error_msg)
{
	if (!cat)
	{
		error_msg = "NULL category !";
		return false;
	}

	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat->getUUID(), cat_array, item_array);
	if (!cat_array || !item_array)
	{
		llwarns << "Failed to get descendents of: " << cat->getUUID()
				<< llendl;
		return false;
	}

	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator
			iter = item_array_copy.begin(), end = item_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryItem* item = *iter;
		if (!item || !can_move_to_marketplace(item, error_msg, false))
		{
			return false;
		}
	}

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator
			iter = cat_array_copy.begin(), end = cat_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryCategory* category = *iter;
		if (!category || !hasPermissionsForSale(category, error_msg))
		{
			return false;
		}
	}

	return true;
}

// Returns true if inv_item can be dropped in dest_folder, a folder nested in
// Marketplace listings (or merchant inventory) under the root_folder root. If
// false is returned, tooltip_msg contains an error message to display to the
// user (localized and all). bundle_size is the amount of sibling items that
// are getting moved to the marketplace at the same time.
//static
bool LLMarketplace::canMoveItemInto(const LLViewerInventoryCategory* root_folder,
									LLViewerInventoryCategory* dest_folder,
									LLViewerInventoryItem* inv_item,
									std::string& tooltip_msg,
									S32 bundle_size, bool from_paste)
{
	// Check stock folder type matches item type in marketplace listings or
	// merchant outbox (even if of no use there for the moment)
	bool move_in_stock =
		dest_folder &&
		dest_folder->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK;
	bool accept = dest_folder && dest_folder->acceptItem(inv_item);
	if (!accept)
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxMixedStock");
	}

	// Check that the item has the right type and permissions to be sold on the
	// marketplace
	if (accept)
	{
		accept = can_move_to_marketplace(inv_item, tooltip_msg, true);
	}

	// Check that the total amount of items woould not violate the max limit on
	// the marketplace
	if (accept)
	{
		// If the dest folder is a stock folder, we do not count the incoming
		// items toward the total (stock items are seen as one)
		S32 existing_item_count = move_in_stock ? 0 : bundle_size;

		// If the dest folder is a stock folder, we do assume that the incoming
		// items are also stock items (they should anyway)
		S32 existing_stock_count = move_in_stock ? bundle_size : 0;

		S32 existing_folder_count = 0;

		// Get the version folder: that's where the counts start from
		const LLViewerInventoryCategory* version_folder = NULL;
		if (root_folder && root_folder != dest_folder)
		{
			version_folder =
				gInventory.getFirstDescendantOf(root_folder->getUUID(),
												dest_folder->getUUID());
		}

		if (version_folder)
		{
			if (!from_paste &&
				gInventory.isObjectDescendentOf(inv_item->getUUID(),
												version_folder->getUUID()))
			{
				// Clear those counts or they will be counted twice because
				// we are already inside the version category
				existing_item_count = 0;
			}

			LLInventoryModel::cat_array_t existing_categories;
			LLInventoryModel::item_array_t existing_items;
			gInventory.collectDescendents(version_folder->getUUID(),
										  existing_categories, existing_items,
										  false);

			existing_item_count += count_copyable_items(existing_items) +
								   count_stock_folders(existing_categories);
			existing_stock_count += count_stock_items(existing_items);
			existing_folder_count += existing_categories.size();

			// If the incoming item is a nocopy (stock) item, we need to
			// consider that it will create a stock folder
			if (!move_in_stock &&
				!inv_item->getPermissions().allowCopyBy(gAgentID,
														gAgent.getGroupID()))
			{
				// Note: we do not assume that all incoming items are no-copy
				// of different kinds...
				++existing_folder_count;
			}
		}

		static LLCachedControl<U32> max_items(gSavedSettings,
											  "InventoryOutboxMaxItemCount");
		static LLCachedControl<U32> max_stock(gSavedSettings,
											  "InventoryOutboxMaxStockItemCount");
		static LLCachedControl<U32> max_folders(gSavedSettings,
												"InventoryOutboxMaxFolderCount");
		if (existing_item_count > (S32)max_items)
		{
			LLStringUtil::format_map_t args;
			args["[AMOUNT]"] = llformat("%d", (S32)max_items);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyObjects",
											 args);
			accept = false;
		}
		else if (existing_stock_count > (S32)max_stock)
		{
			LLStringUtil::format_map_t args;
			args["[AMOUNT]"] = llformat("%d", (S32)max_stock);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyStockItems",
											 args);
			accept = false;
		}
		else if (existing_folder_count > (S32)max_folders)
		{
			LLStringUtil::format_map_t args;
			args["[AMOUNT]"] = llformat("%d", (S32)max_folders);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyFolders",
											 args);
			accept = false;
		}
	}

	return accept;
}

// Returns true if inv_cat can be dropped in dest_folder, a folder nested in
// marketplace listings (or merchant inventory) under the root_folder root.
// If returns is false, tooltip_msg contains an error message to display to the
// user (localized and all). bundle_size is the amount of sibling items that
// are getting moved to the marketplace at the same time.
//static
bool LLMarketplace::canMoveFolderInto(const LLViewerInventoryCategory* root_folder,
									  LLViewerInventoryCategory* dest_folder,
									  LLViewerInventoryCategory* inv_cat,
									  std::string& tooltip_msg,
									  S32 bundle_size, bool from_paste)
{
	bool accept = true;

	// Compute the nested folders level we will add into with that incoming
	// folder
	S32 incoming_folder_depth = get_folder_levels(inv_cat);
	// Compute the nested folders level we are inserting ourselves in.
	// Note: add 1 when inserting under a listing folder as we need to take the
	// root listing folder in the count
	S32 insertion_point = 1;
	if (root_folder)
	{
		insertion_point = get_folder_path_length(root_folder->getUUID(),
												 dest_folder->getUUID()) + 1;
	}

	// Get the version folder: that's where the folders and items counts start
	// from
	const LLViewerInventoryCategory* version_folder = NULL;
	if (insertion_point >= 2)
	{
		version_folder = gInventory.getFirstDescendantOf(root_folder->getUUID(),
														 dest_folder->getUUID());
	}

	// Compare the whole with the nested folders depth limit. Note: substract 2
	// as we leave root and version folder out of the count threshold
	U32 max_depth = gSavedSettings.getU32("InventoryOutboxMaxFolderDepth");
	if (incoming_folder_depth + insertion_point - 2 > (S32)max_depth)
	{
		LLStringUtil::format_map_t args;
		args["[AMOUNT]"] = llformat("%d", (S32)max_depth);
		tooltip_msg = LLTrans::getString("TooltipOutboxFolderLevels", args);
		accept = false;
	}

	if (accept)
	{
		LLInventoryModel::cat_array_t descendent_categories;
		LLInventoryModel::item_array_t descendent_items;
		gInventory.collectDescendents(inv_cat->getUUID(),
									  descendent_categories, descendent_items,
									  false);

		// Note: we assume that we're moving a bunch of folders in. That might
		// be wrong...
		S32 dragged_folder_count = descendent_categories.size() + bundle_size;
		S32 dragged_item_count = count_copyable_items(descendent_items) +
								 count_stock_folders(descendent_categories);
		S32 dragged_stock_count = count_stock_items(descendent_items);
		S32 existing_item_count = 0;
		S32 existing_stock_count = 0;
		S32 existing_folder_count = 0;

		if (version_folder)
		{
			if (!from_paste &&
				gInventory.isObjectDescendentOf(inv_cat->getUUID(),
												version_folder->getUUID()))
			{
				// Clear those counts or they will be counted twice because
				// we are already inside the version category
				dragged_folder_count = 0;
				dragged_item_count = 0;
				dragged_stock_count = 0;
			}

			// Tally the total number of categories and items inside the root
			// folder
			LLInventoryModel::cat_array_t existing_categories;
			LLInventoryModel::item_array_t existing_items;
			gInventory.collectDescendents(version_folder->getUUID(),
										  existing_categories, existing_items,
										  false);

			existing_folder_count += existing_categories.size();
			existing_item_count += count_copyable_items(existing_items) +
								   count_stock_folders(existing_categories);
			existing_stock_count += count_stock_items(existing_items);
		}

		const S32 total_folder_count = existing_folder_count + dragged_folder_count;
		const S32 total_item_count = existing_item_count + dragged_item_count;
		const S32 total_stock_count = existing_stock_count + dragged_stock_count;

		static LLCachedControl<U32> max_items(gSavedSettings,
											  "InventoryOutboxMaxItemCount");
		static LLCachedControl<U32> max_stock(gSavedSettings,
											  "InventoryOutboxMaxStockItemCount");
		static LLCachedControl<U32> max_folders(gSavedSettings,
												"InventoryOutboxMaxFolderCount");

		if (total_folder_count > (S32)max_folders)
		{
			LLStringUtil::format_map_t args;
			args["[AMOUNT]"] = llformat("%d", (S32)max_folders);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyFolders",
											 args);
			accept = false;
		}
		else if (total_item_count > (S32)max_items)
		{
			LLStringUtil::format_map_t args;
			args["[AMOUNT]"] = llformat("%d", (S32)max_items);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyObjects",
											 args);
			accept = false;
		}
		else if (total_stock_count >  (S32)max_stock)
		{
			LLStringUtil::format_map_t args;
			args["[AMOUNT]"] = llformat("%d", (S32)max_stock);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyStockItems",
											 args);
			accept = false;
		}

		// Now check that each item in the folder can be moved into the
		// marketplace
		if (accept)
		{
			for (S32 i = 0, count = descendent_items.size(); i < count; ++i)
			{
				LLViewerInventoryItem* item = descendent_items[i];
				if (!can_move_to_marketplace(item, tooltip_msg))
				{
					accept = false;
					break;
				}
			}
		}
	}

	return accept;
}

//static
bool LLMarketplace::moveItemInto(LLViewerInventoryItem* inv_item,
								 const LLUUID& dest_folder, bool copy)
{
	// Get the marketplace listings depth of the destination folder, exit with
	// error if not under marketplace
	S32 depth = depthNesting(dest_folder);
	if (depth < 0)
	{
		LLSD subs;
		subs["ERROR_CODE"] = LLTrans::getString("Marketplace Error Prefix") +
							 LLTrans::getString("Marketplace Error Not Merchant");
		gNotifications.add("MerchantPasteFailed", subs);
		return false;
	}

	// We will collapse links into items/folders
	LLViewerInventoryItem* vitem = inv_item;
	LLViewerInventoryCategory* linked_category = vitem->getLinkedCategory();
	if (linked_category)
	{
		// Move the linked folder directly
		return moveFolderInto(linked_category, dest_folder, copy);
	}

	// Grab the linked item if any
	LLViewerInventoryItem* linked_item = vitem->getLinkedItem();
	if (linked_item)
	{
		vitem = linked_item;
	}

	// If we want to copy but the item is no copy, fail silently (this is a
	// common case that does not warrant notification)
	if (copy &&
		!vitem->getPermissions().allowCopyBy(gAgentID, gAgent.getGroupID()))
	{
		return false;
	}

	// Check that the agent has transfer permission on the item: this is
	// required as a resident cannot put on sale items they cannot transfer.
	// Proceed with move if we have permission.
	std::string error_msg;
	if (!can_move_to_marketplace(inv_item, error_msg, true))
	{
		LLSD subs;
		subs["ERROR_CODE"] = LLTrans::getString("Marketplace Error Prefix") +
							 error_msg;
		gNotifications.add("MerchantPasteFailed", subs);
		return false;
	}

	LLUUID dest_id(dest_folder);	// Destination id may change
	// When moving an isolated item, we might need to create the folder
	// structure to support it
	if (depth == 0)
	{
		// We need a listing folder
		dest_id = gInventory.createCategoryUDP(dest_id, LLFolderType::FT_NONE,
											   vitem->getName());
		gInventory.notifyObservers();
		++depth;
	}
	if (depth == 1)
	{
		// We need a version folder
		dest_id = gInventory.createCategoryUDP(dest_id, LLFolderType::FT_NONE,
											   vitem->getName());
		gInventory.notifyObservers();
		++depth;
	}
	LLViewerInventoryCategory* dest_cat = gInventory.getCategory(dest_id);
	if (!dest_cat)
	{
		llwarns << "Cannot find category for destination folder Id: "
				<< dest_id << llendl;
		return false;
	}
	if (dest_cat->getPreferredType() != LLFolderType::FT_MARKETPLACE_STOCK &&
		!vitem->getPermissions().allowCopyBy(gAgentID, gAgent.getGroupID()))
	{
		// We need to create a stock folder to move a no copy item
		dest_id = gInventory.createCategoryUDP(dest_id,
											   LLFolderType::FT_MARKETPLACE_STOCK,
											   vitem->getName());
		gInventory.notifyObservers();
		dest_cat = gInventory.getCategory(dest_id);
		++depth;
	}

	// Verify we can have this item in that destination category
	if (!dest_cat->acceptItem(vitem))
	{
		LLSD subs;
		subs["ERROR_CODE"] = LLTrans::getString("Marketplace Error Prefix") +
							 LLTrans::getString("Marketplace Error Not Accepted");
		gNotifications.add("MerchantPasteFailed", subs);
		return false;
	}

	if (copy)
	{
		// Copy the item
		LLPointer<LLInventoryCallback> cb =
			new LLBoostFuncInventoryCallback(boost::bind(update_folder_cb,
														 dest_id));
		copy_inventory_item(vitem->getPermissions().getOwner(),
							vitem->getUUID(), dest_id, LLStringUtil::null, cb);
	}
	else
	{
		// Reparent the item
		gInventory.changeItemParent(vitem, dest_id, true);
		gInventory.notifyObservers();
	}

	return true;
}

//static
bool LLMarketplace::moveFolderInto(LLViewerInventoryCategory* inv_cat,
								   const LLUUID& dest_folder, bool copy,
								   bool move_no_copy_items)
{
	S32 depth = depthNesting(dest_folder);
	if (depth < 0)
	{
		LLSD subs;
		subs["ERROR_CODE"] = LLTrans::getString("Marketplace Error Prefix") +
							 LLTrans::getString("Marketplace Error Not Merchant");
		gNotifications.add("MerchantPasteFailed", subs);
		return false;
	}

	// Check that we have adequate permission on all items being moved. Proceed
	// if we do.
	std::string error_msg;
	if (!hasPermissionsForSale(inv_cat, error_msg))
	{
		LLSD subs;
		subs["ERROR_CODE"] = LLTrans::getString("Marketplace Error Prefix") +
							 error_msg;
		gNotifications.add("MerchantPasteFailed", subs);
		return false;
	}

	// Get the destination folder
	LLViewerInventoryCategory* dest_cat = gInventory.getCategory(dest_folder);
	if (!dest_cat)
	{
		llwarns << "Cannot find category for destination folder Id: "
				<< dest_folder << llendl;
		return false;
	}

	// Check it's not a stock folder
	if (dest_cat->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
	{
		LLSD subs;
		subs["ERROR_CODE"] = LLTrans::getString("Marketplace Error Prefix") +
							 LLTrans::getString("Marketplace Error Not Accepted");
		gNotifications.add("MerchantPasteFailed", subs);
		return false;
	}

	// Get the parent folder of the moved item: we may have to update it
	const LLUUID& src_folder = inv_cat->getParentUUID();
	LLUUID dest_id(dest_folder);	// destination id may change
	if (copy)
	{
		if (depth == 0)
		{
			// We need a listing folder
			dest_id = gInventory.createCategoryUDP(dest_id,
												   LLFolderType::FT_NONE,
												   inv_cat->getName());
			gInventory.notifyObservers();
			++depth;
		}
		// Copy the folder
		copy_inventory_category(&gInventory, inv_cat, dest_id, LLUUID::null,
								move_no_copy_items);
	}
	else
	{
		// Reparent the folder
		gInventory.changeCategoryParent(inv_cat, dest_id, false);
		gInventory.notifyObservers();
		// Check the destination folder recursively for no copy items and
		// promote the including folders if any
		validateListings(dest_cat);
	}

	// Update the modified folders
	updateCategory(src_folder);
	gInventory.notifyObservers();
	updateCategory(dest_id);
	gInventory.notifyObservers();

	return true;
}

//static
void LLMarketplace::updateMovedFrom(const LLUUID& from_folder_uuid,
									const LLUUID& cat_id)
{
	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	if (from_folder_uuid == sMarketplaceListingId && cat_id.notNull())
	{
		// If we moved a folder at the listing folder level (i.e. its parent
		// is the marketplace listings folder). Unlist it.
		if (marketdata->isListed(cat_id))
		{
			marketdata->clearListing(cat_id);
		}
	}
	else
	{
#if 0	// Nope, does not work for forcing an inventory folder label update
		// when moving a stock folder out of a version folder... HB
		LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
		if (cat && cat->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
		{
			// If we moved a stock folder, flag all stock counts as dirty
			marketdata->setDirtyCount();
		}
#endif
		LLUUID version_id = marketdata->getActiveFolder(from_folder_uuid);
		if (version_id.notNull())
		{
			LLViewerInventoryCategory* cat = gInventory.getCategory(version_id);
			if (cat && !validateListings(cat))
			{
				// If we move from an active (listed) listing, check that it is
				// still valid, if not, unlist
				marketdata->activateListing(version_id, false);
			}
		}
#if 0	// Nope, does not work for forcing an inventory folder label update
		// when moving a stock folder out of a version folder... HB
		version_id = marketdata->getVersionFolder(from_folder_uuid);
		if (version_id.notNull())
		{
			LLViewerInventoryCategory* cat = gInventory.getCategory(version_id);
			if (cat)
			{
				// Update the listing folder we moved from
				updateCategory(cat->getParentUUID());
				gInventory.notifyObservers();
				return;
			}
		}
#endif
		// Update the folder we moved from anyway
		updateCategory(from_folder_uuid);
		gInventory.notifyObservers();
	}
}
