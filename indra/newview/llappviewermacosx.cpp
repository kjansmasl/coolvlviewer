/**
 * @file llappviewermacosx.cpp
 * @brief The LLAppViewerMacOSX class definitions
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include <signal.h>
#include <exception>
#include <ApplicationServices/ApplicationServices.h>

#include "llappviewermacosx.h"

#include "llapp.h"
#include "llcommandlineparser.h"
#include "lldir.h"
#include "llmd5.h"

#include "llurldispatcher.h"
#include "llviewercontrol.h"
#include "llwindowmacosx-objc.h"

namespace
{
	// The command line args stored. They are not used immediately by the app.
	int					gArgC;
	char**				gArgV;

	LLAppViewerMacOSX*	gViewerAppPtr = NULL;

	void				(*gOldTerminateHandler)() = NULL;
    std::string			gHandleSLURL;
}

static void exceptionTerminateHandler()
{
	// Reinstall default terminate() handler in case we re-terminate.
	if (gOldTerminateHandler)
	{
		std::set_terminate(gOldTerminateHandler);
	}

	// Treat this like a regular viewer crash, with nice stacktrace etc.
	LLAppViewer::handleViewerCrash();

	// We have probably been killed-off before now, but...
	gOldTerminateHandler(); // call old terminate() handler
}

bool initViewer()
{
	// Set the working dir to <bundle>/Contents/Resources
	if (chdir(gDirUtilp->getAppRODataDir().c_str()) == -1)
	{
		llwarns << "Could not change directory to "
				<< gDirUtilp->getAppRODataDir() << ": " << strerror(errno)
				<< llendl;
	}

	gViewerAppPtr = new LLAppViewerMacOSX();

	// Install unexpected exception handler
	gOldTerminateHandler = std::set_terminate(exceptionTerminateHandler);

	LLApp::setErrorHandler(LLAppViewer::handleViewerCrash);

	LLApp::InitState state = gViewerAppPtr->init();
	bool ok = state == LLApp::INIT_OK;
	if (!ok && state != LLApp::INIT_OK_EXIT)
	{
		llwarns << "Application init failed." << llendl;
	}
	else if (!gHandleSLURL.empty())
	{
		dispatchUrl(gHandleSLURL);
		gHandleSLURL = "";
	}
	return ok;
}

void handleQuit()
{
	gAppViewerp->userQuit();
}

bool pumpMainLoop()
{
	bool ret = LLApp::isQuitting();
	if (!ret && gViewerAppPtr)
	{
		ret = gViewerAppPtr->mainLoop();
	}
	else
	{
		ret = true;
	}
	return ret;
}

void cleanupViewer()
{
	if (!LLApp::isError() && gViewerAppPtr)
	{
		gViewerAppPtr->cleanup();
	}
	delete gViewerAppPtr;
	gViewerAppPtr = NULL;
}

int main(int argc, char** argv)
{
	// Store off the command line args for use later.
	gArgC = argc;
	gArgV = argv;
	S32 exit_code = createNSApp(argc, (const char**)argv);
	return gExitCode ? gExitCode : exit_code;
}

// macOS may add and addition command line arguement for the process serial
// number. The option takes a form like '-psn_0_12345'. The following method
// should be able to recognize and either ignore or return a pair of values for
// the option. Look for this method to be added to the parser in
// parseAndStoreResults.
std::pair<std::string, std::string> parse_psn(const std::string& s)
{
	if (s.find("-psn_") == 0)
	{
		// *FIX:Mani Not sure that the value makes sense.
		// fix it once the actual -psn_XXX syntax is known.
		return std::make_pair("psn", s.substr(5));
	}
	else
	{
		return std::make_pair(std::string(), std::string());
	}
}

bool LLAppViewerMacOSX::initParseCommandLine(LLCommandLineParser& clp)
{
	// The next two lines add the support for parsing the mac -psn_XXX arg.
	clp.addOptionDesc("psn", NULL, 1, "MacOSX process serial number");
	clp.setCustomParser(parse_psn);

	// First parse the command line, not often used on the mac.
	if (clp.parseCommandLine(gArgC, gArgV) == false)
	{
		return false;
	}

	// Now read in the arguments from the arguments.txt file, when it exists.
	// Successive calls to clp.parse... will NOT override earlier options.
	const char* filename = "arguments.txt";
	llifstream ifs(filename, std::ifstream::binary);
	if (ifs.is_open())
	{
		if (clp.parseCommandLineFile(ifs) == false)
		{
			return false;
		}
	}

	// Get the user's preferred language string based on the Mac OS
	// localization mechanism. To add a new localization:
	// go to the "Resources" section of the project
	// get info on "language.txt"
	// in the "General" tab, click the "Add Localization" button
	// create a new localization for the language you're adding
	// set the contents of the new localization of the file to the string
	// corresponding to our localization (i.e. "en-us", "ja", etc. Use the
	// existing ones as a guide.)
	CFURLRef url = CFBundleCopyResourceURL(CFBundleGetMainBundle(),
										   CFSTR("language"), CFSTR("txt"),
										   NULL);
	char path[MAX_PATH];
	if (CFURLGetFileSystemRepresentation(url, false, (UInt8*)path, sizeof(path)))
	{
		std::string lang;
		if (_read_file_into_string(lang, path))
		{
			LLControlVariable* c = gSavedSettings.getControl("SystemLanguage");
			if (c)
			{
				c->setValue(lang, false);
			}
		}
	}
	CFRelease(url);

	return true;
}

// *FIX:Mani It would be nice to provide a clean interface to get the
// default_unix_signal_handler for the LLApp class.
extern void default_unix_signal_handler(int, siginfo_t*, void*);
bool LLAppViewerMacOSX::restoreErrorTrap()
{
	// This method intends to reinstate signal handlers.
	// *NOTE:Mani It was found that the first execution of a shader was overriding
	// our initial signal handlers somehow.
	// This method will be called (at least) once per mainloop execution.
	// *NOTE:Mani The signals used below are copied over from the
	// setup_signals() func in LLApp.cpp
	// LLApp could use some way of overriding that func, but for this viewer
	// fix I opt to avoid affecting the server code.

	// Set up signal handlers that may result in program termination
	//
	struct sigaction act;
	struct sigaction old_act;
	act.sa_sigaction = default_unix_signal_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	unsigned int reset_count = 0;

#define SET_SIG(S) 	sigaction(SIGABRT, &act, &old_act); \
					if (act.sa_sigaction != old_act.sa_sigaction) \
						++reset_count;
	// Synchronous signals
	SET_SIG(SIGABRT)
	SET_SIG(SIGALRM)
	SET_SIG(SIGBUS)
	SET_SIG(SIGFPE)
	SET_SIG(SIGHUP)
	SET_SIG(SIGILL)
	SET_SIG(SIGPIPE)
	SET_SIG(SIGSEGV)
	SET_SIG(SIGSYS)

	SET_SIG(LL_HEARTBEAT_SIGNAL)
	SET_SIG(LL_SMACKDOWN_SIGNAL)

	// Asynchronous signals that are normally ignored
	SET_SIG(SIGCHLD)
	SET_SIG(SIGUSR2)

	// Asynchronous signals that result in attempted graceful exit
	SET_SIG(SIGHUP)
	SET_SIG(SIGTERM)
	SET_SIG(SIGINT)

	// Asynchronous signals that result in core
	SET_SIG(SIGQUIT)
#undef SET_SIG

	return reset_count == 0;
}

void LLAppViewerMacOSX::handleSyncCrashTrace()
{
	// Free our reserved memory space before dumping the stack trace (it should
	// already be freed at this point, but it does not hurt calling this
	// function twice).
	LLMemory::cleanupClass();
}

std::string LLAppViewerMacOSX::generateSerialNumber()
{
	char serial_md5[MD5HEX_STR_SIZE];
	serial_md5[0] = 0;

	// JC: Sample code from http://developer.apple.com/technotes/tn/tn1103.html
	CFStringRef serial_num = NULL;
	io_service_t platformExpert =
		IOServiceGetMatchingService(kIOMasterPortDefault,
									IOServiceMatching("IOPlatformExpertDevice"));
	if (platformExpert)
	{
		serial_num =
			(CFStringRef)IORegistryEntryCreateCFProperty(platformExpert,
														 CFSTR(kIOPlatformSerialNumberKey),
														 kCFAllocatorDefault, 0);
		IOObjectRelease(platformExpert);
	}

	if (serial_num)
	{
		char buffer[MAX_STRING];
		if (CFStringGetCString(serial_num, buffer, MAX_STRING,
							   kCFStringEncodingASCII))
		{
			LLMD5 md5((unsigned char*)buffer);
			md5.hex_digest(serial_md5);
		}
		CFRelease(serial_num);
	}

	return serial_md5;
}

void handleUrl(const char* url_utf8)
{
	if (url_utf8 && gViewerAppPtr)
	{
		gHandleSLURL = "";
		dispatchUrl(url_utf8);
	}
	else if (url_utf8)
	{
		gHandleSLURL = url_utf8;
	}
}

void dispatchUrl(std::string url)
{
	// Safari 3.2 silently mangles secondlife:///app/ URLs into
	// secondlife:/app/ (only one leading slash).
	// Fix them up to meet the URL specification. JC
	const std::string prefix = "secondlife:/app/";
	std::string test_prefix = url.substr(0, prefix.length());
	LLStringUtil::toLower(test_prefix);
	if (test_prefix == prefix)
	{
		url.replace(0, prefix.length(), "secondlife:///app/");
	}

	LLMediaCtrl* web = NULL;
	LLURLDispatcher::dispatch(url, "clicked", web, false);
}
