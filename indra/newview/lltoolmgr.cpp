/**
 * @file lltoolmgr.cpp
 * @brief LLToolMgr class implementation
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

#include "lltoolmgr.h"

#include "llagent.h"
#include "llapp.h"
#include "llfirstuse.h"
#include "llfloatertools.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltool.h"
#include "lltoolbrushland.h"
#include "lltoolcomp.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolpie.h"
#include "lltoolpipette.h"
#include "llviewercontrol.h"
#include "llviewerjoystick.h"
#include "llviewermenu.h"
#include "llviewerwindow.h"

// Globals

LLToolMgr gToolMgr;

// Used when app not active to avoid processing hover.
LLTool* gToolNull = NULL;

LLToolset* gBasicToolset = NULL;
LLToolset* gCameraToolset = NULL;
LLToolset* gMouselookToolset = NULL;
LLToolset* gFaceEditToolset = NULL;

/////////////////////////////////////////////////////
// LLToolMgr

LLToolMgr::LLToolMgr()
:	mBaseTool(NULL),
	mSavedTool(NULL),
	mTransientTool(NULL),
	mOverrideTool(NULL),
	mSelectedTool(NULL),
	mCurrentToolset(NULL)
{
	gToolNull = new LLTool(LLStringUtil::null);  // Does nothing
	setCurrentTool(gToolNull);

	gBasicToolset = new LLToolset();
	gCameraToolset = new LLToolset();
	gMouselookToolset = new LLToolset();
	gFaceEditToolset = new LLToolset();
}

void LLToolMgr::initTools()
{
	static bool initialized = false;
	if (initialized)
	{
		return;
	}
	initialized = true;

	gBasicToolset->addTool(&gToolPie);
	gBasicToolset->addTool(&gToolFocus);
	gCameraToolset->addTool(&gToolFocus);
	gBasicToolset->addTool(&gToolGrab);
	gBasicToolset->addTool(&gToolCompTranslate);
	gBasicToolset->addTool(&gToolCompCreate);
	gBasicToolset->addTool(&gToolBrushLand);
	gMouselookToolset->addTool(&gToolCompGun);
	gBasicToolset->addTool(&gToolCompInspect);
	gFaceEditToolset->addTool(&gToolFocus);

	// In case focus was lost before we got here
	clearSavedTool();
	// On startup, use "select" tool
	setCurrentToolset(gBasicToolset);

	gBasicToolset->selectTool(&gToolPie);
}

LLToolMgr::~LLToolMgr()
{
	delete gBasicToolset;
	gBasicToolset = NULL;

	delete gMouselookToolset;
	gMouselookToolset = NULL;

	delete gFaceEditToolset;
	gFaceEditToolset = NULL;

	delete gCameraToolset;
	gCameraToolset = NULL;

	delete gToolNull;
	gToolNull = NULL;
}

void LLToolMgr::setCurrentToolset(LLToolset* current)
{
	if (!current) return;

	// Switching toolsets ?
	if (current != mCurrentToolset)
	{
		// Deselect current tool
		if (mSelectedTool)
		{
			mSelectedTool->handleDeselect();
		}
		mCurrentToolset = current;
		// Select first tool of new toolset only if toolset changed
		mCurrentToolset->selectFirstTool();
	}
	// Update current tool based on new toolset
	setCurrentTool(mCurrentToolset->getSelectedTool());
}

void LLToolMgr::setCurrentTool(LLTool* tool)
{
	if (mTransientTool)
	{
		mTransientTool = NULL;
	}

	mBaseTool = tool;
	updateToolStatus();
}

LLTool* LLToolMgr::getCurrentTool()
{
	MASK override_mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;

	LLTool* cur_tool = NULL;
	// Always use transient tools if available
	if (mTransientTool)
	{
		mOverrideTool = NULL;
		cur_tool = mTransientTool;
	}
	// Tools currently grabbing mouse input will stay active
	else if (mSelectedTool && mSelectedTool->hasMouseCapture())
	{
		cur_tool = mSelectedTool;
	}
	else
	{
		// Do not override gToolNull
		mOverrideTool = mBaseTool && mBaseTool != gToolNull ?
				mBaseTool->getOverrideTool(override_mask) : NULL;

		// Use override tool if available otherwise drop back to base tool
		cur_tool = mOverrideTool ? mOverrideTool : mBaseTool;
	}

	LLTool* prev_tool = mSelectedTool;
	// Set the selected tool to avoid infinite recursion
	mSelectedTool = cur_tool;

	// Update tool selection status
	if (prev_tool != cur_tool)
	{
		if (prev_tool)
		{
			prev_tool->handleDeselect();
		}
		if (cur_tool)
		{
			cur_tool->handleSelect();
		}
	}

	return mSelectedTool;
}

bool LLToolMgr::inEdit()
{
	return mBaseTool != &gToolPie && mBaseTool != gToolNull;
}

void LLToolMgr::toggleBuildMode()
{
	if (!gViewerWindowp) return;

	if (LLFloaterTools::isVisible())
	{
		if (gSavedSettings.getBool("EditCameraMovement"))
		{
			// Just reset the view, will pull us out of edit mode
			handle_reset_view();
		}
		else
		{
			// Manually disable edit mode, but do not affect the camera
			gAgent.resetView(false);
			gFloaterToolsp->close();
			gViewerWindowp->showCursor();
		}
		// Avoid spurious avatar movements pulling out of edit mode
		LLViewerJoystick::getInstance()->setNeedsReset();
	}
	else
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsRez || gRLInterface.mContainsEdit))
		{
			return;
		}
//mk
		ECameraMode cam_mode = gAgent.getCameraMode();
		if (cam_mode == CAMERA_MODE_MOUSELOOK ||
			cam_mode == CAMERA_MODE_CUSTOMIZE_AVATAR)
		{
			// Pull the user out of mouselook or appearance mode when entering
			// build mode
			handle_reset_view();
		}

		if (gSavedSettings.getBool("EditCameraMovement"))
		{
			// Camera should be set
			if (LLViewerJoystick::getInstance()->getOverrideCamera())
			{
				LLViewerJoystick::getInstance()->toggleFlycam();
			}

			if (gAgent.getFocusOnAvatar())
			{
				// zoom in if we're looking at the avatar
				gAgent.setFocusOnAvatar(false);
				gAgent.setFocusGlobal(gAgent.getPositionGlobal() +
									  2.0 * LLVector3d(gAgent.getAtAxis()));
				gAgent.cameraZoomIn(0.666f);
				gAgent.cameraOrbitOver(30.f * DEG_TO_RAD);
			}
		}

		setCurrentToolset(gBasicToolset);
		getCurrentToolset()->selectTool(&gToolCompCreate);

		// Could be first use
		LLFirstUse::useBuild();

		gAgent.resetView(false);

		// Avoid spurious avatar movements
		LLViewerJoystick::getInstance()->setNeedsReset();
	}
}

bool LLToolMgr::inBuildMode()
{
	// When entering mouselook inEdit() immediately returns true before
	// cameraMouselook() actually starts returning true. Also, appearance edit
	// sets build mode to true, so let's exclude that.
	return inEdit() && mCurrentToolset != gFaceEditToolset &&
#if 0
		   LLFloaterTools::isVisible() &&
#endif
		   !gAgent.cameraMouselook();
}

void LLToolMgr::setTransientTool(LLTool* tool)
{
	if (!tool)
	{
		clearTransientTool();
	}
	else
	{
		if (mTransientTool)
		{
			mTransientTool = NULL;
		}

		mTransientTool = tool;
	}

	updateToolStatus();
}

void LLToolMgr::clearTransientTool()
{
	if (mTransientTool)
	{
		mTransientTool = NULL;
		if (!mBaseTool)
		{
			llwarns << "mBaseTool is NULL" << llendl;
		}
	}
	updateToolStatus();
}

void LLToolMgr::onAppFocusLost()
{
	if (LLApp::isExiting())
	{
		return;
	}
	if (mSelectedTool)
	{
		mSelectedTool->handleDeselect();
	}
	updateToolStatus();
}

void LLToolMgr::onAppFocusGained()
{
	if (mSelectedTool)
	{
		mSelectedTool->handleSelect();
	}
	updateToolStatus();
}

/////////////////////////////////////////////////////
// LLToolset

void LLToolset::addTool(LLTool* tool)
{
	mToolList.push_back(tool);
	if (!mSelectedTool)
	{
		mSelectedTool = tool;
	}
}

void LLToolset::selectTool(LLTool* tool)
{
	mSelectedTool = tool;
	gToolMgr.setCurrentTool(mSelectedTool);
}

void LLToolset::selectToolByIndex(U32 index)
{
	if (index < (U32)mToolList.size())
	{
		LLTool* tool = mToolList[index];
		if (tool)
		{
			mSelectedTool = tool;
			gToolMgr.setCurrentTool(tool);
		}
	}
}

void LLToolset::selectNextTool()
{
	LLTool* next = NULL;
	for (tool_list_t::iterator iter = mToolList.begin();
		 iter != mToolList.end();)
	{
		LLTool* cur = *iter++;
		if (cur == mSelectedTool && iter != mToolList.end())
		{
			next = *iter;
			break;
		}
	}

	if (next)
	{
		mSelectedTool = next;
		gToolMgr.setCurrentTool(mSelectedTool);
	}
	else
	{
		selectFirstTool();
	}
}

void LLToolset::selectPrevTool()
{
	LLTool* prev = NULL;
	for (tool_list_t::reverse_iterator iter = mToolList.rbegin();
		 iter != mToolList.rend(); )
	{
		LLTool* cur = *iter++;
		if (cur == mSelectedTool && iter != mToolList.rend())
		{
			prev = *iter;
			break;
		}
	}

	if (prev)
	{
		mSelectedTool = prev;
		gToolMgr.setCurrentTool(mSelectedTool);
	}
	else
	{
		S32 count = mToolList.size();
		if (count)
		{
			selectToolByIndex(count - 1);
		}
	}
}
