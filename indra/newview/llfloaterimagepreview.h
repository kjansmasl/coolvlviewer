/**
 * @file llfloaterimagepreview.h
 * @brief LLFloaterImagePreview class definition
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERIMAGEPREVIEW_H
#define LL_LLFLOATERIMAGEPREVIEW_H

#include "hbfloateruploadasset.h"
#include "lldynamictexture.h"
#include "llquaternion.h"

class LLCheckBoxCtrl;
class LLComboBox;
class LLJoint;
class LLViewerJointMesh;
class LLVOAvatar;
class LLVertexBuffer;
class LLVolume;

class LLImagePreviewSculpted final : public LLViewerDynamicTexture
{
protected:
	~LLImagePreviewSculpted() override = default;

public:
	LLImagePreviewSculpted(S32 width, S32 height);

	S8 getType() const override;

	void setPreviewTarget(LLImageRaw* imagep, F32 distance);
	LL_INLINE void setTexture(U32 name)		{ mTextureName = name; }

	LL_INLINE bool needsRender() override	{ return mNeedsUpdate; }
	bool render() override;
	void refresh();
	void rotate(F32 yaw_radians, F32 pitch_radians);
	void zoom(F32 zoom_amt);
	void pan(F32 right, F32 up);

protected:
	LLPointer<LLVolume>			mVolume;
	LLPointer<LLVertexBuffer>	mVertexBuffer;
	LLVector3					mCameraOffset;
	U32							mTextureName;
	F32							mCameraDistance;
	F32							mCameraYaw;
	F32							mCameraPitch;
	F32							mCameraZoom;
	bool						mNeedsUpdate;
};

class LLImagePreviewAvatar final : public LLViewerDynamicTexture
{
protected:
	LOG_CLASS(LLImagePreviewAvatar);

	~LLImagePreviewAvatar() override;

public:
	LLImagePreviewAvatar(S32 width, S32 height);

	S8 getType() const override;

	void setPreviewTarget(U32 joint_key, const std::string& mesh_name,
						  F32 distance, bool male);
	LL_INLINE void setTexture(U32 name)		{ mTextureName = name; }
	void clearPreviewTexture(const std::string& mesh_name);

	LL_INLINE bool needsRender() override	{ return mNeedsUpdate; }
	bool render() override;
	void refresh();
	void rotate(F32 yaw_radians, F32 pitch_radians);
	void zoom(F32 zoom_amt);
	void pan(F32 right, F32 up);

protected:
	LLJoint*				mTargetJoint;
	LLViewerJointMesh*		mTargetMesh;
	LLPointer<LLVOAvatar>	mDummyAvatar;
	LLVector3				mCameraOffset;
	F32						mCameraDistance;
	F32						mCameraYaw;
	F32						mCameraPitch;
	F32						mCameraZoom;
	U32						mTextureName;
	bool					mNeedsUpdate;
};

class LLFloaterImagePreview final : public HBFloaterUploadAsset
{
public:
	// For uploading an inventory texture asset.
	LLFloaterImagePreview(const std::string& filename);
	// For uploading inventory thumbnail pictures (which are not inventory
	// assets and do not cost any money to upload). 'thumbnail_inv_id' must be
	// UUID for the inventory object to set the thumbnail for. HB
	LLFloaterImagePreview(const std::string& filename,
						  const LLUUID& thumbnail_inv_id );

	~LLFloaterImagePreview() override;

protected:
	// LLFloater overrides
	bool postBuild() override;
	void draw() override;

	// HBFloaterUploadAsset override
	void uploadAsset() override;

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;

	bool loadImage(const std::string& filename);

	static void onPreviewTypeCommit(LLUICtrl*, void*);

private:
	void init();
	void clearAllPreviewTextures();

private:
	LLComboBox*							mClothingCombo;
	LLCheckBoxCtrl*						mTempAssetCheck;
	LLPointer<LLImageRaw>				mRawImagep;
	LLPointer<LLImagePreviewAvatar>		mAvatarPreview;
	LLPointer<LLImagePreviewSculpted>	mSculptedPreview;
	LLPointer<LLViewerTexture> 			mImagep;
	LLUUID								mThumbnailInventoryId;
	LLRect								mPreviewRect;
	LLRectf								mPreviewImageRect;
	S32									mLastMouseX;
	S32									mLastMouseY;
};

#endif  // LL_LLFLOATERIMAGEPREVIEW_H
