/**
 * @file llpanelobject.h
 * @brief Object editing (position, scale, etc.) in the tools floater
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

#ifndef LL_LLPANELOBJECT_H
#define LL_LLPANELOBJECT_H

#include "llpanel.h"
#include "llpointer.h"
#include "llvolume.h"
#include "llvector3.h"

class LLSpinCtrl;
class LLCheckBoxCtrl;
class LLTextBox;
class LLUICtrl;
class LLButton;
class LLViewerObject;
class LLComboBox;
class LLColorSwatchCtrl;
class LLTextureCtrl;
class LLInventoryItem;
class LLUUID;

class LLPanelObject final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelObject);

public:
	LLPanelObject(const std::string& name);

	bool postBuild() override;
	void draw() override;
	void clearCtrls() override;

	void refresh() override;

private:
	static void onCommitLock(LLUICtrl* ctrl, void* userdata);
	static void onCommitPosition(LLUICtrl* ctrl, void* userdata);
	static void onCommitScale(LLUICtrl* ctrl, void* userdata);
	static void onCommitRotation(LLUICtrl* ctrl, void* userdata);
	static void onCommitTemporary(LLUICtrl* ctrl, void* userdata);
	static void onCommitPhantom(LLUICtrl* ctrl, void* userdata);

	static void onCommitParametric(LLUICtrl* ctrl, void* userdata);

	static void onCommitSculpt(LLUICtrl* ctrl, void* userdata);
	static void onCancelSculpt(LLUICtrl* ctrl, void* userdata);
	static void onSelectSculpt(LLUICtrl* ctrl, void* userdata);
	static bool onDropSculpt(LLUICtrl* ctrl, LLInventoryItem* item, void* ud);
	static void onCommitSculptType(LLUICtrl* ctrl, void* userdata);

	static void onCommitCopyPaste(LLUICtrl* ctrl, void* userdata);
	static void onClickCopy(void* userdata);
	static void onClickPaste(void* userdata);

	void getState();
	void setCopyPasteState();

	void sendRotation(bool btn_down);
	void sendScale(bool btn_down);
	void sendPosition(bool btn_down);
	void sendIsTemporary();
	void sendIsPhantom();
	void sendSculpt();

	void getVolumeParams(LLVolumeParams& volume_params);

private:
	LLTextBox*		mLabelSelectSingle;
	LLTextBox*		mLabelEditObject;

	// Per-object options
	LLTextBox*		mLabelBaseType;
	LLComboBox*		mComboBaseType;

	LLTextBox*		mLabelCut;
	LLSpinCtrl*		mSpinCutBegin;
	LLSpinCtrl*		mSpinCutEnd;

	LLTextBox*		mLabelHollow;
	LLSpinCtrl*		mSpinHollow;
	LLTextBox*		mLabelHollowShape;

	LLComboBox*		mComboHoleType;

	LLTextBox*		mLabelTwist;
	LLSpinCtrl*		mSpinTwist;
	LLSpinCtrl*		mSpinTwistBegin;

	LLTextBox*		mLabelScaleHole;
	LLTextBox*		mLabelScaleTaper;
	LLSpinCtrl*		mSpinScaleX;
	LLSpinCtrl*		mSpinScaleY;

	LLTextBox*		mLabelSkew;
	LLSpinCtrl*		mSpinSkew;

	LLTextBox*		mLabelShear;
	LLSpinCtrl*		mSpinShearX;
	LLSpinCtrl*		mSpinShearY;

	// Advanced Path
	LLTextBox*		mLabelAdvancedCut;
	LLTextBox*		mLabelAdvancedDimple;
	LLTextBox*		mLabelAdvancedSlice;

	LLSpinCtrl*		mCtrlPathBegin;
	LLSpinCtrl*		mCtrlPathEnd;

	LLTextBox*		mLabelTaper;
	LLSpinCtrl*		mSpinTaperX;
	LLSpinCtrl*		mSpinTaperY;

	LLTextBox*		mLabelRadiusOffset;
	LLSpinCtrl*		mSpinRadiusOffset;

	LLTextBox*		mLabelRevolutions;
	LLSpinCtrl*		mSpinRevolutions;

	LLTextBox*		mLabelPosition;
	LLSpinCtrl*		mCtrlPosX;
	LLSpinCtrl*		mCtrlPosY;
	LLSpinCtrl*		mCtrlPosZ;
	LLCheckBoxCtrl*	mCheckCopyPos;

	LLTextBox*		mLabelSize;
	LLSpinCtrl*		mCtrlScaleX;
	LLSpinCtrl*		mCtrlScaleY;
	LLSpinCtrl*		mCtrlScaleZ;
	LLCheckBoxCtrl*	mCheckCopySize;

	LLTextBox*		mLabelRotation;
	LLSpinCtrl*		mCtrlRotX;
	LLSpinCtrl*		mCtrlRotY;
	LLSpinCtrl*		mCtrlRotZ;
	LLCheckBoxCtrl*	mCheckCopyRot;

	LLCheckBoxCtrl*	mCheckLock;
	LLCheckBoxCtrl*	mCheckTemporary;
	LLCheckBoxCtrl*	mCheckPhantom;
	LLCheckBoxCtrl*	mCheckCopyShape;

	LLTextureCtrl*	mCtrlSculptTexture;
	LLTextBox*		mLabelSculptType;
	LLComboBox*		mCtrlSculptType;
	LLCheckBoxCtrl*	mCtrlSculptMirror;
	LLCheckBoxCtrl*	mCtrlSculptInvert;

	LLButton*		mButtonCopy;
	LLButton*		mButtonPaste;

	LLPointer<LLViewerObject> mObject;
	LLPointer<LLViewerObject> mRootObject;

	S32				mSelectedType;			// So we know what selected type we last were

	LLUUID          mSculptTextureRevert;   // so we can revert the sculpt texture on cancel

	LLVector3		mCurEulerDegrees;		// to avoid sending rotation when not changed

	bool			mIsTemporary;			// to avoid sending "temporary" when not changed
	bool			mIsPhantom;				// to avoid sending "phantom" when not changed
	bool			mSizeChanged;

	U8              mSculptTypeRevert;      // so we can revert the sculpt type on cancel

	// Object clipboard data
	static bool				sSavedSizeValid;
	static bool				sSavedPosValid;
	static bool				sSavedRotValid;
	static bool				sSavedShapeValid;

	static LLVector3		sSavedSize;
	static LLVector3		sSavedPos;
	static LLVector3		sSavedRot;
	static LLVolumeParams	sSavedShape;
};

#endif
