/**
 * @file llfloaterbulkpermissions.cpp
 * @author Michelle2 Zenovka
 * @brief A floater which allows task inventory item's properties to be changed on mass.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include "llfloaterbulkpermission.h"

#include "lldir.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterperms.h"			// For utilities
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "roles_constants.h"		// For GP_OBJECT_MANIPULATE

LLFloaterBulkPermission::LLFloaterBulkPermission(const LLSD& seed)
:	mDone(false)
{
	mID.generate();
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_bulk_perms.xml");
	childSetEnabled("next_owner_transfer",
					gSavedSettings.getBool("BulkChangeNextOwnerCopy"));
	if (!gAgent.hasInventoryMaterial())
	{
		gSavedSettings.setBool("BulkChangeIncludeMaterials", false);
		childSetEnabled("check_material", false);
		std::string tooltip = getString("no_material_cap");
		childSetToolTip("icon_material", tooltip);
		childSetToolTip("check_material", tooltip);
	}
	childSetAction("help", onHelpBtn, this);
	childSetAction("apply", onApplyBtn, this);
	childSetAction("close", onCloseBtn, this);
	childSetAction("check_all", onCheckAll, this);
	childSetAction("check_none", onUncheckAll, this);
	childSetCommitCallback("next_owner_copy", &onCommitCopy, this);
}

void LLFloaterBulkPermission::doApply()
{
	// Inspects a stream of selected object contents and adds modifiable ones
	// to the given array.
	class ModifiableGatherer final : public LLSelectedNodeFunctor
	{
	public:
		ModifiableGatherer(uuid_vec_t& q)
		:	mQueue(q)
		{
		}

		bool apply(LLSelectNode* node) override
		{
			if (node->allowOperationOnNode(PERM_MODIFY, GP_OBJECT_MANIPULATE))
			{
				mQueue.emplace_back(node->getObject()->getID());
			}
			return true;
		}

	private:
		uuid_vec_t& mQueue;
	};

	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");
	list->deleteAllItems();
	ModifiableGatherer gatherer(mObjectIDs);
	gSelectMgr.getSelection()->applyToNodes(&gatherer);
	if (mObjectIDs.empty())
	{
		list->addCommentText(getString("nothing_to_modify_text"));
	}
	else
	{
		mDone = false;
		if (!start())
		{
			llwarns << "Unexpected bulk permission change failure." << llendl;
		}
	}
}

// This is the callback method for the viewer object currently being worked on.
//virtual
void LLFloaterBulkPermission::inventoryChanged(LLViewerObject* viewer_object,
											   LLInventoryObject::object_list_t* inv,
											   S32, void* q_id)
{
	// Remove this listener from the object since its listener callback is now
	// being executed.
	// We remove the listener here because the removeVOInventoryListener()
	// method removes the listener from a LLViewerObject which it internally
	// stores.
	// If we call this further down in the method, calls to handleInventory and
	// nextObject may update the interally stored viewer object causing the
	// removal of the incorrect listener from an incorrect object.
	// Fixes SL-6119: recompile scripts fails to complete
	removeVOInventoryListener();

	if (viewer_object && inv && (viewer_object->getID() == mCurrentObjectID))
	{
		handleInventory(viewer_object, inv);
	}
	else
	{
		// Something went wrong... Note that we are not working on this one,
		// and move onto the next object in the list.
		llwarns << "No inventory for " << mCurrentObjectID << llendl;
		nextObject();
	}
}

void LLFloaterBulkPermission::onApplyBtn(void* user_data)
{
	LLFloaterBulkPermission* self = (LLFloaterBulkPermission*)user_data;
	self->doApply();
}

void LLFloaterBulkPermission::onHelpBtn(void* user_data)
{
	gNotifications.add("HelpBulkPermission");
}

void LLFloaterBulkPermission::onCloseBtn(void* user_data)
{
	LLFloaterBulkPermission* self = (LLFloaterBulkPermission*)user_data;
	self->onClose(false);
}

//static
void LLFloaterBulkPermission::onCommitCopy(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterBulkPermission* self = (LLFloaterBulkPermission*)user_data;
	// Implements fair use
	bool copyable = gSavedSettings.getBool("BulkChangeNextOwnerCopy");
	if (!copyable)
	{
		gSavedSettings.setBool("BulkChangeNextOwnerTransfer", true);
	}
	LLCheckBoxCtrl* xfer = self->getChild<LLCheckBoxCtrl>("next_owner_transfer");
	xfer->setEnabled(copyable);
}

bool LLFloaterBulkPermission::start()
{
	// Note: number of top-level objects to modify is mObjectIDs.size().
	getChild<LLScrollListCtrl>("queue output")->addCommentText(getString("start_text"));
	return nextObject();
}

// Go to the next object and start if found. Returns false if no objects left,
// true otherwise.
bool LLFloaterBulkPermission::nextObject()
{
	bool successful_start = false;

	do
	{
		S32 count = mObjectIDs.size();
		mCurrentObjectID.setNull();
		if (count > 0)
		{
			successful_start = popNext();
		}
	}
	while (mObjectIDs.size() > 0 && !successful_start);

	if (isDone() && !mDone)
	{
		getChild<LLScrollListCtrl>("queue output")->addCommentText(getString("done_text"));
		mDone = true;
	}

	return successful_start;
}

// Pops the top object off of the queue. Returns true if the queue has started,
// otherwise false.
bool LLFloaterBulkPermission::popNext()
{
	// Get the head element from the container, and attempt to get its
	// inventory.
	bool rv = false;
	S32 count = mObjectIDs.size();
	if (mCurrentObjectID.isNull() && count > 0)
	{
		mCurrentObjectID = mObjectIDs[0];
		mObjectIDs.erase(mObjectIDs.begin());
		LLViewerObject* obj = gObjectList.findObject(mCurrentObjectID);
		if (obj)
		{
			LLUUID* id = new LLUUID(mID);
			registerVOInventoryListener(obj, id);
			requestVOInventory();
			rv = true;
		}
		else
		{
			llwarns << "NULL LLViewerObject" <<llendl;
		}
	}

	return rv;
}

//static
void LLFloaterBulkPermission::setAllChecked(bool check)
{
	gSavedSettings.setBool("BulkChangeIncludeAnimations", check);
	gSavedSettings.setBool("BulkChangeIncludeBodyParts", check);
	gSavedSettings.setBool("BulkChangeIncludeClothing", check);
	gSavedSettings.setBool("BulkChangeIncludeGestures", check);
	gSavedSettings.setBool("BulkChangeIncludeLandmarks", check);
	bool has_cap = gAgent.hasInventoryMaterial();
	gSavedSettings.setBool("BulkChangeIncludeMaterials", check && has_cap);
	gSavedSettings.setBool("BulkChangeIncludeNotecards", check);
	gSavedSettings.setBool("BulkChangeIncludeObjects", check);
	gSavedSettings.setBool("BulkChangeIncludeScripts", check);
	gSavedSettings.setBool("BulkChangeIncludeSettings", check);
	gSavedSettings.setBool("BulkChangeIncludeSounds", check);
	gSavedSettings.setBool("BulkChangeIncludeTextures", check);
}

void LLFloaterBulkPermission::handleInventory(LLViewerObject* viewer_obj,
											  LLInventoryObject::object_list_t* inv)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");

	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for (; it != end; ++it)
	{
		LLAssetType::EType asstype = (*it)->getType();
		if ((asstype == LLAssetType::AT_ANIMATION &&
			 gSavedSettings.getBool("BulkChangeIncludeAnimations")) ||
			(asstype == LLAssetType::AT_BODYPART &&
			 gSavedSettings.getBool("BulkChangeIncludeBodyParts")) ||
			(asstype == LLAssetType::AT_CLOTHING &&
			 gSavedSettings.getBool("BulkChangeIncludeClothing")) ||
			(asstype == LLAssetType::AT_GESTURE &&
			 gSavedSettings.getBool("BulkChangeIncludeGestures")) ||
			(asstype == LLAssetType::AT_LANDMARK &&
			 gSavedSettings.getBool("BulkChangeIncludeLandmarks")) ||
			(asstype == LLAssetType::AT_MATERIAL &&
			 gSavedSettings.getBool("BulkChangeIncludeMaterials")) ||
			(asstype == LLAssetType::AT_NOTECARD &&
			 gSavedSettings.getBool("BulkChangeIncludeNotecards")) ||
			(asstype == LLAssetType::AT_OBJECT &&
			 gSavedSettings.getBool("BulkChangeIncludeObjects")) ||
			(asstype == LLAssetType::AT_LSL_TEXT &&
			 gSavedSettings.getBool("BulkChangeIncludeScripts")) ||
			(asstype == LLAssetType::AT_SOUND &&
			 gSavedSettings.getBool("BulkChangeIncludeSounds")) ||
			(asstype == LLAssetType::AT_SETTINGS &&
			 gSavedSettings.getBool("BulkChangeIncludeSettings")) ||
			(asstype == LLAssetType::AT_TEXTURE &&
			 gSavedSettings.getBool("BulkChangeIncludeTextures")))
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item =
					(LLInventoryItem*)((LLInventoryObject*)(*it));
				LLViewerInventoryItem* new_item = (LLViewerInventoryItem*)item;
				LLPermissions perm(new_item->getPermissions());
				U32 flags = new_item->getFlags();

				U32 desired_next_owner_perms =
					LLFloaterPerms::getNextOwnerPerms("BulkChange");
				if (asstype == LLAssetType::AT_SETTINGS)
				{
					desired_next_owner_perms |= PERM_COPY;
				}
				U32 desired_everyone_perms =
					LLFloaterPerms::getEveryonePerms("BulkChange");
				U32 desired_group_perms =
					LLFloaterPerms::getGroupPerms("BulkChange");

				// If next owner permissions have changed (and this is an object)
				// then set the slam permissions flag so that they are applied on rez.
				if ((perm.getMaskNextOwner() != desired_next_owner_perms)
				   && (new_item->getType() == LLAssetType::AT_OBJECT))
				{
					flags |= LLInventoryItem::II_FLAGS_OBJECT_SLAM_PERM;
				}
				// If everyone permissions have changed (and this is an object)
				// then set the overwrite everyone permissions flag so they
				// are applied on rez.
				if ((perm.getMaskEveryone() != desired_everyone_perms)
				    && (new_item->getType() == LLAssetType::AT_OBJECT))
				{
					flags |= LLInventoryItem::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
				}
				// If group permissions have changed (and this is an object)
				// then set the overwrite group permissions flag so they
				// are applied on rez.
				if ((perm.getMaskGroup() != desired_group_perms)
				    && (new_item->getType() == LLAssetType::AT_OBJECT))
				{
					flags |= LLInventoryItem::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
				}

				// Chomp the inventory name so it fits in the scroll window
				// nicely and the user can see the [OK]
				std::string invname = item->getName();
				if (invname.size() > 30)
				{
					invname = invname.substr(0, 30);
				}

				LLUIString status_text = getString("status_text");
				status_text.setArg("[NAME]", invname.c_str());

				// Check whether we appear to have the appropriate permissions
				// to change permission on this item. Although the server will
				// disallow any forbidden changes, it is a good idea to guess
				// correctly so that we can warn the user. The risk of getting
				// this check wrong is therefore the possibility of incorrectly
				// choosing to not attempt to make a valid change.

#if 0			// Trouble is this is extremely difficult to do and even when
				// we know the results it is difficult to design the best
				// messaging. Therefore in this initial implementation we will
				// always try to set the requested permissions and consider all
				// cases successful and perhaps later try to implement a
				// smarter, friendlier solution. -MG
				// for group and everyone masks
				if (!gAgent.allowOperation(PERM_MODIFY, perm, GP_OBJECT_MANIPULATE))
				//  && something else // for next owner perms
				{
					//status_text.setArg("[STATUS]", getString("status_bad_text"));
					status_text.setArg("[STATUS]", "");
				}
				else
#endif
				{
					perm.setMaskNext(desired_next_owner_perms);
					perm.setMaskEveryone(desired_everyone_perms);
					perm.setMaskGroup(desired_group_perms);
					new_item->setPermissions(perm); // here's the beef
					new_item->setFlags(flags); // and the tofu
					updateInventory(object, new_item);
					//status_text.setArg("[STATUS]", getString("status_ok_text"));
					status_text.setArg("[STATUS]", "");
				}

				list->addCommentText(status_text.getString());

#if 0			// *TODO if we are an object inside an object we should check a
				// recuse flag and if set open the inventory of the object and
				// recurse - Michelle2 Zenovka
				if (recurse && processObject &&
					(*it)->getType() == LLAssetType::AT_OBJECT)
				{
					// I think we need to get the UUID of the object inside the
					// inventory call item->fetchFromServer(); we need a
					// callback to say item has arrived *sigh* we then need to
					// do something like:
					//	LLUUID* id = new LLUUID(mID);
					//	registerVOInventoryListener(obj, id);
					//	requestVOInventory();
				}
#endif
			}
		}
	}

	nextObject();
}

// Avoid inventory callbacks etc by just fire and forgetting the message with
// the permissions update we could do this via LLViewerObject::updateInventory
// but that uses inventory call backs and buggers us up and we would have a
// dodgy item iterator
void LLFloaterBulkPermission::updateInventory(LLViewerObject* object,
											  LLViewerInventoryItem* item)
{
	// This slices the object into what we're concerned about on the viewer.
	// The simulator will take the permissions and transfer ownership.
	LLPointer<LLViewerInventoryItem> task_item =
		new LLViewerInventoryItem(item->getUUID(), mID, item->getPermissions(),
								  item->getAssetUUID(), item->getType(),
								  item->getInventoryType(), item->getName(),
								  item->getDescription(), item->getSaleInfo(),
								  item->getFlags(), item->getCreationDate());
	task_item->setTransactionID(item->getTransactionID());
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_UpdateTaskInventory);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_UpdateData);
	msg->addU32Fast(_PREHASH_LocalID, object->mLocalID);
	msg->addU8Fast(_PREHASH_Key, TASK_INVENTORY_ITEM_KEY);
	msg->nextBlockFast(_PREHASH_InventoryData);
	task_item->packMessage(msg);
	msg->sendReliable(object->getRegion()->getHost());
}
