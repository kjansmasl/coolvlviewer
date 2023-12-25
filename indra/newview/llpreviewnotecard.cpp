/**
 * @file llpreviewnotecard.cpp
 * @brief Implementation of the notecard editor
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

#include "llpreviewnotecard.h"

#include "llassetstorage.h"
#include "llbutton.h"
#include "lldir.h"
#include "hbexternaleditor.h"
#include "llfilesystem.h"
#include "llfontgl.h"
#include "lliconctrl.h"
#include "llinventory.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llscrollbar.h"
#include "llspellcheck.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llappviewer.h"				// For abortQuit()
#include "llfloatersearchreplace.h"
#include "llinventorymodel.h"
#include "llselectmgr.h"
#include "llviewerassetupload.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"

constexpr S32 PREVIEW_MIN_WIDTH =	2 * PREVIEW_BORDER +
									2 * PREVIEW_BUTTON_WIDTH +
									2 * PREVIEW_PAD + RESIZE_HANDLE_WIDTH;
constexpr S32 PREVIEW_MIN_HEIGHT =	2 * PREVIEW_BORDER +
									3 * (20 + PREVIEW_PAD) +
									2 * SCROLLBAR_SIZE + 128;

std::set<LLPreviewNotecard*> LLPreviewNotecard::sInstances;
LLFontGL* LLPreviewNotecard::sCustomFont = NULL;

LLPreviewNotecard::LLPreviewNotecard(const std::string& name,
									 const LLRect& rect,
									 const std::string& title,
									 const LLUUID& item_id,
									 const LLUUID& object_id,
									 const LLUUID& asset_id,
									 bool show_keep_discard,
									 LLPointer<LLViewerInventoryItem> inv_item)
:	LLPreview(name, rect, title, item_id, object_id, true, PREVIEW_MIN_WIDTH,
			  PREVIEW_MIN_HEIGHT, inv_item),
	mExternalEditor(NULL),
	mAssetID(asset_id),
	mNotecardItemID(item_id),
	mObjectID(object_id),
	mShowKeepDiscard(show_keep_discard)
{
	sInstances.insert(this);

	LLRect curRect = rect;

	std::string file;
	if (!show_keep_discard && mAssetID.isNull())
	{
		const LLInventoryItem* item = getItem();
		if (item)
		{
			mAssetID = item->getAssetUUID();
		}
	}
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_preview_notecard.xml");

	// Only assert shape if not hosted in a multifloater
	if (!getHost())
	{
		reshape(curRect.getWidth(), curRect.getHeight());
		setRect(curRect);
	}

	setTitle(title);
	setNoteName(title);
}

//virtual
LLPreviewNotecard::~LLPreviewNotecard()
{
	sInstances.erase(this);
	if (mExternalEditor)
	{
		delete mExternalEditor;
	}
	if (!mTempFilename.empty())
	{
		LLFile::remove(mTempFilename);
	}
}

//virtual
bool LLPreviewNotecard::postBuild()
{
	mEditor = getChild<LLViewerTextEditor>("text_edit");
	mEditor->setWordWrap(true);
	mEditor->setSourceID(mNotecardItemID);
	mEditor->setHandleEditKeysDirectly(true);
	mEditor->setNotecardInfo(mNotecardItemID, mObjectID);
	mEditor->makePristine();
	// Use separate and possibly different colors for the note card editor. HB
	LLColor4 color = gColors.getColor("TextFgNotecardColor");
	mEditor->setFgColor(color);
	mEditor->setTextDefaultColor(color);
	mEditor->setReadOnlyFgColor(gColors.getColor("TextFgNotecardReadOnlyColor"));
	mEditor->setWriteableBgColor(gColors.getColor("TextBgNotecardColor"));
	mEditor->setReadOnlyBgColor(gColors.getColor("TextBgNotecardReadOnlyColor"));
	if (sCustomFont)
	{
		mEditor->setFont(sCustomFont);
	}

	mDescription = getChild<LLLineEditor>("desc");
	mDescription->setCommitCallback(LLPreview::onText);
	mDescription->setCallbackUserData(this);
	mDescription->setPrevalidate(LLLineEditor::prevalidatePrintableNotPipe);
	const LLInventoryItem* inv_item = getItem();
	if (inv_item)
	{
		mDescription->setText(inv_item->getDescription());
	}

	mSaveButton = getChild<LLButton>("save_btn");
	mSaveButton->setClickedCallback(onClickSave, this);

	if (mShowKeepDiscard)
	{
		childSetAction("keep_btn", onKeepBtn, this);
		childSetAction("discard_btn", onDiscardBtn, this);
	}
	else
	{
		childSetVisible("keep_btn", false);
		childSetVisible("discard_btn", false);
	}

	mLockIcon = getChild<LLIconCtrl>("lock");
	mLockIcon->setVisible(false);

	LLMenuItemCallGL* item = getChild<LLMenuItemCallGL>("load");
	item->setMenuCallback(onLoadFromFile, this);
	item->setEnabledCallback(enableSaveLoadFile);

	item = getChild<LLMenuItemCallGL>("save");
	item->setMenuCallback(onSaveToFile, this);
	item->setEnabledCallback(enableSaveLoadFile);

	item = getChild<LLMenuItemCallGL>("external");
	item->setMenuCallback(onEditExternal, this);
	item->setEnabledCallback(enableSaveLoadFile);

	item = getChild<LLMenuItemCallGL>("undo");
	item->setMenuCallback(onUndoMenu, this);
	item->setEnabledCallback(enableUndoMenu);

	item = getChild<LLMenuItemCallGL>("redo");
	item->setMenuCallback(onRedoMenu, this);
	item->setEnabledCallback(enableRedoMenu);

	item = getChild<LLMenuItemCallGL>("cut");
	item->setMenuCallback(onCutMenu, this);
	item->setEnabledCallback(enableCutMenu);

	item = getChild<LLMenuItemCallGL>("copy");
	item->setMenuCallback(onCopyMenu, this);
	item->setEnabledCallback(enableCopyMenu);

	item = getChild<LLMenuItemCallGL>("paste");
	item->setMenuCallback(onPasteMenu, this);
	item->setEnabledCallback(enablePasteMenu);

	item = getChild<LLMenuItemCallGL>("select_all");
	item->setMenuCallback(onSelectAllMenu, this);
	item->setEnabledCallback(enableSelectAllMenu);

	item = getChild<LLMenuItemCallGL>("deselect");
	item->setMenuCallback(onDeselectMenu, this);
	item->setEnabledCallback(enableDeselectMenu);

	item = getChild<LLMenuItemCallGL>("search");
	item->setMenuCallback(onSearchMenu, this);
	item->setEnabledCallback(NULL);

	LLMenuItemCheckGL* citem = getChild<LLMenuItemCheckGL>("spelling");
	citem->setMenuCallback(onSpellCheckMenu, this);
	citem->setEnabledCallback(enableSpellCheckMenu);
	citem->setCheckCallback(checkSpellCheckMenu);
	citem->setValue(false);

	// Tell LLEditMenuHandler about our editor type: this will trigger a Lua
	// callback if one is configured for context menus. HB
	mEditor->setCustomMenuType("notecard");

	return true;
}

//static
void LLPreviewNotecard::refreshCachedSettings()
{
	std::string font_name = gSavedSettings.getString("NotecardEditorFont");
	if (font_name.empty())
	{
		sCustomFont = NULL;
	}
	else
	{
		sCustomFont = LLFontGL::getFont(font_name.c_str());
	}
}

//virtual
void LLPreviewNotecard::draw()
{
	mSaveButton->setEnabled(getEnabled() && !mEditor->isPristine());
	LLPreview::draw();
}

void LLPreviewNotecard::setNoteName(std::string name)
{
	if (name.find("Note: ") == 0)
	{
		name = name.substr(6);
	}
	if (name.empty())
	{
		name = "untitled";
	}
	mNoteName = name;
}

//virtual
void LLPreviewNotecard::setObjectID(const LLUUID& object_id)
{
	LLPreview::setObjectID(object_id);
	mEditor->setNotecardObjectID(mObjectUUID);
	mEditor->makePristine();
}

bool LLPreviewNotecard::saveItem(LLPointer<LLInventoryItem>* itemptr)
{
	LLInventoryItem* item = NULL;
	if (itemptr && itemptr->notNull())
	{
		item = (LLInventoryItem*)(*itemptr);
	}
	bool res = saveIfNeeded(item);
	if (res)
	{
		delete itemptr;
	}
	return res;
}

void LLPreviewNotecard::setEnabled(bool enabled)
{
	mEditor->setEnabled(enabled);
	mLockIcon->setVisible(!enabled);
	mDescription->setEnabled(enabled);
	mSaveButton->setEnabled(enabled && !mEditor->isPristine());
}

//virtual
bool LLPreviewNotecard::handleKeyHere(KEY key, MASK mask)
{
	if (mask == MASK_CONTROL)
	{
		if (key == 'S')
		{
			saveIfNeeded();
			return true;
		}

		if (key == 'F')
		{
			LLFloaterSearchReplace::show(mEditor);
			return true;
		}
	}

	return LLPreview::handleKeyHere(key, mask);
}

//virtual
bool LLPreviewNotecard::canClose()
{
	if (mForceClose || mEditor->isPristine())
	{
		return true;
	}

	if (!mSaveDialogShown)
	{
		mSaveDialogShown = true;
		// Bring up view-modal dialog: Save changes ? Yes, No, Cancel
		gNotifications.add("SaveChanges", LLSD(), LLSD(),
						   boost::bind(&LLPreviewNotecard::handleSaveChangesDialog,
									   this, _1, _2));
	}
	return false;
}

//virtual
void LLPreviewNotecard::inventoryChanged(LLViewerObject*,
										 LLInventoryObject::object_list_t*,
										 S32, void*)
{
	removeVOInventoryListener();
	loadAsset();
}

const LLInventoryItem* LLPreviewNotecard::getDragItem()
{
	return mEditor->getDragItem();
}

bool LLPreviewNotecard::hasEmbeddedInventory()
{
	return mEditor->hasEmbeddedInventory();
}

void LLPreviewNotecard::refreshFromInventory()
{
	LL_DEBUGS("Notecard") << "Refreshing from inventory" << LL_ENDL;
	loadAsset();
}

void LLPreviewNotecard::loadAsset()
{
	// Request the asset.
	const LLInventoryItem* item = getItem();
	if (!item)
	{
		if (mObjectUUID.notNull() && mItemUUID.notNull())
		{
			LLViewerObject* objectp = gObjectList.findObject(mObjectUUID);
			if (objectp &&
				(objectp->isInventoryPending() || objectp->isInventoryDirty()))
			{
				// It is a notecard in an object inventory and we failed to get
				// it because inventory is not up to date. Subscribe for
				// callback and retry at inventoryChanged().
				// This also removes any previous listener:
				registerVOInventoryListener(objectp, NULL);
				if (objectp->isInventoryDirty())
				{
					objectp->requestInventory();
				}
				return;
			}
		}
		mEditor->setText(LLStringUtil::null);
		mEditor->makePristine();
		mEditor->setEnabled(true);
#if 0	// Do not set the asset status here: we may not have set the item Id
		// yet (e.g. when this gets called initially)
		mAssetStatus = PREVIEW_ASSET_LOADED;
#endif
		return;
	}

	if (gAgent.isGodlike() ||
		gAgent.allowOperation(PERM_COPY, item->getPermissions(),
							  GP_OBJECT_MANIPULATE))
	{
		mAssetID = item->getAssetUUID();
		if (mAssetID.isNull())
		{
			mEditor->setText(LLStringUtil::null);
			mEditor->makePristine();
			mEditor->setEnabled(true);
			mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		else if (gAssetStoragep)
		{
			LLHost source_sim = LLHost();
			if (mObjectUUID.notNull())
			{
				LLViewerObject* objectp = gObjectList.findObject(mObjectUUID);
				if (objectp && objectp->getRegion())
				{
					source_sim = objectp->getRegion()->getHost();
				}
				else
				{
					// The object that we are trying to look at
					// disappeared: bail out.
					llwarns << "Cannot find object " << mObjectUUID
							<< " associated with notecard." << llendl;
					mAssetID.setNull();
					mEditor->setText(getString("no_object"));
					mEditor->makePristine();
					mEditor->setEnabled(false);
					mAssetStatus = PREVIEW_ASSET_LOADED;
					return;
				}
			}
			gAssetStoragep->getInvItemAsset(source_sim,
											gAgentID, gAgentSessionID,
											item->getPermissions().getOwner(),
											mObjectUUID, item->getUUID(),
											item->getAssetUUID(),
											item->getType(), onLoadComplete,
											(void*)new LLUUID(mItemUUID),
											true);
			mAssetStatus = PREVIEW_ASSET_LOADING;
		}
	}
	else
	{
		mAssetID.setNull();
		mEditor->setText(getString("not_allowed"));
		mEditor->makePristine();
		mEditor->setEnabled(false);
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}

	if (!canModify(mObjectUUID, item))
	{
		mEditor->setEnabled(false);
		mLockIcon->setVisible(true);
	}
}

bool LLPreviewNotecard::loadFile(const std::string& filename)
{
	if (filename.empty())
	{
		return false;
	}
	std::ifstream file(filename.c_str());
	if (file.fail())
	{
		return false;
	}

	mEditor->clear();
	std::string line, text;
	while (!file.eof())
	{
		std::getline(file, line);
		text += line + "\n";
	}
	file.close();
	LLWString wtext = utf8str_to_wstring(text);
	LLWStringUtil::replaceTabsWithSpaces(wtext, 4);
	text = wstring_to_utf8str(wtext);
	mEditor->setText(text);

	return true;
}

bool LLPreviewNotecard::saveFile(std::string& filename)
{
	if (filename.empty())
	{
		return false;
	}

	std::string lcname = filename;
	LLStringUtil::toLower(lcname);
	if (lcname.find(".txt") != lcname.length() - 4)
	{
		filename += ".txt";
	}

	std::ofstream file(filename.c_str());
	if (file.fail())
	{
		LLSD args;
		args["FILE"] = filename;
		gNotifications.add("CannotWriteFile", args);
		return false;
	}

	file << mEditor->getText();
	file.close();

	return true;
}

//static
void LLPreviewNotecard::onLoadComplete(const LLUUID& asset_id,
									   LLAssetType::EType type, void* userdata,
									   S32 status, LLExtStat)
{
	LLUUID* item_id = (LLUUID*)userdata;
	LLPreviewNotecard* self = LLPreviewNotecard::getInstance(*item_id);
	delete item_id;
	if (!self)
	{
		return;
	}

	if (status)
	{
		gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);

		if (status == LL_ERR_FILE_EMPTY ||
			status == LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE)
		{
			gNotifications.add("NotecardMissing");
		}
		else if (status == LL_ERR_INSUFFICIENT_PERMISSIONS)
		{
			gNotifications.add("NotecardNoPermissions");
		}
		else
		{
			gNotifications.add("UnableToLoadNotecard");
		}
		self->mAssetStatus = PREVIEW_ASSET_ERROR;
		return;
	}

	S32 pos = 0;
	if (self->mEditor->getLength() > 0)
	{
		pos = self->mEditor->getCursorPos();
	}

	LLFileSystem file(asset_id);
	S32 file_length = file.getSize();
	char* bufferp = new char[file_length + 1];
	file.read((U8*)bufferp, file_length);
	// Put a EOS at the end
	bufferp[file_length] = 0;
	if (file_length > 19 && !strncmp(bufferp, "Linden text version", 19))
	{
		if (!self->mEditor->importBuffer(bufferp, file_length + 1))
		{
			llwarns << "Problem importing notecard" << llendl;
		}
	}
	else
	{
		// Version 0 (just text, does not include version number)
		self->mEditor->setText(std::string(bufferp));
	}
	delete[] bufferp;

	self->mEditor->makePristine();
	if (pos > 0)
	{
		self->mEditor->setCursorPos(pos);
		self->mEditor->scrollToPos(pos);
	}

	bool modifiable = canModify(self->mObjectUUID, self->getItem());
	self->setEnabled(modifiable);
	self->mAssetStatus = PREVIEW_ASSET_LOADED;
}

//static
LLPreviewNotecard* LLPreviewNotecard::getInstance(const LLUUID& item_id)
{
	preview_map_t::iterator it = LLPreview::sInstances.find(item_id);
	return it != LLPreview::sInstances.end() ? (LLPreviewNotecard*)it->second
											 : NULL;
}

//static
void LLPreviewNotecard::onClickSave(void* user_data)
{
	LLPreviewNotecard* preview = (LLPreviewNotecard*)user_data;
	if (preview)
	{
		preview->saveIfNeeded();
	}
}

struct LLSaveNotecardInfo
{
	LLSaveNotecardInfo(LLPreviewNotecard* self, const LLUUID& item_id,
					   const LLUUID& object_id, const LLTransactionID& tid,
					   LLInventoryItem* copyitem)
	:	mSelf(self),
		mItemUUID(item_id),
		mObjectUUID(object_id),
		mTransactionID(tid),
		mCopyItem(copyitem)
	{
	}

	LLPreviewNotecard*			mSelf;
	LLPointer<LLInventoryItem>	mCopyItem;
	LLUUID						mItemUUID;
	LLUUID						mObjectUUID;
	LLTransactionID				mTransactionID;
};

bool LLPreviewNotecard::saveIfNeeded(LLInventoryItem* copyitem)
{
	if (mEditor->isPristine())
	{
		return true;
	}

	std::string buffer;
	if (!mEditor->exportBuffer(buffer))
	{
		return false;
	}
	mEditor->makePristine();

	if (mExternalEditor && mExternalEditor->running() && !mTempFilename.empty())
	{
		// Do not cause a file changed event for something we trigger ourselves
		// (the external editor will cause a file access read event, which is
		// considered a changed event, and would cause HBExternalEditor to call
		// our own changed file event, which we do not want to happen here).
		mExternalEditor->ignoreNextUpdate();
		saveFile(mTempFilename);
	}

	// Save it out to database
	const LLInventoryItem* item = getItem();
	if (!item)
	{
		return true;
	}

	// First try via HTTP capabilities.
	const std::string* urlp = NULL;
	LLResourceUploadInfo::ptr_t infop;
	if (mObjectUUID.isNull())
	{
		const std::string& inv_url =
			gAgent.getRegionCapability("UpdateNotecardAgentInventory");
		if (!inv_url.empty())
		{
			// Saving into agent inventory
			urlp = &inv_url;
			infop = std::make_shared<LLBufferedAssetUploadInfo>(
						mItemUUID, LLAssetType::AT_NOTECARD, buffer,
						boost::bind(&LLPreviewNotecard::finishInventoryUpload,
									_1, _2, _3));
		}
	}
	else
	{
		const std::string& task_url =
			gAgent.getRegionCapability("UpdateNotecardTaskInventory");
		if (!task_url.empty())
		{
			// Saving into task inventory
			urlp = &task_url;
			infop = std::make_shared<LLBufferedAssetUploadInfo>(
						mObjectUUID, mItemUUID, LLAssetType::AT_NOTECARD,
						buffer,
						boost::bind(&LLPreviewNotecard::finishTaskUpload,
									_1, _3));
		}
	}
	if (urlp && infop)
	{
		mAssetStatus = PREVIEW_ASSET_LOADING;
		setEnabled(false);
		LLViewerAssetUpload::enqueueInventoryUpload(*urlp, infop);
		return true;
	}

	// Legacy UDP upload path.
	if (gAssetStoragep)
	{
		// We need to update the asset information
		LLTransactionID tid;
		tid.generate();
		LLAssetID asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

		LLFileSystem file(asset_id, LLFileSystem::APPEND);
		S32 size = buffer.length() + 1;
		if (!file.write((U8*)buffer.c_str(), size))
		{
			llwarns << "Failure to write cache file for asset: " << mAssetID
					<< llendl;
			return false;
		}

		LLSaveNotecardInfo* info = new LLSaveNotecardInfo(this, mItemUUID,
														  mObjectUUID, tid,
														  copyitem);
		gAssetStoragep->storeAssetData(tid, LLAssetType::AT_NOTECARD,
									   onSaveComplete, (void*)info, false);
		return true;
	}

	llwarns << "No capability neither asset storage system. Could not save notecard: "
			<< mAssetID << llendl;

	return false;
}

//static
void LLPreviewNotecard::finishTaskUpload(LLUUID item_id, LLUUID new_asset_id)
{
	LLPreviewNotecard* self = LLPreviewNotecard::getInstance(item_id);
	if (self)
	{
		if (self->hasEmbeddedInventory())
		{
			LLFileSystem::removeFile(new_asset_id);
		}
		self->setAssetId(new_asset_id);
		self->refreshFromInventory();
	}
}

//static
void LLPreviewNotecard::finishInventoryUpload(LLUUID item_id,
											  LLUUID new_asset_id,
											  LLUUID new_item_id)
{
	// Update the UI with the new asset.
	LLPreviewNotecard* self = LLPreviewNotecard::getInstance(item_id);
	if (self)
	{
		// *HACK: we have to delete the asset in the cache so that the viewer
		// will re-download it. This is only really necessary if the asset had
		// to be modified by the uploader, so this can be optimized away in
		// some cases. A better design is to have a new uuid if the notecard
		// actually changed the asset.
		if (self->hasEmbeddedInventory())
		{
			LLFileSystem::removeFile(new_asset_id);
		}
		if (new_item_id.isNull())
		{
			self->setAssetId(new_asset_id);
			self->refreshFromInventory();
		}
		else
		{
			self->setItemID(new_item_id);
			self->refreshFromInventory();
		}
	}
}

//static
void LLPreviewNotecard::onSaveComplete(const LLUUID& asset_id, void* user_data,
									   S32 status, LLExtStat)
{
	LLSaveNotecardInfo* info = (LLSaveNotecardInfo*)user_data;
	if (info && status == 0)
	{
		if (info->mObjectUUID.isNull())
		{
			LLViewerInventoryItem* item;
			item = (LLViewerInventoryItem*)gInventory.getItem(info->mItemUUID);
			if (item)
			{
				LLPointer<LLViewerInventoryItem> new_item =
					new LLViewerInventoryItem(item);
				new_item->setAssetUUID(asset_id);
				new_item->setTransactionID(info->mTransactionID);
				new_item->updateServer(false);
				gInventory.updateItem(new_item);
				gInventory.notifyObservers();
			}
			else
			{
				llwarns << "Inventory item for notecard " << info->mItemUUID
						<< " is no longer in agent inventory." << llendl;
			}
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(info->mObjectUUID);
			LLViewerInventoryItem* item = NULL;
			if (object)
			{
				LLInventoryObject* inv_obj =
					object->getInventoryObject(info->mItemUUID);
				item = (LLViewerInventoryItem*)inv_obj;
			}
			if (item)
			{
				item->setAssetUUID(asset_id);
				item->setTransactionID(info->mTransactionID);
				object->updateInventory(item);
				dialog_refresh_all();
			}
			else
			{
				gNotifications.add("SaveNotecardFailObjectNotFound");
			}
		}
		// Perform item copy to inventory
		if (info->mCopyItem.notNull() && info->mSelf && info->mSelf->mEditor)
		{
			info->mSelf->mEditor->copyInventory(info->mCopyItem);
		}

		// Find our window and close it if requested.
		LLPreviewNotecard* previewp =
				(LLPreviewNotecard*)LLPreview::find(info->mItemUUID);
		if (previewp && previewp->mCloseAfterSave)
		{
			previewp->close();
		}
	}
	else
	{
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		gNotifications.add("SaveNotecardFailReason", args);
	}

	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														  asset_id.asString());
	LLFile::remove(filename + ".tmp");
	delete info;
}

bool LLPreviewNotecard::handleSaveChangesDialog(const LLSD& notification,
												const LLSD& response)
{
	mSaveDialogShown = false;

	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:  // "Yes"
			mCloseAfterSave = true;
			onClickSave((void*)this);
			break;

		case 1:  // "No"
			mForceClose = true;
			close();
			break;

		case 2: // "Cancel"
		default:
			// If we were quitting, we did not really mean it.
			gAppViewerp->abortQuit();
	}

	return false;
}

void LLPreviewNotecard::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLPreview::reshape(width, height, called_from_parent);

	if (!isMinimized())
	{
		// So that next time you open a notecard it will have the same height
		// and width (although not the same position).
		gSavedSettings.setRect("NotecardEditorRect", getRect());
	}
}

//static
bool LLPreviewNotecard::hasChanged(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->getEnabled() && !self->mEditor->isPristine();
}

//static
bool LLPreviewNotecard::enableSaveLoadFile(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->getEnabled() && !HBFileSelector::isInUse();
}

//static
void LLPreviewNotecard::loadFromFileCallback(HBFileSelector::ELoadFilter,
											 std::string& filename,
											 void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (!self || !sInstances.count(self))
	{
		gNotifications.add("LoadNoteAborted");
		return;
	}
	if (!self->loadFile(filename))
	{
		LLSD args;
		args["FILE"] = filename;
		gNotifications.add("CannotReadFile", args);
	}
}

//static
void LLPreviewNotecard::onLoadFromFile(void* userdata)
{
	HBFileSelector::loadFile(HBFileSelector::FFLOAD_TEXT, loadFromFileCallback,
							 userdata);
}

//static
void LLPreviewNotecard::saveToFileCallback(HBFileSelector::ESaveFilter,
										   std::string& filename,
										   void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self && sInstances.count(self))
	{
		self->saveFile(filename);
	}
	else
	{
		gNotifications.add("SaveNoteAborted");
	}
}

//static
void LLPreviewNotecard::onSaveToFile(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self && sInstances.count(self))
	{
		std::string suggestion = self->mNoteName + ".txt";
		HBFileSelector::saveFile(HBFileSelector::FFSAVE_TXT, suggestion,
							 	 saveToFileCallback, userdata);
	}
}

//static
void LLPreviewNotecard::onEditedFileChanged(const std::string& filename,
											void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self && sInstances.count(self))
	{
		if (filename == self->mTempFilename)
		{
			self->loadFile(filename);
		}
		else
		{
			llwarns << "Watched file (" << filename
					<< ") and auto-saved file (" << self->mTempFilename
					<< ") do not match !" << llendl;
		}
	}
}

//static
void LLPreviewNotecard::onEditExternal(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self && sInstances.count(self))
	{
		if (self->mTempFilename.empty())
		{
			self->mTempFilename = gDirUtilp->getTempFilename(false) + ".txt";
		}
		if (!self->saveFile(self->mTempFilename))
		{
			return;
		}

		if (self->mExternalEditor)
		{
			self->mExternalEditor->kill();
		}
		else
		{
			self->mExternalEditor = new HBExternalEditor(onEditedFileChanged,
														 self);
		}
		if (!self->mExternalEditor->open(self->mTempFilename))
		{
			LLSD args;
			args["MESSAGE"] = self->mExternalEditor->getErrorMessage();
			gNotifications.add("GenericAlert", args);
		}
	}
}

//static
void LLPreviewNotecard::onSearchMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		LLFloaterSearchReplace::show(self->mEditor);
	}
}

//static
void LLPreviewNotecard::onUndoMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		self->mEditor->undo();
	}
}

//static
void LLPreviewNotecard::onRedoMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		self->mEditor->redo();
	}
}

//static
void LLPreviewNotecard::onCutMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		self->mEditor->cut();
	}
}

//static
void LLPreviewNotecard::onCopyMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		self->mEditor->copy();
	}
}

//static
void LLPreviewNotecard::onPasteMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		self->mEditor->paste();
	}
}

//static
void LLPreviewNotecard::onSelectAllMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		self->mEditor->selectAll();
	}
}

//static
void LLPreviewNotecard::onDeselectMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		self->mEditor->deselect();
	}
}

//static
void LLPreviewNotecard::onSpellCheckMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	if (self)
	{
		self->mEditor->setSpellCheck(!self->mEditor->getSpellCheck());
	}
}

//static
bool LLPreviewNotecard::enableUndoMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->mEditor->canUndo();
}

//static
bool LLPreviewNotecard::enableRedoMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->mEditor->canRedo();
}

//static
bool LLPreviewNotecard::enableCutMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->mEditor->canCut();
}

//static
bool LLPreviewNotecard::enableCopyMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->mEditor->canCopy();
}

//static
bool LLPreviewNotecard::enablePasteMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->mEditor->canPaste();
}

//static
bool LLPreviewNotecard::enableSelectAllMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->mEditor->canSelectAll();
}

//static
bool LLPreviewNotecard::enableDeselectMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->mEditor->canDeselect();
}

//static
bool LLPreviewNotecard::enableSpellCheckMenu(void* userdata)
{
	return LLSpellCheck::getInstance()->getSpellCheck();
}

//static
bool LLPreviewNotecard::checkSpellCheckMenu(void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	return self && self->mEditor->getSpellCheck() &&
		   LLSpellCheck::getInstance()->getSpellCheck();
}
