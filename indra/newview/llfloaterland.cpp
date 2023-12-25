/**
 * @file llfloaterland.cpp
 * @brief "About land" floater, allowing display and editing of land parcel
 *        properties.
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

#include <sstream>
#include <time.h>

#include "llfloaterland.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llexperiencecache.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llradiogroup.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"					// For gFrameTimeSeconds
#include "llfloaterauction.h"
#include "llfloateravatarinfo.h"
#include "llfloateravatarpicker.h"
#include "llfloatergroups.h"
#include "llfloatergroupinfo.h"
#include "llpanelexperiencelisteditor.h"
#include "llpanellandaudio.h"
#include "hbpanellandenvironment.h"
#include "llpanellandmedia.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltexturectrl.h"
#include "lluserauth.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"				// For formatted_time()
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "roles_constants.h"

static const std::string OWNER_ONLINE = "0";
static const std::string OWNER_OFFLINE = "1";
static const std::string OWNER_GROUP = "2";

// Constants used in callbacks below; syntactic sugar.
constexpr bool BUY_GROUP_LAND = true;
constexpr bool BUY_PERSONAL_LAND = false;

// Statics
LLFloaterLandParcelSelectObserver* LLFloaterLand::sObserver = NULL;
S32 LLFloaterLand::sLastTab = 0;

// Helper function
static std::string plain_text_duration(S32 seconds)
{
	std:: string tmp;
	if (seconds <= 0)
	{
		return tmp;
	}
	S32 amount = seconds;
	if (seconds >= 7200)
	{
		tmp = "hours";
		amount /= 3600;
	}
	else if (seconds >= 3600)
	{
		tmp = "hour";
		amount = 1;
	}
	else if (seconds >= 120)
	{
		tmp = "minutes";
		amount /= 60;
	}
	else if (seconds >= 60)
	{
		tmp = "minute";
		amount = 1;
	}
	else if (seconds > 1)
	{
		tmp = "seconds";
	}
	else
	{
		tmp = "second";
	}
	tmp = "%d " + LLTrans::getString(tmp);
	return llformat(tmp.c_str(), amount);
}

///////////////////////////////////////////////////////////////////////////////
// Local classes
///////////////////////////////////////////////////////////////////////////////

// LLFloaterLandParcelSelectObserver

class LLFloaterLandParcelSelectObserver final
:	public LLParcelSelectionObserver
{
public:
	void changed() override
	{
		LLFloaterLand::getInstance()->refresh();
	}
};

// LLFloaterBanDuration

class LLFloaterBanDuration final : public LLFloater
{
public:
	~LLFloaterBanDuration() override;

	bool postBuild() override;

	typedef void(*callback_t)(const uuid_vec_t&, S32, void*);

	// Call this to set the ban duration on a list of avatars. The callback
	// function will be called with the avatar UUIDs list and an expiration
	// date (in seconds since UNIX epoch) for a temporary ban or 0 for a
	// permanent ban.
	static LLFloaterBanDuration* show(const uuid_vec_t& ids,
									  callback_t callback, void* userdata);

private:
	// Do not call this directly. Use the show() method above.
	LLFloaterBanDuration(const uuid_vec_t& ids, callback_t callback,
						 void* userdata);

	static void onBtnBan(void* userdata);
	static void onBtnCancel(void* userdata);
	static void onRadioCheck(LLUICtrl* ctrl, void* userdata);

private:
	LLSpinCtrl*					mDurationSpin;

	void						(*mCallback)(const uuid_vec_t& ids,
											 S32 duration, void* userdata);
	void*						mCallbackUserdata;
	uuid_vec_t					mAvatarIds;

	bool						mPermanentBan;

	typedef fast_hset<LLFloaterBanDuration*> instances_list_t;
	static instances_list_t		sInstances;
};

LLFloaterBanDuration::instances_list_t LLFloaterBanDuration::sInstances;

//static
LLFloaterBanDuration* LLFloaterBanDuration::show(const uuid_vec_t& ids,
												 callback_t callback,
												 void* userdata)
{
	LLFloaterBanDuration* self = NULL;
	for (instances_list_t::iterator it = sInstances.begin(),
									end = sInstances.end();
		 it != end; ++it)
	{
		LLFloaterBanDuration* instance = *it;
		if (instance && instance->mCallback == callback &&
			instance->mCallbackUserdata == userdata)
		{
			self = instance;
			break;
		}
	}

	if (!self)
	{
		self = new LLFloaterBanDuration(ids, callback, userdata);
	}

	self->open();

	return self;
}

LLFloaterBanDuration::LLFloaterBanDuration(const uuid_vec_t& ids,
										   callback_t callback, void* userdata)
:	mAvatarIds(ids),
	mCallback(callback),
	mCallbackUserdata(userdata),
	mPermanentBan(true)
{
	sInstances.insert(this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_ban_duration.xml");
}

LLFloaterBanDuration::~LLFloaterBanDuration()
{
	sInstances.erase(this);
}

bool LLFloaterBanDuration::postBuild()
{
	mDurationSpin = getChild<LLSpinCtrl>("ban_hours");
	mDurationSpin->setEnabled(false);

	LLRadioGroup* radio = getChild<LLRadioGroup>("ban_type");
	radio->selectNthItem(0);
	radio->setCommitCallback(onRadioCheck);
	radio->setCallbackUserData(this);

	childSetAction("ok_btn", onBtnBan, this);
	childSetAction("cancel_btn", onBtnCancel, this);

	center();

	return true;
}

//static
void LLFloaterBanDuration::onBtnBan(void* userdata)
{
	LLFloaterBanDuration* self = (LLFloaterBanDuration*)userdata;
	if (self)
	{
		if (self->mCallback)
		{
			S32 time = 0;
			if (!self->mPermanentBan)
			{
                time = 3600 * self->mDurationSpin->getValue().asInteger();
				LL_DEBUGS("ParcelAccess") << "Ban duration will be: " << time
										  << " seconds" << LL_ENDL;
                time += LLTimer::getEpochSeconds();
			}
			else
			{
				LL_DEBUGS("ParcelAccess") << "Ban will be permanent"
										  << LL_ENDL;
			}
			LL_DEBUGS("ParcelAccess") << "Requesting ban for the following avatars: ";
			for (U32 i = 0, count = self->mAvatarIds.size(); i < count; ++i)
			{
				if (i > 0)
				{
					LL_CONT << ", ";
				}
				LL_CONT << self->mAvatarIds[i];
			}
			LL_CONT << LL_ENDL;
			self->mCallback(self->mAvatarIds, time, self->mCallbackUserdata);
		}
		self->close();
	}
}

//static
void LLFloaterBanDuration::onBtnCancel(void* userdata)
{
	LLFloaterBanDuration* self = (LLFloaterBanDuration*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterBanDuration::onRadioCheck(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterBanDuration* self = (LLFloaterBanDuration*)userdata;
	LLRadioGroup* radio = (LLRadioGroup*)ctrl;
	if (self && radio)
	{
		self->mPermanentBan = radio->getSelectedIndex() == 0;
		self->mDurationSpin->setEnabled(!self->mPermanentBan);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterLand class proper
///////////////////////////////////////////////////////////////////////////////

void send_parcel_select_objects(S32 parcel_local_id, U32 return_type,
								owners_list_t* return_ids)
{
	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	if (!region) return;

	// Since new highlight will be coming in, drop any highlights
	// that exist right now.
	gSelectMgr.unhighlightAll();

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelSelectObjects);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addU32Fast(_PREHASH_ReturnType, return_type);

	// Throw all return ids into the packet.
	// TODO: Check for too many ids.
	if (return_ids)
	{
		for (owners_list_t::iterator it = return_ids->begin(),
									 end = return_ids->end();
			 it != end; ++it)
		{
			msg->nextBlockFast(_PREHASH_ReturnIDs);
			msg->addUUIDFast(_PREHASH_ReturnID, (*it));
		}
	}
	else
	{
		// Put in a null key so that the message is complete.
		msg->nextBlockFast(_PREHASH_ReturnIDs);
		msg->addUUIDFast(_PREHASH_ReturnID, LLUUID::null);
	}

	msg->sendReliable(region->getHost());
}

//static
LLPanelLandObjects* LLFloaterLand::getCurrentPanelLandObjects()
{
	return getInstance()->mPanelObjects;
}

//static
LLPanelLandCovenant* LLFloaterLand::getCurrentPanelLandCovenant()
{
	return getInstance()->mPanelCovenant;
}

void LLFloaterLand::onOpen()
{
#if 0	// Done automatically when the selected parcel's properties arrive
		// (and hence we have the local id).
	gViewerParcelMgr.sendParcelAccessListRequest(AL_ACCESS | AL_BAN |
												 AL_RENTER);
#endif

	mParcel = gViewerParcelMgr.getFloatingParcelSelection();

	// Refresh even if not over a region so we do not get an uninitialized
	// dialog. The dialog is 0-region aware.
	refresh();
}

//virtual
void LLFloaterLand::onClose(bool app_quitting)
{
	gViewerParcelMgr.removeSelectionObserver(sObserver);
	delete sObserver;
	sObserver = NULL;

	// Might have been showing owned objects
	gSelectMgr.unhighlightAll();

	// Save which panel we had open
	sLastTab = mTabLand->getCurrentPanelIndex();

	destroy();
}

LLFloaterLand::LLFloaterLand(const LLSD&)
:	mPanelExperiences(NULL),
	mPanelEnvironment(NULL)
{
	LLCallbackMap::map_t factory_map;
	factory_map["land_general_panel"] = LLCallbackMap(createPanelLandGeneral,
													  this);
	factory_map["land_covenant_panel"] = LLCallbackMap(createPanelLandCovenant,
													   this);
	factory_map["land_objects_panel"] = LLCallbackMap(createPanelLandObjects,
													  this);
	factory_map["land_options_panel"] = LLCallbackMap(createPanelLandOptions,
													  this);
	factory_map["land_audio_panel"] = LLCallbackMap(createPanelLandAudio,
													this);
	factory_map["land_media_panel"] = LLCallbackMap(createPanelLandMedia,
													this);
	factory_map["land_access_panel"] = LLCallbackMap(createPanelLandAccess,
													 this);

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_about_land.xml",
												 &factory_map, false);

	sObserver = new LLFloaterLandParcelSelectObserver();
	gViewerParcelMgr.addSelectionObserver(sObserver);
}

bool LLFloaterLand::postBuild()
{
	mTabLand = getChild<LLTabContainer>("landtab");

	// Add the experiences tab if needed
	if (gAgent.hasRegionCapability("RegionExperiences"))
	{
		mPanelExperiences = new LLPanelLandExperiences(mParcel);
		mTabLand->addTabPanel(mPanelExperiences,
							  mPanelExperiences->getLabel());
	}

	// Add the environment tab if needed
	if (gAgent.hasExtendedEnvironment())
	{
		mPanelEnvironment = new HBPanelLandEnvironment(mParcel);
		mTabLand->addTabPanel(mPanelEnvironment,
							  mPanelEnvironment->getLabel());
	}

	if (sLastTab < mTabLand->getTabCount())
	{
		mTabLand->selectTab(sLastTab);
	}
	else
	{
		sLastTab = 0;
	}

	return true;
}

//virtual
LLFloaterLand::~LLFloaterLand()
{
	// Release the selection handle
	mParcel = NULL;
}

//virtual
void LLFloaterLand::refresh()
{
	mPanelGeneral->refresh();
	mPanelObjects->refresh();
	mPanelOptions->refresh();
	mPanelAudio->refresh();
	mPanelMedia->refresh();
	mPanelAccess->refresh();
	mPanelCovenant->refresh();
	if (mPanelExperiences)
	{
		mPanelExperiences->refresh();
	}
	if (mPanelEnvironment)
	{
		mPanelEnvironment->refresh();
	}
}

void* LLFloaterLand::createPanelLandGeneral(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelGeneral = new LLPanelLandGeneral(self->mParcel);
	return self->mPanelGeneral;
}

//static
void* LLFloaterLand::createPanelLandCovenant(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelCovenant = new LLPanelLandCovenant(self->mParcel);
	return self->mPanelCovenant;
}

//static
void* LLFloaterLand::createPanelLandObjects(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelObjects = new LLPanelLandObjects(self->mParcel);
	return self->mPanelObjects;
}

//static
void* LLFloaterLand::createPanelLandOptions(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelOptions = new LLPanelLandOptions(self->mParcel);
	return self->mPanelOptions;
}

//static
void* LLFloaterLand::createPanelLandAudio(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelAudio = new LLPanelLandAudio(self->mParcel);
	return self->mPanelAudio;
}

//static
void* LLFloaterLand::createPanelLandMedia(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelMedia = new LLPanelLandMedia(self->mParcel);
	return self->mPanelMedia;
}

//static
void* LLFloaterLand::createPanelLandAccess(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelAccess = new LLPanelLandAccess(self->mParcel);
	return self->mPanelAccess;
}

//---------------------------------------------------------------------------
// LLPanelLandGeneral
//---------------------------------------------------------------------------

LLPanelLandGeneral::LLPanelLandGeneral(LLParcelSelectionHandle& parcel)
:	LLPanel("land_general_panel"),
	mUncheckedSell(false),
	mParcel(parcel)
{
}

bool LLPanelLandGeneral::postBuild()
{
	mEditName = getChild<LLLineEditor>("name_editor");
	mEditName->setCommitCallback(onCommitAny);
	mEditName->setPrevalidate(LLLineEditor::prevalidatePrintableNotPipe);
	mEditName->setCallbackUserData(this);

	mEditDesc = getChild<LLTextEditor>("desc_editor");
	mEditDesc->setCommitOnFocusLost(true);
	mEditDesc->setCommitCallback(onCommitAny);
	mEditDesc->setCallbackUserData(this);
#if 0	// No prevalidate function; historically the prevalidate function was
		// broken, allowing residents to put in characters like U+2661 WHITE
		// HEART SUIT, so preserve that ability.
	mEditDesc->setPrevalidate(LLLineEditor::prevalidatePrintableNotPipe);
#endif

	mTextSalePending = getChild<LLTextBox>("sale_pending");
	mTextOwner = getChild<LLTextBox>("owner_text");

	mContentRating = getChild<LLTextBox>("rating_text");
	mLandType = getChild<LLTextBox>("land_type_text");

	mBtnProfile = getChild<LLButton>("profile_btn");
	mBtnProfile->setClickedCallback(onClickProfile, this);

	mTextGroup = getChild<LLTextBox>("group_text");

	mBtnSetGroup = getChild<LLButton>("group_btn");
	mBtnSetGroup->setClickedCallback(onClickSetGroup, this);

	mCheckDeedToGroup = getChild<LLCheckBoxCtrl>("check_deed");
	mCheckDeedToGroup->setCommitCallback(onCommitAny);
	mCheckDeedToGroup->setCallbackUserData(this);

	mBtnDeedToGroup = getChild<LLButton>("deed_btn");
	mBtnDeedToGroup->setClickedCallback(onClickDeed, this);

	mCheckContributeWithDeed = getChild<LLCheckBoxCtrl>("check_contrib");
	mCheckContributeWithDeed->setCommitCallback(onCommitAny);
	mCheckContributeWithDeed->setCallbackUserData(this);

	mSaleInfoNotForSale = getChild<LLTextBox>("not_for_sale");
	mSaleInfoForSale1 = getChild<LLTextBox>("sale_price");

	mBtnSellLand = getChild<LLButton>("sell_btn");
	mBtnSellLand->setClickedCallback(onClickSellLand, this);

	mSaleInfoForSale2 = getChild<LLTextBox>("for_sale_to");
	mSaleInfoForSaleObjects = getChild<LLTextBox>("sell_with_objects");
	mSaleInfoForSaleNoObjects = getChild<LLTextBox>("sell_without_objects");

	mBtnStopSellLand = getChild<LLButton>("cancel_sale_btn");
	mBtnStopSellLand->setClickedCallback(onClickStopSellLand, this);

	mTextClaimDateLabel = getChild<LLTextBox>("claimed_text");
	mTextClaimDate = getChild<LLTextBox>("date_claimed_text");
	mTextPriceLabel = getChild<LLTextBox>("price_label");
	mTextPrice = getChild<LLTextBox>("price_text");
	mTextDwell = getChild<LLTextBox>("dwell_text");

	mBtnBuyLand = getChild<LLButton>("buy_land_btn");
	mBtnBuyLand->setClickedCallback(onClickBuyLand, (void*)&BUY_PERSONAL_LAND);

	mBtnBuyGroupLand = getChild<LLButton>("buy_for_group_btn");
	mBtnBuyGroupLand->setClickedCallback(onClickBuyLand, (void*)&BUY_GROUP_LAND);

	mBtnBuyPass = getChild<LLButton>("buy_pass_btn");
	mBtnBuyPass->setClickedCallback(onClickBuyPass, this);

	mBtnReleaseLand = getChild<LLButton>("abandon_btn");
	mBtnReleaseLand->setClickedCallback(onClickRelease, NULL);

	mBtnReclaimLand = getChild<LLButton>("reclaim_btn");
	mBtnReclaimLand->setClickedCallback(onClickReclaim, NULL);

	mBtnStartAuction = getChild<LLButton>("sale_btn");
	mBtnStartAuction->setClickedCallback(onClickStartAuction, NULL);

	mAnyoneText = getString("anyone");

	return true;
}

//virtual
LLPanelLandGeneral::~LLPanelLandGeneral()
{
	// Release the selection handle
	mParcel = NULL;
}

//virtual
void LLPanelLandGeneral::refresh()
{
	mBtnStartAuction->setVisible(gAgent.isGodlike());

	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	bool region_owner = false;
	LLViewerRegion* regionp = gViewerParcelMgr.getSelectionRegion();
	if (regionp && regionp->getOwner() == gAgentID)
	{
		region_owner = true;
		mBtnReleaseLand->setVisible(false);
		mBtnReclaimLand->setVisible(true);
	}
	else
	{
		mBtnReleaseLand->setVisible(true);
		mBtnReclaimLand->setVisible(false);
	}
	if (!parcel)
	{
		// Nothing selected, disable panel
		mEditName->setEnabled(false);
		mEditName->setText(LLStringUtil::null);

		mEditDesc->setEnabled(false);
		mEditDesc->setText(getString("no_selection_text"));

		mTextSalePending->setText(LLStringUtil::null);
		mTextSalePending->setEnabled(false);

		mBtnDeedToGroup->setEnabled(false);
		mBtnSetGroup->setEnabled(false);
		mBtnStartAuction->setEnabled(false);

		mCheckDeedToGroup->set(false);
		mCheckDeedToGroup->setEnabled(false);
		mCheckContributeWithDeed->set(false);
		mCheckContributeWithDeed->setEnabled(false);

		mTextOwner->setText(LLStringUtil::null);
		mContentRating->setText(LLStringUtil::null);
		mLandType->setText(LLStringUtil::null);
		mBtnProfile->setLabel(getString("profile_text"));
		mBtnProfile->setEnabled(false);

		mTextClaimDate->setText(LLStringUtil::null);
		mTextGroup->setText(LLStringUtil::null);
		mTextPrice->setText(LLStringUtil::null);

		mSaleInfoForSale1->setVisible(false);
		mSaleInfoForSale2->setVisible(false);
		mSaleInfoForSaleObjects->setVisible(false);
		mSaleInfoForSaleNoObjects->setVisible(false);
		mSaleInfoNotForSale->setVisible(false);
		mBtnSellLand->setVisible(false);
		mBtnStopSellLand->setVisible(false);

		mTextPriceLabel->setText(LLStringUtil::null);
		mTextDwell->setText(LLStringUtil::null);

		mBtnBuyLand->setEnabled(false);
		mBtnBuyGroupLand->setEnabled(false);
		mBtnReleaseLand->setEnabled(false);
		mBtnReclaimLand->setEnabled(false);
		mBtnBuyPass->setEnabled(false);
		return;
	}

	// Something selected, hooray !
	bool is_leased = parcel->getOwnershipStatus() == LLParcel::OS_LEASED;
	bool region_xfer = false;
	if (regionp && !regionp->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL))
	{
		region_xfer = true;
	}

	if (regionp)
	{
		mContentRating->setText(regionp->getSimAccessString());
		mLandType->setText(regionp->getSimProductName());
	}

	// Estate owner/manager cannot edit other parts of the parcel
	bool estate_manager_sellable = !parcel->getAuctionID() &&
									gAgent.canManageEstate() && regionp &&
									// Estate manager/owner can only sell
									// parcels owned by estate owner
									parcel->getOwnerID() == regionp->getOwner();
	bool owner_sellable = region_xfer && !parcel->getAuctionID() &&
						  LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
																	   GP_LAND_SET_SALE_INFO);
	bool can_be_sold = owner_sellable || estate_manager_sellable;

	const LLUUID& owner_id = parcel->getOwnerID();
	bool is_public = parcel->isPublic();

	// Is it owned ?
	if (is_public)
	{
		mTextSalePending->setText(LLStringUtil::null);
		mTextSalePending->setEnabled(false);
		mTextOwner->setText(getString("public_text"));
		mTextOwner->setEnabled(false);
		mBtnProfile->setEnabled(false);
		mTextClaimDate->setText(LLStringUtil::null);
		mTextClaimDate->setEnabled(false);
		mTextGroup->setText(getString("none_text"));
		mTextGroup->setEnabled(false);
		mBtnStartAuction->setEnabled(false);
	}
	else
	{
		if (!is_leased && owner_id == gAgentID)
		{
			mTextSalePending->setText(getString("need_tier_to_modify"));
			mTextSalePending->setEnabled(true);
		}
		else if (parcel->getAuctionID())
		{
			mTextSalePending->setText(getString("auction_id_text"));
			mTextSalePending->setTextArg("[ID]",
										 llformat("%u",
												  parcel->getAuctionID()));
			mTextSalePending->setEnabled(true);
		}
		else
		{
			// Not the owner, or it is leased
			mTextSalePending->setText(LLStringUtil::null);
			mTextSalePending->setEnabled(false);
		}
		mTextOwner->setEnabled(true);

		// We support both group and personal profiles
		mBtnProfile->setEnabled(true);

		bool got_group = parcel->getGroupID().notNull();
		mTextGroup->setEnabled(got_group);
		if (!got_group)
		{
			mTextGroup->setText(getString("none_text"));
		}
		if (got_group && parcel->getIsGroupOwned())
		{
			// Group owned, so "Info"
			mBtnProfile->setLabel(getString("info_text"));
			mTextGroup->setEnabled(true);
		}
		else
		{
			// Not group owned, so "Profile"
			mBtnProfile->setLabel(getString("profile_text"));
		}

		std::string datestr = formatted_time(parcel->getClaimDate());
		mTextClaimDate->setText(datestr);
		mTextClaimDate->setEnabled(is_leased);

		bool enable_auction = gAgent.getGodLevel() >= GOD_LIAISON &&
							  owner_id == GOVERNOR_LINDEN_ID &&
							  parcel->getAuctionID() == 0;
		mBtnStartAuction->setEnabled(enable_auction);
	}

	// Display options
	bool can_edit_identity =
		LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
													 GP_LAND_CHANGE_IDENTITY);
	mEditName->setEnabled(can_edit_identity);
	mEditDesc->setEnabled(can_edit_identity);

	bool can_edit_agent_only =
		LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_NO_POWERS);
	mBtnSetGroup->setEnabled(can_edit_agent_only &&
							 !parcel->getIsGroupOwned());

	const LLUUID& group_id = parcel->getGroupID();

	// Can only allow deeding if you own it and it's got a group.
	bool enable_deed = owner_id == gAgentID && group_id.notNull() &&
					   gAgent.isInGroup(group_id);
	// You do not need special powers to allow your object to be deeded to the
	// group.
	mCheckDeedToGroup->setEnabled(enable_deed);
	mCheckDeedToGroup->set(parcel->getAllowDeedToGroup());
	mCheckContributeWithDeed->setEnabled(enable_deed &&
										 parcel->getAllowDeedToGroup());
	mCheckContributeWithDeed->set(parcel->getContributeWithDeed());

	// Actually doing the deeding requires you to have GP_LAND_DEED powers in
	// the group.
	bool can_deed = gAgent.hasPowerInGroup(group_id, GP_LAND_DEED);
	mBtnDeedToGroup->setEnabled(parcel->getAllowDeedToGroup() &&
								group_id.notNull() && can_deed &&
								!parcel->getIsGroupOwned());

	mEditName->setText(parcel->getName());
	mEditDesc->setText(parcel->getDesc());

	bool for_sale = parcel->getForSale();

	mBtnSellLand->setVisible(false);
	mBtnStopSellLand->setVisible(false);

	// Show pricing information
	S32 area, claim_price, rent_price;
	F32 dwell = DWELL_NAN;
	gViewerParcelMgr.getDisplayInfo(&area, &claim_price, &rent_price,
									&for_sale, &dwell);

	// Area
	LLUIString price = getString("area_size_text");
	price.setArg("[AREA]", llformat("%d", area));
	mTextPriceLabel->setText(getString("area_text"));
	mTextPrice->setText(price.getString());

	if (dwell == DWELL_NAN)
	{
		mTextDwell->setText(LLTrans::getString("LoadingData"));
	}
	else
	{
		mTextDwell->setText(llformat("%.0f", dwell));
	}

	if (for_sale)
	{
		mSaleInfoForSale1->setVisible(true);
		mSaleInfoForSale2->setVisible(true);
		if (parcel->getSellWithObjects())
		{
			mSaleInfoForSaleObjects->setVisible(true);
			mSaleInfoForSaleNoObjects->setVisible(false);
		}
		else
		{
			mSaleInfoForSaleObjects->setVisible(false);
			mSaleInfoForSaleNoObjects->setVisible(true);
		}
		mSaleInfoNotForSale->setVisible(false);

		F32 cost_per_sqm = 0.f;
		if (area > 0)
		{
			cost_per_sqm = (F32)parcel->getSalePrice() / (F32)area;
		}

		mSaleInfoForSale1->setTextArg("[PRICE]",
									  llformat("%d", parcel->getSalePrice()));
		mSaleInfoForSale1->setTextArg("[PRICE_PER_SQM]",
									  llformat("%.1f", cost_per_sqm));
		if (can_be_sold)
		{
			mBtnStopSellLand->setVisible(true);
		}
	}
	else
	{
		mSaleInfoForSale1->setVisible(false);
		mSaleInfoForSale2->setVisible(false);
		mSaleInfoForSaleObjects->setVisible(false);
		mSaleInfoForSaleNoObjects->setVisible(false);
		mSaleInfoNotForSale->setVisible(true);
		if (can_be_sold)
		{
			mBtnSellLand->setVisible(true);
		}
	}

	refreshNames();

	mBtnBuyLand->setEnabled(gViewerParcelMgr.canAgentBuyParcel(parcel, false));
	mBtnBuyGroupLand->setEnabled(gViewerParcelMgr.canAgentBuyParcel(parcel,
																	true));

	if (region_owner)
	{
		mBtnReclaimLand->setEnabled(!is_public &&
									parcel->getOwnerID() != gAgentID);
	}
	else
	{
		bool is_owner_release =
			LLViewerParcelMgr::isParcelOwnedByAgent(parcel, GP_LAND_RELEASE);
		bool is_manager_release = gAgent.canManageEstate() && regionp &&
								  parcel->getOwnerID() != regionp->getOwner();
		mBtnReleaseLand->setEnabled(is_owner_release || is_manager_release);
	}

	bool use_pass = parcel->getOwnerID() != gAgentID &&
					parcel->getParcelFlag(PF_USE_PASS_LIST) &&
					!gViewerParcelMgr.isCollisionBanned();
	mBtnBuyPass->setEnabled(use_pass);
}

void LLPanelLandGeneral::refreshNames()
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (!parcel)
	{
		mTextOwner->setText(LLStringUtil::null);
		return;
	}

	std::string owner;
	if (parcel->getIsGroupOwned())
	{
		owner = getString("group_owned_text");
	}
	else if (gCacheNamep)
	{
		// Figure out the owner's name
		gCacheNamep->getFullName(parcel->getOwnerID(), owner);
	}

	if (parcel->getOwnershipStatus() == LLParcel::OS_LEASE_PENDING)
	{
		owner += getString("sale_pending_text");
	}
	mTextOwner->setText(owner);

	std::string group;
	if (parcel->getGroupID().notNull() && gCacheNamep)
	{
		gCacheNamep->getGroupName(parcel->getGroupID(), group);
	}
	mTextGroup->setText(group);

	const LLUUID& auth_buyer_id = parcel->getAuthorizedBuyerID();
	if (auth_buyer_id.notNull())
	{
		std::string name;
		if (gCacheNamep)
		{
			gCacheNamep->getFullName(auth_buyer_id, name);
		}
		mSaleInfoForSale2->setTextArg("[BUYER]", name);
	}
	else
	{
		mSaleInfoForSale2->setTextArg("[BUYER]", mAnyoneText);
	}
}

//virtual
void LLPanelLandGeneral::draw()
{
	refreshNames();
	LLPanel::draw();
}

//static
void LLPanelLandGeneral::onClickSetGroup(void* userdata)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)userdata;
	if (!self || !gFloaterViewp) return;

	LLFloaterGroupPicker* fg = LLFloaterGroupPicker::show(cbGroupID, userdata);
	if (fg)
	{
		LLFloater* parentp = gFloaterViewp->getParentFloater(self);
		if (!parentp) return;

		LLRect new_rect = gFloaterViewp->findNeighboringPosition(parentp, fg);
		fg->setOrigin(new_rect.mLeft, new_rect.mBottom);
		parentp->addDependentFloater(fg);
	}
}

//static
void LLPanelLandGeneral::onClickProfile(void* data)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)data;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	if (parcel->getIsGroupOwned())
	{
		const LLUUID& group_id = parcel->getGroupID();
		LLFloaterGroupInfo::showFromUUID(group_id);
	}
	else
	{
		const LLUUID& avatar_id = parcel->getOwnerID();
		LLFloaterAvatarInfo::showFromObject(avatar_id);
	}
}

//static
void LLPanelLandGeneral::cbGroupID(LLUUID group_id, void* userdata)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)userdata;
	self->setGroup(group_id);
}

void LLPanelLandGeneral::setGroup(const LLUUID& group_id)
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (!parcel) return;

	// Set parcel properties and send message
	parcel->setGroupID(group_id);
#if 0
	parcel->setGroupName(group_name);
	mTextGroup->setText(group_name);
#endif

	// Send update
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);

	// Update UI
	refresh();
}

//static
void LLPanelLandGeneral::onClickBuyLand(void* data)
{
	bool* for_group = (bool*)data;
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk
	gViewerParcelMgr.startBuyLand(*for_group);
}

#if 0	// unused
bool LLPanelLandGeneral::enableDeedToGroup(void* data)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)data;
	if (!self || !self->mParcel) return false;
	LLParcel* parcel = self->mParcel->getParcel();
	return parcel && parcel->getParcelFlag(PF_ALLOW_DEED_TO_GROUP);
}
#endif

//static
void LLPanelLandGeneral::onClickDeed(void*)
{
	gViewerParcelMgr.startDeedLandToGroup();
}

//static
void LLPanelLandGeneral::onClickRelease(void*)
{
	gViewerParcelMgr.startReleaseLand();
}

//static
void LLPanelLandGeneral::onClickReclaim(void*)
{
	gViewerParcelMgr.reclaimParcel();
}

//static
bool LLPanelLandGeneral::enableBuyPass(void* data)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)data;
	if (!self) return false;

	LLParcel* parcel =
		self->mParcel ? self->mParcel->getParcel()
					  : gViewerParcelMgr.getParcelSelection()->getParcel();
	return parcel && parcel->getParcelFlag(PF_USE_PASS_LIST) &&
		   !gViewerParcelMgr.isCollisionBanned();
}

//static
void LLPanelLandGeneral::onClickBuyPass(void* data)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)data;
	if (!self) return;

	LLParcel* parcel =
		self->mParcel ? self->mParcel->getParcel()
					  : gViewerParcelMgr.getParcelSelection()->getParcel();
	if (!parcel) return;

	S32 pass_price = parcel->getPassPrice();
	std::string parcel_name = parcel->getName();
	F32 pass_hours = parcel->getPassHours();

	std::string cost, time;
	cost = llformat("%d", pass_price);
	time = llformat("%.2f", pass_hours);

	LLSD args;
	args["COST"] = cost;
	args["PARCEL_NAME"] = parcel_name;
	args["TIME"] = time;

	gNotifications.add("LandBuyPass", args, LLSD(), cbBuyPass);
}

//static
void LLPanelLandGeneral::onClickStartAuction(void* data)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)data;
	if (!self || !self->mParcel) return;

	LLParcel* parcelp = self->mParcel->getParcel();
	if (parcelp)
	{
		if (parcelp->getForSale())
		{
			gNotifications.add("CannotStartAuctionAlreadyForSale");
		}
		else
		{
			LLFloaterAuction::showInstance();
		}
	}
}

//static
bool LLPanelLandGeneral::cbBuyPass(const LLSD& notification,
								   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// User clicked OK
		gViewerParcelMgr.buyPass();
	}
	return false;
}

//static
void LLPanelLandGeneral::onCommitAny(LLUICtrl* ctrl, void* userdata)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)userdata;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	std::string name = self->mEditName->getText();
	std::string desc = self->mEditDesc->getText();

	// Valid data from UI

	// Stuff data into selected parcel
	parcel->setName(name);
	parcel->setDesc(desc);

	bool allow_deed_to_group = self->mCheckDeedToGroup->get();
	parcel->setParcelFlag(PF_ALLOW_DEED_TO_GROUP, allow_deed_to_group);

	bool contribute_with_deed = self->mCheckContributeWithDeed->get();
	parcel->setContributeWithDeed(contribute_with_deed);

	// Send update to server
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);

	// Might have changed properties, so let's redraw!
	self->refresh();
}

//static
void LLPanelLandGeneral::onClickSellLand(void* data)
{
	gViewerParcelMgr.startSellLand();
}

//static
void LLPanelLandGeneral::onClickStopSellLand(void* data)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)data;
	if (!self || !self->mParcel) return;
	LLParcel* parcel = self->mParcel->getParcel();

	parcel->setParcelFlag(PF_FOR_SALE, false);
	parcel->setSalePrice(0);
	parcel->setAuthorizedBuyerID(LLUUID::null);

	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);
}

//---------------------------------------------------------------------------
// LLPanelLandObjects
//---------------------------------------------------------------------------

LLPanelLandObjects::LLPanelLandObjects(LLParcelSelectionHandle& parcel)
:	LLPanel("land_objects_panel"),
	mParcel(parcel)
{
}

bool LLPanelLandObjects::postBuild()
{
	mFirstReply = true;

	mParcelObjectBonus = getChild<LLTextBox>("parcel_object_bonus");
	mSWTotalObjects = getChild<LLTextBox>("objects_available");
	mObjectContribution = getChild<LLTextBox>("object_contrib_text");
	mTotalObjects = getChild<LLTextBox>("total_objects_text");
	mOwnerObjects = getChild<LLTextBox>("owner_objects_text");

	mBtnShowOwnerObjects = getChild<LLButton>("show_owner_btn");
	mBtnShowOwnerObjects->setClickedCallback(onClickShowOwnerObjects, this);

	mBtnReturnOwnerObjects = getChild<LLButton>("return_owner_btn");
	mBtnReturnOwnerObjects->setClickedCallback(onClickReturnOwnerObjects,
											   this);

	mGroupObjects = getChild<LLTextBox>("group_objects_text");
	mBtnShowGroupObjects = getChild<LLButton>("show_group_btn");
	mBtnShowGroupObjects->setClickedCallback(onClickShowGroupObjects, this);

	mBtnReturnGroupObjects = getChild<LLButton>("return_group_btn");
	mBtnReturnGroupObjects->setClickedCallback(onClickReturnGroupObjects,
											   this);

	mOtherObjects = getChild<LLTextBox>("other_objects_text");
	mBtnShowOtherObjects = getChild<LLButton>("show_other_btn");
	mBtnShowOtherObjects->setClickedCallback(onClickShowOtherObjects, this);

	mBtnReturnOtherObjects = getChild<LLButton>("return_other_btn");
	mBtnReturnOtherObjects->setClickedCallback(onClickReturnOtherObjects,
											   this);

	mSelectedObjects = getChild<LLTextBox>("selected_objects_text");

	mCleanOtherObjectsTime = getChild<LLLineEditor>("auto_return_delay");
	mCleanOtherObjectsTime->setFocusLostCallback(onLostFocus, this);
	mCleanOtherObjectsTime->setCommitCallback(onCommitClean);
	mCleanOtherObjectsTime->setCallbackUserData(this);
	mCleanOtherObjectsTime->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);

	mBtnRefresh = getChild<LLButton>("refresh_btn");
	mBtnRefresh->setClickedCallback(onClickRefresh, this);

	mBtnReturnOwnerList = getChild<LLButton>("return_btn");
	mBtnReturnOwnerList->setClickedCallback(onClickReturnOwnerList, this);

	mIconAvatarOnline = LLUI::getUIImage("icon_avatar_online.tga");
	mIconAvatarOffline = LLUI::getUIImage("icon_avatar_offline.tga");
	mIconGroup = LLUI::getUIImage("icon_group.tga");

	mOwnerList = getChild<LLNameListCtrl>("owner_list");
	mOwnerList->sortByColumnIndex(3, false);
	mOwnerList->setCommitCallback(onCommitList);
	mOwnerList->setCallbackUserData(this);
	mOwnerList->setDoubleClickCallback(onDoubleClickOwner);

	return true;
}

//virtual
LLPanelLandObjects::~LLPanelLandObjects()
{
	// Release the selection handle
	mParcel = NULL;
}

//static
void LLPanelLandObjects::onDoubleClickOwner(void* userdata)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)userdata;

	LLScrollListItem* item = self->mOwnerList->getFirstSelected();
	if (item)
	{
		LLUUID owner_id = item->getUUID();
		// Look up the selected name, for future dialog box use.
		const LLScrollListCell* cell = item->getColumn(1);
		if (!cell)
		{
			return;
		}
		// Is this a group ?
		if (cell->getValue().asString() == OWNER_GROUP)
		{
			// Yes, show group info
			LLFloaterGroupInfo::showFromUUID(owner_id);
		}
		else
		{
			// No, show owner profile
			LLFloaterAvatarInfo::showFromDirectory(owner_id);
		}
	}
}

//virtual
void LLPanelLandObjects::refresh()
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;

	mBtnShowOwnerObjects->setEnabled(false);
	mBtnShowGroupObjects->setEnabled(false);
	mBtnShowOtherObjects->setEnabled(false);
	mBtnReturnOwnerObjects->setEnabled(false);
	mBtnReturnGroupObjects->setEnabled(false);
	mBtnReturnOtherObjects->setEnabled(false);
	mCleanOtherObjectsTime->setEnabled(false);
	mBtnRefresh->setEnabled(false);
	mBtnReturnOwnerList->setEnabled(false);

	mSelectedOwners.clear();
	mOwnerList->deleteAllItems();
	mOwnerList->setEnabled(false);

	if (!parcel)
	{
		mSWTotalObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mSWTotalObjects->setTextArg("[TOTAL]", llformat("%d", 0));
		mSWTotalObjects->setTextArg("[AVAILABLE]", llformat("%d", 0));
		mObjectContribution->setTextArg("[COUNT]", llformat("%d", 0));
		mTotalObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mOwnerObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mGroupObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mOtherObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mSelectedObjects->setTextArg("[COUNT]", llformat("%d", 0));
	}
	else
	{
		S32 sw_max = parcel->getSimWideMaxPrimCapacity();
		S32 sw_total = parcel->getSimWidePrimCount();
		S32 max = ll_roundp(parcel->getMaxPrimCapacity() *
							parcel->getParcelPrimBonus());
		S32 total = parcel->getPrimCount();
		S32 owned = parcel->getOwnerPrimCount();
		S32 group = parcel->getGroupPrimCount();
		S32 other = parcel->getOtherPrimCount();
		S32 selected = parcel->getSelectedPrimCount();
		F32 parcel_object_bonus = parcel->getParcelPrimBonus();
		mOtherTime = parcel->getCleanOtherTime();

		// Cannot have more than region max tasks, regardless of parcel object
		// bonus factor.
		LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
		if (region)
		{
			S32 max_tasks_per_region = (S32)region->getMaxTasks();
			sw_max = llmin(sw_max, max_tasks_per_region);
			max = llmin(max, max_tasks_per_region);
		}

		if (parcel_object_bonus != 1.f)
		{
			mParcelObjectBonus->setVisible(true);
			mParcelObjectBonus->setTextArg("[BONUS]",
										   llformat("%.2f",
													parcel_object_bonus));
		}
		else
		{
			mParcelObjectBonus->setVisible(false);
		}

		if (sw_total > sw_max)
		{
			mSWTotalObjects->setText(getString("objects_deleted_text"));
			mSWTotalObjects->setTextArg("[DELETED]",
										llformat("%d", sw_total - sw_max));
		}
		else
		{
			mSWTotalObjects->setText(getString("objects_available_text"));
			mSWTotalObjects->setTextArg("[AVAILABLE]",
										llformat("%d", sw_max - sw_total));
		}
		mSWTotalObjects->setTextArg("[COUNT]", llformat("%d", sw_total));
		mSWTotalObjects->setTextArg("[MAX]", llformat("%d", sw_max));

		mObjectContribution->setTextArg("[COUNT]", llformat("%d", max));
		mTotalObjects->setTextArg("[COUNT]", llformat("%d", total));
		mOwnerObjects->setTextArg("[COUNT]", llformat("%d", owned));
		mGroupObjects->setTextArg("[COUNT]", llformat("%d", group));
		mOtherObjects->setTextArg("[COUNT]", llformat("%d", other));
		mSelectedObjects->setTextArg("[COUNT]", llformat("%d", selected));
		mCleanOtherObjectsTime->setText(llformat("%d", mOtherTime));

		bool can_return_owned =
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														 GP_LAND_RETURN_GROUP_OWNED);
		bool can_return_group_set =
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														 GP_LAND_RETURN_GROUP_SET);
		bool can_return_other =
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														 GP_LAND_RETURN_NON_GROUP);
		if (can_return_owned || can_return_group_set || can_return_other)
		{
			if (owned && can_return_owned)
			{
				mBtnShowOwnerObjects->setEnabled(true);
				mBtnReturnOwnerObjects->setEnabled(true);
			}
			if (group && can_return_group_set)
			{
				mBtnShowGroupObjects->setEnabled(true);
				mBtnReturnGroupObjects->setEnabled(true);
			}
			if (other && can_return_other)
			{
				mBtnShowOtherObjects->setEnabled(true);
				mBtnReturnOtherObjects->setEnabled(true);
			}

			mCleanOtherObjectsTime->setEnabled(true);
			mBtnRefresh->setEnabled(true);
		}
	}
}

void send_other_clean_time_message(S32 parcel_local_id, S32 other_clean_time)
{
	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	if (!region) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelSetOtherCleanTime);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addS32Fast(_PREHASH_OtherCleanTime, other_clean_time);

	msg->sendReliable(region->getHost());
}

void send_return_objects_message(S32 parcel_local_id, S32 return_type,
								 owners_list_t* owner_ids = NULL)
{
	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	if (!region) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelReturnObjects);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addU32Fast(_PREHASH_ReturnType, (U32) return_type);

	// Dummy task id, not used
	msg->nextBlock("TaskIDs");
	msg->addUUID("TaskID", LLUUID::null);

	// Throw all return ids into the packet. *TODO: Check for too many Ids.
	if (owner_ids)
	{
		for (owners_list_t::iterator it = owner_ids->begin(),
									 end = owner_ids->end();
			 it != end; ++it)
		{
			msg->nextBlockFast(_PREHASH_OwnerIDs);
			msg->addUUIDFast(_PREHASH_OwnerID, (*it));
		}
	}
	else
	{
		msg->nextBlockFast(_PREHASH_OwnerIDs);
		msg->addUUIDFast(_PREHASH_OwnerID, LLUUID::null);
	}

	msg->sendReliable(region->getHost());
}

bool LLPanelLandObjects::callbackReturnOwnerObjects(const LLSD& notification,
													const LLSD& response)
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (parcel &&
		LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID owner_id = parcel->getOwnerID();
		LLSD args;
		if (owner_id == gAgentID)
		{
			gNotifications.add("OwnedObjectsReturned");
		}
		else
		{
			std::string name;
			if (gCacheNamep)
			{
				gCacheNamep->getFullName(owner_id, name);
			}
			args["NAME"] = name;
			gNotifications.add("OtherObjectsReturned", args);
		}
		send_return_objects_message(parcel->getLocalID(), RT_OWNER);
	}

	gSelectMgr.unhighlightAll();
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);
	refresh();

	return false;
}

bool LLPanelLandObjects::callbackReturnGroupObjects(const LLSD& notification,
													const LLSD& response)
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (parcel &&
		LLNotification::getSelectedOption(notification, response) == 0)
	{
		std::string group_name;
		if (gCacheNamep)
		{
			gCacheNamep->getGroupName(parcel->getGroupID(), group_name);
		}
		LLSD args;
		args["GROUPNAME"] = group_name;
		gNotifications.add("GroupObjectsReturned", args);
		send_return_objects_message(parcel->getLocalID(), RT_GROUP);
	}

	gSelectMgr.unhighlightAll();
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);
	refresh();

	return false;
}

bool LLPanelLandObjects::callbackReturnOtherObjects(const LLSD& notification,
													const LLSD& response)
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (parcel &&
		LLNotification::getSelectedOption(notification, response) == 0)
	{
		gNotifications.add("UnOwnedObjectsReturned");
		send_return_objects_message(parcel->getLocalID(), RT_OTHER);
	}

	gSelectMgr.unhighlightAll();
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);
	refresh();

	return false;
}

bool LLPanelLandObjects::callbackReturnOwnerList(const LLSD& notification,
												 const LLSD& response)
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (parcel &&
		LLNotification::getSelectedOption(notification, response) == 0)
	{
		// Make sure we have something selected.
		owners_list_t::iterator selected = mSelectedOwners.begin();
		if (selected != mSelectedOwners.end())
		{
			LLSD args;
			if (mSelectedIsGroup)
			{
				args["GROUPNAME"] = mSelectedName;
				gNotifications.add("GroupObjectsReturned", args);
			}
			else
			{
				args["NAME"] =
					LLAvatarName::sOmitResidentAsLastName ? LLCacheName::cleanFullName(mSelectedName)
														  : mSelectedName;
				gNotifications.add("OtherObjectsReturned2", args);
			}

			send_return_objects_message(parcel->getLocalID(), RT_LIST,
										&(mSelectedOwners));
		}
	}

	gSelectMgr.unhighlightAll();
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);
	refresh();

	return false;
}

//static
void LLPanelLandObjects::onClickReturnOwnerList(void* userdata)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)userdata;
	if (!self || !self->mParcel) return;

	LLParcel* parcelp = self->mParcel->getParcel();
	if (!parcelp) return;

	// Make sure we have something selected.
	if (self->mSelectedOwners.empty())
	{
		return;
	}

#if 0
	owners_list_t::iterator it = self->mSelectedOwners.begin();
	if (it == self->mSelectedOwners.end()) return;
#endif

	send_parcel_select_objects(parcelp->getLocalID(), RT_LIST,
							   &(self->mSelectedOwners));

	LLSD args;
	args["NAME"] = self->mSelectedName;
	args["N"] = llformat("%d",self->mSelectedCount);
	if (self->mSelectedIsGroup)
	{
		gNotifications.add("ReturnObjectsDeededToGroup", args, LLSD(),
						   boost::bind(&LLPanelLandObjects::callbackReturnOwnerList,
									   self, _1, _2));
	}
	else
	{
		gNotifications.add("ReturnObjectsOwnedByUser", args, LLSD(),
						   boost::bind(&LLPanelLandObjects::callbackReturnOwnerList,
									   self, _1, _2));
	}
}

//static
void LLPanelLandObjects::onClickRefresh(void* userdata)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)userdata;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	if (!region) return;

	// Ready the list for results
	self->mOwnerList->deleteAllItems();
	self->mOwnerList->addCommentText("Searching..."); // *TODO: Translate
	self->mOwnerList->setEnabled(false);
	self->mFirstReply = true;

	// Send the message
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelObjectOwnersRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel->getLocalID());

	msg->sendReliable(region->getHost());
}

//static
void LLPanelLandObjects::processParcelObjectOwnersReply(LLMessageSystem* msg,
														void**)
{
	LLPanelLandObjects* self = LLFloaterLand::getCurrentPanelLandObjects();
	if (!self)
	{
		llwarns << "Received message for nonexistent LLPanelLandObject"
				<< llendl;
		return;
	}

	static const LLFontGL* FONT = LLFontGL::getFontSansSerif();

	// Extract all of the owners.
	S32 rows = msg->getNumberOfBlocksFast(_PREHASH_Data);
	LLUUID owner_id;
	bool is_group_owned;
	S32 object_count;
	U32 most_recent_time = 0;
	bool is_online;
	std::string object_count_str;

	// If we were waiting for the first reply, clear the "Searching..." text.
	if (self->mFirstReply)
	{
		self->mOwnerList->deleteAllItems();
		self->mFirstReply = false;
	}

	for (S32 i = 0; i < rows; ++i)
	{
		msg->getUUIDFast(_PREHASH_Data, _PREHASH_OwnerID, owner_id, i);
		msg->getBoolFast(_PREHASH_Data, _PREHASH_IsGroupOwned,
						 is_group_owned, i);
		msg->getS32Fast(_PREHASH_Data, _PREHASH_Count, object_count, i);
		msg->getBoolFast(_PREHASH_Data, _PREHASH_OnlineStatus, is_online, i);
		if (msg->has("DataExtended"))
		{
			msg->getU32("DataExtended", "TimeStamp", most_recent_time, i);
		}
		if (owner_id.isNull())
		{
			continue;
		}

		LLScrollListItem* row = new LLScrollListItem(true, NULL, owner_id);
		if (is_group_owned)
		{
			row->addColumn(self->mIconGroup);
			row->addColumn(OWNER_GROUP, FONT);
		}
		else if (is_online)
		{
			row->addColumn(self->mIconAvatarOnline);
			row->addColumn(OWNER_ONLINE, FONT);
		}
		else  // Offline
		{
			row->addColumn(self->mIconAvatarOffline);
			row->addColumn(OWNER_OFFLINE, FONT);
		}
		// Placeholder for name.
		row->addColumn(LLStringUtil::null, FONT);

		object_count_str = llformat("%d", object_count);
		row->addColumn(object_count_str, FONT);

		row->addColumn(formatted_time((time_t)most_recent_time), FONT);

		if (is_group_owned)
		{
			self->mOwnerList->addGroupNameItem(row, ADD_BOTTOM);
		}
		else
		{
			self->mOwnerList->addNameItem(row, ADD_BOTTOM);
		}

		LL_DEBUGS("ParcelObject") << "Object owner " << owner_id << " ("
								  << (is_group_owned ? "group" : "agent")
								  << ") owns " << object_count << " objects."
								  << LL_ENDL;
	}
	// Check for no results
	if (self->mOwnerList->getItemCount() == 0)
	{
		// *TODO: translate
		self->mOwnerList->addCommentText("None found.");
	}
	else
	{
		self->mOwnerList->setEnabled(true);
	}
}

//static
void LLPanelLandObjects::onCommitList(LLUICtrl* ctrl, void* data)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)data;
	if (!self || !self->mOwnerList->getCanSelect())
	{
		return;
	}

	LLScrollListItem* item = self->mOwnerList->getFirstSelected();
	if (item)
	{
		// Look up the selected name, for future dialog box use.
		const LLScrollListCell* cell = item->getColumn(1);
		if (!cell)
		{
			return;
		}
		// Is this a group ?
		self->mSelectedIsGroup = cell->getValue().asString() == OWNER_GROUP;
		cell = item->getColumn(2);
		self->mSelectedName = cell->getValue().asString();
		cell = item->getColumn(3);
		self->mSelectedCount = atoi(cell->getValue().asString().c_str());

		// Set the selection, and enable the return button.
		self->mSelectedOwners.clear();
		self->mSelectedOwners.insert(item->getUUID());
		self->mBtnReturnOwnerList->setEnabled(true);

		// Highlight this user's objects
		clickShowCore(self, RT_LIST, &(self->mSelectedOwners));
	}
}

//static
void LLPanelLandObjects::clickShowCore(LLPanelLandObjects* self,
									   S32 return_type,
									   owners_list_t* list)
{
	if (!self || !self->mParcel) return;
	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), return_type, list);
}

//static
void LLPanelLandObjects::onClickShowOwnerObjects(void* userdata)
{
	clickShowCore((LLPanelLandObjects*)userdata, RT_OWNER);
}

//static
void LLPanelLandObjects::onClickShowGroupObjects(void* userdata)
{
	clickShowCore((LLPanelLandObjects*)userdata, (RT_GROUP));
}

//static
void LLPanelLandObjects::onClickShowOtherObjects(void* userdata)
{
	clickShowCore((LLPanelLandObjects*)userdata, RT_OTHER);
}

//static
void LLPanelLandObjects::onClickReturnOwnerObjects(void* userdata)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)userdata;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), RT_OWNER);

	LLSD args;
	args["N"] = llformat("%d", parcel->getOwnerPrimCount());

	const LLUUID& owner_id = parcel->getOwnerID();
	if (owner_id == gAgentID)
	{
		gNotifications.add("ReturnObjectsOwnedBySelf", args, LLSD(),
						   boost::bind(&LLPanelLandObjects::callbackReturnOwnerObjects,
									   self, _1, _2));
	}
	else
	{
		std::string name;
		if (gCacheNamep)
		{
			gCacheNamep->getFullName(owner_id, name);
		}
		args["NAME"] = name;
		gNotifications.add("ReturnObjectsOwnedByUser", args, LLSD(),
						   boost::bind(&LLPanelLandObjects::callbackReturnOwnerObjects,
									   self, _1, _2));
	}
}

//static
void LLPanelLandObjects::onClickReturnGroupObjects(void* userdata)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)userdata;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), RT_GROUP);

	std::string group_name;
	if (gCacheNamep)
	{
		gCacheNamep->getGroupName(parcel->getGroupID(), group_name);
	}

	LLSD args;
	args["NAME"] = group_name;
	args["N"] = llformat("%d", parcel->getGroupPrimCount());

	// Create and show confirmation text box
	gNotifications.add("ReturnObjectsDeededToGroup", args, LLSD(),
					   boost::bind(&LLPanelLandObjects::callbackReturnGroupObjects,
								   self, _1, _2));
}

//static
void LLPanelLandObjects::onClickReturnOtherObjects(void* userdata)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)userdata;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), RT_OTHER);

	LLSD args;
	args["N"] = llformat("%d", parcel->getOtherPrimCount());

	if (parcel->getIsGroupOwned())
	{
		std::string group_name;
		if (gCacheNamep)
		{
			gCacheNamep->getGroupName(parcel->getGroupID(), group_name);
		}
		args["NAME"] = group_name;

		gNotifications.add("ReturnObjectsNotOwnedByGroup", args, LLSD(),
						   boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects,
									   self, _1, _2));
		return;
	}

	const LLUUID& owner_id = parcel->getOwnerID();
	if (owner_id == gAgentID)
	{
		gNotifications.add("ReturnObjectsNotOwnedBySelf", args, LLSD(),
						   boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects,
									   self, _1, _2));
		return;
	}

	std::string name;
	if (gCacheNamep)
	{
		gCacheNamep->getFullName(owner_id, name);
	}
	args["NAME"] = name;
	gNotifications.add("ReturnObjectsNotOwnedByUser", args, LLSD(),
					    boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects,
									self, _1, _2));
}

//static
void LLPanelLandObjects::onLostFocus(LLFocusableElement* caller, void* user_data)
{
	onCommitClean((LLUICtrl*)caller, user_data);
}

//static
void LLPanelLandObjects::onCommitClean(LLUICtrl* caller, void* user_data)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)user_data;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (parcel)
	{
		self->mOtherTime = atoi(self->mCleanOtherObjectsTime->getText().c_str());

		parcel->setCleanOtherTime(self->mOtherTime);
		send_other_clean_time_message(parcel->getLocalID(), self->mOtherTime);
	}
}

//---------------------------------------------------------------------------
// LLPanelLandOptions
//---------------------------------------------------------------------------

LLPanelLandOptions::LLPanelLandOptions(LLParcelSelectionHandle& parcel)
:	LLPanel("land_options_panel"),
	mParcel(parcel)
{
}

bool LLPanelLandOptions::postBuild()
{
	mCreateObjectsCheck = getChild<LLCheckBoxCtrl>("create_obj_check");
	mCreateObjectsCheck->setCommitCallback(onCommitAny);
	mCreateObjectsCheck->setCallbackUserData(this);

	mCreateGrpObjectsCheck = getChild<LLCheckBoxCtrl>("edit_grp_obj_check");
	mCreateGrpObjectsCheck->setCommitCallback(onCommitAny);
	mCreateGrpObjectsCheck->setCallbackUserData(this);

	mAllObjectEntryCheck = getChild<LLCheckBoxCtrl>("all_entry_check");
	mAllObjectEntryCheck->setCommitCallback(onCommitAny);
	mAllObjectEntryCheck->setCallbackUserData(this);

	mGroupObjectEntryCheck = getChild<LLCheckBoxCtrl>("group_entry_check");
	mGroupObjectEntryCheck->setCommitCallback(onCommitAny);
	mGroupObjectEntryCheck->setCallbackUserData(this);

	mEditLandCheck = getChild<LLCheckBoxCtrl>("edit_land_check");
	mEditLandCheck->setCommitCallback(onCommitAny);
	mEditLandCheck->setCallbackUserData(this);

	mAllScriptsCheck = getChild<LLCheckBoxCtrl>("allow_scripts_check");
	mAllScriptsCheck->setCommitCallback(onCommitAny);
	mAllScriptsCheck->setCallbackUserData(this);

	mGroupScriptsCheck = getChild<LLCheckBoxCtrl>("group_scripts_check");
	mGroupScriptsCheck->setCommitCallback(onCommitAny);
	mGroupScriptsCheck->setCallbackUserData(this);

	mCanFlyCheck = getChild<LLCheckBoxCtrl>("fly_check");
	mCanFlyCheck->setCommitCallback(onCommitAny);
	mCanFlyCheck->setCallbackUserData(this);

	mNoDamageCheck = getChild<LLCheckBoxCtrl>("no_damage_check");
	mNoDamageCheck->setCommitCallback(onCommitAny);
	mNoDamageCheck->setCallbackUserData(this);

	mPushRestrictionCheck = getChild<LLCheckBoxCtrl>("restrict_push_check");
	mPushRestrictionCheck->setCommitCallback(onCommitAny);
	mPushRestrictionCheck->setCallbackUserData(this);

	mShowDirectoryCheck = getChild<LLCheckBoxCtrl>("show_directory_check");
	mShowDirectoryCheck->setCommitCallback(onCommitAny);
	mShowDirectoryCheck->setCallbackUserData(this);

	mCategoryCombo = getChild<LLComboBox>("land_category_combo");
	mCategoryCombo->setCommitCallback(onCommitAny);
	mCategoryCombo->setCallbackUserData(this);
	mCategoryCombo->setVisible(true);
	mCategoryCombo->setEnabled(true);

	mPublishHelpButton = getChild<LLButton>("help_btn");
	mPublishHelpButton->setClickedCallback(onClickPublishHelp, this);

	mMatureCheck = getChild<LLCheckBoxCtrl>("mature_check");
	mMatureCheck->setCommitCallback(onCommitAny);
	mMatureCheck->setCallbackUserData(this);

	mPrivacyCheck = getChild<LLCheckBoxCtrl>("privacy_check");
	mPrivacyCheck->setCommitCallback(onCommitAny);
	mPrivacyCheck->setCallbackUserData(this);

	if (gAgent.wantsPGOnly())
	{
		// Disable these buttons if they are PG (Teen) users
		mPublishHelpButton->setVisible(false);
		mPublishHelpButton->setEnabled(false);
		mMatureCheck->setVisible(false);
		mMatureCheck->setEnabled(false);
	}

	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	mSnapshotCtrl->setCommitCallback(onCommitAny);
	mSnapshotCtrl->setCallbackUserData(this);
	mSnapshotCtrl->setAllowNoTexture(true);
	mSnapshotCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
	mSnapshotCtrl->setNonImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
	mSnapshotCtrl->setFallbackImageName("default_land_picture.j2c");

	mLocationText = getChild<LLTextBox>("landing_point");

	mSetBtn = getChild<LLButton>("set_landing_btn");
	mSetBtn->setClickedCallback(onClickSet, this);

	mClearBtn = getChild<LLButton>("clear_landing_btn");
	mClearBtn->setClickedCallback(onClickClear, this);

	mTeleportRoutingCombo = getChild<LLComboBox>("teleport_routing_combo");
	mTeleportRoutingCombo->setCommitCallback(onCommitAny);
	mTeleportRoutingCombo->setCallbackUserData(this);

	return true;
}

//virtual
LLPanelLandOptions::~LLPanelLandOptions()
{
	// Release the selection handle
	mParcel = NULL;
}

//virtual
void LLPanelLandOptions::refresh()
{
	refreshSearch();

	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (!parcel)
	{
		mCreateObjectsCheck->set(false);
		mCreateObjectsCheck->setEnabled(false);

		mCreateGrpObjectsCheck->set(false);
		mCreateGrpObjectsCheck->setEnabled(false);

		mAllObjectEntryCheck->set(false);
		mAllObjectEntryCheck->setEnabled(false);

		mGroupObjectEntryCheck->set(false);
		mGroupObjectEntryCheck->setEnabled(false);

		mEditLandCheck->set(false);
		mEditLandCheck->setEnabled(false);

		mNoDamageCheck->set(false);
		mNoDamageCheck->setEnabled(false);

		mCanFlyCheck->set(false);
		mCanFlyCheck->setEnabled(false);

		mGroupScriptsCheck->set(false);
		mGroupScriptsCheck->setEnabled(false);

		mAllScriptsCheck->set(false);
		mAllScriptsCheck->setEnabled(false);

		mPushRestrictionCheck->set(false);
		mPushRestrictionCheck->setEnabled(false);

		mPrivacyCheck->set(true);
		mPrivacyCheck->setEnabled(false);

		mTeleportRoutingCombo->setCurrentByIndex(0);
		mTeleportRoutingCombo->setEnabled(false);

		mSnapshotCtrl->setImageAssetID(LLUUID::null);
		mSnapshotCtrl->setEnabled(false);

		mLocationText->setTextArg("[LANDING]",
								  getString("landing_point_none"));
		mSetBtn->setEnabled(false);
		mClearBtn->setEnabled(false);

		mMatureCheck->setEnabled(false);
		mPublishHelpButton->setEnabled(false);
		return;
	}

	// Display options
	bool can_change_options =
		LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_OPTIONS);

	mCreateObjectsCheck->set(parcel->getAllowModify());
	mCreateObjectsCheck->setEnabled(can_change_options);

	mCreateGrpObjectsCheck->set(parcel->getAllowGroupModify() ||
								parcel->getAllowModify());
	// If others edit is enabled, then this is explicitly enabled:
	mCreateGrpObjectsCheck->setEnabled(can_change_options &&
									   !parcel->getAllowModify());

	mAllObjectEntryCheck->set(parcel->getAllowAllObjectEntry());
	mAllObjectEntryCheck->setEnabled(can_change_options);

	mGroupObjectEntryCheck->set(parcel->getAllowGroupObjectEntry() ||
								parcel->getAllowAllObjectEntry());
	mGroupObjectEntryCheck->setEnabled(can_change_options &&
									   !parcel->getAllowAllObjectEntry());

	mEditLandCheck->set(parcel->getAllowTerraform());
	mEditLandCheck->setEnabled(LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
																			GP_LAND_EDIT));

	mNoDamageCheck->set(!parcel->getAllowDamage());
	mNoDamageCheck->setEnabled(can_change_options);

	mCanFlyCheck->set(parcel->getAllowFly());
	mCanFlyCheck->setEnabled(can_change_options);

	mGroupScriptsCheck->set(parcel->getAllowGroupScripts() ||
							parcel->getAllowOtherScripts());
	mGroupScriptsCheck->setEnabled(can_change_options &&
								   !parcel->getAllowOtherScripts());

	mAllScriptsCheck->set(parcel->getAllowOtherScripts());
	mAllScriptsCheck->setEnabled(can_change_options);

	mPushRestrictionCheck->set(parcel->getRestrictPushObject());
	if (parcel->getRegionPushOverride())
	{
		mPushRestrictionCheck->setLabel(getString("push_restrict_region_text"));
		mPushRestrictionCheck->setEnabled(false);
		mPushRestrictionCheck->set(true);
	}
	else
	{
		mPushRestrictionCheck->setLabel(getString("push_restrict_text"));
		mPushRestrictionCheck->setEnabled(can_change_options);
	}

	mPrivacyCheck->set(parcel->getSeeAVs() ||
					   !parcel->getHaveNewParcelLimitData());
	mPrivacyCheck->setEnabled(can_change_options &&
							  parcel->getHaveNewParcelLimitData());

	bool can_change_landing =
		LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
													 GP_LAND_SET_LANDING_POINT);
	mTeleportRoutingCombo->setCurrentByIndex((S32)parcel->getLandingType());
	mTeleportRoutingCombo->setEnabled(can_change_landing);

	bool can_change_identity =
		LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
													 GP_LAND_CHANGE_IDENTITY);
	mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
	mSnapshotCtrl->setEnabled(can_change_identity);

	LLVector3 pos = parcel->getUserLocation();
	if (pos.isExactlyZero())
	{
		mLocationText->setTextArg("[LANDING]",
								  getString("landing_point_none"));
	}
	else
	{
		mLocationText->setTextArg("[LANDING]",
								  llformat("%d, %d, %d",
										   ll_roundp(pos.mV[VX]),
										   ll_roundp(pos.mV[VY]),
										   ll_roundp(pos.mV[VZ])));
	}

	mSetBtn->setEnabled(can_change_landing);
	mClearBtn->setEnabled(can_change_landing);

	mPublishHelpButton->setEnabled(can_change_identity);

	if (gAgent.wantsPGOnly())
	{
		// Disable these buttons if they are PG (Teen) users
		mPublishHelpButton->setVisible(false);
		mPublishHelpButton->setEnabled(false);
		mMatureCheck->setVisible(false);
		mMatureCheck->setEnabled(false);
		return;
	}

	// Not teen so fill in the data for the maturity control
	mMatureCheck->setVisible(true);
	mMatureCheck->setLabel(getString("mature_check_mature"));
	mMatureCheck->setToolTip(getString("mature_check_mature_tooltip"));

	// They can see the checkbox, but its disposition depends on the state of
	// the region
	LLViewerRegion* regionp = gViewerParcelMgr.getSelectionRegion();
	if (regionp)
	{
		U8 access = regionp->getSimAccess();
		if (access == SIM_ACCESS_PG)
		{
			mMatureCheck->setEnabled(false);
			mMatureCheck->set(false);
		}
		else if (access == SIM_ACCESS_MATURE)
		{
			mMatureCheck->setEnabled(can_change_identity);
			mMatureCheck->set(parcel->getMaturePublish());
		}
		else if (access == SIM_ACCESS_ADULT)
		{
			mMatureCheck->setEnabled(false);
			mMatureCheck->set(true);
			mMatureCheck->setLabel(getString("mature_check_adult"));
			mMatureCheck->setToolTip(getString("mature_check_adult_tooltip"));
		}
	}
}

//virtual
void LLPanelLandOptions::draw()
{
	static F32 last_update = 0.f;
	if (gFrameTimeSeconds - last_update > 2.f) // One update every 2 seconds
	{
		refreshSearch();	// Is this necessary ? JC
		last_update = gFrameTimeSeconds;
	}
	LLPanel::draw();
}

void LLPanelLandOptions::refreshSearch()
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (!parcel)
	{
		mShowDirectoryCheck->set(false);
		mShowDirectoryCheck->setEnabled(false);

		// *TODO:Translate
		const std::string& none_string =
			LLParcel::getCategoryUIString(LLParcel::C_NONE);
		mCategoryCombo->setSimple(none_string);
		mCategoryCombo->setEnabled(false);
		return;
	}

	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	bool can_change =
		LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
													 GP_LAND_FIND_PLACES) &&
		region && !region->getRegionFlag(REGION_FLAGS_BLOCK_PARCEL_SEARCH);

	bool show_directory = parcel->getParcelFlag(PF_SHOW_DIRECTORY);
	mShowDirectoryCheck->set(show_directory);

	// Set by string in case the order in UI doesn't match the order by index.
	// *TODO:Translate
	LLParcel::ECategory cat = parcel->getCategory();
	const std::string& category_string = LLParcel::getCategoryUIString(cat);
	mCategoryCombo->setSimple(category_string);

	std::string tooltip;
	bool enable_show_directory = false;
	// Parcels <= 128 square meters cannot be listed in search, in an effort to
	// reduce search spam from small parcels. JC
	constexpr S32 MIN_PARCEL_AREA_FOR_SEARCH = 128;
	bool large_enough = parcel->getArea() > MIN_PARCEL_AREA_FOR_SEARCH;
	if (large_enough)
	{
		if (can_change)
		{
			tooltip = getString("search_enabled_tooltip");
			enable_show_directory = true;
		}
		else
		{
			tooltip = getString("search_disabled_permissions_tooltip");
			enable_show_directory = false;
		}
	}
	// Not large enough to include in search
	else if (can_change)
	{
		if (show_directory)
		{
			// Parcels that are too small, but are still in search for
			// legacy reasons, need to have the check box enabled so the
			// owner can delist the parcel. JC
			tooltip = getString("search_enabled_tooltip");
			enable_show_directory = true;
		}
		else
		{
			tooltip = getString("search_disabled_small_tooltip");
			enable_show_directory = false;
		}
	}
	else
	{
		// JC - Both too small and do not have permission, so just show the
		// permissions as the reason (which is probably the more common case).
		tooltip = getString("search_disabled_permissions_tooltip");
		enable_show_directory = false;
	}
	mShowDirectoryCheck->setToolTip(tooltip);
	mCategoryCombo->setToolTip(tooltip);
	mShowDirectoryCheck->setEnabled(enable_show_directory);
	mCategoryCombo->setEnabled(enable_show_directory);
}

//static
void LLPanelLandOptions::onCommitAny(LLUICtrl* ctrl, void* userdata)
{
	LLPanelLandOptions* self = (LLPanelLandOptions*)userdata;
	if (!self || !self->mParcel) return;
	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	bool create_objects = self->mCreateObjectsCheck->get();
	bool create_group_objects = self->mCreateGrpObjectsCheck->get() ||
								self->mCreateObjectsCheck->get();
	bool all_object_entry = self->mAllObjectEntryCheck->get();
	bool group_object_entry = self->mGroupObjectEntryCheck->get() ||
							  self->mAllObjectEntryCheck->get();
	bool allow_terraform = self->mEditLandCheck->get();
	bool allow_damage = !self->mNoDamageCheck->get();
	bool allow_fly = self->mCanFlyCheck->get();
	bool allow_group_scripts = self->mGroupScriptsCheck->get() ||
							   self->mAllScriptsCheck->get();
	bool allow_other_scripts = self->mAllScriptsCheck->get();
	bool allow_publish = false;
	bool mature_publish = self->mMatureCheck->get();
	bool push_restriction = self->mPushRestrictionCheck->get();
	bool see_avs = self->mPrivacyCheck->get();
	bool show_directory = self->mShowDirectoryCheck->get();
	// We have to get the index from a lookup, not from the position in the
	// drop down !
	S32 category_index =
		LLParcel::getCategoryFromString(self->mCategoryCombo->getSelectedValue());
	S32 landing_type_index = self->mTeleportRoutingCombo->getCurrentIndex();
	const LLUUID& snapshot_id = self->mSnapshotCtrl->getImageAssetID();

	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	if (!allow_other_scripts && region && region->getAllowDamage())
	{
		gNotifications.add("UnableToDisableOutsideScripts");
		return;
	}

	// Push data into current parcel
	parcel->setParcelFlag(PF_CREATE_OBJECTS, create_objects);
	parcel->setParcelFlag(PF_CREATE_GROUP_OBJECTS, create_group_objects);
	parcel->setParcelFlag(PF_ALLOW_ALL_OBJECT_ENTRY, all_object_entry);
	parcel->setParcelFlag(PF_ALLOW_GROUP_OBJECT_ENTRY, group_object_entry);
	parcel->setParcelFlag(PF_ALLOW_TERRAFORM, allow_terraform);
	parcel->setParcelFlag(PF_ALLOW_DAMAGE, allow_damage);
	parcel->setParcelFlag(PF_ALLOW_FLY, allow_fly);
	// Cannot restrict landmark creation:
	parcel->setParcelFlag(PF_ALLOW_LANDMARK, true);
	parcel->setParcelFlag(PF_ALLOW_GROUP_SCRIPTS, allow_group_scripts);
	parcel->setParcelFlag(PF_ALLOW_OTHER_SCRIPTS, allow_other_scripts);
	parcel->setParcelFlag(PF_SHOW_DIRECTORY, show_directory);
	parcel->setParcelFlag(PF_ALLOW_PUBLISH, allow_publish);
	parcel->setParcelFlag(PF_MATURE_PUBLISH, mature_publish);
	parcel->setParcelFlag(PF_RESTRICT_PUSHOBJECT, push_restriction);
	parcel->setCategory((LLParcel::ECategory)category_index);
	parcel->setLandingType((LLParcel::ELandingType)landing_type_index);
	parcel->setSnapshotID(snapshot_id);
	parcel->setSeeAVs(see_avs);

	// Send current parcel data upstream to server
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);

	// Might have changed properties, so let's redraw !
	self->refresh();
}

//static
void LLPanelLandOptions::onClickSet(void* userdata)
{
	LLPanelLandOptions* self = (LLPanelLandOptions*)userdata;
	if (!self || !self->mParcel) return;
	LLParcel* selected_parcel = self->mParcel->getParcel();
	if (!selected_parcel) return;

	LLParcel* agent_parcel = gViewerParcelMgr.getAgentParcel();
	if (!agent_parcel) return;

	if (agent_parcel->getLocalID() != selected_parcel->getLocalID())
	{
		gNotifications.add("MustBeInParcel");
		return;
	}

	LLVector3 pos_region = gAgent.getPositionAgent();
	selected_parcel->setUserLocation(pos_region);
	selected_parcel->setUserLookAt(gAgent.getFrameAgent().getAtAxis());

	gViewerParcelMgr.sendParcelPropertiesUpdate(selected_parcel);

	self->refresh();
}

void LLPanelLandOptions::onClickClear(void* userdata)
{
	LLPanelLandOptions* self = (LLPanelLandOptions*)userdata;
	if (!self || !self->mParcel) return;
	LLParcel* selected_parcel = self->mParcel->getParcel();
	if (!selected_parcel) return;

	// Yes, this magic number of 0,0,0 means that it is clear
	LLVector3 zero_vec(0.f, 0.f, 0.f);
	selected_parcel->setUserLocation(zero_vec);
	selected_parcel->setUserLookAt(zero_vec);

	gViewerParcelMgr.sendParcelPropertiesUpdate(selected_parcel);

	self->refresh();
}

//static
void LLPanelLandOptions::onClickPublishHelp(void*)
{
	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	LLParcel* parcel = gViewerParcelMgr.getFloatingParcelSelection()->getParcel();
	llassert(region); // Region should never be null.

	bool can_change_identity = region && parcel ?
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														 GP_LAND_CHANGE_IDENTITY) &&
			!region->getRegionFlag(REGION_FLAGS_BLOCK_PARCEL_SEARCH)
												: false;

	if (!can_change_identity)
	{
		gNotifications.add("ClickPublishHelpLandDisabled");
	}
	else
	{
		gNotifications.add("ClickPublishHelpLand");
	}
}

//---------------------------------------------------------------------------
// LLPanelLandAccess
//---------------------------------------------------------------------------

LLPanelLandAccess::LLPanelLandAccess(LLParcelSelectionHandle& parcel)
:	LLPanel("land_access_panel"),
	mParcel(parcel),
	mOnlyAllowText(NULL),
	mCheckPublicAccess(NULL),
	mCheckLimitPayment(NULL),
	mCheckLimitAge(NULL),
	mCheckLimitGroup(NULL),
	mCheckLimitPass(NULL),
	mPassCombo(NULL),
	mPriceSpin(NULL),
	mHourSpin(NULL),
	mListAccess(NULL),
	mListBanned(NULL),
	mAddAllowedButton(NULL),
	mRemoveAllowedButton(NULL),
	mAddBannedButton(NULL),
	mRemoveBannedButton(NULL)
{
}

bool LLPanelLandAccess::postBuild()
{
	mOnlyAllowText = getChild<LLTextBox>("access_label");

	mCheckPublicAccess = getChild<LLCheckBoxCtrl>("public_access");
	mCheckPublicAccess->setCommitCallback(onCommitPublicAccess);
	mCheckPublicAccess->setCallbackUserData(this);

	mCheckLimitPayment = getChild<LLCheckBoxCtrl>("limit_payment");
	mCheckLimitPayment->setCommitCallback(onCommitAny);
	mCheckLimitPayment->setCallbackUserData(this);

	mCheckLimitAge = getChild<LLCheckBoxCtrl>("limit_age_verified");
	mCheckLimitAge->setCommitCallback(onCommitAny);
	mCheckLimitAge->setCallbackUserData(this);

	mCheckLimitGroup = getChild<LLCheckBoxCtrl>("group_access_check");
	mCheckLimitGroup->setCommitCallback(onCommitGroupCheck);
	mCheckLimitGroup->setCallbackUserData(this);

	mCheckLimitPass = getChild<LLCheckBoxCtrl>("pass_access_check");
	mCheckLimitPass->setCommitCallback(onCommitAny);
	mCheckLimitPass->setCallbackUserData(this);

	mPassCombo = getChild<LLComboBox>("pass_combo");
	mPassCombo->setCommitCallback(onCommitAny);
	mPassCombo->setCallbackUserData(this);

	mPriceSpin = getChild<LLSpinCtrl>("price_spin_ctrl");
	mPriceSpin->setCommitCallback(onCommitAny);
	mPriceSpin->setCallbackUserData(this);

	mHourSpin = getChild<LLSpinCtrl>("hours_spin_ctrl");
	mHourSpin->setCommitCallback(onCommitAny);
	mHourSpin->setCallbackUserData(this);

	mAddAllowedButton = getChild<LLButton>("add_allowed");
	mAddAllowedButton->setClickedCallback(onClickAddAccess, this);

	mRemoveAllowedButton = getChild<LLButton>("remove_allowed");
	mRemoveAllowedButton->setClickedCallback(onClickRemoveAccess, this);

	mAddBannedButton = getChild<LLButton>("add_banned");
	mAddBannedButton->setClickedCallback(onClickAddBanned, this);

	mRemoveBannedButton = getChild<LLButton>("remove_banned");
	mRemoveBannedButton->setClickedCallback(onClickRemoveBanned, this);

	mListAccess = getChild<LLNameListCtrl>("access_list");
	mListAccess->sortByColumnIndex(0, true); // Ascending

	mListBanned = getChild<LLNameListCtrl>("banned_list");
	mListBanned->sortByColumnIndex(0, true); // Ascending

	return true;
}

//virtual
LLPanelLandAccess::~LLPanelLandAccess()
{
	// Release the selection handle
	mParcel = NULL;
}

//virtual
void LLPanelLandAccess::refresh()
{
	mListAccess->deleteAllItems();
	mListBanned->deleteAllItems();

	// Display options
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (!parcel)
	{
		mCheckPublicAccess->set(false);
		mCheckLimitPayment->set(false);
		mCheckLimitAge->set(false);
		mCheckLimitGroup->set(false);
		mCheckLimitGroup->setLabelArg("[GROUP]", LLStringUtil::null);
		mCheckLimitPass->set(false);
		mPriceSpin->setValue((F32)PARCEL_PASS_PRICE_DEFAULT);
		mHourSpin->setValue(PARCEL_PASS_HOURS_DEFAULT);
		mListAccess->setToolTipArg("[LISTED]", "0");
		mListAccess->setToolTipArg("[MAX]", "0");
		mListBanned->setToolTipArg("[LISTED]", "0");
		mListBanned->setToolTipArg("[MAX]", "0");
		return;
	}

	bool use_access_list = parcel->getParcelFlag(PF_USE_ACCESS_LIST);
	bool use_group = parcel->getParcelFlag(PF_USE_ACCESS_GROUP);
	bool public_access = !use_access_list;

	// Estate owner may have disabled allowing the parcel owner from managing
	// access
	if (parcel->getRegionAllowAccessOverride())
	{
		mCheckPublicAccess->set(public_access);
		mCheckLimitGroup->set(use_group);
	}
	else
	{
		mCheckPublicAccess->set(true);
		mCheckLimitGroup->set(false);
	}

	std::string group_name;
	if (gCacheNamep)
	{
		gCacheNamep->getGroupName(parcel->getGroupID(), group_name);
	}
	mCheckLimitGroup->setLabelArg("[GROUP]", group_name);

	std::string duration;

	// Allow list
	LLStringUtil::format_map_t args;
	S32 count = parcel->mAccessList.size();
	mListAccess->setToolTipArg("[LISTED]", llformat("%d", count));
	mListAccess->setToolTipArg("[MAX]",
							   llformat("%d", PARCEL_MAX_ACCESS_LIST));
	for (access_map_t::const_iterator cit = parcel->mAccessList.begin(),
									  end = parcel->mAccessList.end();
		 cit != end; ++cit)
	{
		const LLAccessEntry& entry = cit->second;
		if (entry.mTime)
		{
			args["[DURATION]"] = plain_text_duration(entry.mTime - time(NULL));
			duration = getString("remaining", args);
		}
		else
		{
			duration.clear();
		}
		mListAccess->addNameItem(entry.mID, ADD_SORTED, true, duration);
	}
	mListAccess->sortByName(true);

	// Ban List
	std::string always = getString("always");
	count = parcel->mBanList.size();
	mListBanned->setToolTipArg("[LISTED]", llformat("%d", count));
	mListBanned->setToolTipArg("[MAX]",
							   llformat("%d", PARCEL_MAX_ACCESS_LIST));
	for (access_map_t::const_iterator cit = parcel->mBanList.begin(),
									  end = parcel->mBanList.end();
		 cit != end; ++cit)
	{
		const LLAccessEntry& entry = cit->second;
		if (entry.mTime)
		{
			duration = plain_text_duration(entry.mTime - time(NULL));
			if (duration.empty())
			{
				duration = always;
			}
		}
		else
		{
			duration = always;
		}
		LLSD item;
		item["id"] = entry.mID;
		LLSD& columns = item["columns"];
		columns[0]["column"] = "name";		// Value is automatically populated
		columns[1]["column"] = "duration";
		columns[1]["value"] = duration;
		mListBanned->addElement(item);
	}
	mListBanned->sortByName(true);

	if (parcel->getRegionDenyAnonymousOverride())
	{
		mCheckLimitPayment->set(true);
	}
	else
	{
		mCheckLimitPayment->set(parcel->getParcelFlag(PF_DENY_ANONYMOUS));
	}
	if (parcel->getRegionDenyAgeUnverifiedOverride())
	{
		mCheckLimitAge->set(true);
	}
	else
	{
		mCheckLimitAge->set(parcel->getParcelFlag(PF_DENY_AGEUNVERIFIED));
	}

	bool use_pass = parcel->getParcelFlag(PF_USE_PASS_LIST);
	mCheckLimitPass->set(use_pass);
	if (mPassCombo && (public_access || !use_pass))
	{
		mPassCombo->selectByValue("anyone");
	}

	mPriceSpin->setValue((F32)parcel->getPassPrice());
	mHourSpin->setValue(parcel->getPassHours());
}

void LLPanelLandAccess::refreshUI()
{
	if (!mCheckPublicAccess)
	{
		// Something is *very* wrong !
		return;
	}

	mCheckPublicAccess->setEnabled(false);
	mCheckLimitPayment->setEnabled(false);
	mCheckLimitAge->setEnabled(false);
	mCheckLimitGroup->setEnabled(false);
	mCheckLimitPass->setEnabled(false);
	mPassCombo->setEnabled(false);
	mPriceSpin->setEnabled(false);
	mHourSpin->setEnabled(false);
	mListAccess->setEnabled(false);
	mListBanned->setEnabled(false);

	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (parcel)
	{
		// Estate owner may have disabled allowing the parcel owner from
		// managing access.
		bool can_manage_allowed = false;
		if (parcel->getRegionAllowAccessOverride())
		{
			can_manage_allowed =
				LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
															 GP_LAND_MANAGE_ALLOWED);
		}

		bool can_manage_banned =
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														 GP_LAND_MANAGE_BANNED);

		bool can_allow_groups = false;
		mCheckPublicAccess->setEnabled(can_manage_allowed);
		bool public_access = mCheckPublicAccess->getValue().asBoolean();
		if (public_access)
		{
			bool overriding = false;
			if (parcel->getRegionDenyAnonymousOverride())
			{
				overriding = true;
				mCheckLimitPayment->setEnabled(false);
			}
			else
			{
				mCheckLimitPayment->setEnabled(can_manage_allowed);
			}
			if (parcel->getRegionDenyAgeUnverifiedOverride())
			{
				overriding = true;
				mCheckLimitAge->setEnabled(false);
			}
			else
			{
				mCheckLimitAge->setEnabled(can_manage_allowed);
			}
			if (overriding)
			{
				mOnlyAllowText->setToolTip(getString("estate_override"));
			}
			else
			{
				mOnlyAllowText->setToolTip(std::string());
			}
#if 0
			mCheckLimitGroup->setEnabled(false);
#endif
			mCheckLimitPass->setEnabled(false);
			mPassCombo->setEnabled(false);
			mListAccess->setEnabled(false);
			can_allow_groups = mCheckLimitPayment->getValue().asBoolean() ^
							   mCheckLimitAge->getValue().asBoolean();
		}
		else
		{
			can_allow_groups = true;
			mCheckLimitPayment->setEnabled(false);
			mCheckLimitAge->setEnabled(false);

			bool sell_passes = mCheckLimitPass->getValue().asBoolean();
			mCheckLimitPass->setEnabled(can_manage_allowed);
			if (sell_passes)
			{
				mPassCombo->setEnabled(can_manage_allowed);
				mPriceSpin->setEnabled(can_manage_allowed);
				mHourSpin->setEnabled(can_manage_allowed);
			}
		}
		std::string group_name;
		if (gCacheNamep &&
			gCacheNamep->getGroupName(parcel->getGroupID(), group_name))
		{
			mCheckLimitGroup->setEnabled(can_manage_allowed &&
										 can_allow_groups);
		}
		mListAccess->setEnabled(can_manage_allowed);
		S32 allowed_list_count = parcel->mAccessList.size();
		mAddAllowedButton->setEnabled(can_manage_allowed &&
									  allowed_list_count < PARCEL_MAX_ACCESS_LIST);
		bool has_selected = mListAccess->getFirstSelectedIndex() >= 0;
		mRemoveAllowedButton->setEnabled(can_manage_allowed && has_selected);

		mListBanned->setEnabled(can_manage_banned);
		S32 banned_list_count = parcel->mBanList.size();
		mAddBannedButton->setEnabled(can_manage_banned &&
									 banned_list_count < PARCEL_MAX_ACCESS_LIST);
		has_selected = mListBanned->getFirstSelectedIndex() >= 0;
		mRemoveBannedButton->setEnabled(can_manage_banned && has_selected);
	}
}

void LLPanelLandAccess::refreshNames()
{
	if (!mCheckLimitGroup || !mParcel) return;

	std::string group_name;

	LLParcel* parcel = mParcel->getParcel();
	if (parcel && gCacheNamep)
	{
		gCacheNamep->getGroupName(parcel->getGroupID(), group_name);
	}

	mCheckLimitGroup->setLabelArg("[GROUP]", group_name);
}

//virtual
void LLPanelLandAccess::draw()
{
	refreshUI();
	refreshNames();
	LLPanel::draw();
}

//static
void LLPanelLandAccess::onCommitPublicAccess(LLUICtrl* ctrl, void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (!self || !self->mParcel) return;
	LLParcel* parcel = self->mParcel->getParcel();
	if (parcel)
	{
		onCommitAny(ctrl, userdata);
	}
}

//static
void LLPanelLandAccess::onCommitGroupCheck(LLUICtrl* ctrl, void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	bool use_pass_list = !self->mCheckPublicAccess->getValue().asBoolean();
	bool use_access_group = self->mCheckLimitGroup->getValue().asBoolean();

	if (self->mPassCombo &&  use_access_group && use_pass_list &&
		self->mPassCombo->getSelectedValue().asString() == "group")
	{
		self->mPassCombo->selectByValue("anyone");
	}

	onCommitAny(ctrl, userdata);
}

//static
void LLPanelLandAccess::onCommitAny(LLUICtrl* ctrl, void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (!self || !self->mParcel) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	bool public_access = self->mCheckPublicAccess->getValue().asBoolean();
	bool use_access_group = self->mCheckLimitGroup->getValue().asBoolean();
	if (use_access_group)
	{
		std::string group_name;
		if (!gCacheNamep ||
			!gCacheNamep->getGroupName(parcel->getGroupID(), group_name))
		{
			use_access_group = false;
		}
	}

	bool limit_payment = false;
	bool limit_age_verified = false;
	bool use_access_list = false;
	bool use_pass_list = false;

	if (public_access)
	{
		use_access_list = false;
		limit_payment = self->mCheckLimitPayment->getValue().asBoolean();
		limit_age_verified = self->mCheckLimitAge->getValue().asBoolean();
	}
	else
	{
		use_access_list = true;
		use_pass_list = self->mCheckLimitPass->getValue().asBoolean();
		if (self->mPassCombo && use_access_group && use_pass_list &&
			self->mPassCombo->getSelectedValue().asString() == "group")
		{
			use_access_group = false;
		}
	}

	S32 pass_price = llfloor((F32)self->mPriceSpin->getValue().asReal());
	F32 pass_hours = (F32)self->mHourSpin->getValue().asReal();

	// Push data into current parcel
	parcel->setParcelFlag(PF_USE_ACCESS_GROUP, use_access_group);
	parcel->setParcelFlag(PF_USE_ACCESS_LIST, use_access_list);
	parcel->setParcelFlag(PF_USE_PASS_LIST, use_pass_list);
	parcel->setParcelFlag(PF_USE_BAN_LIST, true);
	parcel->setParcelFlag(PF_DENY_ANONYMOUS, limit_payment);
	parcel->setParcelFlag(PF_DENY_AGEUNVERIFIED, limit_age_verified);

	parcel->setPassPrice(pass_price);
	parcel->setPassHours(pass_hours);

	// Send current parcel data upstream to server
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);

	// Might have changed properties, so let's redraw!
	self->refresh();
}

//static
void LLPanelLandAccess::onClickAddAccess(void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (!self || !self->mParcel || !gFloaterViewp) return;

	LLFloaterAvatarPicker* picker =
		LLFloaterAvatarPicker::show(callbackAvatarCBAccess, userdata);
	if (picker)
	{
		LLFloater* parent = gFloaterViewp->getParentFloater(self);
		if (parent)
		{
			parent->addDependentFloater(picker);
		}
	}
}

//static
void LLPanelLandAccess::callbackAvatarCBAccess(const std::vector<std::string>& names,
											   const uuid_vec_t& ids,
											   void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (self && self->mParcel && !names.empty() && !ids.empty())
	{
		LLUUID id = ids[0];
		LLParcel* parcel = self->mParcel->getParcel();
		if (parcel)
		{
			parcel->addToAccessList(id, 0);
			gViewerParcelMgr.sendParcelAccessListUpdate(AL_ACCESS);
			self->refresh();
		}
	}
}

//static
void LLPanelLandAccess::onClickRemoveAccess(void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (self && self->mParcel)
	{
		LLParcel* parcel = self->mParcel->getParcel();
		if (parcel)
		{
			std::vector<LLScrollListItem*> names =
				self->mListAccess->getAllSelected();
			for (std::vector<LLScrollListItem*>::iterator iter = names.begin();
				 iter != names.end();)
			{
				LLScrollListItem* item = *iter++;
				const LLUUID& agent_id = item->getUUID();
				parcel->removeFromAccessList(agent_id);
			}
			gViewerParcelMgr.sendParcelAccessListUpdate(AL_ACCESS);
			self->refresh();
		}
	}
}

//static
void LLPanelLandAccess::onClickAddBanned(void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (!self || !self->mParcel || !gFloaterViewp) return;

	LLFloaterAvatarPicker* picker =
		LLFloaterAvatarPicker::show(callbackAvatarCBBanned, userdata, true);
	if (picker)
	{
		LLFloater* parent = gFloaterViewp->getParentFloater(self);
		if (parent)
		{
			parent->addDependentFloater(picker);
		}
	}
}

//static
void LLPanelLandAccess::callbackAvatarCBBanned(const std::vector<std::string>& names,
											   const uuid_vec_t& ids,
											   void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (!self || !self->mParcel || !gFloaterViewp) return;

	LLFloaterBanDuration* duration =
		LLFloaterBanDuration::show(ids, callbackAvatarCBBanned2, self);
	if (duration)
	{
		LLFloater* parent = gFloaterViewp->getParentFloater(self);
		if (parent)
		{
			parent->addDependentFloater(duration);
		}
	}
}

//static
void LLPanelLandAccess::callbackAvatarCBBanned2(const uuid_vec_t& ids,
												S32 duration, void* userdata)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)userdata;
	if (!self || !self->mParcel || ids.empty()) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	U32 lists_to_update = 0;
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		const LLUUID& id = ids[i];
		if (parcel->addToBanList(id, duration))
		{
			LL_DEBUGS("ParcelAccess") << "Resident " << id
									  << " added to ban list for " << duration
									  << " seconds" << LL_ENDL;
			lists_to_update |= AL_BAN;
			// The resident was successfully added to the ban list but we also
			// need to check access list to ensure that agent will not be in
			// two lists simultaneously...
			if (parcel->removeFromAccessList(id))
			{
				lists_to_update |= AL_ACCESS;
				LL_DEBUGS("ParcelAccess") << "Resident " << id
										  << " removed from access list"
										  << LL_ENDL;
			}
		}
	}
	if (lists_to_update)
	{
		gViewerParcelMgr.sendParcelAccessListUpdate(lists_to_update);
		self->refresh();
	}
}

//static
void LLPanelLandAccess::onClickRemoveBanned(void* data)
{
	LLPanelLandAccess* self = (LLPanelLandAccess*)data;
	if (self && self->mParcel && self->mListBanned)
	{
		LLParcel* parcel = self->mParcel->getParcel();
		if (parcel)
		{
			std::vector<LLScrollListItem*> names = self->mListBanned->getAllSelected();
			for (std::vector<LLScrollListItem*>::iterator iter = names.begin();
				 iter != names.end(); )
			{
				LLScrollListItem* item = *iter++;
				const LLUUID& agent_id = item->getUUID();
				parcel->removeFromBanList(agent_id);
			}
			gViewerParcelMgr.sendParcelAccessListUpdate(AL_BAN);
			self->refresh();
		}
	}
}

//---------------------------------------------------------------------------
// LLPanelLandCovenant
//---------------------------------------------------------------------------

LLPanelLandCovenant::LLPanelLandCovenant(LLParcelSelectionHandle& parcel)
:	LLPanel("land_covenant_panel"),
	mParcel(parcel),
	mRegionNameText(NULL),
	mRegionTypeText(NULL),
	mRegionMaturityText(NULL),
	mRegionResellClauseText(NULL),
	mRegionChangeClauseText(NULL),
	mEstateNameText(NULL),
	mEstateOwnerText(NULL),
	mCovenantDateText(NULL),
	mCovenantEditor(NULL)
{
}

//virtual
LLPanelLandCovenant::~LLPanelLandCovenant()
{
	// Release the selection handle
	mParcel = NULL;
}

bool LLPanelLandCovenant::postBuild()
{
	mRegionNameText = getChild<LLTextBox>("region_name_text");
	mRegionTypeText = getChild<LLTextBox>("region_landtype_text");
	mRegionMaturityText = getChild<LLTextBox>("region_maturity_text");
	mRegionResellClauseText = getChild<LLTextBox>("resellable_clause");
	mRegionChangeClauseText = getChild<LLTextBox>("changeable_clause");
	mEstateNameText = getChild<LLTextBox>("estate_name_text");
	mEstateOwnerText = getChild<LLTextBox>("estate_owner_text");
	mCovenantDateText = getChild<LLTextBox>("covenant_timestamp_text");
	mCovenantEditor = getChild<LLViewerTextEditor>("covenant_editor");

	refresh();

	return true;
}

//virtual
void LLPanelLandCovenant::refresh()
{
	LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
	if (!region) return;

	mRegionNameText->setText(region->getName());
	mRegionTypeText->setText(region->getSimProductName());
	mRegionMaturityText->setText(region->getSimAccessString());

	if (region->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL))
	{
		mRegionResellClauseText->setText(getString("can_not_resell"));
	}
	else
	{
		mRegionResellClauseText->setText(getString("can_resell"));
	}

	if (region->getRegionFlag(REGION_FLAGS_ALLOW_PARCEL_CHANGES))
	{
		mRegionChangeClauseText->setText(getString("can_change"));
	}
	else
	{
		mRegionChangeClauseText->setText(getString("can_not_change"));
	}

	// Send EstateCovenantInfo message
	region->sendEstateCovenantRequest();
}

//static
void LLPanelLandCovenant::updateCovenantText(const std::string &string)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		self->mCovenantEditor->setHandleEditKeysDirectly(true);
		self->mCovenantEditor->setText(string);
	}
}

//static
void LLPanelLandCovenant::updateLastModified(const std::string& text)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		self->mCovenantDateText->setText(text);
	}
}

//static
void LLPanelLandCovenant::updateEstateName(const std::string& name)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		self->mEstateNameText->setText(name);
	}
}

//static
void LLPanelLandCovenant::updateEstateOwnerName(const std::string& name)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		self->mEstateOwnerText->setText(name);
	}
}

//---------------------------------------------------------------------------
// LLPanelLandExperiences
//---------------------------------------------------------------------------

//static
void* LLPanelLandExperiences::createAllowedExperiencesPanel(void* data)
{
	LLPanelLandExperiences* self = (LLPanelLandExperiences*)data;
	self->mAllowed = new LLPanelExperienceListEditor();
	return self->mAllowed;
}

//static
void* LLPanelLandExperiences::createBlockedExperiencesPanel(void* data)
{
	LLPanelLandExperiences* self = (LLPanelLandExperiences*)data;
	self->mBlocked = new LLPanelExperienceListEditor();
	return self->mBlocked;
}

LLPanelLandExperiences::LLPanelLandExperiences(LLSafeHandle<LLParcelSelection>& parcelp)
:	mParcel(parcelp)
{
	LLCallbackMap::map_t factory_map;
	factory_map["panel_allowed"] = LLCallbackMap(createAllowedExperiencesPanel,
												 this);
	factory_map["panel_blocked"] = LLCallbackMap(createBlockedExperiencesPanel,
												 this);
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_region_experiences.xml",
											   &factory_map);
}

//virtual
LLPanelLandExperiences::~LLPanelLandExperiences()
{
	// Release the selection handle
	mParcel = NULL;
}

bool LLPanelLandExperiences::postBuild()
{
	if (!mAllowed || !mBlocked) return false;

	setupList(mAllowed, "panel_allowed",
			  EXPERIENCE_KEY_TYPE_ALLOWED, AL_ALLOW_EXPERIENCE);
	setupList(mBlocked, "panel_blocked",
			  EXPERIENCE_KEY_TYPE_BLOCKED, AL_BLOCK_EXPERIENCE);

	// Only non-grid-wide experiences
	mAllowed->addFilter(boost::bind(LLExperienceCache::FilterWithProperty,
									_1, LLExperienceCache::PROPERTY_GRID));

	// No privileged ones
	mBlocked->addFilter(boost::bind(LLExperienceCache::FilterWithoutProperties,
									_1,
									LLExperienceCache::PROPERTY_PRIVILEGED |
									LLExperienceCache::PROPERTY_GRID));

	getChild<LLPanel>("trusted_layout_panel")->setVisible(false);
	getChild<LLPanel>("help_text_layout_panel")->setVisible(false);
	mAllowed->getChild<LLTextBox>("text_name")->setToolTip(getString("allowed_parcel_text"));
	mBlocked->getChild<LLTextBox>("text_name")->setToolTip(getString("blocked_parcel_text"));

	return LLPanel::postBuild();
}

void LLPanelLandExperiences::setupList(LLPanelExperienceListEditor* panel,
									   const std::string& control_name,
									   U32 xp_type,
									   U32 access_type)
{
	if (!panel) return;

	panel->getChild<LLTextBox>("text_name")->setText(panel->getString(control_name));
	panel->setMaxExperienceIDs(PARCEL_MAX_EXPERIENCE_LIST);
	panel->setAddedCallback(boost::bind(&LLPanelLandExperiences::experienceAdded,
										this, _1, xp_type, access_type));
	panel->setRemovedCallback(boost::bind(&LLPanelLandExperiences::experienceRemoved,
										  this, _1, access_type));
}

void LLPanelLandExperiences::experienceAdded(const LLUUID& id, U32 xp_type,
											 U32 access_type)
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (parcel)
	{
		parcel->setExperienceKeyType(id, xp_type);
		gViewerParcelMgr.sendParcelAccessListUpdate(access_type);
		refresh();
	}
}

void LLPanelLandExperiences::experienceRemoved(const LLUUID& id,
											   U32 access_type)
{
	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (parcel)
	{
		parcel->setExperienceKeyType(id, EXPERIENCE_KEY_TYPE_NONE);
		gViewerParcelMgr.sendParcelAccessListUpdate(access_type);
		refresh();
	}
}

void LLPanelLandExperiences::refreshPanel(LLPanelExperienceListEditor* panel,
										  U32 xp_type)
{
	if (!panel)	return;

	LLParcel* parcel = mParcel ? mParcel->getParcel() : NULL;
	if (!parcel)
	{
		// Disable the panel
		panel->setReadonly();
		panel->setDisabled();
		panel->setExperienceIds(LLSD::emptyArray());
		return;
	}

	panel->setDisabled(false);
	// Enable the panel, as read only or not depending on permissions
	bool can_modify =
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														 GP_LAND_OPTIONS);
	panel->setReadonly(!can_modify);
	access_map_t entries = parcel->getExperienceKeysByType(xp_type);
	LLSD ids = LLSD::emptyArray();
	for (access_map_t::iterator it = entries.begin(), end = entries.end();
		 it !=  end; ++it)
	{
		ids.append(it->second.mID);
	}
	panel->setExperienceIds(ids);
	panel->refreshExperienceCounter();
}

//virtual
void LLPanelLandExperiences::refresh()
{
	refreshPanel(mAllowed, EXPERIENCE_KEY_TYPE_ALLOWED);
	refreshPanel(mBlocked, EXPERIENCE_KEY_TYPE_BLOCKED);
}
