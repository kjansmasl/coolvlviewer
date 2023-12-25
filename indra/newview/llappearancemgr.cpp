/**
 * @file llappearancemgr.cpp
 * @brief Manager for initiating appearance changes on the viewer
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
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

#include "boost/unordered_set.hpp"

#include "llappearancemgr.h"

#include "llcallbacklist.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "llfoldertype.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "llmessage.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llaisapi.h"
#include "llappviewer.h"				// For gFrameTimeSeconds
#include "llcommandhandler.h"
#include "llfloatercustomize.h"
#include "hbfloatermakenewoutfit.h"
#include "llgesturemgr.h"
#include "llgridmanager.h"				// For gIsInProductionGrid
#include "llinventorybridge.h"
#include "llinventorymodelfetch.h"
//MK
#include "mkrlinterface.h"
//mk
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerjointattachment.h"
#include "llvoavatarself.h"
#include "llwearablelist.h"

// Global
LLAppearanceMgr gAppearanceMgr;

char ORDER_NUMBER_SEPARATOR('@');

// Forward declarations of local function
bool confirm_replace_attachment_rez(const LLSD& notification,
									const LLSD& response);

//-----------------------------------------------------------------------------
// Command handlers
//-----------------------------------------------------------------------------

// Support for secondlife:///app/appearance SLapps
class LLAgentAppearanceHandler final : public LLCommandHandler
{
public:
	// Requests are throttled from a non-trusted browser
	LLAgentAppearanceHandler()
	:	LLCommandHandler("appearance", UNTRUSTED_BLOCK)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		// Support secondlife:///app/appearance/show, but for now we just
		// make all secondlife:///app/appearance SLapps behave this way

		if (gAgentWearables.areWearablesLoaded())
		{
			gAgent.changeCameraToCustomizeAvatar();
		}

		return true;
	}
};
LLAgentAppearanceHandler gAgentAppearanceHandler;

// SLapp for easy-wearing of a stock (library) avatar
class LLWearFolderHandler final : public LLCommandHandler
{
public:
	// Not allowed from outside the app
	LLWearFolderHandler()
	:	LLCommandHandler("wear_folder", UNTRUSTED_BLOCK)
	{
	}

	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl*) override
	{
		LLSD::UUID folder_uuid;
		if (query_map.has("folder_id"))
		{
			folder_uuid = query_map["folder_id"].asUUID();
		}
		if (folder_uuid.isNull() && query_map.has("folder_name"))
		{
			std::string folder_name = query_map["folder_name"];
			LLInventoryModel::cat_array_t cat_array;
			LLInventoryModel::item_array_t item_array;
			LLNameCategoryCollector has_name(folder_name);
			gInventory.collectDescendentsIf(gInventory.getLibraryRootFolderID(),
											cat_array, item_array,
											LLInventoryModel::EXCLUDE_TRASH,
											has_name);
			if (cat_array.size())
			{
				LLViewerInventoryCategory* cat = cat_array[0];
				if (cat)
				{
					folder_uuid = cat->getUUID();
				}
			}
		}
		if (folder_uuid.notNull())
		{
			LLPointer<LLInventoryCategory> category =
				new LLInventoryCategory(folder_uuid, LLUUID::null,
										LLFolderType::FT_CLOTHING,
										"Quick Appearance");

			if (gInventory.getCategory(folder_uuid) != NULL)
			{
				gAppearanceMgr.wearInventoryCategory(category, true, false);
			}
		}
#if 0
		// release avatar picker keyboard focus
		gFocusMgr.setKeyboardFocus(NULL);
#endif
		return true;
	}
};
LLWearFolderHandler gWearFolderHandler;

struct LLWearInfo
{
	LLWearInfo(LLUUID cat_id, bool append, bool replace)
	:	mCategoryID(cat_id),
		mAppend(append),
		mReplace(replace)
	{
	}

	LLUUID	mCategoryID;
	bool	mAppend;
	bool	mReplace;
};

struct LLFoundData
{
	LLFoundData(const LLUUID& item_id, const LLUUID& linked_item_id,
				const LLUUID& asset_id, const std::string& name,
				LLAssetType::EType asset_type)
	:	mItemID(item_id),
		mLinkedItemID(linked_item_id),
		mAssetID(asset_id),
		mName(name),
		mAssetType(asset_type),
		mWearable(NULL)
	{
	}

	LLUUID				mItemID;
	LLUUID				mLinkedItemID;
	LLUUID				mAssetID;
	std::string			mName;
	LLAssetType::EType	mAssetType;
	LLViewerWearable*	mWearable;
};

class LLWearableHoldingPattern
{
public:
	LLWearableHoldingPattern(bool append, bool replace)
	:	mResolved(0),
		mAppend(append),
		mReplace(replace)
	{
	}

	~LLWearableHoldingPattern()
	{
		for_each(mFoundList.begin(), mFoundList.end(), DeletePointer());
		mFoundList.clear();
	}

	typedef std::list<LLFoundData*> found_list_t;
	found_list_t	mFoundList;
	S32				mResolved;
	bool			mAppend;
	bool			mReplace;
};

class LLOutfitObserver final : public LLInventoryFetchObserver
{
public:
	LLOutfitObserver(const LLUUID& cat_id, bool copy_items, bool append)
	:	mCatID(cat_id),
		mCopyItems(copy_items),
		mAppend(append)
	{
	}

	void done() override;

private:
	void createCategoryCB(const LLUUID& cat_id);

protected:
	LLUUID	mCatID;
	bool	mCopyItems;
	bool	mAppend;
};

class LLWearInventoryCategoryCallback final : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLWearInventoryCategoryCallback);

public:
	LLWearInventoryCategoryCallback(const LLUUID& cat_id, bool append)
	{
		mCatID = cat_id;
		mAppend = append;
	}

	void fire(const LLUUID& item_id) override
	{
		// Do nothing. We only care about the destructor. The reason for this
		// is that this callback is used in a hack where the same callback is
		// given to dozens of items, and the destructor is called after the
		// last item has fired the event and dereferenced it, if all the events
		// actually fire !
	}

protected:
	~LLWearInventoryCategoryCallback() override
	{
		// Is the destructor called by ordinary dereference, or because the
		// app's shutting down ?  If the inventory callback manager goes away,
		// we are shutting down, no longer want the callback.
		if (LLInventoryCallbackManager::instanceExists())
		{
			gAppearanceMgr.wearInventoryCategoryOnAvatar(gInventory.getCategory(mCatID),
														 mAppend);
		}
		else
		{
			llwarns << "Dropping unhandled LLWearInventoryCategoryCallback"
					<< llendl;
		}
	}

private:
	LLUUID	mCatID;
	bool	mAppend;
};

class LLWearCategoryAfterCopy final : public LLInventoryCallback
{
public:
	LLWearCategoryAfterCopy(bool append)
	:	mAppend(append)
	{
	}

	void fire(const LLUUID& id) override
	{
		// Wear the inventory category.
		LLInventoryCategory* cat = gInventory.getCategory(id);
		gAppearanceMgr.wearInventoryCategoryOnAvatar(cat, mAppend);
	}

private:
	bool mAppend;
};

void LLOutfitObserver::createCategoryCB(const LLUUID& cat_id)
{
	if (cat_id.isNull())
	{
		gNotifications.add("CantCreateRequestedInvFolder");
		return;
	}

	// This is our new category Id.
	mCatID = cat_id;

	// Copy the items into that new category.
	LLPointer<LLInventoryCallback> cb =
		new LLWearInventoryCategoryCallback(mCatID, mAppend);
	for (U32 i = 0, count = mComplete.size(); i < count; ++i)
	{
		LLViewerInventoryItem* itemp =
			(LLViewerInventoryItem*)gInventory.getItem(mComplete[i]);
		if (itemp)
		{
			copy_inventory_item(itemp->getPermissions().getOwner(),
								itemp->getUUID(), mCatID, LLStringUtil::null,
								cb);
		}
	}
	gInventory.notifyObservers();
}

// We now have an outfit ready to be copied to agent inventory. Do it, and wear
// that outfit normally.
void LLOutfitObserver::done()
{
	if (!mCopyItems)
	{
		// Nothing to do but wear the inventory category as it is.
		gAppearanceMgr.wearInventoryCategoryOnAvatar(gInventory.getCategory(mCatID),
													 mAppend);
		return;
	}

	// We must copy the items to the agent inventory first; the wearing will
	// happen once done, via LLWearInventoryCategoryCallback.

	LLInventoryCategory* catp = gInventory.getCategory(mCatID);
	std::string name;
	if (catp)	
	{
		name = catp->getName();
	}
	else	// This should never happen...
	{
		name = "New outfit";
	}

	// We will make a folder in the user-preferred folder, or the Clothing
	// folder by default (FT_MY_OUTFITS becomes FT_CLOTHING on purpose in
	// findChoosenCategoryUUIDForType() when no user preferred folder is set).
	const LLUUID pid =
		gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_MY_OUTFITS);

	// Create the category and, on completion, call back our method to copy the
	// items into it.
	inventory_func_t func = boost::bind(&LLOutfitObserver::createCategoryCB,
										this, _1);
	gInventory.createNewCategory(pid, LLFolderType::FT_NONE, name, func);
}

class LLOutfitFetch final : public LLInventoryFetchDescendentsObserver
{
protected:
	LOG_CLASS(LLOutfitFetch);

public:
	LLOutfitFetch(bool copy_items, bool append)
	:	mCopyItems(copy_items),
		mAppend(append)
	{
	}

	void done() override;

protected:
	bool mCopyItems;
	bool mAppend;
};

void LLOutfitFetch::done()
{
	if (mCompleteFolders.empty())
	{
		llwarns << "Failed to load data. Removing observer." << llendl;
		gInventory.removeObserver(this);
		delete this;
		return;
	}

	// What we do here is get the complete information on the items in the
	// library, and set up an observer that will wait for that to happen.
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	gInventory.collectDescendents(mCompleteFolders.front(), cat_array,
								  item_array, LLInventoryModel::EXCLUDE_TRASH);
	S32 count = item_array.size();
	if (!count)
	{
		llwarns << "Nothing fetched in category " << mCompleteFolders.front()
				<< llendl;
		gInventory.removeObserver(this);
		delete this;
		return;
	}

	LLOutfitObserver* observerp =
		new LLOutfitObserver(mCompleteFolders.front(), mCopyItems, mAppend);
	uuid_vec_t ids;
	for (S32 i = 0; i < count; ++i)
	{
		ids.emplace_back(item_array[i]->getUUID());
	}

	// Clean up, and remove 'this' as an observer now, since the call to the
	// LLOutfitObserver::done() will notify observers and would throw us into
	// an infinite recursion.
	gInventory.removeObserver(this);
	delete this;

	// Do the fetch
	observerp->fetchItems(ids);
	if (observerp->isFinished())
	{
		// Everything is already here: call done.
		observerp->done();
	}
	else
	{
		// It is all on its way: add an observer, and the inventory will call
		// done for us when everything is here.
		gInventory.addObserver(observerp);
	}
}

//-----------------------------------------------------------------------------
// COF link creation callback
//-----------------------------------------------------------------------------

class LLCreateLinkInCOFCallback final : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLCreateLinkInCOFCallback);

public:
	LLCreateLinkInCOFCallback(const LLUUID& linked_item_id, bool is_wearable)
	:	mLinkedItemID(linked_item_id),
		mIsWearable(is_wearable)
	{
		sLinkedItemUUIDs.emplace(linked_item_id);
	}

	void fire(const LLUUID& item_id) override
	{
		uuid_list_t::iterator it = sLinkedItemUUIDs.find(mLinkedItemID);
		if (it != sLinkedItemUUIDs.end())
		{
			sLinkedItemUUIDs.erase(it);
			LL_DEBUGS("COF") << (sLinkedItemUUIDs.empty() ? "Links creation finished in COF"
														  : "One more link created in COF")
							 << LL_ENDL;
		}
		else
		{
			// Stale/late link created in COF: force a resync
			llwarns << "Stale/late link creation in COF, flagging the latter for resync..."
					<< llendl;
			sLinkedItemUUIDs.clear();
			if (mIsWearable)
			{
				gAppearanceMgr.mNeedsSyncWearables = true;
			}
			else
			{
				gAppearanceMgr.mNeedsSyncAttachments = true;
			}
		}

		gAppearanceMgr.resetCOFUpdateTimer();
	}

	static void clearLinksList() 		{ sLinkedItemUUIDs.clear(); }
	static bool isLinksListEmpty() 		{ return sLinkedItemUUIDs.empty(); }

private:
	LLUUID				mLinkedItemID;
	bool				mIsWearable;
	static uuid_list_t	sLinkedItemUUIDs;
};

uuid_list_t LLCreateLinkInCOFCallback::sLinkedItemUUIDs;

//-----------------------------------------------------------------------------
// COF slamming callback
//-----------------------------------------------------------------------------

class LLSlamCOFCallback final : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLSlamCOFCallback);

public:
	LLSlamCOFCallback()
	{
		++sPendingCallbacks;
	}

	~LLSlamCOFCallback() override
	{
		--sPendingCallbacks;
	}

	void fire(const LLUUID&) override
	{
	}

	static bool pendingCallbacks()		{ return sPendingCallbacks > 0; }
	static void resetPendingCallbacks()	{ sPendingCallbacks = 0; }

private:
	static S32 sPendingCallbacks;
};

S32 LLSlamCOFCallback::sPendingCallbacks = 0;

//-----------------------------------------------------------------------------
// Misc callbacks
//-----------------------------------------------------------------------------

//static
U32 LLWearOnAvatarCallback::sCurrentCounterGeneration = 0;
U32 LLWearOnAvatarCallback::sPendingCallbackCount = 0;

void LLWearOnAvatarCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.notNull())
	{
		LLViewerInventoryItem* item = gInventory.getItem(inv_item);
		if (item)
		{
			gAppearanceMgr.wearItemOnAvatar(item->getLinkedUUID(), mReplace);
		}
		// *TODO: track callbacks by (original) item UUID
		if (mCounterGeneration == sCurrentCounterGeneration)
		{
			if (sPendingCallbackCount > 0)
			{
				--sPendingCallbackCount;
				gAppearanceMgr.resetCOFUpdateTimer();
			}
			else
			{
				llwarns << "Spurious callback firing detected !" << llendl;
			}
		}
		else if (item)
		{
			bool is_object = item->getType() == LLAssetType::AT_OBJECT;
			llwarns << "Stale callback triggered for "
					<< (is_object ? "attachment: " : "wearable: ") << inv_item
					<< ". Flagging COF for resync." << llendl;
			if (is_object)
			{
				gAppearanceMgr.mNeedsSyncAttachments = true;
			}
			else
			{
				gAppearanceMgr.mNeedsSyncWearables = true;
			}
			gAppearanceMgr.resetCOFUpdateTimer();
		}
	}
}

//static
U32 LLRezAttachmentCallback::sCurrentCounterGeneration = 0;
U32 LLRezAttachmentCallback::sPendingCallbackCount = 0;

void LLRezAttachmentCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.notNull())
	{
		LLViewerInventoryItem* item = gInventory.getItem(inv_item);
		if (item)
		{
			gAppearanceMgr.rezAttachment(item, mAttach, mReplace);
		}
		// *TODO: track callbacks by (original) item UUID
		if (mCounterGeneration == sCurrentCounterGeneration)
		{
			if (sPendingCallbackCount > 0)
			{
				--sPendingCallbackCount;
				gAppearanceMgr.resetCOFUpdateTimer();
			}
			else
			{
				llwarns << "Spurious callback firing detected !" << llendl;
			}
		}
		else if (item)
		{
			llwarns << "Stale callback triggered for attachment " << inv_item
					<< ". Flagging COF for resync." << llendl;
			gAppearanceMgr.mNeedsSyncAttachments = true;
			gAppearanceMgr.resetCOFUpdateTimer();
		}
	}
}

//-----------------------------------------------------------------------------

//static
bool LLAppearanceMgr::onSetWearableDialog(const LLSD& notification,
										  const LLSD& response,
										  LLViewerWearable* old_wearable)
{
	if (!old_wearable)
	{
		llwarns << "Callback called for a NULL old wearable !" << llendl;
		return false;
	}

	const LLUUID& item_id = notification["payload"]["item_id"].asUUID();
	LLViewerInventoryItem* item_to_wear = gInventory.getItem(item_id);
	if (!item_to_wear)
	{
		llwarns << "Callback called for a NULL new item !" << llendl;
		return false;
	}
	U32 index;
	if (!gAgentWearables.getWearableIndex(old_wearable, index))
	{
		llwarns << "Wearable not found" << llendl;
		return false;
	}

	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:  // "Save"
			gAgentWearables.saveWearable(old_wearable->getType(), index);
		case 1:  // "Don't Save"
			gAppearanceMgr.wearInventoryItemOnAvatar(item_to_wear, true);
		case 2: // "Cancel"
			break;

		default:
			llassert(false);
			break;
	}

	return false;
}

// *NOTE: hack to get from avatar inventory to avatar
void LLAppearanceMgr::wearInventoryItemOnAvatar(LLInventoryItem* item,
												bool replace)
{
	if (item && isAgentAvatarValid())
	{
		LL_DEBUGS("Appearance") << "wearInventoryItemOnAvatar("
								<< item->getName() << ")" << LL_ENDL;

		LLWearableList::getInstance()->getAsset(item->getAssetUUID(),
												item->getName(),
												gAgentAvatarp,
												item->getType(),
												LLWearableBridge::onWearOnAvatarArrived,
												new OnWearStruct(item->getLinkedUUID(),
												replace));
	}
}

// User-requested action
bool LLAppearanceMgr::wearItemOnAvatar(const LLUUID& item_id_to_wear,
									   bool replace)
{
	if (item_id_to_wear.isNull()) return false;
	LLViewerInventoryItem* item_to_wear = gInventory.getItem(item_id_to_wear);
	if (!item_to_wear) return false;

	if (gInventory.isObjectDescendentOf(item_to_wear->getUUID(),
									    gInventory.getLibraryRootFolderID()))
	{
		LLPointer<LLInventoryCallback> cb = new LLWearOnAvatarCallback(replace);
		copy_inventory_item(item_to_wear->getPermissions().getOwner(),
							item_to_wear->getUUID(), LLUUID::null,
							std::string(), cb);
		return false;
	}
	else if (!gInventory.isObjectDescendentOf(item_to_wear->getUUID(),
											  gInventory.getRootFolderID()))
	{
		return false; // not in library and not in agent's inventory
	}
	else if (gInventory.isInTrash(item_to_wear->getUUID()))
	{
		gNotifications.add("CannotWearTrash");
		return false;
	}

	const LLAssetType::EType asset_type = item_to_wear->getType();
//MK
	if (gRLenabled &&
		// Deal with wearables only, here, since attachments are taken care of
		// in rezAttachment()
		(asset_type == LLAssetType::AT_CLOTHING ||
		 asset_type == LLAssetType::AT_BODYPART))
	{
		const LLWearableType::EType type = item_to_wear->getWearableType();
		if (!gRLInterface.canWear(item_to_wear) ||
			 gRLInterface.contains("addoutfit") ||
			 gRLInterface.contains("addoutfit:" +
								   gRLInterface.getOutfitLayerAsString(type)))
		{
			return false;
		}
		if (replace)
		{
			// Check to see if we are already wearing a wearable of this type
			// and if yes, if we can remove it...
			if (gAgentWearables.getViewerWearable(type, 0) &&
				!gRLInterface.canUnwear(type))
			{
				// Cannot remove this wearable type, so cannot replace it
				// either
				return false;
			}
		}
	}
//mk

	switch (asset_type)
	{
		case LLAssetType::AT_CLOTHING:
		{
			if (gAgentWearables.areWearablesLoaded())
			{
				const LLWearableType::EType type = item_to_wear->getWearableType();
				// See if we want to avoid wearing multiple wearables that
				// don't really make any sense or for which the resulting
				// combination is hard for the user to predict and/or notice.
				// E.g. for Physics, only the last worn item is taken into
				// account, so there's no use wearing more than one...
				if ((type == LLWearableType::WT_PHYSICS &&
					 gSavedSettings.getBool("NoMultiplePhysics")) ||
					(type == LLWearableType::WT_SHOES &&
					 gSavedSettings.getBool("NoMultipleShoes")) ||
					(type == LLWearableType::WT_SKIRT &&
					 gSavedSettings.getBool("NoMultipleSkirts")))
				{
					replace = true;
				}
				if (replace && gAgentWearables.getWearableCount(type))
				{
					gAgentWearables.userRemoveWearablesOfType(type);
				}
				if (!gAgentWearables.canAddWearable(type))
				{
					return false;
				}
				if (replace)
				{
					LLViewerWearable* old_wearable;
					// MULTI_WEARABLE: hardwired to 0
					old_wearable = gAgentWearables.getViewerWearable(type, 0);
					if (old_wearable && old_wearable->isDirty())
					{
						// Bring up modal dialog: Save changes ? Yes/No/Cancel
						LLSD payload;
						payload["item_id"] = item_id_to_wear;
						gNotifications.add("WearableSave", LLSD(), payload,
										   boost::bind(onSetWearableDialog,
													   _1, _2, old_wearable));
						return false;
					}
				}

				wearInventoryItemOnAvatar(item_to_wear, replace);
			}
			break;
		}

		case LLAssetType::AT_BODYPART:
		{
			if (gAgentWearables.areWearablesLoaded())
			{
				const LLWearableType::EType type = item_to_wear->getWearableType();
				LLViewerWearable* old_wearable;
				old_wearable = gAgentWearables.getViewerWearable(type, 0);
				if (old_wearable && old_wearable->isDirty())
				{
					// Bring up modal dialog: Save changes ? Yes/No/Cancel
					LLSD payload;
					payload["item_id"] = item_id_to_wear;
					gNotifications.add("WearableSave", LLSD(), payload,
									   boost::bind(onSetWearableDialog, _1, _2,
												   old_wearable));
					return false;
				}

				wearInventoryItemOnAvatar(item_to_wear, true);
			}
			break;
		}

		case LLAssetType::AT_OBJECT:
		{
			rezAttachment(item_to_wear, NULL, replace);
			break;
		}

		default:
		{
			// Nothing to do...
		}
	}

	return false;
}

#if 0	// Not used for now
// *TODO: investigate wearables may not be loaded at this point EXT-8231
bool LLAppearanceMgr::canAddWearables(const uuid_vec_t& item_ids)
{
	if (!isAgentAvatarValid())
	{
		return false;
	}

	U32 n_objects = 0;
	U32 n_clothes = 0;

	// Count given clothes (by wearable type) and objects.
	for (S32 i = 0, count = item_ids.size(); i < count; ++i)
	{
		LLViewerInventoryItem* item = gInventory.getItem(item_ids[i]);
		if (!item)
		{
			return false;
		}

		LLAssetType::EType type = item->getType();
		if (type == LLAssetType::AT_OBJECT)
		{
			++n_objects;
		}
		else if (type == LLAssetType::AT_CLOTHING)
		{
			++n_clothes;
		}
		else
		{
			llwarns << "Unexpected wearable type: " << (S32)type << llendl;
			return false;
		}
	}

	// Check whether we can add all the objects.
	if (!gAgentAvatarp->canAttachMoreObjects(n_objects))
	{
		return false;
	}

	// Check whether we can add all the clothes.
    U32 sum_clothes = n_clothes + gAgentWearables.getClothingLayerCount();
    return sum_clothes <= LLAgentWearables::MAX_CLOTHING_LAYERS;
}
#endif

void LLAppearanceMgr::getDescendentsOfAssetType(const LLUUID& category,
												LLInventoryModel::item_array_t& items,
												LLAssetType::EType type)
{
	LLInventoryModel::cat_array_t cats;
	LLIsType is_of_type(type);
	gInventory.collectDescendentsIf(category, cats, items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_of_type);
}

void LLAppearanceMgr::getDescendentsOfWearableTypes(const LLUUID& category,
													LLInventoryModel::item_array_t& items)
{
	LLInventoryModel::cat_array_t cats;
	LLFindWearables is_wearable;
	gInventory.collectDescendentsIf(category, cats, items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_wearable);
}

void LLAppearanceMgr::getUserDescendents(const LLUUID& category,
										 LLInventoryModel::item_array_t& wear_items,
										 LLInventoryModel::item_array_t& obj_items,
										 LLInventoryModel::item_array_t& gest_items)
{
	LLInventoryModel::cat_array_t wear_cats;
	LLFindWearables is_wearable;
	gInventory.collectDescendentsIf(category, wear_cats, wear_items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_wearable);

	LLInventoryModel::cat_array_t obj_cats;
	LLIsType is_object(LLAssetType::AT_OBJECT);
	gInventory.collectDescendentsIf(category, obj_cats, obj_items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_object);

	// Find all gestures in this folder
	LLInventoryModel::cat_array_t gest_cats;
	LLIsType is_gesture(LLAssetType::AT_GESTURE);
	gInventory.collectDescendentsIf(category, gest_cats, gest_items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_gesture);
}

void LLAppearanceMgr::wearInventoryCategory(LLInventoryCategory* category,
											bool copy, bool append)
{
	if (!category) return;

	LL_DEBUGS("Appearance") << "wearInventoryCategory(" << category->getName()
							<< ")" << LL_ENDL;

	// If we are copying from library, use AIS to copy the category.
	if (copy && AISAPI::isAvailable())
	{
		// We will make a folder in the user-preferred folder, or the Clothing
		// folder by default (FT_MY_OUTFITS becomes FT_CLOTHING on purpose in
		// findChoosenCategoryUUIDForType() when no user preferred folder is
		// set).
		LLUUID parent_id =
			gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_MY_OUTFITS);
		if (parent_id.isNull())
		{
			parent_id = gInventory.getRootFolderID();
		}

		LLPointer<LLInventoryCallback> copy_cb =
			new LLWearCategoryAfterCopy(append);
        AISAPI::completion_t cr = boost::bind(&doInventoryCb, copy_cb, _1);
        AISAPI::copyLibraryCategory(category->getUUID(), parent_id, false, cr);
	}
	else
	{
		// What we do here is get the complete information on the items in
		// the inventory, and set up an observer that will wait for that to
		// happen.
		LLOutfitFetch* outfit = new LLOutfitFetch(copy, append);
		uuid_vec_t folders;
		folders.emplace_back(category->getUUID());
		outfit->fetchDescendents(folders);
		if (outfit->isFinished())
		{
			// Everything is already here; call done.
			outfit->done();
		}
		else
		{
			// It is all on its way: add an observer, and the inventory will
			// call done for us when everything is here.
			gInventory.addObserver(outfit);
		}
	}
}

void LLAppearanceMgr::wearInventoryCategoryOnAvatar(LLInventoryCategory* category,
													bool append, bool replace)
{
	// Avoid unintentionally overwriting old wearables. We have to do this up
	// front to avoid having to deal with the case of multiple wearables being
	// dirty.
	if (!category) return;
	LL_DEBUGS("Appearance") << "wear_inventory_category_on_avatar("
							<< category->getName() << ")" << LL_ENDL;

	LLWearInfo* info = new LLWearInfo(category->getUUID(), append, replace);

	if (gFloaterCustomizep)
	{
		gFloaterCustomizep->askToSaveIfDirty(wearInventoryCategoryOnAvatarStep2,
											 info);
	}
	else
	{
		wearInventoryCategoryOnAvatarStep2(true, info);
	}
}

//static
void LLAppearanceMgr::wearInventoryCategoryOnAvatarStep2(bool proceed,
														 void* userdata)
{
	if (!gAgent.getRegion() || !isAgentAvatarValid()) return;

	LLWearInfo* wear_info = (LLWearInfo*)userdata;
	if (!wear_info) return;

	// Find all the wearables that are in the category's subtree.
	LL_DEBUGS("Appearance") << "wearInventoryCategoryOnAvatarStep2()"
							<< LL_ENDL;
	if (proceed)
	{
//MK
		bool old_restore = gRLInterface.mRestoringOutfit;
		gRLInterface.mRestoringOutfit =
			gAppearanceMgr.isRestoringInitialOutfit();
//mk

		LLUUID cat_id = wear_info->mCategoryID;
		LLViewerInventoryItem* item;
		S32 i;

#if 0
		// Checking and updating links' descriptions of wearables
		gAppearanceMgr.updateClothingOrderingInfo(cat_id);
#endif

		// Find all the wearables that are in the category's subtree.
		LLInventoryModel::item_array_t wear_items;
		LLInventoryModel::item_array_t obj_items;
		LLInventoryModel::item_array_t gest_items;
		gAppearanceMgr.getUserDescendents(cat_id, wear_items, obj_items,
										  gest_items);

		S32 wearable_count = wear_items.size();
		S32 obj_count = obj_items.size();
		S32 gest_count = gest_items.size();

		if (!wearable_count && !obj_count && !gest_count)
		{
			gNotifications.add("CouldNotPutOnOutfit");
			delete wear_info;
			return;
		}

		// Activate all gestures in this folder
		if (gest_count > 0)
		{
			llinfos << "Activating " << gest_count << " gestures" << llendl;

			gGestureManager.activateGestures(gest_items);

			// Update the inventory item labels to reflect the fact
			// they are active.
			LLViewerInventoryCategory* catp =
				gInventory.getCategory(wear_info->mCategoryID);
			if (catp)
			{
				gInventory.updateCategory(catp);
				gInventory.notifyObservers();
			}
		}

		if (wearable_count > 0)
		{
			// Preparing the list of wearables in the correct order for
			// LLAgentWearables
			sortItemsByActualDescription(wear_items);

			// Note: cannot do normal iteration, because if all the wearables
			// can be resolved immediately, then the callback will be called
			// (and this object deleted) before the final getNextData().
			LLWearableHoldingPattern* holder =
				new LLWearableHoldingPattern(wear_info->mAppend,
											 wear_info->mReplace);
			LLFoundData* found;
			std::vector<LLFoundData*> found_container;
			for (i = 0; i  < wearable_count; ++i)
			{
				item = wear_items[i];
				found = new LLFoundData(item->getUUID(),
										item->getLinkedUUID(),
										item->getAssetUUID(),
										item->getName(),
										item->getType());
				// Pushing back, not front, to preserve order of wearables for
				// LLAgentWearables
				holder->mFoundList.push_back(found);
				found_container.push_back(found);
			}
			LLWearableList* wl = LLWearableList::getInstance();
			for (i = 0; i < wearable_count; ++i)
			{
				found = found_container[i];
				wl->getAsset(found->mAssetID, found->mName, gAgentAvatarp,
							 found->mAssetType, onWearableAssetFetch,
							 (void*)holder);
			}
		}

		// If not appending and the folder does not contain only gestures, take
		// off attachments that we do not need to keep.
		if (!wear_info->mAppend &&
			!(wearable_count == 0 && obj_count == 0 && gest_count > 0))
		{
			if (obj_count > 0)
			{
				// Build a list of the attachments we want to wear
				uuid_list_t keep_these;
				for (i = 0; i < obj_count; ++i)
				{
					keep_these.emplace(obj_items[i]->getLinkedUUID());
				}

				// Remove all worn attachments not in our keep_these list, and
				// remove from the latter the UUIDs of already worn attachments
				detachExtraAttachments(keep_these, true);

				// Check that all attachments we want to wear are in keep_these
				// and remove them if they are not (since already worn).
				LLInventoryModel::item_array_t::iterator it = obj_items.begin();
				while (it != obj_items.end())
				{
					if (!keep_these.count((*it)->getLinkedUUID()))
					{
						it = obj_items.erase(it);
						--obj_count;
					}
					else
					{
						++it;
					}
				}
			}
			else
			{
				// Take off all worn attachments
				LLAgentWearables::userRemoveAllAttachments();
			}
		}

		if (obj_count > 0 && isAgentAvatarValid())
		{
			// We have found some attachements. Add these and build a compound
			// message to send all the objects that need to be rezzed.

			// Limit number of packets to send
			constexpr S32 MAX_PACKETS_TO_SEND = 10;
			constexpr S32 OBJECTS_PER_PACKET = 4;
			constexpr S32 MAX_OBJECTS_TO_SEND = MAX_PACKETS_TO_SEND *
												OBJECTS_PER_PACKET;
			if (obj_count > MAX_OBJECTS_TO_SEND)
			{
				obj_count = MAX_OBJECTS_TO_SEND;
			}

			// Create an Id to keep the parts of the compound message together
			LLUUID compound_msg_id;
			compound_msg_id.generate();

			LLMessageSystem* msg = gMessageSystemp;
			U8 add_flag = wear_info->mReplace ? 0 : ATTACHMENT_ADD;
			for (i = 0; i < obj_count; ++i)
			{
				if (i % OBJECTS_PER_PACKET == 0)
				{
					// Start a new message chunk
					msg->newMessageFast(_PREHASH_RezMultipleAttachmentsFromInv);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
					msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
					msg->nextBlockFast(_PREHASH_HeaderData);
					msg->addUUIDFast(_PREHASH_CompoundMsgID, compound_msg_id);
					msg->addU8Fast(_PREHASH_TotalObjects, obj_count);
					msg->addBoolFast(_PREHASH_FirstDetachAll, false);
				}

				item = obj_items[i];
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addUUIDFast(_PREHASH_ItemID, item->getLinkedUUID());
				msg->addUUIDFast(_PREHASH_OwnerID,
								 item->getPermissions().getOwner());
				// Wear at the previous or default attachment point
				msg->addU8Fast(_PREHASH_AttachmentPt, add_flag);
				pack_permissions_slam(msg, item->getFlags(),
									  item->getPermissions());
				msg->addStringFast(_PREHASH_Name, item->getName());
				msg->addStringFast(_PREHASH_Description,
								   item->getDescription());

				if (obj_count == i + 1 ||
					OBJECTS_PER_PACKET - 1 == i % OBJECTS_PER_PACKET)
				{
					// End of message chunk
					msg->sendReliable(gAgent.getRegionHost());
				}
			}
		}

//MK
		gRLInterface.mRestoringOutfit = old_restore;
//mk
	}

	delete wear_info;
	wear_info = NULL;
}

//static
void LLAppearanceMgr::onWearableAssetFetch(LLViewerWearable* wearable,
										   void* userdata)
{
	LLWearableHoldingPattern* holder = (LLWearableHoldingPattern*)userdata;

	if (wearable)
	{
		for (LLWearableHoldingPattern::found_list_t::iterator
				iter = holder->mFoundList.begin(),
				end = holder->mFoundList.end();
			 iter != end; ++iter)
		{
			LLFoundData* data = *iter;
			if (wearable->getAssetID() == data->mAssetID)
			{
				data->mWearable = wearable;
				break;
			}
		}
	}
	if (++holder->mResolved >= (S32)holder->mFoundList.size())
	{
		wearInventoryCategoryOnAvatarStep3(holder);
	}
}

//static
void LLAppearanceMgr::wearInventoryCategoryOnAvatarStep3(LLWearableHoldingPattern* holder)
{
	LL_DEBUGS("Appearance") << "wearInventoryCategoryOnAvatarStep3()"
							<< LL_ENDL;

//MK
	bool old_restore = gRLInterface.mRestoringOutfit;
	gRLInterface.mRestoringOutfit = gAppearanceMgr.isRestoringInitialOutfit();
//mk

	LLInventoryItem::item_array_t items;
	std::vector<LLViewerWearable*> wearables;

	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		bool remove_old = false;
		for (LLWearableHoldingPattern::found_list_t::iterator
				iter = holder->mFoundList.begin(),
				end = holder->mFoundList.end();
			 iter != end; ++iter)
		{
			LLFoundData* data = *iter;
			LLViewerWearable* wearable = data->mWearable;
			if (wearable && (S32)wearable->getType() == i)
			{
				LLViewerInventoryItem* item =
					(LLViewerInventoryItem*)gInventory.getItem(data->mLinkedItemID);
				if (item && item->getAssetUUID() == wearable->getAssetID())
				{
					items.push_back(item);
					wearables.push_back(wearable);
					if (holder->mReplace &&
						wearable->getAssetType() == LLAssetType::AT_CLOTHING)
					{
						remove_old = true;
					}
				}
			}
		}
		if (remove_old)
		{
			gAgentWearables.removeWearable((LLWearableType::EType)i, true, 0);
		}
	}

	if (wearables.size() > 0)
	{
		gAgentWearables.setWearableOutfit(items, wearables, !holder->mAppend);
		//gInventory.notifyObservers();
	}

//MK
	gRLInterface.mRestoringOutfit = old_restore;
//mk

	delete holder;
}

void LLAppearanceMgr::wearOutfitByName(const std::string& name)
{
	llinfos << "Wearing category " << name << llendl;

	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	LLNameCategoryCollector has_name(name);
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(),
									cat_array,
									item_array,
									LLInventoryModel::EXCLUDE_TRASH,
									has_name);
	bool copy_items = false;
	LLInventoryCategory* cat = NULL;
	if (cat_array.size() > 0)
	{
		// Just wear the first one that matches
		cat = cat_array[0];
	}
	else
	{
		gInventory.collectDescendentsIf(LLUUID::null,
										cat_array,
										item_array,
										LLInventoryModel::EXCLUDE_TRASH,
										has_name);
		if (cat_array.size() > 0)
		{
			cat = cat_array[0];
			copy_items = true;
		}
	}

	if (cat)
	{
		wearInventoryCategory(cat, copy_items, false);
	}
	else
	{
		llwarns << "Couldn't find outfit " << name
				<< " in wearOutfitByName()" << llendl;
	}
}

std::string build_order_string(LLWearableType::EType type, U32 i)
{
	std::ostringstream order_num;
	order_num << ORDER_NUMBER_SEPARATOR << type * 100 + i;
	return order_num.str();
}

// NOTE: despite the name, this is not the same function as in v2/3 viewers:
// this function is used to update the description of the inventory links
// corresponding to a worn clothing item in a folder (category), according
// to their current layer index.
void LLAppearanceMgr::updateClothingOrderingInfo(LLUUID cat_id)
{
	if (cat_id.isNull())
	{
		return;
	}

	LLInventoryModel::item_array_t wear_items;
	getDescendentsOfAssetType(cat_id, wear_items, LLAssetType::AT_CLOTHING);

	if (!wear_items.size())
	{
		return;
	}

	bool inventory_changed = false;

	LLViewerWearable* wearable;
	for (S32 i = 0, count = wear_items.size(); i < count; ++i)
	{
		LLViewerInventoryItem* item = wear_items[i];
		if (!item)
		{
			llwarns << "NULL item found" << llendl;
			continue;
		}
		// Ignore non-links and non-worn link wearables.
		if (!item->getIsLinkType() ||
			!gAgentWearables.isWearingItem(item->getUUID()))
		{
			continue;
		}
		LLWearableType::EType type = item->getWearableType();
		if (type < 0 || type >= LLWearableType::WT_COUNT)
		{
			llwarns << "Invalid wearable type. Inventory type does not match wearable flag bitfield."
					<< llendl;
			continue;
		}
		wearable = gAgentWearables.getWearableFromItemID(item->getUUID());
		U32 index;
		if (!gAgentWearables.getWearableIndex(wearable, index))
		{
			llwarns << "Cannot find wearable index for item: "
					<< item->getName() << llendl;
			continue;
		}

		std::string new_order_str = build_order_string(type, index);
		std::string old_desc = item->getActualDescription();
		if (new_order_str == old_desc) continue;

		LL_DEBUGS("Appearance") << "Changing the description for link item '"
								<< item->getName() << "' from '" << old_desc
								<< "' to '" << new_order_str << "'" << LL_ENDL;

		item->setDescription(new_order_str);
		item->setComplete(true);
		LLSD updates;
		updates["desc"] = new_order_str;
		update_inventory_item(item->getUUID(), updates);

		inventory_changed = true;
	}

	// *TODO: do we really need to notify observers ?
	if (inventory_changed)
	{
		gInventory.notifyObservers();
	}
}

// A predicate for sorting inventory items by actual descriptions
bool sort_by_description(const LLInventoryItem* item1,
						 const LLInventoryItem* item2)
{
	if (!item1 || !item2)
	{
		llwarns << "sort_by_description(): Either item1 or item2 is NULL"
				<< llendl;
		return true;
	}

	return item1->getActualDescription() < item2->getActualDescription();
}

//static
void LLAppearanceMgr::sortItemsByActualDescription(LLInventoryModel::item_array_t& items)
{
	if (items.size() < 2) return;

	std::sort(items.begin(), items.end(), sort_by_description);
}

void LLAppearanceMgr::removeInventoryCategoryFromAvatar(LLInventoryCategory* category)
{
	if (!category) return;

	LL_DEBUGS("Appearance") << "removeInventoryCategoryFromAvatar("
							<< category->getName() << ")" << LL_ENDL;

	LLUUID* uuid = new LLUUID(category->getUUID());

	if (gFloaterCustomizep)
	{
		gFloaterCustomizep->askToSaveIfDirty(removeInventoryCategoryFromAvatarStep2,
											 uuid);
	}
	else
	{
		removeInventoryCategoryFromAvatarStep2(true, uuid);
	}
}

//static
void LLAppearanceMgr::removeInventoryCategoryFromAvatarStep2(bool proceed,
															 void* userdata)
{
	LLUUID* category_id = (LLUUID*)userdata;
	LLViewerInventoryItem* item;
	S32 i;

	LL_DEBUGS("Appearance") << "removeInventoryCategoryFromAvatarStep2()"
							<< LL_ENDL;
	if (proceed && isAgentAvatarValid())
	{
		// Find all the wearables that are in the category's subtree.

		LLInventoryModel::item_array_t wear_items;
		LLInventoryModel::item_array_t obj_items;
		LLInventoryModel::item_array_t gest_items;
		gAppearanceMgr.getUserDescendents(*category_id, wear_items, obj_items,
										  gest_items);

		S32 wearable_count = wear_items.size();
		S32 obj_count = obj_items.size();
		S32 gest_count = gest_items.size();

		if (wearable_count > 0)
		{
			// Loop through wearables. If worn, remove.
			LLWearableList* wl = LLWearableList::getInstance();
			for (i = 0; i < wearable_count; ++i)
			{
				item = wear_items[i];
				if (gAgentWearables.isWearingItem(item->getUUID()))
				{
					wl->getAsset(item->getAssetUUID(), item->getName(),
								 gAgentAvatarp, item->getType(),
								 LLWearableBridge::onRemoveFromAvatarArrived,
								 new OnRemoveStruct(item->getLinkedUUID()));
				}
			}
		}

		if (obj_count > 0)
		{
			for (i = 0; i  < obj_count; ++i)
			{
				item = obj_items[i];
//MK
				if (!gRLenabled || gRLInterface.canDetach(item))
				{
//mk
					LLVOAvatarSelf::detachAttachmentIntoInventory(item->getLinkedUUID());
//MK
				}
//mk
			}
		}

		if (gest_count > 0)
		{
			for (i = 0; i  < gest_count; ++i)
			{
				item = gest_items[i];
				if (gGestureManager.isGestureActive(item->getUUID()))
				{
					gGestureManager.deactivateGesture(item->getUUID());
					gInventory.updateItem(item);
					gInventory.notifyObservers();
				}
			}
		}
	}

	delete category_id;
	category_id = NULL;
}

void LLAppearanceMgr::rezAttachment(LLViewerInventoryItem* item,
									LLViewerJointAttachment* attachment,
									bool replace)
{
	if (!isAgentAvatarValid() || !item) return;

	LLSD payload;
	// Wear the base object in case this is a link.
	payload["item_id"] = item->getLinkedUUID();

	S32 attach_pt = 0;
	if (attachment)
	{
		for (LLVOAvatar::attachment_map_t::iterator
				iter = gAgentAvatarp->mAttachmentPoints.begin(),
				end = gAgentAvatarp->mAttachmentPoints.end();
			 iter != end; ++iter)
		{
			if (iter->second == attachment)
			{
				attach_pt = iter->first;
				break;
			}
		}
	}

	if (!replace)
	{
		attach_pt |= ATTACHMENT_ADD;
	}
	payload["attachment_point"] = attach_pt;
	if (attachment)
	{
		payload["attachment_name"] = attachment->getName();
	}

//MK
	bool old_restore = gRLInterface.mRestoringOutfit;
	gRLInterface.mRestoringOutfit = gAppearanceMgr.isRestoringInitialOutfit();
//mk

	if (replace && attachment && attachment->getNumObjects() > 0)
	{
//MK
		if (!gRLenabled ||
			(gRLInterface.canAttach(item) &&
			 gRLInterface.canDetach(attachment->getName())))
//mk
		{
			gNotifications.add("ReplaceAttachment", LLSD(), payload,
							   confirm_replace_attachment_rez);
		}
	}
	else
	{
//MK
		if (!gRLenabled || gRLInterface.canAttach(item))
//mk
		{
			gNotifications.forceResponse(LLNotification::Params("ReplaceAttachment").payload(payload),
										 0/*YES*/);
		}
	}
//MK
	gRLInterface.mRestoringOutfit = old_restore;
//mk
}

bool confirm_replace_attachment_rez(const LLSD& notification,
									const LLSD& response)
{
	if (!gAgent.getRegion()) return false;

	if (!gAgentAvatarp->canAttachMoreObjects())
	{
		// Avoid piling such notifications... Especially since they are modal
		// ones !
		static LLUUID max_attx_notif_id;
		if (max_attx_notif_id.notNull())
		{
			if (gNotifications.find(max_attx_notif_id))
			{
				return false;
			}
			max_attx_notif_id.setNull();
		}

		LLSD args;
		args["MAX_ATTACHMENTS"] = llformat("%d", gMaxSelfAttachments);
		LLNotificationPtr n = gNotifications.add("MaxAttachmentsOnOutfit",
												 args);
		if (n)
		{
			max_attx_notif_id = n->getID();
		}
		return false;
	}

	if (LLNotification::getSelectedOption(notification, response) == 0) // YES
	{
		S32 attach_pt = notification["payload"]["attachment_point"].asInteger();

		LLViewerInventoryItem* itemp =
			gInventory.getItem(notification["payload"]["item_id"].asUUID());
		if (itemp && itemp->getLinkedItem())
		{
			itemp = itemp->getLinkedItem();
		}
		if (itemp)
		{
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_RezSingleAttachmentFromInv);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addUUIDFast(_PREHASH_ItemID, itemp->getUUID());
			msg->addUUIDFast(_PREHASH_OwnerID,
							 itemp->getPermissions().getOwner());
			msg->addU8Fast(_PREHASH_AttachmentPt, attach_pt);
			pack_permissions_slam(msg, itemp->getFlags(),
								  itemp->getPermissions());
			msg->addStringFast(_PREHASH_Name, itemp->getName());
			msg->addStringFast(_PREHASH_Description, itemp->getDescription());
			msg->sendReliable(gAgent.getRegionHost());
		}
	}

	return false;
}
static LLNotificationFunctorRegistration confirm_replace_attachment_rez_reg("ReplaceAttachment",
																			confirm_replace_attachment_rez);

std::string get_outfit_filename()
{
	std::string filename = gIsInProductionGrid ? "outfit.xml"
											   : "outfit_beta.xml";
	return gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT, filename);
}

void LLAppearanceMgr::checkOutfit()
{
	if (!gAgent.regionCapabilitiesReceived())
	{
		// Wait until the capababilities have been received before dealing with
		// COF resyncs and rebakes...
		return;
	}

	static LLCachedControl<bool> restore_outfit_from_cof(gSavedSettings,
														 "RestoreOutfitFromCOF");
	bool restore_from_cof = restore_outfit_from_cof;
	bool must_sync_cof = true;
	if (!gIsInSecondLife)
	{
		// In OpenSim grids, do not sync with the COF unless we chose to do so
		// (OSUseCOF setting is true), or the grid supports SSB (unlikely, but
		// let's be future-proof...).
		static LLCachedControl<bool> os_use_cof(gSavedSettings, "OSUseCOF");
		if (!os_use_cof)
		{
			restore_from_cof = false;
		}
		bool can_do_ssb = LLVOAvatarSelf::canUseServerBaking();
		if (!can_do_ssb)
		{
			setRebaking(false);
		}
		must_sync_cof = os_use_cof || can_do_ssb;
	}

	static LLCachedControl<U32> outfit_restore_min_delay(gSavedSettings,
														 "OutfitRestorationMinDelay");
	static LLCachedControl<U32> outfit_restore_max_delay(gSavedSettings,
														 "OutfitRestorationMaxDelay");
	if (gAgentWearables.areWearablesLoaded() &&
		gAgentAvatarp->mPendingAttachment.size() == 0)
	{
		if (mIsRestoringInitialOutfit)
		{
			F32 min_delay = (F32)outfit_restore_min_delay;
			if (min_delay < 5.f)
			{
				min_delay = 5.f;
			}
			F32 max_delay = (F32)outfit_restore_max_delay;
			if (max_delay < min_delay + 5.f)
			{
				max_delay = min_delay + 5.f;
			}
			bool loading =
				!LLInventoryModelFetch::getInstance()->isEverythingFetched();
			if (loading)
			{
				// Let's at least wait till the inventory is fully loaded...
				max_delay += gAttachmentsTimer.getElapsedTimeF32();
			}
			if (gAttachmentsTimer.getElapsedTimeF32() < min_delay)
			{
				// Must be reset each time the timer is reset
				mRestorationRetryDelayDelta = 0.f;
			}
			if (gAttachmentsTimer.getElapsedTimeF32() >
					min_delay + mRestorationRetryDelayDelta)
			{
//MK
				gRLInterface.mRestoringOutfit = true;
//mk
				bool can_retry =
					gAttachmentsTimer.getElapsedTimeF32() < max_delay;
				ERestoreOutfitStatus status;
				if (restore_from_cof)
				{
					status = restoreOutfitFromCOF(can_retry);
				}
				else
				{
					status = restoreOutfit(can_retry);
				}
//MK
				gRLInterface.mRestoringOutfit = false;
//mk
				mIsRestoringInitialOutfit = status == RETRY;
				if (mIsRestoringInitialOutfit)
				{
					mRestorationRetryDelayDelta = 
						gAttachmentsTimer.getElapsedTimeF32() - min_delay + 5.f;
					LL_DEBUGS("InitialOutfit") << "Will retry outfit restoration in 5 seconds..."
											   << LL_ENDL;
					if (loading && !mOutfitRestorationRetried)
					{
						mOutfitRestorationRetried = true;
						LLNotificationPtr n =
							gNotifications.add("OutfitRestorationDelayed");
						if (n)
						{
							mLoadingNotificationID = n->getID();
						}
					}
				}
				else if (status == DONE || status == INCOMPLETE)
				{
					if (!restore_from_cof)
					{
						// Remove any worn item that is not part of the saved
						// outfit (may happen if coming from a v2/3 viewer for
						// example).
//MK
						gRLInterface.mRestoringOutfit = true;
//mk
						removeNonMatchingItems();
//MK
						gRLInterface.mRestoringOutfit = false;
//mk
					}

					// Force a saving of the outfit on next run
					gAgentWearables.setWearablesLoaded();
					gAttachmentsListDirty = gWearablesListDirty = true;

					// Dirty attachments spatial groups to avoid missing prims.
					gAgentAvatarp->refreshAttachments();

					// This will force a server-side rebake on next run
					gAgentAvatarp->mLastUpdateRequestCOFVersion =
						LLViewerInventoryCategory::VERSION_UNKNOWN;

					// Make sure we take our Z offset into account
					gAgentAvatarp->scheduleHoverUpdate();

					// Notify the Make New Outfit floater, if opened
					HBFloaterMakeNewOutfit::setDirty();

					// Cancel the "OutfitRestorationDelayed" notification, if
					// any exists and is still active.
					if (mLoadingNotificationID.notNull())
					{
						LLNotificationPtr n =
							gNotifications.find(mLoadingNotificationID);
						if (n)
						{
							gNotifications.cancel(n);
						}
						mLoadingNotificationID.setNull();
					}

					llinfos << "Outfit restoration completed." << llendl;
					std::string msg =
						status == DONE ? "OutfitRestorationCompleted"
									   : "OutfitRestorationPartial";
					gNotifications.add(msg);
				}
				else
				{
					// Dirty attachments spatial groups to avoid missing prims.
					gAgentAvatarp->refreshAttachments();

					// Cancel the "OutfitRestorationDelayed" notification, if
					// any and still active.
					if (mLoadingNotificationID.notNull())
					{
						LLNotificationPtr n =
							gNotifications.find(mLoadingNotificationID);
						if (n)
						{
							gNotifications.cancel(n);
						}
						mLoadingNotificationID.setNull();
					}

					llwarns << "Outfit restoration failed !" << llendl;
					gNotifications.add("OutfitRestorationFailed");
				}
			}
		}
		else if (gAttachmentsListDirty || gWearablesListDirty)
		{
			saveOutfit();
		}
	}
	else
	{
		gAttachmentsListDirty = gWearablesListDirty = true;
		gAttachmentsTimer.reset();
	}

	if (!must_sync_cof)
	{
		// OpenSim: No link support, or user chose not to use COF.
		LLFolderType::setCanDeleteCOF(true);
		return;
	}
	else
	{
		LLFolderType::setCanDeleteCOF(false);
	}

	static LLCachedControl<F32> cof_delay(gSavedSettings,
										  "SyncCOFUpdateDelay");
	if (mIsRestoringInitialOutfit ||
		mUpdateCOFTimer.getElapsedTimeF32() <=
			llclamp((F32)cof_delay, 1.f, 3.f))
	{
		return;
	}

	if (mNeedsSyncAttachments || mNeedsSyncWearables)
	{
		bool cof_complete = gInventory.isCategoryComplete(getCOF(true)) &&
							LLCreateLinkInCOFCallback::isLinksListEmpty() &&
							!LLSlamCOFCallback::pendingCallbacks() &&
							!LLWearOnAvatarCallback::pendingCallbacks() &&
							!LLRezAttachmentCallback::pendingCallbacks() &&
							!LLWearableSaveData::pendingSavedWearables();

		static LLCachedControl<U32> sync_cof_timeout(gSavedSettings,
													 "SyncCOFTimeout");
		if (!cof_complete &&
			mUpdateCOFTimer.getElapsedTimeF32() > llmax((F32)sync_cof_timeout,
														5.f * (F32)cof_delay))
		{
			llwarns << "Timeout waiting for COF update, forcing an update."
					<< llendl;
			LLCreateLinkInCOFCallback::clearLinksList();
			LLSlamCOFCallback::resetPendingCallbacks();
			LLWearOnAvatarCallback::resetPendingCallbacks();
			LLRezAttachmentCallback::resetPendingCallbacks();
			LLWearableSaveData::resetSavedWearableCount();
			cof_complete = true;
		}

		if (cof_complete)
		{
			if (AISAPI::isAvailable() && !mBakeRequestSent &&
				!mForceServerSideRebake)
			{
				LL_DEBUGS("COF") << "COF is complete, resyncing using AIS slam..."
								 << LL_ENDL;
				slamCOF();
			}
			else
			{
				LL_DEBUGS("COF") << "COF is complete." << LL_ENDL;
				if (mNeedsSyncAttachments)
				{
					LL_DEBUGS("COF") << "Resyncing attachments..." << LL_ENDL;
					syncAttachmentLinksInCOF();
				}
				if (mNeedsSyncWearables)
				{
					LL_DEBUGS("COF") << "Resyncing wearables..." << LL_ENDL;
					syncWearableLinksInCOF();
				}
			}
		}
		else
		{
			LL_DEBUGS("COF") << "COF is not yet complete, delaying resync..."
							 << LL_ENDL;
		}
	}
	else if (LLVOAvatarSelf::canUseServerBaking() && !mBakeRequestSent &&
			 gAgentAvatarp->mLastUpdateRequestCOFVersion != getCOFVersion())
	{
		LL_DEBUGS("COF") << "COF now updated, requesting a server-side rebake..."
						 << LL_ENDL;
		requestServerAppearanceUpdate();
	}
}

//static
void LLAppearanceMgr::detachExtraAttachments(uuid_list_t& keep_these,
											 bool erase_worn)
{
	if (!isAgentAvatarValid()) return;

	LLAgentWearables::llvo_vec_t objects_to_detach;
	for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerObject* object =
			gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (object)
		{
			LLUUID id = object->getAttachmentItemID();

			// Check that it is not a temporary attachment: it is not one if it
			// is in our inventory (item != NULL)...
			LLViewerInventoryItem* item = gInventory.getItem(id);
			if (!item) continue;

			if (keep_these.count(id) == 0)
			{
				objects_to_detach.push_back(object);
				LL_DEBUGS("InitialOutfit") << "Attachment: " << id
										   << " not in outfit, marking for detach."
										   << LL_ENDL;
			}
			else if (erase_worn)
			{
				keep_these.erase(id);
			}
		}
	}

	// Take off the attachments that will no longer be in the outfit.
	if (!objects_to_detach.empty())
	{
		LL_DEBUGS("InitialOutfit") << "Removing extra attachments" << LL_ENDL;
		gAgentWearables.userRemoveMultipleAttachments(objects_to_detach);
	}
}

void LLAppearanceMgr::removeNonMatchingItems()
{
	// Open the outfit.xml file for reading
	std::string filename = get_outfit_filename();
	LLSD list;
	llifstream llsd_xml(filename.c_str(), std::ios::in | std::ios::binary);
	if (!llsd_xml.is_open())
	{
		LL_DEBUGS("InitialOutfit") << "No outfit.xml file found, or file not readable"
								   << LL_ENDL;
		return;
	}

	LL_DEBUGS("InitialOutfit") << "Checking currently worn items against saved outfit list..."
							   << LL_ENDL;
	// Create the list of expected items for our final outfit
	uuid_list_t outfit;
	LLSDSerialize::fromXML(list, llsd_xml);
	for (LLSD::map_iterator iter = list.beginMap(), end = list.endMap();
		 iter != end; ++iter)
	{
		LLSD::String key_name = iter->first;
		LLSD array = iter->second;
		if (key_name == "attachments" && array.isArray())
		{
			for (S32 i = 0, count = array.size(); i < count; ++i)
			{
				LLSD map = array[i];
				if (map.has("inv_item_id"))
				{
					LLUUID item_id = map.get("inv_item_id");
					outfit.emplace(item_id);
					LL_DEBUGS("InitialOutfit") << "Expected attachment: "
											   << item_id << LL_ENDL;
				}
			}
		}
		else if (key_name == "wearables" && array.isArray())
		{
			for (S32 i = 0, count = array.size(); i < count; ++i)
			{
				LLSD map = array[i];
				if (map.has("inv_item_id"))
				{
					LLUUID item_id = map.get("inv_item_id");
					outfit.emplace(item_id);
					LL_DEBUGS("InitialOutfit") << "Expected wearable: "
											   << item_id << LL_ENDL;
				}
			}
		}
	}

	// Done with outfit.xml
	llsd_xml.close();

	if (outfit.empty())
	{
		LL_DEBUGS("InitialOutfit") << "Empty or invalid outfit.xml file"
								   << LL_ENDL;
		return;
	}

	// Check the currently worn attachments against our list and remove the
	// worn objects in excess.
	detachExtraAttachments(outfit);

	// Check the currently worn clothes (*not* the bodyparts !... they will
	// get replaced automatically by the outfit ones) against our list and
	// remove the worn items in excess.
	uuid_list_t worn_clothes;
	for (U32 i = (U32)LLWearableType::WT_SHIRT;
		 i < (U32)LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		for (U32 index = 0, count = gAgentWearables.getWearableCount(type);
			 index < count; )
		{
			LLInventoryItem* item =
				gInventory.getItem(gAgentWearables.getWearableItemID(type,
																	 index));
			if (item)
			{
				LLUUID id = item->getUUID();
				if (outfit.count(id) == 0)
				{
					LL_DEBUGS("InitialOutfit") << "Wearable: " << id
											   << " not in outfit, removing."
											   << LL_ENDL;
					gAgentWearables.removeWearable(type, false, index);
					--count;
				}
				else
				{
					++index;
				}
			}
			else
			{
				llwarns << "Wearable for type " << type << " and layer "
						<< index << " not found in inventory !" << llendl;
				++index;
			}
		}
	}

	LL_DEBUGS("InitialOutfit") << "Worn items should now be matching the saved list."
							   << LL_ENDL;
}

LLAppearanceMgr::ERestoreOutfitStatus LLAppearanceMgr::restoreOutfit(bool can_retry)
{
	ERestoreOutfitStatus status = DONE;

	// First, create a list of currently worn inventory items
	uuid_list_t worn;

	// Add the worn attachments inventory items
	for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerObject* object =
			gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (object)
		{
			worn.emplace(object->getAttachmentItemID());
		}
	}

	// Add the worn boby parts and clothes inventory items
	for (U32 i = 0; i < (U32)LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		for (U32 index = 0, count = gAgentWearables.getWearableCount(type);
			 index < count; ++index)
		{
			LLInventoryItem* item =
				gInventory.getItem(gAgentWearables.getWearableItemID(type,
																	 index));
			if (item)
			{
				worn.emplace(item->getUUID());
			}
		}
	}

	// Now, compare to the saved outfit.xml file and re-wear items that are
	// not yet worn
	std::string filename = get_outfit_filename();
	llinfos << "Reading the saved outfit from: " << filename << llendl;
	LLSD list;
	llifstream llsd_xml(filename.c_str(), std::ios::in | std::ios::binary);
	if (llsd_xml.is_open())
	{
		LLSDSerialize::fromXML(list, llsd_xml);
		LL_DEBUGS("InitialOutfit") << "Got outfit items list:\n";
		std::stringstream str;
		LLSDSerialize::toPrettyXML(list, str);
		LL_CONT << "\n" << str.str() << LL_ENDL;
		for (LLSD::map_iterator iter = list.beginMap(),
								end = list.endMap();
			 iter != end; ++iter)
		{
			LLSD::String key_name = iter->first;
			LLSD array = iter->second;
			if (key_name == "attachments" && array.isArray())
			{
				for (S32 i = 0, count = array.size(); i < count; ++i)
				{
					LLSD map = array[i];
					if (map.has("inv_item_id"))
					{
						LLUUID item_id = map.get("inv_item_id");
						if (worn.find(item_id) == worn.end())
						{
							LLViewerInventoryItem* item =
								gInventory.getItem(item_id);
							if (item)
							{
								LL_DEBUGS("InitialOutfit") << "Reattaching: "
														   << item_id.asString()
														   << LL_ENDL;
								rezAttachment(item, NULL, false);
							}
							else
							{
								if (can_retry)
								{
									status = RETRY;
									LL_DEBUGS("InitialOutfit") << item_id.asString()
															   << " not yet found in inventory."
															   << LL_ENDL;
								}
								else
								{
									status = INCOMPLETE;
									llwarns << item_id.asString()
											<< " not found in inventory, could not reattach."
											<< llendl;
								}
							}
						}
						else
						{
							LL_DEBUGS("InitialOutfit") << "Object: " << item_id
													   << " already attached: OK."
													   << LL_ENDL;
						}
					}
					else
					{
						llwarns << "Malformed attachments list (no \"inv_item_id\" key). Aborting."
								<< llendl;
						llsd_xml.close();
						return FAILED;
					}
				}
			}
			else if (key_name == "wearables" && array.isArray())
			{
				for (S32 i = 0, count = array.size(); i < count; ++i)
				{
					LLSD map = array[i];
					if (map.has("inv_item_id"))
					{
						LLUUID item_id = map.get("inv_item_id");
						if (worn.find(item_id) == worn.end())
						{
							LLViewerInventoryItem* item =
								gInventory.getItem(item_id);
							if (item)
							{
								LL_DEBUGS("InitialOutfit") << "Rewearing: "
														   << item_id.asString()
														   << LL_ENDL;
								wearInventoryItemOnAvatar(item, false);
							}
							else
							{
								if (can_retry)
								{
									status = RETRY;
									LL_DEBUGS("InitialOutfit") << item_id.asString()
															   << " not yet found in inventory."
															   << LL_ENDL;
									break;	// Do not wear the rest: we must preserve the order
								}
								else
								{
									status = INCOMPLETE;
									llwarns << item_id.asString()
											<< " not found in inventory, could not rewear."
											<< llendl;
								}
							}
						}
						else
						{
							LL_DEBUGS("InitialOutfit") << "Wearable: " << item_id
													   << " already worn: OK."
													   << LL_ENDL;
						}

					}
					else
					{
						llwarns << "Malformed wearables list (no \"inv_item_id\" key). Aborting."
								<< llendl;
						llsd_xml.close();
						return FAILED;
					}
				}
			}
			else
			{
				llwarns << "Malformed outfit list. Aborting." << llendl;
				llsd_xml.close();
				return FAILED;
			}
		}
		llsd_xml.close();
	}
	else
	{
		llwarns << "Cannot open " << filename << " for outfit restoration."
				<< llendl;
		return FAILED;
	}

	return status;
}

void LLAppearanceMgr::saveOutfit()
{
	// This list will hold the full outfit list (attachments + wearables)
	LLSD list;

	// Save the worn attachments list
	LLSD array = LLSD::emptyArray();
	for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerObject* object =
			gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (object)
		{
			LLUUID item_id = object->getAttachmentItemID();
			LLViewerInventoryItem* item = gInventory.getItem(item_id);
			if (item)
			{
				LLSD entry = LLSD::emptyMap();
				entry.insert("inv_item_id", item_id);
				array.append(entry);
				LL_DEBUGS("InitialOutfit") << "Attachment " << item_id
										   << " saved in outfit list."
										   << LL_ENDL;
			}
			else
			{
				// This happens with temporary attachments
				LL_DEBUGS("InitialOutfit") << item_id
										   << " not found in inventory. Not saving in outfit list."
										   << LL_ENDL;
			}
		}
	}
	list.insert("attachments", array);

	// Save the worn body parts and clothes list
	array = LLSD::emptyArray();
	for (U32 i = 0; i < (U32)LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		U32 count = gAgentWearables.getWearableCount(type);
		if (count > 1 &&
			LLWearableType::getAssetType(type) == LLAssetType::AT_BODYPART)
		{
			llwarns << "More than one layer found for body part type: " << i
					<< llendl;
			count = 1;	// Paranoia: only one wearable per body part type.
		}
		for (U32 index = 0; index < count; ++index)
		{
			LLViewerInventoryItem* item =
				gInventory.getItem(gAgentWearables.getWearableItemID(type,
																	 index));
			if (item)
			{
				LLSD entry = LLSD::emptyMap();
				entry.insert("inv_item_id", item->getUUID());
				array.append(entry);
				LL_DEBUGS("InitialOutfit") << "Wearable " <<  item->getUUID()
										   << " saved in outfit list."
										   << LL_ENDL;
			}
			else
			{
				// This should not happen...
				llwarns << "Wearable type " << type << " on layer " << index
						<< " not found in inventory. Not saving in outfit list."
						<< llendl;
			}
		}
	}
	list.insert("wearables", array);

	// Save the list to the outfit.xml file
	std::string filename = get_outfit_filename();
	llofstream list_file(filename.c_str());
	if (list_file.is_open())
	{
		LLSDSerialize::toPrettyXML(list, list_file);
		list_file.close();
		LL_DEBUGS("InitialOutfit") << "Outfit items list saved to: " << filename;
		std::stringstream str;
		LLSDSerialize::toPrettyXML(list, str);
		LL_CONT << "\n" << str.str() << LL_ENDL;
	}
	else
	{
		llwarns << "Could not open file '" << filename << "' for wirting."
				<< llendl;
	}

	// Notify the Make New Outfit floater, if opened
	HBFloaterMakeNewOutfit::setDirty();

	mNeedsSyncAttachments = mNeedsSyncWearables = true;
	gAttachmentsListDirty = gWearablesListDirty = false;
	mUpdateCOFTimer.reset();
}

bool LLAppearanceMgr::isAvatarFullyBaked() const
{
	static LLCachedControl<bool> os_use_cof(gSavedSettings, "OSUseCOF");
	if (gIsInSecondLife || os_use_cof)
	{
		return !mIsRestoringInitialOutfit && !mRebaking &&
			   !mNeedsSyncAttachments && !mNeedsSyncWearables &&
			   !gAttachmentsListDirty && !gWearablesListDirty;
	}
	else
	{
		return !mIsRestoringInitialOutfit && !mRebaking &&
			   !gAttachmentsListDirty && !gWearablesListDirty;
	}
}

void LLAppearanceMgr::setRebaking(bool rebaking)
{
	if (mRebaking != rebaking)
	{
		mRebaking = rebaking;
		if (!rebaking && gAutomationp)
		{
			gAutomationp->onAgentBaked();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Simple, no-brainer re-implementation of the ugly COF concept... HB
///////////////////////////////////////////////////////////////////////////////

//static
const LLUUID LLAppearanceMgr::getCOF(bool create)
{
	// Note: in OpenSim, we allow to remove the COF, and its UUID may therefore
	// change during a viewer session, so we do not cache this UUID. HB
	return gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT,
											  create);
}

S32 LLAppearanceMgr::getCOFVersion() const
{
	LLViewerInventoryCategory* cof = gInventory.getCategory(getCOF());
	if (cof)
	{
		return cof->getVersion();
	}
	return LLViewerInventoryCategory::VERSION_UNKNOWN;
}

void LLAppearanceMgr::updateCOF() const
{
	const LLUUID cof_id = getCOF(true);
	gInventory.updateCategory(gInventory.getCategory(cof_id));
	gInventory.notifyObservers();
}

LLAppearanceMgr::ERestoreOutfitStatus LLAppearanceMgr::restoreOutfitFromCOF(bool can_retry)
{
	const LLUUID cof_id = getCOF(true);
	if (!gInventory.isCategoryComplete(cof_id))
	{
		return can_retry ? RETRY : FAILED;
	}

	LLViewerInventoryCategory* cof = gInventory.getCategory(cof_id);
	if (cof)
	{
		wearInventoryCategoryOnAvatar(cof, true, false);
		return DONE;
	}

	return FAILED;
}

void LLAppearanceMgr::cleanupCOF(const LLUUID& cof)
{
	const LLUUID& laf = gInventory.getLostAndFoundID();
	const LLUUID& trash = gInventory.getTrashID();

	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendents(cof, cats, items,
								  LLInventoryModel::EXCLUDE_TRASH);
	for (U32 i = 0, count = cats.size(); i < count; ++i)
	{
		LLViewerInventoryCategory* catp = cats[i];
		if (catp)	// Paranoia
		{
			// Move the folder to Lost And Found
			llwarns << "Found (non-link) folder '" << catp->getName()
					<< "' in COF: moving it to Lost And Found." << llendl;
			gInventory.changeCategoryParent(catp, laf, false);
		}
	}
	uuid_list_t linked_items_ids;
	for (U32 i = 0, count = items.size(); i < count; ++i)
	{
		LLViewerInventoryItem* itemp = items[i];
		if (!itemp) continue;	// Paranoia

		const LLUUID& item_id = itemp->getUUID();
		LLInventoryObject* objectp = gInventory.getObject(item_id);
		if (objectp &&
			objectp->getActualType() == LLAssetType::AT_LINK_FOLDER)
		{
			llinfos << "Trashing useless folder link '" << itemp->getName()
					<< "' out of COF." << llendl;
			gInventory.changeItemParent(itemp, trash, false);
			continue;
		}

		if (itemp->getIsBrokenLink())
		{
			remove_inventory_item(item_id);
		}
		else if (itemp->getIsLinkType())
		{
			const LLUUID& linked_id = itemp->getLinkedUUID();
			if (linked_items_ids.count(linked_id))
			{
				remove_inventory_item(item_id);
			}
			else
			{
				linked_items_ids.emplace(linked_id);
			}
		}
		else
		{
			// If it is not a link, move the item to Lost And Found instead
			// of purging it...
			const std::string& name = itemp->getName();
			llwarns << "Found (non-link) object '" << name
					<< "' in COF: moving it to Lost And Found." << llendl;
			move_inventory_item(item_id, laf, name);
		}
	}
}

void LLAppearanceMgr::syncAttachmentLinksInCOF()
{
	const LLUUID cof = getCOF(true);

	// Remove folders from COF (folder links are sometimes created by v4
	// viewers, but are totally useless !) as well as duplicate and broken
	// links
	cleanupCOF(cof);

	// Get the list of attached items in inventory:
	uuid_list_t attached_items_ids;
	for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerObject* objectp =
			gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (objectp)
		{
			const LLUUID& item_id = objectp->getAttachmentItemID();
			if (item_id.isNull())
			{
				llwarns << "Null inventory item UUID found for attached object "
						<< objectp->getID() << llendl;
				continue;
			}
			LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
			if (itemp)	// May be NULL for temporary attachments
			{
				attached_items_ids.emplace(item_id);
			}
		}
	}

	// Get the list of object items in COF:
	LLInventoryModel::item_array_t obj_items;
	getDescendentsOfAssetType(cof, obj_items, LLAssetType::AT_OBJECT);

	// Now remove from COF the links to non-attached objects
	uuid_list_t::iterator attach_id_it;
	for (U32 i = 0, count = obj_items.size(); i < count; ++i)
	{
		LLViewerInventoryItem* itemp = obj_items[i];
		if (itemp)	// Paranoia
		{
			LLUUID item_id = itemp->getUUID();
			attach_id_it = attached_items_ids.find(itemp->getLinkedUUID());
			if (attach_id_it != attached_items_ids.end())
			{
				// Get this item out of the list so that:
				//  1.- the next links pointing to it will be removed
				//  2.- it is removed from the list of items for which a new
				//      link needs to be created
				attached_items_ids.erase(attach_id_it);
				LL_DEBUGS("COF") << "Found a matching link in COF for attachment: "
								 << itemp->getName() << LL_ENDL;
			}
			else if (itemp->getIsLinkType())
			{
				LL_DEBUGS("COF") << "Purging link '" << itemp->getName()
								 << "' from COF." << LL_ENDL;
				remove_inventory_item(item_id);
			}
		}
	}

	// Link the remaining unlinked attachments
	uuid_list_t::iterator end = attached_items_ids.end();
	for (attach_id_it = attached_items_ids.begin(); attach_id_it != end;
		 ++attach_id_it)
	{
		LLViewerInventoryItem* itemp = gInventory.getItem(*attach_id_it);
		if (itemp)	// Paranoia
		{
			// Create a new link for this attached object
			LL_DEBUGS("COF") << "Creating a new link for attached item: "
							 << itemp->getName() << LL_ENDL;
			LLPointer<LLInventoryCallback> cb =
				new LLCreateLinkInCOFCallback(itemp->getUUID(), false);
			link_inventory_object(cof, LLPointer<LLInventoryObject>(itemp),
								  cb);
		}
	}

	updateCOF();

	mNeedsSyncAttachments = false;
	mUpdateCOFTimer.reset();
}

void LLAppearanceMgr::syncWearableLinksInCOF()
{
	const LLUUID cof = getCOF(true);

	// Remove folders from COF (folder links are sometimes created by v4
	// viewers, but are totally useless !) as well as duplicate and broken
	// links
	cleanupCOF(cof);

	// First collect all wearables items present in the COF
	LLInventoryModel::item_array_t wear_items;
	getDescendentsOfWearableTypes(cof, wear_items);

	// Second, remove from the COF all links to non-worn items and all non-link
	// wearable items (doing this first allows to ensure stale links will be
	// removed already when the last created link will fire the server-side
	// rebake callback).
	uuid_list_t linked_items;
	for (S32 i = 0, count = wear_items.size(); i < count; )
	{
		LLViewerInventoryItem* item = wear_items[i];
		if (!item)	// Paranoia
		{
			wear_items.erase(wear_items.begin() + i);
			--count;
			continue;
		}
		LLUUID item_id = item->getUUID();
		LLUUID linked_item_id = item->getLinkedUUID();
		bool link_to_worn_item = item->getIsLinkType() &&
								 !item->getIsBrokenLink() &&
								 gAgentWearables.isWearingItem(linked_item_id);
		if (link_to_worn_item && (mForceServerSideRebake || mBakeRequestSent))
		{
			// Force the removal of the first link to a worn item, so to force
			// a COF version update and thus, a rebake.
			mForceServerSideRebake = false;
			link_to_worn_item = false;
		}
		if (!link_to_worn_item || linked_items.count(linked_item_id))
		{
			if (item->getIsLinkType())
			{
				LL_DEBUGS("COF") << "Purging link to wearable '"
								 << item->getName()
								 << "' from COF." << LL_ENDL;
				remove_inventory_item(item_id);
			}
			wear_items.erase(wear_items.begin() + i);
			--count;
		}
		else
		{
			linked_items.emplace(item->getLinkedUUID());
			++i;
		}
	}

	// Third, update or create links to worn items
	LLUUID item_id;
	std::string order_str;
	for (U32 i = 0; i < (U32)LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		U32 count = gAgentWearables.getWearableCount(type);
		if (count > 1 &&
			LLWearableType::getAssetType(type) == LLAssetType::AT_BODYPART)
		{
			LL_DEBUGS("COF") << "More that one layer found for body part type "
							 << i << LL_ENDL;
			count = 1;	// Paranoia: only one wearable per body part type.
		}
		for (U32 index = 0; index < count; ++index)
		{
			LLViewerInventoryItem* item =
				gInventory.getItem(gAgentWearables.getWearableItemID(type,
																	 index));
			if (!item) continue;	// Paranoia
			item_id = item->getUUID();
			order_str = build_order_string(type, index);
			bool link_exists = false;
			for (S32 j = 0, n = wear_items.size(); j < n; ++j)
			{
				LLViewerInventoryItem* link_item = wear_items[j];
				if (link_item && link_item->getIsLinkType() &&
					link_item->getLinkedUUID() == item_id)
				{
					link_exists = true;
					// If needed, update the link description to match the
					// current layer index...
					if (link_item->getActualDescription() != order_str)
					{
						LL_DEBUGS("COF") << "Changing layer info for item: "
										 << link_item->getName() << LL_ENDL;
						link_item->setDescription(order_str);
						link_item->setComplete(true);
						link_item->updateServer(false);
						gInventory.updateItem(link_item);
					}
					else
					{
						LL_DEBUGS("COF") << "A link already exists for item: "
										 << link_item->getName() << LL_ENDL;
					}

					// Makes next searches faster
					wear_items.erase(wear_items.begin() + j);

					break;
				}
			}
			if (!link_exists)
			{
				// Create a new link for this worn item
				LL_DEBUGS("COF") << "Creating a new link for worn item: "
								 << item->getName() << LL_ENDL;
				LLPointer<LLInventoryCallback> cb =
					new LLCreateLinkInCOFCallback(item->getUUID(), true);
				link_inventory_item(item->getUUID(), cof, order_str,
									LLAssetType::AT_LINK, cb);
			}
		}
	}

	updateCOF();

	mNeedsSyncWearables = false;
	mUpdateCOFTimer.reset();
}

void LLAppearanceMgr::slamCOF()
{
	// Create a list of links to worn items
	LLSD contents = LLSD::emptyArray();

	// Start with wearables
	for (U32 i = 0; i < (U32)LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		U32 count = gAgentWearables.getWearableCount(type);
		if (count > 1 &&
			LLWearableType::getAssetType(type) == LLAssetType::AT_BODYPART)
		{
			LL_DEBUGS("COF") << "More that one layer found for body part type "
							 << i << LL_ENDL;
			count = 1;	// Paranoia: only one wearable per body part type.
		}
		for (U32 index = 0; index < count; ++index)
		{
			LLViewerInventoryItem* item =
				gInventory.getItem(gAgentWearables.getWearableItemID(type,
																	 index));
			if (!item) continue;	// Paranoia

			LLSD item_contents;
			item_contents["name"] = item->getName();
			item_contents["desc"] = build_order_string(type, index);
			item_contents["linked_id"] = item->getLinkedUUID();
			item_contents["type"] = LLAssetType::AT_LINK;
			contents.append(item_contents);
		}
	}

	// Now for attachments...
	for (U32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerObject* object =
			gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (!object) continue;	// Paranoia

		LLViewerInventoryItem* item =
			gInventory.getItem(object->getAttachmentItemID());
		if (!item)	// May be NULL for temporary attachments
		{
			continue;
		}

		LLSD item_contents;
		item_contents["name"] = item->getName();
		item_contents["desc"] = item->getDescription();
		item_contents["linked_id"] = item->getLinkedUUID();
		item_contents["type"] = LLAssetType::AT_LINK;
		contents.append(item_contents);
	}

	// Slam the COF with new links listed in contents
	const LLUUID cof = getCOF(true);
	LLPointer<LLInventoryCallback> cb = new LLSlamCOFCallback();
	slam_inventory_folder(cof, contents, cb);

	mNeedsSyncWearables = mNeedsSyncAttachments = false;
	mUpdateCOFTimer.reset();
}

void LLAppearanceMgr::incrementCofVersion()
{
	if (!gAgent.regionCapabilitiesReceived())
	{
		// Mark as needing a rebake after the capabilities for the new agent
		// region are received.
		gAgent.mRebakeNeeded = true;
		return;
	}
	llinfos << "Forcing an update of the COF and a rebake." << llendl;
	if (!AISAPI::isAvailable())
	{
		const LLUUID cof = getCOF(true);
		remove_folder_contents(cof, LLPointer<LLInventoryCallback>(NULL));
	}
	mForceServerSideRebake = true;
	mNeedsSyncWearables = mNeedsSyncAttachments = true;
}

///////////////////////////////////////////////////////////////////////////////
// Server-side baking stuff.
///////////////////////////////////////////////////////////////////////////////

void LLAppearanceMgr::requestServerAppearanceUpdate()
{
	if (!isAgentAvatarValid() || gAgentAvatarp->isEditingAppearance())
	{
		// Do not send out appearance updates if in appearance editing mode
		LL_DEBUGS("Appearance") << "Not sending appearance updates during editing."
								<< LL_ENDL;
		return;
	}
	if (!LLVOAvatarSelf::canUseServerBaking())
	{
		llwarns << "Server-side baking not enabled. Aborting." << llendl;
		return;
	}

	if (mBakeRequestSent)
	{
		llinfos << "Server-side rebake already requested and will be retried."
				<< llendl;
		mUpdateCOFTimer.reset();
		return;
	}

	setRebaking();

	S32 cof_version = getCOFVersion();
	llinfos << "Sending server-side rebake request with COF version: "
			<< cof_version << " (last requested version: "
			<< gAgentAvatarp->mLastUpdateRequestCOFVersion
			<< " - last received update version: "
			<< gAgentAvatarp->mLastUpdateReceivedCOFVersion << ")" << llendl;
	gAgentAvatarp->mLastUpdateRequestCOFVersion = cof_version;

	LLSD body;
	body["cof_version"] = cof_version;

	LLAgent::httpCallback_t succ =
		boost::bind(&LLAppearanceMgr::serverAppearanceUpdateSuccess, this, _1);
	LLAgent::httpCallback_t fail =
		boost::bind(&LLAppearanceMgr::serverAppearanceUpdateFailure, this, _1);

	mBakeRequestSent = true;
	if (!gAgent.requestPostCapability("UpdateAvatarAppearance", body,
									  succ, fail))
	{
		mBakeRequestSent = false;
	}
}

void LLAppearanceMgr::serverAppearanceUpdateSuccess(const LLSD& result)
{
	mBakeRequestSent = false;

	if (result.isMap() && result.has("success") &&
		result["success"].asBoolean())
	{
		if (mBakeRetryPolicy.notNull())
		{
			mBakeRetryPolicy->onSuccess();
		}
		llinfos << "Request OK." << llendl;
		setRebaking(false);
	}
	else
	{
		const LLSD& http_results =
			result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		serverAppearanceUpdateFailure(http_results);
	}
}

void LLAppearanceMgr::serverAppearanceUpdateFailure(const LLSD& http_results)
{
	if (!isAgentAvatarValid())
	{
		// Oops... Logged off ?
		return;
	}

	mBakeRequestSent = false;

	S32 status = http_results["type"].asInteger();
	llwarns << "Appearance update request failed with status: " << status
			<< llendl;

	LL_DEBUGS("COF") << "HTTP results:\n";
	std::stringstream str;
	LLSDSerialize::toPrettyXML(http_results, str);
	LL_CONT << str.str() << LL_ENDL;

	S32 requested_version = gAgentAvatarp->mLastUpdateRequestCOFVersion;
	S32 expected = requested_version;
	if (http_results.has("error_body"))
	{
		const LLSD& error_body = http_results["error_body"];
		if (error_body.has("expected"))
		{
			expected = error_body["expected"].asInteger();
		}
	}

	if (status == 404)
	{
		llwarns << "Aborting after a 404 error." << llendl;
	}
	else if (requested_version != LLViewerInventoryCategory::VERSION_UNKNOWN &&
			 requested_version < getCOFVersion())
	{
		llinfos << "COF got updated, aborting this request and scheduling another"
				<< llendl;
	}
#if 0	// This does not work... Use the COF refetching below instead. HB
	else if (expected > requested_version)
	{
		llinfos << "Requested COF version was: " << requested_version
				<< " - COF version expected by the server was: " << expected
				<< ". Aborting this request and scheduling another."
				<< llendl;

		// Force an update texture request for ourself. The message will return
		// via UDP and be handled in LLVOAvatar::processAvatarAppearance();
		// this should ensure that we receive a new canonical COF from the sim
		// host. Hopefully it will return before the next rebake retry.
		gAgentAvatarp->sendAvatarTexturesRequest(true);
	}
#endif
	else
	{
		if (mBakeRetryPolicy.isNull())
		{
			mBakeRetryPolicy = new LLAdaptiveRetryPolicy(1.f, 16.f, 2.f, 4,
														 true);
		}
		mBakeRetryPolicy->onFailure(status, http_results["headers"]);

		F32 seconds_to_wait;
		if (mBakeRetryPolicy->shouldRetry(seconds_to_wait))
		{
			if (expected < requested_version)
			{
				llinfos << "Requested COF version was: " << requested_version
						<< " - COF version expected by the server was: "
						<< expected << llendl;
			}
			llinfos << "Retrying..." << llendl;
			doAfterInterval(boost::bind(&LLAppearanceMgr::requestServerAppearanceUpdate,
										&gAppearanceMgr),
							seconds_to_wait);
			return;
		}
		else
		{
			llwarns << "Giving up after too many retries." << llendl;
			if (expected != requested_version)
			{
				// We obviously went out of sync between viewer and server,
				// So try and refetch the COF with the proper version as
				// seen from the server...
				llinfos << "Refetching the COF from the server" << llendl;
				gAgentAvatarp->mLastUpdateRequestCOFVersion =
					LLViewerInventoryCategory::VERSION_UNKNOWN;
				LLViewerInventoryCategory* cof =
					gInventory.getCategory(getCOF(true));
				cof->setVersionUnknown();
				cof->fetch();
			}
		}
	}

	if (mBakeRetryPolicy.notNull())
	{
		mBakeRetryPolicy->reset();
	}

	// Fire a new rebake request after incrementing the COF version
	incrementCofVersion();
}
