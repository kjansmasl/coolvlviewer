/**
 * @file hbfloatermakenewoutfit.h
 * @brief The Make New Outfit floater - header file
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011-2015 Henri Beauchamp
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

#ifndef LL_HBFLOATERMAKENEWOUTFIT_H
#define LL_HBFLOATERMAKENEWOUTFIT_H

#include "llfloater.h"

class LLButton;
class LLCheckBoxCtrl;
class LLScrollListCtrl;

class HBFloaterMakeNewOutfit final
:	public LLFloater, public LLFloaterSingleton<HBFloaterMakeNewOutfit>
{
	friend class LLUISingleton<HBFloaterMakeNewOutfit,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterMakeNewOutfit);

public:
	~HBFloaterMakeNewOutfit() override;

	void getIncludedItems(uuid_vec_t& wearables_to_include,
						  uuid_vec_t& attachments_to_include);

	static void setDirty();

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterMakeNewOutfit(const LLSD&);

	bool postBuild() override;
	void draw() override;

	bool hasCheckedItems();

	static void onCommitWearableList(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBox(LLUICtrl*, void* user_data);
	static void onCommitCheckBoxLinkAll(LLUICtrl* ctrl, void* user_data);
	static void onButtonSave(void* user_data);
	static void onButtonCancel(void* user_data);

private:
	LLButton*			mSaveButton;
	LLCheckBoxCtrl*		mShapeCheck;
	LLCheckBoxCtrl*		mSkinCheck;
	LLCheckBoxCtrl*		mHairCheck;
	LLCheckBoxCtrl*		mEyesCheck;
	LLCheckBoxCtrl*		mUseAllLinksCheck;
	LLCheckBoxCtrl*		mUseClothesLinksCheck;
	LLCheckBoxCtrl*		mUseNoCopyLinksCheck;
	LLCheckBoxCtrl*		mRenameCheck;
	LLScrollListCtrl*	mAttachmentsList;
	LLScrollListCtrl*	mWearablesList;

	bool				mIsDirty;
	bool				mSaveStatusDirty;

	static uuid_list_t	sFetchingRequests;
	static uuid_list_t	sUnderpants;
	static uuid_list_t	sUndershirts;
};

#endif	// LL_HBFLOATERMAKENEWOUTFIT_H
