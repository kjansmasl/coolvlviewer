/**
 * @file llpanelpermissions.cpp
 * @brief LLPanelPermissions class implementation
 * This class represents the panel in the build view for
 * viewing/editing object names, owners, permissions, etc.
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

#include "llpanelpermissions.h"

#include "llbutton.h"
#include "llcategory.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnamebox.h"
#include "llpermissions.h"
#include "llradiogroup.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroupinfo.h"
#include "llfloatergroups.h"
#include "llinventorymodel.h"
//MK
#include "mkrlinterface.h"
//mk
#include "hbobjectbackup.h"		 // For HBObjectBackup::validateAssetPerms()
#include "llviewercontrol.h"

///----------------------------------------------------------------------------
/// Class llpanelpermissions
///----------------------------------------------------------------------------

// Default constructor
LLPanelPermissions::LLPanelPermissions(const std::string& title)
:	LLPanel(title)
{
	setMouseOpaque(false);
}

//virtual
bool LLPanelPermissions::postBuild()
{
	// Object name

	mTextObjectName = getChild<LLTextBox>("Name:");

	mEditorObjectName = getChild<LLLineEditor>("Object Name");
	mEditorObjectName->setCommitCallback(onCommitName);
	mEditorObjectName->setCallbackUserData(this);
	mEditorObjectName->setPrevalidate(LLLineEditor::prevalidatePrintableNotPipe);

	// Object description

	mTextObjectDesc = getChild<LLTextBox>("Description:");

	mEditorObjectDesc = getChild<LLLineEditor>("Object Description");
	mEditorObjectDesc->setCommitCallback(onCommitDesc);
	mEditorObjectDesc->setCallbackUserData(this);
	mEditorObjectDesc->setPrevalidate(LLLineEditor::prevalidatePrintableNotPipe);

	// Object creator

	mTextCreatorLabel = getChild<LLTextBox>("Creator:");
	mTextCreatorName = getChild<LLTextBox>("Creator Name");

	mButtonCreatorProfile = getChild<LLButton>("button creator profile");
	mButtonCreatorProfile->setClickedCallback(onClickCreator, this);

	// Object owner

	mTextOwnerLabel = getChild<LLTextBox>("Owner:");
	mTextOwnerName = getChild<LLTextBox>("Owner Name");

	mButtonOwnerProfile = getChild<LLButton>("button owner profile");
	mButtonOwnerProfile->setClickedCallback(onClickOwner, this);

	// Object group

	mTextGroupName = getChild<LLTextBox>("Group:");

	LLTextBox* group_name = getChild<LLTextBox>("Group Name Proxy");
	mNameBoxGroupName = new LLNameBox("Group Name", group_name->getRect());
	addChild(mNameBoxGroupName);

	mButtonSetGroup = getChild<LLButton>("button set group");
	mButtonSetGroup->setClickedCallback(onClickGroup, this);

	// Permissions

	mTextPermissions = getChild<LLTextBox>("Permissions:");
	mTextPermissionsModify = getChild<LLTextBox>("perm_modify");

	mCheckShareWithGroup = getChild<LLCheckBoxCtrl>("share_with_group");
	mCheckShareWithGroup->setCommitCallback(onCommitGroupShare);
	mCheckShareWithGroup->setCallbackUserData(this);

	mButtonDeed = getChild<LLButton>("button deed");
	mButtonDeed->setClickedCallback(onClickDeedToGroup, this);

	mCheckAllowEveryoneExport = getChild<LLCheckBoxCtrl>("allow_export");
	mCheckAllowEveryoneExport->setCommitCallback(onCommitEveryoneExport);
	mCheckAllowEveryoneExport->setCallbackUserData(this);

	mCheckAllowEveryoneMove = getChild<LLCheckBoxCtrl>("everyone_move");
	mCheckAllowEveryoneMove->setCommitCallback(onCommitEveryoneMove);
	mCheckAllowEveryoneMove->setCallbackUserData(this);

	mCheckAllowEveryoneCopy = getChild<LLCheckBoxCtrl>("everyone_copy");
	mCheckAllowEveryoneCopy->setCommitCallback(onCommitEveryoneCopy);
	mCheckAllowEveryoneCopy->setCallbackUserData(this);

	mCheckShowInSearch = getChild<LLCheckBoxCtrl>("search_check");
	mCheckShowInSearch->setCommitCallback(onCommitIncludeInSearch);
	mCheckShowInSearch->setCallbackUserData(this);

	mCheckForSale = getChild<LLCheckBoxCtrl>("for_sale");
	mCheckForSale->setCommitCallback(onCommitSaleInfo);
	mCheckForSale->setCallbackUserData(this);

	mTextCost = getChild<LLTextBox>("Cost");

	mEditorCost = getChild<LLLineEditor>("Edit Cost");
	mEditorCost->setCommitCallback(onCommitSaleInfo);
	mEditorCost->setCallbackUserData(this);
	mEditorCost->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);

	mRadioSaleType = getChild<LLRadioGroup>("sale type");
	mRadioSaleType->setCommitCallback(onCommitSaleType);
	mRadioSaleType->setCallbackUserData(this);

	mTextNextOwnerCan = getChild<LLTextBox>("next_owner_can");

	mCheckNextCanModify = getChild<LLCheckBoxCtrl>("next_can_modify");
	mCheckNextCanModify->setCommitCallback(onCommitNextOwnerModify);
	mCheckNextCanModify->setCallbackUserData(this);

	mCheckNextCanCopy = getChild<LLCheckBoxCtrl>("next_can_copy");
	mCheckNextCanCopy->setCommitCallback(onCommitNextOwnerCopy);
	mCheckNextCanCopy->setCallbackUserData(this);

	mCheckNextCanTransfer = getChild<LLCheckBoxCtrl>("next_can_transfer");
	mCheckNextCanTransfer->setCommitCallback(onCommitNextOwnerTransfer);
	mCheckNextCanTransfer->setCallbackUserData(this);

	mTextClickAction = getChild<LLTextBox>("label click action");

	mComboClickAction = getChild<LLComboBox>("clickaction");
	mComboClickAction->setCommitCallback(onCommitClickAction);
	mComboClickAction->setCallbackUserData(this);

	mIconNavMeshInfo = getChild<LLIconCtrl>("navmeshicon");
	mTextNavMeshInfo = getChild<LLTextBox>("navmeshinfo");

	mTextDebugPermB = getChild<LLTextBox>("B:");
	mTextDebugPermO = getChild<LLTextBox>("O:");
	mTextDebugPermG = getChild<LLTextBox>("G:");
	mTextDebugPermE = getChild<LLTextBox>("E:");
	mTextDebugPermN = getChild<LLTextBox>("N:");
	mTextDebugPermF = getChild<LLTextBox>("F:");

	mCostTotal   = getString("Cost Total");
	mCostDefault = getString("Cost Default");
	mCostPerUnit = getString("Cost Per Unit");
	mCostMixed   = getString("Cost Mixed");
	mSaleMixed   = getString("Sale Mixed");

	return true;
}

//virtual
void LLPanelPermissions::refresh()
{
	// Static variables, to prevent calling getString() on each refresh()
	static std::string MODIFY_INFO_STRINGS[] =
	{
		getString("text modify info 1"),
		getString("text modify info 2"),
		getString("text modify info 3"),
		getString("text modify info 4"),
		getString("text modify info 5"),
		getString("text modify info 6"),
		getString("text modify warning")
	};

	static std::string text_deed = getString("text deed");
	static std::string text_deed_continued = getString("text deed continued");

	static LLCachedControl<bool> warn_deed_object(gSavedSettings,
												  "WarnDeedObject");
	std::string deed_text = warn_deed_object ? text_deed_continued : text_deed;
	mButtonDeed->setLabelSelected(deed_text);
	mButtonDeed->setLabelUnselected(deed_text);

	LLObjectSelectionHandle selection = gSelectMgr.getSelection();

	LLSelectNode* nodep = selection->getFirstRootNode();
	S32 object_count = selection->getRootObjectCount();
	bool root_selected = true;
	if (!nodep || !object_count)
	{
		nodep = selection->getFirstNode();
		object_count = selection->getObjectCount();
		root_selected = false;
	}

	LLViewerObject* objectp = NULL;
	if (nodep) objectp = nodep->getObject();
	if (!nodep || !objectp)
	{
		// ...nothing selected
		mTextObjectName->setEnabled(false);
		mEditorObjectName->setText(LLStringUtil::null);
		mEditorObjectName->setEnabled(false);

		mTextObjectDesc->setEnabled(false);
		mEditorObjectDesc->setText(LLStringUtil::null);
		mEditorObjectDesc->setEnabled(false);

		mTextCreatorLabel->setEnabled(false);
		mTextCreatorName->setText(LLStringUtil::null);
		mTextCreatorName->setEnabled(false);
		mButtonCreatorProfile->setEnabled(false);

		mTextOwnerLabel->setEnabled(false);
		mTextOwnerName->setText(LLStringUtil::null);
		mTextOwnerName->setEnabled(false);
		mButtonOwnerProfile->setEnabled(false);

		mTextGroupName->setEnabled(false);
		mNameBoxGroupName->setText(LLStringUtil::null);
		mNameBoxGroupName->setEnabled(false);
		mButtonSetGroup->setEnabled(false);

		mTextPermissions->setEnabled(false);

		mTextPermissionsModify->setEnabled(false);
		mTextPermissionsModify->setText(LLStringUtil::null);

		mCheckShareWithGroup->set(false);
		mCheckShareWithGroup->setEnabled(false);
		mButtonDeed->setEnabled(false);

		mCheckAllowEveryoneExport->set(false);
		mCheckAllowEveryoneExport->setEnabled(false);
		mCheckAllowEveryoneMove->set(false);
		mCheckAllowEveryoneMove->setEnabled(false);
		mCheckAllowEveryoneCopy->set(false);
		mCheckAllowEveryoneCopy->setEnabled(false);

		// Next owner can:
		mTextNextOwnerCan->setEnabled(false);
		mCheckNextCanModify->set(false);
		mCheckNextCanModify->setEnabled(false);
		mCheckNextCanCopy->set(false);
		mCheckNextCanCopy->setEnabled(false);
		mCheckNextCanTransfer->set(false);
		mCheckNextCanTransfer->setEnabled(false);

		// path finding info
		mIconNavMeshInfo->setVisible(false);
		mTextNavMeshInfo->setVisible(false);

		// checkbox include in search
		mCheckShowInSearch->set(false);
		mCheckShowInSearch->setEnabled(false);

		// checkbox for sale
		mCheckForSale->set(false);
		mCheckForSale->setEnabled(false);

		mRadioSaleType->setSelectedIndex(-1);
		mRadioSaleType->setEnabled(false);

		mTextCost->setText(mCostDefault);
		mTextCost->setEnabled(false);
		mEditorCost->setText(LLStringUtil::null);
		mEditorCost->setEnabled(false);

		mTextClickAction->setEnabled(false);
		mComboClickAction->setEnabled(false);
		mComboClickAction->clear();
		mTextDebugPermB->setVisible(false);
		mTextDebugPermO->setVisible(false);
		mTextDebugPermG->setVisible(false);
		mTextDebugPermE->setVisible(false);
		mTextDebugPermN->setVisible(false);
		mTextDebugPermF->setVisible(false);

		return;
	}

	mTextPermissions->setEnabled(true);

	// figure out a few variables
	bool is_one_object = object_count == 1;

	// BUG: fails if a root and non-root are both single-selected.
	bool is_perm_modify = gSelectMgr.selectGetModify() ||
						  (selection->getFirstRootNode() &&
						   gSelectMgr.selectGetRootsModify());
	bool is_nonpermanent_enforced =
		gSelectMgr.selectGetNonPermanentEnforced() ||
		(selection->getFirstRootNode() &&
		 gSelectMgr.selectGetRootsNonPermanentEnforced());

	S32 string_index = 0;
	if (!is_perm_modify)
	{
		string_index += 2;
	}
	else if (!is_nonpermanent_enforced)
	{
		string_index += 4;
	}
	if (!is_one_object)
	{
		++string_index;
	}
	mTextPermissionsModify->setEnabled(true);
	mTextPermissionsModify->setText(MODIFY_INFO_STRINGS[string_index]);

	// Path finding attributes, if any.
	std::string pf_info = gSelectMgr.getPathFindingAttributeInfo(true);
	bool pf_info_enabled = !pf_info.empty();
	if (pf_info_enabled)
	{
		mTextNavMeshInfo->setText(pf_info);
	}
	mIconNavMeshInfo->setVisible(pf_info_enabled);
	mTextNavMeshInfo->setVisible(pf_info_enabled);

	// Update creator text field
	mTextCreatorLabel->setEnabled(true);
	std::string creator_name;
	bool creators_identical = gSelectMgr.selectGetCreator(mCreatorID,
														  creator_name);
	mTextCreatorName->setText(creator_name);
	mTextCreatorName->setEnabled(true);
	mButtonCreatorProfile->setEnabled(creators_identical && mCreatorID.notNull());

	// Update owner text field
	mTextOwnerLabel->setEnabled(true);

	std::string owner_name;
	bool owners_identical = gSelectMgr.selectGetOwner(mOwnerID, owner_name);

	if (mOwnerID.isNull())
	{
		if (gSelectMgr.selectIsGroupOwned())
		{
			// Group owned already displayed by selectGetOwner
		}
		else
		{
			// Display last owner if public
			std::string last_owner_name;
			gSelectMgr.selectGetLastOwner(mLastOwnerID, last_owner_name);

			// It should never happen that the last owner is null and the owner
			// is null, but it seems to be a bug in the simulator right now. JC
			if (mLastOwnerID.notNull() && !last_owner_name.empty())
			{
				owner_name.append(", last ");
				owner_name.append(last_owner_name);
			}
		}
	}

//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		owner_name = gRLInterface.getDummyName(owner_name);
	}
//mk
	mTextOwnerName->setText(owner_name);
	mTextOwnerName->setEnabled(true);
	mButtonOwnerProfile->setEnabled(owners_identical &&
									(mOwnerID.notNull() ||
									 gSelectMgr.selectIsGroupOwned()));

	// update group text field
	mTextGroupName->setEnabled(true);
	mNameBoxGroupName->setText(LLStringUtil::null);
	LLUUID group_id;
	bool groups_identical = gSelectMgr.selectGetGroup(group_id);
	if (groups_identical)
	{
		mNameBoxGroupName->setNameID(group_id, true);
		mNameBoxGroupName->setEnabled(true);
	}
	else
	{
		mNameBoxGroupName->setNameID(LLUUID::null, true);
		mNameBoxGroupName->refresh(LLUUID::null, LLStringUtil::null, true);
		mNameBoxGroupName->setEnabled(false);
	}

	mButtonSetGroup->setEnabled(root_selected && owners_identical &&
								mOwnerID == gAgentID &&
								is_nonpermanent_enforced);

	// figure out the contents of the name, description, & category
	mTextObjectName->setEnabled(true);
	mTextObjectDesc->setEnabled(true);

	const LLFocusableElement* keyboard_focus_view = gFocusMgr.getKeyboardFocus();
	if (is_one_object)
	{
		if (keyboard_focus_view != mEditorObjectName)
		{
			mEditorObjectName->setText(nodep->mName);
		}

		if (keyboard_focus_view != mEditorObjectDesc)
		{
			mEditorObjectDesc->setText(nodep->mDescription);
		}
	}
	else
	{
		mEditorObjectName->setText(LLStringUtil::null);
		mEditorObjectDesc->setText(LLStringUtil::null);
	}

	bool edit_name_desc = is_one_object && objectp->permModify() &&
						  !objectp->isPermanentEnforced();
	mEditorObjectName->setEnabled(edit_name_desc);
	mEditorObjectDesc->setEnabled(edit_name_desc);

	S32 total_sale_price = 0;
	S32 individual_sale_price = 0;
	bool is_for_sale_mixed = false;
	bool is_sale_price_mixed = false;
	U32 num_for_sale = 0;
    gSelectMgr.selectGetAggregateSaleInfo(num_for_sale, is_for_sale_mixed,
										  is_sale_price_mixed,
										  total_sale_price,
										  individual_sale_price);

	bool self_owned = gAgentID == mOwnerID;
	bool group_owned = gSelectMgr.selectIsGroupOwned();
	bool public_owned = mOwnerID.isNull() && !gSelectMgr.selectIsGroupOwned();
	bool can_transfer = gSelectMgr.selectGetRootsTransfer();
	bool can_copy = gSelectMgr.selectGetRootsCopy();

	if (!owners_identical)
	{
		mTextCost->setEnabled(false);
		mEditorCost->setText(LLStringUtil::null);
		mEditorCost->setEnabled(false);
	}
	// You own these objects.
	else if (self_owned ||
			 (group_owned &&
			  gAgent.hasPowerInGroup(group_id, GP_OBJECT_SET_SALE)))
	{
		// If there are multiple items for sale then set text to PRICE PER UNIT
		if (num_for_sale > 1)
		{
			mTextCost->setText(mCostPerUnit);
		}
		else
		{
			mTextCost->setText(mCostDefault);
		}

		if (keyboard_focus_view != mEditorCost)
		{
			// If the sale price is mixed then set the cost to MIXED, otherwise
			// set to the actual cost.
			if (num_for_sale > 0 && is_for_sale_mixed)
			{
				mEditorCost->setText(mSaleMixed);
			}
			else if (num_for_sale > 0 && is_sale_price_mixed)
			{
				mEditorCost->setText(mCostMixed);
			}
			else
			{
				mEditorCost->setText(llformat("%d", individual_sale_price));
			}
		}
		// The edit fields are only enabled if you can sell this object
		// and the sale price is not mixed.
		bool enable_edit = num_for_sale && can_transfer ? !is_for_sale_mixed
														: false;
		mTextCost->setEnabled(enable_edit);
		mEditorCost->setEnabled(enable_edit);
	}
	// Someone, not you, owns these objects.
	else if (!public_owned)
	{
		mTextCost->setEnabled(false);
		mEditorCost->setEnabled(false);

		// Don't show a price if none of the items are for sale.
		if (num_for_sale)
		{
			mEditorCost->setText(llformat("%d", total_sale_price));
		}
		else
		{
			mEditorCost->setText(LLStringUtil::null);
		}

		// If multiple items are for sale, set text to TOTAL PRICE.
		if (num_for_sale > 1)
		{
			mTextCost->setText(mCostTotal);
		}
		else
		{
			mTextCost->setText(mCostDefault);
		}
	}
	// This is a public object.
	else
	{
		mTextCost->setText(mCostDefault);
		mTextCost->setEnabled(false);

		mEditorCost->setText(LLStringUtil::null);
		mEditorCost->setEnabled(false);
	}

	// Enable and disable the permissions checkboxes based on who owns the
	// object. * TODO: Creator permissions
	bool valid_base_perms = false;
	bool valid_owner_perms = false;
	bool valid_group_perms = false;
	bool valid_everyone_perms = false;
	bool valid_next_perms = false;
	U32 base_mask_on = 0;
	U32 base_mask_off = 0;
	U32 owner_mask_on = 0;
	U32 owner_mask_off = 0;
	U32 group_mask_on = 0;
	U32 group_mask_off = 0;
	U32 everyone_mask_on = 0;
	U32 everyone_mask_off = 0;
	U32 next_owner_mask_on = 0;
	U32 next_owner_mask_off = 0;

	if (root_selected)
	{
		valid_base_perms = gSelectMgr.selectGetPerm(PERM_BASE,
													&base_mask_on,
													&base_mask_off);

		valid_owner_perms = gSelectMgr.selectGetPerm(PERM_OWNER,
													 &owner_mask_on,
													 &owner_mask_off);

		valid_group_perms = gSelectMgr.selectGetPerm(PERM_GROUP,
													 &group_mask_on,
													 &group_mask_off);

		valid_everyone_perms = gSelectMgr.selectGetPerm(PERM_EVERYONE,
														&everyone_mask_on,
														&everyone_mask_off);

		valid_next_perms = gSelectMgr.selectGetPerm(PERM_NEXT_OWNER,
													&next_owner_mask_on,
													&next_owner_mask_off);
	}
	else if (is_one_object)
	{
		LLSelectNode* node = selection->getFirstNode();
		if (node && node->mValid)
		{
			valid_base_perms = valid_owner_perms = valid_group_perms =
				valid_everyone_perms = valid_next_perms = true;
			base_mask_on = node->mPermissions->getMaskBase();
			owner_mask_on = node->mPermissions->getMaskOwner();
			group_mask_on = node->mPermissions->getMaskGroup();
			everyone_mask_on = node->mPermissions->getMaskEveryone();
			next_owner_mask_on = node->mPermissions->getMaskNextOwner();
		}
	}

	bool export_support = gAgent.regionHasExportPermSupport();

	static LLCachedControl<bool> debug_permissions(gSavedSettings,
												   "DebugPermissions");
	if (debug_permissions)
	{
		std::string perm_string;
		if (valid_base_perms)
		{
			perm_string = "B: " + mask_to_string(base_mask_on,
												 export_support);
			mTextDebugPermB->setText(perm_string);
			mTextDebugPermB->setVisible(true);

			if (valid_owner_perms)
			{
				perm_string = "O: " + mask_to_string(owner_mask_on,
													 export_support);
				mTextDebugPermO->setText(perm_string);
			}
			mTextDebugPermO->setVisible(valid_owner_perms);

			if (valid_group_perms)
			{
				perm_string = "G: " + mask_to_string(group_mask_on);
				mTextDebugPermG->setText(perm_string);
			}
			mTextDebugPermG->setVisible(valid_group_perms);

			if (valid_everyone_perms)
			{
				perm_string = "E: " + mask_to_string(everyone_mask_on,
													 export_support);
				mTextDebugPermE->setText(perm_string);
			}
			mTextDebugPermE->setVisible(valid_everyone_perms);

			if (valid_next_perms)
			{
				perm_string = "N: " + mask_to_string(next_owner_mask_on,
													 export_support);
				mTextDebugPermN->setText(perm_string);
			}
			mTextDebugPermN->setVisible(valid_next_perms);
		}

		U32 flag_mask = 0x0;
		if (objectp->permMove())		flag_mask |= PERM_MOVE;
		if (objectp->permModify())		flag_mask |= PERM_MODIFY;
		if (objectp->permCopy())		flag_mask |= PERM_COPY;
		if (objectp->permTransfer())	flag_mask |= PERM_TRANSFER;
		perm_string = "F: " + mask_to_string(flag_mask);
		mTextDebugPermF->setText(perm_string);
		mTextDebugPermF->setVisible(true);
	}
	else
	{
		mTextDebugPermB->setVisible(false);
		mTextDebugPermO->setVisible(false);
		mTextDebugPermG->setVisible(false);
		mTextDebugPermE->setVisible(false);
		mTextDebugPermN->setVisible(false);
		mTextDebugPermF->setVisible(false);
	}

	bool has_change_perm_ability = false;
	bool has_change_sale_ability = false;

	if (valid_base_perms && is_nonpermanent_enforced &&
		(self_owned ||
		 (group_owned && gAgent.hasPowerInGroup(group_id,
												GP_OBJECT_MANIPULATE))))
	{
		has_change_perm_ability = true;
	}
	if (valid_base_perms && is_nonpermanent_enforced &&
		(self_owned ||
		 (group_owned && gAgent.hasPowerInGroup(group_id,
												GP_OBJECT_SET_SALE))))
	{
		has_change_sale_ability = true;
	}

	if (!has_change_perm_ability && !has_change_sale_ability && !root_selected)
	{
		// ...must select root to choose permissions
		mTextPermissionsModify->setValue(MODIFY_INFO_STRINGS[6]);
	}

	if (has_change_perm_ability)
	{
		mCheckShareWithGroup->setEnabled(true);
		mCheckAllowEveryoneMove->setEnabled(owner_mask_on & PERM_MOVE);
		mCheckAllowEveryoneCopy->setEnabled((owner_mask_on & PERM_COPY) &&
											(owner_mask_on & PERM_TRANSFER));
	}
	else
	{
		mCheckShareWithGroup->setEnabled(false);
		mCheckAllowEveryoneMove->setEnabled(false);
		mCheckAllowEveryoneCopy->setEnabled(false);
	}

	mCheckAllowEveryoneExport->setVisible(export_support);
	if (export_support)
	{
		bool can_export = self_owned && mCreatorID == mOwnerID &&
						  can_set_export(base_mask_on, owner_mask_on,
										 next_owner_mask_on);
		if (can_export)
		{
			// Also check that the applied textures can be exported...
			for (U32 i = 0, count = objectp->getNumTEs();
				 can_export && i < count; ++i)
			{
				if (LLTextureEntry* tep = objectp->getTE(i))
				{
					if (!HBObjectBackup::validateAssetPerms(tep->getID()))
					{
						can_export = false;
						break;
					}
				}
			}
		}

#if 0	// We do not care about the object contents for now, since we never
		// export it !... Plus, it would be possible to set PERM_EXPORT on that
		// object without ever checking for its contents, by simply taking it
		// back to the inventory and setting the export flag from the inventory
		// asset properties floater instead... Just like you can set Copy-OK an
		// object containing no-copy assets from the inventory while you can't
		// (because of no-copy contents checking) while rezzed...
		// <rant>This permissions logic has always been utterly flawed and
		// buggy in both SL and OpenSIM, by lack of a recursive checking of
		// inventory assets contents permissions (plus, forcing a no-mod flag
		// on a container object while it is rezzed just because it contains
		// no-mod assets is also a *serious* BUG: the mod-ok flag should NEVER
		// recurse !)</rant>

		// The object's inventory must have EXPORT.
		objectp->getInventoryContents(objects);
		for (LLInventoryObject::object_list_t::iterator it = objects.begin(),
														end = objects.end();
			 can_export && it != end ; ++it)
		{
			LLViewerInventoryItem* item = static_cast<LLViewerInventoryItem*>(i->get());
			can_export = perms_allow_export(item->getPermissions());
		}
#endif
		mCheckAllowEveryoneExport->setEnabled(can_export);
	}

	if (has_change_sale_ability && (owner_mask_on & PERM_TRANSFER))
	{
		mCheckForSale->setEnabled(can_transfer ||
								  (!can_transfer && num_for_sale));
		// Set the checkbox to tentative if the prices of each object selected
		// are not the same.
		mCheckForSale->setTentative(is_for_sale_mixed);
		mRadioSaleType->setEnabled(num_for_sale && can_transfer &&
								   !is_sale_price_mixed);

		mTextNextOwnerCan->setEnabled(true);
		mCheckNextCanModify->setEnabled(base_mask_on & PERM_MODIFY);
		mCheckNextCanCopy->setEnabled(base_mask_on & PERM_COPY);
		mCheckNextCanTransfer->setEnabled(next_owner_mask_on & PERM_COPY);
	}
	else
	{
		mCheckForSale->setEnabled(false);
		mRadioSaleType->setEnabled(false);

		mTextNextOwnerCan->setEnabled(false);
		mCheckNextCanModify->setEnabled(false);
		mCheckNextCanCopy->setEnabled(false);
		mCheckNextCanTransfer->setEnabled(false);
	}

	if (valid_group_perms)
	{
		if ((group_mask_on & PERM_COPY) && (group_mask_on & PERM_MODIFY) &&
			(group_mask_on & PERM_MOVE))
		{
			mCheckShareWithGroup->set(true);
			mCheckShareWithGroup->setTentative(false);
			mButtonDeed->setEnabled(!group_owned && can_transfer &&
									(owner_mask_on & PERM_TRANSFER) &&
									gAgent.hasPowerInGroup(group_id,
														   GP_OBJECT_DEED));
		}
		else if ((group_mask_off & PERM_COPY) &&
				 (group_mask_off & PERM_MODIFY) &&
				 (group_mask_off & PERM_MOVE))
		{
			mCheckShareWithGroup->set(false);
			mCheckShareWithGroup->setTentative(false);
			mButtonDeed->setEnabled(false);
		}
		else
		{
			mCheckShareWithGroup->set(true);
			mCheckShareWithGroup->setTentative(true);
			mButtonDeed->setEnabled(!group_owned && can_transfer &&
									(group_mask_on & PERM_MOVE) &&
									(owner_mask_on & PERM_TRANSFER) &&
									gAgent.hasPowerInGroup(group_id,
														   GP_OBJECT_DEED));
 		}
	}

	if (valid_everyone_perms)
	{
		// Move
		if (everyone_mask_on & PERM_MOVE)
		{
			mCheckAllowEveryoneMove->set(true);
			mCheckAllowEveryoneMove->setTentative(false);
		}
		else if (everyone_mask_off & PERM_MOVE)
		{
			mCheckAllowEveryoneMove->set(false);
			mCheckAllowEveryoneMove->setTentative(false);
		}
		else
		{
			mCheckAllowEveryoneMove->set(true);
			mCheckAllowEveryoneMove->setTentative(true);
		}

		// Copy == everyone can't copy
		if (everyone_mask_on & PERM_COPY)
		{
			mCheckAllowEveryoneCopy->set(true);
			mCheckAllowEveryoneCopy->setTentative(!can_copy || !can_transfer);
		}
		else if (everyone_mask_off & PERM_COPY)
		{
			mCheckAllowEveryoneCopy->set(false);
			mCheckAllowEveryoneCopy->setTentative(false);
		}
		else
		{
			mCheckAllowEveryoneCopy->set(true);
			mCheckAllowEveryoneCopy->setTentative(true);
		}

		// Export
		if (export_support && (everyone_mask_on & PERM_EXPORT) != 0)
		{
			mCheckAllowEveryoneExport->set(true);
			mCheckAllowEveryoneExport->setTentative(false);
		}
		else if (!export_support || (everyone_mask_off & PERM_EXPORT) != 0)
		{
			mCheckAllowEveryoneExport->set(false);
			mCheckAllowEveryoneExport->setTentative(false);
		}
		else
		{
			mCheckAllowEveryoneExport->set(true);
			mCheckAllowEveryoneExport->setTentative(true);
		}
	}

	if (valid_next_perms)
	{
		// Modify == next owner canot modify
		if (next_owner_mask_on & PERM_MODIFY)
		{
			mCheckNextCanModify->set(true);
			mCheckNextCanModify->setTentative(false);
		}
		else if (next_owner_mask_off & PERM_MODIFY)
		{
			mCheckNextCanModify->set(false);
			mCheckNextCanModify->setTentative(false);
		}
		else
		{
			mCheckNextCanModify->set(true);
			mCheckNextCanModify->setTentative(true);
		}

		// Copy == next owner cannot copy
		if (next_owner_mask_on & PERM_COPY)
		{
			mCheckNextCanCopy->set(true);
			mCheckNextCanCopy->setTentative(!can_copy);
		}
		else if (next_owner_mask_off & PERM_COPY)
		{
			mCheckNextCanCopy->set(false);
			mCheckNextCanCopy->setTentative(false);
		}
		else
		{
			mCheckNextCanCopy->set(true);
			mCheckNextCanCopy->setTentative(true);
		}

		// Transfer == next owner cannot transfer
		if (next_owner_mask_on & PERM_TRANSFER)
		{
			mCheckNextCanTransfer->set(true);
			mCheckNextCanTransfer->setTentative(!can_transfer);
		}
		else if (next_owner_mask_off & PERM_TRANSFER)
		{
			mCheckNextCanTransfer->set(false);
			mCheckNextCanTransfer->setTentative(false);
		}
		else
		{
			mCheckNextCanTransfer->set(true);
			mCheckNextCanTransfer->setTentative(true);
		}
	}

	// reflect sale information
	LLSaleInfo sale_info;
	bool valid_sale_info = gSelectMgr.selectGetSaleInfo(sale_info);
	LLSaleInfo::EForSale sale_type = sale_info.getSaleType();

	if (valid_sale_info)
	{
		mRadioSaleType->setSelectedIndex((S32)sale_type - 1);
		// unfortunately this doesn't do anything at the moment:
		mRadioSaleType->setTentative(false);
	}
	else
	{
		// default option is sell copy, determined to be safest
		mRadioSaleType->setSelectedIndex((S32)LLSaleInfo::FS_COPY - 1);
		// unfortunately this doesn't do anything at the moment:
		mRadioSaleType->setTentative(true);
	}

	mCheckForSale->setValue(num_for_sale != 0);

	// HACK: There are some old objects in world that are set for sale, but are
	// no-transfer. We need to let users turn for-sale off, but only if
	// for-sale is set.
	bool cannot_actually_sell = !can_transfer ||
								(!can_copy &&
								 sale_type == LLSaleInfo::FS_COPY);
	if (num_for_sale && has_change_sale_ability && cannot_actually_sell)
	{
		mCheckForSale->setEnabled(true);
	}
	if (selection->isAttachment())
	{
		mCheckForSale->setEnabled(false);
		mRadioSaleType->setEnabled(false);
		mEditorCost->setEnabled(false);
	}

	// Check search status of objects
	bool all_volume = gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME);
	bool include_in_search;
	bool all_include_in_search = gSelectMgr.selectionGetIncludeInSearch(&include_in_search);
	mCheckShowInSearch->setEnabled(has_change_sale_ability && all_volume);
	mCheckShowInSearch->setValue(include_in_search);
	mCheckShowInSearch->setTentative(!all_include_in_search);

	// Click action (touch, sit, buy)
	U8 click_action = 0;
	if (gSelectMgr.selectionGetClickAction(&click_action))
	{
		mComboClickAction->setCurrentByIndex((S32)click_action);
	}
	mTextClickAction->setEnabled(is_perm_modify && all_volume &&
								 is_nonpermanent_enforced);
	mComboClickAction->setEnabled(is_perm_modify && all_volume &&
								  is_nonpermanent_enforced);
}

//static
void LLPanelPermissions::onClickClaim(void*)
{
	// try to claim ownership
	gSelectMgr.sendOwner(gAgentID, gAgent.getGroupID());
}

//static
void LLPanelPermissions::onClickRelease(void*)
{
	// try to release ownership
	gSelectMgr.sendOwner(LLUUID::null, LLUUID::null);
}

//static
void LLPanelPermissions::onClickCreator(void* data)
{
	LLPanelPermissions* self = (LLPanelPermissions*)data;

	LLFloaterAvatarInfo::showFromObject(self->mCreatorID);
}

//static
void LLPanelPermissions::onClickOwner(void* data)
{
	LLPanelPermissions* self = (LLPanelPermissions*)data;

	if (gSelectMgr.selectIsGroupOwned())
	{
		LLUUID group_id;
		gSelectMgr.selectGetGroup(group_id);
		LLFloaterGroupInfo::showFromUUID(group_id);
	}
	else
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			return;
		}
//mk
		LLFloaterAvatarInfo::showFromObject(self->mOwnerID);
	}
}

void LLPanelPermissions::onClickGroup(void* data)
{
	LLPanelPermissions* panelp = (LLPanelPermissions*)data;
	LLUUID owner_id;
	std::string name;
	bool owners_identical = gSelectMgr.selectGetOwner(owner_id, name);
	if (owners_identical && owner_id == gAgentID)
	{
		LLFloaterGroupPicker* fg = LLFloaterGroupPicker::show(cbGroupID, data);
		if (gFloaterViewp)
		{
			LLFloater* parentp = gFloaterViewp->getParentFloater(panelp);
			if (fg && parentp)
			{
				LLRect rect;
				rect = gFloaterViewp->findNeighboringPosition(parentp, fg);
				fg->setOrigin(rect.mLeft, rect.mBottom);
				parentp->addDependentFloater(fg);
			}
		}
	}
}

//static
void LLPanelPermissions::cbGroupID(LLUUID group_id, void* data)
{
	LLPanelPermissions* self = (LLPanelPermissions*)data;
	if (self)
	{
		self->mNameBoxGroupName->setNameID(group_id, true);
	}
	gSelectMgr.sendGroup(group_id);
}

bool callback_deed_to_group(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (0 == option)
	{
		LLUUID group_id;
		bool groups_identical = gSelectMgr.selectGetGroup(group_id);
		if (group_id.notNull() && groups_identical &&
			gAgent.hasPowerInGroup(group_id, GP_OBJECT_DEED))
		{
			gSelectMgr.sendOwner(LLUUID::null, group_id, false);
		}
	}
	return false;
}

void LLPanelPermissions::onClickDeedToGroup(void* data)
{
	gNotifications.add("DeedObjectToGroup", LLSD(), LLSD(),
					   callback_deed_to_group);
}

///----------------------------------------------------------------------------
/// Permissions checkboxes
///----------------------------------------------------------------------------

//static
void LLPanelPermissions::onCommitPerm(LLUICtrl* ctrl, void* data, U8 field,
									  U32 perm)
{
	LLViewerObject* object = gSelectMgr.getSelection()->getFirstRootObject();
	if (object && ctrl)
	{
		LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
		bool new_state = check->get();
		gSelectMgr.selectionSetObjectPermissions(field, new_state, perm);
	}
}

//static
void LLPanelPermissions::onCommitGroupShare(LLUICtrl* ctrl, void* data)
{
	onCommitPerm(ctrl, data, PERM_GROUP, PERM_MODIFY | PERM_MOVE | PERM_COPY);
}

//static
void LLPanelPermissions::onCommitEveryoneExport(LLUICtrl* ctrl, void* data)
{
	onCommitPerm(ctrl, data, PERM_EVERYONE, PERM_EXPORT);
}

//static
void LLPanelPermissions::onCommitEveryoneMove(LLUICtrl* ctrl, void* data)
{
	onCommitPerm(ctrl, data, PERM_EVERYONE, PERM_MOVE);
}

//static
void LLPanelPermissions::onCommitEveryoneCopy(LLUICtrl* ctrl, void* data)
{
	onCommitPerm(ctrl, data, PERM_EVERYONE, PERM_COPY);
}

//static
void LLPanelPermissions::onCommitNextOwnerModify(LLUICtrl* ctrl, void* data)
{
	onCommitPerm(ctrl, data, PERM_NEXT_OWNER, PERM_MODIFY);
}

//static
void LLPanelPermissions::onCommitNextOwnerCopy(LLUICtrl* ctrl, void* data)
{
	onCommitPerm(ctrl, data, PERM_NEXT_OWNER, PERM_COPY);
}

//static
void LLPanelPermissions::onCommitNextOwnerTransfer(LLUICtrl* ctrl, void* data)
{
	onCommitPerm(ctrl, data, PERM_NEXT_OWNER, PERM_TRANSFER);
}

//static
void LLPanelPermissions::onCommitName(LLUICtrl*, void* data)
{
	LLPanelPermissions* self = (LLPanelPermissions*)data;
	if (!self) return;

	const std::string& name = self->mEditorObjectName->getText();
	gSelectMgr.selectionSetObjectName(name);

	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->isAttachment() && selection->getNumNodes() == 1 &&
		!name.empty())
	{
		const LLUUID& id = selection->getFirstObject()->getAttachmentItemID();
		if (id.isNull()) return;

		LLViewerInventoryItem* item = gInventory.getItem(id);
		if (item)
		{
			LLPointer<LLViewerInventoryItem> new_item =
				new LLViewerInventoryItem(item);
			new_item->rename(name);
			new_item->updateServer(false);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
	}
}

//static
void LLPanelPermissions::onCommitDesc(LLUICtrl*, void* data)
{
	LLPanelPermissions* self = (LLPanelPermissions*)data;
	if (!self) return;

	const std::string& desc = self->mEditorObjectDesc->getText();
	gSelectMgr.selectionSetObjectDescription(desc);

	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->isAttachment() && selection->getNumNodes() == 1 &&
		!desc.empty())
	{
		const LLUUID& id = selection->getFirstObject()->getAttachmentItemID();
		if (id.isNull()) return;

		LLViewerInventoryItem* item = gInventory.getItem(id);
		if (item)
		{
			LLPointer<LLViewerInventoryItem> new_item =
				new LLViewerInventoryItem(item);
			new_item->setDescription(desc);
			new_item->updateServer(false);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
	}
}

//static
void LLPanelPermissions::onCommitSaleInfo(LLUICtrl*, void* data)
{
	LLPanelPermissions* self = (LLPanelPermissions*)data;
	if (self)
	{
		self->setAllSaleInfo();
	}
}

//static
void LLPanelPermissions::onCommitSaleType(LLUICtrl*, void* data)
{
	LLPanelPermissions* self = (LLPanelPermissions*)data;
	if (self)
	{
		self->setAllSaleInfo();
	}
}

void LLPanelPermissions::setAllSaleInfo()
{
	LLSaleInfo::EForSale sale_type = LLSaleInfo::FS_NOT;

	// Set the sale type if the object(s) are for sale.
	if (mCheckForSale->get())
	{
		switch (mRadioSaleType->getSelectedIndex())
		{
			case 0:
				sale_type = LLSaleInfo::FS_ORIGINAL;
				break;

			case 2:
				sale_type = LLSaleInfo::FS_CONTENTS;
				break;

			default:
				sale_type = LLSaleInfo::FS_COPY;
		}
	}

	S32 price = -1;

	// Do not extract the price if it is labeled as MIXED or is empty.
	const std::string& price_string = mEditorCost->getText();
	if (!price_string.empty() && price_string != mCostMixed &&
		price_string != mSaleMixed)
	{
		price = atoi(price_string.c_str());
	}
	else
	{
		price = DEFAULT_PRICE;
	}

	// If somehow an invalid price, turn the sale off.
	if (price < 0)
	{
		sale_type = LLSaleInfo::FS_NOT;
	}

	// Force the sale price of not-for-sale items to DEFAULT_PRICE.
	if (sale_type == LLSaleInfo::FS_NOT)
	{
		price = DEFAULT_PRICE;
	}

	LLSaleInfo old_sale_info;
	gSelectMgr.selectGetSaleInfo(old_sale_info);
	bool was_for_sale = old_sale_info.isForSale();

	// Pack up the sale info and send the update.
	LLSaleInfo sale_info(sale_type, price);
	gSelectMgr.selectionSetObjectSaleInfo(sale_info);
	bool set_for_sale = sale_info.isForSale();

	// Note: won't work right if a root and non-root are both single-
	// selected (here and other places).
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	bool is_perm_modify = gSelectMgr.selectGetModify() ||
						  (selection->getFirstRootNode() &&
						   gSelectMgr.selectGetRootsModify());
	bool is_nonpermanent_enforced =
		gSelectMgr.selectGetNonPermanentEnforced() ||
		(selection->getFirstRootNode() &&
		 gSelectMgr.selectGetRootsNonPermanentEnforced());
	if (!is_perm_modify || !is_nonpermanent_enforced)
	{
		return;
	}

	U8 old_click_action = 0;
	gSelectMgr.selectionGetClickAction(&old_click_action);
	if (old_click_action == CLICK_ACTION_BUY && was_for_sale && !set_for_sale)
	{
		// If turned off for-sale, make sure click-action buy is turned off as
		// well
		gSelectMgr.selectionSetClickAction(CLICK_ACTION_TOUCH);
	}
	else if (old_click_action == CLICK_ACTION_TOUCH && !was_for_sale &&
			 set_for_sale)
	{
		// If just turning on for-sale, preemptively turn on one-click buy
		// unless user has a different click action set
		gSelectMgr.selectionSetClickAction(CLICK_ACTION_BUY);
	}
}

struct LLSelectionPayable final : public LLSelectedObjectFunctor
{
	bool apply(LLViewerObject* obj) override
	{
		// can pay if you or your parent has money() event in script
		LLViewerObject* parent = (LLViewerObject*)obj->getParent();
		return obj->flagTakesMoney() || (parent && parent->flagTakesMoney());
	}
};

//static
void LLPanelPermissions::onCommitClickAction(LLUICtrl* ctrl, void*)
{
	LLComboBox* box = (LLComboBox*)ctrl;
	if (!box) return;

	U8 click_action = (U8)box->getCurrentIndex();
	if (click_action == CLICK_ACTION_BUY)
	{
		LLSaleInfo sale_info;
		gSelectMgr.selectGetSaleInfo(sale_info);
		if (!sale_info.isForSale())
		{
			gNotifications.add("CantSetBuyObject");

			// Set click action back to its old value
			U8 click_action = 0;
			gSelectMgr.selectionGetClickAction(&click_action);
			box->setCurrentByIndex((S32)click_action);

			return;
		}
	}
	else if (click_action == CLICK_ACTION_PAY)
	{
		// Verify object has script with money() handler
		LLSelectionPayable payable;
		bool can_pay = gSelectMgr.getSelection()->applyToObjects(&payable);
		if (!can_pay)
		{
			// Warn, but do it anyway.
			gNotifications.add("ClickActionNotPayable");
		}
	}
	gSelectMgr.selectionSetClickAction(click_action);
}

//static
void LLPanelPermissions::onCommitIncludeInSearch(LLUICtrl* ctrl, void*)
{
	LLCheckBoxCtrl* box = (LLCheckBoxCtrl*)ctrl;
	llassert(box);

	gSelectMgr.selectionSetIncludeInSearch(box->get());
}
