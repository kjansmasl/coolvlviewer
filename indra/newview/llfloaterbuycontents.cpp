/**
 * @file llfloaterbuycontents.cpp
 * @author James Cook
 * @brief LLFloaterBuyContents class implementation
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
 * Shows the contents of an object and their permissions when you click
 * "Buy..." on an object with "Sell Contents" checked.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterbuycontents.h"

#include "llalertdialog.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"			// For gAgentID
#include "llinventoryicon.h"	// For getIconName
#include "llinventorymodel.h"	// For gInventory
#include "llselectmgr.h"
#include "llviewerobject.h"
#include "llviewerregion.h"

LLFloaterBuyContents::LLFloaterBuyContents(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_buy_contents.xml");
}

//virtual
LLFloaterBuyContents::~LLFloaterBuyContents()
{
	// Drop reference to current selection so that it goes away
	mObjectSelection = NULL;
}

//virtual
bool LLFloaterBuyContents::postBuild()
{
	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("buy_btn", onClickBuy, this);

	childDisable("item_list");
	childDisable("buy_btn");
	childDisable("wear_check");

	setDefaultBtn("cancel_btn"); // To avoid accidental buy (SL-43130)

	return true;
}

//static
void LLFloaterBuyContents::show(const LLSaleInfo& sale_info)
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->getRootObjectCount() != 1)
	{
		gNotifications.add("BuyContentsOneOnly");
		return;
	}

	// Create a new instance only if needed
	LLFloaterBuyContents* self = getInstance();

	LLScrollListCtrl* list = self->getChild<LLScrollListCtrl>("item_list");
	list->deleteAllItems();

	self->open();
	self->setFocus(true);
	self->mObjectSelection = gSelectMgr.getEditSelection();

	// Always center the dialog. User can change the size, but purchases are
	// important and should be center screen. This also avoids problems where
	// the user resizes the application window mid-session and the saved rect
	// is off-center.
	self->center();

	LLUUID owner_id;
	std::string owner_name;
	bool owners_identical = gSelectMgr.selectGetOwner(owner_id, owner_name);
	if (!owners_identical)
	{
		gNotifications.add("BuyContentsOneOwner");
		return;
	}

	self->mSaleInfo = sale_info;

	// Update the display
	LLSelectNode* node = selection->getFirstRootNode();
	if (!node) return;

	if (node->mPermissions->isGroupOwned() && gCacheNamep)
	{
		gCacheNamep->getGroupName(owner_id, owner_name);
	}

	self->childSetTextArg("contains_text", "[NAME]", node->mName);
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

void LLFloaterBuyContents::inventoryChanged(LLViewerObject* obj,
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

	// Default to turning off the buy button.
	childDisable("buy_btn");

	LLScrollListCtrl* item_list = getChild<LLScrollListCtrl>("item_list");

	LLUUID owner_id;
	bool is_group_owned;
	LLAssetType::EType asset_type;
	LLInventoryType::EType inv_type;
	S32 wearable_count = 0;
	std::string icon_name;
	for (LLInventoryObject::object_list_t::const_iterator it = inv->begin(),
														  end = inv->end();
		 it != end; ++it)
	{
		LLInventoryObject* obj = *it;
		if (!obj) continue;

		asset_type = obj->getType();
		// Skip folders, so we know we have inventory items only
		if (asset_type == LLAssetType::AT_CATEGORY)
		{
			continue;
		}

		LLInventoryItem* inv_item = (LLInventoryItem*)obj;
		inv_type = inv_item->getInventoryType();

		// Count clothing items for later
		if (LLInventoryType::IT_WEARABLE == inv_type)
		{
			++wearable_count;
		}

		// Skip items the object's owner can't copy (and hence cannot sell)
		if (!inv_item->getPermissions().allowTransferTo(gAgentID) ||
			!inv_item->getPermissions().allowCopyBy(owner_id, owner_id) ||
			!inv_item->getPermissions().getOwnership(owner_id, is_group_owned))
		{
			continue;
		}

		// There will be at least one item shown in the display, so go
		// ahead and enable the buy button.
		childEnable("buy_btn");

		// Create the line in the list
		LLSD row;

		bool item_is_multi = false;
		if (inv_item->getFlags() & LLInventoryItem::II_FLAGS_LANDMARK_VISITED)
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
			text.append(getString("no_copy_text"));
		}
		if (!(next_owner_mask & PERM_MODIFY))
		{
			text.append(getString("no_modify_text"));
		}
		if (!(next_owner_mask & PERM_TRANSFER))
		{
			text.append(getString("no_transfer_text"));
		}

		row["columns"][1]["column"] = "text";
		row["columns"][1]["value"] = text;
		row["columns"][1]["font"] = "SANSSERIF";

		item_list->addElement(row);
	}

	if (wearable_count > 0)
	{
		childEnable("wear_check");
		childSetValue("wear_check", LLSD(false));
	}

	removeVOInventoryListener();
}

//static
void LLFloaterBuyContents::onClickBuy(void* data)
{
	LLFloaterBuyContents* self = (LLFloaterBuyContents*)data;
	if (!self) return;

	// Make sure this was not selected through other mechanisms (i.e. being the
	// default button and pressing enter).
	if (!self->childIsEnabled("buy_btn"))
	{
		// We shouldn't be enabled. Just close.
		self->close();
		return;
	}

	// We may want to wear this item
	if (self->childGetValue("wear_check"))
	{
		LLInventoryModel::sWearNewClothing = true;
	}

	// Put the items where we put new folders.
	const LLUUID& category_id = gInventory.getRootFolderID();

	// *NOTE: does not work for multiple object buy, which UI does not
	// currently support sale info is used for verification only, if it does
	// not match region info then the sale is cancelled.
	gSelectMgr.sendBuy(gAgentID, category_id, self->mSaleInfo);

	self->close();
}

//static
void LLFloaterBuyContents::onClickCancel(void* data)
{
	LLFloaterBuyContents* self = (LLFloaterBuyContents*)data;
	if (self)
	{
		self->close();
	}
}
