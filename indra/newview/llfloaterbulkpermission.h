/**
 * @file llfloaterbulkpermissions.h
 * @brief Allow multiple task inventory properties to be set in one go.
 * @author Michelle2 Zenovka
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

#ifndef LL_LLBULKPERMISSION_H
#define LL_LLBULKPERMISSION_H

#include <vector>

#include "llfloater.h"
#include "llinventory.h"

#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llvoinventorylistener.h"

class LLScrollListCtrl;

class LLFloaterBulkPermission final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterBulkPermission>,
	public LLVOInventoryListener
{
protected:
	LOG_CLASS(LLFloaterBulkPermission);

public:
	LLFloaterBulkPermission(const LLSD& seed);

private:
	~LLFloaterBulkPermission() override			{}

    bool start();	// Returns true if the queue has started, otherwise false.
    bool nextObject();
    bool popNext();

	// This is the callback method for the viewer object currently being worked
	// on.
	void inventoryChanged(LLViewerObject* obj,
						  LLInventoryObject::object_list_t* inv,
						  S32 serial_num, void* queue) override;

	// This is called by inventoryChanged
	void handleInventory(LLViewerObject* viewer_obj,
						 LLInventoryObject::object_list_t* inv);

	void updateInventory(LLViewerObject* object, LLViewerInventoryItem* item);

	// Read the settings and Apply the permissions
	void doApply();

	// Returns true if all permission changes are done
	LL_INLINE bool isDone() const
	{
		return mCurrentObjectID.isNull() || mObjectIDs.size() == 0;
	}

private:
	static void setAllChecked(bool check);
	LL_INLINE static void onCheckAll(void*)		{ setAllChecked(true); }
	LL_INLINE static void onUncheckAll(void*)	{ setAllChecked(false); }

	static void onHelpBtn(void* user_data);
	static void onCloseBtn(void* user_data);
	static void onApplyBtn(void* user_data);
	static void onCommitCopy(LLUICtrl* ctrl, void* data);

private:
	LLUUID		mID;
	LLUUID		mCurrentObjectID;
	uuid_vec_t	mObjectIDs;			// Object Queue
	bool		mDone;
};

#endif
