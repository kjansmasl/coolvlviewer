/**
 * @file llfloatersellland.cpp
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llfloatersellland.h"

#include "llcachename.h"
#include "lllineeditor.h"
#include "llparcel.h"
#include "lluictrlfactory.h"

#include "llfloateravatarpicker.h"
#include "llfloater.h"
#include "llfloaterland.h"
#include "llselectmgr.h"
#include "lltexturectrl.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"

//static
LLParcelSelectionObserver* LLFloaterSellLand::sParcelObserver = NULL;

void LLFloaterSellLand::SelectionObserver::changed()
{
	LLFloaterSellLand* self = LLFloaterSellLand::findInstance();
	if (self)
	{
		if (gViewerParcelMgr.selectionEmpty())
		{
			self->close();
		}
		else
		{
			self->setParcel(gViewerParcelMgr.getSelectionRegion(),
							gViewerParcelMgr.getParcelSelection());
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Public (and static) interface method
///////////////////////////////////////////////////////////////////////////////

//static
void LLFloaterSellLand::sellLand(LLViewerRegion* region,
								 LLParcelSelectionHandle parcel)
{
	LLFloaterSellLand* self = getInstance();
	if (self && self->setParcel(region, parcel))
	{
		self->open();
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterSellLand proper
///////////////////////////////////////////////////////////////////////////////

LLFloaterSellLand::LLFloaterSellLand(const LLSD&)
:	mRegion(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_sell_land.xml",
												 NULL, false);
}

LLFloaterSellLand::~LLFloaterSellLand()
{
	if (sParcelObserver)
	{
		gViewerParcelMgr.removeSelectionObserver(sParcelObserver);
		delete sParcelObserver;
		sParcelObserver = NULL;
	}
	// Release the selection handle
	mParcelSelection = NULL;
}

//virtual
bool LLFloaterSellLand::postBuild()
{
	childSetCommitCallback("sell_to", onChangeValue, this);
	childSetCommitCallback("price", onChangeValue, this);
	childSetPrevalidate("price", LLLineEditor::prevalidateNonNegativeS32);
	childSetCommitCallback("sell_objects", onChangeValue, this);
	childSetAction("sell_to_select_agent", doSelectAgent, this);
	childSetAction("cancel_btn", doCancel, this);
	childSetAction("sell_btn", doSellLand, this);
	childSetAction("show_objects", doShowObjects, this);

	if (!sParcelObserver)
	{
		sParcelObserver = new SelectionObserver;
		gViewerParcelMgr.addSelectionObserver(sParcelObserver);
	}

	center();

	return true;
}

bool LLFloaterSellLand::setParcel(LLViewerRegion* region,
								  LLParcelSelectionHandle parcel)
{
	if (parcel && parcel->getParcel())
	{
		mRegion = region;
		mParcelSelection = parcel;
		mChoseSellTo = false;

		updateParcelInfo();
		refreshUI();

		return true;
	}
	else
	{
		return false;
	}
}

void LLFloaterSellLand::updateParcelInfo()
{
	LLParcel* parcelp = mParcelSelection->getParcel();
	if (!parcelp) return;

	mParcelActualArea = parcelp->getArea();
	mParcelIsForSale = parcelp->getForSale();
	if (mParcelIsForSale)
	{
		mChoseSellTo = true;
	}
	mParcelPrice = mParcelIsForSale ? parcelp->getSalePrice() : 0;
	mParcelSoldWithObjects = parcelp->getSellWithObjects();
	if (mParcelIsForSale)
	{
		childSetValue("price", mParcelPrice);
		if (mParcelSoldWithObjects)
		{
			childSetValue("sell_objects", "yes");
		}
		else
		{
			childSetValue("sell_objects", "no");
		}
	}
	else
	{
		childSetValue("price", "");
		childSetValue("sell_objects", "none");
	}

	mParcelSnapshot = parcelp->getSnapshotID();

	mAuthorizedBuyer = parcelp->getAuthorizedBuyerID();
	mSellToBuyer = mAuthorizedBuyer.notNull();

	if (mSellToBuyer && gCacheNamep)
	{
		std::string name;
		gCacheNamep->getFullName(mAuthorizedBuyer, name);
		childSetText("sell_to_agent", name);
	}
}

void LLFloaterSellLand::setBadge(const char* id, Badge badge)
{
	static std::string badgeOK("badge_ok.j2c");
	static std::string badgeNote("badge_note.j2c");
	static std::string badgeWarn("badge_warn.j2c");
	static std::string badgeError("badge_error.j2c");

	std::string badge_name;
	switch (badge)
	{
		default:
		case BADGE_OK:		badge_name = badgeOK;	break;
		case BADGE_NOTE:	badge_name = badgeNote;	break;
		case BADGE_WARN:	badge_name = badgeWarn;	break;
		case BADGE_ERROR:	badge_name = badgeError;
	}
	childSetValue(id, badge_name);
}

void LLFloaterSellLand::refreshUI()
{
	LLParcel* parcelp = mParcelSelection->getParcel();
	if (!parcelp) return;

	LLTextureCtrl* snapshot = getChild<LLTextureCtrl>("info_image");
	if (snapshot)
	{
		snapshot->setImageAssetID(mParcelSnapshot);
	}

	childSetText("info_parcel", parcelp->getName());
	childSetTextArg("info_size", "[AREA]", llformat("%d", mParcelActualArea));

	std::string price_str = childGetValue("price").asString();
	bool valid_price = !price_str.empty() &&
					   LLLineEditor::prevalidateNonNegativeS32(utf8str_to_wstring(price_str));

	if (valid_price && mParcelActualArea > 0)
	{
		F32 per_meter_price = 0;
		per_meter_price = F32(mParcelPrice) / F32(mParcelActualArea);
		childSetTextArg("price_per_m", "[PER_METER]",
						llformat("%0.2f", per_meter_price));
		childShow("price_per_m");

		setBadge("step_price", BADGE_OK);
	}
	else
	{
		childHide("price_per_m");

		if ("" == price_str)
		{
			setBadge("step_price", BADGE_NOTE);
		}
		else
		{
			setBadge("step_price", BADGE_ERROR);
		}
	}

	if (mSellToBuyer)
	{
		childSetValue("sell_to", "user");
		childShow("sell_to_agent");
		childShow("sell_to_select_agent");
	}
	else
	{
		if (mChoseSellTo)
		{
			childSetValue("sell_to", "anyone");
		}
		else
		{
			childSetValue("sell_to", "select");
		}
		childHide("sell_to_agent");
		childHide("sell_to_select_agent");
	}

	// Must select Sell To: Anybody, or User (with a specified username)
	std::string sell_to = childGetValue("sell_to").asString();
	bool valid_sell_to = "select" != sell_to &&
		("user" != sell_to || mAuthorizedBuyer.notNull());

	if (!valid_sell_to)
	{
		setBadge("step_sell_to", BADGE_NOTE);
	}
	else
	{
		setBadge("step_sell_to", BADGE_OK);
	}

	bool valid_sell_objects = childGetValue("sell_objects").asString() != "none";

	if (!valid_sell_objects)
	{
		setBadge("step_sell_objects", BADGE_NOTE);
	}
	else
	{
		setBadge("step_sell_objects", BADGE_OK);
	}

	if (valid_sell_to && valid_price && valid_sell_objects)
	{
		childEnable("sell_btn");
	}
	else
	{
		childDisable("sell_btn");
	}
}

//static
void LLFloaterSellLand::onChangeValue(LLUICtrl*, void* userdata)
{
	LLFloaterSellLand* self = (LLFloaterSellLand*)userdata;
	if (!self) return;

	std::string sell_to = self->childGetValue("sell_to").asString();

	if (sell_to == "user")
	{
		self->mChoseSellTo = true;
		self->mSellToBuyer = true;
		if (self->mAuthorizedBuyer.isNull())
		{
			doSelectAgent(self);
		}
	}
	else if (sell_to == "anyone")
	{
		self->mChoseSellTo = true;
		self->mSellToBuyer = false;
	}

	self->mParcelPrice = self->childGetValue("price");

	if ("yes" == self->childGetValue("sell_objects").asString())
	{
		self->mParcelSoldWithObjects = true;
	}
	else
	{
		self->mParcelSoldWithObjects = false;
	}

	self->refreshUI();
}

//static
void LLFloaterSellLand::doSelectAgent(void* userdata)
{
	LLFloaterSellLand* self = (LLFloaterSellLand*)userdata;
	if (!self) return;
	// grandparent is a floater, in order to set up dependency
	self->addDependentFloater(LLFloaterAvatarPicker::show(callbackAvatarPick,
														  self, false, true));
}

//static
void LLFloaterSellLand::callbackAvatarPick(const std::vector<std::string>& names,
										   const std::vector<LLUUID>& ids,
										   void* data)
{
	LLFloaterSellLand* self = (LLFloaterSellLand*)data;
	if (!self || names.empty() || ids.empty()) return;

	LLParcel* parcel = self->mParcelSelection->getParcel();
	if (parcel)
	{
		const LLUUID& id = ids[0];
		parcel->setAuthorizedBuyerID(id);
		self->mAuthorizedBuyer = id;

		self->childSetText("sell_to_agent", names[0]);

		self->refreshUI();
	}
}

//static
void LLFloaterSellLand::doCancel(void* userdata)
{
	LLFloaterSellLand* self = (LLFloaterSellLand*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterSellLand::doShowObjects(void* userdata)
{
	LLFloaterSellLand* self = (LLFloaterSellLand*)userdata;
	if (!self) return;

	LLParcel* parcel = self->mParcelSelection->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), RT_SELL);

	gNotifications.add("TransferObjectsHighlighted", LLSD(), LLSD(),
					   callbackHighlightTransferable);
}

//static
bool LLFloaterSellLand::callbackHighlightTransferable(const LLSD& notification,
													  const LLSD& data)
{
	gSelectMgr.unhighlightAll();
	return false;
}

//static
void LLFloaterSellLand::doSellLand(void* userdata)
{
	LLFloaterSellLand* self = (LLFloaterSellLand*)userdata;
	if (!self) return;

	LLParcel* parcel = self->mParcelSelection->getParcel();
	if (!parcel) return;

	// Do a confirmation
	S32 sale_price = self->childGetValue("price");
	S32 area = parcel->getArea();
	std::string authorizedBuyerName = "Anyone";
	bool sell_to_anyone = true;
	if ("user" == self->childGetValue("sell_to").asString())
	{
		authorizedBuyerName = self->childGetText("sell_to_agent");
		sell_to_anyone = false;
	}

	// must sell to someone if indicating sale to anyone
	if (!parcel->getForSale() && sale_price == 0 && sell_to_anyone)
	{
		gNotifications.add("SalePriceRestriction");
		return;
	}

	LLSD args;
	args["LAND_SIZE"] = llformat("%d", area);
	args["SALE_PRICE"] = llformat("%d", sale_price);
	args["NAME"] = authorizedBuyerName;

	LLNotification::Params params("ConfirmLandSaleChange");
	params.substitutions(args).functor(boost::bind(&LLFloaterSellLand::onConfirmSale,
												   self, _1, _2));

	if (sell_to_anyone)
	{
		params.name("ConfirmLandSaleToAnyoneChange");
	}

	if (parcel->getForSale())
	{
		// parcel already for sale, so ignore this question
		gNotifications.forceResponse(params, -1);
	}
	else
	{
		// ask away
		gNotifications.add(params);
	}
}

bool LLFloaterSellLand::onConfirmSale(const LLSD& notification,
									  const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option != 0)
	{
		return false;
	}
	S32  sale_price	= childGetValue("price");

	// Valid extracted data
	if (sale_price < 0)
	{
		// TomY TODO: Throw an error
		return false;
	}

	LLParcel* parcel = mParcelSelection->getParcel();
	if (!parcel) return false;

#if 0	// can_agent_modify_parcel deprecated by GROUPS
 	if (!can_agent_modify_parcel(parcel))
 	{
 		close();
 		return;
 	}
#endif

	parcel->setParcelFlag(PF_FOR_SALE, true);
	parcel->setSalePrice(sale_price);
	bool sell_with_objects = false;
	if ("yes" == childGetValue("sell_objects").asString())
	{
		sell_with_objects = true;
	}
	parcel->setSellWithObjects(sell_with_objects);
	if ("user" == childGetValue("sell_to").asString())
	{
		parcel->setAuthorizedBuyerID(mAuthorizedBuyer);
	}
	else
	{
		parcel->setAuthorizedBuyerID(LLUUID::null);
	}

	// Send update to server
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);

	close();
	return false;
}
