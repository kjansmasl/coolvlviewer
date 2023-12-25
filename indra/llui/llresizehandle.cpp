/**
 * @file llresizehandle.cpp
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

#include "linden_common.h"

#include "llresizehandle.h"

#include "llcontrol.h"
#include "llfloater.h"
#include "llmenugl.h"
#include "llwindow.h"

constexpr S32 RESIZE_BORDER_WIDTH = 3;

LLResizeHandle::LLResizeHandle(const std::string& name, const LLRect& rect,
							   S32 min_width, S32 min_height, ECorner corner)
:	LLView(name, rect, true),
	mDragLastScreenX(0),
	mDragLastScreenY(0),
	mLastMouseScreenX(0),
	mLastMouseScreenY(0),
	mImage(NULL),
	mMinWidth(min_width),
	mMinHeight(min_height),
	mCorner(corner),
	mResizing(false)
{
	// This is a decorator object: never serialize it.
	setSaveToXML(false);

	if (RIGHT_BOTTOM == mCorner)
	{
		mImage = LLUI::getUIImage("UIImgResizeBottomRightUUID");
	}

	switch (mCorner)
	{
		case LEFT_TOP:
			setFollows(FOLLOWS_LEFT | FOLLOWS_TOP);
			break;

		case LEFT_BOTTOM:
			setFollows(FOLLOWS_LEFT | FOLLOWS_BOTTOM);
			break;

		case RIGHT_TOP:
			setFollows(FOLLOWS_RIGHT | FOLLOWS_TOP);
			break;

		case RIGHT_BOTTOM:
			setFollows(FOLLOWS_RIGHT | FOLLOWS_BOTTOM);
	}
}

bool LLResizeHandle::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (pointInHandle(x, y))
	{
		// Route future Mouse messages here preemptively (release on mouse up).
		// No handler needed for focus lost since this class has no state that
		// depends on it.
		gFocusMgr.setMouseCapture(this);

		localPointToScreen(x, y, &mDragLastScreenX, &mDragLastScreenY);
		mLastMouseScreenX = mDragLastScreenX;
		mLastMouseScreenY = mDragLastScreenY;
		return true;
	}
	return false;
}

bool LLResizeHandle::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mResizing = false;
	if (hasMouseCapture())
	{
		// Release the mouse
		gFocusMgr.setMouseCapture(NULL);
		return true;
	}
	if (pointInHandle(x, y))
	{
		return true;
	}
	return false;
}

bool LLResizeHandle::handleHover(S32 x, S32 y, MASK mask)
{
	mResizing = false;
	bool handled = false;

	// We only handle the click if the click both started and ended within us
	if (hasMouseCapture())
	{
		// Make sure the mouse in still over the application: we do not want to
		// make the parent so big that we cannot see the resize handle any
		// more.

		S32 screen_x;
		S32 screen_y;
		localPointToScreen(x, y, &screen_x, &screen_y);
		const LLRect valid_rect = getRootView()->getRect();
		screen_x = llclamp(screen_x, valid_rect.mLeft, valid_rect.mRight);
		screen_y = llclamp(screen_y, valid_rect.mBottom, valid_rect.mTop);

		LLView* resize_viewp = getParent();
		if (resize_viewp)
		{
			mResizing = true;

			// Resize the parent
			LLRect orig_rect = resize_viewp->getRect();
			LLRect scaled_rect = orig_rect;
			S32 delta_x = screen_x - mDragLastScreenX;
			S32 delta_y = screen_y - mDragLastScreenY;
			LLCoordGL mouse_dir;
			// Use hysteresis on mouse motion to preserve user intent when
			// mouse stops moving
			mouse_dir.mX =
				screen_x == mLastMouseScreenX ? mLastMouseDir.mX
											  : screen_x - mLastMouseScreenX;
			mouse_dir.mY =
				screen_y == mLastMouseScreenY ? mLastMouseDir.mY
											  : screen_y - mLastMouseScreenY;
			mLastMouseScreenX = screen_x;
			mLastMouseScreenY = screen_y;
			mLastMouseDir = mouse_dir;

			S32 x_multiple = 1;
			S32 y_multiple = 1;
			switch (mCorner)
			{
				case LEFT_TOP:
					x_multiple = -1;
					y_multiple =  1;
					break;

				case LEFT_BOTTOM:
					x_multiple = -1;
					y_multiple = -1;
					break;

				case RIGHT_TOP:
					x_multiple =  1;
					y_multiple =  1;
					break;

				case RIGHT_BOTTOM:
					x_multiple =  1;
					y_multiple = -1;
			}

			S32 new_width = orig_rect.getWidth() + x_multiple * delta_x;
			if (new_width < mMinWidth)
			{
				new_width = mMinWidth;
				delta_x = x_multiple * (mMinWidth - orig_rect.getWidth());
			}

			S32 new_height = orig_rect.getHeight() + y_multiple * delta_y;
			if (new_height < mMinHeight)
			{
				new_height = mMinHeight;
				delta_y = y_multiple * (mMinHeight - orig_rect.getHeight());
			}

			switch (mCorner)
			{
				case LEFT_TOP:
					scaled_rect.translate(delta_x, 0);
					break;

				case LEFT_BOTTOM:
					scaled_rect.translate(delta_x, delta_y);
					break;

				case RIGHT_TOP:
					break;

				case RIGHT_BOTTOM:
					scaled_rect.translate(0, delta_y);
			}

			// Temporarily set new parent rect
			scaled_rect.mRight = scaled_rect.mLeft + new_width;
			scaled_rect.mTop = scaled_rect.mBottom + new_height;
			resize_viewp->setRect(scaled_rect);

			LLView* snap_viewp = NULL;
			LLView* test_viewp = NULL;

			// Now do snapping
			switch (mCorner)
			{
				case LEFT_TOP:
					snap_viewp =
						resize_viewp->findSnapEdge(scaled_rect.mLeft,
													mouse_dir, SNAP_LEFT,
													SNAP_PARENT_AND_SIBLINGS,
													LLUI::sSnapMargin);
					test_viewp =
						resize_viewp->findSnapEdge(scaled_rect.mTop,
												   mouse_dir, SNAP_TOP,
												   SNAP_PARENT_AND_SIBLINGS,
												   LLUI::sSnapMargin);
					if (!snap_viewp)
					{
						snap_viewp = test_viewp;
					}
					break;

				case LEFT_BOTTOM:
					snap_viewp =
						resize_viewp->findSnapEdge(scaled_rect.mLeft,
												   mouse_dir, SNAP_LEFT,
												   SNAP_PARENT_AND_SIBLINGS,
												   LLUI::sSnapMargin);
					test_viewp =
						resize_viewp->findSnapEdge(scaled_rect.mBottom,
												   mouse_dir, SNAP_BOTTOM,
												   SNAP_PARENT_AND_SIBLINGS,
												   LLUI::sSnapMargin);
					if (!snap_viewp)
					{
						snap_viewp = test_viewp;
					}
					break;

				case RIGHT_TOP:
					snap_viewp =
						resize_viewp->findSnapEdge(scaled_rect.mRight,
												   mouse_dir, SNAP_RIGHT,
												   SNAP_PARENT_AND_SIBLINGS,
												   LLUI::sSnapMargin);
					test_viewp =
						resize_viewp->findSnapEdge(scaled_rect.mTop,
												   mouse_dir, SNAP_TOP,
												   SNAP_PARENT_AND_SIBLINGS,
												   LLUI::sSnapMargin);
					if (!snap_viewp)
					{
						snap_viewp = test_viewp;
					}
					break;

				case RIGHT_BOTTOM:
					snap_viewp =
						resize_viewp->findSnapEdge(scaled_rect.mRight,
												   mouse_dir, SNAP_RIGHT,
												   SNAP_PARENT_AND_SIBLINGS,
												   LLUI::sSnapMargin);
					test_viewp =
						resize_viewp->findSnapEdge(scaled_rect.mBottom,
												   mouse_dir, SNAP_BOTTOM,
												   SNAP_PARENT_AND_SIBLINGS,
												   LLUI::sSnapMargin);
					if (!snap_viewp)
					{
						snap_viewp = test_viewp;
					}
			}

			// Register "snap" behavior with snapped view
			resize_viewp->snappedTo(snap_viewp);

			// Reset parent rect
			resize_viewp->setRect(orig_rect);

			// Translate and scale to new shape
			resize_viewp->userSetShape(scaled_rect);

			// Update last valid mouse cursor position based on resized view's
			// actual size
			LLRect new_rect = resize_viewp->getRect();
			switch (mCorner)
			{
				case LEFT_TOP:
					mDragLastScreenX += new_rect.mLeft - orig_rect.mLeft;
					mDragLastScreenY += new_rect.mTop - orig_rect.mTop;
					break;

				case LEFT_BOTTOM:
					mDragLastScreenX += new_rect.mLeft - orig_rect.mLeft;
					mDragLastScreenY += new_rect.mBottom- orig_rect.mBottom;
					break;

				case RIGHT_TOP:
					mDragLastScreenX += new_rect.mRight - orig_rect.mRight;
					mDragLastScreenY += new_rect.mTop - orig_rect.mTop;
					break;

				case RIGHT_BOTTOM:
					mDragLastScreenX += new_rect.mRight - orig_rect.mRight;
					mDragLastScreenY += new_rect.mBottom- orig_rect.mBottom;

				default:
					break;
			}
		}

		handled = true;
	}
	// We do not have a mouse capture
	else if (pointInHandle(x, y))
	{
		handled = true;
	}

	if (handled)
	{
		switch (mCorner)
		{
			case LEFT_TOP:
			case RIGHT_BOTTOM:
				gWindowp->setCursor(UI_CURSOR_SIZENWSE);
				break;

			case LEFT_BOTTOM:
			case RIGHT_TOP:
				gWindowp->setCursor(UI_CURSOR_SIZENESW);
		}
	}

	return handled;
}

// Assumes GL state is set for 2D
void LLResizeHandle::draw()
{
	if (mCorner == RIGHT_BOTTOM && mImage.notNull() && getVisible())
	{
		mImage->draw(0, 0);
	}
}

bool LLResizeHandle::pointInHandle(S32 x, S32 y)
{
	if (pointInView(x, y))
	{
		switch (mCorner)
		{
			case LEFT_TOP:
				return x <= RESIZE_BORDER_WIDTH ||
					   y >= getRect().getHeight() - RESIZE_BORDER_WIDTH;

			case LEFT_BOTTOM:
				return x <= RESIZE_BORDER_WIDTH || y <= RESIZE_BORDER_WIDTH;

			case RIGHT_TOP:
				return x >= getRect().getWidth() - RESIZE_BORDER_WIDTH ||
					   y >= getRect().getHeight() - RESIZE_BORDER_WIDTH;

			case RIGHT_BOTTOM:
				return true;
		}
	}
	return false;
}
