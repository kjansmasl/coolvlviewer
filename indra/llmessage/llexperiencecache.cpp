/**
 * @file llexperiencecache.cpp
 * @brief Caches information relating to experience keys
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include "boost/concept_check.hpp"
#include "boost/tokenizer.hpp"

#include "llexperiencecache.h"

#include "llcoproceduremanager.h"
#include "llsdserialize.h"
#include "lluri.h"

const std::string LLExperienceCache::PRIVATE_KEY	= "private_id";
const std::string LLExperienceCache::MISSING		= "DoesNotExist";
const std::string LLExperienceCache::AGENT_ID		= "agent_id";
const std::string LLExperienceCache::GROUP_ID		= "group_id";
const std::string LLExperienceCache::EXPERIENCE_ID	= "public_id";
const std::string LLExperienceCache::NAME			= "name";
const std::string LLExperienceCache::PROPERTIES		= "properties";
const std::string LLExperienceCache::EXPIRES		= "expiration";
const std::string LLExperienceCache::DESCRIPTION	= "description";
const std::string LLExperienceCache::QUOTA			= "quota";
const std::string LLExperienceCache::MATURITY		= "maturity";
const std::string LLExperienceCache::METADATA		= "extended_metadata";
const std::string LLExperienceCache::SLURL			= "slurl";

static const std::string MAX_AGE = "max-age";

const boost::char_separator<char> EQUALS_SEPARATOR("=");
const boost::char_separator<char> COMMA_SEPARATOR(",");

//static
std::string LLExperienceCache::sLookupURL;
bool LLExperienceCache::sShutdown = false;

LLExperienceCache::LLExperienceCache()
:	// NOTE: by using these instead of omitting the corresponding
	// xxxAndSuspend() parameters, we avoid seeing such classes constructed
	// and destroyed each time...
	mHttpOptions(new LLCore::HttpOptions),
	mHttpHeaders(new LLCore::HttpHeaders)
{
}

LLExperienceCache::~LLExperienceCache()
{
	mHttpOptions.reset();
	mHttpHeaders.reset();
}

//virtual
void LLExperienceCache::initSingleton()
{
	LLCoprocedureManager::getInstance()->initializePool("ExpCache");
	gCoros.launch("LLExperienceCache::idleCoro",
				  boost::bind(&LLExperienceCache::idleCoro, this));
}

void LLExperienceCache::cleanup()
{
	sShutdown = true;
}

void LLExperienceCache::importFile(std::istream& istr)
{
	LLSD data;
	S32 parse_count = LLSDSerialize::fromXMLDocument(data, istr);
	if (parse_count < 1) return;

	LLSD experiences = data["experiences"];

	LLUUID public_key;
	for (LLSD::map_const_iterator it = experiences.beginMap(),
								  end = experiences.endMap();
		 it != end; ++it)
	{
		public_key.set(it->first);
		mCache[public_key] = it->second;
	}

	LL_DEBUGS("ExperienceCache") << "Loaded " << mCache.size()
								 << " experiences." << LL_ENDL;
}

void LLExperienceCache::exportFile(std::ostream& ostr) const
{
	LLSD experiences;
	for (cache_t::const_iterator it = mCache.begin(), end = mCache.end();
		 it != end; ++it)
	{
		if (!it->second.has(EXPERIENCE_ID) ||
			it->second[EXPERIENCE_ID].asUUID().isNull() ||
			it->second.has("DoesNotExist") ||
			(it->second.has(PROPERTIES) &&
			 (it->second[PROPERTIES].asInteger() & PROPERTY_INVALID)))
		{
			continue;
		}

		experiences[it->first.asString()] = it->second;
	}

	LLSD data;
	data["experiences"] = experiences;

	LLSDSerialize::toPrettyXML(data, ostr);
}

#if 0	// Not used
LLExperienceCache::key_map_t LLExperienceCache::sPrivateToPublicKeyMap;

void LLExperienceCache::bootstrap(const LLSD& legacy_keys,
								  S32 initial_expiration)
{
	mapKeys(legacy_keys);
	for (LLSD::array_const_iterator it = legacy_keys.beginArray(),
									end = legacy_keys.endArray();
		 it != end; ++it)
	{
		LLSD experience = *it;
		if (experience.has(EXPERIENCE_ID))
		{
			if (!experience.has(EXPIRES))
			{
				experience[EXPIRES] = initial_expiration;
			}
			processExperience(experience[EXPERIENCE_ID].asUUID(), experience);
		}
		else
		{
			llwarns << "Skipping bootstrap entry which is missing "
					<< EXPERIENCE_ID << llendl;
		}
	}
}

LLUUID LLExperienceCache::getExperienceId(const LLUUID& private_key,
										  bool null_if_not_found)
{
	if (private_key.isNull())
	{
		return LLUUID::null;
	}

	key_map_t::const_iterator it = sPrivateToPublicKeyMap.find(private_key);
	if (it == sPrivateToPublicKeyMap.end())
	{
		return null_if_not_found ? LLUUID::null : private_key;
	}

	llwarns << "Converted private key " << private_key << " to experience_id "
			<< it->second << llendl;

	return it->second;
}

void LLExperienceCache::mapKeys(const LLSD& legacy_keys)
{
	for (LLSD::array_const_iterator exp = legacy_keys.beginArray(),
									end = legacy_keys.endArray();
		 exp != end; ++exp)
	{
		if (exp->has(EXPERIENCE_ID) && exp->has(PRIVATE_KEY))
		{
			sPrivateToPublicKeyMap[(*exp)[PRIVATE_KEY].asUUID()] =
				(*exp)[EXPERIENCE_ID].asUUID();
		}
	}
}
#endif

void LLExperienceCache::processExperience(const LLUUID& public_key,
										  const LLSD& experience)
{
	LL_DEBUGS("ExperienceCache") << "Processing experience: "
								 << experience[NAME] << " - Key: "
								 << public_key.asString() << LL_ENDL;

	mCache[public_key] = experience;
	LLSD& row = mCache[public_key];
	if (row.has(EXPIRES))
	{
		row[EXPIRES] = row[EXPIRES].asReal() + LLFrameTimer::getTotalSeconds();
	}

	if (row.has(EXPERIENCE_ID))
	{
		mPendingQueue.erase(row[EXPERIENCE_ID].asUUID());
	}

	// signal
	signal_map_t::iterator sig_it =	mSignalMap.find(public_key);
	if (sig_it != mSignalMap.end())
	{
		signal_ptr signal = sig_it->second;
		if (signal)
		{
			(*signal)(experience);
		}

		mSignalMap.erase(public_key);
	}
}

void LLExperienceCache::requestExperiencesCoro(adapter_ptr_t& adapter,
											   std::string url,
											   uuid_list_t requests)
{
	LLSD result = adapter->getAndSuspend(url, mHttpOptions, mHttpHeaders);
		
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		F64 now = LLFrameTimer::getTotalSeconds();

		// Compute the retry delay, depending on the HTTP error or header
		S32 hstatus = status.getType();
		const LLSD& http_results =
			result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		const LLSD& headers =
			http_results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
		F64 retry_after = getErrorRetryDeltaTime(hstatus, headers);

		// build dummy entries for the failed requests
		for (uuid_list_t::const_iterator it = requests.begin(),
										 end = requests.end();
			 it != end; ++it)
		{
			const LLUUID& id = *it;
			if (id.notNull())
			{
				LLSD exp = get(id);
				if (exp.isUndefined())
				{
					// Leave the properties alone if we already have a cache entry
					// for this experience
					exp[PROPERTIES] = PROPERTY_INVALID;
				}
				exp[EXPIRES] = now + retry_after;
				exp[EXPERIENCE_ID] = id;
				exp["key_type"] = EXPERIENCE_ID;
				exp["uuid"] = id;
				exp["error"] = (LLSD::Integer)hstatus;
				exp[QUOTA] = DEFAULT_QUOTA;

				processExperience(id, exp);
			}
		}
		return;
	}

	const LLSD& experiences = result["experience_keys"];
	for (LLSD::array_const_iterator it = experiences.beginArray(),
									end = experiences.endArray();
		 it != end; ++it)
	{
		const LLSD& row = *it;
		LLUUID public_key = row[EXPERIENCE_ID].asUUID();

		LL_DEBUGS("ExperienceCache") << "Received result for " << public_key
									 << " display '" << row[NAME].asString()
									 << "'" << LL_ENDL ;

		processExperience(public_key, row);
	}

	const LLSD& error_ids = result["error_ids"];
	for (LLSD::array_const_iterator it = error_ids.beginArray(),
									end = error_ids.endArray();
		 it != end; ++it)
	{
		LLUUID id = it->asUUID();
		if (id.notNull())
		{
			LLSD exp;
			exp[EXPIRES] = DEFAULT_EXPIRATION;
			exp[EXPERIENCE_ID] = id;
			exp[PROPERTIES] = PROPERTY_INVALID;
			exp[MISSING] = true;
			exp[QUOTA] = DEFAULT_QUOTA;
			processExperience(id, exp);
			llwarns << "Error result for " << id << llendl;
		}
	}
}

void LLExperienceCache::requestExperiences()
{
	if (sLookupURL.empty())
	{
		return;
	}

	F64 now = LLFrameTimer::getTotalSeconds();

	constexpr U32 EXP_URL_SEND_THRESHOLD = 3000;
	// Note: "PAGE_SIZE" is #defined somewhere in macOS headers... So we cannot
	// use it.
	constexpr U32 PAGE_SIZE_ = EXP_URL_SEND_THRESHOLD / UUID_STR_LENGTH;
	const std::string page_size = llformat("?page_size=%d", PAGE_SIZE_);
	const std::string key_query = "&" + EXPERIENCE_ID + "=";
	std::string uri = sLookupURL + page_size;

	LLUUID key;
	uuid_list_t requests;
	while (!mRequestQueue.empty() && !sShutdown)
	{
		uuid_list_t::iterator it = mRequestQueue.begin();
		key = *it;
		mRequestQueue.hset_erase(it);
		if (key.isNull()) continue;

		requests.emplace(key);
		uri += key_query + key.asString();
		mPendingQueue[key] = now;

		if (mRequestQueue.empty() || uri.size() > EXP_URL_SEND_THRESHOLD)
		{
			LL_DEBUGS("ExperienceCache") << "Query: " << uri << LL_ENDL;
			LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
			cpmgr->enqueueCoprocedure("ExpCache", "RequestExperiences",
									  boost::bind(&LLExperienceCache::requestExperiencesCoro,
												  this, _1, uri, requests));
			uri = sLookupURL + page_size;
			requests.clear();
		}
	}
}

bool LLExperienceCache::isRequestPending(const LLUUID& public_key)
{
	bool is_pending = false;
	constexpr F64 PENDING_TIMEOUT_SECS = 300.0;

	pending_map_t::const_iterator it = mPendingQueue.find(public_key);
	if (it != mPendingQueue.end())
	{
		F64 expire_time = LLFrameTimer::getTotalSeconds() -
						  PENDING_TIMEOUT_SECS;
		is_pending = it->second > expire_time;
	}

	return is_pending;
}

void LLExperienceCache::idleCoro()
{
	constexpr F32 SECS_BETWEEN_REQUESTS = 0.5f;
	constexpr F32 ERASE_EXPIRED_TIMEOUT = 60.f; // seconds

	llinfos << "Launching Experience cache idle coro." << llendl;

	do
	{
		if (mEraseExpiredTimer.checkExpirationAndReset(ERASE_EXPIRED_TIMEOUT))
		{
			eraseExpired();
		}

		if (!mRequestQueue.empty())
		{
			requestExperiences();
		}
		llcoro::suspendUntilTimeout(SECS_BETWEEN_REQUESTS);
	}
	while (!sShutdown);

	llinfos << "Experience cache idle coroutine exited." << llendl;
}

void LLExperienceCache::erase(const LLUUID& key)
{
	cache_t::iterator it = mCache.find(key);
	if (it != mCache.end())
	{
		mCache.hmap_erase(it);
	}
}

void LLExperienceCache::eraseExpired()
{
	F64 now = LLFrameTimer::getTotalSeconds();
	cache_t::iterator it = mCache.begin();
	while (it != mCache.end())
	{
		cache_t::iterator cur = it++;
		const LLSD& exp = cur->second;
		if (exp.has(EXPIRES) && exp[EXPIRES].asReal() < now)
		{
			if (!exp.has(EXPERIENCE_ID))
			{
				llwarns << "Removing an experience with no id" << llendl;
				mCache.erase(cur);
			}
			else
			{
				LLUUID id = exp[EXPERIENCE_ID].asUUID();
				LLUUID private_key;
				if (exp.has(PRIVATE_KEY))
				{
					private_key = exp[PRIVATE_KEY].asUUID();
				}
				if (private_key.notNull() || !exp.has("DoesNotExist"))
				{
					fetch(id, true);
				}
				else
				{
					llwarns << "Removing invalid experience: " << id << llendl;
					mCache.erase(cur);
				}
			}
		}
	}
}

bool LLExperienceCache::fetch(const LLUUID& key, bool refresh)
{
	if (!key.isNull() && !isRequestPending(key) &&
		(refresh || mCache.count(key) == 0))
	{
		LL_DEBUGS("ExperienceCache") << "Queue request for " << EXPERIENCE_ID
									 << " " << key << LL_ENDL;
		mRequestQueue.emplace(key);
		return true;
	}
	return false;
}

void LLExperienceCache::insert(const LLSD& exp_data)
{
	if (exp_data.has(EXPERIENCE_ID))
	{
		processExperience(exp_data[EXPERIENCE_ID].asUUID(), exp_data);
	}
	else
	{
		llwarns << "Ignoring cache insert of experience which is missing "
				<< EXPERIENCE_ID << llendl;
	}
}

const LLSD& LLExperienceCache::get(const LLUUID& key)
{
	static const LLSD empty;

	if (key.isNull())
	{
		return empty;
	}

	cache_t::const_iterator it = mCache.find(key);
	if (it != mCache.end())
	{
		return it->second;
	}

	fetch(key);

	return empty;
}

void LLExperienceCache::get(const LLUUID& key, experience_get_fn_t slot)
{
	if (key.isNull()) return;

	cache_t::const_iterator it = mCache.find(key);
	if (it != mCache.end())
	{
		// ...name already exists in cache, fire callback now
		callback_signal_t signal;
		signal.connect(slot);
		signal(it->second);
		return;
	}

	fetch(key);

	signal_ptr signal = signal_ptr(new callback_signal_t());

	std::pair<signal_map_t::iterator, bool> result =
		mSignalMap.emplace(key, signal);
	if (!result.second)
	{
		signal = (*result.first).second;
	}
	if (signal)
	{
		signal->connect(slot);
	}
}

void LLExperienceCache::fetchAssociatedExperience(const LLUUID& object_id,
												  const LLUUID& item_id,
												  const std::string& cap_url,
												  experience_get_fn_t fn)
{
	if (mCapability.empty())
	{
		llwarns << "Capability query method not set." << llendl;
		return;
	}

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "Fetch Associated",
							  boost::bind(&LLExperienceCache::fetchAssociatedExperienceCoro,
										  this, _1, object_id, item_id,
										  cap_url, fn));
}

void LLExperienceCache::fetchAssociatedExperienceCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
													  LLUUID object_id,
													  LLUUID item_id,
													  std::string url,
													  experience_get_fn_t fn)
{
	if (url.empty())
	{
		url = mCapability("GetMetadata");
		if (url.empty())
		{
			llwarns << "No GetMetadata capability." << llendl;
			return;
		}
	}

	LLSD fields;
	fields.append("experience");
	LLSD data;
	data["object-id"] = object_id;
	data["item-id"] = item_id;
	data["fields"] = fields;
	LLSD result = adapter->postAndSuspend(url, data, mHttpOptions,
										  mHttpHeaders);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result.has("experience"))
	{
		LLSD failure;
		if (!status)
		{
			failure["error"] = (LLSD::Integer)status.getType();
			failure["message"] = status.getMessage();
		}
		else
		{
			failure["error"] = -1;
			failure["message"] = "no experience";
		}
		if (fn && !fn.empty())
		{
			fn(failure);
		}
	}
	else
	{
		LLUUID exp_id = result["experience"].asUUID();
		get(exp_id, fn);
	}
}

void LLExperienceCache::findExperienceByName(std::string text, int page,
											 experience_get_fn_t fn)
{
	if (mCapability.empty())
	{
		llwarns << "Capability query method not set." << llendl;
		return;
	}

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "Search Name",
							  boost::bind(&LLExperienceCache::findExperienceByNameCoro,
										  this, _1, text, page, fn));
}

void LLExperienceCache::findExperienceByNameCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
												 std::string text, int page,
												 experience_get_fn_t fn)
{
	std::ostringstream url;
	url << mCapability("FindExperienceByName")  << "?page=" << page
		<< "&page_size=" << SEARCH_PAGE_SIZE << "&query="
		<< LLURI::escape(text);

	LLSD result = adapter->getAndSuspend(url.str(), mHttpOptions,
										 mHttpHeaders);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		fn(LLSD());
		return;
	}

	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);

	const LLSD& experiences = result["experience_keys"];
	for (LLSD::array_const_iterator it = experiences.beginArray(),
									end = experiences.endArray();
		 it != end; ++it)
	{
		insert(*it);
	}

	fn(result);
}

void LLExperienceCache::getGroupExperiences(const LLUUID& group_id,
											experience_get_fn_t fn)
{
	if (mCapability.empty())
	{
		llwarns << "Capability query method not set." << llendl;
		return;
	}

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "Group Experiences",
							  boost::bind(&LLExperienceCache::getGroupExperiencesCoro,
										  this, _1, group_id, fn));
}

void LLExperienceCache::getGroupExperiencesCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
												LLUUID group_id,
												experience_get_fn_t fn)
{
	// search for experiences owned by the current group
	std::string url = mCapability("GroupExperiences");
	if (url.empty())
	{
		llwarns << "No GroupExperiences capability" << llendl;
		return;
	}
	url += "?" + group_id.asString();

	LLSD result = adapter->getAndSuspend(url, mHttpOptions, mHttpHeaders);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		fn(LLSD());
		return;
	}

	const LLSD& exp_ids = result["experience_ids"];
	fn(exp_ids);
}

void LLExperienceCache::getRegionExperiences(cap_query_fn_t regioncaps,
											 experience_get_fn_t fn)
{
	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "Region Experiences",
							  boost::bind(&LLExperienceCache::regionExperiencesCoro,
										  this, _1, regioncaps, false, LLSD(),
										  fn));
}

void LLExperienceCache::setRegionExperiences(cap_query_fn_t regioncaps,
											 const LLSD& experiences,
											 experience_get_fn_t fn)
{
	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "Region Experiences",
							  boost::bind(&LLExperienceCache::regionExperiencesCoro,
										  this, _1, regioncaps, true,
										  experiences, fn));
}

void LLExperienceCache::regionExperiencesCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
											  cap_query_fn_t regioncaps,
											  bool update, LLSD experiences,
											  experience_get_fn_t fn)
{
	// Search for experiences owned by the current group
	const std::string& url = regioncaps("RegionExperiences");
	if (url.empty())
	{
		llwarns << "No RegionExperiences capability" << llendl;
		return;
	}

	LLSD result;
	if (update)
	{
		result = adapter->postAndSuspend(url, experiences, mHttpOptions,
										 mHttpHeaders);
	}
	else
	{
		result = adapter->getAndSuspend(url, mHttpOptions, mHttpHeaders);
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
#if 0
		fn(LLSD());
#endif
		return;
	}

	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
	fn(result);
}

#define COROCAST(T)		static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, const LLSD&, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)
#define COROCAST2(T)	static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)

void LLExperienceCache::getExperiencePermission(const LLUUID& exp_id,
												experience_get_fn_t fn)
{
	if (mCapability.empty())
	{
		llwarns << "Capability query method not set." << llendl;
		return;
	}

	std::string url = mCapability("ExperiencePreferences") + "?" +
					  exp_id.asString();

	permissionInvoker_fn invoker(boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
											 // _1 -> adapter
											 // _2 -> url
											 _1, _2,
											 LLCore::HttpOptions::ptr_t(),
											 LLCore::HttpHeaders::ptr_t()));

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "Preferences Set",
							  boost::bind(&LLExperienceCache::experiencePermissionCoro,
										  this, _1, invoker, url, fn));
}

void LLExperienceCache::setExperiencePermission(const LLUUID& exp_id,
												const std::string& perm,
												experience_get_fn_t fn)
{
	if (mCapability.empty())
	{
		llwarns << "Capability query method not set." << llendl;
		return;
	}

	std::string url = mCapability("ExperiencePreferences");
	if (url.empty())
	{
		llwarns << "No ExperiencePreferences capability" << llendl;
		return;
	}

	LLSD perm_data;
	perm_data["permission"] = perm;
	LLSD data;
	data[exp_id.asString()] = perm_data;
	permissionInvoker_fn invoker(boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::putAndSuspend),
											 // _1 -> adapter
											 // _2 -> url
											 _1, _2, data,
											 LLCore::HttpOptions::ptr_t(),
											 LLCore::HttpHeaders::ptr_t()));

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "Preferences Set",
							  boost::bind(&LLExperienceCache::experiencePermissionCoro,
										  this, _1, invoker, url, fn));
}

void LLExperienceCache::forgetExperiencePermission(const LLUUID& exp_id,
												   experience_get_fn_t fn)
{
	if (mCapability.empty())
	{
		llwarns << "Capability query method not set." << llendl;
		return;
	}

	std::string url = mCapability("ExperiencePreferences") + "?" + exp_id.asString();
	permissionInvoker_fn invoker(boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::deleteAndSuspend),
											 // _1 -> adapter
											 // _2 -> url
											 _1, _2,
											 LLCore::HttpOptions::ptr_t(),
											 LLCore::HttpHeaders::ptr_t()));

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "Preferences Set",
							  boost::bind(&LLExperienceCache::experiencePermissionCoro,
										  this, _1, invoker, url, fn));
}

void LLExperienceCache::experiencePermissionCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
												 permissionInvoker_fn invokerfn,
												 std::string url,
												 experience_get_fn_t fn)
{
	// search for experiences owned by the current group

	LLSD result = invokerfn(adapter, url);

	LLCore::HttpStatus status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		fn(result);
	}
}

void LLExperienceCache::getExperienceAdmin(const LLUUID& exp_id,
										   experience_get_fn_t fn)
{
	if (mCapability.empty())
	{
		llwarns << "Capability query method not set." << llendl;
		return;
	}

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "IsAdmin",
							  boost::bind(&LLExperienceCache::getExperienceAdminCoro,
										  this, _1, exp_id, fn));
}

void LLExperienceCache::getExperienceAdminCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
											   LLUUID exp_id,
											   experience_get_fn_t fn)
{
	std::string url = mCapability("IsExperienceAdmin");
	if (url.empty())
	{
		llwarns << "No Region Experiences capability" << llendl;
		return;
	}
	url += "?experience_id=" + exp_id.asString();

	LLSD result = adapter->getAndSuspend(url, mHttpOptions, mHttpHeaders);
#if 0
	LLCore::HttpStatus status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
#endif
	fn(result);
}

void LLExperienceCache::updateExperience(LLSD upd_data, experience_get_fn_t fn)
{
	if (mCapability.empty())
	{
		llwarns << "Capability query method not set." << llendl;
		return;
	}

	LLCoprocedureManager* cpmgr = LLCoprocedureManager::getInstance();
	cpmgr->enqueueCoprocedure("ExpCache", "IsAdmin",
							  boost::bind(&LLExperienceCache::updateExperienceCoro,
										  this, _1, upd_data, fn));
}

void LLExperienceCache::updateExperienceCoro(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
											 LLSD upd_data,
											 experience_get_fn_t fn)
{
	std::string url = mCapability("UpdateExperience");
	if (url.empty())
	{
		llwarns << "No UpdateExperience capability" << llendl;
		return;
	}

	upd_data.erase(QUOTA);
	upd_data.erase(EXPIRES);
	upd_data.erase(AGENT_ID);
	LLSD result = adapter->postAndSuspend(url, upd_data, mHttpOptions,
										  mHttpHeaders);
#if 0
	LLCore::HttpStatus status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
#endif
	fn(result);
}

// Returns time to retry a request that generated an error, based on error type
// and headers. Return value is seconds-since-epoch.
//static
F64 LLExperienceCache::getErrorRetryDeltaTime(S32 status, LLSD headers)
{
	// Retry-After takes priority
	LLSD retry_after = headers["retry-after"];
	if (retry_after.isDefined())
	{
		// We only support the delta-seconds type
		S32 delta_seconds = retry_after.asInteger();
		if (delta_seconds > 0)
		{
			// ...valid delta-seconds
			return (F64)delta_seconds;
		}
	}

	// If no Retry-After, look for Cache-Control max-age
	// Allow the header to override the default
	LLSD cache_control_header = headers["cache-control"];
	if (cache_control_header.isDefined())
	{
		S32 max_age = 0;
		std::string cache_control = cache_control_header.asString();
		if (maxAgeFromCacheControl(cache_control, &max_age))
		{
			llwarns << "Got EXPIRES from headers, max_age = " << max_age
					<< llendl;
			return (F64)max_age;
		}
	}

	// No information in header, make a guess
	if (status == 503)
	{
		// ...service unavailable, retry soon
		constexpr F64 SERVICE_UNAVAILABLE_DELAY = 600.0; // 10 min
		return SERVICE_UNAVAILABLE_DELAY;
	}
	else if (status == 499)
	{
		// ...we were probably too busy, retry quickly
		constexpr F64 BUSY_DELAY = 10.0; // 10 seconds
		return BUSY_DELAY;
	}
	else
	{
		// ...other unexpected error
		constexpr F64 DEFAULT_DELAY = 3600.0; // 1 hour
		return DEFAULT_DELAY;
	}
}

bool LLExperienceCache::maxAgeFromCacheControl(const std::string& cache_control,
											   S32* max_age)
{
	// Split the string on "," to get a list of directives
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	tokenizer directives(cache_control, COMMA_SEPARATOR);

	for (tokenizer::iterator token_it = directives.begin();
		 token_it != directives.end(); ++token_it)
	{
		// Tokens may have leading or trailing whitespace
		std::string token = *token_it;
		LLStringUtil::trim(token);
		if (token.compare(0, MAX_AGE.size(), MAX_AGE) == 0)
		{
			// this token starts with max-age, so let's chop it up by "="
			tokenizer subtokens(token, EQUALS_SEPARATOR);
			tokenizer::iterator subtoken_it = subtokens.begin();

			// Must have a token
			if (subtoken_it == subtokens.end()) return false;
			std::string subtoken = *subtoken_it;

			// Must exactly equal "max-age"
			LLStringUtil::trim(subtoken);
			if (subtoken != MAX_AGE) return false;

			// Must have another token
			if (++subtoken_it == subtokens.end()) return false;
			subtoken = *subtoken_it;

			// Must be a valid integer
			// *NOTE: atoi() returns 0 for invalid values, so we have to check
			// the string first.
			// *TODO: Do servers ever send "0000" for zero ?  We don't handle
			// it
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

//static
void LLExperienceCache::setLookupURL(const std::string& lookup_url)
{
	sLookupURL = lookup_url;
	if (!sLookupURL.empty())
	{
		sLookupURL += "id/";
	}
}

//static
bool LLExperienceCache::FilterWithProperty(const LLSD& experience, S32 prop)
{
	return (experience[PROPERTIES].asInteger() & prop) != 0;
}

//static
bool LLExperienceCache::FilterWithoutProperties(const LLSD& experience, S32 prop)
{
	return (experience[PROPERTIES].asInteger() & prop) == prop;
}

//static
bool LLExperienceCache::FilterWithoutProperty(const LLSD& experience, S32 prop)
{
	return (experience[PROPERTIES].asInteger() & prop) == 0;
}

//static
bool LLExperienceCache::FilterMatching(const LLSD& experience,
									   const LLUUID& id)
{
	if (experience.isUUID())
	{
		return experience.asUUID() == id;
	}
	return experience[EXPERIENCE_ID].asUUID() == id;
}
