/**
 * @file llpaneldirfind.cpp
 * @brief The "All" panel in the Search floater.
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

#include "llpaneldirfind.h"

#include "llcheckboxctrl.h"
#include "llclassifiedflags.h"
#include "llparcel.h"
#include "llqueryflags.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "hbfloatersearch.h"
#include "llpaneldirbrowser.h"

LLPanelDirFind::LLPanelDirFind(const std::string& name,
								   HBFloaterSearch* floater)
:	LLPanelDirBrowser(name, floater)
{
	mMinSearchChars = 3;
}

bool LLPanelDirFind::postBuild()
{
	LLPanelDirBrowser::postBuild();

	mSearchEditor = getChild<LLSearchEditor>("search_text");
	mSearchEditor->setSearchCallback(LLPanelDirBrowser::onSearchEdit, this);

	childSetAction("search_btn", onClickSearch, this);
	childDisable("search_btn");
	setDefaultBtn("search_btn");

	return true;
}

//virtual
void LLPanelDirFind::draw()
{
	updateMaturityCheckbox();
	LLPanelDirBrowser::draw();
}

//virtual
void LLPanelDirFind::search(const std::string& search_text)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;

	bool inc_pg = !mIncPGCheck || mIncPGCheck->getValue().asBoolean();
	bool inc_mature = mIncMatureCheck && mIncMatureCheck->getValue().asBoolean();
	bool inc_adult = mIncAdultCheck && mIncAdultCheck->getValue().asBoolean();
	if (!(inc_pg || inc_mature || inc_adult))
	{
		gNotifications.add("NoContentToSearch");
		return;
	}

	setupNewSearch();

	// Figure out scope
	U32 scope = 0x0;
	scope |= DFQ_PEOPLE;	// people (not just online = 0x01 | 0x02)
	// places handled below
	scope |= DFQ_EVENTS;	// events
	scope |= DFQ_GROUPS;	// groups
	if (inc_pg)
	{
		scope |= DFQ_INC_PG;
	}
	if (inc_mature)
	{
		scope |= DFQ_INC_MATURE;
	}
	if (inc_adult)
	{
		scope |= DFQ_INC_ADULT;
	}

	// send the message
	S32 start_row = 0;
	sendDirFindQuery(msg, mSearchID, search_text, scope, start_row);

	// Also look up classified ads. JC 12/2005
	bool filter_auto_renew = false;
	U32 classified_flags = pack_classified_flags_request(filter_auto_renew,
														 inc_pg, inc_mature,
														 inc_adult);
	msg->newMessage("DirClassifiedQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("QueryData");
	msg->addUUID("QueryID", mSearchID);
	msg->addString("QueryText", search_text);
	msg->addU32("QueryFlags", classified_flags);
	msg->addU32("Category", 0);	// all categories
	msg->addS32("QueryStart", 0);
	gAgent.sendReliableMessage();

	// Need to use separate find places query because places are
	// sent using the more compact DirPlacesReply message.
	U32 query_flags = DFQ_DWELL_SORT;
	if (inc_pg)
	{
		query_flags |= DFQ_INC_PG;
	}
	if (inc_mature)
	{
		query_flags |= DFQ_INC_MATURE;
	}
	if (inc_adult)
	{
		query_flags |= DFQ_INC_ADULT;
	}
	msg->newMessage("DirPlacesQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("QueryData");
	msg->addUUID("QueryID", mSearchID);
	msg->addString("QueryText", search_text);
	msg->addU32("QueryFlags", query_flags);
	msg->addS32("QueryStart", 0); // Always get the first 100 when using find ALL
	msg->addS8("Category", LLParcel::C_ANY);
	msg->addString("SimName", NULL);
	gAgent.sendReliableMessage();

	mSearchEditor->setText(search_text);
}

//static
void LLPanelDirFind::onCommitScope(LLUICtrl* ctrl, void* userdata)
{
	LLPanelDirFind* self = (LLPanelDirFind*)userdata;
	if (self)
	{
		self->setFocus(true);
	}
}

//static
void LLPanelDirFind::onClickSearch(void* userdata)
{
	LLPanelDirFind* self = (LLPanelDirFind*)userdata;
	if (self)
	{
		std::string search_text = self->mSearchEditor->getText();
		if (search_text.length() >= self->mMinSearchChars)
		{
			self->search(search_text);
		}
	}
}
