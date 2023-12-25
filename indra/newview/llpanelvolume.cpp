/**
 * @file llpanelvolume.cpp
 * @brief Object editing (position, scale, etc.) in the tools floater
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc, (c)2009-2023 Henri Beauchamp
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

#include "llpanelvolume.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lleconomy.h"
#include "llnotifications.h"
#include "llpermissionsflags.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llvolume.h"

#include "llagent.h"
#include "llcolorswatch.h"
#include "lldrawpool.h"
#include "llflexibleobject.h"
#include "llmanipscale.h"
#include "llmeshrepository.h"
#include "llpipeline.h"
#include "llpreviewscript.h"
#include "llselectmgr.h"
#include "lltexturectrl.h"
#include "lltooldraganddrop.h"
#include "lltool.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llvovolume.h"
#include "llworld.h"

constexpr F32 DEFAULT_GRAVITY_MULTIPLIER = 1.f;
constexpr F32 DEFAULT_DENSITY = 1000.f;

///////////////////////////////////////////////////////////////////////////////
// HBReflectionProbe class
///////////////////////////////////////////////////////////////////////////////

// Floater for adjusting reflection probe parameters. Using a separate floater
// for this rarely used feature avoids cluttering excessively the already
// crowded "Features" tab, without having to enlarge the Build tools floater to
// fit the new controls. HB

class HBReflectionProbe final : public LLFloater
{
protected:
	LOG_CLASS(HBReflectionProbe);

public:
	static void show(LLView* ownerp);
	static void hide();
	static void update();

private:
	HBReflectionProbe(LLView* ownerp);		// Use show()
	~HBReflectionProbe() override;			// Use hide()

	bool postBuild() override;
	void draw() override;
	void refresh() override;

	LLVOVolume* getEditedVolume();

	static void onProbeCheck(LLUICtrl*, void* userdata);
	static void onCommitProbe(LLUICtrl* ctrlp, void* userdata);

private:
	LLFloater*					mParentFloater;
	LLCheckBoxCtrl*				mProbeCheck;
	LLCheckBoxCtrl*				mDynamicCheck;
	LLComboBox*					mVolumeCombo;
	LLSpinCtrl*					mAmbianceSpin;
	LLSpinCtrl*					mNearClipSpin;

	bool						mMustClose;

	static HBReflectionProbe*	sInstance;
};

//static
HBReflectionProbe* HBReflectionProbe::sInstance = NULL;

//static
void HBReflectionProbe::show(LLView* ownerp)
{
	if (!sInstance)
	{
		sInstance = new HBReflectionProbe(ownerp);
	}
	sInstance->open();
	sInstance->setFocus(true);
}

//static
void HBReflectionProbe::hide()
{
	if (sInstance)
	{
		sInstance->close();
	}
}

//static
void HBReflectionProbe::update()
{
	if (sInstance)
	{
		sInstance->refresh();
	}
}

HBReflectionProbe::HBReflectionProbe(LLView* ownerp)
:	mParentFloater(NULL),
	mMustClose(false)
{
	llassert_always(!sInstance);
	sInstance = this;

	LLUICtrlFactory* factoryp = LLUICtrlFactory::getInstance();
	factoryp->buildFloater(this, "floater_reflection_probe.xml");
	// Note: at this point postBuild() has been called and returned.
	LLView* parentp = ownerp;
	// Search for our owner's parent floater and register as dependent of
	// it if found.
	while (parentp)
	{
		LLFloater* floaterp = parentp->asFloater();
		if (floaterp)
		{
			mParentFloater = floaterp;
			floaterp->addDependentFloater(this);
			break;
		}
		parentp = parentp->getParent();
	}
}

//virtual
HBReflectionProbe::~HBReflectionProbe()
{
	sInstance = NULL;
}

//virtual
bool HBReflectionProbe::postBuild()
{
	mProbeCheck = getChild<LLCheckBoxCtrl>("probe_check");
	mProbeCheck->setCommitCallback(onProbeCheck);
	mProbeCheck->setCallbackUserData(this);

	mDynamicCheck = getChild<LLCheckBoxCtrl>("dynamic_check");
	mDynamicCheck->setCommitCallback(onCommitProbe);
	mDynamicCheck->setCallbackUserData(this);

	mVolumeCombo = getChild<LLComboBox>("volume_combo");
	mVolumeCombo->setCommitCallback(onCommitProbe);
	mVolumeCombo->setCallbackUserData(this);

	mAmbianceSpin = getChild<LLSpinCtrl>("ambiance_ctrl");
	mAmbianceSpin->setCommitCallback(onCommitProbe);
	mAmbianceSpin->setCallbackUserData(this);

	mNearClipSpin = getChild<LLSpinCtrl>("near_clip_ctrl");
	mNearClipSpin->setCommitCallback(onCommitProbe);
	mNearClipSpin->setCallbackUserData(this);

	return true;
}

LLVOVolume* HBReflectionProbe::getEditedVolume()
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->getObjectCount() != 1 || !gAgent.hasInventoryMaterial())
	{
		mMustClose = true;
		return NULL;
	}
		
	LLViewerObject* objectp = selection->getFirstRootObject();
	LLVOVolume* vovolp = objectp->asVolume();
	if (!vovolp || vovolp->getPCode() != LL_PCODE_VOLUME || vovolp->isMesh())
	{
		mMustClose = true;
		return NULL;
	}
	return vovolp;
}

//virtual
void HBReflectionProbe::draw()
{
	if (mMustClose || (mParentFloater && !mParentFloater->getVisible()))
	{
		close();
		return;
	}
	LLFloater::draw();
}

//virtual
void HBReflectionProbe::refresh()
{
	LLVOVolume* volobjp = getEditedVolume();
	if (!volobjp)
	{
		return;
	}

	bool is_probe = volobjp->isReflectionProbe();
	mProbeCheck->set(is_probe);
	bool enabled = volobjp->permModify() && !volobjp->isPermanentEnforced();
	mProbeCheck->setEnabled(enabled);

	enabled &= is_probe;	// Other controls enabled when actually a probe
	mDynamicCheck->setEnabled(enabled);
	mVolumeCombo->setEnabled(enabled);
	mAmbianceSpin->setEnabled(enabled);
	mNearClipSpin->setEnabled(enabled);
	if (enabled)
	{
		mDynamicCheck->set(volobjp->getReflectionProbeIsDynamic());
		bool is_box = volobjp->getReflectionProbeIsBox();
		mVolumeCombo->setValue(is_box ? "Box" : "Sphere");
		mAmbianceSpin->setValue(volobjp->getReflectionProbeAmbiance());
		mNearClipSpin->setValue(volobjp->getReflectionProbeNearClip());
	}
	else
	{
		mDynamicCheck->clear();
		mVolumeCombo->clear();
		mAmbianceSpin->clear();
		mNearClipSpin->clear();
	}
}

//static
void HBReflectionProbe::onProbeCheck(LLUICtrl*, void* userdata)
{
	HBReflectionProbe* self = (HBReflectionProbe*)userdata;
	if (!self)
	{
		return;
	}

	LLVOVolume* volobjp = self->getEditedVolume();
	if (!volobjp)
	{
		return;
	}

	bool old_value = volobjp->isReflectionProbe();
	bool set_probe = self->mProbeCheck->get();
	volobjp->setIsReflectionProbe(set_probe);
	if (set_probe && set_probe != old_value)
	{
		gSelectMgr.selectionUpdatePhantom(true);
		gSelectMgr.selectionSetGLTFMaterial(LLUUID::null);
		gSelectMgr.selectionSetAlphaOnly(0.f);

		LLVolumeParams params;
		params.getPathParams().setCurveType(LL_PCODE_PATH_CIRCLE);
		params.getProfileParams().setCurveType(LL_PCODE_PROFILE_CIRCLE_HALF);
		volobjp->updateVolume(params);
	}

	self->refresh();
}

//static
void HBReflectionProbe::onCommitProbe(LLUICtrl* ctrlp, void* userdata)
{
	HBReflectionProbe* self = (HBReflectionProbe*)userdata;
	if (!self)
	{
		return;
	}

	LLVOVolume* volobjp = self->getEditedVolume();
	if (!volobjp)
	{
		return;
	}

	F32 ambiance = self->mAmbianceSpin->getValue().asReal();
	F32 nearclip = self->mNearClipSpin->getValue().asReal();
	bool dynamic = self->mDynamicCheck->get();
	bool is_box = self->mVolumeCombo->getValue().asString() == "Box";

	volobjp->setReflectionProbeAmbiance(ambiance);
	volobjp->setReflectionProbeNearClip(nearclip);
	volobjp->setReflectionProbeIsDynamic(dynamic);
	if (volobjp->setReflectionProbeIsBox(is_box))
	{
		// Make the volume match the probe
		gSelectMgr.selectionUpdatePhantom(true);
		gSelectMgr.selectionSetGLTFMaterial(LLUUID::null);
		gSelectMgr.selectionSetAlphaOnly(0.f);

		U8 profile, path;
		if (is_box)
		{
			profile = LL_PCODE_PROFILE_SQUARE;
			path = LL_PCODE_PATH_LINE;
		}
		else
		{
			profile = LL_PCODE_PROFILE_CIRCLE_HALF;
			path = LL_PCODE_PATH_CIRCLE;
			F32 scale = volobjp->getScale().mV[0];
			volobjp->setScale(LLVector3(scale, scale, scale), false);
			gSelectMgr.sendMultipleUpdate(UPD_ROTATION | UPD_POSITION | UPD_SCALE);
		}
		LLVolumeParams params;
		params.getPathParams().setCurveType(profile);
		params.getProfileParams().setCurveType(path);
		volobjp->updateVolume(params);
	}

	self->refresh();
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelVolume class
///////////////////////////////////////////////////////////////////////////////

// "Features" Tab

LLPanelVolume::LLPanelVolume(const std::string& name)
:	LLPanel(name),
	mIsPhysical(false),
	mComboMaterialItemCount(0)
{
	setMouseOpaque(false);
}

LLPanelVolume::~LLPanelVolume()
{
	HBReflectionProbe::hide();
}

bool LLPanelVolume::postBuild()
{
	mLabelSelectSingle = getChild<LLTextBox>("select_single");
	mLabelEditObject = getChild<LLTextBox>("edit_object");

	// Flexible objects parameters

	mCheckFlexiblePath = getChild<LLCheckBoxCtrl>("Flexible1D Checkbox Ctrl");
	mCheckFlexiblePath->setCommitCallback(onCommitIsFlexible);
	mCheckFlexiblePath->setCallbackUserData(this);

	mSpinFlexSections = getChild<LLSpinCtrl>("FlexNumSections");
	mSpinFlexSections->setCommitCallback(onCommitFlexible);
	mSpinFlexSections->setCallbackUserData(this);

	mSpinFlexGravity = getChild<LLSpinCtrl>("FlexGravity");
	mSpinFlexGravity->setCommitCallback(onCommitFlexible);
	mSpinFlexGravity->setCallbackUserData(this);

	mSpinFlexFriction = getChild<LLSpinCtrl>("FlexFriction");
	mSpinFlexFriction->setCommitCallback(onCommitFlexible);
	mSpinFlexFriction->setCallbackUserData(this);

	mSpinFlexWind = getChild<LLSpinCtrl>("FlexWind");
	mSpinFlexWind->setCommitCallback(onCommitFlexible);
	mSpinFlexWind->setCallbackUserData(this);

	mSpinFlexTension = getChild<LLSpinCtrl>("FlexTension");
	mSpinFlexTension->setCommitCallback(onCommitFlexible);
	mSpinFlexTension->setCallbackUserData(this);

	mSpinFlexForceX = getChild<LLSpinCtrl>("FlexForceX");
	mSpinFlexForceX->setCommitCallback(onCommitFlexible);
	mSpinFlexForceX->setCallbackUserData(this);

	mSpinFlexForceY = getChild<LLSpinCtrl>("FlexForceY");
	mSpinFlexForceY->setCommitCallback(onCommitFlexible);
	mSpinFlexForceY->setCallbackUserData(this);

	mSpinFlexForceZ = getChild<LLSpinCtrl>("FlexForceZ");
	mSpinFlexForceZ->setCommitCallback(onCommitFlexible);
	mSpinFlexForceZ->setCallbackUserData(this);

	// Light parameters

	mCheckEmitLight = getChild<LLCheckBoxCtrl>("Light Checkbox Ctrl");
	mCheckEmitLight->setCommitCallback(onCommitIsLight);
	mCheckEmitLight->setCallbackUserData(this);

	mSwatchLightColor = getChild<LLColorSwatchCtrl>("colorswatch");
	mSwatchLightColor->setOnCancelCallback(onLightCancelColor);
	mSwatchLightColor->setOnSelectCallback(onLightSelectColor);
	mSwatchLightColor->setCommitCallback(onCommitLight);
	mSwatchLightColor->setCallbackUserData(this);

	mTextureLight = getChild<LLTextureCtrl>("light texture control");
	mTextureLight->setCommitCallback(onCommitLight);
	mTextureLight->setCallbackUserData(this);
	mTextureLight->setOnCancelCallback(onLightCancelTexture);
	mTextureLight->setOnSelectCallback(onLightSelectTexture);
	mTextureLight->setDragCallback(onDragTexture);

	mSpinLightIntensity = getChild<LLSpinCtrl>("Light Intensity");
	mSpinLightIntensity->setCommitCallback(onCommitLight);
	mSpinLightIntensity->setCallbackUserData(this);

	mSpinLightRadius = getChild<LLSpinCtrl>("Light Radius");
	mSpinLightRadius->setCommitCallback(onCommitLight);
	mSpinLightRadius->setCallbackUserData(this);

	mSpinLightFalloff = getChild<LLSpinCtrl>("Light Falloff");
	mSpinLightFalloff->setCommitCallback(onCommitLight);
	mSpinLightFalloff->setCallbackUserData(this);

	mSpinLightFOV = getChild<LLSpinCtrl>("Light FOV");
	mSpinLightFOV->setCommitCallback(onCommitLight);
	mSpinLightFOV->setCallbackUserData(this);

	mSpinLightFocus = getChild<LLSpinCtrl>("Light Focus");
	mSpinLightFocus->setCommitCallback(onCommitLight);
	mSpinLightFocus->setCallbackUserData(this);

	mSpinLightAmbiance = getChild<LLSpinCtrl>("Light Ambiance");
	mSpinLightAmbiance->setCommitCallback(onCommitLight);
	mSpinLightAmbiance->setCallbackUserData(this);

	// Physics parameters

	mLabelPhysicsShape = getChild<LLTextBox>("label physicsshapetype");

	// Physical checkbox
	mCheckPhysics = getChild<LLCheckBoxCtrl>("Physical Checkbox Ctrl");
	mCheckPhysics->setCommitCallback(onCommitPhysics);
	mCheckPhysics->setCallbackUserData(this);

	mComboPhysicsShape = getChild<LLComboBox>("Physics Shape Type Combo Ctrl");
	mComboPhysicsShape->setCommitCallback(sendPhysicsShapeType);
	mComboPhysicsShape->setCallbackUserData(this);

	mSpinPhysicsGravity = getChild<LLSpinCtrl>("Physics Gravity");
	mSpinPhysicsGravity->setCommitCallback(sendPhysicsGravity);
	mSpinPhysicsGravity->setCallbackUserData(this);

	mSpinPhysicsFriction = getChild<LLSpinCtrl>("Physics Friction");
	mSpinPhysicsFriction->setCommitCallback(sendPhysicsFriction);
	mSpinPhysicsFriction->setCallbackUserData(this);

	mSpinPhysicsDensity = getChild<LLSpinCtrl>("Physics Density");
	mSpinPhysicsDensity->setCommitCallback(sendPhysicsDensity);
	mSpinPhysicsDensity->setCallbackUserData(this);

	mSpinPhysicsRestitution = getChild<LLSpinCtrl>("Physics Restitution");
	mSpinPhysicsRestitution->setCommitCallback(sendPhysicsRestitution);
	mSpinPhysicsRestitution->setCallbackUserData(this);

	mPhysicsNone = getString("None");
	mPhysicsPrim = getString("Prim");
	mPhysicsHull = getString("Convex Hull");

	// Material parameters

	mFullBright = LLTrans::getString("Fullbright");

	std::map<std::string, std::string> material_name_map;
	material_name_map["Stone"]= LLTrans::getString("Stone");
	material_name_map["Metal"]= LLTrans::getString("Metal");
	material_name_map["Glass"]= LLTrans::getString("Glass");
	material_name_map["Wood"]= LLTrans::getString("Wood");
	material_name_map["Flesh"]= LLTrans::getString("Flesh");
	material_name_map["Plastic"]= LLTrans::getString("Plastic");
	material_name_map["Rubber"]= LLTrans::getString("Rubber");
	material_name_map["Light"]= LLTrans::getString("Light");
	gMaterialTable.initTableTransNames(material_name_map);

	mLabelMaterial = getChild<LLTextBox>("label material");
	mComboMaterial = getChild<LLComboBox>("material");
	childSetCommitCallback("material", onCommitMaterial, this);
	mComboMaterial->removeall();

	for (LLMaterialTable::info_list_t::const_iterator
			iter = gMaterialTable.mMaterialInfoList.begin(),
			end = gMaterialTable.mMaterialInfoList.end();
		 iter != end; ++iter)
	{
		const LLMaterialInfo& minfo = *iter;
		if (minfo.mMCode != LL_MCODE_LIGHT)
		{
			mComboMaterial->add(minfo.mName);
		}
	}
	mComboMaterialItemCount = mComboMaterial->getItemCount();

	// Animated mesh/puppet parameter
	mCheckAnimatedMesh = getChild<LLCheckBoxCtrl>("AniMesh Checkbox Ctrl");
	mCheckAnimatedMesh->setCommitCallback(onCommitAnimatedMesh);
	mCheckAnimatedMesh->setCallbackUserData(this);

	// Reflection probe
	mReflectionProbe = getChild<LLButton>("reflection_probe");
	mReflectionProbe->setClickedCallback(onClickProbe, this);

	// Start with everyone disabled
	clearCtrls();

	return true;
}

void LLPanelVolume::getState()
{
	HBReflectionProbe::update();

	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	LLViewerObject* objectp = selection->getFirstRootObject();
	LLViewerObject* root_objectp = objectp;
	if (!objectp)
	{
		objectp = selection->getFirstObject();
		// *FIX: should not we just keep the child ?
		if (objectp)
		{
			LLViewerObject* parentp = objectp->getRootEdit();
			root_objectp = parentp ? parentp : objectp;
		}
	}

	LLVOVolume* volobjp = NULL;
	if (objectp && objectp->getPCode() == LL_PCODE_VOLUME)
	{
		volobjp = (LLVOVolume*)objectp;
	}

	LLVOVolume* root_volobjp = NULL;
	if (root_objectp && root_objectp->getPCode() == LL_PCODE_VOLUME)
	{
		root_volobjp = (LLVOVolume*)root_objectp;
	}

	if (!objectp)
	{
		// Forfeit focus
		if (gFocusMgr.childHasKeyboardFocus(this))
		{
			gFocusMgr.setKeyboardFocus(NULL);
		}

		// Disable all text input fields
		clearCtrls();

		return;
	}

	LLUUID owner_id;
	std::string owner_name;
	gSelectMgr.selectGetOwner(owner_id, owner_name);

	// BUG ?  Check for all objects being editable ?
	bool editable = root_objectp->permModify() &&
					!root_objectp->isPermanentEnforced();
	bool visible_params = (editable || gAgent.isGodlikeWithoutAdminMenuFakery());
	bool single_volume = gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME) &&
						 selection->getObjectCount() == 1;
	bool single_root_volume = gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME) &&
							  gSelectMgr.getSelection()->getRootObjectCount() == 1;

	// Select Single Message
	if (single_volume)
	{
		mLabelSelectSingle->setVisible(false);
		mLabelEditObject->setVisible(true);
		mLabelEditObject->setEnabled(true);
	}
	else
	{
		mLabelSelectSingle->setVisible(true);
		mLabelSelectSingle->setEnabled(true);
		mLabelEditObject->setVisible(false);
	}

	// Light properties
	bool is_light = volobjp && volobjp->getIsLight();
	mCheckEmitLight->setValue(is_light);
	mCheckEmitLight->setEnabled(editable && single_volume && volobjp);

	if (is_light && single_volume && visible_params)
	{
		mLightSavedColor = volobjp->getLightSRGBBaseColor();

		mSwatchLightColor->setEnabled(editable);
		mSwatchLightColor->setValid(true);
		mSwatchLightColor->set(mLightSavedColor);

		mTextureLight->setEnabled(editable);
		mTextureLight->setValid(true);
		mTextureLight->setImageAssetID(volobjp->getLightTextureID());
		LLAggregatePermissions tex_perms;
		if (gSelectMgr.selectGetAggregateTexturePermissions(tex_perms))
		{
			bool can_copy =
				tex_perms.getValue(PERM_COPY) == LLAggregatePermissions::AP_EMPTY ||
			    tex_perms.getValue(PERM_COPY) == LLAggregatePermissions::AP_ALL;
			bool can_transfer =
				tex_perms.getValue(PERM_TRANSFER) == LLAggregatePermissions::AP_EMPTY ||
				tex_perms.getValue(PERM_TRANSFER) == LLAggregatePermissions::AP_ALL;
			mTextureLight->setCanApplyImmediately(can_copy && can_transfer);
		}
		else
		{
			mTextureLight->setCanApplyImmediately(false);
		}
		if (objectp && objectp->isAttachment())
		{
			mTextureLight->setImmediateFilterPermMask(PERM_COPY |
													  PERM_TRANSFER);
		}
		else
		{
			mTextureLight->setImmediateFilterPermMask(PERM_NONE);
		}

		mSpinLightIntensity->setEnabled(editable);
		mSpinLightRadius->setEnabled(true);
		mSpinLightFalloff->setEnabled(true);

		mSpinLightFOV->setEnabled(editable);
		mSpinLightFocus->setEnabled(true);
		mSpinLightAmbiance->setEnabled(true);

		mSpinLightIntensity->setValue(volobjp->getLightIntensity());
		mSpinLightRadius->setValue(volobjp->getLightRadius());
		mSpinLightFalloff->setValue(volobjp->getLightFalloff());

		LLVector3 params = volobjp->getSpotLightParams();
		mSpinLightFOV->setValue(params.mV[0]);
		mSpinLightFocus->setValue(params.mV[1]);
		mSpinLightAmbiance->setValue(params.mV[2]);
	}
	else
	{
		mSpinLightIntensity->clear();
		mSpinLightRadius->clear();
		mSpinLightFalloff->clear();

		mSwatchLightColor->setEnabled(false);
		mSwatchLightColor->setValid(false);

		mTextureLight->setEnabled(false);
		mTextureLight->setValid(false);

		mSpinLightIntensity->setEnabled(false);
		mSpinLightRadius->setEnabled(false);
		mSpinLightFalloff->setEnabled(false);

		mSpinLightFOV->setEnabled(false);
		mSpinLightFocus->setEnabled(false);
		mSpinLightAmbiance->setEnabled(false);
	}

	// Animated mesh property
	bool is_animated_mesh = single_root_volume && root_volobjp &&
							root_volobjp->isAnimatedObject();
	mCheckAnimatedMesh->setValue(is_animated_mesh);

	bool enabled_animated_mesh = false;
	if (editable && single_root_volume && root_volobjp &&
		root_volobjp == volobjp)
	{
		enabled_animated_mesh = root_volobjp->canBeAnimatedObject();
		if (enabled_animated_mesh && !is_animated_mesh &&
			root_volobjp->isAttachment())
		{
			 enabled_animated_mesh =
				isAgentAvatarValid() &&
				gAgentAvatarp->canAttachMoreAnimatedObjects();
		}
	}
	mCheckAnimatedMesh->setEnabled(enabled_animated_mesh);

	// Refresh any bake on mesh texture
	if (root_volobjp)
	{
		root_volobjp->refreshBakeTexture();
		LLViewerObject::const_child_list_t& child_list =
			root_volobjp->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				it = child_list.begin(), end = child_list.end();
			 it != end; ++it)
		{
			LLViewerObject* childp = *it;
			if (childp)
			{
				childp->refreshBakeTexture();
			}
		}
		if (isAgentAvatarValid())
		{
			gAgentAvatarp->updateMeshVisibility();
		}
	}

	// Flexible properties
	bool is_flexible = volobjp && volobjp->isFlexible();
	mCheckFlexiblePath->setValue(is_flexible);
	if (is_flexible || (volobjp && volobjp->canBeFlexible()))
	{
		mCheckFlexiblePath->setEnabled(editable && single_volume && volobjp &&
									   !volobjp->isMesh() &&
									   !objectp->isPermanentEnforced());
	}
	else
	{
		mCheckFlexiblePath->setEnabled(false);
	}
	if (is_flexible && single_volume && visible_params)
	{
		mSpinFlexSections->setVisible(true);
		mSpinFlexGravity->setVisible(true);
		mSpinFlexFriction->setVisible(true);
		mSpinFlexWind->setVisible(true);
		mSpinFlexTension->setVisible(true);
		mSpinFlexForceX->setVisible(true);
		mSpinFlexForceY->setVisible(true);
		mSpinFlexForceZ->setVisible(true);

		mSpinFlexSections->setEnabled(editable);
		mSpinFlexGravity->setEnabled(editable);
		mSpinFlexFriction->setEnabled(editable);
		mSpinFlexWind->setEnabled(editable);
		mSpinFlexTension->setEnabled(editable);
		mSpinFlexForceX->setEnabled(editable);
		mSpinFlexForceY->setEnabled(editable);
		mSpinFlexForceZ->setEnabled(editable);

		const LLFlexibleObjectData* params = objectp->getFlexibleObjectData();
		mSpinFlexSections->setValue((F32)params->getSimulateLOD());
		mSpinFlexGravity->setValue(params->getGravity());
		mSpinFlexFriction->setValue(params->getAirFriction());
		mSpinFlexWind->setValue(params->getWindSensitivity());
		mSpinFlexTension->setValue(params->getTension());
		mSpinFlexForceX->setValue(params->getUserForce().mV[VX]);
		mSpinFlexForceY->setValue(params->getUserForce().mV[VY]);
		mSpinFlexForceZ->setValue(params->getUserForce().mV[VZ]);
	}
	else
	{
		mSpinFlexSections->clear();
		mSpinFlexGravity->clear();
		mSpinFlexFriction->clear();
		mSpinFlexWind->clear();
		mSpinFlexTension->clear();
		mSpinFlexForceX->clear();
		mSpinFlexForceY->clear();
		mSpinFlexForceZ->clear();

		mSpinFlexSections->setEnabled(false);
		mSpinFlexGravity->setEnabled(false);
		mSpinFlexFriction->setEnabled(false);
		mSpinFlexWind->setEnabled(false);
		mSpinFlexTension->setEnabled(false);
		mSpinFlexForceX->setEnabled(false);
		mSpinFlexForceY->setEnabled(false);
		mSpinFlexForceZ->setEnabled(false);
	}

	// Material properties

	// Update material part
	// slightly inefficient - materials are unique per object, not per TE
	U8 mcode = 0;
	struct f : public LLSelectedTEGetFunctor<U8>
	{
		U8 get(LLViewerObject* object, S32 te)
		{
			return object->getMaterial();
		}
	} func;
	bool material_same = selection->getSelectedTEValue(&func, mcode);
	if (single_volume && material_same && visible_params)
	{
		mComboMaterial->setEnabled(editable);
		mLabelMaterial->setEnabled(editable);
		if (mcode == LL_MCODE_LIGHT)
		{
			if (mComboMaterial->getItemCount() == mComboMaterialItemCount)
			{
				mComboMaterial->add(mFullBright);
			}
			mComboMaterial->setSimple(mFullBright);
		}
		else
		{
			if (mComboMaterial->getItemCount() != mComboMaterialItemCount)
			{
				mComboMaterial->remove(mFullBright);
			}
			// *TODO:Translate
			mComboMaterial->setSimple(gMaterialTable.getName(mcode));
		}
	}
	else
	{
		mComboMaterial->setEnabled(false);
		mLabelMaterial->setEnabled(false);
	}

	// Physics properties

	mIsPhysical = root_objectp && root_objectp->flagUsePhysics();
	// Physics checkbox
	mCheckPhysics->set(mIsPhysical);
	bool is_permanent = root_objectp->flagObjectPermanent();
	if (is_permanent && mIsPhysical)
	{
		// *TODO: Pop up a one-time warning with object details
		llwarns_sparse << "PATHFINDING BUG: editing a Permanent object that is also Physical !"
					   << llendl;
	}
	bool enable_physics = !is_flexible && !is_permanent &&
						  selection->getRootObjectCount();
	mCheckPhysics->setEnabled(enable_physics &&
							  (editable || gAgent.isGodlike()));
	if (mIsPhysical && enable_physics && visible_params)
	{
		mSpinPhysicsGravity->setValue(objectp->getPhysicsGravity());
		mSpinPhysicsGravity->setEnabled(editable);
		mSpinPhysicsFriction->setValue(objectp->getPhysicsFriction());
		mSpinPhysicsFriction->setEnabled(editable);
		mSpinPhysicsDensity->setValue(objectp->getPhysicsDensity());
		mSpinPhysicsDensity->setEnabled(editable);
		mSpinPhysicsRestitution->setValue(objectp->getPhysicsRestitution());
		mSpinPhysicsRestitution->setEnabled(editable);
	}
	else
	{
		mSpinPhysicsGravity->clear();
		mSpinPhysicsGravity->setEnabled(false);
		mSpinPhysicsFriction->clear();
		mSpinPhysicsFriction->setEnabled(false);
		mSpinPhysicsDensity->clear();
		mSpinPhysicsDensity->setEnabled(false);
		mSpinPhysicsRestitution->clear();
		mSpinPhysicsRestitution->setEnabled(false);
	}

	// Update the physics shape combo to include allowed physics shapes
	mComboPhysicsShape->removeall();
	mComboPhysicsShape->add(mPhysicsNone, LLSD(1));

	bool is_mesh = false;
	if (objectp)
	{
		const LLSculptParams* sculpt_paramsp = objectp->getSculptParams();
		is_mesh = sculpt_paramsp &&
				  (sculpt_paramsp->getSculptType() &
				   LL_SCULPT_TYPE_MASK) == LL_SCULPT_TYPE_MESH;
	}
	if (is_mesh)
	{
		const LLVolumeParams& volume_params =
			objectp->getVolume()->getParams();
		const LLUUID& mesh_id = volume_params.getSculptID();
		if (gMeshRepo.hasPhysicsShape(mesh_id))
		{
			// If a mesh contains an uploaded or decomposed physics mesh,
			// allow 'Prim'
			mComboPhysicsShape->add(mPhysicsPrim, LLSD(0));
		}
	}
	else
	{
		// Simple prims always allow physics shape prim
		mComboPhysicsShape->add(mPhysicsPrim, LLSD(0));
	}

	mComboPhysicsShape->add(mPhysicsHull, LLSD(2));
	mComboPhysicsShape->setValue(LLSD(objectp->getPhysicsShapeType()));
	bool enabled = editable && !objectp->isPermanentEnforced() &&
				   (!root_objectp || !root_objectp->isPermanentEnforced());

	mComboPhysicsShape->setEnabled(enabled);
	mLabelPhysicsShape->setEnabled(enabled);

	mReflectionProbe->setEnabled(single_root_volume && enabled &&
								 volobjp && !volobjp->isMesh() &&
								 gAgent.hasInventoryMaterial());

	mObject = objectp;
	mRootObject = root_objectp;
}

void LLPanelVolume::refresh()
{
	getState();
	if (mObject.notNull() && mObject->isDead())
	{
		mObject = NULL;
	}

	if (mRootObject.notNull() && mRootObject->isDead())
	{
		mRootObject = NULL;
	}

	bool enable_physics = false;
	LLSD sim_features;
	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
		enable_physics = region->physicsShapeTypes();
	}
	mLabelPhysicsShape->setVisible(enable_physics);
	mComboPhysicsShape->setVisible(enable_physics);
	mSpinPhysicsGravity->setVisible(enable_physics);
	mSpinPhysicsFriction->setVisible(enable_physics);
	mSpinPhysicsDensity->setVisible(enable_physics);
	mSpinPhysicsRestitution->setVisible(enable_physics);
    // *TODO: add/remove individual physics shape types as per the
	// PhysicsShapeTypes simulator features
}

// virtual
void LLPanelVolume::clearCtrls()
{
	LLPanel::clearCtrls();

	mLabelSelectSingle->setEnabled(false);
	mLabelSelectSingle->setVisible(true);
	mLabelEditObject->setEnabled(false);
	mLabelEditObject->setVisible(false);

	mCheckEmitLight->setEnabled(false);
	mSwatchLightColor->setEnabled(false);
	mSwatchLightColor->setValid(false);

	mTextureLight->setEnabled(false);
	mTextureLight->setValid(false);

	mSpinLightIntensity->setEnabled(false);
	mSpinLightRadius->setEnabled(false);
	mSpinLightFalloff->setEnabled(false);
	mSpinLightFOV->setEnabled(false);
	mSpinLightFocus->setEnabled(false);
	mSpinLightAmbiance->setEnabled(false);

	mCheckFlexiblePath->setEnabled(false);
	mSpinFlexSections->setEnabled(false);
	mSpinFlexGravity->setEnabled(false);
	mSpinFlexFriction->setEnabled(false);
	mSpinFlexWind->setEnabled(false);
	mSpinFlexTension->setEnabled(false);
	mSpinFlexForceX->setEnabled(false);
	mSpinFlexForceY->setEnabled(false);
	mSpinFlexForceZ->setEnabled(false);

	mCheckPhysics->set(false);
	mSpinPhysicsGravity->setEnabled(false);
	mSpinPhysicsFriction->setEnabled(false);
	mSpinPhysicsDensity->setEnabled(false);
	mSpinPhysicsRestitution->setEnabled(false);

	mComboMaterial->setEnabled(false);
	mLabelMaterial->setEnabled(false);

	mCheckAnimatedMesh->setEnabled(false);
}

//
// Static functions
//

void LLPanelVolume::sendIsLight()
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)objectp;

	bool value = mCheckEmitLight->getValue();
	volobjp->setIsLight(value);
	llinfos << "update light sent" << llendl;
}

void LLPanelVolume::sendIsFlexible()
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)objectp;

	bool is_flexible = mCheckFlexiblePath->getValue();
	if (is_flexible && objectp->getClickAction() == CLICK_ACTION_SIT)
	{
		gSelectMgr.selectionSetClickAction(CLICK_ACTION_NONE);
	}

	if (volobjp->setIsFlexible(is_flexible))
	{
		mObject->sendShapeUpdate();
		gSelectMgr.selectionUpdatePhantom(volobjp->flagPhantom());
	}

	llinfos << "update flexible sent" << llendl;
}

void LLPanelVolume::sendIsPhysical()
{
	bool value = mCheckPhysics->get();
	if (mIsPhysical != value)
	{
		gSelectMgr.selectionUpdatePhysics(value);
		mIsPhysical = value;
		llinfos << "Update physics sent" << llendl;
	}
}

//static
void LLPanelVolume::onCommitPhysics(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self)
	{
		self->sendIsPhysical();
	}
}

//static
void LLPanelVolume::sendPhysicsShapeType(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*) userdata;
	if (!self || !ctrl) return;
	U8 type = ctrl->getValue().asInteger();
	gSelectMgr.selectionSetPhysicsType(type);

	self->refreshCost();
}

//static
void LLPanelVolume::sendPhysicsGravity(LLUICtrl* ctrl, void* userdata)
{
	F32 val = ctrl->getValue().asReal();
	gSelectMgr.selectionSetGravity(val);
}

//static
void LLPanelVolume::sendPhysicsFriction(LLUICtrl* ctrl, void* userdata)
{
	F32 val = ctrl->getValue().asReal();
	gSelectMgr.selectionSetFriction(val);
}

//static
void LLPanelVolume::sendPhysicsRestitution(LLUICtrl* ctrl, void* userdata)
{
	F32 val = ctrl->getValue().asReal();
	gSelectMgr.selectionSetRestitution(val);
}

//static
void LLPanelVolume::sendPhysicsDensity(LLUICtrl* ctrl, void* userdata)
{
	F32 val = ctrl->getValue().asReal();
	gSelectMgr.selectionSetDensity(val);
}

//static
void LLPanelVolume::refreshCost()
{
	LLViewerObject* obj = gSelectMgr.getSelection()->getFirstObject();
	if (obj)
	{
		obj->getObjectCost();
	}
}

void LLPanelVolume::onLightCancelColor(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self)
	{
		self->mSwatchLightColor->setColor(self->mLightSavedColor);
		onLightSelectColor(NULL, userdata);
	}
}

void LLPanelVolume::onLightCancelTexture(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self)
	{
		self->mTextureLight->setImageAssetID(self->mLightSavedTexture);
	}
}

void LLPanelVolume::onLightSelectColor(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (!self || !self->mObject ||
		self->mObject->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)self->mObject.get();
	if (!volobjp) return;

	LLColor4 clr = self->mSwatchLightColor->get();
	LLColor3 clr3(clr);
	volobjp->setLightSRGBColor(clr3);
	self->mLightSavedColor = clr;
}

void LLPanelVolume::onLightSelectTexture(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (!self || !self->mObject ||
		self->mObject->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)self->mObject.get();
	if (!volobjp) return;

	LLUUID id = self->mTextureLight->getImageAssetID();
	volobjp->setLightTextureID(id);
	self->mLightSavedTexture = id;
}

//static
bool LLPanelVolume::onDragTexture(LLUICtrl* ctrl, LLInventoryItem* item,
								  void* userdata)
{
	bool accept = true;
	for (LLObjectSelection::root_iterator
			iter = gSelectMgr.getSelection()->root_begin(),
			end = gSelectMgr.getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* obj = node->getObject();
		if (!LLToolDragAndDrop::isInventoryDropAcceptable(obj, item))
		{
			accept = false;
			break;
		}
	}
	return accept;
}

//static
void LLPanelVolume::onCommitMaterial(LLUICtrl* ctrl, void* userdata)
{
	if (!userdata || !ctrl)
	{
		return;
	}

	LLPanelVolume* self = (LLPanelVolume*)userdata;

	LLComboBox* combop = (LLComboBox*)ctrl;
	const std::string& material_name = combop->getSimple();
	if (material_name == self->mFullBright)
	{
		return;
	}

	// Apply the currently selected material to the object
	U8 mcode = gMaterialTable.getMCode(material_name);
	LLViewerObject* objectp = self->mObject;
	if (objectp)
	{
		objectp->setPhysicsGravity(DEFAULT_GRAVITY_MULTIPLIER);
		objectp->setPhysicsFriction(gMaterialTable.getFriction(mcode));
		// Currently density is always set to 1000 server side regardless of
		// chosen material, actual material density should be used here, if
		// this behavior changes.
		objectp->setPhysicsDensity(DEFAULT_DENSITY);
		objectp->setPhysicsRestitution(gMaterialTable.getRestitution(mcode));
	}
	gSelectMgr.selectionSetMaterial(mcode);
}

//static
void LLPanelVolume::onCommitLight(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (!self || !self->mObject ||
		self->mObject->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}
	LLVOVolume* volobjp = (LLVOVolume*)self->mObject.get();
	if (!volobjp) return;

	volobjp->setLightIntensity((F32)self->mSpinLightIntensity->getValue().asReal());
	volobjp->setLightRadius((F32)self->mSpinLightRadius->getValue().asReal());
	volobjp->setLightFalloff((F32)self->mSpinLightFalloff->getValue().asReal());

	LLColor4 clr = self->mSwatchLightColor->get();
	volobjp->setLightSRGBColor(LLColor3(clr));

	LLUUID id = self->mTextureLight->getImageAssetID();
	if (id.notNull())
	{
		if (!volobjp->isLightSpotlight())
		{
			// This commit is making this a spot light; set UI to
			// default params
			volobjp->setLightTextureID(id);
			LLVector3 spot_params = volobjp->getSpotLightParams();
			self->mSpinLightFOV->setValue(spot_params.mV[0]);
			self->mSpinLightFocus->setValue(spot_params.mV[1]);
			self->mSpinLightAmbiance->setValue(spot_params.mV[2]);
		}
		else
		{
			// Modifying existing params
			LLVector3 spot_params;
			spot_params.mV[0] = (F32)self->mSpinLightFOV->getValue().asReal();
			spot_params.mV[1] =
				(F32)self->mSpinLightFocus->getValue().asReal();
			spot_params.mV[2] =
				(F32)self->mSpinLightAmbiance->getValue().asReal();
			volobjp->setSpotLightParams(spot_params);
		}
	}
	else if (volobjp->isLightSpotlight())
	{
		// No longer a spot light
		volobjp->setLightTextureID(id);
#if 0
		self->mSpinLightFOV->setEnabled(false);
		self->mSpinLightFocus->setEnabled(false);
		self->mSpinLightAmbiance->setEnabled(false);
#endif
	}
}

//static
void LLPanelVolume::onCommitIsLight(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self)
	{
		self->sendIsLight();
	}
}

//static
void LLPanelVolume::onCommitFlexible(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (!self) return;
	LLViewerObject* objectp = self->mObject;
	if (!objectp || objectp->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}

	const LLFlexibleObjectData* params = objectp->getFlexibleObjectData();
	if (params)
	{
		LLFlexibleObjectData new_params(*params);
		new_params.setSimulateLOD(self->mSpinFlexSections->getValue().asInteger());
		new_params.setGravity((F32)self->mSpinFlexGravity->getValue().asReal());
		new_params.setAirFriction((F32)self->mSpinFlexFriction->getValue().asReal());
		new_params.setWindSensitivity((F32)self->mSpinFlexWind->getValue().asReal());
		new_params.setTension((F32)self->mSpinFlexTension->getValue().asReal());
		F32 fx = (F32)self->mSpinFlexForceX->getValue().asReal();
		F32 fy = (F32)self->mSpinFlexForceY->getValue().asReal();
		F32 fz = (F32)self->mSpinFlexForceZ->getValue().asReal();
		LLVector3 force(fx, fy, fz);

		new_params.setUserForce(force);
		objectp->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, new_params, true);
	}

	// Values may fail validation
	self->refresh();
}

bool handleResponseChangeToFlexible(const LLSD& notification,
									const LLSD& response,
									LLPanelVolume* self)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (self && option == 0)
	{
		self->sendIsFlexible();
	}
	return false;
}

//static
void LLPanelVolume::onCommitIsFlexible(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	if (self && self->mObject)
	{
		if (self->mObject->flagObjectPermanent())
		{
			gNotifications.add("ChangeToFlexiblePath", LLSD(), LLSD(),
							   boost::bind(handleResponseChangeToFlexible,
										   _1, _2, self));
		}
		else
		{
			self->sendIsFlexible();
		}
	}
}

//static
void LLPanelVolume::onCommitAnimatedMesh(LLUICtrl* ctrl, void* userdata)
{
	LLPanelVolume* self = (LLPanelVolume*)userdata;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	LLViewerObject* objectp = self->mObject;
	if (!objectp || objectp->getPCode() != LL_PCODE_VOLUME)
	{
		return;
	}

	LLVOVolume* volobjp = (LLVOVolume*)objectp;
	U32 flags = volobjp->getExtendedMeshFlags();
	U32 new_flags = flags;
	if (check->get())
	{
		new_flags |= LLExtendedMeshParams::ANIMATED_MESH_ENABLED_FLAG;
	}
	else
	{
		new_flags &= ~LLExtendedMeshParams::ANIMATED_MESH_ENABLED_FLAG;
	}
	if (new_flags != flags)
	{
		volobjp->setExtendedMeshFlags(new_flags);
	}

	// Refresh any bake on mesh texture
	volobjp->refreshBakeTexture();
	LLViewerObject::const_child_list_t& child_list = volobjp->getChildren();
	for (LLViewerObject::child_list_t::const_iterator
			it = child_list.begin(), end = child_list.end();
		 it != end; ++it)
	{
		LLViewerObject* childp = *it;
		if (childp)
		{
			childp->refreshBakeTexture();
		}
	}
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->updateMeshVisibility();
	}
}

//static
void LLPanelVolume::onClickProbe(void* userdata)
{
	HBReflectionProbe::show((LLView*)userdata);
}
