/**
 * @file llinventoryactions.cpp
 * @brief Implementation of the actions associated with menu items.
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

#include "llviewerprecompiledheaders.h"

#include <utility>

#include "llinventoryactions.h"

#include "llnotifications.h"
#include "lltrans.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "llavatartracker.h"
#include "llenvsettings.h"
#include "llfloateravatarinfo.h"
#include "llfloaterinventory.h"
#include "hbfloatermakenewoutfit.h"
#include "llfloaterperms.h"
#include "llfloaterproperties.h"
#include "llfloaterworldmap.h"
#include "llfolderview.h"
#include "llimmgr.h"
#include "llinventorybridge.h"
#include "llpanelinventory.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewlandmark.h"
#include "llpreviewmaterial.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llwearablelist.h"

using namespace LLOldEvents;

const std::string NEW_LSL_NAME = "New script";
const std::string NEW_NOTECARD_NAME = "New note";
const std::string NEW_GESTURE_NAME = "New gesture";
const std::string NEW_MATERIAL_NAME = "New material";

typedef LLMemberListener<LLPanelInventory> object_inventory_listener_t;
typedef LLMemberListener<LLFloaterInventory> inventory_listener_t;
typedef LLMemberListener<LLInventoryPanel> inventory_panel_listener_t;

static void create_category_cb(const LLUUID& cat_id, LLHandle<LLPanel> handle)
{
	gInventory.notifyObservers();
	// If possible, select the newly created folder in the inventory panel
	// (when still around).
	LLInventoryPanel* panelp = (LLInventoryPanel*)handle.get();
	if (panelp)
	{
		panelp->setSelection(cat_id, true);
		LLFolderView* folderp = panelp->getRootFolder();
		LLFolderViewItem* itemp = folderp->getItemByID(cat_id);
		if (itemp)
		{
			itemp->setOpen(true);
		}
	}
}

static void move_to_folder_cb(const LLUUID& cat_id, uuid_vec_t selected_items,
							  LLHandle<LLPanel> handle)
{
	if (cat_id.isNull())
	{
		return;
	}
	gInventory.notifyObservers();
	reparent_to_folder(cat_id, selected_items);
	create_category_cb(cat_id, handle);
}

static bool move_to_folder(LLInventoryPanel* panelp,
						   uuid_vec_t selected_items,
						   const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		std::string folder_name = response["message"].asString();
		LLInventoryObject::correctInventoryName(folder_name);
		if (folder_name.empty())
		{
			folder_name = "New folder";
		}
		LLInventoryObject* invobjp = gInventory.getObject(selected_items[0]);
		if (!invobjp)
		{
			return false;
		}
		LLHandle<LLPanel> handle;
		if (panelp)
		{
			handle = panelp->getHandle();
		}
		inventory_func_t func = boost::bind(&move_to_folder_cb, _1,
											selected_items, handle);
		gInventory.createNewCategory(invobjp->getParentUUID(),
									 LLFolderType::FT_NONE, folder_name, func);
	}
	return false;
}

bool doToSelected(LLInventoryPanel* panelp, LLFolderView* folderp,
				  const std::string& action)
{
	LLInventoryModel* modelp = &gInventory;
	if (action == "rename")
	{
		folderp->startRenamingSelectedItem();
		return true;
	}
	if (action == "delete")
	{
		folderp->removeSelectedItems();
		modelp->checkTrashOverflow();
		return true;
	}
	if (action == "copy")
	{
		folderp->copy();
		return true;
	}
	if (action == "cut")
	{
		folderp->cut();
		return true;
	}
	if (action == "paste")
	{
		folderp->paste();
		return true;
	}

	uuid_vec_t selected_items;
	folderp->getSelection(selected_items);

	if (panelp && action == "group")
	{
		if (movable_objects_with_same_parent(selected_items))
		{
			gNotifications.add("CreateSubfolder", LLSD(), LLSD(),
						   	   boost::bind(&move_to_folder, panelp,
										   selected_items, _1, _2));
		}
		return true;
	}

	if (panelp && action == "degroup")
	{
		if (selected_items.size() != 1)
		{
			return true;
		}
		LLUUID cat_id = selected_items[0];
		LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
		if (!cat)
		{
			return true;
		}
		const LLUUID& parent_id = cat->getParentUUID();
		if (parent_id.isNull())
		{
			return true;
		}
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(cat_id, cats, items);
		// NOTE: we cannot direclty use the pointers to inventory objects in
		// cats and items (because these are pointer on the internal inventory
		// structure that itself gets modifed as we move the objects): we must
		// instead collect all the UUIDs, and then use our reparent_to_folder()
		// utility function. HB
		selected_items.clear();
		typedef LLInventoryModel::cat_array_t::const_iterator cat_it_t;
		for (cat_it_t it = cats->begin(), end = cats->end(); it != end; ++it)
		{
			selected_items.emplace_back(it->get()->getUUID());
		}
		typedef LLInventoryModel::item_array_t::const_iterator item_it_t;
		for (item_it_t it = items->begin(), end = items->end(); it != end;
			 ++it)
		{
			selected_items.emplace_back(it->get()->getUUID());
		}
		reparent_to_folder(parent_id, selected_items);
		cat = gInventory.getCategory(cat_id);
		if (cat)
		{
			const LLUUID& trash_id = gInventory.getTrashID();
			if (trash_id.notNull())
			{
				gInventory.changeCategoryParent(cat, trash_id, false);
				gInventory.notifyObservers();
			}
		}
		return true;
	}

	LLMultiPreview* multi_previewp = NULL;
	LLMultiProperties* multi_propertiesp = NULL;
	{	// Scope for LLHostFloater (must be closed before calling open() on the
		// multi-preview). HB
		LLHostFloater host;
		if (selected_items.size() > 1)
		{
			if (action == "task_open" || action == "open")
			{
				bool open_multi_preview = true;
				for (U32 i = 0, count = selected_items.size(); i < count; ++i)
				{
					const LLUUID& id = selected_items[i];
					LLFolderViewItem* folder_item = folderp->getItemByID(id);
					if (!folder_item)
					{
						continue;
					}
					LLInvFVBridge* bridge =
						dynamic_cast<LLInvFVBridge*>(folder_item->getListener());
					if (bridge && !bridge->isMultiPreviewAllowed())
					{
						open_multi_preview = false;
						break;
					}
				}
				if (open_multi_preview)
				{
					S32 left, top;
					gFloaterViewp->getNewFloaterPosition(&left, &top);

					multi_previewp = new LLMultiPreview(LLRect(left, top,
														left + 300,
														top - 100));
					gFloaterViewp->addChild(multi_previewp);

					host.set(multi_previewp);
				}
			}
			else if (action == "task_properties" || action == "properties")
			{
				S32 left, top;
				gFloaterViewp->getNewFloaterPosition(&left, &top);

				multi_propertiesp = new LLMultiProperties(LLRect(left, top,
																 left + 100,
																 top - 100));
				gFloaterViewp->addChild(multi_propertiesp);

				host.set(multi_propertiesp);
			}
		}

		for (U32 i = 0, count = selected_items.size(); i < count; ++i)
		{
			const LLUUID& id = selected_items[i];
			LLFolderViewItem* folder_item = folderp->getItemByID(id);
			if (folder_item)
			{
				LLFolderViewEventListener* listener =
					folder_item->getListener();
				if (listener)
				{
					listener->performAction(folderp, modelp, action);
				}
			}
		}
	}

	if (multi_previewp)
	{
		multi_previewp->open();
	}
	else if (multi_propertiesp)
	{
		multi_propertiesp->open();
	}

	return true;
}

class LLDoToSelectedPanel : public object_inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string action = userdata.asString();
		LLPanelInventory* panelp = mPtr;
		LLFolderView* folderp = panelp->getRootFolder();
		if (!folderp) return true;

		return doToSelected(NULL, folderp, action);
	}
};

class LLDoToSelectedFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string action = userdata.asString();
		LLInventoryPanel* panelp = mPtr->getPanel();
		LLFolderView* folderp = panelp->getRootFolder();
		if (!folderp) return true;

		return doToSelected(panelp, folderp, action);
	}
};

class LLDoToSelected : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string action = userdata.asString();
		LLInventoryPanel* panelp = mPtr;
		LLFolderView* folderp = panelp->getRootFolder();
		if (!folderp) return true;

		return doToSelected(panelp, folderp, action);
	}
};

class LLNewWindow : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel* panelp = mPtr->getActivePanel();
		if (!panelp) return true;	// Paranoia

		LLRect rect(gSavedSettings.getRect("FloaterInventoryRect"));
		S32 left = 0 , top = 0;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		rect.setLeftTopAndSize(left, top, rect.getWidth(), rect.getHeight());
		LLFloaterInventory* floaterp =
			new LLFloaterInventory("Inventory", rect, panelp->getModel());
		floaterp->getActivePanel()->setFilterTypes(panelp->getFilterTypes());
		floaterp->getActivePanel()->setFilterSubString(panelp->getFilterSubString());
		floaterp->open();
		// Force on screen
		gFloaterViewp->adjustToFitScreen(floaterp);

		return true;
	}
};

class LLShowFilters : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		mPtr->toggleFindOptions();
		return true;
	}
};

class LLResetFilter : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		if (!mPtr->getActivePanel()) return true;	// Paranoia

		LLFloaterInventoryFilters* filters = mPtr->getInvFilters();
		mPtr->getActivePanel()->getFilter()->resetDefault();
		if (filters)
		{
			filters->updateElementsFromFilter();
		}

		mPtr->setFilterTextFromFilter();
		return true;
	}
};

class LLCloseAllFolders : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		mPtr->closeAllFolders();
		return true;
	}
};

class LLCloseAllFoldersFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		if (mPtr->getPanel())	// Paranoia
		{
			mPtr->getPanel()->closeAllFolders();
		}
		return true;
	}
};

class LLEmptyTrash : public inventory_panel_listener_t
{
protected:
	LOG_CLASS(LLEmptyTrash);

	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* modelp = mPtr->getModel();
		if (!modelp) return true;

		const LLUUID& trash_id = modelp->getTrashID();
		if (trash_id.isNull() || !modelp->isCategoryComplete(trash_id))
		{
			llwarns << "Not purging the incompletely downloaded Trash folder"
					<< llendl;
			return true;
		}

		gNotifications.add("ConfirmEmptyTrash", LLSD(), LLSD(),
						   boost::bind(&LLEmptyTrash::cb_empty_trash,
									   this, _1, _2));
		return true;
	}

	bool cb_empty_trash(const LLSD& notification, const LLSD& response)
	{
		if (LLNotification::getSelectedOption(notification, response) == 0)
		{
			LLInventoryModel* modelp = mPtr->getModel();
			if (modelp)
			{
				const LLUUID& trash_id = modelp->getTrashID();
				if (trash_id.isNull())
				{
					llwarns << "Could not find the Trash folder" << llendl;
					return false;
				}
				purge_descendents_of(trash_id);
				modelp->notifyObservers();
			}
		}
		return false;
	}
};

class LLEmptyLostAndFound : public inventory_panel_listener_t
{
protected:
	LOG_CLASS(LLEmptyLostAndFound);

	bool handleEvent(LLPointer<LLEvent> event, const LLSD&)
	{
		LLInventoryModel* modelp = mPtr->getModel();
		if (!modelp) return true;

		const LLUUID& laf_id = modelp->getLostAndFoundID();
		if (laf_id.isNull() || !modelp->isCategoryComplete(laf_id))
		{
			llwarns << "Not purging the incompletely downloaded Lost and found folder"
					<< llendl;
			return true;
		}

		gNotifications.add("ConfirmEmptyLostAndFound", LLSD(), LLSD(),
						   boost::bind(&LLEmptyLostAndFound::cb_purge_laf,
									  this, _1, _2));
		return true;
	}

	bool cb_purge_laf(const LLSD& notification, const LLSD& response)
	{
		if (LLNotification::getSelectedOption(notification, response) == 0)
		{
			LLInventoryModel* modelp = mPtr->getModel();
			if (!modelp) return false;

			const LLUUID& laf_id = modelp->getLostAndFoundID();
			if (laf_id.notNull())
			{
				purge_descendents_of(laf_id);
				modelp->notifyObservers();
			}
		}
		return false;
	}
};

class LLHideEmptySystemFolders : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool hide = !gSavedSettings.getBool("HideEmptySystemFolders");
		gSavedSettings.setBool("HideEmptySystemFolders", hide);
		// Force a new filtering
		mPtr->getActivePanel()->getFilter()->setModified();
		return true;
	}
};

class LLHideMarketplaceFolder : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool hide = !gSavedSettings.getBool("HideMarketplaceFolder");
		gSavedSettings.setBool("HideMarketplaceFolder", hide);
		// Force a new filtering
		mPtr->getActivePanel()->getFilter()->setModified();
		return true;
	}
};

class LLHideCurrentOutfitFolder : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool hide = !gSavedSettings.getBool("HideCurrentOutfitFolder");
		gSavedSettings.setBool("HideCurrentOutfitFolder", hide);
		// Force a new filtering
		mPtr->getActivePanel()->getFilter()->setModified();
		return true;
	}
};

class LLCheckSystemFolders : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel::checkSystemFolders();
		return true;
	}
};

class LLResyncCallingCards : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		const LLUUID& parent_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		LLViewerInventoryCategory* cat = gInventory.getCategory(parent_id);
		if (!cat) return true;

		// First, get the list of existing calling cards in the folder and its
		// sub-folders.
		LLBuddyCollector match_functor;
		LLInventoryModel::cat_array_t cats;
		cats.emplace_back(cat);
		LLInventoryModel::item_array_t items;
		while (!cats.empty())
		{
			const LLUUID& cat_id = cats[0]->getUUID();
			cats.erase(cats.begin());
			gInventory.collectDescendentsIf(cat_id, cats, items,
											LLInventoryModel::EXCLUDE_TRASH,
											match_functor);
		}
		std::set<std::string> buddy_cards;
		for (U32 i = 0, count = items.size(); i < count; ++i)
		{
			buddy_cards.emplace(items[i]->getName());
		}
		items.clear();

		LLCollectAllBuddies collector;
		gAvatarTracker.applyFunctor(collector);
		typedef LLCollectAllBuddies::buddy_map_t::const_iterator buddies_it;
		for (buddies_it it = collector.mOnline.begin(),
						end = collector.mOnline.end();
			 it != end; ++it)
		{
			if (buddy_cards.count(it->first))
			{
				continue;
			}
			create_new_item(it->first, parent_id, LLAssetType::AT_CALLINGCARD,
							LLInventoryType::IT_CALLINGCARD,
							PERM_ALL & ~PERM_MODIFY, it->second.asString());
		}
		for (buddies_it it = collector.mOffline.begin(),
						end = collector.mOffline.end();
			 it != end; ++it)
		{
			if (buddy_cards.count(it->first))
			{
				continue;
			}
			create_new_item(it->first, parent_id, LLAssetType::AT_CALLINGCARD,
							LLInventoryType::IT_CALLINGCARD,
							PERM_ALL & ~PERM_MODIFY, it->second.asString());
		}
		return true;
	}
};

class LLMakeNewOutfit : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		HBFloaterMakeNewOutfit::showInstance();
		return true;
	}
};

class LLEmptyTrashFloater : public inventory_listener_t
{
protected:
	LOG_CLASS(LLEmptyTrashFloater);

	bool handleEvent(LLPointer<LLEvent> event, const LLSD&)
	{
		LLInventoryModel* modelp = mPtr->getPanel()->getModel();
		if (modelp)
		{
			const LLUUID& trash_id = modelp->getTrashID();
			if (trash_id.notNull())
			{
				purge_descendents_of(trash_id);
				modelp->notifyObservers();
			}
			else
			{
				llwarns << "Could not find the Trash folder" << llendl;
			}
		}
		return true;
	}
};

#if 0	// Wear on create is not implemented/used in the Cool VL Viewer
static void create_wearable(LLWearableType::EType type,
							const LLUUID& parent_id, bool wear = false)
#else
static void create_wearable(LLWearableType::EType type,
							const LLUUID& parent_id)
#endif
{
	if (type == LLWearableType::WT_INVALID ||
		type == LLWearableType::WT_NONE ||
		!isAgentAvatarValid())
	{
		return;
	}

	if (type == LLWearableType::WT_UNIVERSAL)
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		if (!regionp || !regionp->bakesOnMeshEnabled())
		{
			llwarns << "Cannot create Universal wearable type in this region"
					<< llendl;
			return;
		}
	}

	LLViewerWearable* wearable =
		LLWearableList::getInstance()->createNewWearable(type, gAgentAvatarp);
	LLAssetType::EType asset_type = wearable->getAssetType();
	LLInventoryType::EType inv_type = LLInventoryType::IT_WEARABLE;
#if 0	// Not implemented/used in the Cool VL Viewer
	LLPointer<LLInventoryCallback> cb = wear ? new LLWearAndEditCallback : NULL;
#else
	LLPointer<LLInventoryCallback> cb = NULL;
#endif

	LLUUID folder_id;
	if (parent_id.notNull())
	{
		folder_id = parent_id;
	}
	else
	{
		LLFolderType::EType folder_type =
			LLFolderType::assetTypeToFolderType(asset_type);
		folder_id = gInventory.findCategoryUUIDForType(folder_type);
	}

	create_inventory_item(folder_id, wearable->getTransactionID(),
						  wearable->getName(), wearable->getDescription(),
						  asset_type, inv_type, (U8)wearable->getType(),
						  wearable->getPermissions().getMaskNextOwner(), cb);
}

static void do_create(LLInventoryModel* modelp, LLInventoryPanel* panelp,
					  const std::string& type, LLFolderBridge* self = NULL)
{
	if (type == "category")
	{
		const LLUUID& parent_id = self ? self->getUUID()
									   : gInventory.getRootFolderID();
		LLHandle<LLPanel> handle;
		if (panelp)
		{
			handle = panelp->getHandle();
		}
		inventory_func_t func = boost::bind(&create_category_cb, _1, handle);
		modelp->createNewCategory(parent_id, LLFolderType::FT_NONE,
								  LLStringUtil::null, func);
	}
	else if (type == "lsl")
	{
		U32 perms = PERM_MOVE | LLFloaterPerms::getNextOwnerPerms();
		if (gSavedSettings.getBool("NoModScripts"))
		{
			perms = perms & ~PERM_MODIFY;
		}
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : modelp->findCategoryUUIDForType(LLFolderType::FT_LSL_TEXT);
		create_new_item(NEW_LSL_NAME, parent_id, LLAssetType::AT_LSL_TEXT,
						LLInventoryType::IT_LSL, perms);
	}
	else if (type == "notecard")
	{
		U32 perms = PERM_MOVE | LLFloaterPerms::getNextOwnerPerms();
		if (gSavedSettings.getBool("FullPermNotecards"))
		{
			perms = PERM_ALL;
		}
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : modelp->findCategoryUUIDForType(LLFolderType::FT_NOTECARD);
		create_new_item(NEW_NOTECARD_NAME, parent_id, LLAssetType::AT_NOTECARD,
						LLInventoryType::IT_NOTECARD, perms);
	}
	else if (type == "gesture")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : modelp->findCategoryUUIDForType(LLFolderType::FT_GESTURE);
		create_new_item(NEW_GESTURE_NAME, parent_id, LLAssetType::AT_GESTURE,
						LLInventoryType::IT_GESTURE,
						PERM_MOVE | LLFloaterPerms::getNextOwnerPerms());
	}
	else if (type == "material")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : modelp->findCategoryUUIDForType(LLFolderType::FT_MATERIAL);
		create_new_item(NEW_MATERIAL_NAME, parent_id, LLAssetType::AT_MATERIAL,
						LLInventoryType::IT_MATERIAL,
						LLFloaterPerms::getNextOwnerPerms());
	}
	else if (type == "callingcard")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		std::string name;
		gAgent.getName(name);
		create_new_item(name, parent_id, LLAssetType::AT_CALLINGCARD,
						LLInventoryType::IT_CALLINGCARD,
						PERM_ALL & ~PERM_MODIFY, gAgentID.asString());
	}
	else if (type == "shirt")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_SHIRT, parent_id);
	}
	else if (type == "pants")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_PANTS, parent_id);
	}
	else if (type == "shoes")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_SHOES, parent_id);
	}
	else if (type == "socks")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_SOCKS, parent_id);
	}
	else if (type == "jacket")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_JACKET, parent_id);
	}
	else if (type == "skirt")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_SKIRT, parent_id);
	}
	else if (type == "gloves")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_GLOVES, parent_id);
	}
	else if (type == "undershirt")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_UNDERSHIRT, parent_id);
	}
	else if (type == "underpants")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_UNDERPANTS, parent_id);
	}
	else if (type == "alpha")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_ALPHA, parent_id);
	}
	else if (type == "tattoo")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_TATTOO, parent_id);
	}
	else if (type == "universal")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_UNIVERSAL, parent_id);
	}
	else if (type == "physics")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
		create_wearable(LLWearableType::WT_PHYSICS, parent_id);
	}
	else if (type == "shape")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
		create_wearable(LLWearableType::WT_SHAPE, parent_id);
	}
	else if (type == "skin")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
		create_wearable(LLWearableType::WT_SKIN, parent_id);
	}
	else if (type == "hair")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
		create_wearable(LLWearableType::WT_HAIR, parent_id);
	}
	else if (type == "eyes")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_BODYPART);
		create_wearable(LLWearableType::WT_EYES, parent_id);
	}
	else if (type == "sky")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
		LLEnvSettingsBase::createNewInventoryItem(LLSettingsType::ST_SKY,
												  parent_id);
	}
	else if (type == "water")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
		LLEnvSettingsBase::createNewInventoryItem(LLSettingsType::ST_WATER,
												  parent_id);
	}
	else if (type == "day")
	{
		const LLUUID& parent_id =
			self ? self->getUUID()
				 : gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
		LLEnvSettingsBase::createNewInventoryItem(LLSettingsType::ST_DAYCYCLE,
												  parent_id);
	}

	panelp->getRootFolder()->setNeedsAutoRename(true);
}

class LLDoCreate : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* modelp = mPtr->getModel();
		if (modelp)
		{
			do_create(modelp, mPtr, userdata.asString(),
					  LLFolderBridge::sSelf);
		}
		return true;
	}
};

class LLFileUploadLocation : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* modelp = mPtr->getModel();
		if (!modelp) return true;

		std::string setting_name = userdata.asString();
		LLControlVariable* control =
			gSavedPerAccountSettings.getControl(setting_name.c_str());
		if (control)
		{
			control->setValue(LLFolderBridge::sSelf->getUUID().asString());
		}
		return true;
	}
};

class LLDoCreateFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* modelp = mPtr->getPanel()->getModel();
		if (modelp)
		{
			do_create(modelp, mPtr->getPanel(), userdata.asString());
		}
		return true;
	}
};

class LLSetSortBy : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string sort_field = userdata.asString();
		U32 order = mPtr->getActivePanel()->getSortOrder();
		if (sort_field == "name")
		{
			order &= ~LLInventoryFilter::SO_DATE;
		}
		else if (sort_field == "date")
		{
			order |= LLInventoryFilter::SO_DATE;
		}
		else if (sort_field == "foldersalwaysbyname")
		{
			if (order & LLInventoryFilter::SO_FOLDERS_BY_NAME)
			{
				order &= ~LLInventoryFilter::SO_FOLDERS_BY_NAME;
			}
			else
			{
				order |= LLInventoryFilter::SO_FOLDERS_BY_NAME;
			}
		}
		else if (sort_field == "systemfolderstotop")
		{
			if (order & LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP)
			{
				order &= ~LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;
			}
			else
			{
				order |= LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;
			}
		}
		mPtr->getActivePanel()->setSortOrder(order);
		mPtr->updateSortControls();

		return true;
	}
};

class LLSetSearchType : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string toggle = userdata.asString();
		U32 flags = mPtr->getActivePanel()->getRootFolder()->toggleSearchType(toggle);
		mPtr->getControl("Inventory.SearchName")->setValue((flags & 1) != 0);
		mPtr->getControl("Inventory.SearchDesc")->setValue((flags & 2) != 0);
		mPtr->getControl("Inventory.SearchCreator")->setValue((flags & 4) != 0);
		return true;
	}
};

class LLBeginIMSession : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel* panelp = mPtr;
		LLInventoryModel* modelp = panelp->getModel();
		if (!modelp) return true;

		uuid_list_t selected_items;
		panelp->getRootFolder()->getSelectionList(selected_items);

		std::string name;
		static S32 session_num = 1;

		uuid_vec_t members;
		EInstantMessage type = IM_SESSION_CONFERENCE_START;

		for (uuid_list_t::const_iterator iter = selected_items.begin(),
										 end = selected_items.end();
			 iter != end; ++iter)
		{
			LLUUID item = *iter;

			LLFolderViewItem* folder_item =
				panelp->getRootFolder()->getItemByID(item);
			if (folder_item)
			{
				LLFolderViewEventListener* fve_listener =
					folder_item->getListener();
				if (fve_listener &&
					fve_listener->getInventoryType() ==
						LLInventoryType::IT_CATEGORY)
				{

					LLFolderBridge* bridge =
						(LLFolderBridge*)folder_item->getListener();
					if (!bridge) return true;

					LLViewerInventoryCategory* cat = bridge->getCategory();
					if (!cat) return true;

					name = cat->getName();
					LLUniqueBuddyCollector is_buddy;
					LLInventoryModel::cat_array_t cat_array;
					LLInventoryModel::item_array_t item_array;
					modelp->collectDescendentsIf(bridge->getUUID(),
												 cat_array,
												 item_array,
												 LLInventoryModel::EXCLUDE_TRASH,
												 is_buddy);
					S32 count = item_array.size();
					if (count > 0)
					{
						// Create the session
						gIMMgrp->setFloaterOpen(true);

						LLAvatarTracker& at = gAvatarTracker;
						LLUUID id;
						for (S32 i = 0; i < count; ++i)
						{
							id = item_array[i]->getCreatorUUID();
							if (at.isBuddyOnline(id))
							{
								members.emplace_back(id);
							}
						}
					}
				}
				else
				{
					LLFolderViewItem* folder_item =
						panelp->getRootFolder()->getItemByID(item);
					if (!folder_item) return true;

					LLFolderViewEventListener* listener =
						folder_item->getListener();
					if (listener &&
						listener->getInventoryType() ==
							LLInventoryType::IT_CALLINGCARD)
					{
						LLInventoryItem* itemp =
							gInventory.getItem(listener->getUUID());
						if (itemp)
						{
							LLUUID id = itemp->getCreatorUUID();
							if (gAvatarTracker.isBuddyOnline(id))
							{
								members.emplace_back(id);
							}
						}
					}
				}
			}
		}

		// The session_id is randomly generated UUID which will be replaced
		// later with a server side generated number

		if (name.empty())
		{
			name = llformat("Session %d", session_num++);
		}

		gIMMgrp->addSession(name, type, members[0], members);

		return true;
	}
};

class LLAttachObject : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel* panelp = mPtr;
		LLFolderView* folderp = panelp->getRootFolder();
		if (!folderp || !isAgentAvatarValid()) return true;

		uuid_list_t selected_items;
		folderp->getSelectionList(selected_items);
		LLUUID id = *selected_items.begin();

		std::string joint_name = userdata.asString();
		LLViewerJointAttachment* attachmentp = NULL;
		for (LLVOAvatar::attachment_map_t::iterator
				iter = gAgentAvatarp->mAttachmentPoints.begin();
			 iter != gAgentAvatarp->mAttachmentPoints.end(); )
		{
			LLVOAvatar::attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			std::string name = LLTrans::getString(attachment->getName());
			if (name == joint_name)
			{
				attachmentp = attachment;
				break;
			}
		}
		if (attachmentp == NULL)
		{
			return true;
		}

		LLViewerInventoryItem* item;
		item = (LLViewerInventoryItem*)gInventory.getItem(id);
		if (item &&
			gInventory.isObjectDescendentOf(id, gInventory.getRootFolderID()))
		{
			gAppearanceMgr.rezAttachment(item, attachmentp);
		}
		else if (item && item->isFinished())
		{
			// Must be in library. copy it to our inventory and put it on.
			LLPointer<LLInventoryCallback> cb =
				new LLRezAttachmentCallback(attachmentp);
			copy_inventory_item(item->getPermissions().getOwner(),
								item->getUUID(), LLUUID::null, std::string(),
								cb);
		}
		gFocusMgr.setKeyboardFocus(NULL);

		return true;
	}
};

class LLEnableUniversal : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		bool enable = regionp && regionp->bakesOnMeshEnabled();
		mPtr->findControl(userdata["control"].asString())->setValue(enable);
		return true;
	}
};

class LLEnableSettings : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool enable = gAgent.hasInventorySettings();
		mPtr->findControl(userdata["control"].asString())->setValue(enable);
		return true;
	}
};

void init_object_inventory_panel_actions(LLPanelInventory* panelp)
{
	(new LLDoToSelectedPanel())->registerListener(panelp,
												  "Inventory.DoToSelected");
}

void init_inventory_actions(LLFloaterInventory* floater)
{
	(new LLDoToSelectedFloater())->registerListener(floater,
													"Inventory.DoToSelected");
	(new LLCloseAllFoldersFloater())->registerListener(floater,
													   "Inventory.CloseAllFolders");
	(new LLHideEmptySystemFolders())->registerListener(floater,
													   "Inventory.HideEmptySystemFolders");
	(new LLHideMarketplaceFolder())->registerListener(floater,
													  "Inventory.HideMarketplaceFolder");
	(new LLHideCurrentOutfitFolder())->registerListener(floater,
														"Inventory.HideCurrentOutfitFolder");
	(new LLCheckSystemFolders())->registerListener(floater,
												   "Inventory.CheckSystemFolders");
	(new LLResyncCallingCards())->registerListener(floater,
												   "Inventory.ResyncCallingCards");
	(new LLMakeNewOutfit())->registerListener(floater,
											  "Inventory.MakeNewOutfit");
	(new LLEmptyTrashFloater())->registerListener(floater,
												  "Inventory.EmptyTrash");
	(new LLDoCreateFloater())->registerListener(floater,
												"Inventory.DoCreate");

	(new LLNewWindow())->registerListener(floater, "Inventory.NewWindow");
	(new LLShowFilters())->registerListener(floater, "Inventory.ShowFilters");
	(new LLResetFilter())->registerListener(floater, "Inventory.ResetFilter");
	(new LLSetSortBy())->registerListener(floater, "Inventory.SetSortBy");
	(new LLSetSearchType())->registerListener(floater,
											  "Inventory.SetSearchType");

	(new LLEnableUniversal())->registerListener(floater,
												"Inventory.EnableUniversal");
	(new LLEnableSettings())->registerListener(floater,
											   "Inventory.EnableSettings");
}

void init_inventory_panel_actions(LLInventoryPanel* panelp)
{
	(new LLDoToSelected())->registerListener(panelp, "Inventory.DoToSelected");
	(new LLAttachObject())->registerListener(panelp, "Inventory.AttachObject");
	(new LLCloseAllFolders())->registerListener(panelp,
												"Inventory.CloseAllFolders");
	(new LLEmptyTrash())->registerListener(panelp, "Inventory.EmptyTrash");
	(new LLEmptyLostAndFound())->registerListener(panelp,
												  "Inventory.EmptyLostAndFound");
	(new LLDoCreate())->registerListener(panelp, "Inventory.DoCreate");
	(new LLFileUploadLocation())->registerListener(panelp,
												   "Inventory.FileUploadLocation");
	(new LLBeginIMSession())->registerListener(panelp,
											   "Inventory.BeginIMSession");
}

void open_notecard(LLViewerInventoryItem* itemp, const std::string& title,
				   bool show_keep_discard, const LLUUID& object_id,
				   bool take_focus)
{
//MK
	if (gRLenabled && gRLInterface.contains("viewnote"))
	{
		return;
	}
//mk
	// See if we can bring an existing preview to the front
	if (itemp && !LLPreview::show(itemp->getUUID(), take_focus))
	{
		// There is none, so make a new preview
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("NotecardEditorRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewNotecard* previewp =
			new LLPreviewNotecard("preview notecard", rect, title,
								  itemp->getUUID(), object_id,
								  itemp->getAssetUUID(), show_keep_discard,
								  itemp);
		if (take_focus)
		{
			previewp->setFocus(true);
		}
		// Force to be entirely on screen.
		gFloaterViewp->adjustToFitScreen(previewp);
	}
}

void open_landmark(LLViewerInventoryItem* itemp, const std::string& title,
				   bool show_keep_discard, bool take_focus)
{
	// See if we can bring an exiting preview to the front
	if (itemp && !LLPreview::show(itemp->getUUID(), take_focus))
	{
		// There is none, so make a new preview
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewLandmarkRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);

		LLPreviewLandmark* previewp =
			new LLPreviewLandmark(title, rect, title, itemp->getUUID(),
								  show_keep_discard, itemp);
		if (take_focus)
		{
			previewp->setFocus(true);
		}
		// Force to be entirely on screen.
		gFloaterViewp->adjustToFitScreen(previewp);
	}
}

static bool open_landmark_callback(const LLSD& notification,
								   const LLSD& response)
{
	LLUUID asset_id = notification["payload"]["asset_id"].asUUID();
	LLUUID item_id = notification["payload"]["item_id"].asUUID();
	if (LLNotification::getSelectedOption(notification, response) == 0)	// YES
	{
		gAgent.teleportViaLandmark(asset_id);

		// We now automatically track the landmark you're teleporting to
		// because you will probably arrive at a fixed TP point instead.
		if (gFloaterWorldMapp)
		{
			// Remember this is the item UUID not the asset UUID
			gFloaterWorldMapp->trackLandmark(item_id);
		}
	}

	return false;
}
static LLNotificationFunctorRegistration open_landmark_callback_reg("TeleportFromLandmark",
																	open_landmark_callback);

void open_texture(const LLUUID& item_id, const std::string& title,
				  bool show_keep_discard, const LLUUID& object_id,
				  bool take_focus)
{
//MK
	if (gRLenabled && gRLInterface.contains("viewtexture"))
	{
		return;
	}
//mk
	// See if we can bring an exiting preview to the front
	if (!LLPreview::show(item_id, take_focus))
	{
		// There is none, so make a new preview
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewTexture* previewp = new LLPreviewTexture("preview texture",
														  rect, title, item_id,
														  object_id,
														  show_keep_discard);
		if (take_focus)
		{
			previewp->setFocus(true);
		}
		// Force to be entirely on screen.
		gFloaterViewp->adjustToFitScreen(previewp);
	}
}

void open_callingcard(LLViewerInventoryItem* itemp)
{
	LLUUID id = itemp ? itemp->getCreatorUUID() : LLUUID::null;
	if (id.isNull())
	{
		return;
	}
	if (id == gAgentID)
	{
		// If the calling card was created by us, then it is most probably a
		// v2 viewer force-re-created calling card; if only LL could mind their
		// own ass and let users the choice of what should be kept or not in
		// their inventory !... It would also avoid such invalid calling cards.
		// Try to extract the target avatar UUID from the description, if any.
		std::string desc = itemp->getActualDescription();
		id.set(desc, false);	// false = do not warn if invalid UUID
	}
	if (id.notNull())
	{
		bool online = id == gAgentID || gAvatarTracker.isBuddyOnline(id);
		LLFloaterAvatarInfo::showFromFriend(id, online);
	}
}

void open_sound(const LLUUID& item_id, const std::string& title,
				const LLUUID& object_id, bool take_focus)
{
	// See if we can bring an existing preview to the front
	if (LLPreview::show(item_id, take_focus))
	{
		return;
	}
	// There is none, so make a new preview
	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	LLRect rect = gSavedSettings.getRect("PreviewSoundRect");
	rect.translate(left - rect.mLeft, top - rect.mTop);
	LLPreviewSound* previewp = new LLPreviewSound("preview sound", rect, title,
												  item_id, object_id);
	if (take_focus)
	{
		previewp->setFocus(true);
	}
	// Force to be entirely on screen.
	gFloaterViewp->adjustToFitScreen(previewp);
}

void open_animation(const LLUUID& item_id, const std::string& title,
					S32 activate, const LLUUID& object_id, bool take_focus)
{
	// See if we can bring an existing preview to the front
	if (LLPreview::show(item_id, take_focus))
	{
		return;
	}
	// There is none, so make a new preview
	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	LLRect rect = gSavedSettings.getRect("PreviewAnimRect");
	rect.translate(left - rect.mLeft, top - rect.mTop);
	LLPreviewAnim* previewp = new LLPreviewAnim("preview anim", rect, title,
												 item_id, activate, object_id);
	if (take_focus)
	{
		previewp->setFocus(true);
	}
	// Force to be entirely on screen.
	gFloaterViewp->adjustToFitScreen(previewp);
}

void open_script(const LLUUID& item_id, const std::string& title,
				 bool take_focus)
{
//MK
	if (gRLenabled && gRLInterface.mContainsViewscript)
	{
		return;
	}
//mk
	// See if we can bring an existing preview to the front
	if (LLPreview::show(item_id, take_focus))
	{
		return;
	}
	// There is none, so make a new preview
	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	LLRect rect = gSavedSettings.getRect("PreviewScriptRect");
	rect.translate(left - rect.mLeft, top - rect.mTop);
	LLPreviewScript* previewp = new LLPreviewScript("preview script", rect,
													title, item_id);
	if (take_focus)
	{
		previewp->setFocus(true);
	}
	// Force to be entirely on screen.
	gFloaterViewp->adjustToFitScreen(previewp);
}

void open_gesture(const LLUUID& item_id, const std::string& title,
				  const LLUUID& object_id, bool take_focus)
{
	// See if we can bring an existing preview to the front
	if (LLPreview::show(item_id, take_focus))
	{
		return;
	}
	// There is none, so make a new preview
	// *TODO: save the rectangle
	LLPreviewGesture* previewp = LLPreviewGesture::show(title, item_id,
														object_id, take_focus);
	// Force to be entirely on screen.
	gFloaterViewp->adjustToFitScreen(previewp);
}

void open_material(const LLUUID& item_id, const std::string& title,
				   const LLUUID& object_id, bool take_focus)
{
	// See if we can bring an existing preview to the front
	if (LLPreview::show(item_id, take_focus))
	{
		return;
	}
	// There is none, so make a new preview
	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	LLRect rect = gSavedSettings.getRect("PreviewMaterialRect");
	rect.translate(left - rect.mLeft, top - rect.mTop);
	LLPreviewMaterial* previewp = new LLPreviewMaterial("preview material",
														rect, title, item_id,
														object_id);
	// Force to be entirely on screen.
	gFloaterViewp->adjustToFitScreen(previewp);
	if (take_focus)
	{
		previewp->setFocus(true);
	}
}
