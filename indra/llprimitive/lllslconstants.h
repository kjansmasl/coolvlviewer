/**
 * @file lllslconstants.h
 * @author James Cook
 * @brief Constants used in lsl.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLLSLCONSTANTS_H
#define LL_LLLSLCONSTANTS_H

// LSL: Return flags for llGetAgentInfo
constexpr U32 AGENT_FLYING		= 0x0001;
constexpr U32 AGENT_ATTACHMENTS	= 0x0002;
constexpr U32 AGENT_SCRIPTED	= 0x0004;
constexpr U32 AGENT_MOUSELOOK	= 0x0008;
constexpr U32 AGENT_SITTING		= 0x0010;
constexpr U32 AGENT_ON_OBJECT	= 0x0020;
constexpr U32 AGENT_AWAY		= 0x0040;
constexpr U32 AGENT_WALKING		= 0x0080;
constexpr U32 AGENT_IN_AIR		= 0x0100;
constexpr U32 AGENT_TYPING		= 0x0200;
constexpr U32 AGENT_CROUCHING	= 0x0400;
constexpr U32 AGENT_BUSY		= 0x0800;
constexpr U32 AGENT_ALWAYS_RUN	= 0x1000;
constexpr U32 AGENT_AUTOPILOT	= 0x2000;

constexpr S32 LSL_REMOTE_DATA_CHANNEL		= 1;
constexpr S32 LSL_REMOTE_DATA_REQUEST		= 2;
constexpr S32 LSL_REMOTE_DATA_REPLY			= 3;

// Constants used in extended LSL primitive setter and getters
constexpr S32 LSL_PRIM_TYPE_LEGACY	= 1; // No longer supported.
constexpr S32 LSL_PRIM_MATERIAL		= 2;
constexpr S32 LSL_PRIM_PHYSICS		= 3;
constexpr S32 LSL_PRIM_TEMP_ON_REZ	= 4;
constexpr S32 LSL_PRIM_PHANTOM		= 5;
constexpr S32 LSL_PRIM_POSITION		= 6;
constexpr S32 LSL_PRIM_SIZE			= 7;
constexpr S32 LSL_PRIM_ROTATION		= 8;
constexpr S32 LSL_PRIM_TYPE			= 9; // Replacement for LSL_PRIM_TYPE_LEGACY
constexpr S32 LSL_PRIM_TEXTURE		= 17;
constexpr S32 LSL_PRIM_COLOR		= 18;
constexpr S32 LSL_PRIM_BUMP_SHINY	= 19;
constexpr S32 LSL_PRIM_FULLBRIGHT	= 20;
constexpr S32 LSL_PRIM_FLEXIBLE		= 21;
constexpr S32 LSL_PRIM_TEXGEN		= 22;
constexpr S32 LSL_PRIM_POINT_LIGHT	= 23;
constexpr S32 LSL_PRIM_CAST_SHADOWS	= 24;
constexpr S32 LSL_PRIM_GLOW     	= 25;
constexpr S32 LSL_PRIM_TEXT			= 26;
constexpr S32 LSL_PRIM_NAME			= 27;
constexpr S32 LSL_PRIM_DESC			= 28;
constexpr S32 LSL_PRIM_ROT_LOCAL	= 29;
constexpr S32 LSL_PRIM_PHYSICS_SHAPE_TYPE = 30;
constexpr S32 LSL_PRIM_OMEGA		= 32;
constexpr S32 LSL_PRIM_POS_LOCAL	= 33;
constexpr S32 LSL_PRIM_LINK_TARGET	= 34;
constexpr S32 LSL_PRIM_SLICE		= 35;
constexpr S32 LSL_PRIM_SPECULAR		= 36;
constexpr S32 LSL_PRIM_NORMAL		= 37;
constexpr S32 LSL_PRIM_ALPHA_MODE	= 38;

constexpr S32 LSL_PRIM_TYPE_BOX		= 0;
constexpr S32 LSL_PRIM_TYPE_CYLINDER= 1;
constexpr S32 LSL_PRIM_TYPE_PRISM	= 2;
constexpr S32 LSL_PRIM_TYPE_SPHERE	= 3;
constexpr S32 LSL_PRIM_TYPE_TORUS	= 4;
constexpr S32 LSL_PRIM_TYPE_TUBE	= 5;
constexpr S32 LSL_PRIM_TYPE_RING	= 6;
constexpr S32 LSL_PRIM_TYPE_SCULPT  = 7;

constexpr S32 LSL_PRIM_HOLE_DEFAULT = 0x00;
constexpr S32 LSL_PRIM_HOLE_CIRCLE	= 0x10;
constexpr S32 LSL_PRIM_HOLE_SQUARE  = 0x20;
constexpr S32 LSL_PRIM_HOLE_TRIANGLE= 0x30;

constexpr S32 LSL_PRIM_MATERIAL_STONE	= 0;
constexpr S32 LSL_PRIM_MATERIAL_METAL	= 1;
constexpr S32 LSL_PRIM_MATERIAL_GLASS	= 2;
constexpr S32 LSL_PRIM_MATERIAL_WOOD	= 3;
constexpr S32 LSL_PRIM_MATERIAL_FLESH	= 4;
constexpr S32 LSL_PRIM_MATERIAL_PLASTIC	= 5;
constexpr S32 LSL_PRIM_MATERIAL_RUBBER	= 6;
constexpr S32 LSL_PRIM_MATERIAL_LIGHT	= 7;

constexpr S32 LSL_PRIM_SHINY_NONE		= 0;
constexpr S32 LSL_PRIM_SHINY_LOW		= 1;
constexpr S32 LSL_PRIM_SHINY_MEDIUM		= 2;
constexpr S32 LSL_PRIM_SHINY_HIGH		= 3;

constexpr S32 LSL_PRIM_TEXGEN_DEFAULT	= 0;
constexpr S32 LSL_PRIM_TEXGEN_PLANAR	= 1;

constexpr S32 LSL_PRIM_BUMP_NONE		= 0;
constexpr S32 LSL_PRIM_BUMP_BRIGHT		= 1;
constexpr S32 LSL_PRIM_BUMP_DARK		= 2;
constexpr S32 LSL_PRIM_BUMP_WOOD		= 3;
constexpr S32 LSL_PRIM_BUMP_BARK		= 4;
constexpr S32 LSL_PRIM_BUMP_BRICKS		= 5;
constexpr S32 LSL_PRIM_BUMP_CHECKER		= 6;
constexpr S32 LSL_PRIM_BUMP_CONCRETE	= 7;
constexpr S32 LSL_PRIM_BUMP_TILE		= 8;
constexpr S32 LSL_PRIM_BUMP_STONE		= 9;
constexpr S32 LSL_PRIM_BUMP_DISKS		= 10;
constexpr S32 LSL_PRIM_BUMP_GRAVEL		= 11;
constexpr S32 LSL_PRIM_BUMP_BLOBS		= 12;
constexpr S32 LSL_PRIM_BUMP_SIDING		= 13;
constexpr S32 LSL_PRIM_BUMP_LARGETILE	= 14;
constexpr S32 LSL_PRIM_BUMP_STUCCO		= 15;
constexpr S32 LSL_PRIM_BUMP_SUCTION		= 16;
constexpr S32 LSL_PRIM_BUMP_WEAVE		= 17;

constexpr S32 LSL_PRIM_SCULPT_TYPE_SPHERE   = 1;
constexpr S32 LSL_PRIM_SCULPT_TYPE_TORUS    = 2;
constexpr S32 LSL_PRIM_SCULPT_TYPE_PLANE    = 3;
constexpr S32 LSL_PRIM_SCULPT_TYPE_CYLINDER = 4;
constexpr S32 LSL_PRIM_SCULPT_TYPE_MASK     = 7;
constexpr S32 LSL_PRIM_SCULPT_FLAG_INVERT   = 64;
constexpr S32 LSL_PRIM_SCULPT_FLAG_MIRROR   = 128;

constexpr S32 LSL_PRIM_PHYSICS_SHAPE_PRIM	= 0;
constexpr S32 LSL_PRIM_PHYSICS_SHAPE_NONE	= 1;
constexpr S32 LSL_PRIM_PHYSICS_SHAPE_CONVEX = 2;

constexpr S32 LSL_DENSITY				= 1;
constexpr S32 LSL_FRICTION				= 2;
constexpr S32 LSL_RESTITUTION			= 4;
constexpr S32 LSL_GRAVITY_MULTIPLIER	= 8;

constexpr S32 LSL_ALL_SIDES				= -1;
constexpr S32 LSL_LINK_ROOT				= 1;
constexpr S32 LSL_LINK_FIRST_CHILD		= 2;
constexpr S32 LSL_LINK_SET				= -1;
constexpr S32 LSL_LINK_ALL_OTHERS		= -2;
constexpr S32 LSL_LINK_ALL_CHILDREN		= -3;
constexpr S32 LSL_LINK_THIS				= -4;

// LSL constants for llSetForSell
constexpr S32 SELL_NOT			= 0;
constexpr S32 SELL_ORIGINAL		= 1;
constexpr S32 SELL_COPY			= 2;
constexpr S32 SELL_CONTENTS		= 3;

// LSL constants for llSetPayPrice
constexpr S32 PAY_PRICE_HIDE = -1;
constexpr S32 PAY_PRICE_DEFAULT = -2;
constexpr S32 MAX_PAY_BUTTONS = 4;
constexpr S32 PAY_BUTTON_DEFAULT_0 = 1;
constexpr S32 PAY_BUTTON_DEFAULT_1 = 5;
constexpr S32 PAY_BUTTON_DEFAULT_2 = 10;
constexpr S32 PAY_BUTTON_DEFAULT_3 = 20;

// lsl email registration.
constexpr S32 EMAIL_REG_SUBSCRIBE_OBJECT	= 0x01;
constexpr S32 EMAIL_REG_UNSUBSCRIBE_OBJECT	= 0x02;
constexpr S32 EMAIL_REG_UNSUBSCRIBE_SIM		= 0x04;

constexpr S32 LIST_STAT_RANGE = 0;
constexpr S32 LIST_STAT_MIN		= 1;
constexpr S32 LIST_STAT_MAX		= 2;
constexpr S32 LIST_STAT_MEAN	= 3;
constexpr S32 LIST_STAT_MEDIAN	= 4;
constexpr S32 LIST_STAT_STD_DEV	= 5;
constexpr S32 LIST_STAT_SUM = 6;
constexpr S32 LIST_STAT_SUM_SQUARES = 7;
constexpr S32 LIST_STAT_NUM_COUNT = 8;
constexpr S32 LIST_STAT_GEO_MEAN = 9;

constexpr S32 STRING_TRIM_HEAD = 0x01;
constexpr S32 STRING_TRIM_TAIL = 0x02;
constexpr S32 STRING_TRIM = STRING_TRIM_HEAD | STRING_TRIM_TAIL;

// llGetObjectDetails
constexpr S32 OBJECT_UNKNOWN_DETAIL = -1;
constexpr S32 OBJECT_NAME = 1;
constexpr S32 OBJECT_DESC = 2;
constexpr S32 OBJECT_POS = 3;
constexpr S32 OBJECT_ROT = 4;
constexpr S32 OBJECT_VELOCITY = 5;
constexpr S32 OBJECT_OWNER = 6;
constexpr S32 OBJECT_GROUP = 7;
constexpr S32 OBJECT_CREATOR = 8;
constexpr S32 OBJECT_RUNNING_SCRIPT_COUNT = 9;
constexpr S32 OBJECT_TOTAL_SCRIPT_COUNT = 10;
constexpr S32 OBJECT_SCRIPT_MEMORY = 11;
constexpr S32 OBJECT_SCRIPT_TIME = 12;
constexpr S32 OBJECT_PRIM_EQUIVALENCE = 13;
constexpr S32 OBJECT_SERVER_COST = 14;
constexpr S32 OBJECT_STREAMING_COST = 15;
constexpr S32 OBJECT_PHYSICS_COST = 16;
constexpr S32 OBJECT_CHARACTER_TIME = 17;
constexpr S32 OBJECT_ROOT = 18;
constexpr S32 OBJECT_ATTACHED_POINT = 19;
constexpr S32 OBJECT_PATHFINDING_TYPE = 20;
constexpr S32 OBJECT_PHYSICS = 21;
constexpr S32 OBJECT_PHANTOM = 22;
constexpr S32 OBJECT_TEMP_ON_REZ = 23;
constexpr S32 OBJECT_RENDER_WEIGHT = 24;

// Pathfinding prim types as returned by llGetObjectDetails
constexpr S32 OPT_OTHER = -1;
constexpr S32 OPT_LEGACY_LINKSET = 0;
constexpr S32 OPT_AVATAR = 1;
constexpr S32 OPT_CHARACTER = 2;
constexpr S32 OPT_STATIC_OBSTACLE = 4;
constexpr S32 OPT_MATERIAL_VOLUME = 5;
constexpr S32 OPT_EXCLUSION_VOLUME = 6;

// changed() event flags
constexpr U32 CHANGED_NONE = 0x0;
constexpr U32 CHANGED_INVENTORY = 0x1;
constexpr U32 CHANGED_COLOR = 0x2;
constexpr U32 CHANGED_SHAPE = 0x4;
constexpr U32 CHANGED_SCALE = 0x8;
constexpr U32 CHANGED_TEXTURE = 0x10;
constexpr U32 CHANGED_LINK = 0x20;
constexpr U32 CHANGED_ALLOWED_DROP = 0x40;
constexpr U32 CHANGED_OWNER = 0x80;
constexpr U32 CHANGED_REGION = 0x100;
constexpr U32 CHANGED_TELEPORT = 0x200;
constexpr U32 CHANGED_REGION_START = 0x400;
constexpr U32 CHANGED_MEDIA = 0x800;

// Possible error results
constexpr U32 LSL_STATUS_OK                 = 0;
constexpr U32 LSL_STATUS_MALFORMED_PARAMS   = 1000;
constexpr U32 LSL_STATUS_TYPE_MISMATCH      = 1001;
constexpr U32 LSL_STATUS_BOUNDS_ERROR       = 1002;
constexpr U32 LSL_STATUS_NOT_FOUND          = 1003;
constexpr U32 LSL_STATUS_NOT_SUPPORTED      = 1004;
constexpr U32 LSL_STATUS_INTERNAL_ERROR     = 1999;

// Start per-function errors below, starting at 2000:
constexpr U32 LSL_STATUS_WHITELIST_FAILED   = 2001;

// Memory profiling support
constexpr S32 LSL_PROFILE_SCRIPT_NONE		= 0;
constexpr S32 LSL_PROFILE_SCRIPT_MEMORY		= 1;

// HTTP responses contents type
constexpr S32 LSL_CONTENT_TYPE_TEXT		= 0;
constexpr S32 LSL_CONTENT_TYPE_HTML		= 1;
constexpr S32 LSL_CONTENT_TYPE_XML		= 2;
constexpr S32 LSL_CONTENT_TYPE_XHTML	= 3;
constexpr S32 LSL_CONTENT_TYPE_ATOM		= 4;
constexpr S32 LSL_CONTENT_TYPE_JSON		= 5;
constexpr S32 LSL_CONTENT_TYPE_LLSD		= 6;
constexpr S32 LSL_CONTENT_TYPE_FORM		= 7;
constexpr S32 LSL_CONTENT_TYPE_RSS		= 8;

// Ray casting
constexpr S32 LSL_RCERR_UNKNOWN				= -1;
constexpr S32 LSL_RCERR_SIM_PERF_LOW		= -2;
constexpr S32 LSL_RCERR_CAST_TIME_EXCEEDED	= -3;

constexpr S32 LSL_RC_REJECT_TYPES			= 0;
constexpr S32 LSL_RC_DETECT_PHANTOM			= 1;
constexpr S32 LSL_RC_DATA_FLAGS				= 2;
constexpr S32 LSL_RC_MAX_HITS				= 3;

constexpr S32 LSL_RC_REJECT_AGENTS			= 1;
constexpr S32 LSL_RC_REJECT_PHYSICAL		= 2;
constexpr S32 LSL_RC_REJECT_NONPHYSICAL		= 4;
constexpr S32 LSL_RC_REJECT_LAND			= 8;

constexpr S32 LSL_RC_GET_NORMAL				= 1;
constexpr S32 LSL_RC_GET_ROOT_KEY			= 2;
constexpr S32 LSL_RC_GET_LINK_NUM			= 4;

// Estate management
constexpr S32 LSL_ESTATE_ACCESS_ALLOWED_AGENT_ADD		= 4;
constexpr S32 LSL_ESTATE_ACCESS_ALLOWED_AGENT_REMOVE	= 8;
constexpr S32 LSL_ESTATE_ACCESS_ALLOWED_GROUP_ADD		= 16;
constexpr S32 LSL_ESTATE_ACCESS_ALLOWED_GROUP_REMOVE	= 32;
constexpr S32 LSL_ESTATE_ACCESS_BANNED_AGENT_ADD		= 64;
constexpr S32 LSL_ESTATE_ACCESS_BANNED_AGENT_REMOVE		= 128;

// Key Frame Motion:
constexpr S32 LSL_KFM_COMMAND		= 0;
constexpr S32 LSL_KFM_MODE			= 1;
constexpr S32 LSL_KFM_DATA			= 2;
constexpr S32 LSL_KFM_FORWARD		= 0;
constexpr S32 LSL_KFM_LOOP			= 1;
constexpr S32 LSL_KFM_PING_PONG		= 2;
constexpr S32 LSL_KFM_REVERSE		= 3;
constexpr S32 LSL_KFM_ROTATION		= 1;
constexpr S32 LSL_KFM_TRANSLATION	= 2;
constexpr S32 LSL_KFM_CMD_PLAY		= 0;
constexpr S32 LSL_KFM_CMD_STOP		= 1;
constexpr S32 LSL_KFM_CMD_PAUSE		= 2;

// Experience tools errors:
constexpr S32 LSL_XP_ERROR_NONE					= 0;
constexpr S32 LSL_XP_ERROR_THROTTLED			= 1;
constexpr S32 LSL_XP_ERROR_EXPERIENCES_DISABLED	= 2;
constexpr S32 LSL_XP_ERROR_INVALID_PARAMETERS	= 3;
constexpr S32 LSL_XP_ERROR_NOT_PERMITTED		= 4;
constexpr S32 LSL_XP_ERROR_NO_EXPERIENCE		= 5;
constexpr S32 LSL_XP_ERROR_NOT_FOUND			= 6;
constexpr S32 LSL_XP_ERROR_INVALID_EXPERIENCE	= 7;
constexpr S32 LSL_XP_ERROR_EXPERIENCE_DISABLED	= 8;
constexpr S32 LSL_XP_ERROR_EXPERIENCE_SUSPENDED	= 9;
constexpr S32 LSL_XP_ERROR_UNKNOWN_ERROR		= 10;
constexpr S32 LSL_XP_ERROR_QUOTA_EXCEEDED		= 11;
constexpr S32 LSL_XP_ERROR_STORE_DISABLED		= 12;
constexpr S32 LSL_XP_ERROR_STORAGE_EXCEPTION	= 13;
constexpr S32 LSL_XP_ERROR_KEY_NOT_FOUND		= 14;
constexpr S32 LSL_XP_ERROR_RETRY_UPDATE			= 15;
constexpr S32 LSL_XP_ERROR_MATURITY_EXCEEDED	= 16;

#if 0	// Not used (any more ?) in the viewer code
// This is used by the return to owner code to determine the reason
// that this object is being returned.
enum EReturnReason
{
	RR_GENERIC = 0,
	RR_SANDBOX = 1,
	RR_PARCEL_OWNER = 2,
	RR_PARCEL_AUTO = 3,
	RR_PARCEL_FULL = 4,
	RR_OFF_WORLD = 5,

	RR_COUNT = 6
};
#endif

#endif
