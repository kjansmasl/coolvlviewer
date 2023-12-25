/**
 * @file llkeyboardwin32.cpp
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

#if LL_WINDOWS

#include "linden_common.h"

#include "llkeyboardwin32.h"

#include "llwindow.h"

LLKeyboardWin32::LLKeyboardWin32()
{
	// Set up key mapping for windows - eventually can read this from a file ?
	// Anything not in the key map gets dropped. Add default A-Z.

	// Virtual key mappings from WinUser.h

	KEY cur_char;
	for (cur_char = 'A'; cur_char <= 'Z'; ++cur_char)
	{
		mTranslateKeyMap[cur_char] = (KEY)cur_char;
	}

	for (cur_char = '0'; cur_char <= '9'; ++cur_char)
	{
		mTranslateKeyMap[cur_char] = (KEY)cur_char;
	}
	// numpad number keys
	for (cur_char = 0x60; cur_char <= 0x69; ++cur_char)
	{
		mTranslateKeyMap[cur_char] = (KEY)('0' + (0x60 - cur_char));
	}

	mTranslateKeyMap[VK_SPACE] = ' ';
	mTranslateKeyMap[VK_OEM_1] = ';';
	// When the user hits, for example, Ctrl-= as a keyboard shortcut, Windows
	// generates VK_OEM_PLUS. This is true on both QWERTY and DVORAK keyboards
	// in the US. Numeric keypad '+' generates VK_ADD below. Thus we translate
	// it as '='. Potential bug: This may not be true on international
	// keyboards. JC
	mTranslateKeyMap[VK_OEM_PLUS] = '=';
	mTranslateKeyMap[VK_OEM_COMMA] = ',';
	mTranslateKeyMap[VK_OEM_MINUS] = '-';
	mTranslateKeyMap[VK_OEM_PERIOD] = '.';
	mTranslateKeyMap[VK_OEM_2] = KEY_PAD_DIVIDE;
	mTranslateKeyMap[VK_OEM_3] = '`';
	mTranslateKeyMap[VK_OEM_4] = '[';
	mTranslateKeyMap[VK_OEM_5] = '\\';
	mTranslateKeyMap[VK_OEM_6] = ']';
	mTranslateKeyMap[VK_OEM_7] = '\'';
	mTranslateKeyMap[VK_ESCAPE] = KEY_ESCAPE;
	mTranslateKeyMap[VK_RETURN] = KEY_RETURN;
	mTranslateKeyMap[VK_LEFT] = KEY_LEFT;
	mTranslateKeyMap[VK_RIGHT] = KEY_RIGHT;
	mTranslateKeyMap[VK_UP] = KEY_UP;
	mTranslateKeyMap[VK_DOWN] = KEY_DOWN;
	mTranslateKeyMap[VK_BACK] = KEY_BACKSPACE;
	mTranslateKeyMap[VK_INSERT] = KEY_INSERT;
	mTranslateKeyMap[VK_DELETE] = KEY_DELETE;
	mTranslateKeyMap[VK_SHIFT] = KEY_SHIFT;
	mTranslateKeyMap[VK_CONTROL] = KEY_CONTROL;
	mTranslateKeyMap[VK_MENU] = KEY_ALT;
	mTranslateKeyMap[VK_CAPITAL] = KEY_CAPSLOCK;
	mTranslateKeyMap[VK_HOME] = KEY_HOME;
	mTranslateKeyMap[VK_END] = KEY_END;
	mTranslateKeyMap[VK_PRIOR] = KEY_PAGE_UP;
	mTranslateKeyMap[VK_NEXT] = KEY_PAGE_DOWN;
	mTranslateKeyMap[VK_TAB] = KEY_TAB;
	mTranslateKeyMap[VK_ADD] = KEY_ADD;
	mTranslateKeyMap[VK_SUBTRACT] = KEY_SUBTRACT;
	mTranslateKeyMap[VK_MULTIPLY] = KEY_MULTIPLY;
	mTranslateKeyMap[VK_DIVIDE] = KEY_DIVIDE;
	mTranslateKeyMap[VK_F1] = KEY_F1;
	mTranslateKeyMap[VK_F2] = KEY_F2;
	mTranslateKeyMap[VK_F3] = KEY_F3;
	mTranslateKeyMap[VK_F4] = KEY_F4;
	mTranslateKeyMap[VK_F5] = KEY_F5;
	mTranslateKeyMap[VK_F6] = KEY_F6;
	mTranslateKeyMap[VK_F7] = KEY_F7;
	mTranslateKeyMap[VK_F8] = KEY_F8;
	mTranslateKeyMap[VK_F9] = KEY_F9;
	mTranslateKeyMap[VK_F10] = KEY_F10;
	mTranslateKeyMap[VK_F11] = KEY_F11;
	mTranslateKeyMap[VK_F12] = KEY_F12;
	mTranslateKeyMap[VK_CLEAR] = KEY_PAD_CENTER;

	// Also translate numeric and operator pad keys into normal numeric and
	// character keys (especially useful in menu accelerators for AZERTY
	// keyboards where numeric keys are SHIFTed keys) 
	mTranslateKeyMap[VK_NUMPAD0] = '0';
	mTranslateKeyMap[VK_NUMPAD1] = '1';
	mTranslateKeyMap[VK_NUMPAD2] = '2';
	mTranslateKeyMap[VK_NUMPAD3] = '3';
	mTranslateKeyMap[VK_NUMPAD4] = '4';
	mTranslateKeyMap[VK_NUMPAD5] = '5';
	mTranslateKeyMap[VK_NUMPAD6] = '6';
	mTranslateKeyMap[VK_NUMPAD7] = '7';
	mTranslateKeyMap[VK_NUMPAD8] = '8';
	mTranslateKeyMap[VK_NUMPAD9] = '9';
	mTranslateKeyMap[VK_MULTIPLY] = '*';
	mTranslateKeyMap[VK_ADD] = '+';
	mTranslateKeyMap[VK_SUBTRACT] = '-';
	mTranslateKeyMap[VK_DECIMAL] = '.';
	mTranslateKeyMap[VK_DIVIDE] = '/';

	// Build inverse map
	std::map<U32, KEY>::iterator iter;
	for (iter = mTranslateKeyMap.begin(); iter != mTranslateKeyMap.end();
		 ++iter)
	{
		mInvTranslateKeyMap[iter->second] = iter->first;
	}

	// Numpad map
	mTranslateNumpadMap[VK_NUMPAD0] = KEY_PAD_INS;
	mTranslateNumpadMap[VK_NUMPAD1] = KEY_PAD_END;
	mTranslateNumpadMap[VK_NUMPAD2] = KEY_PAD_DOWN;
	mTranslateNumpadMap[VK_NUMPAD3] = KEY_PAD_PGDN;
	mTranslateNumpadMap[VK_NUMPAD4] = KEY_PAD_LEFT;
	mTranslateNumpadMap[VK_NUMPAD5] = KEY_PAD_CENTER;
	mTranslateNumpadMap[VK_NUMPAD6] = KEY_PAD_RIGHT;
	mTranslateNumpadMap[VK_NUMPAD7] = KEY_PAD_HOME;
	mTranslateNumpadMap[VK_NUMPAD8] = KEY_PAD_UP;
	mTranslateNumpadMap[VK_NUMPAD9] = KEY_PAD_PGUP;
	mTranslateNumpadMap[VK_MULTIPLY] = KEY_PAD_MULTIPLY;
	mTranslateNumpadMap[VK_ADD] = KEY_PAD_ADD;
	mTranslateNumpadMap[VK_SUBTRACT] = KEY_PAD_SUBTRACT;
	mTranslateNumpadMap[VK_DECIMAL] = KEY_PAD_DEL;
	mTranslateNumpadMap[VK_DIVIDE] = KEY_PAD_DIVIDE;

	for (iter = mTranslateNumpadMap.begin(); iter != mTranslateNumpadMap.end();
		 ++iter)
	{
		mInvTranslateNumpadMap[iter->second] = iter->first;
	}
}

// Asynchronously poll the control, alt and shift keys and set the appropriate
// states. Note: this does not generate edges.
//virtual
void LLKeyboardWin32::resetMaskKeys()
{
	// GetAsyncKeyState returns a short and uses the most significant bit to
	// indicate that the key is down.
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
	{
		mKeyLevel[KEY_SHIFT] = true;
	}

	if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
	{
		mKeyLevel[KEY_CONTROL] = true;
	}

	if (GetAsyncKeyState(VK_MENU) & 0x8000)
	{
		mKeyLevel[KEY_ALT] = true;
	}
}

#if 0
void LLKeyboardWin32::setModifierKeyLevel(KEY key, bool new_state)
{
	if (mKeyLevel[key] != new_state)
	{
		mKeyLevelFrameCount[key] = 0;

		if (new_state)
		{
			mKeyLevelTimer[key].reset();
		}
		mKeyLevel[key] = new_state;
	}
}
#endif

MASK LLKeyboardWin32::updateModifiers()
{
	// Used at the login screen, for warning about caps lock on in password
	// field. HB
	// Low order bit carries the toggle state.
	mKeyLevel[KEY_CAPSLOCK] = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

	// Get mask for keyboard events
	return currentMask(false);
}

// mask is ignored, except for extended flag: we poll the modifier keys for the
// other flags
//virtual
bool LLKeyboardWin32::handleKeyDown(U32 key, MASK mask)
{
	U32 translated_mask = updateModifiers();

	KEY translated_key;
	if (translateExtendedKey(key, mask, &translated_key, translated_mask))
	{
		return handleTranslatedKeyDown(translated_key, translated_mask);
	}

	return false;
}

// mask is ignored, except for extended flag: we poll the modifier keys for the
// other flags
//virtual
bool LLKeyboardWin32::handleKeyUp(U32 key, MASK mask)
{
	U32 translated_mask = updateModifiers();

	KEY translated_key;
	if (translateExtendedKey(key, mask, &translated_key, translated_mask))
	{
		U32 translated_mask = updateModifiers();
		return handleTranslatedKeyUp(translated_key, translated_mask);
	}

	return false;
}

//virtual
MASK LLKeyboardWin32::currentMask(bool)
{
	MASK mask = MASK_NONE;
	if (mKeyLevel[KEY_SHIFT])
	{
		mask |= MASK_SHIFT;
	}
	if (mKeyLevel[KEY_CONTROL])
	{
		mask |= MASK_CONTROL;
	}
	if (mKeyLevel[KEY_ALT])
	{
		mask |= MASK_ALT;
	}
	return mask;
}

//virtual
void LLKeyboardWin32::scanKeyboard()
{
	MSG	msg;
	bool pending_key_events = (bool)PeekMessage(&msg, NULL,
												WM_KEYFIRST, WM_KEYLAST,
												PM_NOREMOVE | PM_NOYIELD);
	for (S32 key = 0; key < KEY_COUNT; ++key)
	{
		// On Windows, verify key down state. JC
		// RN: only do this if we do not have further key events in the queue
		// as otherwise there might be key repeat events still waiting for this
		// key we are now dumping
		if (!pending_key_events && mKeyLevel[key])
		{
			// *TODO: I KNOW there must be a better way of interrogating the
			// key state than this, using async key state can cause ALL kinds
			// of bugs - Doug
			if (key < KEY_BUTTON0 && (key < '0' || key > '9'))
			{
				// ...under windows make sure the key actually still is down.
				// ...translate back to windows key
				U32 virtual_key = inverseTranslateExtendedKey(key);
				// keydown in highest bit
				if (!pending_key_events &&
					!(GetAsyncKeyState(virtual_key) & 0x8000))
				{
					mKeyLevel[key] = false;
				}
			}
		}

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

bool LLKeyboardWin32::translateExtendedKey(U32 os_key, MASK mask,
										   KEY* translated_key,
										   MASK translated_mask)
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

	if (!translateKey(os_key, translated_key, translated_mask))
	{
		return false;
	}

	if (mNumpadDistinct != ND_NEVER)
	{
		if (mask & MASK_EXTENDED)
		{
			// This is where we would create new keycodes for extended keys
			// the set of extended keys includes the 'normal' arrow keys and
			// the pgup/Down/insert/home/end/delete cluster above the arrow
			// keys see:
			// http://windowssdk.msdn.microsoft.com/en-us/library/ms646280.aspx

			// Only process the return key if numlock is off
			if (((mNumpadDistinct == ND_NUMLOCK_OFF &&
				 !(GetKeyState(VK_NUMLOCK) & 1)) ||
				 mNumpadDistinct == ND_NUMLOCK_ON) &&
				*translated_key == KEY_RETURN)
			{
				*translated_key = KEY_PAD_RETURN;
			}
		}
		else
		{
			// The non-extended keys, those are in the numpad
			switch (*translated_key)
			{
				case KEY_LEFT:
					*translated_key = KEY_PAD_LEFT;
					break;

				case KEY_RIGHT:
					*translated_key = KEY_PAD_RIGHT;
					break;

				case KEY_UP:
					*translated_key = KEY_PAD_UP;
					break;

				case KEY_DOWN:
					*translated_key = KEY_PAD_DOWN;
					break;

				case KEY_HOME:
					*translated_key = KEY_PAD_HOME;
					break;

				case KEY_END:
					*translated_key = KEY_PAD_END;
					break;

				case KEY_PAGE_UP:
					*translated_key = KEY_PAD_PGUP;
					break;

				case KEY_PAGE_DOWN:
					*translated_key = KEY_PAD_PGDN;
					break;

				case KEY_INSERT:
					*translated_key = KEY_PAD_INS;
					break;

				case KEY_DELETE:
					*translated_key = KEY_PAD_DEL;
			}
		}
	}

	return true;
}

U32 LLKeyboardWin32::inverseTranslateExtendedKey(KEY translated_key)
{
	// If numlock is on, then we need to translate KEY_PAD_FOO to the
	// corresponding number pad number
	if (mNumpadDistinct == ND_NUMLOCK_ON && (GetKeyState(VK_NUMLOCK) & 1))
	{
		std::map<KEY, U32>::iterator iter;
		iter = mInvTranslateNumpadMap.find(translated_key);
		if (iter != mInvTranslateNumpadMap.end())
		{
			return iter->second;
		}
	}

	// If numlock is off or we are not converting numbers to arrows, we map our
	// keypad arrows to regular arrows since Windows does not distinguish
	// between them
	KEY converted_key = translated_key;
	switch (converted_key)
	{
		case KEY_PAD_LEFT:
			converted_key = KEY_LEFT; break;
		case KEY_PAD_RIGHT:
			converted_key = KEY_RIGHT; break;
		case KEY_PAD_UP:
			converted_key = KEY_UP; break;
		case KEY_PAD_DOWN:
			converted_key = KEY_DOWN; break;
		case KEY_PAD_HOME:
			converted_key = KEY_HOME; break;
		case KEY_PAD_END:
			converted_key = KEY_END; break;
		case KEY_PAD_PGUP:
			converted_key = KEY_PAGE_UP; break;
		case KEY_PAD_PGDN:
			converted_key = KEY_PAGE_DOWN; break;
		case KEY_PAD_INS:
			converted_key = KEY_INSERT; break;
		case KEY_PAD_DEL:
			converted_key = KEY_DELETE;
			break;
		case KEY_PAD_RETURN:
			converted_key = KEY_RETURN;
	}

	// Convert our virtual keys to OS keys
	return inverseTranslateKey(converted_key);
}

#endif
