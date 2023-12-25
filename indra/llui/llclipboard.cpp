/**
 * @file llclipboard.cpp
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

#include "linden_common.h"

#include "llclipboard.h"

#include "llwindow.h"

// Global singleton
LLClipboard gClipboard;

void LLClipboard::copyFromSubstring(const LLWString& src, S32 pos, S32 len)
{
	if (pos >= 0 && pos < (S32)src.length())
	{
		mString = src.substr(pos, len);
	}
	else
	{
		mString.clear();
	}
	if (gWindowp)
	{
		gWindowp->copyTextToClipboard(mString);
	}
}

const LLWString& LLClipboard::getPasteWString()
{
	if (gWindowp)
	{
		gWindowp->pasteTextFromClipboard(mString);
	}
	return mString;
}

bool LLClipboard::canPasteString() const
{
	return gWindowp && gWindowp->isClipboardTextAvailable();
}

void LLClipboard::copyFromPrimarySubstring(const LLWString& src, S32 pos,
										   S32 len)
{
	if (pos >= 0 && pos < (S32)src.length())
	{
		mString = src.substr(pos, len);
	}
	else
	{
		mString.clear();
	}
	if (gWindowp)
	{
		gWindowp->copyTextToPrimary(mString);
	}
}

const LLWString& LLClipboard::getPastePrimaryWString()
{
	if (gWindowp)
	{
		gWindowp->pasteTextFromPrimary(mString);
	}
	return mString;
}

bool LLClipboard::canPastePrimaryString() const
{
	return gWindowp && gWindowp->isPrimaryTextAvailable();
}
