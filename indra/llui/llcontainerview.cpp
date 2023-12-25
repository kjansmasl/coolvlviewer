/**
 * @file llcontainerview.cpp
 * @brief Container for all statistics info
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

#include "llcontainerview.h"

#include "llgl.h"
#include "llscrollcontainer.h"

LLContainerView::LLContainerView(const std::string& name, const LLRect& rect)
:	LLView(name, rect, false),
	mShowLabel(true),
	mCanCollapse(true),
	mDisplayChildren(true),
	mScrollContainer(NULL)
{
	setDisplayChildren(true);
#if 0	// Do not do that: this prevents auto-sizing (e.g. in
		// llfloaterjoystick.cpp) and breaks the layout... HB
	reshape(rect.getWidth(), rect.getHeight(), false);
#endif
}

bool LLContainerView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mDisplayChildren && LLView::childrenHandleMouseDown(x, y, mask))
	{
			return true;
	}

	if (mCanCollapse && mShowLabel && y >= getRect().getHeight() - 10)
	{
		setDisplayChildren(!mDisplayChildren);
		reshape(getRect().getWidth(), getRect().getHeight(), false);
		return true;
	}

	return false;
}

bool LLContainerView::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (mDisplayChildren)
	{
		return LLView::childrenHandleMouseUp(x, y, mask) != NULL;
	}
	return false;
}

void LLContainerView::draw()
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	static const LLColor4 bg_color(0.f, 0.f, 0.f, 0.25f);
	S32 height = getRect().getHeight();
	gl_rect_2d(0, height, getRect().getWidth(), 0, bg_color);

	// Draw the label
	if (mShowLabel)
	{
		static const LLFontGL* fontp = LLFontGL::getFontMonospace();
		fontp->renderUTF8(mLabel, 0, 2, height - 2, LLColor4::white,
						  LLFontGL::LEFT, LLFontGL::TOP);
	}
	LLView::draw();
}

void LLContainerView::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLRect scroller_rect;
	if (mScrollContainer)
	{
		scroller_rect = mScrollContainer->getContentWindowRect();
	}
	else
	{
		// If we are uncontained, make height as small as possible
		scroller_rect.setOriginAndSize(0, 0, width, height);
		scroller_rect.mTop = 0;
	}

	arrange(scroller_rect.getWidth(), scroller_rect.getHeight(),
			called_from_parent);

	if (!mScrollContainer)
	{
		return;
	}

	// Sometimes, after layout, our container will change size (scrollbars
	// popping in and out). If so, attempt another layout.
	LLRect new_container_rect = mScrollContainer->getContentWindowRect();
	if (new_container_rect.getWidth() != scroller_rect.getWidth() ||
		new_container_rect.getHeight() != scroller_rect.getHeight())
	{
		// The container size has changed, attempt to arrange again
		arrange(new_container_rect.getWidth(), new_container_rect.getHeight(),
				called_from_parent);
	}
}

// Determines the sizes and locations of all contained views
void LLContainerView::arrange(S32 width, S32 height, bool called_from_parent)
{
	// These will be used for the children
	S32 left = 4;
	S32 top = getRect().getHeight() - 4;
	S32 right = width - 2;
	S32 bottom = top;

	// Leave some space for the top label/grab handle
	S32 total_height = 0;
	if (mShowLabel)
	{
		total_height += 20;
	}

	if (mDisplayChildren)
	{
		// Determine total height
		U32 child_height = 0;
		for (child_list_const_iter_t it = getChildList()->begin(),
									 end = getChildList()->end();
			 it != end; ++it)
		{
			LLView* childp = *it;
			if (!childp->getVisible())
			{
				llwarns << "Incorrect visibility !" << llendl;
			}
			child_height += childp->getRequiredRect().getHeight();
			child_height += 2;
		}
		total_height += child_height;
	}

	if (total_height < height)
	{
		total_height = height;
	}

	LLRect my_rect = getRect();
	if (followsTop())
	{
		my_rect.mBottom = my_rect.mTop - total_height;
	}
	else
	{
		my_rect.mTop = my_rect.mBottom + total_height;
	}
	my_rect.mRight = my_rect.mLeft + width;
	setRect(my_rect);

	top = total_height;
	if (mShowLabel)
	{
		top -= 20;
	}

	bottom = top;

	if (mDisplayChildren)
	{
		LLRect new_rect;
		// Iterate through all children, and put in container from top down.
		for (child_list_const_iter_t it = getChildList()->begin(),
									 end = getChildList()->end();
			 it != end; ++it)
		{
			LLView* childp = *it;
			S32 height = childp->getRequiredRect().getHeight();
			bottom -= height;
			new_rect.set(left, bottom + height, right, bottom);
			childp->setRect(new_rect);
			childp->reshape(right - left, top - bottom);
			top = bottom - 2;
			bottom = top;
		}
	}

	if (!called_from_parent)
	{
		LLView* parentp = getParent();
		if (parentp)
		{
			const LLRect& rect = parentp->getRect();
			parentp->reshape(rect.getWidth(), rect.getHeight(), false);
		}
	}
}

LLRect LLContainerView::getRequiredRect()
{
	LLRect req_rect;
	U32 total_height = 0;

	// Determine the sizes and locations of all contained views

	// Leave some space for the top label/grab handle
	if (mShowLabel)
	{
		total_height = 20;
	}

	if (mDisplayChildren)
	{
		// Determine total height
		for (child_list_const_iter_t it = getChildList()->begin(),
									 end = getChildList()->end();
			 it != end; ++it)
		{
			LLView* childp = *it;
			total_height += childp->getRequiredRect().getHeight() + 2;
		}
	}

	req_rect.mTop = total_height;
	return req_rect;
}

void LLContainerView::setDisplayChildren(bool display)
{
	mDisplayChildren = display;
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* childp = *it;
		childp->setVisible(display);
	}
}
