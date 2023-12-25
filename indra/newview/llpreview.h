/**
 * @file llpreview.h
 * @brief LLPreview class definition
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

#ifndef LL_LLPREVIEW_H
#define LL_LLPREVIEW_H

#include <map>					// For multimap

#include "llfloater.h"
#include "llresizehandle.h"
#include "lltabcontainer.h"

#include "llinventorymodel.h"
#include "llviewerinventory.h"

class LLLineEditor;
class LLRadioGroup;
class LLPreview;

class LLMultiPreview : public LLMultiFloater
{
public:
	LLMultiPreview(const LLRect& rect);

	void open() override;
	void tabOpen(LLFloater* opened_floater, bool from_click) override;
	void userSetShape(const LLRect& new_rect) override;

	static LLMultiPreview* getAutoOpenInstance(const LLUUID& id);
	static void setAutoOpenInstance(LLMultiPreview* previewp,
									const LLUUID& id);

protected:
	typedef fast_hmap<LLUUID, LLHandle<LLFloater> > handle_map_t;
	static handle_map_t sAutoOpenPreviewHandles;
};

class LLPreview : public LLFloater, LLInventoryObserver
{
protected:
	LOG_CLASS(LLPreview);

public:
	typedef enum e_asset_status
	{
		PREVIEW_ASSET_ERROR,
		PREVIEW_ASSET_UNLOADED,
		PREVIEW_ASSET_LOADING,
		PREVIEW_ASSET_LOADED
	} EAssetStatus;

	// Used for XML-based construction.
	LLPreview(const std::string& name);
	LLPreview(const std::string& name, const LLRect& rect,
			  const std::string& title, const LLUUID& item_uuid,
			  const LLUUID& object_uuid, bool allow_resize = false,
			  S32 min_width = 0, S32 min_height = 0,
			  LLPointer<LLViewerInventoryItem> inv_item = NULL);
	~LLPreview() override;

	LL_INLINE virtual void setObjectID(const LLUUID& object_id)
	{
		mObjectUUID = object_id;
	}

	virtual void setItemID(const LLUUID& item_id);
	void setAssetId(const LLUUID& asset_id);
	// Searches if not constructed with it
	const LLViewerInventoryItem* getItem() const;

	static LLPreview* find(const LLUUID& item_uuid);
	static LLPreview* show(const LLUUID& item_uuid, bool take_focus = true);
	static void	hide(const LLUUID& item_uuid, bool no_saving = false);
	static void	rename(const LLUUID& item_uuid, const std::string& new_name);
	static bool	save(const LLUUID& item_uuid,
					 LLPointer<LLInventoryItem>* itemptr);

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	void open() override;
	virtual bool saveItem(LLPointer<LLInventoryItem>* itemptr);

	virtual void setAuxItem(const LLInventoryItem* itemp);

	static void onBtnCopyToInv(void* userdata);

	void addKeepDiscardButtons();
	static void onKeepBtn(void* data);
	static void onDiscardBtn(void* data);
	void userSetShape(const LLRect& new_rect) override;

	void userResized()							{ mUserResized = true; };

	virtual void loadAsset()					{ mAssetStatus = PREVIEW_ASSET_LOADED; }
	virtual EAssetStatus getAssetStatus()		{ return mAssetStatus;}

	void setNotecardInfo(const LLUUID& notecard_inv_id,
						 const LLUUID& object_id);

	void draw() override;

	virtual void refreshFromItem();

	// We cannot modify item or description in preview if either in-world object
	// or item itself is unmodifiable.
	static bool canModify(const LLUUID& task_id, const LLInventoryItem* itemp);

protected:
	void onCommit() override;

	void addDescriptionUI();

	static void onText(LLUICtrl*, void* userdata);
	static void onRadio(LLUICtrl*, void* userdata);

	// LLInventoryObserver override
	void changed(U32 mask) override;

	virtual const char* getTitleName() const	{ return "Preview"; }

protected:
	LLUUID mItemUUID;

	// mObjectID will have a value if it is associated with a rezzed object
	// (task), and will be LLUUID::null if it is in the agent inventory.
	LLUUID mObjectUUID;

	LLRect mClientRect;

	LLPointer<LLInventoryItem> mAuxItem;  // HACK!
	LLButton* mCopyToInvBtn;

	// Close without saving changes
	bool mForceClose;

	bool mDirty;

	bool mUserResized;

	// When closing springs a "Want to save ?" dialog, we want to keep the
	// preview open until the save completes.
	bool mCloseAfterSave;

	// True if the save changes confirmation dialog was already shown
	bool mSaveDialogShown;

	EAssetStatus mAssetStatus;

	typedef fast_hmap<LLUUID, LLPreview*> preview_map_t;

	static preview_map_t sInstances;
	LLUUID mNotecardInventoryID;
	LLUUID mObjectID;
	LLPointer<LLViewerInventoryItem> mItem;
};

constexpr S32 PREVIEW_BORDER = 4;
constexpr S32 PREVIEW_PAD = 5;
constexpr S32 PREVIEW_BUTTON_WIDTH = 100;

constexpr S32 PREVIEW_LINE_HEIGHT = 19;
constexpr S32 PREVIEW_CLOSE_BOX_SIZE = 16;
constexpr S32 PREVIEW_BORDER_WIDTH = 2;
constexpr S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) +
										   PREVIEW_BORDER_WIDTH;
constexpr S32 PREVIEW_VPAD = 2;
constexpr S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
constexpr S32 PREVIEW_HEADER_SIZE = 2 * PREVIEW_LINE_HEIGHT + 2 * PREVIEW_VPAD;

#endif  // LL_LLPREVIEW_H
