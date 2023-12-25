/** 
 * @file hbfloaterteleporthistory.cpp
 * @author Henri Beauchamp
 * @brief HBFloaterTeleportHistory class implementation
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 * 
 * Copyright (c) 2018, Henri Beauchamp
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#include "hbfloaterteleporthistory.h"

#include "llapp.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterworldmap.h"
#include "llgridmanager.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llslurl.h"
#include "llurldispatcher.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llweb.h"					// For escapeURL()

#define COMMENT_PREFIX "\342\200\243 "

// Globals

// Instance created in LLViewerWindow::initWorldUI()
HBFloaterTeleportHistory* gFloaterTeleportHistoryp = NULL;

// Helper function

static std::string get_timestamp()
{
	// Make it easy to sort: use the Year-Month-Day ISO convention
	std::string time_format = "%Y-%m-%d  " +
							  gSavedSettings.getString("ShortTimeFormat");
	return LLGridManager::getTimeStamp(time_corrected(), time_format);
}

// Local class used to populate the favorite places list

class HBTeleportLocation
{
public:
	HBTeleportLocation()
	:	mVisits(1)
	{
	}

public:
	S32			mVisits;
	std::string	mParcel;
	std::string	mRegion;
	std::string	mPosition;
};

// HBFloaterTeleportHistory class

HBFloaterTeleportHistory::HBFloaterTeleportHistory()
:	LLFloater("teleport history"),
	mPlacesListComment(NULL),
	mCount(0),
	mFirstOpen(true),
	mCanTeleport(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_teleport_history.xml",
												 NULL, false);
}

//virtual
HBFloaterTeleportHistory::~HBFloaterTeleportHistory()
{
	mTeleportArrivingConnection.disconnect();
	mTeleportFinishConnection.disconnect();
	mTeleportFailedConnection.disconnect();
	gFloaterTeleportHistoryp = NULL;
	llinfos << "Teleport history instance destroyed." << llendl;
}

//virtual
void HBFloaterTeleportHistory::onFocusReceived()
{
	// Take care to enable or disable buttons depending on the selection in the
	// places list
	setButtonsStatus();
	LLFloater::onFocusReceived();
}

bool HBFloaterTeleportHistory::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("lists");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("tp_history");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("favorite_places");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("search_places");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	mPlacesList = getChild<LLScrollListCtrl>("places_list");
	mPlacesList->setDoubleClickCallback(onTeleport);
	mPlacesList->setCommitCallback(onPlacesSelected);
	mPlacesList->setCallbackUserData(this);

	mFavoritesList = getChild<LLScrollListCtrl>("favorites_list");
	mFavoritesList->setDoubleClickCallback(onTeleport);
	mFavoritesList->setCommitCallback(onPlacesSelected);
	mFavoritesList->setCallbackUserData(this);

	mResultsList = getChild<LLScrollListCtrl>("results_list");
	mResultsList->setDoubleClickCallback(onTeleport);
	mResultsList->setCommitCallback(onPlacesSelected);
	mResultsList->setCallbackUserData(this);

	mSearchEditor = getChild<LLSearchEditor>("search");
	mSearchEditor->setSearchCallback(onSearchEdit, this);

	mTeleportBtn = getChild<LLButton>("teleport");
	mTeleportBtn->setClickedCallback(onTeleport, this);

	mShowOnMapBtn = getChild<LLButton>("show_on_map");
	mShowOnMapBtn->setClickedCallback(onShowOnMap, this);

	mCopySLURLBtn = getChild<LLButton>("copy_slurl");
	mCopySLURLBtn->setClickedCallback(onCopySLURL, this);

	mRefreshBtn = getChild<LLButton>("refresh");
	mRefreshBtn->setClickedCallback(onRefresh, this);

	mRemoveFlyoutBtn = getChild<LLFlyoutButton>("remove");
	mRemoveFlyoutBtn->setCommitCallback(onRemove);
	mRemoveFlyoutBtn->setCallbackUserData(this);

	childSetAction("close", onButtonClose, this);

	mNumEntriesStr = COMMENT_PREFIX + getString("number_of_entries");
	mNoEntryStr = COMMENT_PREFIX + getString("no_entry");

	mTeleportArrivingConnection =
		gViewerParcelMgr.setTPArrivingCallback(boost::bind(&HBFloaterTeleportHistory::onTeleportArriving));
	mTeleportFinishConnection =
		gViewerParcelMgr.setTPFinishedCallback(boost::bind(&HBFloaterTeleportHistory::onTeleportFinished,
														   _1, _2));
	mTeleportFailedConnection =
		gViewerParcelMgr.setTPFailedCallback(boost::bind(&HBFloaterTeleportHistory::onTeleportFailed));

	return true;
}

//virtual
void HBFloaterTeleportHistory::draw()
{
	mTeleportBtn->setEnabled(mCanTeleport && !gAgent.teleportInProgress());
	LLFloater::draw();
}

void HBFloaterTeleportHistory::setButtonsStatus()
{
	LLScrollListCtrl* list = NULL;
	S32 active_tab = mTabContainer->getCurrentPanelIndex();
	switch (active_tab)
	{
		case 0:
			mRefreshBtn->setVisible(false);
			mRemoveFlyoutBtn->setVisible(true);
			mSearchEditor->setVisible(false);
			list = mPlacesList;
			break;

		case 1:
			mRefreshBtn->setVisible(true);
			mRemoveFlyoutBtn->setVisible(false);
			mSearchEditor->setVisible(false);
			list = mFavoritesList;
			break;

		case 2:
			mRefreshBtn->setVisible(false);
			mRemoveFlyoutBtn->setVisible(false);
			mSearchEditor->setVisible(true);
			list = mResultsList;
			break;

		default:
			llwarns << "Unknown tab !" << llendl;
			llassert(false);
			return;
	}

	mCanTeleport = list && list->getFirstSelected();
	mShowOnMapBtn->setEnabled(mCanTeleport);
	mCopySLURLBtn->setEnabled(mCanTeleport);
	mRemoveFlyoutBtn->setEnabled(mCanTeleport);
}

std::string HBFloaterTeleportHistory::getHistoryFileName(bool fallback) const
{
	std::string name = gIsInSecondLifeBetaGrid ? "beta_tp_history.xml"
											   : "tp_history.xml";
	name = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT, name);
	if (fallback && !LLFile::isfile(name))
	{
		name = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
											  "teleport_history.xml");
	}
	return name;
}

void HBFloaterTeleportHistory::addPlacesListComment()
{
	removePlacesListComment();

	std::string comment;
	if (mCount > 0)
	{
		comment = mNumEntriesStr + llformat(" %d", mCount);
	}
	else
	{
		comment = mNoEntryStr;
	}
	mPlacesListComment = mPlacesList->addCommentText(comment);
}

void HBFloaterTeleportHistory::removePlacesListComment()
{
	if (mPlacesListComment)
	{
		mPlacesList->deleteItem(mPlacesListComment);
		mPlacesListComment = NULL;
	}
}

void HBFloaterTeleportHistory::populateLists(const LLSD& file_data)
{
	// Clear all the data
	mPlacesList->clearRows();
	mFavoritesList->clearRows();
	mTPlist.clear();
	mCount = 0;

	std::map<std::string, HBTeleportLocation*> favorites;
	std::string column, value;
	std::string agent_home_parcel =
		gSavedPerAccountSettings.getString("AgentHomeParcel");
	bool has_type, has_parcel, has_region, has_position, has_timestamp;
	bool is_arrival;
	for (S32 idx = 0, entries = file_data.size(); idx < entries; ++idx)
	{
		const LLSD& data = file_data[idx];
		if (!data.has("id") || !data.has("columns"))
		{
			// Silently skip empty maps
			continue;
		}
		LLSD element;
		LLSD& columns = element["columns"];
		HBTeleportLocation* location = new HBTeleportLocation();
		// Let's validate the data and reject badly formatted entries.
		has_type = has_parcel = has_region = has_position = has_timestamp =
				   is_arrival = false;
		for (S32 i = 0, count = data["columns"].size(); i < count; ++i)
		{
			const LLSD& map = data["columns"][i];
			if (!map.has("column") || !map.has("value"))
			{
				// Silently skip empty maps
				continue;
			}
					
			column = map["column"].asString();
			if (column == "type")
			{
				has_type = true;
				value = map["value"].asString();
				is_arrival = value == "A";
				columns[LIST_TYPE]["column"] = "type";
				columns[LIST_TYPE]["value"] = value;
			}
			else if (column == "parcel")
			{
				has_parcel = true;
				value = map["value"].asString();
				LLStringUtil::trim(value);
				location->mParcel = value;
				columns[LIST_PARCEL]["column"] = "parcel";
				columns[LIST_PARCEL]["value"] = value;
			}
			else if (column == "region")
			{
				has_region = true;
				value = map["value"].asString();
				location->mRegion = value;
				columns[LIST_REGION]["column"] = "region";
				columns[LIST_REGION]["value"] = value;
			}
			else if (column == "position")
			{
				has_position = true;
				value = map["value"].asString();
				location->mPosition = value;
				columns[LIST_POSITION]["column"] = "position";
				columns[LIST_POSITION]["value"] = value;
			}
			else if (column == "timestamp")
			{
				has_timestamp = true;
				value = map["value"].asString();
				columns[LIST_TIMESTAMP]["column"] = "timestamp";
				columns[LIST_TIMESTAMP]["value"] = value;
			}
			else
			{
				break;
			}
		}
		if (has_type && has_parcel && has_region && has_position &&
			has_timestamp)
		{
			// We have a valid element, add it to the list
			element["id"] = mCount++;
			mPlacesList->addElement(element, ADD_TOP);
			mTPlist.append(element);

			if (is_arrival)
			{
				value = location->mParcel + "|" + location->mRegion;
				if (value != agent_home_parcel)
				{
					std::map<std::string, HBTeleportLocation*>::iterator it =
						favorites.find(value);
					if (it != favorites.end())
					{
						++it->second->mVisits;
						// Update the position to the one of the last visit
						it->second->mPosition = location->mPosition;
					}
					else
					{
						// Store the new favorite data
						favorites[value] = location;
						location = NULL;	// Prevents data deletion
					}
				}
			}
		}
		if (location)
		{
			delete location;
		}
	}
	addPlacesListComment();

	std::string comment;
	if (!favorites.empty())
	{
		S32 min_visits = gSavedSettings.getU32("MinVisitsForFavorites");
		S32 count = 0;
		for (std::map<std::string, HBTeleportLocation*>::iterator
				it = favorites.begin(), end = favorites.end();
			 it != end; ++it)
		{
			HBTeleportLocation* location = it->second;

			S32 visits = location->mVisits;
			if (visits >= min_visits)
			{
				LLSD element;
				element["id"] = count++;
				LLSD& columns = element["columns"];
				columns[FAV_PARCEL]["column"] = "parcel";
				columns[FAV_PARCEL]["value"] = location->mParcel;
				columns[FAV_REGION]["column"] = "region";
				columns[FAV_REGION]["value"] = location->mRegion;
				columns[FAV_POSITION]["column"] = "position";
				columns[FAV_POSITION]["value"] = location->mPosition;
				columns[FAV_VISITS]["column"] = "visits";
				columns[FAV_VISITS]["value"] = visits;

				mFavoritesList->addElement(element);
			}

			delete location;
		}
		comment = mNumEntriesStr + llformat(" %d", count);
	}
	else
	{
		comment = mNoEntryStr;
	}
	// Sort favorites by visits in decreasing order
	mFavoritesList->sortByColumnIndex(FAV_VISITS, false);
	mFavoritesList->addCommentText(comment);

	updateSearchResults();

	setButtonsStatus();
}

void HBFloaterTeleportHistory::updateSearchResults()
{
	mResultsList->clearRows();
	if (mSearchString.length() < 3)
	{
		return;
	}

	std::set<std::string> places;
	std::string name;
	std::vector<LLScrollListItem*> data = mPlacesList->getAllData();
	S32 results = 0;
	for (S32 i = 0, count = data.size(); i < count; ++i)
	{
		LLScrollListItem* item = data[i];

		// Only take arrival places into account, and illiminate the comment
		// line too...
		if (item->getColumn(LIST_TYPE)->getValue().asString() != "A")
		{
			continue;
		}

		// Concatenate the parcel and region names, lower-cased.
		name = item->getColumn(LIST_PARCEL)->getValue().asString() + "|";
		name += item->getColumn(LIST_REGION)->getValue().asString();
		LLStringUtil::toLower(name);
		// If not matching the search pattern or already listed once, skip it
		if (name.find(mSearchString) == std::string::npos ||
			places.count(name))
		{
			continue;
		}
		// Remember we listed this location already.
		places.emplace(name);

		LLSD element;
		// Same Id as in the places list, for easy selection (see
		// onPlacesSelected())
		element["id"] = item->getValue();

		// Copy the data we need
		LLSD& columns = element["columns"];
		columns[RES_PARCEL]["column"] = "parcel";
		columns[RES_PARCEL]["value"] =
			item->getColumn(LIST_PARCEL)->getValue();
		columns[RES_REGION]["column"] = "region";
		columns[RES_REGION]["value"] =
			item->getColumn(LIST_REGION)->getValue();
		columns[RES_POSITION]["column"] = "position";
		columns[RES_POSITION]["value"] =
			item->getColumn(LIST_POSITION)->getValue();

		mResultsList->addElement(element);

		++results;
	}	
	std::string comment;
	if (results > 0)
	{
		comment = mNumEntriesStr + llformat(" %d", results);
	}
	else
	{
		comment = mNoEntryStr;
	}
	mResultsList->addCommentText(comment);
}

void HBFloaterTeleportHistory::loadEntries()
{
	std::string filename = getHistoryFileName(true);
	if (filename.empty())
	{
		llwarns << "Could not access the teleport history file. History not loaded."
				<< llendl;
		return;
	}

	llifstream file(filename.c_str());
	if (file.is_open())
	{
		LLSD data;

		llinfos << "Loading the teleport history from: " << filename << llendl;
		LLSDSerialize::fromXML(data, file);
		file.close();

		populateLists(data);
		// Save our validated data
		saveList();
	}
	else
	{
		llwarns << "Could not open the teleport history file. History not loaded."
				<< llendl;
	}
}

void HBFloaterTeleportHistory::saveList()
{
	std::string filename = getHistoryFileName();
	if (filename.empty())
	{
		llwarns_sparse << "Could not access the teleport history file. History not saved."
					   << llendl;
		return;
	}
	llofstream file(filename.c_str());
	if (file.is_open())
	{
		llinfos << "Saving the teleport history to: " << filename << llendl;
		LLSDSerialize::toPrettyXML(mTPlist, file);
		file.close();
	}
	else
	{
		llwarns << "Could not open file '" << filename << "' for writing."
				<< llendl;
	}
}

void HBFloaterTeleportHistory::addPendingEntry(const std::string& region_name,
											   LLVector3 pos)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		mPendingRegionName.clear();
		return;
	}
//mk

	// Set pending entry timestamp
	mPendingTimeString = get_timestamp();

	// Set pending region name
	mPendingRegionName = region_name;

	// Set pending position
	if (isAgentAvatarValid())
	{
		// The actual Z coordinate of the TP is at the agent's feet
		pos.mV[VZ] -= 0.5f * (gAgentAvatarp->mBodySize.mV[VZ] +
							  gAgentAvatarp->mAvatarOffset.mV[VZ]);
	}
	mPendingPosition = llformat("%d, %d, %d", (S32)pos.mV[VX], (S32)pos.mV[VY],
								(S32)pos.mV[VZ]);
}

void HBFloaterTeleportHistory::addSourceEntry(const std::string& source_slurl,
											  const std::string& parcel_name)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		mPendingRegionName.clear();
		return;
	}
//mk

	LLSLURL slurl(source_slurl);
	if (slurl.getType() != LLSLURL::LOCATION)
	{
		llwarns << "Could not parse the source SLURL (" << source_slurl
				<< "): TP history entry not added" << llendl;
		return;
	}

	// Extract the region name
	mPendingRegionName = slurl.getRegion();

	// Set pending position
	LLVector3 pos = slurl.getPosition();
	mPendingPosition = llformat("%d, %d, %d", (S32)pos.mV[VX], (S32)pos.mV[VY],
								(S32)pos.mV[VZ]);

	// Set pending entry timestamp
	mPendingTimeString = get_timestamp();

	// Add this pending entry immediately, using the passed (departure) parcel
	// name.
	addEntry(parcel_name, true);
}

void HBFloaterTeleportHistory::addEntry(std::string parcel_name,
										bool departure)
{
	if (mPendingRegionName.empty())
	{
		return;
	}

	// Build the list entry
	LLSD element;
	element["id"] = mCount++;
	LLSD& columns = element["columns"];
	columns[LIST_TYPE]["column"] = "type";
	columns[LIST_TYPE]["value"] = departure ? "D" : "A";
	columns[LIST_PARCEL]["column"] = "parcel";
	LLStringUtil::trim(parcel_name);
	columns[LIST_PARCEL]["value"] = parcel_name;
	columns[LIST_REGION]["column"] = "region";
	columns[LIST_REGION]["value"] = mPendingRegionName;
	columns[LIST_POSITION]["column"] = "position";
	columns[LIST_POSITION]["value"] = mPendingPosition;
	columns[LIST_TIMESTAMP]["column"] = "timestamp";
	columns[LIST_TIMESTAMP]["value"] = mPendingTimeString;

	// Add the new list entry on top of the list, deselect all and disable the
	// buttons
	mPlacesList->addElement(element, ADD_TOP);
	mPlacesList->deselectAllItems(true);
	setButtonsStatus();

	// Update the number of entries line
	addPlacesListComment();

	// Save the entry in the history file
	mTPlist.append(element);
	saveList();

	mPendingRegionName.clear();
}

//static
void HBFloaterTeleportHistory::onTeleportArriving()
{
	if (gFloaterTeleportHistoryp && !gFloaterTeleportHistoryp->isMinimized() &&
		gSavedSettings.getBool("HideFloatersOnTPSuccess"))
	{
		gFloaterTeleportHistoryp->setVisible(false);
	}
}

//static
void HBFloaterTeleportHistory::onTeleportFinished(const LLVector3d& pos,
												  bool local)
{
	if (!gFloaterTeleportHistoryp) return;

	if (local)
	{
		// Do not register local teleports
		gFloaterTeleportHistoryp->mPendingRegionName.clear();
		return;
	}

	gFloaterTeleportHistoryp->addEntry(gViewerParcelMgr.getAgentParcelName());
}

//static
void HBFloaterTeleportHistory::onTeleportFailed()
{
	if (gFloaterTeleportHistoryp)
	{
		gFloaterTeleportHistoryp->mPendingRegionName.clear();
	}
}

//virtual
void HBFloaterTeleportHistory::onOpen()
{
	if (mFirstOpen)
	{
		mFirstOpen = false;
		// Reposition floater from saved settings
		LLRect rect = gSavedSettings.getRect("FloaterTeleportHistoryRect");
		reshape(rect.getWidth(), rect.getHeight(), false);
		setRect(rect);
		mTabContainer->selectTab(gSavedSettings.getS32("LastTPHistoryTab"));
	}
}

//virtual
void HBFloaterTeleportHistory::onClose(bool app_quitting)
{
	LLFloater::setVisible(false);
}

//virtual
bool HBFloaterTeleportHistory::canClose()
{
	return !LLApp::isExiting();
}

void HBFloaterTeleportHistory::toggle()
{
	if (getVisible())
	{
		setVisible(false);
	}
	else
	{
		open();
	}
}

bool HBFloaterTeleportHistory::getSelectedLocation(std::string& region,
												   LLVector3& pos)
{
	LLScrollListCtrl* list = NULL;
	S32 col_region = 0;
	S32 col_pos = 0;
	S32 active_tab = mTabContainer->getCurrentPanelIndex();
	switch (active_tab)
	{
		case 0:
			list = mPlacesList;
			col_region = LIST_REGION;
			col_pos = LIST_POSITION;
			break;

		case 1:
			list = mFavoritesList;
			col_region = FAV_REGION;
			col_pos = FAV_POSITION;
			break;

		case 2:
			list = mResultsList;
			col_region = RES_REGION;
			col_pos = RES_POSITION;
			break;

		default:
			llwarns << "Unknown tab !" << llendl;
			llassert(false);
			return false;
	}

	LLScrollListItem* itemp = list->getFirstSelected();
	if (!itemp)
	{
		return false;
	}

	region = itemp->getColumn(col_region)->getValue().asString();

	std::string pos_str = itemp->getColumn(col_pos)->getValue().asString();
	size_t i = pos_str.find(',');
	if (i != std::string::npos)
	{
		S32 x = atoi(pos_str.substr(0, i++).c_str());
		S32 y = atoi(pos_str.substr(i).c_str());
		S32 z = 0;
		i = pos_str.find(',', i);
		if (i != std::string::npos)
		{
			z = atoi(pos_str.substr(i + 1).c_str());
		}
		pos = LLVector3(x, y, z);
	}

	return true;
}

// Callbacks

//static
void HBFloaterTeleportHistory::onTabChanged(void* data, bool)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (self && self->mTabContainer)	// Paranoia
	{
		gSavedSettings.setS32("LastTPHistoryTab",
							  self->mTabContainer->getCurrentPanelIndex());
		self->setButtonsStatus();
	}
}

//static
void HBFloaterTeleportHistory::onPlacesSelected(LLUICtrl* ctrl, void* data)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (self && ctrl)
	{
		// On selection change check if we need to enable or disable buttons
		self->setButtonsStatus();

		// When selecting an item in the search results, select the
		// corresponding item in the history list.
		LLScrollListCtrl* list = (LLScrollListCtrl*)ctrl;
		if (list == self->mResultsList)
		{
			LLScrollListItem* item = list->getFirstSelected();
			if (item)
			{
				self->mPlacesList->selectByValue(item->getValue());
				self->mPlacesList->scrollToShowSelected();
			}
		}
	}
}

//static
void HBFloaterTeleportHistory::onButtonClose(void* data)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (self)
	{
		self->close();
	}
}

//static
void HBFloaterTeleportHistory::onRefresh(void* data)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (self)
	{
		LLSD data = self->mTPlist;
		self->populateLists(data);
	}
}

//static
void HBFloaterTeleportHistory::onTeleport(void* data)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (!self || gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
	{
		return;
	}

	std::string region;
	LLVector3 pos;
	if (!self->getSelectedLocation(region, pos))
	{
		return;
	}

	// Build the position SLURL for the TP destination
	LLSLURL slurl(region, pos);

	// Build the app SLURL for instant teleport to destination
	std::string app_slurl = LLGridManager::getInstance()->getAppSLURLBase();
	app_slurl += "/teleport/" + slurl.getLocationString();

	LL_DEBUGS("Teleport") << "Teleport SLURL: " << app_slurl << LL_ENDL;

	// Dispatch it
	LLMediaCtrl* web = NULL;
	LLURLDispatcher::dispatch(app_slurl, "clicked", web, true);
}

//static
void HBFloaterTeleportHistory::onShowOnMap(void* data)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (!self) return;

	std::string region;
	LLVector3 pos;
	if (!self->getSelectedLocation(region, pos))
	{
		return;
	}

	// Point world map at position
	gFloaterWorldMapp->trackURL(region, pos.mV[VX], pos.mV[VY], pos.mV[VZ]);
	LLFloaterWorldMap::show(NULL, true);
}

// Gets the SLURL of the selected entry and copy it to the clipboard
//static
void HBFloaterTeleportHistory::onCopySLURL(void* data)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (!self) return;

	std::string region;
	LLVector3 pos;
	if (!self->getSelectedLocation(region, pos))
	{
		return;
	}

	LLSLURL slurl(region, pos);
	gWindowp->copyTextToClipboard(utf8str_to_wstring(slurl.getSLURLString()));
}

//static
void HBFloaterTeleportHistory::onRemove(LLUICtrl* ctrl, void* data)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (!self || !ctrl) return;

	std::string operation = ctrl->getValue().asString();
	if (operation == "remove_all")
	{
		self->mPlacesList->clearRows();
		self->mTPlist.clear();
	}
	else
	{
		LLScrollListItem* itemp = self->mPlacesList->getFirstSelected();
		if (!itemp) return;

		if (operation == "remove_older" || operation == "remove_newer")
		{
			self->removePlacesListComment();

			std::string match =
				itemp->getColumn(LIST_TIMESTAMP)->getValue().asString();
			std::vector<LLScrollListItem*> items =
				self->mPlacesList->getAllData();
			std::string date;
			bool newer = operation == "remove_newer";
			for (S32 i = 0, count = items.size(); i < count; ++i)
			{
				itemp = items[i];
				date = itemp->getColumn(LIST_TIMESTAMP)->getValue().asString();
				S32 result = date.compare(match);
				if ((newer && result > 0) || (!newer && result < 0))
				{
					S32 number = itemp->getValue().asInteger();
					self->mTPlist[number] = LLSD::emptyArray();
				}
			}
			self->saveList();
			self->loadEntries();
		}
		else if (operation == "remove_parcel")
		{
			self->removePlacesListComment();

			std::string match =
				itemp->getColumn(LIST_PARCEL)->getValue().asString();
			LLStringUtil::trim(match);
			LLStringUtil::toLower(match);
			std::vector<LLScrollListItem*> items =
				self->mPlacesList->getAllData();
			std::string name;
			for (S32 i = 0, count = items.size(); i < count; ++i)
			{
				itemp = items[i];
				name = itemp->getColumn(LIST_PARCEL)->getValue().asString();
				LLStringUtil::trim(name);
				LLStringUtil::toLower(name);
				if (name == match)
				{
					S32 number = itemp->getValue().asInteger();
					self->mTPlist[number] = LLSD::emptyArray();
				}
			}
			self->saveList();
			self->loadEntries();
		}
		else if (operation == "remove_region")
		{
			self->removePlacesListComment();

			std::string match =
				itemp->getColumn(LIST_REGION)->getValue().asString();
			std::vector<LLScrollListItem*> items =
				self->mPlacesList->getAllData();
			std::string name;
			for (S32 i = 0, count = items.size(); i < count; ++i)
			{
				itemp = items[i];
				name = itemp->getColumn(LIST_REGION)->getValue().asString();
				if (name == match)
				{
					S32 number = itemp->getValue().asInteger();
					self->mTPlist[number] = LLSD::emptyArray();
				}
			}
			self->saveList();
			self->loadEntries();
		}
		else // "remove_entry" in pull-down list or direct click on the button
		{
			S32 number = itemp->getValue().asInteger();
			self->mPlacesList->deleteItem(itemp);
			// NOTE: it is important *not* to use erase() here: the entry must
			// be kept in the LLSD so that mCount and append() stay in sync
			// (and the TP numbering stays in order). The xml file and LLSD
			// will be cleaned up from the empty arrays on next loadEntries()
			// call.
			self->mTPlist[number] = LLSD::emptyArray();
			self->saveList();
		}
	}

	self->setButtonsStatus();
}

//static
void HBFloaterTeleportHistory::onSearchEdit(const std::string& search_string,
											void* data)
{
	HBFloaterTeleportHistory* self = (HBFloaterTeleportHistory*)data;
	if (self)
	{
		self->mSearchString = search_string;
		LLStringUtil::toLower(self->mSearchString);
		self->updateSearchResults();
	}
}
