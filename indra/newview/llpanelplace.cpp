/**
 * @file llpanelplace.cpp
 * @brief Display of a place in the Find directory.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llpanelplace.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llfloater.h"
#include "llqueryflags.h"
#include "llregionhandle.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "llsdserialize.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroupinfo.h"
#include "llfloaterworldmap.h"
#include "llgridmanager.h"
#include "lltexturectrl.h"
#include "llviewercontrol.h"
#if CREATE_LANDMARK
# include "llviewermenu.h"		// For create_landmark()
#endif
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llweb.h"
#include "llworldmap.h"

//static
std::set<LLPanelPlace*> LLPanelPlace::sInstances;

// LLPanelPlace class proper

LLPanelPlace::LLPanelPlace(bool can_close_parent)
:	LLPanel(std::string("Places Panel")),
	mCanCloseParent(can_close_parent),
	mTeleportRequested(false),
	mOwnerIsGroup(false),
	mAuctionID(0)
{
	sInstances.insert(this);
}

LLPanelPlace::~LLPanelPlace()
{
	sInstances.erase(this);
	mTeleportArrivingConnection.disconnect();
	mTeleportFailedConnection.disconnect();
	gViewerParcelMgr.removeInfoObserver(mParcelID, this);
}

bool LLPanelPlace::postBuild()
{
	// Since this is only used in the directory browser, always
	// disable the snapshot control. Otherwise clicking on it will
	// open a texture picker.
	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	mSnapshotCtrl->setEnabled(false);

    mParcelNameText = getChild<LLTextBox>("name_editor");
	// Text boxes appear to have a " " in them by default.  This breaks the
	// emptiness test for filling in data from the network.  Slam to empty.
	mParcelNameText->setText(LLStringUtil::null);

    mDescEditor = getChild<LLTextEditor>("desc_editor");

	mParcelInfoText = getChild<LLTextBox>("info_editor");

	// This item exists only in panel_place_small.xml
	mLandTypeText = getChild<LLTextBox>("land_type_display", true, false);

	// These items exists only in panel_place.xml
	mOwnerLabel = getChild<LLTextBox>("owner_label", true, false);
	mOwnerText = getChild<LLTextBox>("owner_name", true, false);
	if (mOwnerText)
	{
		mOwnerText->setClickedCallback(onClickOwnerName, this);
		mOwnerText->setColor(LLTextEditor::getLinksColor());
	}

    mLocationText = getChild<LLTextBox>("location_editor");

	mTeleportBtn = getChild<LLButton>( "teleport_btn");
	mTeleportBtn->setClickedCallback(onClickTeleport);
	mTeleportBtn->setCallbackUserData(this);

	mMapBtn = getChild<LLButton>("map_btn");
	mMapBtn->setClickedCallback(onClickMap);
	mMapBtn->setCallbackUserData(this);

#if CREATE_LANDMARK
	mLandmarkBtn = getChild<LLButton>( "landmark_btn");
	mLandmarkBtn->setClickedCallback(onClickLandmark);
	mLandmarkBtn->setCallbackUserData(this);
#endif

	mAuctionBtn = getChild<LLButton>("auction_btn");
	mAuctionBtn->setClickedCallback(onClickAuction);
	mAuctionBtn->setCallbackUserData(this);

	// Default to no auction button. We will show it if we get an auction Id.
	mAuctionBtn->setVisible(false);

	mTeleportArrivingConnection =
		gViewerParcelMgr.setTPArrivingCallback(boost::bind(&LLPanelPlace::onTeleportArriving,
														   this));
	mTeleportFailedConnection =
		gViewerParcelMgr.setTPFailedCallback(boost::bind(&LLPanelPlace::onTeleportFailed,
														 this));
	return true;
}

void LLPanelPlace::displayItemInfo(const LLInventoryItem* pItem)
{
	mParcelNameText->setText(pItem->getName());
	mDescEditor->setText(pItem->getDescription());
}

// Use this for search directory clicks, because we are totally recycling the
// panel and don't need to use what's there.
// For SLURL clicks, don't call this, because we need to cache the location
// info from the user.
void LLPanelPlace::resetLocation()
{
	mTeleportArrivingConnection.disconnect();
	mTeleportFailedConnection.disconnect();
	gViewerParcelMgr.removeInfoObserver(mParcelID, this);
	mParcelID.setNull();
	mRequestedID.setNull();
	mRegionID.setNull();
	mLandmarkAssetID.setNull();
	mPosGlobal.clear();
	mPosRegion.clear();
	mAuctionID = 0;
	mParcelNameText->setText(LLStringUtil::null);
	mDescEditor->setText(LLStringUtil::null);
	mParcelInfoText->setText(LLStringUtil::null);
	if (mLandTypeText)
	{
		mLandTypeText->setText(LLStringUtil::null);
	}
	if (mOwnerLabel)
	{
		mOwnerLabel->setVisible(false);
	}
	if (mOwnerText)
	{
		mOwnerText->setVisible(false);
		mOwnerText->setText(LLStringUtil::null);
	}
	mLocationText->setText(LLStringUtil::null);
}

// Set the name and clear other bits of info. Used for SLURL clicks
void LLPanelPlace::resetName(const std::string& name)
{
	setName(name);
	mDescEditor->setText(LLStringUtil::null);
	llinfos << "Clearing place name" << llendl;
	mParcelNameText->setText(LLStringUtil::null);
	mParcelInfoText->setText(LLStringUtil::null);
	if (mLandTypeText)
	{
		mLandTypeText->setText(LLStringUtil::null);
	}
	if (mOwnerLabel)
	{
		mOwnerLabel->setVisible(false);
	}
	if (mOwnerText)
	{
		mOwnerText->setVisible(false);
		mOwnerText->setText(LLStringUtil::null);
	}
}

//virtual
void LLPanelPlace::setParcelID(const LLUUID& parcel_id)
{
	mParcelID = parcel_id;
	sendParcelInfoRequest();
}

void LLPanelPlace::setSnapshot(const LLUUID& snapshot_id)
{
	mSnapshotCtrl->setImageAssetID(snapshot_id);
}

void LLPanelPlace::setLocationString(const std::string& location)
{
	mLocationText->setText(location);
}

void LLPanelPlace::setLandTypeString(const std::string& land_type)
{
	if (mLandTypeText)
	{
		mLandTypeText->setText(land_type);
	}
}

void LLPanelPlace::sendParcelInfoRequest()
{
	if (mParcelID != mRequestedID)
	{
		gViewerParcelMgr.addInfoObserver(mParcelID, this);
		gViewerParcelMgr.sendParcelInfoRequest(mParcelID);
		mRequestedID = mParcelID;
	}
}

//virtual
void LLPanelPlace::setErrorStatus(S32 status, const std::string& reason)
{
	// Remove the observer
	gViewerParcelMgr.removeInfoObserver(mParcelID, this);

	// We only really handle 404 and 499 errors
	std::string error_text;
	if (status == HTTP_NOT_FOUND)
	{
		error_text = getString("server_error_text");
	}
	else if (status == HTTP_INTERNAL_ERROR)
	{
		error_text = getString("server_forbidden_text");
	}
	mDescEditor->setText(error_text);
}

//virtual
void LLPanelPlace::processParcelInfo(const LLParcelData& parcel_data)
{
	mAuctionID = parcel_data.mAuctionId;

	if (parcel_data.mSnapshotId.notNull())
	{
		mSnapshotCtrl->setImageAssetID(parcel_data.mSnapshotId);
	}

	// Only assign the name and description if they are not empty and there
	// is not a value present (e.g. passed in from a landmark)
	if (mParcelNameText->getText().empty() && !parcel_data.mName.empty())
	{
		mParcelNameText->setText(parcel_data.mName);
	}
	if (mDescEditor->getText().empty() && !parcel_data.mDesc.empty())
	{
		mDescEditor->setText(parcel_data.mDesc);
	}

	U8 flags = parcel_data.mFlags;

	LLUIString traffic = getString("traffic_text");
	traffic.setArg("[TRAFFIC]", llformat("%d", (int)parcel_data.mDwell));
	std::string info_text = traffic;

	info_text += ' ';
	LLUIString area = getString("area_text");
	area.setArg("[AREA]", llformat("%d", parcel_data.mActualArea));
	info_text += area;

	bool for_sale = flags & DFQ_FOR_SALE;
	if (for_sale)
	{
		info_text += ' ';
		LLUIString forsale = getString("forsale_text");
		forsale.setArg("[PRICE]", llformat("%d", parcel_data.mSalePrice));
		info_text += forsale;
	}

	if (mAuctionID)
	{
		for_sale = true;
		info_text += ' ';
		LLUIString auction = getString("auction_text");
		auction.setArg("[ID]", llformat("%010d ", mAuctionID));
		info_text += auction;
	}
	mAuctionBtn->setVisible(mAuctionID > 0);

	mParcelInfoText->setText(info_text);

	mOwnerID = parcel_data.mOwnerId;
	if (!for_sale && mOwnerText && mOwnerID.notNull() && gCacheNamep)
	{
		std::string name;
		bool got_name;
		mOwnerIsGroup = flags & 0x4;	// Depends onto DRTSIM-453
		if (mOwnerIsGroup)
		{
			got_name = gCacheNamep->getGroupName(mOwnerID, name);
		}
		else
		{
			got_name = gCacheNamep->getFullName(mOwnerID, name);
		}
		if (got_name)
		{
			if (mOwnerLabel)
			{
				mOwnerLabel->setVisible(true);
			}
			mOwnerText->setVisible(true);
			mOwnerText->setText(name);
		}
		else
		{
			gCacheNamep->get(mOwnerID, mOwnerIsGroup,
							 boost::bind(&LLPanelPlace::nameCallback,
										 _1, _2, _3, this));
		}
	}

	// *HACK: Flag 0x2 == adult region, Flag 0x1 == mature region, otherwise
	// assume PG.
	std::string rating;
	if (flags & 0x2)
	{
		rating = LLViewerRegion::accessToString(SIM_ACCESS_ADULT);
	}
	else if (flags & 0x1)
	{
		rating = LLViewerRegion::accessToString(SIM_ACCESS_MATURE);
	}
	else
	{
		rating = LLViewerRegion::accessToString(SIM_ACCESS_PG);
	}

	if (mPosGlobal.isExactlyZero())
	{
		mPosGlobal.set(parcel_data.mGlobalX, parcel_data.mGlobalY,
					   parcel_data.mGlobalZ);
	}

	S32 region_x, region_y, region_z;
	// If the region position is zero, grab position from the global
	if (mPosRegion.isExactlyZero())
	{
		region_x = ll_round(parcel_data.mGlobalX) % REGION_WIDTH_UNITS;
		region_y = ll_round(parcel_data.mGlobalY) % REGION_WIDTH_UNITS;
		region_z = ll_round(parcel_data.mGlobalZ);
	}
	else
	{
		// Just use given region position for display
		region_x = ll_round(mPosRegion.mV[0]);
		region_y = ll_round(mPosRegion.mV[1]);
		region_z = ll_round(mPosRegion.mV[2]);
	}

	std::string location = llformat("%s %d, %d, %d (%s)",
									parcel_data.mSimName.c_str(),
									region_x, region_y, region_z,
									rating.c_str());
	mLocationText->setText(location);
}

void LLPanelPlace::displayParcelInfo(const LLVector3& pos_region,
									 const LLUUID& landmark_asset_id,
									 // item_id so to send map correct id
									 const LLUUID& landmark_item_id,
									 const LLUUID& region_id,
									 const LLVector3d& pos_global)
{
	mRegionID = region_id;
	mPosRegion = pos_region;
	mPosGlobal = pos_global;
	mLandmarkAssetID = landmark_asset_id;
	mLandmarkItemID = landmark_item_id;

	const std::string& url = gAgent.getRegionCapability("RemoteParcelRequest");
	if (url.empty())
	{
		mDescEditor->setText(getString("server_update_text"));
	}
	else
	{
		gViewerParcelMgr.requestRegionParcelInfo(url, mRegionID, mPosRegion,
												 mPosGlobal, getObserverHandle());
	}

	mSnapshotCtrl->setImageAssetID(LLUUID::null);
	mSnapshotCtrl->setFallbackImageName("default_land_picture.j2c");
}

//static
void LLPanelPlace::onTeleportArriving(LLPanelPlace* self)
{
	if (sInstances.count(self) && self->mTeleportRequested)
	{
		self->mTeleportRequested = false;
		if (gSavedSettings.getBool("HideFloatersOnTPSuccess"))
		{
			LLView* parent_viewp = self->getParent();
			if (!parent_viewp) return;
			LLFloater* parent_floaterp = parent_viewp->asFloater();
			if (parent_floaterp && parent_floaterp->getVisible() &&
				!parent_floaterp->isMinimized())
			{
				if (self->canCloseParent())
				{
					parent_floaterp->close();
				}
				else
				{
					parent_floaterp->setVisible(false);
				}
			}
		}
	}
}

//static
void LLPanelPlace::onTeleportFailed(LLPanelPlace* self)
{
	if (sInstances.count(self) && self->mTeleportRequested)
	{
		self->mTeleportRequested = false;
	}
}

//static
void LLPanelPlace::onClickTeleport(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;
	if (!self) return;

	self->mTeleportRequested = true;

	if (self->mLandmarkAssetID.notNull())
	{
		gAgent.teleportViaLandmark(self->mLandmarkAssetID);
		if (gFloaterWorldMapp)
		{
			// remember this must be an inventory item id, not an asset UUID
			gFloaterWorldMapp->trackLandmark(self->mLandmarkItemID);
		}
	}
	else if (!self->mPosGlobal.isExactlyZero())
	{
		gAgent.teleportViaLocation(self->mPosGlobal);
		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		}
	}
}

//static
void LLPanelPlace::onClickMap(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;
	if (self && !self->mPosGlobal.isExactlyZero() && gFloaterWorldMapp)
	{
		// It's likely we're going to TP and don't care any more about this
		// panel, so let's flag it like if a TP was requested to allow
		// auto-close on next TP success:
		self->mTeleportRequested = true;

		gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		LLFloaterWorldMap::show(NULL, true);
	}
}

#if CREATE_LANDMARK
//static
void LLPanelPlace::onClickLandmark(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;
	create_landmark(self->mParcelNameText->getText(), "", self->mPosGlobal);
}
#endif

//static
void LLPanelPlace::onClickAuction(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;
	if (!self) return;
	LLSD payload;
	payload["auction_id"] = self->mAuctionID;

	gNotifications.add("GoToAuctionPage", LLSD(), payload,
					   callbackAuctionWebPage);
}

//static
bool LLPanelPlace::callbackAuctionWebPage(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		std::string url;
		S32 auction_id = notification["payload"]["auction_id"].asInteger();
		url = AUCTION_URL + llformat("%010d", auction_id );

		llinfos << "Loading auction page " << url << llendl;

		LLWeb::loadURL(url);
	}
	return false;
}

//static
void LLPanelPlace::onClickOwnerName(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;
	if (self && self->mOwnerID.notNull())
	{
		if (self->mOwnerIsGroup)
		{
			LLFloaterGroupInfo::showFromUUID(self->mOwnerID);
		}
		else
		{
			LLFloaterAvatarInfo::show(self->mOwnerID);
		}
	}
}

//static
void LLPanelPlace::nameCallback(const LLUUID& id, const std::string& name,
								bool is_group, LLPanelPlace* self)
{
	if (self && sInstances.count(self) && self->mOwnerText &&
		id == self->mOwnerID)
	{
		if (self->mOwnerLabel)
		{
			self->mOwnerLabel->setVisible(true);
		}
		self->mOwnerText->setVisible(true);
		self->mOwnerText->setText(name);
	}
}
