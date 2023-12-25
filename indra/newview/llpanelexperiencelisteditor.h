/**
 * @file llpanelexperiencelisteditor.h
 * @brief Header file for LLPanelExperienceListEditor
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

#ifndef LL_LLPANELEXPERIENCELISTEDITOR_H
#define LL_LLPANELEXPERIENCELISTEDITOR_H

#include <set>

#include "boost/signals2.hpp"

#include "llpanel.h"
#include "lluuid.h"

class LLButton;
class LLFloaterExperiencePicker;
class LLScrollListCtrl;
class LLTextBox;

class LLPanelExperienceListEditor final : public LLPanel
{
public:
	typedef boost::signals2::signal<void(const LLUUID&)> list_changed_signal_t;

	// Filter function for experiences, return true if the experience should be
	// hidden.
	typedef boost::function<bool(const LLSD&)> experience_function;

	typedef std::vector<experience_function> filter_list;

	typedef LLHandle<LLFloaterExperiencePicker> PickerHandle;

	LLPanelExperienceListEditor();
	~LLPanelExperienceListEditor() override;

	bool postBuild() override;

	void loading();

	LL_INLINE const uuid_list_t& getExperienceIds() const
	{
		return mExperienceIds;
	}

	void setExperienceIds(const LLSD& experience_ids);
	void addExperienceIds(const uuid_vec_t& experience_ids);

	void addExperience(const LLUUID& id);

	boost::signals2::connection setAddedCallback(list_changed_signal_t::slot_type cb);
	boost::signals2::connection setRemovedCallback(list_changed_signal_t::slot_type cb);

	void setReadonly(bool val = true);
	LL_INLINE bool getReadonly() const					{ return mReadonly; }

	void setDisabled(bool val = true);
	LL_INLINE bool getDisabled() const					{ return mDisabled; }

	void refreshExperienceCounter();

	LL_INLINE void addFilter(experience_function func)	{ mFilters.push_back(func); }

	LL_INLINE void setStickyFunction(experience_function func)
	{
		mSticky = func;
	}

	LL_INLINE U32 getMaxExperienceIDs() const			{ return mMaxExperienceIDs; }
	LL_INLINE void setMaxExperienceIDs(U32 val)			{ mMaxExperienceIDs = val; }

private:
	static void onItems(LLUICtrl* ctrl, void* data);
	static void onRemove(void* data);
	static void onAdd(void* data);
	static void onProfile(void* data);

	static void checkButtonsEnabled(LLUICtrl*, void* data);

	static void experienceDetailsCallback(LLHandle<LLPanelExperienceListEditor> panel,
										  const LLSD& experience);

	void onExperienceDetails(const LLSD& experience);
	void processResponse(const LLSD& content);

private:
	LLButton*					mAdd;
	LLButton*					mRemove;
	LLButton*					mProfile;
	LLScrollListCtrl*			mItems;
	LLTextBox*					mItemsCount;

	PickerHandle				mPicker;

	LLUUID						mKey;
	experience_function			mSticky;
	U32							mMaxExperienceIDs;

	list_changed_signal_t		mAddedCallback;
	list_changed_signal_t		mRemovedCallback;

	filter_list					mFilters;
	uuid_list_t					mExperienceIds;

	bool						mReadonly;
	bool						mDisabled;
	bool						mListEmpty;
};

#endif //LL_LLPANELEXPERIENCELISTEDITOR_H
