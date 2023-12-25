/**
 * @file llmarketplacefunctions.h
 * @brief Miscellaneous marketplace-related functions and classes
 * class definition
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

#ifndef LL_LLMARKETPLACEFUNCTIONS_H
#define LL_LLMARKETPLACEFUNCTIONS_H

#include "boost/function.hpp"
#include "boost/signals2.hpp"

#include "llcorehttpheaders.h"
#include "llcorehttpoptions.h"
#include "llinventory.h"
#include "llsingleton.h"

class LLFolderBridge;
class LLInventoryObserver;
class LLMarketplaceData;
class LLViewerInventoryCategory;
class LLViewerInventoryItem;

namespace MarketplaceStatusCodes
{
	enum sCode
	{
		MARKET_PLACE_NOT_INITIALIZED = 0,
		MARKET_PLACE_INITIALIZING = 1,
		MARKET_PLACE_CONNECTION_FAILURE = 2,
		MARKET_PLACE_MERCHANT = 3,
		MARKET_PLACE_NOT_MERCHANT = 4,
		MARKET_PLACE_NOT_MIGRATED_MERCHANT = 5,
		MARKET_PLACE_MIGRATED_MERCHANT = 6
	};
}

namespace MarketplaceFetchCodes
{
	enum sCode
	{
		MARKET_FETCH_NOT_DONE = 0,
		MARKET_FETCH_LOADING = 1,
		MARKET_FETCH_FAILED = 2,
		MARKET_FETCH_DONE = 3
	};
}

// Classes handling the data coming from and going to the Marketplace SLM
// server DB:
// * implements the Marketplace API
// * caches the current Marketplace data (tuples)
// * provides methods to get Marketplace data on any inventory item
// * sets Marketplace data
// * signals Marketplace updates to inventory
namespace SLMErrorCodes
{
	enum eCode
	{
		SLM_SUCCESS = 200,
		SLM_RECORD_CREATED = 201,
		SLM_MALFORMED_PAYLOAD = 400,
		SLM_NOT_FOUND = 404,
	};
}

// A Marketplace item is known by its tuple
class LLMarketplaceTuple
{
	friend class LLMarketplaceData;

public:
	LLMarketplaceTuple();
	LLMarketplaceTuple(const LLUUID& folder_id);
	LLMarketplaceTuple(const LLUUID& folder_id, S32 listing_id,
					   const LLUUID& version_id, bool is_listed = false);
	LLMarketplaceTuple(const LLUUID& folder_id, S32 listing_id,
					   const LLUUID& version_id, bool is_listed,
					   const std::string& edit_url, S32 count);

private:
	// Representation of a marketplace item in the Marketplace DB (well, what
	// we know of it...)
	LLUUID		mListingFolderId;
	LLUUID		mVersionFolderId;
	S32			mListingId;
	S32			mCountOnHand;
	std::string	mEditURL;
	bool		mIsActive;
};

// Session cache of all Marketplace tuples
// Notes:
// * There's one and only one possible set of Marketplace dataset per agent and
//   per session thus making it an LLSingleton
// * Some of those records might correspond to folders that do not exist in the
//   inventory anymore. We do not clear them out though. They just won't show
//   up in the UI.

class LLMarketplaceData : public LLSingleton<LLMarketplaceData>
{
	friend class LLSingleton<LLMarketplaceData>;

protected:
	LOG_CLASS(LLMarketplaceData);

public:
	// Public SLM API: Initialization and status
	typedef boost::signals2::signal<void()> status_updated_signal_t;
	void initializeSLM(const status_updated_signal_t::slot_type& cb);

	LL_INLINE U32 getSLMStatus() const				{ return mMarketPlaceStatus; }
	void setSLMStatus(S32 status);

	LL_INLINE void setSLMDataFetched(S32 status)	{ mMarketPlaceDataFetched = status; }
	LL_INLINE S32 getSLMDataFetched()				{ return mMarketPlaceDataFetched; }

	LL_INLINE bool isSLMDataFetched()
	{
		return mMarketPlaceDataFetched ==
				MarketplaceFetchCodes::MARKET_FETCH_DONE;
	}

	void getSLMListings();
	LL_INLINE bool isEmpty()						{ return mMarketplaceItems.size() == 0; }

	// High level create/delete/set Marketplace data: each method returns true
	// if the function succeeds, false if error
	bool createListing(const LLUUID& folder_id);
	bool activateListing(const LLUUID& folder_id, bool activate,
						 S32 depth = -1);
	bool clearListing(const LLUUID& folder_id, S32 depth = -1);
	bool setVersionFolder(const LLUUID& folder_id, const LLUUID& version_id,
						  S32 depth = -1);
	bool associateListing(const LLUUID& folder_id,
						  const LLUUID& source_folder_id, S32 listing_id);
	bool updateCountOnHand(const LLUUID& folder_id, S32 depth = -1);
	bool getListing(const LLUUID& folder_id, S32 depth = -1);
	bool getListing(S32 listing_id);
	bool deleteListing(S32 listing_id, bool update = true);

	// Probe the Marketplace data set to identify folders

	// returns true if folder_id is a Listing folder
	bool isListed(const LLUUID& folder_id);
	// returns true if folder_id is an active (listed) Listing folder
	bool isListedAndActive(const LLUUID& folder_id);
	// returns true if folder_id is a Version folder
	bool isVersionFolder(const LLUUID& folder_id);
	// returns true if the obj_id is buried in an active version folder
	bool isInActiveFolder(const LLUUID& obj_id, S32 depth = -1);
	// returns the UUID of the active version folder obj_id is in
	LLUUID getActiveFolder(const LLUUID& obj_id, S32 depth = -1);
	// returns true if we're waiting from SLM incoming data for folder_id
	bool isUpdating(const LLUUID& folder_id, S32 depth = -1);

	// Access Marketplace data set: each method returns a default value if the
	// argument cannot be found
	bool getActivationState(const LLUUID& folder_id);
	S32 getListingID(const LLUUID& folder_id);
	LLUUID getVersionFolder(const LLUUID& folder_id);
	std::string getListingURL(const LLUUID& folder_id, S32 depth = -1);
	LLUUID getListingFolder(S32 listing_id);
	S32 getCountOnHand(const LLUUID& folder_id);

	// Used to flag if stock count values for Marketplace have to be updated
	LL_INLINE bool checkDirtyCount()
	{
		if (mDirtyCount)
		{
			mDirtyCount = false;
			return true;
		}
		else
		{
			return false;
		}
	}

	LL_INLINE void setDirtyCount()					{ mDirtyCount = true; }

	void setUpdating(const LLUUID& folder_id, bool is_updating);

	// Used to add to the list of folder to validate on idle callback
	void listForIdleValidation(const LLUUID& folder_id);
	// Used to decide when to run a validation on listing folders
	void setValidationWaiting(const LLUUID& folder_id, S32 count);
	void decrementValidationWaiting(const LLUUID& folder_id, S32 count = 1);

private:
	LLMarketplaceData();
	~LLMarketplaceData() override;

	// Modify Marketplace data set: each method returns true if the function
	// succeeds, false if error.

	// Used internally only by SLM Responders when data are received from the
	// SLM Server
	bool addListing(const LLUUID& folder_id, S32 listing_id,
					const LLUUID& version_id, bool is_listed,
					const std::string& edit_url, S32 count);
	bool deleteListing(const LLUUID& folder_id, bool update = true);
	bool setListingID(const LLUUID& folder_id, S32 listing_id,
					  bool update = true);
	bool setVersionFolderID(const LLUUID& folder_id, const LLUUID& version_id,
							bool update = true);
	bool setActivationState(const LLUUID& folder_id, bool activate,
							bool update = true);
	bool setListingURL(const LLUUID& folder_id, const std::string& edit_url,
					   bool update = true);
	bool setCountOnHand(const LLUUID& folder_id, S32 count,
						bool update = true);

	// Private SLM API: package data and get/post/put requests to the SLM
	// server through the SLM API
	void createSLMListing(const LLUUID& folder_id, const LLUUID& version_id,
						  S32 count);
	void getSLMListing(S32 listing_id);
	void updateSLMListing(const LLUUID& folder_id, S32 listing_id,
						  const LLUUID& version_id, bool is_listed, S32 count);
	void associateSLMListing(const LLUUID& folder_id, S32 listing_id,
							 const LLUUID& version_id,
							 const LLUUID& source_folder_id);
	void deleteSLMListing(S32 listing_id);
	std::string getSLMConnectURL(const std::string& route);

	// Coroutines for the above methods
	void getMerchantStatusCoro(const std::string& url);
	void getSLMListingsCoro(const std::string& url, LLUUID folder_id);
	void getSLMListingCoro(const std::string& url, LLUUID folder_id);
	void createSLMListingCoro(const std::string& url, LLUUID folder_id,
							  const LLSD& data);
	void updateSLMListingCoro(const std::string& url, LLUUID folder_id,
							  LLUUID version_id, bool is_listed,
							  const LLSD& data);
	void associateSLMListingCoro(const std::string& url, LLUUID folder_id,
								 LLUUID source_folder_id, const LLSD& data);
	void deleteSLMListingCoro(const std::string& url, LLUUID folder_id);

	static void idleCallback(void* userdata);

private:
	LLCore::HttpOptions::ptr_t	mHttpOptions;
	LLCore::HttpHeaders::ptr_t	mHttpHeaders;

	// Handling Marketplace connection and inventory connection
	S32							mMarketPlaceStatus;
	LLInventoryObserver*		mInventoryObserver;
	status_updated_signal_t*	mStatusUpdatedSignal;

	// If true, stock count value need to be updated at the next check
	bool						mDirtyCount;

	// Update data
	S32							mMarketPlaceDataFetched;
	uuid_list_t					mPendingUpdateSet;

	// Listing folders waiting for validation
	typedef fast_hmap<LLUUID, S32> waiting_list_t;
	waiting_list_t				mValidationWaitingList;
	uuid_list_t					mPendingValidations;

	// The cache of SLM data.
	// Notes:
	// * The mListingFolderId is used as a key to this map. It could therefore
	// be taken off the LLMarketplaceTuple objects themselves.
	// * The SLM DB however uses mListingId as its primary key and it shows in
	// its API. In the viewer though, the mListingFolderId is what we use to
	// grab an inventory record.
	typedef fast_hmap<LLUUID, LLMarketplaceTuple> marketplace_items_list_t;
	marketplace_items_list_t	mMarketplaceItems;

	// We need a list (version folder -> listing folder) because such reverse
	// lookups are frequent
	typedef fast_hmap<LLUUID, LLUUID> version_folders_list_t;
	version_folders_list_t		mVersionFolders;
};

// computeStockCount() return error code
constexpr S32 COMPUTE_STOCK_INFINITE = -1;
constexpr S32 COMPUTE_STOCK_NOT_EVALUATED = -2;

// Purely static class used as an interface to the Marketplace Listings API
class LLMarketplace
{
	LLMarketplace() = delete;
	~LLMarketplace() = delete;

protected:
	LOG_CLASS(LLMarketplace);

public:
	static void setup(bool warn = false);
	static void checkMerchantStatus();
	static bool connected();

	LL_INLINE static const LLUUID& getMPL() 	{ return sMarketplaceListingId; }

	static bool contains(const LLUUID& item_id);
	static S32 depthNesting(const LLUUID& item_id);
	static LLUUID nestedParentId(const LLUUID& item_id, S32 depth);
	static S32 computeStockCount(const LLUUID& cat_id,
								 bool force_count = false);

	static void updateFolderHierarchy(const LLUUID& cat_id);
	static void updateCategory(const LLUUID& cur_uuid,
							   bool perform_consistency_enforcement = true);
	static void updateAllCounts(const LLUUID& cat_id);
	static void updateAllCounts();	// Called by the inventory floater

	typedef boost::function<void(std::string& validation_message,
								 S32 depth,
								 LLError::ELevel log_level)> validation_callback_t;
	static bool validateListings(LLViewerInventoryCategory* cat,
								 validation_callback_t cb = NULL,
								 bool fix_hierarchy = true, S32 depth = -1);

	static bool processUpdateNotification(const LLSD& data);

	// Inventory model action

	static bool updateIfListed(const LLUUID& folder_id,
							   const LLUUID& parent_id);

	// Inventory bridge information

	static void inventoryContextMenu(LLFolderBridge* folder,
									 const LLUUID& id, U32 flags,
									 std::vector<std::string>& items,
									 std::vector<std::string>& disabled_items);

	static std::string rootFolderLabelSuffix();
	static std::string folderLabelSuffix(const LLUUID& cat_id);
	static bool isFolderActive(const LLUUID& cat_id);

	static bool hasPermissionsForSale(LLViewerInventoryCategory* cat,
									  std::string& error_msg);

	static bool canMoveItemInto(const LLViewerInventoryCategory* root_folder,
								LLViewerInventoryCategory* dest_folder,
								LLViewerInventoryItem* inv_item,
								std::string& tooltip_msg,
								S32 bundle_size = 1,
								bool from_paste = false);
	static bool canMoveFolderInto(const LLViewerInventoryCategory* root_folder,
								  LLViewerInventoryCategory* dest_folder,
								  LLViewerInventoryCategory* inv_cat,
								  std::string& tooltip_msg,
								  S32 bundle_size = 1,
								  bool from_paste = false);

	// Inventory bridge actions

	static bool moveItemInto(LLViewerInventoryItem* inv_item,
							 const LLUUID& dest_folder, bool copy = false);
	static bool moveFolderInto(LLViewerInventoryCategory* inv_cat,
							   const LLUUID& dest_folder, bool copy = false,
							   bool move_no_copy_items = false);
	static void updateMovedFrom(const LLUUID& from_folder_uuid,
								const LLUUID& cat_id = LLUUID::null);

	static void getListing(const LLUUID& folder_id);
	static void createListing(const LLUUID& folder_id);
	static void clearListing(const LLUUID& folder_id);
	static void editListing(const LLUUID& folder_id);
	static void listFolder(const LLUUID& folder_id, bool list = true);
	static void activateFolder(const LLUUID& folder_id, bool activate = true);

private:
	static void initializeCallback();

	static void gatherMessage(std::string& message, S32 depth,
							  LLError::ELevel log_level);

private:
	static LLUUID		sMarketplaceListingId;
	static std::string	sMessage;
};

#endif // LL_LLMARKETPLACEFUNCTIONS_H
