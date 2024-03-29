/**
 * @file lltool.cpp
 * @brief LLTool class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lltool.h"

#include "llview.h"
#include "llwindow.h"				// For gDebugClicks

#include "llagent.h"
#include "lltoolcomp.h"
#include "lltoolfocus.h"
#include "llviewerjoystick.h"
#include "llviewerwindow.h"

//static
const std::string LLTool::sNameNull("null");

LLTool::LLTool(const std::string& name, LLToolComposite* composite)
:	mComposite(composite),
	mName(name)
{
}

LLTool::~LLTool()
{
	if (hasMouseCapture())
	{
		llwarns << "Tool deleted holding mouse capture. Mouse capture removed."
				<< llendl;
		gFocusMgr.removeMouseCaptureWithoutCallback(this);
	}
}

bool LLTool::handleHover(S32 x, S32 y, MASK mask)
{
	gWindowp->setCursor(UI_CURSOR_ARROW);
	LL_DEBUGS("UserInput") << "hover handled by a tool" << LL_ENDL;
	// by default, do nothing, say we handled it
	return true;
}

bool LLTool::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (gDebugClicks)
	{
		llinfos << "Left mouse down" << llendl;
	}
	// by default, didn't handle it
	gAgent.setControlFlags(AGENT_CONTROL_LBUTTON_DOWN);
	return true;
}

bool LLTool::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (gDebugClicks)
	{
		llinfos << "Left mouse up" << llendl;
	}
	// by default, didn't handle it
	gAgent.setControlFlags(AGENT_CONTROL_LBUTTON_UP);
	return true;
}

void LLTool::setMouseCapture(bool b)
{
	if (b)
	{
		gFocusMgr.setMouseCapture(mComposite ? mComposite : this);
	}
	else if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
	}
}

LLTool* LLTool::getOverrideTool(MASK mask)
{
	// NOTE: if in flycam mode, ALT-ZOOM camera should be disabled
	if (LLViewerJoystick::getInstance()->getOverrideCamera())
	{
		return NULL;
	}
	if (mask & MASK_ALT)
	{
		return &gToolFocus;
	}
	return NULL;
}
