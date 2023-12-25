/**
 * @file llfloaterworldmap.cpp
 * @brief LLFloaterWorldMap class implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
 * Original authors: James Cook, Tom Yedwab
 * Copyright (c) 2009-2021, Henri Beauchamp.
 * Changes by Henri Beauchamp:
 *  - Backported web map tiles support from v2/3 viewers while keeping the old
 *    terrain-only map support.
 *  - Adapted the code for OpenSim variable region size support.
 *  - Added code to save map tiles images and their 3D terrain as scuplties.
 *  - Allowed to keep the map tiles in memory when the floater is closed.
 *  - Added context menu entries to reload a map given tile or all map tiles.
 *  - Allowed to keep both objects and terrain map tiles in memory (avoids
 *    seeing the map tiles fully reloaded at each world map tab change).
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterworldmap.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcallbacklist.h"
#include "llcombobox.h"
#include "lldraghandle.h"
#include "llgl.h"
#include "lliconctrl.h"
#include "llimagetga.h"
#include "lllineeditor.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llavatartracker.h"
#include "llcommandhandler.h"
#include "llfirstuse.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llinventorymodelfetch.h"
#include "lllandmarklist.h"
#include "llpanelworldmap.h"
#include "llpreviewlandmark.h"
#include "llregionhandle.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llslurl.h"
#include "llsurface.h"
#include "lltracker.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexture.h"
#include "llviewerwindow.h"
#include "llweb.h"
#include "llworld.h"
#include "llworldmap.h"

using namespace LLOldEvents;

//---------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------
constexpr F32 MAP_ZOOM_TIME = 0.2f;

enum EPanDirection
{
	PAN_UP,
	PAN_DOWN,
	PAN_LEFT,
	PAN_RIGHT
};

constexpr F32 ZOOM_MAX = 0.f;
constexpr F32 SIM_COORD_DEFAULT = 128.f;
constexpr F32 GODLY_TELEPORT_HEIGHT = 200.f;

//---------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------

// Instance created in LLViewerWindow::initWorldUI()
LLFloaterWorldMap* gFloaterWorldMapp = NULL;

// Handles secondlife:///app/worldmap/{NAME}/{COORDS} URLs
class LLWorldMapHandler final : public LLCommandHandler
{
public:
	LLWorldMapHandler()
	:	LLCommandHandler("worldmap", UNTRUSTED_THROTTLE)
	{
	}

	bool canHandleUntrusted(const LLSD&, const LLSD&, LLMediaCtrl*,
							const std::string& nav_type) override
	{
		// With UNTRUSTED_THROTTLE this will cause "clicked" to pass, 
		// "external" to be throttled, and the rest to be blocked.
		return nav_type == "clicked" || nav_type == "external";
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		size_t count = params.size();
		if (!count)
		{
			// Support the secondlife:///app/worldmap SLapp
			LLFloaterWorldMap::show(NULL, true);
			return true;
		}

		// Support the secondlife:///app/worldmap/{LOCATION}/{COORDS} SLapp
		const std::string region_name = LLURI::unescape(params[0].asString());
		S32 x = count > 1 ? params[1].asInteger() : 128;
		S32 y = count > 2 ? params[2].asInteger() : 128;
		S32 z = count > 3 ? params[3].asInteger() : 0;

		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->trackURL(region_name, x, y, z);
		}
		LLFloaterWorldMap::show(NULL, true);

		return true;
	}
};
LLWorldMapHandler gWorldMapHandler;

// SocialMap handler secondlife:///app/maptrackavatar/id
class LLMapTrackAvatarHandler final : public LLCommandHandler
{
public:
	LLMapTrackAvatarHandler()
	:	LLCommandHandler("maptrackavatar", UNTRUSTED_THROTTLE)
	{
	}

	bool canHandleUntrusted(const LLSD& params, const LLSD&, LLMediaCtrl*,
							const std::string& nav_type) override
	{
		if (!params.size())
		{
			return true;	// Do not block here; it will fail in handle().
		}

		// With UNTRUSTED_THROTTLE this will cause "clicked" to pass, 
		// "external" to be throttled, and the rest to be blocked.
		return nav_type == "clicked" || nav_type == "external";
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		// Make sure we have some parameters
		if (!params.size())
		{
			return false;
		}

		// Get the ID
		LLUUID id;
		if (!id.set(params[0], false))
		{
			return false;
		}

		if (gFloaterWorldMapp && gCacheNamep)
		{
			std::string name;
			gCacheNamep->getFullName(id, name);
			gFloaterWorldMapp->trackAvatar(id, name);
			LLFloaterWorldMap::show(NULL, true);
		}
		return true;
	}
};
LLMapTrackAvatarHandler gMapTrackAvatar;

class LLMapInventoryObserver final : public LLInventoryObserver
{
public:
	LLMapInventoryObserver() 			{}
	~LLMapInventoryObserver() override	{}

	void changed(U32 mask) override;
};

void LLMapInventoryObserver::changed(U32 mask)
{
	// If there is a change we are interested in.
	constexpr U32 interests_mask = LLInventoryObserver::CALLING_CARD |
								   LLInventoryObserver::ADD |
								   LLInventoryObserver::REMOVE;
	if (gFloaterWorldMapp && (mask & interests_mask) != 0)
	{
		gFloaterWorldMapp->inventoryChanged();
	}
}

class LLMapFriendObserver final : public LLFriendObserver
{
public:
	LLMapFriendObserver() = default;

	void changed(U32 mask) override
	{
		// If there is a change we are interested in.
		if (gFloaterWorldMapp &&
			(mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE |
					 LLFriendObserver::ONLINE |
					 LLFriendObserver::POWERS)) != 0)
		{
			gFloaterWorldMapp->friendsChanged();
		}
	}
};

class LLMapParcelInfoObserver final : public LLParcelInfoObserver
{
protected:
	LOG_CLASS(LLMapParcelInfoObserver);

public:
	LLMapParcelInfoObserver(const LLVector3d& pos_global)
	:	LLParcelInfoObserver(),
		mPosGlobal(pos_global)
	{
	}

	~LLMapParcelInfoObserver() override
	{
		// Remove old observer, if any.
		gViewerParcelMgr.removeInfoObserver(mParcelID, this);
	}

	void processParcelInfo(const LLParcelData& parcel_data) override
	{
		if (parcel_data.mParcelId != mParcelID) return;

		// Remove old observer, if any.
		gViewerParcelMgr.removeInfoObserver(mParcelID, this);

		if (gFloaterWorldMapp && !parcel_data.mName.empty() &&
			gTracker.getTrackedPositionGlobal() == mPosGlobal &&
			gTracker.getTrackedLocationType() == LLTracker::LOCATION_NOTHING &&
			gTracker.getTrackingStatus() == LLTracker::TRACKING_LOCATION &&
			!gFloaterWorldMapp->getRequestedParcelInfoGlobalPos().isExactlyZero())
		{
			gFloaterWorldMapp->trackLocation(mPosGlobal, parcel_data.mName);
		}
	}

	void setParcelID(const LLUUID& parcel_id) override
	{
		// Remove old observer, if any.
		gViewerParcelMgr.removeInfoObserver(mParcelID, this);

		// Set new parcel Id, observe and request info.
		mParcelID = parcel_id;
		gViewerParcelMgr.addInfoObserver(mParcelID, this);
		gViewerParcelMgr.sendParcelInfoRequest(mParcelID);
	}

	void setErrorStatus(S32 status, const std::string& reason) override
	{
		gViewerParcelMgr.removeInfoObserver(mParcelID, this);
		llwarns << "Could not retrieve parcel info. Status: " << status
				<< " -  Reason: " << reason << llendl;
	}

private:
	LLVector3d	mPosGlobal;
	LLUUID		mParcelID;
};

//---------------------------------------------------------------------------
// Statics
//---------------------------------------------------------------------------
LLSimInfo* LLFloaterWorldMap::sRightClickedSimInfo = NULL;
LLPointer<LLViewerFetchedTexture> LLFloaterWorldMap::sImageToSave;
LLSurface* LLFloaterWorldMap::sSurfaceToMap = NULL;
U32 LLFloaterWorldMap::sRegionWidth = (U32)REGION_WIDTH_METERS;
bool LLFloaterWorldMap::sSaveAsDecal = false;
bool LLFloaterWorldMap::sSaveAsSpheric = false;
std::string LLFloaterWorldMap::sSaveFileName;

// Used as a pretend asset and inventory id to mean "landmark at my home
// location."
const LLUUID LLFloaterWorldMap::sHomeID("10000000-0000-0000-0000-000000000001");

//---------------------------------------------------------------------------
// Construction and destruction
//---------------------------------------------------------------------------

LLFloaterWorldMap::LLFloaterWorldMap()
:	LLFloater("map"),
	mFirstOpen(true),
	mInventory(NULL),
	mInventoryObserver(NULL),
	mFriendObserver(NULL),
	mParcelInfoObserver(NULL),
	mWaitingForTracker(false),
	mExactMatch(false),
	mIsClosing(false),
	mSetToUserPosition(true),
	mTrackedStatus(LLTracker::TRACKING_NOTHING)
{
	LLCallbackMap::map_t factory_map;
	factory_map["objects_mapview"] = LLCallbackMap(createWorldMapView,
												   (void*)0);
	factory_map["terrain_mapview"] = LLCallbackMap(createWorldMapView,
												   (void*)1);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_world_map.xml",
												 &factory_map, false);

	// Register event listeners for popup menu. HB
	(new LLReloadAllTiles())->registerListener(this,
											   "WorldMap.ReloadAllTiles");
	(new LLReloadTile())->registerListener(this, "WorldMap.ReloadTile");
	(new LLSaveMapTile())->registerListener(this, "WorldMap.SaveMapTile");
	(new LLSaveSculpt())->registerListener(this, "WorldMap.SaveSculpt");

	LLMenuGL* menu =
		LLUICtrlFactory::getInstance()->buildMenu("menu_world_map.xml", this);
	if (!menu)
	{
		menu = new LLMenuGL(LLStringUtil::null);
	}
	menu->setVisible(false);
	mPopupMenuHandle = menu->getHandle();
}

//static
void* LLFloaterWorldMap::createWorldMapView(void* data)
{
	U32 layer = (intptr_t)data;
	return new LLPanelWorldMap(llformat("map layer %u", layer),
							   LLRect(0, 300, 400, 0), layer);
}

bool LLFloaterWorldMap::postBuild()
{
	mTabs = getChild<LLTabContainer>("maptab");

	LLPanel* panel = mTabs->getChild<LLPanel>("objects_mapview");
	mTabs->setTabChangeCallback(panel, onTabChanged);
	mTabs->setTabUserData(panel, this);

	panel = mTabs->getChild<LLPanel>("terrain_mapview");
	mTabs->setTabChangeCallback(panel, onTabChanged);
	mTabs->setTabUserData(panel, this);

	mFriendCombo = getChild<LLComboBox>("friend combo");
	mFriendCombo->setCommitCallback(onAvatarComboCommit);
	mFriendCombo->setCallbackUserData(this);
	mFriendCombo->selectFirstItem();
	mFriendCombo->setPrearrangeCallback(onAvatarComboPrearrange);
	mFriendCombo->setTextEntryCallback(onComboTextEntry);

	mEventsMatureIcon = getChild<LLIconCtrl>("events_mature_icon");
	mEventsAdultIcon = getChild<LLIconCtrl>("events_adult_icon");
	mEventsMatureCheck = getChild<LLCheckBoxCtrl>("event_mature_chk");
	mEventsAdultCheck = getChild<LLCheckBoxCtrl>("event_adult_chk");

	mAvatarIcon = getChild<LLIconCtrl>("avatar_icon");
	mLandmarkIcon = getChild<LLIconCtrl>("landmark_icon");
	mLocationIcon = getChild<LLIconCtrl>("location_icon");

	childSetAction("DoSearch", onLocationCommit, this);

	mLocationEditor = getChild<LLSearchEditor>("location");
	mLocationEditor->setSearchCallback(onSearchTextEntry, this);
	mLocationEditor->setFocusChangedCallback(onLocationFocusChanged, this);

	mSearchResultsList = getChild<LLScrollListCtrl>("search_results");
	mSearchResultsList->setCommitCallback(onCommitSearchResult);
	mSearchResultsList->setCallbackUserData(this);
	mSearchResultsList->setDoubleClickCallback(onClickTeleportBtn);

	mSpinX = getChild<LLSpinCtrl>("spin x");
	mSpinX->setCommitCallback(onCommitLocation);
	mSpinX->setCallbackUserData(this);
	mSpinY = getChild<LLSpinCtrl>("spin y");
	mSpinY->setCommitCallback(onCommitLocation);
	mSpinY->setCallbackUserData(this);
	mSpinZ = getChild<LLSpinCtrl>("spin z");
	mSpinZ->setCommitCallback(onCommitLocation);
	mSpinZ->setCallbackUserData(this);

	mLandmarkCombo = getChild<LLComboBox>("landmark combo");
	mLandmarkCombo->setCommitCallback(onLandmarkComboCommit);
	mLandmarkCombo->setCallbackUserData(this);
	mLandmarkCombo->selectFirstItem();
	mLandmarkCombo->setPrearrangeCallback(onLandmarkComboPrearrange);
	mLandmarkCombo->setTextEntryCallback(onComboTextEntry);

	mGoHomeButton = getChild<LLButton>("Go Home");
	mGoHomeButton->setClickedCallback(onGoHome, this);

	mTeleportButton = getChild<LLButton>("Teleport");
	mTeleportButton->setClickedCallback(onClickTeleportBtn, this);

	mShowDestinationButton = getChild<LLButton>("Show Destination");
	mShowDestinationButton->setClickedCallback(onShowTargetBtn, this);

	childSetAction("Show My Location", onShowAgentBtn, this);
	childSetAction("Clear", onClearBtn, this);

	mCopySLURLButton = getChild<LLButton>("copy_slurl");
	mCopySLURLButton->setClickedCallback(onCopySLURL, this);

	mCurZoomVal = logf(LLPanelWorldMap::sMapScale) / (F_LN2 * 256.f);

	mZoomSlider = getChild<LLSliderCtrl>("zoom slider");
	mZoomSlider->setValue(mCurZoomVal);

	setDefaultBtn((LLButton*)NULL);

	mZoomTimer.stop();

	mTeleportArrivingConnection =
		gViewerParcelMgr.setTPArrivingCallback(boost::bind(&LLFloaterWorldMap::onTeleportArriving));

	return true;
}

//virtual
LLFloaterWorldMap::~LLFloaterWorldMap()
{
	mTeleportArrivingConnection.disconnect();

	// All cleaned up by LLView destructor
	mTabs = NULL;

	clearParcelInfoRequest();

	// Inventory deletes all observers on shutdown
	mInventory = NULL;
	mInventoryObserver = NULL;

	// Avatar tracker will delete this for us.
	mFriendObserver = NULL;

	llinfos << "World map destroyed" << llendl;
	gFloaterWorldMapp = NULL;
}

//virtual
void LLFloaterWorldMap::onOpen()
{
	if (mFirstOpen)
	{
		mFirstOpen = false;
		// Reposition floater from saved settings
		LLRect rect = gSavedSettings.getRect("FloaterWorldMapRect2");
		reshape(rect.getWidth(), rect.getHeight(), false);
		setRect(rect);
		// Sadly, OpenSim grids do not provide terrain-only tiles. HB
		if (!gIsInSecondLife &&
			!gSavedSettings.getBool("OSWorldMapHasTerrain"))
		{
			LLPanel* panel = mTabs->getChild<LLPanel>("terrain_mapview", true,
													  false);
			if (panel)	// Paranoia
			{
				mTabs->removeTabPanel(panel);
				delete panel;
			}
		}
	}
}

//virtual
void LLFloaterWorldMap::onClose(bool app_quitting)
{
	setVisible(false);
}

//static
void LLFloaterWorldMap::onTeleportArriving()
{
	if (gFloaterWorldMapp && !gFloaterWorldMapp->isMinimized() &&
		gSavedSettings.getBool("HideFloatersOnTPSuccess"))
	{
		hide(NULL);
	}
}

//static
void LLFloaterWorldMap::show(void*, bool center_on_target)
{
	if (!gFloaterWorldMapp) return;

//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShowworldmap || gRLInterface.mContainsShowloc))
	{
		return;
	}
//mk

	bool was_visible = gFloaterWorldMapp->getVisible();

	gFloaterWorldMapp->mIsClosing = false;
	gFloaterWorldMapp->open();

	LLPanelWorldMap* panelp =
		(LLPanelWorldMap*)gFloaterWorldMapp->mTabs->getCurrentPanel();
	if (!panelp) return; // Paranoia (or bad world map menu XML...)
	panelp->clearLastClick();

	if (!was_visible)
	{
		// Reset pan on show, so it centers on you again
		if (!center_on_target)
		{
			LLPanelWorldMap::setPan(0, 0, true);
		}
		// Reload the agent positions when we show the window
		gWorldMap.eraseItems();

		// Reload any maps that may have changed
		gWorldMap.clearSimFlags();

		const U32 panel_num = gFloaterWorldMapp->mTabs->getCurrentPanelIndex();
		constexpr bool request_from_sim = true;
		gWorldMap.setCurrentLayer(panel_num, request_from_sim);

		// We may already have a bounding box for the regions of the world, so
		// use that to adjust the view.
		gFloaterWorldMapp->adjustZoomSliderBounds();

		// Could be first show
		LLFirstUse::useMap();

		// Start speculative download of landmarks
		const LLUUID& lm_folder_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);
		LLInventoryModelFetch::getInstance()->start(lm_folder_id);

		gFloaterWorldMapp->mLocationEditor->setFocus(true);
		gFocusMgr.triggerFocusFlash();

		gFloaterWorldMapp->buildAvatarIDList();
		gFloaterWorldMapp->buildLandmarkIDLists();

		// If nothing is being tracked, set flag so the user position will be
		// found
		gFloaterWorldMapp->mSetToUserPosition =
			(gTracker.getTrackingStatus() == LLTracker::TRACKING_NOTHING);

		panelp->updateVisibleBlocks();
	}

	if (center_on_target)
	{
		gFloaterWorldMapp->centerOnTarget(false);
	}
}

//static
void LLFloaterWorldMap::reloadIcons(void*)
{
	gWorldMap.eraseItems();
	gWorldMap.sendMapLayerRequest();
}

//static
void LLFloaterWorldMap::toggle(void*)
{
	if (!gFloaterWorldMapp) return;

	bool visible = gFloaterWorldMapp->getVisible();
	if (!visible)
	{
		show(NULL, false);
	}
	else
	{
		gFloaterWorldMapp->mIsClosing = true;
		gFloaterWorldMapp->close();
	}
}

//static
void LLFloaterWorldMap::hide(void*)
{
	if (gFloaterWorldMapp)
	{
		gFloaterWorldMapp->mIsClosing = true;
		gFloaterWorldMapp->close();
	}
}

//virtual
void LLFloaterWorldMap::setVisible(bool visible)
{
	LLFloater::setVisible(visible);
	gSavedSettings.setBool("ShowWorldMap", visible);
	if (!visible && !gSavedSettings.getBool("KeepWorldMapTilesOnClose"))
	{
		// While we are not visible, discard the overlay images we are using
		gWorldMap.clearImageRefs();
	}
}

bool LLFloaterWorldMap::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (!isMinimized() && isFrontmost())
	{
		LLRect area = mSearchResultsList->getRect();
		if (!area.pointInRect(x, y))
		{
			F32 slider_value = mZoomSlider->getValue().asReal();
			slider_value += ((F32)clicks * -0.3333f);
			mZoomSlider->setValue(LLSD(slider_value));
			return true;
		}
	}

	return LLFloater::handleScrollWheel(x, y, clicks);
}

//virtual
bool LLFloaterWorldMap::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		gViewerWindowp->showCursor();
		gFocusMgr.setMouseCapture(NULL);
		return true;
	}
	return false;
}

//virtual
bool LLFloaterWorldMap::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	LLPanelWorldMap* panelp =
		(LLPanelWorldMap*)gFloaterWorldMapp->mTabs->getCurrentPanel();
	if (!panelp)	// No panel... Bad floater UI xml ?
	{
		return LLFloater::handleRightMouseDown(x, y, mask);
	}

	// When the click is out of the map panel, let the UI handle it
	LLRect panel_rect = panelp->getRect();
	if (x > panel_rect.mRight + panel_rect.mLeft)
	{
		return LLFloater::handleRightMouseDown(x, y, mask);
	}

	if (sImageToSave.notNull() || sSurfaceToMap)
	{
		// There is already a tile being saved, ignore this event
		return true;
	}

	// Find the clicked global position in the grid
	LLVector3d loc = panelp->viewPosToGlobal(x, y);

	// Find and save the sim info for the right-clicked tile
	sRightClickedSimInfo = gWorldMap.simInfoFromPosGlobal(loc);
	if (!sRightClickedSimInfo)
	{
		return false; // No sim here: abort
	}

	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
	if (menu)
	{
		menu->buildDrawLabels();
		menu->updateParent(gMenuHolderp);
		LLMenuGL::showPopup(this, menu, x, y);
	}

	return true;
}

//static
bool LLFloaterWorldMap::LLReloadAllTiles::handleEvent(LLPointer<LLEvent>,
													  const LLSD&)
{
	gWorldMap.clearImageRefs(gWorldMap.getCurrentLayer());
	gWorldMap.clearSimFlags();
	return true;
}

//static
bool LLFloaterWorldMap::LLReloadTile::handleEvent(LLPointer<LLEvent>,
												  const LLSD&)
{
	if (LLFloaterWorldMap::sRightClickedSimInfo)
	{
		U64 handle = LLFloaterWorldMap::sRightClickedSimInfo->mHandle;
		gWorldMap.forceUpdateRegion(handle);
	}
	return true;
}

//static
void LLFloaterWorldMap::saveTileCallback(HBFileSelector::ESaveFilter type,
										 std::string& filename, void*)
{
	if (filename.empty())
	{
		sImageToSave = NULL;
		return;
	}

	if (sImageToSave.isNull())
	{
		return;
	}

	sSaveFileName = filename;
	LLStringUtil::toLower(filename);
	if (filename.rfind(".tga") != filename.length() - 4)
	{
		sSaveFileName += ".tga";
	}

	// Re-fetch the raw image if the old one is removed.
	sImageToSave->forceToSaveRawImage(0);
	sImageToSave->setLoadedCallback(onTileLoadedForSave, 0, true, false, NULL,
									NULL);
}

//static
void LLFloaterWorldMap::onTileLoadedForSave(bool success,
											LLViewerFetchedTexture* src_vi,
											LLImageRaw* src, LLImageRaw*,
											S32 discard_level, bool is_final,
											void*)
{
	if (is_final && success)
	{
		// This is needed to avoid seeing the raw image vanishing on us !
		LLPointer<LLImageRaw> source = src;

		if (sSaveAsDecal)
		{
			// Make a duplicate to keep the original raw image untouched:
			source = source->duplicate();

			LLPointer<LLImageRaw> decal;
			bool success = source->scale(240, 240, true);
			if (success)
			{
				decal = new LLImageRaw(256, 256, source->getComponents());
				if (decal.isNull())
				{
					success = false;
				}
			}

			if (success)
			{
				decal->fill(LLColor4U(0, 0, 0, 1));
				success = decal->setSubImage(8, 8, 240, 240,
											 source->getData());
				source = decal.get();
			}

			if (!success)
			{
				gNotifications.add("CannotRescaleImage");
				sImageToSave = NULL;
				return;
			}
		}

		LLSD args;
		args["FILE"] = sSaveFileName;

		LLPointer<LLImageTGA> image_tga = new LLImageTGA;
		if (!image_tga->encode(source))
		{
			gNotifications.add("CannotEncodeFile", args);
		}
		else if (!image_tga->save(sSaveFileName))
		{
			gNotifications.add("CannotWriteFile", args);
		}
		else
		{
			gNotifications.add("FileSaved", args);
		}

		sImageToSave = NULL;
	}
	else if (!success)
	{
		gNotifications.add("CannotDownloadFile");
		sImageToSave = NULL;
	}
}

//static
bool LLFloaterWorldMap::LLSaveMapTile::handleEvent(LLPointer<LLEvent>,
												   const LLSD& userdata)
{
	LLSimInfo* infop = LLFloaterWorldMap::sRightClickedSimInfo;
	if (!infop) return true;

	// Get the image for that sim
	LLFloaterWorldMap::sImageToSave =
		infop->mCurrentImage[gWorldMap.getCurrentLayer()];
	if (!LLFloaterWorldMap::sImageToSave) return true;

	// Call the file selector
	std::string suggestion = infop->mName;
	LLFloaterWorldMap::sSaveAsDecal = userdata.asInteger() > 0;
	if (LLFloaterWorldMap::sSaveAsDecal)
	{
		suggestion += "Decal";
	}
	HBFileSelector::saveFile(HBFileSelector::FFSAVE_TGA, suggestion,
							 saveTileCallback);

	return true;
}

//static
void LLFloaterWorldMap::saveSculptCallback(HBFileSelector::ESaveFilter type,
										   std::string& filename, void*)
{
	if (filename.empty())
	{
		sSurfaceToMap = NULL;
		return;
	}

	if (!sSurfaceToMap)
	{
		return;
	}

	constexpr S32 SCULPT_PIXELS = 64;
	LLPointer<LLImageRaw> sculpt = new LLImageRaw(SCULPT_PIXELS,
												  SCULPT_PIXELS, 3);
	if (sculpt.isNull())
	{
		llwarns << "Out of memory creating a 64x64 sculpt map !" << llendl;
		return;
	}
	U8* data = sculpt->getData();

	// Get the region height data to compute bottom altitude and Z scale

	S32 min_z = sSurfaceToMap->getMinZ();
	S32 max_z = sSurfaceToMap->getMaxZ();
	S32 scale = (max_z - min_z) / 256 + 1;
	if (max_z <= 255)
	{
		min_z = 0;
	}

	// Construct the final filename

	sSaveFileName = filename;
	// remove the .tga extension, if any
	LLStringUtil::toLower(filename);
	size_t i = filename.rfind(".tga");
	if (i == filename.length() - 4)
	{
		sSaveFileName = sSaveFileName.substr(0, i);
	}
	// Adjust the name to add the scale and minimum Z data
	if (scale != 1)
	{
		sSaveFileName += llformat("_S%d", scale);
	}
	if (min_z != 0)
	{
		sSaveFileName += llformat("_B%d", min_z);
	}
	// Add the .tga extension
	sSaveFileName += ".tga";

	// Fill-up the sculpt map

	S32 increment = sRegionWidth / SCULPT_PIXELS;
	// To get the altitude at the center of each land patch:
	S32 delta = increment / 2;
	for (S32 y = 0; y < (S32)sRegionWidth - 1; y += increment)
	{
		for (S32 x = 0; x < (S32)sRegionWidth - 1; x += increment)
		{
			*data++ = (U8)x;
			*data++ = (U8)y;
			if (sSaveAsSpheric &&
				(x < 2 || y < 2 || x >= (S32)sRegionWidth - 2 * increment ||
				 y >= (S32)sRegionWidth - 2 * increment))
			{
				*data++ = 0;
			}
			else
			{
				S32 height = sSurfaceToMap->resolveHeightRegion(x + delta,
																y + delta);
				*data++ = (U8)((height - min_z) / scale);
			}
		}
	}

	// Save the sculpt map now...

	LLSD args;
	args["FILE"] = sSaveFileName;

	LLPointer<LLImageTGA> image_tga = new LLImageTGA;
	if (!image_tga->encode(sculpt))
	{
		gNotifications.add("CannotEncodeFile", args);
	}
	else if (!image_tga->save(sSaveFileName))
	{
		gNotifications.add("CannotWriteFile", args);
	}
	else
	{
		gNotifications.add("FileSaved", args);
	}

	sSurfaceToMap = NULL;
}

//static
bool LLFloaterWorldMap::LLSaveSculpt::handleEvent(LLPointer<LLEvent>,
												  const LLSD& userdata)
{
	LLSimInfo* infop = LLFloaterWorldMap::sRightClickedSimInfo;
	if (!infop) return true;

	// Get the surface for that sim
	U64 handle = infop->mHandle;
	LLViewerRegion* regionp = gWorld.getRegionFromHandle(handle);
	if (!regionp)
	{
		gNotifications.add("NoDataForRegion");
		return true;
	}

	LLFloaterWorldMap::sSurfaceToMap = &regionp->getLand();
	if (!LLFloaterWorldMap::sSurfaceToMap)
	{
		gNotifications.add("NoDataForRegion");
		return true;
	}

	LLFloaterWorldMap::sRegionWidth = (U32)regionp->getWidth();

	LLFloaterWorldMap::sSaveAsSpheric = userdata.asInteger() > 0;

	// Call the file selector
	std::string suggestion = infop->mName + "Sculpt";
	HBFileSelector::saveFile(HBFileSelector::FFSAVE_TGA, suggestion,
							 saveSculptCallback);

	return true;
}

//virtual
void LLFloaterWorldMap::draw()
{
//MK
	// Fast enough that it can be kept here
	if (gRLenabled &&
		(gRLInterface.mContainsShowworldmap || gRLInterface.mContainsShowloc))
	{
		setVisible(false);
		return;
	}
//mk

	// Hide/Show Mature Events controls
	bool can_access_mature = gAgent.canAccessMature();
	bool adult_enabled = gAgent.canAccessAdult();
	mEventsMatureIcon->setVisible(can_access_mature);
	mEventsAdultIcon->setVisible(can_access_mature);
	mEventsMatureCheck->setVisible(can_access_mature);
	mEventsAdultCheck->setVisible(can_access_mature);
	mEventsAdultCheck->setEnabled(adult_enabled);
	if (!adult_enabled)
	{
		mEventsAdultCheck->setValue(false);
	}

	// On orientation island, users do not have a home location yet, so
	// do not let them teleport "home".  It dumps them in an often-crowed
	// welcome area (infohub) and they get confused. JC
	LLViewerRegion* regionp = gAgent.getRegion();
	bool agent_on_prelude = regionp && regionp->isPrelude();
	bool enable_go_home = gAgent.isGodlike() || !agent_on_prelude;
	mGoHomeButton->setEnabled(enable_go_home);

	updateLocation();

	LLTracker::ETrackingStatus tracking_status = gTracker.getTrackingStatus();

	if (tracking_status == LLTracker::TRACKING_AVATAR)
	{
		mAvatarIcon->setColor(LLUI::sTrackColor);
	}
	else
	{
		mAvatarIcon->setColor(LLUI::sDisabledTrackColor);
	}

	if (tracking_status == LLTracker::TRACKING_LANDMARK)
	{
		mLandmarkIcon->setColor(LLUI::sTrackColor);
	}
	else
	{
		mLandmarkIcon->setColor(LLUI::sDisabledTrackColor);
	}

	if (tracking_status == LLTracker::TRACKING_LOCATION)
	{
		mLocationIcon->setColor(LLUI::sTrackColor);
	}
	else if (!mCompletingRegionName.empty())
	{
		F32 seconds = LLTimer::getElapsedSeconds();
		F32 value = fmodf(seconds, 2);
		value = 0.5f + 0.5f * cosf(value * F_PI);
		LLColor4 loading_color(0.f, value * 0.5f, value, 1.f);
		mLocationIcon->setColor(loading_color);
	}
	else
	{
		mLocationIcon->setColor(LLUI::sDisabledTrackColor);
	}

	// check for completion of tracking data
	if (mWaitingForTracker)
	{
		centerOnTarget(true);
	}

	bool is_tracking = tracking_status != LLTracker::TRACKING_NOTHING;
	mTeleportButton->setEnabled(is_tracking);
#if 0
	childSetEnabled("Clear", is_tracking);
#endif

	mShowDestinationButton->setEnabled(is_tracking ||
									   gWorldMap.mIsTrackingUnknownLocation);

	mCopySLURLButton->setEnabled(mSLURL.size() > 0);

	setMouseOpaque(true);
	getDragHandle()->setMouseOpaque(true);

	// RN: snaps to zoom value because interpolation caused jitter in the text
	// rendering
	if (!mZoomTimer.getStarted() &&
		mCurZoomVal != (F32)mZoomSlider->getValue().asReal())
	{
		mZoomTimer.start();
	}
	F32 interp = mZoomTimer.getElapsedTimeF32() / MAP_ZOOM_TIME;
	if (interp > 1.f)
	{
		interp = 1.f;
		mZoomTimer.stop();
	}
	mCurZoomVal = lerp(mCurZoomVal, (F32)mZoomSlider->getValue().asReal(),
					   interp);
	F32 map_scale = 256.f * powf(2.f, mCurZoomVal);
	LLPanelWorldMap::setScale(map_scale);

	LLFloater::draw();
}

//-------------------------------------------------------------------------
// Internal utility functions
//-------------------------------------------------------------------------

void LLFloaterWorldMap::trackAvatar(const LLUUID& avatar_id,
									const std::string& name)
{
	clearParcelInfoRequest();

	buildAvatarIDList();
	if (mFriendCombo->setCurrentByID(avatar_id) || gAgent.isGodlike())
	{
		// *HACK: Adjust Z values automatically for liaisons & gods so
		// they swoop down when they click on the map. Requested
		// convenience.
		if (gAgent.isGodlike())
		{
			mSpinZ->setValue(GODLY_TELEPORT_HEIGHT);
		}
		// Do not re-request info if we already have it or we would not have it
		// in time to teleport
		if (mTrackedStatus != LLTracker::TRACKING_AVATAR ||
			mTrackedAvatarId != avatar_id)
		{
			mTrackedStatus = LLTracker::TRACKING_AVATAR;
			mTrackedAvatarId = avatar_id;
			gTracker.trackAvatar(avatar_id, name);
		}
	}
	else
	{
		gTracker.stopTracking();
	}
	setDefaultBtn(mTeleportButton);
}

void LLFloaterWorldMap::trackLandmark(const LLUUID& landmark_item_id)
{
	clearParcelInfoRequest();

	buildLandmarkIDLists();
	bool found = false;
	S32 idx;
	S32 count = mLandmarkItemIDList.size();
	for (idx = 0; idx < count; ++idx)
	{
		if (mLandmarkItemIDList[idx] == landmark_item_id)
		{
			found = true;
			break;
		}
	}

	if (found && mLandmarkCombo->setCurrentByID(landmark_item_id))
	{
		const LLUUID& asset_id = mLandmarkAssetIDList[idx];
		mTrackedStatus = LLTracker::TRACKING_LANDMARK;
		gTracker.trackLandmark(asset_id, mLandmarkItemIDList[idx],
							   mLandmarkCombo->getSimple()); // Name
		if (asset_id != sHomeID)
		{
			// Start the download process
			gLandmarkList.getAsset(asset_id);
		}
	}
	else
	{
		gTracker.stopTracking();
	}
	setDefaultBtn(mTeleportButton);
}

void LLFloaterWorldMap::trackEvent(const LLItemInfo& event_info)
{
	clearParcelInfoRequest();

	mTrackedStatus = LLTracker::TRACKING_LOCATION;
	gTracker.trackLocation(event_info.mPosGlobal, event_info.mName,
						   event_info.mToolTip, LLTracker::LOCATION_EVENT);
	setDefaultBtn(mTeleportButton);
}

void LLFloaterWorldMap::trackGenericItem(const LLItemInfo& item)
{
	clearParcelInfoRequest();

	mTrackedStatus = LLTracker::TRACKING_LOCATION;
	gTracker.trackLocation(item.mPosGlobal, item.mName, item.mToolTip,
						   LLTracker::LOCATION_ITEM);
	setDefaultBtn(mTeleportButton);
}

void LLFloaterWorldMap::trackLocation(const LLVector3d& pos_global,
									  const std::string& tooltip)
{
	LLPanelWorldMap::setDefaultZ(pos_global.mdV[VZ]);

	LLSimInfo* sim_info = gWorldMap.simInfoFromPosGlobal(pos_global);
	if (!sim_info)
	{
		gTracker.stopTracking();
		gWorldMap.mInvalidLocation = false;
		gWorldMap.mIsTrackingUnknownLocation = true;
		gWorldMap.mUnknownLocation = pos_global;
		S32 world_x = S32(pos_global.mdV[VX] / (F64)REGION_WIDTH_METERS);
		S32 world_y = S32(pos_global.mdV[VY] / (F64)REGION_WIDTH_METERS);
		gWorldMap.sendMapBlockRequest(world_x, world_y, world_x, world_y,
									  true);
		setDefaultBtn("");
		return;
	}
	if (sim_info->mAccess == SIM_ACCESS_DOWN)
	{
		// Down sim. Show the blue circle of death !
		gTracker.stopTracking();
		gWorldMap.mInvalidLocation = true;
		gWorldMap.mIsTrackingUnknownLocation = true;
		gWorldMap.mUnknownLocation = pos_global;
		setDefaultBtn("");
		return;
	}

	// Force an update of the number of agents in this sim
	sim_info->mAgentsUpdateTime = 0.0;

	std::string sim_name;
	gWorldMap.simNameFromPosGlobal(pos_global, sim_name);

	// Variable region size support
	U32 locX, locY;
	from_region_handle(sim_info->getHandle(), &locX, &locY);
	F32 region_x = pos_global.mdV[VX] - locX;
	F32 region_y = pos_global.mdV[VY] - locY;

	std::string full_name = llformat("%s (%d, %d, %d)", sim_name.c_str(),
									 ll_round(region_x), ll_round(region_y),
									 ll_round((F32)pos_global.mdV[VZ]));

	mTrackedStatus = LLTracker::TRACKING_LOCATION;
	gTracker.trackLocation(pos_global, full_name, tooltip);
	gWorldMap.mIsTrackingUnknownLocation = false;
	gWorldMap.mIsTrackingDoubleClick = false;
	gWorldMap.mIsTrackingCommit = false;

	requestParcelInfo(pos_global);

	setDefaultBtn(mTeleportButton);
}

void LLFloaterWorldMap::requestParcelInfo(const LLVector3d& pos_global)
{
	if (pos_global == mRequestedGlobalPos) return;

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;

	const std::string& url = regionp->getCapability("RemoteParcelRequest");
	if (url.empty()) return;

	mRequestedGlobalPos = pos_global;

	if (mParcelInfoObserver)
	{
		delete mParcelInfoObserver;
	}
	mParcelInfoObserver = new LLMapParcelInfoObserver(pos_global);

	LLVector3 pos_region((F32)fmod(pos_global.mdV[VX],
								   (F64)REGION_WIDTH_METERS),
						 (F32)fmod(pos_global.mdV[VY],
								   (F64)REGION_WIDTH_METERS),
						 (F32)pos_global.mdV[VZ]);
	gViewerParcelMgr.requestRegionParcelInfo(url, regionp->getRegionID(),
											 pos_region, pos_global,
											 mParcelInfoObserver->getObserverHandle());
}

void LLFloaterWorldMap::clearParcelInfoRequest()
{
	mRequestedGlobalPos.clear();
	if (mParcelInfoObserver)
	{
		delete mParcelInfoObserver;
		mParcelInfoObserver = NULL;
	}
}

void LLFloaterWorldMap::updateLocationSpinners(const LLVector3d& pos,
											   LLVector3* local_pos)
{
	// Convert global specified position to a local one
	F32 region_local_x = (F32)fmod(pos.mdV[VX], (F64)REGION_WIDTH_METERS);
	F32 region_local_y = (F32)fmod(pos.mdV[VY], (F64)REGION_WIDTH_METERS);
	F32 region_local_z = (F32)pos.mdV[VZ];

	// Support for variable size regions
	LLSimInfo* sim_info = gWorldMap.simInfoFromPosGlobal(pos);
	if (sim_info)
	{
		U32 loc_x, loc_y;
		from_region_handle(sim_info->getHandle(), &loc_x, &loc_y);
		region_local_x = pos.mdV[VX] - loc_x;
		region_local_y = pos.mdV[VY] - loc_y;
	}

	mSpinX->setValue(LLSD(region_local_x));
	mSpinY->setValue(LLSD(region_local_y));
	mSpinZ->setValue(LLSD(region_local_z));

	if (local_pos)
	{
		*local_pos = LLVector3(region_local_x, region_local_y, region_local_z);
	}
}

void LLFloaterWorldMap::updateLocation()
{
	bool got_sim_name;

	LLTracker::ETrackingStatus status = gTracker.getTrackingStatus();

	// These values may get updated by a message, so need to check them every
	// frame. The fields may be changed by the user, so only update them if the
	// data changes
	LLVector3d pos_global = gTracker.getTrackedPositionGlobal();
	if (pos_global.isExactlyZero())
	{
		LLVector3d agent_global_pos = gAgent.getPositionGlobal();

		// Set to avatar's current postion if nothing is selected
		if (status == LLTracker::TRACKING_NOTHING && mSetToUserPosition)
		{
			// Make sure we know where we are before setting the current user
			// position
			std::string agent_sim_name;
			got_sim_name = gWorldMap.simNameFromPosGlobal(agent_global_pos,
														  agent_sim_name);
			if (got_sim_name)
			{
				mSetToUserPosition = false;

				// Fill out the location field
				mLocationEditor->setValue(agent_sim_name);

				// Figure out where user is
				LLVector3 agent_pos;
				updateLocationSpinners(agent_global_pos, &agent_pos);
				LLPanelWorldMap::setDefaultZ(agent_pos.mV[VZ]);

				// Set the current SLURL
				mSLURL = LLSLURL(agent_sim_name, agent_pos).getSLURLString();
			}
		}

		return; // Invalid location
	}
	std::string sim_name;
	got_sim_name = gWorldMap.simNameFromPosGlobal(pos_global, sim_name);
	if (status != LLTracker::TRACKING_NOTHING &&
		(status != mTrackedStatus || pos_global != mTrackedLocation ||
		 sim_name != mTrackedSimName))
	{
		mTrackedStatus = status;
		mTrackedLocation = pos_global;
		mTrackedSimName = sim_name;

		if (status == LLTracker::TRACKING_AVATAR)
		{
			// *HACK: adjust Z values automatically for liaisons & gods so they
			// swoop down when they click on the map. Requested convenience.
			if (gAgent.isGodlike())
			{
				pos_global[2] = 200;
			}
		}

		mLocationEditor->setValue(sim_name);

		LLVector3 local_pos;
		updateLocationSpinners(pos_global, &local_pos);
		LLPanelWorldMap::setDefaultZ(local_pos.mV[VZ]);

		// simNameFromPosGlobal can fail, so do not give the user an invalid
		// SLURL
		if (got_sim_name)
		{
			mSLURL = LLSLURL(sim_name, local_pos).getSLURLString();
		}
		else
		{
			// Empty SLURL will disable the "Copy SLURL to clipboard" button
			mSLURL.clear();
		}
	}
}

void LLFloaterWorldMap::trackURL(const std::string& region_name,
								 S32 x_coord, S32 y_coord, S32 z_coord)
{
	if (!gFloaterWorldMapp) return;

	LLSimInfo* sim_info = gWorldMap.simInfoFromName(region_name);
	z_coord = llclamp(z_coord, 0, 4096);
	if (sim_info)
	{
		LLVector3 local_pos;
		local_pos.mV[VX] = (F32)x_coord;
		local_pos.mV[VY] = (F32)y_coord;
		local_pos.mV[VZ] = (F32)z_coord;
		LLVector3d global_pos = sim_info->getGlobalPos(local_pos);
		trackLocation(global_pos);
		setDefaultBtn(mTeleportButton);
		// Force an update of the number of agents in this sim
		sim_info->mAgentsUpdateTime = 0.0;
	}
	else
	{
		// Fill in UI based on URL
		gFloaterWorldMapp->mLocationEditor->setValue(region_name);
		mSpinX->setValue(LLSD((F32)x_coord));
		mSpinY->setValue(LLSD((F32)y_coord));
		mSpinZ->setValue(LLSD((F32)z_coord));
		LLPanelWorldMap::setDefaultZ((F32)z_coord);

		// pass sim name to combo box
		gFloaterWorldMapp->mCompletingRegionName = region_name;
		gWorldMap.sendNamedRegionRequest(region_name);
		LLStringUtil::toLower(gFloaterWorldMapp->mCompletingRegionName);
		gWorldMap.mIsTrackingCommit = true;
	}
}

void LLFloaterWorldMap::observeInventory(LLInventoryModel* model)
{
	if (mInventory)
	{
		mInventory->removeObserver(mInventoryObserver);
		delete mInventoryObserver;
		mInventory = NULL;
		mInventoryObserver = NULL;
	}
	if (model)
	{
		mInventory = model;
		mInventoryObserver = new LLMapInventoryObserver;
		// Inventory deletes all observers on shutdown
		mInventory->addObserver(mInventoryObserver);
		inventoryChanged();
	}
}

void LLFloaterWorldMap::inventoryChanged()
{
	if (gTracker.getTrackedLandmarkItemID().notNull())
	{
		LLUUID item_id = gTracker.getTrackedLandmarkItemID();
		buildLandmarkIDLists();
		trackLandmark(item_id);
	}
}

void LLFloaterWorldMap::observeFriends()
{
	if (!mFriendObserver)
	{
		mFriendObserver = new LLMapFriendObserver;
		gAvatarTracker.addObserver(mFriendObserver);
		friendsChanged();
	}
}

void LLFloaterWorldMap::friendsChanged()
{
	const LLUUID& avatar_id = gAvatarTracker.getAvatarID();
	buildAvatarIDList();
	if (avatar_id.notNull())
	{
		const LLRelationship* buddy = gAvatarTracker.getBuddyInfo(avatar_id);
		if (!buddy || !mFriendCombo || gAgent.isGodlike() ||
			!mFriendCombo->setCurrentByID(avatar_id) ||
			!buddy->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION))
		{
			gTracker.stopTracking();
		}
	}
}

// No longer really builds a list. Instead, just updates mAvatarCombo.
void LLFloaterWorldMap::buildAvatarIDList()
{
	if (!mFriendCombo) return;

    // Delete all but the "None" entry
	S32 list_size = mFriendCombo->getItemCount();
	while (list_size > 1)
	{
		mFriendCombo->selectNthItem(1);
		mFriendCombo->operateOnSelection(LLComboBox::OP_DELETE);
		--list_size;
	}

	LLSD default_column;
	default_column["name"] = "friend name";
	default_column["label"] = "Friend Name";
	default_column["width"] = 500;
	mFriendCombo->addColumn(default_column);

	// Get all of the calling cards for avatar that are currently online
	LLCollectMappableBuddies collector;
	gAvatarTracker.applyFunctor(collector);
	LLCollectMappableBuddies::buddy_map_t::iterator it;
	LLCollectMappableBuddies::buddy_map_t::iterator end;
	it = collector.mMappable.begin();
	end = collector.mMappable.end();
	for ( ; it != end; ++it)
	{
		mFriendCombo->addSimpleElement(it->first, ADD_BOTTOM, it->second);
	}

	mFriendCombo->setCurrentByID(gAvatarTracker.getAvatarID());
	mFriendCombo->selectFirstItem();
}

void LLFloaterWorldMap::buildLandmarkIDLists()
{
	if (!mLandmarkCombo)
	{
		return;
	}

    // Delete all but the "None" entry
	S32 list_size = mLandmarkCombo->getItemCount();
	if (list_size > 1)
	{
		mLandmarkCombo->selectItemRange(1, -1);
		mLandmarkCombo->operateOnSelection(LLComboBox::OP_DELETE);
	}

	mLandmarkItemIDList.clear();
	mLandmarkAssetIDList.clear();

	// Get all of the current landmarks
	mLandmarkAssetIDList.emplace_back(LLUUID::null);
	mLandmarkItemIDList.emplace_back(LLUUID::null);

	mLandmarkAssetIDList.emplace_back(sHomeID);
	mLandmarkItemIDList.emplace_back(sHomeID);

	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLIsType is_landmark(LLAssetType::AT_LANDMARK);
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(), cats, items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_landmark);

	std::sort(items.begin(), items.end(),
			  LLViewerInventoryItem::comparePointers());

	for (S32 i = 0, count = items.size(); i < count; ++i)
	{
		LLInventoryItem* item = items[i];
		if (!item) continue;	// Paranoia

		mLandmarkCombo->addSimpleElement(item->getName(), ADD_BOTTOM,
										 item->getUUID());

		mLandmarkAssetIDList.emplace_back(item->getAssetUUID());
		mLandmarkItemIDList.emplace_back(item->getUUID());
	}
	mLandmarkCombo->sortByColumn("landmark name", true);

	mLandmarkCombo->selectFirstItem();
}

F32 LLFloaterWorldMap::getDistanceToDestination(const LLVector3d& destination,
												F32 z_attenuation) const
{
	LLVector3d delta = destination - gAgent.getPositionGlobal();
	// by attenuating the z-component we effectively
	// give more weight to the x-y plane
	delta.mdV[VZ] *= z_attenuation;
	F32 distance = (F32)delta.length();
	return distance;
}

void LLFloaterWorldMap::clearLocationSelection(bool clear_ui)
{
	if (mSearchResultsList)
	{
		mSearchResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);
	}
	if (!gFocusMgr.childHasKeyboardFocus(mSpinX))
	{
		mSpinX->setValue(SIM_COORD_DEFAULT);
	}
	if (!gFocusMgr.childHasKeyboardFocus(mSpinY))
	{
		mSpinY->setValue(SIM_COORD_DEFAULT);
	}
	if (!gFocusMgr.childHasKeyboardFocus(mSpinZ))
	{
		mSpinZ->setValue(0);
		LLPanelWorldMap::setDefaultZ(-1.f);	// reset default Z
	}
	gWorldMap.mIsTrackingCommit = false;
	mCompletingRegionName.clear();
	mExactMatch = false;
}

void LLFloaterWorldMap::clearLandmarkSelection(bool clear_ui)
{
	if (clear_ui || !gFocusMgr.childHasKeyboardFocus(mLandmarkCombo))
	{
		if (mLandmarkCombo)
		{
			mLandmarkCombo->selectByValue("None");
		}
	}
}

void LLFloaterWorldMap::clearAvatarSelection(bool clear_ui)
{
	if (clear_ui || !gFocusMgr.childHasKeyboardFocus(mFriendCombo))
	{
		mTrackedStatus = LLTracker::TRACKING_NOTHING;
		if (mFriendCombo)
		{
			mFriendCombo->selectByValue("None");
		}
	}
}

// Adjust the maximally zoomed out limit of the zoom slider so you
// can see the whole world, plus a little.
void LLFloaterWorldMap::adjustZoomSliderBounds()
{
	// World size in regions
	S32 world_width_regions = gWorldMap.getWorldWidth() / REGION_WIDTH_UNITS;
	S32 world_height_regions = gWorldMap.getWorldHeight() / REGION_WIDTH_UNITS;

	// Pad the world size a little bit, so we have a nice border on the edge
	++world_width_regions;
	++world_height_regions;

	// Find how much space we have to display the world
	LLPanelWorldMap* panelp = (LLPanelWorldMap*)mTabs->getCurrentPanel();
	LLRect view_rect = panelp->getRect();

	// View size in pixels
	S32 view_width = view_rect.getWidth();
	S32 view_height = view_rect.getHeight();

	// Pixels per region to display entire width/height
	F32 width_pixels_per_region = (F32) view_width / (F32) world_width_regions;
	F32 height_pixels_per_region = (F32) view_height / (F32) world_height_regions;

	F32 pixels_per_region = llmin(width_pixels_per_region,
								  height_pixels_per_region);

	// Round pixels per region to an even number of slider increments
	S32 slider_units = llfloor(pixels_per_region / 0.2f);
	pixels_per_region = slider_units * 0.2f;

	// Make sure the zoom slider can be moved at least a little bit.
	// Likewise, less than the increment pixels per region is just silly.
	pixels_per_region = llclamp(pixels_per_region, 1.f,
								powf(2.f, ZOOM_MAX) * 128.f);

	F32 min_power = logf(pixels_per_region / 256.f) / F_LN2;
	mZoomSlider->setMinValue(min_power);
}

//static
void LLFloaterWorldMap::onGoHome(void*)
{
	gAgent.teleportHome();
}

//static
void LLFloaterWorldMap::onLandmarkComboPrearrange(LLUICtrl*, void* userdata)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)userdata;
	if (!self || self->mIsClosing)
	{
		return;
	}

	LLComboBox* combop = self->mLandmarkCombo;
	if (!combop) return;

	LLUUID current_choice;
	if (combop->getFirstSelectedIndex())	// If not "None" selected
	{
		current_choice = combop->getCurrentID();
	}

	self->buildLandmarkIDLists();

	if (current_choice.isNull() || !combop->setCurrentByID(current_choice))
	{
		gTracker.stopTracking();
	}
}

//static
void LLFloaterWorldMap::onLocationFocusChanged(LLFocusableElement* focus,
											   void* userdata)
{
	updateSearchEnabled();
}

//static
void LLFloaterWorldMap::onComboTextEntry(LLLineEditor*, void*)
{
	// Reset the tracking whenever we start typing into any of the search
	// fields, so that hitting <enter> does an auto-complete versus teleporting
	// us to the previously selected landmark/friend.
	gTracker.clearFocus();
}

//static
void LLFloaterWorldMap::onSearchTextEntry(const std::string&, void*)
{
	// Reset the tracking whenever we start typing into any of the search
	// fields, so that hitting <enter> does an auto-complete versus teleporting
	// us to the previously selected landmark/friend.
	gTracker.clearFocus();
	updateSearchEnabled();
}

//static
void LLFloaterWorldMap::onLandmarkComboCommit(LLUICtrl*, void* userdata)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)userdata;
	if (!self || self->mIsClosing)
	{
		return;
	}

	LLComboBox* combop = self->mLandmarkCombo;
	if (!combop) return;

	// If "None" is selected, we are done.
	if (!combop->getFirstSelectedIndex())
	{
		return;
	}

	LLUUID asset_id;
	LLUUID item_id = combop->getCurrentID();

	gTracker.stopTracking();

	// RN: stopTracking() clears current combobox selection, need to reassert
	// it here
	combop->setCurrentByID(item_id);

	if (item_id.isNull())
	{
	}
	else if (item_id == sHomeID)
	{
		asset_id = sHomeID;
	}
	else
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if (item)
		{
			asset_id = item->getAssetUUID();
		}
		else
		{
			// Something went wrong, so revert to a safe value.
			item_id.setNull();
		}
	}

	self->trackLandmark(item_id);
	onShowTargetBtn(self);

	// Reset to user postion if nothing is tracked
	self->mSetToUserPosition =
		(gTracker.getTrackingStatus() == LLTracker::TRACKING_NOTHING);
}

//static
void LLFloaterWorldMap::onAvatarComboPrearrange(LLUICtrl*, void* userdata)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)userdata;
	if (!self || self->mIsClosing)
	{
		return;
	}

	LLComboBox* combop = self->mFriendCombo;
	if (!combop) return;

	LLUUID current_choice;

	if (gAvatarTracker.haveTrackingInfo())
	{
		current_choice = gAvatarTracker.getAvatarID();
	}

	self->buildAvatarIDList();

	if (!combop->setCurrentByID(current_choice) || current_choice.isNull())
	{
		gTracker.stopTracking();
	}
}

//static
void LLFloaterWorldMap::onAvatarComboCommit(LLUICtrl*, void* userdata)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)userdata;
	if (!self || self->mIsClosing)
	{
		return;
	}

	LLComboBox* combop = self->mFriendCombo;
	if (!combop) return;

	const LLUUID& new_avatar_id = combop->getCurrentID();
	if (new_avatar_id.notNull())
	{
		std::string name = self->mFriendCombo->getSimple();
		self->trackAvatar(new_avatar_id, name);
		onShowTargetBtn(self);
	}
	else
	{
		// Reset to user postion if nothing is tracked
		self->mSetToUserPosition =
			gTracker.getTrackingStatus() == LLTracker::TRACKING_NOTHING;
	}
}

//static
void LLFloaterWorldMap::updateSearchEnabled()
{
	if (!gFloaterWorldMapp || gFloaterWorldMapp->mIsClosing)
	{
		return;
	}

	if (gFocusMgr.childHasKeyboardFocus(gFloaterWorldMapp->mLocationEditor) &&
		gFloaterWorldMapp->mLocationEditor->getValue().asString().length() > 0)
	{
		gFloaterWorldMapp->setDefaultBtn("DoSearch");
	}
	else
	{
		gFloaterWorldMapp->setDefaultBtn((LLButton*)NULL);
	}
}

//static
void LLFloaterWorldMap::onLocationCommit(void* userdata)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)userdata;
	if (!self || self->mIsClosing)
	{
		return;
	}

	self->clearLocationSelection(false);
	self->mCompletingRegionName.clear();
	self->mLastRegionName.clear();

	std::string str = self->mLocationEditor->getValue().asString();

	// Trim any leading and trailing spaces in the search target
	std::string saved_str = str;
	LLStringUtil::trim(str);
	if (str != saved_str)
	{
		// Set the value in the UI if any spaces were removed
		self->mLocationEditor->setValue(str);
	}

	LLStringUtil::toLower(str);
	self->mCompletingRegionName = str;
	gWorldMap.mIsTrackingCommit = true;
	self->mExactMatch = false;
	if (str.length() >= 3)
	{
		gWorldMap.sendNamedRegionRequest(str);
	}
	else
	{
		str += "#";
		gWorldMap.sendNamedRegionRequest(str);
	}
}

//static
void LLFloaterWorldMap::onClearBtn(void* data)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)data;
	if (self)
	{
		LLPanelWorldMap::setDefaultZ(-1.f);	// reset default Z
		self->mTrackedStatus = LLTracker::TRACKING_NOTHING;
		gTracker.stopTracking(true);
		gWorldMap.mIsTrackingUnknownLocation = false;
		// Clear the SLURL since it's invalid
		self->mSLURL.clear();
		// Revert back to the current user position
		self->mSetToUserPosition = true;
	}
}

//static
void LLFloaterWorldMap::onShowTargetBtn(void* data)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)data;
	if (self)
	{
		self->centerOnTarget(true);
	}
}

//static
void LLFloaterWorldMap::onShowAgentBtn(void* data)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)data;
	if (self)
	{
		LLPanelWorldMap::setPan(0, 0, false);	// false == animate

		// Set flag so user's location will be displayed if not tracking
		// anything else
		self->mSetToUserPosition = true;
	}
}

//static
void LLFloaterWorldMap::onClickTeleportBtn(void* data)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)data;
	if (self)
	{
		self->teleport();
	}
}

//static
void LLFloaterWorldMap::onCopySLURL(void* data)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)data;
	if (self)
	{
		gWindowp->copyTextToClipboard(utf8str_to_wstring(self->mSLURL));

		LLSD args;
		args["SLURL"] = self->mSLURL;
		gNotifications.add("CopySLURL", args);
	}
}

void LLFloaterWorldMap::centerOnTarget(bool animate)
{
	LLVector3d pos_global;
	if (gTracker.getTrackingStatus() != LLTracker::TRACKING_NOTHING)
	{
		LLVector3d tracked_position = gTracker.getTrackedPositionGlobal();
		// RN: tracker does not allow us to query completion, so we check for a
		// tracking position of absolute zero, and keep trying in the draw loop
		if (tracked_position.isExactlyZero())
		{
			mWaitingForTracker = true;
			return;
		}
		pos_global = gTracker.getTrackedPositionGlobal() -
					 gAgent.getCameraPositionGlobal();
	}
	else if (gWorldMap.mIsTrackingUnknownLocation)
	{
		pos_global = gWorldMap.mUnknownLocation -
					 gAgent.getCameraPositionGlobal();;
	}
	else
	{
		// Default behavior = center on agent
		pos_global.clear();
	}

	LLPanelWorldMap::setPan(-llfloor((F32)(pos_global.mdV[VX] *
								    (F64)LLPanelWorldMap::sPixelsPerMeter)),
						   -llfloor((F32)(pos_global.mdV[VY] *
									(F64)LLPanelWorldMap::sPixelsPerMeter)),
						   !animate);
	mWaitingForTracker = false;
}

void LLFloaterWorldMap::teleport()
{
	bool teleport_home = false;
	LLUUID lm_asset_id;
	LLVector3d pos_global;
	LLAvatarTracker& av_tracker = gAvatarTracker;

	LLTracker::ETrackingStatus tracking_status = gTracker.getTrackingStatus();
	if (tracking_status == LLTracker::TRACKING_AVATAR &&
		av_tracker.haveTrackingInfo())
	{
		pos_global = av_tracker.getGlobalPos();
		pos_global.mdV[VZ] = mSpinZ->getValue();
	}
	else if (tracking_status == LLTracker::TRACKING_LANDMARK)
	{
		lm_asset_id = gTracker.getTrackedLandmarkAssetID();
		if (lm_asset_id == sHomeID)
		{
			teleport_home = true;
		}
		else
		{
			LLLandmark* landmark = gLandmarkList.getAsset(lm_asset_id);
			LLUUID region_id;
			if (landmark && !landmark->getGlobalPos(pos_global) &&
				landmark->getRegionID(region_id))
			{
				LLLandmark::requestRegionHandle(gMessageSystemp,
												gAgent.getRegionHost(),
												region_id, NULL);
			}
		}
	}
	else if (tracking_status == LLTracker::TRACKING_LOCATION)
	{
		// Make sure any change to spinners is committed:
		onCommitLocation(NULL, this);

		pos_global = gTracker.getTrackedPositionGlobal();
	}
	else
	{
		make_ui_sound("UISndInvalidOp");
	}

	// Do the teleport, which will also close the floater
	if (teleport_home)
	{
		gAgent.teleportHome();
	}
	else if (!pos_global.isExactlyZero())
	{
		if (lm_asset_id.notNull())
		{
			gAgent.teleportViaLandmark(lm_asset_id);
		}
		else
		{
			gAgent.teleportViaLocation(pos_global);
		}
	}
}

// *HACK: to work around non loading tiles on first tab change. HB
static void force_reload_tiles(U32 layer)
{
	gWorldMap.clearImageRefs(layer);
	gWorldMap.clearSimFlags();
}

//static
void LLFloaterWorldMap::onTabChanged(void* userdata, bool from_click)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)userdata;
	if (self)
	{
		// Find my index
		U32 index = self->mTabs->getCurrentPanelIndex();
		gWorldMap.setCurrentLayer(index);

		// *HACK: to work around non loading tiles on first tab change. HB
		static bool first_tab_change = true;
		if (first_tab_change)
		{
			first_tab_change = false;
			constexpr F32 delay = 2.f;	// In seconds
			doAfterInterval(boost::bind(&force_reload_tiles, index), delay);
		}
	}
}

void LLFloaterWorldMap::updateSims(bool found_null_sim)
{
	if (mCompletingRegionName.empty())
	{
		return;
	}

	mSearchResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);

	LLSD selected_value = mSearchResultsList->getSelectedValue();

	S32 name_length = mCompletingRegionName.length();

	bool match_found = false;
	S32 num_results = 0;
	for (LLWorldMap::sim_info_map_t::const_iterator
			it = gWorldMap.mSimInfoMap.begin(),
			end = gWorldMap.mSimInfoMap.end();
		 it != end; ++it)
	{
		LLSimInfo* info = it->second;
		std::string sim_name = info->mName;
		std::string sim_name_lower = sim_name;
		LLStringUtil::toLower(sim_name_lower);

		if (sim_name_lower.substr(0, name_length) == mCompletingRegionName)
		{
			if (gWorldMap.mIsTrackingCommit)
			{
				if (sim_name_lower == mCompletingRegionName)
				{
					selected_value = sim_name;
					match_found = true;
					// Force an update of the number of agents in this sim
					info->mAgentsUpdateTime = 0.0;
				}
			}

			LLSD value;
			value["id"] = sim_name;
			value["columns"][0]["column"] = "sim_name";
			value["columns"][0]["value"] = sim_name;
			mSearchResultsList->addElement(value);
			++num_results;
		}
	}

	mSearchResultsList->selectByValue(selected_value);

	if (found_null_sim)
	{
		mCompletingRegionName.clear();
	}

	if (match_found)
	{
		mExactMatch = true;
		mSearchResultsList->setFocus(true);
		onCommitSearchResult(mSearchResultsList, this);
	}
	else if (!mExactMatch && num_results > 0)
	{
		mSearchResultsList->selectFirstItem(); // select first item by default
		mSearchResultsList->setFocus(true);
		onCommitSearchResult(mSearchResultsList, this);
	}
	else
	{
		mSearchResultsList->addCommentText("None found.");
		mSearchResultsList->operateOnAll(LLScrollListCtrl::OP_DESELECT);
	}
}

//static
void LLFloaterWorldMap::onCommitLocation(LLUICtrl*, void* userdata)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)userdata;
	if (self)
	{
		S32 local_x = self->mSpinX->getValue();
		S32 local_y = self->mSpinY->getValue();
		S32 local_z = self->mSpinZ->getValue();
		std::string region_name = self->mLocationEditor->getValue().asString();
		self->trackURL(region_name, local_x, local_y, local_z);
	}
}

//static
void LLFloaterWorldMap::onCommitSearchResult(LLUICtrl*, void* userdata)
{
	LLFloaterWorldMap* self = (LLFloaterWorldMap*)userdata;
	if (!self) return;

	LLScrollListCtrl* listp = self->mSearchResultsList;
	if (!listp) return;

	LLSD selected_value = listp->getSelectedValue();
	std::string sim_name = selected_value.asString();
	if (sim_name.empty())
	{
		return;
	}
	LLStringUtil::toLower(sim_name);

	for (LLWorldMap::sim_info_map_t::const_iterator
			it = gWorldMap.mSimInfoMap.begin(),
			end = gWorldMap.mSimInfoMap.end();
		 it != end; ++it)
	{
		LLSimInfo* info = it->second;
		std::string info_sim_name = info->mName;
		LLStringUtil::toLower(info_sim_name);

		if (sim_name == info_sim_name)
		{
			LLVector3d pos_global = from_region_handle(info->mHandle);
			F64 local_x = self->mSpinX->getValue();
			F64 local_y = self->mSpinY->getValue();
			F64 local_z = self->mSpinZ->getValue();
			pos_global.mdV[VX] += local_x;
			pos_global.mdV[VY] += local_y;
			pos_global.mdV[VZ] = local_z;

			self->mLocationEditor->setValue(sim_name);
			self->trackLocation(pos_global);
			self->setDefaultBtn(self->mTeleportButton);

			// Force an update of the number of agents in this sim
			info->mAgentsUpdateTime = 0.0;
			break;
		}
	}

	onShowTargetBtn(self);
}
