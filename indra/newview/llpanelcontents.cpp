/**
 * @file llpanelcontents.cpp
 * @brief Object contents panel in the tools floater.
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

#include "llpanelcontents.h"

#include "llbutton.h"
#include "llpermissionsflags.h"

#include "llagent.h"
#include "llfloaterbulkpermission.h"
#include "llfloaterperms.h"
#include "llpanelinventory.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"

LLPanelContents::LLPanelContents(const std::string& name)
:	LLPanel(name),
	mPanelInventory(NULL)
{
}

//virtual
bool LLPanelContents::postBuild()
{
	setMouseOpaque(false);

	mButtonNewScript = getChild<LLButton>("button new script");
	mButtonNewScript->setClickedCallback(onClickNewScript, this);

	mButtonPermissions = getChild<LLButton>("button permissions");
	mButtonPermissions->setClickedCallback(onClickPermissions, this);

	return true;
}

void LLPanelContents::getState(LLViewerObject* objectp)
{
	if (mPanelInventory)
	{
		mPanelInventory->refresh();
	}
	if (!objectp)
	{
		mButtonNewScript->setEnabled(false);
		mButtonPermissions->setEnabled(false);
		return;
	}

	LLUUID group_id;
	// sets group_id as a side effect SL-23488
	gSelectMgr.selectGetGroup(group_id);

	// BUG ? Check for all objects being editable ?
	bool editable = gAgent.isGodlike() ||
					(objectp->permModify() &&
					 !objectp->isPermanentEnforced() &&
					 // solves SL-23488
					 (objectp->permYouOwner() ||
					  (group_id.notNull() && gAgent.isInGroup(group_id))));
	bool all_volume = gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME);

	// Edit script button - ok if object is editable and there's an unambiguous
	// destination for the object.
	bool enabled = editable && all_volume &&
				   (gSelectMgr.getSelection()->getRootObjectCount() == 1 ||
					gSelectMgr.getSelection()->getObjectCount() == 1);
	mButtonNewScript->setEnabled(enabled);

	enabled = !objectp->isPermanentEnforced();
	mButtonPermissions->setEnabled(enabled);
	if (mPanelInventory)
	{
		mPanelInventory->setEnabled(enabled);
	}
}

//virtual
void LLPanelContents::refresh()
{
	LLViewerObject* object;
	object = gSelectMgr.getSelection()->getFirstRootObject(true);
	getState(object);
}

//static
void LLPanelContents::onClickNewScript(void* userdata)
{
	LLViewerObject* object;
	object = gSelectMgr.getSelection()->getFirstRootObject(true);
	if (object)
	{
//MK
		if (gRLenabled)
		{
			// Cannot edit objects that we are sitting on, when sit-restricted
			if (object->isAgentSeat() &&
				(gRLInterface.mContainsUnsit ||
				 gRLInterface.mSittpMax < EXTREMUM))
			{
				return;
			}

			if (!gRLInterface.canDetach(object))
			{
				return;
			}
		}
//mk
		LLPermissions perm;
		perm.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
		U32 perms = PERM_MOVE | LLFloaterPerms::getNextOwnerPerms();
		U32 group_perms = LLFloaterPerms::getGroupPerms();
		if (gSavedSettings.getBool("NoModScripts"))
		{
			perms = perms & ~PERM_MODIFY;
			group_perms = PERM_NONE;
		}
		perm.initMasks(PERM_ALL, PERM_ALL,
					   LLFloaterPerms::getEveryonePerms(), group_perms, perms);
		std::string desc;
		LLAssetType::generateDescriptionFor(LLAssetType::AT_LSL_TEXT, desc);
		LLPointer<LLViewerInventoryItem> new_item;
		new_item = new LLViewerInventoryItem(LLUUID::null, LLUUID::null,
											 perm, LLUUID::null,
											 LLAssetType::AT_LSL_TEXT,
											 LLInventoryType::IT_LSL,
											 std::string("New Script"),
											 desc, LLSaleInfo::DEFAULT,
											 LLViewerInventoryItem::II_FLAGS_NONE,
											 time_corrected());
		object->saveScript(new_item, true, true);
	}
}

//static
void LLPanelContents::onClickPermissions(void *userdata)
{
	LLPanelContents* self = (LLPanelContents*)userdata;
	if (self && gFloaterViewp)
	{
		LLFloater* parentp = gFloaterViewp->getParentFloater(self);
		if (parentp)
		{
			parentp->addDependentFloater(LLFloaterBulkPermission::showInstance());
		}
	}
}
