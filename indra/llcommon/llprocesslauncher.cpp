/**
 * @file llprocesslauncher.cpp
 * @brief Utility class for launching, terminating, and tracking the state of
 *        processes.
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

#include "llprocesslauncher.h"

#if LL_DARWIN || LL_LINUX
# include <sys/wait.h>
# include <unistd.h>
# include <signal.h>
# include <fcntl.h>
# include <errno.h>
#endif

// Sadly, some gcc/glibc "flavours" (Ubuntu's ones, apparently), warn about
// unused results for (void)chdir() and (void)fchdir(), even though the (void)
// is there to prevent this !  So, let's take more radical measures...
#if LL_LINUX && defined(__GNUC__)
# pragma GCC diagnostic ignored "-Wunused-result"
#endif

LLProcessLauncher::LLProcessLauncher()
{
#if LL_WINDOWS
	mProcessHandle = 0;
#else
	mProcessID = 0;
#endif
}

LLProcessLauncher::~LLProcessLauncher()
{
	kill();
}

#if LL_WINDOWS

S32 LLProcessLauncher::launch()
{
	// If there was already a process associated with this object, kill it.
	kill();
	orphan();

	S32 result = 0;

	PROCESS_INFORMATION pinfo;
	STARTUPINFOA sinfo;
	memset(&sinfo, 0, sizeof(sinfo));

	std::string args = "\"" + mExecutable + "\"";
	for (S32 i = 0, count = mLaunchArguments.size(); i < count; ++i)
	{
		args += ' ';
		const std::string& arg = mLaunchArguments[i];
		if (arg.find(' ') != std::string::npos)
		{
			// Quote arguments containing spaces !
			args += "\""  + arg + "\"";
		}
		else
		{
			args += arg;
		}
	}
	llinfos << "Executable: " << mExecutable << " arguments: " << args
			<< llendl;

	// So retarded. Windows requires that the second parameter to
	// CreateProcessA be a writable (non-const) string...
	char* args2 = new char[args.size() + 1];
	strcpy(args2, args.c_str());

	if (!CreateProcessA(NULL, args2, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo,
						&pinfo))
	{
		// *TODO: do better than returning the OS-specific error code on
		// failure...
		result = GetLastError();
		if (result == 0)
		{
			// Make absolutely certain we return a non-zero value on failure.
			result = -1;
		}
	}
	else
	{
		// foo = pinfo.dwProcessId; // Get your pid here if you want to use it
		// later on
		mProcessHandle = pinfo.hProcess;
		CloseHandle(pinfo.hThread);	// Stops leaks, nothing else
	}

	delete[] args2;

	return result;
}

bool LLProcessLauncher::isRunning()
{
	if (mProcessHandle)
	{
		if (WaitForSingleObject(mProcessHandle, 0) == WAIT_OBJECT_0)
		{
			// The process has completed.
			mProcessHandle = 0;
		}
	}

	return mProcessHandle != 0;
}
bool LLProcessLauncher::kill()
{
	if (mProcessHandle)
	{
		TerminateProcess(mProcessHandle, 0);

		if (isRunning())
		{
			return false;
		}
	}
	return true;
}

void LLProcessLauncher::orphan()
{
	// Forget about the process
	mProcessHandle = 0;
}

//static
void LLProcessLauncher::reap()
{
	// No actions necessary on Windows.
}

#else // Linux and macOS

static std::list<pid_t> sZombies;

// Attempts to reap a process Id. Returns true if the process has exited and has
// been reaped, false otherwise.
static bool reap_pid(pid_t pid)
{
	bool result = false;

	pid_t wait_result = ::waitpid(pid, NULL, WNOHANG);
	if (wait_result == pid)
	{
		result = true;
	}
	else if (wait_result == -1)
	{
		if (errno == ECHILD)
		{
			// No such process: this may mean we are ignoring SIGCHILD.
			result = true;
		}
	}

	return result;
}

S32 LLProcessLauncher::launch()
{
	// If there was already a process associated with this object, kill it.
	kill();
	orphan();

	// Create an argv vector for the child process (size + 1 for the executable
	// path and + 1 for the NULL terminator)
	const char** fake_argv = new const char *[mLaunchArguments.size() + 2];
	S32 i = 0;
	// Add the executable path
	fake_argv[i++] = mExecutable.c_str();
	// And any arguments
	for (U32 j = 0, count = mLaunchArguments.size(); j < count; ++j)
	{
		fake_argv[i++] = mLaunchArguments[j].c_str();
	}
	// Terminate with a null pointer
	fake_argv[i] = NULL;

	int current_wd = -1;
	if (!mWorkingDir.empty())
	{
		// Save the current working directory
		current_wd = open(".", O_RDONLY);

		// And change to the one the child will be executed in
		(void)chdir(mWorkingDir.c_str());
	}

 	// Flush all buffers before the child inherits them
 	fflush(NULL);

	pid_t id = fork();
	if (id == 0)
	{
		// Child process code path
		execv(mExecutable.c_str(), (char* const*)fake_argv);

		// If we reach this point, the exec failed. Use _exit() instead of
		// exit() per the vfork man page.
		_exit(0);
	}

	// Parent process code path
	if (current_wd >= 0)
	{
		// Restore the previous working directory
		(void)fchdir(current_wd);
		(void)close(current_wd);
	}

	delete[] fake_argv;

	mProcessID = id;

	// At this point, the child process will have been created (since that is
	// how vfork works: the child borrowed our execution context until it
	// forked). If the process does not exist at this point, the exec failed.
	if (!isRunning())
	{
		llwarns << "Failed to exec: " << mExecutable << llendl;
		return -1;
	}

	LL_DEBUGS("ProcessLauncher") << "Successfully launched: " << mExecutable
								 << " - pid = " << mProcessID << LL_ENDL;
	return 0;
}

bool LLProcessLauncher::isRunning()
{
	if (mProcessID)
	{
		LL_DEBUGS("ProcessLauncher") << "Testing status of: " << mExecutable
									 << " - pid = " << mProcessID << LL_ENDL;
		// Check whether the process has exited, and reap it if it has.
		if (reap_pid(mProcessID))
		{
			// The process has exited.
			mProcessID = 0;
		}
	}

	LL_DEBUGS("ProcessLauncher") << "Process for " << mExecutable << " is "
								 << (mProcessID ? "running" : "terminated")
								 << LL_ENDL;

	return mProcessID != 0;
}

bool LLProcessLauncher::kill()
{
	if (mProcessID)
	{
		// Try to kill the process. We will do approximately the same thing
		// whether the kill returns an error or not, so we ignore the result.
		(void)::kill(mProcessID, SIGTERM);

		// This will have the side-effect of reaping the zombie if the process
		// has exited.
		if (isRunning())
		{
			return false;
		}
	}
	return true;
}

void LLProcessLauncher::orphan()
{
	// Disassociate the process from this object
	if (mProcessID)
	{
		// We may still need to reap the process's zombie eventually
		sZombies.push_back(mProcessID);
		mProcessID = 0;
	}
}

//static
void LLProcessLauncher::reap()
{
	// Attempt to real all saved process ID's.
	std::list<pid_t>::iterator iter = sZombies.begin();
	while (iter != sZombies.end())
	{
		if (reap_pid(*iter))
		{
			iter = sZombies.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

#endif // Linux and macOS
