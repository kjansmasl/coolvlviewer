/**
 * @file hbfloaterthumbnail.h
 * @author Henri Beauchamp
 * @brief HBFloaterThumbnail class declaration
 *
 * $LicenseInfo:firstyear=2023&license=viewergpl$
 *
 * Copyright (c) 2023, Henri Beauchamp.
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

#ifndef LL_HBFLOATERTHUMBNAIL_H
#define LL_HBFLOATERTHUMBNAIL_H

#include "hbfastmap.h"
#include "llfloater.h"

class LLButton;
class LLIconCtrl;
class LLImageRaw;
class LLInventoryObject;
class LLTextBox;
class LLScrollListItem;
class HBThumbnailDropTarget;
class LLViewerFetchedTexture;
class LLViewerInventoryItem;

class HBFloaterThumbnail final : public LLFloater
{
	friend class HBThumbnailDropTarget;

protected:
	LOG_CLASS(HBFloaterThumbnail);

public:
	LL_INLINE bool isForViewOnly() const		{ return mForViewOnly; }

	// Note: here the 'id' is either the agent's inventory item Id, or the
	// inventory item Id XORed with its container object (task) Id.
	static HBFloaterThumbnail* findInstance(const LLUUID& id);

	// When 'for_view' is true, show the (unique) temporary floater without
	// controls.
	static void showInstance(const LLUUID& inv_obj_id,
							 const LLUUID& task_id = LLUUID::null,
							 bool for_view = false);
	// Omitting 'id' (or passing a null UUID) causes this call to close the
	// (unique) temporary thumbnail view floater. If the floater is not the
	// temporary one and got unsaved changes, it is not closed. Note that 'id'
	// is either the agent's inventory object Id, or the task_id XORed with
	// the Id of the item it contains and with which the thumbnail is
	// associated.
	static void hideInstance(const LLUUID& id = LLUUID::null);

	// Note: the raw image may be modified (scaled down) by this method.
	static void uploadThumbnail(const LLUUID& inv_obj_id,
								LLPointer<LLImageRaw> rawp);

private:
	// Use showInstance() only
	HBFloaterThumbnail(const LLUUID& inv_obj_id, const LLUUID& task_id,
					   bool for_view);
	~HBFloaterThumbnail() override;

	void unregister();

	// LLFloater overrides
	bool postBuild() override;
	void draw() override;

	void updateDropTarget();

	void setInventoryObjectId(const LLUUID& inv_obj_id);
	LLInventoryObject* getInventoryObject();

	void setThumbTexture();
	void setThumbnail();

	void uploadFailure(const std::string& reason);

	static void uploadThumbnailCoro(std::string url, LLSD data, LLUUID id);

	void onChoosenTexture(LLViewerInventoryItem* itemp, bool final_choice);

	static void onBtnChange(LLUICtrl* ctrlp, void* userdata);
	static void onBtnCancel(void* userdata);
	static void onBtnClose(void* userdata);

private:
	LLUUID								mTaskId;
	LLUUID								mInventoryObjectId;
	LLUUID								mInitialThumbnailId;
	LLUUID								mThumbnailId;
	LLUUID								mTempThumbId;
	HBThumbnailDropTarget*				mDropTarget;
	LLIconCtrl*							mIcon;
	LLTextBox*							mInventoryObjectName;
	LLButton*							mCancelButton;
	LLScrollListItem*					mCopyThumbnail;
	LLScrollListItem*					mPasteThumbnail;
	LLScrollListItem*					mClearThumbnail;
	LLScrollListItem*					mUndoThumbnail;
	LLPointer<LLViewerFetchedTexture>	mTexturep;
	LLRect								mThumbnailRect;
	std::string							mTempFilename;
	bool								mForViewOnly;
	bool								mMustClose;
	bool								mIsCategory;

	typedef fast_hmap<LLUUID, HBFloaterThumbnail*> instances_map_t;
	static instances_map_t				sInstances;
};

#endif	// LL_HBFLOATERTHUMBNAIL_H
