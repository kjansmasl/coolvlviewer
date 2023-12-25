/**
 * @file lltabcontainer.cpp
 * @brief LLTabContainer class
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

#include "lltabcontainer.h"

#include "llbutton.h"
#include "llcriticaldamp.h"
#include "llrender.h"
#include "llresizehandle.h"
#include "llstl.h"				// For DeletePointer()
#include "lltextbox.h"
#include "lluictrlfactory.h"

constexpr F32 SCROLL_STEP_TIME = 0.4f;
constexpr F32 SCROLL_DELAY_TIME = 0.5f;
constexpr S32 TAB_PADDING = 15;
constexpr S32 TABCNTR_TAB_MIN_WIDTH = 60;
constexpr S32 TABCNTR_VERT_TAB_MIN_WIDTH = 100;
constexpr S32 TABCNTR_TAB_MAX_WIDTH = 150;
// When tabs are parially obscured, how much can you still see:
constexpr S32 TABCNTR_TAB_PARTIAL_WIDTH = 12;
constexpr S32 TABCNTR_TAB_HEIGHT = 16;
constexpr S32 TABCNTR_ARROW_BTN_SIZE = 16;
// How many pixels the tab buttons and tab panels overlap:
constexpr S32 TABCNTR_BUTTON_PANEL_OVERLAP = 1;
constexpr S32 TABCNTR_TAB_H_PAD = 4;
constexpr S32 TABCNTR_TAB_BTN_MARGIN = LLPANEL_BORDER_WIDTH +
									   2 * (TABCNTR_ARROW_BTN_SIZE +
											TABCNTR_TAB_H_PAD);

constexpr S32 TABCNTRV_ARROW_BTN_SIZE = 16;
constexpr S32 TABCNTRV_PAD = 0;

static const std::string LL_TAB_CONTAINER_COMMON_TAG = "tab_container";
static LLRegisterWidget<LLTabContainer> r25(LL_TAB_CONTAINER_COMMON_TAG);

LLTabContainer::LLTabContainer(const std::string& name, const LLRect& rect,
							   TabPosition pos, bool bordered,bool vertical)
:	LLPanel(name, rect, bordered),
	mCurrentTabIdx(-1),
	mNextTabIdx(-1),
	mTabsHidden(false),
	mScrolled(false),
	mScrollPos(0),
	mScrollPosPixels(0),
	mMaxScrollPos(0),
	mCloseCallback(NULL),
	mCallbackUserdata(NULL),
	mTitleBox(NULL),
	mTopBorderHeight(LLPANEL_BORDER_WIDTH),
	mTabPosition(pos),
	mLockedTabCount(0),
	mMinTabWidth(TABCNTR_TAB_MIN_WIDTH),
	mMaxTabWidth(TABCNTR_TAB_MAX_WIDTH),
	mPrevArrowBtn(NULL),
	mNextArrowBtn(NULL),
	mIsVertical(vertical),
	// Horizontal Specific
	mJumpPrevArrowBtn(NULL),
	mJumpNextArrowBtn(NULL),
	mRightTabBtnOffset(0),
	mTotalTabWidth(0)
{
	// *HACK: to support default min width for legacy vertical tab containers
	if (mIsVertical)
	{
		mMinTabWidth = TABCNTR_VERT_TAB_MIN_WIDTH;
	}
	setMouseOpaque(false);
	initButtons();
	mDragAndDropDelayTimer.stop();
}

LLTabContainer::~LLTabContainer()
{
	std::for_each(mTabList.begin(), mTabList.end(), DeletePointer());
	mTabList.clear();
}

//virtual
void LLTabContainer::setValue(const LLSD& value)
{
	selectTab((S32)value.asInteger());
}

//virtual
void LLTabContainer::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);
	updateMaxScrollPos();
}

//virtual
LLView* LLTabContainer::getChildView(const char* name, bool recurse,
									 bool create_if_missing) const
{
	for (tuple_list_t::const_iterator itor = mTabList.begin(),
									  end = mTabList.end();
		 itor != end; ++itor)
	{
		LLPanel* panel = (*itor)->mTabPanel;
		if (strcmp(panel->getName().c_str(), name) == 0)
		{
			return panel;
		}
	}

	if (recurse)
	{
		for (tuple_list_t::const_iterator itor = mTabList.begin(),
										  end = mTabList.end();
			 itor != end; ++itor)
		{
			LLPanel* panel = (*itor)->mTabPanel;
			LLView* child = panel->getChildView(name, recurse, false);
			if (child)
			{
				return child;
			}
		}
	}
	return LLView::getChildView(name, recurse, create_if_missing);
}

//virtual
void LLTabContainer::draw()
{
	tuple_list_t::iterator begin = mTabList.begin();
	tuple_list_t::iterator end = mTabList.end();

	S32 target_pixel_scroll = 0;
	S32 cur_scroll_pos = getScrollPos();

	if (cur_scroll_pos > 0)
	{
		if (!mIsVertical)
		{
			S32 available_width_with_arrows = getRect().getWidth() -
											  mRightTabBtnOffset -
											  2 * TABCNTR_TAB_BTN_MARGIN;
			for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
			{
				if (cur_scroll_pos == 0)
				{
					break;
				}
				target_pixel_scroll += (*iter)->mButton->getRect().getWidth();
				cur_scroll_pos--;
			}

			// Show part of the tab to the left of what is fully visible
			target_pixel_scroll -= TABCNTR_TAB_PARTIAL_WIDTH;
			// clamp so that rightmost tab never leaves right side of screen
			target_pixel_scroll = llmin(mTotalTabWidth - available_width_with_arrows,
										target_pixel_scroll);
		}
		else
		{
			S32 available_height_with_arrows = getRect().getHeight() -
											   getTopBorderHeight() -
											   TABCNTR_TAB_BTN_MARGIN;
			for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
			{
				if (cur_scroll_pos==0)
				{
					break;
				}
				target_pixel_scroll += (*iter)->mButton->getRect().getHeight();
				cur_scroll_pos--;
			}
			S32 total_tab_height = (gBtnHeight + TABCNTRV_PAD) * getTabCount() + TABCNTRV_PAD;
			// clamp so that the bottom tab never leaves bottom of panel
			target_pixel_scroll = llmin(total_tab_height - available_height_with_arrows,
										target_pixel_scroll);
		}
	}

	setScrollPosPixels((S32)lerp((F32)getScrollPosPixels(),
					   (F32)target_pixel_scroll,
					   LLCriticalDamp::getInterpolant(0.08f)));

	bool has_scroll_arrows = mMaxScrollPos > 0 || mScrollPosPixels > 0;
	if (!mIsVertical)
	{
		mJumpPrevArrowBtn->setVisible(has_scroll_arrows);
		mJumpNextArrowBtn->setVisible(has_scroll_arrows);
	}
	mPrevArrowBtn->setVisible(has_scroll_arrows);
	mNextArrowBtn->setVisible(has_scroll_arrows);

	S32 left = 0, top = 0;
	if (mIsVertical)
	{
		top = getRect().getHeight() - getTopBorderHeight() -
			  LLPANEL_BORDER_WIDTH - 1 -
			  (has_scroll_arrows ? TABCNTRV_ARROW_BTN_SIZE : 0);
		top += getScrollPosPixels();
	}
	else
	{
		// Set the leftmost position of the tab buttons.
		left = LLPANEL_BORDER_WIDTH + (has_scroll_arrows ? TABCNTR_ARROW_BTN_SIZE * 2
														 : TABCNTR_TAB_H_PAD);
		left -= getScrollPosPixels();
	}

	// Hide all the buttons
	for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
	{
		LLTabTuple* tuple = *iter;
		if (tuple)
		{
			LLButton* tab_button = tuple->mButton;
			if (tab_button)
			{
				tab_button->setVisible(false);
			}
		}
	}

	LLPanel::draw();

	// If tabs are hidden, do not draw them and leave them in the invisible
	// state
	if (!getTabsHidden())
	{
		// Show all the buttons
		for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
		{
			LLTabTuple* tuple = *iter;
			if (tuple)
			{
				LLButton* tab_button = tuple->mButton;
				if (tab_button)
				{
					tab_button->setVisible(true);
				}
			}
		}

		// Draw some of the buttons...
		LLRect clip_rect = getLocalRect();
		if (has_scroll_arrows)
		{
			// ...but clip them.
			if (mIsVertical)
			{
				clip_rect.mBottom = mNextArrowBtn->getRect().mTop + 3*TABCNTRV_PAD;
				clip_rect.mTop = mPrevArrowBtn->getRect().mBottom - 3*TABCNTRV_PAD;
			}
			else
			{
				clip_rect.mLeft = mPrevArrowBtn->getRect().mRight;
				clip_rect.mRight = mNextArrowBtn->getRect().mLeft;
			}
		}
		LLLocalClipRect clip(clip_rect);

		S32 max_scroll_visible = getTabCount() - getMaxScrollPos() + getScrollPos();
		S32 idx = 0;
		for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
		{
			LLTabTuple* tuple = *iter;
			if (tuple)
			{
				LLButton* tab_button = tuple->mButton;
				if (tab_button)
				{
					tab_button->translate(left ? left - tab_button->getRect().mLeft
											   : 0,
										  top ? top - tab_button->getRect().mTop
											  : 0);
					if (top)
					{
						top -= gBtnHeight + TABCNTRV_PAD;
					}
					if (left)
					{
						left += tab_button->getRect().getWidth();
					}

					if (!mIsVertical)
					{
						if (idx < getScrollPos())
						{
							if (tab_button->getFlashing())
							{
								mPrevArrowBtn->setFlashing(true);
							}
						}
						else if (max_scroll_visible < idx)
						{
							if (tab_button->getFlashing())
							{
								mNextArrowBtn->setFlashing(true);
							}
						}
					}
					LLUI::pushMatrix();
					{
						LLUI::translate((F32)tab_button->getRect().mLeft,
										(F32)tab_button->getRect().mBottom,
										0.f);
						tab_button->draw();
					}
					LLUI::popMatrix();
				}
			}
			++idx;
		}

		if (mIsVertical && has_scroll_arrows)
		{
			// Redraw the arrows so that they appears on top.
			gGL.pushUIMatrix();
			gGL.translateUI((F32)mPrevArrowBtn->getRect().mLeft,
						    (F32)mPrevArrowBtn->getRect().mBottom, 0.f);
			mPrevArrowBtn->draw();
			gGL.popUIMatrix();

			gGL.pushUIMatrix();
			gGL.translateUI((F32)mNextArrowBtn->getRect().mLeft,
						    (F32)mNextArrowBtn->getRect().mBottom, 0.f);
			mNextArrowBtn->draw();
			gGL.popUIMatrix();
		}
	}

	mPrevArrowBtn->setFlashing(false);
	mNextArrowBtn->setFlashing(false);
}

//virtual
bool LLTabContainer::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	bool has_scroll_arrows = getMaxScrollPos() > 0;

	if (has_scroll_arrows)
	{
		if (mJumpPrevArrowBtn &&
			mJumpPrevArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mJumpPrevArrowBtn->getRect().mLeft;
			S32 local_y = y - mJumpPrevArrowBtn->getRect().mBottom;
			handled = mJumpPrevArrowBtn->handleMouseDown(local_x, local_y, mask);
		}
		else if (mJumpNextArrowBtn &&
				 mJumpNextArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mJumpNextArrowBtn->getRect().mLeft;
			S32 local_y = y - mJumpNextArrowBtn->getRect().mBottom;
			handled = mJumpNextArrowBtn->handleMouseDown(local_x, local_y, mask);
		}
		else if (mPrevArrowBtn && mPrevArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mPrevArrowBtn->getRect().mLeft;
			S32 local_y = y - mPrevArrowBtn->getRect().mBottom;
			handled = mPrevArrowBtn->handleMouseDown(local_x, local_y, mask);
		}
		else if (mNextArrowBtn && mNextArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mNextArrowBtn->getRect().mLeft;
			S32 local_y = y - mNextArrowBtn->getRect().mBottom;
			handled = mNextArrowBtn->handleMouseDown(local_x, local_y, mask);
		}
	}
	if (!handled)
	{
		handled = LLPanel::handleMouseDown(x, y, mask);
	}

	S32 tab_count = getTabCount();
	if (tab_count > 0)
	{
		LLTabTuple* firsttuple = getTab(0);
		if (!firsttuple) return handled;
		LLButton* tab_button = firsttuple->mButton;
		if (!tab_button) return handled;

		LLRect tab_rect;
		if (mIsVertical)
		{
			tab_rect = LLRect(tab_button->getRect().mLeft,
							  has_scroll_arrows ? mPrevArrowBtn->getRect().mBottom - TABCNTRV_PAD
												: mPrevArrowBtn->getRect().mTop,
							  tab_button->getRect().mRight,
							  has_scroll_arrows ? mNextArrowBtn->getRect().mTop + TABCNTRV_PAD
												: mNextArrowBtn->getRect().mBottom);
		}
		else
		{
			tab_rect = LLRect(has_scroll_arrows ? mPrevArrowBtn->getRect().mRight
												: mJumpPrevArrowBtn->getRect().mLeft,
							  tab_button->getRect().mTop,
							  has_scroll_arrows ? mNextArrowBtn->getRect().mLeft
												: mJumpNextArrowBtn->getRect().mRight,
							  tab_button->getRect().mBottom);
		}
		if (tab_rect.pointInRect(x, y))
		{
			S32 index = getCurrentPanelIndex();
			index = llclamp(index, 0, tab_count - 1);
			gFocusMgr.setMouseCapture(this);
			tab_button = getTab(index)->mButton;
			if (tab_button)
			{
				gFocusMgr.setKeyboardFocus(tab_button);
			}
		}
	}

	return handled;
}

//virtual
bool LLTabContainer::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	bool has_scroll_arrows = getMaxScrollPos() > 0;

	if (has_scroll_arrows)
	{
		if (mJumpPrevArrowBtn &&
			mJumpPrevArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mJumpPrevArrowBtn->getRect().mLeft;
			S32 local_y = y - mJumpPrevArrowBtn->getRect().mBottom;
			handled = mJumpPrevArrowBtn->handleHover(local_x, local_y, mask);
		}
		else if (mJumpNextArrowBtn &&
				 mJumpNextArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mJumpNextArrowBtn->getRect().mLeft;
			S32 local_y = y - mJumpNextArrowBtn->getRect().mBottom;
			handled = mJumpNextArrowBtn->handleHover(local_x, local_y, mask);
		}
		else if (mPrevArrowBtn && mPrevArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mPrevArrowBtn->getRect().mLeft;
			S32 local_y = y - mPrevArrowBtn->getRect().mBottom;
			handled = mPrevArrowBtn->handleHover(local_x, local_y, mask);
		}
		else if (mNextArrowBtn && mNextArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mNextArrowBtn->getRect().mLeft;
			S32 local_y = y - mNextArrowBtn->getRect().mBottom;
			handled = mNextArrowBtn->handleHover(local_x, local_y, mask);
		}
	}
	if (!handled)
	{
		handled = LLPanel::handleHover(x, y, mask);
	}

	commitHoveredButton(x, y);

	return handled;
}

//virtual
bool LLTabContainer::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	bool has_scroll_arrows = getMaxScrollPos() > 0;

	if (has_scroll_arrows)
	{
		if (mJumpPrevArrowBtn &&
			mJumpPrevArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mJumpPrevArrowBtn->getRect().mLeft;
			S32 local_y = y - mJumpPrevArrowBtn->getRect().mBottom;
			handled = mJumpPrevArrowBtn->handleMouseUp(local_x, local_y, mask);
		}
		else if (mJumpNextArrowBtn &&
				 mJumpNextArrowBtn->getRect().pointInRect(x, y))
		{
			S32	local_x	= x	- mJumpNextArrowBtn->getRect().mLeft;
			S32	local_y	= y	- mJumpNextArrowBtn->getRect().mBottom;
			handled = mJumpNextArrowBtn->handleMouseUp(local_x,	local_y, mask);
		}
		else if (mPrevArrowBtn && mPrevArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mPrevArrowBtn->getRect().mLeft;
			S32 local_y = y - mPrevArrowBtn->getRect().mBottom;
			handled = mPrevArrowBtn->handleMouseUp(local_x, local_y, mask);
		}
		else if (mNextArrowBtn && mNextArrowBtn->getRect().pointInRect(x, y))
		{
			S32 local_x = x - mNextArrowBtn->getRect().mLeft;
			S32 local_y = y - mNextArrowBtn->getRect().mBottom;
			handled = mNextArrowBtn->handleMouseUp(local_x, local_y, mask);
		}
	}
	if (!handled)
	{
		handled = LLPanel::handleMouseUp(x, y, mask);
	}

	commitHoveredButton(x, y);
	LLPanel* cur_panel = getCurrentPanel();
	if (hasMouseCapture())
	{
#if 0	// This causes scroll list items to vanish when tab is selected. TODO:
		// find out why...
		// If nothing in the panel gets focus, make sure the new tab does
		// otherwise the last tab might keep focus
		if (cur_panel && !cur_panel->focusFirstItem(false))
#else
		if (cur_panel)
#endif
		{
			// Make sure new tab gets focus
			getTab(getCurrentPanelIndex())->mButton->setFocus(true);
		}
		gFocusMgr.setMouseCapture(NULL);
	}

	return handled;
}

//virtual
bool LLTabContainer::handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect)
{
	bool handled = LLPanel::handleToolTip(x, y, msg, sticky_rect);
	if (!handled && getTabCount() > 0)
	{
		LLTabTuple* firsttuple = getTab(0);
		if (!firsttuple) return handled;
		LLButton* tab_button = firsttuple->mButton;
		if (!tab_button) return handled;

		bool has_scroll_arrows = getMaxScrollPos() > 0;
		LLRect clip;
		if (mIsVertical)
		{
			clip = LLRect(tab_button->getRect().mLeft,
						  has_scroll_arrows ? mPrevArrowBtn->getRect().mBottom - TABCNTRV_PAD
											: mPrevArrowBtn->getRect().mTop,
						  tab_button->getRect().mRight,
						  has_scroll_arrows ? mNextArrowBtn->getRect().mTop + TABCNTRV_PAD
											: mNextArrowBtn->getRect().mBottom);
		}
		else
		{
			clip = LLRect(has_scroll_arrows ? mPrevArrowBtn->getRect().mRight
											: mJumpPrevArrowBtn->getRect().mLeft,
						  tab_button->getRect().mTop,
						  has_scroll_arrows ? mNextArrowBtn->getRect().mLeft
											: mJumpNextArrowBtn->getRect().mRight,
						  tab_button->getRect().mBottom);
		}

		tuple_list_t::iterator begin = mTabList.begin();
		tuple_list_t::iterator end = mTabList.end();

		if (clip.pointInRect(x, y))
		{
			for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
			{
				LLTabTuple* tuple = *iter;
				if (!tuple) continue;
				tab_button = tuple->mButton;
				if (!tab_button) continue;
				tab_button->setVisible(true);
				S32 local_x = x - tab_button->getRect().mLeft;
				S32 local_y = y - tab_button->getRect().mBottom;
				handled = tab_button->handleToolTip(local_x, local_y, msg,
													sticky_rect);
				if (handled)
				{
					break;
				}
			}
		}

		for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
		{
			LLTabTuple* tuple = *iter;
			if (tuple)
			{
				tab_button = tuple->mButton;
				if (tab_button)
				{
					tab_button->setVisible(false);
				}
			}
		}
	}

	return handled;
}

//virtual
bool LLTabContainer::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;
	if (key == KEY_LEFT && mask == MASK_ALT)
	{
		selectPrevTab();
		handled = true;
	}
	else if (key == KEY_RIGHT && mask == MASK_ALT)
	{
		selectNextTab();
		handled = true;
	}

	if (handled)
	{
		if (getCurrentPanel())
		{
			getCurrentPanel()->setFocus(true);
		}
	}

	if (!gFocusMgr.childHasKeyboardFocus(getCurrentPanel()))
	{
		// if child has focus, but not the current panel, focus is on a button
		if (mIsVertical)
		{
			handled = true;
			switch (key)
			{
				case KEY_UP:
					selectPrevTab();
					break;

				case KEY_DOWN:
					selectNextTab();
					break;

				case KEY_LEFT:
					break;

				case KEY_RIGHT:
					if (getTabPosition() == LEFT && getCurrentPanel())
					{
						getCurrentPanel()->setFocus(true);
					}
					break;

				default:
					handled = false;
			}
		}
		else
		{
			handled = true;
			switch (key)
			{
				case KEY_UP:
					if (getTabPosition() == BOTTOM && getCurrentPanel())
					{
						getCurrentPanel()->setFocus(true);
					}
					break;

				case KEY_DOWN:
					if (getTabPosition() == TOP && getCurrentPanel())
					{
						getCurrentPanel()->setFocus(true);
					}
					break;

				case KEY_LEFT:
					selectPrevTab();
					break;

				case KEY_RIGHT:
					selectNextTab();
					break;

				default:
					handled = false;
			}
		}
	}

	return handled;
}

//virtual
LLXMLNodePtr LLTabContainer::getXML(bool save_children) const
{
	LLXMLNodePtr nodep = LLPanel::getXML();

	nodep->setName(LL_TAB_CONTAINER_COMMON_TAG);

	const std::string pos = getTabPosition() == TOP ? "top" : "bottom";
	nodep->createChild("tab_position", true)->setStringValue(pos);

	return nodep;
}

//virtual
bool LLTabContainer::handleDragAndDrop(S32 x, S32 y, MASK mask,	bool drop,
									   EDragAndDropType type, void* cargo_data,
									   EAcceptance* accept,
									   std::string& tooltip)
{
	bool has_scroll_arrows = getMaxScrollPos() > 0;

	if (mDragAndDropDelayTimer.getElapsedTimeF32() > SCROLL_DELAY_TIME)
	{
		if (has_scroll_arrows)
		{
			if (mJumpPrevArrowBtn &&
				mJumpPrevArrowBtn->getRect().pointInRect(x, y))
			{
				S32	local_x	= x	- mJumpPrevArrowBtn->getRect().mLeft;
				S32	local_y	= y	- mJumpPrevArrowBtn->getRect().mBottom;
				mJumpPrevArrowBtn->handleHover(local_x,	local_y, mask);
			}
			if (mJumpNextArrowBtn &&
				mJumpNextArrowBtn->getRect().pointInRect(x, y))
			{
				S32	local_x	= x	- mJumpNextArrowBtn->getRect().mLeft;
				S32	local_y	= y	- mJumpNextArrowBtn->getRect().mBottom;
				mJumpNextArrowBtn->handleHover(local_x,	local_y, mask);
			}
			if (mPrevArrowBtn->getRect().pointInRect(x,	y))
			{
				S32	local_x	= x	- mPrevArrowBtn->getRect().mLeft;
				S32	local_y	= y	- mPrevArrowBtn->getRect().mBottom;
				mPrevArrowBtn->handleHover(local_x,	local_y, mask);
			}
			else if	(mNextArrowBtn->getRect().pointInRect(x, y))
			{
				S32	local_x	= x	- mNextArrowBtn->getRect().mLeft;
				S32	local_y	= y	- mNextArrowBtn->getRect().mBottom;
				mNextArrowBtn->handleHover(local_x, local_y, mask);
			}
		}

		for (tuple_list_t::iterator iter = mTabList.begin(),
									end = mTabList.end();
			 iter != end; ++iter)
		{
			LLTabTuple*	tuple =	*iter;
			if (!tuple) continue;
			LLButton* tab_button = tuple->mButton;
			if (!tab_button) continue;

			tab_button->setVisible(true);
			S32	local_x	= x	- tab_button->getRect().mLeft;
			S32	local_y	= y	- tab_button->getRect().mBottom;
			if (tab_button->pointInView(local_x, local_y) &&
				tab_button->getEnabled() && !tuple->mTabPanel->getVisible())
			{
				tab_button->onCommit();
				mDragAndDropDelayTimer.stop();
			}
		}
	}

	return LLView::handleDragAndDrop(x,	y, mask, drop, type, cargo_data,
									 accept, tooltip);
}

void LLTabContainer::addTabPanel(LLPanel* child, const std::string& label,
								 bool select,
								 void (*on_tab_clicked)(void*, bool),
								 void* userdata,
								 S32 indent, bool placeholder,
								 eInsertionPoint insertion_point)
{
	if (child->getParent() == this)
	{
		// Already a child of mine
		return;
	}
	const LLFontGL* font = mIsVertical ? LLFontGL::getFontSansSerif()
									   : LLFontGL::getFontSansSerifSmall();

	// Store the original label for possible xml export.
	child->setLabel(label);
	std::string trimmed_label = label;
	LLStringUtil::trim(trimmed_label);

	S32 button_width = mMinTabWidth;
	if (!mIsVertical)
	{
		button_width = llclamp(font->getWidth(trimmed_label) + TAB_PADDING,
							   mMinTabWidth, mMaxTabWidth);
	}

	// Tab panel
	S32 tab_panel_top;
	S32 tab_panel_bottom;
	if (getTabPosition() == LLTabContainer::TOP)
	{
		S32 tab_height = mIsVertical ? gBtnHeight : TABCNTR_TAB_HEIGHT;
		tab_panel_top = getRect().getHeight() - getTopBorderHeight() -
						(tab_height - TABCNTR_BUTTON_PANEL_OVERLAP);
		tab_panel_bottom = LLPANEL_BORDER_WIDTH;
	}
	else
	{
		tab_panel_top = getRect().getHeight() - getTopBorderHeight();
		// Run to the edge, covering up the border
		tab_panel_bottom = TABCNTR_TAB_HEIGHT - TABCNTR_BUTTON_PANEL_OVERLAP;
	}

	LLRect tab_panel_rect;
	if (mIsVertical)
	{
		tab_panel_rect = LLRect(mMinTabWidth + LLPANEL_BORDER_WIDTH * 2 + TABCNTRV_PAD,
								getRect().getHeight() - LLPANEL_BORDER_WIDTH,
								getRect().getWidth() - LLPANEL_BORDER_WIDTH,
								LLPANEL_BORDER_WIDTH);
	}
	else
	{
		tab_panel_rect = LLRect(LLPANEL_BORDER_WIDTH, tab_panel_top,
								getRect().getWidth() - LLPANEL_BORDER_WIDTH,
								tab_panel_bottom);
	}
	child->setFollowsAll();
	child->translate(tab_panel_rect.mLeft - child->getRect().mLeft,
					 tab_panel_rect.mBottom - child->getRect().mBottom);
	child->reshape(tab_panel_rect.getWidth(), tab_panel_rect.getHeight());
	// Add this child later

	child->setVisible(false);  // Will be made visible when selected

	mTotalTabWidth += button_width;

	// Tab button

	// Note: btn_rect.mLeft is just a dummy. Will be updated in draw().
	LLRect btn_rect;
	std::string tab_img;
	std::string tab_selected_img;
	//  To make new tab art look better, nudge buttons up 1 pixel
	S32 tab_fudge = 1;

	if (mIsVertical)
	{
		btn_rect.setLeftTopAndSize(TABCNTRV_PAD + LLPANEL_BORDER_WIDTH + 2,	// JC - Fudge factor
								   getRect().getHeight() -
								   getTopBorderHeight() -
								   LLPANEL_BORDER_WIDTH - 1 -
								   (gBtnHeight + TABCNTRV_PAD) * getTabCount(),
								   mMinTabWidth, gBtnHeight);
	}
	else if (getTabPosition() == LLTabContainer::TOP)
	{
		btn_rect.setLeftTopAndSize(0,
								   getRect().getHeight() -
								   getTopBorderHeight() + tab_fudge,
								   button_width, TABCNTR_TAB_HEIGHT);
		tab_img = "tab_top_blue.tga";
		tab_selected_img = "tab_top_selected_blue.tga";
	}
	else
	{
		btn_rect.setOriginAndSize(0, tab_fudge, button_width,
								  TABCNTR_TAB_HEIGHT);
		tab_img = "tab_bottom_blue.tga";
		tab_selected_img = "tab_bottom_selected_blue.tga";
	}

	LLTextBox* textbox = NULL;
	LLButton* btn = NULL;

	if (placeholder)
	{
		btn_rect.translate(0, -gButtonVPad - 2);
		textbox = new LLTextBox(trimmed_label, btn_rect, trimmed_label, font);

		btn = new LLButton(LLStringUtil::null, LLRect(0, 0, 0, 0));
	}
	else
	{
		if (mIsVertical)
		{
			btn = new LLButton("vert tab button", btn_rect, LLStringUtil::null,
							   LLStringUtil::null, NULL,
							   &LLTabContainer::onTabBtn, NULL,
							   font, trimmed_label, trimmed_label);
			btn->setImages("tab_left.tga", "tab_left_selected.tga");
			btn->setScaleImage(true);
			btn->setHAlign(LLFontGL::LEFT);
			btn->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
			btn->setTabStop(false);
			if (indent)
			{
				btn->setLeftHPad(indent);
			}
		}
		else
		{
			std::string tooltip = trimmed_label;
			tooltip += "\nAlt-Left arrow for previous tab";
			tooltip += "\nAlt-Right arrow for next tab";

			btn = new LLButton(child->getName() + " tab", btn_rect,
							   LLStringUtil::null, LLStringUtil::null, NULL,
							   &LLTabContainer::onTabBtn,
							   NULL, // Set userdata below
							   font, trimmed_label, trimmed_label);
			btn->setVisible(false);
			btn->setToolTip(tooltip);
			btn->setScaleImage(true);
			btn->setImages(tab_img, tab_selected_img);

			// Try to squeeze in a bit more text
			btn->setLeftHPad(4);
			btn->setRightHPad(2);
			btn->setHAlign(LLFontGL::LEFT);
			btn->setTabStop(false);
			if (indent)
			{
				btn->setLeftHPad(indent);
			}

			if (getTabPosition() == TOP)
			{
				btn->setFollowsTop();
			}
			else
			{
				btn->setFollowsBottom();
			}
		}
	}

	LLTabTuple* tuple = new LLTabTuple(this, child, btn, on_tab_clicked,
									   userdata, textbox);
	insertTuple(tuple, insertion_point);

	if (textbox)
	{
		textbox->setSaveToXML(false);
		addChild(textbox, 0);
	}
	if (btn)
	{
		btn->setSaveToXML(false);
		btn->setCallbackUserData(tuple);
		addChild(btn, 0);
	}
	if (child)
	{
		addChild(child, 1);
	}

	if (select)
	{
		selectLastTab();
	}

	updateMaxScrollPos();
}

void LLTabContainer::addPlaceholder(LLPanel* child, const std::string& label)
{
	addTabPanel(child, label, false, NULL, NULL, 0, true);
}

void LLTabContainer::removeTabPanel(LLPanel* child)
{
	tuple_list_t::iterator begin = mTabList.begin();
	tuple_list_t::iterator end = mTabList.end();

	if (mIsVertical)
	{
		// Fix-up button sizes
		S32 tab_count = 0;
		for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
		{
			LLTabTuple* tuple = *iter;
			LLRect rect;
			rect.setLeftTopAndSize(TABCNTRV_PAD + LLPANEL_BORDER_WIDTH + 2,	// JC - Fudge factor
								   getRect().getHeight() -
								   LLPANEL_BORDER_WIDTH - 1 -
								   (gBtnHeight + TABCNTRV_PAD) * tab_count,
								   mMinTabWidth, gBtnHeight);
			if (tuple->mPlaceholderText)
			{
				tuple->mPlaceholderText->setRect(rect);
			}
			else if (tuple->mButton)
			{
				tuple->mButton->setRect(rect);
			}
			++tab_count;
		}
	}
	else
	{
		// Adjust the total tab width.
		for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
		{
			LLTabTuple* tuple = *iter;
			if (tuple->mTabPanel == child && tuple->mButton)
			{
				mTotalTabWidth -= tuple->mButton->getRect().getWidth();
				break;
			}
		}
	}

	bool has_focus = gFocusMgr.childHasKeyboardFocus(this);

	// If the tab being deleted is the selected one, select a different tab.
	for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
	{
		LLTabTuple* tuple = *iter;
		if (tuple->mTabPanel == child)
		{
			if (tuple->mButton)
			{
	 			removeChild(tuple->mButton);
 				delete tuple->mButton;
			}

 			removeChild(tuple->mTabPanel);
#if 0
			delete tuple->mTabPanel;
#endif

			mTabList.erase(iter);
			delete tuple;

			break;
		}
	}

	// Make sure we do not have more locked tabs than we have tabs
	mLockedTabCount = llmin(getTabCount(), mLockedTabCount);

	if (mCurrentTabIdx >= (S32)mTabList.size())
	{
		mCurrentTabIdx = mTabList.size() - 1;
	}
	selectTab(mCurrentTabIdx);
	if (has_focus)
	{
		LLPanel* panelp = getPanelByIndex(mCurrentTabIdx);
		if (panelp)
		{
			panelp->setFocus(true);
		}
	}

	updateMaxScrollPos();
}

void LLTabContainer::lockTabs(S32 num_tabs)
{
	// Count current tabs or use supplied value and ensure no new tabs get
	// inserted between them
	mLockedTabCount = num_tabs > 0 ? llmin(getTabCount(), num_tabs)
								   : getTabCount();
}

void LLTabContainer::unlockTabs()
{
	mLockedTabCount = 0;
}

void LLTabContainer::enableTabButton(S32 which, bool enable)
{
	if (which >= 0 && which < (S32)mTabList.size())
	{
		mTabList[which]->mButton->setEnabled(enable);
	}
}

void LLTabContainer::deleteAllTabs()
{
	tuple_list_t::iterator begin = mTabList.begin();
	tuple_list_t::iterator end = mTabList.end();

	// Remove all the tab buttons and delete them.  Also, unlink all the child
	// panels.
	for (tuple_list_t::iterator iter = begin; iter != end; ++iter)
	{
		LLTabTuple* tuple = *iter;

		if (tuple->mButton)
		{
			removeChild(tuple->mButton);
			delete tuple->mButton;
		}

 		removeChild(tuple->mTabPanel);
#if 0
 		delete tuple->mTabPanel;
#endif
	}

	// Actually delete the tuples themselves
	std::for_each(begin, end, DeletePointer());
	mTabList.clear();

	// And there is not a current tab any more
	mCurrentTabIdx = -1;
}

LLPanel* LLTabContainer::getCurrentPanel()
{
	if (mCurrentTabIdx >= 0 && mCurrentTabIdx < (S32) mTabList.size())
	{
		return mTabList[mCurrentTabIdx]->mTabPanel;
	}
	return NULL;
}

S32 LLTabContainer::getCurrentPanelIndex()
{
	return mCurrentTabIdx;
}

S32 LLTabContainer::getTabCount()
{
	return mTabList.size();
}

LLPanel* LLTabContainer::getPanelByIndex(S32 index)
{
	if (index >= 0 && index < (S32)mTabList.size())
	{
		return mTabList[index]->mTabPanel;
	}
	return NULL;
}

S32 LLTabContainer::getIndexForPanel(LLPanel* panel)
{
	for (S32 index = 0, count = mTabList.size(); index < count; ++index)
	{
		if (mTabList[index]->mTabPanel == panel)
		{
			return index;
		}
	}
	return -1;
}

S32 LLTabContainer::getPanelIndexByTitle(const std::string& title)
{
	for (S32 index = 0, count = mTabList.size(); index < count; ++index)
	{
		if (title == mTabList[index]->mButton->getLabelSelected())
		{
			return index;
		}
	}
	return -1;
}

LLPanel* LLTabContainer::getPanelByName(const std::string& name)
{
	for (S32 index = 0, count = mTabList.size(); index < count; ++index)
	{
		LLPanel* panel = mTabList[index]->mTabPanel;
		if (name == panel->getName())
		{
			return panel;
		}
	}
	return NULL;
}

// Change the name of the button for the current tab.
void LLTabContainer::setCurrentTabName(const std::string& name)
{
	// Might not have a tab selected
	if (mCurrentTabIdx < 0) return;

	mTabList[mCurrentTabIdx]->mButton->setLabelSelected(name);
	mTabList[mCurrentTabIdx]->mButton->setLabelUnselected(name);
}

void LLTabContainer::selectFirstTab()
{
	selectTab(0);
}

void LLTabContainer::selectLastTab()
{
	selectTab(mTabList.size() - 1);
}

void LLTabContainer::selectNextTab()
{
	bool tab_has_focus = false;
	if (mCurrentTabIdx >= 0 && mTabList[mCurrentTabIdx]->mButton->hasFocus())
	{
		tab_has_focus = true;
	}
	S32 idx = mCurrentTabIdx+1;
	if (idx >= (S32)mTabList.size())
	{
		idx = 0;
	}

	while (!selectTab(idx) && idx != mCurrentTabIdx)
	{
		idx = (idx + 1) % (S32)mTabList.size();
	}

	if (tab_has_focus)
	{
		mTabList[idx]->mButton->setFocus(true);
	}
}

void LLTabContainer::selectPrevTab()
{
	bool tab_has_focus = false;
	if (mCurrentTabIdx >= 0 && mTabList[mCurrentTabIdx]->mButton->hasFocus())
	{
		tab_has_focus = true;
	}
	S32 idx = mCurrentTabIdx - 1;
	if (idx < 0)
	{
		idx = mTabList.size() - 1;
	}

	while (!selectTab(idx) && idx != mCurrentTabIdx)
	{
		if (--idx < 0)
		{
			idx = mTabList.size() - 1;
		}
	}
	if (tab_has_focus)
	{
		mTabList[idx]->mButton->setFocus(true);
	}
}

bool LLTabContainer::selectTabPanel(LLPanel* child)
{
	S32 idx = 0;
	for (tuple_list_t::iterator iter = mTabList.begin(),
								end = mTabList.end();
		 iter != end; ++iter)
	{
		LLTabTuple* tuple = *iter;
		if (tuple->mTabPanel == child)
		{
			return selectTab(idx);
		}
		++idx;
	}
	return false;
}

bool LLTabContainer::selectTab(S32 which)
{
	if (which >= getTabCount() || which < 0)
	{
		return false;
	}

#if 0
	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}
#endif

	LLTabTuple* selected_tuple = getTab(which);
	if (!selected_tuple)
	{
		return false;
	}

	if (!selected_tuple->mPrecommitChangeCallback)
	{
		return setTab(which);
	}

	mNextTabIdx = which;
	selected_tuple->mPrecommitChangeCallback(selected_tuple->mUserData, false);

	return true;
}

bool LLTabContainer::setTab(S32 which)
{
	if (which == -1)
	{
		if (mNextTabIdx == -1)
		{
			return false;
		}
		which = mNextTabIdx;
		mNextTabIdx = -1;
	}

	LLTabTuple* selected_tuple = getTab(which);
	if (!selected_tuple)
	{
		return false;
	}

	bool is_visible = false;
	if (selected_tuple->mButton->getEnabled())
	{
		setCurrentPanelIndex(which);

		S32 i = 0;
		for (tuple_list_t::iterator iter = mTabList.begin(),
									end = mTabList.end();
			 iter != end; ++iter)
		{
			LLTabTuple* tuple = *iter;
			bool is_selected = tuple == selected_tuple;
			tuple->mTabPanel->setVisible(is_selected);
#if 0		// not clear that we want to do this here.
			tuple->mTabPanel->setFocus(is_selected);
#endif
			tuple->mButton->setToggleState(is_selected);
			// RN: this limits tab-stops to active button only, which would
			// require arrow keys to switch tabs
			tuple->mButton->setTabStop(is_selected);

			if (is_selected && (mIsVertical || getMaxScrollPos() > 0))
			{
				// Make sure selected tab is within scroll region
				if (mIsVertical)
				{
					S32 num_visible = getTabCount() - getMaxScrollPos();
					if (i >= getScrollPos() &&
						i <= getScrollPos() + num_visible)
					{
						setCurrentPanelIndex(which);
						is_visible = true;
					}
					else
					{
						is_visible = false;
					}
				}
				else if (tuple->mButton)
				{
					if (i < getScrollPos())
					{
						setScrollPos(i);
					}
					else
					{
						S32 available_width_with_arrows = getRect().getWidth() -
														  mRightTabBtnOffset -
														  2 * (LLPANEL_BORDER_WIDTH +
															   TABCNTR_ARROW_BTN_SIZE +
															   TABCNTR_ARROW_BTN_SIZE + 1);
						S32 running_tab_width = tuple->mButton->getRect().getWidth();
						S32 j = i - 1;
						S32 min_scroll_pos = i;
						if (running_tab_width < available_width_with_arrows)
						{
							while (j >= 0)
							{
								LLTabTuple* other_tuple = getTab(j);
								if (other_tuple && other_tuple->mButton)
								{
									running_tab_width += other_tuple->mButton->getRect().getWidth();
								}
								if (running_tab_width > available_width_with_arrows)
								{
									break;
								}
								--j;
							}
							min_scroll_pos = j + 1;
						}
						setScrollPos(llclamp(getScrollPos(),
									 min_scroll_pos, i));
						setScrollPos(llmin(getScrollPos(), getMaxScrollPos()));
					}
					is_visible = true;
				}
			}
			++i;
		}
		if (selected_tuple->mOnChangeCallback)
		{
			selected_tuple->mOnChangeCallback(selected_tuple->mUserData,
											  false);
		}
	}
	if (mIsVertical && getCurrentPanelIndex() >= 0)
	{
		LLTabTuple* tuple = getTab(getCurrentPanelIndex());
		if (tuple && tuple->mTabPanel && tuple->mButton)
		{
			tuple->mTabPanel->setVisible(true);
			tuple->mButton->setToggleState(true);
		}
	}
	return is_visible;
}

bool LLTabContainer::selectTabByName(const std::string& name)
{
	LLPanel* panel = getPanelByName(name);
	if (!panel)
	{
		llwarns << "Cannot find a tab named: " << name << llendl;
		return false;
	}

	return selectTabPanel(panel);
}

bool LLTabContainer::getTabPanelFlashing(LLPanel *child)
{
	LLTabTuple* tuple = getTabByPanel(child);
	if (tuple && tuple->mButton)
	{
		return tuple->mButton->getFlashing();
	}
	return false;
}

void LLTabContainer::setTabPanelFlashing(LLPanel* child, bool state)
{
	LLTabTuple* tuple = getTabByPanel(child);
	if (tuple && tuple->mButton)
	{
		tuple->mButton->setFlashing(state);
	}
}

void LLTabContainer::setTabButtonTooltip(LLPanel* child,
										 const std::string& tooltip)
{
	for (tuple_list_t::iterator iter = mTabList.begin(),
								end = mTabList.end();
		 iter != end; ++iter)
	{
		LLTabTuple* tuple = *iter;
		if (tuple->mTabPanel == child)
		{
			if (!mIsVertical && tooltip.empty())
			{
				std::string deflt = tuple->mButton->getLabelUnselected();
				deflt += "\nAlt-Left arrow for previous tab";
				deflt += "\nAlt-Right arrow for next tab";
				tuple->mButton->setToolTip(deflt);
				return;
			}
			tuple->mButton->setToolTip(tooltip);
			return;
		}
	}
}

void LLTabContainer::setTabImage(LLPanel* child, std::string image_name,
								 const LLColor4& color)
{
	LLTabTuple* tuple = getTabByPanel(child);
	if (!tuple) return;

	LLButton* button = tuple->mButton;
	if (!button) return;

	button->setImageOverlay(image_name, LLFontGL::RIGHT, color);

	if (mIsVertical)
	{
		return;
	}

	// Remove current width from total tab strip width
	mTotalTabWidth -= button->getRect().getWidth();

	S32 image_overlay_width = 0;
	if (button->getImageOverlay().notNull())
	{
		image_overlay_width = button->getImageOverlay()->getImage()->getWidth(0);
	}
	tuple->mPadding = image_overlay_width;

	button->setRightHPad(6);

	static const LLFontGL* fontp = LLFontGL::getFontSansSerifSmall();
	button->reshape(llclamp(fontp->getWidth(button->getLabelSelected()) +
							TAB_PADDING + tuple->mPadding,
							mMinTabWidth, mMaxTabWidth),
							button->getRect().getHeight());

	// Add back in button width to total tab strip width
	mTotalTabWidth += button->getRect().getWidth();

	// Tabs have changed size, might need to scroll to see current tab
	updateMaxScrollPos();
}

void LLTabContainer::setTitle(const std::string& title)
{
	if (mTitleBox)
	{
		mTitleBox->setText(title);
	}
}

const std::string LLTabContainer::getPanelTitle(S32 index)
{
	if (index >= 0 && index < (S32)mTabList.size())
	{
		LLButton* tab_button = mTabList[index]->mButton;
		return tab_button->getLabelSelected();
	}
	return LLStringUtil::null;
}

void LLTabContainer::setTopBorderHeight(S32 height)
{
	mTopBorderHeight = height;
}

S32 LLTabContainer::getTopBorderHeight() const
{
	return mTopBorderHeight;
}

void LLTabContainer::setTabChangeCallback(LLPanel* tab,
										  void (*on_tab_clicked)(void*, bool))
{
	LLTabTuple* tuplep = getTabByPanel(tab);
	if (tuplep)
	{
		tuplep->mOnChangeCallback = on_tab_clicked;
	}
}

void LLTabContainer::setTabPrecommitChangeCallback(LLPanel* tab,
												   void (*on_precommit)(void*, bool))
{
	LLTabTuple* tuplep = getTabByPanel(tab);
	if (tuplep)
	{
		tuplep->mPrecommitChangeCallback = on_precommit;
	}
}

void LLTabContainer::setTabUserData(LLPanel* tab, void* userdata)
{
	LLTabTuple* tuplep = getTabByPanel(tab);
	if (tuplep)
	{
		tuplep->mUserData = userdata;
	}
}

void LLTabContainer::setRightTabBtnOffset(S32 offset)
{
	mNextArrowBtn->translate(-offset - mRightTabBtnOffset, 0);
	mRightTabBtnOffset = offset;
	updateMaxScrollPos();
}

void LLTabContainer::setPanelTitle(S32 index, const std::string& title)
{
	static const LLFontGL* fontp = LLFontGL::getFontSansSerifSmall();

	if (index >= 0 && index < getTabCount())
	{
		LLTabTuple* tuple = getTab(index);
		if (tuple)
		{
			LLButton* tab_button = tuple->mButton;
			if (tab_button)
			{
				mTotalTabWidth -= tab_button->getRect().getWidth();
				tab_button->reshape(llclamp(fontp->getWidth(title) +
											TAB_PADDING + tuple->mPadding,
											mMinTabWidth, mMaxTabWidth),
									tab_button->getRect().getHeight());
				mTotalTabWidth += tab_button->getRect().getWidth();
				tab_button->setLabelSelected(title);
				tab_button->setLabelUnselected(title);
			}
		}
	}
	updateMaxScrollPos();
}

// static
void LLTabContainer::onTabBtn(void* userdata)
{
	LLTabTuple* tuple = (LLTabTuple*)userdata;
	if (tuple)
	{
		LLTabContainer* self = tuple->mTabContainer;
		if (self)
		{
			self->selectTabPanel(tuple->mTabPanel);
		}
		tuple->mTabPanel->setFocus(true);
	}
}

// static
void LLTabContainer::onCloseBtn(void* userdata)
{
	LLTabContainer* self = (LLTabContainer*)userdata;
	if (self && self->mCloseCallback)
	{
		self->mCloseCallback(self->mCallbackUserdata);
	}
}

// static
void LLTabContainer::onNextBtn(void* userdata)
{
	// Scroll tabs to the left
	LLTabContainer* self = (LLTabContainer*)userdata;
	if (!self) return;

	if (!self->mScrolled)
	{
		self->scrollNext();
	}
	self->mScrolled = false;

	if ((size_t)self->mCurrentTabIdx < self->mTabList.size() - 1)
	{
		self->selectNextTab();
	}
}

// static
void LLTabContainer::onNextBtnHeld(void* userdata)
{
	LLTabContainer* self = (LLTabContainer*)userdata;
	if (self && self->mScrollTimer.getElapsedTimeF32() > SCROLL_STEP_TIME)
	{
		self->mScrollTimer.reset();
		self->scrollNext();
		if ((size_t)self->mCurrentTabIdx < self->mTabList.size() - 1)
		{
			self->selectNextTab();
		}
		self->mScrolled = true;
	}
}

// static
void LLTabContainer::onPrevBtn(void* userdata)
{
	LLTabContainer* self = (LLTabContainer*)userdata;
	if (self)
	{
		if (!self->mScrolled)
		{
			self->scrollPrev();
		}
		self->mScrolled = false;

		if (self->mCurrentTabIdx > 0)
		{
			self->selectPrevTab();
		}
	}
}

// static
void LLTabContainer::onJumpFirstBtn(void* userdata)
{
	LLTabContainer* self = (LLTabContainer*)userdata;
	if (self)
	{
		self->mScrollPos = 0;
	}
}

// static
void LLTabContainer::onJumpLastBtn(void* userdata)
{
	LLTabContainer* self = (LLTabContainer*)userdata;
	if (self)
	{
		self->mScrollPos = self->mMaxScrollPos;
	}
}

// static
void LLTabContainer::onPrevBtnHeld(void* userdata)
{
	LLTabContainer* self = (LLTabContainer*)userdata;
	if (self && self->mScrollTimer.getElapsedTimeF32() > SCROLL_STEP_TIME)
	{
		self->mScrollTimer.reset();
		self->scrollPrev();
		if (self->mCurrentTabIdx > 0)
		{
			self->selectPrevTab();
		}
		self->mScrolled = true;
	}
}

//static
LLView* LLTabContainer::fromXML(LLXMLNodePtr nodep, LLView* parentp,
								LLUICtrlFactory* factoryp)
{
	std::string name = LL_TAB_CONTAINER_COMMON_TAG;
	nodep->getAttributeString("name", name);

	// Figure out if we are creating a vertical or horizontal tab container.
	bool is_vertical = false;
	TabPosition tab_position = LLTabContainer::TOP;
	if (nodep->hasAttribute("tab_position"))
	{
		std::string tab_position_string;
		nodep->getAttributeString("tab_position", tab_position_string);
		LLStringUtil::toLower(tab_position_string);

		if (tab_position_string == "top")
		{
			tab_position = LLTabContainer::TOP;
			is_vertical = false;
		}
		else if (tab_position_string == "bottom")
		{
			tab_position = LLTabContainer::BOTTOM;
			is_vertical = false;
		}
		else if (tab_position_string == "left")
		{
			is_vertical = true;
		}
	}

	bool border = false;
	nodep->getAttributeBool("border", border);

	LLTabContainer*	containerp = new LLTabContainer(name, LLRect::null,
													tab_position, border,
													is_vertical);

	S32 tab_min_width = containerp->mMinTabWidth;
	if (nodep->hasAttribute("tab_width"))
	{
		nodep->getAttributeS32("tab_width", tab_min_width);
	}
	else if (nodep->hasAttribute("tab_min_width"))
	{
		nodep->getAttributeS32("tab_min_width", tab_min_width);
	}

	S32	tab_max_width = containerp->mMaxTabWidth;
	if (nodep->hasAttribute("tab_max_width"))
	{
		nodep->getAttributeS32("tab_max_width", tab_max_width);
	}

	containerp->setMinTabWidth(tab_min_width);
	containerp->setMaxTabWidth(tab_max_width);

	bool hidden = containerp->getTabsHidden();
	nodep->getAttributeBool("hide_tabs", hidden);
	containerp->setTabsHidden(hidden);

	containerp->setPanelParameters(nodep, parentp);

	if (LLFloater::getFloaterHost())
	{
		LLFloater::getFloaterHost()->setTabContainer(containerp);
	}

#if 0
	parentp->addChild(containerp);
#endif

	// Add all tab panels.
	std::string label;
	for (LLXMLNodePtr childp = nodep->getFirstChild(); childp.notNull();
		 childp = childp->getNextSibling())
	{
		LLView* controlp = factoryp->createCtrlWidget(containerp, childp);
		// Yes, it may happen with bad XUI files. HB
		if (!controlp)
		{
			continue;
		}

		LLPanel* panelp = controlp->asPanel();
		if (panelp)
		{
			childp->getAttributeString("label", label);
			if (label.empty())
			{
				label = panelp->getLabel();
			}
			bool placeholder = false;
			childp->getAttributeBool("placeholder", placeholder);
			containerp->addTabPanel(panelp, label, false, NULL, NULL, 0,
									placeholder);
			label.clear();	// Must be empty for next getAttributeString() call
		}
	}

	containerp->selectFirstTab();

	containerp->postBuild();

	containerp->initButtons(); // Now that we have the correct rect

	return containerp;
}

void LLTabContainer::initButtons()
{
	// *HACK:
	if (getRect().getHeight() == 0 || mPrevArrowBtn)
	{
		return; // Do not have a rect yet or already got called
	}

	std::string out_id;
	std::string in_id;

	if (mIsVertical)
	{
		// Left and right scroll arrows (for when there are too many tabs to show all at once).
		S32 btn_top = getRect().getHeight();
		S32 btn_top_lower = getRect().mBottom+TABCNTRV_ARROW_BTN_SIZE;

		LLRect up_arrow_btn_rect;
		up_arrow_btn_rect.setLeftTopAndSize(mMinTabWidth / 2, btn_top,
											TABCNTRV_ARROW_BTN_SIZE,
											TABCNTRV_ARROW_BTN_SIZE);

		LLRect down_arrow_btn_rect;
		down_arrow_btn_rect.setLeftTopAndSize(mMinTabWidth / 2, btn_top_lower,
											  TABCNTRV_ARROW_BTN_SIZE,
											  TABCNTRV_ARROW_BTN_SIZE);

		out_id = "UIImgBtnScrollUpOutUUID";
		in_id = "UIImgBtnScrollUpInUUID";
		mPrevArrowBtn = new LLButton("Up Arrow", up_arrow_btn_rect, out_id,
									 in_id, NULL, &onPrevBtn, this, NULL);
		mPrevArrowBtn->setFollowsTop();
		mPrevArrowBtn->setFollowsLeft();

		out_id = "UIImgBtnScrollDownOutUUID";
		in_id = "UIImgBtnScrollDownInUUID";
		mNextArrowBtn = new LLButton("Down Arrow", down_arrow_btn_rect, out_id,
									 in_id, NULL, &onNextBtn, this, NULL);
		mNextArrowBtn->setFollowsBottom();
		mNextArrowBtn->setFollowsLeft();
	}
	else // Horizontal
	{
		S32 arrow_fudge = 1;	//  match new art better

		// tabs on bottom reserve room for resize handle (just in case)
		if (getTabPosition() == BOTTOM)
		{
			mRightTabBtnOffset = RESIZE_HANDLE_WIDTH;
		}

		// Left and right scroll arrows (for when there are too many tabs to
		// show all at once).
		S32 btn_top = getTabPosition() == TOP ?
				getRect().getHeight() - getTopBorderHeight() :
				TABCNTR_ARROW_BTN_SIZE + 1;

		LLRect left_arrow_btn_rect;
		left_arrow_btn_rect.setLeftTopAndSize(LLPANEL_BORDER_WIDTH +
											  TABCNTR_ARROW_BTN_SIZE + 1,
											  btn_top + arrow_fudge,
											  TABCNTR_ARROW_BTN_SIZE,
											  TABCNTR_ARROW_BTN_SIZE);

		LLRect jump_left_arrow_btn_rect;
		jump_left_arrow_btn_rect.setLeftTopAndSize(LLPANEL_BORDER_WIDTH + 1,
												   btn_top + arrow_fudge,
												   TABCNTR_ARROW_BTN_SIZE,
												   TABCNTR_ARROW_BTN_SIZE);

		S32 right_pad = TABCNTR_ARROW_BTN_SIZE + LLPANEL_BORDER_WIDTH + 1;

		LLRect right_arrow_btn_rect;
		right_arrow_btn_rect.setLeftTopAndSize(getRect().getWidth() -
											   mRightTabBtnOffset - right_pad -
											   TABCNTR_ARROW_BTN_SIZE,
											   btn_top + arrow_fudge,
											   TABCNTR_ARROW_BTN_SIZE,
											   TABCNTR_ARROW_BTN_SIZE);

		LLRect jump_right_arrow_btn_rect;
		jump_right_arrow_btn_rect.setLeftTopAndSize(getRect().getWidth() -
													mRightTabBtnOffset -
													right_pad,
													btn_top + arrow_fudge,
													TABCNTR_ARROW_BTN_SIZE,
													TABCNTR_ARROW_BTN_SIZE);

		static const LLFontGL* font = LLFontGL::getFontSansSerif();
		out_id = "UIImgBtnJumpLeftOutUUID";
		in_id = "UIImgBtnJumpLeftInUUID";
		mJumpPrevArrowBtn = new LLButton("Jump Left Arrow",
										 jump_left_arrow_btn_rect, out_id,
										 in_id, NULL,
										 &LLTabContainer::onJumpFirstBtn,
										 this, font);
		mJumpPrevArrowBtn->setFollowsLeft();

		out_id = "UIImgBtnScrollLeftOutUUID";
		in_id = "UIImgBtnScrollLeftInUUID";
		mPrevArrowBtn = new LLButton("Left Arrow", left_arrow_btn_rect,
									 out_id, in_id, NULL,
									 &LLTabContainer::onPrevBtn,
									 this, font);
		mPrevArrowBtn->setHeldDownCallback(onPrevBtnHeld);
		mPrevArrowBtn->setFollowsLeft();

		out_id = "UIImgBtnJumpRightOutUUID";
		in_id = "UIImgBtnJumpRightInUUID";
		mJumpNextArrowBtn = new LLButton("Jump Right Arrow",
										 jump_right_arrow_btn_rect,
										 out_id, in_id, NULL,
										 &LLTabContainer::onJumpLastBtn,
										 this, font);
		mJumpNextArrowBtn->setFollowsRight();

		out_id = "UIImgBtnScrollRightOutUUID";
		in_id = "UIImgBtnScrollRightInUUID";
		mNextArrowBtn = new LLButton("Right Arrow", right_arrow_btn_rect,
									 out_id, in_id, NULL,
									 &LLTabContainer::onNextBtn,
									 this, font);
		mNextArrowBtn->setFollowsRight();

		if (getTabPosition() == TOP)
		{
			mNextArrowBtn->setFollowsTop();
			mPrevArrowBtn->setFollowsTop();
			mJumpPrevArrowBtn->setFollowsTop();
			mJumpNextArrowBtn->setFollowsTop();
		}
		else
		{
			mNextArrowBtn->setFollowsBottom();
			mPrevArrowBtn->setFollowsBottom();
			mJumpPrevArrowBtn->setFollowsBottom();
			mJumpNextArrowBtn->setFollowsBottom();
		}
	}

	mPrevArrowBtn->setHeldDownCallback(onPrevBtnHeld);
	mPrevArrowBtn->setSaveToXML(false);
	mPrevArrowBtn->setTabStop(false);
	addChild(mPrevArrowBtn);

	mNextArrowBtn->setHeldDownCallback(onNextBtnHeld);
	mNextArrowBtn->setSaveToXML(false);
	mNextArrowBtn->setTabStop(false);
	addChild(mNextArrowBtn);

	if (mJumpPrevArrowBtn)
	{
		mJumpPrevArrowBtn->setSaveToXML(false);
		mJumpPrevArrowBtn->setTabStop(false);
		addChild(mJumpPrevArrowBtn);
	}

	if (mJumpNextArrowBtn)
	{
		mJumpNextArrowBtn->setSaveToXML(false);
		mJumpNextArrowBtn->setTabStop(false);
		addChild(mJumpNextArrowBtn);
	}

	// Set default tab group to be panel contents
	setDefaultTabGroup(1);
}

LLTabContainer::LLTabTuple* LLTabContainer::getTabByPanel(LLPanel* child)
{
	for (tuple_list_t::iterator iter = mTabList.begin(),
								end = mTabList.end();
		 iter != end; ++iter)
	{
		LLTabTuple* tuple = *iter;
		if (tuple->mTabPanel == child)
		{
			return tuple;
		}
	}
	return NULL;
}

void LLTabContainer::insertTuple(LLTabTuple* tuple,
								 eInsertionPoint insertion_point)
{
	switch (insertion_point)
	{
		case START:
			// Insert the new tab in the front of the list
			mTabList.insert(mTabList.begin() + mLockedTabCount, tuple);
			break;

		case LEFT_OF_CURRENT:
		{
			// Insert the new tab before the current tab (but not before
			// mLockedTabCount)

			tuple_list_t::iterator current_iter = mTabList.begin() +
												  llmax(mLockedTabCount,
														mCurrentTabIdx);
			mTabList.insert(current_iter, tuple);
			break;
		}

		case RIGHT_OF_CURRENT:
		{
			// Insert the new tab after the current tab (but not before
			// mLockedTabCount)
			tuple_list_t::iterator current_iter = mTabList.begin() +
												  llmax(mLockedTabCount,
														mCurrentTabIdx + 1);
			mTabList.insert(current_iter, tuple);
			break;
		}

		case END:
		default:
			mTabList.push_back(tuple);
	}
}

void LLTabContainer::updateMaxScrollPos()
{
	bool no_scroll = true;
	if (mIsVertical)
	{
		S32 tab_total_height = (gBtnHeight + TABCNTRV_PAD) * getTabCount();
		S32 available_height = getRect().getHeight() - getTopBorderHeight();
		if (tab_total_height > available_height)
		{
			S32 available_height_with_arrows =
				getRect().getHeight() -
				2 * (TABCNTRV_ARROW_BTN_SIZE + 3 * TABCNTRV_PAD);
			S32 additional_needed = tab_total_height -
									available_height_with_arrows;
			setMaxScrollPos((S32) ceil(additional_needed / F32(gBtnHeight)));
			no_scroll = false;
		}
	}
	else
	{
		S32 tab_space = 0;
		S32 available_space = 0;
		tab_space = mTotalTabWidth;
		available_space = getRect().getWidth() - mRightTabBtnOffset -
						  2 * (LLPANEL_BORDER_WIDTH + TABCNTR_TAB_H_PAD);

		if (tab_space > available_space)
		{
			S32 available_width_with_arrows = getRect().getWidth() -
											  mRightTabBtnOffset -
											  2 * TABCNTR_TAB_BTN_MARGIN;
			// Subtract off reserved portion on left
			available_width_with_arrows -= TABCNTR_TAB_PARTIAL_WIDTH;

			S32 running_tab_width = 0;
			setMaxScrollPos(getTabCount());
			for (tuple_list_t::reverse_iterator tab_it = mTabList.rbegin(),
												rend = mTabList.rend();
				 tab_it != rend; ++tab_it)
			{
				running_tab_width += (*tab_it)->mButton->getRect().getWidth();
				if (running_tab_width > available_width_with_arrows)
				{
					break;
				}
				setMaxScrollPos(getMaxScrollPos() - 1);
			}
			// In case last tab does not actually fit on screen, make it the
			// last scrolling position
			setMaxScrollPos(llmin(getMaxScrollPos(), getTabCount() - 1));
			no_scroll = false;
		}
	}
	if (no_scroll)
	{
		setMaxScrollPos(0);
		setScrollPos(0);
	}
	if (getScrollPos() > getMaxScrollPos())
	{
		// Maybe just enforce this via limits in setScrollPos instead ?
		setScrollPos(getMaxScrollPos());
	}
}

void LLTabContainer::commitHoveredButton(S32 x, S32 y)
{
	if (hasMouseCapture())
	{
		for (tuple_list_t::iterator iter = mTabList.begin(),
									end = mTabList.end();
			 iter != end; ++iter)
		{
			LLTabTuple* tuple = *iter;
			if (!tuple || !tuple->mTabPanel) continue;
			LLButton* tab_button = tuple->mButton;
			if (!tab_button) continue;
			tab_button->setVisible(true);
			S32 local_x = x - tab_button->getRect().mLeft;
			S32 local_y = y - tab_button->getRect().mBottom;
			if (tab_button->pointInView(local_x, local_y) &&
				tab_button->getEnabled() && !tuple->mTabPanel->getVisible())
			{
				tab_button->onCommit();
			}
		}
	}
}
