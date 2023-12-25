/**
 * @file llavatarnamecache.h
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

#ifndef LLAVATARNAMECACHE_H
#define LLAVATARNAMECACHE_H

#include "hbfastmap.h"
#include "llcorehttputil.h"

class LLFrameTimer;

class LLAvatarName
{
public:
	LLAvatarName();

	bool operator<(const LLAvatarName& rhs) const;

	LLSD asLLSD() const;

	void fromLLSD(const LLSD& sd);

	// For normal names, returns "James Linden (james.linden)". When display
	// names are disabled returns just "James Linden"
	std::string getCompleteName() const;

	// For normal names, returns "Whatever Display Name [John Doe]" when
	// display name and legacy name are different, or just "John Doe" when they
	// are equal or when display names are disabled. When linefeed == true, the
	// space between the display name and the opening square bracket for the
	// legacy name is replaced with a line feed.
	std::string getNames(bool linefeed = false) const;

	// Returns "James Linden" or "bobsmith123 Resident" for backwards
	// compatibility with systems like voice and muting
	// Never omit "Resident" when full is true.
	std::string getLegacyName(bool full = false) const;

public:
	// "bobsmith123" or "james.linden", US-ASCII only
	std::string mUsername;

	// "Jose' Sanchez" or "James Linden", UTF-8 encoded Unicode. Contains
	// data whether or not user has explicitly set a display name; may
	// duplicate their username.
	std::string mDisplayName;

	// For "James Linden" -> "James"; for "bobsmith123" -> "bobsmith123".
	// Used to communicate with legacy systems like voice and muting which
	// rely on old-style names.
	std::string mLegacyFirstName;

	// For "James Linden" -> "Linden"; for "bobsmith123" -> "Resident".
	// See above for rationale
	std::string mLegacyLastName;

	// Under error conditions, we may insert "dummy" records with names like
	// "???" into caches as placeholders.  These can be shown in UI, but are
	// not serialized.
	bool mIsDisplayNameDefault;

	// Under error conditions, we may insert "dummy" records with names equal
	// to legacy name into caches as placeholders. These can be shown in UI,
	// but are not serialized.
	bool mIsTemporaryName;

	// Names can change, so need to keep track of when name was last checked.
	// Unix time-from-epoch seconds for efficiency
	F64 mExpires;

	// You can only change your name every N hours, so record when the next
	// update is allowed. Unix time-from-epoch seconds
	F64 mNextUpdate;

	// true to prevent the displaying of "Resident" as a last name in legacy
	// names
	static bool sOmitResidentAsLastName;

	// true to force the use of legacy names for friends (only used in newview/
	// but kept here for consistency with the above flag).
	static bool sLegacyNamesForFriends;

	// true to force the use of legacy names for speakers in IM and voice
	// panels (only used in newview/ but kept here for consistency with the
	// above flag).
	static bool sLegacyNamesForSpeakers;
};

// Purely static class
class LLAvatarNameCache
{
	LLAvatarNameCache() = delete;
	~LLAvatarNameCache() = delete;

protected:
	LOG_CLASS(LLAvatarNameCache);

public:
	typedef boost::signals2::signal<void(void)> use_display_name_signal_t;

	static void initClass();
	static void cleanupClass();

	static bool importFile(std::istream& istr);
	static void exportFile(std::ostream& ostr);

	// On the viewer, usually a simulator capabilitity. If empty, name cache
	// will fall back to using legacy name lookup system
	static void setNameLookupURL(const std::string& name_lookup_url);

	// Do we have a valid lookup URL, hence are we trying to use the new
	// display name lookup system?
	static bool hasNameLookupURL();

	// Maximum number of simultaneous HTTP requests
	static void setMaximumRequests(U32 num);

	// Periodically makes a batch request for display names not already in
	// cache. Called once per frame.
	static void idle();

	// If name is in cache, returns true and fills in provided LLAvatarName
	// otherwise returns false
	static bool get(const LLUUID& agent_id, LLAvatarName* av_name);

	// Callback types for get() below
	typedef boost::signals2::signal<void(const LLUUID& agent_id,
										 const LLAvatarName& av_name)>
			callback_signal_t;
	typedef callback_signal_t::slot_type callback_slot_t;
	typedef boost::signals2::connection callback_connection_t;

	// Fetches name information and calls callback. If name information is in
	// cache, callback will be called immediately.
	static callback_connection_t get(const LLUUID& agent_id,
									 callback_slot_t slot);

	// Set and get the display names usage policy.
	static void setUseDisplayNames(U32 use);
	static U32 useDisplayNames();

	static void erase(const LLUUID& agent_id);

    // Provide some fallback for agents that return errors
	static void handleAgentError(const LLUUID& agent_id);

	static void insert(const LLUUID& agent_id, const LLAvatarName& av_name);

	// Compute name expiration time from HTTP Cache-Control header, or return
	// default value, in seconds from epoch.
	static F64 nameExpirationFromHeaders(const LLSD& headers);

	static void addUseDisplayNamesCallback(const use_display_name_signal_t::slot_type& cb);

private:
	// Handle name response off network. Optionally skip adding to cache, used
	// when this is a fallback to the legacy name system.
	static void processName(const LLUUID& agent_id,
							const LLAvatarName& av_name, bool add_to_cache);

	static void requestNamesViaCapability();

	// Legacy name system callback
	static void legacyNameCallback(const LLUUID& agent_id,
								   const std::string& full_name,
								   bool is_group);

	static void requestNamesViaLegacy();

	// Fill in an LLAvatarName with the legacy name data
	static void buildLegacyName(const std::string& full_name,
								LLAvatarName* av_name);

	// Do a single callback to a given slot
	static void fireSignal(const LLUUID& agent_id, const callback_slot_t& slot,
						   const LLAvatarName& av_name);

	// Is a request in-flight over the network ?
	static bool isRequestPending(const LLUUID& agent_id);

	// Erase expired names from cache
	static void eraseUnrefreshed();

	static bool expirationFromCacheControl(const LLSD& headers, F64* expires);

	static void requestAvatarNameCacheCoro(const std::string& url,
										   uuid_vec_t agent_ids);
	static void handleAvNameCacheSuccess(const LLSD& data, F64 expires);

private:
	// Usage policy for display names: 0 = legacy names, 1 = display name and
	// legacy name, 2 = display name (legacy if absent)
	static U32 sUseDisplayNames;

	// Time when unrefreshed cached names were checked last
	static F64 sLastExpireCheck;

	// Let's not launch too many simultaneous coroutines when filling up large
	// lists (group member lists, for example):
	static S32 sPendingRequests;
	static S32 sMaximumRequests;

	// Base lookup URL for name service capability. Includes the trailing
	// slash, like "http://pdp60.lindenlab.com:8000/agents/"
	static std::string sNameLookupURL;

	static use_display_name_signal_t sUseDisplayNamesSignal;

	// Accumulated agent IDs for next query against service
	static uuid_list_t sAskQueue;

	// Agent IDs that have been requested, but with no reply maps agent ID to
	// frame time request was made.
	typedef fast_hmap<LLUUID, F64> pending_queue_t;
	static pending_queue_t sPendingQueue;

	// Callbacks to fire when we received a name. May have multiple callbacks
	// for a single ID, which are represented as multiple slots bound to the
	// signal. Avoid copying signals via pointers.
	typedef fast_hmap<LLUUID, callback_signal_t*> signal_map_t;
	static signal_map_t sSignalMap;

	// Names we know about
	typedef fast_hmap<LLUUID, LLAvatarName> cache_t;
	static cache_t sCache;

	// Send bulk lookup requests a few times a second at most only need per-
	// frame timing resolution
	static LLFrameTimer sRequestTimer;

	static LLCore::HttpRequest::ptr_t sHttpRequest;
	static LLCore::HttpHeaders::ptr_t sHttpHeaders;
	static LLCore::HttpOptions::ptr_t sHttpOptions;

	// Cache starts in a paused state until we can determine if the current
	// region supports display names.
	static bool sRunning;
};

#endif
