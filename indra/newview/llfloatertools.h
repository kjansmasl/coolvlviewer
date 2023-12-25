/**
 * @file llfloatertools.h
 * @brief The edit tools, including move, position, land, etc.
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

#ifndef LL_LLFLOATERTOOLS_H
#define LL_LLFLOATERTOOLS_H

#include "llfloater.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcoord.h"
#include "llparcelselection.h"
#include "llsafehandle.h"

class LLButton;
class LLComboBox;
class LLMediaCtrl;
class LLObjectSelection;
class LLPanelContents;
class LLPanelFace;
class LLPanelLandInfo;
class LLPanelObject;
class LLPanelPermissions;
class LLPanelVolume;
class LLParcelSelection;
class LLSlider;
class LLTabContainer;
class LLTextBox;
class LLTool;

typedef LLSafeHandle<LLObjectSelection> LLObjectSelectionHandle;

class LLFloaterTools final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterTools);

public:
	LLFloaterTools();
	~LLFloaterTools() override;

	bool postBuild() override;

	static void* createPanelPermissions(void* vdata);
	static void* createPanelObject(void* vdata);
	static void* createPanelVolume(void* vdata);
	static void* createPanelFace(void* vdata);
	static void* createPanelContents(void* vdata);
	static void* createPanelContentsInventory(void* vdata);
	static void* createPanelLandInfo(void* vdata);

	void onOpen() override;
	void onClose(bool app_quitting) override;
	bool canClose() override;
	void onFocusReceived() override;
	void draw() override;

	// Call this once per frame to handle visibility, rect location,
	// button highlights, etc.
	void updatePopup(LLCoordGL center, MASK mask);

	void toolsPrecision();

	// When the floater is going away, reset any options that need to be
	// cleared.
	void resetToolState();

	enum EInfoPanel
	{
		PANEL_GENERAL = 0,
		PANEL_OBJECT,
		PANEL_FEATURES,
		PANEL_FACE,
		PANEL_CONTENTS,
		PANEL_COUNT
	};

	void dirty();
	void showPanel(EInfoPanel panel);

	void setStatusText(const std::string& text);
	static void setEditTool(void* data);
	void saveLastTool();

	LL_INLINE void setGridMode(S32 mode)		{ mComboGridMode->setCurrentByIndex(mode); }

	LL_INLINE LLPanelFace* getPanelFace()		{ return mPanelFace; }

	static bool isVisible();

private:
	static void setObjectType(void* data);

	void refresh() override;

	void updatePrevNextBtns();

	void getMediaState();
	void updateMediaSettings();
	bool selectedMediaEditable();

	void updateTreeGrassCombo(bool visible);

	static void onCommitGridMode(LLUICtrl* ctrl, void* data);

	static void commitSelectComponent(LLUICtrl* ctrl, void* data);

	static bool deleteMediaConfirm(const LLSD& notification,
								   const LLSD& response);
	static bool multipleFacesSelectedConfirm(const LLSD& notification,
											 const LLSD& response);

	static void onClickBtnEditMedia(void* data);
	static void onClickBtnAddMedia(void* data);
	static void onClickBtnDeleteMedia(void* data);

	static void onClickGridOptions(void* data);
	static void onClickLink(void* data);
	static void onClickUnlink(void* data);

	static void onSelectTreesGrass(LLUICtrl*, void*);

private:
	LLButton*				mBtnFocus;
	LLButton*				mBtnMove;
	LLButton*				mBtnEdit;
	LLButton*				mBtnCreate;
	LLButton*				mBtnLand;

	LLTextBox*				mTextStatus;

	// Focus buttons
	LLCheckBoxCtrl*			mRadioOrbit;
	LLCheckBoxCtrl*			mRadioZoom;
	LLCheckBoxCtrl*			mRadioPan;
	LLSlider*				mSliderZoom;

	// Move buttons
	LLCheckBoxCtrl*			mRadioMove;
	LLCheckBoxCtrl*			mRadioLift;
	LLCheckBoxCtrl*			mRadioSpin;

	// Edit buttons
	LLCheckBoxCtrl*			mRadioPosition;
	LLCheckBoxCtrl*			mRadioAlign;
	LLCheckBoxCtrl*			mRadioRotate;
	LLCheckBoxCtrl*			mRadioStretch;
	LLCheckBoxCtrl*			mRadioSelectFace;

	LLCheckBoxCtrl*			mCheckSelectIndividual;
	LLButton*				mBtnPrevChild;
	LLButton*				mBtnNextChild;
	LLButton*				mBtnLink;
	LLButton*				mBtnUnlink;

	LLTextBox*				mTextObjectCount;
	LLTextBox*				mTextPrimCount;

	LLButton*				mBtnGridOptions;
	LLTextBox*				mTextGridMode;
	LLComboBox*				mComboGridMode;
	LLCheckBoxCtrl*			mCheckStretchUniform;
	LLCheckBoxCtrl*			mCheckStretchTexture;
	LLCheckBoxCtrl*			mCheckUseRootForPivot;

	LLButton*				mBtnRotateLeft;
	LLButton*				mBtnRotateReset;
	LLButton*				mBtnRotateRight;

	LLButton*				mBtnDelete;
	LLButton*				mBtnDuplicate;
	LLButton*				mBtnDuplicateInPlace;

	// Create buttons
	LLCheckBoxCtrl*			mCheckSticky;
	LLCheckBoxCtrl*			mCheckCopySelection;
	LLCheckBoxCtrl*			mCheckCopyCenters;
	LLCheckBoxCtrl*			mCheckCopyRotates;

	// Land buttons
	LLCheckBoxCtrl*			mRadioSelectLand;
	LLCheckBoxCtrl*			mRadioDozerFlatten;
	LLCheckBoxCtrl*			mRadioDozerRaise;
	LLCheckBoxCtrl*			mRadioDozerLower;
	LLCheckBoxCtrl*			mRadioDozerSmooth;
	LLCheckBoxCtrl*			mRadioDozerNoise;
	LLCheckBoxCtrl*			mRadioDozerRevert;
	LLSlider*				mSliderDozerSize;
	LLSlider*				mSliderDozerForce;
	LLButton*				mBtnApplyToSelection;
	LLTextBox*				mTextBulldozer;
	LLTextBox*				mTextDozerSize;
	LLTextBox*				mTextStrength;

	LLComboBox*				mComboTreesGrass;
	LLTextBox*				mTextTreeGrass;
	LLButton*				mBtnToolTree;
	LLButton*				mBtnToolGrass;

	std::vector<LLButton*>	mButtons;	//[15];

	LLTabContainer*			mTab;
	LLPanelPermissions*		mPanelPermissions;
	LLPanelObject*			mPanelObject;
	LLPanelVolume*			mPanelVolume;
	LLPanelContents*		mPanelContents;
	LLPanelFace*			mPanelFace;
	LLPanelLandInfo*		mPanelLandInfo;

	LLTabContainer*			mTabLand;

	LLButton*				mBtnEditMedia;
	LLButton*				mBtnAddMedia;
	LLButton*				mBtnDeleteMedia;
	LLTextBox*				mTextMediaInfo;
	LLSD					mMediaSettings;

	std::string				mGridScreenText;
	std::string				mGridLocalText;
	std::string				mGridWorldText;
	std::string				mGridReferenceText;
	std::string				mGridAttachmentText;

	typedef std::map<std::string, std::string> status_map_t;
	status_map_t			mStatusText;

	LLParcelSelectionHandle	mParcelSelection;
	LLObjectSelectionHandle	mObjectSelection;
	U32						mPrecision;

	S32						mLastObjectCount;
	S32						mLastPrimCount;
	S32						mLastLandImpact;

	bool					mDirty;
};

class LLFloaterBuildOptions : public LLFloater,
							  public LLFloaterSingleton<LLFloaterBuildOptions>
{
	friend class LLUISingleton<LLFloaterBuildOptions,
							   VisibilityPolicy<LLFloater> >;

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterBuildOptions(const LLSD&);
};

extern LLFloaterTools* gFloaterToolsp;

#endif
