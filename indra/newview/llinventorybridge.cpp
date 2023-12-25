/**
 * @file llinventorybridge.cpp
 * @brief Implementation of the Inventory-Folder-View-Bridge classes.
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

#include <utility> // for std::pair<>

#include "llinventorybridge.h"

#include "llaudioengine.h"
#include "llevent.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llradiogroup.h"
#include "llscrollcontainer.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llenvironment.h"
#include "llfloaterchat.h"
#include "llfloatercustomize.h"
#include "hbfloatereditenvsettings.h"
#include "llfloaterinventory.h"
#include "llfloatermarketplace.h"
#include "llfloateropenobject.h"
#include "llfloaterproperties.h"
#include "hbfloaterthumbnail.h"
#include "llfloaterworldmap.h"
#include "llgesturemgr.h"
#include "llinventoryactions.h"
#include "hbinventoryclipboard.h"
#include "llinventoryicon.h"
#include "llinventorymodelfetch.h"
#include "lllandmarklist.h"
#include "llmarketplacefunctions.h"
#include "llpreviewtexture.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltooldraganddrop.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewerfoldertype.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"						// For handle_object_edit()
#include "llviewermessage.h"					// For send_sound_trigger()
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#if LL_RESTORE_TO_WORLD
# include "llviewerregion.h"
#endif
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llwearablelist.h"

using namespace LLOldEvents;

// Helper functions

static std::string safe_inv_type_lookup(LLInventoryType::EType inv_type)
{
	std::string type = LLInventoryType::lookupHumanReadable(inv_type);
	if (!type.empty())
	{
		return type;
	}
	return "<" + LLTrans::getString("invalid") + ">";
}

#if LL_RESTORE_TO_WORLD
static bool restore_to_world_callback(const LLSD& notification,
									  const LLSD& response, LLItemBridge* self)
{
	if (self && LLNotification::getSelectedOption(notification, response) == 0)
	{
		self->restoreToWorld();
	}
	return false;
}
#endif

void set_menu_entries_state(LLMenuGL& menu,
							const std::vector<std::string>& entries_to_show,
							const std::vector<std::string>& disabled_entries)
{
	const LLView::child_list_t* list = menu.getChildList();
	for (LLView::child_list_t::const_iterator it = list->begin(),
											  end = list->end();
		 it != end; ++it)
	{
		std::string name = (*it)->getName();

		// Descend into split menus:
		LLMenuItemBranchGL* branchp = dynamic_cast<LLMenuItemBranchGL*>(*it);
		if (branchp && name == "More")
		{
			set_menu_entries_state(*branchp->getBranch(), entries_to_show,
								   disabled_entries);
		}

		bool found = false;
		for (S32 i = 0, count = entries_to_show.size(); i < count; ++i)
		{
			if (entries_to_show[i] == name)
			{
				found = true;
				break;
			}
		}
		if (found)
		{
			for (S32 i = 0, count = disabled_entries.size(); i < count; ++i)
			{
				if (disabled_entries[i] == name)
				{
					(*it)->setEnabled(false);
					break;
				}
			}
		}
		else
		{
			(*it)->setVisible(false);
		}
	}
}

//-----------------------------------------------------------------------------
// Class LLInventoryWearObserver
//
// Observer for "copy and wear" operation to support knowing when the all of
// the contents has been added to inventory.
//-----------------------------------------------------------------------------

class LLInventoryCopyAndWearObserver final : public LLInventoryObserver
{
public:
	LLInventoryCopyAndWearObserver(const LLUUID& cat_id, S32 count,
								   bool folder_added = false)
	:	mCatID(cat_id),
		mContentsCount(count),
		mFolderAdded(folder_added)
	{
	}

	void changed(U32 mask) override;

protected:
	LLUUID mCatID;
	S32    mContentsCount;
	bool   mFolderAdded;
};

void LLInventoryCopyAndWearObserver::changed(U32 mask)
{
	if ((mask & LLInventoryObserver::ADD) != 0)
	{
		if (!mFolderAdded)
		{
			const uuid_list_t& changed_items = gInventory.getChangedIDs();
			for (uuid_list_t::const_iterator it = changed_items.begin(),
											 end = changed_items.end();
				 it != end; ++it)
			{
				if (*it == mCatID)
				{
					mFolderAdded = true;
					break;
				}
			}
		}

		if (mFolderAdded)
		{
			LLViewerInventoryCategory* category = gInventory.getCategory(mCatID);
			if (!category)
			{
				llwarns << "Couldn't find category: " << mCatID  << llendl;
			}
			else if (category->getDescendentCount() == mContentsCount)
			{
				gInventory.removeObserver(this);
				gAppearanceMgr.wearInventoryCategory(category, false, true);
				delete this;
			}
		}
	}
}

typedef std::pair<LLUUID, LLUUID> two_uuids_t;
typedef std::list<two_uuids_t> two_uuids_list_t;
typedef std::pair<LLUUID, two_uuids_list_t> uuid_move_list_t;

struct LLMoveInv
{
	LLUUID				mObjectID;
	LLUUID				mCategoryID;
	two_uuids_list_t	mMoveList;
	void				(*mCallback)(S32, void*);
	void*				mUserData;
};

static bool move_task_inventory_callback(const LLSD& notification,
										 const LLSD& response,
										 LLMoveInv* move_inv)
{
	LLViewerObject* object = gObjectList.findObject(move_inv->mObjectID);
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (object && move_inv && option == 0)
	{
		LLFloaterOpenObject::LLCatAndWear* cat_and_wear =
			(LLFloaterOpenObject::LLCatAndWear*)move_inv->mUserData;
		if (cat_and_wear && cat_and_wear->mWear
#if 0
			&& !cat_and_wear->mFolderResponded
#endif
			)
		{
			LLInventoryObject::object_list_t inventory_objects;
			object->getInventoryContents(inventory_objects);
			// Subtract one for containing folder
			S32 contents_count = inventory_objects.size() - 1;

			LLInventoryCopyAndWearObserver* inv_observer =
				new LLInventoryCopyAndWearObserver(cat_and_wear->mCatID,
												   contents_count,
												   cat_and_wear->mFolderResponded);
			gInventory.addObserver(inv_observer);
		}

		for (two_uuids_list_t::iterator it = move_inv->mMoveList.begin(),
										end = move_inv->mMoveList.end();
			 it != end; ++it)
		{
			object->moveInventory(it->first, it->second);
		}

		// Update the UI.
		dialog_refresh_all();
	}

	if (move_inv->mCallback)
	{
		move_inv->mCallback(option, move_inv->mUserData);
	}
	delete move_inv;

	return false;
}

static void warn_move_inventory(LLViewerObject* object, LLMoveInv* move_inv)
{
	std::string dialog =
		object->flagScripted() ? "MoveInventoryFromScriptedObject"
							   : "MoveInventoryFromObject";
	gNotifications.add(dialog, LLSD(), LLSD(),
					   boost::bind(move_task_inventory_callback, _1, _2,
								   move_inv));
}

// Move/copy all inventory items from the Contents folder of an in-world
// object to the agent's inventory, inside a given category.
bool move_inv_category_world_to_agent(const LLUUID& object_id,
									  const LLUUID& category_id, bool drop,
									  void (*callback)(S32, void*),
									  void* user_data)
{
	// Make sure the object exists. If we allowed dragging from anonymous
	// objects, it would be possible to bypass permissions.
	// content category has same ID as object itself
	LLViewerObject* object = gObjectList.findObject(object_id);
	if (!object)
	{
		llinfos << "Object not found for drop." << llendl;
		return false;
	}

	// This folder is coming from an object, as there is only one folder in an
	// object, the root, we need to collect the entire contents and handle them
	// as a group
	LLInventoryObject::object_list_t inventory_objects;
	object->getInventoryContents(inventory_objects);

	if (inventory_objects.empty())
	{
		llinfos << "Object contents not found for drop." << llendl;
		return false;
	}

	bool accept = true;
	bool is_move = false;

	// Coming from a task. Need to figure out if the person can move/copy this
	// item.
	LLInventoryObject::object_list_t::iterator it = inventory_objects.begin();
	LLInventoryObject::object_list_t::iterator end = inventory_objects.end();
	for ( ; it != end; ++it)
	{
		LLInventoryItem* item = dynamic_cast<LLInventoryItem*>(it->get());
		if (!item)
		{
			llwarns << "Invalid inventory item for drop" << llendl;
			continue;
		}
		LLPermissions perm(item->getPermissions());
		if (perm.allowCopyBy(gAgentID, gAgent.getGroupID()) &&
			perm.allowTransferTo(gAgentID))	/* || gAgent.isGodlike()) */
		{
			accept = true;
		}
		else if (object->permYouOwner())
		{
			// If the object cannot be copied, but the object the inventory is
			// owned by the agent, then the item can be moved from the task to
			// agent inventory.
			is_move = accept = true;
		}
		else
		{
			accept = false;
			break;
		}
	}

	if (drop && accept)
	{
		it = inventory_objects.begin();
		LLMoveInv* move_inv = new LLMoveInv;
		move_inv->mObjectID = object_id;
		move_inv->mCategoryID = category_id;
		move_inv->mCallback = callback;
		move_inv->mUserData = user_data;

		for ( ; it != end; ++it)
		{
			move_inv->mMoveList.emplace_back(category_id, (*it)->getUUID());
		}

		if (is_move)
		{
			// Callback called from within here.
			warn_move_inventory(object, move_inv);
		}
		else
		{
			LLNotification::Params params("MoveInventoryFromObject");
			params.functor(boost::bind(move_task_inventory_callback,
									   _1, _2, move_inv));
			gNotifications.forceResponse(params, 0);
		}
	}
	return accept;
}

///////////////////////////////////////////////////////////////////////////////
// LLInvFVBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLInvFVBridge::getName() const
{
	LLInventoryObject* obj = getInventoryObject();
	return obj ? obj->getName() : LLStringUtil::null;
}

// Can it be destroyed (or moved to trash) ?
//virtual
bool LLInvFVBridge::isItemRemovable()
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp)
	{
		return false;
	}

	const LLViewerInventoryItem* itemp = modelp->getItem(mUUID);
	if (itemp && itemp->getIsLinkType())
	{
		return true;
	}

	return modelp->isObjectDescendentOf(mUUID, gInventory.getRootFolderID());
}

// Can it be moved to another folder ?
//virtual
bool LLInvFVBridge::isItemMovable()
{
	bool can_move = false;
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (model)
	{
		can_move = model->isObjectDescendentOf(mUUID,
											   gInventory.getRootFolderID());
	}
	return can_move;
}

//virtual
void LLInvFVBridge::showProperties()
{
	LLFloaterProperties::show(mUUID, LLUUID::null, mInventoryPanel);
}

//virtual
void LLInvFVBridge::removeBatch(std::vector<LLFolderViewEventListener*>& batch)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return;

	// Deactivate gestures and close settings editors when moving them into the
	// Trash.
	S32 count = batch.size();
	for (S32 i = 0; i < count; ++i)
	{
		LLInvFVBridge* bridge = (LLInvFVBridge*)batch[i];
		if (!bridge) continue;	// Paranoia
		if (bridge->isItemRemovable())
		{
			LLViewerInventoryItem* item = model->getItem(bridge->getUUID());
			if (item && item->getType() == LLAssetType::AT_GESTURE)
			{
				gGestureManager.deactivateGesture(item->getUUID());
			}
		}
		else if (!bridge->isMultiPreviewAllowed())
		{
			LLViewerInventoryItem* item = model->getItem(bridge->getUUID());
			if (item && item->getType() == LLAssetType::AT_SETTINGS)
			{
				HBFloaterEditEnvSettings::destroy(item->getUUID());
			}
		}
	}
	for (S32 i = 0; i < count; ++i)
	{
		LLInvFVBridge* bridge = (LLInvFVBridge*)batch[i];
		if (!bridge || !bridge->isItemRemovable()) continue;
		LLViewerInventoryCategory* cat = model->getCategory(bridge->getUUID());
		if (cat)
		{
			LLInventoryModel::cat_array_t descendent_categories;
			LLInventoryModel::item_array_t descendent_items;
			model->collectDescendents(cat->getUUID(), descendent_categories,
									  descendent_items, false);
			for (S32 j = 0, count2 = descendent_items.size(); j < count2; ++j)
			{
				LLViewerInventoryItem* item = descendent_items[j];
				if (!item) continue;	// Paranoia

				LLPreview::hide(item->getUUID());
				if (item->getType() == LLAssetType::AT_GESTURE)
				{
					gGestureManager.deactivateGesture(item->getUUID());
				}
				else if (item->getType() == LLAssetType::AT_SETTINGS)
				{
					HBFloaterEditEnvSettings::destroy(item->getUUID());
				}
			}
		}
	}

	removeBatchNoCheck(batch);
}

// This method moves a bunch of items and folders to the trash. As per design
// guidelines for the inventory model, the message is built and the accounting
// is performed first. Once done, we call LLInventoryModel::moveObject() to
// move everything around.
void LLInvFVBridge::removeBatchNoCheck(std::vector<LLFolderViewEventListener*>& batch)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	LLMessageSystem* msg = gMessageSystemp;
	if (!model || !msg) return;

	const LLUUID& trash_id = model->getTrashID();
	uuid_vec_t move_ids;
	LLInventoryModel::update_map_t update;

	bool start_new_message = true;
	S32 count = batch.size();
	for (S32 i = 0; i < count; ++i)
	{
		LLInvFVBridge* bridge = (LLInvFVBridge*)batch[i];
		if (!bridge || !bridge->isItemRemovable())
		{
			continue;
		}

		LLViewerInventoryItem* item = model->getItem(bridge->getUUID());
		if (!item || item->getParentUUID() == trash_id)
		{
			continue;
		}

		move_ids.emplace_back(item->getUUID());
		--update[item->getParentUUID()];
		++update[trash_id];

		if (start_new_message)
		{
			start_new_message = false;
			msg->newMessageFast(_PREHASH_MoveInventoryItem);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->addBoolFast(_PREHASH_Stamp, true);
		}
		msg->nextBlockFast(_PREHASH_InventoryData);
		msg->addUUIDFast(_PREHASH_ItemID, item->getUUID());
		msg->addUUIDFast(_PREHASH_FolderID, trash_id);
		msg->addString("NewName", NULL);
		if (msg->isSendFullFast(_PREHASH_InventoryData))
		{
			start_new_message = true;
			gAgent.sendReliableMessage();
			model->accountForUpdate(update);
			update.clear();
			// Move everything. Note: this does need to be done after each
			// message is sent to avoid loosing accounting sync with the
			// server !  HB
			for (U32 j = 0, items = move_ids.size(); j < items; ++j)
			{
				const LLUUID& item_id = move_ids[j];
				model->moveObject(item_id, trash_id);
				LLViewerInventoryItem* item = model->getItem(item_id);
				if (item)
				{
					model->updateItem(item);
				}
			}
			move_ids.clear();
		}
	}
	if (!start_new_message)
	{
		start_new_message = true;
		gAgent.sendReliableMessage();
		model->accountForUpdate(update);
		update.clear();
		// Move everything. Note: this does need to be done after each message
		// is sent to avoid loosing accounting sync with the server !  HB
		for (U32 j = 0, items = move_ids.size(); j < items; ++j)
		{
			const LLUUID& item_id = move_ids[j];
			model->moveObject(item_id, trash_id);
			LLViewerInventoryItem* item = model->getItem(item_id);
			if (item)
			{
				model->updateItem(item);
			}
		}
		move_ids.clear();
	}

	for (S32 i = 0; i < count; ++i)
	{
		LLInvFVBridge* bridge = (LLInvFVBridge*)batch[i];
		if (!bridge || !bridge->isItemRemovable())
		{
			continue;
		}

		LLViewerInventoryCategory* cat = model->getCategory(bridge->getUUID());
		if (!cat || cat->getParentUUID() == trash_id)
		{
			continue;
		}

		move_ids.emplace_back(cat->getUUID());
		--update[cat->getParentUUID()];
		++update[trash_id];

		if (start_new_message)
		{
			start_new_message = false;
			msg->newMessageFast(_PREHASH_MoveInventoryFolder);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->addBool("Stamp", true);
		}
		msg->nextBlockFast(_PREHASH_InventoryData);
		msg->addUUIDFast(_PREHASH_FolderID, cat->getUUID());
		msg->addUUIDFast(_PREHASH_ParentID, trash_id);
		if (msg->isSendFullFast(_PREHASH_InventoryData) )
		{
			start_new_message = true;
			gAgent.sendReliableMessage();
			model->accountForUpdate(update);
			update.clear();
			// Move everything. Note: this does need to be done after each
			// message is sent to avoid loosing accounting sync with the
			// server !  HB
			for (U32 j = 0, items = move_ids.size(); j < items; ++j)
			{
				const LLUUID& item_id = move_ids[j];
				model->moveObject(item_id, trash_id);
				LLViewerInventoryItem* item = model->getItem(item_id);
				if (item)
				{
					model->updateItem(item);
				}
			}
			move_ids.clear();
		}
	}
	if (!start_new_message)
	{
		gAgent.sendReliableMessage();
		model->accountForUpdate(update);
		// Move everything. Note: this does need to be done after each message
		// is sent to avoid loosing accounting sync with the server !  HB
		for (U32 j = 0, items = move_ids.size(); j < items; ++j)
		{
			const LLUUID& item_id = move_ids[j];
			model->moveObject(item_id, trash_id);
			LLViewerInventoryItem* item = model->getItem(item_id);
			if (item)
			{
				model->updateItem(item);
			}
		}
	}

	// Notify inventory observers.
	model->notifyObservers();
}

//virtual
bool LLInvFVBridge::isClipboardPasteable() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp)
	{
		modelp = &gInventory;
	}
	if (!isAgentInventory() || !HBInventoryClipboard::hasContents())
	{
		return false;
	}

	const LLUUID& agent_id = gAgentID;
	uuid_vec_t objects;
	HBInventoryClipboard::retrieve(objects);
	S32 count = objects.size();
	for (S32 i = 0; i < count; ++i)
	{
		const LLUUID& object_id = objects[i];

		const LLViewerInventoryCategory* catp = modelp->getCategory(object_id);
		if (catp)
		{
			if (catp->getPreferredType() != LLFolderType::FT_NONE)
			{
				// Do not allow to copy any special folder
				return false;
			}

			LLFolderBridge cat_br(mInventoryPanel, object_id);
			if (!cat_br.isItemCopyable())
			{
				return false;
			}
		}
		else
		{
			const LLViewerInventoryItem* itemp = modelp->getItem(object_id);
			if (!itemp || !itemp->getPermissions().allowCopyBy(agent_id))
			{
				return false;
			}
		}
	}
//MK
	if (gRLenabled)
	{
		// Do not allow if either the destination folder or the source folder
		// is locked
		LLViewerInventoryCategory* catp = modelp->getCategory(mUUID);
		if (catp)
		{
			for (S32 i = objects.size() - 1; i >= 0; --i)
			{
				const LLUUID& obj_id = objects[i];
				if (gRLInterface.isFolderLocked(catp) ||
					gRLInterface.isFolderLocked(modelp->getCategory(modelp->getObject(obj_id)->getParentUUID())))
				{
					return false;
				}
			}
			HBInventoryClipboard::retrieveCuts(objects);
			count = objects.size();
			for (S32 i = objects.size() - 1; i >= 0; --i)
			{
				const LLUUID& obj_id = objects[i];
				if (gRLInterface.isFolderLocked(catp) ||
					gRLInterface.isFolderLocked(modelp->getCategory(modelp->getObject(obj_id)->getParentUUID())))
				{
					return false;
				}
			}
		}
	}
//mk
	return true;
}

//virtual
bool LLInvFVBridge::isClipboardPasteableAsLink() const
{
	if (!HBInventoryClipboard::hasCopiedContents() ||
		!isAgentInventory())
	{
		return false;
	}
	const LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	const LLUUID& root_id = gInventory.getRootFolderID();
	uuid_vec_t objects;
	HBInventoryClipboard::retrieve(objects);
	S32 count = objects.size();
	for (S32 i = 0; i < count; ++i)
	{
		const LLUUID& object_id = objects[i];

		if (!model->isObjectDescendentOf(object_id, root_id))
		{
			return false;
		}

		const LLViewerInventoryItem* item = model->getItem(object_id);
		if (item && !LLAssetType::lookupCanLink(item->getActualType()))
		{
			return false;
		}

		const LLViewerInventoryCategory* cat = model->getCategory(object_id);
#if 0	// We do not support pasting folders as links (it is useless anyway...)
		if (cat && cat->isProtected())
#else
		if (cat)
#endif
		{
			return false;
		}
	}
//MK
	if (gRLenabled)
	{
		// Do not allow if either the destination folder or the source folder
		// is locked
		LLViewerInventoryCategory* current_cat = model->getCategory(mUUID);
		if (current_cat)
		{
			for (S32 i = count - 1; i >= 0; --i)
			{
				const LLUUID& obj_id = objects[i];
				const LLUUID& parent_id =
					model->getObject(obj_id)->getParentUUID();
				if (gRLInterface.isFolderLocked(current_cat) ||
					gRLInterface.isFolderLocked(model->getCategory(parent_id)))
				{
					return false;
				}
			}
		}
	}
//mk
	return true;
}

//virtual
bool LLInvFVBridge::cutToClipboard() const
{
	HBInventoryClipboard::addCut(mUUID);
	return true;
}

// Generic method for commonly-used entries
void LLInvFVBridge::getClipboardEntries(bool show_asset_id,
										std::vector<std::string>& items,
										std::vector<std::string>& disabled_items,
										U32 flags)
{
	bool not_first_selected_item = (flags & FIRST_SELECTED_ITEM) == 0;
	bool agent_inventory = isAgentInventory();

	const LLInventoryObject* invobjp = getInventoryObject();
	if (invobjp)
	{
		bool need_separator = false;
		if (invobjp->getIsLinkType())
		{
			items.emplace_back("Find Original");
			if (isLinkedObjectMissing())
			{
				disabled_items.emplace_back("Find Original");
			}
			need_separator = true;
		}
		else
		{
			if (agent_inventory)
			{
				// Add thumbnail support, when using the AIS3 fetching. HB
				if (LLInventoryModelFetch::useAISFetching())
				{
					items.emplace_back("Thumbnail");
				}
				items.emplace_back("Rename");
				if (not_first_selected_item || !isItemRenameable())
				{
					disabled_items.emplace_back("Rename");
				}
				need_separator = true;
			}

			if (show_asset_id)
			{
				items.emplace_back("Copy Asset UUID");
				if (not_first_selected_item ||
					!(isItemPermissive() || gAgent.isGodlike()))
				{
					disabled_items.emplace_back("Copy Asset UUID");
				}
				need_separator = true;
			}

		}

		if (need_separator)
		{
			items.emplace_back("Copy Separator");
		}
		items.emplace_back("Copy");
		if (!isItemCopyable())
		{
			disabled_items.emplace_back("Copy");
		}
	}

	if (agent_inventory)
	{
		items.emplace_back("Cut");
		if (!isItemMovable())
		{
			disabled_items.emplace_back("Cut");
		}
	}

	if (!isInCOF() && agent_inventory)
	{
		items.emplace_back("Paste");
		if (not_first_selected_item || !isClipboardPasteable())
		{
			disabled_items.emplace_back("Paste");
		}

		if (!isInMarketplace())
		{
			items.emplace_back("Paste As Link");
			if (not_first_selected_item || !isClipboardPasteableAsLink())
			{
				disabled_items.emplace_back("Paste As Link");
			}
		}
	}

	if (agent_inventory)
	{
		uuid_vec_t selected_items;
		mInventoryPanel->getRootFolder()->getSelection(selected_items);
		if (movable_objects_with_same_parent(selected_items))
		{
			items.emplace_back("Move In New Folder");
		}
		// If this is the context menu for a folder and only one folder is
		// selected, and that folder got children, and it is not unique, then
		// add the option to extract all the children from that folder.
		if ((flags & ITEM_IN_MULTI_SELECTION) == 0 && hasChildren())
		{
			const LLViewerInventoryCategory* cat =
				invobjp->asViewerInventoryCategory();
			if (cat && !cat->isUnique())
			{
				items.emplace_back("Extract From Folder");
			}
		}

		items.emplace_back("Paste Separator");

		items.emplace_back("Delete");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Delete");
		}
	}
}

//virtual
void LLInvFVBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	if (isInTrash())
	{
		const LLInventoryObject* invobjp = getInventoryObject();
		if (invobjp && invobjp->getIsLinkType())
		{
			items.emplace_back("Find Original");
			if (isLinkedObjectMissing())
			{
				disabled_items.emplace_back("Find Original");
			}
		}
		items.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Purge Item");
		}
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
bool LLInvFVBridge::startDrag(EDragAndDropType* type, LLUUID* id) const
{
	const LLInventoryObject* invobjp = getInventoryObject();
	if (invobjp)
	{
		*type = LLAssetType::lookupDragAndDropType(invobjp->getActualType());
		if (*type == DAD_NONE)
		{
			return false;
		}

		const LLUUID& obj_id = invobjp->getUUID();
		*id = obj_id;

		if (*type == DAD_CATEGORY)
		{
			LLInventoryModelFetch::getInstance()->start(obj_id);
		}

		return true;
	}
	return false;
}

LLInventoryObject* LLInvFVBridge::getInventoryObject() const
{
	LLInventoryObject* invobjp = NULL;
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (modelp)
	{
		invobjp = (LLInventoryObject*)modelp->getObject(mUUID);
	}
	return invobjp;
}

bool LLInvFVBridge::isInTrash() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	return modelp && modelp->isInTrash(mUUID);
}

bool LLInvFVBridge::isInLostAndFound() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	return modelp &&
		   modelp->isObjectDescendentOf(mUUID, modelp->getLostAndFoundID());
}

bool LLInvFVBridge::isInCOF() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	return modelp && modelp->isInCOF(mUUID);
}

bool LLInvFVBridge::isInMarketplace() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	return modelp && modelp->isInMarketPlace(mUUID);
}

bool LLInvFVBridge::isLinkedObjectInTrash() const
{
	if (isInTrash())
	{
		return true;
	}
	const LLInventoryObject* invobjp = getInventoryObject();
	if (invobjp && invobjp->getIsLinkType())
	{
		LLInventoryModel* modelp = mInventoryPanel->getModel();
		return modelp && modelp->isInTrash(invobjp->getLinkedUUID());
	}
	return false;
}

bool LLInvFVBridge::isLinkedObjectMissing() const
{
	const LLInventoryObject* invobjp = getInventoryObject();
	return !invobjp ||
		   (invobjp->getIsLinkType() &&
			LLAssetType::lookupIsLinkType(invobjp->getType()));
}

bool LLInvFVBridge::isAgentInventory() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp)
	{
		return false;
	}
	if (gInventory.getRootFolderID() == mUUID)
	{
		return true;
	}
	return modelp->isObjectDescendentOf(mUUID, gInventory.getRootFolderID());
}

//static
void LLInvFVBridge::changeItemParent(LLInventoryModel* modelp,
									 LLViewerInventoryItem* vitemp,
									 const LLUUID& new_parent_id,
									 bool restamp)
{
	if (modelp && vitemp && vitemp->getParentUUID() != new_parent_id)
	{
//MK
		if (gRLenabled)
		{
			LLViewerInventoryCategory* cat_parentp =
				modelp->getCategory(vitemp->getParentUUID());
			LLViewerInventoryCategory* cat_new_parentp =
				modelp->getCategory(new_parent_id);
			// We can move this category if we are moving it from a non shared
			// folder to another one, even if both folders are locked
			if ((gRLInterface.isUnderRlvShare(cat_parentp) ||
				 gRLInterface.isUnderRlvShare(cat_new_parentp)) &&
				(gRLInterface.isFolderLocked(cat_parentp) ||
				 gRLInterface.isFolderLocked(cat_new_parentp)))
			{
				return;
			}
		}
//mk
		modelp->changeItemParent(vitemp, new_parent_id, restamp);
	}
}

//static
void LLInvFVBridge::changeCategoryParent(LLInventoryModel* modelp,
										 LLViewerInventoryCategory* vcatp,
										 const LLUUID& new_parent_id,
										 bool restamp)
{
	if (modelp && vcatp &&
		!modelp->isObjectDescendentOf(new_parent_id, vcatp->getUUID()))
	{
//MK
		if (gRLenabled)
		{
			LLViewerInventoryCategory* cat_new_parentp =
				modelp->getCategory(new_parent_id);
			// We can move this category if we are moving it from a non shared
			// folder to another one, even if both folders are locked
			if ((gRLInterface.isUnderRlvShare(vcatp) ||
				 gRLInterface.isUnderRlvShare(cat_new_parentp)) &&
				(gRLInterface.isFolderLocked(vcatp) ||
				 gRLInterface.isFolderLocked(cat_new_parentp)))
			{
				return;
			}
		}
//mk
		modelp->changeCategoryParent(vcatp, new_parent_id, restamp);
	}
}

//static
LLInvFVBridge* LLInvFVBridge::createBridge(LLAssetType::EType asset_type,
										   LLAssetType::EType actual_asset_type,
										   LLInventoryType::EType inv_type,
										   LLInventoryPanel* panelp,
										   const LLUUID& uuid,
										   U32 flags)
{
	static LLUUID last_uuid;
	bool warn = false;
	S32 sub_type = -1;
	LLInvFVBridge* self = NULL;

	switch (asset_type)
	{
	case LLAssetType::AT_TEXTURE:
		if (inv_type != LLInventoryType::IT_TEXTURE &&
			inv_type != LLInventoryType::IT_SNAPSHOT)
		{
			warn = true;
		}
		self = new LLTextureBridge(panelp, uuid, inv_type);
		break;

	case LLAssetType::AT_SOUND:
		if (inv_type != LLInventoryType::IT_SOUND)
		{
			warn = true;
		}
		self = new LLSoundBridge(panelp, uuid);
		break;

	case LLAssetType::AT_LANDMARK:
		if (inv_type != LLInventoryType::IT_LANDMARK)
		{
			warn = true;
		}
		self = new LLLandmarkBridge(panelp, uuid, flags);
		break;

	case LLAssetType::AT_CALLINGCARD:
		if (inv_type != LLInventoryType::IT_CALLINGCARD)
		{
			warn = true;
		}
		self = new LLCallingCardBridge(panelp, uuid);
		break;

	case LLAssetType::AT_SCRIPT:
		if (inv_type != LLInventoryType::IT_LSL)
		{
			warn = true;
		}
		self = new LLScriptBridge(panelp, uuid);
		break;

	case LLAssetType::AT_OBJECT:
		if (inv_type != LLInventoryType::IT_OBJECT &&
			inv_type != LLInventoryType::IT_ATTACHMENT)
		{
			warn = true;
		}
		self = new LLObjectBridge(panelp, uuid, inv_type, flags);
		break;

	case LLAssetType::AT_NOTECARD:
		if (inv_type != LLInventoryType::IT_NOTECARD)
		{
			warn = true;
		}
		self = new LLNotecardBridge(panelp, uuid);
		break;

	case LLAssetType::AT_ANIMATION:
		if (inv_type != LLInventoryType::IT_ANIMATION)
		{
			warn = true;
		}
		self = new LLAnimationBridge(panelp, uuid);
		break;

	case LLAssetType::AT_GESTURE:
		if (inv_type != LLInventoryType::IT_GESTURE)
		{
			warn = true;
		}
		self = new LLGestureBridge(panelp, uuid);
		break;

	case LLAssetType::AT_LSL_TEXT:
		if (inv_type != LLInventoryType::IT_LSL)
		{
			warn = true;
		}
		self = new LLLSLTextBridge(panelp, uuid);
		break;

	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_BODYPART:
		sub_type = flags & LLInventoryItem::II_FLAGS_SUBTYPE_MASK;
		if (inv_type != LLInventoryType::IT_WEARABLE)
		{
			warn = true;
		}
		self = new LLWearableBridge(panelp, uuid, asset_type, inv_type,
									(LLWearableType::EType)sub_type);
		break;

	case LLAssetType::AT_CATEGORY:
		if (actual_asset_type == LLAssetType::AT_LINK_FOLDER)
		{
			// Create a link folder handler instead.
			self = new LLLinkFolderBridge(panelp, uuid);
			break;
		}
		self = new LLFolderBridge(panelp, uuid);
		break;

	case LLAssetType::AT_LINK:
	case LLAssetType::AT_LINK_FOLDER:
		// Only should happen for broken links.
		self = new LLLinkItemBridge(panelp, uuid);
		break;

#if LL_MESH_ASSET_SUPPORT
	case LLAssetType::AT_MESH:
		if (inv_type != LLInventoryType::IT_MESH)
		{
			warn = true;
		}
		self = new LLMeshBridge(panelp, uuid);
		break;
#endif

	case LLAssetType::AT_SETTINGS:
		sub_type = flags & LLInventoryItem::II_FLAGS_SUBTYPE_MASK;
		if (inv_type != LLInventoryType::IT_SETTINGS)
		{
			warn = true;
		}
		self = new LLSettingsBridge(panelp, uuid, sub_type);
		break;

	case LLAssetType::AT_MATERIAL:
		if (inv_type != LLInventoryType::IT_MATERIAL)
		{
			warn = true;
		}
		self = new LLMaterialBridge(panelp, uuid);
		break;

	default:
		llwarns_once << "Unhandled asset type: " << (S32)asset_type << llendl;
	}

	if (warn && uuid != last_uuid)
	{
		last_uuid = uuid;
		llwarns << LLAssetType::lookup(asset_type)
				<< " asset has inventory type "
				<< safe_inv_type_lookup(inv_type)
				<< " on uuid " << uuid << llendl;
	}

	if (self)
	{
		self->mInvType = inv_type;
		self->mSubType = sub_type;
	}

	return self;
}

void LLInvFVBridge::purgeItem(LLInventoryModel* modelp, const LLUUID& id)
{
	LLInventoryObject* invobjp = modelp ? modelp->getObject(id) : NULL;
	if (invobjp && id.notNull())
	{
		remove_inventory_object(id);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLItemBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
void LLItemBridge::performAction(LLFolderView* folderp,
								 LLInventoryModel* modelp,
								 const std::string& action)
{
	if (action == "goto")
	{
		gotoItem(folderp);
	}
	else if (action == "open")
	{
		openItem();
	}
	else if (action == "properties")
	{
		showProperties();
	}
	else if (action == "purge")
	{
		purgeItem(modelp, mUUID);
	}
#if LL_RESTORE_TO_WORLD
	else if (action == "restoreToWorld")
	{
		gNotifications.add("ObjectRestoreToWorld", LLSD(), LLSD(),
						   boost::bind(restore_to_world_callback, _1, _2,
									   this));
	}
#endif
	else if (action == "restore")
	{
		restoreItem();
	}
	else if (action == "thumbnail")
	{
		HBFloaterThumbnail::showInstance(mUUID);
	}
	else if (action == "copy_uuid")
	{
		// Single item only
		HBInventoryClipboard::storeAsset(modelp->getItem(mUUID));
	}
	else if (action == "paste_link")
	{
		// Single item only
		LLViewerInventoryItem* vitemp = modelp->getItem(mUUID);
		if (vitemp)
		{
			LLFolderViewItem* fvitemp =
				folderp->getItemByID(vitemp->getParentUUID());
			if (fvitemp)
			{
				fvitemp->getListener()->pasteLinkFromClipboard();
			}
		}
	}
	else if (action == "marketplace_edit_listing")
	{
		LLMarketplace::editListing(mUUID);
	}
}

//virtual
void LLItemBridge::selectItem()
{
	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp && !vitemp->isFinished())
	{
		vitemp->fetchFromServer();
	}
}

//virtual
void LLItemBridge::restoreItem()
{
	LLViewerInventoryItem* vitemp = getItem();
	if (!vitemp)
	{
		return;
	}

	LLInventoryModel* modelp = mInventoryPanel->getModel();
	const LLUUID& new_parent =
		modelp->findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(vitemp->getType()));
	// 'false' -> Do not restamp on restore.
	changeItemParent(modelp, vitemp, new_parent, false);
}

#if LL_RESTORE_TO_WORLD
//virtual
void LLItemBridge::restoreToWorld()
{
	if (!gAgent.getRegion()) return;

	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage("RezRestoreToWorld");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

		msg->nextBlockFast(_PREHASH_InventoryData);
		vitemp->packMessage(msg);
		msg->sendReliable(gAgent.getRegionHost());
	}

	// Similar functionality to the drag and drop rez logic
	bool remove_from_inventory = false;

	// Remove local inventory copy, sim will deal with permissions and removing
	// the item from the actual inventory if its a no-copy etc
	if (!vitemp->getPermissions().allowCopyBy(gAgentID))
	{
		remove_from_inventory = true;
	}

	// Check if it is in the trash (again similar to the normal rez logic).
	if (gInventory.isInTrash(vitemp->getUUID()))
	{
		remove_from_inventory = true;
	}

	if (remove_from_inventory)
	{
		gInventory.deleteObject(itemp->getUUID());
		gInventory.notifyObservers();
	}
}
#endif

//virtual
void LLItemBridge::gotoItem(LLFolderView*)
{
	LLInventoryObject* invobjp = getInventoryObject();
	if (invobjp && invobjp->getIsLinkType())
	{
		LLFloaterInventory* floaterp = LLFloaterInventory::getActiveFloater();
		if (floaterp)
		{
			floaterp->getPanel()->setSelection(invobjp->getLinkedUUID(),
											   TAKE_FOCUS_NO);
		}
	}
}

//virtual
LLUIImagePtr LLItemBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLInventoryType::ICONNAME_OBJECT);
}

//virtual
PermissionMask LLItemBridge::getPermissionMask() const
{
	PermissionMask perm_mask = 0;
	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp)
	{
		const LLPermissions& perms = vitemp->getPermissions();
		if (perms.allowCopyBy(gAgentID))
		{
			perm_mask |= PERM_COPY;
		}
		if (perms.allowModifyBy(gAgentID))
		{
			perm_mask |= PERM_MODIFY;
		}
		if (perms.allowTransferBy(gAgentID))
		{
			perm_mask |= PERM_TRANSFER;
		}
	}
	return perm_mask;
}

//virtual
const std::string& LLItemBridge::getDisplayName() const
{
	if (mDisplayName.empty())
	{
		buildDisplayName(getItem(), mDisplayName);
	}
	return mDisplayName;
}

//virtual
void LLItemBridge::buildDisplayName(LLInventoryItem* itemp, std::string& name)
{
	if (itemp)
	{
		name = itemp->getName();
	}
	else
	{
		name.clear();
	}
}

//virtual
std::string LLItemBridge::getLabelSuffix() const
{
	static const std::string link = " (" + LLTrans::getString("link") + ")";
	static const std::string broken = " (" + LLTrans::getString("brokenlink") +
									  ")";
	static const std::string nocopy = " (" + LLTrans::getString("nocopy") + ")";
	static const std::string nomod = " (" + LLTrans::getString("nomod") + ")";
	static const std::string noxfr = " (" + LLTrans::getString("noxfr") + ")";

	std::string suffix;
	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp)
	{
		if (LLAssetType::lookupIsLinkType(vitemp->getType()))
		{
			return broken;
		}
		if (vitemp->getIsLinkType())
		{
			return link;
		}
		// It is a bit confusing to list permissions for calling cards.
		if (vitemp->getType() != LLAssetType::AT_CALLINGCARD)
		{
			const LLPermissions& perms = vitemp->getPermissions();
			if (perms.getOwner() == gAgentID)
			{
				if (!perms.allowCopyBy(gAgentID))
				{
					suffix += nocopy;
				}
				if (!perms.allowModifyBy(gAgentID))
				{
					suffix += nomod;
				}
				if (!perms.allowTransferBy(gAgentID))
				{
					suffix += noxfr;
				}
			}
		}
	}
	return suffix;
}

//virtual
time_t LLItemBridge::getCreationDate() const
{
	LLViewerInventoryItem* vitemp = getItem();
	return vitemp ? vitemp->getCreationDate() : 0;
}

//virtual
bool LLItemBridge::isItemRenameable() const
{
	LLViewerInventoryItem* vitemp = getItem();
	return vitemp && vitemp->getPermissions().allowModifyBy(gAgentID);
}

//virtual
bool LLItemBridge::renameItem(const std::string& new_name)
{
	if (!isItemRenameable()) return false;

	LLPreview::rename(mUUID, getPrefix() + new_name);
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model) return false;

	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp && vitemp->getName() != new_name)
	{
		LLSD updates;
		updates["name"] = new_name;
		update_inventory_item(vitemp->getUUID(), updates);
	}

	// Return false because we either notified observers (and therefore
	// rebuilt) or we didn't update.
	return false;
}

//virtual
bool LLItemBridge::removeItem()
{
	if (!isItemRemovable())
	{
		return false;
	}
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp)
	{
		return false;
	}

	const LLUUID& trash_id = modelp->getTrashID();
	LLViewerInventoryItem* vitemp = getItem();
	// If item is not already in trash
	if (vitemp && !modelp->isObjectDescendentOf(mUUID, trash_id))
	{
		// Move to trash, and restamp
		changeItemParent(modelp, vitemp, trash_id, true);
		// Delete was successful
		return true;
	}

	// Tried to delete already item in trash (should purge ?)
	return false;
}

//virtual
bool LLItemBridge::isItemCopyable() const
{
	LLViewerInventoryItem* vitemp = getItem();
	// All non-links can be copied (at least as a link), and non-broken links
	// can get their linked object copied too (at least as a link as well).
	return vitemp && (!vitemp->getIsLinkType() || !isLinkedObjectMissing());
}

//virtual
bool LLItemBridge::copyToClipboard() const
{
	if (isItemCopyable())
	{
		LLViewerInventoryItem* vitemp = getItem();
		if (!vitemp || (vitemp->getIsLinkType() && isLinkedObjectMissing()))
		{
			// No item (paranoia, should not happen), or broken link: abort !
			return false;
		}
		HBInventoryClipboard::add(vitemp->getLinkedUUID());
		return true;
	}
	return false;
}

//virtual
LLViewerInventoryItem* LLItemBridge::getItem() const
{
	LLViewerInventoryItem* vitemp = NULL;
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (modelp)
	{
		vitemp = modelp->getItem(mUUID);
	}
	return vitemp;
}

//virtual
const LLUUID& LLItemBridge::getThumbnailUUID() const
{
	LLViewerInventoryItem* vitemp = NULL;
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (modelp)
	{
		vitemp = modelp->getItem(mUUID);
	}
	return vitemp ? vitemp->getThumbnailUUID() : LLUUID::null;
}

//virtual
bool LLItemBridge::isItemPermissive() const
{
	LLViewerInventoryItem* vitemp = getItem();
	return vitemp && vitemp->getPermissions().unrestricted();
}

///////////////////////////////////////////////////////////////////////////////
// LLFolderBridge
///////////////////////////////////////////////////////////////////////////////

LLFolderBridge* LLFolderBridge::sSelf = NULL;

//virtual
LLFolderBridge::~LLFolderBridge()
{
	if (sSelf == this)
	{
		sSelf = NULL;
	}
}

// Can it be moved to another folder ?
//virtual
bool LLFolderBridge::isItemMovable()
{
	bool can_move = false;
	LLInventoryObject* obj = getInventoryObject();
	if (obj)
	{
		LLViewerInventoryCategory* cat = (LLViewerInventoryCategory*)obj;
		can_move = !cat->isProtected();
	}
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (model && can_move)
	{
		can_move = model->isObjectDescendentOf(mUUID,
											   model->getRootFolderID());
	}
	return can_move;
}

// Can it be destroyed (or moved to trash) ?
//virtual
bool LLFolderBridge::isItemRemovable()
{
	const LLUUID& root_id = gInventory.getRootFolderID();
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp || !modelp->isObjectDescendentOf(mUUID, root_id))
	{
		return false;
	}

	LLViewerInventoryCategory* catp = modelp->getCategory(mUUID);
	if (!isAgentAvatarValid() || !catp || catp->isProtected())
	{
		return false;
	}

	if (isInMarketplace())
	{
		return LLMarketplaceData::getInstance()->isSLMDataFetched() &&
			   !LLMarketplace::isFolderActive(mUUID);
	}

	LLInventoryModel::cat_array_t child_cat;
	LLInventoryModel::item_array_t child_items;
	modelp->collectDescendents(mUUID, child_cat, child_items, false);

	for (S32 i = 0, count = child_items.size(); i < count; ++i)
	{
		LLViewerInventoryItem* vitemp = child_items[i];
		if (vitemp && !vitemp->getIsLinkType() &&
			get_is_item_worn(vitemp->getUUID(), false))
		{
			return false;
		}
	}

	return true;
}

//virtual
bool LLFolderBridge::isUpToDate() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp) return false;

	LLViewerInventoryCategory* vcatp = modelp->getCategory(mUUID);
	return vcatp && !vcatp->isVersionUnknown();
}

//virtual
bool LLFolderBridge::isItemCopyable() const
{
	if (getPreferredType() != LLFolderType::FT_NONE)
	{
		// Do not allow to copy any special folder
		return false;
	}

	// Get the content of the folder
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(mUUID, cat_array, item_array);

	// Check the items
	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator
			iter = item_array_copy.begin(), end = item_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryItem* vitemp = *iter;
		if (vitemp)	// Paranoia
		{
			LLItemBridge item_br(mInventoryPanel, vitemp->getUUID());
			if (!item_br.isItemCopyable())
			{
				return false;
			}
		}
	}

	// Recurse through the sub-folders
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator
			iter = cat_array_copy.begin(), end = cat_array_copy.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryCategory* cat = *iter;
		if (cat)	// Paranoia
		{
			LLFolderBridge cat_br(mInventoryPanel, cat->getUUID());
			if (!cat_br.isItemCopyable())
			{
				return false;
			}
		}
	}

	return true;
}

//virtual
bool LLFolderBridge::copyToClipboard() const
{
	if (isItemCopyable())
	{
		LLViewerInventoryCategory* cat = getCategory();
		if (!cat || (cat->getIsLinkType() && isLinkedObjectMissing()))
		{
			// No category (paranoia, should not happen), or broken link
			return false;
		}
		HBInventoryClipboard::add(cat->getLinkedUUID());
		return true;
	}
	return false;
}

//virtual
bool LLFolderBridge::isClipboardPasteableAsLink() const
{
#if 0	// We do not support pasting folders as links (it is useless anyway...)
	// Check normal paste-as-link permissions
	if (!LLInvFVBridge::isClipboardPasteableAsLink())
	{
		return false;
	}

	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	const LLViewerInventoryCategory* current_cat = getCategory();
	if (current_cat)
	{
		const LLUUID& current_cat_id = current_cat->getUUID();
		uuid_vec_t objects;
		HBInventoryClipboard::retrieve(objects);
		S32 count = objects.size();
		for (S32 i = 0; i < count; ++i)
		{
			const LLUUID& obj_id = objects[i];
			const LLViewerInventoryCategory* cat = model->getCategory(obj_id);
			if (cat)
			{
				const LLUUID& cat_id = cat->getUUID();
				// Don't allow recursive pasting
				if (cat_id == current_cat_id ||
					model->isObjectDescendentOf(current_cat_id, cat_id))
				{
					return false;
				}
			}
		}
	}

	return true;
#else
	// Check normal paste-as-link permissions
	return LLInvFVBridge::isClipboardPasteableAsLink();
#endif
}

bool LLFolderBridge::dragCategoryIntoFolder(LLInventoryCategory* catp,
											bool drop,
											std::string& tooltip_msg)
{
	// This should never happen, but if an inventory item is incorrectly
	// parented, the UI will get confused and pass in a NULL.
	if (!catp)
	{
		return false;
	}

	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp || !isAgentAvatarValid() || !isAgentInventory() || isInCOF())
	{
		return false;
	}

	const LLUUID& cat_id = catp->getUUID();
	const LLUUID& from_folder_uuid = catp->getParentUUID();

	if (mUUID == cat_id ||								// Not into self
		mUUID == from_folder_uuid ||					// Nothing would change
		modelp->isObjectDescendentOf(mUUID, cat_id))	// Avoid circularity
	{
		return false;
	}

	const LLUUID& market_id = LLMarketplace::getMPL();
	bool move_is_into_market = modelp->isObjectDescendentOf(mUUID, market_id);
	bool move_is_from_market = modelp->isObjectDescendentOf(cat_id, market_id);
	bool move_is_into_trash = isInTrash();

	bool accept = false;
	LLInventoryModel::cat_array_t	descendent_categories;
	LLInventoryModel::item_array_t	descendent_items;

	// Check to make sure source is agent inventory, and is represented there.
	LLToolDragAndDrop::ESource source = gToolDragAndDrop.getSource();
	bool is_agent_inventory = modelp->getCategory(cat_id) != NULL &&
							  source == LLToolDragAndDrop::SOURCE_AGENT;
	if (is_agent_inventory)
	{
		bool movable = !((LLViewerInventoryCategory*)catp)->isProtected();
		if (movable &&
			getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
		{
			// Cannot move a folder into a stock folder
			movable = false;
		}

		// Is the destination the trash ?
		if (movable && move_is_into_trash)
		{
			for (S32 i = 0, count = descendent_items.size(); i < count; ++i)
			{
				LLViewerInventoryItem* item = descendent_items[i];
				if (!item || item->getIsLinkType())
				{
					// Inventory links can always be destroyed
					continue;
				}
				if (get_is_item_worn(item->getUUID(), false))
				{
					// It is generally movable, but not into the trash !
					movable = false;
					break;
				}
			}
		}

		LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
		if (movable && move_is_from_market &&
			marketdata->getActivationState(cat_id))
		{
			movable = false;
			if (!tooltip_msg.empty())
			{
				tooltip_msg += ' ';
			}
			tooltip_msg = LLTrans::getString("TooltipOutboxDragActive");
		}

		if (movable && move_is_into_market)
		{
			const LLViewerInventoryCategory* master_catp =
				modelp->getFirstDescendantOf(market_id, mUUID);

			LLViewerInventoryCategory* dest_catp = getCategory();
			S32 bundle_size = 1;
			if (!drop)
			{
				bundle_size = gToolDragAndDrop.getCargoCount();
			}
			std::string error_msg;
			movable = LLMarketplace::canMoveFolderInto(master_catp,
													   dest_catp,
													   (LLViewerInventoryCategory*)catp,
													   error_msg,
													   bundle_size);
		}

		accept = movable;

		if (accept && !drop && (move_is_from_market || move_is_into_market))
		{
			if (move_is_from_market)
			{
				if (marketdata->isInActiveFolder(cat_id) ||
					marketdata->isListedAndActive(cat_id))
				{
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					if (marketdata->isListed(cat_id) ||
						marketdata->isVersionFolder(cat_id))
					{
						// Moving the active version folder or listing folder
						// itself outside the Marketplace Listings would unlist
						// the listing
						tooltip_msg += LLTrans::getString("TipMerchantUnlist");
					}
					else
					{
						tooltip_msg += LLTrans::getString("TipMerchantActiveChange");
					}
				}
				else if (marketdata->isVersionFolder(cat_id))
				{
					// Moving the version folder from its location would
					// deactivate it.
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					tooltip_msg += LLTrans::getString("TipMerchantClearVersion");
				}
				else if (marketdata->isListed(cat_id))
				{
					// Moving a whole listing folder folder would result in
					// archival of SLM data.
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					tooltip_msg += LLTrans::getString("TipListingDelete");
				}
			}
			else // move_is_into_market
			{
				if (marketdata->isInActiveFolder(mUUID))
				{
					// Moving something in an active listed listing would
					// modify it.
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					tooltip_msg += LLTrans::getString("TipMerchantActiveChange");
				}
				if (!move_is_from_market)
				{
					if (!tooltip_msg.empty())
					{
						tooltip_msg += ' ';
					}
					tooltip_msg += LLTrans::getString("TipMerchantMoveInventory");
				}
			}
		}

		if (accept && drop)
		{
 			// Look for any gestures and deactivate them
			if (move_is_into_trash)
			{
				for (S32 i = 0, count = descendent_items.size();
					 i < count; ++i)
				{
					LLViewerInventoryItem* itemp = descendent_items[i];
					if (itemp && itemp->getType() == LLAssetType::AT_GESTURE &&
						gGestureManager.isGestureActive(itemp->getUUID()))
					{
						gGestureManager.deactivateGesture(itemp->getUUID());
					}
				}
			}

			if (move_is_into_market)
			{
				LLMarketplace::moveFolderInto((LLViewerInventoryCategory*)catp,
											  mUUID);
			}
			else
			{
				// Reparent the folder and restamp children if it's moving
				// into trash.
				changeCategoryParent(modelp,
									 (LLViewerInventoryCategory*)catp,
									 mUUID, move_is_into_trash);
			}
			if (move_is_from_market)
			{
				LLMarketplace::updateMovedFrom(from_folder_uuid, cat_id);
			}
		}
	}
	else if (LLToolDragAndDrop::SOURCE_WORLD == source)
	{
		if (move_is_into_market)
		{
			accept = false;
		}
		else
		{
			// Content category has same ID as object itself
			accept = move_inv_category_world_to_agent(cat_id, mUUID, drop);
		}
	}

	if (accept && drop && move_is_into_trash)
	{
		modelp->checkTrashOverflow();
	}

	return accept;
}

bool LLFindWearables::operator()(LLInventoryCategory*, LLInventoryItem* itemp)
{
	if (!itemp)
	{
		return false;
	}
	LLAssetType::EType t = itemp->getType();
	return t == LLAssetType::AT_CLOTHING || t == LLAssetType::AT_BODYPART;
}

// Used by LLFolderBridge as callback for directory recursion.
class LLRightClickInventoryFetchObserver final
:	public LLInventoryFetchObserver
{
public:
	LLRightClickInventoryFetchObserver()
	:	mCopyItems(false)
	{
	}

	LLRightClickInventoryFetchObserver(const LLUUID& cat_id, bool copy_items)
	:	mCatID(cat_id),
		mCopyItems(copy_items)
	{
	}

	void done() override
	{
		// We have downloaded all the items, so repaint the dialog
		LLFolderBridge::staticFolderOptionsMenu();

		gInventory.removeObserver(this);
		delete this;
	}

protected:
	LLUUID mCatID;
	bool mCopyItems;
};

// Used by LLFolderBridge as callback for directory recursion.
class LLRightClickInventoryFetchDescendentsObserver final
:	public LLInventoryFetchDescendentsObserver
{
public:
	LLRightClickInventoryFetchDescendentsObserver(bool copy_items)
	:	mCopyItems(copy_items)
	{
	}

	~LLRightClickInventoryFetchDescendentsObserver() override
	{
	}

	void done() override;

protected:
	bool mCopyItems;
};

//virtual
void LLRightClickInventoryFetchDescendentsObserver::done()
{
	// Avoid passing a NULL-ref as mCompleteFolders.front() down to
	// gInventory.collectDescendents()
	if (mCompleteFolders.empty())
	{
		llwarns << "Empty mCompleteFolders" << llendl;
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
	LLRightClickInventoryFetchObserver* outfit =
		new LLRightClickInventoryFetchObserver(mCompleteFolders.front(),
											   mCopyItems);
	uuid_vec_t ids;
	for (S32 i = 0, count = item_array.size(); i < count; ++i)
	{
		ids.emplace_back(item_array[i]->getUUID());
	}

	// Clean up, and remove this as an observer since the call to the outfit
	// could notify observers and throw us into an infinite loop.
	gInventory.removeObserver(this);
	delete this;

	// Do the fetch
	outfit->fetchItems(ids);
	// Not interested in waiting and this will be right 99% of the time:
	outfit->done();
}

//virtual
void LLFolderBridge::performAction(LLFolderView* folderp,
								   LLInventoryModel* modelp,
								   const std::string& action)
{
	if (action == "open")
	{
		openItem();
	}
	else if (action == "paste_link")
	{
		pasteLinkFromClipboard();
	}
	else if (action == "properties")
	{
		showProperties();
	}
	else if (action == "thumbnail")
	{
		HBFloaterThumbnail::showInstance(mUUID);
	}
	else if (action == "replaceoutfit")
	{
		modifyOutfit(false, false);
	}
	else if (action == "addtooutfit")
	{
		modifyOutfit(true, false);
	}
	else if (action == "wearitems")
	{
		modifyOutfit(true, true);
	}
	else if (action == "removefromoutfit")
	{
		LLInventoryModel* modelp = mInventoryPanel->getModel();
		if (!modelp) return;
		LLViewerInventoryCategory* catp = getCategory();
		if (!catp) return;
//MK
		if (gRLenabled && !gRLInterface.canDetachCategory(catp))
		{
			return;
		}
//mk
		gAppearanceMgr.removeInventoryCategoryFromAvatar(catp);
	}
	else if (action == "updatelinks")
	{
		gAppearanceMgr.updateClothingOrderingInfo(mUUID);
		gNotifications.add("ReorderingWearablesLinks");
	}
	else if (action == "purge")
	{
		if (modelp->isCategoryComplete(mUUID))
		{
			purgeItem(modelp, mUUID);
		}
		else
		{
			llwarns << "Not purging the incompletely downloaded folder: "
					<< mUUID << llendl;
		}
	}
	else if (action == "restore")
	{
		restoreItem();
	}
	else if (action == "marketplace_connect")
	{
		LLMarketplace::checkMerchantStatus();
	}
	else if (action == "marketplace_list")
	{
		LLMarketplace::listFolder(mUUID);
	}
	else if (action == "marketplace_unlist")
	{
		LLMarketplace::listFolder(mUUID, false);
	}
	else if (action == "marketplace_activate")
	{
		LLMarketplace::activateFolder(mUUID);
	}
	else if (action == "marketplace_deactivate")
	{
		LLMarketplace::activateFolder(mUUID, false);
	}
	else if (action == "marketplace_get_listing")
	{
		LLMarketplace::getListing(mUUID);
	}
	else if (action == "marketplace_create_listing")
	{
		LLMarketplace::createListing(mUUID);
	}
	else if (action == "marketplace_associate_listing")
	{
		LLFloaterAssociateListing::show(mUUID);
	}
	else if (action == "marketplace_disassociate_listing")
	{
		LLMarketplace::clearListing(mUUID);
	}
	else if (action == "marketplace_check_listing")
	{
		LLFloaterMarketplaceValidation::show(mUUID);
	}
	else if (action == "marketplace_edit_listing")
	{
		LLMarketplace::editListing(mUUID);
	}
}

//virtual
void LLFolderBridge::openItem()
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (modelp)
	{
		modelp->fetchDescendentsOf(mUUID);
	}
}

//virtual
bool LLFolderBridge::isItemRenameable() const
{
	LLViewerInventoryCategory* vcatp = getCategory();
//MK
	if (gRLenabled && gRLInterface.isUnderRlvShare(vcatp) &&
		gRLInterface.isFolderLocked(vcatp))
	{
		return false;
	}
//mk
	return vcatp && vcatp->getOwnerID() == gAgentID &&
		   !LLFolderType::lookupIsProtectedType(vcatp->getPreferredType());
}

//virtual
void LLFolderBridge::restoreItem()
{
	LLViewerInventoryCategory* vcatp = getCategory();
	if (vcatp)
	{
		LLInventoryModel* modelp = mInventoryPanel->getModel();
		LLFolderType::EType type =
			LLFolderType::assetTypeToFolderType(vcatp->getType());
		const LLUUID& new_parent = modelp->findCategoryUUIDForType(type);
		// false to avoid restamping children on restore
		changeCategoryParent(modelp, vcatp, new_parent, false);
	}
}

//virtual
LLFolderType::EType LLFolderBridge::getPreferredType() const
{
	LLFolderType::EType preferred_type = LLFolderType::FT_NONE;
	LLViewerInventoryCategory* vcatp = getCategory();
	if (vcatp)
	{
		preferred_type = vcatp->getPreferredType();
	}
	return preferred_type;
}

// Icons for folders are based on the preferred type
//virtual
LLUIImagePtr LLFolderBridge::getIcon() const
{
	LLFolderType::EType preferred_type = LLFolderType::FT_NONE;
	LLViewerInventoryCategory* vcatp = getCategory();
	if (vcatp)
	{
		preferred_type = vcatp->getPreferredType();
	}
	if (preferred_type == LLFolderType::FT_NONE &&
		LLMarketplace::depthNesting(mUUID) == 2)
	{
		preferred_type = LLFolderType::FT_MARKETPLACE_VERSION;
	}
	return LLViewerFolderType::lookupIcon(preferred_type);
}

//virtual
std::string LLFolderBridge::getLabelSuffix() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (modelp)
	{
		const LLUUID& market_id = LLMarketplace::getMPL();
		if (market_id.notNull())
		{
			if (mUUID == market_id)
			{
				return LLMarketplace::rootFolderLabelSuffix();
			}
			if (modelp->isObjectDescendentOf(mUUID, market_id))
			{
				return LLMarketplace::folderLabelSuffix(mUUID);
			}
		}
	}
	return LLStringUtil::null;
}

//virtual
const LLUUID& LLFolderBridge::getThumbnailUUID() const
{
	LLViewerInventoryCategory* catp = getCategory();
	return catp ? catp->getThumbnailUUID() : LLUUID::null;
}

//virtual
LLFontGL::StyleFlags LLFolderBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

	if (isInMarketplace() && LLMarketplace::isFolderActive(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	return (LLFontGL::StyleFlags)font;
}

//virtual
bool LLFolderBridge::renameItem(const std::string& new_name)
{
	if (!isItemRenameable()) return false;

	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp) return false;

	rename_category(modelp, mUUID, new_name);

	// Return false because we either notified observers (and therefore
	// rebuilt) or we did not update.
	return false;
}

//virtual
bool LLFolderBridge::removeItem()
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp || !isItemRemovable())
	{
		return false;
	}

	// Move it to the trash
	modelp->removeCategory(mUUID);

	return true;
}

//virtual
void LLFolderBridge::pasteFromClipboard()
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp || !isClipboardPasteable() || isInTrash() || isInCOF())
	{
		return;
	}

	bool move_is_into_market = isInMarketplace();

	bool is_cut = false;	// Copy mode in force
	uuid_vec_t objects;
	// Retrieve copied objects, if any
	HBInventoryClipboard::retrieve(objects);
	S32 count = objects.size();
	if (!count)
	{
		// Retrieve cut objects, then, if any...
		HBInventoryClipboard::retrieveCuts(objects);
		count = objects.size();
		if (!count)
		{
			return; // nothing to do !
		}
		is_cut = true;	// Cut mode in force
	}

	LLViewerInventoryCategory* dest_catp = getCategory();
	if (move_is_into_market)
	{
		std::string error_msg;
		const LLUUID& root_id = LLMarketplace::getMPL();
		const LLViewerInventoryCategory* master_catp =
			modelp->getFirstDescendantOf(root_id, mUUID);

		for (S32 i = 0; i < count; ++i)
		{
			const LLUUID& object_id = objects[i];
			LLViewerInventoryItem* itemp = modelp->getItem(object_id);
			if (itemp &&
				!LLMarketplace::canMoveItemInto(master_catp, dest_catp,
												itemp, error_msg, count - i,
												true))
			{
				break;
			}
			LLViewerInventoryCategory* catp = modelp->getCategory(object_id);
			if (catp &&
				!LLMarketplace::canMoveFolderInto(master_catp, dest_catp, catp,
												  error_msg, count - i, true))
			{
				break;
			}
		}
		if (!error_msg.empty())
		{
			LLSD subs;
			subs["[ERROR_CODE]"] = error_msg;
			gNotifications.add("MerchantPasteFailed", subs);
			return;
		}
	}
	else
	{
		// Check that all items can be moved into that folder: for the
		// moment, only stock folder mismatch is checked
		bool dest_is_stock =
			dest_catp->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK;
		for (S32 i = 0; i < count; ++i)
		{
			const LLUUID& object_id = objects[i];
			LLViewerInventoryItem* itemp = modelp->getItem(object_id);
			LLViewerInventoryCategory* catp = modelp->getCategory(object_id);
			if ((catp && dest_is_stock) ||
				(itemp && !dest_catp->acceptItem(itemp)))
			{
				LLSD subs;
				subs["[ERROR_CODE]"] =
					LLTrans::getString("TooltipOutboxMixedStock");
				gNotifications.add("StockPasteFailed", subs);
				return;
			}
		}
	}

	for (S32 i = 0; i < count; ++i)
	{
		const LLUUID& object_id = objects[i];
		LLViewerInventoryCategory* catp = modelp->getCategory(object_id);
		if (catp)
		{
			if (is_cut)
			{
				LLMarketplace::clearListing(object_id);
				if (move_is_into_market)
				{
					LLMarketplace::moveFolderInto(catp, mUUID);
				}
				else if (mUUID != object_id &&
						 mUUID != catp->getParentUUID() &&
						 !modelp->isObjectDescendentOf(mUUID, object_id))
				{
					changeCategoryParent(modelp, catp, mUUID, false);
				}
			}
			else if (move_is_into_market)
			{
				LLMarketplace::moveFolderInto(catp, mUUID, true);
			}
			else
			{
				copy_inventory_category(modelp, catp, mUUID);
			}
			continue;
		}

		LLViewerInventoryItem* itemp = modelp->getItem(object_id);
		if (!itemp)
		{
			continue;
		}

		if (is_cut)
		{
			if (move_is_into_market)
			{
				if (!LLMarketplace::moveItemInto(itemp, mUUID))
				{
					// Stop pasting into the marketplace as soon as we get an
					// error
					break;
				}
			}
			else if (mUUID != itemp->getParentUUID())
			{
				changeItemParent(modelp, itemp, mUUID, false);
			}
		}
		else if (move_is_into_market)
		{
			if (!LLMarketplace::moveItemInto(itemp, mUUID, true))
			{
				// Stop pasting into the marketplace as soon as we get an error
				break;
			}
		}
		else
		{
			copy_inventory_item(itemp->getPermissions().getOwner(),
								itemp->getUUID(), mUUID);
		}
	}

	modelp->notifyObservers();
}

//virtual
void LLFolderBridge::pasteLinkFromClipboard()
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp || isInTrash() || isInCOF() || isInMarketplace())
	{
		return;
	}

	// This description should only show if the object cannot find its baseobj:
	const std::string description = "Broken link";
	uuid_vec_t objects;
	HBInventoryClipboard::retrieve(objects);
	for (uuid_vec_t::const_iterator iter = objects.begin(),
									end = objects.end();
		 iter != end; ++iter)
	{
		const LLUUID& object_id = *iter;
		if (LLViewerInventoryItem* vitemp = modelp->getItem(object_id))
		{
			link_inventory_item(vitemp->getLinkedUUID(), mUUID, description,
								LLAssetType::AT_LINK);
		}
	}
}

//static
void LLFolderBridge::staticFolderOptionsMenu()
{
	if (sSelf)
	{
		sSelf->folderOptionsMenu();
	}
}

void LLFolderBridge::folderOptionsMenu(U32 flags)
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp) return;

	if (mUUID == modelp->getLostAndFoundID())
	{
		// This is the lost+found folder.
		mItems.emplace_back("Empty Lost And Found");
		LLViewerInventoryCategory* laf = getCategory();
		LLInventoryModel::cat_array_t* cat_array;
		LLInventoryModel::item_array_t* item_array;
		modelp->getDirectDescendentsOf(mUUID, cat_array, item_array);
		// Enable "Empty Lost And Found" menu item only when there is something
		// to act upon. Also do not enable menu if folder is not fully fetched.
		if ((item_array->size() == 0 && cat_array->size() == 0) || !laf ||
			laf->isVersionUnknown() || !modelp->isCategoryComplete(mUUID))
		{
			mDisabledItems.emplace_back("Empty Lost And Found");
		}
	}
	else if (mUUID == modelp->getTrashID())
	{
		// This is the trash.
		mItems.emplace_back("Empty Trash");
		LLViewerInventoryCategory* trash = getCategory();
		LLInventoryModel::cat_array_t* cat_array;
		LLInventoryModel::item_array_t* item_array;
		modelp->getDirectDescendentsOf(mUUID, cat_array, item_array);
		// Enable "Empty Trash" menu item only when there is something to act
		// upon. Also do not enable menu if folder is not fully fetched.
		if ((item_array->size() == 0 && cat_array->size() == 0) || !trash ||
			trash->isVersionUnknown() || !modelp->isCategoryComplete(mUUID))
		{
			mDisabledItems.emplace_back("Empty Trash");
		}
	}
	else if (modelp->isInTrash(mUUID))
	{
		// This is a folder in the trash.
		mItems.clear(); // clear any items that used to exist
		const LLInventoryObject* obj = getInventoryObject();
		if (obj && obj->getIsLinkType())
		{
			mItems.emplace_back("Find Original");
			if (isLinkedObjectMissing())
			{
				mDisabledItems.emplace_back("Find Original");
			}
		}
		mItems.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			mDisabledItems.emplace_back("Purge Item");
		}

		mItems.emplace_back("Restore Item");
	}
	else if (isInMarketplace())
	{
		// Allow to use the clipboard actions
		getClipboardEntries(false, mItems, mDisabledItems, flags);

		if (isInMarketplace())
		{
			LLMarketplace::inventoryContextMenu(this, mUUID, flags, mItems,
												mDisabledItems);
		}
	}
	else
	{
		bool agent_inventory = isAgentInventory();
		const LLUUID& cof_id = gAppearanceMgr.getCOF();
		 // Do not allow creating in library neither in COF
		if (mUUID != cof_id)
		{
			if (agent_inventory)
			{
				mItems.emplace_back("New Folder");
				mItems.emplace_back("New Script");
				mItems.emplace_back("New Note");
				mItems.emplace_back("New Gesture");
				mItems.emplace_back("New Material");
				if (!gAgent.hasInventoryMaterial())
				{
					mDisabledItems.emplace_back("New Material");
				}
				mItems.emplace_back("New Clothes");
				mItems.emplace_back("New Body Parts");
				if (mUUID ==
						modelp->findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD))
				{
					mItems.emplace_back("New Calling Card");
				}
				mItems.emplace_back("New Settings");
				if (!gAgent.hasInventorySettings())
				{
					mDisabledItems.emplace_back("New Settings");
				}
				mItems.emplace_back("Upload Prefs Separator");
				mItems.emplace_back("Upload Prefs");
			}

			getClipboardEntries(false, mItems, mDisabledItems, flags);
		}
		else if (cof_id == mUUID && LLFolderType::getCanDeleteCOF())
		{
			// Allow to delete the COF when not in use
			mItems.emplace_back("Delete");
		}

		if (!mCallingCards)
		{
			LLIsType is_callingcard(LLAssetType::AT_CALLINGCARD);
			mCallingCards = checkFolderForContentsOfType(modelp,
														 is_callingcard);
		}
		if (mCallingCards)
		{
			mItems.emplace_back("Calling Card Separator");
			mItems.emplace_back("Conference Chat Folder");
		}

		if (!mWearables)
		{
			LLFindWearables is_wearable;
			LLIsType is_object(LLAssetType::AT_OBJECT);
			LLIsType is_gesture(LLAssetType::AT_GESTURE);
			mWearables = checkFolderForContentsOfType(modelp, is_wearable) ||
						 checkFolderForContentsOfType(modelp, is_object) ||
						 checkFolderForContentsOfType(modelp, is_gesture);
		}
		if (cof_id != mUUID && mWearables)
		{
			mItems.emplace_back("Folder Wearables Separator");
			mItems.emplace_back("Add To Outfit");
			mItems.emplace_back("Wear Items");
			mItems.emplace_back("Replace Outfit");
			if (agent_inventory)
			{
				mItems.emplace_back("Take Off Items");
				mItems.emplace_back("Update Links");
			}
//MK
			if (gRLenabled)
			{
				if (gRLInterface.mContainsDetach &&
					(!gSavedSettings.getBool("RestrainedLoveAllowWear") ||
					 gRLInterface.mContainsDefaultwear))
				{
					mDisabledItems.emplace_back("Add To Outfit");
					mDisabledItems.emplace_back("Wear Items");
					mDisabledItems.emplace_back("Replace Outfit");
					if (agent_inventory)
					{
						mDisabledItems.emplace_back("Take Off Items");
					}
				}
				else
				{
					LLViewerInventoryCategory* vcatp =
						modelp->getCategory(mUUID);
					if (vcatp && !gRLInterface.canAttachCategory(vcatp))
					{
						mDisabledItems.emplace_back("Add To Outfit");
						mDisabledItems.emplace_back("Wear Items");
						mDisabledItems.emplace_back("Replace Outfit");
					}
					if (vcatp && agent_inventory &&
						!gRLInterface.canDetachCategory(vcatp))
					{
						mDisabledItems.emplace_back("Take Off Items");
					}
				}
			}
//mk
		}
	}

	set_menu_entries_state(*mMenu, mItems, mDisabledItems);
}

bool LLFolderBridge::checkFolderForContentsOfType(LLInventoryModel* modelp,
												  LLInventoryCollectFunctor& is_type)
{
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	modelp->collectDescendentsIf(mUUID, cat_array, item_array,
								 LLInventoryModel::EXCLUDE_TRASH, is_type);
	return item_array.size() > 0;
}

//virtual
void LLFolderBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp) return;

	LLViewerInventoryCategory* vcatp = modelp->getCategory(mUUID);
	if (!vcatp) return;

	mItems.clear();
	mDisabledItems.clear();
	mCallingCards = mWearables = false;
	mMenu = &menu;
	folderOptionsMenu(flags);

	sSelf = this;
	LLRightClickInventoryFetchDescendentsObserver* observerp =
		new LLRightClickInventoryFetchDescendentsObserver(false);
	uuid_vec_t folders;
	folders.emplace_back(vcatp->getUUID());
	observerp->fetchDescendents(folders);
	if (observerp->isFinished())
	{
		// Everything is already here.
		observerp->done();
	}
	else
	{
		// It is all on its way: add an observer, and the inventory will call
		// done for us when everything is here.
		modelp->addObserver(observerp);
	}
}

//virtual
bool LLFolderBridge::hasChildren() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	return modelp &&
		   modelp->categoryHasChildren(mUUID) != LLInventoryModel::CHILDREN_NO;
}

//virtual
bool LLFolderBridge::dragOrDrop(MASK mask, bool drop,
								EDragAndDropType cargo_type,
								void* cargo_data,
								std::string& tooltip_msg)
{
	if ((cargo_type == DAD_SETTINGS && !gAgent.hasInventorySettings()) ||
		(cargo_type == DAD_MATERIAL && !gAgent.hasInventoryMaterial()))
	{
		return false;
	}

	bool accept = false;
	switch (cargo_type)
	{
		case DAD_TEXTURE:
		case DAD_SOUND:
		case DAD_CALLINGCARD:
		case DAD_LANDMARK:
		case DAD_SCRIPT:
		case DAD_OBJECT:
		case DAD_NOTECARD:
		case DAD_CLOTHING:
		case DAD_BODYPART:
		case DAD_ANIMATION:
		case DAD_GESTURE:
#if LL_MESH_ASSET_SUPPORT
		case DAD_MESH:
#endif
		case DAD_SETTINGS:
		case DAD_MATERIAL:
		case DAD_LINK:
			accept = dragItemIntoFolder((LLInventoryItem*)cargo_data, drop,
										tooltip_msg);
			break;

		case DAD_CATEGORY:
			accept = dragCategoryIntoFolder((LLInventoryCategory*)cargo_data,
											drop, tooltip_msg);
			break;

		default:
			break;
	}

	return accept;
}

LLViewerInventoryCategory* LLFolderBridge::getCategory() const
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	return modelp ? modelp->getCategory(mUUID) : NULL;
}

// Separate method so can be called by global menu as well as right-click menu
void LLFolderBridge::modifyOutfit(bool append, bool replace)
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp) return;
	LLViewerInventoryCategory* vcatp = getCategory();
	if (!vcatp) return;
//MK
	if (gRLenabled && !gRLInterface.canAttachCategory(vcatp))
	{
		return;
	}
//mk
	if (isAgentInventory())
	{
		gAppearanceMgr.wearInventoryCategoryOnAvatar(vcatp, append, replace);
		return;
	}

	// If in library, copy then add to/replace outfit
	if (!append &&
//MK
		(!gRLenabled || gRLInterface.canDetachCategory(vcatp)))
//mk
	{
		LLAgentWearables::userRemoveAllAttachments();
		LLAgentWearables::userRemoveAllClothes();
	}
	LLPointer<LLInventoryCategory> catp =
		new LLInventoryCategory(mUUID, LLUUID::null, LLFolderType::FT_CLOTHING,
								"Quick appearance");
	gAppearanceMgr.wearInventoryCategory(catp, true, !replace);
}

bool LLFolderBridge::dragItemIntoFolder(LLInventoryItem* itemp, bool drop,
										std::string& tooltip_msg)
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!itemp || !modelp || !isAgentInventory() || !isAgentAvatarValid() ||
		isInCOF())
	{
		return false;
	}

	const LLUUID& market_id = LLMarketplace::getMPL();
	const LLUUID& from_folder_uuid = itemp->getParentUUID();

	bool move_is_into_market = modelp->isObjectDescendentOf(mUUID, market_id);
	bool move_is_from_market = modelp->isObjectDescendentOf(itemp->getUUID(),
															market_id);
	bool move_is_into_trash = isInTrash();

	bool accept = false;
	LLViewerObject* object = NULL;
	LLToolDragAndDrop::ESource source = gToolDragAndDrop.getSource();
	if (source == LLToolDragAndDrop::SOURCE_AGENT)
	{
		bool movable = true;
		if (itemp->getActualType() == LLAssetType::AT_CATEGORY)
		{
			movable = !((LLViewerInventoryCategory*)itemp)->isProtected();
		}

		if (movable && move_is_into_trash)
		{
			movable = itemp->getIsLinkType() ||
					  !get_is_item_worn(itemp->getUUID(), false);
		}

		if (move_is_into_market && !move_is_from_market)
		{
			const LLViewerInventoryCategory* master_catp =
				modelp->getFirstDescendantOf(market_id, mUUID);

			S32 count = gToolDragAndDrop.getCargoCount() -
						gToolDragAndDrop.getCargoIndex();
            LLViewerInventoryCategory* dest_catp = getCategory();
			accept = LLMarketplace::canMoveItemInto(master_catp, dest_catp,
													(LLViewerInventoryItem*)itemp,
													tooltip_msg, count);
		}
		else
		{
			accept = movable && mUUID != from_folder_uuid;
		}

		// Check that the folder can accept this item based on folder/item type
		// compatibility (e.g. stock folder compatibility)
		if (accept)
		{
			LLViewerInventoryCategory* dest_catp = getCategory();
			accept = dest_catp->acceptItem(itemp);
		}

		if (accept && !drop && (move_is_into_market || move_is_from_market))
		{
			LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
			if ((move_is_from_market &&
				 (marketdata->isInActiveFolder(itemp->getUUID()) ||
				  marketdata->isListedAndActive(itemp->getUUID()))) ||
				(move_is_into_market && marketdata->isInActiveFolder(mUUID)))
			{
				if (!tooltip_msg.empty())
				{
					tooltip_msg += ' ';
				}
				tooltip_msg += LLTrans::getString("TipMerchantActiveChange");
			}
			if (move_is_into_market && !move_is_from_market)
			{
				if (!tooltip_msg.empty())
				{
					tooltip_msg += ' ';
				}
				tooltip_msg += LLTrans::getString("TipMerchantMoveInventory");
			}
		}

		if (accept && drop)
		{
			if (move_is_into_trash &&
				itemp->getType() == LLAssetType::AT_GESTURE &&
				gGestureManager.isGestureActive(itemp->getUUID()))
			{
				gGestureManager.deactivateGesture(itemp->getUUID());
			}
			// If an item is being dragged between windows, unselect everything
			// in the active window so that we don't follow the selection to
			// its new location (which is very annoying).
			if (LLFloaterInventory::getActiveFloater())
			{
				LLInventoryPanel* active_floater =
					LLFloaterInventory::getActiveFloater()->getPanel();
				if (active_floater && mInventoryPanel != active_floater)
				{
					active_floater->unSelectAll();
				}
			}

			if (move_is_into_market)
			{
				LLMarketplace::moveItemInto((LLViewerInventoryItem*)itemp,
											mUUID);
			}
			else
			{
				changeItemParent(modelp, (LLViewerInventoryItem*)itemp,
								 mUUID, move_is_into_trash);
			}
#if 0
			if (move_is_from_market)
			{
				LLMarketplace::updateMovedFrom(from_folder_uuid);
			}
#endif
		}
	}
	else if (source == LLToolDragAndDrop::SOURCE_WORLD)
	{
		// Make sure the object exists. If we allowed dragging from anonymous
		// objects, it would be possible to bypass permissions.
		object = gObjectList.findObject(itemp->getParentUUID());
		if (!object)
		{
			llinfos << "Object not found for drop." << llendl;
			return false;
		}

		// Coming from a task. Need to figure out if the person can move/copy
		// this item.
		LLPermissions perm(itemp->getPermissions());
		bool is_move = false;
		if (perm.allowCopyBy(gAgentID, gAgent.getGroupID()) &&
			perm.allowTransferTo(gAgentID))	//   || gAgent.isGodlike())
		{
			accept = true;
		}
		else if (object->permYouOwner())
		{
			// If the object cannot be copied, but the object the inventory is
			// owned by the agent, then the item can be moved from the task to
			// agent inventory.
			is_move = accept = true;
		}
		if (move_is_into_market)
		{
			accept = false;
		}
		if (drop && accept)
		{
			LLMoveInv* move_inv = new LLMoveInv;
			move_inv->mObjectID = itemp->getParentUUID();
			move_inv->mMoveList.emplace_back(mUUID, itemp->getUUID());
			move_inv->mCallback = NULL;
			move_inv->mUserData = NULL;
			if (is_move)
			{
				warn_move_inventory(object, move_inv);
			}
			else
			{
				LLNotification::Params params("MoveInventoryFromObject");
				params.functor(boost::bind(move_task_inventory_callback,
										   _1, _2, move_inv));
				gNotifications.forceResponse(params, 0);
			}
		}
	}
	else if (source == LLToolDragAndDrop::SOURCE_NOTECARD)
	{
		accept = !move_is_into_market;
		if (accept && itemp->getActualType() == LLAssetType::AT_SETTINGS)
		{
			accept = gAgent.hasInventorySettings();
		}
		if (accept && drop)
		{
			copy_inventory_from_notecard(gToolDragAndDrop.getObjectID(),
										 gToolDragAndDrop.getSourceID(),
										 itemp);
		}
	}
	else if (source == LLToolDragAndDrop::SOURCE_LIBRARY)
	{
		LLViewerInventoryItem* vitemp = (LLViewerInventoryItem*)itemp;
		if (vitemp && vitemp->isFinished())
		{
			accept = !move_is_into_market;
			if (accept && drop)
			{
				copy_inventory_item(itemp->getPermissions().getOwner(),
									itemp->getUUID(), mUUID);
			}
		}
	}
	else
	{
		llwarns << "Unhandled drag source" << llendl;
	}

	if (accept && drop && move_is_into_trash)
	{
		modelp->checkTrashOverflow();
	}

	return accept;
}

///////////////////////////////////////////////////////////////////////////////
// LLScriptBridge (DEPRECATED)
///////////////////////////////////////////////////////////////////////////////

//virtual
LLUIImagePtr LLScriptBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SCRIPT,
									LLInventoryType::IT_LSL, 0, false);
}

///////////////////////////////////////////////////////////////////////////////
// LLTextureBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLTextureBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Texture") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLTextureBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_TEXTURE, mInvType, 0,
									false);
}

//virtual
void LLTextureBridge::openItem()
{
	open_texture(mUUID, getPrefix() + getName());
}

///////////////////////////////////////////////////////////////////////////////
// LLSoundBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLSoundBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Sound") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLSoundBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SOUND,
									LLInventoryType::IT_SOUND, 0, false);
}

//virtual
void LLSoundBridge::openItem()
{
	open_sound(mUUID, getPrefix() + getName());
}

//virtual
void LLSoundBridge::previewItem()
{
	LLViewerInventoryItem* item = getItem();
	if (!item) return;

	U32 action = gSavedSettings.getU32("DoubleClickInventorySoundAction");
	if (action == 0)
	{
		open_sound(mUUID, getPrefix() + getName());
	}
	else if (action == 1 && gAudiop)
	{
		// Play the sound locally.
		LLVector3d lpos_global = gAgent.getPositionGlobal();
		gAudiop->triggerSound(item->getAssetUUID(), gAgentID, 1.0,
							  LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
	}
	else if (action == 2)
	{
		// Play the sound in-world.
		send_sound_trigger(item->getAssetUUID(), 1.0);
	}
}

//virtual
void LLSoundBridge::performAction(LLFolderView* folderp,
								  LLInventoryModel* modelp,
								  const std::string& action)
{
	if (action == "playworld")
	{
		LLViewerInventoryItem* vitemp = getItem();
		if (vitemp)
		{
			send_sound_trigger(vitemp->getAssetUUID(), 1.0);
		}
	}
	else if (action == "playlocal")
	{
		LLViewerInventoryItem* vitemp = getItem();
		if (vitemp && gAudiop)
		{
			// Play the sound locally.
			LLVector3d lpos_global = gAgent.getPositionGlobal();
			gAudiop->triggerSound(vitemp->getAssetUUID(), gAgentID, 1.0,
								  LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
		}
	}
	else
	{
		LLItemBridge::performAction(folderp, modelp, action);
	}
}

//virtual
void LLSoundBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Sound Open");
		items.emplace_back("Sound Play1");
		items.emplace_back("Sound Play2");
		items.emplace_back("Sound Separator");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	set_menu_entries_state(menu, items, disabled_items);
}

///////////////////////////////////////////////////////////////////////////////
// LLLandmarkBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLLandmarkBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Landmark") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLLandmarkBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_LANDMARK,
									LLInventoryType::IT_LANDMARK, mVisited,
									false);
}

//virtual
void LLLandmarkBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Landmark Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	items.emplace_back("Landmark Separator");
	items.emplace_back("About Landmark");
	items.emplace_back("Show on Map");

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLLandmarkBridge::performAction(LLFolderView* folderp,
									 LLInventoryModel* modelp,
									 const std::string& action)
{
	if (action == "teleport")
	{
		LLViewerInventoryItem* vitemp = getItem();
		if (vitemp)
		{
			gAgent.teleportViaLandmark(vitemp->getAssetUUID());

			// We now automatically track the landmark you are teleporting to
			// because you will probably arrive at a telehub instead.
			if (gFloaterWorldMapp)
			{
				// remember this must be the item UUID, not the asset UUID
				gFloaterWorldMapp->trackLandmark(vitemp->getUUID());
			}
		}
	}
	else if (action == "about")
	{
		LLViewerInventoryItem* vitemp = getItem();
		if (vitemp)
		{
			std::string title = "  " + getPrefix() + vitemp->getName();
			open_landmark(vitemp, title);
		}
	}
	else if (action == "show_on_map")
	{
		LLViewerInventoryItem* vitemp = getItem();
		if (vitemp)
		{
			const LLUUID& asset_id = vitemp->getAssetUUID();
			if (asset_id.isNull()) return;	// Paranoia
			LLLandmark* landmarkp =
				gLandmarkList.getAsset(asset_id,
									   boost::bind(&LLLandmarkBridge::showOnMap,
												   this, _1));
			if (landmarkp)
			{
				showOnMap(landmarkp);
			}
		}
	}
	else
	{
		LLItemBridge::performAction(folderp, modelp, action);
	}
}

//virtual
void LLLandmarkBridge::openItem()
{
	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp)
	{
		// Double-clicking a landmark immediately teleports, but warns you the
		// first time.
		LLSD payload;
		payload["asset_id"] = vitemp->getAssetUUID();
		payload["item_id"] = vitemp->getUUID();
		gNotifications.add("TeleportFromLandmark", LLSD(), payload);
	}
}

void LLLandmarkBridge::showOnMap(LLLandmark* landmarkp)
{
	if (!landmarkp || !gFloaterWorldMapp)
	{
		return;
	}

	LLVector3d pos;
	if (landmarkp->getGlobalPos(pos))
	{
		if (!pos.isExactlyZero())
		{
			gFloaterWorldMapp->trackLocation(pos);
			LLFloaterWorldMap::show(NULL, true);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLCallingCardBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
void LLCallingCardObserver::changed(U32 mask)
{
	mBridgep->refreshFolderViewItem();
}

//virtual
const std::string& LLCallingCardBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Calling card") +
									  ": ";
	return prefix;
}

LLCallingCardBridge::LLCallingCardBridge(LLInventoryPanel* inventory,
										 const LLUUID& uuid)
:	LLItemBridge(inventory, uuid)
{
	mObserver = new LLCallingCardObserver(this);
	gAvatarTracker.addObserver(mObserver);
}

//virtual
LLCallingCardBridge::~LLCallingCardBridge()
{
	gAvatarTracker.removeObserver(mObserver);
	delete mObserver;
}

void LLCallingCardBridge::refreshFolderViewItem()
{
	LLFolderViewItem* itemp =
		mInventoryPanel->getRootFolder()->getItemByID(mUUID);
	if (itemp)
	{
		itemp->refresh();
	}
}

//virtual
void LLCallingCardBridge::performAction(LLFolderView* folderp,
										LLInventoryModel* modelp,
										const std::string& action)
{
	if (action == "begin_im")
	{
		LLViewerInventoryItem* vitemp = getItem();
		const LLUUID& id = vitemp ? vitemp->getCreatorUUID() : LLUUID::null;
		if (id.notNull() && id != gAgentID)
		{
			LLAvatarActions::startIM(id);
		}
	}
	else if (action == "lure")
	{
		LLViewerInventoryItem* vitemp = getItem();
		const LLUUID& id = vitemp ? vitemp->getCreatorUUID() : LLUUID::null;
		if (id.notNull() && id != gAgentID)
		{
			LLAvatarActions::offerTeleport(id);
		}
	}
	else if (action == "request_teleport")
	{
		LLViewerInventoryItem* vitemp = getItem();
		const LLUUID& id = vitemp ? vitemp->getCreatorUUID() : LLUUID::null;
		if (id.notNull() && id != gAgentID)
		{
			LLAvatarActions::teleportRequest(id);
		}
	}
	else
	{
		LLItemBridge::performAction(folderp, modelp, action);
	}
}

//virtual
LLUIImagePtr LLCallingCardBridge::getIcon() const
{
	bool online = false;
	LLViewerInventoryItem* vitemp = getItem();
	LLUUID id = get_calling_card_buddy_id(vitemp);
	if (id.notNull())
	{
		online = gAvatarTracker.isBuddyOnline(id);
	}
	return LLInventoryIcon::getIcon(LLAssetType::AT_CALLINGCARD,
									LLInventoryType::IT_CALLINGCARD,
									online, false);
}

//virtual
std::string LLCallingCardBridge::getLabelSuffix() const
{
	static const std::string online = " (" + LLTrans::getString("online") +
									  ")";
	LLViewerInventoryItem* vitemp = getItem();
	LLUUID id = get_calling_card_buddy_id(vitemp);
	if (id.notNull() && gAvatarTracker.isBuddyOnline(id))
	{
		return LLItemBridge::getLabelSuffix() + online;
	}
	return LLItemBridge::getLabelSuffix();
}

//virtual
void LLCallingCardBridge::openItem()
{
	open_callingcard((LLViewerInventoryItem*)getItem());
}

//virtual
void LLCallingCardBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);

		LLViewerInventoryItem* vitemp = getItem();
		LLUUID buddy_id = get_calling_card_buddy_id(vitemp);
		bool good_card = buddy_id.notNull();
		bool user_online = false;
		if (vitemp)
		{
			user_online = gAvatarTracker.isBuddyOnline(buddy_id);
		}
		items.emplace_back("Send Instant Message Separator");
		items.emplace_back("Send Instant Message");
		items.emplace_back("Offer Teleport...");
		items.emplace_back("Request Teleport...");
		items.emplace_back("Conference Chat");

		if (!good_card)
		{
			disabled_items.emplace_back("Send Instant Message");
		}
		if (!good_card || !user_online)
		{
			disabled_items.emplace_back("Offer Teleport...");
			disabled_items.emplace_back("Request Teleport...");
			disabled_items.emplace_back("Conference Chat");
		}
	}
	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
bool LLCallingCardBridge::dragOrDrop(MASK mask, bool drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data,
									 std::string& tooltip_msg)
{
	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp && cargo_data)
	{
		if ((cargo_type == DAD_SETTINGS && !gAgent.hasInventorySettings()) ||
			(cargo_type == DAD_MATERIAL && !gAgent.hasInventoryMaterial()))
		{
			return false;
		}

		// Check the type
		switch (cargo_type)
		{
			case DAD_TEXTURE:
			case DAD_SOUND:
			case DAD_LANDMARK:
			case DAD_SCRIPT:
			case DAD_CLOTHING:
			case DAD_OBJECT:
			case DAD_NOTECARD:
			case DAD_BODYPART:
			case DAD_ANIMATION:
			case DAD_GESTURE:
#if LL_MESH_ASSET_SUPPORT
			case DAD_MESH:
#endif
			case DAD_SETTINGS:
			case DAD_MATERIAL:
			{
				LLInventoryItem* itemp = (LLInventoryItem*)cargo_data;
				const LLPermissions& perm = itemp->getPermissions();
				if (gInventory.getItem(itemp->getUUID()) &&
					perm.allowTransferBy(gAgentID))
				{
					if (drop)
					{
						LLToolDragAndDrop::giveInventory(vitemp->getCreatorUUID(),
														 itemp);
					}
					return true;
				}
				else
				{
					// It is not in the user's inventory (it is probably in an
					// object's contents), so disallow dragging it here. You
					// cannot give something you do not yet have.
					return false;
				}
				break;
			}

			case DAD_CATEGORY:
			{
				LLInventoryCategory* inv_cat = (LLInventoryCategory*)cargo_data;
				if (gInventory.getCategory(inv_cat->getUUID()))
				{
					if (drop)
					{
						LLToolDragAndDrop::giveInventoryCategory(vitemp->getCreatorUUID(),
																 inv_cat);
					}
					return true;
				}
				else
				{
					// It is not in the user's inventory (it is probably in an
					// object's contents), so disallow dragging it here. You
					// cannot give something you do not yet have.
					return false;
				}
				break;
			}

			default:
				break;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLNotecardBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLNotecardBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Note") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLNotecardBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_NOTECARD,
									LLInventoryType::IT_NOTECARD, 0, false);
}

//virtual
void LLNotecardBridge::openItem()
{
	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp)
	{
		open_notecard(vitemp, getPrefix() + getName());
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLGestureBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLGestureBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Gesture") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLGestureBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_GESTURE,
									LLInventoryType::IT_GESTURE, 0, false);
}

//virtual
LLFontGL::StyleFlags LLGestureBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

	if (gGestureManager.isGestureActive(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	const LLViewerInventoryItem* vitemp = getItem();
	if (vitemp && vitemp->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

//virtual
std::string LLGestureBridge::getLabelSuffix() const
{
	static const std::string active = " (" + LLTrans::getString("active") +
									  ")";
	if (gGestureManager.isGestureActive(mUUID))
	{
		return LLItemBridge::getLabelSuffix() + active;
	}
	return LLItemBridge::getLabelSuffix();
}

//virtual
void LLGestureBridge::performAction(LLFolderView* folderp,
									LLInventoryModel* modelp,
									const std::string& action)
{
	if (action == "activate")
	{
		gGestureManager.activateGesture(mUUID);

		LLViewerInventoryItem* vitemp = gInventory.getItem(mUUID);
		if (!vitemp) return;

		// Since we just changed the suffix to indicate (active)
		// the server doesn't need to know, just the viewer.
		gInventory.updateItem(vitemp);
		gInventory.notifyObservers();
	}
	else if (action == "deactivate")
	{
		gGestureManager.deactivateGesture(mUUID);

		LLViewerInventoryItem* vitemp = gInventory.getItem(mUUID);
		if (!vitemp) return;

		// Since we just changed the suffix to indicate (active)
		// the server doesn't need to know, just the viewer.
		gInventory.updateItem(vitemp);
		gInventory.notifyObservers();
	}
	else
	{
		LLItemBridge::performAction(folderp, modelp, action);
	}
}

//virtual
void LLGestureBridge::openItem()
{
	open_gesture(mUUID, getPrefix() + getName());
}

//virtual
bool LLGestureBridge::removeItem()
{
	// Force close the preview window, if it exists
	gGestureManager.deactivateGesture(mUUID);
	return LLItemBridge::removeItem();
}

//virtual
void LLGestureBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	if (isInTrash())
	{
		const LLInventoryObject* invobjp = getInventoryObject();
		if (invobjp && invobjp->getIsLinkType())
		{
			items.emplace_back("Find Original");
			if (isLinkedObjectMissing())
			{
				disabled_items.emplace_back("Find Original");
			}
		}
		items.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Purge Item");
		}

		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Gesture Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);

		if (!isInMarketplace())
		{
			items.emplace_back("Gesture Separator");
			items.emplace_back("Activate");
			items.emplace_back("Deactivate");
		}
	}
	set_menu_entries_state(menu, items, disabled_items);
}

///////////////////////////////////////////////////////////////////////////////
// LLAnimationBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLAnimationBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Animation") + ": ";
	return prefix;
}

LLUIImagePtr LLAnimationBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_ANIMATION,
									LLInventoryType::IT_ANIMATION, 0, false);
}

//virtual
void LLAnimationBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Animation Open");
		items.emplace_back("Animation Play");
		items.emplace_back("Animation Audition");
		items.emplace_back("Animation Separator");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLAnimationBridge::performAction(LLFolderView* folderp,
									  LLInventoryModel* modelp,
									  const std::string& action)
{
	if (action == "playworld" || action == "playlocal")
	{
		S32 activate = action == "playworld" ? 1 : 2;
		open_animation(mUUID, getPrefix() + getName(), activate, LLUUID::null,
					   false);
	}
	else
	{
		LLItemBridge::performAction(folderp, modelp, action);
	}
}

//virtual
void LLAnimationBridge::openItem()
{
	open_animation(mUUID, getPrefix() + getName());
}

///////////////////////////////////////////////////////////////////////////////
// LLObjectBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLObjectBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Object") + ": ";
	return prefix;
}

//virtual
bool LLObjectBridge::isItemRemovable()
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp)
	{
		return false;
	}

	const LLInventoryObject* invobjp = modelp->getItem(mUUID);
	if (invobjp && invobjp->getIsLinkType())
	{
		return true;
	}

	if (!isAgentAvatarValid() || gAgentAvatarp->isWearingAttachment(mUUID))
	{
		return false;
	}

	return LLInvFVBridge::isItemRemovable();
}

//virtual
LLUIImagePtr LLObjectBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_OBJECT, mInvType,
									mAttachPt, mIsMultiObject);
}

//virtual
void LLObjectBridge::performAction(LLFolderView* folderp,
								   LLInventoryModel* modelp,
								   const std::string& action)
{
	if (action == "attach" || action == "attach_add")
	{
		bool replace = action == "attach"; // Replace if "Wear"ing.
		LLUUID object_id = gInventory.getLinkedItemID(mUUID);
		LLViewerInventoryItem* vitemp = gInventory.getItem(object_id);
		if (vitemp &&
			gInventory.isObjectDescendentOf(object_id,
											gInventory.getRootFolderID()))
		{
//MK
			if (gRLenabled && gRLInterface.canAttach(vitemp))
			{
				LLViewerJointAttachment* attachmentp = NULL;
				// If it is a no-mod item, the containing folder has priority
				// to decide where to wear it
				if (!vitemp->getPermissions().allowModifyBy(gAgentID))
				{
					attachmentp =
						gRLInterface.findAttachmentPointFromParentName(vitemp);
					if (attachmentp)
					{
						gAppearanceMgr.rezAttachment(vitemp, attachmentp,
													 replace);
					}
					else
					{
						// But the name itself could also have the information
						// => check
						attachmentp =
							gRLInterface.findAttachmentPointFromName(vitemp->getName());
						if (attachmentp)
						{
							gAppearanceMgr.rezAttachment(vitemp, attachmentp,
														 replace);
						}
						else if (!gRLInterface.mContainsDefaultwear &&
								 gSavedSettings.getBool("RestrainedLoveAllowWear"))
						{
							gAppearanceMgr.rezAttachment(vitemp, NULL, replace);
						}
					}
				}
				else
				{
					// This is a mod item, wear it according to its name
					attachmentp =
						gRLInterface.findAttachmentPointFromName(vitemp->getName());
					if (attachmentp)
					{
						gAppearanceMgr.rezAttachment(vitemp, attachmentp,
													 replace);
					}
					else if (!gRLInterface.mContainsDefaultwear &&
							 gSavedSettings.getBool("RestrainedLoveAllowWear"))
					{
						gAppearanceMgr.rezAttachment(vitemp, NULL, replace);
					}
				}
			}
			else
//mk
			{
				gAppearanceMgr.rezAttachment(vitemp, NULL, replace);
			}
		}
		else if (vitemp && vitemp->isFinished())
		{
			// Must be in the inventory library. Copy it to our inventory and
			// put it on right away.
			LLPointer<LLInventoryCallback> cb =
				new LLRezAttachmentCallback(NULL, replace);
			copy_inventory_item(vitemp->getPermissions().getOwner(),
								vitemp->getUUID(), LLUUID::null,
								LLStringUtil::null, cb);
		}
		else if (vitemp)
		{
			// *TODO: we should fetch the item details, and then do the
			// operation above.
			gNotifications.add("CannotWearInfoNotComplete");
		}
		gFocusMgr.setKeyboardFocus(NULL);
	}
	else if (action == "detach")
	{
		LLViewerInventoryItem* vitemp = gInventory.getItem(mUUID);
		if (vitemp)
		{
			LLVOAvatarSelf::detachAttachmentIntoInventory(vitemp->getLinkedUUID());
		}
	}
	else if (action == "edit" || action == "inspect")
	{
		LLViewerInventoryItem* vitemp = gInventory.getItem(mUUID);
		if (vitemp && isAgentAvatarValid())
		{
			LLViewerObject* vobj =
				gAgentAvatarp->getWornAttachment(vitemp->getLinkedUUID());
			if (vobj)
			{
				gSelectMgr.deselectAll();
				gSelectMgr.selectObjectAndFamily(vobj);
				if (action == "edit")
				{
					handle_object_edit();
				}
				else
				{
					handle_object_inspect();
				}
			}
		}
	}
	else
	{
		LLItemBridge::performAction(folderp, modelp, action);
	}
}

//virtual
void LLObjectBridge::openItem()
{
	if (isAgentAvatarValid() && !isInMarketplace())
	{
		if (gAgentAvatarp->isWearingAttachment(mUUID))
		{
//MK
			if (gRLenabled &&
				!gRLInterface.canDetach(gAgentAvatarp->getWornAttachment(mUUID)))
			{
				return;
			}
//mk
			performAction(NULL, NULL, "detach");
		}
		else
		{
			performAction(NULL, NULL, "attach");
		}
	}
}

//virtual
LLFontGL::StyleFlags LLObjectBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

	if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	const LLViewerInventoryItem* vitemp = getItem();
	if (vitemp && vitemp->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

//virtual
std::string LLObjectBridge::getLabelSuffix() const
{
	static const std::string wornon = " (" + LLTrans::getString("wornon") +
									  " ";
	std::string suffix = LLItemBridge::getLabelSuffix();
	if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(mUUID))
	{
		suffix += wornon;
		suffix += gAgentAvatarp->getAttachedPointName(mUUID, true) + ")";
	}
	return suffix;
}

//virtual
void LLObjectBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return;
	}
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Purge Item");
		}

		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Properties");
#if LL_RESTORE_TO_WORLD
		if (isInLostAndFound())
		{
			items.emplace_back("Restore to Last Position");
		}
#endif

		getClipboardEntries(true, items, disabled_items, flags);

		LLViewerInventoryItem* vitemp = getItem();
		if (vitemp && !isInMarketplace())
		{
			if (!isAgentAvatarValid())
			{
				return;
			}

			if (gAgentAvatarp->isWearingAttachment(mUUID))
			{
				items.emplace_back("Attach Separator");
				items.emplace_back("Detach From Yourself");
				items.emplace_back("Edit");
				items.emplace_back("Inspect");
				bool disable_edit = (flags & FIRST_SELECTED_ITEM) == 0;
				bool disable_inspect = disable_edit;
//MK
				if (gRLenabled)
				{
					if (gRLInterface.mContainsRez ||
						gRLInterface.mContainsEdit)
					{
						disable_edit = true;
					}
					if (gRLInterface.mContainsShownames ||
						gRLInterface.mContainsShownametags)
					{
						disable_inspect = true;
					}
					if (!gRLInterface.canDetach(gAgentAvatarp->getWornAttachment(mUUID)))
					{
						disabled_items.emplace_back("Detach From Yourself");
					}
				}
//mk
				if (disable_edit)
				{
					disabled_items.emplace_back("Edit");
				}
				if (disable_inspect)
				{
					disabled_items.emplace_back("Inspect");
				}
			}
			else if (isAgentInventory())
			{
				items.emplace_back("Attach Separator");
				items.emplace_back("Object Wear");
				items.emplace_back("Object Add");
				if (!gAgentAvatarp->canAttachMoreObjects())
				{
					disabled_items.emplace_back("Object Add");
				}
				items.emplace_back("Attach To");
				items.emplace_back("Attach To HUD");
//MK
				if (gRLenabled && gRLInterface.mContainsDetach &&
					(gRLInterface.mContainsDefaultwear ||
					 !gSavedSettings.getBool("RestrainedLoveAllowWear")) &&
					 !gRLInterface.findAttachmentPointFromName(vitemp->getName()) &&
					 !gRLInterface.findAttachmentPointFromParentName(vitemp))
				{
					disabled_items.emplace_back("Object Wear");
				}
//mk

				LLMenuGL* attach_menup  = menu.getChildMenuByName("Attach To",
																  true);
				LLMenuGL* attach_hud_menup =
					menu.getChildMenuByName("Attach To HUD", true);
				if (attach_menup && !attach_menup->getChildCount() &&
					attach_hud_menup && !attach_hud_menup->getChildCount())
				{
					std::string name;
					for (LLVOAvatar::attachment_map_t::iterator
							iter = gAgentAvatarp->mAttachmentPoints.begin(),
							end = gAgentAvatarp->mAttachmentPoints.end();
						 iter != end; ++iter)
					{
						LLViewerJointAttachment* attachmentp = iter->second;
						if (!attachmentp) continue;	// Paranoia

						name = LLTrans::getString(attachmentp->getName());
						LLMenuItemCallGL* entryp =
							new LLMenuItemCallGL(name, NULL, NULL,
												 &attach_label,
												 (void*)attachmentp);
						if (attachmentp->getIsHUDAttachment())
						{
							attach_hud_menup->append(entryp);
						}
						else
						{
							attach_menup->append(entryp);
						}
						LLSimpleListener* cb =
							mInventoryPanel->getListenerByName("Inventory.AttachObject");
						if (cb)
						{
							entryp->addListener(cb, "on_click", LLSD(name));
						}
					}
				}
//MK
				if (gRLenabled && !gRLInterface.canAttach(vitemp))
				{
					disabled_items.emplace_back("Object Wear");
					disabled_items.emplace_back("Object Add");
					disabled_items.emplace_back("Attach To");
					disabled_items.emplace_back("Attach To HUD");
				}
//mk
			}
		}
	}
	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
bool LLObjectBridge::renameItem(const std::string& new_name)
{
	if (!isItemRenameable())
	{
		return false;
	}

	LLPreview::rename(mUUID, getPrefix() + new_name);

	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return false;
	}

	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp && vitemp->getName() != new_name)
	{
		LLPointer<LLViewerInventoryItem> new_vitemp;
		new_vitemp = new LLViewerInventoryItem(vitemp);
		new_vitemp->rename(new_name);
		buildDisplayName(new_vitemp, mDisplayName);
		new_vitemp->updateServer(false);
		model->updateItem(new_vitemp);
		model->notifyObservers();

		if (isAgentAvatarValid())
		{
			LLViewerObject* obj;
			obj = gAgentAvatarp->getWornAttachment(vitemp->getUUID());
			if (obj)
			{
				gSelectMgr.deselectAll();
				gSelectMgr.addAsIndividual(obj, SELECT_ALL_TES, false);
				gSelectMgr.selectionSetObjectName(new_name);
				gSelectMgr.deselectAll();
			}
		}
	}

	// Return false because we either notified observers (and therefore
	// rebuilt) or we did not update.
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLLSLTextBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLLSLTextBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Script") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLLSLTextBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SCRIPT,
									LLInventoryType::IT_LSL, 0, false);
}

//virtual
void LLLSLTextBridge::openItem()
{
	open_script(mUUID, getPrefix() + getName());
}

///////////////////////////////////////////////////////////////////////////////
// LLWearableBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
bool LLWearableBridge::renameItem(const std::string& new_name)
{
	if (gAgentWearables.isWearingItem(mUUID))
	{
		gAgentWearables.setWearableName(mUUID, new_name);
	}
	return LLItemBridge::renameItem(new_name);
}

//virtual
bool LLWearableBridge::isItemRemovable()
{
	LLInventoryModel* modelp = mInventoryPanel->getModel();
	if (!modelp)
	{
		return false;
	}

	const LLViewerInventoryItem* vitemp = modelp->getItem(mUUID);
	if (vitemp && vitemp->getIsLinkType())
	{
		return true;
	}

	if (gAgentWearables.isWearingItem(mUUID))
	{
		return false;
	}

	return LLInvFVBridge::isItemRemovable();
}

//virtual
LLFontGL::StyleFlags LLWearableBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

	if (gAgentWearables.isWearingItem(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	const LLViewerInventoryItem* vitemp = getItem();
	if (vitemp && vitemp->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

std::string LLWearableBridge::getLabelSuffix() const
{
	static const std::string worn = " (" + LLTrans::getString("worn") + ")";
	if (gAgentWearables.isWearingItem(mUUID))
	{
		return LLItemBridge::getLabelSuffix() + worn;
	}
	return LLItemBridge::getLabelSuffix();
}

//virtual
LLUIImagePtr LLWearableBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(mAssetType, mInvType, mWearableType,
									false);
}

//virtual
void LLWearableBridge::performAction(LLFolderView* folderp,
									 LLInventoryModel* modelp,
									 const std::string& action)
{
	bool agent_inventory = isAgentInventory();
	if (action == "wear")
	{
		if (agent_inventory)
		{
			wearOnAvatar();
		}
	}
	else if (action == "wear_add")
	{
		if (agent_inventory)
		{
			wearOnAvatar(false);
		}
	}
	else if (action == "edit")
	{
		if (agent_inventory)
		{
			editOnAvatar();
		}
	}
	else if (action == "take_off")
	{
		if (isAgentAvatarValid() && gAgentWearables.isWearingItem(mUUID))
		{
			LLViewerInventoryItem* vitemp = getItem();
			if (vitemp
//MK
				&& (!gRLenabled || gRLInterface.canUnwear(vitemp)))
//mk
			{
				LLWearableList* wlist = LLWearableList::getInstance();
				wlist->getAsset(vitemp->getAssetUUID(), vitemp->getName(),
								gAgentAvatarp, vitemp->getType(),
								LLWearableBridge::onRemoveFromAvatarArrived,
								new OnRemoveStruct(vitemp->getLinkedUUID()));
			}
		}
	}
	else
	{
		LLItemBridge::performAction(folderp, modelp, action);
	}
}

//virtual
void LLWearableBridge::openItem()
{
	if (isInTrash())
	{
		gNotifications.add("CannotWearTrash");
	}
	else if (gAgentWearables.isWearingItem(mUUID))
	{
		performAction(NULL, NULL, "take_off");
	}
	else if (isAgentInventory())
	{
		if (!isInMarketplace())
		{
			performAction(NULL, NULL, "wear");
		}
	}
	else
	{
		// Must be in the inventory library. Copy it to our inventory and put
		// it on right away.
		LLViewerInventoryItem* vitemp = getItem();
		if (vitemp && vitemp->isFinished())
		{
			LLPointer<LLInventoryCallback> cb = new LLWearOnAvatarCallback();
			copy_inventory_item(vitemp->getPermissions().getOwner(),
								vitemp->getUUID(), LLUUID::null,
								LLStringUtil::null, cb);
		}
		else if (vitemp)
		{
			// *TODO: We should fetch the item details, and then do
			// the operation above.
			gNotifications.add("CannotWearInfoNotComplete");
		}
	}
}

//virtual
void LLWearableBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Purge Item");
		}

		items.emplace_back("Restore Item");
	}
	else if (isInMarketplace())
	{
		items.emplace_back("Properties");
		getClipboardEntries(true, items, disabled_items, flags);
	}
	else
	{
		// FWIW, it looks like SUPPRESS_OPEN_ITEM is not set anywhere
		bool no_open = ((flags & SUPPRESS_OPEN_ITEM) == SUPPRESS_OPEN_ITEM);

		// If we have clothing, do not add "Open" as it is the same action as
		// "Wear"   SL-18976
		LLViewerInventoryItem* vitemp = getItem();
		if (!no_open && vitemp)
		{
			no_open = vitemp->getType() == LLAssetType::AT_CLOTHING ||
					  vitemp->getType() == LLAssetType::AT_BODYPART;
		}
		if (!no_open)
		{
			items.emplace_back("Open");
		}

		bool wearing = gAgentWearables.isWearingItem(mUUID);
		bool agent_inventory = isAgentInventory();
		// Allow to wear only non-library items in SSB-enabled sims
		if (wearing || agent_inventory)
		{
			if (wearing)
			{
				items.emplace_back("Edit");
				if (!agent_inventory || (flags & FIRST_SELECTED_ITEM) == 0)
				{
					disabled_items.emplace_back("Edit");
				}
			}
			else
			{
				items.emplace_back("Wearable Wear");
//MK
				if (gRLenabled && !gRLInterface.canWear(vitemp))
				{
					disabled_items.emplace_back("Wearable Wear");
				}
//mk
			}
			if (vitemp && vitemp->getType() == LLAssetType::AT_CLOTHING)
			{
				if (wearing)
				{
					items.emplace_back("Take Off");
//MK
					if (gRLenabled && !gRLInterface.canUnwear(vitemp))
					{
						disabled_items.emplace_back("Take Off");
					}
//mk
				}
				else
				{
					items.emplace_back("Wearable Add");
//MK
					if (gRLenabled && !gRLInterface.canWear(vitemp))
					{
						disabled_items.emplace_back("Wearable Add");
					}
//mk
				}
			}

			items.emplace_back("Wearable Separator");
		}

		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);

	}
	set_menu_entries_state(menu, items, disabled_items);
}

// Called from menus
//static
bool LLWearableBridge::canWearOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	return self && self->isAgentInventory() &&
		   !gAgentWearables.isWearingItem(self->mUUID);
}

// Called from menus
//static
void LLWearableBridge::onWearOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if (self)
	{
		self->wearOnAvatar();
	}
}

void LLWearableBridge::wearOnAvatar(bool replace)
{
	// Do not wear anything until initial wearables are loaded; could destroy
	// clothing items.
	if (!gAgentWearables.areWearablesLoaded())
	{
		gNotifications.add("CanNotChangeAppearanceUntilLoaded");
		return;
	}

	LLViewerInventoryItem* vitemp = getItem();
	if (vitemp)
	{
		gAppearanceMgr.wearItemOnAvatar(vitemp->getLinkedUUID(), replace);
	}
}

//static
void LLWearableBridge::onWearOnAvatarArrived(LLViewerWearable* wearable,
											 void* userdata)
{
	OnWearStruct* datap = (OnWearStruct*)userdata;
	const LLUUID& item_id = datap->mUUID;
	bool replace = datap->mReplace;

	if (wearable)
	{
		LLViewerInventoryItem* vitemp = gInventory.getItem(item_id);
		if (vitemp)
		{
			if (vitemp->getAssetUUID() == wearable->getAssetID())
			{
//MK
				bool old_restore = gRLInterface.mRestoringOutfit;
				gRLInterface.mRestoringOutfit =
					gAppearanceMgr.isRestoringInitialOutfit();
//mk
				gAgentWearables.setWearableItem(vitemp, wearable, !replace);
//MK
				gRLInterface.mRestoringOutfit = old_restore;
//mk
				gInventory.notifyObservers();
			}
			else
			{
				llinfos << "By the time wearable asset arrived, its inv item already pointed to a different asset."
						<< llendl;
			}
		}
	}
	delete datap;
}

//static
bool LLWearableBridge::canEditOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	return self && gAgentWearables.isWearingItem(self->mUUID);
}

//static
void LLWearableBridge::onEditOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if (self)
	{
		self->editOnAvatar();
	}
}

// *TODO: implement v3's way and allow wear & edit
void LLWearableBridge::editOnAvatar()
{
	const LLUUID& linked_id = gInventory.getLinkedItemID(mUUID);
	LLViewerWearable* wearable =
		gAgentWearables.getWearableFromItemID(linked_id);
	if (wearable)
	{
		// Set the tab to the right wearable.
		LLFloaterCustomize::setCurrentWearableType(wearable->getType());

		if (gAgent.getCameraMode() != CAMERA_MODE_CUSTOMIZE_AVATAR)
		{
			// Start Avatar Customization
			gAgent.changeCameraToCustomizeAvatar();
		}
	}
}

//static
bool LLWearableBridge::canRemoveFromAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	return self && self->mAssetType != LLAssetType::AT_BODYPART &&
		   gAgentWearables.isWearingItem(self->mUUID);
}

//static
void LLWearableBridge::onRemoveFromAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if (self && isAgentAvatarValid() &&
		gAgentWearables.isWearingItem(self->mUUID))
	{
		LLViewerInventoryItem* vitemp = self->getItem();
		if (vitemp)
		{
			LLWearableList* wlist = LLWearableList::getInstance();
			wlist->getAsset(vitemp->getAssetUUID(), vitemp->getName(),
							gAgentAvatarp, vitemp->getType(),
							onRemoveFromAvatarArrived,
							new OnRemoveStruct(LLUUID(self->mUUID)));
		}
	}
}

//static
void LLWearableBridge::onRemoveFromAvatarArrived(LLViewerWearable* wearable,
												 void* userdata)
{
	OnRemoveStruct* on_remove_struct = (OnRemoveStruct*)userdata;
	if (!on_remove_struct) return;	// Paranoia

	const LLUUID& item_id =
		gInventory.getLinkedItemID(on_remove_struct->mUUID);
	if (wearable)
	{
		if (get_is_item_worn(item_id))
		{
			LLWearableType::EType type = wearable->getType();
			U32 index;
			if (gAgentWearables.getWearableIndex(wearable, index))
			{
				gAgentWearables.userRemoveWearable(type, index);
				gInventory.notifyObservers();
			}
		}
	}

	delete on_remove_struct;
}

///////////////////////////////////////////////////////////////////////////////
// LLLinkItemBridge
///////////////////////////////////////////////////////////////////////////////
// For broken links

//virtual
const std::string& LLLinkItemBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Link") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLLinkItemBridge::getIcon() const
{
	if (LLViewerInventoryItem* vitemp = getItem())
	{
		// Low byte of inventory flags
		U32 attachment_point = vitemp->getFlags() & 0xff;

		bool is_multi = (LLInventoryItem::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS &
						 vitemp->getFlags()) != 0;

		return LLInventoryIcon::getIcon(vitemp->getActualType(),
										vitemp->getInventoryType(),
										attachment_point, is_multi);
	}
	return LLInventoryIcon::getIcon(LLAssetType::AT_LINK,
									LLInventoryType::IT_NONE, 0, false);
}

//virtual
void LLLinkItemBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	items.emplace_back("Find Original");
	disabled_items.emplace_back("Find Original");

	if (isInTrash())
	{
		disabled_items.emplace_back("Find Original");
		if (isLinkedObjectMissing())
		{
			disabled_items.emplace_back("Find Original");
		}
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Properties");
		items.emplace_back("Find Original");
		if (isLinkedObjectMissing())
		{
			disabled_items.emplace_back("Find Original");
		}
		items.emplace_back("Delete");
	}
	set_menu_entries_state(menu, items, disabled_items);
}

#if LL_MESH_ASSET_SUPPORT
///////////////////////////////////////////////////////////////////////////////
// LLMeshBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLMeshBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Mesh") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLMeshBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_MESH,
									LLInventoryType::IT_MESH, 0, false);
}

//virtual
void LLMeshBridge::openItem()
{
}

//virtual
void LLMeshBridge::previewItem()
{
}

//virtual
void LLMeshBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLMeshBridge::performAction(LLFolderView* folderp,
								 LLInventoryModel* modelp,
								 const std::string& action)
{
	LLItemBridge::performAction(folderp, modelp, action);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// LLSettingsBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLSettingsBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Settings") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLSettingsBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_SETTINGS,
									LLInventoryType::IT_SETTINGS,
									mSettingsType, false);
}

//virtual
LLFontGL::StyleFlags LLSettingsBridge::getLabelStyle() const
{
	S32 font = LLFontGL::NORMAL;

#if	0 // *TODO: use bold font when settings active
	if ()
	{
		font |= LLFontGL::BOLD;
	}
#endif

	const LLViewerInventoryItem* item = getItem();
	if (item && item->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

//virtual
void LLSettingsBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item && item->getPermissions().getOwner() == gAgentID)
	{
		HBFloaterEditEnvSettings* floaterp =
			HBFloaterEditEnvSettings::show(mUUID);
		if (floaterp)
		{
			floaterp->setEditContextInventory();
		}
	}
	else
	{
		gNotifications.add("NoEditFromLibrary");
	}
}

//virtual
void LLSettingsBridge::previewItem()
{
	openItem();
}

//virtual
void LLSettingsBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
		if (!gAgent.hasInventorySettings())
		{
			disabled_items.emplace_back("Restore Item");
		}
	}
	else if (gAgent.hasInventorySettings())
	{
		items.emplace_back("Settings Open");
		items.emplace_back("Properties");

		getClipboardEntries(true, items, disabled_items, flags);

		items.emplace_back("Setings Separator");
		items.emplace_back("Apply Local");
		items.emplace_back("Apply Parcel");
		if (!LLEnvironment::canAgentUpdateParcelEnvironment())
		{
			disabled_items.emplace_back("Apply Parcel");
		}
		items.emplace_back("Apply Region");
		if (!LLEnvironment::canAgentUpdateRegionEnvironment())
		{
			disabled_items.emplace_back("Apply Region");
		}
	}
	else
	{
		items.emplace_back("Properties");
		disabled_items.emplace_back("Properties");
		items.emplace_back("Delete");
		if (!isItemRemovable())
		{
			disabled_items.emplace_back("Delete");
		}
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLSettingsBridge::performAction(LLFolderView* folderp,
									 LLInventoryModel* modelp,
									 const std::string& action)
{
	if (action == "apply_settings_local")
	{
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			LLUUID asset_id = item->getAssetUUID();
			gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, asset_id,
										LLEnvironment::TRANSITION_INSTANT);
			gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
												LLEnvironment::TRANSITION_INSTANT);
			if (gAutomationp)
			{
				LLSettingsType::EType type = item->getSettingsType();
				const std::string& name = item->getName();
				if (type == LLSettingsType::ST_SKY)
				{
					gAutomationp->onWindlightChange(name, "", "");
				}
				else if (type == LLSettingsType::ST_WATER)
				{
					gAutomationp->onWindlightChange("", name, "");
				}
				else if (type == LLSettingsType::ST_DAYCYCLE)
				{
					gAutomationp->onWindlightChange("", "", name);
				}
			}
		}
	}
	else if (action == "apply_settings_parcel")
	{
		LLViewerInventoryItem* item = getItem();
		if (!item) return;

		LLUUID asset_id = item->getAssetUUID();
		std::string name = item->getName();
		LLParcel* parcelp = gViewerParcelMgr.getSelectedOrAgentParcel();
		if (!parcelp)
		{
			llwarns << "Could not find any selected or agent parcel. Aborted."
					<< llendl;
			return;
		}

		if (!LLEnvironment::canAgentUpdateParcelEnvironment(parcelp))
		{
			gNotifications.add("WLParcelApplyFail");
			return;
		}

		S32 parcel_id = parcelp->getLocalID();
		LL_DEBUGS("Environment") << "Applying environment settings asset Id "
								 << asset_id << " to parcel " << parcel_id
								 << LL_ENDL;

		U32 flags = 0;
		const LLPermissions& perms = item->getPermissions();
		if (!perms.allowModifyBy(gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOMOD;
		}
		if (!perms.allowTransferBy(gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOTRANS;
		}

		gEnvironment.updateParcel(parcel_id, asset_id, name,
								  LLEnvironment::NO_TRACK, -1, -1, flags);
		gEnvironment.setSharedEnvironment();
	}
	else if (action == "apply_settings_region")
	{
		LLViewerInventoryItem* item = getItem();
		if (!item) return;

		if (!LLEnvironment::canAgentUpdateRegionEnvironment())
		{
			LLSD args;
			args["FAIL_REASON"] = LLTrans::getString("no_permission");
			gNotifications.add("WLRegionApplyFail", args);
			return;
		}

		U32 flags = 0;
		const LLPermissions& perms = item->getPermissions();
		if (!perms.allowModifyBy(gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOMOD;
		}
		if (!perms.allowTransferBy(gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOTRANS;
		}

		gEnvironment.updateRegion(item->getAssetUUID(), item->getName(),
								  LLEnvironment::NO_TRACK, -1, -1, flags);
	}
	else if (action == "open")
	{
		openItem();
	}
	else
	{
		LLItemBridge::performAction(folderp, modelp, action);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLMaterialBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLMaterialBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Material") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLMaterialBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_MATERIAL,
									LLInventoryType::IT_MATERIAL, 0, false);
}

//virtual
void LLMaterialBridge::openItem()
{
	open_material(mUUID, getName());
}

//virtual
void LLMaterialBridge::previewItem()
{
	open_material(mUUID, getName());
}

//virtual
void LLMaterialBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
		if (!gAgent.hasInventoryMaterial())
		{
			disabled_items.emplace_back("Restore Item");
		}
	}
	else if (gAgent.hasInventoryMaterial())
	{
		items.emplace_back("Properties");
		items.emplace_back("Edit");
		bool disable_edit = (flags & FIRST_SELECTED_ITEM) == 0;
		if (disable_edit)
		{
			disabled_items.emplace_back("Edit");
		}
		getClipboardEntries(true, items, disabled_items, flags);
	}
	else
	{
		items.emplace_back("Properties");
		disabled_items.emplace_back("Properties");
		items.emplace_back("Delete");
	}

	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLMaterialBridge::performAction(LLFolderView* folderp,
									 LLInventoryModel* modelp,
									 const std::string& action)
{
	if (action == "edit")
	{
		openItem();
		return;
	}
	LLItemBridge::performAction(folderp, modelp, action);
}

///////////////////////////////////////////////////////////////////////////////
// LLLinkFolderBridge
///////////////////////////////////////////////////////////////////////////////

//virtual
const std::string& LLLinkFolderBridge::getPrefix()
{
	static const std::string prefix = LLTrans::getString("Link") + ": ";
	return prefix;
}

//virtual
LLUIImagePtr LLLinkFolderBridge::getIcon() const
{
	return LLUI::getUIImage("inv_link_folder.tga");
}

//virtual
void LLLinkFolderBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	items.emplace_back("Find Original");
	if (isLinkedObjectMissing())
	{
		disabled_items.emplace_back("Find Original");
	}
	if (isInTrash())
	{
		items.emplace_back("Purge Item");
		items.emplace_back("Restore Item");
	}
	else
	{
		items.emplace_back("Delete");
	}
	set_menu_entries_state(menu, items, disabled_items);
}

//virtual
void LLLinkFolderBridge::performAction(LLFolderView* folderp,
									   LLInventoryModel* modelp,
									   const std::string& action)
{
	if (action == "goto")
	{
		gotoItem(folderp);
		return;
	}
	LLItemBridge::performAction(folderp, modelp, action);
}

//virtual
void LLLinkFolderBridge::gotoItem(LLFolderView* folderp)
{
	const LLUUID& cat_uuid = getFolderID();
	if (cat_uuid.notNull())
	{
		if (LLFolderViewItem* base_folderp = folderp->getItemByID(cat_uuid))
		{
			if (LLInventoryModel* modelp = mInventoryPanel->getModel())
			{
				modelp->fetchDescendentsOf(cat_uuid);
			}
			base_folderp->setOpen(true);
			folderp->setSelectionFromRoot(base_folderp, true);
			folderp->scrollToShowSelection();
		}
	}
}

const LLUUID& LLLinkFolderBridge::getFolderID() const
{
	if (LLViewerInventoryItem* link_item = getItem())
	{
		const LLViewerInventoryCategory* cat = link_item->getLinkedCategory();
		if (cat)
		{
			const LLUUID& cat_uuid = cat->getUUID();
			return cat_uuid;
		}
	}
	return LLUUID::null;
}
