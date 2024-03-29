/**
 * @file llfloatermove.cpp
 * @brief Container for movement buttons like forward, left, fly
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

#include "llfloatermove.h"

#include "llbutton.h"
#include "llspinctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "lljoystickbutton.h"
#include "llviewercontrol.h"
#include "llvoavatarself.h"

// Constants
constexpr F32 MOVE_BUTTON_DELAY = 0.f;
constexpr F32 YAW_NUDGE_RATE = 0.05f;			// fraction of normal speed
constexpr F32 NUDGE_TIME = 0.25f;				// in seconds

LLFloaterMove::LLFloaterMove(const LLSD& key)
:	LLFloater("movement controls")
{
	setIsChrome(true);

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_moveview.xml",
												 NULL, false);

	mForwardButton = getChild<LLJoystickAgentTurn>("forward btn");
	mForwardButton->setHeldDownDelay(MOVE_BUTTON_DELAY);

	mBackwardButton = getChild<LLJoystickAgentTurn>("backward btn");
	mBackwardButton->setHeldDownDelay(MOVE_BUTTON_DELAY);

	mSlideLeftButton = getChild<LLJoystickAgentSlide>("slide left btn");
	mSlideLeftButton->setHeldDownDelay(MOVE_BUTTON_DELAY);

	mSlideRightButton = getChild<LLJoystickAgentSlide>("slide right btn");
	mSlideRightButton->setHeldDownDelay(MOVE_BUTTON_DELAY);

	mTurnLeftButton = getChild<LLButton>("turn left btn");
	mTurnLeftButton->setHeldDownDelay(MOVE_BUTTON_DELAY);
	mTurnLeftButton->setHeldDownCallback(turnLeft);

	mTurnRightButton = getChild<LLButton>("turn right btn");
	mTurnRightButton->setHeldDownDelay(MOVE_BUTTON_DELAY);
	mTurnRightButton->setHeldDownCallback(turnRight);

	mMoveUpButton = getChild<LLButton>("move up btn");
	childSetAction("move up btn",moveUp,NULL);
	mMoveUpButton->setHeldDownDelay(MOVE_BUTTON_DELAY);
	mMoveUpButton->setHeldDownCallback(moveUp);

	mMoveDownButton = getChild<LLButton>("move down btn");
	childSetAction("move down btn",moveDown,NULL);
	mMoveDownButton->setHeldDownDelay(MOVE_BUTTON_DELAY);
	mMoveDownButton->setHeldDownCallback(moveDown);

	mFlyButton = getChild<LLButton>("fly btn");
	childSetAction("fly btn", onFlyButtonClicked, NULL);

	mZOffsetSpinner = getChild<LLSpinCtrl>("z_offset");
	mZOffsetSpinner->setToolTip(getString("z_offset_tooltip"));
}

//virtual
void LLFloaterMove::draw()
{
	bool sitting = isAgentAvatarValid() && gAgentAvatarp->mIsSitting;
	mFlyButton->setEnabled(!sitting && (gAgent.canFly() || gAgent.getFlying()));

	mZOffsetSpinner->setEnabled(!LLVOAvatarSelf::canUseServerBaking() ||
								LLVOAvatarSelf::useAvatarHoverHeight());

	LLFloater::draw();
}

//virtual
void LLFloaterMove::onClose(bool app_quitting)
{
	LLFloater::onClose(app_quitting);

	if (!app_quitting)
	{
		gSavedSettings.setBool("ShowMovementControls", false);
	}
}

void LLFloaterMove::onOpen()
{
	LLFloater::onOpen();
	gSavedSettings.setBool("ShowMovementControls", true);
}

//static
void LLFloaterMove::onFlyButtonClicked(void*)
{
	gAgent.toggleFlying();
}

//static
F32 LLFloaterMove::getYawRate(F32 time)
{
	if (time < NUDGE_TIME)
	{
		F32 rate = YAW_NUDGE_RATE + time * (1.f - YAW_NUDGE_RATE) / NUDGE_TIME;
		return rate;
	}
	else
	{
		return 1.f;
	}
}

//static
void LLFloaterMove::turnLeft(void*)
{
	F32 time = getInstance()->mTurnLeftButton->getHeldDownTime();
	gAgent.moveYaw(getYawRate(time));
}

//static
void LLFloaterMove::turnRight(void*)
{
	F32 time = getInstance()->mTurnRightButton->getHeldDownTime();
	gAgent.moveYaw(-getYawRate(time));
}

//static
void LLFloaterMove::moveUp(void*)
{
	// Jumps or flys up, depending on fly state
	gAgent.moveUp(1);
}

//static
void LLFloaterMove::moveDown(void*)
{
	// Crouches or flys down, depending on fly state
	gAgent.moveUp(-1);
}
