/**
 * @file llscrollcontainer.h
 * @brief LLScrollableContainer class header file.
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

#ifndef LL_LLSCROLLCONTAINER_H
#define LL_LLSCROLLCONTAINER_H

#include "llcoord.h"
#include "llscrollbar.h"
#include "lluictrl.h"
#include "llcolor4.h"

class LLViewBorder;
class LLUICtrlFactory;

// A decorator view class meant to encapsulate a clipped region which is
// scrollable. It automatically takes care of pixel perfect scrolling and
// clipping, as well as turning the scrollbars on or off based on the width
// and height of the view you're scrolling.

class LLScrollableContainer : public LLUICtrl
{
protected:
	LOG_CLASS(LLScrollableContainer);

public:
	// Note: vertical comes before horizontal because vertical
	// scrollbars have priority for mouse and keyboard events.
	enum SCROLL_ORIENTATION { VERTICAL, HORIZONTAL, SCROLLBAR_COUNT };

	LLScrollableContainer(const std::string& name, const LLRect& rect,
						  LLView* scrolled_view, bool is_opaque = false,
						  const LLColor4& bg_color = LLColor4(0, 0, 0, 0));
	LLScrollableContainer(const std::string& name, const LLRect& rect,
						  LLUICtrl* scrolled_ctrl, bool is_opaque = false,
						  const LLColor4& bg_color = LLColor4(0, 0, 0, 0));
	~LLScrollableContainer() override;

	LL_INLINE void setScrolledView(LLView* view)		{ mScrolledView = view; }

	LL_INLINE void setValue(const LLSD& v) override		{ mInnerRect.setValue(v); }

	void calcVisibleSize(S32* visible_width, S32* visible_height,
						 bool* show_h_scrollbar, bool* show_v_scrollbar) const;
	void calcVisibleSize(const LLRect& doc_rect, S32* visible_width,
						 S32* visible_height, bool* show_h_scrollbar,
						 bool* show_v_scrollbar) const;
	void setBorderVisible(bool b);

	void scrollToShowRect(const LLRect& rect, const LLCoordGL& desired_offset);

	LL_INLINE void setReserveScrollCorner(bool b)		{ mReserveScrollCorner = b; }
	LL_INLINE const LLRect& getScrolledViewRect() const	{ return mScrolledView->getRect(); }
	LLRect getContentWindowRect();

	void pageUp(S32 overlap = 0);
	void pageDown(S32 overlap = 0);
	void goToTop();
	void goToBottom();
	S32 getBorderWidth() const;

	bool needsToScroll(S32 x, S32 y, SCROLL_ORIENTATION axis) const;

	// LLView functionality

	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type,
						   void* cargo_data, EAcceptance* accept,
						   std::string& tooltip_msg) override;

	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* rect) override;
	void draw() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

private:
	void init();

	// Internal scrollbar handlers
	virtual void scrollHorizontal(S32 new_pos);
	virtual void scrollVertical(S32 new_pos);
	void updateScroll();

private:
	LLScrollbar*	mScrollbar[SCROLLBAR_COUNT];
	LLView*			mScrolledView;
	LLColor4		mBackgroundColor;
	LLRect			mInnerRect;
	LLViewBorder*	mBorder;
	F32				mAutoScrollRate;
	bool			mAutoScrolling;
	bool			mReserveScrollCorner;
	bool			mIsOpaque;
};

#endif // LL_LLSCROLLCONTAINER_H
