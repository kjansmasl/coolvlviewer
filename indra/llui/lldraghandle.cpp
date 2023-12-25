/**
 * @file lldraghandle.cpp
 * @brief LLDragHandle base class
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

// A widget for dragging a view around the screen using the mouse.

#include "linden_common.h"

#include "lldraghandle.h"

#include "llcontrol.h"
#include "llmenugl.h"
#include "lltextbox.h"
#include "llwindow.h"

constexpr S32 LEADING_PAD = 5;
constexpr S32 TITLE_PAD = 8;
constexpr S32 BORDER_PAD = 1;
constexpr S32 LEFT_PAD = BORDER_PAD + TITLE_PAD + LEADING_PAD;
constexpr S32 RIGHT_PAD = BORDER_PAD + 32; // 32=space for close & minimize btn

LLDragHandle::LLDragHandle(const std::string& name, const LLRect& rect,
						   const std::string& title)
:	LLView(name, rect, true),
	mTitleBox(NULL),
	mClickedCallback(NULL),
	mCallbackUserData(NULL),
	mDragLastScreenX(0),
	mDragLastScreenY(0),
	mLastMouseScreenX(0),
	mLastMouseScreenY(0),
	mMaxTitleWidth(0),
	mForeground(true)
{
	setSaveToXML(false);
}

LLDragHandle::~LLDragHandle()
{
	setTitleBox(NULL);
}

void LLDragHandle::setTitleVisible(bool visible)
{
	if (mTitleBox)
	{
		mTitleBox->setVisible(visible);
	}
}

void LLDragHandle::setTitleBox(LLTextBox* titlebox)
{
	if (mTitleBox)
	{
		removeChild(mTitleBox);
		delete mTitleBox;
	}
	mTitleBox = titlebox;
	if (mTitleBox)
	{
		addChild(mTitleBox);
	}
}

LLDragHandleTop::LLDragHandleTop(const std::string& name, const LLRect &rect,
								 const std::string& title)
:	LLDragHandle(name, rect, title)
{
	mFont = LLFontGL::getFontSansSerif();
	setFollowsAll();
	setTitle(title);
}

LLDragHandleLeft::LLDragHandleLeft(const std::string& name, const LLRect &rect,
								   const std::string& title)
:	LLDragHandle(name, rect, title)
{
	setFollowsAll();
	setTitle(title);
}

void LLDragHandleTop::setTitle(const std::string& title)
{
	std::string trimmed_title = title;
	LLStringUtil::trim(trimmed_title);

	if (mTitleBox)
	{
		mTitleBox->setText(trimmed_title);
	}
	else
	{
		mTitleBox = new LLTextBox("Drag Handle Title", getRect(),
								  trimmed_title, mFont);
		mTitleBox->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT | FOLLOWS_RIGHT);
		mTitleBox->setFontStyle(LLFontGL::DROP_SHADOW_SOFT);
		addChild(mTitleBox);
	}

	reshapeTitleBox();
}

const std::string& LLDragHandleTop::getTitle() const
{
	return mTitleBox ? mTitleBox->getText() : LLStringUtil::null;
}

void LLDragHandleLeft::setTitle(const std::string&)
{
	setTitleBox(NULL);
	/* no title on left edge */
}

const std::string& LLDragHandleLeft::getTitle() const
{
	return LLStringUtil::null;
}

void LLDragHandleTop::draw()
{
	// Colorize the text to match the frontmost state
	if (mTitleBox)
	{
		mTitleBox->setEnabled(getForeground());
	}
	LLView::draw();
}

// Assumes GL state is set for 2D
void LLDragHandleLeft::draw()
{
	// Colorize the text to match the frontmost state
	if (mTitleBox)
	{
		mTitleBox->setEnabled(getForeground());
	}
	LLView::draw();
}

void LLDragHandleTop::reshapeTitleBox()
{
	if (!mTitleBox)
	{
		return;
	}
	S32 title_width = mFont->getWidth(mTitleBox->getText()) + TITLE_PAD;
	if (getMaxTitleWidth() > 0)
	{
		title_width = llmin(title_width, getMaxTitleWidth());
	}
	S32 title_height = ll_roundp(mFont->getLineHeight());
	LLRect title_rect;
	title_rect.setLeftTopAndSize(LEFT_PAD, getRect().getHeight() - BORDER_PAD,
								 getRect().getWidth() - LEFT_PAD - RIGHT_PAD,
								 title_height);

	mTitleBox->setRect(title_rect);
}

void LLDragHandleTop::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLView::reshape(width, height, called_from_parent);
	reshapeTitleBox();
}

void LLDragHandleLeft::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLView::reshape(width, height, called_from_parent);
}

//-------------------------------------------------------------
// UI event handling
//-------------------------------------------------------------

bool LLDragHandle::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Route future Mouse messages here preemptively (release on mouse up).
	// No handler needed for focus lost since this clas has no state that
	// depends on it.
	gFocusMgr.setMouseCapture(this);

	localPointToScreen(x, y, &mDragLastScreenX, &mDragLastScreenY);
	mLastMouseScreenX = mDragLastScreenX;
	mLastMouseScreenY = mDragLastScreenY;

	if (mClickedCallback && getSoundFlags() & MOUSE_DOWN)
	{
		make_ui_sound("UISndClick");
	}

	// Note: do not pass on to children
	return true;
}

bool LLDragHandle::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		// Release the mouse
		gFocusMgr.setMouseCapture(NULL);
	}

	if (mClickedCallback)
	{
		if (getSoundFlags() & MOUSE_UP)
		{
			make_ui_sound("UISndClickRelease");
		}
		// DO THIS AT THE VERY END to allow the handle to be destroyed as a
		// result of being clicked. If mouseup in the widget, it has been
		// clicked.
		(*mClickedCallback)(x, y, mCallbackUserData);
	}

	// Note: do not pass on to children
	return true;
}

bool LLDragHandle::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	// We only handle the click if the click both started and ended within us
	if (hasMouseCapture())
	{
		S32 screen_x;
		S32 screen_y;
		localPointToScreen(x, y, &screen_x, &screen_y);

		// Resize the parent
		S32 delta_x = screen_x - mDragLastScreenX;
		S32 delta_y = screen_y - mDragLastScreenY;

		LLRect original_rect = getParent()->getRect();
		LLRect translated_rect = getParent()->getRect();
		translated_rect.translate(delta_x, delta_y);
		// Temporarily slam dragged window to new position
		getParent()->setRect(translated_rect);
		S32 pre_snap_x = getParent()->getRect().mLeft;
		S32 pre_snap_y = getParent()->getRect().mBottom;
		mDragLastScreenX = screen_x;
		mDragLastScreenY = screen_y;

		LLRect new_rect;
		LLCoordGL mouse_dir;
		// Use hysteresis on mouse motion to preserve user intent when mouse
		// stops moving
		mouse_dir.mX = screen_x == mLastMouseScreenX ? mLastMouseDir.mX
													 : screen_x -
													   mLastMouseScreenX;
		mouse_dir.mY = screen_y == mLastMouseScreenY ? mLastMouseDir.mY
													 : screen_y -
													   mLastMouseScreenY;
		mLastMouseDir = mouse_dir;
		mLastMouseScreenX = screen_x;
		mLastMouseScreenY = screen_y;

		LLView* snap_view = getParent()->findSnapRect(new_rect, mouse_dir,
													  SNAP_PARENT_AND_SIBLINGS,
													  LLUI::sSnapMargin);

		getParent()->snappedTo(snap_view);
		delta_x = new_rect.mLeft - pre_snap_x;
		delta_y = new_rect.mBottom - pre_snap_y;
		translated_rect.translate(delta_x, delta_y);

		// Restore original rect so delta are detected, then call user reshape
		// method to handle snapped floaters, etc
		getParent()->setRect(original_rect);
		getParent()->userSetShape(translated_rect);

		mDragLastScreenX += delta_x;
		mDragLastScreenY += delta_y;

		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (active)" << LL_ENDL;
		handled = true;
	}
	else
	{
		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (inactive)" << LL_ENDL;
		handled = true;
	}

	// Note: do not pass on to children
	return handled;
}

void LLDragHandle::setValue(const LLSD& value)
{
	setTitle(value.asString());
}
