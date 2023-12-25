/**
 * @file lltoolobjpicker.cpp
 * @brief LLToolObjPicker class implementation
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

// LLToolObjPicker is a transient tool, useful for a single object pick.

#include "llviewerprecompiledheaders.h"

#include "lltoolobjpicker.h"

#include "llagent.h"
#include "lltoolmgr.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"

LLToolObjPicker gToolObjPicker;

LLToolObjPicker::LLToolObjPicker()
:	LLTool("ObjPicker", NULL),
	mExitCallback(NULL),
	mExitCallbackData(NULL),
	mPicked(false)
{
}

// Returns true if an object was selected
//virtual
bool LLToolObjPicker::handleMouseDown(S32 x, S32 y, MASK mask)
{
	LLView* viewp = gViewerWindowp->getRootView();
	bool handled = viewp->handleMouseDown(x, y, mask);

	mHitObjectID.setNull();

	if (!handled)
	{
		// didn't click in any UI object, so must have clicked in the world
		gViewerWindowp->pickAsync(x, y, mask, pickCallback);
		handled = true;
	}
	else
	{
		if (hasMouseCapture())
		{
			setMouseCapture(false);
		}
		else
		{
			llwarns << "PickerTool doesn't have mouse capture on mouseDown"
					<< llendl;
		}
	}

	// Pass mousedown to base class
	LLTool::handleMouseDown(x, y, mask);

	return handled;
}

//static
void LLToolObjPicker::pickCallback(const LLPickInfo& pick_info)
{
	gToolObjPicker.mHitObjectID = pick_info.mObjectID;
	gToolObjPicker.mPicked = pick_info.mObjectID.notNull();
}

//virtual
bool LLToolObjPicker::handleMouseUp(S32 x, S32 y, MASK mask)
{
	LLView* viewp = gViewerWindowp->getRootView();
	// Let the UI handle this
	bool handled = viewp->handleHover(x, y, mask);
	LLTool::handleMouseUp(x, y, mask);
	if (hasMouseCapture())
	{
		setMouseCapture(false);
	}
	else
	{
		llwarns_sparse << "No capture on mouse up" << llendl;
	}
	return handled;
}

//virtual
bool LLToolObjPicker::handleHover(S32 x, S32 y, MASK mask)
{
	LLView* viewp = gViewerWindowp->getRootView();
	bool handled = viewp->handleHover(x, y, mask);
	if (!handled)
	{
		// Used to do pick on hover. Now we just always display the cursor.
		ECursorType cursor = UI_CURSOR_ARROWLOCKED;

		cursor = UI_CURSOR_TOOLPICKOBJECT3;

		gWindowp->setCursor(cursor);
	}
	return handled;
}

//virtual
void LLToolObjPicker::onMouseCaptureLost()
{
	if (mExitCallback)
	{
		mExitCallback(mExitCallbackData);

		mExitCallback = NULL;
		mExitCallbackData = NULL;
	}

	mPicked = false;
	mHitObjectID.setNull();
}


//virtual
void LLToolObjPicker::handleSelect()
{
	LLTool::handleSelect();
	setMouseCapture(true);
}

//virtual
void LLToolObjPicker::handleDeselect()
{
	if (hasMouseCapture())
	{
		LLTool::handleDeselect();
		setMouseCapture(false);
	}
}
