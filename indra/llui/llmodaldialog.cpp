/**
 * @file llmodaldialog.cpp
 * @brief LLModalDialog base class
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llmodaldialog.h"

#include "llwindow.h"
#include "llvector2.h"

// static
std::list<LLModalDialog*> LLModalDialog::sModalStack;

LLModalDialog::LLModalDialog(const std::string& title, S32 width, S32 height,
							 bool modal)
:	LLFloater("modal dialog", LLRect(0, height, width, 0), title,
			  RESIZE_NO, DEFAULT_MIN_WIDTH, DEFAULT_MIN_HEIGHT, DRAG_ON_TOP,
			  // minimizable and closeable only if not modal. Bordered.
			  !modal, !modal, true),
	mModal(modal)
{
	setVisible(false);
	setBackgroundVisible(true);
	setBackgroundOpaque(true);
	centerOnScreen(); // default position
}

LLModalDialog::~LLModalDialog()
{
	// don't unlock focus unless we have it
	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		gFocusMgr.unlockFocus();
	}
}

// virtual
void LLModalDialog::open()
{
	LLHostFloater host;	// Make sure we do not ever host a modal dialog
	LLFloater::open();
}

void LLModalDialog::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLFloater::reshape(width, height, called_from_parent);
	centerOnScreen();
}

void LLModalDialog::startModal()
{
	if (mModal)
	{
		// If Modal, Hide the active modal dialog
		if (!sModalStack.empty())
		{
			LLModalDialog* front = sModalStack.front();
			front->setVisible(false);
		}

		// This is a modal dialog. It sucks up all mouse and keyboard
		// operations.
		gFocusMgr.setMouseCapture(this);
		gFocusMgr.setTopCtrl(this);
		setFocus(true);

		sModalStack.push_front(this);
	}

	setVisible(true);
}

void LLModalDialog::stopModal()
{
	gFocusMgr.unlockFocus();
	gFocusMgr.releaseFocusIfNeeded(this);

	if (mModal)
	{
		std::list<LLModalDialog*>::iterator iter = std::find(sModalStack.begin(),
															 sModalStack.end(),
															 this);
		if (iter != sModalStack.end())
		{
			sModalStack.erase(iter);
		}
		else
		{
			llwarns << "Dialog not in list !" << llendl;
		}
	}
	if (!sModalStack.empty())
	{
		LLModalDialog* front = sModalStack.front();
		front->setVisible(true);
	}
}

void LLModalDialog::setVisible(bool visible)
{
	if (mModal)
	{
		if (visible)
		{
			// This is a modal dialog. It sucks up all mouse and keyboard
			// operations.
			gFocusMgr.setMouseCapture(this);

			// The dialog view is a root view
			gFocusMgr.setTopCtrl(this);
			setFocus(true);
		}
		else
		{
			gFocusMgr.releaseFocusIfNeeded(this);
		}
	}

	LLFloater::setVisible(visible);
}

bool LLModalDialog::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mModal)
	{
		if (!LLFloater::handleMouseDown(x, y, mask))
		{
			// Click was outside the panel
			make_ui_sound("UISndInvalidOp");
		}
	}
	else
	{
		LLFloater::handleMouseDown(x, y, mask);
	}

	return true;
}

bool LLModalDialog::handleHover(S32 x, S32 y, MASK mask)
{
	if (childrenHandleHover(x, y, mask) == NULL)
	{
		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName() << LL_ENDL;
	}

	return true;
}

bool LLModalDialog::handleMouseUp(S32 x, S32 y, MASK mask)
{
	childrenHandleMouseUp(x, y, mask);
	return true;
}

bool LLModalDialog::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	childrenHandleScrollWheel(x, y, clicks);
	return true;
}

bool LLModalDialog::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!LLFloater::handleDoubleClick(x, y, mask))
	{
		// Click outside the panel
		make_ui_sound("UISndInvalidOp");
	}
	return true;
}

bool LLModalDialog::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	childrenHandleRightMouseDown(x, y, mask);
	return true;
}

bool LLModalDialog::handleKeyHere(KEY key, MASK mask)
{
	LLFloater::handleKeyHere(key, mask);

	if (mModal)
	{
		// Suck up all keystokes except CTRL-Q.
		return key != 'Q' || mask != MASK_CONTROL;
	}
	else
	{
		// Don't process escape key until message box has been on screen a
		// minimal amount of time to avoid accidentally destroying the message
		// box when user is hitting escape at the time it appears
		if (key == KEY_ESCAPE && mask == MASK_NONE &&
			mVisibleTime.getElapsedTimeF32() > 1.0f)
		{
			close();
			return true;
		}
	}

	return false;
}

void LLModalDialog::onClose(bool app_quitting)
{
	stopModal();
	LLFloater::onClose(app_quitting);
}

// virtual
void LLModalDialog::draw()
{
	gl_drop_shadow(0, getRect().getHeight(), getRect().getWidth(), 0,
				   LLUI::sColorDropShadow, LLUI::sDropShadowFloater);

	LLFloater::draw();

	if (mModal)
	{
		// If we've lost focus to a non-child, get it back ASAP.
		if (gFocusMgr.getTopCtrl() != this)
		{
			gFocusMgr.setTopCtrl(this);
		}

		if (!gFocusMgr.childHasKeyboardFocus(this))
		{
			setFocus(true);
		}

		if (!gFocusMgr.childHasMouseCapture(this))
		{
			gFocusMgr.setMouseCapture(this);
		}
	}
}

void LLModalDialog::centerOnScreen()
{
	LLVector2 window_size = LLUI::getWindowSize();
	centerWithin(LLRect(0, 0, ll_roundp(window_size.mV[VX]),
						ll_roundp(window_size.mV[VY])));
}

// static
void LLModalDialog::onAppFocusLost()
{
	if (!sModalStack.empty())
	{
		LLModalDialog* instance = LLModalDialog::sModalStack.front();
		if (gFocusMgr.childHasMouseCapture(instance))
		{
			gFocusMgr.setMouseCapture(NULL);
		}

		if (gFocusMgr.childHasKeyboardFocus(instance))
		{
			gFocusMgr.setKeyboardFocus(NULL);
		}
	}
}

// static
void LLModalDialog::onAppFocusGained()
{
	if (!sModalStack.empty())
	{
		LLModalDialog* instance = LLModalDialog::sModalStack.front();

		// This is a modal dialog. It sucks up all mouse and keyboard
		// operations.
		gFocusMgr.setMouseCapture(instance);
		instance->setFocus(true);
		gFocusMgr.setTopCtrl(instance);

		instance->centerOnScreen();
	}
}

void LLModalDialog::shutdownModals()
{
	// This method is only for use during app shutdown. ~LLModalDialog()
	// checks sModalStack, and if the dialog instance is still there, it
	// crumps with "Attempt to delete dialog while still in sModalStack!" But
	// at app shutdown, all bets are off. If the user asks to shut down the
	// app, we shouldn't have to care WHAT's open. Put differently, if a modal
	// dialog is so crucial that we can't let the user terminate until s/he
	// addresses it, we should reject a termination request. The current state
	// of affairs is that we accept it, but then produce an llerrs popup that
	// simply makes our software look unreliable.
	sModalStack.clear();
}
