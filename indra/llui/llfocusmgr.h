/**
 * @file llfocusmgr.h
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

// Singleton that manages keyboard and mouse focus

#ifndef LL_LLFOCUSMGR_H
#define LL_LLFOCUSMGR_H

#include "llstring.h"
#include "llframetimer.h"
#include "llhandle.h"
#include "llcolor4.h"

class LLFrameTimer;
class LLMouseHandler;
class LLUICtrl;
class LLView;

class LLFocusableElement
{
	friend class LLFocusMgr; // Allow access to focus change handlers

public:
	LLFocusableElement();
	virtual ~LLFocusableElement() = default;

	// MUST be defined and return false when the class derived from
	// LLFocusableElement is not also derived from LLUICtrl, or true when it is.
	virtual bool isUICtrl() = 0;

	LL_INLINE virtual void setFocus(bool b)				{}

	virtual bool hasFocus() const;

	void setFocusLostCallback(void (*cb)(LLFocusableElement*, void*),
							  void* user_data = NULL);
	void setFocusReceivedCallback(void (*cb)(LLFocusableElement*, void*),
								  void* user_data = NULL);
	void setFocusChangedCallback(void (*cb)(LLFocusableElement*, void*),
								 void* user_data = NULL);

	// These were brought up the hierarchy from LLView so that we do not have
	// to use dynamic casts when dealing with keyboard focus.

	LL_INLINE virtual bool handleKey(KEY key, MASK mask, bool from_parent)
	{
		return false;
	}

	LL_INLINE virtual bool handleKeyUp(KEY key, MASK mask, bool from_parent)
	{
		return false;
	}

	LL_INLINE virtual bool handleUnicodeChar(llwchar uchar, bool from_parent)
	{
		return false;
	}

	// If these methods return true, this LLFocusableElement wants to receive
	// KEYUP and KEYDOWN messages. Default implementation returns false.
	LL_INLINE virtual bool wantsKeyUpKeyDown() const	{ return false; }
	LL_INLINE virtual bool wantsReturnKey() const		{ return false; }

protected:
	virtual void onFocusReceived();
	virtual void onFocusLost();

protected:
	void (*mFocusLostCallback)(LLFocusableElement* caller, void* userdata);
	void (*mFocusReceivedCallback)(LLFocusableElement* ctrl, void* userdata);
	void (*mFocusChangedCallback)(LLFocusableElement* ctrl, void* userdata);
	void* mFocusCallbackUserData;
};

class LLFocusMgr
{
public:
	LLFocusMgr();
	~LLFocusMgr();

	LL_INLINE void setFocusColor(LLColor4 color)		{ mFocusColor = color; }

	// Mouse Captor

	// new_captor = NULL to release the mouse:
	void setMouseCapture(LLMouseHandler* new_captor);

	LL_INLINE LLMouseHandler* getMouseCapture() const	{ return mMouseCaptor; }

	void removeMouseCaptureWithoutCallback(const LLMouseHandler* captor);
	bool childHasMouseCapture(const LLView* parent) const;

	// Keyboard Focus

	// new_focus = NULL to release the focus:
	void setKeyboardFocus(LLFocusableElement* new_focus, bool lock = false,
						  bool keystrokes_only = false);

	LL_INLINE LLFocusableElement* getKeyboardFocus() const
	{
		return mKeyboardFocus;
	}

	LL_INLINE LLFocusableElement* getLastKeyboardFocus() const
	{
		return mLastKeyboardFocus;
	}

	LLUICtrl* getKeyboardFocusUICtrl();
	LLUICtrl* getLastKeyboardFocusUICtrl();

	bool childHasKeyboardFocus(const LLView* parent) const;
	void removeKeyboardFocusWithoutCallback(const LLFocusableElement* focus);
	LL_INLINE bool getKeystrokesOnly()					{ return mKeystrokesOnly; }
	LL_INLINE void setKeystrokesOnly(bool b)			{ mKeystrokesOnly = b; }

	LL_INLINE F32 getFocusTime() const					{ return mFocusTimer.getElapsedTimeF32(); }
	F32 getFocusFlashAmt() const;
	LL_INLINE S32 getFocusFlashWidth() const			{ return ll_roundp(lerp(1.f, 3.f, getFocusFlashAmt())); }
	LLColor4 getFocusColor() const;
	void triggerFocusFlash();
	LL_INLINE bool getAppHasFocus() const				{ return mAppHasFocus; }
	void setAppHasFocus(bool focus);
	LLUICtrl* getLastFocusForGroup(LLView* subtree_root) const;
	void clearLastFocusForGroup(LLView* subtree_root);

	// If setKeyboardFocus(NULL) is called, and there is a non-NULL default
	// keyboard focus view, focus goes there. JC
	LL_INLINE void setDefaultKeyboardFocus(LLFocusableElement* default_focus)
	{
		mDefaultKeyboardFocus = default_focus;
	}

	LL_INLINE LLFocusableElement* getDefaultKeyboardFocus() const
	{
		return mDefaultKeyboardFocus;
	}

	// Top View

	void setTopCtrl(LLUICtrl* new_top);
	LL_INLINE LLUICtrl* getTopCtrl() const				{ return mTopCtrl; }
	void removeTopCtrlWithoutCallback(const LLUICtrl* top_view);
	bool childIsTopCtrl(const LLView* parent) const;

	// All Three

	void releaseFocusIfNeeded(const LLView* top_view);
	void lockFocus();
	void unlockFocus();
	LL_INLINE bool focusLocked() const					{ return mLockedView != NULL; }

private:
	LLColor4			mFocusColor;

	LLUICtrl*			mLockedView;

	// Top View
	LLUICtrl*			mTopCtrl;

	LLFrameTimer		mFocusTimer;
	F32					mFocusWeight;

	// Mouse events are premptively routed to this object
	LLMouseHandler*		mMouseCaptor;

	// Keyboard events are preemptively routed to this object:
	LLFocusableElement*	mKeyboardFocus;
	LLFocusableElement*	mLastKeyboardFocus;		// who last had focus
	LLFocusableElement*	mDefaultKeyboardFocus;

	typedef std::map<LLHandle<LLView>, LLHandle<LLView> > focus_history_map_t;
	focus_history_map_t mFocusHistory;

	bool				mKeystrokesOnly;
	bool				mAppHasFocus;

#if LL_DEBUG
	std::string			mMouseCaptorName;
	std::string			mKeyboardFocusName;
	std::string			mTopCtrlName;
#endif
};

extern LLFocusMgr gFocusMgr;

#endif  // LL_LLFOCUSMGR_H
