/**
 * @file llfloaterpathfindinglinksets.h
 * @brief Pathfinding linksets floater, allowing manipulation of the linksets
 * on the current region.
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLFLOATERPATHFINDINGLINKSETS_H
#define LL_LLFLOATERPATHFINDINGLINKSETS_H

#include "llfloaterpathfindingobjects.h"

class LLComboBox;
class LLLineEditor;
class LLUICtrl;
class LLVector3;

class LLFloaterPathfindingLinksets final
:	public LLFloaterPathfindingObjects,
	public LLFloaterSingleton<LLFloaterPathfindingLinksets>

{
	friend class LLUISingleton<LLFloaterPathfindingLinksets,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterPathfindingLinksets);

public:
	static void openLinksetsWithSelectedObjects();

protected:
	// Open only via openLinksetsWithSelectedObjects()
	LLFloaterPathfindingLinksets(const LLSD&);

	bool postBuild() override;

	void requestGetObjects() override;

	void addObjectsIntoScrollList(const LLPathfindingObjectList::ptr_t) override;

	LL_INLINE void resetLoadingNameObjectsList() override
 	{
		mLoadingNameObjects.clear();
	}

	void updateControlsOnScrollListChange() override;

	LL_INLINE S32 getNameColumnIndex() const override
	{
		return 0;
	}

	LL_INLINE S32 getOwnerNameColumnIndex() const override
	{
		return 2;
	}

	std::string getOwnerName(const LLPathfindingObject* obj) const override;

	LL_INLINE const LLColor4& getBeaconColor() const override
	{
		return mBeaconColor;
	}

	LLPathfindingObjectList::ptr_t getEmptyObjectList() const override;

	static void newObjectList(LLPathfindingManager::request_id_t request_id,
							  LLPathfindingManager::ERequestStatus req_status,
							  LLPathfindingObjectList::ptr_t pobjects);
private:
	void requestSetLinksets(LLPathfindingObjectList::ptr_t linkset_list,
							LLPathfindingLinkset::ELinksetUse use,
							S32 a, S32 b, S32 c, S32 d);

	static void onClearFiltersClicked(void* data);
	static void onApplyChangesClicked(void* data);
	static void onApplyAllFiltersClicked(void* data);
	static void onApplyAllFilters(LLUICtrl* ctrl, void* data);
	static void onWalkabilityCoefficientEntered(LLUICtrl* ctrl, void* data);

	static void handleObjectNameResponse(const LLPathfindingObject* pobj);

	void clearFilters();

	void updateEditFieldValues();
	LLSD buildLinksetScrollListItemData(const LLPathfindingLinkset* plinkset,
										const LLVector3& av_pos);

	void rebuildScrollListAfterAvatarNameLoads(const LLPathfindingObject::ptr_t pobj);

	bool showUnmodifiablePhantomWarning(LLPathfindingLinkset::ELinksetUse use) const;
	bool showPhantomToggleWarning(LLPathfindingLinkset::ELinksetUse use) const;
	bool showCannotBeVolumeWarning(LLPathfindingLinkset::ELinksetUse use) const;

	void updateStateOnEditFields();
	void updateStateOnEditLinksetUse();

	void applyEdit();
	void handleApplyEdit(const LLSD& notification, const LLSD& response);
	void doApplyEdit();

	std::string getLinksetUseString(LLPathfindingLinkset::ELinksetUse use) const;

	LLPathfindingLinkset::ELinksetUse getFilterLinksetUse() const;
	void setFilterLinksetUse(LLPathfindingLinkset::ELinksetUse use);

	LLPathfindingLinkset::ELinksetUse getEditLinksetUse() const;
	void setEditLinksetUse(LLPathfindingLinkset::ELinksetUse use);

	LLPathfindingLinkset::ELinksetUse convertToLinksetUse(LLSD value) const;
	LLSD convertToXuiValue(LLPathfindingLinkset::ELinksetUse use) const;

private:
	LLLineEditor*		mFilterByName;
	LLLineEditor*		mFilterByDescription;
	LLComboBox*			mFilterByLinksetUse;
	LLComboBox*			mEditLinksetUse;
	LLScrollListItem*	mUseUnset;
	LLScrollListItem*	mUseWalkable;
	LLScrollListItem*	mUseStaticObstacle;
	LLScrollListItem*	mUseDynamicObstacle;
	LLScrollListItem*	mUseMaterialVolume;
	LLScrollListItem*	mUseExclusionVolume;
	LLScrollListItem*	mUseDynamicPhantom;
	LLTextBox*			mLabelCoefficients;
	LLTextBox*			mLabelEditA;
	LLTextBox*			mLabelEditB;
	LLTextBox*			mLabelEditC;
	LLTextBox*			mLabelEditD;
	LLLineEditor*		mEditA;
	LLLineEditor*		mEditB;
	LLLineEditor*		mEditC;
	LLLineEditor*		mEditD;
	LLTextBox*			mLabelSuggestedUseA;
	LLTextBox*			mLabelSuggestedUseB;
	LLTextBox*			mLabelSuggestedUseC;
	LLTextBox*			mLabelSuggestedUseD;
	LLButton*			mApplyEditsButton;

	LLColor4			mBeaconColor;

	uuid_list_t			mLoadingNameObjects;

	S32					mPreviousValueA;
	S32					mPreviousValueB;
	S32					mPreviousValueC;
	S32					mPreviousValueD;

	S32					mScriptedColumnWidth;

	bool				mHasKnownScripedStatus;
};

#endif // LL_LLFLOATERPATHFINDINGLINKSETS_H
