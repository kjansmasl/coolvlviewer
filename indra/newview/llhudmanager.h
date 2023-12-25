/**
 * @file llhudmanager.h
 * @brief LLHUDManager class definition
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

#ifndef LL_LLHUDMANAGER_H
#define LL_LLHUDMANAGER_H

// Responsible for managing all HUD elements.

#include <vector>

#include "llhudobject.h"

class LLHUDEffect;
class LLMessageSystem;

// Purely static class
class LLHUDManager
{
	LLHUDManager() = delete;
	~LLHUDManager() = delete;

protected:
	LOG_CLASS(LLHUDManager);

public:
	static LLHUDEffect* createEffect(U8 type, bool send_to_sim = true,
									 bool originated_here = true);

	static void updateEffects();
	static void sendEffects();
	static void cleanupEffects();

	static void cleanupClass();

	static void processViewerEffect(LLMessageSystem* mesgsys, void**);

protected:
	typedef std::vector<LLPointer<LLHUDEffect> > effects_list_t;
	static effects_list_t sHUDEffects;
};

#endif // LL_LLHUDMANAGER_H
