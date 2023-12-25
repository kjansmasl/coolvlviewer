/**
 * @file llpanelpick.cpp
 * @brief LLPanelPick class implementation
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

// Display of a "Top Pick" used both for the global top picks in the
// Find directory, and also for each individual user's picks in their
// profile.

#include "llviewerprecompiledheaders.h"

#include "llpanelpick.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llparcel.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "lltexturectrl.h"
#include "llfloaterworldmap.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llworldmap.h"

LLPanelPick::LLPanelPick(bool top_pick)
:	LLPanel(top_pick ? "Top picks panel" : "Picks panel"),
	LLAvatarPropertiesObserver(LLUUID::null, APT_PICK_INFO),
	mTopPick(top_pick),
	mDataRequested(false),
	mDataReceived(false)
{
	std::string xml_file = top_pick ? "panel_top_pick.xml"
									: "panel_avatar_pick.xml";
	LLUICtrlFactory::getInstance()->buildPanel(this, xml_file);

	LLAvatarProperties::addObserver(this);
}

LLPanelPick::~LLPanelPick()
{
	LLAvatarProperties::removeObserver(this);
}

void LLPanelPick::reset()
{
	mPickID.setNull();
	mCreatorID.setNull();
	mParcelID.setNull();

	// Do not request data, this is not valid
	mDataRequested = true;
	mDataReceived = false;

	mPosGlobal.clear();

	clearCtrls();
}

//virtual
bool LLPanelPick::postBuild()
{
	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	mSnapshotCtrl->setCommitCallback(onCommitAny);
	mSnapshotCtrl->setCallbackUserData(this);

	mNameEditor = getChild<LLLineEditor>("given_name_editor");
	mNameEditor->setCommitOnFocusLost(true);
	mNameEditor->setCommitCallback(onCommitAny);
	mNameEditor->setCallbackUserData(this);

	mDescEditor = getChild<LLTextEditor>("desc_editor");
	mDescEditor->setCommitOnFocusLost(true);
	mDescEditor->setCommitCallback(onCommitAny);
	mDescEditor->setCallbackUserData(this);
	mDescEditor->setTabsToNextField(true);

	mLocationEditor = getChild<LLLineEditor>("location_editor");

	mSetBtn = getChild<LLButton>( "set_location_btn");
	mSetBtn->setClickedCallback(onClickSetLocation);
	mSetBtn->setCallbackUserData(this);

	mTeleportBtn = getChild<LLButton>( "pick_teleport_btn");
	mTeleportBtn->setClickedCallback(onClickTeleport);
	mTeleportBtn->setCallbackUserData(this);

	mMapBtn = getChild<LLButton>( "pick_map_btn");
	mMapBtn->setClickedCallback(onClickMap);
	mMapBtn->setCallbackUserData(this);

	mSortOrderText = getChild<LLTextBox>("sort_order_text");

	mSortOrderEditor = getChild<LLLineEditor>("sort_order_editor");
	mSortOrderEditor->setPrevalidate(LLLineEditor::prevalidateInt);
	mSortOrderEditor->setCommitOnFocusLost(true);
	mSortOrderEditor->setCommitCallback(onCommitAny);
	mSortOrderEditor->setCallbackUserData(this);

	mEnabledCheck = getChild<LLCheckBoxCtrl>( "enabled_check");
	mEnabledCheck->setCommitCallback(onCommitAny);
	mEnabledCheck->setCallbackUserData(this);

	return true;
}

// Fill in some reasonable defaults for a new pick.
void LLPanelPick::initNewPick()
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk

	mPickID.generate();
	mCreatorID = gAgentID;
	mPosGlobal = gAgent.getPositionGlobal();

	// Try to fill in the current parcel
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		mNameEditor->setText(parcel->getName());
		mDescEditor->setText(parcel->getDesc());
		mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
	}

	// Commit to the database, since we have got "new" values.
	sendPickInfoUpdate();
}

void LLPanelPick::setPickID(const LLUUID& pick_id, const LLUUID& creator_id)
{
	mPickID = pick_id;
	mCreatorID = creator_id;
}

// Schedules the panel to request data from the server next time it is drawn
void LLPanelPick::markForServerRequest()
{
	mDataRequested = false;
	mDataReceived = false;
}

std::string LLPanelPick::getPickName()
{
	return mNameEditor->getText();
}

void LLPanelPick::sendPickInfoRequest()
{
	LLAvatarProperties::sendPickInfoRequest(mCreatorID, mPickID);
	mDataRequested = true;
}

void LLPanelPick::sendPickInfoUpdate()
{
	// If we do not have a pick Id yet, we will need to generate one,
	// otherwise we will keep overwriting pick_id 00000 in the database.
	if (mPickID.isNull())
	{
		mPickID.generate();
	}

	LLAvatarPickInfo data;
	data.mAvatarId = mCreatorID;
	data.mPickId = mPickID;
	data.mSnapshotId = mSnapshotCtrl->getImageAssetID();
	data.mParcelId = mParcelID;
	data.mName = mNameEditor->getText();
	data.mDesc = mDescEditor->getText();
	data.mPosGlobal = mPosGlobal;
	// Only top picks have a sort order
	data.mSortOrder = mTopPick ? atoi(mSortOrderEditor->getText().c_str()) : 0;
	data.mEnabled = mEnabledCheck->get();

	LLAvatarProperties::sendPickInfoUpdate(data);
}

// Returns a location text made up from the owner name, the parcel name, the
// sim name and the coordinates in that sim.
//static
std::string LLPanelPick::createLocationText(const std::string& owner_name,
											std::string parcel_name,
											const std::string& sim_name,
											const LLVector3d& pos_global)
{
	std::string location_text = owner_name;
	// strip leading spaces in parcel name
	while (!parcel_name.empty() && parcel_name[0] == ' ')
	{
		parcel_name = parcel_name.substr(1);
	}
	if (!parcel_name.empty())
	{
		if (location_text.empty())
		{
			location_text = parcel_name;
		}
		else
		{
			location_text += ", " + parcel_name;
		}
	}
	if (!sim_name.empty())
	{
		if (location_text.empty())
		{
			location_text = sim_name;
		}
		else
		{
			location_text += ", " + sim_name;
		}
	}
	if (!pos_global.isNull())
	{
		if (!location_text.empty())
		{
			location_text += " ";
		}
		S32 x = ll_roundp((F32)pos_global.mdV[VX]) % REGION_WIDTH_UNITS;
		S32 y = ll_roundp((F32)pos_global.mdV[VY]) % REGION_WIDTH_UNITS;
		S32 z = ll_roundp((F32)pos_global.mdV[VZ]);
		location_text += llformat("(%d, %d, %d)", x, y, z);
	}

	return location_text;
}

//virtual
void LLPanelPick::processProperties(S32 type, void* data)
{
	if (type != APT_PICK_INFO || mPickID.isNull())
	{
		return;	// Bad info, or we have not yet been assigned a pick.
	}

	LLAvatarPickInfo* info = (LLAvatarPickInfo*)data;
	if (info->mPickId != mPickID)
	{
		return;	// Not for us.
	}

	mDataReceived = true;
	mCreatorID = info->mAvatarId;
	mParcelID = info->mParcelId;
	mSimName = info->mSimName;
	mPosGlobal = info->mPosGlobal;
	std::string location_text = createLocationText(info->mUserName,
												   info->mParcelName,
												   mSimName,
												   mPosGlobal);
	mNameEditor->setText(info->mName);

	mDescEditor->clear();
	mDescEditor->setParseHTML(true);
	if (mCreatorID == gAgentID)
	{
		mDescEditor->setText(info->mDesc);
	}
	else
	{
		mDescEditor->appendColoredText(info->mDesc, false, false,
									   mDescEditor->getReadOnlyFgColor());
	}

	mSnapshotCtrl->setImageAssetID(info->mSnapshotId);
	mLocationEditor->setText(location_text);
	mEnabledCheck->set(info->mEnabled);

	mSortOrderEditor->setText(llformat("%d", info->mSortOrder));
}

void LLPanelPick::draw()
{
	refresh();

	LLPanel::draw();
}

void LLPanelPick::refresh()
{
	if (!mDataRequested)
	{
		sendPickInfoRequest();
	}

	// Check for god mode
	bool godlike = gAgent.isGodlike();
	bool is_self = gAgentID == mCreatorID;

	// Set button visibility/enablement appropriately
	if (mTopPick)
	{
		mSnapshotCtrl->setEnabled(godlike);
		mNameEditor->setEnabled(godlike);
		mDescEditor->setEnabled(godlike);

		mSortOrderText->setVisible(godlike);

		mSortOrderEditor->setVisible(godlike);
		mSortOrderEditor->setEnabled(godlike);

		mEnabledCheck->setVisible(godlike);
		mEnabledCheck->setEnabled(godlike);

		mSetBtn->setVisible(godlike);
		mSetBtn->setEnabled(godlike);
	}
	else
	{
		mSnapshotCtrl->setEnabled(is_self);
		mNameEditor->setEnabled(is_self);
		mDescEditor->setEnabled(is_self);

		mSortOrderText->setVisible(false);

		mSortOrderEditor->setVisible(false);
		mSortOrderEditor->setEnabled(false);

		mEnabledCheck->setVisible(false);
		mEnabledCheck->setEnabled(false);

		mSetBtn->setVisible(is_self);
		mSetBtn->setEnabled(is_self);
	}
}

//static
void LLPanelPick::onClickTeleport(void* data)
{
	LLPanelPick* self = (LLPanelPick*)data;
	if (self && !self->mPosGlobal.isExactlyZero())
	{
		gAgent.teleportViaLocation(self->mPosGlobal);
		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		}
	}
}

//static
void LLPanelPick::onClickMap(void* data)
{
	LLPanelPick* self = (LLPanelPick*)data;
	if (self && gFloaterWorldMapp)
	{
		gFloaterWorldMapp->trackLocation(self->mPosGlobal);
		LLFloaterWorldMap::show(NULL, true);
	}
}

//static
void LLPanelPick::onClickSetLocation(void* data)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		// Do not allow to set the location while under @showloc
		return;
	}
//mk

	LLPanelPick* self = (LLPanelPick*)data;
	if (!self) return;

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;

	self->mSimName = regionp->getName();
	self->mPosGlobal = gAgent.getPositionGlobal();

	std::string parcel_name;
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		self->mParcelID = parcel->getID();
		parcel_name = parcel->getName();
	}

	self->mLocationEditor->setText(createLocationText("", parcel_name,
													  self->mSimName,
													  self->mPosGlobal));
	onCommitAny(NULL, data);
}

//static
void LLPanelPick::onCommitAny(LLUICtrl* ctrl, void* data)
{
	LLPanelPick* self = (LLPanelPick*)data;
	// Have we received up to date data for this pick ?
	if (self && self->mDataReceived)
	{
		self->sendPickInfoUpdate();
		LLTabContainer* tab = (LLTabContainer*)self->getParent();
		if (tab)
		{
			tab->setCurrentTabName(self->mNameEditor->getText());
		}
	}
}
