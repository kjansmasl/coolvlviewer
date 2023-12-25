/**
 * @file llavatarproperties.cpp
 * @brief Class for requesting and storing avatar properties.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2022, Linden Research, Inc.
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

#include "llavatarproperties.h"

#include "lldate.h"
#include "llcorehttputil.h"

#include "llagent.h"
#include "llappviewer.h"				// For gFrameTimeSeconds
#include "llviewermessage.h"			// For send_generic_message()
#include "llviewercontrol.h"

// Static member variables
LLAvatarProperties::observers_set_t LLAvatarProperties::sObservers;
LLAvatarProperties::pending_map_t LLAvatarProperties::sPendingRequests;

//static
void LLAvatarProperties::addPendingRequest(const LLUUID& id, S32 type)
{
	pending_map_t::key_type key = std::make_pair(id, type);
	sPendingRequests[key] = gFrameTimeSeconds;
}

//static
void LLAvatarProperties::removePendingRequest(const LLUUID& id, S32 type)
{
	pending_map_t::key_type key = std::make_pair(id, type);
	sPendingRequests.erase(key);
}

//static
bool LLAvatarProperties::isPendingRequest(const LLUUID& id, S32 type)
{
	pending_map_t::key_type key = std::make_pair(id, type);
	pending_map_t::iterator it = sPendingRequests.find(key);
	if (it == sPendingRequests.end())
	{
		return false;
	}
	constexpr F32 REQUEST_EXPIRE_SECS = 5.f;	// 5s timeout.
	return it->second + REQUEST_EXPIRE_SECS > gFrameTimeSeconds;
}

//static
void LLAvatarProperties::notifyObservers(const LLUUID& id, S32 type,
										 void* data)
{
	// This request is no more pending. Do this before calling observers, so
	// that they may relaunch a request immediately if needed, and it *is*
	// needed to recover the Interests data via UDP when using the capability
	// to get the main porperties: see LLPanelAvatar::processProperties(). HB
	if (type > APT_NONE)
	{
		removePendingRequest(id, type);
	}

	// Note: observers could remove themselves following a call to their
	// processProperties() method, so we need to build the list of the
	// observers to call *before* calling each observer method... HB
	std::vector<LLAvatarPropertiesObserver*> observers;
	for (observers_set_t::iterator it = sObservers.begin(),
								   end = sObservers.end();
		 it != end; ++it)
	{
		LLAvatarPropertiesObserver* obsp = *it;

		// Check if of the right observed update type.
		S32 update_type = obsp->getUpdateType();
		if (update_type == APT_NONE ||
			(update_type != APT_ALL && update_type != type))
		{
			continue;
		}

		// Add to the list of observers to call if the avatar Id matches.
		const LLUUID& observed_id = obsp->getAvatarId();
		if (observed_id.isNull() || observed_id == id)
		{
			observers.push_back(obsp);
		}
	}

	// Now, do call the interested observers.
	for (U32 i = 0, count = observers.size(); i < count; ++i)
	{
		observers[i]->processProperties(type, data);
	}
}

//static
void LLAvatarProperties::sendGenericRequest(const LLUUID& avatar_id, S32 type)
{
	if (type != APT_AVATAR_INFO && (type < APT_GROUPS || type > APT_NOTES))
	{
		llerrs << "Invalid request type: " << type << llendl;
	}

	if (isPendingRequest(avatar_id, type))
	{
		LL_DEBUGS("AvatarProperties") << "Skipping duplicate request type "
									  << type << " for avatar " << avatar_id
									  << LL_ENDL;
		return;
	}

	if (type != APT_CLASSIFIEDS &&
		gSavedSettings.getBool("UseAgentProfileCap"))
	{
		std::string url = gAgent.getRegionCapability("AgentProfile");
		if (!url.empty())
		{
			LL_DEBUGS("AvatarProperties") << "Using AgentProfile capability to retrieve data for avatar: "
										  << avatar_id << LL_ENDL;
			addPendingRequest(avatar_id, APT_GROUPS);
			addPendingRequest(avatar_id, APT_PICKS);
			addPendingRequest(avatar_id, APT_NOTES);
			addPendingRequest(avatar_id, APT_AVATAR_INFO);
			url += "/" + avatar_id.asString();
			gCoros.launch("requestAgentUserInfoCoro",
						  boost::bind(requestAvatarPropertiesCoro, avatar_id,
									  url));
			// Also request an agent groups list refresh for LLAgent. HB
			if (avatar_id == gAgentID)
			{
				gAgent.sendAgentDataUpdateRequest();
			}
			return;
		}
	}

	if (type == APT_AVATAR_INFO)
	{
		sendAvatarPropertiesRequest(avatar_id);
		return;
	}

	addPendingRequest(avatar_id, type);

	// Must match the order defined in EAvatarPropertiesUpdateType
	static const char* udp_methods[] =
	{
		"avatargroupsrequest",		// APT_GROUPS
		"avatarpicksrequest",		// APT_PICKS
		"avatarclassifiedsrequest",	// APT_CLASSIFIEDS
		"avatarnotesrequest",		// APT_NOTES
	};
	const char* method = udp_methods[type - APT_GROUPS];
	LL_DEBUGS("AvatarProperties") << "Sending UDP request \"" << method
								  << "\" for avatar: " << avatar_id << LL_ENDL;
	std::vector<std::string> params;
	params.emplace_back(avatar_id.asString());
	send_generic_message(method, params);
	// When requesting groups data for our agent, also request an agent groups
	// list refresh for LLAgent. HB
	if (type == APT_GROUPS && avatar_id == gAgentID)
	{
		gAgent.sendAgentDataUpdateRequest();
	}
}

//static
void LLAvatarProperties::requestAvatarPropertiesCoro(LLUUID avatar_id,
													 const std::string& url)
{
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setFollowRedirects(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("requestAvatarPropertiesCoro");
	LLSD result = adapter.getAndSuspend(url, options);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result.has("id") || result["id"].asUUID() != avatar_id)
	{
		llwarns << "Failed to retrieve data for avatar: " << avatar_id
				<< llendl;
		if (!status)
		{
			llwarns << "Error: " << status.toString() << llendl;
		}
		removePendingRequest(avatar_id, APT_GROUPS);
		removePendingRequest(avatar_id, APT_PICKS);
		removePendingRequest(avatar_id, APT_NOTES);
		removePendingRequest(avatar_id, APT_AVATAR_INFO);
		// *TODO: fall back to UDP methods ?
		return;
	}

	LL_DEBUGS("AvatarProperties") << "Received properties via capability for avatar: "
								  << avatar_id << LL_ENDL;

	// Generic avatar data
	LLAvatarInfo avatar_data;
	avatar_data.mReceivedViaCap = true;
	avatar_data.mAvatarId = avatar_id;
	avatar_data.mImageId = result["sl_image_id"].asUUID();
	avatar_data.mFLImageId = result["fl_image_id"].asUUID();
	avatar_data.mPartnerId = result["partner_id"].asUUID();
	avatar_data.mBirthDate = result["member_since"].asDate().asString();
	tm t;
	if (sscanf(avatar_data.mBirthDate.c_str(), "%u-%u-%u",
			   &t.tm_year, &t.tm_mon, &t.tm_mday) == 3 && t.tm_year > 1900)
	{
		t.tm_year -= 1900;
		--t.tm_mon;
		t.tm_hour = t.tm_min = t.tm_sec = 0;
		timeStructToFormattedString(&t,
									gSavedSettings.getString("ShortDateFormat"),
									avatar_data.mBirthDate);
	}
	avatar_data.mAbout = result["sl_about_text"].asString();
	avatar_data.mFLAbout = result["fl_about_text"].asString();
	// The Web URL is not provided by the new capability... HB
	avatar_data.mProfileUrl.clear();
	avatar_data.mFlags = 0;
	if (result["online"].asBoolean())
	{
		avatar_data.mFlags |= AVATAR_ONLINE;
	}
	if (result["allow_publish"].asBoolean())
	{
		avatar_data.mFlags |= AVATAR_ALLOW_PUBLISH;
		avatar_data.mAllowPublish = true;
	}
	else
	{
		avatar_data.mAllowPublish = false;
	}
	if (result["online"].asBoolean())
	{
		avatar_data.mFlags |= AVATAR_ONLINE;
	}
	if (result["identified"].asBoolean())
	{
		avatar_data.mFlags |= AVATAR_IDENTIFIED;
	}
	if (result["transacted"].asBoolean())
	{
		avatar_data.mFlags |= AVATAR_TRANSACTED;
	}
	if (result.has("charter_member")) // Not present when "caption" is set
	{
		avatar_data.mCaptionIndex = result["charter_member"].asInteger();
	}
	else if (result.has("caption"))
	{
		avatar_data.mCaptionText = result["caption"].asString();
		avatar_data.mCaptionIndex = 0;
	}
	notifyObservers(avatar_id, APT_AVATAR_INFO, (void*)&avatar_data);

	// Avatar picks
	LLAvatarPicks avatar_picks;
	avatar_picks.mReceivedViaCap = true;
	avatar_picks.mAvatarId = avatar_id;
	LLSD picks_array = result["picks"];
	for (LLSD::array_const_iterator it = picks_array.beginArray(),
									end = picks_array.endArray();
		 it != end; ++it)
	{
		const LLSD& pick_data = *it;
		avatar_picks.mMap.emplace(pick_data["id"].asUUID(),
								  pick_data["name"].asString());
	}
	notifyObservers(avatar_id, APT_PICKS, (void*)&avatar_picks);

	// Avatar groups
	LLAvatarGroups avatar_groups;
	avatar_groups.mAvatarId = avatar_id;
	LLSD groups_array = result["groups"];
	for (LLSD::array_const_iterator it = groups_array.beginArray(),
									end = groups_array.endArray();
		 it != end; ++it)
	{
		const LLSD& group_info = *it;
		LLGroupData group_data(group_info["id"].asUUID(),
							   group_info["name"].asString(), 0);
		group_data.mInsigniaID = group_info["image_id"].asUUID();
		avatar_groups.mGroups.emplace_back(group_data);
	}
	notifyObservers(avatar_id, APT_GROUPS, (void*)&avatar_groups);

	// Notes
	LLAvatarNotes avatar_notes;
	avatar_notes.mReceivedViaCap = true;
	avatar_notes.mAvatarId = avatar_id;
	avatar_notes.mNotes = result["notes"].asString();
	notifyObservers(avatar_id, APT_NOTES, (void*)&avatar_notes);
}

//static
void LLAvatarProperties::sendAvatarPropertiesRequest(const LLUUID& avatar_id)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	if (isPendingRequest(avatar_id, APT_AVATAR_INFO))
	{
		LL_DEBUGS("AvatarProperties") << "Skipping duplicate request for avatar "
									  << avatar_id << LL_ENDL;
		return;
	}
	addPendingRequest(avatar_id, APT_AVATAR_INFO);

	msg->newMessageFast(_PREHASH_AvatarPropertiesRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_AvatarID, avatar_id);
	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::processAvatarPropertiesReply(LLMessageSystem* msg,
													  void**)
{	
	if (!msg) return;	// Paranoia

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent ID mismatch. Got: " << agent_id << llendl;
		return;
	}

	LLAvatarInfo data;
	data.mReceivedViaCap = false;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AvatarID, data.mAvatarId);
	LL_DEBUGS("AvatarProperties") << "Received properties via UDP for avatar: "
								  << data.mAvatarId << LL_ENDL;
	msg->getUUIDFast(_PREHASH_PropertiesData, _PREHASH_ImageID, data.mImageId);
	msg->getUUIDFast(_PREHASH_PropertiesData, _PREHASH_FLImageID,
					 data.mFLImageId);
	msg->getUUIDFast(_PREHASH_PropertiesData, _PREHASH_PartnerID,
					 data.mPartnerId);
	msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_AboutText,
					   data.mAbout);
	msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_FLAboutText,
					   data.mFLAbout);
	msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_BornOn,
					   data.mBirthDate);
	tm t;
	if (sscanf(data.mBirthDate.c_str(), "%u/%u/%u",
			   &t.tm_mon, &t.tm_mday, &t.tm_year) == 3 && t.tm_year > 1900)
	{
		t.tm_year -= 1900;
		--t.tm_mon;
		t.tm_hour = t.tm_min = t.tm_sec = 0;
		timeStructToFormattedString(&t,
									gSavedSettings.getString("ShortDateFormat"),
									data.mBirthDate);
	}
	msg->getString(_PREHASH_PropertiesData, _PREHASH_ProfileURL,
				   data.mProfileUrl);
	msg->getU32Fast(_PREHASH_PropertiesData, _PREHASH_Flags, data.mFlags);

	data.mCaptionIndex = 0;
	data.mCaptionText.clear();
	S32 charter_member_size = msg->getSize(_PREHASH_PropertiesData,
										   _PREHASH_CharterMember);
	if (charter_member_size == 1)
	{
		msg->getBinaryData(_PREHASH_PropertiesData, _PREHASH_CharterMember,
						   &data.mCaptionIndex, 1);
	}
	else if (charter_member_size > 1)
	{
		msg->getString(_PREHASH_PropertiesData, _PREHASH_CharterMember,
					   data.mCaptionText);
	}

	notifyObservers(data.mAvatarId, APT_AVATAR_INFO, (void*)&data);
}

//static
void LLAvatarProperties::sendAvatarPropertiesUpdate(const LLAvatarInfo& data)
{
	constexpr size_t MAX_UDP_TEXT_SIZE = 510;
	bool try_cap = gSavedSettings.getBool("UseAgentProfileCap");
	bool large_sl_about = data.mAbout.size() > MAX_UDP_TEXT_SIZE;
	bool large_fl_about = data.mFLAbout.size() > MAX_UDP_TEXT_SIZE;
	if (!try_cap && (large_sl_about || large_fl_about))
	{
		llinfos << "Large About text detected; attempting to use the AgentProfile capability..."
				<< llendl;
		try_cap = true;
	}
	if (try_cap)
	{
		std::string url = gAgent.getRegionCapability("AgentProfile");
		if (!url.empty())
		{
			llinfos << "Using AgentProfile capability to update agent info"
					<< llendl;
			LLSD updates;
			updates["sl_about_text"] = data.mAbout;
			updates["fl_about_text"] = data.mFLAbout;
			updates["sl_image_id"] = data.mImageId;
			updates["fl_image_id"] = data.mFLImageId;
			updates["allow_publish"] = data.mAllowPublish;
			url += "/" + gAgentID.asString();
			gCoros.launch("sendAvatarPropertiesUpdateCoro",
						  boost::bind(sendAvatarPropertiesUpdateCoro,
									  updates, url));
			return;
		}
	}

	llinfos << "Using legacy UDP messaging to update agent info." << llendl;

	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	msg->newMessageFast(_PREHASH_AvatarPropertiesUpdate);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_PropertiesData);
	msg->addUUIDFast(_PREHASH_ImageID, data.mImageId);
	msg->addUUIDFast(_PREHASH_FLImageID, data.mFLImageId);
	std::string text = data.mAbout;
	if (large_sl_about)
	{
		llwarns << "Second Life 'About' text truncated to 510 characters."
				<< llendl;
		text.erase(MAX_UDP_TEXT_SIZE);
	}
	msg->addStringFast(_PREHASH_AboutText, text);
	text = data.mFLAbout;
	if (large_fl_about)
	{
		llwarns << "First Life 'About' text truncated to 510 characters."
				<< llendl;
		text.erase(MAX_UDP_TEXT_SIZE);
	}
	msg->addStringFast(_PREHASH_FLAboutText, text);
	msg->addBool(_PREHASH_AllowPublish, data.mAllowPublish);
	// A profile should never be mature
	msg->addBool(_PREHASH_MaturePublish, false);
	msg->addString(_PREHASH_ProfileURL, data.mProfileUrl);
	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::sendAvatarPropertiesUpdateCoro(LLSD data,
														const std::string& url)
{
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setFollowRedirects(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("sendAvatarPropertiesUpdateCoro");
	LLSD result = adapter.putAndSuspend(url, data, options);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Error: " << status.toString() << llendl;
	}
}

//static
void LLAvatarProperties::processAvatarGroupsReply(LLMessageSystem* msg, void**)
{
	if (!msg) return;	// Paranoia

	LL_DEBUGS("AvatarProperties") << "Groups packet size: "
								  << msg->getReceiveSize()
								  << LL_ENDL;

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent ID mismatch. Got: " << agent_id << llendl;
		return;
	}

	LLAvatarGroups groups;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AvatarID, groups.mAvatarId);

	S32 group_count = msg->getNumberOfBlocksFast(_PREHASH_GroupData);

	std::string	name;
	LLUUID group_id, insignia;
	U64 powers;
	for (S32 i = 0; i < group_count; ++i)
	{
		msg->getU64(_PREHASH_GroupData, _PREHASH_GroupPowers, powers, i);
#if 0	// Not used
		msg->getStringFast(_PREHASH_GroupData, _PREHASH_GroupTitle, title, i);
#endif
		msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id, i);
		msg->getStringFast(_PREHASH_GroupData, _PREHASH_GroupName, name, i);
		msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupInsigniaID,
						 insignia, i);
		if (group_id.isNull())
		{
			name.clear();
		}

		LLGroupData data(group_id, name, powers);
		data.mInsigniaID = insignia;

		groups.mGroups.emplace_back(data);
	}

	notifyObservers(groups.mAvatarId, APT_GROUPS, (void*)&groups);
}

//static
void LLAvatarProperties::processAvatarInterestsReply(LLMessageSystem* msg,
													 void**)
{
	if (!msg) return;	// Paranoia

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent ID mismatch. Got: " << agent_id << llendl;
		return;
	}

	LLAvatarInterests data;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AvatarID, data.mAvatarId);
	msg->getU32Fast(_PREHASH_PropertiesData, _PREHASH_WantToMask,
					data.mWantsMask);
	msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_WantToText,
					   data.mWantsText);
	msg->getU32Fast(_PREHASH_PropertiesData, _PREHASH_SkillsMask,
					data.mSkillsMask);
	msg->getStringFast(_PREHASH_PropertiesData, _PREHASH_SkillsText,
					   data.mSkillsText);
	msg->getString(_PREHASH_PropertiesData, _PREHASH_LanguagesText,
				   data.mLanguages);

	notifyObservers(data.mAvatarId, APT_INTERESTS, (void*)&data);
}

//static
void LLAvatarProperties::sendInterestsInfoUpdate(const LLAvatarInterests& data)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	llinfos << "Sending agent interests update" << llendl;

	msg->newMessage(_PREHASH_AvatarInterestsUpdate);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_PropertiesData);
	msg->addU32Fast(_PREHASH_WantToMask, data.mWantsMask);
	msg->addStringFast(_PREHASH_WantToText, data.mWantsText);
	msg->addU32Fast(_PREHASH_SkillsMask, data.mSkillsMask);
	msg->addStringFast(_PREHASH_SkillsText, data.mSkillsText);
	msg->addString(_PREHASH_LanguagesText, data.mLanguages);
	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::processAvatarPicksReply(LLMessageSystem* msg, void**)
{
	if (!msg) return;	// Paranoia

	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent ID mismatch. Got: " << agent_id << llendl;
		return;
	}

	LLAvatarPicks picks;
	picks.mReceivedViaCap = false;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_TargetID, picks.mAvatarId);

	LLUUID pick_id;
	std::string pick_name;
	S32 block_count = msg->getNumberOfBlocks(_PREHASH_Data);
	for (S32 i = 0; i < block_count; ++i)
	{
		msg->getUUID(_PREHASH_Data, _PREHASH_PickID, pick_id, i);
		msg->getString(_PREHASH_Data, _PREHASH_PickName, pick_name, i);
		picks.mMap.emplace(pick_id, pick_name);
	}

	notifyObservers(picks.mAvatarId, APT_PICKS, (void*)&picks);
}

//static
void LLAvatarProperties::sendPickInfoRequest(const LLUUID& avatar_id,
											 const LLUUID& pick_id)
{
	// We must ask for a pick based on the creator Id because the pick database
	// is distributed to the inventory cluster. JC
	std::vector<std::string> params;
	params.emplace_back(avatar_id.asString());
	params.emplace_back(pick_id.asString());
	send_generic_message("pickinforequest", params);
}

//static
void LLAvatarProperties::processPickInfoReply(LLMessageSystem* msg, void**)
{
	if (!msg) return;	// Paranoia

	// Extract the agent id and verify the message is for this client.
	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent ID mismatch. Got agent ID " << agent_id << llendl;
		return;
	}

	LLAvatarPickInfo data;
	msg->getUUID(_PREHASH_Data, _PREHASH_PickID, data.mPickId);
	msg->getUUID(_PREHASH_Data, _PREHASH_CreatorID, data.mAvatarId);
	// Legacy. Not used any more server-side.
	msg->getBool(_PREHASH_Data, _PREHASH_TopPick, data.mTopPick);
	msg->getUUID(_PREHASH_Data, _PREHASH_ParcelID, data.mParcelId);
	msg->getString(_PREHASH_Data, _PREHASH_Name, data.mName);
	msg->getString(_PREHASH_Data, _PREHASH_Desc, data.mDesc);
	msg->getUUID(_PREHASH_Data, _PREHASH_SnapshotID, data.mSnapshotId);
	msg->getString(_PREHASH_Data, _PREHASH_User, data.mUserName);
	msg->getString(_PREHASH_Data, _PREHASH_OriginalName, data.mParcelName);
	msg->getString(_PREHASH_Data, _PREHASH_SimName, data.mSimName);
	msg->getVector3d(_PREHASH_Data, _PREHASH_PosGlobal, data.mPosGlobal);
	msg->getS32(_PREHASH_Data, _PREHASH_SortOrder, data.mSortOrder);
	msg->getBool(_PREHASH_Data, _PREHASH_Enabled, data.mEnabled);

	notifyObservers(data.mAvatarId, APT_PICK_INFO, (void*)&data);
}

//static
void LLAvatarProperties::sendPickInfoUpdate(const LLAvatarPickInfo& data)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	msg->newMessage(_PREHASH_PickInfoUpdate);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_Data);
	msg->addUUID(_PREHASH_PickID, data.mPickId);
	msg->addUUID(_PREHASH_CreatorID, data.mAvatarId);
	// Legacy, no more used server-side.
	msg->addBool(_PREHASH_TopPick, false);
	// Fills in on simulator if null
	msg->addUUID(_PREHASH_ParcelID, data.mParcelId);
	msg->addString(_PREHASH_Name, data.mName);
	msg->addString(_PREHASH_Desc, data.mDesc);
	msg->addUUID(_PREHASH_SnapshotID, data.mSnapshotId);
	msg->addVector3d(_PREHASH_PosGlobal, data.mPosGlobal);
	msg->addS32(_PREHASH_SortOrder, data.mSortOrder);
	msg->addBool(_PREHASH_Enabled, data.mEnabled);
	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::sendPickDelete(const LLUUID& avatar_id,
										const LLUUID& pick_id)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	if (avatar_id != gAgentID)
	{
		if (gAgent.isGodlikeWithoutAdminMenuFakery())
		{
			llinfos << "Attempting to delete a pick not pertaining to us. Owner Id: "
					<< avatar_id << " - Pick Id: " << pick_id << llendl;
			msg->newMessage(_PREHASH_PickGodDelete);
			msg->nextBlock(_PREHASH_AgentData);
			msg->addUUID(_PREHASH_AgentID, gAgentID);
			msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlock(_PREHASH_Data);
			msg->addUUID(_PREHASH_PickID, pick_id);
			// *HACK: we need to send the pick's creator id to accomplish the
			// delete, and we do not use the query id for anything. JC
			msg->addUUID(_PREHASH_QueryID, avatar_id);
			gAgent.sendReliableMessage();
		}
		else
		{
			llwarns << "Attempting to delete a pick not pertaining to us. Aborted."
					<< llendl;
		}
		return;
	}

	msg->newMessage(_PREHASH_PickDelete);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_Data);
	msg->addUUID(_PREHASH_PickID, pick_id);
	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::processAvatarClassifiedReply(LLMessageSystem* msg,
													  void**)
{
	if (!msg) return;	// Paranoia

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent ID mismatch. Got: " << agent_id << llendl;
		return;
	}

	LLAvatarClassifieds data;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_TargetID, data.mAvatarId);

	LLUUID id;
	std::string name;
	S32 block_count = msg->getNumberOfBlocksFast(_PREHASH_Data);
	for (S32 i = 0; i < block_count; ++i)
	{
		msg->getUUIDFast(_PREHASH_Data, _PREHASH_ClassifiedID, id, i);
		msg->getStringFast(_PREHASH_Data, _PREHASH_Name, name, i);
		data.mMap.emplace(id, name);
	}
	notifyObservers(data.mAvatarId, APT_CLASSIFIEDS, (void*)&data);
}

//static
void LLAvatarProperties::sendClassifiedInfoRequest(const LLUUID& classified_id)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	msg->newMessageFast(_PREHASH_ClassifiedInfoRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_ClassifiedID, classified_id);
	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::processClassifiedInfoReply(LLMessageSystem* msg,
													void**)
{
	if (!msg) return;	// Paranoia

	// Extract the agent id and verify the message is for this client.
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent Id mismatch. Got: " << agent_id << llendl;
		return;
	}

	LLAvatarClassifiedInfo info;
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_ClassifiedID, info.mClassifiedId);
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_CreatorID, info.mAvatarId);
	msg->getU32Fast(_PREHASH_Data, _PREHASH_CreationDate, info.mCreationDate);
	msg->getU32(_PREHASH_Data, _PREHASH_ExpirationDate, info.mExpirationDate);
	msg->getU32Fast(_PREHASH_Data, _PREHASH_Category, info.mCategory);
	msg->getStringFast(_PREHASH_Data, _PREHASH_Name, info.mName);
	msg->getStringFast(_PREHASH_Data, _PREHASH_Desc, info.mDesc);
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_ParcelID, info.mParcelId);
	msg->getU32(_PREHASH_Data, _PREHASH_ParentEstate, info.mParentEstate);
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_SnapshotID, info.mSnapshotId);
	msg->getStringFast(_PREHASH_Data, _PREHASH_SimName, info.mSimName);
	msg->getVector3dFast(_PREHASH_Data, _PREHASH_PosGlobal, info.mPosGlobal);
	msg->getStringFast(_PREHASH_Data, _PREHASH_ParcelName, info.mParcelName);
	msg->getU8Fast(_PREHASH_Data, _PREHASH_ClassifiedFlags, info.mFlags);
	msg->getS32(_PREHASH_Data, _PREHASH_PriceForListing, info.mListingPrice);

	notifyObservers(info.mAvatarId, APT_CLASSIFIED_INFO, (void*)&info);
}

//static
void LLAvatarProperties::sendClassifiedInfoUpdate(const LLAvatarClassifiedInfo& data)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	llinfos << "Sending update for agent classified: " << data.mName
			<< llendl;

	msg->newMessageFast(_PREHASH_ClassifiedInfoUpdate);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_ClassifiedID, data.mClassifiedId);
	msg->addU32Fast(_PREHASH_Category, data.mCategory);
	msg->addStringFast(_PREHASH_Name, data.mName);
	msg->addStringFast(_PREHASH_Desc, data.mDesc);
	// Fills in on simulator if null
	msg->addUUIDFast(_PREHASH_ParcelID, data.mParcelId);
	msg->addU32Fast(_PREHASH_ParentEstate, 0);	// Fills in on simulator
	msg->addUUIDFast(_PREHASH_SnapshotID, data.mSnapshotId);
	msg->addVector3dFast(_PREHASH_PosGlobal, data.mPosGlobal);
	msg->addU8Fast(_PREHASH_ClassifiedFlags, data.mFlags);
	msg->addS32(_PREHASH_PriceForListing,data.mListingPrice);
	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::sendClassifiedDelete(const LLUUID& classified_id)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	msg->newMessageFast(_PREHASH_ClassifiedDelete);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_ClassifiedID, classified_id);
	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::sendAvatarNotesUpdate(const LLUUID& avatar_id,
											   const std::string& notes)
{
	if (gSavedSettings.getBool("UseAgentProfileCap"))
	{
		std::string url = gAgent.getRegionCapability("AgentProfile");
		if (!url.empty())
		{
			LL_DEBUGS("AvatarProperties") << "Using AgentProfile capability to update notes for avatar: "
										  << avatar_id << LL_ENDL;
			LLSD data;
			data["notes"] = notes;
			url += "/" + avatar_id.asString();
			gCoros.launch("sendAvatarPropertiesUpdateCoro",
						  boost::bind(sendAvatarPropertiesUpdateCoro,
									  data, url));
			return;
		}
	}

	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	msg->newMessage(_PREHASH_AvatarNotesUpdate);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_Data);
	msg->addUUID(_PREHASH_TargetID, avatar_id);
	msg->addString(_PREHASH_Notes, notes);

	gAgent.sendReliableMessage();
}

//static
void LLAvatarProperties::processAvatarNotesReply(LLMessageSystem* msg, void**)
{
	if (!msg) return;	// Paranoia

	// Extract the agent id and verify the message is for this client.
	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Agent Id mismatch. Got: " << agent_id << llendl;
		return;
	}

	LLAvatarNotes notes;
	notes.mReceivedViaCap = false;
	msg->getUUID(_PREHASH_Data, _PREHASH_TargetID, notes.mAvatarId);
	msg->getString(_PREHASH_Data, _PREHASH_Notes, notes.mNotes);

	notifyObservers(notes.mAvatarId, APT_NOTES, (void*)&notes);
}
