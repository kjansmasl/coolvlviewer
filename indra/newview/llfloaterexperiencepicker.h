/**
 * @file llfloaterexperiencepicker.h
 * @brief Header file for LLFloaterExperiencePicker and LLPanelExperiencePicker
 * @author dolphin@lindenlab.com
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

#ifndef LL_LLFLOATEREXPERIENCEPICKER_H
#define LL_LLFLOATEREXPERIENCEPICKER_H

#include "boost/function.hpp"

#include "llavatarnamecache.h"
#include "llfloater.h"

class LLButton;
class LLComboBox;
class LLLineEditor;
class LLScrollListCtrl;

class LLPanelExperiencePicker : public LLPanel
{
	friend class LLExperienceSearchResponder;
	friend class LLFloaterExperiencePicker;

public:
	typedef boost::function<void(const uuid_vec_t&)> select_callback_t;

	// filter function for experiences, return true if the experience should
	// be hidden.

	typedef boost::function<bool(const LLSD&)> filter_function;

	typedef std::vector<filter_function> filter_list;

	LLPanelExperiencePicker();

	bool postBuild() override;

	void hideOkCancel();

	LL_INLINE void addFilter(filter_function func)	{ mFilters.push_back(func); }

	template <class IT>
	LL_INLINE void addFilters(IT begin, IT end)
	{
		mFilters.insert(mFilters.end(), begin, end);
	}

	void setDefaultFilters();

	bool FilterOverRating(const LLSD& experience);

private:
	void closeParent();

	void getSelectedExperienceIds(const LLScrollListCtrl* results,
								  uuid_vec_t& experience_ids);
	void setAllowMultiple(bool allow_multiple);

	void find();
	static void findResults(LLHandle<LLPanelExperiencePicker> handle,
						    LLUUID query_id, LLSD result);

	bool isSelectButtonEnabled();
	void processResponse(const LLUUID& query_id, const LLSD& content);

	void filterContent();
	bool isExperienceHidden(const LLSD& experience) const;

	static void onBtnFind(void* data);
	static void onBtnSelect(void* data);
	static void onBtnClose(void* data);
	static void onBtnProfile(void* data);
	static void onNextPage(void* data);
	static void onPrevPage(void* data);

	static void onList(LLUICtrl* ctrl, void* data);
	static void onMaturity(LLUICtrl* ctrl, void* data);

	static void nameCallback(const LLHandle<LLPanelExperiencePicker>& floater,
							 const LLUUID& experience_id,
							 const LLUUID& agent_id,
							 const LLAvatarName& av_name);

private:
	LLButton*			mOkBtn;
	LLButton*			mCancelBtn;
	LLButton*			mProfileBtn;
	LLButton*			mNextBtn;
	LLButton*			mPrevBtn;
	LLComboBox*			mMaturityCombo;
	LLLineEditor*		mLineEditor;
	LLScrollListCtrl*	mSearchResultsList;

	S32					mCurrentPage;
	LLUUID				mQueryID;
	select_callback_t	mSelectionCallback;
	filter_list			mFilters;
	LLSD				mResponse;
	bool				mCloseOnSelect;
};

class LLFloaterExperiencePicker final : public LLFloater
{
public:
	LLFloaterExperiencePicker(const LLUUID& key);
	~LLFloaterExperiencePicker() override;

	typedef boost::function<void(const uuid_vec_t&)> select_callback_t;
	// Filter function for experiences, return true if the experience should be
	// hidden.
	typedef boost::function<bool(const LLSD&)> filter_function;
	typedef std::vector<filter_function> filter_list;

	static LLFloaterExperiencePicker* show(select_callback_t callback,
										   const LLUUID& key,
										   bool allow_multiple,
										   bool close_on_select,
										   filter_list filters);

private:
	static void* createSearchPanel(void* data);

private:
	LLUUID						mKey;
	LLPanelExperiencePicker*	mSearchPanel;

	typedef fast_hmap<LLUUID, LLFloaterExperiencePicker*> instances_map_t;
	static instances_map_t		sInstancesMap;
};

#endif // LL_LLFLOATEREXPERIENCEPICKER_H
