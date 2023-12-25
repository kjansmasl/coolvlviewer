/**
 * @file llwindow.cpp
 * @brief Basic graphical window class
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

#include "linden_common.h"

#if LL_LINUX
#include "llwindowsdl.h"
#elif LL_WINDOWS
#include "llwindowwin32.h"
#elif LL_DARWIN
#include "llwindowmacosx.h"
#endif

#include "llkeyboard.h"

// Globals

LLWindow* gWindowp = NULL;
LLSplashScreen* gSplashScreenp = NULL;

bool gDebugClicks = false;
bool gDebugWindowProc = false;
bool gHiDPISupport = false;

const std::string gURLProtocolWhitelist[] =
{
	"file:",
	"http:",
	"https:",
	"ftp:",
	"data:"
};
const S32 gURLProtocolWhitelistCount = LL_ARRAY_SIZE(gURLProtocolWhitelist);

// Static instance for default callbacks
LLWindowCallbacks LLWindow::sDefaultCallbacks;

// Helper function
S32 OSMessageBox(const std::string& text, const std::string& caption, U32 type)
{
	// Properly hide the splash screen when displaying the message box
	bool was_visible = LLSplashScreen::isVisible();
	if (was_visible)
	{
		LLSplashScreen::hide();
	}

	S32 result = 0;
#if LL_DARWIN
	result = OSMessageBoxMacOSX(text, caption, type);
#elif LL_LINUX
	result = OSMessageBoxSDL(text, caption, type);
#elif LL_WINDOWS
	result = OSMessageBoxWin32(text, caption, type);
#else
# error("OSMessageBox not implemented for this platform !")
#endif

	if (was_visible)
	{
		LLSplashScreen::show();
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
// LLWindow class
///////////////////////////////////////////////////////////////////////////////

//static
void LLWindow::createWindow(const std::string& title, S32 x, S32 y,
							U32 width, U32 height, U32 flags, bool fullscreen,
							bool disable_vsync, U32 fsaa_samples)
{
	llassert_always(gWindowp == NULL);

#if LL_LINUX
	gWindowp = new LLWindowSDL(title, x, y, width, height, flags,
							   fullscreen, disable_vsync, fsaa_samples);
#elif LL_WINDOWS
	gWindowp = new LLWindowWin32(title, x, y, width, height, flags, fullscreen,
								 disable_vsync, fsaa_samples);
#elif LL_DARWIN
	gWindowp = new LLWindowMacOSX(title, flags, fullscreen, disable_vsync,
								  fsaa_samples);
#endif

	if (!gWindowp->isValid())
	{
		llwarns << "Invalid window. Destroying it." << llendl;
		delete gWindowp;
		gWindowp = NULL;
	}
}

//static
void LLWindow::destroyWindow()
{
	if (gWindowp)
	{
		gWindowp->close();
		delete gWindowp;
		gWindowp = NULL;
	}
}

LLWindow::LLWindow(bool fullscreen, U32 flags)
:	mCallbacks(&sDefaultCallbacks),
	mPostQuit(true),
	mFullscreen(fullscreen),
	mFullscreenWidth(0),
	mFullscreenHeight(0),
	mFullscreenBits(0),
	mFullscreenRefresh(0),
	mOverrideAspectRatio(0.f),
	mCurrentGamma(1.f),
	mSupportedResolutions(NULL),
	mNumSupportedResolutions(0),
	mCurrentCursor(UI_CURSOR_ARROW),
	mCursorFrozen(false),
	mCursorHidden(false),
#if LL_DARWIN
	mNextCursor(UI_CURSOR_ARROW),
#endif
	mBusyCount(0),
	mIsMouseClipping(false),
	mSwapMethod(SWAP_METHOD_UNDEFINED),
	mHideCursorPermanent(false),
	mFlags(flags),
	mHighSurrogate(0)
{
}

void LLWindow::decBusyCount()
{
	if (mBusyCount > 0)
	{
		--mBusyCount;
	}
}

void LLWindow::setCallbacks(LLWindowCallbacks* callbacks)
{
	mCallbacks = callbacks;
	if (gKeyboardp)
	{
		gKeyboardp->setCallbacks(callbacks);
	}
}

//static
std::vector<std::string> LLWindow::getDynamicFallbackFontList()
{
#if LL_WINDOWS
	return LLWindowWin32::getDynamicFallbackFontList();
#elif LL_DARWIN
	return LLWindowMacOSX::getDynamicFallbackFontList();
#elif LL_LINUX
	return LLWindowSDL::getDynamicFallbackFontList();
#else
	return std::vector<std::string>();
#endif
}

#define UTF16_IS_HIGH_SURROGATE(U) ((U16)((U) - 0xD800) < 0x0400)
#define UTF16_IS_LOW_SURROGATE(U)  ((U16)((U) - 0xDC00) < 0x0400)
#define UTF16_SURROGATE_PAIR_TO_UTF32(H,L) (((H) << 10) + (L) - (0xD800 << 10) - 0xDC00 + 0x00010000)

void LLWindow::handleUnicodeUTF16(U16 utf16, MASK mask)
{
	LL_DEBUGS("Window") << "UTF16 key = " << std::hex << (U32)utf16 << std::dec
						<< " - mask = " << mask << LL_ENDL;

	// Note that we could discard unpaired surrogates, but I am following the
	// Unicode Consortium's recommendation here, that is, to preserve those
	// unpaired surrogates in UTF-32 values. _To_preserve_ means to pass to the
	// callback in our context.

	if (mHighSurrogate == 0)
	{
		if (UTF16_IS_HIGH_SURROGATE(utf16))
		{
			mHighSurrogate = utf16;
		}
		else
		{
			mCallbacks->handleUnicodeChar(utf16, mask);
		}
	}
	else if (UTF16_IS_LOW_SURROGATE(utf16))
	{
		// A legal surrogate pair.
		mCallbacks->handleUnicodeChar(UTF16_SURROGATE_PAIR_TO_UTF32(mHighSurrogate,
																	utf16),
									  mask);
		mHighSurrogate = 0;
	}
	else if (UTF16_IS_HIGH_SURROGATE(utf16))
	{
		// Two consecutive high surrogates.
		mCallbacks->handleUnicodeChar(mHighSurrogate, mask);
		mHighSurrogate = utf16;
	}
	else
	{
		// A non-low-surrogate preceeded by a high surrogate.
		mCallbacks->handleUnicodeChar(mHighSurrogate, mask);
		mHighSurrogate = 0;
		mCallbacks->handleUnicodeChar(utf16, mask);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLSplashScreen class
///////////////////////////////////////////////////////////////////////////////

//static
bool LLSplashScreen::isVisible()
{
	return gSplashScreenp != NULL;
}

//static
void LLSplashScreen::show()
{
	if (!gSplashScreenp)
	{
#if LL_DARWIN
		gSplashScreenp = new LLSplashScreenMacOSX;
#elif LL_LINUX
		gSplashScreenp = new LLSplashScreenSDL;
#elif LL_WINDOWS
		gSplashScreenp = new LLSplashScreenWin32;
#endif
		gSplashScreenp->showImpl();
	}
}

//static
void LLSplashScreen::update(const std::string& str)
{
	LLSplashScreen::show();
	if (gSplashScreenp)
	{
		gSplashScreenp->updateImpl(str);
	}
}

//static
void LLSplashScreen::hide()
{
	if (gSplashScreenp)
	{
		gSplashScreenp->hideImpl();
	}
	delete gSplashScreenp;
	gSplashScreenp = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// LLWindowCallbacks class
///////////////////////////////////////////////////////////////////////////////

//virtual
bool LLWindowCallbacks::handleTranslatedKeyDown(KEY, MASK, bool)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleTranslatedKeyUp(KEY, MASK)
{
	return false;
}

//virtual
void LLWindowCallbacks::handleScanKey(KEY, bool, bool, bool)
{
}

//virtual
bool LLWindowCallbacks::handleUnicodeChar(llwchar, MASK)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleMouseDown(LLWindow*, const LLCoordGL, MASK)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleMouseUp(LLWindow*, const LLCoordGL, MASK)
{
	return false;
}

//virtual
void LLWindowCallbacks::handleMouseLeave(LLWindow*)
{
	return;
}

//virtual
bool LLWindowCallbacks::handleCloseRequest(LLWindow*)
{
	// Allow the window to close
	return true;
}

//virtual
void LLWindowCallbacks::handleQuit(LLWindow* window)
{
	if (window == gWindowp)
	{
		LLWindow::destroyWindow();
	}
	else
	{
		llerrs << "Invalid window !" << llendl;
	}
}

//virtual
bool LLWindowCallbacks::handleRightMouseDown(LLWindow*, const LLCoordGL, MASK)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleRightMouseUp(LLWindow*, const LLCoordGL, MASK)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleMiddleMouseDown(LLWindow*, const LLCoordGL, MASK)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleMiddleMouseUp(LLWindow*, const LLCoordGL, MASK)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleActivate(LLWindow*, bool)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleActivateApp(LLWindow*, bool)
{
	return false;
}

//virtual
void LLWindowCallbacks::handleMouseMove(LLWindow*, const LLCoordGL, MASK)
{
}

#if LL_DARWIN
//virtual
void LLWindowCallbacks::handleMouseDragged(LLWindow*, const LLCoordGL, MASK)
{
}
#endif

//virtual
void LLWindowCallbacks::handleScrollWheel(LLWindow*, S32)
{
}

//virtual
void LLWindowCallbacks::handleResize(LLWindow*, S32, S32)
{
}

//virtual
void LLWindowCallbacks::handleFocus(LLWindow*)
{
}

//virtual
void LLWindowCallbacks::handleFocusLost(LLWindow*)
{
}

//virtual
void LLWindowCallbacks::handleMenuSelect(LLWindow*, S32)
{
}

//virtual
bool LLWindowCallbacks::handlePaint(LLWindow*, S32, S32, S32, S32)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleDoubleClick(LLWindow*, const LLCoordGL, MASK)
{
	return false;
}

//virtual
void LLWindowCallbacks::handleWindowBlock(LLWindow*)
{
}

//virtual
void LLWindowCallbacks::handleWindowUnblock(LLWindow*)
{
}

//virtual
void LLWindowCallbacks::handleDataCopy(LLWindow*, S32, void*)
{
}

//virtual
bool LLWindowCallbacks::handleTimerEvent(LLWindow*)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleDeviceChange(LLWindow*)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleDPIChanged(LLWindow*, F32, S32, S32)
{
	return false;
}

//virtual
bool LLWindowCallbacks::handleWindowDidChangeScreen(LLWindow*)
{
	return false;
}
