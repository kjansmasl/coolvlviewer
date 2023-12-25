/**
 * @file llfloatersnapshot.h
 * @brief Snapshot preview window, allowing saving, e-mailing, etc.
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

#ifndef LL_LLFLOATERSNAPSHOT_H
#define LL_LLFLOATERSNAPSHOT_H

#include "llfloater.h"

#include "llselectmgr.h"			// For LLAnimPauseRequest
#include "llviewerwindow.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLSnapshotLivePreview;
class LLFlyoutButton;
class LLRadioGroup;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;
class LLToolset;

class LLFloaterSnapshot final : public LLFloater,
								public LLFloaterSingleton<LLFloaterSnapshot>
{
	friend class LLSnapshotLivePreview;
	friend class LLUISingleton<LLFloaterSnapshot,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterSnapshot);

public:
	typedef enum e_snapshot_format : U32
	{
		SNAPSHOT_FORMAT_PNG,
		SNAPSHOT_FORMAT_JPEG,
		SNAPSHOT_FORMAT_BMP
	} ESnapshotFormat;

	bool postBuild() override;
	void draw() override;
	void onClose(bool app_quitting) override;

	// Returns true when temporary texture assets upload is possible and has
	// been choosen by the user of this snapshot. HB
	bool isTempAsset() const;

	static void show(void*);
	static void hide(void*);
	static void update();

	LL_INLINE static S32 getUIWinHeightLong()			{ return sUIWinHeightLong; }
	LL_INLINE static S32 getUIWinHeightShort()			{ return sUIWinHeightShort; }
	LL_INLINE static S32 getUIWinWidth()				{ return sUIWinWidth; }

	// Setup the floater to take a snapshot for a thumbnail of inventory object
	// which UUID is inv_obj_id. HB
	void setupForInventoryThumbnail(const LLUUID& inv_obj_id);

private:
	// Use show() and hide() only !
	LLFloaterSnapshot(const LLSD&);
	~LLFloaterSnapshot() override;

	S32 getTypeIndex();
	U32 getFormatIndex();
	U32 getLayerType();

	void updateControls();
	void updateLayout();

	void comboSetCustom(LLComboBox* combop);

	void checkAspectRatio(S32 index);
	void resetSnapshotSizeOnUI(S32 width, S32 height);

	static void onClickDiscard(void* data);
	static void onClickKeep(void* data);
	static void onCommitSave(LLUICtrl* ctrl, void* data);
	static void onClickNewSnapshot(void* data);
	static void onClickAutoSnap(LLUICtrl* ctrl, void* data);
	static void onClickLess(void* data);
	static void onClickMore(void* data);
	static void onClickUICheck(LLUICtrl* ctrl, void* data);
	static void onClickHUDCheck(LLUICtrl* ctrl, void* data);
	static void onClickKeepAspectCheck(LLUICtrl* ctrl, void* data);
	static void onCommitQuality(LLUICtrl* ctrl, void* data);

	static void onCommitResolution(LLUICtrl* ctrl, void* data)
	{
		updateResolution(ctrl, data);
	}

	static void updateResolution(LLUICtrl* ctrl, void* data,
								 bool do_update = true);
	static void onCommitFreezeFrame(LLUICtrl* ctrl, void* data);
	static void onCommitLayerTypes(LLUICtrl* ctrl, void* data);
	static void onCommitSnapshotType(LLUICtrl* ctrl, void* data);
	static void onCommitSnapshotFormat(LLUICtrl* ctrl, void* data);
	static void onCommitCustomResolution(LLUICtrl* ctrl, void* data);

private:
	LLButton*						mMoreButton;
	LLButton*						mLessButton;
	LLButton*						mUploadButton;
	LLButton*						mSendButton;
	LLTextBox*						mFileSizeLabel;
	LLTextBox*						mTypeLabel;
	LLTextBox*						mFormatLabel;
	LLTextBox*						mLayerLabel;
	LLComboBox*						mPostcardSizeCombo;
	LLComboBox*						mTextureSizeCombo;
	LLComboBox*						mLocalSizeCombo;
	LLComboBox*						mThumbnailSizeCombo;
	LLComboBox*						mLocalFormatCombo;
	LLComboBox*						mLayerTypeCombo;
	LLSpinCtrl*						mImageWidthSpinner;
	LLSpinCtrl*						mImageHeightSpinner;
	LLRadioGroup*					mSnapshotTypeRadio;
	LLSliderCtrl*					mImageQualitySlider;
	LLFlyoutButton*					mSaveButton;
	LLCheckBoxCtrl*					mUICheck;
	LLCheckBoxCtrl*					mHUDCheck;
	LLCheckBoxCtrl*					mAutoCloseCheck;
	LLCheckBoxCtrl*					mKeepAspectCheck;
	LLCheckBoxCtrl*					mAutoSnapCheck;
	LLCheckBoxCtrl*					mFreezeFrameCheck;
	LLCheckBoxCtrl*					mTempCheck;

	LLSnapshotLivePreview*			mLivePreview;
	LLToolset*						mLastToolset;
	std::vector<LLAnimPauseRequest>	mAvatarPauseHandles;

	LLUUID							mInventoryObjectId;

	static S32						sUIWinHeightLong;
	static S32						sUIWinHeightShort;
	static S32						sUIWinWidth;
	static U32						sSavedLastSelectedType;
	static bool		 				sAspectRatioCheckOff;
};

class LLSnapshotFloaterView : public LLFloaterView
{
public:
	LLSnapshotFloaterView(const std::string& name, const LLRect& rect);

	bool handleKey(KEY key, MASK mask, bool called_from_parent) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
};

extern LLSnapshotFloaterView* gSnapshotFloaterViewp;

#endif // LL_LLFLOATERSNAPSHOT_H
