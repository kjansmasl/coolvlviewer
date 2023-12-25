/**
 * @file llfloatermarketplace.cpp
 * @brief LLFloaterMarketplaceValidation and LLFloaterAssociateListing classes
 *
 * $LicenseInfo:firstyear=2014&license=viewergpl$
 *
 * Copyright (c) 2014, Linden Research, Inc.
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

#include "llfloatermarketplace.h"

#include "lllineeditor.h"
#include "llnotifications.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

#include "llinventorybridge.h"
#include "llmarketplacefunctions.h"
#include "llviewerinventory.h"

// static variables
LLFloaterMarketplaceValidation::instances_map_t LLFloaterMarketplaceValidation::sInstances;
LLFloaterAssociateListing::instances_map_t LLFloaterAssociateListing::sInstances;

//
// LLFloaterMarketplaceValidation class
//

//static
void LLFloaterMarketplaceValidation::show(const LLUUID& folder_id)
{
	LLFloaterMarketplaceValidation* self = NULL;

	instances_map_t::iterator it = sInstances.find(folder_id);
	if (it == sInstances.end())
	{
		self = new LLFloaterMarketplaceValidation(folder_id);
	}
	else
	{
		self = it->second;
	}

	if (self)
	{
		self->open();
		self->setFocus(true);
	}
}

LLFloaterMarketplaceValidation::LLFloaterMarketplaceValidation(const LLUUID& folder_id)
:	LLFloater(folder_id.asString()),
	mFolderId(folder_id),
	mTitleSet(false)
{
	sInstances[folder_id] = this;
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_marketplace_validation.xml");
}

LLFloaterMarketplaceValidation::~LLFloaterMarketplaceValidation()
{
	sInstances.erase(mFolderId);
}

bool LLFloaterMarketplaceValidation::postBuild()
{
	mEditor = getChild<LLTextEditor>("validation_text");
	mEditor->setEnabled(false);

	childSetAction("OK", onButtonOK, this);

	// Define a bold style for errors
	mBoldStyle = new LLStyle;
	mBoldStyle->setVisible(true);
	mBoldStyle->mBold = true;
	mBoldStyle->setFontName(LLStringUtil::null);
	mBoldStyle->setColor(LLUI::sTextFgReadOnlyColor);

	return true;
}

void LLFloaterMarketplaceValidation::onOpen()
{
	mGotMessages = false;
	mEditor->clear();

	// Validates the folder
	LLViewerInventoryCategory* cat = NULL;
	if (mFolderId.notNull())
	{
		cat = gInventory.getCategory(mFolderId);
	}
	if (cat)
	{
		std::string text = getString("scanning") + " " + cat->getName();
		mEditor->appendText(text, false, false);
		if (!mTitleSet)
		{
			text = getTitle() + " - " + getString("auditing") + " " +
				   cat->getName();
			setTitle(text);
			mTitleSet = true;
		}
		LLMarketplace::validateListings(cat,
										boost::bind(&LLFloaterMarketplaceValidation::appendMessage,
													this, _1, _2, _3), false);
		if (!mGotMessages)
		{
			// Display a no error message
			mEditor->appendText(getString("no_error"), false, true);
		}
	}
	else
	{
		mEditor->appendText(getString("null_cat"), false, false);
	}
}

void LLFloaterMarketplaceValidation::appendMessage(std::string& message,
												   S32 depth,
												   LLError::ELevel level)
{
	if (level == LLError::LEVEL_ERROR)
	{
		mEditor->appendText(message, false, true, mBoldStyle);
		mGotMessages = true;
	}
	else if (level == LLError::LEVEL_WARN)
	{
		mEditor->appendText(message, false, true);
		mGotMessages = true;
	}
}

//static
void LLFloaterMarketplaceValidation::onButtonOK(void* userdata)
{
	LLFloaterMarketplaceValidation* self = (LLFloaterMarketplaceValidation*)userdata;
	if (self)
	{
		self->close();
	}
}

//
// LLFloaterAssociateListing class
//

//static
void LLFloaterAssociateListing::show(const LLUUID& folder_id)
{
	LLFloaterAssociateListing* self = NULL;

	instances_map_t::iterator it = sInstances.find(folder_id);
	if (it == sInstances.end())
	{
		self = new LLFloaterAssociateListing(folder_id);
	}
	else
	{
		self = it->second;
	}

	if (self)
	{
		self->open();
		self->setFocus(true);
	}
}

//static
LLFloaterAssociateListing* LLFloaterAssociateListing::getInstance(const LLUUID& folder_id)
{
	instances_map_t::iterator it = sInstances.find(folder_id);
	return it == sInstances.end() ? NULL : it->second;
}

LLFloaterAssociateListing::LLFloaterAssociateListing(const LLUUID& folder_id)
:	LLFloater(folder_id.asString()),
	mFolderId(folder_id)
{
	sInstances[folder_id] = this;
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_associate_listing.xml");
}

LLFloaterAssociateListing::~LLFloaterAssociateListing()
{
	sInstances.erase(mFolderId);
}

bool LLFloaterAssociateListing::postBuild()
{
	mInputLine = getChild<LLLineEditor>("listing_id");

	childSetAction("OK", onButtonOK, this);
	childSetAction("Cancel", onButtonCancel, this);

	std::string text = getString("invalid");
	if (mFolderId.notNull())
	{
		LLViewerInventoryCategory* cat = gInventory.getCategory(mFolderId);
		if (cat)
		{
			text = getString("associating") + " " + cat->getName();
		}
	}
	childSetValue("prompt", LLSD(text));

	return true;
}

bool LLFloaterAssociateListing::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		apply();
		return true;
	}
	else if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		close();
		return true;
	}

	return LLFloater::handleKeyHere(key, mask);
}

bool apply_callback(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // yes
	{
		const LLUUID& folder_id = notification["payload"]["folder_id"].asUUID();
		LLFloaterAssociateListing* floater;
		floater = LLFloaterAssociateListing::getInstance(folder_id);
		if (floater)
		{
			floater->apply(false);
		}
	}
	return false;
}

void LLFloaterAssociateListing::apply(bool user_confirm)
{
	if (mFolderId.notNull())
	{
        S32 id = mInputLine->getValue().asInteger();
		if (id > 0)
		{
			// Get the number of version folders in this listing
			LLInventoryModel::cat_array_t* categories;
			LLInventoryModel::item_array_t* items;
			gInventory.getDirectDescendentsOf(mFolderId, categories, items);
			S32 versions = categories->size();

			// Check if the id exists in the merchant SLM DB: note that this
			// record might exist in the LLMarketplaceData structure even if
			// unseen in the UI, for instance, if its listing_uuid doesn't
			// exist in the merchant inventory
			LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
			LLUUID listing_uuid = marketdata->getListingFolder(id);

			if (user_confirm && versions != 1 && listing_uuid.notNull() &&
				marketdata->getActivationState(listing_uuid))
			{
                // Look for user confirmation before unlisting
				LLSD payload;
				payload["folder_id"] = mFolderId;
				gNotifications.add("ConfirmMerchantUnlist", LLSD(), payload,
								   apply_callback);
				return;
			}

			// Associate the id with the user chosen folder
			marketdata->associateListing(mFolderId, listing_uuid,id);
			// Update the folder widgets now that the action is launched
			LLMarketplace::updateCategory(listing_uuid);
			LLMarketplace::updateCategory(mFolderId);
			gInventory.notifyObservers();
		}
		else
		{
			gNotifications.add("AlertMerchantListingInvalidID");
		}
	}

	close();
}

//static
void LLFloaterAssociateListing::onButtonOK(void* userdata)
{
	LLFloaterAssociateListing* self = (LLFloaterAssociateListing*)userdata;
	if (self)
	{
		self->apply();
	}
}

//static
void LLFloaterAssociateListing::onButtonCancel(void* userdata)
{
	LLFloaterAssociateListing* self = (LLFloaterAssociateListing*)userdata;
	if (self)
	{
		self->close();
	}
}
