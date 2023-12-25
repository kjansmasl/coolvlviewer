/**
 * @file slplugin.cpp
 * @brief Loader shell for plugins, intended to be launched by the plugin host application, which directly loads a plugin dynamic library.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include "llapr.h"
#include "llerrorcontrol.h"
#include "llpluginprocesschild.h"
#include "llpluginmessage.h"
#include "llstring.h"

#include <iostream>
#include <fstream>
using namespace std;

#if LL_WINDOWS
# include <windows.h>
#endif

#if LL_DARWIN
# include "slplugin-objc.h"
#endif

#if LL_DARWIN || LL_LINUX
# include <signal.h>
#endif

#if LL_LINUX
# include <X11/Xlib.h>
#endif

#if LL_DARWIN || LL_LINUX
// Signal handlers to make crashes not show an OS dialog...
static void crash_handler(int sig)
{
	// Just exit cleanly. *TODO: add our own crash reporting
	_exit(1);
}
#endif

#if LL_WINDOWS
// Our exception handler: it will probably just exit and the host application
// will miss the heartbeat and log the error in the usual fashion.
LONG WINAPI myWin32ExceptionHandler(struct _EXCEPTION_POINTERS* exception_infop)
{
	// *TODO: replace exception handler before we exit ?
	return EXCEPTION_EXECUTE_HANDLER;
}

bool checkExceptionHandler()
{
	bool ok = true;
	LPTOP_LEVEL_EXCEPTION_FILTER prev_filter =
		SetUnhandledExceptionFilter(myWin32ExceptionHandler);

	if ((void*)prev_filter != (void*)myWin32ExceptionHandler)
	{
		llwarns << "Our exception handler (" << (void*)myWin32ExceptionHandler
				<< ") replaced with " << (void*)prev_filter << "!" << llendl;
		ok = false;
	}

	if (!prev_filter)
	{
		ok = false;
		llwarns << "Our exception handler (" << (void*)myWin32ExceptionHandler
				<< ") replaced with NULL !" << llendl;
	}

	return ok;
}
#endif

// If this application on Windows platform is a console application, a console
// is always created which is bad. Making it a Windows "application" via CMake
// settings but not adding any code to explicitly create windows does the right
// thing.
#if LL_WINDOWS
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR cmd_line, int)
#else
extern "C" {
int main(int argc, char** argv);
}

int main(int argc, char** argv)
#endif
{
#if LL_LINUX
	// Ensure Xlib is started in thread-safe state
	XInitThreads();
#endif

	ll_init_apr();

	// Set up llerror logging
	{
		LLError::initForApplication(".");
		LLError::setDefaultLevel(LLError::LEVEL_INFO);
#if 0
		LLError::setTagLevel("Plugin", LLError::LEVEL_DEBUG);
		LLError::logToFile("slplugin.log");
#endif
	}

#if LL_WINDOWS
	if (strlen(cmd_line) == 0)
	{
		llerrs << "Usage: SLPlugin launcher_port" << llendl;
	}

	U32 port = 0;
	if (!LLStringUtil::convertToU32(cmd_line, port))
	{
		llerrs << "Port number must be numeric" << llendl;
	}

	// Insert our exception handler into the system so this plugin doesn't
	// display a crash message if something bad happens. The host app will
	// see the missing heartbeat and log appropriately.
	SetUnhandledExceptionFilter(myWin32ExceptionHandler);
#elif LL_DARWIN || LL_LINUX
	if (argc < 2)
	{
		llerrs << "Usage: " << argv[0] << " launcher_port" << llendl;
	}

	U32 port = 0;
	if (!LLStringUtil::convertToU32(argv[1], port))
	{
		llerrs << "Port number must be numeric" << llendl;
	}

	// Catch signals that most kinds of crashes will generate, and exit cleanly
	// so the system crash dialog isn't shown.
	signal(SIGILL, &crash_handler);		// illegal instruction
	signal(SIGFPE, &crash_handler);		// floating-point exception
	signal(SIGBUS, &crash_handler);		// bus error
	signal(SIGSEGV, &crash_handler);	// segmentation violation
	signal(SIGSYS, &crash_handler);		// non-existent system call invoked
# if LL_DARWIN
	signal(SIGEMT, &crash_handler);		// emulate instruction executed

    LLCocoaPlugin cocoa_interface;
	cocoa_interface.setupCocoa();
	cocoa_interface.createAutoReleasePool();
# endif	// LL_DARWIN
#endif

	LLPluginProcessChild* plugin = new LLPluginProcessChild();
	plugin->init(port);

#if LL_DARWIN
	cocoa_interface.deleteAutoReleasePool();
#endif

	LLTimer timer;
	timer.start();

#if LL_WINDOWS
	checkExceptionHandler();
#endif

#if LL_DARWIN && 0	// Disabled
	// If the plugin opens a new window (such as the Flash plugin's fullscreen
	// player), we may need to bring this plugin process to the foreground.
	// Use this to track the current frontmost window and bring this process to
	// the front if it changes.
	cocoa_interface.mEventTarget = GetEventDispatcherTarget();
#endif
	while (!plugin->isDone())
	{
#if LL_DARWIN
		cocoa_interface.createAutoReleasePool();
#endif
		timer.reset();
		plugin->idle();
#if LL_DARWIN
		cocoa_interface.processEvents();
#endif
		F64 elapsed = timer.getElapsedTimeF64();
		F64 remaining = plugin->getSleepTime() - elapsed;

		if (remaining <= 0.f)
		{
			// We have already used our full allotment. Still need to service
			// the network...
			plugin->pump();
		}
		else
		{
			// This also services the network as needed.
			plugin->sleep(remaining);
		}

#if LL_WINDOWS && 0	// Does not appear to be required so far, even for plugins
					// that do crash with a single call to the intercept
					// exception handler.
		// More agressive checking of interfering exception handlers.
		checkExceptionHandler();
#endif

#if LL_DARWIN
		cocoa_interface.deleteAutoReleasePool();
#endif
	}

	delete plugin;

	ll_cleanup_apr();

	return 0;
}
