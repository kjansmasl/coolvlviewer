/**
 * @file llwindowwin32.h
 * @brief Windows implementation of LLWindow class
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

#ifndef LL_LLWINDOWWIN32_H
#define LL_LLWINDOWWIN32_H

// Limit Windows API to small and manageable set.
#include "llwin32headerslean.h"

#include "llwindow.h"

// Hack for async host by name
#define LL_WM_HOST_RESOLVED      (WM_APP + 1)
typedef void (*LLW32MsgCallback)(const MSG& msg);

class LLWindowWin32 final : public LLWindow
{
	friend class LLWindow;

protected:
	LOG_CLASS(LLWindowWin32);

public:
	void setWindowTitle(const std::string& title) override;
	void show() override;
	void hide() override;
	void close() override;
	void minimize() override;
	void restore() override;
	bool getVisible() override;
	bool getMinimized() override;
	bool getMaximized() override;
	bool maximize() override;
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
	void setCursor(ECursorType cursor) override;
	void captureMouse() override;
	void releaseMouse() override;
	void setMouseClipping(bool b) override;
	bool isClipboardTextAvailable() override;
	bool pasteTextFromClipboard(LLWString& dst) override;
	bool copyTextToClipboard(const LLWString& src) override;
	bool isPrimaryTextAvailable() override;
	bool pasteTextFromPrimary(LLWString& text) override;
	bool copyTextToPrimary(const LLWString& text) override;
	void flashIcon(F32 seconds) override;

	// Sets the gamma
	bool setGamma(F32 gamma) override;
	// Restore original gamma table (before updating gamma)
	bool restoreGamma() override;

	LL_INLINE void setFSAASamples(U32 n) override		{ mFSAASamples = n; }
	LL_INLINE U32 getFSAASamples() override				{ return mFSAASamples; }

	LL_INLINE ESwapMethod getSwapMethod() override		{ return mSwapMethod; }
	void gatherInput() override;
	void delayInputProcessing() override;
	void swapBuffers() override;

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

	void* getPlatformWindow() override;
	void bringToFront() override;
	void focusClient() override;

	void allowLanguageTextInput(LLPreeditor* preeditor, bool b) override;
	void setLanguageTextInput(const LLCoordGL& pos) override;
	void updateLanguageTextInputArea() override;
	void interruptLanguageTextInput() override;
	void spawnWebBrowser(const std::string& escaped_url, bool async) override;

	F32 getSystemUISize() override;

	static void setDPIAwareness();

	static std::vector<std::string> getDynamicFallbackFontList();

protected:
	LLWindowWin32(const std::string& title, S32 x, S32 y, U32 width,
				  U32 height, U32 flags, bool fullscreen, bool disable_vsync,
				  U32 fsaa_samples);
	~LLWindowWin32() override;

	LL_INLINE bool isValid() override					{ return mWindowHandle != NULL; }
	LLSD getNativeKeyData() override;

	void initCursors();
	void initInputDevices();
	HCURSOR loadColorCursor(LPCTSTR name);
	void moveWindow(const LLCoordScreen& position,const LLCoordScreen& size);

	// Changes display resolution. Returns true if successful
	bool setDisplayResolution(S32 width, S32 height, S32 bits, S32 refresh);

	// Go back to last fullscreen display resolution.
	bool setFullscreenResolution();

	// Restore the display resolution to its value before we ran the app.
	bool resetDisplayResolution();

	void fillCompositionForm(const LLRect& bounds, COMPOSITIONFORM* form);
	void fillCandidateForm(const LLCoordGL& caret, const LLRect& bounds,
						   CANDIDATEFORM* form);
	void fillCharPosition(const LLCoordGL& caret, const LLRect& bounds,
						  const LLRect& control, IMECHARPOSITION* char_pos);
	void fillCompositionLogfont(LOGFONT *logfont);
	U32 fillReconvertString(const LLWString& text, S32 focus, S32 focus_len,
							RECONVERTSTRING* reconvert_string);
	void handleStartCompositionMessage();
	void handleCompositionMessage(U32 indexes);
	bool handleImeRequests(WPARAM request, LPARAM param, LRESULT* result);

	//
	// Platform specific methods
	//

	bool getClientRectInScreenSpace(RECT* rectp);
	void updateJoystick();

	static LRESULT CALLBACK mainWindowProc(HWND h_wnd, UINT u_msg,
										   WPARAM w_param, LPARAM l_param);
	static BOOL CALLBACK enumChildWindows(HWND h_wnd, LPARAM l_param);

protected:	// Platform specific variables
	WCHAR*					mWindowTitle;
	WCHAR*					mWindowClassName;

	HWND					mWindowHandle;	// Window handle
	HGLRC					mhRC;			// OpenGL rendering context
	HDC						mhDC;			// Windows Device context handle
	HINSTANCE				mhInstance;		// Handle to application instance
	RECT					mOldMouseClip;  // Screen rect to which the mouse cursor was globally constrained before we changed it in clipMouse()
	WPARAM					mLastSizeWParam;
	F32						mNativeAspectRatio;

	// Array of all mouse cursors
	HCURSOR					mCursor[UI_CURSOR_COUNT];

	LPWSTR					mIconResource;

	U32						mFSAASamples;
	WORD					mPrevGammaRamp[3][256];
	WORD					mCurrentGammaRamp[3][256];

	bool					mCustomGammaSet;
	bool					mMousePositionModified;
	bool					mInputProcessingPaused;

	// Has the window class been registered ?
	static bool				sIsClassRegistered;

	// The following variables are for Language Text Input control.
	// They are all static, since one context is shared by all LLWindowWin32
	// instances.
	static bool				sLanguageTextInputAllowed;
	static bool				sWinIMEOpened;
	static HKL				sWinInputLocale;
	static DWORD			sWinIMEConversionMode;
	static DWORD			sWinIMESentenceMode;
	static LLCoordWindow	sWinIMEWindowPosition;
	LLCoordGL				mLanguageTextInputPointGL;
	LLRect					mLanguageTextInputAreaGL;

	LLPreeditor*			mPreeditor;

	U32						mKeyCharCode;
	U32						mKeyScanCode;
	U32						mKeyVirtualKey;
	U32						mRawMsg;
	U32						mRawWParam;
	U32						mRawLParam;

	bool					mMouseVanish;
};

class LLSplashScreenWin32 final : public LLSplashScreen
{
public:
	LLSplashScreenWin32();

	void showImpl() override;
	void updateImpl(const std::string& mesg) override;
	void hideImpl() override;

#if LL_WINDOWS
	static LRESULT CALLBACK windowProc(HWND h_wnd, UINT u_msg, WPARAM w_param,
									   LPARAM l_param);
#endif

private:
#if LL_WINDOWS
	HWND mWindow;
#endif
};

extern LLW32MsgCallback gAsyncMsgCallback;
extern LPWSTR gIconResource;
extern bool gIgnoreHiDPIEvents;

S32 OSMessageBoxWin32(const std::string& text, const std::string& caption, U32 type);

#endif //LL_LLWINDOWWIN32_H
