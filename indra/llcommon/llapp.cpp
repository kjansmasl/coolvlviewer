/**
 * @file llapp.cpp
 * @brief Implementation of the LLApp class.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#if !LL_WINDOWS
# include <sys/wait.h>			// For waitpid()
#endif

#include "llapp.h"

#include "llcommon.h"
#include "llerrorcontrol.h"
#include "lleventtimer.h"
#include "llevents.h"
#include "llframetimer.h"
#include "llmemory.h"
#include "llthread.h"

///////////////////////////////////////////////////////////////////////////////
// LLErrorThread class declaration (used to be in llerrorthread.h, but only
// used by and with LLApp, so...)
///////////////////////////////////////////////////////////////////////////////

class LLErrorThread : public LLThread
{
protected:
	LOG_CLASS(LLErrorThread);

public:
	LLErrorThread();

	void run() override;

	void setUserData(void* user_data);
	void* getUserData() const;

protected:
	void* mUserDatap; // User data associated with this thread
};

///////////////////////////////////////////////////////////////////////////////
// Signal handling
///////////////////////////////////////////////////////////////////////////////

#if LL_WINDOWS

// Windows uses structured exceptions, so it is handled a bit differently.
LONG WINAPI default_windows_exception_handler(struct _EXCEPTION_POINTERS* e);
BOOL ConsoleCtrlHandler(DWORD fdw_ctrl_type);

#else

# include <signal.h>
# include <unistd.h> // for fork()
void setup_signals();
void default_unix_signal_handler(int signum, siginfo_t* info, void*);

# if LL_DARWIN

// OS-X does not support SIGRT*
const S32 LL_SMACKDOWN_SIGNAL = SIGUSR1;
const S32 LL_HEARTBEAT_SIGNAL = SIGUSR2;

# else // Linux or (assumed) other similar unixoid

// Do not catch SIGCHLD in our base application class for the viewer: some of
// our 3rd party libs may need their *own* SIGCHLD handler to work. The viewer
// does not need to catch SIGCHLD anyway.
#define LL_IGNORE_SIGCHLD 1

// We want reliable delivery of our signals: SIGRT* is it. Old LinuxThreads
// versions eat SIGRTMIN+0 to SIGRTMIN+2, avoid those. Note that SIGRTMIN/
// SIGRTMAX may expand to a glibc function call with a non-constant result so
// these are not consts and cannot be used in constant expressions. SIGRTMAX
// may return -1 on rare broken setups.
const S32 LL_SMACKDOWN_SIGNAL = SIGRTMAX >= 0 ? SIGRTMAX - 1 : SIGUSR1;
const S32 LL_HEARTBEAT_SIGNAL = SIGRTMAX >= 0 ? SIGRTMAX : SIGUSR2;

# endif // LL_DARWIN

#endif // LL_WINDOWS

///////////////////////////////////////////////////////////////////////////////
// LLApp class proper
///////////////////////////////////////////////////////////////////////////////

// Static variables

// The static application instance
LLApp* LLApp::sApplication = NULL;

// Local flag for whether or not to do logging in signal handlers.
bool LLApp::sLogInSignal = false;

// Keeps track of application status:
LLAtomicS32 LLApp::sStatus(LLApp::APP_STATUS_STOPPED);
LLAppErrorHandler LLApp::sErrorHandler = NULL;
LLAppErrorHandler LLApp::sSyncErrorHandler = NULL;
bool LLApp::sErrorThreadRunning = false;
#if !LL_WINDOWS
LLApp::child_map LLApp::sChildMap;
LLAtomicU32* LLApp::sSigChildCount = NULL;
LLAppChildCallback LLApp::sDefaultChildCallback = NULL;
#endif

LLApp::LLApp()
:	mThreadErrorp(NULL)
{
	assert_main_thread();		// Make sure we record the main thread
	commonCtor();
	startErrorThread();
}

void LLApp::commonCtor()
{
	// Set our status to running
	setStatus(APP_STATUS_RUNNING);

	LLCommon::initClass();

#if !LL_WINDOWS
	// This must be initialized before the error handler.
	sSigChildCount = new LLAtomicU32(0);
#endif

	// Setup error handling
	setupErrorHandling();

	// Initialize the options structure. We need to make this an array because
	// the structured data will not auto-allocate if we reference an invalid
	// location with the [] operator.
	mOptions = LLSD::emptyArray();
	LLSD sd;
	for (int i = 0; i < PRIORITY_COUNT; ++i)
	{
		mOptions.append(sd);
	}

	// Set the application to this instance.
	sApplication = this;
}

LLApp::LLApp(LLErrorThread* error_thread)
:	mThreadErrorp(error_thread)
{
	commonCtor();
}

LLApp::~LLApp()
{
#if !LL_WINDOWS
	delete sSigChildCount;
	sSigChildCount = NULL;
#endif
	setStopped();

	// *HACK: wait for the error thread to clean itself
	ms_sleep(100);

	if (mThreadErrorp)
	{
		delete mThreadErrorp;
		mThreadErrorp = NULL;
	}

	LLCommon::cleanupClass();
}

LLSD LLApp::getOption(const std::string& name) const
{
	LLSD rv;
	for (LLSD::array_const_iterator iter = mOptions.beginArray(),
									end = mOptions.endArray();
		 iter != end; ++iter)
	{
		rv = (*iter)[name];
		if (rv.isDefined())
		{
			break;
		}
	}
	return rv;
}

bool LLApp::parseCommandOptions(int argc, char** argv)
{
	LLSD commands;
	std::string name;
	std::string value;
	for (S32 ii = 1; ii < argc; ++ii)
	{
		if (argv[ii][0] != '-')
		{
			llinfos << "Did not find option identifier while parsing token: "
					<< argv[ii] << llendl;
			return false;
		}
		int offset = 1;
		if (argv[ii][1] == '-')
		{
			++offset;
		}
		name.assign(&argv[ii][offset]);
		if (ii + 1 >= argc || argv[ii + 1][0] == '-')
		{
			// we found another option after this one or we have
			// reached the end. simply record that this option was
			// found and continue.
			int flag = name.compare("logfile");
			if (0 == flag)
			{
				commands[name] = "log";
			}
			else
			{
				commands[name] = true;
			}

			continue;
		}
		value.assign(argv[++ii]);
#if LL_WINDOWS
		// Windows changed command line parsing. Deal with it.
		S32 slen = value.length() - 1;
		S32 start = 0;
		S32 end = slen;
		if (argv[ii][start] == '"')
		{
			++start;
		}
		if (argv[ii][end] == '"')
		{
			--end;
		}
		if (start != 0 || end != slen)
		{
			value = value.substr(start, end);
		}
#endif
		commands[name] = value;
	}
	setOptionData(PRIORITY_COMMAND_LINE, commands);
	return true;
}

bool LLApp::setOptionData(OptionPriority level, LLSD data)
{
	if (level < 0 || level >= PRIORITY_COUNT || data.type() != LLSD::TypeMap)
	{
		return false;
	}
	mOptions[level] = data;
	return true;
}

LLSD LLApp::getOptionData(OptionPriority level)
{
	if (level < 0 || level >= PRIORITY_COUNT)
	{
		return LLSD();
	}
	return mOptions[level];
}

void LLApp::stepFrame()
{
	LLFrameTimer::stepFrame();
	LLEventTimer::stepFrame();
}

#if LL_WINDOWS
// The following code is needed for 32-bit apps on 64-bit windows to keep it
// from eating crashes. It is a lovely undocumented 'feature' in SP1 of
// Windows 7. An excellent in-depth article on the issue may be found here:
// http://randomascii.wordpress.com/2012/07/05/when-even-crashing-doesnt-work/
void EnableCrashingOnCrashes()
{
	typedef BOOL (WINAPI* tGetPolicy)(LPDWORD lpFlags);
	typedef BOOL (WINAPI* tSetPolicy)(DWORD dwFlags);
	const DWORD EXCEPTION_SWALLOWING = 0x1;

	HMODULE kernel32 = LoadLibraryA("kernel32.dll");
	tGetPolicy pGetPolicy =
		(tGetPolicy)GetProcAddress(kernel32,
								   "GetProcessUserModeExceptionPolicy");
	tSetPolicy pSetPolicy =
		(tSetPolicy)GetProcAddress(kernel32,
								   "SetProcessUserModeExceptionPolicy");
	if (pGetPolicy && pSetPolicy)
	{
		DWORD dwFlags;
		if (pGetPolicy(&dwFlags))
		{
			// Turn off the filter
			pSetPolicy(dwFlags & ~EXCEPTION_SWALLOWING);
		}
	}
}
#endif

// Error handling is done by starting up an error handling thread, which just
// sleeps and occasionally checks to see if the app is in an error state, and
// sees if it needs to be run.
void LLApp::setupErrorHandling()
{
#if LL_WINDOWS
	// Windows does not have the same signal handling mechanisms as UNIX. What
	// we do is install an unhandled exception handler, which will try to do
	// the right thing in the case of an error (generate a minidump).

	EnableCrashingOnCrashes();

	// This sets a callback to handle w32 signals to the console window. The
	// viewer should not be affected, since its a windowed app.
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleCtrlHandler, TRUE);
#else
	// Start up signal handling.
	//
	// There are two different classes of signals. Synchronous signals are
	// delivered to a specific thread, asynchronous signals can be delivered to
	// any thread (in theory)
	setup_signals();
#endif
}

// Starts the error handling thread, which is responsible for taking action
// when the app goes into the APP_STATUS_ERROR state
void LLApp::startErrorThread()
{
	llinfos << "Starting error thread" << llendl;
	mThreadErrorp = new LLErrorThread();
	mThreadErrorp->setUserData((void*)this);
	mThreadErrorp->start();
}

//static
void LLApp::runSyncErrorHandler()
{
	if (sSyncErrorHandler)
	{
		sSyncErrorHandler();
	}
}

//static
void LLApp::runErrorHandler()
{
	if (sErrorHandler)
	{
		sErrorHandler();
	}
	setStatus(APP_STATUS_STOPPED);
}

//static
void LLApp::setError()
{
	if (!isError())
	{
		// Perform any needed synchronous error-handling
		runSyncErrorHandler();
		// Set app status to ERROR so that the LLErrorThread notices
		setStatus(APP_STATUS_ERROR);
	}
}

//static
void LLApp::setQuitting()
{
	if (!isExiting())
	{
		// If we are already exiting, we do not want to reset our state back to
		// quitting.
		llinfos << "Setting app state to QUITTING" << llendl;
		setStatus(APP_STATUS_QUITTING);
	}
}

//static
void LLApp::setStatus(S32 status)
{
	sStatus = status;
	// This can also happen very late in the application lifecycle; do not
	// resurrect a deleted LLSingleton...
	if (LLEventPumps::destroyed())
	{
		return;
	}

	// Notify interested parties of status change
	LLSD value;
	switch (status)
	{
		case APP_STATUS_STOPPED:
			value = LLSD::String("stopped");
			break;

		case APP_STATUS_RUNNING:
			value = LLSD::String("running");
			break;

		case APP_STATUS_QUITTING:
			value = LLSD::String("quitting");
			break;

		case APP_STATUS_ERROR:
			value = LLSD::String("error");
			break;

		default:
			value = LLSD::Integer(status);
	}
	LLSD data;
	data["status"] = value;
	gEventPumps.obtain("LLApp").post(data);
}

#if !LL_WINDOWS
//static
U32 LLApp::getSigChildCount()
{
	if (sSigChildCount)
	{
		return U32(*sSigChildCount);
	}
	return 0;
}

//static
void LLApp::incSigChildCount()
{
	if (sSigChildCount)
	{
		++(*sSigChildCount);
	}
}

#endif

#if LL_WINDOWS

// Translates the signals/exceptions into cross-platform stuff Windows
// implementation
LONG WINAPI default_windows_exception_handler(struct _EXCEPTION_POINTERS*)
{
	// Make sure the user sees something to indicate that the app crashed.
	LONG retval = EXCEPTION_EXECUTE_HANDLER;

	if (LLApp::isError())
	{
		llwarns << "Got another fatal signal while in the error handler, die now !"
				<< llendl;
		return retval;
	}

	// Flag status to error, so thread_error starts its work
	LLApp::setError();

	// Block in the exception handler until the app has stopped. This is pretty
	// sketchy, but appears to work just fine
	while (!LLApp::isStopped())
	{
		ms_sleep(10);
	}

	// *TODO: generate a minidump if we can. This needs to be ported over from
	// the viewer-specific LLWinDebug class

	// At this point, we always want to exit the app. There is no graceful
	// recovery for an unhandled exception. Just kill the process.
	return retval;
}

// Win32 does not support signals. This is used instead.
BOOL ConsoleCtrlHandler(DWORD fdw_ctrl_type)
{
	switch (fdw_ctrl_type)
	{
		// For these, just set our state to quitting, not error
 		case CTRL_BREAK_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
		case CTRL_CLOSE_EVENT:	// From end task or the window close button.
		case CTRL_C_EVENT:		// from CTRL-C on the keyboard
			if (LLApp::isExiting())
			{
				// We are already trying to die, just ignore this signal
				if (LLApp::sLogInSignal)
				{
					llinfos << "Signal handler - Already trying to quit, ignoring signal !"
							<< llendl;
				}
				return TRUE;
			}
			LLApp::setQuitting();
			return TRUE;

		default:
			return FALSE;
	}
}

#else

void LLApp::setChildCallback(pid_t pid, LLAppChildCallback callback)
{
	LLChildInfo child_info;
	child_info.mCallback = callback;
	LLApp::sChildMap[pid] = child_info;
}

void LLApp::setDefaultChildCallback(LLAppChildCallback callback)
{
	LLApp::sDefaultChildCallback = callback;
}

pid_t LLApp::fork()
{
	fflush(NULL);		// flush all buffers before the child inherits them
	pid_t pid = ::fork();
	if (pid < 0)
	{
		int system_error = errno;
		llwarns << "Unable to fork !  Operating system error code: "
				<< system_error << llendl;
	}
	else if (pid == 0)
	{
		// Sleep a bit to allow the parent to set up child callbacks.
		ms_sleep(10);

		// We need to disable signal handling, because we don't have a signal
		// handling thread anymore.
		setupErrorHandling();
	}
	else
	{
		llinfos << "Forked child process " << pid << llendl;
	}
	return pid;
}

void setup_signals()
{
	// Set up signal handlers that may result in program termination
	struct sigaction act;
	act.sa_sigaction = default_unix_signal_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	// Synchronous signals
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGSYS, &act, NULL);

	sigaction(LL_HEARTBEAT_SIGNAL, &act, NULL);
	sigaction(LL_SMACKDOWN_SIGNAL, &act, NULL);

	// Asynchronous signals that are normally ignored
#ifndef LL_IGNORE_SIGCHLD
	sigaction(SIGCHLD, &act, NULL);
#endif // LL_IGNORE_SIGCHLD
	sigaction(SIGUSR2, &act, NULL);

	// Asynchronous signals that result in attempted graceful exit
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	// Asynchronous signals that result in core
	sigaction(SIGQUIT, &act, NULL);
}

void clear_signals()
{
	struct sigaction act;
	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	// Synchronous signals
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGSYS, &act, NULL);

	sigaction(LL_HEARTBEAT_SIGNAL, &act, NULL);
	sigaction(LL_SMACKDOWN_SIGNAL, &act, NULL);

	// Asynchronous signals that are normally ignored
#ifndef LL_IGNORE_SIGCHLD
	sigaction(SIGCHLD, &act, NULL);
#endif // LL_IGNORE_SIGCHLD

	// Asynchronous signals that result in attempted graceful exit
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	// Asynchronous signals that result in core
	sigaction(SIGUSR2, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
}

// Unix implementation of synchronous signal handler. This runs in the thread
// that threw the signal. We do the somewhat sketchy operation of blocking in
// here until the error handler has gracefully stopped the app.
void default_unix_signal_handler(int signum, siginfo_t* info, void*)
{
	if (LLApp::sLogInSignal)
	{
		llinfos << "Signal handler - Got signal " << signum << llendl;
	}

	switch (signum)
	{
		case SIGCHLD:
			if (LLApp::sLogInSignal)
			{
				llinfos << "Signal handler - Got SIGCHLD from " << info->si_pid
						<< llendl;
			}

			// Check result code for all childs for which we have registered
			// callbacks THIS WILL NOT WORK IF SIGCHLD IS SENT without killing
			// the child.
			// *TODO: now that we are using SIGACTION, we could actually
			// implement the launcher behavior to determine who sent the
			// SIGCHLD even if it does not result in child termination
			if (LLApp::sChildMap.count(info->si_pid))
			{
				LLApp::sChildMap[info->si_pid].mGotSigChild = true;
			}

			LLApp::incSigChildCount();
			return;

		case SIGABRT:
			// Abort just results in termination of the app, no funky error
			// handling.
			if (LLApp::sLogInSignal)
			{
				llwarns << "Signal handler - Got SIGABRT, terminating"
						<< llendl;
			}
			clear_signals();
			raise(signum);
			return;

		case SIGINT:
		case SIGHUP:
		case SIGTERM:
			if (LLApp::sLogInSignal)
			{
				llwarns << "Signal handler - Got SIGINT, HUP, or TERM, exiting gracefully"
						<< llendl;
			}
			// Graceful exit... Just set our state to quitting, not error.
			if (LLApp::isExiting())
			{
				// We are already trying to die, just ignore this signal
				if (LLApp::sLogInSignal)
				{
					llinfos << "Signal handler - Already trying to quit, ignoring signal !"
							<< llendl;
				}
				return;
			}
			LLApp::setQuitting();
			return;

		case SIGALRM:
		case SIGPIPE:
		case SIGUSR2:
		default:
			if (signum == LL_SMACKDOWN_SIGNAL || signum == SIGBUS ||
		    	signum == SIGILL || signum == SIGFPE || signum == SIGSEGV ||
				signum == SIGQUIT)
			{
				if (signum == LL_SMACKDOWN_SIGNAL)
				{
					// Smackdown treated just like any other app termination,
					// for now
					if (LLApp::sLogInSignal)
					{
						llwarns << "Signal handler - Handling smackdown signal !"
								<< llendl;
					}
					else
					{
						// Do not log anything, even errors: this is because
						// this signal could happen anywhere.
						LLError::setDefaultLevel(LLError::LEVEL_NONE);
					}

					// Change the signal that we re-raise to SIGABRT, so we
					// generate a core dump.
					signum = SIGABRT;
				}

				if (LLApp::sLogInSignal)
				{
					llwarns << "Signal handler - Handling fatal signal !"
							<< llendl;
				}
				if (LLApp::isError())
				{
					// Received second fatal signal while handling first, just
					// die right now. Set the signal handlers back to default
					// before handling the signal: this makes the next signal
					// wipe out the app.
					clear_signals();

					if (LLApp::sLogInSignal)
					{
						llwarns << "Signal handler - Got another fatal signal while in the error handler, die now !"
								<< llendl;
					}
					raise(signum);
					return;
				}

				if (LLApp::sLogInSignal)
				{
					llwarns << "Signal handler - Flagging error status and waiting for shutdown"
							<< llendl;
				}

				// Flag status to ERROR, so thread_error does its work.
				LLApp::setError();
				// Block in the signal handler until somebody says that we are done.
				while (LLApp::sErrorThreadRunning && !LLApp::isStopped())
				{
					ms_sleep(10);
				}

				if (LLApp::sLogInSignal)
				{
					llwarns << "Signal handler - App is stopped, reraising signal"
							<< llendl;
				}
				clear_signals();
				raise(signum);
				return;
		}
		else if (LLApp::sLogInSignal)
		{
			llinfos << "Signal handler - Unhandled signal " << signum
					<< ", ignoring !" << llendl;
		}
	}
}

#endif

///////////////////////////////////////////////////////////////////////////////
// LLErrorThread class (used to be in llerrorthread.cpp, but only used by and
// with LLApp, so...)
///////////////////////////////////////////////////////////////////////////////

LLErrorThread::LLErrorThread()
:	LLThread("Error"),
	mUserDatap(NULL)
{
}

void LLErrorThread::setUserData(void* user_data)
{
	mUserDatap = user_data;
}

void* LLErrorThread::getUserData() const
{
	return mUserDatap;
}

#if !LL_WINDOWS
//
// Various signal/error handling functions that can't be put into the class
//
void get_child_status(const int waitpid_status, int& process_status,
					  bool& exited, bool do_logging)
{
	exited = false;
	process_status = -1;
	// The child process exited.  Call its callback, and then clean it up
	if (WIFEXITED(waitpid_status))
	{
		process_status = WEXITSTATUS(waitpid_status);
		exited = true;
		if (do_logging)
		{
			llinfos << "get_child_status - Child exited cleanly with return of "
					<< process_status << llendl;
		}
		return;
	}
	else if (WIFSIGNALED(waitpid_status))
	{
		process_status = WTERMSIG(waitpid_status);
		exited = true;
		if (do_logging)
		{
			llinfos << "get_child_status - Child died because of uncaught signal "
					<< process_status << llendl;
#ifdef WCOREDUMP
			if (WCOREDUMP(waitpid_status))
			{
				llinfos << "get_child_status - Child dumped core" << llendl;
			}
			else
			{
				llinfos << "get_child_status - Child didn't dump core"
						<< llendl;
			}
#endif
		}
		return;
	}
	else if (do_logging)
	{
		// This is weird.  I just dump the waitpid status into the status code,
		// not that there's any way of telling what it is...
		llinfos << "get_child_status - Got SIGCHILD but child didn't exit"
				<< llendl;
		process_status = waitpid_status;
	}

}
#endif

void LLErrorThread::run()
{
	LLApp::sErrorThreadRunning = true;
	// This thread sits and waits for the sole purpose
	// of waiting for the signal/exception handlers to flag the
	// application state as APP_STATUS_ERROR.
	llinfos << "thread_error - Waiting for an error" << llendl;

#if !LL_WINDOWS
	U32 last_sig_child_count = 0;
#endif
	while (true)
	{
		if (LLApp::isError() || LLApp::isStopped())
		{
			// The application has stopped running, time to take action (maybe)
			break;
		}
#if !LL_WINDOWS
		// Check whether or not the main thread had a sig child we have not
		// handled.
		U32 current_sig_child_count = LLApp::getSigChildCount();
		if (last_sig_child_count != current_sig_child_count)
		{
			int status = 0;
			pid_t child_pid = 0;
			last_sig_child_count = current_sig_child_count;
			if (LLApp::sLogInSignal)
			{
				llinfos << "thread_error handling SIGCHLD #"
						<< current_sig_child_count << llendl;
			}
			for (LLApp::child_map::iterator iter = LLApp::sChildMap.begin();
				 iter != LLApp::sChildMap.end(); )
			{
				child_pid = iter->first;
				LLChildInfo &child_info = iter->second;
				// Check the status of *all* children, in case we missed a
				// signal
				if (waitpid(child_pid, &status, WNOHANG) != 0)
				{
					bool exited = false;
					int exit_status = -1;
					get_child_status(status, exit_status, exited,
									 LLApp::sLogInSignal);
					if (child_info.mCallback)
					{
						if (LLApp::sLogInSignal)
						{
							llinfos << "Signal handler - Running child callback"
									<< llendl;
						}
						child_info.mCallback(child_pid, exited, status);
					}
					LLApp::sChildMap.erase(iter++);
				}
				else
				{
					// Child did not terminate, yet we got a sigchild somewhere
					if (child_info.mGotSigChild && child_info.mCallback)
					{
						child_info.mCallback(child_pid, false, 0);
					}
					child_info.mGotSigChild = false;
					++iter;
				}
			}

			// Check the status of *all* children, in case we missed a signal
			// Same as above, but use the default child callback
			while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0)
			{
				if (waitpid(child_pid, &status, WNOHANG) != 0)
				{
					bool exited = false;
					int exit_status = -1;
					get_child_status(status, exit_status, exited, LLApp::sLogInSignal);
					if (LLApp::sDefaultChildCallback)
					{
						if (LLApp::sLogInSignal)
						{
							llinfos << "Signal handler - Running default child callback"
									<< llendl;
						}
						LLApp::sDefaultChildCallback(child_pid, true, status);
					}
				}
			}
		}
#endif
		ms_sleep(10);
	}
	if (LLApp::isError())
	{
		// The app is in an error state, run the application's error handler.
#if 0
		llinfos << "thread_error - An error has occurred, running error callback !"
				<< llendl;
#endif
		// Run the error handling callback
		LLApp::runErrorHandler();
	}
#if 0
	else
	{
		// Everything is okay, a clean exit.
		llinfos << "thread_error - Application exited cleanly" << llendl;
	}

	llinfos << "thread_error - Exiting" << llendl;
#endif

	LLApp::sErrorThreadRunning = false;
}
