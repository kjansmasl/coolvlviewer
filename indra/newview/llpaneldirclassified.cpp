/**
 * @file llpaneldirclassified.cpp
 * @brief Classified panel in the Search floater.
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

#include "llpaneldirclassified.h"

#include "llcheckboxctrl.h"
#include "llclassifiedflags.h"
#include "lllineeditor.h"
#include "llqueryflags.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfloateravatarinfo.h"
#include "hbfloatersearch.h"
#include "llpaneldirbrowser.h"

LLPanelDirClassified::LLPanelDirClassified(const std::string& name,
										   HBFloaterSearch* floater)
:	LLPanelDirBrowser(name, floater)
{
}

bool LLPanelDirClassified::postBuild()
{
	LLPanelDirBrowser::postBuild();

	// 0 or 3+ character searches allowed, exciting
	mSearchEditor = getChild<LLSearchEditor>("search_text");
	mSearchEditor->setSearchCallback(onSearchEditClassified, this);

	childSetAction("search_btn", onClickSearchCore, this);
	childSetAction("browse_btn", onClickSearchCore, this);
	setDefaultBtn("browse_btn");

	childSetAction("Place an Ad...", onClickCreateNewClassified, this);

	mDeleteButton = getChild<LLButton>("Delete");
	mDeleteButton->setClickedCallback(onClickDelete, this);
	mDeleteButton->setEnabled(false);
	mDeleteButton->setVisible(false);

#if 0	// Do not do this every time we open find, it is expensive; require
		// clicking 'search'
	requestClassified();
#endif

	return true;
}

void LLPanelDirClassified::draw()
{
	refresh();
	LLPanelDirBrowser::draw();
}

void LLPanelDirClassified::refresh()
{
	bool godlike = gAgent.isGodlike();
	mDeleteButton->setEnabled(godlike);
	mDeleteButton->setVisible(godlike);
	updateMaturityCheckbox();
}

// Open Profile to Classifieds tab
void LLPanelDirClassified::onClickCreateNewClassified(void* userdata)
{
	LLFloaterAvatarInfo::showFromObject(gAgentID, "Classified");
}

//static
void LLPanelDirClassified::onClickDelete(void* userdata)
{
	LLPanelDirClassified* self = (LLPanelDirClassified*)userdata;

	LLUUID classified_id;
	S32 type;

	self->getSelectedInfo(&classified_id, &type);

	// Clear out the list.  Deleting a classified will cause a refresh to be
	// sent.
	self->setupNewSearch();

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ClassifiedGodDelete);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_ClassifiedID, classified_id);
	msg->addUUIDFast(_PREHASH_QueryID, self->mSearchID);
	gAgent.sendReliableMessage();
}

void LLPanelDirClassified::performQuery()
{
	bool inc_pg = !mIncPGCheck || mIncPGCheck->getValue().asBoolean();
	bool inc_mature = mIncMatureCheck && mIncMatureCheck->getValue().asBoolean();
	bool inc_adult = mIncAdultCheck && mIncAdultCheck->getValue().asBoolean();
	if (!(inc_pg || inc_mature || inc_adult))
	{
		gNotifications.add("NoContentToSearch");
		return;
	}

	// This sets mSearchID and clears the list of results
	setupNewSearch();

	// send the message
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_DirClassifiedQuery);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	bool filter_auto_renew = false;
	U32 query_flags = pack_classified_flags_request(filter_auto_renew,
													inc_pg, inc_mature,
													inc_adult);
#if 0
	if (gAgent.isTeen()) query_flags |= DFQ_PG_SIMS_ONLY;
#endif

	U32 category = childGetValue("Category").asInteger();

	msg->nextBlockFast(_PREHASH_QueryData);
	msg->addUUIDFast(_PREHASH_QueryID, mSearchID);
	msg->addStringFast(_PREHASH_QueryText, mSearchEditor->getText());
	msg->addU32Fast(_PREHASH_QueryFlags, query_flags);
	msg->addU32Fast(_PREHASH_Category, category);
	msg->addS32Fast(_PREHASH_QueryStart, mSearchStart);

	gAgent.sendReliableMessage();
}

void LLPanelDirClassified::onSearchEditClassified(const std::string& text,
												  void* data)
{
	LLPanelDirClassified* self = (LLPanelDirClassified*)data;
	if (!self) return;

	S32 len = text.length();
	if (len == 0 || len >= 3)
	{
		// Ho text searches are cheap, as are longer searches
		self->setDefaultBtn("search_btn");
		self->childEnable("search_btn");
	}
	else
	{
		self->setDefaultBtn();
		self->childDisable("search_btn");
	}

	// Change the Browse to Search or vice versa
	if (len > 0)
	{
		self->childSetVisible("search_btn", true);
		self->childSetVisible("browse_btn", false);
	}
	else
	{
		self->setDefaultBtn("browse_btn");
		self->childSetVisible("search_btn", false);
		self->childSetVisible("browse_btn", true);
	}
}
