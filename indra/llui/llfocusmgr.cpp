/**
 * @file llfocusmgr.cpp
 * @brief LLFocusMgr base class
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

#include "lluictrl.h"		// also includes llfocusmgr.h
#include "llcolor4.h"

constexpr F32 FOCUS_FADE_TIME = 0.3f;

LLFocusMgr gFocusMgr;

///////////////////////////////////////////////////////////////////////////////
// LLFocusableElement class
///////////////////////////////////////////////////////////////////////////////

LLFocusableElement::LLFocusableElement()
:	mFocusLostCallback(NULL),
	mFocusReceivedCallback(NULL),
	mFocusChangedCallback(NULL),
	mFocusCallbackUserData(NULL)
{
}

void LLFocusableElement::setFocusLostCallback(void (*cb)(LLFocusableElement*,
														 void*),
											  void* user_data)
{
	mFocusLostCallback = cb;
	mFocusCallbackUserData = user_data;
}

void LLFocusableElement::setFocusReceivedCallback(void (*cb)(LLFocusableElement*,
															 void*),
												  void* user_data)
{
	mFocusReceivedCallback = cb;
	mFocusCallbackUserData = user_data;
}

void LLFocusableElement::setFocusChangedCallback(void (*cb)(LLFocusableElement*,
															void*),
												 void* user_data)
{
	mFocusChangedCallback = cb;
	mFocusCallbackUserData = user_data;
}

void LLFocusableElement::onFocusReceived()
{
	if (mFocusReceivedCallback)
	{
		mFocusReceivedCallback(this, mFocusCallbackUserData);
	}
	if (mFocusChangedCallback)
	{
		mFocusChangedCallback(this, mFocusCallbackUserData);
	}
}

void LLFocusableElement::onFocusLost()
{
	if (mFocusLostCallback)
	{
		mFocusLostCallback(this, mFocusCallbackUserData);
	}

	if (mFocusChangedCallback)
	{
		mFocusChangedCallback(this, mFocusCallbackUserData);
	}
}

bool LLFocusableElement::hasFocus() const
{
	return gFocusMgr.getKeyboardFocus() == this;
}

///////////////////////////////////////////////////////////////////////////////
// LLFocusMgr class
///////////////////////////////////////////////////////////////////////////////

LLFocusMgr::LLFocusMgr()
:	mLockedView(NULL),
	mMouseCaptor(NULL),
	mKeyboardFocus(NULL),
	mLastKeyboardFocus(NULL),
	mDefaultKeyboardFocus(NULL),
	mKeystrokesOnly(false),
	mTopCtrl(NULL),
	mFocusWeight(0.f),
	mFocusColor(LLColor4::white),
#if LL_DEBUG
	mMouseCaptorName("none"),
	mKeyboardFocusName("none"),
	mTopCtrlName("none"),
#endif
	// Macs don't seem to notify us that we've gotten focus, so default to true
	mAppHasFocus(true)
{
}

LLFocusMgr::~LLFocusMgr()
{
	mFocusHistory.clear();
}

void LLFocusMgr::releaseFocusIfNeeded(const LLView* view)
{
	if (childHasMouseCapture(view))
	{
		setMouseCapture(NULL);
	}

	if (childHasKeyboardFocus(view))
	{
		if (view == mLockedView)
		{
			mLockedView = NULL;
			setKeyboardFocus(NULL);
		}
		else
		{
			setKeyboardFocus(mLockedView, false, mKeystrokesOnly);
		}
	}

	if (childIsTopCtrl(view))
	{
		setTopCtrl(NULL);
	}
}

LLUICtrl* LLFocusMgr::getKeyboardFocusUICtrl()
{
	if (mKeyboardFocus && mKeyboardFocus->isUICtrl())
	{
		return (LLUICtrl*)mKeyboardFocus;
	}
	return NULL;
}

LLUICtrl* LLFocusMgr::getLastKeyboardFocusUICtrl()
{
	if (mLastKeyboardFocus && mLastKeyboardFocus->isUICtrl())
	{
		return (LLUICtrl*)mLastKeyboardFocus;
	}
	return NULL;
}

void LLFocusMgr::setKeyboardFocus(LLFocusableElement* new_focus,
								  bool lock, bool keystrokes_only)
{
	// When locked, do not allow focus to go to anything that is not the locked
	// focus or one of its descendants
	if (mLockedView)
	{
		if (new_focus == NULL)
		{
			return;
		}
		if (new_focus != mLockedView)
		{
			LLView* new_focus_view = dynamic_cast<LLView*>(new_focus);
			if (!new_focus_view || !new_focus_view->hasAncestor(mLockedView))
			{
				return;
			}
		}
	}

	mKeystrokesOnly = keystrokes_only;
	if (LLView::sDebugKeys)
	{
		llinfos << "mKeystrokesOnly = " << mKeystrokesOnly << llendl;
	}

	if (new_focus != mKeyboardFocus)
	{
		mLastKeyboardFocus = mKeyboardFocus;
		mKeyboardFocus = new_focus;

		if (mLastKeyboardFocus)
		{
			mLastKeyboardFocus->onFocusLost();
		}

		// clear out any existing flash
		if (new_focus)
		{
			mFocusWeight = 0.f;
			new_focus->onFocusReceived();
		}
		mFocusTimer.reset();

#if LL_DEBUG
		LLUICtrl* focus_ctrl = NULL;
		if (new_focus && new_focus->isUICtrl())
		{
			focus_ctrl = (LLUICtrl*)new_focus;
		}
		mKeyboardFocusName = focus_ctrl ? focus_ctrl->getName()
										: std::string("none");
#endif

		// If we have got a default keyboard focus, and the caller is
		// releasing keyboard focus, move to the default.
		if (mDefaultKeyboardFocus != NULL && mKeyboardFocus == NULL)
		{
			mDefaultKeyboardFocus->setFocus(true);
		}

		LLView* focused_view = dynamic_cast<LLView*>(mKeyboardFocus);
		LLView* focus_subtree = focused_view;
		LLView* viewp = focus_subtree;
		// Find root-most focus root
		while (viewp)
		{
			if (viewp->isFocusRoot())
			{
				focus_subtree = viewp;
			}
			viewp = viewp->getParent();
		}
		if (focus_subtree)
		{
			mFocusHistory[focus_subtree->getHandle()] =
				focused_view ? focused_view->getHandle() : LLHandle<LLView>();
		}
	}

	if (lock)
	{
		lockFocus();
	}
}

// Returns true is parent or any descedent of parent has keyboard focus.
bool LLFocusMgr::childHasKeyboardFocus(const LLView* parent) const
{
	LLView* focus_view = dynamic_cast<LLView*>(mKeyboardFocus);
	while (focus_view)
	{
		if (focus_view == parent)
		{
			return true;
		}
		focus_view = focus_view->getParent();
	}
	return false;
}

// Returns true is parent or any descedent of parent is the mouse captor.
bool LLFocusMgr::childHasMouseCapture(const LLView* parent) const
{
	if (mMouseCaptor && mMouseCaptor->isView())
	{
		LLView* captor_view = (LLView*)mMouseCaptor;
		while (captor_view)
		{
			if (captor_view == parent)
			{
				return true;
			}
			captor_view = captor_view->getParent();
		}
	}
	return false;
}

void LLFocusMgr::removeKeyboardFocusWithoutCallback(const LLFocusableElement* focus)
{
	// should be ok to unlock here, as you have to know the locked view
	// in order to unlock it
	if (focus == mLockedView)
	{
		mLockedView = NULL;
	}

	if (mKeyboardFocus == focus)
	{
		mKeyboardFocus = NULL;
#if LL_DEBUG
		mKeyboardFocusName = "none";
#endif
	}
}

void LLFocusMgr::setMouseCapture(LLMouseHandler* new_captor)
{
#if 0
	if (mFocusLocked)
	{
		return;
	}
#endif

	if (new_captor != mMouseCaptor)
	{
		LLMouseHandler* old_captor = mMouseCaptor;
		mMouseCaptor = new_captor;
		if (old_captor)
		{
			old_captor->onMouseCaptureLost();
		}

#if LL_DEBUG
		mMouseCaptorName = new_captor ? new_captor->getName()
									  : std::string("none");
#endif
	}
}

void LLFocusMgr::removeMouseCaptureWithoutCallback(const LLMouseHandler* captor)
{
#if 0
	if (mFocusLocked)
	{
		return;
	}
#endif
	if (mMouseCaptor == captor)
	{
		mMouseCaptor = NULL;
#if LL_DEBUG
		mMouseCaptorName = "none";
#endif
	}
}

bool LLFocusMgr::childIsTopCtrl(const LLView* parent) const
{
	LLView* top_view = (LLView*)mTopCtrl;
	while (top_view)
	{
		if (top_view == parent)
		{
			return true;
		}
		top_view = top_view->getParent();
	}
	return false;
}

// set new_top = NULL to release top_view.
void LLFocusMgr::setTopCtrl(LLUICtrl* new_top)
{
	LLUICtrl* old_top = mTopCtrl;
	if (new_top != old_top)
	{
		mTopCtrl = new_top;
#if LL_DEBUG
		mTopCtrlName = new_top ? new_top->getName() : std::string("none");
#endif
		if (old_top)
		{
			old_top->onLostTop();
		}
	}
}

void LLFocusMgr::removeTopCtrlWithoutCallback(const LLUICtrl* top_view)
{
	if (mTopCtrl == top_view)
	{
		mTopCtrl = NULL;
#if LL_DEBUG
		mTopCtrlName = "none";
#endif
	}
}

void LLFocusMgr::lockFocus()
{
	if (mKeyboardFocus && mKeyboardFocus->isUICtrl())
	{
		mLockedView = (LLUICtrl*)mKeyboardFocus;
	}
	else
	{
		mLockedView = NULL;
	}
}

void LLFocusMgr::unlockFocus()
{
	mLockedView = NULL;
}

F32 LLFocusMgr::getFocusFlashAmt() const
{
	return clamp_rescale(getFocusTime(), 0.f, FOCUS_FADE_TIME,
						 mFocusWeight, 0.f);
}

LLColor4 LLFocusMgr::getFocusColor() const
{
	LLColor4 focus_color = lerp(mFocusColor, LLColor4::white,
								getFocusFlashAmt());
	// de-emphasize keyboard focus when app has lost focus (to avoid typing
	// into wrong window problem)
	if (!mAppHasFocus)
	{
		focus_color.mV[VALPHA] *= 0.4f;
	}
	return focus_color;
}

void LLFocusMgr::triggerFocusFlash()
{
	mFocusTimer.reset();
	mFocusWeight = 1.f;
}

void LLFocusMgr::setAppHasFocus(bool focus)
{
	if (!mAppHasFocus && focus)
	{
		triggerFocusFlash();
	}

	// release focus from "top ctrl"s, which generally hides them
	if (!focus && mTopCtrl)
	{
		setTopCtrl(NULL);
	}
	mAppHasFocus = focus;
}

LLUICtrl* LLFocusMgr::getLastFocusForGroup(LLView* subtree_root) const
{
	if (subtree_root)
	{
		focus_history_map_t::const_iterator found_it;
		found_it = mFocusHistory.find(subtree_root->getHandle());
		if (found_it != mFocusHistory.end())
		{
			// found last focus for this subtree
			return static_cast<LLUICtrl*>(found_it->second.get());
		}
	}
	return NULL;
}

void LLFocusMgr::clearLastFocusForGroup(LLView* subtree_root)
{
	if (subtree_root)
	{
		mFocusHistory.erase(subtree_root->getHandle());
	}
}
