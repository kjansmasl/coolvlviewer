/**
 * @file llappviewerwin32.cpp
 * @brief The LLAppViewerWin32 class definitions
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

// Unreferenced formal parameter:
#pragma warning(disable: 4100)
// Non-standard extension used (zero-sized array in struct/union):
#pragma warning(disable: 4200)
// printf format string
#pragma warning(disable: 4477)

#include <fcntl.h>				// For _O_APPEND
#include <io.h>					// For _open_osfhandle()
#include <werapi.h>				// For WerAddExcludedApplication()
#include <process.h>			// For _spawnl()
#include <tchar.h>				// For TCHAR support
#include <tlhelp32.h>
#include <dbghelp.h>
#include <malloc.h>

#include "res/resource.h"		// *FIX: for setting gIconResource.

#include "llappviewerwin32.h"

#include "llapp.h"
#include "llcommandlineparser.h"
#include "lldir.h"
#include "lldxhardware.h"
#include "llfindlocale.h"
#include "llmd5.h"
#include "llsdserialize.h"
#include "llsys.h"
#include "llwindowwin32.h"

#include "llgridmanager.h"
#include "llviewercontrol.h"
#include "llweb.h"

///////////////////////////////////////////////////////////////////////////////
// LLWinDebug class (used to be held in llwindebug.h and llwindebug.cpp)
///////////////////////////////////////////////////////////////////////////////

static const std::string sDumpFilename = "CoolVLViewer.dmp";

class LLWinDebug
{
protected:
	LOG_CLASS(LLWinDebug);

public:
	// Initializes the LLWinDebug exception filter callback. Hands a windows
	// unhandled exception filter to LLWinDebug. This method should only be
	// called to change the exception filter used by LLWinDebug. Setting
	// filter_func to NULL will clear any custom filters.
	static void initExceptionHandler(LPTOP_LEVEL_EXCEPTION_FILTER filter_func);

	// Checks the status of the exception filter. Resets unhandled exception
	// filter to the filter specified  with initExceptionFilter. Returns false
	// if the exception filter was modified.
	static bool checkExceptionHandler();

	static void generateCrashStacks(struct _EXCEPTION_POINTERS* ex = NULL);
	static void clearCrashStacks(); // Deletes the crash stack file(s).
};

// Based on dbghelp.h
typedef BOOL (WINAPI* MINIDUMPWRITEDUMP)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
										 CONST PMINIDUMP_EXCEPTION_INFORMATION,
										 CONST PMINIDUMP_USER_STREAM_INFORMATION,
										 CONST PMINIDUMP_CALLBACK_INFORMATION);
static MINIDUMPWRITEDUMP sMiniDumpProcAdrp = NULL;
static LPTOP_LEVEL_EXCEPTION_FILTER sFilterFunc = NULL;

class LLMemoryReserve
{
public:
	LLMemoryReserve()
	:	mReserved(NULL)
	{
	}

	~LLMemoryReserve()
	{
		release();
	}

	void reserve()
	{
		if (!mReserved)
		{
			constexpr size_t MEMORY_RESERVATION_SIZE = 8 * 1024 * 1024;
			mReserved = malloc(MEMORY_RESERVATION_SIZE);
		}
	}

	void release()
	{
		if (mReserved)
		{
			free(mReserved);
			mReserved = NULL;
		}
	}

private:
	void* mReserved;
};

static LLMemoryReserve sEmergencyMemoryReserve;

//static
void LLWinDebug::initExceptionHandler(LPTOP_LEVEL_EXCEPTION_FILTER filter_func)
{
	static bool s_first_run = true;
	// Load the dbghelp dll now, instead of waiting for the crash. Less
	// potential for stack mangling.
	if (s_first_run)
	{
		s_first_run = false;

		// First, try loading from the directory that the app resides in.
		std::string local_dll_name =
				gDirUtilp->findFile("dbghelp.dll", gDirUtilp->getWorkingDir(),
									gDirUtilp->getExecutableDir());

		HMODULE dll_handle = LoadLibraryA(local_dll_name.c_str());
		if (!dll_handle)
		{
			dll_handle = LoadLibrary(L"dbghelp.dll");
		}

		if (!dll_handle)
		{
			llwarns << "Could not find dbghelp.dll !" << llendl;
		}
		else
		{
			sMiniDumpProcAdrp =
				(MINIDUMPWRITEDUMP)GetProcAddress(dll_handle,
												  "MiniDumpWriteDump");
			if (!sMiniDumpProcAdrp)
			{
				llwarns << "Could not find the MiniDumpWriteDump() function !"
						<< llendl;
				FreeLibrary(dll_handle);
				dll_handle = NULL;
			}
		}

		sEmergencyMemoryReserve.reserve();
	}

	LPTOP_LEVEL_EXCEPTION_FILTER prev_filter;
	prev_filter = SetUnhandledExceptionFilter(filter_func);
	if (prev_filter != sFilterFunc)
	{
		llwarns << "Replacing unknown exception (" << (void*)prev_filter
				<< ") with (" << (void *)filter_func << ") !" << llendl;
	}

	sFilterFunc = filter_func;
}

bool LLWinDebug::checkExceptionHandler()
{
	bool ok = true;
	LPTOP_LEVEL_EXCEPTION_FILTER prev_filter;
	prev_filter = SetUnhandledExceptionFilter(sFilterFunc);

	if (prev_filter != sFilterFunc)
	{
		llwarns << "Our exception handler (" << (void*)sFilterFunc
				<< ") replaced with " << (void*)prev_filter << "!" << llendl;
		ok = false;
	}

	if (!prev_filter)
	{
		ok = false;
		if (sFilterFunc)
		{
			llwarns << "Our exception handler (" << (void*)sFilterFunc
					<< ") replaced with NULL!" << llendl;
		}
		else
		{
			llwarns << "Exception handler uninitialized." << llendl;
		}
	}
	return ok;
}

// *NOTE: Mani - This method is no longer the exception handler. It is called
// from viewer_windows_exception_handler() and other places.
//static
void LLWinDebug::generateCrashStacks(struct _EXCEPTION_POINTERS* ex_infop)
{
	// Since there is exception info... Release the hounds.
	sEmergencyMemoryReserve.release();

	if (!ex_infop || !sMiniDumpProcAdrp || !gDirUtilp)
	{
		return;
	}

	_MINIDUMP_EXCEPTION_INFORMATION info;
	info.ThreadId = ::GetCurrentThreadId();
	info.ExceptionPointers = ex_infop;
	info.ClientPointers = NULL;

	std::string dump_path =
		gDirUtilp->getExpandedFilename(LL_PATH_LOGS, sDumpFilename);
	HANDLE fhandle = CreateFileA(dump_path.c_str(), GENERIC_WRITE,
								 FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
								 FILE_ATTRIBUTE_NORMAL, NULL);
	if (fhandle != INVALID_HANDLE_VALUE)
	{
		// Write the dump, ignoring the return value
		sMiniDumpProcAdrp(GetCurrentProcess(), GetCurrentProcessId(),
						  fhandle, MiniDumpNormal, &info, NULL, NULL);
		CloseHandle(fhandle);
	}
}

void LLWinDebug::clearCrashStacks()
{
	LLFile::remove(gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
												  sDumpFilename));
}

///////////////////////////////////////////////////////////////////////////////
// LLAppViewerWin32 stuff proper
///////////////////////////////////////////////////////////////////////////////

static bool sErrorReportingDisabled = false;

LONG WINAPI viewer_windows_exception_handler(struct _EXCEPTION_POINTERS* ex_infop)
{
	// Guard against possible reentry of the error handler... HB
	static bool reentry = false;
	if (reentry)
	{
	    _tprintf(_T("Got another fatal signal while in the exception handler, die now !\n"));
		llwarns << "Got another fatal signal while in the exception handler, die now !"
				<< llendl;
		return EXCEPTION_EXECUTE_HANDLER;
	}
	reentry = true;

    // *NOTE:Mani - this code is stolen from LLApp, where it is never actually
	// used. Translate the signals/exceptions into cross-platform stuff Windows
	// implementation
    _tprintf(_T("Entering Windows exception handler...\n"));
	llinfos << "Entering Windows exception handler..." << llendl;

	// Generate a minidump if we can.
	LLWinDebug::generateCrashStacks(ex_infop);

	if (!LLApp::isError())
	{
		// Flag status to error
		LLApp::setError();

		// Block in the exception handler until the app has stopped; this is
		// pretty sketchy, but appears to work just fine.
		while (!LLApp::isStopped())
		{
			ms_sleep(10);
		}
	}

	// At this point, we always want to exit the app. There is no graceful
	// recovery for an unhandled exception. Just kill the process.
	return EXCEPTION_EXECUTE_HANDLER;
}

// MSVC 2022 clang does not like the appearance following which hMutex is "not
// used" (it is, but only to verify its creation does not trigger an error)...
// So, let's silence that warning. HB
#if LL_CLANG
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-variable"
#endif
// Creates a unique global windows object. If the object can be created it
// returns true, otherwise it returns false. The false result can be used to
// determine if another instance of a Second Life app (this version or later)
// is running.
// NOTE: Do not use this method to run a single instance of the app. This is
// intended to help debug problems with the cross-platform locked file method
// used for that purpose.
bool create_app_mutex()
{
	bool result = true;
	LPCWSTR unique_mutex_name = L"SecondLifeAppMutex";
	HANDLE hMutex = CreateMutex(NULL, TRUE, unique_mutex_name);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		result = false;
	}
	return result;
}
#if LL_CLANG
# pragma clang diagnostic pop
#endif

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR cmd_line, int)
{
	LLWindowWin32::setDPIAwareness();

	// Enable the low fragmentation heap; this results in a 2-3x improvement in
	// opening a new Inventory window (which uses a large numebr of
	// allocations).
	_CrtSetDbgFlag(0);	// Default, just making explicit
	ULONG enable_heap = 2;
	constexpr S32 MAX_HEAPS = 255;
	DWORD heap_enable_lfh_error[MAX_HEAPS];
	HANDLE* heap_handles = new HANDLE[MAX_HEAPS];
	S32 num_heaps = GetProcessHeaps(MAX_HEAPS, heap_handles);
	for (S32 i = 0; i < num_heaps; ++i)
	{
		if (HeapSetInformation(heap_handles[i],
							   HeapCompatibilityInformation,
							   &enable_heap, sizeof(enable_heap)))
		{
			heap_enable_lfh_error[i] = 0;
		}
		else
		{
			heap_enable_lfh_error[i] = GetLastError();
		}
	}

	// *FIX: global
	gIconResource = MAKEINTRESOURCE(IDI_LL_ICON);

	LLAppViewerWin32* viewer_app_ptr = new LLAppViewerWin32(cmd_line);

	LLWinDebug::initExceptionHandler(viewer_windows_exception_handler);

	LLApp::setErrorHandler(LLAppViewer::handleViewerCrash);

	// Set a debug info flag to indicate if multiple instances are running.
	bool found_other_instance = !create_app_mutex();
	gDebugInfo["FoundOtherInstanceAtStartup"] = LLSD::Boolean(found_other_instance);

	LLApp::InitState state = viewer_app_ptr->init();
	if (state != LLApp::INIT_OK)
	{
		if (state != LLApp::INIT_OK_EXIT)
		{
			llwarns << "Application init failed." << llendl;
			return LLAppViewer::EXIT_INIT_FAILED;
		}
		return LLAppViewer::EXIT_OK;	// No error, just exiting immediately.
	}

	// We have to wait until after logging is initialized to be able write to
	// the log file...
	if (sErrorReportingDisabled)
	{
		llinfos << "Windows error reporting disabled successfully." << llendl;
	}
	else
	{
		llinfos << "Windows error reporting disabling failed." << llendl;
	}
	if (num_heaps > 0)
	{
		llinfos << "Attempting to enable the Low Fragmentation Heap feature for "
				<< num_heaps << " heaps:" << llendl;
		for (S32 i = 0; i < num_heaps; ++i)
		{
			if (heap_enable_lfh_error[i])
			{
				LL_DEBUGS("AppInit") << " -> LFH enabling failed for heap "
									 << i << " with error: "
									 << heap_enable_lfh_error[i] << LL_ENDL;
			}
			else
			{
				llinfos << " -> LFH enabled for heap " << i << llendl;
			}
		}
	}

	// Run the application main loop
	if (!LLApp::isQuitting())
	{
		viewer_app_ptr->mainLoop();
	}

	// We do not want to do cleanup here if the error handler got called; the
	// assumption is that the error handler is responsible for doing app
	// cleanup if there was a problem.
	if (LLApp::isError())
	{
		delete viewer_app_ptr;
	}
	else
	{
		viewer_app_ptr->cleanup();
		delete viewer_app_ptr;
#if 1
		// *HACK: force-kill the viewer process to avoid getting a zombie
		// (background, never-ending) process.
		// *TODO: find out why the f*cking hell the use of libepoxy causes the
		// viewer to never exit otherwise under Windoze... HB
		TerminateProcess(GetCurrentProcess(), gExitCode);
#endif
	}

	return gExitCode;
}

// SL-13528: This code used to be based on:
// http://dslweb.nwnexus.com/~ast/dload/guicon.htm
// (referenced in https://stackoverflow.com/a/191880).
// But one of the comments on that StackOverflow answer points out that
// assigning to *stdout or *stderr "probably doesn't even work with the
// Universal CRT that was introduced in 2015," suggesting freopen_s() instead.
// Code below is based on https://stackoverflow.com/a/55875595.
static bool set_stream(FILE* fp, DWORD handle_id, const char* name,
					   const std::string& mode)
{
	HANDLE l_std_handle = GetStdHandle(handle_id);
	if (l_std_handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	if (mode.find('w') != std::string::npos)
	{
		// Enable color processing for output streams
		DWORD dw_mode = 0;
		GetConsoleMode(l_std_handle, &dw_mode);
		dw_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(l_std_handle, dw_mode);
	}

	// Redirect the passed fp to the console.
	FILE* ignore;
	if (freopen_s(&ignore, name, mode.c_str(), fp) == 0)
	{
		// Use unbuffered I/O
		setvbuf(fp, NULL, _IONBF, 0);
	}
	return true;
}

LLAppViewerWin32::LLAppViewerWin32(const char* cmd_line)
:	mCmdLine(cmd_line),
	mIsConsoleAllocated(false)
{
}

// Platform specific initialization.
LLApp::InitState LLAppViewerWin32::init()
{
	// Turn off Windows error reporting (do not send our data to Microsoft)
	std::string executable_name = gDirUtilp->getExecutableFilename();
	std::wstring utf16_exec_name = ll_convert_string_to_wide(executable_name);
	sErrorReportingDisabled =
		WerAddExcludedApplication(utf16_exec_name.c_str(), FALSE) == S_OK;
	// Also exclude SLVoice.exe, SLPlugin.exe and dullahan_host.exe. HB
	executable_name = gDirUtilp->getExecutableDir() + "\\SLVoice.exe";
	utf16_exec_name = ll_convert_string_to_wide(executable_name);
	sErrorReportingDisabled &=
		WerAddExcludedApplication(utf16_exec_name.c_str(), FALSE) == S_OK;
	executable_name = gDirUtilp->getExecutableDir() + "\\SLPlugin.exe";
	utf16_exec_name = ll_convert_string_to_wide(executable_name);
	sErrorReportingDisabled &=
		WerAddExcludedApplication(utf16_exec_name.c_str(), FALSE) == S_OK;
	executable_name = gDirUtilp->getLLPluginDir() + "\\dullahan_host.exe";
	utf16_exec_name = ll_convert_string_to_wide(executable_name);
	sErrorReportingDisabled &=
		WerAddExcludedApplication(utf16_exec_name.c_str(), FALSE) == S_OK;

	// Initialize the viewer app proper.
	return LLAppViewer::init();
}

bool LLAppViewerWin32::cleanup()
{
	bool result = LLAppViewer::cleanup();

	if (mIsConsoleAllocated)
	{
		FreeConsole();
		mIsConsoleAllocated = false;
	}

	return result;
}

void LLAppViewerWin32::initLogging()
{
	// Remove the crash stack log from previous executions.
	// Since we have started logging a new instance of the app, we can assume
	// NOTE: This should happen before the we send a 'previous instance froze'
	// crash report, but it must happen after we initialize the DirUtil.
	LLWinDebug::clearCrashStacks();

	LLAppViewer::initLogging();
}

// Pops up the log console
void LLAppViewerWin32::initConsole()
{
	// Allocate a console for this app
	mIsConsoleAllocated = AllocConsole();
	if (mIsConsoleAllocated)
	{
		// Set the screen buffer to be big enough to let us scroll text
		CONSOLE_SCREEN_BUFFER_INFO coninfo;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
		constexpr S32 MAX_CONSOLE_LINES = 8000;
		coninfo.dwSize.Y = MAX_CONSOLE_LINES;
		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),
								   coninfo.dwSize);

		// Redirect unbuffered STDOUT to the console
		if (!set_stream(stdout, STD_OUTPUT_HANDLE, "CONOUT$", "w"))
		{
			llwarns << "Failed to redirect stdout to the console." << llendl;
		}

		// Redirect unbuffered STDERR to the console
		if (!set_stream(stderr, STD_ERROR_HANDLE, "CONOUT$", "w"))
		{
			llwarns << "Failed to redirect stderr to the console." << llendl;
		}

		// Redirect unbuffered STDIN to the console
		if (!set_stream(stdout, STD_OUTPUT_HANDLE, "CONIN$", "r"))
		{
			llwarns << "Failed to redirect stdin to the console." << llendl;
		}
	}
	return LLAppViewer::initConsole();
}

void write_debug_dx(const char* str)
{
	gDebugInfo["DXInfo"] = LLSD::String(gDebugInfo["DXInfo"].asString() + str);
}

void write_debug_dx(const std::string& str)
{
	write_debug_dx(str.c_str());
}

// Driver verification and initialization based on DirectX hardware polling and
// driver versions.
bool LLAppViewerWin32::initHardwareTest()
{
	if (!gSavedSettings.getBool("NoHardwareProbe"))
	{
		LLSplashScreen::update("Detecting hardware...");
		LL_DEBUGS("AppInit") << "Attempting to poll DirectX for hardware info"
							 << LL_ENDL;
		LLDXHardware::setWriteDebugFunc(write_debug_dx);
		LLSD info = gDXHardware.getDisplayInfo();
		if (!info.size() && gSavedSettings.getWarning("AboutDirectX"))
		{
			llwarns << "DirectX probe failed, alerting user." << llendl;
			// Warn them that running without DirectX 9 will not allow us to
			// tell them about driver issues.
			std::ostringstream msg;
			msg << gSecondLife
				<< " is unable to detect your graphics hardware via DirectX.\n\n"
				<< "This may be due to unsupported or outdated graphics drivers or\n"
				<< "hardware, and in these conditions the viewer might fail to run.\n"
				<< "\nDo you wish to continue ?\n";
			S32 button = OSMessageBox(msg.str(), "Warning", OSMB_YESNO);
			if (button == OSBTN_NO)
			{
				llinfos << "User quitting after failed DirectX detection"
						<< llendl;
				LLWeb::loadURLExternal(SUPPORT_URL, false);
				return false;
			}
			gSavedSettings.setWarning("AboutDirectX", false);
		}
		LL_DEBUGS("AppInit") << "Done polling DirectX for hardware info"
							 << LL_ENDL;

		std::ostringstream splash_msg;
		splash_msg << "Loading " << gSecondLife << "...";
		LLSplashScreen::update(splash_msg.str());
	}

	if (!gGLManager.mVRAM)
	{
		llinfos << "Detecting available VRAM via DXGI..." << llendl;
		gGLManager.mVRAM = LLDXHardware::getMBVideoMemoryViaDXGI();
	}
	llinfos << "Detected VRAM: " << gGLManager.mVRAM << llendl;

	if (!restoreErrorTrap())
	{
		llwarns << "Someone took over my exception handler (post hardware probe) !"
				<< llendl;
	}

	return true;
}

bool LLAppViewerWin32::initParseCommandLine(LLCommandLineParser& clp)
{
	if (!clp.parseCommandLineString(mCmdLine))
	{
		return false;
	}

	// Find the system language.
	FL_Locale* locale = NULL;
	FL_Success success = FL_FindLocale(&locale, FL_MESSAGES);
	if (success)
	{
		if (success >= 2 && locale->lang) // Confident !
		{
			llinfos << "Language: " << ll_safe_string(locale->lang) << llendl;
			llinfos << "Location: " << ll_safe_string(locale->country)
					<< llendl;
			llinfos << "Variant: " << ll_safe_string(locale->variant)
					<< llendl;
			LLControlVariable* c = gSavedSettings.getControl("SystemLanguage");
			if (c)
			{
				c->setValue(std::string(locale->lang), false);
			}
		}
	}
	FL_FreeLocale(&locale);

	return true;
}

bool LLAppViewerWin32::beingDebugged()
{
	bool debugged = LLError::Log::sIsBeingDebugged = IsDebuggerPresent();
	return debugged;
}

bool LLAppViewerWin32::restoreErrorTrap()
{
	static F32 last_check = 0.f;
	if (gFrameTimeSeconds - last_check < 2.f)
	{
		// Do not waste time every frame on this: checking the exception
		// handler is costly. Kuddos to Kathrine Jansma for pointing this
		// hot spot in the VS profiler. HB
		return true;
	}
	last_check = gFrameTimeSeconds;
	return LLWinDebug::checkExceptionHandler();
}

void LLAppViewerWin32::handleSyncCrashTrace()
{
	// Free our reserved memory space before dumping the stack trace (it should
	// already be freed at this point, but it does not hurt calling this method
	// twice).
	LLMemory::cleanupClass();
}

//virtual
bool LLAppViewerWin32::sendURLToOtherInstance(const std::string& url)
{
	wchar_t window_class[256];    // Assume max length < 255 chars.
	// Use the default window class name to for all Second Life viewers to
	// find any running session of any viewer.
	mbstowcs(window_class, "Second Life", 255);
	window_class[255] = 0;
	HWND other_window = FindWindow(window_class, NULL);
	if (other_window)
	{
		LL_DEBUGS("AppInit") << "Found other window with the class name 'Second Life'"
							 << LL_ENDL;
		COPYDATASTRUCT cds;
		constexpr S32 SLURL_MESSAGE_TYPE = 0;
		cds.dwData = SLURL_MESSAGE_TYPE;
		cds.cbData = url.length() + 1;
		cds.lpData = (void*)url.c_str();

		LRESULT msg_result = SendMessage(other_window, WM_COPYDATA, NULL,
										 (LPARAM)&cds);
		LL_DEBUGS("AppInit") << "SendMessage(WM_COPYDATA) to other window 'Second Life' returned "
							 << msg_result << LL_ENDL;
		return true;
	}
	return false;
}

std::string LLAppViewerWin32::generateSerialNumber()
{
	char serial_md5[MD5HEX_STR_SIZE];
	serial_md5[0] = 0;

	DWORD serial = 0;
	DWORD flags = 0;
	if (GetVolumeInformation(L"C:\\",
							 NULL,		// Volume name buffer
							 0,			// Volume name buffer size
							 &serial,	// Volume serial
							 NULL,		// Max component length
							 &flags,	// File system flags
							 NULL,		// File system name buffer
							 0))		// File system name buffer size
	{
		LLMD5 md5;
		md5.update((unsigned char*)&serial, sizeof(DWORD));
		md5.finalize();
		md5.hex_digest(serial_md5);
	}
	else
	{
		llwarns << "GetVolumeInformation failed" << llendl;
	}

	return serial_md5;
}

///////////////////////////////////////////////////////////////////////////////
// Vulkan detection used by llviewerstats.cpp
///////////////////////////////////////////////////////////////////////////////

// Minimal Vulkan API defines to avoid having to #include <vulkan/vulkan.h>
#if defined(_WIN32)
# define VKAPI_ATTR
# define VKAPI_CALL __stdcall
# define VKAPI_PTR VKAPI_CALL
#else
# define VKAPI_ATTR
# define VKAPI_CALL
# define VKAPI_PTR
#endif

#define VK_API_VERSION_MAJOR(v)		((U32(v) >> 22)	& 0x07FU)	// 7 bits
#define VK_API_VERSION_MINOR(v)		((U32(v) >> 12)	& 0x3FFU)	// 10 bits
#define VK_API_VERSION_PATCH(v)		(U32(v)			& 0xFFFU)	// 12 bits
#define VK_API_VERSION_VARIANT(v)	((U32(v) >> 29)	& 0x007U)	// 3 bits

// NOTE: variant is first parameter !  This is to match vulkan/vulkan_core.h
#define VK_MAKE_API_VERSION(variant, major, minor, patch) (0 | \
		((U32(major) & 0x07FU) << 22) | ((U32(minor) & 0x3FFU) << 12) | \
		(U32(patch) << 12) | ((U32(variant) & 0x007U) << 29))

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;

// Types
VK_DEFINE_HANDLE(VkInstance);

typedef enum VkResult
{
	VK_SUCCESS = 0,
	VK_RESULT_MAX_ENUM = 0x7FFFFFFF
} VkResult;

// Prototypes
typedef void (VKAPI_PTR* PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (VKAPI_PTR* PFN_vkGetInstanceProcAddr)(VkInstance instance,
																 const char* namep);
typedef VkResult (VKAPI_PTR* PFN_vkEnumerateInstanceVersion)(U32* versionp);

//virtual
bool LLAppViewerWin32::probeVulkan(std::string& version)
{
	static std::string vk_api_version;
	static S32 has_vulkan = -1;	// -1 = not yet probed
	if (has_vulkan == -1)
	{
		has_vulkan = 0;	// Default to no Vulkan support
		// Probe for Vulkan capability (Dave Houlton 05/2020)
		// Check for presense of a Vulkan loader DLL, as a proxy for a
		// Vulkan-capable GPU. Gives a good approximation of Vulkan capability
		// within current user systems from this.
		if (HMODULE vulkan_loader = LoadLibraryA("vulkan-1.dll"))
		{
			has_vulkan = 1;
			// We have at least 1.0.
			// See the note about vkEnumerateInstanceVersion() below.
			vk_api_version = "1.0";
			PFN_vkGetInstanceProcAddr pGetInstanceProcAddr =
				(PFN_vkGetInstanceProcAddr)GetProcAddress(vulkan_loader,
														  "vkGetInstanceProcAddr");
			if (pGetInstanceProcAddr)
			{
				// Check for vkEnumerateInstanceVersion. If it exists then we
				// have at least 1.1 and can query the max API version.
				// NOTE: Each VkPhysicalDevice that supports Vulkan has its own
				// VkPhysicalDeviceProperties.apiVersion which is separate from
				// the max API version !  See:
				// https://www.lunarg.com/wp-content/uploads/2019/02/
				//  Vulkan-1.1-Compatibility-Statement_01_19.pdf
				PFN_vkEnumerateInstanceVersion pEnumerateInstanceVersion =
					(PFN_vkEnumerateInstanceVersion)pGetInstanceProcAddr(NULL,
																		 "vkEnumerateInstanceVersion");
				if (pEnumerateInstanceVersion)
				{
					U32 version = VK_MAKE_API_VERSION(0, 1, 1, 0);
					VkResult status = pEnumerateInstanceVersion(&version);
					if (status != VK_SUCCESS)
					{
						llinfos << "Failed to get Vulkan version. Assuming v1.0."
								<< llendl;
					}
					else
					{
						S32 major = VK_API_VERSION_MAJOR(version);
						S32 minor = VK_API_VERSION_MINOR(version);
						S32 patch = VK_API_VERSION_PATCH(version);
						S32 variant = VK_API_VERSION_VARIANT(version);
						vk_api_version = llformat("%d.%d.%d.%d", major, minor,
												  patch, variant);
					}
				}
			}
			else
			{
				llwarns << "Failed to get Vulkan vkGetInstanceProcAddr()"
						<< llendl;
			}
			FreeLibrary(vulkan_loader);
		}
	}
	version = vk_api_version;
	return has_vulkan == 1;
}
