/**
 * @file llfloater.cpp
 * @brief LLFloater base class
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

// Floating "windows" within the GL display, like the inventory floater,
// mini-map floater, etc.

#include "linden_common.h"

#include "llfloater.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcontrol.h"
#include "lldraghandle.h"
#include "llresizebar.h"
#include "llresizehandle.h"
#include "llkeyboard.h"
#include "lltextbox.h"
#include "llwindow.h"
#include "lltabcontainer.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

S32 gMenuBarHeight = 18;

static const std::string LL_FLOATER_TAG = "floater";
static const std::string LL_MULTI_FLOATER_TAG = "multi_floater";

constexpr S32 MINIMIZED_WIDTH = 160;
constexpr S32 CLOSE_BOX_FROM_TOP = 1;
// Use this to control "jumping" behavior when Ctrl-Tabbing
constexpr S32 TABBED_FLOATER_OFFSET = 0;

U32 LLFloater::sLastFloaterId = 0;

bool LLFloaterView::sStackMinimizedTopToBottom = false;
bool LLFloaterView::sStackMinimizedRightToLeft = false;
U32 LLFloaterView::sStackScreenWidthFraction = 1;

std::string	LLFloater::sButtonActiveImageNames[BUTTON_COUNT] =
{
	"UIImgBtnCloseActiveUUID",		// BUTTON_CLOSE
	"UIImgBtnRestoreActiveUUID",	// BUTTON_RESTORE
	"UIImgBtnMinimizeActiveUUID",	// BUTTON_MINIMIZE
	"UIImgBtnTearOffActiveUUID",	// BUTTON_TEAR_OFF
};

std::string	LLFloater::sButtonInactiveImageNames[BUTTON_COUNT] =
{
	"UIImgBtnCloseInactiveUUID",	// BUTTON_CLOSE
	"UIImgBtnRestoreInactiveUUID",	// BUTTON_RESTORE
	"UIImgBtnMinimizeInactiveUUID",	// BUTTON_MINIMIZE
	"UIImgBtnTearOffInactiveUUID",	// BUTTON_TEAR_OFF
};

std::string	LLFloater::sButtonPressedImageNames[BUTTON_COUNT] =
{
	"UIImgBtnClosePressedUUID",		// BUTTON_CLOSE
	"UIImgBtnRestorePressedUUID",	// BUTTON_RESTORE
	"UIImgBtnMinimizePressedUUID",	// BUTTON_MINIMIZE
	"UIImgBtnTearOffPressedUUID",	// BUTTON_TEAR_OFF
};

std::string	LLFloater::sButtonNames[BUTTON_COUNT] =
{
	"llfloater_close_btn",			// BUTTON_CLOSE
	"llfloater_restore_btn",		// BUTTON_RESTORE
	"llfloater_minimize_btn",		// BUTTON_MINIMIZE
	"llfloater_tear_off_btn",		// BUTTON_TEAR_OFF
};

std::string	LLFloater::sButtonToolTipNames[BUTTON_COUNT] =
{
#ifdef LL_DARWIN
	"button-mac-close",				// BUTTON_CLOSE
#else
	"button-close",					// BUTTON_CLOSE
#endif
	"button-restore",				// BUTTON_RESTORE
	"button-minimize",				// BUTTON_MINIMIZE
	"button-tear-off",				// BUTTON_TEAR_OFF
};

LLFloater::click_callback LLFloater::sButtonCallbacks[BUTTON_COUNT] =
{
	LLFloater::onClickClose,		// BUTTON_CLOSE
	LLFloater::onClickMinimize,		// BUTTON_RESTORE
	LLFloater::onClickMinimize,		// BUTTON_MINIMIZE
	LLFloater::onClickTearOff,		// BUTTON_TEAR_OFF
};

LLMultiFloater* LLFloater::sHostp = NULL;

static bool sResizing = false;
static S32 sLastSizeX = 0;
static S32 sLastSizeY = 0;

//static
bool LLFloater::resizing(S32& size_x, S32& size_y)
{
	bool resizing = sResizing;
	if (resizing)
	{
		size_x = sLastSizeX;
		size_y = sLastSizeY;
		sResizing = false;
	}
	return resizing;
}

// Instance created in LLViewerWindow::initBase() and destroyed in
// LLViewerWindow::shutdownViews()
LLFloaterView* gFloaterViewp = NULL;

LLFloater::LLFloater()
:	LLPanel(),
	mId(++sLastFloaterId),
	mTitleIsPristine(true),
	mAutoFocus(true),
	mResizable(false),
	mDragOnLeft(false),
	mMinWidth(0),
	mMinHeight(0),
	mDragHandle(NULL)
{
	for (S32 i = 0; i < BUTTON_COUNT; ++i)
	{
		mButtonsEnabled[i] = false;
		mButtons[i] = NULL;
	}
	for (S32 i = 0; i < 4; ++i)
	{
		mResizeBar[i] = NULL;
		mResizeHandle[i] = NULL;
	}

	mNotificationContext = new LLFloaterNotificationContext(getHandle());
}

LLFloater::LLFloater(const std::string& name)
:	LLPanel(name),
	mId(++sLastFloaterId),
	mTitleIsPristine(true),
	mAutoFocus(true) // Automatically take focus when opened
{
	for (S32 i = 0; i < BUTTON_COUNT; ++i)
	{
		mButtonsEnabled[i] = false;
		mButtons[i] = NULL;
	}
	for (S32 i = 0; i < 4; ++i)
	{
		mResizeBar[i] = NULL;
		mResizeHandle[i] = NULL;
	}
	initFloater(LLStringUtil::null, false,
				DEFAULT_MIN_WIDTH, DEFAULT_MIN_HEIGHT,
				false, true, true); // defaults
}

LLFloater::LLFloater(const std::string& name, const LLRect& rect,
					 const std::string& title, bool resizable,
					 S32 min_width, S32 min_height, bool drag_on_left,
					 bool minimizable, bool close_btn, bool bordered)
:	LLPanel(name, rect, bordered),
	mId(++sLastFloaterId),
	mTitleIsPristine(true),
	mAutoFocus(true)	// Automatically take focus when opened
{
	for (S32 i = 0; i < BUTTON_COUNT; ++i)
	{
		mButtonsEnabled[i] = false;
		mButtons[i] = NULL;
	}
	for (S32 i = 0; i < 4; ++i)
	{
		mResizeBar[i] = NULL;
		mResizeHandle[i] = NULL;
	}
	initFloater(title, resizable, min_width, min_height, drag_on_left,
				minimizable, close_btn);
}

LLFloater::LLFloater(const std::string& name, const std::string& rect_control,
					 const std::string& title, bool resizable,
					 S32 min_width, S32 min_height, bool drag_on_left,
					 bool minimizable, bool close_btn, bool bordered)
:	LLPanel(name, rect_control, bordered),
	mId(++sLastFloaterId),
	mTitleIsPristine(true),
	mAutoFocus(true)	// Automatically take focus when opened
{
	for (S32 i = 0; i < BUTTON_COUNT; ++i)
	{
		mButtonsEnabled[i] = false;
		mButtons[i] = NULL;
	}
	for (S32 i = 0; i < 4; ++i)
	{
		mResizeBar[i] = NULL;
		mResizeHandle[i] = NULL;
	}
	initFloater(title, resizable, min_width, min_height, drag_on_left,
				minimizable, close_btn);
}

// NOTE: floaters constructed from XML call init() twice !
void LLFloater::initFloater(const std::string& title, bool resizable,
							S32 min_width, S32 min_height,
							bool drag_on_left, bool minimizable,
							bool close_btn)
{
	mNotificationContext = new LLFloaterNotificationContext(getHandle());

	// Init function can be called more than once, so clear out old data.
	for (S32 i = 0; i < BUTTON_COUNT; ++i)
	{
		mButtonsEnabled[i] = false;
		if (mButtons[i])
		{
			removeChild(mButtons[i]);
			delete mButtons[i];
			mButtons[i] = NULL;
		}
	}
	mButtonScale = 1.f;

	// SJB: this is a bit of a hack:
	bool need_border = hasBorder();
	// Remove the border since deleteAllChildren() will also delete the border
	// (but not clear mBorder)
	removeBorder();
	// this will delete mBorder too
	deleteAllChildren();
	// add the border back if we want it
	if (need_border)
	{
	    addBorder();
	}

	// Chrome floaters do not take focus at all
	setFocusRoot(!getIsChrome());

	// Reset cached pointers
	mDragHandle = NULL;
	for (S32 i = 0; i < 4; ++i)
	{
		mResizeBar[i] = NULL;
		mResizeHandle[i] = NULL;
	}
	mCanTearOff = true;

	// Clicks stop here.
	setMouseOpaque(true);

	mForeground = false;
	mDragOnLeft = drag_on_left == true;

	// Floaters always draw their background, unlike every other panel.
	setBackgroundVisible(true);

	// Floaters start not minimized. When minimized, they save their former
	// rectangle to be used on restore.
	mMinimized = false;
	mExpandedRect.set(0, 0, 0, 0);

	S32 close_box_size;		// For layout purposes, how big is the close box ?
	if (close_btn)
	{
		close_box_size = LLFLOATER_CLOSE_BOX_SIZE;
	}
	else
	{
		close_box_size = 0;
	}

	// Drag handle; we add it first so that it is in the background.
	if (drag_on_left)
	{
		LLRect drag_handle_rect;
		drag_handle_rect.setOriginAndSize(0, 0, DRAG_HANDLE_WIDTH,
										  getRect().getHeight() -
										  LLPANEL_BORDER_WIDTH -
										  close_box_size);
		mDragHandle = new LLDragHandleLeft("drag", drag_handle_rect, title);
	}
	else // Drag on top
	{
		LLRect drag_handle_rect(0, getRect().getHeight(),
								getRect().getWidth(), 0);
		mDragHandle = new LLDragHandleTop("Drag Handle", drag_handle_rect,
										  title);
	}
	addChild(mDragHandle);

	// Resize Handle
	mResizable = resizable;
	mMinWidth = min_width;
	mMinHeight = min_height;

	if (mResizable)
	{
		// Resize bars (sides)
		constexpr S32 RESIZE_BAR_THICKNESS = 3;
		mResizeBar[LLResizeBar::LEFT] =
			new LLResizeBar("resizebar_left", this,
							LLRect(0, getRect().getHeight(),
								   RESIZE_BAR_THICKNESS, 0),
							min_width, S32_MAX, LLResizeBar::LEFT);
		addChild(mResizeBar[0]);

		mResizeBar[LLResizeBar::TOP] =
			new LLResizeBar("resizebar_top", this,
							LLRect(0, getRect().getHeight(),
								   getRect().getWidth(),
								   getRect().getHeight() -
								   RESIZE_BAR_THICKNESS),
							min_height, S32_MAX, LLResizeBar::TOP);
		addChild(mResizeBar[1]);

		mResizeBar[LLResizeBar::RIGHT] =
			new LLResizeBar("resizebar_right", this,
							LLRect(getRect().getWidth() - RESIZE_BAR_THICKNESS,
								   getRect().getHeight(), getRect().getWidth(),
								   0),
							min_width, S32_MAX, LLResizeBar::RIGHT);
		addChild(mResizeBar[2]);

		mResizeBar[LLResizeBar::BOTTOM] =
			new LLResizeBar("resizebar_bottom", this,
							LLRect(0, RESIZE_BAR_THICKNESS,
								   getRect().getWidth(), 0),
							min_height, S32_MAX, LLResizeBar::BOTTOM);
		addChild(mResizeBar[3]);

		// Resize handles (corners)
		mResizeHandle[0] =
			new LLResizeHandle("Resize Handle",
							   LLRect(getRect().getWidth() -
									  RESIZE_HANDLE_WIDTH,
									  RESIZE_HANDLE_HEIGHT,
									  getRect().getWidth(), 0),
							   min_width, min_height,
							   LLResizeHandle::RIGHT_BOTTOM);
		addChild(mResizeHandle[0]);

		mResizeHandle[1] =
			new LLResizeHandle("resize",
							   LLRect(getRect().getWidth() -
									  RESIZE_HANDLE_WIDTH,
									  getRect().getHeight(),
									  getRect().getWidth(),
									  getRect().getHeight() -
									  RESIZE_HANDLE_HEIGHT),
							   min_width, min_height,
							   LLResizeHandle::RIGHT_TOP);
		addChild(mResizeHandle[1]);

		mResizeHandle[2] =
			new LLResizeHandle("resize",
							   LLRect(0, RESIZE_HANDLE_HEIGHT,
									  RESIZE_HANDLE_WIDTH, 0),
							   min_width, min_height,
							   LLResizeHandle::LEFT_BOTTOM);
		addChild(mResizeHandle[2]);

		mResizeHandle[3] =
			new LLResizeHandle("resize",
							   LLRect(0, getRect().getHeight(),
									  RESIZE_HANDLE_WIDTH,
									  getRect().getHeight() -
									  RESIZE_HANDLE_HEIGHT),
							   min_width, min_height,
							   LLResizeHandle::LEFT_TOP);
		addChild(mResizeHandle[3]);
	}

	// Close button.
	if (close_btn)
	{
		mButtonsEnabled[BUTTON_CLOSE] = true;
	}

	// Minimize button only for top draggers
	if (!drag_on_left && minimizable)
	{
		mButtonsEnabled[BUTTON_MINIMIZE] = true;
	}

	// Keep track of whether this window has ever been dragged while it was
	// minimized. If it has, we will remember its position for the next time
	// it is minimized.
	mHasBeenDraggedWhileMinimized = false;
	mPreviousMinimizedLeft = 0;
	mPreviousMinimizedBottom = 0;

	buildButtons();

#if 0	// JC: Do not do this here, because many floaters first construct
		// themselves, then show themselves. Put it in setVisibleAndFrontmost.
	make_ui_sound("UISndWindowOpen");
#endif

	// RN: floaters are created in the invisible state
	setVisible(false);

	if (gFloaterViewp && !getParent())
	{
		gFloaterViewp->addChild(this);
	}
}

//virtual
LLFloater::~LLFloater()
{
	delete mNotificationContext;
	mNotificationContext = NULL;

#if 0
	// Am I not hosted by another floater ?
	if (mHostHandle.isDead())
	{
		LLFloaterView* parent = dynamic_cast<LLFloaterView*>(getParent());
		if (parent)
		{
			parent->removeChild(this);
		}
	}
#endif

	// Just in case we might still have focus here, release it.
	releaseFocus();

	// This is important so that floaters with persistent rects (i.e., those
	// created with rect control rather than an LLRect) are restored in their
	// correct, non-minimized positions.
	setMinimized(false);

	delete mDragHandle;
	for (S32 i = 0; i < 4; ++i)
	{
		delete mResizeBar[i];
		delete mResizeHandle[i];
	}
}

void LLFloater::setVisible(bool visible)
{
	LLPanel::setVisible(visible);

	if (!visible)
	{
		if (gFocusMgr.childIsTopCtrl(this))
		{
			gFocusMgr.setTopCtrl(NULL);
		}

		if (gFocusMgr.childHasMouseCapture(this))
		{
			gFocusMgr.setMouseCapture(NULL);
		}
	}

	for (handle_set_iter_t it = mDependents.begin(), end = mDependents.end();
		 it != end; ++it)
	{
		LLFloater* floaterp = it->get();
		if (floaterp)
		{
			floaterp->setVisible(visible);
		}
	}
}

void LLFloater::open()
{
	if (getSoundFlags() != SILENT && (!getVisible() || isMinimized()) &&
		// Do not play open sound for hosted (tabbed) windows
		!getHost() && !sHostp)
	{
		make_ui_sound("UISndWindowOpen");
	}

	// NOTE: do not allow rehosting from one multifloater to another
	if (getHost())
	{
		// Already hosted
		getHost()->showFloater(this);
	}
	else if (sHostp)
	{
		// Needs a host; only select tabs if window they are hosted in is
		// visible
		sHostp->addFloater(this, sHostp->getVisible());
	}
	else
	{
		setMinimized(false);
		setVisibleAndFrontmost(mAutoFocus);
	}

	onOpen();
}

void LLFloater::close(bool app_quitting)
{
	// Always unminimize before trying to close. Most of the time the user will
	// never see this state.
	setMinimized(false);

	if (canClose())
	{
		if (getHost())
		{
			getHost()->removeFloater(this);
			if (gFloaterViewp)	// Paranoia
			{
				gFloaterViewp->addChild(this);
			}
		}

		if (!app_quitting && getVisible() && !getHost() &&
			getSoundFlags() != SILENT)
		{
			make_ui_sound("UISndWindowClose");
		}

		// Now close dependent floater
		while (!mDependents.empty())
		{
			handle_set_iter_t it = mDependents.begin();
			LLFloater* floaterp = it->get();
			mDependents.erase(it);
			if (floaterp)
			{
				floaterp->mDependeeHandle = LLHandle<LLFloater>();
				floaterp->close();
			}
		}

		cleanupHandles();
		gFocusMgr.clearLastFocusForGroup(this);

		if (hasFocus())
		{
			// Do this early, so UI controls will commit before the window is
			// taken down.
			releaseFocus();

			// Give focus to dependee floater if it exists, and we had focus
			// first
			if (isDependent())
			{
				LLFloater* dependee = mDependeeHandle.get();
				if (dependee && !dependee->isDead())
				{
					dependee->setFocus(true);
				}
			}
		}

		// Let floater do cleanup.
		onClose(app_quitting);
	}
}

//virtual
void LLFloater::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);
}

void LLFloater::releaseFocus()
{
	if (gFocusMgr.childIsTopCtrl(this))
	{
		gFocusMgr.setTopCtrl(NULL);
	}

	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}

	if (gFocusMgr.childHasMouseCapture(this))
	{
		gFocusMgr.setMouseCapture(NULL);
	}
}

void LLFloater::setResizeLimits(S32 min_width, S32 min_height)
{
	mMinWidth = min_width;
	mMinHeight = min_height;

	for (S32 i = 0; i < 4; ++i)
	{
		if (mResizeBar[i])
		{
			if (i == LLResizeBar::LEFT || i == LLResizeBar::RIGHT)
			{
				mResizeBar[i]->setResizeLimits(min_width, S32_MAX);
			}
			else
			{
				mResizeBar[i]->setResizeLimits(min_height, S32_MAX);
			}
		}
		if (mResizeHandle[i])
		{
			mResizeHandle[i]->setResizeLimits(min_width, min_height);
		}
	}
}

bool LLFloater::resizedFromHandles() const
{
	for (S32 i = 0; i < 4; ++i)
	{
		if (mResizeBar[i] && mResizeBar[i]->resizing())
		{
			return true;
		}
		if (mResizeHandle[i] && mResizeHandle[i]->resizing())
		{
			return true;
		}
	}
	return false;
}

void LLFloater::center()
{
	if (gFloaterViewp && !getHost())	// Hosted floaters cannot move
	{
		centerWithin(gFloaterViewp->getRect());
	}
}

void LLFloater::applyRectControl()
{
	if (!getRectControl().empty())
	{
		const LLRect& rect =
			LLUI::sConfigGroup->getRect(getRectControl().c_str());
		translate(rect.mLeft - getRect().mLeft,
				  rect.mBottom - getRect().mBottom);
		if (mResizable)
		{
			reshape(llmax(mMinWidth, rect.getWidth()),
					llmax(mMinHeight, rect.getHeight()));
		}
	}
}

void LLFloater::applyTitle()
{
	if (!mDragHandle)
	{
		return;
	}

	if (isMinimized() && !mShortTitle.empty())
	{
		mDragHandle->setTitle(mShortTitle);
	}
	else
	{
		mDragHandle->setTitle (mTitle);
	}
}

const std::string& LLFloater::getCurrentTitle() const
{
	return mDragHandle ? mDragHandle->getTitle() : LLStringUtil::null;
}

void LLFloater::setTitle(const std::string& title)
{
	mTitleIsPristine = false;
	mTitle = title;
	applyTitle();
}

std::string LLFloater::getTitle()
{
	if (mTitle.empty() && mDragHandle)
	{
		return mDragHandle->getTitle();
	}

	return mTitle;
}

void LLFloater::setShortTitle(const std::string& short_title)
{
	mShortTitle = short_title;
	applyTitle();
}

std::string LLFloater::getShortTitle()
{
	if (mShortTitle.empty() && mDragHandle)
	{
		return mDragHandle->getTitle();
	}

	return mShortTitle;
}

bool LLFloater::canSnapTo(LLView* other_view)
{
	if (!other_view)
	{
		llwarns << "Cannot snap to a NULL view" << llendl;
		return false;
	}

	if (other_view != getParent())
	{
		LLFloater* floaterp = other_view->asFloater();
		if (floaterp && floaterp->getSnapTarget() == getHandle() &&
			mDependents.count(floaterp->getHandle()))
		{
			// This is a dependent that is already snapped to us, so do not
			// snap back to it
			return false;
		}
	}

	return LLPanel::canSnapTo(other_view);
}

void LLFloater::snappedTo(LLView* snap_view)
{
	if (!snap_view || snap_view == getParent())
	{
		clearSnapTarget();
	}
	else
	{
		LLFloater* floaterp = snap_view->asFloater();
		if (floaterp)
		{
			setSnapTarget(floaterp->getHandle());
		}
	}
}

void LLFloater::userSetShape(const LLRect& new_rect)
{
	const LLRect old_rect = getRect();
	LLView::userSetShape(new_rect);

	// If not minimized, adjust all snapped dependents to new shape
	if (!isMinimized())
	{
		// Gather all snapped dependents
		for (handle_set_iter_t it = mDependents.begin(),
							   end = mDependents.end();
			 it != end; ++it)
		{
			LLFloater* floaterp = it->get();
			// Is a dependent snapped to us ?
			if (floaterp && floaterp->getSnapTarget() == getHandle())
			{
				S32 delta_x = 0;
				S32 delta_y = 0;
				// Check to see if it snapped to right or top, and move if
				// dependee floater is resizing
				LLRect dependent_rect = floaterp->getRect();
				S32 old_width = old_rect.getWidth();
					// dependent on my right ?
				if (dependent_rect.mLeft - getRect().mLeft >= old_width ||
					// dependent aligned with my right ?
					dependent_rect.mRight == getRect().mLeft + old_width)
				{
					// Was snapped directly onto right side or aligned with it
					delta_x += new_rect.getWidth() - old_width;
				}
				S32 old_height = old_rect.getHeight();
				if (dependent_rect.mBottom - getRect().mBottom >= old_height ||
					dependent_rect.mTop == getRect().mBottom + old_height)
				{
					// Was snapped directly onto top side or aligned with it
					delta_y += new_rect.getHeight() - old_height;
				}

				// Take translation of dependee floater into account as well
				delta_x += new_rect.mLeft - old_rect.mLeft;
				delta_y += new_rect.mBottom - old_rect.mBottom;

				dependent_rect.translate(delta_x, delta_y);
				floaterp->userSetShape(dependent_rect);
			}
		}
		if (resizedFromHandles())
		{
			sResizing = true;
			sLastSizeX = getRect().getWidth();
			sLastSizeY = getRect().getHeight();
		}
	}
	else if (new_rect.mLeft != old_rect.mLeft ||
			 new_rect.mBottom != old_rect.mBottom)
	{
		// If minimized, and origin has changed
		mHasBeenDraggedWhileMinimized = true;
	}
}

void LLFloater::setMinimized(bool minimize)
{
	if (minimize == mMinimized) return;

	if (minimize)
	{
		mExpandedRect = getRect();

		// If the floater has been dragged while minimized in the past, then
		// locate it at its previous minimized location. Otherwise, ask the
		// view for a minimize position.
		if (mHasBeenDraggedWhileMinimized)
		{
			setOrigin(mPreviousMinimizedLeft, mPreviousMinimizedBottom);
		}
		else if (gFloaterViewp)	// Paranoa
		{
			S32 left, bottom;
			gFloaterViewp->getMinimizePosition(&left, &bottom);
			setOrigin(left, bottom);
		}

		if (mButtonsEnabled[BUTTON_MINIMIZE])
		{
			mButtonsEnabled[BUTTON_MINIMIZE] = false;
			mButtonsEnabled[BUTTON_RESTORE] = true;
		}

		if (mDragHandle)
		{
			mDragHandle->setVisible(true);
		}
		setBorderVisible(true);

		for (handle_set_iter_t it = mDependents.begin(),
							   end = mDependents.end();
			 it != end; ++it)
		{
			LLFloater* floaterp = it->get();
			if (floaterp)
			{
				if (floaterp->isMinimizeable())
				{
					floaterp->setMinimized(true);
				}
				else if (!floaterp->isMinimized())
				{
					floaterp->setVisible(false);
				}
			}
		}

		// Lose keyboard focus when minimized
		releaseFocus();

		for (S32 i = 0; i < 4; ++i)
		{
			if (mResizeBar[i])
			{
				mResizeBar[i]->setEnabled(false);
			}
			if (mResizeHandle[i])
			{
				mResizeHandle[i]->setEnabled(false);
			}
		}

		mMinimized = true;

		// Reshape *after* setting mMinimized
		reshape(MINIMIZED_WIDTH, LLFLOATER_HEADER_SIZE);
	}
	else
	{
		// If this window has been dragged while minimized (at any time),
		// remember its position for the next time it's minimized.
		if (mHasBeenDraggedWhileMinimized)
		{
			const LLRect& currentRect = getRect();
			mPreviousMinimizedLeft = currentRect.mLeft;
			mPreviousMinimizedBottom = currentRect.mBottom;
		}

		setOrigin(mExpandedRect.mLeft, mExpandedRect.mBottom);

		if (mButtonsEnabled[BUTTON_RESTORE])
		{
			mButtonsEnabled[BUTTON_MINIMIZE] = true;
			mButtonsEnabled[BUTTON_RESTORE] = false;
		}

		// show dependent floater
		for (handle_set_iter_t it = mDependents.begin(),
							   end = mDependents.end();
			 it != end; ++it)
		{
			LLFloater* floaterp = it->get();
			if (floaterp)
			{
				floaterp->setMinimized(false);
				floaterp->setVisible(true);
			}
		}

		for (S32 i = 0; i < 4; ++i)
		{
			if (mResizeBar[i])
			{
				mResizeBar[i]->setEnabled(isResizable());
			}
			if (mResizeHandle[i])
			{
				mResizeHandle[i]->setEnabled(isResizable());
			}
		}

		mMinimized = false;

		// Reshape *after* setting mMinimized
		reshape(mExpandedRect.getWidth(), mExpandedRect.getHeight());
	}

	applyTitle ();

	make_ui_sound("UISndWindowClose");
	updateButtons();
}

void LLFloater::setFocus(bool b)
{
	if (b && getIsChrome())
	{
		return;
	}
	LLUICtrl* last_focus = gFocusMgr.getLastFocusForGroup(this);
	// A descendent already has focus
	bool child_had_focus = gFocusMgr.childHasKeyboardFocus(this);

	// Give focus to first valid descendent
	LLPanel::setFocus(b);

	if (b)
	{
		LLFloaterView* parent = dynamic_cast<LLFloaterView*>(getParent());
		// Only push focused floaters to front of stack if not in midst of
		// ctrl-tab cycle
		if (!getHost() && (!parent || !parent->getCycleMode()) &&
			!isFrontmost())
		{
			setFrontmost();
		}

		// When getting focus, delegate to last descendent which had focus
		if (last_focus && !child_had_focus &&
			last_focus->isInEnabledChain() &&
			last_focus->isInVisibleChain())
		{
			// *FIX: should handle case where focus doesn't stick
			last_focus->setFocus(true);
		}
	}
}

//virtual
void LLFloater::setIsChrome(bool is_chrome)
{
	// Chrome floaters do not take focus at all
	if (is_chrome)
	{
		// Remove focus if we are changing to chrome
		setFocus(false);
		// Cannot CTRL-TAB to "chrome" floaters
		setFocusRoot(false);
	}

	// No titles displayed on "chrome" floaters
	if (mDragHandle)
	{
		mDragHandle->setTitleVisible(!is_chrome);
	}

	LLPanel::setIsChrome(is_chrome);
}

void LLFloater::setTitleVisible(bool visible)
{
	if (mDragHandle)
	{
		mDragHandle->setTitleVisible(visible);
	}
}

// Change the draw style to account for the foreground state.
void LLFloater::setForeground(bool front)
{
	if (front != mForeground)
	{
		mForeground = front;
		if (mDragHandle)
			mDragHandle->setForeground(front);

		if (!front)
		{
			releaseFocus();
		}

		setBackgroundOpaque(front);
	}
}

void LLFloater::cleanupHandles()
{
	// Remove handles to non-existent dependents
	for (handle_set_iter_t it = mDependents.begin(); it != mDependents.end(); )
	{
		LLFloater* floaterp = it->get();
		if (!floaterp)
		{
			it = mDependents.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void LLFloater::setHost(LLMultiFloater* host)
{
	if (mHostHandle.isDead() && host)
	{
		// Make buttons smaller for hosted windows to differentiate from parent
		mButtonScale = 0.9f;

		// Add tear off button
		if (mCanTearOff)
		{
			mButtonsEnabled[BUTTON_TEAR_OFF] = true;
		}
	}
	else if (!mHostHandle.isDead() && !host)
	{
		mButtonScale = 1.f;
#if 0
		mButtonsEnabled[BUTTON_TEAR_OFF] = false;
#endif
	}
	updateButtons();
	if (host)
	{
		mHostHandle = host->getHandle();
		mLastHostHandle = host->getHandle();
	}
	else
	{
		mHostHandle.markDead();
	}
}

void LLFloater::moveResizeHandlesToFront()
{
	for (S32 i = 0; i < 4; ++i)
	{
		if (mResizeBar[i])
		{
			sendChildToFront(mResizeBar[i]);
		}
	}

	for (S32 i = 0; i < 4; ++i)
	{
		if (mResizeHandle[i])
		{
			sendChildToFront(mResizeHandle[i]);
		}
	}
}

bool LLFloater::isFrontmost()
{
	return getVisible() &&
		   gFloaterViewp && gFloaterViewp->getFrontmost() == this;
}

void LLFloater::addDependentFloater(LLFloater* floaterp, bool reposition)
{
	if (!gFloaterViewp)	return;	// Paranoia

	mDependents.insert(floaterp->getHandle());
	floaterp->mDependeeHandle = getHandle();

	if (reposition)
	{
		floaterp->setRect(gFloaterViewp->findNeighboringPosition(this,
																 floaterp));
		floaterp->setSnapTarget(getHandle());
	}
	gFloaterViewp->adjustToFitScreen(floaterp);
	if (floaterp->isFrontmost())
	{
		// Make sure to bring self and sibling floaters to front
		gFloaterViewp->bringToFront(floaterp);
	}
}

void LLFloater::addDependentFloater(LLHandle<LLFloater> dependent,
									bool reposition)
{
	LLFloater* dependent_floaterp = dependent.get();
	if (dependent_floaterp)
	{
		addDependentFloater(dependent_floaterp, reposition);
	}
}

void LLFloater::removeDependentFloater(LLFloater* floaterp)
{
	mDependents.erase(floaterp->getHandle());
	floaterp->mDependeeHandle = LLHandle<LLFloater>();
}

bool LLFloater::offerClickToButton(S32 x, S32 y, MASK mask,
								   EFloaterButtons index)
{
	if (mButtonsEnabled[index])
	{
		LLButton* my_butt = mButtons[index];
		S32 local_x = x - my_butt->getRect().mLeft;
		S32 local_y = y - my_butt->getRect().mBottom;

		if (my_butt->pointInView(local_x, local_y) &&
			my_butt->handleMouseDown(local_x, local_y, mask))
		{
			// The button handled it
			return true;
		}
	}
	return false;
}

//virtual
bool LLFloater::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mMinimized)
	{
		// Offer the click to titlebar buttons. Note: this block and the
		// offerClickToButton helper method could be removed because the parent
		// container will handle it for us but we will keep it here for safety
		// until after reworking the panel code to manage hidden children.
		if (offerClickToButton(x, y, mask, BUTTON_CLOSE) ||
			offerClickToButton(x, y, mask, BUTTON_RESTORE) ||
			offerClickToButton(x, y, mask, BUTTON_TEAR_OFF))
		{
			return true;
		}

		// Otherwise pass to drag handle for movement
		return mDragHandle->handleMouseDown(x, y, mask);
	}

	bringToFront(x, y);
	return LLPanel::handleMouseDown(x, y, mask);
}

//virtual
bool LLFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	bool was_minimized = mMinimized;
	bringToFront(x, y);
	return was_minimized || LLPanel::handleRightMouseDown(x, y, mask);
}

bool LLFloater::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	bringToFront(x, y);
	return LLPanel::handleMiddleMouseDown(x, y, mask);
}

//virtual
bool LLFloater::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	bool was_minimized = mMinimized;
	setMinimized(false);
	return was_minimized || LLPanel::handleDoubleClick(x, y, mask);
}

void LLFloater::bringToFront(S32 x, S32 y)
{
	if (getVisible() && pointInView(x, y))
	{
		LLMultiFloater* hostp = getHost();
		if (hostp)
		{
			hostp->showFloater(this);
		}
		else
		{
			LLFloaterView* parent = dynamic_cast<LLFloaterView*>(getParent());
			if (parent)
			{
				parent->bringToFront(this);
			}
		}
	}
}

//virtual
void LLFloater::setVisibleAndFrontmost(bool take_focus)
{
	setVisible(true);
	setFrontmost(take_focus);
}

void LLFloater::setFrontmost(bool take_focus)
{
	LLMultiFloater* hostp = getHost();
	if (hostp)
	{
		// This will bring the host floater to the front and select the
		// appropriate panel
		hostp->showFloater(this);
	}
	else
	{
		// There are more than one floater view so we need to query our parent
		// directly
		LLFloaterView* parent = dynamic_cast<LLFloaterView*>(getParent());
		if (parent)
		{
			parent->bringToFront(this, take_focus);
		}
	}
}

//static
void LLFloater::onClickMinimize(void* userdata)
{
	LLFloater* self = (LLFloater*) userdata;
	if (self)
	{
		self->setMinimized(!self->isMinimized());
	}
}

void LLFloater::onClickTearOff(void* userdata)
{
	LLFloater* self = (LLFloater*) userdata;
	if (!self || !gFloaterViewp) return;

	LLMultiFloater* host_floater = self->getHost();
	if (host_floater) //Tear off
	{
		LLRect new_rect;
		host_floater->removeFloater(self);
		// Re-parent to floater view
		gFloaterViewp->addChild(self);

		self->open();

		// Only force position for floaters that don't have that data saved
		if (self->getRectControl().empty())
		{
			new_rect.setLeftTopAndSize(host_floater->getRect().mLeft + 5,
									   host_floater->getRect().mTop -
									   LLFLOATER_HEADER_SIZE - 5,
									   self->getRect().getWidth(),
									   self->getRect().getHeight());
			self->setRect(new_rect);
		}
		gFloaterViewp->adjustToFitScreen(self);
		// Give focus to new window to keep continuity for the user
		self->setFocus(true);
	}
	else  //Attach to parent.
	{
		LLMultiFloater* new_host;
		new_host = (LLMultiFloater*)self->mLastHostHandle.get();
		if (new_host)
		{
			// To reenable minimize button if it was minimized
			self->setMinimized(false);

			new_host->showFloater(self);

			// Make sure host is visible
			new_host->open();
		}
	}
}

//static
LLFloater* LLFloater::getClosableFloaterFromFocus()
{
	if (!gFloaterViewp)
	{
		return NULL;
	}

	LLFloater* floaterp = NULL;

	for (child_list_const_iter_t it = gFloaterViewp->getChildList()->begin(),
								 end = gFloaterViewp->getChildList()->end();
		 it != end; ++it)
	{
		LLFloater* candidatep = (*it)->asFloater();
		if (candidatep && candidatep->hasFocus())
		{
			floaterp = candidatep;
			break;
		}
	}

	// The focused floater may not be closable: find and close a parental
	// floater that is closeable, if any.
	while (floaterp)
	{
		if (floaterp->isCloseable())
		{
			break;
		}
		floaterp = gFloaterViewp->getParentFloater(floaterp);
	}

	return floaterp;
}

//static
void LLFloater::closeFocusedFloater()
{
	LLFloater* floaterp = getClosableFloaterFromFocus();
	if (floaterp)
	{
		floaterp->close();
	}

	// If nothing took focus after closing focused floater give it to next
	// floater (to allow closing multiple windows via keyboard in rapid
	// succession)
	if (gFloaterViewp && !gFocusMgr.getKeyboardFocus())
	{
		// *HACK: use gFloaterViewp directly in case we are using CTRL-W to
		// close snapshot window which sits in gSnapshotFloaterViewp, and
		// needs to pass focus on to normal floater view
		gFloaterViewp->focusFrontFloater();
	}
}

//static
void LLFloater::onClickClose(void* userdata)
{
	LLFloater* self = (LLFloater*)userdata;
	if (self)
	{
		self->close();
	}
}

//virtual
void LLFloater::draw()
{
	// Draw background
	if (isBackgroundVisible())
	{
		S32 left = LLPANEL_BORDER_WIDTH;
		S32 top = getRect().getHeight() - LLPANEL_BORDER_WIDTH;
		S32 right = getRect().getWidth() - LLPANEL_BORDER_WIDTH;
		S32 bottom = LLPANEL_BORDER_WIDTH;

		LLColor4 shadow_color = LLUI::sColorDropShadow;
		F32 shadow_offset = (F32)LLUI::sDropShadowFloater;
		if (!isBackgroundOpaque())
		{
			shadow_offset *= 0.2f;
			shadow_color.mV[VALPHA] *= 0.5f;
		}
		gl_drop_shadow(left, top, right, bottom, shadow_color,
					   ll_round(shadow_offset));

		// No transparent windows in simple UI
		if (isBackgroundOpaque())
		{
			gl_rect_2d(left, top, right, bottom, getBackgroundColor());
		}
		else
		{
			gl_rect_2d(left, top, right, bottom, getTransparentColor());
		}

		if (gFocusMgr.childHasKeyboardFocus(this) && !getIsChrome() &&
			!getCurrentTitle().empty())
		{
			// Draw highlight on title bar to indicate focus. RDW
			static S32 font_line_height = 0;
			if (!font_line_height)
			{
				static const LLFontGL* font =  LLFontGL::getFontSansSerif();
				font_line_height = font->getLineHeight() - 1;
			}
			LLRect r = getRect();
			gl_rect_2d_offset_local(0, r.getHeight(), r.getWidth(),
									r.getHeight() - font_line_height,
									LLUI::sTitleBarFocusColor, 0, true);
		}
	}

	LLPanel::updateDefaultBtn();

	if (getDefaultButton())
	{
		if (hasFocus() && getDefaultButton()->getEnabled())
		{
			LLFocusableElement* focus_ctrl = gFocusMgr.getKeyboardFocus();
			// Is this button a direct descendent and not a nested widget (e.g.
			// checkbox) ?
			LLButton* btn = dynamic_cast<LLButton*>(focus_ctrl);
			bool focus_is_child_button = btn && btn->getParent() == this;
			// only enable default button when current focus is not a button
			getDefaultButton()->setBorderEnabled(!focus_is_child_button);
		}
		else
		{
			getDefaultButton()->setBorderEnabled(false);
		}
	}
	if (isMinimized())
	{
		for (S32 i = 0; i < BUTTON_COUNT; ++i)
		{
			drawChild(mButtons[i]);
		}
		drawChild(mDragHandle);
	}
	else
	{
		// Do not call LLPanel::draw() since we have implemented custom
		// background rendering
		LLView::draw();
	}

	if (isBackgroundVisible())
	{
		// Add in a border to improve spacialized visual clarity; use lines
		// instead of gl_rect_2d so we can round the edges as per James'
		// recommendation
		LLUI::setLineWidth(1.5f);
		LLColor4 outline_color =
			gFocusMgr.childHasKeyboardFocus(this) ? LLUI::sFloaterFocusBorderColor
												  : LLUI::sFloaterUnfocusBorderColor;
		gl_rect_2d_offset_local(0, getRect().getHeight() + 1,
								getRect().getWidth() + 1, 0, outline_color,
								-LLPANEL_BORDER_WIDTH, false);
		LLUI::setLineWidth(1.f);
	}

	// Update tearoff button for torn off floaters when last host goes away
	if (mCanTearOff && !getHost())
	{
		LLFloater* old_host = mLastHostHandle.get();
		if (!old_host)
		{
			setCanTearOff(false);
		}
	}
}

void LLFloater::setCanMinimize(bool can_minimize)
{
	// if removing minimize/restore button programmatically,
	// go ahead and unminimize floater
	if (!can_minimize)
	{
		setMinimized(false);
	}

	mButtonsEnabled[BUTTON_MINIMIZE] = can_minimize && !isMinimized();
	mButtonsEnabled[BUTTON_RESTORE]  = can_minimize &&  isMinimized();

	updateButtons();
}

void LLFloater::setCanClose(bool can_close)
{
	mButtonsEnabled[BUTTON_CLOSE] = can_close;
	updateButtons();
}

void LLFloater::setCanTearOff(bool can_tear_off)
{
	mCanTearOff = can_tear_off;
	mButtonsEnabled[BUTTON_TEAR_OFF] = mCanTearOff && !mHostHandle.isDead();

	updateButtons();
}

void LLFloater::setCanResize(bool can_resize)
{
	if (mResizable && !can_resize)
	{
		for (S32 i = 0; i < 4; ++i)
		{
			removeChild(mResizeBar[i], true);
			mResizeBar[i] = NULL;

			removeChild(mResizeHandle[i], true);
			mResizeHandle[i] = NULL;
		}
	}
	else if (!mResizable && can_resize)
	{
		// Resize bars (sides)
		constexpr S32 RESIZE_BAR_THICKNESS = 3;
		mResizeBar[0] = new LLResizeBar("resizebar_left", this,
										LLRect(0, getRect().getHeight(),
											   RESIZE_BAR_THICKNESS, 0),
										mMinWidth, S32_MAX, LLResizeBar::LEFT);
		addChild(mResizeBar[0]);

		mResizeBar[1] = new LLResizeBar("resizebar_top", this,
										LLRect(0, getRect().getHeight(),
											   getRect().getWidth(),
											   getRect().getHeight() -
											   RESIZE_BAR_THICKNESS),
										mMinHeight, S32_MAX,
										LLResizeBar::TOP);
		addChild(mResizeBar[1]);

		mResizeBar[2] = new LLResizeBar("resizebar_right", this,
										LLRect(getRect().getWidth() -
											   RESIZE_BAR_THICKNESS,
											   getRect().getHeight(),
											   getRect().getWidth(), 0),
										mMinWidth, S32_MAX,
										LLResizeBar::RIGHT);
		addChild(mResizeBar[2]);

		mResizeBar[3] = new LLResizeBar("resizebar_bottom", this,
										LLRect(0, RESIZE_BAR_THICKNESS,
											   getRect().getWidth(), 0),
										mMinHeight, S32_MAX,
										LLResizeBar::BOTTOM);
		addChild(mResizeBar[3]);

		// Resize handles (corners)
		mResizeHandle[0] = new LLResizeHandle("Resize Handle",
											  LLRect(getRect().getWidth() -
													 RESIZE_HANDLE_WIDTH,
													 RESIZE_HANDLE_HEIGHT,
													 getRect().getWidth(), 0),
											  mMinWidth, mMinHeight,
											  LLResizeHandle::RIGHT_BOTTOM);
		addChild(mResizeHandle[0]);

		mResizeHandle[1] = new LLResizeHandle("resize",
											  LLRect(getRect().getWidth() -
													 RESIZE_HANDLE_WIDTH,
													 getRect().getHeight(),
													 getRect().getWidth(),
													 getRect().getHeight() -
													 RESIZE_HANDLE_HEIGHT),
											  mMinWidth, mMinHeight,
											  LLResizeHandle::RIGHT_TOP);
		addChild(mResizeHandle[1]);

		mResizeHandle[2] = new LLResizeHandle("resize",
											  LLRect(0, RESIZE_HANDLE_HEIGHT,
													 RESIZE_HANDLE_WIDTH, 0),
											  mMinWidth, mMinHeight,
											  LLResizeHandle::LEFT_BOTTOM);
		addChild(mResizeHandle[2]);

		mResizeHandle[3] = new LLResizeHandle("resize",
											  LLRect(0, getRect().getHeight(),
													 RESIZE_HANDLE_WIDTH,
													 getRect().getHeight() -
													 RESIZE_HANDLE_HEIGHT),
											  mMinWidth, mMinHeight,
											  LLResizeHandle::LEFT_TOP);
		addChild(mResizeHandle[3]);
	}
	mResizable = can_resize;
}

void LLFloater::setCanDrag(bool can_drag)
{
	// If we delete drag handle, we no longer have access to the floater title
	// so just enable/disable it
	if (!can_drag && mDragHandle->getEnabled())
	{
		mDragHandle->setEnabled(false);
	}
	else if (can_drag && !mDragHandle->getEnabled())
	{
		mDragHandle->setEnabled(true);
	}
}

void LLFloater::updateButtons()
{
	S32 close_box_size = ll_roundp((F32)LLFLOATER_CLOSE_BOX_SIZE *
								   mButtonScale);
	S32 button_count = 0;
	for (S32 i = 0; i < BUTTON_COUNT; ++i)
	{
		if (!mButtons[i]) continue;
		mButtons[i]->setEnabled(mButtonsEnabled[i]);

		if (mButtonsEnabled[i] ||
			// *HACK: always render close button for hosted floaters so that
			// users do not accidentally hit the button when closing multiple
			// windows in the chatterbox
			(i == BUTTON_CLOSE && mButtonScale != 1.f))
		{
			++button_count;

			LLRect btn_rect;
			if (mDragOnLeft)
			{
				btn_rect.setLeftTopAndSize(LLPANEL_BORDER_WIDTH,
										   getRect().getHeight() -
										   CLOSE_BOX_FROM_TOP -
										   (LLFLOATER_CLOSE_BOX_SIZE + 1) *
										   button_count,
										   close_box_size, close_box_size);
			}
			else
			{
				btn_rect.setLeftTopAndSize(getRect().getWidth() -
										   LLPANEL_BORDER_WIDTH -
										   (LLFLOATER_CLOSE_BOX_SIZE + 1) *
										   button_count,
										   getRect().getHeight() -
										   CLOSE_BOX_FROM_TOP,
										   close_box_size, close_box_size);
			}

			mButtons[i]->setRect(btn_rect);
			mButtons[i]->setVisible(true);
			// The restore button should have a tab stop so that it takes
			// action when you Ctrl-Tab to a minimized floater
			mButtons[i]->setTabStop(i == BUTTON_RESTORE);
		}
		else if (mButtons[i])
		{
			mButtons[i]->setVisible(false);
		}
	}
	if (mDragHandle)
	{
		mDragHandle->setMaxTitleWidth(getRect().getWidth() -
									  button_count *
									  (LLFLOATER_CLOSE_BOX_SIZE + 1));
	}
}

void LLFloater::buildButtons()
{
	static std::vector<std::string> tooltips;
	if (tooltips.empty())
	{
		tooltips.reserve(BUTTON_COUNT);
		for (U32 i = 0; i < BUTTON_COUNT; ++i)
		{
			tooltips.emplace_back(LLTrans::getUIString(sButtonToolTipNames[i]));
		}
	}

	S32 close_box_size = ll_roundp((F32)LLFLOATER_CLOSE_BOX_SIZE *
								   mButtonScale);
	for (S32 i = 0; i < BUTTON_COUNT; ++i)
	{
		LLRect btn_rect;
		if (mDragOnLeft)
		{
			btn_rect.setLeftTopAndSize(LLPANEL_BORDER_WIDTH,
									   getRect().getHeight() -
									   CLOSE_BOX_FROM_TOP -
									   (LLFLOATER_CLOSE_BOX_SIZE + 1) *
									   (i + 1),
									   close_box_size, close_box_size);
		}
		else
		{
			btn_rect.setLeftTopAndSize(getRect().getWidth() -
									   LLPANEL_BORDER_WIDTH -
									   (LLFLOATER_CLOSE_BOX_SIZE + 1) *
									   (i + 1),
									   getRect().getHeight() -
									   CLOSE_BOX_FROM_TOP,
									   close_box_size, close_box_size);
		}

		LLButton* buttonp = new LLButton(sButtonNames[i], btn_rect,
										 sButtonActiveImageNames[i],
										 sButtonPressedImageNames[i],
										 NULL, sButtonCallbacks[i], this,
										 LLFontGL::getFontSansSerif());

		buttonp->setTabStop(false);
		buttonp->setFollowsTop();
		buttonp->setFollowsRight();
		buttonp->setToolTip(tooltips[i]);
		buttonp->setImageColor(LLUI::sFloaterButtonImageColor);
		buttonp->setHoverImages(sButtonPressedImageNames[i],
								sButtonPressedImageNames[i]);
		buttonp->setScaleImage(true);
		buttonp->setSaveToXML(false);
		addChild(buttonp);
		mButtons[i] = buttonp;
	}

	updateButtons();
}

/////////////////////////////////////////////////////
// LLFloaterView

LLFloaterView::LLFloaterView(const std::string& name, const LLRect& rect)
:	LLUICtrl(name, rect, false, NULL, NULL, FOLLOWS_ALL),
	mFocusCycleMode(false),
	mSnapOffsetBottom(0)
{
	setTabStop(false);
	resetStartingFloaterPosition();
}

// By default, adjust vertical.
void LLFloaterView::reshape(S32 width, S32 height, bool called_from_parent)
{
	reshapeFloater(width, height, called_from_parent, ADJUST_VERTICAL_YES);
}

// When reshaping this view, make the floaters follow their closest edge.
void LLFloaterView::reshapeFloater(S32 width, S32 height,
								   bool called_from_parent,
								   bool adjust_vertical)
{
	S32 old_width = getRect().getWidth();
	S32 old_height = getRect().getHeight();

	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (!floaterp || floaterp->isDependent())
		{
			// Dependents use same follow flags as their "dependee"
			continue;
		}

		LLRect r = floaterp->getRect();

		// Compute absolute distance from each edge of screen
		S32 left_offset = abs(r.mLeft);
		S32 right_offset = abs(old_width - r.mRight);

		S32 top_offset = abs(old_height - r.mTop);
		S32 bottom_offset = abs(r.mBottom);

		// Make if follow the edge it is closest to
		U32 follow_flags = 0x0;

		if (left_offset < right_offset)
		{
			follow_flags |= FOLLOWS_LEFT;
		}
		else
		{
			follow_flags |= FOLLOWS_RIGHT;
		}

		// "No vertical adjustment" usually means that the bottom of the view
		// has been pushed up or down. Hence we want the floaters to follow the
		// top.
		if (!adjust_vertical)
		{
			follow_flags |= FOLLOWS_TOP;
		}
		else if (top_offset < bottom_offset)
		{
			follow_flags |= FOLLOWS_TOP;
		}
		else
		{
			follow_flags |= FOLLOWS_BOTTOM;
		}

		floaterp->setFollows(follow_flags);

		// RN: all dependent floaters copy follow behavior of "parent"
		for (LLFloater::handle_set_iter_t
				dep_it = floaterp->mDependents.begin(),
				dep_end = floaterp->mDependents.end();
			 dep_it != dep_end; ++dep_it)
		{
			LLFloater* dependent_floaterp = dep_it->get();
			if (dependent_floaterp)
			{
				dependent_floaterp->setFollows(follow_flags);
			}
		}
	}

	LLView::reshape(width, height, called_from_parent);
}

void LLFloaterView::restoreAll()
{
	// Make sure all subwindows aren't minimized
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp)
		{
			floaterp->setMinimized(false);
		}
	}
}

void LLFloaterView::getNewFloaterPosition(S32* left, S32* top)
{
	// Workaround: mRect may change between when this object is created and the
	// first time it is used.
	static bool first = true;
	if (first)
	{
		resetStartingFloaterPosition();
		first = false;
	}

	constexpr S32 FLOATER_PAD = 16;
	LLCoordWindow window_size;
	gWindowp->getSize(&window_size);
	LLRect full_window(0, window_size.mY, window_size.mX, 0);
	LLRect floater_creation_rect(160,
								 full_window.getHeight() - 2 * gMenuBarHeight,
								 full_window.getWidth() * 2 / 3, 130);
	floater_creation_rect.stretch(-FLOATER_PAD);

	*left = mNextLeft;
	*top = mNextTop;

	constexpr S32 STEP = 25;
	S32 bottom = floater_creation_rect.mBottom + 2 * STEP;
	S32 right = floater_creation_rect.mRight - 4 * STEP;

	mNextTop -= STEP;
	mNextLeft += STEP;

	if (mNextTop < bottom || mNextLeft > right)
	{
		++mColumn;
		mNextTop = floater_creation_rect.mTop;
		mNextLeft = STEP * mColumn;

		if (mNextTop < bottom || mNextLeft > right)
		{
			// Advancing the column did not work, so start back at the
			// beginning
			resetStartingFloaterPosition();
		}
	}
}

void LLFloaterView::resetStartingFloaterPosition()
{
	constexpr S32 FLOATER_PAD = 16;
	LLCoordWindow window_size;
	gWindowp->getSize(&window_size);
	LLRect full_window(0, window_size.mY, window_size.mX, 0);
	LLRect floater_creation_rect(
		160,
		full_window.getHeight() - 2 * gMenuBarHeight,
		full_window.getWidth() * 2 / 3,
		130);
	floater_creation_rect.stretch(-FLOATER_PAD);

	mNextLeft = floater_creation_rect.mLeft;
	mNextTop = floater_creation_rect.mTop;
	mColumn = 0;
}

LLRect LLFloaterView::findNeighboringPosition(LLFloater* reference_floater,
											  LLFloater* neighbor)
{
	LLRect base_rect = reference_floater->getRect();
	S32 width = neighbor->getRect().getWidth();
	S32 height = neighbor->getRect().getHeight();
	LLRect new_rect = neighbor->getRect();

	LLRect expanded_base_rect = base_rect;
	expanded_base_rect.stretch(10);
	for (LLFloater::handle_set_iter_t
			dep_it = reference_floater->mDependents.begin();
		 dep_it != reference_floater->mDependents.end(); ++dep_it)
	{
		LLFloater* sibling = dep_it->get();
		// Check for dependents within 10 pixels of base floater
		if (sibling && sibling != neighbor && sibling->getVisible() &&
			expanded_base_rect.overlaps(sibling->getRect()))
		{
			base_rect.unionWith(sibling->getRect());
		}
	}

	S32 left_margin = llmax(0, base_rect.mLeft);
	S32 right_margin = llmax(0, getRect().getWidth() - base_rect.mRight);
	S32 top_margin = llmax(0, getRect().getHeight() - base_rect.mTop);
	S32 bottom_margin = llmax(0, base_rect.mBottom);

	// Find position for floater in following order right->left->bottom->top
	for (S32 i = 0; i < 5; ++i)
	{
		if (right_margin > width)
		{
			new_rect.translate(base_rect.mRight - neighbor->getRect().mLeft,
							   base_rect.mTop - neighbor->getRect().mTop);
			return new_rect;
		}
		else if (left_margin > width)
		{
			new_rect.translate(base_rect.mLeft - neighbor->getRect().mRight,
							   base_rect.mTop - neighbor->getRect().mTop);
			return new_rect;
		}
		else if (bottom_margin > height)
		{
			new_rect.translate(base_rect.mLeft - neighbor->getRect().mLeft,
							   base_rect.mBottom - neighbor->getRect().mTop);
			return new_rect;
		}
		else if (top_margin > height)
		{
			new_rect.translate(base_rect.mLeft - neighbor->getRect().mLeft,
							   base_rect.mTop - neighbor->getRect().mBottom);
			return new_rect;
		}

		// keep growing margins to find "best" fit
		left_margin += 20;
		right_margin += 20;
		top_margin += 20;
		bottom_margin += 20;
	}

	// didn't find anything, return initial rect
	return new_rect;
}

// *TODO: make this respect the floater mAutoFocus value, instead of using the
// give_focus parameter.
bool LLFloaterView::bringToFront(LLFloater* child, bool give_focus)
{
	if (!child || !getChildList())
 	{
		// NULL child or no children for us...
		return false;
	}

	if (child->getHost() ||
		std::find(getChildList()->begin(), getChildList()->end(), child) ==
			getChildList()->end())
 	{
		// This floater is hosted elsewhere and hence not one of our children,
		// abort
		return false;
	}

	std::vector<LLView*> floaters_to_move;
	// Look at all floaters...tab
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (!floaterp || !floaterp->isDependent())
		{
			continue;
		}

		// If I am a dependent floater look for floaters that have me as a
		// dependent...
		if (floaterp->mDependents.find(child->getHandle()) !=
				floaterp->mDependents.end())
		{
			// ...and make sure all children of that floater (including me) are
			// brought to front...
			for (LLFloater::handle_set_iter_t
					dep_it = floaterp->mDependents.begin(),
					dep_end = floaterp->mDependents.end();
				 dep_it != dep_end; ++dep_it)
			{
				LLFloater* sibling = dep_it->get();
				if (sibling)
				{
					floaters_to_move.push_back(sibling);
				}
			}
			// ...before bringing my parent to the front...
			floaters_to_move.push_back(floaterp);
		}
	}

	for (std::vector<LLView*>::iterator it = floaters_to_move.begin(),
										end = floaters_to_move.end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp)
		{
			sendChildToFront(floaterp);
			// Always unminimize dependee, but allow dependents to stay
			// minimized
			if (!floaterp->isDependent())
			{
				floaterp->setMinimized(false);
			}
		}
	}
	floaters_to_move.clear();

	// ...then bringing my own dependents to the front...
	for (LLFloater::handle_set_iter_t it = child->mDependents.begin(),
									  end = child->mDependents.end();
		 it != end; ++it)
	{
		LLFloater* dependent = it->get();
		if (dependent)
		{
			sendChildToFront(dependent);
#if 0		// Do not un-minimize dependent windows automatically: respect the
			// user's wishes !
			dependent->setMinimized(false);
#endif
		}
	}

	// ...and finally bringing myself to front (do this last, so that I am left
	// in front at end of this call)
	if (*getChildList()->begin() != child)
	{
		sendChildToFront(child);
	}
	child->setMinimized(false);
	if (give_focus && !gFocusMgr.childHasKeyboardFocus(child))
	{
		child->setFocus(true);
	}

	return true;
}

void LLFloaterView::highlightFocusedFloater()
{
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (!floaterp || floaterp->isDependent())
		{
			// Skip dependent floaters, as we will handle them in a batch along
			// with their dependee
			continue;
		}

		bool has_focus = gFocusMgr.childHasKeyboardFocus(floaterp);
		for (LLFloater::handle_set_iter_t
				dep_it = floaterp->mDependents.begin(),
				dep_end = floaterp->mDependents.end();
			 dep_it != dep_end; ++dep_it)
		{
			LLFloater* dependent_floaterp = dep_it->get();
			if (dependent_floaterp &&
				gFocusMgr.childHasKeyboardFocus(dependent_floaterp))
			{
				has_focus = true;
				break;
			}
		}

		// Now set this floater and all its dependents
		floaterp->setForeground(has_focus);

		for (LLFloater::handle_set_iter_t
				dep_it = floaterp->mDependents.begin(),
				dep_end = floaterp->mDependents.end();
			 dep_it != dep_end; ++dep_it)
		{
			LLFloater* dependent_floaterp = dep_it->get();
			if (dependent_floaterp)
			{
				dependent_floaterp->setForeground(has_focus);
			}
		}

		floaterp->cleanupHandles();
	}
}

void LLFloaterView::unhighlightFocusedFloater()
{
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp)
		{
			floaterp->setForeground(false);
		}
	}
}

void LLFloaterView::focusFrontFloater()
{
	LLFloater* floaterp = getFrontmost();
	if (floaterp)
	{
		floaterp->setFocus(true);
	}
}

void LLFloaterView::getMinimizePosition(S32* left, S32* bottom)
{
	S32 row, row_start, row_end, row_delta;
	S32 col, col_start, col_end, col_delta, width;
	LLRect snap_rect_local = getLocalSnapRect();

	if (sStackMinimizedTopToBottom)
	{
		row_start = snap_rect_local.getHeight(); // - LLFLOATER_HEADER_SIZE;
		row_end = snap_rect_local.mBottom;
		row_delta = -LLFLOATER_HEADER_SIZE;
	}
	else
	{
		row_start = snap_rect_local.mBottom;
		row_end = snap_rect_local.getHeight() - LLFLOATER_HEADER_SIZE;
		row_delta = LLFLOATER_HEADER_SIZE;
	}

	width = (snap_rect_local.getWidth() - MINIMIZED_WIDTH -
			 snap_rect_local.mLeft) / sStackScreenWidthFraction;
	if (width < MINIMIZED_WIDTH)
	{
		width = MINIMIZED_WIDTH;
	}

	if (sStackMinimizedRightToLeft)
	{
		col_start = snap_rect_local.getWidth() - MINIMIZED_WIDTH;
		col_end = col_start - width;
		col_delta = -MINIMIZED_WIDTH;
	}
	else
	{
		col_start = snap_rect_local.mLeft;
		col_end = col_start + width;
		col_delta = MINIMIZED_WIDTH;
	}

	for (row = row_start; (row_delta > 0 ? row < row_end : row > row_end);
		 row += row_delta)
	{
		for (col = col_start; (col_delta > 0 ? col < col_end : col > col_end);
			 col += col_delta)
		{
			bool found_gap = true;
			for (child_list_const_iter_t it = getChildList()->begin(),
										 end = getChildList()->end();
				 it != end; ++it)
			{
				LLView* viewp = *it;
				if (!viewp) continue;

				LLFloater* floaterp = viewp->asFloater();
				if (!floaterp || !floaterp->isMinimized())
				{
					continue;
				}
				// Examine minimized children.
				LLRect r = floaterp->getRect();
				if (r.mBottom < row + LLFLOATER_HEADER_SIZE &&
					r.mBottom > row - LLFLOATER_HEADER_SIZE &&
					r.mLeft < col + MINIMIZED_WIDTH &&
					r.mLeft > col - MINIMIZED_WIDTH)
				{
					// Needs the check for off grid; cannot drag, but window
					// resize makes them off
					found_gap = false;
					break;
				}
			}
			if (found_gap)
			{
				*left = col;
				*bottom = row;
				return; // Done
			}
		}
	}

	// Crude: stack them all there when screen is full of minimized floaters.
	*left = col_start;
	*bottom = row_start;
}

void LLFloaterView::destroyAllChildren()
{
	LLView::deleteAllChildren();
}

void LLFloaterView::closeAllChildren(bool app_quitting)
{
	// Iterate over a copy of the list, because closing windows will destroy
	// some windows on the list.
	child_list_t child_list = *getChildList();

	for (child_list_const_iter_t it = child_list.begin(),
								 end = child_list.end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		child_list_const_iter_t exists = std::find(getChildList()->begin(),
												   getChildList()->end(),
												   viewp);
		if (exists == getChildList()->end())
		{
			// This floater has already been removed
			continue;
		}

		// Attempt to close floater. This will cause the "do you want to save"
		// dialogs to appear.
		LLFloater* floaterp = viewp->asFloater();
		if (floaterp && floaterp->canClose() && !floaterp->isDead())
		{
			floaterp->close(app_quitting);
		}
	}
}

bool LLFloaterView::allChildrenClosed()
{
	// See if there are any visible floaters (some floaters "close" by setting
	// themselves invisible)
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp && !floaterp->isDead() && floaterp->getVisible() &&
			floaterp->isCloseable())
		{
			return false;
		}
	}
	return true;
}

void LLFloaterView::refresh()
{
	// Constrain children to be entirely on the screen
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp && floaterp->getVisible())
		{
			// Minimized floaters are kept fully onscreen
			adjustToFitScreen(floaterp, !floaterp->isMinimized());
		}
	}
}

void LLFloaterView::adjustToFitScreen(LLFloater* floater,
									  bool allow_partial_outside)
{
	if (floater->getParent() != this)
	{
		// Floater is hosted elsewhere, so ignore
		return;
	}

	S32 screen_width = getSnapRect().getWidth();
	S32 screen_height = getSnapRect().getHeight();
	// Convert to local coordinate frame
	LLRect snap_rect_local = getLocalSnapRect();

	if (floater->isResizable())
	{
		LLRect view_rect = floater->getRect();
		S32 old_width = view_rect.getWidth();
		S32 old_height = view_rect.getHeight();
		S32 min_width;
		S32 min_height;
		floater->getResizeLimits(&min_width, &min_height);

		// Make sure floater is not already smaller than its min height/width
		S32 new_width = llmax(min_width, old_width);
		S32 new_height = llmax(min_height, old_height);

		if (new_width > screen_width || new_height > screen_height)
		{
			// We have to make this window able to fit on screen
			new_width = llmin(new_width, screen_width);
			new_height = llmin(new_height, screen_height);

			// ...while respecting minimum width/height
			new_width = llmax(new_width, min_width);
			new_height = llmax(new_height, min_height);

			floater->reshape(new_width, new_height);
			if (floater->followsRight())
			{
				floater->translate(old_width - new_width, 0);
			}

			if (floater->followsTop())
			{
				floater->translate(0, old_height - new_height);
			}
		}
	}

	// Move window fully onscreen
	if (floater->translateIntoRect(snap_rect_local, allow_partial_outside))
	{
		floater->clearSnapTarget();
	}
}

void LLFloaterView::draw()
{
	refresh();

	// Hide focused floater if in cycle mode, so that it can be drawn on top
	LLFloater* floaterp = mFocusCycleMode ? getFocusedFloater() : NULL;
	if (floaterp)
	{
		for (child_list_const_iter_t it = getChildList()->begin(),
									 end = getChildList()->end();
			 it != end; ++it)
		{
			if (*it != floaterp)
			{
				drawChild(*it);
			}
		}

		drawChild(floaterp, -TABBED_FLOATER_OFFSET, TABBED_FLOATER_OFFSET);
	}
	else
	{
		LLView::draw();
	}
}

LLRect LLFloaterView::getSnapRect() const
{
	LLRect snap_rect = getRect();
	snap_rect.mBottom += mSnapOffsetBottom;

	return snap_rect;
}

LLFloater* LLFloaterView::getFocusedFloater()
{
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp && floaterp->hasFocus())
		{
			return floaterp;
		}
	}

	return NULL;
}

LLFloater* LLFloaterView::getFrontmost()
{
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp && floaterp->getVisible() && !floaterp->isDead())
		{
			return floaterp;
		}
	}

	return NULL;
}

LLFloater* LLFloaterView::getBackmost()
{
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp && floaterp->getVisible())
		{
			return floaterp;
		}
	}

	return NULL;
}

void LLFloaterView::syncFloaterTabOrder()
{
	// bring focused floater to front
	for (child_list_const_reverse_iter_t rit = getChildList()->rbegin(),
										 rend = getChildList()->rend();
		 rit != rend; ++rit)
	{
		LLView* viewp = *rit;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp && gFocusMgr.childHasKeyboardFocus(floaterp))
		{
			bringToFront(floaterp, false);
			break;
		}
	}

	// Then sync draw order to tab order
	for (child_list_const_reverse_iter_t rit = getChildList()->rbegin(),
										 rend = getChildList()->rend();
		 rit != rend; ++rit)
	{
		LLView* viewp = *rit;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp)
		{
			moveChildToFrontOfTabGroup(floaterp);
		}
	}
}

LLFloater*	LLFloaterView::getParentFloater(LLView* viewp)
{
	LLView* parentp = viewp->getParent();
	while (parentp && parentp != this)
	{
		viewp = parentp;
		parentp = parentp->getParent();
	}

	if (parentp == this)
	{
		return viewp->asFloater();
	}

	return NULL;
}

S32 LLFloaterView::getZOrder(LLFloater* child)
{
	S32 rv = 0;
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		if (*it == child)
		{
			break;
		}
		++rv;
	}
	return rv;
}

void LLFloaterView::pushVisibleAll(bool visible, const skip_list_t& skip_list)
{
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* view = *it;
		if (skip_list.find(view) == skip_list.end())
		{
			view->pushVisible(visible);
		}
	}
}

void LLFloaterView::popVisibleAll(const skip_list_t& skip_list)
{
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* view = *it;
		if (skip_list.find(view) == skip_list.end())
		{
			view->popVisible();
		}
	}
}

void LLFloaterView::fitAllToScreen()
{
	for (child_list_const_iter_t it = getChildList()->begin(),
								 end = getChildList()->end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;

		LLFloater* floaterp = viewp->asFloater();
		if (floaterp && floaterp->getVisible() && !floaterp->isDead())
		{
			adjustToFitScreen(floaterp);
		}
	}
}

//
// LLMultiFloater
//

LLMultiFloater::LLMultiFloater()
:	mTabContainer(NULL),
	mTabPos(LLTabContainer::TOP),
	mAutoResize(true),
	mOrigMinWidth(0),
	mOrigMinHeight(0)
{
}

LLMultiFloater::LLMultiFloater(LLTabContainer::TabPosition tab_pos)
:	mTabContainer(NULL),
	mTabPos(tab_pos),
	mAutoResize(true),
	mOrigMinWidth(0),
	mOrigMinHeight(0)
{
}

LLMultiFloater::LLMultiFloater(const std::string& name)
:	LLFloater(name),
	mTabContainer(NULL),
	mTabPos(LLTabContainer::TOP),
	mAutoResize(false),
	mOrigMinWidth(0),
	mOrigMinHeight(0)
{
}

LLMultiFloater::LLMultiFloater(const std::string& name,
							   const LLRect& rect,
							   LLTabContainer::TabPosition tab_pos,
							   bool auto_resize)
:	LLFloater(name, rect, name),
	mTabContainer(NULL),
	mTabPos(LLTabContainer::TOP),
	mAutoResize(auto_resize),
	mOrigMinWidth(0),
	mOrigMinHeight(0)
{
	mTabContainer =
		new LLTabContainer("Preview Tabs",
						   LLRect(LLPANEL_BORDER_WIDTH,
								  getRect().getHeight() -
								  LLFLOATER_HEADER_SIZE,
								  getRect().getWidth() - LLPANEL_BORDER_WIDTH,
								  0),
						   mTabPos, false, false);
	mTabContainer->setFollowsAll();
	if (isResizable())
	{
		mTabContainer->setRightTabBtnOffset(RESIZE_HANDLE_WIDTH);
	}

	addChild(mTabContainer);
}

LLMultiFloater::LLMultiFloater(const std::string& name,
							   const std::string& rect_control,
							   LLTabContainer::TabPosition tab_pos,
							   bool auto_resize)
:	LLFloater(name, rect_control, name),
	mTabContainer(NULL),
	mTabPos(tab_pos),
	mAutoResize(auto_resize),
	mOrigMinWidth(0),
	mOrigMinHeight(0)
{
	mTabContainer =
		new LLTabContainer("Preview Tabs",
						   LLRect(LLPANEL_BORDER_WIDTH,
								  getRect().getHeight() -
								  LLFLOATER_HEADER_SIZE,
								  getRect().getWidth() - LLPANEL_BORDER_WIDTH,
								  0),
						   mTabPos, false, false);
	mTabContainer->setFollowsAll();
	if (isResizable() && mTabPos == LLTabContainer::BOTTOM)
	{
		mTabContainer->setRightTabBtnOffset(RESIZE_HANDLE_WIDTH);
	}

	addChild(mTabContainer);
}

//virtual
LLXMLNodePtr LLMultiFloater::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLFloater::getXML();

	node->setName(LL_MULTI_FLOATER_TAG);

	return node;
}

//virtual
void LLMultiFloater::open()
{
	if (mTabContainer->getTabCount() > 0)
	{
		LLFloater::open();
	}
	else
	{
		// For now, do not allow multifloaters without any child floaters
		close();
	}
}

//virtual
void LLMultiFloater::onClose(bool app_quitting)
{
	if (closeAllFloaters())
	{
		LLFloater::onClose(app_quitting);
	}
}

//virtual
void LLMultiFloater::draw()
{
	if (mTabContainer->getTabCount() == 0)
	{
		// RN: could this potentially crash in draw hierarchy ?
		close();
	}
	else
	{
		for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
		{
			LLFloater* floaterp = (LLFloater*)mTabContainer->getPanelByIndex(i);
			if (floaterp->getShortTitle() != mTabContainer->getPanelTitle(i))
			{
				mTabContainer->setPanelTitle(i, floaterp->getShortTitle());
			}
		}
		LLFloater::draw();
	}
}

bool LLMultiFloater::closeAllFloaters()
{
	S32	tab_to_close = 0;
	S32	lastTabCount = mTabContainer->getTabCount();
	while (tab_to_close < mTabContainer->getTabCount())
	{
		LLFloater* first_floater =
			(LLFloater*)mTabContainer->getPanelByIndex(tab_to_close);
		first_floater->close();
		if (lastTabCount == mTabContainer->getTabCount())
		{
			// Tab did not actually close, possibly due to a pending save
			// confirmation dialog, so try and close the next one in the list
			++tab_to_close;
		}
		else
		{
			// Tab closed ok.
			lastTabCount = mTabContainer->getTabCount();
		}
	}
	if (mTabContainer->getTabCount() != 0)
	{
		// Could not close all the tabs (pending save dialog ?)
		return false;
	}

	return true; // else all tabs were successfully closed...
}

void LLMultiFloater::growToFit(S32 content_width, S32 content_height)
{
	S32 new_width = llmax(getRect().getWidth(),
						  content_width + LLPANEL_BORDER_WIDTH * 2);
	S32 new_height = llmax(getRect().getHeight(),
						   content_height + LLFLOATER_HEADER_SIZE +
						   TABCNTR_HEADER_HEIGHT);

    if (isMinimized())
    {
        LLRect newrect;
        newrect.setLeftTopAndSize(getExpandedRect().mLeft,
								  getExpandedRect().mTop,
								  new_width, new_height);
        setExpandedRect(newrect);
    }
	else
	{
		S32 old_height = getRect().getHeight();
		reshape(new_width, new_height);
		// Keep top left corner in same position
		translate(0, old_height - new_height);
	}
}

// Adds the LLFloater pointed to by floaterp to this. If floaterp is already
// hosted by this, then it is re-added to get new titles, etc.
// If select_added_floater is true, the LLFloater pointed to by floaterp will
// become the selected tab in this
void LLMultiFloater::addFloater(LLFloater* floaterp, bool select_added_floater,
								LLTabContainer::eInsertionPoint insertion_pt)
{
	if (!floaterp)
	{
		return;
	}

	if (!mTabContainer)
	{
		llerrs << "Tab Container used without having been initialized."
			   << llendl;
		return;
	}

	if (floaterp->getHost() == this)
	{
		// Already hosted by me, remove do this so we get updated title, etc.
		mFloaterDataMap.erase(floaterp->getHandle());
		mTabContainer->removeTabPanel(floaterp);
	}
	else if (floaterp->getHost())
	{
		// floaterp is hosted by somebody else and this is adding it, so remove
		// it from its old host
		floaterp->getHost()->removeFloater(floaterp);
	}
	else if (gFloaterViewp && floaterp->getParent() == gFloaterViewp)
	{
		// Re-host preview floater as child panel
		gFloaterViewp->removeChild(floaterp);
	}

	// Store original configuration
	LLFloaterData floater_data;
	floater_data.mWidth = floaterp->getRect().getWidth();
	floater_data.mHeight = floaterp->getRect().getHeight();
	floater_data.mCanMinimize = floaterp->isMinimizeable();
	floater_data.mCanResize = floaterp->isResizable();

	// Remove minimize and close buttons
	floaterp->setCanMinimize(false);
	floaterp->setCanResize(false);
	floaterp->setCanDrag(false);
	floaterp->storeRectControl();
	// Avoid double rendering of floater background (makes it more opaque)
	floaterp->setBackgroundVisible(false);

	if (mAutoResize)
	{
		growToFit(floater_data.mWidth, floater_data.mHeight);
	}

	// Add the panel, add it to proper maps
	mTabContainer->addTabPanel(floaterp, floaterp->getShortTitle(), false,
							   onTabSelected, this, 0, false, insertion_pt);
	mFloaterDataMap[floaterp->getHandle()] = floater_data;

	updateResizeLimits();

	if (select_added_floater)
	{
		mTabContainer->selectTabPanel(floaterp);
	}
	else
	{
		// Reassert visible tab (hiding new floater if necessary)
		mTabContainer->selectTab(mTabContainer->getCurrentPanelIndex());
	}

	floaterp->setHost(this);
	if (isMinimized())
	{
		floaterp->setVisible(false);
	}
}

// If the LLFloater pointed to by floaterp is hosted by this, then its tab is
// selected and returns true. Otherwise returns false.
bool LLMultiFloater::selectFloater(LLFloater* floaterp)
{
	return mTabContainer->selectTabPanel(floaterp);
}

//virtual
void LLMultiFloater::selectNextFloater()
{
	mTabContainer->selectNextTab();
}

//virtual
void LLMultiFloater::selectPrevFloater()
{
	mTabContainer->selectPrevTab();
}

void LLMultiFloater::showFloater(LLFloater* floaterp)
{
	// We won't select a panel that already is selected it is hard to do this
	// internally to tab container as tab selection is handled via index and
	// the tab at a given index might have changed
	if (floaterp != mTabContainer->getCurrentPanel() &&
		!mTabContainer->selectTabPanel(floaterp))
	{
		addFloater(floaterp, true);
	}
}

void LLMultiFloater::removeFloater(LLFloater* floaterp)
{
	if (floaterp->getHost() != this)
		return;

	floater_data_map_t::iterator found_data_it =
		mFloaterDataMap.find(floaterp->getHandle());
	if (found_data_it != mFloaterDataMap.end())
	{
		LLFloaterData& floater_data = found_data_it->second;
		floaterp->setCanMinimize(floater_data.mCanMinimize);
		if (!floater_data.mCanResize)
		{
			// Restore original size
			floaterp->reshape(floater_data.mWidth, floater_data.mHeight);
		}
		floaterp->setCanResize(floater_data.mCanResize);
		mFloaterDataMap.erase(found_data_it);
	}
	mTabContainer->removeTabPanel(floaterp);
	floaterp->setBackgroundVisible(true);
	floaterp->setCanDrag(true);
	floaterp->setHost(NULL);
	floaterp->applyRectControl();

	updateResizeLimits();

	tabOpen((LLFloater*)mTabContainer->getCurrentPanel(), false);
}

void LLMultiFloater::tabOpen(LLFloater* opened_floater, bool from_click)
{
	// Default implementation does nothing
}

void LLMultiFloater::tabClose()
{
	if (mTabContainer && mTabContainer->getTabCount() == 0)
	{
		// No more children, close myself
		close();
	}
}

void LLMultiFloater::setVisible(bool visible)
{
	// *FIX: should not have to do this, fix adding to minimized multifloater
	LLFloater::setVisible(visible);

	if (mTabContainer)
	{
		LLPanel* cur_floaterp = mTabContainer->getCurrentPanel();

		if (cur_floaterp)
		{
			cur_floaterp->setVisible(visible);
		}

		// if no tab selected, and we're being shown,
		// select last tab to be added
		if (visible && !cur_floaterp)
		{
			mTabContainer->selectLastTab();
		}
	}
}

bool LLMultiFloater::handleKeyHere(KEY key, MASK mask)
{
	if (key == 'W' && mask == MASK_CONTROL)
	{
		LLFloater* floaterp = getActiveFloater();
		// Is it user closeable and is system closeable ?
		if (floaterp && floaterp->canClose() && floaterp->isCloseable())
		{
			floaterp->close();
		}
		return true;
	}

	return LLFloater::handleKeyHere(key, mask);
}

LLFloater* LLMultiFloater::getActiveFloater()
{
	LLView* viewp = mTabContainer->getCurrentPanel();
	if (viewp)
	{
		return viewp->asFloater();
	}
	return NULL;
}

S32	LLMultiFloater::getFloaterCount()
{
	return mTabContainer->getTabCount();
}

// Returns true if the LLFloater pointed to by floaterp	is currently in a
// flashing state and is hosted by this.
bool LLMultiFloater::isFloaterFlashing(LLFloater* floaterp)
{
	if (floaterp && floaterp->getHost() == this)
	{
		return mTabContainer->getTabPanelFlashing(floaterp);
	}

	return false;
}

// Sets the current flashing state of the LLFloater pointed to by floaterp to
// be the bool flashing if the LLFloater pointed to by floaterp is hosted by
// this.
void LLMultiFloater::setFloaterFlashing(LLFloater* floaterp, bool flashing)
{
	if (floaterp && floaterp->getHost() == this)
	{
		mTabContainer->setTabPanelFlashing(floaterp, flashing);
	}
}

//static
void LLMultiFloater::onTabSelected(void* userdata, bool from_click)
{
	LLMultiFloater* floaterp = (LLMultiFloater*)userdata;

	floaterp->tabOpen((LLFloater*)floaterp->mTabContainer->getCurrentPanel(),
					  from_click);
}

void LLMultiFloater::setCanResize(bool can_resize)
{
	LLFloater::setCanResize(can_resize);
	if (isResizable() &&
		mTabContainer->getTabPosition() == LLTabContainer::BOTTOM)
	{
		mTabContainer->setRightTabBtnOffset(RESIZE_HANDLE_WIDTH);
	}
	else
	{
		mTabContainer->setRightTabBtnOffset(0);
	}
}

bool LLMultiFloater::postBuild()
{
	// Remember any original xml minimum size
	getResizeLimits(&mOrigMinWidth, &mOrigMinHeight);

	if (mTabContainer)
	{
		return true;
	}

	mTabContainer = getChild<LLTabContainer>("Preview Tabs");
	return true;
}

void LLMultiFloater::updateResizeLimits()
{
	// Initialize minimum size constraint to the original xml values.
	S32 new_min_width = mOrigMinWidth;
	S32 new_min_height = mOrigMinHeight;
	// Possibly increase minimum size constraint due to children's minimums.
	for (S32 tab_idx = 0; tab_idx < mTabContainer->getTabCount(); ++tab_idx)
	{
		LLFloater* floaterp =
			(LLFloater*)mTabContainer->getPanelByIndex(tab_idx);
		if (floaterp)
		{
			new_min_width = llmax(new_min_width,
								  floaterp->getMinWidth() +
								  LLPANEL_BORDER_WIDTH * 2);
			new_min_height = llmax(new_min_height,
								   floaterp->getMinHeight() +
								   LLFLOATER_HEADER_SIZE +
								   TABCNTR_HEADER_HEIGHT);
		}
	}
	setResizeLimits(new_min_width, new_min_height);

	S32 cur_height = getRect().getHeight();
	S32 new_width = llmax(getRect().getWidth(), new_min_width);
	S32 new_height = llmax(getRect().getHeight(), new_min_height);

	if (isMinimized())
	{
		const LLRect& expanded = getExpandedRect();
		LLRect newrect;
		newrect.setLeftTopAndSize(expanded.mLeft, expanded.mTop,
								  llmax(expanded.getWidth(), new_width),
								  llmax(expanded.getHeight(), new_height));
		setExpandedRect(newrect);
	}
	else
	{
		reshape(new_width, new_height);

		// Make sure upper left corner doesn't move
		translate(0, cur_height - getRect().getHeight());

		// Make sure this window is visible on screen when it has been modified
		// (tab added, etc)
		if (gFloaterViewp)	// Paranoia
		{
			gFloaterViewp->adjustToFitScreen(this, true);
		}
	}
}

//virtual
LLXMLNodePtr LLFloater::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLPanel::getXML();

	node->setName(LL_FLOATER_TAG);

	node->createChild("title", true)->setStringValue(getCurrentTitle());

	node->createChild("can_resize", true)->setBoolValue(isResizable());

	node->createChild("can_minimize", true)->setBoolValue(isMinimizeable());

	node->createChild("can_close", true)->setBoolValue(isCloseable());

	node->createChild("can_drag_on_left", true)->setBoolValue(isDragOnLeft());

	node->createChild("min_width", true)->setIntValue(getMinWidth());

	node->createChild("min_height", true)->setIntValue(getMinHeight());

	node->createChild("can_tear_off", true)->setBoolValue(mCanTearOff);

	return node;
}

//static
LLView* LLFloater::fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory)
{
	std::string name = LL_FLOATER_TAG;
	node->getAttributeString("name", name);

	LLFloater* floaterp = new LLFloater(name);

	std::string filename;
	node->getAttributeString("filename", filename);

	if (filename.empty())
	{
		// Load from node
		floaterp->initFloaterXML(node, parent, factory);
	}
	else
	{
		// Load from file
		factory->buildFloater(floaterp, filename);
	}

	return floaterp;
}

void LLFloater::initFloaterXML(LLXMLNodePtr node, LLView* parent,
							   LLUICtrlFactory* factory, bool open_it)
{
	std::string name = getName();
	node->getAttributeString("name", name);
	std::string title = getCurrentTitle();
	node->getAttributeString("title", title);
	std::string short_title = getShortTitle();
	node->getAttributeString("short_title", short_title);
	std::string rect_control;
	node->getAttributeString("rect_control", rect_control);
	bool resizable = isResizable();
	node->getAttributeBool("can_resize", resizable);
	bool minimizable = isMinimizeable();
	node->getAttributeBool("can_minimize", minimizable);
	bool close_btn = isCloseable();
	node->getAttributeBool("can_close", close_btn);
	bool drag_on_left = isDragOnLeft();
	node->getAttributeBool("can_drag_on_left", drag_on_left);
	S32 min_width = getMinWidth();
	node->getAttributeS32("min_width", min_width);
	S32 min_height = getMinHeight();
	node->getAttributeS32("min_height", min_height);

	if (!rect_control.empty())
	{
		setRectControl(rect_control);
	}

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	setRect(rect);
	setName(name);

	initFloater(title, resizable, min_width, min_height, drag_on_left,
				minimizable, close_btn);

	mTitle = title;
	applyTitle();

	setShortTitle(short_title);

	bool can_tear_off;
	if (node->getAttributeBool("can_tear_off", can_tear_off))
	{
		setCanTearOff(can_tear_off);
	}

	initFromXML(node, parent);

	LLMultiFloater* last_host = sHostp;
	bool is_multi_floater = node->hasName("multi_floater");
	if (is_multi_floater)
	{
		sHostp = (LLMultiFloater*)this;
	}

	initChildrenXML(node, factory);

	if (is_multi_floater)
	{
		sHostp = last_host;
	}

	if (!postBuild())
	{
		llerrs << "Failed to construct floater " << name << llendl;
	}

	applyRectControl();

	if (open_it)
	{
		open();
	}

	moveResizeHandlesToFront();
}
