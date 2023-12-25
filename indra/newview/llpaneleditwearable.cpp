/**
 * @file llpaneleditwearable.cpp
 * @brief  A LLPanel dedicated to the editing of wearables.
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

#include "llpaneleditwearable.h"

#include "imageids.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lliconctrl.h"
#include "llnotifications.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llvisualparam.h"
#include "llxmltree.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llcolorswatch.h"
#include "llfloatercustomize.h"
#include "llinventoryicon.h"
#include "llmorphview.h"
#include "lltexturectrl.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llvisualparamhint.h"
#include "llwearablelist.h"

using namespace LLAvatarAppearanceDefines;

LLPanelEditWearable::LLPanelEditWearable(LLWearableType::EType type)
:	LLPanel(LLWearableType::getTypeLabel(type)),
	mType(type),
	mLayer(0),					// Use the first layer by default
	mSpinLayer(NULL),
	mButtonImport(NULL),
	mButtonCreateNew(NULL),
	mButtonSave(NULL),
	mButtonSaveAs(NULL),
	mButtonRevert(NULL),
	mButtonTakeOff(NULL),
	mSexRadio(NULL),
	mLockIcon(NULL),
	mWearableIcon(NULL),
	mNotWornInstructions(NULL),
	mNoModifyInstructions(NULL),
	mTitle(NULL),
	mTitleNoModify(NULL),
	mTitleNotWorn(NULL),
	mTitleLoading(NULL),
	mPath(NULL)
{
	mWearable = gAgentWearables.getViewerWearable(type, mLayer);
}

bool LLPanelEditWearable::postBuild()
{
	mSpinLayer = getChild<LLSpinCtrl>("layer", true, false);
	if (mSpinLayer)
	{
		if ((gSavedSettings.getBool("NoMultiplePhysics") &&
			 mType == LLWearableType::WT_PHYSICS) ||
			(gSavedSettings.getBool("NoMultipleShoes") &&
			 mType == LLWearableType::WT_SHOES) ||
			(gSavedSettings.getBool("NoMultipleSkirts") &&
			 mType == LLWearableType::WT_SKIRT))
		{
			mSpinLayer->setVisible(false);
			mSpinLayer = NULL;
		}
		else
		{
			setMaxLayers();
			mSpinLayer->set((F32)mLayer);
			mSpinLayer->setCommitCallback(onCommitLayer);
			mSpinLayer->setCallbackUserData(this);
		}
	}

	mLockIcon = getChild<LLIconCtrl>("lock", true, false);
	mWearableIcon = getChild<LLIconCtrl>("icon", true, false);
	if (mWearableIcon)
	{
		LLAssetType::EType asset_type = LLWearableType::getAssetType(mType);
		std::string icon_name =
			LLInventoryIcon::getIconName(asset_type,
										 LLInventoryType::IT_WEARABLE,
										 mType, false);
		mWearableIcon->setValue(icon_name);
	}

	mNotWornInstructions = getChild<LLTextBox>("not worn instructions",
											   true, false);
	mNoModifyInstructions = getChild<LLTextBox>("no modify instructions",
												true, false);
	mTitle = getChild<LLTextBox>("title", true, false);
	mTitleNoModify = getChild<LLTextBox>("title_no_modify", true, false);
	mTitleNotWorn = getChild<LLTextBox>("title_not_worn", true, false);
	mTitleLoading = getChild<LLTextBox>("title_loading", true, false);
	mPath = getChild<LLTextBox>("path", true, false);

	mButtonImport = getChild<LLButton>("import", true, false);
	if (mButtonImport)
	{
		mButtonImport->setClickedCallback(onBtnImport, this);
	}

	mButtonCreateNew = getChild<LLButton>("Create New", true, false);
	if (mButtonCreateNew)
	{
		mButtonCreateNew->setClickedCallback(onBtnCreateNew, this);
	}

	// If PG, cannot take off underclothing or shirt
	mCanTakeOff = LLWearableType::getAssetType(mType) ==
					LLAssetType::AT_CLOTHING;
#if LL_TEEN_WERABLE_RESTRICTIONS
	
	mCanTakeOff &= !(gAgent.isTeen() &&
					 (mType == LLWearableType::WT_UNDERSHIRT ||
					  mType == LLWearableType::WT_UNDERPANTS));
#endif

	mButtonTakeOff = getChild<LLButton>("Take Off", true, false);
	if (mButtonTakeOff)
	{
		mButtonTakeOff->setVisible(mCanTakeOff);
		mButtonTakeOff->setClickedCallback(onBtnTakeOff, this);
	}

	mButtonSave = getChild<LLButton>("Save", true, false);
	if (mButtonSave)
	{
		mButtonSave->setClickedCallback(onBtnSave, this);
	}

	mButtonSaveAs = getChild<LLButton>("Save As", true, false);
	if (mButtonSaveAs)
	{
		mButtonSaveAs->setClickedCallback(onBtnSaveAs, this);
	}

	mButtonRevert = getChild<LLButton>("Revert", true, false);
	if (mButtonRevert)
	{
		mButtonRevert->setClickedCallback(onBtnRevert, this);
	}

	mSexRadio = getChild<LLUICtrl>("sex radio", true, false);
	if (mSexRadio)
	{
		mSexRadio->setCommitCallback(onCommitSexChange);
		mSexRadio->setCallbackUserData(this);
	}

	return true;
}

LLPanelEditWearable::~LLPanelEditWearable()
{
	std::for_each(mSubpartList.begin(), mSubpartList.end(),
				  DeletePairedPointer());
	mSubpartList.clear();

	// Clear colorswatch commit callbacks that point to this object.
	for (std::map<std::string, S32>::iterator iter = mColorList.begin();
		 iter != mColorList.end(); ++iter)
	{
		childSetCommitCallback(iter->first.c_str(), NULL, NULL);
	}
}

void LLPanelEditWearable::addSubpart(const std::string& name, ESubpart id,
									 LLSubpart* part)
{
	if (!name.empty())
	{
		childSetAction(name.c_str(), &LLPanelEditWearable::onBtnSubpart,
					   (void*)id);
		part->mButtonName = name;
	}
	mSubpartList[id] = part;
}

//static
void LLPanelEditWearable::onBtnSubpart(void* userdata)
{
	if (!gFloaterCustomizep) return;
	LLPanelEditWearable* self = gFloaterCustomizep->getCurrentWearablePanel();
	if (!self) return;
	ESubpart subpart = (ESubpart) (intptr_t)userdata;
	self->setSubpart(subpart);
}

void LLPanelEditWearable::setSubpart(ESubpart subpart)
{
	mCurrentSubpart = subpart;

	for (std::map<ESubpart, LLSubpart*>::iterator iter = mSubpartList.begin();
		 iter != mSubpartList.end(); ++iter)
	{
		LLButton* btn = getChild<LLButton>(iter->second->mButtonName.c_str(),
										   true, false);
		if (btn)
		{
			btn->setToggleState(subpart == iter->first);
		}
	}

	LLSubpart* part = get_ptr_in_map(mSubpartList, (ESubpart)subpart);
	if (part && isAgentAvatarValid())
	{
		// Update the thumbnails we display
		LLFloaterCustomize::param_map sorted_params;
		ESex avatar_sex = gAgentAvatarp->getSex();

		LLViewerInventoryItem* item =
			gAgentWearables.getWearableInventoryItem(mType, mLayer);
		U32 perm_mask = 0x0;
		bool is_complete = false;
		if (item)
		{
			perm_mask = item->getPermissions().getMaskOwner();
			is_complete = item->isFinished();
		}
		setUIPermissions(perm_mask, is_complete);
		bool editable = (perm_mask & PERM_MODIFY) && is_complete;
		std::string param_name;

		for (LLViewerVisualParam* param =
				(LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam();
			 param;
			 param = (LLViewerVisualParam*)gAgentAvatarp->getNextVisualParam())
		{
			if (!param || param->getID() == -1 || !param->isTweakable() ||
				param->getEditGroup() != part->mEditGroup ||
				!(param->getSex() & avatar_sex))
			{
				continue;
			}

			// Exclude wrinkles since the baking code was removed for them...
			// We still allow them for the skin (face wrinkles) in OpenSim
			// since they can still render in non-SSB grids (the Cool VL Viewer
			// does allow to bake them).
			param_name = param->getName();
			LLStringUtil::toLower(param_name);
			if (param_name.find("wrinkles") != std::string::npos &&
				(!LLTexLayerSet::sAllowFaceWrinkles || param->getID() != 163))
			{
				continue;
			}

			// Check for duplicates
			llassert(sorted_params.find(-param->getDisplayOrder()) ==
						sorted_params.end());

			// Negative getDisplayOrder() to make lowest order the highest
			// priority
			sorted_params.emplace(-param->getDisplayOrder(),
								  LLFloaterCustomize::editable_param(editable,
																	 param));
		}
		LLJoint* jointp = gAgentAvatarp->getJoint(part->mTargetJointKey);
		gFloaterCustomizep->generateVisualParamHints(this, NULL, sorted_params,
													 mWearable,
													 part->mVisualHint,
													 jointp);
		gFloaterCustomizep->updateScrollingPanelUI();

		// Update the camera
		gMorphViewp->setCameraTargetJoint(jointp);
		gMorphViewp->setCameraTargetOffset(part->mTargetOffset);
		gMorphViewp->setCameraOffset(part->mCameraOffset);
		if (gSavedSettings.getBool("AppearanceCameraMovement"))
		{
			gAgent.setFocusOnAvatar(false, gAgent.getCameraAnimating());
			gMorphViewp->updateCamera();
		}
	}
}

//static
void LLPanelEditWearable::onBtnTakeOff(void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (self && gAgentWearables.getViewerWearable(self->mType, self->mLayer))
	{
		gAgentWearables.removeWearable(self->mType, false, self->mLayer);
	}
}

//static
void LLPanelEditWearable::onBtnSave(void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (!self) return;

	gAgentWearables.saveWearable(self->mType, self->mLayer);
}

//static
void LLPanelEditWearable::onBtnSaveAs(void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (!self) return;

	LLViewerWearable* wearable;
	wearable = gAgentWearables.getViewerWearable(self->mType, self->mLayer);
	if (wearable)
	{
		LLWearableSaveAsDialog* save_as_dialog;
		save_as_dialog = new LLWearableSaveAsDialog(wearable->getName(),
													onSaveAsCommit, self);
		save_as_dialog->startModal();
		// LLWearableSaveAsDialog deletes itself.
	}
}

//static
void LLPanelEditWearable::onSaveAsCommit(LLWearableSaveAsDialog* save_as_dialog,
										 void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (self && isAgentAvatarValid())
	{
		gAgentWearables.saveWearableAs(self->mType, self->mLayer,
									   save_as_dialog->getItemName());
	}
}

//static
void LLPanelEditWearable::onBtnRevert(void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (!self) return;

	gAgentWearables.revertWearable(self->mType, self->mLayer);
}

//static
void LLPanelEditWearable::onBtnCreateNew(void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (self && isAgentAvatarValid())
	{
		// Create a new wearable in the default folder for the wearable's asset
		// type.
		LLViewerWearable* wearable =
			LLWearableList::getInstance()->createNewWearable(self->mType,
															 gAgentAvatarp);
		LLAssetType::EType asset_type = wearable->getAssetType();

		// Regular UI, items get created in normal folder
		LLUUID folder_id =
			gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(asset_type));

		LLPointer<LLInventoryCallback> cb = new LLWearOnAvatarCallback(false);
		create_inventory_item(folder_id, wearable->getTransactionID(),
							  wearable->getName(), wearable->getDescription(),
							  asset_type, LLInventoryType::IT_WEARABLE,
							  (U8)wearable->getType(),
							  wearable->getPermissions().getMaskNextOwner(),
							  cb);
	}
}

bool LLPanelEditWearable::textureIsInvisible(ETextureIndex te)
{
	if (isAgentAvatarValid() &&
		gAgentWearables.getViewerWearable(mType, getWearableIndex()))
	{
		const LLTextureEntry* current_te = gAgentAvatarp->getTE(te);
		return (current_te && current_te->getID() == IMG_INVISIBLE);
	}
	return false;
}

void LLPanelEditWearable::addInvisibilityCheckbox(ETextureIndex te,
												  const std::string& name)
{
	childSetCommitCallback(name.c_str(), onInvisibilityCommit, this);

	mInvisibilityList[name] = te;
}

//static
void LLPanelEditWearable::onInvisibilityCommit(LLUICtrl* ctrl, void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	LLCheckBoxCtrl* checkbox_ctrl = (LLCheckBoxCtrl*)ctrl;
	if (!self || !checkbox_ctrl || !self->mWearable || !isAgentAvatarValid())
	{
		return;
	}

	ETextureIndex te =
		(ETextureIndex)(self->mInvisibilityList[ctrl->getName()]);

	bool new_invis_state = checkbox_ctrl->get();
	if (new_invis_state)
	{
		LLLocalTextureObject* lto = self->mWearable->getLocalTextureObject(te);
		self->mPreviousTextureList[te] = lto->getID();

		LLViewerTexture* image =
			LLViewerTextureManager::getFetchedTexture(IMG_INVISIBLE);
		gAgentAvatarp->setLocalTexture(te, image, false, self->mLayer);
		gAgentAvatarp->wearableUpdated(self->mType, false);
	}
	else
	{
		// Try to restore previous texture, if any.
		LLUUID prev_id = self->mPreviousTextureList[(S32)te];
		if (prev_id.isNull() || prev_id == IMG_INVISIBLE)
		{
			prev_id = LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID"));
		}
		if (prev_id.notNull())
		{
			LLViewerTexture* image =
				LLViewerTextureManager::getFetchedTexture(prev_id);
			if (image)
			{
				gAgentAvatarp->setLocalTexture(te, image, false, self->mLayer);
				gAgentAvatarp->wearableUpdated(self->mType, false);
			}
		}
	}
}

void LLPanelEditWearable::addColorSwatch(ETextureIndex te,
										 const std::string& name)
{
	childSetCommitCallback(name.c_str(), LLPanelEditWearable::onColorCommit, this);
	mColorList[name] = te;
}

//static
void LLPanelEditWearable::onColorCommit(LLUICtrl* ctrl, void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	LLColorSwatchCtrl* color_ctrl = (LLColorSwatchCtrl*)ctrl;

	if (self && color_ctrl && isAgentAvatarValid() && self->mWearable)
	{
		std::map<std::string, S32>::const_iterator cl_itr =
			self->mColorList.find(ctrl->getName());
		if (cl_itr != self->mColorList.end())
		{
			ETextureIndex te = (ETextureIndex)cl_itr->second;

			LLColor4 old_color = self->mWearable->getClothesColor(te);
			const LLColor4& new_color = color_ctrl->get();
			if (old_color != new_color)
			{
				// Set the new version
				self->mWearable->setClothesColor(te, new_color, true);
#if 0
				gAgentAvatarp->setClothesColor(te, new_color, true);
#endif
				LLVisualParamHint::requestHintUpdates();
 				gAgentAvatarp->wearableUpdated(self->mType, false);
			}
		}
	}
}

void LLPanelEditWearable::initPreviousTextureList()
{
	initPreviousTextureListEntry(TEX_LOWER_ALPHA);
	initPreviousTextureListEntry(TEX_UPPER_ALPHA);
	initPreviousTextureListEntry(TEX_HEAD_ALPHA);
	initPreviousTextureListEntry(TEX_EYES_ALPHA);
	initPreviousTextureListEntry(TEX_LOWER_ALPHA);
}

void LLPanelEditWearable::initPreviousTextureListEntry(ETextureIndex te)
{
	if (mWearable)
	{
		LLUUID id;
		LLLocalTextureObject* lto = mWearable->getLocalTextureObject(te);
		if (lto)
		{
			id = lto->getID();
		}
		mPreviousTextureList[te] = id;
	}
}

void LLPanelEditWearable::addTextureDropTarget(ETextureIndex te,
											   const std::string& name,
											   const LLUUID& default_image_id,
											   bool allow_no_texture)
{
	childSetCommitCallback(name.c_str(), onTextureCommit, this);
	LLTextureCtrl* tex_ctrl = getChild<LLTextureCtrl>(name.c_str(), true,
													  false);
	if (tex_ctrl)
	{
		tex_ctrl->setDefaultImageAssetID(default_image_id);
		tex_ctrl->setAllowNoTexture(allow_no_texture);
		// Do not allow (no copy) or (no transfer) textures to be selected.
		tex_ctrl->setImmediateFilterPermMask(PERM_NONE);
		tex_ctrl->setNonImmediateFilterPermMask(PERM_NONE);
	}
	mTextureList[name] = te;
	if (mType == LLWearableType::WT_ALPHA)
	{
		initPreviousTextureListEntry(te);
	}
}

//static
void LLPanelEditWearable::onTextureCommit(LLUICtrl* ctrl, void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	LLTextureCtrl* tex_ctrl = (LLTextureCtrl*)ctrl;

	if (self && ctrl && isAgentAvatarValid())
	{
		ETextureIndex te =
			(ETextureIndex)(self->mTextureList[ctrl->getName()]);

		// Set the new version
		LLViewerTexture* image =
			LLViewerTextureManager::getFetchedTexture(tex_ctrl->getImageAssetID());
		if (image->getID().isNull() || image->getID() == IMG_DEFAULT)
		{
			image =
				LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
		}
		self->mTextureList[ctrl->getName()] = te;
		if (self->mWearable)
		{
			gAgentAvatarp->setLocalTexture(te, image, false, self->mLayer);
			LLVisualParamHint::requestHintUpdates();
			gAgentAvatarp->wearableUpdated(self->mType, false);
		}
		if (self->mType == LLWearableType::WT_ALPHA &&
			image->getID() != IMG_INVISIBLE)
		{
			self->mPreviousTextureList[te] = image->getID();
		}
	}
}

ESubpart LLPanelEditWearable::getDefaultSubpart()
{
	switch (mType)
	{
		case LLWearableType::WT_SHAPE:
			return SUBPART_SHAPE_WHOLE;

		case LLWearableType::WT_SKIN:
			return SUBPART_SKIN_COLOR;

		case LLWearableType::WT_HAIR:
			return SUBPART_HAIR_COLOR;

		case LLWearableType::WT_EYES:
			return SUBPART_EYES;

		case LLWearableType::WT_SHIRT:
			return SUBPART_SHIRT;

		case LLWearableType::WT_PANTS:
			return SUBPART_PANTS;

		case LLWearableType::WT_SHOES:
			return SUBPART_SHOES;

		case LLWearableType::WT_SOCKS:
			return SUBPART_SOCKS;

		case LLWearableType::WT_JACKET:
			return SUBPART_JACKET;

		case LLWearableType::WT_GLOVES:
			return SUBPART_GLOVES;

		case LLWearableType::WT_UNDERSHIRT:
			return SUBPART_UNDERSHIRT;

		case LLWearableType::WT_UNDERPANTS:
			return SUBPART_UNDERPANTS;

		case LLWearableType::WT_SKIRT:
			return SUBPART_SKIRT;

		case LLWearableType::WT_ALPHA:
			return SUBPART_ALPHA;

		case LLWearableType::WT_TATTOO:
			return SUBPART_TATTOO;

		case LLWearableType::WT_UNIVERSAL:
			return SUBPART_UNIVERSAL;

		case LLWearableType::WT_PHYSICS:
			return SUBPART_PHYSICS_BELLY_UPDOWN;

		default:
			llwarns << "Unknown sub-part type: " << mType << llendl;
			llassert(false);
			return SUBPART_SHAPE_WHOLE;
	}
}

void LLPanelEditWearable::draw()
{
	if (!gFloaterCustomizep || gFloaterCustomizep->isMinimized() ||
		!isAgentAvatarValid())
	{
		return;
	}

	bool has_wearable = mWearable != NULL;
	bool is_dirty = isDirty();
	bool is_modifiable = false;
	bool is_copyable = false;
	bool is_complete = false;
	LLViewerInventoryItem* item = NULL;
	if (has_wearable)
	{
		item = gAgentWearables.getWearableInventoryItem(mType, mLayer);
		if (item)
		{
			const LLPermissions& perm = item->getPermissions();
			is_modifiable = perm.allowModifyBy(gAgentID, gAgent.getGroupID());
			is_copyable = perm.allowCopyBy(gAgentID, gAgent.getGroupID());
			is_complete = item->isFinished();
		}
	}

	setMaxLayers();

	if (mButtonSave)
	{
		mButtonSave->setEnabled(is_modifiable && is_complete && has_wearable &&
								is_dirty);
		mButtonSave->setVisible(has_wearable || !mButtonCreateNew);
	}
	if (mButtonSaveAs)
	{
		mButtonSaveAs->setEnabled(is_copyable && is_complete && has_wearable);
		mButtonSaveAs->setVisible(has_wearable || !mButtonCreateNew);
	}
	if (mButtonRevert)
	{
		mButtonRevert->setEnabled(has_wearable && is_dirty);
		mButtonRevert->setVisible(has_wearable || !mButtonCreateNew);
	}
	if (mButtonTakeOff)
	{
		mButtonTakeOff->setEnabled(has_wearable);
		mButtonTakeOff->setVisible(mCanTakeOff && has_wearable);
	}
	if (mButtonCreateNew)
	{
		mButtonCreateNew->setVisible(!has_wearable);
	}

	if (mNotWornInstructions)
	{
		mNotWornInstructions->setVisible(!has_wearable);
	}
	if (mNoModifyInstructions)
	{
		mNoModifyInstructions->setVisible(has_wearable && !is_modifiable);
	}

	for (std::map<ESubpart, LLSubpart*>::iterator iter = mSubpartList.begin(),
												  end = mSubpartList.end();
		 iter != end; ++iter)
	{
		std::string btn_name = iter->second->mButtonName;
		LLButton* button = getChild<LLButton>(btn_name.c_str(), true, false);
		if (button)
		{
			button->setVisible(has_wearable);
			if (has_wearable && is_complete && is_modifiable)
			{
				button->setEnabled(iter->second->mSex &
								   gAgentAvatarp->getSex());
			}
			else
			{
				button->setEnabled(false);
			}
		}
	}

	if (mLockIcon)
	{
		mLockIcon->setVisible(!is_modifiable);
	}
	if (mTitle)
	{
		mTitle->setVisible(false);
	}
	if (mTitleNoModify)
	{
		mTitleNoModify->setVisible(false);
	}
	if (mTitleNotWorn)
	{
		mTitleNotWorn->setVisible(false);
	}
	if (mTitleLoading)
	{
		mTitleLoading->setVisible(false);
	}
	if (mPath)
	{
		mPath->setVisible(false);
	}

	if (has_wearable && !is_modifiable)
	{
		if (mTitleNoModify)
		{
			mTitleNoModify->setVisible(true);
			// *TODO:Translate
			mTitleNoModify->setTextArg("[DESC]", item ? item->getName()
													  : mWearable->getName());
		}

		hideTextureControls();
	}
	else if (has_wearable && !is_complete)
	{
		if (mTitleLoading)
		{
			mTitleLoading->setVisible(true);
			// *TODO:Translate
			mTitleLoading->setTextArg("[DESC]",
									  LLWearableType::getTypeLabel(mType));
		}

		if (mPath)
		{
			std::string path;
			const LLUUID& item_id = gAgentWearables.getWearableItemID(mType,
																	  mLayer);
			gInventory.appendPath(item_id, path);
			mPath->setVisible(true);
			mPath->setTextArg("[PATH]", path);
		}

		hideTextureControls();
	}
	else if (has_wearable && is_modifiable)
	{
		if (mTitle)
		{
			mTitle->setVisible(true);
			mTitle->setTextArg("[DESC]", item ? item->getName()
											  : mWearable->getName());
		}

		if (mPath)
		{
			std::string path;
			const LLUUID& item_id = gAgentWearables.getWearableItemID(mType,
																	  mLayer);
			gInventory.appendPath(item_id, path);
			mPath->setVisible(true);
			mPath->setTextArg("[PATH]", path);
		}

		for (std::map<std::string, S32>::iterator iter = mTextureList.begin(),
												  end = mTextureList.end();
			 iter != end; ++iter)
		{
			std::string name = iter->first;
			LLTextureCtrl* tex_ctrl = getChild<LLTextureCtrl>(name.c_str(),
															  true, false);
			if (tex_ctrl)
			{
				tex_ctrl->setVisible(is_copyable && is_modifiable &&
									 is_complete);

				ETextureIndex te = (ETextureIndex)iter->second;
				LLLocalTextureObject* lto =
					mWearable->getLocalTextureObject(te);

				LLUUID new_id;
				if (lto && lto->getID() != IMG_DEFAULT_AVATAR)
				{
					new_id = lto->getID();
				}

				if (tex_ctrl->getImageAssetID() != new_id)
				{
					// Texture has changed, close the floater to avoid
					// DEV-22461
					tex_ctrl->closeFloater();
				}

				tex_ctrl->setImageAssetID(new_id);
			}
		}

		for (std::map<std::string, S32>::iterator iter = mColorList.begin(),
												  end = mColorList.end();
			 iter != end; ++iter)
		{
			std::string name = iter->first;
			LLColorSwatchCtrl* ctrl = getChild<LLColorSwatchCtrl>(name.c_str(),
																  true, false);
			if (ctrl)
			{
				ctrl->setVisible(is_modifiable && is_complete);
				ctrl->setEnabled(is_modifiable && is_complete);
				ETextureIndex te = (ETextureIndex)iter->second;
				ctrl->set(mWearable->getClothesColor(te));
			}
		}

		for (std::map<std::string, S32>::iterator
				iter = mInvisibilityList.begin(),
				end = mInvisibilityList.end();
			 iter != end; ++iter)
		{
			std::string name = iter->first;
			LLCheckBoxCtrl* ctrl = getChild<LLCheckBoxCtrl>(name.c_str(), true,
															false);
			if (ctrl)
			{
				ctrl->setVisible(is_copyable && is_modifiable && is_complete);
				ctrl->setEnabled(is_copyable && is_modifiable && is_complete);
				ETextureIndex te = (ETextureIndex)iter->second;
				ctrl->set(!gAgentAvatarp->isTextureVisible(te, mWearable));
			}
		}
	}
	else
	{
		if (mTitleNotWorn)
		{
			mTitleNotWorn->setVisible(true);
			// *TODO:Translate
			mTitleNotWorn->setTextArg("[DESC]",
									  LLWearableType::getTypeLabel(mType));
		}

		hideTextureControls();
	}
#if 0
	if (mWearableIcon)
	{
		mWearableIcon->setVisible(has_wearable);
	}
#endif
	LLPanel::draw();
}

void LLPanelEditWearable::hideTextureControls()
{
	for (std::map<std::string, S32>::iterator iter = mTextureList.begin(),
											  end = mTextureList.end();
			 iter != end; ++iter)
	{
		childSetVisible(iter->first.c_str(), false);
	}
	for (std::map<std::string, S32>::iterator iter = mColorList.begin(),
											  end = mColorList.end();
			 iter != end; ++iter)
	{
		childSetVisible(iter->first.c_str(), false);
	}
	for (std::map<std::string, S32>::iterator iter = mInvisibilityList.begin(),
											  end = mInvisibilityList.end();
		 iter != end; ++iter)
	{
		childSetVisible(iter->first.c_str(), false);
	}
}

void LLPanelEditWearable::setMaxLayers()
{
	if (mSpinLayer)
	{
		mSpinLayer->setMaxValue((F32)gAgentWearables.getWearableCount(mType));
	}
}

void LLPanelEditWearable::setWearable(LLViewerWearable* wearable,
									  U32 perm_mask, bool is_complete)
{
	mWearable = wearable;
	if (wearable)
	{
		mLayer = 0;
		gAgentWearables.getWearableIndex(wearable, mLayer);

		if (mSpinLayer)
		{
			setMaxLayers();
			mSpinLayer->set((F32)mLayer);
		}
		if (mType == LLWearableType::WT_ALPHA)
		{
			initPreviousTextureList();
		}
	}
	setUIPermissions(perm_mask, is_complete);
}

void LLPanelEditWearable::switchToDefaultSubpart()
{
	setSubpart(getDefaultSubpart());
}

void LLPanelEditWearable::setVisible(bool visible)
{
	LLPanel::setVisible(visible);
	if (!visible)
	{
		for (std::map<std::string, S32>::iterator iter = mColorList.begin(),
												  end = mColorList.end();
			 iter != end; ++iter)
		{
			// This forces any open color pickers to cancel their selection
			childSetEnabled(iter->first.c_str(), false);
		}
	}
}

bool LLPanelEditWearable::isDirty() const
{
	LLViewerWearable* wearable = gAgentWearables.getViewerWearable(mType,
																   mLayer);
	return wearable && wearable->isDirty();
}

//static
void LLPanelEditWearable::onCommitSexChange(LLUICtrl*, void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (!self || !self->mWearable) return;

	if (!isAgentAvatarValid() || !gFloaterCustomizep)
	{
		return;
	}

	if (!gAgentWearables.isWearableModifiable(self->mType, self->mLayer))
	{
		return;
	}

	ESex new_sex = gSavedSettings.getU32("AvatarSex") ? SEX_MALE : SEX_FEMALE;

	LLViewerVisualParam* param =
		(LLViewerVisualParam*)gAgentAvatarp->getVisualParam("male");
	if (!param)
	{
		return;
	}
	self->mWearable->setVisualParamWeight(param->getID(), new_sex == SEX_MALE,
										  true);
	self->mWearable->writeToAvatar(gAgentAvatarp);

	gAgentAvatarp->updateSexDependentLayerSets(true);

	gAgentAvatarp->updateVisualParams();

	gFloaterCustomizep->clearScrollingPanelList();

	// Assumes that we're in the "Shape" Panel.
	self->setSubpart(SUBPART_SHAPE_WHOLE);
}

// Helper for the callback below
void error_message(std::string message)
{
	LLSD args;
	args["MESSAGE"] = message;
	gNotifications.add("GenericAlert", args);
}

//static
void LLPanelEditWearable::importCallback(HBFileSelector::ELoadFilter type,
										 std::string& filename,
										 void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (!gFloaterCustomizep || !self || !self->mWearable || filename.empty() ||
		!isAgentAvatarValid())
	{
		return;
	}
	llinfos << "Selected import file: " << filename << llendl;
	LLXmlTree xml_tree;
	if (!xml_tree.parseFile(filename, false))
	{
		error_message("Can't read the xml file, aborting.");
		return;
	}

	// Check the file format and version
	LLXmlTreeNode* root = xml_tree.getRoot();
	if (!root)
	{
		error_message("No root node found in xml file, aborting.");
		return;
	}
	if (!root->hasName("linden_genepool"))
	{
		error_message("Not an avatar dump, aborting.");
		return;
	}
	std::string version;
	static LLStdStringHandle version_string =
		LLXmlTree::addAttributeString("version");
	if (!root->getFastAttributeString(version_string, version) ||
		version != "1.0")
	{
		error_message("Invalid or missing avatar dump version, aborting.");
		return;
	}
	LLXmlTreeNode* node = root->getChildByName("archetype");
	if (!node)
	{
		error_message("Missing archetype node in avatar dump, aborting.");
		return;
	}

	// Read the file and place the params' id and value in a map
	S32 id;
	F32 value;
	static LLStdStringHandle id_string = LLXmlTree::addAttributeString("id");
	static LLStdStringHandle value_string =
		LLXmlTree::addAttributeString("value");
	std::map<S32, F32> params_map;
	for (LLXmlTreeNode* child = node->getChildByName("param"); child;
		 child = node->getNextNamedChild())
	{
		if (child->getFastAttributeS32(id_string, id) &&
			child->getFastAttributeF32(value_string, value))
		{
			params_map.emplace(id, value);
		}
	}

	// Now set the visual params that correspond to our type
	bool sex_changed = false;
	std::map<S32, F32>::const_iterator it;
	std::map<S32, F32>::const_iterator end = params_map.end();
	for (LLVisualParam* param = gAgentAvatarp->getFirstVisualParam(); param;
		 param = gAgentAvatarp->getNextVisualParam())
	{
		LLViewerVisualParam* viewer_param = (LLViewerVisualParam*)param;
		if (viewer_param->getWearableType() == self->mType &&
			viewer_param->isTweakable())
		{
			id = viewer_param->getID();
			it = params_map.find(id);
			if (it != end)
			{
				value = it->second;
				if (viewer_param->getName() == "male")
				{
					ESex sex = gSavedSettings.getU32("AvatarSex") ? SEX_MALE
																  : SEX_FEMALE;
					ESex new_sex = value > 0.5f ? SEX_MALE : SEX_FEMALE;
					if (new_sex != sex)
					{
						gSavedSettings.setU32("AvatarSex",
											  new_sex == SEX_MALE);
						sex_changed = true;
					}
				}
				llinfos << "Setting param id " << id << " to value "
						<< value << llendl;
				self->mWearable->setVisualParamWeight(id, value, true);
			}
		}
	}
	self->mWearable->writeToAvatar(gAgentAvatarp);
	if (sex_changed)
	{
		gAgentAvatarp->updateSexDependentLayerSets(true);
		gAgentAvatarp->updateVisualParams();
		gFloaterCustomizep->clearScrollingPanelList();
		// Assumes that we're in the "Shape" Panel.
		self->setSubpart(SUBPART_SHAPE_WHOLE);
	}
	else
	{
		gAgentAvatarp->updateVisualParams();
		gFloaterCustomizep->updateScrollingPanelUI();
	}
}

//static
void LLPanelEditWearable::onBtnImport(void* userdata)
{
	HBFileSelector::loadFile(HBFileSelector::FFLOAD_XML, importCallback,
							 userdata);
}

//static
void LLPanelEditWearable::onCommitLayer(LLUICtrl*, void* userdata)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*)userdata;
	if (!self || !self->mSpinLayer || !gFloaterCustomizep) return;

	U32 index = (U32)self->mSpinLayer->get();
	LLViewerWearable* wearable =
		gAgentWearables.getViewerWearable(self->mType, index);
	if (wearable)
	{
		gFloaterCustomizep->updateWearableType(self->mType, wearable);
	}
	else
	{
		self->setWearable(NULL, PERM_ALL, true);
		LLFloaterCustomize::setCurrentWearableType(self->mType);
		gFloaterCustomizep->updateScrollingPanelUI();
	}
}

void LLPanelEditWearable::setUIPermissions(U32 perm_mask, bool is_complete)
{
	bool is_copyable = (perm_mask & PERM_COPY) != 0;
	bool is_modifiable = (perm_mask & PERM_MODIFY) != 0;

	if (mButtonImport)
	{
		mButtonImport->setEnabled(is_modifiable && is_complete);
	}
	if (mButtonSave)
	{
		mButtonSave->setEnabled(is_modifiable && is_complete);
	}
	if (mButtonSaveAs)
	{
		mButtonSaveAs->setEnabled(is_copyable && is_complete);
	}
	if (mSexRadio)
	{
		mSexRadio->setEnabled(is_modifiable && is_complete);
	}

	for (std::map<std::string, S32>::iterator iter = mTextureList.begin(),
											  end = mTextureList.end();
		 iter != end; ++iter)
	{
		childSetVisible(iter->first.c_str(),
						is_copyable && is_modifiable && is_complete);
	}
	for (std::map<std::string, S32>::iterator iter = mColorList.begin(),
											  end = mColorList.end();
		 iter != end; ++iter)
	{
		childSetVisible(iter->first.c_str(), is_modifiable && is_complete);
	}
	for (std::map<std::string, S32>::iterator iter = mInvisibilityList.begin(),
											  end = mInvisibilityList.end();
		 iter != end; ++iter)
	{
		childSetVisible(iter->first.c_str(),
						is_copyable && is_modifiable && is_complete);
	}
}

////////////////////////////////////////////////////////////////////////////

LLWearableSaveAsDialog::LLWearableSaveAsDialog(const std::string& desc,
											   void (*commit_cb)(LLWearableSaveAsDialog*,
																 void*),
											   void* userdata)
:	LLModalDialog(LLStringUtil::null, 240, 100),
	mCommitCallback(commit_cb),
	mCallbackUserData(userdata)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_wearable_save_as.xml");

	childSetAction("Save", LLWearableSaveAsDialog::onSave, this);
	childSetAction("Cancel", LLWearableSaveAsDialog::onCancel, this);
	childSetTextArg("name ed", "[DESC]", desc);
}

//virtual
void LLWearableSaveAsDialog::startModal()
{
	LLModalDialog::startModal();
	LLLineEditor* edit = getChild<LLLineEditor>("name ed", true, false);
	if (edit)
	{
		edit->setFocus(true);
		edit->selectAll();
	}
}

//static
void LLWearableSaveAsDialog::onSave(void* userdata)
{
	LLWearableSaveAsDialog* self = (LLWearableSaveAsDialog*)userdata;
	self->mItemName = self->childGetValue("name ed").asString();
	LLStringUtil::trim(self->mItemName);
	if (!self->mItemName.empty())
	{
		if (self->mCommitCallback)
		{
			self->mCommitCallback(self, self->mCallbackUserData);
		}
		self->close(); // Destroys this object
	}
}

//static
void LLWearableSaveAsDialog::onCancel(void* userdata)
{
	LLWearableSaveAsDialog* self = (LLWearableSaveAsDialog*)userdata;
	self->close(); // Destroys this object
}
