/**
 * @file llfloaterpathfindingobjects.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterpathfindingobjects.h"

#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llfloater.h"
#include "lllocale.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "llstyle.h"
#include "lltextbox.h"

#include "llagent.h"
#include "llpathfindingmanager.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llviewermenu.h"			// For enable_object_*(), etc
#include "llviewerobjectlist.h"
#include "llviewerregion.h"

//---------------------------------------------------------------------------
// LLFloaterPathfindingObjects
//---------------------------------------------------------------------------

LLFloaterPathfindingObjects::LLFloaterPathfindingObjects()
:	mObjectsScrollList(NULL),
	mMessagingStatus(NULL),
	mRefreshListButton(NULL),
	mSelectAllButton(NULL),
	mSelectNoneButton(NULL),
	mShowBeaconCheckBox(NULL),
	mTakeButton(NULL),
	mTakeCopyButton(NULL),
	mReturnButton(NULL),
	mDeleteButton(NULL),
	mTeleportButton(NULL),
	mMessagingState(kMessagingUnknown),
	mMessagingRequestId(0U),
	mHasObjectsToBeSelected(false)
{
}

LLFloaterPathfindingObjects::~LLFloaterPathfindingObjects()
{
	clearAllObjects();
}

bool LLFloaterPathfindingObjects::postBuild()
{
	mDefaultBeaconColor =
		LLUI::sColorsGroup->getColor("PathfindingDefaultBeaconColor");

	mDefaultBeaconTextColor =
		LLUI::sColorsGroup->getColor("PathfindingDefaultBeaconTextColor");

	mGoodTextColor = LLUI::sColorsGroup->getColor("PathfindingGoodColor");

	mWarningTextColor =
		LLUI::sColorsGroup->getColor("PathfindingWarningColor");

	mErrorTextColor = LLUI::sColorsGroup->getColor("PathfindingErrorColor");

	mObjectsScrollList = getChild<LLScrollListCtrl>("objects_scroll_list");
	mObjectsScrollList->setCommitCallback(onScrollListSelectionChanged);
	mObjectsScrollList->setCallbackUserData(this);
	mObjectsScrollList->setCommitOnSelectionChange(true);
	mObjectsScrollList->sortByColumnIndex(getNameColumnIndex(), true);

	mMessagingStatus = getChild<LLTextBox>("messaging_status");

	mRefreshListButton = getChild<LLButton>("refresh_objects_list");
	mRefreshListButton->setClickedCallback(onRefreshObjectsClicked, this);

	mSelectAllButton = getChild<LLButton>("select_all_objects");
	mSelectAllButton->setClickedCallback(onSelectAllObjectsClicked, this);

	mSelectNoneButton = getChild<LLButton>("select_none_objects");
	mSelectNoneButton->setClickedCallback(onSelectNoneObjectsClicked, this);

	mShowBeaconCheckBox = getChild<LLCheckBoxCtrl>("show_beacon");

	mTakeButton = getChild<LLButton>("take_objects");
	mTakeButton->setClickedCallback(onTakeClicked, this);

	mTakeCopyButton = getChild<LLButton>("take_copy_objects");
	mTakeCopyButton->setClickedCallback(onTakeCopyClicked, this);

	mReturnButton = getChild<LLButton>("return_objects");
	mReturnButton->setClickedCallback(onReturnClicked, this);

	mDeleteButton = getChild<LLButton>("delete_objects");
	mDeleteButton->setClickedCallback(onDeleteClicked, this);

	mTeleportButton = getChild<LLButton>("teleport_me_to_object");
	mTeleportButton->setClickedCallback(onTeleportClicked, this);

	return true;
}

void LLFloaterPathfindingObjects::onOpen()
{
	LLFloater::onOpen();

	selectNoneObjects();
	mObjectsScrollList->setCommitOnSelectionChange(true);

	if (!mSelectionUpdateSlot.connected())
	{
		mSelectionUpdateSlot =
			gSelectMgr.mUpdateSignal.connect(boost::bind(&LLFloaterPathfindingObjects::onInWorldSelectionListChanged,
														 this));
	}

	if (!mRegionBoundaryCrossingSlot.connected())
	{
		mRegionBoundaryCrossingSlot =
			gAgent.addRegionChangedCB(boost::bind(&LLFloaterPathfindingObjects::onRegionBoundaryCrossed,
												  this));
	}

	if (!mGodLevelChangeSlot.connected())
	{
		mGodLevelChangeSlot =
			gAgent.registerGodLevelChanageListener(boost::bind(&LLFloaterPathfindingObjects::onGodLevelChange, this, _1));
	}

	requestGetObjects();
}

void LLFloaterPathfindingObjects::onClose(bool app_quitting)
{
	if (mGodLevelChangeSlot.connected())
	{
		mGodLevelChangeSlot.disconnect();
	}

	if (mRegionBoundaryCrossingSlot.connected())
	{
		mRegionBoundaryCrossingSlot.disconnect();
	}

	if (mSelectionUpdateSlot.connected())
	{
		mSelectionUpdateSlot.disconnect();
	}

	mObjectsScrollList->setCommitOnSelectionChange(false);
	selectNoneObjects();

	if (mObjectsSelection.notNull())
	{
		mObjectsSelection.clear();
	}

	if (app_quitting)
	{
		clearAllObjects();
	}

	LLFloater::onClose(app_quitting);
}

void LLFloaterPathfindingObjects::draw()
{
//MK
	// Fast enough that it can be kept here
	if (gRLenabled && gRLInterface.mContainsEdit)
	{
		close();
		return;
	}
//mk

	LLFloater::draw();

	if (showBeacons())
	{
		std::vector<LLScrollListItem*> items =
			mObjectsScrollList->getAllSelected();
		if (items.empty())
		{
			return;
		}

		S32 name_col_idx = getNameColumnIndex();
		const LLColor4& beacon_color = getBeaconColor();
		const LLColor4& text_color = getBeaconTextColor();
		S32 beacon_width = getBeaconWidth();

		std::vector<LLViewerObject*> objects;
		objects.reserve(items.size());

		for (std::vector<LLScrollListItem*>::const_iterator it = items.begin(),
															end = items.end();
			 it != end; ++it)
		{
			const LLScrollListItem* item = *it;

			LLViewerObject* vobj = gObjectList.findObject(item->getUUID());
			if (!vobj) continue;

			const std::string& name =
				item->getColumn(name_col_idx)->getValue().asString();
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), name,
									   beacon_color, text_color, beacon_width);
		}
	}
}

LLPathfindingManager::request_id_t LLFloaterPathfindingObjects::getNewRequestId()
{
	return ++mMessagingRequestId;
}

void LLFloaterPathfindingObjects::handleNewObjectList(LLPathfindingManager::request_id_t req_id,
													  LLPathfindingManager::ERequestStatus req_status,
													  LLPathfindingObjectList::ptr_t pobjects)
{
	if (req_id == mMessagingRequestId)
	{
		switch (req_status)
		{
			case LLPathfindingManager::kRequestStarted:
				setMessagingState(kMessagingGetRequestSent);
				break;

			case LLPathfindingManager::kRequestCompleted:
				mObjectList = pobjects;
				rebuildObjectsScrollList();
				setMessagingState(kMessagingComplete);
				break;

			case LLPathfindingManager::kRequestNotEnabled:
				clearAllObjects();
				setMessagingState(kMessagingNotEnabled);
				break;

			case LLPathfindingManager::kRequestError:
				clearAllObjects();
				setMessagingState(kMessagingGetError);
				break;

			default:
				clearAllObjects();
				setMessagingState(kMessagingGetError);
				llwarns << "Unknown status !" << llendl;
		}
	}
	else if (req_id > mMessagingRequestId)
	{
		llwarns << "Invalid request id !" << llendl;
	}
}

void LLFloaterPathfindingObjects::handleUpdateObjectList(LLPathfindingManager::request_id_t req_id,
														 LLPathfindingManager::ERequestStatus req_status,
														 LLPathfindingObjectList::ptr_t pobjects)
{
	// We currently assume that handleUpdateObjectList is called only when
	// objects are being SET
	if (req_id == mMessagingRequestId)
	{
		switch (req_status)
		{
			case LLPathfindingManager::kRequestStarted:
				setMessagingState(kMessagingSetRequestSent);
				break;

			case LLPathfindingManager::kRequestCompleted:
				if (mObjectList == NULL)
				{
					mObjectList = pobjects;
				}
				else
				{
					mObjectList->update(pobjects);
				}
				rebuildObjectsScrollList();
				setMessagingState(kMessagingComplete);
				break;

			case LLPathfindingManager::kRequestNotEnabled:
				clearAllObjects();
				setMessagingState(kMessagingNotEnabled);
				break;

			case LLPathfindingManager::kRequestError:
				clearAllObjects();
				setMessagingState(kMessagingSetError);
				break;

			default:
				clearAllObjects();
				setMessagingState(kMessagingSetError);
				llwarns << "Unknown status !" << llendl;
		}
	}
	else if (req_id > mMessagingRequestId)
	{
		llwarns << "Invalid request id !" << llendl;
	}
}

void LLFloaterPathfindingObjects::rebuildObjectsScrollList(bool update_if_needed)
{
	if (!mHasObjectsToBeSelected)
	{
		std::vector<LLScrollListItem*> items =
			mObjectsScrollList->getAllSelected();
		if (items.size() > 0)
		{
			mObjectsToBeSelected.reserve(items.size());
			for (std::vector<LLScrollListItem*>::const_iterator
					iter = items.begin(), end = items.end();
				 iter != end; ++iter)
			{
				const LLScrollListItem* item = *iter;
				mObjectsToBeSelected.emplace_back(item->getUUID());
			}
		}
	}

	S32 orig_scroll_pos = mObjectsScrollList->getScrollPos();
	mObjectsScrollList->deleteAllItems();

	if (mObjectList && !mObjectList->isEmpty())
	{
		addObjectsIntoScrollList(mObjectList);

		if (mObjectsScrollList->selectMultiple(mObjectsToBeSelected) == 0)
		{
			if (update_if_needed && mRefreshListButton->getEnabled())
			{
				requestGetObjects();
				return;
			}
		}
		if (mHasObjectsToBeSelected)
		{
			mObjectsScrollList->scrollToShowSelected();
		}
		else
		{
			mObjectsScrollList->setScrollPos(orig_scroll_pos);
		}
	}

	mObjectsToBeSelected.clear();
	mHasObjectsToBeSelected = false;

	updateControlsOnScrollListChange();
}

void LLFloaterPathfindingObjects::updateControlsOnScrollListChange()
{
	updateMessagingStatus();
	updateStateOnListControls();
	selectScrollListItemsInWorld();
	updateStateOnActionControls();
}

void LLFloaterPathfindingObjects::updateControlsOnInWorldSelectionChange()
{
	updateStateOnActionControls();
}

void LLFloaterPathfindingObjects::showFloaterWithSelectionObjects()
{
	mObjectsToBeSelected.clear();

	LLObjectSelectionHandle sel_handle = gSelectMgr.getSelection();
	if (sel_handle.notNull())
	{
		LLObjectSelection* objects = sel_handle.get();
		if (!objects->isEmpty())
		{
			for (LLObjectSelection::valid_iterator
					iter = objects->valid_begin();
				iter != objects->valid_end(); ++iter)
			{
				LLSelectNode* object = *iter;
				LLViewerObject* vobj = object->getObject();
				mObjectsToBeSelected.emplace_back(vobj->getID());
			}
		}
	}
	mHasObjectsToBeSelected = true;

	if (!getVisible())
	{
		open();
		setVisibleAndFrontmost();
	}
	else
	{
		rebuildObjectsScrollList(true);
		if (isMinimized())
		{
			setMinimized(false);
		}
		setVisibleAndFrontmost();
	}
	setFocus(true);
}

bool LLFloaterPathfindingObjects::showBeacons() const
{
	return mShowBeaconCheckBox->get();
}

void LLFloaterPathfindingObjects::clearAllObjects()
{
	selectNoneObjects();
	mObjectsScrollList->deleteAllItems();
	mObjectList.reset();
}

void LLFloaterPathfindingObjects::selectAllObjects()
{
	mObjectsScrollList->selectAll();
}

void LLFloaterPathfindingObjects::selectNoneObjects()
{
	mObjectsScrollList->deselectAllItems();
}

void LLFloaterPathfindingObjects::teleportToSelectedObject()
{
	std::vector<LLScrollListItem*> items =
		mObjectsScrollList->getAllSelected();
	if (items.size() != 1)
	{
		llwarns << "Can only TP to one object !" << llendl;
		return;
	}

	std::vector<LLScrollListItem*>::const_reference item_ref = items.front();
	const LLScrollListItem* item = item_ref;
	LLVector3d tp_loc;
	LLViewerObject* vobj = gObjectList.findObject(item->getUUID());
	if (!vobj)
	{
		// If we cannot find the object in the viewer list, teleport to the
		// last reported position
		if (mObjectList)
		{
			const LLPathfindingObject::ptr_t objectp =
				mObjectList->find(item->getUUID());
			if (!objectp)
			{
				llwarns << "Cannot find the object, aborting !" << llendl;
				return;
			}
			tp_loc = gAgent.getPosGlobalFromAgent(objectp->getLocation());
		}
		else
		{
			llwarns << "NULL mObjectList, aborting !" << llendl;
			return;
		}
	}
	else
	{
		// If we can find the object in the viewer list, teleport to the
		// known current position
		tp_loc = vobj->getPositionGlobal();
	}
	gAgent.teleportViaLocationLookAt(tp_loc);
}

S32 LLFloaterPathfindingObjects::getNumSelectedObjects() const
{
	return mObjectsScrollList->getNumSelected();
}

LLPathfindingObjectList::ptr_t LLFloaterPathfindingObjects::getSelectedObjects() const
{
	LLPathfindingObjectList::ptr_t objects = getEmptyObjectList();

	std::vector<LLScrollListItem*> items =
		mObjectsScrollList->getAllSelected();
	if (!items.empty())
	{
		for (std::vector<LLScrollListItem*>::const_iterator it = items.begin(),
															end = items.end();
			 it != end; ++it)
		{
			LLPathfindingObject::ptr_t objectp = findObject(*it);
			if (objectp)
			{
				objects->update(objectp);
			}
		}
	}

	return objects;
}

LLPathfindingObject::ptr_t LLFloaterPathfindingObjects::getFirstSelectedObject() const
{
	LLPathfindingObject::ptr_t objectp;

	std::vector<LLScrollListItem*> items =
		mObjectsScrollList->getAllSelected();
	if (!items.empty())
	{
		objectp = findObject(items.front());
	}

	return objectp;
}

void LLFloaterPathfindingObjects::setMessagingState(EMessagingState state)
{
	mMessagingState = state;
	updateControlsOnScrollListChange();
}

//static
void LLFloaterPathfindingObjects::onRefreshObjectsClicked(void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (self)
	{
		self->resetLoadingNameObjectsList();
		self->requestGetObjects();
	}
}

//static
void LLFloaterPathfindingObjects::onSelectAllObjectsClicked(void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (self)
	{
		self->selectAllObjects();
	}
}

//static
void LLFloaterPathfindingObjects::onSelectNoneObjectsClicked(void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (self)
	{
		self->selectNoneObjects();
	}
}

//static
void LLFloaterPathfindingObjects::onTakeClicked(void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (self)
	{
		handle_take();
		self->requestGetObjects();
	}
}

//static
void LLFloaterPathfindingObjects::onTakeCopyClicked(void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (self)
	{
		handle_take_copy();
	}
}

//static
void LLFloaterPathfindingObjects::onReturnClicked(void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (!self) return;

	LLNotification::Params params("PathfindingReturnMultipleItems");
	params.functor(boost::bind(&LLFloaterPathfindingObjects::handleReturnItemsResponse,
							   self, _1, _2));

	LLSD substitutions;
	S32 count = self->getNumSelectedObjects();
	substitutions["NUM_ITEMS"] = LLSD::Integer(count);
	params.substitutions = substitutions;
	if (count == 1)
	{
		gNotifications.forceResponse(params, 0);
	}
	else if (count > 1)
	{
		gNotifications.add(params);
	}
}

//static
void LLFloaterPathfindingObjects::onDeleteClicked(void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (!self) return;

	LLNotification::Params params("PathfindingDeleteMultipleItems");
	params.functor(boost::bind(&LLFloaterPathfindingObjects::handleDeleteItemsResponse,
							   self, _1, _2));

	LLSD substitutions;
	S32 count = self->getNumSelectedObjects();
	substitutions["NUM_ITEMS"] = LLSD::Integer(count);
	params.substitutions = substitutions;
	if (count == 1)
	{
		gNotifications.forceResponse(params, 0);
	}
	else if (count > 1)
	{
		gNotifications.add(params);
	}
}

//static
void LLFloaterPathfindingObjects::onTeleportClicked(void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (self)
	{
		self->teleportToSelectedObject();
	}
}

//static
void LLFloaterPathfindingObjects::onScrollListSelectionChanged(LLUICtrl* ctrl,
															   void* data)
{
	LLFloaterPathfindingObjects* self = (LLFloaterPathfindingObjects*)data;
	if (self)
	{
		self->updateControlsOnScrollListChange();
	}
}

void LLFloaterPathfindingObjects::onInWorldSelectionListChanged()
{
	updateControlsOnInWorldSelectionChange();
}

void LLFloaterPathfindingObjects::onRegionBoundaryCrossed()
{
	requestGetObjects();
}

void LLFloaterPathfindingObjects::onGodLevelChange(U8 level)
{
	requestGetObjects();
}

void LLFloaterPathfindingObjects::updateMessagingStatus()
{
	std::string text;
	LLColor4 color = mGoodTextColor;

	switch (getMessagingState())
	{
		case kMessagingGetRequestSent:
		{
			static std::string get_in_progress =
				getString("messaging_get_inprogress");
			text = get_in_progress;
			color = mWarningTextColor;
			break;
		}

		case kMessagingGetError:
		{
			static std::string get_error = getString("messaging_get_error");
			text = get_error;
			color = mErrorTextColor;
			break;
		}

		case kMessagingSetRequestSent:
		{
			static std::string set_in_progress =
				getString("messaging_set_inprogress");
			text = set_in_progress;
			color = mWarningTextColor;
			break;
		}

		case kMessagingSetError:
		{
			static std::string set_error = getString("messaging_set_error");
			text = set_error;
			color = mErrorTextColor;
			break;
		}

		case kMessagingComplete:
			if (mObjectsScrollList->isEmpty())
			{
				static std::string not_found =
					getString("messaging_complete_none_found");
				text = not_found;
			}
			else
			{
				LLStringUtil::format_map_t args;
				LLLocale locale(LLStringUtil::getLocale());

				S32 count = mObjectsScrollList->getItemCount();
				std::string literal;
				LLLocale::getIntegerString(literal, count);
				args["[NUM_TOTAL]"] = literal;

				literal.clear();
				count = mObjectsScrollList->getNumSelected();
				LLLocale::getIntegerString(literal, count);
				args["[NUM_SELECTED]"] = literal;

				text = getString("messaging_complete_available", args);
			}
			break;

		case kMessagingNotEnabled:
		{
			static std::string disabled = getString("messaging_not_enabled");
			text = disabled;
			color = mErrorTextColor;
			break;
		}

		default:
			llwarns << "Unknown state !" << llendl;
			// Fall-through
		case kMessagingUnknown:
		{
			static std::string initial = getString("messaging_initial");
			text = initial;
			color = mErrorTextColor;
		}
	}

	mMessagingStatus->setText(text);
	mMessagingStatus->setColor(color);
}

void LLFloaterPathfindingObjects::updateStateOnListControls()
{
	switch (getMessagingState())
	{
		case kMessagingUnknown:
		case kMessagingGetRequestSent:
		case kMessagingSetRequestSent:
			mRefreshListButton->setEnabled(false);
			mSelectAllButton->setEnabled(false);
			mSelectNoneButton->setEnabled(false);
			break;

		case kMessagingGetError:
		case kMessagingSetError:
		case kMessagingNotEnabled:
			mRefreshListButton->setEnabled(true);
			mSelectAllButton->setEnabled(false);
			mSelectNoneButton->setEnabled(false);
			break;

		case kMessagingComplete:
		{
			S32 numItems = mObjectsScrollList->getItemCount();
			S32 numSelectedItems = mObjectsScrollList->getNumSelected();
			mRefreshListButton->setEnabled(true);
			mSelectAllButton->setEnabled(numSelectedItems < numItems);
			mSelectNoneButton->setEnabled(numSelectedItems > 0);
			break;
		}

		default:
			llwarns << "Unknown state !" << llendl;
	}
}

void LLFloaterPathfindingObjects::updateStateOnActionControls()
{
	S32 count = mObjectsScrollList->getNumSelected();
	bool enabled = count > 0;

	mShowBeaconCheckBox->setEnabled(enabled);
	mTakeButton->setEnabled(enabled && visible_take_object());
	mTakeCopyButton->setEnabled(enabled && enable_object_take_copy());
	mReturnButton->setEnabled(enabled && enable_object_return());
	mDeleteButton->setEnabled(enabled && enable_object_delete());
	mTeleportButton->setEnabled(count == 1);
}

void LLFloaterPathfindingObjects::selectScrollListItemsInWorld()
{
	mObjectsSelection.clear();
	gSelectMgr.deselectAll();

	std::vector<LLScrollListItem*> items =
		mObjectsScrollList->getAllSelected();
	if (items.empty())
	{
		return;
	}

	std::vector<LLViewerObject*> objects;
	objects.reserve(items.size());

	for (std::vector<LLScrollListItem*>::const_iterator iter = items.begin(),
														end = items.end();
		 iter != end; ++iter)
	{
		const LLScrollListItem* item = *iter;

		LLViewerObject* vobj = gObjectList.findObject(item->getUUID());
		if (vobj)
		{
			objects.push_back(vobj);
		}
	}

	if (!objects.empty())
	{
		mObjectsSelection = gSelectMgr.selectObjectAndFamily(objects);
	}
}

void LLFloaterPathfindingObjects::handleReturnItemsResponse(const LLSD& notif,
															const LLSD& response)
{
	if (LLNotification::getSelectedOption(notif, response) == 0)
	{
		handle_object_return();
		requestGetObjects();
	}
}

void LLFloaterPathfindingObjects::handleDeleteItemsResponse(const LLSD& notif,
															const LLSD& response)
{
	if (LLNotification::getSelectedOption(notif, response) == 0)
	{
		handle_object_delete();
		requestGetObjects();
	}
}

LLPathfindingObject::ptr_t LLFloaterPathfindingObjects::findObject(const LLScrollListItem* item) const
{
	LLPathfindingObject::ptr_t objectp;

	const LLUUID& id = item->getUUID();
	if (mObjectList)
	{
		objectp = mObjectList->find(id);
	}
	else
	{
		llwarns << "NULL mObjectList !" << llendl;
	}

	return objectp;
}
