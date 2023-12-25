/**
 * @file llthreadpool.cpp
 * @brief Configures a LLWorkQueue along with a pool of threads to service it.
 * @author Nat Goodspeed
 * @date   2021-10-21
 *
 * $LicenseInfo:firstyear=2021&license=viewergpl$
 *
 * Copyright (c) 2021, Linden Research, Inc. (c) 2022 Henri Beauchamp.
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

#include "llthreadpool.h"

#include "llevents.h"
#include "llsys.h"
#include "lltimer.h"		// For ms_sleep()
#include "hbtracy.h"

LLThreadPool::LLThreadPool(const std::string& name, U32 threads, U32 capacity)
:	mQueue(name, capacity),
	mName("ThreadPool:" + name),
#if TRACY_ENABLE
	mThreadPoolName(NULL),
#endif
	mThreadCount(threads),
	mStartedThreads(0)
{
	mThreads.reserve(threads);
}

const std::string& LLThreadPool::getThreadName(U64 id_hash)
{
	static const std::string invalid = "invalid";
	mThreadNamesMutex.lock();
	tnames_map_t::const_iterator it = mThreadNames.find(id_hash);
	tnames_map_t::const_iterator end = mThreadNames.end();
	mThreadNamesMutex.unlock();
	return it != end ? it->second : invalid;
}

void LLThreadPool::start(bool wait_for_start)
{
	std::string tname;
	for (U32 i = 0; i < mThreadCount; ++i)
	{
		tname = llformat("%s:%d/%d", mName.c_str(), i + 1, mThreadCount);
		// Note 1: this fails to compile with gcc v5.5 which does not like
		// emplacing a lambda as a boost::thread (or std::thread)... HB
		// Note 2: since we queue a thread, this is also a threaded operation
		// and the newly created thread will start asynchronously to this
		// queueing operation. HB
		mThreads.emplace_back(tname,
							  [this, tname]()
							  {
									run(tname);
							  });
	}

	LLEventPump& pump = gEventPumps.obtain("LLApp");
	pump.listen(mName,
				[this](const LLSD& stat)
				{
					std::string status = stat["status"];
					if (status != "running")
					{
						// Note: on crash, the app first goes to "error" status,
						// then to "stopped" status as soon as it ran its error
						// handler. If the status directly reaches "stopped",
						// then it is too late to join the threads anyway, so
						// we consider anything else than "quitting" to be the
						// result of a crash, in order to avoid never ending
						// loops waiting for thread shutdown. HB
						close(true, status != "quitting");
					}
					return false;
				});

	if (wait_for_start)
	{
		// We wait for all threads to start before we return to the caller
		// (which is normally itself running on the main thread); this ensures
		// no operation will be attempted before all threads are ready to
		// process the queue. HB
		do
		{
			ms_sleep(1);
		}
		while ((U32)mStartedThreads < mThreadCount);
	}

	// Yield and give a tiny bit of time for the threads to start; use true for
	// the wait_for_start boolean if you want to ensure all threads get started
	// before we return to the caller (in which case, this additional sleep
	// gives a chance for the last started thread to call its run() method
	// before we return). HB
	ms_sleep(1);
}

void LLThreadPool::close(bool on_shutdown, bool on_crash)
{
	if (on_crash)
	{
		llinfos << mName << " was informed of viewer crash." << llendl;
	}
	else if (on_shutdown)
	{
		llinfos << mName << " was informed of viewer shutdown." << llendl;
	}

	// Un-register ourselves from the event pump to avoid a crash should we
	// re-register later under the same name (which happens for the GL image
	// worker thread on restoreGL() calls). HB
	LL_DEBUGS("ThreadPool") << mName << ": stop listening to LLApp events..."
							<< LL_ENDL;
	gEventPumps.obtain("LLApp").stopListening(mName);

	if (mQueue.isClosed())
	{
		LL_DEBUGS("ThreadPool") << mName << " queue is already closed."
								<< LL_ENDL;
		return;	// Nothing to do...
	}

	llinfos << mName << ": closing queue..." << llendl;
	mQueue.close();
	// Do not join threads on crash, to avoid waiting indefinitely (those
	// threads might rely on conditions to exit, that might not be possible
	// to fulfil any more due to the crash). Instead, let the OS clean up
	// for us... HB
	if (!on_crash)
	{
		llinfos << mName << ": joining threads..." << llendl;
		for (auto& pair : mThreads)
		{
			LL_DEBUGS("ThreadPool") << mName << " waiting on thread "
									<< pair.first << LL_ENDL;
			pair.second.join();
		}
	}
	llinfos << mName << " shutdown complete with "
			<< (mQueue.empty() ? "an " : "a non-") << "empty queue." << llendl;
}

void LLThreadPool::run(const std::string& name)
{
	llinfos << "Starting thread: " << name << llendl;

	mThreadNamesMutex.lock();
#if TRACY_ENABLE
	if (!mThreadPoolName)
	{
		// We must keep the thread name string till the program exits (i.e. it
		// must outlive the thread), and the string pointer must be unique...
		// See the chapters 3.1.1 and 3.1.2 of the Tracy manual. HB
		gTracyThreadNamesLock.lock();
		gTracyThreadNames.emplace_back(name.substr(0, name.rfind(':')));
		mThreadPoolName = gTracyThreadNames.back().c_str();
		gTracyThreadNamesLock.unlock();
	}
	// We give to all threads of a given pool, the same (pool) name for Tracy
	// since we do not care about per-thread timing, but only want to see stats
	// about the pool itself, as a whole. HB
	tracy::SetThreadName(mThreadPoolName);
#endif
	// Give the thread a way to find its own name later on... HB
	mThreadNames.emplace(LLThread::thisThreadIdHash(), name);
	mThreadNamesMutex.unlock();

	// Set the CPU affinity for this child thread to the complementary of the
	// main thread affinity, so that they run on different cores. When the main
	// thread affinity is 0 (or under macOS) this call is a no-operation and no
	// affinity is set for any thread. HB
	S32 result = LLCPUInfo::setThreadCPUAffinity();
	if (!result)
	{
		llwarns << "Failed to set CPU affinity for thread: " << name << llendl;
	}
	else if (result == -1)
	{
		llinfos << "Could not set CPU affinity for thead: " << name
				<< " (main thread affinity not yet set)." << llendl;
	}

	// One more thread has been started, but maybe not fully intialized and
	// ready, in which case you must override this call with your own, no-op
	// maybeIncStartedThreads() method, and call, when appropriate, the
	// doIncStartedThreads() method in your own overridden run() method. HB
	maybeIncStartedThreads();

	run();

	llinfos << "Thread " << name
			<< " stopped. Number of operations performed: "
			<< mQueue.getCalls() << llendl;
}

//virtual
void LLThreadPool::run()
{
	mQueue.runUntilClose();
}
