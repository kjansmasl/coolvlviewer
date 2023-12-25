/**
 * @file llslurl.cpp
 * @brief Handles "SLURL fragments" like Ahern/123/45 for startup processing,
 * login screen, prefs, etc.
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include <regex>

#include "curl/curl.h"

#include "llslurl.h"

#include "llcachename.h"
#include "llexperiencecache.h"

#include "llgridmanager.h"
#include "llfloaterchat.h"
#include "llnotify.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llworldmap.h"			// Variable region size support

const char* LLSLURL::SLURL_SECONDLIFE_SCHEME			= "secondlife";
const char* LLSLURL::SLURL_HOP_SCHEME					= "hop";
const char* LLSLURL::SLURL_X_GRID_INFO_SCHEME			= "x-grid-info";
const char* LLSLURL::SLURL_X_GRID_LOCATION_INFO_SCHEME	= "x-grid-location-info";

const char* LLSLURL::SLURL_HTTP_SCHEME					= "http";
const char* LLSLURL::SLURL_HTTPS_SCHEME					= "https";
const char* LLSLURL::SLURL_SECONDLIFE_PATH				= "secondlife";

const char* LLSLURL::SLURL_COM							= "slurl.com";
// For DnD - even though www.slurl.com redirects to slurl.com in a browser, you
// can copy and drag text with www.slurl.com or a link explicitly pointing at
// www.slurl.com so testing for this version is required also:
const char* LLSLURL::WWW_SLURL_COM						= "www.slurl.com";

const char* LLSLURL::MAPS_SECONDLIFE_COM				= "maps.secondlife.com";

const char* LLSLURL::SLURL_APP_PATH						= "app";
const char* LLSLURL::SLURL_REGION_PATH					= "region";

const char* LLSLURL::SIM_LOCATION_HOME					= "home";
const char* LLSLURL::SIM_LOCATION_LAST					= "last";

const std::string MAIN_GRID_SLURL_BASE = "http://maps.secondlife.com/secondlife/";

uuid_list_t LLSLURL::sAvatarUUIDs;
uuid_list_t LLSLURL::sGroupUUIDs;
uuid_list_t LLSLURL::sExperienceUUIDs;
uuid_list_t LLSLURL::sObjectsUUIDs;
LLSLURL::slurls_map_t LLSLURL::sPendingSLURLs;

// Regular expression used to match app/agent SLURLs
#define APP_AGENT_REGEX "(((x-grid-info|x-grid-location-info)://[-\\w\\.]+/app/agent/)|((secondlife|hop):///app/agent/))[\\da-f-]+/"
static std::regex sAgentPattern =
	std::regex(APP_AGENT_REGEX, std::regex::ECMAScript | std::regex::icase);

// Regular expression used to match app/group SLURLs
#define APP_GROUP_REGEX "(((x-grid-info|x-grid-location-info)://[-\\w\\.]+/app/group/)|((secondlife|hop):///app/group/))[\\da-f-]+/about"
static std::regex sGroupPattern =
	std::regex(APP_GROUP_REGEX, std::regex::ECMAScript | std::regex::icase);

// Regular expression used to match app/experience SLURLs
#define APP_EXP_REGEX "(((x-grid-info|x-grid-location-info)://[-\\w\\.]+/app/experience/)|((secondlife|hop):///app/experience/))[\\da-f-]+/profile"
static std::regex sExperiencePattern =
	std::regex(APP_EXP_REGEX, std::regex::ECMAScript | std::regex::icase);

// Regular expression used to match app/objectim SLURLs
#define APP_OBJ_REGEX "(((x-grid-info|x-grid-location-info)://[-\\w\\.]+/app/objectim/)|((secondlife|hop):///app/objectim/))[\\da-f-]+[/\?]"
static std::regex sObjectIMPattern =
	std::regex(APP_OBJ_REGEX, std::regex::ECMAScript | std::regex::icase);

// Helper function
bool match_regex(const char* text, std::regex regex, size_t& start,
				 size_t& end)
{
	std::cmatch result;
	try
	{
		if (!std::regex_search(text, result, regex))
		{
			return false;
		}
	}
	catch (std::regex_error& e)
	{
		llwarns << "Regex error: " << e.what() << llendl;
		return false;
	}

	// Return the first/last character offset for the matched substring
	start = result[0].first - text;
	end = result[0].second - text - 1;

	// We allow certain punctuation to terminate a Url but not match it,
	// e.g., "http://foo.com/." should just match "http://foo.com/"
	if (text[end] == '.' || text[end] == ',')
	{
		--end;
	}
	// Ignore a terminating ')' when Url contains no matching '('
	else if (text[end] == ')' &&
			 std::string(text + start,
						 end - start).find('(') == std::string::npos)
	{
		--end;
	}

	return end > start;
}

//static
uuid_list_t LLSLURL::findSLURLs(const std::string& txt)
{
	uuid_list_t result;
	if (txt.size() < 66 || txt.find("://") == std::string::npos)
	{
		// If no chance of an SLURL in the text, return right now
		return result;
	}

	std::string temp;
	size_t start = 0;
	size_t end = 0;
	LLUUID slurl_id;

	// Search for avatar name SLURLs
	std::string text = txt;
	while (match_regex(text.c_str(), sAgentPattern, start, end))
	{
		S32 uuid_start = end - 36;
		if (uuid_start >= 0)
		{
			slurl_id.set(text.substr(uuid_start, 36));
			if (slurl_id.notNull())
			{
				bool translate = true;
				temp = text.substr(end);
				if (temp.find("/completename") == 0)
				{
					end += 12; //strlen("completename");
				}
				else if (temp.find("/displayname") == 0)
				{
					end += 11; // strlen("displayname");
				}
				else if (temp.find("/username") == 0)
				{
					end += 8; // strlen("username");
				}
				else if (temp.find("/inspect") == 0)
				{
					end += 7; // strlen("inspect");
				}
				else if (temp.find("/about") == 0)
				{
					end += 5; // strlen("about");
				}
				else
				{
					// Non-translatable SLURL
					translate = false;
				}
				if (translate)
				{
					temp = text.substr(start, end - start + 1);
					result.emplace(slurl_id);
					sAvatarUUIDs.emplace(slurl_id);
					sPendingSLURLs.emplace(temp, slurl_id);
				}
			}
		}
		if (++end >= text.size())
		{
			break;
		}
		text = text.substr(end);
	}

	// Search for group name SLURLs
	text = txt;
	while (match_regex(text.c_str(), sGroupPattern, start, end))
	{
		S32 uuid_start = end - 41;
		if (uuid_start >= 0)
		{
			slurl_id.set(text.substr(uuid_start, 36));
			if (slurl_id.notNull())
			{
				temp = text.substr(start, end - start + 1);
				result.emplace(slurl_id);
				sGroupUUIDs.emplace(slurl_id);
				sPendingSLURLs.emplace(temp, slurl_id);
			}
		}
		if (++end >= text.size())
		{
			break;
		}
		text = text.substr(end);
	}

	// Search for experience name SLURLs
	text = txt;
	while (match_regex(text.c_str(), sExperiencePattern, start, end))
	{
		S32 uuid_start = end - 43;
		if (uuid_start >= 0)
		{
			slurl_id.set(text.substr(uuid_start, 36));
			if (slurl_id.notNull())
			{
				temp = text.substr(start, end - start + 1);
				result.emplace(slurl_id);
				sExperienceUUIDs.emplace(slurl_id);
				sPendingSLURLs.emplace(temp, slurl_id);
			}
		}
		if (++end >= text.size())
		{
			break;
		}
		text = text.substr(end);
	}

	// Search for objects name SLURLs
	text = txt;
	static const std::string valid_in_url("/?&=$%-_.+!*'(),");
	while (match_regex(text.c_str(), sObjectIMPattern, start, end))
	{
		S32 uuid_start = end - 36;
		if (uuid_start >= 0)
		{
			slurl_id.set(text.substr(uuid_start, 36));
			if (slurl_id.notNull())
			{
				for (size_t i = end, l = text.size(); i < l; ++i)
				{
					char c = text[i];
					if (c == ' ')	// A non-escaped space is an URL end
					{
						break;
					}
					if (!isalnum(c) &&
						valid_in_url.find(c) == std::string::npos)
					{
						break;
					}
					++end;
				}
				temp = text.substr(start, end - start);
				result.emplace(slurl_id);
				sObjectsUUIDs.emplace(slurl_id);
				sPendingSLURLs.emplace(temp, slurl_id);
			}
		}
		if (++end >= text.size())
		{
			break;
		}
		text = text.substr(end);
	}

	return result;
}

//static
void LLSLURL::avatarNameCallback(const LLUUID& id,
								 const LLAvatarName& avatar_name)
{
//MK
	bool censor_names = gRLenabled &&
						(gRLInterface.mContainsShownames ||
						 gRLInterface.mContainsShownametags);
//mk
	std::string substitute;
	for (slurls_map_t::iterator it = sPendingSLURLs.begin(),
								end = sPendingSLURLs.end();
		 it != end; )
	{
		if (it->second == id)
		{
			const std::string& slurl = it->first;
			if (slurl.find("/username") != std::string::npos)
			{
				// Note: we purposely display the legacy name instead of the
				// user name (the Cool VL Viewer doesn't use the user name
				// anywhere)
				substitute = avatar_name.getLegacyName();
			}
			else if (slurl.find("/displayname") != std::string::npos)
			{
				substitute = avatar_name.mDisplayName;
			}
			else
			{
				substitute = avatar_name.getNames();
			}
//MK
			if (censor_names)
			{
				substitute = gRLInterface.getCensoredMessage(substitute);
			}
//mk
			LLFloaterChat::substituteSLURL(id, slurl, substitute);
			LLNotifyBox::substituteSLURL(id, slurl, substitute);
			sPendingSLURLs.erase(it++);
		}
		else
		{
			++it;
		}
	}
	LLFloaterChat::substitutionDone(id);
	LLNotifyBox::substitutionDone(id);
}

//static
void LLSLURL::cacheNameCallback(const LLUUID& id,
								const std::string& name, bool is_group)
{
	std::string substitute = name;
//MK
	if (!is_group && gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		substitute = gRLInterface.getCensoredMessage(name);
	}
//mk
	for (slurls_map_t::iterator it = sPendingSLURLs.begin(),
								end = sPendingSLURLs.end();
		 it != end; )
	{
		if (it->second == id)
		{
			const std::string& slurl = it->first;
			LLFloaterChat::substituteSLURL(id, slurl, substitute);
			LLNotifyBox::substituteSLURL(id, slurl, substitute);
			sPendingSLURLs.erase(it++);
		}
		else
		{
			++it;
		}
	}
	LLFloaterChat::substitutionDone(id);
	LLNotifyBox::substitutionDone(id);
}

//static
void LLSLURL::experienceNameCallback(const LLSD& experience_details)
{
	LLUUID id = experience_details[LLExperienceCache::EXPERIENCE_ID].asUUID();
	std::string name = experience_details[LLExperienceCache::NAME].asString();

	for (slurls_map_t::iterator it = sPendingSLURLs.begin(),
								end = sPendingSLURLs.end();
		 it != end; )
	{
		if (it->second == id)
		{
			const std::string& slurl = it->first;
			LLFloaterChat::substituteSLURL(id, slurl, name);
			LLNotifyBox::substituteSLURL(id, slurl, name);
			sPendingSLURLs.erase(it++);
		}
		else
		{
			++it;
		}
	}
	LLFloaterChat::substitutionDone(id);
	LLNotifyBox::substitutionDone(id);
}

//static
void LLSLURL::resolveSLURLs()
{
	if (!gCacheNamep) return;	// Paranoia

	bool use_display_names = LLAvatarNameCache::useDisplayNames();
	for (uuid_list_t::iterator it = sAvatarUUIDs.begin(),
							   end = sAvatarUUIDs.end();
		 it != end; ++it)
	{
		if (use_display_names)
		{
			LLAvatarNameCache::get(*it, avatarNameCallback);
		}
		else
		{
			gCacheNamep->get(*it, false, cacheNameCallback);
		}
	}
	sAvatarUUIDs.clear();

	for (uuid_list_t::iterator it = sGroupUUIDs.begin(),
							   end = sGroupUUIDs.end();
		 it != end; ++it)
	{
		gCacheNamep->get(*it, true, cacheNameCallback);
	}
	sGroupUUIDs.clear();

	LLExperienceCache* expcache = LLExperienceCache::getInstance();
	for (uuid_list_t::iterator it = sExperienceUUIDs.begin(),
							   end = sExperienceUUIDs.end();
		 it != end; ++it)
	{
		expcache->get(*it, experienceNameCallback);
	}
	sExperienceUUIDs.clear();

	// No need for an asynchronous query to servers for objects: we subtsitute
	// their SLURL with any name found behind the "name=" query field, when it
	// exists.
	std::string substitute;
	for (uuid_list_t::iterator it = sObjectsUUIDs.begin(),
							   end = sObjectsUUIDs.end();
		 it != end; ++it)
	{
		const LLUUID& id = *it;
		for (slurls_map_t::iterator it2 = sPendingSLURLs.begin(),
									end2 = sPendingSLURLs.end();
			 it2 != end2; )
		{
			if (it2->second != id)
			{
				++it2;
				continue;
			}

			const std::string& slurl = it2->first;
			size_t i = slurl.find("name=");
			if (i == std::string::npos)
			{
				++it2;
				continue;
			}

			substitute = slurl.substr(i + 5);	// strlen("name=")
			i = substitute.find('&');
			if (i != std::string::npos)
			{
				substitute.erase(i);
			}
			substitute = LLURI::unescape(substitute);

			LLFloaterChat::substituteSLURL(id, slurl, substitute);
			LLNotifyBox::substituteSLURL(id, slurl, substitute);
			sPendingSLURLs.erase(it2++);
		}
		LLFloaterChat::substitutionDone(id);
		LLNotifyBox::substitutionDone(id);
	}
	sObjectsUUIDs.clear();
}

// Resolves a simstring from a slurl
LLSLURL::LLSLURL(const std::string& slurl)
{
	// By default we go to agni.
	mType = INVALID;

	if (slurl == SIM_LOCATION_HOME)
	{
		mType = HOME_LOCATION;
	}
	else if (slurl.empty() || slurl == SIM_LOCATION_LAST)
	{
		mType = LAST_LOCATION;
	}
	else
	{
		LLGridManager* gm = LLGridManager::getInstance();
		LLURI slurl_uri;
		// Parse the slurl as a uri
		if (slurl.find(':') == std::string::npos)
		{
			// There may be no scheme ('secondlife:' etc.) passed in. In that
			// case we want to normalize the slurl by putting the appropriate
			// scheme in front of the slurl. So, we grab the appropriate slurl
			// base from the grid manager which may be
			// http://slurl.com/secondlife/ for maingrid, or
			// https://<hostname>/region/ for Standalone grid (the word region,
			// not the region name); these slurls are typically passed in from
			// the 'starting location' box on the login panel, where the user
			// can type in <regionname>/<x>/<y>/<z>
			std::string fixed_slurl = gm->getSLURLBase();

			// The slurl that was passed in might have a prepended / or not.
			// So, we strip off the prepended '/' so we don't end up with
			// http://slurl.com/secondlife/<region>/<x>/<y>/<z> or some such.
			if (slurl[0] == '/')
		    {
				fixed_slurl += slurl.substr(1);
		    }
			else
		    {
				fixed_slurl += slurl;
		    }
			// We then load the slurl into a LLURI form
			slurl_uri = LLURI(fixed_slurl);
		}
		else
		{
		    // As we did have a scheme, implying a URI style slurl, we
		    // simply parse it as a URI
		    slurl_uri = LLURI(slurl);
		}

		LLSD path_array = slurl_uri.pathArray();

		// Determine whether it is a maingrid URI or a standalone/open style
		// URI/ by looking at the scheme. If it's a 'secondlife:' slurl scheme
		// or 'sl:' scheme, we know it's maingrid
		// At the end of this if/else block, we will have determined the grid,
		// and the slurl type (APP or LOCATION)
		const std::string& scheme = slurl_uri.scheme();
		if (scheme == LLSLURL::SLURL_SECONDLIFE_SCHEME)
		{
			// Parse a maingrid style slurl. We know the grid is maingrid, so
			// grab it. A location slurl for maingrid (with the special
			// schemes) can be in the form
			// secondlife://<regionname>/<x>/<y>/<z>
			// or
			// secondlife://<Grid>/secondlife/<region>/<x>/<y>/<z>
			// where if grid is empty, it specifies Agni
			// An app style slurl for maingrid can be
			// secondlife://<Grid>/app/<app parameters>
			// where an empty grid implies Agni
			// We will start by checking the top of the 'path' which will be
			// either 'app', 'secondlife', or <x>.

			// Default to maingrid.
			mGrid = "secondlife";

			std::string path = path_array[0].asString();
			if (path == LLSLURL::SLURL_SECONDLIFE_PATH ||
				path == LLSLURL::SLURL_APP_PATH)
		    {
				// Set the type as appropriate.
				mType = path == LLSLURL::SLURL_APP_PATH ? APP : LOCATION;

				// It is in the form secondlife://<grid>/(app|secondlife), so
				// parse the grid name to derive the grid ID
				std::string hostname = slurl_uri.hostName();
				if (!hostname.empty())
				{
					mGrid = gm->getGridId(hostname);
				}
				else if (mType == LOCATION)
				{
					// If the slurl is in the form secondlife:///secondlife/<region>
					// form, then we are in fact on maingrid.
					mGrid = "secondlife";
				}
				else if (mType == APP)
				{
					// For app style slurls, where no grid name is specified,
					// assume the currently selected or logged in grid.
					mGrid = gm->getGridId();
				}

				if (mType != APP && mGrid.empty())
				{
					mType = INVALID;
					// We could not find the grid in the grid manager, so bail
					llwarns << "Unable to find grid for: " << slurl << llendl;
					return;
				}
				path_array.erase(0);
		    }
			else
		    {
				// It was not a /secondlife/<region> or /app/<params>, so it
				// must be secondlife://<region>.  Therefore the hostname will
				// be the region name, and it's a location type
				mType = LOCATION;
				// 'normalize' it so the region name is in fact the head of the
				// path_array
				path_array.insert(0, slurl_uri.hostName());
		    }
		}
		else if (scheme == LLSLURL::SLURL_HTTP_SCHEME ||
				 scheme == LLSLURL::SLURL_HTTPS_SCHEME ||
				 scheme == LLSLURL::SLURL_HOP_SCHEME ||
				 scheme == LLSLURL::SLURL_X_GRID_INFO_SCHEME ||
				 scheme == LLSLURL::SLURL_X_GRID_LOCATION_INFO_SCHEME)
		{
			// We are dealing with either a standalone style slurl or slurl.com
			// slurl
			std::string hostname = slurl_uri.hostName();
			if (hostname == LLSLURL::SLURL_COM ||
				hostname == LLSLURL::WWW_SLURL_COM ||
				hostname == LLSLURL::MAPS_SECONDLIFE_COM)
			{
				// slurl.com implies maingrid
				mGrid = "secondlife";
			}
		    else
			{
				// Do not try to match any old http://<host>/ URL as a SLurl.
				// SL SLURLs will have the grid hostname in the URL, so only
				// match http URLs if the hostname matches the grid hostname
				// (or it is a slurl.com or maps.secondlife.com URL).
				if ((scheme == LLSLURL::SLURL_HTTP_SCHEME ||
					 scheme == LLSLURL::SLURL_HTTPS_SCHEME) &&
					hostname != gm->getGridHost())
				{
					return;
				}

				// As it is a standalone grid/open, we will always have a
				// hostname, as Standalone/open  style urls are properly
				// formed, unlike the stinky maingrid style
				mGrid = hostname;
			}
		    if (path_array.size() == 0)
			{
				// We would need a path...
				return;
			}

			// We need to normalize the urls so the path portion starts with
			// the 'command' that we want to do. It can either be region or
			// app.
			std::string path = path_array[0].asString();
		    if (path == LLSLURL::SLURL_REGION_PATH ||
				path == LLSLURL::SLURL_SECONDLIFE_PATH)
			{
				// Strip off 'region' or 'secondlife'
				path_array.erase(0);
				// It is a location
				mType = LOCATION;
			}
			else if (path == LLSLURL::SLURL_APP_PATH)
			{
				mType = APP;
				path_array.erase(0);
				// Leave app appended.
			}
			else if (scheme == LLSLURL::SLURL_HOP_SCHEME)
			{
				mGrid = hostname;
				mType = LOCATION;
			}
			else
			{
				// Not a valid https/http/x-grid-*info slurl...
				return;
			}
		}
		else
		{
		    // Invalid scheme, so bail
		    return;
		}

		if (path_array.size() == 0)
		{
			// We must have some stuff after the specifier as to whether it is
			// a region or command
			return;
		}

		// Now that we know whether it is an app slurl or a location slurl,
		// parse the slurl into the proper data structures.
		if (mType == APP)
		{
			// Grab the app command type and strip it (could be a command to
			// jump somewhere, or whatever)
			mAppCmd = path_array[0].asString();
			path_array.erase(0);

			// Grab the parameters
			mAppPath = path_array;
			// And the query
			mAppQuery = slurl_uri.query();
			mAppQueryMap = slurl_uri.queryMap();
			return;
		}
		else if (mType == LOCATION)
		{
			// At this point, head of the path array should be
			// [ <region>, <x>, <y>, <z> ] where x, y and z are collectively
			// optional.
			mRegion = LLURI::unescape(path_array[0].asString());
			if (LLStringUtil::containsNonprintable(mRegion))
			{
				LLStringUtil::stripNonprintable(mRegion);
			}

			path_array.erase(0);

			// Parse the x, y, and optionally z
			if (path_array.size() >= 2)
			{
				// This construction handles LLSD without all components
				// (values default to 0.f)
				mPosition = LLVector3(path_array);
				// Variable region size support: using 8192 instead of
				// REGION_WIDTH_METERS and REGION_HEIGHT_METERS as limits.
				if (mPosition.mV[VX] < 0.f || mPosition.mV[VX] > 8192.f ||
					mPosition.mV[VY] < 0.f || mPosition.mV[VY] > 8192.f ||
					mPosition.mV[VZ] < 0.f || mPosition.mV[VZ] > 8192.f)
				{
					mType = INVALID;
					return;
				}
			}
			else
			{
				// If x, y and z were not fully passed in, go to the middle of
				// the region. Teleport will adjust the actual location to make
				// sure you are on the ground and such
				mPosition = LLVector3(REGION_WIDTH_METERS * 0.5f,
									  REGION_WIDTH_METERS * 0.5f, 0.f);
			}
		}
	}
}

// Creates a slurl for the middle of the region
LLSLURL::LLSLURL(const std::string& grid, const std::string& region)
:	mType(LOCATION),
	mGrid(grid),
	mRegion(region)
{
	mPosition = LLVector3(REGION_WIDTH_METERS * 0.5f,
						  REGION_WIDTH_METERS * 0.5f, 0.f);
}

// Creates a slurl given the position. The position will be modded with the
// region width handling global positions as well
LLSLURL::LLSLURL(const std::string& grid, const std::string& region,
				 const LLVector3& position)
:	mType(LOCATION),
	mGrid(grid),
	mRegion(region)
{
#if 1	// Variable region size support (part 1, see below for part 2)
	S32 x = ll_roundp(position.mV[VX]);
	S32 y = ll_roundp(position.mV[VY]);
#else
	S32 x = ll_roundp((F32)fmod(position.mV[VX], REGION_WIDTH_METERS));
	S32 y = ll_roundp((F32)fmod(position.mV[VY], REGION_WIDTH_METERS));
#endif
	S32 z = ll_roundp(position.mV[VZ]);
	mPosition = LLVector3(x, y, z);
}

// Creates a simstring
LLSLURL::LLSLURL(const std::string& region, const LLVector3& position)
{
	*this = LLSLURL(LLGridManager::getInstance()->getGridId(), region,
					position);
}

// Creates a slurl from a global position
LLSLURL::LLSLURL(const std::string& grid, const std::string& region,
				 const LLVector3d& global_position)
{
	LLVector3 pos(global_position);
	std::string grid_id = LLGridManager::getInstance()->getGridId(grid);

	// Variable region size support (part 2, see above for part 1)
	bool adjusted = false;
	if (grid.empty() || grid_id == LLGridManager::getInstance()->getGridId())
	{
		// If we build a SLURL for the current grid, then we can use the data
		// of this grid to find the region size.
		LLSimInfo* sim;
		sim = gWorldMap.simInfoFromPosGlobal(global_position);
		if (sim)
		{
			pos.mV[VX] = fmod(pos.mV[VX], sim->getSizeX());
			pos.mV[VY] = fmod(pos.mV[VY], sim->getSizeY());
			adjusted = true;
		}
		else if (!gIsInSecondLife)
		{
			llwarns << "Sim info unavailable for: " << region
					<< ". The SLURL is created with the default region width (may cause issues if the grid supports VAR REGIONs)"
					<< llendl;
		}
	}

	if (!adjusted)
	{
		// Use the default region size as a fallback
		pos.mV[VX] = fmod(pos.mV[VX], REGION_WIDTH_METERS);
		pos.mV[VY] = fmod(pos.mV[VY], REGION_WIDTH_METERS);
	}

	*this = LLSLURL(grid_id, region, pos);
}

// Creates a slurl from a global position
LLSLURL::LLSLURL(const std::string& region, const LLVector3d& global_position)
{
	*this = LLSLURL(LLGridManager::getInstance()->getGridHost(), region,
					global_position);
}

LLSLURL::LLSLURL(const std::string& command, const LLUUID& id,
				 const std::string& verb)
:	mType(APP),
	mAppCmd(command)
{
	mAppPath = LLSD::emptyArray();
	mAppPath.append(LLSD(id));
	mAppPath.append(LLSD(verb));
}

std::string LLSLURL::getSLURLString() const
{
	switch (mType)
	{
		case HOME_LOCATION:
			return SIM_LOCATION_HOME;

		case LAST_LOCATION:
			return SIM_LOCATION_LAST;

		case LOCATION:
		{
			// Lookup the grid
			S32 x = ll_roundp(mPosition.mV[VX]);
			S32 y = ll_roundp(mPosition.mV[VY]);
			S32 z = ll_roundp(mPosition.mV[VZ]);
			return LLGridManager::getInstance()->getSLURLBase(mGrid) +
				   LLURI::escape(mRegion) + llformat("/%d/%d/%d", x, y, z);
		}

		case APP:
		{
			std::ostringstream app_url;
			app_url << LLGridManager::getInstance()->getAppSLURLBase()
					<< "/" << mAppCmd;
			for (LLSD::array_const_iterator i = mAppPath.beginArray();
				 i != mAppPath.endArray(); ++i)
			{
				app_url << "/" << i->asString();
			}
			if (mAppQuery.length() > 0)
			{
				app_url << "?" << mAppQuery;
			}
			return app_url.str();
		}

		default:
			llwarns << "Unexpected SLURL type for SLURL string: " << (S32)mType
					<< llendl;
			return LLStringUtil::null;
	}
}

bool LLSLURL::operator==(const LLSLURL& rhs)
{
	if (rhs.mType != mType) return false;
	switch (mType)
	{
		case LOCATION:
			return mGrid == rhs.mGrid && mRegion == rhs.mRegion &&
				   mPosition == rhs.mPosition;

		case APP:
			return getSLURLString() == rhs.getSLURLString();

		case HOME_LOCATION:
		case LAST_LOCATION:
			return true;

		default:
			return false;
	}
}

std::string LLSLURL::getLocationString() const
{
	return llformat("%s/%d/%d/%d", mRegion.c_str(),
					ll_roundp(mPosition.mV[VX]), ll_roundp(mPosition.mV[VY]),
					ll_roundp(mPosition.mV[VZ]));
}

// static
const std::string LLSLURL::typeName[NUM_SLURL_TYPES] =
{
	"INVALID",
	"LOCATION",
	"HOME_LOCATION",
	"LAST_LOCATION",
	"APP",
	"HELP"
};

std::string LLSLURL::getTypeString(SLURL_TYPE type)
{
	if (type >= INVALID && type < NUM_SLURL_TYPES)
	{
		return LLSLURL::typeName[type];
	}
	return llformat("Out of Range (%d)", type);
}

std::string LLSLURL::asString() const
{
    std::ostringstream result;
    result << "   mType: " << LLSLURL::getTypeString(mType)
		   << "   mGrid: " << getGrid()
		   << "   mRegion: " << getRegion()
		   << "   mPosition: " << mPosition
		   << "   mAppCmd:"  << getAppCmd()
		   << "   mAppPath:" << getAppPath().asString()
		   << "   mAppQueryMap:" << getAppQueryMap().asString()
		   << "   mAppQuery: " << getAppQuery();

    return result.str();
}
