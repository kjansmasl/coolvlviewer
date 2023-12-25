/**
 * @file llgridmanager.cpp
 * @brief Grids management.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llgridmanager.h"

#include "lldir.h"
#include "llhost.h"
#include "llsd.h"
#include "llsdserialize.h"

#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"

// Assume SL production grid by default (strictest policy).
bool gIsInSecondLife = true;
bool gIsInSecondLifeProductionGrid = true;
bool gIsInSecondLifeBetaGrid = false;
bool gIsInProductionGrid = true;
bool gPacificDaylightTime = false;

EGridInfo GRID_INFO_OTHER;

///////////////////////////////////////////////////////////////////////////////
// SecondLife URLs
///////////////////////////////////////////////////////////////////////////////

const std::string CREATE_ACCOUNT_URL("https://join.secondlife.com/");

const std::string AUCTION_URL(
	"https://secondlife.com/auctions/auction-detail.php?id=");

const std::string EVENTS_URL("http://events.secondlife.com/viewer/embed/event/");

const std::string SUPPORT_URL("https://support.secondlife.com/");

const std::string FORGOTTEN_PASSWORD_URL(
    "https://secondlife.com/account/request.php");

const std::string BUY_CURRENCY_URL("https://secondlife.com/my/lindex/");

const std::string LSL_DOC_URL("http://wiki.secondlife.com/wiki/LSL_Portal");

const std::string RELEASE_NOTES_BASE_URL(
	"http://secondlife.com/app/releasenotes/");

const std::string AGNI_LOGIN_URI(
	"https://login.agni.lindenlab.com/cgi-bin/login.cgi");

const std::string ADITI_LOGIN_URI(
	"https://login.aditi.lindenlab.com/cgi-bin/login.cgi");

const std::string AGNI_HELPER_URI("https://secondlife.com/helpers/");

const std::string ADITI_HELPER_URI(
	"https://secondlife.aditi.lindenlab.com/helpers/");

const std::string SL_LOGIN_PAGE_URL("https://viewer-splash.secondlife.com/");

const std::string AGNI_VALIDATE_MESH_UPLOAD_PAGE_URL(
	"https://secondlife.com/my/account/ip/index.php");

const std::string ADITI_VALIDATE_MESH_UPLOAD_PAGE_URL(
	"https://secondlife.aditi.lindenlab.com/my/account/mesh.php");

const std::string SL_GRID_STATUS_URL("https://status.secondlifegrid.net/");

const std::string MAIN_GRID_SLURL_BASE = "https://maps.secondlife.com/secondlife/";
const std::string SYSTEM_GRID_APP_SLURL_BASE = "secondlife:///app";

const char* SYSTEM_GRID_SLURL_BASE = "secondlife://%s/secondlife/";
const char* DEFAULT_SLURL_BASE = "x-grid-info://%s/region/";
const char* DEFAULT_APP_SLURL_BASE = "x-grid-info://%s/app";

///////////////////////////////////////////////////////////////////////////////
// LLGridManager class
///////////////////////////////////////////////////////////////////////////////

LLGridManager::LLGridManager()
:	mGridChoice(DEFAULT_GRID_CHOICE),
	mNameEdited(false)
{
	loadGridsList();
	parseCommandLineURIs();
}

void LLGridManager::loadGridsList()
{
	if (LLStartUp::isLoggedIn())
	{
		// Never change the grids list once started, else bad things will
		// happen because the grid choice is done on an index in the list...
		return;
	}

	mGridList.clear();

	LLSD array = mGridList.emptyArray();
	LLSD entry = mGridList.emptyMap();
	entry.insert("label", "None");
	entry.insert("name", "");
	entry.insert("login_uri", "");
	entry.insert("helper_uri", "");
	entry.insert("login_page", "");
	entry.insert("can_edit", "never");
	array.append(entry);

	// Add SecondLife servers (main and beta grid):

	entry = mGridList.emptyMap();
	entry.insert("label", "SecondLife");
	entry.insert("name", "agni.lindenlab.com");
	entry.insert("login_uri", AGNI_LOGIN_URI);
	entry.insert("helper_uri", AGNI_HELPER_URI);
	entry.insert("support_url", SUPPORT_URL);
	entry.insert("register_url", CREATE_ACCOUNT_URL);
	entry.insert("password_url", FORGOTTEN_PASSWORD_URL);
	entry.insert("login_page", SL_LOGIN_PAGE_URL);
	entry.insert("can_edit", "never");
	array.append(entry);

	entry = mGridList.emptyMap();
	entry.insert("label", "SecondLife Beta");
	entry.insert("name", "aditi.lindenlab.com");
	entry.insert("login_uri", ADITI_LOGIN_URI);
	entry.insert("helper_uri", ADITI_HELPER_URI);
	entry.insert("support_url", SUPPORT_URL);
	entry.insert("register_url", CREATE_ACCOUNT_URL);
	entry.insert("password_url", FORGOTTEN_PASSWORD_URL);
	entry.insert("login_page", SL_LOGIN_PAGE_URL);
	entry.insert("can_edit", "never");
	array.append(entry);

	mGridList.insert("grids", array);

	mVerbose = true;
	// See if we have a grids_custom.xml file to append
	loadGridsLLSD(mGridList,
				  gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
												 "grids_custom.xml"),
				  true);
	// Load the additional grids if available
	loadGridsLLSD(mGridList,
				  gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
												 "grids.xml"));
	mVerbose = false;

	entry = mGridList.emptyMap();
	entry.insert("label", "Other");
	entry.insert("name", "");
	entry.insert("login_uri", "");
	entry.insert("helper_uri", "");
	entry.insert("can_edit", "never");
	mGridList["grids"].append(entry);

	GRID_INFO_OTHER = (EGridInfo)mGridList["grids"].size() - 1;
}

const EGridInfo LLGridManager::gridIndexInList(LLSD& grids,
											   std::string name,
											   std::string label)
{
	bool has_name = !name.empty();
	bool has_label = !label.empty();

	if (!has_name && !has_label) return -1;

	LLStringUtil::toLower(name);
	LLStringUtil::toLower(label);

	for (LLSD::map_iterator it = grids.beginMap(); it != grids.endMap(); ++it)
	{
		LLSD::String key_name = it->first;
		LLSD grid_array = it->second;
		if (key_name == "grids" && grid_array.isArray())
		{
			std::string temp;
			for (size_t i = 0; i < grid_array.size(); ++i)
			{
				if (has_name)
				{
					temp = grid_array[i]["name"].asString();
					LLStringUtil::toLower(temp);
					if (temp == name)
					{
						return i;
					}
				}
				if (has_label)
				{
					temp = grid_array[i]["label"].asString();
					LLStringUtil::toLower(temp);
					if (temp == label)
					{
						return i;
					}
				}
			}
		}
	}
	return -1;
}

void LLGridManager::loadGridsLLSD(LLSD& grids,
								  const std::string& xml_filename,
								  bool can_edit)
{
	LLSD other_grids;
	llifstream llsd_xml(xml_filename.c_str(), std::ios::in | std::ios::binary);
	if (llsd_xml.is_open())
	{
		if (mVerbose)
		{
			llinfos << "Reading grid info: " << xml_filename << llendl;
		}
		LLSDSerialize::fromXML(other_grids, llsd_xml);
		for (LLSD::map_iterator it = other_grids.beginMap();
			 it != other_grids.endMap(); ++it)
		{
			LLSD::String key_name = it->first;
			LLSD grid_array = it->second;
			if (mVerbose)
			{
				llinfos << "reading: " << key_name << llendl;
			}
			if (key_name == "grids" && grid_array.isArray())
			{
				for (size_t i = 0; i < grid_array.size(); ++i)
				{
					LLSD gmap = grid_array[i];
					if (gmap.has("name") && gmap.has("label") &&
						gmap.has("login_uri") && gmap.has("helper_uri"))
					{
						if (gridIndexInList(grids, gmap["name"].asString(),
											gmap["label"].asString()) != -1)
						{
							if (mVerbose)
							{
								llinfos << "Skipping overridden grid parameters for: "
										<< gmap.get("name") << llendl;
							}
						}
						else
						{
							gmap.insert("can_edit", can_edit ? "true" : "false");
							grids["grids"].append(gmap);
							if (mVerbose)
							{
								llinfos << "Added grid: " << gmap.get("name") << llendl;
							}
						}
					}
					else
					{
						if (mVerbose)
						{
							if (gmap.has("name"))
							{
								llwarns << "Incomplete grid definition for: "
										<< gmap.get("name") << llendl;
							}
							else
							{
								llwarns << "Incomplete grid definition: no name specified"
										<< llendl;
							}
						}
					}
				}
			}
			else if (mVerbose)
			{
				llwarns << "\"" << key_name << "\" is not an array" << llendl;
			}
		}
		llsd_xml.close();
	}
}

void LLGridManager::setMenuColor() const
{
	if (mGridList["grids"][mGridChoice].has("menu_color"))
	{
		std::string colorName = mGridList["grids"][mGridChoice].get("menu_color").asString();
		LLColor4 color4;
		LLColor4::parseColor(colorName.c_str(), &color4);
		if (color4 != LLColor4::black)
		{
			gMenuBarViewp->setBackgroundColor(color4);
		}
	}
}

void LLGridManager::setGridChoice(EGridInfo grid)
{
	if (grid < 0 || grid > GRID_INFO_OTHER)
	{
		llwarns << "Invalid grid index specified." << llendl;
		grid = DEFAULT_GRID_CHOICE;
	}

	mGridChoice = grid;
	std::string name = mGridList["grids"][grid].get("label").asString();
	LLStringUtil::toLower(name);
	if (name == "other")
	{
		// *FIX: Mani - could this possibly be valid?
		mGridName = "other";
		mGridHost = "other";
		setHelperURI("");
		setLoginPageURI("");
	}
	else
	{
		mGridName = mGridList["grids"][grid].get("label").asString();
		mGridHost = mGridList["grids"][grid].get("name").asString();
		setGridURI(mGridList["grids"][grid].get("login_uri").asString());
		setHelperURI(mGridList["grids"][grid].get("helper_uri").asString());
		setLoginPageURI(mGridList["grids"][grid].get("login_page").asString());
		mWebsiteURL = mGridList["grids"][grid].get("website_url").asString();
		mSupportURL = mGridList["grids"][grid].get("support_url").asString();
		mAccountURL = mGridList["grids"][grid].get("register_url").asString();
		mPasswordURL = mGridList["grids"][grid].get("password_url").asString();
	}

	gSavedSettings.setS32("ServerChoice", mGridChoice);
	gSavedSettings.setString("CustomServer", mGridName);
}

void LLGridManager::setGridChoice(const std::string& grid_name)
{
	// Set the grid choice based on a string.
	// The string can be:
	// - a grid label from the gGridInfo table
	// - an ip address
	if (!grid_name.empty())
	{
		// Find the grid choice from the user setting.
		std::string pattern(grid_name);
		LLStringUtil::toLower(pattern);
		for (EGridInfo grid_index = GRID_INFO_NONE;
			 grid_index < GRID_INFO_OTHER; ++grid_index)
		{
			std::string label = mGridList["grids"][grid_index].get("label").asString();
			std::string name = mGridList["grids"][grid_index].get("name").asString();
			LLStringUtil::toLower(label);
			LLStringUtil::toLower(name);
			if (label.find(pattern) == 0 || name.find(pattern) == 0)
			{
				// Found a matching label in the list...
				setGridChoice(grid_index);
				return;
			}
		}

		mGridChoice = GRID_INFO_OTHER;
		mGridName = grid_name;
		gSavedSettings.setS32("ServerChoice", mGridChoice);
		gSavedSettings.setString("CustomServer", mGridName);
	}
}

std::string LLGridManager::getGridLabel()
{
	if (mGridChoice == GRID_INFO_NONE)
	{
		return "None";
	}
	if (mGridChoice < GRID_INFO_OTHER)
	{
		return mGridList["grids"][mGridChoice].get("label").asString();
	}
	if (!mGridName.empty())
	{
		return mGridName;
	}
	return LLURI(getGridURI()).hostName();
}

std::string LLGridManager::getKnownGridLabel(EGridInfo grid) const
{
	if (grid > GRID_INFO_NONE && grid < GRID_INFO_OTHER)
	{
		return mGridList["grids"][grid].get("label").asString();
	}
	return mGridList["grids"][GRID_INFO_NONE].get("label").asString();
}

const std::vector<std::string>& LLGridManager::getCommandLineURIs()
{
	return mCommandLineURIs;
}

void LLGridManager::parseCommandLineURIs()
{
	// Return the login uri set on the command line.
	LLControlVariable* c = gSavedSettings.getControl("CmdLineLoginURI");
	if (c)
	{
		LLSD v = c->getValue();
		if (!v.isUndefined())
		{
			bool found_real_uri = false;
			if (v.isArray())
			{
				for (LLSD::array_const_iterator itr = v.beginArray();
					 itr != v.endArray(); ++itr)
				{
					std::string uri = itr->asString();
					if (!uri.empty())
					{
						found_real_uri = true;
						mCommandLineURIs.emplace_back(uri);
					}
				}
			}
			else if (v.isString())
			{
				std::string uri = v.asString();
				if (!uri.empty())
				{
					found_real_uri = true;
					mCommandLineURIs.emplace_back(uri);
				}
			}

			if (found_real_uri)
			{
				mGridChoice = GRID_INFO_OTHER;
				mGridName = getGridLabel();
			}
		}
	}

	setLoginPageURI(gSavedSettings.getString("LoginPage"));
	setHelperURI(gSavedSettings.getString("CmdLineHelperURI"));
}

const std::string LLGridManager::getStaticGridHelperURI(const EGridInfo grid) const
{
	std::string helper_uri;
	// grab URI from selected grid
	if (grid > GRID_INFO_NONE && grid < GRID_INFO_OTHER)
	{
		helper_uri = mGridList["grids"][grid].get("helper_uri").asString();
	}

	if (helper_uri.empty())
	{
		// What do we do with unnamed/miscellaneous grids ? For now, operations
		// that rely on the helper URI (currency/land purchasing) will fail.
		llwarns << "Missing Helper URI for this grid !  Currency/land purchasing operations will fail..."
				<< llendl;
	}
	return helper_uri;
}

const std::string LLGridManager::getHelperURI() const
{
	return mHelperURI;
}

void LLGridManager::setHelperURI(const std::string& uri)
{
	mHelperURI = uri;
}

const std::string LLGridManager::getLoginPageURI() const
{
	return mLoginPageURI;
}

void LLGridManager::setLoginPageURI(const std::string& uri)
{
	mLoginPageURI = uri;
}

const std::string LLGridManager::getStaticGridURI(const EGridInfo grid) const
{
	// If its a known grid choice, get the uri from the table,
	// else try the grid name.
	if (grid > GRID_INFO_NONE && grid < GRID_INFO_OTHER)
	{
		return mGridList["grids"][grid].get("login_uri").asString();
	}
	else
	{
		return std::string("");
	}
}

#if 0
const std::string LLGridManager::getGridIP() const
{
	std::string domain = getDomain(getGridURI());
	// Get the IP
	LLHost host;
	host.setHostByName(domain);
	return host.getIPString();
}
#endif

void LLGridManager::setIsInSecondlife()
{
	// NOTE: with the migration of SL servers to AWS, it becomes harder to
	// distinguish SL from OpenSim grids based on the sole IP (not working any
	// more) or login URI (since some rogue OpenSim grid could try and use
	// "lindenlab" or "secondlife" in their grid URI to fake SL).
	gIsInSecondLife = mGridURI.find(".lindenlab.com/") != std::string::npos ||
					  mGridURI.find(".secondlife.com/") != std::string::npos ||
					  mGridURI.find(".lindenlab.io/") != std::string::npos;
	// AFAIK, there is no universal way to detect an OpenSim beta grid...
	gIsInProductionGrid = !gIsInSecondLife ||
						  mGridURI.find("aditi.") == std::string::npos;
	gIsInSecondLifeProductionGrid = gIsInSecondLife && gIsInProductionGrid;
	gIsInSecondLifeBetaGrid = gIsInSecondLife && !gIsInProductionGrid;
	if (gIsInSecondLifeBetaGrid)
	{
		llinfos << "Second Life beta grid assumed." << llendl;
	}
	else if (gIsInSecondLife)
	{
		llinfos << "Second Life grid assumed." << llendl;
	}
	else
	{
		llinfos << "OpenSim grid assumed." << llendl;
	}
}

std::string LLGridManager::getGridId(const std::string& name)
{
	std::string domain;

	if (name.empty())
	{
		if (gIsInSecondLifeProductionGrid)
		{
			domain = "secondlife";
			return domain;
		}
		else if (gIsInSecondLife)
		{
			domain = "aditi";
			return domain;
		}
		else
		{
			domain = mGridHost;
		}
	}
	else
	{
		std::string grid = name;
		LLStringUtil::trim(grid);
		LLStringUtil::toLower(grid);
		if (grid == "secondlife" || grid.find("agni") == 0)
		{
			return "secondlife";
		}
		if (grid == "secondlife_beta" || grid.find("aditi") == 0)
		{
			return "aditi";
		}

		for (LLSD::map_iterator it = mGridList.beginMap();
			 it != mGridList.endMap(); ++it)
		{
			LLSD::String key_name = it->first;
			LLSD grid_array = it->second;
			if (key_name == "grids" && grid_array.isArray())
			{
				std::string temp;
				for (size_t i = 0; i < grid_array.size(); ++i)
				{
					temp = grid_array[i]["name"].asString();
					LLStringUtil::toLower(temp);
					if (temp == grid)
					{
						domain = temp;
					}
				}
			}
		}
	}

	if (domain.empty())
	{
		return LLStringUtil::null;
	}

	// Remove trailing ".suffix" and any leading "prefix." from the domain name

	// Get rid of any leading "grid." or "world."
	size_t i = domain.find("grid.");
	if (i == 0)
	{
		domain = domain.substr(5);
	}
	i = domain.find("world.");
	if (i == 0)
	{
		domain = domain.substr(6);
	}
	// Get rid of trailing ".com", ".net", ".org", etc...
	i = domain.rfind('.');
	if (i > 0)
	{
		domain = domain.substr(0, i);
	}
	// Get rid of any trailing sub-domain
	i = domain.rfind('.');
	if (i > 0)
	{
		domain = domain.substr(0, i);
	}

	return domain;
}

std::string LLGridManager::getGridHost(std::string grid)
{
	if (grid.empty())
	{
		if (gIsInSecondLifeProductionGrid)
		{
			return "secondlife";
		}
		else if (gIsInSecondLifeBetaGrid)
		{
			return "aditi";
		}
		else
		{
			return mGridHost;
		}
	}
	else
	{
		LLStringUtil::trim(grid);
		LLStringUtil::toLower(grid);
		if (grid == "secondlife" || grid.find("agni") == 0)
		{
			return "secondlife";
		}
		if (grid == "secondlife_beta" || grid.find("aditi") == 0)
		{
			return "aditi";
		}

		// When it is a domain name, get the corresponding grid Id
		if (grid.find('.') != std::string::npos)
		{
			grid = getGridId(grid);
		}

		std::string best_match;
		for (LLSD::map_iterator it = mGridList.beginMap();
			 it != mGridList.endMap(); ++it)
		{
			LLSD::String key_name = it->first;
			LLSD grid_array = it->second;
			if (key_name == "grids" && grid_array.isArray())
			{
				std::string temp;
				for (size_t i = 0; i < grid_array.size(); ++i)
				{
					temp = grid_array[i]["name"].asString();
					LLStringUtil::toLower(temp);
					if (temp == grid)
					{
						return grid;
					}
					else if (temp.find(grid) != std::string::npos)
					{
						// Keep the shorter matching grid name
						size_t len = best_match.size();
						if (len == 0 || temp.size() < len)
						{
							best_match = temp;
						}
					}
				}
			}
		}
		if (best_match.find("agni") == 0)
		{
			return "secondlife";
		}
		if (best_match.find("aditi") == 0)
		{
			return "aditi";
		}
		return best_match;
	}
}

// Build a slurl for the given region within the selected grid
std::string LLGridManager::getSLURLBase(const std::string& grid)
{
	std::string grid_base;
	std::string name = grid;
	LLStringUtil::toLower(name);
	if (grid.empty() ||
		(gIsInSecondLifeProductionGrid &&
		 (name == "secondlife" || name.find("agni") == 0)) ||
		(gIsInSecondLifeBetaGrid &&
		 (name == "secondlife_beta" || name.find("aditi") == 0)))
	{
		if (gIsInSecondLifeProductionGrid)
		{
			grid_base = MAIN_GRID_SLURL_BASE;
		}
		else if (gIsInSecondLifeBetaGrid)
		{
			grid_base = llformat(SYSTEM_GRID_SLURL_BASE, "aditi");
		}
		else
		{
			grid_base = llformat(DEFAULT_SLURL_BASE, mGridHost.c_str());
		}
	}
	else
	{
		std::string host = getGridHost(grid);
		if (!host.empty())
		{
			grid_base = llformat(DEFAULT_SLURL_BASE, host.c_str());
		}
	}

	return grid_base;
}

// Build an app slurl for the given region within the selected grid
std::string LLGridManager::getAppSLURLBase(const std::string& grid)
{
	std::string grid_base;
	std::string name = grid;
	LLStringUtil::toLower(name);
	if (grid.empty() ||
		(gIsInSecondLifeProductionGrid &&
		 (name == "secondlife" || name.find("agni") == 0)) ||
		(gIsInSecondLifeBetaGrid &&
		 (name == "secondlife_beta" || name.find("aditi") == 0)))
	{
		if (gIsInSecondLife)
		{
			grid_base = SYSTEM_GRID_APP_SLURL_BASE;
		}
		else
		{
			grid_base = llformat(DEFAULT_APP_SLURL_BASE, mGridHost.c_str());
		}
	}
	else
	{
		std::string host = getGridHost(grid);
		if (!host.empty())
		{
			grid_base = llformat(DEFAULT_APP_SLURL_BASE, host.c_str());
		}
	}

	return grid_base;
}

//static
std::string LLGridManager::getDomain(const std::string& url)
{
	if (url.empty())
	{
		return url;
	}

	std::string domain = url;
	LLStringUtil::toLower(domain);

	size_t pos = domain.find("//");

	if (pos != std::string::npos)
	{
		size_t count = domain.size() - pos + 2;
		domain = domain.substr(pos + 2, count);
	}

	// Check that there is at least one slash in the URL and add a trailing
	// one if not
	if (domain.find('/') == std::string::npos)
	{
		domain += '/';
	}

	// Paranoia: If there is a user:password@ part, remove it
	pos = domain.find('@');
	if (pos != std::string::npos &&
		// if '@' is not before the first '/', then it is not a user:password
		pos < domain.find('/'))
	{
		size_t count = domain.size() - pos + 1;
		domain = domain.substr(pos + 1, count);
	}

	pos = domain.find(':');
	if (pos != std::string::npos && pos < domain.find('/'))
	{
		// Keep anything before the port number and strip the rest off
		domain = domain.substr(0, pos);
	}
	else
	{
		pos = domain.find('/');	// We earlier made sure that there is one
		domain = domain.substr(0, pos);
	}

	return domain;
}

//static
std::string LLGridManager::getTimeStamp(time_t t_utc, const std::string& fmt,
										bool append_tz)
{
	struct tm* timep;
	if (gIsInSecondLife)
	{
		// Convert to Pacific, based on server opinion of whether it is
		// daylight savings time there.
		timep = utc_to_pacific_time(t_utc, gPacificDaylightTime);
	}
	else	// OpenSim grids do not always use US time zones... HB
	{
		timep = utc_time_to_tm(t_utc);
	}

	std::string timestamp;
	timeStructToFormattedString(timep, fmt, timestamp);
	if (append_tz)
	{
		timestamp += gIsInSecondLife ? (gPacificDaylightTime ? " PDT" : " PST")
									 : " UTC";
	}
	return timestamp;
}
