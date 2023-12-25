/**
 * @file llavataractions.cpp
 * @brief avatar-related actions (IM, teleporting, etc)
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llavataractions.h"

#include "llcachename.h"
#include "llnotifications.h"
#include "lltrans.h"
#include "roles_constants.h"	// For GP_LAND_ADMIN

#include "llagent.h"
#include "llcommandhandler.h"
#include "llfloateravatarinfo.h"
#include "llfloaterfriends.h"
#include "llfloaterinspect.h"
#include "llfloatermute.h"
#include "llfloaterpay.h"
#include "llinventorymodel.h"
#include "llimmgr.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewermessage.h"	// send_improved_im() handle_lure() give_money()
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llvoavatar.h"

//-----------------------------------------------------------------------------
// Command handler
//-----------------------------------------------------------------------------

static void on_name_cache_mute(const LLUUID& agent_id, const std::string& name,
							   bool is_group, bool mute_it)
{
	LLMute mute(agent_id, name, LLMute::AGENT);
	if (LLMuteList::isMuted(agent_id, name))
	{
		if (!mute_it)
		{
			LLMuteList::remove(mute);
		}
	}
	else
	{
		if (mute_it)
		{
			LLMuteList::add(mute);
		}
		LLFloaterMute::selectMute(agent_id);
	}
}

class LLAgentHandler final : public LLCommandHandler
{
public:
	LLAgentHandler()
	:	LLCommandHandler("agent", UNTRUSTED_THROTTLE)
	{
	}

	bool canHandleUntrusted(const LLSD& params, const LLSD&,
							LLMediaCtrl*, const std::string& nav_type) override
	{
		if (params.size() < 2)
		{
			return true;	// Do not block; it will fail later in handle()
		}

		if (nav_type == "clicked" || nav_type == "external")
		{
			return true;
		}

		std::string verb = params[1].asString();
		return verb == "about" || verb == "inspect" || verb == "username" ||
			   verb == "displayname" || verb == "completename";
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		if (params.size() < 2) return false;
		LLUUID agent_id;
		if (!agent_id.set(params[0], false))
		{
			return false;
		}

		const std::string verb = params[1].asString();
		if (verb == "about" || verb == "username" || verb == "displayname" ||
			verb == "completename")
		{
			LLFloaterAvatarInfo::show(agent_id);
		}
		else if (verb == "inspect")
		{
			HBFloaterInspectAvatar::show(agent_id);
		}
		else if (verb == "pay")
		{
			LLAvatarActions::pay(agent_id);
		}
		else if (verb == "offerteleport")
		{
			LLAvatarActions::offerTeleport(agent_id);
		}
		else if (verb == "im")
		{
			LLAvatarActions::startIM(agent_id);
		}
		else if (verb == "requestfriend")
		{
			LLAvatarActions::requestFriendshipDialog(agent_id);
		}
		else if (verb == "mute" || verb == "unmute" ||
				 verb == "block" || verb == "unblock")
		{
			if (!gCacheNamep) return false;	// Paranoia
			gCacheNamep->get(agent_id, false,
							 boost::bind(&on_name_cache_mute,
										 _1, _2, _3,
										 verb == "mute" || verb == "block"));
		}
		else
		{
			return false;
		}

		return true;
	}
};
LLAgentHandler gAgentHandler;

///////////////////////////////////////////////////////////////////////////////
// LLAvatarActions class
///////////////////////////////////////////////////////////////////////////////

static void on_avatar_name_friendship(const LLUUID& id,
									  const LLAvatarName& av_name)
{
	std::string fullname;
	if (!LLAvatarName::sLegacyNamesForFriends &&
		LLAvatarNameCache::useDisplayNames())
	{
		if (LLAvatarNameCache::useDisplayNames() == 2)
		{
			fullname = av_name.mDisplayName;
		}
		else
		{
			fullname = av_name.getNames();
		}
	}
	else
	{
		fullname = av_name.getLegacyName();
	}

	LLAvatarActions::requestFriendshipDialog(id, fullname);
}

//static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id)
{
	if (id.isNull() || !gCacheNamep)
	{
		return;
	}

	std::string fullname;
	if (gCacheNamep->getFullName(id, fullname) &&
		(LLAvatarName::sLegacyNamesForFriends ||
		 !LLAvatarNameCache::useDisplayNames()))
	{
		requestFriendshipDialog(id, fullname);
		return;
	}

	LLAvatarNameCache::get(id,
						   boost::bind(&on_avatar_name_friendship, _1, _2));
}

static bool callback_add_friend(const LLSD& notification,
								const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID id = notification["payload"]["id"].asUUID();
		std::string message = response["message"].asString();
//MK
		if (gRLenabled && !gRLInterface.canSendIM(id))
		{
			message = "(Hidden)";
		}
//mk
		std::string name = notification["payload"]["name"].asString();
		LLAvatarActions::requestFriendship(id, name, message);
	}
	return false;
}

//static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id,
											  const std::string& name)
{
	if (id == gAgentID)
	{
		gNotifications.add("AddSelfFriend");
		return;
	}

	LLSD args;
	args["NAME"] = name;
	LLSD payload;
	payload["id"] = id;
	payload["name"] = name;
   	gNotifications.add("AddFriendWithMessage", args, payload,
					   callback_add_friend);
}

//static
void LLAvatarActions::requestFriendship(const LLUUID& id,
										const std::string& name,
										const std::string& message)
{
	const LLUUID& folder_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	send_improved_im(id, name, message, IM_ONLINE, IM_FRIENDSHIP_OFFERED,
					 folder_id);
}

//static
void LLAvatarActions::offerTeleport(const LLUUID& id)
{
	if (id.isNull())
	{
		llwarns << "Null avatar UUID, aborted." << llendl;
	}
	else if (id == gAgentID)
	{
		llwarns << "Cannot teleport self !" << llendl;
	}
	else
	{
		uuid_vec_t ids;
		ids.push_back(id);
		handle_lure(ids);
	}
}

//static
void LLAvatarActions::offerTeleport(const uuid_vec_t& ids) 
{
	if (ids.size() > 0)
	{
		handle_lure(ids);
	}
	else
	{
		llwarns << "Tried to offer teleport to an empty list of avatars"
				<< llendl;
	}
}

static void teleport_request_callback(const LLSD& notification,
									  const LLSD& response)
{
	S32 option = 0;
	if (response.isInteger())
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotification::getSelectedOption(notification, response);
	}
	if (option == 0)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

		msg->nextBlockFast(_PREHASH_MessageBlock);
		msg->addBoolFast(_PREHASH_FromGroup, false);
		LLUUID target_id = notification["substitutions"]["uuid"].asUUID();
		msg->addUUIDFast(_PREHASH_ToAgentID, target_id);
		msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
		msg->addU8Fast(_PREHASH_Dialog, IM_TELEPORT_REQUEST);
		msg->addUUIDFast(_PREHASH_ID, LLUUID::null);

		// no timestamp necessary
		msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP);

		std::string name;
		gAgent.buildFullname(name);
		msg->addStringFast(_PREHASH_FromAgentName, name);

//MK
		if (gRLenabled && !gRLInterface.canSendIM(target_id))
		{
			msg->addStringFast(_PREHASH_Message, "(Hidden)");
		}
		else
//mk
		{
			msg->addStringFast(_PREHASH_Message, response["message"]);
		}

		msg->addU32Fast(_PREHASH_ParentEstateID, 0);
		msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
		msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());

		msg->addBinaryDataFast(_PREHASH_BinaryBucket, EMPTY_BINARY_BUCKET,
							   EMPTY_BINARY_BUCKET_SIZE);

		gAgent.sendReliableMessage();
	}
}

//static
void LLAvatarActions::teleportRequest(const LLUUID& id)
{
	if (id.isNull())
	{
		llwarns << "Null avatar UUID, aborted." << llendl;
	}
	else if (id == gAgentID)
	{
		llwarns << "Cannot request a teleport to self !" << llendl;
	}
	else
	{
		LLAvatarName av_name;
		if (LLAvatarNameCache::get(id, &av_name))
		{
			LLSD notification;
			notification["uuid"] = id;
			notification["NAME"] = av_name.getNames();
			LLSD payload;
			gNotifications.add("TeleportRequestPrompt", notification, payload,
							   teleport_request_callback);
		}
		else	// Unlikely ... they just picked this name from somewhere...
		{
			// Re-invoke this very method after the name resolves
			LLAvatarNameCache::get(id, boost::bind(&teleportRequest, id));
		}
	}
}

static void on_avatar_name_cache_start_im(const LLUUID& agent_id,
										  const LLAvatarName& av_name)
{
	if (gIMMgrp)
	{
		gIMMgrp->setFloaterOpen(true);
		gIMMgrp->addSession(av_name.getLegacyName(), IM_NOTHING_SPECIAL,
							agent_id);
		make_ui_sound("UISndStartIM");
	}
}

//static
void LLAvatarActions::startIM(const LLUUID& id)
{
	if (id.isNull())
	{
		llwarns << "Null avatar UUID, aborted." << llendl;
	}
	else if (id == gAgentID)
	{
		llwarns << "Cannot IM to self !" << llendl;
	}
	else
	{
		LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_cache_start_im,
											   _1, _2));
	}
}

//static
void LLAvatarActions::startIM(const uuid_vec_t& ids, bool friends)
{
	if (!gIMMgrp) return;

	S32 count = ids.size();
	if (count > 1)
	{
		// Group IM
		LLUUID session_id;
		session_id.generate();
		gIMMgrp->setFloaterOpen(true);
		// *TODO: translate
		gIMMgrp->addSession(friends ? "Friends Conference"
									: "Avatars Conference",
							IM_SESSION_CONFERENCE_START, ids[0], ids);
		make_ui_sound("UISndStartIM");
	}
	else if (count == 1)
	{
		// Single avatar
		LLUUID agent_id = ids[0];
		startIM(agent_id);
	}
	else
	{
		llwarns << "Tried to initiate an IM conference with an empty list of participants"
				<< llendl;
	}
}

//static
void LLAvatarActions::pay(const LLUUID& id)
{
	if (id.isNull())
	{
		llwarns << "Null avatar UUID, aborted." << llendl;
	}
	else
	{
		LLFloaterPay::payDirectly(&give_money, id, false);
	}
}

//static
void LLAvatarActions::buildAvatarsList(std::vector<LLAvatarName> avatar_names,
									   std::string& avatars, bool force_legacy,
									   const std::string& separator)
{
	U32 name_usage = force_legacy ? 0 : LLAvatarNameCache::useDisplayNames();
	std::sort(avatar_names.begin(), avatar_names.end());
	for (std::vector<LLAvatarName>::const_iterator it = avatar_names.begin(),
												   end = avatar_names.end();
		 it != end; ++it)
	{
		if (!avatars.empty())
		{
			avatars.append(separator);
		}

		switch (name_usage)
		{
			case 2:
				avatars.append(it->mDisplayName);
				break;

			case 1:
				avatars.append(it->getNames());
				break;

			default:
				avatars.append(it->getLegacyName());
		}
	}
}

//static
LLViewerRegion* LLAvatarActions::canEjectOrFreeze(const LLUUID& avatar_id)
{
	LLVOAvatar* avatarp = gObjectList.findAvatar(avatar_id);
	if (!avatarp)
	{
		return NULL;
	}
	LLViewerRegion* regionp = avatarp->getRegion();
	if (!regionp)
	{
		return NULL;
	}

	const LLVector3& pos = avatarp->getPositionRegion();
	bool can_do = regionp->isOwnedSelf(pos);

	const LLVector3d& pos_global = avatarp->getPositionGlobal();
	LLParcel* parcelp =
		gViewerParcelMgr.selectParcelAt(pos_global)->getParcel();
	if (parcelp && (!can_do || regionp->isOwnedGroup(pos)))
	{
		can_do = gViewerParcelMgr.isParcelOwnedByAgent(parcelp, GP_LAND_ADMIN);
	}

	return regionp;
}

//static
bool LLAvatarActions::sendEject(const LLUUID& avatar_id, bool ban)
{
	LLViewerRegion* regionp = canEjectOrFreeze(avatar_id);
	if (!regionp)
	{
		return false;
	}

	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return false;	// Paranoia

	msg->newMessage(_PREHASH_EjectUser);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_Data);
	msg->addUUID(_PREHASH_TargetID, avatar_id);
	msg->addU32(_PREHASH_Flags, ban ? 0x1 : 0x0);
	msg->sendReliable(regionp->getHost());
	return true;
}

//static
bool LLAvatarActions::sendFreeze(const LLUUID& avatar_id, bool freeze)
{
	LLViewerRegion* regionp = canEjectOrFreeze(avatar_id);
	if (!regionp)
	{
		return false;
	}

	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return false;	// Paranoia

	msg->newMessage(_PREHASH_FreezeUser);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_Data);
	msg->addUUID(_PREHASH_TargetID, avatar_id);
	msg->addU32(_PREHASH_Flags, freeze ? 0x0 : 0x1);
	msg->sendReliable(regionp->getHost());
	return true;
}

static bool god_finish_kick(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLMessageSystem* msg = gMessageSystemp;
		if (!msg) return false;	// Paranoia

		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		U32 flags = notification["payload"]["flags"].asInteger();
		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID, gAgentID);
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_AgentID, avatar_id);
		msg->addU32(_PREHASH_KickFlags, flags);
		msg->addStringFast(_PREHASH_Reason, response["message"].asString());
		gAgent.sendReliableMessage();
	}
	return false;
}

static bool user_finish_eject(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		LLAvatarActions::sendEject(avatar_id, false);
	}
	return false;
}

static bool user_finish_freeze(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		bool freeze = notification["payload"]["freeze"].asBoolean();
		LLAvatarActions::sendFreeze(avatar_id, freeze);
	}
	return false;
}

//static
void LLAvatarActions::kick(const LLUUID& avatar_id)
{
	LLSD payload;
	payload["avatar_id"] = avatar_id;

	LLSD args;
	std::string fullname;
	if (gCacheNamep && gCacheNamep->getFullName(avatar_id, fullname))
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			fullname = gRLInterface.getDummyName(fullname);
		}
//mk
		args["AVATAR_NAME"] = fullname;
	}
	else
	{
		args["AVATAR_NAME"] = LLTrans::getString("this_resident");
	}

	if (gAgent.isGodlikeWithoutAdminMenuFakery())
	{
		payload["flags"] = S32(KICK_FLAGS_DEFAULT);
		gNotifications.add("KickUser", args, payload, god_finish_kick);
	}
	else if (canEjectOrFreeze(avatar_id))
	{
		gNotifications.add("EjectUserNoMessage", args, payload,
						   user_finish_eject);
	}
}

//static
void LLAvatarActions::freeze(const LLUUID& avatar_id, bool freeze)
{
	LLSD payload;
	payload["avatar_id"] = avatar_id;

	LLSD args;
	std::string fullname;
	if (gCacheNamep && gCacheNamep->getFullName(avatar_id, fullname))
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			fullname = gRLInterface.getDummyName(fullname);
		}
//mk
		args["AVATAR_NAME"] = fullname;
	}
	else
	{
		args["AVATAR_NAME"] = LLTrans::getString("this_resident");
	}

	if (gAgent.isGodlikeWithoutAdminMenuFakery())
	{
		payload["flags"] = S32(freeze ? KICK_FLAGS_FREEZE
									  : KICK_FLAGS_UNFREEZE);
		gNotifications.add("FreezeUser", args, payload, god_finish_kick);
	}
	else if (canEjectOrFreeze(avatar_id))
	{
		payload["freeze"] = freeze;
		gNotifications.add("FreezeUserNoMessage", args, payload,
						   user_finish_freeze);
	}
}
