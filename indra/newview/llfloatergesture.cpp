/**
 * @file llfloatergesture.cpp
 * @brief Read-only list of gestures from your inventory.
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

#include "llfloatergesture.h"

#include "llkeyboard.h"
#include "llmultigesture.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llfloaterinventory.h"
#include "llfloaterperms.h"
#include "llgesturemgr.h"
#include "llpreviewgesture.h"

//-----------------------------------------------------------------------------
// Gesture manager observer
//-----------------------------------------------------------------------------

class LLFloaterGestureObserver final : public LLGestureManagerObserver
{
public:
	LLFloaterGestureObserver()				{}
	~LLFloaterGestureObserver() override	{}

	LL_INLINE void changed() override
	{
		LLFloaterGesture::refreshAll();
	}
};

//-----------------------------------------------------------------------------
// Gesture inventory callback
//-----------------------------------------------------------------------------

class GestureShowCallback : public LLInventoryCallback
{
public:
	GestureShowCallback(std::string& title)
	{
		mTitle = title;
	}

	void fire(const LLUUID &inv_item)
	{
		LLPreviewGesture::show(mTitle, inv_item, LLUUID::null);
	}

private:
	std::string mTitle;
};

//-----------------------------------------------------------------------------
// LLFloaterGesture class proper
//-----------------------------------------------------------------------------

LLFloaterGesture::LLFloaterGesture(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_gesture.xml");

	mObserver = new LLFloaterGestureObserver;
	gGestureManager.addObserver(mObserver);
}

//virtual
LLFloaterGesture::~LLFloaterGesture()
{
	if (mObserver)
	{
		gGestureManager.removeObserver(mObserver);
		delete mObserver;
		mObserver = NULL;
	}
}

//virtual
bool LLFloaterGesture::postBuild()
{
	mGesturesList = getChild<LLScrollListCtrl>("gesture_list");
	mGesturesList->setCommitCallback(onCommitList);
	mGesturesList->setDoubleClickCallback(onClickPlay);
	mGesturesList->setCallbackUserData(this);

#if 0	// Not used
	childSetAction("inventory_btn", onClickInventory, this);
#endif

	childSetAction("new_gesture_btn", onClickNew, this);
	childSetAction("edit_btn", onClickEdit, this);

	childSetAction("play_btn", onClickPlay, this);
	childSetVisible("play_btn", true);
	childSetAction("stop_btn", onClickPlay, this);
	childSetVisible("stop_btn", false);
	setDefaultBtn("play_btn");

	buildGestureList();
	mGesturesList->setFocus(true);
	// Sort on name, ascending
	mGesturesList->sortByColumn(std::string("name"), true);
	mGesturesList->selectFirstItem();

	// Update button labels
	onCommitList(NULL, this);

	return true;
}

void LLFloaterGesture::buildGestureList()
{
	// Remember any selected gesture and the scroll position in the list
	S32 scrollpos = mGesturesList->getScrollPos();
	LLScrollListItem* item = mGesturesList->getFirstSelected();
	LLUUID selected_id;
	if (item)
	{
		selected_id = mGesturesList->getValue().asUUID();
	}

	mGesturesList->deleteAllItems();

	bool found_selected_id = false;
	std::string font_style, key_string, buffer;
	for (LLGestureManager::item_map_t::iterator
			it = gGestureManager.mActive.begin(),
			end = gGestureManager.mActive.end();
		 it != end; ++it)
	{
		const LLUUID& item_id = it->first;
		LLMultiGesture* gesture = it->second;

		if (item_id == selected_id)
		{
			found_selected_id = true;
		}

		// Note: Can have NULL item if inventory has not arrived yet.
		std::string item_name = "Loading...";
		LLInventoryItem* item = gInventory.getItem(item_id);
		if (item)
		{
			item_name = item->getName();
		}

		font_style = "NORMAL";

		LLSD element;
		element["id"] = item_id;

		if (gesture)
		{
			// If gesture is playing, bold it
			font_style = gesture->mPlaying ? "BOLD" : "NORMAL";

			element["columns"][0]["column"] = "trigger";
			element["columns"][0]["value"] = gesture->mTrigger;
			element["columns"][0]["font"] = "SANSSERIF";
			element["columns"][0]["font-style"] = font_style;

			if (gesture->mKey == KEY_NONE)
			{
				buffer = "---";
				key_string = "~~~";		// Alphabetize to end
			}
			else
			{
				key_string = LLKeyboard::stringFromKey(gesture->mKey);
				if (gesture->mMask & MASK_ALT)
				{
					buffer.append("ALT ");
				}
				if (gesture->mMask & MASK_CONTROL)
				{
					buffer.append("CTRL ");
				}
				if (gesture->mMask & MASK_SHIFT)
				{
					buffer.append("SHIFT ");
				}
				buffer.append(key_string);
			}
			element["columns"][1]["column"] = "shortcut";
			element["columns"][1]["value"] = buffer;
			element["columns"][1]["font"] = "SANSSERIF";
			element["columns"][1]["font-style"] = font_style;

			// Hidden column for sorting
			element["columns"][2]["column"] = "key";
			element["columns"][2]["value"] = key_string;
			element["columns"][2]["font"] = "SANSSERIF";
			element["columns"][2]["font-style"] = font_style;

			// Only add "playing" if we've got the name, less confusing. JC
			if (item && gesture->mPlaying)
			{
				item_name += " (Playing)";
			}
			element["columns"][3]["column"] = "name";
			element["columns"][3]["value"] = item_name;
			element["columns"][3]["font"] = "SANSSERIF";
			element["columns"][3]["font-style"] = font_style;
		}
		else
		{
			element["columns"][0]["column"] = "trigger";
			element["columns"][0]["value"] = "";
			element["columns"][0]["font"] = "SANSSERIF";
			element["columns"][0]["font-style"] = "NORMAL";
			element["columns"][0]["column"] = "trigger";
			element["columns"][0]["value"] = "---";
			element["columns"][0]["font"] = "SANSSERIF";
			element["columns"][0]["font-style"] = "NORMAL";
			element["columns"][2]["column"] = "key";
			element["columns"][2]["value"] = "~~~";
			element["columns"][2]["font"] = "SANSSERIF";
			element["columns"][2]["font-style"] = "NORMAL";
			element["columns"][3]["column"] = "name";
			element["columns"][3]["value"] = item_name;
			element["columns"][3]["font"] = "SANSSERIF";
			element["columns"][3]["font-style"] = "NORMAL";
		}
		mGesturesList->addElement(element, ADD_BOTTOM);
	}

	// Restore any selected item and scroll position in list
	if (found_selected_id)
	{
		mGesturesList->selectByID(selected_id);
	}
	if (scrollpos)
	{
		mGesturesList->setScrollPos(scrollpos);
	}
	else if (found_selected_id)
	{
		mGesturesList->scrollToShowSelected();
	}
}

//static
void LLFloaterGesture::refreshAll()
{
	LLFloaterGesture* self = findInstance();
	if (self)
	{
		self->buildGestureList();
		// Update button labels
		onCommitList(NULL, self);
	}
}

//static
void LLFloaterGesture::onCommitList(LLUICtrl* ctrl, void* data)
{
	LLFloaterGesture* self = (LLFloaterGesture*)data;
	if (!self) return;

	const LLUUID& item_id = self->mGesturesList->getValue().asUUID();
	if (gGestureManager.isGesturePlaying(item_id))
	{
		self->childSetVisible("play_btn", false);
		self->childSetVisible("stop_btn", true);
	}
	else
	{
		self->childSetVisible("play_btn", true);
		self->childSetVisible("stop_btn", false);
	}
}
//static
void LLFloaterGesture::onClickPlay(void* data)
{
	LLFloaterGesture* self = (LLFloaterGesture*)data;
	if (!self) return;

	const LLUUID& item_id = self->mGesturesList->getCurrentID();
	if (gGestureManager.isGesturePlaying(item_id))
	{
		gGestureManager.stopGesture(item_id);
	}
	else
	{
		gGestureManager.playGesture(item_id);
	}
}

//static
void LLFloaterGesture::onClickNew(void* data)
{
	std::string title("Gesture: New Gesture");
	LLPointer<LLInventoryCallback> cb = new GestureShowCallback(title);
	create_inventory_item(LLUUID::null, LLTransactionID::tnull,
						  "New Gesture", "", LLAssetType::AT_GESTURE,
						  LLInventoryType::IT_GESTURE, NO_INV_SUBTYPE,
						  PERM_MOVE | LLFloaterPerms::getNextOwnerPerms(), cb);
}

//static
void LLFloaterGesture::onClickEdit(void* data)
{
	LLFloaterGesture* self = (LLFloaterGesture*)data;
	if (!self) return;

	const LLUUID& item_id = self->mGesturesList->getCurrentID();

	LLInventoryItem* item = gInventory.getItem(item_id);
	if (!item) return;

	std::string title("Gesture: ");
	title.append(item->getName());

	LLPreviewGesture* previewp = LLPreviewGesture::show(title, item_id,
														LLUUID::null);
	if (gFloaterViewp && previewp && !previewp->getHost())
	{
		previewp->setRect(gFloaterViewp->findNeighboringPosition(self,
																 previewp));
	}
}

#if 0	// Not used
//static
void LLFloaterGesture::onClickInventory(void* data)
{
	LLFloaterGesture* self = (LLFloaterGesture*)data;
	LLFloaterInventory* inv = LLFloaterInventory::showAgentInventory();
	if (self && inv)
	{
		const LLUUID& item_id = self->mGesturesList->getCurrentID();
		inv->getPanel()->setSelection(item_id, true);
	}
}
#endif

