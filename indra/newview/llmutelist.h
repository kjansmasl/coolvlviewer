/**
 * @file llmutelist.h
 * @brief Management of list of muted players
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_MUTELIST_H
#define LL_MUTELIST_H

#include "boost/signals2.hpp"

#include "llextendedstatus.h"
#include "hbfastmap.h"
#include "llstring.h"
#include "lluuid.h"

class LLMessageSystem;
class LLMuteListObserver;
class LLViewerObject;

// An entry in the mute list.
class LLMute
{
public:
	// Legacy mutes are BY_NAME and have null UUID.
	enum EType { BY_NAME = 0, AGENT = 1, OBJECT = 2, GROUP = 3, COUNT = 4 };

	// Bits in the mute flags. For backwards compatibility (since any mute list
	// entries that were created before the flags existed will have a flags
	// field of 0), the flags are "inverted" in the stored mute entry.
	// Note that it's possible, through flags, to completely disable an entry in
	// the mute list.
	// *TODO (?): consider adding partial mute flags for inventory offers,
	// script dialogs, Av rendering, TP offers, etc and then restore the
	// equivalence of flagAll = full mute ?
	enum
	{
		flagTextChat		= 0x00000001,	// If set do not mute text chat
		flagVoiceChat		= 0x00000002,	// If set do not mute voice chat
		flagParticles		= 0x00000004,	// If set do not mute particles
		flagObjectSounds	= 0x00000008,	// If set mute object sounds

		flagAll				= 0x0000000F,	// Mask of all defined flags

		flagPartialMute 	= 0x00000010	// Set when any of the above flags
											// is in use to differenciate
											// partial mutes from full mutes
											// and especially when an entry got
											// all partial mute flags set
											// (which is still not a full mute
											// but would appear as one in the
											// stored mask without
											// flagPartialMute).
	};

	static char CHAT_SUFFIX[];
	static char VOICE_SUFFIX[];
	static char PARTICLES_SUFFIX[];
	static char SOUNDS_SUFFIX[];

	static char BY_NAME_SUFFIX[];
	static char AGENT_SUFFIX[];
	static char OBJECT_SUFFIX[];
	static char GROUP_SUFFIX[];

	LLMute(const LLUUID& id, const std::string& name = std::string(),
		   EType type = BY_NAME, U32 flags = 0);

	// Returns name + suffix based on type
	// For example:  "James Tester (resident)"
	std::string getNameAndType() const;

	// Converts an entry name in the UI scroll list into just the agent or
	// object name. For example: "James Tester (resident)" sets the name to
	// "James Tester" and the type to AGENT.
	void setFromDisplayName(const std::string& entry_name);

public:
	LLUUID		mID;	// agent or object id
	std::string	mName;	// agent or object name
	EType		mType;	// needed for UI display of existing mutes
	U32			mFlags;	// flags pertaining to this mute entry
};

// Purely static class
class LLMuteList
{
	friend class LLDispatchEmptyMuteList;

	LLMuteList() = delete;
	~LLMuteList() = delete;

protected:
	LOG_CLASS(LLMuteList);

public:
	// reasons for auto-unmuting a resident
	enum EAutoReason
	{
		AR_IM = 0,			// agent IMed a muted resident
		AR_MONEY = 1,		// agent paid L$ to a muted resident
		AR_INVENTORY = 2,	// agent offered inventory to a muted resident
		AR_COUNT			// enum count
	};

	static void initClass();
	static void shutDownClass();

	static void addObserver(LLMuteListObserver* observer);
	static void removeObserver(LLMuteListObserver* observer);

	// Add either a normal or a BY_NAME mute, for any or all properties.
	static bool add(const LLMute& mute, U32 flags = 0);

	// Remove both normal and legacy mutes, for any or all properties.
	static bool remove(const LLMute& mute, U32 flags = 0);
	static bool autoRemove(const LLUUID& agent_id,
						   const EAutoReason reason,
						   const std::string& first_name = LLStringUtil::null,
						   const std::string& last_name = LLStringUtil::null);

	// Name is required to test against legacy text-only mutes.
	static bool isMuted(const LLUUID& id,
						const std::string& name = LLStringUtil::null,
						U32 flags = 0,
						LLMute::EType type = LLMute::COUNT);

	// Alternate (convenience) form for places we do not need to pass the name,
	// but do need flags
	LL_INLINE static bool isMuted(const LLUUID& id, U32 flags)
	{
		return isMuted(id, LLStringUtil::null, flags);
	}

	static S32 getMuteFlags(const LLUUID& id, std::string& description);

	static bool isLinden(const std::string& name);

	static LL_INLINE bool isLoaded()				{ return sIsLoaded; }

	static std::vector<LLMute> getMutes();

	// request the mute list
	static void requestFromServer();

	// call this method on logout to save everything.
	static void cache(bool force = false);

	static void setSavedResidentVolume(const LLUUID& id, F32 volume);
	static F32 getSavedResidentVolume(const LLUUID& id);

private:
	static void loadUserVolumes();

	static void loadPerAccountMuteList();
	static void savePerAccountMuteList();

	static bool loadFromFile(const std::string& filename);
	static bool saveToFile(const std::string& filename);

	static void setLoaded();
	static void notifyObservers();

	static void onRegionBoundaryCrossed();
	static void getGodsNames();

	static void updateAdd(const LLMute& mute);
	static void updateRemove(const LLMute& mute);

	static std::string getCachedMuteFilename();

	static void processMuteListUpdate(LLMessageSystem* msg, void**);
	static void processUseCachedMuteList(LLMessageSystem* msg, void**);

	static void onFileMuteList(void** user_data, S32 code,
							   LLExtStat ext_status);

	struct compare_by_name
	{
		LL_INLINE bool operator()(const LLMute& a, const LLMute& b) const
		{
			return a.mName < b.mName;
		}
	};

	struct compare_by_id
	{
		LL_INLINE bool operator()(const LLMute& a, const LLMute& b) const
		{
			return a.mID < b.mID;
		}
	};

private:
	static bool							sIsLoaded;
	static bool							sUserVolumesLoaded;

	static boost::signals2::connection	sRegionBoundaryCrossingSlot;
	static boost::signals2::connection	sSimFeaturesReceivedSlot;

	typedef std::set<LLMute, compare_by_id> mute_set_t;
	static mute_set_t					sMutes;

	typedef std::set<std::string> string_set_t;
	static string_set_t					sLegacyMutes;

	typedef std::set<LLMuteListObserver*> observer_set_t;
	static observer_set_t				sObservers;

	static std::set<std::string>		sGodLastNames;
	static std::set<std::string>		sGodFullNames;

	typedef fast_hmap<LLUUID, F32> user_volume_map_t;
	static user_volume_map_t			sUserVolumeSettings;
};

class LLMuteListObserver
{
public:
	virtual ~LLMuteListObserver() = default;
	virtual void onChange() = 0;
};

#endif	// LL_MUTELIST_H
