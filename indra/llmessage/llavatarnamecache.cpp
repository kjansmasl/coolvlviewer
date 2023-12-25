/**
 * @file llavatarnamecache.cpp
 * @brief Provides lookup of avatar SLIDs ("bobsmith123") and display names
 * ("James Cook") from avatar UUIDs.
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

#include "linden_common.h"

#include "boost/tokenizer.hpp"

#include "llavatarnamecache.h"

#include "llcachename.h"		// we wrap this system
#include "lldate.h"
#include "llframetimer.h"
#include "llsd.h"
#include "llsdserialize.h"

///////////////////////////////////////////////////////////////////////////////
// Helper function

static const std::string MAX_AGE("max-age");
static const boost::char_separator<char> EQUALS_SEPARATOR("=");
static const boost::char_separator<char> COMMA_SEPARATOR(",");

// Parse a cache-control header to get the max-age delta-seconds. Returns true
// if header has max-age param and it parses correctly. Exported here to ease
// unit testing.
static bool max_age_from_cache_control(const std::string& cache_control,
									   S32* max_age)
{
	// Split the string on "," to get a list of directives
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	tokenizer directives(cache_control, COMMA_SEPARATOR);

	for (tokenizer::iterator token_it = directives.begin(),
							 end = directives.end();
		 token_it != end; ++token_it)
	{
		// Tokens may have leading or trailing whitespace
		std::string token = *token_it;
		LLStringUtil::trim(token);

		if (token.compare(0, MAX_AGE.size(), MAX_AGE) == 0)
		{
			// ...this token starts with max-age, so let's chop it up by "="
			tokenizer subtokens(token, EQUALS_SEPARATOR);
			tokenizer::iterator subtoken_it = subtokens.begin();

			// Must have a token
			if (subtoken_it == subtokens.end()) return false;
			std::string subtoken = *subtoken_it;

			// Must exactly equal "max-age"
			LLStringUtil::trim(subtoken);
			if (subtoken != MAX_AGE) return false;

			// Must have another token
			++subtoken_it;
			if (subtoken_it == subtokens.end()) return false;
			subtoken = *subtoken_it;

			// Must be a valid integer
			// *NOTE: atoi() returns 0 for invalid values, so we have to check
			//		  the string first.
			// *TODO: Do servers ever send "0000" for zero?  We don't handle it
			LLStringUtil::trim(subtoken);
			if (subtoken == "0")
			{
				*max_age = 0;
				return true;
			}
			S32 val = atoi(subtoken.c_str());
			if (val > 0 && val < S32_MAX)
			{
				*max_age = val;
				return true;
			}
			return false;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLAvatarName class
///////////////////////////////////////////////////////////////////////////////

bool LLAvatarName::sOmitResidentAsLastName = false;
bool LLAvatarName::sLegacyNamesForFriends = true;
bool LLAvatarName::sLegacyNamesForSpeakers = true;

// Store these in pre-built std::strings to avoid memory allocations in
// LLSD map lookups
static const std::string USERNAME("username");
static const std::string DISPLAY_NAME("display_name");
static const std::string LEGACY_FIRST_NAME("legacy_first_name");
static const std::string LEGACY_LAST_NAME("legacy_last_name");
static const std::string IS_DISPLAY_NAME_DEFAULT("is_display_name_default");
static const std::string DISPLAY_NAME_EXPIRES("display_name_expires");
static const std::string DISPLAY_NAME_NEXT_UPDATE("display_name_next_update");

LLAvatarName::LLAvatarName()
:	mUsername(),
	mDisplayName(),
	mLegacyFirstName(),
	mLegacyLastName(),
	mIsDisplayNameDefault(false),
	mIsTemporaryName(false),
	mExpires(F64_MAX),
	mNextUpdate(0.0)
{
}

bool LLAvatarName::operator<(const LLAvatarName& rhs) const
{
	if (LL_LIKELY(mUsername != rhs.mUsername))
	{
		return mUsername < rhs.mUsername;
	}
	else
	{
		return mDisplayName < rhs.mDisplayName;
	}
}

LLSD LLAvatarName::asLLSD() const
{
	LLSD sd;
	sd[USERNAME] = mUsername;
	sd[DISPLAY_NAME] = mDisplayName;
	sd[LEGACY_FIRST_NAME] = mLegacyFirstName;
	sd[LEGACY_LAST_NAME] = mLegacyLastName;
	sd[IS_DISPLAY_NAME_DEFAULT] = mIsDisplayNameDefault;
	sd[DISPLAY_NAME_EXPIRES] = LLDate(mExpires);
	sd[DISPLAY_NAME_NEXT_UPDATE] = LLDate(mNextUpdate);
	return sd;
}

void LLAvatarName::fromLLSD(const LLSD& sd)
{
	mUsername = sd[USERNAME].asString();
	mDisplayName = sd[DISPLAY_NAME].asString();
	mLegacyFirstName = sd[LEGACY_FIRST_NAME].asString();
	mLegacyLastName = sd[LEGACY_LAST_NAME].asString();
	mIsDisplayNameDefault = sd[IS_DISPLAY_NAME_DEFAULT].asBoolean();
	LLDate expires = sd[DISPLAY_NAME_EXPIRES];
	mExpires = expires.secondsSinceEpoch();
	LLDate next_update = sd[DISPLAY_NAME_NEXT_UPDATE];
	mNextUpdate = next_update.secondsSinceEpoch();
}

std::string LLAvatarName::getCompleteName() const
{
	std::string name;
	if (mUsername.empty() || mIsDisplayNameDefault)
	{
		// If the display name feature is off
		// OR this particular display name is defaulted (i.e. based
		// on user name), then display only the easier to read instance
		// of the person's name.
		name = mDisplayName;
	}
	else
	{
		name = mDisplayName + " (" + mUsername + ")";
	}
	return name;
}

std::string LLAvatarName::getLegacyName(bool full) const
{
	std::string name;
	name.reserve(mLegacyFirstName.size() + 1 + mLegacyLastName.size());
	name = mLegacyFirstName;
	if (full || !sOmitResidentAsLastName || mLegacyLastName != "Resident")
	{
		name += " ";
		name += mLegacyLastName;
	}
	return name;
}

std::string LLAvatarName::getNames(bool linefeed) const
{
	std::string name = getLegacyName();

	if (!mIsTemporaryName && !mUsername.empty() && name != mDisplayName)
	{
		if (linefeed)
		{
			name = mDisplayName + "\n[" + name + "]";
		}
		else
		{
			name = mDisplayName + " [" + name + "]";
		}
	}

	return name;
}

///////////////////////////////////////////////////////////////////////////////
// LLAvatarNameCache class
///////////////////////////////////////////////////////////////////////////////

// Time-to-live for a temp cache entry.
constexpr F64 TEMP_CACHE_ENTRY_LIFETIME = 60.0;
// Maximum time an unrefreshed cache entry is allowed
constexpr F64 MAX_UNREFRESHED_TIME = 20.0 * 60.0;

// static variables initialization
bool LLAvatarNameCache::sRunning = false;
std::string LLAvatarNameCache::sNameLookupURL;
U32 LLAvatarNameCache::sUseDisplayNames = 0;
LLFrameTimer LLAvatarNameCache::sRequestTimer;

LLAvatarNameCache::use_display_name_signal_t LLAvatarNameCache::sUseDisplayNamesSignal;

uuid_list_t LLAvatarNameCache::sAskQueue;
LLAvatarNameCache::pending_queue_t LLAvatarNameCache::sPendingQueue;
LLAvatarNameCache::signal_map_t LLAvatarNameCache::sSignalMap;
LLAvatarNameCache::cache_t LLAvatarNameCache::sCache;

F64 LLAvatarNameCache::sLastExpireCheck = 0.0;
S32 LLAvatarNameCache::sPendingRequests = 0;
S32 LLAvatarNameCache::sMaximumRequests = 32;

LLCore::HttpRequest::ptr_t LLAvatarNameCache::sHttpRequest;
LLCore::HttpHeaders::ptr_t LLAvatarNameCache::sHttpHeaders;
LLCore::HttpOptions::ptr_t LLAvatarNameCache::sHttpOptions;

/* Sample response:
<?xml version="1.0"?>
<llsd>
  <map>
	<key>agents</key>
	<array>
	  <map>
		<key>display_name_next_update</key>
		<date>2010-04-16T21:34:02+00:00Z</date>
		<key>display_name_expires</key>
		<date>2010-04-16T21:32:26.142178+00:00Z</date>
		<key>display_name</key>
		<string>MickBot390 LLQABot</string>
		<key>sl_id</key>
		<string>mickbot390.llqabot</string>
		<key>id</key>
		<string>0012809d-7d2d-4c24-9609-af1230a37715</string>
		<key>is_display_name_default</key>
		<boolean>false</boolean>
	  </map>
	  <map>
		<key>display_name_next_update</key>
		<date>2010-04-16T21:34:02+00:00Z</date>
		<key>display_name_expires</key>
		<date>2010-04-16T21:32:26.142178+00:00Z</date>
		<key>display_name</key>
		<string>Bjork Gudmundsdottir</string>
		<key>sl_id</key>
		<string>sardonyx.linden</string>
		<key>id</key>
		<string>3941037e-78ab-45f0-b421-bd6e77c1804d</string>
		<key>is_display_name_default</key>
		<boolean>true</boolean>
	  </map>
	</array>
  </map>
</llsd>
*/

//static
void LLAvatarNameCache::requestAvatarNameCacheCoro(const std::string& url,
												   uuid_vec_t agent_ids)
{
	LL_DEBUGS("NameCache") << "Entering coroutine: " << LLCoros::getName()
						   << " - URL: " << url << " - Requesting "
						   << agent_ids.size() << " agent IDs." << LL_ENDL;

	if (!sHttpRequest || !sHttpOptions || !sHttpHeaders)
	{
		llwarns << "Trying to request name cache when http parameters are not initialized"
				<< llendl;
		return;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("NameCache", sHttpRequest);
	++sPendingRequests;
	LLSD result = adapter.getAndSuspend(url, sHttpOptions, sHttpHeaders);
	--sPendingRequests;

	const LLSD& http_results =
		result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(http_results);
	bool success = bool(status);
	if (!success)
	{
		llwarns << "Fetch error: " << status.toString() << llendl;
	}
	else if (!http_results.has("success") ||
			 !http_results["success"].asBoolean())
	{
		llwarns << "Request error " << http_results["status"] << ": "
				<< http_results["message"] << llendl;
		success = false;
	}
	if (!success)
	{
		// On any sort of failure add dummy records for any agent IDs in this
		// request that we do not have cached already
		for (S32 i = 0, count = agent_ids.size(); i < count; ++i)
		{
			const LLUUID& agent_id = agent_ids[i];
			handleAgentError(agent_id);
		}
		return;
	}

	// Pull expiration out of headers if available
	const LLSD& headers = http_results["headers"];
	F64 expires = nameExpirationFromHeaders(headers);

	LLAvatarNameCache::handleAvNameCacheSuccess(result, expires);
}

//static
void LLAvatarNameCache::handleAvNameCacheSuccess(const LLSD& data, F64 expires)
{
	F64 now = LLFrameTimer::getTotalSeconds();

	if (data.has("agents"))
	{
		const LLSD& agents = data["agents"];
		for (LLSD::array_const_iterator it = agents.beginArray(),
										end = agents.endArray();
			 it != end; ++it)
		{
			const LLSD& row = *it;
			LLUUID agent_id = row["id"].asUUID();

			LLAvatarName av_name;
			av_name.fromLLSD(row);

			// Use expiration time from header
			av_name.mExpires = expires;

			// Some avatars don't have explicit display names set
			if (av_name.mDisplayName.empty())
			{
				av_name.mDisplayName = av_name.mUsername;
			}

			LL_DEBUGS("NameCache") << "Result for " << agent_id
								   << " - username '" << av_name.mUsername
								   << "' - displayname '"
								   << av_name.mDisplayName << "' - expires in "
								   << expires - now << " seconds" << LL_ENDL;

			// Cache it and fire signals
			processName(agent_id, av_name, true);
		}
	}

	if (data.has("bad_ids"))
	{
		// Same logic as error response case
		const LLSD& unresolved_agents = data["bad_ids"];
		S32 num_unresolved = unresolved_agents.size();
		if (num_unresolved > 0)
		{
			llwarns << num_unresolved << " unresolved ids; expires in "
					<< expires - now << " seconds" << llendl;

			for (LLSD::array_const_iterator
					it = unresolved_agents.beginArray(),
					end = unresolved_agents.endArray();
				 it != end; ++it)
			{
				const LLUUID& agent_id = *it;
				llwarns << "Failed id " << agent_id << llendl;
				handleAgentError(agent_id);
			}
		}
	}

	LL_DEBUGS("NameCache") << sCache.size() << " cached names" << LL_ENDL;
}

// Provide some fallback for agents that return errors
//static
void LLAvatarNameCache::handleAgentError(const LLUUID& agent_id)
{
	cache_t::iterator existing = sCache.find(agent_id);
	if (existing == sCache.end())
	{
		// There is no existing cache entry, so make a temporary name from
		// legacy
		llwarns << "Get legacy for agent " << agent_id << llendl;
		if (gCacheNamep)
		{
			gCacheNamep->get(agent_id, false, legacyNameCallback);
		}
		return;
	}

	// We have a cached (but probably expired) entry - since that would have
	// been returned by the get method, there is no need to signal anyone

	// Clear this agent from the pending list
	sPendingQueue.erase(agent_id);

	LLAvatarName& av_name = existing->second;
	LL_DEBUGS("NameCache") << "Use cache for agent " << agent_id
						   << "user '" << av_name.mUsername << "' "
						   << "display '" << av_name.mDisplayName
						   << "' expires in "
						   << av_name.mExpires - LLFrameTimer::getTotalSeconds()
						   << " seconds" << LL_ENDL;
	// Reset expiry time so we do not constantly rerequest.
	av_name.mExpires = LLFrameTimer::getTotalSeconds() + TEMP_CACHE_ENTRY_LIFETIME;
}

//static
void LLAvatarNameCache::processName(const LLUUID& agent_id,
									const LLAvatarName& av_name,
									bool add_to_cache)
{
	if (agent_id.isNull())
	{
		return;
	}

	if (add_to_cache)
	{
		sCache[agent_id] = av_name;
	}

	sPendingQueue.erase(agent_id);

	// signal everyone waiting on this name
	signal_map_t::iterator sig_it =	sSignalMap.find(agent_id);
	if (sig_it != sSignalMap.end())
	{
		callback_signal_t* signal = sig_it->second;
		(*signal)(agent_id, av_name);

		sSignalMap.erase(agent_id);

		delete signal;
		signal = NULL;
	}
}

//static
void LLAvatarNameCache::requestNamesViaCapability()
{
	if (sPendingRequests >= sMaximumRequests || sNameLookupURL.empty())
	{
		return;
	}

	F64 now = LLFrameTimer::getTotalSeconds();

	// URL format is like:
	// http://pdp60.lindenlab.com:8000/agents/?ids=3941037e-78ab-45f0-b421-bd6e77c1804d&ids=0012809d-7d2d-4c24-9609-af1230a37715&ids=0019aaba-24af-4f0a-aa72-6457953cf7f0
	//
	// Apache can handle URLs of 4096 chars, but let's be conservative
	constexpr U32 NAME_URL_MAX = 4096;
	constexpr U32 NAME_URL_SEND_THRESHOLD = 3500;

	std::string url;
	url.reserve(NAME_URL_MAX);

	std::vector<LLUUID> agent_ids;
	agent_ids.reserve(sAskQueue.size());

	U32 ids = 0;
	uuid_list_t::const_iterator it;
	while (!sAskQueue.empty())
	{
		it = sAskQueue.begin();
		LLUUID agent_id = *it;
		sAskQueue.hset_erase(it);

		if (agent_id.isNull()) continue;	// It happens...

		if (url.empty())
		{
			// ...starting new request
			url += sNameLookupURL;
			url += "?ids=";
			ids = 1;
		}
		else
		{
			// ...continuing existing request
			url += "&ids=";
			++ids;
		}
		url += agent_id.asString();
		agent_ids.push_back(agent_id);

		// Mark request as pending
		sPendingQueue[agent_id] = now;

		if (url.size() > NAME_URL_SEND_THRESHOLD)
		{
			break;
		}
	}

	if (!url.empty())
	{
		LL_DEBUGS("NameCache") << "Requested "  << ids << " ids" << LL_ENDL;
		gCoros.launch("LLAvatarNameCache::requestAvatarNameCacheCoro",
					  boost::bind(&LLAvatarNameCache::requestAvatarNameCacheCoro,
								  url, agent_ids));
	}
}

//static
void LLAvatarNameCache::legacyNameCallback(const LLUUID& agent_id,
										   const std::string& full_name,
										   bool is_group)
{
	// Construct a dummy record for this name. By convention, SLID is blank
	// Never expires, but not written to disk, so lasts until end of session.
	LLAvatarName av_name;
	LL_DEBUGS("NameCache") << "Callback for agent " << agent_id
						   << " - full name '" << full_name
						   << (is_group ? "' (group)" : "'") << LL_ENDL;
	buildLegacyName(full_name, &av_name);

	// Add to cache, because if we don't we'll keep rerequesting the same
	// record forever. buildLegacyName should always guarantee that these
	// records expire reasonably soon (in TEMP_CACHE_ENTRY_LIFETIME seconds),
	// so if the failure was due to something temporary we will eventually
	// request and get the right data.
	processName(agent_id, av_name, true);
}

//static
void LLAvatarNameCache::requestNamesViaLegacy()
{
	if (!gCacheNamep)
	{
		llwarns_once << "Cache name not initialized or already deleted !"
					 << llendl;
		return;
	}

	constexpr S32 MAX_REQUESTS = 100;
	F64 now = LLFrameTimer::getTotalSeconds();
	std::string full_name;
	uuid_list_t::const_iterator it;
	for (S32 requests = 0; requests < MAX_REQUESTS && !sAskQueue.empty();
		 ++requests)
	{
		it = sAskQueue.begin();
		LLUUID agent_id = *it;
		sAskQueue.hset_erase(it);

		if (agent_id.isNull()) continue;	// It happens...

		// Mark as pending first, just in case the callback is immediately
		// invoked below. This should never happen in practice.
		sPendingQueue[agent_id] = now;

		LL_DEBUGS("NameCache") << "Requesting name for agent " << agent_id
							   << LL_ENDL;

		gCacheNamep->get(agent_id, false, legacyNameCallback);
	}
}

//static
void LLAvatarNameCache::initClass()
{
	sHttpRequest = DEFAULT_HTTP_REQUEST;
	sHttpHeaders = DEFAULT_HTTP_HEADERS;
	sHttpOptions = DEFAULT_HTTP_OPTIONS;
}

//static
void LLAvatarNameCache::cleanupClass()
{
	sHttpRequest.reset();
	sHttpHeaders.reset();
	sHttpOptions.reset();
	sCache.clear();
}

//static
bool LLAvatarNameCache::importFile(std::istream& istr)
{
	LLSD data;
	if (LLSDSerialize::fromXMLDocument(data, istr) == LLSDParser::PARSE_FAILURE)
	{
		return false;
	}

	// by convention LLSD storage is a map
	// we only store one entry in the map
	LLSD agents = data["agents"];

	LLUUID agent_id;
	LLAvatarName av_name;
	for (LLSD::map_const_iterator it = agents.beginMap(),
								  end = agents.endMap();
		 it != end; ++it)
	{
		agent_id.set(it->first);
		av_name.fromLLSD(it->second);
		sCache[agent_id] = av_name;
	}
	llinfos << "Loaded " << sCache.size() << " avatar names." << llendl;

	return true;
}

//static
void LLAvatarNameCache::exportFile(std::ostream& ostr)
{
	LLSD agents;
	F64 max_unrefreshed = LLFrameTimer::getTotalSeconds() - MAX_UNREFRESHED_TIME;
	for (cache_t::const_iterator it = sCache.begin(), end = sCache.end();
		 it != end; ++it)
	{
		const LLUUID& agent_id = it->first;
		const LLAvatarName& av_name = it->second;
		// Do not write temporary or expired entries to the stored cache
		if (!av_name.mIsTemporaryName && av_name.mExpires >= max_unrefreshed)
		{
			// key must be a string
			agents[agent_id.asString()] = av_name.asLLSD();
		}
	}
	LLSD data;
	data["agents"] = agents;
	LLSDSerialize::toPrettyXML(data, ostr);
}

//static
void LLAvatarNameCache::setNameLookupURL(const std::string& name_lookup_url)
{
	sNameLookupURL = name_lookup_url;
}

//static
bool LLAvatarNameCache::hasNameLookupURL()
{
	return !sNameLookupURL.empty();
}

//static
void LLAvatarNameCache::setMaximumRequests(U32 num)
{
	sMaximumRequests = num;
}

//static
void LLAvatarNameCache::idle()
{
	// By convention, start running at first idle() call
	sRunning = true;

	constexpr F32 SECS_BETWEEN_REQUESTS = 0.1f;
	if (!sRequestTimer.hasExpired())
	{
		return;
	}

	if (!sAskQueue.empty())
	{
		if (useDisplayNames())
		{
			requestNamesViaCapability();
		}
		else
		{
			// Fall back to legacy name cache system
			requestNamesViaLegacy();
		}
	}

	if (sAskQueue.empty())
	{
		// Cleared the list, reset the request timer.
		sRequestTimer.resetWithExpiry(SECS_BETWEEN_REQUESTS);
	}

	// Erase anything that has not been refreshed for more than
	// MAX_UNREFRESHED_TIME
	eraseUnrefreshed();
}

//static
bool LLAvatarNameCache::isRequestPending(const LLUUID& agent_id)
{
	pending_queue_t::const_iterator it = sPendingQueue.find(agent_id);
	if (it == sPendingQueue.end())
	{
		return false;
	}
	// In the list of requests in flight, retry if too old
	constexpr F64 PENDING_TIMEOUT_SECS = 5.0 * 60.0;
	F64 expire_time = LLFrameTimer::getTotalSeconds() - PENDING_TIMEOUT_SECS;
	return it->second > expire_time;
}

//static
void LLAvatarNameCache::eraseUnrefreshed()
{
	F64 now = LLFrameTimer::getTotalSeconds();
	F64 max_unrefreshed = now - MAX_UNREFRESHED_TIME;

	if (!sLastExpireCheck || sLastExpireCheck < max_unrefreshed)
	{
		sLastExpireCheck = now;
		cache_t::iterator it = sCache.begin();
		while (it != sCache.end())
		{
			cache_t::iterator cur = it++;
			const LLAvatarName& av_name = cur->second;
			if (av_name.mExpires < max_unrefreshed)
			{
				LL_DEBUGS("NameCache") << cur->first << " user '"
									   << av_name.mUsername << "' expired "
									   << now - av_name.mExpires << " secs ago"
									   << LL_ENDL;
				sCache.erase(cur);
			}
		}
		llinfos << sCache.size() << " cached avatar names" << llendl;
	}
}

//static
void LLAvatarNameCache::buildLegacyName(const std::string& full_name,
										LLAvatarName* av_name)
{
	llassert(av_name);
	av_name->mUsername = "";
	av_name->mDisplayName = full_name;
	size_t i = full_name.find(' ');
	if (i > 0)
	{
		av_name->mLegacyFirstName = full_name.substr(0, i);
		av_name->mLegacyLastName = full_name.substr(i + 1);
	}
	else
	{
		// Should never happen... Just in case.
		av_name->mLegacyFirstName = full_name;
		av_name->mLegacyLastName = "Resident";
	}
	av_name->mIsDisplayNameDefault = true;
	av_name->mIsTemporaryName = true;
	av_name->mExpires = LLFrameTimer::getTotalSeconds() + TEMP_CACHE_ENTRY_LIFETIME;
	LL_DEBUGS("NameCache") << "Processed " << full_name << LL_ENDL;
}

// Fills in av_name if it has it in the cache, even if expired (can check
// expiry time). Returns bool specifying if av_name was filled, false
// otherwise.
//static
bool LLAvatarNameCache::get(const LLUUID& agent_id, LLAvatarName* av_name)
{
	if (sRunning)
	{
		// ...only do immediate lookups when cache is running
		if (useDisplayNames())
		{
			// ...use display names cache
			cache_t::iterator it = sCache.find(agent_id);
			if (it != sCache.end())
			{
				*av_name = it->second;

				// re-request name if entry is expired
				if (av_name->mExpires < LLFrameTimer::getTotalSeconds())
				{
					if (!isRequestPending(agent_id))
					{
						LL_DEBUGS("NameCache") << "Refreshing cache for agent "
											   << agent_id << LL_ENDL;
						sAskQueue.emplace(agent_id);
					}
				}

				return true;
			}
		}
		else if (gCacheNamep)
		{
			// ...use legacy names cache
			std::string full_name;
			if (gCacheNamep->getFullName(agent_id, full_name))
			{
				buildLegacyName(full_name, av_name);
				return true;
			}
		}
	}

	if (!isRequestPending(agent_id))
	{
		LL_DEBUGS("NameCache") << "Request queued for agent " << agent_id
							   << LL_ENDL;
		sAskQueue.emplace(agent_id);
	}

	return false;
}

//static
void LLAvatarNameCache::fireSignal(const LLUUID& agent_id,
								   const callback_slot_t& slot,
								   const LLAvatarName& av_name)
{
	callback_signal_t signal;
	signal.connect(slot);
	signal(agent_id, av_name);
}

//static
LLAvatarNameCache::callback_connection_t LLAvatarNameCache::get(const LLUUID& agent_id,
																callback_slot_t slot)
{
	callback_connection_t connection;

	if (sRunning)
	{
		// ...only do immediate lookups when cache is running
		if (useDisplayNames())
		{
			// ...use new cache
			cache_t::iterator it = sCache.find(agent_id);
			if (it != sCache.end())
			{
				const LLAvatarName& av_name = it->second;

				if (av_name.mExpires > LLFrameTimer::getTotalSeconds())
				{
					// ...name already exists in cache, fire callback now
					fireSignal(agent_id, slot, av_name);

					return connection;
				}
			}
		}
		else if (gCacheNamep)
		{
			// ...use old name system
			std::string full_name;
			if (gCacheNamep->getFullName(agent_id, full_name))
			{
				LLAvatarName av_name;
				buildLegacyName(full_name, &av_name);
				fireSignal(agent_id, slot, av_name);
				return connection;
			}
		}
	}

	// Schedule a request
	if (!isRequestPending(agent_id))
	{
		sAskQueue.emplace(agent_id);
	}

	// Always store additional callback, even if request is pending
	signal_map_t::iterator sig_it = sSignalMap.find(agent_id);
	if (sig_it == sSignalMap.end())
	{
		// New callback for this Id
		callback_signal_t* signal = new callback_signal_t();
		connection = signal->connect(slot);
		sSignalMap.emplace(agent_id, signal);
	}
	else
	{
		// Existing callback, bind additional slot
		callback_signal_t* signal = sig_it->second;
		connection = signal->connect(slot);
	}

	return connection;
}

//static
void LLAvatarNameCache::setUseDisplayNames(U32 use)
{
	if (use != sUseDisplayNames)
	{
		if (use > 2)
		{
			sUseDisplayNames = 1;
		}
		else
		{
			sUseDisplayNames = use;
		}
		// Flush our cache
		sCache.clear();

		sUseDisplayNamesSignal();
	}
}

//static
U32 LLAvatarNameCache::useDisplayNames()
{
	// Must be both manually set on and able to look up names.
	if (sNameLookupURL.empty())
	{
		return 0;
	}
	else
	{
		return sUseDisplayNames;
	}
}

//static
void LLAvatarNameCache::erase(const LLUUID& agent_id)
{
	sCache.erase(agent_id);
}

//static
void LLAvatarNameCache::insert(const LLUUID& agent_id,
							   const LLAvatarName& av_name)
{
	// *TODO: update timestamp if zero ?
	sCache[agent_id] = av_name;
}

//static
F64 LLAvatarNameCache::nameExpirationFromHeaders(const LLSD& headers)
{
	F64 expires = 0.0;
	if (expirationFromCacheControl(headers, &expires))
	{
		return expires;
	}
	else
	{
		// With no expiration info, default to an hour
		constexpr F64 DEFAULT_EXPIRES = 60.0 * 60.0;
		F64 now = LLFrameTimer::getTotalSeconds();
		return now + DEFAULT_EXPIRES;
	}
}

//static
bool LLAvatarNameCache::expirationFromCacheControl(const LLSD& headers,
												   F64* expires)
{
	bool fromCacheControl = false;
	F64 now = LLFrameTimer::getTotalSeconds();

	// Allow the header to override the default
	std::string cache_control;
	if (headers.has(HTTP_IN_HEADER_CACHE_CONTROL))
	{
		cache_control = headers[HTTP_IN_HEADER_CACHE_CONTROL].asString();
	}

	if (!cache_control.empty())
	{
		S32 max_age = 0;
		if (max_age_from_cache_control(cache_control, &max_age))
		{
			*expires = now + (F64)max_age;
			fromCacheControl = true;
		}
	}
	LL_DEBUGS("NameCache") << (fromCacheControl ? "Expires based on cache control "
												: "default expiration ")
						   << "in " << *expires - now << " seconds" << LL_ENDL;

	return fromCacheControl;
}

//static
void LLAvatarNameCache::addUseDisplayNamesCallback(const use_display_name_signal_t::slot_type& cb)
{
	sUseDisplayNamesSignal.connect(cb);
}
