/**
 * @file llresizebar.h
 * @brief LLResizeBar base class
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

#ifndef LL_RESIZEBAR_H
#define LL_RESIZEBAR_H

#include "llview.h"
#include "llcoord.h"

class LLResizeBar : public LLView
{
public:
	enum Side { LEFT, TOP, RIGHT, BOTTOM };

	LLResizeBar(const std::string& name, LLView* resizing_viewp,
				const LLRect& rect, S32 min_size, S32 max_size, Side side);

	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;

	LL_INLINE void setResizeLimits(S32 min, S32 max)	{ mMinSize = min; mMaxSize = max; }
	LL_INLINE void setEnableSnapping(bool enable)		{ mSnappingEnabled = enable; }
	LL_INLINE void setAllowDoubleClickSnapping(bool ok)	{ mAllowDoubleClickSnapping = ok; }

	// Returns true when a resizing is in progress, or false otherwise. HB
	LL_INLINE bool resizing() const						{ return mResizing; }

private:
	LLView*			mResizingView;
	S32				mDragLastScreenX;
	S32				mDragLastScreenY;
	S32				mLastMouseScreenX;
	S32				mLastMouseScreenY;
	LLCoordGL		mLastMouseDir;
	S32				mMinSize;
	S32				mMaxSize;
	const Side		mSide;
	bool			mSnappingEnabled;
	bool			mAllowDoubleClickSnapping;
	bool			mResizing;
};

#endif  // LL_RESIZEBAR_H
