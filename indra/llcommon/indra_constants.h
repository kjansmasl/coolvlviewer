/** 
 * @file indra_constants.h
 * @brief some useful short term constants for Indra
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

#ifndef LL_INDRA_CONSTANTS_H
#define LL_INDRA_CONSTANTS_H

#include "stdtypes.h"

class LLUUID;

constexpr F32 REGION_WIDTH_METERS = 256.f;
constexpr S32 REGION_WIDTH_UNITS = (S32)REGION_WIDTH_METERS;
constexpr U32 REGION_WIDTH_U32 = (U32)REGION_WIDTH_METERS;

constexpr F32 REGION_HEIGHT_METERS = 4096.f;

constexpr F32 DEFAULT_AGENT_DEPTH = 0.45f;
constexpr F32 DEFAULT_AGENT_WIDTH = 0.60f;

enum ETerrainBrushType
{
	// The valid brush numbers cannot be reordered, because they are used in
	// the binary LSL format as arguments to llModifyLand()
	E_LANDBRUSH_LEVEL	= 0,
	E_LANDBRUSH_RAISE	= 1,
	E_LANDBRUSH_LOWER	= 2,
	E_LANDBRUSH_SMOOTH 	= 3,
	E_LANDBRUSH_NOISE	= 4,
	E_LANDBRUSH_REVERT 	= 5,
	E_LANDBRUSH_INVALID = 6
};

// Bit masks for various keyboard modifier keys.
constexpr MASK MASK_NONE		= 0x0000;
constexpr MASK MASK_CONTROL		= 0x0001;		// Mapped to cmd on Macs
constexpr MASK MASK_ALT			= 0x0002;
constexpr MASK MASK_SHIFT		= 0x0004;
constexpr MASK MASK_NORMALKEYS	= 0x0007;     // A real mask - only get the bits for normal modifier keys
constexpr MASK MASK_MAC_CONTROL	= 0x0008;		// Un-mapped Ctrl key on Macs, not used on Windows
constexpr MASK MASK_MODIFIERS	= MASK_CONTROL|MASK_ALT|MASK_SHIFT|MASK_MAC_CONTROL;

// Special keys go into >128
constexpr KEY KEY_SPECIAL	= 0x80;	// special keys start here
constexpr KEY KEY_RETURN	= 0x81;
constexpr KEY KEY_LEFT		= 0x82;
constexpr KEY KEY_RIGHT		= 0x83;
constexpr KEY KEY_UP		= 0x84;
constexpr KEY KEY_DOWN		= 0x85;
constexpr KEY KEY_ESCAPE	= 0x86;
constexpr KEY KEY_BACKSPACE	= 0x87;
constexpr KEY KEY_DELETE	= 0x88;
constexpr KEY KEY_SHIFT		= 0x89;
constexpr KEY KEY_CONTROL	= 0x8A;
constexpr KEY KEY_ALT		= 0x8B;
constexpr KEY KEY_HOME		= 0x8C;
constexpr KEY KEY_END		= 0x8D;
constexpr KEY KEY_PAGE_UP	= 0x8E;
constexpr KEY KEY_PAGE_DOWN	= 0x8F;
constexpr KEY KEY_HYPHEN	= 0x90;
constexpr KEY KEY_EQUALS	= 0x91;
constexpr KEY KEY_INSERT	= 0x92;
constexpr KEY KEY_CAPSLOCK	= 0x93;
constexpr KEY KEY_TAB		= 0x94;
constexpr KEY KEY_ADD		= 0x95;
constexpr KEY KEY_SUBTRACT	= 0x96;
constexpr KEY KEY_MULTIPLY	= 0x97;
constexpr KEY KEY_DIVIDE	= 0x98;
constexpr KEY KEY_F1		= 0xA1;
constexpr KEY KEY_F2		= 0xA2;
constexpr KEY KEY_F3		= 0xA3;
constexpr KEY KEY_F4		= 0xA4;
constexpr KEY KEY_F5		= 0xA5;
constexpr KEY KEY_F6		= 0xA6;
constexpr KEY KEY_F7		= 0xA7;
constexpr KEY KEY_F8		= 0xA8;
constexpr KEY KEY_F9		= 0xA9;
constexpr KEY KEY_F10		= 0xAA;
constexpr KEY KEY_F11		= 0xAB;
constexpr KEY KEY_F12		= 0xAC;

constexpr KEY KEY_PAD_UP		= 0xC0;
constexpr KEY KEY_PAD_DOWN		= 0xC1;
constexpr KEY KEY_PAD_LEFT		= 0xC2;
constexpr KEY KEY_PAD_RIGHT		= 0xC3;
constexpr KEY KEY_PAD_HOME		= 0xC4;
constexpr KEY KEY_PAD_END		= 0xC5;
constexpr KEY KEY_PAD_PGUP		= 0xC6;
constexpr KEY KEY_PAD_PGDN		= 0xC7;
constexpr KEY KEY_PAD_CENTER	= 0xC8; // the 5 in the middle
constexpr KEY KEY_PAD_INS		= 0xC9;
constexpr KEY KEY_PAD_DEL		= 0xCA;
constexpr KEY KEY_PAD_RETURN	= 0xCB;
constexpr KEY KEY_PAD_ADD		= 0xCC; // not used
constexpr KEY KEY_PAD_SUBTRACT	= 0xCD; // not used
constexpr KEY KEY_PAD_MULTIPLY  = 0xCE; // not used
constexpr KEY KEY_PAD_DIVIDE	= 0xCF; // not used

constexpr KEY KEY_BUTTON0	= 0xD0;
constexpr KEY KEY_BUTTON1	= 0xD1;
constexpr KEY KEY_BUTTON2	= 0xD2;
constexpr KEY KEY_BUTTON3	= 0xD3;
constexpr KEY KEY_BUTTON4	= 0xD4;
constexpr KEY KEY_BUTTON5	= 0xD5;
constexpr KEY KEY_BUTTON6	= 0xD6;
constexpr KEY KEY_BUTTON7	= 0xD7;
constexpr KEY KEY_BUTTON8	= 0xD8;
constexpr KEY KEY_BUTTON9	= 0xD9;
constexpr KEY KEY_BUTTON10	= 0xDA;
constexpr KEY KEY_BUTTON11	= 0xDB;
constexpr KEY KEY_BUTTON12	= 0xDC;
constexpr KEY KEY_BUTTON13	= 0xDD;
constexpr KEY KEY_BUTTON14	= 0xDE;
constexpr KEY KEY_BUTTON15	= 0xDF;

constexpr KEY KEY_NONE		= 0xFF; // not sent from keyboard.  For internal use only.

constexpr S32 KEY_COUNT		= 256;

constexpr F32 DEFAULT_WATER_HEIGHT = 20.0f;

// Maturity ratings for simulators
constexpr U8 SIM_ACCESS_MIN 	= 0;		// Treated as 'unknown', usually ends up being SIM_ACCESS_PG
constexpr U8 SIM_ACCESS_PG		= 13;
constexpr U8 SIM_ACCESS_MATURE	= 21;
constexpr U8 SIM_ACCESS_ADULT	= 42;		// Seriously Adult Only
constexpr U8 SIM_ACCESS_DOWN	= 254;
constexpr U8 SIM_ACCESS_MAX 	= SIM_ACCESS_ADULT;

// Attachment constants
constexpr S32 MAX_AGENT_ATTACHMENTS = 38;
constexpr U8  ATTACHMENT_ADD = 0x80;

// God levels
constexpr U8 GOD_MAINTENANCE = 250;
constexpr U8 GOD_FULL = 200;
constexpr U8 GOD_LIAISON = 150;
constexpr U8 GOD_CUSTOMER_SERVICE = 100;
constexpr U8 GOD_LIKE = 1;
constexpr U8 GOD_NOT = 0;

// "agent id" for things that should be done to ALL agents
extern const LLUUID LL_UUID_ALL_AGENTS;

// Inventory library owner
extern const LLUUID ALEXANDRIA_LINDEN_ID;

extern const LLUUID GOVERNOR_LINDEN_ID;
// Maintenance's group id.
extern const LLUUID MAINTENANCE_GROUP_ID;

// Attachments baking pseudo-textures
extern const LLUUID IMG_USE_BAKED_HEAD;
extern const LLUUID IMG_USE_BAKED_UPPER;
extern const LLUUID IMG_USE_BAKED_LOWER;
extern const LLUUID IMG_USE_BAKED_HAIR;
extern const LLUUID IMG_USE_BAKED_EYES;
extern const LLUUID IMG_USE_BAKED_SKIRT;
extern const LLUUID IMG_USE_BAKED_LEFTARM;
extern const LLUUID IMG_USE_BAKED_LEFTLEG;
extern const LLUUID IMG_USE_BAKED_AUX1;
extern const LLUUID IMG_USE_BAKED_AUX2;
extern const LLUUID IMG_USE_BAKED_AUX3;

// Blank GLTF material asset Id
extern const LLUUID BLANK_MATERIAL_ASSET_ID;

// Flags for kick message
constexpr U32 KICK_FLAGS_DEFAULT	= 0x0;
constexpr U32 KICK_FLAGS_FREEZE		= 1 << 0;
constexpr U32 KICK_FLAGS_UNFREEZE	= 1 << 1;

// Radius within which a chat message is fully audible
constexpr F32 CHAT_WHISPER_RADIUS	= 10.f;
constexpr F32 CHAT_NORMAL_RADIUS	= 20.f;
constexpr F32 CHAT_SHOUT_RADIUS		= 100.f;

// Media commands
constexpr U32 PARCEL_MEDIA_COMMAND_STOP			= 0;
constexpr U32 PARCEL_MEDIA_COMMAND_PAUSE		= 1;
constexpr U32 PARCEL_MEDIA_COMMAND_PLAY			= 2;
constexpr U32 PARCEL_MEDIA_COMMAND_LOOP			= 3;
constexpr U32 PARCEL_MEDIA_COMMAND_TEXTURE		= 4;
constexpr U32 PARCEL_MEDIA_COMMAND_URL			= 5;
constexpr U32 PARCEL_MEDIA_COMMAND_TIME			= 6;
constexpr U32 PARCEL_MEDIA_COMMAND_AGENT		= 7;
constexpr U32 PARCEL_MEDIA_COMMAND_UNLOAD		= 8;
constexpr U32 PARCEL_MEDIA_COMMAND_AUTO_ALIGN	= 9;
constexpr U32 PARCEL_MEDIA_COMMAND_TYPE			= 10;
constexpr U32 PARCEL_MEDIA_COMMAND_SIZE			= 11;
constexpr U32 PARCEL_MEDIA_COMMAND_DESC			= 12;
constexpr U32 PARCEL_MEDIA_COMMAND_LOOP_SET		= 13;

constexpr S32 CHAT_CHANNEL_DEBUG = S32_MAX;

// DO NOT CHANGE THE SEQUENCE OF THIS LIST !
constexpr U8 CLICK_ACTION_NONE			= 0;
constexpr U8 CLICK_ACTION_TOUCH			= 0;
constexpr U8 CLICK_ACTION_SIT			= 1;
constexpr U8 CLICK_ACTION_BUY			= 2;
constexpr U8 CLICK_ACTION_PAY			= 3;
constexpr U8 CLICK_ACTION_OPEN			= 4;
constexpr U8 CLICK_ACTION_PLAY			= 5;
constexpr U8 CLICK_ACTION_OPEN_MEDIA	= 6;
constexpr U8 CLICK_ACTION_ZOOM			= 7;
constexpr U8 CLICK_ACTION_DISABLED		= 8;
constexpr U8 CLICK_ACTION_IGNORE		= 9;

// Primitve-related "constants" (a couple are not since they differ in SL and
// in OpenSim grids). Used to be exported from llprimitive.cpp (the couple non-
// constant ones still are). Added here, since quite a few of these constants
// are needed by llmath, and I did not want to make the latter depend on
// llprimitive; this also allows to use constexpr, with avoids the allocation
// of a storage for each constant by the compiler... HB

constexpr F32 OBJECT_CUT_MIN			= 0.f;
constexpr F32 OBJECT_CUT_MAX			= 1.f;
constexpr F32 OBJECT_CUT_INC			= 0.05f;
constexpr F32 OBJECT_MIN_CUT_INC		= 0.02f;
constexpr F32 OBJECT_ROTATION_PRECISION	= 0.05f;

constexpr F32 OBJECT_TWIST_MIN = -360.f;
constexpr F32 OBJECT_TWIST_MAX =  360.f;
constexpr F32 OBJECT_TWIST_INC =   18.f;

// This is used for linear paths, since twist is used in a slightly different
// manner.
constexpr F32 OBJECT_TWIST_LINEAR_MIN = -180.f;
constexpr F32 OBJECT_TWIST_LINEAR_MAX =  180.f;
constexpr F32 OBJECT_TWIST_LINEAR_INC =    9.f;

extern F32 OBJECT_MIN_HOLE_SIZE;				// Differs in SL and OpenSim
constexpr F32 OBJECT_MAX_HOLE_SIZE_X = 1.f;
constexpr F32 OBJECT_MAX_HOLE_SIZE_Y = 0.5f;

// Hollow
constexpr F32 OBJECT_HOLLOW_MIN			= 0.f;
extern F32 OBJECT_HOLLOW_MAX;					// Differs in SL and OpenSim
constexpr F32 OBJECT_HOLLOW_MAX_SQUARE	= 0.7f;

// Revolutions parameters.
constexpr F32 OBJECT_REV_MIN = 1.f;
constexpr F32 OBJECT_REV_MAX = 4.f;
constexpr F32 OBJECT_REV_INC = 0.1f;

// Lights
constexpr F32 LIGHT_MIN_RADIUS = 0.f;
constexpr F32 LIGHT_DEFAULT_RADIUS = 5.f;
constexpr F32 LIGHT_MAX_RADIUS = 20.f;
constexpr F32 LIGHT_MIN_FALLOFF = 0.f;
constexpr F32 LIGHT_DEFAULT_FALLOFF = 1.f;
constexpr F32 LIGHT_MAX_FALLOFF = 2.f;
constexpr F32 LIGHT_MIN_CUTOFF = 0.f;
constexpr F32 LIGHT_DEFAULT_CUTOFF = 0.f;
constexpr F32 LIGHT_MAX_CUTOFF = 180.f;

// "Tension" => [0,10], increments of 0.1
constexpr F32 FLEXIBLE_OBJECT_MIN_TENSION = 0.f;
constexpr F32 FLEXIBLE_OBJECT_DEFAULT_TENSION = 1.f;
constexpr F32 FLEXIBLE_OBJECT_MAX_TENSION = 10.f;

// "Drag" => [0,10], increments of 0.1
constexpr F32 FLEXIBLE_OBJECT_MIN_AIR_FRICTION = 0.f;
constexpr F32 FLEXIBLE_OBJECT_DEFAULT_AIR_FRICTION = 2.f;
constexpr F32 FLEXIBLE_OBJECT_MAX_AIR_FRICTION = 10.f;

// "Gravity" = [-10,10], increments of 0.1
constexpr F32 FLEXIBLE_OBJECT_MIN_GRAVITY = -10.f;
constexpr F32 FLEXIBLE_OBJECT_DEFAULT_GRAVITY = 0.3f;
constexpr F32 FLEXIBLE_OBJECT_MAX_GRAVITY = 10.f;

// "Wind" = [0,10], increments of 0.1
constexpr F32 FLEXIBLE_OBJECT_MIN_WIND_SENSITIVITY = 0.f;
constexpr F32 FLEXIBLE_OBJECT_DEFAULT_WIND_SENSITIVITY = 0.f;
constexpr F32 FLEXIBLE_OBJECT_MAX_WIND_SENSITIVITY = 10.f;

constexpr F32 FLEXIBLE_OBJECT_MAX_INTERNAL_TENSION_FORCE = 0.99f;

constexpr F32 FLEXIBLE_OBJECT_DEFAULT_LENGTH = 1.f;
constexpr bool FLEXIBLE_OBJECT_DEFAULT_USING_COLLISION_SPHERE = false;
constexpr bool FLEXIBLE_OBJECT_DEFAULT_RENDERING_COLLISION_SPHERE = false;

// The following constants were defined in llvomulme.cpp, with a "to do"
// commment "move to llprimitive.cpp". They are best placed here. HB

constexpr F32 RATIO_MIN = 0.f;
// Inverted sense here: 0 = top taper, 2 = bottom taper
constexpr F32 RATIO_MAX = 2.f;

constexpr F32 SHEAR_MIN = -0.5f;
constexpr F32 SHEAR_MAX =  0.5f;

constexpr F32 TAPER_MIN = -1.f;
constexpr F32 TAPER_MAX =  1.f;

constexpr F32 SKEW_MIN	= -0.95f;
constexpr F32 SKEW_MAX	=  0.95f;

constexpr F32 SCULPT_MIN_AREA = 0.002f;
constexpr S32 SCULPT_MIN_AREA_DETAIL = 1;

#endif
