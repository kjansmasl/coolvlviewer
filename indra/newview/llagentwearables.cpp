/**
 * @file llagentwearables.cpp
 * @brief LLAgentWearables class implementation
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
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

#include "llviewerprecompiledheaders.h"

#include "boost/scoped_ptr.hpp"

#include "llagentwearables.h"

#include "llcallbacklist.h"
#include "llmd5.h"
#include "llnotifications.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "llfloatercustomize.h"
#include "llfloaterinventory.h"
#include "hbfloatermakenewoutfit.h"
#include "llgesturemgr.h"
#include "llgridmanager.h"					// For gIsInSecondLife
#include "llinventorybridge.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llviewerwearable.h"
#include "llwearablelist.h"

// Globals
LLAgentWearables gAgentWearables;
bool gWearablesListDirty = false;

using namespace LLAvatarAppearanceDefines;

//-----------------------------------------------------------------------------
// Support classes
//-----------------------------------------------------------------------------

class LLCreateStandardWearablesDoneCallback : public LLRefCount
{
protected:
	LOG_CLASS(LLCreateStandardWearablesDoneCallback);

	~LLCreateStandardWearablesDoneCallback()
	{
		LL_DEBUGS("Wearables") << "Destructor - all done ?" << LL_ENDL;
		gAgentWearables.createStandardWearablesAllDone();
	}
};

class LLSendAgentWearablesUpdateCallback : public LLRefCount
{
protected:
	~LLSendAgentWearablesUpdateCallback()
	{
		gAgentWearables.sendAgentWearablesUpdate();
	}
};

/**
 * @brief Construct a callback for dealing with the wearables.
 *
 * Would like to pass the agent in here, but we can't safely
 * count on it being around later.  Just use gAgent directly.
 * @param cb callback to execute on completion (??? unused ???)
 * @param type Type for the wearable in the agent
 * @param wearable The wearable data.
 * @param todo Bitmask of actions to take on completion.
 */
class LLAddWearableToInventoryCallback final : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLAddWearableToInventoryCallback);

public:
	enum ETodo
	{
		CALL_NONE = 0,
		CALL_UPDATE = 1,
		CALL_RECOVERDONE = 2,
		CALL_CREATESTANDARDDONE = 4,
		CALL_MAKENEWOUTFITDONE = 8
	};

	LLAddWearableToInventoryCallback(LLPointer<LLRefCount> cb,
									LLWearableType::EType type, U32 index,
									LLViewerWearable* wearable,
									U32 todo = CALL_NONE)
	:	mType(type),
		mIndex(index),
		mWearable(wearable),
		mTodo(todo),
		mCB(cb)
	{
		LL_DEBUGS("Wearables") << "Constructor" << LL_ENDL;
	}

	void fire(const LLUUID& inv_item) override
	{
		if (mTodo & CALL_CREATESTANDARDDONE)
		{
			llinfos << "Callback fired, inv_item " << inv_item.asString()
					<< llendl;
		}

		if (inv_item.isNull())
		{
			return;
		}

		gAgentWearables.addWearabletoAgentInventoryDone(mType, mIndex,
														inv_item, mWearable);

		if (mTodo & CALL_UPDATE)
		{
			// This calls sendAgentWearablesUpdate() (which was the call that
			// was made in v1 viewers), but also takes care of fully syncing
			// the outfit data (especially the outfit.xml file and/or COF).
			gAgentWearables.updateServer();
		}
		if (mTodo & CALL_RECOVERDONE)
		{
			gAgentWearables.recoverMissingWearableDone();
		}

		// Do this for every one in the loop
		if (mTodo & CALL_CREATESTANDARDDONE)
		{
			gAgentWearables.createStandardWearablesDone(mType, mIndex);
		}
		if (mTodo & CALL_MAKENEWOUTFITDONE)
		{
			gAgentWearables.makeNewOutfitDone(mType, mIndex);
		}
	}

private:
	LLWearableType::EType mType;
	U32 mIndex;
	LLViewerWearable* mWearable;
	U32 mTodo;
	LLPointer<LLRefCount> mCB;
};

class LLMoveAfterCopyDoneCallback final : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLMoveAfterCopyDoneCallback);

public:
	LLMoveAfterCopyDoneCallback(const LLUUID& item_id,
								const LLUUID& folder_id,
								const std::string& item_name)
	:	mItemID(item_id),
		mFolderID(folder_id),
		mItemName(item_name)
	{
	}

	void fire(const LLUUID& inv_item) override
	{
		move_inventory_item(mItemID, mFolderID, mItemName);
	}

private:
	LLUUID		mItemID;
	LLUUID		mFolderID;
	std::string	mItemName;
};

///////////////////////////////////////////////////////////////////////////////

LLAgentWearables::LLAgentWearables()
:	LLWearableData(),
	mWearablesLoaded(false),
	mIsSettingOutfit(false)
{
}

void LLAgentWearables::setAvatarObject(LLVOAvatarSelf* avatar)
{
	if (avatar)
	{
//MK
		// Set wear/unwear checking functions for class LLWearableData
		setCanWearFunc(LLAgent::canWear);
		setCanUnwearFunc(LLAgent::canUnwear);
//mk
		sendAgentWearablesRequest();
		setAvatarAppearance(avatar);
	}
}

void LLAgentWearables::sendAgentWearablesUpdate()
{
	LLViewerWearable* wearable;
	// First make sure that we have inventory items for each wearable
	for (S32 type = 0; type < LLWearableType::WT_COUNT; ++type)
	{
		for (U32 index = 0,
				 count = getWearableCount((LLWearableType::EType)type);
			 index < count; ++index)
		{
			wearable = getViewerWearable((LLWearableType::EType)type, index);
			if (wearable)
			{
				if (wearable->getItemID().isNull())
				{
					LLPointer<LLInventoryCallback> cb =
						new LLAddWearableToInventoryCallback(LLPointer<LLRefCount>(NULL),
															 (LLWearableType::EType)type,
															 index, wearable,
															 LLAddWearableToInventoryCallback::CALL_NONE);
					addWearableToAgentInventory(cb, wearable);
				}
				else
				{
					gInventory.addChangedMask(LLInventoryObserver::LABEL,
											  wearable->getItemID());
				}
			}
		}
	}

	// Then make sure the inventory is in sync with the avatar.
	gInventory.notifyObservers();

	// Send the AgentIsNowWearing
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AgentIsNowWearing);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	LL_DEBUGS("Wearables") << "sendAgentWearablesUpdate()" << LL_ENDL;
	// MULTI-WEARABLE: DEPRECATED: HACK: index to 0 - server database tables
	// don't support concept of multiwearables.
	for (S32 type = 0; type < LLWearableType::WT_COUNT; ++type)
	{
		msg->nextBlockFast(_PREHASH_WearableData);

		U8 type_u8 = (U8)type;
		msg->addU8Fast(_PREHASH_WearableType, type_u8);

		wearable = getViewerWearable((LLWearableType::EType)type, 0);
		if (wearable)
		{
			LLUUID item_id = wearable->getItemID();
			LL_DEBUGS("Wearables") << "Sending wearable " << wearable->getName()
								   << " mItemID = " << item_id << LL_ENDL;
			const LLViewerInventoryItem* item = gInventory.getItem(item_id);
			if (item && item->getIsLinkType())
			{
				// Get the itemID that this item points to.  i.e. make sure
				// we are storing baseitems, not their links, in the database.
				item_id = item->getLinkedUUID();
			}
			msg->addUUIDFast(_PREHASH_ItemID, item_id);
		}
		else
		{
			LL_DEBUGS("Wearables") << "Not wearing wearable type: "
								   << LLWearableType::getTypeName((LLWearableType::EType)type)
								   << LL_ENDL;
			msg->addUUIDFast(_PREHASH_ItemID, LLUUID::null);
		}

		LL_DEBUGS("Wearables") << "       "
							   << LLWearableType::getTypeLabel((LLWearableType::EType)type)
							   << ": "
							   << (wearable ? wearable->getAssetID() : LLUUID::null)
							   << LL_ENDL;
	}
	gAgent.sendReliableMessage();
}

void LLAgentWearables::saveWearable(LLWearableType::EType type, U32 index,
									bool send_update,
									const std::string& new_name)
{
	LLViewerWearable* old_wearable = getViewerWearable(type, index);
	if (!old_wearable || !isAgentAvatarValid()) return;
	bool name_changed = !new_name.empty() &&
						new_name != old_wearable->getName();
	if (name_changed || old_wearable->isDirty() ||
		old_wearable->isOldVersion())
	{
		LLUUID old_item_id = old_wearable->getItemID();
		LLViewerWearable* new_wearable =
			LLWearableList::getInstance()->createCopy(old_wearable, "");
		// should the following line be in LLViewerWearable::copyDataFrom() ?
		new_wearable->setItemID(old_item_id);
		setWearable(type, index, new_wearable);
#if 1
		// old_wearable may still be referred to by other inventory items.
		// Revert unsaved changes so other inventory items aren't affected by
		// the changes that were just saved.
		old_wearable->revertValuesWithoutUpdate();
#endif
		LLViewerInventoryItem* item = gInventory.getItem(old_item_id);
		if (item)
		{
			std::string item_name = item->getName();
			if (name_changed)
			{
				llinfos << "Changing name from " << item->getName() << " to "
						<< new_name << llendl;
				item_name = new_name;
			}
			// Update existing inventory item
			LLPointer<LLViewerInventoryItem> template_item =
				new LLViewerInventoryItem(item->getUUID(),
										  item->getParentUUID(),
										  item->getPermissions(),
										  new_wearable->getAssetID(),
										  new_wearable->getAssetType(),
										  item->getInventoryType(),
										  item_name, item->getDescription(),
										  item->getSaleInfo(),
										  item->getFlags(),
										  item->getCreationDate());
			template_item->setTransactionID(new_wearable->getTransactionID());
			update_inventory_item(template_item);
		}
		else
		{
			// Add a new inventory item (shouldn't ever happen here)
			U32 todo = LLAddWearableToInventoryCallback::CALL_NONE;
			if (send_update)
			{
				todo |= LLAddWearableToInventoryCallback::CALL_UPDATE;
			}
			LLPointer<LLInventoryCallback> cb =
				new LLAddWearableToInventoryCallback(LLPointer<LLRefCount>(NULL),
													 type, index, new_wearable,
													 todo);
			addWearableToAgentInventory(cb, new_wearable);
			return;
		}

		gAgentAvatarp->wearableUpdated(type, true);

		if (send_update)
		{
			sendAgentWearablesUpdate();
		}
	}
}

void LLAgentWearables::saveWearableAs(LLWearableType::EType type, U32 index,
									  const std::string& new_name,
									  bool save_in_laf)
{
	if (!isWearableCopyable(type, index))
	{
		llwarns << "Wearable not copyable." << llendl;
		return;
	}
	LLViewerWearable* old_wearable = getViewerWearable(type, index);
	if (!old_wearable)
	{
		llwarns << "No old wearable." << llendl;
		return;
	}

	LLInventoryItem* item = gInventory.getItem(getWearableItemID(type, index));
	if (!item)
	{
		llwarns << "No inventory item." << llendl;
		return;
	}
	std::string trunc_name(new_name);
	LLStringUtil::truncate(trunc_name, DB_INV_ITEM_NAME_STR_LEN);
	LLViewerWearable* new_wearable =
		LLWearableList::getInstance()->createCopy(old_wearable, trunc_name);
	LLPointer<LLInventoryCallback> cb =
		new LLAddWearableToInventoryCallback(LLPointer<LLRefCount>(NULL),
											 type, index, new_wearable,
											 LLAddWearableToInventoryCallback::CALL_UPDATE);
	const LLUUID& cat_id = save_in_laf ? gInventory.getLostAndFoundID()
									   : item->getParentUUID();
	if (cat_id.isNull())
	{
		llwarns << "Could not find the destination folder." << llendl;
		return;
	}
	copy_inventory_item(item->getPermissions().getOwner(), item->getUUID(),
						cat_id, new_name, cb);
#if 1
	// old_wearable may still be referred to by other inventory items. Revert
	// unsaved changes so other inventory items aren't affected by the changes
	// that were just saved.
	old_wearable->revertValuesWithoutUpdate();
#endif
}

void LLAgentWearables::revertWearable(LLWearableType::EType type, U32 index)
{
	LLViewerWearable* wearable = getViewerWearable(type, index);
	if (wearable)
	{
		wearable->revertValues();
	}
	gAgent.sendAgentSetAppearance();
}

void LLAgentWearables::saveAllWearables()
{
#if 0
	if (!gInventory.isLoaded())
	{
		return;
	}
#endif

	// This prevents too fast an update of the COF while each wearable saving
	// gets (slowly) acknowledged one after the other by the asset server:
	LLWearableSaveData::sResetCOFTimer = true;

	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		for (U32 j = 0, count = getWearableCount((LLWearableType::EType)i);
			 j < count; ++j)
		{
			saveWearable((LLWearableType::EType)i, j, false);
		}
	}

	LLWearableSaveData::sResetCOFTimer = false;

	sendAgentWearablesUpdate();
}

// Called when the user changes the name of a wearable inventory item that is
// currently being worn.
void LLAgentWearables::setWearableName(const LLUUID& item_id,
									   const std::string& new_name)
{
	LLWearableList* wl = LLWearableList::getInstance();
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		for (U32 j = 0, count = getWearableCount((LLWearableType::EType)i);
			 j < count; ++j)
		{
			const LLUUID& curr_item_id =
				getWearableItemID((LLWearableType::EType)i, j);
			if (curr_item_id == item_id)
			{
				LLViewerWearable* old_wearable =
					getViewerWearable((LLWearableType::EType)i, j);
				llassert(old_wearable);	//if (!old_wearable) continue;

				std::string old_name = old_wearable->getName();
				old_wearable->setName(new_name);
				LLViewerWearable* new_wearable = wl->createCopy(old_wearable);
				new_wearable->setItemID(item_id);
				LLInventoryItem* item = gInventory.getItem(item_id);
				if (item)
				{
					new_wearable->setPermissions(item->getPermissions());
				}
				old_wearable->setName(old_name);

				setWearable((LLWearableType::EType)i, j, new_wearable);
				sendAgentWearablesUpdate();
				break;
			}
		}
	}
}

bool LLAgentWearables::isWearableModifiable(LLWearableType::EType type,
											U32 index) const
{
	const LLUUID& item_id = getWearableItemID(type, index);
	return item_id.notNull() && isWearableModifiable(item_id);
}

bool LLAgentWearables::isWearableModifiable(const LLUUID& item_id) const
{
	const LLUUID& linked_id = gInventory.getLinkedItemID(item_id);
	if (linked_id.notNull())
	{
		LLInventoryItem* item = gInventory.getItem(linked_id);
		if (item && item->getPermissions().allowModifyBy(gAgentID,
														 gAgent.getGroupID()))
		{
			return true;
		}
	}
	return false;
}

bool LLAgentWearables::isWearableCopyable(LLWearableType::EType type,
										  U32 index) const
{
	const LLUUID& item_id = getWearableItemID(type, index);
	if (item_id.notNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if (item && item->getPermissions().allowCopyBy(gAgentID,
													   gAgent.getGroupID()))
		{
			return true;
		}
	}
	return false;
}

LLViewerInventoryItem* LLAgentWearables::getWearableInventoryItem(LLWearableType::EType type,
																  U32 index)
{
	const LLUUID& item_id = getWearableItemID(type, index);
	return item_id.notNull() ? gInventory.getItem(item_id) : NULL;
}

const LLViewerWearable* LLAgentWearables::getWearableFromItemID(const LLUUID& item_id) const
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		for (U32 j = 0, count = getWearableCount((LLWearableType::EType)i);
			 j < count; ++j)
		{
			const LLViewerWearable* curr_wearable =
				getViewerWearable((LLWearableType::EType)i, j);
			if (curr_wearable && curr_wearable->getItemID() == base_item_id)
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

LLViewerWearable* LLAgentWearables::getWearableFromItemID(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		for (U32 j = 0, count = getWearableCount((LLWearableType::EType)i);
			 j < count; ++j)
		{
			LLViewerWearable* curr_wearable =
				getViewerWearable((LLWearableType::EType)i, j);
			if (curr_wearable && curr_wearable->getItemID() == base_item_id)
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

LLViewerWearable* LLAgentWearables::getWearableFromAssetID(const LLUUID& asset_id)
{
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		for (U32 j = 0, count = getWearableCount((LLWearableType::EType)i);
			 j < count; ++j)
		{
			LLViewerWearable* curr_wearable =
				getViewerWearable((LLWearableType::EType)i, j);
			if (curr_wearable && curr_wearable->getAssetID() == asset_id)
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

void LLAgentWearables::sendAgentWearablesRequest()
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AgentWearablesRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	gAgent.sendReliableMessage();
}

LLViewerWearable* LLAgentWearables::getViewerWearable(LLWearableType::EType type,
													  U32 index)
{
	LLWearable* wearable = getWearable(type, index);
	return wearable ? wearable->asViewerWearable() : NULL;
}

const LLViewerWearable* LLAgentWearables::getViewerWearable(LLWearableType::EType type,
															U32 index) const
{
	const LLWearable* wearable = getWearable(type, index);
	return wearable ? wearable->asViewerWearable() : NULL;
}

//static
bool LLAgentWearables::selfHasWearable(LLWearableType::EType type)
{
	return gAgentWearables.getWearableCount(type) > 0;
}

//virtual
void LLAgentWearables::wearableUpdated(LLWearable* wearable, bool removed)
{
	if (!wearable || !isAgentAvatarValid()) return;

	gAgentAvatarp->wearableUpdated(wearable->getType(), removed);
	LLWearableData::wearableUpdated(wearable, removed);

	LLViewerWearable* viewer_wearable = wearable->asViewerWearable();
	if (!removed && viewer_wearable)
	{
		viewer_wearable->refreshName();

		// Hack pt 2. If the wearable we just loaded has definition version 24,
		// then force a re-save of this wearable after slamming the version
		// number to 22. This number was incorrectly incremented for internal
		// builds before release, and this fix will ensure that the affected
		// wearables are re-saved with the right version number. The versions
		// themselves are compatible. This code can be removed before release.
		if (wearable->getDefinitionVersion() == 24)
		{
			U32 index;
			if (getWearableIndex(wearable, index))
			{
				llinfos << "forcing werable type " << wearable->getType()
						<< " to version 22 from 24" << llendl;
				wearable->setDefinitionVersion(22);
				saveWearable(wearable->getType(), index, true);
			}
		}
	}

	if (gFloaterCustomizep && viewer_wearable)
	{
		gFloaterCustomizep->updateWearableType(viewer_wearable->getType(),
											   viewer_wearable);
	}
}

const LLUUID& LLAgentWearables::getWearableItemID(LLWearableType::EType type,
												 U32 index) const
{
	const LLViewerWearable* wearable = getViewerWearable(type, index);
	return wearable ? wearable->getItemID() : LLUUID::null;
}

const LLUUID& LLAgentWearables::getWearableAssetID(LLWearableType::EType type,
												  U32 index) const
{
	const LLViewerWearable* wearable = getViewerWearable(type, index);
	return wearable ? wearable->getAssetID() : LLUUID::null;
}

bool LLAgentWearables::isWearingItem(const LLUUID& item_id) const
{
	return getWearableFromItemID(gInventory.getLinkedItemID(item_id)) != NULL;
}

void LLAgentWearables::setInitialWearablesUpdateReceived()
{
	mInitialWearablesUpdateReceived = mWearablesLoaded = true;
}

// OPENSIM COMPATIBILITY
//static
void LLAgentWearables::processAgentInitialWearablesUpdate(LLMessageSystem* mesgsys,
														  void** user_data)
{
	if (gIsInSecondLife)
	{
		gAgentWearables.setInitialWearablesUpdateReceived();
		llinfos << "Received initial agent wearables message in state: "
				<< LLStartUp::getStartupStateString() << llendl;
		// Simply ignore this message: it's no more conveying valid data in SL
		return;
	}

	// We should only receive this message a single time. Ignore subsequent
	// AgentWearablesUpdates that may result from AgentWearablesRequest having
	// been sent more than once.
	if (gAgentWearables.mInitialWearablesUpdateReceived)
	{
		LL_DEBUGS("InitialOutfit") << "Spurious AgentWearablesUpdates message received, ignoring..."
								   << LL_ENDL;
		return;
	}

	LLUUID agent_id;
	LLMessageSystem* msg = gMessageSystemp;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (isAgentAvatarValid() && agent_id == gAgentAvatarp->getID())
	{
		LL_DEBUGS("InitialOutfit") << "Initial AgentWearablesUpdates message received."
								   << LL_ENDL;
		gAgentWearables.mInitialWearablesUpdateReceived = true;
		msg->getU32Fast(_PREHASH_AgentData, _PREHASH_SerialNum,
						gAgentQueryManager.mUpdateSerialNum);

		constexpr S32 NUM_BODY_PARTS = 4;
		S32 num_wearables = msg->getNumberOfBlocksFast(_PREHASH_WearableData);
		if (num_wearables < NUM_BODY_PARTS)
		{
			// Transitional state. Avatars should always have at least their
			// body parts (hair, eyes, shape and skin).
			// The fact that they don't have any here (only a dummy is sent)
			// implies that either:
			// 1. This account existed before we had wearables
			// 2. The database has gotten messed up
			// 3. This is the account's first login (i.e. the wearables haven't
			//    been generated yet).
			llwarns << "Insufficient number of wearables, aborting." << llendl;
			return;
		}

		bool restore_from_cof = gSavedSettings.getBool("RestoreOutfitFromCOF");
		if (!gIsInSecondLife && !gSavedSettings.getBool("OSUseCOF"))
		{
			restore_from_cof = false;
		}

		// Add wearables
		// MULTI-WEARABLE: DEPRECATED: Message only supports one wearable per
		// type, will be ignored in future.
		std::pair<LLUUID, LLUUID> asset_id_array[LLWearableType::WT_COUNT];
		for (S32 i = 0; i < num_wearables; ++i)
		{
			// Parse initial wearables data from message system
			U8 type_u8 = 0;
			msg->getU8Fast(_PREHASH_WearableData, _PREHASH_WearableType, type_u8, i);
			if (type_u8 >= LLWearableType::WT_COUNT)
			{
				continue;
			}
			LLWearableType::EType type = (LLWearableType::EType)type_u8;

			LLAssetType::EType asset_type = LLWearableType::getAssetType(type);
			if (asset_type == LLAssetType::AT_NONE)
			{
				continue;
			}

			LLUUID item_id;
			msg->getUUIDFast(_PREHASH_WearableData, _PREHASH_ItemID,
							 item_id, i);
			if (item_id.isNull())
			{
				continue;
			}

			LLUUID asset_id;
			msg->getUUIDFast(_PREHASH_WearableData, _PREHASH_AssetID,
							 asset_id, i);
			if (asset_id.isNull())
			{
//MK
				gRLInterface.mRestoringOutfit = true;
//mk
				LLViewerWearable::removeFromAvatar(type, false);
//MK
				gRLInterface.mRestoringOutfit = false;
//mk
			}
			else
			{
				// NOTE: when restoring from COF, only wear the body parts (so
				// to de-cloud the avatar): the rest will be restored from the
				// COF. This prevents issues with wearables that use the
				// maximum number of layers (the last layer would fail to be
				// worn if the first layer is already worn, which is not
				// detected by the setWearableOutfit() method which is used to
				// restore from the COF).
				if (!restore_from_cof ||
					asset_type == LLAssetType::AT_BODYPART)
				{
					asset_id_array[type] = std::make_pair(asset_id, item_id);
					LL_DEBUGS("InitialOutfit") << "Wearable type: "
											   << LLWearableType::getTypeLabel(type)
											   << ", Asset Id: " << asset_id
											   << ", Item Id: "
											   << gAgentWearables.getWearableItemID(type, 0)
											   << LL_ENDL;
				}
			}
		}

		// Now that we have the asset IDs, request the wearable assets
		LLWearableList* wl = LLWearableList::getInstance();
		for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
		{
			LL_DEBUGS("InitialOutfit") << "Fetching asset. Id: "
									   << asset_id_array[i].first << LL_ENDL;
			const LLUUID item_id = asset_id_array[i].second;
			if (asset_id_array[i].second.notNull())
			{
				wl->getAsset(asset_id_array[i].first, LLStringUtil::null,
							 gAgentAvatarp,
							 LLWearableType::getAssetType((LLWearableType::EType)i),
							 LLAgentWearables::onInitialWearableAssetArrived,
							 (void*)new std::pair<const LLWearableType::EType,
												  const LLUUID>((LLWearableType::EType)i,
																item_id));
			}
		}
	}
	else
	{
		LL_DEBUGS("InitialOutfit") << "AgentWearablesUpdates message received but not for us, ignoring..."
								   << LL_ENDL;
	}
}

// A single wearable that the avatar was wearing on start-up has arrived from
// the database.
//static
void LLAgentWearables::onInitialWearableAssetArrived(LLViewerWearable* wearable,
													 void* userdata)
{
	if (!userdata) return;

	std::pair<const LLWearableType::EType, const LLUUID>* wearable_data;
	wearable_data = (std::pair<const LLWearableType::EType,
							   const LLUUID>*)userdata;
	LLWearableType::EType type = wearable_data->first;
	LLUUID item_id = wearable_data->second;

	if (!isAgentAvatarValid())
	{
		LL_DEBUGS("InitialOutfit") << "Agent is not valid !" << LL_ENDL;
		return;
	}

//MK
	gRLInterface.mRestoringOutfit = true;
//mk
	if (wearable)
	{
		LL_DEBUGS("InitialOutfit") << "Adding wearable: " << item_id << LL_ENDL;
		llassert(type == wearable->getType());

		wearable->setItemID(item_id);
		gAgentWearables.setWearable(type, 0, wearable);

		// disable composites if initial textures are baked
		gAgentAvatarp->setupComposites();
		gAgentWearables.queryWearableCache();

		gAgentAvatarp->setCompositeUpdatesEnabled(true);
		gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);
	}
	else
	{
		// Somehow the asset doesn't exist in the database.
		LL_DEBUGS("InitialOutfit") << "Missing wearable for type " << type
								   << ", starting recovery." << LL_ENDL;
		gAgentWearables.recoverMissingWearable(type, 0);
	}
//MK
	gRLInterface.mRestoringOutfit = false;
//mk

	gInventory.notifyObservers();

	// Have all the wearables that the avatar was wearing at log-in arrived ?
	if (!gAgentWearables.mWearablesLoaded)
	{
		gAgentWearables.mWearablesLoaded = true;
		for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
		{
			if (gAgentWearables.getWearableItemID((LLWearableType::EType)i, 0).notNull() &&
				!gAgentWearables.getViewerWearable((LLWearableType::EType)i, 0))
			{
				LL_DEBUGS("InitialOutfit") << "Not all wearables have loaded yet."
										   << LL_ENDL;
				gAgentWearables.mWearablesLoaded = false;
				break;
			}
		}
	}

	if (gAgentWearables.mWearablesLoaded)
	{
		LL_DEBUGS("InitialOutfit") << "All wearables have loaded." << LL_ENDL;
		// Make sure that the server's idea of the avatar's wearables actually
		// match the wearables.
		gAgent.sendAgentSetAppearance();

		// Check to see if there are any baked textures that we hadn't uploaded
		// before we logged off last time.
		// If there are any, schedule them to be uploaded as soon as the layer
		// textures they depend on arrive.
		if (!gAgent.cameraCustomizeAvatar())
		{
			gAgentAvatarp->requestLayerSetUploads();
		}
	}
}

// Normally, all wearables referred to "AgentWearablesUpdate" will correspond
// to actual assets in the database. If for some reason, we can't load one of
// those assets, we can try to reconstruct it so that the user isn't left
// without a shape, for example. We can do that only after the inventory has
// loaded.
void LLAgentWearables::recoverMissingWearable(LLWearableType::EType type,
											  U32 index)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	// Try to recover by replacing missing wearable with a new one.
	gNotifications.add("ReplacedMissingWearable");
	LL_DEBUGS("Wearables") << "Wearable " << LLWearableType::getTypeLabel(type)
						   << " could not be downloaded. Replaced inventory item with default wearable."
						   << LL_ENDL;
	LLViewerWearable* new_wearable =
		LLWearableList::getInstance()->createNewWearable(type, gAgentAvatarp);
	setWearable(type, index, new_wearable);

	// Add a new one in the lost and found folder (we used to overwrite the
	// "not found" one, but that could potentially destroy content). JC
	LLPointer<LLInventoryCallback> cb =
		new LLAddWearableToInventoryCallback(LLPointer<LLRefCount>(NULL),
											 type, index, new_wearable,
											 LLAddWearableToInventoryCallback::CALL_RECOVERDONE);
	addWearableToAgentInventory(cb, new_wearable,
								gInventory.getLostAndFoundID(), true);
}

void LLAgentWearables::recoverMissingWearableDone()
{
	// Have all the wearables that the avatar was wearing at log-in arrived or
	// been fabricated?
	updateWearablesLoaded();
	if (areWearablesLoaded())
	{
		// Make sure that the server's idea of the avatar's wearables actually
		// match the wearables.
		gAgent.sendAgentSetAppearance();
	}
	else
	{
		gInventory.addChangedMask(LLInventoryObserver::LABEL, LLUUID::null);
		gInventory.notifyObservers();
	}
}

LLLocalTextureObject* LLAgentWearables::addLocalTextureObject(LLWearableType::EType type,
															  LLAvatarAppearanceDefines::ETextureIndex texture_type,
															  U32 index)
{
	LLViewerWearable* wearable =
		getViewerWearable((LLWearableType::EType)type, index);
	if (wearable)
	{
		LLLocalTextureObject lto;
		return wearable->setLocalTextureObject(texture_type, lto);
	}
	return NULL;
}

void LLAgentWearables::createStandardWearables(bool female)
{
	llwarns << "Creating standard " << (female ? "female" : "male")
			<< " wearables" << llendl;

	if (!isAgentAvatarValid())
	{
		return;
	}

	gAgentAvatarp->setSex(female ? SEX_FEMALE : SEX_MALE);

	bool create[LLWearableType::WT_COUNT] =
	{
		true,  //WT_SHAPE
		true,  //WT_SKIN
		true,  //WT_HAIR
		true,  //WT_EYES
		true,  //WT_SHIRT
		true,  //WT_PANTS
		true,  //WT_SHOES
		true,  //WT_SOCKS
		false, //WT_JACKET
		false, //WT_GLOVES
		true,  //WT_UNDERSHIRT
		true,  //WT_UNDERPANTS
		false, //WT_SKIRT
		false, //WT_ALPHA
		false, //WT_TATTOO
		false, //WT_PHYSICS
		false  //WT_UNIVERSAL
	};

	LLWearableList* wl = LLWearableList::getInstance();
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		bool once = false;
		LLPointer<LLRefCount> donecb = NULL;
		if (create[i])
		{
			if (!once)
			{
				once = true;
				donecb = new LLCreateStandardWearablesDoneCallback;
			}
			llassert(getWearableCount((LLWearableType::EType)i) == 0);
			LLViewerWearable* wearable =
				wl->createNewWearable((LLWearableType::EType)i, gAgentAvatarp);
			// no need to update here...
			LLPointer<LLInventoryCallback> cb =
				new LLAddWearableToInventoryCallback(donecb,
													 (LLWearableType::EType)i,
													 0, wearable,
													 LLAddWearableToInventoryCallback::CALL_CREATESTANDARDDONE);
			addWearableToAgentInventory(cb, wearable, LLUUID::null, false);
		}
	}
}

void LLAgentWearables::createStandardWearablesDone(S32 type, U32 index)
{
	if (isAgentAvatarValid())
	{
		// Copy wearable params to avatar.
		gAgentAvatarp->writeWearablesToAvatar();
		// Then update the avatar based on the copied params.
		gAgentAvatarp->updateVisualParams();
	}
}

void LLAgentWearables::createStandardWearablesAllDone()
{
	// ...because sendAgentWearablesUpdate will notify inventory observers.
	mWearablesLoaded = true;
	updateServer();

	// Treat this as the first texture entry message, if none received yet
	gAgentAvatarp->onFirstTEMessageReceived();
}

S32 LLAgentWearables::getWearableTypeAndIndex(LLViewerWearable* wearable,
											  LLWearableType::EType& type)
{
	S32 layer = -1;
	if (wearable)
	{
		type = wearable->getType();
		for (U32 index = 0, count = getWearableCount(type); index < count;
			 ++index)
		{
			LLViewerWearable* worn = getViewerWearable(type, index);
			if (worn == wearable)
			{
				layer = index;
				break;
			}
		}
		
	}
	return layer;
}

struct HBNewOutfitData
{
	HBNewOutfitData(const uuid_vec_t& wearables_to_include,
					const uuid_vec_t& attachments_to_include,
					bool rename_clothing)
	:	mWearables(wearables_to_include),
		mAttachments(attachments_to_include),
		mRenameClothing(rename_clothing)
	{
	}

	uuid_vec_t	mWearables;
	uuid_vec_t	mAttachments;
	bool		mRenameClothing;
};

void LLAgentWearables::makeNewOutfit(const std::string& new_folder_name,
									 const uuid_vec_t& wearables_to_include,
									 const uuid_vec_t& attachments_to_include,
									 bool rename_clothing)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	// Store the required data for the callback in a temporary structure (this
	// is slow, but this is the cost for using asynchronous callbacks). HB
	HBNewOutfitData* datap = new HBNewOutfitData(wearables_to_include,
												 attachments_to_include,
												 rename_clothing);

	// We will create a sub-folder in the user-preferred folder or the Clothing
	// folder by default (FT_MY_OUTFITS becomes FT_CLOTHING on purpose in
	// findChoosenCategoryUUIDForType() when no user preferred folder is set).
	const LLUUID clothing_folder_id =
		gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_MY_OUTFITS);

	// Create the category and, on completion, call back our method to link,
	// copy or move the items into it. HB
	inventory_func_t func = boost::bind(&LLAgentWearables::makeNewOutfitCopy,
										this, _1, datap);
	gInventory.createNewCategory(clothing_folder_id, LLFolderType::FT_NONE,
								 new_folder_name, func);

}

void LLAgentWearables::makeNewOutfitCopy(const LLUUID& cat_id,
										 HBNewOutfitData* datap)
{
	if (!datap) return;	// Paranoia

	if (cat_id.isNull())
	{
		gNotifications.add("CantCreateRequestedInvFolder");
		delete datap;
		return;
	}

	bool found_first_item = false;
	bool no_link = !gSavedSettings.getBool("UseInventoryLinks");
	bool do_link = gSavedSettings.getBool("UseInventoryLinksAlways");
	bool cloth_link = gSavedSettings.getBool("UseInventoryLinksForClothes");

	///////////////////
	// Wearables

	if (datap->mWearables.size())
	{
		LLWearableList* wl = LLWearableList::getInstance();
		// Then, iterate though each of the wearables and save copies of them
		// in the folder.
		LLPointer<LLRefCount> cbdone = NULL;
		LLWearableType::EType type;
		std::string new_name, name_base;
		if (datap->mRenameClothing)
		{
			LLViewerInventoryCategory* catp = gInventory.getCategory(cat_id);
			if (catp)	// Paranoia
			{
				name_base = catp->getName() + " ";
			}
		}
		for (S32 i = 0, count = datap->mWearables.size(); i < count; ++i)
		{
			const LLUUID& item_id = datap->mWearables[i];
			LLViewerWearable* old_wearablep = getWearableFromItemID(item_id);
			S32 index = getWearableTypeAndIndex(old_wearablep, type);
			if (index < 0) continue;	// Not found/not worn

			bool use_link = do_link ||
							(cloth_link && type >= LLWearableType::WT_SHIRT);
			LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
			if (!itemp)
			{
				llwarns << "Could not find inventory item for wearable type: "
						<< LLWearableType::getTypeLabel(type)
						<< " - layer index: " << index << llendl;
				continue;
			}

			if (name_base.empty())
			{
				new_name = itemp->getName();
			}
			else
			{
				new_name = name_base + old_wearablep->getTypeLabel();
				if (index > 0)
				{
					if ((S32)new_name.length() + 10 <= DB_INV_ITEM_NAME_STR_LEN)
					{
						new_name += llformat(" (layer %d)", index);
					}
					else
					{
						new_name += llformat("#%d", index);
					}
				}
				LLStringUtil::truncate(new_name, DB_INV_ITEM_NAME_STR_LEN);
			}

			bool can_copy = isWearableCopyable(type, index);
			if (!use_link && (no_link || can_copy))
			{
				if (can_copy)
				{
					LLViewerWearable* new_wearablep = wl->createCopy(old_wearablep);
					if (datap->mRenameClothing)
					{
						new_wearablep->setName(new_name);
					}

					S32 todo = LLAddWearableToInventoryCallback::CALL_NONE;
					if (!found_first_item)
					{
						found_first_item = true;
						// set the focus to the first item
						todo |= LLAddWearableToInventoryCallback::CALL_MAKENEWOUTFITDONE;
						// Send the agent wearables update when done
						cbdone = new LLSendAgentWearablesUpdateCallback;
					}
					LLPointer<LLInventoryCallback> cb =
						new LLAddWearableToInventoryCallback(cbdone, type,
															 index,
															 new_wearablep,
															 todo);
					copy_inventory_item(itemp->getPermissions().getOwner(),
										itemp->getLinkedUUID(), cat_id,
										new_name, cb);
				}
				else
				{
					move_inventory_item(itemp->getLinkedUUID(), cat_id,
										new_name);
				}
			}
			else
			{
				link_inventory_item(itemp->getLinkedUUID(), cat_id,
									// For auto-ordering on outfit wearing:
									build_order_string(type, index),
									LLAssetType::AT_LINK);
			}
		}
		gInventory.notifyObservers();
	}

	///////////////////
	// Attachments

	if (datap->mAttachments.size())
	{
		for (S32 i = 0, count = datap->mAttachments.size(); i < count; ++i)
		{
			const LLUUID& item_id = datap->mAttachments[i];
			LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
			if (!itemp) continue;

			const LLUUID& inv_item_id = itemp->getLinkedUUID();
			if (!do_link &&
				(no_link || itemp->getPermissions().allowCopyBy(gAgentID)))
			{
				const std::string item_name = itemp->getName();
				const LLUUID& old_cat_id = itemp->getParentUUID();
				if (itemp->getPermissions().allowCopyBy(gAgentID))
				{
					LLPointer<LLInventoryCallback> cb =
						new LLMoveAfterCopyDoneCallback(inv_item_id, cat_id,
														item_name);
					copy_inventory_item(itemp->getPermissions().getOwner(),
										inv_item_id, old_cat_id, item_name,
										cb);
				}
				else
				{
					move_inventory_item(inv_item_id, cat_id, item_name);
				}
			}
			else
			{
				link_inventory_item(inv_item_id, cat_id,
									itemp->getDescription(),
									LLAssetType::AT_LINK);
			}
		}
		gInventory.notifyObservers();
	}

	delete datap;
}

void LLAgentWearables::makeNewOutfitDone(LLWearableType::EType type, U32 index)
{
	const LLUUID& first_item_id = getWearableItemID(type, index);
	if (first_item_id.isNull()) return;

	// Open the inventory and select the first item we added.
	LLFloaterInventory* floaterp = LLFloaterInventory::getActiveFloater();
	if (floaterp)
	{
		floaterp->getPanel()->setSelection(first_item_id, TAKE_FOCUS_NO);
	}
}

void LLAgentWearables::addWearableToAgentInventory(LLPointer<LLInventoryCallback> cb,
												   LLViewerWearable* wearable,
												   const LLUUID& category_id,
												   bool)	// unused: remove ?
{
	create_inventory_item(category_id, wearable->getTransactionID(),
						  wearable->getName(), wearable->getDescription(),
						  wearable->getAssetType(),
						  LLInventoryType::IT_WEARABLE,
						  (U8)wearable->getType(),
						  wearable->getPermissions().getMaskNextOwner(), cb);
}

void LLAgentWearables::addWearabletoAgentInventoryDone(LLWearableType::EType type,
													   U32 index,
													   const LLUUID& item_id,
													   LLViewerWearable* wearable)
{
	llinfos << "type " << type << " index " << index << " item "
			<< item_id.asString() << llendl;

	if (item_id.isNull())
	{
		return;
	}

	const LLUUID& old_item_id = getWearableItemID(type, index);

	if (wearable)
	{
		wearable->setItemID(item_id);

		if (old_item_id.notNull())
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
			setWearable(type, index, wearable);
		}
		else
		{
			pushWearable(type, wearable);
		}
	}

	gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if (item && wearable)
	{
		// We're changing the asset id, so we both need to set it
		// locally via setAssetUUID() and via setTransactionID() which
		// will be decoded on the server. JC
		item->setAssetUUID(wearable->getAssetID());
		item->setTransactionID(wearable->getTransactionID());
		gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		item->updateServer(false);
	}
	gInventory.notifyObservers();
}

void LLAgentWearables::removeWearable(LLWearableType::EType type,
									  bool do_remove_all, U32 index)
{
//MK
	if (gRLenabled && !gRLInterface.canUnwear(type))
	{
		return;
	}
//mk
	U32 count = getWearableCount(type);
#if LL_TEEN_WERABLE_RESTRICTIONS
	bool is_teen_and_underwear = gAgent.isTeen() &&
								 (type == LLWearableType::WT_UNDERSHIRT ||
								  type == LLWearableType::WT_UNDERPANTS);
	if (count == 0 || (is_teen_and_underwear && count == 1))
#else
	if (count == 0)
#endif
	{
		// No wearable to remove or teen trying to remove their last underwear
		return;
	}

	if (do_remove_all)
	{
#if LL_TEEN_WERABLE_RESTRICTIONS
		if (is_teen_and_underwear)
		{
			// Remove all but one layer
			for (U32 index = count - 1; index > 0; --index)
			{
				removeWearableFinal(type, false, index);
			}
		}
		else
#endif
		{
			removeWearableFinal(type, true, 0);
		}
	}
	else
	{
		LLViewerWearable* old_wearable = getViewerWearable(type, index);
		if (old_wearable)
		{
			if (old_wearable->isDirty())
			{
				LLSD payload;
				payload["wearable_type"] = (S32)type;
				payload["wearable_index"] = (S32)index;
				// Bring up view-modal dialog: Save changes? Yes, No, Cancel
				gNotifications.add("WearableSave", LLSD(), payload,
								   onRemoveWearableDialog);
				return;
			}
			else
			{
				removeWearableFinal(type, do_remove_all, index);
			}
		}
	}
}

//static
bool LLAgentWearables::onRemoveWearableDialog(const LLSD& notification,
											  const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLWearableType::EType type =
		(LLWearableType::EType)notification["payload"]["wearable_type"].asInteger();
	S32 index = (S32)notification["payload"]["wearable_index"].asInteger();
	switch (option)
	{
		case 0:  // "Save"
			gAgentWearables.saveWearable(type, index);
			gAgentWearables.removeWearableFinal(type, false, index);
			break;

		case 1:  // "Don't Save"
			gAgentWearables.removeWearableFinal(type, false, index);
			break;

		case 2: // "Cancel"
			break;

		default:
			llassert(0);
			break;
	}
	return false;
}

// Called by removeWearable() and onRemoveWearableDialog() to actually do the
// removal.
void LLAgentWearables::removeWearableFinal(LLWearableType::EType type,
										   bool do_remove_all,
										   U32 index)
{
	LLViewerWearable* old_wearable;
	if (do_remove_all)
	{
//MK
		bool all_removed = true;
//mk
		S32 max_entry = getWearableCount(type) - 1;
		for (S32 i = max_entry; i >= 0; --i)
		{
			old_wearable = getViewerWearable(type, i);
			if (old_wearable)
			{
//MK
				if (gRLenabled)
				{
					LLViewerInventoryItem* old_item;
					old_item = gInventory.getItem(old_wearable->getItemID());
					if (old_item && !gRLInterface.canUnwear(old_item))
					{
						all_removed = false;
						continue;
					}
				}
//mk
				eraseWearable(old_wearable);
				old_wearable->removeFromAvatar(true);
			}
		}
//MK
		if (all_removed)
//mk
		{
			clearWearableType(type);
		}
	}
	else
	{
		old_wearable = getViewerWearable(type, index);
		if (old_wearable)
		{
//MK
			if (gRLenabled)
			{
				LLViewerInventoryItem* old_item;
				old_item = gInventory.getItem(old_wearable->getItemID());
				if (old_item && !gRLInterface.canUnwear(old_item))
				{
					return;
				}
			}
//mk
			eraseWearable(old_wearable);
			old_wearable->removeFromAvatar(true);
		}
	}

//MK
	if (gRLenabled)
	{
		std::string layer = gRLInterface.getOutfitLayerAsString(type);
		if (!layer.empty())
		{
			gRLInterface.notify("unworn legally " + layer);
		}
	}
//mk

	queryWearableCache();

	// Update the server
	updateServer();

	if (gFloaterCustomizep)
	{
		gFloaterCustomizep->updateWearableType(type, NULL);
	}
}

// Assumes existing wearables are not dirty.
void LLAgentWearables::setWearableOutfit(const LLInventoryItem::item_array_t& items,
										 const std::vector<LLViewerWearable*>& wearables,
										 bool remove)
{
	LL_DEBUGS("Wearables") << "setWearableOutfit() start" << LL_ENDL;

	S32 i;
	S32 count = wearables.size();
	if (count == 0) return;
	llassert((S32)items.size() == count);

	mIsSettingOutfit = true;

	// Keep track of all worn AT_BODYPART wearables that are to be replaced
	// with a new bodypart of the same type, so that we can remove them prior
	// to wearing the new ones (removing the replaced body parts must be done
	// to avoid seeing removed items still flagged as "worn" (label in bold)
	// in the inventory when they are copies of the newly worn body part (e.g.
	// when you have a copy of the same eyes in the newly worn outfit folder
	// as the eyes you were wearing before).
	// Also check for duplicate body parts (so to wear only one of each type),
	// scanning backwards to stay compatible with older viewers behaviour as
	// to which duplicate part will actually be worn in the end.
	std::set<LLWearableType::EType> new_bodyparts;
	std::set<S32> skip_wearable;
	bool changing_shape = false;
	for (i = count - 1; i >= 0; --i)
	{
		LLWearableType::EType type = wearables[i]->getType();
		if (LLWearableType::getAssetType(type) == LLAssetType::AT_BODYPART)
		{
			if (new_bodyparts.count(type) ||
//MK
				(gRLenabled && !gRLInterface.canUnwear(type)))
//mk
			{
				// Do not try and wear two body parts of the same type
				// or to replace a body part that RestrainedLove locked
				skip_wearable.insert(i);
			}
			else
			{
				// Keep track of what body part type is present in the new
				// outift we are going to wear
				new_bodyparts.insert(type);
				if (type == LLWearableType::WT_SHAPE)
				{
					changing_shape = true;
				}
			}
		}
	}

	// Before changing the shape, do reset all rigged meshes joint offsets
	if (changing_shape && isAgentAvatarValid())
	{
		gAgentAvatarp->clearAttachmentOverrides();
	}

	// When remove == true, this loop removes all clothing.
	// Note that removeWearable() will also take care of checking whether
	// the items can actually be removed (for teens and underwear, and for
	// RestrainedLove locked items).
	// It also always removes the body parts that will be replaced with the new
	// wearables.
	for (i = 0; i < (S32)LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		if (new_bodyparts.count(type) ||
			(remove &&
			 LLWearableType::getAssetType(type) == LLAssetType::AT_CLOTHING))
		{
			removeWearable(type, true, 0);
		}
	}

	bool no_multiple_physics = gSavedSettings.getBool("NoMultiplePhysics");
	bool no_multiple_shoes = gSavedSettings.getBool("NoMultipleShoes");
	bool no_multiple_skirts = gSavedSettings.getBool("NoMultipleSkirts");

	for (i = 0; i < count; ++i)
	{
		if (skip_wearable.count(i)) continue;

		LLViewerWearable* new_wearable = wearables[i];
		LLPointer<LLInventoryItem> new_item = items[i];

		llassert(new_wearable);
		if (new_wearable)
		{
			bool success = false;
			LLWearableType::EType type = new_wearable->getType();

			new_wearable->setName(new_item->getName());
			new_wearable->setItemID(new_item->getUUID());

			if (((no_multiple_physics && type == LLWearableType::WT_PHYSICS) ||
				 (no_multiple_shoes && type == LLWearableType::WT_SHOES) ||
				 (no_multiple_skirts && type == LLWearableType::WT_SKIRT) ||
				 LLWearableType::getAssetType(type) == LLAssetType::AT_BODYPART))
			{
				// exactly one wearable per body part, or per Physics, or per
				// cloth types which combination is confusing to the user
				success = setWearable(type, 0, new_wearable);
			}
			else if (!canAddWearable(type))
			{
				llwarns_once << "Attempted to wear more than "
							 << MAX_CLOTHING_LAYERS << " wearables" << llendl;
				continue;
			}
			else
			{
				success = pushWearable(type, new_wearable);
			}
			wearableUpdated(new_wearable, false);
//MK
			if (success && gRLenabled)
			{
				// Notify that this layer has been worn
				std::string layer = gRLInterface.getOutfitLayerAsString(type);
				gRLInterface.notify("worn legally " + layer);
			}
//mk
		}
	}

	mIsSettingOutfit = false;

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->setCompositeUpdatesEnabled(true);
		if (!gAgentAvatarp->getIsCloud())
		{
			// If we have not yet declouded, we may want to use baked texture
			// UUIDs sent from the first objectUpdate message don't overwrite
			// these. If we have already declouded, we've saved these ids as
			// the last known good textures and can invalidate without
			// re-clouding.
			gAgentAvatarp->invalidateAll();
		}
		// Copy wearable params to avatar.
		gAgentAvatarp->writeWearablesToAvatar();
		// Then update the avatar based on the copied params.
		gAgentAvatarp->updateVisualParams();		
		// After changing the shape, restore all rigged meshes joint offsets
		if (changing_shape)
		{
			gAgentAvatarp->rebuildAttachmentOverrides();
		}
	}

	// Start rendering & update the server
	mWearablesLoaded = true;
	queryWearableCache();
	updateServer();

	bool dump_outfit = false;
	LL_DEBUGS("Wearables") << "New outfit dump:";
	dump_outfit = isAgentAvatarValid();
	LL_CONT << LL_ENDL;
	if (dump_outfit)
	{
		gAgentAvatarp->dumpAvatarTEs("setWearableOutfit");
	}
}

// User has picked "wear" from a menu and this function gets called from the
// LLWearableBridge::onWearOnAvatarArrived() callback.
// NOTE: we used to check for a dirty old wearable here, when replacing the
// latter with a new one (do_append == false), but this implied setting up
// a notification callback within this function which is itself a callback,
// and very bad things were happening (among which crashes)... That check
// and also all checks related with RestrainedLove restrictions were therefore
// moved to LLAppearanceMgr::wearItemOnAvatar(). HB.
void LLAgentWearables::setWearableItem(LLInventoryItem* new_item,
									   LLViewerWearable* new_wearable,
									   bool do_append)
{
	if (isWearingItem(new_item->getUUID()))
	{
		llwarns << "Wearable " << new_item->getUUID() << " is already worn"
				<< llendl;
		return;
	}

	const LLWearableType::EType type = new_wearable->getType();

	if (!do_append)
	{
		// Check old wearable, if any. MULTI_WEARABLE: hardwired to 0
		LLViewerWearable* old_wearable = getViewerWearable(type, 0);
		if (old_wearable)
		{
			const LLUUID& old_item_id = old_wearable->getItemID();
			if (old_wearable->getAssetID() == new_wearable->getAssetID() &&
				old_item_id == new_item->getUUID())
			{
				LL_DEBUGS("Wearables") << "No change to wearable asset and item: "
									   << LLWearableType::getTypeName(type)
									   << LL_ENDL;
				return;
			}
		}
	}

	setWearableFinal(new_item, new_wearable, do_append);
}

// Called from setWearableItem() and onSetWearableDialog() to actually set the
// wearable.
// MULTI_WEARABLE: unify code after null objects are gone.
void LLAgentWearables::setWearableFinal(LLInventoryItem* new_item,
										LLViewerWearable* new_wearable,
										bool do_append)
{
	const LLWearableType::EType type = new_wearable->getType();

	// Before changing the shape, do reset all rigged meshes joint offsets
	bool reset_joints = type == LLWearableType::WT_SHAPE;
	if (reset_joints && isAgentAvatarValid())
	{
		gAgentAvatarp->clearAttachmentOverrides();
	}

	mIsSettingOutfit = true;

	if (type == LLWearableType::WT_SHAPE || type == LLWearableType::WT_SKIN ||
		type == LLWearableType::WT_HAIR || type == LLWearableType::WT_EYES)
	{
		// Can't wear more than one body part of each type
		do_append = false;
	}

	bool success = false;
	if (do_append && getWearableItemID(type, 0).notNull())
	{
		new_wearable->setItemID(new_item->getUUID());
		success = pushWearable(type, new_wearable, false);
		llinfos << "Added additional wearable for type " << type
				<< " size is now " << getWearableCount(type) << llendl;
		if (gFloaterCustomizep)
		{
			gFloaterCustomizep->updateWearableType(type, new_wearable);
		}
	}
	else
	{
		// Replace the old wearable with a new one.
		llassert(new_item->getAssetUUID() == new_wearable->getAssetID());

		LLViewerWearable* old_wearable = getViewerWearable(type, 0);
		LLUUID old_item_id;
		if (old_wearable)
		{
			old_item_id = old_wearable->getItemID();
		}
		new_wearable->setItemID(new_item->getUUID());
		success = setWearable(type, 0, new_wearable);

		if (old_item_id.notNull())
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
			gInventory.notifyObservers();
		}
		LL_DEBUGS("Wearables") << "Replaced current element 0 for type "
							   << type << " size is now "
							   << getWearableCount(type) << LL_ENDL;
	}

//MK
	if (success && gRLenabled)
	{
		// Notify that this layer has been worn
		std::string layer = gRLInterface.getOutfitLayerAsString(type);
		gRLInterface.notify("worn legally " + layer);
	}
//mk

	mIsSettingOutfit = false;

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->setCompositeUpdatesEnabled(true);
		// Copy wearable params to avatar.
		gAgentAvatarp->writeWearablesToAvatar();
		// Then update the avatar based on the copied params.
		gAgentAvatarp->updateVisualParams();
		if (!gAgentAvatarp->getIsCloud())
		{
			// If we have not yet declouded, we may want to use baked texture
			// UUIDs sent from the first objectUpdate message don't overwrite
			// these. If we have already declouded, we've saved these ids as
			// the last known good textures and can invalidate without
			// re-clouding.
			gAgentAvatarp->invalidateAll();
		}
		// After changing the shape, restore all rigged meshes joint offsets
		if (reset_joints)
		{
			gAgentAvatarp->rebuildAttachmentOverrides();
		}
	}

	queryWearableCache();
	updateServer();
}

void LLAgentWearables::queryWearableCache()
{
	if (!areWearablesLoaded() || LLVOAvatarSelf::canUseServerBaking())
	{
		return;
	}
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->setIsUsingServerBakes(false);
	}

	// Look up affected baked textures.
	// If they exist:
	//  - Disallow updates for affected layersets (until dataserver responds
	//    with cache request).
	//  - If cache miss, turn updates back on and invalidate composite.
	//  - If cache hit, modify baked texture entries.
	//
	// Cache requests contain list of hashes for each baked texture entry.
	// Response is list of valid baked texture assets. (same message)

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AgentCachedTexture);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addS32Fast(_PREHASH_SerialNum,
					gAgentQueryManager.mWearablesCacheQueryID);

	S32 num_queries = 0;
	for (U8 bake_idx = 0; bake_idx < gAgent.mUploadedBakes; ++bake_idx)
	{
		LLUUID hash_id =
			computeBakedTextureHash((EBakedTextureIndex)bake_idx);
		if (hash_id.notNull())
		{
			++num_queries;
			// *NOTE: make sure at least one request gets packed
			ETextureIndex te_index =
				LLAvatarAppearanceDictionary::bakedToLocalTextureIndex((EBakedTextureIndex)bake_idx);
			msg->nextBlockFast(_PREHASH_WearableData);
			msg->addUUIDFast(_PREHASH_ID, hash_id);
			msg->addU8Fast(_PREHASH_TextureIndex, (U8)te_index);
		}

		gAgentQueryManager.mActiveCacheQueries[bake_idx] =
			gAgentQueryManager.mWearablesCacheQueryID;
	}

	// VWR-22113: gAgent.getRegion() can return null if invalid, seen here on
	// logout
	if (gAgent.getRegion())
	{
		llinfos << "Requesting texture cache entry for " << num_queries
				<< " baked textures" << llendl;
		msg->sendReliable(gAgent.getRegionHost());
		++gAgentQueryManager.mNumPendingQueries;
		++gAgentQueryManager.mWearablesCacheQueryID;
	}
}

// virtual
void LLAgentWearables::invalidateBakedTextureHash(LLMD5& hash) const
{
	// Add some garbage into the hash so that it becomes invalid.
	if (isAgentAvatarValid())
	{
		hash.update((const unsigned char*)gAgentAvatarp->getID().mData,
					UUID_BYTES);
	}
}

// User has picked "remove from avatar" from a menu.
//static
void LLAgentWearables::userRemoveWearable(LLWearableType::EType type, U32 idx)
{
	if (type != LLWearableType::WT_SHAPE && type != LLWearableType::WT_SKIN &&
		type != LLWearableType::WT_HAIR && type != LLWearableType::WT_EYES)
	{
		gAgentWearables.removeWearable(type, false, idx);
	}
}

//static
void LLAgentWearables::userRemoveWearablesOfType(LLWearableType::EType type)
{
	if (type != LLWearableType::WT_SHAPE && type != LLWearableType::WT_SKIN &&
		type != LLWearableType::WT_HAIR && type != LLWearableType::WT_EYES)
	{
		gAgentWearables.removeWearable(type, true, 0);
	}
}

void LLAgentWearables::userRemoveAllClothes()
{
	// We have to do this up front to avoid having to deal with the case of
	// multiple wearables being dirty.
	if (gFloaterCustomizep)
	{
		gFloaterCustomizep->askToSaveIfDirty(userRemoveAllClothesStep2, NULL);
	}
	else
	{
		userRemoveAllClothesStep2(true, NULL);
	}
}

void LLAgentWearables::userRemoveAllClothesStep2(bool proceed, void*)
{
	if (proceed)
	{
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_SHIRT);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_PANTS);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_SHOES);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_SOCKS);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_JACKET);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_GLOVES);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_UNDERSHIRT);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_UNDERPANTS);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_SKIRT);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_ALPHA);
		gAgentWearables.userRemoveWearablesOfType(LLWearableType::WT_TATTOO);
	}
}

void LLAgentWearables::userRemoveMultipleAttachments(llvo_vec_t& objects_to_remove)
{
	if (!isAgentAvatarValid()) return;

	if (objects_to_remove.empty())
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("ObjectDetach");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	for (llvo_vec_t::iterator it = objects_to_remove.begin(),
							  end = objects_to_remove.end();
		 it != end; ++it)
	{
		LLViewerObject* objectp = *it;
//MK
		if (gRLenabled && !gRLInterface.canDetach(objectp))
		{
			continue;
		}
//mk
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
	}
	msg->sendReliable(gAgent.getRegionHost());
}

void LLAgentWearables::userRemoveAllAttachments(bool only_temp_attach)
{
	if (!isAgentAvatarValid()) return;

	llvo_vec_t objects_to_remove;

	for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerObject* object =
			gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (!object) continue;	// Paranoia

		if (!only_temp_attach ||
			(only_temp_attach && object->isTempAttachment()))
		{
			objects_to_remove.push_back(object);
		}
	}
	userRemoveMultipleAttachments(objects_to_remove);
}

void LLAgentWearables::userAttachMultipleAttachments(LLInventoryModel::item_array_t& objects)
{
	if (!gAgent.getRegion()) return;

	// Build a compound message to send all the objects that need to be rezzed.
	S32 obj_count = objects.size();

	// Limit number of packets to send
	constexpr S32 MAX_PACKETS_TO_SEND = 10;
	constexpr S32 OBJECTS_PER_PACKET = 4;
	constexpr S32 MAX_OBJECTS_TO_SEND = MAX_PACKETS_TO_SEND *
										OBJECTS_PER_PACKET;
	if (obj_count > MAX_OBJECTS_TO_SEND)
	{
		obj_count = MAX_OBJECTS_TO_SEND;
	}

	// Create an id to keep the parts of the compound message together
	LLUUID compound_msg_id;
	compound_msg_id.generate();
	LLMessageSystem* msg = gMessageSystemp;

	for (S32 i = 0; i < obj_count; ++i)
	{
		if (i % OBJECTS_PER_PACKET == 0)
		{
			// Start a new message chunk
			msg->newMessageFast(_PREHASH_RezMultipleAttachmentsFromInv);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_HeaderData);
			msg->addUUIDFast(_PREHASH_CompoundMsgID, compound_msg_id);
			msg->addU8Fast(_PREHASH_TotalObjects, obj_count);
			msg->addBoolFast(_PREHASH_FirstDetachAll, false);
		}

		const LLInventoryItem* item = objects[i].get();
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addUUIDFast(_PREHASH_ItemID, item->getLinkedUUID());
		msg->addUUIDFast(_PREHASH_OwnerID, item->getPermissions().getOwner());
		// Wear at the previous or default attachment point:
		msg->addU8Fast(_PREHASH_AttachmentPt, 0 | ATTACHMENT_ADD);
		pack_permissions_slam(msg, item->getFlags(), item->getPermissions());
		msg->addStringFast(_PREHASH_Name, item->getName());
		msg->addStringFast(_PREHASH_Description, item->getDescription());

		if (obj_count == i + 1 ||
			i % OBJECTS_PER_PACKET == OBJECTS_PER_PACKET - 1)
		{
			// End of message chunk
			msg->sendReliable(gAgent.getRegionHost());
		}
	}
}

// Returns false if the given wearable is already topmost/bottommost
// (depending on closer_to_body parameter).
bool LLAgentWearables::canMoveWearable(const LLUUID& item_id,
									   bool closer_to_body) const
{
	const LLWearable* wearable = getWearableFromItemID(item_id);
	if (!wearable) return false;

	LLWearableType::EType wtype = wearable->getType();
	const LLWearable* marginal_wearable = closer_to_body ? getBottomWearable(wtype)
														 : getTopWearable(wtype);

	return marginal_wearable && wearable != marginal_wearable;
}

bool LLAgentWearables::areWearablesLoaded() const
{
	return mWearablesLoaded;
}

// MULTI-WEARABLE: DEPRECATED: item pending count relies on old messages that
// don't support multi-wearables. do not trust to be accurate
void LLAgentWearables::updateWearablesLoaded()
{
	mWearablesLoaded = true;
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		if (getWearableItemID((LLWearableType::EType)i, 0).notNull() &&
			!getViewerWearable((LLWearableType::EType)i, 0))
		{
			mWearablesLoaded = false;
			break;
		}
	}
	LL_DEBUGS("Wearables") << "mWearablesLoaded = " << mWearablesLoaded
						   << LL_ENDL;
}

bool LLAgentWearables::canWearableBeRemoved(const LLViewerWearable* wearable) const
{
	if (!wearable) return false;

	LLWearableType::EType type = wearable->getType();
//MK
	if (gRLenabled && !gRLInterface.canUnwear(type))
	{
		return false;
	}
//mk
	// Make sure the user always has at least one shape, skin, eyes, and hair
	// type currently worn.
	return getWearableCount(type) > 1 ||
		   (type != LLWearableType::WT_SHAPE &&
			type != LLWearableType::WT_SKIN &&
			type != LLWearableType::WT_HAIR &&
			type != LLWearableType::WT_EYES);
}

void LLAgentWearables::animateAllWearableParams(F32 delta, bool upload_bake)
{
	for (S32 type = 0; type < LLWearableType::WT_COUNT; ++type)
	{
		for (S32 count = 0,
				 total = getWearableCount((LLWearableType::EType)type);
			 count < total; ++count)
		{
			LLViewerWearable* wearable =
				getViewerWearable((LLWearableType::EType)type, count);
			if (wearable)
			{
				wearable->animateParams(delta, upload_bake);
			}
		}
	}
}

void LLAgentWearables::updateServer()
{
	sendAgentWearablesUpdate();
	gAgent.sendAgentSetAppearance();
	gInventory.notifyObservers();

	// Ensure the new outfit will be saved
	gWearablesListDirty = true;

	// Notify the Make new outfit floater, if opened
	HBFloaterMakeNewOutfit::setDirty();
}
