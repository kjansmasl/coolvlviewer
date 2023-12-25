/**
 * @file llfloaterexperienceprofile.h
 * @brief llfloaterexperienceprofile and related class definitions
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
 *
 * Copyright (c) 2013, Linden Research, Inc.
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

#ifndef LL_LLFLOATEREXPERIENCEPROFILE_H
#define LL_LLFLOATEREXPERIENCEPROFILE_H

#include "llfloater.h"
#include "llsd.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLTextBox;
class LLTextEditor;
class LLTextureCtrl;

class LLFloaterExperienceProfile final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterExperienceProfile);

public:
	enum PostSaveAction
	{
		NOTHING,
		CLOSE,
		VIEW,
	};

	LLFloaterExperienceProfile(const LLUUID& exp_id);
	~LLFloaterExperienceProfile() override;

	bool postBuild() override;

	bool canClose() override;
	void onClose(bool app_quitting) override;

	LL_INLINE LLUUID getExperienceId() const		{ return mExperienceId; }
	void setPreferences(const LLSD& content);

	void refreshExperience(const LLSD& experience);
	void onSaveComplete(const LLSD& content);

	static LLFloaterExperienceProfile* show(const LLUUID& exp_id);
	static LLUUID getInstanceId(LLFloaterExperienceProfile* instance);

protected:
	static void onClickEdit(void* data);
	static void onClickAllow(void* data);
	static void onClickBlock(void* data);
	static void onClickForget(void* data);
	static void onClickCancel(void* data);
	static void onClickSave(void* data);
	static void onClickLocation(void* data);
	static void onClickClear(void* data);
	static void onPickGroup(void* data);
	static void onClickExperienceTitle(void* data);
	static void onOwnerProfile(void* data);
	static void onShowGroupInfo(void* data);
	static void onShowLocation(void* data);
	static void onOpenMarketplaceURL(void* data);
	static void onLineKeystroke(LLLineEditor*, void* data);
	static void onTextKeystroke(LLTextEditor*, void* data);
	static void onFieldChanged(LLUICtrl*, void* data);
	static void onReportExperience(void* data);

	static void nameCallback(const LLUUID& group_id, const std::string& name,
							 bool is_group,LLFloaterExperienceProfile* self);

	static void setOwnerId(LLUUID agent_id, void* data);
	static void setEditGroup(LLUUID group_id, void* data);

	void setPermission(const char* perm);

	void changeToView();

	void experienceForgotten();
	void experienceBlocked();
	void experienceAllowed();

	static void experienceCallback(LLHandle<LLFloaterExperienceProfile> handle,
								   const LLSD& experience);
	static bool experiencePermission(LLHandle<LLFloaterExperienceProfile> handle,
									 const LLSD& permission);

	bool setMaturityString(S32 maturity);
	bool handleSaveChangesDialog(const LLSD& notification,
								 const LLSD& response, PostSaveAction action);
	void doSave(int success_action);

	void updatePackage();

	void updatePermission(const LLSD& permission);

private:
	static bool hasPermission(const LLSD& content, const std::string& name,
							  const LLUUID& test);
	static void experiencePermissionResults(LLUUID exp_id, LLSD result);
	static void experienceIsAdmin(LLHandle<LLFloaterExperienceProfile> handle,
								  const LLSD& result);
	static void experienceUpdateResult(LLHandle<LLFloaterExperienceProfile> handle,
									   const LLSD& result);

protected:
	LLButton*				mAllowBtn;
	LLButton*				mForgetBtn;
	LLButton*				mBlockBtn;
	LLButton*				mEditBtn;
	LLButton*				mSaveBtn;
	LLButton*				mGroupBtn;
	LLCheckBoxCtrl*			mEnableCheck;
	LLCheckBoxCtrl*			mPrivateCheck;
	LLComboBox*				mRatingCombo;
	LLLineEditor*			mMarketplaceEditor;
	LLLineEditor*			mExperienceTitleEditor;
	LLTextBox*				mRatingText;
	LLTextBox*				mExperienceTitleText;
	LLTextBox*				mLocationText;
	LLTextBox*				mEditLocationText;
	LLTextBox*				mGroupText;
	LLTextBox*				mEditGroupText;
	LLTextBox*				mOwnerText;
	LLTextBox*				mMarketplaceText;
	LLTextEditor*			mExperienceDescEditor;
	LLTextureCtrl*			mLogoTexture;
	LLTextureCtrl*			mEditLogoTexture;

	S32						mSaveCompleteAction;
	LLUUID					mExperienceId;
	LLUUID					mOwnerId;
	LLUUID					mGroupId;
	bool					mDirty;
	bool					mForceClose;
	LLSD					mExperienceDetails;
	LLSD					mPackage;
	std::string				mLocationSLURL;
	std::string				mExperienceSLURL;
	std::string				mMarketplaceURL;

	typedef fast_hmap<LLUUID, LLFloaterExperienceProfile*> instances_map_t;
	static instances_map_t	sInstances;
};

#endif // LL_LLFLOATEREXPERIENCEPROFILE_H
