/**
 * @file lltexturectrl.cpp
 * @author Richard Nelson, James Cook
 * @brief LLTextureCtrl class implementation including related functions
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

#include "lltexturectrl.h"

#include "llassetstorage.h"
#include "llavatarappearancedefines.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lldraghandle.h"
#include "llgl.h"
#include "llpermissions.h"
#include "llradiogroup.h"
#include "llrender.h"
#include "llresizehandle.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "llsaleinfo.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterimagepreview.h"
#include "llfloaterinventory.h"
#include "llfolderview.h"
#include "llinventorymodel.h"
#include "llinventorymodelfetch.h"
#include "lllocalbitmaps.h"
#include "llpreviewtexture.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltool.h"
#include "lltoolmgr.h"
#include "lltoolpipette.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"

using namespace LLAvatarAppearanceDefines;

constexpr S32 CLOSE_BTN_WIDTH = 100;
constexpr S32 SMALL_BTN_WIDTH = 64;
constexpr S32 TEX_PICKER_MIN_WIDTH = (HPAD + CLOSE_BTN_WIDTH + HPAD +
									  CLOSE_BTN_WIDTH + HPAD +
									  SMALL_BTN_WIDTH + HPAD +
									  SMALL_BTN_WIDTH + HPAD +
									  30 + RESIZE_HANDLE_WIDTH * 2);
constexpr S32 TEX_PICKER_MIN_HEIGHT = 290;
constexpr S32 FOOTER_HEIGHT = 100;
constexpr S32 BORDER_PAD = HPAD;
constexpr S32 TEXTURE_INVENTORY_PADDING = 30;
constexpr F32 CONTEXT_CONE_IN_ALPHA = 0.f;
constexpr F32 CONTEXT_CONE_OUT_ALPHA = 1.f;
constexpr F32 CONTEXT_FADE_TIME = 0.08f;

///////////////////////////////////////////////////////////////////////////////
// LLFloaterTexturePicker class
///////////////////////////////////////////////////////////////////////////////

class LLFloaterTexturePicker final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterTexturePicker);

public:
	LLFloaterTexturePicker(LLTextureCtrl* owner,
						   const LLRect& rect,
						   const std::string& label,
						   PermissionMask immediate_filter_perm_mask,
						   PermissionMask non_immediate_filter_perm_mask,
						   bool can_apply_immediately,
						   bool allow_local_texture,
						   bool bake_texture_enabled,
						   LLViewerFetchedTexture* texp);

	~LLFloaterTexturePicker() override				{}

	// LLView overrides
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;
	void draw() override;
	bool handleKeyHere(KEY key, MASK mask) override;

	// LLFloater overrides
	bool postBuild() override;
	void onClose(bool app_quitting) override;

	void setImageID(const LLUUID& image_asset_id);
	void updateImageStats();

	LL_INLINE const LLUUID& getAssetID() 			{ return mImageAssetID; }

	const LLUUID& findItemID(const LLUUID& asset_id, bool copyable_only);

	void setCanApplyImmediately(bool b);
	void setLocalTextureEnabled(bool b);
	void setBakeTextureEnabled(bool enabled);

	LL_INLINE void setDirty(bool b)					{ mIsDirty = b; }
	LL_INLINE bool isDirty() const override			{ return mIsDirty; }
	void setActive(bool active);

	LL_INLINE LLTextureCtrl* getOwner() const		{ return mOwner; }
	LL_INLINE void setOwner(LLTextureCtrl* owner)	{ mOwner = owner; }

	void stopUsingPipette();
	PermissionMask getFilterPermMask();
	void updateFilterPermMask();
	void setImmediateFilterPermMask(PermissionMask mask);
	void commitIfImmediateSet();

private:
	static void onBtnSetToDefault(void* userdata);
	static void onBtnSelect(void* userdata);
	static void onBtnCancel(void* userdata);
	static void onBtnPipette(void* userdata);
	static void onBtnBlank(void* userdata);
	static void onBtnInvisible(void* userdata);
	static void onBtnNone(void* userdata);
	static void onBtnClear(void* userdata);
	static void onBtnAdd(void* userdata);
	static void onBtnRemove(void* userdata);
	static void onBtnUpload(void* userdata);
	static void onSelectionChange(LLFolderView* folderp, bool user_action,
								  void* userdata);
	static void onApplyImmediateCheck(LLUICtrl* ctrlp, void* userdata);
	static void onBakeTextureSelect(LLUICtrl* ctrlp, void* userdata);
	static void onSearchEdit(const std::string& search_string, void* userdata);
	static void onTextureSelect(const LLTextureEntry& te, void* userdata);

	static void onModeSelect(LLUICtrl*, void* userdata);
	static void onLocalScrollCommit(LLUICtrl*, void* userdata);

	static void onDragHandleClicked(S32 x, S32 y, void* userdata);

protected:
	LLPointer<LLViewerFetchedTexture>	mTexturep;
	// What to show if currently selected texture is null:
	LLPointer<LLViewerFetchedTexture>	mFallbackImagep;

	LLTextureCtrl*						mOwner;

	LLTextBox*							mTentativeLabel;
	LLTextBox*							mResolutionLabel;

	LLButton*							mPipetteButton;
	LLButton*							mSelectButton;
	LLButton*							mDefaultButton;
	LLButton*							mNoneButton;
	LLButton*							mBlankButton;
	LLButton*							mInvisibleButton;
	LLButton*							mAddButton;
	LLButton*							mRemoveButton;
	LLButton*							mUploadButton;

	LLCheckBoxCtrl*						mApplyImmediatelyCheck;

	LLComboBox*							mBakeTextureCombo;

	LLSearchEditor*						mSearchEdit;

	LLInventoryPanel*					mInventoryPanel;

	LLRadioGroup*						mModeSelector;

	LLScrollListCtrl*					mLocalScrollCtrl;

	LLUUID								mBlankImageAssetID;
	LLUUID								mInvisibleImageAssetID;
	// Currently selected texture
	LLUUID								mImageAssetID;
	// Used when the asset id has no corresponding texture in the user's
	// inventory:
	LLUUID								mSpecialCurrentImageAssetID;
	LLUUID								mOriginalImageAssetID;

	std::string							mLabel;
	std::string							mPendingName;

	S32									mLastBitmapsListVersion;
	F32									mContextConeOpacity;

	LLSaveFolderState					mSavedFolderState;

	PermissionMask						mImmediateFilterPermMask;
	PermissionMask						mNonImmediateFilterPermMask;

	bool								mIsDirty;
	bool								mActive;
	bool								mCanApplyImmediately;
	bool								mNoCopyTextureSelected;
	bool								mBakeTextureEnabled;
};

LLFloaterTexturePicker::LLFloaterTexturePicker(LLTextureCtrl* owner,
											   const LLRect& rect,
											   const std::string& label,
											   PermissionMask immediate_filter_perm_mask,
											   PermissionMask non_immediate_filter_perm_mask,
											   bool can_apply_immediately,
											   bool allow_local_texture,
						 					   bool bake_texture_enabled,
											   LLViewerFetchedTexture* texp)
:	LLFloater("texture picker", rect, "Pick: " + label, true,
			  TEX_PICKER_MIN_WIDTH, TEX_PICKER_MIN_HEIGHT),
	mOwner(owner),
	mImageAssetID(owner->getImageAssetID()),
	mBlankImageAssetID(owner->getBlankImageAssetID()),
	mInvisibleImageAssetID(gSavedSettings.getString("UIImgInvisibleUUID")),
	mOriginalImageAssetID(owner->getImageAssetID()),
	mLabel(label),
	mIsDirty(false),
	mActive(true),
	mImmediateFilterPermMask(immediate_filter_perm_mask),
	mNonImmediateFilterPermMask(non_immediate_filter_perm_mask),
	mNoCopyTextureSelected(false),
	mCanApplyImmediately(can_apply_immediately),
	mBakeTextureEnabled(false),
	mFallbackImagep(texp),
	mContextConeOpacity(0.f),
	mLastBitmapsListVersion(-1)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_texture_ctrl.xml");

	mTentativeLabel = getChild<LLTextBox>("Multiple");

	mResolutionLabel = getChild<LLTextBox>("unknown");

	mDefaultButton = getChild<LLButton>("Default");
	mDefaultButton->setClickedCallback(onBtnSetToDefault, this);

	mNoneButton = getChild<LLButton>("None");
	mNoneButton->setClickedCallback(onBtnNone, this);

	mBlankButton = getChild<LLButton>("Blank");
	mBlankButton->setClickedCallback(onBtnBlank, this);

	mInvisibleButton = getChild<LLButton>("Invisible");
	mInvisibleButton->setClickedCallback(onBtnInvisible, this);

	mAddButton = getChild<LLButton>("Add");
	mAddButton->setClickedCallback(onBtnAdd, this);

	mRemoveButton = getChild<LLButton>("Remove");
	mRemoveButton->setClickedCallback(onBtnRemove, this);
	mRemoveButton->setEnabled(false);

	mUploadButton = getChild<LLButton>("Upload");
	mUploadButton->setClickedCallback(onBtnUpload, this);
	mUploadButton->setEnabled(false);

	mModeSelector = getChild<LLRadioGroup>("mode_selection");
	mModeSelector->setCommitCallback(onModeSelect);
	mModeSelector->setCallbackUserData(this);

	mLocalScrollCtrl = getChild<LLScrollListCtrl>("l_name_list");
	mLocalScrollCtrl->setCommitCallback(onLocalScrollCommit);
	mLocalScrollCtrl->setCallbackUserData(this);
	mLocalScrollCtrl->setCommitOnSelectionChange(true);

	mSearchEdit = getChild<LLSearchEditor>("inventory search editor");
	mSearchEdit->setSearchCallback(onSearchEdit, this);

	// Initialize before mInventoryPanel, since mApplyImmediatelyCheck is used
	// in getFilterPermMask() that we call to set the inventory panel filter
	// permission mask. HB
	mApplyImmediatelyCheck = getChild<LLCheckBoxCtrl>("apply_immediate_check");
	mApplyImmediatelyCheck->set(mCanApplyImmediately &&
								gSavedSettings.getBool("ApplyTextureImmediately"));
	mApplyImmediatelyCheck->setEnabled(mCanApplyImmediately);
	mApplyImmediatelyCheck->setCommitCallback(onApplyImmediateCheck);
	mApplyImmediatelyCheck->setCallbackUserData(this);

	mInventoryPanel = getChild<LLInventoryPanel>("inventory panel");
	U32 filter_types = 0x0;
	filter_types |= 0x1 << LLInventoryType::IT_TEXTURE;
	filter_types |= 0x1 << LLInventoryType::IT_SNAPSHOT;
	mInventoryPanel->setFilterTypes(filter_types);
	mInventoryPanel->setFilterPermMask(getFilterPermMask());
	mInventoryPanel->setFilterPermMask(mImmediateFilterPermMask);
	mInventoryPanel->setSelectCallback(onSelectionChange, this);
	mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
	mInventoryPanel->setAllowMultiSelect(false);
	// Store this filter as the default one
	mInventoryPanel->getRootFolder()->getFilter()->markDefault();
	mInventoryPanel->openDefaultFolderForType(LLAssetType::AT_TEXTURE);
	// Do not put keyboard focus on selected item, because the selection
	// callback will assume that this was user input:
	mInventoryPanel->setSelection(findItemID(mImageAssetID, false),
								  TAKE_FOCUS_NO);

	mBakeTextureCombo = getChild<LLComboBox>("bake_texture_combo");
	mBakeTextureCombo->setCommitCallback(onBakeTextureSelect);
	mBakeTextureCombo->setCallbackUserData(this);

	mPipetteButton = getChild<LLButton>("Pipette");
	mPipetteButton->setClickedCallback(onBtnPipette, this);

	childSetAction("Cancel", LLFloaterTexturePicker::onBtnCancel, this);

	mSelectButton = getChild<LLButton>("Select");
	mSelectButton->setClickedCallback(onBtnSelect, this);

	// Update permission filter once UI is fully initialized
	updateFilterPermMask();

	setCanMinimize(false);

	mSavedFolderState.setApply(false);

	LLDragHandle* drag_handle = getDragHandle();
	if (drag_handle)
	{
		drag_handle->setClickedCallback(onDragHandleClicked, this);
	}

	setLocalTextureEnabled(allow_local_texture);
	setBakeTextureEnabled(bake_texture_enabled);
}

//virtual
bool LLFloaterTexturePicker::postBuild()
{
	if (!mLabel.empty())
	{
		std::string pick = getString("pick title");
		setTitle(pick + mLabel);
	}

	return true;
}

//virtual
void LLFloaterTexturePicker::onClose(bool app_quitting)
{
	if (mOwner)
	{
		mOwner->onFloaterClose();
	}
	stopUsingPipette();
	destroy();
}

//virtual
void LLFloaterTexturePicker::draw()
{
	if (mOwner)
	{
		// Draw cone of context pointing back to texture swatch
		LLRect owner_rect;
		mOwner->localRectToOtherView(mOwner->getLocalRect(), &owner_rect,
									 this);
		LLRect local_rect = getLocalRect();
		if (gFocusMgr.childHasKeyboardFocus(this) &&
			mOwner->isInVisibleChain() && mContextConeOpacity > 0.001f)
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			LLGLEnable(GL_CULL_FACE);
			gGL.begin(LLRender::TRIANGLE_STRIP);
			{
				F32 alpha_in = CONTEXT_CONE_IN_ALPHA * mContextConeOpacity;
				F32 alpha_out = CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity;

				gGL.color4f(0.f, 0.f, 0.f, alpha_out);
				gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, alpha_in);
				gGL.vertex2i(owner_rect.mLeft, owner_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, alpha_out);
				gGL.vertex2i(local_rect.mRight, local_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, alpha_in);
				gGL.vertex2i(owner_rect.mRight, owner_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, alpha_out);
				gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
				gGL.color4f(0.f, 0.f, 0.f, alpha_in);
				gGL.vertex2i(owner_rect.mRight, owner_rect.mBottom);
				gGL.color4f(0.f, 0.f, 0.f, alpha_out);
				gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
				gGL.color4f(0.f, 0.f, 0.f, alpha_in);
				gGL.vertex2i(owner_rect.mLeft, owner_rect.mBottom);
				gGL.color4f(0.f, 0.f, 0.f, alpha_out);
				gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, alpha_in);
				gGL.vertex2i(owner_rect.mLeft, owner_rect.mTop);
			}
			gGL.end();
		}
	}

	F32 opacity = 0.f;
	if (gFocusMgr.childHasMouseCapture(getDragHandle()))
	{
		static LLCachedControl<F32> picker_opacity(gSavedSettings,
												   "PickerContextOpacity");
		opacity = picker_opacity;
	}
	mContextConeOpacity =
		lerp(mContextConeOpacity, opacity,
			 LLCriticalDamp::getInterpolant(CONTEXT_FADE_TIME));

	updateImageStats();

	// If we are inactive, gray out "apply immediate" checkbox
	mSelectButton->setEnabled(mActive);
	mPipetteButton->setEnabled(mActive);
	mPipetteButton->setValue(gToolMgr.isCurrentTool(&gToolPipette));

	// RN: reset search bar to reflect actual search query (all caps, for
	// example)
	mSearchEdit->setText(mInventoryPanel->getFilterSubString());

	if (mOwner)
	{
		mTexturep = NULL;
		if (mImageAssetID.notNull())
		{
			if (LLAvatarAppearanceDictionary::isBakedImageId(mImageAssetID))
			{
				LLViewerObject* obj =
					gSelectMgr.getSelection()->getFirstObject();
				if (obj)
				{
					LLViewerTexture* baked_tex =
						obj->getBakedTextureForMagicId(mImageAssetID);
					if (baked_tex)
					{
						mTexturep = baked_tex->asFetched();
					}
				}
			}
			if (mTexturep.isNull())
			{
				mTexturep =
					LLViewerTextureManager::getFetchedTexture(mImageAssetID,
															  FTT_DEFAULT,
															  true,
															  LLGLTexture::BOOST_PREVIEW);
			}
		}
		else if (mFallbackImagep.notNull())
		{
			mTexturep = mFallbackImagep;
		}

		if (mTentativeLabel)
		{
			mTentativeLabel->setVisible(false);
		}

		const LLUUID& default_id = mOwner->getDefaultImageAssetID();
		mDefaultButton->setEnabled(default_id.notNull() &&
								   mImageAssetID != default_id);
		mBlankButton->setEnabled(mImageAssetID != mBlankImageAssetID);
		mInvisibleButton->setEnabled(mOwner->getAllowInvisibleTexture() &&
									 mImageAssetID != mInvisibleImageAssetID);
		mNoneButton->setEnabled(mOwner->getAllowNoTexture() &&
								mImageAssetID.notNull());

		// Fill-up the local bitmap list if needed
		if (mLastBitmapsListVersion != LLLocalBitmap::getBitmapListVersion())
		{
			mLastBitmapsListVersion = LLLocalBitmap::getBitmapListVersion();

			mLocalScrollCtrl->clearRows();

			const LLLocalBitmap::list_t& bitmaps = LLLocalBitmap::getBitmapList();
			if (!bitmaps.empty())
			{
				for (LLLocalBitmap::list_t::const_iterator
						iter = bitmaps.begin(), end = bitmaps.end();
					 iter != end; ++iter)
				{
					LLLocalBitmap* bitmap = *iter;
					if (!bitmap) continue;	// Paranoia

					LLSD element;
					element["id"] = bitmap->getTrackingID();

					element["columns"][0]["column"] = "unit_name";
					element["columns"][0]["type"]   = "text";
					element["columns"][0]["value"]  = bitmap->getShortName();

					mLocalScrollCtrl->addElement(element);
				}
			}
		}

		LLFloater::draw();

		if (isMinimized())
		{
			return;
		}

		// Border
		LLRect border(BORDER_PAD,
					  getRect().getHeight() - LLFLOATER_HEADER_SIZE - BORDER_PAD,
					  TEX_PICKER_MIN_WIDTH / 2 - TEXTURE_INVENTORY_PADDING - HPAD - BORDER_PAD,
					  BORDER_PAD + FOOTER_HEIGHT + getRect().getHeight() - TEX_PICKER_MIN_HEIGHT);
		gl_rect_2d(border, LLColor4::black, false);

		// Interior
		LLRect interior = border;
		interior.stretch(-1);

		if (mTexturep)
		{
			if (mTexturep->getComponents() == 4)
			{
				gl_rect_2d_checkerboard(interior);
			}

			F32 width = interior.getWidth();
			F32 height = interior.getHeight();
			gl_draw_scaled_image(interior.mLeft, interior.mBottom,
								 width, height, mTexturep);
			// Pump the priority
			mTexturep->addTextureStats(width * height);

			// Draw Tentative Label over the image
			if (mOwner->getTentative() && !mIsDirty)
			{
				mTentativeLabel->setVisible(true);
				drawChild(mTentativeLabel);
			}
		}
		else
		{
			gl_rect_2d(interior, LLColor4::grey);

			// Draw X
			gl_draw_x(interior, LLColor4::black);
		}
	}
}

//virtual
bool LLFloaterTexturePicker::handleDragAndDrop(S32 x, S32 y, MASK mask,
											   bool drop,
											   EDragAndDropType cargo_type,
											   void* cargo_data,
											   EAcceptance* accept,
											   std::string& tooltip_msg)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowinv)
	{
		*accept = ACCEPT_NO;
		return true;
	}
//mk

#if LL_MESH_ASSET_SUPPORT
	if (cargo_type == DAD_TEXTURE || cargo_type == DAD_MESH)
#else
	if (cargo_type == DAD_TEXTURE)
#endif
	{
		LLInventoryItem* item = (LLInventoryItem*)cargo_data;
		const LLPermissions& perms = item->getPermissions();
		PermissionMask item_perm_mask = 0;
		if (perms.allowCopyBy(gAgentID))
		{
			item_perm_mask = PERM_COPY;
		}
		if (perms.allowModifyBy(gAgentID))
		{
			item_perm_mask |= PERM_MODIFY;
		}
		if (perms.allowTransferBy(gAgentID))
		{
			item_perm_mask |= PERM_TRANSFER;
		}

		PermissionMask filter_perm_mask = getFilterPermMask();
		if ((item_perm_mask & filter_perm_mask) == filter_perm_mask)
		{
			if (drop)
			{
				setImageID(item->getAssetUUID());
				commitIfImmediateSet();
			}

			*accept = ACCEPT_YES_SINGLE;
		}
		else
		{
			*accept = ACCEPT_NO;
		}
	}
	else
	{
		*accept = ACCEPT_NO;
	}

	LL_DEBUGS("UserInput") << "dragAndDrop handled by LLFloaterTexturePicker "
						   << getName() << LL_ENDL;

	return true;
}

//virtual
bool LLFloaterTexturePicker::handleKeyHere(KEY key, MASK mask)
{
	LLFolderView* root_folder = mInventoryPanel->getRootFolder();
	if (root_folder && mSearchEdit)
	{
		if (mSearchEdit->hasFocus() && mask == MASK_NONE &&
			(key == KEY_RETURN || key == KEY_DOWN))
		{
			if (!root_folder->getCurSelectedItem())
			{
				LLFolderViewItem* itemp =
					root_folder->getItemByID(gInventory.getRootFolderID());
				if (itemp)
				{
					root_folder->setSelection(itemp, false, false);
					mSelectButton->setEnabled(true);
				}
				else
				{
					mSelectButton->setEnabled(false);
				}
			}
			root_folder->scrollToShowSelection();

			// Move focus to inventory proper
			root_folder->setFocus(true);

			// Treat this as a user selection of the first filtered result
			commitIfImmediateSet();

			return true;
		}

		if (root_folder->hasFocus() && key == KEY_UP)
		{
			mSearchEdit->focusFirstItem(true);
		}
	}

	return LLFloater::handleKeyHere(key, mask);
}

void LLFloaterTexturePicker::setImageID(const LLUUID& image_id)
{
	if (!mActive || mImageAssetID == image_id)
	{
		return;
	}

	mNoCopyTextureSelected = false;
	mIsDirty = true;
	mImageAssetID = image_id;

	S32 mode = mModeSelector->getSelectedIndex();
	if (LLAvatarAppearanceDictionary::isBakedImageId(mImageAssetID))
	{
		if (mBakeTextureEnabled && mode != 2)
		{
			mModeSelector->setSelectedIndex(2, 0);
			onModeSelect((LLUICtrl*)mModeSelector, this);
		}
	}
	else
	{
		if (mode == 2)
		{
			mModeSelector->setSelectedIndex(0, 0);
			onModeSelect((LLUICtrl*)mModeSelector, this);
		}
		LLUUID item_id = findItemID(mImageAssetID, false);
		if (item_id.isNull())
		{
			mInventoryPanel->getRootFolder()->clearSelection();
		}
		else
		{
			LLInventoryItem* itemp = gInventory.getItem(image_id);
			if (itemp && !itemp->getPermissions().allowCopyBy(gAgentID))
			{
				// No-copy texture
				mApplyImmediatelyCheck->set(false);
				mNoCopyTextureSelected = true;
			}
			mInventoryPanel->setSelection(item_id, TAKE_FOCUS_NO);
		}
	}
}

void LLFloaterTexturePicker::setActive(bool active)
{
	if (!active && mPipetteButton->getValue().asBoolean())
	{
		stopUsingPipette();
	}
	mActive = active;
}

void LLFloaterTexturePicker::setCanApplyImmediately(bool b)
{
	mCanApplyImmediately = b;
	if (!mCanApplyImmediately)
	{
	   mApplyImmediatelyCheck->set(false);
	}
	mApplyImmediatelyCheck->setEnabled(mCanApplyImmediately);
	updateFilterPermMask();
}

void LLFloaterTexturePicker::setLocalTextureEnabled(bool b)
{
	mModeSelector->setIndexEnabled(1, b);
}

void LLFloaterTexturePicker::setBakeTextureEnabled(bool b)
{
	bool changed = b != mBakeTextureEnabled;
	mBakeTextureEnabled = b;

	mModeSelector->setIndexEnabled(2, b);

	S32 mode = mModeSelector->getSelectedIndex();
	if (!b && mode == 2)
	{
		mModeSelector->setSelectedIndex(0, 0);
	}
	if (changed && b && mode != 2 &&
		LLAvatarAppearanceDictionary::isBakedImageId(mImageAssetID))
	{
		mModeSelector->setSelectedIndex(2, 0);
	}

	onModeSelect((LLUICtrl*)mModeSelector, this);
}

void LLFloaterTexturePicker::stopUsingPipette()
{
	if (gToolMgr.isCurrentTool(&gToolPipette))
	{
		gToolMgr.clearTransientTool();
	}
}

void LLFloaterTexturePicker::updateImageStats()
{
	if (mTexturep.notNull())
	{
		// RN: have we received header data for this image ?
		if (mTexturep->getFullWidth() > 0 && mTexturep->getFullHeight() > 0)
		{
			std::string formatted_dims = llformat("%d x %d",
												  mTexturep->getFullWidth(),
												  mTexturep->getFullHeight());
			mResolutionLabel->setTextArg("[DIMENSIONS]", formatted_dims);
		}
		else
		{
			mResolutionLabel->setTextArg("[DIMENSIONS]",
										 std::string("[? x ?]"));
		}
	}
	else
	{
		mResolutionLabel->setTextArg("[DIMENSIONS]", std::string(""));
	}
}

const LLUUID& LLFloaterTexturePicker::findItemID(const LLUUID& asset_id,
												 bool copyable_only)
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null, cats, items,
									LLInventoryModel::INCLUDE_TRASH,
									asset_id_matches);
	S32 count = items.size();
	if (count > 0)
	{
		// Search for copyable version first
		for (S32 i = 0; i < count; ++i)
		{
			LLInventoryItem* itemp = items[i];
			LLPermissions item_permissions = itemp->getPermissions();
			if (item_permissions.allowCopyBy(gAgentID, gAgent.getGroupID()))
			{
				return itemp->getUUID();
			}
		}
		// Otherwise just return first instance, unless copyable requested
		if (!copyable_only)
		{
			return items[0]->getUUID();
		}
	}

	return LLUUID::null;
}

PermissionMask LLFloaterTexturePicker::getFilterPermMask()
{
	return mApplyImmediatelyCheck->get() ? mImmediateFilterPermMask
										 : mNonImmediateFilterPermMask;
}

void LLFloaterTexturePicker::commitIfImmediateSet()
{
	if (!mNoCopyTextureSelected && mOwner)
	{
		if (mCanApplyImmediately && mApplyImmediatelyCheck->get())
		{
			mOwner->onFloaterCommit(LLTextureCtrl::TEXTURE_CHANGE);
		}
	}
}

//static
void LLFloaterTexturePicker::onBtnSetToDefault(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (self)
	{
		if (self->mOwner)
		{
			self->setImageID(self->mOwner->getDefaultImageAssetID());
		}
		self->mSelectButton->setEnabled(true);
		self->commitIfImmediateSet();
	}
}

//static
void LLFloaterTexturePicker::onBtnBlank(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (self)
	{
		self->mSelectButton->setEnabled(true);
		self->setImageID(self->mBlankImageAssetID);
		self->commitIfImmediateSet();
	}
}

//static
void LLFloaterTexturePicker::onBtnInvisible(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (self)
	{
		self->mSelectButton->setEnabled(true);
		self->setImageID(self->mInvisibleImageAssetID);
		self->commitIfImmediateSet();
	}
}

//static
void LLFloaterTexturePicker::onBtnNone(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (self)
	{
		self->mSelectButton->setEnabled(true);
		self->setImageID(LLUUID::null);
		self->commitIfImmediateSet();
	}
}

//static
void LLFloaterTexturePicker::onBtnCancel(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (self)
	{
		self->setImageID(self->mOriginalImageAssetID);
		if (self->mOwner)
		{
			self->mOwner->onFloaterCommit(LLTextureCtrl::TEXTURE_CANCEL);
		}
		self->mIsDirty = false;
		self->close();
	}
}

//static
void LLFloaterTexturePicker::onBtnSelect(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (self)
	{
		if (self->mOwner)
		{
			LLUUID local_id, tracking_id;
			if (self->mLocalScrollCtrl->getVisible() &&
				!self->mLocalScrollCtrl->getAllSelected().empty())
			{
				tracking_id = self->mLocalScrollCtrl->getCurrentID();
				local_id = LLLocalBitmap::getWorldID(tracking_id);
			}
			self->mOwner->onFloaterCommit(LLTextureCtrl::TEXTURE_SELECT,
										  local_id, tracking_id);
		}
		self->close();
	}
}

//static
void LLFloaterTexturePicker::onBtnPipette(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (self)
	{
		bool pipette_active = self->mPipetteButton->getValue().asBoolean();
		pipette_active = !pipette_active;
		if (pipette_active)
		{
			gToolPipette.setSelectCallback(onTextureSelect, self);
			gToolMgr.setTransientTool(&gToolPipette);
		}
		else
		{
			gToolMgr.clearTransientTool();
		}
	}
}

//static
void LLFloaterTexturePicker::onSelectionChange(LLFolderView* folderp,
											   bool user_action, void* data)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)data;
	if (!self || !folderp) return;

	bool can_select = false;

	const LLFolderView::selected_items_t& items = folderp->getSelectedItems();
	if (items.size())
	{
		LLFolderViewItem* first_itemp = items.front();
		LLInventoryItem* itemp =
			gInventory.getItem(first_itemp->getListener()->getUUID());
		self->mNoCopyTextureSelected = false;
		if (itemp)
		{
			can_select = true;
			if (!itemp->getPermissions().allowCopyBy(gAgentID))
			{
				self->mNoCopyTextureSelected = true;
			}
			self->mImageAssetID = itemp->getAssetUUID();
			self->mIsDirty = true;
			if (user_action)
			{
				// Only commit intentional selections, not implicit ones
				self->commitIfImmediateSet();
			}
		}
	}

	self->mSelectButton->setEnabled(can_select);
}

//static
void LLFloaterTexturePicker::onModeSelect(LLUICtrl*, void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (!self) return;

	S32 mode = self->mModeSelector->getSelectedIndex();
	bool inventory = mode == 0;
	bool local = mode == 1;
	bool bakes = mode == 2;

	self->mDefaultButton->setVisible(inventory);
	self->mBlankButton->setVisible(inventory);
	self->mNoneButton->setVisible(inventory);
	self->mInvisibleButton->setVisible(inventory);
	self->mPipetteButton->setVisible(inventory);
	self->mSearchEdit->setVisible(inventory);
	self->mPipetteButton->setVisible(inventory);
	self->mInventoryPanel->setVisible(inventory);

	self->mAddButton->setVisible(local);
	self->mRemoveButton->setVisible(local);
	self->mUploadButton->setVisible(local);
	self->mLocalScrollCtrl->setVisible(local);

	self->mBakeTextureCombo->setVisible(bakes);
	if (bakes)
	{
		self->stopUsingPipette();

		const LLUUID& image_id = self->mImageAssetID;
		S32 idx = -1;
		if (image_id == IMG_USE_BAKED_HEAD)
		{
			idx = 0;
		}
		else if (image_id == IMG_USE_BAKED_UPPER)
		{
			idx = 1;
		}
		else if (image_id == IMG_USE_BAKED_LOWER)
		{
			idx = 2;
		}
		else if (image_id == IMG_USE_BAKED_HAIR)
		{
			idx = 3;
		}
		else if (image_id == IMG_USE_BAKED_EYES)
		{
			idx = 4;
		}
		else if (image_id == IMG_USE_BAKED_SKIRT)
		{
			idx = 5;
		}
		else if (image_id == IMG_USE_BAKED_LEFTARM)
		{
			idx = 6;
		}
		else if (image_id == IMG_USE_BAKED_LEFTLEG)
		{
			idx = 7;
		}
		else if (image_id == IMG_USE_BAKED_AUX1)
		{
			idx = 8;
		}
		else if (image_id == IMG_USE_BAKED_AUX2)
		{
			idx = 9;
		}
		else if (image_id == IMG_USE_BAKED_AUX3)
		{
			idx = 10;
		}
		self->mBakeTextureCombo->setSelectedByValue(idx, true);
		self->mSelectButton->setEnabled(true);
	}
}

//static
void LLFloaterTexturePicker::onBtnAdd(void* userdata)
{
	LLLocalBitmap::addUnits();
}

//static
void LLFloaterTexturePicker::onBtnRemove(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (!self) return;

	std::vector<LLScrollListItem*> selected_items =
		self->mLocalScrollCtrl->getAllSelected();
	if (!selected_items.empty())
	{
		for (std::vector<LLScrollListItem*>::iterator
					iter = selected_items.begin(), end = selected_items.end();
			 iter != end; ++iter)
		{
			LLScrollListItem* list_item = *iter;
			if (list_item)
			{
				LLUUID id = list_item->getUUID();
				LLLocalBitmap::delUnit(id);
			}
		}

		self->mRemoveButton->setEnabled(false);
		self->mUploadButton->setEnabled(false);
	}
}

//static
void LLFloaterTexturePicker::onBtnUpload(void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (self)
	{
		std::vector<LLScrollListItem*> items =
			self->mLocalScrollCtrl->getAllSelected();
		for (std::vector<LLScrollListItem*>::iterator
					iter = items.begin(), end = items.end();
			 iter != end; ++iter)
		{
			LLScrollListItem* list_item = *iter;
			if (list_item)
			{
				LLUUID tracking_id = list_item->getUUID();
				std::string filename = LLLocalBitmap::getFilename(tracking_id);
				new LLFloaterImagePreview(filename);
			}
		}
	}
}

//static
void LLFloaterTexturePicker::onLocalScrollCommit(LLUICtrl*, void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (!self) return;

	uuid_vec_t ids = self->mLocalScrollCtrl->getSelectedIDs();
	S32 items = ids.size();
	bool has_selection = items > 0;
	bool has_one_selection = items == 1;

	self->mRemoveButton->setEnabled(has_selection);
	self->mUploadButton->setEnabled(has_selection);
	self->mSelectButton->setEnabled(has_one_selection);

	if (has_one_selection && self->mOwner)
	{
		const LLUUID& inworld_id = LLLocalBitmap::getWorldID(ids[0]);
		self->mOwner->setImageAssetID(inworld_id);
		if (self->mCanApplyImmediately && self->mApplyImmediatelyCheck->get())
		{
			self->mOwner->onFloaterCommit(LLTextureCtrl::TEXTURE_CHANGE,
										  inworld_id, ids[0]);
		}
	}
}

//static
void LLFloaterTexturePicker::onApplyImmediateCheck(LLUICtrl* ctrlp,
												   void* user_data)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)user_data;
	LLCheckBoxCtrl* checkp = (LLCheckBoxCtrl*)ctrlp;
	if (self && checkp)
	{
		gSavedSettings.setBool("ApplyTextureImmediately", checkp->get());
		self->updateFilterPermMask();
		self->commitIfImmediateSet();
	}
}

//static
void LLFloaterTexturePicker::onBakeTextureSelect(LLUICtrl* ctrlp,
												 void* user_data)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)user_data;
	LLComboBox* combop = (LLComboBox*)ctrlp;
	if (!self || !combop) return;

	LLUUID image_id = self->mOwner->getDefaultImageAssetID();
	S32 type = combop->getValue().asInteger();
	switch (type)
	{
		case 0: image_id = IMG_USE_BAKED_HEAD; break;
		case 1: image_id = IMG_USE_BAKED_UPPER; break;
		case 2: image_id = IMG_USE_BAKED_LOWER; break;
		case 3: image_id = IMG_USE_BAKED_HAIR; break;
		case 4: image_id = IMG_USE_BAKED_EYES; break;
		case 5: image_id = IMG_USE_BAKED_SKIRT; break;
		case 6: image_id = IMG_USE_BAKED_LEFTARM; break;
		case 7: image_id = IMG_USE_BAKED_LEFTLEG; break;
		case 8: image_id = IMG_USE_BAKED_AUX1; break;
		case 9: image_id = IMG_USE_BAKED_AUX2; break;
		case 10: image_id = IMG_USE_BAKED_AUX3; break;
		default: break;
	}

	self->setImageID(image_id);
	self->mSelectButton->setEnabled(true);
	self->commitIfImmediateSet();
}

void LLFloaterTexturePicker::updateFilterPermMask()
{
	mInventoryPanel->setFilterPermMask(getFilterPermMask());
}

void LLFloaterTexturePicker::setImmediateFilterPermMask(PermissionMask mask)
{
	// Do not re-apply the same mask: it would cause an useless inventory
	// re-filtering. HB
	if (mImmediateFilterPermMask != mask)
	{
		mImmediateFilterPermMask = mask;
		mInventoryPanel->setFilterPermMask(mask);
	}
}

void LLFloaterTexturePicker::onSearchEdit(const std::string& search_string,
										  void* user_data)
{
	LLFloaterTexturePicker* picker = (LLFloaterTexturePicker*)user_data;
	if (!picker) return;

	std::string upper_case_search_string = search_string;
	LLStringUtil::toUpper(upper_case_search_string);

	if (upper_case_search_string.empty())
	{
		if (picker->mInventoryPanel->getFilterSubString().empty())
		{
			// current filter and new filter empty, do nothing
			return;
		}

		picker->mSavedFolderState.setApply(true);
		picker->mInventoryPanel->getRootFolder()->applyFunctorRecursively(picker->mSavedFolderState);
		// Add folder with current item to list of previously opened folders
		LLOpenFoldersWithSelection opener;
		picker->mInventoryPanel->getRootFolder()->applyFunctorRecursively(opener);
		picker->mInventoryPanel->getRootFolder()->scrollToShowSelection();

	}
	else if (picker->mInventoryPanel->getFilterSubString().empty())
	{
		// First letter in search term, save existing folder open state
		if (!picker->mInventoryPanel->getRootFolder()->isFilterModified())
		{
			picker->mSavedFolderState.setApply(false);
			picker->mInventoryPanel->getRootFolder()->applyFunctorRecursively(picker->mSavedFolderState);
		}
	}

	picker->mInventoryPanel->setFilterSubString(upper_case_search_string);
}

//static
void LLFloaterTexturePicker::onTextureSelect(const LLTextureEntry& te,
											 void* data)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)data;
	if (!self) return;

	LLUUID inventory_item_id = self->findItemID(te.getID(), true);
	if (inventory_item_id.notNull())
	{
		gToolPipette.setResult(true, "");
		self->setImageID(te.getID());

		self->mNoCopyTextureSelected = false;
		LLInventoryItem* itemp = gInventory.getItem(inventory_item_id);

		if (itemp && !itemp->getPermissions().allowCopyBy(gAgentID))
		{
			// No-copy texture
			self->mNoCopyTextureSelected = true;
		}

		self->commitIfImmediateSet();
		self->mSelectButton->setEnabled(true);
	}
	else
	{
		self->mSelectButton->setEnabled(false);
		gToolPipette.setResult(false, self->getString("not_in_inventory"));
	}
}

void LLFloaterTexturePicker::onDragHandleClicked(S32 x, S32 y, void* userdata)
{
	LLFloaterTexturePicker* self = (LLFloaterTexturePicker*)userdata;
	if (!self || !self->mTexturep || self->mImageAssetID.isNull() ||
		LLPreview::show(self->mImageAssetID))
	{
		return;
	}

	// Only react if the mouse pointer is within the preview area
	const LLRect& rect = self->getRect();
	LLRect preview_area(BORDER_PAD,
						rect.getHeight() - LLFLOATER_HEADER_SIZE - BORDER_PAD,
						TEX_PICKER_MIN_WIDTH / 2 - TEXTURE_INVENTORY_PADDING -
						HPAD - BORDER_PAD,
						BORDER_PAD + FOOTER_HEIGHT + rect.getHeight() -
						TEX_PICKER_MIN_HEIGHT);
	if (preview_area.pointInRect(x, y))
	{
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		std::string title = "Texture preview";
		LLPreviewTexture* preview = new LLPreviewTexture(title, rect, title,
														 self->mImageAssetID,
														 false);
		preview->setNotCopyable();
		preview->childSetText("desc", title);
		preview->childSetEnabled("desc", false);
		preview->setFocus(true);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLTextureCtrl class
///////////////////////////////////////////////////////////////////////////////

static const std::string LL_TEXTURE_CTRL_TAG = "texture_picker";
static LLRegisterWidget<LLTextureCtrl> r(LL_TEXTURE_CTRL_TAG);

LLTextureCtrl::LLTextureCtrl(const std::string& name,
							 const LLRect& rect,
							 const std::string& label,
							 const LLUUID& image_id,
							 const LLUUID& default_image_id,
							 const std::string& default_image_name)
:	LLUICtrl(name, rect, true, NULL, NULL, FOLLOWS_LEFT | FOLLOWS_TOP),
	mDragCallback(NULL),
	mDropCallback(NULL),
	mOnCancelCallback(NULL),
	mOnCloseCallback(NULL),
	mOnSelectCallback(NULL),
	mBorderColor(LLUI::sDefaultHighlightLight),
	mImageAssetID(image_id),
	mDefaultImageAssetID(default_image_id),
	mDefaultImageName(default_image_name),
	mBlankImageAssetID(gSavedSettings.getString("UIImgWhiteUUID")),
	mDisplayRatio(0.f),
	mLabel(label),
	mAllowNoTexture(false),
	mAllowInvisibleTexture(false),
	mAllowLocalTexture(true),
	mBakeTextureEnabled(false),
	mImmediateFilterPermMask(PERM_NONE),
	mNonImmediateFilterPermMask(PERM_NONE),
	mCanApplyImmediately(false),
	mValid(true),
	mDirty(false),
	mEnabled(true),
	mCaptionAlwaysEnabled(false),
	mShowLoadingPlaceholder(true)
{
	mCaption = new LLTextBox(label,
							 LLRect(0, gBtnHeightSmall, getRect().getWidth(), 0),
							 label, LLFontGL::getFontSansSerifSmall());
	mCaption->setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT | FOLLOWS_BOTTOM);
	addChild(mCaption);

	S32 image_top = getRect().getHeight();
	S32 image_bottom = gBtnHeightSmall;
	S32 image_middle = (image_top + image_bottom) / 2;
	S32 line_height =
		ll_roundp(LLFontGL::getFontSansSerifSmall()->getLineHeight());

	mTentativeLabel = new LLTextBox(std::string("Multiple"),
									LLRect(0, image_middle + line_height / 2,
										   getRect().getWidth(),
										   image_middle - line_height / 2),
									std::string("Multiple"),
									LLFontGL::getFontSansSerifSmall());
	mTentativeLabel->setHAlign(LLFontGL::HCENTER);
	mTentativeLabel->setFollowsAll();
	addChild(mTentativeLabel);

	LLRect border_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
	border_rect.mBottom += gBtnHeightSmall;
	mBorder = new LLViewBorder(std::string("border"), border_rect,
							   LLViewBorder::BEVEL_IN);
	mBorder->setFollowsAll();
	addChild(mBorder);

	setEnabled(true); // For the tooltip
	mLoadingPlaceholderString = LLTrans::getWString("texture_loading");
}

//virtual
LLTextureCtrl::~LLTextureCtrl()
{
	closeFloater();
}

//virtual
LLXMLNodePtr LLTextureCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_TEXTURE_CTRL_TAG);

	node->createChild("label", true)->setStringValue(getLabel());

	node->createChild("default_image_name", true)->setStringValue(getDefaultImageName());

	node->createChild("allow_no_texture", true)->setBoolValue(mAllowNoTexture);

	node->createChild("allow_invisible_texture", true)->setBoolValue(mAllowInvisibleTexture);

	node->createChild("can_apply_immediately", true)->setBoolValue(mCanApplyImmediately);

	return node;
}

LLView* LLTextureCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
							   LLUICtrlFactory* factory)
{
	std::string name = LL_TEXTURE_CTRL_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent);

	std::string label;
	node->getAttributeString("label", label);

	std::string image_id("");
	node->getAttributeString("image", image_id);

	std::string default_image_id("");
	node->getAttributeString("default_image", default_image_id);

	std::string default_image_name("Default");
	node->getAttributeString("default_image_name", default_image_name);

	bool allow_no_texture = false;
	node->getAttributeBool("allow_no_texture", allow_no_texture);

	bool allow_invisible_texture = false;
	node->getAttributeBool("allow_invisible_texture", allow_invisible_texture);

	bool can_apply_immediately = false;
	node->getAttributeBool("can_apply_immediately", can_apply_immediately);

	bool can_use_bakes = false;
	node->getAttributeBool("can_use_bakes", can_use_bakes);

	if (label.empty())
	{
		label.assign(node->getValue());
	}

	LLTextureCtrl* self = new LLTextureCtrl(name, rect, label,
											LLUUID(image_id),
											LLUUID(default_image_id),
											default_image_name);
	self->setAllowNoTexture(allow_no_texture);
	self->setAllowInvisibleTexture(allow_invisible_texture);
	self->setCanApplyImmediately(can_apply_immediately);
	self->setBakeTextureEnabled(can_use_bakes);

	self->initFromXML(node, parent);

	return self;
}

void LLTextureCtrl::setCaption(const std::string& caption)
{
	mCaption->setText(caption);
}

void LLTextureCtrl::setCanApplyImmediately(bool b)
{
	mCanApplyImmediately = b;
	LLFloaterTexturePicker* floaterp =
		(LLFloaterTexturePicker*)mFloaterHandle.get();
	if (floaterp)
	{
		floaterp->setCanApplyImmediately(b);
	}
}

void LLTextureCtrl::setBakeTextureEnabled(bool b)
{
	mBakeTextureEnabled = b;
	LLFloaterTexturePicker* floaterp =
		(LLFloaterTexturePicker*)mFloaterHandle.get();
	if (floaterp)
	{
		floaterp->setBakeTextureEnabled(b);
	}
}

void LLTextureCtrl::setImmediateFilterPermMask(PermissionMask mask)
{
	mImmediateFilterPermMask = mask;
	LLFloaterTexturePicker* floaterp =
		(LLFloaterTexturePicker*)mFloaterHandle.get();
	if (floaterp)
	{
		floaterp->setImmediateFilterPermMask(mask);
	}
}

//virtual
void LLTextureCtrl::setVisible(bool visible)
{
	if (!visible)
	{
		closeFloater();
	}
	LLUICtrl::setVisible(visible);
}

//virtual
void LLTextureCtrl::setEnabled(bool enabled)
{
	mEnabled = enabled;

	LLFloaterTexturePicker* floaterp =
		(LLFloaterTexturePicker*)mFloaterHandle.get();
	if (floaterp)
	{
		if (!enabled)
		{
			// *TODO: would be better to keep floater open and show disabled
			// state.
			closeFloater();
		}
		floaterp->setActive(enabled);
	}

	LLView::setEnabled(true);
	mCaption->setEnabled(enabled || mCaptionAlwaysEnabled);
}

void LLTextureCtrl::setValid(bool valid)
{
	mValid = valid;
	if (!valid)
	{
		LLFloaterTexturePicker* pickerp =
			(LLFloaterTexturePicker*)mFloaterHandle.get();
		if (pickerp)
		{
			pickerp->setActive(false);
		}
	}
}

void LLTextureCtrl::setFallbackImageName(const std::string& image_name)
{
	mFallbackImagep =
		LLViewerTextureManager::getFetchedTextureFromFile(image_name,
														  MIPMAP_YES,
														  LLGLTexture::BOOST_PREVIEW,
														  LLViewerTexture::LOD_TEXTURE);
}

//virtual
void LLTextureCtrl::clear()
{
	setImageAssetID(LLUUID::null);
}

void LLTextureCtrl::setLabel(const std::string& label)
{
	mLabel = label;
	mCaption->setText(label);
}

void LLTextureCtrl::showPicker(bool take_focus)
{
	LLFloater* floaterp = mFloaterHandle.get();

	// Show the dialog
	if (floaterp)
	{
		floaterp->open();
	}
	else
	{
		if (!mLastFloaterLeftTop.mX && !mLastFloaterLeftTop.mY)
		{
			gFloaterViewp->getNewFloaterPosition(&mLastFloaterLeftTop.mX,
												&mLastFloaterLeftTop.mY);
		}
		LLRect rect = gSavedSettings.getRect("TexturePickerRect");
		rect.translate(mLastFloaterLeftTop.mX - rect.mLeft,
					   mLastFloaterLeftTop.mY - rect.mTop);

		floaterp = new LLFloaterTexturePicker(this, rect, mLabel,
											  mImmediateFilterPermMask,
											  mNonImmediateFilterPermMask,
											  mCanApplyImmediately,
											  mAllowLocalTexture,
											  mBakeTextureEnabled,
											  mFallbackImagep.get());
		mFloaterHandle = floaterp->getHandle();

		if (gFloaterViewp)
		{
			LLFloater* parentp = gFloaterViewp->getParentFloater(this);
			if (parentp)
			{
				parentp->addDependentFloater(floaterp);
			}
		}

		floaterp->open();
	}

	if (take_focus)
	{
		floaterp->setFocus(true);
	}
}

void LLTextureCtrl::closeFloater()
{
	LLFloaterTexturePicker* floaterp =
		(LLFloaterTexturePicker*)mFloaterHandle.get();
	if (floaterp)
	{
		floaterp->setOwner(NULL);
		floaterp->close();
	}
}

//virtual
bool LLTextureCtrl::handleHover(S32 x, S32 y, MASK mask)
{
	gWindowp->setCursor(UI_CURSOR_HAND);
	return true;
}

//virtual
bool LLTextureCtrl::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (!LLUICtrl::handleMouseDown(x, y , mask))
	{
		return false;
	}

	if (mEnabled)
	{
		showPicker(false);

		// Ensure textures default folder is loaded
		const LLUUID& tex_folder_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_TEXTURE);
		LLInventoryModelFetch::getInstance()->start(tex_folder_id);
	}
	else if (mImageAssetID.notNull() && !LLPreview::show(mImageAssetID))
	{
		// There is no preview, so make a new one
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		std::string title = "Texture Preview";
		LLPreviewTexture* preview = new LLPreviewTexture(title, rect, title,
														 mImageAssetID, false);
		preview->setNotCopyable();
		preview->childSetText("desc", title);
		preview->childSetEnabled("desc", false);
		preview->setFocus(true);
	}

	return true;
}

void LLTextureCtrl::onFloaterClose()
{
	LLFloaterTexturePicker* floaterp =
		(LLFloaterTexturePicker*)mFloaterHandle.get();
	if (floaterp)
	{
		if (mOnCloseCallback)
		{
			mOnCloseCallback(this, mCallbackUserData);
		}
		floaterp->setOwner(NULL);
		mLastFloaterLeftTop.set(floaterp->getRect().mLeft,
								floaterp->getRect().mTop);
	}
	mFloaterHandle.markDead();
}

void LLTextureCtrl::onFloaterCommit(ETexturePickOp op, const LLUUID& id,
									const LLUUID& tracking_id)
{
	LLFloaterTexturePicker* floaterp =
		(LLFloaterTexturePicker*)mFloaterHandle.get();
	if (floaterp && getEnabled())
	{
		mLocalTrackingID = tracking_id;
		mDirty = (op != TEXTURE_CANCEL);
		if (floaterp->isDirty() || id.notNull())
		{
			setTentative(false);
			if (id.notNull())
			{
				mImageItemID = id;
				mImageAssetID = id;
			}
			else
			{
				mImageItemID = floaterp->findItemID(floaterp->getAssetID(),
													false);
				LL_DEBUGS("TextureCtrl") << "mImageItemID: " << mImageItemID
										 << LL_ENDL;
				mImageAssetID = floaterp->getAssetID();
				LL_DEBUGS("TextureCtrl") << "mImageAssetID: " << mImageAssetID
										 << LL_ENDL;
			}
			if (op == TEXTURE_SELECT && mOnSelectCallback)
			{
				mOnSelectCallback(this, mCallbackUserData);
			}
			else if (op == TEXTURE_CANCEL && mOnCancelCallback)
			{
				mOnCancelCallback(this, mCallbackUserData);
			}
			else
			{
				onCommit();
			}
		}
	}
}

void LLTextureCtrl::setImageAssetID(const LLUUID& asset_id)
{
	if (mImageAssetID != asset_id)
	{
		mImageItemID.setNull();
		mLocalTrackingID.setNull();
		mImageAssetID = asset_id;
		LLFloaterTexturePicker* floaterp =
			(LLFloaterTexturePicker*)mFloaterHandle.get();
		if (floaterp && getEnabled())
		{
			floaterp->setImageID(asset_id);
			floaterp->setDirty(false);
		}
	}
}

//virtual
bool LLTextureCtrl::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
									  EDragAndDropType cargo_type,
									  void* cargo_data, EAcceptance* accept,
									  std::string& tooltip_msg)
{
	// This downcast may be invalid, but if the second test below returns true,
	// then the cast was valid, and we can perform the third test without
	// problem.
	LLInventoryItem* item = (LLInventoryItem*)cargo_data;

	if (getEnabled() &&
#if LL_MESH_ASSET_SUPPORT
		(cargo_type == DAD_TEXTURE || cargo_type == DAD_MESH) &&
#else
		cargo_type == DAD_TEXTURE &&
#endif
		allowDrop(item))
	{
		if (drop)
		{
			if (doDrop(item))
			{
				// This removes the 'Multiple' overlay, since there is now only
				// one texture selected.
				setTentative(false);
				onCommit();
			}
		}

		*accept = ACCEPT_YES_SINGLE;
	}
	else
	{
		*accept = ACCEPT_NO;
	}

	LL_DEBUGS("UserInput") << "dragAndDrop handled by LLTextureCtrl "
						   << getName() << LL_ENDL;

	return true;
}

//virtual
void LLTextureCtrl::draw()
{
	mBorder->setKeyboardFocusHighlight(hasFocus());

	if (!mValid)
	{
		mTexturep = NULL;
	}
	else if (mImageAssetID.notNull())
	{
		LLPointer<LLViewerFetchedTexture> texture = NULL;
		if (LLAvatarAppearanceDictionary::isBakedImageId(mImageAssetID))
		{
			LLViewerObject* obj = gSelectMgr.getSelection()->getFirstObject();
			if (obj)
			{
				LLViewerTexture* baked_tex =
					obj->getBakedTextureForMagicId(mImageAssetID);
				if (baked_tex)
				{
					texture = baked_tex->asFetched();
				}
			}
		}
		if (texture.notNull())
		{
			mTexturep = texture;
		}
		else
		{
			mTexturep =
				LLViewerTextureManager::getFetchedTexture(mImageAssetID,
														  FTT_DEFAULT, true,
														  LLGLTexture::BOOST_PREVIEW,
														  LLViewerTexture::LOD_TEXTURE);
		}
		mTexturep->forceToSaveRawImage(0);
	}
	else if (mFallbackImagep.notNull())
	{
		// Show fallback image.
		mTexturep = mFallbackImagep;
	}
	else	// mImageAssetID.notNull()
	{
		mTexturep = NULL;
	}

	// Border
	LLRect border(0, getRect().getHeight(), getRect().getWidth(),
				  gBtnHeightSmall);
	gl_rect_2d(border, mBorderColor, false);

	// Interior
	LLRect interior = border;
	interior.stretch(-1);

	if (mTexturep)
	{
		bool draw_checker_board = mTexturep->getComponents() == 4;
		if (draw_checker_board)
		{
			gl_rect_2d_checkerboard(interior);
		}

		F32 left = interior.mLeft;
		F32 bottom = interior.mBottom;
		F32 width = interior.getWidth();
		F32 height = interior.getHeight();
		// Pump the priority
		mTexturep->addTextureStats(width * height);
		if (mDisplayRatio > 0.f &&
			!is_approx_zero(mDisplayRatio - width / height))
		{
			if (!draw_checker_board)
			{
				// Draw a black background that will show as thick strips
				// around the resized picture.
				gl_rect_2d(interior, LLColor4::black);
			}
			F32 proportion = mDisplayRatio * height / width;
			if (proportion < 1.f)
			{
				left += (width - width * proportion) * 0.5f;
				width *= proportion;
			}
			else
			{
				bottom += (height - height / proportion) * 0.5f;
				height /= proportion;
			}
		}
		gl_draw_scaled_image(left, bottom, width, height, mTexturep);
	}
	else
	{
		gl_rect_2d(interior, LLColor4::grey);

		// Draw X
		gl_draw_x(interior, LLColor4::black);
	}

	mTentativeLabel->setVisible(mTexturep.notNull() && getTentative());

	// Show "Loading..." string on the top left corner while this texture is
	// loading. Using the discard level, do not show the string if the texture
	// is almost but not fully loaded.
	if (mTexturep.notNull() && mShowLoadingPlaceholder &&
		!mTexturep->isFullyLoaded())
	{
		static LLFontGL* sans = LLFontGL::getFontSansSerif();
		static LLFontGL* big = LLFontGL::getFontSansSerifBig();
		LLFontGL* font = interior.getWidth() < 128 ? sans : big;
		font->render(mLoadingPlaceholderString, 0,
					llfloor(interior.mLeft + 4), llfloor(interior.mTop - 20),
					 LLColor4::white, LLFontGL::LEFT, LLFontGL::BASELINE,
					 LLFontGL::DROP_SHADOW);
	}

	LLUICtrl::draw();
}

bool LLTextureCtrl::allowDrop(LLInventoryItem* item)
{
	const LLPermissions& perms = item->getPermissions();
	PermissionMask item_perm_mask = 0;
	if (perms.allowCopyBy(gAgentID))
	{
		item_perm_mask = PERM_COPY;
	}
	if (perms.allowModifyBy(gAgentID))
	{
		item_perm_mask |= PERM_MODIFY;
	}
	if (perms.allowTransferBy(gAgentID))
	{
		item_perm_mask |= PERM_TRANSFER;
	}

	// Never allow to apply no-copy textures by dropping them: the drop code
	// would delete the texture from the inventory... HB
	PermissionMask filter_perm_mask = PERM_COPY;
	filter_perm_mask |= mCanApplyImmediately ? mImmediateFilterPermMask
											 : mNonImmediateFilterPermMask;
	if ((item_perm_mask & filter_perm_mask) != filter_perm_mask)
	{
		return false;
	}

	if (!mDragCallback)
	{
		return true;
	}

	return mDragCallback(this, item, mCallbackUserData);
}

bool LLTextureCtrl::doDrop(LLInventoryItem* item)
{
	if (!mDropCallback)
	{
		// No callback installed, so just set the image ids and carry on.
		setImageAssetID(item->getAssetUUID());
		mImageItemID = item->getUUID();
		return true;
	}

	// Call callback; if it returns true, we return true, and therefore the
	// commit is called above.
	return mDropCallback(this, item, mCallbackUserData);
}

//virtual
bool LLTextureCtrl::handleUnicodeCharHere(llwchar uni_char)
{
	if (uni_char == ' ')
	{
		showPicker(true);
		return true;
	}
	return LLUICtrl::handleUnicodeCharHere(uni_char);
}

//virtual
void LLTextureCtrl::setValue(const LLSD& value)
{
	setImageAssetID(value.asUUID());
}

//virtual
LLSD LLTextureCtrl::getValue() const
{
	return LLSD(getImageAssetID());
}
