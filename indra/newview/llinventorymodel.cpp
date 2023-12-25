/**
 * @file llinventorymodel.cpp
 * @brief Implementation of the inventory model used to track agent inventory.
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

#include "llviewerprecompiledheaders.h"

#include <sstream>
#include <utility>

#include "llinventorymodel.h"

#include "llcallbacklist.h"
#include "llcorebufferarray.h"
#include "llcorehttputil.h"
#include "hbfastmap.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llstreamtools.h"				// g[un]zip_file()

#include "llagent.h"
#include "llagentwearables.h"
#include "llaisapi.h"
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llfloaterinventory.h"
#include "hbfloatereditenvsettings.h"
#include "llgesturemgr.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llinventorybridge.h"
#include "llinventorymodelfetch.h"
#include "llmarketplacefunctions.h"
#include "llmutelist.h"
#include "llpreview.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerfoldertype.h"
#include "llviewerinventory.h"
#include "llviewermessage.h"			// For gGenericDispatcher
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

// Increment this if the inventory contents change in a non-backwards
// compatible way. For viewers with link items support, former caches are
// incorrect.
constexpr S32 INVENTORY_CACHE_VERSION = 2;
constexpr S32 MAX_INDIVIDUAL_ITEM_REQUESTS = 7;

bool LLInventoryModel::sWearNewClothing = false;
LLUUID LLInventoryModel::sWearNewClothingTransactionID;

//----------------------------------------------------------------------------
// Local function declarations, constants, enums, and typedefs
//----------------------------------------------------------------------------

struct InventoryIDPtrLess
{
	bool operator()(const LLViewerInventoryCategory* i1,
					const LLViewerInventoryCategory* i2) const
	{
		return i1->getUUID() < i2->getUUID();
	}
};

class LLCanCache final : public LLInventoryCollectFunctor
{
public:
	LLCanCache(LLInventoryModel* model)
	:	mModel(model)
	{
	}

	bool operator()(LLInventoryCategory* catp,
					LLInventoryItem* itemp) override;

protected:
	LLInventoryModel* mModel;
	uuid_list_t mCachedCatIDs;
};

bool LLCanCache::operator()(LLInventoryCategory* catp, LLInventoryItem* itemp)
{
	if (itemp)
	{
		return mCachedCatIDs.count(itemp->getParentUUID()) != 0;
	}
	if (catp)
	{
		// *HACK: downcast
		LLViewerInventoryCategory* vcatp = (LLViewerInventoryCategory*)catp;
		if (!vcatp->isVersionUnknown())
		{
			S32 descendents_server = vcatp->getDescendentCount();
			S32 descendents_actual = vcatp->getViewerDescendentCount();
			if (descendents_server == descendents_actual)
			{
				mCachedCatIDs.emplace(vcatp->getUUID());
				return true;
			}
		}
	}
	return false;
}

struct InventoryCallbackInfo
{
	InventoryCallbackInfo(U32 callback, const LLUUID& inv_id)
	:	mCallback(callback),
		mInvID(inv_id)
	{
	}

	LLUUID	mInvID;
	U32		mCallback;
};

class LLDispatchBulkUpdateInventory final : public LLDispatchHandler
{
protected:
	LOG_CLASS(LLDispatchBulkUpdateInventory);

public:
	bool operator()(const LLDispatcher*, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override;
};

static LLDispatchBulkUpdateInventory sBulkUpdateInventory;

bool LLDispatchBulkUpdateInventory::operator()(const LLDispatcher*,
											   const std::string& key,
											   const LLUUID& invoice,
											   const sparam_t& strings)
{
	LLSD message;

	// Expect single string parameter in the form of a notation serialized LLSD.
	sparam_t::const_iterator it = strings.begin();
	if (it != strings.end())
	{
		const std::string& llsd_raw = *it++;
		std::istringstream llsd_data(llsd_raw);
		if (!LLSDSerialize::deserialize(message, llsd_data, llsd_raw.length()))
		{
			llwarns << "Attempted to read parameter data into LLSD but failed: "
					<< llsd_raw << llendl;
		}
	}

//MK
	bool check_rlv_share =
		gRLenabled && gRLInterface.getRlvShare() &&
		!gSavedSettings.getBool("RestrainedLoveForbidGiveToRLV");
	std::vector<LLPointer<LLViewerInventoryCategory> > folders_to_move;
//mk

	LLInventoryModel::update_map_t update;
	LLInventoryModel::cat_array_t folders;
	LLInventoryModel::item_array_t items;
	typedef std::list<InventoryCallbackInfo> cblist_t;
	cblist_t callback_list;
	uuid_vec_t wearable_ids;

	const LLSD& item_data = message["item_data"];
	if (item_data.isArray())
	{
		for (LLSD::array_const_iterator it = item_data.beginArray(),
										end = item_data.endArray();
			 it != end; ++it)
		{
			const LLSD& item_llsd = *it;
			// Agent Id probably should be in the root of the message.
			if (item_llsd["agent_id"].asUUID() != gAgentID)
			{
				llwarns << "Got a message for the wrong agent. Ignored."
						<< llendl;
				return false;
			}
			LLPointer<LLViewerInventoryItem> itemp = new LLViewerInventoryItem;
			itemp->unpackMessage(item_llsd);
			const LLUUID& item_id = itemp->getUUID();
			const LLUUID& parent_id = itemp->getParentUUID();
			LL_DEBUGS("Inventory") << "Unpacked item '" << itemp->getName()
								   << "' (" << item_id << ") in folder Id: "
								   << parent_id << LL_ENDL;

			// Note: the callback_id might be no longer supported...
			U32 callback_id = item_llsd["callback_id"].asInteger();
			callback_list.emplace_back(callback_id, item_id);

			if (item_id.isNull())	// This should not happen...
			{
				continue;
			}

			items.push_back(itemp);
			if (itemp->getInventoryType() == LLInventoryType::IT_WEARABLE)
			{
				wearable_ids.push_back(itemp->getUUID());
			}

			// Examine update for changes.
			LLViewerInventoryItem* old_itemp = gInventory.getItem(item_id);
			if (old_itemp)
			{
				const LLUUID& old_parent_id = old_itemp->getParentUUID();
				if (parent_id == old_parent_id)
				{
					update[parent_id];
				}
				else
				{
					++update[parent_id];
					--update[old_parent_id];
				}
			}
			else
			{
				LLViewerInventoryCategory* catp =
					gInventory.getCategory(parent_id);
				if (catp)
				{
					++update[parent_id];
				}
			}
		}
	}

	const LLSD& folder_data = message["folder_data"];
	if (folder_data.isArray())
	{
		for (LLSD::array_const_iterator it = folder_data.beginArray(),
										end = folder_data.endArray();
			 it != end; ++it)
		{
			const LLSD& folder_llsd = *it;
			LLPointer<LLViewerInventoryCategory> catp =
				new LLViewerInventoryCategory(gAgentID);
			catp->unpackMessage(folder_llsd);
			const LLUUID& cat_id = catp->getUUID();
			const LLUUID& parent_id = catp->getParentUUID();
			LL_DEBUGS("Inventory") << "Unpacked folder '" << catp->getName()
								   << "' (" << cat_id << ") in folder Id: "
								   << parent_id << LL_ENDL;

			if (cat_id.isNull())	// This should not happen...
			{
				continue;
			}

			// If the folder is a listing or a version folder, all we need to
			// do is to update the SLM data
			if (LLMarketplace::updateIfListed(cat_id, parent_id))
			{
				// In that case, there is no item to update so no callback, so
				// we skip the rest of the update
				continue;
			}

			folders.push_back(catp);

			LLViewerInventoryCategory* old_catp =
				gInventory.getCategory(cat_id);
			if (old_catp)
			{
				const LLUUID& old_parent_id = old_catp->getParentUUID();
				if (parent_id == old_parent_id)
				{
					if (parent_id.notNull())
					{
						update[parent_id];
					}
					else
					{
						llwarns << "Null parent Id for folder " << cat_id
								<< llendl;
					}
				}
				else
				{
					if (parent_id.notNull())
					{
						++update[parent_id];
					}
					else
					{
						llwarns << "Null new parent Id for folder " << cat_id
								<< llendl;
					}
					if (old_parent_id.notNull())
					{
						--update[old_parent_id];
					}
					else
					{
						llwarns << "Null old parent Id for folder " << cat_id
							    << llendl;
					}
				}
//MK
				if (check_rlv_share &&
					gRLInterface.shouldMoveToSharedSubFolder(catp))
				{
					folders_to_move.emplace_back(catp);
				}
//mk
			}
			else if (parent_id.notNull())
			{
				// We could not find the folder, so it is probably new. We
				// however still want to attempt accounting for any parent.
				LLViewerInventoryCategory* catp =
					gInventory.getCategory(parent_id);
				if (catp)
				{
					++update[parent_id];
				}
			}
			else
			{
				llwarns << "Null new parent Id for non-found folder " << cat_id
						<< llendl;
			}
		}
	}

	gInventory.accountForUpdate(update);
	for (LLInventoryModel::cat_array_t::iterator it = folders.begin(),
												 end = folders.end();
		 it != end; ++it)
	{
		gInventory.updateCategory(*it);
	}
	for (LLInventoryModel::item_array_t::iterator it = items.begin(),
												  end = items.end();
		 it != end; ++it)
	{
		gInventory.updateItem(*it);
	}
	gInventory.notifyObservers();

	// Transaction Id is missing from this message, so just wear what we got in
	// it when wanting to wear incomming wearable items, regardless of that Id.
	// Note: it means that should more wearable items arrive for the same
	// transaction in other message packets, we would fail to wear them... This
	// "feature" is only used by llfloaterbuycontents.cpp: see if we can
	// reimplement it properly, without relying on this hacky and fragile
	// "sWearNewClothing" trick. HB
	if (LLInventoryModel::sWearNewClothing && !wearable_ids.empty())
	{
		LLInventoryModel::sWearNewClothing = false;

		for (S32 i = 0, count = wearable_ids.size(); i < count; ++i)
		{
			LLViewerInventoryItem* wearablep =
				gInventory.getItem(wearable_ids[i]);
			if (wearablep)
			{
				gAppearanceMgr.wearInventoryItemOnAvatar(wearablep, true);
			}
		}
	}

	for (cblist_t::const_iterator it = callback_list.begin(),
								  end = callback_list.end();
		 it != end; ++it)
	{
		const InventoryCallbackInfo& cbinfo = *it;
		gInventoryCallbacks.fire(cbinfo.mCallback, cbinfo.mInvID);
	}

//MK
	for (U32 i = 0, count = folders_to_move.size(); i < count; ++i)
	{
		gRLInterface.moveToSharedSubFolder(folders_to_move[i].get());
	}
//mk

	return true;
}

//----------------------------------------------------------------------------
// Class LLInventoryModel
//----------------------------------------------------------------------------

// Global for the agent inventory.
LLInventoryModel gInventory;

// Default constructor
LLInventoryModel::LLInventoryModel()
:	mModifyMask(LLInventoryObserver::ALL),
	mModifyMaskBacklog(LLInventoryObserver::NONE),
	mLastItem(NULL),
	mIsNotifyObservers(false),
	mIsAgentInvUsable(false),
	mHttpRequestFG(NULL),
	mHttpRequestBG(NULL),
	mHttpPolicyClass(LLCore::HttpRequest::DEFAULT_POLICY_ID)
{
}

// Destroys the object
LLInventoryModel::~LLInventoryModel()
{
	cleanupInventory();
}

void LLInventoryModel::cleanupInventory()
{
	empty();
	// Deleting one observer might erase others from the list, so always pop
	// off the front
	while (!mObservers.empty())
	{
		observer_list_t::iterator iter = mObservers.begin();
		LLInventoryObserver* observer = *iter;
		mObservers.erase(iter);
		delete observer;
	}
	mObservers.clear();

	// Run down HTTP transport
	mHttpHeaders.reset();
	mHttpOptions.reset();

	delete mHttpRequestFG;
	mHttpRequestFG = NULL;
	delete mHttpRequestBG;
	mHttpRequestBG = NULL;
}

// This is a convenience method to check if one object has a parent chain up to
// the category specified by UUID.
bool LLInventoryModel::isObjectDescendentOf(const LLUUID& obj_id,
											const LLUUID& cat_id) const
{
	if (obj_id == cat_id) return true;

	const LLInventoryObject* obj = getObject(obj_id);
	while (obj)
	{
		const LLUUID& parent_id = obj->getParentUUID();
		if (parent_id.isNull())
		{
			return false;
		}
		if (parent_id == cat_id)
		{
			return true;
		}
		// Since we are scanning up the parents, we only need to check in the
		// category list.
		obj = getCategory(parent_id);
	}
	return false;
}

bool LLInventoryModel::isInCOF(const LLUUID& inv_object_id) const
{
	// Note: in OpenSim, we allow to remove the COF, and its UUID may therefore
	// change during a viewer session, so we do not cache this UUID. HB
	const LLUUID cof_id = LLAppearanceMgr::getCOF();
	return cof_id.notNull() && isObjectDescendentOf(inv_object_id, cof_id);
}

bool LLInventoryModel::isInMarketPlace(const LLUUID& inv_object_id) const
{
	const LLUUID& mp_id = LLMarketplace::getMPL();
	return mp_id.notNull() && isObjectDescendentOf(inv_object_id, mp_id);
}

// Searches up the parent chain until we get to the specified parent, then
// returns the first child category under it.
const LLViewerInventoryCategory* LLInventoryModel::getFirstDescendantOf(const LLUUID& master_parent_id,
																		const LLUUID& obj_id) const
{
	if (master_parent_id == obj_id)
	{
		return NULL;
	}

	const LLViewerInventoryCategory* current_cat = getCategory(obj_id);

	if (!current_cat)
	{
		current_cat = getCategory(getObject(obj_id)->getParentUUID());
	}

	while (current_cat)
	{
		const LLUUID& current_parent_id = current_cat->getParentUUID();

		if (current_parent_id == master_parent_id)
		{
			return current_cat;
		}

		current_cat = getCategory(current_parent_id);
	}

	return NULL;
}

// Get the object by id. Returns NULL if not found.
LLInventoryObject* LLInventoryModel::getObject(const LLUUID& id) const
{
	LLViewerInventoryCategory* cat = getCategory(id);
	if (cat)
	{
		return cat;
	}
	LLViewerInventoryItem* item = getItem(id);
	if (item)
	{
		return item;
	}
	return NULL;
}

// Get the item by id. Returns NULL if not found.
LLViewerInventoryItem* LLInventoryModel::getItem(const LLUUID& id) const
{
	LLViewerInventoryItem* item = NULL;
	if (mLastItem.notNull() && mLastItem->getUUID() == id)
	{
		item = mLastItem;
	}
	else
	{
		item_map_t::const_iterator iter = mItemMap.find(id);
		if (iter != mItemMap.end())
		{
			item = iter->second;
			mLastItem = item;
		}
	}
	return item;
}

// Get the category by id. Returns NULL if not found
LLViewerInventoryCategory* LLInventoryModel::getCategory(const LLUUID& id) const
{
	LLViewerInventoryCategory* catp = NULL;
	if (mCategoryMap.size() > 0)
	{
		cat_map_t::const_iterator iter = mCategoryMap.find(id);
		if (iter != mCategoryMap.end())
		{
			catp = iter->second;
		}
	}
	return catp;
}

S32 LLInventoryModel::getItemCount() const
{
	return mItemMap.size();
}

S32 LLInventoryModel::getCategoryCount() const
{
	return mCategoryMap.size();
}

// Return the direct descendents of the id provided. The array provided points
// straight into the guts of this object, and should only be used for read
// operations, since modifications may invalidate the internal state of the
// inventory. Set passed in values to NULL if the call fails.
void LLInventoryModel::getDirectDescendentsOf(const LLUUID& cat_id,
											  cat_array_t*& categories,
											  item_array_t*& items) const
{
	categories = get_ptr_in_map(mParentChildCategoryTree, cat_id);
	items = get_ptr_in_map(mParentChildItemTree, cat_id);
}

#if LL_HAS_ASSERT
// SJB: added version to lock the arrays to catch potential logic bugs
void LLInventoryModel::lockDirectDescendentArrays(const LLUUID& cat_id,
												  cat_array_t*& categories,
												  item_array_t*& items)
{
	getDirectDescendentsOf(cat_id, categories, items);
	if (categories)
	{
		mCategoryLock[cat_id] = true;
	}
	if (items)
	{
		mItemLock[cat_id] = true;
	}
}

void LLInventoryModel::unlockDirectDescendentArrays(const LLUUID& cat_id)
{
	mCategoryLock[cat_id] = mItemLock[cat_id] = false;
}
#endif

void LLInventoryModel::consolidateForType(const LLUUID& main_id,
										  LLFolderType::EType type,
										  bool is_root_cat)
{
	if (main_id.isNull())
	{
		llwarns << "Cannot consolidate for type: "
				<< LLFolderType::lookup(type) << " - Missing system folder."
				<< llendl;
		if (isInventoryUsable())
		{
			llinfos << "Creating missing system folder for type: "
					<< LLFolderType::lookup(type) << llendl;
			createNewCategory(getRootFolderID(), type, LLStringUtil::null,
							  NULL);
		}
		return;
	}

	// Make a list of folders that are not "main_id" and are of "type"
	uuid_vec_t folder_ids;
	for (cat_map_t::iterator cit = mCategoryMap.begin(),
							 end = mCategoryMap.end();
		 cit != end; ++cit)
	{
		LLViewerInventoryCategory* cat = cit->second;
		if (!cat) continue;	// Paranoia

		const LLUUID& cat_id = cat->getUUID();
		if (cat_id.notNull() && cat_id != main_id &&
			cat->getPreferredType() == type)
		{
			folder_ids.emplace_back(cat_id);
		}
	}

	// Iterate through those folders
	for (S32 i = 0, count = folder_ids.size(); i < count; ++i)
	{
		const LLUUID& folder_id = folder_ids[i];
		if (!isObjectDescendentOf(folder_id, gInventory.getRootFolderID()))
		{
			// Do not consolidate folders contained in the library...
			continue;
		}

		// Get the content of this folder
		cat_array_t* cats;
		item_array_t* items;
		getDirectDescendentsOf(folder_id, cats, items);

		// Move all items to the main folder.
		// Note : we get the list of UUIDs and iterate on them instead of
		// iterating directly on item_array_t elements. This is because moving
		// elements modify the maps and, consequently, invalidate iterators on
		// them. This "gather and iterate" method is verbose but resilient.
		uuid_vec_t list_uuids;
		for (item_array_t::const_iterator it = items->begin(),
										  end = items->end();
			 it != end; ++it)
		{
			LLViewerInventoryItem* item = *it;
			if (item)	// Paranoia
			{
				list_uuids.emplace_back(item->getUUID());
			}
		}
		for (S32 j = 0, count2 = list_uuids.size(); j < count2; ++j)
		{
			LLViewerInventoryItem* item = getItem(list_uuids[j]);
			changeItemParent(item, main_id, true);
		}

		// Move all folders to the main folder
		list_uuids.clear();
		for (cat_array_t::const_iterator it = cats->begin(),
										 end = cats->end();
			 it != end; ++it)
		{
			LLViewerInventoryCategory* cat = *it;
			if (cat)	// Paranoia
			{
				list_uuids.emplace_back(cat->getUUID());
			}
		}
		for (S32 j = 0, count2 = list_uuids.size(); j < count2; ++j)
		{
			LLViewerInventoryCategory* cat = getCategory(list_uuids[j]);
			changeCategoryParent(cat, main_id, true);
		}

		// Purge the emptied folder
		removeCategory(folder_id);
		remove_inventory_category(folder_id, NULL, false);
		notifyObservers();
	}

	if (is_root_cat)
	{
		// Make sure this category is parented to the root folder
		const LLUUID& root_id = getRootFolderID();
		LLViewerInventoryCategory* cat = getCategory(main_id);
		if (cat && cat->getParentUUID() != root_id)
		{
			changeCategoryParent(cat, root_id, true);
		}
	}
}

// Returns the UUID of the category that specifies 'type' as what it defaults
// to containing. The category is not necessarily only for that type.
// *NOTE: this will create a new inventory category on the fly if one does not
// exist.
LLUUID LLInventoryModel::findCategoryUUIDForType(LLFolderType::EType t,
												 bool create_folder)
{
	LLUUID cat_id = findCatUUID(t);
	if (cat_id.isNull() && create_folder)
	{
		if (!isInventoryUsable())
		{
			llwarns_once << "Cannot create missing category: "
						 << LLFolderType::lookup(t)
						 << " - Inventory not in usable state." << llendl;
		}
		else
		{
			llinfos << "Using legacy UDP messaging to create missing category: "
					<< LLFolderType::lookup(t) << llendl;
			cat_id = createCategoryUDP(getRootFolderID(), t,
									   LLStringUtil::null);
		}
	}
	return cat_id;
}

LLUUID LLInventoryModel::findChoosenCategoryUUIDForType(LLFolderType::EType t)
{
	static LLCachedControl<std::string> animation_id(gSavedPerAccountSettings,
													 "UploadAnimationFolder");
	static LLCachedControl<std::string> material_id(gSavedPerAccountSettings,
												   "UploadMaterialFolder");
	static LLCachedControl<std::string> model_id(gSavedPerAccountSettings,
												 "UploadModelFolder");
	static LLCachedControl<std::string> outfits_id(gSavedPerAccountSettings,
												   "NewOutfitFolder");
	static LLCachedControl<std::string> sound_id(gSavedPerAccountSettings,
												 "UploadSoundFolder");
	static LLCachedControl<std::string> texture_id(gSavedPerAccountSettings,
												   "UploadTextureFolder");
	std::string id_str;
	switch (t)	// Cases are in LLFolderType::EType order. HB
	{
		case LLFolderType::FT_TEXTURE:
			id_str = texture_id;
			break;

		case LLFolderType::FT_SOUND:
			id_str = sound_id;
			break;

		case LLFolderType::FT_OBJECT:
			id_str = model_id;
			break;

		case LLFolderType::FT_ANIMATION:
			id_str = animation_id;
			break;

		case LLFolderType::FT_MY_OUTFITS:
			// FT_MY_OUTFITS becomes FT_CLOTHING on purpose when no user
			// preferred folder is set, since it is where v1 viewers always
			// create new outfits. HB
			t = LLFolderType::FT_CLOTHING;
			id_str = outfits_id;
			break;

		case LLFolderType::FT_MATERIAL:
			id_str = material_id;
			break;

		default:
			break;
	}

	if (!id_str.empty())
	{
		LLUUID cat_id;
		cat_id.set(id_str, false);
		if (cat_id.notNull() && getCategory(cat_id))
		{
			return cat_id;
		}
	}

	return findCategoryUUIDForType(t, true);
}

// Internal method which looks for a category with the specified preferred
// type. Returns LLUUID::null if not found.
LLUUID LLInventoryModel::findCatUUID(LLFolderType::EType type)
{
	LLUUID root_id = getRootFolderID();
	if (type == LLFolderType::FT_ROOT_INVENTORY)
	{
		return root_id;
	}
	if (type == LLFolderType::FT_ROOT_INVENTORY_OS && !gIsInSecondLife)
	{
		return root_id;
	}
	if (root_id.notNull())
	{
		cat_array_t* cats = get_ptr_in_map(mParentChildCategoryTree, root_id);
		if (cats)
		{
			S32 count = cats->size();
			for (S32 i = 0; i < count; ++i)
			{
				LLInventoryCategory* cat = (*cats)[i];
				if (cat->getPreferredType() == type)
				{
					return cat->getUUID();
				}
			}
		}
	}
	return LLUUID::null;
}

static LLPointer<LLViewerInventoryCategory> create_cat(const LLUUID& cat_id,
													   const LLUUID& parent_id,
													   const LLUUID& thumb_id,
													   LLFolderType::EType type,
													   const std::string& name)
{
	LLPointer<LLViewerInventoryCategory> catp = NULL;

	// For when the AIS callback reports a failure: abort the creation then.
	if (cat_id.isNull())
	{
		return catp;
	}

	// If the category already exists (may be the case with the AIS callback),
	// there is nothing to do.
	catp = gInventory.getCategory(cat_id);
	if (catp.notNull())
	{
		return catp;
	}

	// Add the category to the internal representation
	catp = new LLViewerInventoryCategory(cat_id, parent_id, type, name,
										 gAgentID);
	// Note: VERSION_INITIAL - 1 because accountForUpdate() will increment it
	catp->setVersion(LLViewerInventoryCategory::VERSION_INITIAL - 1);
	catp->setDescendentCount(0);
	if (thumb_id.notNull())
	{
		catp->setThumbnailUUID(thumb_id);
	}
	LLInventoryModel::LLCategoryUpdate update(catp->getParentUUID(), 1);
	gInventory.accountForUpdate(update);
	gInventory.updateCategory(catp);

	return catp;
}

LLUUID LLInventoryModel::createCategoryUDP(const LLUUID& parent_id,
										   LLFolderType::EType preferred_type,
										   const std::string& pname,
										   const LLUUID& thumbnail_id)
{
	if (!isInventoryUsable())
	{
		llwarns << "Inventory is broken." << llendl;
		return LLUUID::null;
	}

	if (LLFolderType::lookup(preferred_type) == LLFolderType::badLookup())
	{
		LL_DEBUGS("Inventory") << "Attempt to create unsupported category type: "
							   << preferred_type << LL_ENDL;
		return LLUUID::null;
	}

	std::string name = pname;
	if (pname.empty())
	{
		name.assign(LLViewerFolderType::lookupNewCategoryName(preferred_type));
	}
	else
	{
		name.assign(pname);
	}

	LLUUID cat_id;
	cat_id.generate();

	LL_DEBUGS("Inventory") << "Using UDP messaging to create category: "
						   << name << " (Id: " << cat_id << ")" << LL_ENDL;

	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return LLUUID::null;		// Paranoia.

	// Add the category to the internal representation
	LLPointer<LLViewerInventoryCategory> catp = create_cat(cat_id,
														   parent_id,
														   thumbnail_id,
														   preferred_type,
														   name);

	// Create the category with our generated UUID on the server.
	msg->newMessage(_PREHASH_CreateInventoryFolder);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_FolderData);
	catp->packMessage(msg);
	gAgent.sendReliableMessage();

	// Return the UUID of the category we just asked the server to create.
	return cat_id;
}

void LLInventoryModel::createNewCategory(const LLUUID& parent_id,
										 LLFolderType::EType preferred_type,
										 const std::string& pname,
										 inventory_func_t callback,
										 const LLUUID& thumbnail_id)
{
	if (!isInventoryUsable())
	{
		llwarns << "Inventory is broken." << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	if (LLFolderType::lookup(preferred_type) == LLFolderType::badLookup())
	{
		LL_DEBUGS("Inventory") << "Attempt to create unsupported category type: "
							   << preferred_type << LL_ENDL;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	std::string name = pname;
	if (pname.empty())
	{
		name.assign(LLViewerFolderType::lookupNewCategoryName(preferred_type));
	}
	else
	{
		name.assign(pname);
	}

	if (AISAPI::isAvailable())
	{
		LL_DEBUGS("Inventory") << "Using AIS to create category: " << name
							   << LL_ENDL;

		LLPointer<LLViewerInventoryCategory> catp =
				new LLViewerInventoryCategory(LLUUID::null, parent_id,
											  preferred_type, name, gAgentID);
		if (thumbnail_id.notNull())
		{
			catp->setThumbnailUUID(thumbnail_id);
		}
		LLSD cat_sd = catp->asAISCreateCatLLSD();
		LLSD new_inventory = LLSD::emptyMap();
		new_inventory["categories"] = LLSD::emptyArray();
		new_inventory["categories"].append(cat_sd);
		AISAPI::createInventory(parent_id, new_inventory,
								[callback, parent_id, thumbnail_id,
								 preferred_type, name](const LLUUID& cat_id)
								{
									create_cat(cat_id, parent_id, thumbnail_id,
											   preferred_type, name);
									if (callback && !callback.empty())
									{
										callback(cat_id);
									}
								});
		return;
	}

	const std::string& url =
		gAgent.getRegionCapability("CreateInventoryCategory");
	if (!url.empty())
	{
		// Use the capability.

		// Note; currently, in SL, this requested id will *not* be honoured. HB
		LLUUID cat_id;
		cat_id.generate();
		LL_DEBUGS("Inventory") << "Using the capability to create category: "
							   << name << " (requested Id: " << cat_id << ")"
							   << LL_ENDL;
		LLSD body;
		body["folder_id"] = cat_id;
		body["parent_id"] = parent_id;
		body["type"] = (LLSD::Integer)preferred_type;
		body["name"] = name;
		gCoros.launch("LLInventoryModel::createNewCategoryCoro",
					  boost::bind(&LLInventoryModel::createNewCategoryCoro,
								  this, url, body, thumbnail_id, callback));
		return;
	}

	// Fall back to the old UDP message.
	LLUUID cat_id = createCategoryUDP(parent_id, preferred_type, pname,
									  thumbnail_id);
	if (callback)
	{
		callback(cat_id);
	}
}

void LLInventoryModel::createNewCategoryCoro(const std::string& url,
											 const LLSD& data, LLUUID thumb_id,
											 inventory_func_t callback)
{
	llinfos << "Generic POST for " << url << llendl;

	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setWantHeaders(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("createNewCategoryCoro",
												 mHttpPolicyClass);
	LLSD result = adapter.postAndSuspend(url, data, options);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "HTTP failure attempting to create category." << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	if (!result.has("folder_id"))
	{
		llwarns << "Malformed response contents:\n"
				<< ll_pretty_print_sd(result) << llendl;
		if (callback)
		{
			callback(LLUUID::null);
		}
		return;
	}

	// Get the new category parameters.
	LLUUID cat_id = result["folder_id"].asUUID();
	LLFolderType::EType type = (LLFolderType::EType)result["type"].asInteger();
	LLUUID parent_id = result["parent_id"].asUUID();
	std::string name = result["name"].asString();
	// Add the category to the internal representation.
	LLPointer<LLViewerInventoryCategory> catp =
		new LLViewerInventoryCategory(cat_id, parent_id, type, name, gAgentID);
	if (thumb_id.notNull())
	{
		catp->setThumbnailUUID(thumb_id);
	}

	bool needs_accounting = true;
	LLViewerInventoryCategory* folderp = gInventory.getCategory(cat_id);
	if (folderp)
	{
		// Bulk processing was faster than the coroutine (coro request ->
		// processBulkUpdateInventory -> coro response): the category already
		// exists, but needs an update.
		S32 version = folderp->getVersion();
		S32 descendents = folderp->getDescendentCount();
		if (version != LLViewerInventoryCategory::VERSION_INITIAL ||
			descendents != LLViewerInventoryCategory::DESCENDENT_COUNT_UNKNOWN)
		{
			LL_DEBUGS("Inventory") << "Inventory desynchronization on folder creation. Folder "
								   << folderp->getName() << "("
								   << folderp->getUUID() << ").";
			if (descendents != LLViewerInventoryCategory::DESCENDENT_COUNT_UNKNOWN)
			{
				LL_CONT << " Already got descendents: " << descendents << ".";
			}
			if (version != LLViewerInventoryCategory::VERSION_INITIAL)
			{
				LL_CONT << " Already got a version: " << version << ".";
			}
			LL_CONT << LL_ENDL;
		}
		if (folderp->getParentUUID() != parent_id)
		{
			llwarns << "Inventory desynchronization on folder creation. Folder "
					<< folderp->getName() << "(" << cat_id
					<< ") has wrong parent (" << folderp->getParentUUID()
					<< "). Expected parent: " << parent_id << llendl;
		}
		else
		{
			// Do not update parent, parent is already aware of the change.
			// See processBulkUpdateInventory().
			needs_accounting = false;
		}
	}
	catp->setDescendentCount(0);
	if (needs_accounting)
	{
		// Note: VERSION_INITIAL-1 because accountForUpdate() will increment it
		catp->setVersion(LLViewerInventoryCategory::VERSION_INITIAL - 1);
		LLCategoryUpdate update(catp->getParentUUID(), 1);
		accountForUpdate(update);
	}
	else
	{
		catp->setVersionInitial();
	}
	
	updateCategory(catp);

	if (callback)
	{
		llinfos << "Calling creation callback for category: " << cat_id
				<< llendl;
		callback(cat_id);
	}
}

// Starting with the object specified, add its descendents to the array
// provided, but do not add the inventory object specified by id. There is no
// guaranteed order. Neither array will be erased before adding objects to it.
// Do not store a copy of the pointers collected - use them, and collect them
// again later if you need to reference the same objects.

class LLAlwaysCollect final : public LLInventoryCollectFunctor
{
public:
	LL_INLINE bool operator()(LLInventoryCategory*, LLInventoryItem*) override
	{
		return true;
	}
};

void LLInventoryModel::collectDescendents(const LLUUID& id,
										  cat_array_t& cats,
										  item_array_t& items,
										  bool include_trash)
{
	LLAlwaysCollect always;
	collectDescendentsIf(id, cats, items, include_trash, always);
}

void LLInventoryModel::collectDescendentsIf(const LLUUID& id,
											cat_array_t& cats,
											item_array_t& items,
											bool include_trash,
											LLInventoryCollectFunctor& add)
{
	// Start with categories
	if (!include_trash)
	{
		const LLUUID& trash_id = getTrashID();
		if (trash_id.notNull() && trash_id == id)
		{
			return;
		}
	}
	cat_array_t* cat_array = get_ptr_in_map(mParentChildCategoryTree, id);
	if (cat_array)
	{
		S32 count = cat_array->size();
		for (S32 i = 0; i < count; ++i)
		{
			LLViewerInventoryCategory* cat = (*cat_array)[i];
			if (add(cat, NULL))
			{
				cats.emplace_back(cat);
			}
			collectDescendentsIf(cat->getUUID(), cats, items, include_trash,
								 add);
		}
	}

	// Move onto items
	item_array_t* item_array = get_ptr_in_map(mParentChildItemTree, id);
	if (item_array)
	{
		S32 count = item_array->size();
		for (S32 i = 0; i < count; ++i)
		{
			LLViewerInventoryItem* item = (*item_array)[i];
			if (add(NULL, item))
			{
				items.emplace_back(item);
			}
		}
	}
}

void LLInventoryModel::addChangedMaskForLinks(const LLUUID& object_id,
											  U32 mask)
{
	const LLInventoryObject* obj = getObject(object_id);
	if (!obj || obj->getIsLinkType())
	{
		return;
	}

	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	LLLinkedItemIDMatches is_linked_item_match(object_id);
	collectDescendentsIf(getRootFolderID(), cat_array, item_array,
						 LLInventoryModel::INCLUDE_TRASH,
						 is_linked_item_match);
	if (cat_array.empty() && item_array.empty())
	{
		return;
	}
	for (LLInventoryModel::cat_array_t::iterator cat_iter = cat_array.begin(),
												 cat_end = cat_array.end();
		 cat_iter != cat_end; ++cat_iter)
	{
		LLViewerInventoryCategory* linked_cat = *cat_iter;
		addChangedMask(mask, linked_cat->getUUID());
	}

	for (LLInventoryModel::item_array_t::iterator iter = item_array.begin(),
												  end = item_array.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryItem* linked_item = *iter;
		addChangedMask(mask, linked_item->getUUID());
	}
}

const LLUUID& LLInventoryModel::getLinkedItemID(const LLUUID& object_id) const
{
	const LLInventoryItem* item = getItem(object_id);
	if (!item)
	{
		return object_id;
	}

	// Find the base item in case this a link (if it is not a link, this will
	// just be inv_item_id)
	return item->getLinkedUUID();
}

// Generates a string containing the path to the item specified by item_id.
void LLInventoryModel::appendPath(const LLUUID& id, std::string& path)
{
	std::string temp;
	LLInventoryObject* obj = getObject(id);
	LLUUID parent_id;
	if (obj) parent_id = obj->getParentUUID();
	std::string forward_slash("/");
	while (obj)
	{
		obj = getCategory(parent_id);
		if (obj)
		{
			temp.assign(forward_slash + obj->getName() + temp);
			parent_id = obj->getParentUUID();
		}
	}
	path.append(temp);
}

LLInventoryModel::item_array_t LLInventoryModel::collectLinkedItems(const LLUUID& id,
																	const LLUUID& start_folder_id)
{
	item_array_t items;
	const LLInventoryObject* obj = getObject(id);
	if (!obj || obj->getIsLinkType())
	{
		return items;
	}

	LLInventoryModel::cat_array_t cat_array;
	LLLinkedItemIDMatches is_linked_item_match(id);
	collectDescendentsIf(start_folder_id.isNull() ? getRootFolderID()
												  : start_folder_id,
						 cat_array, items, LLInventoryModel::INCLUDE_TRASH,
						 is_linked_item_match);
	return items;
}

bool LLInventoryModel::isInventoryUsable() const
{
	return getRootFolderID().notNull() && mIsAgentInvUsable;
}

// Calling this method with an inventory item will either change an existing
// item with a matching item_id, or will add the item to the current inventory.
U32 LLInventoryModel::updateItem(const LLViewerInventoryItem* item, U32 mask)
{
	if (!item || item->getUUID().isNull())
	{
		return mask;
	}

	if (!isInventoryUsable())
	{
		llwarns_sparse << "Inventory is broken." << llendl;
		return mask;
	}
	const LLUUID& laf = getLostAndFoundID();

	LLPointer<LLViewerInventoryItem> old_item = getItem(item->getUUID());
	if (old_item)
	{
		// We already have an old item, modify its values
		LLUUID old_parent_id = old_item->getParentUUID();
		LLUUID new_parent_id = item->getParentUUID();
		if (old_parent_id != new_parent_id)
		{
			bool null_parent_id = new_parent_id.isNull();
			if (null_parent_id)
			{
				llwarns << "Null parent UUID for item " << item->getUUID()
						<< " - " << old_item->getName()
						<< ". Moving item to Lost And Found" << llendl;
				new_parent_id = laf;
			}
			// We need to update the parent-child tree
			item_array_t* item_array = get_ptr_in_map(mParentChildItemTree,
													  old_parent_id);
			if (item_array)
			{
				vector_replace_with_last(*item_array, old_item);
			}
			item_array = get_ptr_in_map(mParentChildItemTree, new_parent_id);
			if (item_array)
			{
				item_array->emplace_back(old_item);
			}
			mask |= LLInventoryObserver::STRUCTURE;
			if (null_parent_id)
			{
				LLCategoryUpdate update(new_parent_id, 1);
				accountForUpdate(update);
				old_item->setParent(new_parent_id);
				old_item->updateParentOnServer(false);
			}
		}
		if (old_item->getName() != item->getName())
		{
			mask |= LLInventoryObserver::LABEL;
		}
		old_item->copyViewerItem(item);
		mask |= LLInventoryObserver::INTERNAL;
	}
	else
	{
		// Simply add this item
		LLPointer<LLViewerInventoryItem> new_item =
			new LLViewerInventoryItem(item);
		addItem(new_item);

		if (item->getParentUUID().isNull())
		{
			LLFolderType::EType cat_type =
				LLFolderType::assetTypeToFolderType(new_item->getType());
			LLUUID category_id = findCategoryUUIDForType(cat_type);
			new_item->setParent(category_id);
			item_array_t* item_array =
				get_ptr_in_map(mParentChildItemTree, category_id);
			if (item_array)
			{
				LLCategoryUpdate update(category_id, 1);
				accountForUpdate(update);
				// *FIX: bit of a hack to call update server from here...
				new_item->updateParentOnServer(false);
				item_array->emplace_back(new_item);
			}
			else
			{
				llwarns << "Could not find parent-child item tree for "
						<< new_item->getName() << llendl;
			}
		}
		else
		{
			// *NOTE: The general scheme is that if every byte of the UUID is
			// null, except for the last one or two, the use the last two bytes
			// of the parent id, and match that up against the type. For now,
			// we are only worried about Lost And Found.
			LLUUID parent_id = item->getParentUUID();
			if (parent_id == CATEGORIZE_LOST_AND_FOUND_ID)
			{
				parent_id = laf;
				new_item->setParent(parent_id);
				update_list_t update;
				update.emplace_back(parent_id, 1);
				accountForUpdate(update);
			}
			item_array_t* item_array = get_ptr_in_map(mParentChildItemTree,
													  parent_id);
			if (item_array)
			{
				item_array->emplace_back(new_item);
			}
			else
			{
				// Whoops ! No such parent, make one.
				llinfos << "Lost item: " << new_item->getUUID() << " - "
						<< new_item->getName() << llendl;
				parent_id = laf;
				new_item->setParent(parent_id);
				item_array = get_ptr_in_map(mParentChildItemTree, parent_id);
				if (item_array)
				{
					LLCategoryUpdate update(parent_id, 1);
					accountForUpdate(update);
					// *FIX: bit of a hack to call update server from here...
					new_item->updateParentOnServer(false);
					item_array->emplace_back(new_item);
				}
				else
				{
					llwarns << "Lost and found not there !" << llendl;
				}
			}
		}
		mask |= LLInventoryObserver::ADD;
	}
	if (item->getType() == LLAssetType::AT_CALLINGCARD)
	{
		mask |= LLInventoryObserver::CALLING_CARD;
	}
	addChangedMask(mask, item->getUUID());
	return mask;
}

LLInventoryModel::cat_array_t* LLInventoryModel::getUnlockedCatArray(const LLUUID& id)
{
	cat_array_t* cat_array = get_ptr_in_map(mParentChildCategoryTree, id);
	llassert(cat_array && !mCategoryLock[id]);
	return cat_array;
}

LLInventoryModel::item_array_t* LLInventoryModel::getUnlockedItemArray(const LLUUID& id)
{
	item_array_t* item_array = get_ptr_in_map(mParentChildItemTree, id);
	llassert(item_array && !mItemLock[id]);
	return item_array;
}

// Calling this method with an inventory category will either change an
// existing item with the matching id, or it will add the category.
void LLInventoryModel::updateCategory(const LLViewerInventoryCategory* cat,
									  U32 mask)
{
	if (!cat)
	{
		return;
	}

	if (!isInventoryUsable())
	{
		llwarns << "Inventory is broken." << llendl;
		return;
	}

	const LLUUID& cat_id = cat->getUUID();
	if (cat_id.isNull())
	{
		return;
	}

	LLPointer<LLViewerInventoryCategory> old_cat = getCategory(cat_id);
	if (old_cat)
	{
		// We already have an old category, modify its values
		LLUUID old_parent_id = old_cat->getParentUUID();
		LLUUID new_parent_id = cat->getParentUUID();
		if (old_parent_id != new_parent_id)
		{
			// Need to update the parent-child tree
			cat_array_t* cat_array = getUnlockedCatArray(old_parent_id);
			if (cat_array)
			{
				vector_replace_with_last(*cat_array, old_cat);
			}
			cat_array = getUnlockedCatArray(new_parent_id);
			if (cat_array)
			{
				cat_array->emplace_back(old_cat);
			}
			mask |= LLInventoryObserver::STRUCTURE;
		}
		if (old_cat->getName() != cat->getName() ||
			// Under marketplace, category labels are quite complex and need
			// an extra update
			LLMarketplace::contains(cat_id))
		{
			mask |= LLInventoryObserver::LABEL;
		}

		old_cat->copyViewerCategory(cat);
		addChangedMask(mask, cat_id);
	}
	else
	{
		// Add this category
		LLPointer<LLViewerInventoryCategory> new_cat =
			new LLViewerInventoryCategory(cat->getOwnerID());
		new_cat->copyViewerCategory(cat);
		addCategory(new_cat);

		// Make sure this category is correctly referenced by its parent.
		cat_array_t* cat_array = getUnlockedCatArray(cat->getParentUUID());
		if (cat_array)
		{
			cat_array->emplace_back(new_cat);
		}

		// Make space in the tree for this category's children.
		llassert(!mCategoryLock[new_cat->getUUID()] &&
				 !mItemLock[new_cat->getUUID()]);
		cat_array_t* catsp = new cat_array_t;
		item_array_t* itemsp = new item_array_t;
		mParentChildCategoryTree[new_cat->getUUID()] = catsp;
		mParentChildItemTree[new_cat->getUUID()] = itemsp;
		mask |= LLInventoryObserver::ADD;
		addChangedMask(mask, cat_id);
	}
}

void LLInventoryModel::moveObject(const LLUUID& object_id,
								  const LLUUID& cat_id)
{
	if (!isInventoryUsable())
	{
		llwarns << "Inventory is broken." << llendl;
		return;
	}

	if (object_id == cat_id || !mCategoryMap.count(cat_id))
	{
		llwarns << "Could not move inventory object " << object_id << " to "
				<< cat_id << llendl;
		return;
	}
	LLPointer<LLViewerInventoryCategory> cat = getCategory(object_id);
	if (cat && cat->getParentUUID() != cat_id)
	{
		cat_array_t* cat_array;
		cat_array = getUnlockedCatArray(cat->getParentUUID());
		if (cat_array)
		{
			vector_replace_with_last(*cat_array, cat);
		}
		cat_array = getUnlockedCatArray(cat_id);
		cat->setParent(cat_id);
		if (cat_array)
		{
			cat_array->emplace_back(cat);
		}
		addChangedMask(LLInventoryObserver::STRUCTURE, object_id);
		return;
	}
	LLPointer<LLViewerInventoryItem> item = getItem(object_id);
	if (item && item->getParentUUID() != cat_id)
	{
		item_array_t* item_array;
		item_array = getUnlockedItemArray(item->getParentUUID());
		if (item_array)
		{
			vector_replace_with_last(*item_array, item);
		}
		item_array = getUnlockedItemArray(cat_id);
		item->setParent(cat_id);
		if (item_array)
		{
			item_array->emplace_back(item);
		}
		addChangedMask(LLInventoryObserver::STRUCTURE, object_id);
	}
}

// Migrated from llinventorybridge.cpp
void LLInventoryModel::changeItemParent(LLViewerInventoryItem* itemp,
										const LLUUID& new_parent_id,
										bool restamp)
{
	if (itemp && itemp->getParentUUID() != new_parent_id)
	{
		const LLUUID& item_id = itemp->getUUID();
		llinfos << "Moving '" << itemp->getName() << "' (" << item_id
				<< ") from category " << itemp->getParentUUID()
				<< " to category " << new_parent_id << llendl;

		if (new_parent_id == getTrashID())
		{
			// Hide any preview
			LLPreview::hide(item_id, true);
			if (itemp->getType() == LLAssetType::AT_GESTURE)
			{
				gGestureManager.deactivateGesture(item_id);
			}
			else if (itemp->getType() == LLAssetType::AT_SETTINGS)
			{
				HBFloaterEditEnvSettings::destroy(item_id);
			}
		}

		LLCategoryUpdate old_folder(itemp->getParentUUID(), -1);
		accountForUpdate(old_folder);
		LLCategoryUpdate new_folder(new_parent_id, 1, false);
		accountForUpdate(new_folder);

		LLPointer<LLViewerInventoryItem> new_itemp =
			new LLViewerInventoryItem(itemp);
		new_itemp->setParent(new_parent_id);
		new_itemp->updateParentOnServer(restamp);
		updateItem(new_itemp);
		notifyObservers();
	}
}

// Migrated from llinventorybridge.cpp
void LLInventoryModel::changeCategoryParent(LLViewerInventoryCategory* catp,
											const LLUUID& new_parent_id,
											bool restamp)
{
	if (catp && !isObjectDescendentOf(new_parent_id, catp->getUUID()))
	{
		const LLUUID& cat_id = catp->getUUID();
		llinfos << "Moving '" << catp->getName() << "' (" << cat_id
				<< ") from category " << catp->getParentUUID()
				<< " to category " << new_parent_id << llendl;

		LLCategoryUpdate old_folder(catp->getParentUUID(), -1);
		accountForUpdate(old_folder);
		LLCategoryUpdate new_folder(new_parent_id, 1, false);
		accountForUpdate(new_folder);

		LLPointer<LLViewerInventoryCategory> new_catp =
			new LLViewerInventoryCategory(catp);
		new_catp->setParent(new_parent_id);
		new_catp->updateParentOnServer(restamp);
		updateCategory(new_catp);
		notifyObservers();
	}
}

#if 0 // Do not appear to be used currently.
void LLInventoryModel::onItemUpdated(const LLUUID& item_id,
									 const LLSD& updates,
									 bool update_parent_version)
{
	U32 mask = LLInventoryObserver::NONE;

	LLPointer<LLViewerInventoryItem> item = gInventory.getItem(item_id);
	LL_DEBUGS("Inventory") << "item_id: " << item_id << " - name: "
						   << (item ? item->getName() : "(NOT FOUND)")
						   << LL_ENDL;
	if (item)
	{
		for (LLSD::map_const_iterator it = updates.beginMap();
			 it != updates.endMap(); ++it)
		{
			if (it->first == "name")
			{
				llinfos << "Updating name from " << item->getName() << " to "
						<< it->second.asString() << llendl;
				item->rename(it->second.asString());
				mask |= LLInventoryObserver::LABEL;
			}
			else if (it->first == "desc")
			{
				llinfos << "Updating description from "
						<< item->getActualDescription()
						<< " to " << it->second.asString() << llendl;
				item->setDescription(it->second.asString());
			}
			else
			{
				llwarns << "Unhandled updates for field: " << it->first
						<< llendl;
				llassert(false);
			}
		}
		mask |= LLInventoryObserver::INTERNAL;
		addChangedMask(mask, item->getUUID());
		if (update_parent_version)
		{
			// Descendent count is unchanged, but folder version incremented.
			LLInventoryModel::LLCategoryUpdate up(item->getParentUUID(), 0);
			accountForUpdate(up);
		}
		// Do we want to be able to make this optional ?
		notifyObservers();
	}
}

void LLInventoryModel::onCategoryUpdated(const LLUUID& cat_id,
										 const LLSD& updates)
{
	U32 mask = LLInventoryObserver::NONE;

	LLPointer<LLViewerInventoryCategory> cat = gInventory.getCategory(cat_id);
	LL_DEBUGS("Inventory") << "cat_id: " << cat_id << " - name: "
						   << (cat ? cat->getName() : "(NOT FOUND)")
						   << LL_ENDL;
	if (cat)
	{
		for (LLSD::map_const_iterator it = updates.beginMap();
			 it != updates.endMap(); ++it)
		{
			if (it->first == "name")
			{
				llinfos << "Updating name from " << cat->getName() << " to "
						<< it->second.asString() << llendl;
				cat->rename(it->second.asString());
				mask |= LLInventoryObserver::LABEL;
			}
			else
			{
				llwarns << "Unhandled updates for field: " << it->first
						<< llendl;
				llassert(false);
			}
		}
		mask |= LLInventoryObserver::INTERNAL;
		addChangedMask(mask, cat->getUUID());
		// Do we want to be able to make this optional ?
		notifyObservers();
	}
}
#endif

// Update model after descendents have been purged.
void LLInventoryModel::onDescendentsPurgedFromServer(const LLUUID& object_id,
													 bool fix_broken_links)
{
	LLPointer<LLViewerInventoryCategory> cat = getCategory(object_id);
	if (cat.notNull())
	{
		// do the cache accounting
		S32 descendents = cat->getDescendentCount();
		if (descendents > 0)
		{
			LLCategoryUpdate up(object_id, -descendents);
			accountForUpdate(up);
		}

		// We know that descendent count is 0, however since the accounting may
		// actually not do an update, we should force it here.
		cat->setDescendentCount(0);

		// Unceremoniously remove anything we have locally stored.
		cat_array_t categories;
		item_array_t items;
		collectDescendents(object_id, categories, items,
						   LLInventoryModel::INCLUDE_TRASH);
		S32 count = items.size();

		LLUUID uu_id;
		for (S32 i = 0; i < count; ++i)
		{
			uu_id = items[i]->getUUID();

			// This check prevents the deletion of a previously deleted item.
			// This is necessary because deletion is not done in a hierarchical
			// order. The current item may have been already deleted as a child
			// of its deleted parent.
			if (getItem(uu_id))
			{
				deleteObject(uu_id, fix_broken_links);
			}
		}

		count = categories.size();
		// Slightly kludgy way to make sure categories are removed only after
		// their child categories have gone away.

		// *FIXME: Would probably make more sense to have this whole
		// descendent-clearing thing be a post-order recursive function to get
		// the leaf-up behavior automatically.
		S32 deleted_count;
		S32 total_deleted_count = 0;
		do
		{
			deleted_count = 0;
			for (S32 i = 0; i < count; ++i)
			{
				uu_id = categories[i]->getUUID();
				if (getCategory(uu_id))
				{
					cat_array_t* cat_list = getUnlockedCatArray(uu_id);
					if (!cat_list || cat_list->size() == 0)
					{
						deleteObject(uu_id, fix_broken_links);
						++deleted_count;
					}
				}
			}
			total_deleted_count += deleted_count;
		}
		while (deleted_count > 0);

		if (total_deleted_count != count)
		{
			llwarns << "Unexpected count of categories deleted, got "
					<< total_deleted_count << " expected " << count << llendl;
		}
	}
}

// Update model after an item is confirmed as removed from server. Works for
// categories or items.
void LLInventoryModel::onObjectDeletedFromServer(const LLUUID& object_id,
												 bool fix_broken_links,
												 bool update_parent_version,
												 bool do_notify_observers)
{
	LLPointer<LLInventoryObject> obj = getObject(object_id);
	if (obj)
	{
		if (getCategory(object_id))
		{
			// For category, need to delete/update all children first.
			onDescendentsPurgedFromServer(object_id, fix_broken_links);
		}

		// From item/cat removeFromServer()
		if (update_parent_version)
		{
			LLCategoryUpdate up(obj->getParentUUID(), -1);
			accountForUpdate(up);
		}

		LLPreview::hide(object_id, true);
		HBFloaterEditEnvSettings::destroy(object_id);

		deleteObject(object_id, fix_broken_links, do_notify_observers);
	}
}

// Delete a particular inventory object by ID.
void LLInventoryModel::deleteObject(const LLUUID& id, bool fix_broken_links,
									bool do_notify_observers)
{
	LLPointer<LLInventoryObject> obj = getObject(id);
	if (!obj)
	{
		llwarns << "Deleting non-existent object (id: " << id << " )"
				<< llendl;
		return;
	}

	LL_DEBUGS("Inventory") << "Deleting inventory object " << id << LL_ENDL;

	// Hide any preview
	LLPreview::hide(id, true);
	HBFloaterEditEnvSettings::destroy(id);

	mLastItem = NULL;
	LLUUID parent_id = obj->getParentUUID();
	mCategoryMap.erase(id);
	mItemMap.erase(id);
#if 0
	mInventory.erase(id);
#endif
	item_array_t* item_list = getUnlockedItemArray(parent_id);
	if (item_list)
	{
		LLPointer<LLViewerInventoryItem> item =
			(LLViewerInventoryItem*)((LLInventoryObject*)obj);
		vector_replace_with_last(*item_list, item);
	}
	cat_array_t* cat_list = getUnlockedCatArray(parent_id);
	if (cat_list)
	{
		LLPointer<LLViewerInventoryCategory> cat =
			(LLViewerInventoryCategory*)((LLInventoryObject*)obj);
		vector_replace_with_last(*cat_list, cat);
	}

	item_list = getUnlockedItemArray(id);
	if (item_list)
	{
		if (item_list->size())
		{
			llwarns << "Deleting cat " << id
					<< " while it still has child items" << llendl;
		}
		delete item_list;
		mParentChildItemTree.erase(id);
	}
	cat_list = getUnlockedCatArray(id);
	if (cat_list)
	{
		if (cat_list->size())
		{
			llwarns << "Deleting cat " << id
					<< " while it still has child cats" << llendl;
		}
		delete cat_list;
		mParentChildCategoryTree.erase(id);
	}
	addChangedMask(LLInventoryObserver::REMOVE, id);

	// Cannot have links to links, so there is no need for this update if the
	// item removed is a link. Can also skip if source of the update is
	// getting broken link info separately.
	bool is_link_type = obj->getIsLinkType();
#if 0	// *TODO ?
	if (is_link_type)
	{
		removeBacklinkInfo(obj->getUUID(), obj->getLinkedUUID());
	}
#endif

	if (fix_broken_links && !is_link_type)
	{
		updateLinkedObjectsFromPurge(id);
	}

	obj = NULL; // Delete obj

	if (do_notify_observers)
	{
		notifyObservers();
	}
}

void LLInventoryModel::updateLinkedObjectsFromPurge(const LLUUID& baseobj_id)
{
	LLInventoryModel::item_array_t item_array = collectLinkedItems(baseobj_id);

	// REBUILD is expensive, so clear the current change list first else
	// everything else on the changelist will also get rebuilt.
	if (item_array.size() > 0)
	{
		notifyObservers();
		// Icon and label may have changed.
		constexpr S32 CHANGED_MASK = LLInventoryObserver::REBUILD |
									 LLInventoryObserver::LABEL;
		for (LLInventoryModel::item_array_t::const_iterator
				iter = item_array.begin(), end = item_array.end();
			iter != end; ++iter)
		{
			const LLViewerInventoryItem* linked_item = *iter;
			const LLUUID& item_id = linked_item->getUUID();
			if (item_id != baseobj_id)
			{
				addChangedMask(CHANGED_MASK, item_id);
			}
		}
		notifyObservers();
	}
}

void LLInventoryModel::addObserver(LLInventoryObserver* observer)
{
	mObservers.insert(observer);
}

void LLInventoryModel::removeObserver(LLInventoryObserver* observer)
{
	mObservers.erase(observer);
}

bool LLInventoryModel::containsObserver(LLInventoryObserver* observer)
{
	return mObservers.find(observer) != mObservers.end();
}

void LLInventoryModel::idleNotifyObservers()
{
	// *FIX: make this conditional or moved elsewhere...
	handleResponses(true);

	if (!mLinksRebuildList.empty())
	{
		// Mark "broken" links corresponding to recently restored valid items
		// for repair (rebuild).
		for (U32 i = 0, count = mLinksRebuildList.size(); i < count; ++i)
		{
			const LLUUID& link_id = mLinksRebuildList[i];
			if (mItemMap.count(link_id))	// Still there ?
			{
				addChangedMask(LLInventoryObserver::REBUILD, link_id);
				LL_DEBUGS("Inventory") << "Repaired broken link: " << link_id
									   << LL_ENDL;
			}
		}
		mLinksRebuildList.clear();
	}

	if (mModifyMask != LLInventoryObserver::NONE || mChangedItemIDs.size())
	{
		notifyObservers();
	}
}

// Call this method when it is time to update everyone on a new state.
void LLInventoryModel::notifyObservers()
{
	if (mIsNotifyObservers)
	{
		// Within notifyObservers, something called notifyObservers again.
		// This type of recursion is unsafe because it causes items to be
		// processed twice, and this can easily lead to infinite loops.
		// Note: the Cool VL Viewer code should have been purged from any such
		// bugs, but letting this in place, just in case... HB
		llwarns << "Recursive call detected; aborting and delaying to next frame."
				<< llendl;
		return;
	}

	mIsNotifyObservers = true;

	for (observer_list_t::iterator iter = mObservers.begin();
		 iter != mObservers.end(); )
	{
		LLInventoryObserver* observer = *iter;
		observer->changed(mModifyMask);
		// Safe way to increment since changed() may delete a few entries.
		iter = mObservers.upper_bound(observer);
	}

	// If any changes arrived during the calls to the observers, schedule them
	// for the next notifyObservers() call.
	mModifyMask = mModifyMaskBacklog;
	mModifyMaskBacklog = LLInventoryObserver::NONE;
	mChangedItemIDs.clear();
	if (!mChangedItemIDsBacklog.empty())
	{
		// This the fastest method and also clears mChangedItemIDsBacklog. HB
		mChangedItemIDs.swap(mChangedItemIDsBacklog);
	}
	mAddedItemIDs.clear();
	if (!mAddedItemIDsBacklog.empty())
	{
		// This the fastest method and also clears mAddedItemIDsBacklog. HB
		mAddedItemIDs.swap(mAddedItemIDsBacklog);
	}

	mIsNotifyObservers = false;
}

// Stores flags for change and Id of object that change applies to
void LLInventoryModel::addChangedMask(U32 mask, const LLUUID& referent)
{
	uuid_list_t* changed_items;
	uuid_list_t* added_items;
	if (mIsNotifyObservers)
	{
		// This condition sometimes happen when we update our agent's outfit
		// and receive the wearable data during an idleNotifyObservers() call.
		// There is no way to avoid it, so we must instead track the updated
		// items and the type of updates, so to reinsert them at the end of
		// the notifyObservers() call and get those changes accounted for on
		// next call. HB
		LL_DEBUGS("Inventory") << "Modify mask changed within notify observer for ";
		std::string name = "<unknown>";
		LLViewerInventoryCategory* cat = getCategory(referent);
		if (cat)
		{
			name = "category: " + cat->getName();
		}
		else
		{
			LLViewerInventoryItem* item = getItem(referent);
			if (item)
			{
				name = "item: " + item->getName();
			}
		}
		LL_CONT << name <<  LL_ENDL;
		mModifyMaskBacklog |= mask;
		changed_items = &mChangedItemIDsBacklog;
		added_items = &mAddedItemIDsBacklog;
	}
	else
	{
		mModifyMask |= mask;
		changed_items = &mChangedItemIDs;
		added_items = &mAddedItemIDs;
	}

	if (referent.notNull() && !changed_items->count(referent))
	{
		changed_items->emplace(referent);
#if 0	// This is a bogus thing to do here (because updateCategory() would
		// change the modify masks and that change would have all risks to be
		// ignored, simply triggering the warning above), and should not be
		// needed any more now that I fixed the observer code for the
		// marketplace (by moving changes to the inventory structure out of
		// the observer event code and into an idle callback). HB
		if (LLMarketplace::contains(referent))
		{
			LLMarketplace::updateCategory(referent, false);
		}
#endif
		if (mask & LLInventoryObserver::ADD)
		{
			added_items->emplace(referent);
		}

		// Update all linked items. Starting with just LABEL because I am
		// not sure what else might need to be accounted for this.
		if (mask & LLInventoryObserver::LABEL)
		{
			addChangedMaskForLinks(referent, LLInventoryObserver::LABEL);
		}
	}
}

void LLInventoryModel::fetchDescendentsOf(const LLUUID& folder_id)
{
	if (folder_id.isNull())
	{
		llwarns << "Calling fetch descendents on NULL folder id !" << llendl;
		return;
	}
	LLViewerInventoryCategory* cat = getCategory(folder_id);
	if (!cat)
	{
		llwarns_once << "Asked to fetch descendents of non-existent folder: "
					 << folder_id << llendl;
		return;
	}
#if 0
	S32 known_descendents = 0;
	cat_array_t* categories = get_ptr_in_map(mParentChildCategoryTree,
											 folder_id);
	item_array_t* items = get_ptr_in_map(mParentChildItemTree, folder_id);
	if (categories)
	{
		known_descendents += categories->size();
	}
	if (items)
	{
		known_descendents += items->size();
	}
	llinfos << "Known descendents for " << folder_id << ": "
			<< known_descendents << llendl;
#endif
	if (!cat->fetch())
	{
		LL_DEBUGS("Inventory") << "Not fetching descendents" <<  LL_ENDL;
	}
}

std::string LLInventoryModel::getCacheFileName(const LLUUID& agent_id)
{
	std::string agent_id_str;
	agent_id.toString(agent_id_str);
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														  agent_id_str);
	if (!gIsInSecondLife)
	{
		static std::string grid_label =
			LLDir::getScrubbedFileName(LLGridManager::getInstance()->getGridLabel());
		filename += "_" + grid_label;
	}
	else if (!gIsInSecondLifeProductionGrid)
	{
		filename += "_beta";
	}

	filename += "_inv.llsd";

	return filename;
}

void LLInventoryModel::cache(const LLUUID& parent_folder_id,
							 const LLUUID& agent_id)
{
	if (agent_id.isNull())
	{
		LL_DEBUGS("Inventory") << "Null UUID passed as agent Id. Aborting."
							   << LL_ENDL;
		return;
	}
	if (parent_folder_id.isNull())
	{
		LL_DEBUGS("Inventory") << "Null UUID passed as folder Id. Aborting."
							   << LL_ENDL;
		return;
	}

	LL_DEBUGS("Inventory") << "Caching " << parent_folder_id << " for "
						   << agent_id << LL_ENDL;
	LLViewerInventoryCategory* root_cat = getCategory(parent_folder_id);
	if (!root_cat) return;

	cat_array_t categories;
	categories.push_back(root_cat);
	item_array_t items;
	LLCanCache can_cache(this);
	can_cache(root_cat, NULL);
	collectDescendentsIf(parent_folder_id, categories, items, INCLUDE_TRASH,
						 can_cache);
	std::string inventory_filename = getCacheFileName(agent_id);
	saveToFile(inventory_filename, categories, items);
	std::string gzip_filename = inventory_filename + ".gz";
	if (!LLFile::gzip(inventory_filename, gzip_filename))
	{
		llwarns << "Unable to compress " << inventory_filename << llendl;
		return;
	}
	LL_DEBUGS("Inventory") << "Successfully compressed "
						   << inventory_filename << LL_ENDL;
	LLFile::remove(inventory_filename);
}

void LLInventoryModel::addCategory(LLViewerInventoryCategory* category)
{
	if (category)
	{
		// Insert category uniquely into the map
		// LLPointer will deref and delete the old one
		mCategoryMap[category->getUUID()] = category;
		//mInventory[category->getUUID()] = category;
	}
}

void LLInventoryModel::addItem(LLViewerInventoryItem* itemp)
{
	if (!itemp)
	{
		return;
	}

	if (itemp->getType() == LLAssetType::AT_NONE)
	{
		llwarns << "Got bad asset type for item. Name: " << itemp->getName()
				<< " - type: " << itemp->getType() << " inv-type: "
				<< itemp->getInventoryType() << ". Ignoring." << llendl;
		return;
	}

	// This can happen if assettype enums from llassettype.h ever change. For
	// example, there is a known backwards compatibility issue in some viewer
	// prototypes prior to when the AT_LINK enum changed from 23 to 24.
	if (LLAssetType::lookup(itemp->getType()) == LLAssetType::badLookup())
	{
		llwarns << "Got unsupported asset type for item. Name: "
				<< itemp->getName() << " - Type: " << itemp->getType()
				<< " Inventory type: " << itemp->getInventoryType()
				<< llendl;
	}

	const LLUUID& item_id = itemp->getUUID();

	// This condition means that we tried to add a link without the baseobj
	// being in memory. The item will show up as a broken link.
	if (itemp->getIsBrokenLink())
	{
		// The UUID of the linked item is stored as the link item's asset UUID.
		const LLUUID& linked_id = itemp->getAssetUUID();
		if (linked_id.notNull())
		{
			// Schedule  this link for a recheck as inventory gets loaded.
			mBrokenLinks[linked_id].emplace_back(item_id);
			LL_DEBUGS("Inventory") << "Registering apparently broken link '"
								   << itemp->getName()
								   << "' with linked item Id: " << linked_id
								   << LL_ENDL;
		}
		else
		{
			llinfos << "Adding broken link. Name: " << itemp->getName()
					<< " - Item Id: " << itemp->getUUID() << " - Asset Id: "
					<< itemp->getAssetUUID() << " - Parent Id: "
					<< itemp->getParentUUID() << llendl;
		}
	}
	else if (!mBrokenLinks.empty())
	{
		// Check to see if this item corresponds to broken links.
		broken_links_map_t::iterator it = mBrokenLinks.find(item_id);
		if (it != mBrokenLinks.end())
		{
			const uuid_vec_t& links = it->second;
			for (U32 i = 0, count = links.size(); i < count; ++i)
			{
				mLinksRebuildList.emplace_back(links[i]);
			}
			mBrokenLinks.erase(it);
		}
	}

	mItemMap.emplace(item_id, itemp);
}

void LLInventoryModel::rebuildBrokenLinks()
{
	// Make sure we are not adding a potentially expensive rebuild to anything
	// else.
	notifyObservers();

	bool has_rebuilds = false;
	if (!mBrokenLinks.empty())
	{
		for (broken_links_map_t::iterator it = mBrokenLinks.begin(),
										  end = mBrokenLinks.end();
			 it != end; ++it)
		{
			if (!mItemMap.count(it->first))
			{
				continue;	// Still not in our inventory representation.
			}
			const uuid_vec_t& links = it->second;
			for (U32 i = 0, count = links.size(); i < count; ++i)
			{
				const LLUUID& link_id = links[i];
				if (mItemMap.count(link_id))	// Still there ?
				{
					addChangedMask(LLInventoryObserver::REBUILD, link_id);
					LL_DEBUGS("Inventory") << "Repaired broken link: "
										   << link_id << LL_ENDL;
					has_rebuilds = true;
				}
			}
			// We can get away with this one...
			it = mBrokenLinks.erase(it);
		}
	}
#if 0	// This is done in idleNotifyObservers(). HB
	if (!mLinksRebuildList.empty())
	{
		for (U32 i = 0, count = mLinksRebuildList.size(); i < count; ++i)
		{
			const LLUUID& link_id = mLinksRebuildList[i];
			if (mItemMap.find(link_id))	// Still there ?
			{
				addChangedMask(LLInventoryObserver::REBUILD, link_id);
				LL_DEBUGS("Inventory") << "Repaired broken link: " << link_id
									   << LL_ENDL;
				has_rebuilds = true;
			}
		}
		mLinksRebuildList.clear();
	}
#endif
	if (has_rebuilds)
	{
		notifyObservers();
	}
}

// Empty the entire contents
void LLInventoryModel::empty()
{
	for (auto it = mParentChildCategoryTree.begin(),
			  end = mParentChildCategoryTree.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mParentChildCategoryTree.clear();

	for (auto it = mParentChildItemTree.begin(),
			  end = mParentChildItemTree.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mParentChildItemTree.clear();

	mCategoryMap.clear();	// Remove all references (should delete entries)
	mItemMap.clear();		// Remove all references (should delete entries)
	mLastItem = NULL;
#if 0
	mInventory.clear();
#endif
}

void LLInventoryModel::accountForUpdate(const LLCategoryUpdate& update) const
{
	if (update.mCategoryID.isNull())
	{
		llwarns << "Got a null category UUID. Ignoring." << llendl;
		return;
	}
	LLViewerInventoryCategory* catp = getCategory(update.mCategoryID);
	if (!catp)
	{
		llwarns << "No category found for update " << update.mCategoryID
				<< llendl;
		return;
	}

	S32 version = catp->getVersion();
	if (version == LLViewerInventoryCategory::VERSION_UNKNOWN)
	{
		llwarns << "Accounting failed for '" << catp->getName()
				<< "' version: unknown (" << version << ")" << llendl;
		return;
	}

	S32 descendents_server = catp->getDescendentCount();
	S32 descendents_actual = catp->getViewerDescendentCount();
	if (descendents_server != descendents_actual)
	{
		// Error condition, this means that the category did not register that
		// it got new descendents (perhaps because it is still being loaded)
		// which means its descendent count will be wrong.
		llwarns << "No accounting for: '" << catp->getName() << "' version "
				<< version
				<< " due to mismatched descendents count: server count = "
				<< descendents_server << " - viewer count = "
				<< descendents_actual << llendl;
		return;
	}

	descendents_actual += update.mDescendentDelta;
	catp->setDescendentCount(descendents_actual);
	if (update.mChangeVersion)
	{
		catp->setVersion(++version);
	}
	LL_DEBUGS("Inventory")	<< "Accounted: '" << catp->getName() << "' "
							<< version << " with " << descendents_actual
							<< " descendents." << LL_ENDL;
}

void LLInventoryModel::accountForUpdate(const LLInventoryModel::update_list_t& update) const
{
	for (update_list_t::const_iterator it = update.begin(), end = update.end();
		 it != end; ++it)
	{
		accountForUpdate(*it);
	}
}

void LLInventoryModel::accountForUpdate(const LLInventoryModel::update_map_t& update) const
{
	LLCategoryUpdate up;
	for (update_map_t::const_iterator it = update.begin(), end = update.end();
		 it != end; ++it)
	{
		up.mCategoryID = it->first;
		up.mDescendentDelta = it->second.mValue;
		accountForUpdate(up);
	}
}

LLInventoryModel::EHasChildren LLInventoryModel::categoryHasChildren(const LLUUID& cat_id) const
{
	LLViewerInventoryCategory* catp = getCategory(cat_id);
	if (!catp)
	{
		return CHILDREN_NO;
	}
	if (catp->getDescendentCount() > 0)
	{
		return CHILDREN_YES;
	}
	if (catp->getDescendentCount() == 0)
	{
		return CHILDREN_NO;
	}
	if (catp->isVersionUnknown() || catp->isDescendentCountUnknown())
	{
		return CHILDREN_MAYBE;
	}

	// Should not have to run this, but who knows ?...
	parent_cat_map_t::const_iterator cat_it =
		mParentChildCategoryTree.find(catp->getUUID());
	if (cat_it != mParentChildCategoryTree.end() &&
		cat_it->second->size() > 0)
	{
		return CHILDREN_YES;
	}
	parent_item_map_t::const_iterator item_it =
		mParentChildItemTree.find(catp->getUUID());
	if (item_it != mParentChildItemTree.end() && item_it->second->size() > 0)
	{
		return CHILDREN_YES;
	}

	return CHILDREN_NO;
}

bool LLInventoryModel::isCategoryComplete(const LLUUID& cat_id) const
{
	LLViewerInventoryCategory* catp = getCategory(cat_id);
	return catp && !catp->isVersionUnknown() &&
		   catp->getDescendentCount() == catp->getViewerDescendentCount();
}

//static
void LLInventoryModel::checkSystemFolders(void*)
{
	llinfos << "Checking system folders..." << llendl;

	llinfos << "Consolidating the Trash..." << llendl;
	gInventory.consolidateForType(gInventory.getTrashID(),
								  LLFolderType::FT_TRASH);

	llinfos << "Consolidating Lost And Found..." << llendl;
	gInventory.consolidateForType(gInventory.getLostAndFoundID(),
								  LLFolderType::FT_LOST_AND_FOUND);

	LLUUID id = gInventory.findCategoryUUIDForType(LLFolderType::FT_ANIMATION);
	llinfos << "Consolidating Animations..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_ANIMATION);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
	llinfos << "Consolidating Body Parts..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_BODYPART);

	// Note: we do not consolidate calling cards, because the root Calling
	// Cards folder may contain Calling Card sub-folders (this is an exception,
	// stupidely introduced by v2 viewers, to the rule that a special folder
	// should be parented directly to the root of the inventory...
	llinfos << "Ensuring Calling Cards existence..." << llendl;
	gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
	llinfos << "Consolidating Clothing..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_CLOTHING);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);
	llinfos << "Consolidating Landmarks..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_LANDMARK);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_NOTECARD);
	llinfos << "Consolidating Notecards..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_NOTECARD);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_SNAPSHOT_CATEGORY);
	llinfos << "Consolidating Photo Album..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_SNAPSHOT_CATEGORY);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_OBJECT);
	llinfos << "Consolidating Objects..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_OBJECT);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LSL_TEXT);
	llinfos << "Consolidating Scripts..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_LSL_TEXT);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_SOUND);
	llinfos << "Consolidating Sounds..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_SOUND);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TEXTURE);
	llinfos << "Consolidating Textures..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_TEXTURE);

	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_GESTURE);
	llinfos << "Consolidating Gestures..." << llendl;
	gInventory.consolidateForType(id, LLFolderType::FT_GESTURE);

	// Do not impose an extended environment Settings folder: let the user
	// choose...
	bool create = gSavedSettings.getBool("CreateSettingsFolder") &&
				  gAgent.hasInventorySettings();
	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS, create);
	if (id.notNull())
	{
		llinfos << "Consolidating Settings..." << llendl;
		gInventory.consolidateForType(id, LLFolderType::FT_SETTINGS);
	}

	// Do not impose a Materials folder: let the user choose...
	create = gSavedSettings.getBool("CreateMaterialsFolder") &&
			 gAgent.hasInventoryMaterial();
	id = gInventory.findCategoryUUIDForType(LLFolderType::FT_MATERIAL, create);
	if (id.notNull())
	{
		llinfos << "Consolidating Materials..." << llendl;
		gInventory.consolidateForType(id, LLFolderType::FT_MATERIAL);
	}

	id = LLMarketplace::getMPL();
	if (id.notNull())
	{
		llinfos << "Consolidating the Marketplace Listings..." << llendl;
		gInventory.consolidateForType(id,
									  LLFolderType::FT_MARKETPLACE_LISTINGS);
	}

	if (gIsInSecondLife || gSavedSettings.getBool("OSUseCOF"))
	{
		id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT);
		llinfos << "Consolidating the Current Outfit folder..." << llendl;
		gInventory.consolidateForType(id, LLFolderType::FT_CURRENT_OUTFIT);
	}

	if (gIsInSecondLife)
	{
		id = gInventory.findCategoryUUIDForType(LLFolderType::FT_INBOX);
		llinfos << "Consolidating the Received Items folder..." << llendl;
		gInventory.consolidateForType(id, LLFolderType::FT_INBOX);
	}

	if (gRLenabled && !gRLInterface.getRlvShare())
	{
		llinfos << "Creating the missing #RLV folder..." << llendl;
		gInventory.createNewCategory(gInventory.getRootFolderID(),
									 LLFolderType::FT_NONE, RL_SHARED_FOLDER,
									 NULL);
	}

	gInventory.notifyObservers();
}

bool LLInventoryModel::loadSkeleton(const LLSD& options,
									const LLUUID& owner_id)
{
	LL_DEBUGS("LoadInventory") << "Importing inventory skeleton for "
							   << owner_id << LL_ENDL;

	typedef std::set<LLPointer<LLViewerInventoryCategory>,
							   InventoryIDPtrLess> cat_set_t;
	cat_set_t temp_cats;
	bool rv = true;

	for (LLSD::array_const_iterator it = options.beginArray(),
									end = options.endArray();
		 it != end; ++it)
	{
		LLSD name = (*it)["name"];
		LLSD folder_id = (*it)["folder_id"];
		LLSD parent_id = (*it)["parent_id"];
		LLSD version = (*it)["version"];
		if (name.isDefined() && folder_id.isDefined() &&
			parent_id.isDefined() && version.isDefined() &&
			// If an Id is null, it locks the viewer.
			folder_id.asUUID().notNull())
		{
			LLPointer<LLViewerInventoryCategory> cat =
				new LLViewerInventoryCategory(owner_id);
			cat->rename(name.asString());
			cat->setUUID(folder_id.asUUID());
			cat->setParent(parent_id.asUUID());

			LLFolderType::EType preferred_type = LLFolderType::FT_NONE;
			LLSD type_default = (*it)["type_default"];
			if (type_default.isDefined())
			{
				preferred_type = (LLFolderType::EType)type_default.asInteger();
			}
			cat->setPreferredType(preferred_type);
			cat->setVersion(version.asInteger());
			temp_cats.emplace(std::move(cat));
		}
		else
		{
			llwarns << "Unable to import near " << name.asString() << llendl;
			rv = false;
		}
	}

	S32 cached_category_count = 0;
	S32 cached_item_count = 0;
	if (!temp_cats.empty())
	{
		update_map_t child_counts;
		item_array_t items;
		cat_array_t categories;
		cat_set_t invalid_categories;
		uuid_list_t cats_to_update;

		std::string inventory_filename = getCacheFileName(owner_id);
		std::string gzip_filename = inventory_filename + ".gz";

		bool remove_inventory_file = false;
		if (LLFile::exists(gzip_filename))
		{
			if (LLFile::gunzip(gzip_filename, inventory_filename))
			{
				// We only want to remove the inventory file if it was
				// gzipped before we loaded, and we successfully gunziped it.
				remove_inventory_file = true;
			}
			else
			{
				llinfos << "Unable to gunzip " << gzip_filename << llendl;
			}
		}

		bool is_cache_obsolete = false;
		if (loadFromFile(inventory_filename, categories, items, cats_to_update,
						 is_cache_obsolete))
		{
			// We were able to find a cache of files. So, use what we found to
			// generate a set of categories we should add. We will go through
			// each category loaded and if the version does not match,
			// invalidate the version.
			S32 count = categories.size();
			cat_set_t::iterator not_cached = temp_cats.end();
			std::set<LLUUID> cached_ids;
			for (S32 i = 0; i < count; ++i)
			{
				LLViewerInventoryCategory* cat = categories[i];
				cat_set_t::iterator cit = temp_cats.find(cat);
				if (cit == temp_cats.end())
				{
					// Cache corruption ?  Not sure why this happens - SJB
					continue;
				}
				LLViewerInventoryCategory* tcat = *cit;

				// We can safely ignore anything loaded from file, but not sent
				// down in the skeleton.
				if (cit == not_cached)
				{
					if (cats_to_update.count(tcat->getUUID()))
					{
						tcat->setVersionUnknown();
					}
					continue;
				}
				if (cat->getVersion() != tcat->getVersion() ||
					cats_to_update.count(tcat->getUUID()))
				{
					// If the cached version does not match the server version,
					// throw away the version we have so we can fetch the
					// correct contents the next time the viewer opens the
					// folder.
					tcat->setVersionUnknown();
				}
				else if (tcat->getPreferredType() ==
							LLFolderType::FT_MARKETPLACE_STOCK)
				{
					// Do not trust stock folders being updated
					tcat->setVersionUnknown();
				}
				else
				{
					cached_ids.emplace(tcat->getUUID());
				}
				if (cat->getThumbnailUUID().notNull() &&
					tcat->getThumbnailUUID().isNull())
				{
					tcat->setThumbnailUUID(cat->getThumbnailUUID());
				}
			}

			// Go ahead and add the cats returned during the download
			std::set<LLUUID>::const_iterator not_cached_id = cached_ids.end();
			cached_category_count = cached_ids.size();
			for (cat_set_t::iterator it = temp_cats.begin();
				 it != temp_cats.end(); ++it)
			{
				LLViewerInventoryCategory* llvic = *it;
				if (cached_ids.find(llvic->getUUID()) == not_cached_id)
				{
					// This check is performed so that we do not mark new
					// folders in the skeleton (and not in cache) as being
					// cached.
					llvic->setVersionUnknown();
				}
				addCategory(llvic);
				++child_counts[llvic->getParentUUID()];
			}

			// Add all the items loaded which are parented to a category with a
			// correctly cached parent
			cat_map_t::iterator unparented = mCategoryMap.end();
			// First, we add non-link items and links which base objects have
			// been loaded
			for (item_array_t::const_iterator item_iter = items.begin();
				 item_iter != items.end();
				 ++item_iter)
			{
				LLViewerInventoryItem* item = item_iter->get();
				const cat_map_t::iterator cit =
					mCategoryMap.find(item->getParentUUID());
				if (cit != unparented)
				{
					const LLViewerInventoryCategory* cat = cit->second.get();
					if (!cat->isVersionUnknown() && !item->getIsBrokenLink())
					{
						addItem(item);
						++cached_item_count;
						++child_counts[cat->getUUID()];
					}
				}
			}
			// Then we can add the remaining links since their base objects have
			// now all been loaded...
			S32 bad_link_count = 0;
			item_map_t::const_iterator iit;
			for (item_array_t::const_iterator item_iter = items.begin();
				 item_iter != items.end();
				 ++item_iter)
			{
				LLViewerInventoryItem* item = item_iter->get();
				const cat_map_t::iterator cit =
					mCategoryMap.find(item->getParentUUID());
				if (cit != unparented)
				{
					const LLViewerInventoryCategory* cat = cit->second.get();
					if (!cat->isVersionUnknown())
					{
						iit = mItemMap.find(item->getUUID());
						if (iit == mItemMap.end())	// Not yet added
						{
							// This can happen if the linked object's baseobj is
							// removed from the cache but the linked object is
							// still in the cache.
							if (item->getIsBrokenLink())
							{
								++bad_link_count;
								LL_DEBUGS("LoadInventory") << "Attempted to add cached link item without baseobj present (name: "
														   << item->getName()
														   << " - itemID: "
														   << item->getUUID()
														   << " - assetID: "
														   << item->getAssetUUID()
														   << "). Ignoring and invalidating: "
														   << cat->getName()
														   << LL_ENDL;
								invalid_categories.emplace(cit->second);
								continue;
							}
							addItem(item);
							++cached_item_count;
							++child_counts[cat->getUUID()];
						}
					}
				}
			}
			if (bad_link_count > 0)
			{
				llinfos << "Attempted to add " << bad_link_count
						<< " cached link items without baseobj present. "
						<< "The corresponding categories were invalidated."
						<< llendl;
			}
		}
		else
		{
			// Go ahead and add everything after stripping the version
			// information.
			for (cat_set_t::iterator it = temp_cats.begin();
				 it != temp_cats.end(); ++it)
			{
				LLViewerInventoryCategory* llvic = *it;
				if (llvic)
				{
					llvic->setVersionUnknown();
					addCategory(llvic);
				}
			}
		}

		// Invalidate all categories that failed fetching descendents for
		// whatever reason (e.g. one of the descendents was a broken link).
		for (cat_set_t::iterator invalid_cat_it = invalid_categories.begin();
			 invalid_cat_it != invalid_categories.end();
			 invalid_cat_it++)
		{
			LLViewerInventoryCategory* cat = invalid_cat_it->get();
			cat->setVersionUnknown();
			llinfos << "Invalidating category name: " << cat->getName()
					<< " - UUID: " << cat->getUUID()
					<< ", due to invalid descendents cache" << llendl;
		}

		// At this point, we need to set the known descendents for each
		// category which successfully cached so that we do not needlessly
		// fetch descendents for categories which we have.
		update_map_t::const_iterator no_child_counts = child_counts.end();
		for (cat_set_t::iterator it = temp_cats.begin();
			 it != temp_cats.end(); ++it)
		{
			LLViewerInventoryCategory* cat = it->get();
			if (!cat->isVersionUnknown())
			{
				update_map_t::const_iterator the_count =
					child_counts.find(cat->getUUID());
				if (the_count != no_child_counts)
				{
					const S32 num_descendents = the_count->second.mValue;
					cat->setDescendentCount(num_descendents);
				}
				else
				{
					cat->setDescendentCount(0);
				}
			}
		}

		if (remove_inventory_file)
		{
			// Clean up the gunzipped file.
			LLFile::remove(inventory_filename);
		}
		if (is_cache_obsolete)
		{
			// If out of date, remove the gzipped file too.
			llwarns << "Inv cache out of date, removing" << llendl;
			LLFile::remove(gzip_filename);
		}
		categories.clear(); // will unref and delete entries
	}

	llinfos << "Successfully loaded " << cached_category_count
			<< " categories and " << cached_item_count << " items from cache."
			<< llendl;

	return rv;
}

// This is a brute force method to rebuild the entire parent-child relations.
// The overall operation has O(NlogN) performance, which should be sufficient
// for our needs.
void LLInventoryModel::buildParentChildMap()
{
	llinfos << "Building parent child map..." << llendl;

	// *NOTE: I am skipping the logic around folder version synchronization
	// here because it seems if a folder is lost, we might actually want to
	// invalidate it at that point - not attempt to cache. More time &
	// thought is necessary.

	// First the categories. We will copy all of the categories into a
	// temporary container to iterate over (oh for real iterators). While we
	// are at it, we will allocate the arrays in the trees.
	cat_array_t cats;
	cat_array_t* catsp;
	item_array_t* itemsp;

	for (cat_map_t::iterator cit = mCategoryMap.begin();
		 cit != mCategoryMap.end(); ++cit)
	{
		LLViewerInventoryCategory* cat = cit->second;
		cats.push_back(cat);

		const LLUUID& cat_id = cat->getUUID();
		if (mParentChildCategoryTree.count(cat_id) == 0)
		{
			llassert(!mCategoryLock[cat_id]);
			catsp = new cat_array_t;
			mParentChildCategoryTree[cat_id] = catsp;
		}
		if (mParentChildItemTree.count(cat_id) == 0)
		{
			llassert(!mItemLock[cat_id]);
			itemsp = new item_array_t;
			mParentChildItemTree[cat_id] = itemsp;
		}
	}

	// Insert a special parent for the root - so that lookups on LLUUID::null
	// as the parent work correctly. This is kind of a blatent wastes of space
	// since we allocate a block of memory for the array, but whatever - it is
	// not that much space.
	if (mParentChildCategoryTree.count(LLUUID::null) == 0)
	{
		catsp = new cat_array_t;
		mParentChildCategoryTree[LLUUID::null] = catsp;
	}

	// Now we have a structure with all of the categories that we can iterate
	// over and insert into the correct place in the child category tree.
	S32 count = cats.size();
	S32 lost = 0;
	cat_array_t lost_cats;
	for (S32 i = 0; i < count; ++i)
	{
		LLViewerInventoryCategory* cat = cats[i];
		catsp = getUnlockedCatArray(cat->getParentUUID());
		LLFolderType::EType type = cat->getPreferredType();
		// *HACK: work-around for bogus OpenSim servers
		if (type == LLFolderType::FT_ROOT_INVENTORY_OS && !gIsInSecondLife)
		{
			llwarns << "Found bad inventory root type (9 instead or 8) for folder "
					<< cat->getName() << llendl;
			type = LLFolderType::FT_ROOT_INVENTORY;
		}
		if (catsp &&
			// Only the two root folders should be children of null. Others
			// should go to Lost And Found.
			(cat->getParentUUID().notNull() ||
			 type == LLFolderType::FT_ROOT_INVENTORY))
		{
			catsp->push_back(cat);
		}
		else
		{
			// *NOTE: This process could be a lot more efficient if we used the
			// new MoveInventoryFolder message, but we would have to continue
			// to do the update & build here. So, to implement it, we would
			// need a set or map of uuid pairs which would be (folder_id,
			// new_parent_id) to be sent up to the server.
			llinfos << "Lost category: " << cat->getUUID() << " - "
					<< cat->getName() << " with parent:"
					<< cat->getParentUUID() << llendl;
			++lost;
			lost_cats.push_back(cat);
		}
	}
	if (lost)
	{
		llwarns << "Found  " << lost << " lost categories." << llendl;
	}

	const LLUUID& laf = getLostAndFoundID();
	bool ais_available = AISAPI::isAvailable();
	// Do moves in a separate pass to make sure we have properly filed the
	// FT_LOST_AND_FOUND category before we try to find its UUID.
	for (S32 i = 0, count = lost_cats.size(); i < count; ++i)
	{
		LLViewerInventoryCategory* cat = lost_cats[i];
		if (!cat) continue;	// Paranoia

		// Plop it into the Lost And Found.
		LLFolderType::EType pref = cat->getPreferredType();
		// *HACK: work-around for bogus OpenSim servers
		if (pref == LLFolderType::FT_ROOT_INVENTORY_OS && !gIsInSecondLife)
		{
			pref = LLFolderType::FT_ROOT_INVENTORY;
		}
		if (pref == LLFolderType::FT_NONE)
		{
			cat->setParent(laf);
		}
		else if (pref == LLFolderType::FT_ROOT_INVENTORY)
		{
			// It is the root
			cat->setParent(LLUUID::null);
		}
		else
		{
			// It is a protected folder.
			cat->setParent(gInventory.getRootFolderID());
		}
		// UpdateServer() uses AIS, but AIS cat move is not implemented yet.
		if (ais_available)
		{
			cat->updateParentOnServer(false);
		}
		else
		{
			// *FIXME: note that updateServer() fails with protected types, so
			// this will not work as intended in that case.
			cat->updateServer(true);
		}
		catsp = getUnlockedCatArray(cat->getParentUUID());
		if (catsp)
		{
			catsp->push_back(cat);
		}
		else
		{
			llwarns << "Lost and found Not there !" << llendl;
		}
	}

	// Now the items. We allocated in the last step, so now all we have to do
	// is iterate over the items and put them in the right place.
	item_array_t items;
	if (!mItemMap.empty())
	{
		LLPointer<LLViewerInventoryItem> item;
		for (item_map_t::iterator it = mItemMap.begin();
			 it != mItemMap.end(); ++it)
		{
			item = it->second;
			items.emplace_back(item);
		}
	}
	count = items.size();
	lost = 0;
	uuid_vec_t lost_item_ids;
	for (S32 i = 0; i < count; ++i)
	{
		LLPointer<LLViewerInventoryItem> item = items[i];
		itemsp = getUnlockedItemArray(item->getParentUUID());
		if (itemsp)
		{
			itemsp->emplace_back(item);
		}
		else
		{
			llinfos << "Lost item: " << item->getUUID() << " - "
					<< item->getName() << llendl;
			++lost;
			// Plop it into the Lost And Found.
			item->setParent(laf);
#if 0		// Move it later using a special message to move items. If we
			// update server here, the client might crash.
			item->updateServer();
#endif
			lost_item_ids.emplace_back(item->getUUID());
			itemsp = getUnlockedItemArray(item->getParentUUID());
			if (itemsp)
			{
				itemsp->emplace_back(item);
			}
			else
			{
				llwarns << "Lost and found not there !" << llendl;
			}
		}
	}
	if (lost)
	{
		llwarns << "Found " << lost << " lost items." << llendl;
		LLMessageSystem* msg = gMessageSystemp;
		bool start_new_message = true;
		for (uuid_vec_t::iterator it = lost_item_ids.begin();
			 it < lost_item_ids.end(); ++it)
		{
			if (start_new_message)
			{
				start_new_message = false;
				msg->newMessageFast(_PREHASH_MoveInventoryItem);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->addBoolFast(_PREHASH_Stamp, false);
			}
			msg->nextBlockFast(_PREHASH_InventoryData);
			msg->addUUIDFast(_PREHASH_ItemID, (*it));
			msg->addUUIDFast(_PREHASH_FolderID, laf);
			msg->addString("NewName", NULL);
			if (msg->isSendFull(NULL))
			{
				start_new_message = true;
				gAgent.sendReliableMessage();
			}
		}
		if (!start_new_message)
		{
			gAgent.sendReliableMessage();
		}
	}

	const LLUUID& agent_inv_root_id = gInventory.getRootFolderID();
	if (agent_inv_root_id.notNull())
	{
		// 'My Inventory', root of the agent's inventory found. The inventory
		// tree is built.
		mIsAgentInvUsable = true;

		llinfos << "Inventory initialized, notifying observers" << llendl;
		addChangedMask(LLInventoryObserver::ALL, LLUUID::null);
		notifyObservers();
	}
}

// Would normally do this at construction but that is too early in the process
// for gInventory. Have the first requestPost() call set things up.
void LLInventoryModel::initHttpRequest()
{
	if (!mHttpRequestFG)
	{
		// Have not initialized, get to it
		LLAppCoreHttp& app_core_http = gAppViewerp->getAppCoreHttp();

		mHttpRequestFG = new LLCore::HttpRequest;
		mHttpRequestBG = new LLCore::HttpRequest;

		mHttpOptions = DEFAULT_HTTP_OPTIONS;
		mHttpOptions->setTransferTimeout(300);
		mHttpOptions->setUseRetryAfter(true);
#if 0	// Enable to do tracing of requests
		mHttpOptions->setTrace(2);
#endif
		mHttpHeaders = DEFAULT_HTTP_HEADERS;
		mHttpHeaders->append(HTTP_OUT_HEADER_CONTENT_TYPE,
							 HTTP_CONTENT_LLSD_XML);
		mHttpHeaders->append(HTTP_OUT_HEADER_ACCEPT,
							 HTTP_CONTENT_LLSD_XML);

		mHttpPolicyClass = app_core_http.getPolicy(LLAppCoreHttp::AP_INVENTORY);
	}

	if (!gGenericDispatcher.isHandlerPresent(_PREHASH_BulkUpdateInventory))
	{
		gGenericDispatcher.addHandler(_PREHASH_BulkUpdateInventory,
									  &sBulkUpdateInventory);
	}
}

void LLInventoryModel::handleResponses(bool foreground)
{
	if (foreground && mHttpRequestFG)
	{
		mHttpRequestFG->update(0);
	}
	else if (!foreground && mHttpRequestBG)
	{
		mHttpRequestBG->update(50000L);
	}
}

LLCore::HttpHandle LLInventoryModel::requestPost(bool foreground,
												 const std::string& url,
												 const LLSD& body,
												 const LLCore::HttpHandler::ptr_t& handler,
												 const char* message)
{
	if (!mHttpRequestFG)
	{
		// We do the initialization late and lazily as this class is statically
		// constructed and not all the bits are ready at that time.
		initHttpRequest();
	}

	LLCore::HttpRequest* request = foreground ? mHttpRequestFG
											  : mHttpRequestBG;
	LLCore::HttpHandle handle =
		LLCoreHttpUtil::requestPostWithLLSD(request, mHttpPolicyClass,
											url, body, mHttpOptions,
											mHttpHeaders, handler);
	if (handle == LLCORE_HTTP_HANDLE_INVALID)
	{
		LLCore::HttpStatus status = request->getStatus();
		llwarns << "HTTP POST request failed for " << message
				<< " - Status: " << status.toTerseString() << " - Reason: '"
				<< status.toString() << "'" << llendl;
	}

	return handle;
}

//static
bool LLInventoryModel::loadFromFile(const std::string& filename,
									LLInventoryModel::cat_array_t& categories,
									LLInventoryModel::item_array_t& items,
									uuid_list_t& cats_to_update,
									bool& is_cache_obsolete)
{
	// Cache is considered obsolete until proven current
	is_cache_obsolete = true;

	if (filename.empty())
	{
		llerrs << "Filename is empty !" << llendl;
		return false;
	}
	llinfos << "Loading cached inventory from file: " << filename << llendl;

	llifstream file(filename.c_str());
	if (!file.is_open())
	{
		llinfos << "Unable to load inventory from: " << filename << llendl;
		return false;
	}

	std::string line;
	LLPointer<LLSDParser> parser(new LLSDNotationParser);
	while (std::getline(file, line))
	{
		LLSD s_item;
		std::istringstream iss(line);
		if (parser->parse(iss, s_item,
						  line.length()) == LLSDParser::PARSE_FAILURE)
		{
			llwarns << "Parsing inventory cache failed, line:\n" << line
					<< llendl;
			continue;
		}

		if (s_item.has("inv_cache_version"))
		{
			S32 version = s_item["inv_cache_version"].asInteger();
			if (version == INVENTORY_CACHE_VERSION)
			{
				// Cache is up to date
				is_cache_obsolete = false;
				continue;
			}
			else
			{
				// Cache is out of date
				llwarns << "Inventory is outdated" << llendl;
				break;
			}
		}
		if (s_item.has("cat_id"))
		{
			LLPointer<LLViewerInventoryCategory> inv_cat =
				new LLViewerInventoryCategory(LLUUID::null);
			if (inv_cat->importLLSD(s_item))
			{
				categories.emplace_back(inv_cat);
			}
			continue;
		}
		if (s_item.has("item_id"))
		{
			LLPointer<LLViewerInventoryItem> inv_item =
				new LLViewerInventoryItem;
			if (inv_item->fromLLSD(s_item))
			{
				if (inv_item->getUUID().isNull())
				{
					llwarns << "Ignoring inventory with null item id: "
							<< inv_item->getName() << llendl;
				}
				else if (inv_item->getType() == LLAssetType::AT_NONE)
				{
					cats_to_update.insert(inv_item->getParentUUID());
				}
				else
				{
					items.emplace_back(inv_item);
				}
			}
		}
	}

	file.close();

	return !is_cache_obsolete;
}

//static
bool LLInventoryModel::saveToFile(const std::string& filename,
								  const cat_array_t& categories,
								  const item_array_t& items)
{
	if (filename.empty())
	{
		llerrs << "Filename is empty !" << llendl;
		return false;
	}
	llinfos << "Saving cached inventory to file: " << filename << llendl;

	llofstream file(filename.c_str());
	if (!file.is_open())
	{
		llwarns << "Unable to open file: " << filename << llendl;
		return false;
	}

	LLSD cache_ver;
	cache_ver["inv_cache_version"] = INVENTORY_CACHE_VERSION;
	file << LLSDOStreamer<LLSDNotationFormatter>(cache_ver) << std::endl;
	if (file.fail())
	{
		llwarns << "Failed to write cache version to file. Unable to save inventory to: "
				<< filename << llendl;
		return false;
	}

	S32 cat_count = 0;
	for (S32 i = 0, count = categories.size(); i < count; ++i)
	{
		LLViewerInventoryCategory* catp = categories[i];
		if (!catp->isVersionUnknown())
		{
			file << LLSDOStreamer<LLSDNotationFormatter>(catp->exportLLSD())
				 << std::endl;
			if (file.fail())
			{
				llwarns << "Failed to write a folder to file. Unable to save inventory to: "
						<< filename << llendl;
				return false;
			}
			++cat_count;
		}
	}

	S32 it_count = items.size();
	for (S32 i = 0; i < it_count; ++i)
	{
		file << LLSDOStreamer<LLSDNotationFormatter>(items[i]->asLLSD())
			 << std::endl;
		if (file.fail())
		{
			llwarns << "Failed to write an item to file. Unable to save inventory to: "
					<< filename << llendl;
			return false;
		}
	}

	file.close();

	llinfos << "Saved " << it_count << " items in " << cat_count
			<< " categories." << llendl;

	return true;
}

//----------------------------------------------------------------------------
// Message handling functionality
//----------------------------------------------------------------------------

//static
void LLInventoryModel::registerCallbacks(LLMessageSystem* msg)
{
	msg->setHandlerFuncFast(_PREHASH_UpdateCreateInventoryItem,
							processUpdateCreateInventoryItem, NULL);
	msg->setHandlerFuncFast(_PREHASH_RemoveInventoryItem,
							processRemoveInventoryItem, NULL);
	msg->setHandlerFuncFast(_PREHASH_RemoveInventoryFolder,
							processRemoveInventoryFolder, NULL);
	msg->setHandlerFuncFast(_PREHASH_RemoveInventoryObjects,
							processRemoveInventoryObjects, NULL);
	msg->setHandlerFuncFast(_PREHASH_SaveAssetIntoInventory,
							processSaveAssetIntoInventory, NULL);
	msg->setHandlerFuncFast(_PREHASH_BulkUpdateInventory,
							processBulkUpdateInventory, NULL);
	msg->setHandlerFunc(_PREHASH_MoveInventoryItem, processMoveInventoryItem);
#if 1
	// Note: 2023-10: this was removed from LL's viewer. Verify if still in
	// actual use in OpenSim, or when AISv3 is switched off. HB
	msg->setHandlerFuncFast(_PREHASH_UpdateInventoryFolder,
							processUpdateInventoryFolder, NULL);
	// Note: thees were removed a while ago from LL's viewer. Verify if still
	// in actual use in OpenSim. HB
	msg->setHandlerFunc(_PREHASH_FetchInventoryReply,
						processFetchInventoryReply);
	msg->setHandlerFunc(_PREHASH_InventoryDescendents,
						processInventoryDescendents);
#endif
}

//static
void LLInventoryModel::processUpdateCreateInventoryItem(LLMessageSystem* msg,
														void**)
{
	// Do accounting and highlight new items if they arrive
	if (gInventory.messageUpdateCore(msg, true, LLInventoryObserver::CREATE))
	{
		U32 callback_id;
		LLUUID item_id;
		msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_ItemID, item_id);
		msg->getU32Fast(_PREHASH_InventoryData, _PREHASH_CallbackID,
						callback_id);

		gInventoryCallbacks.fire(callback_id, item_id);

		if (LLInventoryModelFetch::useAISFetching())
		{
			// *TODO: instead of unpacking message fully, grab only an item_id,
			// then fetch.
			LLInventoryModelFetch::getInstance()->scheduleItemFetch(item_id,
																	true);
		}
	}
}

bool LLInventoryModel::messageUpdateCore(LLMessageSystem* msg, bool account,
										 U32 mask)
{
	// NOTE: crashes may happen as a result of the stale calling of this method
	// on logout. So test for the logging out or quitting flags, and abort when
	// any is true. HB.
	if (gLogoutInProgress || LLApp::isQuitting())
	{
		llwarns << "Application is quitting: skipping stale inventory message update."
				<< llendl;
		return false;
	}

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got an inventory update for the wrong agent: " << agent_id
				<< llendl;
		return false;
	}
	item_array_t items;
	update_map_t update;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_InventoryData);
	// Does this loop ever execute more than once ?
	for (S32 i = 0; i < count; ++i)
	{
		LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
		titem->unpackMessage(msg, _PREHASH_InventoryData, i);
		const LLUUID& item_id = titem->getUUID();
		if (item_id.isNull())
		{
			llwarns << "Null item Id, skipping..." << llendl;
			continue;
		}
		const LLUUID& parent_id = titem->getParentUUID();
		LL_DEBUGS("Inventory") << "Processing item id: " << item_id
							   << " - parent id: " << parent_id << LL_ENDL;
		items.emplace_back(titem);
		// Examine update for changes.
		LLViewerInventoryItem* itemp = getItem(item_id);
		if (itemp)
		{
			const LLUUID& old_parent_id = itemp->getParentUUID();
			if (parent_id == old_parent_id)
			{
				if (parent_id.notNull())
				{
					update[parent_id];
				}
				else
				{
					llwarns << "Null parent Id for item " << item_id << llendl;
				}
			}
			else
			{
				if (parent_id.notNull())
				{
					++update[parent_id];
				}
				else
				{
					llwarns << "Null new parent id for item " << item_id
							<< llendl;
				}
				if (old_parent_id.notNull())
				{
					--update[old_parent_id];
				}
				else
				{
					llwarns << "Null old parent id for item " << item_id
							<< llendl;
				}
			}
		}
		else if (parent_id.notNull())
		{
			++update[parent_id];
		}
		else
		{
			llwarns << "Null new parent id for non-found item " << item_id
					<< llendl;
		}
	}
	if (account)
	{
		accountForUpdate(update);
		mask |= LLInventoryObserver::CREATE;
	}

	// As above, this loop never seems to loop more than once per call
	for (item_array_t::iterator it = items.begin(); it != items.end(); ++it)
	{
		updateItem(*it, mask);
	}
	notifyObservers();

	if (gWindowp)
	{
		gWindowp->decBusyCount();
	}

	return true;
}

//static
void LLInventoryModel::removeInventoryItem(LLUUID agent_id,
										   LLMessageSystem* msg,
										   const char* msg_label)
{
	LLUUID item_id;
	S32 count = msg->getNumberOfBlocksFast(msg_label);
	LL_DEBUGS("Inventory") << "Message has " << count << " item blocks"
						   << LL_ENDL;
	uuid_vec_t item_ids;
	update_map_t update;
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUIDFast(msg_label, _PREHASH_ItemID, item_id, i);
		LL_DEBUGS("Inventory") << "Checking for item-to-be-removed " << item_id
							   << LL_ENDL;
		LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
		if (itemp)
		{
			LL_DEBUGS("Inventory") << "Item will be removed " << item_id
								   << LL_ENDL;
			// We only bother with the delete and account if we found the item:
			// this is usually a back-up for permissions, so frequently the
			// item will already be gone.
			const LLUUID& parent_id = itemp->getParentUUID();
			if (parent_id.notNull())
			{
				--update[parent_id];
			}
			else
			{
				llwarns << "Null parent Id for item " << item_id << llendl;
			}
			item_ids.emplace_back(item_id);
		}
	}
	gInventory.accountForUpdate(update);
	for (U32 i = 0, count = item_ids.size(); i < count; ++i)
	{
		const LLUUID& item_id = item_ids[i];
		LL_DEBUGS("Inventory") << "Calling deleteObject " << item_id << LL_ENDL;
		gInventory.deleteObject(item_id);
	}
}

//static
void LLInventoryModel::processRemoveInventoryItem(LLMessageSystem* msg, void**)
{
	LLUUID agent_id, item_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got a RemoveInventoryItem for the wrong agent."
				<< llendl;
		return;
	}
	removeInventoryItem(agent_id, msg, _PREHASH_InventoryData);
	gInventory.notifyObservers();
}

#if 1	// Note: this was removed from LL's viewer a while ago. Verify if still
		// in actual use in OpenSim. HB
//static
void LLInventoryModel::processUpdateInventoryFolder(LLMessageSystem* msg,
													void**)
{
	// Warn once, so that we can notice its continued usage...
	llwarns_once << "This supposedly deprecated callback got called !!!"
				 << llendl;

	LLUUID agent_id, folder_id, parent_id;
	//char name[DB_INV_ITEM_NAME_BUF_SIZE];
	msg->getUUIDFast(_PREHASH_FolderData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got an UpdateInventoryFolder for the wrong agent."
				<< llendl;
		return;
	}

//MK
	bool check_rlv_share =
		gRLenabled && gRLInterface.getRlvShare() &&
		!gSavedSettings.getBool("RestrainedLoveForbidGiveToRLV");
	std::vector<LLPointer<LLViewerInventoryCategory> > folders_to_move;
//mk

	LLPointer<LLViewerInventoryCategory> lastfolder;	// *HACK part 1
	cat_array_t folders;
	update_map_t update;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_FolderData);
	for (S32 i = 0; i < count; ++i)
	{
		LLPointer<LLViewerInventoryCategory> tfolder =
			new LLViewerInventoryCategory(gAgentID);
		lastfolder = tfolder;
		tfolder->unpackMessage(msg, _PREHASH_FolderData, i);
		// Make sure it is not a protected folder
		tfolder->setPreferredType(LLFolderType::FT_NONE);
		folders.emplace_back(tfolder);
		const LLUUID& parent_id = tfolder->getParentUUID();
		LLViewerInventoryCategory* folderp = gInventory.getCategory(parent_id);
		const LLUUID& new_folder_id = tfolder->getUUID();
		// Examine update for changes.
		if (folderp)
		{
			const LLUUID& old_parent_id = folderp->getParentUUID();
			if (parent_id == old_parent_id)
			{
				if (parent_id.notNull())
				{
					update[parent_id];
				}
				else
				{
					llwarns << "Null parent Id for folder " << new_folder_id
							<< llendl;
				}
			}
			else
			{
				if (parent_id.notNull())
				{
					++update[parent_id];
				}
				else
				{
					llwarns << "Null new parent Id for folder "
							<< new_folder_id << llendl;
				}
				if (old_parent_id.notNull())
				{
					--update[old_parent_id];
				}
				else
				{
					llwarns << "Null old parent Id for folder "
						    << new_folder_id << llendl;
				}
			}
		}
		else if (parent_id.notNull())
		{
			++update[parent_id];
		}
		else
		{
			llwarns << "Null parent Id for non-found folder "
					<< new_folder_id << llendl;
		}
//MK
		if (check_rlv_share &&
			gRLInterface.shouldMoveToSharedSubFolder(tfolder))
		{
			folders_to_move.emplace_back(tfolder);
		}
//mk
	}

	gInventory.accountForUpdate(update);
	for (cat_array_t::iterator it = folders.begin(); it != folders.end(); ++it)
	{
		gInventory.updateCategory(*it);
	}
	gInventory.notifyObservers();

//MK
	for (U32 i = 0, count = folders_to_move.size(); i < count; ++i)
	{
		gRLInterface.moveToSharedSubFolder(folders_to_move[i].get());
	}
//mk

	// *HACK part 2: Do the 'show' logic for a new item in the inventory.
	LLFloaterInventory* inv = LLFloaterInventory::getActiveFloater();
	if (inv)
	{
		inv->getPanel()->setSelection(lastfolder->getUUID(), TAKE_FOCUS_NO);
	}
}
#endif

//static
void LLInventoryModel::removeInventoryFolder(LLUUID agent_id,
											 LLMessageSystem* msg)
{
	LLUUID folder_id;
	uuid_vec_t folder_ids;
	update_map_t update;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_FolderData);
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_FolderData, _PREHASH_FolderID, folder_id, i);
		LLViewerInventoryCategory* folderp = gInventory.getCategory(folder_id);
		if (folderp)
		{
			const LLUUID& parent_id = folderp->getParentUUID();
			if (parent_id.notNull())
			{
				--update[parent_id];
			}
			else
			{
				llwarns << "Null parent Id for folder " << folder_id << llendl;
			}
			folder_ids.emplace_back(folder_id);
		}
	}
	gInventory.accountForUpdate(update);
	for (U32 i = 0, count = folder_ids.size(); i < count; ++i)
	{
		const LLUUID& cat_id = folder_ids[i];
		LL_DEBUGS("Inventory") << "Calling deleteObject " << cat_id << LL_ENDL;
		gInventory.deleteObject(cat_id);
	}
}

//static
void LLInventoryModel::processRemoveInventoryFolder(LLMessageSystem* msg,
													void**)
{
	LLUUID agent_id, folder_id;
	msg->getUUIDFast(_PREHASH_FolderData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got a RemoveInventoryFolder for the wrong agent."
				<< llendl;
		return;
	}
	removeInventoryFolder(agent_id, msg);
	gInventory.notifyObservers();
}

//static
void LLInventoryModel::processRemoveInventoryObjects(LLMessageSystem* msg,
													 void**)
{
	LLUUID agent_id, session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	LL_DEBUGS("Inventory") << "Remove inventory objects: " << session_id
						   << LL_ENDL;
	if (agent_id != gAgentID)
	{
		llwarns << "Got a RemoveInventoryObjects for the wrong agent."
				<< llendl;
		return;
	}
	removeInventoryFolder(agent_id, msg);
	removeInventoryItem(agent_id, msg, _PREHASH_ItemData);
	gInventory.notifyObservers();
}

//static
void LLInventoryModel::processSaveAssetIntoInventory(LLMessageSystem* msg,
													 void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got a SaveAssetIntoInventory message for the wrong agent."
				<< llendl;
		return;
	}

	LLUUID item_id;
	msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_ItemID, item_id);

	// The viewer ignores the asset id because this message is only used for
	// attachments/objects, so the asset id is not used in the viewer anyway.
	LL_DEBUGS("Inventory") << "Processing itemID = " << item_id << LL_ENDL;
	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if (item)
	{
		LLCategoryUpdate up(item->getParentUUID(), 0);
		gInventory.accountForUpdate(up);
		gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		gInventory.notifyObservers();
	}
	else
	{
		llinfos << "Item not found: " << item_id << llendl;
	}
	if (gViewerWindowp)
	{
		gWindowp->decBusyCount();
	}
}

//static
void LLInventoryModel::processBulkUpdateInventory(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got a BulkUpdateInventory for the wrong agent." << llendl;
		return;
	}
	LLUUID tid;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_TransactionID, tid);
	LL_DEBUGS("Inventory") << "Bulk inventory: " << tid << LL_ENDL;

//MK
	bool check_rlv_share =
		gRLenabled && gRLInterface.getRlvShare() &&
		!gSavedSettings.getBool("RestrainedLoveForbidGiveToRLV");
	std::vector<LLPointer<LLViewerInventoryCategory> > folders_to_move;
//mk

	update_map_t update;
	cat_array_t folders;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_FolderData);
	for (S32 i = 0; i < count; ++i)
	{
		LLPointer<LLViewerInventoryCategory> tfolder =
			new LLViewerInventoryCategory(gAgentID);
		tfolder->unpackMessage(msg, _PREHASH_FolderData, i);
		const LLUUID& folder_id = tfolder->getUUID();
		if (folder_id.isNull())
		{
			LL_DEBUGS("Inventory") << "Null folder Id, skipping." << LL_ENDL;
			continue;
		}

		const LLUUID& parent_id = tfolder->getParentUUID();
		LL_DEBUGS("Inventory") << "Unpacked folder '" << tfolder->getName()
							   << "' (" << folder_id << ") in " << parent_id
							   << LL_ENDL;

		// If the folder is a listing or a version folder, all we need to do is
		// to update the SLM data
		if (LLMarketplace::updateIfListed(folder_id, parent_id))
		{
			// In that case, there is no item to update so no callback, so we
			// skip the rest of the update
			continue;
		}

		LLViewerInventoryCategory* folderp = gInventory.getCategory(parent_id);
		folders.emplace_back(tfolder);
		if (folderp)
		{
			const LLUUID& old_parent_id = folderp->getParentUUID();
			if (folderp->isVersionUnknown())
			{
				 folderp->fetch();
			}
			else if (parent_id == old_parent_id)
			{
				if (parent_id.notNull())
				{
					update[parent_id];
				}
				else
				{
					llwarns << "Null parent Id for folder " << folder_id
							<< llendl;
				}
			}
			else
			{
				if (parent_id.notNull())
				{
					++update[parent_id];
				}
				else
				{
					llwarns << "Null new parent Id for folder " << folder_id
							<< llendl;
				}
				if (old_parent_id.notNull())
				{
					--update[old_parent_id];
				}
				else
				{
					llwarns << "Null old parent Id for folder " << folder_id
							<< llendl;
				}
			}
		}
		else if (parent_id.notNull())
		{
			// We could not find the folder, so it is probably new.
			// However, we only want to attempt accounting for the parent
			// if we can find the parent.
			folderp = gInventory.getCategory(parent_id);
			if (folderp)
			{
				if (folderp->isVersionUnknown())
				{
				 	folderp->fetch();
				}
				else
				{
					++update[parent_id];
				}
			}
		}
		else
		{
			llwarns << "Null new parent Id for non-found folder " << folder_id
					<< llendl;
		}
//MK
		if (check_rlv_share &&
			gRLInterface.shouldMoveToSharedSubFolder(tfolder))
		{
			folders_to_move.emplace_back(tfolder);
		}
//mk
	}

	count = msg->getNumberOfBlocksFast(_PREHASH_ItemData);
	uuid_vec_t wearable_ids;
	item_array_t items;
	typedef std::list<InventoryCallbackInfo> cblist_t;
	cblist_t callback_list;
	for (S32 i = 0; i < count; ++i)
	{
		LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
		titem->unpackMessage(msg, _PREHASH_ItemData, i);
		const LLUUID& item_id = titem->getUUID();
		const LLUUID& parent_id = titem->getParentUUID();
		LL_DEBUGS("Inventory") << "Unpacked item '" << titem->getName()
							   << "' in " << parent_id << LL_ENDL;
		U32 callback_id;
		msg->getU32Fast(_PREHASH_ItemData, _PREHASH_CallbackID, callback_id);
		if (item_id.isNull())
		{
			llwarns << "Null item Id, skipping..." << llendl;
			continue;
		}
		items.emplace_back(titem);
		if (titem->getInventoryType() == LLInventoryType::IT_WEARABLE)
		{
			wearable_ids.emplace_back(item_id);
		}
		callback_list.emplace_back(callback_id, item_id);
		// Examine update for changes.
		LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
		if (itemp)
		{
			const LLUUID& old_parent_id = itemp->getParentUUID();
			if (parent_id == old_parent_id)
			{
				if (parent_id.notNull())
				{
					update[parent_id];
				}
				else
				{
					llwarns << "Null parent Id for item " << item_id << llendl;
				}
			}
			else
			{
				if (parent_id.notNull())
				{
					++update[parent_id];
				}
				else
				{
					llwarns << "Null new parent Id for item " << item_id
							<< llendl;
				}
				if (old_parent_id.notNull())
				{
					--update[old_parent_id];
				}
				else
				{
					llwarns << "Null old parent Id for item " << item_id
							<< llendl;
				}
			}
		}
		else
		{
			LLViewerInventoryCategory* folderp =
				gInventory.getCategory(parent_id);
			if (folderp)
			{
				++update[parent_id];
			}
		}
	}

	LLInventoryModelFetch* fetcherp = NULL;
	if (LLInventoryModelFetch::useAISFetching())
	{
		fetcherp = LLInventoryModelFetch::getInstance();
	}

	gInventory.accountForUpdate(update);
	for (cat_array_t::iterator it = folders.begin(), end = folders.end();
		 it != end; ++it)
	{
		gInventory.updateCategory(*it);
		if (fetcherp)
		{
			// Temporary workaround: just fetch the item using AIS to get the
			// missing fields. If this works fine we might want to extract Ids
			// only from the message then use AIS as a primary fetcher.
			fetcherp->scheduleFolderFetch((*it)->getUUID(), true);
		}
	}
	for (item_array_t::iterator it = items.begin(), end = items.end();
		 it != end; ++it)
	{
		gInventory.updateItem(*it);
		if (fetcherp)
		{
			// Temporary workaround: just fetch the item using AIS to get the
			// missing fields. If this works fine we might want to extract Ids
			// only from the message then use AIS as a primary fetcher.
			fetcherp->scheduleItemFetch((*it)->getUUID(), true);
		}
	}
	gInventory.notifyObservers();

	// The incoming inventory could span more than one BulkInventoryUpdate
	// packet, so record the transaction ID for this purchase, then wear all
	// clothing that comes in as part of that transaction ID. JC
	if (sWearNewClothing)
	{
		sWearNewClothingTransactionID = tid;
		sWearNewClothing = false;
	}

	if (tid.notNull() && tid == sWearNewClothingTransactionID)
	{
		for (S32 i = 0, count = wearable_ids.size(); i < count; ++i)
		{
			LLViewerInventoryItem* wearablep =
				gInventory.getItem(wearable_ids[i]);
			if (wearablep)
			{
				gAppearanceMgr.wearInventoryItemOnAvatar(wearablep, true);
			}
		}
	}

	for (cblist_t::const_iterator it = callback_list.begin(),
								  end = callback_list.end();
		 it != end; ++it)
	{
		const InventoryCallbackInfo& cbinfo = *it;
		gInventoryCallbacks.fire(cbinfo.mCallback, cbinfo.mInvID);
	}

//MK
	for (U32 i = 0, count = folders_to_move.size(); i < count; ++i)
	{
		gRLInterface.moveToSharedSubFolder(folders_to_move[i].get());
	}
//mk
}

#if 1	// Note: this was removed from LL's viewer a while ago. Verify if still
		// in actual use in OpenSim. HB
//static
void LLInventoryModel::processFetchInventoryReply(LLMessageSystem* msg, void**)
{
	llwarns_once << "This supposedly deprecated callback got called !!!"
				 << llendl;

	// No accounting
	gInventory.messageUpdateCore(msg, false);
}

//static
void LLInventoryModel::processInventoryDescendents(LLMessageSystem* msg,
												   void**)
{
	if (!msg) return;

	llwarns_once << "This supposedly deprecated callback got called !!!"
				 << llendl;

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got a UpdateInventoryItem for the wrong agent." << llendl;
		return;
	}

	LLUUID parent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_FolderID, parent_id);
	LLUUID owner_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_OwnerID, owner_id);
	S32 version;
	msg->getS32(_PREHASH_AgentData, _PREHASH_Version, version);
	S32 descendents;
	msg->getS32(_PREHASH_AgentData, _PREHASH_Descendents, descendents);

	S32 count = msg->getNumberOfBlocksFast(_PREHASH_FolderData);
	LLPointer<LLViewerInventoryCategory> tcategory =
		new LLViewerInventoryCategory(owner_id);
	for (S32 i = 0; i < count; ++i)
	{
		tcategory->unpackMessage(msg, _PREHASH_FolderData, i);
		gInventory.updateCategory(tcategory);
	}

	count = msg->getNumberOfBlocksFast(_PREHASH_ItemData);
	LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
	for (S32 i = 0; i < count; ++i)
	{
		titem->unpackMessage(msg, _PREHASH_ItemData, i);
		// If the item has already been added (e.g. from link prefetch),
		// then it doesn't need to be re-added.
		if (gInventory.getItem(titem->getUUID()))
		{
			LL_DEBUGS("Inventory") << "Skipping prefetched item [ Name: "
								   << titem->getName() << " | Type: "
								   << titem->getActualType()
								   << " | ItemUUID: " << titem->getUUID()
								   << " ] " << LL_ENDL;
			continue;
		}
		gInventory.updateItem(titem);
	}

	// Set version and descendentcount according to message.
	LLViewerInventoryCategory* cat = gInventory.getCategory(parent_id);
	if (cat)
	{
		cat->setVersion(version);
		cat->setDescendentCount(descendents);
		// Get this UUID on the changed list so that whatever's listening for
		// it will get triggered.
		gInventory.addChangedMask(LLInventoryObserver::INTERNAL,
								  cat->getUUID());
	}
	gInventory.notifyObservers();
}
#endif

//static
void LLInventoryModel::processMoveInventoryItem(LLMessageSystem* msg, void**)
{
	if (!msg) return;

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got a MoveInventoryItem message for the wrong agent."
				<< llendl;
		return;
	}

	LLUUID item_id;
	LLUUID folder_id;
	std::string new_name;
	bool anything_changed = false;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_InventoryData);
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_ItemID, item_id, i);
		LLViewerInventoryItem* item = gInventory.getItem(item_id);
		if (item)
		{
			LLPointer<LLViewerInventoryItem> new_item =
				new LLViewerInventoryItem(item);
			msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_FolderID,
							 folder_id, i);
			msg->getString("InventoryData", "NewName", new_name, i);

			LL_DEBUGS("Inventory") << "moving item " << item_id
								   << " to folder " << folder_id << LL_ENDL;
			update_list_t update;
			// Old folder - 1 item
			update.emplace_back(item->getParentUUID(), -1);
			// New folder + 1 item
			update.emplace_back(folder_id, 1);
			gInventory.accountForUpdate(update);

			new_item->setParent(folder_id);
			if (new_name.length() > 0)
			{
				new_item->rename(new_name);
			}
			gInventory.updateItem(new_item);
			anything_changed = true;
		}
		else
		{
			llinfos << "Item not found: " << item_id << llendl;
		}
	}
	if (anything_changed)
	{
		gInventory.notifyObservers();
	}
}

//----------------------------------------------------------------------------

// *NOTE: DEBUG functionality
void LLInventoryModel::dumpInventory()
{
	llinfos << "\nBegin Inventory Dump\n**********************:" << llendl;
	llinfos << "mCategroy[] contains " << mCategoryMap.size() << " items."
			<< llendl;
	for (cat_map_t::iterator cit = mCategoryMap.begin();
		 cit != mCategoryMap.end(); ++cit)
	{
		LLViewerInventoryCategory* cat = cit->second;
		if (cat)
		{
			llinfos << "  " <<  cat->getUUID() << " '"
					<< cat->getName() << "' "
					<< cat->getVersion() << " "
					<< cat->getDescendentCount() << " parent: "
					<< cat->getParentUUID() << llendl;
		}
		else
		{
			llinfos << "  NULL category !" << llendl;
		}
	}
	llinfos << "mItemMap[] contains " << mItemMap.size() << " items."
			<< llendl;
	for (item_map_t::iterator iit = mItemMap.begin(); iit != mItemMap.end();
		 ++iit)
	{
		LLViewerInventoryItem* item = iit->second;
		if (item)
		{
			llinfos << "  " << item->getUUID() << " " << item->getName()
					<< " (asset Id: " << item->getAssetUUID() << ")" << llendl;
		}
		else
		{
			llinfos << "  NULL item !" << llendl;
		}
	}
	llinfos << "\n**********************\nEnd Inventory Dump" << llendl;
}

void LLInventoryModel::removeItem(const LLUUID& item_id)
{
	const LLUUID& new_parent = getTrashID();
	LLViewerInventoryItem* item = item_id.notNull() ? getItem(item_id) : NULL;
	if (item && new_parent.notNull())
	{
		changeItemParent(item, new_parent, true);
	}
}

void LLInventoryModel::removeCategory(const LLUUID& category_id)
{
	if (category_id.isNull()) return;

	// Look for previews or gestures and deactivate them
	LLInventoryModel::cat_array_t descendent_categories;
	LLInventoryModel::item_array_t descendent_items;
	gInventory.collectDescendents(category_id, descendent_categories,
								  descendent_items, false);
	for (S32 i = 0, count = descendent_items.size(); i < count; ++i)
	{
		LLInventoryItem* item = descendent_items[i];
		if (!item) continue;	// Paranoia
		const LLUUID& item_id = item->getUUID();

		// Hide any preview
		LLPreview::hide(item_id, true);
		if (item->getType() == LLAssetType::AT_SETTINGS)
		{
			gGestureManager.deactivateGesture(item_id);
		}
		else if (item->getType() == LLAssetType::AT_GESTURE &&
			gGestureManager.isGestureActive(item_id))
		{
			gGestureManager.deactivateGesture(item_id);
		}
	}

	// Go ahead and remove the category now (i.e. move it to the trash)
	LLViewerInventoryCategory* cat = getCategory(category_id);
	if (cat)
	{
		const LLUUID& trash_id = getTrashID();
		if (trash_id.notNull())
		{
			changeCategoryParent(cat, trash_id, true);
		}
	}
}

bool trash_full_callback(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		const LLUUID& trash_id = gInventory.getTrashID();
		if (trash_id.notNull())
		{
			purge_descendents_of(trash_id, NULL);
		}
	}

	return false;
}

void LLInventoryModel::checkTrashOverflow()
{
	static LLCachedControl<U32> max_capacity(gSavedSettings,
											"InventoryTrashMaxCapacity");
	static bool warned = false;
	if (warned) return;

	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	collectDescendents(getTrashID(), cats, items,
					   LLInventoryModel::INCLUDE_TRASH);
	if (items.size() + cats.size() >= (size_t)max_capacity)
	{
		warned = true;	// Do not spam user if they elect not to purge trash
		gNotifications.add("TrashIsFull", LLSD(), LLSD(),
						   boost::bind(&trash_full_callback, _1, _2));
	}
}

void LLInventoryModel::setRootFolderID(const LLUUID& val)
{
	mRootFolderID = val;
}

void LLInventoryModel::setLibraryRootFolderID(const LLUUID& val)
{
	mLibraryRootFolderID = val;
}

void LLInventoryModel::setLibraryOwnerID(const LLUUID& val)
{
	mLibraryOwnerID = val;
}

const LLUUID& LLInventoryModel::getTrashID()
{
	if (mTrashID.isNull())
	{
		mTrashID = findCategoryUUIDForType(LLFolderType::FT_TRASH);
	}
	return mTrashID;
}

const LLUUID& LLInventoryModel::getLostAndFoundID()
{
	if (mLostAndFoundID.isNull())
	{
		mLostAndFoundID =
			findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
	}
	return mLostAndFoundID;
}

//----------------------------------------------------------------------------
// LLInventoryCollectFunctor implementations
//----------------------------------------------------------------------------

//static
bool LLInventoryCollectFunctor::itemTransferCommonlyAllowed(LLInventoryItem* item)
{
	if (!item)
	{
		return false;
	}

	switch (item->getType())
	{
		case LLAssetType::AT_OBJECT:
		{
			if (isAgentAvatarValid() &&
				!gAgentAvatarp->isWearingAttachment(item->getUUID()))
			{
				return true;
			}
			break;
		}

		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
		{
			if (!gAgentWearables.isWearingItem(item->getUUID()))
			{
				return true;
			}
			break;
		}

		default:
			break;
	}

	return true;
}

bool LLIsType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if (cat && mType == LLAssetType::AT_CATEGORY)
	{
		return true;
	}
	return item ? item->getType() == mType : false;
}

bool LLIsNotType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if (cat && mType == LLAssetType::AT_CATEGORY)
	{
		return false;
	}
	return item ? item->getType() != mType : true;
}

bool LLIsTypeWithPermissions::operator()(LLInventoryCategory* cat,
										 LLInventoryItem* item)
{
	if (cat && mType == LLAssetType::AT_CATEGORY)
	{
		return true;
	}
	if (item && item->getType() == mType)
	{
		LLPermissions perm = item->getPermissions();
		if ((perm.getMaskBase() & mPerm) == mPerm)
		{
			return true;
		}
	}
	return false;
}

bool LLBuddyCollector::operator()(LLInventoryCategory*,
								  LLInventoryItem* item)
{
	return get_calling_card_buddy_id((LLViewerInventoryItem*)item).notNull();
}

bool LLUniqueBuddyCollector::operator()(LLInventoryCategory*,
										LLInventoryItem* item)
{
	LLUUID buddy_id = get_calling_card_buddy_id((LLViewerInventoryItem*)item);
	if (buddy_id.isNull() || mFoundIds.count(buddy_id))
	{
		return false;
	}
	mFoundIds.emplace(buddy_id);
	return true;
}

bool LLParticularBuddyCollector::operator()(LLInventoryCategory*,
											LLInventoryItem* item)
{
	return item && item->getType() == LLAssetType::AT_CALLINGCARD &&
		   (item->getCreatorUUID() == mBuddyID ||
			item->getDescription() == mBuddyID.asString());
}

bool LLNameCategoryCollector::operator()(LLInventoryCategory* cat,
										 LLInventoryItem* item)
{
	return cat && !LLStringUtil::compareInsensitive(mName, cat->getName());
}

//----------------------------------------------------------------------------
// LLInventoryCompletionObserver class
//----------------------------------------------------------------------------

void LLInventoryCompletionObserver::changed(U32 mask)
{
	// Scan through the incomplete items and move or erase them as appropriate.
	if (!mIncomplete.empty())
	{
		for (uuid_vec_t::iterator it = mIncomplete.begin();
			 it < mIncomplete.end(); )
		{
			LLViewerInventoryItem* item = gInventory.getItem(*it);
			if (!item)
			{
				it = mIncomplete.erase(it);
				continue;
			}
			if (item->isFinished())
			{
				mComplete.emplace_back(*it);
				it = mIncomplete.erase(it);
				continue;
			}
			++it;
		}
		if (mIncomplete.empty())
		{
			done();
		}
	}
}

void LLInventoryCompletionObserver::watchItem(const LLUUID& id)
{
	if (id.notNull())
	{
		mIncomplete.emplace_back(id);
	}
}

//----------------------------------------------------------------------------
// LLInventoryFetchObserver class
//----------------------------------------------------------------------------

void LLInventoryFetchObserver::changed(U32 mask)
{
	// Scan through the incomplete items and move or erase them as
	// appropriate.
	if (!mIncomplete.empty())
	{
		for (uuid_vec_t::iterator it = mIncomplete.begin();
			 it < mIncomplete.end(); )
		{
			LLViewerInventoryItem* itemp = gInventory.getItem(*it);
			if (!itemp)
			{
				// This happens with the LLGestureInventoryFetchObserver that
				// loads gestures at startup. JC
				it = mIncomplete.erase(it);
				continue;
			}
			if (itemp->isFinished())
			{
				mComplete.emplace_back(*it);
				it = mIncomplete.erase(it);
				continue;
			}
			++it;
		}
		if (mIncomplete.empty())
		{
			done();
		}
	}
}

bool LLInventoryFetchObserver::isFinished() const
{
	return mIncomplete.empty();
}

void fetch_items_from_llsd(const LLSD& items_llsd)
{
	if (!items_llsd.size()) return;

	LLSD body;
	const std::string& url1 = gAgent.getRegionCapability("FetchInventory2");
	if (url1.empty())
	{
		// All grids servers should have this capability now, including current
		// OpenSim servers versions. HB
		llwarns << "Cannot fetch agent inventory items: missing FetchInventory2 capability."
				<< llendl;
	}
	const std::string& url2 = gAgent.getRegionCapability("FetchLib2");
	if (url2.empty())
	{
		// I have seen at least one OpenSim grid without such a capability, but
		// it seems to correspond to an empty/unavailable inventory library,
		// since falling back to the deprecated UDP messaging on that grid to
		// fetch the library did not yield any result either. HB
		llwarns_sparse << "Cannot fetch inventory library items: missing FetchLib2 capability."
					   << llendl;
	}
	if (url1.empty() && url2.empty())
	{
		return;	// Nothing we can do at this point... HB
	}

	body[0]["cap_url"] = url1;
	body[1]["cap_url"] = url2;

	const std::string lib_owner_id = gInventory.getLibraryOwnerID().asString();
	for (size_t i = 0, count = items_llsd.size(); i < count; ++i)
	{
		if (items_llsd[i]["owner_id"].asString() == gAgentID.asString())
		{
			body[0]["items"].append(items_llsd[i]);
		}
		else if (items_llsd[i]["owner_id"].asString() == lib_owner_id)
		{
			body[1]["items"].append(items_llsd[i]);
		}
	}

	static const char* inv_item_str = "inventory item";
	for (size_t i = 0; i < body.size(); ++i)
	{
		if (!body[i].size() || !body[i]["items"].size())
		{
			// Nothing to fetch...
			continue;
		}

		std::string url = body[i]["cap_url"].asString();
		if (url.empty())
		{
			LL_DEBUGS("Inventory") << "No capability to fetch:\n"
								   << ll_pretty_print_sd(body[i]["items"])
								   << LL_ENDL;
			continue;
		}

		body[i]["agent_id"]	= gAgentID;
		LLCore::HttpHandler::ptr_t
			handler(new LLInventoryModel::FetchItemHttpHandler(body[i]));
		gInventory.requestPost(true, url, body[i], handler, inv_item_str);
	}
}

void LLInventoryFetchObserver::fetchItems(const uuid_vec_t& ids)
{
	LLInventoryModelFetch* fetcherp = NULL;
	if (LLInventoryModelFetch::useAISFetching())
	{
		fetcherp = LLInventoryModelFetch::getInstance();
	}
	typedef fast_hmap<LLUUID, uuid_vec_t> requests_by_folders_t;
    requests_by_folders_t requests;

	LLSD items_llsd;
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		const LLUUID& id = ids[i];
		if (id.isNull())
		{
			llwarns_sparse << "Skipping fetch for a null UUID" << llendl;
			continue;
		}

		LLViewerInventoryItem* itemp = gInventory.getItem(id);
		if (itemp)
		{
			if (itemp->isFinished())
			{
				// It is complete, so put it on the complete container.
				mComplete.emplace_back(id);
				continue;
			}
		}
		else if (gInventory.getCategory(id))
		{
			// Ignore categories since they are not items.
			continue;
		}

		// It is incomplete, so put it on the incomplete container, and pack
		// this on the message.
		mIncomplete.emplace_back(id);

#if 0	// This does not work when using AISv3 for fetches, because with the
		// latter, we cannot fetch an item for which we ignore the parent
		// category (case of inventory offers of individual items by other
		// residents), so when itemp == NULL, the fetch systematically fails...
		// HB
		if (fetcherp)
		{
			if (itemp)
			{
				const LLUUID& parent_id = itemp->getParentUUID();
				requests[parent_id].emplace_back(id);
			}
			else
			{
				fetcherp->scheduleItemFetch(id);
			}
		}
		else
		{
			// Prepare the data to fetch
			LLSD item_entry;
			item_entry["owner_id"] = itemp ? itemp->getPermissions().getOwner()
											 // Assume it is agent inventory.
										   : gAgentID;
			item_entry["item_id"] = id;
			items_llsd.append(item_entry);
		}
#else
		// Unconditionnally prepare the data to fetch items via the legacy
		// capabilities, in case we find out later we need to fetch an item we
		// do not now the parent category for... HB
		LLSD item_entry;
		item_entry["owner_id"] = itemp ? itemp->getPermissions().getOwner()
										 // Assume it is agent inventory.
									   : gAgentID;
		item_entry["item_id"] = id;
		items_llsd.append(item_entry);

		// If we want AISv3 fetches, let's try this too...
		if (fetcherp)
		{
			if (itemp)	// This will work fine in this case.
			{
				requests[itemp->getParentUUID()].emplace_back(id);
			}
			else		// Forget it, we do need a parent category !
			{
				LL_DEBUGS("Inventory") << "Parent folder unknown for item "
									   << id
									   << ": falling back to capability fetch."
									   << LL_ENDL;
				fetcherp = NULL;
			}
		}
#endif
	}

	if (!fetcherp)
	{
		fetch_items_from_llsd(items_llsd);
		return;
	}

	for (const auto& folder : requests)
	{
		const LLUUID& cat_id = folder.first;
		S32 count = folder.second.size();
		if (count > MAX_INDIVIDUAL_ITEM_REQUESTS)
		{
			// Requesting one by one would take a while; request the whole
			// folder instead.
			fetcherp->scheduleFolderFetch(cat_id, true);
			continue;
		}
		LLViewerInventoryCategory* catp = gInventory.getCategory(cat_id);
		if (!catp)
		{
			// This should not happen: we should have all folders and if items
			// exist, the folder is supposed to exist as well.
			llwarns << "Missing folder: " << cat_id
					<< ". Fetching items individually." << llendl;
		}
		else if (catp->isVersionUnknown())
		{
			// Start fetching the whole folder since it is not ready either
			// way.
			catp->fetch();
			continue;
		}
		else if (count >= catp->getViewerDescendentCount() ||
				 count >= catp->getDescendentCount())
		{
			// Start fetching the whole folder since we need all items.
			fetcherp->scheduleFolderFetch(cat_id, true);
			continue;
		}
		// If we got here, then we need to fetch items one by one.
		for (const LLUUID& item_id : folder.second)
		{
			fetcherp->scheduleItemFetch(item_id);
		}
	}
}

//----------------------------------------------------------------------------
// LLInventoryFetchDescendentsObserver class
//----------------------------------------------------------------------------

//virtual
void LLInventoryFetchDescendentsObserver::changed(U32 mask)
{
	for (uuid_vec_t::iterator it = mIncompleteFolders.begin();
		 it < mIncompleteFolders.end(); )
	{
		LLViewerInventoryCategory* catp = gInventory.getCategory(*it);
		if (!catp)
		{
			it = mIncompleteFolders.erase(it);
			continue;
		}
		if (isCategoryComplete(catp))
		{
			mCompleteFolders.emplace_back(*it);
			it = mIncompleteFolders.erase(it);
			continue;
		}
		++it;
	}
	if (mIncompleteFolders.empty())
	{
		done();
	}
}

void LLInventoryFetchDescendentsObserver::fetchDescendents(const uuid_vec_t& ids)
{
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		const LLUUID& id = ids[i];

		LLViewerInventoryCategory* catp = gInventory.getCategory(id);
		if (!catp) continue;

		if (isCategoryComplete(catp))
		{
			mCompleteFolders.emplace_back(id);
		}
		else
		{
			// Blindly fetch it without seeing if anything else is fetching it.
			catp->fetch();
			// Add to list of things being downloaded for this observer.
			mIncompleteFolders.emplace_back(id);
		}
	}
}

bool LLInventoryFetchDescendentsObserver::isFinished() const
{
	return mIncompleteFolders.empty();
}

bool LLInventoryFetchDescendentsObserver::isCategoryComplete(LLViewerInventoryCategory* catp)
{
	S32 descendents = catp->getDescendentCount();
	if (descendents == LLViewerInventoryCategory::DESCENDENT_COUNT_UNKNOWN ||
		catp->isVersionUnknown())
	{
		return false;
	}

	// It might be complete; check known descendents against currently
	// available.
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(catp->getUUID(), cats, items);
	if (!cats || !items)
	{
		// Bit of a hack: pretend we are done if they are gone or incomplete.
		// Should never know, but it would suck if this kept tight looping
		// because of incomplete syncing state.
		return true;
	}

	S32 actual_descendents = cats->size() + items->size();
	if (actual_descendents == descendents)
	{
		return true;
	}

	// Error condition, but recoverable. This happens if something was added to
	// the category before it was initialized, so accountForUpdate() did not
	// update descendent count and thus the category thinks it has fewer
	// descendents than it actually has.
	if (actual_descendents > descendents)
	{
		llwarns << "Resyncing descendents count for category "
				<< catp->getName() << ": expected " << descendents
				<< " descendents but actually got " << actual_descendents
				<< "." << llendl;
		catp->setDescendentCount(actual_descendents);
		return true;
	}

	return false;
}

//----------------------------------------------------------------------------
// LLInventoryFetchComboObserver class
//----------------------------------------------------------------------------

void LLInventoryFetchComboObserver::changed(U32 mask)
{
	if (!mIncompleteItems.empty())
	{
		for (uuid_vec_t::iterator it = mIncompleteItems.begin();
			 it < mIncompleteItems.end(); )
		{
			LLViewerInventoryItem* item = gInventory.getItem(*it);
			if (!item)
			{
				it = mIncompleteItems.erase(it);
				continue;
			}
			if (item->isFinished())
			{
				mCompleteItems.emplace_back(*it);
				it = mIncompleteItems.erase(it);
				continue;
			}
			++it;
		}
	}
	if (!mIncompleteFolders.empty())
	{
		for (uuid_vec_t::iterator it = mIncompleteFolders.begin();
			 it < mIncompleteFolders.end(); )
		{
			LLViewerInventoryCategory* cat = gInventory.getCategory(*it);
			if (!cat)
			{
				it = mIncompleteFolders.erase(it);
				continue;
			}
			if (gInventory.isCategoryComplete(*it))
			{
				mCompleteFolders.emplace_back(*it);
				it = mIncompleteFolders.erase(it);
				continue;
			}
			++it;
		}
	}
	if (!mDone && mIncompleteItems.empty() && mIncompleteFolders.empty())
	{
		mDone = true;
		done();
	}
}

void LLInventoryFetchComboObserver::fetch(const uuid_vec_t& folder_ids,
										  const uuid_vec_t& item_ids)
{
	for (U32 i = 0, count = folder_ids.size(); i < count; ++i)
	{
		const LLUUID& id = folder_ids[i];

		LLViewerInventoryCategory* cat = gInventory.getCategory(id);
		if (!cat) continue;

		if (!gInventory.isCategoryComplete(id))
		{
			cat->fetch();
			LL_DEBUGS("Inventory") << "Fetching folder " << id << LL_ENDL;
			mIncompleteFolders.emplace_back(id);
		}
		else
		{
			mCompleteFolders.emplace_back(id);
			LL_DEBUGS("Inventory") << "Completing folder " << id << LL_ENDL;
		}
	}

	// Now for the items: we fetch everything which is not a direct descendent
	// of an incomplete folder because the item will show up in an inventory
	// descendents message soon enough so we do not have to fetch it
	// individually.
	LLSD items_llsd;
	LLUUID owner_id;
	for (U32 i = 0, count = item_ids.size(); i < count; ++i)
	{
		const LLUUID& id = item_ids[i];
		LLViewerInventoryItem* item = gInventory.getItem(id);
		if (!item)
		{
			LL_DEBUGS("Inventory") << "Unable to find item " << id
								   << LL_ENDL;
			continue;
		}
		if (item->isFinished())
		{
			// It is complete, so put it on the complete container.
			mCompleteItems.emplace_back(id);
			LL_DEBUGS("Inventory") << "Completing item " << id << LL_ENDL;
			continue;
		}
		else
		{
			mIncompleteItems.emplace_back(id);
			owner_id = item->getPermissions().getOwner();
		}
		if (std::find(mIncompleteFolders.begin(), mIncompleteFolders.end(),
					  item->getParentUUID()) == mIncompleteFolders.end())
		{
			LLSD item_entry;
			item_entry["owner_id"] = owner_id;
			item_entry["item_id"] = (id);
			items_llsd.append(item_entry);
		}
		else
		{
			LL_DEBUGS("Inventory") << "Not worrying about " << id << LL_ENDL;
		}
	}
	fetch_items_from_llsd(items_llsd);
}

//----------------------------------------------------------------------------
// LLInventoryExistenceObserver class
//----------------------------------------------------------------------------

void LLInventoryExistenceObserver::watchItem(const LLUUID& id)
{
	if (id.notNull())
	{
		mMIA.emplace_back(id);
	}
}

void LLInventoryExistenceObserver::changed(U32 mask)
{
	// Scan through the incomplete items and move or erase them as appropriate.
	if (!mMIA.empty())
	{
		for (uuid_vec_t::iterator it = mMIA.begin(); it < mMIA.end(); )
		{
			LLViewerInventoryItem* item = gInventory.getItem(*it);
			if (!item)
			{
				++it;
				continue;
			}
			mExist.emplace_back(*it);
			it = mMIA.erase(it);
		}
		if (mMIA.empty())
		{
			done();
		}
	}
}

//----------------------------------------------------------------------------
// LLInventoryAddedObserver class
//----------------------------------------------------------------------------

//static
LLInventoryAddedObserver::hashes_map_t
	LLInventoryAddedObserver::sCopiedItemsHashes;

//static
void LLInventoryAddedObserver::registerCopiedItem(const LLUUID& item_id)
{
	LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
	if (itemp)
	{
		// Keep a hash of what we are going to copy, which will be used below
		// to distinguish items we already had in inventory and just copied,
		// from items we have newly received in our inventory. In the case when
		// we copy several identical items, we need to keep track of their
		// count in excess of their hash.
		LLUUID hash = itemp->hashContents();
		hashes_map_t::iterator it = sCopiedItemsHashes.find(hash);
		if (it == sCopiedItemsHashes.end())
		{
			sCopiedItemsHashes.emplace(hash, 1);
		}
		else
		{
			// Already seen, just add 1 to the count of such items.
			++(it->second);
		}
	}	
}

void LLInventoryAddedObserver::changed(U32 mask)
{
	constexpr U32 OBSERVED_CHANGES = LLInventoryObserver::ADD |
									 LLInventoryObserver::CREATE;
	if (!(mask & OBSERVED_CHANGES))
	{
		return;
	}

	for (uuid_list_t::const_iterator it = gInventory.getAddedIDs().begin(),
									 end = gInventory.getAddedIDs().end();
		 it != end; ++it)
	{
		const LLUUID& id = *it;
		// Do not consider items copied from the inventory as newly added
		// items. HB
		LLViewerInventoryItem* itemp = gInventory.getItem(id);
		if (itemp)
		{
			hashes_map_t::iterator iter =
				sCopiedItemsHashes.find(itemp->hashContents());
			if (iter != sCopiedItemsHashes.end())
			{
				if (--(iter->second) == 0)
				{
					// No more such item's copy expected, erase it from map.
					sCopiedItemsHashes.erase(iter);
				}
				continue;
			}
		}
		// This is indeed a new item: add it.
		mAdded.emplace_back(id);
	}

	if (!mAdded.empty())
	{
		done();
	}
}

//----------------------------------------------------------------------------
// LLInventoryTransactionObserver class
//----------------------------------------------------------------------------

void LLInventoryTransactionObserver::changed(U32 mask)
{
	if (!(mask & LLInventoryObserver::ADD))
	{
		return;
	}

	// This could be it: see if we are processing a bulk update
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg->getMessageName() ||
		strcmp(msg->getMessageName(), "BulkUpdateInventory"))
	{
		return;	// Nope... Ignore.
	}

	// We have a match for the message - now check the transaction id.
	LLUUID id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_TransactionID, id);
	if (id == mTransactionID)
	{
		// We found it
		uuid_vec_t folders, items;
		S32 count = msg->getNumberOfBlocksFast(_PREHASH_FolderData);
		for (S32 i = 0; i < count; ++i)
		{
			msg->getUUIDFast(_PREHASH_FolderData, _PREHASH_FolderID, id, i);
			if (id.notNull())
			{
				folders.emplace_back(id);
			}
		}
		count = msg->getNumberOfBlocksFast(_PREHASH_ItemData);
		for (S32 i = 0; i < count; ++i)
		{
			msg->getUUIDFast(_PREHASH_ItemData, _PREHASH_ItemID, id, i);
			if (id.notNull())
			{
				items.emplace_back(id);
			}
		}

		// Call the derived class the implements this method.
		done(folders, items);
	}
}

//----------------------------------------------------------------------------
// LLAssetIDMatches class
//----------------------------------------------------------------------------

bool LLAssetIDMatches ::operator()(LLInventoryCategory*, LLInventoryItem* item)
{
	return item && item->getAssetUUID() == mAssetID;
}

//----------------------------------------------------------------------------
// LLLinkedItemIDMatches class
//----------------------------------------------------------------------------

bool LLLinkedItemIDMatches::operator()(LLInventoryCategory*,
									   LLInventoryItem* item)
{
	return item && item->getIsLinkType() &&
			// A linked item's asset Id must be the compared to the item's Id.
		   item->getLinkedUUID() == mBaseItemID;
}

//----------------------------------------------------------------------------
// LLInventoryModel::FetchItemHttpHandler class
//----------------------------------------------------------------------------

LLInventoryModel::FetchItemHttpHandler::FetchItemHttpHandler(const LLSD& request_sd)
:	LLCore::HttpHandler(),
	mRequestSD(request_sd)
{
}

void LLInventoryModel::FetchItemHttpHandler::onCompleted(LLCore::HttpHandle handle,
														 LLCore::HttpResponse* response)
{
	LLCore::HttpStatus status = response->getStatus();
	if (!status)
	{
		processFailure(status, response);
		return;
	}

	LLCore::BufferArray* body = response->getBody();
	if (!body || !body->size())
	{
		llwarns << "Missing data in inventory item query." << llendl;
		processFailure("HTTP response for inventory item query missing body",
					   response);
		return;
	}

	LLSD body_llsd;
	if (!LLCoreHttpUtil::responseToLLSD(response, true, body_llsd))
	{
		// INFOS-level logging will occur on the parsed failure
		processFailure("HTTP response for inventory item query has malformed LLSD",
					   response);
		return;
	}

	// Expect top-level structure to be a map
	if (!body_llsd.isMap())
	{
		processFailure("LLSD response for inventory item not a map", response);
		return;
	}

	// Check for 200-with-error failures
	//
	// Original responder-based serivce model did not check for these errors.
	// It may be more robust to ignore this condition. With aggregated
	// requests, an error in one inventory item might take down the entire
	// request. So if this instead broke up the aggregated items into single
	// requests, maybe that would make progress. Or perhaps there is structured
	// information that can tell us what went wrong. Need to dig into this and
	// firm up the API.
	if (body_llsd.has("error"))
	{
		processFailure("Inventory application error (200-with-error)",
					   response);
		return;
	}

	processData(body_llsd, response);
}

void LLInventoryModel::FetchItemHttpHandler::processData(LLSD& content,
														 LLCore::HttpResponse* response)
{
	LLInventoryModel::item_array_t items;
	LLInventoryModel::update_map_t update;
	LLSD content_items = content["items"];
	S32 count = content_items.size();

	// Does this loop ever execute more than once ?
	for (S32 i = 0; i < count; ++i)
	{
		LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
		titem->unpackMessage(content_items[i]);
		const LLUUID& item_id = titem->getUUID();
		if (item_id.isNull())
		{
			llwarns << "Null item id. Skipping." << llendl;
			continue;
		}
		const LLUUID& parent_id = titem->getParentUUID();
		LL_DEBUGS("Inventory") << "Success for item id: " << item_id
							   << " - new parent id: " << parent_id
							   << LL_ENDL;
		items.emplace_back(titem);

		// Examine update for changes.
		LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
		if (itemp)
		{
			const LLUUID& old_parent_id = itemp->getParentUUID();
			if (parent_id == old_parent_id)
			{
				if (parent_id.notNull())
				{
					update[parent_id];
				}
				else
				{
					llwarns << "Null parent Id for item " << item_id
							<< llendl;
				}
			}
			else
			{
				if (parent_id.notNull())
				{
					++update[parent_id];
				}
				else
				{
					llwarns << "Null new parent for item " << item_id
							<< llendl;
				}
				if (old_parent_id.notNull())
				{
					--update[old_parent_id];
				}
				else
				{
					llwarns << "Null old parent for item " << item_id
							<< llendl;
				}
			}
		}
		else if (parent_id.notNull())
		{
			++update[parent_id];
		}
		else
		{
			llwarns << "Null new parent id for item " << item_id << llendl;
		}
	}

	// As above, this loop never seems to loop more than once per call
	for (LLInventoryModel::item_array_t::iterator it = items.begin(),
												  end = items.end();
		 it != end; ++it)
	{
		gInventory.updateItem(*it);
	}
	gInventory.notifyObservers();

	if (gWindowp)
	{
		gWindowp->decBusyCount();
	}
}

void LLInventoryModel::FetchItemHttpHandler::processFailure(LLCore::HttpStatus status,
															LLCore::HttpResponse* response)
{
	// Warn once only, because these can get really spammy, when a capability
	// is not found, for example... *TODO: search where to abort failed cap
	// requests.
	llwarns_once << "Inventory item fetch failure - Status: "
				 << status.toTerseString() << " - Reason: "
				 << status.toString() << " - Content-type: "
				 << response->getContentType() << " - Content (abridged): "
				 << LLCoreHttpUtil::responseToString(response) << llendl;

#if 0 // Avoid: "Call was made to notifyObservers within notifyObservers !"
	  // Since there was an error, no update happened anyway... HB
	gInventory.notifyObservers();
#endif
}

void LLInventoryModel::FetchItemHttpHandler::processFailure(const char* const reason,
															LLCore::HttpResponse* response)
{
	llwarns << "Inventory item fetch failure - Status: internal error - Reason: "
			<< reason << " - Content (abridged): "
			<< LLCoreHttpUtil::responseToString(response) << llendl;

#if 0 // Avoid: "Call was made to notifyObservers within notifyObservers !"
	  // Since there was an error, no update happened anyway... HB
	gInventory.notifyObservers();
#endif
}
