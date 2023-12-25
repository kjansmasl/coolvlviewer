/**
 * @file llfloatergodtools.cpp
 * @brief The on-screen rectangle with tool options.
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

#include "llfloatergodtools.h"

#include "llcombobox.h"
#include "llfloater.h"
#include "llhost.h"
#include "lllineeditor.h"
#include "llnotifications.h"
#include "llregionflags.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "llxfermanager.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfloateravatarpicker.h"
#include "llfloatertopobjects.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llworld.h"

constexpr F32 SECONDS_BETWEEN_UPDATE_REQUESTS = 5.f;

//*****************************************************************************
// LLFloaterGodTools
//*****************************************************************************

LLFloaterGodTools::LLFloaterGodTools(const LLSD&)
:	mCurrentHost(LLHost()),
	mUpdateTimer()
{
	LLCallbackMap::map_t factory_map;
	factory_map["grid"] = LLCallbackMap(createPanelGrid, this);
	factory_map["region"] = LLCallbackMap(createPanelRegion, this);
	factory_map["objects"] = LLCallbackMap(createPanelObjects, this);
	factory_map["request"] = LLCallbackMap(createPanelRequest, this);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_god_tools.xml",
												 &factory_map);
}

///virtual
bool LLFloaterGodTools::postBuild()
{
	childSetTabChangeCallback("GodTools Tabs", "grid", onTabChanged, this);
	childSetTabChangeCallback("GodTools Tabs", "region", onTabChanged, this);
	childSetTabChangeCallback("GodTools Tabs", "objects", onTabChanged, this);
	childSetTabChangeCallback("GodTools Tabs", "request", onTabChanged, this);

	childShowTab("GodTools Tabs", "region");

	center();

	setFocus(true);

	return true;
}

//virtual
void LLFloaterGodTools::onOpen()
{
	LLPanel* panel = childGetVisibleTab("GodTools Tabs");
	panel->setFocus(true);

	if (mPanelObjectTools)
	{
		mPanelObjectTools->setTargetAvatar(LLUUID::null);
	}

	if (gAgent.getRegionHost() != mCurrentHost)
	{
		// we're in a new region
		sendRegionInfoRequest();
	}
}

//static
void* LLFloaterGodTools::createPanelGrid(void* userdata)
{
	return new LLPanelGridTools("grid");
}

//static
void* LLFloaterGodTools::createPanelRegion(void* userdata)
{
	LLFloaterGodTools* self = (LLFloaterGodTools*)userdata;
	self->mPanelRegionTools = new LLPanelRegionTools("region");
	return self->mPanelRegionTools;
}

//static
void* LLFloaterGodTools::createPanelObjects(void* userdata)
{
	LLFloaterGodTools* self = (LLFloaterGodTools*)userdata;
	self->mPanelObjectTools = new LLPanelObjectTools("objects");
	return self->mPanelObjectTools;
}

//static
void* LLFloaterGodTools::createPanelRequest(void* userdata)
{
	return new LLPanelRequestTools("region");
}

U64 LLFloaterGodTools::computeRegionFlags() const
{
	if (!gAgent.getRegion()) return 0;
	U64 flags = gAgent.getRegion()->getRegionFlags();
	if (mPanelRegionTools) flags = mPanelRegionTools->computeRegionFlags(flags);
	if (mPanelObjectTools) flags = mPanelObjectTools->computeRegionFlags(flags);
	return flags;
}

// virtual
void LLFloaterGodTools::draw()
{
	if (mCurrentHost.isInvalid())
	{
		if (mUpdateTimer.getElapsedTimeF32() > SECONDS_BETWEEN_UPDATE_REQUESTS)
		{
			sendRegionInfoRequest();
		}
	}
	else if (gAgent.getRegionHost() != mCurrentHost)
	{
		sendRegionInfoRequest();
	}
	LLFloater::draw();
}

//static
void LLFloaterGodTools::onTabChanged(void* data, bool from_click)
{
	LLPanel* panel = (LLPanel*)data;
	if (panel)
	{
		panel->setFocus(true);
	}
}

//static
void LLFloaterGodTools::updateFromRegionInfo()
{
	LLFloaterGodTools* self = findInstance();
	// Push values to god tools, if available
	if (self && self->mPanelRegionTools && self->mPanelObjectTools &&
		gAgent.isGodlike())
	{
		LLPanelRegionTools* rtool = self->mPanelRegionTools;
		// We know we are in the agent's region, else this method would not
		// have been called by LLViewerRegion::processRegionInfo()
		self->mCurrentHost = gAgent.getRegionHost();

		// Store locally
		rtool->setSimName(LLRegionInfoModel::sSimName);
		rtool->setEstateID(LLRegionInfoModel::sEstateID);
		rtool->setParentEstateID(LLRegionInfoModel::sParentEstateID);
		rtool->setCheckFlags(LLRegionInfoModel::sRegionFlags);
		rtool->setBillableFactor(LLRegionInfoModel::sBillableFactor);
		rtool->setPricePerMeter(LLRegionInfoModel::sPricePerMeter);
		rtool->setRedirectGridX(LLRegionInfoModel::sRedirectGridX);
		rtool->setRedirectGridY(LLRegionInfoModel::sRedirectGridY);
		rtool->enableAllWidgets();

		LLPanelObjectTools* otool = self->mPanelObjectTools;
		otool->setCheckFlags(LLRegionInfoModel::sRegionFlags);
		otool->enableAllWidgets();

		LLViewerRegion* regionp = gAgent.getRegion();
		if (!regionp)
		{
			// -1 implies non-existent
			rtool->setGridPosX(-1);
			rtool->setGridPosY(-1);
		}
		else
		{
			// Compute the grid position of the region
			LLVector3d global_pos =
				regionp->getPosGlobalFromRegion(LLVector3::zero);
			S32 grid_pos_x = (S32) (global_pos.mdV[VX] / 256.0f);
			S32 grid_pos_y = (S32) (global_pos.mdV[VY] / 256.0f);

			rtool->setGridPosX(grid_pos_x);
			rtool->setGridPosY(grid_pos_y);
		}
	}
}

void LLFloaterGodTools::sendRegionInfoRequest()
{
	if (mPanelRegionTools)
	{
		mPanelRegionTools->clearAllWidgets();
	}
	if (mPanelObjectTools)
	{
		mPanelObjectTools->clearAllWidgets();
	}

	mCurrentHost = LLHost();
	mUpdateTimer.reset();

	LLMessageSystem* msg = gMessageSystemp;
	if (msg)
	{
		msg->newMessage(_PREHASH_RequestRegionInfo);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		gAgent.sendReliableMessage();
	}
}

void LLFloaterGodTools::sendGodUpdateRegionInfo()
{
	if (mPanelRegionTools && gAgent.isGodlike() && gAgent.getRegion() &&
		gAgent.getRegionHost() == mCurrentHost)
	{
		U64 region_flags = computeRegionFlags();
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage("GodUpdateRegionInfo");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_RegionInfo);
		msg->addStringFast(_PREHASH_SimName,
						   mPanelRegionTools->getSimName());
		msg->addU32Fast(_PREHASH_EstateID, mPanelRegionTools->getEstateID());
		msg->addU32Fast(_PREHASH_ParentEstateID,
						mPanelRegionTools->getParentEstateID());
		// Legacy flags
		msg->addU32Fast(_PREHASH_RegionFlags, U32(region_flags));
		msg->addF32Fast(_PREHASH_BillableFactor,
						mPanelRegionTools->getBillableFactor());
		msg->addS32Fast(_PREHASH_PricePerMeter,
						mPanelRegionTools->getPricePerMeter());
		msg->addS32Fast(_PREHASH_RedirectGridX,
						mPanelRegionTools->getRedirectGridX());
		msg->addS32Fast(_PREHASH_RedirectGridY,
						mPanelRegionTools->getRedirectGridY());
		msg->nextBlockFast(_PREHASH_RegionInfo2);
		msg->addU64Fast(_PREHASH_RegionFlagsExtended, region_flags);

		gAgent.sendReliableMessage();
	}
}

//*****************************************************************************
// LLPanelRegionTools
//*****************************************************************************

//   || Region |______________________________________
//   |                                                |
//   |  Sim Name: [________________________________]  |
//   |  ^         ^                                   |
//   |  LEFT      R1         Estate id:   [----]      |
//   |                       Parent id:   [----]      |
//   |  [X] Prelude          Grid Pos:     [--] [--]  |
//   |  [X] Visible          Redirect Pos: [--] [--]  |
//   |  [X] Damage           Bill Factor  [8_______]  |
//   |  [X] Block Terraform  PricePerMeter[8_______]  |
//   |                                    [Apply]     |
//   |                                                |
//   |  [Bake Terrain]            [Select Region]     |
//   |  [Revert Terrain]          [Autosave Now]      |
//   |  [Swap Terrain]                                |
//   |				                                  |
//   |________________________________________________|
//      ^                    ^                     ^
//      LEFT                 R2                   RIGHT

// Floats because spinners only support floats. JC
constexpr F32 BILLABLE_FACTOR_DEFAULT = 1.f;
constexpr F32 PRICE_PER_METER_DEFAULT = 1.f;

LLPanelRegionTools::LLPanelRegionTools(const std::string& title)
: 	LLPanel(title)
{
}

//virtual
bool LLPanelRegionTools::postBuild()
{
	childSetCommitCallback("region name", onChangeAnything, this);
	childSetKeystrokeCallback("region name", onChangeSimName, this);
	childSetPrevalidate("region name", &LLLineEditor::prevalidatePrintableNotPipe);

	childSetCommitCallback("check prelude", onChangePrelude, this);
	childSetCommitCallback("check fixed sun", onChangeAnything, this);
	childSetCommitCallback("check reset home", onChangeAnything, this);
	childSetCommitCallback("check visible", onChangeAnything, this);
	childSetCommitCallback("check damage", onChangeAnything, this);
	childSetCommitCallback("block dwell", onChangeAnything, this);
	childSetCommitCallback("block terraform", onChangeAnything, this);
	childSetCommitCallback("is sandbox", onChangeAnything, this);

	childSetAction("Bake Terrain", onBakeTerrain, this);
	childSetAction("Revert Terrain", onRevertTerrain, this);
	childSetAction("Swap Terrain", onSwapTerrain, this);

	childSetCommitCallback("estate", onChangeAnything, this);
	childSetPrevalidate("estate", &LLLineEditor::prevalidatePositiveS32);

	childSetCommitCallback("parentestate", onChangeAnything, this);
	childSetPrevalidate("parentestate", &LLLineEditor::prevalidatePositiveS32);
	childDisable("parentestate");

	childSetCommitCallback("gridposx", onChangeAnything, this);
	childSetPrevalidate("gridposx", &LLLineEditor::prevalidatePositiveS32);
	childDisable("gridposx");

	childSetCommitCallback("gridposy", onChangeAnything, this);
	childSetPrevalidate("gridposy", &LLLineEditor::prevalidatePositiveS32);
	childDisable("gridposy");

	childSetCommitCallback("redirectx", onChangeAnything, this);
	childSetPrevalidate("redirectx", &LLLineEditor::prevalidatePositiveS32);

	childSetCommitCallback("redirecty", onChangeAnything, this);
	childSetPrevalidate("redirecty", &LLLineEditor::prevalidatePositiveS32);

	childSetCommitCallback("billable factor", onChangeAnything, this);

	childSetCommitCallback("land cost", onChangeAnything, this);

	childSetAction("Refresh", onRefresh, this);
	childSetAction("Apply", onApplyChanges, this);

	childSetAction("Select Region", onSelectRegion, this);
	childSetAction("Autosave now", onSaveState, this);

	return true;
}

U64 LLPanelRegionTools::computeRegionFlags(U64 flags) const
{
	flags &= getRegionFlagsMask();
	flags |= getRegionFlags();
	return flags;
}

void LLPanelRegionTools::clearAllWidgets()
{
	// clear all widgets
	childSetValue("region name", "unknown");
	childSetFocus("region name", false);

	childSetValue("check prelude", false);
	childDisable("check prelude");

	childSetValue("check fixed sun", false);
	childDisable("check fixed sun");

	childSetValue("check reset home", false);
	childDisable("check reset home");

	childSetValue("check damage", false);
	childDisable("check damage");

	childSetValue("check visible", false);
	childDisable("check visible");

	childSetValue("block terraform", false);
	childDisable("block terraform");

	childSetValue("block dwell", false);
	childDisable("block dwell");

	childSetValue("is sandbox", false);
	childDisable("is sandbox");

	childSetValue("billable factor", BILLABLE_FACTOR_DEFAULT);
	childDisable("billable factor");

	childSetValue("land cost", PRICE_PER_METER_DEFAULT);
	childDisable("land cost");

	childDisable("Apply");
	childDisable("Bake Terrain");
	childDisable("Autosave now");
}

void LLPanelRegionTools::enableAllWidgets()
{
	// enable all of the widgets

	childEnable("check prelude");
	childEnable("check fixed sun");
	childEnable("check reset home");
	childEnable("check damage");
	childDisable("check visible"); // use estates to update...
	childEnable("block terraform");
	childEnable("block dwell");
	childEnable("is sandbox");

	childEnable("billable factor");
	childEnable("land cost");

	childDisable("Apply");	// don't enable this one
	childEnable("Bake Terrain");
	childEnable("Autosave now");
}

//static
void LLPanelRegionTools::onSaveState(void* userdata)
{
	if (gAgent.isGodlike())
	{
		// Send message to save world state
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_StateSave);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_DataBlock);
		msg->addStringFast(_PREHASH_Filename, NULL);
		gAgent.sendReliableMessage();
	}
}

const std::string LLPanelRegionTools::getSimName() const
{
	return childGetValue("region name");
}

U32 LLPanelRegionTools::getEstateID() const
{
	U32 id = (U32)childGetValue("estate").asInteger();
	return id;
}

U32 LLPanelRegionTools::getParentEstateID() const
{
	U32 id = (U32)childGetValue("parentestate").asInteger();
	return id;
}

S32 LLPanelRegionTools::getRedirectGridX() const
{
	return childGetValue("redirectx").asInteger();
}

S32 LLPanelRegionTools::getRedirectGridY() const
{
	return childGetValue("redirecty").asInteger();
}

S32 LLPanelRegionTools::getGridPosX() const
{
	return childGetValue("gridposx").asInteger();
}

S32 LLPanelRegionTools::getGridPosY() const
{
	return childGetValue("gridposy").asInteger();
}

U64 LLPanelRegionTools::getRegionFlags() const
{
	U64 flags = 0x0;
	flags = childGetValue("check prelude").asBoolean() ? set_prelude_flags(flags)
													   : unset_prelude_flags(flags);

	// override prelude
	if (childGetValue("check fixed sun").asBoolean())
	{
		flags |= REGION_FLAGS_SUN_FIXED;
	}
	if (childGetValue("check reset home").asBoolean())
	{
		flags |= REGION_FLAGS_RESET_HOME_ON_TELEPORT;
	}
	if (childGetValue("check visible").asBoolean())
	{
		flags |= REGION_FLAGS_EXTERNALLY_VISIBLE;
	}
	if (childGetValue("check damage").asBoolean())
	{
		flags |= REGION_FLAGS_ALLOW_DAMAGE;
	}
	if (childGetValue("block terraform").asBoolean())
	{
		flags |= REGION_FLAGS_BLOCK_TERRAFORM;
	}
	if (childGetValue("block dwell").asBoolean())
	{
		flags |= REGION_FLAGS_BLOCK_DWELL;
	}
	if (childGetValue("is sandbox").asBoolean())
	{
		flags |= REGION_FLAGS_SANDBOX;
	}
	return flags;
}

U64 LLPanelRegionTools::getRegionFlagsMask() const
{
	U64 flags = 0xFFFFFFFFFFFFFFFFULL;
	flags = childGetValue("check prelude").asBoolean() ? set_prelude_flags(flags)
													   : unset_prelude_flags(flags);

	if (!childGetValue("check fixed sun").asBoolean())
	{
		flags &= ~REGION_FLAGS_SUN_FIXED;
	}
	if (!childGetValue("check reset home").asBoolean())
	{
		flags &= ~REGION_FLAGS_RESET_HOME_ON_TELEPORT;
	}
	if (!childGetValue("check visible").asBoolean())
	{
		flags &= ~REGION_FLAGS_EXTERNALLY_VISIBLE;
	}
	if (!childGetValue("check damage").asBoolean())
	{
		flags &= ~REGION_FLAGS_ALLOW_DAMAGE;
	}
	if (!childGetValue("block terraform").asBoolean())
	{
		flags &= ~REGION_FLAGS_BLOCK_TERRAFORM;
	}
	if (!childGetValue("block dwell").asBoolean())
	{
		flags &= ~REGION_FLAGS_BLOCK_DWELL;
	}
	if (!childGetValue("is sandbox").asBoolean())
	{
		flags &= ~REGION_FLAGS_SANDBOX;
	}
	return flags;
}

F32 LLPanelRegionTools::getBillableFactor() const
{
	return (F32)childGetValue("billable factor").asReal();
}

S32 LLPanelRegionTools::getPricePerMeter() const
{
	return childGetValue("land cost");
}

void LLPanelRegionTools::setSimName(const std::string& name)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		childSetVisible("region name", false);
	}
	else
	{
		childSetVisible("region name", true);
	}
//mk
	childSetValue("region name", name);
}

void LLPanelRegionTools::setEstateID(U32 id)
{
	childSetValue("estate", (S32)id);
}

void LLPanelRegionTools::setGridPosX(S32 pos)
{
	childSetValue("gridposx", pos);
}

void LLPanelRegionTools::setGridPosY(S32 pos)
{
	childSetValue("gridposy", pos);
}

void LLPanelRegionTools::setRedirectGridX(S32 pos)
{
	childSetValue("redirectx", pos);
}

void LLPanelRegionTools::setRedirectGridY(S32 pos)
{
	childSetValue("redirecty", pos);
}

void LLPanelRegionTools::setParentEstateID(U32 id)
{
	childSetValue("parentestate", (S32)id);
}

void LLPanelRegionTools::setCheckFlags(U64 flags)
{
	childSetValue("check prelude", is_prelude(flags));
	childSetValue("check fixed sun", (flags & REGION_FLAGS_SUN_FIXED) != 0);
	childSetValue("check reset home",
				  (flags & REGION_FLAGS_RESET_HOME_ON_TELEPORT) != 0);
	childSetValue("check damage", (flags & REGION_FLAGS_ALLOW_DAMAGE) != 0);
	childSetValue("check visible",
				  (flags & REGION_FLAGS_EXTERNALLY_VISIBLE) != 0);
	childSetValue("block terraform",
				  (flags & REGION_FLAGS_BLOCK_TERRAFORM) != 0);
	childSetValue("block dwell", (flags & REGION_FLAGS_BLOCK_DWELL) != 0);
	childSetValue("is sandbox", (flags & REGION_FLAGS_SANDBOX) != 0);
}

void LLPanelRegionTools::setBillableFactor(F32 billable_factor)
{
	childSetValue("billable factor", billable_factor);
}

void LLPanelRegionTools::setPricePerMeter(S32 price)
{
	childSetValue("land cost", price);
}

//static
void LLPanelRegionTools::onChangeAnything(LLUICtrl* ctrl, void* userdata)
{
	LLPanelRegionTools* self = (LLPanelRegionTools*)userdata;
	if (self && LLFloaterGodTools::findInstance() && gAgent.isGodlike())
	{
		self->childEnable("Apply");
	}
}

//static
void LLPanelRegionTools::onChangePrelude(LLUICtrl* ctrl, void* userdata)
{
	// checking prelude auto-checks fixed sun
	LLPanelRegionTools* self = (LLPanelRegionTools*)userdata;
	if (self && self->childGetValue("check prelude").asBoolean())
	{
		self->childSetValue("check fixed sun", true);
		self->childSetValue("check reset home", true);
	}
	// pass on to default onChange handler
	onChangeAnything(ctrl, userdata);
}

//static
void LLPanelRegionTools::onChangeSimName(LLLineEditor* caller, void* userdata)
{
	LLPanelRegionTools* self = (LLPanelRegionTools*)userdata;
	if (self && LLFloaterGodTools::findInstance() && gAgent.isGodlike())
	{
		self->childEnable("Apply");
	}
}

//static
void LLPanelRegionTools::onRefresh(void*)
{
	LLFloaterGodTools* fgt = LLFloaterGodTools::findInstance();
	if (fgt && gAgent.getRegion() && gAgent.isGodlike())
	{
		fgt->sendRegionInfoRequest();
	}
}

//static
void LLPanelRegionTools::onApplyChanges(void* userdata)
{
	LLPanelRegionTools* self = (LLPanelRegionTools*)userdata;
	LLFloaterGodTools* fgt = LLFloaterGodTools::findInstance();
	if (self && fgt && gAgent.getRegion() && gAgent.isGodlike())
	{
		self->childDisable("Apply");
		fgt->sendGodUpdateRegionInfo();
	}
}

//static
void LLPanelRegionTools::onBakeTerrain(void* userdata)
{
	LLPanelRequestTools::sendRequest("terrain", "bake",
									 gAgent.getRegionHost());
}

//static
void LLPanelRegionTools::onRevertTerrain(void* userdata)
{
	LLPanelRequestTools::sendRequest("terrain", "revert",
									 gAgent.getRegionHost());
}

//static
void LLPanelRegionTools::onSwapTerrain(void* userdata)
{
	LLPanelRequestTools::sendRequest("terrain", "swap",
									 gAgent.getRegionHost());
}

//static
void LLPanelRegionTools::onSelectRegion(void* userdata)
{
	LLViewerRegion* regionp =
		gWorld.getRegionFromPosGlobal(gAgent.getPositionGlobal());
	if (regionp)
	{
		LLVector3d origin = regionp->getOriginGlobal();
		LLVector3d north_east(REGION_WIDTH_METERS, REGION_WIDTH_METERS, 0);
		gViewerParcelMgr.selectLand(origin, origin + north_east, false);
	}
}

//*****************************************************************************
// Class LLPanelGridTools
//*****************************************************************************

//   || Grid   |_____________________________________
//   |                                               |
//   |                                               |
//   |  Sun Phase: >--------[]---------< [________]  |
//   |                                               |
//   |  ^         ^                                  |
//   |  LEFT      R1                                 |
//   |                                               |
//   |  [Kick all users]                             |
//   |                                               |
//   |                                               |
//   |                                               |
//   |                                               |
//   |                                               |
//   |_______________________________________________|
//      ^                                ^        ^
//      LEFT                             R2       RIGHT

LLPanelGridTools::LLPanelGridTools(const std::string& name)
:	LLPanel(name)
{
}

//virtual
bool LLPanelGridTools::postBuild()
{
	childSetAction("Kick all users", onClickKickAll, this);
	childSetAction("Flush This Region's Map Visibility Caches",
				   onClickFlushMapVisibilityCaches, this);
	return true;
}

//static
void LLPanelGridTools::onClickKickAll(void* userdata)
{
	gNotifications.add("KickAllUsers", LLSD(), LLSD(),
					   LLPanelGridTools::confirmKick);
}

bool LLPanelGridTools::confirmKick(const LLSD& notification,
								   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLSD payload;
		payload["kick_message"] = response["message"].asString();
		gNotifications.add("ConfirmKick", LLSD(), payload, finishKick);
	}
	return false;
}

//static
bool LLPanelGridTools::finishKick(const LLSD& notification,
								  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID, gAgentID);
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_AgentID, LL_UUID_ALL_AGENTS);
		msg->addU32("KickFlags", KICK_FLAGS_DEFAULT );
		msg->addStringFast(_PREHASH_Reason,
						   notification["payload"]["kick_message"].asString());
		gAgent.sendReliableMessage();
	}
	return false;
}

//static
void LLPanelGridTools::onClickFlushMapVisibilityCaches(void* data)
{
	gNotifications.add("FlushMapVisibilityCaches", LLSD(), LLSD(),
					   flushMapVisibilityCachesConfirm);
}

//static
bool LLPanelGridTools::flushMapVisibilityCachesConfirm(const LLSD& notification,
													   const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option != 0) return false;

	// HACK: Send this as an EstateOwnerRequest so it gets routed
	// correctly by the spaceserver. JC
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("EstateOwnerMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); // not used
	msg->nextBlock("MethodData");
	msg->addString("Method", "refreshmapvisibility");
	msg->addUUID("Invoice", LLUUID::null);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", gAgentID.asString());
	gAgent.sendReliableMessage();
	return false;
}

//*****************************************************************************
// LLPanelObjectTools
//*****************************************************************************

//   || Object |_______________________________________________________
//   |                                                                 |
//   |  Sim Name: Foo                                                  |
//   |  ^         ^                                                    |
//   |  LEFT      R1                                                   |
//   |                                                                 |
//   |  [X] Disable Scripts [X] Disable Collisions [X] Disable Physics |
//   |                                                  [ Apply  ]     |
//   |                                                                 |
//   |  [Set Target Avatar]	Avatar Name                                |
//   |  [Delete Target's Objects on Public Land	   ]                   |
//   |  [Delete All Target's Objects			   ]                   |
//   |  [Delete All Scripted Objects on Public Land]                   |
//   |  [Get Top Colliders ]                                           |
//   |  [Get Top Scripts   ]                                           |
//   |_________________________________________________________________|
//      ^                                         ^
//      LEFT                                      RIGHT

// Default constructor
LLPanelObjectTools::LLPanelObjectTools(const std::string& name)
: 	LLPanel(name),
	mTargetAvatar()
{
}

//virtual
bool LLPanelObjectTools::postBuild()
{
	childSetCommitCallback("disable scripts", onChangeAnything, this);
	childSetCommitCallback("disable collisions", onChangeAnything, this);
	childSetCommitCallback("disable physics", onChangeAnything, this);

	childSetAction("Apply", onApplyChanges, this);

	childSetAction("Set Target", onClickSet, this);

	childSetAction("Delete Target's Scripted Objects On Others Land",
				   onClickDeletePublicOwnedBy, this);
	childSetAction("Delete Target's Scripted Objects On *Any* Land",
				   onClickDeleteAllScriptedOwnedBy, this);
	childSetAction("Delete *ALL* Of Target's Objects",
				   onClickDeleteAllOwnedBy, this);

	childSetAction("Get Top Colliders", onGetTopColliders, this);
	childSetAction("Get Top Scripts", onGetTopScripts, this);
	childSetAction("Scripts digest", onGetScriptDigest, this);

	return true;
}

void LLPanelObjectTools::setTargetAvatar(const LLUUID& target_id)
{
	mTargetAvatar = target_id;
	if (target_id.isNull())
	{
		childSetValue("target_avatar_name", "(no target)");
	}
}

//virtual
void LLPanelObjectTools::refresh()
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		childSetVisible("region name", false);
	}
	else
	{
		childSetVisible("region name", true);
	}
//mk
	LLViewerRegion *regionp = gAgent.getRegion();
	if (regionp)
	{
		childSetText("region name", regionp->getName());
	}
}

U64 LLPanelObjectTools::computeRegionFlags(U64 flags) const
{
	if (childGetValue("disable scripts").asBoolean())
	{
		flags |= REGION_FLAGS_SKIP_SCRIPTS;
	}
	else
	{
		flags &= ~REGION_FLAGS_SKIP_SCRIPTS;
	}
	if (childGetValue("disable collisions").asBoolean())
	{
		flags |= REGION_FLAGS_SKIP_COLLISIONS;
	}
	else
	{
		flags &= ~REGION_FLAGS_SKIP_COLLISIONS;
	}
	if (childGetValue("disable physics").asBoolean())
	{
		flags |= REGION_FLAGS_SKIP_PHYSICS;
	}
	else
	{
		flags &= ~REGION_FLAGS_SKIP_PHYSICS;
	}
	return flags;
}

void LLPanelObjectTools::setCheckFlags(U64 flags)
{
	childSetValue("disable scripts", (flags & REGION_FLAGS_SKIP_SCRIPTS) != 0);
	childSetValue("disable collisions",
				  (flags & REGION_FLAGS_SKIP_COLLISIONS) != 0);
	childSetValue("disable physics", (flags & REGION_FLAGS_SKIP_PHYSICS) != 0);
}

void LLPanelObjectTools::clearAllWidgets()
{
	childSetValue("disable scripts", false);
	childDisable("disable scripts");

	childDisable("Apply");
	childDisable("Set Target");
	childDisable("Delete Target's Scripted Objects On Others Land");
	childDisable("Delete Target's Scripted Objects On *Any* Land");
	childDisable("Delete *ALL* Of Target's Objects");
}

void LLPanelObjectTools::enableAllWidgets()
{
	childEnable("disable scripts");

	childDisable("Apply");	// don't enable this one
	childEnable("Set Target");
	childEnable("Delete Target's Scripted Objects On Others Land");
	childEnable("Delete Target's Scripted Objects On *Any* Land");
	childEnable("Delete *ALL* Of Target's Objects");
	childEnable("Get Top Colliders");
	childEnable("Get Top Scripts");
}

//static
void LLPanelObjectTools::onGetTopColliders(void*)
{
	if (LLFloaterGodTools::findInstance() && gAgent.isGodlike())
	{
		LLFloaterTopObjects::showInstance();
		LLFloaterTopObjects::setMode(STAT_REPORT_TOP_COLLIDERS);
		LLFloaterTopObjects::sendRefreshRequest();
	}
}

//static
void LLPanelObjectTools::onGetTopScripts(void*)
{
	if (LLFloaterGodTools::findInstance() && gAgent.isGodlike())
	{
		LLFloaterTopObjects::showInstance();
		LLFloaterTopObjects::setMode(STAT_REPORT_TOP_SCRIPTS);
		LLFloaterTopObjects::sendRefreshRequest();
	}
}

//static
void LLPanelObjectTools::onGetScriptDigest(void*)
{
	if (LLFloaterGodTools::findInstance() && gAgent.isGodlike())
	{
		// get the list of scripts and number of occurences of each
		// (useful for finding self-replicating objects)
		LLPanelRequestTools::sendRequest("scriptdigest", "0",
										 gAgent.getRegionHost());
	}
}

void LLPanelObjectTools::onClickDeletePublicOwnedBy(void* userdata)
{
	// Bring up view-modal dialog
	LLPanelObjectTools* self = (LLPanelObjectTools*)userdata;
	if (self && self->mTargetAvatar.notNull())
	{
		self->mSimWideDeletesFlags = SWD_SCRIPTED_ONLY | SWD_OTHERS_LAND_ONLY;

		LLSD args;
		args["AVATAR_NAME"] = self->childGetValue("target_avatar_name").asString();
		LLSD payload;
		payload["avatar_id"] = self->mTargetAvatar;
		payload["flags"] = (S32)self->mSimWideDeletesFlags;

		gNotifications.add("GodDeleteAllScriptedPublicObjectsByUser", args,
						   payload, callbackSimWideDeletes);
	}
}

//static
void LLPanelObjectTools::onClickDeleteAllScriptedOwnedBy(void* userdata)
{
	// Bring up view-modal dialog
	LLPanelObjectTools* self = (LLPanelObjectTools*)userdata;
	if (self->mTargetAvatar.notNull())
	{
		self->mSimWideDeletesFlags = SWD_SCRIPTED_ONLY;

		LLSD args;
		args["AVATAR_NAME"] = self->childGetValue("target_avatar_name").asString();
		LLSD payload;
		payload["avatar_id"] = self->mTargetAvatar;
		payload["flags"] = (S32)self->mSimWideDeletesFlags;

		gNotifications.add("GodDeleteAllScriptedObjectsByUser", args, payload,
						   callbackSimWideDeletes);
	}
}

//static
void LLPanelObjectTools::onClickDeleteAllOwnedBy(void* userdata)
{
	// Bring up view-modal dialog
	LLPanelObjectTools* self = (LLPanelObjectTools*)userdata;
	if (self->mTargetAvatar.notNull())
	{
		self->mSimWideDeletesFlags = 0;

		LLSD args;
		args["AVATAR_NAME"] = self->childGetValue("target_avatar_name").asString();
		LLSD payload;
		payload["avatar_id"] = self->mTargetAvatar;
		payload["flags"] = (S32)self->mSimWideDeletesFlags;

		gNotifications.add("GodDeleteAllObjectsByUser", args, payload,
						   callbackSimWideDeletes);
	}
}

//static
bool LLPanelObjectTools::callbackSimWideDeletes(const LLSD& notification,
												const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		if (notification["payload"]["avatar_id"].asUUID().notNull())
		{
			send_sim_wide_deletes(notification["payload"]["avatar_id"].asUUID(),
								  notification["payload"]["flags"].asInteger());
		}
	}
	return false;
}

void LLPanelObjectTools::onClickSet(void* userdata)
{
	LLPanelObjectTools* self = (LLPanelObjectTools*)userdata;
	if (self)
	{
		LLFloaterAvatarPicker* pickerp =
			LLFloaterAvatarPicker::show(callbackAvatarID, userdata);
		if (pickerp && gFloaterViewp)
		{
			LLFloater* parentp = gFloaterViewp->getParentFloater(self);
			if (parentp)
			{
				parentp->addDependentFloater(pickerp);
			}
		}
	}
}

void LLPanelObjectTools::onClickSetBySelection(void* userdata)
{
	LLPanelObjectTools* self = (LLPanelObjectTools*)userdata;
	if (!self) return;

	LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode(NULL,
																	 true);
	if (!node) return;

	std::string owner_name;
	LLUUID owner_id;
	gSelectMgr.selectGetOwner(owner_id, owner_name);

	self->mTargetAvatar = owner_id;
	std::string name = "Object " + node->mName + " owned by " + owner_name;
	self->childSetValue("target_avatar_name", name);
}

//static
void LLPanelObjectTools::callbackAvatarID(const std::vector<std::string>& names,
										  const uuid_vec_t& ids,
										  void* userdata)
{
	LLPanelObjectTools* self = (LLPanelObjectTools*)userdata;
	if (self && !ids.empty() && !names.empty())
	{
		self->mTargetAvatar = ids[0];
		self->childSetValue("target_avatar_name", names[0]);
		self->refresh();
	}
}

//static
void LLPanelObjectTools::onChangeAnything(LLUICtrl*, void* userdata)
{
	LLPanelObjectTools* self = (LLPanelObjectTools*)userdata;
	if (self && LLFloaterGodTools::findInstance() && gAgent.isGodlike())
	{
		self->childEnable("Apply");
	}
}

//static
void LLPanelObjectTools::onApplyChanges(void* userdata)
{
	LLPanelObjectTools* self = (LLPanelObjectTools*)userdata;
	LLFloaterGodTools* fgt = LLFloaterGodTools::findInstance();
	if (self && fgt && gAgent.isGodlike() && gAgent.getRegion())
	{
		self->childDisable("Apply");
		fgt->sendGodUpdateRegionInfo();
	}
}

// --------------------
// LLPanelRequestTools
// --------------------

const std::string SELECTION = "Selection";
const std::string AGENT_REGION = "Agent Region";

LLPanelRequestTools::LLPanelRequestTools(const std::string& name)
:	LLPanel(name)
{
}

//virtual
bool LLPanelRequestTools::postBuild()
{
	childSetAction("Make Request", onClickRequest, this);
	refresh();

	return true;
}

//virtual
void LLPanelRequestTools::refresh()
{
	LLComboBox* combop = getChild<LLComboBox>("destination");
	std::string buffer = combop->getValue();

	combop->operateOnAll(LLComboBox::OP_DELETE);
	combop->addSimpleElement(SELECTION);
	combop->addSimpleElement(AGENT_REGION);
	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		std::string name = regionp->getName();
//MK
		if (gRLenabled && gRLInterface.mContainsShowloc)
		{
			name = "(Hidden)";
		}
//mk
		if (!name.empty())
		{
			combop->addSimpleElement(name);
		}
	}
	if (!buffer.empty())
	{
		combop->selectByValue(buffer);
	}
	else
	{
		combop->selectByValue(SELECTION);
	}
}

//static
void LLPanelRequestTools::sendRequest(const std::string& request,
									  const std::string& parameter,
									  const LLHost& host)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("GodlikeMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); // not used
	msg->nextBlock("MethodData");
	msg->addString("Method", request);
	msg->addUUID("Invoice", LLUUID::null);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", parameter);
	msg->sendReliable(host);
}

//static
void LLPanelRequestTools::onClickRequest(void* data)
{
	LLPanelRequestTools* self = (LLPanelRequestTools*)data;
	const std::string dest = self->childGetValue("destination").asString();
	if (dest == SELECTION)
	{
		std::string req = self->childGetValue("request");
		req = req.substr(0, req.find_first_of(" "));
		std::string param = self->childGetValue("parameter");
		gSelectMgr.sendGodlikeRequest(req, param);
	}
	else if (dest == AGENT_REGION)
	{
		self->sendRequest(gAgent.getRegionHost());
	}
	else
	{
		// find region by name
		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			if (regionp && dest == regionp->getName())
			{
				// found it
				self->sendRequest(regionp->getHost());
			}
		}
	}
}

void terrain_download_done(void** data, S32 status, LLExtStat ext_status)
{
	gNotifications.add("TerrainDownloaded");
}

void LLPanelRequestTools::sendRequest(const LLHost& host)
{
	// intercept viewer local actions here
	std::string req = childGetValue("request");
	if (req == "terrain download")
	{
		if (!gXferManagerp)
		{
			llwarns << "No transfer manager. Aborted." << llendl;
			return;
		}
		gXferManagerp->requestFile("terrain.raw", "terrain.raw", LL_PATH_NONE,
								  host, false, terrain_download_done, NULL);
	}
	else
	{
		req = req.substr(0, req.find_first_of(" "));
		sendRequest(req, childGetValue("parameter").asString(), host);
	}
}

// Flags are SWD_ flags.
void send_sim_wide_deletes(const LLUUID& owner_id, U32 flags)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_SimWideDeletes);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_DataBlock);
	msg->addUUIDFast(_PREHASH_TargetID, owner_id);
	msg->addU32Fast(_PREHASH_Flags, flags);
	gAgent.sendReliableMessage();
}
