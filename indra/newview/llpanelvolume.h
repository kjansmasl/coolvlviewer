/**
 * @file llpanelvolume.h
 * @brief Object editing (position, scale, etc.) in the tools floater
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

#ifndef LL_LLPANELVOLUME_H
#define LL_LLPANELVOLUME_H

#include "llpanel.h"
#include "llpointer.h"
#include "llvolume.h"
#include "llvector3.h"

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLInventoryItem;
class LLSpinCtrl;
class LLTextBox;
class LLTextureCtrl;
class LLUICtrl;
class LLViewerObject;

class LLPanelVolume final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelVolume);

public:
	LLPanelVolume(const std::string& name);
	~LLPanelVolume() override;

	bool postBuild() override;
	void refresh() override;
	void clearCtrls() override;

	void sendIsPhysical();
	void sendIsLight();
	void sendIsFlexible();

private:
	void getState();
	void refreshCost();

	static void onCommitPhysics(LLUICtrl* ctrl, void* userdata);
	static void onCommitIsLight(LLUICtrl* ctrl, void* userdata);
	static void onCommitLight(LLUICtrl* ctrl, void* userdata);
	static void onCommitIsFlexible(LLUICtrl* ctrl, void* userdata);
	static void onCommitFlexible(LLUICtrl* ctrl, void* userdata);
	static void onCommitMaterial(LLUICtrl* ctrl, void* userdata);

	static void onLightCancelColor(LLUICtrl* ctrl, void* userdata);
	static void onLightSelectColor(LLUICtrl* ctrl, void* userdata);

	static void onLightCancelTexture(LLUICtrl* ctrl, void* userdata);
	static void onLightSelectTexture(LLUICtrl* ctrl, void* userdata);
	static bool onDragTexture(LLUICtrl* ctrl, LLInventoryItem* item,
							  void* userdata);

	static void sendPhysicsShapeType(LLUICtrl* ctrl, void* userdata);
	static void sendPhysicsGravity(LLUICtrl* ctrl, void* userdata);
	static void sendPhysicsFriction(LLUICtrl* ctrl, void* userdata);
	static void sendPhysicsRestitution(LLUICtrl* ctrl, void* userdata);
	static void sendPhysicsDensity(LLUICtrl* ctrl, void* userdata);

	static void onCommitAnimatedMesh(LLUICtrl* ctrl, void* userdata);

	static void onClickProbe(void* userdata);

private:
	// Common UI elements
	LLTextBox*					mLabelSelectSingle;
	LLTextBox*					mLabelEditObject;

	// Flexible UI elements
	LLCheckBoxCtrl*				mCheckFlexiblePath;
	LLSpinCtrl*					mSpinFlexSections;
	LLSpinCtrl*					mSpinFlexGravity;
	LLSpinCtrl*					mSpinFlexFriction;
	LLSpinCtrl*					mSpinFlexWind;
	LLSpinCtrl*					mSpinFlexTension;
	LLSpinCtrl*					mSpinFlexForceX;
	LLSpinCtrl*					mSpinFlexForceY;
	LLSpinCtrl*					mSpinFlexForceZ;

	// Physics UI elements
	LLTextBox*					mLabelPhysicsShape;
	LLCheckBoxCtrl*				mCheckPhysics;
	LLComboBox*					mComboPhysicsShape;
	LLSpinCtrl*					mSpinPhysicsGravity;
	LLSpinCtrl*					mSpinPhysicsFriction;
	LLSpinCtrl*					mSpinPhysicsDensity;
	LLSpinCtrl*					mSpinPhysicsRestitution;

	// Material UI elements
	LLTextBox*					mLabelMaterial;
	LLComboBox*					mComboMaterial;

	std::string					mFullBright;

	// Light UI elements
	LLCheckBoxCtrl*				mCheckEmitLight;
	LLColorSwatchCtrl*			mSwatchLightColor;
	LLTextureCtrl*				mTextureLight;
	LLSpinCtrl*					mSpinLightIntensity;
	LLSpinCtrl*					mSpinLightRadius;
	LLSpinCtrl*					mSpinLightFalloff;
	LLSpinCtrl*					mSpinLightFOV;
	LLSpinCtrl*					mSpinLightFocus;
	LLSpinCtrl*					mSpinLightAmbiance;

	// Animated mesh/puppet element
	LLCheckBoxCtrl*				mCheckAnimatedMesh;

	// Reflection probe
	LLButton*					mReflectionProbe;

	std::string					mPhysicsNone;
	std::string					mPhysicsPrim;
	std::string					mPhysicsHull;

	S32							mComboMaterialItemCount;
	LLColor4					mLightSavedColor;
	LLUUID						mLightSavedTexture;
	LLPointer<LLViewerObject>	mObject;
	LLPointer<LLViewerObject>	mRootObject;

	// To avoid sending "physical" when not changed
	bool						mIsPhysical;
};

#endif
