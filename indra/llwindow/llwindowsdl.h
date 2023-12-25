/**
 * @file llwindowsdl.h
 * @brief SDL implementation of LLWindow class
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

#ifndef LL_LLWINDOWSDL_H
#define LL_LLWINDOWSDL_H

// Simple Directmedia Layer (http://libsdl.org/) implementation of LLWindow
// class

#include "lltimer.h"
#include "llwindow.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_endian.h"
#include "SDL2/SDL_syswm.h"

#include <X11/Xutil.h>
// For Xlib selection routines
#include <X11/Xlib.h>
#include <X11/Xatom.h>

class LLWindowSDL final : public LLWindow
{
	friend class LLWindow;

protected:
	LOG_CLASS(LLWindowSDL);

public:
	void setWindowTitle(const std::string& title) override;
	LL_INLINE void show() override						{}
	LL_INLINE void hide() override						{}
	void close() override;
	void minimize() override;
	void restore() override;
	bool getVisible() override;
	bool getMinimized() override;

	// *TODO
	LL_INLINE bool getMaximized() override				{ return false; }
	LL_INLINE bool maximize() override					{ return false; }

	LL_INLINE bool getFullscreen() override				{ return mFullscreen; }
	bool getPosition(LLCoordScreen* pos) override;
	bool getSize(LLCoordScreen* size) override;
	bool getSize(LLCoordWindow* size) override;

	// SDL-specific method needed to force-redraw the screen after a graphics
	// benchmark or a GL shared context creation. HB.
	void refresh() override;

	// *TODO
	LL_INLINE bool setPosition(LLCoordScreen pos) override
	{
		return true;
	}

	bool setSize(LLCoordScreen size) override;
	bool switchContext(bool fullscreen, const LLCoordScreen& size,
					   bool disable_vsync,
					   const LLCoordScreen* const posp = NULL) override;
	void* createSharedContext() override;
	void makeContextCurrent(void* context) override;
	void destroySharedContext(void* context) override;
	bool setCursorPosition(const LLCoordWindow& position) override;
	bool getCursorPosition(LLCoordWindow* position) override;
	void showCursor() override;
	void hideCursor() override;
	void showCursorFromMouseMove() override;
	void hideCursorUntilMouseMove() override;
	LL_INLINE bool isCursorHidden() override			{ return mCursorHidden; }
	void setCursor(ECursorType cursor) override;
	void captureMouse() override;
	void releaseMouse() override;
	void setMouseClipping(bool b) override;

	bool isClipboardTextAvailable() override;
	bool pasteTextFromClipboard(LLWString& text) override;
	bool copyTextToClipboard(const LLWString& text) override;

	bool isPrimaryTextAvailable() override;
	bool pasteTextFromPrimary(LLWString& text) override;
	bool copyTextToPrimary(const LLWString& text) override;

	void flashIcon(F32 seconds) override;

	// Sets the gamma
	bool setGamma(F32 gamma) override;

	LL_INLINE U32 getFSAASamples() override				{ return mFSAASamples; }
	LL_INLINE void setFSAASamples(U32 n) override		{ mFSAASamples = n; }

	// Restore original gamma table (before updating gamma):
	bool restoreGamma() override;

	ESwapMethod getSwapMethod() override				{ return mSwapMethod; }
	void gatherInput() override;
	void swapBuffers() override;

	LL_INLINE void delayInputProcessing() override		{}

	// Handy coordinate space conversion routines
	bool convertCoords(LLCoordScreen from, LLCoordWindow* to) override;
	bool convertCoords(LLCoordWindow from, LLCoordScreen* to) override;
	bool convertCoords(LLCoordWindow from, LLCoordGL* to) override;
	bool convertCoords(LLCoordGL from, LLCoordWindow* to) override;
	bool convertCoords(LLCoordScreen from, LLCoordGL* to) override;
	bool convertCoords(LLCoordGL from, LLCoordScreen* to) override;

	LLWindowResolution* getSupportedResolutions(S32& num_res) override;
	F32	getNativeAspectRatio() override;
	F32 getPixelAspectRatio() override;

	void beforeDialog() override;
	void afterDialog() override;

	void* getPlatformWindow() override;
	void bringToFront() override;

	void spawnWebBrowser(const std::string& escaped_url, bool async) override;

	// *HACK: to compute window borders offsets
	void calculateBordersOffsets() override;

	bool getSelectionText(Atom selection, LLWString& text);
	bool setSelectionText(Atom selection, const LLWString& text);
	LL_INLINE LLWString& getPrimaryText()				{ return mPrimaryClipboard; }
	LL_INLINE LLWString& getSecondaryText()				{ return mSecondaryClipboard; }
	LL_INLINE void clearPrimaryText()					{ mPrimaryClipboard.clear(); }
	LL_INLINE void clearSecondaryText()					{ mSecondaryClipboard.clear(); }

	static void initXlibThreads();
	static Window getSDLXWindowID();
	static Display* getSDLDisplay();

	static std::vector<std::string> getDynamicFallbackFontList();

protected:
	LLWindowSDL(const std::string& title, S32 x, S32 y, U32 width, U32 height,
				U32 flags, bool fullscreen, bool disable_vsync,
				U32 fsaa_samples);
	~LLWindowSDL() override;

	LL_INLINE bool isValid() override					{ return mWindow != NULL; }
	LLSD getNativeKeyData() override;

	void initCursors();
	void quitCursors();
	void moveWindow(const LLCoordScreen& position, const LLCoordScreen& size);

	//
	// Platform specific methods
	//

	// Creates or re-creates the GL context/window. Called from the constructor
	// and switchContext():
	bool createContext(S32 x, S32 y, S32 width, S32 height, S32 bits,
					   bool fullscreen, bool disable_vsync);
	void destroyContext();
	void setupFailure(const std::string& text);
	void fixWindowSize();
	U32 SDLCheckGrabbyKeys(U32 keysym, bool gain);
	bool SDLReallyCaptureInput(bool capture);

	void x11_set_urgent(bool urgent);
	void initialiseX11Clipboard();

private:
	void setWindowIcon();

	// Returns true when successful, false otherwise.
	bool getFullScreenSize(S32& width, S32& height);

public:
	// Not great that these are public, but they have to be accessible
	// by non-class code and it is better than making them global.
	Window			mSDL_XWindowID;
	Display*		mSDL_Display;
	SDL_Window*		mWindow;

protected:
	//
	// Platform specific variables
	//
	SDL_GLContext	mContext;

	S32				mInitialPosX;
	S32				mInitialPosY;
	S32				mPosOffsetX;
	S32				mPosOffsetY;

	std::string		mWindowTitle;
	F32				mOriginalAspectRatio;
	U32				mFSAASamples;

	S32				mSDLFlags;

	SDL_Cursor*		mSDLCursors[UI_CURSOR_COUNT];

	U16				mPrevGammaRamp[3][256];
	U16				mCurrentGammaRamp[256];

	U32				mKeyModifiers;
	U32				mKeyVirtualKey;

	LLWString		mSecondaryClipboard;

	U32				mGrabbyKeyFlags;
	bool			mCaptured;

	LLTimer			mFlashTimer;
	bool			mFlashing;

	bool			mCustomGammaSet;
};

class LLSplashScreenSDLImpl;

class LLSplashScreenSDL final : public LLSplashScreen
{
public:
	LLSplashScreenSDL();
	~LLSplashScreenSDL() override;

	void showImpl() override;
	void updateImpl(const std::string& mesg) override;
	void hideImpl() override;

private:
	LLSplashScreenSDLImpl*	mImpl;
};

S32 OSMessageBoxSDL(const std::string& text, const std::string& caption,
					U32 type);

extern bool gXlibThreadSafe;
extern bool gXWayland;
extern bool gUseFullDesktop;

#endif // LL_LLWINDOWSDL_H
