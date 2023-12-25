/**
 * @file llwindowmacosx.cpp
 * @brief Platform-dependent implementation of llwindow
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

#include <OpenGL/OpenGL.h>
#include <CoreServices/CoreServices.h>

#include "llwindowmacosx.h"

#include "indra_constants.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "llkeyboardmacosx.h"
#include "llpreeditor.h"
#include "llrender.h"				// For LLRender::sGLCoreProfile
#include "llstring.h"
#include "llwindowmacosx-objc.h"

constexpr S32 BITS_PER_PIXEL = 32;
constexpr S32 MAX_NUM_RESOLUTIONS = 32;

namespace
{
	NSKeyEventRef mRawKeyEvent = NULL;
}

//
// LLWindowMacOSX
//

bool LLWindowMacOSX::sUseMultGL = false;

// Cross-platform bits:

bool check_for_card(const char* RENDERER, const char* bad_card)
{
	if (!strnicmp(RENDERER, bad_card, strlen(bad_card)))
	{
		std::string buffer = llformat(
			"Your video card appears to be a %s, which Second Life does not support.\n"
			"\n"
			"Second Life requires a video card with 32 Mb of memory or more, as well as\n"
			"multitexture support.  We explicitly support nVidia GeForce 2 or better, \n"
			"and ATI Radeon 8500 or better.\n"
			"\n"
			"If you own a supported card and continue to receive this message, try \n"
			"updating to the latest video card drivers. Otherwise look in the\n"
			"secondlife.com support section or e-mail technical support\n"
			"\n"
			"You can try to run Second Life, but it will probably crash or run\n"
			"very slowly.  Try anyway?",
			bad_card);
		S32 button = OSMessageBox(buffer.c_str(), "Unsupported video card",
								  OSMB_YESNO);
		return button != OSBTN_YES;
	}

	return false;
}

// Get a long value from a dictionary
static long getDictLong(CFDictionaryRef refDict, CFStringRef key)
{
	CFNumberRef number_value = (CFNumberRef)CFDictionaryGetValue(refDict, key);
	if (!number_value)
	{
		// if can't get a number for the dictionary
		return -1;  // fail
	}

	long int_value;
	if (!CFNumberGetValue(number_value, kCFNumberLongType, &int_value))
	{
		// or if cant convert it
		return -1; // fail
	}

	return int_value; // otherwise return the long value
}


// *HACK: on the Mac, to put up an OS dialog in full screen mode, we must first
// switch OUT of full screen mode. The proper way to do this is to bracket the
// dialog with calls to beforeDialog() and afterDialog(), but these require a
// pointer to the LLWindowMacOSX object. Stash it here and maintain in the
// constructor and destructor. This assumes that there will be only one object
// of this class at any time, which is currently the case.
static LLWindowMacOSX* sWindowImplementation = NULL;

LLWindowMacOSX::LLWindowMacOSX(const std::string& title, U32 flags,
							   bool fullscreen, bool disable_vsync,
							   U32 fsaa_samples)
:	LLWindow(fullscreen, flags)
{
	setupCocoa();

	// Initialize the keyboard
	gKeyboardp = new LLKeyboardMacOSX();

	mWindow = NULL;
	mContext = NULL;
	mPixelFormat = NULL;
	mDisplay = CGMainDisplayID();
	mSimulatedRightClick = false;
	mLastModifiers = 0;
	mCursorDecoupled = false;
	mCursorLastEventDeltaX = 0;
	mCursorLastEventDeltaY = 0;
	mCursorIgnoreNextDelta = false;
	mMinimized = false;
	mLanguageTextInputAllowed = false;
	mPreeditor = NULL;
	mFSAASamples = fsaa_samples;
	mForceRebuild = false;

	// Get the original aspect ratio of the main device.
	mOriginalAspectRatio = (double)CGDisplayPixelsWide(mDisplay) /
						   (double)CGDisplayPixelsHigh(mDisplay);

	// Stash the window title
	mWindowTitle = title;

	// Stash an object pointer for OSMessageBox()
	sWindowImplementation = this;

	// Create the GL context and set it up for windowed or fullscreen, as
	// appropriate.
	if (createContext(fullscreen, disable_vsync))
	{
		if (mWindow)
		{
			makeWindowOrderFront(mWindow);
		}

		if (!gGLManager.initGL())
		{
			setupFailure(
				"Second Life is unable to run because your video card drivers\n"
				"are out of date or unsupported. Please make sure you have\n"
				"the latest video card drivers installed.\n"
				"If you continue to receive this message, contact customer service.");
			return;
		}

		// Start with arrow cursor
		initCursors();
		setCursor(UI_CURSOR_ARROW);

		allowLanguageTextInput(NULL, false);
	}

	stop_glerror();
}

void LLWindowMacOSX::setWindowTitle(const std::string& title)
{
	// Remember the new title, for when we switch context
	mWindowTitle = title;
	setWinTitle(mWindow, title);
}

bool LLWindowMacOSX::createContext(bool fullscreen, bool disable_vsync)
{
	mFullscreen = fullscreen;

	if (!mWindow)
	{
		mWindow = getMainAppWindow();
	}

	if (!mContext)
	{
		// Our OpenGL view is already defined within CoolVLViewer.xib.
		// Get the view instead.
		mGLView = createOpenGLView(mWindow, mFSAASamples, !disable_vsync,
								   LLRender::sGLCoreProfile);
		mContext = getCGLContextObj(mGLView);
		gGLManager.mVRAM = getVramSize(mGLView);

		if (!mPixelFormat)
		{
			CGLPixelFormatAttribute attribs[] =
			{
				kCGLPFANoRecovery,
				kCGLPFADoubleBuffer,
				kCGLPFAClosestPolicy,
				kCGLPFAAccelerated,
				kCGLPFAMultisample,
				kCGLPFASampleBuffers, CGLPixelFormatAttribute(mFSAASamples ? 1
																		   : 0),
				kCGLPFASamples, CGLPixelFormatAttribute(mFSAASamples),
				kCGLPFAStencilSize, CGLPixelFormatAttribute(8),
				kCGLPFADepthSize, CGLPixelFormatAttribute(24),
				kCGLPFAAlphaSize, CGLPixelFormatAttribute(8),
				kCGLPFAColorSize, CGLPixelFormatAttribute(24),
				CGLPixelFormatAttribute(0)
			};
			GLint num_formats;
			CGLChoosePixelFormat(attribs, &mPixelFormat, &num_formats);
			if (!mPixelFormat)
			{
				// Try again
				CGLChoosePixelFormat(attribs, &mPixelFormat, &num_formats);
			}
		}
	}

	// This sets up our view to receive text from our non-inline text input
	// window.
	setupInputWindow(mWindow, mGLView);

	if (mContext)
	{
		LL_DEBUGS("Window") << "Setting current context" << LL_ENDL;
		U32 err = CGLSetCurrentContext(mContext);
		if (err != kCGLNoError)
		{
			setupFailure("Cannot activate GL rendering context");
			return false;
		}
	}

	// Disable vertical sync for swap
	GLint frames_per_swap = disable_vsync ? 0 : 1;
	CGLSetParameter(mContext, kCGLCPSwapInterval, &frames_per_swap);

	// Enable multi-threaded OpenGL when configured to do so.
	if (sUseMultGL)
	{
		CGLError cgl_err;
		CGLContextObj ctx = CGLGetCurrentContext();

		cgl_err =  CGLEnable(ctx, kCGLCEMPEngine);

		if (cgl_err != kCGLNoError)
		{
			LL_DEBUGS("GLInit") << "Multi-threaded OpenGL not available."
								<< LL_ENDL;
		}
		else
		{
			LL_DEBUGS("GLInit") << "Multi-threaded OpenGL enabled." << LL_ENDL;
		}
	}

	makeFirstResponder(mWindow, mGLView);

	return true;
}

// We only support OS X 10.7's fullscreen app mode which is literally a full
// screen window that fills a virtual desktop. This makes this method obsolete.
bool LLWindowMacOSX::switchContext(bool, const LLCoordScreen&, bool,
								   const LLCoordScreen* const)
{
	return false;
}

void LLWindowMacOSX::destroyContext()
{
	if (!mContext)
	{
		// We do not have a context
		return;
	}

	// Unhook the GL context from any drawable it may have
	LL_DEBUGS("Window") << "Unhooking drawable" << LL_ENDL;
	CGLSetCurrentContext(NULL);

	// Clean up remaining GL state before blowing away window
	gGLManager.shutdownGL();

	// Clean up the pixel format
	if (mPixelFormat)
	{
		LL_DEBUGS("Window") << "Destroying pixel format" << LL_ENDL;
		CGLDestroyPixelFormat(mPixelFormat);
		mPixelFormat = NULL;
	}

	LL_DEBUGS("Window") << "Destroying context" << LL_ENDL;
	CGLDestroyContext(mContext);
#if 1
	mContext = NULL;
#endif

	// Destroy our LLOpenGLView
	if (mGLView)
	{
		LL_DEBUGS("Window") << "Destroying GL view" << LL_ENDL;
		removeGLView(mGLView);
		mGLView = NULL;
	}

	// Close the window
	if (mWindow)
	{
		LL_DEBUGS("Window") << "Disposing window" << LL_ENDL;
        NSWindowRef dead_window = mWindow;
        mWindow = NULL;
		closeWindow(dead_window);
	}
}

LLWindowMacOSX::~LLWindowMacOSX()
{
	destroyContext();

	if (mSupportedResolutions)
	{
		delete[] mSupportedResolutions;
	}

	sWindowImplementation = NULL;
}

void LLWindowMacOSX::show()
{
}

void LLWindowMacOSX::hide()
{
	setMouseClipping(false);
}

void LLWindowMacOSX::minimize()
{
	setMouseClipping(false);
	showCursor();
}

void LLWindowMacOSX::restore()
{
	show();
}

// Destroys all OS-specific code associated with a window. Usually called from
// LLWindow::destroyWindow()
void LLWindowMacOSX::close()
{
#if 0
	// Is window is already closed ?
	if (!mWindow)
	{
		return;
	}
#endif

	// Make sure cursor is visible and we have not mangled the clipping state.
	setMouseClipping(false);
	showCursor();

	destroyContext();
}

bool LLWindowMacOSX::isValid()
{
	return mFullscreen || mWindow != NULL;
}

bool LLWindowMacOSX::getVisible()
{
	return mFullscreen || mWindow != NULL;
}

bool LLWindowMacOSX::getMinimized()
{
	return mMinimized;
}

void LLWindowMacOSX::gatherInput()
{
	updateCursor();
}

bool LLWindowMacOSX::getPosition(LLCoordScreen* position)
{
	if (mFullscreen)
	{
		position->mX = 0;
		position->mY = 0;
		return true;
	}
	else if (mWindow)
	{
		const CGPoint& pos = getContentViewBoundsPosition(mWindow);
		position->mX = pos.x;
		position->mY = pos.y;
		return true;
	}

	llerrs << "No window and not fullscreen !" << llendl;

	return false;
}

bool LLWindowMacOSX::getSize(LLCoordScreen* size)
{
	if (mFullscreen)
	{
		size->mX = mFullscreenWidth;
		size->mY = mFullscreenHeight;
		return true;
	}
	else if (mWindow)
	{
		const CGSize& sz = gHiDPISupport ? getDeviceContentViewSize(mWindow,
																	mGLView)
										 : getContentViewBoundsSize(mWindow);
		size->mX = sz.width;
		size->mY = sz.height;
		return true;
	}

	llerrs << "No window and not fullscreen !" << llendl;

	return false;
}

bool LLWindowMacOSX::getSize(LLCoordWindow* size)
{
	if (mFullscreen)
	{
		size->mX = mFullscreenWidth;
		size->mY = mFullscreenHeight;
		return true;
	}
	else if (mWindow)
	{
		const CGSize& sz = gHiDPISupport ? getDeviceContentViewSize(mWindow,
																	mGLView)
										 : getContentViewBoundsSize(mWindow);
		size->mX = sz.width;
		size->mY = sz.height;
		return true;
	}

	llerrs << "No window and not fullscreen !" << llendl;

	return false;
}

bool LLWindowMacOSX::setPosition(const LLCoordScreen position)
{
	if (mWindow)
	{
		float pos[2] = { (float)position.mX, (float)position.mY };
		setWindowPos(mWindow, pos);
		return true;
	}

	return false;
}

bool LLWindowMacOSX::setSize(const LLCoordScreen size)
{
	if (mWindow)
	{
		LLCoordWindow to;
		convertCoords(size, &to);
		setWindowSize(mWindow, to.mX, to.mY);
		return true;
	}

	return false;
}

void LLWindowMacOSX::swapBuffers()
{
	LL_FAST_TIMER(FTM_SWAP);
	CGLFlushDrawable(mContext);
}

void LLWindowMacOSX::setFSAASamples(U32 samples)
{
	mFSAASamples = samples;
	mForceRebuild = true;
}

bool LLWindowMacOSX::restoreGamma()
{
	CGDisplayRestoreColorSyncSettings();
	return true;
}

#if 0
F32 LLWindowMacOSX::getGamma()
{
	F32 result = 1.f;	// Default to something sane

	CGGammaValue rmin, rmax, rgamma, gmin, gmax, ggamma, bmin, bmax, bgamma;
	if (CGGetDisplayTransferByFormula(mDisplay, &rmin, &rmax, &rgamma,
									  &gmin, &gmax, &ggamma,
									  &bmin, &bmax, &bgamma) == noErr)
	{
		F32 sum = rgamma + ggamma + bgamma;
		if (sum > 0.f)
		{
			result = 3.f / sum;
		}
	}

	return result;
}
#endif

// Should we allow this in windowed mode ?
bool LLWindowMacOSX::setGamma(F32 gamma)
{
	mCurrentGamma = llclamp(gamma, 0.01f, 10.f);
	LL_DEBUGS("Window") << "Setting gamma to " << mCurrentGamma << LL_ENDL;
	CGGammaValue rmin, rmax, rgamma, gmin, gmax, ggamma, bmin, bmax, bgamma;
	if (CGGetDisplayTransferByFormula(mDisplay,
									  &rmin, &rmax, &rgamma,
									  &gmin, &gmax, &ggamma,
									  &bmin, &bmax, &bgamma) != noErr)
	{
		return false;
	}

	F32 inv_gamma = 1.f / mCurrentGamma;
	return CGSetDisplayTransferByFormula(mDisplay,
										 rmin, rmax, inv_gamma,
										 gmin, gmax, inv_gamma,
										 bmin, bmax, inv_gamma) == noErr;
}

// Constrains the mouse to the window.
void LLWindowMacOSX::setMouseClipping(bool b)
{
	// Just stash the requested state. We will simulate this when the cursor is
	// hidden by decoupling.
	mIsMouseClipping = b;
	adjustCursorDecouple();
}

bool LLWindowMacOSX::setCursorPosition(const LLCoordWindow& position)
{
	LLCoordScreen screen_pos;
	if (!mCallbacks || !convertCoords(position, &screen_pos))
	{
		return false;
	}

	CGPoint new_pos;
	new_pos.x = screen_pos.mX;
	new_pos.y = screen_pos.mY;
	CGSetLocalEventsSuppressionInterval(0.0);
	bool result = CGWarpMouseCursorPosition(new_pos) == noErr;

	// Under certain circumstances, this will trigger us to decouple the cursor
	adjustCursorDecouple(true);

	// Trigger mouse move callback
	LLCoordGL gl_pos;
	convertCoords(position, &gl_pos);
	F32 scale = getSystemUISize();
	gl_pos.mX *= scale;
	gl_pos.mY *= scale;
	mCallbacks->handleMouseMove(this, gl_pos, (MASK)0);

	return result;
}

bool LLWindowMacOSX::getCursorPosition(LLCoordWindow* position)
{
	if (!mWindow || !position)
	{
		return false;
	}

	float cursor_point[2];
	getCursorPos(mWindow, cursor_point);

	if (mCursorDecoupled)
	{
		cursor_point[0] += mCursorLastEventDeltaX;
		cursor_point[1] += mCursorLastEventDeltaY;
	}

	F32 scale = getSystemUISize();
	position->mX = cursor_point[0] * scale;
	position->mY = cursor_point[1] * scale;

	return true;
}

void LLWindowMacOSX::adjustCursorDecouple(bool warping_mouse)
{
	if (mIsMouseClipping && mCursorHidden)
	{
		if (warping_mouse)
		{
			// The cursor should be decoupled. Make sure it is.
			if (!mCursorDecoupled)
			{
				LL_DEBUGS("Window") << "Decoupling cursor" << LL_ENDL;
				CGAssociateMouseAndMouseCursorPosition(false);
				mCursorDecoupled = true;
				mCursorIgnoreNextDelta = true;
			}
		}
	}
	else
	{
		// The cursor should not be decoupled. Make sure it is not.
		if (mCursorDecoupled)
		{
			LL_DEBUGS("Window") << "Re-coupling cursor" << LL_ENDL;
			CGAssociateMouseAndMouseCursorPosition(true);
			mCursorDecoupled = false;
		}
	}
}

F32 LLWindowMacOSX::getNativeAspectRatio()
{
	if (mFullscreen)
	{
		return (F32)mFullscreenWidth / (F32)mFullscreenHeight;
	}
	else
	{
		// The constructor for this class grabs the aspect ratio of the monitor
		// before doing any resolution switching, and stashes it in
		// mOriginalAspectRatio. Here, we just return it.
		if (mOverrideAspectRatio > 0.f)
		{
			return mOverrideAspectRatio;
		}

		return mOriginalAspectRatio;
	}
}

F32 LLWindowMacOSX::getPixelAspectRatio()
{
	// OS X always enforces a 1:1 pixel aspect ratio, regardless of video mode
	return 1.f;
}


// Since we are no longer supporting the "typical" fullscreen mode with CGL or
// NSOpenGL anymore, these are unnecessary. -Geenz
void LLWindowMacOSX::beforeDialog()
{
}

void LLWindowMacOSX::afterDialog()
{
	// Fixes crash with Core Flow view on OS-X
    CGLSetCurrentContext(mContext);
}

void LLWindowMacOSX::flashIcon(F32)
{
	// For consistency with OS X conventions, the number of seconds given is
	// ignored and left up to the OS (which will actually bounce it for one
	// second).
	requestUserAttention();
}

bool LLWindowMacOSX::isClipboardTextAvailable()
{
	return pasteBoardAvailable();
}

bool LLWindowMacOSX::pasteTextFromClipboard(LLWString& dst)
{
	llutf16string str(copyFromPBoard());
	dst = utf16str_to_wstring(str);
	// *HACK: there is (sometimes) a spurious 'nul' character appearing at the
	// end of the string returned by copyFromPBoard()... So, let's remove it.
	size_t len = dst.size();
	if (len > 1 && dst[len - 1] == '\0')
	{
		dst = dst.substr(0, len - 1);
	}
	return dst != L"";
}

bool LLWindowMacOSX::copyTextToClipboard(const LLWString& s)
{
	llutf16string utf16str = wstring_to_utf16str(s);
	return copyToPBoard(utf16str.data(), utf16str.length());
}

//virtual
bool LLWindowMacOSX::isPrimaryTextAvailable()
{
	return !mPrimaryClipboard.empty();
}

//virtual
bool LLWindowMacOSX::pasteTextFromPrimary(LLWString& text)
{
	if (mPrimaryClipboard.empty())
	{
		return false;
	}

	text = mPrimaryClipboard;
	return true;
}

// virtual
bool LLWindowMacOSX::copyTextToPrimary(const LLWString& text)
{
	mPrimaryClipboard = text;
	return true;
}

LLWindow::LLWindowResolution* LLWindowMacOSX::getSupportedResolutions(S32& num_resolutions)
{
	if (mSupportedResolutions)
	{
		num_resolutions = mNumSupportedResolutions;
		return mSupportedResolutions;
	}

	CFArrayRef modes = CGDisplayAvailableModes(mDisplay);
	if (modes)
	{
		mSupportedResolutions = new LLWindowResolution[MAX_NUM_RESOLUTIONS];
		mNumSupportedResolutions = 0;

		// Examine each mode
		CFIndex cnt = CFArrayGetCount(modes);

		for (CFIndex index = 0;
			 index < cnt && mNumSupportedResolutions < MAX_NUM_RESOLUTIONS;
			 ++index)
		{
			// Pull the mode dictionary out of the CFArray
			CFDictionaryRef mode =
				(CFDictionaryRef)CFArrayGetValueAtIndex(modes, index);
			S32 w = getDictLong(mode, kCGDisplayWidth);
			S32 h = getDictLong(mode, kCGDisplayHeight);
			S32 bits = getDictLong(mode, kCGDisplayBitsPerPixel);
			if (bits == BITS_PER_PIXEL && w >= 800 && h >= 600)
			{
				bool resolution_exists = false;
				for (S32 i = 0; i < mNumSupportedResolutions; ++i)
				{
					if (mSupportedResolutions[i].mWidth == w &&
						mSupportedResolutions[i].mHeight == h)
					{
						resolution_exists = true;
						break;
					}
				}
				if (!resolution_exists)
				{
					mSupportedResolutions[mNumSupportedResolutions].mWidth = w;
					mSupportedResolutions[mNumSupportedResolutions++].mHeight = h;
				}
			}
		}
	}

	num_resolutions = mNumSupportedResolutions;
	return mSupportedResolutions;
}

bool LLWindowMacOSX::convertCoords(LLCoordGL from, LLCoordWindow* to)
{
	to->mX = from.mX;
	to->mY = from.mY;
	return true;
}

bool LLWindowMacOSX::convertCoords(LLCoordWindow from, LLCoordGL* to)
{
	to->mX = from.mX;
	to->mY = from.mY;
	return true;
}

bool LLWindowMacOSX::convertCoords(LLCoordScreen from, LLCoordWindow* to)
{
	if (mWindow && to)
	{
		float mouse_point[2];
		mouse_point[0] = from.mX;
		mouse_point[1] = from.mY;

		convertScreenToWindow(mWindow, mouse_point);

		to->mX = mouse_point[0];
		to->mY = mouse_point[1];

		return true;
	}
	return false;
}

bool LLWindowMacOSX::convertCoords(LLCoordWindow from, LLCoordScreen* to)
{
	if (mWindow && to)
	{
		float mouse_point[2];
		mouse_point[0] = from.mX;
		mouse_point[1] = from.mY;

		convertWindowToScreen(mWindow, mouse_point);

		to->mX = mouse_point[0];
		to->mY = mouse_point[1];

		return true;
	}
	return false;
}

bool LLWindowMacOSX::convertCoords(LLCoordScreen from, LLCoordGL* to)
{
	LLCoordWindow window_coord;
	return convertCoords(from, &window_coord) &&
		   convertCoords(window_coord, to);
}

bool LLWindowMacOSX::convertCoords(LLCoordGL from, LLCoordScreen* to)
{
	LLCoordWindow window_coord;
	return convertCoords(from, &window_coord) &&
		   convertCoords(window_coord, to);
}

void LLWindowMacOSX::setupFailure(const std::string& text)
{
	destroyContext();
	OSMessageBox(text);
}

static const char* cursorIDToName(int id)
{
	switch (id)
	{
		case UI_CURSOR_ARROW:							return "ui_cursor_arrow";
		case UI_CURSOR_WAIT:							return "ui_cursor_wait";
		case UI_CURSOR_HAND:							return "ui_cursor_hand";
		case UI_CURSOR_IBEAM:							return "ui_cursor_ibeam";
		case UI_CURSOR_CROSS:							return "ui_cursor_cross";
		case UI_CURSOR_SIZENWSE:						return "ui_cursor_sizenwse";
		case UI_CURSOR_SIZENESW:						return "ui_cursor_sizenesw";
		case UI_CURSOR_SIZEWE:							return "ui_cursor_sizewe";
		case UI_CURSOR_SIZENS:							return "ui_cursor_sizens";
		case UI_CURSOR_NO:								return "ui_cursor_no";
		case UI_CURSOR_WORKING:							return "ui_cursor_working";
		case UI_CURSOR_TOOLGRAB:						return "ui_cursor_toolgrab";
		case UI_CURSOR_TOOLLAND:						return "ui_cursor_toolland";
		case UI_CURSOR_TOOLFOCUS:						return "ui_cursor_toolfocus";
		case UI_CURSOR_TOOLCREATE:						return "ui_cursor_toolcreate";
		case UI_CURSOR_ARROWDRAG:						return "ui_cursor_arrowdrag";
		case UI_CURSOR_ARROWCOPY:						return "ui_cursor_arrowcopy";
		case UI_CURSOR_ARROWDRAGMULTI:					return "ui_cursor_arrowdragmulti";
		case UI_CURSOR_ARROWCOPYMULTI:					return "ui_cursor_arrowcopymulti";
		case UI_CURSOR_NOLOCKED:						return "ui_cursor_nolocked";
		case UI_CURSOR_ARROWLOCKED:						return "ui_cursor_arrowlocked";
		case UI_CURSOR_GRABLOCKED:						return "ui_cursor_grablocked";
		case UI_CURSOR_TOOLTRANSLATE:					return "ui_cursor_tooltranslate";
		case UI_CURSOR_TOOLROTATE:						return "ui_cursor_toolrotate";
		case UI_CURSOR_TOOLSCALE:						return "ui_cursor_toolscale";
		case UI_CURSOR_TOOLCAMERA:						return "ui_cursor_toolcamera";
		case UI_CURSOR_TOOLPAN:							return "ui_cursor_toolpan";
		case UI_CURSOR_TOOLZOOMIN:						return "ui_cursor_toolzoomin";
		case UI_CURSOR_TOOLPICKOBJECT3:					return "ui_cursor_toolpickobject3";
		case UI_CURSOR_TOOLSIT:							return "ui_cursor_toolsit";
		case UI_CURSOR_TOOLBUY:							return "ui_cursor_toolbuy";
		case UI_CURSOR_TOOLPAY:							return "ui_cursor_toolpay";
		case UI_CURSOR_TOOLOPEN:						return "ui_cursor_toolopen";
		case UI_CURSOR_TOOLPLAY:						return "ui_cursor_toolplay";
		case UI_CURSOR_TOOLPAUSE:						return "ui_cursor_toolpause";
		case UI_CURSOR_TOOLMEDIAOPEN:					return "ui_cursor_toolmediaopen";
		case UI_CURSOR_PIPETTE:							return "ui_cursor_pipette";
		case UI_CURSOR_TOOLPATHFINDING:					return "ui_cursor_pathfinding";
		case UI_CURSOR_TOOLPATHFINDING_PATH_START:		return "ui_cursor_pathfinding_start";
		case UI_CURSOR_TOOLPATHFINDING_PATH_START_ADD:	return "ui_cursor_pathfinding_start_add";
		case UI_CURSOR_TOOLPATHFINDING_PATH_END:		return "ui_cursor_pathfinding_end";
		case UI_CURSOR_TOOLPATHFINDING_PATH_END_ADD:	return "ui_cursor_pathfinding_end_add";
		case UI_CURSOR_TOOLNO:							return "ui_cursor_no";
	}

	llerrs << "Unknown cursor id: " << id << llendl;

	return "ui_cursor_arrow";
}

static CursorRef gCursors[UI_CURSOR_COUNT];

static void initPixmapCursor(int cursorid, int hotspot_x, int hotspot_y)
{
	// Cursors are in:
	// <Application Bundle>/Contents/Resources/cursors_mac/ui_cursor_foo.tif
	std::string fullpath = gDirUtilp->getAppRODataDir() + "/cursors_mac/";
	fullpath += cursorIDToName(cursorid);
	fullpath += ".tif";

	gCursors[cursorid] = createImageCursor(fullpath.c_str(), hotspot_x,
										   hotspot_y);
}

void LLWindowMacOSX::updateCursor()
{
	if (mNextCursor == UI_CURSOR_ARROW && mBusyCount > 0)
	{
		mNextCursor = UI_CURSOR_WORKING;
	}

	if (mCurrentCursor == mNextCursor)
	{
		if (mCursorHidden && mHideCursorPermanent && isCGCursorVisible())
		{
			hideNSCursor();
			adjustCursorDecouple();
		}
		return;
	}

	// RN: replace multi-drag cursors with single versions
	if (mNextCursor == UI_CURSOR_ARROWDRAGMULTI)
	{
		mNextCursor = UI_CURSOR_ARROWDRAG;
	}
	else if (mNextCursor == UI_CURSOR_ARROWCOPYMULTI)
	{
		mNextCursor = UI_CURSOR_ARROWCOPY;
	}

	switch (mNextCursor)
	{
		default:
		case UI_CURSOR_ARROW:
			setArrowCursor();
			if (mCursorHidden)
			{
				// Since InitCursor resets the hide level, correct for it here.
				hideNSCursor();
			}
			break;

		// Some of the standard Windows cursors have no standard Mac
		// equivalents. Find out what they look like and replicate them.

		// These are essentially correct
		case UI_CURSOR_WAIT:
			// Apple purposely does not allow us to set the beachball cursor
			// manually. Let NSApp figure out when to do this.
			break;
		case UI_CURSOR_IBEAM:
			setIBeamCursor();
			break;
		case UI_CURSOR_CROSS:
			setCrossCursor();
			break;
		case UI_CURSOR_HAND:
			setPointingHandCursor();
			break;
		case UI_CURSOR_ARROWCOPY:
			setCopyCursor();
			break;

		// Double-check these
		case UI_CURSOR_NO:
		case UI_CURSOR_SIZEWE:
		case UI_CURSOR_SIZENS:
		case UI_CURSOR_SIZENWSE:
		case UI_CURSOR_SIZENESW:
		case UI_CURSOR_WORKING:
		case UI_CURSOR_TOOLGRAB:
		case UI_CURSOR_TOOLLAND:
		case UI_CURSOR_TOOLFOCUS:
		case UI_CURSOR_TOOLCREATE:
		case UI_CURSOR_ARROWDRAG:
		case UI_CURSOR_NOLOCKED:
		case UI_CURSOR_ARROWLOCKED:
		case UI_CURSOR_GRABLOCKED:
		case UI_CURSOR_TOOLTRANSLATE:
		case UI_CURSOR_TOOLROTATE:
		case UI_CURSOR_TOOLSCALE:
		case UI_CURSOR_TOOLCAMERA:
		case UI_CURSOR_TOOLPAN:
		case UI_CURSOR_TOOLZOOMIN:
		case UI_CURSOR_TOOLPICKOBJECT3:
		case UI_CURSOR_TOOLPLAY:
		case UI_CURSOR_TOOLPAUSE:
		case UI_CURSOR_TOOLMEDIAOPEN:
		case UI_CURSOR_TOOLSIT:
		case UI_CURSOR_TOOLBUY:
		case UI_CURSOR_TOOLPAY:
		case UI_CURSOR_TOOLOPEN:
		case UI_CURSOR_TOOLPATHFINDING:
		case UI_CURSOR_TOOLPATHFINDING_PATH_START:
		case UI_CURSOR_TOOLPATHFINDING_PATH_START_ADD:
		case UI_CURSOR_TOOLPATHFINDING_PATH_END:
		case UI_CURSOR_TOOLPATHFINDING_PATH_END_ADD:
		case UI_CURSOR_TOOLNO:
		{
			if (setImageCursor(gCursors[mNextCursor]) != noErr)
			{
				setArrowCursor();
			}
		}
	}

	mCurrentCursor = mNextCursor;
}

void LLWindowMacOSX::initCursors()
{
	initPixmapCursor(UI_CURSOR_NO, 8, 8);
	initPixmapCursor(UI_CURSOR_WORKING, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLGRAB, 2, 14);
	initPixmapCursor(UI_CURSOR_TOOLLAND, 13, 8);
	initPixmapCursor(UI_CURSOR_TOOLFOCUS, 7, 6);
	initPixmapCursor(UI_CURSOR_TOOLCREATE, 7, 7);
	initPixmapCursor(UI_CURSOR_ARROWDRAG, 1, 1);
	initPixmapCursor(UI_CURSOR_ARROWCOPY, 1, 1);
	initPixmapCursor(UI_CURSOR_NOLOCKED, 8, 8);
	initPixmapCursor(UI_CURSOR_ARROWLOCKED, 1, 1);
	initPixmapCursor(UI_CURSOR_GRABLOCKED, 2, 14);
	initPixmapCursor(UI_CURSOR_TOOLTRANSLATE, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLROTATE, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLSCALE, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLCAMERA, 7, 6);
	initPixmapCursor(UI_CURSOR_TOOLPAN, 7, 6);
	initPixmapCursor(UI_CURSOR_TOOLZOOMIN, 7, 6);

	initPixmapCursor(UI_CURSOR_TOOLPICKOBJECT3, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLSIT, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLBUY, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLPAY, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLOPEN, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLPLAY, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLPAUSE, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLMEDIAOPEN, 1, 1);
	initPixmapCursor(UI_CURSOR_TOOLPATHFINDING, 16, 16);
	initPixmapCursor(UI_CURSOR_TOOLPATHFINDING_PATH_START, 16, 16);
	initPixmapCursor(UI_CURSOR_TOOLPATHFINDING_PATH_START_ADD, 16, 16);
	initPixmapCursor(UI_CURSOR_TOOLPATHFINDING_PATH_END, 16, 16);
	initPixmapCursor(UI_CURSOR_TOOLPATHFINDING_PATH_END_ADD, 16, 16);
	initPixmapCursor(UI_CURSOR_TOOLNO, 8, 8);

	initPixmapCursor(UI_CURSOR_SIZENWSE, 10, 10);
	initPixmapCursor(UI_CURSOR_SIZENESW, 10, 10);
	initPixmapCursor(UI_CURSOR_SIZEWE, 10, 10);
	initPixmapCursor(UI_CURSOR_SIZENS, 10, 10);
}

void LLWindowMacOSX::setCursor(ECursorType c)
{
	if (!mCursorFrozen)
	{
		mNextCursor = c;
	}
}

void LLWindowMacOSX::captureMouse()
{
	// By registering a global Event handler for mouse move events, we ensure
	// that mouse events are always processed. Thus, capture and release are
	// unnecessary.
}

void LLWindowMacOSX::releaseMouse()
{
	// By registering a global Event handler for mouse move events, we ensure
	// that mouse events are always processed. Thus, capture and release are
	// unnecessary.
}

void LLWindowMacOSX::hideCursor()
{
	if (!mCursorHidden)
	{
		mCursorHidden = mHideCursorPermanent = true;
		hideNSCursor();
	}

	adjustCursorDecouple();
}

void LLWindowMacOSX::showCursor()
{
	if (mCursorHidden)
	{
		mCursorHidden = mHideCursorPermanent = false;
		showNSCursor();
	}

	adjustCursorDecouple();
}

void LLWindowMacOSX::showCursorFromMouseMove()
{
	if (!mHideCursorPermanent)
	{
		showCursor();
	}
}

void LLWindowMacOSX::hideCursorUntilMouseMove()
{
	if (!mHideCursorPermanent)
	{
		hideCursor();
		mHideCursorPermanent = false;
	}
}

//
// LLSplashScreenMacOSX
//
LLSplashScreenMacOSX::LLSplashScreenMacOSX()
{
	mWindow = NULL;
}

void LLSplashScreenMacOSX::showImpl()
{
	// This _could_ be used to display a spash screen...
}

void LLSplashScreenMacOSX::updateImpl(const std::string& mesg)
{
}

void LLSplashScreenMacOSX::hideImpl()
{
	if (mWindow)
	{
		mWindow = NULL;
	}
}

S32 OSMessageBoxMacOSX(const std::string& text, const std::string& caption,
					   U32 type)
{
	return showAlert(text, caption, type);
}

// Open a URL with the user's default web browser. Must begin with protocol
// identifier.
void LLWindowMacOSX::spawnWebBrowser(const std::string& escaped_url,
									 bool async)
{
	bool found = false;
	for (S32 i = 0; i < gURLProtocolWhitelistCount; ++i)
	{
		if (escaped_url.find(gURLProtocolWhitelist[i]) != std::string::npos)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		llwarns << "spawn_web_browser called for url with protocol not on whitelist: "
				<< escaped_url << llendl;
		return;
	}

	S32 result = 0;
	CFURLRef urlRef = NULL;

	llinfos << "Opening URL " << escaped_url << llendl;

	CFStringRef	stringRef = CFStringCreateWithCString(NULL,
													  escaped_url.c_str(),
													  kCFStringEncodingUTF8);
	if (stringRef)
	{
		// This will succeed if the string is a full URL, including the http://
		// Note that URLs specified this way need to be properly percent-
		// escaped.
		urlRef = CFURLCreateWithString(NULL, stringRef, NULL);

		// Do not use CRURLCreateWithFileSystemPath -- only want valid URLs
		CFRelease(stringRef);
	}

	if (urlRef)
	{
		result = LSOpenCFURLRef(urlRef, NULL);

		if (result != noErr)
		{
			llwarns << "Error " << result << " on open." << llendl;
		}

		CFRelease(urlRef);
	}
	else
	{
		llwarns << "Could not create URL." << llendl;
	}
}

// Make the raw keyboard data available
LLSD LLWindowMacOSX::getNativeKeyData()
{
	LLSD result = LLSD::emptyMap();

	if (mRawKeyEvent)
	{
		result["event_type"] = LLSD::Integer(mRawKeyEvent->mEventType);
		result["event_modifiers"] =
			LLSD::Integer(mRawKeyEvent->mEventModifiers);
		result["event_keycode"] = LLSD::Integer(mRawKeyEvent->mEventKeyCode);
		result["event_chars"] =
			mRawKeyEvent->mEventChars ? LLSD(LLSD::Integer(mRawKeyEvent->mEventChars))
									  : LLSD();
		result["event_umodchars"] =
			mRawKeyEvent->mEventUnmodChars ? LLSD(LLSD::Integer(mRawKeyEvent->mEventUnmodChars))
										   : LLSD();
		result["event_isrepeat"] = LLSD::Boolean(mRawKeyEvent->mEventRepeat);
	}

	LL_DEBUGS("Window") << "Native key data is: " << result << LL_ENDL;

	return result;
}

F32 LLWindowMacOSX::getSystemUISize()
{
	return gHiDPISupport ? ::getDeviceUnitSize(mGLView) : 1.f;
}

void* LLWindowMacOSX::getPlatformWindow()
{
	// NOTE: this will be NULL in fullscreen mode. Plan accordingly.
	return (void*)mWindow;
}

void LLWindowMacOSX::allowLanguageTextInput(LLPreeditor* preeditor, bool b)
{
	if (!preeditor) return;

	if (preeditor != mPreeditor && !b)
	{
		// This condition may occur by a call to
		// setEnabled(bool) against LLTextEditor or LLLineEditor
		// when the control is not focused.
		// We need to silently ignore the case so that
		// the language input status of the focused control
		// is not disturbed.
		return;
	}

	// Take care of old and new preeditors.
	if (preeditor != mPreeditor || !b)
	{
		// We need to interrupt before updating mPreeditor,
		// so that the fix string from input method goes to
		// the old preeditor.
		if (mLanguageTextInputAllowed)
		{
			interruptLanguageTextInput();
		}
		mPreeditor = (b ? preeditor : NULL);
	}

	if (mLanguageTextInputAllowed != b)
	{
		mLanguageTextInputAllowed = b;
		allowDirectMarkedTextInput(b, mGLView);
	}
}

void LLWindowMacOSX::interruptLanguageTextInput()
{
	commitCurrentPreedit(mGLView);
}

//static
std::vector<std::string> LLWindowMacOSX::getDynamicFallbackFontList()
{
	// Fonts previously in getFontListSans() have moved to fonts.xml.
	return std::vector<std::string>();
}

void LLWindowMacOSX::updateMouseDeltas(float* deltas)
{
	if (mCursorDecoupled && deltas)
	{
		if (mCursorIgnoreNextDelta)
		{
			mCursorLastEventDeltaX = mCursorLastEventDeltaY = 0;
			mCursorIgnoreNextDelta = false;
		}
		else
		{
			mCursorLastEventDeltaX = ll_round(deltas[0]);
			mCursorLastEventDeltaY = ll_round(-deltas[1]);
		}
	}
	else
	{
		mCursorLastEventDeltaX = mCursorLastEventDeltaY = 0;
	}
}

void LLWindowMacOSX::getMouseDeltas(float* deltas)
{
	if (deltas)
	{
		deltas[0] = mCursorLastEventDeltaX;
		deltas[1] = mCursorLastEventDeltaY;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Shared OpenGL context support
///////////////////////////////////////////////////////////////////////////////

struct LLSharedOpenGLContext
{
	CGLContextObj mContext;
};

void* LLWindowMacOSX::createSharedContext()
{
	LLSharedOpenGLContext* context = new LLSharedOpenGLContext;
	CGLCreateContext(mPixelFormat, mContext, &(context->mContext));
	if (!context->mContext)
	{
		// Something went (very) wrong... Free the structure and return a NULL
		// pointer to signify we do not have a GL context available. HB
		llwarns_sparse << "Failed to create a new shared GL context."
					   << llendl;
		delete context;
		return NULL;
	}
	if (sUseMultGL)
	{
		CGLEnable(mContext, kCGLCEMPEngine);
	}
	return (void*)context;
}

void LLWindowMacOSX::makeContextCurrent(void* context)
{
	if (context)
	{
		CGLSetCurrentContext(((LLSharedOpenGLContext*)context)->mContext);
	}
	else
	{
		// Restore main GL thread context.
		CGLSetCurrentContext(mContext);
	}
}

void LLWindowMacOSX::destroySharedContext(void* context)
{
	if (context)	// Ignore attempts to destroy invalid contexts. HB
	{
		LLSharedOpenGLContext* sc = (LLSharedOpenGLContext*)context;
		CGLDestroyContext(sc->mContext);
		delete sc;
	}
}

///////////////////////////////////////////////////////////////////////////////
// These functions are used as wrappers for our internal event handling
// callbacks. It is a good idea to wrap these to avoid reworking more code than
// we need to within LLWindow.
///////////////////////////////////////////////////////////////////////////////

bool callKeyUp(NSKeyEventRef event, unsigned short key, unsigned int mask)
{
	bool ret_val = false;
	if (gKeyboardp)
	{
		mRawKeyEvent = event;
		ret_val = gKeyboardp->handleKeyUp(key, mask);
		mRawKeyEvent = NULL;
	}
	return ret_val;
}

bool callKeyDown(NSKeyEventRef event, unsigned short key, unsigned int mask)
{
	bool ret_val = false;
	if (gKeyboardp)
	{
		mRawKeyEvent = event;
		ret_val = gKeyboardp->handleKeyDown(key, mask);
		mRawKeyEvent = NULL;
	}
	return ret_val;
}

void callResetKeys()
{
	if (gKeyboardp)
	{
		gKeyboardp->resetKeys();
	}
}

bool callUnicodeCallback(wchar_t character, unsigned int mask)
{
	if (!sWindowImplementation) return false;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return false;

	NativeKeyEventData event_data;
	memset(&event_data, 0, sizeof(NativeKeyEventData));

	event_data.mKeyEvent = NativeKeyEventData::KEYCHAR;
	event_data.mEventType = 0;
	event_data.mEventModifiers = mask;
	event_data.mEventKeyCode = 0;
	event_data.mEventChars = character;
	event_data.mEventUnmodChars = character;
	event_data.mEventRepeat = false;

	mRawKeyEvent = &event_data;

	bool result = callbacks->handleUnicodeChar(character, mask);
	mRawKeyEvent = NULL;

	return result;
}

void callFocus()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleFocus(sWindowImplementation);
		}
	}
}

void callFocusLost()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleFocusLost(sWindowImplementation);
		}
	}
}

void callRightMouseDown(float* pos, MASK mask)
{
	if (!sWindowImplementation || !gKeyboardp) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	if (sWindowImplementation->allowsLanguageInput())
	{
		sWindowImplementation->interruptLanguageTextInput();
	}

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	callbacks->handleRightMouseDown(sWindowImplementation, out_coords,
									gKeyboardp->currentMask(true));
}

void callRightMouseUp(float* pos, MASK mask)
{
	if (!sWindowImplementation || !gKeyboardp) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	if (sWindowImplementation->allowsLanguageInput())
	{
		sWindowImplementation->interruptLanguageTextInput();
	}

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	callbacks->handleRightMouseUp(sWindowImplementation, out_coords,
								  gKeyboardp->currentMask(true));
}

void callLeftMouseDown(float* pos, MASK mask)
{
	if (!sWindowImplementation || !gKeyboardp) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	if (sWindowImplementation->allowsLanguageInput())
	{
		sWindowImplementation->interruptLanguageTextInput();
	}

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	callbacks->handleMouseDown(sWindowImplementation, out_coords,
							   gKeyboardp->currentMask(true));
}

void callLeftMouseUp(float* pos, MASK mask)
{
	if (!sWindowImplementation || !gKeyboardp) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	if (sWindowImplementation->allowsLanguageInput())
	{
		sWindowImplementation->interruptLanguageTextInput();
	}

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	callbacks->handleMouseUp(sWindowImplementation, out_coords,
							 gKeyboardp->currentMask(true));

}

void callDoubleClick(float* pos, MASK mask)
{
	if (!sWindowImplementation || !gKeyboardp) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	if (sWindowImplementation->allowsLanguageInput())
	{
		sWindowImplementation->interruptLanguageTextInput();
	}

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	callbacks->handleDoubleClick(sWindowImplementation, out_coords,
								 gKeyboardp->currentMask(true));
}

void callResize(unsigned int width, unsigned int height)
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleResize(sWindowImplementation, width, height);
		}
	}
}

void callMouseMoved(float* pos, MASK mask)
{
	if (!sWindowImplementation || !gKeyboardp) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	float deltas[2];
	sWindowImplementation->getMouseDeltas(deltas);
	out_coords.mX += deltas[0];
	out_coords.mY += deltas[1];
	callbacks->handleMouseMove(sWindowImplementation, out_coords,
							   gKeyboardp->currentMask(true));
}

void callMouseDragged(float* pos, MASK mask)
{
	if (!sWindowImplementation || !gKeyboardp) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	float deltas[2];
	sWindowImplementation->getMouseDeltas(deltas);
	out_coords.mX += deltas[0];
	out_coords.mY += deltas[1];
	callbacks->handleMouseDragged(sWindowImplementation, out_coords,
								  gKeyboardp->currentMask(true));
}

void callScrollMoved(float delta)
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleScrollWheel(sWindowImplementation, delta);
		}
	}
}

void callMouseExit()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleMouseLeave(sWindowImplementation);
		}
	}
}

void callWindowFocus()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleFocus(sWindowImplementation);
			return;
		}
	}

	llwarns << "Window implementation or callbacks not yet initialized."
			<< llendl;
}

void callWindowUnfocus()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleFocusLost(sWindowImplementation);
		}
	}
}

void callWindowHide()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleActivate(sWindowImplementation, false);
		}
	}
}

void callWindowUnhide()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleActivate(sWindowImplementation, true);
		}
	}
}

void callWindowDidChangeScreen()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleWindowDidChangeScreen(sWindowImplementation);
		}
	}
}

void callDeltaUpdate(float* delta, MASK mask)
{
	if (sWindowImplementation)
	{
		sWindowImplementation->updateMouseDeltas(delta);
	}
}

void callMiddleMouseDown(float* pos, MASK mask)
{
	if (!sWindowImplementation) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	float deltas[2];
	sWindowImplementation->getMouseDeltas(deltas);
	out_coords.mX += deltas[0];
	out_coords.mY += deltas[1];
	callbacks->handleMiddleMouseDown(sWindowImplementation, out_coords, mask);
}

void callMiddleMouseUp(float* pos, MASK mask)
{
	if (!sWindowImplementation) return;

	LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
	if (!callbacks) return;

	LLCoordGL out_coords;
	out_coords.mX = ll_round(pos[0]);
	out_coords.mY = ll_round(pos[1]);
	float deltas[2];
	sWindowImplementation->getMouseDeltas(deltas);
	out_coords.mX += deltas[0];
	out_coords.mY += deltas[1];
	callbacks->handleMiddleMouseUp(sWindowImplementation, out_coords, mask);
}

void callQuitHandler()
{
	if (sWindowImplementation)
	{
		LLWindowCallbacks* callbacks = sWindowImplementation->getCallbacks();
		if (callbacks)
		{
			callbacks->handleQuit(sWindowImplementation);
		}
	}
}

void getPreeditSelectionRange(int* position, int* length)
{
	if (sWindowImplementation)
	{
		LLPreeditor* preeditor = sWindowImplementation->getPreeditor();
		if (preeditor)
		{
			preeditor->getSelectionRange(position, length);
		}
	}
}

void getPreeditMarkedRange(int* position, int* length)
{
	if (sWindowImplementation)
	{
		LLPreeditor* preeditor = sWindowImplementation->getPreeditor();
		if (preeditor)
		{
			preeditor->getPreeditRange(position, length);
		}
	}
}

void setPreeditMarkedRange(int position, int length)
{
	if (sWindowImplementation)
	{
		LLPreeditor* preeditor = sWindowImplementation->getPreeditor();
		if (preeditor)
		{
			preeditor->markAsPreedit(position, length);
		}
	}
}

bool handleUnicodeCharacter(wchar_t c)
{
	if (sWindowImplementation)
	{
		LLPreeditor* preeditor = sWindowImplementation->getPreeditor();
		if (preeditor)
		{
			return preeditor->handleUnicodeCharHere(c);
		}
	}
	return false;
}

void resetPreedit()
{
	if (sWindowImplementation)
	{
		LLPreeditor* preeditor = sWindowImplementation->getPreeditor();
		if (preeditor)
		{
			preeditor->resetPreedit();
		}
	}
}

// For reasons of convenience, handle IME updates here. This largely mirrors the
// old implementation.
void setMarkedText(unsigned short* unitext, unsigned int* sel_range,
				   unsigned int* replace_range, long text_len,
				   attributedStringInfo segments)
{
	if (!sWindowImplementation) return;

	LLPreeditor* preeditor = sWindowImplementation->getPreeditor();
	if (!preeditor) return;

	preeditor->resetPreedit();

	// This should be a viable replacement for the
	// kEventParamTextInputSendReplaceRange parameter.
	if (replace_range[0] < replace_range[1])
	{
		const LLWString& text = preeditor->getWText();
		S32 location = wstring_length_from_utf16_length(text, 0,
														replace_range[0]);
		S32 length = wstring_length_from_utf16_length(text, location,
													  replace_range[1]);
		preeditor->markAsPreedit(location, length);
	}

	LLWString fix_str = utf16str_to_wstring(llutf16string(unitext, text_len));
	S32 caret_position = fix_str.length();
	preeditor->updatePreedit(fix_str, segments.seg_lengths,
							 segments.seg_standouts, caret_position);
}

void getPreeditLocation(float* location, unsigned int length)
{
	if (!sWindowImplementation)
	{
		return;
	}

	LLPreeditor* preeditor = sWindowImplementation->getPreeditor();
	if (preeditor)
	{
		LLCoordGL coord;
		LLCoordScreen screen;
		LLRect rect;
		preeditor->getPreeditLocation(length, &coord, &rect, NULL);

		float c[4] = { (float)coord.mX, (float)coord.mY, 0.f, 0.f };

		convertRectToScreen(sWindowImplementation->getPlatformWindow(), c);
		location[0] = c[0];
		location[1] = c[1];
	}
}

void callModifier(MASK mask)
{
	if (gKeyboardp)
	{
		gKeyboardp->handleModifier(mask);
	}
}

// Drag and drop into viewer window not yet implemented in the Cool VL Viewer

void callHandleDragEntered(std::string url)
{
}

void callHandleDragExited(std::string url)
{
}

void callHandleDragUpdated(std::string url)
{
}

void callHandleDragDropped(std::string url)
{
}
