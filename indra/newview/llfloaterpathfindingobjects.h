/**
 * @file llfloaterpathfindingobjects.h
 * @brief Base class for both the pathfinding linksets and characters floater.
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

#ifndef LL_LLFLOATERPATHFINDINGOBJECTS_H
#define LL_LLFLOATERPATHFINDINGOBJECTS_H

#include "boost/signals2.hpp"

#include "llavatarnamecache.h"
#include "llfloater.h"
#include "lluuid.h"
#include "llcolor4.h"

#include "llagent.h"
#include "llpathfindingmanager.h"
#include "llpathfindingobjectlist.h"
#include "llselectmgr.h"

#define PF_DEFAULT_BEACON_WIDTH 6

class LLAvatarName;
class LLButton;
class LLCheckBoxCtrl;
class LLScrollListCtrl;
class LLScrollListItem;
class LLSD;
class LLTextBox;

class LLFloaterPathfindingObjects : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterPathfindingObjects);

public:
	void onOpen() override;
	void onClose(bool app_quitting) override;
	void draw() override;

protected:
	typedef enum
	{
		kMessagingUnknown,
		kMessagingGetRequestSent,
		kMessagingGetError,
		kMessagingSetRequestSent,
		kMessagingSetError,
		kMessagingComplete,
		kMessagingNotEnabled
	} EMessagingState;

	LLFloaterPathfindingObjects();
	~LLFloaterPathfindingObjects() override;

	bool postBuild() override;

	virtual void requestGetObjects() = 0;
	LLPathfindingManager::request_id_t getNewRequestId();
	void handleNewObjectList(LLPathfindingManager::request_id_t request_id,
							 LLPathfindingManager::ERequestStatus req_status,
							 LLPathfindingObjectList::ptr_t pobjects);
	void handleUpdateObjectList(LLPathfindingManager::request_id_t request_id,
								LLPathfindingManager::ERequestStatus req_status,
								LLPathfindingObjectList::ptr_t pobjects);

	void rebuildObjectsScrollList(bool update_if_needed = false);

	virtual void addObjectsIntoScrollList(const LLPathfindingObjectList::ptr_t) = 0;

	virtual void resetLoadingNameObjectsList() = 0;

	virtual void updateControlsOnScrollListChange();
	virtual void updateControlsOnInWorldSelectionChange();

	virtual S32 getNameColumnIndex() const = 0;
	virtual S32 getOwnerNameColumnIndex() const = 0;
	virtual std::string getOwnerName(const LLPathfindingObject* obj) const = 0;

	LL_INLINE virtual const LLColor4& getBeaconColor() const
	{
		return mDefaultBeaconColor;
	}

	LL_INLINE virtual const LLColor4& getBeaconTextColor() const
	{
		return mDefaultBeaconTextColor;
	}

	LL_INLINE virtual S32 getBeaconWidth() const
	{
		return PF_DEFAULT_BEACON_WIDTH;
	}

	void showFloaterWithSelectionObjects();

	bool showBeacons() const;
	void clearAllObjects();
	void selectAllObjects();
	void selectNoneObjects();
	void teleportToSelectedObject();

	virtual LLPathfindingObjectList::ptr_t getEmptyObjectList() const = 0;
	S32 getNumSelectedObjects() const;
	LLPathfindingObjectList::ptr_t getSelectedObjects() const;
	LLPathfindingObject::ptr_t getFirstSelectedObject() const;

	LL_INLINE EMessagingState getMessagingState() const
	{
		return mMessagingState;
	}

private:
	void setMessagingState(EMessagingState state);

	static void onRefreshObjectsClicked(void* data);
	static void onSelectAllObjectsClicked(void* data);
	static void onSelectNoneObjectsClicked(void* data);
	static void onTakeClicked(void* data);
	static void onTakeCopyClicked(void* data);
	static void onReturnClicked(void* data);
	static void onDeleteClicked(void* data);
	static void onTeleportClicked(void* data);
	static void onScrollListSelectionChanged(LLUICtrl* ctrl, void* data);

	void onInWorldSelectionListChanged();
	void onRegionBoundaryCrossed();
	void onGodLevelChange(U8 level);

	void updateMessagingStatus();
	void updateStateOnListControls();
	void updateStateOnActionControls();
	void selectScrollListItemsInWorld();

	void handleReturnItemsResponse(const LLSD& notif, const LLSD& response);
	void handleDeleteItemsResponse(const LLSD& notif, const LLSD& response);

	LLPathfindingObject::ptr_t findObject(const LLScrollListItem* item) const;

protected:
	LLScrollListCtrl*	mObjectsScrollList;

private:
	LLTextBox*			mMessagingStatus;
	LLButton*			mRefreshListButton;
	LLButton*			mSelectAllButton;
	LLButton*			mSelectNoneButton;
	LLCheckBoxCtrl*		mShowBeaconCheckBox;

	LLButton*			mTakeButton;
	LLButton*			mTakeCopyButton;
	LLButton*			mReturnButton;
	LLButton*			mDeleteButton;
	LLButton*			mTeleportButton;

	LLColor4			mGoodTextColor;
	LLColor4			mDefaultBeaconColor;
	LLColor4			mDefaultBeaconTextColor;
	LLColor4			mErrorTextColor;
	LLColor4			mWarningTextColor;

	EMessagingState						mMessagingState;
	LLPathfindingManager::request_id_t	mMessagingRequestId;

	LLPathfindingObjectList::ptr_t		mObjectList;

	LLObjectSelectionHandle				mObjectsSelection;

	bool								mHasObjectsToBeSelected;
	uuid_vec_t							mObjectsToBeSelected;

	boost::signals2::connection			mSelectionUpdateSlot;
	boost::signals2::connection			mRegionBoundaryCrossingSlot;
	LLAgent::god_level_change_slot_t	mGodLevelChangeSlot;
};

#endif // LL_LLFLOATERPATHFINDINGOBJECTS_H
