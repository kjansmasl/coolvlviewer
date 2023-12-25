/**
 * @file llfloaterinspect.cpp
 * @brief Implementation of floaters for object and avatar inspection tool
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc, 2009-2023 Henri Beauchamp.
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

#include "llfloaterinspect.h"

#include "llbutton.h"
#include "llcachename.h"
#include "lldate.h"
#include "lliconctrl.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llcommandhandler.h"
#include "llfloateravatarinfo.h"
#include "llfloaterobjectweights.h"
#include "llfloatertools.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llvoavatar.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"

#define COMMENT_PREFIX "\342\200\243 "

// Command handler

// Handlea secondlife:///app/object/<ID>/inspect SLURLs
class LLInspectObjectHandler final : public LLCommandHandler
{
public:
	LLInspectObjectHandler() : LLCommandHandler("object", UNTRUSTED_BLOCK) {}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		if (params.size() < 2) return false;

		LLUUID object_id;
		if (!object_id.set(params[0], false))
		{
			return false;
		}

		const std::string verb = params[1].asString();
		if (verb == "inspect")
		{
			LLViewerObject* object = gObjectList.findObject(object_id);
			if (object)
			{
				LLFloaterInspect::show(object);
				return true;
			}
		}

		return false;
	}
};

LLInspectObjectHandler gInspectObjectHandler;

// LLFloaterInspect class

LLFloaterInspect::LLFloaterInspect(const LLSD&)
:	mDirty(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inspect.xml");
}

//virtual
LLFloaterInspect::~LLFloaterInspect()
{
	if (!LLFloaterTools::isVisible())
	{
		if (gToolMgr.getBaseTool() == &gToolCompInspect)
		{
			gToolMgr.clearTransientTool();
		}
		// Switch back to basic toolset
		gToolMgr.setCurrentToolset(gBasicToolset);
	}
	else
	{
		gFloaterToolsp->setFocus(true);
	}
}

//virtual
bool LLFloaterInspect::postBuild()
{
	mObjectList = getChild<LLScrollListCtrl>("object_list");
	mObjectList->setCommitCallback(onSelectObject);
	mObjectList->setCallbackUserData(this);

	mButtonOwner = getChild<LLButton>("button owner");
	mButtonOwner->setClickedCallback(onClickOwnerProfile, this);

	mButtonCreator = getChild<LLButton>("button creator");
	mButtonCreator->setClickedCallback(onClickCreatorProfile, this);

	mButtonWeights = getChild<LLButton>("button weights");
	mButtonWeights->setClickedCallback(onClickWeights, this);

	mIconNavMeshInfo = getChild<LLIconCtrl>("nav_mesh_info");
	mNavMeshToolTip = mIconNavMeshInfo->getToolTip();

	childSetAction("refresh", onClickRefresh, this);
	childSetAction("close", onClickClose, this);

	return true;
}

//virtual
void LLFloaterInspect::draw()
{
	if (mDirty)
	{
		mDirty = false;
		refresh();
	}

	LLFloater::draw();
}

//virtual
void LLFloaterInspect::refresh()
{
	LLUUID creator_id;
	std::string creator_name;
	S32 pos = mObjectList->getScrollPos();
	mButtonOwner->setEnabled(false);
	mButtonCreator->setEnabled(false);
	mButtonWeights->setEnabled(false);
	LLUUID selected_uuid;
	S32 selected_index = mObjectList->getFirstSelectedIndex();
	if (selected_index > -1)
	{
		LLScrollListItem* item = mObjectList->getFirstSelected();
		if (item)
		{
			selected_uuid = item->getUUID();
		}
	}
	mObjectList->deleteAllItems();

	const std::string& loadingstr = getString("loading");

	bool loading = false;
	S32 total_scripts = 0;
	const std::string format = gSavedSettings.getString("TimestampFormat");
	// List all transient objects, then all linked objects
	for (LLObjectSelection::valid_iterator
			iter = mObjectSelection->valid_begin(),
			end = mObjectSelection->valid_end();
		 iter != end; ++iter)
	{
		LLSelectNode* obj = *iter;
		if (!obj || obj->mCreationDate == 0)
		{
			// Do not have valid information from the server, so skip this one
			continue;
		}

		LLViewerObject* vobj = obj->getObject();
		if (!vobj || vobj->isDead())
		{
			// Object gone or soon gone !
			continue;
		}

		time_t timestamp = (time_t)(obj->mCreationDate / 1000000);
		std::string owner_name, creator_name, last_owner_name;
		if (gCacheNamep)
		{
			gCacheNamep->getFullName(obj->mPermissions->getOwner(),
									 owner_name);
			gCacheNamep->getFullName(obj->mPermissions->getCreator(),
									 creator_name);
			gCacheNamep->getFullName(obj->mPermissions->getLastOwner(),
									 last_owner_name);
		}

		LLUUID id = vobj->getID();
		S32 scripts = -1;
		S32 total = -1;
		invcounts_map_t::iterator itr = mInventoryNums.find(id);
		if (itr != mInventoryNums.end())
		{
			scripts = itr->second.first;
			total_scripts += scripts;
			total = itr->second.second;
		}
		else
		{
			requestInventory(vobj);
			loading = true;
		}
		LLSD row;
		row["id"] = id;
		row["columns"][0]["column"] = "object_name";
		row["columns"][0]["type"] = "text";
		// Make sure we are either at the top of the link chain
		// or top of the editable chain, for attachments
		if (vobj->isRoot() || vobj->isRootEdit())
		{
			row["columns"][0]["value"] = obj->mName;
			row["columns"][0]["font-style"] = "BOLD";
		}
		else
		{
			row["columns"][0]["value"] = std::string("   ") + obj->mName;
		}
		row["columns"][1]["column"] = "owner_name";
		row["columns"][1]["type"] = "text";
		row["columns"][1]["value"] = owner_name;
		row["columns"][2]["column"] = "last_owner_name";
		row["columns"][2]["type"] = "text";
		row["columns"][2]["value"] = last_owner_name;
		row["columns"][3]["column"] = "creator_name";
		row["columns"][3]["type"] = "text";
		row["columns"][3]["value"] = creator_name;
		row["columns"][4]["column"] = "creation_date";
		row["columns"][4]["type"] = "date";
		row["columns"][4]["format"] = format;
		row["columns"][4]["value"] = LLDate(timestamp);
		row["columns"][5]["column"] = "inventory";
		row["columns"][5]["type"] = "text";
		row["columns"][5]["value"] = (total < 0 ? loadingstr
												: llformat("%d/%d", scripts, total));
		mObjectList->addElement(row, ADD_TOP);
	}

	std::string comment = COMMENT_PREFIX + getString("total_scripts") +
						  llformat(" %d", total_scripts);
	if (loading)
	{
		comment += " " + getString("so_far");
	}
	mObjectList->addCommentText(comment);

	if (selected_index > -1 &&
		mObjectList->getItemIndex(selected_uuid) == selected_index)
	{
		mObjectList->selectNthItem(selected_index);
	}
	else
	{
		mObjectList->selectNthItem(0);
	}
	onSelectObject(mObjectList, this);
	mObjectList->setScrollPos(pos);

	// Navmesh/pathfinding attribute(s)
	std::string pf_info = gSelectMgr.getPathFindingAttributeInfo(true);
	bool show_icon = !pf_info.empty();
	if (show_icon)
	{
		mIconNavMeshInfo->setToolTip(mNavMeshToolTip + " " + pf_info);
	}
	mIconNavMeshInfo->setVisible(show_icon);
}

void LLFloaterInspect::onFocusReceived()
{
	gToolMgr.setTransientTool(&gToolCompInspect);
	LLFloater::onFocusReceived();
}

void LLFloaterInspect::requestInventory(LLViewerObject* vobj)
{
	if (vobj && !hasRegisteredListener(vobj))
	{
		registerVOInventoryListener(vobj, NULL);
		requestVOInventory(vobj);
	}
}

void LLFloaterInspect::inventoryChanged(LLViewerObject* vobj,
										LLInventoryObject::object_list_t* inv,
										S32, void*)
{
	if (!vobj || !inv)
	{
		return;
	}
	removeVOInventoryListener(vobj);
	const LLUUID id = vobj->getID();
	S32 scripts = 0, total = 0;
	for (LLInventoryObject::object_list_t::const_iterator
			it = inv->begin(), end = inv->end();
		 it != end; ++it)
	{
		LLAssetType::EType type = (*it)->getType();
		if (type == LLAssetType::AT_LSL_TEXT ||
			type == LLAssetType::AT_SCRIPT)			// Legacy scripts
		{
			++scripts;
		}
		if (type != LLAssetType::AT_LSL_BYTECODE &&	// Do not count the bytecode associated with AT_LSL_TEXT
			type != LLAssetType::AT_CATEGORY &&		// Do not count folders
			type != LLAssetType::AT_NONE)			// There's one such unknown item per prim...
		{
			++total;
		}
	}
	mInventoryNums[id] = std::make_pair(scripts, total);
	mDirty = true;
}

//static
void LLFloaterInspect::dirty()
{
	LLFloaterInspect* self = findInstance();
	if (self)
	{
		self->removeVOInventoryListeners();
		self->mInventoryNums.clear();
		self->mDirty = true;
	}
}

//static
LLUUID LLFloaterInspect::getSelectedUUID()
{
	LLFloaterInspect* self = findInstance();
	if (self)
	{
		if (self->mObjectList->getAllSelected().size() > 0)
		{
			LLScrollListItem* item = self->mObjectList->getFirstSelected();
			if (item)
			{
				return item->getUUID();
			}
		}
	}
	return LLUUID::null;
}

//static
void LLFloaterInspect::show(void* data)
{
	if (data)
	{
		gSelectMgr.selectObjectAndFamily((LLViewerObject*)data);
	}

	// Ensure that the pie menu does not deselect things when it looses the
	// focus (this can happen with "select own objects only" is enabled).
	bool forcesel = gSelectMgr.setForceSelection(true);

	LLFloaterInspect* self = getInstance();

	self->open();
	gToolMgr.setTransientTool(&gToolCompInspect);
	gSelectMgr.setForceSelection(forcesel);	// Restore previous value

	self->mObjectSelection = gSelectMgr.getSelection();
	self->refresh();
}

//static
void LLFloaterInspect::onClickCreatorProfile(void* data)
{
	LLFloaterInspect* self = (LLFloaterInspect*)data;
	if (!self || self->mObjectList->getAllSelected().size() == 0)
	{
		return;
	}

	LLScrollListItem* item = self->mObjectList->getFirstSelected();
	if (item)
	{
		struct f final : public LLSelectedNodeFunctor
		{
			LLUUID obj_id;
			f(const LLUUID& id) : obj_id(id) {}
			bool apply(LLSelectNode* node) override
			{
				return (obj_id == node->getObject()->getID());
			}
		} func(item->getUUID());

		LLSelectNode* node = self->mObjectSelection->getFirstNode(&func);
		if (node)
		{
			LLFloaterAvatarInfo::showFromDirectory(node->mPermissions->getCreator());
		}
	}
}

//static
void LLFloaterInspect::onClickOwnerProfile(void* data)
{
	LLFloaterInspect* self = (LLFloaterInspect*)data;
	if (!self || self->mObjectList->getAllSelected().size() == 0)
	{
		return;
	}

	LLScrollListItem* item = self->mObjectList->getFirstSelected();
	if (item)
	{
		LLUUID selected_id = item->getUUID();
		struct f final : public LLSelectedNodeFunctor
		{
			LLUUID obj_id;
			f(const LLUUID& id) : obj_id(id) {}
			bool apply(LLSelectNode* node) override
			{
				return (obj_id == node->getObject()->getID());
			}
		} func(selected_id);

		LLSelectNode* node = self->mObjectSelection->getFirstNode(&func);
		if (node)
		{
			const LLUUID& owner_id = node->mPermissions->getOwner();
			LLFloaterAvatarInfo::showFromDirectory(owner_id);
		}
	}
}

//static
void LLFloaterInspect::onClickWeights(void* data)
{
	LLFloaterInspect* self = (LLFloaterInspect*)data;
	if (self)
	{
		LLFloaterObjectWeights::show(self);
	}
}

//static
void LLFloaterInspect::onClickRefresh(void* data)
{
	LLFloaterInspect* self = (LLFloaterInspect*)data;
	if (self)
	{
		self->mInventoryNums.clear();
		dirty();
	}
}

//static
void LLFloaterInspect::onClickClose(void* data)
{
	LLFloaterInspect* self = (LLFloaterInspect*)data;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterInspect::onSelectObject(LLUICtrl*, void* data)
{
	LLFloaterInspect* self = (LLFloaterInspect*)data;
	if (self && getSelectedUUID().notNull())
	{
		self->mButtonOwner->setEnabled(true);
		self->mButtonCreator->setEnabled(true);
		self->mButtonWeights->setEnabled(true);
	}
}

// HBFloaterInspectAvatar class

HBFloaterInspectAvatar::HBFloaterInspectAvatar(const LLSD&)
:	mDirty(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_inspect_avatar.xml");
}

//virtual
bool HBFloaterInspectAvatar::postBuild()
{
	mObjectList = getChild<LLScrollListCtrl>("object_list");
	mObjectList->setDoubleClickCallback(onDoubleClickObject);
	mObjectList->setCallbackUserData(this);

	childSetAction("profile", onClickProfile, this);
	childSetAction("refresh", onClickRefresh, this);
	childSetAction("close", onClickClose, this);

	mTitle = getTitle();

	return true;
}

//virtual
void HBFloaterInspectAvatar::draw()
{
	if (mDirty)
	{
		mDirty = false;
		refresh();
	}

	LLFloater::draw();
}

//virtual
void HBFloaterInspectAvatar::refresh()
{
	S32 pos = mObjectList->getScrollPos();
	LLUUID selected_uuid;
	S32 selected_index = mObjectList->getFirstSelectedIndex();
	if (selected_index > -1)
	{
		LLScrollListItem* item = mObjectList->getFirstSelected();
		if (item)
		{
			selected_uuid = item->getUUID();
		}
	}
	mObjectList->deleteAllItems();

	LLVOAvatar* avatar = gObjectList.findAvatar(mAvatarID);
	if (!avatar)
	{
		setTitle(mTitle);
		mObjectList->addCommentText(getString("no_avatar"));
		removeVOInventoryListeners();
		mScriptCounts.clear();
		return;
	}
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames || gRLInterface.mContainsShowNearby ||
		 gRLInterface.mContainsShownametags))
	{
		setTitle(mTitle);
	}
	else
//mk
	{
		std::string avatar_name;
		if (gCacheNamep && gCacheNamep->getFullName(mAvatarID, avatar_name))
		{
			setTitle(mTitle +  ": " + avatar_name);
		}
	}

	const std::string& loadingstr = getString("loading");

	bool loading = false;
	S32 attachments = 0;
	S32 total_primitives = 0;
	S32 total_scripts = 0;
	// List all attachements
	for (S32 i = 0, count = avatar->mAttachedObjectsVector.size(); i < count;
		 ++i)
	{
		LLViewerObject* vobj = avatar->mAttachedObjectsVector[i].first;
		LLViewerJointAttachment* vatt = avatar->mAttachedObjectsVector[i].second;
		if (!vatt || !vatt->getValid() || !vobj || vobj->isDead())
		{
			continue;
		}
		const std::string& joint_name = vatt->getName();
		++attachments;
		const LLUUID& id = vobj->getID();

		S32 primitives = 1;
		S32 scripts = -1; // Value for "loading"
		scriptcounts_map_t::iterator itr = mScriptCounts.find(id);
		if (itr == mScriptCounts.end())
		{
			loading = true;
			requestInventory(vobj);
		}
		else
		{
			scripts = itr->second;
			total_scripts += scripts;
		}

		// We must also get the child primitives info...
		LLViewerObject::child_list_t child_list = vobj->getChildren();
		for (LLViewerObject::child_list_t::iterator
				child_it = child_list.begin(), end3 = child_list.end();
			 child_it != end3; ++child_it)
		{
			LLViewerObject* child = *child_it;
			if (!child || child->isDead() || child->isAvatar())
			{
				continue;
			}

			++primitives;
			scriptcounts_map_t::iterator itrc = mScriptCounts.find(child->getID());
			if (itrc != mScriptCounts.end())
			{
				if (scripts >= 0)
				{
					scripts += itrc->second;
				}
				total_scripts += itrc->second;
			}
			else
			{
				loading = true;
				requestInventory(child);
				scripts = -1; // Value for "loading"
			}
		}
		total_primitives += primitives;

		LLSD row;
		row["id"] = id;
		row["columns"][0]["column"] = "object_id";
		row["columns"][0]["type"] = "text";
		row["columns"][0]["value"] = id.asString();
		row["columns"][1]["column"] = "attach";
		row["columns"][1]["type"] = "text";
		row["columns"][1]["value"] = joint_name;
		row["columns"][2]["column"] = "primitives";
		row["columns"][2]["type"] = "text";
		row["columns"][2]["value"] = llformat("%d", primitives);
		row["columns"][3]["column"] = "scripts";
		row["columns"][3]["type"] = "text";
		row["columns"][3]["value"] = (scripts < 0 ? loadingstr
												  : llformat("%d", scripts));

		LLScrollListItem* item = mObjectList->addElement(row);
		if (item)
		{
			item->setEnabled(!vobj->isHUDAttachment());
		}
	}

	std::string comment = COMMENT_PREFIX + getString("total_attachments") +
						  llformat(" %d ", attachments) +
						  getString("total_primitives") +
						  llformat(" %d ", total_primitives) +
						  getString("total_scripts") +
						  llformat(" %d", total_scripts);
	if (loading)
	{
		comment += " " + getString("so_far");
	}
	mObjectList->addCommentText(comment);

	if (selected_index > -1 &&
		mObjectList->getItemIndex(selected_uuid) == selected_index)
	{
		mObjectList->selectNthItem(selected_index);
	}
	else
	{
		mObjectList->selectNthItem(0);
	}
	mObjectList->setScrollPos(pos);
}

void HBFloaterInspectAvatar::requestInventory(LLViewerObject* vobj)
{
	if (vobj && !hasRegisteredListener(vobj))
	{
		registerVOInventoryListener(vobj, NULL);
		requestVOInventory(vobj);
	}
}

void HBFloaterInspectAvatar::inventoryChanged(LLViewerObject* vobj,
											  LLInventoryObject::object_list_t* inv,
											  S32, void*)
{
	if (!vobj || !inv)
	{
		return;
	}
	removeVOInventoryListener(vobj);
	const LLUUID id = vobj->getID();
	S32 scripts = 0;
	for (LLInventoryObject::object_list_t::const_iterator
			it = inv->begin(), end = inv->end();
		 it != end; ++it)
	{
		LLAssetType::EType type = (*it)->getType();
		if (type == LLAssetType::AT_LSL_TEXT ||
			// Legacy scripts
			type == LLAssetType::AT_SCRIPT)
		{
			++scripts;
		}
	}
	mScriptCounts[id] = scripts;
	mDirty = true;
}

//static
void HBFloaterInspectAvatar::show(const LLUUID& av_id)
{
	HBFloaterInspectAvatar* self = getInstance();
	self->mScriptCounts.clear();
	self->mAvatarID = av_id;
	self->open();
	self->refresh();
}

//static
void HBFloaterInspectAvatar::onClickProfile(void* data)
{
	HBFloaterInspectAvatar* self = (HBFloaterInspectAvatar*)data;
	if (self && self->mAvatarID.notNull())
	{
		LLFloaterAvatarInfo::showFromDirectory(self->mAvatarID);
	}
}

//static
void HBFloaterInspectAvatar::onClickRefresh(void* data)
{
	HBFloaterInspectAvatar* self = (HBFloaterInspectAvatar*)data;
	if (self)
	{
		self->removeVOInventoryListeners();
		self->mScriptCounts.clear();
		self->mDirty = true;
	}
}

//static
void HBFloaterInspectAvatar::onClickClose(void* data)
{
	HBFloaterInspectAvatar* self = (HBFloaterInspectAvatar*)data;
	if (self)
	{
		self->close();
	}
}

//static
void HBFloaterInspectAvatar::onDoubleClickObject(void* data)
{
	HBFloaterInspectAvatar* self = (HBFloaterInspectAvatar*)data;
	if (self)
	{
		if (self->mObjectList->getAllSelected().size() > 0)
		{
			LLScrollListItem* item = self->mObjectList->getFirstSelected();
			if (item)
			{
				const LLUUID& id = item->getUUID();
				LLViewerObject* vobj = gObjectList.findObject(id);
				if (vobj)
				{
					LLFloaterInspect::show(vobj);
				}
			}
		}
	}
}
