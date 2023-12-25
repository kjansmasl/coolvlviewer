/**
 * @file llfloatergroups.cpp
 * @brief LLFloaterGroups class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llfloatergroups.h"

#include "llalertdialog.h"
#include "llbutton.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llmessage.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llfloatergroupinfo.h"
#include "hbfloatergrouptitles.h"
#include "hbfloatersearch.h"
#include "llimmgr.h"
#include "llmutelist.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llstartup.h"
#include "llviewercontrol.h"

using namespace LLOldEvents;

//static
LLFloaterGroupPicker::instances_list_t LLFloaterGroupPicker::sInstances;

// Helper function used by the two LLFloaterGroupPicker and LLFloaterGroups
// classes, to populate their groups list.
void populate_groups_list(LLFloater* self, LLScrollListCtrl* group_list,
						  const LLUUID& highlight_id, U64 powers_mask,
						  bool with_checkboxes = false)
{
	group_list->deleteAllItems();

	LLUUID id;
	for (S32 i = 0, count = gAgent.mGroups.size(); i < count; ++i)
	{
		id = gAgent.mGroups[i].mID;
		const LLGroupData& group_data = gAgent.mGroups[i];

		if (powers_mask == GP_ALL_POWERS ||
			(group_data.mPowers & powers_mask) != 0)
		{
			std::string style = "NORMAL";
			if (highlight_id == id)
			{
				style = "BOLD";
			}

			LLSD element;
			element["id"] = id;
			LLSD& name_column = element["columns"][0];
			name_column["column"] = "name";
			name_column["value"] = group_data.mName;
			name_column["font"] = "SANSSERIF";
			name_column["font-style"] = style;

			if (with_checkboxes)
			{
				LLSD& profile_column = element["columns"][1];
				profile_column["column"] = "profile";
				profile_column["type"] = "checkbox";
				profile_column["value"] = group_data.mListInProfile;

				LLSD& chat_column = element["columns"][2];
				chat_column["column"] = "chat";
				chat_column["type"] = "checkbox";
				chat_column["value"] = !LLMuteList::isMuted(id, "",
															LLMute::flagTextChat);

				LLSD& notices_column = element["columns"][3];
				notices_column["column"] = "notices";
				notices_column["type"] = "checkbox";
				notices_column["value"] = group_data.mAcceptNotices;
			}

			group_list->addElement(element, ADD_SORTED);
		}
	}

	// Add "none" to list at top
	std::string style = "NORMAL";
	if (highlight_id.isNull())
	{
		style = "BOLD";
	}
	LLSD element;
	element["id"] = LLUUID::null;

	LLSD& name_column = element["columns"][0];
	name_column["column"] = "name";
	name_column["value"] = self->getString("none");
	name_column["font"] = "SANSSERIF";
	name_column["font-style"] = style;

	if (with_checkboxes)
	{
		LLSD& profile_column = element["columns"][1];
		profile_column["column"] = "profile";
		profile_column["value"] = "";

		LLSD& chat_column = element["columns"][2];
		chat_column["column"] = "chat";
		chat_column["value"] = "";

		LLSD& notices_column = element["columns"][3];
		notices_column["column"] = "notices";
		notices_column["value"] = "";
	}

	group_list->addElement(element, ADD_TOP);

	group_list->selectByValue(highlight_id);
	group_list->scrollToShowSelected();
}

//-----------------------------------------------------------------------------
// LLFloaterGroupPicker class
//-----------------------------------------------------------------------------

//static
LLFloaterGroupPicker* LLFloaterGroupPicker::show(LLFloaterGroupPicker::callback_t callback,
												 void* userdata)
{
	LLFloaterGroupPicker* self = NULL;
	for (instances_list_t::iterator it = sInstances.begin(),
									end = sInstances.end();
		 it != end; ++it)
	{
		LLFloaterGroupPicker* instance = *it;
		if (instance && instance->mSelectCallback == callback &&
			instance->mCallbackUserdata == userdata)
		{
			self = instance;
			break;
		}
	}

	if (!self)
	{
		self = new LLFloaterGroupPicker(callback, userdata);
	}

	self->open();

	return self;
}

LLFloaterGroupPicker::LLFloaterGroupPicker(callback_t callback, void* userdata)
:	mSelectCallback(callback),
	mCallbackUserdata(userdata),
	mPowersMask(GP_ALL_POWERS)
{
	sInstances.insert(this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_choose_group.xml");
}

//virtual
LLFloaterGroupPicker::~LLFloaterGroupPicker()
{
	sInstances.erase(this);
}

bool LLFloaterGroupPicker::postBuild()
{
	mGroupsList = getChild<LLScrollListCtrl>("group list");
	mGroupsList->setDoubleClickCallback(onBtnOK);
	mGroupsList->setCallbackUserData(this);
	populate_groups_list(this, mGroupsList, gAgent.getGroupID(), mPowersMask);

	childSetAction("Cancel", onBtnCancel, this);
	childSetAction("OK", onBtnOK, this);

	setDefaultBtn("OK");

	return true;
}

void LLFloaterGroupPicker::setPowersMask(U64 powers_mask)
{
	mPowersMask = powers_mask;
	populate_groups_list(this, mGroupsList, gAgent.getGroupID(), mPowersMask);
}

//static
void LLFloaterGroupPicker::onBtnOK(void* userdata)
{
	LLFloaterGroupPicker* self = (LLFloaterGroupPicker*)userdata;
	if (self)
	{
		if (self->mSelectCallback)
		{
			const LLUUID& group_id = self->mGroupsList->getCurrentID();
			self->mSelectCallback(group_id, self->mCallbackUserdata);
		}

		self->close();
	}
}

//static
void LLFloaterGroupPicker::onBtnCancel(void* userdata)
{
	LLFloaterGroupPicker* self = (LLFloaterGroupPicker*)userdata;
	if (self)
	{
		self->close();
	}
}

//-----------------------------------------------------------------------------
// LLFloaterGroups class
//-----------------------------------------------------------------------------

LLFloaterGroups::LLFloaterGroups(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_groups.xml");
	gAgent.addListener(this, "new group");
	gSavedSettings.setBool("ShowGroups", true);
}

//virtual
LLFloaterGroups::~LLFloaterGroups()
{
	gAgent.removeListener(this);
	gFocusMgr.releaseFocusIfNeeded(this);
	gSavedSettings.setBool("ShowGroups", false);
}

//virtual
bool LLFloaterGroups::postBuild()
{
	mGroupsList = getChild<LLScrollListCtrl>("group list");
	mGroupsList->setCommitCallback(onGroupList);
	mGroupsList->setDoubleClickCallback(onBtnIM);
	mGroupsList->setCallbackUserData(this);

	mActivateBtn = getChild<LLButton>("Activate");
	mActivateBtn->setClickedCallback(onBtnActivate, this);

	mInfoBtn = getChild<LLButton>("Info");
	mInfoBtn->setClickedCallback(onBtnInfo, this);

	mIMBtn = getChild<LLButton>("IM");
	mIMBtn->setClickedCallback(onBtnIM, this);

	mLeaveBtn = getChild<LLButton>("Leave");
	mLeaveBtn->setClickedCallback(onBtnLeave, this);

	mCreateBtn = getChild<LLButton>("Create");
	mCreateBtn->setClickedCallback(onBtnCreate, this);

	childSetAction("Search...", onBtnSearch, this);

	childSetAction("Titles...", onBtnTitles, this);

	childSetAction("OK", onBtnClose, this);

	setDefaultBtn("IM");

	reset();

	return true;
}

//virtual
bool LLFloaterGroups::handleEvent(LLPointer<LLEvent> event, const LLSD&)
{
	if (event->desc() == "new group")
	{
		reset();
		return true;
	}
	return false;
}

void LLFloaterGroups::reset()
{
	childSetTextArg("groupcount", "[COUNT]",
					llformat("%d",gAgent.mGroups.size()));
	childSetTextArg("groupcount", "[MAX]", llformat("%d", gMaxAgentGroups));

	populate_groups_list(this, mGroupsList, gAgent.getGroupID(),
						 GP_ALL_POWERS, true);
	enableButtons();
}

void LLFloaterGroups::enableButtons()
{
	const LLUUID& group_id = mGroupsList->getCurrentID();

	mActivateBtn->setEnabled(group_id != gAgent.getGroupID());

	bool enable = group_id.notNull();
	mLeaveBtn->setEnabled(enable);
	mInfoBtn->setEnabled(enable);
	mIMBtn->setEnabled(enable &&
					   !LLMuteList::isMuted(group_id, LLMute::flagTextChat));

	mCreateBtn->setEnabled((S32)gAgent.mGroups.size() < gMaxAgentGroups);
}

//static
void LLFloaterGroups::onGroupList(LLUICtrl*, void* userdata)
{
	LLFloaterGroups* self = (LLFloaterGroups*)userdata;
	if (!self)
	{
		return;
	}

	self->enableButtons();

	LLScrollListItem* item = self->mGroupsList->getFirstSelected();
	if (!item)
	{
		return;
	}

	const LLUUID group_id =  item->getValue().asUUID();
	if (group_id.isNull())
	{
		return;
	}

	LLGroupData group_data;
	if (!gAgent.getGroupData(group_id, group_data))
	{
		return;
	}

	bool profile = item->getColumn(1)->getValue().asBoolean();
	bool chat = item->getColumn(2)->getValue().asBoolean();
	bool notices = item->getColumn(3)->getValue().asBoolean();
	bool update_floaters = false;

	bool muted = LLMuteList::isMuted(group_id, "", LLMute::flagTextChat);
	if (muted == chat)
	{
		LLMute mute(group_id, group_data.mName, LLMute::GROUP);
		if (chat)
		{
			if (muted)
			{
				LLMuteList::remove(mute, LLMute::flagTextChat);
			}
		}
		else
		{
			if (!muted)
			{
				LLMuteList::add(mute, LLMute::flagTextChat);
			}
		}
		update_floaters = true;
	}

	if (group_data.mListInProfile != profile ||
		group_data.mAcceptNotices != notices)
	{
		gAgent.setUserGroupFlags(group_id, notices, profile);
		// gAgent.setUserGroupFlags calls update_group_floaters()
		update_floaters = false;
	}

	if (update_floaters)
	{
		update_group_floaters(group_id);
	}
}

//static
void LLFloaterGroups::onBtnCreate(void*)
{
	LLFloaterGroupInfo::showCreateGroup(NULL);
}

//static
void LLFloaterGroups::onBtnActivate(void* userdata)
{
	LLFloaterGroups* self = (LLFloaterGroups*)userdata;
	if (self)
	{
		gAgent.setGroup(self->mGroupsList->getCurrentID());
	}
}

//static
void LLFloaterGroups::onBtnInfo(void* userdata)
{
	LLFloaterGroups* self = (LLFloaterGroups*)userdata;
	if (self)
	{
		const LLUUID& group_id = self->mGroupsList->getCurrentID();
		if (group_id.notNull())
		{
			LLFloaterGroupInfo::showFromUUID(group_id);
		}
	}
}

//static
void LLFloaterGroups::onBtnIM(void* userdata)
{
	LLFloaterGroups* self = (LLFloaterGroups*)userdata;
	if (self)
	{
		const LLUUID& group_id = self->mGroupsList->getCurrentID();
		if (gIMMgrp && group_id.notNull())
		{
			LLGroupData group_data;
			if (gAgent.getGroupData(group_id, group_data) &&
				!LLMuteList::isMuted(group_id, "", LLMute::flagTextChat))
			{
				gIMMgrp->setFloaterOpen(true);
				gIMMgrp->addSession(group_data.mName, IM_SESSION_GROUP_START,
									group_id);
				make_ui_sound("UISndStartIM");
			}
			else
			{
				// Muted group
				make_ui_sound("UISndInvalidOp");
			}
		}
	}
}

//static
bool LLFloaterGroups::callbackLeaveGroup(const LLSD& notification,
										 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		const LLUUID& group_id = notification["payload"]["group_id"].asUUID();
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_LeaveGroupRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_GroupData);
		msg->addUUIDFast(_PREHASH_GroupID, group_id);
		gAgent.sendReliableMessage();
	}
	return false;
}

//static
void LLFloaterGroups::onBtnLeave(void* userdata)
{
	LLFloaterGroups* self = (LLFloaterGroups*)userdata;
	if (self)
	{
		const LLUUID& group_id = self->mGroupsList->getCurrentID();
		if (group_id.notNull())
		{
			S32 count = gAgent.mGroups.size();
			S32 i;
			for (i = 0; i < count; ++i)
			{
				if (gAgent.mGroups[i].mID == group_id)
				{
					break;
				}
			}
			if (i < count)
			{
				LLSD args, payload;
				args["GROUP"] = gAgent.mGroups[i].mName;
				payload["group_id"] = group_id;
				gNotifications.add("GroupLeaveConfirmMember", args, payload,
								   callbackLeaveGroup);
			}
		}
	}
}

//static
void LLFloaterGroups::onBtnSearch(void*)
{
	HBFloaterSearch::showGroups();
}

//static
void LLFloaterGroups::onBtnTitles(void*)
{
	HBFloaterGroupTitles::showInstance();
}

//static
void LLFloaterGroups::onBtnClose(void* userdata)
{
	LLFloaterGroups* self = (LLFloaterGroups*)userdata;
	if (self)
	{
		self->close();
	}
}
