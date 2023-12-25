/**
 * @file llpaneldirevents.cpp
 * @brief Events listing in the Find directory.
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

#include "llviewerprecompiledheaders.h"

#include <sstream>

#include "llpaneldirevents.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llnotifications.h"
#include "llqueryflags.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llgridmanager.h"
#include "llpaneldirbrowser.h"
#include "llpanelevent.h"
#include "llviewercontrol.h"

bool gDisplayEventHack = false;

// Helper function
static std::string get_event_date(S32 relative_day)
{
	// Get time UTC
	time_t utc_time = time_corrected();

	// Correct for offset
	constexpr S32 SECONDS_PER_DAY = 24 * 60 * 60;
	utc_time += relative_day * SECONDS_PER_DAY;

	return LLGridManager::getTimeStamp(utc_time, "%m-%d", false);	
}

LLPanelDirEvents::LLPanelDirEvents(const std::string& name,
								   HBFloaterSearch* floater)
:	LLPanelDirBrowser(name, floater),
	mDoneQuery(false),
	mDay(0)
{
	// More results per page for this
	mResultsPerPage = 200;
}

//virtual
bool LLPanelDirEvents::postBuild()
{
	LLPanelDirBrowser::postBuild();

	childSetCommitCallback("date_mode", onDateModeCallback, this);

	childSetAction("<<", onBackBtn, this);
	childSetAction(">>", onForwardBtn, this);

	childSetAction("Today", onClickToday, this);

	childSetAction("search_btn", onClickSearchCore, this);
	setDefaultBtn("search_btn");

	mDeleteButton = getChild<LLButton>("Delete");
	mDeleteButton->setClickedCallback(onClickDelete, this);
	mDeleteButton->setEnabled(false);
	mDeleteButton->setVisible(false);

	onDateModeCallback(NULL, this);

	mCurrentSortColumn = "time";

	if (!gDisplayEventHack)
	{
		setDay(0);	// For today
	}
	gDisplayEventHack = false;

	return true;
}

//virtual
void LLPanelDirEvents::draw()
{
	refresh();
	LLPanelDirBrowser::draw();
}

//virtual
void LLPanelDirEvents::refresh()
{
	bool godlike = gAgent.isGodlike();
	mDeleteButton->setEnabled(godlike);
	mDeleteButton->setVisible(godlike);
	updateMaturityCheckbox();
}

void LLPanelDirEvents::setDay(S32 day)
{
	mDay = day;
	childSetValue("date_text", get_event_date(day));
}

//virtual
void LLPanelDirEvents::performQuery()
{
	// event_id 0 will perform no delete action.
	performQueryOrDelete(0);
}

void LLPanelDirEvents::performQueryOrDelete(U32 event_id)
{
	childSetValue("date_text", get_event_date(mDay));

	mDoneQuery = true;

	bool inc_pg = !mIncPGCheck || mIncPGCheck->getValue().asBoolean();
	bool inc_mature = mIncMatureCheck &&
					  mIncMatureCheck->getValue().asBoolean();
	bool inc_adult = mIncAdultCheck && mIncAdultCheck->getValue().asBoolean();

	U32 scope = DFQ_DATE_EVENTS;
	if (gAgent.wantsPGOnly())	scope |= DFQ_PG_SIMS_ONLY;
	if (inc_pg)					scope |= DFQ_INC_PG;
	if (inc_mature)				scope |= DFQ_INC_MATURE;
	if (inc_adult)				scope |= DFQ_INC_ADULT;

	// Add old query flags in case we are talking to an old server
	if (inc_pg && !inc_mature)
	{
		scope |= DFQ_PG_EVENTS_ONLY;
	}

	if (!(scope & (DFQ_INC_PG | DFQ_INC_MATURE | DFQ_INC_ADULT)))
	{
		gNotifications.add("NoContentToSearch");
		return;
	}

	setupNewSearch();

	std::ostringstream params;

	// Date mode for the search
	if ("current" == childGetValue("date_mode").asString())
	{
		params << "u|";
	}
	else
	{
		params << mDay << "|";
	}

	// Categories are stored in the database in table indra.event_category
	// XML must match.
	U32 cat_id = childGetValue("category combo").asInteger();

	params << cat_id << "|";
	params << childGetValue("search_text").asString();

	// send the message
	LLMessageSystem* msg = gMessageSystemp;
	if (event_id == 0)
	{
		sendDirFindQuery(msg, mSearchID, params.str(), scope, mSearchStart);
	}
	else
	{
		// This delete will also perform a query.
		msg->newMessageFast(_PREHASH_EventGodDelete);

		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

		msg->nextBlockFast(_PREHASH_EventData);
		msg->addU32Fast(_PREHASH_EventID, event_id);

		msg->nextBlockFast(_PREHASH_QueryData);
		msg->addUUIDFast(_PREHASH_QueryID, mSearchID);
		msg->addStringFast(_PREHASH_QueryText, params.str());
		msg->addU32Fast(_PREHASH_QueryFlags, scope);
		msg->addS32Fast(_PREHASH_QueryStart, mSearchStart);
		gAgent.sendReliableMessage();
	}
}

//static
void LLPanelDirEvents::onDateModeCallback(LLUICtrl* ctrl, void* data)
{
	LLPanelDirEvents* self = (LLPanelDirEvents*)data;
	if (!self) return;

	if (self->childGetValue("date_mode").asString() == "date")
	{
		self->childEnable("Today");
		self->childEnable(">>");
		self->childEnable("<<");
	}
	else
	{
		self->childDisable("Today");
		self->childDisable(">>");
		self->childDisable("<<");
	}
}

//static
void LLPanelDirEvents::onClickToday(void* data)
{
	LLPanelDirEvents* self = (LLPanelDirEvents*)data;
	if (self)
	{
		self->resetSearchStart();
		self->setDay(0);
		self->performQuery();
	}
}

//static
void LLPanelDirEvents::onBackBtn(void* data)
{
	LLPanelDirEvents* self = (LLPanelDirEvents*)data;
	if (self)
	{
		self->resetSearchStart();
		self->setDay(self->mDay - 1);
		self->performQuery();
	}
}

//static
void LLPanelDirEvents::onForwardBtn(void* data)
{
	LLPanelDirEvents* self = (LLPanelDirEvents*)data;
	if (self)
	{
		self->resetSearchStart();
		self->setDay(self->mDay + 1);
		self->performQuery();
	}
}

//static
void LLPanelDirEvents::onClickDelete(void* data)
{
	LLPanelDirEvents* self = (LLPanelDirEvents*)data;
	if (self)
	{
		U32 event_id = self->getSelectedEventID();
		if (event_id)
		{
			self->performQueryOrDelete(event_id);
		}
	}
}
