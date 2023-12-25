/**
 * @file llfloaterbuyland.h
 * @brief LLFloaterBuyLand class definition
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

#ifndef LL_LLFLOATERBUYLAND_H
#define LL_LLFLOATERBUYLAND_H

#include "llfloater.h"
#include "llsafehandle.h"

#include "llcurrencyuimanager.h"
#include "llviewerparcelmgr.h"

class LLViewerRegion;
class LLXMLRPCTransaction;
class LLXMLRPCValue;

class LLFloaterBuyLand final : public LLFloater,
							   public LLFloaterSingleton<LLFloaterBuyLand>
{
	friend class LLUISingleton<LLFloaterBuyLand, VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterBuyLand);

public:
	~LLFloaterBuyLand() override;

	bool postBuild() override;

	void draw() override;

	void setMinimized(bool b) override;
	bool canClose() override;

	// Public interface methods:
	static void buyLand(LLViewerRegion* region,
						LLSafeHandle<LLParcelSelection> parcel,
						bool is_for_group);
	static void updateCovenantText(const std::string& string,
								   const LLUUID& asset_id);
	static void updateEstateName(const std::string& name);
	static void updateLastModified(const std::string& text);
	static void updateEstateOwnerName(const std::string& name);

private:
	// Open only via the buyLand() method defined above
	LLFloaterBuyLand(const LLSD&);

	void setForGroup(bool is_for_group);
	void setParcel(LLViewerRegion* region, LLParcelSelectionHandle parcel);

	void updateAgentInfo();
	void updateParcelInfo();
	void updateCovenantInfo();
	void setCovenantText(const std::string& string, const LLUUID& asset_id);
	void setEstateName(const std::string& name);
	void setLastModified(const std::string& text);
	void setEstateOwnerName(const std::string& name);
	void updateWebSiteInfo();
	void finishWebSiteInfo();

	void runWebSitePrep(const std::string& password);
	void finishWebSitePrep();
	void sendBuyLand();

	void updateNames();

	void refreshUI();

	enum TransactionType
	{
		TransactionPreflight,
		TransactionCurrency,
		TransactionBuy
	};
	void startTransaction(TransactionType type, LLXMLRPCValue params);
	bool checkTransaction();

	void tellUserError(const std::string& message, const std::string& uri);

	void startBuyPreConfirm();
	void startBuyPostConfirm(const std::string& password);

	static void onChangeAgreeCovenant(LLUICtrl*, void* data);

	static void onClickBuy(void* data);
	static void onClickCancel(void* data);
	static void onClickErrorWeb(void* data);

	class SelectionObserver final : public LLParcelSelectionObserver
	{
	public:
		void changed() override;
	};

	static void callbackCacheName(const LLUUID&, const std::string&, bool);

private:
	LLXMLRPCTransaction*				mTransaction;
	TransactionType		 				mTransactionType;

	LLViewerParcelMgr::ParcelBuyInfo*	mParcelBuyInfo;

	LLViewerRegion*						mRegion;
	LLParcelSelectionHandle				mParcelSelection;

	LLCurrencyUIManager					mCurrency;

	// Values in current Preflight transaction... used to avoid extra
	// preflights when the parcel manager goes update crazy
	S32									mPreflightAskBillableArea;
	S32									mPreflightAskCurrencyBuy;

	// Information about the parcel
	LLUUID								mParcelSnapshot;
	S32									mParcelGroupContribution;
	S32									mParcelPrice;
	S32									mParcelActualArea;
	S32									mParcelBillableArea;
	S32									mParcelSupportedObjects;

	// User's choices
	S32									mUserPlanChoice;

	// Information about the agent
	S32									mAgentCommittedTier;
	S32									mAgentCashBalance;

	bool								mIsClaim;
	bool								mIsForGroup;

	bool								mBought;
	bool								mCanBuy;
	bool								mCannotBuyIsError;

	// Information about the agent
	bool								mAgentHasNeverOwnedLand;

	// Information about the parcel
	bool								mParcelValid;
	bool								mParcelIsForSale;
	bool								mParcelIsGroupLand;
	bool								mParcelSoldWithObjects;

	// From website
	bool								mSiteValid;
	bool								mSiteMembershipUpgrade;
	bool								mSiteLandUseUpgrade;

	std::string							mCannotBuyReason;
	std::string							mCannotBuyURI;

	// Information about the parcel
	std::string							mParcelLocation;
	std::string							mParcelSellerName;

	// From website
	std::string							mSiteMembershipAction;
	std::string							mSiteLandUseAction;
	std::string							mSiteConfirm;
	std::vector<std::string>			mSiteMembershipPlanIDs;
	std::vector<std::string>			mSiteMembershipPlanNames;

	static LLParcelSelectionObserver*	sParcelObserver;
};

#endif
