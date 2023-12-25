/**
 * @file llfloaterbuy.cpp
 * @author James Cook
 * @brief LLFloaterBuy class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

/**
 * Floater that appears when buying an object, giving a preview of its contents
 * and their permissions.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterbuy.h"

#include "llalertdialog.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"			// For gAgentID
#include "llinventoryicon.h"	// For getIconName
#include "llinventorymodel.h"	// For gInventory
#include "llselectmgr.h"
#include "llviewerobject.h"

LLFloaterBuy::LLFloaterBuy(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_buy_object.xml");
}

//virtual
LLFloaterBuy::~LLFloaterBuy()
{
	// Drop reference to current selection so that it goes away
	mObjectSelection = NULL;
}

//virtual
bool LLFloaterBuy::postBuild()
{
	mObjectsList = getChild<LLScrollListCtrl>("object_list");
	mObjectsList->setEnabled(false);
	mItemsList = getChild<LLScrollListCtrl>("item_list");
	mItemsList->setEnabled(false);

	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("buy_btn", onClickBuy, this);

	setDefaultBtn("cancel_btn");	// To avoid accidental buy (SL-43130)

	return true;
}

void LLFloaterBuy::reset()
{
	mObjectsList->deleteAllItems();
	mItemsList->deleteAllItems();
}

//static
void LLFloaterBuy::show(const LLSaleInfo& sale_info)
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->getRootObjectCount() != 1)
	{
		gNotifications.add("BuyOneObjectOnly");
		return;
	}

	// Create a new instance only if needed
	LLFloaterBuy* self = getInstance();

	// Clean up the lists...
	self->reset();

	self->open();
	self->setFocus(true);
	self->mSaleInfo = sale_info;
	self->mObjectSelection = gSelectMgr.getEditSelection();

	// Always center the dialog.  User can change the size, but purchases are
	// important and should be center screen. This also avoids problems where
	// the user resizes the application window mid-session and the saved rect
	// is off-center.
	self->center();

	LLSelectNode* node = selection->getFirstRootNode();
	if (!node) return;

	// Set title based on sale type
	LLUIString title;
	switch (sale_info.getSaleType())
	{
		case LLSaleInfo::FS_ORIGINAL:
			title = self->getString("title_buy_text");
			break;

		case LLSaleInfo::FS_COPY:
		default:
			title = self->getString("title_buy_copy_text");
	}
	title.setArg("[NAME]", node->mName);
	self->setTitle(title);

	LLUUID owner_id;
	std::string owner_name;
	bool owners_identical = gSelectMgr.selectGetOwner(owner_id, owner_name);
	if (!owners_identical)
	{
		gNotifications.add("BuyObjectOneOwner");
		return;
	}

	// Update the display
	// Display next owner permissions
	LLSD row;

	// Compute icon for this item
	std::string icon_name =
		LLInventoryIcon::getIconName(LLAssetType::AT_OBJECT,
									 LLInventoryType::IT_OBJECT, 0x0, false);
	row["columns"][0]["column"] = "icon";
	row["columns"][0]["type"] = "icon";
	row["columns"][0]["value"] = icon_name;

	// Append the permissions that you will acquire (not the current
	// permissions).
	U32 next_owner_mask = node->mPermissions->getMaskNextOwner();
	std::string text = node->mName;
	if (!(next_owner_mask & PERM_COPY))
	{
		text.append(self->getString("no_copy_text"));
	}
	if (!(next_owner_mask & PERM_MODIFY))
	{
		text.append(self->getString("no_modify_text"));
	}
	if (!(next_owner_mask & PERM_TRANSFER))
	{
		text.append(self->getString("no_transfer_text"));
	}

	row["columns"][1]["column"] = "text";
	row["columns"][1]["value"] = text;
	row["columns"][1]["font"] = "SANSSERIF";

	// Add after columns added so appropriate heights are correct.
	self->mObjectsList->addElement(row);

	self->childSetTextArg("buy_text", "[AMOUNT]",
						  llformat("%d", sale_info.getSalePrice()));
	self->childSetTextArg("buy_text", "[NAME]", owner_name);

	// Must do this after the floater is created, because sometimes the
	// inventory is already there and the callback is called immediately.
	LLViewerObject* obj = selection->getFirstRootObject();
	if (obj)
	{
		self->registerVOInventoryListener(obj, NULL);
		self->requestVOInventory();
	}
}

void LLFloaterBuy::inventoryChanged(LLViewerObject* obj,
									LLInventoryObject::object_list_t* inv,
									S32 serial_num, void* data)
{
	if (!obj)
	{
		llwarns << "No object !" << llendl;
		return;
	}

	if (!inv)
	{
		llwarns << "No inventory !" << llendl;
		removeVOInventoryListener();
		return;
	}

	std::string icon_name;
	for (LLInventoryObject::object_list_t::const_iterator it = inv->begin(),
														  end = inv->end();
		 it != end; ++it)
	{
		LLInventoryObject* obj = *it;

		// Skip folders, so we know we have inventory items only and also skip
		// the mysterious blank InventoryObject
		if (!obj || obj->getType() == LLAssetType::AT_CATEGORY ||
			obj->getType() == LLAssetType::AT_NONE)
		{
			continue;
		}

		LLInventoryItem* inv_item = (LLInventoryItem*)obj;
		// Skip items we can't transfer
		if (!inv_item->getPermissions().allowTransferTo(gAgentID))
		{
			continue;
		}

		// Create the line in the list
		LLSD row;

		// Compute icon for this item
		bool item_is_multi = false;
		if ((inv_item->getFlags() & LLInventoryItem::II_FLAGS_LANDMARK_VISITED) ||
			(inv_item->getFlags() & LLInventoryItem::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS))
		{
			item_is_multi = true;
		}

		icon_name = LLInventoryIcon::getIconName(inv_item->getType(),
												 inv_item->getInventoryType(),
							 					 inv_item->getFlags(),
							 					 item_is_multi);
		row["columns"][0]["column"] = "icon";
		row["columns"][0]["type"] = "icon";
		row["columns"][0]["value"] = icon_name;

		// Append the permissions that you will acquire (not the current
		// permissions).
		U32 next_owner_mask = inv_item->getPermissions().getMaskNextOwner();
		std::string text = obj->getName();
		if (!(next_owner_mask & PERM_COPY))
		{
			text.append(" (no copy)");
		}
		if (!(next_owner_mask & PERM_MODIFY))
		{
			text.append(" (no modify)");
		}
		if (!(next_owner_mask & PERM_TRANSFER))
		{
			text.append(" (no transfer)");
		}

		row["columns"][1]["column"] = "text";
		row["columns"][1]["value"] = text;
		row["columns"][1]["font"] = "SANSSERIF";

		mItemsList->addElement(row);
	}

	removeVOInventoryListener();
}

//static
void LLFloaterBuy::onClickBuy(void* data)
{
	LLFloaterBuy* self = (LLFloaterBuy*)data;
	if (self)
	{
		// Put the items where we put new folders.
		LLUUID category_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_OBJECT);

		// *NOTE: does not work for multiple objects buy, which UI does not
		// currently support sale info is used for verification only, if it
		// does not match region info then the sale is cancelled.
		gSelectMgr.sendBuy(gAgentID, category_id, self->mSaleInfo);

		self->close();
	}
}

//static
void LLFloaterBuy::onClickCancel(void* data)
{
	LLFloaterBuy* self = (LLFloaterBuy*)data;
	if (self)
	{
		self->close();
	}
}
