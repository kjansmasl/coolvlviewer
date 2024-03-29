/**
 * @file llclipboard.h
 * @brief LLClipboard base class
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * Copyright (c) 2009-2023, Henri Beauchamp.
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

#ifndef LL_LLCLIPBOARD_H
#define LL_LLCLIPBOARD_H

#include "llstring.h"

// We support two flavors of clipboard. The default is the explicitly
// copy-and-pasted clipboard. The second is the so-called "primary" clipboard
// which is implicitly copied upon mouse selection (similarly to what happens
// under X11/Linux: the same behaviour is emulated, within the viewer text
// fields, for the other OSes: see in llwindow/llwindow.cpp).

class LLClipboard
{
public:
	LLClipboard() = default;

	void copyFromSubstring(const LLWString& text, S32 pos, S32 len);

	bool canPasteString() const;
	const LLWString& getPasteWString();

	void copyFromPrimarySubstring(const LLWString& text, S32 pos, S32 len);

	bool canPastePrimaryString() const;
	const LLWString& getPastePrimaryWString();

	LL_INLINE const LLWString& getClipBoardString()		{ return mString; }

private:
	LLWString mString;
};

// Global instance
extern LLClipboard gClipboard;

#endif  // LL_LLCLIPBOARD_H
