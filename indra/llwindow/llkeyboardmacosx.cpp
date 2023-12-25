/**
 * @file llkeyboardmacosx.cpp
 * @brief Handler for assignable key bindings
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

#if LL_DARWIN

#include "linden_common.h"
#include "llkeyboardmacosx.h"
#include "llwindow.h"

#include "llwindowmacosx-objc.h"

LLKeyboardMacOSX::LLKeyboardMacOSX()
{
	// Virtual keycode mapping table.  Yes, this was as annoying to generate as it looks.
	mTranslateKeyMap[0x00] = 'A';
	mTranslateKeyMap[0x01] = 'S';
	mTranslateKeyMap[0x02] = 'D';
	mTranslateKeyMap[0x03] = 'F';
	mTranslateKeyMap[0x04] = 'H';
	mTranslateKeyMap[0x05] = 'G';
	mTranslateKeyMap[0x06] = 'Z';
	mTranslateKeyMap[0x07] = 'X';
	mTranslateKeyMap[0x08] = 'C';
	mTranslateKeyMap[0x09] = 'V';
	mTranslateKeyMap[0x0b] = 'B';
	mTranslateKeyMap[0x0c] = 'Q';
	mTranslateKeyMap[0x0d] = 'W';
	mTranslateKeyMap[0x0e] = 'E';
	mTranslateKeyMap[0x0f] = 'R';
	mTranslateKeyMap[0x10] = 'Y';
	mTranslateKeyMap[0x11] = 'T';
	mTranslateKeyMap[0x12] = '1';
	mTranslateKeyMap[0x13] = '2';
	mTranslateKeyMap[0x14] = '3';
	mTranslateKeyMap[0x15] = '4';
	mTranslateKeyMap[0x16] = '6';
	mTranslateKeyMap[0x17] = '5';
	mTranslateKeyMap[0x18] = '=';	// KEY_EQUALS
	mTranslateKeyMap[0x19] = '9';
	mTranslateKeyMap[0x1a] = '7';
	mTranslateKeyMap[0x1b] = '-';	// KEY_HYPHEN
	mTranslateKeyMap[0x1c] = '8';
	mTranslateKeyMap[0x1d] = '0';
	mTranslateKeyMap[0x1e] = ']';
	mTranslateKeyMap[0x1f] = 'O';
	mTranslateKeyMap[0x20] = 'U';
	mTranslateKeyMap[0x21] = '[';
	mTranslateKeyMap[0x22] = 'I';
	mTranslateKeyMap[0x23] = 'P';
	mTranslateKeyMap[0x24] = KEY_RETURN;
	mTranslateKeyMap[0x25] = 'L';
	mTranslateKeyMap[0x26] = 'J';
	mTranslateKeyMap[0x27] = '\'';
	mTranslateKeyMap[0x28] = 'K';
	mTranslateKeyMap[0x29] = ';';
	mTranslateKeyMap[0x2a] = '\\';
	mTranslateKeyMap[0x2b] = ',';
	mTranslateKeyMap[0x2c] = KEY_DIVIDE;
	mTranslateKeyMap[0x2d] = 'N';
	mTranslateKeyMap[0x2e] = 'M';
	mTranslateKeyMap[0x2f] = '.';
	mTranslateKeyMap[0x30] = KEY_TAB;
	mTranslateKeyMap[0x31] = ' ';	// space!
	mTranslateKeyMap[0x32] = '`';
	mTranslateKeyMap[0x33] = KEY_BACKSPACE;
	mTranslateKeyMap[0x35] = KEY_ESCAPE;
	//mTranslateKeyMap[0x37] = 0;	// Command key.  (not used yet)
	mTranslateKeyMap[0x38] = KEY_SHIFT;
	mTranslateKeyMap[0x39] = KEY_CAPSLOCK;
	mTranslateKeyMap[0x3a] = KEY_ALT;
	mTranslateKeyMap[0x3b] = KEY_CONTROL;
	mTranslateKeyMap[0x41] = '.';	// keypad
	mTranslateKeyMap[0x43] = '*';	// keypad
	mTranslateKeyMap[0x45] = '+';	// keypad
	mTranslateKeyMap[0x4b] = KEY_PAD_DIVIDE;	// keypad
	mTranslateKeyMap[0x4c] = KEY_RETURN;	// keypad enter
	mTranslateKeyMap[0x4e] = '-';	// keypad
	mTranslateKeyMap[0x51] = '=';	// keypad
	mTranslateKeyMap[0x52] = '0';	// keypad
	mTranslateKeyMap[0x53] = '1';	// keypad
	mTranslateKeyMap[0x54] = '2';	// keypad
	mTranslateKeyMap[0x55] = '3';	// keypad
	mTranslateKeyMap[0x56] = '4';	// keypad
	mTranslateKeyMap[0x57] = '5';	// keypad
	mTranslateKeyMap[0x58] = '6';	// keypad
	mTranslateKeyMap[0x59] = '7';	// keypad
	mTranslateKeyMap[0x5b] = '8';	// keypad
	mTranslateKeyMap[0x5c] = '9';	// keypad
	mTranslateKeyMap[0x60] = KEY_F5;
	mTranslateKeyMap[0x61] = KEY_F6;
	mTranslateKeyMap[0x62] = KEY_F7;
	mTranslateKeyMap[0x63] = KEY_F3;
	mTranslateKeyMap[0x64] = KEY_F8;
	mTranslateKeyMap[0x65] = KEY_F9;
	mTranslateKeyMap[0x67] = KEY_F11;
	mTranslateKeyMap[0x6d] = KEY_F10;
	mTranslateKeyMap[0x6f] = KEY_F12;
	mTranslateKeyMap[0x72] = KEY_INSERT;
	mTranslateKeyMap[0x73] = KEY_HOME;
	mTranslateKeyMap[0x74] = KEY_PAGE_UP;
	mTranslateKeyMap[0x75] = KEY_DELETE;
	mTranslateKeyMap[0x76] = KEY_F4;
	mTranslateKeyMap[0x77] = KEY_END;
	mTranslateKeyMap[0x78] = KEY_F2;
	mTranslateKeyMap[0x79] = KEY_PAGE_DOWN;
	mTranslateKeyMap[0x7a] = KEY_F1;
	mTranslateKeyMap[0x7b] = KEY_LEFT;
	mTranslateKeyMap[0x7c] = KEY_RIGHT;
	mTranslateKeyMap[0x7d] = KEY_DOWN;
	mTranslateKeyMap[0x7e] = KEY_UP;

	// Build inverse map
	std::map<U32, KEY>::iterator iter;
	for (iter = mTranslateKeyMap.begin(); iter != mTranslateKeyMap.end(); iter++)
	{
		mInvTranslateKeyMap[iter->second] = iter->first;
	}

	// build numpad maps
	mTranslateNumpadMap[0x52] = KEY_PAD_INS;    // keypad 0
	mTranslateNumpadMap[0x53] = KEY_PAD_END;   // keypad 1
	mTranslateNumpadMap[0x54] = KEY_PAD_DOWN;	// keypad 2
	mTranslateNumpadMap[0x55] = KEY_PAD_PGDN;	// keypad 3
	mTranslateNumpadMap[0x56] = KEY_PAD_LEFT;	// keypad 4
	mTranslateNumpadMap[0x57] = KEY_PAD_CENTER;	// keypad 5
	mTranslateNumpadMap[0x58] = KEY_PAD_RIGHT;	// keypad 6
	mTranslateNumpadMap[0x59] = KEY_PAD_HOME;	// keypad 7
	mTranslateNumpadMap[0x5b] = KEY_PAD_UP;		// keypad 8
	mTranslateNumpadMap[0x5c] = KEY_PAD_PGUP;	// keypad 9
	mTranslateNumpadMap[0x41] = KEY_PAD_DEL;	// keypad .
	mTranslateNumpadMap[0x4c] = KEY_PAD_RETURN;	// keypad enter

	// Build inverse numpad map
	for (iter = mTranslateNumpadMap.begin(); iter != mTranslateNumpadMap.end(); iter++)
	{
		mInvTranslateNumpadMap[iter->second] = iter->first;
	}
}

void LLKeyboardMacOSX::resetMaskKeys()
{
	U32 mask = getModifiers();

	// MBW -- XXX -- This mirrors the operation of the Windows version of
	// resetMaskKeys(). It looks a bit suspicious, as it won't correct for keys
	// that have been released. Is this the way it's supposed to work ?

	if (mask & MAC_SHIFT_KEY)
	{
		mKeyLevel[KEY_SHIFT] = true;
	}

	if (mask & MAC_CTRL_KEY)
	{
		mKeyLevel[KEY_CONTROL] = true;
	}

	if (mask & MAC_ALT_KEY)
	{
		mKeyLevel[KEY_ALT] = true;
	}
}

#if 0
static bool translateKeyMac(U32 key, U32 mask, KEY& out_key, U32& out_mask)
{
	// Translate the virtual keycode into the keycodes the keyboard system
	// expects.
	U32 virtual_key = (mask >> 24) & 0x0000007F;
	outKey = macKeyTransArray[virtual_key];


	return outKey != 0;
}
#endif

void LLKeyboardMacOSX::handleModifier(MASK mask)
{
	updateModifiers(mask);
}

MASK LLKeyboardMacOSX::updateModifiers(U32 mask)
{
	// translate the mask
	MASK out_mask = 0;

	if (mask & MAC_SHIFT_KEY)
	{
		out_mask |= MASK_SHIFT;
	}

	if (mask & (MAC_CTRL_KEY | MAC_CMD_KEY))
	{
		out_mask |= MASK_CONTROL;
	}

	if (mask & MAC_ALT_KEY)
	{
		out_mask |= MASK_ALT;
	}

	return out_mask;
}

bool LLKeyboardMacOSX::handleKeyDown(U32 key, U32 mask)
{
	U32 translated_mask = updateModifiers(mask);

	KEY translated_key = 0;
	if (translateNumpadKey(key, &translated_key, translated_mask))
	{
		return handleTranslatedKeyDown(translated_key, translated_mask);
	}

	return false;
}


bool LLKeyboardMacOSX::handleKeyUp(U32 key, U32 mask)
{
	U32 translated_mask = updateModifiers(mask);

	KEY translated_key = 0;
	if (translateNumpadKey(key, &translated_key, translated_mask))
	{
		return handleTranslatedKeyUp(translated_key, translated_mask);
	}

	return false;
}

MASK LLKeyboardMacOSX::currentMask(bool for_mouse_event)
{
	MASK result = MASK_NONE;

	U32 mask = getModifiers();
	if (mask & MAC_SHIFT_KEY)
	{
		result |= MASK_SHIFT;
	}
	if (mask & MAC_CTRL_KEY)
	{
		result |= MASK_CONTROL;
	}
	if (mask & MAC_ALT_KEY)
	{
		result |= MASK_ALT;
	}
	// For keyboard events, consider Command equivalent to Control
	if (!for_mouse_event && (mask & MAC_CMD_KEY))
	{
		result |= MASK_CONTROL;
	}

	return result;
}

void LLKeyboardMacOSX::scanKeyboard()
{
	for (S32 key = 0; key < KEY_COUNT; ++key)
	{
		// Generate callback if any event has occurred on this key this frame.
		// Cannot just test mKeyLevel, because this could be a slow frame and
		// key might have gone down then up. JC
		if (mKeyLevel[key] || mKeyDown[key] || mKeyUp[key])
		{
			mCurScanKey = key;
			mCallbacks->handleScanKey(key, mKeyDown[key], mKeyUp[key],
									  mKeyLevel[key]);
		}
	}

	// Reset edges for next frame
	for (S32 key = 0; key < KEY_COUNT; ++key)
	{
		mKeyUp[key] = false;
		mKeyDown[key] = false;
		if (mKeyLevel[key])
		{
			++mKeyLevelFrameCount[key];
		}
	}
}

bool LLKeyboardMacOSX::translateNumpadKey(U32 os_key, KEY* translated_key,
										  MASK mask)
{
	if (mNumpadDistinct == ND_NUMLOCK_ON)
	{
		std::map<U32, KEY>::iterator iter = mTranslateNumpadMap.find(os_key);
		if (iter != mTranslateNumpadMap.end())
		{
			*translated_key = iter->second;
			return true;
		}
	}
	return translateKey(os_key, translated_key, mask);
}

U32	LLKeyboardMacOSX::inverseTranslateNumpadKey(KEY translated_key)
{
	if (mNumpadDistinct == ND_NUMLOCK_ON)
	{
		std::map<KEY, U32>::iterator iter =
			mInvTranslateNumpadMap.find(translated_key);
		if (iter != mInvTranslateNumpadMap.end())
		{
			return iter->second;
		}
	}
	return inverseTranslateKey(translated_key);
}

#endif // LL_DARWIN
