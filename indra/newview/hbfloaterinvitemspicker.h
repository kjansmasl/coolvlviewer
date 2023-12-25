/**
 * @file hbfloaterinvitemspicker.h
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

#ifndef LL_HBFLOATERINVENTORYPICKER_H
#define LL_HBFLOATERINVENTORYPICKER_H

#include "llfloater.h"

#include "llfolderview.h"

class LLButton;
class LLCheckBoxCtrl;
class LLFolderView;
class LLInventoryPanel;
class LLSearchEditor;
class LLTextBox;

class HBFloaterInvItemsPicker final : public LLFloater
{
protected:
	LOG_CLASS(HBFloaterInvItemsPicker);

public:
	// The callback receives a vector containing the selected inventory items
	// names, another vector containing the corresponding item UUIDs (with the
	// same index), the configured user data pointer, and a boolean which is
	// true whenever the floater gets closed immediately after the callback has
	// returned.
	typedef void(*callback_t)(const std::vector<std::string>& names,
							  const uuid_vec_t& ids, void* userdata,
							  bool on_close);

	// Call this to select one or several inventory items. The callback
	// function will be passed the selected inventory name(s) and UUID(s), if
	// any.
	// The inventory picker floater will automatically become dependent on the
	// parent floater of 'ownerp', if there is one (and if owner is not NULL,
	// of course), else it will stay independent.
	HBFloaterInvItemsPicker(LLView* ownerp, callback_t cb, void* userdata);

	~HBFloaterInvItemsPicker() override;

	// Use this method to restrict the inventory items asset type (and possibly
	// sub-type, such as for wearables and environment settings). Showing all
	// items of all types is the default behaviour when the floater is created.
	void setAssetType(LLAssetType::EType type, S32 sub_type = -1);

	// Use this method to (dis)allow multiple inventory items selection. Single
	// item selection is the default behaviour when the floater is created.
	void setAllowMultiple(bool allow_multiple = true);

	// Use this method to exclude the Library from the list of selectable items
	// (when the floater is created, the default behaviour is to show the
	// library).
	void setExcludeLibrary(bool exclude = true);

	// When 'auto_close' is true, the picker will auto-close when parented and
	// loosing focus (thus cancelling the picking action) and when the "Select"
	// button is pressed (else "Select" just invokes the callback). Auto-close
	// is the default behaviour when the floater is created.
	void setAutoClose(bool auto_close = true);

	// Causes the triggering of the callback with an empty selection when the
	// floater is closed in any way other than via the "Select" button (with
	// auto-close).
	LL_INLINE void callBackOnClose()			{ mCallBackOnClose = true; }

	// Shows or hides the "Apply immediately" check box (defaults to hidden).
	// When "Apply immediately" is shown and checked, any selection event in
	// the list triggers a callback invocation.
	void allowApplyImmediately(bool enable = true);

	// When shown, sets the "Apply immediately" check box status to checked or
	// not. Returns true on success, or false on failure (check box not shown).
	// When "Apply immediately" is shown and checked, any selection event in
	// the list triggers a callback invocation.
	bool setApplyImmediately(bool checked = true);

	// Sets the debug setting name associated with the "Apply immediately"
	// check box, enables/allows the latter, and syncs its state with the
	// corresponding debug setting (which will also be updated with the check
	// box status when changed by the user actions). The debug setting must be
	// a global one and of the boolean type (the method then succeeds and
	// returns true), otherwise this method complains with a warning and aborts
	// (returning false).
	bool setApplyImmediatelyControl(const char* control_name);

	// Sets the permissions mask for inventory filtering.
	void setFilterPermMask(PermissionMask mask);

	// Selects a given inventory object in the panel on opening, if possible
	// (the inventory object must exist, match the type, the permissions).
	// Set this *last* after any call to setAssetType() or setFilterPermMask(),
	// else the selection will not happen (it is cleared by the asset type and
	// permissions setters).
	void setSelection(const LLUUID& id);

private:
	bool postBuild() override;
	void onClose(bool app_quitting) override;
	void onFocusLost() override;

	static void onBtnSelect(void* userdata);
	static void onBtnClose(void* userdata);
	static void onSearchEdit(const std::string& search_string, void* userdata);
	static void onInventorySelectionChange(LLFolderView* folderp, bool,
										   void* userdata);

private:
	LLInventoryPanel*			mInventoryPanel;
	LLSearchEditor*				mSearchEditor;
	LLCheckBoxCtrl*				mApplyImmediatelyCheck;
	LLTextBox*					mSelectToApplyText;
	LLButton*					mSelectButton;

	LLUUID						mSelectId;

	LLSaveFolderState			mSavedFolderState;

	uuid_vec_t					mSelectedInvIDs;
	std::vector<std::string>	mSelectedInvNames;

	void						(*mCallback)(const std::vector<std::string>&,
											 const uuid_vec_t&, void*, bool);
	void*						mCallbackUserdata;

	PermissionMask				mPermissionMask;

	LLAssetType::EType			mAssetType;
	S32							mSubType;

	bool						mHasParentFloater;
	bool						mAutoClose;
	bool						mCallBackOnClose;
	bool						mCanApplyImmediately;
};

#endif
