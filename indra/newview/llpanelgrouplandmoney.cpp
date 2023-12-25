/**
 * @file llpanelgrouplandmoney.cpp
 * @brief Panel for group land and L$.
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

#include "llpanelgrouplandmoney.h"

#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llparcel.h"
#include "llqueryflags.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lltabcontainer.h"
#include "lltrans.h"
#include "lltransactiontypes.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterworldmap.h"
#include "llproductinforequest.h"
#include "llstatusbar.h"
#include "llviewermessage.h"	// send_places_query()
#include "roles_constants.h"

// Account history, how far to look into past, in days
constexpr S32 SUMMARY_INTERVAL = 7;
constexpr S32 SUMMARY_MAX = 8;

////////////////////////////////////////////////////////////////////////////

class LLGroupMoneyTabEventHandler
{
protected:
	LOG_CLASS(LLGroupMoneyTabEventHandler);

public:
	LLGroupMoneyTabEventHandler(LLButton* earlier_button,
								LLButton* later_button,
								LLTextEditor* text_editor,
								LLTabContainer* tab_containerp,
								LLPanel* panelp,
								const std::string& loading_text,
								const LLUUID& group_id,
								S32 interval_length_days,
								S32 max_interval_days);
	virtual ~LLGroupMoneyTabEventHandler();

	virtual void requestData(LLMessageSystem* msg);
	virtual void processReply(LLMessageSystem* msg, void** data);

	virtual void onClickEarlier();
	virtual void onClickLater();
	virtual void onClickTab();

	static void clickEarlierCallback(void* data);
	static void clickLaterCallback(void* data);
	static void clickTabCallback(void* user_data, bool from_click);

public:
	typedef fast_hmap<LLUUID, LLGroupMoneyTabEventHandler*> evt_instances_t;
	static evt_instances_t	sInstanceIDs;

	typedef std::map<LLPanel*, LLGroupMoneyTabEventHandler*> handlers_map_t;
	static handlers_map_t	sTabsToHandlers;

protected:
	class impl;
	impl*					mImplementationp;
};

class LLGroupMoneyDetailsTabEventHandler final
:	public LLGroupMoneyTabEventHandler
{
protected:
	LOG_CLASS(LLGroupMoneyDetailsTabEventHandler);

public:
	LLGroupMoneyDetailsTabEventHandler(LLButton* earlier_buttonp,
									   LLButton* later_buttonp,
									   LLTextEditor* text_editorp,
									   LLTabContainer* tab_containerp,
									   LLPanel* panelp,
									   const std::string& loading_text,
									   const LLUUID& group_id);

	void requestData(LLMessageSystem* msg) override;
	void processReply(LLMessageSystem* msg, void** data) override;
};

class LLGroupMoneySalesTabEventHandler final
:	public LLGroupMoneyTabEventHandler
{
protected:
	LOG_CLASS(LLGroupMoneySalesTabEventHandler);

public:
	LLGroupMoneySalesTabEventHandler(LLButton* earlier_buttonp,
									 LLButton* later_buttonp,
									 LLTextEditor* text_editorp,
									 LLTabContainer* tab_containerp,
									 LLPanel* panelp,
									 const std::string& loading_text,
									 const LLUUID& group_id);

	void requestData(LLMessageSystem* msg) override;
	void processReply(LLMessageSystem* msg, void** data) override;
};

class LLGroupMoneyPlanningTabEventHandler final
:	public LLGroupMoneyTabEventHandler
{
protected:
	LOG_CLASS(LLGroupMoneyPlanningTabEventHandler);

public:
	LLGroupMoneyPlanningTabEventHandler(LLTextEditor* text_editor,
										LLTabContainer* tab_containerp,
										LLPanel* panelp,
										const std::string& loading_text,
										const LLUUID& group_id);

	void requestData(LLMessageSystem* msg) override;
	void processReply(LLMessageSystem* msg, void** data) override;
};

////////////////////////////////////////////////////////////////////////////

class LLPanelGroupLandMoney::impl
{
protected:
	LOG_CLASS(LLPanelGroupLandMoney::impl);

public:
	impl(LLPanelGroupLandMoney& panel, const LLUUID& group_id);
	~impl();

	void requestGroupLandInfo();

	int getStoredContribution();
	void setYourContributionTextField(int contrib);
	void setYourMaxContributionTextBox(int max);

	void onMapButton();
	bool applyContribution();
	void processGroupLand(LLMessageSystem* msg);

	static void mapCallback(void* data);
	static void contributionCommitCallback(LLUICtrl* ctrl, void* userdata);
	static void contributionKeystrokeCallback(LLLineEditor* caller,
											  void* userdata);

public:
	LLPanelGroupLandMoney&			mPanel;

	LLTextBox*						mGroupOverLimitTextp;
	LLIconCtrl*						mGroupOverLimitIconp;
	LLLineEditor*					mYourContributionEditorp;
	LLButton*						mMapButtonp;
	LLGroupMoneyTabEventHandler*	mMoneyDetailsTabEHp;
	LLGroupMoneyTabEventHandler*	mMoneyPlanningTabEHp;
	LLGroupMoneyTabEventHandler*	mMoneySalesTabEHp;
	LLScrollListCtrl*				mGroupParcelsp;

	LLUUID							mGroupID;
	LLUUID							mTransID;

	std::string						mCantViewParcelsText;
	std::string						mCantViewAccountsText;

	bool							mBeenActivated;
	bool							mNeedsSendGroupLandRequest;
	bool							mNeedsApply;
};

//*******************************************
//** LLPanelGroupLandMoney::impl Functions **
//*******************************************
LLPanelGroupLandMoney::impl::impl(LLPanelGroupLandMoney& panel,
								  const LLUUID& group_id)
:	mPanel(panel),
	mGroupID(group_id),
	mBeenActivated(false),
	mNeedsSendGroupLandRequest(true),
	mNeedsApply(false),
	mYourContributionEditorp(NULL),
	mMapButtonp(NULL),
	mGroupParcelsp(NULL),
	mGroupOverLimitTextp(NULL),
	mGroupOverLimitIconp(NULL),
	mMoneySalesTabEHp(NULL),
	mMoneyPlanningTabEHp(NULL),
	mMoneyDetailsTabEHp(NULL)
{
}

LLPanelGroupLandMoney::impl::~impl()
{
	if (mMoneySalesTabEHp)
	{
		delete mMoneySalesTabEHp;
		mMoneySalesTabEHp = NULL;
	}

	if (mMoneyDetailsTabEHp)
	{
		delete mMoneyDetailsTabEHp;
		mMoneyDetailsTabEHp = NULL;
	}

	if (mMoneyPlanningTabEHp)
	{
		delete mMoneyPlanningTabEHp;
		mMoneyPlanningTabEHp = NULL;
	}
}

void LLPanelGroupLandMoney::impl::requestGroupLandInfo()
{
	U32 query_flags = DFQ_GROUP_OWNED;

	mTransID.generate();
	if (mGroupParcelsp) mGroupParcelsp->deleteAllItems();

	send_places_query(mGroupID, mTransID, "", query_flags, LLParcel::C_ANY,
					  "");
}

void LLPanelGroupLandMoney::impl::onMapButton()
{
	if (!mGroupParcelsp)  return;
	LLScrollListItem* itemp = mGroupParcelsp->getFirstSelected();
	if (!itemp) return;

	const LLScrollListCell* cellp =
		itemp->getColumn(itemp->getNumColumns() - 1); // hidden column is last

	F32 global_x = 0.f;
	F32 global_y = 0.f;
	sscanf(cellp->getValue().asString().c_str(), "%f %f", &global_x,
		   &global_y);

	// *HACK: Use the agent's z-height
	F64 global_z = gAgent.getPositionGlobal().mdV[VZ];

	LLVector3d pos_global(global_x, global_y, global_z);
	if (gFloaterWorldMapp)
	{
		gFloaterWorldMapp->trackLocation(pos_global);
		LLFloaterWorldMap::show(NULL, true);
	}
}

bool LLPanelGroupLandMoney::impl::applyContribution()
{
	// Calculate max donation, which is sum of available and current.
	S32 your_contribution = 0;
	S32 sqm_avail;

	your_contribution = getStoredContribution();
	sqm_avail = your_contribution;

	if (gStatusBarp)
	{
		sqm_avail += gStatusBarp->getSquareMetersLeft();
	}

	// Get new contribution and compare to available
	S32 new_contribution = -1;
	if (mYourContributionEditorp)
	{
		new_contribution = atoi(mYourContributionEditorp->getText().c_str());
	}

	if (new_contribution != your_contribution && new_contribution >= 0 &&
	    new_contribution <= sqm_avail)
	{
		// Update group info and server
		if (!gAgent.setGroupContribution(mGroupID, new_contribution))
		{
			// This should never happen...
			llwarns << "Unable to set contribution." << llendl;
			return false;
		}
	}
	else
	{
		// *TODO: throw up some error message here and return ?  For now we
		// just fail silently and force the previous value.
		new_contribution =  your_contribution;
	}

	//set your contribution
	setYourContributionTextField(new_contribution);

	return true;
}

// Retrieves the land contribution for this agent that is currently stored in
// the database, NOT what is currently entered in the text field
int LLPanelGroupLandMoney::impl::getStoredContribution()
{
	LLGroupData group_data;
	group_data.mContribution = 0;
	gAgent.getGroupData(mGroupID, group_data);
	return group_data.mContribution;
}

// Fills in the text field with the contribution, contrib
void LLPanelGroupLandMoney::impl::setYourContributionTextField(int contrib)
{
	if (mYourContributionEditorp)
	{
		mYourContributionEditorp->setText(llformat("%d", contrib));
	}
}

void LLPanelGroupLandMoney::impl::setYourMaxContributionTextBox(int max)
{
	mPanel.childSetTextArg("your_contribution_max_value", "[AMOUNT]",
						   llformat("%d", max));
}

//static
void LLPanelGroupLandMoney::impl::mapCallback(void* data)
{
	LLPanelGroupLandMoney::impl* selfp = (LLPanelGroupLandMoney::impl*)data;
	if (selfp)
	{
		selfp->onMapButton();
	}
}

void LLPanelGroupLandMoney::impl::contributionCommitCallback(LLUICtrl* ctrl,
															 void* userdata)
{
	LLPanelGroupLandMoney* tabp = (LLPanelGroupLandMoney*)userdata;
	LLLineEditor* editorp = (LLLineEditor*)ctrl;
	if (tabp && editorp)
	{
		impl* self = tabp->mImplementationp;
		if (!self) return;	// Paranoia

		S32 new_contribution= atoi(editorp->getText().c_str());
		S32 your_contribution = self->getStoredContribution();

		// reset their junk data to be "good" data to us
		self->setYourContributionTextField(new_contribution);

		// check to see if they're contribution text has changed
		self->mNeedsApply = new_contribution != your_contribution;
		tabp->notifyObservers();
	}
}

void LLPanelGroupLandMoney::impl::contributionKeystrokeCallback(LLLineEditor* caller,
																void* userdata)
{
	impl::contributionCommitCallback(caller, userdata);
}

//static
void LLPanelGroupLandMoney::impl::processGroupLand(LLMessageSystem* msg)
{
	S32 count = msg->getNumberOfBlocks("QueryData");
	if (count > 0 && mGroupParcelsp)
	{
		S32 first_block = 0;

		LLUUID owner_id;
		msg->getUUID("QueryData", "OwnerID", owner_id, 0);

		LLUUID trans_id;
		msg->getUUID("TransactionData", "TransactionID", trans_id);

		S32 total_contribution = 0;
		if (owner_id.isNull())
		{
			// Special block which has total contribution
			++first_block;

			msg->getS32("QueryData", "ActualArea", total_contribution, 0);
			mPanel.childSetTextArg("total_contributed_land_value", "[AREA]",
								   llformat("%d", total_contribution));
		}
		else
		{
			mPanel.childSetTextArg("total_contributed_land_value", "[AREA]",
								   "0");
		}

		if (!gAgent.isInGroup(mGroupID) ||
#if 0		// This power was removed to make group roles simpler
			!gAgent.hasPowerInGroup(mGroupID, GP_LAND_VIEW_OWNED) ||
#endif
			trans_id != mTransID)
		{
			return;
		}

		// We updated more than just the available area special block
		if (count > 1 && mMapButtonp)
		{
			mMapButtonp->setEnabled(true);
		}

		std::string name, desc, sim_name, land_sku, land_type;
		F32 global_x, global_y;
		S32 actual_area, billable_area;
		S32 committed = 0;
		U8 flags;

		LLProductInfoRequestManager* pinfreqmgr =
			LLProductInfoRequestManager::getInstance();
		bool stats_dirty = true;
		for (S32 i = first_block; i < count; ++i)
		{
			msg->getUUID("QueryData", "OwnerID", owner_id, i);
			msg->getString("QueryData", "Name", name, i);
			msg->getString("QueryData", "Desc", desc, i);
			msg->getS32("QueryData", "ActualArea", actual_area, i);
			msg->getS32("QueryData", "BillableArea", billable_area, i);
			msg->getU8("QueryData", "Flags", flags, i);
			msg->getF32("QueryData", "GlobalX", global_x, i);
			msg->getF32("QueryData", "GlobalY", global_y, i);
			msg->getString("QueryData", "SimName", sim_name, i);

			if (msg->getSizeFast(_PREHASH_QueryData, i,
								 _PREHASH_ProductSKU) > 0)
			{
				msg->getStringFast(_PREHASH_QueryData, _PREHASH_ProductSKU,
								   land_sku, i);
				LL_DEBUGS("GroupPanel") << "Land sku: " << land_sku << LL_ENDL;
				land_type = pinfreqmgr->getDescriptionForSku(land_sku);
			}
			else
			{
				land_sku.clear();
				land_type = LLTrans::getString("unknown");
			}

			S32 region_x = ll_roundp(global_x) % REGION_WIDTH_UNITS;
			S32 region_y = ll_roundp(global_y) % REGION_WIDTH_UNITS;
			std::string location = sim_name +
								   llformat(" (%d, %d)", region_x, region_y);

			std::string area;
			if (billable_area == actual_area)
			{
				area = llformat("%d", billable_area);
			}
			else
			{
				area = llformat("%d / %d", billable_area, actual_area);
			}

			std::string hidden;
			hidden = llformat("%f %f", global_x, global_y);

			LLSD row;

			row["columns"][0]["column"] = "name";
			row["columns"][0]["value"] = name;
			row["columns"][0]["font"] = "SANSSERIF_SMALL";

			row["columns"][1]["column"] = "location";
			row["columns"][1]["value"] = location;
			row["columns"][1]["font"] = "SANSSERIF_SMALL";

			row["columns"][2]["column"] = "area";
			row["columns"][2]["value"] = area;
			row["columns"][2]["font"] = "SANSSERIF_SMALL";

			row["columns"][3]["column"] = "type";
			row["columns"][3]["value"] = land_type;
			row["columns"][3]["font"] = "SANSSERIF_SMALL";

			// hidden is always last column
			row["columns"][4]["column"] = "hidden";
			row["columns"][4]["value"] = hidden;

			mGroupParcelsp->addElement(row, ADD_SORTED);

			committed += billable_area;
			mPanel.childSetTextArg("total_land_in_use_value", "[AREA]",
								   llformat("%d", committed));

			S32 available = total_contribution - committed;
			mPanel.childSetTextArg("land_available_value", "[AREA]",
								   llformat("%d", available));

			if (mGroupOverLimitTextp && mGroupOverLimitIconp)
			{
				mGroupOverLimitIconp->setVisible(available < 0);
				mGroupOverLimitTextp->setVisible(available < 0);
			}
			stats_dirty = false;
		}
		if (stats_dirty)
		{
			mPanel.childSetTextArg("total_land_in_use_value", "[AREA]", "0");
			mPanel.childSetTextArg("land_available_value", "[AREA]", "0");
		}
	}
}

//*************************************
//** LLPanelGroupLandMoney Functions **
//*************************************

//static
void* LLPanelGroupLandMoney::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupLandMoney("panel group land money", *group_id);
}

//static
LLPanelGroupLandMoney::group_id_map_t LLPanelGroupLandMoney::sGroupIDs;

LLPanelGroupLandMoney::LLPanelGroupLandMoney(const std::string& name,
											 const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id)
{
	mImplementationp = new impl(*this, group_id);

	// Problem: what if someone has both the group floater open and the finder
	// open to the same group ?  Some maps that map group ids to panels will
	// then only be working for the last panel for a given group id :-(
	LLPanelGroupLandMoney::sGroupIDs[group_id] = this;
}

LLPanelGroupLandMoney::~LLPanelGroupLandMoney()
{
	delete mImplementationp;
	LLPanelGroupLandMoney::sGroupIDs.erase(mGroupID);
}

void LLPanelGroupLandMoney::activate()
{
	if (!mImplementationp->mBeenActivated)
	{
		// Select the first tab
		LLTabContainer* tabp =
			getChild<LLTabContainer>("group_money_tab_container", true, false);
		if (tabp)
		{
			tabp->selectFirstTab();
			mImplementationp->mBeenActivated = true;
		}

		// Fill in the max contribution
		S32 max_avail = mImplementationp->getStoredContribution();
		if (gStatusBarp)
		{
			max_avail += gStatusBarp->getSquareMetersLeft();
		}
		mImplementationp->setYourMaxContributionTextBox(max_avail);
	}

	update(GC_ALL);
}

void LLPanelGroupLandMoney::update(LLGroupChange gc)
{
	if (gc != GC_ALL) return;  // Do not update if it is the wrong panel !

	LLTabContainer* tabp =
		getChild<LLTabContainer>("group_money_tab_container", true, false);
	if (tabp)
	{
		LLPanel* panelp = tabp->getCurrentPanel();
		// Now pull the event handler associated with that L$ tab
		if (panelp)
		{
			LLGroupMoneyTabEventHandler* eh =
				get_ptr_in_map(LLGroupMoneyTabEventHandler::sTabsToHandlers,
							   panelp);
			if (eh)
			{
				eh->onClickTab();
			}
		}
	}

	mImplementationp->requestGroupLandInfo();
	mImplementationp->setYourContributionTextField(mImplementationp->getStoredContribution());
}

bool LLPanelGroupLandMoney::needsApply(std::string& mesg)
{
	return mImplementationp->mNeedsApply;
}

bool LLPanelGroupLandMoney::apply(std::string& mesg)
{
	if (!mImplementationp->applyContribution())
	{
		mesg = getString("land_contrib_error");
		return false;
	}

	mImplementationp->mNeedsApply = false;
	notifyObservers();

	return true;
}

void LLPanelGroupLandMoney::cancel()
{
	// Set the contribution back to the "stored value"
	mImplementationp->setYourContributionTextField(mImplementationp->getStoredContribution());

	mImplementationp->mNeedsApply = false;
	notifyObservers();
}

bool LLPanelGroupLandMoney::postBuild()
{
#if 0	// These powers were removed to make group roles simpler
	bool has_parcel_view = gAgent.hasPowerInGroup(mGroupID,
												  GP_LAND_VIEW_OWNED);
	bool has_accounting_view = gAgent.hasPowerInGroup(mGroupID,
													  GP_ACCOUNTING_VIEW);
#endif

	bool can_view = gAgent.isInGroup(mGroupID);

	mImplementationp->mGroupOverLimitIconp =
		getChild<LLIconCtrl>("group_over_limit_icon", true, false);
	mImplementationp->mGroupOverLimitTextp =
		getChild<LLTextBox>("group_over_limit_text", true, false);

	mImplementationp->mYourContributionEditorp =
		getChild<LLLineEditor>("your_contribution_line_editor", true, false);
	if (mImplementationp->mYourContributionEditorp)
	{
		LLLineEditor* editor = mImplementationp->mYourContributionEditorp;
	    editor->setCommitCallback(mImplementationp->contributionCommitCallback);
		editor->setKeystrokeCallback(mImplementationp->contributionKeystrokeCallback);
		editor->setCallbackUserData(this);
	}

	mImplementationp->mMapButtonp = getChild<LLButton>("map_button", true,
													   false);

	mImplementationp->mGroupParcelsp =
		getChild<LLScrollListCtrl>("group_parcel_list", true, false);

	mImplementationp->mCantViewParcelsText =
		getString("cant_view_group_land_text");
	mImplementationp->mCantViewAccountsText =
		getString("cant_view_group_accounting_text");

	if (mImplementationp->mMapButtonp)
	{
		mImplementationp->mMapButtonp->setClickedCallback(impl::mapCallback,
														  mImplementationp);
	}

	if (mImplementationp->mGroupOverLimitTextp)
	{
		mImplementationp->mGroupOverLimitTextp->setVisible(false);
	}

	if (mImplementationp->mGroupOverLimitIconp)
	{
		mImplementationp->mGroupOverLimitIconp->setVisible(false);
	}

	if (mImplementationp->mMapButtonp)
	{
		mImplementationp->mMapButtonp->setEnabled(false);
	}

	if (mImplementationp->mGroupParcelsp && !can_view)
	{
		mImplementationp->mGroupParcelsp->addCommentText(mImplementationp->mCantViewParcelsText);
		mImplementationp->mGroupParcelsp->setEnabled(false);
	}

	LLTabContainer* tabcp =
		getChild<LLTabContainer>("group_money_tab_container", true, false);
	if (tabcp && !can_view)
	{
		for (S32 i = tabcp->getTabCount() - 1; i >= 0; --i)
		{
			tabcp->enableTabButton(i, false);
		}
	}

	// Pull out the widgets for the L$ details tab
	LLButton* earlierp = getChild<LLButton>("earlier_details_button", true,
											false);
	LLButton* laterp = getChild<LLButton>("later_details_button", true, false);
	LLTextEditor* textp = getChild<LLTextEditor>("group_money_details_text",
												 true, false);
	LLPanel* panelp = getChild<LLPanel>("group_money_details_tab", true,
										false);

	std::string loading_text = getString("loading_txt");
	if (!can_view)
	{
		textp->setText(mImplementationp->mCantViewAccountsText);
	}
	else
	{
		mImplementationp->mMoneyDetailsTabEHp =
			new LLGroupMoneyDetailsTabEventHandler(earlierp, laterp, textp,
												   tabcp, panelp, loading_text,
												   mGroupID);
	}

	textp = getChild<LLTextEditor>("group_money_planning_text", true, false);
	panelp = getChild<LLPanel>("group_money_planning_tab", true, false);

	if (!can_view)
	{
		textp->setText(mImplementationp->mCantViewAccountsText);
	}
	else
	{
		// Temporally disabled for DEV-11287.
		mImplementationp->mMoneyPlanningTabEHp =
			new LLGroupMoneyPlanningTabEventHandler(textp, tabcp, panelp,
													loading_text, mGroupID);
	}

	// Pull out the widgets for the L$ sales tab
	earlierp = getChild<LLButton>("earlier_sales_button", true, false);
	laterp = getChild<LLButton>("later_sales_button", true, false);
	textp = getChild<LLTextEditor>("group_money_sales_text", true, false);
	panelp = getChild<LLPanel>("group_money_sales_tab", true, false);

	if (!can_view)
	{
		textp->setText(mImplementationp->mCantViewAccountsText);
	}
	else
	{
		mImplementationp->mMoneySalesTabEHp =
			new LLGroupMoneySalesTabEventHandler(earlierp, laterp, textp,
												 tabcp, panelp, loading_text,
												 mGroupID);
	}

	return LLPanelGroupTab::postBuild();
}

bool LLPanelGroupLandMoney::isVisibleByAgent()
{
	return mAllowEdit && gAgent.isInGroup(mGroupID);
}

void LLPanelGroupLandMoney::processPlacesReply(LLMessageSystem* msg, void**)
{
	LLUUID group_id;
	msg->getUUID("AgentData", "QueryID", group_id);

	group_id_map_t::iterator found_it = sGroupIDs.find(group_id);
	if (found_it != sGroupIDs.end())
	{
		found_it->second->mImplementationp->processGroupLand(msg);
	}
	else
	{
		llinfos << "Group Panel Land and Money for " << group_id
				<< " no longer in existence. Stale reply ignored." << llendl;
	}
}

//*************************************************
//** LLGroupMoneyTabEventHandler::impl Functions **
//*************************************************

class LLGroupMoneyTabEventHandler::impl
{
protected:
	LOG_CLASS(LLGroupMoneyTabEventHandler);

public:
	impl(LLButton* earlier_buttonp, LLButton* later_buttonp,
		 LLTextEditor* text_editorp, LLPanel* tabpanelp,
		 const std::string& loading_text, const LLUUID& group_id,
		 S32 interval_length_days, S32 max_interval_days);

	bool getCanClickLater();
	bool getCanClickEarlier();

	void updateButtons();

//member variables
public:
	LLUUID mGroupID;
	LLUUID mPanelID;

	LLPanel* mTabPanelp;

	int mIntervalLength;
	int mMaxInterval;
	int mCurrentInterval;

	LLTextEditor* mTextEditorp;
	LLButton*     mEarlierButtonp;
	LLButton*     mLaterButtonp;

	std::string mLoadingText;
};

LLGroupMoneyTabEventHandler::impl::impl(LLButton* earlier_buttonp,
										LLButton* later_buttonp,
										LLTextEditor* text_editorp,
										LLPanel* tabpanelp,
										const std::string& loading_text,
										const LLUUID& group_id,
										S32 interval_length_days,
										S32 max_interval_days)
{
	mGroupID = group_id;
	mPanelID.generate();

	mIntervalLength = interval_length_days;
	mMaxInterval = max_interval_days;
	mCurrentInterval = 0;

	mTextEditorp = text_editorp;
	mEarlierButtonp = earlier_buttonp;
	mLaterButtonp = later_buttonp;
	mTabPanelp = tabpanelp;

	mLoadingText = loading_text;
}

bool LLGroupMoneyTabEventHandler::impl::getCanClickEarlier()
{
	return (mCurrentInterval < mMaxInterval);
}

bool LLGroupMoneyTabEventHandler::impl::getCanClickLater()
{
	return (mCurrentInterval > 0);
}

void LLGroupMoneyTabEventHandler::impl::updateButtons()
{
	if (mEarlierButtonp)
	{
		mEarlierButtonp->setEnabled(getCanClickEarlier());
	}
	if (mLaterButtonp)
	{
		mLaterButtonp->setEnabled(getCanClickLater());
	}
}

//*******************************************
//** LLGroupMoneyTabEventHandler Functions **
//*******************************************

LLGroupMoneyTabEventHandler::evt_instances_t LLGroupMoneyTabEventHandler::sInstanceIDs;
LLGroupMoneyTabEventHandler::handlers_map_t LLGroupMoneyTabEventHandler::sTabsToHandlers;

LLGroupMoneyTabEventHandler::LLGroupMoneyTabEventHandler(LLButton* earlier_buttonp,
														 LLButton* later_buttonp,
														 LLTextEditor* text_editorp,
														 LLTabContainer* tab_containerp,
														 LLPanel* panelp,
														 const std::string& loading_text,
														 const LLUUID& group_id,
														 S32 interval_length_days,
														 S32 max_interval_days)
{
	mImplementationp = new impl(earlier_buttonp, later_buttonp,
								text_editorp, panelp, loading_text,
								group_id, interval_length_days,
								max_interval_days);

	if (earlier_buttonp)
	{
		earlier_buttonp->setClickedCallback(clickEarlierCallback, this);
	}

	if (later_buttonp)
	{
		later_buttonp->setClickedCallback(clickLaterCallback, this);
	}

	mImplementationp->updateButtons();

	if (tab_containerp && panelp)
	{
		tab_containerp->setTabChangeCallback(panelp, clickTabCallback);
		tab_containerp->setTabUserData(panelp, this);
	}

	sInstanceIDs[mImplementationp->mPanelID] = this;
	sTabsToHandlers[panelp] = this;
}

LLGroupMoneyTabEventHandler::~LLGroupMoneyTabEventHandler()
{
	sInstanceIDs.erase(mImplementationp->mPanelID);
	sTabsToHandlers.erase(mImplementationp->mTabPanelp);

	delete mImplementationp;
}

void LLGroupMoneyTabEventHandler::onClickTab()
{
	requestData(gMessageSystemp);
}

void LLGroupMoneyTabEventHandler::requestData(LLMessageSystem*)
{
	// Do nothing
}

void LLGroupMoneyTabEventHandler::processReply(LLMessageSystem*, void**)
{
	// Do nothing
}

void LLGroupMoneyTabEventHandler::onClickEarlier()
{
	if (mImplementationp->mTextEditorp)
	{
		mImplementationp->mTextEditorp->setText(mImplementationp->mLoadingText);
	}
	++mImplementationp->mCurrentInterval;

	mImplementationp->updateButtons();

	requestData(gMessageSystemp);
}

void LLGroupMoneyTabEventHandler::onClickLater()
{
	if (mImplementationp->mTextEditorp)
	{
		mImplementationp->mTextEditorp->setText(mImplementationp->mLoadingText);
	}
	--mImplementationp->mCurrentInterval;

	mImplementationp->updateButtons();

	requestData(gMessageSystemp);
}

//static
void LLGroupMoneyTabEventHandler::clickEarlierCallback(void* data)
{
	LLGroupMoneyTabEventHandler* selfp = (LLGroupMoneyTabEventHandler*)data;
	if (selfp)
	{
		selfp->onClickEarlier();
	}
}

//static
void LLGroupMoneyTabEventHandler::clickLaterCallback(void* data)
{
	LLGroupMoneyTabEventHandler* selfp = (LLGroupMoneyTabEventHandler*)data;
	if (selfp)
	{
		selfp->onClickLater();
	}
}

//static
void LLGroupMoneyTabEventHandler::clickTabCallback(void* data, bool from_click)
{
	LLGroupMoneyTabEventHandler* selfp = (LLGroupMoneyTabEventHandler*)data;
	if (selfp)
	{
		selfp->onClickTab();
	}
}

//**************************************************
//** LLGroupMoneyDetailsTabEventHandler Functions **
//**************************************************

LLGroupMoneyDetailsTabEventHandler::LLGroupMoneyDetailsTabEventHandler(LLButton* earlier_buttonp,
																	   LLButton* later_buttonp,
																	   LLTextEditor* text_editorp,
																	   LLTabContainer* tab_containerp,
																	   LLPanel* panelp,
																	   const std::string& loading_text,
																	   const LLUUID& group_id)
:	LLGroupMoneyTabEventHandler(earlier_buttonp, later_buttonp,
							   text_editorp, tab_containerp, panelp,
							   loading_text, group_id, SUMMARY_INTERVAL,
							   SUMMARY_MAX)
{
}

void LLGroupMoneyDetailsTabEventHandler::requestData(LLMessageSystem* msg)
{
	msg->newMessageFast(_PREHASH_GroupAccountDetailsRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_GroupID,  mImplementationp->mGroupID);
	msg->nextBlockFast(_PREHASH_MoneyData);
	msg->addUUIDFast(_PREHASH_RequestID, mImplementationp->mPanelID);
	msg->addS32Fast(_PREHASH_IntervalDays, mImplementationp->mIntervalLength);
	msg->addS32Fast(_PREHASH_CurrentInterval, mImplementationp->mCurrentInterval);

	gAgent.sendReliableMessage();

	if (mImplementationp->mTextEditorp)
	{
		mImplementationp->mTextEditorp->setText(mImplementationp->mLoadingText);
	}

	LLGroupMoneyTabEventHandler::requestData(msg);
}

void LLGroupMoneyDetailsTabEventHandler::processReply(LLMessageSystem* msg,
													  void** data)
{
	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id);
	if (mImplementationp->mGroupID != group_id)
	{
		llwarns << "Group Account details not for this group !" << llendl;
		return;
	}

	S32 interval_days;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_IntervalDays, interval_days);
	S32 current_interval;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_CurrentInterval, current_interval);
	std::string start_date;
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_StartDate, start_date);

	if (interval_days != mImplementationp->mIntervalLength ||
		current_interval != mImplementationp->mCurrentInterval)
	{
		llinfos << "Out of date details packet " << interval_days << " "
				<< current_interval << llendl;
		return;
	}

	std::string text = start_date + "\n\n";

	S32 total_amount = 0;
	S32 transactions = msg->getNumberOfBlocksFast(_PREHASH_HistoryData);
	for (S32 i = 0; i < transactions; ++i)
	{
		std::string desc;
		desc.clear();
		msg->getStringFast(_PREHASH_HistoryData, _PREHASH_Description,
						   desc, i);
		S32 amount = 0;
		msg->getS32Fast(_PREHASH_HistoryData, _PREHASH_Amount, amount, i);

		if (amount != 0)
		{
			text.append(llformat("%-24s %6d\n", desc.c_str(), amount));
		}

		total_amount += amount;
	}

	text += "\n" + llformat("%-24s %6d\n", "Total", total_amount);

	if (mImplementationp->mTextEditorp)
	{
		mImplementationp->mTextEditorp->setText(text);
	}
}

//static
void LLPanelGroupLandMoney::processGroupAccountDetailsReply(LLMessageSystem* msg,
															void** data)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (gAgentID != agent_id)
	{
		llwarns << "Got group L$ history reply for another agent !" << llendl;
		return;
	}

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_MoneyData, _PREHASH_RequestID, request_id);

	LLGroupMoneyTabEventHandler* selfp =
		get_ptr_in_map(LLGroupMoneyTabEventHandler::sInstanceIDs, request_id);
	if (selfp)
	{
		selfp->processReply(msg, data);
	}
	else
	{
		llwarns << "GroupAccountDetails received for non-existent group panel."
				<< llendl;
	}
}

//************************************************
//** LLGroupMoneySalesTabEventHandler Functions **
//************************************************

LLGroupMoneySalesTabEventHandler::LLGroupMoneySalesTabEventHandler(LLButton* earlier_buttonp,
																   LLButton* later_buttonp,
																   LLTextEditor* text_editorp,
																   LLTabContainer* tab_containerp,
																   LLPanel* panelp,
																   const std::string& loading_text,
																   const LLUUID& group_id)
:	LLGroupMoneyTabEventHandler(earlier_buttonp, later_buttonp, text_editorp,
								tab_containerp, panelp, loading_text, group_id,
								SUMMARY_INTERVAL, SUMMARY_MAX)
{
}

void LLGroupMoneySalesTabEventHandler::requestData(LLMessageSystem* msg)
{
	msg->newMessageFast(_PREHASH_GroupAccountTransactionsRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_GroupID, mImplementationp->mGroupID);
	msg->nextBlockFast(_PREHASH_MoneyData);
	msg->addUUIDFast(_PREHASH_RequestID, mImplementationp->mPanelID);
	msg->addS32Fast(_PREHASH_IntervalDays, mImplementationp->mIntervalLength);
	msg->addS32Fast(_PREHASH_CurrentInterval, mImplementationp->mCurrentInterval);

	gAgent.sendReliableMessage();

	if (mImplementationp->mTextEditorp)
	{
		mImplementationp->mTextEditorp->setText(mImplementationp->mLoadingText);
	}

	LLGroupMoneyTabEventHandler::requestData(msg);
}

void LLGroupMoneySalesTabEventHandler::processReply(LLMessageSystem* msg,
													void** data)
{
	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id);
	if (mImplementationp->mGroupID != group_id)
	{
		llwarns << "Group Account Transactions not for this group !" << llendl;
		return;
	}

	std::string text = mImplementationp->mTextEditorp->getText();

	S32 interval_days;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_IntervalDays, interval_days);
	S32 current_interval;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_CurrentInterval,
					current_interval);
	std::string start_date;
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_StartDate, start_date);

	if (interval_days != mImplementationp->mIntervalLength ||
	    current_interval != mImplementationp->mCurrentInterval)
	{
		llinfos << "Out of date details packet " << interval_days << " "
				<< current_interval << llendl;
		return;
	}

	// If this is the first packet, clear the text, do not append.
	if (text == mImplementationp->mLoadingText)
	{
		// Start with the date.
		text = start_date + "\n\n";
	}

	S32 transactions = msg->getNumberOfBlocksFast(_PREHASH_HistoryData);
	if (transactions == 0)
	{
		text += "(none)";
	}
	else
	{
		for (S32 i = 0; i < transactions; ++i)
		{
			std::string time, user, item;
			S32 type = 0;
			S32 amount = 0;

			msg->getStringFast(_PREHASH_HistoryData, _PREHASH_Time, time, i);
			msg->getStringFast(_PREHASH_HistoryData, _PREHASH_User, user, i);
			msg->getS32Fast(_PREHASH_HistoryData, _PREHASH_Type, type, i);
			msg->getStringFast(_PREHASH_HistoryData, _PREHASH_Item, item, i);
			msg->getS32Fast(_PREHASH_HistoryData, _PREHASH_Amount, amount, i);

			if (amount != 0)
			{
				std::string verb;
				switch (type)
				{
					case TRANS_OBJECT_SALE:
						verb = "bought";
						break;

					case TRANS_GIFT:
						verb = "paid you";
						break;

					case TRANS_PAY_OBJECT:
						verb = "paid into";
						break;

					case TRANS_LAND_PASS_SALE:
						verb = "bought pass to";
						break;

					case TRANS_EVENT_FEE:
						verb = "paid fee for event";
						break;

					case TRANS_EVENT_PRIZE:
						verb = "paid prize for event";
						break;

					default:
						verb = "";
				}

				text +=  llformat("%s %6d - %s %s %s\n", time.c_str(), amount,
								  user.c_str(), verb.c_str(), item.c_str());
			}
		}
	}

	if (mImplementationp->mTextEditorp)
	{
		mImplementationp->mTextEditorp->setText(text);
	}
}

//static
void LLPanelGroupLandMoney::processGroupAccountTransactionsReply(LLMessageSystem* msg,
																 void** data)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (gAgentID != agent_id)
	{
		llwarns << "Got group L$ history reply for another agent !" << llendl;
		return;
	}

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_MoneyData, _PREHASH_RequestID, request_id);

	LLGroupMoneyTabEventHandler* self =
		get_ptr_in_map(LLGroupMoneyTabEventHandler::sInstanceIDs, request_id);
	if (self)
	{
		self->processReply(msg, data);
	}
	else
	{
		llwarns << "GroupAccountTransactions received for non-existent group panel."
				<< llendl;
	}
}

//***************************************************
//** LLGroupMoneyPlanningTabEventHandler Functions **
//***************************************************

LLGroupMoneyPlanningTabEventHandler::LLGroupMoneyPlanningTabEventHandler(LLTextEditor* text_editorp,
																		 LLTabContainer* tab_containerp,
																		 LLPanel* panelp,
																		 const std::string& loading_text,
																		 const LLUUID& group_id)
:	LLGroupMoneyTabEventHandler(NULL, NULL, text_editorp, tab_containerp,
								panelp, loading_text, group_id,
								SUMMARY_INTERVAL, SUMMARY_MAX)
{
}

void LLGroupMoneyPlanningTabEventHandler::requestData(LLMessageSystem* msg)
{
	msg->newMessageFast(_PREHASH_GroupAccountSummaryRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_GroupID, mImplementationp->mGroupID);
	msg->nextBlockFast(_PREHASH_MoneyData);
	msg->addUUIDFast(_PREHASH_RequestID, mImplementationp->mPanelID);
	msg->addS32Fast(_PREHASH_IntervalDays, mImplementationp->mIntervalLength);
	msg->addS32Fast(_PREHASH_CurrentInterval, 0); //planning has 0 interval

	gAgent.sendReliableMessage();

	if (mImplementationp->mTextEditorp)
	{
		mImplementationp->mTextEditorp->setText(mImplementationp->mLoadingText);
	}

	LLGroupMoneyTabEventHandler::requestData(msg);
}

void LLGroupMoneyPlanningTabEventHandler::processReply(LLMessageSystem* msg,
													   void** data)
{
	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id);
	if (mImplementationp->mGroupID != group_id)
	{
		llwarns << "Group Account Summary received not for this group !"
				<< llendl;
		return;
	}

	S32 interval_days;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_IntervalDays, interval_days);
	S32 current_interval;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_CurrentInterval, current_interval);
	S32 balance;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_Balance, balance);
	S32 total_credits;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_TotalCredits, total_credits);
	S32 total_debits;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_TotalDebits, total_debits);
	S32 cur_object_tax;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_ObjectTaxCurrent,
					cur_object_tax);
	S32 cur_light_tax;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_LightTaxCurrent,
					cur_light_tax);
	S32 cur_land_tax;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_LandTaxCurrent, cur_land_tax);
	S32 cur_group_tax;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_GroupTaxCurrent,
					cur_group_tax);
	S32 cur_parcel_dir_fee;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_ParcelDirFeeCurrent,
					cur_parcel_dir_fee);
	S32 proj_object_tax;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_ObjectTaxEstimate,
					proj_object_tax);
	S32 proj_light_tax;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_LightTaxEstimate,
					proj_light_tax);
	S32 proj_land_tax;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_LandTaxEstimate,
					proj_land_tax);
	S32 proj_group_tax;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_GroupTaxEstimate,
					proj_group_tax);
	S32 proj_parcel_dir_fee;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_ParcelDirFeeEstimate,
					proj_parcel_dir_fee);
	S32	non_exempt_members;
	msg->getS32Fast(_PREHASH_MoneyData, _PREHASH_NonExemptMembers,
					non_exempt_members);

	std::string start_date;
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_StartDate, start_date);
	std::string last_stipend_date;
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_LastTaxDate,
					   last_stipend_date);
	std::string next_stipend_date;
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_TaxDate,
					   next_stipend_date);

	if (interval_days != mImplementationp->mIntervalLength ||
		current_interval != mImplementationp->mCurrentInterval)
	{
		llinfos << "Out of date summary packet " << interval_days << " "
				<< current_interval << llendl;
		return;
	}

	std::string text = "Summary for this week, beginning on " + start_date;
	if (current_interval == 0)
	{
		text += "The next stipend day is " + next_stipend_date + "\n\n";
		text += llformat("%-24sL$%6d\n\n", "Balance", balance);
	}

#if 0	// [DEV-29503] Hide the individual info since non_exempt_member here is
		// a wrong choice to calculate individual shares.
	text += "                      Group       Individual Share\n";
	text += llformat("%-24s %6d      %6d \n", "Credits", total_credits,
					 (S32)floorf((F32)total_credits /
								 (F32)non_exempt_members));
	text += llformat("%-24s %6d      %6d \n", "Debits", total_debits,
					 (S32)floorf((F32)total_debits / (F32)non_exempt_members));
	text += llformat("%-24s %6d      %6d \n", "Total",
					 total_credits + total_debits,
					 (S32)floorf((F32)(total_credits + total_debits) /
								 (F32)non_exempt_members));
#endif

	text += "                      Group\n";
	text += llformat("%-24s %6d\n", "Credits", total_credits);
	text += llformat("%-24s %6d\n", "Debits", total_debits);
	text += llformat("%-24s %6d\n", "Total", total_credits + total_debits);

	if (mImplementationp->mTextEditorp)
	{
		mImplementationp->mTextEditorp->setText(text);
	}
}

//static
void LLPanelGroupLandMoney::processGroupAccountSummaryReply(LLMessageSystem* msg,
															void** data)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (gAgentID != agent_id)
	{
		llwarns << "Got group L$ history reply for another agent!" << LL_ENDL;
		return;
	}

	LLUUID request_id;
	gMessageSystemp->getUUIDFast(_PREHASH_MoneyData, _PREHASH_RequestID,
								request_id);

	LLGroupMoneyTabEventHandler* self =
		get_ptr_in_map(LLGroupMoneyTabEventHandler::sInstanceIDs, request_id);
	if (self)
	{
		self->processReply(gMessageSystemp, data);
	}
	else
	{
		llwarns << "GroupAccountSummary received for non-existent group L$ planning tab."
				<< llendl;
	}
}
