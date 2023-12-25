/**
 * @file llthread.cpp
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#if LL_WINDOWS
# include <stdexcept>
#endif

#include "boost/container_hash/hash.hpp"

#include "llthread.h"

#include "lltimer.h"
#include "hbtracy.h"
#include "llsys.h"

#if TRACY_ENABLE
std::list<std::string> gTracyThreadNames;
LLMutex gTracyThreadNamesLock;
#endif

#ifdef LL_WINDOWS
constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

# pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType;		// Must be 0x1000.
	LPCSTR szName;		// Pointer to name (in user addr space).
	DWORD dwThreadID;	// Thread ID (-1=caller thread).
	DWORD dwFlags;		// Reserved for future use, must be zero.
} THREADNAME_INFO;
# pragma pack(pop)

void set_thread_name(DWORD thread_id, const char* thread_name)
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = thread_name;
	info.dwThreadID = thread_id;
	info.dwFlags = 0;

	__try
	{
		::RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(DWORD),
						 (ULONG_PTR*)&info);
	}
	__except(EXCEPTION_CONTINUE_EXECUTION)
	{
	}
}
#endif

// Caching the current thread Id in a thread_local variable for speed... HB
static thread_local LLThread::id_t tThreadId = boost::this_thread::get_id();

static LLThread::id_t get_main_thread_id()
{
	// Using a function-static variable to identify the main thread requires
	// that control reaches here from the main thread before it reaches here
	// from any other thread. We simply trust that whichever thread gets here
	// first is the main thread.
	static LLThread::id_t main_thread_id = tThreadId;
	return main_thread_id;
}

bool is_main_thread()
{
	return tThreadId == get_main_thread_id();
}

void assert_main_thread()
{
	if (tThreadId != get_main_thread_id())
	{
		llerrs << "Illegal execution from thread id " << tThreadId
			   << " outside main thread " << get_main_thread_id() << llendl;
	}
}

//static
LLThread::id_t LLThread::currentID()
{
	return tThreadId;
}

//static
U64 LLThread::thisThreadIdHash()
{
	// Caching the hash in a thread_local static variable for speed. HB
	thread_local U64 id_hash = boost::hash<id_t>()(tThreadId);
	return id_hash;
}

//static
void LLThread::yield()
{
	boost::this_thread::yield();
}

LLThread::LLThread(const std::string& name)
:	mName(name),
#if TRACY_ENABLE
	mThreadName(NULL),
#endif
	mThreadp(NULL),
	mStatus(STOPPED),
	mRetries(1),
	mPaused(false),
	mNeedsAffinity(false)
{
	mRunCondition = new LLCondition();
	mDataLock = new LLMutex();
}

LLThread::~LLThread()
{
	shutdown();
}

void LLThread::threadRun()
{
#ifdef LL_WINDOWS
	set_thread_name(-1, mName.c_str());
#endif
#if TRACY_ENABLE
	if (!mThreadName)
	{
		// We must keep the thread name string till the program exits (i.e. it
		// must outlive the thread), and the string pointer must be unique...
		// See the chapters 3.1.1 and 3.1.2 of the Tracy manual. HB
		gTracyThreadNamesLock.lock();
		gTracyThreadNames.emplace_back(mName);
		mThreadName = gTracyThreadNames.back().c_str();
		gTracyThreadNamesLock.unlock();
	}
	tracy::SetThreadName(mThreadName);
#endif

	mID = tThreadId;
	llinfos << "Running thread " << mName << " with Id: " << mID << llendl;

	// Set the CPU affinity for this child thread to the complementary of the
	// main thread affinity, so that they run on different cores.
	// When the main thread affinity is 0 (or under macOS) this call is a no-
	// operation and no affinity is set for any thread. HB
	S32 result = LLCPUInfo::setThreadCPUAffinity();
	if (!result)
	{
		llwarns << "Failed to set CPU affinity for thread: " << mName
				<< " - Id: " << mID << llendl;
	}
	else if (result == -1)
	{
		mNeedsAffinity = true;
	}

	while (mRetries)
	{
		--mRetries;
		LL_DEBUGS("Threads") << "Running: " << mName << " - Retries left: "
							 << mRetries << LL_ENDL;
		try
		{
			// Run the user supplied function
			run();
		}
		catch (std::runtime_error& e)
		{
			llwarns << "Caught exception '" << e.what() << "' in thread: "
					<< mName << " - Id: " << mID << llendl;
			continue;
		}
		catch (...)
		{
			llwarns << "An unknown exception occurred during thread"
					<< mName << " - Id: " << mID << llendl;
		}
		break;
	}

	LL_DEBUGS("Threads") << "Exiting: " << mName << " - Id: "
						 << mID << LL_ENDL;

	// We are done with the run function, this thread is done executing now.
	mStatus = STOPPED;
}

void LLThread::shutdown()
{
	// WARNING: if you somehow call the thread destructor from itself, the
	// thread will die in an unclean fashion !
	if (mThreadp)
	{
		if (!isStopped())
		{
			// The thread is not already stopped. First, set the flag
			// indicating that we are ready to die
			setQuitting();

			LL_DEBUGS("Threads") << "Killing thread: " << mName << " Status: "
								 << mStatus << LL_ENDL;
			// Now wait a bit for the thread to exit. It is unclear whether I
			// should even bother doing this; this destructor should never get
			// called unless we are already stopped, really...
			S32 counter = 0;
			constexpr S32 MAX_WAIT = 600;
			while (counter < MAX_WAIT)
			{
				if (isStopped())
				{
					break;
				}
				// Sleep for a tenth of a second
				ms_sleep(100);
				yield();
				++counter;
			}
		}

		if (!isStopped())
		{
			// This thread just would not stop, even though we gave it time
			llwarns << "Exiting thread before clean exit !" << llendl;
#if !LL_TERMINATE_THREAD_ON_STALL
			// Simply detach the thread so that no wait on exit will happen
			// (which does not seem to work for Windows, thus why we do use
			// LL_TERMINATE_THREAD_ON_STALL on it). HB
			mThreadp->detach();
			return;
#elif LL_WINDOWS
			TerminateThread(mNativeHandle, 0);
#else
			pthread_cancel(mNativeHandle);
#endif
		}
		mThreadp = NULL;
	}

	delete mRunCondition;
	mRunCondition = NULL;

	delete mDataLock;
	mDataLock = NULL;
}

void LLThread::start()
{
	llassert(isStopped());

	// Set thread state to running
	mStatus = RUNNING;

	try
	{
		mThreadp = new boost::thread(boost::bind(&LLThread::threadRun, this));
#if LL_TERMINATE_THREAD_ON_STALL
		mNativeHandle = mThreadp->native_handle();
#endif
	}
	catch ( ...)
	{
		mStatus = STOPPED;
		llwarns << "Failed to start thread: " << mName << " - Id: " << mID
				<< llendl;
	}
}

// Called from MAIN THREAD. Requests that the thread pauses. The thread will
// pause when (and if) it calls checkPause()
void LLThread::pause()
{
	if (!mPaused)
	{
		// This will cause the thread to stop execution as soon as checkPause()
		// is called. Does not need to be atomic since this is only set/unset
		// from the main thread
		mPaused = true;
	}
}

// Request that the thread pause/resume.
// Called from MAIN THREAD. Requests that the thread resumes.
void LLThread::unpause()
{
	if (mPaused)
	{
		mPaused = false;
	}

	wake(); // Wake up the thread if necessary
}

// Virtual predicate function. Returns true if the thread should wake up, false
// if it should sleep.
bool LLThread::runCondition()
{
	// By default, always run. Handling of pause/unpause is done regardless of
	// this function's result.
	return true;
}

// Called from run() (CHILD THREAD). Stops thread execution if requested until
// unpaused.
void LLThread::checkPause()
{
	if (mNeedsAffinity)
	{
		S32 result = LLCPUInfo::setThreadCPUAffinity();
		if (result == 1)
		{
			mNeedsAffinity = false;
		}
		else if (!result)
		{
			llwarns << "Failed to set CPU affinity for thread: " << mName
					<< " - Id: " << mID << llendl;
		}
	}

	mDataLock->lock();

	// This is in a while loop because the pthread API allows for spurious
	// wakeups.
	while (shouldSleep())
	{
		mDataLock->unlock();
		mRunCondition->wait(); // Locks mRunCondition
		mDataLock->lock();
		// mRunCondition is locked when the thread wakes up
	}

 	mDataLock->unlock();
}

void LLThread::setQuitting()
{
	mDataLock->lock();
	if (mStatus == RUNNING)
	{
		mStatus = QUITTING;
	}
	// It is only safe to remove mRunCondition if all locked threads were
	// notified
	mRunCondition->broadcast();
	mDataLock->unlock();
}

void LLThread::wake()
{
	mDataLock->lock();
	if (!shouldSleep())
	{
		mRunCondition->signal();
	}
	mDataLock->unlock();
}

void LLThread::wakeLocked()
{
	if (!shouldSleep())
	{
		mRunCondition->signal();
	}
}
