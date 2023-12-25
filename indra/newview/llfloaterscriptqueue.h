/**
 * @file llfloaterscriptqueue.h
 * @brief LLFloaterScriptQueue class header file
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

#ifndef LL_LLFLOATERSCRIPTQUEUE_H
#define LL_LLFLOATERSCRIPTQUEUE_H

#include "llfloater.h"
#include "llinventory.h"
#include "llscrolllistctrl.h"

#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llvoinventorylistener.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterScriptQueue
//
// This class provides a mechanism of adding objects to a list that will go
// through and execute action for the scripts on each object. The objects will
// be accessed serially and the scripts may be manipulated in parallel. For
// example, selecting two objects each with three scripts will result in the
// first object having all three scripts manipulated.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterScriptQueue : public LLFloater, public LLVOInventoryListener
{
protected:
	LOG_CLASS(LLFloaterScriptQueue);

public:
	// Returns true if the queue has started, otherwise false.
	bool start();

	void logMessage(const std::string& message);

	// Finds an instance by Id. Returns NULL if it does not exist.
	static LLFloaterScriptQueue* findInstance(const LLUUID& id);

protected:
	LLFloaterScriptQueue(const std::string& title, const std::string& verb);
	~LLFloaterScriptQueue() override;

	bool postBuild() override;

	// This is the callback method for the viewer object currently being worked
	// on.
	void inventoryChanged(LLViewerObject* obj,
						  LLInventoryObject::object_list_t* inv,
						  S32, void*) override;

	void requestInventory(LLViewerObject* objectp);

	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								 LLInventoryObject::object_list_t* inv) = 0;

	static void onCloseBtn(void* user_data);

	// Returns true if this is done
	LL_INLINE bool isDone() const
	{
		return mCurrentObjectID.isNull() && mObjectIDs.size() == 0;
	}

	virtual bool startQueue();

	// Goes to the next object.
	bool nextObject();

	// Get this instances ID.
	LL_INLINE const LLUUID& getID() const	{ return mID; }

protected:
	LLScrollListCtrl*		mMessages;
	LLButton*				mCloseBtn;

	std::string				mVerb;
	LLUUID					mID;

	uuid_vec_t				mObjectIDs;
	LLUUID					mCurrentObjectID;

	bool					mDone;

	typedef fast_hmap<LLUUID, LLFloaterScriptQueue*> instances_map_t;
	static instances_map_t	sInstances;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterCompileQueue
//
// This script queue recompiles each script in selection.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct LLCompileQueueData
{
	LLUUID mQueueID;
	LLUUID mItemId;

	LLCompileQueueData(const LLUUID& q_id, const LLUUID& item_id)
	:	mQueueID(q_id),
		mItemId(item_id)
	{
	}
};

class LLFloaterCompileQueue final : public LLFloaterScriptQueue
{
protected:
	LOG_CLASS(LLFloaterCompileQueue);

public:
	// Use this method to create a compile queue. Once created, it
	// will be responsible for it's own destruction.
	static LLFloaterCompileQueue* create(bool mono);

	// Remove any object in mCurrentScripts with the matching uuid.
	void removeItemByItemID(const LLUUID& item_id);

	void experienceIdsReceived(const LLSD& content);
	bool hasExperience(const LLUUID& id) const;

protected:
	LLFloaterCompileQueue();
	~LLFloaterCompileQueue() override = default;

	// This is called by inventoryChanged
	void handleInventory(LLViewerObject* viewer_obj,
						 LLInventoryObject::object_list_t* inv) override;

	bool startQueue() override;

	static void finishLSLUpload(LLUUID item_id, LLUUID task_id,
								LLUUID new_asset_id, LLSD response,
								std::string script_name, LLUUID queue_id);

	// This is the callback for when each script arrives
	static void scriptArrived(const LLUUID& asset_id, LLAssetType::EType type,
							  void* user_data, S32 status, LLExtStat);

	static void requestAsset(struct LLScriptQueueData* datap,
							 const LLSD& experience);

	static void processExperienceIdResults(LLSD result, LLUUID queue_id);

protected:
	LLViewerInventoryItem::item_array_t mCurrentScripts;

private:
	uuid_list_t			mExperienceIds;
	bool				mMono; // Compile to mono.
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterResetQueue
//
// This script queue resets each script in selection.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterResetQueue final : public LLFloaterScriptQueue
{
public:
	// Use this method to create a reset queue. Once created, it
	// will be responsible for it's own destruction.
	static LLFloaterResetQueue* create();

protected:
	LLFloaterResetQueue();
	~LLFloaterResetQueue() override = default;

	// This is called by inventoryChanged
	void handleInventory(LLViewerObject* viewer_obj,
						 LLInventoryObject::object_list_t* inv) override;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterRunQueue
//
// This script queue runs each script in selection.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterRunQueue final : public LLFloaterScriptQueue
{
public:
	// Use this method to create a run queue. Once created, it
	// will be responsible for it's own destruction.
	static LLFloaterRunQueue* create();

protected:
	LLFloaterRunQueue();
	~LLFloaterRunQueue() override = default;

	// This is called by inventoryChanged
	void handleInventory(LLViewerObject* viewer_obj,
						 LLInventoryObject::object_list_t* inv) override;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterStopQueue
//
// This script queue stops each script in selection.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterStopQueue final : public LLFloaterScriptQueue
{
public:
	// Use this method to create a not run queue. Once created, it
	// will be responsible for it's own destruction.
	static LLFloaterStopQueue* create();

protected:
	LLFloaterStopQueue();
	~LLFloaterStopQueue() override = default;

	// This is called by inventoryChanged
	void handleInventory(LLViewerObject* viewer_obj,
						 LLInventoryObject::object_list_t* inv) override;
};

#endif // LL_LLFLOATERSCRIPTQUEUE_H
