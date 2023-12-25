/**
 * @file llfloaterworldmap.h
 * @brief LLFloaterWorldMap class definition
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
 * Copyright (c) 2009-2021, Henri Beauchamp.
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

/*
 * Map of the entire world, with multiple background images,
 * avatar tracking, teleportation by double-click, etc.
 */

#ifndef LL_LLFLOATERWORLDMAP_H
#define LL_LLFLOATERWORLDMAP_H

#include <vector>

#include "boost/signals2.hpp"

#include "hbfileselector.h"
#include "llfloater.h"
#include "llmemberlistener.h"
#include "llpointer.h"

#include "lltracker.h"

class LLButton;
class LLComboBox;
class LLCheckBoxCtrl;
class LLFriendObserver;
class LLIconCtrl;
class LLImageRaw;
class LLInventoryModel;
class LLInventoryObserver;
class LLItemInfo;
class LLLineEditor;
class LLMapParcelInfoObserver;
class LLScrollListCtrl;
class LLSearchEditor;
class LLSimInfo;
class LLSliderCtrl;
class LLSurface;
class LLSpinCtrl;
class LLTabContainer;
class LLViewerFetchedTexture;
struct LLParcelData;

class LLFloaterWorldMap final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterWorldMap);

public:
	LLFloaterWorldMap();
	~LLFloaterWorldMap() override;

	static void* createWorldMapView(void* data);

	bool postBuild() override;

	void onOpen() override;
	void onClose(bool app_quitting) override;

	static void show(void*, bool center_on_target);
	static void reloadIcons(void*);
	static void toggle(void*);
	static void hide(void*);

	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	void setVisible(bool visible) override;
	void draw() override;

	// Methods for dealing with inventory. The observe() method is called
	// during program startup. inventoryUpdated() will be called by a helper
	// object when an interesting change has occurred.
	void observeInventory(LLInventoryModel* inventory);
	void inventoryChanged();

	// Calls for dealing with changes in friendship
	void observeFriends();
	void friendsChanged();

	// Tracking methods
	void trackAvatar(const LLUUID& avatar_id, const std::string& name);
	void trackLandmark(const LLUUID& landmark_item_id);
	void trackLocation(const LLVector3d& pos,
					   const std::string& tooltip = LLStringUtil::null);
	void trackEvent(const LLItemInfo &event_info);
	void trackGenericItem(const LLItemInfo &item);
	void trackURL(const std::string& region_name, S32 x_coord, S32 y_coord,
				  S32 z_coord);

	void clearParcelInfoRequest();

	LL_INLINE LLVector3d& getRequestedParcelInfoGlobalPos()
	{
		return mRequestedGlobalPos;
	}

	LL_INLINE static const LLUUID& getHomeID()	{ return sHomeID; }

	// A z_attenuation of 0.0f collapses the distance into the X-Y plane
	F32 getDistanceToDestination(const LLVector3d& pos_global,
								 F32 z_attenuation = 0.5f) const;

	void clearLocationSelection(bool clear_ui = false);
	void clearAvatarSelection(bool clear_ui = false);
	void clearLandmarkSelection(bool clear_ui = false);

	// Adjust the maximally zoomed out limit of the zoom slider so you can
	// see the whole world, plus a little.
	void adjustZoomSliderBounds();

	// Catch changes in the sim list
	void updateSims(bool found_null_sim);

	// Teleport to the tracked item, if there is one
	void teleport();

protected:
	static void onGoHome(void*);

	static void onLandmarkComboPrearrange(LLUICtrl*, void* data);
	static void onLandmarkComboCommit(LLUICtrl*, void* data);

	static void onAvatarComboPrearrange(LLUICtrl*, void* data);
	static void onAvatarComboCommit(LLUICtrl*, void* data);

	static void onTabChanged(void* data, bool from_click);

	static void onComboTextEntry(LLLineEditor*, void*);

	static void onSearchTextEntry(const std::string&, void*);

	static void onClearBtn(void*);
	static void onClickTeleportBtn(void*);
	static void onShowTargetBtn(void*);
	static void onShowAgentBtn(void*);
	static void onCopySLURL(void*);

	void requestParcelInfo(const LLVector3d& pos_global);

	void centerOnTarget(bool animate);
	void updateLocationSpinners(const LLVector3d& pos,
		                        LLVector3* local_pos = NULL);
	void updateLocation();

	void buildLandmarkIDLists();
	void setLandmarkVisited();

	void buildAvatarIDList();

	static void onTeleportArriving();

	static void updateSearchEnabled();
	static void onLocationFocusChanged(LLFocusableElement*, void*);
	static void onLocationCommit(void* userdata);
	static void onCommitLocation(LLUICtrl*, void* userdata);
	static void onCommitSearchResult(LLUICtrl*, void* userdata);

	void cacheLandmarkPosition();

	class LLReloadAllTiles final : public LLMemberListener<LLFloaterWorldMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLReloadTile final : public LLMemberListener<LLFloaterWorldMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLSaveMapTile final : public LLMemberListener<LLFloaterWorldMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLSaveSculpt final : public LLMemberListener<LLFloaterWorldMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	static void saveTileCallback(HBFileSelector::ESaveFilter type,
								 std::string& filename, void*);

	static void saveSculptCallback(HBFileSelector::ESaveFilter type,
								   std::string& filename, void*);

	static void onTileLoadedForSave(bool success,
									LLViewerFetchedTexture* src_vi,
									LLImageRaw* src, LLImageRaw* aux,
									S32 discard_level, bool final,
									void* userdata);
protected:
	LLTabContainer*				mTabs;

	LLHandle<LLView>			mPopupMenuHandle;

	// Sets sMapScale, in pixels per region
	F32							mCurZoomVal;
	LLFrameTimer				mZoomTimer;

	std::vector<LLUUID>			mLandmarkAssetIDList;
	std::vector<LLUUID>			mLandmarkItemIDList;

	static const LLUUID			sHomeID;

	LLInventoryModel*			mInventory;
	LLInventoryObserver*		mInventoryObserver;
	LLFriendObserver*			mFriendObserver;
	LLMapParcelInfoObserver*	mParcelInfoObserver;

	boost::signals2::connection	mTeleportArrivingConnection;

	std::string					mCompletingRegionName;
	std::string					mLastRegionName;

	LLVector3d					mTrackedLocation;
	LLTracker::ETrackingStatus	mTrackedStatus;

	LLVector3d					mRequestedGlobalPos;

	LLSearchEditor*				mLocationEditor;
	LLSpinCtrl*					mSpinX;
	LLSpinCtrl*					mSpinY;
	LLSpinCtrl*					mSpinZ;
	LLComboBox*					mLandmarkCombo;
	LLComboBox*					mFriendCombo;
	LLScrollListCtrl*			mSearchResultsList;
	LLIconCtrl*					mEventsMatureIcon;
	LLIconCtrl*					mEventsAdultIcon;
	LLIconCtrl*					mAvatarIcon;
	LLIconCtrl*					mLandmarkIcon;
	LLIconCtrl*					mLocationIcon;
	LLCheckBoxCtrl*				mEventsMatureCheck;
	LLCheckBoxCtrl*				mEventsAdultCheck;
	LLButton*					mGoHomeButton;
	LLButton*					mTeleportButton;
	LLButton*					mShowDestinationButton;
	LLButton*					mCopySLURLButton;
	LLSliderCtrl*				mZoomSlider;

	LLUUID						mTrackedAvatarId;
	std::string					mTrackedSimName;
	std::string					mSLURL;

	bool						mFirstOpen;

	bool						mWaitingForTracker;
	bool						mExactMatch;

	bool						mIsClosing;
	bool						mSetToUserPosition;

public:
	static LLSimInfo*							sRightClickedSimInfo;
	static LLPointer<LLViewerFetchedTexture>	sImageToSave;
	static LLSurface*							sSurfaceToMap;
	static U32									sRegionWidth;
	static bool									sSaveAsDecal;
	static bool									sSaveAsSpheric;
	static std::string							sSaveFileName;
};

extern LLFloaterWorldMap* gFloaterWorldMapp;

#endif
