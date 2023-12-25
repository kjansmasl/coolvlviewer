/**
 * @file llpanelobject.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llpanelobject.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lleconomy.h"
#include "llpermissionsflags.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llcolorswatch.h"
#include "lldrawpool.h"
#include "llfirstuse.h"
#include "llmanipscale.h"
#include "llpipeline.h"
#include "llpreviewscript.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltexturectrl.h"
#include "lltool.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"		// For LLPickInfo
//MK
#include "mkrlinterface.h"
#include "llvoavatarself.h"
//mk
#include "llvovolume.h"
#include "llworld.h"

// Static variables for the object clipboard
bool LLPanelObject::sSavedSizeValid = false;
bool LLPanelObject::sSavedPosValid = false;
bool LLPanelObject::sSavedRotValid = false;
bool LLPanelObject::sSavedShapeValid = false;

LLVector3 LLPanelObject::sSavedSize;
LLVector3 LLPanelObject::sSavedPos;
LLVector3 LLPanelObject::sSavedRot;
LLVolumeParams LLPanelObject::sSavedShape;

//
// Constants
//
enum {
	MI_BOX,
	MI_CYLINDER,
	MI_PRISM,
	MI_SPHERE,
	MI_TORUS,
	MI_TUBE,
	MI_RING,
	MI_SCULPT,
	MI_NONE,
	MI_VOLUME_COUNT
};

enum {
	MI_HOLE_SAME,
	MI_HOLE_CIRCLE,
	MI_HOLE_SQUARE,
	MI_HOLE_TRIANGLE,
	MI_HOLE_COUNT
};

LLPanelObject::LLPanelObject(const std::string& name)
:	LLPanel(name),
	mIsTemporary(false),
	mIsPhantom(false),
	mSizeChanged(false),
	mSelectedType(MI_BOX),
	mSculptTextureRevert(LLUUID::null),
	mSculptTypeRevert(0)
{
}

//virtual
bool LLPanelObject::postBuild()
{
	setMouseOpaque(false);

	mLabelSelectSingle = getChild<LLTextBox>("select_single");
	mLabelEditObject = getChild<LLTextBox>("edit_object");

	mButtonCopy = getChild<LLButton>("copy");
	mButtonCopy->setClickedCallback(onClickCopy, this);

	mButtonPaste = getChild<LLButton>("paste");
	mButtonPaste->setClickedCallback(onClickPaste, this);

	// Lock checkbox
	mCheckLock = getChild<LLCheckBoxCtrl>("checkbox locked");
	mCheckLock->setCommitCallback(onCommitLock);
	mCheckLock->setCallbackUserData(this);

	// Temporary checkbox
	mCheckTemporary = getChild<LLCheckBoxCtrl>("Temporary Checkbox Ctrl");
	mCheckTemporary->setCommitCallback(onCommitTemporary);
	mCheckTemporary->setCallbackUserData(this);

	// Phantom checkbox
	mCheckPhantom = getChild<LLCheckBoxCtrl>("Phantom Checkbox Ctrl");
	mCheckPhantom->setCommitCallback(onCommitPhantom);
	mCheckPhantom->setCallbackUserData(this);

	// Position

	mLabelPosition = getChild<LLTextBox>("label position");

	mCtrlPosX = getChild<LLSpinCtrl>("Pos X");
	mCtrlPosX->setCommitCallback(onCommitPosition);
	mCtrlPosX->setCallbackUserData(this);

	mCtrlPosY = getChild<LLSpinCtrl>("Pos Y");
	mCtrlPosY->setCommitCallback(onCommitPosition);
	mCtrlPosY->setCallbackUserData(this);

	mCtrlPosZ = getChild<LLSpinCtrl>("Pos Z");
	mCtrlPosZ->setCommitCallback(onCommitPosition);
	mCtrlPosZ->setCallbackUserData(this);

	mCheckCopyPos = getChild<LLCheckBoxCtrl>("paste_position");
	mCheckCopyPos->setCommitCallback(onCommitCopyPaste);
	mCheckCopyPos->setCallbackUserData(this);

	// Scale

	mLabelSize = getChild<LLTextBox>("label size");

	mCtrlScaleX = getChild<LLSpinCtrl>("Scale X");
	mCtrlScaleX->setCommitCallback(onCommitScale);
	mCtrlScaleX->setCallbackUserData(this);

	mCtrlScaleY = getChild<LLSpinCtrl>("Scale Y");
	mCtrlScaleY->setCommitCallback(onCommitScale);
	mCtrlScaleY->setCallbackUserData(this);

	mCtrlScaleZ = getChild<LLSpinCtrl>("Scale Z");
	mCtrlScaleZ->setCommitCallback(onCommitScale);
	mCtrlScaleZ->setCallbackUserData(this);

	mCheckCopySize = getChild<LLCheckBoxCtrl>("paste_size");
	mCheckCopySize->setCommitCallback(onCommitCopyPaste);
	mCheckCopySize->setCallbackUserData(this);

	// Rotation

	mLabelRotation = getChild<LLTextBox>("label rotation");

	mCtrlRotX = getChild<LLSpinCtrl>("Rot X");
	mCtrlRotX->setCommitCallback(onCommitRotation);
	mCtrlRotX->setCallbackUserData(this);

	mCtrlRotY = getChild<LLSpinCtrl>("Rot Y");
	mCtrlRotY->setCommitCallback(onCommitRotation);
	mCtrlRotY->setCallbackUserData(this);

	mCtrlRotZ = getChild<LLSpinCtrl>("Rot Z");
	mCtrlRotZ->setCommitCallback(onCommitRotation);
	mCtrlRotZ->setCallbackUserData(this);

	mCheckCopyRot = getChild<LLCheckBoxCtrl>("paste_rotation");
	mCheckCopyRot->setCommitCallback(onCommitCopyPaste);
	mCheckCopyRot->setCallbackUserData(this);

	mCheckCopyShape = getChild<LLCheckBoxCtrl>("paste_shape");
	mCheckCopyShape->setCommitCallback(onCommitCopyPaste);
	mCheckCopyShape->setCallbackUserData(this);

	// Base Type

	mLabelBaseType = getChild<LLTextBox>("label basetype");

	mComboBaseType = getChild<LLComboBox>("comboBaseType");
	mComboBaseType->setCommitCallback(onCommitParametric);
	mComboBaseType->setCallbackUserData(this);

	// Cut

	mLabelCut = getChild<LLTextBox>("text cut");

	mSpinCutBegin = getChild<LLSpinCtrl>("cut begin");
	mSpinCutBegin->setCommitCallback(onCommitParametric);
	mSpinCutBegin->setCallbackUserData(this);

	mSpinCutEnd = getChild<LLSpinCtrl>("cut end");
	mSpinCutEnd->setCommitCallback(onCommitParametric);
	mSpinCutEnd->setCallbackUserData(this);

	// Hollow / Skew

	mLabelHollow = getChild<LLTextBox>("text hollow");
	mLabelSkew = getChild<LLTextBox>("text skew");

	mSpinHollow = getChild<LLSpinCtrl>("Scale 1");
	mSpinHollow->setCommitCallback(onCommitParametric);
	mSpinHollow->setCallbackUserData(this);

	mSpinSkew = getChild<LLSpinCtrl>("Skew");
	mSpinSkew->setCommitCallback(onCommitParametric);
	mSpinSkew->setCallbackUserData(this);

	mLabelHollowShape = getChild<LLTextBox>("Hollow Shape");

	// Hole Type

	mComboHoleType = getChild<LLComboBox>("hole");
	mComboHoleType->setCommitCallback(onCommitParametric);
	mComboHoleType->setCallbackUserData(this);

	// Twist

	mLabelTwist = getChild<LLTextBox>("text twist");

	mSpinTwistBegin = getChild<LLSpinCtrl>("Twist Begin");
	mSpinTwistBegin->setCommitCallback(onCommitParametric);
	mSpinTwistBegin->setCallbackUserData(this);

	mSpinTwist = getChild<LLSpinCtrl>("Twist End");
	mSpinTwist->setCommitCallback(onCommitParametric);
	mSpinTwist->setCallbackUserData(this);

	// Scale

	mLabelScaleHole = getChild<LLTextBox>("scale_hole");
	mLabelScaleTaper = getChild<LLTextBox>("scale_taper");

	mSpinScaleX = getChild<LLSpinCtrl>("Taper Scale X");
	mSpinScaleX->setCommitCallback(onCommitParametric);
	mSpinScaleX->setCallbackUserData(this);

	mSpinScaleY = getChild<LLSpinCtrl>("Taper Scale Y");
	mSpinScaleY->setCommitCallback(onCommitParametric);
	mSpinScaleY->setCallbackUserData(this);

	// Shear

	mLabelShear = getChild<LLTextBox>("text topshear");

	mSpinShearX = getChild<LLSpinCtrl>("Shear X");
	mSpinShearX->setCommitCallback(onCommitParametric);
	mSpinShearX->setCallbackUserData(this);

	mSpinShearY = getChild<LLSpinCtrl>("Shear Y");
	mSpinShearY->setCommitCallback(onCommitParametric);
	mSpinShearY->setCallbackUserData(this);

	// Path / Profile

	mLabelAdvancedCut = getChild<LLTextBox>("advanced_cut");
	mLabelAdvancedDimple = getChild<LLTextBox>("advanced_dimple");
	mLabelAdvancedSlice = getChild<LLTextBox>("advanced_slice");

	mCtrlPathBegin = getChild<LLSpinCtrl>("Path Limit Begin");
	mCtrlPathBegin->setCommitCallback(onCommitParametric);
	mCtrlPathBegin->setCallbackUserData(this);

	mCtrlPathEnd = getChild<LLSpinCtrl>("Path Limit End");
	mCtrlPathEnd->setCommitCallback(onCommitParametric);
	mCtrlPathEnd->setCallbackUserData(this);

	// Taper

	mLabelTaper = getChild<LLTextBox>("text taper2");

	mSpinTaperX = getChild<LLSpinCtrl>("Taper X");
	mSpinTaperX->setCommitCallback(onCommitParametric);
	mSpinTaperX->setCallbackUserData(this);

	mSpinTaperY = getChild<LLSpinCtrl>("Taper Y");
	mSpinTaperY->setCommitCallback(onCommitParametric);
	mSpinTaperY->setCallbackUserData(this);

	// Radius Offset / Revolutions

	mLabelRadiusOffset = getChild<LLTextBox>("text radius delta");
	mLabelRevolutions = getChild<LLTextBox>("text revolutions");

	mSpinRadiusOffset = getChild<LLSpinCtrl>("Radius Offset");
	mSpinRadiusOffset->setCommitCallback(onCommitParametric);
	mSpinRadiusOffset->setCallbackUserData(this);

	mSpinRevolutions = getChild<LLSpinCtrl>("Revolutions");
	mSpinRevolutions->setCommitCallback(onCommitParametric);
	mSpinRevolutions->setCallbackUserData(this);

	// Sculpt

	mCtrlSculptTexture = getChild<LLTextureCtrl>("sculpt texture control");
	mCtrlSculptTexture->setDefaultImageAssetID(LLUUID(SCULPT_DEFAULT_TEXTURE));
	mCtrlSculptTexture->setCommitCallback(LLPanelObject::onCommitSculpt);
	mCtrlSculptTexture->setOnCancelCallback(LLPanelObject::onCancelSculpt);
	mCtrlSculptTexture->setOnSelectCallback(LLPanelObject::onSelectSculpt);
	mCtrlSculptTexture->setDropCallback(LLPanelObject::onDropSculpt);
	mCtrlSculptTexture->setCallbackUserData(this);
	// Don't allow (no copy) or (no transfer) textures to be selected during
	// immediate mode
	mCtrlSculptTexture->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
	// Allow any texture to be used during non-immediate mode.
	mCtrlSculptTexture->setNonImmediateFilterPermMask(PERM_NONE);
	LLAggregatePermissions texture_perms;
	if (gSelectMgr.selectGetAggregateTexturePermissions(texture_perms))
	{
		bool can_copy = texture_perms.getValue(PERM_COPY) == LLAggregatePermissions::AP_EMPTY ||
						texture_perms.getValue(PERM_COPY) == LLAggregatePermissions::AP_ALL;
		bool can_transfer = texture_perms.getValue(PERM_TRANSFER) == LLAggregatePermissions::AP_EMPTY ||
							texture_perms.getValue(PERM_TRANSFER) == LLAggregatePermissions::AP_ALL;
		mCtrlSculptTexture->setCanApplyImmediately(can_copy && can_transfer);
	}
	else
	{
		mCtrlSculptTexture->setCanApplyImmediately(false);
	}

	mLabelSculptType = getChild<LLTextBox>("label sculpt type");

	mCtrlSculptType = getChild<LLComboBox>("sculpt type control");
	mCtrlSculptType->setCommitCallback(onCommitSculptType);
	mCtrlSculptType->setCallbackUserData(this);

	mCtrlSculptMirror = getChild<LLCheckBoxCtrl>("sculpt mirror control");
	mCtrlSculptMirror->setCommitCallback(onCommitSculptType);
	mCtrlSculptMirror->setCallbackUserData(this);

	mCtrlSculptInvert = getChild<LLCheckBoxCtrl>("sculpt invert control");
	mCtrlSculptInvert->setCommitCallback(onCommitSculptType);
	mCtrlSculptInvert->setCallbackUserData(this);

	// Start with everyone disabled
	clearCtrls();

	return true;
}

void LLPanelObject::getState()
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	LLViewerObject* objectp = selection->getFirstRootObject();
	LLViewerObject* root_objectp = objectp;
	if (!objectp)
	{
		objectp = selection->getFirstObject();
		// *FIXME: should not we just keep the child ?
		if (objectp)
		{
			LLViewerObject* parentp = objectp->getRootEdit();

			if (parentp)
			{
				root_objectp = parentp;
			}
			else
			{
				root_objectp = objectp;
			}
		}
	}

	LLVOVolume* volobjp = NULL;
	if (objectp && objectp->getPCode() == LL_PCODE_VOLUME)
	{
		volobjp = (LLVOVolume*)objectp;
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

	static LLCachedControl<bool> edit_linked_parts(gSavedSettings,
												   "EditLinkedParts");

	bool enable_move, enable_modify;
	gSelectMgr.selectGetEditMoveLinksetPermissions(enable_move, enable_modify);
	bool enable_scale = enable_modify;
	// Already accounts for children case, which needs permModify() as well:
	bool enable_rotate = enable_move;

	S32 selected_count = selection->getObjectCount();
	bool single_volume = selected_count == 1 &&
						  gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME);

	if (selection->getRootObjectCount() > 1)
	{
		enable_move = false;
		enable_scale = false;
		enable_rotate = false;
	}

//MK
	// Cannot edit objects that we are sitting on, when sit-restricted
	if (gRLenabled &&
		(gRLInterface.mSittpMax < EXTREMUM || gRLInterface.mContainsUnsit) &&
		 isAgentAvatarValid() && gAgentAvatarp->mIsSitting &&
		 objectp->getRootEdit()->isAgentSeat())
	{
		enable_move = enable_scale = enable_rotate = false;
	}
//mk

	bool is_attachment = objectp->isAttachment();
	if (is_attachment && (enable_move || enable_rotate) &&
		gSelectMgr.getGridMode() != 0)
	{
		// Position and rotation for attachments are always in given mode 0, so
		// disable the position and rotation spinners when another mode is in
		// force.
		enable_move = enable_rotate = false;
	}

	LLVector3 vec;
	if (enable_move)
	{
		if (is_attachment)
		{
			// Attachments Z (relative to avatar joint) can be negative !
			mCtrlPosZ->setMinValue(-mCtrlPosZ->getMaxValue());
		}
		else
		{
			// Normal objects cannot have a negative altitude
			mCtrlPosZ->setMinValue(0.f);
		}
		vec = objectp->getPositionEdit();
		mCtrlPosX->set(vec.mV[VX]);
		mCtrlPosY->set(vec.mV[VY]);
		mCtrlPosZ->set(vec.mV[VZ]);
	}
	else
	{
		mCtrlPosX->clear();
		mCtrlPosY->clear();
		mCtrlPosZ->clear();
	}

	mLabelPosition->setEnabled(enable_move);
	mCtrlPosX->setEnabled(enable_move);
	mCtrlPosY->setEnabled(enable_move);
	mCtrlPosZ->setEnabled(enable_move);
	mCheckCopyPos->setEnabled(enable_move);

	if (enable_scale)
	{
		vec = objectp->getScale();
		mCtrlScaleX->set(vec.mV[VX]);
		mCtrlScaleY->set(vec.mV[VY]);
		mCtrlScaleZ->set(vec.mV[VZ]);
	}
	else
	{
		mCtrlScaleX->clear();
		mCtrlScaleY->clear();
		mCtrlScaleZ->clear();
	}

	mLabelSize->setEnabled(enable_scale);
	mCtrlScaleX->setEnabled(enable_scale);
	mCtrlScaleY->setEnabled(enable_scale);
	mCtrlScaleZ->setEnabled(enable_scale);
	mCheckCopySize->setEnabled(enable_scale);

	LLQuaternion object_rot = objectp->getRotationEdit();
	object_rot.getEulerAngles(&(mCurEulerDegrees.mV[VX]),
							  &(mCurEulerDegrees.mV[VY]),
							  &(mCurEulerDegrees.mV[VZ]));
	mCurEulerDegrees *= RAD_TO_DEG;
	mCurEulerDegrees.mV[VX] = fmod(ll_round(mCurEulerDegrees.mV[VX],
								   OBJECT_ROTATION_PRECISION) + 360.f, 360.f);
	mCurEulerDegrees.mV[VY] = fmod(ll_round(mCurEulerDegrees.mV[VY],
								   OBJECT_ROTATION_PRECISION) + 360.f, 360.f);
	mCurEulerDegrees.mV[VZ] = fmod(ll_round(mCurEulerDegrees.mV[VZ],
								   OBJECT_ROTATION_PRECISION) + 360.f, 360.f);

	if (enable_rotate)
	{
		mCtrlRotX->set(mCurEulerDegrees.mV[VX]);
		mCtrlRotY->set(mCurEulerDegrees.mV[VY]);
		mCtrlRotZ->set(mCurEulerDegrees.mV[VZ]);
	}
	else
	{
		mCtrlRotX->clear();
		mCtrlRotY->clear();
		mCtrlRotZ->clear();
	}

	mLabelRotation->setEnabled(enable_rotate);
	mCtrlRotX->setEnabled(enable_rotate);
	mCtrlRotY->setEnabled(enable_rotate);
	mCtrlRotZ->setEnabled(enable_rotate);
	mCheckCopyRot->setEnabled(enable_rotate);

	LLUUID owner_id;
	std::string owner_name;
	gSelectMgr.selectGetOwner(owner_id, owner_name);

	// BUG ? Check for all objects being editable ?
	S32 roots_selected = selection->getRootObjectCount();
	bool editable = root_objectp->permModify();

	// Select Single Message
	mLabelSelectSingle->setVisible(false);
	mLabelEditObject->setVisible(false);
	if (!editable || single_volume || selected_count <= 1)
	{
		mLabelEditObject->setVisible(true);
		mLabelEditObject->setEnabled(true);
		mCheckCopyShape->setVisible(true);
	}
	else
	{
		mLabelSelectSingle->setVisible(true);
		mLabelSelectSingle->setEnabled(true);
		mCheckCopyShape->setVisible(false);
	}

	bool is_permanent = root_objectp->flagObjectPermanent();
	bool is_permanent_enforced = root_objectp->isPermanentEnforced();
	bool is_character = root_objectp->flagCharacter();
	// Should never have a permanent object that is also a character
	if (is_permanent && is_character)
	{
		// *TODO: Pop up a one-time warning with object details
		llwarns << "PATHFINDING BUG: editing a Permanent object that is also a Character !"
				<< llendl;
	}

	// Lock checkbox - only modifiable if you own the object.
	bool self_owned = (gAgentID == owner_id);
	mCheckLock->setEnabled(roots_selected > 0 && self_owned &&
						   !is_permanent_enforced);

	// More lock and debit checkbox - get the values
	U32 owner_mask_on;
	U32 owner_mask_off;
	bool valid = gSelectMgr.selectGetPerm(PERM_OWNER,
										  &owner_mask_on, &owner_mask_off);
	if (valid)
	{
		if (owner_mask_on & PERM_MOVE)
		{
			// Owner can move, so not locked
			mCheckLock->set(false);
			mCheckLock->setTentative(false);
		}
		else if (owner_mask_off & PERM_MOVE)
		{
			// Owner can't move, so locked
			mCheckLock->set(true);
			mCheckLock->setTentative(false);
		}
		else
		{
			// Some locked, some not locked
			mCheckLock->set(false);
			mCheckLock->setTentative(true);
		}
	}

	bool is_flexible = volobjp && volobjp->isFlexible();

	mIsTemporary = root_objectp->flagTemporaryOnRez();
	if (is_permanent && mIsTemporary)
	{
		// *TODO: Pop up a one-time warning with object details
		llwarns << "PATHFINDING BUG: editing a Permanent object that is also Temporary !"
				<< llendl;
	}
	mCheckTemporary->set(mIsTemporary);
	mCheckTemporary->setEnabled(roots_selected > 0 && editable &&
								!is_permanent);

	mIsPhantom = root_objectp->flagPhantom();
	if (is_character && mIsPhantom)
	{
		// *TODO: Pop up a one-time warning with object details
		llwarns << "PATHFINDING BUG: editing a Character object that is also Phantom !"
				<< llendl;
	}
	bool is_volume_detect = root_objectp->flagVolumeDetect();
	mCheckPhantom->set(mIsPhantom);
	mCheckPhantom->setEnabled(roots_selected > 0 && editable && !is_flexible &&
							  !is_permanent_enforced && !is_character &&
							  !is_volume_detect);

	S32 selected_item = MI_BOX;
	S32	selected_hole = MI_HOLE_SAME;
	bool enabled = false;
	bool hole_enabled = false;
	F32 scale_x = 1.f, scale_y = 1.f;

	if (!objectp || !objectp->getVolume() || !editable || !single_volume)
	{
		// Clear out all geometry fields.
		mComboBaseType->clear();
		mSpinHollow->clear();
		mSpinCutBegin->clear();
		mSpinCutEnd->clear();
		mCtrlPathBegin->clear();
		mCtrlPathEnd->clear();
		mSpinScaleX->clear();
		mSpinScaleY->clear();
		mSpinTwist->clear();
		mSpinTwistBegin->clear();
		mComboHoleType->clear();
		mSpinShearX->clear();
		mSpinShearY->clear();
		mSpinTaperX->clear();
		mSpinTaperY->clear();
		mSpinRadiusOffset->clear();
		mSpinRevolutions->clear();
		mSpinSkew->clear();

		mSelectedType = MI_NONE;
	}
	else
	{
		// Only allowed to change these parameters for objects that you have
		// permissions on AND are not attachments.
		enabled = root_objectp->permModify() &&
				  !root_objectp->isPermanentEnforced();

		// Volume type
		const LLVolumeParams& volume_params =
			objectp->getVolume()->getParams();
		U8 path = volume_params.getPathParams().getCurveType();
		U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
		U8 profile = profile_and_hole & LL_PCODE_PROFILE_MASK;
		U8 hole = profile_and_hole & LL_PCODE_HOLE_MASK;

		// Scale goes first so we can differentiate between a sphere and a
		// torus, which have the same profile and path types.

		// Scale
		scale_x = volume_params.getRatioX();
		scale_y = volume_params.getRatioY();

		bool linear_path = (path == LL_PCODE_PATH_LINE ||
							path == LL_PCODE_PATH_FLEXIBLE);
		if (linear_path && profile == LL_PCODE_PROFILE_CIRCLE)
		{
			selected_item = MI_CYLINDER;
		}
		else if (linear_path && profile == LL_PCODE_PROFILE_SQUARE)
		{
			selected_item = MI_BOX;
		}
		else if (linear_path && profile == LL_PCODE_PROFILE_ISOTRI)
		{
			selected_item = MI_PRISM;
		}
		else if (linear_path && profile == LL_PCODE_PROFILE_EQUALTRI)
		{
			selected_item = MI_PRISM;
		}
		else if (linear_path && profile == LL_PCODE_PROFILE_RIGHTTRI)
		{
			selected_item = MI_PRISM;
		}
		else if (path == LL_PCODE_PATH_FLEXIBLE) // shouldn't happen
		{
			selected_item = MI_CYLINDER; // reasonable default
		}
		else if (path == LL_PCODE_PATH_CIRCLE &&
				 profile == LL_PCODE_PROFILE_CIRCLE && scale_y > 0.75f)
		{
			selected_item = MI_SPHERE;
		}
		else if (path == LL_PCODE_PATH_CIRCLE &&
				 profile == LL_PCODE_PROFILE_CIRCLE && scale_y <= 0.75f)
		{
			selected_item = MI_TORUS;
		}
		else if (path == LL_PCODE_PATH_CIRCLE &&
				 profile == LL_PCODE_PROFILE_CIRCLE_HALF)
		{
			selected_item = MI_SPHERE;
		}
		else if (path == LL_PCODE_PATH_CIRCLE2 &&
				 profile == LL_PCODE_PROFILE_CIRCLE)
		{
			// Spirals aren't supported.  Make it into a sphere.  JC
			selected_item = MI_SPHERE;
		}
		else if (path == LL_PCODE_PATH_CIRCLE &&
				 profile == LL_PCODE_PROFILE_EQUALTRI)
		{
			selected_item = MI_RING;
		}
		else if (path == LL_PCODE_PATH_CIRCLE &&
				 profile == LL_PCODE_PROFILE_SQUARE && scale_y <= 0.75f)
		{
			selected_item = MI_TUBE;
		}
		else
		{
			llwarns << "Unknown path " << (S32)path << " - profile "
					<< (S32)profile << " in getState()" << llendl;
			selected_item = MI_BOX;
		}

		if (objectp->getParameterEntryInUse(LLNetworkData::PARAMS_SCULPT))
		{
			selected_item = MI_SCULPT;
			LLFirstUse::useSculptedPrim();
		}

		mComboBaseType->setCurrentByIndex(selected_item);
		mSelectedType = selected_item;

		// Grab S path
		F32 begin_s	= volume_params.getBeginS();
		F32 end_s	= volume_params.getEndS();

		// Compute cut and advanced cut from S and T
		F32 begin_t = volume_params.getBeginT();
		F32 end_t = volume_params.getEndT();

		// Hollowness
		F32 hollow = volume_params.getHollow();
		mSpinHollow->set(100.f * hollow);

		// All hollow objects allow a shape to be selected.
		if (hollow > 0.f)
		{
			switch (hole)
			{
				case LL_PCODE_HOLE_CIRCLE:
					selected_hole = MI_HOLE_CIRCLE;
					break;

				case LL_PCODE_HOLE_SQUARE:
					selected_hole = MI_HOLE_SQUARE;
					break;

				case LL_PCODE_HOLE_TRIANGLE:
					selected_hole = MI_HOLE_TRIANGLE;
					break;

				case LL_PCODE_HOLE_SAME:
				default:
					selected_hole = MI_HOLE_SAME;
			}
			mComboHoleType->setCurrentByIndex(selected_hole);
			hole_enabled = enabled;
		}
		else
		{
			mComboHoleType->setCurrentByIndex(MI_HOLE_SAME);
			hole_enabled = false;
		}

		// Cut interpretation varies based on base object type
		F32 cut_begin, cut_end, adv_cut_begin, adv_cut_end;

		if (selected_item == MI_SPHERE || selected_item == MI_TORUS ||
			 selected_item == MI_TUBE   || selected_item == MI_RING)
		{
			cut_begin		= begin_t;
			cut_end			= end_t;
			adv_cut_begin	= begin_s;
			adv_cut_end		= end_s;
		}
		else
		{
			cut_begin		= begin_s;
			cut_end			= end_s;
			adv_cut_begin   = begin_t;
			adv_cut_end		= end_t;
		}

		mSpinCutBegin->set(cut_begin);
		mSpinCutEnd->set(cut_end);
		mCtrlPathBegin->set(adv_cut_begin);
		mCtrlPathEnd->set(adv_cut_end);

		// Twist
		F32 twist = volume_params.getTwistEnd();
		F32 twist_begin = volume_params.getTwistBegin();
		// Check the path type for conversion.
		if (path == LL_PCODE_PATH_LINE || path == LL_PCODE_PATH_FLEXIBLE)
		{
			twist *= OBJECT_TWIST_LINEAR_MAX;
			twist_begin *= OBJECT_TWIST_LINEAR_MAX;
		}
		else
		{
			twist *= OBJECT_TWIST_MAX;
			twist_begin *= OBJECT_TWIST_MAX;
		}

		mSpinTwist->set(twist);
		mSpinTwistBegin->set(twist_begin);

		// Shear
		F32 shear_x = volume_params.getShearX();
		F32 shear_y = volume_params.getShearY();
		mSpinShearX->set(shear_x);
		mSpinShearY->set(shear_y);

		// Taper
		F32 taper_x	= volume_params.getTaperX();
		F32 taper_y = volume_params.getTaperY();
		mSpinTaperX->set(taper_x);
		mSpinTaperY->set(taper_y);

		// Radius offset.
		F32 radius_offset = volume_params.getRadiusOffset();
		// Limit radius offset, based on taper and hole size y.
		F32 radius_mag = fabs(radius_offset);
		F32 hole_y_mag = fabs(scale_y);
		F32 taper_y_mag  = fabs(taper_y);
		// Check to see if the taper effects us.
		if ((radius_offset > 0.f && taper_y < 0.f) ||
			(radius_offset < 0.f && taper_y > 0.f))
		{
			// The taper does not help increase the radius offset range.
			taper_y_mag = 0.f;
		}
		F32 max_radius_mag = 1.f - hole_y_mag * (1.f - taper_y_mag) /
							 (1.f - hole_y_mag);
		// Enforce the maximum magnitude.
		if (radius_mag > max_radius_mag)
		{
			// Check radius offset sign.
			if (radius_offset < 0.f)
			{
				radius_offset = -max_radius_mag;
			}
			else
			{
				radius_offset = max_radius_mag;
			}
		}
		mSpinRadiusOffset->set(radius_offset);

		// Revolutions
		F32 revolutions = volume_params.getRevolutions();
		mSpinRevolutions->set(revolutions);

		// Skew
		F32 skew = volume_params.getSkew();
		// Limit skew, based on revolutions hole size x.
		F32 skew_mag = fabs(skew);
		F32 min_skew_mag = 1.f - 1.f / (revolutions * scale_x + 1.f);
		// Discontinuity; A revolution of 1 allows skews below 0.5.
		if (fabsf(revolutions - 1.f) < 0.001f)
		{
			min_skew_mag = 0.f;
		}

		// Clip skew.
		if (skew_mag < min_skew_mag)
		{
			// Check skew sign.
			if (skew < 0.f)
			{
				skew = -min_skew_mag;
			}
			else
			{
				skew = min_skew_mag;
			}
		}
		mSpinSkew->set(skew);
	}

	// Compute control visibility, label names, and twist range.
	// Start with defaults.
	bool cut_visible				= true;
	bool hollow_visible				= true;
	bool top_size_x_visible			= true;
	bool top_size_y_visible			= true;
	bool top_shear_x_visible		= true;
	bool top_shear_y_visible		= true;
	bool twist_visible				= true;
	bool advanced_cut_visible		= false;
	bool taper_visible				= false;
	bool skew_visible				= false;
	bool radius_offset_visible		= false;
	bool revolutions_visible		= false;
	bool sculpt_texture_visible		= false;
	F32	 twist_min					= OBJECT_TWIST_LINEAR_MIN;
	F32	 twist_max					= OBJECT_TWIST_LINEAR_MAX;
	F32	 twist_inc					= OBJECT_TWIST_LINEAR_INC;
	bool advanced_is_dimple			= false;
	bool advanced_is_slice			= false;
	bool size_is_hole				= false;

	// Tune based on overall volume type
	switch (selected_item)
	{
		case MI_SPHERE:
			top_size_x_visible		= false;
			top_size_y_visible		= false;
			top_shear_x_visible		= false;
			top_shear_y_visible		= false;
			//twist_visible			= false;
			advanced_cut_visible	= true;
			advanced_is_dimple		= true;
			twist_min				= OBJECT_TWIST_MIN;
			twist_max				= OBJECT_TWIST_MAX;
			twist_inc				= OBJECT_TWIST_INC;
			break;

		case MI_TORUS:
		case MI_TUBE:
		case MI_RING:
			//top_size_x_visible	= false;
			//top_size_y_visible	= false;
		  	size_is_hole 			= true;
			skew_visible			= true;
			advanced_cut_visible	= true;
			taper_visible			= true;
			radius_offset_visible	= true;
			revolutions_visible		= true;
			twist_min				= OBJECT_TWIST_MIN;
			twist_max				= OBJECT_TWIST_MAX;
			twist_inc				= OBJECT_TWIST_INC;
			break;

		case MI_SCULPT:
			cut_visible				= false;
			hollow_visible			= false;
			twist_visible			= false;
			top_size_x_visible		= false;
			top_size_y_visible		= false;
			top_shear_x_visible		= false;
			top_shear_y_visible		= false;
			skew_visible			= false;
			advanced_cut_visible	= false;
			taper_visible			= false;
			radius_offset_visible	= false;
			revolutions_visible		= false;
			sculpt_texture_visible	= true;
			break;

		case MI_BOX:
			advanced_cut_visible	= true;
			advanced_is_slice		= true;
			break;

		case MI_CYLINDER:
			advanced_cut_visible	= true;
			advanced_is_slice		= true;
			break;

		case MI_PRISM:
			advanced_cut_visible	= true;
			advanced_is_slice		= true;
			break;

		default:
			break;
	}

	// Check if we need to change top size/hole size params.
	switch (selected_item)
	{
		case MI_SPHERE:
		case MI_TORUS:
		case MI_TUBE:
		case MI_RING:
			mSpinScaleX->set(scale_x);
			mSpinScaleY->set(scale_y);
			mSpinScaleX->setMinValue(OBJECT_MIN_HOLE_SIZE);
			mSpinScaleX->setMaxValue(OBJECT_MAX_HOLE_SIZE_X);
			mSpinScaleY->setMinValue(OBJECT_MIN_HOLE_SIZE);
			mSpinScaleY->setMaxValue(OBJECT_MAX_HOLE_SIZE_Y);
			break;

		default:
			if (editable && single_volume)
			{
				mSpinScaleX->set(1.f - scale_x);
				mSpinScaleY->set(1.f - scale_y);
				mSpinScaleX->setMinValue(-1.f);
				mSpinScaleX->setMaxValue(1.f);
				mSpinScaleY->setMinValue(-1.f);
				mSpinScaleY->setMaxValue(1.f);
			}
	}

	// Check if we need to limit the hollow based on the hole type.
	if (selected_hole == MI_HOLE_SQUARE &&
		(selected_item == MI_CYLINDER || selected_item == MI_TORUS ||
		 selected_item == MI_PRISM	|| selected_item == MI_RING  ||
		 selected_item == MI_SPHERE))
	{
		mSpinHollow->setMinValue(OBJECT_HOLLOW_MIN * 100.f);
		mSpinHollow->setMaxValue(OBJECT_HOLLOW_MAX_SQUARE * 100.f);
	}
	else
	{
		mSpinHollow->setMinValue(OBJECT_HOLLOW_MIN * 100.f);
		mSpinHollow->setMaxValue(OBJECT_HOLLOW_MAX * 100.f);
	}

	// Update field enablement
	mLabelBaseType->setEnabled(enabled);
	mComboBaseType->setEnabled(enabled);

	mLabelCut->setEnabled(enabled);
	mSpinCutBegin->setEnabled(enabled);
	mSpinCutEnd->setEnabled(enabled);

	mLabelHollow->setEnabled(enabled);
	mSpinHollow->setEnabled(enabled);
	mLabelHollowShape->setEnabled(hole_enabled);
	mComboHoleType->setEnabled(hole_enabled);

	mLabelTwist->setEnabled(enabled);
	mSpinTwist->setEnabled(enabled);
	mSpinTwistBegin->setEnabled(enabled);

	mLabelSkew->setEnabled(enabled);
	mSpinSkew->setEnabled(enabled);

	mLabelScaleHole->setVisible(false);
	mLabelScaleTaper->setVisible(false);
	if (top_size_x_visible || top_size_y_visible)
	{
		if (size_is_hole)
		{
			mLabelScaleHole->setVisible(true);
			mLabelScaleHole->setEnabled(enabled);
		}
		else
		{
			mLabelScaleTaper->setVisible(true);
			mLabelScaleTaper->setEnabled(enabled);
		}
	}
	mSpinScaleX->setEnabled(enabled);
	mSpinScaleY->setEnabled(enabled);

	mLabelShear->setEnabled(enabled);
	mSpinShearX->setEnabled(enabled);
	mSpinShearY->setEnabled(enabled);

	mLabelAdvancedCut->setVisible(false);
	mLabelAdvancedDimple->setVisible(false);
	mLabelAdvancedSlice->setVisible(false);

	if (advanced_cut_visible)
	{
		if (advanced_is_dimple)
		{
			mLabelAdvancedDimple->setVisible(true);
			mLabelAdvancedDimple->setEnabled(enabled);
		}
		else if (advanced_is_slice)
		{
			mLabelAdvancedSlice->setVisible(true);
			mLabelAdvancedSlice->setEnabled(enabled);
		}
		else
		{
			mLabelAdvancedCut->setVisible(true);
			mLabelAdvancedCut->setEnabled(enabled);
		}
	}

	mCtrlPathBegin->setEnabled(enabled);
	mCtrlPathEnd->setEnabled(enabled);

	mLabelTaper->setEnabled(enabled);
	mSpinTaperX->setEnabled(enabled);
	mSpinTaperY->setEnabled(enabled);

	mLabelRadiusOffset->setEnabled(enabled);
	mSpinRadiusOffset->setEnabled(enabled);

	mLabelRevolutions->setEnabled(enabled);
	mSpinRevolutions->setEnabled(enabled);

	mCheckCopyShape->setEnabled(enabled && mCheckCopyShape->getVisible());

	// Update field visibility
	mLabelCut->setVisible(cut_visible);
	mSpinCutBegin->setVisible(cut_visible);
	mSpinCutEnd->setVisible(cut_visible);

	mLabelHollow->setVisible(hollow_visible);
	mSpinHollow->setVisible(hollow_visible);
	mLabelHollowShape->setVisible(hollow_visible);
	mComboHoleType->setVisible(hollow_visible);

	mLabelTwist->setVisible(twist_visible);
	mSpinTwist->setVisible(twist_visible);
	mSpinTwistBegin->setVisible(twist_visible);
	mSpinTwist->setMinValue(twist_min);
	mSpinTwist->setMaxValue(twist_max);
	mSpinTwist->setIncrement(twist_inc);
	mSpinTwistBegin->setMinValue(twist_min);
	mSpinTwistBegin->setMaxValue(twist_max);
	mSpinTwistBegin->setIncrement(twist_inc);

	mSpinScaleX->setVisible(top_size_x_visible);
	mSpinScaleY->setVisible(top_size_y_visible);

	mLabelSkew->setVisible(skew_visible);
	mSpinSkew->setVisible(skew_visible);

	mLabelShear->setVisible(top_shear_x_visible || top_shear_y_visible);
	mSpinShearX->setVisible(top_shear_x_visible);
	mSpinShearY->setVisible(top_shear_y_visible);

	mCtrlPathBegin->setVisible(advanced_cut_visible);
	mCtrlPathEnd->setVisible(advanced_cut_visible);

	mLabelTaper->setVisible(taper_visible);
	mSpinTaperX->setVisible(taper_visible);
	mSpinTaperY->setVisible(taper_visible);

	mLabelRadiusOffset->setVisible(radius_offset_visible);
	mSpinRadiusOffset->setVisible(radius_offset_visible);

	mLabelRevolutions->setVisible(revolutions_visible);
	mSpinRevolutions->setVisible(revolutions_visible);

	// sculpt texture and parameters
	bool is_mesh = false;
	if (selected_item == MI_SCULPT)
	{
		LLUUID id;
		const LLSculptParams* sculpt_params = objectp->getSculptParams();
		if (sculpt_params)
		{
			// If we have a legal sculpt param block for this object:
			if (mObject != objectp)
			{
				 // We have just selected a new object, so save for undo
				mSculptTextureRevert = sculpt_params->getSculptTexture();
				mSculptTypeRevert = sculpt_params->getSculptType();
			}

			U8 sculpt_type = sculpt_params->getSculptType();
			U8 sculpt_stitching = sculpt_type & LL_SCULPT_TYPE_MASK;
			bool sculpt_invert = sculpt_type & LL_SCULPT_FLAG_INVERT;
			bool sculpt_mirror = sculpt_type & LL_SCULPT_FLAG_MIRROR;
			is_mesh = (sculpt_stitching == LL_SCULPT_TYPE_MESH);

			mCtrlSculptTexture->setTentative(false);
			mCtrlSculptTexture->setEnabled(editable && !is_mesh);
			mCtrlSculptTexture->setImageAssetID(editable ? sculpt_params->getSculptTexture()
														 : LLUUID::null);

			mComboBaseType->setEnabled(!is_mesh);

			mCtrlSculptType->setCurrentByIndex(sculpt_stitching);
			mCtrlSculptType->setEnabled(editable && !is_mesh);

			mCtrlSculptMirror->set(sculpt_mirror);
			mCtrlSculptMirror->setEnabled(editable && !is_mesh);

			mCtrlSculptInvert->set(sculpt_invert);
			mCtrlSculptInvert->setEnabled(editable && !is_mesh);

			mLabelSculptType->setEnabled(!is_mesh);
		}
	}
	else
	{
		mSculptTextureRevert.setNull();
	}
	mLabelSculptType->setVisible(sculpt_texture_visible && !is_mesh);
	mCtrlSculptType->setVisible(sculpt_texture_visible && !is_mesh);
	mCtrlSculptMirror->setVisible(sculpt_texture_visible && !is_mesh);
	mCtrlSculptInvert->setVisible(sculpt_texture_visible && !is_mesh);
	mCtrlSculptTexture->setVisible(sculpt_texture_visible && !is_mesh);

	if (selected_item == MI_SCULPT)
	{
		mCheckCopyShape->setVisible(false);
	}
	setCopyPasteState();

	mObject = objectp;
	mRootObject = root_objectp;
}

void LLPanelObject::setCopyPasteState()
{
	bool shape_enabled	= mCheckCopyShape->getVisible() &&
						  mCheckCopyShape->getEnabled();
	bool size_enabled	= mCheckCopySize->getVisible() &&
						  mCheckCopySize->getEnabled();
	bool pos_enabled	= mCheckCopyPos->getVisible() &&
						  mCheckCopyPos->getEnabled();
	bool rot_enabled	= mCheckCopyRot->getVisible() &&
						  mCheckCopyRot->getEnabled();

	mButtonCopy->setEnabled(shape_enabled || size_enabled ||
							pos_enabled || rot_enabled);

	bool shape_checked	= shape_enabled && mCheckCopyShape->get();
	bool size_checked	= size_enabled && mCheckCopySize->get();
	bool pos_checked	= pos_enabled && mCheckCopyPos->get();
	bool rot_checked	= rot_enabled && mCheckCopyRot->get();

	bool can_paste = (shape_checked && sSavedShapeValid) ||
					 (size_checked && sSavedSizeValid) ||
					 (pos_checked && sSavedPosValid) ||
					 (rot_checked && sSavedRotValid);

	if (!sSavedShapeValid && shape_checked)	can_paste = false;
	if (!sSavedSizeValid && size_checked)	can_paste = false;
	if (!sSavedPosValid && pos_checked)		can_paste = false;
	if (!sSavedRotValid && rot_checked)		can_paste = false;

	mButtonPaste->setEnabled(can_paste);
}

void LLPanelObject::sendIsTemporary()
{
	bool value = mCheckTemporary->get();
	if (mIsTemporary != value)
	{
		gSelectMgr.selectionUpdateTemporary(value);
		mIsTemporary = value;

		llinfos << "Update temporary state sent" << llendl;
	}
}

void LLPanelObject::sendIsPhantom()
{
	bool value = mCheckPhantom->get();
	if (mIsPhantom != value)
	{
		gSelectMgr.selectionUpdatePhantom(value);
		mIsPhantom = value;

		llinfos << "Update phantom sent" << llendl;
	}
}

//static
void LLPanelObject::onCommitParametric(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (!self) return;

	if (self->mObject.isNull())
	{
		return;
	}

	if (self->mObject->getPCode() != LL_PCODE_VOLUME)
	{
		// Do not allow modification of non-volume objects.
		return;
	}

	LLVolume *volume = self->mObject->getVolume();
	if (!volume)
	{
		return;
	}

	LLVolumeParams volume_params;
	self->getVolumeParams(volume_params);

	// set sculpting
	S32 selected_type = self->mComboBaseType->getCurrentIndex();

	if (selected_type == MI_SCULPT)
	{
		self->mObject->setParameterEntryInUse(LLNetworkData::PARAMS_SCULPT,
											  true, true);
		const LLSculptParams* sculpt_params = self->mObject->getSculptParams();
		if (sculpt_params)
		{
			volume_params.setSculptID(sculpt_params->getSculptTexture(),
									  sculpt_params->getSculptType());
		}
	}
	else
	{
		const LLSculptParams* sculpt_params = self->mObject->getSculptParams();
		if (sculpt_params)
		{
			self->mObject->setParameterEntryInUse(LLNetworkData::PARAMS_SCULPT,
												  false, true);
		}
	}

	// Update the volume, if necessary.
	self->mObject->updateVolume(volume_params);

	// This was added to make sure thate when changes are made, the UI adjusts
	// to present valid options. *FIX: only some changes, ie, hollow or
	// primitive type changes, require a refresh.
	self->refresh();
}

void LLPanelObject::getVolumeParams(LLVolumeParams& volume_params)
{
	// Figure out what type of volume to make
	S32 was_selected_type = mSelectedType;
	S32 selected_type = mComboBaseType->getCurrentIndex();
	U8 profile;
	U8 path;
	switch (selected_type)
	{
		case MI_CYLINDER:
			profile = LL_PCODE_PROFILE_CIRCLE;
			path = LL_PCODE_PATH_LINE;
			break;

		case MI_BOX:
			profile = LL_PCODE_PROFILE_SQUARE;
			path = LL_PCODE_PATH_LINE;
			break;

		case MI_PRISM:
			profile = LL_PCODE_PROFILE_EQUALTRI;
			path = LL_PCODE_PATH_LINE;
			break;

		case MI_SPHERE:
			profile = LL_PCODE_PROFILE_CIRCLE_HALF;
			path = LL_PCODE_PATH_CIRCLE;
			break;

		case MI_TORUS:
			profile = LL_PCODE_PROFILE_CIRCLE;
			path = LL_PCODE_PATH_CIRCLE;
			break;

		case MI_TUBE:
			profile = LL_PCODE_PROFILE_SQUARE;
			path = LL_PCODE_PATH_CIRCLE;
			break;

		case MI_RING:
			profile = LL_PCODE_PROFILE_EQUALTRI;
			path = LL_PCODE_PATH_CIRCLE;
			break;

		case MI_SCULPT:
			profile = LL_PCODE_PROFILE_CIRCLE;
			path = LL_PCODE_PATH_CIRCLE;
			break;

		default:
			llwarns << "Unknown base type " << selected_type
					<< " in getVolumeParams()" << llendl;
			// assume a box
			selected_type = MI_BOX;
			profile = LL_PCODE_PROFILE_SQUARE;
			path = LL_PCODE_PATH_LINE;
			break;
	}

	if (path == LL_PCODE_PATH_LINE)
	{
		LLVOVolume* volobjp = (LLVOVolume*)((LLViewerObject*)mObject);
		if (volobjp && volobjp->isFlexible())
		{
			path = LL_PCODE_PATH_FLEXIBLE;
		}
	}

	S32 selected_hole = mComboHoleType->getCurrentIndex();
	U8 hole;
	switch (selected_hole)
	{
		case MI_HOLE_CIRCLE:
			hole = LL_PCODE_HOLE_CIRCLE;
			break;
		case MI_HOLE_SQUARE:
			hole = LL_PCODE_HOLE_SQUARE;
			break;
		case MI_HOLE_TRIANGLE:
			hole = LL_PCODE_HOLE_TRIANGLE;
			break;
		case MI_HOLE_SAME:
		default:
			hole = LL_PCODE_HOLE_SAME;
			break;
	}

	volume_params.setType(profile | hole, path);
	mSelectedType = selected_type;

	// Compute cut start/end
	F32 cut_begin	= mSpinCutBegin->get();
	F32 cut_end		= mSpinCutEnd->get();

	// Make sure at least OBJECT_CUT_INC of the object survives
	if (cut_begin > cut_end - OBJECT_MIN_CUT_INC)
	{
		cut_begin = cut_end - OBJECT_MIN_CUT_INC;
		mSpinCutBegin->set(cut_begin);
	}

	F32 adv_cut_begin	= mCtrlPathBegin->get();
	F32 adv_cut_end		= mCtrlPathEnd->get();

	// Make sure at least OBJECT_CUT_INC of the object survives
	if (adv_cut_begin > adv_cut_end - OBJECT_MIN_CUT_INC)
	{
		adv_cut_begin = adv_cut_end - OBJECT_MIN_CUT_INC;
		mCtrlPathBegin->set(adv_cut_begin);
	}

	F32 begin_s, end_s;
	F32 begin_t, end_t;

	if (selected_type == MI_SPHERE || selected_type == MI_TORUS ||
		selected_type == MI_TUBE   || selected_type == MI_RING)
	{
		begin_s = adv_cut_begin;
		end_s	= adv_cut_end;

		begin_t = cut_begin;
		end_t	= cut_end;
	}
	else
	{
		begin_s = cut_begin;
		end_s	= cut_end;

		begin_t = adv_cut_begin;
		end_t	= adv_cut_end;
	}

	volume_params.setBeginAndEndS(begin_s, end_s);
	volume_params.setBeginAndEndT(begin_t, end_t);

	// Hollowness
	F32 hollow = mSpinHollow->get() / 100.f;

	if (hollow > 0.7f && selected_hole == MI_HOLE_SQUARE &&
		(selected_type == MI_CYLINDER || selected_type == MI_TORUS ||
		 selected_type == MI_PRISM	|| selected_type == MI_RING  ||
		 selected_type == MI_SPHERE))
	{
		hollow = 0.7f;
	}

	volume_params.setHollow(hollow);

	// Twist Begin,End
	F32 twist_begin = mSpinTwistBegin->get();
	F32 twist = mSpinTwist->get();
	// Check the path type for twist conversion.
	if (path == LL_PCODE_PATH_LINE || path == LL_PCODE_PATH_FLEXIBLE)
	{
		constexpr F32 linear_factor = 1.f / OBJECT_TWIST_LINEAR_MAX;
		twist_begin	*= linear_factor;
		twist *= linear_factor;
	}
	else
	{
		constexpr F32 factor = 1.f / OBJECT_TWIST_MAX;
		twist_begin	*= factor;
		twist *= factor;
	}

	volume_params.setTwistBegin(twist_begin);
	volume_params.setTwistEnd(twist);

	// Scale X,Y
	F32 scale_x = mSpinScaleX->get();
	F32 scale_y = mSpinScaleY->get();
	if (was_selected_type == MI_BOX || was_selected_type == MI_CYLINDER ||
		was_selected_type == MI_PRISM)
	{
		scale_x = 1.f - scale_x;
		scale_y = 1.f - scale_y;
	}

	// Skew
	F32 skew = mSpinSkew->get();

	// Taper X,Y
	F32 taper_x = mSpinTaperX->get();
	F32 taper_y = mSpinTaperY->get();

	// Radius offset
	F32 radius_offset = mSpinRadiusOffset->get();

	// Revolutions
	F32 revolutions	  = mSpinRevolutions->get();

	if (selected_type == MI_SPHERE)
	{
		// Snap values to valid sphere parameters.
		scale_x			= 1.f;
		scale_y			= 1.f;
		skew			= 0.f;
		taper_x			= 0.f;
		taper_y			= 0.f;
		radius_offset	= 0.f;
		revolutions		= 1.f;
	}
	else if (selected_type == MI_TORUS || selected_type == MI_TUBE ||
			 selected_type == MI_RING)
	{
		scale_x = llclamp(scale_x, OBJECT_MIN_HOLE_SIZE, OBJECT_MAX_HOLE_SIZE_X);
		scale_y = llclamp(scale_y, OBJECT_MIN_HOLE_SIZE, OBJECT_MAX_HOLE_SIZE_Y);

		// Limit radius offset, based on taper and hole size y.
		F32 radius_mag = fabs(radius_offset);
		F32 hole_y_mag = fabs(scale_y);
		F32 taper_y_mag = fabs(taper_y);
		// Check to see if the taper effects us.
		if ((radius_offset > 0.f && taper_y < 0.f) ||
			(radius_offset < 0.f && taper_y > 0.f))
		{
			// The taper does not help increase the radius offset range.
			taper_y_mag = 0.f;
		}
		F32 max_radius_mag = 1.f - hole_y_mag * (1.f - taper_y_mag) /
							 (1.f - hole_y_mag);
		// Enforce the maximum magnitude.
		if (radius_mag > max_radius_mag)
		{
			// Check radius offset sign.
			if (radius_offset < 0.f)
			{
				radius_offset = -max_radius_mag;
			}
			else
			{
				radius_offset = max_radius_mag;
			}
		}

		// Check the skew value against the revolutions.
		F32 skew_mag= fabs(skew);
		F32 min_skew_mag = 1.f - 1.f / (revolutions * scale_x + 1.f);
		// Discontinuity; A revolution of 1 allows skews below 0.5.
		if (fabsf(revolutions - 1.f) < 0.001f)
		{
			min_skew_mag = 0.f;
		}

		// Clip skew.
		if (skew_mag < min_skew_mag)
		{
			// Check skew sign.
			if (skew < 0.f)
			{
				skew = -min_skew_mag;
			}
			else
			{
				skew = min_skew_mag;
			}
		}
	}

	volume_params.setRatio(scale_x, scale_y);
	volume_params.setSkew(skew);
	volume_params.setTaper(taper_x, taper_y);
	volume_params.setRadiusOffset(radius_offset);
	volume_params.setRevolutions(revolutions);

	// Shear X,Y
	F32 shear_x = mSpinShearX->get();
	F32 shear_y = mSpinShearY->get();
	volume_params.setShear(shear_x, shear_y);

	if (selected_type == MI_SCULPT)
	{
		volume_params.setSculptID(LLUUID::null, 0);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setHollow(0.f);
		volume_params.setTwistBegin(0.f);
		volume_params.setTwistEnd(0.f);
		volume_params.setRatio(1.f, 0.5f);
		volume_params.setShear(0.f, 0.f);
		volume_params.setTaper(0.f, 0.f);
		volume_params.setRevolutions(1.f);
		volume_params.setRadiusOffset(0.f);
		volume_params.setSkew(0.f);
	}
}

// *FIXME: make it work with multiple objects
void LLPanelObject::sendRotation(bool btn_down)
{
	if (mObject.isNull()) return;

	LLVector3 new_rot(mCtrlRotX->get(), mCtrlRotY->get(), mCtrlRotZ->get());
	new_rot.mV[VX] = ll_round(new_rot.mV[VX], OBJECT_ROTATION_PRECISION);
	new_rot.mV[VY] = ll_round(new_rot.mV[VY], OBJECT_ROTATION_PRECISION);
	new_rot.mV[VZ] = ll_round(new_rot.mV[VZ], OBJECT_ROTATION_PRECISION);

	// Note: must compare before conversion to radians
	LLVector3 delta = new_rot - mCurEulerDegrees;

	if (delta.length() >= 0.00001f)
	{
		mCurEulerDegrees = new_rot;
		new_rot *= DEG_TO_RAD;

		LLQuaternion rotation;
		rotation.setEulerAngles(new_rot.mV[VX], new_rot.mV[VY],
								new_rot.mV[VZ]);

		if (mRootObject != mObject)
		{
			rotation = rotation * ~mRootObject->getRotationRegion();
		}
		// To include avatars into movements and rotation.
		// If false, all children are selected anyway, so move avatar.
		// If true, not all children are selected: save positions.
		static LLCachedControl<bool> edit_linked_parts(gSavedSettings,
													   "EditLinkedParts");
		std::vector<LLVector3>& child_positions =
			mObject->mUnselectedChildrenPositions;
		std::vector<LLQuaternion> child_rotations;
		if (mObject->isRootEdit() && edit_linked_parts)
		{
			mObject->saveUnselectedChildrenRotation(child_rotations);
			mObject->saveUnselectedChildrenPosition(child_positions);
		}

		mObject->setRotation(rotation);
		LLManip::rebuild(mObject);

		// For individually selected roots, we need to counterrotate all the
		// children
		if (mObject->isRootEdit() && edit_linked_parts)
		{
			mObject->resetChildrenRotationAndPosition(child_rotations,
													  child_positions);
		}

		if (!btn_down)
		{
			child_positions.clear();
			gSelectMgr.sendMultipleUpdate(UPD_ROTATION | UPD_POSITION);
		}
	}
}

// *FIXME: make it work with multiple objects
void LLPanelObject::sendScale(bool btn_down)
{
	if (mObject.isNull()) return;

	LLVector3 newscale(mCtrlScaleX->get(), mCtrlScaleY->get(),
					   mCtrlScaleZ->get());

	LLVector3 delta = newscale - mObject->getScale();
	if (delta.length() >= 0.00001f || (mSizeChanged && !btn_down))
	{
		// Scale changed by more than 1/2 millimeter
		mSizeChanged = btn_down;

		// Check to see if we are not scaling the textures (in which case the
		// tex coords need to be recomputed)
		bool dont_stretch_textures = !LLManipScale::getStretchTextures();
		if (dont_stretch_textures)
		{
			gSelectMgr.saveSelectedObjectTransform(SELECT_ACTION_TYPE_SCALE);
		}

		mObject->setScale(newscale, true);

		if (!btn_down)
		{
			gSelectMgr.sendMultipleUpdate(UPD_SCALE | UPD_POSITION);
		}

		gSelectMgr.adjustTexturesByScale(true, !dont_stretch_textures);
	}
}

void LLPanelObject::sendPosition(bool btn_down)
{
	if (mObject.isNull()) return;

	LLVector3 newpos(mCtrlPosX->get(), mCtrlPosY->get(), mCtrlPosZ->get());
	LLViewerRegion* regionp = mObject->getRegion();

	bool is_attachment = mObject->isAttachment();
	if (is_attachment)
	{
		if (newpos.length() > MAX_ATTACHMENT_DIST)
		{
			newpos.clampLength(MAX_ATTACHMENT_DIST);
			mCtrlPosX->set(newpos.mV[VX]);
			mCtrlPosY->set(newpos.mV[VY]);
			mCtrlPosZ->set(newpos.mV[VZ]);
		}
	}
	else
	{
		// Clamp the Z height
		const F32 height = newpos.mV[VZ];
		const F32 min_height = gWorld.getMinAllowedZ(mObject);
		constexpr F32 max_height = MAX_OBJECT_Z;

		if (height < min_height)
		{
			newpos.mV[VZ] = min_height;
			mCtrlPosZ->set(min_height);
		}
		else if (height > max_height)
		{
			newpos.mV[VZ] = max_height;
			mCtrlPosZ->set(max_height);
		}

		// Grass is always drawn on the ground, so clamp its position to the
		// ground
		if (mObject->getPCode() == LL_PCODE_LEGACY_GRASS)
		{
			mCtrlPosZ->set(gWorld.resolveLandHeightAgent(newpos) + 1.f);
		}
	}

	// Make sure new position is in a valid region, so the object would not get
	// dumped by the simulator.
	LLVector3d new_pos_global = regionp->getPosGlobalFromRegion(newpos);
	bool is_valid_pos;
	if (is_attachment)
	{
		LLVector3 delta_pos = mObject->getPositionEdit() - newpos;
		LLVector3d attachment_pos =
			regionp->getPosGlobalFromRegion(mObject->getPositionRegion() +
											delta_pos);
		is_valid_pos = gWorld.positionRegionValidGlobal(attachment_pos);
	}
	else
	{
		is_valid_pos = gWorld.positionRegionValidGlobal(new_pos_global);
	}
	if (is_valid_pos)
	{
		// Send only if the position is changed, that is, the delta vector is
		// not zero
		LLVector3d old_pos_global = mObject->getPositionGlobal();
		LLVector3d delta = new_pos_global - old_pos_global;
		// Moved more than 1/2 millimeter
		if (delta.length() >= 0.00001f)
		{
			if (mRootObject != mObject)
			{
				newpos = newpos - mRootObject->getPositionRegion();
				newpos = newpos * ~mRootObject->getRotationRegion();
				mObject->setPositionParent(newpos);
			}
			else
			{
				mObject->setPositionEdit(newpos);
			}

			LLManip::rebuild(mObject);

			// For individually selected roots, we need to counter-translate
			// all unselected children
			if (mObject->isRootEdit())
			{
				// Only offset by parent's translation
				mObject->resetChildrenPosition(LLVector3(-delta), true, true);
			}

			if (!btn_down)
			{
				gSelectMgr.sendMultipleUpdate(UPD_POSITION);
			}

			gSelectMgr.updateSelectionCenter();
		}
	}
	else
	{
		// Move failed, so we update the UI with the correct values
		LLVector3 vec = mRootObject->getPositionRegion();
		mCtrlPosX->set(vec.mV[VX]);
		mCtrlPosY->set(vec.mV[VY]);
		mCtrlPosZ->set(vec.mV[VZ]);
	}
}

void LLPanelObject::sendSculpt()
{
	if (mObject.isNull())
	{
		return;
	}

	const LLUUID& sculpt_id = mCtrlSculptTexture->getImageAssetID();

	U8 sculpt_type = 0;

	sculpt_type |= mCtrlSculptType->getCurrentIndex();

	bool enabled = sculpt_type != LL_SCULPT_TYPE_MESH;

	mCtrlSculptMirror->setEnabled(enabled);
	if (mCtrlSculptMirror->get())
	{
		sculpt_type |= LL_SCULPT_FLAG_MIRROR;
	}
	mCtrlSculptInvert->setEnabled(enabled);
	if (mCtrlSculptInvert->get())
	{
		sculpt_type |= LL_SCULPT_FLAG_INVERT;
	}

	LLSculptParams sculpt_params;
	sculpt_params.setSculptTexture(sculpt_id, sculpt_type);
	mObject->setParameterEntry(LLNetworkData::PARAMS_SCULPT, sculpt_params,
							   true);
}

//virtual
void LLPanelObject::refresh()
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

	if (mObject)
	{
		bool is_flora = LLPickInfo::isFlora(mObject);
		F32 max_scale = LLManipScale::maxPrimScale(is_flora);
		mCtrlScaleX->setMaxValue(max_scale);
		mCtrlScaleY->setMaxValue(max_scale);
		mCtrlScaleZ->setMaxValue(max_scale);
		F32 min_scale = LLManipScale::minPrimScale(is_flora);
		mCtrlScaleX->setMinValue(min_scale);
		mCtrlScaleY->setMinValue(min_scale);
		mCtrlScaleZ->setMinValue(min_scale);
	}
}

//virtual
void LLPanelObject::draw()
{
	static const LLColor4 white(1.f, 1.f, 1.f, 1.f);
	static const LLColor4 red(1.f, 0.25f, 0.f, 1.f);
	static const LLColor4 green(0.f, 1.f, 0.f, 1.f);
	static const LLColor4 blue(0.f, 0.5f, 1.f, 1.f);

	// Tune the colors of the labels
	LLTool* tool = gToolMgr.getCurrentTool();
	if (tool == &gToolCompTranslate)
	{
		mCtrlPosX->setLabelColor(red);
		mCtrlPosY->setLabelColor(green);
		mCtrlPosZ->setLabelColor(blue);

		mCtrlScaleX->setLabelColor(white);
		mCtrlScaleY->setLabelColor(white);
		mCtrlScaleZ->setLabelColor(white);

		mCtrlRotX->setLabelColor(white);
		mCtrlRotY->setLabelColor(white);
		mCtrlRotZ->setLabelColor(white);
	}
	else if (tool == &gToolCompScale)
	{
		mCtrlPosX->setLabelColor(white);
		mCtrlPosY->setLabelColor(white);
		mCtrlPosZ->setLabelColor(white);

		mCtrlScaleX->setLabelColor(red);
		mCtrlScaleY->setLabelColor(green);
		mCtrlScaleZ->setLabelColor(blue);

		mCtrlRotX->setLabelColor(white);
		mCtrlRotY->setLabelColor(white);
		mCtrlRotZ->setLabelColor(white);
	}
	else if (tool == &gToolCompRotate)
	{
		mCtrlPosX->setLabelColor(white);
		mCtrlPosY->setLabelColor(white);
		mCtrlPosZ->setLabelColor(white);

		mCtrlScaleX->setLabelColor(white);
		mCtrlScaleY->setLabelColor(white);
		mCtrlScaleZ->setLabelColor(white);

		mCtrlRotX->setLabelColor(red);
		mCtrlRotY->setLabelColor(green);
		mCtrlRotZ->setLabelColor(blue);
	}
	else
	{
		mCtrlPosX->setLabelColor(white);
		mCtrlPosY->setLabelColor(white);
		mCtrlPosZ->setLabelColor(white);

		mCtrlScaleX->setLabelColor(white);
		mCtrlScaleY->setLabelColor(white);
		mCtrlScaleZ->setLabelColor(white);

		mCtrlRotX->setLabelColor(white);
		mCtrlRotY->setLabelColor(white);
		mCtrlRotZ->setLabelColor(white);
	}

	LLPanel::draw();
}

//virtual
void LLPanelObject::clearCtrls()
{
	LLPanel::clearCtrls();

	mCheckLock->set(false);
	mCheckLock->setEnabled(false);
	mCheckTemporary->set(false);
	mCheckTemporary->setEnabled(false);
	mCheckPhantom->set(false);
	mCheckPhantom->setEnabled(false);
	// Disable text labels
	mLabelPosition->setEnabled(false);
	mLabelSize->setEnabled(false);
	mLabelRotation->setEnabled(false);
	mLabelBaseType->setEnabled(false);
	mLabelCut->setEnabled(false);
	mLabelHollow->setEnabled(false);
	mLabelHollowShape->setEnabled(false);
	mLabelTwist->setEnabled(false);
	mLabelSkew->setEnabled(false);
	mLabelShear->setEnabled(false);
	mLabelScaleTaper->setEnabled(false);
	mLabelRadiusOffset->setEnabled(false);
	mLabelRevolutions->setEnabled(false);

	mLabelSelectSingle->setVisible(false);
	mLabelEditObject->setVisible(true);
	mLabelEditObject->setEnabled(false);

	mLabelScaleHole->setEnabled(false);
	mLabelScaleTaper->setEnabled(false);
	mLabelAdvancedCut->setEnabled(false);
	mLabelAdvancedDimple->setEnabled(false);
	mLabelAdvancedSlice->setVisible(false);
}

//static
void LLPanelObject::onCommitLock(LLUICtrl* ctrl, void* userdata)
{
	// Checkbox will have toggled itself
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (!self || self->mRootObject.isNull())
	{
		return;
	}

	bool new_state = self->mCheckLock->get();
	gSelectMgr.selectionSetObjectPermissions(PERM_OWNER, !new_state,
											 PERM_MOVE | PERM_MODIFY);
}

//static
void LLPanelObject::onCommitPosition(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self && ctrl)
	{
		bool btn_down = ((LLSpinCtrl*)ctrl)->isMouseHeldDown();
		self->sendPosition(btn_down);
	}
}

//static
void LLPanelObject::onCommitScale(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self && ctrl)
	{
		bool btn_down = ((LLSpinCtrl*)ctrl)->isMouseHeldDown();
		self->sendScale(btn_down);
	}
}

//static
void LLPanelObject::onCommitRotation(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self && ctrl)
	{
		bool btn_down = ((LLSpinCtrl*)ctrl)->isMouseHeldDown();
		self->sendRotation(btn_down);
	}
}

//static
void LLPanelObject::onCommitTemporary(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self)
	{
		self->sendIsTemporary();
	}
}

//static
void LLPanelObject::onCommitPhantom(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self)
	{
		self->sendIsPhantom();
	}
}

//static
void LLPanelObject::onSelectSculpt(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self)
	{
		self->mSculptTextureRevert =
			self->mCtrlSculptTexture->getImageAssetID();
		self->sendSculpt();
	}
}

//static
void LLPanelObject::onCommitSculpt(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self)
	{
		self->sendSculpt();
	}
}

//static
bool LLPanelObject::onDropSculpt(LLUICtrl*, LLInventoryItem* item,
								 void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self)
	{
		LLUUID asset = item->getAssetUUID();
		self->mCtrlSculptTexture->setImageAssetID(asset);
		self->mSculptTextureRevert = asset;
	}
	return true;
}

//static
void LLPanelObject::onCancelSculpt(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self)
	{
		if (self->mSculptTextureRevert.isNull())
		{
			self->mSculptTextureRevert = LLUUID(SCULPT_DEFAULT_TEXTURE);
		}
		self->mCtrlSculptTexture->setImageAssetID(self->mSculptTextureRevert);
		self->sendSculpt();
	}
}

//static
void LLPanelObject::onCommitSculptType(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self)
	{
		self->sendSculpt();
	}
}

//static
void LLPanelObject::onCommitCopyPaste(LLUICtrl* ctrl, void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (self)
	{
		self->setCopyPasteState();
	}
}

//static
void LLPanelObject::onClickCopy(void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (!self) return;

	if (self->mCheckCopySize->getVisible() &&
		self->mCheckCopySize->getEnabled())
	{
		self->sSavedSize = LLVector3(self->mCtrlScaleX->get(),
									 self->mCtrlScaleY->get(),
									 self->mCtrlScaleZ->get());
		sSavedSizeValid = true;
	}
	else
	{
		sSavedSizeValid = false;
	}

	if (self->mCheckCopyPos->getVisible() && self->mCheckCopyPos->getEnabled())
	{
		self->sSavedPos = LLVector3(self->mCtrlPosX->get(),
									self->mCtrlPosY->get(),
									self->mCtrlPosZ->get());
		sSavedPosValid = true;
	}
	else
	{
		sSavedPosValid = false;
	}

	if (self->mCheckCopyRot->getVisible() && self->mCheckCopyRot->getEnabled())
	{
		self->sSavedRot = LLVector3(self->mCtrlRotX->get(),
									self->mCtrlRotY->get(),
									self->mCtrlRotZ->get());
		sSavedRotValid = true;
	}
	else
	{
		sSavedRotValid = false;
	}

	if (self->mCheckCopyShape->getVisible() &&
		self->mCheckCopyShape->getEnabled())
	{
		self->getVolumeParams(sSavedShape);
		sSavedShapeValid = true;
	}
	else
	{
		sSavedShapeValid = false;
	}

	self->setCopyPasteState();
}

//static
void LLPanelObject::onClickPaste(void* userdata)
{
	LLPanelObject* self = (LLPanelObject*)userdata;
	if (!self) return;

	if (sSavedSizeValid && self->mCheckCopySize->getVisible() &&
		self->mCheckCopySize->getEnabled() && self->mCheckCopySize->get())
	{
		self->mCtrlScaleX->set(sSavedSize.mV[VX]);
		self->mCtrlScaleY->set(sSavedSize.mV[VY]);
		self->mCtrlScaleZ->set(sSavedSize.mV[VZ]);
		self->sendScale(false);
	}

	if (sSavedPosValid && self->mCheckCopyPos->getVisible() &&
		self->mCheckCopyPos->getEnabled() && self->mCheckCopyPos->get())
	{
		LLVector3 newpos = sSavedPos;
		if (self->mObject->isAttachment() &&
			newpos.length() > MAX_ATTACHMENT_DIST)
		{
			newpos.clampLength(MAX_ATTACHMENT_DIST);
			llwarns << "Clamping pasted position " << sSavedPos
					<< " to fit attachment distance limit. New position is: "
					<< newpos << llendl;
		}
		self->mCtrlPosX->set(newpos.mV[VX]);
		self->mCtrlPosY->set(newpos.mV[VY]);
		self->mCtrlPosZ->set(newpos.mV[VZ]);
		self->sendPosition(false);
	}

	if (sSavedRotValid && self->mCheckCopyRot->getVisible() &&
		self->mCheckCopyRot->getEnabled() && self->mCheckCopyRot->get())
	{
		self->mCtrlRotX->set(sSavedRot.mV[VX]);
		self->mCtrlRotY->set(sSavedRot.mV[VY]);
		self->mCtrlRotZ->set(sSavedRot.mV[VZ]);
		self->sendRotation(false);
	}

	if (sSavedShapeValid && self->mCheckCopyShape->getVisible() &&
		self->mCheckCopyShape->getEnabled() && self->mCheckCopyShape->get())
	{
		self->mObject->updateVolume(sSavedShape);
	}
}
