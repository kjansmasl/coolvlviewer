/**
 * @file llthreadpool.h
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

#ifndef LL_THREADPOOL_H
#define LL_THREADPOOL_H

#include "hbfastmap.h"
#include "llworkqueue.h"

class LLThreadPool
{
protected:
	LOG_CLASS(LLThreadPool);

public:
	// LLThreadPool takes a string name. This can be used to look up the
	// relevant LLWorkQueue.
	LLThreadPool(const std::string& name, U32 threads = 1,
				 // The default capacity is huge to avoid blocking the main
				 // thread due to a starvation.
				 U32 capacity = 1024 * 1024);
	virtual ~LLThreadPool() = default;

	// Launch the LLThreadPool. Until this call, a constructed LLThreadPool
	// launches no threads. That permits coders to derive from LLThreadPool,
	// or store it as a member of some other class, but refrain from launching
	// it until all other construction is complete.
	// If wait_for_start is true, wait until all threads have actually started
	// before returning to the caller. HB
	void start(bool wait_for_start = false);

	// LLThreadPool listens for application shutdown messages on the "LLApp"
	// LLEventPump. Call close() to shut down this LLThreadPool early. Note
	// that this is a wrapper to the "real" close(), so that the "on_shutdown"
	// and "on_crash" booleans cannot be wrongly used in the latter. HB
	LL_INLINE void close()							{ close(false, false); }

	LL_INLINE const std::string& getName() const	{ return mName; }
	LL_INLINE U32 getWidth() const					{ return mThreads.size(); }

	// Number of threads used to service the queue. HB
	LL_INLINE U32 getThreadsCount() const			{ return mThreadCount; }
	// Number of threads actually and currently started. HB
	LL_INLINE U32 getStartedThreads() 				{ return mStartedThreads; }

	// Override this if you do not want your thread to be accounted as
	// "started" by LLThreadPool::run(const std::string& name) before some
	// initialization work is fully performed in your own run() method; in this
	// case, simply override this with a no-op method, and do call the second,
	// non overridable method below when appropriate in your overriden run()
	// method. HB
	LL_INLINE virtual void maybeIncStartedThreads()	{ ++mStartedThreads; }
	LL_INLINE void doIncStartedThreads()			{ ++mStartedThreads; }

	// Returns the name for a thread with a given thread Id hash, or "invalid"
	// when that hash is not found. HB
	const std::string& getThreadName(U64 id_hash);

	// Obtains a non-const reference to the LLWorkQueue to post work to it.
	LL_INLINE LLWorkQueue& getQueue()				{ return mQueue; }

	// Override run() if you need special processing. The default run()
	// implementation simply calls LLWorkQueue::runUntilClose().
	virtual void run();

private:
	void close(bool on_shutdown, bool on_crash);
	void run(const std::string& name);
	void closeOnShutdown();

private:
	LLWorkQueue		mQueue;
	std::string		mName;
#if TRACY_ENABLE
	const char*		mThreadPoolName;
#endif
	typedef std::vector<std::pair<std::string, boost::thread> > threads_list_t;
	threads_list_t	mThreads;
	LLMutex			mThreadNamesMutex;
	typedef safe_hmap<U64, std::string> tnames_map_t;
	tnames_map_t	mThreadNames;
	// mStartedThreads is incremented each time a new thread is actually
	// started since threads launch is itself a threaded operation; thus why
	// we also must use an atomic counter here. HB
	LLAtomicU32		mStartedThreads;
	U32				mThreadCount;
};

#endif	// LL_THREADPOOL_H
