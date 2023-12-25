/**
 * @file llscriptpermissions.h
 * @brief Shared code between compiler and assembler and LSL
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

#ifndef LL_SCRIPTPERMISSIONS_H
#define LL_SCRIPTPERMISSIONS_H

#include "stdtypes.h"

typedef enum e_lscript_runtime_permissions
{
	SCRIPT_PERMISSION_DEBIT,
	SCRIPT_PERMISSION_TAKE_CONTROLS,
	SCRIPT_PERMISSION_REMAP_CONTROLS,
	SCRIPT_PERMISSION_TRIGGER_ANIMATION,
	SCRIPT_PERMISSION_ATTACH,
	SCRIPT_PERMISSION_RELEASE_OWNERSHIP,
	SCRIPT_PERMISSION_CHANGE_LINKS,
	SCRIPT_PERMISSION_CHANGE_JOINTS,
	SCRIPT_PERMISSION_CHANGE_PERMISSIONS,
	SCRIPT_PERMISSION_TRACK_CAMERA,
	SCRIPT_PERMISSION_CONTROL_CAMERA,
	SCRIPT_PERMISSION_TELEPORT,
	SCRIPT_PERMISSION_EXPERIENCE,
	SCRIPT_PERMISSION_SILENT_ESTATE_MANAGEMENT,
	SCRIPT_PERMISSION_OVERRIDE_ANIMATIONS,
	SCRIPT_PERMISSION_RETURN_OBJECTS,
	SCRIPT_PERMISSION_FORCE_SIT_AVATAR,
	SCRIPT_PERMISSION_CHANGE_ENV_SETTINGS,
	SCRIPT_PERMISSION_EOF
} LSCRIPTRunTimePermissions;

extern const U32 LSCRIPTRunTimePermissionBits[];

#endif
