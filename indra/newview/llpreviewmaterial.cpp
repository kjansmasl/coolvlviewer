/**
 * @file llpreviewmaterial.cpp
 * @brief LLPreviewMaterial class implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewergpl$
 *
 * Copyright (c) 2022, Linden Research, Inc., (c) 2023 Henri Beauchamp
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

#include <sstream>

#include "boost/signals2.hpp"

#include "llpreviewmaterial.h"

#include "llassetstorage.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfilesystem.h"
#include "llnotifications.h"
#include "lleconomy.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llcolorswatch.h"
#include "llfloaterperms.h"
#include "llinventorymodel.h"
#include "lllocalbitmaps.h"
#include "lllocalgltfmaterials.h"
#include "hbobjectbackup.h"			// For HBObjectBackup::validateAssetPerms()
#include "llselectmgr.h"
#include "llstatusbar.h"			// For can_afford_transaction()
#include "lltexturectrl.h"
#include "lltinygltfhelper.h"
#include "lltoolpie.h"
#include "llviewerassetupload.h"
#include "llviewerinventory.h"		// For move_or_copy_item_from_object()
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"
#include "llvovolume.h"
#include "roles_constants.h"		// For GP_OBJECT_MANIPULATE

// Holds all the pointers to material previews/editors instances. Used by
// callbacks to verify that its parent instance has not vanished. HB
static std::set<LLPreviewMaterial*> sEditorInstances;

// Static variables for use only with the "singleton" live editor. They are not
// static members of LLPreviewMaterial, because they are only used within this
// module, and by various classes (which would have forced to use public static
// members, or to make all these classes friends of LLPreviewMaterial, or to
// add variable reference/pointer accessors). HB
static LLPreviewMaterial* sLiveEditorInstance = NULL;
static LLUUID sOverrideObjectId;
static S32 sOverrideObjectTE = 0;
static bool sOverrideInProgress = false;
static bool sSelectionNeedsUpdate = true;
static boost::signals2::connection sSelectionUpdateSlot;

static const std::string MAT_BASE_COLOR_DEFAULT_NAME = "Base Color";
static const std::string MAT_NORMAL_DEFAULT_NAME = "Normal";
static const std::string MAT_METALLIC_DEFAULT_NAME = "Metallic Roughness";
static const std::string MAT_EMISSIVE_DEFAULT_NAME = "Emissive";

// WARNING: if you change this enum (including just the order of its values),
// you must also revise LLSelectedMaterialChangeFunctor and onSelectCtrl()
// accordinly. HB
enum eDirtyFlags : U32
{
	MAT_BASE_COLOR_TEX_DIRTY	= 0x1 << 0,
	MAT_NORMAL_TEX_DIRTY 		= 0x1 << 1,
	MAT_ROUGHTNESS_TEX_DIRTY	= 0x1 << 2,
	MAT_EMISIVE_TEX_DIRTY		= 0x1 << 3,
	MAT_BASE_COLOR_DIRTY		= 0x1 << 4,
	MAT_EMISIVE_COLOR_DIRTY		= 0x1 << 5,
	MAT_TRANSPARENCY_DIRTY		= 0x1 << 6,
	MAT_ALPHA_MODE_DIRTY		= 0x1 << 7,
	MAT_ALPHA_CUTOFF_DIRTY		= 0x1 << 8,
	MAT_METALNESS_DIRTY			= 0x1 << 9,
	MAT_ROUGHTNESS_DIRTY		= 0x1 << 10,
	MAT_DOUBLE_SIDED_DIRTY		= 0x1 << 11,
};

//static
LLPreviewMaterial* LLPreviewMaterial::getLiveEditorInstance()
{
	return sLiveEditorInstance;
}

// Constructor used internally only, for the live editor and uploads.
LLPreviewMaterial::LLPreviewMaterial(const std::string& name, bool live_editor)
:	LLPreview(name),
	mUnsavedChanges(0),
	mRevertedChanges(0),
	mUploadingTexturesCount(0),
	mExpectedUploadCost(0),
	mIsOverride(live_editor),
	mCanCopy(false),
	mCanModify(false),
	mHasSelection(false),
	mUploadingTexturesFailure(false)
{
	if (live_editor)
	{
		llassert_always(!sLiveEditorInstance);
		sLiveEditorInstance = this;
	}
	sEditorInstances.insert(this);

	LLUICtrlFactory* factoryp = LLUICtrlFactory::getInstance();
	factoryp->buildFloater(this, "floater_preview_material.xml");
}

// Constructor used to preview/edit inventory items.
LLPreviewMaterial::LLPreviewMaterial(const std::string& name,
									 const LLRect& rect,
									 const std::string& title,
									 const LLUUID& item_id,
									 const LLUUID& object_id)
:	LLPreview(name, rect, title, item_id, object_id),
	mUnsavedChanges(0),
	mRevertedChanges(0),
	mUploadingTexturesCount(0),
	mExpectedUploadCost(0),
	mIsOverride(false),
	mCanCopy(false),
	mCanModify(false),
	mHasSelection(false),
	mUploadingTexturesFailure(false)
{
	sEditorInstances.insert(this);

	const LLViewerInventoryItem* itemp = getItem();
	if (itemp)
	{
		mAssetID = itemp->getAssetUUID();
	}

	LLUICtrlFactory* factoryp = LLUICtrlFactory::getInstance();
	factoryp->buildFloater(this, "floater_preview_material.xml");

	setTitle(title);
	loadAsset();
}

//virtual
LLPreviewMaterial::~LLPreviewMaterial()
{
	for (connection_map_t::iterator it = mTextureChangesUpdates.begin(),
									end = mTextureChangesUpdates.end();
		 it != end; ++it)
	{
		it->second.mConnection.disconnect();
	}
	if (this == sLiveEditorInstance)
	{
		if (sSelectionUpdateSlot.connected())
		{
			sSelectionUpdateSlot.disconnect();
		}
		sLiveEditorInstance = NULL;
	}
	sEditorInstances.erase(this);
}

//virtual
bool LLPreviewMaterial::postBuild()
{
	mDoubleSidedCheck = getChild<LLCheckBoxCtrl>("double_sided_check");
	mDoubleSidedCheck->setCommitCallback(onSelectCtrl);
	mDoubleSidedCheck->setCallbackUserData(this);

	mUploadFeeText = getChild<LLTextBox>("upload_fee");
	mUploadFeeText->setVisible(!mIsOverride);

	mBaseColorTexCtrl = getChild<LLTextureCtrl>("base_color_texture");
	mBaseColorTexCtrl->setCommitCallback(onTextureCtrl);
	mBaseColorTexCtrl->setCallbackUserData(this);

	mMetallicTexCtrl = getChild<LLTextureCtrl>("roughness_texture");
	mMetallicTexCtrl->setCommitCallback(onTextureCtrl);
	mMetallicTexCtrl->setCallbackUserData(this);

	mEmissiveTexCtrl = getChild<LLTextureCtrl>("emissive_texture");
	mEmissiveTexCtrl->setCommitCallback(onTextureCtrl);
	mEmissiveTexCtrl->setCallbackUserData(this);

	mNormalTexCtrl = getChild<LLTextureCtrl>("normal_texture");
	mNormalTexCtrl->setCommitCallback(onTextureCtrl);
	mNormalTexCtrl->setCallbackUserData(this);

	if (!gAgent.isGodlike())
	{
		constexpr PermissionMask full_perm_mask = PERM_COPY | PERM_TRANSFER;
		mBaseColorTexCtrl->setImmediateFilterPermMask(full_perm_mask);
		mMetallicTexCtrl->setImmediateFilterPermMask(full_perm_mask);
		mEmissiveTexCtrl->setImmediateFilterPermMask(full_perm_mask);
		mNormalTexCtrl->setImmediateFilterPermMask(full_perm_mask);
	}

	mBaseColorCtrl = getChild<LLColorSwatchCtrl>("base_color");
	mBaseColorCtrl->setCommitCallback(onSelectCtrl);
	mBaseColorCtrl->setCallbackUserData(this);

	mEmissiveColorCtrl = getChild<LLColorSwatchCtrl>("emissive_color");
	mEmissiveColorCtrl->setCommitCallback(onSelectCtrl);
	mEmissiveColorCtrl->setCallbackUserData(this);

	if (mIsOverride)
	{
		// Material override change success callback
		LLGLTFMaterialList::addSelectionUpdateCallback(&updateLive);

		// Live editing needs a recovery mechanism on cancel
		mBaseColorTexCtrl->setOnCancelCallback(onCancelCtrl);
		mMetallicTexCtrl->setOnCancelCallback(onCancelCtrl);
		mEmissiveTexCtrl->setOnCancelCallback(onCancelCtrl);
		mNormalTexCtrl->setOnCancelCallback(onCancelCtrl);
		mBaseColorCtrl->setOnCancelCallback(onCancelCtrl);
		mEmissiveColorCtrl->setOnCancelCallback(onCancelCtrl);

		// Save applied changes on 'OK' to our recovery mechanism.
		mBaseColorTexCtrl->setOnSelectCallback(onSelectCtrl);
		mMetallicTexCtrl->setOnSelectCallback(onSelectCtrl);
		mEmissiveTexCtrl->setOnSelectCallback(onSelectCtrl);
		mNormalTexCtrl->setOnSelectCallback(onSelectCtrl);
		mBaseColorCtrl->setOnSelectCallback(onCancelCtrl);
		mEmissiveColorCtrl->setOnSelectCallback(onCancelCtrl);
	}
	else
	{
		mBaseColorTexCtrl->setCanApplyImmediately(false);
		mMetallicTexCtrl->setCanApplyImmediately(false);
		mEmissiveTexCtrl->setCanApplyImmediately(false);
		mNormalTexCtrl->setCanApplyImmediately(false);
		mBaseColorCtrl->setCanApplyImmediately(false);
		mEmissiveColorCtrl->setCanApplyImmediately(false);
	}

	mTransparencyCtrl = getChild<LLSpinCtrl>("transparency");
	mTransparencyCtrl->setCommitCallback(onSelectCtrl);
	mTransparencyCtrl->setCallbackUserData(this);

	mAlphaModeCombo = getChild<LLComboBox>("alpha_mode");
	mAlphaModeCombo->setCommitCallback(onSelectCtrl);
	mAlphaModeCombo->setCallbackUserData(this);

	mAlphaCutoffCtrl = getChild<LLSpinCtrl>("alpha_cutoff");
	mAlphaCutoffCtrl->setCommitCallback(onSelectCtrl);
	mAlphaCutoffCtrl->setCallbackUserData(this);

	mMetalnessCtrl = getChild<LLSpinCtrl>("metalness");
	mMetalnessCtrl->setCommitCallback(onSelectCtrl);
	mMetalnessCtrl->setCallbackUserData(this);

	mRoughnessCtrl = getChild<LLSpinCtrl>("roughness");
	mRoughnessCtrl->setCommitCallback(onSelectCtrl);
	mRoughnessCtrl->setCallbackUserData(this);

	mCancelButton = getChild<LLButton>("cancel_btn");
	mCancelButton->setClickedCallback(onClickCancel, this);
	mSaveButton = getChild<LLButton>("save_btn");
	mSaveAsButton = getChild<LLButton>("save_as_btn");
	if (mIsOverride)
	{
		mCancelButton->setLabel(getString("close"));
		mSaveButton->setVisible(false);
		mSaveAsButton->setVisible(false);
	}
	else
	{
		mSaveButton->setClickedCallback(onClickSave, this);
		mSaveAsButton->setClickedCallback(onClickSaveAs, this);
	}

	// Sync Save button state and cost.
	markChangesUnsaved(0);

	return LLPreview::postBuild();
}

//virtual
void LLPreviewMaterial::setItemID(const LLUUID& item_id)
{
	LLPreview::setItemID(item_id);
	const LLViewerInventoryItem* itemp = getItem();
	if (itemp)
	{
		mAssetID = itemp->getAssetUUID();
	}
}

//virtual
void LLPreviewMaterial::setAuxItem(const LLInventoryItem* itemp)
{
	LLPreview::setAuxItem(itemp);
	if (itemp)
	{
		mAssetID = itemp->getAssetUUID();
	}
}

//virtual
void LLPreviewMaterial::inventoryChanged(LLViewerObject*,
										 LLInventoryObject::object_list_t*,
										 S32, void*)
{
	removeVOInventoryListener();
	loadAsset();
}

//virtual
void LLPreviewMaterial::draw()
{
	if (mIsOverride)
	{
		if (sSelectionNeedsUpdate ||
		 	(mHasSelection && gSelectMgr.getSelection()->isEmpty()))
		{
			LL_DEBUGS("GLTF") << "Reloading live material from selection"
							  << LL_ENDL;
			sSelectionNeedsUpdate = false;
			clearTextures();
			setFromSelection();
		}
	}
	else
	{
		bool loaded = mAssetStatus == PREVIEW_ASSET_LOADED;
		mSaveButton->setEnabled(loaded && mCanModify &&
								(mUnsavedChanges || mRevertedChanges));
		mSaveAsButton->setEnabled(loaded && mCanCopy);
	}
	LLPreview::draw();
}

void LLPreviewMaterial::setMaterialName(const std::string& name)
{
	setTitle(name);
	mMaterialName = name;
}

void LLPreviewMaterial::refreshFromInventory(const LLUUID& new_item_id)
{
	if (mIsOverride)	// Should never happen.
	{
		llassert(false);
		return;
	}
	if (new_item_id.notNull())
	{
		setItemID(new_item_id);
	}
	loadAsset();
}

struct LLPreviewMaterialInfo
{
	LLPreviewMaterial*	mPreviewp;
	LLUUID				mAssetUUID;
	LLUUID				mItemUUID;
	LLUUID				mObjectUUID;
};

//virtual
void LLPreviewMaterial::loadAsset()
{
	if (mIsOverride)
	{
		// Overrides do not have an asset... HB
		return;
	}

	const LLInventoryItem* itemp = getItem();

	if (!itemp)
	{
		if (mObjectUUID.isNull() || mItemUUID.isNull())
		{
			llwarns << "Cannot load asset: no object or no inventory item set."
					<< llendl;
			return;
		}
		LLViewerObject* objectp = gObjectList.findObject(mObjectUUID);
		if (!objectp)
		{
			llwarns << "Cannot load asset: object gone ?" << llendl;
			return;
		}
		bool inv_dirty = objectp->isInventoryDirty();
		if (inv_dirty || objectp->isInventoryPending())
		{
			registerVOInventoryListener(objectp, NULL);
			if (inv_dirty)
			{
				objectp->requestInventory();
			}
		}
		return;
	}

	setMaterialName(itemp->getName());

	// Set permissions
	LLPermissions perm = itemp->getPermissions();
	mCanCopy = mObjectUUID.isNull() &&
			   gAgent.allowOperation(PERM_COPY, perm, GP_OBJECT_MANIPULATE);
	mCanModify = canModify(mObjectUUID, itemp);
	if (mCanModify)
	{
		const LLUUID& lib_id = gInventory.getLibraryRootFolderID();
		if (mObjectUUID.isNull() &&
			gInventory.isObjectDescendentOf(mItemUUID, lib_id))
		{
			mCanModify = false;
		}
	}

	mAssetID = itemp->getAssetUUID();
	if (mAssetID.isNull())
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
		loadDefaults();
		resetUnsavedChanges();
		setEnableEditing(mCanModify);
		return;
	}

	setEnableEditing(false);	// Wait for it to load
	mAssetStatus = PREVIEW_ASSET_LOADING;

	if (!gAssetStoragep)
	{
		return;
	}

	LLPreviewMaterialInfo* infop = new LLPreviewMaterialInfo;
	infop->mPreviewp = this;
	infop->mAssetUUID = itemp->getAssetUUID();
	if (mObjectUUID.notNull())
	{
		LLViewerObject* objectp = gObjectList.findObject(mObjectUUID);
		if (!objectp)
		{
			llwarns << "Cannot load asset: object gone ?" << llendl;
			mAssetID.setNull();
			mAssetStatus = PREVIEW_ASSET_LOADED;
			resetUnsavedChanges();
			setEnableEditing(mCanModify);
			delete infop;
			return;
		}
		infop->mItemUUID = mItemUUID;
		infop->mObjectUUID = mObjectUUID;
	}
	else
	{
		infop->mItemUUID = mItemUUID;
	}

	gAssetStoragep->getAssetData(infop->mAssetUUID, LLAssetType::AT_MATERIAL,
								 onLoadComplete, (void*)infop, true);
}

//static
void LLPreviewMaterial::onLoadComplete(const LLUUID& asset_id,
									   LLAssetType::EType type, void* userdata,
									   S32 status, LLExtStat)
{
	LLPreviewMaterialInfo* infop = (LLPreviewMaterialInfo*)userdata;
	if (!infop)	// Should never happen.
	{
		llassert(false);
		return;
	}

	LLPreviewMaterial* self = infop->mPreviewp;
	if (!self || !sEditorInstances.count(self) || asset_id != self->mAssetID)
	{
		// Floater already gone.
		delete infop;
		return;
	}

	// Check for any error
	if (status)
	{
		gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);

		if (status == LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE ||
			status == LL_ERR_FILE_EMPTY)
		{
			gNotifications.add("MaterialMissing");
		}
		else if (status == LL_ERR_INSUFFICIENT_PERMISSIONS)
		{
			gNotifications.add("MaterialNoPermissions");
		}
		else
		{
			gNotifications.add("UnableToLoadMaterial");
		}
		self->setEnableEditing(false);
		self->mAssetStatus = PREVIEW_ASSET_ERROR;
		delete infop;
		return;
	}

	LLFileSystem file(asset_id);
	S32 file_length = file.getSize();
	std::string buffer(file_length + 1, '\0');
	file.read((U8*)&buffer[0], file_length);
	self->decodeAsset(buffer);

	bool can_modify = LLPreview::canModify(self->mObjectUUID, self->getItem());
	if (can_modify && self->mObjectUUID.isNull())
	{
		const LLUUID& lib_id = gInventory.getLibraryRootFolderID();
		can_modify = !gInventory.isObjectDescendentOf(self->mItemUUID, lib_id);
	}
	self->setEnableEditing(can_modify);
	self->resetUnsavedChanges();
	self->mAssetStatus = PREVIEW_ASSET_LOADED;
	self->setEnabled(true);						// Ready for use

	delete infop;
}

LLUUID LLPreviewMaterial::getTextureId(LLTextureCtrl* ctrlp) const
{
	return ctrlp->getValue().asUUID();
}

void LLPreviewMaterial::setTextureId(LLTextureCtrl* ctrlp, const LLUUID& id)
{
	ctrlp->setValue(id);
	ctrlp->setDefaultImageAssetID(id);
	ctrlp->setTentative(false);
}

F32 LLPreviewMaterial::getCtrlValue(LLSpinCtrl* ctrlp) const
{
	return ctrlp->getValue().asReal();
}

void LLPreviewMaterial::setCtrlValue(LLSpinCtrl* ctrlp, F32 value)
{
	ctrlp->setValue(value);
}

LLColor4 LLPreviewMaterial::getBaseColor() const
{
	LLColor4 color = linearColor4(LLColor4(mBaseColorCtrl->getValue()));
	color.mV[3] = getTransparency();
	return color;
}

void LLPreviewMaterial::setBaseColor(const LLColor4& color)
{
	mBaseColorCtrl->setValue(srgbColor4(color).getValue());
	setTransparency(color.mV[3]);
}

LLColor4 LLPreviewMaterial::getEmissiveColor() const
{
	return linearColor4(LLColor4(mEmissiveColorCtrl->getValue()));
}

void LLPreviewMaterial::setEmissiveColor(const LLColor4& color)
{
	mEmissiveColorCtrl->setValue(srgbColor4(color).getValue());
}

std::string LLPreviewMaterial::getAlphaMode() const
{
	return mAlphaModeCombo->getValue().asString();
}

void LLPreviewMaterial::setAlphaMode(const std::string& alpha_mode)
{
	mAlphaModeCombo->setValue(alpha_mode);
}

bool LLPreviewMaterial::getDoubleSided() const
{
	return mDoubleSidedCheck->get();
}

void LLPreviewMaterial::setDoubleSided(bool double_sided)
{
	mDoubleSidedCheck->set(double_sided);
}

void LLPreviewMaterial::resetUnsavedChanges()
{
	mUnsavedChanges = mRevertedChanges = 0;
	if (!mIsOverride)
	{
		mExpectedUploadCost = 0;
		mUploadFeeText->setTextArg("[COST]", "0");
		mUploadFeeText->setVisible(false);
	}
}

void LLPreviewMaterial::markChangesUnsaved(U32 dirty_flag)
{
	mUnsavedChanges |= dirty_flag;
	if (mIsOverride)
	{
		return;
	}

	if (mUnsavedChanges)
	{
		const LLInventoryItem* itemp = getItem();
		if (itemp)
		{
			mCanModify = canModify(mObjectUUID, itemp);
			if (mCanModify && mObjectUUID.isNull())
			{
				const LLUUID& lib_id = gInventory.getLibraryRootFolderID();
				mCanModify = !gInventory.isObjectDescendentOf(mItemUUID,
															  lib_id);
			}
		}
	}
	S32 count = 0;
	if (mBaseColorTextureUploadId.notNull() &&
		mBaseColorTextureUploadId == getBaseColorId())
	{
		++count;
	}
	if (mMetallicTextureUploadId.notNull() &&
		mMetallicTextureUploadId == getMetallicRoughnessId())
	{
		++count;
	}
	if (mEmissiveTextureUploadId.notNull() &&
		mEmissiveTextureUploadId == getEmissiveId())
	{
		++count;
	}
	if (mNormalTextureUploadId.notNull() &&
		mNormalTextureUploadId == getNormalId())
	{
		++count;
	}
	mExpectedUploadCost = LLEconomy::getInstance()->getTextureUploadCost() *
						  count;
	mUploadFeeText->setTextArg("[COST]", llformat("%d", mExpectedUploadCost));
	mUploadFeeText->setVisible(mExpectedUploadCost > 0);
}

void LLPreviewMaterial::setEnableEditing(bool can_modify)
{
	mDoubleSidedCheck->setEnabled(can_modify);
	mBaseColorTexCtrl->setEnabled(can_modify);
	mMetallicTexCtrl->setEnabled(can_modify);
	mEmissiveTexCtrl->setEnabled(can_modify);
	mNormalTexCtrl->setEnabled(can_modify);
	mBaseColorCtrl->setEnabled(can_modify);
	mEmissiveColorCtrl->setEnabled(can_modify);
	mAlphaModeCombo->setEnabled(can_modify);
	mTransparencyCtrl->setEnabled(can_modify);
	mAlphaCutoffCtrl->setEnabled(can_modify);
	mMetalnessCtrl->setEnabled(can_modify);
	mRoughnessCtrl->setEnabled(can_modify);
}

void LLPreviewMaterial::clearTextures()
{
	mBaseColorJ2C = mNormalJ2C = mMetallicRoughnessJ2C = mEmissiveJ2C = NULL;
	mBaseColorFetched = mNormalFetched = mMetallicRoughnessFetched =
						mEmissiveFetched = NULL;
	mBaseColorTextureUploadId.setNull();
	mMetallicTextureUploadId.setNull();
	mEmissiveTextureUploadId.setNull();
	mNormalTextureUploadId.setNull();
}

void LLPreviewMaterial::subscribeToLocalTexture(U32 flag, const LLUUID& t_id)
{
	LocalTexConnection& connection = mTextureChangesUpdates[flag];
	if (connection.mTrackingId == t_id)
	{
		return;	// Already registered with us...
	}
	connection.mConnection.disconnect();
	connection.mTrackingId = t_id;
	connection.mConnection =
		LLLocalBitmap::setOnChangedCallback(t_id,
											[this, flag](const LLUUID& tid,
														 const LLUUID& oid,
														 const LLUUID& nid)
											{
												if (nid.notNull())
												{
													replaceLocalTexture(oid,
																		nid);
													return;
												}
												mTextureChangesUpdates[flag].mConnection.disconnect();											
											});
}

const LLUUID& LLPreviewMaterial::getLocalTexTrackingIdFromFlag(U32 flag) const
{
	connection_map_t::const_iterator it = mTextureChangesUpdates.find(flag);
	return it != mTextureChangesUpdates.end() ? it->second.mTrackingId
											  : LLUUID::null;
}

bool LLPreviewMaterial::updateMaterialLocalSubscription(LLGLTFMaterial* matp)
{
	if (!matp)
	{
		return false;
	}

	bool seen = false;
	for (connection_map_t::const_iterator it = mTextureChangesUpdates.begin(),
										  end = mTextureChangesUpdates.end();
		 it != end; ++it)
	{
		const LLUUID& tracking_id = it->second.mTrackingId;
		const LLUUID& world_id = LLLocalBitmap::getWorldID(tracking_id);
		if (matp->mTextureId[BASECOLIDX] == world_id ||
			matp->mTextureId[NORMALIDX] == world_id ||
			matp->mTextureId[MROUGHIDX] == world_id ||
			matp->mTextureId[EMISSIVEIDX] == world_id)
		{
			LLLocalBitmap::associateGLTFMaterial(tracking_id, matp);
			seen = true;
		}
	}
	return seen;
}

void LLPreviewMaterial::replaceLocalTexture(const LLUUID& old_id,
											const LLUUID& new_id)
{
	if (old_id == new_id)
	{
		return;	// Nothing to do...
	}

	if (getBaseColorId() == old_id)
	{
		setBaseColorId(new_id);
	}
	if (mBaseColorTexCtrl->getDefaultImageAssetID() == old_id)
	{
		mBaseColorTexCtrl->setDefaultImageAssetID(new_id);
	}

	if (getMetallicRoughnessId() == old_id)
	{
		setMetallicRoughnessId(new_id);
	}
	if (mMetallicTexCtrl->getDefaultImageAssetID() == old_id)
	{
		mMetallicTexCtrl->setDefaultImageAssetID(new_id);
	}

	if (getEmissiveId() == old_id)
	{
		setEmissiveId(new_id);
	}
	if (mEmissiveTexCtrl->getDefaultImageAssetID() == old_id)
	{
		mEmissiveTexCtrl->setDefaultImageAssetID(new_id);
	}

	if (getNormalId() == old_id)
	{
		setNormalId(new_id);
	}
	if (mNormalTexCtrl->getDefaultImageAssetID() == old_id)
	{
		mNormalTexCtrl->setDefaultImageAssetID(new_id);
	}
}

void LLPreviewMaterial::loadDefaults()
{
	tinygltf::Model model_in;
	model_in.materials.resize(1);
	setFromGltfModel(model_in, 0, true);
}

void LLPreviewMaterial::setTextureUploadId(LLTextureCtrl* ctrlp,
										   const LLUUID& id)
{
	U32 dirty_flag = getDirtyFlagFromCtrl(ctrlp);
	// If HBObjectBackup::validateAssetPerms() returns true, then we do have
	// an inventory item bearing the proper texture Id and suitable permissions
	// for reuse by this material. HB
	if (id.notNull() && !HBObjectBackup::validateAssetPerms(id, true))
	{
		switch (dirty_flag)
		{
			case MAT_BASE_COLOR_TEX_DIRTY:
				mBaseColorTextureUploadId = id;
				break;
			case MAT_NORMAL_TEX_DIRTY:
				mNormalTextureUploadId = id;
				break;
			case MAT_ROUGHTNESS_TEX_DIRTY:
				mMetallicTextureUploadId = id;
				break;
			case MAT_EMISIVE_TEX_DIRTY:
				mEmissiveTextureUploadId = id;
			default:
				break;
		}
	}
	markChangesUnsaved(dirty_flag);
}

struct LLRenderMaterialFunctor final : public LLSelectedTEFunctor
{
	LLRenderMaterialFunctor(const LLUUID& id)
	:	mMatId(id)
	{
	}

	bool apply(LLViewerObject* objectp, S32 te) override
	{
		if (objectp && objectp->permModify() && objectp->getVolume())
		{
			LLVOVolume* vobjp = (LLVOVolume*)objectp;
			// Note: false = preview only
			vobjp->setRenderMaterialID(te, mMatId, false);
			vobjp->updateTEMaterialTextures(te);
		}
		return true;
	}

	LLUUID mMatId;
};

class LLRenderMatOverrider final : public LLSelectedNodeFunctor
{
protected:
	LOG_CLASS(LLRenderMatOverrider);

public:
	LLRenderMatOverrider(const LLUUID& object_id, S32 te)
	:	mObjectId(object_id),
		mObjectTE(te),
		mSuccess(false)
	{
	}

	bool apply(LLSelectNode* nodep) override
	{
		LLPreviewMaterial* editorp =
			LLPreviewMaterial::getLiveEditorInstance();
		if (!editorp)	// Check in case the live preview has gone... HB
		{
			return false;
		}

		LLViewerObject* objectp = nodep->getObject();
		if (!objectp || !objectp->permModify() || !objectp->getVolume())
		{
			return false;
		}

		// Avatars have TEs but no faces.
		U8 num_tes = llmin(objectp->getNumTEs(), objectp->getNumFaces());
		for (U8 te = 0; te < num_tes; ++te)
		{
			if (!nodep->isTESelected(te))
			{
				continue;
			}

			LLTextureEntry* tep = objectp->getTE(te);
			if (!tep || !tep->getGLTFMaterial())
			{
				// Overrides are not supposed to work or apply if there is no
				// base material to work from.
				continue;
			}
			LLPointer<LLGLTFMaterial> matp = tep->getGLTFMaterialOverride();
			if (matp.isNull())
			{
				// Start with a material override which does not make any
				// changes
				matp = new LLGLTFMaterial();
			}
			else
			{
				matp = new LLGLTFMaterial(*matp);
			}

			U32 changed_flags = editorp->getUnsavedChangesFlags();
			U32 reverted_flags = editorp->getRevertedChangesFlags();

			LLPointer<LLGLTFMaterial> revmatp;
			if (nodep->mSavedGLTFOverrideMaterials.size() > te)
			{
				if (nodep->mSavedGLTFOverrideMaterials[te].notNull())
				{
					revmatp = nodep->mSavedGLTFOverrideMaterials[te];
				}
				else
				{
					// mSavedGLTFOverrideMaterials[te] being present but null
					// means we need to use a default value.
					revmatp = new LLGLTFMaterial();
				}
			}
			bool has_revert = revmatp.notNull();
			bool check_local_tex = false;

			// Override the object values with values from editor where
			// appropriate

			if (changed_flags & MAT_BASE_COLOR_TEX_DIRTY)
			{
				matp->setBaseColorId(editorp->getBaseColorId(), true);
				check_local_tex = true;
			}
			else if (has_revert && (reverted_flags & MAT_BASE_COLOR_TEX_DIRTY))
			{
				matp->setBaseColorId(revmatp->mTextureId[BASECOLIDX]);
				check_local_tex = true;
			}
			if (check_local_tex)
			{
				check_local_tex = false;
				const LLUUID& tracking_id =
					editorp->getLocalTexTrackingIdFromFlag(BASECOLIDX);
				if (tracking_id.notNull())
				{
					LLLocalBitmap::associateGLTFMaterial(tracking_id, matp);
				}
			}

			if (changed_flags & MAT_NORMAL_TEX_DIRTY)
			{
				matp->setNormalId(editorp->getNormalId(), true);
				check_local_tex = true;
			}
			else if (has_revert && (reverted_flags & MAT_NORMAL_TEX_DIRTY))
			{
				matp->setNormalId(revmatp->mTextureId[NORMALIDX]);
				check_local_tex = true;
			}
			if (check_local_tex)
			{
				check_local_tex = false;
				const LLUUID& tracking_id =
					editorp->getLocalTexTrackingIdFromFlag(NORMALIDX);
				if (tracking_id.notNull())
				{
					LLLocalBitmap::associateGLTFMaterial(tracking_id, matp);
				}
			}

			if (changed_flags & MAT_ROUGHTNESS_TEX_DIRTY)
			{
				matp->setMetallicRoughnessId(editorp->getMetallicRoughnessId(),
											 true);
			}
			else if (has_revert && (reverted_flags & MAT_ROUGHTNESS_TEX_DIRTY))
			{
				matp->setMetallicRoughnessId(revmatp->mTextureId[MROUGHIDX],
											 false);
			}
			if (check_local_tex)
			{
				check_local_tex = false;
				const LLUUID& tracking_id =
					editorp->getLocalTexTrackingIdFromFlag(MROUGHIDX);
				if (tracking_id.notNull())
				{
					LLLocalBitmap::associateGLTFMaterial(tracking_id, matp);
				}
			}

			if (changed_flags & MAT_EMISIVE_TEX_DIRTY)
			{
				matp->setEmissiveId(editorp->getEmissiveId(), true);
			}
			else if (has_revert && (reverted_flags & MAT_EMISIVE_TEX_DIRTY))
			{
				matp->setEmissiveId(revmatp->mTextureId[EMISSIVEIDX]);
			}
			if (check_local_tex)
			{
				check_local_tex = false;
				const LLUUID& tracking_id =
					editorp->getLocalTexTrackingIdFromFlag(EMISSIVEIDX);
				if (tracking_id.notNull())
				{
					LLLocalBitmap::associateGLTFMaterial(tracking_id, matp);
				}
			}

			constexpr U32 COLOR_FLAGS = MAT_TRANSPARENCY_DIRTY |
										MAT_BASE_COLOR_DIRTY;
			if (changed_flags & COLOR_FLAGS)
			{
				matp->setBaseColorFactor(editorp->getBaseColor(), true);
			}
			else if (has_revert && (reverted_flags & COLOR_FLAGS))
			{
				matp->setBaseColorFactor(revmatp->mBaseColor);
			}

			if (changed_flags & MAT_EMISIVE_COLOR_DIRTY)
			{
				LLColor3 color(editorp->getEmissiveColor());
				matp->setEmissiveColorFactor(color, true);
			}
			else if (has_revert && (reverted_flags & MAT_EMISIVE_COLOR_DIRTY))
			{
				matp->setEmissiveColorFactor(revmatp->mEmissiveColor);
			}

			if (changed_flags & MAT_ALPHA_MODE_DIRTY)
			{
				matp->setAlphaMode(editorp->getAlphaMode(), true);
			}
			else if (has_revert && (reverted_flags & MAT_ALPHA_MODE_DIRTY))
			{
				matp->setAlphaMode(revmatp->mAlphaMode);
			}

			if (changed_flags & MAT_ALPHA_CUTOFF_DIRTY)
			{
				matp->setAlphaCutoff(editorp->getAlphaCutoff(), true);
			}
			else if (has_revert && (reverted_flags & MAT_ALPHA_CUTOFF_DIRTY))
			{
				matp->setAlphaCutoff(revmatp->mAlphaCutoff);
			}

			if (changed_flags & MAT_METALNESS_DIRTY)
			{
				matp->setMetallicFactor(editorp->getMetalnessFactor(), true);
			}
			else if (has_revert && (reverted_flags & MAT_METALNESS_DIRTY))
			{
				matp->setMetallicFactor(revmatp->mMetallicFactor);
			}

			if (changed_flags & MAT_ROUGHTNESS_DIRTY)
			{
				matp->setRoughnessFactor(editorp->getRoughnessFactor(), true);
			}
			else if (has_revert && (reverted_flags & MAT_ROUGHTNESS_DIRTY))
			{
				matp->setRoughnessFactor(revmatp->mRoughnessFactor);
			}

			if (changed_flags & MAT_DOUBLE_SIDED_DIRTY)
			{
				matp->setDoubleSided(editorp->getDoubleSided(), true);
			}
			else if (has_revert && (reverted_flags & MAT_DOUBLE_SIDED_DIRTY))
			{
				matp->setDoubleSided(revmatp->mDoubleSided);
			}

			if (te == mObjectTE && objectp->getID() == mObjectId)
			{
				mSuccess = true;
			}

			LLGLTFMaterialList::queueModify(objectp, te, matp.get());
		}
		return true;
	}

	bool getResult() const						{ return mSuccess; }

	static void modifyCallback(bool success)
	{
		if (!success)
		{
			// Something went wrong update selection
			llwarns << "Failed to update material" << llendl;
			LLPreviewMaterial::markForLiveUpdate();
		}
		// Else we will get a call to updateLive() from LLGLTFMaterialList
	}

private:
	LLUUID	mObjectId;
	S32		mObjectTE;
	bool	mSuccess;
};

void LLPreviewMaterial::applyToSelection()
{
	if (!mIsOverride || (!mUnsavedChanges && !mRevertedChanges))
	{
		return;
	}

	const std::string& url =
		gAgent.getRegionCapability("ModifyMaterialParams");
	if (url.empty())
	{
		llwarns << "Missing ModifyMaterialParams capability in this region"
				<< llendl;
		LLPointer<LLFetchedGLTFMaterial> matp(new LLFetchedGLTFMaterial());
		getGLTFMaterial(matp);
		static const LLUUID dummy("984e183e-7811-4b05-a502-d79c6f978a98");
		gGLTFMaterialList.addMaterial(dummy, matp);
		LLRenderMaterialFunctor mat_func(dummy);
		gSelectMgr.getSelection()->applyToTEs(&mat_func);
		return;
	}

	sOverrideInProgress = true;
	LLRenderMatOverrider func(sOverrideObjectId, sOverrideObjectTE);
	gSelectMgr.getSelection()->applyToNodes(&func);
	LLGLTFMaterialList::flushUpdates(&LLRenderMatOverrider::modifyCallback);
	if (!func.getResult())
	{
		sOverrideInProgress = false;
	}
	mUnsavedChanges = mRevertedChanges = 0;
}

void LLPreviewMaterial::getGLTFMaterial(LLGLTFMaterial* matp)
{
	if (!matp) return;	// Paranoia

	matp->mTextureId[BASECOLIDX] = getBaseColorId();
	matp->mTextureId[NORMALIDX] = getNormalId();
	matp->mTextureId[MROUGHIDX] = getMetallicRoughnessId();
	matp->mTextureId[EMISSIVEIDX] = getEmissiveId();
	matp->mBaseColor = getBaseColor();
	matp->mBaseColor.mV[3] = getTransparency();
	matp->mEmissiveColor = getEmissiveColor();
	matp->setAlphaMode(getAlphaMode());
	matp->mAlphaCutoff = getAlphaCutoff();
	matp->mMetallicFactor = getMetalnessFactor();
	matp->mRoughnessFactor = getRoughnessFactor();
	matp->mAlphaCutoff = getAlphaCutoff();
}

void LLPreviewMaterial::setFromGLTFMaterial(LLGLTFMaterial* matp)
{
	if (!matp) return;	// Paranoia

	setBaseColorId(matp->mTextureId[BASECOLIDX]);
	setNormalId(matp->mTextureId[NORMALIDX]);
	setMetallicRoughnessId(matp->mTextureId[MROUGHIDX]);
	setEmissiveId(matp->mTextureId[EMISSIVEIDX]);

	setBaseColor(matp->mBaseColor);
	setAlphaMode(matp->getAlphaMode());
	setAlphaCutoff(matp->mAlphaCutoff);
	setMetalnessFactor(matp->mMetallicFactor);
	setRoughnessFactor(matp->mRoughnessFactor);
	setEmissiveColor(matp->mEmissiveColor);

	setDoubleSided(matp->mDoubleSided);

	if (!matp->hasLocalTextures())
	{
		return;
	}

	for (LLGLTFMaterial::local_tex_map_t::const_iterator
			it = matp->mTrackingIdToLocalTexture.begin(),
			end = matp->mTrackingIdToLocalTexture.end();
		 it != end; ++it)
	{
		const LLUUID& tracking_id = it->first;
		const LLUUID& world_id = LLLocalBitmap::getWorldID(tracking_id);
		if (it->second != world_id)
		{
			llwarns << "World Id for local texture " << tracking_id
					<< " does not match." << llendl;
		}
		if (matp->mTextureId[BASECOLIDX] == world_id)
		{
			subscribeToLocalTexture(MAT_BASE_COLOR_TEX_DIRTY, tracking_id);
		}
		if (matp->mTextureId[NORMALIDX] == world_id)
		{
			subscribeToLocalTexture(MAT_NORMAL_TEX_DIRTY, tracking_id);
		}
		if (matp->mTextureId[MROUGHIDX] == world_id)
		{
			subscribeToLocalTexture(MAT_ROUGHTNESS_TEX_DIRTY, tracking_id);
		}
		if (matp->mTextureId[EMISSIVEIDX] == world_id)
		{
			subscribeToLocalTexture(MAT_EMISIVE_TEX_DIRTY, tracking_id);
		}
	}
}

class LLSelectedTEGetMatData final : public LLSelectedTEFunctor
{
protected:
	LOG_CLASS(LLSelectedTEGetMatData);

public:
	LLSelectedTEGetMatData(bool for_override)
	:	mIsOverride(for_override),
		mObject(NULL),
		mObjectTE(-1),
		mFirst(true),
		mIdenticalTexColor(true),
		mIdenticalTexMetal(true),
		mIdenticalTexEmissive(true),
		mIdenticalTexNormal(true)
	{
	}

	bool apply(LLViewerObject* objectp, S32 te_index) override
	{
		if (!objectp)
		{
			return false;
		}

		mMaterialId = objectp->getRenderMaterialID(te_index);
		if (mMaterialId.isNull())
		{
			return false;
		}

		bool can_use = mIsOverride ? objectp->permModify()
								   : objectp->permCopy();
		if (!can_use)
		{
			return false;
		}

		LLTextureEntry* tep = objectp->getTE(te_index);
		if (!tep)
		{
			return false;
		}

		if (!mIsOverride)
		{
			LLLocalGLTFMaterial* matp =
				dynamic_cast<LLLocalGLTFMaterial*>(tep->getGLTFMaterial());
			if (matp)
			{
				mLocalMaterial = matp;
			}

			mMaterial = tep->getGLTFRenderMaterial();
			if (mMaterial.isNull())
			{
				llwarns << "Object " << objectp->getID()
						<< " has material Id " << mMaterialId
						<< " but no render material." << llendl;
				mMaterial = gGLTFMaterialList.getMaterial(mMaterialId);
			}

			return true;
		}

		LLUUID tex_color_id, tex_metal_id, tex_emissive_id, tex_normal_id;
		LLPointer<LLGLTFMaterial> matp = tep->getGLTFRenderMaterial();
		if (matp.notNull())
		{
			tex_color_id = matp->mTextureId[BASECOLIDX];
			tex_metal_id = matp->mTextureId[MROUGHIDX];
			tex_emissive_id = matp->mTextureId[EMISSIVEIDX];
			tex_normal_id = matp->mTextureId[NORMALIDX];
		}
		if (mFirst)
		{
			mFirst = false;
			mObject = objectp;
			mObjectTE = te_index;
			mObjectId = objectp->getID();
			mMaterial = matp;
			mTexColorId = tex_color_id;
			mTexMetalId = tex_metal_id;
			mTexEmissiveId = tex_emissive_id;
			mTexNormalId = tex_normal_id;
		}
		else
		{
			if (mTexColorId != tex_color_id)
			{
				mIdenticalTexColor = false;
			}
			if (mTexMetalId != tex_metal_id)
			{
				mIdenticalTexMetal = false;
			}
			if (mTexEmissiveId != tex_emissive_id)
			{
				mIdenticalTexEmissive = false;
			}
			if (mTexNormalId != tex_normal_id)
			{
				mIdenticalTexNormal = false;
			}
		}

		return true;
	}

public:
	LLUUID							mObjectId;
	LLUUID							mMaterialId;
	LLUUID							mTexColorId;
	LLUUID							mTexMetalId;
	LLUUID							mTexEmissiveId;
	LLUUID							mTexNormalId;
	// Used by can_use_objects_material() to pass any found inv item Id for
	// this material. HB
	LLUUID							mInvItemId;
	LLViewerObject*					mObject;
	LLPointer<LLGLTFMaterial>		mMaterial;
	LLPointer<LLLocalGLTFMaterial>	mLocalMaterial;
	S32								mObjectTE;
	bool							mIdenticalTexColor;
	bool							mIdenticalTexMetal;
	bool							mIdenticalTexEmissive;
	bool							mIdenticalTexNormal;

private:
	bool							mIsOverride;
	bool							mFirst;
};

struct LLSelectedTEUpdateOverrides final : public LLSelectedNodeFunctor
{
	LLSelectedTEUpdateOverrides(LLPreviewMaterial* editorp)
	:	mEditor(editorp)
	{
	}

	bool apply(LLSelectNode* nodep) override
	{
		LLViewerObject* objectp = nodep->getObject();
		if (!objectp)
		{
			return false;
		}

		// Avatars have TEs but no faces.
		U8 num_tes = llmin(objectp->getNumTEs(), objectp->getNumFaces());
		for (U8 te = 0; te < num_tes; ++te)
		{
			LLTextureEntry* tep = objectp->getTE(te);
			if (!tep)
			{
				return false;
			}
			LLGLTFMaterial* omatp = tep->getGLTFMaterialOverride();
			if (mEditor->updateMaterialLocalSubscription(omatp))
			{
				LLGLTFMaterial* rmatp = tep->getGLTFRenderMaterial();
				mEditor->updateMaterialLocalSubscription(rmatp);
			}
		}

		return true;
	}

	LLPreviewMaterial* mEditor;
};

bool LLPreviewMaterial::setFromSelection()
{
	sSelectionNeedsUpdate = false;

	LLObjectSelectionHandle selectionp = gSelectMgr.getSelection();
	mHasSelection = !selectionp->isEmpty();

	LLSelectedTEGetMatData func(mIsOverride);
	selectionp->applyToTEs(&func);
	if (func.mMaterial.notNull())
	{
		setFromGLTFMaterial(func.mMaterial);
		setEnableEditing(true);

		// *TODO: apply local texture data to all materials in selection.
	}
	else
	{
		// Pick defaults from a blank material;
		LLGLTFMaterial blank_mat;
		setFromGLTFMaterial(&blank_mat);
		if (mIsOverride)
		{
			setEnableEditing(false);
		}
	}

	if (mIsOverride)
	{
		mBaseColorTexCtrl->setTentative(!func.mIdenticalTexColor);
		mMetallicTexCtrl->setTentative(!func.mIdenticalTexMetal);
		mEmissiveTexCtrl->setTentative(!func.mIdenticalTexEmissive);
		mNormalTexCtrl->setTentative(!func.mIdenticalTexNormal);

		// Memorize selection data for filtering further updates
		sOverrideObjectId = func.mObjectId;
		sOverrideObjectTE = func.mObjectTE;

		// Overrides might have been updated: refresh state of local textures.
		LLSelectedTEUpdateOverrides local_tex_func(this);
		selectionp->applyToNodes(&local_tex_func);
	}

	return func.mMaterial.notNull();
}

bool LLPreviewMaterial::setFromGltfModel(const tinygltf::Model& model, S32 idx,
										 bool set_textures)
{
	if (idx >= (S32)model.materials.size())
	{
		return false;
	}

	const tinygltf::Material& mat = model.materials[idx];

	if (set_textures)
	{
		LLUUID id;

		S32 i = mat.pbrMetallicRoughness.baseColorTexture.index;
		if (i >= 0)
		{
			id.set(model.images[i].uri);
			setBaseColorId(id);
		}
		else
		{
			setBaseColorId(LLUUID::null);
		}

		i = mat.normalTexture.index;
		if (i >= 0)
		{
			id.set(model.images[i].uri);
			setNormalId(id);
		}
		else
		{
			setNormalId(LLUUID::null);
		}

		i = mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
		if (i >= 0)
		{
			id.set(model.images[i].uri);
			setMetallicRoughnessId(id);
		}
		else
		{
			setMetallicRoughnessId(LLUUID::null);
		}

		i = mat.emissiveTexture.index;
		if (i >= 0)
		{
			id.set(model.images[i].uri);
			setEmissiveId(id);
		}
		else
		{
			setEmissiveId(LLUUID::null);
		}
	}

	setAlphaMode(mat.alphaMode);
	setAlphaCutoff(mat.alphaCutoff);

	setBaseColor(LLTinyGLTFHelper::getColor(mat.pbrMetallicRoughness.baseColorFactor));
	setEmissiveColor(LLTinyGLTFHelper::getColor(mat.emissiveFactor));

	setMetalnessFactor(mat.pbrMetallicRoughness.metallicFactor);
	setRoughnessFactor(mat.pbrMetallicRoughness.roughnessFactor);

	setDoubleSided(mat.doubleSided);

	return true;
}

std::string LLPreviewMaterial::getImageNameFromUri(std::string image_uri,
												   std::string texture_type)
{
	// Make the texture type all lower case. HB
	LLStringUtil::toLower(texture_type);

	// Replace alien directory limiters so that getBaseFileName() works.
#if LL_WINDOWS
	LLStringUtil::replaceChar(image_uri, '/', LL_DIR_DELIM_CHR);
#else
	LLStringUtil::replaceChar(image_uri, '\\', LL_DIR_DELIM_CHR);
#endif
	// Use the base file name, limited to 64 characters as the image URI.
	image_uri = gDirUtilp->getBaseFileName(image_uri, true);
	if (image_uri.size() > 64)
	{
		image_uri.resize(64);
	}

	std::string uri = image_uri;	
	// Lower-case it for comparison
	LLStringUtil::toLower(uri);
	// Remove spacing characters from URI
	uri.erase(std::remove_if(uri.begin(), uri.end(), isspace), uri.end());
	if (uri.empty())
	{
		// URI is empty, so we must reorganize the string a bit to include the
		// name and an explicit name type. E.g. "DamagedHelmet: (emissive)".
		return llformat("%s (%s)", mMaterialNameShort.c_str(),
						texture_type.c_str());
	}

	// Remove spacing characters from texture type
	std::string type = texture_type;
	type.erase(std::remove_if(type.begin(), type.end(), isspace), type.end());
	// Let's see if texture_type is already part of the URI.
	if (uri.find(type) != std::string::npos)
	{
		// It is indeed part of it, so just use it directly with the name of
		// the material. E.g. AlienBust: normal_layer
		return llformat("%s: %s", mMaterialNameShort.c_str(),
						image_uri.c_str());
	}

	// URI does not include the type and is not empty, so we can include
	// everything. E.g. "DamagedHelmet: base layer (base color)"
	return llformat("%s: %s (%s)", mMaterialNameShort.c_str(),
					image_uri.c_str(), texture_type.c_str());
}

void LLPreviewMaterial::setFromGltfMetaData(const std::string& filename,
											const tinygltf::Model& model,
											S32 index)
{
	mMaterialNameShort = gDirUtilp->getBaseFileName(filename, true);
	LLInventoryObject::correctInventoryName(mMaterialNameShort);

	S32 count = model.materials.size();

	std::string material_name;
	if (count > index && !model.materials[index].name.empty())
	{
		material_name = model.materials[index].name;
	}
	else if (model.scenes.size())
	{
		const tinygltf::Scene& scene_in = model.scenes[0];
		if (scene_in.name.size())
		{
			material_name = scene_in.name;
		}
	}
	if (material_name.empty())
	{
		mMaterialName = mMaterialNameShort;
	}
	else
	{
		mMaterialName = llformat("%s (%s)", mMaterialNameShort.c_str(),
								 material_name.c_str());
		LLInventoryObject::correctInventoryName(mMaterialName);
	}

	setTitle(mMaterialName);

	// For ease of inventory management, we prepend the material name.
	std::string base_name = mMaterialName + ": ";
	mBaseColorName = base_name + MAT_BASE_COLOR_DEFAULT_NAME;
	mNormalName = base_name + MAT_NORMAL_DEFAULT_NAME;
	mMetallicRoughnessName = base_name + MAT_METALLIC_DEFAULT_NAME;
	mEmissiveName = base_name + MAT_EMISSIVE_DEFAULT_NAME;

	if (index < 0 || index >= count)
	{
		return;
	}

	S32 images = model.images.size();

	const tinygltf::Material& first_mat = model.materials[index];

	S32 i = first_mat.pbrMetallicRoughness.baseColorTexture.index;
	if (i >= 0 && i < images)
	{
		mBaseColorName = getImageNameFromUri(model.images[i].uri,
											 MAT_BASE_COLOR_DEFAULT_NAME);
		LLInventoryObject::correctInventoryName(mBaseColorName);
	}

	i = first_mat.normalTexture.index;
	if (i >= 0 && i < images)
	{
		mNormalName = getImageNameFromUri(model.images[i].uri,
										  MAT_NORMAL_DEFAULT_NAME);
		LLInventoryObject::correctInventoryName(mNormalName);
	}

	i = first_mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
	if (i >= 0 && i < images)
	{
		mMetallicRoughnessName =
			getImageNameFromUri(model.images[i].uri,
								MAT_METALLIC_DEFAULT_NAME);
		LLInventoryObject::correctInventoryName(mMetallicRoughnessName);
	}

	i = first_mat.emissiveTexture.index;
	if (i >= 0 && i < images)
	{
		mEmissiveName = getImageNameFromUri(model.images[i].uri,
											MAT_EMISSIVE_DEFAULT_NAME);
		LLInventoryObject::correctInventoryName(mEmissiveName);
	}
}

void LLPreviewMaterial::loadMaterial(const tinygltf::Model& model,
									 const std::string& filename, S32 index)
{
	S32 count = model.materials.size();
	if (index < 0 || index >= count)
	{
		llwarns << "Material index (" << index << ") out of range for file: "
				<< filename << " - Max index is: " << count - 1 << llendl;
		return;
	}

	std::string folder = gDirUtilp->getDirName(filename);
	tinygltf::Material mat = model.materials[index];
	tinygltf::Model  model_out;
	model_out.asset.version = "2.0";
	model_out.materials.resize(1);
	// Get base color texture
	LLPointer<LLImageRaw> base_imgp =
		LLTinyGLTFHelper::getTexture(folder, model,
									 mat.pbrMetallicRoughness.baseColorTexture.index,
									 mBaseColorName);
	// Get normal texture
	LLPointer<LLImageRaw> norm_imgp =
		LLTinyGLTFHelper::getTexture(folder, model, mat.normalTexture.index,
									 mNormalName);
	// Get metallic-roughness texture
	LLPointer<LLImageRaw> mr_imgp =
		LLTinyGLTFHelper::getTexture(folder, model,
									 mat.pbrMetallicRoughness.metallicRoughnessTexture.index,
									 mMetallicRoughnessName);
	// Get emissive texture
	LLPointer<LLImageRaw> em_imgp =
		LLTinyGLTFHelper::getTexture(folder, model, mat.emissiveTexture.index,
									 mNormalName);

	// Get occlusion  map if needed
	LLPointer<LLImageRaw> occl_imgp;
	if (mat.occlusionTexture.index !=
			mat.pbrMetallicRoughness.metallicRoughnessTexture.index)
	{
		std::string tmp;
		occl_imgp = LLTinyGLTFHelper::getTexture(folder, model,
												 mat.occlusionTexture.index,
												 tmp);
	}

	LLTinyGLTFHelper::initFetchedTextures(mat, base_imgp, norm_imgp, mr_imgp,
										  em_imgp, occl_imgp,
										  mBaseColorFetched, mNormalFetched,
										  mMetallicRoughnessFetched,
										  mEmissiveFetched);
	if (base_imgp.notNull())
	{
		mBaseColorJ2C = LLViewerTextureList::convertToUploadFile(base_imgp);
	}
	if (norm_imgp.notNull())
	{
		mNormalJ2C = LLViewerTextureList::convertToUploadFile(norm_imgp, 1024,
															  true);
	}
	if (mr_imgp.notNull())
	{
		mMetallicRoughnessJ2C =
			LLViewerTextureList::convertToUploadFile(mr_imgp);
	}
	if (em_imgp.notNull())
	{
		mEmissiveJ2C = LLViewerTextureList::convertToUploadFile(em_imgp);
	}

	LLUUID base_color_id;
	if (mBaseColorFetched.notNull())
	{
		mBaseColorFetched->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
		mBaseColorFetched->forceToSaveRawImage(0, F32_MAX);
		base_color_id = mBaseColorFetched->getID();
		if (mBaseColorName.empty())
		{
			mBaseColorName = MAT_BASE_COLOR_DEFAULT_NAME;
		}
	}
	setBaseColorId(base_color_id);
	setBaseColorUploadId(base_color_id);

	LLUUID normal_id;
	if (mNormalFetched.notNull())
	{
		mNormalFetched->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
		mNormalFetched->forceToSaveRawImage(0, F32_MAX);
		normal_id = mNormalFetched->getID();
		if (mNormalName.empty())
		{
			mNormalName = MAT_NORMAL_DEFAULT_NAME;
		}
	}
	setNormalId(normal_id);
	setNormalUploadId(normal_id);

	LLUUID mr_id;
	if (mMetallicRoughnessFetched.notNull())
	{
		mMetallicRoughnessFetched->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
		mMetallicRoughnessFetched->forceToSaveRawImage(0, F32_MAX);
		mr_id = mMetallicRoughnessFetched->getID();
		if (mMetallicRoughnessName.empty())
		{
			mMetallicRoughnessName = MAT_METALLIC_DEFAULT_NAME;
		}
	}
	setMetallicRoughnessId(mr_id);
	setMetallicRoughnessUploadId(mr_id);

	LLUUID emissive_id;
	if (mEmissiveFetched.notNull())
	{
		mEmissiveFetched->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
		mEmissiveFetched->forceToSaveRawImage(0, F32_MAX);
		emissive_id = mEmissiveFetched->getID();
		if (mEmissiveName.empty())
		{
			mEmissiveName = MAT_EMISSIVE_DEFAULT_NAME;
		}
	}
	setEmissiveId(emissive_id);
	setEmissiveUploadId(emissive_id);

	setFromGltfModel(model, index);
	setFromGltfMetaData(filename, model, index);

	// SL-19392: double sided materials double the number of pixels that must
	// be rasterized,and a great many tools that export GLTF simply leave
	// double sided enabled whether or not it is necessary.
	if (getDoubleSided())
	{
		setDoubleSided(false);
	}

	mCanCopy = true;
	mAssetStatus = PREVIEW_ASSET_LOADED;
	markChangesUnsaved(U32_MAX);

	setFocus(true);

	applyToSelection();
}

//static
void LLPreviewMaterial::onSelectionChanged()
{
	// Drop selection updates if we are waiting for overrides to finish
	// applying to not reset values (might need a timeout).
	if (!sOverrideInProgress)
	{
		sSelectionNeedsUpdate = true;
	}
}

//static
void LLPreviewMaterial::markForLiveUpdate()
{
	if (sOverrideInProgress)
	{
		LL_DEBUGS("GLTF") << "Updating live material from selection"
						  << LL_ENDL;
	}
	sSelectionNeedsUpdate = true;
	sOverrideInProgress = false;
}

//static
void LLPreviewMaterial::updateLive(const LLUUID& object_id, S32 te)
{
	if (object_id != sOverrideObjectId || te != sOverrideObjectTE)
	{
		// Ignore if waiting for override, but if not waiting, mark selection
		// dirty.
		LL_DEBUGS("GLTF") << "Received a stale object update. Ignoring."
						  << LL_ENDL;
		sSelectionNeedsUpdate = !sOverrideInProgress;
		return;
	}
	LL_DEBUGS("GLTF") << "Updating live material from selection"
					  << LL_ENDL;

	// Mark object for rebuild. HB
	LLViewerObject* objectp = gObjectList.findObject(sOverrideObjectId);
	if (objectp)
	{
		objectp->refreshMaterials();
	}

	sSelectionNeedsUpdate = true;
	sOverrideInProgress = false;
}

//static
void LLPreviewMaterial::loadLive()
{
	if (!sLiveEditorInstance)
	{
		LL_DEBUGS("GLTF") << "Creating a new live editor instance..."
						  << LL_ENDL;
		sLiveEditorInstance = new LLPreviewMaterial("live editor", true);
	}
	LL_DEBUGS("GLTF") << "Loading live material from selection" << LL_ENDL;
	sOverrideInProgress = false;
	sLiveEditorInstance->setFromSelection();
	if (!sSelectionUpdateSlot.connected())
	{
		sSelectionUpdateSlot =
			gSelectMgr.mUpdateSignal.connect(boost::bind(&onSelectionChanged));
	}
	sLiveEditorInstance->open();
	sLiveEditorInstance->setFocus(true);
}

//static
LLPreviewMaterial* LLPreviewMaterial::loadFromFile(const std::string& filename,
												   S32 index)
{
	std::string error_msg, warn_msg;
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	bool loaded = false;
	std::string exten = gDirUtilp->getExtension(filename);
	if (exten == "gltf")
	{
		loaded = loader.LoadASCIIFromFile(&model, &error_msg, &warn_msg,
										  filename);
	}
	else
	{
		loaded = loader.LoadBinaryFromFile(&model, &error_msg, &warn_msg,
										   filename);
	}
	if (!loaded || model.materials.empty())
	{
		gNotifications.add("CannotUploadMaterial");
		return NULL;
	}

	S32 count = model.materials.size();

	LLPreviewMaterial* self = NULL;
	if (index >= 0)
	{
		if (index < count)
		{
			self = new LLPreviewMaterial("material_preview");
			self->loadMaterial(model, filename, index);
		}
		else
		{
			gNotifications.add("CannotUploadMaterialIndex");
		}
		return self;
	}

	// Open as many material previews as there are materials in the file. HB
	for (S32 i = 0; i < count; ++i)
	{
		LLPreviewMaterial* self = new LLPreviewMaterial("material_preview");
		self->loadMaterial(model, filename, i);
	}
	// Return a pointer on the last opened preview floater.
	return self;
}

U32 LLPreviewMaterial::getDirtyFlagFromCtrl(LLUICtrl* ctrlp)
{
	// Spinners first, as they are high frequency events. HB
	if (ctrlp == (LLUICtrl*)mTransparencyCtrl)
	{
		return MAT_TRANSPARENCY_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mAlphaCutoffCtrl)
	{
		return MAT_ALPHA_CUTOFF_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mMetalnessCtrl)
	{
		return MAT_METALNESS_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mRoughnessCtrl)
	{
		return MAT_ROUGHTNESS_DIRTY;
	}
	// Texture and color controls, combo and check box last, as they are low
	// frequency events. HB
	if (ctrlp == (LLUICtrl*)mBaseColorCtrl)
	{
		return MAT_BASE_COLOR_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mBaseColorTexCtrl)
	{
		return MAT_BASE_COLOR_TEX_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mNormalTexCtrl)
	{
		return MAT_NORMAL_TEX_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mMetallicTexCtrl)
	{
		return MAT_ROUGHTNESS_TEX_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mEmissiveTexCtrl)
	{
		return MAT_EMISIVE_TEX_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mEmissiveColorCtrl)
	{
		return MAT_EMISIVE_COLOR_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mAlphaModeCombo)
	{
		return MAT_ALPHA_MODE_DIRTY;
	}
	if (ctrlp == (LLUICtrl*)mDoubleSidedCheck)
	{
		return MAT_DOUBLE_SIDED_DIRTY;
	}
	return 0;
}

static void check_for_local_texture(LLUICtrl* ctrlp, LLGLTFMaterial* matp)
{
	LLTextureCtrl* texctrlp = (LLTextureCtrl*)ctrlp;
	if (texctrlp->isImageLocal())
	{
		// Subscribe material to updates of local textures
		LLLocalBitmap::associateGLTFMaterial(texctrlp->getLocalTrackingID(),
											 matp);
	}
}

struct LLSelectedMaterialChangeFunctor final : public LLSelectedNodeFunctor
{
	LLSelectedMaterialChangeFunctor(LLUICtrl* ctrlp, U32 dirty_flag)
	:	mCtrl(ctrlp),
		mDirtyFlag(dirty_flag)
	{
		if (!dirty_flag)
		{
			return;
		}
		if (dirty_flag <= MAT_EMISIVE_TEX_DIRTY)
		{
			mTextureId = ctrlp->getValue().asUUID();
		}
		else if (dirty_flag <= MAT_EMISIVE_COLOR_DIRTY)
		{
			mColor = LLColor4(ctrlp->getValue());
		}
		else if (dirty_flag == MAT_ALPHA_MODE_DIRTY)
		{
			mValue = F32(ctrlp->getValue().asInteger());
		}
		else if (dirty_flag == MAT_DOUBLE_SIDED_DIRTY)
		{
			mValue = ctrlp->getValue().asBoolean() ? 1.f : 0.f;
		}
		else
		{
			mValue = ctrlp->getValue().asReal();
		}
	}

	bool apply(LLSelectNode* nodep) override
	{
		LLViewerObject* objectp = nodep->getObject();
		if (!objectp)
		{
			return false;
		}

		// Avatars have TEs but no faces.
		U8 num_tes = llmin(objectp->getNumTEs(), objectp->getNumFaces());
		for (U8 te = 0; te < num_tes; ++te)
		{
			if (!nodep->isTESelected(te) ||
				te >= nodep->mSavedGLTFOverrideMaterials.size())
			{
				continue;
			}

			LLPointer<LLGLTFMaterial>& matp =
				nodep->mSavedGLTFOverrideMaterials[te];
			if (matp.isNull())
			{
				// Populate with default values, default values basically mean
				// 'not in use'.
				matp = new LLGLTFMaterial();
			}

			switch (mDirtyFlag)
			{
				case MAT_BASE_COLOR_TEX_DIRTY:
					matp->setBaseColorId(mTextureId, true);
					check_for_local_texture(mCtrl, matp);
					break;

				case MAT_NORMAL_TEX_DIRTY:
					matp->setNormalId(mTextureId, true);
					check_for_local_texture(mCtrl, matp);
					break;

				case MAT_ROUGHTNESS_TEX_DIRTY:
					matp->setMetallicRoughnessId(mTextureId, true);
					check_for_local_texture(mCtrl, matp);
					break;

				case MAT_EMISIVE_TEX_DIRTY:
					matp->setEmissiveId(mTextureId, true);
					check_for_local_texture(mCtrl, matp);
					break;

				case MAT_BASE_COLOR_DIRTY:
				{
					LLColor4 color = linearColor4(mColor);
					// Do not touch the transparency value
					color.mV[3] = matp->mBaseColor.mV[3];
					matp->setBaseColorFactor(color, true);
					break;
				}

				case MAT_EMISIVE_COLOR_DIRTY:
					matp->setEmissiveColorFactor(LLColor3(mColor), true);
					break;

				case MAT_TRANSPARENCY_DIRTY:
				{
					LLColor4 color = matp->mBaseColor;
					// Only touch the transparency value
					color.mV[3] = mValue;
					matp->setBaseColorFactor(color, true);
					break;
				}

				case MAT_ALPHA_MODE_DIRTY:
					matp->setAlphaMode(U32(mValue), true);
					break;

				case MAT_ALPHA_CUTOFF_DIRTY:
					matp->setAlphaCutoff(mValue, true);
					break;

				case MAT_METALNESS_DIRTY:
					matp->setMetallicFactor(mValue, true);
					break;

				case MAT_ROUGHTNESS_DIRTY:
					matp->setRoughnessFactor(mValue, true);
					break;

				case MAT_DOUBLE_SIDED_DIRTY:
					matp->setDoubleSided(mValue != 0.f, true);

				default:
					break;
			}
		}
		return true;
	}

	LLUICtrl*	mCtrl;
	LLUUID		mTextureId;
	LLColor4	mColor;
	U32			mDirtyFlag;
	F32			mValue;
};

//static
void LLPreviewMaterial::onSelectCtrl(LLUICtrl* ctrlp, void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (!self || !ctrlp)
	{
		return;
	}

	U32 dirty_flag = self->getDirtyFlagFromCtrl(ctrlp);
	self->mUnsavedChanges |= dirty_flag;
	self->applyToSelection();

	// If needed, propagate any change in textures or colors
	if (self->mIsOverride)
	{
		LLSelectedMaterialChangeFunctor func(ctrlp, dirty_flag);
		gSelectMgr.getSelection()->applyToNodes(&func);
	}
}

//static
void LLPreviewMaterial::onTextureCtrl(LLUICtrl* ctrlp, void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (self && ctrlp)
	{
		U32 dirty_flag = self->getDirtyFlagFromCtrl(ctrlp);
		// Note: as long as onTextureCtrl() is only used with an LLTextureCtrl,
		// the static cast is valid. HB
		LLTextureCtrl* tctrlp = (LLTextureCtrl*)ctrlp;
		if (tctrlp->isImageLocal())
		{
			self->subscribeToLocalTexture(dirty_flag,
										  tctrlp->getLocalTrackingID());
		}
		else
		{
			// Unsubcribe potential old callback
			connection_map_t::iterator it =
				self->mTextureChangesUpdates.find(dirty_flag);
			if (it != self->mTextureChangesUpdates.end())
			{
				it->second.mConnection.disconnect();
			}
		}
		self->markChangesUnsaved(dirty_flag);
		self->applyToSelection();
	}
}

//static
void LLPreviewMaterial::onCancelCtrl(LLUICtrl* ctrlp, void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (self && ctrlp)
	{
		self->mRevertedChanges |= self->getDirtyFlagFromCtrl(ctrlp);
		self->applyToSelection();
	}
}

bool LLPreviewMaterial::onCancelMsgCallback(const LLSD& notification,
											const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		close();
	}
	return false;
}

//static
void LLPreviewMaterial::onClickCancel(void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (!self)
	{
		return;
	}

	if (self->mIsOverride || !self->mUnsavedChanges)
	{
		self->close();
		return;
	}

	gNotifications.add("UnsavedMaterialChanges", LLSD(), LLSD(),
					   boost::bind(&LLPreviewMaterial::onCancelMsgCallback,
								   self, _1, _2));
}

//static
void LLPreviewMaterial::onClickSave(void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (!self)
	{
		return;
	}

	if (!gAgent.hasInventoryMaterial())
	{
		gNotifications.add("MissingMaterialCaps");
		return;
	}

	if (!can_afford_transaction(self->mExpectedUploadCost))
	{
		LLSD args;
		args["COST"] = llformat("%d", self->mExpectedUploadCost);
		gNotifications.add("ErrorCannotAffordUpload", args);
		return;
	}

	self->applyToSelection();
	self->saveIfNeeded();
}

//static
void LLPreviewMaterial::onClickSaveAs(void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (!self)
	{
		return;
	}

	if (!gAgent.hasInventoryMaterial())
	{
		gNotifications.add("MissingMaterialCaps");
		return;
	}

	if (!can_afford_transaction(self->mExpectedUploadCost))
	{
		LLSD args;
		args["COST"] = llformat("%d", self->mExpectedUploadCost);
		gNotifications.add("ErrorCannotAffordUpload", args);
		return;
	}

	LLSD args;
	args["DESC"] = self->mMaterialName;
	gNotifications.add("SaveMaterialAs", args, LLSD(),
					   boost::bind(&LLPreviewMaterial::onSaveAsMsgCallback,
								   self, _1, _2));
}

class LLMaterialCopiedCB final : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLMaterialCopiedCB);

public:
	LLMaterialCopiedCB(LLPreviewMaterial* previewp,
					   const std::string& buffer)
	:	mPreviewp(previewp),
		mBuffer(buffer)
	{
	}

	void fire(const LLUUID& inv_item_id) override
	{
		if (mPreviewp && sEditorInstances.count(mPreviewp))	// Still around ?
		{
			mPreviewp->finishSaveAs(inv_item_id, mBuffer);
		}
	}

private:
	LLPreviewMaterial*	mPreviewp;
	std::string			mBuffer;
};

void LLPreviewMaterial::finishSaveAs(const LLUUID& new_item_id,
									 const std::string& buffer)
{
	LLViewerInventoryItem* itemp = gInventory.getItem(new_item_id);
	if (!itemp)
	{
		llwarns << "Cannot find the inventory item " << new_item_id << llendl;
		setEnabled(true);
		return;
	}

	setItemID(new_item_id);
	mObjectUUID.setNull();
	mAuxItem = NULL;
	setMaterialName(itemp->getName());

	if (!mUnsavedChanges)
	{
		loadAsset();
		setEnabled(true);

		if (mTextureChangesUpdates.empty())
		{
			return;
		}
		LLGLTFMaterial* matp =
			gGLTFMaterialList.getMaterial(itemp->getAssetUUID());
		if (!matp)
		{
			return;
		}
		// Local textures were assigned, force load material and init tracking
		for (connection_map_t::iterator it = mTextureChangesUpdates.begin(),
										end = mTextureChangesUpdates.end();
			 it != end; ++it)
		{
			LLLocalBitmap::associateGLTFMaterial(it->second.mTrackingId, matp);
		}
	}
	else if (!updateInventoryItem(buffer, new_item_id, LLUUID::null))
	{
		setEnabled(true);
	}
}

bool LLPreviewMaterial::onSaveAsMsgCallback(const LLSD& notification,
											const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)	// Yes
	{
		std::string new_name = response["message"].asString();
		LLInventoryObject::correctInventoryName(new_name);
		if (new_name.empty())
		{
			gNotifications.add("InvalidMaterialName");
			return false;	
		}
		const LLInventoryItem* itemp = getItem();
		if (!itemp)
		{
			setMaterialName(new_name);
			onClickSave(this);
			return false;	
		}

		LLPointer<LLInventoryCallback> cb =
			new LLMaterialCopiedCB(this, getEncodedAsset());
		copy_inventory_item(itemp->getPermissions().getOwner(),
							itemp->getUUID(), itemp->getParentUUID(), new_name,
							cb);
		mAssetStatus = PREVIEW_ASSET_LOADING;
		setEnabled(false);
	}
	return false;	
}

static bool can_use_objects_material(LLSelectedTEGetMatData& func,
									 const std::vector<U32>& ops,
									 LLPermissions& permissions)
{
	if (!gAgent.hasInventoryMaterial())
	{
		return false;
	}

	gSelectMgr.getSelection()->applyToTEs(&func, true);
	LLViewerObject* objectp = func.mObject;
	if (!objectp || objectp->isInventoryPending())
	{
		return false;
	}

	if (objectp->isPermanentEnforced())
	{
		for (U32 i = 0, count = ops.size(); i < count; ++i)
		{
			if (ops[i] == PERM_MODIFY)
			{
				return false;
			}
		}
	}

	LLPermissions item_perms;
	LLViewerInventoryItem* itemp =
		objectp->getInventoryItemByAsset(func.mMaterialId);
	if (itemp)
	{
		item_perms.set(itemp->getPermissions());
		for (U32 i = 0, count = ops.size(); i < count; ++i)
		{
			if (!gAgent.allowOperation(ops[i], item_perms,
									   GP_OBJECT_MANIPULATE))
			{
				return false;
			}
		}
		// Update flags for new owner
		if (!item_perms.setOwnerAndGroup(LLUUID::null, gAgentID,
										 LLUUID::null, true))
		{
			return false;
		}
	}
	else
	{
		item_perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
	}

	// Use the root object for permissions checking
	LLViewerObject* rootp = objectp->getRootEdit();
	LLPermissions obj_perms;
	LLPermissions* permsp = gSelectMgr.findObjectPermissions(rootp);
	if (permsp)
	{
		obj_perms.set(*permsp);
		for (U32 i = 0, count = ops.size(); i < count; ++i)
		{
			if (!gAgent.allowOperation(ops[i], obj_perms,
									   GP_OBJECT_MANIPULATE))
			{
				return false;
			}
		}
		// Update flags for new owner
		if (!obj_perms.setOwnerAndGroup(LLUUID::null, gAgentID,
										LLUUID::null, true))
		{
			return false;
		}
	}
	else
	{
		obj_perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
	}

	LLPermissions default_perms;
	default_perms.setMaskEveryone(LLFloaterPerms::getEveryonePerms());
	default_perms.setMaskGroup(LLFloaterPerms::getGroupPerms());
	default_perms.setMaskNext(LLFloaterPerms::getNextOwnerPerms());

	// Note: a close inspection of LLPermissions::accumulate shows that
	// conflicting UUIDs will be unset. This is acceptable behavior for now.
	// The server will populate creator info based on the item creation method
	// used. There is currently no good way to preserve creation history when
	// there is no material item present. In that case, the agent who saved the
	// material will be considered the creator.
	if (itemp)
	{
		func.mInvItemId = itemp->getUUID();
		permissions.set(item_perms);
	}
	else
	{
		permissions.set(obj_perms);
	}
	permissions.accumulate(default_perms);

	return true;
}

//static
bool LLPreviewMaterial::canModifyObjectsMaterial()
{
	static const std::vector<U32> perm_mod({ PERM_MODIFY });
	LLSelectedTEGetMatData func(true);
	LLPermissions permissions;
	return can_use_objects_material(func, perm_mod, permissions);
}

//static
bool LLPreviewMaterial::canSaveObjectsMaterial()
{
	static const std::vector<U32> perm_copy_mod({ PERM_COPY, PERM_MODIFY });
	LLSelectedTEGetMatData func(true);
	LLPermissions permissions;
	return can_use_objects_material(func, perm_copy_mod, permissions);
}

//static
void LLPreviewMaterial::saveObjectsMaterial()
{
	static const std::vector<U32> perm_copy_mod({ PERM_COPY, PERM_MODIFY });
	LLSelectedTEGetMatData func(true);
	LLPermissions permissions;
	if (!can_use_objects_material(func, perm_copy_mod, permissions))
	{
		return;
	}

	const LLPointer<LLLocalGLTFMaterial>& localmatp = func.mLocalMaterial;
	const LLPointer<LLGLTFMaterial>& matp = func.mMaterial;

	if (localmatp.notNull() && matp.notNull())
	{
		// This is a local material, reload it from file so that the user would
		// not end up with grey textures on next login.
		LLPreviewMaterial* self = loadFromFile(localmatp->getFilename(),
											   localmatp->getIndexInFile());
		if (!self)
		{
			return;	// Failed !
		}
		// Do not use override material here, it has 'hacked ids' and values;
		// use end result, apply it on top of local.
		const LLColor4& base_color = matp->mBaseColor;
		self->setBaseColor(LLColor3(base_color));
		self->setTransparency(base_color[VW]);
		self->setAlphaMode(matp->getAlphaMode());
		self->setAlphaCutoff(matp->mAlphaCutoff);
		self->setMetalnessFactor(matp->mMetallicFactor);
		self->setRoughnessFactor(matp->mRoughnessFactor);
		self->setDoubleSided(matp->mDoubleSided);

		// Most things like colors we can apply without verifying, but textures
		// Ids are going to be different from both, base and override, so only
		// apply override Id if there is actually a difference.
		if (localmatp->mTextureId[BASECOLIDX] != matp->mTextureId[BASECOLIDX])
		{
			self->setBaseColorId(matp->mTextureId[BASECOLIDX]);
		}
		if (localmatp->mTextureId[NORMALIDX] != matp->mTextureId[NORMALIDX])
		{
			self->setNormalId(matp->mTextureId[NORMALIDX]);
		}
		if (localmatp->mTextureId[MROUGHIDX] != matp->mTextureId[MROUGHIDX])
		{
			self->setMetallicRoughnessId(matp->mTextureId[MROUGHIDX]);
		}
		if (localmatp->mTextureId[EMISSIVEIDX] !=
				matp->mTextureId[EMISSIVEIDX])
		{
			self->setEmissiveId(matp->mTextureId[EMISSIVEIDX]);
		}
		// Recalculate upload cost.
		self->markChangesUnsaved(0);
	}

	LLSD payload;
	if (matp.notNull())
	{
		// Make a copy of the render material with unsupported transforms
		// removed
		LLGLTFMaterial asset_mat = *matp;
		asset_mat.sanitizeAssetMaterial();
		payload["data"] = asset_mat.asJSON();
	}
	else
	{
		// This should not happen, but just in case, use a blank material.
		LLGLTFMaterial blank_mat;
		payload["data"] = blank_mat.asJSON();
		llwarns << "Got no material when trying to save selected faces material"
				<< llendl;
	}
	LLSD args;
	args["DESC"] = LLTrans::getString("New Material");
	if (localmatp.isNull() && func.mInvItemId.notNull())
	{
		payload["object_id"] = func.mObjectId;
		payload["item_id"] = func.mInvItemId;
		permissions.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
	}
	gNotifications.add("SaveMaterialAs", args, payload,
					   boost::bind(&LLPreviewMaterial::onSaveObjectsMaterialCB,
								   _1, _2, permissions));
}

class LLMaterialInventoryCB final : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLMaterialInventoryCB);

public:
	LLMaterialInventoryCB(const LLPermissions& permissions,
						  const std::string& buffer,
						  const std::string& item_name)
	:	mPermissions(permissions),
		mBuffer(buffer),
		mItemName(item_name)
	{
	}

	void fire(const LLUUID& inv_item_id) override
	{
		LLViewerInventoryItem* itemp = gInventory.getItem(inv_item_id);
		if (!itemp) return;

		// create_inventory_item() does not allow presetting some permissions;
		// fix it now.
		itemp->setPermissions(mPermissions);
		itemp->updateServer(false);
		gInventory.updateItem(itemp);
		gInventory.notifyObservers();

		if (itemp->getName() != mItemName)
		{
			LLSD updates;
			updates["name"] = mItemName;
			update_inventory_item(inv_item_id, updates, NULL);
		}

		LLResourceUploadInfo::ptr_t infop =
		std::make_shared<LLBufferedAssetUploadInfo>(
			inv_item_id, LLAssetType::AT_MATERIAL, mBuffer, uploadDone);
		const std::string& cap_url =
			gAgent.getRegionCapability("UpdateMaterialAgentInventory");
		LLViewerAssetUpload::enqueueInventoryUpload(cap_url, infop);
	}

	static void uploadDone(LLUUID, LLUUID, LLUUID, LLSD)
	{
		gNotifications.add("MaterialCreated");
	}

private:
	std::string		mBuffer;
	std::string		mItemName;
	LLPermissions	mPermissions;
};

//static
bool LLPreviewMaterial::onSaveObjectsMaterialCB(const LLSD& notification,
												const LLSD& response,
												const LLPermissions& perms)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLSD asset;
		asset["version"] = LLGLTFMaterial::ASSET_VERSION;
		asset["type"] = LLGLTFMaterial::ASSET_TYPE;
		// This is the string serialized from LLGLTFMaterial::asJSON
		asset["data"] = notification["payload"]["data"];
		std::stringstream buffer;
		LLSDSerialize::serialize(asset, buffer, LLSDSerialize::LLSD_BINARY);
		std::string new_name = response["message"].asString();
		if (notification["payload"].has("item_id"))
		{
			LLUUID object_id = notification["payload"]["object_id"];
			LLViewerObject* objectp = gObjectList.findObject(object_id);
			if (!objectp) return false;

			LLUUID item_id = notification["payload"]["item_id"];
			const LLInventoryItem* itemp = objectp->getInventoryItem(item_id);
			if (!itemp) return false;

			const LLUUID& mat_cat_id =
				gInventory.findCategoryUUIDForType(LLFolderType::FT_MATERIAL);
			LLPointer<LLInventoryCallback> cb =
				new LLMaterialInventoryCB(perms, buffer.str(), new_name);
			move_or_copy_item_from_object(mat_cat_id, object_id, item_id, cb);
		}
		else
		{
			createInventoryItem(buffer.str(), new_name, LLStringUtil::null,
								perms);
		}
	}
	return false;
}

std::string LLPreviewMaterial::getEncodedAsset()
{
	LLSD asset;
	asset["version"] = LLGLTFMaterial::ASSET_VERSION;
	asset["type"] = LLGLTFMaterial::ASSET_TYPE;
	LLGLTFMaterial mat;
	getGLTFMaterial(&mat);
	asset["data"] = mat.asJSON();

	std::stringstream buffer;
	LLSDSerialize::serialize(asset, buffer, LLSDSerialize::LLSD_BINARY);
	return buffer.str();	
}

bool LLPreviewMaterial::decodeAsset(const std::string& buffer)
{
	std::stringstream llsdstream(buffer);
	LLSD asset;
	if (!LLSDSerialize::deserialize(asset, llsdstream, buffer.size()))
	{
		llwarns << "Failed to deserialize material data." << llendl;
		return false;
	}

	if (!asset.has("version") ||
		!LLGLTFMaterial::isAcceptedVersion(asset["version"].asString()))
	{
		llwarns << "Invalid or missing material data version." << llendl;
		return false;
	}

	static const std::string asset_type(LLGLTFMaterial::ASSET_TYPE);
	if (!asset.has("type") || asset["type"].asString() != asset_type)
	{
		llwarns << "Not a " << asset_type << " asset." << llendl;
		return false;
	}

	if (!asset.has("data") || !asset["data"].isString())
	{
		llwarns << "Material asset has no data." << llendl;
		return false;
	}

	std::string data = asset["data"];
	std::string error_msg, warn_msg;
	tinygltf::TinyGLTF gltf;
	tinygltf::TinyGLTF loader;
	tinygltf::Model model_in;
	if (!loader.LoadASCIIFromString(&model_in, &error_msg, &warn_msg,
									data.c_str(), data.size(), ""))
	{
		llwarns << "Failed to decode GLTF material data: "
				<< (error_msg.empty() ? warn_msg : error_msg) << llendl;
		return false;
	}

	// Assets are only supposed to have one item. This duplicates some
	// functionality from LLGLTFMaterial::fromJSON, but currently does the job
	// better for the material preview use case. However LLGLTFMaterial::asJSON
	// should always be used when uploading materials, to ensure the asset is
	// valid.
	return setFromGltfModel(model_in, 0, true);
}

std::string LLPreviewMaterial::buildMaterialDescription()
{
	static const char* separator = ", ";
	bool needs_separator = false;

	std::string desc = getString("mat_desc");
	if (mBaseColorTexCtrl->getValue().asUUID().notNull() &&
		!mBaseColorName.empty())
	{
		desc += mBaseColorName;
		needs_separator = true;
	}
	if (mMetallicTexCtrl->getValue().asUUID().notNull() &&
		!mMetallicRoughnessName.empty())
	{
		if (needs_separator)
		{
			desc.append(separator);
		}
		desc += mMetallicRoughnessName;
		needs_separator = true;
	}
	if (mEmissiveTexCtrl->getValue().asUUID().notNull() &&
		!mEmissiveName.empty())
	{
		if (needs_separator)
		{
			desc.append(separator);
		}
		desc += mEmissiveName;
		needs_separator = true;
	}
	if (mNormalTexCtrl->getValue().asUUID().notNull() && !mNormalName.empty())
	{
		if (needs_separator)
		{
			desc.append(separator);
		}
		desc += mNormalName;
	}
	LLInventoryObject::correctInventoryName(desc);

	return desc;
}

void LLPreviewMaterial::saveIfNeeded()
{
	if (mUploadingTexturesCount > 0)
	{
		// An upload is already in progress; wait until textures upload will
		// retry saving on callback. Also should prevent some failure
		// callbacks.
		return;
	}

	if (saveTextures())
	{
		// Started texture upload
		setEnabled(false);
		return;
	}

	std::string buffer = getEncodedAsset();

	const LLInventoryItem* itemp = getItem();
	if (!itemp)
	{
		// Create an new inventory item
		LLPermissions perms;
		perms.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
		createInventoryItem(buffer, mMaterialName, buildMaterialDescription(),
							perms);
		// We do not update floater with uploaded asset yet, so just close it.
		close();
		return;
	}

	if (!updateInventoryItem(buffer, mItemUUID, mObjectUUID))
	{
		return;
	}
	if (mCloseAfterSave)
	{
		close();
		return;
	}
	mAssetStatus = PREVIEW_ASSET_LOADING;
	setEnabled(false);
}

//static
void LLPreviewMaterial::uploadFailure(void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (self && sEditorInstances.count(self))	// Floater still there ?
	{
		// Stop upload if possible, unblock and let user decide
		self->setFailedToUploadTexture();
	}
}

//static
void LLPreviewMaterial::finishInventoryUpload(const LLUUID& item_id,
											  const LLUUID& new_asset_id,
											  const LLUUID& new_item_id,
											  void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (!self || !sEditorInstances.count(self))
	{
		return;	// Floater already gone.
	}

	if (new_asset_id.notNull())
	{
		self->setAssetId(new_asset_id);
	}
	self->refreshFromInventory(new_item_id.notNull() ? new_item_id : item_id);

	if (self->mTextureChangesUpdates.empty())
	{
		return;
	}
	const LLViewerInventoryItem* itemp = self->getItem();
	if (!itemp)
	{
		return;
	}
	LLGLTFMaterial* matp =
		gGLTFMaterialList.getMaterial(itemp->getAssetUUID());
	if (!matp)
	{
		return;
	}
	// Local textures were assigned, force load material and init tracking
	for (connection_map_t::iterator it = self->mTextureChangesUpdates.begin(),
									end = self->mTextureChangesUpdates.end();
		 it != end; ++it)
	{
		LLLocalBitmap::associateGLTFMaterial(it->second.mTrackingId, matp);
	}
}

//static
void LLPreviewMaterial::finishTaskUpload(const LLUUID& item_id,
										 const LLUUID& new_asset_id,
										 const LLUUID& task_id, void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (self && sEditorInstances.count(self))	// Floater still there ?
	{
#if 1	// Refreshing from an object inventory does not properly work because
		// it takes an indeterminate amount of time for the object inventory to
		// get refreshed, so just close the floater... HB
		self->close();
#else
		self->setAssetId(new_asset_id);
		self->refreshFromInventory();
		self->setEnabled(true);

		if (self->mTextureChangesUpdates.empty())
		{
			return;
		}

		// Local textures were assigned, force load material and init tracking
		LLGLTFMaterial* matp = gGLTFMaterialList.getMaterial(new_asset_id);
		if (!matp)
		{
			return;
		}
		for (connection_map_t::iterator it = mTextureChangesUpdates.begin(),
										end = mTextureChangesUpdates.end();
			 it != end; ++it)
		{
			LLLocalBitmap::associateGLTFMaterial(it->second.mTrackingId, matp);
		}
#endif
	}
}

bool LLPreviewMaterial::updateInventoryItem(const std::string& buffer,
											const LLUUID& item_id,
											const LLUUID& task_id)
{
	if (!gAgent.hasInventoryMaterial())
	{
		llwarns << "Not connected to a GLTF material capable region, cannot save material."
				<< llendl;
		return false;
	}

	const std::string* urlp = NULL;
	LLResourceUploadInfo::ptr_t infop;
	if (task_id.notNull())
	{
		const std::string& task_url =
			gAgent.getRegionCapability("UpdateMaterialTaskInventory");
		if (!task_url.empty())
		{
			// Saving into task inventory
			urlp = &task_url;
			infop = std::make_shared<LLBufferedAssetUploadInfo>(
						task_id, item_id, LLAssetType::AT_MATERIAL, buffer,
						boost::bind(finishTaskUpload, _1, _2, _3, (void*)this),
						boost::bind(uploadFailure, (void*)this));
		}
	}
	else
	{
		const std::string& inv_url =
			gAgent.getRegionCapability("UpdateMaterialAgentInventory");
		if (!inv_url.empty())
		{
			// Saving into agent inventory
			urlp = &inv_url;
			infop = std::make_shared<LLBufferedAssetUploadInfo>(
						item_id, LLAssetType::AT_MATERIAL, buffer,
						boost::bind(finishInventoryUpload, _1, _2, _3,
									(void*)this),
						boost::bind(uploadFailure, (void*)this));
		}
	}
	if (urlp && infop)
	{
		LLViewerAssetUpload::enqueueInventoryUpload(*urlp, infop);
		return true;
	}

	return false;
}

//static
void LLPreviewMaterial::createInventoryItem(const std::string& buffer,
											const std::string& name,
											const std::string& desc,
											const LLPermissions& permissions)
{
	if (!gAgent.hasInventoryMaterial())
	{
		gNotifications.add("MissingMaterialCaps");
		return;
	}

	LLTransactionID tid;
	tid.generate();
	LLUUID parent_id =
		gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_MATERIAL);
	LLPointer<LLInventoryCallback> cb = new LLMaterialInventoryCB(permissions,
																  buffer,
																  name);
	create_inventory_item(parent_id, tid, name, desc, LLAssetType::AT_MATERIAL,
						  LLInventoryType::IT_MATERIAL, NO_INV_SUBTYPE,
						  permissions.getMaskNextOwner(), cb);
}

//static
void LLPreviewMaterial::uploadSuccess(const LLUUID& asset_id,
									  const LLSD& response,
									  U32 tex_type, void* userdata)
{
	LLPreviewMaterial* self = (LLPreviewMaterial*)userdata;
	if (!self || !sEditorInstances.count(self))
	{
		return;	// Floater already gone !
	}

	if (!response["success"].asBoolean())
	{
		// Stop upload if possible, unblock and let user decide
		self->setFailedToUploadTexture();
		return;
	}

	switch (tex_type)
	{
		case MAT_BASE_COLOR_TEX_DIRTY:
			self->setBaseColorId(asset_id);
			self->mBaseColorJ2C = NULL;
			self->mBaseColorFetched = NULL;
			self->mBaseColorTextureUploadId.setNull();
			break;

		case MAT_NORMAL_TEX_DIRTY:
			self->setNormalId(asset_id);
			self->mNormalJ2C = NULL;
			self->mNormalFetched = NULL;
			self->mNormalTextureUploadId.setNull();
			break;

		case MAT_ROUGHTNESS_TEX_DIRTY:
			self->setMetallicRoughnessId(asset_id);
			self->mMetallicRoughnessJ2C = NULL;
			self->mMetallicRoughnessFetched = NULL;
			self->mMetallicTextureUploadId.setNull();
			break;

		case MAT_EMISIVE_TEX_DIRTY:
			self->setEmissiveId(asset_id);
			self->mEmissiveJ2C = NULL;
			self->mEmissiveFetched = NULL;
			self->mEmissiveTextureUploadId.setNull();

		default:
			break;
	}

	--self->mUploadingTexturesCount;
	if (!self->mUploadingTexturesFailure)
	{
		// Try saving
		self->saveIfNeeded();
	}
	else if (!self->mUploadingTexturesCount)
	{
		self->setEnabled(true);
	}
}

bool LLPreviewMaterial::saveTexture(LLImageJ2C* imagep, U32 tex_type,
									const std::string& name,
									const LLUUID& asset_id)
{
	if (asset_id.isNull() || !imagep || !imagep->getDataSize())
	{
		return false;
	}

	++mUploadingTexturesCount;

	// Copy image bytes into a string buffer
	std::string buffer;
	buffer.assign((const char*)imagep->getData(), imagep->getDataSize());

	LLResourceUploadInfo::ptr_t info =
		std::make_shared<LLNewBufferedResourceUploadInfo>(
			buffer, asset_id, name, name, 0, LLFolderType::FT_TEXTURE,
			LLInventoryType::IT_TEXTURE, LLAssetType::AT_TEXTURE,
			LLFloaterPerms::getNextOwnerPerms(),
			LLFloaterPerms::getGroupPerms(),
			LLFloaterPerms::getEveryonePerms(),
			LLEconomy::getInstance()->getTextureUploadCost(),
			boost::bind(uploadSuccess, _1, _2, tex_type, (void*)this),
			boost::bind(uploadFailure, (void*)this));
	upload_new_resource(info);

	return true;
}

void LLPreviewMaterial::setFailedToUploadTexture()
{
	mUploadingTexturesFailure = true;
	if (--mUploadingTexturesCount == 0)
	{
		setEnabled(true);
	}
}

U32 LLPreviewMaterial::saveTextures()
{
	mUploadingTexturesFailure = false;

	U32 work_count = 0;

	if (!mUploadingTexturesCount && // Only 1 texture uploaded at a time ! HB
		mBaseColorTextureUploadId.notNull() &&
		mBaseColorTextureUploadId == getBaseColorId())
	{
		if (saveTexture(mBaseColorJ2C, MAT_BASE_COLOR_TEX_DIRTY,
						mBaseColorName, mBaseColorTextureUploadId))
		{
			++work_count;
		}
	}

	if (!mUploadingTexturesCount && // Only 1 texture uploaded at a time ! HB
		mNormalTextureUploadId.notNull() &&
		mNormalTextureUploadId == getNormalId())
	{
		if (saveTexture(mNormalJ2C, MAT_NORMAL_TEX_DIRTY, mNormalName,
						mNormalTextureUploadId))
		{
			++work_count;
		}
	}

	if (!mUploadingTexturesCount && // Only 1 texture uploaded at a time ! HB
		mMetallicTextureUploadId.notNull() &&
		mMetallicTextureUploadId == getMetallicRoughnessId())
	{
		if (saveTexture(mMetallicRoughnessJ2C, MAT_ROUGHTNESS_TEX_DIRTY,
						mMetallicRoughnessName, mMetallicTextureUploadId))
		{
			++work_count;
		}
	}

	if (!mUploadingTexturesCount && // Only 1 texture uploaded at a time ! HB
		mEmissiveTextureUploadId.notNull() &&
		mEmissiveTextureUploadId == getEmissiveId())
	{
		if (saveTexture(mEmissiveJ2C, MAT_EMISIVE_TEX_DIRTY,
						mEmissiveName, mEmissiveTextureUploadId))
		{
			++work_count;
		}
	}

	if (!mUploadingTexturesCount && !work_count)
	{
		// Discard upload buffers once textures have been confirmed as saved.
		// Otherwise we keep buffers for potential upload failure recovery.
		clearTextures();
	}

	// Asset storage can callback immediately, causing a decrease of
	// mUploadingTexturesCount, so report the amount of work scheduled, not the
	// amount of work remaining.
	return work_count;
}
