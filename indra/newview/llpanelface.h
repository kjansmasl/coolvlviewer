/**
 * @file llpanelface.h
 * @brief Panel in the tools floater for editing face textures, colors, etc.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLPANELFACE_H
#define LL_LLPANELFACE_H

#include "llmaterial.h"
#include "llpanel.h"
#include "llcolor4.h"

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLFace;
class LLInventoryItem;
class LLRadioGroup;
class LLSpinCtrl;
class LLTextBox;
class LLTextureCtrl;
class LLViewerObject;

class LLPanelFace final : public LLPanel
{
public:
	LLPanelFace(const std::string& name);
	~LLPanelFace() override;

	bool postBuild() override;
	void refresh() override;

	// *TODO: not yet implemented in LL's viewer v3...
	void setMediaURL(const std::string& url)		{}
	void setMediaType(const std::string& mime_type)	{}

	LLMaterialPtr	createDefaultMaterial(LLMaterialPtr curmat);

	LL_INLINE LLComboBox* getComboTexGen()			{ return mComboTexGen; }
	LL_INLINE LLSpinCtrl* getTexScaleU()			{ return mTexScaleU; }
	LL_INLINE LLSpinCtrl* getTexScaleV()			{ return mTexScaleV; }
	LL_INLINE LLSpinCtrl* getTexOffsetU()			{ return mTexOffsetU; }
	LL_INLINE LLSpinCtrl* getTexOffsetV()			{ return mTexOffsetV; }
	LL_INLINE LLSpinCtrl* getTexRot()				{ return mTexRot; }
	LL_INLINE LLCheckBoxCtrl* getTexFlipS()			{ return mCheckTexFlipS; }
	LL_INLINE LLCheckBoxCtrl* getTexFlipT()			{ return mCheckTexFlipT; }
	LL_INLINE LLCheckBoxCtrl* getPlanarAlign()		{ return mCheckPlanarAlign; }

	static LLRender::eTexIndex getTextureChannelToEdit();

private:
	LLFace* getLastSelectedFace();

	void getState();

	void sendTexture();		// Applies and sends texture
	void sendTextureInfo();	// Applies and sends texture scale, offset, etc.
	void sendColor();		// Applies and sends color
	void sendAlpha();		// Applies and sends transparency
	void sendBump();		// Applies and sends bump map
	void sendTexGen();		// Applies and sends texture mapping
	void sendShiny();		// Applies and sends shininess
	void sendFullbright();	// Applies and sends full bright
	void sendGlow();		// Applies and sends glow

	void updateMaterial();	// Applies and sends materials
	void removeMaterial();	// Remove materials from selection

	bool canEditSelection();
	void updateAlphaControls();
	void updateBumpyControls();
	void updateShinyControls();

	// This method returns true if the drag should succeed.
	static bool onDragTexture(LLUICtrl* ctrl, LLInventoryItem* item, void* ud);

	static void onSelectMapType(LLUICtrl* ctrl, void* userdata);
	static void onCommitNormalMap(LLUICtrl* ctrl, void* userdata);
	static void onCommitSpecularMap(LLUICtrl* ctrl, void* userdata);
	static void onCommitAlphaMaterial(LLUICtrl* ctrl, void* userdata);
	static void onCommitBumpyMaterial(LLUICtrl* ctrl, void* userdata);
	static void onCommitShinyMaterial(LLUICtrl* ctrl, void* userdata);
	static void onCommitShinyColor(LLUICtrl* ctrl, void* userdata);
	static void onCommitAlphaMode(LLUICtrl* ctrl, void* userdata);
	static void onCommitTexture(LLUICtrl* ctrl, void* userdata);
	static void onCancelTexture(LLUICtrl* ctrl, void* userdata);
	static void onSelectTexture(LLUICtrl* ctrl, void* userdata);
	static void onCommitTextureInfo(LLUICtrl* ctrl, void* userdata);
	static void onCommitRepeatsPerMeter(LLUICtrl* ctrl, void* userdata);
	static void onCommitColor(LLUICtrl* ctrl, void* userdata);
	static void onCommitAlpha(LLUICtrl* ctrl, void* userdata);
	static void onCancelColor(LLUICtrl* ctrl, void* userdata);
	static void onSelectColor(LLUICtrl* ctrl, void* userdata);
	static void onCommitBump(LLUICtrl* ctrl, void* userdata);
	static void onCommitTexGen(LLUICtrl* ctrl, void* userdata);
	static void onCommitShiny(LLUICtrl* ctrl, void* userdata);
	static void onCommitFullbright(LLUICtrl* ctrl, void* userdata);
	static void onCommitGlow(LLUICtrl* ctrl, void *userdata);
	static void onCommitPlanarAlign(LLUICtrl* ctrl, void* userdata);
	static void onCommitPbrMaterial(LLUICtrl* ctrl, void* userdata);

	static void onClickEditPBR(void*);
	static void onClickLocalPBR(void* userdata);
	static void onClickLoadPBR(void* userdata);
	static void onClickSavePBR(void*);

	static void onClickRemoveMaterial(void* userdata);
	static void onClickAutoFix(void*);
#if 0
	static void onAlignTextureLayers(void* userdata);
#endif

	static F32 valueGlow(LLViewerObject* object, S32 face);

	static void onSelectLocalPBR(const LLUUID& id, void*);
	static void onSelectInventoryPBR(const std::vector<std::string>&,
									 const uuid_vec_t& ids, void*, bool);

private:
	LLColorSwatchCtrl*	mColorSwatch;
	LLColorSwatchCtrl*	mShinyColorSwatch;

	LLRadioGroup*		mMapsRadio;

	LLTextureCtrl*		mTextureCtrl;
	LLTextureCtrl*		mNormalCtrl;
	LLTextureCtrl*		mSpecularCtrl;

	LLComboBox*			mComboAlphaMode;
	LLComboBox*			mComboBumpiness;
	LLComboBox*			mComboShininess;
	LLComboBox*			mComboTexGen;

	LLCheckBoxCtrl*		mCheckFullbright;
	LLCheckBoxCtrl*		mCheckPlanarAlign;
	LLCheckBoxCtrl*		mCheckTexFlipS;
	LLCheckBoxCtrl*		mCheckTexFlipT;
	LLCheckBoxCtrl*		mCheckBumpyFlipS;
	LLCheckBoxCtrl*		mCheckBumpyFlipT;
	LLCheckBoxCtrl*		mCheckShinyFlipS;
	LLCheckBoxCtrl*		mCheckShinyFlipT;

	LLTextBox*			mLabelDiffuseColor;
	LLTextBox*			mLabelMaps;
	LLTextBox*			mLabelAlphaMode;
	LLTextBox*			mLabelMaskCutoff;
	LLTextBox*			mLabelShinyColor;
	LLTextBox*			mLabelGlossiness;
	LLTextBox*			mLabelEnvironment;
	LLTextBox*			mLabelShininess;
	LLTextBox*			mLabelBumpiness;
	LLTextBox*			mLabelColorTransp;
	LLTextBox*			mLabelRepeats;
	LLTextBox*			mLabelTexScale;
	LLTextBox*			mLabelTexScaleUnit;
	LLTextBox*			mLabelTexScaleHoriz;
	LLTextBox*			mLabelTexScaleVert;
	LLTextBox*			mLabelTexOffset;
	LLTextBox*			mLabelTexOffsetHoriz;
	LLTextBox*			mLabelTexOffsetVert;
	LLTextBox*			mLabelTexRotate;
	LLTextBox*			mLabelTexGen;
	LLTextBox*			mLabelMedia;

	LLSpinCtrl*			mTransparency;	// Transparency = 1 - alpha
	LLSpinCtrl*     	mGlow;
	LLSpinCtrl*     	mTexScaleU;
	LLSpinCtrl*     	mTexScaleV;
	LLSpinCtrl*     	mTexOffsetU;
	LLSpinCtrl*     	mTexOffsetV;
	LLSpinCtrl*     	mTexRot;
	LLSpinCtrl*     	mGlossiness;
	LLSpinCtrl*     	mEnvironment;
	LLSpinCtrl*     	mMaskCutoff;
	LLSpinCtrl*     	mBumpyScaleU;
	LLSpinCtrl*     	mBumpyScaleV;
	LLSpinCtrl*     	mBumpyOffsetU;
	LLSpinCtrl*     	mBumpyOffsetV;
	LLSpinCtrl*     	mBumpyRot;
	LLSpinCtrl*     	mShinyScaleU;
	LLSpinCtrl*     	mShinyScaleV;
	LLSpinCtrl*     	mShinyOffsetU;
	LLSpinCtrl*     	mShinyOffsetV;
	LLSpinCtrl*     	mShinyRot;
	LLSpinCtrl*     	mPbrScaleU;
	LLSpinCtrl*     	mPbrScaleV;
	LLSpinCtrl*     	mPbrOffsetU;
	LLSpinCtrl*     	mPbrOffsetV;
	LLSpinCtrl*     	mPbrRot;
	LLSpinCtrl*     	mRepeats;

	LLButton*			mButtonResetMaterial;
	LLButton*			mButtonEditPBR;
	LLButton*			mButtonLocalPBR;
	LLButton*			mButtonLoadPBR;
	LLButton*			mButtonSavePBR;
#if 0
	LLButton*			mButtonAlignMap;
#endif
	LLButton*			mButtonAlignMedia;

	std::string			mRepeatsPerMeterText;
	std::string			mRepeatsPerFaceText;
	std::string			mUseTextureText;

	bool				mIsAlpha;

	static LLPanelFace*	sInstance;
};

#endif
