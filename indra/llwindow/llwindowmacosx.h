/**
 * @file llwindowmacosx.h
 * @brief Mac implementation of LLWindow class
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

#ifndef LL_LLWINDOWMACOSX_H
#define LL_LLWINDOWMACOSX_H

#include "lltimer.h"
#include "llwindow.h"
#include "llwindowmacosx-objc.h"

#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>

class LLWindowMacOSX final : public LLWindow
{
	friend class LLWindow;

protected:
	LOG_CLASS(LLWindowMacOSX);

public:
	void setWindowTitle(const std::string& title) override;
	void show() override;
	void hide() override;
	void close() override;
	void minimize() override;
	void restore() override;
	bool getVisible() override;
	bool getMinimized() override;

	// *TODO
	LL_INLINE bool getMaximized() override				{ return false; }
	LL_INLINE bool maximize() override					{ return false; }

	LL_INLINE bool getFullscreen() override				{ return mFullscreen; }

	bool getPosition(LLCoordScreen* position) override;
	bool getSize(LLCoordScreen* size) override;
	bool getSize(LLCoordWindow* size) override;
	bool setPosition(LLCoordScreen position) override;
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
	void setCursor(ECursorType c) override;
	void captureMouse() override;
	void releaseMouse() override;
	void setMouseClipping(bool b) override;
	bool isClipboardTextAvailable() override;
	bool pasteTextFromClipboard(LLWString& dst) override;
	bool copyTextToClipboard(const LLWString& src) override;
	bool isPrimaryTextAvailable() override;
	bool pasteTextFromPrimary(LLWString& text) override;
	bool copyTextToPrimary(const LLWString& text) override;
	void flashIcon(F32) override;
	// Sets the gamma
	bool setGamma(F32 gamma) override;
	// Restore original gamma table (before updating gamma)
	bool restoreGamma() override;
	LL_INLINE U32 getFSAASamples() override				{ return mFSAASamples; }
	void setFSAASamples(U32 fsaa_samples) override;

	LL_INLINE ESwapMethod getSwapMethod() override		{ return mSwapMethod; }
	void gatherInput() override;
	LL_INLINE void delayInputProcessing() override		{}
	void swapBuffers() override;

	// handy coordinate space conversion routines
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
	LL_INLINE void bringToFront() override				{}

	void allowLanguageTextInput(LLPreeditor* preeditor, bool b) override;
	void interruptLanguageTextInput() override;
	void spawnWebBrowser(const std::string& escaped_url, bool async) override;

	static std::vector<std::string> getDynamicFallbackFontList();

	LL_INLINE LLWindowCallbacks* getCallbacks()			{ return mCallbacks; }
	LL_INLINE LLPreeditor* getPreeditor()				{ return mPreeditor; }
	LL_INLINE bool allowsLanguageInput()				{ return mLanguageTextInputAllowed; }

	void updateCursor();
	void updateMouseDeltas(float* deltas);
	void getMouseDeltas(float* delta);

protected:
	LLWindowMacOSX(const std::string& title, U32 flags, bool fullscreen,
				   bool disable_vsync, U32 fsaa_samples);
	~LLWindowMacOSX() override;

	bool isValid() override;
	LLSD getNativeKeyData() override;

	F32 getSystemUISize() override;

	void initCursors();
	void moveWindow(const LLCoordScreen& position,const LLCoordScreen& size);

	//
	// Platform specific methods
	//

	// Create or re-create the GL context/window. Called from the constructor
	// and switchContext().
	bool createContext(bool fullscreen, bool disable_vsync);
	void destroyContext();
	void setupFailure(const std::string& text);

	void adjustCursorDecouple(bool warping_mouse = false);

protected:
	//
	// Platform specific variables
	//
	NSWindowRef			mWindow;
	GLViewRef			mGLView;
	CGLContextObj		mContext;
	CGLPixelFormatObj	mPixelFormat;
	CGDirectDisplayID	mDisplay;

	// Screen rect to which the mouse cursor was globally constrained before we
	// changed it in clipMouse():
	Rect				mOldMouseClip;
	std::string			mWindowTitle;
	double				mOriginalAspectRatio;
	UInt32				mLastModifiers;
	S32					mCursorLastEventDeltaX;
	S32					mCursorLastEventDeltaY;
	U32					mFSAASamples;

	// Input method management through Text Service Manager.
	LLPreeditor*		mPreeditor;
	bool				mLanguageTextInputAllowed;

	bool				mSimulatedRightClick;

	bool				mCursorDecoupled;
	bool				mCursorIgnoreNextDelta;
	bool				mMinimized;
	bool				mForceRebuild;

public:
	static bool			sUseMultGL;
};

class LLSplashScreenMacOSX final : public LLSplashScreen
{
public:
	LLSplashScreenMacOSX();

	void showImpl() override;
	void updateImpl(const std::string& mesg) override;
	void hideImpl() override;

private:
	WindowRef   mWindow;
};

S32 OSMessageBoxMacOSX(const std::string& text, const std::string& caption,
					   U32 type);

void load_url_external(const char* url);

#endif //LL_LLWINDOWMACOSX_H
