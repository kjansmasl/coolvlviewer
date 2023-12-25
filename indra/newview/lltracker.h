/**
 * @file lltracker.h
 * @brief Container for objects user is tracking.
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

// A singleton class for tracking stuff.
//
// TODO -- LLAvatarTracker functionality should probably be moved
// to the LLTracker class.

#ifndef LL_LLTRACKER_H
#define LL_LLTRACKER_H

#include "llpointer.h"
#include "llstring.h"
#include "lluuid.h"
#include "llvector3d.h"

class LLHUDText;

class LLTracker
{
protected:
	LOG_CLASS(LLTracker);

public:
	enum ETrackingStatus
	{
		TRACKING_NOTHING = 0,
		TRACKING_AVATAR = 1,
		TRACKING_LANDMARK = 2,
		TRACKING_LOCATION = 3,
	};

	enum ETrackingLocationType
	{
		LOCATION_NOTHING,
		LOCATION_EVENT,
		LOCATION_ITEM,
	};

	LLTracker();
	~LLTracker();

	// These are static so that they can be used a callbacks
	LL_INLINE ETrackingStatus getTrackingStatus()				{ return mTrackingStatus; }
	LL_INLINE ETrackingLocationType getTrackedLocationType()	{ return mTrackingLocationType; }
	LL_INLINE bool isTracking()									{ return mTrackingStatus != TRACKING_NOTHING; }
	LL_INLINE void clearFocus()									{ mTrackingStatus = TRACKING_NOTHING; }

	LL_INLINE const LLUUID& getTrackedLandmarkAssetID()			{ return mTrackedLandmarkAssetID; }
	LL_INLINE const LLUUID& getTrackedLandmarkItemID()			{ return mTrackedLandmarkItemID; }

	void trackAvatar(const LLUUID& avatar_id, const std::string& name);
	void trackLandmark(const LLUUID& landmark_asset_id,
					   const LLUUID& landmark_item_id,
					   const std::string& name);
	void trackLocation(const LLVector3d& pos, const std::string& full_name,
					   const std::string& tooltip,
					   ETrackingLocationType location_type = LOCATION_NOTHING);
	void stopTracking(bool clear_ui = false);

	// Returns global pos of tracked thing
	LLVector3d getTrackedPositionGlobal();

	bool hasLandmarkPosition();
	LL_INLINE const std::string& getTrackedLocationName()		{ return mTrackedLocationName; }

	void drawHUDArrow();

	// Draw in-world 3D tracking stuff
	void render3D();

	bool handleMouseDown(S32 x, S32 y);

	LL_INLINE const std::string& getLabel()						{ return mLabel; }
	LL_INLINE const std::string& getToolTip()					{ return mToolTip; }

protected:
	static void renderBeacon(LLVector3d pos_global, const LLColor4& color,
							 LLHUDText* hud_textp, const std::string& label);

	void stopTrackingAvatar(bool clear_ui = false);
	void stopTrackingLocation(bool clear_ui = false);
	void stopTrackingLandmark(bool clear_ui = false);

	void drawMarker(const LLVector3d& pos_global, const LLColor4& color);
	void setLandmarkVisited();
	void cacheLandmarkPosition();
	void purgeBeaconText();

protected:
	ETrackingStatus 		mTrackingStatus;
	ETrackingLocationType	mTrackingLocationType;

	LLPointer<LLHUDText>	mBeaconText;

	S32						mHUDArrowCenterX;
	S32						mHUDArrowCenterY;

	LLVector3d				mTrackedPositionGlobal;

	LLUUID					mTrackedLandmarkAssetID;
	LLUUID					mTrackedLandmarkItemID;

	std::string				mLabel;
	std::string				mToolTip;
	std::string				mTrackedLandmarkName;
	std::string				mTrackedLocationName;

	uuid_vec_t				mLandmarkAssetIDList;
	uuid_vec_t				mLandmarkItemIDList;

	bool					mIsTrackingLocation;
	bool					mHasReachedLandmark;
	bool 					mHasLandmarkPosition;
	bool					mLandmarkHasBeenVisited;
};

extern LLTracker gTracker;

#endif
