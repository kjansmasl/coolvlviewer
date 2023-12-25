/**
 * @file llfloatercustomize.cpp
 * @brief The customize avatar floater, triggered by "Appearance..."
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

// *TODO:translate: the UI xml for this really needs to be integrated with the
// appearance paramaters

#include "llviewerprecompiledheaders.h"

#include "llfloatercustomize.h"

#include "llassetstorage.h"
#include "llscrollcontainer.h"
#include "llscrollingpanellist.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "hbfloatermakenewoutfit.h"
#include "llpaneleditwearable.h"
#include "llselectmgr.h"
#include "llscrollingpanelparam.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"				// For handle_reset_view()
#include "llviewerregion.h"
#include "llvisualparamhint.h"
#include "llvoavatarself.h"

using namespace LLAvatarAppearanceDefines;

///////////////////////////////////////////////////////////////////////////////
// LLFloaterCustomizeObserver class
///////////////////////////////////////////////////////////////////////////////

class LLFloaterCustomizeObserver final : public LLInventoryObserver
{
public:
	LLFloaterCustomizeObserver(LLFloaterCustomize* fc)
	:	mFC(fc)
	{
	}

	~LLFloaterCustomizeObserver() override	{}
	void changed(U32 mask) override			{ mFC->updateScrollingPanelUI(); }

protected:
	LLFloaterCustomize* mFC;
};

///////////////////////////////////////////////////////////////////////////////
// LLFloaterCustomize class
///////////////////////////////////////////////////////////////////////////////

LLFloaterCustomize* gFloaterCustomizep = NULL;

//static
LLWearableType::EType LLFloaterCustomize::sCurrentWearableType =
	LLWearableType::WT_INVALID;

struct WearablePanelData
{
	WearablePanelData(LLFloaterCustomize* floater, LLWearableType::EType type)
	:	mFloater(floater),
		mType(type)
	{
	}

	LLFloaterCustomize* mFloater;
	LLWearableType::EType mType;
};

static void* createUniversalPanel(void*)
{
	return new LLPanel("Universal");
}

LLFloaterCustomize::LLFloaterCustomize()
:	LLFloater("customize"),
	mScrollingPanelList(NULL),
	mInventoryObserver(NULL),
	mNextStepAfterSaveCallback(NULL),
	mNextStepAfterSaveUserdata(NULL)
{
	LLVOAvatarSelf::onCustomizeStart();

	// Initialize to 0
	memset(&mWearablePanelList[0], 0, sizeof(char*)*LLWearableType::WT_COUNT);

	if (isAgentAvatarValid())
	{
		gSavedSettings.setU32("AvatarSex",
							  gAgentAvatarp->getSex() == SEX_MALE);
	}

	mResetParams = new LLVisualParamReset();

	// Create the observer which will watch for matching incoming inventory
	mInventoryObserver = new LLFloaterCustomizeObserver(this);
	gInventory.addObserver(mInventoryObserver);

	LLCallbackMap::map_t factory_map;
	std::string name;
	for (U32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		name = LLWearableType::getCapitalizedTypeName(type);
		factory_map[name] = LLCallbackMap(createWearablePanel,
										  (new WearablePanelData(this, type)));
	}

	if (!gAgent.getRegion() || !gAgent.getRegion()->bakesOnMeshEnabled())
	{
		factory_map["Universal"] = LLCallbackMap(createUniversalPanel, NULL);
	}

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_customize.xml",
												 &factory_map);
}

bool LLFloaterCustomize::postBuild()
{
	childSetAction("Make Outfit", onBtnMakeOutfit, this);
	childSetAction("Ok", onBtnOk, this);
	childSetAction("Cancel", LLFloater::onClickClose, this);

	// Wearable panels
	initWearablePanels();

	// Tab container
	for (U32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		childSetTabChangeCallback("customize tab container",
								  LLWearableType::getCapitalizedTypeName(type),
								  onTabChanged, (void*)type, onTabPrecommit);
	}

	LLTabContainer* tab = getChild<LLTabContainer>("customize tab container",
												   true, false);
	if (tab)
	{
		LLPanel* panel = tab->getPanelByName("Universal");
		if (panel)
		{
			if (!gAgent.getRegion() ||
				!gAgent.getRegion()->bakesOnMeshEnabled())
			{
				tab->removeTabPanel(panel);
				delete panel;
			}
		}
#if LL_TEEN_WERABLE_RESTRICTIONS
		// Remove underwear panels for teens
		if (gAgent.isTeen())
		{
			panel = tab->getPanelByName("Undershirt");
			if (panel)
			{
				tab->removeTabPanel(panel);
				delete panel;
				mWearablePanelList[LLWearableType::WT_UNDERSHIRT] = NULL;
			}
			panel = tab->getPanelByName("Underpants");
			if (panel)
			{
				tab->removeTabPanel(panel);
				delete panel;
				mWearablePanelList[LLWearableType::WT_UNDERPANTS] = NULL;
			}
		}
#endif
	}

	// Scrolling Panel
	initScrollingPanelList();

	return true;
}

void LLFloaterCustomize::open()
{
	LLFloater::open();
	// childShowTab depends on gFloaterCustomizep being defined and therefore
	// must be called after the constructor. - Nyx
	childShowTab("customize tab container", "Shape", true);
	setCurrentWearableType(LLWearableType::WT_SHAPE);
	// *HACK: For some reason, a (NULL !) selection gets set when the customize
	// floater is opened, which confuses the enable check functions for the
	// menu bar... Let's reset it to avoid problems...
	gSelectMgr.clearSelections();
}

//static
bool LLFloaterCustomize::isVisible()
{
	return gFloaterCustomizep && gFloaterCustomizep->getVisible();
}

//static
void LLFloaterCustomize::updateAvatarHeightDisplay()
{
	if (gFloaterCustomizep && isAgentAvatarValid())
	{
		F32 shoes = gAgentAvatarp->getVisualParamWeight("Shoe_Heels") * 0.08f;
		shoes += gAgentAvatarp->getVisualParamWeight("Shoe_Platform") * 0.07f;
		std::string sizestr = llformat("%.2f", shoes) + "m";
		gFloaterCustomizep->getChild<LLTextBox>("ShoesText")->setValue(sizestr);
		// mBodySize is actually quite a bit off.
		F32 avatar_size = gAgentAvatarp->mBodySize.mV[VZ] - shoes + 0.17f;
		sizestr = llformat("%.2f", avatar_size) + "m";
		gFloaterCustomizep->getChild<LLTextBox>("HeightTextM")->setValue(sizestr);
		F32 feet = avatar_size / 0.3048f;
		F32 inches = (feet - (F32)((U32)feet)) * 12.0f;
		sizestr = llformat("%d'%d\"", (U32)feet, (U32)inches);
		gFloaterCustomizep->getChild<LLTextBox>("HeightTextI")->setValue(sizestr);
		sizestr = llformat("%.2f", gAgentAvatarp->getPelvisToFoot()) + "m";
		gFloaterCustomizep->getChild<LLTextBox>("PelvisToFootText")->setValue(sizestr);
	}
}

//static
void LLFloaterCustomize::setCurrentWearableType(LLWearableType::EType type)
{
	if (LLFloaterCustomize::sCurrentWearableType != type)
	{
		LLFloaterCustomize::sCurrentWearableType = type;

		S32 type_int = (S32)type;
		if (gFloaterCustomizep &&
			gFloaterCustomizep->mWearablePanelList[type_int])
		{
			std::string panelname =
				gFloaterCustomizep->mWearablePanelList[type_int]->getName();
			gFloaterCustomizep->childShowTab("customize tab container",
											 panelname);
			gFloaterCustomizep->switchToDefaultSubpart();
		}
	}
}

//static
void LLFloaterCustomize::onBtnOk(void* userdata)
{
	gAgentWearables.saveAllWearables();

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->invalidateAll();
		gAgentAvatarp->requestLayerSetUploads();
		gAgent.sendAgentSetAppearance();
	}

	if (gFloaterViewp)
	{
		gFloaterViewp->sendChildToBack((LLFloaterCustomize*)userdata);
	}
	handle_reset_view();  // Calls askToSaveIfDirty
}

//static
void LLFloaterCustomize::onBtnMakeOutfit(void* userdata)
{
	HBFloaterMakeNewOutfit::showInstance();
}

//static
void* LLFloaterCustomize::createWearablePanel(void* userdata)
{
	WearablePanelData* data = (WearablePanelData*)userdata;
	LLWearableType::EType type = data->mType;
	LLPanelEditWearable* panel = new LLPanelEditWearable(type);
	data->mFloater->mWearablePanelList[type] = panel;
	delete data;
	return panel;
}

void LLFloaterCustomize::initWearablePanels()
{
	LLSubpart* part;

	/////////////////////////////////////////
	// Shape
	LLPanelEditWearable* panel = mWearablePanelList[LLWearableType::WT_SHAPE];

	// body
	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
	part->mEditGroup = "shape_body";
	part->mTargetOffset.set(0.f, 0.f, 0.1f);
	part->mCameraOffset.set(-2.5f, 0.5f, 0.8f);
	panel->addSubpart("Body", SUBPART_SHAPE_WHOLE, part);

	// head supparts
	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "shape_head";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Head", SUBPART_SHAPE_HEAD, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "shape_eyes";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Eyes", SUBPART_SHAPE_EYES, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "shape_ears";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Ears", SUBPART_SHAPE_EARS, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "shape_nose";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Nose", SUBPART_SHAPE_NOSE, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "shape_mouth";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Mouth", SUBPART_SHAPE_MOUTH, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "shape_chin";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Chin", SUBPART_SHAPE_CHIN, part);

	// torso
	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_TORSO;
	part->mEditGroup = "shape_torso";
	part->mTargetOffset.set(0.f, 0.f, 0.3f);
	part->mCameraOffset.set(-1.f, 0.15f, 0.3f);
	panel->addSubpart("Torso", SUBPART_SHAPE_TORSO, part);

	// legs
	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
	part->mEditGroup = "shape_legs";
	part->mTargetOffset.set(0.f, 0.f, -0.5f);
	part->mCameraOffset.set(-1.6f, 0.15f, -0.5f);
	panel->addSubpart("Legs", SUBPART_SHAPE_LEGS, part);

	/////////////////////////////////////////
	// Skin
	panel = mWearablePanelList[LLWearableType::WT_SKIN];

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "skin_color";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Skin Color", SUBPART_SKIN_COLOR, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "skin_facedetail";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Face Detail", SUBPART_SKIN_FACEDETAIL, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "skin_makeup";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Makeup", SUBPART_SKIN_MAKEUP, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
	part->mEditGroup = "skin_bodydetail";
	part->mTargetOffset.set(0.f, 0.f, -0.2f);
	part->mCameraOffset.set(-2.5f, 0.5f, 0.5f);
	panel->addSubpart("Body Detail", SUBPART_SKIN_BODYDETAIL, part);

	panel->addTextureDropTarget(TEX_HEAD_BODYPAINT, "Head Skin",
								LLUUID::null, true);
	panel->addTextureDropTarget(TEX_UPPER_BODYPAINT, "Upper Body",
								LLUUID::null, true);
	panel->addTextureDropTarget(TEX_LOWER_BODYPAINT, "Lower Body",
								LLUUID::null, true);

	/////////////////////////////////////////
	// Hair
	panel = mWearablePanelList[LLWearableType::WT_HAIR];

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "hair_color";
	part->mTargetOffset.set(0.f, 0.f, 0.10f);
	part->mCameraOffset.set(-0.4f, 0.05f, 0.10f);
	panel->addSubpart("Color", SUBPART_HAIR_COLOR, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "hair_style";
	part->mTargetOffset.set(0.f, 0.f, 0.10f);
	part->mCameraOffset.set(-0.4f, 0.05f, 0.10f);
	panel->addSubpart("Style", SUBPART_HAIR_STYLE, part);

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "hair_eyebrows";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Eyebrows", SUBPART_HAIR_EYEBROWS, part);

	part = new LLSubpart();
	part->mSex = SEX_MALE;
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "hair_facial";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart("Facial", SUBPART_HAIR_FACIAL, part);

	panel->addTextureDropTarget(TEX_HAIR, "Texture",
								LLUUID(gSavedSettings.getString("UIImgDefaultHairUUID")));

	/////////////////////////////////////////
	// Eyes
	panel = mWearablePanelList[LLWearableType::WT_EYES];

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_HEAD;
	part->mEditGroup = "eyes";
	part->mTargetOffset.set(0.f, 0.f, 0.05f);
	part->mCameraOffset.set(-0.5f, 0.05f, 0.07f);
	panel->addSubpart(LLStringUtil::null, SUBPART_EYES, part);

	panel->addTextureDropTarget(TEX_EYES_IRIS, "Iris",
								LLUUID(gSavedSettings.getString("UIImgDefaultEyesUUID")));

	/////////////////////////////////////////
	// Shirt
	panel = mWearablePanelList[LLWearableType::WT_SHIRT];

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_TORSO;
	part->mEditGroup = "shirt";
	part->mTargetOffset.set(0.f, 0.f, 0.3f);
	part->mCameraOffset.set(-1.f, 0.15f, 0.3f);
	panel->addSubpart(LLStringUtil::null, SUBPART_SHIRT, part);

	panel->addTextureDropTarget(TEX_UPPER_SHIRT, "Fabric",
								LLUUID(gSavedSettings.getString("UIImgDefaultShirtUUID")));

	panel->addColorSwatch(TEX_UPPER_SHIRT, "Color/Tint");

	/////////////////////////////////////////
	// Pants
	panel = mWearablePanelList[LLWearableType::WT_PANTS];

	part = new LLSubpart();
	part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
	part->mEditGroup = "pants";
	part->mTargetOffset.set(0.f, 0.f, -0.5f);
	part->mCameraOffset.set(-1.6f, 0.15f, -0.5f);
	panel->addSubpart(LLStringUtil::null, SUBPART_PANTS, part);

	panel->addTextureDropTarget(TEX_LOWER_PANTS, "Fabric",
								LLUUID(gSavedSettings.getString("UIImgDefaultPantsUUID")));

	panel->addColorSwatch(TEX_LOWER_PANTS, "Color/Tint");

	/////////////////////////////////////////
	// Shoes
	panel = mWearablePanelList[LLWearableType::WT_SHOES];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "shoes";
		part->mTargetOffset.set(0.f, 0.f, -0.5f);
		part->mCameraOffset.set(-1.6f, 0.15f, -0.5f);
		panel->addSubpart(LLStringUtil::null, SUBPART_SHOES, part);

		panel->addTextureDropTarget(TEX_LOWER_SHOES, "Fabric",
									LLUUID(gSavedSettings.getString("UIImgDefaultShoesUUID")));

		panel->addColorSwatch(TEX_LOWER_SHOES, "Color/Tint");
	}

	/////////////////////////////////////////
	// Socks
	panel = mWearablePanelList[LLWearableType::WT_SOCKS];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "socks";
		part->mTargetOffset.set(0.f, 0.f, -0.5f);
		part->mCameraOffset.set(-1.6f, 0.15f, -0.5f);
		panel->addSubpart(LLStringUtil::null, SUBPART_SOCKS, part);

		panel->addTextureDropTarget(TEX_LOWER_SOCKS, "Fabric",
									LLUUID(gSavedSettings.getString("UIImgDefaultSocksUUID")));

		panel->addColorSwatch(TEX_LOWER_SOCKS, "Color/Tint");
	}

	/////////////////////////////////////////
	// Jacket
	panel = mWearablePanelList[LLWearableType::WT_JACKET];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_TORSO;
		part->mEditGroup = "jacket";
		part->mTargetOffset.set(0.f, 0.f, 0.f);
		part->mCameraOffset.set(-2.f, 0.1f, 0.3f);
		panel->addSubpart(LLStringUtil::null, SUBPART_JACKET, part);

		panel->addTextureDropTarget(TEX_UPPER_JACKET, "Upper Fabric",
									LLUUID(gSavedSettings.getString("UIImgDefaultJacketUUID")));
		panel->addTextureDropTarget(TEX_LOWER_JACKET, "Lower Fabric",
									LLUUID(gSavedSettings.getString("UIImgDefaultJacketUUID")));

		panel->addColorSwatch(TEX_UPPER_JACKET, "Color/Tint");
	}

	/////////////////////////////////////////
	// Skirt
	panel = mWearablePanelList[LLWearableType::WT_SKIRT];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "skirt";
		part->mTargetOffset.set(0.f, 0.f, -0.5f);
		part->mCameraOffset.set(-1.6f, 0.15f, -0.5f);
		panel->addSubpart(LLStringUtil::null, SUBPART_SKIRT, part);

		panel->addTextureDropTarget(TEX_SKIRT, "Fabric",
									LLUUID(gSavedSettings.getString("UIImgDefaultSkirtUUID")));

		panel->addColorSwatch(TEX_SKIRT, "Color/Tint");
	}

	/////////////////////////////////////////
	// Gloves
	panel = mWearablePanelList[LLWearableType::WT_GLOVES];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_TORSO;
		part->mEditGroup = "gloves";
		part->mTargetOffset.set(0.f, 0.f, 0.f);
		part->mCameraOffset.set(-1.f, 0.15f, 0.f);
		panel->addSubpart(LLStringUtil::null, SUBPART_GLOVES, part);

		panel->addTextureDropTarget(TEX_UPPER_GLOVES, "Fabric",
									LLUUID(gSavedSettings.getString("UIImgDefaultGlovesUUID")));

		panel->addColorSwatch(TEX_UPPER_GLOVES, "Color/Tint");
	}

	/////////////////////////////////////////
	// Undershirt
	panel = mWearablePanelList[LLWearableType::WT_UNDERSHIRT];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_TORSO;
		part->mEditGroup = "undershirt";
		part->mTargetOffset.set(0.f, 0.f, 0.3f);
		part->mCameraOffset.set(-1.f, 0.15f, 0.3f);
		panel->addSubpart(LLStringUtil::null, SUBPART_UNDERSHIRT, part);

		panel->addTextureDropTarget(TEX_UPPER_UNDERSHIRT, "Fabric",
									LLUUID(gSavedSettings.getString("UIImgDefaultUnderwearUUID")));

		panel->addColorSwatch(TEX_UPPER_UNDERSHIRT, "Color/Tint");
	}

	/////////////////////////////////////////
	// Underpants
	panel = mWearablePanelList[LLWearableType::WT_UNDERPANTS];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "underpants";
		part->mTargetOffset.set(0.f, 0.f, -0.5f);
		part->mCameraOffset.set(-1.6f, 0.15f, -0.5f);
		panel->addSubpart(LLStringUtil::null, SUBPART_UNDERPANTS, part);

		panel->addTextureDropTarget(TEX_LOWER_UNDERPANTS, "Fabric",
									LLUUID(gSavedSettings.getString("UIImgDefaultUnderwearUUID")));

		panel->addColorSwatch(TEX_LOWER_UNDERPANTS, "Color/Tint");
	}

	/////////////////////////////////////////
	// Alpha
	panel = mWearablePanelList[LLWearableType::WT_ALPHA];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "alpha";
		part->mTargetOffset.set(0.f, 0.f, 0.1f);
		part->mCameraOffset.set(-2.5f, 0.5f, 0.8f);
		panel->addSubpart(LLStringUtil::null, SUBPART_ALPHA, part);

		panel->addTextureDropTarget(TEX_LOWER_ALPHA, "Lower Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									true);
		panel->addTextureDropTarget(TEX_UPPER_ALPHA, "Upper Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									true);
		panel->addTextureDropTarget(TEX_HEAD_ALPHA, "Head Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									true);
		panel->addTextureDropTarget(TEX_EYES_ALPHA, "Eye Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									true);
		panel->addTextureDropTarget(TEX_HAIR_ALPHA, "Hair Alpha",
									LLUUID(gSavedSettings.getString("UIImgDefaultAlphaUUID")),
									true);

		panel->addInvisibilityCheckbox(TEX_LOWER_ALPHA, "lower alpha texture invisible");
		panel->addInvisibilityCheckbox(TEX_UPPER_ALPHA, "upper alpha texture invisible");
		panel->addInvisibilityCheckbox(TEX_HEAD_ALPHA, "head alpha texture invisible");
		panel->addInvisibilityCheckbox(TEX_EYES_ALPHA, "eye alpha texture invisible");
		panel->addInvisibilityCheckbox(TEX_HAIR_ALPHA, "hair alpha texture invisible");
	}

	/////////////////////////////////////////
	// Tattoo
	panel = mWearablePanelList[LLWearableType::WT_TATTOO];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "tattoo";
		part->mTargetOffset.set(0.f, 0.f, 0.1f);
		part->mCameraOffset.set(-2.5f, 0.5f, 0.8f);
		panel->addSubpart(LLStringUtil::null, SUBPART_TATTOO, part);

		panel->addTextureDropTarget(TEX_LOWER_TATTOO, "Lower Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_UPPER_TATTOO, "Upper Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_HEAD_TATTOO, "Head Tattoo",
									LLUUID::null, true);
		panel->addColorSwatch(TEX_LOWER_TATTOO, "Color/Tint");
	}

	/////////////////////////////////////////
	// Universal
	panel = mWearablePanelList[LLWearableType::WT_UNIVERSAL];
	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "universal";
		part->mTargetOffset.set(0.f, 0.f, 0.1f);
		part->mCameraOffset.set(-2.5f, 0.5f, 0.8f);
		panel->addSubpart(LLStringUtil::null, SUBPART_UNIVERSAL, part);

		panel->addTextureDropTarget(TEX_HEAD_UNIVERSAL_TATTOO,
									"Head Universal Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_HAIR_TATTOO, "Hair Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_EYES_TATTOO, "Eyes Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_UPPER_UNIVERSAL_TATTOO,
									"Upper Universal Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_LEFT_ARM_TATTOO, "Left Arm Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_LOWER_UNIVERSAL_TATTOO,
									"Lower Universal Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_LEFT_LEG_TATTOO, "Left Leg Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_SKIRT_TATTOO, "Skirt Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_AUX1_TATTOO, "Aux1 Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_AUX2_TATTOO, "Aux2 Tattoo",
									LLUUID::null, true);
		panel->addTextureDropTarget(TEX_AUX3_TATTOO, "Aux3 Tattoo",
									LLUUID::null, true);
		panel->addColorSwatch(TEX_HEAD_UNIVERSAL_TATTOO, "Color/Tint");
	}

	/////////////////////////////////////////
	// Physics
	panel = mWearablePanelList[LLWearableType::WT_PHYSICS];
	if (panel)
	{
		part = new LLSubpart();
		part->mSex = SEX_FEMALE;
		part->mTargetJointKey = LL_JOINT_KEY_TORSO;
		part->mEditGroup = "physics_breasts_updown";
		part->mTargetOffset.set(0.f, 0.f, 0.1f);
		part->mCameraOffset.set(-0.8f, 0.15f, 0.38f);
		part->mVisualHint = false;
		panel->addSubpart("Breast Bounce", SUBPART_PHYSICS_BREASTS_UPDOWN, part);

		part = new LLSubpart();
		part->mSex = SEX_FEMALE;
		part->mTargetJointKey = LL_JOINT_KEY_TORSO;
		part->mEditGroup = "physics_breasts_inout";
		part->mTargetOffset.set(0.f, 0.f, 0.1f);
		part->mCameraOffset.set(-0.8f, 0.15f, 0.38f);
		part->mVisualHint = false;
		panel->addSubpart("Breast Cleavage", SUBPART_PHYSICS_BREASTS_INOUT, part);

		part = new LLSubpart();
		part->mSex = SEX_FEMALE;
		part->mTargetJointKey = LL_JOINT_KEY_TORSO;
		part->mEditGroup = "physics_breasts_leftright";
		part->mTargetOffset.set(0.f, 0.f, 0.1f);
		part->mCameraOffset.set(-0.8f, 0.15f, 0.38f);
		part->mVisualHint = false;
		panel->addSubpart("Breast Sway", SUBPART_PHYSICS_BREASTS_LEFTRIGHT, part);

		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_TORSO;
		part->mEditGroup = "physics_belly_updown";
		part->mTargetOffset.set(0.f, 0.f, 0.1f);
		part->mCameraOffset.set(-0.8f, 0.15f, 0.38f);
		part->mVisualHint = false;
		panel->addSubpart("Belly Bounce", SUBPART_PHYSICS_BELLY_UPDOWN, part);

		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "physics_butt_updown";
		part->mTargetOffset.set(0.f, 0.f, -0.1f);
		part->mCameraOffset.set(0.3f, 0.8f, -0.1f);
		part->mVisualHint = false;
		panel->addSubpart("Butt Bounce", SUBPART_PHYSICS_BUTT_UPDOWN, part);

		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_PELVIS;
		part->mEditGroup = "physics_butt_leftright";
		part->mTargetOffset.set(0.f, 0.f, -0.1f);
		part->mCameraOffset.set(0.3f, 0.8f, -0.1f);
		part->mVisualHint = false;
		panel->addSubpart("Butt Sway", SUBPART_PHYSICS_BUTT_LEFTRIGHT, part);

		part = new LLSubpart();
		part->mTargetJointKey = LL_JOINT_KEY_TORSO;
		part->mEditGroup = "physics_advanced";
		part->mTargetOffset.set(0.f, 0.f, 0.1f);
		part->mCameraOffset.set(-2.5f, 0.5f, 0.8f);
		part->mVisualHint = false;
		panel->addSubpart("Advanced Parameters", SUBPART_PHYSICS_ADVANCED, part);
	}
}

LLFloaterCustomize::~LLFloaterCustomize()
{
	llinfos << "Destroying LLFloaterCustomize" << llendl;
	mResetParams = NULL;
	gInventory.removeObserver(mInventoryObserver);
	delete mInventoryObserver;
	LLVOAvatarSelf::onCustomizeEnd();

	// Update Avatar Z offset according to AVATAR_HOVER if needed
	if (LLVOAvatarSelf::canUseServerBaking() &&
		!LLVOAvatarSelf::useAvatarHoverHeight())
	{
		LLViewerWearable* shape;
		shape = gAgentWearables.getViewerWearable(LLWearableType::WT_SHAPE, 0);
		if (shape)
		{
			F32 offset = shape->getVisualParamWeight(AVATAR_HOVER);
#if 1
			F32 factor = gSavedSettings.getF32("HoverToZOffsetFactor");
			if (factor > 1.f)
			{
				// Hover is wrongly accounted twice in LL's viewer...
				offset *= factor;
			}
#endif
			gSavedSettings.setF32("AvatarOffsetZ", offset);
		}
	}
}

void LLFloaterCustomize::switchToDefaultSubpart()
{
	getCurrentWearablePanel()->switchToDefaultSubpart();
}

void LLFloaterCustomize::draw()
{
	if (!isMinimized())
	{
		// Only do this if we are in the customize avatar mode and not
		// transitioning into or out of it
		// *TODO: This is a sort of expensive call, which only needs to be
		// called when the tabs change or an inventory item arrives. Figure
		// out some way to avoid this if possible.
		updateInventoryUI();

		updateAvatarHeightDisplay();

		LLScrollingPanelParam::sUpdateDelayFrames = 0;
	}

	LLFloater::draw();
}

bool LLFloaterCustomize::isDirty() const
{
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		if (mWearablePanelList[i] && mWearablePanelList[i]->isDirty())
		{
			return true;
		}
	}
	return false;
}

//static
void LLFloaterCustomize::onTabPrecommit(void* userdata, bool from_click)
{
	LLWearableType::EType type = (LLWearableType::EType)(intptr_t) userdata;
	if (type != LLWearableType::WT_INVALID && gFloaterCustomizep &&
		gFloaterCustomizep->getCurrentWearableType() != type)
	{
		gFloaterCustomizep->askToSaveIfDirty(onCommitChangeTab, userdata);
	}
	else
	{
		onCommitChangeTab(true, NULL);
	}
}

//static
void LLFloaterCustomize::onTabChanged(void* userdata, bool from_click)
{
	LLWearableType::EType wearable_type = (LLWearableType::EType) (intptr_t)userdata;
	if (wearable_type != LLWearableType::WT_INVALID)
	{
		LLFloaterCustomize::setCurrentWearableType(wearable_type);
	}
}

void LLFloaterCustomize::onClose(bool app_quitting)
{
	// since this window is potentially staying open, push to back to let next
	// window take focus
	if (gFloaterViewp)
	{
		gFloaterViewp->sendChildToBack(this);
	}
	handle_reset_view();  // Calls askToSaveIfDirty
}

//static
void LLFloaterCustomize::onCommitChangeTab(bool proceed, void* userdata)
{
	if (!proceed || !gFloaterCustomizep)
	{
		return;
	}

	LLTabContainer* tab;
	tab = gFloaterCustomizep->getChild<LLTabContainer>("customize tab container",
													   true, false);
	if (tab)
	{
		tab->setTab(-1);
	}
}

void LLFloaterCustomize::initScrollingPanelList()
{
	LLScrollableContainer* scroll_container = 
		getChild<LLScrollableContainer>("panel_container", true, false);
#if 0	// LLScrollingPanelList's do not import correctly
	mScrollingPanelList = LLUICtrlFactory::getScrollingPanelList(this,
																 "panel_list");
#else
	mScrollingPanelList = new LLScrollingPanelList("panel_list", LLRect());
#endif

	if (scroll_container)
	{
		scroll_container->setScrolledView(mScrollingPanelList);
		scroll_container->addChild(mScrollingPanelList);
	}
}

void LLFloaterCustomize::clearScrollingPanelList()
{
	if (mScrollingPanelList)
	{
		mScrollingPanelList->clearPanels();
	}
}

void LLFloaterCustomize::generateVisualParamHints(LLPanelEditWearable* panel,
												  LLViewerJointMesh* joint_mesh,
												  LLFloaterCustomize::param_map& params,
												  LLWearable* wearable,
												  bool use_hints,
												  LLJoint* jointp)
{
	// sorted_params is sorted according to magnitude of effect from least to
	// greatest. Adding to the front of the child list reverses that order.
	if (mScrollingPanelList)
	{
		mScrollingPanelList->clearPanels();
		for (param_map::iterator it = params.begin(), end = params.end();
			 it != end; ++it)
		{
			mScrollingPanelList->addPanel(new LLScrollingPanelParam(panel,
																	joint_mesh,
																	it->second.second,
																	it->second.first,
																	wearable,
																	jointp,
																	use_hints));
		}
	}
}

void LLFloaterCustomize::setWearable(LLWearableType::EType type,
									 LLViewerWearable* wearable,
									 U32 perm_mask, bool is_complete)
{
	llassert(type < LLWearableType::WT_COUNT);
	LLPanelEditWearable* panel = mWearablePanelList[type];
	if (panel && isAgentAvatarValid())
	{
		gSavedSettings.setU32("AvatarSex", (gAgentAvatarp->getSex() == SEX_MALE));
		panel->setWearable(wearable, perm_mask, is_complete);
		bool allow_modify = wearable && is_complete &&
							(perm_mask & PERM_MODIFY) != 0;
		updateScrollingPanelList(allow_modify);
	}
}

void LLFloaterCustomize::updateScrollingPanelList(bool allow_modify)
{
	if (mScrollingPanelList)
	{
		LLScrollingPanelParam::sUpdateDelayFrames = 0;
		mScrollingPanelList->updatePanels(allow_modify);
	}
}

void LLFloaterCustomize::askToSaveIfDirty(void(*next_step_callback)(bool, void*),
										  void* userdata)
{
	if (isDirty())
	{
		// Ask if user wants to save, then continue to next step afterwards
		mNextStepAfterSaveCallback = next_step_callback;
		mNextStepAfterSaveUserdata = userdata;

		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		gNotifications.add("SaveClothingBodyChanges", LLSD(), LLSD(),
						   boost::bind(&LLFloaterCustomize::onSaveDialog, this,
									   _1, _2));
		return;
	}

	// Try to move to the next step
	if (next_step_callback)
	{
		next_step_callback(true, userdata);
	}
}

bool LLFloaterCustomize::onSaveDialog(const LLSD& notification,
									  const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	bool proceed = false;
	U32 index = 0;
	LLPanelEditWearable* panel = mWearablePanelList[sCurrentWearableType];
	if (panel)
	{
		index = panel->getWearableIndex();
	}

	switch (option)
	{
		case 0:  // "Save"
		{
			gAgentWearables.saveWearable(sCurrentWearableType, index);
			proceed = true;
			break;
		}

		case 1:  // "Don't Save"
		{
			gAgentWearables.revertWearable(sCurrentWearableType, index);
			proceed = true;
			break;
		}

		case 2: // "Cancel"
			break;

		default:
			llassert(0);
	}

	if (mNextStepAfterSaveCallback)
	{
		mNextStepAfterSaveCallback(proceed, mNextStepAfterSaveUserdata);
	}
	return false;
}

// Fetch observer
class LLCurrentlyWorn final : public LLInventoryFetchObserver
{
public:
	LLCurrentlyWorn()				{}
	~LLCurrentlyWorn() override		{}

	// No operation necessary
	void done() override			{}
};

void LLFloaterCustomize::fetchInventory()
{
	// Fetch currently worn items
	uuid_vec_t ids;
	for (S32 i = 0; i < (S32)LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		U32 count = gAgentWearables.getWearableCount(type);
		for (U32 index = 0; index < count; ++index)
		{
			const LLUUID& item_id = gAgentWearables.getWearableItemID(type,
																	  index);
			if (item_id.notNull())
			{
				ids.emplace_back(item_id);
			}
		}
	}

	// Fire & forget. The mInventoryObserver will catch inventory
	// updates and correct the UI as necessary.
	LLCurrentlyWorn worn;
	worn.fetchItems(ids);
}

void LLFloaterCustomize::updateInventoryUI()
{
	bool all_complete = true;
	bool is_complete = false;
	U32 perm_mask = 0x0;
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		LLViewerInventoryItem* item = NULL;
		LLPanelEditWearable* panel = mWearablePanelList[i];
		if (panel)
		{
			U32 index = panel->getWearableIndex();
			item = gAgentWearables.getWearableInventoryItem(panel->getType(),
															index);
		}
		if (item)
		{
			is_complete = item->isFinished();
			if (!is_complete)
			{
				all_complete = false;
			}
			perm_mask = item->getPermissions().getMaskOwner();
		}
		else
		{
			is_complete = false;
			perm_mask = 0x0;
		}
		if (i == sCurrentWearableType)
		{
			if (panel)
			{
				panel->setUIPermissions(perm_mask, is_complete);
			}
			bool is_vis = panel && item && is_complete && (perm_mask & PERM_MODIFY);
			childSetVisible("panel_container", is_vis);
		}
	}

	childSetEnabled("Make Outfit", all_complete);
}

void LLFloaterCustomize::updateScrollingPanelUI()
{
	LLPanelEditWearable* panel = mWearablePanelList[sCurrentWearableType];
	if (panel)
	{
		U32 index = panel->getWearableIndex();
		LLViewerInventoryItem* item =
			gAgentWearables.getWearableInventoryItem(panel->getType(), index);
		bool allow_modify = false;
		if (item)
		{
			U32 perm_mask = item->getPermissions().getMaskOwner();
			allow_modify = (perm_mask & PERM_MODIFY) ? item->isFinished()
													 : false;
		}
		updateScrollingPanelList(allow_modify);
	}
}

void LLFloaterCustomize::updateWearableType(LLWearableType::EType type,
											LLViewerWearable* wearable)
{
	setCurrentWearableType(type);

	U32 perm_mask = PERM_NONE;
	bool is_complete = false;
	if (!wearable && gAgentWearables.getWearableCount(type))
	{
		// Select the first layer
		wearable = gAgentWearables.getViewerWearable(type, 0);
	}
	if (wearable)
	{
		LLViewerInventoryItem* item;
		item = (LLViewerInventoryItem*)gInventory.getItem(wearable->getItemID());
		if (item)
		{
			perm_mask = item->getPermissions().getMaskOwner();
			is_complete = item->isFinished();
			if (!is_complete)
			{
				item->fetchFromServer();
			}
		}
	}
	else
	{
		perm_mask = PERM_ALL;
		is_complete = true;
	}

	setWearable(type, wearable, perm_mask, is_complete);
}
