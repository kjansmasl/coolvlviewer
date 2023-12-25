/**
 * @file llcontainerview.h
 * @brief Container for all statistics info.
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

#ifndef LL_LLCONTAINERVIEW_H
#define LL_LLCONTAINERVIEW_H

#include "llstatbar.h"
#include "lltextbox.h"

class LLScrollableContainer;

class LLContainerView : public LLView
{
protected:
	LOG_CLASS(LLContainerView);

public:
	LLContainerView(const std::string& name, const LLRect& rect);

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	// Returns the height of this object, given the set options:
	LLRect getRequiredRect() override;

	LL_INLINE void setLabel(const std::string& label)			{ mLabel = label; }
	LL_INLINE void showLabel(bool show)							{ mShowLabel = show; }

	void setDisplayChildren(bool display);
	LL_INLINE bool getDisplayChildren()							{ return mDisplayChildren; }

	LL_INLINE void setScrollContainer(LLScrollableContainer* scroll)
	{
		mScrollContainer = scroll;
	}

	LL_INLINE void setCanCollapse(bool b = true)				{ mCanCollapse = b; }

private:
	void arrange(S32 width, S32 height, bool called_from_parent = true);

private:
	LLScrollableContainer*	mScrollContainer;
	bool					mCanCollapse;
	bool					mShowLabel;

protected:
	bool					mDisplayChildren;
	std::string				mLabel;
};

#endif // LL_LLCONTAINERVIEW_H
