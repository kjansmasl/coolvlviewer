/**
 * @file llpanelclassified.cpp
 * @brief LLPanelClassified class implementation
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

// Display of a classified used both for the global view in the
// Find directory, and also for each individual user's classified in their
// profile.

#include "llviewerprecompiledheaders.h"

#include "llpanelclassified.h"

#include "llalertdialog.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llclassifiedflags.h"
#include "llcombobox.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "lllineeditor.h"
#include "llparcel.h"
#include "llsdserialize.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"			// For abortQuit()
#include "llfloateravatarinfo.h"
#include "llfloaterclassified.h"
#include "llfloaterworldmap.h"
#include "lltexturectrl.h"
#include "llurldispatcher.h"		// For classified detail click teleports
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For gGenericDispatcher
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llworldmap.h"

constexpr S32 MINIMUM_PRICE_FOR_LISTING = 50;	// L$
constexpr S32 MATURE_CONTENT = 1;
constexpr S32 PG_CONTENT = 2;
constexpr S32 DECLINE_TO_STATE = 0;

///////////////////////////////////////////////////////////////////////////////
// LLClassifiedInfo static class
///////////////////////////////////////////////////////////////////////////////

LLClassifiedInfo::map_t LLClassifiedInfo::sCategories;

//static
void LLClassifiedInfo::loadCategories(const LLSD& options)
{
	std::string name;
	for (LLSD::array_const_iterator it = options.beginArray(),
									end = options.endArray();
		 it != end; ++it)
	{
		const LLSD& entry = *it;
		if (entry.has("name") && entry.has("category_id"))
		{
			U32 id = entry["category_id"].asInteger();
			sCategories[id] = entry["category_name"].asString();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelClassified class
///////////////////////////////////////////////////////////////////////////////

//static
LLPanelClassified::panel_list_t LLPanelClassified::sInstances;

// "classifiedclickthrough"
// strings[0] = classified_id
// strings[1] = teleport_clicks
// strings[2] = map_clicks
// strings[3] = profile_clicks
class LLDispatchClassifiedClickThrough final : public LLDispatchHandler
{
public:
	bool operator()(const LLDispatcher* dispatcher, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override
	{
		if (strings.size() != 4)
		{
			return false;
		}
		LLUUID classified_id(strings[0]);
		S32 teleport_clicks = atoi(strings[1].c_str());
		S32 map_clicks = atoi(strings[2].c_str());
		S32 profile_clicks = atoi(strings[3].c_str());
		LLPanelClassified::setClickThrough(classified_id, teleport_clicks,
										   map_clicks, profile_clicks, false);
		return true;
	}
};
static LLDispatchClassifiedClickThrough sClassifiedClickThrough;

LLPanelClassified::LLPanelClassified(bool in_finder, bool from_search)
:	LLPanel("Classified Panel"),
	LLAvatarPropertiesObserver(LLUUID::null, APT_CLASSIFIED_INFO),
	mInFinder(in_finder),
	mFromSearch(from_search),
	mDirty(false),
	mForceClose(false),
	mLocationChanged(false),
	mPriceForListing(0),
	mDataRequested(false),
	mPaidFor(false),
	mAutoRenewCheck(NULL),
	mUpdateBtn(NULL),
	mTeleportBtn(NULL),
	mMapBtn(NULL),
	mProfileBtn(NULL),
	mInfoText(NULL),
	mSetBtn(NULL),
	mClickThroughText(NULL),
	mTeleportClicksOld(0),
	mMapClicksOld(0),
	mProfileClicksOld(0),
	mTeleportClicksNew(0),
	mMapClicksNew(0),
	mProfileClicksNew(0)
{
	sInstances.insert(this);

	std::string file = mInFinder ? "panel_classified.xml"
								 : "panel_avatar_classified.xml";
	LLUICtrlFactory::getInstance()->buildPanel(this, file);

	LLAvatarProperties::addObserver(this);

	// Register dispatcher
	gGenericDispatcher.addHandler("classifiedclickthrough",
								  &sClassifiedClickThrough);
}

LLPanelClassified::~LLPanelClassified()
{
	LLAvatarProperties::removeObserver(this);
	sInstances.erase(this);
}

void LLPanelClassified::reset()
{
	mClassifiedID.setNull();
	mCreatorID.setNull();
	mParcelID.setNull();

	// Do not request data, this is not valid
	mDataRequested = true;

	mDirty = false;
	mPaidFor = false;

	mPosGlobal.clear();

	clearCtrls();
	resetDirty();
}

bool LLPanelClassified::postBuild()
{
	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	mSnapshotCtrl->setCommitCallback(onCommitAny);
	mSnapshotCtrl->setCallbackUserData(this);
	mSnapshotSize = mSnapshotCtrl->getRect();

	mNameEditor = getChild<LLLineEditor>("given_name_editor");
	mNameEditor->setMaxTextLength(DB_PARCEL_NAME_LEN);
	mNameEditor->setCommitOnFocusLost(true);
	mNameEditor->setFocusReceivedCallback(focusReceived, this);
	mNameEditor->setCommitCallback(onCommitAny);
	mNameEditor->setCallbackUserData(this);
	mNameEditor->setPrevalidate(LLLineEditor::prevalidateASCII);

	mDescEditor = getChild<LLTextEditor>("desc_editor");
	mDescEditor->setCommitOnFocusLost(true);
	mDescEditor->setFocusReceivedCallback(focusReceived, this);
	mDescEditor->setCommitCallback(onCommitAny);
	mDescEditor->setCallbackUserData(this);
	mDescEditor->setTabsToNextField(true);

	mLocationEditor = getChild<LLLineEditor>("location_editor");

	mSetBtn = getChild<LLButton>("set_location_btn");
	mSetBtn->setClickedCallback(onClickSet);
	mSetBtn->setCallbackUserData(this);

	mTeleportBtn = getChild<LLButton>("classified_teleport_btn");
	mTeleportBtn->setClickedCallback(onClickTeleport);
	mTeleportBtn->setCallbackUserData(this);

	mMapBtn = getChild<LLButton>("classified_map_btn");
	mMapBtn->setClickedCallback(onClickMap);
	mMapBtn->setCallbackUserData(this);

	if (mInFinder)
	{
		mProfileBtn = getChild<LLButton>("classified_profile_btn");
		mProfileBtn->setClickedCallback(onClickProfile);
		mProfileBtn->setCallbackUserData(this);
	}

	mCategoryCombo = getChild<LLComboBox>("classified_category_combo");
	for (LLClassifiedInfo::map_t::iterator
			it = LLClassifiedInfo::sCategories.begin(),
			end = LLClassifiedInfo::sCategories.end();
		 it != end; ++it)
	{
		mCategoryCombo->add(it->second, (void*)((intptr_t)it->first));
	}
	mCategoryCombo->setCurrentByIndex(0);
	mCategoryCombo->setCommitCallback(onCommitAny);
	mCategoryCombo->setCallbackUserData(this);

	mMatureCombo = getChild<LLComboBox>("classified_mature_check");
	mMatureCombo->setCurrentByIndex(0);
	mMatureCombo->setCommitCallback(onCommitAny);
	mMatureCombo->setCallbackUserData(this);
	if (gAgent.wantsPGOnly())
	{
		// Teens do not get to set mature flag. JC
		mMatureCombo->setVisible(false);
		mMatureCombo->setCurrentByIndex(PG_CONTENT);
	}

	if (!mInFinder)
	{
		mAutoRenewCheck = getChild<LLCheckBoxCtrl>("auto_renew_check");
		mAutoRenewCheck->setCommitCallback(onCommitAny);
		mAutoRenewCheck->setCallbackUserData(this);
	}

	mUpdateBtn = getChild<LLButton>("classified_update_btn");
	mUpdateBtn->setClickedCallback(onClickUpdate);
	mUpdateBtn->setCallbackUserData(this);

	if (!mInFinder)
	{
		mClickThroughText = getChild<LLTextBox>("click_through_text");
	}

	resetDirty();

	return true;
}

bool LLPanelClassified::titleIsValid()
{
	// Disallow leading spaces, punctuation, etc that screw up sort order.
	const std::string& name = mNameEditor->getText();
	if (name.empty())
	{
		gNotifications.add("BlankClassifiedName");
		return false;
	}
	if (!isalnum(name[0]))
	{
		gNotifications.add("ClassifiedMustBeAlphanumeric");
		return false;
	}

	return true;
}

void LLPanelClassified::apply()
{
	// Apply is used for automatically saving results, so only do that if there
	// is a difference, and this is a save not create.
	if (checkDirty() && mPaidFor)
	{
		sendClassifiedInfoUpdate();
	}
}

bool LLPanelClassified::saveCallback(const LLSD& notification,
									 const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	switch (option)
	{
		case 0:		// Save
		{
			sendClassifiedInfoUpdate();
			// fall through to close
		}

		case 1:		// Do not Save
		{
			mForceClose = true;
			// Close containing floater
			if (gFloaterViewp && gFloaterViewp->getParentFloater(this))
			{
				gFloaterViewp->getParentFloater(this)->close();
			}
			break;
		}

		default:	// Cancel
		{
			gAppViewerp->abortQuit();
		}
	}
	return false;
}

bool LLPanelClassified::canClose()
{
	if (mForceClose || !checkDirty())
	{
		return true;
	}

	LLSD args;
	args["NAME"] = mNameEditor->getText();
	gNotifications.add("ClassifiedSave", args, LLSD(),
					   boost::bind(&LLPanelClassified::saveCallback, this,
								   _1, _2));
	return false;
}

// Fill in some reasonable defaults for a new classified.
void LLPanelClassified::initNewClassified()
{
	// TODO: do not generate this on the client.
	mClassifiedID.generate();

	mCreatorID = gAgentID;

	mPosGlobal = gAgent.getPositionGlobal();

	mPaidFor = false;

	// Try to fill in the current parcel
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		mNameEditor->setText(parcel->getName());
		mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
		mCategoryCombo->setCurrentByIndex(0);
	}

	mUpdateBtn->setLabel(getString("publish_txt"));

	// Simulate clicking the "location" button
	LLPanelClassified::onClickSet(this);
}

//static
void LLPanelClassified::setClickThrough(const LLUUID& classified_id,
										S32 teleport, S32 map, S32 profile,
										bool from_new_table)
{
	for (panel_list_t::iterator it = sInstances.begin(), end = sInstances.end();
		 it != end; ++it)
	{
		LLPanelClassified* self = *it;
		// For top picks, must match pick id
		if (self->mClassifiedID != classified_id)
		{
			continue;
		}

		// We need to check to see if the data came from the new stat_table
		// or the old classified table. We also need to cache the data from
		// the two separate sources so as to display the aggregate totals.

		if (from_new_table)
		{
			self->mTeleportClicksNew = teleport;
			self->mMapClicksNew = map;
			self->mProfileClicksNew = profile;
		}
		else
		{
			self->mTeleportClicksOld = teleport;
			self->mMapClicksOld = map;
			self->mProfileClicksOld = profile;
		}

		if (self->mClickThroughText)
		{
			std::string msg =
				llformat("Clicks: %d teleport, %d map, %d profile",
						 self->mTeleportClicksNew + self->mTeleportClicksOld,
						 self->mMapClicksNew + self->mMapClicksOld,
						 self->mProfileClicksNew + self->mProfileClicksOld);
			self->mClickThroughText->setText(msg);
		}
	}
}

// Schedules the panel to request data from the server next time it is drawn.
void LLPanelClassified::markForServerRequest()
{
	mDataRequested = false;
}

std::string LLPanelClassified::getClassifiedName()
{
	return mNameEditor->getText();
}

void LLPanelClassified::sendClassifiedInfoRequest()
{
	if (mClassifiedID == mRequestedID)
	{
		// Nothing to do.
		return;
	}

	LLAvatarProperties::sendClassifiedInfoRequest(mClassifiedID);
	mRequestedID = mClassifiedID;
	mDataRequested = true;

	// While we are at it let's get the stats from the new table if that
	// capability exists.
	const std::string& url = gAgent.getRegionCapability("SearchStatRequest");
	if (url.empty())
	{
		return;
	}
	llinfos << "Classified stat request via capability. Classified Id: "
			<< mClassifiedID << llendl;
	LLSD body;
	body["classified_id"] = mClassifiedID;
	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost(url, body,
														   boost::bind(&LLPanelClassified::handleSearchStatResponse,
																	   mClassifiedID,
																	   _1));
}

//static
void LLPanelClassified::handleSearchStatResponse(LLUUID id, LLSD result)
{
	if (!result.isMap())
	{
		llwarns << "Malformed response for classified: " << id << llendl;
		return;
	}

	S32 teleport = result["teleport_clicks"].asInteger();
	S32 map = result["map_clicks"].asInteger();
	S32 profile = result["profile_clicks"].asInteger();
	S32 search_teleport = result["search_teleport_clicks"].asInteger();
	S32 search_map = result["search_map_clicks"].asInteger();
	S32 search_profile = result["search_profile_clicks"].asInteger();

	LLPanelClassified::setClickThrough(id, teleport + search_teleport,
									   map + search_map,
									   profile + search_profile, true);
}

void LLPanelClassified::sendClassifiedInfoUpdate()
{
	// If we do not have a classified id yet, we will need to generate one,
	// otherwise we will keep overwriting classified_id 00000 in the database.
	if (mClassifiedID.isNull())
	{
		// *TODO: do not do this on the client.
		mClassifiedID.generate();
	}

	LLAvatarClassifiedInfo data;
	data.mClassifiedId = mClassifiedID;
	data.mCategory = mCategoryCombo->getCurrentIndex() + 1;
	data.mName = mNameEditor->getText();
	data.mDesc = mDescEditor->getText();
	data.mParcelId = mParcelID;
	data.mSnapshotId = mSnapshotCtrl->getImageAssetID();
	data.mPosGlobal = mPosGlobal;
	data.mListingPrice = mPriceForListing;
	bool auto_renew = mAutoRenewCheck && mAutoRenewCheck->get();
	bool mature = mMatureCombo->getCurrentIndex() == MATURE_CONTENT;
	// These flags do not matter here.
	constexpr bool adult_enabled = false;
	constexpr bool is_pg = false;
	data.mFlags = pack_classified_flags_request(auto_renew, is_pg, mature,
												adult_enabled);
	LLAvatarProperties::sendClassifiedInfoUpdate(data);

	mDirty = false;
}

//virtual
void LLPanelClassified::processProperties(S32 type, void* data)
{
	if (type != APT_CLASSIFIED_INFO || mClassifiedID.isNull())
	{
		return;	// Bad info, or we have not yet been assigned a classified.
	}

	LLAvatarClassifiedInfo* info = (LLAvatarClassifiedInfo*)data;
	if (info->mClassifiedId != mClassifiedID)
	{
		return;	// Not for us.
	}

	mCreatorID = info->mAvatarId;

	mParcelID = info->mParcelId;
	// "Location text" is actually the original name that the owner gave to
	// the parcel, and the location.
	std::string location_text = info->mParcelName;
	if (!location_text.empty())
	{
		location_text.append(", ");
	}
	mSimName = info->mSimName;
	mPosGlobal = info->mPosGlobal;
	S32 region_x = ll_roundp((F32)mPosGlobal.mdV[VX]) % REGION_WIDTH_UNITS;
	S32 region_y = ll_roundp((F32)mPosGlobal.mdV[VY]) % REGION_WIDTH_UNITS;
	S32 region_z = ll_roundp((F32)mPosGlobal.mdV[VZ]);
	std::string buffer = llformat("%s (%d, %d, %d)", mSimName.c_str(),
								  region_x, region_y, region_z);
	location_text.append(buffer);
	mLocationEditor->setText(location_text);
	mLocationChanged = false;

	mPriceForListing = info->mListingPrice;

	mNameEditor->setText(info->mName);
	mDescEditor->setText(info->mDesc);
	mSnapshotCtrl->setImageAssetID(info->mSnapshotId);

	mCategoryCombo->setCurrentByIndex(info->mCategory - 1);

	if (is_cf_mature(info->mFlags))
	{
		mMatureCombo->setCurrentByIndex(MATURE_CONTENT);
	}
	else
	{
		mMatureCombo->setCurrentByIndex(PG_CONTENT);
	}

	if (mAutoRenewCheck)
	{
		mAutoRenewCheck->set(is_cf_auto_renew(info->mFlags));
	}

	time_t tim = info->mCreationDate;
	tm* now = localtime(&tim);
	std::string datestr;
	timeStructToFormattedString(now,
								gSavedSettings.getString("ShortDateFormat"),
								datestr);
	LLStringUtil::format_map_t string_args;
	string_args["[DATE]"] = datestr;
	string_args["[AMT]"] = llformat("%d", mPriceForListing);
	if (getChild<LLTextBox>("classified_info_text", true, false))
	{
		childSetText("classified_info_text",
					 getString("ad_placed_paid", string_args));
	}

	// If we got data from the database, we know the listing is paid for.
	mPaidFor = true;

	mUpdateBtn->setLabel(getString("update_txt"));

	resetDirty();
}

void LLPanelClassified::draw()
{
	refresh();
	LLPanel::draw();
}

void LLPanelClassified::refresh()
{
	if (!mDataRequested)
	{
		sendClassifiedInfoRequest();
	}

	// Check for god mode
	bool godlike = gAgent.isGodlike();
	bool is_self = (gAgentID == mCreatorID);

	// Set button visibility/enablement appropriately
	if (mInFinder)
	{
		// End user does not need to see price twice, or date posted.

		mSnapshotCtrl->setEnabled(godlike);
		if (godlike)
		{
			// Make it smaller, so text is more legible
			mSnapshotCtrl->reshape(360, 270);
		}
		else
		{
			mSnapshotCtrl->setOrigin(mSnapshotSize.mLeft,
									 mSnapshotSize.mBottom);
			mSnapshotCtrl->reshape(mSnapshotSize.getWidth(),
								   mSnapshotSize.getHeight());
		}
		mNameEditor->setEnabled(godlike);
		mDescEditor->setEnabled(godlike);
		mCategoryCombo->setEnabled(godlike);
		mCategoryCombo->setVisible(godlike);

		mMatureCombo->setEnabled(godlike);
		mMatureCombo->setVisible(godlike);

		// Jesse (who is the only one who uses this, as far as we can tell
		// Says that he does not want a set location button - he has used it
		// accidently in the past.
		mSetBtn->setVisible(false);
		mSetBtn->setEnabled(false);

		mUpdateBtn->setEnabled(godlike);
		mUpdateBtn->setVisible(godlike);
	}
	else
	{
		mSnapshotCtrl->setEnabled(is_self);
		mNameEditor->setEnabled(is_self);
		mDescEditor->setEnabled(is_self);
		//mPriceEditor->setEnabled(is_self);
		mCategoryCombo->setEnabled(is_self);
		mMatureCombo->setEnabled(is_self);

		if (is_self && mMatureCombo->getCurrentIndex() == 0)
		{
			// It is a new panel. PG regions should have PG classifieds and
			// Adult should have Mature.
			setDefaultAccessCombo();
		}

		if (mAutoRenewCheck)
		{
			mAutoRenewCheck->setEnabled(is_self);
			mAutoRenewCheck->setVisible(is_self);
		}

		mClickThroughText->setEnabled(is_self);
		mClickThroughText->setVisible(is_self);

		mSetBtn->setVisible(is_self);
		mSetBtn->setEnabled(is_self);

		mUpdateBtn->setEnabled(is_self && checkDirty());
		mUpdateBtn->setVisible(is_self);
	}
}

//static
void LLPanelClassified::onClickUpdate(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	if (!self) return;

	// Disallow leading spaces, punctuation, etc that screw up sort order.
	if (!self->titleIsValid())
	{
		return;
	}

	// If user has not set mature, do not allow publish
	if (self->mMatureCombo->getCurrentIndex() == DECLINE_TO_STATE)
	{
		// Tell user about it
		gNotifications.add("SetClassifiedMature", LLSD(), LLSD(),
						   boost::bind(&LLPanelClassified::confirmMature, self,
									   _1, _2));
	}
	else
	{
		// Mature content flag is set, proceed
		self->gotMature();
	}
}

// Callback from a dialog indicating response to mature notification
bool LLPanelClassified::confirmMature(const LLSD& notification,
									  const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:		// 0 == Yes
			mMatureCombo->setCurrentByIndex(MATURE_CONTENT);
			break;

		case 1:		// 1 == No
			mMatureCombo->setCurrentByIndex(PG_CONTENT);
			break;

		default:	// 2 == Cancel
			return false;
	}

	// If we got here it means they set a valid value
	gotMature();
	return false;
}

// Called after we have determined whether this classified has
// mature content or not.
void LLPanelClassified::gotMature()
{
	// If already paid for, just do the update
	if (mPaidFor)
	{
		LLNotification::Params params("PublishClassified");
		params.functor(boost::bind(&LLPanelClassified::confirmPublish,
								   this, _1, _2));
		gNotifications.forceResponse(params, 0);
	}
	else
	{
		// Ask the user how much they want to pay
		LLFloaterPriceForListing::show(callbackGotPriceForListing, this);
	}
}

//static
void LLPanelClassified::callbackGotPriceForListing(S32 option,
												   std::string text,
												   void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;

	// Only do something if user hits publish
	if (option != 0) return;

	S32 price_for_listing = strtol(text.c_str(), NULL, 10);
	if (price_for_listing < MINIMUM_PRICE_FOR_LISTING)
	{
		LLSD args;
		std::string price_text = llformat("%d", MINIMUM_PRICE_FOR_LISTING);
		args["MIN_PRICE"] = price_text;

		gNotifications.add("MinClassifiedPrice", args);
		return;
	}

	// Price is acceptable, put it in the dialog for later read by update send
	self->mPriceForListing = price_for_listing;

	LLSD args;
	args["AMOUNT"] = llformat("%d", price_for_listing);
	gNotifications.add("PublishClassified", args, LLSD(),
					   boost::bind(&LLPanelClassified::confirmPublish, self,
								   _1, _2));
}

void LLPanelClassified::resetDirty()
{
	// Tell all the widgets to reset their dirty state since the ad was just
	// saved
	mSnapshotCtrl->resetDirty();
	mNameEditor->resetDirty();
	mDescEditor->resetDirty();
	mLocationEditor->resetDirty();
	mLocationChanged = false;
	mCategoryCombo->resetDirty();
	mMatureCombo->resetDirty();
	if (mAutoRenewCheck)
	{
		mAutoRenewCheck->resetDirty();
	}
}

bool LLPanelClassified::confirmPublish(const LLSD& notification,
									   const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	// Option 0 = publish
	if (option != 0) return false;

	sendClassifiedInfoUpdate();

	// *HACK: assume that top picks are always in a browser and non-finder
	// classifieds are always in a tab container.
	if (!mInFinder)
	{
		LLTabContainer* tab = (LLTabContainer*)getParent();
		tab->setCurrentTabName(mNameEditor->getText());
	}
#if 0	// *TODO: enable this
	else
	{
		LLPanelDirClassifieds* panelp = (LLPanelDirClassifieds*)getParent();
		panelp->renameClassified(mClassifiedID, mNameEditor->getText());
	}
#endif

	resetDirty();
	return false;
}

//static
void LLPanelClassified::onClickTeleport(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;

	if (self && !self->mPosGlobal.isExactlyZero())
	{
		gAgent.teleportViaLocation(self->mPosGlobal);
		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		}
		self->sendClassifiedClickMessage("teleport");
	}
}

//static
void LLPanelClassified::onClickMap(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	if (self)
	{
		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		}
		LLFloaterWorldMap::show(NULL, true);

		self->sendClassifiedClickMessage("map");
	}
}

//static
void LLPanelClassified::onClickProfile(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	if (self)
	{
		LLFloaterAvatarInfo::showFromDirectory(self->mCreatorID);
		self->sendClassifiedClickMessage("profile");
	}
}

#if 0
//static
void LLPanelClassified::onClickLandmark(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	create_landmark(self->mNameEditor->getText(), "", self->mPosGlobal);
}
#endif

//static
void LLPanelClassified::onClickSet(void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;

	// Save location for later.
	self->mPosGlobal = gAgent.getPositionGlobal();

	std::string location_text;
	std::string regionName = "(will update after publish)";
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		regionName = regionp->getName();
	}
	location_text.assign(regionName);
	location_text.append(", ");

	S32 region_x = ll_roundp((F32)self->mPosGlobal.mdV[VX]) %
				   REGION_WIDTH_UNITS;
	S32 region_y = ll_roundp((F32)self->mPosGlobal.mdV[VY]) %
				   REGION_WIDTH_UNITS;
	S32 region_z = ll_roundp((F32)self->mPosGlobal.mdV[VZ]);

	location_text.append(self->mSimName);
	location_text.append(llformat(" (%d, %d, %d)", region_x, region_y,
								  region_z));

	self->mLocationEditor->setText(location_text);
	self->mLocationChanged = true;

	self->setDefaultAccessCombo();

	// Set this to null so it updates on the next save.
	self->mParcelID.setNull();

	onCommitAny(NULL, data);
}

bool LLPanelClassified::checkDirty()
{
	mDirty = mLocationChanged || mSnapshotCtrl->isDirty() ||
			 mNameEditor->isDirty() || mDescEditor->isDirty() ||
			 mLocationEditor->isDirty() || mCategoryCombo->isDirty() ||
			 mMatureCombo->isDirty() ||
			 (mAutoRenewCheck && mAutoRenewCheck->isDirty());

	return mDirty;
}

//static
void LLPanelClassified::onCommitAny(LLUICtrl* ctrl, void* data)
{
	LLPanelClassified* self = (LLPanelClassified*)data;
	if (self)
	{
		self->checkDirty();
	}
}

//static
void LLPanelClassified::focusReceived(LLFocusableElement* ctrl, void* data)
{
	// Allow the data to be saved
	onCommitAny((LLUICtrl*)ctrl, data);
}

void LLPanelClassified::sendClassifiedClickMessage(const std::string& type)
{
	// You are allowed to click on your own ads to reassure yourself that the
	// system is working.
	LLSD body;
	body["type"] = type;
	body["from_search"] = mFromSearch;
	body["classified_id"] = mClassifiedID;
	body["parcel_id"] = mParcelID;
	body["dest_pos_global"] = mPosGlobal.getValue();
	body["region_name"] = mSimName;

	const std::string& url = gAgent.getRegionCapability("SearchStatTracking");
	if (url.empty())
	{
		return;
	}
	llinfos << "Sending classified click message via capability" << llendl;
	LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, body,
														  "Tracking click report sent.",
														  "Failed to send tracking click report.");
}

////////////////////////////////////////////////////////////////////////////////////////////

LLFloaterPriceForListing::LLFloaterPriceForListing()
:	LLFloater("classified price"),
	mCallback(NULL),
	mUserData(NULL)
{
}

//virtual
bool LLFloaterPriceForListing::postBuild()
{
	LLLineEditor* edit = getChild<LLLineEditor>("price_edit");
	if (edit)
	{
		edit->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);
		std::string min_price = llformat("%d", MINIMUM_PRICE_FOR_LISTING);
		edit->setText(min_price);
		edit->selectAll();
		edit->setFocus(true);
	}

	childSetAction("set_price_btn", onClickSetPrice, this);
	childSetAction("cancel_btn", onClickCancel, this);
	setDefaultBtn("set_price_btn");

	return true;
}

//static
void LLFloaterPriceForListing::show(void (*callback)(S32, std::string, void*),
									void* userdata)
{
	LLFloaterPriceForListing* self = new LLFloaterPriceForListing();

	// Builds and adds to gFloaterViewp
	LLUICtrlFactory::getInstance()->buildFloater(self,
												 "floater_price_for_listing.xml");
	self->center();

	self->mCallback = callback;
	self->mUserData = userdata;
}

//static
void LLFloaterPriceForListing::onClickSetPrice(void* data)
{
	buttonCore(0, data);
}

//static
void LLFloaterPriceForListing::onClickCancel(void* data)
{
	buttonCore(1, data);
}

//static
void LLFloaterPriceForListing::buttonCore(S32 button, void* data)
{
	LLFloaterPriceForListing* self = (LLFloaterPriceForListing*)data;
	if (self && self->mCallback)
	{
		std::string text = self->childGetText("price_edit");
		self->mCallback(button, text, self->mUserData);
		self->close();
	}
}

void LLPanelClassified::setDefaultAccessCombo()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp)
	{
		return;
	}
	U8 access = regionp->getSimAccess();
	if (access == SIM_ACCESS_PG)
	{
		mMatureCombo->setCurrentByIndex(PG_CONTENT);
	}
	else if (access == SIM_ACCESS_ADULT)
	{
		mMatureCombo->setCurrentByIndex(MATURE_CONTENT);
	}
}
