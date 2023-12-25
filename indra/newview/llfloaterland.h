/**
 * @file llfloaterland.h
 * @author James Cook
 * @brief "About Land" floater, allowing display and editing of land parcel properties.
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

#ifndef LL_LLFLOATERLAND_H
#define LL_LLFLOATERLAND_H

#include <set>
#include <vector>

#include "llfloater.h"
#include "llsafehandle.h"

typedef std::set<LLUUID> owners_list_t;
constexpr F32 CACHE_REFRESH_TIME = 2.5f;

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLFloaterLandParcelSelectObserver;
class LLLineEditor;
class LLMessageSystem;
class LLNameListCtrl;
class LLPanelExperienceListEditor;
class LLPanelLandAccess;
class LLPanelLandAudio;
class LLPanelLandBan;
class LLPanelLandCovenant;
class HBPanelLandEnvironment;
class LLPanelLandExperiences;
class LLPanelLandGeneral;
class LLPanelLandMedia;
class LLPanelLandObjects;
class LLPanelLandOptions;
class LLPanelLandRenters;
class LLParcelSelection;
class LLRadioGroup;
class LLSpinCtrl;
class LLTabContainer;
class LLTextBox;
class LLTextEditor;
class LLTextureCtrl;
class LLUIImage;
class LLViewerTextEditor;

class LLFloaterLand final : public LLFloater,
							public LLFloaterSingleton<LLFloaterLand>
{
	friend class LLUISingleton<LLFloaterLand, VisibilityPolicy<LLFloater> >;

public:
	static LLPanelLandObjects* getCurrentPanelLandObjects();
	static LLPanelLandCovenant* getCurrentPanelLandCovenant();

	// Destroys itself on close.
	void onClose(bool app_quitting) override;
	void onOpen() override;
	bool postBuild() override;
	void refresh() override;

protected:
	// Does its own instance management, so clients not allowed to allocate or
	// destroy.
	LLFloaterLand(const LLSD&);
	~LLFloaterLand() override;

	static void* createPanelLandGeneral(void* data);
	static void* createPanelLandCovenant(void* data);
	static void* createPanelLandObjects(void* data);
	static void* createPanelLandOptions(void* data);
	static void* createPanelLandAudio(void* data);
	static void* createPanelLandMedia(void* data);
	static void* createPanelLandAccess(void* data);

public:
	// When closing the dialog, we want to deselect the land. But when we
	// send an update to the simulator, it usually replies with the parcel
	// information, causing the land to be reselected. This allows us to
	// suppress that behavior.
	static bool sRequestReplyOnUpdate;

protected:
	static LLFloaterLandParcelSelectObserver* sObserver;
	static S32 sLastTab;

	LLTabContainer*			mTabLand;
	LLPanelLandGeneral*		mPanelGeneral;
	LLPanelLandObjects*		mPanelObjects;
	LLPanelLandOptions*		mPanelOptions;
	LLPanelLandAudio*		mPanelAudio;
	LLPanelLandMedia*		mPanelMedia;
	LLPanelLandAccess*		mPanelAccess;
	LLPanelLandCovenant*	mPanelCovenant;
	LLPanelLandExperiences*	mPanelExperiences;
	HBPanelLandEnvironment*	mPanelEnvironment;

	LLSafeHandle<LLParcelSelection>	mParcel;
};

class LLPanelLandGeneral final : public LLPanel
{
public:
	LLPanelLandGeneral(LLSafeHandle<LLParcelSelection>& parcelp);
	virtual ~LLPanelLandGeneral();

	bool postBuild() override;
	void refresh() override;
	void draw() override;

	void refreshNames();

	void setGroup(const LLUUID& group_id);

	// used in llviewermenu.cpp
	static bool enableBuyPass(void*);
	// also used in lltoolpie.cpp
	static void onClickBuyPass(void* deselect_when_done);

private:
	static void onClickProfile(void*);
	static void onClickSetGroup(void*);
	static void cbGroupID(LLUUID group_id, void* userdata);
#if 0	// unused
	static bool enableDeedToGroup(void*);
#endif
	static void onClickDeed(void*);
	static void onClickBuyLand(void* data);
	static void onClickRelease(void*);
	static void onClickReclaim(void*);
	static void onCommitAny(LLUICtrl* ctrl, void* userdata);

	static bool cbBuyPass(const LLSD& notification, const LLSD& response);

	static void onClickSellLand(void* data);
	static void onClickStopSellLand(void* data);
	static void onClickSet(void* data);
	static void onClickClear(void* data);
	static void onClickShow(void* data);
	static void onClickStartAuction(void*);

private:
	// true only when verifying land information when land is for sale on sale
	// info change:
	bool			mUncheckedSell;

	LLTextBox*		mLabelName;
	LLLineEditor*	mEditName;
	LLTextBox*		mLabelDesc;
	LLTextEditor*	mEditDesc;

	LLTextBox*		mTextSalePending;

 	LLButton*		mBtnDeedToGroup;
 	LLButton*		mBtnSetGroup;

	LLTextBox*		mTextOwner;
	LLButton*		mBtnProfile;

	LLTextBox*		mContentRating;
	LLTextBox*		mLandType;

	LLTextBox*		mTextGroup;
	LLTextBox*		mTextClaimDateLabel;
	LLTextBox*		mTextClaimDate;

	LLTextBox*		mTextPriceLabel;
	LLTextBox*		mTextPrice;

	LLCheckBoxCtrl* mCheckDeedToGroup;
	LLCheckBoxCtrl* mCheckContributeWithDeed;

	LLTextBox*		mSaleInfoForSale1;
	LLTextBox*		mSaleInfoForSale2;
	LLTextBox*		mSaleInfoForSaleObjects;
	LLTextBox*		mSaleInfoForSaleNoObjects;
	LLTextBox*		mSaleInfoNotForSale;
	LLButton*		mBtnSellLand;
	LLButton*		mBtnStopSellLand;

	LLTextBox*		mTextDwell;

	LLButton*		mBtnBuyLand;
	LLButton*		mBtnBuyGroupLand;

	// These buttons share the same location, but reclaim is in exactly the
	// same visual place, and is only shown for estate owners on their estate
	// since they cannot release land.
	LLButton*		mBtnReleaseLand;
	LLButton*		mBtnReclaimLand;

	LLButton*		mBtnBuyPass;
	LLButton*		mBtnStartAuction;

	std::string		mAnyoneText;

	LLSafeHandle<LLParcelSelection>&	mParcel;

	static LLHandle<LLFloater> sBuyPassDialogHandle;
};

class LLPanelLandObjects final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelLandObjects);

public:
	LLPanelLandObjects(LLSafeHandle<LLParcelSelection>& parcelp);
	~LLPanelLandObjects() override;

	bool postBuild() override;
	void refresh() override;

	static void processParcelObjectOwnersReply(LLMessageSystem* msg, void**);

private:
	bool callbackReturnOwnerObjects(const LLSD& notification,
									const LLSD& response);
	bool callbackReturnGroupObjects(const LLSD& notification,
									const LLSD& response);
	bool callbackReturnOtherObjects(const LLSD& notification,
									const LLSD& response);
	bool callbackReturnOwnerList(const LLSD& notification,
								 const LLSD& response);

	static void clickShowCore(LLPanelLandObjects* panelp, S32 return_type,
							  owners_list_t* list = 0);

	static void onClickShowOwnerObjects(void*);
	static void onClickShowGroupObjects(void*);
	static void onClickShowOtherObjects(void*);

	static void onClickReturnOwnerObjects(void*);
	static void onClickReturnGroupObjects(void*);
	static void onClickReturnOtherObjects(void*);
	static void onClickReturnOwnerList(void*);
	static void onClickRefresh(void*);

	static void onDoubleClickOwner(void*);

	static void onCommitList(LLUICtrl* ctrl, void* data);
	static void onLostFocus(LLFocusableElement* caller, void* user_data);
	static void onCommitClean(LLUICtrl* caller, void* user_data);

private:
	LLSafeHandle<LLParcelSelection>& mParcel;

	LLTextBox*		mParcelObjectBonus;
	LLTextBox*		mSWTotalObjects;
	LLTextBox*		mObjectContribution;
	LLTextBox*		mTotalObjects;
	LLTextBox*		mOwnerObjects;
	LLButton*		mBtnShowOwnerObjects;
	LLButton*		mBtnReturnOwnerObjects;
	LLTextBox*		mGroupObjects;
	LLButton*		mBtnShowGroupObjects;
	LLButton*		mBtnReturnGroupObjects;
	LLTextBox*		mOtherObjects;
	LLButton*		mBtnShowOtherObjects;
	LLButton*		mBtnReturnOtherObjects;
	LLTextBox*		mSelectedObjects;
	LLLineEditor*	mCleanOtherObjectsTime;
	S32				mOtherTime;
	LLButton*		mBtnRefresh;
	LLButton*		mBtnReturnOwnerList;
	LLNameListCtrl*	mOwnerList;

	LLUIImagePtr	mIconAvatarOnline;
	LLUIImagePtr	mIconAvatarOffline;
	LLUIImagePtr	mIconGroup;

	S32				mSelectedCount;

	owners_list_t	mSelectedOwners;
	std::string		mSelectedName;

	bool			mSelectedIsGroup;
	bool			mFirstReply;
};

class LLPanelLandOptions final : public LLPanel
{
public:
	LLPanelLandOptions(LLSafeHandle<LLParcelSelection>& parcelp);
	virtual ~LLPanelLandOptions();

	bool postBuild() override;
	void draw() override;
	void refresh() override;

private:
	// Refresh the "show in search" checkbox and category selector.
	void refreshSearch();

	static void onCommitAny(LLUICtrl* ctrl, void* userdata);
	static void onClickSet(void* userdata);
	static void onClickClear(void* userdata);
	static void onClickPublishHelp(void*);

private:
	LLCheckBoxCtrl*	mCreateObjectsCheck;
	LLCheckBoxCtrl*	mCreateGrpObjectsCheck;
	LLCheckBoxCtrl*	mAllObjectEntryCheck;
	LLCheckBoxCtrl*	mGroupObjectEntryCheck;
	LLCheckBoxCtrl*	mEditLandCheck;
	LLCheckBoxCtrl*	mNoDamageCheck;
	LLCheckBoxCtrl*	mCanFlyCheck;
	LLCheckBoxCtrl*	mGroupScriptsCheck;
	LLCheckBoxCtrl*	mAllScriptsCheck;
	LLCheckBoxCtrl*	mShowDirectoryCheck;
	LLCheckBoxCtrl*	mMatureCheck;
	LLCheckBoxCtrl*	mPushRestrictionCheck;
	LLCheckBoxCtrl*	mPrivacyCheck;

	LLComboBox*		mCategoryCombo;
	LLComboBox*		mTeleportRoutingCombo;

	LLTextureCtrl*	mSnapshotCtrl;

	LLTextBox*		mLocationText;

	LLButton*		mSetBtn;
	LLButton*		mClearBtn;
	LLButton*		mPublishHelpButton;

	LLSafeHandle<LLParcelSelection>& mParcel;
};

class LLPanelLandAccess final : public LLPanel
{
public:
	LLPanelLandAccess(LLSafeHandle<LLParcelSelection>& parcelp);
	~LLPanelLandAccess() override;

	bool postBuild() override;
	void draw() override;
	void refresh() override;

	void refreshUI();
	void refreshNames();

private:
	static void onCommitPublicAccess(LLUICtrl* ctrl, void* userdata);
	static void onCommitGroupCheck(LLUICtrl* ctrl, void* userdata);
	static void onCommitAny(LLUICtrl* ctrl, void* userdata);
	static void onClickAddAccess(void*);
	static void callbackAvatarCBAccess(const std::vector<std::string>& names,
									   const uuid_vec_t& ids, void* userdata);
	static void onClickRemoveAccess(void* userdata);
	static void onClickAddBanned(void* userdata);
	static void callbackAvatarCBBanned(const std::vector<std::string>& names,
									   const uuid_vec_t& ids, void* userdata);
	static void callbackAvatarCBBanned2(const uuid_vec_t& ids, S32 duration,
										void* userdata);
	static void onClickRemoveBanned(void*);

private:
	LLTextBox*			mOnlyAllowText;
	LLCheckBoxCtrl*		mCheckPublicAccess;
	LLCheckBoxCtrl*		mCheckLimitPayment;
	LLCheckBoxCtrl*		mCheckLimitAge;
	LLCheckBoxCtrl*		mCheckLimitGroup;
	LLCheckBoxCtrl*		mCheckLimitPass;
	LLComboBox*			mPassCombo;
	LLSpinCtrl*			mPriceSpin;
	LLSpinCtrl*			mHourSpin;
	LLNameListCtrl*		mListAccess;
	LLNameListCtrl*		mListBanned;
	LLButton*			mAddAllowedButton;
	LLButton*			mRemoveAllowedButton;
	LLButton*			mAddBannedButton;
	LLButton*			mRemoveBannedButton;

	LLSafeHandle<LLParcelSelection>& mParcel;
};

class LLPanelLandCovenant final : public LLPanel
{
public:
	LLPanelLandCovenant(LLSafeHandle<LLParcelSelection>& parcelp);
	~LLPanelLandCovenant() override;

	bool postBuild() override;
	void refresh() override;

	static void updateCovenantText(const std::string& string);
	static void updateLastModified(const std::string& text);
	static void updateEstateName(const std::string& name);
	static void updateEstateOwnerName(const std::string& name);

private:
	LLTextBox*			mRegionNameText;
	LLTextBox*			mRegionTypeText;
	LLTextBox*			mRegionMaturityText;
	LLTextBox*			mRegionResellClauseText;
	LLTextBox*			mRegionChangeClauseText;
	LLTextBox*			mEstateNameText;
	LLTextBox*			mEstateOwnerText;
	LLTextBox*			mCovenantDateText;
	LLViewerTextEditor*	mCovenantEditor;

	LLSafeHandle<LLParcelSelection>& mParcel;
};

class LLPanelLandExperiences final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelLandExperiences);

public:	
	LLPanelLandExperiences(LLSafeHandle<LLParcelSelection>& parcelp);
	~LLPanelLandExperiences() override;

	bool postBuild() override;
	void refresh() override;

	void experienceAdded(const LLUUID& id, U32 xp_type, U32 access_type);
	void experienceRemoved(const LLUUID& id, U32 access_type);

protected:
	static void* createAllowedExperiencesPanel(void* data);
	static void* createBlockedExperiencesPanel(void* data);

	void setupList(LLPanelExperienceListEditor* panel,
				   const std::string& control_name,
				   U32 xp_type, U32 access_type);

	void refreshPanel(LLPanelExperienceListEditor* panel, U32 xp_type);

protected:
	LLPanelExperienceListEditor*		mAllowed;
	LLPanelExperienceListEditor*		mBlocked;

	LLSafeHandle<LLParcelSelection>&	mParcel;
};

void send_parcel_select_objects(S32 parcel_local_id, U32 return_type,
								owners_list_t* return_ids = NULL);

#endif
