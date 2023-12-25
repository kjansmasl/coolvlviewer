/**
 * @file hbfloaterinvitemspicker.cpp
 * @brief Generic inventory items picker.
 * Also replaces LL's environment settings and materials pickers.
 *
 * $LicenseInfo:firstyear=2019&license=viewergpl$
 *
 * Copyright (c) 2019-2023, Henri Beauchamp
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

#include <deque>

#include "hbfloaterinvitemspicker.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"						// For gAgentID
#include "llinventorymodel.h"
#include "llviewercontrol.h"

HBFloaterInvItemsPicker::HBFloaterInvItemsPicker(LLView* ownerp, callback_t cb,
												 void* userdata)
:	mCallback(cb),
	mCallbackUserdata(userdata),
	mPermissionMask(PERM_NONE),			// No constraint on permissions.
	mAssetType(LLAssetType::AT_NONE),	// No constraint on asset type.
	mSubType(-1),						// No constraint on asset sub-type.
	mHasParentFloater(false),
	mAutoClose(true),
	mCallBackOnClose(false),
	mCanApplyImmediately(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_inv_items_picker.xml");
	// Note: at this point postBuild() has been called and returned.
	LLView* parentp = ownerp;
	// Search for our owner's parent floater and register as dependent of
	// it if found.
	while (parentp)
	{
		LLFloater* floaterp = parentp->asFloater();
		if (floaterp)
		{
			mHasParentFloater = true;
			floaterp->addDependentFloater(this);
			break;
		}
		parentp = parentp->getParent();
	}
	if (!parentp)
	{
		// Place ourselves in a smart way, like preview floaters...
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		translate(left - getRect().mLeft, top - getRect().mTop);
		gFloaterViewp->adjustToFitScreen(this);
	}
}

//virtual
HBFloaterInvItemsPicker::~HBFloaterInvItemsPicker()
{
	gFocusMgr.releaseFocusIfNeeded(this);
}

//virtual
bool HBFloaterInvItemsPicker::postBuild()
{
	mSearchEditor = getChild<LLSearchEditor>("search_editor");
	mSearchEditor->setSearchCallback(onSearchEdit, this);

	mInventoryPanel = getChild<LLInventoryPanel>("inventory_panel");
	mInventoryPanel->setFollowsAll();
	mInventoryPanel->getRootFolder()->setCanAutoSelect(false);
	mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
	mInventoryPanel->setSelectCallback(onInventorySelectionChange, this);

	mApplyImmediatelyCheck = getChild<LLCheckBoxCtrl>("apply_immediate_check");
	mApplyImmediatelyCheck->setVisible(mCanApplyImmediately);

	mSelectToApplyText = getChild<LLTextBox>("select_to_apply_text");
	mSelectToApplyText->setVisible(!mCanApplyImmediately);

	mSelectButton = getChild<LLButton>("select_btn");
	mSelectButton->setClickedCallback(onBtnSelect, this);
	mSelectButton->setEnabled(false);

	childSetAction("close_btn", onBtnClose, this);

	setAllowMultiple(false);

	return true;
}

//virtual
void HBFloaterInvItemsPicker::onClose(bool app_quitting)
{
	if (!app_quitting && mCallBackOnClose)
	{
		// Send an empty selection on "Cancel" action.
		mCallback(std::vector<std::string>(), uuid_vec_t(), mCallbackUserdata,
				  true);
	}
	LLFloater::onClose(app_quitting);
}

//virtual
void HBFloaterInvItemsPicker::onFocusLost()
{
	// NOTE: never auto-close when loosing focus if not parented
	if (mAutoClose && mHasParentFloater)
	{
		close();
	}
	else
	{
		LLFloater::onFocusLost();
	}
}

void HBFloaterInvItemsPicker::setAllowMultiple(bool allow_multiple)
{
	mInventoryPanel->setAllowMultiSelect(allow_multiple);
}

void HBFloaterInvItemsPicker::setExcludeLibrary(bool exclude)
{
	mInventoryPanel->setFilterHideLibrary(exclude);
}

void HBFloaterInvItemsPicker::allowApplyImmediately(bool enable)
{
	mCanApplyImmediately = enable;
	mApplyImmediatelyCheck->setVisible(enable);
	mSelectToApplyText->setVisible(!enable);
}

bool HBFloaterInvItemsPicker::setApplyImmediately(bool checked)
{
	if (mCanApplyImmediately)
	{
		mApplyImmediatelyCheck->set(checked);
		return true;
	}
	return false;
}

bool HBFloaterInvItemsPicker::setApplyImmediatelyControl(const char* ctrl_name)
{
	LLControlVariable* controlp = gSavedSettings.getControl(ctrl_name);
	if (!controlp || controlp->type() != TYPE_BOOLEAN)
	{
		llwarns << "No such boolean global debug setting found: " << ctrl_name
				<< llendl;
		return false;
	}
	allowApplyImmediately(true);
	setApplyImmediately(controlp->getValue().asBoolean());
	mApplyImmediatelyCheck->setControlName(ctrl_name, NULL);
	return true;
}

void HBFloaterInvItemsPicker::setAssetType(LLAssetType::EType type,
										   S32 sub_type)
{
	// Just in case: new asset type and sub-type may not be suitable for any
	// set Id.
	mSelectId.setNull();

	mAssetType = type;
	mSubType = sub_type;
	U32 filter = 1 << LLInventoryType::defaultForAssetType(type);
	mInventoryPanel->setFilterTypes(filter);
	mInventoryPanel->setFilterSubType(sub_type);
	mInventoryPanel->openDefaultFolderForType(type);
	// Set the floater title according to the type of asset we want to pick
	std::string type_name = LLAssetType::lookupHumanReadable(type);
	LLUIString title = getString("title");
	LLSD args;
	args["ASSETTYPE"] = LLTrans::getString(type_name);
	title.setArgs(args);
	setTitle(title);
}

void HBFloaterInvItemsPicker::setFilterPermMask(PermissionMask mask)
{
	// Do not reapply the same mask to avoid pointeless refiltering.
	if (mask != mPermissionMask)
	{
		// Just in case: new permissions may not be suitable for any set Id.
		mSelectId.setNull();

		mPermissionMask = mask;
		mInventoryPanel->setFilterPermMask(mask);
	}
}

void HBFloaterInvItemsPicker::setSelection(const LLUUID& id)
{
	mSelectId.setNull();	// Reset any pending selection

	LLViewerInventoryItem* itemp = gInventory.getItem(id);
	if (!itemp)
	{
		llwarns << "Could not find any inventory item for Id: " << id
				<< llendl;
		return;
	}
	if (mAssetType != LLAssetType::AT_NONE && itemp->getType() != mAssetType)
	{
		llwarns << "Inventory item of wrong asset type for Id: " << id
				<< llendl;
		return;
	}
	if (mSubType != -1 && itemp->getSubType() != mSubType)
	{
		llwarns << "Inventory item of wrong asset sub-type for Id: " << id
				<< llendl;
		return;
	}
	if (mPermissionMask != PERM_NONE)
	{
		bool good_perms = true;
		const LLPermissions& perms = itemp->getPermissions();
		if (mPermissionMask & PERM_COPY)
		{
			good_perms = perms.allowCopyBy(gAgentID);
		}
		if (good_perms && (mPermissionMask & PERM_TRANSFER))
		{
			good_perms = perms.allowTransferBy(gAgentID);
		}
		if (good_perms && (mPermissionMask & PERM_MODIFY))
		{
			good_perms = perms.allowModifyBy(gAgentID);
		}
		if (!good_perms)
		{
			llwarns << "Inventory item of wrong permissions for Id: " << id
					<< llendl;
			return;
		}
	}
	mSelectId = id;
}

//static
void HBFloaterInvItemsPicker::onBtnSelect(void* userdata)
{
	HBFloaterInvItemsPicker* self = (HBFloaterInvItemsPicker*)userdata;
	if (!self) return;

	self->mCallback(self->mSelectedInvNames, self->mSelectedInvIDs,
					self->mCallbackUserdata, self->mAutoClose);

	self->mInventoryPanel->setSelection(LLUUID::null, false);

	if (self->mAutoClose)
	{
		self->mAutoClose = self->mCallBackOnClose = false;
		self->close();
	}
}

//static
void HBFloaterInvItemsPicker::onBtnClose(void* userdata)
{
	HBFloaterInvItemsPicker* self = (HBFloaterInvItemsPicker*)userdata;
	if (self)
	{
		self->mAutoClose = false;
		self->close();
	}
}

void HBFloaterInvItemsPicker::onSearchEdit(const std::string& search_str,
										   void* userdata)
{
	HBFloaterInvItemsPicker* self = (HBFloaterInvItemsPicker*)userdata;
	if (!self) return;	// Paranoia

	LLFolderView* folderp = self->mInventoryPanel->getRootFolder();
	if (search_str.empty())
	{
		if (self->mInventoryPanel->getFilterSubString().empty())
		{
			// Current and new filters are empty: nothing to do !
			return;
		}

		self->mSavedFolderState.setApply(true);
		folderp->applyFunctorRecursively(self->mSavedFolderState);
		// Add folder with current item to the list of previously opened
		// folders
		LLOpenFoldersWithSelection opener;
		folderp->applyFunctorRecursively(opener);
		folderp->scrollToShowSelection();

	}
	else if (self->mInventoryPanel->getFilterSubString().empty())
	{
		// User just typed the first letter in the search editor; save existing
		// folder open state.
		if (!folderp->isFilterModified())
		{
			self->mSavedFolderState.setApply(false);
			folderp->applyFunctorRecursively(self->mSavedFolderState);
		}
	}

	std::string uc_search_str = search_str;
	LLStringUtil::toUpper(uc_search_str);
	self->mInventoryPanel->setFilterSubString(uc_search_str);
}

//static
void HBFloaterInvItemsPicker::onInventorySelectionChange(LLFolderView* folderp,
														 bool, void* userdata)
{
	HBFloaterInvItemsPicker* self = (HBFloaterInvItemsPicker*)userdata;
	if (!self || !folderp)	// Paranoia
	{
		return;
	}

	const LLFolderView::selected_items_t& items = folderp->getSelectedItems();
	if (self->mSelectId.notNull())
	{
		bool selected = false;
		for (std::deque<LLFolderViewItem*>::const_iterator it = items.begin(),
														   end = items.end();
			 it != end; ++it)
		{
			LLFolderViewEventListener* listenerp = (*it)->getListener();
			if (listenerp && listenerp->getUUID() == self->mSelectId)
			{
				selected = true;
				break;
			}
		}
		if (!selected)
		{
			self->mInventoryPanel->setSelection(self->mSelectId, true);
			return;
		}
		self->mSelectId.setNull();
	}

	self->mSelectedInvIDs.clear();
	self->mSelectedInvNames.clear();

	for (std::deque<LLFolderViewItem*>::const_iterator it = items.begin(),
													   end = items.end();
		 it != end; ++it)
	{
		LLFolderViewEventListener* listenerp = (*it)->getListener();
		if (!listenerp) continue;	// Paranoia

		LLInventoryType::EType type = listenerp->getInventoryType();
		if (type != LLInventoryType::IT_CATEGORY &&
			type != LLInventoryType::IT_ROOT_CATEGORY)
		{
			LLInventoryItem* itemp = gInventory.getItem(listenerp->getUUID());
			if (itemp)	// Paranoia
			{
				self->mSelectedInvIDs.emplace_back(itemp->getUUID());
				self->mSelectedInvNames.emplace_back(listenerp->getName());
			}
		}
	}

	bool has_selection = !self->mSelectedInvIDs.empty();
	self->mSelectButton->setEnabled(has_selection);
	if (has_selection && self->mCanApplyImmediately &&
		self->mApplyImmediatelyCheck->get())
	{
		self->mCallback(self->mSelectedInvNames, self->mSelectedInvIDs,
						self->mCallbackUserdata, false);
	}
}
