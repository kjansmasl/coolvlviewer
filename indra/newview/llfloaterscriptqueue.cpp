/**
 * @file llfloaterscriptqueue.cpp
 * @brief LLFloaterScriptQueue class implementation
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

/**
 *
 * Implementation of the script queue which keeps an array of object
 * UUIDs and manipulates all of the scripts on each of them.
 *
 */

#include "llviewerprecompiledheaders.h"

#include <utility>

#include "llfloaterscriptqueue.h"

#include "llbutton.h"
#include "lldir.h"
#include "llexperiencecache.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"
#include "lluistring.h"

#include "llagent.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llselectmgr.h"
#include "llviewerassetupload.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerstats.h"

struct LLScriptQueueData
{
	LLUUID mQueueID;
	LLUUID mTaskId;
	LLPointer<LLInventoryItem> mItem;
	LLHost mHost;
	LLUUID mExperienceId;
	std::string mExperiencename;

	LLScriptQueueData(const LLUUID& q_id, const LLUUID& task_id,
					  LLInventoryItem* item, const LLHost& host)
	:	mQueueID(q_id),
		mTaskId(task_id),
		mItem(new LLInventoryItem(item)),
		mHost(host)
	{
	}
};

// NOTE: minor specialization of LLScriptAssetUpload: it does not require a
// buffer (and does not save a buffer to the cache) and it finds the compile
// queue floater and displays a compiling message.
class LLQueuedScriptAssetUpload final : public LLScriptAssetUpload
{
protected:
	LOG_CLASS(LLQueuedScriptAssetUpload);

public:
	LLQueuedScriptAssetUpload(const LLUUID& task_id, const LLUUID& item_id,
							  const LLUUID& asset_id, TargetType_t target_type,
							  bool running, const std::string& script_name,
							  const LLUUID& queue_id, const LLUUID& exp_id,
							  task_uploaded_cb_t finish)
	:	LLScriptAssetUpload(task_id, item_id, target_type, running, exp_id,
							// *TODO: provide a proper failed_cb_t callback.
							LLStringUtil::null, finish, failed_cb_t()),
		mScriptName(script_name),
		mQueueId(queue_id)
	{
		setAssetId(asset_id);
	}

	LLSD prepareUpload() override
	{
		// NOTE: the parent class (LLScriptAssetUpload) will attempt to save
		// the script buffer into to the cache. Since the resource is already
		// in the cache we do not want to do that. Just put a compiling message
		// in the window and move on.
		LLFloaterCompileQueue* queue;
		queue = (LLFloaterCompileQueue*)LLFloaterScriptQueue::findInstance(mQueueId);
		if (queue)
		{
			std::string message = queue->getString("compiling") + " ";
 			queue->logMessage(message + mScriptName);
		}

		return LLSD().with("success", LLSD::Boolean(true));
	}

private:
	void setScriptName(const std::string& name)			{ mScriptName = name; }

private:
	LLUUID		mQueueId;
	std::string	mScriptName;
};

///////////////////////////////////////////////////////////////////////////////
// Class LLFloaterScriptQueue
///////////////////////////////////////////////////////////////////////////////

//static
LLFloaterScriptQueue::instances_map_t LLFloaterScriptQueue::sInstances;

LLFloaterScriptQueue::LLFloaterScriptQueue(const std::string& title,
										   const std::string& verb)
:	LLFloater("script queue"),
	mDone(false)
{
	mID.generate();
	sInstances[mID] = this;

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_script_queue.xml");
	setTitle(getString(title));
	mVerb = getString(verb);
}

//virtual
LLFloaterScriptQueue::~LLFloaterScriptQueue()
{
	sInstances.erase(mID);
}

//virtual
bool LLFloaterScriptQueue::postBuild()
{
	mCloseBtn = getChild<LLButton>("close");
	mCloseBtn->setClickedCallback(onCloseBtn, this);
	mCloseBtn->setEnabled(false);

	mMessages = getChild<LLScrollListCtrl>("queue output");

	return true;
}

// Finds an instance by Id. Returns NULL if it does not exist.
//static
LLFloaterScriptQueue* LLFloaterScriptQueue::findInstance(const LLUUID& id)
{
	instances_map_t::iterator it = sInstances.find(id);
	return it != sInstances.end() ? it->second : NULL;
}

void LLFloaterScriptQueue::logMessage(const std::string& message)
{
	mMessages->addCommentText(message);
}

void LLFloaterScriptQueue::requestInventory(LLViewerObject* objectp)
{
	if (objectp && !hasRegisteredListener(objectp))
	{
		registerVOInventoryListener(objectp, NULL);
		requestVOInventory(objectp);
	}
}

// This is the callback method for the viewer object currently being worked on.
//virtual
void LLFloaterScriptQueue::inventoryChanged(LLViewerObject* objectp,
											 LLInventoryObject::object_list_t* inv,
											 S32, void*)
{
	if (!objectp)
	{
		return;
	}

	llinfos << "Processing object " << objectp->getID() << llendl;

	removeVOInventoryListener(objectp);

	if (inv && objectp->getID() == mCurrentObjectID)
	{
		llinfos << "Processing object " << mCurrentObjectID << llendl;
		handleInventory(objectp, inv);
	}
	else
	{
		// No inventory for the current primitive: move to the next.
		llinfos << "No inventory for " << mCurrentObjectID << llendl;
		nextObject();
	}
}

//static
void LLFloaterScriptQueue::onCloseBtn(void* user_data)
{
	LLFloaterScriptQueue* self = (LLFloaterScriptQueue*)user_data;
	self->close();
}

bool LLFloaterScriptQueue::start()
{
	// Note: we add all the selected objects, be them flagged as scripted or
	// not, because this info is received asynchronously from the server and
	// may not yet be known to the viewer, especially in child primitives. We
	// therefore need to retreive the inventory for each and every selected
	// primitive.
	LLObjectSelectionHandle object_selection = gSelectMgr.getSelection();
	for (LLObjectSelection::valid_iterator
			iter = object_selection->valid_begin(),
			end = object_selection->valid_end();
		 iter != end; ++iter)
	{
		LLSelectNode* obj = *iter;
		if (!obj) continue;		// Paranoia

		LLViewerObject* vobj = obj->getObject();
		if (!vobj || vobj->isDead())
		{
			// Object gone or soon gone !
			continue;
		}

		LLUUID id = vobj->getID();

	 	if (obj->mCreationDate == 0)
		{
			llwarns << "Object skipped due to missing information from the server. Id: "
					<< id << llendl;
		}
		else if (vobj->permModify())
		{
			llinfos << "Adding object id: " << id << llendl;
			mObjectIDs.emplace_back(id);
		}
	}

	LLUIString starting = getString("starting");
	starting.setArg("[VERB]", mVerb);
	starting.setArg("[ITEMS]", llformat("%d", mObjectIDs.size()));
	logMessage(starting.getString());

	return startQueue();
}

bool LLFloaterScriptQueue::nextObject()
{
	bool successful_start = false;

	do
	{
		mCurrentObjectID.setNull();
		S32 count = mObjectIDs.size();
		llinfos << count << " objects left to process." << llendl;
		if (count > 0)
		{
			mCurrentObjectID = mObjectIDs.back();
			mObjectIDs.pop_back();
			LLViewerObject* obj = gObjectList.findObject(mCurrentObjectID);
			if (obj && !obj->isDead())
			{
				llinfos << "Requesting inventory for " << mCurrentObjectID
						<< llendl;
				requestInventory(obj);
				successful_start = true;
			}
			else
			{
				llinfos << "Removed dead object id: " << mCurrentObjectID
						<< llendl;
				mCurrentObjectID.setNull();
			}
		}
		llinfos << "Operation "
				<< (successful_start ? "successful" : "unsuccessful")
				<< llendl;
	}
	while (mObjectIDs.size() > 0 && !successful_start);

	if (isDone() && !mDone)
	{
		mDone = true;
		logMessage(getString("done"));
		mCloseBtn->setEnabled(true);
	}

	return successful_start;
}

//virtual
bool LLFloaterScriptQueue::startQueue()
{
	return nextObject();
}

///////////////////////////////////////////////////////////////////////////////
// Class LLFloaterCompileQueue
///////////////////////////////////////////////////////////////////////////////

//static
LLFloaterCompileQueue* LLFloaterCompileQueue::create(bool mono)
{
	LLFloaterCompileQueue* self = new LLFloaterCompileQueue();
	self->mMono = mono;
	return self;
}

LLFloaterCompileQueue::LLFloaterCompileQueue()
:	LLFloaterScriptQueue("compile_title", "compile_verb")
{
}

void LLFloaterCompileQueue::experienceIdsReceived(const LLSD& content)
{
	for (LLSD::array_const_iterator it = content.beginArray(),
									end = content.endArray();
		 it != end; ++it)
	{
		mExperienceIds.emplace(it->asUUID());
	}
	nextObject();
}

bool LLFloaterCompileQueue::hasExperience(const LLUUID& id) const
{
	return mExperienceIds.count(id) != 0;
}

void LLFloaterCompileQueue::handleInventory(LLViewerObject* objectp,
											LLInventoryObject::object_list_t* inv)
{
	if (!objectp || !inv) return;

	// Find all of the LSL, leaving off duplicates. We will remove all matching
	// asset UUIDs on compilation success.
	typedef std::multimap<LLUUID, LLPointer<LLInventoryItem> > uuid_item_map;
	uuid_item_map asset_item_map;
	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		LLAssetType::EType type = (*it)->getType();
		if (type == LLAssetType::AT_LSL_TEXT ||
			type == LLAssetType::AT_SCRIPT)			// Legacy scripts
		{
			LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
			// Check permissions before allowing the user to retrieve data.
			if (item->getPermissions().allowModifyBy(gAgentID, gAgent.getGroupID())  &&
				item->getPermissions().allowCopyBy(gAgentID, gAgent.getGroupID()))
			{
				LLPointer<LLViewerInventoryItem> script =
					new LLViewerInventoryItem(item);
				mCurrentScripts.emplace_back(std::move(script));
				asset_item_map.emplace(item->getAssetUUID(), item);
			}
		}
	}

	if (asset_item_map.empty())
	{
		// There is no script in this object. Move on.
		nextObject();
	}
	else
	{
		LLViewerRegion* regionp = objectp->getRegion();
		if (!regionp)
		{
			// No region associated with this object !... Move on.
			llwarns << "NULL region for object: " << objectp->getID()
					<< ". Skipping." << llendl;
			nextObject();
			return;
		}

		const std::string& url = regionp->getCapability("GetMetadata");
		LLExperienceCache* expcache = LLExperienceCache::getInstance();
		// Request all of the assets.
		for (uuid_item_map::iterator iter = asset_item_map.begin(),
									 end = asset_item_map.end();
			 iter != end; ++iter)
		{
			LLInventoryItem* itemp = iter->second;
			LLScriptQueueData* datap =
				new LLScriptQueueData(getID(), objectp->getID(), itemp,
									  regionp->getHost());
			if (!url.empty())
			{
				expcache->fetchAssociatedExperience(itemp->getParentUUID(),
													itemp->getUUID(), url,
													boost::bind(LLFloaterCompileQueue::requestAsset,
																datap, _1));
			}
			else
			{
				requestAsset(datap, LLSD());
			}
		}
	}
}

//static
void LLFloaterCompileQueue::requestAsset(LLScriptQueueData* datap,
										 const LLSD& experience)
{
	if (!datap || !gAssetStoragep) return;

	LLFloaterCompileQueue* queue =
		(LLFloaterCompileQueue*)LLFloaterScriptQueue::findInstance(datap->mQueueID);
	if (!queue)
	{
		delete datap;
		return;
	}
	if (experience.has(LLExperienceCache::EXPERIENCE_ID))
	{
		datap->mExperienceId=experience[LLExperienceCache::EXPERIENCE_ID].asUUID();
		if (!queue->hasExperience(datap->mExperienceId))
		{
			LLUIString skipping = queue->getString("skipping");
			skipping.setArg("[SCRIPT]", datap->mItem->getName());
			skipping.setArg("[EXP]",
							experience[LLExperienceCache::NAME].asString());
			queue->logMessage(skipping.getString());
			queue->removeItemByItemID(datap->mItem->getUUID());
			delete datap;
			return;
		}
	}
	gAssetStoragep->getInvItemAsset(datap->mHost, gAgentID, gAgentSessionID,
									datap->mItem->getPermissions().getOwner(),
									datap->mTaskId, datap->mItem->getUUID(),
									datap->mItem->getAssetUUID(),
									datap->mItem->getType(),
									scriptArrived, (void*)datap);
}

//static
void LLFloaterCompileQueue::finishLSLUpload(LLUUID item_id, LLUUID task_id,
											LLUUID new_asset_id, LLSD response,
											std::string script_name,
											LLUUID queue_id)
{
	std::string message;
	LLFloaterCompileQueue* queue =
		(LLFloaterCompileQueue*)LLFloaterScriptQueue::findInstance(queue_id);
	if (queue)
	{
		// Bytecode save completed
		if (response["compiled"])
		{
			message = "Compilation of \"" + script_name + "\" succeeded.";
			queue->logMessage(message);
			llinfos << message << llendl;
		}
		else
		{
			LLSD compile_errors = response["errors"];
			for (LLSD::array_const_iterator line = compile_errors.beginArray(),
											endl = compile_errors.endArray();
				 line != endl; ++line)
			{
				std::string str = line->asString();
				str.erase(std::remove(str.begin(), str.end(), '\n'),
									  str.end());
				queue->logMessage(message);
			}
			llinfos << response["errors"] << llendl;
		}
		queue->removeItemByItemID(item_id);
	}
}

// This is the callback for when each script arrives
//static
void LLFloaterCompileQueue::scriptArrived(const LLUUID& asset_id,
										  LLAssetType::EType type,
										  void* user_data, S32 status,
										  LLExtStat)
{
	LLScriptQueueData* data = (LLScriptQueueData*)user_data;
	if (!data) return;

	std::string buffer;
	std::string script_name = data->mItem->getName();

	LLFloaterCompileQueue* queue =
		(LLFloaterCompileQueue*)LLFloaterScriptQueue::findInstance(data->mQueueID);
	if (queue && status == 0)
	{
		LLViewerObject* object = gObjectList.findObject(data->mTaskId);
		if (!object)
		{
			llwarns << "Object " << data->mTaskId
					<< " is gone. Skipping script." << llendl;
			return;
		}
		LLViewerRegion* regionp = object->getRegion();
		if (!regionp)
		{
			llwarns << "NULL region for object: " << object->getID()
					<< ". Skipping script." << llendl;
			return;
		}
		
		const std::string& url = regionp->getCapability("UpdateScriptTask");
		if (url.empty())
		{
			llwarns << "Missing UpdateScriptTask capability for region of object "
					<< object->getID() << ". Skipping script." << llendl;
			return;
		}
		LLBufferedAssetUploadInfo::task_uploaded_cb_t proc =
			boost::bind(&LLFloaterCompileQueue::finishLSLUpload, _1, _2, _3,
						_4, script_name, queue->getID());
		LLResourceUploadInfo::ptr_t
			info(new LLQueuedScriptAssetUpload(data->mTaskId,
											   data->mItem->getUUID(),
											   asset_id,
											   queue->mMono ? LLScriptAssetUpload::MONO
															: LLScriptAssetUpload::LSL2,
											   true, script_name,
											   queue->getID(),
											   data->mExperienceId, proc));
		LLViewerAssetUpload::enqueueInventoryUpload(url, info);
	}
	else
	{
		gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);

		if (status == LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE)
		{
			buffer = queue->getString("not_found") + " " + script_name;
		}
		else if (status == LL_ERR_INSUFFICIENT_PERMISSIONS)
		{
			buffer = queue->getString("bad_perm") + " " + script_name;
		}
		else
		{
			buffer = queue->getString("failure") + " " + script_name;
		}

		llwarns << "Problem downloading script: " << script_name << llendl;
		if (gSavedSettings.getBool("ScriptErrorsAsChat"))
		{
			LLChat chat(buffer);
			LLFloaterChat::addChat(chat);
		}

		if (queue)
		{
			queue->removeItemByItemID(data->mItem->getUUID());
		}
	}

	if (queue && buffer.size() > 0)
	{
		queue->logMessage(buffer);
	}
	delete data;
}

void LLFloaterCompileQueue::removeItemByItemID(const LLUUID& asset_id)
{
	for (size_t i = 0; i < mCurrentScripts.size(); )
	{
		if (mCurrentScripts[i]->getUUID() == asset_id)
		{
			vector_replace_with_last(mCurrentScripts,
									 mCurrentScripts.begin() + i);
		}
		else
		{
			++i;
		}
	}
	if (mCurrentScripts.empty())
	{
		nextObject();
	}
}

bool LLFloaterCompileQueue::startQueue()
{
	const std::string& url =
		gAgent.getRegionCapability("GetCreatorExperiences");
	if (!url.empty())
	{
		LLCoreHttpUtil::HttpCoroutineAdapter::completionCallback_t
			succ = boost::bind(&LLFloaterCompileQueue::processExperienceIdResults,
							   _1, getID());
		LLCoreHttpUtil::HttpCoroutineAdapter::completionCallback_t
			fail = boost::bind(&LLFloaterCompileQueue::processExperienceIdResults,
							   LLSD(), getID());
		LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet(url, succ, fail);
		return true;
	}
	return nextObject();
}

//static
void LLFloaterCompileQueue::processExperienceIdResults(LLSD result,
													   LLUUID queue_id)
{
	LLFloaterCompileQueue* self =
		(LLFloaterCompileQueue*)LLFloaterScriptQueue::findInstance(queue_id);
	if (self)
	{
		self->experienceIdsReceived(result["experience_ids"]);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Class LLFloaterResetQueue
///////////////////////////////////////////////////////////////////////////////

//static
LLFloaterResetQueue* LLFloaterResetQueue::create()
{
	return new LLFloaterResetQueue();
}

LLFloaterResetQueue::LLFloaterResetQueue()
:	LLFloaterScriptQueue("reset_title", "reset_verb")
{
}

void LLFloaterResetQueue::handleInventory(LLViewerObject* viewer_obj,
										  LLInventoryObject::object_list_t* inv)
{
	// find all of the lsl, leaving off duplicates. We'll remove all matching
	// asset uuids on compilation success.

	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		if ((*it)->getType() == LLAssetType::AT_LSL_TEXT)
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
				logMessage(getString("resetting") + " " + item->getName());

				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessageFast(_PREHASH_ScriptReset);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_Script);
				msg->addUUIDFast(_PREHASH_ObjectID, viewer_obj->getID());
				msg->addUUIDFast(_PREHASH_ItemID, (*it)->getUUID());
				msg->sendReliable(object->getRegion()->getHost());
			}
		}
	}

	nextObject();
}

///////////////////////////////////////////////////////////////////////////////
// Class LLFloaterRunQueue
///////////////////////////////////////////////////////////////////////////////

//static
LLFloaterRunQueue* LLFloaterRunQueue::create()
{
	return new LLFloaterRunQueue();
}

LLFloaterRunQueue::LLFloaterRunQueue()
:	LLFloaterScriptQueue("run_title", "run_verb")
{
}

void LLFloaterRunQueue::handleInventory(LLViewerObject* viewer_obj,
										LLInventoryObject::object_list_t* inv)
{
	// find all of the LSL, leaving off duplicates. We will remove all matching
	// asset uuids on compilation success.
	for (LLInventoryObject::object_list_t::const_iterator it = inv->begin(),
														  end = inv->end();
		 it != end; ++it)
	{
		if ((*it)->getType() == LLAssetType::AT_LSL_TEXT)
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
				logMessage(getString("running") + " " + item->getName());

				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessageFast(_PREHASH_SetScriptRunning);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_Script);
				msg->addUUIDFast(_PREHASH_ObjectID, viewer_obj->getID());
				msg->addUUIDFast(_PREHASH_ItemID, (*it)->getUUID());
				msg->addBoolFast(_PREHASH_Running, true);
				msg->sendReliable(object->getRegion()->getHost());
			}
		}
	}

	nextObject();
}

///////////////////////////////////////////////////////////////////////////////
// Class LLFloaterStopQueue
///////////////////////////////////////////////////////////////////////////////

//static
LLFloaterStopQueue* LLFloaterStopQueue::create()
{
	return new LLFloaterStopQueue();
}

LLFloaterStopQueue::LLFloaterStopQueue()
:	LLFloaterScriptQueue("stop_title", "stop_verb")
{
}

void LLFloaterStopQueue::handleInventory(LLViewerObject* viewer_obj,
										 LLInventoryObject::object_list_t* inv)
{
	// find all of the lsl, leaving off duplicates. We'll remove
	// all matching asset uuids on compilation success.

	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		if ((*it)->getType() == LLAssetType::AT_LSL_TEXT)
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)*it);
				logMessage(getString("stopping") + " " + item->getName());

				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessageFast(_PREHASH_SetScriptRunning);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_Script);
				msg->addUUIDFast(_PREHASH_ObjectID, viewer_obj->getID());
				msg->addUUIDFast(_PREHASH_ItemID, (*it)->getUUID());
				msg->addBoolFast(_PREHASH_Running, false);
				msg->sendReliable(object->getRegion()->getHost());
			}
		}
	}

	nextObject();
}
