/**
 * @file llpreview.cpp
 * @brief LLPreview class implementation
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

#include "llpreview.h"

#include "llassetstorage.h"
#include "lldbstrings.h"
#include "llinventory.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "lltextbox.h"

#include "llagent.h"
#include "llfloatersearchreplace.h"
#include "llinventorymodel.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llselectmgr.h"
#include "lltooldraganddrop.h"
#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"
#include "roles_constants.h"

// Static members
LLPreview::preview_map_t LLPreview::sInstances;
LLMultiPreview::handle_map_t LLMultiPreview::sAutoOpenPreviewHandles;

LLPreview::LLPreview(const std::string& name)
:	LLFloater(name),
	mCopyToInvBtn(NULL),
	mForceClose(false),
	mUserResized(false),
	mCloseAfterSave(false),
	mSaveDialogShown(false),
	mAssetStatus(PREVIEW_ASSET_UNLOADED),
	mItem(NULL),
	mDirty(true)
{
	// Do not add to instance list, since Item Id is null
	mAuxItem = new LLInventoryItem;	// (LLPointer is auto-deleted)
	// Do not necessarily steal focus on creation; sometimes these guys pop up
	// without user action.
	setAutoFocus(false);
	gInventory.addObserver(this);
}

LLPreview::LLPreview(const std::string& name, const LLRect& rect,
					 const std::string& title, const LLUUID& item_id,
					 const LLUUID& object_id, bool allow_resize,
					 S32 min_width, S32 min_height,
					 LLPointer<LLViewerInventoryItem> inv_item)
:	LLFloater(name, rect, title, allow_resize, min_width, min_height),
	mItemUUID(item_id),
	mObjectUUID(object_id),
	mCopyToInvBtn(NULL),
	mForceClose(false),
	mUserResized(false),
	mCloseAfterSave(false),
	mSaveDialogShown(false),
	mAssetStatus(PREVIEW_ASSET_UNLOADED),
	mItem(inv_item),
	mDirty(true)
{
	mAuxItem = new LLInventoryItem;
	// Do not necessarily steal focus on creation; sometimes these guys pop up
	// without user action.
	setAutoFocus(false);

	if (mItemUUID.notNull())
	{
		sInstances[mItemUUID] = this;
	}
	gInventory.addObserver(this);
}

//virtual
LLPreview::~LLPreview()
{
	gFocusMgr.releaseFocusIfNeeded(this); // Calls onCommit()

	if (mItemUUID.notNull())
	{
		sInstances.erase(mItemUUID);
	}

	gInventory.removeObserver(this);
}

void LLPreview::setItemID(const LLUUID& item_id)
{
	if (mItemUUID.notNull())
	{
		sInstances.erase(mItemUUID);
	}

	mItemUUID = item_id;

	if (mItemUUID.notNull())
	{
		sInstances[mItemUUID] = this;
	}
}

const LLViewerInventoryItem* LLPreview::getItem() const
{
	if (mItem)
	{
		return mItem;
	}

	if (mObjectUUID.isNull())
	{
		// It is an inventory item, so get the item.
		return gInventory.getItem(mItemUUID);
	}
	// It is an object's inventory item.
	LLViewerObject* objp = gObjectList.findObject(mObjectUUID);
	if (!objp)
	{
		return NULL;
	}
	return (const LLViewerInventoryItem*)objp->getInventoryObject(mItemUUID);
}

// Sub-classes should override this function if they allow editing
void LLPreview::onCommit()
{
	const LLViewerInventoryItem* old_itemp = getItem();
	if (!old_itemp)
	{
		return;
	}
	if (!old_itemp->isFinished())
	{
		// We are attempting to save an item that was never loaded
		llwarns << "Call done for an unfinished asset - Type: "
				<< old_itemp->getType() << " - ID: " << old_itemp->getUUID()
				<< llendl;
		return;
	}

	LLPointer<LLViewerInventoryItem> itemp =
		new LLViewerInventoryItem(old_itemp);

	std::string desc;
	LLView* viewp = getChild<LLView>("desc", true, false);
	if (viewp)
	{
		desc = viewp->getValue().asString();
	}
	itemp->setDescription(desc);

	if (mObjectUUID.notNull())
	{
		// Must be in an object
		LLViewerObject* objectp = gObjectList.findObject(mObjectUUID);
		if (objectp)
		{
			objectp->updateInventory(itemp);
		}
	}
	else if (old_itemp->getPermissions().getOwner() == gAgentID)
	{
		itemp->updateServer(false);
		gInventory.updateItem(itemp);
		gInventory.notifyObservers();

		// If the old item is an attachment that is currently being worn,
		// update the object itself.
		if (old_itemp->getType() == LLAssetType::AT_OBJECT &&
			isAgentAvatarValid())
		{
			LLViewerObject* objectp =
				gAgentAvatarp->getWornAttachment(old_itemp->getUUID());
			if (objectp)
			{
				gSelectMgr.deselectAll();
				gSelectMgr.addAsIndividual(objectp, SELECT_ALL_TES, false);
				gSelectMgr.selectionSetObjectDescription(desc);
				gSelectMgr.deselectAll();
			}
		}
	}
}

void LLPreview::changed(U32 mask)
{
	mDirty = true;
}

//virtual
void LLPreview::draw()
{
	LLFloater::draw();
	if (mDirty)
	{
		mDirty = false;
		refreshFromItem();
	}
}

//virtual
void LLPreview::setAuxItem(const LLInventoryItem* itemp)
{
	if (mAuxItem)
	{
		mAuxItem->copyItem(itemp);
	}
}

void LLPreview::setNotecardInfo(const LLUUID& notecard_inv_id,
								const LLUUID& object_id)
{
	mNotecardInventoryID = notecard_inv_id;
	mObjectID = object_id;
}

//virtual
void LLPreview::refreshFromItem()
{
	const LLViewerInventoryItem* item = getItem();
	if (!item) return;

	setTitle(llformat("%s: %s", getTitleName(), item->getName().c_str()));
	if (getChild<LLView>("desc", true, false))
	{
		childSetText("desc", item->getDescription());
		childSetEnabled("desc", canModify(mObjectUUID, item));
	}
}

bool LLPreview::canModify(const LLUUID& task_id, const LLInventoryItem* itemp)
{
	if (task_id.notNull())
	{
		LLViewerObject* objectp = gObjectList.findObject(task_id);
		if (objectp && !objectp->permModify())
		{
			// No permission to edit in-world inventory
			return false;
		}
	}

	return itemp && gAgent.allowOperation(PERM_MODIFY, itemp->getPermissions(),
										  GP_OBJECT_MANIPULATE);
}

//static
void LLPreview::onText(LLUICtrl*, void* userdata)
{
	LLPreview* self = (LLPreview*) userdata;
	self->onCommit();
}

//static
void LLPreview::onRadio(LLUICtrl*, void* userdata)
{
	LLPreview* self = (LLPreview*) userdata;
	self->onCommit();
}

//static
LLPreview* LLPreview::find(const LLUUID& item_id)
{
	preview_map_t::iterator it = sInstances.find(item_id);
	return it != sInstances.end() ? it->second : NULL;
}

//static
LLPreview* LLPreview::show(const LLUUID& item_id, bool take_focus)
{
	LLPreview* self = find(item_id);
	if (self)
	{
		LLMultiFloater* floaterp = LLFloater::getFloaterHost();
		if (floaterp && floaterp != self->getHost())
		{
			// This preview window is being opened in a new context needs to
			// be rehosted
			floaterp->addFloater(self, true);
		}
		self->open();
		if (take_focus)
		{
			self->setFocus(true);
		}
	}
	return self;
}

//static
bool LLPreview::save(const LLUUID& item_id, LLPointer<LLInventoryItem>* itempp)
{
	bool res = false;
	LLPreview* self = find(item_id);
	if (self)
	{
		res = self->saveItem(itempp);
	}
	if (!res)
	{
		delete itempp;
	}
	return res;
}

//static
void LLPreview::hide(const LLUUID& item_id, bool no_saving)
{
	preview_map_t::iterator it = sInstances.find(item_id);
	if (it != sInstances.end())
	{
		LLPreview* self = it->second;

		if (no_saving)
		{
			self->mForceClose = true;
		}

		self->close();
	}
}

//static
void LLPreview::rename(const LLUUID& item_id, const std::string& new_name)
{
	preview_map_t::iterator it = sInstances.find(item_id);
	if (it != sInstances.end())
	{
		it->second->setTitle(new_name);
	}
}

bool LLPreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mClientRect.pointInRect(x, y))
	{
		// No handler needed for focus lost since this class has no state that
		// depends on it.
		bringToFront(x, y);
		gFocusMgr.setMouseCapture(this);
		S32 screen_x;
		S32 screen_y;
		localPointToScreen(x, y, &screen_x, &screen_y);
		gToolDragAndDrop.setDragStart(screen_x, screen_y);
		return true;
	}
	return LLFloater::handleMouseDown(x, y, mask);
}

bool LLPreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
		return true;
	}
	return LLFloater::handleMouseUp(x, y, mask);
}

bool LLPreview::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		S32 screen_x;
		S32 screen_y;
		const LLViewerInventoryItem* item = getItem();

		localPointToScreen(x, y, &screen_x, &screen_y);
		if (item && item->getPermissions().allowCopyBy(gAgentID,
													   gAgent.getGroupID()) &&
			gToolDragAndDrop.isOverThreshold(screen_x, screen_y))
		{
			EDragAndDropType type =
				LLAssetType::lookupDragAndDropType(item->getType());
			LLToolDragAndDrop::ESource src = LLToolDragAndDrop::SOURCE_LIBRARY;
			if (mObjectUUID.notNull())
			{
				src = LLToolDragAndDrop::SOURCE_WORLD;
			}
			else if (item->getPermissions().getOwner() == gAgentID)
			{
				src = LLToolDragAndDrop::SOURCE_AGENT;
			}
			gToolDragAndDrop.beginDrag(type, item->getUUID(), src,
									   mObjectUUID);
			return gToolDragAndDrop.handleHover(x, y, mask);
		}
	}
	return LLFloater::handleHover(x, y, mask);
}

void LLPreview::open()
{
	if (!getFloaterHost() && !getHost() &&
		getAssetStatus() == PREVIEW_ASSET_UNLOADED)
	{
		loadAsset();
	}
	LLFloater::open();
}

// virtual
bool LLPreview::saveItem(LLPointer<LLInventoryItem>* itemptr)
{
	return false;
}

//static
void LLPreview::onBtnCopyToInv(void* userdata)
{
	LLPreview* self = (LLPreview*) userdata;
	LLInventoryItem *item = self->mAuxItem;

	if (item && item->getUUID().notNull())
	{
		// Copy to inventory
		if (self->mNotecardInventoryID.notNull())
		{
			copy_inventory_from_notecard(self->mObjectID,
										 self->mNotecardInventoryID,
										 item);
		}
		else
		{
			copy_inventory_item(item->getPermissions().getOwner(),
								item->getUUID(), LLUUID::null);
		}
	}
	self->close();
}

//static
void LLPreview::onKeepBtn(void* data)
{
	LLPreview* self = (LLPreview*)data;
	self->close();
}

//static
void LLPreview::onDiscardBtn(void* data)
{
	LLPreview* self = (LLPreview*)data;

	const LLViewerInventoryItem* item = self->getItem();
	if (!item) return;

	self->mForceClose = true;
	self->close();

	// Move the item to the trash
	const LLUUID& trash_id = gInventory.getTrashID();
	if (item->getParentUUID() != trash_id)
	{
		LLInventoryModel::update_list_t update;
		update.emplace_back(item->getParentUUID(), -1);
		update.emplace_back(trash_id, 1);
		gInventory.accountForUpdate(update);

		LLPointer<LLViewerInventoryItem> new_item =
			new LLViewerInventoryItem(item);
		new_item->setParent(trash_id);
		// No need to restamp it though it is a move into trash because it is a
		// brand new item already.
		new_item->updateParentOnServer(false);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
	}
}

void LLPreview::userSetShape(const LLRect& new_rect)
{
	if (new_rect.getWidth() != getRect().getWidth() ||
		new_rect.getHeight() != getRect().getHeight())
	{
		userResized();
	}
	LLFloater::userSetShape(new_rect);
}

//
// LLMultiPreview
//

LLMultiPreview::LLMultiPreview(const LLRect& rect)
:	LLMultiFloater(std::string("Preview"), rect)
{
	setCanResize(true);
}

void LLMultiPreview::open()
{
	LLMultiFloater::open();
	LLPreview* frontmost_preview = (LLPreview*)mTabContainer->getCurrentPanel();
	if (frontmost_preview &&
		frontmost_preview->getAssetStatus() == LLPreview::PREVIEW_ASSET_UNLOADED)
	{
		frontmost_preview->loadAsset();
	}
}

void LLMultiPreview::userSetShape(const LLRect& new_rect)
{
	if (new_rect.getWidth() != getRect().getWidth() ||
		new_rect.getHeight() != getRect().getHeight())
	{
		LLPreview* frontmost_preview = (LLPreview*)mTabContainer->getCurrentPanel();
		if (frontmost_preview) frontmost_preview->userResized();
	}
	LLFloater::userSetShape(new_rect);
}

void LLMultiPreview::tabOpen(LLFloater* opened_floater, bool from_click)
{
	LLPreview* opened_preview = (LLPreview*)opened_floater;
	if (opened_preview &&
		opened_preview->getAssetStatus() == LLPreview::PREVIEW_ASSET_UNLOADED)
	{
		opened_preview->loadAsset();
	}

	LLFloater* search_floater = LLFloaterSearchReplace::findInstance();
	if (search_floater && search_floater->getDependee() == this)
	{
		LLPreviewNotecard* notecard_preview;
		LLPreviewScript* script_preview;
		if ((notecard_preview = dynamic_cast<LLPreviewNotecard*>(opened_preview)) != NULL)
		{
			LLFloaterSearchReplace::show(notecard_preview->getEditor());
		}
		else if ((script_preview = dynamic_cast<LLPreviewScript*>(opened_preview)) != NULL)
		{
			LLFloaterSearchReplace::show(script_preview->getEditor());
		}
		else
		{
			search_floater->setVisible(false);
		}
	}
}

//static
LLMultiPreview* LLMultiPreview::getAutoOpenInstance(const LLUUID& id)
{
	handle_map_t::iterator found_it = sAutoOpenPreviewHandles.find(id);
	if (found_it != sAutoOpenPreviewHandles.end())
	{
		return (LLMultiPreview*)found_it->second.get();
	}
	return NULL;
}

//static
void LLMultiPreview::setAutoOpenInstance(LLMultiPreview* previewp, const LLUUID& id)
{
	if (previewp)
	{
		sAutoOpenPreviewHandles[id] = previewp->getHandle();
	}
}

void LLPreview::setAssetId(const LLUUID& asset_id)
{
	const LLViewerInventoryItem* item = getItem();
	if (!item)
	{
		return;
	}

	if (mObjectUUID.isNull())
	{
		// Update avatar inventory asset_id.
		LLPointer<LLViewerInventoryItem> new_item =
			new LLViewerInventoryItem(item);
		new_item->setAssetUUID(asset_id);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
	}
	else
	{
		// Update object inventory asset_id.
		LLViewerObject* object = gObjectList.findObject(mObjectUUID);
		if (NULL == object)
		{
			llwarns << "Call done with unrecognized object, UUID: "
					<< mObjectUUID << llendl;
			return;
		}
		object->updateViewerInventoryAsset(item, asset_id);
	}
}
