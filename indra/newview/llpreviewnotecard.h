/**
 * @file llpreviewnotecard.h
 * @brief LLPreviewNotecard class declaration
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

#ifndef LL_LLPREVIEWNOTECARD_H
#define LL_LLPREVIEWNOTECARD_H

#include "llassettype.h"
#include "llextendedstatus.h"
#include "hbfileselector.h"

#include "llpreview.h"
#include "llvoinventorylistener.h"

class LLButton;
class HBExternalEditor;
class LLFontGL;
class LLIconCtrl;
class LLLineEditor;
class LLTextEditor;
class LLViewerTextEditor;

// This class allows to edit notecards
class LLPreviewNotecard final : public LLPreview, public LLVOInventoryListener
{
protected:
	LOG_CLASS(LLPreviewNotecard);

public:
	LLPreviewNotecard(const std::string& name, const LLRect& rect,
					  const std::string& title, const LLUUID& item_id,
					  const LLUUID& object_id = LLUUID::null,
					  const LLUUID& asset_id = LLUUID::null,
					  bool show_keep_discard = false,
					  LLPointer<LLViewerInventoryItem> itemp = NULL);
	~LLPreviewNotecard() override;

	// LLPreview override
	bool saveItem(LLPointer<LLInventoryItem>* itemptr) override;

	// LLView overrides
	void draw() override;
	bool handleKeyHere(KEY key, MASK mask) override;
	void setEnabled(bool enabled) override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	// LLFloater override
	bool canClose() override;

	// LLPanel override
	bool postBuild() override;

	const LLInventoryItem* getDragItem();

	// Returns true if there is any embedded inventory.
	bool hasEmbeddedInventory();

	// After saving a notecard, the tcp based upload system will change the
	// asset, therefore, we need to re-fetch it from the asset system. :(
	void refreshFromInventory();

	LL_INLINE LLTextEditor* getEditor()					{ return (LLTextEditor*)mEditor; }

	// LLVOInventoryListener override
	void inventoryChanged(LLViewerObject*, LLInventoryObject::object_list_t*,
						  S32, void*) override;

	static void refreshCachedSettings();

private:
	// LLPreview overrides
	void loadAsset() override;
	LL_INLINE const char* getTitleName() const override	{ return "Note"; }

	bool loadFile(const std::string& filename);
	bool saveFile(std::string& filename);

	bool saveIfNeeded(LLInventoryItem* copyitem = NULL);

	static void finishInventoryUpload(LLUUID item_id, LLUUID new_asset_id,
									  LLUUID new_item_id);
	static void finishTaskUpload(LLUUID item_id, LLUUID new_asset_id);

	void setNoteName(std::string name);

	void setObjectID(const LLUUID& object_id) override;

	static LLPreviewNotecard* getInstance(const LLUUID& uuid);

	static void onLoadComplete(const LLUUID& asset_uuid,
							   LLAssetType::EType type, void* user_data,
							   S32 status, LLExtStat);

	static void onClickSave(void* data);

	static void onSaveComplete(const LLUUID& asset_uuid, void* user_data,
							   S32 status, LLExtStat);

	bool handleSaveChangesDialog(const LLSD& notification,
								 const LLSD& response);

	static void onLoadFromFile(void* userdata);
	static void loadFromFileCallback(HBFileSelector::ELoadFilter,
									 std::string& filename, void* userdata);

	static void onSaveToFile(void* userdata);
	static void saveToFileCallback(HBFileSelector::ESaveFilter,
								   std::string& filename, void* userdata);

	static void onEditedFileChanged(const std::string& filename,
									void* userdata);
	static void onEditExternal(void* userdata);

	static void onSearchMenu(void* userdata);
	static void onUndoMenu(void* userdata);
	static void onRedoMenu(void* userdata);
	static void onCutMenu(void* userdata);
	static void onCopyMenu(void* userdata);
	static void onPasteMenu(void* userdata);
	static void onSelectAllMenu(void* userdata);
	static void onDeselectMenu(void* userdata);
	static void onSpellCheckMenu(void* userdata);

	static bool hasChanged(void* userdata);
	static bool enableSaveLoadFile(void* userdata);

	static bool enableUndoMenu(void* userdata);
	static bool enableRedoMenu(void* userdata);
	static bool enableCutMenu(void* userdata);
	static bool enableCopyMenu(void* userdata);
	static bool enablePasteMenu(void* userdata);
	static bool enableSelectAllMenu(void* userdata);
	static bool enableDeselectMenu(void* userdata);
	static bool enableSpellCheckMenu(void* userdata);

	static bool checkSpellCheckMenu(void* userdata);

private:
	LLButton*							mSaveButton;
	LLIconCtrl*							mLockIcon;
	LLLineEditor*						mDescription;
	LLViewerTextEditor*					mEditor;

	HBExternalEditor*					mExternalEditor;

	LLUUID								mAssetID;
	LLUUID								mNotecardItemID;
	LLUUID								mObjectID;

	std::string							mNoteName;
	std::string							mTempFilename;

	bool								mShowKeepDiscard;

	static std::set<LLPreviewNotecard*>	sInstances;
	static LLFontGL*					sCustomFont;
};

#endif // LL_LLPREVIEWNOTECARD_H
