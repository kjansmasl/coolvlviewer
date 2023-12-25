/**
 * @file llcoord.h
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

#ifndef LL_LLCOORD_H
#define LL_LLCOORD_H

#include "llpreprocessor.h"

// A two-dimensional pixel value
class LLCoord
{
public:
	LL_INLINE LLCoord() noexcept
	:	mX(0),
		mY(0)
	{
	}

	LL_INLINE LLCoord(S32 x, S32 y) noexcept
	:	mX(x),
		mY(y)
	{
	}

	// Allow the use of the default C++11 move constructor and assignation
	LLCoord(LLCoord&& other) noexcept = default;
	LLCoord& operator=(LLCoord&& other) noexcept = default;

	LLCoord(const LLCoord& other) = default;
	LLCoord& operator=(const LLCoord& other) = default;

	virtual ~LLCoord() = default;

	LL_INLINE virtual void set(S32 x, S32 y)	{ mX = x; mY = y; }

public:
	S32 mX;
	S32 mY;
};

// GL coordinates start in the client region of a window, with origin on bottom
// left of the screen.
class LLCoordGL : public LLCoord
{
public:
	LL_INLINE LLCoordGL() noexcept
	:	LLCoord()
	{
	}

	LL_INLINE LLCoordGL(S32 x, S32 y) noexcept
	:	LLCoord(x, y)
	{
	}

	// Allow the use of the default C++11 move constructor and assignation
	LLCoordGL(LLCoordGL&& other) noexcept = default;
	LLCoordGL& operator=(LLCoordGL&& other) noexcept = default;

	LLCoordGL(const LLCoordGL& other) = default;
	LLCoordGL& operator=(const LLCoordGL& other) = default;

	LL_INLINE bool operator==(const LLCoordGL& other) const
	{
		return mX == other.mX && mY == other.mY;
	}

	LL_INLINE bool operator!=(const LLCoordGL& other) const
	{
		return mX != other.mX || mY != other.mY;
	}
};

//bool operator ==(const LLCoordGL& a, const LLCoordGL& b);

// Window coords include things like window borders, menu regions, etc.
class LLCoordWindow : public LLCoord
{
public:
	LL_INLINE LLCoordWindow() noexcept
	:	LLCoord()
	{
	}

	LL_INLINE LLCoordWindow(S32 x, S32 y) noexcept
	:	LLCoord(x, y)
	{
	}

	// Allow the use of the default C++11 move constructor and assignation
	LLCoordWindow(LLCoordWindow&& other) noexcept = default;
	LLCoordWindow& operator=(LLCoordWindow&& other) noexcept = default;

	LLCoordWindow(const LLCoordWindow& other) = default;
	LLCoordWindow& operator=(const LLCoordWindow& other) = default;

	LL_INLINE bool operator==(const LLCoordWindow& other) const
	{
		return mX == other.mX && mY == other.mY;
	}

	LL_INLINE bool operator!=(const LLCoordWindow& other) const
	{
		return mX != other.mX || mY != other.mY;
	}
};

// Screen coords start at left, top = 0, 0
class LLCoordScreen : public LLCoord
{
public:
	LL_INLINE LLCoordScreen() noexcept
	:	LLCoord()
	{
	}

	LL_INLINE LLCoordScreen(S32 x, S32 y) noexcept
	:	LLCoord(x, y)
	{
	}

	// Allow the use of the default C++11 move constructor and assignation
	LLCoordScreen(LLCoordScreen&& other) noexcept = default;
	LLCoordScreen& operator=(LLCoordScreen&& other) noexcept = default;

	LLCoordScreen(const LLCoordScreen& other) = default;
	LLCoordScreen& operator=(const LLCoordScreen& other) = default;

	LL_INLINE bool operator==(const LLCoordScreen& other) const
	{
		return mX == other.mX && mY == other.mY;
	}

	LL_INLINE bool operator!=(const LLCoordScreen& other) const
	{
		return mX != other.mX || mY != other.mY;
	}
};

#endif
