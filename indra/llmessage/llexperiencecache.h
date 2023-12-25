/**
 * @file llexperiencecache.h
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

#ifndef LL_LLEXPERIENCECACHE_H
#define LL_LLEXPERIENCECACHE_H

#include "boost/function.hpp"
#include "boost/signals2.hpp"

#include "llcorehttputil.h"
#include "hbfastmap.h"
#include "llframetimer.h"
#include "llsingleton.h"
#include "llsd.h"
#include "lluuid.h"

class LLUUID;

class LLExperienceCache final : public LLSingleton<LLExperienceCache>
{
	friend class LLSingleton<LLExperienceCache>;

protected:
	LOG_CLASS(LLExperienceCache);

public:
	typedef boost::function<const std::string&(const char*)> cap_query_fn_t;
	LL_INLINE void setCapabilityQuery(cap_query_fn_t queryfn)
	{
    	mCapability = queryfn;
	}

	void importFile(std::istream& istr);
	void exportFile(std::ostream& ostr) const;

	void cleanup();

	void erase(const LLUUID& key);
	bool fetch(const LLUUID& key, bool refresh = false);
	void insert(const LLSD& experience_data);
	const LLSD& get(const LLUUID& key);

	// If name information is in cache, callback will be called immediately.
	typedef boost::function<void(const LLSD&)> experience_get_fn_t;
	void get(const LLUUID& key, experience_get_fn_t slot);

	bool isRequestPending(const LLUUID& public_key);

	void fetchAssociatedExperience(const LLUUID& object_id,
								   const LLUUID& item_id,
								   const std::string& cap_url,
								   experience_get_fn_t fn);
	void findExperienceByName(std::string text, int page,
							  experience_get_fn_t fn);
	void getGroupExperiences(const LLUUID& group_id, experience_get_fn_t fn);

	// The get/set Region Experiences take a CapabilityQuery to get the
	// capability since  the region being queried may not be the region that
	// the agent is standing on.
	void getRegionExperiences(cap_query_fn_t regioncaps,
							  experience_get_fn_t fn);
	void setRegionExperiences(cap_query_fn_t regioncaps, const LLSD& exp,
							  experience_get_fn_t fn);

	void getExperiencePermission(const LLUUID& exp_id, experience_get_fn_t fn);
	void setExperiencePermission(const LLUUID& exp_id, const std::string& perm,
								 experience_get_fn_t fn);
	void forgetExperiencePermission(const LLUUID& exp_id,
									experience_get_fn_t fn);

	void getExperienceAdmin(const LLUUID& exp_id, experience_get_fn_t fn);

	void updateExperience(LLSD update_data, experience_get_fn_t fn);

	static void setLookupURL(const std::string& lookup_url);

	static bool FilterWithProperty(const LLSD& experience, S32 prop);
	static bool FilterWithoutProperty(const LLSD& experience, S32 prop);
	static bool FilterWithoutProperties(const LLSD& experience, S32 prop);
	static bool FilterMatching(const LLSD& experience, const LLUUID& id);

private:
	LLExperienceCache();
	~LLExperienceCache() override;

	void initSingleton() override;

	typedef boost::function<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t& adapter,
							std::string)> permissionInvoker_fn;

	void processExperience(const LLUUID& public_key, const LLSD& experience);

	void idleCoro();
	void eraseExpired();

	typedef LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter_ptr_t;
	void requestExperiencesCoro(adapter_ptr_t& adapter, std::string url,
								uuid_list_t requests);
	void requestExperiences();

	void fetchAssociatedExperienceCoro(adapter_ptr_t& adapter,
									   LLUUID object_id, LLUUID item_id,
									   std::string url,
									   experience_get_fn_t fn);
    void findExperienceByNameCoro(adapter_ptr_t& adapter, std::string text,
								  int page, experience_get_fn_t fn);
    void getGroupExperiencesCoro(adapter_ptr_t& adapter, LLUUID group_id,
								 experience_get_fn_t fn);
    void regionExperiencesCoro(adapter_ptr_t& adapter,
							   cap_query_fn_t regioncaps, bool update,
							   LLSD experiences, experience_get_fn_t fn);
    void experiencePermissionCoro(adapter_ptr_t& adapter,
								  permissionInvoker_fn invokerfn,
								  std::string url, experience_get_fn_t fn);
    void getExperienceAdminCoro(adapter_ptr_t& adapter, LLUUID exp_id,
								experience_get_fn_t fn);
    void updateExperienceCoro(adapter_ptr_t& adapter, LLSD updateData,
							  experience_get_fn_t fn);

#if 0	// Not used
	// maps an experience private key to the experience id
	LLUUID getExperienceId(const LLUUID& private_key,
						   bool null_if_not_found = false);
	void bootstrap(const LLSD& legacy_keys, S32 initial_expiration);
	static void mapKeys(const LLSD& legacy_keys);
#endif

	typedef fast_hmap<LLUUID, LLSD> cache_t;

	LL_INLINE const cache_t& getCached()
	{
		return mCache;
	}

	LL_INLINE friend std::ostream& operator<<(std::ostream& os,
											  const LLExperienceCache& cache)
	{
		cache.exportFile(os);
		return os;
	}

	LL_INLINE friend std::istream& operator>>(std::istream& is,
											  LLExperienceCache& cache)
	{
		cache.importFile(is);
		return is;
	}

	static F64 getErrorRetryDeltaTime(S32 status, LLSD headers);
	static bool maxAgeFromCacheControl(const std::string& cache_control,
									   S32* max_age);

public:
	static const std::string MISSING;

	static const std::string AGENT_ID;
	static const std::string GROUP_ID;
	static const std::string PRIVATE_KEY;
	static const std::string EXPERIENCE_ID;
	static const std::string NAME;
	static const std::string PROPERTIES;
	static const std::string EXPIRES;
	static const std::string DESCRIPTION;
	static const std::string QUOTA;
	static const std::string MATURITY;
	static const std::string METADATA;
	static const std::string SLURL;

	static constexpr S32 PROPERTY_INVALID		= 1 << 0;
	static constexpr S32 PROPERTY_PRIVILEGED	= 1 << 3;
	static constexpr S32 PROPERTY_GRID 			= 1 << 4;
	static constexpr S32 PROPERTY_PRIVATE		= 1 << 5;
	static constexpr S32 PROPERTY_DISABLED		= 1 << 6;
	static constexpr S32 PROPERTY_SUSPENDED		= 1 << 7;

private:
	typedef boost::signals2::signal<void(const LLSD& exp)> callback_signal_t;
	typedef std::shared_ptr<callback_signal_t> signal_ptr;
	// May have multiple callbacks for a single ID, which are represented as
	// multiple slots bound to the signal. Avoid copying signals via pointers.
	typedef fast_hmap<LLUUID, signal_ptr> signal_map_t;
	signal_map_t				mSignalMap;

	cache_t						mCache;

	uuid_list_t					mRequestQueue;

	typedef fast_hmap<LLUUID, F64> pending_map_t;
	pending_map_t				mPendingQueue;

	// To periodically clean out expired entries from the cache
	LLFrameTimer				mEraseExpiredTimer;

	cap_query_fn_t				mCapability;

	LLCore::HttpOptions::ptr_t	mHttpOptions;
	LLCore::HttpHeaders::ptr_t	mHttpHeaders;

#if 0	// Not used
	typedef fast_hmap<LLUUID, LLUUID> key_map_t;
	static key_map_t			sPrivateToPublicKeyMap;
#endif

	static std::string			sLookupURL;

	static constexpr  F64		DEFAULT_EXPIRATION	= 600.0;
	static constexpr  S32		DEFAULT_QUOTA		= 128;		// In megabytes
	static constexpr  S32		SEARCH_PAGE_SIZE	= 30;

	static bool					sShutdown;
};

#endif // LL_LLEXPERIENCECACHE_H
