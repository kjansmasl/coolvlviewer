/**
 * @file llscrollcontainer.cpp
 * @brief LLScrollableContainer base class
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

#include "llscrollcontainer.h"

#include "llframetimer.h"
#include "llkeyboard.h"
#include "llrender.h"
#include "lluictrlfactory.h"
#include "llviewborder.h"

static constexpr S32 HORIZONTAL_MULTIPLE = 8;
static constexpr S32 VERTICAL_MULTIPLE = 16;
static constexpr F32 MIN_AUTO_SCROLL_RATE = 120.f;
static constexpr F32 MAX_AUTO_SCROLL_RATE = 500.f;
static constexpr F32 AUTO_SCROLL_RATE_ACCEL = 120.f;

static const std::string LL_SCROLLABLE_CONTAINER_VIEW_TAG = "scroll_container";
static LLRegisterWidget<LLScrollableContainer>
	r18(LL_SCROLLABLE_CONTAINER_VIEW_TAG);

// Default constructor
LLScrollableContainer::LLScrollableContainer(const std::string& name,
											 const LLRect& rect,
											 LLView* scrolled_view,
											 bool is_opaque,
											 const LLColor4& bg_color)
:	LLUICtrl(name, rect, false, NULL, NULL),
	mScrolledView(scrolled_view),
	mIsOpaque(is_opaque),
	mBackgroundColor(bg_color),
	mReserveScrollCorner(false),
	mAutoScrolling(false),
	mAutoScrollRate(0.f)
{
	if (mScrolledView)
	{
		addChild(mScrolledView);
	}

	init();
}

// LLUICtrl constructor
LLScrollableContainer::LLScrollableContainer(const std::string& name,
											 const LLRect& rect,
											 LLUICtrl* scrolled_ctrl,
											 bool is_opaque,
											 const LLColor4& bg_color)
:	LLUICtrl(name, rect, false, NULL, NULL),
	mScrolledView(scrolled_ctrl),
	mIsOpaque(is_opaque),
	mBackgroundColor(bg_color),
	mReserveScrollCorner(false),
	mAutoScrolling(false),
	mAutoScrollRate(0.f)
{
	if (scrolled_ctrl)
	{
		addChild(scrolled_ctrl);
	}

	init();
}

void LLScrollableContainer::init()
{
	LLRect border_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
	mBorder = new LLViewBorder("scroll border", border_rect,
							   LLViewBorder::BEVEL_IN);
	addChild(mBorder);

	mInnerRect.set(0, getRect().getHeight(), getRect().getWidth(), 0);
	mInnerRect.stretch(-getBorderWidth());

	LLRect vertical_scroll_rect = mInnerRect;
	vertical_scroll_rect.mLeft = vertical_scroll_rect.mRight - SCROLLBAR_SIZE;
	mScrollbar[VERTICAL] = new LLScrollbar("scrollable vertical",
										   vertical_scroll_rect,
										   LLScrollbar::VERTICAL,
										   mInnerRect.getHeight(), 0,
										   mInnerRect.getHeight(), NULL, this,
										   VERTICAL_MULTIPLE);
	addChild(mScrollbar[VERTICAL]);
	mScrollbar[VERTICAL]->setVisible(false);
	mScrollbar[VERTICAL]->setFollowsRight();
	mScrollbar[VERTICAL]->setFollowsTop();
	mScrollbar[VERTICAL]->setFollowsBottom();

	LLRect horizontal_scroll_rect = mInnerRect;
	horizontal_scroll_rect.mTop =
		horizontal_scroll_rect.mBottom + SCROLLBAR_SIZE;
	mScrollbar[HORIZONTAL] = new LLScrollbar("scrollable horizontal",
											 horizontal_scroll_rect,
											 LLScrollbar::HORIZONTAL,
											 mInnerRect.getWidth(), 0,
											 mInnerRect.getWidth(), NULL,
											 this, HORIZONTAL_MULTIPLE);
	addChild(mScrollbar[HORIZONTAL]);
	mScrollbar[HORIZONTAL]->setVisible(false);
	mScrollbar[HORIZONTAL]->setFollowsLeft();
	mScrollbar[HORIZONTAL]->setFollowsRight();

	setTabStop(false);
}

LLScrollableContainer::~LLScrollableContainer()
{
	// mScrolledView and mScrollbar are child views, so the LLView destructor
	// takes care of memory deallocation.
	for (S32 i = 0; i < SCROLLBAR_COUNT; ++i)
	{
		mScrollbar[i] = NULL;
	}
	mScrolledView = NULL;
}

// Internal scrollbar handlers
//virtual
void LLScrollableContainer::scrollHorizontal(S32 new_pos)
{
	if (mScrolledView)
	{
		LLRect doc_rect = mScrolledView->getRect();
		S32 old_pos = -(doc_rect.mLeft - mInnerRect.mLeft);
		mScrolledView->translate(-(new_pos - old_pos), 0);
	}
}

//virtual
void LLScrollableContainer::scrollVertical(S32 new_pos)
{
	if (mScrolledView)
	{
		LLRect doc_rect = mScrolledView->getRect();
		S32 old_pos = doc_rect.mTop - mInnerRect.mTop;
		mScrolledView->translate(0, new_pos - old_pos);
	}
}

// LLView functionality
void LLScrollableContainer::reshape(S32 width, S32 height,
										bool called_from_parent)
{
	LLUICtrl::reshape(width, height, called_from_parent);

	mInnerRect.set(0, getRect().getHeight(), getRect().getWidth(), 0);
	mInnerRect.stretch(-getBorderWidth());

	if (mScrolledView)
	{
		const LLRect& scrolled_rect = mScrolledView->getRect();

		S32 visible_width = 0;
		S32 visible_height = 0;
		bool show_v_scrollbar = false;
		bool show_h_scrollbar = false;
		calcVisibleSize(scrolled_rect, &visible_width, &visible_height,
						&show_h_scrollbar, &show_v_scrollbar);

		mScrollbar[VERTICAL]->setDocSize(scrolled_rect.getHeight());
		mScrollbar[VERTICAL]->setPageSize(visible_height);

		mScrollbar[HORIZONTAL]->setDocSize(scrolled_rect.getWidth());
		mScrollbar[HORIZONTAL]->setPageSize(visible_width);
		updateScroll();
	}
}

bool LLScrollableContainer::handleKeyHere(KEY key, MASK mask)
{
	for (S32 i = 0; i < SCROLLBAR_COUNT; ++i)
	{
		if (mScrollbar[i] && mScrollbar[i]->handleKeyHere(key, mask))
		{
			return true;
		}
	}

	return false;
}

// Note: tries vertical and then horizontal
bool LLScrollableContainer::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	for (S32 i = 0; i < SCROLLBAR_COUNT; ++i)
	{
		// Pretend the mouse is over the scrollbar
		if (mScrollbar[i] && mScrollbar[i]->handleScrollWheel(0, 0, clicks))
		{
			return true;
		}
	}

	// Eat scroll wheel event (to avoid scrolling nested containers ?)
	return true;
}

bool LLScrollableContainer::needsToScroll(S32 x, S32 y,
										  SCROLL_ORIENTATION axis) const
{
	if (mScrollbar[axis] && mScrollbar[axis]->getVisible())
	{
		LLRect inner_rect_local(0, mInnerRect.getHeight(),
								mInnerRect.getWidth(), 0);
		constexpr S32 AUTOSCROLL_SIZE = 10;
		if (mScrollbar[axis]->getVisible())
		{
			inner_rect_local.mRight -= SCROLLBAR_SIZE;
			inner_rect_local.mTop += AUTOSCROLL_SIZE;
			inner_rect_local.mBottom = inner_rect_local.mTop - AUTOSCROLL_SIZE;
		}
		if (inner_rect_local.pointInRect(x, y) &&
			mScrollbar[axis]->getDocPos() > 0)
		{
			return true;
		}

	}
	return false;
}

bool LLScrollableContainer::handleDragAndDrop(S32 x, S32 y, MASK mask,
											  bool drop,
											  EDragAndDropType cargo_type,
											  void* cargo_data,
											  EAcceptance* accept,
											  std::string& tooltip_msg)
{
	// Scroll folder view if needed. Never accepts a drag or drop.
	*accept = ACCEPT_NO;
	bool handled = false;
	if (mScrollbar[HORIZONTAL] && mScrollbar[VERTICAL] &&
		(mScrollbar[HORIZONTAL]->getVisible() ||
		 mScrollbar[VERTICAL]->getVisible()))
	{
		constexpr S32 AUTOSCROLL_SIZE = 10;
		S32 auto_scroll_speed =
			ll_roundp(mAutoScrollRate * LLFrameTimer::getFrameDeltaTimeF32());

		LLRect inner_rect_local(0, mInnerRect.getHeight(),
								mInnerRect.getWidth(), 0);
		if (mScrollbar[HORIZONTAL]->getVisible())
		{
			inner_rect_local.mBottom += SCROLLBAR_SIZE;
		}
		if (mScrollbar[VERTICAL]->getVisible())
		{
			inner_rect_local.mRight -= SCROLLBAR_SIZE;
		}

		if (mScrollbar[HORIZONTAL]->getVisible())
		{
			LLRect left_scroll_rect = inner_rect_local;
			left_scroll_rect.mRight = AUTOSCROLL_SIZE;
			if (left_scroll_rect.pointInRect(x, y) &&
				mScrollbar[HORIZONTAL]->getDocPos() > 0)
			{
				mScrollbar[HORIZONTAL]->setDocPos(mScrollbar[HORIZONTAL]->getDocPos() -
												  auto_scroll_speed);
				mAutoScrolling = true;
				handled = true;
			}

			LLRect right_scroll_rect = inner_rect_local;
			right_scroll_rect.mLeft = inner_rect_local.mRight - AUTOSCROLL_SIZE;
			if (right_scroll_rect.pointInRect(x, y) &&
				mScrollbar[HORIZONTAL]->getDocPos() <
					mScrollbar[HORIZONTAL]->getDocPosMax())
			{
				mScrollbar[HORIZONTAL]->setDocPos(mScrollbar[HORIZONTAL]->getDocPos() +
												  auto_scroll_speed);
				mAutoScrolling = true;
				handled = true;
			}
		}
		if (mScrollbar[VERTICAL]->getVisible())
		{
			LLRect bottom_scroll_rect = inner_rect_local;
			bottom_scroll_rect.mTop = AUTOSCROLL_SIZE + bottom_scroll_rect.mBottom;
			if (bottom_scroll_rect.pointInRect(x, y) &&
				mScrollbar[VERTICAL]->getDocPos() <
					mScrollbar[VERTICAL]->getDocPosMax())
			{
				mScrollbar[VERTICAL]->setDocPos(mScrollbar[VERTICAL]->getDocPos() +
												auto_scroll_speed);
				mAutoScrolling = true;
				handled = true;
			}

			LLRect top_scroll_rect = inner_rect_local;
			top_scroll_rect.mBottom = inner_rect_local.mTop - AUTOSCROLL_SIZE;
			if (top_scroll_rect.pointInRect(x, y) &&
				mScrollbar[VERTICAL]->getDocPos() > 0)
			{
				mScrollbar[VERTICAL]->setDocPos(mScrollbar[VERTICAL]->getDocPos() -
												auto_scroll_speed);
				mAutoScrolling = true;
				handled = true;
			}
		}
	}

	if (!handled)
	{
		handled = childrenHandleDragAndDrop(x, y, mask, drop, cargo_type,
											cargo_data, accept, tooltip_msg) != NULL;
	}

	return true;
}

bool LLScrollableContainer::handleToolTip(S32 x, S32 y, std::string& msg,
										  LLRect* sticky_rect)
{
	S32 local_x, local_y;
	for (S32 i = 0; i < SCROLLBAR_COUNT; ++i)
	{
		local_x = x - mScrollbar[i]->getRect().mLeft;
		local_y = y - mScrollbar[i]->getRect().mBottom;
		if (mScrollbar[i] &&
			mScrollbar[i]->handleToolTip(local_x, local_y, msg, sticky_rect))
		{
			return true;
		}
	}
	// Handle 'child' view.
	if (mScrolledView)
	{
		local_x = x - mScrolledView->getRect().mLeft;
		local_y = y - mScrolledView->getRect().mBottom;
		if (mScrolledView->handleToolTip(local_x, local_y, msg, sticky_rect))
		{
			return true;
		}
	}

	// Opaque
	return true;
}

void LLScrollableContainer::calcVisibleSize(S32* visible_width,
											S32* visible_height,
											bool* show_h_scrollbar,
											bool* show_v_scrollbar) const
{
	if (mScrolledView)
	{
		const LLRect& rect = mScrolledView->getRect();
		calcVisibleSize(rect, visible_width, visible_height, show_h_scrollbar,
						show_v_scrollbar);
	}
}

void LLScrollableContainer::calcVisibleSize(const LLRect& doc_rect,
											S32* visible_width,
											S32* visible_height,
											bool* show_h_scrollbar,
											bool* show_v_scrollbar) const
{
	S32 doc_width = doc_rect.getWidth();
	S32 doc_height = doc_rect.getHeight();

	S32 width_delta = 2 * getBorderWidth();
	*visible_width = getRect().getWidth() - width_delta;
	*visible_height = getRect().getHeight() - width_delta;

	*show_v_scrollbar = false;
	if (*visible_height < doc_height)
	{
		*show_v_scrollbar = true;
		*visible_width -= SCROLLBAR_SIZE;
	}

	*show_h_scrollbar = false;
	if (*visible_width < doc_width)
	{
		*show_h_scrollbar = true;
		*visible_height -= SCROLLBAR_SIZE;

		// Must retest now that visible_height has changed
		if (!*show_v_scrollbar && (*visible_height < doc_height))
		{
			*show_v_scrollbar = true;
			*visible_width -= SCROLLBAR_SIZE;
		}
	}
}

void LLScrollableContainer::draw()
{
	if (mAutoScrolling)
	{
		// Add acceleration to autoscroll
		mAutoScrollRate = llmin(mAutoScrollRate +
								LLFrameTimer::getFrameDeltaTimeF32() *
								AUTO_SCROLL_RATE_ACCEL,
								MAX_AUTO_SCROLL_RATE);
	}
	else
	{
		// Reset to minimum
		mAutoScrollRate = MIN_AUTO_SCROLL_RATE;
	}
	// Clear this flag to be set on next call to handleDragAndDrop
	mAutoScrolling = false;

	// Auto-focus when scrollbar active. This allows us to capture user intent
	// (i.e. stop automatically scrolling the view/etc).
	if (!gFocusMgr.childHasKeyboardFocus(this) &&
		(mScrollbar[VERTICAL]->hasMouseCapture() ||
		 mScrollbar[HORIZONTAL]->hasMouseCapture()))
	{
		focusFirstItem();
	}

	// Draw background
	if (mIsOpaque)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv(mBackgroundColor.mV);
		gl_rect_2d(mInnerRect);
	}

	// Draw mScrolledViews and update scroll bars. Get a scissor region ready,
	// and draw the scrolling view. The scissor region ensures that we don't
	// draw outside of the bounds of the rectangle.
	if (mScrolledView)
	{
		updateScroll();

		// Draw the scrolled area.
		S32 visible_width = 0;
		S32 visible_height = 0;
		bool show_v_scrollbar = false;
		bool show_h_scrollbar = false;
		calcVisibleSize(&visible_width, &visible_height, &show_h_scrollbar,
						&show_v_scrollbar);

		LLLocalClipRect clip(LLRect(mInnerRect.mLeft,
							 mInnerRect.mBottom +
							 (show_h_scrollbar ? SCROLLBAR_SIZE : 0) +
							 visible_height,
							 visible_width,
							 mInnerRect.mBottom +
							 (show_h_scrollbar ? SCROLLBAR_SIZE : 0)));
		drawChild(mScrolledView);
	}

	// Highlight border if a child of this container has keyboard focus
	if (mBorder && mBorder->getVisible())
	{
		mBorder->setKeyboardFocusHighlight(gFocusMgr.childHasKeyboardFocus(this));
	}

	// Draw all children except mScrolledView
	// Note: scrollbars have been adjusted by above drawing code
	for (child_list_const_reverse_iter_t child_iter = getChildList()->rbegin(),
										 rend = getChildList()->rend();
		 child_iter != rend; ++child_iter)
	{
		LLView* viewp = *child_iter;
		if (!viewp) continue;

		if (sDebugRects)
		{
			++sDepth;
		}
		if (viewp != mScrolledView && viewp->getVisible())
		{
			drawChild(viewp);
		}
		if (sDebugRects)
		{
			--sDepth;
		}
	}

	if (sDebugRects)
	{
		drawDebugRect();
	}

}

void LLScrollableContainer::updateScroll()
{
	if (!mScrolledView || !mScrollbar[VERTICAL] || !mScrollbar[HORIZONTAL])
	{
		return;
	}

	LLRect doc_rect = mScrolledView->getRect();
	S32 doc_width = doc_rect.getWidth();
	S32 doc_height = doc_rect.getHeight();
	S32 visible_width = 0;
	S32 visible_height = 0;
	bool show_v_scrollbar = false;
	bool show_h_scrollbar = false;
	calcVisibleSize(doc_rect, &visible_width, &visible_height,
					&show_h_scrollbar, &show_v_scrollbar);

	S32 border_width = getBorderWidth();
	if (show_v_scrollbar)
	{
		if (doc_rect.mTop < getRect().getHeight() - border_width)
		{
			mScrolledView->translate(0,
									 getRect().getHeight() - border_width -
									 doc_rect.mTop);
		}

		scrollVertical(	mScrollbar[VERTICAL]->getDocPos());
		mScrollbar[VERTICAL]->setVisible(true);

		S32 v_scrollbar_height = visible_height;
		if (!show_h_scrollbar && mReserveScrollCorner)
		{
			v_scrollbar_height -= SCROLLBAR_SIZE;
		}
		mScrollbar[VERTICAL]->reshape(SCROLLBAR_SIZE, v_scrollbar_height);

		// Make room for the horizontal scrollbar (or not)
		S32 v_scrollbar_offset = 0;
		if (show_h_scrollbar || mReserveScrollCorner)
		{
			v_scrollbar_offset = SCROLLBAR_SIZE;
		}
		LLRect r = mScrollbar[VERTICAL]->getRect();
		r.translate(0, mInnerRect.mBottom - r.mBottom + v_scrollbar_offset);
		mScrollbar[VERTICAL]->setRect(r);
	}
	else
	{
		mScrolledView->translate(0,
								 getRect().getHeight() - border_width -
								 doc_rect.mTop);

		mScrollbar[VERTICAL]->setVisible(false);
		mScrollbar[VERTICAL]->setDocPos(0);
	}

	if (show_h_scrollbar)
	{
		if (doc_rect.mLeft > border_width)
		{
			mScrolledView->translate(border_width - doc_rect.mLeft, 0);
			mScrollbar[HORIZONTAL]->setDocPos(0);
		}
		else
		{
			scrollHorizontal(mScrollbar[HORIZONTAL]->getDocPos());
		}

		mScrollbar[HORIZONTAL]->setVisible(true);
		S32 h_scrollbar_width = visible_width;
		if (!show_v_scrollbar && mReserveScrollCorner)
		{
			h_scrollbar_width -= SCROLLBAR_SIZE;
		}
		mScrollbar[HORIZONTAL]->reshape(h_scrollbar_width, SCROLLBAR_SIZE);
	}
	else
	{
		mScrolledView->translate(border_width - doc_rect.mLeft, 0);

		mScrollbar[HORIZONTAL]->setVisible(false);
		mScrollbar[HORIZONTAL]->setDocPos(0);
	}

	mScrollbar[HORIZONTAL]->setDocSize(doc_width);
	mScrollbar[HORIZONTAL]->setPageSize(visible_width);

	mScrollbar[VERTICAL]->setDocSize(doc_height);
	mScrollbar[VERTICAL]->setPageSize(visible_height);
}

void LLScrollableContainer::setBorderVisible(bool b)
{
	if (mBorder)
	{
		mBorder->setVisible(b);
	}
}

LLRect LLScrollableContainer::getContentWindowRect()
{
	updateScroll();
	LLRect scroller_view_rect;
	S32 visible_width = 0;
	S32 visible_height = 0;
	bool show_h_scrollbar = false;
	bool show_v_scrollbar = false;
	calcVisibleSize(&visible_width, &visible_height, &show_h_scrollbar,
					&show_v_scrollbar);
	S32 left = getBorderWidth();
	S32 bottom = show_h_scrollbar ? mScrollbar[HORIZONTAL]->getRect().mTop
								  : left;	// = border width
	scroller_view_rect.setOriginAndSize(left, bottom, visible_width,
										visible_height);
	return scroller_view_rect;
}

// Scroll so that as much of rect as possible is showing (where rect is defined
// in the space of scroller view, not scrolled)
void LLScrollableContainer::scrollToShowRect(const LLRect& rect,
												 const LLCoordGL& offset)
{
	if (!mScrolledView || !mScrollbar[VERTICAL] || !mScrollbar[HORIZONTAL])
	{
		return;
	}

	S32 visible_width = 0;
	S32 visible_height = 0;
	bool show_v_scrollbar = false;
	bool show_h_scrollbar = false;
	const LLRect& scrolled_rect = mScrolledView->getRect();
	calcVisibleSize(scrolled_rect, &visible_width, &visible_height,
					&show_h_scrollbar, &show_v_scrollbar);

	// Cannot be so far left that right side of rect goes off screen, or so far
	// right that left side does
	S32 horiz_offset = llclamp(offset.mX,
							   llmin(0, -visible_width + rect.getWidth()), 0);
	// Cannot be so high that bottom of rect goes off screen, or so low that
	// top does
	S32 vert_offset = llclamp(offset.mY, 0,
							  llmax(0, visible_height - rect.getHeight()));

	// Vertical
	// 1. First make sure the top is visible
	// 2. Then, if possible without hiding the top, make the bottom visible.
	S32 vert_pos = mScrollbar[VERTICAL]->getDocPos();

	// Find scrollbar position to get top of rect on screen (scrolling up)
	S32 top_offset = scrolled_rect.mTop - rect.mTop - vert_offset;
	// Find scrollbar position to get bottom of rect on screen (scrolling down)
	S32 bottom_offset = vert_offset == 0 ? scrolled_rect.mTop - rect.mBottom -
										   visible_height
										 : top_offset;
	// Scroll up far enough to see top or scroll down just enough if item is
	// bigger than visual area
	if (vert_pos >= top_offset || visible_height < rect.getHeight())
	{
		vert_pos = top_offset;
	}
	// Else scroll down far enough to see bottom
	else if (vert_pos <= bottom_offset)
	{
		vert_pos = bottom_offset;
	}

	mScrollbar[VERTICAL]->setDocSize(scrolled_rect.getHeight());
	mScrollbar[VERTICAL]->setPageSize(visible_height);
	mScrollbar[VERTICAL]->setDocPos(vert_pos);

	// Horizontal
	// 1. First make sure left side is visible
	// 2. Then, if possible without hiding the left side, make the right side
	//    visible.
	S32 horiz_pos = mScrollbar[HORIZONTAL]->getDocPos();
	S32 left_offset = rect.mLeft - scrolled_rect.mLeft + horiz_offset;
	S32 right_offset = horiz_offset == 0 ? rect.mRight - scrolled_rect.mLeft -
										   visible_width
										 : left_offset;

	if (horiz_pos >= left_offset || visible_width < rect.getWidth())
	{
		horiz_pos = left_offset;
	}
	else if (horiz_pos <= right_offset)
	{
		horiz_pos = right_offset;
	}

	mScrollbar[HORIZONTAL]->setDocSize(scrolled_rect.getWidth());
	mScrollbar[HORIZONTAL]->setPageSize(visible_width);
	mScrollbar[HORIZONTAL]->setDocPos(horiz_pos);

	// Propagate scroll to document
	updateScroll();
}

void LLScrollableContainer::pageUp(S32 overlap)
{
	if (mScrollbar[VERTICAL])
	{
		mScrollbar[VERTICAL]->pageUp(overlap);
	}
}

void LLScrollableContainer::pageDown(S32 overlap)
{
	if (mScrollbar[VERTICAL])
	{
		mScrollbar[VERTICAL]->pageDown(overlap);
	}
}

void LLScrollableContainer::goToTop()
{
	if (mScrollbar[VERTICAL])
	{
		mScrollbar[VERTICAL]->setDocPos(0);
	}
}

void LLScrollableContainer::goToBottom()
{
	if (mScrollbar[VERTICAL])
	{
		mScrollbar[VERTICAL]->setDocPos(mScrollbar[VERTICAL]->getDocSize());
	}
}

S32 LLScrollableContainer::getBorderWidth() const
{
	return mBorder ? mBorder->getBorderWidth() : 0;
}

//virtual
LLXMLNodePtr LLScrollableContainer::getXML(bool save_children) const
{
	LLXMLNodePtr nodep = LLUICtrl::getXML();

	nodep->setName(LL_SCROLLABLE_CONTAINER_VIEW_TAG);

	// Attributes
	nodep->createChild("opaque", true)->setBoolValue(mIsOpaque);
	if (mIsOpaque)
	{
		nodep->createChild("color", true)->setFloatValue(4,
														 mBackgroundColor.mV);
	}

	// Contents
	LLXMLNodePtr child_nodep = mScrolledView->getXML();
	nodep->addChild(child_nodep);

	return nodep;
}

LLView* LLScrollableContainer::fromXML(LLXMLNodePtr nodep, LLView* parentp,
									   LLUICtrlFactory* factoryp)
{
	std::string name(LL_SCROLLABLE_CONTAINER_VIEW_TAG);
	nodep->getAttributeString("name", name);

	LLRect rect;
	createRect(nodep, rect, parentp, LLRect());

	bool opaque = false;
	nodep->getAttributeBool("opaque", opaque);

	LLColor4 color(0.f, 0.f, 0.f, 0.f);
	LLUICtrlFactory::getAttributeColor(nodep, "color", color);

	// Create the scroll view
	LLPanel* panelp = NULL;
	LLScrollableContainer* containerp = new LLScrollableContainer(name, rect,
																  panelp,
																  opaque,
																  color);
	// Find a child panel to add
	for (LLXMLNodePtr childp = nodep->getFirstChild(); childp.notNull();
		 childp = childp->getNextSibling())
	{
		LLView* viewp = factoryp->createCtrlWidget(panelp, childp);
		if (viewp && viewp->asPanel())
		{
			if (panelp)
			{
				llwarns << "Attempting to put multiple panels into a scrollable container view !"
						<< llendl;
				delete viewp;
			}
			else
			{
				panelp = viewp->asPanel();
				containerp->addChild(panelp);
			}
		}
	}
	containerp->mScrolledView = panelp;
	return containerp;
}
