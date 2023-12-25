/**
 * @file llfloateravatarpicker.h
 * @brief was llavatarpicker.h and also replaces LL's llfloaterbanduration.h
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2019, Linden Research, Inc.
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

#ifndef LLFLOATERAVATARPICKER_H
#define LLFLOATERAVATARPICKER_H

#include <deque>
#include <vector>

#include "llfloater.h"

class LLButton;
class LLFolderView;
class LLInventoryPanel;
class LLLineEditor;
class LLMessageSystem;
class LLScrollListCtrl;
class LLTabContainer;
class LLUICtrl;

class LLFloaterAvatarPicker final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterAvatarPicker);

public:
	~LLFloaterAvatarPicker() override;

	bool postBuild() override;
	void draw() override;
	bool handleKeyHere(KEY key, MASK mask) override;

	typedef void(*callback_t)(const std::vector<std::string>&,
							  const uuid_vec_t&, void*);

	// Call this to select an avatar. The callback function will be called with
	// an avatar name and UUID. Whenever 'search_name' is passed and non-empty,
	// a search for that avatar name is also automatically launched on floater
	// opening.
	static LLFloaterAvatarPicker* show(callback_t callback, void* userdata,
									   bool allow_multiple = false,
									   bool close_on_select = false,
									   const std::string& search_name =
											LLStringUtil::null);

	static LLFloaterAvatarPicker* findInstance(const LLUUID& query_id);

	static void processAvatarPickerReply(LLMessageSystem* msg, void**);

private:
	// Do not call this directly. Use the show() method above.
	LLFloaterAvatarPicker(callback_t callback, void* userdata);

	void populateNearMe();
	void populateFriends();

	// Returns true if any items in the current tab are selected:
	bool visibleItemsSelected() const;
	void setAllowMultiple(bool allow_multiple);

	void doCallingCardSelectionChange(LLFolderView* folderp);

	void find();
	void processResponse(const LLUUID& query_id, const LLSD& content);

	static void findCoro(const std::string& url, LLUUID query_id);

	static void onCallingCardSelectionChange(LLFolderView* folderp,
											 bool user_action, void* userdata);
	static void editKeystroke(LLLineEditor* caller, void* user_data);

	static void onBtnFind(void* userdata);
	static void onBtnSelect(void* userdata);
	static void onBtnRefresh(void* userdata);
	static void onRangeAdjust(LLUICtrl* source, void* userdata);
	static void onBtnClose(void* userdata);
	static void onList(LLUICtrl* ctrl, void* userdata);
	static void onTabChanged(void* userdata, bool from_click);

private:
	LLTabContainer*				mResidentChooserTabs;
	LLPanel*					mSearchPanel;
	LLPanel*					mFriendsPanel;
	LLPanel*					mCallingCardsPanel;
	LLPanel*					mNearMePanel;
	LLScrollListCtrl*			mSearchResults;
	LLScrollListCtrl*			mFriends;
	LLScrollListCtrl*			mNearMe;
	LLInventoryPanel*			mInventoryPanel;
	LLButton*					mSelect;
	LLButton*					mFind;
	LLLineEditor*				mEdit;

	uuid_vec_t					mSelectedInventoryAvatarIDs;
	std::vector<std::string>	mSelectedInventoryAvatarNames;
	LLUUID						mQueryID;
	bool						mNearMeListComplete;
	bool						mCloseOnSelect;

	void						(*mCallback)(const std::vector<std::string>& name,
											 const uuid_vec_t& id,
											 void* userdata);
	void*						mCallbackUserdata;

	typedef fast_hset<LLFloaterAvatarPicker*> instances_list_t;
	static instances_list_t		sInstances;
};

#endif
