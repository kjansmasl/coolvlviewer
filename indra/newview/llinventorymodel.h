/**
 * @file llinventorymodel.h
 * @brief LLInventoryModel class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLINVENTORYMODEL_H
#define LL_LLINVENTORYMODEL_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "boost/optional.hpp"

#include "llassettype.h"
#include "llcorehttpheaders.h"
#include "llcorehttpoptions.h"
#include "llcorehttprequest.h"
#include "hbfastmap.h"
#include "llfoldertype.h"
#include "llpermissionsflags.h"
#include "llstring.h"
#include "lluuid.h"

#include "llviewerinventory.h"

class LLInventoryCategory;
class LLInventoryCollectFunctor;
class LLInventoryItem;
class LLInventoryObject;
class LLMessageSystem;
class LLViewerInventoryItem;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryObserver
//
// This class is designed to be a simple abstract base class which can
// relay messages when the inventory changes.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryObserver
{
public:
	// This enumeration is a way to refer to what changed in a more human
	// readable format. You can mask the value provided by changed() to see if
	// the observer is interested in the change.
	enum
	{
		NONE = 0,
		LABEL = 1,				// Name changed
		INTERNAL = 2,			// Internal change, eg, asset UUID different
		ADD = 4,				// Something added
		REMOVE = 8,				// Something deleted
		STRUCTURE = 16,			// Structural change, eg, item or folder moved
		CALLING_CARD = 32,		// Online, grant status, cancel, etc change
		REBUILD = 128,			// Icon changed, for example. Rebuild all.
		CREATE = 512,			// With ADD, item has just been created.
		ALL = 0xffffffff
	};

	virtual ~LLInventoryObserver() = default;

	virtual void changed(U32 mask) = 0;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLInventoryModel
//
// Represents a collection of inventory, and provides efficient ways to access
// that information.
// NOTE: This class could in theory be used for any place where you need
// inventory, though it optimizes for time efficiency - not space efficiency,
// probably making it inappropriate for use on tasks.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryModel
{
protected:
	LOG_CLASS(LLInventoryModel);

public:
	LLInventoryModel();
	~LLInventoryModel();

	void cleanupInventory();

	typedef std::vector<LLPointer<LLViewerInventoryCategory> > cat_array_t;
	typedef std::vector<LLPointer<LLViewerInventoryItem> > item_array_t;

protected:
	// Empty the entire contents
	void empty();

	//--------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------
private:
	// One-time initialization of HTTP system.
	void initHttpRequest();

public:
	// The inventory model usage is sensitive to the initial construction of
	// the model
	bool isInventoryUsable() const;

	//--------------------------------------------------------------------
	// Root Folders
	//--------------------------------------------------------------------
	// The following are set during login with data from the server
	void setRootFolderID(const LLUUID& id);
	void setLibraryOwnerID(const LLUUID& id);
	void setLibraryRootFolderID(const LLUUID& id);

	LL_INLINE const LLUUID& getRootFolderID() const	{ return mRootFolderID; }

	LL_INLINE const LLUUID& getLibraryOwnerID() const
	{
		return mLibraryOwnerID;
	}

	LL_INLINE const LLUUID& getLibraryRootFolderID() const
	{
		return mLibraryRootFolderID;
	}

	// Commonly used folders. HB
	const LLUUID& getTrashID();
	const LLUUID& getLostAndFoundID();

	//--------------------------------------------------------------------
	// Structure
	//--------------------------------------------------------------------
	// Methods to load up inventory skeleton & meat. These are used during
	// authentication. Returns true if everything parsed.
	bool loadSkeleton(const LLSD& options, const LLUUID& owner_id);

	// Brute force method to rebuild the entire parent-child relations.
	void buildParentChildMap();

	std::string getCacheFileName(const LLUUID& agent_id);

	// Call on logout to save a terse representation.
	void cache(const LLUUID& parent_folder_id, const LLUUID& agent_id);

	// Consolidates and (re)-creates any missing system folder. May be used as
	// a menu callback.
	static void checkSystemFolders(void* dummy_data_ptr = NULL);

	//--------------------------------------------------------------------
	// Descendents
	//--------------------------------------------------------------------
	// Make sure we have the descendents in the structure.
	void fetchDescendentsOf(const LLUUID& folder_id);

	// Returns the direct descendents of the id provided. Sets passed in values
	// to NULL if the call fails.
	// NOTE: the array provided points straight into the guts of this object,
	// and should only be used for read operations, since modifications may
	// invalidate the internal state of the inventory.
	void getDirectDescendentsOf(const LLUUID& cat_id, cat_array_t*& categories,
								item_array_t*& items) const;

	enum
	{
		EXCLUDE_TRASH = false,
		INCLUDE_TRASH = true
	};

	// Starting with the object specified, add its descendents to the array
	// provided, but do not add the inventory object specified by id. There is
	// no guaranteed order.
	// NOTE: Neither array will be erased before adding objects to it. Do not
	// store a copy of the pointers collected - use them, and collect them
	// again later if you need to reference the same objects.
	void collectDescendents(const LLUUID& id, cat_array_t& categories,
							item_array_t& items, bool include_trash);

	void collectDescendentsIf(const LLUUID& id, cat_array_t& categories,
							  item_array_t& items, bool include_trash,
							  LLInventoryCollectFunctor& add);

	// Collect all items in inventory that are linked to item_id. Assumes
	// item_id is itself not a linked item.
	item_array_t collectLinkedItems(const LLUUID& item_id,
									const LLUUID& start_folder = LLUUID::null);

	// Checks if one object has a parent chain up to the category specified by
	// UUID.
	bool isObjectDescendentOf(const LLUUID& obj_id,
							  const LLUUID& cat_id) const;

	// Returns true when inv_object_id is in trash; forces the creation of the
	// Trash folder when absent. HB
	LL_INLINE bool isInTrash(const LLUUID& inv_object_id)
	{
		return isObjectDescendentOf(inv_object_id, getTrashID());
	}

	// Returns true when inv_object_id is in the current outfit folder; does
	// *not* force the creation of the COF when absent. HB
	bool isInCOF(const LLUUID& inv_object_id) const;

	// Returns true when inv_object_id is in market place folder; does
	// *not* force the creation of the Marketplace Listings folder when
	// absent. HB
	bool isInMarketPlace(const LLUUID& inv_object_id) const;

	//--------------------------------------------------------------------
	// Find
	//--------------------------------------------------------------------
	// Returns the UUID of the category that specifies 'type' as what it
	// defaults to containing. The category is not necessarily only for that
	// type. NOTE: If create_folder is true, this will create a new inventory
	// category on the fly if one does not exist.
	LLUUID findCategoryUUIDForType(LLFolderType::EType preferred_type,
								   bool create_folder = true);

	// Returns the UUID of the category that specifies 'type' as what its
	// choosen (user defined) folder. If the user-defined folder does not
	// exist, it is equivalent to calling findCategoryUUIDForType(type, true)
	LLUUID findChoosenCategoryUUIDForType(LLFolderType::EType preferred_type);

	// Get first descendant of the child object under the specified parent
	const LLViewerInventoryCategory* getFirstDescendantOf(const LLUUID& parent_id,
														  const LLUUID& obj_id) const;

	// Get the object by id. Returns NULL if not found.
	// NOTE: Use the pointer returned for read operations - do not modify the
	// object values in place or you will break stuff.
	LLInventoryObject* getObject(const LLUUID& id) const;

	// Get the item by id. Returns NULL if not found.
	// NOTE: Use the pointer for read operations - use the updateItem() method
	// to actually modify values.
	LLViewerInventoryItem* getItem(const LLUUID& id) const;

	// Get the category by id. Returns NULL if not found.
	// NOTE: Use the pointer for read operations - use the updateCategory()
	// method to actually modify values.
	LLViewerInventoryCategory* getCategory(const LLUUID& id) const;

	// Get the inventoryID that this item points to, else just return item_id
	const LLUUID& getLinkedItemID(const LLUUID& object_id) const;

	// Copies the contents of all folders of type "type" into folder "id" and
	// delete/purge the empty folders. When is_root_cat is true, also makes
	// sure that id is parented to the root folder. Note: the trash is also
	// emptied in the process.
    void consolidateForType(const LLUUID& id, LLFolderType::EType type,
							bool is_root_cat = true);

protected:
	// Internal method which looks for a category with the specified
	// preferred type. Returns LLUUID::null if not found
 	LLUUID findCatUUID(LLFolderType::EType preferred_type);

	//--------------------------------------------------------------------
	// Count
	//--------------------------------------------------------------------
public:
	// Return the number of items or categories
	S32 getItemCount() const;
	S32 getCategoryCount() const;

	//
	// Mutators
	//
	// Change an existing item with a matching item_id or add the item to the
	// current inventory. Returns the change mask generated by the update. No
	// notification will be sent to observers. This method will only generate
	// network traffic if the item had to be reparented.
	// NOTE: In usage, you will want to perform cache accounting operations in
	// LLInventoryModel::accountForUpdate() or
	// LLViewerInventoryItem::updateServer() before calling this method.
	U32 updateItem(const LLViewerInventoryItem* item, U32 mask = 0);

	// Change an existing item with the matching id or add the category. No
	// notification will be sent to observers. This method will only generate
	// network traffic if the item had to be reparented.
	// NOTE: In usage, you will want to perform cache accounting operations in
	// accountForUpdate() or LLViewerInventoryCategory::updateServer() before
	// calling this method.
	void updateCategory(const LLViewerInventoryCategory* catp, U32 mask = 0);

	// Move the specified object id to the specified category and update the
	// internal structures. No cache accounting, observer notification, or
	// server update is performed.
	void moveObject(const LLUUID& object_id, const LLUUID& cat_id);

	// Migrated from llinventorybridge.cpp
	void changeItemParent(LLViewerInventoryItem* itemp,
						  const LLUUID& new_parent_id, bool restamp);

	// Migrated from llinventorybridge.cpp
	void changeCategoryParent(LLViewerInventoryCategory* catp,
							  const LLUUID& new_parent_id, bool restamp);

	void checkTrashOverflow();

	//--------------------------------------------------------------------
	// Delete
	//--------------------------------------------------------------------

	// Update model after an item is confirmed as removed from server. Works
	// for categories or items.
	void onObjectDeletedFromServer(const LLUUID& item_id,
								   bool fix_broken_links = true,
								   bool update_parent_version = true,
								   bool do_notify_observers = true);

	// Update model after all descendents removed from server.
	void onDescendentsPurgedFromServer(const LLUUID& object_id,
									   bool fix_broken_links = true);

#if 0 // Do not appear to be used currently.
	// Update model after an existing item gets updated on server.
	void onItemUpdated(const LLUUID& item_id, const LLSD& updates,
					   bool update_parent_version);

	// Update model after an existing category gets updated on server.
	void onCategoryUpdated(const LLUUID& cat_id, const LLSD& updates);
#endif

	// Delete a particular inventory object by ID. Will purge one object from
	// the internal data structures, maintaining a consistent internal state.
	// No cache accounting or server update is performed.
	void deleteObject(const LLUUID& id, bool fix_broken_links = true,
					  bool do_notify_observers = true);

	// Moves item item_id to Trash
	void removeItem(const LLUUID& item_id);

	// Moves category category_id to Trash
	void removeCategory(const LLUUID& category_id);

	//--------------------------------------------------------------------
	// Creation
	//--------------------------------------------------------------------

	// Creates a new category. If you want to use the default name based on
	// type, pass an empty string as the 'name' parameter.
	// AIS3 will be used to create the category if available and enabled, else
	// the "CreateInventoryCategory" capability will be used if present (it
	// should exist even in OpenSim) and finally a fall back to the legacy
	// createCategoryUDP() method below is performed when there is no such
	// capability available; in all cases the category Id is transmitted
	// (asynchronously for when AIS3 or the capability were used) via the
	// callback function, provided it is not NULL (you may pass NULL, if you
	// do not need the category UUID to perform other operations with it).
	void createNewCategory(const LLUUID& parent_id,
						   LLFolderType::EType preferred_type,
						   const std::string& name,
						   inventory_func_t callback,
						   const LLUUID& thumbnail_id = LLUUID::null);

	// Same as above, but this method uses the legacy "CreateInventoryFolder"
	// UDP message (see note) and the UUID of the created category is then
	// immediately returned instead of using a callback function.
	// In case of error (unusable inventory or unknown 'preferred_type'), this
	// method returns a null UUID.
	// Note: the "CreateInventoryFolder" UDP message might stop working at some
	//       point in the future in SL; as of 2023-10 the latest SL release
	//       viewer already got rid of its usage in its code. HB
	LLUUID createCategoryUDP(const LLUUID& parent_id,
							 LLFolderType::EType preferred_type,
							 const std::string& name,
							 const LLUUID& thumbnail_id = LLUUID::null);

	void rebuildBrokenLinks();

protected:
	void updateLinkedObjectsFromPurge(const LLUUID& baseobj_id);

	// Internal methods that add inventory and make sure that all of the
	// internal data structures are consistent. These methods should be passed
	// pointers of newly created objects, and the instance will take over the
	// memory management from there.
	void addCategory(LLViewerInventoryCategory* category);
	void addItem(LLViewerInventoryItem* item);

	void createNewCategoryCoro(const std::string& url, const LLSD& data,
							   LLUUID thumb_id, inventory_func_t callback);

	//
	// Category accounting.
	//
public:
	// Represents the number of items added or removed from a category.
	struct LLCategoryUpdate
	{
		LL_INLINE LLCategoryUpdate()
		:	mDescendentDelta(0),
			mChangeVersion(true)
		{
		}

		LL_INLINE LLCategoryUpdate(const LLUUID& category_id, S32 delta,
								   bool change_version = true)
		:	mCategoryID(category_id),
			mDescendentDelta(delta),
			mChangeVersion(change_version)
		{
		}

		LLUUID	mCategoryID;
		S32		mDescendentDelta;
		bool	mChangeVersion;
	};
	typedef std::vector<LLCategoryUpdate> update_list_t;

	// This exists to make it easier to account for deltas in a map.
	struct LLInitializedS32
	{
		LL_INLINE LLInitializedS32()
		:	mValue(0)
		{
		}

		LL_INLINE LLInitializedS32(S32 value)
		:	mValue(value)
		{
		}

		LL_INLINE LLInitializedS32& operator++()	{ ++mValue; return *this; }
		LL_INLINE LLInitializedS32& operator--()	{ --mValue; return *this; }

		S32 mValue;
	};
	typedef fast_hmap<LLUUID, LLInitializedS32> update_map_t;

	// Call these methods when there are category updates, but call them
	// *before* the actual update so the method can do descendent accounting
	// correctly.
	void accountForUpdate(const LLCategoryUpdate& update) const;
	void accountForUpdate(const update_list_t& updates) const;
	void accountForUpdate(const update_map_t& updates) const;

	enum EHasChildren
	{
		CHILDREN_NO,
		CHILDREN_YES,
		CHILDREN_MAYBE
	};

	// Returns (yes/no/maybe) child status of category children.
	EHasChildren categoryHasChildren(const LLUUID& cat_id) const;

	// Returns true if category version is known and theoretical
	// descendents == actual descendents.
	bool isCategoryComplete(const LLUUID& cat_id) const;

	//
	// Notifications
	//
	// Called by the idle loop. Only updates if new state is detected. Call
	// notifyObservers() manually to update regardless of whether state change
	// has been indicated.
	void idleNotifyObservers();

	// Call to explicitly update everyone on a new state.
	void notifyObservers();

	// Allows outsiders to tell the inventory if something has been changed
	// 'under the hood', but outside the control of the inventory. The next
	// notify will include that notification.
	void addChangedMask(U32 mask, const LLUUID& referent);

	LL_INLINE const uuid_list_t& getChangedIDs()	{ return mChangedItemIDs; }
	LL_INLINE const uuid_list_t& getAddedIDs()		{ return mAddedItemIDs; }

protected:
	// Updates all linked items pointing to this id.
	void addChangedMaskForLinks(const LLUUID& object_id, U32 mask);

	//--------------------------------------------------------------------
	// Observers
	//--------------------------------------------------------------------
public:
	// If the observer is destroyed, be sure to remove it.
	void addObserver(LLInventoryObserver* observerp);
	void removeObserver(LLInventoryObserver* observerp);
	bool containsObserver(LLInventoryObserver* observerp);

	//--------------------------------------------------------------------
	// HTTP Transport
	//--------------------------------------------------------------------

	// HTTP handler for individual item requests (inventory or library).
	// Background item requests are derived from this in the background
	// inventory system.  All folder requests are also located there but have
	// their own handler derived from HttpHandler.
	class FetchItemHttpHandler : public LLCore::HttpHandler
	{
	protected:
		LOG_CLASS(FetchItemHttpHandler);

	public:
		FetchItemHttpHandler(const LLSD& request_sd);

		FetchItemHttpHandler(const FetchItemHttpHandler&) = delete;
		void operator=(const FetchItemHttpHandler&) = delete;

		void onCompleted(LLCore::HttpHandle handle,
						 LLCore::HttpResponse* response) override;

	private:
		void processData(LLSD& body, LLCore::HttpResponse* response);
		void processFailure(LLCore::HttpStatus status,
							LLCore::HttpResponse* response);
		void processFailure(const char* const reason,
							LLCore::HttpResponse* response);

	protected:
		LLSD mRequestSD;
	};

	// Invoke handler completion method (onCompleted) for all requests that are
	// ready.
	void handleResponses(bool foreground);

	// Request an inventory HTTP operation to either the foreground or
	// background processor. These are actually the same service queue but the
	// background requests are seviced more slowly effectively de-prioritizing
	// new requests.
	LLCore::HttpHandle requestPost(bool foreground, const std::string& url,
								   const LLSD& body,
								   const LLCore::HttpHandler::ptr_t& handler,
								   // This must be a static const char* !
								   const char* message);

	//--------------------------------------------------------------------
	// Callbacks
	//--------------------------------------------------------------------

	// Message handling functionality
	static void registerCallbacks(LLMessageSystem* msg);

	//--------------------------------------------------------------------
	// File I/O
	//--------------------------------------------------------------------
protected:
	static bool loadFromFile(const std::string& filename,
							 cat_array_t& categories, item_array_t& items,
							 uuid_list_t& cats_to_update,
							 bool& is_cache_obsolete);
	static bool saveToFile(const std::string& filename,
						   const cat_array_t& categories,
						   const item_array_t& items);

	//--------------------------------------------------------------------
	// Message handling functionality
	//--------------------------------------------------------------------
public:
	static void processUpdateCreateInventoryItem(LLMessageSystem* msg, void**);
	static void removeInventoryItem(LLUUID agent_id, LLMessageSystem* msg,
									const char* msg_label);
	static void processRemoveInventoryItem(LLMessageSystem* msg, void**);
	static void processUpdateInventoryFolder(LLMessageSystem* msg, void**);
	static void removeInventoryFolder(LLUUID agent_id, LLMessageSystem* msg);
	static void processRemoveInventoryFolder(LLMessageSystem* msg, void**);
	static void processRemoveInventoryObjects(LLMessageSystem* msg, void**);
	static void processSaveAssetIntoInventory(LLMessageSystem* msg, void**);
	static void processBulkUpdateInventory(LLMessageSystem* msg, void**);
	static void processInventoryDescendents(LLMessageSystem* msg, void**);
	static void processMoveInventoryItem(LLMessageSystem* msg, void**);
	static void processFetchInventoryReply(LLMessageSystem* msg, void**);

protected:
	bool messageUpdateCore(LLMessageSystem* msg, bool do_accounting,
						   U32 mask = 0);

	cat_array_t* getUnlockedCatArray(const LLUUID& id);
	item_array_t* getUnlockedItemArray(const LLUUID& id);

public:
	//--------------------------------------------------------------------
	// Other
	//--------------------------------------------------------------------

	// Generates a string containing the path to the item specified by
	// item_id.
	void appendPath(const LLUUID& id, std::string& path);

	//--------------------------------------------------------------------
	// Debugging
	//--------------------------------------------------------------------
	void dumpInventory();

#if LL_HAS_ASSERT
	void lockDirectDescendentArrays(const LLUUID& cat_id, cat_array_t*& cats,
									item_array_t*& items);
	void unlockDirectDescendentArrays(const LLUUID& cat_id);

private:
	fast_hmap<LLUUID, bool>						mCategoryLock;
	fast_hmap<LLUUID, bool>						mItemLock;
#endif

	//--------------------------------------------------------------------
	// Member variables
	//--------------------------------------------------------------------
private:
	LLUUID										mRootFolderID;
	LLUUID										mLibraryRootFolderID;
	LLUUID										mLibraryOwnerID;
	// Often used and *irremovable* folder Ids, cached for speed. HB
	LLUUID										mTrashID;
	LLUUID										mLostAndFoundID;

	// Cache for recent lookups
	mutable LLPointer<LLViewerInventoryItem>	mLastItem;

	// Observers
	typedef std::set<LLInventoryObserver*> observer_list_t;
	observer_list_t								mObservers;

	// Usual plumbing for LLCore:: HTTP operations.
	LLCore::HttpRequest*						mHttpRequestFG;
	LLCore::HttpRequest*						mHttpRequestBG;
	LLCore::HttpOptions::ptr_t					mHttpOptions;
	LLCore::HttpHeaders::ptr_t					mHttpHeaders;
	LLCore::HttpRequest::policy_t				mHttpPolicyClass;

	// Information for tracking the actual inventory. We index this information
	// in a lot of different ways so we can access the inventory using several
	// different identifiers. mCategoryMap and mItemMap store uuid->object
	// mappings.

	typedef fast_hmap<LLUUID, LLPointer<LLViewerInventoryCategory> > cat_map_t;
	cat_map_t									mCategoryMap;

	typedef fast_hmap<LLUUID, LLPointer<LLViewerInventoryItem> > item_map_t;
	item_map_t									mItemMap;

	// This last set of indices is used to map parents to children.
	typedef fast_hmap<LLUUID, cat_array_t*> parent_cat_map_t;
	parent_cat_map_t							mParentChildCategoryTree;

	typedef fast_hmap<LLUUID, item_array_t*> parent_item_map_t;
	parent_item_map_t							mParentChildItemTree;

	// Map of the inventory item UUIDs which got broken link inventory items;
	// the said link items UUIDs are stored in a vector.
	typedef fast_hmap<LLUUID, uuid_vec_t> broken_links_map_t;
	broken_links_map_t							mBrokenLinks;
	// List of links to rebuild after we received the linked item data.
	uuid_vec_t									mLinksRebuildList;

	// Variables used to track what has changed since the last notify.
	uuid_list_t									mChangedItemIDs;
	uuid_list_t									mChangedItemIDsBacklog;
	uuid_list_t									mAddedItemIDs;
	uuid_list_t									mAddedItemIDsBacklog;
	U32											mModifyMask;
	U32											mModifyMaskBacklog;

	// Used to handle an invalid inventory state
	bool										mIsAgentInvUsable;

	// Flag set at notifyObservers() entance and reset at its end, to detect
	// both, bogus recursive calls (should never happen, in theory), and calls
	// to addChangedMask() within an inventory observer called from
	// notifyObservers() (see below), which might happen, sometimes, after
	// an operation that triggered an asynchronous inventory fetch.
	bool										mIsNotifyObservers;

public:
	// Wear all clothing in this transaction
	static LLUUID								sWearNewClothingTransactionID;

	// *HACK: until we can route this info through the instant message
	// hierarchy
	static bool									sWearNewClothing;
};

// a special inventory model for the agent
extern LLInventoryModel gInventory;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryCollectFunctor
//
// Base class for LLInventoryModel::collectDescendentsIf() method which accepts
// an instance of one of these objects to use as the function to determine if
// it should be added. Derive from this class and override the () operator to
// return true if you want to collect the category or item passed in.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryCollectFunctor
{
public:
	virtual ~LLInventoryCollectFunctor() = default;
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item) = 0;

	static bool itemTransferCommonlyAllowed(LLInventoryItem* item);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLAssetIDMatches
//
// This functor finds inventory items pointing to the specified asset
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLAssetIDMatches : public LLInventoryCollectFunctor
{
public:
	LL_INLINE LLAssetIDMatches(const LLUUID& asset_id)
	:	mAssetID(asset_id)
	{
	}

	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override;

protected:
	LLUUID mAssetID;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLLinkedItemIDMatches
//
// This functor finds inventory items linked to the specific inventory id.
// Assumes the inventory id is itself not a linked item.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLLinkedItemIDMatches : public LLInventoryCollectFunctor
{
public:
	LL_INLINE LLLinkedItemIDMatches(const LLUUID& item_id)
	:	mBaseItemID(item_id)
	{
	}

	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override;

protected:
	LLUUID mBaseItemID;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLIsType
//
// Implementation of a LLInventoryCollectFunctor which returns true if the type
// is the type passed in during construction.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLIsType : public LLInventoryCollectFunctor
{
public:
	LL_INLINE LLIsType(LLAssetType::EType type)
	:	mType(type)
	{
	}

	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override;

protected:
	LLAssetType::EType mType;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLIsNotType
//
// Implementation of a LLInventoryCollectFunctor which returns false if the
// type is the type passed in during construction, otherwise false.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLIsNotType : public LLInventoryCollectFunctor
{
public:
	LL_INLINE LLIsNotType(LLAssetType::EType type)
	:	mType(type)
	{
	}

	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override;

protected:
	LLAssetType::EType mType;
};

class LLIsTypeWithPermissions : public LLInventoryCollectFunctor
{
public:
	LL_INLINE LLIsTypeWithPermissions(LLAssetType::EType type,
									  const PermissionBit perms,
									  const LLUUID& agent_id,
									  const LLUUID& group_id)
	:	mType(type),
		mPerm(perms),
		mAgentID(agent_id),
		mGroupID(group_id)
	{
	}

	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override;

protected:
	LLAssetType::EType mType;
	PermissionBit mPerm;
	LLUUID			mAgentID;
	LLUUID			mGroupID;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLBuddyCollector
//
// Simple class that collects calling cards that are not null, and not the
// agent; the card may have been given or recreated. Duplicates are possible.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLBuddyCollector final : public LLInventoryCollectFunctor
{
public:
	LLBuddyCollector() = default;

	bool operator()(LLInventoryCategory*, LLInventoryItem* item) override;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLUniqueBuddyCollector
//
// Simple class that collects calling cards that are not null, and not the
// agent; the card may have been given or recreated. Duplicates are discarded.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLUniqueBuddyCollector final : public LLInventoryCollectFunctor
{
public:
	LLUniqueBuddyCollector() = default;

	bool operator()(LLInventoryCategory*, LLInventoryItem* item) override;

private:
	uuid_list_t mFoundIds;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLParticularBuddyCollector
//
// Simple class that collects calling cards that match a particular UUID
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLParticularBuddyCollector final : public LLInventoryCollectFunctor
{
public:
	LL_INLINE LLParticularBuddyCollector(const LLUUID& id)
	:	mBuddyID(id)
	{
	}

	bool operator()(LLInventoryCategory*, LLInventoryItem* item) override;

protected:
	LLUUID mBuddyID;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLNameCategoryCollector
//
// Collects categories based on case-insensitive match of prefix
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLNameCategoryCollector final : public LLInventoryCollectFunctor
{
public:
	LL_INLINE LLNameCategoryCollector(const std::string& name)
	:	mName(name)
	{
	}

	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override;

protected:
	std::string mName;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLEnvSettingsCollector
//
// Collects environment settings items.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLEnvSettingsCollector final : public LLInventoryCollectFunctor
{
public:
	LLEnvSettingsCollector() = default;

	LL_INLINE bool operator()(LLInventoryCategory*,
							  LLInventoryItem* item) override
	{
		return item && item->getType() == LLAssetType::AT_SETTINGS;
	}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryCompletionObserver
//
// Class which can be used as a base class for doing something when all the
// observed items are locally complete. This class implements the changed()
// method of LLInventoryObserver and declares a new method named done() which
// is called when all watched items have complete information in the inventory
// model.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryCompletionObserver : public LLInventoryObserver
{
public:
	LLInventoryCompletionObserver() = default;

	void changed(U32 mask) override;

	void watchItem(const LLUUID& id);

protected:
	virtual void done() = 0;

protected:
	uuid_vec_t mComplete;
	uuid_vec_t mIncomplete;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryFetchObserver
//
// This class is much like the LLInventoryCompletionObserver, except that it
// handles all the the fetching necessary. Override the done() method to do the
// thing you want.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryFetchObserver : public LLInventoryObserver
{
public:
	LLInventoryFetchObserver() = default;

	void changed(U32 mask) override;

	bool isFinished() const;
	virtual void fetchItems(const uuid_vec_t& ids);
	virtual void done() = 0;

protected:
	uuid_vec_t mComplete;
	uuid_vec_t mIncomplete;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryFetchDescendentsObserver
//
// This class is much like the LLInventoryCompletionObserver, except that it
// handles fetching based on category. Override the done() method to do the
// thing you want.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryFetchDescendentsObserver : public LLInventoryObserver
{
public:
	LLInventoryFetchDescendentsObserver() = default;

	void changed(U32 mask) override;

	void fetchDescendents(const uuid_vec_t& ids);
	bool isFinished() const;

	virtual void done() = 0;

protected:
	bool isCategoryComplete(LLViewerInventoryCategory* cat);
	uuid_vec_t mIncompleteFolders;
	uuid_vec_t mCompleteFolders;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryFetchComboObserver
//
// This class does an appropriate combination of fetch descendents and item
// fetches based on completion of categories and items. Much like the fetch and
// fetch descendents, this will call done() when everything has arrived.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryFetchComboObserver : public LLInventoryObserver
{
public:
	LL_INLINE LLInventoryFetchComboObserver()
	:	mDone(false)
	{
	}

	void changed(U32 mask) override;

	void fetch(const uuid_vec_t& folder_ids, const uuid_vec_t& item_ids);

	virtual void done() = 0;

protected:
	uuid_vec_t mCompleteFolders;
	uuid_vec_t mIncompleteFolders;
	uuid_vec_t mCompleteItems;
	uuid_vec_t mIncompleteItems;
	bool mDone;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryExistenceObserver
//
// This class is used as a base class for doing somethign when all the observed
// item ids exist in the inventory somewhere. You can derive a class from this
// class and implement the done() method to do something useful.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryExistenceObserver : public LLInventoryObserver
{
public:
	LLInventoryExistenceObserver() = default;

	void changed(U32 mask) override;

	void watchItem(const LLUUID& id);

protected:
	virtual void done() = 0;

protected:
	uuid_vec_t mExist;
	uuid_vec_t mMIA;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryAddedObserver
//
// This class is used as a base class for doing something when a new item
// arrives in inventory. It does not watch for a certain UUID, rather it acts
// when anything is added Derive a class from this class and implement the
// done() method to do something useful.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryAddedObserver : public LLInventoryObserver
{
public:
	LLInventoryAddedObserver() = default;

	void changed(U32 mask) override;

	// Only used by copy_inventory_item() for now (if you implement new ways of
	// copying inventory items, you should use this too to avoid a false "added
	// item" positive). HB
	static void registerCopiedItem(const LLUUID& item_id);

protected:
	virtual void done() = 0;

protected:
	uuid_vec_t			mAdded;
	// We keep track of the items we copy from the inventory, so to avoid a
	// false "added item" positive when the server creates the copy (this is
	// not a new item we received from a third party, so it shall not be added
	// to mAdded). HB
	typedef fast_hmap<LLUUID, U32> hashes_map_t;
	static hashes_map_t	sCopiedItemsHashes;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryTransactionObserver
//
// Class which can be used as a base class for doing something when an
// inventory transaction completes.
//
// NOTE: this class is not quite complete. Avoid using unless you fix up its
// functionality gaps.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryTransactionObserver : public LLInventoryObserver
{
public:
	LL_INLINE LLInventoryTransactionObserver(const LLTransactionID& tid)
	:	mTransactionID(tid)
	{
	}

	void changed(U32 mask) override;

protected:
	virtual void done(const uuid_vec_t& folders,
					  const uuid_vec_t& items) = 0;

protected:
	LLTransactionID mTransactionID;
};

#endif // LL_LLINVENTORYMODEL_H
