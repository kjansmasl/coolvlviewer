/**
 * @file llpaneldirbrowser.cpp
 * @brief LLPanelDirBrowser class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

// Base class for the various search panels/results browsers in the Search
// floater. For example, Search > Places is derived from this.

#include "llviewerprecompiledheaders.h"

#include <sstream>

#include "llpaneldirbrowser.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llqueryflags.h"
#include "llscrolllistctrl.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfloateravatarinfo.h"
#include "hbfloatersearch.h"
#include "llgridmanager.h"
#include "llpanelavatar.h"
#include "llpanelclassified.h"
#include "llpaneldirland.h"
#include "llpanelevent.h"
#include "llpanelgroup.h"
#include "llpanelpick.h"
#include "llpanelplace.h"
#include "llproductinforequest.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"

//static
LLPanelDirBrowser::instances_map_t LLPanelDirBrowser::sInstances;

LLPanelDirBrowser::LLPanelDirBrowser(const std::string& name,
									 HBFloaterSearch* floater)
:	LLPanel(name),
	mCurrentSortColumn("name"),
	mCurrentSortAscending(true),
	mSearchStart(0),
	mResultsPerPage(100),
	mResultsReceived(0),
	mMinSearchChars(1),
	mHaveSearchResults(false),
	mDidAutoSelect(true),
	mFloaterSearch(floater),
	mLastWantPGOnly(true),
	mLastCanAccessMature(true),
	mLastCanAccessAdult(true),
	mIncAdultCheck(NULL),
	mIncMatureCheck(NULL),
	mIncPGCheck(NULL),
	mPrevButton(NULL),
	mNextButton(NULL),
	mResultsList(NULL)
{
}

//virtual
bool LLPanelDirBrowser::postBuild()
{
	mIncAdultCheck = getChild<LLCheckBoxCtrl>("incadult", true, false);
	if (mIncAdultCheck)
	{
		mIncMatureCheck = getChild<LLCheckBoxCtrl>("incmature");
		mIncPGCheck = getChild<LLCheckBoxCtrl>("incpg");

		// Note: each check box is associated with a control name. Changing the
		// control automatically changes the check box but the other way
		// around (i.e. doing a setValue() on the checkbox) is not true (only
		// a click in the checkbox does change the control accordingly). This
		// is why we must use gSavedSettings.setBool("control_name") to set
		// the checkboxes in updateMaturityCheckbox(), thus the necessity to
		// get the control names (we cache them for speed).
		mControlNameAdult = mIncAdultCheck->getControlName();
		mControlNameMature = mIncMatureCheck->getControlName();
		mControlNamePG = mIncPGCheck->getControlName();

		updateMaturityCheckbox(true);	// true to force an update
	}

	mPrevButton = getChild<LLButton>("< Prev", true, false);
	if (mPrevButton)
	{
		mPrevButton->setClickedCallback(onClickPrev, this);
		mPrevButton->setVisible(false);

		mNextButton = getChild<LLButton>("Next >", true, false);
		mNextButton->setClickedCallback(onClickNext, this);
		mNextButton->setVisible(false);

		mResultsList = getChild<LLScrollListCtrl>("results");
		mResultsList->setCommitCallback(onCommitList);
		mResultsList->setCallbackUserData(this);
	}

	return true;
}

//virtual
LLPanelDirBrowser::~LLPanelDirBrowser()
{
	sInstances.erase(mSearchID);
}

//virtual
void LLPanelDirBrowser::draw()
{
	// *HACK: if the results panel has data, we want to select the first item.
	// Unfortunately, we do not know when the find is actually done, so only do
	// this if it has been some time since the last packet of results was
	// received.
	if (mLastResultTimer.getElapsedTimeF32() > 1.f)
	{
		if (!mDidAutoSelect && mResultsList && !mResultsList->hasFocus())
		{
			if (mResultsList->getCanSelect())
			{
				// Select first item by default
				mResultsList->selectFirstItem();
				mResultsList->setFocus(true);
			}
			// Request specific data from the server
			onCommitList(NULL, this);
		}
		mDidAutoSelect = true;
	}

	LLPanel::draw();
}

//virtual
void LLPanelDirBrowser::nextPage()
{
	mSearchStart += mResultsPerPage;
	if (mPrevButton)
	{
		mPrevButton->setVisible(true);
	}
	performQuery();
}

//virtual
void LLPanelDirBrowser::prevPage()
{
	mSearchStart -= mResultsPerPage;
	if (mPrevButton)
	{
		mPrevButton->setVisible(mSearchStart > 0);
	}
	performQuery();
}

void LLPanelDirBrowser::resetSearchStart()
{
	mSearchStart = 0;
	if (mPrevButton)
	{
		mPrevButton->setVisible(false);
		mNextButton->setVisible(false);
	}
}

void LLPanelDirBrowser::updateResultCount()
{
	if (!mResultsList) return;

	S32 result_count = mHaveSearchResults ? mResultsList->getItemCount() : 0;

	std::string result_text;
	if (mNextButton && mNextButton->getVisible())
	{
		// Item count be off by a few if bogus items sent from database
		// Just use the number of results per page. JC
		result_text = llformat(">%d found", mResultsPerPage);
	}
	else
	{
		result_text = llformat("%d found", result_count);
	}

	childSetValue("result_text", result_text);

	if (result_count == 0)
	{
		// Add none found response
		if (mResultsList->getItemCount() == 0)
		{
			// *TODO: Translate
			mResultsList->addCommentText("None found.");
			mResultsList->operateOnAll(LLScrollListCtrl::OP_DESELECT);
		}
	}
	else
	{
		mResultsList->setEnabled(true);
	}
}

//static
void LLPanelDirBrowser::onClickPrev(void* data)
{
	LLPanelDirBrowser* self = (LLPanelDirBrowser*)data;
	if (self)
	{
		self->prevPage();
	}
}

//static
void LLPanelDirBrowser::onClickNext(void* data)
{
	LLPanelDirBrowser* self = (LLPanelDirBrowser*)data;
	if (self)
	{
		self->nextPage();
	}
}

//static
std::string LLPanelDirBrowser::filterShortWords(const std::string source_str,
												S32 shortest_word_length,
												bool& was_filtered)
{
	// Degenerate case
	if (!source_str.length())
	{
		return "";
	}

	std::stringstream codec(source_str);
	std::string each_word;
	std::vector<std::string> all_words;

	while (codec >> each_word)
	{
        all_words.push_back(each_word);
	}

	std::ostringstream dest_string("");

	was_filtered = false;

	std::vector<std::string>::iterator iter = all_words.begin();
	while (iter != all_words.end())
	{
		if ((S32)iter->length() >= shortest_word_length)
		{
			dest_string << *iter;
			dest_string << " ";
		}
		else
		{
			was_filtered = true;
		}

		++iter;
	}

	return dest_string.str();
}

void LLPanelDirBrowser::updateMaturityCheckbox(bool force)
{
	if (!mIncAdultCheck)
	{
		return;
	}

	// You only have a choice if your maturity is 'mature' or higher. Logic: if
	// you are not at least mature, hide the mature and adult options. After
	// that, enable only the options you can legitimately choose. If you're PG
	// only, show you the checkbox but don't let you change it.
	bool pg_only_access = gAgent.wantsPGOnly();
	bool mature_access = gAgent.canAccessMature();
	bool adult_access = gAgent.canAccessAdult();

	if (!force && pg_only_access == mLastWantPGOnly &&
		mature_access == mLastCanAccessMature &&
		adult_access == mLastCanAccessAdult)
	{
		// Nothing to update
		return;
	}

	mLastWantPGOnly = pg_only_access;
	mLastCanAccessMature = mature_access;
	mLastCanAccessAdult = adult_access;

	if (pg_only_access)
	{
		// Teens do not get mature/adult choices
		gSavedSettings.setBool(mControlNamePG.c_str(), true);
		gSavedSettings.setBool(mControlNameMature.c_str(), false);
		gSavedSettings.setBool(mControlNameAdult.c_str(), false);
		mIncPGCheck->setEnabled(false);
		mIncMatureCheck->setVisible(false);
		mIncAdultCheck->setVisible(false);
	}
	else
	{
		mIncPGCheck->setEnabled(true);
		mIncMatureCheck->setVisible(true);
		mIncAdultCheck->setVisible(true);

		if (mature_access)
		{
			mIncMatureCheck->setEnabled(true);
		}
		else
		{
			gSavedSettings.setBool(mControlNameMature.c_str(), false);
			mIncMatureCheck->setEnabled(false);
		}

		if (adult_access)
		{
			mIncAdultCheck->setEnabled(true);
		}
		else
		{
			gSavedSettings.setBool(mControlNameAdult.c_str(), false);
			mIncAdultCheck->setEnabled(false);
		}
	}
}

void LLPanelDirBrowser::selectByUUID(const LLUUID& id)
{
	if (!mResultsList) return;

	if (mResultsList->setCurrentByID(id))
	{
		// We got it, do not wait for network. Do not bother looking for this
		// in the draw loop.
		mWantSelectID.setNull();
		// Make sure UI updates.
		onCommitList(NULL, this);
	}
	else
	{
		// Waiting for this item from the network
		mWantSelectID = id;
	}
}

void LLPanelDirBrowser::selectEventByID(S32 event_id)
{
	if (mFloaterSearch)
	{
		LLPanelEvent* panelp = mFloaterSearch->mPanelEventp;
		if (panelp)
		{
			panelp->setVisible(true);
			panelp->setEventID(event_id);
		}
	}
}

U32 LLPanelDirBrowser::getSelectedEventID() const
{
	if (mFloaterSearch)
	{
		LLPanelEvent* panelp = mFloaterSearch->mPanelEventp;
		if (panelp)
		{
			return panelp->getEventID();
		}
	}
	return 0;
}

void LLPanelDirBrowser::getSelectedInfo(LLUUID* id, S32* type)
{
	if (!mResultsList) return;

	LLSD id_sd = mResultsList->getValue();
	*id = id_sd.asUUID();
	std::string id_str = id_sd.asString();
	*type = mResultsContents[id_str]["type"];
}

//static
void LLPanelDirBrowser::onCommitList(LLUICtrl* ctrl, void* data)
{
	LLPanelDirBrowser* self = (LLPanelDirBrowser*)data;
	if (!self || !self->mResultsList) return;

	// Start with everyone invisible
	if (self->mFloaterSearch)
	{
		self->mFloaterSearch->hideAllDetailPanels();
	}

	if (!self->mResultsList->getCanSelect())
	{
		return;
	}

	std::string id_str = self->mResultsList->getValue().asString();
	if (id_str.empty())
	{
		return;
	}

	LLSD item_id = self->mResultsList->getCurrentID();
	S32 type = self->mResultsContents[id_str]["type"];
	if (type == EVENT_CODE)
	{
		// All but events use the UUID above
		item_id = self->mResultsContents[id_str]["event_id"];
	}
	self->showDetailPanel(type, item_id);

	if (type == FOR_SALE_CODE && self->mFloaterSearch)
	{
		LLPanelPlace* panelp = self->mFloaterSearch->mPanelPlaceSmallp;
		if (panelp)
		{
			std::string land_type =
				self->mResultsContents[id_str]["landtype"].asString();
			panelp->setLandTypeString(land_type);
		}
	}
}

void LLPanelDirBrowser::showDetailPanel(S32 type, LLSD id)
{
	if (!mFloaterSearch) return;

	switch (type)
	{
		case AVATAR_CODE:
		{
			LLPanelAvatar* panelp = mFloaterSearch->mPanelAvatarp;
			if (panelp)
			{
				panelp->setVisible(true);
				panelp->setAvatarID(id.asUUID(), LLStringUtil::null,
									ONLINE_STATUS_NO);
			}
			break;
		}

		case EVENT_CODE:
		{
			mFloaterSearch->hideAllDetailPanels();
			LLPanelEvent* panelp = mFloaterSearch->mPanelEventp;
			if (panelp)
			{
				U32 event_id = (U32)id.asInteger();
				panelp->setVisible(true);
				panelp->setEventID(event_id);
			}
			break;
		}

		case GROUP_CODE:
		{
			LLPanel* holderp = mFloaterSearch->mPanelGroupHolderp;
			if (holderp)
			{
				holderp->setVisible(true);
			}
			LLPanelGroup* panelp = mFloaterSearch->mPanelGroupp;
			if (panelp)
			{
				panelp->setVisible(true);
				panelp->setGroupID(id.asUUID());
			}
			break;
		}

		case CLASSIFIED_CODE:
		{
			LLPanelClassified* panelp = mFloaterSearch->mPanelClassifiedp;
			if (panelp)
			{
				panelp->setVisible(true);
				panelp->setClassifiedID(id.asUUID());
				panelp->sendClassifiedInfoRequest();
			}
			break;
		}

		case FOR_SALE_CODE:
		case AUCTION_CODE:
		{
			LLPanelPlace* panelp = mFloaterSearch->mPanelPlaceSmallp;
			if (panelp)
			{
				panelp->setVisible(true);
				panelp->resetLocation();
				panelp->setParcelID(id.asUUID());
			}
			break;
		}

		case PLACE_CODE:
		case POPULAR_CODE:
		{
			LLPanelPlace* panelp = mFloaterSearch->mPanelPlacep;
			if (panelp)
			{
				panelp->setVisible(true);
				panelp->resetLocation();
				panelp->setParcelID(id.asUUID());
			}
			break;
		}

		default:
			llwarns << "Unknown event type: " << type << llendl;
	}
}

void LLPanelDirBrowser::processDirPeopleReply(LLMessageSystem* msg, void**)
{
	LLUUID query_id;
	msg->getUUIDFast(_PREHASH_QueryData,_PREHASH_QueryID, query_id);

	LLPanelDirBrowser* self = get_ptr_in_map(sInstances, query_id);
	if (!self || !self->mResultsList)
	{
		// Data from an old query
		return;
	}

	self->mHaveSearchResults = true;

	if (!self->mResultsList->getCanSelect())
	{
		self->mResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);
		self->mResultsContents = LLSD();
	}

	S32 rows = msg->getNumberOfBlocksFast(_PREHASH_QueryReplies);
	self->mResultsReceived += rows;

	LLUUID agent_id;
	std::string first_name, last_name;
	rows = self->showNextButton(rows);
	for (S32 i = 0; i < rows; ++i)
	{
		msg->getStringFast(_PREHASH_QueryReplies, _PREHASH_FirstName,
						   first_name, i);
		msg->getStringFast(_PREHASH_QueryReplies, _PREHASH_LastName,
						   last_name, i);
		msg->getUUIDFast(_PREHASH_QueryReplies, _PREHASH_AgentID,
						 agent_id, i);
#if 0	// Legacy data, no more used.
		msg->getU8Fast(_PREHASH_QueryReplies, _PREHASH_Online, online, i);
		msg->getStringFast(_PREHASH_QueryReplies, _PREHASH_Group, group, i);
		msg->getS32Fast(_PREHASH_QueryReplies, _PREHASH_Reputation,
						reputation, i);
#endif

		if (agent_id.isNull())
		{
			continue;
		}

		LLSD content;

		LLSD row;
		row["id"] = agent_id;

		// We do not show online status in the finder anymore, so just use the
		// 'offline' icon as the generic 'person' icon
		LLSD& columns = row["columns"];
		columns[0]["column"] = "icon";
		columns[0]["type"] = "icon";
		columns[0]["value"] = "icon_avatar_offline.tga";

		content["type"] = AVATAR_CODE;

		std::string fullname = first_name + " " + last_name;
		columns[1]["column"] = "name";
		columns[1]["value"] = fullname;
		columns[1]["font"] = "SANSSERIF";

		content["name"] = fullname;

		self->mResultsList->addElement(row);
		self->mResultsContents[agent_id.asString()] = content;
	}

	self->mResultsList->sortByColumn(self->mCurrentSortColumn,
									 self->mCurrentSortAscending);
	self->updateResultCount();

	// Poke the result received timer
	self->mLastResultTimer.reset();
	self->mDidAutoSelect = false;
}

void LLPanelDirBrowser::processDirPlacesReply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id, query_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	msg->getUUID("QueryData", "QueryID", query_id);

	if (msg->getNumberOfBlocks("StatusData"))
	{
		U32 status;
		msg->getU32("StatusData", "Status", status);
		if (status & STATUS_SEARCH_PLACES_BANNEDWORD)
		{
			gNotifications.add("SearchWordBanned");
		}
	}

	LLPanelDirBrowser* self = get_ptr_in_map(sInstances, query_id);
	if (!self)
	{
		// Data from an old query
		return;
	}

	self->mHaveSearchResults = true;

	if (!self->mResultsList) return;

	if (!self->mResultsList->getCanSelect())
	{
		self->mResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);
		self->mResultsContents = LLSD();
	}

	S32 count = msg->getNumberOfBlocks("QueryReplies");
	self->mResultsReceived += count;

	LLUUID parcel_id;
	std::string	name;
	F32	dwell;
	bool is_for_sale, is_auction;
	count = self->showNextButton(count);
	for (S32 i = 0; i < count ; ++i)
	{
		msg->getUUID("QueryReplies", "ParcelID", parcel_id, i);
		msg->getString("QueryReplies", "Name", name, i);
		msg->getBool("QueryReplies", "ForSale", is_for_sale, i);
		msg->getBool("QueryReplies", "Auction", is_auction, i);
		msg->getF32("QueryReplies", "Dwell", dwell, i);

		if (parcel_id.isNull())
		{
			continue;
		}

		S32 type;
		LLSD row = self->createLandSale(parcel_id, is_auction, is_for_sale,
										name, &type);
		LLSD content;
		content["type"] = type;
		content["name"] = name;

		LLSD& columns = row["columns"];
		columns[3]["column"] = "dwell";
		columns[3]["value"] = llformat("%.0f", (F64)dwell);
		columns[3]["font"] = "SANSSERIF_SMALL";

		self->mResultsList->addElement(row);
		self->mResultsContents[parcel_id.asString()] = content;
	}

	self->mResultsList->sortByColumn(self->mCurrentSortColumn,
									 self->mCurrentSortAscending);
	self->updateResultCount();

	// Poke the result received timer
	self->mLastResultTimer.reset();
	self->mDidAutoSelect = false;
}

void LLPanelDirBrowser::processDirEventsReply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id, query_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	msg->getUUID("QueryData", "QueryID", query_id);

	LLPanelDirBrowser* self = get_ptr_in_map(sInstances, query_id);
	if (!self)
	{
		return;
	}

	if (msg->getNumberOfBlocks("StatusData"))
	{
		U32 status;
		msg->getU32("StatusData", "Status", status);
		if (status & STATUS_SEARCH_EVENTS_BANNEDWORD)
		{
			gNotifications.add("SearchWordBanned");
		}
	}

	self->mHaveSearchResults = true;

	if (!self->mResultsList) return;

	if (!self->mResultsList->getCanSelect())
	{
		self->mResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);
		self->mResultsContents = LLSD();
	}

	S32 rows = msg->getNumberOfBlocks("QueryReplies");
	self->mResultsReceived += rows;

	bool show_pg = gSavedSettings.getBool("ShowPGEvents");
	bool show_mature = gSavedSettings.getBool("ShowMatureEvents");
	bool show_adult = gSavedSettings.getBool("ShowAdultEvents");

	std::string time_format = "%m-%d " +
							  gSavedSettings.getString("ShortTimeFormat");
	LLUUID owner_id;
	std::string name;
	rows = self->showNextButton(rows);
	for (S32 i = 0; i < rows; ++i)
	{
		U32 event_id;
		U32 unix_time;
		U32 event_flags;

		msg->getUUID("QueryReplies", "OwnerID", owner_id, i);
		msg->getString("QueryReplies", "Name", name, i);
		msg->getU32("QueryReplies", "EventID", event_id, i);
#if 0
		msg->getString("QueryReplies", "Date", date, i);
#endif
		msg->getU32("QueryReplies", "UnixTime", unix_time, i);
		msg->getU32("QueryReplies", "EventFlags", event_flags, i);

		// Skip empty events
		if (owner_id.isNull())
		{
			// RN: should this check event_id instead ?
			llwarns << "skipped event due to owner_id null, event_id "
					<< event_id << llendl;
			continue;
		}

		// Skip events that do not match the flags; there's no PG flag, so we
		// make sure neither adult nor mature is set
		if (!show_pg &&
			((event_flags & (EVENT_FLAG_ADULT | EVENT_FLAG_MATURE)) ==
				EVENT_FLAG_NONE))
		{
			continue;
		}

		if (!show_mature && (event_flags & EVENT_FLAG_MATURE))
		{
			continue;
		}

		if (!show_adult && (event_flags & EVENT_FLAG_ADULT))
		{
			continue;
		}

		LLSD content;
		content["type"] = EVENT_CODE;
		content["name"] = name;
		content["event_id"] = (S32)event_id;

		LLSD row;
		row["id"] = llformat("%u", event_id);

		LLSD& columns = row["columns"];
		// Column 0 - event icon
		if (event_flags == EVENT_FLAG_ADULT)
		{
			columns[0]["column"] = "icon";
			columns[0]["type"] = "icon";
			columns[0]["value"] = "icon_event_adult.tga";
		}
		else if (event_flags == EVENT_FLAG_MATURE)
		{
			columns[0]["column"] = "icon";
			columns[0]["type"] = "icon";
			columns[0]["value"] = "icon_event_mature.tga";
		}
		else
		{
			columns[0]["column"] = "icon";
			columns[0]["type"] = "icon";
			columns[0]["value"] = "icon_event.tga";
		}

		columns[1]["column"] = "name";
		columns[1]["value"] = name;
		columns[1]["font"] = "SANSSERIF";

		columns[2]["column"] = "date";
		columns[2]["value"] = LLGridManager::getTimeStamp(unix_time,
														  time_format);
		columns[2]["font"] = "SANSSERIF_SMALL";

		columns[3]["column"] = "time";
		columns[3]["value"] = llformat("%u", unix_time);
		columns[3]["font"] = "SANSSERIF_SMALL";

		self->mResultsList->addElement(row, ADD_SORTED);

		std::string id_str = llformat("%u", event_id);
		self->mResultsContents[id_str] = content;
	}

	self->mResultsList->sortByColumn(self->mCurrentSortColumn,
									 self->mCurrentSortAscending);
	self->updateResultCount();

	// Poke the result received timer
	self->mLastResultTimer.reset();
	self->mDidAutoSelect = false;
}

//static
void LLPanelDirBrowser::processDirGroupsReply(LLMessageSystem* msg, void**)
{
	LLUUID query_id;
	msg->getUUIDFast(_PREHASH_QueryData,_PREHASH_QueryID, query_id);

	LLPanelDirBrowser* self = get_ptr_in_map(sInstances, query_id);
	if (!self)
	{
		return;
	}

	self->mHaveSearchResults = true;

	if (!self->mResultsList) return;

	if (!self->mResultsList->getCanSelect())
	{
		self->mResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);
		self->mResultsContents = LLSD();
	}

	S32 rows = msg->getNumberOfBlocksFast(_PREHASH_QueryReplies);
	self->mResultsReceived += rows;

	S32 members;
	F32 search_order;
	LLUUID group_id;
	std::string	group_name;
	rows = self->showNextButton(rows);
	for (S32 i = 0; i < rows; ++i)
	{
		msg->getUUIDFast(_PREHASH_QueryReplies, _PREHASH_GroupID, group_id, i);
		msg->getStringFast(_PREHASH_QueryReplies, _PREHASH_GroupName,
						   group_name, i);
		msg->getS32Fast(_PREHASH_QueryReplies, _PREHASH_Members, members, i);
		msg->getF32Fast(_PREHASH_QueryReplies, _PREHASH_SearchOrder,
						search_order, i);

		if (group_id.isNull())
		{
			continue;
		}

		LLSD content;
		content["type"] = GROUP_CODE;
		content["name"] = group_name;

		LLSD row;
		row["id"] = group_id;

		LLSD& columns = row["columns"];
		columns[0]["column"] = "icon";
		columns[0]["type"] = "icon";
		columns[0]["value"] = "icon_group.tga";

		columns[1]["column"] = "name";
		columns[1]["value"] = group_name;
		columns[1]["font"] = "SANSSERIF";

		columns[2]["column"] = "members";
		columns[2]["value"] = members;
		columns[2]["font"] = "SANSSERIF_SMALL";

		columns[3]["column"] = "score";
		columns[3]["value"] = search_order;

		self->mResultsList->addElement(row);
		self->mResultsContents[group_id.asString()] = content;
	}
	self->mResultsList->sortByColumn(self->mCurrentSortColumn,
									 self->mCurrentSortAscending);
	self->updateResultCount();

	// Poke the result received timer
	self->mLastResultTimer.reset();
	self->mDidAutoSelect = false;
}

//static
void LLPanelDirBrowser::processDirClassifiedReply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Message for wrong agent " << agent_id
				<< " in processDirClassifiedReply" << llendl;
		return;
	}

	LLUUID query_id;
	msg->getUUID("QueryData", "QueryID", query_id);
	LLPanelDirBrowser* self = get_ptr_in_map(sInstances, query_id);
	if (!self)
	{
		return;
	}

	if (msg->getNumberOfBlocks("StatusData"))
	{
		U32 status;
		msg->getU32("StatusData", "Status", status);
		if (status & STATUS_SEARCH_CLASSIFIEDS_BANNEDWORD)
		{
			gNotifications.add("SearchWordBanned");
		}
	}

	self->mHaveSearchResults = true;

	if (!self->mResultsList) return;

	if (!self->mResultsList->getCanSelect())
	{
		self->mResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);
		self->mResultsContents = LLSD();
	}

	S32 num_new_rows = msg->getNumberOfBlocksFast(_PREHASH_QueryReplies);
	self->mResultsReceived += num_new_rows;

	LLUUID classified_id;
	std::string name;
	num_new_rows = self->showNextButton(num_new_rows);
	for (S32 i = 0; i < num_new_rows; ++i)
	{
		msg->getUUID("QueryReplies", "ClassifiedID", classified_id, i);
		msg->getString("QueryReplies", "Name", name, i);
		U32 creation_date = 0;	// unix timestamp
		msg->getU32("QueryReplies","CreationDate", creation_date, i);
		U32 expiration_date = 0;	// future use
		msg->getU32("QueryReplies","ExpirationDate", expiration_date, i);
		S32 price_for_listing = 0;
		msg->getS32("QueryReplies","PriceForListing", price_for_listing, i);

		if (classified_id.notNull())
		{
			self->addClassified(self->mResultsList, classified_id, name,
								creation_date, price_for_listing);

			LLSD content;
			content["type"] = CLASSIFIED_CODE;
			content["name"] = name;
			self->mResultsContents[classified_id.asString()] = content;
		}
	}
	// The server does the initial sort, by price paid per listing and date. JC
	self->updateResultCount();

	// Poke the result received timer
	self->mLastResultTimer.reset();
	self->mDidAutoSelect = false;
}

void LLPanelDirBrowser::processDirLandReply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id, query_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	msg->getUUID("QueryData", "QueryID", query_id);

	LLPanelDirBrowser* browser = get_ptr_in_map(sInstances, query_id);
	if (!browser)
	{
		// Data from an old query
		return;
	}

	// Only handled by LLPanelDirLand
	LLPanelDirLand* self = (LLPanelDirLand*)browser;

	self->mHaveSearchResults = true;

	if (!self->mResultsList) return;

	if (!self->mResultsList->getCanSelect())
	{
		self->mResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);
		self->mResultsContents = LLSD();
	}

	bool use_price = gSavedSettings.getBool("FindLandPrice");
	S32 limit_price = self->childGetValue("priceedit").asInteger();

	bool use_area = gSavedSettings.getBool("FindLandArea");
	S32 limit_area = self->childGetValue("areaedit").asInteger();

	S32 count = msg->getNumberOfBlocks("QueryReplies");
	self->mResultsReceived += count;

	S32 non_auction_count = 0;
	S32	sale_price, actual_area;
	LLUUID parcel_id;
	std::string	name, land_sku, land_type;
	bool auction, for_sale;
	LLProductInfoRequestManager* mgrp =
		LLProductInfoRequestManager::getInstance();
	for (S32 i = 0; i < count; ++i)
	{
		msg->getUUID("QueryReplies", "ParcelID", parcel_id, i);
		msg->getString("QueryReplies", "Name", name, i);
		msg->getBool("QueryReplies", "Auction", auction, i);
		msg->getBool("QueryReplies", "ForSale", for_sale, i);
		msg->getS32("QueryReplies", "SalePrice", sale_price, i);
		msg->getS32("QueryReplies", "ActualArea", actual_area, i);

		if (msg->getSizeFast(_PREHASH_QueryReplies, i,
							 _PREHASH_ProductSKU) > 0)
		{
			msg->getStringFast(_PREHASH_QueryReplies, _PREHASH_ProductSKU,
							   land_sku, i);
			LL_DEBUGS("Land SKU") << "Land sku: " << land_sku << LL_ENDL;
			land_type = mgrp->getDescriptionForSku(land_sku);
		}
		else
		{
			land_sku.clear();
			land_type = LLTrans::getString("unknown");
		}

		if (parcel_id.isNull() || (use_price && sale_price > limit_price) ||
			(use_area && actual_area < limit_area))
		{
			continue;
		}

		S32 type;
		LLSD row = self->createLandSale(parcel_id, auction, for_sale,  name,
										&type);
		LLSD content;
		content["type"] = type;
		content["name"] = name;
		content["landtype"] = land_type;

		std::string buffer = "Auction";
		if (!auction)
		{
			buffer = llformat("%d", sale_price);
			non_auction_count++;
		}

		LLSD& columns = row["columns"];
		columns[3]["column"] = "price";
		columns[3]["value"] = buffer;
		columns[3]["font"] = "SANSSERIF_SMALL";

		buffer = llformat("%d", actual_area);
		columns[4]["column"] = "area";
		columns[4]["value"] = buffer;
		columns[4]["font"] = "SANSSERIF_SMALL";

		if (!auction)
		{
			F32 price_per_meter;
			if (actual_area > 0)
			{
				price_per_meter = (F32)sale_price / (F32)actual_area;
			}
			else
			{
				price_per_meter = 0.f;
			}
			// Prices are usually L$1 - L$10 / meter
			buffer = llformat("%.1f", price_per_meter);
			columns[5]["column"] = "per_meter";
			columns[5]["value"] = buffer;
			columns[5]["font"] = "SANSSERIF_SMALL";
		}
		else
		{
			// Auctions start at L$1 per meter
			columns[5]["column"] = "per_meter";
			columns[5]["value"] = "1.0";
			columns[5]["font"] = "SANSSERIF_SMALL";
		}

		columns[6]["column"] = "landtype";
		columns[6]["value"] = land_type;
		columns[6]["font"] = "SANSSERIF_SMALL";

		self->mResultsList->addElement(row);
		self->mResultsContents[parcel_id.asString()] = content;
	}

	// All auction results are shown on the first page. But they do not count
	// towards the 100 / page limit. So figure out the next button here, when
	// we know how many aren't auctions
	count = self->showNextButton(non_auction_count);

	self->updateResultCount();

	// Poke the result received timer
	self->mLastResultTimer.reset();
	self->mDidAutoSelect = false;
}

void LLPanelDirBrowser::addClassified(LLScrollListCtrl* listp,
									  const LLUUID& pick_id,
									  const std::string& name,
									  U32 creation_date, S32 price_for_listing)
{
	std::string type = llformat("%d", CLASSIFIED_CODE);

	LLSD row;
	row["id"] = pick_id;

	LLSD& columns = row["columns"];
	columns[0]["column"] = "icon";
	columns[0]["type"] = "icon";
	columns[0]["value"] = "icon_top_pick.tga";

	columns[1]["column"] = "name";
	columns[1]["value"] = name;
	columns[1]["font"] = "SANSSERIF";

	columns[2]["column"] = "price";
	columns[2]["value"] = price_for_listing;
	columns[2]["font"] = "SANSSERIF_SMALL";

	listp->addElement(row);
}

LLSD LLPanelDirBrowser::createLandSale(const LLUUID& parcel_id,
									   bool is_auction, bool is_for_sale,
									   const std::string& name, S32* type)
{
	LLSD row;
	row["id"] = parcel_id;

	LLSD& columns = row["columns"];
	// Icon and type
	if (is_auction)
	{
		columns[0]["column"] = "icon";
		columns[0]["type"] = "icon";
		columns[0]["value"] = "icon_auction.tga";

		*type = AUCTION_CODE;
	}
	else if (is_for_sale)
	{
		columns[0]["column"] = "icon";
		columns[0]["type"] = "icon";
		columns[0]["value"] = "icon_for_sale.tga";

		*type = FOR_SALE_CODE;
	}
	else
	{
		columns[0]["column"] = "icon";
		columns[0]["type"] = "icon";
		columns[0]["value"] = "icon_place.tga";

		*type = PLACE_CODE;
	}

	columns[2]["column"] = "name";
	columns[2]["value"] = name;
	columns[2]["font"] = "SANSSERIF";

	return row;
}

void LLPanelDirBrowser::newClassified()
{
	if (!mResultsList) return;

	LLPanelClassified* panelp = mFloaterSearch->mPanelClassifiedp;
	if (panelp)
	{
		// Clear the panel on the right
		panelp->reset();

		// Set up the classified with the info we have created and a sane
		// default position.
		panelp->initNewClassified();

		// We need the ID to select in the list.
		LLUUID classified_id = panelp->getClassifiedID();

		// Put it in the list on the left
		addClassified(mResultsList, classified_id, panelp->getClassifiedName(),
					  0, 0);

		// Select it.
		mResultsList->setCurrentByID(classified_id);

		// Make the right panel visible (should already be)
		panelp->setVisible(true);
	}
}

void LLPanelDirBrowser::setupNewSearch()
{
	sInstances.erase(mSearchID);
	mSearchID.generate();			// Make a new query ID
	sInstances[mSearchID] = this;

	if (mResultsList)
	{
		// Ready the list for results
		mResultsList->operateOnAll(LLScrollListCtrl::OP_DELETE);
		// *TODO: translate
		mResultsList->addCommentText("Searching...");
		mResultsList->setEnabled(false);
	}

	mResultsReceived = 0;
	mHaveSearchResults = false;

	// Set all panels to be invisible
	mFloaterSearch->hideAllDetailPanels();

	updateResultCount();
}

//static
// Called from classifieds, events, groups, land, people, and places
void LLPanelDirBrowser::onClickSearchCore(void* userdata)
{
	LLPanelDirBrowser* self = (LLPanelDirBrowser*)userdata;
	if (self)
	{
		self->resetSearchStart();
		self->performQuery();
	}
}

//static
void LLPanelDirBrowser::sendDirFindQuery(LLMessageSystem* msg,
										 const LLUUID& query_id,
										 const std::string& text,
										 U32 flags, S32 query_start)
{
	msg->newMessage("DirFindQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("QueryData");
	msg->addUUID("QueryID", query_id);
	msg->addString("QueryText", text);
	msg->addU32("QueryFlags", flags);
	msg->addS32("QueryStart", query_start);
	gAgent.sendReliableMessage();
}

//static
void LLPanelDirBrowser::onSearchEdit(const std::string& text, void* data)
{
	LLPanelDirBrowser* self = (LLPanelDirBrowser*)data;
	if (!self) return;

	if ((U32)text.length() >= self->mMinSearchChars)
	{
		self->setDefaultBtn("search_btn");
		self->childEnable("search_btn");
	}
	else
	{
		self->setDefaultBtn();
		self->childDisable("search_btn");
	}
}

// Setups results when shown
void LLPanelDirBrowser::onVisibilityChange(bool new_visibility)
{
	if (new_visibility)
	{
		onCommitList(NULL, this);
	}
	LLPanel::onVisibilityChange(new_visibility);
}

S32 LLPanelDirBrowser::showNextButton(S32 rows)
{
	if (!mPrevButton) return rows;

	// *HACK: this hack does not work for LLPanelDirFind because some other
	// data is being returned as well.
	if (getName() != "find_all_panel")
	{
		// *HACK: The mResultsPerPage+1th entry indicates there are 'more'
		bool show_next = mResultsReceived > mResultsPerPage;
		mNextButton->setVisible(show_next);
		if (show_next)
		{
			rows -= mResultsReceived - mResultsPerPage;
		}
	}
	else if (mPrevButton)
	{
		// Hide page buttons
		mPrevButton->setVisible(false);
		mNextButton->setVisible(false);
	}

	return rows;
}
