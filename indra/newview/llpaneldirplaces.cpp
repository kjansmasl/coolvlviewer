/**
 * @file llpaneldirplaces.cpp
 * @brief "Places" panel in the Search floater
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

#include "llpaneldirplaces.h"

#include "llcheckboxctrl.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "llqueryflags.h"
#include "llregionflags.h"
#include "llmessage.h"

#include "llagent.h"
#include "hbfloatersearch.h"
#include "llpaneldirbrowser.h"
#include "llviewercontrol.h"
#include "llworldmap.h"

LLPanelDirPlaces::LLPanelDirPlaces(const std::string& name,
								   HBFloaterSearch* floater)
:	LLPanelDirBrowser(name, floater)
{
	mMinSearchChars = 3;
}

bool LLPanelDirPlaces::postBuild()
{
	LLPanelDirBrowser::postBuild();

	mSearchEditor = getChild<LLSearchEditor>("search_text");
	mSearchEditor->setSearchCallback(onSearchEdit, this);

	childSetAction("search_btn", onClickSearchCore, this);
	childDisable("search_btn");

	mCurrentSortColumn = "dwell";
	mCurrentSortAscending = false;

	// Don't prepopulate the places list, as it hurts the database as of
	// 2006-12-04. JC
	//initialQuery();

	return true;
}

//virtual
void LLPanelDirPlaces::draw()
{
	updateMaturityCheckbox();
	LLPanelDirBrowser::draw();
}

//virtual
void LLPanelDirPlaces::performQuery()
{
	std::string place_name = mSearchEditor->getText();
	if (place_name.length() < mMinSearchChars)
	{
		return;
	}

    // "hi " is three chars but not a long-enough search
	std::string query_string = place_name;
	LLStringUtil::trim(query_string);
	bool query_was_filtered = (query_string != place_name);

	// Possible we threw away all the short words in the query so check length
	if (query_string.length() < mMinSearchChars)
	{
		gNotifications.add("SeachFilteredOnShortWordsEmpty");
		return;
	}

	// If we filtered something out, display a popup
	if (query_was_filtered)
	{
		LLSD args;
		args["FINALQUERY"] = query_string;
		gNotifications.add("SeachFilteredOnShortWords", args);
	}

	std::string catstring = childGetValue("Category").asString();
	// Because LLParcel::C_ANY is -1, must do special check
	S32 category = 0;
	if (catstring == "any")
	{
		category = LLParcel::C_ANY;
	}
	else
	{
		category = LLParcel::getCategoryFromString(catstring);
	}

	bool inc_pg = !mIncPGCheck || mIncPGCheck->getValue().asBoolean();
	bool inc_mature = mIncMatureCheck &&
					  mIncMatureCheck->getValue().asBoolean();
	bool inc_adult = mIncAdultCheck && mIncAdultCheck->getValue().asBoolean();

	U32 flags = 0x0;
	if (inc_pg)		flags |= DFQ_INC_PG;
	if (inc_mature)	flags |= DFQ_INC_MATURE;
	if (inc_adult)	flags |= DFQ_INC_ADULT;

	// Pack old query flag in case we are talking to an old server
	if ((flags & DFQ_INC_PG) == DFQ_INC_PG &&
		(flags & DFQ_INC_MATURE) != DFQ_INC_MATURE)
	{
		flags |= DFQ_PG_PARCELS_ONLY;
	}

	if (flags == 0x0)
	{
		gNotifications.add("NoContentToSearch");
		return;
	}

	queryCore(query_string, category, flags);
}

void LLPanelDirPlaces::initialQuery()
{
	// All Linden locations in PG/Mature sims, any name.
	U32 flags = DFQ_INC_PG | DFQ_INC_MATURE;
	queryCore(LLStringUtil::null, LLParcel::C_LINDEN, flags);
}

void LLPanelDirPlaces::queryCore(const std::string& name, S32 category,
								 U32 flags)
{
	setupNewSearch();

	// JC: Sorting by dwell severely impacts the performance of the query.
	// Instead of sorting on the dataserver, we sort locally once the results
	// are received.
	// IW: Re-enabled dwell sort based on new 3-character minimum description
	flags |= DFQ_DWELL_SORT;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("DirPlacesQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("QueryData");
	msg->addUUID("QueryID", getSearchID());
	msg->addString("QueryText", name);
	msg->addU32("QueryFlags", flags);
	msg->addS8("Category", (S8)category);
	// No longer support queries by region name, too many regions
	// for combobox, no easy way to do autocomplete. JC
	msg->addString("SimName", "");
	msg->addS32Fast(_PREHASH_QueryStart, mSearchStart);
	gAgent.sendReliableMessage();
}
