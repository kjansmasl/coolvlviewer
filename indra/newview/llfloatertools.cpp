/**
 * @file llfloatertools.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llfloatertools.h"

#include "llapp.h"
#include "llbutton.h"
#include "lldraghandle.h"
#include "llgl.h"
#include "llmenugl.h"
#include "llmediaentry.h"
#include "llslider.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltextureentry.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloatermediasettings.h"
#include "llfloaterobjectweights.h"
#include "llfloateropenobject.h"
#include "llmeshrepository.h"
#include "llpanelcontents.h"
#include "llpanelface.h"
#include "llpanelinventory.h"
#include "llpanelland.h"
#include "llpanelobject.h"
#include "llpanelpermissions.h"
#include "llpanelvolume.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "qltoolalign.h"
#include "lltoolbrushland.h"
#include "lltoolcomp.h"
#include "lltooldraganddrop.h"
#include "lltoolface.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltoolpipette.h"
#include "lltoolplacer.h"
#include "lltoolselectland.h"
#include "llviewercontrol.h"
#include "llviewerjoystick.h"
#include "llviewermenu.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvograss.h"
#include "llvotree.h"
#include "llvovolume.h"

// Globals

// Instance created in LLViewerWindow::initWorldUI()
LLFloaterTools* gFloaterToolsp = NULL;

static const std::string PANEL_NAMES[LLFloaterTools::PANEL_COUNT] =
{
	std::string("General"), 	// PANEL_GENERAL,
	std::string("Object"),		// PANEL_OBJECT,
	std::string("Features"),	// PANEL_FEATURES,
	std::string("Texture"),		// PANEL_FACE,
	std::string("Contents"),	// PANEL_CONTENTS,
};

// Local prototypes
void commit_select_tool(LLUICtrl* ctrl, void* data);
void select_next_part(void* data);
void select_previous_part(void* data);
void click_popup_grab_drag(LLUICtrl*, void*);
void click_popup_grab_lift(LLUICtrl*, void*);
void click_popup_grab_spin(LLUICtrl*, void*);
void click_popup_dozer_mode(LLUICtrl*, void* data);
void commit_slider_dozer_size(LLUICtrl*, void*);
void commit_slider_dozer_force(LLUICtrl*, void*);
void click_apply_to_selection(void*);
void commit_radio_zoom(LLUICtrl*, void*);
void commit_radio_orbit(LLUICtrl*, void*);
void commit_radio_pan(LLUICtrl*, void*);
void commit_slider_zoom(LLUICtrl*, void*);
void click_count(void*);

// Floater for setting global object-editing options, such as grid size and
// spacing.
LLFloaterBuildOptions::LLFloaterBuildOptions(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_build_options.xml");
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterTools class
///////////////////////////////////////////////////////////////////////////////

//static
bool LLFloaterTools::isVisible()
{
	return gFloaterToolsp && gFloaterToolsp->getVisible();
}

//static
void* LLFloaterTools::createPanelPermissions(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelPermissions = new LLPanelPermissions("General");
	return floater->mPanelPermissions;
}

//static
void* LLFloaterTools::createPanelObject(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelObject = new LLPanelObject("Object");
	return floater->mPanelObject;
}

//static
void* LLFloaterTools::createPanelVolume(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelVolume = new LLPanelVolume("Features");
	return floater->mPanelVolume;
}

//static
void* LLFloaterTools::createPanelFace(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelFace = new LLPanelFace("Texture");
	return floater->mPanelFace;
}

//static
void* LLFloaterTools::createPanelContents(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelContents = new LLPanelContents("Contents");
	return floater->mPanelContents;
}

//static
void* LLFloaterTools::createPanelContentsInventory(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelContents->mPanelInventory =
		new LLPanelInventory(std::string("ContentsInventory"), LLRect());
	return floater->mPanelContents->mPanelInventory;
}

//static
void* LLFloaterTools::createPanelLandInfo(void* data)
{
	LLFloaterTools* floater = (LLFloaterTools*)data;
	floater->mPanelLandInfo =
		new LLPanelLandInfo(std::string("land info panel"));
	return floater->mPanelLandInfo;
}

void LLFloaterTools::toolsPrecision()
{
	static LLCachedControl<U32> decimals(gSavedSettings, "DecimalsForTools");
	if (mPrecision != decimals)
	{
		mPrecision = decimals;
		if (mPrecision > 5)
		{
			mPrecision = 5;
		}
		getChild<LLSpinCtrl>("Pos X")->setPrecision(mPrecision);
		getChild<LLSpinCtrl>("Pos Y")->setPrecision(mPrecision);
		getChild<LLSpinCtrl>("Pos Z")->setPrecision(mPrecision);
		getChild<LLSpinCtrl>("Scale X")->setPrecision(mPrecision);
		getChild<LLSpinCtrl>("Scale Y")->setPrecision(mPrecision);
		getChild<LLSpinCtrl>("Scale Z")->setPrecision(mPrecision);
		getChild<LLSpinCtrl>("Rot X")->setPrecision(mPrecision);
		getChild<LLSpinCtrl>("Rot Y")->setPrecision(mPrecision);
		getChild<LLSpinCtrl>("Rot Z")->setPrecision(mPrecision);
	}
}

//virtual
bool LLFloaterTools::postBuild()
{
	// Hide until tool selected
	setVisible(false);

	// Since we constantly show and hide this during drags, do not make sounds
	// on visibility changes.
	setSoundFlags(LLView::SILENT);

	getDragHandle()->setEnabled(true);

	LLRect rect;
	mBtnFocus = getChild<LLButton>("button focus");
	mBtnFocus->setClickedCallback(setEditTool, (void*)&gToolFocus);

	mBtnMove = getChild<LLButton>("button move");
	mBtnMove->setClickedCallback(setEditTool, (void*)&gToolGrab);

	mBtnEdit = getChild<LLButton>("button edit");
	mBtnEdit->setClickedCallback(setEditTool, (void*)&gToolCompTranslate);

	mBtnCreate = getChild<LLButton>("button create");
	mBtnCreate->setClickedCallback(setEditTool, (void*)&gToolCompCreate);

	mBtnLand = getChild<LLButton>("button land");
	mBtnLand->setClickedCallback(setEditTool, (void*)&gToolSelectLand);

	mTextStatus = getChild<LLTextBox>("text status");

	mRadioZoom = getChild<LLCheckBoxCtrl>("radio zoom");
	mRadioZoom->setCommitCallback(commit_radio_zoom);
	mRadioZoom->setCallbackUserData(this);

	mRadioOrbit = getChild<LLCheckBoxCtrl>("radio orbit");
	mRadioOrbit->setCommitCallback(commit_radio_orbit);
	mRadioOrbit->setCallbackUserData(this);

	mRadioPan = getChild<LLCheckBoxCtrl>("radio pan");
	mRadioPan->setCommitCallback(commit_radio_pan);
	mRadioPan->setCallbackUserData(this);

	mSliderZoom = getChild<LLSlider>("slider zoom");
	mSliderZoom->setCommitCallback(commit_slider_zoom);
	mSliderZoom->setCallbackUserData(this);

	mRadioMove = getChild<LLCheckBoxCtrl>("radio move");
	mRadioMove->setCommitCallback(click_popup_grab_drag);
	mRadioMove->setCallbackUserData(this);

	mRadioLift = getChild<LLCheckBoxCtrl>("radio lift");
	mRadioLift->setCommitCallback(click_popup_grab_lift);
	mRadioLift->setCallbackUserData(this);

	mRadioSpin = getChild<LLCheckBoxCtrl>("radio spin");
	mRadioSpin->setCommitCallback(click_popup_grab_spin);
	mRadioSpin->setCallbackUserData(this);

	mRadioPosition = getChild<LLCheckBoxCtrl>("radio position");
	mRadioPosition->setCommitCallback(commit_select_tool);
	mRadioPosition->setCallbackUserData((void*)&gToolCompTranslate);

	mRadioAlign = getChild<LLCheckBoxCtrl>("radio align");
	mRadioAlign->setCommitCallback(commit_select_tool);
	mRadioAlign->setCallbackUserData((void*)&gToolAlign);

	mRadioRotate = getChild<LLCheckBoxCtrl>("radio rotate");
	mRadioRotate->setCommitCallback(commit_select_tool);
	mRadioRotate->setCallbackUserData((void*)&gToolCompRotate);

	mRadioStretch = getChild<LLCheckBoxCtrl>("radio stretch");
	mRadioStretch->setCommitCallback(commit_select_tool);
	mRadioStretch->setCallbackUserData((void*)&gToolCompScale);

	mRadioSelectFace = getChild<LLCheckBoxCtrl>("radio select face");
	mRadioSelectFace->setCommitCallback(commit_select_tool);
	mRadioSelectFace->setCallbackUserData((void*)&gToolFace);

	mCheckSelectIndividual = getChild<LLCheckBoxCtrl>("checkbox edit linked parts");
	mCheckSelectIndividual->setCommitCallback(commitSelectComponent);
	mCheckSelectIndividual->setCallbackUserData(this);

	mBtnGridOptions = getChild<LLButton>("Grid Options");
	mBtnGridOptions->setClickedCallback(onClickGridOptions, this);
	mBtnGridOptions->setControlName("GridOptionState", NULL);

	mCheckStretchUniform = getChild<LLCheckBoxCtrl>("checkbox uniform");

	mCheckStretchTexture = getChild<LLCheckBoxCtrl>("checkbox stretch textures");

	mCheckUseRootForPivot = getChild<LLCheckBoxCtrl>("checkbox use root for pivot");

	mTextGridMode = getChild<LLTextBox>("text ruler mode");

	mComboGridMode = getChild<LLComboBox>("combobox grid mode");
	mComboGridMode->setCommitCallback(onCommitGridMode);
	mComboGridMode->setCallbackUserData(this);

	mBtnPrevChild = getChild<LLButton>("prev_child");
	mBtnPrevChild->setClickedCallback(select_previous_part, this);

	mBtnNextChild = getChild<LLButton>("next_child");
	mBtnNextChild->setClickedCallback(select_next_part, this);

	updatePrevNextBtns();

	mBtnLink = getChild<LLButton>("Link");
	mBtnLink->setClickedCallback(onClickLink, this);

	mBtnUnlink = getChild<LLButton>("Unlink");
	mBtnUnlink->setClickedCallback(onClickUnlink, this);

	mTextObjectCount = getChild<LLTextBox>("obj_count");
	mTextObjectCount->setClickedCallback(click_count, this);
	mTextPrimCount = getChild<LLTextBox>("prim_count");
	mTextPrimCount->setClickedCallback(click_count, this);

	toolsPrecision();

	//
	// Create Buttons
	//

	static const char* tool_names[] =
	{
		"ToolCube",
		"ToolPrism",
		"ToolPyramid",
		"ToolTetrahedron",
		"ToolCylinder",
		"ToolHemiCylinder",
		"ToolCone",
		"ToolHemiCone",
		"ToolSphere",
		"ToolHemiSphere",
		"ToolTorus",
		"ToolTube",
		"ToolRing",
		"ToolTree",
		"ToolGrass"
	};
	constexpr size_t tool_names_count = LL_ARRAY_SIZE(tool_names);

	void* tool_data[] =
	{
		&LLToolPlacerPanel::sCube,
		&LLToolPlacerPanel::sPrism,
		&LLToolPlacerPanel::sPyramid,
		&LLToolPlacerPanel::sTetrahedron,
		&LLToolPlacerPanel::sCylinder,
		&LLToolPlacerPanel::sCylinderHemi,
		&LLToolPlacerPanel::sCone,
		&LLToolPlacerPanel::sConeHemi,
		&LLToolPlacerPanel::sSphere,
		&LLToolPlacerPanel::sSphereHemi,
		&LLToolPlacerPanel::sTorus,
		&LLToolPlacerPanel::sSquareTorus,
		&LLToolPlacerPanel::sTriangleTorus,
		&LLToolPlacerPanel::sTree,
		&LLToolPlacerPanel::sGrass
	};

	for (size_t t = 0; t < tool_names_count; ++t)
	{
		LLButton* found = getChild<LLButton>(tool_names[t]);
		if (found)
		{
			found->setClickedCallback(setObjectType, tool_data[t]);
			mButtons.push_back(found);
		}
		else
		{
			llerrs << "Tool button '" << tool_names[t] << "' not found !"
				   << llendl;
		}
	}

	mCheckCopySelection = getChild<LLCheckBoxCtrl>("checkbox copy selection");
	mCheckCopySelection->setValue(gSavedSettings.getBool("CreateToolCopySelection"));

	mCheckSticky = getChild<LLCheckBoxCtrl>("checkbox sticky");
	mCheckSticky->setValue(gSavedSettings.getBool("CreateToolKeepSelected"));

	mCheckCopyCenters = getChild<LLCheckBoxCtrl>("checkbox copy centers");
	mCheckCopyCenters->setValue(gSavedSettings.getBool("CreateToolCopyCenters"));

	mCheckCopyRotates = getChild<LLCheckBoxCtrl>("checkbox copy rotates");
	mCheckCopyRotates->setValue(gSavedSettings.getBool("CreateToolCopyRotates"));

	mRadioSelectLand = getChild<LLCheckBoxCtrl>("radio select land");
	mRadioSelectLand->setCommitCallback(commit_select_tool);
	mRadioSelectLand->setCallbackUserData((void*)&gToolSelectLand);

	mRadioDozerFlatten = getChild<LLCheckBoxCtrl>("radio flatten");
	mRadioDozerFlatten->setCommitCallback(click_popup_dozer_mode);
	mRadioDozerFlatten->setCallbackUserData((void*)0);

	mRadioDozerRaise = getChild<LLCheckBoxCtrl>("radio raise");
	mRadioDozerRaise->setCommitCallback(click_popup_dozer_mode);
	mRadioDozerRaise->setCallbackUserData((void*)1);

	mRadioDozerLower = getChild<LLCheckBoxCtrl>("radio lower");
	mRadioDozerLower->setCommitCallback(click_popup_dozer_mode);
	mRadioDozerLower->setCallbackUserData((void*)2);

	mRadioDozerSmooth = getChild<LLCheckBoxCtrl>("radio smooth");
	mRadioDozerSmooth->setCommitCallback(click_popup_dozer_mode);
	mRadioDozerSmooth->setCallbackUserData((void*)3);

	mRadioDozerNoise = getChild<LLCheckBoxCtrl>("radio noise");
	mRadioDozerNoise->setCommitCallback(click_popup_dozer_mode);
	mRadioDozerNoise->setCallbackUserData((void*)4);

	mRadioDozerRevert = getChild<LLCheckBoxCtrl>("radio revert");
	mRadioDozerRevert->setCommitCallback(click_popup_dozer_mode);
	mRadioDozerRevert->setCallbackUserData((void*)5);

	mBtnApplyToSelection = getChild<LLButton>("button apply to selection");
	mBtnApplyToSelection->setClickedCallback(click_apply_to_selection, NULL);

	mSliderDozerSize = getChild<LLSlider>("slider brush size");
	mSliderDozerSize->setCommitCallback(commit_slider_dozer_size);
	mSliderDozerSize->setValue(gSavedSettings.getF32("LandBrushSize"));

	mSliderDozerForce = getChild<LLSlider>("slider force");
	mSliderDozerForce->setCommitCallback(commit_slider_dozer_force);
	// The setting stores the actual force multiplier, but the slider is
	// logarithmic, so we convert here
	mSliderDozerForce->setValue(log10f(gSavedSettings.getF32("LandBrushForce")));

	mTextBulldozer = getChild<LLTextBox>("Bulldozer:");
	mTextDozerSize = getChild<LLTextBox>("Dozer Size:");
	mTextStrength = getChild<LLTextBox>("Strength:");

	mComboTreesGrass = getChild<LLComboBox>("tree_grass");
	mComboTreesGrass->setCommitCallback(onSelectTreesGrass);
	mComboTreesGrass->setCallbackUserData(this);

	mTextTreeGrass = getChild<LLTextBox>("tree_grass_label");

	mBtnToolTree = getChild<LLButton>("ToolTree");
	mBtnToolGrass = getChild<LLButton>("ToolGrass");

	mTab = getChild<LLTabContainer>("Object Info Tabs");
	mTab->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
	mTab->setBorderVisible(false);
	mTab->selectFirstTab();

	mStatusText["rotate"] = getString("status_rotate");
	mStatusText["scale"] = getString("status_scale");
	mStatusText["move"] = getString("status_move");
	mStatusText["align"] = getString("status_align");
	mStatusText["modifyland"] = getString("status_modifyland");
	mStatusText["camera"] = getString("status_camera");
	mStatusText["grab"] = getString("status_grab");
	mStatusText["place"] = getString("status_place");
	mStatusText["selectland"] = getString("status_selectland");

	mGridScreenText = getString("grid_screen_text");
	mGridLocalText = getString("grid_local_text");
	mGridWorldText = getString("grid_world_text");
	mGridReferenceText = getString("grid_reference_text");
	mGridAttachmentText = getString("grid_attachment_text");

	mBtnEditMedia = getChild<LLButton>("edit_media");
	mBtnEditMedia->setClickedCallback(onClickBtnEditMedia, this);

	mBtnAddMedia = getChild<LLButton>("add_media");
	mBtnAddMedia->setClickedCallback(onClickBtnAddMedia, this);

	mBtnDeleteMedia = getChild<LLButton>("delete_media");
	mBtnDeleteMedia->setClickedCallback(onClickBtnDeleteMedia, this);

	mTextMediaInfo = getChild<LLTextBox>("media_info");

	return true;
}

// Create the popupview with a dummy center. It will be moved into place during
// LLViewerWindow's per-frame hover processing.
LLFloaterTools::LLFloaterTools()
:	LLFloater("build"),
	mDirty(true),
	mPrecision(3),
	mLastObjectCount(-1),
	mLastPrimCount(-1),
	mLastLandImpact(-1)
{
	setAutoFocus(false);
	LLCallbackMap::map_t factory_map;
	// LLPanelPermissions
	factory_map["General"] = LLCallbackMap(createPanelPermissions, this);
	// LLPanelObject
	factory_map["Object"] = LLCallbackMap(createPanelObject, this);
	// LLPanelVolume
	factory_map["Features"] = LLCallbackMap(createPanelVolume, this);
	// LLPanelFace
	factory_map["Texture"] = LLCallbackMap(createPanelFace, this);
	// LLPanelContents
	factory_map["Contents"] = LLCallbackMap(createPanelContents, this);
	// LLPanelContents
	factory_map["ContentsInventory"] =
		LLCallbackMap(createPanelContentsInventory, this);
	// LLPanelLandInfo
	factory_map["land info panel"] = LLCallbackMap(createPanelLandInfo, this);

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_tools.xml",
												 &factory_map, false);
}

//virtual
LLFloaterTools::~LLFloaterTools()
{
	llinfos << "Floater Tools destroyed" << llendl;
	gFloaterToolsp = NULL;
}

void LLFloaterTools::setStatusText(const std::string& text)
{
	std::map<std::string, std::string>::iterator iter = mStatusText.find(text);
	if (iter != mStatusText.end())
	{
		mTextStatus->setText(iter->second);
	}
	else
	{
		mTextStatus->setText(text);
	}
}

//virtual
void LLFloaterTools::refresh()
{
	constexpr S32 INFO_HEIGHT = 384;
	LLRect object_info_rect(0, 0, getRect().getWidth(), -INFO_HEIGHT);
	bool all_volume = gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME);

	S32 idx_features = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_FEATURES]);
	S32 idx_face = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_FACE]);
	S32 idx_contents = mTab->getPanelIndexByTitle(PANEL_NAMES[PANEL_CONTENTS]);

	S32 selected_index = mTab->getCurrentPanelIndex();

	if (!all_volume &&
		(selected_index == idx_features || selected_index == idx_face ||
		 selected_index == idx_contents))
	{
		mTab->selectFirstTab();
	}

	mTab->enableTabButton(idx_features, all_volume);
	mTab->enableTabButton(idx_face, all_volume);
	mTab->enableTabButton(idx_contents, all_volume);

	// Refresh object and prim count labels
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	S32 objects = selection->getRootObjectCount();
	S32 prims = selection->getObjectCount();
	S32 cost = prims;
	if (gMeshRepo.meshRezEnabled())
	{
		cost = (S32)(selection->getSelectedObjectCost() + 0.5f);
	}
	if (mLastObjectCount != objects || mLastPrimCount != prims ||
		mLastLandImpact != cost)
	{
		mLastObjectCount = objects;
		mLastPrimCount = prims;
		mLastLandImpact = cost;

		std::string count = llformat("%d", objects);
		mTextObjectCount->setTextArg("[COUNT]", count);

		count = llformat("%d", prims);
		if (cost != prims)
		{
			count += llformat(" (%d)", cost);
		}
		mTextPrimCount->setTextArg("[COUNT]", count);
	}

	updatePrevNextBtns();

	toolsPrecision();

	// Refresh child tabs
	mPanelPermissions->refresh();
	mPanelObject->refresh();
	mPanelVolume->refresh();
	mPanelFace->refresh();
	mPanelContents->refresh();
	mPanelLandInfo->refresh();
	getMediaState();
}

//virtual
void LLFloaterTools::draw()
{
//MK
	// Fast enough that it can be kept here
	if (gRLenabled && gRLInterface.mContainsEdit)
	{
		LLViewerObject* objp = gSelectMgr.getSelection()->getPrimaryObject();
		if (!objp || !gRLInterface.canEdit(objp))
		{
			close();
			return;
		}
	}
//mk

	if (mDirty)
	{
		refresh();
		mDirty = false;
	}

	LLFloater::draw();
}

void LLFloaterTools::dirty()
{
	mDirty = true;
	LLFloaterOpenObject::dirty();
}

// Clean up any tool state that should not persist when the floater is closed.
void LLFloaterTools::resetToolState()
{
	gCameraBtnZoom = true;
	gCameraBtnOrbit = false;
	gCameraBtnPan = false;

	gGrabBtnSpin = false;
	gGrabBtnVertical = false;
}

void LLFloaterTools::updatePrevNextBtns()
{
	bool can_do_prev_next = (mCheckSelectIndividual->get() &&
							 !gSelectMgr.getSelection()->isEmpty()) ||
							gToolMgr.isCurrentTool(&gToolFace);
	mBtnPrevChild->setEnabled(can_do_prev_next);
	mBtnNextChild->setEnabled(can_do_prev_next);
}

void LLFloaterTools::updatePopup(LLCoordGL center, MASK mask)
{
	LLTool* tool = gToolMgr.getCurrentTool();

	// *HACK to allow seeing the buttons when you have the app in a window.
	// Keep the visibility the same as it
	if (tool == gToolNull)
	{
		return;
	}

	if (isMinimized())
	{
		// SL looks odd if we draw the tools while the window is minimized
		return;
	}

	// Focus buttons
	bool focus_visible = tool == &gToolFocus;

	mBtnFocus->setToggleState(focus_visible);

	mRadioZoom->setVisible(focus_visible);
	mRadioOrbit->setVisible(focus_visible);
	mRadioPan->setVisible(focus_visible);
	mSliderZoom->setVisible(focus_visible);
	mSliderZoom->setEnabled(gCameraBtnZoom);

	mRadioZoom->set(!gCameraBtnOrbit && !gCameraBtnPan &&
					mask != MASK_ORBIT && mask != (MASK_ORBIT | MASK_ALT) &&
					mask != MASK_PAN && mask != (MASK_PAN | MASK_ALT));

	mRadioOrbit->set(gCameraBtnOrbit || mask == MASK_ORBIT ||
					 mask == (MASK_ORBIT | MASK_ALT));

	mRadioPan->set(gCameraBtnPan || mask == MASK_PAN ||
				   mask == (MASK_PAN | MASK_ALT));

	// Multiply by correction factor because volume sliders go [0, 0.5]
	mSliderZoom->setValue(gAgent.getCameraZoomFraction() * 0.5f);

	// Move buttons
	bool move_visible = tool == &gToolGrab;

	mBtnMove->setToggleState(move_visible);

	// HACK - highlight buttons for next click
	mRadioMove->setVisible(move_visible);
	mRadioMove->set(!gGrabBtnSpin && !gGrabBtnVertical &&
					mask != MASK_VERTICAL && mask != MASK_SPIN);

	mRadioLift->setVisible(move_visible);
	mRadioLift->set(gGrabBtnVertical || mask == MASK_VERTICAL);

	mRadioSpin->setVisible(move_visible);
	mRadioSpin->set(gGrabBtnSpin || mask == MASK_SPIN);

	// Edit buttons
	bool edit_visible = tool->isObjectEditTool();

	mBtnEdit->setToggleState(edit_visible);

	mRadioPosition->setVisible(edit_visible);
	mRadioAlign->setVisible(edit_visible);
	mRadioRotate->setVisible(edit_visible);
	mRadioStretch->setVisible(edit_visible);

	mRadioSelectFace->setVisible(edit_visible);
	mRadioSelectFace->set(tool == &gToolFace);

	mBtnPrevChild->setVisible(edit_visible);
	mBtnNextChild->setVisible(edit_visible);
	mBtnLink->setVisible(edit_visible);
	mBtnUnlink->setVisible(edit_visible);

	mBtnLink->setEnabled(gSelectMgr.enableLinkObjects());
	mBtnUnlink->setEnabled(gSelectMgr.enableUnlinkObjects());
	updatePrevNextBtns();

	mCheckSelectIndividual->setVisible(edit_visible);

	mRadioPosition->set(tool == &gToolCompTranslate);
	mRadioAlign ->set(tool == &gToolAlign);
	mRadioRotate->set(tool == &gToolCompRotate);
	mRadioStretch->set(tool == &gToolCompScale);

	mComboGridMode->setVisible(edit_visible);
	S32 index = mComboGridMode->getCurrentIndex();
	mComboGridMode->removeall();
	switch (mObjectSelection->getSelectType())
	{
		case SELECT_TYPE_HUD:
			mComboGridMode->add(mGridScreenText);
			mComboGridMode->add(mGridLocalText);
			//mComboGridMode->add(mGridReferenceText);
			break;

		case SELECT_TYPE_WORLD:
			mComboGridMode->add(mGridWorldText);
			mComboGridMode->add(mGridLocalText);
			mComboGridMode->add(mGridReferenceText);
			break;

		case SELECT_TYPE_ATTACHMENT:
			mComboGridMode->add(mGridAttachmentText);
			mComboGridMode->add(mGridLocalText);
			mComboGridMode->add(mGridReferenceText);
	}
	mComboGridMode->setCurrentByIndex(index);

	mTextGridMode->setVisible(edit_visible);
	mBtnGridOptions->setVisible(edit_visible);

	//mCheckSelectLinked->setVisible(edit_visible);
	mCheckStretchUniform->setVisible(edit_visible);
	mCheckStretchTexture->setVisible(edit_visible);
	mCheckUseRootForPivot->setVisible(edit_visible);

	// Create buttons
	bool create_visible = tool == &gToolCompCreate;

	mBtnCreate->setToggleState(tool == &gToolCompCreate);

	updateTreeGrassCombo(create_visible);

	if (mCheckCopySelection->get())
	{
		// don't highlight any placer button
		for (std::vector<LLButton*>::size_type i = 0; i < mButtons.size(); i++)
		{
			mButtons[i]->setToggleState(false);
			mButtons[i]->setVisible(create_visible);
		}
	}
	else
	{
		// Highlight the correct placer button
		for (std::vector<LLButton*>::size_type i = 0, count = mButtons.size();
			 i < count; ++i)
		{
			LLPCode pcode = LLToolPlacer::getObjectType();
			void* userdata = mButtons[i]->getCallbackUserData();
			LLPCode* cur = (LLPCode*)userdata;

			bool state = (pcode == *cur);
			mButtons[i]->setToggleState(state);
			mButtons[i]->setVisible(create_visible);
		}
	}

	mCheckSticky->setVisible(create_visible);
	mCheckCopySelection->setVisible(create_visible);
	mCheckCopyCenters->setVisible(create_visible);
	mCheckCopyRotates->setVisible(create_visible);

	mCheckCopyCenters->setEnabled(mCheckCopySelection->get());
	mCheckCopyRotates->setEnabled(mCheckCopySelection->get());

	bool is_tool_brush = tool == &gToolBrushLand;

	// Land buttons
	bool land_visible = is_tool_brush || tool == &gToolSelectLand;

	mBtnLand->setToggleState(land_visible);

	mRadioSelectLand->set(tool == &gToolSelectLand);
	mRadioSelectLand->setVisible(land_visible);

	static LLCachedControl<S32> dozer_mode(gSavedSettings,
										   "RadioLandBrushAction");

	mRadioDozerFlatten->set(is_tool_brush && dozer_mode == 0);
	mRadioDozerFlatten->setVisible(land_visible);

	mRadioDozerRaise->set(is_tool_brush && dozer_mode == 1);
	mRadioDozerRaise->setVisible(land_visible);

	mRadioDozerLower->set(is_tool_brush && dozer_mode == 2);
	mRadioDozerLower->setVisible(land_visible);

	mRadioDozerSmooth->set(is_tool_brush && dozer_mode == 3);
	mRadioDozerSmooth->setVisible(land_visible);

	mRadioDozerNoise->set(is_tool_brush && dozer_mode == 4);
	mRadioDozerNoise->setVisible(land_visible);

	mRadioDozerRevert->set(is_tool_brush && dozer_mode == 5);
	mRadioDozerRevert->setVisible(land_visible);

	mBtnApplyToSelection->setVisible(land_visible);
	mBtnApplyToSelection->setEnabled(land_visible &&
									 !gViewerParcelMgr.selectionEmpty() &&
									 tool != &gToolSelectLand);

	mSliderDozerSize->setVisible(land_visible);
	mTextBulldozer->setVisible(land_visible);
	mTextDozerSize->setVisible(land_visible);

	mSliderDozerForce->setVisible(land_visible);
	mTextStrength->setVisible(land_visible);

	mTextObjectCount->setVisible(!land_visible);
	mTextPrimCount->setVisible(!land_visible);
	mTab->setVisible(!land_visible);
	mPanelLandInfo->setVisible(land_visible);
}

//virtual
bool LLFloaterTools::canClose()
{
	// Do not close when quitting, so camera will stay put
	return !LLApp::isExiting();
}

//virtual
void LLFloaterTools::onOpen()
{
	mParcelSelection = gViewerParcelMgr.getFloatingParcelSelection();
	mObjectSelection = gSelectMgr.getEditSelection();

	gSavedSettings.setBool("BuildBtnState", true);
}

//virtual
void LLFloaterTools::onClose(bool app_quitting)
{
	setMinimized(false);
	setVisible(false);
	mTab->setVisible(false);

	// Must be called, even though this is a dependent floater; this call
	// actually closes the floater (instead of hiding it) and unloads the
	// media preview as a result (ending a SLPlugin).
	LLFloaterMediaSettings::hideInstance();

	LLViewerJoystick::getInstance()->moveAvatar(false);

    // Different from handle_reset_view() in that it does not actually move the
	// camera if EditCameraMovement is not set.
	gAgent.resetView(gSavedSettings.getBool("EditCameraMovement"));

	// Exit component selection mode
	gSelectMgr.promoteSelectionToRoot();
	gSavedSettings.setBool("EditLinkedParts", false);

	gViewerWindowp->showCursor();

	resetToolState();

	mParcelSelection = NULL;
	mObjectSelection = NULL;

	if (!gAgent.cameraMouselook())
	{
		// Switch back to basic toolset
		gToolMgr.setCurrentToolset(gBasicToolset);
		// we were already in basic toolset, using build tools
		// so manually reset tool to default (pie menu tool)
		gToolMgr.getCurrentToolset()->selectFirstTool();
	}
	else
	{
		// Switch back to mouselook toolset
		gToolMgr.setCurrentToolset(gMouselookToolset);

		gViewerWindowp->hideCursor();
		gViewerWindowp->moveCursorToCenter();
	}

	gSavedSettings.setBool("BuildBtnState", false);
}

void LLFloaterTools::showPanel(EInfoPanel panel)
{
	llassert(panel >= 0 && panel < PANEL_COUNT);
	mTab->selectTabByName(PANEL_NAMES[panel]);
}

void click_popup_grab_drag(LLUICtrl*, void*)
{
	gGrabBtnVertical = false;
	gGrabBtnSpin = false;
}

void click_popup_grab_lift(LLUICtrl*, void*)
{
	gGrabBtnVertical = true;
	gGrabBtnSpin = false;
}

void click_popup_grab_spin(LLUICtrl*, void*)
{
	gGrabBtnVertical = false;
	gGrabBtnSpin = true;
}

void commit_radio_zoom(LLUICtrl*, void*)
{
	gCameraBtnZoom = true;
	gCameraBtnOrbit = false;
	gCameraBtnPan = false;
}

void commit_radio_orbit(LLUICtrl*, void*)
{
	gCameraBtnZoom = false;
	gCameraBtnOrbit = true;
	gCameraBtnPan = false;
}

void commit_radio_pan(LLUICtrl*, void*)
{
	gCameraBtnZoom = gCameraBtnOrbit = false;
	gCameraBtnPan = true;
}

void commit_slider_zoom(LLUICtrl* ctrl, void*)
{
	// renormalize value, since max "volume" level is 0.5 for some reason
	F32 zoom_level = (F32)ctrl->getValue().asReal() * 2.f; // / 0.5f;
	gAgent.setCameraZoomFraction(zoom_level);
}

void click_popup_dozer_mode(LLUICtrl*, void* data)
{
	S32 mode = (S32)(intptr_t)data;
	gFloaterToolsp->setEditTool(&gToolBrushLand);
	gSavedSettings.setS32("RadioLandBrushAction", mode);
}

void commit_slider_dozer_size(LLUICtrl* ctrl, void*)
{
	F32 size = (F32)ctrl->getValue().asReal();
	gSavedSettings.setF32("LandBrushSize", size);
}

void commit_slider_dozer_force(LLUICtrl* ctrl, void*)
{
	// The slider is logarithmic, so we exponentiate to get the actual force
	// multiplier
	F32 dozer_force = powf(10.f, ctrl->getValue().asReal());
	gSavedSettings.setF32("LandBrushForce", dozer_force);
}

void click_apply_to_selection(void*)
{
	gToolBrushLand.modifyLandInSelectionGlobal();
}

void commit_select_tool(LLUICtrl* ctrl, void* data)
{
	S32 show_owners = gSavedSettings.getBool("ShowParcelOwners");
	gFloaterToolsp->setEditTool(data);
	gSavedSettings.setBool("ShowParcelOwners", show_owners);
}

void select_next_part(void* data)
{
	select_face_or_linked_prim("next");
}

void select_previous_part(void* data)
{
	select_face_or_linked_prim("previous");
}

void click_count(void* data)
{
	LLFloaterObjectWeights::show(gFloaterToolsp);
}

void LLFloaterTools::onCommitGridMode(LLUICtrl* ctrl, void* data)
{
	LLFloaterTools* self = (LLFloaterTools*)data;
	LLComboBox* combo = (LLComboBox*)ctrl;
	if (combo && self)
	{
		gSelectMgr.setGridMode((EGridMode)combo->getCurrentIndex());
		self->mPanelObject->refresh();
	}
}

//static
void LLFloaterTools::commitSelectComponent(LLUICtrl* ctrl, void* data)
{
	LLFloaterTools* self = (LLFloaterTools*)data;
	if (!self) return;

	// forfeit focus
	if (gFocusMgr.childHasKeyboardFocus(self))
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}

	if (self->mCheckSelectIndividual->get())
	{
		gSelectMgr.demoteSelectionToIndividuals();
	}
	else
	{
		gSelectMgr.promoteSelectionToRoot();
	}

	self->dirty();
}

//static
void LLFloaterTools::setObjectType(void* data)
{
	LLPCode pcode = *(LLPCode*)data;
	LLToolPlacer::setObjectType(pcode);
	gSavedSettings.setBool("CreateToolCopySelection", false);
	gFloaterToolsp->updateTreeGrassCombo(true);
	gFocusMgr.setMouseCapture(NULL);
}

//static
void LLFloaterTools::onClickGridOptions(void* data)
{
	LLFloaterBuildOptions::toggleInstance();

	// Makes grid options dependent on build tools window
	LLFloaterTools* self = (LLFloaterTools*)data;
	LLFloaterBuildOptions* floater = LLFloaterBuildOptions::findInstance();
	if (self && floater)
	{
		self->addDependentFloater(floater);
	}
}

//static
void LLFloaterTools::onClickLink(void* data)
{
	gSelectMgr.linkObjects();
}

//static
void LLFloaterTools::onClickUnlink(void* data)
{
	gSelectMgr.unlinkObjects();
}

void LLFloaterTools::setEditTool(void* tool)
{
	gToolMgr.getCurrentToolset()->selectTool((LLTool*)tool);
}

void LLFloaterTools::onFocusReceived()
{
	gToolMgr.setCurrentToolset(gBasicToolset);
	LLFloater::onFocusReceived();
}

//static
void LLFloaterTools::onSelectTreesGrass(LLUICtrl*, void*)
{
	const std::string& selected = gFloaterToolsp->mComboTreesGrass->getValue();
	LLPCode pcode = LLToolPlacer::getObjectType();
	if (pcode == LLToolPlacerPanel::sTree)
	{
		gSavedSettings.setString("LastTree", selected);
	}
	else if (pcode == LLToolPlacerPanel::sGrass)
	{
		gSavedSettings.setString("LastGrass", selected);
	}
}

void LLFloaterTools::updateTreeGrassCombo(bool visible)
{
	if (visible)
	{
		LLPCode pcode = LLToolPlacer::getObjectType();
		std::map<std::string, S32>::iterator it, end;
		std::string selected;
		if (pcode == LLToolPlacerPanel::sTree)
		{
			mTextTreeGrass->setVisible(visible);
			mTextTreeGrass->setText(mBtnToolTree->getToolTip());

			static LLCachedControl<std::string> last_tree(gSavedSettings,
														  "LastTree");
			selected = last_tree;
			it = LLVOTree::sSpeciesNames.begin();
			end = LLVOTree::sSpeciesNames.end();
		}
		else if (pcode == LLToolPlacerPanel::sGrass)
		{
			mTextTreeGrass->setVisible(visible);
			mTextTreeGrass->setText(mBtnToolGrass->getToolTip());

			static LLCachedControl<std::string> last_grass(gSavedSettings,
														   "LastGrass");
			selected = last_grass;
			it = LLVOGrass::sSpeciesNames.begin();
			end = LLVOGrass::sSpeciesNames.end();
		}
		else
		{
			mComboTreesGrass->removeall();
			// LLComboBox::removeall() does not clear the label
			mComboTreesGrass->setLabel("");
			mComboTreesGrass->setEnabled(false);
			mComboTreesGrass->setVisible(false);
			mTextTreeGrass->setVisible(false);
			return;
		}

		mComboTreesGrass->removeall();
		mComboTreesGrass->add("Random");

		S32 select = 0, i = 0;

		while (it != end)
		{
			const std::string& species = it->first;
			mComboTreesGrass->add(species);
			++i;
			if (species == selected)
			{
				select = i;
			}
			++it;
		}
		// if saved species not found, default to "Random"
		mComboTreesGrass->selectNthItem(select);
		mComboTreesGrass->setEnabled(true);
	}

	mComboTreesGrass->setVisible(visible);
	mTextTreeGrass->setVisible(visible);
}

// Media stuff
// *TODO: move this to llpanelface.cpp, where it belongs...

bool LLFloaterTools::selectedMediaEditable()
{
	U32 owner_mask_on, owner_mask_off;
	bool valid_owner_perms = gSelectMgr.selectGetPerm(PERM_OWNER,
													  &owner_mask_on,
													  &owner_mask_off);
	U32 group_mask_on, group_mask_off;
	bool valid_group_perms = gSelectMgr.selectGetPerm(PERM_GROUP,
													  &group_mask_on,
													  &group_mask_off);
	U32 everyone_mask_on, everyone_mask_off;
	bool valid_everyone_perms = gSelectMgr.selectGetPerm(PERM_EVERYONE,
														 &everyone_mask_on,
														 &everyone_mask_off);
	bool selected_media_editable = false;

	// if perms we got back are valid
	if (valid_owner_perms && valid_group_perms && valid_everyone_perms)
	{
		if ((owner_mask_on & PERM_MODIFY) || (group_mask_on & PERM_MODIFY) ||
			(group_mask_on & PERM_MODIFY))
		{
			selected_media_editable = true;
		}
		else
		{
			// user is NOT allowed to press the RESET button
			selected_media_editable = false;
		}
	}

	return selected_media_editable;
}

void LLFloaterTools::getMediaState()
{
	static LLCachedControl<bool> streaming_media(gSavedSettings,
												 "EnableStreamingMedia");
	static LLCachedControl<bool> prim_media_master(gSavedSettings,
												   "PrimMediaMasterEnabled");
	bool media_enabled = streaming_media && prim_media_master;
	mBtnEditMedia->setVisible(media_enabled);
	mBtnDeleteMedia->setVisible(media_enabled);
	mBtnAddMedia->setVisible(media_enabled);
	mTextMediaInfo->setVisible(media_enabled);
	if (!media_enabled)
	{
		return;
	}

	LLObjectSelectionHandle selected_objects = gSelectMgr.getSelection();
	LLViewerObject* first_object = selected_objects->getFirstObject();

	if (!first_object || first_object->getPCode() != LL_PCODE_VOLUME ||
		!first_object->permModify())
	{
		mBtnEditMedia->setEnabled(false);
		mBtnDeleteMedia->setEnabled(false);
		mBtnAddMedia->setEnabled(false);
		mTextMediaInfo->clear();
		mTextMediaInfo->setToolTip(LLStringUtil::null);
		LLFloaterMediaSettings::clearValues(false);
		return;
	}

	if (first_object->getRegion()->getCapability("ObjectMedia").empty())
	{
		llwarns << "Media not enabled (no capability) in this region"
				<< llendl;
		mBtnEditMedia->setEnabled(false);
		mBtnDeleteMedia->setEnabled(false);
		mBtnAddMedia->setEnabled(false);
		mTextMediaInfo->clear();
		mTextMediaInfo->setToolTip(LLStringUtil::null);
		LLFloaterMediaSettings::clearValues(false);
		return;
	}

	bool is_nonpermanent_enforced = (selected_objects->getFirstRootNode() &&
									 gSelectMgr.selectGetRootsNonPermanentEnforced()) ||
									gSelectMgr.selectGetNonPermanentEnforced();
	bool editable = is_nonpermanent_enforced &&
					(first_object->permModify() || selectedMediaEditable());

	// Check modify permissions and whether any selected objects are in
	// the process of being fetched. If they are, then we're not editable
	if (editable)
	{
		for (LLObjectSelection::iterator iter = selected_objects->begin(),
										 end = selected_objects->end();
			 iter != end; ++iter)
		{
			LLSelectNode* node = *iter;
			LLViewerObject* objectp = node->getObject();
			if (!objectp) continue;	// Paranoia

			LLVOVolume* vobjp = objectp->asVolume();
			if (vobjp && !vobjp->permModify())
			{
				llinfos << "Selection not editable due to lack of modify permissions on object id "
						<< vobjp->getID() << llendl;
				editable = false;
				break;
			}
		}
	}

	// Media settings
	bool bool_has_media = false;
	struct media_functor : public LLSelectedTEGetFunctor<bool>
	{
		bool get(LLViewerObject* object, S32 face)
		{
			LLTextureEntry* te = object->getTE(face);
			if (te)
			{
				return te->hasMedia();
			}
			return false;
		}
	} func;

	// check if all faces have media(or, all dont have media)
	LLFloaterMediaSettings::setHasMediaInfo(selected_objects->getSelectedTEValue(&func,
																				 bool_has_media));
	const LLMediaEntry default_media_data;

	struct functor_getter_media_data : public LLSelectedTEGetFunctor<LLMediaEntry>
    {
		functor_getter_media_data(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        LLMediaEntry get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* te = object ? object->getTE(face) : NULL;
			if (te && te->getMediaData())
			{
				return *(te->getMediaData());
			}
			return mMediaEntry;
        }

		const LLMediaEntry& mMediaEntry;

    } func_media_data(default_media_data);

	LLMediaEntry media_data_get;
    LLFloaterMediaSettings::setMultipleMedia(!selected_objects->getSelectedTEValue(&func_media_data,
																				   media_data_get));

	static const std::string multi_media_info_str = "Multiple Media";
	std::string media_title;
	// update UI depending on whether "object" (prim or face) has media and
	// whether or not you are allowed to edit it.

	if (LLFloaterMediaSettings::getHasMediaInfo())
	{
		// If all the faces have media (or all don't have media)

		mTextMediaInfo->clear();
		// if identical is set, all faces are same (whether all empty or has
		// the same media)
		if (!LLFloaterMediaSettings::getMultipleMedia())
		{
			// Media data is valid
			if (media_data_get != default_media_data)
			{
				// media title is the media URL
				media_title = media_data_get.getHomeURL();
			}
			// else all faces might be empty.
		}
		else // there are different medias on the faces.
		{
			media_title = multi_media_info_str;
		}

		mBtnEditMedia->setEnabled(bool_has_media && editable &&
								  LLFloaterMediaSettings::getHasMediaInfo());
		mBtnDeleteMedia->setEnabled(bool_has_media && editable);
		mBtnAddMedia->setEnabled(!bool_has_media && editable);
	}
	else
	{
		// Not all faces have media but at least one does.

		LLFloaterMediaSettings::setMultipleValidMedia(selected_objects->isMultipleTEValue(&func_media_data,
																						  default_media_data));
		if (LLFloaterMediaSettings::getMultipleValidMedia())
		{
			// Selected faces don't have identical values
			media_title = multi_media_info_str;
		}
		else
		{
			// Media data is valid
			if (media_data_get != default_media_data)
			{
				// media title is the media URL
				media_title = media_data_get.getHomeURL();
			}
		}

		mBtnEditMedia->setEnabled(LLFloaterMediaSettings::getHasMediaInfo());
		mBtnDeleteMedia->setEnabled(true);
		mBtnAddMedia->setEnabled(false);
	}
	mTextMediaInfo->setText(media_title);
	mTextMediaInfo->setToolTip(media_title);
	mTextMediaInfo->setEnabled(true);

	// load values for media settings
	updateMediaSettings();

	LLFloaterMediaSettings::initValues(mMediaSettings, editable);
}

//static
bool LLFloaterTools::multipleFacesSelectedConfirm(const LLSD& notification,
												  const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (gFloaterToolsp && option == 0)	// "Yes"
	{
		gFloaterToolsp->onClickBtnEditMedia(gFloaterToolsp);
	}
	return false;
}

//static
void LLFloaterTools::onClickBtnAddMedia(void* data)
{
	LLFloaterTools* self = (LLFloaterTools*)data;
	if (self)
	{
		if (gSelectMgr.getSelection()->isMultipleTESelected())
		{
			gNotifications.add("MultipleFacesSelected", LLSD(), LLSD(),
							   multipleFacesSelectedConfirm);
		}
		else
		{
			onClickBtnEditMedia(data);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to edit existing media settings on a prim or prim
// face.
void LLFloaterTools::onClickBtnEditMedia(void* data)
{
	LLFloaterTools* self = (LLFloaterTools*)data;
	if (self)
	{
		self->getMediaState();
		// Makes the media settings dependent on build tools window
		self->addDependentFloater(LLFloaterMediaSettings::showInstance(),
								  false);
	}
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to delete media from a prim or prim face
//static
bool LLFloaterTools::deleteMediaConfirm(const LLSD& notification,
										const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)	// "Yes"
	{
		gSelectMgr.selectionSetMedia(0, LLSD());
		LLFloaterMediaSettings::hideInstance();
	}
	return false;
}

//static
void LLFloaterTools::onClickBtnDeleteMedia(void* data)
{
	gNotifications.add("DeleteMedia", LLSD(), LLSD(), deleteMediaConfirm);
}

void LLFloaterTools::updateMediaSettings()
{
	static const std::string tentative_suffix(LLMediaEntry::TENTATIVE_SUFFIX);
    bool identical = false;
    std::string base_key, value_str;
    int value_int = 0;
    bool value_bool = false;
	LLObjectSelectionHandle selected_objects = gSelectMgr.getSelection();

    const LLMediaEntry default_media_data;

    // controls
    U8 value_u8 = default_media_data.getControls();
    struct functor_getter_controls : public LLSelectedTEGetFunctor<U8>
    {
		functor_getter_controls(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        U8 get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
			if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getControls();
			}
			return mMediaEntry.getControls();
        }

		const LLMediaEntry& mMediaEntry;
    } func_controls(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_controls, value_u8);
    base_key = std::string(LLMediaEntry::CONTROLS_KEY);
    mMediaSettings[base_key] = value_u8;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // First click (formerly left click)
    value_bool = default_media_data.getFirstClickInteract();
    struct functor_getter_first_click : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_first_click(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
			if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getFirstClickInteract();
			}
			return mMediaEntry.getFirstClickInteract();
        }

		const LLMediaEntry& mMediaEntry;
    } func_first_click(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_first_click,
													 value_bool);
    base_key = std::string(LLMediaEntry::FIRST_CLICK_INTERACT_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Home URL
    value_str = default_media_data.getHomeURL();
    struct functor_getter_home_url : public LLSelectedTEGetFunctor<std::string>
    {
		functor_getter_home_url(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        std::string get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
			if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getHomeURL();
			}
			return mMediaEntry.getHomeURL();
        }

		const LLMediaEntry& mMediaEntry;
    } func_home_url(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_home_url,
													 value_str);
    base_key = std::string(LLMediaEntry::HOME_URL_KEY);
    mMediaSettings[base_key] = value_str;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Current URL
    value_str = default_media_data.getCurrentURL();
    struct functor_getter_current_url : public LLSelectedTEGetFunctor<std::string>
    {
		functor_getter_current_url(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

		std::string get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
			if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getCurrentURL();
			}
			return mMediaEntry.getCurrentURL();
        }

		const LLMediaEntry& mMediaEntry;
    } func_current_url(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_current_url,
													 value_str);
    base_key = std::string(LLMediaEntry::CURRENT_URL_KEY);
    mMediaSettings[base_key] = value_str;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Auto zoom
    value_bool = default_media_data.getAutoZoom();
    struct functor_getter_auto_zoom : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_auto_zoom(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
			if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getAutoZoom();
			}
			return mMediaEntry.getAutoZoom();
        }

		const LLMediaEntry& mMediaEntry;
    } func_auto_zoom(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_auto_zoom,
													 value_bool);
    base_key = std::string(LLMediaEntry::AUTO_ZOOM_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Auto play
#if 0	// set default to auto play true -- angela  EXT-5172
    value_bool = default_media_data.getAutoPlay();
#else
	value_bool = true;
#endif
    struct functor_getter_auto_play : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_auto_play(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
			if (tep && tep->getMediaData())
			{
				return object->getTE(face)->getMediaData()->getAutoPlay();
			}
#if 0		// set default to auto play true -- angela  EXT-5172
			return mMediaEntry.getAutoPlay();
#else
			return true;
#endif
        }

		const LLMediaEntry& mMediaEntry;
    } func_auto_play(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_auto_play,
													 value_bool);
    base_key = std::string(LLMediaEntry::AUTO_PLAY_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Auto scale
#if 0		// set default to auto scale true -- angela  EXT-5172
    value_bool = default_media_data.getAutoScale();
#else
	value_bool = true;
#endif
    struct functor_getter_auto_scale : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_auto_scale(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
			if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getAutoScale();
			}
#if 0		// set default to auto scale true -- angela  EXT-5172
			return mMediaEntry.getAutoScale();
#else
			return true;
#endif
		}

		const LLMediaEntry& mMediaEntry;
    } func_auto_scale(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_auto_scale,
													 value_bool);
    base_key = std::string(LLMediaEntry::AUTO_SCALE_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Auto loop
    value_bool = default_media_data.getAutoLoop();
    struct functor_getter_auto_loop : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_auto_loop(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getAutoLoop();
			}
			return mMediaEntry.getAutoLoop();
        }

		const LLMediaEntry& mMediaEntry;
    } func_auto_loop(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_auto_loop,
													 value_bool);
    base_key = std::string(LLMediaEntry::AUTO_LOOP_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // width pixels (if not auto scaled)
    value_int = default_media_data.getWidthPixels();
    struct functor_getter_width_pixels : public LLSelectedTEGetFunctor<int>
    {
		functor_getter_width_pixels(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        int get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getWidthPixels();
			}
			return mMediaEntry.getWidthPixels();
        }

		const LLMediaEntry& mMediaEntry;
    } func_width_pixels(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_width_pixels,
													 value_int);
    base_key = std::string(LLMediaEntry::WIDTH_PIXELS_KEY);
    mMediaSettings[base_key] = value_int;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // height pixels (if not auto scaled)
    value_int = default_media_data.getHeightPixels();
    struct functor_getter_height_pixels : public LLSelectedTEGetFunctor<int>
    {
		functor_getter_height_pixels(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

		int get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getHeightPixels();
			}
			return mMediaEntry.getHeightPixels();
        }

		const LLMediaEntry& mMediaEntry;
    } func_height_pixels(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_height_pixels,
													 value_int);
    base_key = std::string(LLMediaEntry::HEIGHT_PIXELS_KEY);
    mMediaSettings[base_key] = value_int;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Enable Alt image
    value_bool = default_media_data.getAltImageEnable();
    struct functor_getter_enable_alt_image : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_enable_alt_image(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

		bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getAltImageEnable();
			}
			return mMediaEntry.getAltImageEnable();
        }

		const LLMediaEntry& mMediaEntry;
    } func_enable_alt_image(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_enable_alt_image,
													 value_bool);
    base_key = std::string(LLMediaEntry::ALT_IMAGE_ENABLE_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Perms - owner interact
    value_bool = (default_media_data.getPermsInteract() &
				  LLMediaEntry::PERM_OWNER) != 0;
    struct functor_getter_perms_owner_interact : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_perms_owner_interact(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

		bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return (tep->getMediaData()->getPermsInteract() &
						LLMediaEntry::PERM_OWNER) != 0;
			}
			return (mMediaEntry.getPermsInteract() &
					LLMediaEntry::PERM_OWNER) != 0;
        }

		const LLMediaEntry& mMediaEntry;
    } func_perms_owner_interact(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_owner_interact,
													 value_bool);
    base_key = std::string(LLMediaEntry::PERMS_OWNER_INTERACT_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Perms - owner control
    value_bool = (default_media_data.getPermsControl() &
				  LLMediaEntry::PERM_OWNER) != 0;
    struct functor_getter_perms_owner_control : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_perms_owner_control(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return (tep->getMediaData()->getPermsControl() &
						LLMediaEntry::PERM_OWNER) != 0;
			}
			return (mMediaEntry.getPermsControl() &
					LLMediaEntry::PERM_OWNER) != 0;
        }

		const LLMediaEntry& mMediaEntry;
    } func_perms_owner_control(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_owner_control,
													 value_bool);
    base_key = std::string(LLMediaEntry::PERMS_OWNER_CONTROL_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Perms - group interact
    value_bool = (default_media_data.getPermsInteract() &
				  LLMediaEntry::PERM_GROUP) != 0;
    struct functor_getter_perms_group_interact : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_perms_group_interact(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return (tep->getMediaData()->getPermsInteract() &
						LLMediaEntry::PERM_GROUP) != 0;
			}
			return (mMediaEntry.getPermsInteract() &
					LLMediaEntry::PERM_GROUP) != 0;
        }

		const LLMediaEntry& mMediaEntry;
    } func_perms_group_interact(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_group_interact,
													 value_bool);
    base_key = std::string(LLMediaEntry::PERMS_GROUP_INTERACT_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Perms - group control
    value_bool = (default_media_data.getPermsControl() &
				  LLMediaEntry::PERM_GROUP) != 0;
    struct functor_getter_perms_group_control : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_perms_group_control(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return (tep->getMediaData()->getPermsControl() &
						LLMediaEntry::PERM_GROUP) != 0;
			}
			return (mMediaEntry.getPermsControl() &
					LLMediaEntry::PERM_GROUP) != 0;
        }

		const LLMediaEntry& mMediaEntry;
    } func_perms_group_control(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_group_control,
													 value_bool);
    base_key = std::string(LLMediaEntry::PERMS_GROUP_CONTROL_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Perms - anyone interact
    value_bool = (default_media_data.getPermsInteract() &
				  LLMediaEntry::PERM_ANYONE) != 0;
    struct functor_getter_perms_anyone_interact : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_perms_anyone_interact(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return (tep->getMediaData()->getPermsInteract() &
						LLMediaEntry::PERM_ANYONE) != 0;
			}
			return (mMediaEntry.getPermsInteract() &
					LLMediaEntry::PERM_ANYONE) != 0;
        }

		const LLMediaEntry& mMediaEntry;
    } func_perms_anyone_interact(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_anyone_interact,
													 value_bool);
    base_key = std::string(LLMediaEntry::PERMS_ANYONE_INTERACT_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Perms - anyone control
    value_bool = (default_media_data.getPermsControl() &
				  LLMediaEntry::PERM_ANYONE) != 0;
    struct functor_getter_perms_anyone_control : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_perms_anyone_control(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return (tep->getMediaData()->getPermsControl() &
						LLMediaEntry::PERM_ANYONE) != 0;
			}
			return (mMediaEntry.getPermsControl() &
					LLMediaEntry::PERM_ANYONE) != 0;
        }

		const LLMediaEntry& mMediaEntry;
    } func_perms_anyone_control(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_anyone_control,
													 value_bool);
    base_key = std::string(LLMediaEntry::PERMS_ANYONE_CONTROL_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Security - whitelist enable
    value_bool = default_media_data.getWhiteListEnable();
    struct functor_getter_whitelist_enable : public LLSelectedTEGetFunctor<bool>
    {
		functor_getter_whitelist_enable(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        bool get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getWhiteListEnable();
			}
			return mMediaEntry.getWhiteListEnable();
        }

		const LLMediaEntry& mMediaEntry;
    } func_whitelist_enable(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_whitelist_enable,
													 value_bool);
    base_key = std::string(LLMediaEntry::WHITELIST_ENABLE_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + tentative_suffix] = !identical;

    // Security - whitelist URLs
    std::vector<std::string> value_vector_str = default_media_data.getWhiteList();
    struct functor_getter_whitelist_urls : public LLSelectedTEGetFunctor<std::vector<std::string> >
    {
		functor_getter_whitelist_urls(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

        std::vector<std::string> get(LLViewerObject* object, S32 face)
        {
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
            if (tep && tep->getMediaData())
			{
				return tep->getMediaData()->getWhiteList();
			}
			return mMediaEntry.getWhiteList();
        }

		const LLMediaEntry& mMediaEntry;
    } func_whitelist_urls(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_whitelist_urls,
													 value_vector_str);
    base_key = std::string(LLMediaEntry::WHITELIST_KEY);
	mMediaSettings[base_key].clear();
    std::vector<std::string>::iterator iter = value_vector_str.begin();
    std::vector<std::string>::iterator end = value_vector_str.end();
    while (iter != end)
    {
        std::string white_list_url = *iter++;
        mMediaSettings[base_key].append(white_list_url);
    }

    mMediaSettings[base_key + tentative_suffix] = !identical;
}
