/**
 * @file llresizehandle.h
 * @brief LLResizeHandle base class
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

#ifndef LL_RESIZEHANDLE_H
#define LL_RESIZEHANDLE_H

#include "stdtypes.h"
#include "llview.h"
#include "llimagegl.h"
#include "llcoord.h"

class LLResizeHandle : public LLView
{
public:
	enum ECorner { LEFT_TOP, LEFT_BOTTOM, RIGHT_TOP, RIGHT_BOTTOM };

	LLResizeHandle(const std::string& name, const LLRect& rect, S32 min_width,
				   S32 min_height, ECorner corner = RIGHT_BOTTOM );

	void draw() override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	void setResizeLimits(S32 min_width, S32 min_height)
	{
		mMinWidth = min_width;
		mMinHeight = min_height;
	}

	// Returns true when a resizing is in progress, or false otherwise. HB
	LL_INLINE bool resizing() const			{ return mResizing; }

private:
	bool pointInHandle(S32 x, S32 y);

private:
	S32						mDragLastScreenX;
	S32						mDragLastScreenY;
	S32						mLastMouseScreenX;
	S32						mLastMouseScreenY;
	LLCoordGL				mLastMouseDir;
	LLUIImagePtr			mImage;
	S32						mMinWidth;
	S32						mMinHeight;
	const ECorner			mCorner;
	bool					mResizing;
};

constexpr S32 RESIZE_HANDLE_HEIGHT = 16;
constexpr S32 RESIZE_HANDLE_WIDTH = 16;

#endif  // LL_RESIZEHANDLE_H
