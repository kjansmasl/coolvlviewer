/**
 * @file llfloatertopobjects.cpp
 * @brief Shows top colliders, top scripts, etc.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloatertopobjects.h"

#include "lllineeditor.h"
#include "llparcel.h"			// For RT_NONE
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "lltracker.h"
#include "llviewermessage.h"	// For formatted_time()
#include "llviewerregion.h"

LLFloaterTopObjects::LLFloaterTopObjects(const LLSD&)
:	mInitialized(false),
	mCurrentMode(STAT_REPORT_TOP_SCRIPTS),
	mFlags(0),
	mTotalScore(0.f)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_top_objects.xml");
}

//virtual
bool LLFloaterTopObjects::postBuild()
{
	mObjectsList = getChild<LLScrollListCtrl>("objects_list");
	mObjectsList->setCommitOnSelectionChange(true);
	mObjectsList->setCommitCallback(onCommitObjectsList);
	mObjectsList->setDoubleClickCallback(onClickShowBeacon);
	mObjectsList->setCallbackUserData(this);
	mObjectsList->setFocus(true);

	childSetAction("show_beacon_btn", onClickShowBeacon, this);
	childSetAction("return_selected_btn", onReturnSelected, this);
	childSetAction("return_all_btn", onReturnAll, this);
	childSetAction("disable_selected_btn", onDisableSelected, this);
	childSetAction("disable_all_btn", onDisableAll, this);
	childSetAction("refresh_btn", onRefresh, this);
	childSetAction("filter_object_btn", onGetByObjectNameClicked, this);
	childSetAction("filter_owner_btn", onGetByOwnerNameClicked, this);

	setDefaultBtn("show_beacon_btn");

#if 0
	LLLineEditor* line_editor = getChild<LLLineEditor>("owner_name_editor");
	if (line_editor)
	{
		line_editor->setCommitOnFocusLost(false);
		line_editor->setCommitCallback(onGetByOwnerName);
		line_editor->setCallbackUserData(this);
	}

	line_editor = getChild<LLLineEditor>("object_name_editor");
	if (line_editor)
	{
		line_editor->setCommitOnFocusLost(false);
		line_editor->setCommitCallback(onGetByObjectName);
		line_editor->setCallbackUserData(this);
	}
#endif

	center();

	return true;
}

void LLFloaterTopObjects::updateSelectionInfo()
{
	const LLUUID& object_id = mObjectsList->getCurrentID();
	if (object_id.isNull()) return;

	std::string id_str = object_id.asString();
	childSetValue("id_editor", LLSD(id_str));
	childSetValue("object_name_editor",
				  mObjectsList->getFirstSelected()->getColumn(1)->getValue().asString());
	childSetValue("owner_name_editor",
				  mObjectsList->getFirstSelected()->getColumn(2)->getValue().asString());
}

void LLFloaterTopObjects::doToObjects(S32 action, bool all)
{
	LLMessageSystem* msg = gMessageSystemp;
	LLViewerRegion* region = gAgent.getRegion();
	if (!region || !msg) return;

	if (mObjectsList->getItemCount() == 0) return;

	bool start_message = true;
	for (S32 i = 0, count = mObjectListIDs.size(); i < count; ++i)
	{
		const LLUUID& task_id = mObjectListIDs[i];
		if (!all && !mObjectsList->isSelected(task_id))
		{
			// Selected only
			continue;
		}
		if (start_message)
		{
			if (action == ACTION_RETURN)
			{
				msg->newMessageFast(_PREHASH_ParcelReturnObjects);
			}
			else
			{
				msg->newMessageFast(_PREHASH_ParcelDisableObjects);
			}
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_ParcelData);
			msg->addS32Fast(_PREHASH_LocalID, -1); // Whole region
			msg->addS32Fast(_PREHASH_ReturnType, RT_NONE);
			start_message = false;
		}

		msg->nextBlockFast(_PREHASH_TaskIDs);
		msg->addUUIDFast(_PREHASH_TaskID, task_id);

		if (msg->isSendFullFast(_PREHASH_TaskIDs))
		{
			msg->sendReliable(region->getHost());
			start_message = true;
		}
	}

	if (!start_message)
	{
		msg->sendReliable(region->getHost());
	}
}

void LLFloaterTopObjects::showBeacon()
{
	LLScrollListItem* first_selected = mObjectsList->getFirstSelected();
	if (!first_selected) return;

	std::string name = first_selected->getColumn(1)->getValue().asString();
	std::string pos_string =  first_selected->getColumn(3)->getValue().asString();

	F32 x, y, z;
	S32 matched = sscanf(pos_string.c_str(), "<%g,%g,%g>", &x, &y, &z);
	if (matched != 3) return;

	LLVector3 pos_agent(x, y, z);
	LLVector3d pos_global = gAgent.getPosGlobalFromAgent(pos_agent);
	gTracker.trackLocation(pos_global, name, "", LLTracker::LOCATION_ITEM);
}

void LLFloaterTopObjects::handleReply(LLMessageSystem* msg, void** data)
{
	U32 request_flags;
	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_RequestFlags,
					request_flags);
	U32 total_count;
	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_TotalObjectCount,
					total_count);
	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_ReportType, mCurrentMode);

	U64 total_memory = 0;
	std::string location, name_buf, owner_buf; 
	LLUUID task_id;
	S32 block_count = msg->getNumberOfBlocks("ReportData");
	for (S32 block = 0; block < block_count; ++block)
	{
		U32 task_local_id;
		msg->getU32Fast(_PREHASH_ReportData, _PREHASH_TaskLocalID,
						task_local_id, block);

		task_id.setNull();
		msg->getUUIDFast(_PREHASH_ReportData, _PREHASH_TaskID, task_id, block);

		F32 pos_x, pos_y, pos_z;
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationX, pos_x, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationY, pos_y, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationZ, pos_z, block);

		F32 score;
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_Score, score, block);

		name_buf.clear();
		msg->getStringFast(_PREHASH_ReportData, _PREHASH_TaskName, name_buf,
						   block);
		owner_buf.clear();
		msg->getStringFast(_PREHASH_ReportData, _PREHASH_OwnerName, owner_buf,
						   block);

		U32 time_stamp = 0;
		F32 mono_score = 0.f;
		S32 public_urls = 0;
		F32 script_size = 0.f;
		location.clear();
		bool have_extended_data = false;
		if (msg->has("DataExtended"))
		{
			have_extended_data = true;
			msg->getU32("DataExtended", "TimeStamp", time_stamp, block);
			msg->getF32("DataExtended", "MonoScore", mono_score, block);
			msg->getS32("DataExtended", "PublicURLs", public_urls, block);
			msg->getString("DataExtended", "ParcelName", location, block);
			msg->getF32("DataExtended", "Size", script_size, block);
			total_memory += (U64)script_size;
		}

		LLSD element;
		element["id"] = task_id;

		element["object_name"] = name_buf;
		element["owner_name"] = owner_buf;
		element["columns"][0]["column"] = "score";
		element["columns"][0]["value"] = llformat("%0.3f", score);
		element["columns"][0]["font"] = "SANSSERIF";

		element["columns"][1]["column"] = "name";
		element["columns"][1]["value"] = name_buf;
		element["columns"][1]["font"] = "SANSSERIF";
		element["columns"][2]["column"] = "owner";
		element["columns"][2]["value"] = owner_buf;
		element["columns"][2]["font"] = "SANSSERIF";
		element["columns"][3]["column"] = "location";
		if (location.empty())
		{
			location = llformat("<%0.1f,%0.1f,%0.1f>", pos_x, pos_y, pos_z);
		}
		else
		{
			location = llformat("<%0.1f,%0.1f,%0.1f> ", pos_x, pos_y, pos_z) +
					   location;
		}
		element["columns"][3]["value"] = location;
		element["columns"][3]["font"] = "SANSSERIF";
		element["columns"][4]["column"] = "time";
		element["columns"][4]["value"] = formatted_time((time_t)time_stamp);
		element["columns"][4]["font"] = "SANSSERIF";

		if (mCurrentMode == STAT_REPORT_TOP_SCRIPTS && have_extended_data)
		{
			element["columns"][5]["column"] = "mono_time";
			element["columns"][5]["value"] = llformat("%0.3f", mono_score);
			element["columns"][5]["font"] = "SANSSERIF";

			element["columns"][6]["column"] = "memory";
			element["columns"][6]["value"] = llformat("%d",
													  (S32)script_size / 1024);
			element["columns"][6]["font"] = "SANSSERIF";

			element["columns"][7]["column"] = "URLs";
			element["columns"][7]["value"] = llformat("%d", public_urls);
			element["columns"][7]["font"] = "SANSSERIF";
		}

		mObjectsList->addElement(element);

		mObjectListData.append(element);
		mObjectListIDs.emplace_back(task_id);

		mTotalScore += score;
	}

	if (total_count == 0 && mObjectsList->getItemCount() == 0)
	{
		mObjectsList->addCommentText(getString("none_descriptor"));
	}
	else
	{
		mObjectsList->selectFirstItem();
	}

	if (mCurrentMode == STAT_REPORT_TOP_SCRIPTS)
	{
		setTitle(getString("top_scripts_title"));
		mObjectsList->setColumnLabel("score",
									 getString("scripts_score_label"));
		mObjectsList->setColumnLabel("mono_time",
									 getString("scripts_mono_time_label"));

		LLUIString format = getString("top_scripts_text");
		total_memory /= 1024;
		format.setArg("[MEMORY]", llformat("%d", total_memory));
		format.setArg("[COUNT]", llformat("%d", total_count));
		format.setArg("[TIME]", llformat("%0.1f", mTotalScore));
		childSetValue("title_text", LLSD(format));
	}
	else
	{
		setTitle(getString("top_colliders_title"));
		mObjectsList->setColumnLabel("score",
									 getString("colliders_score_label"));
		mObjectsList->setColumnLabel("mono_time", "");
		LLUIString format = getString("top_colliders_text");
		format.setArg("[COUNT]", llformat("%d", total_count));
		childSetValue("title_text", LLSD(format));
	}
}

//static
void LLFloaterTopObjects::handleLandReply(LLMessageSystem* msg, void** data)
{
	// Make sure dialog is on screen
	LLFloaterTopObjects* self = showInstance();
	if (!self) return;	// Could be out of memory...

	self->handleReply(msg, data);

	// *HACK: for some reason sometimes top scripts originally comes back with
	// no results even though they are there
	if (!self->mObjectListIDs.size() && !self->mInitialized)
	{
		self->onRefresh(self);
		self->mInitialized = true;
	}
}

//static
void LLFloaterTopObjects::setMode(U32 mode)
{
	LLFloaterTopObjects* self = findInstance();
	if (self)
	{
		self->mCurrentMode = mode;
	}
}

//static
void LLFloaterTopObjects::onCommitObjectsList(LLUICtrl* ctrl, void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	if (self)
	{
		self->updateSelectionInfo();
	}
}

//static
void LLFloaterTopObjects::onClickShowBeacon(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	if (self)
	{
		self->showBeacon();
	}
}

//static
bool LLFloaterTopObjects::callbackReturnAll(const LLSD& notification,
											const LLSD& response)
{
	LLFloaterTopObjects* self = findInstance();
	if (self && LLNotification::getSelectedOption(notification, response) == 0)
	{
		self->doToObjects(ACTION_RETURN, true);
	}
	return false;
}

//static
void LLFloaterTopObjects::onReturnAll(void*)
{
	gNotifications.add("ReturnAllTopObjects", LLSD(), LLSD(),
					   callbackReturnAll);
}


//static
void LLFloaterTopObjects::onReturnSelected(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	if (self)
	{
		self->doToObjects(ACTION_RETURN, false);
	}
}

//static
bool LLFloaterTopObjects::callbackDisableAll(const LLSD& notification,
											 const LLSD& response)
{
	LLFloaterTopObjects* self = findInstance();
	if (self && LLNotification::getSelectedOption(notification, response) == 0)
	{
		self->doToObjects(ACTION_DISABLE, true);
	}
	return false;
}

void LLFloaterTopObjects::onDisableAll(void*)
{
	gNotifications.add("DisableAllTopObjects", LLSD(), LLSD(),
					   callbackDisableAll);
}

void LLFloaterTopObjects::onDisableSelected(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	if (self)
	{
		self->doToObjects(ACTION_DISABLE, false);
	}
}

//static
void LLFloaterTopObjects::clearList()
{
	LLFloaterTopObjects* self = findInstance();
	if (self)
	{
		self->mObjectsList->deleteAllItems();
		self->mObjectListData.clear();
		self->mObjectListIDs.clear();
		self->mTotalScore = 0.f;
	}
}

//static
void LLFloaterTopObjects::onRefresh(void* data)
{
	U32 mode = STAT_REPORT_TOP_SCRIPTS;
	U32 flags = 0;
	std::string filter;

	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	if (self)
	{
		mode = self->mCurrentMode;
		flags = self->mFlags;
		filter = self->mFilter;
		self->clearList();
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_LandStatRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_RequestData);
	msg->addU32Fast(_PREHASH_ReportType, mode);
	msg->addU32Fast(_PREHASH_RequestFlags, flags);
	msg->addStringFast(_PREHASH_Filter, filter);
	msg->addS32Fast(_PREHASH_ParcelLocalID, 0);

	msg->sendReliable(gAgent.getRegionHost());

	if (self)
	{
		self->mFilter.clear();
		self->mFlags = 0;
	}
}

//static
void LLFloaterTopObjects::sendRefreshRequest()
{
	onRefresh(findInstance());
}

//static
void LLFloaterTopObjects::onGetByObjectName(LLUICtrl*, void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	if (self)
	{
		self->mFlags = STAT_FILTER_BY_OBJECT;
		self->mFilter = self->childGetText("object_name_editor");
		onRefresh(data);
	}
}

//static
void LLFloaterTopObjects::onGetByOwnerNameClicked(void* data)
{
	onGetByOwnerName(NULL, data);
}

//static
void LLFloaterTopObjects::onGetByOwnerName(LLUICtrl*, void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	if (self)
	{
		self->mFlags = STAT_FILTER_BY_OWNER;
		self->mFilter = self->childGetText("owner_name_editor");
		onRefresh(data);
	}
}

//static
void LLFloaterTopObjects::onGetByObjectNameClicked(void* data)
{
	onGetByObjectName(NULL, data);
}
