/**
 * @file llwindowsdl.cpp
 * @brief SDL implementation of LLWindow class
 * @author This module has many fathers, and it shows.
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

#if LL_LINUX

#include "linden_common.h"
#include "indra_constants.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "fontconfig/fontconfig.h"

#include "llwindowsdl.h"

#include <regex>	// For regex in x11_detect_vram_kb_from_file()

#include "lldir.h"
#include "llfasttimer.h"
#include "llfindlocale.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llkeyboardsdl.h"
#include "llstring.h"
#include "lltimer.h"

// *HACK: stash a pointer to the LLWindowSDL object here and maintain in the
// constructor and destructor. This assumes that there will be only one object
// of this class at any time. Currently this is true.
static LLWindowSDL* sWindowImplementation = NULL;

// *HACK: to avoid white screen flickering; defined in llviewerdisplay.cpp
extern U32 gFrameSleepTime;

bool gXlibThreadSafe = false;
bool gXWayland = false;
bool gUseFullDesktop = false;

constexpr S32 MAX_NUM_RESOLUTIONS = 200;

S32 exec_cmd(const std::string& cmd, const std::string& arg)
{
	char* const argv[] = { (char*)cmd.c_str(), (char*)arg.c_str(), NULL };
	fflush(NULL);
	pid_t pid = fork();
	if (pid == 0)
	{
		// Child path. Disconnect from stdin/stdout/stderr, or child will
		// keep our output pipe undesirably alive if it outlives us.
		close(0);
		close(1);
		close(2);
		// End ourself by running the command
		execv(cmd.c_str(), argv);
		// If execv returns at all, there was a problem.
		llwarns << "execv() failure when trying to start: " << cmd << llendl;
		_exit(1); // _exit because we do not want atexit() clean-up !
	}
	else if (pid > 0)
	{
		// Parent path. Wait for child to die.
		int child_exit_status;
		waitpid(pid, &child_exit_status, 0);
		return child_exit_status;
	}
	else
	{
		llwarns << "Fork failure." << llendl;
	}
	return -1;
}

//static
void LLWindowSDL::initXlibThreads()
{
	// Ensure Xlib is started in thread-safe state, so that the NVIDIA drivers
	// can use multi-threading.
	if (!gXlibThreadSafe)
	{
		gXlibThreadSafe = XInitThreads();
		if (gXlibThreadSafe)
		{
			llinfos << "Xlib successfully initialized in thread-safe state"
					<< llendl;
		}
		else
		{
			llwarns << "Failed to initialize Xlib in thread-safe state: NVIDIA drivers will run single-threaded."
					<< llendl;
		}
	}
}

//static
Window LLWindowSDL::getSDLXWindowID()
{
	if (sWindowImplementation)
	{
		return sWindowImplementation->mSDL_XWindowID;
	}
	return None;
}

//static
Display* LLWindowSDL::getSDLDisplay()
{
	if (sWindowImplementation)
	{
		return sWindowImplementation->mSDL_Display;
	}
	return NULL;
}

LLWindowSDL::LLWindowSDL(const std::string& title, S32 x, S32 y, U32 width,
						 U32 height, U32 flags, bool fullscreen,
						 bool disable_vsync, U32 fsaa_samples)
:	LLWindow(fullscreen, flags),
	mWindow(NULL),
	mSDLFlags(0),
	mInitialPosX(x),
	mInitialPosY(y),
	mPosOffsetX(-1),
	mPosOffsetY(-1),
	mCustomGammaSet(false),
	mKeyVirtualKey(0),
	mKeyModifiers(KMOD_NONE),
	mCaptured(false),
	mGrabbyKeyFlags(0),
	mFSAASamples(fsaa_samples),
	mOriginalAspectRatio(4.f / 3.f),	// Assume 4:3 until we know better
	mWindowTitle(title.empty() ? "SL viewer" : title)
{
	// Initialize the keyboard. Note that we cannot set up key-repeat until
	// after SDL has initialized the video
	gKeyboardp = new LLKeyboardSDL();

	// This should have already been called in LLSplashScreenSDL, but better
	// safe than sorry...
	initXlibThreads();

	// Wayland *SUCKS*, and XWayland is *NOT* 100% X11-compatible...
	char* wayland_env = getenv("WAYLAND_DISPLAY");
	gXWayland = wayland_env && wayland_env[0];
	if (gXWayland)
	{
		llwarns << "XWayland compatibility mode detected. This will cause unexpected behaviours. The viewer is a genuine X11 application, not a Wayland one, please run it under a genuine X11 server. NO SUPPORT provided for viewer sessions ran under XWayland !"
				<< llendl;
	}

	mSDL_XWindowID = None;
	mSDL_Display = NULL;
	mContext = {};
	memset(mCurrentGammaRamp, 0, sizeof(mCurrentGammaRamp));
	memset(mPrevGammaRamp, 0, sizeof(mPrevGammaRamp));

	// Create the GL context and set it up for windowed or fullscreen, as
	// appropriate.
	if (createContext(x, y, width, height, 32, fullscreen, disable_vsync))
	{
		gGLManager.initGL();

		// Start with arrow cursor
		initCursors();
		setCursor(UI_CURSOR_ARROW);
	}

	stop_glerror();

	sWindowImplementation = this;

	mFlashing = false;

	initialiseX11Clipboard();
}

//virtual
LLWindowSDL::~LLWindowSDL()
{
	quitCursors();
	destroyContext();

	if (mSupportedResolutions)
	{
		delete[] mSupportedResolutions;
		mSupportedResolutions = NULL;
	}

	sWindowImplementation = NULL;
}

static SDL_Surface* load_bmp_resource(const char* basename)
{
	constexpr S32 PATH_BUFFER_SIZE = 1000;
	char path_buffer[PATH_BUFFER_SIZE];

	// Figure out where our BMP is living on the disk
	snprintf(path_buffer, PATH_BUFFER_SIZE - 1, "%s/res-sdl/%s",
			 gDirUtilp->getAppRODataDir().c_str(), basename);
	path_buffer[PATH_BUFFER_SIZE - 1] = '\0';

	return SDL_LoadBMP(path_buffer);
}

// This function scans the Xorg log to determine the amount of VRAM available
// to the system.
// Returns	-1 if it could not open the file.
//           0 if it could open the file but could not detect the amount of
//			   VRAM.
//          >0 (VRAM amount in kilobytes) if successful.
static S32 x11_detect_vram_kb_from_file(const std::string& filename)
{
	std::ifstream in(filename.c_str());
	if (!in.is_open())
	{
		LL_DEBUGS("Window") << "Could not open file: " << filename << LL_ENDL;
		return -1;
	}
	std::regex pattern(".*?(VRAM|Memory|Video\\s?RAM)\\D*(\\d+)\\s?([kK]B?)");
	S32 amount = 0;
	std::string line;
	while (std::getline(in, line))
	{
		std::cmatch match;
		if (std::regex_search(line.c_str(), match, pattern))
		{
			amount = atoi(std::string(match[2]).c_str());
			LL_DEBUGS("Window") << "Match found in line: " << line
								<< "VRAM amount: " << amount << LL_ENDL;
		}
	}
	in.close();
	return amount;
}

static S32 x11_detect_vram_kb()
{
	S32 amount = 0;
	// Let the user override the detection in case it fails on their system.
	// They can specify the amount of VRAM in megabytes, via the LL_VRAM_MB
	// environment variable.
	char* vram_override = getenv("LL_VRAM_MB");
	if (vram_override)
	{
		amount = atoi(vram_override);
		if (amount > 0)
		{
			llinfos << "Amount of VRAM overridden via the LL_VRAM_MB environment variable; detection step skipped."
					<< llendl;
			return 1024 * amount;	// Converted to kilobytes
		}
	}

	// We parse VGL_DISPLAY first so we can grab the right Xorg filename if we
	// are using VirtualGL (like Optimus systems do).
	char* display_env = getenv("VGL_DISPLAY"); // e.g. :0 or :0.0 or :1.0 etc
	if (!display_env)
	{
		display_env = getenv("DISPLAY");
	}

	// Parse DISPLAY number so we can go grab the right log file
	S32 display_num = 0;
	if (display_env && display_env[0] == ':' && display_env[1] >= '0' &&
		display_env[1] <= '9')
	{
		display_num = display_env[1] - '0';
	}

	// *TODO: we could be smarter and see which of Xorg/XFree86 has the
	// freshest time-stamp.

	// Try Xorg log first
	std::string x_log_location = "/var/log/";
	std::string fname = x_log_location + "Xorg.";
	fname += ('0' + display_num);
	fname += ".log";
	llinfos << "Looking in " << fname << " for VRAM info..." << llendl;
	amount = x11_detect_vram_kb_from_file(fname);
	if (amount < 0)
	{
		llinfos << "Could not open " << fname << " - skipped." << llendl;
		// Try old XFree86 log otherwise
		fname = x_log_location + "XFree86.";
		fname += ('0' + display_num);
		fname += ".log";
		amount = x11_detect_vram_kb_from_file(fname);
		if (amount < 0)
		{
			llinfos << "Could not open " << fname << " - skipped." << llendl;
			amount = 0;
		}
	}
	if (amount > 0)
	{
		llinfos << "X11 log-parser detected " << amount / 1024 << "MB VRAM."
				<< llendl;
	}
	else
	{
		llwarns << "VRAM amount detection failed. You could use the LL_VRAM_MB environment variable to specify it. "
				<< llendl;
	}
	return amount;
}

//virtual
void LLWindowSDL::setWindowTitle(const std::string& title)
{
	// Remember the new title, for when we switch context
	mWindowTitle = title;
	if (mWindow)
	{
		SDL_SetWindowTitle(mWindow, title.c_str());
	}
}

//virtual
bool LLWindowSDL::getFullScreenSize(S32& width, S32& height)
{
	// When width and height are not 0, consider we already know what size we
	// can use.
	if (width != 0 && height != 0)
	{
		return true;
	}

	// Scan through the list of modes, looking for one which has height between
	// 700 and 800 and aspect ratio closest to the user's original mode.
	S32 res_count = 0;
	LLWindowResolution* res_list = getSupportedResolutions(res_count);
	if (res_list)
	{
		F32 closest_aspect = 0.f;
		S32 closest_width = 0;
		S32 closest_height = 0;

		llinfos << "Searching for a display mode, original aspect is "
				<< mOriginalAspectRatio << llendl;

		for (S32 i = 0; i < res_count; ++i)
		{
			S32 h = res_list[i].mHeight;
			S32 w = res_list[i].mWidth;
			F32 aspect = (F32)w / (F32)h;
			llinfos << "width = " << w << " - height = " << h << " - aspect = "
					<< aspect;
			if (h >= 700 && h <= 800 &&
				fabs(aspect - mOriginalAspectRatio) <
					fabs(closest_aspect - mOriginalAspectRatio))
			{
				llcont << " (new closest mode)";
				closest_width = w;
				closest_height = h;
				closest_aspect = aspect;
			}
			llcont << llendl;
		}
		width = closest_width;
		height = closest_height;
	}

	if (width == 0 || height == 0)
	{
		// Mode search failed: used some common/acceptable default.
		width = 1024;
		height = 768;
		return false;
	}

	return true;
}

// The Cool VL Viewer window icon is opaque, so no need to bother about the
// transparency code... HB
#define WINDOW_ICON_TANSPARENCY 0

// Reimplementation of SDL1's mask generation for SDL_WM_SetIcon()
static Uint8* generate_icon_mask(SDL_Surface* icon)
{
	S32 width = icon->w;
	S32 height = icon->h;
	S32 bpl = (width + 7) / 8;	// Bytes per line
	S32 mask_len = height * bpl;
	Uint8* mask = (Uint8*)malloc(mask_len);
	if (!mask)
	{
		return NULL;
	}

	// Set as an opaque mask (all bits at 1 in the mask)
	memset((void*)mask, ~0, mask_len);

#if WINDOW_ICON_TANSPARENCY
	Uint32 colorkey;
	if (SDL_GetColorKey(icon, &colorkey) == 0)
	{
		S32 picth = icon->pitch;
		switch (icon->format->BytesPerPixel)
		{
			case 1:
			{
				for (S32 y = 0; y < height; ++y)
				{
					Uint8* pixels = (Uint8*)icon->pixels + y * picth;
					for (S32 x = 0; x < width; ++x)
					{
						if (*pixels++ == colorkey)
						{
							mask[y * bpl + x / 8] &= ~(1 << (7 - x % 8));
						}
					}
				}
				break;
			}

			case 2:
			{
				for (S32 y = 0; y < height; ++y)
				{
					Uint16* pixels = (Uint16*)icon->pixels + y * picth / 2;
					for (S32 x = 0; x < width; ++x)
					{
						if (*pixels++ == colorkey)
						{
							mask[y * bpl + x / 8] &= ~(1 << (7 - x % 8));
						}
					}
				}
				break;
			}

			case 4:
			{
				for (S32 y = 0; y < height; ++y)
				{
					Uint32* pixels = (Uint32*)icon->pixels + y * picth / 4;
					for (S32 x = 0; x < width; ++x)
					{
						if (*pixels++ == colorkey)
						{
							mask[y * bpl + x / 8] &= ~(1 << (7 - x % 8));
						}
					}
				}
				break;
			}

			default:	// Invalid icon ?... Return an opaque mask
				return mask;
		}

		// We need the mask as given, except in LSBfirst format instead of
		// MSBfirst. Reverse the bits in each byte.
		Uint8* lsb_mask = (Uint8*)malloc(mask_len);
		if (!lsb_mask)
		{
			free(mask);
			return NULL;
		}

		memset((void*)lsb_mask, 0, mask_len);
		for (S32 i = 0; i < mask_len; ++i)
		{
			Uint8 x = mask[i];
			x = (x & 0xaa) >> 1 | (x & 0x55) << 1;
			x = (x & 0xcc) >> 2 | (x & 0x33) << 2;
			x = (x & 0xf0) >> 4 | (x & 0x0f) << 4;
			lsb_mask[i] = x;
		}
		free(mask);

		return lsb_mask;
	}
#endif

	// Opaque mask.
	return mask;
}

// This method must be called at the end of createContext() so that
// mSDL_Display and mSDL_XWindowID got initialized...
void LLWindowSDL::setWindowIcon()
{
#if 0	// Does not seem to make any difference...
	// Give an icon name hint to X11
	static const char* icon_hint = "cvlv_icon";
	XTextProperty icon_prop;
	XStringListToTextProperty((char**)&icon_hint, 1, &icon_prop);
	XSetWMIconName(mSDL_Display, mSDL_XWindowID, &icon_prop);
	XFree(icon_prop.value);
#endif

	// Set the application icon.
	S32 icon_size = 48;
	char* envvar = getenv("LL_WINDOW_ICON_SIZE");
	if (envvar)
	{
		icon_size = atoi(envvar);
		if (icon_size != 32 && icon_size != 48 && icon_size != 64 &&
			icon_size != 128 && icon_size != 256)
		{
			icon_size = 48;
		}
	}
	std::string icon_name = llformat("cvlv_icon%d.bmp", icon_size);
	SDL_Surface* icon = load_bmp_resource(icon_name.c_str());
	if (!icon)
	{
		return;
	}

#if WINDOW_ICON_TANSPARENCY
	// This attempts to give a black-keyed mask to the icon.
	SDL_SetColorKey(icon, SDL_TRUE, SDL_MapRGB(icon->format, 0, 0, 0));
#endif

	bool success = false;

	// Reimplementation of SDL1's SDL_WM_SetIcon() for X11
	do
	{
		// *FIXME: 32 bits is what *appears* to be needed, although the default
		// visual depth is 24 bits... There is some voodoo magic performed by
		// SDL1 with available visuals and their possible depths to determine
		// the right pixel format, but implementing it seems quite an overkill:
		// 32 bits *should* work for everyone ! HB
		S32 bpp = 32;
		// And just in case, the LL_WINDOW_ICON_BPP environment variable allows
		// to change that value at runtime...
		envvar = getenv("LL_WINDOW_ICON_BPP");
		if (envvar)
		{
			bpp = atoi(envvar);
			if (bpp != 32 && bpp != 24 && bpp != 16 && bpp != 15)
			{
				// We would not deal with those... Let's try and fallback to
				// SDL_SetWindowIcon() for them.
				break;
			}
			llinfos << "Using " << bpp
					<< " bits per pixel for the window icon." << llendl;
		}

		// Various X11 data needed below...
		S32 screen = DefaultScreen(mSDL_Display);
		S32 default_depth = DefaultDepth(mSDL_Display, screen);
		if (default_depth == 8)
		{
			break;
		}
		Visual* default_visual = DefaultVisual(mSDL_Display, screen);
		S32 root_window = RootWindow(mSDL_Display, screen);

		S32 width = icon->w;
		S32 height = icon->h;
		SDL_Surface* sicon = SDL_CreateRGBSurface(0, width, height, bpp,
												  default_visual->red_mask,
												  default_visual->green_mask,
												  default_visual->blue_mask,
												  0);
		if (!sicon)
		{
			break;
		}

		// At this point, I skip all the 8 bits depth conversion code, since we
		// excluded this case above already... HB

		SDL_Rect bounds;
		bounds.x = 0;
		bounds.y = 0;
		bounds.w = width;
		bounds.h = height;
		if (SDL_LowerBlit(icon, &bounds, sicon, &bounds) != 0)
		{
			SDL_FreeSurface(sicon);
			break;
		}

		// Generate a mask.
		Uint8* mask = generate_icon_mask(icon);
		if (!mask)
		{
			SDL_FreeSurface(sicon);
			break;
		}

		Pixmap mask_pixmap = XCreatePixmapFromBitmapData(mSDL_Display,
														 mSDL_XWindowID,
														 (char*)mask,
														 width, height,
														 1L, 0L, 1);
		free(mask);

		// Transfer the image to an X11 pixmap
		XImage* icon_image = XCreateImage(mSDL_Display, default_visual,
										  default_depth, ZPixmap, 0,
										  (char*)sicon->pixels, width, height,
										  32, 0);
		if (!icon_image)
		{
			SDL_FreeSurface(sicon);
			break;
		}
#if LL_BIG_ENDIAN
		icon_image->byte_order = MSBFirst;
#else
		icon_image->byte_order = LSBFirst;
#endif

		Pixmap icon_pixmap = XCreatePixmap(mSDL_Display, root_window,
										   width, height, default_depth);

		XGCValues gc_values;
		GC gc = XCreateGC(mSDL_Display, icon_pixmap, 0, &gc_values);
		XPutImage(mSDL_Display, icon_pixmap, gc, icon_image, 0, 0, 0, 0,
				  width, height);
		XFreeGC(mSDL_Display, gc);
		sicon->pixels = NULL;

		// I skip the SDL_VIDEO_X11_ICONWIN fix here, which was only for use
		// with some old buggy versions of Enlightenment... HB

		// Set the window icon to the icon pixmap and associated mask
		XWMHints* wmhints = XAllocWMHints();
		wmhints->flags = IconPixmapHint | IconMaskHint | InputHint;
		wmhints->icon_pixmap = icon_pixmap;
		wmhints->icon_mask = mask_pixmap;
		wmhints->input = True;
		XSetWMHints(mSDL_Display, mSDL_XWindowID, wmhints);
		XFree(wmhints);
		XSync(mSDL_Display, False);
		success = true;
	}
	while (false);

	// Fallback code, using SDL2's SDL_SetWindowIcon()
	if (!success)
	{
		// SDL2's SDL_SetWindowIcon() fails to set the application menu button
		// in the title bar (and Meta-TAB window cycler, unless the BMP icon
		// size is exactly 32x32 pixels for the latter... Go figure !), while
		// SDL1's SDL_WM_SetIcon() works fine in this respect ! :-(  HB
		SDL_Surface* icon2 = load_bmp_resource("cvlv_icon32.bmp");
		if (icon2)
		{
			SDL_FreeSurface(icon);
			icon = icon2;
#if WINDOW_ICON_TANSPARENCY
			// This attempts to give a black-keyed mask to the icon.
			SDL_SetColorKey(icon, SDL_TRUE, SDL_MapRGB(icon->format, 0, 0, 0));
#endif
		}
		SDL_SetWindowIcon(mWindow, icon);
	}

	SDL_FreeSurface(icon);
}

bool LLWindowSDL::createContext(S32 x, S32 y, S32 width, S32 height, S32 bits,
								bool fullscreen, bool disable_vsync)
{
	llinfos << "Fullscreen = " << fullscreen << " - Size = " << width << "x"
			<< height << llendl;

	// Captures do not survive contexts
	mGrabbyKeyFlags = 0;
	mCaptured = false;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		llinfos << "sdl_init() failed !  " << SDL_GetError() << llendl;
		setupFailure("sdl_init() failure, window creation error");
		return false;
	}

	SDL_version c_sdl_version;
	SDL_VERSION(&c_sdl_version);
	llinfos << "Compiled against SDL " << S32(c_sdl_version.major) << "."
			<< S32(c_sdl_version.minor) << "." << S32(c_sdl_version.patch)
			<< llendl;

	SDL_version r_sdl_version;
	SDL_GetVersion(&r_sdl_version);
	llinfos << "Running against SDL " << S32(r_sdl_version.major) << "."
			<< S32(r_sdl_version.minor) << "." << S32(r_sdl_version.patch)
			<< llendl;

	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm) == 0)
	{
		if (dm.h > 0)
		{
			mOriginalAspectRatio = (F32)dm.w / (F32)dm.h;
			llinfos << "Original aspect ratio was " << dm.w << ":" << dm.h
					<< " = " << mOriginalAspectRatio << llendl;
		}
	}

	if (width == 0)
	{
		width = 1024;
	}
	if (height == 0)
	{
		height = 768;
	}

	mFullscreen = fullscreen;

	mSDLFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#if 0
	if (fullscreen)
	{
		mSDLFlags |= SDL_WINDOW_FULLSCREEN;
		getFullScreenSize(width, height);
	}
#endif

	GLint alpha_bits = 8;
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, alpha_bits);

	GLint red_bits = 8;
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, red_bits);

	GLint green_bits = 8;
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, green_bits);

	GLint blue_bits = 8;
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, blue_bits);

	GLint depth_bits = bits <= 16 ? 16 : 24;
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth_bits);

	// Note: we need stencil support for a few (minor) things.
	GLint stencil_bits = 8;
	if (getenv("LL_GL_NO_STENCIL"))
	{
		stencil_bits = 0;
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencil_bits);
	}

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (mFSAASamples > 0)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, mFSAASamples);
	}

	U32 context_flags = gDebugGL ? SDL_GL_CONTEXT_DEBUG_FLAG : 0;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);

	if (LLRender::sGLCoreProfile)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
							SDL_GL_CONTEXT_PROFILE_CORE);
	}

	// Request shared context support.
	SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

	mWindow = SDL_CreateWindow(mWindowTitle.c_str(), x, y, width, height,
							   mSDLFlags);
	if (!mWindow)
	{
		llwarns << "Window creation failure. SDL error: " << SDL_GetError()
				<< llendl;
		setupFailure("Window creation failure.");
		return false;
	}

	// Clear the window (in the two buffers)
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(mWindow);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	if (mFullscreen)
	{
#if 0	// This should be OK, now that we forbid changes during sessions. HB
		if (LLRender::sGLCoreProfile)
		{
			// Full screen mode crashes the viewer when core GL profile is
			// enabled... Use full desktop mode instead. HB
			gUseFullDesktop = true;
		}
#endif
		U32 flags = gUseFullDesktop ? SDL_WINDOW_FULLSCREEN_DESKTOP
									: SDL_WINDOW_FULLSCREEN;
		mFullscreen = SDL_SetWindowFullscreen(mWindow, flags) == 0;
		if (!mFullscreen && !gUseFullDesktop)
		{
			gUseFullDesktop = true;	// For next time...
			llwarns << "Failed to set real full screen mode, trying full desktop mode..."
					<< llendl;
			flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
			mFullscreen = SDL_SetWindowFullscreen(mWindow, flags) == 0;
		}
		if (!mFullscreen)
		{
			llwarns << "Failure to set up full screen window " << width << "x"
					<< height << llendl;
		}
	}

	if (mFullscreen)
	{
		llinfos << "Setting up fullscreen " << width << "x" << height
				<< llendl;

		SDL_DisplayMode target_mode;
		SDL_zero(target_mode);
		target_mode.w = width;
		target_mode.h = height;

		SDL_DisplayMode closest_mode;
		SDL_zero(closest_mode);
		SDL_GetClosestDisplayMode(SDL_GetWindowDisplayIndex(mWindow),
								  &target_mode, &closest_mode);
		if (SDL_SetWindowDisplayMode(mWindow, &closest_mode) == 0)
		{
			SDL_DisplayMode mode;
			SDL_GetWindowDisplayMode(mWindow, &mode);
			mFullscreenWidth = mode.w;
			mFullscreenHeight = mode.h;
			mFullscreenBits = SDL_BITSPERPIXEL(mode.format);
			mFullscreenRefresh = mode.refresh_rate;
			llinfos << "Running at " << mFullscreenWidth << "x"
					<< mFullscreenHeight << "x" << mFullscreenBits << " @ "
					<< mFullscreenRefresh << "Hz" << llendl;
		}
		else
		{
			llwarns << "Fullscreen creation failure. SDL error: "
					<< SDL_GetError() << llendl;
			// No fullscreen support
			mFullscreen = false;
			mFullscreenWidth = -1;
			mFullscreenHeight = -1;
			mFullscreenBits = -1;
			mFullscreenRefresh = -1;
			SDL_SetWindowFullscreen(mWindow, 0);
			SDL_SetWindowResizable(mWindow, SDL_TRUE);
			std::string error =
				llformat("Unable to run fullscreen at %d x %d.", width,
						 height);
			setupFailure(error);
			return false;
		}
	}

	if (LLRender::sGLCoreProfile)
	{
		U32 major = 4;
		U32 minor = 6;
		while (true)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
			mContext = SDL_GL_CreateContext(mWindow);
			if (mContext)
			{
				llinfos << "Activated core GL profile v" << major << "."
						<< minor << llendl;
				break;	// Success !
			}
			if (minor > 0)
			{
				--minor;
			}
			else if (major == 4)
			{
				// Continue from 3.3 downwards
				major = minor = 3;
			}
			else
			{
				break;	// Failed to set core GL profile...
			}
		}
	}
	else
	{
		mContext = SDL_GL_CreateContext(mWindow);
	}
	if (!mContext)
	{
		llwarns << "Cannot create GL context: " << SDL_GetError() << llendl;
		setupFailure("GL context creation error");
		return false;
	}

	// Detect video memory size.
	S32 vram_mb = x11_detect_vram_kb() / 1024;
	if (vram_mb > 0 && !gGLManager.mVRAM)
	{
		gGLManager.mVRAM = vram_mb;
	}
	// If VRAM is not detected, that is handled later

	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &red_bits);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &green_bits);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &blue_bits);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &alpha_bits);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth_bits);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits);

	llinfos << "GL buffer:" << llendl;
	llinfos << "  Red bits " << S32(red_bits) << llendl;
	llinfos << "  Green bits " << S32(green_bits) << llendl;
	llinfos << "  Blue bits " << S32(blue_bits) << llendl;
	llinfos	<< "  Alpha bits " << S32(alpha_bits) << llendl;
	llinfos	<< "  Depth bits " << S32(depth_bits) << llendl;
	llinfos	<< "  Stencil bits " << S32(stencil_bits) << llendl;

	GLint color_bits = red_bits + green_bits + blue_bits + alpha_bits;
	// *FIXME: actually it is REALLY important for picking that we get at least
	// 8 bits each of red,green,blue. Alpha we can be a bit more relaxed about
	// if we have to.
	if (color_bits < 32)
	{
		close();
		setupFailure(
			"Second Life requires True Color (32 bits) to run in a window.\n"
			"Please go to Control Panels -> Display -> Settings and\n"
			"set the screen to 32 bits color.\n"
			"Alternately, if you choose to run fullscreen, Second Life\n"
			"will automatically adjust the screen each time it runs.");
		return false;
	}

	// Grab the window manager specific information
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(mWindow, &info))
	{
		// Save the information for later use
		if (info.subsystem == SDL_SYSWM_X11)
		{
			mSDL_Display = info.info.x11.display;
			mSDL_XWindowID = info.info.x11.window;
		}
		else
		{
			llwarns << "We are not running under X11 !" << llendl;
		}
	}
	else
	{
		llwarns << "We are not running under any known WM !" << llendl;
	}

	// Set the application icon.
	setWindowIcon();

	SDL_StartTextInput();

	// Make sure multisampling is disabled by default
	glDisable(GL_MULTISAMPLE);

	// We do not need to get the current gamma, since there is a call that
	// restores it to the system defaults.
	return true;
}

// Changes fullscreen resolution, or switches between windowed and fullscreen
// modes.
//virtual
bool LLWindowSDL::switchContext(bool fullscreen, const LLCoordScreen& size,
								bool disable_vsync,
								const LLCoordScreen* const posp)
{
	llinfos << "Fullscreen: " << (fullscreen ? "yes" : "no") << llendl;

	// Just nuke the context and start over.
	destroyContext();
	bool result = createContext(0, 0, size.mX, size.mY, 32, fullscreen,
								disable_vsync);
	if (result)
	{
		gGLManager.initGL();

		// Start with arrow cursor
		initCursors();
		setCursor(UI_CURSOR_ARROW);
	}

	stop_glerror();

	return result;
}

struct LLSharedOpenGLContext
{
	SDL_GLContext mContext;
};

//virtual
void* LLWindowSDL::createSharedContext()
{
	LLSharedOpenGLContext* context = new LLSharedOpenGLContext;
	context->mContext = SDL_GL_CreateContext(mWindow);
	if (context->mContext)
	{
		// Do not use VSYNC on any shared context since they are not used for
		// actual rendering. HB
		SDL_GL_SetSwapInterval(0);
	}
	// Make our main (renderer) context current again (even if the new context
	// creation failed, since SDL2 documentation does not say anything about
	// what happens to the current context selection in this case). HB
	SDL_GL_MakeCurrent(mWindow, mContext);
	if (!context->mContext)
	{
		// Something went (very) wrong... Free the structure and return a NULL
		// pointer to signify we do not have a GL context available. HB
		llwarns_sparse << "Failed to create a new shared GL context."
					   << llendl;
		delete context;
		return NULL;
	}
	// *HACK: this will cause a proper screen refresh by triggering a full
	// redraw event at the SDL level. Without this hack, you get a "blocky"
	// UI until SDL receives a redraw event (which may take seconds). HB
	refresh();
	return (void*)context;
}

//virtual
void LLWindowSDL::makeContextCurrent(void* context)
{
	if (context)
	{
		SDL_GL_MakeCurrent(mWindow,
						   ((LLSharedOpenGLContext*)context)->mContext);
	}
	else
	{
		// Restore main GL thread context.
		SDL_GL_MakeCurrent(mWindow, mContext);
	}
}

//virtual
void LLWindowSDL::destroySharedContext(void* context)
{
	if (context)	// Ignore attempts to destroy invalid contexts. HB
	{
		LLSharedOpenGLContext* sc = (LLSharedOpenGLContext*)context;
		SDL_GL_DeleteContext(sc->mContext);
		delete sc;
	}
}

//virtual
void LLWindowSDL::destroyContext()
{
	SDL_StopTextInput();

	mSDL_Display = NULL;
	mSDL_XWindowID = None;

	// Clean up remaining GL state before blowing away window
	llinfos << "Shutting down GL..." << llendl;
	gGLManager.shutdownGL();

	if (mContext)
	{
		llinfos << "Destroying context..." << llendl;
		SDL_GL_DeleteContext(mContext);
		mContext = NULL;
	}

	if (mWindow)
	{
		llinfos << "Destroying window..." << llendl;
		SDL_DestroyWindow(mWindow);
	}
	mWindow = NULL;

	llinfos << "Quitting SDL video sub-system..." << llendl;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// Destroys all OS-specific code associated with a window. Usually called from
// LLWindow::destroyWindow()
//virtual
void LLWindowSDL::close()
{
#if 0	// Is window is already closed ?
	if (!mWindow)
	{
		return;
	}
#endif

	// Make sure cursor is visible and we have not mangled the clipping state
	setMouseClipping(false);
	showCursor();

	destroyContext();
}

//virtual
void LLWindowSDL::minimize()
{
	if (mWindow)
	{
		SDL_MinimizeWindow(mWindow);
	}
}

//virtual
void LLWindowSDL::restore()
{
	if (mWindow)
	{
		SDL_RestoreWindow(mWindow);
	}
}

//virtual
bool LLWindowSDL::getVisible()
{
	return mWindow && (SDL_GetWindowFlags(mWindow) & SDL_WINDOW_SHOWN);
}

//virtual
bool LLWindowSDL::getMinimized()
{
	return mWindow && (SDL_GetWindowFlags(mWindow) & SDL_WINDOW_MINIMIZED);
}

//virtual
void LLWindowSDL::calculateBordersOffsets()
{
	if (!mWindow || mFullscreen)
	{
		return;
	}
	int x, y;
	SDL_GetWindowPosition(mWindow, &x, &y);
	mPosOffsetX = x - mInitialPosX;
	mPosOffsetY = y - mInitialPosY;
	// *TODO: make the max border size configurable ?  The 25 and 50 fixed
	// values should cover all themes, but who knows ?...
	if (mPosOffsetX < 0 || mPosOffsetY < 0 || mPosOffsetX > 25 ||
		mPosOffsetX > 50)
	{
		// This could happen if the window manager overrides the position of
		// the window or lets the user move it around on creation. In this
		// case, we must report a 0,0 position with getPosition() (since the
		// saved window position would not be used anyway, in such a window
		// manager configuration). HB
		llwarns << "Incoherent window borders offsets found: x = "
				<< mPosOffsetX << " - y = " << mPosOffsetY
				<< ". Did you move the window on creation ?  Window position will always been reported as 0,0."
				<< llendl;
		mPosOffsetX = mPosOffsetY = -1;	// Flag for "report 0,0"
	}
	else
	{
		llinfos << "Window borders offsets: x = " << mPosOffsetX << " - y = "
				<< mPosOffsetY << llendl;
	}
}

//virtual
bool LLWindowSDL::getPosition(LLCoordScreen* position)
{
	if (position && mWindow)
	{
		// Problem: the window coordinates returned by SDL_GetWindowPosition()
		// are offset by the size of the window borders (but only after the
		// window got fully decorated by the window manager, which happens
		// some time after it is created), while SDL_CreateWindow() needs the
		// absolute coordinates... Since getPosition() is used on viewer exit,
		// wrong coordinates would be stored in saved settings.
		// This is the reason why we need the mPosOffsetX and mPosOffsetY
		// offsets, computing them with calculateBordersOffsets() above just
		// after the window is decorated (i.e. before the user would have a
		// chance to move it manually). HB
		int x, y;
		SDL_GetWindowPosition(mWindow, &x, &y);
#if 0	// Saddly, this does not work, or at least not with all window managers
		// (I always seen it return top = left = 0)... HB
		int top, left;
		if (SDL_GetWindowBordersSize(mWindow, &top, &left, NULL, NULL) == 0)
		{
			x -= left;
			y -= top;
		}
#else
		// Report a 0,0 (like with SDL1) position if we do not know what the
		// borders offsets are or if in full screen mode.
		if (mFullscreen || mPosOffsetX < 0)
		{
			position->mX = position->mY = 0;
		}
		else
		{
			position->mX = x - mPosOffsetX;
			position->mY = y - mPosOffsetY;
		}
#endif
		return true;
	}

	return false;
}

//virtual
bool LLWindowSDL::getSize(LLCoordScreen* size)
{
	if (mWindow && size)
	{
		SDL_GetWindowSize(mWindow, &size->mX, &size->mY);
		return true;
	}
	return false;
}

//virtual
bool LLWindowSDL::getSize(LLCoordWindow* size)
{
	if (mWindow && size)
	{
		SDL_GetWindowSize(mWindow, &size->mX, &size->mY);
		return true;
	}
	return false;
}

//virtual
bool LLWindowSDL::setSize(const LLCoordScreen size)
{
	if (!mWindow)
	{
		return false;
	}

	if (SDL_GetWindowFlags(mWindow) & SDL_WINDOW_MAXIMIZED)
	{
		SDL_RestoreWindow(mWindow);
	}

	SDL_SetWindowSize(mWindow, size.mX, size.mY);
	SDL_Event event;
	event.type = SDL_WINDOWEVENT;
	event.window.event = SDL_WINDOWEVENT_RESIZED;
	event.window.windowID = SDL_GetWindowID(mWindow);
	event.window.data1 = size.mX;
	event.window.data2 = size.mY;
	SDL_PushEvent(&event);

	return true;
}

// *HACK: this method causes a proper screen refresh by triggering a full
// redraw event at the SDL level. HB
//virtual
void LLWindowSDL::refresh()
{
	LLCoordScreen size;
	if (getSize(&size))
	{
		setSize(size);
	}
}

//virtual
void LLWindowSDL::swapBuffers()
{
	if (mWindow)
	{
		LL_FAST_TIMER(FTM_SWAP);
		SDL_GL_SwapWindow(mWindow);
	}
}

//virtual
bool LLWindowSDL::restoreGamma()
{
	if (!mCustomGammaSet)
	{
		return true;
	}
	mCustomGammaSet = false;
	return mWindow && SDL_SetWindowGammaRamp(mWindow, mPrevGammaRamp[0],
											 mPrevGammaRamp[1],
											 mPrevGammaRamp[2]) == 0;
}

//virtual
bool LLWindowSDL::setGamma(F32 gamma)
{
	LL_DEBUGS("Window") << "Setting gamma to " << gamma << LL_ENDL;
	mCurrentGamma = llclamp(gamma, 0.01f, 10.f);
	if (!mWindow)
	{
		return false;
	}
	// Get the previous gamma ramp to restore later.
	if (!mCustomGammaSet)
	{
		if (SDL_GetWindowGammaRamp(mWindow, mPrevGammaRamp[0],
								   mPrevGammaRamp[1], mPrevGammaRamp[2]) != 0)
		{
			llwarns << "Failed to get the previous gamma ramp." << llendl;
			// Use a gamma ramp with default gamma = 1.f
			SDL_CalculateGammaRamp(1.f, mPrevGammaRamp[0]);
			SDL_CalculateGammaRamp(1.f, mPrevGammaRamp[1]);
			SDL_CalculateGammaRamp(1.f, mPrevGammaRamp[2]);
		}
		mCustomGammaSet = true;
	}
	SDL_CalculateGammaRamp(mCurrentGamma, mCurrentGammaRamp);
	return SDL_SetWindowGammaRamp(mWindow, mCurrentGammaRamp,
								  mCurrentGammaRamp, mCurrentGammaRamp) == 0;
}

// Constrains the mouse to the window.
//virtual
void LLWindowSDL::setMouseClipping(bool b)
{
#if 0
	SDL_WM_GrabInput(b ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
}

//virtual
bool LLWindowSDL::setCursorPosition(const LLCoordWindow& position)
{
	LLCoordScreen screen_pos;
	if (!convertCoords(position, &screen_pos))
	{
		return false;
	}

	// Do the actual forced cursor move.
	if (mWindow)
	{
		SDL_WarpMouseInWindow(mWindow, screen_pos.mX, screen_pos.mY);
	}
	return true;
}

//virtual
bool LLWindowSDL::getCursorPosition(LLCoordWindow* position)
{
	// Point cursor_point
	LLCoordScreen screen_pos;

	int x, y;
	SDL_GetMouseState(&x, &y);

	screen_pos.mX = x;
	screen_pos.mY = y;

	return convertCoords(screen_pos, position);
}

//virtual
F32 LLWindowSDL::getNativeAspectRatio()
{
#if 0
	// RN: this hack presumes that the largest supported resolution is
	// monitor-limited and that pixels in that mode are square, therefore
	// defining the native aspect ratio of the monitor... this seems to work to
	// a close approximation for most CRTs/LCDs
	S32 num_resolutions;
	LLWindowResolution* resolutions = getSupportedResolutions(num_resolutions);

	return (F32)resolutions[num_resolutions - 1].mWidth /
		   (F32)resolutions[num_resolutions - 1].mHeight;
#endif

	// MBW: there are a couple of bad assumptions here. One is that the display
	// list would not include ridiculous resolutions nobody would ever use. The
	// other is that the list is in order.

	// New assumptions:
	// - pixels are square (the only reasonable choice, really)
	// - The user runs their display at a native resolution, so the resolution
	//   of the display when the app is launched has an aspect ratio that
	//   matches the monitor.

	// RN: actually, the assumption that there are no ridiculous resolutions
	// (above the display's native capabilities) has been born out in my
	// experience.
	// Pixels are often not square (just ask the people who run their LCDs at
	// 1024x768 or 800x600 when running fullscreen, like me).
	// The ordering of display list is a blind assumption though, so we should
	// check for max values.
	// Things might be different on the Mac though, so I'll defer to MBW

	// The constructor for this class grabs the aspect ratio of the monitor
	// before doing any resolution switching, and stashes it in
	// mOriginalAspectRatio. Here, we just return it.
	return mOverrideAspectRatio > 0.f ? mOverrideAspectRatio
									  : mOriginalAspectRatio;
}

//virtual
F32 LLWindowSDL::getPixelAspectRatio()
{
	F32 pixel_aspect = 1.f;
	if (getFullscreen())
	{
		LLCoordScreen screen_size;
		if (getSize(&screen_size))
		{
			pixel_aspect = getNativeAspectRatio() * (F32)screen_size.mY /
						   (F32)screen_size.mX;
		}
	}
	return pixel_aspect;
}

// This is to support 'temporarily windowed' mode so that dialogs are still
// usable in fullscreen.
//virtual
void LLWindowSDL::beforeDialog()
{
	llinfos << "called" << llendl;

	if (SDLReallyCaptureInput(false)) // Must un-grab input so popup works !
	{
		if (mFullscreen)
		{
			// Need to temporarily go non-fullscreen; bless SDL for providing a
			// SDL_WM_ToggleFullScreen() - though it only works in X11
			if (mSDL_XWindowID != None && mWindow)
			{
				SDL_SetWindowFullscreen(mWindow, 0);
			}
		}
	}

	if (mSDL_Display)
	{
		// Everything that we/SDL asked for should happen before we potentially
		// hand control over to GTK.
		XSync(mSDL_Display, False);
	}
}

//virtual
void LLWindowSDL::afterDialog()
{
	llinfos << "called." << llendl;

	if (mFullscreen)
	{
		// Need to restore fullscreen mode after dialog; only works in X11
		if (mSDL_XWindowID != None && mWindow)
		{
			SDL_SetWindowFullscreen(mWindow, 0);
		}
	}
}

// Sets/reset the XWMHints flag for 'urgency' that usually makes the icon flash
void LLWindowSDL::x11_set_urgent(bool urgent)
{
	if (mSDL_Display && !mFullscreen)
	{
		LL_DEBUGS("Window") << "X11 hint for urgency, " << urgent << LL_ENDL;

		XWMHints* wm_hints = XGetWMHints(mSDL_Display, mSDL_XWindowID);
		if (!wm_hints)
		{
			wm_hints = XAllocWMHints();
		}

		if (urgent)
		{
			wm_hints->flags |= XUrgencyHint;
		}
		else
		{
			wm_hints->flags &= ~XUrgencyHint;
		}

		XSetWMHints(mSDL_Display, mSDL_XWindowID, wm_hints);
		XFree(wm_hints);
		XSync(mSDL_Display, False);
	}
}

//virtual
void LLWindowSDL::flashIcon(F32 seconds)
{
	LL_DEBUGS("Window") << "Flashing icon for " << seconds << " seconds"
						<< LL_ENDL;

	F32 remaining_time = mFlashTimer.getRemainingTimeF32();
	if (remaining_time < seconds)
	{
		remaining_time = seconds;
	}
	mFlashTimer.reset();
	mFlashTimer.setTimerExpirySec(remaining_time);

	x11_set_urgent(true);
	mFlashing = true;
}

///////////////////////////////////////////////////////////////////////////////
// Clipboards (primary and secondary) implementation (c)2015 Henri Beauchamp
///////////////////////////////////////////////////////////////////////////////

static Atom XA_CLIPBOARD;
static Atom XA_COMPOUND_TEXT;
static Atom XA_UTF8_STRING;
static Atom XA_TARGETS;
static Atom PVT_PASTE_BUFFER;

// Filters through SDL_Events searching for clipboard requests from the X
// server. evt is the event to filter.
static int x11_clipboard_filter(void*, SDL_Event* evt)
{
	Display* display = LLWindowSDL::getSDLDisplay();
	if (!display) return 1;

	// We are only interested in window manager events
	if (evt->type == SDL_SYSWMEVENT)
	{
		XEvent xevent = evt->syswm.msg->msg.x11.event;

		// See if the event is a selection/clipboard request
		if (xevent.type == SelectionRequest)
		{
			// Get the request in question
			XSelectionRequestEvent* request = &xevent.xselectionrequest;
			if (!request) return 1;	// Paranoia

			LL_DEBUGS("Window") << "Caught event type SelectionRequest. Request target: "
								<< request->target << " - Selection type: "
								<< request->selection << LL_ENDL;

			// Generate a reply to the selection request
			XSelectionEvent reply;
			reply.type = SelectionNotify;
			reply.serial = xevent.xany.serial;
			reply.send_event = xevent.xany.send_event;
			reply.display = display;
			reply.requestor = request->requestor;
			reply.selection = request->selection;
			reply.target = request->target;
			reply.property = request->property;
			reply.time = request->time;

			// They want to know what we can provide/offer
			if (request->target == XA_TARGETS)
			{
				LL_DEBUGS("Window") << "Request is XA_TARGETS" << LL_ENDL;
				Atom possibleTargets[] =
				{
					XA_STRING,
					XA_UTF8_STRING,
					XA_COMPOUND_TEXT
				};

				XChangeProperty(display, request->requestor, request->property,
								XA_ATOM, 32, PropModeReplace,
				                (unsigned char*)possibleTargets, 3);
			}
			// They want a string (all we can provide)
			else if (request->target == XA_STRING ||
					 request->target == XA_UTF8_STRING ||
					 request->target == XA_COMPOUND_TEXT)
			{
				std::string utf8;
				if (request->selection == XA_PRIMARY)
				{
					LL_DEBUGS("Window") << "Primary selection request"
										<< LL_ENDL;
					utf8 =
						wstring_to_utf8str(sWindowImplementation->getPrimaryText());
				}
				else
				{
					LL_DEBUGS("Window") << "Clipboard request" << LL_ENDL;
					utf8 =
						wstring_to_utf8str(sWindowImplementation->getSecondaryText());
				}

				XChangeProperty(display, request->requestor, request->property,
								request->target, 8, PropModeReplace,
								(unsigned char*)utf8.c_str(), utf8.length());
			}
			else if (request->selection == XA_CLIPBOARD)
			{
				LL_DEBUGS("Window") << "Unhandled request" << LL_ENDL;
				// Did not have what they wanted, so no property set
				reply.property = None;
			}
			else
			{
				LL_DEBUGS("Window") << "Unknown selection request. Ignoring."
									<< LL_ENDL;
				return 1;
			}

			// Dispatch the event
			XSendEvent(request->display, request->requestor, False,
					   NoEventMask, (XEvent*)&reply);
			XSync(display, False);
		}
		else if (xevent.type == SelectionClear)
		{
			// Get the request in question
			XSelectionRequestEvent* request = &xevent.xselectionrequest;
			if (!request) return 1;	// Paranoia

			// We no longer own the clipboard: clear our stored data.
			if (request->selection == XA_PRIMARY)
			{
				LL_DEBUGS("Window") << "Someone else took the ownership of the primary selection; clearing our primary selection buffer."
									<< LL_ENDL;
				sWindowImplementation->clearPrimaryText();
			}
			else if (request->selection == XA_CLIPBOARD)
			{
				LL_DEBUGS("Window") << "Someone else took the ownership of the clipboard; clearing our clipboard buffer."
									<< LL_ENDL;
				sWindowImplementation->clearSecondaryText();
			}
		}
	}
	return 1;
}

void LLWindowSDL::initialiseX11Clipboard()
{
	if (mSDL_Display)
	{
		LL_DEBUGS("Window") << "Initializing the X11 clipboard" << LL_ENDL;

		// Register the event filter
		SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
		SDL_SetEventFilter(x11_clipboard_filter, NULL);

		// Get the clipboard atom (it is not defined by default)
		XA_CLIPBOARD = XInternAtom(mSDL_Display, "CLIPBOARD", False);

		// Get the compound text type atom
		XA_COMPOUND_TEXT = XInternAtom(mSDL_Display, "COMPOUND_TEXT", False);

		// UTF-8 string atom
		XA_UTF8_STRING = XInternAtom(mSDL_Display, "UTF8_STRING", False);

		// TARGETS atom
		XA_TARGETS = XInternAtom(mSDL_Display, "TARGETS", False);

		// SL_PASTE_BUFFER atom
		PVT_PASTE_BUFFER = XInternAtom(mSDL_Display, "SL_PASTE_BUFFER", False);
	}
}

static bool grab_property(Display* display, Window window, Atom selection,
						 Atom target)
{
	XDeleteProperty(display, window, PVT_PASTE_BUFFER);
	XFlush(display);

	XConvertSelection(display, selection, target, PVT_PASTE_BUFFER, window,
					  CurrentTime);

	// We now need to wait for a response from the window that owns the
	// clipboard.
	LL_DEBUGS("Window") << "Waiting for the selection owner to provide its text..."
						<< LL_ENDL;
	SDL_Event event;
	XEvent xevent;
	Uint32 start = SDL_GetTicks();
	constexpr Uint32 maxticks = 1000; // 1 second
	bool response = false;
	while (!response && SDL_GetTicks() - start < maxticks)
	{
		// Wait for an event
		SDL_WaitEvent(&event);

		// If the event is a window manager event
		if (event.type == SDL_SYSWMEVENT)
		{
			xevent = event.syswm.msg->msg.x11.event;
			// See if it is a response to our request
			if (xevent.type == SelectionNotify &&
				xevent.xselection.requestor == window)
			{
				response = true;
			}
		}
	}

	bool ret = response && xevent.xselection.property != None;
	LL_DEBUGS("Window") << "... " << (ret ? "Succeeded" : "Failed") << " !"
						<< LL_ENDL;
	return ret;
}

bool LLWindowSDL::getSelectionText(Atom selection, LLWString& text)
{
	if (!mSDL_Display) return false;

	// Get the owner of the clipboard selection.
	Window owner = XGetSelectionOwner(mSDL_Display, selection);
	if (owner == None)
	{
		// Only the primary selection may be owned by None, in the cut buffer
		// (legacy, xterm way of dealing with selections). Else, it means the
		// selection is empty...
		if (selection != XA_PRIMARY)
		{
			// The clipboard is empty...
			text.clear();
			return false;
		}
		LL_DEBUGS("Window") << "No owner for current selection. Using default root window and XA_CUT_BUFFER0"
							<< LL_ENDL;
		owner = DefaultRootWindow(mSDL_Display);
		selection = XA_CUT_BUFFER0;
	}

	// Ask the window that currently owns the clipboard to convert it
	LL_DEBUGS("Window") << "Requesting conversion to XA_UTF8_STRING"
						<< LL_ENDL;
	if (!grab_property(mSDL_Display, mSDL_XWindowID, selection,
					   XA_UTF8_STRING))
	{
#if 0	// We do not support ISO 2022 encoding in the viewer, so...
		LL_DEBUGS("Window") << "Requesting conversion to XA_COMPOUND_TEXT"
							<< LL_ENDL;
		if (!grab_property(mSDL_Display, mSDL_XWindowID, selection,
						   XA_COMPOUND_TEXT))
#endif
		{
			LL_DEBUGS("Window") << "Requesting conversion to XA_STRING"
								<< LL_ENDL;
			if (!grab_property(mSDL_Display, mSDL_XWindowID, selection,
							   XA_STRING))
			{
				// The clipboard does not contains any valid text.
				text.clear();
				return false;
			}
		}
	}

	// Recover any paste buffer text (using S32_MAX for max length).
	Atom type;
	int format = 0;
	unsigned long len = 0;
	unsigned long remaining = 0;
	unsigned char* data = NULL;
	int res = XGetWindowProperty(mSDL_Display, mSDL_XWindowID,
								 PVT_PASTE_BUFFER, 0, S32_MAX, False,
								 AnyPropertyType, &type, &format, &len,
								 &remaining, &data);
	if (data && len)
	{
		if (format == 8)
		{
			std::string tmp;
			tmp.assign((const char*)data);
			text = LLWString(utf8str_to_wstring(tmp));
		}
		else
		{
			// This should not happen since we asked above for 8 bits encoding
			// conversions only...
			llwarns << "Unsupported clipboard text format type: "
					<< format << " bits characters instead of 8." << llendl;
			len = 0;
		}
	}
	if (!len)
	{
		text.clear();
	}
	// Note: XGetWindowProperty() is documented as always allocating at least
	// one "extra byte", even if the property is zero-length, so we need to
	// free any allocated data, even for len == 0.
	if (data)
	{
		XFree(data);
	}

	return res == Success;
}

bool LLWindowSDL::setSelectionText(Atom selection, const LLWString& text)
{
	std::string utf8 = wstring_to_utf8str(text);

	if (selection == XA_PRIMARY)
	{
		// Copy the text into the root window's cut buffer
		XStoreBytes(mSDL_Display, utf8.c_str(), utf8.length() + 1);
		mPrimaryClipboard = text;
		LL_DEBUGS("Window") << "Setting the primary selection text" << LL_ENDL;
	}
	else
	{
		mSecondaryClipboard = text;
		LL_DEBUGS("Window") << "Setting the clipboard text" << LL_ENDL;
	}

	// Set ourself as the owner of the selection atom
	XSetSelectionOwner(mSDL_Display, selection, mSDL_XWindowID, CurrentTime);

	// Check if we acquired ownership or not
	Window owner = XGetSelectionOwner(mSDL_Display, selection);

	return owner == mSDL_XWindowID;
}

//virtual
bool LLWindowSDL::isClipboardTextAvailable()
{
	return mSDL_Display &&
		   XGetSelectionOwner(mSDL_Display, XA_CLIPBOARD) != None;
}

//virtual
bool LLWindowSDL::pasteTextFromClipboard(LLWString& text)
{
	return getSelectionText(XA_CLIPBOARD, text);
}

//virtual
bool LLWindowSDL::copyTextToClipboard(const LLWString& text)
{
	return setSelectionText(XA_CLIPBOARD, text);
}

//virtual
bool LLWindowSDL::isPrimaryTextAvailable()
{
	if (mSDL_Display)
	{
		LLWString text;
		return getSelectionText(XA_PRIMARY, text);
	}
	return false; // failure
}

//virtual
bool LLWindowSDL::pasteTextFromPrimary(LLWString& text)
{
	return getSelectionText(XA_PRIMARY, text);
}

//virtual
bool LLWindowSDL::copyTextToPrimary(const LLWString& text)
{
	return setSelectionText(XA_PRIMARY, text);
}

///////////////////////////////////////////////////////////////////////////////

//virtual
LLWindow::LLWindowResolution* LLWindowSDL::getSupportedResolutions(S32& num_resolutions)
{
	if (!mSupportedResolutions)
	{
		mSupportedResolutions = new LLWindowResolution[MAX_NUM_RESOLUTIONS];
		mNumSupportedResolutions = 0;

		// *TODO: get count from display corresponding to mWindow
		S32 count = llclamp(0, (S32)SDL_GetNumDisplayModes(0),
							MAX_NUM_RESOLUTIONS);
		for (S32 i = 0; i < count; ++i)
		{
			SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
			if (SDL_GetDisplayMode(0 , i, &mode) != 0)
			{
				continue;
			}
			S32 w = mode.w;
			S32 h = mode.h;
			if (w >= 800 && h >= 600)
			{
				// Make sure we do not add the same resolution multiple times !
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

//virtual
bool LLWindowSDL::convertCoords(LLCoordGL from, LLCoordWindow* to)
{
	if (to && mWindow)
	{
		to->mX = from.mX;
		S32 height;
		SDL_GetWindowSize(mWindow, NULL, &height);
		to->mY = height - from.mY - 1;
		return true;
	}
	return false;
}

//virtual
bool LLWindowSDL::convertCoords(LLCoordWindow from, LLCoordGL* to)
{
	if (to && mWindow)
	{
		to->mX = from.mX;
		S32 height;
		SDL_GetWindowSize(mWindow, NULL, &height);
		to->mY = height - from.mY - 1;
		return true;
	}
	return false;
}

//virtual
bool LLWindowSDL::convertCoords(LLCoordScreen from, LLCoordWindow* to)
{
	if (to)
	{
		// In the fullscreen case, window and screen coordinates are the same.
		to->mX = from.mX;
		to->mY = from.mY;
		return true;
	}
	return false;
}

//virtual
bool LLWindowSDL::convertCoords(LLCoordWindow from, LLCoordScreen* to)
{
	if (to)
	{
		// In the fullscreen case, window and screen coordinates are the same.
		to->mX = from.mX;
		to->mY = from.mY;
		return true;
	}
	return false;
}

//virtual
bool LLWindowSDL::convertCoords(LLCoordScreen from, LLCoordGL* to)
{
	LLCoordWindow wcoord;
	return convertCoords(from, &wcoord) && convertCoords(wcoord, to);
}

//virtual
bool LLWindowSDL::convertCoords(LLCoordGL from, LLCoordScreen* to)
{
	LLCoordWindow wcoord;
	return convertCoords(from, &wcoord) && convertCoords(wcoord, to);
}

void LLWindowSDL::setupFailure(const std::string& text)
{
	destroyContext();
	OSMessageBox(text);
}

bool LLWindowSDL::SDLReallyCaptureInput(bool capture)
{
	mCaptured = capture;

	bool newgrab = capture;

	// Only bother if we are windowed
	if (!mFullscreen && mSDL_Display)
	{
		// We dirtily mix raw X11 with SDL so that our pointer is not (as
		// often) constrained to the limits of the window while grabbed, which
		// feels nicer and hopefully eliminates some reported 'sticky pointer'
		// problems. We use raw X11 instead of SDL_WM_GrabInput() because the
		// latter constrains the pointer to the window and also steals all
		// *keyboard* input from the window manager, which was frustrating
		// users.
		if (capture)
		{
			newgrab = XGrabPointer(mSDL_Display, mSDL_XWindowID, True, 0,
								   GrabModeAsync, GrabModeAsync, None, None,
								   CurrentTime) == GrabSuccess;
		}
		else
		{
			XUngrabPointer(mSDL_Display, CurrentTime);
			// Make sure the ungrab happens RIGHT NOW.
			XSync(mSDL_Display, False);
			newgrab = false;
		}
	}

	// Return boolean success for whether we ended up in the desired state
	return capture == newgrab;
}

U32 LLWindowSDL::SDLCheckGrabbyKeys(U32 keysym, bool gain)
{
	// Part of the fix for SL-13243: Some popular window managers like to
	// totally eat alt-drag for the purposes of moving windows. We spoil their
	// day by acquiring the exclusive X11 mouse lock for as long as ALT is held
	// down, so the window manager cannot easily see what is happening. Tested
	// successfully with Metacity. And... do the same with CTRL, for other darn
	// WMs. We do not care about other metakeys as SL does not use them with
	// dragging (for now).

	// We maintain a bitmap of critical keys which are up and down instead of
	// simply key-counting, because SDL sometimes reports misbalanced
	// keyup/keydown event pairs to us for whatever reason.

	U32 mask = 0;
	switch (keysym)
	{
		case SDLK_LALT:
			mask = 1U << 0;
			break;

		case SDLK_RALT:
			mask = 1U << 1;
			break;

		case SDLK_LCTRL:
			mask = 1U << 2;
			break;

		case SDLK_RCTRL:
			mask = 1U << 3;
			break;

		default:
			break;
	}

	if (gain)
	{
		mGrabbyKeyFlags |= mask;
	}
	else
	{
		mGrabbyKeyFlags &= ~mask;
	}

	// 0 means we do not need to mousegrab, otherwise grab.
	return mGrabbyKeyFlags;
}

//virtual
void LLWindowSDL::gatherInput()
{
	constexpr Uint32 CLICK_THRESHOLD = 300;  // milliseconds
	static S32 left_click = 0;
	static S32 right_click = 0;
	static Uint32 last_left_down = 0;
	static Uint32 last_right_down = 0;
	SDL_Event event;

	// Handle all outstanding SDL events
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_MOUSEWHEEL:
			{
				if (event.wheel.y != 0)
				{
					mCallbacks->handleScrollWheel(this, -event.wheel.y);
				}
				break;
			}

			case SDL_MOUSEMOTION:
			{
				LLCoordWindow win_coord(event.button.x, event.button.y);
				LLCoordGL open_gl_coord;
				convertCoords(win_coord, &open_gl_coord);
				MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
				mCallbacks->handleMouseMove(this, open_gl_coord, mask);
				break;
			}

			case SDL_TEXTINPUT:
			{
				auto str = utf8str_to_utf16str(event.text.text);
				mKeyVirtualKey = str[0];
				mKeyModifiers = SDL_GetModState();
				MASK mask = gKeyboardp ? gKeyboardp->currentMask(false)
									   : 0;
				for (const auto& key : str)
				{
					handleUnicodeUTF16(key, mask);
				}
				break;
			}

			case SDL_KEYDOWN:
			{
		    	mKeyVirtualKey = event.key.keysym.sym;
		    	mKeyModifiers = event.key.keysym.mod;
				if (mKeyVirtualKey == SDLK_KP_ENTER)
				{
					mKeyVirtualKey = SDLK_RETURN;	// For CEF to get it right
				}
				if (gKeyboardp)
				{
					gKeyboardp->handleKeyDown(mKeyVirtualKey, mKeyModifiers);
				}
				// Because, with SDL2, RETURN (and key pad ENTER, but it was
				// already turned into SDLK_RETURN above) is not part of the
				// text characters sent via the SDL_TEXTINPUT event, in excess
				// of handleKeyDown() we also must invoke handleUnicodeUTF16()
				// with the SDLK_RETURN virtual key, since this is where the
				// viewer code deals with RETURN/ENTER...
				if (mKeyVirtualKey == SDLK_RETURN)
				{
					MASK mask = gKeyboardp ? gKeyboardp->currentMask(false)
										   : 0;
					handleUnicodeUTF16(SDLK_RETURN, mask);
				}
				// Part of the fix for SL-13243
				if (SDLCheckGrabbyKeys(event.key.keysym.sym, true) != 0)
				{
					SDLReallyCaptureInput(true);
				}
				break;
			}

			case SDL_KEYUP:
			{
		    	mKeyVirtualKey = event.key.keysym.sym;
				if (mKeyVirtualKey == SDLK_KP_ENTER)
				{
					mKeyVirtualKey = SDLK_RETURN;	// For CEF to get it right
				}
		    	mKeyModifiers = event.key.keysym.mod;
				// Part of the fix for SL-13243
				if (SDLCheckGrabbyKeys(mKeyVirtualKey, false) == 0)
				{
					SDLReallyCaptureInput(false);
				}
				if (gKeyboardp)
				{
					gKeyboardp->handleKeyUp(mKeyVirtualKey, mKeyModifiers);
				}
				break;
			}

			case SDL_MOUSEBUTTONDOWN:
			{
				bool is_double_click = false;
				LLCoordWindow win_coord(event.button.x, event.button.y);
				LLCoordGL open_gl_coord;
				convertCoords(win_coord, &open_gl_coord);
				MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;

				if (event.button.button == SDL_BUTTON_LEFT)
				{
					// SDL does not manage double clicking...
					Uint32 now = SDL_GetTicks();
					if (now - last_left_down > CLICK_THRESHOLD)
					{
						left_click = 1;
					}
					else if (++left_click >= 2)
					{
						left_click = 0;
						is_double_click = true;
					}
					last_left_down = now;
				}
				else if (event.button.button == SDL_BUTTON_RIGHT)
				{
					Uint32 now = SDL_GetTicks();
					if (now - last_right_down > CLICK_THRESHOLD)
					{
						right_click = 1;
					}
					else if (++right_click >= 2)
					{
						right_click = 0;
						is_double_click = true;
					}
					last_right_down = now;
				}

				if (event.button.button == SDL_BUTTON_LEFT)  // left
				{
					if (is_double_click)
					{
						mCallbacks->handleDoubleClick(this, open_gl_coord,
													  mask);
					}
					else
					{
						mCallbacks->handleMouseDown(this, open_gl_coord, mask);
					}
				}
				else if (event.button.button == SDL_BUTTON_RIGHT)  // right
				{
					mCallbacks->handleRightMouseDown(this, open_gl_coord, mask);
				}
				else if (event.button.button == SDL_BUTTON_MIDDLE)  // middle
				{
					mCallbacks->handleMiddleMouseDown(this, open_gl_coord, mask);
				}
				else if (event.button.button == 4)
				{
					// Mousewheel up...thanks to X11 for making SDL consider
					// these "buttons".
					mCallbacks->handleScrollWheel(this, -1);
				}
				else if (event.button.button == 5)
				{
					// Mousewheel down...thanks to X11 for making SDL consider
					// these "buttons".
					mCallbacks->handleScrollWheel(this, 1);
				}

				break;
			}

			case SDL_MOUSEBUTTONUP:
			{
				LLCoordWindow win_coord(event.button.x, event.button.y);
				LLCoordGL open_gl_coord;
				convertCoords(win_coord, &open_gl_coord);
				MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;

				if (event.button.button == SDL_BUTTON_LEFT)
				{
					mCallbacks->handleMouseUp(this, open_gl_coord, mask);
				}
				else if (event.button.button == SDL_BUTTON_RIGHT)
				{
					mCallbacks->handleRightMouseUp(this, open_gl_coord, mask);
				}
				else if (event.button.button == SDL_BUTTON_MIDDLE)
				{
					mCallbacks->handleMiddleMouseUp(this, open_gl_coord, mask);
				}
				// Do not handle mousewheel here...

				break;
			}

			case SDL_WINDOWEVENT:
			{
				Uint8 ev = event.window.event;
				if (ev == SDL_WINDOWEVENT_FOCUS_GAINED)
				{
					mCallbacks->handleFocus(this);
				}
				else if (ev == SDL_WINDOWEVENT_FOCUS_LOST)
				{
					mCallbacks->handleFocusLost(this);
				}
				else if (ev == SDL_WINDOWEVENT_RESIZED)
				{
					if (!mWindow)
					{
						// *FIXME: More informative dialog ?
						llinfos << "Could not recreate context after resize !  Quitting..."
								<< llendl;
						if (mCallbacks->handleCloseRequest(this))
						{
							// Get the app to initiate cleanup.
							mCallbacks->handleQuit(this);
							// The app is responsible for calling destroyWindow
							// when done with GL
						}
						break;
					}
					S32 width = event.window.data1;
					S32 height = event.window.data2;
					LL_DEBUGS("Window") << "Handling a resize event: " << width
										<< "x" << height << LL_ENDL;
					if (gFrameSleepTime > 0)
					{
						// *HACK: clear the window to black to avoid a white
						// flickering when resizing the viewer window while not
						// logged in or when yieding each frame to the OS, due
						// to the volontary throttling of the frame rate with
						// ms_sleep(), and only in this case. Sadly, and unlike
						// SLD1, SDL2 clears the screen to white (instead of
						// black) on resize, so if we do not redraw immediately
						// after a resize event, we get a rather flashy and
						// totally ugly flickering effect. HB
						glClearColor(0.f, 0.f, 0.f, 1.f);
						glClear(GL_COLOR_BUFFER_BIT);
					}
					mCallbacks->handleResize(this, width, height);
				}
				else if (ev == SDL_WINDOWEVENT_RESTORED)
				{
					bool minimized = SDL_GetWindowFlags(mWindow) &
									 SDL_WINDOW_MINIMIZED;
					llinfos << "SDL minimized state switched to "
							<< !minimized << llendl;
					mCallbacks->handleActivate(this, !minimized);
				}
				else if (ev == SDL_WINDOWEVENT_EXPOSED)
				{
					// Repaint the whole window.
					S32 width = 0;
					S32 height = 0;
					SDL_GetWindowSize(mWindow, &width, &height);
					mCallbacks->handlePaint(this, 0, 0, width, height);
				}
				break;
			}

			case SDL_QUIT:
			{
				if (mCallbacks->handleCloseRequest(this))
				{
					// Get the app to initiate cleanup.
					mCallbacks->handleQuit(this);
					// The app is responsible for calling destroyWindow when
					// done with GL
				}
				break;
			}

			default:
			{
				LL_DEBUGS("Window") << "Unhandled SDL event type "
									<< event.type << LL_ENDL;
			}
		}
	}

	// This is a good time to stop flashing the icon if our mFlashTimer has
	// expired.
	if (mFlashing && mFlashTimer.hasExpired())
	{
		x11_set_urgent(false);
		mFlashing = false;
	}
}

//virtual
void LLWindowSDL::setCursor(ECursorType cursor)
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
		if (cursor < UI_CURSOR_COUNT)
		{
			SDL_Cursor* sdlcursor = mSDLCursors[cursor];
			// Try to default to the arrow for any cursors that did not load
			// correctly.
			if (!sdlcursor && mSDLCursors[UI_CURSOR_ARROW])
			{
				sdlcursor = mSDLCursors[UI_CURSOR_ARROW];
			}
			if (sdlcursor)
			{
				SDL_SetCursor(sdlcursor);
			}
		}
		else
		{
			llwarns << "Tried to set invalid cursor number " << cursor
					<< llendl;
		}
		mCurrentCursor = cursor;
	}
}

static SDL_Cursor* sdl_cursor_from_bmp(const char* fname, int hotx = 0,
									   int hoty = 0)
{
	SDL_Cursor* sdlcursor = NULL;

	// Load cursor pixel data from BMP file
	SDL_Surface* bmpsurface = load_bmp_resource(fname);
	if (bmpsurface && bmpsurface->w % 8 == 0)
	{
		LL_DEBUGS("Window") << "Loaded cursor file " << fname << " "
							<< bmpsurface->w << "x" << bmpsurface->h
							<< LL_ENDL;
		SDL_Surface* cursurface =
			SDL_CreateRGBSurface(SDL_SWSURFACE, bmpsurface->w, bmpsurface->h,
								 32, SDL_SwapLE32(0xFFU),
								 SDL_SwapLE32(0xFF00U),
								 SDL_SwapLE32(0xFF0000U),
								 SDL_SwapLE32(0xFF000000U));
		SDL_FillRect(cursurface, NULL, SDL_SwapLE32(0x00000000U));

		// Blit the cursor pixel data onto a 32 bits RGBA surface so we
		// only have to cope with processing one type of pixel format.
		if (SDL_BlitSurface(bmpsurface, NULL, cursurface, NULL) == 0)
		{
			// NB: we already checked that width is a multiple of 8.
			const int bitmap_bytes = cursurface->w * cursurface->h / 8;
			unsigned char* cursor_data = new unsigned char[bitmap_bytes];
			unsigned char* cursor_mask = new unsigned char[bitmap_bytes];
			memset(cursor_data, 0, bitmap_bytes);
			memset(cursor_mask, 0, bitmap_bytes);
			// Walk the RGBA cursor pixel data, extracting both data and
			// mask to build SDL-friendly cursor bitmaps from.  The mask
			// is inferred by color-keying against 200,200,200
			for (S32 i = 0; i < cursurface->h; ++i)
			{
				for (S32 j = 0; j < cursurface->w; ++j)
				{
					U8* pixelp = ((U8*)cursurface->pixels) +
								 cursurface->pitch * i +
								 j * cursurface->format->BytesPerPixel;
					U8 srcred = pixelp[0];
					U8 srcgreen = pixelp[1];
					U8 srcblue = pixelp[2];
					bool mask_bit = srcred != 200 || srcgreen != 200 ||
									srcblue != 200;
					bool data_bit = mask_bit && srcgreen <= 80;	// not 0x80
					unsigned char bit_offset = cursurface->w / 8 * i + j / 8;
					cursor_data[bit_offset]	|= ((U8)data_bit) << (7 - (j & 7));
					cursor_mask[bit_offset]	|= ((U8)mask_bit) << (7 - (j & 7));
				}
			}
			sdlcursor = SDL_CreateCursor((Uint8*)cursor_data,
										 (Uint8*)cursor_mask,
										 cursurface->w, cursurface->h,
										 hotx, hoty);
			delete[] cursor_data;
			delete[] cursor_mask;
		}
		else
		{
			llwarns << "Cursor blit failure for: " << fname << llendl;
		}
		SDL_FreeSurface(cursurface);
		SDL_FreeSurface(bmpsurface);
	}
	else
	{
		llwarns << "Cursor load failure for: " << fname << llendl;
	}

	return sdlcursor;
}

void LLWindowSDL::initCursors()
{
	// Blank the cursor pointer array for those we may miss.
	for (S32 i = 0; i < UI_CURSOR_COUNT; ++i)
	{
		mSDLCursors[i] = NULL;
	}

	// Pre-make an SDL cursor for each of the known cursor types. We hardcode
	// the hotspots - to avoid that we would have to write a .cur file loader.
	// NOTE: SDL does not load RLE-compressed BMP files.

	if (getenv("LL_USE_SYSTEM_CURSORS"))
	{
		// Use the user's theme cursors where possible.
		mSDLCursors[UI_CURSOR_ARROW] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
		mSDLCursors[UI_CURSOR_WAIT] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
		mSDLCursors[UI_CURSOR_HAND] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
		mSDLCursors[UI_CURSOR_IBEAM] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
		mSDLCursors[UI_CURSOR_CROSS] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
		mSDLCursors[UI_CURSOR_SIZENWSE] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
		mSDLCursors[UI_CURSOR_SIZENESW] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
		mSDLCursors[UI_CURSOR_SIZEWE] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
		mSDLCursors[UI_CURSOR_SIZENS] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
		mSDLCursors[UI_CURSOR_NO] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
		mSDLCursors[UI_CURSOR_WORKING] =
			SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAITARROW);
	}
	else	// Use our custom cursors instead.
	{
		mSDLCursors[UI_CURSOR_ARROW] = sdl_cursor_from_bmp("llarrow.bmp");
		mSDLCursors[UI_CURSOR_WAIT] = sdl_cursor_from_bmp("wait.bmp", 12, 15);
		mSDLCursors[UI_CURSOR_HAND] = sdl_cursor_from_bmp("hand.bmp", 7, 10);
		mSDLCursors[UI_CURSOR_IBEAM] = sdl_cursor_from_bmp("ibeam.bmp",
														   15, 16);
		mSDLCursors[UI_CURSOR_CROSS] = sdl_cursor_from_bmp("cross.bmp",
														   16, 14);
		mSDLCursors[UI_CURSOR_SIZENWSE] = sdl_cursor_from_bmp("sizenwse.bmp",
														  14, 17);
		mSDLCursors[UI_CURSOR_SIZENESW] = sdl_cursor_from_bmp("sizenesw.bmp",
															  17, 17);
		mSDLCursors[UI_CURSOR_SIZEWE] = sdl_cursor_from_bmp("sizewe.bmp",
															16, 14);
		mSDLCursors[UI_CURSOR_SIZENS] = sdl_cursor_from_bmp("sizens.bmp",
															17, 16);
		mSDLCursors[UI_CURSOR_NO] = sdl_cursor_from_bmp("llno.bmp", 8, 8);
		mSDLCursors[UI_CURSOR_WORKING] = sdl_cursor_from_bmp("working.bmp",
															 12, 15);
	}
	mSDLCursors[UI_CURSOR_TOOLGRAB] = sdl_cursor_from_bmp("lltoolgrab.bmp",
														  2, 13);
	mSDLCursors[UI_CURSOR_TOOLLAND] = sdl_cursor_from_bmp("lltoolland.bmp",
														  1, 6);
	mSDLCursors[UI_CURSOR_TOOLFOCUS] = sdl_cursor_from_bmp("lltoolfocus.bmp",
														   8, 5);
	mSDLCursors[UI_CURSOR_TOOLCREATE] = sdl_cursor_from_bmp("lltoolcreate.bmp",
															7, 7);
	mSDLCursors[UI_CURSOR_ARROWDRAG] = sdl_cursor_from_bmp("arrowdrag.bmp");
	mSDLCursors[UI_CURSOR_ARROWCOPY] = sdl_cursor_from_bmp("arrowcop.bmp");
	mSDLCursors[UI_CURSOR_ARROWDRAGMULTI] =
		sdl_cursor_from_bmp("llarrowdragmulti.bmp");
	mSDLCursors[UI_CURSOR_ARROWCOPYMULTI] =
		sdl_cursor_from_bmp("arrowcopmulti.bmp", 0, 0);
	mSDLCursors[UI_CURSOR_NOLOCKED] = sdl_cursor_from_bmp("llnolocked.bmp",
														  8, 8);
	mSDLCursors[UI_CURSOR_ARROWLOCKED] =
		sdl_cursor_from_bmp("llarrowlocked.bmp");
	mSDLCursors[UI_CURSOR_GRABLOCKED] = sdl_cursor_from_bmp("llgrablocked.bmp",
															2, 13);
	mSDLCursors[UI_CURSOR_TOOLTRANSLATE] =
		sdl_cursor_from_bmp("lltooltranslate.bmp");
	mSDLCursors[UI_CURSOR_TOOLROTATE] =
		sdl_cursor_from_bmp("lltoolrotate.bmp");
	mSDLCursors[UI_CURSOR_TOOLSCALE] = sdl_cursor_from_bmp("lltoolscale.bmp");
	mSDLCursors[UI_CURSOR_TOOLCAMERA] = sdl_cursor_from_bmp("lltoolcamera.bmp",
															7, 5);
	mSDLCursors[UI_CURSOR_TOOLPAN] = sdl_cursor_from_bmp("lltoolpan.bmp",
														 7, 5);
	mSDLCursors[UI_CURSOR_TOOLZOOMIN] = sdl_cursor_from_bmp("lltoolzoomin.bmp",
															7, 5);
	mSDLCursors[UI_CURSOR_TOOLPICKOBJECT3] =
		sdl_cursor_from_bmp("toolpickobject3.bmp");
	mSDLCursors[UI_CURSOR_TOOLSIT] = sdl_cursor_from_bmp("toolsit.bmp");
	mSDLCursors[UI_CURSOR_TOOLBUY] = sdl_cursor_from_bmp("toolbuy.bmp");
	mSDLCursors[UI_CURSOR_TOOLPAY] = sdl_cursor_from_bmp("toolpay.bmp");
	mSDLCursors[UI_CURSOR_TOOLOPEN] = sdl_cursor_from_bmp("toolopen.bmp");
	mSDLCursors[UI_CURSOR_TOOLPLAY] = sdl_cursor_from_bmp("toolplay.bmp");
	mSDLCursors[UI_CURSOR_TOOLPAUSE] = sdl_cursor_from_bmp("toolpause.bmp");
	mSDLCursors[UI_CURSOR_TOOLMEDIAOPEN] =
		sdl_cursor_from_bmp("toolmediaopen.bmp");
	mSDLCursors[UI_CURSOR_PIPETTE] = sdl_cursor_from_bmp("lltoolpipette.bmp",
														 2, 28);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING] =
		sdl_cursor_from_bmp("lltoolpathfinding.bmp", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING_PATH_START] =
		sdl_cursor_from_bmp("lltoolpathfindingpathstart.bmp", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING_PATH_START_ADD] =
		sdl_cursor_from_bmp("lltoolpathfindingpathstartadd.bmp", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING_PATH_END] =
		sdl_cursor_from_bmp("lltoolpathfindingpathend.bmp", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING_PATH_END_ADD] =
		sdl_cursor_from_bmp("lltoolpathfindingpathendadd.bmp", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLNO] = sdl_cursor_from_bmp("llno.bmp", 8, 8);
}

void LLWindowSDL::quitCursors()
{
	if (mWindow)
	{
		for (S32 i = 0; i < UI_CURSOR_COUNT; ++i)
		{
			if (mSDLCursors[i])
			{
				SDL_FreeCursor(mSDLCursors[i]);
				mSDLCursors[i] = NULL;
			}
		}
	}
	else
	{
		// SDL does not refcount cursors, so if the window has already been
		// destroyed then the cursors have gone with it.
		llinfos << "Skipping quitCursors: mWindow already gone." << llendl;
		for (S32 i = 0; i < UI_CURSOR_COUNT; ++i)
		{
			mSDLCursors[i] = NULL;
		}
	}
}

//virtual
void LLWindowSDL::captureMouse()
{
	// SDL already enforces the semantics that captureMouse is used for, i.e.
	// that we continue to get mouse events as long as a button is down
	// regardless of whether we left the window, and in a less obnoxious way
	// than SDL_WM_GrabInput which would confine the cursor to the window too.
	LL_DEBUGS("Window") << "called" << LL_ENDL;
}

//virtual
void LLWindowSDL::releaseMouse()
{
	// See comment in LWindowSDL::captureMouse()
	LL_DEBUGS("Window") << "called" << LL_ENDL;
}

//virtual
void LLWindowSDL::hideCursor()
{
	if (!mCursorHidden)
	{
		mCursorHidden = true;
		mHideCursorPermanent = true;
		SDL_ShowCursor(0);
	}
}

//virtual
void LLWindowSDL::showCursor()
{
	if (mCursorHidden)
	{
		mCursorHidden = false;
		mHideCursorPermanent = false;
		SDL_ShowCursor(1);
	}
}

//virtual
void LLWindowSDL::showCursorFromMouseMove()
{
	if (!mHideCursorPermanent)
	{
		showCursor();
	}
}

//virtual
void LLWindowSDL::hideCursorUntilMouseMove()
{
	if (!mHideCursorPermanent)
	{
		hideCursor();
		mHideCursorPermanent = false;
	}
}

// Make the raw keyboard data available - used to poke through to CEF so that
// the embedded browser has access to the virtual keycodes etc that it needs.
//virtual
LLSD LLWindowSDL::getNativeKeyData()
{
	LLSD result = LLSD::emptyMap();

	result["virtual_key"] = (S32)mKeyVirtualKey;
	// Genuine native SDL modifiers.
	result["sdl_modifiers"] = (S32)mKeyModifiers;

	return result;
}

// Open a URL with the user's default web browser.
// Must begin with protocol identifier.
//virtual
void LLWindowSDL::spawnWebBrowser(const std::string& escaped_url, bool async)
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

	llinfos << "Spawning browser with URL: " << escaped_url << llendl;

	if (mSDL_Display)
	{
		// Just in case - before forking.
		XSync(mSDL_Display, False);
	}

	std::string cmd = gDirUtilp->getAppRODataDir() + "/bin/launch_url.sh";
	std::string arg = escaped_url;
	exec_cmd(cmd, arg);

	llinfos << "Returned from web browser launch." << llendl;
}

//virtual
void* LLWindowSDL::getPlatformWindow()
{
	return NULL;
}

// This method is currently used when we are 'launched' via an SLURL or, with
// SDL2, before taking snapshots (so that the latter does not get ruined with
// dirty (unrefreshed) rectangles due to other overlapping windows). HB
//virtual
void LLWindowSDL::bringToFront()
{
	if (mWindow)
	{
		llinfos << "Bringing viewer window to front" << llendl;
		SDL_RaiseWindow(mWindow);
	}
	if (mFullscreen || !mSDL_Display)
	{
		return;
	}

	llinfos << "Bringing viewer window to front" << llendl;

	// We must find the frame window Id used by the window manager, but if we
	// cannot find any WM window frame, use our own window... HB
	Window wm_window = mSDL_XWindowID;

	Window root, parent;
	Window* childlist = NULL;
	unsigned int num_children;
	int res = XQueryTree(mSDL_Display, mSDL_XWindowID, &root, &parent,
						 &childlist, &num_children);
	if (res && parent && parent != mSDL_XWindowID)
	{
		wm_window = parent;
		llinfos << "Found WM frame window Id: " << wm_window << llendl;
	}
	if (childlist)
	{
		 XFree(childlist);
	}

	// Now raise the frame to the top of the window stack (which will raise all
	// its chidren with it, our own root window included).
	XRaiseWindow(mSDL_Display, wm_window);
	XMapRaised(mSDL_Display, wm_window);

	// This could be needed by some window managers which would ignore the X11
	// request, on the condition they are configured to raise focused windows
	// on top... HB
	XEvent event = { 0 };
	event.xclient.type = ClientMessage;
	event.xclient.serial = 0;
	event.xclient.send_event = True;
	event.xclient.message_type = XInternAtom(mSDL_Display,
											 "_NET_ACTIVE_WINDOW", False);
	event.xclient.window = wm_window;
	event.xclient.format = 32;
	XSendEvent(mSDL_Display, DefaultRootWindow(mSDL_Display), False,
			   SubstructureRedirectMask | SubstructureNotifyMask, &event);

	XSync(mSDL_Display, False);
}

//static
std::vector<std::string> LLWindowSDL::getDynamicFallbackFontList()
{
	// Use libfontconfig to find us a nice ordered list of fallback fonts
	// specific to this system.
	std::string final_fallback = "/usr/share/fonts/TTF/dejavu/DejaVuSans.ttf";

	// Fonts are expensive in the current system, do not enumerate too many of
	// them
	constexpr size_t max_font_count_cutoff = 40;

	// Our 'ideal' font properties which define the sorting results.
	// slant=0 means Roman, index=0 means the first face in a font file (the
	// one we actually use), weight=80 means medium weight, spacing=0 means
	// proportional spacing.
	std::string sort_order("slant=0:index=0:weight=80:spacing=0");
	// elide_unicode_coverage removes fonts from the list whose unicode range
	// is covered by fonts earlier in the list. This usually removes ~90% of
	// the fonts as redundant (which is great because the font list can be
	// huge), but might unnecessarily reduce the renderable range if for some
	// reason our FreeType actually fails to use some of the fonts we want it
	// to.
	constexpr bool elide_unicode_coverage = true;
	std::vector<std::string> rtns;
	FcFontSet* fs = NULL;
	FcPattern* sortpat = NULL;

	llinfos << "Getting system font list from FontConfig..." << llendl;

	// If the user has a system-wide language preference, then favor fonts from
	// that language group. This does not affect the types of languages that
	// can be displayed, but ensures that their preferred language is rendered
	// from a single consistent font where possible.
	FL_Locale* locale = NULL;
	FL_Success success = FL_FindLocale(&locale, FL_MESSAGES);
	if (success != 0)
	{
		if (success >= 2 && locale->lang) // confident!
		{
			llinfos << "Language " << locale->lang << llendl;
			llinfos << "Location " << locale->country << llendl;
			llinfos << "Variant " << locale->variant << llendl;
			llinfos << "Preferring fonts of language: " << locale->lang
					<< llendl;
			sort_order = "lang=" + std::string(locale->lang) + ":" + sort_order;
		}
	}
	FL_FreeLocale(&locale);

	if (!FcInit())
	{
		llwarns << "FontConfig failed to initialize." << llendl;
		rtns.emplace_back(final_fallback);
		return rtns;
	}

	sortpat = FcNameParse((FcChar8*)sort_order.c_str());
	if (sortpat)
	{
		// Sort the list of system fonts from most-to-least-desirable.
		FcResult result;
		fs = FcFontSort(NULL, sortpat, elide_unicode_coverage, NULL, &result);
		FcPatternDestroy(sortpat);
	}

	int found_font_count = 0;
	if (fs)
	{
		// Get the full pathnames to the fonts, where available, which is what
		// we really want.
		std::string lc_name;
		found_font_count = fs->nfont;
		for (int i = 0; i < fs->nfont; ++i)
		{
			FcChar8* filename;
			if (FcResultMatch == FcPatternGetString(fs->fonts[i], FC_FILE, 0,
													&filename)
				&& filename)
			{
				std::string name((const char*)filename);
				lc_name = name;
				LLStringUtil::toLower(lc_name);
				size_t len = lc_name.length();
				if (lc_name.rfind(".pcf") == len - 4 ||
					lc_name.rfind(".pcf.gz") == len - 7)
				{
					// Not a true type font, skip it !
					LL_DEBUGS("Window") << name
										<< " is a bitmap font, skipping..."
										<< LL_ENDL;
					continue;
				}

				rtns.emplace_back(name);
				if (rtns.size() >= max_font_count_cutoff)
				{
					break; // hit limit
				}
			}
		}
		FcFontSetDestroy(fs);
	}

	LL_DEBUGS("Window") << "Using font list: ";
	for (size_t i = 0, count = rtns.size(); i < count; ++i)
	{
		LL_CONT << "    " << rtns[i];
	}
	LL_CONT << LL_ENDL;
	llinfos << "Using " << rtns.size() << "/" << found_font_count
			<< " system fonts." << llendl;

	rtns.emplace_back(final_fallback);
	return rtns;
}

///////////////////////////////////////////////////////////////////////////////
// Splash screen implementation (c)2020-2021 Henri Beauchamp
///////////////////////////////////////////////////////////////////////////////

class LLSplashScreenSDLImpl
{
public:
	LLSplashScreenSDLImpl();
	~LLSplashScreenSDLImpl();

	void show();
	void hide();
	void update(const std::string& msg);

private:
	Display*	mDisplay;
	Window		mWindow;
	XColor		mShadow;
	GC			mGC;
	S32			mScreen;
	S32			mWidth;
	S32			mHeight;
	S32			mTextXOffset;
	S32			mTextYOffset;
};

// Default splash screen size. Must be smaller than the splash background that
// contains an icon on the left: if the splash pixmap can be created, its size
// will be used instead.
#define SPLASH_WIDTH 220
#define SPLASH_HEIGHT 50
// Offsets for the text in the splash screen
#define SPLASH_TEXT_X_OFFSET 15
#define SPLASH_TEXT_Y_OFFSET 28

LLSplashScreenSDLImpl::LLSplashScreenSDLImpl()
:	mWidth(SPLASH_WIDTH),
	mHeight(SPLASH_HEIGHT),
	mTextXOffset(SPLASH_TEXT_X_OFFSET),
	mTextYOffset(SPLASH_TEXT_Y_OFFSET)
{
	// Open a connection to the X11 server
	mDisplay = XOpenDisplay(NULL);
	if (!mDisplay)
	{
		llwarns << "Could not open the X11 display !" << llendl;
		return;
	}

	mScreen = DefaultScreen(mDisplay);

	S32 root_window = RootWindow(mDisplay, mScreen);

	// Try to create a pixmap from our splash bitmap file
	Pixmap splash_pixmap = {};
	bool has_spash_pixmap = false;
	while (true)
	{
		SDL_Surface* splash = load_bmp_resource("splash.bmp");
		if (!splash)
		{
			llwarns << "Could not load the splash background." << llendl;
			break;
		}
		Visual* default_visual = DefaultVisual(mDisplay, mScreen);
		S32 width = splash->w;
		S32 height = splash->h;
		SDL_Surface* surf = SDL_CreateRGBSurface(0, width, height, 32,
												 default_visual->red_mask,
												 default_visual->green_mask,
												 default_visual->blue_mask, 0);
		if (!surf)
		{
			llwarns << "Could not create the splash surface." << llendl;
			SDL_FreeSurface(splash);
			break;
		}

		SDL_Rect bounds;
		bounds.x = 0;
		bounds.y = 0;
		bounds.w = width;
		bounds.h = height;
		if (SDL_LowerBlit(splash, &bounds, surf, &bounds) != 0)
		{
			llwarns << "Could not blit the splash surface." << llendl;
			SDL_FreeSurface(splash);
			SDL_FreeSurface(surf);
			break;
		}

		S32 default_depth = DefaultDepth(mDisplay, mScreen);
		XImage* splash_img = XCreateImage(mDisplay, default_visual,
										  default_depth, ZPixmap, 0,
										  (char*)surf->pixels,
										  width, height, 32, 0);
		if (!splash_img)
		{
			llwarns << "Could not create the splash image." << llendl;
			SDL_FreeSurface(splash);
			SDL_FreeSurface(surf);
			break;
		}

#if LL_BIG_ENDIAN
		splash_img->byte_order = MSBFirst;
#else
		splash_img->byte_order = LSBFirst;
#endif
		splash_pixmap = XCreatePixmap(mDisplay, root_window, width, height,
									  default_depth);
		XGCValues gc_values;
		GC gc = XCreateGC(mDisplay, splash_pixmap, 0, &gc_values);
		XPutImage(mDisplay, splash_pixmap, gc, splash_img, 0, 0, 0, 0,
				  width, height);
		XFreeGC(mDisplay, gc);
		surf->pixels = NULL;
		SDL_FreeSurface(splash);
		SDL_FreeSurface(surf);

		// We have a splash background, adjust the window size and text offsets
		// accordingly.
		has_spash_pixmap = true;
		// Assuming the difference is the icon width on left:
		mTextXOffset += width - mWidth;
		// Half the difference since we want the text vertically centered
		mTextYOffset += (height - mHeight) / 2;
		mWidth = width;
		mHeight = height;
		break;
	}

	// *TODO: use a nice and adequately sized font...

	// Create a custom color map for the grey background.
	Colormap colormap = DefaultColormap(mDisplay, mScreen);
	XColor grey;
	XParseColor(mDisplay, colormap, "#D8D8D8", &grey);
	XAllocColor(mDisplay, colormap, &grey);
	// Create an X11 window
	mWindow = XCreateSimpleWindow(mDisplay, root_window, 0, 0, mWidth, mHeight,
								  0, BlackPixel(mDisplay, mScreen),
								  grey.pixel);
	if (!mWindow)
	{
		llwarns << "Could not open an X11 window !" << llendl;
		XCloseDisplay(mDisplay);
		mDisplay = NULL;
		return;
	}

	// Specify the type of window (splash screen).
	// Note: the splash screen should automatically be centered by the window
	// manager, based on the _NET_WM_WINDOW_TYPE_SPLASH property set here. It
	// will also not have any window decoration.
	Atom type = XInternAtom(mDisplay, "_NET_WM_WINDOW_TYPE", False);
	Atom value = XInternAtom(mDisplay, "_NET_WM_WINDOW_TYPE_SPLASH", False);
	XChangeProperty(mDisplay, mWindow, type, XA_ATOM, 32, PropModeReplace,
					reinterpret_cast<unsigned char*>(&value), 1);

	if (has_spash_pixmap)
	{
		XSetWindowBackgroundPixmap(mDisplay, mWindow, splash_pixmap);
		mGC = NULL;
	}
	else
	{
		llwarns << "Could not create the background pixmap. The icon will be missing from the splash."
				<< llendl;
		// Create a custom GC for drawing the 3D borders in update()
		unsigned long valuemask = 0;
		XGCValues values;
		mGC = XCreateGC(mDisplay, mWindow, valuemask, &values);
		if (mGC)
		{
			XSetLineAttributes(mDisplay, mGC, 1, LineSolid, CapButt,
							   JoinBevel);
			// Color for the shadow line in the 3D borders.
			XParseColor(mDisplay, colormap, "#606060", &mShadow);
			XAllocColor(mDisplay, colormap, &mShadow);
		}
		else
		{
			llwarns << "Could not create a graphics context. The borders will be missing from the splash."
					<< llendl;
		}
	}

	// Select the only event we care for (Expose)
	XSelectInput(mDisplay, mWindow, ExposureMask);
}

LLSplashScreenSDLImpl::~LLSplashScreenSDLImpl()
{
	hide();
}

void LLSplashScreenSDLImpl::show()
{
	if (mDisplay)
	{
		XMapWindow(mDisplay, mWindow);
		// Wait for the window to be displayed
		XEvent e;
		while (!XCheckTypedEvent(mDisplay, Expose, &e)) ;
	}
}

void LLSplashScreenSDLImpl::hide()
{
	if (mDisplay)
	{
		if (mGC)
		{
			XFreeGC(mDisplay, mGC);
		}
		XUnmapWindow(mDisplay, mWindow);
		XDestroyWindow(mDisplay, mWindow);
		XFlush(mDisplay);
		XCloseDisplay(mDisplay);
		mDisplay = NULL;
	}
}

void LLSplashScreenSDLImpl::update(const std::string& msg)
{
	if (!mDisplay) return;

	// Clear old contents
	XClearWindow(mDisplay, mWindow);
	// Draw the text itself
	XDrawString(mDisplay, mWindow, DefaultGC(mDisplay, mScreen),
				mTextXOffset, mTextYOffset, msg.c_str(), msg.size());

	// Draw a 3D-like border around the window when we could not create a
	// background from the pixmap...
	if (mGC)
	{
		// First, a black "outer" rectangle (to ensure proper contrast when
		// the window is drawn above a light background).
		XSetForeground(mDisplay, mGC, BlackPixel(mDisplay, mScreen));
		XDrawLine(mDisplay, mWindow, mGC, 0, 0, mWidth - 1, 0);
		XDrawLine(mDisplay, mWindow, mGC, mWidth - 1, 0,
				  mWidth - 1, mHeight);
		XDrawLine(mDisplay, mWindow, mGC, mWidth, mHeight - 1,
				  0, mHeight - 1);
		XDrawLine(mDisplay, mWindow, mGC, 0, mHeight, 0, 0);

		// Then, the lower right, "inner" shadow corner
		XSetForeground(mDisplay, mGC, mShadow.pixel);
		XDrawLine(mDisplay, mWindow, mGC, mWidth - 2, 2,
				  mWidth - 2, mHeight - 1);
		XDrawLine(mDisplay, mWindow, mGC, mWidth - 2,
				  mHeight - 2, 2, mHeight - 2);

		// Finally, the upper left, "inner" lit corner
		XSetForeground(mDisplay, mGC, WhitePixel(mDisplay, mScreen));
		XDrawLine(mDisplay, mWindow, mGC, 1, mHeight - 2, 1, 1);
		XDrawLine(mDisplay, mWindow, mGC, 1, 1, mWidth - 2, 1);
	}

	XFlush(mDisplay);
	//sleep(1);	// For debugging, to let some time to read it !
}

LLSplashScreenSDL::LLSplashScreenSDL()
:	mImpl(NULL)
{
	// Since LLSplashScreen is invoked before creating the main window, we must
	// call this here !
	LLWindowSDL::initXlibThreads();

	mImpl = new LLSplashScreenSDLImpl;
}

LLSplashScreenSDL::~LLSplashScreenSDL()
{
	if (mImpl)
	{
		delete mImpl;
		mImpl = NULL;
	}
}

void LLSplashScreenSDL::showImpl()
{
	if (mImpl)
	{
		mImpl->show();
	}
}

void LLSplashScreenSDL::updateImpl(const std::string& msg)
{
	if (mImpl)
	{
		mImpl->update(msg);
	}
}

void LLSplashScreenSDL::hideImpl()
{
	if (mImpl)
	{
		mImpl->hide();
	}
}

///////////////////////////////////////////////////////////////////////////////
// Message box implementation (c)2015 Henri Beauchamp
///////////////////////////////////////////////////////////////////////////////

S32 OSMessageBoxSDL(const std::string& text, const std::string& caption,
					U32 type)
{
#if 0 // Sadly, this does not work ("No message system available" or
	  // "msgbox child process failed", depending on SDL2 version), and yes
	  // I did also try with the "full" SDL_ShowMessageBox() function, with the
	  // same result... HB
	// SDL2 implements a simple message box with just one OK button.
	// This is the only type currently used by the Linux viewer anyway.
	if (type == OSMB_OK)
	{
		std::string lc_caption = caption;
		LLStringUtil::toLower(lc_caption);
		U32 sdl_type;
		if (lc_caption.find("error") != std::string::npos)
		{
			sdl_type = SDL_MESSAGEBOX_ERROR;
		}
		else
		{
			sdl_type = SDL_MESSAGEBOX_INFORMATION;
		}
		S32 ret = SDL_ShowSimpleMessageBox(sdl_type, caption.c_str(),
										   text.c_str(), NULL);
		if (ret == 0)
		{
			return OSBTN_OK;
		}
		llwarns << "Error creating SDL dialog: " << SDL_GetError() << llendl;
		// Else, fall-back to the generic message box code below...
	}
#endif
	setenv("MESSAGE_BOX_CAPTION", caption.c_str(), 1);
	const std::string typestr = llformat("%d", type);
	setenv("MESSAGE_BOX_TYPE", typestr.c_str(), 1);
	std::string cmd = gDirUtilp->getAppRODataDir() + "/bin/messagebox.sh";
	S32 ret = exec_cmd(cmd, text);
	if (ret == -1)
	{
		llwarns << "MSGBOX (" << type << "): " << caption << ": " << text
				<< llendl;
	}
	return ret;
}

#endif // LL_LINUX
