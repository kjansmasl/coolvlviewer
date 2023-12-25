/**
 * @file llwindow.h
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

#ifndef LL_LLWINDOW_H
#define LL_LLWINDOW_H

#include "llcoord.h"
#include "llcursortypes.h"
#include "llrect.h"
#include "llsd.h"
#include "llstring.h"

class LLPreeditor;
class LLSplashScreen;
class LLWindow;

class LLWindowCallbacks
{
public:
	virtual ~LLWindowCallbacks()					{}
	virtual bool handleTranslatedKeyDown(KEY key,  MASK mask, bool repeated);
	virtual bool handleTranslatedKeyUp(KEY key,  MASK mask);
	virtual void handleScanKey(KEY key, bool key_down, bool key_up, bool key_level);
	virtual bool handleUnicodeChar(llwchar uni_char, MASK mask);

	virtual bool handleMouseDown(LLWindow* window,  LLCoordGL pos, MASK mask);
	virtual bool handleMouseUp(LLWindow* window,  LLCoordGL pos, MASK mask);
	virtual void handleMouseLeave(LLWindow* window);
	// Returns true to allow window to close, which will then cause handleQuit
	// to be called
	virtual bool handleCloseRequest(LLWindow* window);
	// window is about to be destroyed, clean up your business
	virtual void handleQuit(LLWindow* window);
	virtual bool handleRightMouseDown(LLWindow* window,  LLCoordGL pos, MASK mask);
	virtual bool handleRightMouseUp(LLWindow* window,  LLCoordGL pos, MASK mask);
	virtual bool handleMiddleMouseDown(LLWindow* window,  LLCoordGL pos, MASK mask);
	virtual bool handleMiddleMouseUp(LLWindow* window,  LLCoordGL pos, MASK mask);
	virtual bool handleActivate(LLWindow* window, bool activated);
	virtual bool handleActivateApp(LLWindow* window, bool activating);
	virtual void handleMouseMove(LLWindow* window,  LLCoordGL pos, MASK mask);
#if LL_DARWIN
	virtual void handleMouseDragged(LLWindow* window, LLCoordGL pos, MASK mask);
#endif
	virtual void handleScrollWheel(LLWindow* window,  S32 clicks);
	virtual void handleResize(LLWindow* window,  S32 width,  S32 height);
	virtual void handleFocus(LLWindow* window);
	virtual void handleFocusLost(LLWindow* window);
	virtual void handleMenuSelect(LLWindow* window,  S32 menu_item);
	virtual bool handlePaint(LLWindow* window, S32 x, S32 y, S32 width, S32 height);
	// Double-click of left mouse button:
	virtual bool handleDoubleClick(LLWindow* window,  LLCoordGL pos, MASK mask);
	// Window is taking over CPU for a while:
	virtual void handleWindowBlock(LLWindow* window);
	// Window coming back after taking over CPU for a while:
	virtual void handleWindowUnblock(LLWindow* window);
	virtual void handleDataCopy(LLWindow* window, S32 data_type, void* data);
	virtual bool handleTimerEvent(LLWindow* window);
	virtual bool handleDeviceChange(LLWindow* window);
	virtual bool handleDPIChanged(LLWindow* window, F32 ui_scale_factor,
								  S32 window_width, S32 window_height);
	virtual bool handleWindowDidChangeScreen(LLWindow* window);
};

// Refer to llwindow_test in test/common/llwindow for usage example

class LLWindow
{
public:
	struct LLWindowResolution
	{
		S32 mWidth;
		S32 mHeight;
	};

	enum ESwapMethod
	{
		SWAP_METHOD_UNDEFINED,
		SWAP_METHOD_EXCHANGE,
		SWAP_METHOD_COPY
	};

	enum EFlags
	{
		// Currently unused
	};

public:
	static void createWindow(const std::string& title, S32 x, S32 y,
							 U32 width, U32 height, U32 flags = 0,
							 bool fullscreen = false,
							 bool disable_vsync = true,
							 U32 fsaa_samples = 0);

	static void destroyWindow();

	virtual void setWindowTitle(const std::string& title) = 0;

	virtual void show() = 0;
	virtual void hide() = 0;
	virtual void close() = 0;
	virtual void minimize() = 0;
	virtual void restore() = 0;
	virtual bool getVisible() = 0;
	virtual bool getMinimized() = 0;
	virtual bool getMaximized() = 0;
	virtual bool maximize() = 0;
	virtual bool getFullscreen() = 0;

	virtual bool getSize(LLCoordScreen* size) = 0;
	virtual bool getSize(LLCoordWindow* size) = 0;
	virtual bool setSize(LLCoordScreen size) = 0;

	// *HACK: to compute window borders offsets (needed for SDL2). HB
	LL_INLINE virtual void calculateBordersOffsets()		{}
	// *HACK: to force-redraw the screen (needed for SDL). HB
	LL_INLINE virtual void refresh()						{}

	virtual bool getPosition(LLCoordScreen* position) = 0;
	virtual bool setPosition(LLCoordScreen position) = 0;

	virtual bool switchContext(bool fullscreen, const LLCoordScreen& size,
							   bool disable_vsync,
							   const LLCoordScreen* const posp = NULL) = 0;

	// Creates a new GL context that shares a namespace with this Window's main
	// GL context and makes it current on the current thread. Returns a pointer
	// to be handed back to destroySharedConext()/makeContextCurrent().
	virtual void* createSharedContext() = 0;
	// Makes the given context current on the current thread
	virtual void makeContextCurrent(void* context) = 0;
	// Destroys the given context that was retrieved by createSharedContext().
	// Must be called on the same thread that called createSharedContext().
	virtual void destroySharedContext(void* context) = 0;

	virtual bool setCursorPosition(const LLCoordWindow& position) = 0;
	virtual bool getCursorPosition(LLCoordWindow* position) = 0;
	virtual void showCursor() = 0;
	virtual void hideCursor() = 0;
	virtual bool isCursorHidden() = 0;
	virtual void showCursorFromMouseMove() = 0;
	virtual void hideCursorUntilMouseMove() = 0;

	// These two methods create a way to make a busy cursor instead of an arrow
	// when someone is busy doing something.
	LL_INLINE void incBusyCount()						{ ++mBusyCount; }
	void decBusyCount();
	LL_INLINE void resetBusyCount()						{ mBusyCount = 0; }
	LL_INLINE S32 getBusyCount() const					{ return mBusyCount; }

	// Sets cursor, may set to arrow+hourglass
	virtual void setCursor(ECursorType cursor) = 0;
	LL_INLINE ECursorType getCursor() const				{ return mCurrentCursor; }
	// Used to prevent any cursor change during a call where various methods
	// are called that can each chenge the cursor, causing a flickering. HB
	LL_INLINE void freezeCursor(bool freeze)			{ mCursorFrozen = freeze; }

	virtual void captureMouse() = 0;
	virtual void releaseMouse() = 0;
	virtual void setMouseClipping(bool b) = 0;
	virtual bool isClipboardTextAvailable() = 0;
	virtual bool pasteTextFromClipboard(LLWString& text) = 0;
	virtual bool copyTextToClipboard(const LLWString& text) = 0;

	virtual bool isPrimaryTextAvailable() = 0;
	virtual bool pasteTextFromPrimary(LLWString& text) = 0;
	virtual bool copyTextToPrimary(const LLWString& text) = 0;

	virtual void flashIcon(F32 seconds) = 0;
	LL_INLINE virtual F32 getGamma()					{ return mCurrentGamma; }
	virtual bool setGamma(F32 gamma) = 0; // Set the gamma
	// Restore original gamma table (before updating gamma)
	virtual bool restoreGamma() = 0;
	// set number of FSAA samples
	virtual void setFSAASamples(U32 fsaa_samples) = 0;
	virtual U32 getFSAASamples() = 0;

	LL_INLINE virtual ESwapMethod getSwapMethod()		{ return mSwapMethod; }
	virtual void gatherInput() = 0;
	virtual void delayInputProcessing() = 0;
	virtual void swapBuffers() = 0;
	virtual void bringToFront() = 0;
	// This may not have meaning or be required on other platforms, therefore,
	// it is not abstract
	LL_INLINE virtual void focusClient()				{}

	// Handy coordinate space conversion routines
	// NB: screen to window and vice verse won't work on width/height
	// coordinate pairs, as the conversion must take into account left AND
	// right border widths, etc.
	virtual bool convertCoords(LLCoordScreen from, LLCoordWindow* to) = 0;
	virtual bool convertCoords(LLCoordWindow from, LLCoordScreen* to) = 0;
	virtual bool convertCoords(LLCoordWindow from, LLCoordGL* to) = 0;
	virtual bool convertCoords(LLCoordGL from, LLCoordWindow* to) = 0;
	virtual bool convertCoords(LLCoordScreen from, LLCoordGL* to) = 0;
	virtual bool convertCoords(LLCoordGL from, LLCoordScreen* to) = 0;

	// Query supported resolutions
	virtual LLWindowResolution* getSupportedResolutions(S32& num_resolutions) = 0;
	virtual F32	getNativeAspectRatio() = 0;
	virtual F32 getPixelAspectRatio() = 0;

	LL_INLINE virtual void setNativeAspectRatio(F32 ratio)
	{
		mOverrideAspectRatio = ratio;
	}

	void setCallbacks(LLWindowCallbacks* callbacks);

	// Prepares to put up an OS dialog (if special measures are required, such
	// as in fullscreen mode)
	LL_INLINE virtual void beforeDialog()				{}
	// Undoes whatever was done in beforeDialog()
	LL_INLINE virtual void afterDialog()				{}

	// Returns a platform-specific window reference (HWND on Windows,
	// WindowRef on the Mac, Gtk window on Linux)
	virtual void* getPlatformWindow() = 0;

	// Control the platform language text input mechanisms.

	LL_INLINE virtual void allowLanguageTextInput(LLPreeditor* p, bool b)
	{
	}

	LL_INLINE virtual void setLanguageTextInput(const LLCoordGL& pos)
	{
	}

	LL_INLINE virtual void updateLanguageTextInputArea()
	{
	}

	LL_INLINE virtual void interruptLanguageTextInput()
	{
	}

	LL_INLINE virtual void spawnWebBrowser(const std::string& escaped_url,
										   bool async)
	{
	}

	static std::vector<std::string> getDynamicFallbackFontList();

	// Provides native key event data
	LL_INLINE virtual LLSD getNativeKeyData()			{ return LLSD::emptyMap(); }

	// Get system UI size based on DPI (for 96 DPI UI size should be 1.f)
	LL_INLINE virtual F32 getSystemUISize()				{ return 1.f; }

protected:
	LLWindow(bool fullscreen, U32 flags);
	virtual ~LLWindow()									{}
	LL_INLINE virtual bool isValid()					{ return true; }

	LL_INLINE bool shouldPostQuit()						{ return mPostQuit; }

	// Handles a UTF-16 encoding unit received from keyboard. Converting the
	// series of UTF-16 encoding units to UTF-32 data, this method passes the
	// resulting UTF-32 data to mCallback's handleUnicodeChar. The mask should
	// be that to be passed to the callback. This method uses mHighSurrogate as
	// a dedicated work variable.
	void handleUnicodeUTF16(U16 utf16, MASK mask);

protected:
	static LLWindowCallbacks	sDefaultCallbacks;

protected:
	LLWindowCallbacks*	mCallbacks;
	LLWindowResolution*	mSupportedResolutions;

	S32					mFullscreenWidth;
	S32					mFullscreenHeight;
	S32					mFullscreenBits;
	S32					mFullscreenRefresh;
	F32					mOverrideAspectRatio;
	F32					mCurrentGamma;
	S32					mNumSupportedResolutions;
	S32					mBusyCount;			// how deep is the "cursor busy" stack ?
	U32					mFlags;
	U16					mHighSurrogate;

	ECursorType			mCurrentCursor;
#if LL_DARWIN
	ECursorType			mNextCursor;
#endif
	ESwapMethod			mSwapMethod;
	// should this window post a quit message when destroyed ?
	bool				mPostQuit;
	bool				mFullscreen;
	bool				mCursorFrozen;
	bool				mCursorHidden;
	bool				mHideCursorPermanent;
	// Is this window currently clipping the mouse ?
	bool				mIsMouseClipping;

	// "Primary" (mouse selection) clipboard buffer for systems without one
	// such clipboard.
	LLWString			mPrimaryClipboard;
};

// LLSplashScreen
// A simple, OS-specific splash screen that we can display while initializing
// the application and before creating a GL window

class LLSplashScreen
{
public:
	LLSplashScreen()								{}
	virtual ~LLSplashScreen()						{}

	// Call to display the window.
	static void show();
	static void hide();
	static void update(const std::string& string);

	static bool isVisible();

protected:
	// These are overridden by the platform implementation
	virtual void showImpl() = 0;
	virtual void updateImpl(const std::string& string) = 0;
	virtual void hideImpl() = 0;
};

// Platform-neutral for accessing the platform specific message box

// Buttons in the message box
constexpr U32 OSMB_OK = 0;
constexpr U32 OSMB_OKCANCEL = 1;
constexpr U32 OSMB_YESNO = 2;

// Returned button number
constexpr S32 OSBTN_YES = 0;
constexpr S32 OSBTN_NO = 1;
constexpr S32 OSBTN_OK = 2;
constexpr S32 OSBTN_CANCEL = 3;

S32 OSMessageBox(const std::string& text, const std::string& caption = "Error",
				 U32 type = OSMB_OK);

// Globals
extern LLWindow* gWindowp;
extern bool gDebugClicks;
extern bool gDebugWindowProc;
extern bool gHiDPISupport;

// Protocols, like "http" and "https" we support in URLs
extern const S32 gURLProtocolWhitelistCount;
extern const std::string gURLProtocolWhitelist[];

void simpleEscapeString(std::string& stringIn);

#endif // LL_LLWINDOW_H
