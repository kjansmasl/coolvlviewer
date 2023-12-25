/**
 * @file llkeyboardsdl.cpp
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

#if LL_LINUX

#include "linden_common.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_keycode.h"

#include "llkeyboardsdl.h"

#include "llwindow.h"

LLKeyboardSDL::LLKeyboardSDL()
{
	// Set up key mapping for SDL - eventually can read this from a file ?
	// Anything not in the key map gets dropped
	// Add default A-Z

	// Virtual key mappings from SDL_keysym.h ...

	// SDL maps the letter keys to the ASCII you'd expect, but it's lowercase...
	U32 cur_char;
	for (cur_char = 'A'; cur_char <= 'Z'; cur_char++)
	{
		mTranslateKeyMap[cur_char] = cur_char;
	}
	for (cur_char = 'a'; cur_char <= 'z'; cur_char++)
	{
		mTranslateKeyMap[cur_char] = (cur_char - 'a') + 'A';
	}

	for (cur_char = '0'; cur_char <= '9'; cur_char++)
	{
		mTranslateKeyMap[cur_char] = cur_char;
	}

	mTranslateKeyMap[SDLK_RETURN] = KEY_RETURN;
	mTranslateKeyMap[SDLK_LEFT] = KEY_LEFT;
	mTranslateKeyMap[SDLK_RIGHT] = KEY_RIGHT;
	mTranslateKeyMap[SDLK_UP] = KEY_UP;
	mTranslateKeyMap[SDLK_DOWN] = KEY_DOWN;
	mTranslateKeyMap[SDLK_ESCAPE] = KEY_ESCAPE;
	mTranslateKeyMap[SDLK_KP_ENTER] = KEY_RETURN;
	mTranslateKeyMap[SDLK_ESCAPE] = KEY_ESCAPE;
	mTranslateKeyMap[SDLK_BACKSPACE] = KEY_BACKSPACE;
	mTranslateKeyMap[SDLK_DELETE] = KEY_DELETE;
	mTranslateKeyMap[SDLK_LSHIFT] = KEY_SHIFT;
	mTranslateKeyMap[SDLK_RSHIFT] = KEY_SHIFT;
	mTranslateKeyMap[SDLK_LCTRL] = KEY_CONTROL;
	mTranslateKeyMap[SDLK_RCTRL] = KEY_CONTROL;
	mTranslateKeyMap[SDLK_LALT] = KEY_ALT;
	mTranslateKeyMap[SDLK_RALT] = KEY_ALT;
	mTranslateKeyMap[SDLK_HOME] = KEY_HOME;
	mTranslateKeyMap[SDLK_END] = KEY_END;
	mTranslateKeyMap[SDLK_PAGEUP] = KEY_PAGE_UP;
	mTranslateKeyMap[SDLK_PAGEDOWN] = KEY_PAGE_DOWN;
	mTranslateKeyMap[SDLK_EQUALS] = KEY_EQUALS;
	mTranslateKeyMap[SDLK_KP_EQUALS] = KEY_EQUALS;
	mTranslateKeyMap[SDLK_INSERT] = KEY_INSERT;
	mTranslateKeyMap[SDLK_CAPSLOCK] = KEY_CAPSLOCK;
	mTranslateKeyMap[SDLK_TAB] = KEY_TAB;
	mTranslateKeyMap[SDLK_KP_PLUS] = KEY_ADD;
	mTranslateKeyMap[SDLK_KP_MINUS] = KEY_SUBTRACT;
	mTranslateKeyMap[SDLK_KP_MULTIPLY] = KEY_MULTIPLY;
	mTranslateKeyMap[SDLK_KP_DIVIDE] = KEY_PAD_DIVIDE;
	mTranslateKeyMap[SDLK_F1] = KEY_F1;
	mTranslateKeyMap[SDLK_F2] = KEY_F2;
	mTranslateKeyMap[SDLK_F3] = KEY_F3;
	mTranslateKeyMap[SDLK_F4] = KEY_F4;
	mTranslateKeyMap[SDLK_F5] = KEY_F5;
	mTranslateKeyMap[SDLK_F6] = KEY_F6;
	mTranslateKeyMap[SDLK_F7] = KEY_F7;
	mTranslateKeyMap[SDLK_F8] = KEY_F8;
	mTranslateKeyMap[SDLK_F9] = KEY_F9;
	mTranslateKeyMap[SDLK_F10] = KEY_F10;
	mTranslateKeyMap[SDLK_F11] = KEY_F11;
	mTranslateKeyMap[SDLK_F12] = KEY_F12;

	// Build inverse map
	std::map<U32, KEY>::iterator iter;
	for (iter = mTranslateKeyMap.begin(); iter != mTranslateKeyMap.end(); iter++)
	{
		mInvTranslateKeyMap[iter->second] = iter->first;
	}

	// Numpad map
	mTranslateNumpadMap[SDLK_KP_0] = KEY_PAD_INS;
	mTranslateNumpadMap[SDLK_KP_1] = KEY_PAD_END;
	mTranslateNumpadMap[SDLK_KP_2] = KEY_PAD_DOWN;
	mTranslateNumpadMap[SDLK_KP_3] = KEY_PAD_PGDN;
	mTranslateNumpadMap[SDLK_KP_4] = KEY_PAD_LEFT;
	mTranslateNumpadMap[SDLK_KP_5] = KEY_PAD_CENTER;
	mTranslateNumpadMap[SDLK_KP_6] = KEY_PAD_RIGHT;
	mTranslateNumpadMap[SDLK_KP_7] = KEY_PAD_HOME;
	mTranslateNumpadMap[SDLK_KP_8] = KEY_PAD_UP;
	mTranslateNumpadMap[SDLK_KP_9] = KEY_PAD_PGUP;
	mTranslateNumpadMap[SDLK_KP_PERIOD] = KEY_PAD_DEL;

	// Build inverse numpad map
	for (iter = mTranslateNumpadMap.begin();
	     iter != mTranslateNumpadMap.end();
	     iter++)
	{
		mInvTranslateNumpadMap[iter->second] = iter->first;
	}
}

// This mirrors the operation of the Windows version of resetMaskKeys(). It
// looks a quite bit suspicious, as it would not correct for keys that have
// been released. Is this the way it is supposed to work ?
//virtual
void LLKeyboardSDL::resetMaskKeys()
{
	SDL_Keymod mask = SDL_GetModState();

	if (mask & KMOD_SHIFT)
	{
		mKeyLevel[KEY_SHIFT] = true;
	}
	else
	{
		mKeyLevel[KEY_SHIFT] = false;
	}

	if (mask & KMOD_CTRL)
	{
		mKeyLevel[KEY_CONTROL] = true;
	}

	if (mask & KMOD_ALT)
	{
		mKeyLevel[KEY_ALT] = true;
	}
}

MASK LLKeyboardSDL::updateModifiers(U32 mask)
{
	// translate the mask
	MASK out_mask = MASK_NONE;

	if (mask & KMOD_SHIFT)
	{
		out_mask |= MASK_SHIFT;
	}

	if (mask & KMOD_CTRL)
	{
		out_mask |= MASK_CONTROL;
	}

	if (mask & KMOD_ALT)
	{
		out_mask |= MASK_ALT;
	}

	// Used at the login screen, for warning about caps lock on in password
	// field. HB
	if (mask & KMOD_CAPS)
	{
		mKeyLevel[KEY_CAPSLOCK] = true;
	}
	else
	{
		mKeyLevel[KEY_CAPSLOCK] = false;
	}

	return out_mask;
}

static U32 adjustNativekeyFromUnhandledMask(U32 key, U32 mask)
{
	// SDL does not automatically adjust the keysym according to whether
	// NUMLOCK is engaged, so we manage the keysym manually.
	// Also translate numeric and operator pad keys into normal numeric and
	// character keys (especially useful in menu accelerators for AZERTY
	// keyboards where numeric keys are SHIFTed keys) 
	U32 rtn = key;
	if (mask & KMOD_NUM)
	{
		switch (key)
		{
			case SDLK_KP_DIVIDE: rtn = SDLK_SLASH; break;
			case SDLK_KP_MULTIPLY: rtn = SDLK_ASTERISK; break;
			case SDLK_KP_MINUS: rtn = SDLK_MINUS; break;
			case SDLK_KP_PLUS: rtn = SDLK_PLUS; break;
			case SDLK_KP_EQUALS: rtn = SDLK_EQUALS; break;
			case SDLK_KP_0: rtn = SDLK_0; break;
			case SDLK_KP_1: rtn = SDLK_1; break;
			case SDLK_KP_2: rtn = SDLK_2; break;
			case SDLK_KP_3: rtn = SDLK_3; break;
			case SDLK_KP_4: rtn = SDLK_4; break;
			case SDLK_KP_5: rtn = SDLK_5; break;
			case SDLK_KP_6: rtn = SDLK_6; break;
			case SDLK_KP_7: rtn = SDLK_7; break;
			case SDLK_KP_8: rtn = SDLK_8; break;
			case SDLK_KP_9: rtn = SDLK_9;
		}
	}
	else
	{
		switch (key)
		{
			case SDLK_KP_DIVIDE: rtn = SDLK_SLASH; break;
			case SDLK_KP_MULTIPLY: rtn = SDLK_ASTERISK; break;
			case SDLK_KP_MINUS: rtn = SDLK_MINUS; break;
			case SDLK_KP_PLUS: rtn = SDLK_PLUS; break;
			case SDLK_KP_EQUALS: rtn = SDLK_EQUALS; break;
			case SDLK_KP_PERIOD: rtn = SDLK_DELETE; break;
			case SDLK_KP_0: rtn = SDLK_INSERT; break;
			case SDLK_KP_1: rtn = SDLK_END; break;
			case SDLK_KP_2: rtn = SDLK_DOWN; break;
			case SDLK_KP_3: rtn = SDLK_PAGEDOWN; break;
			case SDLK_KP_4: rtn = SDLK_LEFT; break;
			case SDLK_KP_6: rtn = SDLK_RIGHT; break;
			case SDLK_KP_7: rtn = SDLK_HOME; break;
			case SDLK_KP_8: rtn = SDLK_UP; break;
			case SDLK_KP_9: rtn = SDLK_PAGEUP;
		}
	}
	return rtn;
}

//virtual
bool LLKeyboardSDL::handleKeyDown(U32 key, U32 mask)
{
	U32 translated_code = adjustNativekeyFromUnhandledMask(key, mask);
	U32	translated_mask = updateModifiers(mask);
	LL_DEBUGS("KeyCodes") << "Key code: " << std::hex << key
						  << " - Mask: "<< mask
						  << " Translated code: " << translated_code
						  << " Translated mask: " << translated_mask
						  << std::dec << LL_ENDL;

	KEY	translated_key = 0;
	if (translateNumpadKey(translated_code, &translated_key, translated_mask))
	{
		return handleTranslatedKeyDown(translated_key, translated_mask);
	}

	return false;
}

//virtual
bool LLKeyboardSDL::handleKeyUp(U32 key, U32 mask)
{
	U32 translated_code = adjustNativekeyFromUnhandledMask(key, mask);
	U32	translated_mask = updateModifiers(mask);

	KEY	translated_key = 0;
	if (translateNumpadKey(translated_code, &translated_key, translated_mask))
	{
		return handleTranslatedKeyUp(translated_key, translated_mask);
	}

	return false;
}

//virtual
MASK LLKeyboardSDL::currentMask(bool for_mouse_event)
{
	MASK result = MASK_NONE;
	SDL_Keymod mask = SDL_GetModState();

	if (mask & KMOD_SHIFT)
	{
		result |= MASK_SHIFT;
	}
	if (mask & KMOD_CTRL)
	{
		result |= MASK_CONTROL;
	}
	if (mask & KMOD_ALT)
	{
		result |= MASK_ALT;
	}

	// For keyboard events, consider Meta keys equivalent to Control
	if (!for_mouse_event && (mask & KMOD_GUI))
	{
		result |= MASK_CONTROL;
	}

	return result;
}

//virtual
void LLKeyboardSDL::scanKeyboard()
{
	for (S32 key = 0; key < KEY_COUNT; ++key)
	{
		// Generate callback if any event has occurred on this key this frame.
		// Can't just test mKeyLevel, because this could be a slow frame and
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

bool LLKeyboardSDL::translateNumpadKey(U32 os_key, KEY* translated_key,
									   MASK mask)
{
	if (mNumpadDistinct == ND_NUMLOCK_ON)
	{
		std::map<U32, KEY>::iterator iter= mTranslateNumpadMap.find(os_key);
		if (iter != mTranslateNumpadMap.end())
		{
			*translated_key = iter->second;
			return true;
		}
	}
	return translateKey(os_key, translated_key, mask);
}

U32 LLKeyboardSDL::inverseTranslateNumpadKey(KEY translated_key)
{
	if (mNumpadDistinct == ND_NUMLOCK_ON)
	{
		std::map<KEY, U32>::iterator iter= mInvTranslateNumpadMap.find(translated_key);
		if (iter != mInvTranslateNumpadMap.end())
		{
			return iter->second;
		}
	}
	return inverseTranslateKey(translated_key);
}

#endif // LL_LINUX
