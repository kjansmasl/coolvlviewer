/**
 * @file llfloateropenobject.cpp
 * @brief LLFloaterOpenObject class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llfloateropenobject.h"

#include "llnotifications.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterinventory.h"
#include "llinventorybridge.h"
#include "llpanelinventory.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"

//static
void* LLFloaterOpenObject::createPanelInventory(void* data)
{
	LLFloaterOpenObject* self = (LLFloaterOpenObject*)data;
	self->mPanelInventory = new LLPanelInventory("Object contents", LLRect());
	return self->mPanelInventory;
}

LLFloaterOpenObject::LLFloaterOpenObject(const LLSD&)
:	mPanelInventory(NULL),
	mLastCount(0),
	mDirty(true)
{
	LLCallbackMap::map_t factory_map;
	factory_map["object_contents"] = LLCallbackMap(createPanelInventory, this);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_openobject.xml",
												 &factory_map);
}

//virtual
bool LLFloaterOpenObject::postBuild()
{
	mDescription = getChild<LLTextBox>("object_name");

	childSetAction("copy_to_inventory_button", onClickMoveToInventory, this);

	if (gSavedSettings.getBool("EnableCopyAndWear"))
	{
		childSetAction("copy_and_wear_button", onClickMoveAndWear, this);
	}
	else
	{
		childSetVisible("copy_and_wear_button", false);
	}

	center();

	return true;
}

//virtual
void LLFloaterOpenObject::refresh()
{
	mPanelInventory->refresh();

	LLSelectNode* node = mObjectSelection->getFirstRootNode();
	if (node)
	{
		static std::string item = getString("item");
		static std::string items = getString("items");
		mLastCount = mPanelInventory->getViewsCount();
#if 0	// WARNING: UTF-8 characters encoding issues at play: you cannot just
		// use node->mName or item(s) within llformat(), else you get bogus
		// characters... HB
		mDescription->setText(llformat("%s (%d %s)", node->mName, mLastCount,
									   (mLastCount > 1 ? items : item)));
#else
		mDescription->setText(node->mName + llformat(" (%d ", mLastCount) +
							  (mLastCount > 1 ? items : item) + ")");
#endif
	}
}

//virtual
void LLFloaterOpenObject::draw()
{
	if (mDirty || mPanelInventory->getViewsCount() != mLastCount)
	{
		refresh();
		mDirty = false;
	}
	LLFloater::draw();
}

void LLFloaterOpenObject::moveToInventory(bool wear)
{
	if (mObjectSelection->getRootObjectCount() != 1)
	{
		gNotifications.add("OnlyCopyContentsOfSingleItem");
		return;
	}

	LLSelectNode* nodep = mObjectSelection->getFirstRootNode();
	if (!nodep) return;

	LLViewerObject* objectp = nodep->getObject();
	if (!objectp) return;

	// Either create a sub-folder for worn clothing, or of the root folder.
	LLUUID parent_cat_id;
	if (wear)
	{
		parent_cat_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
	}
	else
	{
		parent_cat_id = gInventory.getRootFolderID();
	}

	inventory_func_t func =
		boost::bind(LLFloaterOpenObject::callbackCreateCategory, _1,
					objectp->getID(), wear);
	gInventory.createNewCategory(parent_cat_id, LLFolderType::FT_NONE,
								 nodep->mName, func);
}

//static
void LLFloaterOpenObject::dirty()
{
	LLFloaterOpenObject* self = findInstance();
	if (self)
	{
		self->mDirty = true;
	}
}

//static
void LLFloaterOpenObject::show()
{
	LLObjectSelectionHandle object_selection = gSelectMgr.getSelection();
	if (object_selection->getRootObjectCount() != 1)
	{
		gNotifications.add("UnableToViewContentsMoreThanOne");
		return;
	}
//MK
	if (gRLenabled && gRLInterface.mContainsEdit)
	{
		LLViewerObject* objp = gSelectMgr.getSelection()->getPrimaryObject();
		if (objp && !gRLInterface.canEdit(objp))
		{
			return;
		}
	}
//mk

	LLFloaterOpenObject* self = getInstance(); // Create new instance if needed
	self->open();
	self->setFocus(true);
	self->mObjectSelection = gSelectMgr.getEditSelection();
}

//static
void LLFloaterOpenObject::callbackCreateCategory(const LLUUID& cat_id,
												 LLUUID object_id, bool wear)
{
	if (cat_id.isNull())
	{
		gNotifications.add("CantCreateRequestedInvFolder");
		return;
	}

	LLCatAndWear* datap = new LLCatAndWear;
	datap->mCatID = cat_id;
	datap->mWear = wear;
	datap->mFolderResponded = true;

 	// Copy and/or move the items into the newly created folder. Ignore any
	// "You are going to break this item" messages.
 	if (!move_inv_category_world_to_agent(object_id, cat_id, true,
 										  callbackMoveInventory,
										  (void*)datap))
 	{
		gNotifications.add("OpenObjectCannotCopy");
		delete datap;
 	}
}

//static
void LLFloaterOpenObject::callbackMoveInventory(S32 result, void* userdata)
{
	LLCatAndWear* datap = (LLCatAndWear*)userdata;
	if (datap && result == 0)
	{
		LLFloaterInventory::showAgentInventory();
		LLFloaterInventory* floaterp = LLFloaterInventory::getActiveFloater();
		if (floaterp)
		{
			floaterp->getPanel()->setSelection(datap->mCatID, TAKE_FOCUS_NO);
		}
	}
	delete datap;
}

//static
void LLFloaterOpenObject::onClickMoveToInventory(void* data)
{
	LLFloaterOpenObject* self = (LLFloaterOpenObject*)data;
	if (self)
	{
		self->moveToInventory(false);
		self->close();
	}
}

//static
void LLFloaterOpenObject::onClickMoveAndWear(void* data)
{
	LLFloaterOpenObject* self = (LLFloaterOpenObject*)data;
	if (self)
	{
//MK
		if (gRLenabled && gRLInterface.mContainsDetach)
		{
			self->moveToInventory(false);
		}
		else
		{
			self->moveToInventory(true);
		}
//mk
		self->close();
	}
}
