/**
 * @file llwindowwin32.cpp
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

#include "indra_constants.h"

#if LL_WINDOWS

 // DO NOT CHANGE THE ORDER WITH OTHER INCLUDES !
#include "llwindowwin32.h"

#include "llkeyboardwin32.h"
#include "llpreeditor.h"

#include "lldir.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llsdutil.h"
#include "llstring.h"

#include <commdlg.h>
#include <WinUser.h>
#include <mapi.h>
#include <process.h>	// For _spawn
#include <shellapi.h>
#include <Imm.h>

// Require DirectInput version 8
#define DIRECTINPUT_VERSION 0x0800

#ifndef WM_DPICHANGED
# define WM_DPICHANGED 0x02E0
#endif
#ifndef USER_DEFAULT_SCREEN_DPI
# define USER_DEFAULT_SCREEN_DPI 96
#endif

#include <dinput.h>
#include <Dbt.h.>

// Expose desired use of high-performance graphics processor to Optimus driver
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

// *HACK: allow the user to ignore HiDPI WM events (for resetting the GPU after
// wiewer exit, in case of black screen).
bool gIgnoreHiDPIEvents = false;

// Culled from winuser.h
#ifndef WM_MOUSEWHEEL /* Added to be compatible with later SDK's */
constexpr S32 WM_MOUSEWHEEL = 0x020A;
#endif
#ifndef WHEEL_DELTA /* Added to be compatible with later SDK's */
constexpr S32 WHEEL_DELTA = 120;     /* Value for rolling one detent */
#endif
constexpr S32 MAX_MESSAGE_PER_UPDATE = 20;
constexpr S32 BITS_PER_PIXEL = 32;
constexpr S32 MAX_NUM_RESOLUTIONS = 32;
constexpr F32 ICON_FLASH_TIME = 0.5f;

LPWSTR gIconResource = IDI_APPLICATION;

LLW32MsgCallback gAsyncMsgCallback = NULL;

#ifndef DPI_ENUMS_DECLARED
typedef enum PROCESS_DPI_AWARENESS
{
	PROCESS_DPI_UNAWARE = 0,
	PROCESS_SYSTEM_DPI_AWARE = 1,
	PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE
{
	MDT_EFFECTIVE_DPI = 0,
	MDT_ANGULAR_DPI = 1,
	MDT_RAW_DPI = 2,
	MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#endif

typedef HRESULT(STDAPICALLTYPE* SetProcessDpiAwarenessType)(_In_ PROCESS_DPI_AWARENESS value);

typedef HRESULT(STDAPICALLTYPE* GetProcessDpiAwarenessType)(_In_ HANDLE hprocess,
															_Out_ PROCESS_DPI_AWARENESS* value);

typedef HRESULT(STDAPICALLTYPE* GetDpiForMonitorType)(_In_ HMONITOR hmonitor,
													  _In_ MONITOR_DPI_TYPE type,
													  _Out_ UINT* dpiX,
													  _Out_ UINT* dpiY);

//
// LLWindowWin32
//

void show_window_creation_error(const std::string& title)
{
	llwarns << title << llendl;
}

HGLRC SafeCreateContext(HDC& hdc)
{
	__try
	{
		return wglCreateContext(hdc);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return NULL;
	}
}

GLuint SafeChoosePixelFormat(HDC& hdc, const PIXELFORMATDESCRIPTOR* ppfd)
{
	__try
	{
		return ChoosePixelFormat(hdc, ppfd);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// Convert to C++ styled exception. C exception do not allow classes,
		// so it is a regular char array.
		char integer_string[32];
		sprintf(integer_string, "SEH, code: %lu\n", GetExceptionCode());
		throw std::exception(integer_string);
	}
}

//static
bool LLWindowWin32::sIsClassRegistered = false;

bool LLWindowWin32::sLanguageTextInputAllowed = true;
bool LLWindowWin32::sWinIMEOpened = false;
HKL LLWindowWin32::sWinInputLocale = 0;
DWORD LLWindowWin32::sWinIMEConversionMode = IME_CMODE_NATIVE;
DWORD LLWindowWin32::sWinIMESentenceMode = IME_SMODE_AUTOMATIC;
LLCoordWindow LLWindowWin32::sWinIMEWindowPosition(-1, -1);

LLWindowWin32::LLWindowWin32(const std::string& title, S32 x, S32 y, U32 width,
							 U32 height, U32 flags, bool fullscreen,
							 bool disable_vsync, U32 fsaa_samples)
:	LLWindow(fullscreen, flags),
	mWindowHandle(NULL),
	mhDC(NULL),
	mhRC(NULL),
	mFSAASamples(fsaa_samples),
	mIconResource(gIconResource),
	mNativeAspectRatio(0.f),
	mMousePositionModified(false),
	mInputProcessingPaused(false),
	mCustomGammaSet(false),
	mPreeditor(NULL),
	mKeyCharCode(0),
	mKeyScanCode(0),
	mKeyVirtualKey(0),
	mRawMsg(0),
	mRawWParam(0),
	mRawLParam(0)
{
	// MAINT-516: force-load opengl32.dll just in case windows went sideways
	LoadLibrary(L"opengl32.dll");

	memset(mCurrentGammaRamp, 0, sizeof(mCurrentGammaRamp));
	memset(mPrevGammaRamp, 0, sizeof(mPrevGammaRamp));

	if (!SystemParametersInfo(SPI_GETMOUSEVANISH, 0, &mMouseVanish, 0))
	{
		mMouseVanish = true;
	}

	// Initialize the keyboard
	gKeyboardp = new LLKeyboardWin32();

	// Initialize (boot strap) the language text input management, based on the
	// system (or user's) default settings.
	allowLanguageTextInput(mPreeditor, false);

	// Set the window title
	if (title.empty())
	{
		mWindowTitle = new WCHAR[50];
		wsprintf(mWindowTitle, L"OpenGL Window");
	}
	else
	{
		mWindowTitle = new WCHAR[256]; // Assume title length < 255 chars.
		mbstowcs(mWindowTitle, title.c_str(), 255);
		mWindowTitle[255] = 0;
	}

	// Set the window class name to "Second Life" so that it will always be
	// found when being sent an SLURL by any other viewer instance wia
	// LLAppViewerWin32::sendURLToOtherInstance()
	mWindowClassName = new WCHAR[256];
	mWindowClassName[255] = 0;
	wsprintf(mWindowClassName, L"Second Life");

	// We are not clipping yet
	SetRect(&mOldMouseClip, 0, 0, 0, 0);

	// Make an instance of our window then define the window class
	mhInstance = GetModuleHandle(NULL);

	mSwapMethod = SWAP_METHOD_UNDEFINED;

	// No WPARAM yet.
	mLastSizeWParam = 0;

	// Windows GDI rects do not include rightmost pixel
	RECT window_rect;
	window_rect.left = 0L;
	window_rect.right = (long)width;
	window_rect.top = 0L;
	window_rect.bottom = (long)height;

	// Grab screen size to sanitize the window
	S32 window_border_y = GetSystemMetrics(SM_CYBORDER);
	S32 virtual_screen_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
	S32 virtual_screen_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
	S32 virtual_screen_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	S32 virtual_screen_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	if (x < virtual_screen_x)
	{
		x = virtual_screen_x;
	}
	if (y < virtual_screen_y - window_border_y)
	{
		y = virtual_screen_y - window_border_y;
	}
	if (x + width > virtual_screen_x + virtual_screen_width)
	{
		x = virtual_screen_x + virtual_screen_width - width;
	}
	if (y + height > virtual_screen_y + virtual_screen_height)
	{
		y = virtual_screen_y + virtual_screen_height - height;
	}

	WNDCLASS wc;
	if (!sIsClassRegistered)
	{
		// Force redraw when resized and create a private device context

		// Makes double click messages.
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;

		// Set message handler function
		wc.lpfnWndProc = (WNDPROC)mainWindowProc;

		// Unused
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;

		wc.hInstance = mhInstance;
		wc.hIcon = LoadIcon(mhInstance, mIconResource);

		// We will set the cursor ourselves
		wc.hCursor = NULL;

		// Use a black background. HB
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

		// We do not use windows menus
		wc.lpszMenuName = NULL;

		wc.lpszClassName = mWindowClassName;

		if (!RegisterClass(&wc))
		{
			OSMessageBox("RegisterClass failed", "Error", OSMB_OK);
			return;
		}
		sIsClassRegistered = true;
	}

	// Get the current refresh rate
	DEVMODE dev_mode;
	::ZeroMemory(&dev_mode, sizeof(DEVMODE));
	dev_mode.dmSize = sizeof(DEVMODE);
	DWORD current_refresh;
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode))
	{
		current_refresh = dev_mode.dmDisplayFrequency;
		mNativeAspectRatio = (F32)dev_mode.dmPelsWidth /
			(F32)dev_mode.dmPelsHeight;
	}
	else
	{
		current_refresh = 60;
	}

	// Drop resolution and go fullscreen. Use a display mode with our desired
	// size and depth, with a refresh rate as close at possible to the user's
	// default
	if (mFullscreen)
	{
		bool success = false;
		DWORD closest_refresh = 0;
		S32 mode_num = 0;
		while (EnumDisplaySettings(NULL, mode_num++, &dev_mode))
		{
			if (dev_mode.dmPelsWidth == width &&
				dev_mode.dmPelsHeight == height &&
				dev_mode.dmBitsPerPel == BITS_PER_PIXEL)
			{
				success = true;
				if (closest_refresh == 0 ||
					dev_mode.dmDisplayFrequency - current_refresh
					< closest_refresh - current_refresh)
				{
					closest_refresh = dev_mode.dmDisplayFrequency;
				}
			}
		}

		if (!success)
		{
			llwarns << "Could not find display mode " << width << " by "
					<< height << " at " << BITS_PER_PIXEL << " bits per pixel"
					<< llendl;

			if (!EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode))
			{
				success = false;
			}
			else if (dev_mode.dmBitsPerPel == BITS_PER_PIXEL)
			{
				window_rect.right = width = dev_mode.dmPelsWidth;
				window_rect.bottom = height = dev_mode.dmPelsHeight;
				llwarns << "Current BBP is OK falling back to: " << width
						<< "x" << height << llendl;
				success = true;
			}
			else
			{
				llwarns << "Current BBP is BAD: " << dev_mode.dmBitsPerPel
						<< llendl;
				success = false;
			}
		}

		// If we found a good resolution, use it.
		if (success)
		{
			success = setDisplayResolution(width, height, BITS_PER_PIXEL,
				closest_refresh);
		}

		// Keep a copy of the actual current device mode in case we minimize
		// and change the screen resolution.   JC
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode);

		// If it failed, we do not want to run fullscreen
		if (success)
		{
			mFullscreen = true;
			mFullscreenWidth = dev_mode.dmPelsWidth;
			mFullscreenHeight = dev_mode.dmPelsHeight;
			mFullscreenBits = dev_mode.dmBitsPerPel;
			mFullscreenRefresh = dev_mode.dmDisplayFrequency;

			llinfos << "Running at " << dev_mode.dmPelsWidth << "x"
					<< dev_mode.dmPelsHeight << "x" << dev_mode.dmBitsPerPel
					<< " @ " << dev_mode.dmDisplayFrequency << llendl;
		}
		else
		{
			mFullscreen = false;
			mFullscreenWidth = -1;
			mFullscreenHeight = -1;
			mFullscreenBits = -1;
			mFullscreenRefresh = -1;

			std::string error =
				llformat("Unable to run fullscreen at %d x %d.\nRunning in window.",
						 width, height);
			OSMessageBox(error, "Error", OSMB_OK);
		}
	}

#if 0	// *TODO: add this after resolving _WIN32_WINNT issue
	if (!fullscreen)
	{
		TRACKMOUSEEVENT track_mouse_event;
		track_mouse_event.cbSize = sizeof(TRACKMOUSEEVENT);
		track_mouse_event.dwFlags = TME_LEAVE;
		track_mouse_event.hwndTrack = mWindowHandle;
		track_mouse_event.dwHoverTime = HOVER_DEFAULT;
		TrackMouseEvent(&track_mouse_event);
	}
#endif

	// Create GL drawing context
	LLCoordScreen win_pos(x, y);
	LLCoordScreen win_size(window_rect.right - window_rect.left,
						   window_rect.bottom - window_rect.top);
	if (!switchContext(mFullscreen, win_size, disable_vsync, &win_pos))
	{
		return;
	}

	// Start with arrow cursor
	initCursors();
	setCursor(UI_CURSOR_ARROW);

	// Initialize (boot strap) the Language text input management, based on the
	// system (or user's) default settings.
	allowLanguageTextInput(NULL, false);
}

LLWindowWin32::~LLWindowWin32()
{
	delete[] mWindowTitle;
	mWindowTitle = NULL;

	delete[] mSupportedResolutions;
	mSupportedResolutions = NULL;

	delete[] mWindowClassName;
	mWindowClassName = NULL;
}

void LLWindowWin32::setWindowTitle(const std::string& title)
{
	// Remember the new title, for when we switch context
	delete mWindowTitle;
	mWindowTitle = new WCHAR[256]; // Assume title length < 255 chars.
	mbstowcs(mWindowTitle, title.c_str(), 255);
	mWindowTitle[255] = 0;

	int len = title.size() + 1;
	wchar_t* wText = new wchar_t[len];
	if (wText)
	{
		memset(wText, 0, len);
		MultiByteToWideChar(CP_ACP, NULL, title.c_str(), -1, wText, len);
		SetWindowText(mWindowHandle, wText);
		delete[] wText;
	}
}

void LLWindowWin32::show()
{
	ShowWindow(mWindowHandle, SW_SHOW);
	SetForegroundWindow(mWindowHandle);
	SetFocus(mWindowHandle);
}

void LLWindowWin32::hide()
{
	setMouseClipping(false);
	ShowWindow(mWindowHandle, SW_HIDE);
}

void LLWindowWin32::minimize()
{
	setMouseClipping(false);
	showCursor();
	ShowWindow(mWindowHandle, SW_MINIMIZE);
}

void LLWindowWin32::restore()
{
	ShowWindow(mWindowHandle, SW_RESTORE);
	SetForegroundWindow(mWindowHandle);
	SetFocus(mWindowHandle);
}

// According to callstack "c0000005 Access violation" happened inside __try
// block, deep in DestroyWindow and crashed viewer, which should not be
// possible. Manually causing this exception was caught without issues, so
// turning off optimizations for this part to be sure code executes as intended
// (no idea why else __try can get overruled).
#pragma optimize("", off)
bool destroy_window_handler(HWND& hwnd)
{
	bool res = true;
	__try
	{
		if (hwnd)
		{
			res = DestroyWindow(hwnd);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		res = false;
	}
	return res;
}
#pragma optimize("", on)

// close() destroys all OS-specific code associated with a window.
// Usually called from LLWindow::destroyWindow()
void LLWindowWin32::close()
{
	// Is window is already closed ?
	if (!mWindowHandle)
	{
		return;
	}

	LL_DEBUGS("Window") << "Closing window..." << LL_ENDL;

	// Go back to screen mode written in the registry.
	if (mFullscreen)
	{
		minimize();
		resetDisplayResolution();
	}

#if 1
	// Do not process events in our mainWindowProc any longer.
	LL_DEBUGS("Window") << "Stopping WM events processing." << LL_ENDL;
	SetWindowLongPtr(mWindowHandle, GWLP_USERDATA, NULL);
#endif

	// Make sure cursor is visible and we have not mangled the clipping state.
	showCursor();
	// Make sure cursor is visible and we have not mangled the clipping state.
	setMouseClipping(false);

	if (gKeyboardp)
	{
		gKeyboardp->resetKeys();
	}

	// Clean up remaining GL state
	if (gGLManager.mInited)
	{
		LL_DEBUGS("Window") << "Shutting down GL" << LL_ENDL;
		gGLManager.shutdownGL();
	}

	LL_DEBUGS("Window") << "Releasing Context" << LL_ENDL;
	if (mhRC)
	{
		if (!wglMakeCurrent(NULL, NULL))
		{
			llwarns << "Release of DC and RC failed" << llendl;
		}

		if (!wglDeleteContext(mhRC))
		{
			llwarns << "Release of rendering context failed" << llendl;
		}

		mhRC = NULL;
	}

	// Restore gamma to the system values.
	restoreGamma();

	if (mhDC)
	{
		if (!ReleaseDC(mWindowHandle, mhDC))
		{
			llwarns << "Release of ghDC failed" << llendl;
		}
		mhDC = NULL;
	}

	LL_DEBUGS("Window") << "Destroying Window" << LL_ENDL;

	if (IsWindow(mWindowHandle))
	{
		// Make sure we do not leave a blank toolbar button.
		ShowWindow(mWindowHandle, SW_HIDE);

		// This causes WM_DESTROY to be sent *immediately*
		if (!destroy_window_handler(mWindowHandle))
		{
			OSMessageBox("DestroyWindow(mWindowHandle) failed",
						 "Shutdown Error", OSMB_OK);
		}
	}
	else
	{
		// Something killed the window while we were busy destroying GL or the
		// handle somehow got broken.
		llwarns << "Failed to destroy Window, invalid handle !" << llendl;
	}

	mWindowHandle = NULL;
}

bool LLWindowWin32::getVisible()
{
	return mWindowHandle && IsWindowVisible(mWindowHandle);
}

bool LLWindowWin32::getMinimized()
{
	return mWindowHandle && IsIconic(mWindowHandle);
}

bool LLWindowWin32::getMaximized()
{
	return mWindowHandle && IsZoomed(mWindowHandle);
}

bool LLWindowWin32::maximize()
{
	if (!mWindowHandle) return false;

	WINDOWPLACEMENT placement;
	placement.length = sizeof(WINDOWPLACEMENT);

	bool success = GetWindowPlacement(mWindowHandle, &placement);
	if (success)
	{
		placement.showCmd = SW_MAXIMIZE;
		success = SetWindowPlacement(mWindowHandle, &placement);
	}

	return success;
}

bool LLWindowWin32::getPosition(LLCoordScreen* position)
{
	RECT window_rect;

	if (!position || !mWindowHandle ||
		!GetWindowRect(mWindowHandle, &window_rect))
	{
		return false;
	}

	position->mX = window_rect.left;
	position->mY = window_rect.top;
	return true;
}

bool LLWindowWin32::getSize(LLCoordScreen* size)
{
	RECT window_rect;

	if (!size || !mWindowHandle ||
		!GetWindowRect(mWindowHandle, &window_rect))
	{
		return false;
	}

	size->mX = window_rect.right - window_rect.left;
	size->mY = window_rect.bottom - window_rect.top;
	return true;
}

bool LLWindowWin32::getSize(LLCoordWindow* size)
{
	RECT client_rect;

	if (!size || !mWindowHandle ||
		!GetClientRect(mWindowHandle, &client_rect))
	{
		return false;
	}

	size->mX = client_rect.right - client_rect.left;
	size->mY = client_rect.bottom - client_rect.top;
	return true;
}

bool LLWindowWin32::setPosition(const LLCoordScreen position)
{
	if (mWindowHandle)
	{
		LLCoordScreen size;
		getSize(&size);
		moveWindow(position, size);
		return true;
	}
	return false;
}

bool LLWindowWin32::setSize(const LLCoordScreen size)
{
	if (mWindowHandle)
	{
		LLCoordScreen position;
		getPosition(&position);
		moveWindow(position, size);
		return true;
	}
	return false;
}

// Changing fullscreen resolution
bool LLWindowWin32::switchContext(bool fullscreen, const LLCoordScreen& size,
								  bool disable_vsync,
								  const LLCoordScreen* const posp)
{
	GLuint pixel_format;
	DEVMODE dev_mode;
	::ZeroMemory(&dev_mode, sizeof(DEVMODE));
	dev_mode.dmSize = sizeof(DEVMODE);
	DWORD current_refresh, dw_ex_style, dw_style;
	RECT window_rect = { 0, 0, 0, 0 };
	S32 width = size.mX;
	S32 height = size.mY;

	bool auto_show = false;
	if (mhRC)
	{
		auto_show = true;
		resetDisplayResolution();
	}

	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode))
	{
		current_refresh = dev_mode.dmDisplayFrequency;
	}
	else
	{
		current_refresh = 60;
	}

	gGLManager.shutdownGL();

	// Destroy GL context
	if (mhRC)
	{
		if (!wglMakeCurrent(NULL, NULL))
		{
			llwarns << "Release of DC and RC failed" << llendl;
		}

		if (!wglDeleteContext(mhRC))
		{
			llwarns << "Release of rendering context failed" << llendl;
		}

		mhRC = NULL;
	}

	if (fullscreen)
	{
		mFullscreen = true;
		bool success = false;
		DWORD closest_refresh = 0;
		S32 mode_num = 0;
		while (EnumDisplaySettings(NULL, mode_num++, &dev_mode))
		{
			if (dev_mode.dmPelsWidth == width &&
				dev_mode.dmPelsHeight == height &&
				dev_mode.dmBitsPerPel == BITS_PER_PIXEL)
			{
				success = true;
				if (closest_refresh == 0 ||
					dev_mode.dmDisplayFrequency - current_refresh <
					closest_refresh - current_refresh)
				{
					closest_refresh = dev_mode.dmDisplayFrequency;
				}
			}
		}

		if (!success)
		{
			llwarns << "Could not find display mode " << width << " by "
					<< height << " at " << BITS_PER_PIXEL << " bits per pixel"
					<< llendl;
			return false;
		}

		// If we found a good resolution, use it.
		success = setDisplayResolution(width, height, BITS_PER_PIXEL,
			closest_refresh);

		// Keep a copy of the actual current device mode in case we minimize
		// and change the screen resolution.   JC
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode);

		if (success)
		{
			mFullscreen = true;
			mFullscreenWidth = dev_mode.dmPelsWidth;
			mFullscreenHeight = dev_mode.dmPelsHeight;
			mFullscreenBits = dev_mode.dmBitsPerPel;
			mFullscreenRefresh = dev_mode.dmDisplayFrequency;

			llinfos << "Running at " << dev_mode.dmPelsWidth << "x"
					<< dev_mode.dmPelsHeight << "x" << dev_mode.dmBitsPerPel
					<< " @ " << dev_mode.dmDisplayFrequency << llendl;

			window_rect.left = 0L;
			// Windows GDI rects do not include rightmost pixel:
			window_rect.right = (long)width;
			window_rect.top = 0L;
			window_rect.bottom = (long)height;
			dw_ex_style = WS_EX_APPWINDOW;
			dw_style = WS_POPUP;

			// Move window borders out not to cover window contents
			AdjustWindowRectEx(&window_rect, dw_style, FALSE, dw_ex_style);
		}
		// If it failed, we do not want to run fullscreen
		else
		{
			mFullscreen = false;
			mFullscreenWidth = -1;
			mFullscreenHeight = -1;
			mFullscreenBits = -1;
			mFullscreenRefresh = -1;
			llinfos << "Unable to run fullscreen at " << width << "x" << height
					<< llendl;
			return false;
		}
	}
	else
	{
		mFullscreen = false;
		window_rect.left = (long)(posp ? posp->mX : 0);
		// Windows GDI rects do not include rightmost pixel:
		window_rect.right = (long)width + window_rect.left;
		window_rect.top = (long)(posp ? posp->mY : 0);
		window_rect.bottom = (long)height + window_rect.top;
		// Window with an edge
		dw_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		dw_style = WS_OVERLAPPEDWINDOW;
	}

	// Do not post quit messages when destroying old windows
	mPostQuit = false;

	if (mWindowHandle && !destroy_window_handler(mWindowHandle))
	{
		llwarns << "Failed to properly close window before recreating it"
				<< llendl;
	}

	// Create window
	destroy_window_handler(mWindowHandle);
	mWindowHandle =
		CreateWindowEx(dw_ex_style, mWindowClassName, mWindowTitle,
					   WS_CLIPSIBLINGS | WS_CLIPCHILDREN | dw_style,
					   window_rect.left,						// x pos
					   window_rect.top,							// y pos
					   window_rect.right - window_rect.left,	// width
					   window_rect.bottom - window_rect.top,	// height
					   NULL, NULL, mhInstance, NULL);
	if (mWindowHandle)
	{
		llinfos << "Window has been created." << llendl;
	}
	else
	{
		llwarns << "Failed to create window. Error code: " << GetLastError()
				<< llendl;
	}

	//-------------------------------------------------------------------------
	// Create GL drawing context
	//-------------------------------------------------------------------------
	static PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
			PFD_TYPE_RGBA,
			BITS_PER_PIXEL,
			0, 0, 0, 0, 0, 0,	// RGB bits and shift, unused
			8,					// alpha bits
			0,					// alpha shift
			0,					// accum bits
			0, 0, 0, 0,			// accum RGBA
			24,					// depth bits
			8,					// stencil bits, avi added for stencil test
			0,
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
	};

	if (!(mhDC = GetDC(mWindowHandle)))
	{
		OSMessageBox("Cannot make GL device context", "Error", OSMB_OK);
		close();
		return false;
	}

	try
	{
		// ChoosePixelFormat may crash in case of faulty driver
		if (!(pixel_format = SafeChoosePixelFormat(mhDC, &pfd)))
		{
			OSMessageBox("Cannot find suitable pixel format", "Error",
						 OSMB_OK);
			close();
			return false;
		}
	}
	catch (...)
	{
		llwarns << "ChoosePixelFormat() failed, with error code: "
				<< GetLastError() << llendl;
		OSMessageBox("Error while selecting pixel format", "Error",
					 OSMB_OK);
		close();
		return false;
	}

	// Verify what pixel format we actually received.
	if (!DescribePixelFormat(mhDC, pixel_format, sizeof(PIXELFORMATDESCRIPTOR),
							 &pfd))
	{
		OSMessageBox("Cannot get pixel format description", "Error", OSMB_OK);
		close();
		return false;
	}

	if (pfd.cColorBits < 32)
	{
		OSMessageBox(
			"The viewer requires True Color (32 bits) to run in a window.\n"
			"Please go to Control Panels -> Display -> Settings and\n"
			"set the screen to 32 bits color.\n"
			"Alternately, if you choose to run fullscreen, The viewer\n"
			"will automatically adjust the screen each time it runs.",
			"Error",
			OSMB_OK);
		close();
		return false;
	}

	if (pfd.cAlphaBits < 8)
	{
		OSMessageBox(
			"The viewer is unable to run because it cannot get an 8 bit alpha\n"
			"channel.  Usually this is due to video card driver issues.\n"
			"Please make sure you have the latest video card drivers installed.\n"
			"Also be sure your monitor is set to True Color (32 bits) in\n"
			"Control Panels -> Display -> Settings.\n"
			"If you continue to receive this message, contact customer service.",
			"Error",
			OSMB_OK);
		close();
		return false;
	}

	if (!SetPixelFormat(mhDC, pixel_format, &pfd))
	{
		OSMessageBox("Cannot set pixel format", "Error", OSMB_OK);
		close();
		return false;
	}

	if (!(mhRC = SafeCreateContext(mhDC)))
	{
		OSMessageBox("Cannot create GL rendering context", "Error", OSMB_OK);
		close();
		return false;
	}

	if (!wglMakeCurrent(mhDC, mhRC))
	{
		OSMessageBox("Cannot activate GL rendering context", "Error", OSMB_OK);
		close();
		return false;
	}

	gGLManager.initWGL(mhDC);

	HWND oldWND = NULL;
	HDC oldDC = NULL;
	HGLRC oldRC = NULL;
	if (epoxy_has_wgl_extension(mhDC, "WGL_ARB_pixel_format"))
	{
		// OK, at this point, use the ARB wglChoosePixelFormatsARB function to
		// see if we can get exactly what we want.
		GLint attrib_list[256];
		S32 cur_attrib = 0;

		attrib_list[cur_attrib++] = WGL_DEPTH_BITS_ARB;
		attrib_list[cur_attrib++] = 24;

		attrib_list[cur_attrib++] = WGL_STENCIL_BITS_ARB;
		attrib_list[cur_attrib++] = 8;

		attrib_list[cur_attrib++] = WGL_DRAW_TO_WINDOW_ARB;
		attrib_list[cur_attrib++] = GL_TRUE;

		attrib_list[cur_attrib++] = WGL_ACCELERATION_ARB;
		attrib_list[cur_attrib++] = WGL_FULL_ACCELERATION_ARB;

		attrib_list[cur_attrib++] = WGL_SUPPORT_OPENGL_ARB;
		attrib_list[cur_attrib++] = GL_TRUE;

		attrib_list[cur_attrib++] = WGL_DOUBLE_BUFFER_ARB;
		attrib_list[cur_attrib++] = GL_TRUE;

		attrib_list[cur_attrib++] = WGL_COLOR_BITS_ARB;
		attrib_list[cur_attrib++] = 24;

		attrib_list[cur_attrib++] = WGL_ALPHA_BITS_ARB;
		attrib_list[cur_attrib++] = 8;

		U32 end_attrib = 0;
		if (mFSAASamples > 0)
		{
			end_attrib = cur_attrib;
			attrib_list[cur_attrib++] = WGL_SAMPLE_BUFFERS_ARB;
			attrib_list[cur_attrib++] = GL_TRUE;

			attrib_list[cur_attrib++] = WGL_SAMPLES_ARB;
			attrib_list[cur_attrib++] = mFSAASamples;
		}

		// End the list
		attrib_list[cur_attrib++] = 0;

		GLint pixel_formats[256];
		U32 num_formats = 0;

		// First we try and get a 32 bit depth pixel format
		bool result = (bool)wglChoosePixelFormatARB(mhDC, attrib_list, NULL,
													256, pixel_formats,
													&num_formats);

		while (!result && mFSAASamples > 0)
		{
			llwarns << "FSAASamples: " << mFSAASamples << " not supported."
					<< llendl;

			// Try to decrease sample pixel number until to disable
			// anti-aliasing
			mFSAASamples /= 2;
			if (mFSAASamples < 2)
			{
				mFSAASamples = 0;
			}

			if (mFSAASamples > 0)
			{
				attrib_list[end_attrib + 3] = mFSAASamples;
			}
			else
			{
				cur_attrib = end_attrib;
				end_attrib = 0;
				attrib_list[cur_attrib++] = 0; // End
			}
			result = wglChoosePixelFormatARB(mhDC, attrib_list, NULL, 256,
				pixel_formats, &num_formats);

			if (result)
			{
				llwarns << "Only support FSAASamples: " << mFSAASamples << llendl;
			}
		}

		if (!result)
		{
			show_window_creation_error("Error after wglChoosePixelFormatARB 32 bits");
			close();
			return false;
		}

		if (!num_formats)
		{
			if (end_attrib > 0)
			{
				llinfos << "No valid pixel format for " << mFSAASamples
						<< "x anti-aliasing." << llendl;
				attrib_list[end_attrib] = 0;

				if (!wglChoosePixelFormatARB(mhDC, attrib_list, NULL, 256,
					pixel_formats, &num_formats))
				{
					show_window_creation_error("Error after wglChoosePixelFormatARB 32 bits no AA");
					close();
					return false;
				}
			}

			if (!num_formats)
			{
				llinfos << "No 32 bit z-buffer, trying 24 bits instead"
						<< llendl;
				// Try 24-bit format
				attrib_list[1] = 24;
				if (!wglChoosePixelFormatARB(mhDC, attrib_list, NULL, 256,
											 pixel_formats, &num_formats))
				{
					show_window_creation_error("Error after wglChoosePixelFormatARB 24-bit");
					close();
					return false;
				}

				if (!num_formats)
				{
					llwarns << "Could not get 24 bit z-buffer,trying 16 bits instead !"
							<< llendl;
					attrib_list[1] = 16;
					bool result = (bool)wglChoosePixelFormatARB(mhDC,
																attrib_list,
																NULL, 256,
																pixel_formats,
																&num_formats);
					if (!result || !num_formats)
					{
						show_window_creation_error("Error after wglChoosePixelFormatARB 16-bit");
						close();
						return false;
					}
				}
			}

			llinfos << "Choosing pixel formats: " << num_formats
					<< " pixel formats returned" << llendl;
		}

		// SL-14705: fix name tags showing in front of objects with AMD GPUs.
		// On AMD hardware we need to iterate from the first pixel format to
		// the end. Reference:
		// https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt
		const S32 max_format = (S32)num_formats - 1;
		S32 cur_format = 0;
		S32 swap_method = 0;
		GLint swap_query = WGL_SWAP_METHOD_ARB;
		while (wglGetPixelFormatAttribivARB(mhDC, pixel_formats[cur_format], 0,
											1, &swap_query, &swap_method))
		{
			if (swap_method == WGL_SWAP_UNDEFINED_ARB)
			{
				break;
			}
			if (cur_format >= max_format)
			{
				cur_format = 0;
				break;
			}
			++cur_format;
		}

		pixel_format = pixel_formats[cur_format];

		if (mWindowHandle)
		{
			if (mhDC)				// Does the window have a device context ?
			{
				if (mhRC)
				{
					oldRC = mhRC;
					mhRC = NULL;	// Zero the rendering context
				}
				oldDC = mhDC;
				mhDC = NULL;		// Zero the device context
			}
			oldWND = mWindowHandle;
		}

		mWindowHandle = CreateWindowEx(dw_ex_style, mWindowClassName,
									   mWindowTitle,
									   WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
									   dw_style,
									   window_rect.left, window_rect.top,
									   window_rect.right - window_rect.left,
									   window_rect.bottom - window_rect.top,
									   NULL, NULL, mhInstance, NULL);
		if (mWindowHandle)
		{
			llinfos << "Window has been recreated." << llendl;
		}
		else
		{
			llwarns << "Failed to recreate window). Error code: "
					<< GetLastError() << llendl;
		}

		if (!(mhDC = GetDC(mWindowHandle)))
		{
			OSMessageBox("Cannot make GL device context", "Error", OSMB_OK);
			close();
			return false;
		}

		if (!SetPixelFormat(mhDC, pixel_format, &pfd))
		{
			OSMessageBox("Cannot set pixel format", "Error", OSMB_OK);
			close();
			return false;
		}

		if (wglGetPixelFormatAttribivARB(mhDC, pixel_format, 0, 1, &swap_query,
			&swap_method))
		{
			switch (swap_method)
			{
			case WGL_SWAP_EXCHANGE_ARB:
				mSwapMethod = SWAP_METHOD_EXCHANGE;
				LL_DEBUGS("Window") << "Swap Method: Exchange" << LL_ENDL;
				break;

			case WGL_SWAP_COPY_ARB:
				mSwapMethod = SWAP_METHOD_COPY;
				LL_DEBUGS("Window") << "Swap Method: Copy" << LL_ENDL;
				break;

			case WGL_SWAP_UNDEFINED_ARB:
			default:
				mSwapMethod = SWAP_METHOD_UNDEFINED;
				LL_DEBUGS("Window") << "Swap Method: Undefined" << LL_ENDL;
			}
		}
	}
	else
	{
		llwarns << "No wgl_ARB_pixel_format extension, using default ChoosePixelFormat "
				<< llendl;
	}

	// Verify what pixel format we actually received.
	if (!DescribePixelFormat(mhDC, pixel_format, sizeof(PIXELFORMATDESCRIPTOR),
		&pfd))
	{
		OSMessageBox("Cannot get pixel format description", "Error", OSMB_OK);
		close();
		return false;
	}

	llinfos << "GL buffer: Color Bits " << S32(pfd.cColorBits)
			<< " Alpha Bits " << S32(pfd.cAlphaBits) << " Depth Bits "
			<< S32(pfd.cDepthBits) << llendl;

	// Make sure we have 32 bits per pixel
	if (pfd.cColorBits < 32 || GetDeviceCaps(mhDC, BITSPIXEL) < 32)
	{
		OSMessageBox(
			"The viewer requires True Color (32 bits) to run in a window.\n"
			"Please go to Control Panels -> Display -> Settings and\n"
			"set the screen to 32 bits color.\n"
			"Alternately, if you choose to run fullscreen, The viewer\n"
			"will automatically adjust the screen each time it runs.",
			"Error",
			OSMB_OK);
		close();
		return false;
	}

	if (pfd.cAlphaBits < 8)
	{
		OSMessageBox(
			"The viewer is unable to run because it cannot get an 8 bit alpha\n"
			"channel.  Usually this is due to video card driver issues.\n"
			"Please make sure you have the latest video card drivers installed.\n"
			"Also be sure your monitor is set to True Color (32 bits) in\n"
			"Control Panels -> Display -> Settings.\n"
			"If you continue to receive this message, contact customer service.",
			"Error",
			OSMB_OK);
		close();
		return false;
	}

	mhRC = NULL;
	if (epoxy_has_wgl_extension(mhDC, "WGL_ARB_create_context"))
	{
		// Attempt to create a specific versioned context
		mhRC = (HGLRC)createSharedContext();
		if (!mhRC)
		{
			OSMessageBox("Cannot create versioned context", "Error", OSMB_OK);
			close();
			return false;
		}
	}

	if (!wglMakeCurrent(mhDC, mhRC))
	{
		OSMessageBox("Cannot activate GL rendering context", "Error", OSMB_OK);
		close();
		return false;
	}

	if (oldWND)
	{
		if (oldDC)
		{
			if (oldRC)
			{
				wglDeleteContext(oldRC);
				oldRC = NULL;
			}
			ReleaseDC(oldWND, oldDC);
			oldDC = NULL;
		}
		destroy_window_handler(oldWND);
		oldWND = NULL;
	}

	if (!gGLManager.initGL())
	{
		OSMessageBox("The viewer is unable to run because your video card drivers\n"
			"did not install properly, are out of date, or are for unsupported\n"
			"hardware. Please make sure you have the latest video card drivers\n"
			"and even if you do have the latest, try reinstalling them.\n\n"
			"If you continue to receive this message, contact customer service.",
			"Error",
			OSMB_OK);
		close();
		return false;
	}

	// Disable vertical sync for swap
	if (epoxy_has_wgl_extension(mhDC, "WGL_EXT_swap_control"))
	{
		LL_DEBUGS("Window") << (disable_vsync ? "En" : "Dis")
							<< "abling vertical sync" << LL_ENDL;
		wglSwapIntervalEXT(disable_vsync ? 0 : 1);
	}

	SetWindowLongPtr(mWindowHandle, GWLP_USERDATA, (LONG_PTR)this);

	// Register joystick timer callback
	SetTimer(mWindowHandle, 0, 1000.f / 30.f, NULL); // 30 fps timer

	// OK to post quit messages now
	mPostQuit = true;

	if (auto_show)
	{
		show();
		glClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);
		swapBuffers();
	}

	return true;
}

//virtual
void* LLWindowWin32::createSharedContext()
{
	S32 attribs[] =
	{
		// Start at 4.6
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
		WGL_CONTEXT_MINOR_VERSION_ARB, 6,
		WGL_CONTEXT_PROFILE_MASK_ARB,
			LLRender::sGLCoreProfile ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB
									 : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
		WGL_CONTEXT_FLAGS_ARB, gDebugGL ? WGL_CONTEXT_DEBUG_BIT_ARB : 0,
		0
	};

	while (true)
	{
		HGLRC rc = wglCreateContextAttribsARB(mhDC, mhRC, attribs);
		if (rc)
		{
			llinfos << "Created OpenGL "
					<< llformat("%d.%d", attribs[1], attribs[3])
					<< (LLRender::sGLCoreProfile ? " core" : " compatibility")
					<< " context." << llendl;
			return (void*)rc;
		}
		if (attribs[3] > 0)
		{
			// Decrement minor version
			--attribs[3];
		}
		else if (attribs[1] > 3)
		{
			// Decrement major version and start minor version over at 3
			--attribs[1];
			attribs[3] = 3;
		}
		else
		{
			// We reached 3.0 and still failed, bail out
			break;
		}
	}
	return (void*)wglCreateContext(mhDC);
}

//virtual
void LLWindowWin32::makeContextCurrent(void* context)
{
	if (!mhDC)
	{
		llerrs << "Trying to make a context current on a destroyed device context."
			   << llendl;
	}
	if (context)
	{
		wglMakeCurrent(mhDC, (HGLRC)context);
	}
	else
	{
		// Restore main GL thread context.
		wglMakeCurrent(mhDC, mhRC);
	}
}

//virtual
void LLWindowWin32::destroySharedContext(void* context)
{
	if (context)	// Ignore attempts to destroy invalid contexts. HB
	{
		wglDeleteContext((HGLRC)context);
	}
}

void LLWindowWin32::moveWindow(const LLCoordScreen& position,
							   const LLCoordScreen& size)
{
	if (mIsMouseClipping)
	{
		RECT client_rect_in_screen_space;
		if (getClientRectInScreenSpace(&client_rect_in_screen_space))
		{
			ClipCursor(&client_rect_in_screen_space);
		}
	}

	// If the window was already maximized, MoveWindow seems to still set the
	// maximized flag even if the window is smaller than maximized. So we're
	// going to do a restore first (which is a ShowWindow call) (SL-44655).

#if 0	// THIS CAUSES DEV-15484 and DEV-15949
	ShowWindow(mWindowHandle, SW_RESTORE);
#endif

	// NOW we can call MoveWindow
	MoveWindow(mWindowHandle, position.mX, position.mY, size.mX, size.mY,
		TRUE);
}

bool LLWindowWin32::setCursorPosition(const LLCoordWindow& position)
{
	mMousePositionModified = true;
	if (!mWindowHandle)
	{
		return false;
	}

	LLCoordScreen screen_pos;
	if (!convertCoords(position, &screen_pos))
	{
		return false;
	}

	// Inform the application of the new mouse position (needed for per-frame
	// hover/picking to function).
	LLCoordGL gl_pos;
	convertCoords(position, &gl_pos);
	mCallbacks->handleMouseMove(this, gl_pos, (MASK)0);

	// DEV-18951 VWR-8524 Camera moves wildly when alt-clicking. Because we
	// have preemptively notified the application of the new mouse position via
	// handleMouseMove() above, we need to clear out any stale mouse move
	// events. RN/JC
	MSG msg;
	while (PeekMessage(&msg, NULL, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE));

	return (bool)SetCursorPos(screen_pos.mX, screen_pos.mY);
}

bool LLWindowWin32::getCursorPosition(LLCoordWindow* position)
{
	POINT cursor_point;
	if (!mWindowHandle || !GetCursorPos(&cursor_point))
	{
		return false;
	}

	LLCoordScreen screen_pos;
	screen_pos.mX = cursor_point.x;
	screen_pos.mY = cursor_point.y;

	return convertCoords(screen_pos, position);
}

void LLWindowWin32::hideCursor()
{
	while (ShowCursor(FALSE) >= 0)
	{
		// Nothing, wait for cursor to push down
	}
	mCursorHidden = true;
	mHideCursorPermanent = true;
}

void LLWindowWin32::showCursor()
{
	// Makes sure the cursor shows up
	while (ShowCursor(TRUE) < 0)
	{
		// do nothing, wait for cursor to pop out
	}
	mCursorHidden = false;
	mHideCursorPermanent = false;
}

void LLWindowWin32::showCursorFromMouseMove()
{
	if (!mHideCursorPermanent)
	{
		showCursor();
	}
}

void LLWindowWin32::hideCursorUntilMouseMove()
{
	if (!mHideCursorPermanent && mMouseVanish)
	{
		hideCursor();
		mHideCursorPermanent = false;
	}
}

HCURSOR LLWindowWin32::loadColorCursor(LPCTSTR name)
{
	return (HCURSOR)LoadImage(mhInstance, name, IMAGE_CURSOR,
							  0,	// Default width
							  0,	// Default height
							  LR_DEFAULTCOLOR);
}

void LLWindowWin32::initCursors()
{
	mCursor[UI_CURSOR_ARROW] = LoadCursor(NULL, IDC_ARROW);
	mCursor[UI_CURSOR_WAIT] = LoadCursor(NULL, IDC_WAIT);
	mCursor[UI_CURSOR_HAND] = LoadCursor(NULL, IDC_HAND);
	mCursor[UI_CURSOR_IBEAM] = LoadCursor(NULL, IDC_IBEAM);
	mCursor[UI_CURSOR_CROSS] = LoadCursor(NULL, IDC_CROSS);
	mCursor[UI_CURSOR_SIZENWSE] = LoadCursor(NULL, IDC_SIZENWSE);
	mCursor[UI_CURSOR_SIZENESW] = LoadCursor(NULL, IDC_SIZENESW);
	mCursor[UI_CURSOR_SIZEWE] = LoadCursor(NULL, IDC_SIZEWE);
	mCursor[UI_CURSOR_SIZENS] = LoadCursor(NULL, IDC_SIZENS);
	mCursor[UI_CURSOR_NO] = LoadCursor(NULL, IDC_NO);
	mCursor[UI_CURSOR_WORKING] = LoadCursor(NULL, IDC_APPSTARTING);

	HMODULE module = GetModuleHandle(NULL);
	mCursor[UI_CURSOR_TOOLGRAB] = LoadCursor(module, TEXT("TOOLGRAB"));
	mCursor[UI_CURSOR_TOOLLAND] = LoadCursor(module, TEXT("TOOLLAND"));
	mCursor[UI_CURSOR_TOOLFOCUS] = LoadCursor(module, TEXT("TOOLFOCUS"));
	mCursor[UI_CURSOR_TOOLCREATE] = LoadCursor(module, TEXT("TOOLCREATE"));
	mCursor[UI_CURSOR_ARROWDRAG] = LoadCursor(module, TEXT("ARROWDRAG"));
	mCursor[UI_CURSOR_ARROWCOPY] = LoadCursor(module, TEXT("ARROWCOPY"));
	mCursor[UI_CURSOR_ARROWDRAGMULTI] = LoadCursor(module, TEXT("ARROWDRAGMULTI"));
	mCursor[UI_CURSOR_ARROWCOPYMULTI] = LoadCursor(module, TEXT("ARROWCOPYMULTI"));
	mCursor[UI_CURSOR_NOLOCKED] = LoadCursor(module, TEXT("NOLOCKED"));
	mCursor[UI_CURSOR_ARROWLOCKED] = LoadCursor(module, TEXT("ARROWLOCKED"));
	mCursor[UI_CURSOR_GRABLOCKED] = LoadCursor(module, TEXT("GRABLOCKED"));
	mCursor[UI_CURSOR_TOOLTRANSLATE] = LoadCursor(module, TEXT("TOOLTRANSLATE"));
	mCursor[UI_CURSOR_TOOLROTATE] = LoadCursor(module, TEXT("TOOLROTATE"));
	mCursor[UI_CURSOR_TOOLSCALE] = LoadCursor(module, TEXT("TOOLSCALE"));
	mCursor[UI_CURSOR_TOOLCAMERA] = LoadCursor(module, TEXT("TOOLCAMERA"));
	mCursor[UI_CURSOR_TOOLPAN] = LoadCursor(module, TEXT("TOOLPAN"));
	mCursor[UI_CURSOR_TOOLZOOMIN] = LoadCursor(module, TEXT("TOOLZOOMIN"));
	mCursor[UI_CURSOR_TOOLPICKOBJECT3] = LoadCursor(module, TEXT("TOOLPICKOBJECT3"));
	mCursor[UI_CURSOR_PIPETTE] = LoadCursor(module, TEXT("TOOLPIPETTE"));
	mCursor[UI_CURSOR_TOOLPATHFINDING] = LoadCursor(module, TEXT("TOOLPATHFINDING"));
	mCursor[UI_CURSOR_TOOLPATHFINDING_PATH_START_ADD] = LoadCursor(module, TEXT("TOOLPATHFINDINGPATHSTARTADD"));
	mCursor[UI_CURSOR_TOOLPATHFINDING_PATH_START] = LoadCursor(module, TEXT("TOOLPATHFINDINGPATHSTART"));
	mCursor[UI_CURSOR_TOOLPATHFINDING_PATH_END] = LoadCursor(module, TEXT("TOOLPATHFINDINGPATHEND"));
	mCursor[UI_CURSOR_TOOLPATHFINDING_PATH_END_ADD] = LoadCursor(module, TEXT("TOOLPATHFINDINGPATHENDADD"));
	mCursor[UI_CURSOR_TOOLNO] = LoadCursor(module, TEXT("TOOLNO"));

	// Color cursors
	mCursor[UI_CURSOR_TOOLSIT] = loadColorCursor(TEXT("TOOLSIT"));
	mCursor[UI_CURSOR_TOOLBUY] = loadColorCursor(TEXT("TOOLBUY"));
	mCursor[UI_CURSOR_TOOLPAY] = loadColorCursor(TEXT("TOOLPAY"));
	mCursor[UI_CURSOR_TOOLOPEN] = loadColorCursor(TEXT("TOOLOPEN"));
	mCursor[UI_CURSOR_TOOLPLAY] = loadColorCursor(TEXT("TOOLPLAY"));
	mCursor[UI_CURSOR_TOOLPAUSE] = loadColorCursor(TEXT("TOOLPAUSE"));
	mCursor[UI_CURSOR_TOOLMEDIAOPEN] = loadColorCursor(TEXT("TOOLMEDIAOPEN"));

	// Note: custom cursors that are not found make LoadCursor() return NULL.
	for (S32 i = 0; i < UI_CURSOR_COUNT; ++i)
	{
		if (!mCursor[i])
		{
			mCursor[i] = LoadCursor(NULL, IDC_ARROW);
		}
	}
}

//virtual
void LLWindowWin32::setCursor(ECursorType cursor)
{
	if (mCursorFrozen)
	{
		return;
	}

	if (cursor == UI_CURSOR_ARROW && mBusyCount > 0)
	{
		cursor = UI_CURSOR_WORKING;
	}

	if (mCurrentCursor != cursor)
	{
		mCurrentCursor = cursor;
		SetCursor(mCursor[cursor]);
	}
}

void LLWindowWin32::captureMouse()
{
	SetCapture(mWindowHandle);
}

void LLWindowWin32::releaseMouse()
{
	ReleaseCapture();
}

void LLWindowWin32::delayInputProcessing()
{
	mInputProcessingPaused = true;
}

void LLWindowWin32::gatherInput()
{
	MSG msg;
	int msg_count = 0;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) &&
		   msg_count++ < MAX_MESSAGE_PER_UPDATE)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (mInputProcessingPaused)
		{
			break;
		}

		// For async host by name support. Really hacky.
		if (gAsyncMsgCallback && LL_WM_HOST_RESOLVED == msg.message)
		{
			gAsyncMsgCallback(msg);
		}
	}

	mInputProcessingPaused = false;

	// Clear this once we have processed all mouse messages that might have
	// occurred after we slammed the mouse position
	mMousePositionModified = false;
}

LRESULT CALLBACK LLWindowWin32::mainWindowProc(HWND h_wnd, UINT u_msg,
											   WPARAM w_param, LPARAM l_param)
{
	LLWindowWin32* window_imp =
		(LLWindowWin32*)GetWindowLongPtr(h_wnd, GWLP_USERDATA);
	if (window_imp)
	{
		LLWindowCallbacks* callbacksp = window_imp->mCallbacks;

		// Juggle to make sure we can get negative positions for when mouse is
		// outside window.
		LLCoordWindow window_coord((S32)(S16)LOWORD(l_param),
								   (S32)(S16)HIWORD(l_param));

		LLCoordGL gl_coord;

		// Pass along extended flag in mask
		MASK mask = (l_param >> 16 & KF_EXTENDED) ? MASK_EXTENDED : 0x0;
		bool eat_keystroke = true;

		switch (u_msg)
		{
			RECT update_rect;
			S32 update_width, update_height;

		case WM_TIMER:
		{
			callbacksp->handleTimerEvent(window_imp);
			break;
		}

		case WM_DEVICECHANGE:
		{
			if (gDebugWindowProc)
			{
				llinfos << "  WM_DEVICECHANGE: wParam=" << w_param
						<< "; lParam=" << l_param << llendl;
			}
			if (w_param == DBT_DEVNODES_CHANGED ||
				w_param == DBT_DEVICEARRIVAL)
			{
				if (callbacksp->handleDeviceChange(window_imp))
				{
					return 0;
				}
			}
			break;
		}

		case WM_ERASEBKGND:
		{
			break;
		}

		case WM_PAINT:
		{
			GetUpdateRect(window_imp->mWindowHandle, &update_rect, FALSE);
			update_width = update_rect.right - update_rect.left + 1;
			update_height = update_rect.bottom - update_rect.top + 1;
			callbacksp->handlePaint(window_imp, update_rect.left,
									update_rect.top, update_width,
									update_height);
			break;
		}

		case WM_PARENTNOTIFY:
		{
			break;
		}

		// This message is sent whenever the cursor is moved in a window. You
		// need to set the appropriate cursor appearance.
		case WM_SETCURSOR:
		{
			// Only take control of cursor over client region of window
			// This allows Windows(tm) to handle resize cursors, etc.
			if (LOWORD(l_param) == HTCLIENT)
			{
				SetCursor(window_imp->mCursor[window_imp->mCurrentCursor]);
				return 0;
			}
			break;
		}

		case WM_ENTERMENULOOP:
		{
			callbacksp->handleWindowBlock(window_imp);
			break;
		}

		case WM_EXITMENULOOP:
		{
			callbacksp->handleWindowUnblock(window_imp);
			break;
		}

		case WM_ACTIVATEAPP:
		{
			// This message should be sent whenever the app gains or loses
			// focus.
			bool activating = (bool)w_param;
			bool minimized = window_imp->getMinimized();
			bool fullscreen = window_imp->mFullscreen;

			if (gDebugWindowProc)
			{
				llinfos << "WINDOWPROC ActivateApp. Activating: "
						<< (activating ? "yes" : "no") << " - Minimized: "
						<< (minimized ? "yes" : "no") << " - Fullscreen: "
						<< (fullscreen ? "yes" : "no") << llendl;
			}

			if (fullscreen)
			{
				// When we run fullscreen, restoring or minimizing the app
				// needs to switch the screen resolution
				if (activating)
				{
					window_imp->setFullscreenResolution();
					window_imp->restore();
				}
				else
				{
					window_imp->minimize();
					window_imp->resetDisplayResolution();
				}
			}

			callbacksp->handleActivateApp(window_imp, activating);

			break;
		}

		case WM_ACTIVATE:
		{
			// Can be one of WA_ACTIVE, WA_CLICKACTIVE, or WA_INACTIVE
			bool activating = LOWORD(w_param) != WA_INACTIVE;
			bool minimized = (bool)HIWORD(w_param);
			if (!activating && window_imp->mPreeditor)
			{
				window_imp->interruptLanguageTextInput();
			}

			// JC - I am not sure why, but if we do not report that we
			// handled the WM_ACTIVATE message, the WM_ACTIVATEAPP messages
			// do not work properly when we run fullscreen.
			if (gDebugWindowProc)
			{
				llinfos << "WINDOWPROC Activate. Activating: "
						<< (activating ? "yes" : "no") << " - Minimized: "
						<< (minimized ? "yes" : "no") << llendl;
			}

			// Do not handle this.
			break;
		}

		case WM_QUERYOPEN:
			// *TODO: use this to return a nice icon
			break;

		case WM_SYSCOMMAND:
		{
			switch (w_param)
			{
			case SC_KEYMENU:
				// Disallow the ALT key from triggering the default
				// system menu.
				return 0;

			case SC_SCREENSAVE:
			case SC_MONITORPOWER:
				// Eat screen save messages and prevent them !
				return 0;
			}
			break;
		}

		case WM_CLOSE:
		{
			// Will the app allow the window to close?
			if (callbacksp->handleCloseRequest(window_imp))
			{
				// Get the app to initiate cleanup.
				callbacksp->handleQuit(window_imp);
				// The app is responsible for calling destroyWindow when done
				// with GL
			}
			return 0;
		}

		case WM_DESTROY:
		{
			if (window_imp->shouldPostQuit())
			{
				// Posts WM_QUIT with an exit code of 0
				PostQuitMessage(0);
			}
			return 0;
		}

		case WM_COMMAND:
		{
			if (!HIWORD(w_param)) // this message is from a menu
			{
				callbacksp->handleMenuSelect(window_imp, LOWORD(w_param));
			}
			break;
		}

		case WM_SYSKEYDOWN:
		{
			// Allow system keys, such as ALT-F4 to be processed by Windows
			eat_keystroke = false;
		}

		case WM_KEYDOWN:
		{
			LL_FAST_TIMER(FTM_KEYHANDLER);

			// Do not know until wm_char comes in next:
			window_imp->mKeyCharCode = 0;
			window_imp->mKeyScanCode = (l_param >> 16) & 0xff;
			window_imp->mKeyVirtualKey = w_param;
			window_imp->mRawMsg = u_msg;
			window_imp->mRawWParam = w_param;
			window_imp->mRawLParam = l_param;

			if (gDebugWindowProc)
			{
				llinfos << "Debug WindowProc WM_KEYDOWN - key "
						<< S32(w_param) << llendl;
			}
			if (gKeyboardp &&
				gKeyboardp->handleKeyDown(w_param, mask) && eat_keystroke)
			{
				return 0;
			}
			// Pass on to windows if we did not handle it
			break;
		}

		case WM_SYSKEYUP:
			eat_keystroke = false;

		case WM_KEYUP:
		{
			LL_FAST_TIMER(FTM_KEYHANDLER);

			window_imp->mKeyScanCode = (l_param >> 16) & 0xff;
			window_imp->mKeyVirtualKey = w_param;
			window_imp->mRawMsg = u_msg;
			window_imp->mRawWParam = w_param;
			window_imp->mRawLParam = l_param;

			if (gDebugWindowProc)
			{
				llinfos << "Debug WindowProc WM_KEYUP - key: "
						<< S32(w_param) << llendl;
			}
			if (gKeyboardp &&
				gKeyboardp->handleKeyUp(w_param, mask) && eat_keystroke)
			{
				return 0;
			}

			// Pass on to windows
			break;
		}

		case WM_IME_SETCONTEXT:
		{
			if (gDebugWindowProc)
			{
				llinfos << "WM_IME_SETCONTEXT" << llendl;
			}
			if (window_imp->mPreeditor)
			{
				l_param &= ~ISC_SHOWUICOMPOSITIONWINDOW;
				// Invoke DefWinProc with the modified LPARAM.
			}
			break;
		}

		case WM_IME_STARTCOMPOSITION:
		{
			LL_FAST_TIMER(FTM_KEYHANDLER);

			if (gDebugWindowProc)
			{
				llinfos << "WM_IME_STARTCOMPOSITION" << llendl;
			}
			if (window_imp->mPreeditor)
			{
				window_imp->handleStartCompositionMessage();
				return 0;
			}
			break;
		}

		case WM_IME_ENDCOMPOSITION:
		{
			LL_FAST_TIMER(FTM_KEYHANDLER);

			if (gDebugWindowProc)
			{
				llinfos << "WM_IME_ENDCOMPOSITION" << llendl;
			}
			if (window_imp->mPreeditor)
			{
				return 0;
			}
			break;
		}

		case WM_IME_COMPOSITION:
		{
			LL_FAST_TIMER(FTM_KEYHANDLER);

			if (gDebugWindowProc)
			{
				llinfos << "WM_IME_COMPOSITION" << llendl;
			}
			if (window_imp->mPreeditor)
			{
				window_imp->handleCompositionMessage(l_param);
				return 0;
			}
			break;
		}

		case WM_IME_REQUEST:
		{
			LL_FAST_TIMER(FTM_KEYHANDLER);

			if (gDebugWindowProc)
			{
				llinfos << "WM_IME_REQUEST" << llendl;
			}
			if (window_imp->mPreeditor)
			{
				LRESULT result = 0;
				if (window_imp->handleImeRequests(w_param, l_param, &result))
				{
					return result;
				}
			}
			break;
		}

		case WM_CHAR:
		{
			LL_FAST_TIMER(FTM_KEYHANDLER);

			window_imp->mKeyCharCode = w_param;
			window_imp->mRawMsg = u_msg;
			window_imp->mRawWParam = w_param;
			window_imp->mRawLParam = l_param;

			// Should really use WM_UNICHAR eventually, but it requires a
			// specific Windows version and I need to figure out how that
			// works. - Doug
			// ... Well, I do not think so. How it works is explained in
			// Win32 API document, but WM_UNICHAR did not work as specified
			// at least on Windows XP SP1 Japanese version. I have never
			// used it since then and I'm not sure whether it has been
			// fixed now, but I do not think it is worth trying. The good
			// old WM_CHAR works just fine even for supplementary
			// characters. We just need to take care of surrogate pairs
			// sent as two WM_CHAR's by ourselves. It is not that tough.
			// - Alissa Sabre @ SL
			if (gDebugWindowProc)
			{
				llinfos << "Debug WindowProc WM_CHAR - key "
						<< S32(w_param) << llendl;
			}
			// Even if LLWindowCallbacks::handleUnicodeChar(llwchar, bool)
			// returned false, we *did* process the event, so I believe we
			// should not pass it to DefWindowProc...
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(false) : 0;
			window_imp->handleUnicodeUTF16((U16)w_param, mask);
			return 0;
		}

		case WM_LBUTTONDOWN:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			if (window_imp->mPreeditor)
			{
				window_imp->interruptLanguageTextInput();
			}

			// Because we move the cursor position in the app, we need to
			// query to find out where the cursor at the time the event is
			// handled. If we do not do this, many clicks could get
			// buffered up, and if the first click changes the cursor
			// position, all subsequent clicks will occur at the wrong
			// location. JC
			LLCoordWindow cursor_coord_window;
			if (window_imp->mMousePositionModified)
			{
				window_imp->getCursorPosition(&cursor_coord_window);
				window_imp->convertCoords(cursor_coord_window, &gl_coord);
			}
			else
			{
				window_imp->convertCoords(window_coord, &gl_coord);
			}
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			// Generate move event to update mouse coordinates
			callbacksp->handleMouseMove(window_imp, gl_coord, mask);
			if (callbacksp->handleMouseDown(window_imp, gl_coord, mask))
			{
				return 0;
			}
			break;
		}

		case WM_LBUTTONDBLCLK:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			// Because we move the cursor position in the app, we need to
			// query to find out where the cursor at the time the event is
			// handled. If we do not do this, many clicks could get
			// buffered up, and if the first click changes the cursor
			// position, all subsequent clicks will occur at the wrong
			// location. JC
			LLCoordWindow cursor_coord_window;
			if (window_imp->mMousePositionModified)
			{
				window_imp->getCursorPosition(&cursor_coord_window);
				window_imp->convertCoords(cursor_coord_window, &gl_coord);
			}
			else
			{
				window_imp->convertCoords(window_coord, &gl_coord);
			}
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			// generate move event to update mouse coordinates
			callbacksp->handleMouseMove(window_imp, gl_coord, mask);
			if (callbacksp->handleDoubleClick(window_imp, gl_coord, mask))
			{
				return 0;
			}
			break;
		}

		case WM_LBUTTONUP:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			// Because we move the cursor position in the app, we need to
			// query to find out where the cursor at the time the event is
			// handled. If we do not do this, many clicks could get
			// buffered up, and if the first click changes the cursor
			// position, all subsequent clicks will occur at the wrong
			// location. JC
			LLCoordWindow cursor_coord_window;
			if (window_imp->mMousePositionModified)
			{
				window_imp->getCursorPosition(&cursor_coord_window);
				window_imp->convertCoords(cursor_coord_window, &gl_coord);
			}
			else
			{
				window_imp->convertCoords(window_coord, &gl_coord);
			}
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			// generate move event to update mouse coordinates
			callbacksp->handleMouseMove(window_imp, gl_coord, mask);
			if (callbacksp->handleMouseUp(window_imp, gl_coord, mask))
			{
				return 0;
			}
			break;
		}

		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			if (window_imp->mPreeditor)
			{
				window_imp->interruptLanguageTextInput();
			}

			// Because we move the cursor position in the app, we need to
			// query to find out where the cursor at the time the event is
			// handled. If we do not do this, many clicks could get
			// buffered up, and if the first click changes the cursor
			// position, all subsequent clicks will occur at the wrong
			// location. JC
			LLCoordWindow cursor_coord_window;
			if (window_imp->mMousePositionModified)
			{
				window_imp->getCursorPosition(&cursor_coord_window);
				window_imp->convertCoords(cursor_coord_window, &gl_coord);
			}
			else
			{
				window_imp->convertCoords(window_coord, &gl_coord);
			}
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			// generate move event to update mouse coordinates
			callbacksp->handleMouseMove(window_imp, gl_coord, mask);
			if (callbacksp->handleRightMouseDown(window_imp, gl_coord, mask))
			{
				return 0;
			}
			break;
		}

		case WM_RBUTTONUP:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			// Because we move the cursor position in the app, we need to
			// query to find out where the cursor at the time the event is
			// handled. If we do not do this, many clicks could get
			// buffered up, and if the first click changes the cursor
			// position, all subsequent clicks will occur at the wrong
			// location. JC
			LLCoordWindow cursor_coord_window;
			if (window_imp->mMousePositionModified)
			{
				window_imp->getCursorPosition(&cursor_coord_window);
				window_imp->convertCoords(cursor_coord_window, &gl_coord);
			}
			else
			{
				window_imp->convertCoords(window_coord, &gl_coord);
			}
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			// generate move event to update mouse coordinates
			callbacksp->handleMouseMove(window_imp, gl_coord, mask);
			if (callbacksp->handleRightMouseUp(window_imp, gl_coord, mask))
			{
				return 0;
			}
		}
		break;

		case WM_MBUTTONDOWN:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			if (window_imp->mPreeditor)
			{
				window_imp->interruptLanguageTextInput();
			}

			// Because we move the cursor position in the app, we need to
			// query to find out where the cursor at the time the event is
			// handled. If we do not do this, many clicks could get
			// buffered up, and if the first click changes the cursor
			// position, all subsequent clicks will occur at the wrong
			// location. JC
			LLCoordWindow cursor_coord_window;
			if (window_imp->mMousePositionModified)
			{
				window_imp->getCursorPosition(&cursor_coord_window);
				window_imp->convertCoords(cursor_coord_window, &gl_coord);
			}
			else
			{
				window_imp->convertCoords(window_coord, &gl_coord);
			}
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			// generate move event to update mouse coordinates
			callbacksp->handleMouseMove(window_imp, gl_coord, mask);
			if (callbacksp->handleMiddleMouseDown(window_imp, gl_coord, mask))
			{
				return 0;
			}
			break;
		}

		case WM_MBUTTONUP:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			// Because we move the cursor position in the app, we need to
			// query to find out where the cursor at the time the event is
			// handled. If we do not do this, many clicks could get
			// buffered up, and if the first click changes the cursor
			// position, all subsequent clicks will occur at the wrong
			// location. JC
			LLCoordWindow cursor_coord_window;
			if (window_imp->mMousePositionModified)
			{
				window_imp->getCursorPosition(&cursor_coord_window);
				window_imp->convertCoords(cursor_coord_window, &gl_coord);
			}
			else
			{
				window_imp->convertCoords(window_coord, &gl_coord);
			}
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			// generate move event to update mouse coordinates
			callbacksp->handleMouseMove(window_imp, gl_coord, mask);
			if (callbacksp->handleMiddleMouseUp(window_imp, gl_coord, mask))
			{
				return 0;
			}
			break;
		}

		case WM_MOUSEWHEEL:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			static short z_delta = 0;
			z_delta += HIWORD(w_param);
			// Current mouse wheels report changes in increments of zDelta
			// (+120, -120). Future, higher resolution mouse wheels may
			// report smaller deltas. So we sum the deltas and only act
			// when we have exceeded WHEEL_DELTA
			// If the user rapidly spins the wheel, we can get messages
			// with large deltas, like 480 or so. Thus we need to scroll
			// more quickly.
			if (z_delta <= -WHEEL_DELTA || WHEEL_DELTA <= z_delta)
			{
				short clicks = -z_delta / WHEEL_DELTA;
				callbacksp->handleScrollWheel(window_imp, clicks);
				z_delta = 0;
			}
			return 0;
		}

		// Handle mouse movement within the window
		case WM_MOUSEMOVE:
		{
			LL_FAST_TIMER(FTM_MOUSEHANDLER);

			window_imp->convertCoords(window_coord, &gl_coord);
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			callbacksp->handleMouseMove(window_imp, gl_coord, mask);
			return 0;
		}

		case WM_SIZE:
		{
			S32 width = S32(LOWORD(l_param));
			S32 height = S32(HIWORD(l_param));

			if (gDebugWindowProc)
			{
				bool maximized = w_param == SIZE_MAXIMIZED;
				bool minimized = w_param == SIZE_MINIMIZED;
				bool restored = w_param == SIZE_RESTORED;
				llinfos << "WINDOWPROC - Size: " << width << "x" << height
						<< " - Maximized: " << (maximized ? "yes" : "no")
						<< " - Minimized: " << (minimized ? "yes" : "no")
						<< " - Restored: " << (restored ? "yes" : "no")
						<< llendl;
			}

			// There is an odd behavior with WM_SIZE that I would call a bug.
			// If the window is maximized, and you call MoveWindow() with a
			// size smaller than a maximized window, it ends up sending WM_SIZE
			// with w_param set to SIZE_MAXIMIZED, which is not true. So the
			// logic below does not work.
			// Fixed it by calling ShowWindow(SW_RESTORE) first (see
			// moveWindow() in this file). SL-44655

			// If we are now restored, but we were not before, this means that
			// the window was un-minimized.
			if (w_param == SIZE_RESTORED &&
				window_imp->mLastSizeWParam != SIZE_RESTORED)
			{
				callbacksp->handleActivate(window_imp, true);
			}

			// Handle case of window being maximized from fully minimized state
			if (w_param == SIZE_MAXIMIZED &&
				window_imp->mLastSizeWParam != SIZE_MAXIMIZED)
			{
				callbacksp->handleActivate(window_imp, true);
			}

			// Also handle the minimization case
			if (w_param == SIZE_MINIMIZED &&
				window_imp->mLastSizeWParam != SIZE_MINIMIZED)
			{
				callbacksp->handleActivate(window_imp, false);
			}

			// Actually resize all of our views
			if (w_param != SIZE_MINIMIZED)
			{
				// Ignore updates for minimizing and minimized "windows"
				callbacksp->handleResize(window_imp, LOWORD(l_param),
										 HIWORD(l_param));
			}

			window_imp->mLastSizeWParam = w_param;

			return 0;
		}

		case WM_DPICHANGED:
		{
			LL_DEBUGS("Window") << "Got a WM_DPICHANGED event." << LL_ENDL;
			if (!gHiDPISupport)
			{
				LL_DEBUGS("Window") << "Ignoring based on gHiDPISupport."
									<< LL_ENDL;
				break;
			}
			if (gIgnoreHiDPIEvents)
			{
				LL_DEBUGS("Window") << "Ignoring based on gIgnoreHiDPIEvents."
									<< LL_ENDL;
				break;
			}
			LPRECT lprc_new_scale = (LPRECT)l_param;
			F32 scale = F32(LOWORD(w_param)) / F32(USER_DEFAULT_SCREEN_DPI);
			S32 width = lprc_new_scale->right - lprc_new_scale->left;
			S32 height = lprc_new_scale->bottom - lprc_new_scale->top;
			if (callbacksp->handleDPIChanged(window_imp, scale, width, height))
			{
				SetWindowPos(h_wnd, HWND_TOP, lprc_new_scale->left,
							 lprc_new_scale->top, width, height,
							 SWP_NOZORDER | SWP_NOACTIVATE);
			}
			return 0;
		}

		case WM_SETFOCUS:
		{
			if (gDebugWindowProc)
			{
				llinfos << "WINDOWPROC SetFocus" << llendl;
			}

			// Stop flashing the task bar button when our window gains focus
			if (window_imp->mWindowHandle)
			{
				FLASHWINFO flash_info;
				flash_info.cbSize = sizeof(FLASHWINFO);
				flash_info.hwnd = window_imp->mWindowHandle;
				flash_info.dwFlags = FLASHW_STOP;
				flash_info.uCount = 0;
				flash_info.dwTimeout = 0;
				FlashWindowEx(&flash_info);
			}

			callbacksp->handleFocus(window_imp);
			return 0;
		}

		case WM_KILLFOCUS:
		{
			if (gDebugWindowProc)
			{
				llinfos << "WINDOWPROC KillFocus" << llendl;
			}
			callbacksp->handleFocusLost(window_imp);
			return 0;
		}

		case WM_COPYDATA:
		{
			// Received an URL
			PCOPYDATASTRUCT myCDS = (PCOPYDATASTRUCT)l_param;
			callbacksp->handleDataCopy(window_imp, myCDS->dwData,
									   myCDS->lpData);
			return 0;
		}

		case WM_SETTINGCHANGE:
		{
			if (w_param == SPI_SETMOUSEVANISH &&
				!SystemParametersInfo(SPI_GETMOUSEVANISH, 0,
									  &window_imp->mMouseVanish, 0))
			{
				window_imp->mMouseVanish = true;
			}
			break;
		}

		default:
			if (gDebugWindowProc)
			{
				llinfos << "Unhandled windows message code: " << U32(u_msg)
						<< llendl;
			}
		}
	}

	// Pass unhandled messages down to Windows
	return DefWindowProc(h_wnd, u_msg, w_param, l_param);
}

bool LLWindowWin32::convertCoords(LLCoordGL from, LLCoordWindow* to)
{
	S32 client_height;
	RECT client_rect;

	if (!mWindowHandle || !to ||
		!GetClientRect(mWindowHandle, &client_rect))
	{
		return false;
	}

	to->mX = from.mX;
	client_height = client_rect.bottom - client_rect.top;
	to->mY = client_height - from.mY - 1;

	return true;
}

bool LLWindowWin32::convertCoords(LLCoordWindow from, LLCoordGL* to)
{
	S32 client_height;
	RECT client_rect;

	if (!mWindowHandle || !to || !GetClientRect(mWindowHandle, &client_rect))
	{
		return false;
	}

	to->mX = from.mX;
	client_height = client_rect.bottom - client_rect.top;
	to->mY = client_height - from.mY - 1;

	return true;
}

bool LLWindowWin32::convertCoords(LLCoordScreen from, LLCoordWindow* to)
{
	if (!mWindowHandle || !to)
	{
		return false;
	}

	POINT mouse_point;

	mouse_point.x = from.mX;
	mouse_point.y = from.mY;

	bool result = (bool)ScreenToClient(mWindowHandle, &mouse_point);
	if (result)
	{
		to->mX = mouse_point.x;
		to->mY = mouse_point.y;
	}

	return result;
}

bool LLWindowWin32::convertCoords(LLCoordWindow from, LLCoordScreen* to)
{
	if (!mWindowHandle || !to)
	{
		return false;
	}

	POINT mouse_point;

	mouse_point.x = from.mX;
	mouse_point.y = from.mY;

	bool result = (bool)ClientToScreen(mWindowHandle, &mouse_point);
	if (result)
	{
		to->mX = mouse_point.x;
		to->mY = mouse_point.y;
	}

	return result;
}

bool LLWindowWin32::convertCoords(LLCoordScreen from, LLCoordGL* to)
{
	LLCoordWindow window_coord;
	return convertCoords(from, &window_coord) &&
		convertCoords(window_coord, to);
}

bool LLWindowWin32::convertCoords(LLCoordGL from, LLCoordScreen* to)
{
	LLCoordWindow window_coord;
	return convertCoords(from, &window_coord) &&
		convertCoords(window_coord, to);
}

bool LLWindowWin32::isClipboardTextAvailable()
{
	return IsClipboardFormatAvailable(CF_UNICODETEXT);
}

bool LLWindowWin32::pasteTextFromClipboard(LLWString &dst)
{
	bool success = false;

	if (IsClipboardFormatAvailable(CF_UNICODETEXT))
	{
		if (OpenClipboard(mWindowHandle))
		{
			HGLOBAL h_data = GetClipboardData(CF_UNICODETEXT);
			if (h_data)
			{
				WCHAR *utf16str = (WCHAR*)GlobalLock(h_data);
				if (utf16str)
				{
					dst = ll_convert_wide_to_wstring(utf16str);
					LLWStringUtil::removeCRLF(dst);
					GlobalUnlock(h_data);
					success = true;
				}
			}
			CloseClipboard();
		}
	}

	return success;
}

bool LLWindowWin32::copyTextToClipboard(const LLWString& wstr)
{
	bool success = false;

	if (OpenClipboard(mWindowHandle))
	{
		EmptyClipboard();

		// Provide a copy of the data in Unicode format.
		LLWString sanitized_string(wstr);
		LLWStringUtil::addCRLF(sanitized_string);
		llutf16string out_utf16 = wstring_to_utf16str(sanitized_string);
		const size_t size_utf16 = (out_utf16.length() + 1) * sizeof(WCHAR);

		// Memory is allocated and then ownership of it is transfered to the system.
		HGLOBAL hglobal_copy_utf16 = GlobalAlloc(GMEM_MOVEABLE, size_utf16);
		if (hglobal_copy_utf16)
		{
			WCHAR* copy_utf16 = (WCHAR*)GlobalLock(hglobal_copy_utf16);
			if (copy_utf16)
			{
				memcpy(copy_utf16, out_utf16.c_str(), size_utf16);
				GlobalUnlock(hglobal_copy_utf16);

				if (SetClipboardData(CF_UNICODETEXT, hglobal_copy_utf16))
				{
					success = true;
				}
			}
		}

		CloseClipboard();
	}

	return success;
}

//virtual
bool LLWindowWin32::isPrimaryTextAvailable()
{
	return !mPrimaryClipboard.empty();
}

//virtual
bool LLWindowWin32::pasteTextFromPrimary(LLWString& text)
{
	if (mPrimaryClipboard.empty())
	{
		return false;
	}

	text = mPrimaryClipboard;
	return true;
}

// virtual
bool LLWindowWin32::copyTextToPrimary(const LLWString& text)
{
	mPrimaryClipboard = text;
	return true;
}

// Constrains the mouse to the window.
void LLWindowWin32::setMouseClipping(bool b)
{
	if (b != mIsMouseClipping)
	{
		bool success = false;

		if (b)
		{
			GetClipCursor(&mOldMouseClip);

			RECT client_rect_in_screen_space;
			if (getClientRectInScreenSpace(&client_rect_in_screen_space))
			{
				success = (bool)ClipCursor(&client_rect_in_screen_space);
			}
		}
		else
		{
			// Must restore the old mouse clip, which may be set by another
			// window.
			success = (bool)ClipCursor(&mOldMouseClip);
			SetRect(&mOldMouseClip, 0, 0, 0, 0);
		}

		if (success)
		{
			mIsMouseClipping = b;
		}
	}
}

bool LLWindowWin32::getClientRectInScreenSpace(RECT* rectp)
{
	bool success = false;

	RECT client_rect;
	if (mWindowHandle && GetClientRect(mWindowHandle, &client_rect))
	{
		POINT top_left;
		top_left.x = client_rect.left;
		top_left.y = client_rect.top;
		ClientToScreen(mWindowHandle, &top_left);

		POINT bottom_right;
		bottom_right.x = client_rect.right;
		bottom_right.y = client_rect.bottom;
		ClientToScreen(mWindowHandle, &bottom_right);

		SetRect(rectp, top_left.x, top_left.y, bottom_right.x, bottom_right.y);

		success = true;
	}

	return success;
}

void LLWindowWin32::flashIcon(F32 seconds)
{
	FLASHWINFO flash_info;

	flash_info.cbSize = sizeof(FLASHWINFO);
	flash_info.hwnd = mWindowHandle;
	flash_info.dwFlags = FLASHW_TRAY;
	flash_info.uCount = UINT(seconds / ICON_FLASH_TIME);
	flash_info.dwTimeout = DWORD(1000.f * ICON_FLASH_TIME);
	FlashWindowEx(&flash_info);
}

bool LLWindowWin32::restoreGamma()
{
	if (mCustomGammaSet)
	{
		mCustomGammaSet = false;
		return (bool)SetDeviceGammaRamp(mhDC, mPrevGammaRamp);
	}
	return true;
}

bool LLWindowWin32::setGamma(F32 gamma)
{
	mCurrentGamma = llclamp(gamma, 0.01f, 10.f);
	LL_DEBUGS("Window") << "Setting gamma to " << mCurrentGamma << LL_ENDL;

	// Get the previous gamma ramp to restore later.
	if (!mCustomGammaSet)
	{
		if (!gGLManager.mIsIntel)
		{
			LL_DEBUGS("Window") << "Getting previous gamma ramp to restore it later"
								<< LL_ENDL;
			if (!GetDeviceGammaRamp(mhDC, mPrevGammaRamp))
			{
				llwarns << "Failed to get the previous gamma ramp. Aborted."
						<< llendl;
				return false;
			}
		}
		mCustomGammaSet = true;
	}

	constexpr F32 ONE256TH = 1.f / 256.f;
	F32 inv_gamma = 1.f / mCurrentGamma;
	for (S32 i = 0; i < 256; ++i)
	{
		S32 value = (S32)(powf((F32)i * ONE256TH, inv_gamma) * 65535.f + 0.5f);
		if (value > 65535)
		{
			value = 65535;
		}
		mCurrentGammaRamp[0][i] = mCurrentGammaRamp[1][i] =
			mCurrentGammaRamp[2][i] = (WORD)value;
	}

	return SetDeviceGammaRamp(mhDC, mCurrentGammaRamp);
}

LLWindow::LLWindowResolution* LLWindowWin32::getSupportedResolutions(S32& num_resolutions)
{
	if (!mSupportedResolutions)
	{
		mSupportedResolutions = new LLWindowResolution[MAX_NUM_RESOLUTIONS];
		DEVMODE dev_mode;

		mNumSupportedResolutions = 0;
		for (S32 mode_num = 0; mNumSupportedResolutions < MAX_NUM_RESOLUTIONS;
			++mode_num)
		{
			if (!EnumDisplaySettings(NULL, mode_num, &dev_mode))
			{
				break;
			}
			S32 w = dev_mode.dmPelsWidth;
			S32 h = dev_mode.dmPelsHeight;

			if (dev_mode.dmBitsPerPel == BITS_PER_PIXEL && w >= 800 &&
				h >= 600)
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

F32 LLWindowWin32::getNativeAspectRatio()
{
	if (mOverrideAspectRatio > 0.f)
	{
		return mOverrideAspectRatio;
	}
	if (mNativeAspectRatio > 0.f)
	{
		// We grabbed this value at startup, based on the user's desktop settings
		return mNativeAspectRatio;
	}
	// RN: this hack presumes that the largest supported resolution is monitor-
	// limited and that pixels in that mode are square, therefore defining the
	// native aspect ratio of the monitor... This seems to work to a close
	// approximation for most CRTs/LCDs.
	S32 num_resolutions;
	LLWindowResolution* resolutions = getSupportedResolutions(num_resolutions);

	return (F32)resolutions[num_resolutions - 1].mWidth /
		(F32)resolutions[num_resolutions - 1].mHeight;
}

F32 LLWindowWin32::getPixelAspectRatio()
{
	F32 pixel_aspect = 1.f;
	if (getFullscreen())
	{
		LLCoordScreen screen_size;
		getSize(&screen_size);
		pixel_aspect = getNativeAspectRatio() * (F32)screen_size.mY /
			(F32)screen_size.mX;
	}
	return pixel_aspect;
}

// Change display resolution.  Returns true if successful.
// protected
bool LLWindowWin32::setDisplayResolution(S32 width, S32 height, S32 bits,
										 S32 refresh)
{
	DEVMODE dev_mode;
	::ZeroMemory(&dev_mode, sizeof(DEVMODE));
	dev_mode.dmSize = sizeof(dev_mode);

	// Do not change anything if we do not have to
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode))
	{
		if (dev_mode.dmPelsWidth == width && dev_mode.dmPelsHeight == height &&
			dev_mode.dmBitsPerPel == bits &&
			dev_mode.dmDisplayFrequency == refresh)
		{
			// Display mode identical, do nothing
			return true;
		}
	}

	memset(&dev_mode, 0, sizeof(dev_mode));
	dev_mode.dmSize = sizeof(dev_mode);
	dev_mode.dmPelsWidth = width;
	dev_mode.dmPelsHeight = height;
	dev_mode.dmBitsPerPel = bits;
	dev_mode.dmDisplayFrequency = refresh;
	dev_mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
		DM_DISPLAYFREQUENCY;

	// CDS_FULLSCREEN indicates that this is a temporary change to the device
	// mode.
	LONG cds_result = ChangeDisplaySettings(&dev_mode, CDS_FULLSCREEN);
	bool success = DISP_CHANGE_SUCCESSFUL == cds_result;
	if (!success)
	{
		llwarns << "setDisplayResolution failed, " << width << "x" << height
				<< "x" << bits << " @ " << refresh << llendl;
	}

	return success;
}

// protected
bool LLWindowWin32::setFullscreenResolution()
{
	if (mFullscreen)
	{
		return setDisplayResolution(mFullscreenWidth, mFullscreenHeight,
			mFullscreenBits, mFullscreenRefresh);
	}
	return false;
}

// protected
bool LLWindowWin32::resetDisplayResolution()
{
	LL_DEBUGS("Window") << "Resetting the display resolution" << LL_ENDL;

	bool success = ChangeDisplaySettings(NULL, 0) == DISP_CHANGE_SUCCESSFUL;
	if (!success)
	{
		llwarns << "Failure to reset display resolution" << llendl;
	}

	LL_DEBUGS("Window") << "Display resolution reset done" << LL_ENDL;

	return success;
}

void LLWindowWin32::swapBuffers()
{
	if (mhDC)
	{
		LL_FAST_TIMER(FTM_SWAP);
		SwapBuffers(mhDC);
	}
}

//
// LLSplashScreenImp
//
LLSplashScreenWin32::LLSplashScreenWin32()
:	mWindow(NULL)
{
}

void LLSplashScreenWin32::showImpl()
{
	// This appears to work ???
	HINSTANCE hinst = GetModuleHandle(NULL);
	mWindow = CreateDialog(hinst, TEXT("SPLASHSCREEN"), NULL,	// No parent
						   (DLGPROC)LLSplashScreenWin32::windowProc);
	ShowWindow(mWindow, SW_SHOW);
}

void LLSplashScreenWin32::updateImpl(const std::string& mesg)
{
	if (mWindow)
	{
		WCHAR w_mesg[1024];
		mbstowcs(w_mesg, mesg.c_str(), 1024);

		SendDlgItemMessage(mWindow, 666, // HACK: text id
			WM_SETTEXT, FALSE, (LPARAM)w_mesg);
	}
}

void LLSplashScreenWin32::hideImpl()
{
	if (mWindow)
	{
		destroy_window_handler(mWindow);
		mWindow = NULL;
	}
}

//static
LRESULT CALLBACK LLSplashScreenWin32::windowProc(HWND h_wnd, UINT u_msg,
												 WPARAM w_param,
												 LPARAM l_param)
{
	// Just give it to windows
	return DefWindowProc(h_wnd, u_msg, w_param, l_param);
}

//
// Helper Funcs
//

S32 OSMessageBoxWin32(const std::string& text, const std::string& caption,
					  U32 type)
{
	UINT uType;
	switch (type)
	{
	case OSMB_OK:
		uType = MB_OK;
		break;

	case OSMB_OKCANCEL:
		uType = MB_OKCANCEL;
		break;

	case OSMB_YESNO:
		uType = MB_YESNO;
		break;

	default:
		uType = MB_OK;
	}
	int retval_win = MessageBoxW(NULL,
		ll_convert_string_to_wide(text).c_str(),
		ll_convert_string_to_wide(caption).c_str(),
		uType);
	S32 retval;
	switch (retval_win)
	{
	case IDYES:
		retval = OSBTN_YES;
		break;

	case IDNO:
		retval = OSBTN_NO;
		break;

	case IDOK:
		retval = OSBTN_OK;
		break;

	case IDCANCEL:
		retval = OSBTN_CANCEL;
		break;

	default:
		retval = OSBTN_CANCEL;
	}
	return retval;
}

void LLWindowWin32::spawnWebBrowser(const std::string& escaped_url, bool async)
{
	bool found = false;
	for (S32 i = 0; i < gURLProtocolWhitelistCount; ++i)
	{
		if (escaped_url.find(gURLProtocolWhitelist[i]) == 0)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		llwarns << "spawn_web_browser() called for url with protocol not on whitelist: "
				<< escaped_url << llendl;
		return;
	}

	llinfos << "Opening URL " << escaped_url << llendl;

	// Replaced ShellExecute code with ShellExecuteEx since ShellExecute does
	// not work reliablly on Vista.

	// This is madness.. no, this is..
	std::wstring url_utf16 = ll_convert_string_to_wide(escaped_url);

	// let the OS decide what to use to open the URL
	SHELLEXECUTEINFO sei = { sizeof(sei) };
	// NOTE: this assumes that SL will stick around long enough to complete the
	// DDE message exchange
	// necessary for ShellExecuteEx to complete
	if (async)
	{
		sei.fMask = SEE_MASK_ASYNCOK;
	}
	sei.nShow = SW_SHOWNORMAL;
	sei.lpVerb = L"open";
	sei.lpFile = url_utf16.c_str();
	ShellExecuteEx(&sei);
}

// Make the raw keyboard data available - used to poke through to CEF so
// that it has access to the virtual keycodes etc that it needs.
LLSD LLWindowWin32::getNativeKeyData()
{
	LLSD result = LLSD::emptyMap();
	result["scan_code"] = (S32)mKeyScanCode;
	result["virtual_key"] = (S32)mKeyVirtualKey;
	result["msg"] = ll_sd_from_U32(mRawMsg);
	result["w_param"] = ll_sd_from_U32(mRawWParam);
	result["l_param"] = ll_sd_from_U32(mRawLParam);

	return result;
}

void* LLWindowWin32::getPlatformWindow()
{
	return (void*)mWindowHandle;
}

void LLWindowWin32::bringToFront()
{
	BringWindowToTop(mWindowHandle);
}

// set (OS) window focus back to the client
void LLWindowWin32::focusClient()
{
	SetFocus(mWindowHandle);
}

void LLWindowWin32::allowLanguageTextInput(LLPreeditor* preeditor, bool b)
{
	if (sLanguageTextInputAllowed == b)
	{
		return;
	}

	if (!b && preeditor != mPreeditor)
	{
		// This condition may occur with a call to
		// setEnabled(bool) from LLTextEditor or LLLineEditor
		// when the control is not focused.
		// We need to silently ignore the case so that
		// the language input status of the focused control
		// is not disturbed.
		return;
	}

	// Take care of old and new preeditors.
	if (preeditor != mPreeditor || !b)
	{
		if (sLanguageTextInputAllowed)
		{
			interruptLanguageTextInput();
		}
		mPreeditor = (b ? preeditor : NULL);
	}

	sLanguageTextInputAllowed = b;

	if (sLanguageTextInputAllowed)
	{
		// Allowing: Restore the previous IME status, so that the user has a
		// feeling that the previous text input continues naturally. Be
		// careful, however, the IME status is meaningful only during the user
		// keeps using same Input Locale (aka Keyboard Layout).
		if (sWinIMEOpened && sWinInputLocale == GetKeyboardLayout(0))
		{
			HIMC himc = ImmGetContext(mWindowHandle);
			ImmSetOpenStatus(himc, TRUE);
			ImmSetConversionStatus(himc, sWinIMEConversionMode,
				sWinIMESentenceMode);
			ImmReleaseContext(mWindowHandle, himc);
		}
	}
	else
	{
		// Disallowing: Turn off the IME so that succeeding key events bypass
		// IME and come to us directly. However, do it after saving the current
		// IME status. We need to restore the status when allowing language
		// text input again.
		sWinInputLocale = GetKeyboardLayout(0);
		sWinIMEOpened = ImmIsIME(sWinInputLocale);
		if (sWinIMEOpened)
		{
			HIMC himc = ImmGetContext(mWindowHandle);
			sWinIMEOpened = ImmGetOpenStatus(himc);
			if (sWinIMEOpened)
			{
				ImmGetConversionStatus(himc, &sWinIMEConversionMode,
					&sWinIMESentenceMode);

				// We need both ImmSetConversionStatus and ImmSetOpenStatus
				// here to surely disable IME's keyboard hooking, because some
				// IME reacts only on the former and some other on the latter.
				ImmSetConversionStatus(himc, IME_CMODE_NOCONVERSION,
					sWinIMESentenceMode);
				ImmSetOpenStatus(himc, FALSE);
			}
			ImmReleaseContext(mWindowHandle, himc);
		}
	}
}

void LLWindowWin32::fillCandidateForm(const LLCoordGL& caret,
									  const LLRect& bounds,
									  CANDIDATEFORM* form)
{
	LLCoordWindow caret_coord, top_left, bottom_right;
	convertCoords(caret, &caret_coord);
	convertCoords(LLCoordGL(bounds.mLeft, bounds.mTop), &top_left);
	convertCoords(LLCoordGL(bounds.mRight, bounds.mBottom), &bottom_right);

	memset(form, 0, sizeof(CANDIDATEFORM));
	form->dwStyle = CFS_EXCLUDE;
	form->ptCurrentPos.x = caret_coord.mX;
	form->ptCurrentPos.y = caret_coord.mY;
	form->rcArea.left = top_left.mX;
	form->rcArea.top = top_left.mY;
	form->rcArea.right = bottom_right.mX;
	form->rcArea.bottom = bottom_right.mY;
}

// Put the IME window at the right place (near current text input). Point
// coordinates should be the top of the current text line.
void LLWindowWin32::setLanguageTextInput(const LLCoordGL& position)
{
	if (sLanguageTextInputAllowed)
	{
		HIMC himc = ImmGetContext(mWindowHandle);

		LLCoordWindow win_pos;
		convertCoords(position, &win_pos);

		if (win_pos.mX >= 0 && win_pos.mY >= 0 &&
			(win_pos.mX != sWinIMEWindowPosition.mX ||
			 win_pos.mY != sWinIMEWindowPosition.mY))
		{
			COMPOSITIONFORM ime_form;
			memset(&ime_form, 0, sizeof(ime_form));
			ime_form.dwStyle = CFS_POINT;
			ime_form.ptCurrentPos.x = win_pos.mX;
			ime_form.ptCurrentPos.y = win_pos.mY;

			ImmSetCompositionWindow(himc, &ime_form);

			sWinIMEWindowPosition.set(win_pos.mX, win_pos.mY);
		}

		ImmReleaseContext(mWindowHandle, himc);
	}
}

void LLWindowWin32::fillCharPosition(const LLCoordGL& caret,
									 const LLRect& bounds,
									 const LLRect& control,
									 IMECHARPOSITION* char_position)
{
	LLCoordScreen caret_coord, top_left, bottom_right;
	convertCoords(caret, &caret_coord);
	convertCoords(LLCoordGL(bounds.mLeft, bounds.mTop), &top_left);
	convertCoords(LLCoordGL(bounds.mRight, bounds.mBottom), &bottom_right);

	char_position->pt.x = caret_coord.mX;
	// Windows wants the coordinate of upper left corner of a character...
	char_position->pt.y = top_left.mY;
	char_position->cLineHeight = bottom_right.mY - top_left.mY;
	char_position->rcDocument.left = top_left.mX;
	char_position->rcDocument.top = top_left.mY;
	char_position->rcDocument.right = bottom_right.mX;
	char_position->rcDocument.bottom = bottom_right.mY;
}

void LLWindowWin32::fillCompositionLogfont(LOGFONT* logfont)
{
	// Our font is a list of FreeType recognized font files that may not have a
	// corresponding ones in Windows' fonts. Hence, we cannot simply tell
	// Windows which font we are using. We will notify a _standard_ font for a
	// current input locale instead. We use a hard-coded knowledge about the
	// Windows' standard configuration to do so...

	memset(logfont, 0, sizeof(LOGFONT));

	const WORD lang_id = LOWORD(GetKeyboardLayout(0));
	switch (PRIMARYLANGID(lang_id))
	{
	case LANG_CHINESE:
		// We need to identify one of two Chinese fonts.
		switch (SUBLANGID(lang_id))
		{
		case SUBLANG_CHINESE_SIMPLIFIED:
		case SUBLANG_CHINESE_SINGAPORE:
			logfont->lfCharSet = GB2312_CHARSET;
			lstrcpy(logfont->lfFaceName, TEXT("SimHei"));
			break;
		case SUBLANG_CHINESE_TRADITIONAL:
		case SUBLANG_CHINESE_HONGKONG:
		case SUBLANG_CHINESE_MACAU:
		default:
			logfont->lfCharSet = CHINESEBIG5_CHARSET;
			lstrcpy(logfont->lfFaceName, TEXT("MingLiU"));
			break;
		}
		break;
	case LANG_JAPANESE:
		logfont->lfCharSet = SHIFTJIS_CHARSET;
		lstrcpy(logfont->lfFaceName, TEXT("MS Gothic"));
		break;
	case LANG_KOREAN:
		logfont->lfCharSet = HANGUL_CHARSET;
		lstrcpy(logfont->lfFaceName, TEXT("Gulim"));
		break;
	default:
		logfont->lfCharSet = ANSI_CHARSET;
		lstrcpy(logfont->lfFaceName, TEXT("Tahoma"));
		break;
	}

	logfont->lfHeight = mPreeditor->getPreeditFontSize();
	logfont->lfWeight = FW_NORMAL;
}

U32 LLWindowWin32::fillReconvertString(const LLWString &text,
									   S32 focus, S32 focus_length,
									   RECONVERTSTRING* reconvert_string)
{
	const llutf16string text_utf16 = wstring_to_utf16str(text);
	const DWORD required_size = sizeof(RECONVERTSTRING) +
		(text_utf16.length() + 1) * sizeof(WCHAR);
	if (reconvert_string && reconvert_string->dwSize >= required_size)
	{
		const DWORD focus_utf16_at = wstring_utf16_length(text, 0, focus);
		const DWORD focus_utf16_length = wstring_utf16_length(text, focus,
			focus_length);

		reconvert_string->dwVersion = 0;
		reconvert_string->dwStrLen = text_utf16.length();
		reconvert_string->dwStrOffset = sizeof(RECONVERTSTRING);
		reconvert_string->dwCompStrLen = focus_utf16_length;
		reconvert_string->dwCompStrOffset = focus_utf16_at * sizeof(WCHAR);
		reconvert_string->dwTargetStrLen = 0;
		reconvert_string->dwTargetStrOffset = focus_utf16_at * sizeof(WCHAR);

		const LPWSTR text = (LPWSTR)((BYTE*)reconvert_string +
			sizeof(RECONVERTSTRING));
		memcpy(text, text_utf16.c_str(),
			(text_utf16.length() + 1) * sizeof(WCHAR));
	}
	return required_size;
}

void LLWindowWin32::updateLanguageTextInputArea()
{
	if (!mPreeditor)
	{
		return;
	}

	LLCoordGL caret_coord;
	LLRect preedit_bounds;
	if (mPreeditor->getPreeditLocation(-1, &caret_coord, &preedit_bounds, NULL))
	{
		mLanguageTextInputPointGL = caret_coord;
		mLanguageTextInputAreaGL = preedit_bounds;

		CANDIDATEFORM candidate_form;
		fillCandidateForm(caret_coord, preedit_bounds, &candidate_form);

		HIMC himc = ImmGetContext(mWindowHandle);
		// Win32 document says there may be up to 4 candidate windows.
		// This magic number 4 appears only in the document, and
		// there are no constant/macro for the value...
		for (int i = 3; i >= 0; --i)
		{
			candidate_form.dwIndex = i;
			ImmSetCandidateWindow(himc, &candidate_form);
		}
		ImmReleaseContext(mWindowHandle, himc);
	}
}

void LLWindowWin32::interruptLanguageTextInput()
{
	if (mPreeditor)
	{
		HIMC himc = ImmGetContext(mWindowHandle);
		ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
		ImmReleaseContext(mWindowHandle, himc);

		// Win32 document says there will be no composition string after
		// NI_COMPOSITIONSTR returns. The following call to resetPreedit should
		// be a NOP unless IME goes mad...
		mPreeditor->resetPreedit();
	}
}

void LLWindowWin32::handleStartCompositionMessage()
{
	// Let IME know the font to use in feedback UI.
	LOGFONT logfont;
	fillCompositionLogfont(&logfont);
	HIMC himc = ImmGetContext(mWindowHandle);
	ImmSetCompositionFont(himc, &logfont);
	ImmReleaseContext(mWindowHandle, himc);
}

// Handle WM_IME_COMPOSITION message.

void LLWindowWin32::handleCompositionMessage(U32 indexes)
{
	if (!mPreeditor)
	{
		return;
	}

	bool needs_update = false;
	LLWString result_string;
	LLWString preedit_string;
	S32 preedit_string_utf16_length = 0;
	LLPreeditor::segment_lengths_t preedit_segment_lengths;
	LLPreeditor::standouts_t preedit_standouts;

	// Step I: Receive details of preedits from IME.

	HIMC himc = ImmGetContext(mWindowHandle);

	if (indexes & GCS_RESULTSTR)
	{
		LONG size = ImmGetCompositionString(himc, GCS_RESULTSTR, NULL, 0);
		if (size >= 0)
		{
			const LPWSTR data = new WCHAR[size / sizeof(WCHAR) + 1];
			size = ImmGetCompositionString(himc, GCS_RESULTSTR, data, size);
			if (size > 0)
			{
				result_string =
					ll_convert_wide_to_wstring(std::wstring(data,
															size / sizeof(WCHAR)));
			}
			delete[] data;
			needs_update = true;
		}
	}

	if (indexes & GCS_COMPSTR)
	{
		LONG size = ImmGetCompositionString(himc, GCS_COMPSTR, NULL, 0);
		if (size >= 0)
		{
			const LPWSTR data = new WCHAR[size / sizeof(WCHAR) + 1];
			size = ImmGetCompositionString(himc, GCS_COMPSTR, data, size);
			if (size)
			{
				preedit_string_utf16_length = size / sizeof(WCHAR);
				preedit_string =
					ll_convert_wide_to_wstring(std::wstring(data,
															size / sizeof(WCHAR)));
			}
			delete[] data;
			needs_update = true;
		}
	}

	if ((indexes & GCS_COMPCLAUSE) && preedit_string.length())
	{
		LONG size = ImmGetCompositionString(himc, GCS_COMPCLAUSE, NULL, 0);
		if (size > 0)
		{
			const LPDWORD data = new DWORD[size / sizeof(DWORD)];
			size = ImmGetCompositionString(himc, GCS_COMPCLAUSE, data, size);
			if (size >= sizeof(DWORD) * 2 && data[0] == 0 &&
				data[size / sizeof(DWORD) - 1] == preedit_string_utf16_length)
			{
				preedit_segment_lengths.resize(size / sizeof(DWORD) - 1);
				S32 offset = 0;
				for (U32 i = 0; i < preedit_segment_lengths.size(); ++i)
				{
					const S32 length =
						wstring_length_from_utf16_length(preedit_string,
														 offset,
														 data[i + 1] -
														 data[i]);
					preedit_segment_lengths[i] = length;
					offset += length;
				}
			}
			delete[] data;
		}
	}

	if ((indexes & GCS_COMPATTR) && preedit_segment_lengths.size() > 1)
	{
		LONG size = ImmGetCompositionString(himc, GCS_COMPATTR, NULL, 0);
		if (size > 0)
		{
			const LPBYTE data = new BYTE[size / sizeof(BYTE)];
			size = ImmGetCompositionString(himc, GCS_COMPATTR, data, size);
			if (size == preedit_string_utf16_length)
			{
				preedit_standouts.assign(preedit_segment_lengths.size(),
										 false);
				S32 offset = 0;
				for (U32 i = 0; i < preedit_segment_lengths.size(); ++i)
				{
					if (ATTR_TARGET_CONVERTED == data[offset] ||
						ATTR_TARGET_NOTCONVERTED == data[offset])
					{
						preedit_standouts[i] = true;
					}
					offset += wstring_utf16_length(preedit_string, offset,
							  preedit_segment_lengths[i]);
				}
			}
			delete[] data;
		}
	}

	S32 caret_position = preedit_string.length();
	if (indexes & GCS_CURSORPOS)
	{
		const S32 caret_position_utf16 =
			ImmGetCompositionString(himc, GCS_CURSORPOS, NULL, 0);
		if (caret_position_utf16 >= 0 &&
			caret_position <= preedit_string_utf16_length)
		{
			caret_position =
				wstring_length_from_utf16_length(preedit_string, 0,
												 caret_position_utf16);
		}
	}

	if (!indexes)
	{
		// I am not sure this condition really happens, but Windows SDK
		// document says it is an indication of "reset everything."
		needs_update = true;
	}

	ImmReleaseContext(mWindowHandle, himc);

	// Step II: Update the active preeditor.

	if (needs_update)
	{
		if (preedit_string.length() || result_string.length())
		{
			mPreeditor->resetPreedit();
		}

		if (result_string.length())
		{
			for (LLWString::const_iterator i = result_string.begin();
				i != result_string.end(); ++i)
			{
				mPreeditor->handleUnicodeCharHere(*i);
			}
		}

		if (!preedit_string.length())
		{
			preedit_segment_lengths.clear();
			preedit_standouts.clear();
		}
		else
		{
			if (!preedit_segment_lengths.size())
			{
				preedit_segment_lengths.assign(1, preedit_string.length());
			}
			if (!preedit_standouts.size())
			{
				preedit_standouts.assign(preedit_segment_lengths.size(),
										 false);
			}
		}
		mPreeditor->updatePreedit(preedit_string, preedit_segment_lengths,
								  preedit_standouts, caret_position);

		// Some IME does not query char position after WM_IME_COMPOSITION, so
		// we need to update them actively.
		updateLanguageTextInputArea();
	}
}

// Given a text and a focus range, finds and returns a surrounding context of
// the focused subtext. A variable pointed to by offset receives the offset in
// llwchars of the beginning of the returned context string in the given wtext.
static LLWString find_context(const LLWString& wtext, S32 focus,
							  S32 focus_length, S32 *offset)
{
	constexpr S32 CONTEXT_EXCESS = 30;	// This value is empirical

	S32 e = llmin((S32)wtext.length(), focus + focus_length + CONTEXT_EXCESS);
	S32 end = focus + focus_length;
	while (end < e && '\n' != wtext[end])
	{
		end++;
	}

	S32 s = llmax(0, focus - CONTEXT_EXCESS);
	S32 start = focus;
	while (start > s && '\n' != wtext[start - 1])
	{
		--start;
	}

	*offset = start;
	return wtext.substr(start, end - start);
}

// Handles WM_IME_REQUEST message. If it handled the message, returns true.
// Otherwise, false. When it handled the message, the value to be returned from
// the Window Procedure is set to *result.
bool LLWindowWin32::handleImeRequests(WPARAM request, LPARAM param,
									  LRESULT* result)
{
	if (!mPreeditor)
	{
		return false;
	}

	switch (request)
	{
		// http://msdn2.microsoft.com/en-us/library/ms776080.aspx
		case IMR_CANDIDATEWINDOW:
		{
			LLCoordGL caret_coord;
			LLRect preedit_bounds;
			mPreeditor->getPreeditLocation(-1, &caret_coord,
				&preedit_bounds, NULL);

			CANDIDATEFORM* const form = (CANDIDATEFORM*)param;
			DWORD const dwIndex = form->dwIndex;
			fillCandidateForm(caret_coord, preedit_bounds, form);
			form->dwIndex = dwIndex;

			*result = 1;
			return true;
		}

		case IMR_QUERYCHARPOSITION:
		{
			IMECHARPOSITION* const char_position = (IMECHARPOSITION*)param;

			// char_position->dwCharPos counts in number of
			// WCHARs, i.e., UTF-16 encoding units, so we cannot simply
			// pass the number to getPreeditLocation.

			const LLWString& wtext = mPreeditor->getWText();
			S32 preedit, preedit_length;
			mPreeditor->getPreeditRange(&preedit, &preedit_length);
			LLCoordGL caret_coord;
			LLRect preedit_bounds, text_control;
			S32 position =
				wstring_length_from_utf16_length(wtext, preedit,
												 char_position->dwCharPos);

			if (!mPreeditor->getPreeditLocation(position, &caret_coord,
												&preedit_bounds,
												&text_control))
			{
				llwarns << "IMR_QUERYCHARPOSITON called but getPreeditLocation() failed."
						<< llendl;
				return false;
			}
			fillCharPosition(caret_coord, preedit_bounds, text_control,
							 char_position);

			*result = 1;
			return true;
		}

		case IMR_COMPOSITIONFONT:
		{
			fillCompositionLogfont((LOGFONT*)param);

			*result = 1;
			return true;
		}

		case IMR_RECONVERTSTRING:
		{
			mPreeditor->resetPreedit();
			const LLWString & wtext = mPreeditor->getWText();
			S32 select, select_length;
			mPreeditor->getSelectionRange(&select, &select_length);

			S32 context_offset;
			const LLWString context = find_context(wtext, select,
												   select_length,
												   &context_offset);

			RECONVERTSTRING* const reconvert_string = (RECONVERTSTRING*)param;
			const U32 size = fillReconvertString(context,
												 select - context_offset,
												 select_length,
												 reconvert_string);
			if (reconvert_string)
			{
				if (select_length == 0)
				{
					// Let the IME to decide the reconversion range, and adjust
					// the reconvert_string structure accordingly.
					HIMC himc = ImmGetContext(mWindowHandle);
					BOOL adjusted =
						ImmSetCompositionString(himc, SCS_QUERYRECONVERTSTRING,
												reconvert_string, size, NULL,
												0);
					ImmReleaseContext(mWindowHandle, himc);
					if (adjusted)
					{
						const llutf16string& text_utf16 =
							wstring_to_utf16str(context);
						S32 new_preedit_start =
							reconvert_string->dwCompStrOffset / sizeof(WCHAR);
						S32 new_preedit_end =
							new_preedit_start + reconvert_string->dwCompStrLen;
						select = utf16str_wstring_length(text_utf16,
														 new_preedit_start);
						select_length =
							utf16str_wstring_length(text_utf16,
													new_preedit_end) - select;
						select += context_offset;
					}
				}
				mPreeditor->markAsPreedit(select, select_length);
			}

			*result = size;
			return true;
		}

		case IMR_CONFIRMRECONVERTSTRING:
		{
			*result = FALSE;
			return true;
		}

		case IMR_DOCUMENTFEED:
		{
			const LLWString & wtext = mPreeditor->getWText();
			S32 preedit, preedit_length;
			mPreeditor->getPreeditRange(&preedit, &preedit_length);

			S32 context_offset;
			LLWString context = find_context(wtext, preedit, preedit_length,
											 &context_offset);
			preedit -= context_offset;
			if (preedit_length > 0 && preedit >= 0)
			{
				// IMR_DOCUMENTFEED may be called when we have an active
				// preedit. We should pass the context string *excluding*
				// the preedit string. Otherwise, some IME are confused.
				context.erase(preedit, preedit_length);
			}

			RECONVERTSTRING* reconvert_string = (RECONVERTSTRING*)param;
			*result = fillReconvertString(context, preedit, 0,
										  reconvert_string);
			return true;
		}

		default:
			break;
	}

	return false;
}

//static
void LLWindowWin32::setDPIAwareness()
{
	HMODULE shcorep = LoadLibrary(L"shcore.dll");
	if (!shcorep)
	{
		llwarns << "Could not load the shcore.dll library. Will use legacy DPI awareness API of Windows 7"
				<< llendl;
		return;
	}

	SetProcessDpiAwarenessType spdap =
		(SetProcessDpiAwarenessType)GetProcAddress(shcorep,
												   "SetProcessDpiAwareness");
	if (spdap)
	{
		HRESULT hr = spdap(PROCESS_PER_MONITOR_DPI_AWARE);
		if (hr != S_OK)
		{
			llwarns << "SetProcessDpiAwareness() returned an error; will use legacy DPI awareness API of Windows 7"
					<< llendl;
		}
	}

	FreeLibrary(shcorep);
}

F32 LLWindowWin32::getSystemUISize()
{
	F32 scale_value = 1.f;
	if (!gHiDPISupport)
	{
		return scale_value;
	}

	HWND hwnd = (HWND)getPlatformWindow();
	HDC hdc = GetDC(hwnd);

	HMODULE shcorep = LoadLibrary(L"shcore.dll");
	if (shcorep)
	{
		GetProcessDpiAwarenessType gpdap =
			(GetProcessDpiAwarenessType)GetProcAddress(shcorep,
													   "GetProcessDpiAwareness");
		GetDpiForMonitorType gdfmp =
			(GetDpiForMonitorType)GetProcAddress(shcorep, "GetDpiForMonitor");
		if (gpdap && gdfmp)
		{
			HANDLE hprocess = GetCurrentProcess();
			PROCESS_DPI_AWARENESS dpi_awareness;
			gpdap(hprocess, &dpi_awareness);
			if (dpi_awareness == PROCESS_PER_MONITOR_DPI_AWARE)
			{
				RECT rect;
				GetWindowRect(hwnd, &rect);

				// Get the DPI for the monitor, on which the center of window
				// is displayed and set the scaling factor
				POINT pt;
				pt.x = (rect.left + rect.right) / 2;
				pt.y = (rect.top + rect.bottom) / 2;
				HMONITOR hmonitor = MonitorFromPoint(pt,
													 MONITOR_DEFAULTTONEAREST);

				UINT dpix = 0, dpiy = 0;
				HRESULT hr = gdfmp(hmonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy);
				if (hr == S_OK)
				{
					scale_value = F32(dpix) / F32(USER_DEFAULT_SCREEN_DPI);
				}
				else
				{
					llwarns << "Could not determine DPI for monitor; setting scale to 100%."
							<< llendl;
				}
			}
			else
			{
				llwarns << "Process is not per-monitor DPI-aware; setting scale to 100%."
						<< llendl;
			}
		}
		FreeLibrary(shcorep);
	}
	else
	{
		llwarns << "Could not load shcore.dll library; using legacy DPI awareness API of Windows 7."
				<< llendl;
		scale_value = F32(GetDeviceCaps(hdc, LOGPIXELSX)) /
					  F32(USER_DEFAULT_SCREEN_DPI);
	}

	ReleaseDC(hwnd, hdc);

	return scale_value;
}

//static
std::vector<std::string> LLWindowWin32::getDynamicFallbackFontList()
{
	// Fonts previously in getFontListSans() have moved to fonts.xml.
	return std::vector<std::string>();
}

#endif // LL_WINDOWS
