/**
 * @file llviewerinventory.h
 * @brief Declaration of the inventory bits that only used on the viewer.
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

#ifndef LL_LLVIEWERINVENTORY_H
#define LL_LLVIEWERINVENTORY_H

#include "llinitdestroyclass.h"	// Also includes "boost/function.hpp"
#include "llinventory.h"
#include "llframetimer.h"
#include "llsettingstype.h"
#include "llwearabletype.h"

class LLInventoryModel;
class LLViewerInventoryCategory;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLViewerInventoryItem
//
// An inventory item represents something that the current user has in
// their inventory.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLViewerInventoryItem final : public LLInventoryItem
{
protected:
	LOG_CLASS(LLViewerInventoryItem);

public:
	typedef std::vector<LLPointer<LLViewerInventoryItem> > item_array_t;

protected:
	~LLViewerInventoryItem() override = default;

public:
	// LLInventoryItem / LLInventoryObject overrides
	LLAssetType::EType getType() const override;
	const LLUUID& getAssetUUID() const override;
	const std::string& getName() const override;
	const LLPermissions& getPermissions() const override;
	const LLUUID& getCreatorUUID() const override;
	const std::string& getDescription() const override;
	const LLSaleInfo& getSaleInfo() const override;
	const LLUUID& getThumbnailUUID() const override;
	LLInventoryType::EType getInventoryType() const override;
	U32 getFlags() const override;

	LL_INLINE LLViewerInventoryItem* asViewerInventoryItem() override
	{
		return this;
	}

	LL_INLINE const LLViewerInventoryItem* asViewerInventoryItem() const override
	{
		return this;
	}

	// New virtual methods
	virtual S32 getSubType() const;
	virtual bool isWearableType() const;
	virtual LLWearableType::EType getWearableType() const;
	virtual bool isSettingsType() const;
	virtual LLSettingsType::EType getSettingsType() const;

	// Construct a complete viewer inventory item
	LLViewerInventoryItem(const LLUUID& item_id, const LLUUID& parent_id,
						  const LLPermissions& permissions,
						  const LLUUID& asset_id, LLAssetType::EType type,
						  LLInventoryType::EType inv_type,
						  const std::string& name,
						  const std::string& desc,
						  const LLSaleInfo& sale_info,
						  U32 flags, time_t creation_date_utc);

	// Construct a viewer inventory item which has the minimal amount of
	// information to use in the UI.
	LLViewerInventoryItem(const LLUUID& item_id, const LLUUID& parent_id,
						  const std::string& name,
						  LLInventoryType::EType inv_type);

	// Construct an invalid and incomplete viewer inventory item. Useful for
	// unpacking or importing or what have you.
	// *NOTE: it is important to call setComplete() if you expect the
	// operations to provide all necessary information.
	LLViewerInventoryItem();

	// Create a copy of an inventory item from a pointer to another item.
	// Note: Because InventoryItems are ref counted, reference copy (a = b) is
	// prohibited
	LLViewerInventoryItem(const LLViewerInventoryItem* other);
	LLViewerInventoryItem(const LLInventoryItem* other);

	// LLInventoryItem override
	void copyItem(const LLInventoryItem* other) override;
	// New method
	void copyViewerItem(const LLViewerInventoryItem* other);

	// Construct a new clone of this item: it creates a new viewer inventory
	// item using the copy constructor, and returns it. It is up to the caller
	// to delete (unref) the item.
	void cloneViewerItem(LLPointer<LLViewerInventoryItem>& newitem) const;

	// LLInventoryObject overrides
	void updateParentOnServer(bool restamp) const override;
	void updateServer(bool is_new) const override;

	void fetchFromServer() const;

	// LLInventoryItem overrides
	void packMessage(LLMessageSystem* msg) const override;
	bool unpackMessage(LLMessageSystem* msg, const char* block,
					   S32 block_num = 0) override;
	// New virtual method
	virtual bool unpackMessage(LLSD item);
	
	// LLInventoryObject overrides
	bool importLegacyStream(std::istream& input_stream) override;

	// New methods
	LL_INLINE bool isFinished() const			{ return mIsComplete; }
	LL_INLINE void setComplete(bool complete)	{ mIsComplete = complete; }

	virtual void setTransactionID(const LLTransactionID& transaction_id);

	struct comparePointers
	{
		LL_INLINE bool operator()(const LLPointer<LLViewerInventoryItem>& a,
								  const LLPointer<LLViewerInventoryItem>& b)
		{
			return a->getName().compare(b->getName()) < 0;
		}
	};

	LL_INLINE LLTransactionID getTransactionID() const
	{
		return mTransactionID;
	}

	// true if the baseitem this points to doesn't exist in memory.
	bool getIsBrokenLink() const;

	LLViewerInventoryItem* getLinkedItem() const;
	LLViewerInventoryCategory* getLinkedCategory() const;

protected:
	LLTransactionID	mTransactionID;
	bool			mIsComplete;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLViewerInventoryCategory
//
// An instance of this class represents a category of inventory items. Users
// come with a set of default categories, and can create new ones as needed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLViewerInventoryCategory final : public LLInventoryCategory
{
protected:
	LOG_CLASS(LLViewerInventoryCategory);

public:
	typedef std::vector<LLPointer<LLViewerInventoryCategory> > cat_array_t;

protected:
	~LLViewerInventoryCategory() override = default;

public:
	LLViewerInventoryCategory(const LLUUID& uuid, const LLUUID& parent_uuid,
							  LLFolderType::EType preferred_type,
							  const std::string& name,
							  const LLUUID& owner_id);
	LLViewerInventoryCategory(const LLUUID& owner_id);
	// Create a copy of an inventory category from a pointer to another category
	// Note: Because InventoryCategorys are ref counted, reference copy (a = b)
	// is prohibited
	LLViewerInventoryCategory(const LLViewerInventoryCategory* other);
	void copyViewerCategory(const LLViewerInventoryCategory* other);

	LL_INLINE LLViewerInventoryCategory* asViewerInventoryCategory() override
	{
		return this;
	}

	LL_INLINE const LLViewerInventoryCategory* asViewerInventoryCategory() const override
	{
		return this;
	}

	void updateParentOnServer(bool restamp_children) const override;
	void updateServer(bool is_new) const override;

	LL_INLINE const LLUUID& getOwnerID() const	{ return mOwnerID; }

	// Version handling
	enum { VERSION_UNKNOWN = -1, VERSION_INITIAL = 1 };
	LL_INLINE void setVersion(S32 version)		{ mVersion = version; }
	LL_INLINE void setVersionUnknown()			{ mVersion = VERSION_UNKNOWN; }
	LL_INLINE void setVersionInitial()			{ mVersion = VERSION_INITIAL; }
	LL_INLINE S32 getVersion() const			{ return mVersion; }
	LL_INLINE bool isVersionUnknown() const		{ return mVersion == VERSION_UNKNOWN; }
	LL_INLINE bool isVersionInitial() const		{ return mVersion == VERSION_INITIAL; }

	// Returns true if a fetch was issued (but not necessarily in progress).
	bool fetch();

	typedef enum : U32
	{
		FETCH_NONE = 0,
		FETCH_NORMAL,
		FETCH_RECURSIVE,
	} EFetchType;

	void setFetching(U32 type);
	U32 getFetching();

	// Returns false when the category is not protected (not at root, or not
	// of a protected type, or not bearing the default name of the protected
	// type). Returns true for genuinely protected categories.
	bool isProtected() const;

	// Returns false when the category is not unique (FT_NONE type, or not at
	// root, or not bearing the default name of its special type). Returns true
	// for genuinely unique categories. Note that for the Cool VL Viewer, a
	// unique folder may nonetheless be unprotected (i.e. isProtected() may
	// return false while isUnique() returns true), which is the case for all
	// v2+ folder types considered as dispensable. HB
	bool isUnique() const;

	// Used to help make caching more robust. For example, if someone is
	// getting 4 packets but logs out after 3, the viewer may never know the
	// cache is wrong.
	enum { DESCENDENT_COUNT_UNKNOWN = -1 };
	LL_INLINE S32 getDescendentCount() const	{ return mDescendentCount; }
	LL_INLINE void setDescendentCount(S32 n)	{ mDescendentCount = n; }

	LL_INLINE bool isDescendentCountUnknown() const
	{
		return mDescendentCount == DESCENDENT_COUNT_UNKNOWN;
	}

	// How many descendents do we currently have information for in the
	// InventoryModel ?
	S32 getViewerDescendentCount() const;

	LLSD exportLLSD() const;
	bool importLLSD(const LLSD& cat_data);

	// Returns true if the category object will accept the incoming item
	bool acceptItem(LLInventoryItem* itemp);

	// LLInventoryItem overrides
	void packMessage(LLMessageSystem* msg) const override;
	void unpackMessage(LLMessageSystem* msg, const char* block,
					   S32 block_num = 0) override;
	// New virtual method
	virtual bool unpackMessage(const LLSD& category);

protected:
	LLUUID			mOwnerID;
	LLFrameTimer	mDescendentsRequested;
	S32				mVersion;
	S32				mDescendentCount;
	U32				mFetching;
};

class LLInventoryCallback : public LLRefCount
{
public:
	virtual void fire(const LLUUID& inv_item) = 0;
};

class ActivateGestureCallback final : public LLInventoryCallback
{
public:
	void fire(const LLUUID& inv_item) override;
};

void doInventoryCb(LLPointer<LLInventoryCallback> cb, LLUUID id);

typedef boost::function<void(const LLUUID&)> inventory_func_t;
typedef boost::function<void()> nullary_inv_func_t;

void no_inv_op(const LLUUID&);	// A do-nothing inventory_func_t
void no_op();					// A do-nothing nullary_inv_func_t.

// Shim between inventory callback and boost function/callable
class LLBoostFuncInventoryCallback : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLBoostFuncInventoryCallback);

public:
	LLBoostFuncInventoryCallback(inventory_func_t fire_fn = no_inv_op,
								 nullary_inv_func_t destroy_fn = no_op)
	:	mFireFunc(fire_fn),
		mDestroyFunc(destroy_fn)
	{
	}

	void fire(const LLUUID& item_id) override
	{
		mFireFunc(item_id);
	}

	// virtual
	~LLBoostFuncInventoryCallback() override
	{
		mDestroyFunc();
	}

private:
	inventory_func_t	mFireFunc;
	nullary_inv_func_t	mDestroyFunc;
};

class LLInventoryCallbackManager final
:	public LLDestroyClass<LLInventoryCallbackManager>
{
	friend class LLDestroyClass<LLInventoryCallbackManager>;

protected:
	LOG_CLASS(LLInventoryCallbackManager);

public:
	LLInventoryCallbackManager();
	~LLInventoryCallbackManager();

	void fire(U32 callback_id, const LLUUID& item_id);
	U32 registerCB(LLPointer<LLInventoryCallback> cb);

private:
	static void destroyClass();

private:
	typedef std::map<U32, LLPointer<LLInventoryCallback> > callback_map_t;
	callback_map_t						mMap;
	U32									mLastCallback;
	static LLInventoryCallbackManager*	sInstance;

public:
	LL_INLINE static bool instanceExists()		{ return sInstance != NULL; }
};
extern LLInventoryCallbackManager gInventoryCallbacks;

#define NO_INV_SUBTYPE 0
#define NULL_INV_CB LLPointer<LLInventoryCallback>(NULL)

void update_folder_cb(const LLUUID& folder_id);

// Helper function which creates an item with a good description, updates the
// inventory, updates the server, and pushes the inventory update out to other
// observers.
void create_new_item(const std::string& name, const LLUUID& parent_id,
					 LLAssetType::EType asset_type,
					 LLInventoryType::EType inv_type,
					 U32 next_owner_perm,
					 std::string desc = LLStringUtil::null);

void create_inventory_item(const LLUUID& parent_id,
						   const LLTransactionID& transaction_id,
						   const std::string& name, const std::string& desc,
						   LLAssetType::EType asset_type,
						   LLInventoryType::EType inv_type, U8 sub_type,
						   U32 next_owner_perm,
						   LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

// Securely creates a new inventory item by copying from another.
void copy_inventory_item(const LLUUID& current_owner,
						 const LLUUID& item_id,
						 const LLUUID& parent_id,
						 const std::string& new_name = LLStringUtil::null,
						 LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void copy_inventory_category(LLInventoryModel* model,
							 LLViewerInventoryCategory* cat,
							 const LLUUID& parent_id,
							 const LLUUID& root_copy_id = LLUUID::null,
							 bool move_no_copy_items = false);

void link_inventory_object(const LLUUID& parent_id,
						   LLPointer<LLInventoryObject> baseobj,
						   LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void link_inventory_object(const LLUUID& parent_id,
						   const LLUUID& id,
						   LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void link_inventory_array(const LLUUID& parent_id,
						  LLInventoryObject::object_list_t& baseobj_array,
						  LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

// HB: I kept this function (and added AIS3 support to it) because it is
// faster, easier and reliabler to be able to pass at creation time the new
// link item description (containing the layer info) of links created for
// wearables than to change the description of the newly created link (with
// link_inventory_object() the link desc always matches the linked item desc)
// in a callback after it has been created...
void link_inventory_item(const LLUUID& item_id, const LLUUID& parent_id,
						 const std::string& new_description,
						 const LLAssetType::EType asset_type,
						 LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void move_inventory_item(const LLUUID& item_id, const LLUUID& parent_id,
						 const std::string& new_name,
						 LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

// Returns true when the UUIDs held in inv_items correspond to inventory items
// or non-protected sub-categories contained inside the same category (parent),
// or false otherwise (including when inv_items is empty). HB
bool movable_objects_with_same_parent(const uuid_vec_t& inv_items);

// Reparents inventory objects (items or categories) which UUIDs are listed in
// inv_items, to the parent_id category. WARNING: no check is done on the
// former parents of the inventory objects. Returns true when some objects have
// actually been moved. HB
bool reparent_to_folder(const LLUUID& parent_id, uuid_vec_t inv_items);

void copy_inventory_from_notecard(const LLUUID& object_id,
								  const LLUUID& notecard_inv_id,
								  const LLInventoryItem* srcp,
								  U32 callback_id = 0);

void update_inventory_item(LLViewerInventoryItem* update_item,
						   LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void update_inventory_item(const LLUUID& item_id, const LLSD& updates,
						   LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void update_inventory_category(const LLUUID& cat_id, const LLSD& updates,
							   LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void remove_inventory_items(LLInventoryObject::object_list_t& items,
							LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void remove_inventory_item(LLPointer<LLInventoryObject> obj,
						   LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void remove_inventory_item(const LLUUID& item_id,
						   LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void remove_inventory_category(const LLUUID& cat_id,
							   LLPointer<LLInventoryCallback> cb = NULL_INV_CB,
							   bool check_protected = true);

void remove_inventory_object(const LLUUID& object_id,
							 LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void remove_folder_contents(const LLUUID& category,
							LLPointer<LLInventoryCallback> cb);

void slam_inventory_folder(const LLUUID& folder_id, const LLSD& contents,
						   LLPointer<LLInventoryCallback> cb);

void purge_descendents_of(const LLUUID& cat_id,
						  LLPointer<LLInventoryCallback> cb = NULL_INV_CB);

void rename_category(LLInventoryModel* modelp, const LLUUID& cat_id,
					 const std::string& new_name);

bool get_is_category_renameable(const LLInventoryModel* modelp,
								const LLUUID& id);

bool get_is_item_worn(const LLUUID& id, bool include_gestures = true);

S32 get_folder_levels(LLInventoryCategory* catp);

S32 get_folder_path_length(const LLUUID& ancestor_id,
						   const LLUUID& descendant_id);

S32 count_descendants_items(const LLUUID& cat_id);

LLUUID get_calling_card_buddy_id(LLViewerInventoryItem* itemp);

void move_or_copy_item_from_object(const LLUUID& dest_cat_id,
								   const LLUUID& object_id,
								   const LLUUID& item_id,
								   LLPointer<LLInventoryCallback> cb =
										NULL_INV_CB);

#undef NULL_INV_CB

#endif // LL_LLVIEWERINVENTORY_H
