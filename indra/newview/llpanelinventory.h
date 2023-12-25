/**
 * @file llpanelinventory.h
 * @brief LLPanelInventory class definition
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

// ****************************************************************************
// This class represents the panel used to view and control a particular task's
// inventory.
// ****************************************************************************

#ifndef LL_LLPANELINVENTORY_H
#define LL_LLPANELINVENTORY_H

#include <string>
#include <vector>

#include "llinventory.h"
#include "llpanel.h"
#include "lluuid.h"

#include "llviewerobject.h"
#include "llvoinventorylistener.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPanelInventory
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuGL;
class LLFolderView;
class LLFolderViewEventListener;
class LLFolderViewFolder;
class LLScrollableContainer;
class LLViewerObject;

// Utility function to hide all entries except those in the list
void hideContextEntries(LLMenuGL& menu,
						const std::vector<std::string>& entries_to_show,
						const std::vector<std::string>& disabled_entries);

class LLPanelInventory final : public LLPanel, public LLVOInventoryListener
{
protected:
	LOG_CLASS(LLPanelInventory);

protected:
	void reset();
	void inventoryChanged(LLViewerObject* object,
						  LLInventoryObject::object_list_t* invp, S32,
						  void*) override;
	void updateInventory();

	// These two methods return the number of total views (inventory items and
	// sub-categories) created in the category. 
	U32 createFolderViews(LLInventoryObject* inventory_root,
						  LLInventoryObject::object_list_t& contents);
	U32 createViewsForCategory(LLInventoryObject::object_list_t* inventory,
							   LLInventoryObject* parent,
							   LLFolderViewFolder* folder);

	void clearContents();

public:
	LLPanelInventory(const std::string& name, const LLRect& rect);
	~LLPanelInventory() override;

	void refresh() override;

	LL_INLINE const LLUUID& getTaskUUID()			{ return mTaskUUID;}
	void removeSelectedItem();
	void startRenamingSelectedItem();

	LL_INLINE LLFolderView* getRootFolder() const	{ return mFolders; }

	LL_INLINE U32 getViewsCount() const				{ return mItemsCount; }

	void draw() override;
	void deleteAllChildren() override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;

	static void idle(void* user_data);

protected:
	LLScrollableContainer*	mScroller;
	LLFolderView*			mFolders;

	LLUUID					mTaskUUID;
	LLUUID					mAttachmentUUID;

	U32						mItemsCount;

	bool					mHaveInventory;
	bool					mIsInventoryEmpty;
	bool					mInventoryNeedsUpdate;
};

#endif // LL_LLPANELINVENTORY_H
