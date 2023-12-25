/**
 * @file llfloaterminimap.cpp
 * @brief The "mini-map" or radar in the upper right part of the screen.
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

#include "llfloaterminimap.h"

#include "llapp.h"
#include "lldraghandle.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llpanelminimap.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"

//static
void* LLFloaterMiniMap::createPanelMiniMap(void* data)
{
	LLFloaterMiniMap* self = (LLFloaterMiniMap*)data;
	self->mPanelMiniMap = new LLPanelMiniMap("Mapview");
	return self->mPanelMiniMap;
}

LLFloaterMiniMap::LLFloaterMiniMap(const LLSD& key)
:	LLFloater("mini map"),
	mPanelMiniMap(NULL)
{
	LLCallbackMap::map_t factory_map;
	factory_map["mini_mapview"] = LLCallbackMap(createPanelMiniMap, this);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_mini_map.xml",
												 &factory_map, false);
}

bool LLFloaterMiniMap::postBuild()
{
	// Send the drag handle to the back, but make sure close stays on top
	sendChildToBack(getDragHandle());
	sendChildToFront(getChild<LLButton>("llfloater_close_btn"));
	setIsChrome(true);
	return true;
}

//virtual
void LLFloaterMiniMap::onOpen()
{
	gFloaterViewp->adjustToFitScreen(this);
	gSavedSettings.setBool("ShowMiniMap", true);
}

//virtual
void LLFloaterMiniMap::onClose(bool app_quitting)
{
	LLFloater::setVisible(false);

	if (!app_quitting)
	{
		gSavedSettings.setBool("ShowMiniMap", false);
	}
}

bool LLFloaterMiniMap::canClose()
{
	return !LLApp::isExiting();
}

//virtual
void LLFloaterMiniMap::draw()
{
//MK
	// Fast enough that it can be kept here
	if (gRLenabled && gRLInterface.mContainsShowminimap)
	{
		close();
		return;
	}
//mk

	if (gAgent.cameraMouselook())
	{
		setMouseOpaque(false);
		getDragHandle()->setMouseOpaque(false);

		drawChild(mPanelMiniMap);
	}
	else
	{
		setMouseOpaque(true);
		getDragHandle()->setMouseOpaque(true);

		LLFloater::draw();
	}
}
