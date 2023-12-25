/**
 * @file llkeyboardwin32.h
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

#ifndef LL_LLKEYBOARDWIN32_H
#define LL_LLKEYBOARDWIN32_H

#include "llkeyboard.h"

// This mask distinguishes extended keys, which include non-numpad arrow keys
// (and, curiously, the num lock and numpad '/')
const MASK MASK_EXTENDED =  0x0100;

class LLKeyboardWin32 final : public LLKeyboard
{
public:
	LLKeyboardWin32();
	~LLKeyboardWin32() override		{}

	bool handleKeyUp(U32 key, MASK mask) override;
	bool handleKeyDown(U32 key, MASK mask) override;
	void resetMaskKeys() override;
	MASK currentMask(bool for_mouse_event) override;
	void scanKeyboard() override;

protected:
	bool translateExtendedKey(U32 os_key, MASK mask, KEY* translated_key,
							  MASK translated_mask);
	U32 inverseTranslateExtendedKey(KEY translated_key);

	MASK updateModifiers();

#if 0
	void setModifierKeyLevel(KEY key, bool new_state);
#endif

private:
	std::map<U32, KEY> mTranslateNumpadMap;
	std::map<KEY, U32> mInvTranslateNumpadMap;
};

#endif
