/** 
 * @file llpaneldirpeople.cpp
 * @brief People (online and off) in the Find directory.
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

#include "llpaneldirpeople.h"

#include "llnotifications.h"
#include "llqueryflags.h"
#include "llmessage.h"

LLPanelDirPeople::LLPanelDirPeople(const std::string& name,
								   HBFloaterSearch* floater)
:	LLPanelDirBrowser(name, floater)
{
	mMinSearchChars = 3;
}

bool LLPanelDirPeople::postBuild()
{
	LLPanelDirBrowser::postBuild();

	mSearchEditor = getChild<LLSearchEditor>("search_text");
	mSearchEditor->setSearchCallback(onSearchEdit, this);

	childSetAction("search_btn", onClickSearchCore, this);
	childDisable("search_btn");
	setDefaultBtn("search_btn");

	return true;
}

//virtual
void LLPanelDirPeople::performQuery()
{
	std::string name = mSearchEditor->getText();
	if (name.length() < mMinSearchChars)
	{
		return;
	}

	// Filter short words out of the query string and indidate if we did have
	// to filter it. The shortest username is 2 characters long.
	bool filtered = false;
	std::string query_string = filterShortWords(name, 2, filtered);

	// Possible we threw away all the short words in the query so check length
	if (query_string.length() < mMinSearchChars)
	{
		gNotifications.add("SeachFilteredOnShortWordsEmpty");
		return;
	}

	// If we filtered something out, display a popup
	if (filtered)
	{
		LLSD args;
		args["FINALQUERY"] = query_string;
		gNotifications.add("SeachFilteredOnShortWords", args);
	}

	setupNewSearch();

	U32 scope = DFQ_PEOPLE;

	// Send the message
	sendDirFindQuery(gMessageSystemp, mSearchID, query_string, scope,
					 mSearchStart);
}
