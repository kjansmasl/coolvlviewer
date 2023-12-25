/**
 * @file llparcelflags.h
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

#ifndef LL_LLPARCEL_FLAGS_H
#define LL_LLPARCEL_FLAGS_H

//---------------------------------------------------------------------------
// Parcel Flags (PF) constants
//---------------------------------------------------------------------------
constexpr U32 PF_ALLOW_FLY			= 1 << 0;// Can start flying
constexpr U32 PF_ALLOW_OTHER_SCRIPTS= 1 << 1;// Scripts by others can run.
constexpr U32 PF_FOR_SALE			= 1 << 2;// Can buy this land
constexpr U32 PF_FOR_SALE_OBJECTS	= 1 << 7;// Can buy all objects on this land
constexpr U32 PF_ALLOW_LANDMARK		= 1 << 3;
constexpr U32 PF_ALLOW_TERRAFORM	= 1 << 4;
constexpr U32 PF_ALLOW_DAMAGE		= 1 << 5;
constexpr U32 PF_CREATE_OBJECTS		= 1 << 6;
// 7 is moved above
constexpr U32 PF_USE_ACCESS_GROUP	= 1 << 8;
constexpr U32 PF_USE_ACCESS_LIST	= 1 << 9;
constexpr U32 PF_USE_BAN_LIST		= 1 << 10;
constexpr U32 PF_USE_PASS_LIST		= 1 << 11;
constexpr U32 PF_SHOW_DIRECTORY		= 1 << 12;
constexpr U32 PF_ALLOW_DEED_TO_GROUP		= 1 << 13;
constexpr U32 PF_CONTRIBUTE_WITH_DEED		= 1 << 14;
constexpr U32 PF_SOUND_LOCAL				= 1 << 15;	// Hear sounds in this parcel only
constexpr U32 PF_SELL_PARCEL_OBJECTS		= 1 << 16;	// Objects on land are included as part of the land when the land is sold
constexpr U32 PF_ALLOW_PUBLISH				= 1 << 17;	// Allow publishing of parcel information on the web
constexpr U32 PF_MATURE_PUBLISH				= 1 << 18;	// The information on this parcel is mature
constexpr U32 PF_URL_WEB_PAGE				= 1 << 19;	// The "media URL" is an HTML page
constexpr U32 PF_URL_RAW_HTML				= 1 << 20;	// The "media URL" is a raw HTML string like <H1>Foo</H1>
constexpr U32 PF_RESTRICT_PUSHOBJECT		= 1 << 21;	// Restrict push object to either on agent or on scripts owned by parcel owner
constexpr U32 PF_DENY_ANONYMOUS				= 1 << 22;	// Deny all non identified/transacted accounts
#if 0	// not used
constexpr U32 PF_DENY_IDENTIFIED			= 1 << 23;	// Deny identified accounts
constexpr U32 PF_DENY_TRANSACTED			= 1 << 24;	// Deny identified accounts
#endif
constexpr U32 PF_ALLOW_GROUP_SCRIPTS		= 1 << 25;	// Allow scripts owned by group
constexpr U32 PF_CREATE_GROUP_OBJECTS		= 1 << 26;	// Allow object creation by group members or objects
constexpr U32 PF_ALLOW_ALL_OBJECT_ENTRY		= 1 << 27;	// Allow all objects to enter a parcel
constexpr U32 PF_ALLOW_GROUP_OBJECT_ENTRY	= 1 << 28;	// Only allow group (and owner) objects to enter the parcel
constexpr U32 PF_ALLOW_VOICE_CHAT			= 1 << 29;	// Allow residents to use voice chat on this parcel
constexpr U32 PF_USE_ESTATE_VOICE_CHAN      = 1 << 30;
constexpr U32 PF_DENY_AGEUNVERIFIED         = 1 << 31;  // Prevent residents who aren't age-verified
// NOTE: At one point we have used all of the bits.
// We have deprecated two of them in 1.19.0 which *could* be reused,
// but only after we are certain there are no simstates using those bits.

//constexpr U32 PF_RESERVED			= 1U << 31;

// If any of these are true the parcel is restricting access in some maner.
constexpr U32 PF_USE_RESTRICTED_ACCESS = PF_USE_ACCESS_GROUP
										| PF_USE_ACCESS_LIST
										| PF_USE_BAN_LIST
										| PF_USE_PASS_LIST
										| PF_DENY_ANONYMOUS
										| PF_DENY_AGEUNVERIFIED;
constexpr U32 PF_NONE = 0x00000000;
constexpr U32 PF_ALL  = 0xFFFFFFFF;
constexpr U32 PF_DEFAULT =  PF_ALLOW_FLY
							| PF_ALLOW_OTHER_SCRIPTS
							| PF_ALLOW_GROUP_SCRIPTS
							| PF_ALLOW_LANDMARK
							| PF_CREATE_OBJECTS
							| PF_CREATE_GROUP_OBJECTS
							| PF_USE_BAN_LIST
							| PF_ALLOW_ALL_OBJECT_ENTRY
							| PF_ALLOW_GROUP_OBJECT_ENTRY
    	                    | PF_ALLOW_VOICE_CHAT
        	                | PF_USE_ESTATE_VOICE_CHAN;

// Access list flags
constexpr U32 AL_ACCESS				= (1 << 0);
constexpr U32 AL_BAN				= (1 << 1);
#if 0	// not used
constexpr U32 AL_RENTER				= (1 << 2);
#endif
constexpr U32 AL_ALLOW_EXPERIENCE	= (1 << 3);
constexpr U32 AL_BLOCK_EXPERIENCE	= (1 << 4);

// Block access return values. BA_ALLOWED is the only success case since some
// code in the simulator relies on that assumption. All other BA_ values should
// be reasons why you are not allowed.
constexpr S32 BA_ALLOWED = 0;
constexpr S32 BA_NOT_IN_GROUP = 1;
constexpr S32 BA_NOT_ON_LIST = 2;
constexpr S32 BA_BANNED = 3;
constexpr S32 BA_NO_ACCESS_LEVEL = 4;
constexpr S32 BA_NOT_AGE_VERIFIED = 5;

// ParcelRelease flags
constexpr U32 PR_NONE		= 0x0;
constexpr U32 PR_GOD_FORCE	= (1 << 0);

enum EObjectCategory
{
	OC_INVALID = -1,
	OC_NONE = 0,
	OC_TOTAL = 0,	// yes zero, like OC_NONE
	OC_OWNER,
	OC_GROUP,
	OC_OTHER,
	OC_SELECTED,
	OC_TEMP,
	OC_COUNT
};

constexpr S32 PARCEL_DETAILS_NAME = 0;
constexpr S32 PARCEL_DETAILS_DESC = 1;
constexpr S32 PARCEL_DETAILS_OWNER = 2;
constexpr S32 PARCEL_DETAILS_GROUP = 3;
constexpr S32 PARCEL_DETAILS_AREA = 4;
constexpr S32 PARCEL_DETAILS_ID = 5;
constexpr S32 PARCEL_DETAILS_SEE_AVATARS = 6;

#endif
