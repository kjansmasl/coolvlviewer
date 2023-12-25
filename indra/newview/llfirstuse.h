/**
 * @file llfirstuse.h
 * @brief Methods that spawn "first-use" dialogs.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLFIRSTUSE_H
#define LL_LLFIRSTUSE_H

#include <string>
#include <vector>

#include "stdtypes.h"

class LLFirstUse
{
public:
	// Sets all controls back to show the dialogs.
	static void disableFirstUse();
	static void resetFirstUse();

	// These methods are called each time the appropriate action is taken. The
	// functions themselves handle only showing the dialog the first time, or
	// subsequent times if the user wishes.
	static void useBalanceIncrease(S32 delta);
	static void useBalanceDecrease(S32 delta);
	static void useSit();
	static void useMap();
	static void useBuild();
	static void useLeftClickNoHit();
	static void useOverrideKeys();
	static void useAppearance();
	static void useInventory();
	static void useSandbox();
	static void useSculptedPrim();
	static void useMedia();
	static void useJellyDoll();

private:
	// Adds all the config variables to sConfigVariables for use by
	// disableFirstUse() and resetFirstUse()
	static void populate();

	static void simpleNotification(const std::string& name);

private:
	static std::vector<std::string> sConfigVariables;
};

#endif	// LL_LLFIRSTUSE_H
