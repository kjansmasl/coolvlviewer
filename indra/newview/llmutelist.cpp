/**
 * @file llmutelist.cpp
 * @author Richard Nelson, James Cook
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

#include "llviewerprecompiledheaders.h"

#include "llmutelist.h"

#include "llcachename.h"
#include "llcrc.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "llxfermanager.h"
#include "llmessage.h"

#include "llagent.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llfloaterim.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For gGenericDispatcher
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"
#include "llworld.h"				// For particle system banning

namespace
{
	// This method is used to return an object to mute given an object id.
	// It is used by the LLMute constructor and LLMuteList::isMuted.
	LLViewerObject* get_object_to_mute_from_id(LLUUID object_id)
	{
		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (objectp && !objectp->isAvatar())
		{
			LLViewerObject* parentp = objectp->getRootEdit();
			if (parentp)
			{
				objectp = parentp;
			}
		}
		return objectp;
	}
}

// "emptymutelist"
class LLDispatchEmptyMuteList final : public LLDispatchHandler
{
protected:
	LOG_CLASS(LLDispatchEmptyMuteList);

public:
	bool operator()(const LLDispatcher* dispatcher, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override
	{
		LLMuteList::setLoaded();
		llinfos << "Mute list dispatched." << llendl;
		return true;
	}
};

static LLDispatchEmptyMuteList sDispatchEmptyMuteList;

const char EMPTY_NAME[] = "*[<empty name>]*";

//-----------------------------------------------------------------------------
// LLMute class
//-----------------------------------------------------------------------------

char LLMute::CHAT_SUFFIX[] = " (chat)";
char LLMute::VOICE_SUFFIX[] = " (voice)";
char LLMute::PARTICLES_SUFFIX[] = " (particles)";
char LLMute::SOUNDS_SUFFIX[] = " (sounds)";

char LLMute::BY_NAME_SUFFIX[] = " (by name)";
char LLMute::AGENT_SUFFIX[] = " (resident)";
char LLMute::OBJECT_SUFFIX[] = " (object)";
char LLMute::GROUP_SUFFIX[] = " (group)";

LLMute::LLMute(const LLUUID& id, const std::string& name, EType type,
			   U32 flags)
:	mID(id),
	mName(name),
	mType(type),
	mFlags(flags)
{
	if (id.isNull())
	{
		mType = BY_NAME;
		return;
	}

	// Muting is done on root objects only, so get the root of this object
	LLViewerObject* mute_object = get_object_to_mute_from_id(id);
	if (mute_object && mute_object->getID() != id)
	{
		mID = mute_object->getID();
		if (mute_object->isAvatar())
		{
			mType = AGENT;
			LLNameValue* firstname = mute_object->getNVPair("FirstName");
			LLNameValue* lastname = mute_object->getNVPair("LastName");
			if (firstname && lastname)
			{
				mName.assign(firstname->getString());
				mName.append(" ");
				mName.append(lastname->getString());
			}
		}
		else
		{
			mType = OBJECT;
		}
	}
	if (mType == AGENT)
	{
		if (mName.empty())
		{
			std::string fullname;
			if (gCacheNamep && gCacheNamep->getFullName(id, fullname))
			{
				mName = fullname;
			}
		}
		else if (mName.find(" ") == std::string::npos)
		{
			// Residents must always appear with their legacy name in the mute
			// list
			mName += " Resident";
		}
	}
	else if (mName.empty())
	{
		if (mType == GROUP)
		{
			std::string groupname;
			if (gCacheNamep && gCacheNamep->getGroupName(id, groupname))
			{
				mName = groupname;
			}
		}
		else if (mType == OBJECT)
		{
			mName = "Object";
		}
	}
}

std::string LLMute::getNameAndType() const
{
	std::string name_with_suffix = mName;
	switch (mType)
	{
		case BY_NAME:
		default:
			name_with_suffix += BY_NAME_SUFFIX;
			break;
		case AGENT:
			name_with_suffix += AGENT_SUFFIX;
			break;
		case OBJECT:
			name_with_suffix += OBJECT_SUFFIX;
			break;
		case GROUP:
			name_with_suffix += GROUP_SUFFIX;
			break;
	}
	if (mFlags != 0)
	{
		if (~mFlags & flagTextChat)
		{
			name_with_suffix += CHAT_SUFFIX;
		}
		if (~mFlags & flagVoiceChat)
		{
			name_with_suffix += VOICE_SUFFIX;
		}
		if (~mFlags & flagObjectSounds)
		{
			name_with_suffix += SOUNDS_SUFFIX;
		}
		if (~mFlags & flagParticles)
		{
			name_with_suffix += PARTICLES_SUFFIX;
		}
	}
	return name_with_suffix;
}

void LLMute::setFromDisplayName(const std::string& entry_name)
{
	size_t pos = 0;
	mName = entry_name;

	pos = mName.rfind(GROUP_SUFFIX);
	if (pos != std::string::npos)
	{
		mName.erase(pos);
		mType = GROUP;
		return;
	}

	pos = mName.rfind(OBJECT_SUFFIX);
	if (pos != std::string::npos)
	{
		mName.erase(pos);
		mType = OBJECT;
		return;
	}

	pos = mName.rfind(AGENT_SUFFIX);
	if (pos != std::string::npos)
	{
		mName.erase(pos);
		mType = AGENT;
		return;
	}

	pos = mName.rfind(BY_NAME_SUFFIX);
	if (pos != std::string::npos)
	{
		mName.erase(pos);
		mType = BY_NAME;
		return;
	}

	llwarns << "Unable to set mute from entry: " << entry_name << llendl;
}

//-----------------------------------------------------------------------------
// LLMuteList class (purely static class)
//-----------------------------------------------------------------------------

// static member variables
bool LLMuteList::sIsLoaded = false;
bool LLMuteList::sUserVolumesLoaded = false;
boost::signals2::connection LLMuteList::sRegionBoundaryCrossingSlot;
boost::signals2::connection LLMuteList::sSimFeaturesReceivedSlot;
std::set<std::string> LLMuteList::sGodLastNames;
std::set<std::string> LLMuteList::sGodFullNames;
LLMuteList::mute_set_t LLMuteList::sMutes;
LLMuteList::string_set_t LLMuteList::sLegacyMutes;
LLMuteList::observer_set_t LLMuteList::sObservers;
LLMuteList::user_volume_map_t LLMuteList::sUserVolumeSettings;

//static
void LLMuteList::initClass()
{
	gGenericDispatcher.addHandler("emptymutelist", &sDispatchEmptyMuteList);
	// Register our callbacks
	gMessageSystemp->setHandlerFuncFast(_PREHASH_MuteListUpdate,
									   processMuteListUpdate);
	gMessageSystemp->setHandlerFuncFast(_PREHASH_UseCachedMuteList,
									   processUseCachedMuteList);
}

//static
void LLMuteList::shutDownClass()
{
	if (sSimFeaturesReceivedSlot.connected())
	{
		sSimFeaturesReceivedSlot.disconnect();
	}

	if (sRegionBoundaryCrossingSlot.connected())
	{
		sRegionBoundaryCrossingSlot.disconnect();
	}

	if (gDirUtilp && sUserVolumesLoaded)
	{
		std::string user_dir = gDirUtilp->getLindenUserDir();
		std::string filename =
			gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
										   "volume_settings.xml");
		LLSD settings_llsd;

		for (user_volume_map_t::iterator iter = sUserVolumeSettings.begin(),
										 end = sUserVolumeSettings.end();
			 iter != end; ++iter)
		{
			settings_llsd[iter->first.asString()] = iter->second;
		}

		llofstream file(filename.c_str());
		if (file.is_open())
		{
			LLSDSerialize::toPrettyXML(settings_llsd, file);
			file.close();
			llinfos << "User volumes saved" << llendl;
		}
		else
		{
			llwarns << "Could not open file '" << filename << "' for writing."
					<< llendl;
		}
	}
}

// Call once, after LLDir::setLindenUserDir() has been called
//static
void LLMuteList::loadUserVolumes()
{
	if (sUserVolumesLoaded)
	{
		return;
	}
	sUserVolumesLoaded = true;

	// Load per-resident voice volume information. Conceptually, this is part
	// of the mute list information, although it is only stored locally
	std::string filename =
		gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
									   "volume_settings.xml");
	LLSD settings_llsd;
	llifstream file(filename.c_str());
	if (file.is_open())
	{
		LLSDSerialize::fromXML(settings_llsd, file);
	}

	for (LLSD::map_const_iterator iter = settings_llsd.beginMap(),
								  end = settings_llsd.endMap();
		 iter != end; ++iter)
	{
		sUserVolumeSettings[LLUUID(iter->first)] = (F32)iter->second.asReal();
	}
}

//static
bool LLMuteList::isLinden(const std::string& name)
{
	if (sGodFullNames.count(name)) return true;

	if (sGodLastNames.empty()) return false;

	size_t i = name.rfind(' ');
	if (i == std::string::npos || i >= name.length() - 1)
	{
		// No space in name, or one space at the end of the name (the latter
		// should not happen, but let's be paranoid).
		return false;
	}

	return sGodLastNames.count(name.substr(i + 1)) != 0;
}

//static
bool LLMuteList::add(const LLMute& mute, U32 flags)
{
	// Cannot mute text from Lindens
	if (mute.mType == LLMute::AGENT && isLinden(mute.mName) &&
		(flags == 0 || (flags & LLMute::flagTextChat)))
	{
		gNotifications.add("MuteLinden");
		return false;
	}

	if (mute.mID.notNull())
	{
		LLViewerObject* vobj = gObjectList.findObject(mute.mID);
		if (mute.mID == gAgentID)
		{
			if (flags != LLMute::flagVoiceChat)
			{
				// Cannot mute self.
				gNotifications.add("MuteSelf");
				return false;
			}
		}
		else if (vobj && vobj->permYouOwner())
		{
			// Cannot mute our own objects
			gNotifications.add("MuteOwnObject");
			return false;
		}
	}

	size_t max_entries = gSavedSettings.getU32("MuteListLimit");
	if (sMutes.size() >= max_entries)
	{
		llwarns << "Mute list too large; new mute discarded." << llendl;
		LLSD args;
		args["MUTE_LIMIT"] = llformat("%d", (S32)max_entries);
		gNotifications.add("MuteLimitReached", args);
		return false;
	}

	if (mute.mType == LLMute::BY_NAME)
	{
		// Cannot mute empty string by name
		if (mute.mName.empty())
		{
			llwarns << "Trying to mute empty string by-name" << llendl;
			return false;
		}

		// Null mutes must have uuid null
		if (mute.mID.notNull())
		{
			llwarns << "Trying to add by-name mute with non-null id" << llendl;
			return false;
		}

		if (!isAgentAvatarValid())
		{
			return false;
		}

		std::string name;
		name.assign(gAgentAvatarp->getNVPair("FirstName")->getString());
		name.append(" ");
		name.append(gAgentAvatarp->getNVPair("LastName")->getString());
		if (mute.mName == name)
		{
			// Cannot mute self.
			gNotifications.add("MuteSelf");
			return false;
		}

		std::pair<string_set_t::iterator, bool> result =
			sLegacyMutes.emplace(mute.mName);
		if (result.second)
		{
			llinfos << "Muting by name " << mute.mName << llendl;
			updateAdd(mute);
			notifyObservers();
			cache(true);
			return true;
		}
		else
		{
			// Was a duplicate
			gNotifications.add("MuteByNameFailed");
			return false;
		}
	}
	else
	{
		// Need a local (non-const) copy to set up flags properly.
		LLMute localmute = mute;

		// If an entry for the same entity is already in the list, remove it,
		// saving flags as necessary.
		mute_set_t::iterator it = sMutes.find(localmute);
		if (it != sMutes.end())
		{
			// This mute is already in the list. Save the existing entry's
			// flags if that's warranted.
			localmute.mFlags = it->mFlags;

			sMutes.erase(it);
			// Do not need to call notifyObservers() here, since it will happen
			// after the entry has been re-added below.
		}
		else
		{
			// There was no entry in the list previously. Fake things up by
			// making it look like the previous entry had all properties
			// unmuted.
			localmute.mFlags = LLMute::flagAll;
		}

		if (flags)
		{
			// The user passed some combination of flags.
			// Make sure those flag bits are turned off (i.e. those properties
			// will be muted) and that mFlags will not be 0 (0 = full mute,
			// including things not covered by flags such as script dialogs,
			// inventory offers, avatar rendering, etc...).
			localmute.mFlags = LLMute::flagPartialMute | (localmute.mFlags & ~flags);
		}
		else
		{
			// The user passed 0. Make sure all flag bits are turned off (i.e.
			// all properties will be muted).
			localmute.mFlags = 0;
		}

		std::string trimed_name = localmute.mName;
		LLStringUtil::trimHead(trimed_name);
		if (trimed_name.empty())
		{
			// Do not pass an empty name (or a name with only spaces in it)
			// to the server because it is impossible to remove such mutes
			// (server bug) !  Since it is a mute by id, changing the name
			// is not an issue... We simply use a name that is unlikely to
			// correspond to anormal object name.
			localmute.mName.assign(EMPTY_NAME);
		}

		// (Re)add the mute entry.
		std::pair<mute_set_t::iterator, bool> result =
			sMutes.emplace(localmute);
		if (result.second)
		{
			llinfos << "Muting " << localmute.mName << " id "
					<< localmute.mID << " flags " << localmute.mFlags
					<< llendl;
			updateAdd(localmute);
			notifyObservers();
			if (!(localmute.mFlags & LLMute::flagParticles))
			{
				// Kill all particle systems owned by muted task
				if (localmute.mType == LLMute::AGENT)
				{
					gViewerPartSim.clearParticlesByOwnerID(localmute.mID);
				}
				else if (localmute.mType == LLMute::OBJECT)
				{
					gViewerPartSim.clearParticlesByRootObjectID(localmute.mID);
				}
			}
			cache(true);

			return true;
		}
	}

	// If we were going to return success, we would have done it by now.
	return false;
}

//static
void LLMuteList::updateAdd(const LLMute& mute)
{
	// Update the database
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_UpdateMuteListEntry);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addUUIDFast(_PREHASH_MuteID, mute.mID);
	msg->addStringFast(_PREHASH_MuteName, mute.mName);
	msg->addS32("MuteType", mute.mType);
	msg->addU32("MuteFlags", mute.mFlags);
	gAgent.sendReliableMessage();
}

//static
bool LLMuteList::remove(const LLMute& mute, U32 flags)
{
	bool removed = false;

	// First, remove from main list.
	mute_set_t::iterator it = sMutes.find(mute);
	if (it != sMutes.end())
	{
		LLMute localmute = *it;
		// When the caller did not pass any flag remove the entire entry:
		removed = true;

		if (flags)
		{
			// If the user passed mute flags, we may only want to change some flags.
			localmute.mFlags |= flags | LLMute::flagPartialMute;
			if (localmute.mFlags != (LLMute::flagAll | LLMute::flagPartialMute))
			{
				// Only some of the properties are masked out. Update the entry.
				removed = false;
			}
			else
			{
				localmute.mFlags = 0;
			}
		}

		// Always remove the entry from the set -- it will be re-added with new
		// flags if necessary.
		sMutes.erase(it);

		if (removed)
		{
			// The entry was actually removed. Notify the server.
			updateRemove(localmute);
			llinfos << "Unmuting " << localmute.mName << " id "
					<< localmute.mID << " flags " << localmute.mFlags
					<< llendl;
		}
		else
		{
			// Flags were updated, the mute entry needs to be retransmitted to
			// the server and re-added to the list.
			sMutes.emplace(localmute);
			updateAdd(localmute);
			llinfos << "Updating mute entry " << localmute.mName << " id "
					<< localmute.mID << " flags " << localmute.mFlags
					<< llendl;
		}
	}

	// Clean up any legacy mutes
	string_set_t::iterator legacy_it = sLegacyMutes.find(mute.mName);
	if (legacy_it != sLegacyMutes.end())
	{
		removed = true;

		// Database representation of legacy mute is UUID null.
		LLMute mute(LLUUID::null, *legacy_it, LLMute::BY_NAME);
		updateRemove(mute);
		sLegacyMutes.erase(legacy_it);
	}

	if (removed)
	{
		cache(true);
		notifyObservers();
	}

	return removed;
}

//static
void LLMuteList::updateRemove(const LLMute& mute)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_RemoveMuteListEntry);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addUUIDFast(_PREHASH_MuteID, mute.mID);
	msg->addString("MuteName", mute.mName);
	gAgent.sendReliableMessage();
}

void notify_automute_callback(const LLUUID& agent_id,
							  const std::string& full_name,
							  bool is_group, LLMuteList::EAutoReason reason)
{
	std::string notif_name;
	switch (reason)
	{
		default:
		case LLMuteList::AR_IM:
			notif_name = "AutoUnmuteByIM";
			break;
		case LLMuteList::AR_INVENTORY:
			notif_name = "AutoUnmuteByInventory";
			break;
		case LLMuteList::AR_MONEY:
			notif_name = "AutoUnmuteByMoney";
			break;
	}

	LLSD args;
	args["NAME"] = full_name;

	LLNotificationPtr notif_ptr = gNotifications.add(notif_name, args);
	if (notif_ptr)
	{
		std::string message = notif_ptr->getMessage();

		if (reason == LLMuteList::AR_IM)
		{
			LLFloaterIMSession* timp =
				LLFloaterIMSession::findInstance(agent_id);
			if (timp)
			{
				timp->addHistoryLine(message);
			}
		}

		LLChat auto_chat(message);
		LLFloaterChat::addChat(auto_chat, false, false);
	}
}

//static
bool LLMuteList::autoRemove(const LLUUID& agent_id,
							const EAutoReason reason,
							const std::string& first_name,
							const std::string& last_name)
{
	bool removed = false;

	if (isMuted(agent_id))
	{
		LLMute automute(agent_id, LLStringUtil::null, LLMute::AGENT);
		removed = true;
		remove(automute);

		std::string full_name = LLCacheName::buildFullName(first_name,
														   last_name);
		if (full_name.empty())
		{
			if (!gCacheNamep) return removed;	// Paranoia

			if (gCacheNamep->getFullName(agent_id, full_name))
			{
				// Name in cache, call callback directly
				notify_automute_callback(agent_id, full_name, false, reason);
			}
			else
			{
				// Not in cache, lookup name from cache
				gCacheNamep->get(agent_id, false,
								 boost::bind(&notify_automute_callback,
											 _1, _2, _3, reason));
			}
		}
		else
		{
			// Call callback directly
			notify_automute_callback(agent_id, full_name, false, reason);
		}
	}

	return removed;
}

//static
std::vector<LLMute> LLMuteList::getMutes()
{
	std::vector<LLMute> mutes;

	for (mute_set_t::const_iterator it = sMutes.begin(), end = sMutes.end();
		 it != end; ++it)
	{
		mutes.emplace_back(*it);
	}

	for (string_set_t::const_iterator it = sLegacyMutes.begin(),
									  end = sLegacyMutes.end();
		 it != end; ++it)
	{
		mutes.emplace_back(LLUUID::null, *it);
	}

	std::sort(mutes.begin(), mutes.end(), compare_by_name());
	return mutes;
}

//static
std::string LLMuteList::getCachedMuteFilename()
{
	std::string agent_id_string;
	gAgentID.toString(agent_id_string);

	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														  agent_id_string) +
						   ".cached_mute";
	return filename;
}

//static
bool LLMuteList::loadFromFile(const std::string& filename)
{
	if (!sRegionBoundaryCrossingSlot.connected())
	{
		onRegionBoundaryCrossed();
		sRegionBoundaryCrossingSlot =
			gAgent.addRegionChangedCB(boost::bind(&onRegionBoundaryCrossed));
	}
	if (filename.empty())
	{
		llwarns << "Mute list filename is empty !" << llendl;
		return false;
	}

	LLFILE* fp = LLFile::open(filename, "rb");
	if (!fp)
	{
		llwarns << "Could not open mute list " << filename << llendl;
		return false;
	}

	// *NOTE: Changing the size of these buffers will require changes
	// in the scanf below.
	char id_buffer[MAX_STRING];
	char name_buffer[MAX_STRING];
	char buffer[MAX_STRING];
	while (!feof(fp) && fgets(buffer, MAX_STRING, fp))
	{
		id_buffer[0] = '\0';
		name_buffer[0] = '\0';
		S32 type = 0;
		U32 flags = 0;
		sscanf(buffer, " %d %254s %254[^|]| %u\n",
			   &type, id_buffer, name_buffer, &flags);
		LLUUID id = LLUUID(id_buffer);

		// Work around for a server bug that prevents removing a mute entry
		// without a name... Just do not take these entries into account since
		// they are most likely stale ones (entries the user tried to remove
		// but that crept back in)... Our own code does not allow name-less
		// entries (EMPTY_NAME is used as the name for name-less objects muted
		// by id: see LLMute::add() for details).
		if (strlen(name_buffer) == 0)
		{
			llwarns << "Received a mute entry without a name from the server for id: "
					<< id << ". Ignoring..." << llendl;
			continue;
		}

		LLMute mute(id, std::string(name_buffer), (LLMute::EType)type, flags);
		if (mute.mID.isNull() || mute.mType == LLMute::BY_NAME)
		{
			sLegacyMutes.emplace(mute.mName);
		}
		else
		{
			sMutes.emplace(mute);
		}
	}
	LLFile::close(fp);
	setLoaded();

	llinfos << "Mute list loaded from file: " << filename << llendl;

	return true;
}

//static
bool LLMuteList::saveToFile(const std::string& filename)
{
	if (filename.empty())
	{
		llwarns << "Mute list filename is empty !" << llendl;
		return false;
	}

	LLFILE* fp = LLFile::open(filename, "wb");
	if (!fp)
	{
		llwarns << "Could not open mute list " << filename << llendl;
		return false;
	}

	// Legacy mutes have null uuid
	std::string id_string;
	LLUUID::null.toString(id_string);
	for (string_set_t::iterator it = sLegacyMutes.begin(),
								end = sLegacyMutes.end();
		 it != end; ++it)
	{
		fprintf(fp, "%d %s %s|\n", (S32)LLMute::BY_NAME, id_string.c_str(),
				it->c_str());
	}
	for (mute_set_t::iterator it = sMutes.begin(), end = sMutes.end();
		 it != end; ++it)
	{
		it->mID.toString(id_string);
		const std::string& name = it->mName;
		fprintf(fp, "%d %s %s|%u\n", (S32)it->mType, id_string.c_str(),
				name.c_str(), it->mFlags);
	}

	LLFile::close(fp);

	llinfos << "Mute list saved to file: " << filename << llendl;

	return true;
}

//static
bool LLMuteList::isMuted(const LLUUID& id, const std::string& name, U32 flags,
						 LLMute::EType type)
{
	if (id.notNull())
	{
		// For objects, check for muting on their parent prim
		LLViewerObject* mute_object = get_object_to_mute_from_id(id);
		LLUUID id_to_check = mute_object ? mute_object->getID() : id;
		LL_DEBUGS("Mute") << "Checking mute by id for object " << id
						  << " (parent object: " << id_to_check << ")..."
						  << LL_ENDL;
		// Do not need name or type for lookup
		LLMute mute(id_to_check);
		mute_set_t::const_iterator mute_it = sMutes.find(mute);
		if (mute_it != sMutes.end())
		{
			// If any of the flags the caller passed are set, this item is not
			// considered muted for this caller.
			if (flags & mute_it->mFlags)
			{
				LL_DEBUGS("Mute") << "The object is not muted for this mute type ("
								  << flags << ")" << LL_ENDL;
				return false;
			}
			// If the mute got flags and no flag was passed by the caller,
			// this item is not considered muted for this caller (for example
			// if we muted for chat only, we do not want the avatar to be
			// considered muted for the rest)
			if (flags == 0 &&  mute_it->mFlags != 0)
			{
				LL_DEBUGS("Mute") << "The object is not muted for everything."
								  << LL_ENDL;
				return false;
			}

			LL_DEBUGS("Mute") << "The object is muted." << LL_ENDL;
			return true;
		}
		LL_DEBUGS("Mute") << "The object is not muted by id..." << LL_ENDL;
	}

	// If no name was provided, we cannot proceed further
	if (name.empty())
	{
		LL_DEBUGS("Mute") << "The object is not muted." << LL_ENDL;
		return false;
	}
	LL_DEBUGS("Mute") << "Checking mute by name for: " << name << LL_ENDL;

	// The following checks are useful when we want to check for mutes
	// on something for which we do not have an UUID for, but that was
	// previously muted by UUID and not by name (legacy mute). This can
	// be used in some callbacks of llviewermessage.cpp, since not all
	// callbacks provide both object and avatar UUIDs, for example...
	// Note that partial mutes (mutes containing flags, such as flagVoiceChat
	// for example) will not be taken into account (we want to check for
	// full mutes only, here).
	if (type != LLMute::COUNT)
	{
		std::string name_and_type = name;
		switch (type)
		{
			case LLMute::AGENT:
			{
				LL_DEBUGS("Mute") << "Checking mute by name for AGENT '"
								  << name << "'" << LL_ENDL;
				if (name_and_type.find(" ") == std::string::npos)
				{
					// Residents always appear with their legacy name in the
					// mute list
					name_and_type += " Resident";
				}
				name_and_type += LLMute::AGENT_SUFFIX;
				break;
			}
			case LLMute::OBJECT:
			{
				LL_DEBUGS("Mute") << "Checking mute by name for OBJECT '"
								  << name << "'" << LL_ENDL;
				name_and_type += LLMute::OBJECT_SUFFIX;
				break;
			}
			case LLMute::GROUP:
			{
				LL_DEBUGS("Mute") << "Checking mute by name for GROUP '"
								  << name << "'" << LL_ENDL;
				name_and_type += LLMute::GROUP_SUFFIX;
				break;
			}
			case LLMute::BY_NAME:
			default:
			{
				LL_DEBUGS("Mute") << "Checking mute BY_NAME for '"
								  << name << "'" << LL_ENDL;
				name_and_type += LLMute::BY_NAME_SUFFIX;
			}
		}
		std::vector<LLMute> mutes = getMutes();
		for (std::vector<LLMute>::iterator it = mutes.begin(),
										   end = mutes.end();
			 it != end; ++it)
		{
			if (name_and_type == it->getNameAndType())
			{
				// If any of the flags the caller passed are set, this item
				// is not considered muted for this caller.
				if (flags & it->mFlags)
				{
					LL_DEBUGS("Mute") << "The object is not muted for this mute type ("
									  << flags << ")" << LL_ENDL;
					return false;
				}

				LL_DEBUGS("Mute") << "The object is muted." << LL_ENDL;
				return true;
			}
		}
	}

	// Agents and groups are always muted by Id and thus should never appear in
	// the legacy mutes (and we do not want to mute an agent or group whose
	// name would accidentally match an object muted by name...).
	if (type == LLMute::AGENT || type == LLMute::GROUP)
	{
		LL_DEBUGS("Mute") << "Non-muted "
						  << (type == LLMute::AGENT ? "AGENT" : "GROUP")
						  << LL_ENDL;
		return false;
	}
	else
	{
		// Look in legacy pile
		string_set_t::const_iterator legacy_it = sLegacyMutes.find(name);
		bool muted = legacy_it != sLegacyMutes.end();
		LL_DEBUGS("Mute") << "Legacy mutes check: "
						  << (muted ? "muted." : "not muted.")
						  << LL_ENDL;
		return muted;
	}
}

//static
S32 LLMuteList::getMuteFlags(const LLUUID& id, std::string& description)
{
	S32 flags = -1;			// Defaults to no mute
	description.clear();	// Empty description for no mute.

	if (id.notNull())
	{
		// For objects, check for muting on their parent prim
		LLViewerObject* mute_object = get_object_to_mute_from_id(id);
		LLUUID id_to_check  = (mute_object) ? mute_object->getID() : id;

		LLMute mute(id_to_check);
		mute_set_t::const_iterator mute_it = sMutes.find(mute);
		if (mute_it != sMutes.end())
		{
			flags = (S32)mute_it->mFlags;
			if (flags == 0)
			{
				description = "Muted";
			}
			else
			{
				flags = ~flags & LLMute::flagAll;

				if (flags & LLMute::flagTextChat)
				{
					description = 'C';
				}
				if (flags & LLMute::flagVoiceChat)
				{
					if (!description.empty())
					{
						description += '/';
					}
					description += 'V';
				}
				if (flags & LLMute::flagObjectSounds)
				{
					if (!description.empty())
					{
						description += '/';
					}
					description += 'S';
				}
				if (flags & LLMute::flagParticles)
				{
					if (!description.empty())
					{
						description += '/';
					}
					description += 'P';
				}
				description = "Muted (" + description + ")";
			}
		}
	}

	return flags;
}

//static
void LLMuteList::loadPerAccountMuteList()
{
	std::string filename =
		gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
									   "mute_list.txt");
	if (LLFile::exists(filename))
	{
		llinfos << "Loading per-account mute-list..." << llendl;
		loadFromFile(filename);
	}
}

//static
void LLMuteList::savePerAccountMuteList()
{
	std::string filename =
		gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT, "mute_list.txt");
	llinfos << "Saving per-account mute-list..." << llendl;
	saveToFile(filename);
}

//static
void LLMuteList::requestFromServer()
{
	loadUserVolumes();
	loadPerAccountMuteList();

	LLCRC crc;
	crc.update(getCachedMuteFilename());

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_MuteListRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addU32Fast(_PREHASH_MuteCRC, crc.getCRC());
	gAgent.sendReliableMessage(2);
}

//static
void LLMuteList::cache(bool force)
{
	// Write to disk even if empty.
	if (sIsLoaded || force)
	{
		saveToFile(getCachedMuteFilename());
		savePerAccountMuteList();
	}
}

//static
void LLMuteList::setSavedResidentVolume(const LLUUID& id, F32 volume)
{
	// Store new value in volume settings file
	sUserVolumeSettings[id] = volume;
}

//static
F32 LLMuteList::getSavedResidentVolume(const LLUUID& id)
{
	constexpr F32 DEFAULT_VOLUME = 0.5f;

	user_volume_map_t::iterator found_it = sUserVolumeSettings.find(id);
	if (found_it != sUserVolumeSettings.end())
	{
		return found_it->second;
	}
	// *FIXME: assumes default, should get this from somewhere
	return DEFAULT_VOLUME;
}

//static
void LLMuteList::processMuteListUpdate(LLMessageSystem* msg, void**)
{
	if (!gXferManagerp)
	{
		llwarns << "Transfer manager gone. Aborted." << llendl;
		return;
	}

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_MuteData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got a mute list update for the wrong agent." << llendl;
		return;
	}

	std::string unclean_filename;
	msg->getStringFast(_PREHASH_MuteData, _PREHASH_Filename, unclean_filename);
	std::string filename = LLDir::getScrubbedFileName(unclean_filename);

	llinfos << "Updating mute list from server..." << llendl;
	std::string* local_filename_and_path =
		new std::string(gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
													   filename));
	gXferManagerp->requestFile(*local_filename_and_path, filename,
							   LL_PATH_CACHE, msg->getSender(),
							   true, // make the remote file temporary.
							   onFileMuteList, (void**)local_filename_and_path,
							   LLXferManager::HIGH_PRIORITY);
}

//static
void LLMuteList::processUseCachedMuteList(LLMessageSystem* msg, void**)
{
	if (gSavedSettings.getBool("MuteListIgnoreServer"))
	{
		llinfos << "Server-provided (cached) mute list ignore, as per user setting."
				<< llendl;
	}
	else
	{
		llinfos << "Using cached mute list" << llendl;
		std::string filename = getCachedMuteFilename();
		loadFromFile(filename);
	}
}

//static
void LLMuteList::onFileMuteList(void** user_data, S32 error_code,
								LLExtStat ext_status)
{
	std::string* local_filename_and_path = (std::string*)user_data;
	if (local_filename_and_path && !local_filename_and_path->empty() &&
		error_code == 0)
	{
		if (gSavedSettings.getBool("MuteListIgnoreServer"))
		{
			llinfos << "Server-provided mute list ignore, as per user setting."
					<< llendl;
		}
		else
		{
			llinfos << "Loading server-provided mute list." << llendl;
			loadFromFile(*local_filename_and_path);
		}
		LLFile::remove(*local_filename_and_path);
	}
	else
	{
		llwarns << "Mute list not received." << llendl;
	}
	delete local_filename_and_path;
}

//static
void LLMuteList::addObserver(LLMuteListObserver* observer)
{
	sObservers.insert(observer);
}

//static
void LLMuteList::removeObserver(LLMuteListObserver* observer)
{
	sObservers.erase(observer);
}

//static
void LLMuteList::setLoaded()
{
	llinfos << "Mute list loaded." << llendl;
	sIsLoaded = true;
	notifyObservers();
}

//static
void LLMuteList::notifyObservers()
{
	for (observer_set_t::iterator it = sObservers.begin();
		 it != sObservers.end(); )
	{
		LLMuteListObserver* observer = *it;
		observer->onChange();
		// In case onChange() deleted an entry.
		it = sObservers.upper_bound(observer);
	}
}

//static
void LLMuteList::onRegionBoundaryCrossed()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		if (regionp->getFeaturesReceived())
		{
			if (sSimFeaturesReceivedSlot.connected())
			{
				sSimFeaturesReceivedSlot.disconnect();
			}
			getGodsNames();
		}
		else if (!sSimFeaturesReceivedSlot.connected())
		{
			sSimFeaturesReceivedSlot =
				regionp->setFeaturesReceivedCB(boost::bind(&getGodsNames));
		}
	}
}

//static
void LLMuteList::getGodsNames()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;

	sGodLastNames.clear();
	sGodFullNames.clear();

	const LLSD& info = regionp->getSimulatorFeatures();
	if (info.has("god_names"))
	{
		const LLSD& god_names = info["god_names"];

		if (god_names.has("last_names"))
		{
			const LLSD& names = god_names["last_names"];
			for (LLSD::array_const_iterator it = names.beginArray(),
											end = names.endArray();
				 it != end; ++it)
			{
				std::string name = it->asString();
				sGodLastNames.emplace(name);
				LL_DEBUGS("Mute") << "Added '" << name
								  << "' to the list of grid Gods' last name."
								  << LL_ENDL;
			}
		}

		if (god_names.has("full_names"))
		{
			const LLSD& names = god_names["full_names"];
			for (LLSD::array_const_iterator it = names.beginArray(),
											end = names.endArray();
				 it != end; ++it)
			{
				std::string name = it->asString();
				sGodFullNames.emplace(name);
				LL_DEBUGS("Mute") << "Added '" << name
								  << "' to the list of grid Gods." << LL_ENDL;
			}
		}
	}
	else
	{
		sGodLastNames.emplace("Linden");
	}
}
