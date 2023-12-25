/**
 * @file llpreviewtexture.h
 * @brief LLPreviewTexture class definition
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

#ifndef LL_LLPREVIEWTEXTURE_H
#define LL_LLPREVIEWTEXTURE_H

#include "llbutton.h"
#include "hbfileselector.h"
#include "llframetimer.h"

#include "llpreview.h"
#include "llviewertexture.h"

class LLImageRaw;

class LLPreviewTexture final : public LLPreview
{
public:
	LLPreviewTexture(const std::string& name,
					 const LLRect& rect,
					 const std::string& title,
					 const LLUUID& item_uuid,
					 const LLUUID& object_id,
					 bool show_keep_discard = false);
	LLPreviewTexture(const std::string& name,
					 const LLRect& rect,
					 const std::string& title,
					 const LLUUID& asset_id,
					 bool copy_to_inv = false);
	~LLPreviewTexture();

	void draw() override;

	bool canSaveAs() const override;
	void saveAs() override;

	void loadAsset() override;
	EAssetStatus getAssetStatus() override;

	LL_INLINE void setNotCopyable()						{ mIsCopyable = false; }

	static void saveAsCallback(HBFileSelector::ESaveFilter type,
							   std::string& filename, void* user_data);

	static void onFileLoadedForSave(bool success,
									LLViewerFetchedTexture* src_vi,
									LLImageRaw* src,
									LLImageRaw* aux_src,
									S32 discard_level,
									bool is_final,
									void* userdata);

	LL_INLINE static S32 getPreviewCount()				{ return sList.size(); }

protected:
	void init();
	bool setAspectRatio(F32 width, F32 height);
	static void onAspectRatioCommit(LLUICtrl*, void* userdata);
	static void onRefreshBtn(void* data);

	LL_INLINE const char* getTitleName() const override	{ return "Texture"; }

private:
	void updateDimensions();

private:
	LLPointer<LLViewerFetchedTexture> mImage;

	LLFrameTimer		mSavedFileTimer;

	std::string			mSaveFileName;

	LLUUID				mImageID;

	uuid_list_t			mCallbackTextureList;

	S32                 mImageOldBoostLevel;

	S32					mLastHeight;
	S32					mLastWidth;
	F32					mAspectRatio;	// 0 = Unconstrained

	bool                mShowKeepDiscard;
	bool                mCopyToInv;
	bool				mLoadingFullImage;

	// This is stored off in a member variable, because the save-as
	// button and drag and drop functionality need to know.
	bool				mIsCopyable;

	static std::set<LLPreviewTexture*> sList;
};

#endif  // LL_LLPREVIEWTEXTURE_H
