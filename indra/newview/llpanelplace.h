/**
 * @file llpanelplace.h
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

#ifndef LL_LLPANELPLACE_H
#define LL_LLPANELPLACE_H

#include "boost/signals2.hpp"

#include "llpanel.h"
#include "llvector3d.h"

#include "llviewerparcelmgr.h"

#define CREATE_LANDMARK 0

class LLButton;
class LLTextBox;
class LLLineEditor;
class LLTextEditor;
class LLTextureCtrl;
class LLInventoryItem;

class LLPanelPlace final : public LLPanel, LLParcelInfoObserver
{
protected:
	LOG_CLASS(LLPanelPlace);

public:
	LLPanelPlace(bool can_close_parent = true);
	~LLPanelPlace() override;

	bool postBuild() override;

	// Ignore all old location information, useful if you are recycling an
	// existing dialog and need to clear it.
	void resetLocation();

	// Sends a request for data about the given parcel, which will only update
	// the location if there is none already available.
	void setParcelID(const LLUUID& parcel_id) override;
	void setErrorStatus(S32 status, const std::string& reason) override;
	void processParcelInfo(const LLParcelData& parcel_data) override;

	void displayItemInfo(const LLInventoryItem* item);
	void setRegionID(const LLUUID& region_id)		{ mRegionID = region_id; }
	void setSnapshot(const LLUUID& snapshot_id);
	void setLocationString(const std::string& location);
	void setLandTypeString(const std::string& land_type);
	void resetName(const std::string& name);

	void sendParcelInfoRequest();
	void displayParcelInfo(const LLVector3& pos_region,
						   const LLUUID& landmark_asset_id,
						   const LLUUID& landmark_item_id,   // so we can send map floater the correct inventory item id
						   const LLUUID& region_id,
						   const LLVector3d& pos_global);

	LLTextureCtrl* getSnapshotCtrl() const			{ return mSnapshotCtrl; }

	bool canCloseParent()							{ return mCanCloseParent; }

protected:
	static void onClickTeleport(void* data);
	static void onClickMap(void* data);
#if CREATE_LANDMARK
	static void onClickLandmark(void* data);
#endif
	static void onClickAuction(void* data);
	static void onClickOwnerName(void* data);

	static void nameCallback(const LLUUID& id, const std::string& name,
							 bool is_group, LLPanelPlace* self);

	// Go to auction web page if user clicked OK
	static bool callbackAuctionWebPage(const LLSD& notification,
									   const LLSD& response);

	static void	onTeleportArriving(LLPanelPlace* self);
	static void	onTeleportFailed(LLPanelPlace* self);

protected:
	LLTextureCtrl*				mSnapshotCtrl;
	LLTextEditor*				mDescEditor;
	LLTextBox*					mParcelNameText;
	LLTextBox*					mParcelInfoText;
	LLTextBox*					mLandTypeText;
	LLTextBox*					mOwnerLabel;
	LLTextBox*					mOwnerText;
	LLTextBox*					mLocationText;
	LLButton*					mTeleportBtn;
	LLButton*					mMapBtn;
	LLButton*					mAuctionBtn;
#if CREATE_LANDMARK
	LLButton*					mLandmarkBtn;
#endif

	LLUUID						mParcelID;
	LLUUID						mRequestedID;
	LLUUID						mRegionID;
	LLUUID						mLandmarkAssetID;
	LLUUID						mLandmarkItemID;
	LLUUID						mOwnerID;

	// Absolute position of the location for teleport, may not be available
	// (hence zero)
	LLVector3d					mPosGlobal;

	// Region-local position for teleport, always available.
	LLVector3					mPosRegion;

	// Zero if this is not an auction
	S32							mAuctionID;

	boost::signals2::connection	mTeleportArrivingConnection;
	boost::signals2::connection	mTeleportFailedConnection;

	bool						mCanCloseParent;
	bool						mTeleportRequested;
	bool						mOwnerIsGroup;

	typedef std::set<LLPanelPlace*> panel_list_t;
	static panel_list_t 		sInstances;
};

#endif // LL_LLPANELPLACE_H
