/**
 * @file llkeyboardmacosx.h
 * @brief Handler for assignable key bindings
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLKEYBOARDMACOSX_H
#define LL_LLKEYBOARDMACOSX_H

#include "llkeyboard.h"

// These more or less mirror their equivalents in NSEvent.h.
enum EMacEventKeys {
	MAC_SHIFT_KEY = 1 << 17,
	MAC_CTRL_KEY = 1 << 18,
	MAC_ALT_KEY = 1 << 19,
	MAC_CMD_KEY = 1 << 20,
	MAC_FN_KEY = 1 << 23
};

class LLKeyboardMacOSX final : public LLKeyboard
{
public:
	LLKeyboardMacOSX();
	~LLKeyboardMacOSX() override	 {}

	bool handleKeyUp(U32 key, MASK mask) override;
	bool handleKeyDown(U32 key, MASK mask) override;
	void resetMaskKeys() override;
	MASK currentMask(bool for_mouse_event) override;
	void scanKeyboard() override;
	void handleModifier(MASK mask) override;

protected:
	MASK updateModifiers(U32 mask);
#if 0
	void setModifierKeyLevel(KEY key, bool new_state);
#endif
	bool translateNumpadKey(U32 os_key, KEY* translated_key, MASK mask);
	U32 inverseTranslateNumpadKey(KEY translated_key);

private:
	// Special map for translating OS keys to numpad keys
	std::map<U32, KEY> mTranslateNumpadMap;
	// Inverse of the above
	std::map<KEY, U32> mInvTranslateNumpadMap;
};

#endif
