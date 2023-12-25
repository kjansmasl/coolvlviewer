/**
 * @file lldraghandle.h
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

#ifndef LL_DRAGHANDLE_H
#define LL_DRAGHANDLE_H

#include "llcoord.h"
#include "llrect.h"
#include "llview.h"
#include "llcolor4.h"

class LLFontGL;
class LLTextBox;

class LLDragHandle : public LLView
{
public:
	LLDragHandle(const std::string& name, const LLRect& rect,
				 const std::string& title);
	~LLDragHandle() override;

	void setValue(const LLSD& value) override;

	LL_INLINE void setForeground(bool b)	{ mForeground = b; }
	LL_INLINE bool getForeground() const	{ return mForeground; }
	LL_INLINE void setMaxTitleWidth(S32 w)	{ mMaxTitleWidth = llmin(w, mMaxTitleWidth); }
	LL_INLINE S32 getMaxTitleWidth() const	{ return mMaxTitleWidth; }
	void setTitleVisible(bool visible);

	virtual void setTitle(const std::string& title) = 0;
	virtual const std::string& getTitle() const = 0;

	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	// Mouse click (left mouse button down then up) special callback
	LL_INLINE void setClickedCallback(void (*cb)(S32, S32, void*),
									  void* data = NULL)
	{
		mClickedCallback = cb;
		mCallbackUserData = data;
	}

protected:
	void setTitleBox(LLTextBox* titlep);

protected:
	S32				mDragLastScreenX;
	S32				mDragLastScreenY;
	S32				mLastMouseScreenX;
	S32				mLastMouseScreenY;
	S32				mMaxTitleWidth;
	LLTextBox*		mTitleBox;
	LLCoordGL		mLastMouseDir;
	void			(*mClickedCallback)(S32 x, S32 y, void* data);
	void*			mCallbackUserData;
	bool			mForeground;
};

// Use this one for traditional top-of-window draggers
class LLDragHandleTop : public LLDragHandle
{
public:
	LLDragHandleTop(const std::string& name,
					const LLRect& rect,
					const std::string& title);

	void setTitle(const std::string& title) override;
	const std::string& getTitle() const override;

	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

private:
	void reshapeTitleBox();

private:
	LLFontGL* mFont;
};

// Use this for left-side, vertical text draggers
class LLDragHandleLeft : public LLDragHandle
{
public:
	LLDragHandleLeft(const std::string& name,
					 const LLRect& rect,
					 const std::string& title);

	void setTitle(const std::string& title) override;
	const std::string& getTitle() const override;

	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
};

constexpr S32 DRAG_HANDLE_HEIGHT = 16;
constexpr S32 DRAG_HANDLE_WIDTH = 16;

#endif  // LL_DRAGHANDLE_H
