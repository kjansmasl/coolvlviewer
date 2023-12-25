/**
 * @file llregionflags.h
 * @brief Flags that are sent in the statistics message region_flags field.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLREGIONFLAGS_H
#define LL_LLREGIONFLAGS_H

#include "llpreprocessor.h"

// Can you be hurt here ?  Should health be on ?
constexpr U64 REGION_FLAGS_ALLOW_DAMAGE				= (1 << 0);

// Can you make landmarks here ?
constexpr U64 REGION_FLAGS_ALLOW_LANDMARK			= (1 << 1);

// Do we reset the home position when someone teleports away from here ?
constexpr U64 REGION_FLAGS_ALLOW_SET_HOME			= (1 << 2);

// Do we reset the home position when someone teleports away from here ?
constexpr U64 REGION_FLAGS_RESET_HOME_ON_TELEPORT	= (1 << 3);

// Does the sun move ?  Deprecated in SL.
constexpr U64 REGION_FLAGS_SUN_FIXED				= (1 << 4);

// Does the estate owner allow private parcels ?
constexpr U64 REGION_FLAGS_ALLOW_ACCESS_OVERRIDE	= (1 << 5);

// Cannot change the terrain heightfield, even on owned parcels, but can plant
// trees and grass.
constexpr U64 REGION_FLAGS_BLOCK_TERRAFORM			= (1 << 6);

// Can't release, sell, or buy land.
constexpr U64 REGION_FLAGS_BLOCK_LAND_RESELL		= (1 << 7);

// All content wiped once per night
constexpr U64 REGION_FLAGS_SANDBOX					= (1 << 8);
#if 0	// REGION_FLAGS_NULL_LAYER is deprecated in SL...
constexpr U64 REGION_FLAGS_NULL_LAYER				= (1 << 9);
#else	// ..and reused by REGION_FLAGS_ALLOW_ENVIRONMENT_OVERRIDE
constexpr U64 REGION_FLAGS_ALLOW_ENVIRONMENT_OVERRIDE = (1 << 9);
#endif
constexpr U64 REGION_FLAGS_SKIP_COLLISIONS			= (1 << 12); // Pin all non agent rigid bodies
constexpr U64 REGION_FLAGS_SKIP_SCRIPTS				= (1 << 13);
constexpr U64 REGION_FLAGS_SKIP_PHYSICS				= (1 << 14); // Skip all physics
constexpr U64 REGION_FLAGS_EXTERNALLY_VISIBLE		= (1 << 15);
constexpr U64 REGION_FLAGS_ALLOW_RETURN_ENCROACHING_OBJECT = (1 << 16);
constexpr U64 REGION_FLAGS_ALLOW_RETURN_ENCROACHING_ESTATE_OBJECT = (1 << 17);
constexpr U64 REGION_FLAGS_BLOCK_DWELL				= (1 << 18);

// Is flight allowed ?
constexpr U64 REGION_FLAGS_BLOCK_FLY				= (1 << 19);

// Is direct teleport (p2p) allowed ?
constexpr U64 REGION_FLAGS_ALLOW_DIRECT_TELEPORT	= (1 << 20);

// Is there an administrative override on scripts in the region at the
// moment. This is the similar skip scripts, except this flag is
// presisted in the database on an estate level.
constexpr U64 REGION_FLAGS_ESTATE_SKIP_SCRIPTS		= (1 << 21);

constexpr U64 REGION_FLAGS_RESTRICT_PUSHOBJECT		= (1 << 22);

constexpr U64 REGION_FLAGS_DENY_ANONYMOUS			= (1 << 23);

constexpr U64 REGION_FLAGS_ALLOW_PARCEL_CHANGES		= (1 << 26);

// Used to be: REGION_FLAGS_ABUSE_EMAIL_TO_ESTATE_OWNER
constexpr U64 REGION_FLAGS_BLOCK_FLYOVER			= (1 << 27);

constexpr U64 REGION_FLAGS_ALLOW_VOICE				= (1 << 28);

constexpr U64 REGION_FLAGS_BLOCK_PARCEL_SEARCH		= (1 << 29);
constexpr U64 REGION_FLAGS_DENY_AGEUNVERIFIED		= (1 << 30);

constexpr U64 REGION_FLAGS_DENY_BOTS				= (1 << 31);

constexpr U64 REGION_FLAGS_ENABLE_ANIMATION_TRACKING = (1ULL << 33);

constexpr U64 REGION_FLAGS_DEFAULT = REGION_FLAGS_ALLOW_LANDMARK |
									 REGION_FLAGS_ALLOW_SET_HOME |
									 REGION_FLAGS_ALLOW_PARCEL_CHANGES |
									 REGION_FLAGS_ALLOW_VOICE;


constexpr U64 REGION_FLAGS_PRELUDE_SET = REGION_FLAGS_RESET_HOME_ON_TELEPORT;
constexpr U64 REGION_FLAGS_PRELUDE_UNSET = REGION_FLAGS_ALLOW_LANDMARK
										   | REGION_FLAGS_ALLOW_SET_HOME;

constexpr U64 REGION_FLAGS_ESTATE_MASK = REGION_FLAGS_EXTERNALLY_VISIBLE
										 | REGION_FLAGS_SUN_FIXED
										 | REGION_FLAGS_DENY_ANONYMOUS
										 | REGION_FLAGS_DENY_AGEUNVERIFIED;

LL_INLINE bool is_prelude(U64 flags)
{
	// definition of prelude does not depend on fixed-sun
	return (flags & REGION_FLAGS_PRELUDE_UNSET) == 0 &&
		   (flags & REGION_FLAGS_PRELUDE_SET) != 0;
}

LL_INLINE U32 set_prelude_flags(U64 flags)
{
	// also set the sun-fixed flag
	return ((flags & ~REGION_FLAGS_PRELUDE_UNSET) |
			(REGION_FLAGS_PRELUDE_SET | REGION_FLAGS_SUN_FIXED));
}

LL_INLINE U32 unset_prelude_flags(U64 flags)
{
	// also unset the fixed-sun flag
	return ((flags | REGION_FLAGS_PRELUDE_UNSET) &
			 ~(REGION_FLAGS_PRELUDE_SET | REGION_FLAGS_SUN_FIXED));
}

// Region protocols
constexpr U64 REGION_PROTOCOLS_AGENT_APPEARANCE_SERVICE = (1 << 0);

// Estate constants. Need to match first few etries in indra.estate table.
constexpr U32 ESTATE_ALL = 0; // will not match in db, reserved key for logic
constexpr U32 ESTATE_MAINLAND = 1;
constexpr U32 ESTATE_ORIENTATION = 2;
constexpr U32 ESTATE_INTERNAL = 3;
constexpr U32 ESTATE_SHOWCASE = 4;
constexpr U32 ESTATE_TEEN = 5;
constexpr U32 ESTATE_LAST_LINDEN = 5; // last linden owned/managed estate

// For EstateOwnerRequest, setaccess message
constexpr U32 ESTATE_ACCESS_ALLOWED_AGENTS	= 1 << 0;
constexpr U32 ESTATE_ACCESS_ALLOWED_GROUPS	= 1 << 1;
constexpr U32 ESTATE_ACCESS_BANNED_AGENTS	= 1 << 2;
constexpr U32 ESTATE_ACCESS_MANAGERS		= 1 << 3;

// Maximum number of access list entries we can fit in one packet
constexpr S32 ESTATE_ACCESS_MAX_ENTRIES_PER_PACKET = 63;

// For reply to "getinfo", don't need to forward to all sims in estate
constexpr U32 ESTATE_ACCESS_SEND_TO_AGENT_ONLY = 1 << 4;

constexpr U32 ESTATE_ACCESS_ALL = ESTATE_ACCESS_ALLOWED_AGENTS
								  | ESTATE_ACCESS_ALLOWED_GROUPS
								  | ESTATE_ACCESS_BANNED_AGENTS
								  | ESTATE_ACCESS_MANAGERS;

// For EstateOwnerRequest, estateaccessdelta message
constexpr U32 ESTATE_ACCESS_APPLY_TO_ALL_ESTATES		= 1U << 0;
constexpr U32 ESTATE_ACCESS_APPLY_TO_MANAGED_ESTATES	= 1U << 1;

constexpr U32 ESTATE_ACCESS_ALLOWED_AGENT_ADD			= 1U << 2;
constexpr U32 ESTATE_ACCESS_ALLOWED_AGENT_REMOVE		= 1U << 3;
constexpr U32 ESTATE_ACCESS_ALLOWED_GROUP_ADD			= 1U << 4;
constexpr U32 ESTATE_ACCESS_ALLOWED_GROUP_REMOVE		= 1U << 5;
constexpr U32 ESTATE_ACCESS_BANNED_AGENT_ADD			= 1U << 6;
constexpr U32 ESTATE_ACCESS_BANNED_AGENT_REMOVE			= 1U << 7;
constexpr U32 ESTATE_ACCESS_MANAGER_ADD					= 1U << 8;
constexpr U32 ESTATE_ACCESS_MANAGER_REMOVE				= 1U << 9;
constexpr U32 ESTATE_ACCESS_NO_REPLY					= 1U << 10;
constexpr U32 ESTATE_ACCESS_FAILED_BAN_ESTATE_MANAGER	= 1U << 11;

constexpr S32 ESTATE_MAX_MANAGERS = 20;
constexpr S32 ESTATE_MAX_ACCESS_IDS = 500;
constexpr S32 ESTATE_MAX_BANNED_IDS = 750;
constexpr S32 ESTATE_MAX_GROUP_IDS = (S32)ESTATE_ACCESS_MAX_ENTRIES_PER_PACKET;

// 'Sim Wide Delete' flags
constexpr U32 SWD_OTHERS_LAND_ONLY		= (1 << 0);
constexpr U32 SWD_ALWAYS_RETURN_OBJECTS = (1 << 1);
constexpr U32 SWD_SCRIPTED_ONLY			= (1 << 2);

// Controls experience key validity in the estate
constexpr U32 EXPERIENCE_KEY_TYPE_NONE		= 0;
constexpr U32 EXPERIENCE_KEY_TYPE_BLOCKED	= 1;
constexpr U32 EXPERIENCE_KEY_TYPE_ALLOWED	= 2;
constexpr U32 EXPERIENCE_KEY_TYPE_TRUSTED	= 3;

constexpr U32 EXPERIENCE_KEY_TYPE_FIRST		= EXPERIENCE_KEY_TYPE_BLOCKED;
constexpr U32 EXPERIENCE_KEY_TYPE_LAST		= EXPERIENCE_KEY_TYPE_TRUSTED;

constexpr U32 ESTATE_EXPERIENCE_TRUSTED_ADD		= 1U << 2;
constexpr U32 ESTATE_EXPERIENCE_TRUSTED_REMOVE	= 1U << 3;
constexpr U32 ESTATE_EXPERIENCE_ALLOWED_ADD		= 1U << 4;
constexpr U32 ESTATE_EXPERIENCE_ALLOWED_REMOVE	= 1U << 5;
constexpr U32 ESTATE_EXPERIENCE_BLOCKED_ADD		= 1U << 6;
constexpr U32 ESTATE_EXPERIENCE_BLOCKED_REMOVE	= 1U << 7;

constexpr S32 ESTATE_MAX_EXPERIENCE_IDS = 8;

#endif	// LL_LLREGIONFLAGS_H
