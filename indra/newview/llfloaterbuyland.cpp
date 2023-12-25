/**
 * @file llfloaterbuyland.cpp
 * @brief LLFloaterBuyLand class implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloaterbuyland.h"

#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llconfirmationmanager.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llxmlrpctransaction.h"

#include "llagent.h"
#include "llcurrencyuimanager.h"
#include "llgridmanager.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llstatusbar.h"
#include "lltexturectrl.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llweb.h"
#include "roles_constants.h"

#if LL_WINDOWS
// Passing 'this' during construction generates a warning. The callee only uses
// the pointer to hold a reference to 'this' which is already valid, so this
// call does the correct thing. Disable the warning so that we can compile
// without generating a warning.
#pragma warning(disable : 4355)
#endif

constexpr F32 GROUP_LAND_BONUS_FACTOR = 1.1f;

//static
LLParcelSelectionObserver* LLFloaterBuyLand::sParcelObserver = NULL;

///////////////////////////////////////////////////////////////////////////////
// Observer methods
///////////////////////////////////////////////////////////////////////////////

void LLFloaterBuyLand::SelectionObserver::changed()
{
	LLFloaterBuyLand* self = LLFloaterBuyLand::findInstance();
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
// Public (and static) interface methods
///////////////////////////////////////////////////////////////////////////////

//static
void LLFloaterBuyLand::buyLand(LLViewerRegion* region,
							   LLParcelSelectionHandle parcel,
							   bool is_for_group)
{
	if (is_for_group && !gAgent.hasPowerInActiveGroup(GP_LAND_DEED))
	{
		gNotifications.add("OnlyOfficerCanBuyLand");
		return;
	}

	LLFloaterBuyLand* self = getInstance();
	if (self)
	{
		self->setForGroup(is_for_group);
		self->setParcel(region, parcel);
		self->open();
	}
}

//static
void LLFloaterBuyLand::updateCovenantText(const std::string& string,
										  const LLUUID& asset_id)
{
	LLFloaterBuyLand* self = findInstance();
	if (self)
	{
		self->setCovenantText(string, asset_id);
	}
}

//static
void LLFloaterBuyLand::updateEstateName(const std::string& name)
{
	LLFloaterBuyLand* self = findInstance();
	if (self)
	{
		self->setEstateName(name);
	}
}

//static
void LLFloaterBuyLand::updateLastModified(const std::string& text)
{
	LLFloaterBuyLand* self = findInstance();
	if (self)
	{
		self->setLastModified(text);
	}
}

//static
void LLFloaterBuyLand::updateEstateOwnerName(const std::string& name)
{
	LLFloaterBuyLand* self = findInstance();
	if (self)
	{
		self->setEstateOwnerName(name);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterBuyLand proper
///////////////////////////////////////////////////////////////////////////////

LLFloaterBuyLand::LLFloaterBuyLand(const LLSD&)
:	mParcelSelection(NULL),
	mParcelBuyInfo(NULL),
	mTransaction(NULL),
	mCurrency(*this),
	mBought(false),
	mParcelValid(false),
	mSiteValid(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_buy_land.xml",
												 NULL, false);
}

//virtual
LLFloaterBuyLand::~LLFloaterBuyLand()
{
	if (sParcelObserver)
	{
		gViewerParcelMgr.removeSelectionObserver(sParcelObserver);
		delete sParcelObserver;
		sParcelObserver = NULL;
	}
	if (mTransaction)
	{
		delete mTransaction;
		mTransaction = NULL;
	}
	if (mParcelBuyInfo)
	{
		gViewerParcelMgr.deleteParcelBuy(mParcelBuyInfo);
		mParcelBuyInfo = NULL;
	}
	// Release the selection handle
	mParcelSelection = NULL;
}

//virtual
bool LLFloaterBuyLand::postBuild()
{
	mCurrency.prepare();

	childSetAction("buy_btn", onClickBuy, this);
	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("error_web", onClickErrorWeb, this);

	if (!sParcelObserver)
	{
		sParcelObserver = new SelectionObserver;
		gViewerParcelMgr.addSelectionObserver(sParcelObserver);
	}

	center();

	return true;
}

void LLFloaterBuyLand::updateAgentInfo()
{
	mAgentCommittedTier = gStatusBarp->getSquareMetersCommitted();
	mAgentCashBalance = gStatusBarp->getBalance();

	// *TODO: This is an approximation, we should send this value down to the
	// viewer. See SL-10728 for details.
	mAgentHasNeverOwnedLand = mAgentCommittedTier == 0;
}

void LLFloaterBuyLand::updateParcelInfo()
{
	LLParcel* parcel = mParcelSelection->getParcel();
	if (!parcel) return;	// Paranoia

	mParcelValid = parcel && mRegion;
	mParcelIsForSale = false;
	mParcelIsGroupLand = false;
	mParcelGroupContribution = 0;
	mParcelPrice = 0;
	mParcelActualArea = 0;
	mParcelBillableArea = 0;
	mParcelSupportedObjects = 0;
	mParcelSoldWithObjects = false;
	mParcelLocation.clear();
	mParcelSnapshot.setNull();
	mParcelSellerName.clear();

	mCanBuy = false;
	mCannotBuyIsError = false;

	if (!mParcelValid)
	{
		mCannotBuyReason = getString("no_land_selected");
		return;
	}

	if (mParcelSelection->getMultipleOwners())
	{
		mCannotBuyReason = getString("multiple_parcels_selected");
		return;
	}

	const LLUUID& parcel_owner = parcel->getOwnerID();

	mIsClaim = parcel->isPublic();
	if (!mIsClaim)
	{
		mParcelActualArea = parcel->getArea();
		mParcelIsForSale = parcel->getForSale();
		mParcelIsGroupLand = parcel->getIsGroupOwned();
		mParcelPrice = mParcelIsForSale ? parcel->getSalePrice() : 0;

		if (mParcelIsGroupLand)
		{
			LLUUID group_id = parcel->getGroupID();
			mParcelGroupContribution = gAgent.getGroupContribution(group_id);
		}
	}
	else
	{
		mParcelActualArea = mParcelSelection->getClaimableArea();
		mParcelIsForSale = true;
		mParcelPrice = mParcelActualArea * parcel->getClaimPricePerMeter();
	}

	mParcelBillableArea = ll_round(mRegion->getBillableFactor() *
								   mParcelActualArea);

 	mParcelSupportedObjects = ll_round(parcel->getMaxPrimCapacity() *
									   parcel->getParcelPrimBonus());
 	// Can't have more than region max tasks, regardless of parcel object bonus
	// factor.
 	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
 	if (region)
 	{
		S32 max_tasks_per_region = (S32)region->getMaxTasks();
		mParcelSupportedObjects = llmin(mParcelSupportedObjects,
										max_tasks_per_region);
 	}

	mParcelSoldWithObjects = parcel->getSellWithObjects();

	LLVector3 center = parcel->getCenterpoint();
	mParcelLocation = llformat("%s %d,%d", mRegion->getName().c_str(),
							   (S32)center[VX], (S32)center[VY]);

	mParcelSnapshot = parcel->getSnapshotID();

	updateNames();

	bool got_the_cash = mParcelPrice <= mAgentCashBalance;
	S32 missing_cash = got_the_cash ? 0 : mParcelPrice - mAgentCashBalance;
	mCurrency.setAmount(missing_cash, true);
	mCurrency.setZeroMessage(got_the_cash ? getString("none_needed")
										  : LLStringUtil::null);

	// checks that we can buy the land

	if (mIsForGroup && !gAgent.hasPowerInActiveGroup(GP_LAND_DEED))
	{
		mCannotBuyReason = getString("cant_buy_for_group");
		return;
	}

	if (!mIsClaim)
	{
		const LLUUID& authorized_buyer = parcel->getAuthorizedBuyerID();
		const LLUUID buyer = gAgentID;
		const LLUUID new_owner = mIsForGroup ? gAgent.getGroupID() : buyer;

		if (!mParcelIsForSale ||
			(mParcelPrice == 0 && authorized_buyer.isNull()))
		{

			mCannotBuyReason = getString("parcel_not_for_sale");
			return;
		}

		if (parcel_owner == new_owner)
		{
			if (mIsForGroup)
			{
				mCannotBuyReason = getString("group_already_owns");
			}
			else
			{
				mCannotBuyReason = getString("you_already_own");
			}
			return;
		}

		if (authorized_buyer.notNull() && buyer != authorized_buyer)
		{
			mCannotBuyReason = getString("set_to_sell_to_other");
			return;
		}
	}
	else
	{
		if (mParcelActualArea == 0)
		{
			mCannotBuyReason = getString("no_public_land");
			return;
		}

		if (mParcelSelection->hasOthersSelected())
		{
			// Policy: Must not have someone else's land selected
			mCannotBuyReason = getString("not_owned_by_you");
			return;
		}
	}

	mCanBuy = true;
}

void LLFloaterBuyLand::updateCovenantInfo()
{
	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	if (!region) return;

	LLTextBox* region_name = getChild<LLTextBox>("region_name_text");
	region_name->setText(region->getName());

	LLTextBox* region_type = getChild<LLTextBox>("region_type_text");
	region_type->setText(region->getSimProductName());

	LLTextBox* resellable_txt = getChild<LLTextBox>("resellable_clause");
	if (region->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL))
	{
		resellable_txt->setText(getString("can_not_resell"));
	}
	else
	{
		resellable_txt->setText(getString("can_resell"));
	}

	LLTextBox* changeable_txt = getChild<LLTextBox>("changeable_clause");
	if (region->getRegionFlag(REGION_FLAGS_ALLOW_PARCEL_CHANGES))
	{
		changeable_txt->setText(getString("can_change"));
	}
	else
	{
		changeable_txt->setText(getString("can_not_change"));
	}

	LLCheckBoxCtrl* check = getChild<LLCheckBoxCtrl>("agree_covenant");
	check->set(false);
	check->setEnabled(true);
	check->setCommitCallback(onChangeAgreeCovenant);
	check->setCallbackUserData(this);

	childHide("covenant_text");

	// Send EstateCovenantInfo message
	region->sendEstateCovenantRequest();
}

void LLFloaterBuyLand::setCovenantText(const std::string& string,
									   const LLUUID& asset_id)
{
	LLViewerTextEditor* editor =
		getChild<LLViewerTextEditor>("covenant_editor");
	editor->setHandleEditKeysDirectly(false);
	editor->setText(string);

	LLCheckBoxCtrl* check = getChild<LLCheckBoxCtrl>("agree_covenant");
	if (asset_id.isNull())
	{
		check->set(true);
		check->setEnabled(false);
		refreshUI();

		// Remove the line stating that you must agree
		childHide("covenant_text");
	}
	else
	{
		check->setEnabled(true);
		// Show the line stating that you must agree
		childShow("covenant_text");
	}
}

void LLFloaterBuyLand::setEstateName(const std::string& name)
{
	LLTextBox* box = getChild<LLTextBox>("estate_name_text");
	if (box) box->setText(name);
}

void LLFloaterBuyLand::setLastModified(const std::string& text)
{
	LLTextBox* editor = getChild<LLTextBox>("covenant_timestamp_text");
	editor->setText(text);
}

void LLFloaterBuyLand::setEstateOwnerName(const std::string& name)
{
	LLTextBox* box = getChild<LLTextBox>("estate_owner_text");
	box->setText(name);
}

void LLFloaterBuyLand::updateWebSiteInfo()
{
	S32 askBillableArea = mIsForGroup ? 0 : mParcelBillableArea;
	S32 askCurrencyBuy = mCurrency.getAmount();

	if (mTransaction && mTransactionType == TransactionPreflight &&
		mPreflightAskBillableArea == askBillableArea &&
		mPreflightAskCurrencyBuy == askCurrencyBuy)
	{
		return;
	}

	mPreflightAskBillableArea = askBillableArea;
	mPreflightAskCurrencyBuy = askCurrencyBuy;

#if 0 // enable this code if you want the details to blank while we're talking
	  // to the web site... it's kind of jarring.
	mSiteValid = false;
	mSiteMembershipUpgrade = false;
	mSiteMembershipAction = "(waiting)";
	mSiteMembershipPlanIDs.clear();
	mSiteMembershipPlanNames.clear();
	mSiteLandUseUpgrade = false;
	mSiteLandUseAction = "(waiting)";
	mSiteCurrencyEstimated = false;
	mSiteCurrencyEstimatedCost = 0;
#endif

	LLXMLRPCValue keywordArgs = LLXMLRPCValue::createStruct();
	keywordArgs.appendString("agentId", gAgentID.asString());
	keywordArgs.appendString(
		"secureSessionId",
		gAgent.getSecureSessionID().asString());
	keywordArgs.appendInt("billableArea", mPreflightAskBillableArea);
	keywordArgs.appendInt("currencyBuy", mPreflightAskCurrencyBuy);

	LLXMLRPCValue params = LLXMLRPCValue::createArray();
	params.append(keywordArgs);

	startTransaction(TransactionPreflight, params);
}

void LLFloaterBuyLand::finishWebSiteInfo()
{

	LLXMLRPCValue result = mTransaction->responseValue();

	mSiteValid = result["success"].asBool();
	if (!mSiteValid)
	{
		tellUserError(result["errorMessage"].asString(),
					 result["errorURI"].asString());
		return;
	}

	LLXMLRPCValue membership = result["membership"];
	mSiteMembershipUpgrade = membership["upgrade"].asBool();
	mSiteMembershipAction = membership["action"].asString();
	mSiteMembershipPlanIDs.clear();
	mSiteMembershipPlanNames.clear();
	LLXMLRPCValue levels = membership["levels"];
	for (LLXMLRPCValue level = levels.rewind(); level.isValid();
		 level = levels.next())
	{
		mSiteMembershipPlanIDs.emplace_back(level["id"].asString());
		mSiteMembershipPlanNames.emplace_back(level["description"].asString());
	}
	mUserPlanChoice = 0;

	LLXMLRPCValue landUse = result["landUse"];
	mSiteLandUseUpgrade = landUse["upgrade"].asBool();
	mSiteLandUseAction = landUse["action"].asString();

	LLXMLRPCValue currency = result["currency"];
	mCurrency.setEstimate(currency["estimatedCost"].asInt());

	mSiteConfirm = result["confirm"].asString();
}

void LLFloaterBuyLand::runWebSitePrep(const std::string& password)
{
	if (!mCanBuy)
	{
		return;
	}

	bool remove_contribution = childGetValue("remove_contribution").asBoolean();
	mParcelBuyInfo = gViewerParcelMgr.setupParcelBuy(gAgentID, gAgentSessionID,
													 gAgent.getGroupID(),
													 mIsForGroup, mIsClaim,
													 remove_contribution);

	if (mParcelBuyInfo && !mSiteMembershipUpgrade && !mSiteLandUseUpgrade &&
		mCurrency.getAmount() == 0 && mSiteConfirm != "password")
	{
		sendBuyLand();
		return;
	}

	std::string newLevel = "noChange";

	if (mSiteMembershipUpgrade)
	{
		LLComboBox* levels = getChild<LLComboBox>( "account_level");
		if (levels)
		{
			mUserPlanChoice = levels->getCurrentIndex();
			newLevel = mSiteMembershipPlanIDs[mUserPlanChoice];
		}
	}

	LLXMLRPCValue keywordArgs = LLXMLRPCValue::createStruct();
	keywordArgs.appendString("agentId", gAgentID.asString());
	keywordArgs.appendString("secureSessionId",
							 gAgent.getSecureSessionID().asString());
	keywordArgs.appendString("levelId", newLevel);
	keywordArgs.appendInt("billableArea",
						  mIsForGroup ? 0 : mParcelBillableArea);
	keywordArgs.appendInt("currencyBuy", mCurrency.getAmount());
	keywordArgs.appendInt("estimatedCost", mCurrency.getEstimate());
	keywordArgs.appendString("confirm", mSiteConfirm);
	if (!password.empty())
	{
		keywordArgs.appendString("password", password);
	}

	LLXMLRPCValue params = LLXMLRPCValue::createArray();
	params.append(keywordArgs);

	startTransaction(TransactionBuy, params);
}

void LLFloaterBuyLand::finishWebSitePrep()
{
	LLXMLRPCValue result = mTransaction->responseValue();

	bool success = result["success"].asBool();
	if (!success)
	{
		tellUserError(result["errorMessage"].asString(),
					  result["errorURI"].asString());
		return;
	}

	sendBuyLand();
}

void LLFloaterBuyLand::sendBuyLand()
{
	if (mParcelBuyInfo)
	{
		gViewerParcelMgr.sendParcelBuy(mParcelBuyInfo);
		gViewerParcelMgr.deleteParcelBuy(mParcelBuyInfo);
		mBought = true;
	}
}

//static
void LLFloaterBuyLand::callbackCacheName(const LLUUID&, const std::string&,
										 bool)
{
	LLFloaterBuyLand* self = findInstance();
	if (self)
	{
		self->updateNames();
	}
}

void LLFloaterBuyLand::updateNames()
{
	LLParcel* parcelp = mParcelSelection->getParcel();
	if (!parcelp)
	{
		mParcelSellerName.clear();
		return;
	}

	if (mIsClaim)
	{
		mParcelSellerName = "Linden Lab";
	}
	else if (parcelp->getIsGroupOwned())
	{
		const LLUUID& group_id = parcelp->getGroupID();
		if (gCacheNamep &&
			!gCacheNamep->getGroupName(group_id, mParcelSellerName))
		{
			gCacheNamep->get(group_id, true, callbackCacheName);
		}
	}
	else
	{
		const LLUUID& owner_id = parcelp->getOwnerID();
		if (gCacheNamep &&
			!gCacheNamep->getFullName(owner_id, mParcelSellerName))
		{
			gCacheNamep->get(owner_id, false, callbackCacheName);
		}
	}
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		mParcelSellerName = gRLInterface.getDummyName(mParcelSellerName);
	}
//mk
}

void LLFloaterBuyLand::startTransaction(TransactionType type,
										LLXMLRPCValue params)
{
	if (mTransaction)
	{
		delete mTransaction;
		mTransaction = NULL;
	}

	mTransactionType = type;

	// Select a URI and method appropriate for the transaction type.
	static std::string transaction_uri;
	if (transaction_uri.empty())
	{
		transaction_uri = LLGridManager::getInstance()->getHelperURI() +
						  "landtool.php";
	}

	const char* method;
	switch (mTransactionType)
	{
		case TransactionPreflight:
			method = "preflightBuyLandPrep";
			break;

		case TransactionBuy:
			method = "buyLandPrep";
			break;

		default:
			llwarns << "Unknown transaction type !" << llendl;
			return;
	}

	mTransaction = new LLXMLRPCTransaction(transaction_uri, method, params,
										   // Do not use gzip
										   false);
}

bool LLFloaterBuyLand::checkTransaction()
{
	if (!mTransaction)
	{
		return false;
	}

	if (!mTransaction->process())
	{
		return false;
	}

	if (mTransaction->status(NULL) != LLXMLRPCTransaction::StatusComplete)
	{
		tellUserError(mTransaction->statusMessage(),
					  mTransaction->statusURI());
	}
	else
	{
		switch (mTransactionType)
		{
			case TransactionPreflight:
				finishWebSiteInfo();
				break;

			case TransactionBuy:
				finishWebSitePrep();
				break;

			default:
				break;
		}
	}

	delete mTransaction;
	mTransaction = NULL;

	return true;
}

void LLFloaterBuyLand::tellUserError(const std::string& message,
									 const std::string& uri)
{
	mCanBuy = false;
	mCannotBuyIsError = true;
	mCannotBuyReason = getString("fetching_error");
	mCannotBuyReason += message;
	mCannotBuyURI = uri;
}

void LLFloaterBuyLand::setParcel(LLViewerRegion* region,
								 LLParcelSelectionHandle parcel)
{
	if (mTransaction &&  mTransactionType == TransactionBuy)
	{
		// The user is buying, do not change the selection
		return;
	}

	mRegion = region;
	mParcelSelection = parcel;

	updateAgentInfo();
	updateParcelInfo();
	updateCovenantInfo();
	if (mCanBuy)
	{
		updateWebSiteInfo();
	}
	refreshUI();
}

void LLFloaterBuyLand::setForGroup(bool forGroup)
{
	mIsForGroup = forGroup;
}

//virtual
void LLFloaterBuyLand::draw()
{
	bool dirty = checkTransaction();
	dirty |= mCurrency.process();

	if (mBought)
	{
		close();
	}
	else if (dirty)
	{
		if (mCanBuy && mCurrency.hasError())
		{
			tellUserError(mCurrency.errorMessage(), mCurrency.errorURI());
		}

		refreshUI();
	}

	LLFloater::draw();
}

//virtual
bool LLFloaterBuyLand::canClose()
{
	bool can_close = !mTransaction &&
					 (mCurrency.canCancel() ||
					  mTransactionType != TransactionBuy);
	if (!can_close)
	{
		// Explain to the user why they cannot do this, see DEV-9605
		gNotifications.add("CannotCloseFloaterBuyLand");
	}
	return can_close;
}

//virtual
void LLFloaterBuyLand::setMinimized(bool minimize)
{
	bool restored = isMinimized() && !minimize;
	LLFloater::setMinimized(minimize);
	if (restored)
	{
		refreshUI();
	}
}

void LLFloaterBuyLand::refreshUI()
{
	std::string message;

	// Section zero: title area
	{
		LLTextureCtrl* snapshot = getChild<LLTextureCtrl>("info_image");
		if (mParcelValid)
		{
			snapshot->setImageAssetID(mParcelSnapshot);

			childSetText("info_parcel", mParcelLocation);

			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", mParcelActualArea);
			string_args["[AMOUNT2]"] = llformat("%d", mParcelSupportedObjects);

			childSetText("info_size",
						 getString("meters_supports_object", string_args));

			F32 cost_per_sqm = 0.0f;
			if (mParcelActualArea > 0)
			{
				cost_per_sqm = (F32)mParcelPrice / (F32)mParcelActualArea;
			}

			LLStringUtil::format_map_t info_price_args;
			info_price_args["[PRICE]"] = llformat("%d", mParcelPrice);
			info_price_args["[PRICE_PER_SQM]"] = llformat("%.1f", cost_per_sqm);
			if (mParcelSoldWithObjects)
			{
				info_price_args["[SOLD_WITH_OBJECTS]"] =
					getString("sold_with_objects");
			}
			else
			{
				info_price_args["[SOLD_WITH_OBJECTS]"] =
					getString("sold_without_objects");
			}
			childSetText("info_price",
						 getString("info_price_string", info_price_args));
			childSetVisible("info_price", mParcelIsForSale);
		}
		else
		{
			snapshot->setImageAssetID(LLUUID::null);
			childSetText("info_parcel", getString("no_parcel_selected"));
			childSetText("info_size", LLStringUtil::null);
			childSetText("info_price", LLStringUtil::null);
		}

		if (mCanBuy)
		{
			// "Buying land for group:" or "Buying this land will:"
			message = mIsForGroup ? getString("buying_for_group")
								  : getString("buying_will");
		}
		else
		{
			// "Cannot buy now:" or "Not for sale:"
			message = mCannotBuyIsError ? getString("cannot_buy_now")
										: getString("not_for_sale");
		}
		childSetText("info_action", message);
	}

	bool showing_error = !mCanBuy || !mSiteValid;

	// Error section
	if (showing_error)
	{
		childSetBadge("step_error",
					  mCannotBuyIsError ? BADGE_ERROR : BADGE_WARN);

		LLTextBox* msgbox = getChild<LLTextBox>("error_message");
		if (msgbox)
		{
			msgbox->setVisible(true);
			msgbox->setWrappedText(!mCanBuy ? mCannotBuyReason
											: "(waiting for data)");
		}

		childSetVisible("error_web",
						mCannotBuyIsError && !mCannotBuyURI.empty());
	}
	else
	{
		childHide("step_error");
		childHide("error_message");
		childHide("error_web");
	}

	// Section one: account
	if (!showing_error)
	{
		childSetBadge("step_1",
					  mSiteMembershipUpgrade ? BADGE_NOTE : BADGE_OK);
		childSetText("account_action", mSiteMembershipAction);
		childSetText("account_reason",
					 mSiteMembershipUpgrade ? getString("must_upgrade")
											: getString("cant_own_land"));

		LLComboBox* levels = getChild<LLComboBox>( "account_level");
		if (levels)
		{
			levels->setVisible(mSiteMembershipUpgrade);

			levels->removeall();
			for (std::vector<std::string>::const_iterator
					i = mSiteMembershipPlanNames.begin(),
					end = mSiteMembershipPlanNames.end();
				 i != end; ++i)
			{
				levels->add(*i);
			}

			levels->setCurrentByIndex(mUserPlanChoice);
		}

		childShow("step_1");
		childShow("account_action");
		childShow("account_reason");
	}
	else
	{
		childHide("step_1");
		childHide("account_action");
		childHide("account_reason");
		childHide("account_level");
	}

	// Section two: land use fees
	if (!showing_error)
	{
		childSetBadge("step_2", mSiteLandUseUpgrade ? BADGE_NOTE : BADGE_OK);
		childSetText("land_use_action", mSiteLandUseAction);

		if (mIsForGroup)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[GROUP]"] = std::string(gAgent.mGroupName);

			message = getString("insufficient_land_credits", string_args);

		}
		else
		{
			LLStringUtil::format_map_t string_args;
			string_args["[BUYER]"] = llformat("%d", mAgentCommittedTier);
			message = getString("land_holdings", string_args);
		}

		if (!mParcelValid)
		{
			message += "(no parcel selected)";
		}
		else if (mParcelBillableArea == mParcelActualArea)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d ", mParcelActualArea);
			message += getString("parcel_meters", string_args);
		}
		else if (mParcelBillableArea > mParcelActualArea)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d ", mParcelBillableArea);
			message += getString("premium_land", string_args);
		}
		else
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d ", mParcelBillableArea);
			message += getString("discounted_land", string_args);
		}

		childSetWrappedText("land_use_reason", message);

		childShow("step_2");
		childShow("land_use_action");
		childShow("land_use_reason");
	}
	else
	{
		childHide("step_2");
		childHide("land_use_action");
		childHide("land_use_reason");
	}

	// Section three: purchase & currency
	S32 final_balance = mAgentCashBalance + mCurrency.getAmount() - mParcelPrice;
	bool enough_money = final_balance >= 0;
	bool can_pay = mAgentCashBalance >= mParcelPrice;
	S32 min_contrib = llceil((F32)mParcelBillableArea / GROUP_LAND_BONUS_FACTOR);
	bool groupContributionEnough = mParcelGroupContribution >= min_contrib;

	mCurrency.updateUI(!showing_error && !can_pay);

	if (!showing_error)
	{
		childSetBadge("step_3",
					  !enough_money ? BADGE_WARN
									: mCurrency.getAmount() > 0 ? BADGE_NOTE
																: BADGE_OK);

		childSetText("purchase_action",
					 llformat("Pay L$ %d to %s for this land", mParcelPrice,
							  mParcelSellerName.c_str()));
		childSetVisible("purchase_action", mParcelValid);

		std::string reasonString;

		if (can_pay)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", mAgentCashBalance);

			childSetText("currency_reason",
						 getString("have_enough_lindens", string_args));
		}
		else
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", mAgentCashBalance);
			string_args["[AMOUNT2]"] = llformat("%d",
												mParcelPrice - mAgentCashBalance);

			childSetText("currency_reason",
						 getString("not_enough_lindens", string_args));

			childSetTextArg("currency_est", "[AMOUNT2]",
							llformat("%#.2f",
									 mCurrency.getEstimate() / 100.0));
		}

		if (enough_money)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", final_balance);

			childSetText("currency_balance",
						 getString("balance_left", string_args));

		}
		else
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] =
				llformat("%d", mParcelPrice - mAgentCashBalance);

			childSetText("currency_balance",
						 getString("balance_needed", string_args));

		}

		childSetValue("remove_contribution", LLSD(groupContributionEnough));
		childSetEnabled("remove_contribution", groupContributionEnough);
		childSetLabelArg("remove_contribution", "[AMOUNT]",
						 llformat("%d", min_contrib));
		childSetVisible("remove_contribution",
						mParcelIsGroupLand && mParcelGroupContribution > 0);

		childShow("step_3");
		childShow("purchase_action");
		childShow("currency_reason");
		childShow("currency_balance");
	}
	else
	{
		childHide("step_3");
		childHide("purchase_action");
		childHide("currency_reason");
		childHide("currency_balance");
		childHide("remove_contribution");
	}

	bool agrees_to_covenant = false;
	LLCheckBoxCtrl* check = getChild<LLCheckBoxCtrl>("agree_covenant");
	if (check)
	{
	    agrees_to_covenant = check->get();
	}

	childSetEnabled("buy_btn",
					mCanBuy && mSiteValid && enough_money && !mTransaction &&
					agrees_to_covenant);
}

void LLFloaterBuyLand::startBuyPreConfirm()
{
	std::string action;

	if (mSiteMembershipUpgrade)
	{
		action += mSiteMembershipAction;
		action += "\n";

		LLComboBox* levels = getChild<LLComboBox>( "account_level");
		if (levels)
		{
			action += " * ";
			action += mSiteMembershipPlanNames[levels->getCurrentIndex()];
			action += "\n";
		}
	}
	if (mSiteLandUseUpgrade)
	{
		action += mSiteLandUseAction;
		action += "\n";
	}
	if (mCurrency.getAmount() > 0)
	{
		LLStringUtil::format_map_t string_args;
		string_args["[AMOUNT]"] = llformat("%d", mCurrency.getAmount());
		string_args["[AMOUNT2]"] = llformat("%#.2f",
											mCurrency.getEstimate() / 100.0);

		action += getString("buy_for_US", string_args);
	}

	LLStringUtil::format_map_t string_args;
	string_args["[AMOUNT]"] = llformat("%d", mParcelPrice);
	string_args["[SELLER]"] = mParcelSellerName;
	action += getString("pay_to_for_land", string_args);

	LLConfirmationManager::confirm(mSiteConfirm, action, *this,
								   &LLFloaterBuyLand::startBuyPostConfirm);
}

void LLFloaterBuyLand::startBuyPostConfirm(const std::string& password)
{
	runWebSitePrep(password);

	mCanBuy = false;
	mCannotBuyReason = getString("processing");
	refreshUI();
}

//static
void LLFloaterBuyLand::onChangeAgreeCovenant(LLUICtrl*, void* data)
{
	LLFloaterBuyLand* self = (LLFloaterBuyLand*)data;
	if (self)
	{
		self->refreshUI();
	}
}

//static
void LLFloaterBuyLand::onClickBuy(void* data)
{
	LLFloaterBuyLand* self = (LLFloaterBuyLand*)data;
	if (self)
	{
		self->startBuyPreConfirm();
	}
}

//static
void LLFloaterBuyLand::onClickCancel(void* data)
{
	LLFloaterBuyLand* self = (LLFloaterBuyLand*)data;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterBuyLand::onClickErrorWeb(void* data)
{
	LLFloaterBuyLand* self = (LLFloaterBuyLand*)data;
	if (self)
	{
		LLWeb::loadURLExternal(self->mCannotBuyURI);
		self->close();
	}
}
