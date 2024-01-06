/**
 * @file llpanelface.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llpanelface.h"

#include "indra_constants.h"				// For BLANK_MATERIAL_ASSET_ID
#include "llavatarappearancedefines.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llpluginclassmedia.h"
#include "llradiogroup.h"
#include "llspinctrl.h"
#include "llrender.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llcolorswatch.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "hbfloaterinvitemspicker.h"
#include "llfloatertools.h"
#include "llgltfmateriallist.h"
#include "lllocalgltfmaterials.h"
#include "llmaterialmgr.h"
#include "llpreviewmaterial.h"
#include "llselectmgr.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "lltooldraganddrop.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"

using namespace LLAvatarAppearanceDefines;

// Constant definitions for comboboxes
// Must match the UI elements definitions in floater_tools.xml
constexpr S32 MATTYPE_DIFFUSE = 0;		// Diffuse material texture
constexpr S32 MATTYPE_NORMAL = 1;		// Normal map
constexpr S32 MATTYPE_SPECULAR = 2;		// Specular map
constexpr S32 MATTYPE_PBR = 3;			// PBR GLTF material
constexpr S32 BUMPY_TEXTURE = 18;		// use supplied normal map
constexpr S32 SHINY_TEXTURE = 4;		// use supplied specular map
constexpr S32 ALPHAMODE_NONE = 0;		// No alpha mask applied
constexpr S32 ALPHAMODE_BLEND = 1;		// Alpha blending mode
constexpr S32 ALPHAMODE_MASK = 2;		// Alpha masking mode
constexpr S32 ALPHAMODE_EMISSIVE = 3;	// Emissive masking mode

//static
LLPanelFace* LLPanelFace::sInstance = NULL;

//
// Methods
//

LLPanelFace::LLPanelFace(const std::string& name)
:	LLPanel(name),
	mIsAlpha(false)
{
	sInstance = this;
}

LLPanelFace::~LLPanelFace()
{
	// Children all cleaned up by default view destructor.
	sInstance = NULL;
}

//virtual
bool LLPanelFace::postBuild()
{
	setMouseOpaque(false);

	// Face color label and swatch
	mLabelDiffuseColor = getChild<LLTextBox>("color_text");

	mColorSwatch = getChild<LLColorSwatchCtrl>("colorswatch");
	mColorSwatch->setCommitCallback(onCommitColor);
	mColorSwatch->setOnCancelCallback(onCancelColor);
	mColorSwatch->setOnSelectCallback(onSelectColor);
	mColorSwatch->setCallbackUserData(this);
	mColorSwatch->setCanApplyImmediately(false);

	// Face transparency
	mLabelColorTransp = getChild<LLTextBox>("color trans");

	mTransparency = getChild<LLSpinCtrl>("ColorTrans");
	mTransparency->setCommitCallback(onCommitAlpha);
	mTransparency->setCallbackUserData(this);
	mTransparency->setPrecision(0);

	// Face glow strength
	mGlow = getChild<LLSpinCtrl>("glow");
	mGlow->setCommitCallback(onCommitGlow);
	mGlow->setCallbackUserData(this);

	// Face full bright
	mCheckFullbright = getChild<LLCheckBoxCtrl>("checkbox fullbright");
	mCheckFullbright->setCommitCallback(onCommitFullbright);
	mCheckFullbright->setCallbackUserData(this);

	mButtonResetMaterial = getChild<LLButton>("reset_material");
	mButtonResetMaterial->setClickedCallback(onClickRemoveMaterial, this);
#if 0
	mButtonAlignMap = getChild<LLButton>("btn_align_map");
	mButtonAlignMap->setClickedCallback(onAlignTextureLayers, this);
#else
	childHide("btn_align_map");
#endif
	mLabelMaps = getChild<LLTextBox>("label maps");

	mMapsRadio = getChild<LLRadioGroup>("map_selector");
	mMapsRadio->setCommitCallback(onSelectMapType);
	mMapsRadio->setCallbackUserData(this);

	// PBR material buttons
	mButtonEditPBR = getChild<LLButton>("btn_edit_pbr");
	mButtonEditPBR->setClickedCallback(onClickEditPBR, this);
	mButtonLocalPBR = getChild<LLButton>("btn_local_pbr");
	mButtonLocalPBR->setClickedCallback(onClickLocalPBR, this);
	mButtonLoadPBR = getChild<LLButton>("btn_load_pbr");
	mButtonLoadPBR->setClickedCallback(onClickLoadPBR, this);
	mButtonSavePBR = getChild<LLButton>("btn_save_pbr");
	mButtonSavePBR->setClickedCallback(onClickSavePBR, this);

	std::string default_tex_id = gSavedSettings.getString("DefaultObjectTexture");
	mTextureCtrl = getChild<LLTextureCtrl>("texture control");
	mTextureCtrl->setDefaultImageAssetID(LLUUID(default_tex_id));
	mTextureCtrl->setCommitCallback(onCommitTexture);
	mTextureCtrl->setOnCancelCallback(onCancelTexture);
	mTextureCtrl->setOnSelectCallback(onSelectTexture);
	mTextureCtrl->setDragCallback(onDragTexture);
	mTextureCtrl->setCallbackUserData(this);
	mTextureCtrl->setCanApplyImmediately(false);

	// Diffuse map parameters

	mLabelAlphaMode = getChild<LLTextBox>("label alphamode");

	mComboAlphaMode = getChild<LLComboBox>("combobox alphamode");
	mComboAlphaMode->setCommitCallback(onCommitAlphaMode);
	mComboAlphaMode->setCallbackUserData(this);

	mLabelMaskCutoff = getChild<LLTextBox>("label maskcutoff");

	mMaskCutoff = getChild<LLSpinCtrl>("maskcutoff");
	mMaskCutoff->setCommitCallback(onCommitAlphaMaterial);
	mMaskCutoff->setCallbackUserData(this);

	// Normal map texture picker

	default_tex_id = gSavedSettings.getString("BlankNormalTexture");
	mNormalCtrl = getChild<LLTextureCtrl>("normal control");
	mNormalCtrl->setDefaultImageAssetID(LLUUID::null);
	mNormalCtrl->setBlankImageAssetID(LLUUID(default_tex_id));
	mNormalCtrl->setCommitCallback(onCommitNormalMap);
	mNormalCtrl->setOnSelectCallback(onCommitNormalMap);
	mNormalCtrl->setOnCancelCallback(NULL);
	mNormalCtrl->setDragCallback(onDragTexture);
	mNormalCtrl->setCallbackUserData(this);
	mNormalCtrl->setCanApplyImmediately(false);

	// Specular map texture picker and parameters

	mSpecularCtrl = getChild<LLTextureCtrl>("specular control");
	mSpecularCtrl->setDefaultImageAssetID(LLUUID::null);
	mSpecularCtrl->setCommitCallback(onCommitSpecularMap);
	mSpecularCtrl->setOnSelectCallback(onCommitSpecularMap);
	mSpecularCtrl->setOnCancelCallback(NULL);
	mSpecularCtrl->setDragCallback(onDragTexture);
	mSpecularCtrl->setCallbackUserData(this);
	mSpecularCtrl->setCanApplyImmediately(false);

	mLabelShinyColor = getChild<LLTextBox>("label shinycolor");

	mShinyColorSwatch = getChild<LLColorSwatchCtrl>("shinycolorswatch");
	mShinyColorSwatch->setCommitCallback(onCommitShinyColor);
	mShinyColorSwatch->setOnSelectCallback(onCommitShinyColor);
	mShinyColorSwatch->setOnCancelCallback(NULL);
	mShinyColorSwatch->setCallbackUserData(this);
	mShinyColorSwatch->setCanApplyImmediately(false);

	mLabelGlossiness = getChild<LLTextBox>("label glossiness");

	mGlossiness = getChild<LLSpinCtrl>("glossiness");
	mGlossiness->setCommitCallback(onCommitShinyMaterial);
	mGlossiness->setCallbackUserData(this);

	mLabelEnvironment = getChild<LLTextBox>("label environment");

	mEnvironment = getChild<LLSpinCtrl>("environment");
	mEnvironment->setCommitCallback(onCommitShinyMaterial);
	mEnvironment->setCallbackUserData(this);

	// Use texture element text for normal and specular combo boxes
	mUseTextureText = getString("use_texture");

	mLabelShininess = getChild<LLTextBox>("label shininess");

	mComboShininess = getChild<LLComboBox>("combobox shininess");
	mComboShininess->setCommitCallback(onCommitShiny);
	mComboShininess->setCallbackUserData(this);

	mLabelBumpiness = getChild<LLTextBox>("label bumpiness");

	mComboBumpiness = getChild<LLComboBox>("combobox bumpiness");
	mComboBumpiness->setCommitCallback(onCommitBump);
	mComboBumpiness->setCallbackUserData(this);

	// Default and Planar alignment

	mLabelTexGen = getChild<LLTextBox>("tex gen");

	mComboTexGen = getChild<LLComboBox>("combobox texgen");
	mComboTexGen->setCommitCallback(onCommitTexGen);
	mComboTexGen->setCallbackUserData(this);

	mCheckPlanarAlign = getChild<LLCheckBoxCtrl>("checkbox planar align");
	mCheckPlanarAlign->setCommitCallback(onCommitPlanarAlign);
	mCheckPlanarAlign->setCallbackUserData(this);

	// Repeats per face/meter, offset and rotation labels

	mLabelRepeats = getChild<LLTextBox>("rpt");
	mLabelTexScale = getChild<LLTextBox>("tex scale");
	mLabelTexScaleUnit = getChild<LLTextBox>("tex scale unit");
	mLabelTexScaleHoriz = getChild<LLTextBox>("tex scale horiz");
	mLabelTexScaleVert = getChild<LLTextBox>("tex scale vert");
	mLabelTexOffset = getChild<LLTextBox>("tex offset");
	mLabelTexOffsetHoriz = getChild<LLTextBox>("tex offset horiz");
	mLabelTexOffsetVert = getChild<LLTextBox>("tex offset vert");
	mLabelTexRotate = getChild<LLTextBox>("tex rotate");

	mRepeatsPerMeterText = getString("string repeats per meter");
	mRepeatsPerFaceText = getString("string repeats per face");

	// Repeats per meter spinner (used for all maps)

	mRepeats = getChild<LLSpinCtrl>("rptctrl");
	mRepeats->setCommitCallback(onCommitRepeatsPerMeter);
	mRepeats->setCallbackUserData(this);

	// Texture scale, offset and rotation

	mTexScaleU = getChild<LLSpinCtrl>("TexScaleU");
	mTexScaleU->setCommitCallback(onCommitTextureInfo);
	mTexScaleU->setCallbackUserData(this);

	mCheckTexFlipS = getChild<LLCheckBoxCtrl>("TexFlipS");
	mCheckTexFlipS->setCommitCallback(onCommitTextureInfo);
	mCheckTexFlipS->setCallbackUserData(this);

	mTexScaleV = getChild<LLSpinCtrl>("TexScaleV");
	mTexScaleV->setCommitCallback(onCommitTextureInfo);
	mTexScaleV->setCallbackUserData(this);

	mCheckTexFlipT = getChild<LLCheckBoxCtrl>("TexFlipT");
	mCheckTexFlipT->setCommitCallback(onCommitTextureInfo);
	mCheckTexFlipT->setCallbackUserData(this);

	mTexOffsetU = getChild<LLSpinCtrl>("TexOffsetU");
	mTexOffsetU->setCommitCallback(onCommitTextureInfo);
	mTexOffsetU->setCallbackUserData(this);

	mTexOffsetV = getChild<LLSpinCtrl>("TexOffsetV");
	mTexOffsetV->setCommitCallback(onCommitTextureInfo);
	mTexOffsetV->setCallbackUserData(this);

	mTexRot = getChild<LLSpinCtrl>("TexRot");
	mTexRot->setCommitCallback(onCommitTextureInfo);
	mTexRot->setCallbackUserData(this);

	// Normal map scale, offset and rotation

	mBumpyScaleU = getChild<LLSpinCtrl>("BumpyScaleU");
	mBumpyScaleU->setCommitCallback(onCommitBumpyMaterial);
	mBumpyScaleU->setCallbackUserData(this);

	mCheckBumpyFlipS = getChild<LLCheckBoxCtrl>("BumpyFlipS");
	mCheckBumpyFlipS->setCommitCallback(onCommitBumpyMaterial);
	mCheckBumpyFlipS->setCallbackUserData(this);

	mBumpyScaleV = getChild<LLSpinCtrl>("BumpyScaleV");
	mBumpyScaleV->setCommitCallback(onCommitBumpyMaterial);
	mBumpyScaleV->setCallbackUserData(this);

	mCheckBumpyFlipT = getChild<LLCheckBoxCtrl>("BumpyFlipT");
	mCheckBumpyFlipT->setCommitCallback(onCommitBumpyMaterial);
	mCheckBumpyFlipT->setCallbackUserData(this);

	mBumpyOffsetU = getChild<LLSpinCtrl>("BumpyOffsetU");
	mBumpyOffsetU->setCommitCallback(onCommitBumpyMaterial);
	mBumpyOffsetU->setCallbackUserData(this);

	mBumpyOffsetV = getChild<LLSpinCtrl>("BumpyOffsetV");
	mBumpyOffsetV->setCommitCallback(onCommitBumpyMaterial);
	mBumpyOffsetV->setCallbackUserData(this);

	mBumpyRot = getChild<LLSpinCtrl>("BumpyRot");
	mBumpyRot->setCommitCallback(onCommitBumpyMaterial);
	mBumpyRot->setCallbackUserData(this);

	// Specular map scale, offset and rotation

	mShinyScaleU = getChild<LLSpinCtrl>("ShinyScaleU");
	mShinyScaleU->setCommitCallback(onCommitShinyMaterial);
	mShinyScaleU->setCallbackUserData(this);

	mCheckShinyFlipS = getChild<LLCheckBoxCtrl>("ShinyFlipS");
	mCheckShinyFlipS->setCommitCallback(onCommitShinyMaterial);
	mCheckShinyFlipS->setCallbackUserData(this);

	mShinyScaleV = getChild<LLSpinCtrl>("ShinyScaleV");
	mShinyScaleV->setCommitCallback(onCommitShinyMaterial);
	mShinyScaleV->setCallbackUserData(this);

	mCheckShinyFlipT = getChild<LLCheckBoxCtrl>("ShinyFlipT");
	mCheckShinyFlipT->setCommitCallback(onCommitShinyMaterial);
	mCheckShinyFlipT->setCallbackUserData(this);

	mShinyOffsetU = getChild<LLSpinCtrl>("ShinyOffsetU");
	mShinyOffsetU->setCommitCallback(onCommitShinyMaterial);
	mShinyOffsetU->setCallbackUserData(this);

	mShinyOffsetV = getChild<LLSpinCtrl>("ShinyOffsetV");
	mShinyOffsetV->setCommitCallback(onCommitShinyMaterial);
	mShinyOffsetV->setCallbackUserData(this);

	mShinyRot = getChild<LLSpinCtrl>("ShinyRot");
	mShinyRot->setCommitCallback(onCommitShinyMaterial);
	mShinyRot->setCallbackUserData(this);

	// PBR material scale, offset and rotation

	mPbrScaleU = getChild<LLSpinCtrl>("PbrScaleU");
	mPbrScaleU->setCommitCallback(onCommitPbrMaterial);
	mPbrScaleU->setCallbackUserData(this);

	mPbrScaleV = getChild<LLSpinCtrl>("PbrScaleV");
	mPbrScaleV->setCommitCallback(onCommitPbrMaterial);
	mPbrScaleV->setCallbackUserData(this);

	mPbrOffsetU = getChild<LLSpinCtrl>("PbrOffsetU");
	mPbrOffsetU->setCommitCallback(onCommitPbrMaterial);
	mPbrOffsetU->setCallbackUserData(this);

	mPbrOffsetV = getChild<LLSpinCtrl>("PbrOffsetV");
	mPbrOffsetV->setCommitCallback(onCommitPbrMaterial);
	mPbrOffsetV->setCallbackUserData(this);

	mPbrRot = getChild<LLSpinCtrl>("PbrRot");
	mPbrRot->setCommitCallback(onCommitPbrMaterial);
	mPbrRot->setCallbackUserData(this);

	// Media stuff
	// *TODO: move the face-related media stuff from llfloatertools.cpp to here

	mLabelMedia = getChild<LLTextBox>("media label");

	mButtonAlignMedia = getChild<LLButton>("button align");
	mButtonAlignMedia->setClickedCallback(onClickAutoFix, this);

	clearCtrls();

	return true;
}

struct LLPanelFaceSetTEFunctor final : public LLSelectedTEFunctor
{
	LLPanelFaceSetTEFunctor(LLPanelFace* panelp)
	:	mPanel(panelp)
	{
	}

	bool apply(LLViewerObject* objectp, S32 te) override;

private:
	LLPanelFace* mPanel;
};

// Functor that aligns a face to mCenterFace
struct LLPanelFaceSetAlignedTEFunctor final : public LLSelectedTEFunctor
{
	LLPanelFaceSetAlignedTEFunctor(LLPanelFace* panelp, LLFace* center_facep,
								   S32 map = -1)
	:	mPanel(panelp),
		mCenterFace(center_facep),
		mMap(map)
	{
	}

	template<void (LLMaterial::*MaterialEditFunc)(F32 data)>
	struct LLMaterialEditFunctor
	{
		LLMaterialEditFunctor(const F32& data)
		:	mData(data)
		{
		}

		void apply(LLMaterialPtr& matp)
		{
			(matp->*(MaterialEditFunc))(mData);
		}

		F32 mData;
	};

	// Updates material parameters by applying 'MaterialEditFunc' to selected
	// TEs
	template<void (LLMaterial::*MaterialEditFunc)(F32 data)>
	static void edit(LLPanelFace* p, F32 data, S32 te = -1,
					 const LLUUID& only_for_obj_id = LLUUID::null)
	{
		LLMaterialEditFunctor<MaterialEditFunc> edit(data);
		struct LLSelectedTEEditMaterial : public LLSelectedTEMaterialFunctor
		{
			LLSelectedTEEditMaterial(LLPanelFace* panel,
									 LLMaterialEditFunctor<MaterialEditFunc>* editp,
									 const LLUUID& only_for_obj_id)
			:	mPanelFace(panel),
				mEdit(editp),
				mOnlyForObjId(only_for_obj_id)
			{
			}

			LLMaterialPtr apply(LLViewerObject* objectp, S32 face,
								LLTextureEntry* tep,
								LLMaterialPtr& curmatp) override
			{
				if (!mEdit)
				{
					return NULL;
				}

				if (mOnlyForObjId.notNull() &&
					mOnlyForObjId != objectp->getID())
				{
					return NULL;
				}

				LLMaterialPtr newmatp =
					mPanelFace->createDefaultMaterial(curmatp);
				if (!newmatp)
				{
					return NULL;
				}

				// Determine correct alpha mode for current diffuse texture
				// (i.e. does it have an alpha channel that makes alpha mode
				// useful)
				//
				// mPanelFace->isAlpha() "lies" when one face has alpha and
				// the rest do not (NORSPEC-329) need to get per-face answer to
				// this question for sane alpha mode retention on updates.
				bool is_alpha_face = objectp->isImageAlphaBlended(face);

				// Need to keep this original answer for valid comparisons in
				// logic below
				U8 orig_deflt_alpha_mode =
					is_alpha_face ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND
								  : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

				U8 dflt_alpha_mode =
					curmatp.isNull() ? orig_deflt_alpha_mode
									 : curmatp->getDiffuseAlphaMode();

				// Ensure we do not inherit the default of blend by accident;
				// this will be stomped by a legit request to change the alpha
				// mode by the apply() below.
				newmatp->setDiffuseAlphaMode(dflt_alpha_mode);

				// Apply change
				mEdit->apply(newmatp);

				U32 new_alpha_mode = newmatp->getDiffuseAlphaMode();
				if (!is_alpha_face &&
					new_alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND)
				{
					new_alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
					newmatp->setDiffuseAlphaMode(new_alpha_mode);
				}

				const LLUUID& obj_id = objectp->getID();
				if (new_alpha_mode != orig_deflt_alpha_mode ||
					newmatp->getNormalID().notNull() ||
					newmatp->getSpecularID().notNull())
				{
					LL_DEBUGS("Materials") << "Putting material on object "
										   << obj_id << " - Face " << face
										   << " - Material: "
										   << newmatp->asLLSD() << LL_ENDL;
					LLMaterialMgr::getInstance()->put(obj_id, face, *newmatp);
				}
				else
				{
					LL_DEBUGS("Materials") << "Removing material from object "
										   << obj_id << " - Face " << face
										   << LL_ENDL;
					LLMaterialMgr::getInstance()->remove(obj_id, face);
					newmatp = NULL;
				}

				objectp->setTEMaterialParams(face, newmatp);

				return newmatp;
			}

			LLMaterialEditFunctor<MaterialEditFunc>*	mEdit;
			LLPanelFace*								mPanelFace;
			LLUUID										mOnlyForObjId;
		} editor(p, &edit, only_for_obj_id);

		gSelectMgr.selectionSetMaterialParams(&editor, te);
	}

	LL_INLINE static void setNormalOffsetX(LLPanelFace* panel, F32 data,
										   S32 te = -1,
										   const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setNormalOffsetX>(panel, data, te, obj_id);
	}

	LL_INLINE static void setNormalOffsetY(LLPanelFace* panel, F32 data,
										   S32 te = -1,
										   const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setNormalOffsetY>(panel, data, te, obj_id);
	}

	LL_INLINE static void setNormalRepeatX(LLPanelFace* panel, F32 data,
										   S32 te = -1,
										   const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setNormalRepeatX>(panel, data, te, obj_id);
	}

	LL_INLINE static void setNormalRepeatY(LLPanelFace* panel, F32 data,
										   S32 te = -1,
										   const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setNormalRepeatY>(panel, data, te, obj_id);
	}

	LL_INLINE static void setSpecularOffsetX(LLPanelFace* panel, F32 data,
											 S32 te = -1,
											 const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setSpecularOffsetX>(panel, data, te, obj_id);
	}

	LL_INLINE static void setSpecularOffsetY(LLPanelFace* panel, F32 data,
											 S32 te = -1,
											 const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setSpecularOffsetY>(panel, data, te, obj_id);
	}

	LL_INLINE static void setSpecularRepeatX(LLPanelFace* panel, F32 data,
											 S32 te = -1,
											 const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setSpecularRepeatX>(panel, data, te, obj_id);
	}

	LL_INLINE static void setSpecularRepeatY(LLPanelFace* panel, F32 data,
											 S32 te = -1,
											 const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setSpecularRepeatY>(panel, data, te, obj_id);
	}

	LL_INLINE static void setNormalRotation(LLPanelFace* panel, F32 data,
											S32 te = -1,
											const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setNormalRotation>(panel, data, te, obj_id);
	}

	LL_INLINE static void setSpecularRotation(LLPanelFace* panel, F32 data,
											  S32 te = -1,
											  const LLUUID& obj_id = LLUUID::null)
	{
		edit<&LLMaterial::setSpecularRotation>(panel, data, te, obj_id);
	}

	bool apply(LLViewerObject* objectp, S32 te) override
	{
		LLFace* facep = objectp->mDrawable->getFace(te);
		if (!facep || !facep->getViewerObject() ||
			!facep->getViewerObject()->getVolume() ||
			facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
		{
			// Volume face does not exist, cannot be aligned
			return true;
		}

		bool set_aligned = facep != mCenterFace;
		if (set_aligned)
		{
			LLVector2 uv_offset, uv_scale;
			F32 uv_rot;
			S32 map = mMap >= 0 ? mMap : LLRender::DIFFUSE_MAP;
			set_aligned = facep->calcAlignedPlanarTE(mCenterFace, &uv_offset,
													 &uv_scale, &uv_rot, map);
			if (set_aligned)
			{
				const LLUUID& obj_id = objectp->getID();
				F32 offset_x = uv_offset.mV[VX];
				F32 offset_y = uv_offset.mV[VY];
				if (mMap == -1 || mMap == LLRender::NORMAL_MAP)
				{
					setNormalOffsetX(mPanel, offset_x, te, obj_id);
					setNormalOffsetY(mPanel, offset_y, te, obj_id);
					setNormalRotation(mPanel, uv_rot, te, obj_id);
				}
				if (mMap == -1 || mMap == LLRender::SPECULAR_MAP)
				{
					setSpecularOffsetX(mPanel, offset_x, te, obj_id);
					setSpecularOffsetY(mPanel, offset_y, te, obj_id);
					setSpecularRotation(mPanel, uv_rot, te, obj_id);
				}
				if (mMap == -1 || mMap == LLRender::DIFFUSE_MAP)
				{
					objectp->setTEOffset(te, offset_x, offset_y);
					objectp->setTEScale(te, uv_scale.mV[VX], uv_scale.mV[VY]);
					objectp->setTERotation(te, uv_rot);
				}
			}
		}
		if (mMap == -1 && !set_aligned)
		{
			LLPanelFaceSetTEFunctor setfunc(mPanel);
			setfunc.apply(objectp, te);
		}
		return true;
	}

private:
	LLPanelFace*	mPanel;
	LLFace*			mCenterFace;
	S32				mMap;
};

//virtual
bool LLPanelFaceSetTEFunctor::apply(LLViewerObject* object, S32 te)
{
	F32 value;
	bool align_planar = mPanel->getPlanarAlign()->get();
	LLComboBox* texgen = mPanel->getComboTexGen();

	LLSpinCtrl*	spinctrl = mPanel->getTexScaleU();
	if (align_planar || !spinctrl->getTentative())
	{
		value = spinctrl->get();
		if (mPanel->getTexFlipS()->get())
		{
			value = -value;
		}
		if (texgen->getCurrentIndex() == 1)
		{
			value *= 0.5f;
		}
		object->setTEScaleS(te, value);

		if (align_planar)
		{
			LLPanelFaceSetAlignedTEFunctor::setNormalRepeatX(mPanel, value,
															 te);
			LLPanelFaceSetAlignedTEFunctor::setSpecularRepeatX(mPanel, value,
															   te);
		}
	}

	spinctrl = mPanel->getTexScaleV();
	if (align_planar || !spinctrl->getTentative())
	{
		value = spinctrl->get();
		if (mPanel->getTexFlipT()->get())
		{
			value = -value;
		}
		if (texgen->getCurrentIndex() == 1)
		{
			value *= 0.5f;
		}
		object->setTEScaleT(te, value);

		if (align_planar)
		{
			LLPanelFaceSetAlignedTEFunctor::setNormalRepeatY(mPanel, value,
															 te);
			LLPanelFaceSetAlignedTEFunctor::setSpecularRepeatY(mPanel, value,
															   te);
		}
	}

	spinctrl = mPanel->getTexOffsetU();
	if (align_planar || !spinctrl->getTentative())
	{
		value = spinctrl->get();
		object->setTEOffsetS(te, value);

		if (align_planar)
		{
			LLPanelFaceSetAlignedTEFunctor::setNormalOffsetX(mPanel, value,
															 te);
			LLPanelFaceSetAlignedTEFunctor::setSpecularOffsetX(mPanel, value,
															   te);
		}
	}

	spinctrl = mPanel->getTexOffsetV();
	if (align_planar || !spinctrl->getTentative())
	{
		value = spinctrl->get();
		object->setTEOffsetT(te, value);

		if (align_planar)
		{
			LLPanelFaceSetAlignedTEFunctor::setNormalOffsetY(mPanel, value,
															 te);
			LLPanelFaceSetAlignedTEFunctor::setSpecularOffsetY(mPanel, value,
															   te);
		}
	}

	spinctrl = mPanel->getTexRot();
	if (align_planar || !spinctrl->getTentative())
	{
		value = spinctrl->get() * DEG_TO_RAD;
		object->setTERotation(te, value);

		if (align_planar)
		{
			LLPanelFaceSetAlignedTEFunctor::setNormalRotation(mPanel, value,
															  te);
			LLPanelFaceSetAlignedTEFunctor::setSpecularRotation(mPanel, value,
																te);
		}
	}
	return true;
}

// Functor that tests if a face is aligned to mCenterFace
struct LLPanelFaceGetIsAlignedTEFunctor final : public LLSelectedTEFunctor
{
	LLPanelFaceGetIsAlignedTEFunctor(LLFace* center_facep)
	:	mCenterFace(center_facep)
	{
	}

	bool apply(LLViewerObject* objectp, S32 te) override
	{
		LLFace* facep = objectp->mDrawable->getFace(te);
		if (!facep || !facep->getViewerObject() ||
			!facep->getViewerObject()->getVolume() ||
			facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
		{
			// Volume face does not exist, cannot be aligned
			return true;
		}

		if (facep == mCenterFace)
		{
			return true;
		}

		LLVector2 aligned_st_offset, aligned_st_scale;
		F32 aligned_st_rot;
		if (facep->calcAlignedPlanarTE(mCenterFace, &aligned_st_offset,
									   &aligned_st_scale, &aligned_st_rot))
		{
			const LLTextureEntry* tep = facep->getTextureEntry();
			if (!tep) return false;
			LLVector2 st_offset, st_scale;
			tep->getOffset(&st_offset.mV[VX], &st_offset.mV[VY]);
			tep->getScale(&st_scale.mV[VX], &st_scale.mV[VY]);
			F32 st_rot = tep->getRotation();
			// Needs a fuzzy comparison, because of FP errors
			if (is_approx_equal_fraction(st_offset.mV[VX],
										 aligned_st_offset.mV[VX], 12) &&
				is_approx_equal_fraction(st_offset.mV[VY],
										 aligned_st_offset.mV[VY], 12) &&
				is_approx_equal_fraction(st_scale.mV[VX],
										 aligned_st_scale.mV[VX], 12) &&
				is_approx_equal_fraction(st_scale.mV[VY],
										 aligned_st_scale.mV[VY], 12) &&
				is_approx_equal_fraction(st_rot, aligned_st_rot, 6))
			{
				return true;
			}
		}
		return false;
	}

private:
	LLFace* mCenterFace;
};

struct LLPanelFaceSendFunctor final : public LLSelectedObjectFunctor
{
	bool apply(LLViewerObject* objectp) override
	{
		objectp->sendTEUpdate();
		return true;
	}
};

//virtual
void LLPanelFace::refresh()
{
	getState();
}

LLFace* LLPanelFace::getLastSelectedFace()
{
	struct get_last_face_func final : public LLSelectedTEGetFunctor<LLFace*>
	{
		LLFace* get(LLViewerObject* objectp, S32 te) override
		{
			LLDrawable* drawablep = objectp->mDrawable;
			return drawablep ? drawablep->getFace(te) : NULL;
		}
	} func;

	LLFace* last_facep = NULL;
	gSelectMgr.getSelection()->getSelectedTEValue(&func, last_facep);
	return last_facep;
}

void LLPanelFace::getState()
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	LLViewerObject* objectp = selection->getFirstObject();
	if (objectp && objectp->getPCode() == LL_PCODE_VOLUME &&
		(objectp->permModify() || gAgent.isGodlikeWithoutAdminMenuFakery()))
	{
		bool is_attachment = objectp->isAttachment();
		bool editable = objectp->permModify() &&
						!objectp->isPermanentEnforced();

		mLabelMaps->setEnabled(editable);
		mMapsRadio->setEnabled(editable);

		// Do we have PBR mat support ?
		bool has_pbr_mat = gAgent.hasInventoryMaterial();
		mButtonLocalPBR->setEnabled(editable && has_pbr_mat);
		mButtonLoadPBR->setEnabled(editable && has_pbr_mat);

		bool identical;
		// Any PBR material on selected faces ?
		if (has_pbr_mat)
		{
			struct pbr_id_get final : public LLSelectedTEGetFunctor<LLUUID>
			{
				LLUUID get(LLViewerObject* objectp, S32 face) override
				{
					return objectp->getRenderMaterialID(face);
				}
			} func;
			LLUUID pbr_id;
			identical = selection->getSelectedTEValue(&func, pbr_id);
			has_pbr_mat = pbr_id.notNull();
		}
		mButtonEditPBR->setEnabled(editable && has_pbr_mat &&
								   LLPreviewMaterial::canModifyObjectsMaterial());
		mButtonSavePBR->setEnabled(editable && has_pbr_mat &&
								   LLPreviewMaterial::canSaveObjectsMaterial());

		// Texture
		bool identical_diffuse;
		LLUUID id;
		{
			struct tex_get final : public LLSelectedTEGetFunctor<LLUUID>
			{
				LLUUID get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					const LLUUID& te_id = tep ? tep->getID() : LLUUID::null;
					if (tep &&
						LLAvatarAppearanceDictionary::isBakedImageId(te_id))
					{
						return te_id;
					}
					LLViewerTexture* imagep = objectp->getTEImage(face);
					const LLUUID& id = imagep ? imagep->getID() : LLUUID::null;
					if (id.notNull() && te_id.notNull() &&
						LLViewerMedia::textureHasMedia(id))
					{
						LLViewerTexture* texp = gTextureList.findImage(te_id);
						if (!texp)
						{
							texp = LLViewerFetchedTexture::sDefaultImagep;
						}
						if (texp)
						{
							return texp->getID();
						}
					}
					return id;
				}
			} func;
			identical_diffuse = selection->getSelectedTEValue(&func, id);
		}
		mTextureCtrl->setTentative(!identical_diffuse);
		mTextureCtrl->setEnabled(editable);
		mTextureCtrl->setImageAssetID(id);
		mTextureCtrl->setBakeTextureEnabled(editable);
		if (is_attachment)
		{
			mTextureCtrl->setImmediateFilterPermMask(PERM_COPY |
													 PERM_TRANSFER);
		}
		else
		{
			mTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		}

		// Only turn on auto-align button if there is a media renderer and the
		// media is loaded
		bool has_media = LLViewerMedia::textureHasMedia(id);
		mButtonAlignMedia->setEnabled(editable && has_media);
		mLabelMedia->setEnabled(editable);

		LLAggregatePermissions texture_perms;
		if (gSelectMgr.selectGetAggregateTexturePermissions(texture_perms))
		{
			bool can_copy = texture_perms.getValue(PERM_COPY) ==
								LLAggregatePermissions::AP_EMPTY ||
							texture_perms.getValue(PERM_COPY) ==
								LLAggregatePermissions::AP_ALL;
			bool can_transfer = texture_perms.getValue(PERM_TRANSFER) ==
									LLAggregatePermissions::AP_EMPTY ||
								texture_perms.getValue(PERM_TRANSFER) ==
									LLAggregatePermissions::AP_ALL;
			mTextureCtrl->setCanApplyImmediately(can_copy && can_transfer);
		}
		else
		{
			mTextureCtrl->setCanApplyImmediately(false);
		}

		// Color swatch
		LLColor4 color = LLColor4::white;
		{
			struct color_get final : public LLSelectedTEGetFunctor<LLColor4>
			{
				LLColor4 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getColor() : LLColor4::white;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, color);
		}
		mLabelDiffuseColor->setEnabled(editable);
		LLColor4 prev_color = mColorSwatch->get();
		mColorSwatch->setOriginal(color);
		mColorSwatch->set(color, !editable || prev_color != color);
		mColorSwatch->setValid(editable);
		mColorSwatch->setEnabled(editable);
		mColorSwatch->setCanApplyImmediately(editable);
		mColorSwatch->setFallbackImageName("materials_ui_x_24.png");

		// Transparency
		mLabelColorTransp->setEnabled(editable && !has_pbr_mat);

		F32 transparency = (1.f - color.mV[VALPHA]) * 100.f;
		mTransparency->setValue(editable ? transparency : 0.f);
		mTransparency->setEnabled(editable);

		// Alpha channel
		GLenum image_format = 0;
		{
			struct image_format_get final
			:	public LLSelectedTEGetFunctor<GLenum>
			{
				GLenum get(LLViewerObject* objectp, S32 face) override
				{
					GLenum image_format = GL_RGB;
					LLViewerTexture* imagep = objectp->getTEImage(face);
					if (imagep)
					{
						image_format = imagep->getPrimaryFormat();
					}
					return image_format;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, image_format);
		}
		switch (image_format)
		{
			case GL_RGBA:
			case GL_ALPHA:
			{
				mIsAlpha = true;
				break;
			}

			case GL_RGB:
			{
				mIsAlpha = false;
				break;
			}

			default:
			{
				llwarns << "Unexpected texture format: resorting to no alpha."
						<< llendl;
				mIsAlpha = false;
			}
		}

		// Alpha mode
		U8 alpha_mode;
		{
			struct alpha_get final : public LLSelectedTEGetFunctor<U8>
			{
				U8 get(LLViewerObject* objectp, S32 te_index) override
				{
					U8 ret = 1;
					LLTextureEntry* tep = objectp->getTE(te_index);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						ret = matp->getDiffuseAlphaMode();
					}
					return ret;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, alpha_mode);
		}
		if (transparency > 0.f || has_pbr_mat)
		{
			// It is invalid to have any alpha mode other than blend if
			// transparency is greater than zero or a PBR material is
			// present...
			// Note: alpha blend with PBR material only works for 0% and 100%
			// transparency values (anything below 100% behaves like 0%). HB
			alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
		}
		if (!mIsAlpha || has_pbr_mat)
		{
			// ... unless there is no alpha channel in the texture, in which
			// case alpha mode MUST be none.
			alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
		}
		mComboAlphaMode->setCurrentByIndex(alpha_mode);
		mComboAlphaMode->setTentative(!identical);
		mComboAlphaMode->setEnabled(!has_pbr_mat);
		updateAlphaControls();

		// Normal map (and legacy material presence)
		bool has_material = false;
        bool identical_norm;
		LLUUID normmap_id;
		{
			struct norm_get final : public LLSelectedTEGetFunctor<LLUUID>
			{
				LLUUID get(LLViewerObject* objectp, S32 te_index) override
				{
					LLUUID id;
					LLTextureEntry* tep = objectp->getTE(te_index);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						mHasMaterial = true;
						id = matp->getNormalID();
					}
					return id;
				}

				bool mHasMaterial = false;

			} func;
			identical_norm = selection->getSelectedTEValue(&func, normmap_id);
			has_material = func.mHasMaterial;
		}
		mNormalCtrl->setTentative(!identical_norm);
		mNormalCtrl->setEnabled(editable && !has_pbr_mat);
		mNormalCtrl->setImageAssetID(normmap_id);
		mNormalCtrl->setFallbackImageName("materials_ui_x_24.png");
		if (is_attachment)
		{
			mNormalCtrl->setImmediateFilterPermMask(PERM_COPY |
													PERM_TRANSFER);
		}
		else
		{
			mNormalCtrl->setImmediateFilterPermMask(PERM_NONE);
		}

		// Selected faces cannot bear both a legacy and a PBR material.
		mMapsRadio->setIndexEnabled(MATTYPE_PBR,
									editable && !has_material &&
									gAgent.hasInventoryMaterial());

		// Specular map
        bool identical_spec;
		LLUUID specmap_id;
		{
			struct spec_get final : public LLSelectedTEGetFunctor<LLUUID>
			{
				LLUUID get(LLViewerObject* objectp, S32 te_index) override
				{
					LLUUID id;
					LLTextureEntry* tep = objectp->getTE(te_index);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						id = matp->getSpecularID();
					}
					return id;
				}
			} func;
			identical_spec = selection->getSelectedTEValue(&func, specmap_id);
		}
		mSpecularCtrl->setTentative(!identical_spec);
		mSpecularCtrl->setEnabled(editable && !has_pbr_mat);
		mSpecularCtrl->setImageAssetID(specmap_id);
		mSpecularCtrl->setFallbackImageName("materials_ui_x_24.png");
		if (is_attachment)
		{
			mSpecularCtrl->setImmediateFilterPermMask(PERM_COPY |
													  PERM_TRANSFER);
		}
		else
		{
			mSpecularCtrl->setImmediateFilterPermMask(PERM_NONE);
		}
		mShinyColorSwatch->setFallbackImageName("materials_ui_x_24.png");

		// Planar align
		bool align_planar = mCheckPlanarAlign->get();
		bool identical_planar_aligned = false;
		bool is_planar = false;
		bool enabled;
		{
			struct planar_get final : public LLSelectedTEGetFunctor<bool>
			{
				bool get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep &&
						   tep->getTexGen() == LLTextureEntry::TEX_GEN_PLANAR;
				}
			} func1;
			bool texgens_identical = selection->getSelectedTEValue(&func1,
																   is_planar);

			enabled = editable && texgens_identical && is_planar;
			if (align_planar && enabled)
			{
				LLFace* last_face = getLastSelectedFace();
				LLPanelFaceGetIsAlignedTEFunctor get_is_aligned_func(last_face);
				// This will determine if the texture param controls are tentative:
				identical_planar_aligned =
					selection->applyToTEs(&get_is_aligned_func);
			}
		}
		if (!enabled)
		{
			align_planar = false;
		}
		mCheckPlanarAlign->setValue(align_planar);
		mCheckPlanarAlign->setEnabled(enabled);

		LLTextureEntry::e_texgen selected_texgen =
			LLTextureEntry::TEX_GEN_DEFAULT;
		bool identical_planar_texgen;
		{
			bool identical_texgen;
			struct texgen_get final
			:	public LLSelectedTEGetFunctor<LLTextureEntry::e_texgen>
			{
				LLTextureEntry::e_texgen get(LLViewerObject* objectp,
											 S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? (LLTextureEntry::e_texgen)tep->getTexGen()
							   : LLTextureEntry::TEX_GEN_DEFAULT;
				}
			} func;
			identical_texgen = selection->getSelectedTEValue(&func, selected_texgen);
			identical_planar_texgen = identical_texgen &&
									  selected_texgen == LLTextureEntry::TEX_GEN_PLANAR;
		}
		F32 scale_factor = identical_planar_texgen ? 2.f : 1.f;

		// Texture scale
		mLabelTexScale->setEnabled(editable);
		mLabelTexScaleUnit->setEnabled(editable);
		mLabelTexScaleHoriz->setEnabled(editable);
		mLabelTexScaleVert->setEnabled(editable);
		F32 scale_s = 1.f;
		{
			struct tex_scale_s_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getScaleS() : 0.f;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, scale_s, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		F32 scale = editable ? fabsf(scale_factor * scale_s) : 0.f;
		mTexScaleU->setValue(scale);
		mTexScaleU->setTentative(!identical);
		mTexScaleU->setEnabled(editable && !has_pbr_mat);
		mCheckTexFlipS->setValue(LLSD(scale_s < 0));
		mCheckTexFlipS->setTentative(!identical);
		mCheckTexFlipS->setEnabled(editable && !has_pbr_mat);

		F32 scale_t = 1.f;
		{
			struct tex_scale_t_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getScaleT() : 0.f;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, scale_t, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		scale = editable ? fabsf(scale_factor * scale_t) : 0.f;
		mTexScaleV->setValue(scale);
		mTexScaleV->setTentative(!identical);
		mTexScaleV->setEnabled(editable && !has_pbr_mat);
		mCheckTexFlipT->setValue(LLSD(scale_t < 0));
		mCheckTexFlipT->setTentative(!identical);
		mCheckTexFlipT->setEnabled(editable && !has_pbr_mat);

		// Texture offset
		mLabelTexOffset->setEnabled(editable);
		mLabelTexOffsetHoriz->setEnabled(editable);
		mLabelTexOffsetVert->setEnabled(editable);

		F32 offset_s = 0.f;
		{
			struct tex_offset_s_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getOffsetS() : 0.f;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, offset_s, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mTexOffsetU->setValue(editable ? offset_s : 0.f);
		mTexOffsetU->setTentative(!identical);
		mTexOffsetU->setEnabled(editable && !has_pbr_mat);

		F32 offset_t = 0.f;
		{
			struct tex_offset_t_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getOffsetT() : 0.f;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, offset_t, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mTexOffsetV->setValue(editable ? offset_t : 0.f);
		mTexOffsetV->setTentative(!identical);
		mTexOffsetV->setEnabled(editable && !has_pbr_mat);

		// Texture rotation
		mLabelTexRotate->setEnabled(editable);

		F32 rotation = 0.f;
		{
			struct tex_rot_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getRotation() : 0.f;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, rotation, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mTexRot->setValue(editable ? rotation * RAD_TO_DEG : 0.f);
		mTexRot->setTentative(!identical);
		mTexRot->setEnabled(editable && !has_pbr_mat);

		// Nomal map scale
		{
			struct bump_scale_s_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						F32 t = 0.f;
						matp->getNormalRepeat(s, t);
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, scale_s, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		scale = editable ? fabsf(scale_factor * scale_s) : 0.f;
		mBumpyScaleU->setValue(scale);
		mBumpyScaleU->setTentative(!identical);
		mBumpyScaleU->setEnabled(editable && !has_pbr_mat &&
								 normmap_id.notNull());
		mCheckBumpyFlipS->setValue(LLSD(scale_s < 0));
		mCheckBumpyFlipS->setTentative(!identical);
		mCheckBumpyFlipS->setEnabled(editable && !has_pbr_mat &&
									 normmap_id.notNull());

		{
			struct bump_scale_t_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 t = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						F32 s = 0.f;
						matp->getNormalRepeat(s, t);
					}
					return t;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, scale_t, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		scale = editable ? fabsf(scale_factor * scale_t) : 0.f;
		mBumpyScaleV->setValue(scale);
		mBumpyScaleV->setTentative(!identical);
		mBumpyScaleV->setEnabled(editable && !has_pbr_mat &&
								 normmap_id.notNull());
		mCheckBumpyFlipT->setValue(LLSD(scale_t < 0));
		mCheckBumpyFlipT->setTentative(!identical);
		mCheckBumpyFlipT->setEnabled(editable && !has_pbr_mat &&
									 normmap_id.notNull());

		// Normal map offset
		{
			struct bump_offset_s_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 0.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						F32 t = 0.f;
						matp->getNormalOffset(s, t);
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, offset_s, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mBumpyOffsetU->setValue(editable ? offset_s : 0.f);
		mBumpyOffsetU->setTentative(!identical);
		mBumpyOffsetU->setEnabled(editable && !has_pbr_mat &&
								  normmap_id.notNull());

		{
			struct bump_offset_t_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 t = 0.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						F32 s = 0.f;
						matp->getNormalOffset(s, t);
					}
					return t;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, offset_t, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mBumpyOffsetV->setValue(editable ? offset_t : 0.f);
		mBumpyOffsetV->setTentative(!identical);
		mBumpyOffsetV->setEnabled(editable && !has_pbr_mat &&
								  normmap_id.notNull());

		// Normal map rotation
		{
			struct bump_rot_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 ret = 0.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						ret = matp->getNormalRotation();
					}
					return ret;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, rotation, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mBumpyRot->setValue(editable ? rotation * RAD_TO_DEG : 0.f);
		mBumpyRot->setTentative(!identical);
		mBumpyRot->setEnabled(editable && !has_pbr_mat &&
							  normmap_id.notNull());

		// Specular map scale
		{
			struct shiny_scale_s_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						F32 t = 0.f;
						matp->getSpecularRepeat(s, t);
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, scale_s, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		scale = editable ? fabsf(scale_factor * scale_s) : 0.f;
		mShinyScaleU->setValue(scale);
		mShinyScaleU->setTentative(!identical);
		mShinyScaleU->setEnabled(editable && !has_pbr_mat &&
								 specmap_id.notNull());
		mCheckShinyFlipS->setValue(LLSD(scale_s < 0));
		mCheckShinyFlipS->setTentative(!identical);
		mCheckShinyFlipS->setEnabled(editable && !has_pbr_mat &&
									 specmap_id.notNull());

		{
			struct shiny_scale_t_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 t = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						F32 s = 0.f;
						matp->getSpecularRepeat(s, t);
					}
					return t;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, scale_t, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		scale = editable ? fabsf(scale_factor * scale_t) : 0.f;
		mShinyScaleV->setValue(scale);
		mShinyScaleV->setTentative(!identical);
		mShinyScaleV->setEnabled(editable && !has_pbr_mat &&
								 specmap_id.notNull());
		mCheckShinyFlipT->setValue(LLSD(scale_t < 0));
		mCheckShinyFlipT->setTentative(!identical);
		mCheckShinyFlipT->setEnabled(editable && !has_pbr_mat &&
									 specmap_id.notNull());

		// Specular map offset
		{
			struct shiny_offset_s_get final
			:	public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 0.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						F32 t = 0.f;
						matp->getSpecularOffset(s, t);
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, offset_s, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mShinyOffsetU->setValue(editable ? offset_s : 0.f);
		mShinyOffsetU->setTentative(!identical);
		mShinyOffsetU->setEnabled(editable && !has_pbr_mat &&
								  specmap_id.notNull());

		{
			struct shiny_offset_t_get final
			:	public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 t = 0.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						F32 s = 0.f;
						matp->getSpecularOffset(s, t);
					}
					return t;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, offset_t, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mShinyOffsetV->setValue(editable ? offset_t : 0.f);
		mShinyOffsetV->setTentative(!identical);
		mShinyOffsetV->setEnabled(editable && !has_pbr_mat &&
								  specmap_id.notNull());

		// Specular map rotation
		{
			struct shiny_rot_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 ret = 0.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLMaterial* matp = tep ? tep->getMaterialParams().get()
										   : NULL;
					if (matp)
					{
						ret = matp->getSpecularRotation();
					}
					return ret;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, rotation, 0.001f);
		}
		identical = align_planar ? identical_planar_aligned : identical;
		mShinyRot->setValue(editable ? rotation * RAD_TO_DEG : 0.f);
		mShinyRot->setTentative(!identical);
		mShinyRot->setEnabled(editable && !has_pbr_mat &&
							  specmap_id.notNull());

		// Glow
		F32 glow = 0.f;
		{
			struct glow_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getGlow() : 0.f;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, glow, 0.001f);
		}

		mGlow->setValue(glow);
		mGlow->setEnabled(editable);
		mGlow->setTentative(!identical);

		// Shiny
		mLabelShininess->setEnabled(editable);
		U8 shiny = 0;
		{
			struct shiny_get final : public LLSelectedTEGetFunctor<U8>
			{
				U8 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getShiny() : 0;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, shiny);
		}
		if (specmap_id.notNull())
		{
			shiny = SHINY_TEXTURE;
		}
		LL_DEBUGS("Materials") << "Specular map texture: " << specmap_id
							   << " - Shininess index: " << (S32)shiny
							   << LL_ENDL;
		// Do not attempt to set the combo to SHINY_TEXTURE if the "Use
		// Texture" entry does not exist (in which case updateShinyControls()
		// will automatically create the entry and select it later for us).
		if (shiny != SHINY_TEXTURE ||
			mComboShininess->itemExists(mUseTextureText))
		{
			mComboShininess->setCurrentByIndex((S32)shiny);
		}
		mComboShininess->setEnabled(editable);
		mComboShininess->setTentative(!identical);
		mShinyColorSwatch->setTentative(!identical);
		mGlossiness->setTentative(!identical);
		mEnvironment->setTentative(!identical);
		updateShinyControls();

		// PBR material scale
		if (has_pbr_mat)
		{
			struct pbr_scale_s_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLGLTFMaterial* matp = tep ? tep->getGLTFMaterialOverride()
											   : NULL;
					if (matp)
					{
						// *NOTE: here, we cheat and assume that all scales are
						// equal for all PBR texture maps. *TODO: see if it is
						// a problem at all (probably not)... HB
						s = matp->mTextureTransform[0].mScale.mV[VX];
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, scale_s, 0.001f);

			identical = align_planar ? identical_planar_aligned : identical;
			scale = editable ? fabsf(scale_factor * scale_s) : 0.f;
			mPbrScaleU->setValue(scale);
			mPbrScaleU->setTentative(!identical);
		}
		mPbrScaleU->setEnabled(editable && has_pbr_mat);

		if (has_pbr_mat)
		{
			struct pbr_scale_t_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLGLTFMaterial* matp = tep ? tep->getGLTFMaterialOverride()
											   : NULL;
					if (matp)
					{
						// *NOTE: here, we cheat and assume that all scales are
						// equal for all PBR texture maps. *TODO: see if it is
						// a problem at all (probably not)... HB
						s = matp->mTextureTransform[0].mScale.mV[VY];
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, scale_t, 0.001f);

			identical = align_planar ? identical_planar_aligned : identical;
			scale = editable ? fabsf(scale_factor * scale_t) : 0.f;
			mPbrScaleV->setValue(scale);
			mPbrScaleV->setTentative(!identical);
		}
		mPbrScaleV->setEnabled(editable && has_pbr_mat);

		// PBR material offset
		if (has_pbr_mat)
		{
			struct pbr_offset_s_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLGLTFMaterial* matp = tep ? tep->getGLTFMaterialOverride()
											   : NULL;
					if (matp)
					{
						// *NOTE: here, we cheat and assume that all scales are
						// equal for all PBR texture maps. *TODO: see if it is
						// a problem at all (probably not)... HB
						s = matp->mTextureTransform[0].mOffset.mV[VX];
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, offset_s, 0.001f);

			identical = align_planar ? identical_planar_aligned : identical;
			mPbrOffsetU->setValue(editable ? offset_s : 0.f);
			mPbrOffsetU->setTentative(!identical);
		}
		mPbrOffsetU->setEnabled(editable && has_pbr_mat);

		if (has_pbr_mat)
		{
			struct pbr_offset_t_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLGLTFMaterial* matp = tep ? tep->getGLTFMaterialOverride()
											   : NULL;
					if (matp)
					{
						// *NOTE: here, we cheat and assume that all scales are
						// equal for all PBR texture maps. *TODO: see if it is
						// a problem at all (probably not)... HB
						s = matp->mTextureTransform[0].mOffset.mV[VY];
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, offset_t, 0.001f);

			identical = align_planar ? identical_planar_aligned : identical;
			mPbrOffsetV->setValue(editable ? offset_t : 0.f);
			mPbrOffsetV->setTentative(!identical);
		}
		mPbrOffsetV->setEnabled(editable && has_pbr_mat);

		// Specular map rotation
		if (has_pbr_mat)
		{
			struct pbr_rot_get final : public LLSelectedTEGetFunctor<F32>
			{
				F32 get(LLViewerObject* objectp, S32 face) override
				{
					F32 s = 1.f;
					LLTextureEntry* tep = objectp->getTE(face);
					LLGLTFMaterial* matp = tep ? tep->getGLTFMaterialOverride()
											   : NULL;
					if (matp)
					{
						// *NOTE: here, we cheat and assume that all rotations
						// are equal for all PBR texture maps. *TODO: see if it
						// is a problem at all (probably not)... HB
						s = matp->mTextureTransform[0].mRotation;
					}
					return s;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, rotation, 0.001f);

			identical = align_planar ? identical_planar_aligned : identical;
			mPbrRot->setValue(editable ? rotation * RAD_TO_DEG : 0.f);
			mPbrRot->setTentative(!identical);
		}
		mPbrRot->setEnabled(editable && has_pbr_mat);

		// Bump
		mLabelBumpiness->setEnabled(editable);
		U8 bump = 0;
		{
			struct bump_get final : public LLSelectedTEGetFunctor<U8>
			{
				U8 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getBumpmap() : 0;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, bump);
		}
		if (normmap_id.notNull())
		{
			bump = BUMPY_TEXTURE;
		}
		LL_DEBUGS("Materials") << "Normal map texture: " << normmap_id
							   << " - Bumpininess index: " << (S32)bump
							   << LL_ENDL;
		// Do not attempt to set the combo to BUMPY_TEXTURE if the "Use
		// Texture" entry does not exist (in which case updateBumpyControls()
		// will automatically create the entry and select it later for us).
		if (bump != BUMPY_TEXTURE ||
			mComboBumpiness->itemExists(mUseTextureText))
		{
			mComboBumpiness->setCurrentByIndex((S32)bump);
		}
		mComboBumpiness->setEnabled(editable);
		mComboBumpiness->setTentative(!identical);
		updateBumpyControls();

		// Texgen
		// Note: selected_texgen and identical_planar_texgen have been set far
		// above, before texture offsets.
		mLabelTexGen->setEnabled(editable);
		mComboTexGen->setCurrentByIndex(selected_texgen >> TEM_TEX_GEN_SHIFT);
		mComboTexGen->setEnabled(editable && !has_pbr_mat);
		mComboTexGen->setTentative(!identical_planar_texgen);

		if (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR)
		{
			mLabelTexScaleUnit->setText(mRepeatsPerMeterText);
		}
		else // if (selected_texgen == LLTextureEntry::TEX_GEN_DEFAULT)
		{
			mLabelTexScaleUnit->setText(mRepeatsPerFaceText);
		}

		// Full bright
		U8 fullbright = 0;
		{
			struct fullbright_get final : public LLSelectedTEGetFunctor<U8>
			{
				U8 get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getFullbright() : 0;
				}
			} func;
			identical = selection->getSelectedTEValue(&func, fullbright);
		}
		mCheckFullbright->setValue(fullbright != 0);
		mCheckFullbright->setEnabled(editable && !has_pbr_mat);
		mCheckFullbright->setTentative(!identical);

		// Repeats per meter
		F32 repeats = 1.f;
		identical = false;
		S32 map = mMapsRadio->getSelectedIndex();
		switch (map)
		{
			case MATTYPE_SPECULAR:
			{
				struct shiny_repeats_get final
				:	public LLSelectedTEGetFunctor<F32>
				{
					F32 get(LLViewerObject* objectp, S32 face) override
					{
						LLTextureEntry* tep = objectp->getTE(face);
						LLMaterial* matp = tep ? tep->getMaterialParams().get()
											   : NULL;
						if (!matp)
						{
							return 1.f;
						}

						F32 repeats_s = 0.f;
						F32 repeats_t = 0.f;
						matp->getSpecularRepeat(repeats_s, repeats_t);
						repeats_s /= objectp->getScale().mV[VX];
						repeats_t /= objectp->getScale().mV[VY];
						return llmax(repeats_s, repeats_t);
					}
				} func;
				identical = selection->getSelectedTEValue(&func, repeats,
														  0.001f);
				enabled = shiny == SHINY_TEXTURE && specmap_id.notNull();
				break;
			}

			case MATTYPE_NORMAL:
			{
				struct bump_repeats_get final
				:	public LLSelectedTEGetFunctor<F32>
				{
					F32 get(LLViewerObject* objectp, S32 face) override
					{
						LLTextureEntry* tep = objectp->getTE(face);
						LLMaterial* matp = tep ? tep->getMaterialParams().get()
											   : NULL;
						if (!matp)
						{
							return 1.f;
						}

						F32 repeats_s = 0.f;
						F32 repeats_t = 0.f;
						matp->getNormalRepeat(repeats_s, repeats_t);
						repeats_s /= objectp->getScale().mV[VX];
						repeats_t /= objectp->getScale().mV[VY];
						return llmax(repeats_s, repeats_t);
					}
				} func;
				identical = selection->getSelectedTEValue(&func, repeats,
														  0.001f);
				enabled = bump == BUMPY_TEXTURE && normmap_id.notNull();
				break;
			}

			default:	// MATTYPE_DIFFUSE *and* MATTYPE_PBR
			{
				struct tex_repeats_get final
				:	public LLSelectedTEGetFunctor<F32>
				{
					F32 get(LLViewerObject* objectp, S32 face) override
					{
						LLTextureEntry* tep = objectp->getTE(face);
						if (!tep)
						{
							return 1.f;
						}

						U32 s_axis = VX;
						U32 t_axis = VY;
						// *BUG: only repeats along S axis and only works for
						// boxes.
						LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
#if 1
						F32 repeats_s = tep->getScaleS() /
										objectp->getScale().mV[s_axis];
						F32 repeats_t = tep->getScaleT() /
										objectp->getScale().mV[t_axis];
						return llmax(repeats_s, repeats_t);
#else
						return tep->getScaleS() /
							   objectp->getScale().mV[s_axis];
#endif
					}
				} func;
				identical = selection->getSelectedTEValue(&func, repeats,
														  0.001f);
				enabled = id.notNull();
			}
		}
		enabled = enabled && editable && !identical_planar_texgen;
		mLabelRepeats->setEnabled(editable);
		mRepeats->setValue(editable ? repeats : 1.f);
		mRepeats->setTentative(!identical);
		mRepeats->setEnabled(enabled);
		mLabelRepeats->setVisible(!identical_planar_texgen);
		mRepeats->setVisible(!identical_planar_texgen);

		if (has_pbr_mat || mIsAlpha || normmap_id.notNull() ||
			specmap_id.notNull())
		{
			mButtonResetMaterial->setEnabled(editable);
		}
		else
		{
			mButtonResetMaterial->setEnabled(false);
		}
#if 0
		mButtonAlignMap->setEnabled(selection->getObjectCount() > 1);
#endif
		// Materials
		{
			struct mat_get final : public LLSelectedTEGetFunctor<LLMaterialPtr>
			{
				LLMaterialPtr get(LLViewerObject* objectp, S32 face) override
				{
					LLTextureEntry* tep = objectp->getTE(face);
					return tep ? tep->getMaterialParams()
							   : LLMaterialPtr(NULL);
				}
			} func;
			LLMaterialPtr material;
			identical = selection->getSelectedTEValue(&func, material);
			if (material && editable && !has_pbr_mat)
			{
				LL_DEBUGS("Materials") << "Material: " << material->asLLSD()
									   << LL_ENDL;

				// There is duplicate code below, with what we already dealt
				// with above... We should deal with material first *then* set
				// the rest of the controls accordingly.
				// *TODO: move this code up in getState() and properly merge
				// with existing duplicate code.

				// Alpha
				alpha_mode = material->getDiffuseAlphaMode();
				if (transparency > 0.f)
				{
					// It is invalid to have any alpha mode other than blend if
					// transparency is greater than zero...
					alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
				}
				if (!mIsAlpha)
				{
					// ... unless there is no alpha channel in the texture, in
					// which case alpha mode MUST be none.
					alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
				}
				mComboAlphaMode->setCurrentByIndex(alpha_mode);
				mMaskCutoff->setValue(material->getAlphaMaskCutoff());
				updateAlphaControls();

				LLTextureEntry::e_texgen selected_texgen =
					LLTextureEntry::TEX_GEN_DEFAULT;
				bool identical_texgen = true;
				bool identical_planar_texgen = false;

				struct get_texgen final
				:	public LLSelectedTEGetFunctor<LLTextureEntry::e_texgen>
				{
					LLTextureEntry::e_texgen get(LLViewerObject* objectp,
												 S32 face) override
					{
						LLTextureEntry* tep = objectp->getTE(face);
						return tep ? (LLTextureEntry::e_texgen)tep->getTexGen()
								   : LLTextureEntry::TEX_GEN_DEFAULT;
					}
				} func2;
				LLObjectSelectionHandle selection = gSelectMgr.getSelection();
				identical_texgen =
					selection->getSelectedTEValue(&func2, selected_texgen);
				identical_planar_texgen = identical_texgen &&
										  selected_texgen == LLTextureEntry::TEX_GEN_PLANAR;

				// Shiny (specular)
				F32 offset_x, offset_y, repeat_x, repeat_y, rot;
				mSpecularCtrl->setImageAssetID(material->getSpecularID());
				if (material->getSpecularID().notNull())
				{
					material->getSpecularRepeat(repeat_x, repeat_y);
					if (identical_planar_texgen)
					{
						repeat_x *= 2.f;
						repeat_y *= 2.f;
					}
					mShinyScaleU->setValue(fabsf(repeat_x));
					mCheckShinyFlipS->setValue(LLSD(repeat_x < 0.f));
					mShinyScaleV->setValue(fabsf(repeat_y));
					mCheckShinyFlipT->setValue(LLSD(repeat_y < 0.f));

					material->getSpecularOffset(offset_x, offset_y);
					mShinyOffsetU->setValue(offset_x);
					mShinyOffsetV->setValue(offset_y);

					rot = material->getSpecularRotation();
					mShinyRot->setValue(rot * RAD_TO_DEG);

					mGlossiness->setValue(material->getSpecularLightExponent());

					mEnvironment->setValue(material->getEnvironmentIntensity());
				}
				updateShinyControls();
				if (material->getSpecularID().notNull())
				{
					mShinyColorSwatch->setOriginal(material->getSpecularLightColor());
					mShinyColorSwatch->set(material->getSpecularLightColor(), true);
				}

				// Update the selection manager as to which channel we are
				// editing so that it can reflect the correct overlay UI.
				gSelectMgr.setTextureChannel(getTextureChannelToEdit());

				// Bumpy (normal)
				mNormalCtrl->setImageAssetID(material->getNormalID());
				if (material->getNormalID().notNull())
				{
					material->getNormalRepeat(repeat_x, repeat_y);
					if (identical_planar_texgen)
					{
						repeat_x *= 2.f;
						repeat_y *= 2.f;
					}
					mBumpyScaleU->setValue(fabsf(repeat_x));
					mCheckBumpyFlipS->setValue(LLSD(repeat_x < 0.f));
					mBumpyScaleV->setValue(fabsf(repeat_y));
					mCheckBumpyFlipT->setValue(LLSD(repeat_y < 0.f));

					material->getNormalOffset(offset_x, offset_y);
					mBumpyOffsetU->setValue(offset_x);
					mBumpyOffsetV->setValue(offset_y);

					rot = material->getNormalRotation();
					mBumpyRot->setValue(rot * RAD_TO_DEG);
				}
				updateBumpyControls();
			}
			else
			{
				gSelectMgr.setTextureChannel(LLRender::DIFFUSE_MAP);
			}
		}
	}
	else
	{
		// Disable all UICtrls
		clearCtrls();

		// Disable non-UICtrls
		mTextureCtrl->clear();
		mTextureCtrl->setFallbackImageName("locked_image.j2c");
		mTextureCtrl->setEnabled(false);
		//mTextureCtrl->setValid(false);
		mTextureCtrl->setBakeTextureEnabled(false);

		mNormalCtrl->clear();
		mNormalCtrl->setFallbackImageName("locked_image.j2c");
		mNormalCtrl->setEnabled(false);
		//mNormalCtrl->setValid(false);

		mSpecularCtrl->clear();
		mSpecularCtrl->setFallbackImageName("locked_image.j2c");
		mSpecularCtrl->setEnabled(false);
		//mSpecularCtrl->setValid(false);

		mLabelDiffuseColor->setEnabled(false);
		mColorSwatch->setEnabled(false);
		mColorSwatch->setFallbackImageName("locked_image.j2c");
		mColorSwatch->setValid(false);

		mShinyColorSwatch->setEnabled(false);
		mShinyColorSwatch->setFallbackImageName("locked_image.j2c");
		mShinyColorSwatch->setValid(false);
	}
}

void LLPanelFace::sendTexture()
{
	if (!mTextureCtrl->getTentative())
	{
		// We grab the item Id first, because we want to do a permissions check
		// in the selection manager. ARGH!
		LLUUID id = mTextureCtrl->getImageItemID();
		if (id.isNull())
		{
			id = mTextureCtrl->getImageAssetID();
		}
		gSelectMgr.selectionSetTexture(id);
	}
}

void LLPanelFace::sendBump()
{
	S32 bumpiness = mComboBumpiness->getCurrentIndex();
	if (bumpiness < BUMPY_TEXTURE)
	{
		mNormalCtrl->clear();
	}
	U8 bump = (U8)bumpiness & TEM_BUMP_MASK;
	gSelectMgr.selectionSetBumpmap(bump);
}

void LLPanelFace::sendTexGen()
{
	U8 tex_gen = (U8)mComboTexGen->getCurrentIndex() << TEM_TEX_GEN_SHIFT;
	gSelectMgr.selectionSetTexGen(tex_gen);
}

void LLPanelFace::sendShiny()
{
	S32 shininess = mComboShininess->getCurrentIndex();
	if (shininess < SHINY_TEXTURE)
	{
		mSpecularCtrl->clear();
	}
	U8 shiny = (U8)shininess & TEM_SHINY_MASK;
	gSelectMgr.selectionSetShiny(shiny);
}

void LLPanelFace::sendFullbright()
{
	U8 fullbright = mCheckFullbright->get() ? TEM_FULLBRIGHT_MASK : 0;
	gSelectMgr.selectionSetFullbright(fullbright);
}

void LLPanelFace::sendColor()
{
	LLColor4 color = mColorSwatch->get();
	gSelectMgr.selectionSetColorOnly(color);
}

void LLPanelFace::sendAlpha()
{
	F32 alpha = (100.f - mTransparency->get()) / 100.f;
	gSelectMgr.selectionSetAlphaOnly(alpha);
}

void LLPanelFace::sendGlow()
{
	F32 glow = mGlow->get();
	gSelectMgr.selectionSetGlow(glow);
}

void LLPanelFace::sendTextureInfo()
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (mCheckPlanarAlign->getValue().asBoolean())
	{
		LLFace* last_face = getLastSelectedFace();
		LLPanelFaceSetAlignedTEFunctor setfunc(this, last_face);
		selection->applyToTEs(&setfunc);
	}
	else
	{
		LLPanelFaceSetTEFunctor setfunc(this);
		selection->applyToTEs(&setfunc);
	}

	LLPanelFaceSendFunctor sendfunc;
	selection->applyToObjects(&sendfunc);
}

bool LLPanelFace::canEditSelection()
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	LLViewerObject* objectp = selection->getFirstObject();
	return objectp && objectp->getPCode() == LL_PCODE_VOLUME &&
		   objectp->permModify() && !objectp->isPermanentEnforced();
}

void LLPanelFace::updateAlphaControls()
{
	bool enable = canEditSelection() && mIsAlpha &&
				  mTextureCtrl->getImageAssetID().notNull() &&
				  mTransparency->get() <= 0.f;
	mLabelAlphaMode->setEnabled(enable);
	mComboAlphaMode->setEnabled(enable);
	S32 alpha_mode = mComboAlphaMode->getCurrentIndex();
	enable = enable && alpha_mode == ALPHAMODE_MASK;
    mLabelMaskCutoff->setEnabled(enable);
    mMaskCutoff->setEnabled(enable);
	// Set an equivalent cut-off value for non alpha masking mode:
	switch (alpha_mode)
	{
		case ALPHAMODE_NONE:
		case ALPHAMODE_BLEND:
			mMaskCutoff->setValue(100);
			break;

		case ALPHAMODE_EMISSIVE:
			mMaskCutoff->setValue(0);
			break;

		default:
			break;
	}
}

void LLPanelFace::updateShinyControls()
{
	LLUUID shiny_tex_id = mSpecularCtrl->getImageAssetID();
	S32 shiny = mComboShininess->getCurrentIndex();
	LL_DEBUGS("Materials") << "Specular map texture: " << shiny_tex_id
						   << " - Shininess index: " << shiny << LL_ENDL;

	// *HACK: This depends on adding the "Use texture" item at the end of a
	// list of known length.
	if (shiny_tex_id.notNull())
	{
		if (!mComboShininess->itemExists(mUseTextureText))
		{
			LL_DEBUGS("Materials") << "Adding a '" << mUseTextureText
								   << "' entry to the shininess combo."
								   << LL_ENDL;
			mComboShininess->add(mUseTextureText);
			mComboShininess->setCurrentByIndex(SHINY_TEXTURE);
			// NORSPEC-94: Set default specular color to white
			mShinyColorSwatch->setOriginal(LLColor4::white);
			mShinyColorSwatch->set(LLColor4::white, true);
			mGlossiness->setValue(LLMaterial::DEFAULT_SPECULAR_LIGHT_EXPONENT);
			mEnvironment->setValue(0);
		}
	}
	else if (mComboShininess->itemExists(mUseTextureText))
	{
		LL_DEBUGS("Materials") << "Removing the '" << mUseTextureText
							   << "' entry from the shininess combo."
							   << LL_ENDL;
		mComboShininess->remove(SHINY_TEXTURE);
		if (shiny == SHINY_TEXTURE || shiny < 0)
		{
			mComboShininess->setCurrentByIndex(0);
		}
	}

	LL_DEBUGS("Materials") << " New shininess index: "
						   << mComboShininess->getCurrentIndex() << LL_ENDL;

	bool enable = canEditSelection() &&
				  mComboShininess->getCurrentIndex() == SHINY_TEXTURE;
	mLabelGlossiness->setEnabled(enable);
	mGlossiness->setEnabled(enable);
	mLabelEnvironment->setEnabled(enable);
	mEnvironment->setEnabled(enable);
	mLabelShinyColor->setEnabled(enable);
	mShinyColorSwatch->setEnabled(enable);
	mShinyColorSwatch->setValid(enable);
}

void LLPanelFace::updateBumpyControls()
{
	LLUUID bump_tex_id = mNormalCtrl->getImageAssetID();
	S32 bump = mComboShininess->getCurrentIndex();
	LL_DEBUGS("Materials") << "Normal map texture: " << bump_tex_id
						   << " - Bumpininess index: " << bump << LL_ENDL;

	// *HACK: This depends on adding the "Use texture" item at the end of a
	// list of known length.
	if (bump_tex_id.notNull())
	{
		if (!mComboBumpiness->itemExists(mUseTextureText))
		{
			LL_DEBUGS("Materials") << "Adding a '" << mUseTextureText
								   << "' entry to the bumpininess combo."
								   << LL_ENDL;
			mComboBumpiness->add(mUseTextureText);
			mComboBumpiness->setCurrentByIndex(BUMPY_TEXTURE);
		}
	}
	else if (mComboBumpiness->itemExists(mUseTextureText))
	{
		LL_DEBUGS("Materials") << "Removing the '" << mUseTextureText
							   << "' entry from the bumpininess combo."
							   << LL_ENDL;
		mComboBumpiness->remove(BUMPY_TEXTURE);
		if (bump == BUMPY_TEXTURE || bump < 0)
		{
			mComboBumpiness->setCurrentByIndex(0);
		}
	}
	LL_DEBUGS("Materials") << " New bumpininess index: "
						   << mComboShininess->getCurrentIndex() << LL_ENDL;
}

void LLPanelFace::removeMaterial()
{
	LL_DEBUGS("Materials") << "Resetting material entry" << LL_ENDL;
	gSelectMgr.selectionRemoveMaterial();

	// Check if any PBR material is present, and if yes, remove it. HB
	struct pbr_mat_used final : public LLSelectedTEGetFunctor<bool>
	{
		bool get(LLViewerObject* objectp, S32 te) override
		{
			return objectp->getRenderMaterialID(te).notNull();
		}
	} func;
	bool has_pbr_mat = false;
	gSelectMgr.getSelection()->getSelectedTEValue(&func, has_pbr_mat);
	if (has_pbr_mat)
	{
		gSelectMgr.selectionSetGLTFMaterial(LLUUID::null);
	}

	// Refresh the UI.
	getState();
}

// Assign current state of UI to material definition for submit to sim
void LLPanelFace::updateMaterial()
{
	S32 alpha_mode = mComboAlphaMode->getCurrentIndex();
	S32 bumpiness = mComboBumpiness->getCurrentIndex();
	S32 shininess = mComboShininess->getCurrentIndex();

	LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();

	struct gettexgen final
	:	public LLSelectedTEGetFunctor<LLTextureEntry::e_texgen>
	{
		LLTextureEntry::e_texgen get(LLViewerObject* object, S32 face) override
		{
			LLTextureEntry* te = object->getTE(face);
			return te ? (LLTextureEntry::e_texgen)te->getTexGen()
					  : LLTextureEntry::TEX_GEN_DEFAULT;
		}
	} func;
	bool identical = selection->getSelectedTEValue(&func, selected_texgen);
	identical = identical && selected_texgen == LLTextureEntry::TEX_GEN_PLANAR;
	bool default_blend =
		mIsAlpha ? alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND
				 : alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

	if (!default_blend || bumpiness == BUMPY_TEXTURE ||
		shininess == SHINY_TEXTURE)
	{
		if (mComboAlphaMode->getTentative() && mNormalCtrl->getTentative() &&
			mSpecularCtrl->getTentative())
		{
			// In these conditions, there is nothing to update !
			return;
		}

		// The user's specified something that needs a material.

		// This should match getState()
		struct f1 final : public LLSelectedTEGetFunctor<LLMaterialPtr>
		{
			LLMaterialPtr get(LLViewerObject* object, S32 te_index) override
			{
				LLTextureEntry* te = object->getTE(te_index);
				return te ? te->getMaterialParams() : LLMaterialPtr(NULL);
			}
		} func;
		LLMaterialPtr curmatp;
		selection->getSelectedTEValue(&func, curmatp);
		bool new_mat = curmatp.isNull();
		LLMaterialPtr matp;
		if (new_mat)
		{
			matp = new LLMaterial();
		}
		else
		{
			matp = new LLMaterial(curmatp->asLLSD());
		}
		if (matp.isNull())
		{
			llwarns << "NULL material pointer, aborting !" << llendl;
			llassert(false);
			return;
		}

		if (!mComboAlphaMode->getTentative())
		{
			matp->setDiffuseAlphaMode(mComboAlphaMode->getCurrentIndex());
			matp->setAlphaMaskCutoff((U8)mMaskCutoff->getValue().asInteger());
		}

		LLUUID norm_map_id = mNormalCtrl->getImageAssetID();
		if (bumpiness == BUMPY_TEXTURE && norm_map_id.notNull() &&
			!mNormalCtrl->getTentative())
		{
			LL_DEBUGS("Materials") << "Setting normal map texture, bumpiness = "
								   << bumpiness << LL_ENDL;
			matp->setNormalID(norm_map_id);

			matp->setNormalOffset(mBumpyOffsetU->getValue().asReal(),
								  mBumpyOffsetV->getValue().asReal());

			F32 bumpy_scale_u = mBumpyScaleU->getValue().asReal();
			if (mCheckBumpyFlipS->get())
			{
				bumpy_scale_u = -bumpy_scale_u;
			}
			F32 bumpy_scale_v = mBumpyScaleV->getValue().asReal();
			if (mCheckBumpyFlipT->get())
			{
				bumpy_scale_v = -bumpy_scale_v;
			}
			if (identical)
			{
				bumpy_scale_u *= 0.5f;
				bumpy_scale_v *= 0.5f;
			}
			matp->setNormalRepeat(bumpy_scale_u, bumpy_scale_v);

			matp->setNormalRotation(mBumpyRot->getValue().asReal() *
									DEG_TO_RAD);
		}
		else if (!mNormalCtrl->getTentative())
		{
			LL_DEBUGS("Materials") << "Removing normal map texture, bumpiness = "
								   << bumpiness << LL_ENDL;
			matp->setNormalID(LLUUID::null);
			matp->setNormalOffset(0.f, 0.f);
			matp->setNormalRepeat(1.f, 1.f);
			matp->setNormalRotation(0.f);
		}

		LLUUID spec_map_id = mSpecularCtrl->getImageAssetID();
		if (shininess == SHINY_TEXTURE && spec_map_id.notNull() &&
			!mSpecularCtrl->getTentative())

		{
			LL_DEBUGS("Materials") << "Setting specular map texture, shininess = "
								   << shininess << LL_ENDL;
			matp->setSpecularID(spec_map_id);

			matp->setSpecularOffset(mShinyOffsetU->getValue().asReal(),
									mShinyOffsetV->getValue().asReal());

			F32 shiny_scale_u = mShinyScaleU->getValue().asReal();
			if (mCheckShinyFlipS->get())
			{
				shiny_scale_u = -shiny_scale_u;
			}
			F32 shiny_scale_v = mShinyScaleV->getValue().asReal();
			if (mCheckShinyFlipT->get())
			{
				shiny_scale_v = -shiny_scale_v;
			}
			if (identical)
			{
				shiny_scale_u *= 0.5f;
				shiny_scale_v *= 0.5f;
			}
			matp->setSpecularRepeat(shiny_scale_u, shiny_scale_v);

			matp->setSpecularRotation(mShinyRot->getValue().asReal() *
									  DEG_TO_RAD);

			// Override shininess to 0.2f if this is a new material
			if (!new_mat)
			{
				matp->setSpecularLightColor(mShinyColorSwatch->get());
				matp->setSpecularLightExponent(mGlossiness->getValue().asInteger());
				matp->setEnvironmentIntensity(mEnvironment->getValue().asInteger());
			}
		}
		else if (!mSpecularCtrl->getTentative())
		{
			LL_DEBUGS("Materials") << "Removing specular map texture, shininess = "
								   << shininess << LL_ENDL;
			matp->setSpecularID(LLUUID::null);
			matp->setSpecularOffset(0.f, 0.f);
			matp->setSpecularRepeat(1.f, 1.f);
			matp->setSpecularRotation(0.f);
			matp->setSpecularLightColor(LLMaterial::DEFAULT_SPECULAR_LIGHT_COLOR);
			matp->setSpecularLightExponent(LLMaterial::DEFAULT_SPECULAR_LIGHT_EXPONENT);
			matp->setEnvironmentIntensity(0);
		}

		LL_DEBUGS("Materials") << "Updating material:\n" << matp->asLLSD()
							   << LL_ENDL;
		gSelectMgr.selectionSetMaterials(matp);
	}
	else
	{
		// The user has specified settings that do not need a material.
		removeMaterial();
	}
}

LLMaterialPtr LLPanelFace::createDefaultMaterial(LLMaterialPtr curmat)
{
	LLMaterialPtr newmatp;

	if (curmat.isNull())
	{
		U8 alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
		if (mIsAlpha)
		{
			// Use blend mode for the alpha channel
			alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
		}
		newmatp = new LLMaterial();
		if (newmatp)
		{
			newmatp->setDiffuseAlphaMode(alpha_mode);
		}
	}
	else
	{
		newmatp = new LLMaterial(curmat->asLLSD());
	}
	if (!newmatp)
	{
		llerrs << "Could not create a new material !" << llendl;
	}

	return newmatp;
}

// This callback controls the *visibility* of the UI elements specific to the
// diffuse, normal, specular and PBR maps. The elements enabling/disabling is
// done in getState(), based on the primitive parameters. HB
//static
void LLPanelFace::onSelectMapType(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (!self) return;

	S32 map = self->mMapsRadio->getSelectedIndex();

	bool show_diffuse = map == MATTYPE_DIFFUSE;
	if (!show_diffuse && self->mTextureCtrl->isPickerShown())
	{
		self->mTextureCtrl->closeFloater();
	}
	self->mTextureCtrl->setVisible(show_diffuse);
	self->mLabelAlphaMode->setVisible(show_diffuse);
	self->mComboAlphaMode->setVisible(show_diffuse);
	self->mLabelMaskCutoff->setVisible(show_diffuse);
	self->mMaskCutoff->setVisible(show_diffuse);
	self->mTexScaleU->setVisible(show_diffuse);
	self->mTexScaleV->setVisible(show_diffuse);
	self->mCheckTexFlipS->setVisible(show_diffuse);
	self->mCheckTexFlipT->setVisible(show_diffuse);
	self->mTexOffsetU->setVisible(show_diffuse);
	self->mTexOffsetV->setVisible(show_diffuse);
	self->mTexRot->setVisible(show_diffuse);

	bool show_normal = map == MATTYPE_NORMAL;
	if (!show_normal && self->mNormalCtrl->isPickerShown())
	{
		self->mNormalCtrl->closeFloater();
	}
	self->mNormalCtrl->setVisible(show_normal);
	self->mLabelBumpiness->setVisible(show_normal);
	self->mComboBumpiness->setVisible(show_normal);
	self->mBumpyScaleU->setVisible(show_normal);
	self->mBumpyScaleV->setVisible(show_normal);
	self->mCheckBumpyFlipS->setVisible(show_normal);
	self->mCheckBumpyFlipT->setVisible(show_normal);
	self->mBumpyOffsetU->setVisible(show_normal);
	self->mBumpyOffsetV->setVisible(show_normal);
	self->mBumpyRot->setVisible(show_normal);

	bool show_specular = map == MATTYPE_SPECULAR;
	if (!show_specular && self->mSpecularCtrl->isPickerShown())
	{
		self->mSpecularCtrl->closeFloater();
	}
	self->mSpecularCtrl->setVisible(show_specular);
	self->mLabelShinyColor->setVisible(show_specular);
	self->mShinyColorSwatch->setVisible(show_specular);
	self->mLabelShininess->setVisible(show_specular);
	self->mComboShininess->setVisible(show_specular);
	self->mLabelGlossiness->setVisible(show_specular);
	self->mGlossiness->setVisible(show_specular);
	self->mLabelEnvironment->setVisible(show_specular);
	self->mEnvironment->setVisible(show_specular);
	self->mShinyScaleU->setVisible(show_specular);
	self->mShinyScaleV->setVisible(show_specular);
	self->mCheckShinyFlipS->setVisible(show_specular);
	self->mCheckShinyFlipT->setVisible(show_specular);
	self->mShinyOffsetU->setVisible(show_specular);
	self->mShinyOffsetV->setVisible(show_specular);
	self->mShinyRot->setVisible(show_specular);

	bool show_pbr = map == MATTYPE_PBR;
	self->mPbrScaleU->setVisible(show_pbr);
	self->mPbrScaleV->setVisible(show_pbr);
	self->mPbrOffsetU->setVisible(show_pbr);
	self->mPbrOffsetV->setVisible(show_pbr);
	self->mPbrRot->setVisible(show_pbr);
	self->mButtonEditPBR->setVisible(show_pbr);
	self->mButtonLocalPBR->setVisible(show_pbr);
	self->mButtonLoadPBR->setVisible(show_pbr);
	self->mButtonSavePBR->setVisible(show_pbr);

	// Update all controls
	self->getState();
}

//static
F32 LLPanelFace::valueGlow(LLViewerObject* objectp, S32 face)
{
	if (objectp && objectp->getTE(face))
	{
		return (F32)objectp->getTE(face)->getGlow();
	}
	return 0.f;
}

//static
void LLPanelFace::onClickEditPBR(void*)
{
	LLPreviewMaterial::loadLive();
}

//static
void LLPanelFace::onSelectLocalPBR(const LLUUID& id, void*)
{
	if (id.notNull())
	{
		gSelectMgr.selectionSetGLTFMaterial(id);
	}
}

//static
void LLPanelFace::onClickLocalPBR(void* userdata)
{
	new HBFloaterLocalMaterial((LLView*)userdata, onSelectLocalPBR, NULL);
}

//static
void LLPanelFace::onSelectInventoryPBR(const std::vector<std::string>&,
									   const uuid_vec_t& ids, void*, bool)
{
	if (!ids.empty())
	{
		gSelectMgr.selectionSetGLTFMaterial(ids[0]);
	}
}

//static
void LLPanelFace::onClickLoadPBR(void* userdata)
{
	HBFloaterInvItemsPicker* pickerp =
		new HBFloaterInvItemsPicker((LLView*)userdata, onSelectInventoryPBR,
									NULL);
	pickerp->setAssetType(LLAssetType::AT_MATERIAL);
	pickerp->setApplyImmediatelyControl("ApplyMaterialImmediately");
}

//static
void LLPanelFace::onClickSavePBR(void*)
{
	LLPreviewMaterial::saveObjectsMaterial();
}

//static
void LLPanelFace::onClickRemoveMaterial(void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->removeMaterial();
	}
}

//static
void LLPanelFace::onCommitColor(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->sendColor();
	}
}

//static
void LLPanelFace::onCommitShinyColor(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->updateMaterial();
	}
}

//static
void LLPanelFace::onCommitAlpha(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mComboAlphaMode->setTentative(false);
		self->sendAlpha();
	}
}

//static
void LLPanelFace::onCancelColor(LLUICtrl* ctrl, void* userdata)
{
	gSelectMgr.selectionRevertColors();
}

//static
void LLPanelFace::onSelectColor(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		gSelectMgr.saveSelectedObjectColors();
		self->sendColor();
	}
}

//static
void LLPanelFace::onCommitTexGen(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->sendTexGen();
	}
}

//static
void LLPanelFace::onCommitFullbright(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->sendFullbright();
	}
}

//static
void LLPanelFace::onCommitGlow(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->sendGlow();
	}
}

//static
void LLPanelFace::onCommitAlphaMode(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mComboAlphaMode->setTentative(false);
		self->updateAlphaControls();
		self->updateMaterial();
	}
}

//static
void LLPanelFace::onCommitBump(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mNormalCtrl->setTentative(false);
		self->sendBump();
		self->updateBumpyControls();
		self->updateMaterial();
	}
}

//static
void LLPanelFace::onCommitShiny(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mSpecularCtrl->setTentative(false);
		self->sendShiny();
		self->updateShinyControls();
		self->updateMaterial();
	}
}

//static
bool LLPanelFace::onDragTexture(LLUICtrl*, LLInventoryItem* item, void*)
{
	if (!item) return false;

	for (LLObjectSelection::root_iterator
			iter = gSelectMgr.getSelection()->root_begin(),
			end = gSelectMgr.getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* obj = node->getObject();
		if (!obj || !LLToolDragAndDrop::isInventoryDropAcceptable(obj, item))
		{
			return false;
		}
	}
	return true;
}

//static
void LLPanelFace::onCommitTexture(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		gViewerStats.incStat(LLViewerStats::ST_EDIT_TEXTURE_COUNT);
		self->sendTexture();
	}
}

//static
void LLPanelFace::onCancelTexture(LLUICtrl* ctrl, void* userdata)
{
	gSelectMgr.selectionRevertTextures();
}

//static
void LLPanelFace::onCommitNormalMap(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mNormalCtrl->setTentative(false);
		self->updateBumpyControls();
		self->updateMaterial();
	}
}

//static
void LLPanelFace::onCommitSpecularMap(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mSpecularCtrl->setTentative(false);
		self->updateShinyControls();
		self->updateMaterial();
	}
}

//static
void LLPanelFace::onSelectTexture(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		gSelectMgr.saveSelectedObjectTextures();
		self->sendTexture();
	}
}

//static
void LLPanelFace::onCommitTextureInfo(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->sendTextureInfo();
	}
}

//static
void LLPanelFace::onCommitAlphaMaterial(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mComboAlphaMode->setTentative(false);
		self->updateMaterial();
	}
}

//static
void LLPanelFace::onCommitBumpyMaterial(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mNormalCtrl->setTentative(false);
		self->updateMaterial();
	}
}

//static
void LLPanelFace::onCommitShinyMaterial(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		self->mSpecularCtrl->setTentative(false);
		self->updateMaterial();
	}
}

//static
void LLPanelFace::onCommitPbrMaterial(LLUICtrl* ctrlp, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (!self || !ctrlp)
	{
		return;
	}

	enum
	{
		PBR_SCALE_U,
		PBR_SCALE_V,
		PBR_OFFSET_U,
		PBR_OFFSET_V,
		PBR_ROT,
	};

	S32 param = -1;
	if (ctrlp == (LLUICtrl*)self->mPbrScaleU)
	{
		param = PBR_SCALE_U;
	}
	else if (ctrlp == (LLUICtrl*)self->mPbrScaleV)
	{
		param = PBR_SCALE_V;
	}
	else if (ctrlp == (LLUICtrl*)self->mPbrOffsetU)
	{
		param = PBR_OFFSET_U;
	}
	else if (ctrlp == (LLUICtrl*)self->mPbrOffsetV)
	{
		param = PBR_OFFSET_V;
	}
	else if (ctrlp == (LLUICtrl*)self->mPbrRot)
	{
		param = PBR_ROT;
	}
	else
	{
		llwarns << "Unknown control. Aborted." << llendl;
		return;
	}
	F32 value = ctrlp->getValue().asReal();

	U32 start = 0;
	U32 end = LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT;
#if 0	// *TODO ?
	if (!gSavedSettings.getBool("SyncMaterialSettings"))
	{
		start = self->mPbrTexChannel->getSelectedIndex();
		end = start + 1;
	}
#endif

	struct LLSelectedTEGLTFMaterialFunctor : public LLSelectedTEFunctor
	{
		LLSelectedTEGLTFMaterialFunctor(S32 param, F32 value, U32 start,
										U32 end)
		:	mParam(param),
			mValue(value),
			mStart(start),
			mEnd(end)
		{
		}

		bool apply(LLViewerObject* objectp, S32 face) override
		{
			const LLTextureEntry* tep = objectp->getTE(face);
			if (!tep) return true;

			LLGLTFMaterial new_mat;
			if (tep->getGLTFMaterialOverride())
			{
				new_mat = *tep->getGLTFMaterialOverride();
			}

			for (U32 i = mStart; i < mEnd; ++i)
			{
				LLGLTFMaterial::TextureTransform& new_tt =
					new_mat.mTextureTransform[i];
				switch (mParam)
				{
					case PBR_SCALE_U:
						new_tt.mScale.mV[VX] = mValue;
						break;

					case PBR_SCALE_V:
						new_tt.mScale.mV[VY] = mValue;
						break;

					case PBR_OFFSET_U:
						new_tt.mOffset.mV[VX] = mValue;
						break;

					case PBR_OFFSET_V:
						new_tt.mOffset.mV[VY] = mValue;
						break;

					case PBR_ROT:
						new_tt.mRotation = mValue;

					default:
						break;
				}
			}

			LLGLTFMaterialList::queueModify(objectp, face, &new_mat);
			return true;
		}

		S32 mParam;
		F32 mValue;
		U32 mStart;
		U32 mEnd;
	} select_func(param, value, start, end);

	gSelectMgr.getSelection()->applyToTEs(&select_func);
}

// Commit the number of repeats per meter
//static
void LLPanelFace::onCommitRepeatsPerMeter(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (!self) return;

	F32 repeats_per_meter = self->mRepeats->getValue().asReal();
	S32 map = self->mMapsRadio->getSelectedIndex();
	if (map == MATTYPE_DIFFUSE)
	{
		gSelectMgr.selectionTexScaleAutofit(repeats_per_meter);
		return;
	}

	struct f_objscale_s final : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face) override
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			return object->getScale().mV[s_axis];
		}
	} scale_s_func;

	struct f_objscale_t final : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face) override
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			return object->getScale().mV[t_axis];
		}
	} scale_t_func;

    F32 obj_scale_s;
    F32 obj_scale_t;
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	selection->getSelectedTEValue(&scale_s_func, obj_scale_s, 0.001f);
	selection->getSelectedTEValue(&scale_t_func, obj_scale_t, 0.001f);

	F32 scale_u = obj_scale_s * repeats_per_meter;
	F32 scale_v = obj_scale_t * repeats_per_meter;

	if (map == MATTYPE_NORMAL)
	{
		self->mBumpyScaleU->setValue(fabsf(scale_u));
		self->mCheckBumpyFlipS->setValue(LLSD(scale_u < 0.f));
		self->mBumpyScaleV->setValue(fabsf(scale_v));
		self->mCheckBumpyFlipT->setValue(LLSD(scale_v < 0.f));
	}
	else if (map == MATTYPE_SPECULAR)
	{
		self->mShinyScaleU->setValue(fabsf(scale_u));
		self->mCheckShinyFlipS->setValue(LLSD(scale_u < 0.f));
		self->mShinyScaleV->setValue(fabsf(scale_v));
		self->mCheckShinyFlipT->setValue(LLSD(scale_v < 0.f));
	}

	self->updateMaterial();
}

//static
LLRender::eTexIndex LLPanelFace::getTextureChannelToEdit()
{
	if (!sInstance || !LLFloaterTools::isVisible())
	{
		return gSelectMgr.getTextureChannel();
	}

	S32 map = sInstance->mMapsRadio->getSelectedIndex();
	switch (map)
	{
		case MATTYPE_NORMAL:
			return LLRender::NORMAL_MAP;

		case MATTYPE_SPECULAR:
			return LLRender::SPECULAR_MAP;

		default:	// MATTYPE_DIFFUSE *and* MATTYPE_PBR
			return LLRender::DIFFUSE_MAP;
	}
}

// Commit the fit media texture to prim button

struct LLPanelFaceSetMediaFunctor final : public LLSelectedTEFunctor
{
	bool apply(LLViewerObject* object, S32 te) override
	{
		LLTextureEntry* tep = object->getTE(te);
		if (!tep) return true;

		// *TODO: the media impl pointer should actually be stored by the
		// texture
		viewer_media_t impl =
			LLViewerMedia::getMediaImplFromTextureID(tep->getID());
		// Only do this if it's a media texture
		if (impl.notNull())
		{
			LLPluginClassMedia* media = impl->getMediaPlugin();
			if (media)
			{
				S32 media_width = media->getWidth();
				S32 media_height = media->getHeight();
				S32 texture_width = media->getTextureWidth();
				S32 texture_height = media->getTextureHeight();
				F32 scale_s = (F32)media_width / (F32)texture_width;
				F32 scale_t = (F32)media_height / (F32)texture_height;

				// Set scale and adjust offset
				object->setTEScaleS(te, scale_s);
				// Do not need to flip Y anymore since CEF does this for us now
				object->setTEScaleT(te, scale_t);
				object->setTEOffsetS(te, (scale_s - 1.f) * 0.5f);
				object->setTEOffsetT(te, (scale_t - 1.f) * 0.5f);
			}
		}
		return true;
	};
};

//static
void LLPanelFace::onClickAutoFix(void*)
{
	LLPanelFaceSetMediaFunctor setfunc;
	gSelectMgr.getSelection()->applyToTEs(&setfunc);

	LLPanelFaceSendFunctor sendfunc;
	gSelectMgr.getSelection()->applyToObjects(&sendfunc);
}

#if 0
//static
void LLPanelFace::onAlignTextureLayers(void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (!self)
	{
		return;
	}

	LLObjectSelectionHandle selection = gSelectMgr.getSelection();

	LLFace* last_face = self->getLastSelectedFace();
	S32 map = self->mMapsRadio->getSelectedIndex();
	LLPanelFaceSetAlignedTEFunctor setfunc(self, last_face, map);
	selection->applyToTEs(&setfunc);
}
#endif

//static
void LLPanelFace::onCommitPlanarAlign(LLUICtrl*, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*)userdata;
	if (self)
	{
		// Update all controls
		self->getState();
		self->sendTextureInfo();
	}
}
