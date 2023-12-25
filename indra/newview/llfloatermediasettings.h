/**
 * @file llfloatermediasettings.h
 * @brief Media settings floater - class definition
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLFLOATERMEDIASETTINGS_H
#define LL_LLFLOATERMEDIASETTINGS_H

#include "llerror.h"
#include "llfloater.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLMediaCtrl;
class LLNameEditor;
class LLScrollListCtrl;
class LLSpinCtrl;
class LLTabContainer;
class LLTextBox;

class LLFloaterMediaSettings final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterMediaSettings>
{
	friend class LLUISingleton<LLFloaterMediaSettings,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterMediaSettings);

public:
	bool postBuild() override;
	void draw() override;
	void close(bool app_quitting = false) override;

	static void initValues(LLSD& media_settings, bool editable);
	static void clearValues(bool editable);

	static void setHasMediaInfo(bool b)				{ sIdenticalHasMediaInfo = b; }
	static bool getHasMediaInfo()					{ return sIdenticalHasMediaInfo; }
	static void setMultipleMedia(bool b)			{ sMultipleMedia = b; }
	static bool getMultipleMedia()					{ return sMultipleMedia; }
	static void setMultipleValidMedia(bool b)		{ sMultipleValidMedia = b; }
	static bool getMultipleValidMedia()				{ return sMultipleValidMedia; }
	static const std::string getHomeUrl();

private:
	LLFloaterMediaSettings(const LLSD&);

	void getValues(LLSD& fill_me_in, bool include_tentative);
	bool haveValuesChanged();
	void apply();

	void updateMediaPreview();
	bool checkHomeUrlPassesWhitelist();
	bool urlPassesWhiteList(const std::string& test_url);
	const std::string makeValidUrl(const std::string& src_url);
	void updateWhitelistEnableStatus();
	void updateCurrentUrl();
	bool navigateHomeSelectedFace(bool only_if_current_is_empty);
	void commitFields();
	void addWhiteListEntry(const std::string& url);

	static void createInstance();
	static bool isMultiple();

	// Callbacks
	static void onTabChanged(void* userdata, bool from_click);
	static void onCommitHomeURL(LLUICtrl* ctrl, void* userdata);
	static void onCommitNewPattern(LLUICtrl* ctrl, void* userdata);
	static void onBtnResetCurrentUrl(void *userdata);
	static void onBtnOK(void* userdata);
	static void onBtnCancel(void* userdata);
	static void onBtnApply(void* userdata);
	static void onBtnDel(void* userdata);

private:
	LLButton*			mOKBtn;
	LLButton*			mCancelBtn;
	LLButton*			mApplyBtn;
	LLButton*			mResetCurrentUrlBtn;
	LLButton*			mDeleteBtn;
	LLTextBox*			mCurrentUrlLabel;
	LLTextBox*			mCurrentURL;
	LLTextBox*			mFailWhiteListText;
	LLTextBox*			mHomeUrlFailsWhiteListText;
	LLSpinCtrl*			mWidthPixels;
	LLSpinCtrl*			mHeightPixels;
	LLComboBox*			mControls;
	LLMediaCtrl*		mPreviewMedia;
	LLLineEditor*		mHomeURL;
	LLLineEditor*		mNewWhiteListPattern;
	LLNameEditor*		mPermsGroupName;
	LLCheckBoxCtrl*		mAutoLoop;
	LLCheckBoxCtrl*		mFirstClick;
	LLCheckBoxCtrl*		mAutoZoom;
	LLCheckBoxCtrl*		mAutoPlay;
	LLCheckBoxCtrl*		mAutoScale;
	LLCheckBoxCtrl*		mPermsOwnerInteract;
	LLCheckBoxCtrl*		mPermsOwnerControl;
	LLCheckBoxCtrl*		mPermsGroupInteract;
	LLCheckBoxCtrl*		mPermsGroupControl;
	LLCheckBoxCtrl*		mPermsWorldInteract;
	LLCheckBoxCtrl*		mPermsWorldControl;
	LLCheckBoxCtrl*		mEnableWhiteList;
	LLScrollListCtrl*	mWhiteListList;
	LLTabContainer*		mTabContainer;

	LLUUID				mGroupId;
	LLSD				mInitialValues;

	bool				mFirstRun;
	bool				mMediaEditable;
	bool				mHomeUrlCommitted;

	static bool			sIdenticalHasMediaInfo;
	static bool			sMultipleMedia;
	static bool			sMultipleValidMedia;
};

#endif	// LL_LLFLOATERMEDIASETTINGS_H
