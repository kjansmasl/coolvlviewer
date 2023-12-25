/**
 * @file llurlhistory.cpp
 * @brief Manages a list of recent URLs
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "linden_common.h"

#include "llurlhistory.h"

#include "lldir.h"
#include "llsdserialize.h"
#include "lluri.h"

LLSD LLURLHistory::sHistorySD;

constexpr S32 MAX_URL_COUNT = 10;

/////////////////////////////////////////////////////////////////////////////

//static
bool LLURLHistory::loadFile(const std::string& filename)
{
	std::string temp_str = gDirUtilp->getLindenUserDir() + LL_DIR_DELIM_STR +
						   filename;
	llifstream file(temp_str.c_str());
	if (file.is_open())
	{
		llinfos << "Loading URL history: " << temp_str << llendl;
		LLSD data;
		LLSDSerialize::fromXML(data, file);
		if (data.isUndefined())
		{
			llinfos << temp_str << " ill-formed or empty; loading aborted."
					<< llendl;
			sHistorySD = LLSD();
		}
		else
		{
			sHistorySD = data;
			return true;
		}
	}

	return false;
}

//static
bool LLURLHistory::saveFile(const std::string& filename)
{
	std::string temp_str = gDirUtilp->getLindenUserDir();
	if (temp_str.empty())
	{
		llwarns << "Can't save URL history. No user directory set." << llendl;
		return false;
	}

	temp_str += LL_DIR_DELIM_STR + filename;
	llofstream out(temp_str.c_str());
	if (!out.is_open())
	{
		llwarns << "Unable to open '" << temp_str << "' for writing."
				<< llendl;
		return false;
	}

	LLSDSerialize::toXML(sHistorySD, out);
	out.close();
	return true;
}

// This function returns a portion of the history llsd that contains the
// collected url history
//static
LLSD LLURLHistory::getURLHistory(const std::string& collection)
{
	if (sHistorySD.has(collection))
	{
		return sHistorySD[collection];
	}
	return LLSD();
}

//static
void LLURLHistory::addURL(const std::string& collection,
						  const std::string& url)
{
	if (!url.empty())
	{
		LLURI u(url);
		std::string simplified_url = u.scheme() + "://" + u.authority() + u.path();
		sHistorySD[collection].insert(0, simplified_url);
		LLURLHistory::limitSize(collection);
	}
}

//static
void LLURLHistory::removeURL(const std::string& collection,
							 const std::string& url)
{
	if (!url.empty())
	{
		LLURI u(url);
		std::string simplified_url = u.scheme() + "://" + u.authority() +
									 u.path();
		for (size_t index = 0; index < sHistorySD[collection].size(); ++index)
		{
			if (sHistorySD[collection].get(index).asString() == simplified_url)
			{
				sHistorySD[collection].erase(index);
			}
		}
	}
}

//static
void LLURLHistory::clear(const std::string& collection)
{
	sHistorySD[collection] = LLSD();
}

void LLURLHistory::limitSize(const std::string& collection)
{
	while ((S32)sHistorySD[collection].size() > MAX_URL_COUNT)
	{
		sHistorySD[collection].erase(MAX_URL_COUNT);
	}
}
