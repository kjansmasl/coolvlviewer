/**
 * @file llfloaterlandholdings.cpp
 * @brief "My Land" floater showing all your land parcels.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llfloaterlandholdings.h"

#include "llparcel.h"
#include "llqueryflags.h"
#include "llscrolllistctrl.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloatergroupinfo.h"
#include "llfloaterworldmap.h"
#include "llproductinforequest.h"
#include "llstatusbar.h"
#include "llviewermessage.h"		// send_places_query()

// protected
LLFloaterLandHoldings::LLFloaterLandHoldings(const LLSD&)
:	mActualArea(0),
	mBillableArea(0),
	mIsDirty(true),
	mFirstPacketReceived(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_land_holdings.xml");
}

//virtual
bool LLFloaterLandHoldings::postBuild()
{
	// Parcels list
	mParcelsList = getChild<LLScrollListCtrl>("parcel list");
	mParcelsList->setCommitCallback(onSelectParcel);
	mParcelsList->setCallbackUserData(this);

	// Grant list
	mGrantList = getChild<LLScrollListCtrl>("grant list");
	mGrantList->setDoubleClickCallback(onGrantList);
	mGrantList->setCallbackUserData(this);

	childSetAction("Teleport", onClickTeleport, this);
	childSetAction("Show on Map", onClickMap, this);

	for (S32 i = 0, count = gAgent.mGroups.size(); i < count; ++i)
	{
		LLUUID id(gAgent.mGroups[i].mID);

		LLSD element;
		element["id"] = id;

		element["columns"][0]["column"] = "group";
		element["columns"][0]["value"] = gAgent.mGroups[i].mName;
		element["columns"][0]["font"] = "SANSSERIF";

		LLUIString areastr = getString("area_string");
		areastr.setArg("[AREA]", llformat("%d",
										  gAgent.mGroups[i].mContribution));
		element["columns"][1]["column"] = "area";
		element["columns"][1]["value"] = areastr.getString();
		element["columns"][1]["font"] = "SANSSERIF";

		mGrantList->addElement(element, ADD_SORTED);
	}

	// Look only for parcels we own:
	send_places_query(LLUUID::null, LLUUID::null, "", DFQ_AGENT_OWNED,
					  LLParcel::C_ANY, "");

	return true;
}

//virtual
void LLFloaterLandHoldings::draw()
{
	if (mIsDirty)
	{
		refresh();
	}
	LLFloater::draw();
}

//virtual
void LLFloaterLandHoldings::refresh()
{
	bool enable = mParcelsList->getFirstSelectedIndex() > -1;
	childSetEnabled("Teleport", enable);
	childSetEnabled("Show on Map", enable);

	S32 allowed_area = gStatusBarp->getSquareMetersCredit();
	S32 current_area = gStatusBarp->getSquareMetersCommitted();
	S32 available_area = gStatusBarp->getSquareMetersLeft();

	childSetTextArg("allowed_text", "[AREA]",
					llformat("%d", allowed_area));
	childSetTextArg("current_text", "[AREA]",
					llformat("%d", current_area));
	childSetTextArg("available_text", "[AREA]",
					llformat("%d", available_area));

	mIsDirty = false;
}

void LLFloaterLandHoldings::buttonCore(S32 which)
{
	S32 index = mParcelsList->getFirstSelectedIndex();
	if (index < 0) return;

	// 'hidden' is always last column
	std::string location =
		mParcelsList->getSelectedItemLabel(mParcelsList->getNumColumns() - 1);

	F32 global_x = 0.f;
	F32 global_y = 0.f;
	sscanf(location.c_str(), "%f %f", &global_x, &global_y);

	// Hack: Use the agent's z-height
	F64 global_z = gAgent.getPositionGlobal().mdV[VZ];

	LLVector3d pos_global(global_x, global_y, global_z);

	switch (which)
	{
		case 0:
			gAgent.teleportViaLocation(pos_global);
			if (gFloaterWorldMapp)
			{
				gFloaterWorldMapp->trackLocation(pos_global);
			}
			break;

		case 1:
			if (gFloaterWorldMapp)
			{
				gFloaterWorldMapp->trackLocation(pos_global);
				LLFloaterWorldMap::show(NULL, true);
			}
			break;

		default:
			break;
	}
}

//static
void LLFloaterLandHoldings::onSelectParcel(LLUICtrl*, void* data)
{
	LLFloaterLandHoldings* self = (LLFloaterLandHoldings*)data;
	if (self)
	{
		self->mIsDirty = true;
	}
}

// static
void LLFloaterLandHoldings::processPlacesReply(LLMessageSystem* msg, void**)
{
	LLFloaterLandHoldings* self = findInstance();
	if (!self || !msg) return;

	// If this is the first packet, clear out the "loading..." indicator
	if (!self->mFirstPacketReceived)
	{
		self->mFirstPacketReceived = true;
		self->mParcelsList->deleteAllItems();
	}

#if 0	// Not used
	LLUUID owner_id;
	std::string desc;
#endif
	std::string name, sim_name, land_sku, land_type;
	LLProductInfoRequestManager* mgrp =
		LLProductInfoRequestManager::getInstance();
	for (S32 i = 0, count = msg->getNumberOfBlocks(_PREHASH_QueryData);
		 i < count; ++i)
	{
#if 0	// Not used
		msg->getUUID(_PREHASH_QueryData, _PREHASH_OwnerID, owner_id, i);
		msg->getString(_PREHASH_QueryData, _PREHASH_Desc, desc, i);
		U8 flags;
		msg->getU8(_PREHASH_QueryData, _PREHASH_Flags, flags, i);
#endif
		msg->getString(_PREHASH_QueryData, _PREHASH_Name, name, i);

		S32 actual_area;
		msg->getS32(_PREHASH_QueryData, _PREHASH_ActualArea, actual_area, i);
		self->mActualArea += actual_area;

		S32 billable;
		msg->getS32(_PREHASH_QueryData, _PREHASH_BillableArea, billable, i);
		self->mBillableArea += billable;

		F32 global_x;
		msg->getF32(_PREHASH_QueryData, _PREHASH_GlobalX, global_x, i);
		F32 global_y;
		msg->getF32(_PREHASH_QueryData, _PREHASH_GlobalY, global_y, i);

		msg->getString(_PREHASH_QueryData, _PREHASH_SimName, sim_name, i);

		if (msg->getSizeFast(_PREHASH_QueryData, i, _PREHASH_ProductSKU) > 0)
		{
			msg->getStringFast(	_PREHASH_QueryData, _PREHASH_ProductSKU,
							   land_sku, i);
			LL_DEBUGS("Land SKU") << "Land sku: " << land_sku << LL_ENDL;
			land_type = mgrp->getDescriptionForSku(land_sku);
		}
		else
		{
			land_sku.clear();
			land_type = LLTrans::getString("unknown");
		}

		LLSD element;
		element["columns"][0]["column"] = "name";
		element["columns"][0]["value"] = name;
		element["columns"][0]["font"] = "SANSSERIF";

		S32 region_x = ll_roundp(global_x) % REGION_WIDTH_UNITS;
		S32 region_y = ll_roundp(global_y) % REGION_WIDTH_UNITS;
		element["columns"][1]["column"] = "location";
		element["columns"][1]["value"] =
			llformat("%s (%d, %d)", sim_name.c_str(), region_x, region_y);
		element["columns"][1]["font"] = "SANSSERIF";

		element["columns"][2]["column"] = "area";
		if (billable == actual_area)
		{
			element["columns"][2]["value"] = llformat("%d", billable);
		}
		else
		{
			element["columns"][2]["value"] = llformat("%d / %d", billable,
													  actual_area);
		}
		element["columns"][2]["font"] = "SANSSERIF";

		element["columns"][3]["column"] = "type";
		element["columns"][3]["value"] = land_type;
		element["columns"][3]["font"] = "SANSSERIF";

		// 'hidden' is always last column
		element["columns"][4]["column"] = "hidden";
		element["columns"][4]["value"] = llformat("%f %f", global_x, global_y);

		self->mParcelsList->addElement(element);
	}

	self->mIsDirty = true;
}

// static
void LLFloaterLandHoldings::onClickTeleport(void* data)
{
	LLFloaterLandHoldings* self = (LLFloaterLandHoldings*)data;
	if (self)
	{
		self->buttonCore(0);
		self->close();
	}
}

// static
void LLFloaterLandHoldings::onClickMap(void* data)
{
	LLFloaterLandHoldings* self = (LLFloaterLandHoldings*)data;
	if (self)
	{
		self->buttonCore(1);
	}
}

// static
void LLFloaterLandHoldings::onGrantList(void* data)
{
	LLFloaterLandHoldings* self = (LLFloaterLandHoldings*)data;
	if (self)
	{
		LLUUID group_id = self->mGrantList->getCurrentID();
		if (group_id.notNull())
		{
			LLFloaterGroupInfo::showFromUUID(group_id);
		}
	}
}
