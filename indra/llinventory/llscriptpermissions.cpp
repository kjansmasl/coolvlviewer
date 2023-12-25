/**
 * @file llscriptpermissions.cpp
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

#include "llscriptpermissions.h"

const U32 LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_EOF] =
{
	(0x1 << 1),		//	SCRIPT_PERMISSION_DEBIT
	(0x1 << 2),		//	SCRIPT_PERMISSION_TAKE_CONTROLS
	(0x1 << 3),		//	SCRIPT_PERMISSION_REMAP_CONTROLS
	(0x1 << 4),		//	SCRIPT_PERMISSION_TRIGGER_ANIMATION
	(0x1 << 5),		//	SCRIPT_PERMISSION_ATTACH
	(0x1 << 6),		//	SCRIPT_PERMISSION_RELEASE_OWNERSHIP
	(0x1 << 7),		//	SCRIPT_PERMISSION_CHANGE_LINKS
	(0x1 << 8),		//	SCRIPT_PERMISSION_CHANGE_JOINTS
	(0x1 << 9),		//	SCRIPT_PERMISSION_CHANGE_PERMISSIONS
	(0x1 << 10),	//	SCRIPT_PERMISSION_TRACK_CAMERA
	(0x1 << 11),	//	SCRIPT_PERMISSION_CONTROL_CAMERA
	(0x1 << 12),	//	SCRIPT_PERMISSION_TELEPORT
	(0x1 << 13),	//	SCRIPT_PERMISSION_EXPERIENCE
	(0x1 << 14),	//	SCRIPT_PERMISSION_SILENT_ESTATE_MANAGEMENT
	(0x1 << 15),	//	SCRIPT_PERMISSION_OVERRIDE_ANIMATIONS
	(0x1 << 16),	//	SCRIPT_PERMISSION_RETURN_OBJECTS
	(0x1 << 17),	//	SCRIPT_PERMISSION_FORCE_SIT_AVATAR
	(0x1 << 18),	//	SCRIPT_PERMISSION_CHANGE_ENV_SETTINGS
};
