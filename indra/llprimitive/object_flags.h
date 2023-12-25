/**
 * @file object_flags.h
 * @brief Flags for object creation and transmission
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

#ifndef LL_OBJECT_FLAGS_H
#define LL_OBJECT_FLAGS_H

// Downstream flags from sim->viewer
constexpr U32 FLAGS_USE_PHYSICS				= (1U << 0);
constexpr U32 FLAGS_CREATE_SELECTED			= (1U << 1);
constexpr U32 FLAGS_OBJECT_MODIFY			= (1U << 2);
constexpr U32 FLAGS_OBJECT_COPY				= (1U << 3);
constexpr U32 FLAGS_OBJECT_ANY_OWNER		= (1U << 4);
constexpr U32 FLAGS_OBJECT_YOU_OWNER		= (1U << 5);
constexpr U32 FLAGS_SCRIPTED				= (1U << 6);
constexpr U32 FLAGS_HANDLE_TOUCH			= (1U << 7);
constexpr U32 FLAGS_OBJECT_MOVE				= (1U << 8);
constexpr U32 FLAGS_TAKES_MONEY				= (1U << 9);
constexpr U32 FLAGS_PHANTOM					= (1U << 10);
constexpr U32 FLAGS_INVENTORY_EMPTY			= (1U << 11);

constexpr U32 FLAGS_AFFECTS_NAVMESH			= (1U << 12);
constexpr U32 FLAGS_CHARACTER				= (1U << 13);
constexpr U32 FLAGS_VOLUME_DETECT			= (1U << 14);
constexpr U32 FLAGS_INCLUDE_IN_SEARCH		= (1U << 15);

constexpr U32 FLAGS_ALLOW_INVENTORY_DROP	= (1U << 16);
constexpr U32 FLAGS_OBJECT_TRANSFER			= (1U << 17);
constexpr U32 FLAGS_OBJECT_GROUP_OWNED		= (1U << 18);
//constexpr U32 FLAGS_OBJECT_YOU_OFFICER	= (1U << 19);	// No more in use

constexpr U32 FLAGS_CAMERA_DECOUPLED 		= (1U << 20);
constexpr U32 FLAGS_ANIM_SOURCE				= (1U << 21);
constexpr U32 FLAGS_CAMERA_SOURCE			= (1U << 22);

//constexpr U32 FLAGS_CAST_SHADOWS			= (1U << 23);	// No more in use

constexpr U32 FLAGS_OBJECT_OWNER_MODIFY	= (1U << 28);

constexpr U32 FLAGS_TEMPORARY_ON_REZ		= (1U << 29);
//constexpr U32 FLAGS_TEMPORARY				= (1U << 30);	// No more in use
//constexpr U32 FLAGS_ZLIB_COMPRESSED		= (1U << 31);	// No more in use

constexpr U32 FLAGS_LOCAL					= FLAGS_ANIM_SOURCE | FLAGS_CAMERA_SOURCE;

typedef enum e_havok_joint_type
{
	HJT_INVALID = 0,
	HJT_HINGE  	= 1,
	HJT_POINT 	= 2,
//	HJT_LPOINT 	= 3,
//	HJT_WHEEL 	= 4,
	HJT_EOF 	= 3
} EHavokJointType;

#endif

