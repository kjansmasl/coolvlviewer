/**
 * @file llfloaterinventory.h
 * @brief Inventory floaters classes definitions.
 * class definition
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

#ifndef LL_LLFLOATERINVENTORY_H
#define LL_LLFLOATERINVENTORY_H

#include <set>
#include <vector>

#include "llfloater.h"

#include "llinventorymodel.h"

class LLCheckBoxCtrl;
class LLFloaterInventory;
class LLFolderView;
class LLInventoryFilter;
class LLInventoryPanel;
class LLSaveFolderState;
class LLSearchEditor;
class LLSpinCtrl;
class LLTabContainer;

class LLFloaterInventoryFilters : public LLFloater
{
public:
	LLFloaterInventoryFilters(const std::string& name, const LLRect& rect,
							  LLFloaterInventory* inv);

	bool postBuild() override;
	void draw() override;
	void onClose(bool app_quitting) override;

	void changeFilter(LLInventoryFilter* filter);
	void updateElementsFromFilter();

	static void onTimeAgo(LLUICtrl*, void* userdata);
	static void onResetFilters(void* userdata);
	static void onCloseBtn(void* userdata);
	static void selectAllTypes(void* userdata);
	static void selectNoTypes(void* userdata);

protected:
	LLFloaterInventory*	mInventoryView;
	LLInventoryFilter*	mFilter;

	LLCheckBoxCtrl*		mCheckSinceLogoff;
	LLCheckBoxCtrl*		mCheckShowEmpty;
	LLCheckBoxCtrl*		mCheckAnimation;
	LLCheckBoxCtrl*		mCheckCallingcard;
	LLCheckBoxCtrl*		mCheckClothing;
	LLCheckBoxCtrl*		mCheckGesture;
	LLCheckBoxCtrl*		mCheckLandmark;
	LLCheckBoxCtrl*		mCheckMaterial;
	LLCheckBoxCtrl*		mCheckNotecard;
	LLCheckBoxCtrl*		mCheckObject;
	LLCheckBoxCtrl*		mCheckScript;
	LLCheckBoxCtrl*		mCheckSnapshot;
	LLCheckBoxCtrl*		mCheckSound;
	LLCheckBoxCtrl*		mCheckTexture;
#if LL_MESH_ASSET_SUPPORT
	LLCheckBoxCtrl*		mCheckMesh;
#endif
	LLCheckBoxCtrl*		mCheckSettings;

	LLSpinCtrl*			mSpinSinceDays;
	LLSpinCtrl*			mSpinSinceHours;

	bool				mHasMaterial;
};

class LLFloaterInventory final : public LLFloater, LLInventoryObserver
{
friend class LLFloaterInventoryFilters;

protected:
	LOG_CLASS(LLFloaterInventory);

	// Internal initialization code
	void init(LLInventoryModel* modelp);

public:
	LLFloaterInventory(const std::string& name, const std::string& rect,
					   LLInventoryModel* modelp);
	LLFloaterInventory(const std::string& name, const LLRect& rect,
					   LLInventoryModel* modelp);
	~LLFloaterInventory() override;

	void changed(U32 mask) override;

	bool postBuild() override;

	//
	// Misc functions
	//
	void setFilterTextFromFilter();

	void startSearch();

	// This method makes sure that an inventory floater exists, is visible and
	// has focus. The chosen view is returned.
	static LLFloaterInventory* showAgentInventory();

	// Return the active inventory floater if there is one. Active is defined
	// as the floater that is the closest to the front and is visible.
	static LLFloaterInventory* getActiveFloater();

	// This method calls showAgentInventory() if no views are visible, or hides
	// and destroys them all if any are visible.
	static void toggleVisibility(void* dummy = NULL);

	// Final cleanup, destroy all open inventory floaters.
	static void cleanup();

	// LLView & LLFloater functionality
	void onClose(bool app_quitting) override;
	void setVisible(bool visible) override;
	void draw() override;
	bool handleKeyHere(KEY key, MASK mask) override;

	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept,
						   std::string& tooltip_msg) override;


	LL_INLINE LLInventoryPanel* getPanel()				{ return mActivePanel; }
	LL_INLINE LLInventoryPanel* getActivePanel()		{ return mActivePanel; }

	const std::string getFilterSubString();

	void setFilterSubString(const std::string& string);

	void toggleFindOptions();
	void updateSortControls();

	LL_INLINE LLFloaterInventoryFilters* getInvFilters()
	{
		return (LLFloaterInventoryFilters*)mInvFiltersHandle.get();
	}

protected:
	static void onClearSearch(void* userdata);
	static void onSearchEdit(const std::string& search_string, void* userdata);
	static void onFilterSelected(void* userdata, bool);
	static void onSelectionChange(LLFolderView* folderp, bool user_action,
								  void* userdata);
	static void onCommitLockLastOpenCheck(LLUICtrl* ctrl, void* userdata);

protected:
	LLSearchEditor*		mSearchEditor;
	LLTabContainer*		mFilterTabs;
	LLHandle<LLFloater>	mInvFiltersHandle;
	LLInventoryPanel*	mActivePanel;
	LLInventoryPanel*	mEverythingPanel;
	LLInventoryPanel*	mRecentPanel;
	LLInventoryPanel*	mWornPanel;
	LLInventoryPanel*	mLastOpenPanel;
	LLSaveFolderState*	mSavedFolderState;
	LLCheckBoxCtrl*		mLockLastOpenCheck;
	LLView*				mNewSettingsMenuItem;
	LLView*				mNewMaterialMenuItem;

	std::string			mFilterText;

private:
	S32					mLastCount;
	std::string			mLastCountString;

	// This container is used to hold all active inventory floaters. This is
	// here to support the inventory toggle show button.
	static std::vector<LLFloaterInventory*> sActiveViews;
};

constexpr bool TAKE_FOCUS_YES = true;
constexpr bool TAKE_FOCUS_NO = false;

#endif // LL_LLFLOATERINVENTORY_H
