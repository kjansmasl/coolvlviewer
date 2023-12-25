/** 
 * @file lltoolpipette.cpp
 * @brief LLToolPipette class implementation
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

/**
 * A tool to pick texture entry info from objects in world (color/texture)
 */

#include "llviewerprecompiledheaders.h"

#include "lltoolpipette.h" 

#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llselectmgr.h"
#include "lltoolmgr.h"

LLToolPipette gToolPipette;

LLToolPipette::LLToolPipette()
:	LLTool("Pipette"),
	mSelectCallback(NULL),
	mUserData(NULL),
	mSuccess(true)
{ 
}

bool LLToolPipette::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mSuccess = true;
	mTooltipMsg.clear();
	setMouseCapture(true);
	gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	return true;
}

bool LLToolPipette::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mSuccess = true;
	gSelectMgr.unhighlightAll();
	// *NOTE: This assumes the pipette tool is a transient tool.
	gToolMgr.clearTransientTool();
	setMouseCapture(false);
	return true;
}

bool LLToolPipette::handleHover(S32 x, S32 y, MASK mask)
{
	gViewerWindowp->setCursor(mSuccess ? UI_CURSOR_PIPETTE : UI_CURSOR_NO);
	if (hasMouseCapture()) // mouse button is down
	{
		gViewerWindowp->pickAsync(x, y, mask, pickCallback);
		return true;
	}
	return false;
}

bool LLToolPipette::handleToolTip(S32 x, S32 y, std::string& msg,
								  LLRect* sticky_rect_screen)
{
	if (mTooltipMsg.empty())
	{
		return false;
	}
	// Keep tooltip message up when mouse in this part of screen
	sticky_rect_screen->setCenterAndSize(x, y, 20, 20);
	msg = mTooltipMsg;
	return true;
}

void LLToolPipette::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* hit_obj	= pick_info.getObject();
	gSelectMgr.unhighlightAll();

	// If we clicked on a face of a valid prim, save off texture entry data
	if (hit_obj && 
		hit_obj->getPCode() == LL_PCODE_VOLUME &&
		pick_info.mObjectFace != -1)
	{
		// *TODO: this should highlight the selected face only
		gSelectMgr.highlightObjectOnly(hit_obj);
		gToolPipette.mTextureEntry = *hit_obj->getTE(pick_info.mObjectFace);
		if (gToolPipette.mSelectCallback)
		{
			gToolPipette.mSelectCallback(gToolPipette.mTextureEntry,
										 gToolPipette.mUserData);
		}
	}
}

void LLToolPipette::setSelectCallback(select_callback callback, void* user_data)
{
	mSelectCallback = callback;
	mUserData = user_data;
}

void LLToolPipette::setResult(bool success, const std::string& msg)
{
	mTooltipMsg = msg;
	mSuccess = success;
}
