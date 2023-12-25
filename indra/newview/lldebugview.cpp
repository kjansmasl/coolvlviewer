/**
 * @file lldebugview.cpp
 * @brief A view containing UI elements only visible in build mode.
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

#include "lldebugview.h"

#include "llconsole.h"

#include "llfasttimerview.h"
#include "lltextureview.h"
#include "llvelocitybar.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"

// Instance created in LLViewerWindow::initBase()
LLDebugView* gDebugViewp = NULL;

LLDebugView::LLDebugView(const std::string& name, const LLRect& rect)
:	LLView(name, rect, false)
{
	LLRect r(CONSOLE_PADDING_LEFT, rect.getHeight() - 100,
			 rect.getWidth() - CONSOLE_PADDING_RIGHT, 100);
	mDebugConsolep = new LLConsole("Debug console", r, -1,
								   gSavedSettings.getU32("DebugConsoleMaxLines"),
								   0.f);
	if (mDebugConsolep)
	{
		mDebugConsolep->setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT |
								   FOLLOWS_BOTTOM);
		mDebugConsolep->setVisible(false);
		addChild(mDebugConsolep);
	}
	else
	{
		llwarns << "Unable to initialize the debug console !" << llendl;
	}

#if LL_FAST_TIMERS_ENABLED
	if (new LLFastTimerView("Fast timers"))
	{
		addChild(gFastTimerViewp);
	}
	else
	{
		llwarns << "Unable to initialize the fast timers view !" << llendl;
	}
#endif

	if (new LLTextureView("Texture view"))
	{
		addChild(gTextureViewp);
	}
	else
	{
		llwarns << "Unable to initialize the texture console !" << llendl;
	}

	if (new LLVelocityBar("Velocity bar"))
	{
		addChild(gVelocityBarp);
	}
	else
	{
		llwarns << "Unable to initialize the velocity bar !" << llendl;
	}
}

LLDebugView::~LLDebugView()
{
	gDebugViewp = NULL;
}
