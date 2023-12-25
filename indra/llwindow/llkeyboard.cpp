/**
 * @file llkeyboard.cpp
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

#include "linden_common.h"

#include "indra_constants.h"

#include "llkeyboard.h"

#include "llstl.h"
#include "llwindow.h"

//
// Globals
//

LLKeyboard* gKeyboardp = NULL;

//static
std::map<KEY,std::string> LLKeyboard::sKeysToNames;
std::map<std::string,KEY> LLKeyboard::sNamesToKeys;

//
// Class Implementation
//

LLKeyboard::LLKeyboard()
:	mCallbacks(NULL),
	mNumpadDistinct(ND_NUMLOCK_OFF)
{
	// Constructor for LLTimer inits each timer. We want them to be constructed
	// without being initialized, so we shut them down here.
	for (S32 i = 0; i < KEY_COUNT; ++i)
	{
		mKeyLevelFrameCount[i] = 0;
		mKeyLevel[i] = false;
		mKeyUp[i] = false;
		mKeyDown[i] = false;
		mKeyRepeated[i] = false;
	}

	mInsertMode = LL_KIM_INSERT;
	mCurTranslatedKey = KEY_NONE;
	mCurScanKey = KEY_NONE;

	addKeyName(' ', "Space" );
	addKeyName(KEY_RETURN, "Enter" );
	addKeyName(KEY_LEFT, "Left" );
	addKeyName(KEY_RIGHT, "Right" );
	addKeyName(KEY_UP, "Up" );
	addKeyName(KEY_DOWN, "Down" );
	addKeyName(KEY_ESCAPE, "Esc" );
	addKeyName(KEY_HOME, "Home" );
	addKeyName(KEY_END, "End" );
	addKeyName(KEY_PAGE_UP, "PgUp" );
	addKeyName(KEY_PAGE_DOWN, "PgDn" );
	addKeyName(KEY_F1, "F1" );
	addKeyName(KEY_F2, "F2" );
	addKeyName(KEY_F3, "F3" );
	addKeyName(KEY_F4, "F4" );
	addKeyName(KEY_F5, "F5" );
	addKeyName(KEY_F6, "F6" );
	addKeyName(KEY_F7, "F7" );
	addKeyName(KEY_F8, "F8" );
	addKeyName(KEY_F9, "F9" );
	addKeyName(KEY_F10, "F10" );
	addKeyName(KEY_F11, "F11" );
	addKeyName(KEY_F12, "F12" );
	addKeyName(KEY_TAB, "Tab" );
	addKeyName(KEY_ADD, "Add" );
	addKeyName(KEY_SUBTRACT, "Subtract" );
	addKeyName(KEY_MULTIPLY, "Multiply" );
	addKeyName(KEY_DIVIDE, "Divide" );
	addKeyName(KEY_PAD_DIVIDE, "PAD_DIVIDE" );
	addKeyName(KEY_PAD_LEFT, "PAD_LEFT" );
	addKeyName(KEY_PAD_RIGHT, "PAD_RIGHT" );
	addKeyName(KEY_PAD_DOWN, "PAD_DOWN" );
	addKeyName(KEY_PAD_UP, "PAD_UP" );
	addKeyName(KEY_PAD_HOME, "PAD_HOME" );
	addKeyName(KEY_PAD_END, "PAD_END" );
	addKeyName(KEY_PAD_PGUP, "PAD_PGUP" );
	addKeyName(KEY_PAD_PGDN, "PAD_PGDN" );
	addKeyName(KEY_PAD_CENTER, "PAD_CENTER" );
	addKeyName(KEY_PAD_INS, "PAD_INS" );
	addKeyName(KEY_PAD_DEL, "PAD_DEL" );
	addKeyName(KEY_PAD_RETURN, "PAD_Enter" );
	addKeyName(KEY_BUTTON0, "PAD_BUTTON0" );
	addKeyName(KEY_BUTTON1, "PAD_BUTTON1" );
	addKeyName(KEY_BUTTON2, "PAD_BUTTON2" );
	addKeyName(KEY_BUTTON3, "PAD_BUTTON3" );
	addKeyName(KEY_BUTTON4, "PAD_BUTTON4" );
	addKeyName(KEY_BUTTON5, "PAD_BUTTON5" );
	addKeyName(KEY_BUTTON6, "PAD_BUTTON6" );
	addKeyName(KEY_BUTTON7, "PAD_BUTTON7" );
	addKeyName(KEY_BUTTON8, "PAD_BUTTON8" );
	addKeyName(KEY_BUTTON9, "PAD_BUTTON9" );
	addKeyName(KEY_BUTTON10, "PAD_BUTTON10" );
	addKeyName(KEY_BUTTON11, "PAD_BUTTON11" );
	addKeyName(KEY_BUTTON12, "PAD_BUTTON12" );
	addKeyName(KEY_BUTTON13, "PAD_BUTTON13" );
	addKeyName(KEY_BUTTON14, "PAD_BUTTON14" );
	addKeyName(KEY_BUTTON15, "PAD_BUTTON15" );

	addKeyName(KEY_BACKSPACE, "Backsp" );
	addKeyName(KEY_DELETE, "Del" );
	addKeyName(KEY_SHIFT, "Shift" );
	addKeyName(KEY_CONTROL, "Ctrl" );
	addKeyName(KEY_ALT, "Alt" );
	addKeyName(KEY_HYPHEN, "-" );
	addKeyName(KEY_EQUALS, "=" );
	addKeyName(KEY_INSERT, "Ins" );
	addKeyName(KEY_CAPSLOCK, "CapsLock" );
}

void LLKeyboard::addKeyName(KEY key, const std::string& name)
{
	sKeysToNames[key] = name;
	std::string nameuc = name;
	LLStringUtil::toUpper(nameuc);
	sNamesToKeys[nameuc] = key;
}

// BUG this has to be called when an OS dialog is shown, otherwise modifier key
// state is wrong because the keyup event is never received by the main window.
void LLKeyboard::resetKeys()
{
	for (S32 i = 0; i < KEY_COUNT; ++i)
	{
		if (mKeyLevel[i])
		{
			mKeyLevel[i] = false;
		}
	}

	for (S32 i = 0; i < KEY_COUNT; ++i)
	{
		mKeyUp[i] = false;
	}

	for (S32 i = 0; i < KEY_COUNT; ++i)
	{
		mKeyDown[i] = false;
	}

	for (S32 i = 0; i < KEY_COUNT; ++i)
	{
		mKeyRepeated[i] = false;
	}
}

bool LLKeyboard::translateKey(U32 os_key, KEY* out_key, MASK mask)
{
#if LL_LINUX || LL_WINDOWS
	// *HACK: translate AZERTY PC keyboards '²' key into its QWERTY equivalent
	// '`' key for accelerators/shortcuts (e.g. quick snapshot with CTRL `).
# if LL_LINUX
	if (os_key == 0xb2 && (mask & (MASK_CONTROL | MASK_ALT)))
# else	// LL_WINDOWS
	if (os_key == 0xde && (mask & (MASK_CONTROL | MASK_ALT)))
# endif
	{
		LL_DEBUGS("KeyCodes") << "Key code: " << std::hex << os_key
							  << " - Mask: " << mask << std::dec
							  << " - Translated key: 0x60" << LL_ENDL;
		*out_key = 0x60;
		return true;
	}
#endif

	// Only translate keys in the map, ignore all other keys for now
	std::map<U32, KEY>::iterator iter = mTranslateKeyMap.find(os_key);
	if (iter == mTranslateKeyMap.end())
	{
		*out_key = 0;
		LL_DEBUGS("KeyCodes") << "Unknown key code: 0x" << std::hex << os_key
							  << std::dec << LL_ENDL;
		return false;
	}

	*out_key = iter->second;
	LL_DEBUGS("KeyCodes") << "Key code: 0x" << std::hex << os_key
						  << " - Translation: 0x" << (U32)*out_key << std::dec
						  << LL_ENDL;
	return true;
}

U32 LLKeyboard::inverseTranslateKey(KEY translated_key)
{
	std::map<KEY, U32>::iterator iter;
	iter = mInvTranslateKeyMap.find(translated_key);
	if (iter == mInvTranslateKeyMap.end())
	{
		LL_DEBUGS("KeyCodes") << "Unknown translated key: 0x" << std::hex
							  << (U32)translated_key << std::dec << LL_ENDL;
		return 0;
	}

	LL_DEBUGS("KeyCodes") << "Translated key: 0x" << std::hex
						  << (U32)translated_key << " - Original key code: 0x"
						  << iter->second << std::dec << LL_ENDL;
	return iter->second;
}

bool LLKeyboard::handleTranslatedKeyDown(KEY translated_key,
										 U32 translated_mask)
{
	bool repeated = false;

	// Is this the first time the key went down ?
	// If so, generate "character" message
	if (!mKeyLevel[translated_key])
	{
		mKeyLevel[translated_key] = true;
		mKeyLevelTimer[translated_key].reset();
		mKeyLevelFrameCount[translated_key] = 0;
		mKeyRepeated[translated_key] = false;
	}
	else
	{
		// Level is already down, assume it is repeated.
		repeated = true;
		mKeyRepeated[translated_key] = true;
	}

	mKeyDown[translated_key] = true;
	mCurTranslatedKey = (KEY)translated_key;
	return mCallbacks->handleTranslatedKeyDown(translated_key, translated_mask,
											   repeated);
}

bool LLKeyboard::handleTranslatedKeyUp(KEY translated_key, U32 translated_mask)
{
	LL_DEBUGS("UserInput") << "keyup: " << translated_key << LL_ENDL;

	if (!mKeyLevel[translated_key])
	{
		return false;
	}

	mKeyLevel[translated_key] = false;

	// Only generate key up events if the key is thought to be down. This
	// allows you to call resetKeys() in the middle of a frame and ignore
	// subsequent KEY_UP messages in the same frame.  This was causing the
	// sequence W<return> in chat to move agents forward. JC
	mKeyUp[translated_key] = true;
	return mCallbacks->handleTranslatedKeyUp(translated_key, translated_mask);
}

void LLKeyboard::toggleInsertMode()
{
	if (LL_KIM_INSERT == mInsertMode)
	{
		mInsertMode = LL_KIM_OVERWRITE;
	}
	else
	{
		mInsertMode = LL_KIM_INSERT;
	}
}

// Returns time in seconds since key was pressed.
F32 LLKeyboard::getKeyElapsedTime(KEY key)
{
	return mKeyLevelTimer[key].getElapsedTimeF32();
}

// Returns time in frames since key was pressed.
S32 LLKeyboard::getKeyElapsedFrameCount(KEY key)
{
	return mKeyLevelFrameCount[key];
}

//static
bool LLKeyboard::keyFromString(const char* str, KEY* key)
{
	S32 length = strlen(str);
	if (length < 1)
	{
		return false;
	}
	if (length == 1)
	{
		const char ch = toupper(*str);
		if (('0' <= ch && ch <= '9') ||
			('A' <= ch && ch <= 'Z') ||
			('!' <= ch && ch <= '/') || // !"#$%&'()*+,-./
			(':' <= ch && ch <= '@') || // :;<=>?@
			('[' <= ch && ch <= '`') || // [\]^_`
			('{' <= ch && ch <= '~'))   // {|}~
		{
			*key = ch;
			return true;
		}
	}

	std::string instring(str);
	LLStringUtil::toUpper(instring);
	KEY res = get_if_there(sNamesToKeys, instring, (KEY)0);
	if (res != 0)
	{
		*key = res;
		return true;
	}

	llwarns << "Failed to convert string: " << str << llendl;
	return false;
}

//static
std::string LLKeyboard::stringFromKey(KEY key)
{
	std::string res = get_if_there(sKeysToNames, key, std::string());
	if (res.empty())
	{
		char buffer[2];
		buffer[0] = key;
		buffer[1] = '\0';
		res = std::string(buffer);
	}
	return res;
}

//static
bool LLKeyboard::maskFromString(const char* str, MASK* mask)
{
	if (!str || *str == '\0')
	{
		return false;
	}

	if (!strcmp(str, "NONE"))
	{
		*mask = MASK_NONE;
		return true;
	}
	if (!strcmp(str, "SHIFT"))
	{
		*mask = MASK_SHIFT;
		return true;
	}
	if (!strcmp(str, "CTL"))
	{
		*mask = MASK_CONTROL;
		return true;
	}
	if (!strcmp(str, "ALT"))
	{
		*mask = MASK_ALT;
		return true;
	}
	if (!strcmp(str, "CTL_SHIFT"))
	{
		*mask = MASK_CONTROL | MASK_SHIFT;
		return true;
	}
	if (!strcmp(str, "ALT_SHIFT"))
	{
		*mask = MASK_ALT | MASK_SHIFT;
		return true;
	}
	if (!strcmp(str, "CTL_ALT"))
	{
		*mask = MASK_CONTROL | MASK_ALT;
		return true;
	}
	if (!strcmp(str, "CTL_ALT_SHIFT"))
	{
		*mask = MASK_CONTROL | MASK_ALT | MASK_SHIFT;
		return true;
	}

	return false;
}
