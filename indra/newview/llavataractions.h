/**
 * @file llavataractions.h
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

#ifndef LL_LLAVATARACTIONS_H
#define LL_LLAVATARACTIONS_H

#include "lluuid.h"

class LLAvatarName;
class LLViewerRegion;

// Purely static class
class LLAvatarActions
{
	LLAvatarActions() = delete;
	~LLAvatarActions() = delete;

protected:
	LOG_CLASS(LLAvatarActions);

public:
	// Friendship offers.

	// Request with avatar name resolution and a dialog
	static void requestFriendshipDialog(const LLUUID& id);
	// Request with known avatar name and a dialog
	static void requestFriendshipDialog(const LLUUID& id,
										const std::string& name);
	// Request with known name and withour dialog
	static void requestFriendship(const LLUUID& id,
								  const std::string& name,
								  const std::string& message);

	// Send teleport offers.
	static void offerTeleport(const LLUUID& id);
	static void offerTeleport(const uuid_vec_t& ids);

	// Request teleport from another avatar.
	static void teleportRequest(const LLUUID& id);

	// Start instant messaging session.
	static void startIM(const LLUUID& id);
	static void startIM(const uuid_vec_t& ids, bool friends = false);

	// Give money to the avatar.
	static void pay(const LLUUID& id);

	// Builds a string containing a list of avatar names.
	//
	// If force_legacy is true, then lecacy names are used, regardless of the
	// name displaying settings (useful for mute or ban lists and other
	// security sensitive lists). HB
	static void buildAvatarsList(std::vector<LLAvatarName> avatar_names,
								 std::string& avatars,
								 bool force_legacy = false,
								 const std::string& separator = ", ");

	// Returns the avatar region when you have permission to eject or freeze
	// this avatar, or NULL otherwise. HB
	static LLViewerRegion* canEjectOrFreeze(const LLUUID& avatar_id);

	// These methods kick (log out) or (un)freeze a given avatar (would work
	// only if you have permission to do so). They ask for confirmation via a
	// dialog. When executed by a God, the God kick message is used, otherwise
	// a user freeze or eject (not kick/logout !) message is sent, when on a
	// controlled parcel or land. HB
	static void kick(const LLUUID& avatar_id);
	static void freeze(const LLUUID& avatar_id, bool freeze);

	// User (not God) eject (with optional ban) and freeze/unfreeze messages
	// sending, with prior land/parcel permission verification. No confirmation
	// is requested. The returned value is true when the permission was correct
	// and the message actually sent. HB
	static bool sendEject(const LLUUID& avatar_id, bool ban);
	static bool sendFreeze(const LLUUID& avatar_id, bool freeze);
};

#endif	// LL_LLAVATARACTIONS_H
