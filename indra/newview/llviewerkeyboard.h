/**
 * @file llviewerkeyboard.h
 * @brief LLViewerKeyboard class header file
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERKEYBOARD_H
#define LL_LLVIEWERKEYBOARD_H

#include "llkeyboard.h" // For EKeystate

constexpr S32 MAX_NAMED_FUNCTIONS = 100;
constexpr S32 MAX_KEY_BINDINGS = 128; // was 60

class LLNamedFunction
{
public:
	LLNamedFunction() : mFunction(NULL)	{}
	~LLNamedFunction()					{}

	std::string	mName;
	LLKeyFunc	mFunction;
};

typedef enum e_keyboard_mode
{
	MODE_FIRST_PERSON,
	MODE_THIRD_PERSON,
	MODE_EDIT,
	MODE_EDIT_AVATAR,
	MODE_SITTING,
	MODE_COUNT
} EKeyboardMode;

void bind_keyboard_functions();

class LLViewerKeyboard
{
protected:
	LOG_CLASS(LLViewerKeyboard);

public:
	LLViewerKeyboard();

	bool handleKey(KEY key, MASK mask, bool repeated);
	bool handleKeyUp(KEY key, MASK mask);

	void bindNamedFunction(const std::string& name, LLKeyFunc func);

	// Returns number bound, 0 on error
	S32 loadBindings(const std::string& filename);

	EKeyboardMode getMode();

	// false on failure
	bool modeFromString(const std::string& string, S32* mode);

	void scanKey(KEY key, bool key_down, bool key_up, bool key_level);

protected:
	bool bindKey(S32 mode, KEY key, MASK mask, const std::string& func_name);

protected:
	S32				mNamedFunctionCount;
	LLNamedFunction	mNamedFunctions[MAX_NAMED_FUNCTIONS];

	// Hold all the ugly stuff torn out to make LLKeyboard non-viewer-specific
	// here
	S32				mBindingCount[MODE_COUNT];
	LLKeyBinding	mBindings[MODE_COUNT][MAX_KEY_BINDINGS];

	typedef std::map<U32, U32> key_remap_t;
	key_remap_t		mRemapKeys[MODE_COUNT];
	std::set<KEY>	mKeysSkippedByUI;
	// Key processed successfully by UI
	bool			mKeyHandledByUI[KEY_COUNT];
};

extern LLViewerKeyboard gViewerKeyboard;

#endif // LL_LLVIEWERKEYBOARD_H
