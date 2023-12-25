/**
 * @file llpreviewmaterial.h
 * @brief LLPreviewMaterial class declaration
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

#ifndef LL_LLPREVIEWMATERIAL_H
#define LL_LLPREVIEWMATERIAL_H

#include <set>

#include "boost/signals2.hpp"

#include "llassettype.h"
#include "hbfastmap.h"
#include "llimagej2c.h"

#include "llpreview.h"
#include "llviewertexture.h"
#include "llvoinventorylistener.h"

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLGLTFMaterial;
class LLLocalGLTFMaterial;
class LLPermissions;
class LLSpinCtrl;
class LLTextBox;
class LLTextureCtrl;

namespace tinygltf
{
	class Model;
}

class LLPreviewMaterial final : public LLPreview, public LLVOInventoryListener
{
	friend class LLMaterialCopiedCB;

protected:
	LOG_CLASS(LLPreviewMaterial);

	// Constructor used internally only, for the live editor and uploads.
	LLPreviewMaterial(const std::string& name, bool live_editor = false);

public:
	// Constructor used to preview/edit inventory items.
	LLPreviewMaterial(const std::string& name, const LLRect& rect,
					  const std::string& title, const LLUUID& item_uuid,
					  const LLUUID& object_uuid);
	~LLPreviewMaterial() override;

	bool setFromGltfModel(const tinygltf::Model& model, S32 index,
						  bool set_textures = false);

	void setFromGltfMetaData(const std::string& filename,
							 const tinygltf::Model& model, S32 index);

	// For live preview, applies current material to currently selected object
	void applyToSelection();

	void getGLTFMaterial(LLGLTFMaterial* matp);

	void setMaterialName(const std::string& name);

	LL_INLINE LLUUID getBaseColorId()
	{
		return getTextureId(mBaseColorTexCtrl);
	}

	LL_INLINE void setBaseColorId(const LLUUID& id)
	{
		setTextureId(mBaseColorTexCtrl, id);
	}

	LL_INLINE void setBaseColorUploadId(const LLUUID& id)
	{
		setTextureUploadId(mBaseColorTexCtrl, id);
	}

	// Gets/sets both base color and transparency
	LLColor4 getBaseColor() const;
	void setBaseColor(const LLColor4& color);

	LL_INLINE F32 getTransparency() const
	{
		// Note: spinner is from 0 to 100% for 1.0 to 0.0 alpha value. HB
		return 1.f - getCtrlValue(mTransparencyCtrl) / 100.f;
	}

	LL_INLINE void setTransparency(F32 transparency)
	{
		// Note: spinner is from 0 to 100% for 1.0 to 0.0 alpha value. HB
		setCtrlValue(mTransparencyCtrl, (1.f - transparency) * 100.f);
	}

	std::string getAlphaMode() const;
	void setAlphaMode(const std::string& alpha_mode);

	LL_INLINE F32 getAlphaCutoff() const
	{
		return getCtrlValue(mAlphaCutoffCtrl);
	}

	LL_INLINE void setAlphaCutoff(F32 alpha_cutoff)
	{
		setCtrlValue(mAlphaCutoffCtrl, alpha_cutoff);
	}

	LL_INLINE LLUUID getMetallicRoughnessId() const
	{
		return getTextureId(mMetallicTexCtrl);
	}

	LL_INLINE void setMetallicRoughnessId(const LLUUID& id)
	{
		setTextureId(mMetallicTexCtrl, id);
	}

	LL_INLINE void setMetallicRoughnessUploadId(const LLUUID& id)
	{
		setTextureUploadId(mMetallicTexCtrl, id);
	}

	LL_INLINE F32 getMetalnessFactor() const
	{
		return getCtrlValue(mMetalnessCtrl);
	}

	LL_INLINE void setMetalnessFactor(F32 factor)
	{
		setCtrlValue(mMetalnessCtrl, factor);
	}

	LL_INLINE F32 getRoughnessFactor() const
	{
		return getCtrlValue(mRoughnessCtrl);
	}

	LL_INLINE void setRoughnessFactor(F32 factor)
	{
		setCtrlValue(mRoughnessCtrl, factor);
	}

	LL_INLINE LLUUID getEmissiveId() const
	{
		return getTextureId(mEmissiveTexCtrl);
	}

	LL_INLINE void setEmissiveId(const LLUUID& id)
	{
		setTextureId(mEmissiveTexCtrl, id);
	}

	LL_INLINE void setEmissiveUploadId(const LLUUID& id)
	{
		setTextureUploadId(mEmissiveTexCtrl, id);
	}

	LLColor4 getEmissiveColor() const;
	void setEmissiveColor(const LLColor4& color);

	LL_INLINE LLUUID getNormalId() const
	{
		return getTextureId(mNormalTexCtrl);
	}

	LL_INLINE void setNormalId(const LLUUID& id)
	{
		setTextureId(mNormalTexCtrl, id);
	}

	LL_INLINE void setNormalUploadId(const LLUUID& id)
	{
		setTextureUploadId(mNormalTexCtrl, id);
	}

	bool getDoubleSided() const;
	void setDoubleSided(bool double_sided);

	// Returns a pointer on the last opened preview floater on success (there
	// may be several opened floaters when the file contains more than one
	// material and 'index' is ommitted or negative), or NULL on failure. HB
	static LLPreviewMaterial* loadFromFile(const std::string& filename,
										   S32 index = -1);

	static bool canModifyObjectsMaterial();
	static bool canSaveObjectsMaterial();

	static void saveObjectsMaterial();

	static void loadLive();
	static void updateLive();
	static void updateLive(const LLUUID& object_id, S32 te);
	// Called on live overrides selection changes
	static void onSelectionChanged();

	// Initializes the UI from a default GLTF material
	void loadDefaults();

	LL_INLINE U32 getUnsavedChangesFlags() const	{ return mUnsavedChanges; }
	LL_INLINE U32 getRevertedChangesFlags() const	{ return mRevertedChanges; }

	// Local textures support
	const LLUUID& getLocalTexTrackingIdFromFlag(U32 flag) const;
	bool updateMaterialLocalSubscription(LLGLTFMaterial* matp);

	static LLPreviewMaterial* getLiveEditorInstance();

private:
	// LLView overrides
	void draw() override;

	// LLPanel override
	bool postBuild() override;

	// LLVOInventoryListener override
	void inventoryChanged(LLViewerObject*, LLInventoryObject::object_list_t*,
						  S32, void*) override;

	void refreshFromInventory(const LLUUID& new_item_id = LLUUID::null);

	// LLPreview overrides
	void loadAsset() override;
	LL_INLINE const char* getTitleName() const override	{ return "Material"; }
	void setItemID(const LLUUID& object_id) override;
	void setAuxItem(const LLInventoryItem* itemp) override;

	LLUUID getTextureId(LLTextureCtrl* ctrlp) const;
	void setTextureId(LLTextureCtrl* ctrlp, const LLUUID& id);
	void setTextureUploadId(LLTextureCtrl* ctrlp, const LLUUID& id);
	F32 getCtrlValue(LLSpinCtrl* ctrlp) const;
	void setCtrlValue(LLSpinCtrl* ctrlp, F32 value);

	// Utility method for converting image URI into a texture name.
	std::string getImageNameFromUri(std::string image_uri,
									std::string texture_type);
	// Utility method for building a description of the imported material.
	std::string buildMaterialDescription();

	void resetUnsavedChanges();
	void markChangesUnsaved(U32 dirty_flag);

	// Saves textures to inventory if needed; returns number of scheduled
	// uploads.
	U32 saveTextures();
	bool saveTexture(LLImageJ2C* imagep, U32 tex_type, const std::string& name,
					 const LLUUID& asset_id);

	void setFailedToUploadTexture();

	// Upload and inventory updates callbacks
	static void uploadFailure(void* userdata);
	static void uploadSuccess(const LLUUID& asset_id, const LLSD& response,
							  U32 tex_type, void* userdata);
	static void finishInventoryUpload(const LLUUID& item_id,
									  const LLUUID& new_asset_id,
									  const LLUUID& new_item_id,
									  void* userdata);
	static void finishTaskUpload(const LLUUID& item_id,
								 const LLUUID& new_asset_id,
								 const LLUUID& task_id, void* userdata);
	bool updateInventoryItem(const std::string& buffer,
							 const LLUUID& item_id,
							 const LLUUID& task_id);
	static void createInventoryItem(const std::string& buffer,
									const std::string& name,
									const std::string& desc,
									const LLPermissions& permissions);
	void clearTextures();

	void getGLTFModel(tinygltf::Model& model);

	std::string getEncodedAsset();
	bool decodeAsset(const std::string& buffer);

	void saveIfNeeded();

	static void finishInventoryUpload(LLUUID item_id, LLUUID new_asset_id,
									  LLUUID new_item_id);
	static void finishTaskUpload(LLUUID item_id, LLUUID new_asset_id);

	static LLPreviewMaterial* getInstance(const LLUUID& uuid);

	static void onLoadComplete(const LLUUID& asset_id, LLAssetType::EType type,
							   void* userdata, S32 status, LLExtStat);

	static void onSaveComplete(const LLUUID& asset_uuid, void* user_data,
							   S32 status, LLExtStat);

	bool handleSaveChangesDialog(const LLSD& notification,
								 const LLSD& response);

	void setEnableEditing(bool can_modify);

	void setFromGLTFMaterial(LLGLTFMaterial* matp);
	bool setFromSelection();

	void loadMaterial(const tinygltf::Model& model,
					  const std::string& filename, S32 index);

	// Resolves what type of parameter get dirtied from the UI control that got
	// touched. Used from UI controls callbacks to avoid having to pass more
	// parameters (the dirty flag) to them. HB
	U32 getDirtyFlagFromCtrl(LLUICtrl* ctrlp);

	// Local textures support.
	void subscribeToLocalTexture(U32 dirty_flag, const LLUUID& tracking_id);
	void replaceLocalTexture(const LLUUID& old_id, const LLUUID& new_id);

	// Notifications callback methods
	bool onCancelMsgCallback(const LLSD& notification, const LLSD& response);
	bool onSaveAsMsgCallback(const LLSD& notification, const LLSD& response);
	static bool onSaveObjectsMaterialCB(const LLSD& notification,
										const LLSD& response,
										const LLPermissions& permissions);

	void finishSaveAs(const LLUUID& new_item_id, const std::string& buffer);

	static void saveMaterial(const LLGLTFMaterial* render_matp,
							 const LLLocalGLTFMaterial* local_matp);

	static void onCancelCtrl(LLUICtrl* ctrlp, void* userdata);
	static void onSelectCtrl(LLUICtrl* ctrlp, void* userdata);
	static void onTextureCtrl(LLUICtrl* ctrlp, void* userdata);
	static void onClickCancel(void* userdata);
	static void onClickOK(void* userdata);
	static void onClickSave(void* userdata);
	static void onClickSaveAs(void* userdata);

private:
	LLUUID								mAssetID;

	LLUUID								mBaseColorTextureUploadId;
	LLUUID								mMetallicTextureUploadId;
	LLUUID								mEmissiveTextureUploadId;
	LLUUID								mNormalTextureUploadId;

	// We keep pointers to fetched textures or viewer will remove them if user
	// temporary selects something else with 'apply now'.
	LLPointer<LLViewerFetchedTexture>	mBaseColorFetched;
	LLPointer<LLViewerFetchedTexture>	mNormalFetched;
	LLPointer<LLViewerFetchedTexture>	mMetallicRoughnessFetched;
	LLPointer<LLViewerFetchedTexture>	mEmissiveFetched;

	// J2C versions of packed buffers for uploading
	LLPointer<LLImageJ2C>				mBaseColorJ2C;
	LLPointer<LLImageJ2C>				mNormalJ2C;
	LLPointer<LLImageJ2C>				mMetallicRoughnessJ2C;
	LLPointer<LLImageJ2C>				mEmissiveJ2C;

	// Local textures support
	struct LocalTexConnection
	{
		LLUUID						mTrackingId;
		boost::signals2::connection	mConnection;
	};
	typedef fast_hmap<S32, LocalTexConnection> connection_map_t;
	connection_map_t					mTextureChangesUpdates;

	LLCheckBoxCtrl*						mDoubleSidedCheck;
	LLTextBox*							mUploadFeeText;
	LLTextureCtrl*						mBaseColorTexCtrl;
	LLTextureCtrl*						mMetallicTexCtrl;
	LLTextureCtrl*						mEmissiveTexCtrl;
	LLTextureCtrl*						mNormalTexCtrl;
	LLColorSwatchCtrl*					mBaseColorCtrl;
	LLColorSwatchCtrl*					mEmissiveColorCtrl;
	LLComboBox*							mAlphaModeCombo;
	LLSpinCtrl*     					mTransparencyCtrl;
	LLSpinCtrl*     					mAlphaCutoffCtrl;
	LLSpinCtrl*     					mMetalnessCtrl;
	LLSpinCtrl*     					mRoughnessCtrl;
	LLButton*							mSaveButton;
	LLButton*							mSaveAsButton;
	LLButton*							mCancelButton;

	std::string							mMaterialName;
	std::string							mMaterialNameShort;
	// Last known name of each texture
	std::string							mBaseColorName;
	std::string							mMetallicRoughnessName;
	std::string							mEmissiveName;
	std::string							mNormalName;

	// Flags to indicate individual changed parameters
	U32									mUnsavedChanges;
	// Flags to indicate individual reverted parameters
	U32									mRevertedChanges;

	S32									mUploadingTexturesCount;
	S32									mExpectedUploadCost;

	bool								mIsOverride;
	bool								mCanCopy;
	bool								mCanModify;
	bool								mHasSelection;
	bool								mUploadingTexturesFailure;
};

#endif // LL_LLPREVIEWMATERIAL_H
