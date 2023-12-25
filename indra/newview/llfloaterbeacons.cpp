/**
 * @file llfloaterbeacons.cpp
 * @brief Front-end to LLPipeline controls for highlighting various kinds of objects.
 * @author Coco
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

#include "llfloaterbeacons.h"

#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"

#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"

LLFloaterBeacons::LLFloaterBeacons(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_beacons.xml");
}

bool LLFloaterBeacons::postBuild()
{
	childSetCommitCallback("touch_only", onClickUICheck, this);
	childSetCommitCallback("scripted", onClickUICheck, this);
	childSetCommitCallback("physical", onClickUICheck, this);
	childSetCommitCallback("permanent", onClickUICheck, this);
	childSetCommitCallback("character", onClickUICheck, this);
	childSetCommitCallback("sounds", onClickUICheck, this);
	childSetCommitCallback("particles", onClickUICheck, this);
	childSetCommitCallback("moap", onClickUICheck, this);
	childSetCommitCallback("highlights", onClickUICheck, this);
	childSetCommitCallback("beacons", onClickUICheck, this);
	childSetCommitCallback("invisiblesounds", onClickUICheck, this);
	childSetCommitCallback("attachments", onClickUICheck, this);
	childSetCommitCallback("owner", onClickUICheck, this);

	return true;
}

// Needed to make the floater visibility toggle the beacons.
//virtual
void LLFloaterBeacons::open()
{
//MK
	if (gRLenabled && 
		(gRLInterface.mContainsEdit || gRLInterface.mVisionRestricted))
	{
		return;
	}
//mk
	LLFloater::open();
	LLPipeline::sRenderBeaconsFloaterOpen = true;

	// Sort out any possible conflict
	if (!LLPipeline::sRenderBeacons && !LLPipeline::sRenderHighlight)
	{
		gSavedSettings.setBool("renderhighlights", true);
	}
	if (LLPipeline::sRenderInvisibleSoundBeacons &&
		(!LLPipeline::sRenderBeacons || !LLPipeline::sRenderSoundBeacons))
	{
		gSavedSettings.setBool("invisiblesoundsbeacon", false);
	}
	if (LLPipeline::sRenderScriptedTouchBeacons &&
		LLPipeline::sRenderScriptedBeacons)
	{
		gSavedSettings.setBool("scripttouchbeacon", false);
	}
}

//MK
//virtual
void LLFloaterBeacons::draw()
{
	// Fast enough that it can be kept here
	if (gRLenabled && 
		(gRLInterface.mContainsEdit || gRLInterface.mVisionRestricted))
	{
		gSavedSettings.setBool("BeaconAlwaysOn", false);
		close();
		return;
	}
	LLFloater::draw();
}
//mk

//virtual
void LLFloaterBeacons::close(bool app_quitting)
{
	LLFloater::close(app_quitting);
	if (!app_quitting)
	{
		LLPipeline::sRenderBeaconsFloaterOpen = false;
	}
}

// Callback attached to each check box control to both affect their main purpose
// and to implement the couple screwy interdependency rules that some have.
//static
void LLFloaterBeacons::onClickUICheck(LLUICtrl* ctrl, void* data)
{
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!check) return;

	std::string name = check->getName();
	if (name == "touch_only")
	{
		// Don't allow both to be ON at the same time. Toggle the other one off
		// if both now on.
		if (LLPipeline::sRenderScriptedTouchBeacons &&
			LLPipeline::sRenderScriptedBeacons)
		{
			gSavedSettings.setBool("scriptsbeacon", false);
		}
	}
	else if (name == "scripted")
	{
		if (LLPipeline::sRenderScriptedTouchBeacons &&
			LLPipeline::sRenderScriptedBeacons)
		{
			gSavedSettings.setBool("scripttouchbeacon", false);
		}
	}
	else if (name == "sounds")
	{
		if (!LLPipeline::sRenderSoundBeacons &&
			LLPipeline::sRenderInvisibleSoundBeacons)
		{
			gSavedSettings.setBool("invisiblesoundsbeacon", false);
		}
	}
	else if (name == "invisiblesounds")
	{
		if (LLPipeline::sRenderInvisibleSoundBeacons)
		{
			if (!LLPipeline::sRenderSoundBeacons)
			{
				gSavedSettings.setBool("soundsbeacon", true);
			}
			if (!LLPipeline::sRenderBeacons)
			{
				gSavedSettings.setBool("renderbeacons", true);
			}
		}
	}
	else if (name == "highlights")
	{
		// Don't allow both to be OFF at the same time. Toggle the other one on
		// if both now off.
		if (!LLPipeline::sRenderBeacons && !LLPipeline::sRenderHighlight)
		{
			gSavedSettings.setBool("renderbeacons", true);
		}
	}
	else if (name == "beacons")
	{
		if (!LLPipeline::sRenderBeacons)
		{
			// Don't allow both to be OFF at the same time. Toggle the other
			// one on if both now off.
			if (!LLPipeline::sRenderHighlight)
			{
				gSavedSettings.setBool("renderhighlights", true);
			}
			if (LLPipeline::sRenderInvisibleSoundBeacons)
			{
				gSavedSettings.setBool("invisiblesoundsbeacon", false);
			}
		}
	}
}
