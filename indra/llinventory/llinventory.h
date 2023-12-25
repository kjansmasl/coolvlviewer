/**
 * @file llinventory.h
 * @brief LLInventoryItem and LLInventoryCategory class declaration.
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

#ifndef LL_LLINVENTORY_H
#define LL_LLINVENTORY_H

#include <functional>
#include <vector>

#include "llassetstorage.h"
#include "llerror.h"
#include "llfoldertype.h"
#include "llinventorytype.h"
#include "llpermissions.h"
#include "llpreprocessor.h"
#include "llrefcount.h"
#include "llsaleinfo.h"
#include "llsd.h"
#include "lluuid.h"
#include "llxmlnode.h"

class LLMessageSystem;
class LLInventoryCategory;
class LLInventoryItem;
class LLViewerInventoryItem;
class LLViewerInventoryCategory;

// Constants for Key field in the task inventory update message
constexpr U8 TASK_INVENTORY_ITEM_KEY = 0;
#if 0	// Not used. HB
constexpr U8 TASK_INVENTORY_ASSET_KEY = 1;
#endif

// Anonymous enumeration to specify a max inventory buffer size for use in
// packBinaryBucket()
enum
{
	MAX_INVENTORY_BUFFER_SIZE = 1024
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Base class for anything in the user's inventory. Handles the common code
// between items and categories.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryObject : public LLRefCount
{
protected:
	LOG_CLASS(LLInventoryObject);

	~LLInventoryObject() override = default;

public:
	typedef std::list<LLPointer<LLInventoryObject> > object_list_t;

	LLInventoryObject();
	LLInventoryObject(const LLUUID& uuid, const LLUUID& parent_uuid,
					  LLAssetType::EType type, const std::string& name);

	LL_INLINE virtual LLInventoryItem* asInventoryItem()
	{
		return NULL;
	}

	LL_INLINE virtual const LLInventoryItem* asInventoryItem() const
	{
		return NULL;
	}

	LL_INLINE virtual LLInventoryCategory* asInventoryCategory()
	{
		return NULL;
	}

	LL_INLINE virtual const LLInventoryCategory* asInventoryCategory() const
	{
		return NULL;
	}

	LL_INLINE virtual LLViewerInventoryItem* asViewerInventoryItem()
	{
		return NULL;
	}

	LL_INLINE virtual const LLViewerInventoryItem* asViewerInventoryItem() const
	{
		return NULL;
	}

	LL_INLINE virtual LLViewerInventoryCategory* asViewerInventoryCategory()
	{
		return NULL;
	}

	LL_INLINE virtual const LLViewerInventoryCategory* asViewerInventoryCategory() const
	{
		return NULL;
	}

	// LLRefCount requires custom copy:
	void copyObject(const LLInventoryObject* otherp);

	// File support. Implemented here so that a minimal information set can be
	// transmitted between simulator and viewer.
	virtual bool importLegacyStream(std::istream& input_stream);
	virtual bool exportLegacyStream(std::ostream& output_stream,
									bool include_asset_key = true) const;

	virtual void updateParentOnServer(bool) const;
	virtual void updateServer(bool) const;

	// Inventory Id that this item points to
	LL_INLINE virtual const LLUUID& getUUID() const			{ return mUUID; }

	// Inventory Id that this item points to, else this item's inventory Id
	// See LLInventoryItem override.
	LL_INLINE virtual const LLUUID& getLinkedUUID() const	{ return mUUID; }

	LL_INLINE const LLUUID& getParentUUID() const			{ return mParentUUID; }

	LL_INLINE virtual const LLUUID& getThumbnailUUID() const
	{
		return mThumbnailUUID;
	}

	LL_INLINE void setThumbnailUUID(const LLUUID& id)		{ mThumbnailUUID = id; }

	LL_INLINE virtual const std::string& getName() const	{ return mName; }
	LL_INLINE virtual LLAssetType::EType getType() const	{ return mType; }


	// To bypass linked items, since llviewerinventory's getType() will return
	// the linked-to item's type instead of this object's type:
	LL_INLINE LLAssetType::EType getActualType() const		{ return mType; }

	LL_INLINE bool getIsLinkType() const					{ return LLAssetType::lookupIsLinkType(mType); }

	LL_INLINE virtual time_t getCreationDate() const		{ return mCreationDate; }

	// Mutators not calling updateServer()
	LL_INLINE void setUUID(const LLUUID& new_uuid)			{ mUUID = new_uuid; }
	LL_INLINE void setParent(const LLUUID& new_parent)		{ mParentUUID = new_parent; }
	LL_INLINE void setType(LLAssetType::EType type)			{ mType = type; }

	virtual void rename(const std::string& new_name);

	// Only stored for items
	LL_INLINE virtual void setCreationDate(time_t utc)		{ mCreationDate = utc; }

	// In-place correction for inventory name string
	static void correctInventoryName(std::string& name);

protected:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variables, counting the 32 bits counter from LLRefCount. HB
	LLAssetType::EType	mType;
	std::string			mName;
	LLUUID				mUUID;
	// Parent category. Root categories have LLUUID::NULL.
	LLUUID				mParentUUID;
	LLUUID				mThumbnailUUID;
	time_t				mCreationDate;	// Seconds since 1970-01-01, UTC
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// A class for an item in the current user's inventory.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryItem : public LLInventoryObject
{
protected:
	LOG_CLASS(LLInventoryItem);

	~LLInventoryItem() override = default;

public:
	typedef std::vector<LLPointer<LLInventoryItem> > item_array_t;

	enum
	{
		// The shared flags at the top are shared among all inventory types.
		// After that section, all values of flags are type dependent. The
		//shared flags will start at 2^30 and work down while item type
		// specific flags will start at 2^0 and work up.
		II_FLAGS_NONE = 0,

		//
		// Shared flags
		//
		//

		// This value means that the asset has only one reference in the
		// system. If the inventory item is deleted, or the asset Id updated,
		// then we can remove the old reference.
		II_FLAGS_SHARED_SINGLE_REFERENCE = 0x40000000,

		//
		// Landmark flags
		//
		II_FLAGS_LANDMARK_VISITED = 1,

		//
		// Object flags
		//

		// flag to indicate that object permissions should have next owner perm
		// be more restrictive on rez. We bump this into the second byte of the
		// flags since the low byte is used to track attachment points.
		II_FLAGS_OBJECT_SLAM_PERM = 0x100,

		// flag to indicate that the object sale information has been changed.
		II_FLAGS_OBJECT_SLAM_SALE = 0x1000,

		// These flags specify which permissions masks to overwrite upon rez.
		// Normally, if no permissions slam (above) or overwrite flags are set,
		// the asset's permissions are used and the inventory's permissions are
		// ignored. If any of these flags are set, the inventory's permissions
		// take precedence.
		II_FLAGS_OBJECT_PERM_OVERWRITE_BASE			= 0x010000,
		II_FLAGS_OBJECT_PERM_OVERWRITE_OWNER		= 0x020000,
		II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP		= 0x040000,
		II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE		= 0x080000,
		II_FLAGS_OBJECT_PERM_OVERWRITE_NEXT_OWNER	= 0x100000,

 		// flag to indicate whether an object that is returned is composed
		// of multiple items or not.
		II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS			= 0x200000,

		// Some items like Wearables and Settings use the low order byte of
		// flags to store the sub type of the inventory item. EWearableType
		// enumeration found in llappearance/llwearable.h
		II_FLAGS_SUBTYPE_MASK						= 0x0000ff,

		II_FLAGS_PERM_OVERWRITE_MASK = 				(II_FLAGS_OBJECT_SLAM_PERM |
													 II_FLAGS_OBJECT_SLAM_SALE |
													 II_FLAGS_OBJECT_PERM_OVERWRITE_BASE |
													 II_FLAGS_OBJECT_PERM_OVERWRITE_OWNER |
													 II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP |
													 II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE |
													 II_FLAGS_OBJECT_PERM_OVERWRITE_NEXT_OWNER),
			// These bits need to be cleared whenever the asset_id is updated
			// on a pre-existing inventory item (DEV-28098 and DEV-30997)
	};

	LLInventoryItem(const LLUUID& uuid, const LLUUID& parent_uuid,
					const LLPermissions& permissions, const LLUUID& asset_uuid,
					LLAssetType::EType type, LLInventoryType::EType inv_type,
					const std::string& name, const std::string& desc,
					const LLSaleInfo& sale_info, U32 flags, S32 creation_date);
	LLInventoryItem();
	// Create a copy of an inventory item from a pointer to another item
	// Note: because InventoryItems are ref counted, reference copy (a = b) is
	// prohibited
	LLInventoryItem(const LLInventoryItem* otherp);

	LL_INLINE LLInventoryItem* asInventoryItem() override
	{
		return this;
	}

	LL_INLINE const LLInventoryItem* asInventoryItem() const override
	{
		return this;
	}

	// LLRefCount requires custom copy:
	virtual void copyItem(const LLInventoryItem* otherp);

	LL_INLINE void generateUUID()							{ mUUID.generate(); }

	const LLUUID& getLinkedUUID() const override;

	LL_INLINE virtual const LLPermissions& getPermissions() const
	{
		return mPermissions;
	}

	LL_INLINE virtual const LLUUID& getCreatorUUID() const	{ return mPermissions.getCreator(); }
	LL_INLINE virtual const LLUUID& getAssetUUID() const	{ return mAssetUUID; }

	LL_INLINE virtual const std::string& getDescription() const
	{
		return mDescription;
	}

	// Does not follow links
	LL_INLINE virtual const std::string& getActualDescription() const
	{
		return mDescription;
	}

	LL_INLINE virtual const LLSaleInfo& getSaleInfo() const
	{
		return mSaleInfo;
	}

	LL_INLINE virtual LLInventoryType::EType getInventoryType() const
	{
		return mInventoryType;
	}

	LL_INLINE virtual U32 getFlags() const					{ return mFlags; }
	LL_INLINE time_t getCreationDate() const override		{ return mCreationDate; }

	virtual U32 getCRC32() const; // really more of a checksum.

	LL_INLINE void setAssetUUID(const LLUUID& asset_id)		{ mAssetUUID = asset_id; }

	static void correctInventoryDescription(std::string& name);
	void setDescription(const std::string& new_desc);
	LL_INLINE void setSaleInfo(const LLSaleInfo& sale_info)	{ mSaleInfo = sale_info; }
	void setPermissions(const LLPermissions& perm);

	LL_INLINE void setInventoryType(LLInventoryType::EType inv_type)
	{
		mInventoryType = inv_type;
	}

	LL_INLINE void setFlags(U32 flags)						{ mFlags = flags; }

	// Currently only used in the Viewer to handle calling cards where the
	// creator is actually used to store the target.
	LL_INLINE void setCreator(const LLUUID& creator)		{ mPermissions.setCreator(creator); }

	// Checks for changes in permissions masks and sale info and sets the
	// corresponding bits in mFlags.
	void accumulatePermissionSlamBits(const LLInventoryItem& old_item);

	// Puts this inventory item onto the current outgoing mesage.
	// Assumes you have already called nextBlock().
	virtual void packMessage(LLMessageSystem* msg) const;

	// Returns true if the inventory item came through the network correctly.
	// Uses a simple crc check which is defeatable, but we want to detect
	// network mangling somehow.
	virtual bool unpackMessage(LLMessageSystem* msg, const char* block,
							   S32 block_num = 0);

	// File support
	bool importLegacyStream(std::istream& input_stream) override;
	bool exportLegacyStream(std::ostream& output_stream,
							bool include_asset_key = true) const override;

	// Helper methods

	LLSD asLLSD() const;
	void asLLSD(LLSD& sd) const;
	bool fromLLSD(const LLSD& sd, bool is_new = true);

	// Used to compare two inventory items independently of their current UUID,
	// parent folder, and creation date: when their hashContents() is the same,
	// they are just copies. HB
	LLUUID hashContents() const;

protected:
	LLUUID					mAssetUUID;
	std::string				mDescription;
	LLSaleInfo				mSaleInfo;
	LLPermissions			mPermissions;
	U32						mFlags;
	LLInventoryType::EType	mInventoryType;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// A  class for category/folder of inventory items. Users come with a set of
// default categories, and can create new ones as needed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryCategory : public LLInventoryObject
{
protected:
	LOG_CLASS(LLInventoryCategory);

	~LLInventoryCategory() override = default;

public:
	typedef std::vector<LLPointer<LLInventoryCategory> > cat_array_t;

	LLInventoryCategory(const LLUUID& uuid, const LLUUID& parent_uuid,
						LLFolderType::EType preferred_type,
						const std::string& name);
	LLInventoryCategory();
	LLInventoryCategory(const LLInventoryCategory* otherp);

	LL_INLINE LLInventoryCategory* asInventoryCategory() override
	{
		return this;
	}

	LL_INLINE const LLInventoryCategory* asInventoryCategory() const override
	{
		return this;
	}

	// LLRefCount requires custom copy
	void copyCategory(const LLInventoryCategory* otherp);

	LL_INLINE LLFolderType::EType getPreferredType() const	{ return mPreferredType; }
	LL_INLINE void setPreferredType(LLFolderType::EType t)	{ mPreferredType = t; }

	LLSD asLLSD() const;
	LLSD asAISCreateCatLLSD() const;
	bool fromLLSD(const LLSD& sd);

	// Messaging
	virtual void packMessage(LLMessageSystem* msg) const;
	virtual void unpackMessage(LLMessageSystem* msg, const char* block,
							   S32 block_num = 0);

	// File support
	bool importLegacyStream(std::istream& input_stream) override;
	bool exportLegacyStream(std::ostream& output_stream,
							bool include_asset_key = true) const override;

protected:
	// Type that this category was "meant" to hold (although it may hold any
	// type)
	LLFolderType::EType	mPreferredType;
};

#endif // LL_LLINVENTORY_H
