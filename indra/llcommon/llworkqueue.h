/**
 * @file llworkqueue.h
 * @brief Queue used for inter-thread work passing.
 * @author Nat Goodspeed
 * @date   2021-09-30
 *
 * $LicenseInfo:firstyear=2021&license=viewergpl$
 *
 * Copyright (c) 2021, Linden Research, Inc.
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

#ifndef LL_WORKQUEUE_H
#define LL_WORKQUEUE_H

#include <exception>				// For std::current_exception

#include "llcoros.h"
#include "llthreadsafequeue.h"

class LLWorkQueue
:	public LLInstanceTracker<LLWorkQueue, std::string,
							 // Allow replacing an old, deleted work queue
							 // with a new one (needed for restoreGL() and
							 // the GL image worker). HB
							 LLInstanceTrackerReplaceOnCollision>
{
protected:
	LOG_CLASS(LLWorkQueue);

public:
	using Work = std::function<void()>;

private:
	using super = LLInstanceTracker<LLWorkQueue, std::string,
									LLInstanceTrackerReplaceOnCollision>;
	// Changed from LLThreadSafeSchedule to LLThreadSafeQueue, to get rid of
	// the useless and slow std::chrono timestamps. HB
	using Queue = LLThreadSafeQueue<Work>;

public:
	using Closed = LLThreadSafeQueueInterrupt;
	// For runFor(), runUntil()
	using TimePoint = std::chrono::steady_clock::time_point;

	struct Error : public std::runtime_error
	{
		Error(const std::string& what)
		:	std::runtime_error(what)
		{
		}
	};

	// You may omit the LLWorkQueue name, in which case a unique name is
	// synthesized; for practical purposes that makes it anonymous.
	LLWorkQueue(const std::string& name = std::string(),
				// The default capacity is huge to avoid blocking the main
				// thread due to a starvation.
				U32 capacity = 1024 * 1024)
	:	super(makeName(name)),
		mQueue(capacity)
	{
	}

	// Since the point of LLWorkQueue is to pass work to some other worker
	// thread(s) asynchronously, it is important that the LLWorkQueue continues
	// to exist until the worker thread(s) have drained it. To communicate that
	// it is time for them to quit, close() the queue.
	LL_INLINE void close()					{ mQueue.close(); }

	// LLWorkQueue supports multiple producers and multiple consumers. In the
	// general case it is misleading to test size(), since any other thread
	// might change it the nanosecond the lock is released. On that basis, some
	// might argue against publishing a size() method at all. But there are two
	// specific cases in which a test based on size() might be reasonable:
	// - If you are the only producer, noticing that size() == 0 is meaningful.
	// - If you are the only consumer, noticing that size() > 0 is meaningful.
	LL_INLINE U32 size()					{ return mQueue.size(); }

	// Returns true when the storage is empty (lock-less and yet thread-safe
	// since based on a cached atomic boolean). HB
	LL_INLINE bool empty()					{ return mQueue.empty(); }

	// Producer's end: are we prevented from pushing any additional items ?
	LL_INLINE bool isClosed()				{ return mQueue.isClosed(); }

	// Consumer's end: are we done, is the queue entirely drained ?
	LL_INLINE bool done()					{ return mQueue.done(); }

	// Statistics (number of completed operations) for the thread calling this.
	LL_INLINE U32 getCalls()				{ return mQueue.getCalls(); }

	//------------------------ Fire and forget API --------------------------//

	// Fire and forget
	template <typename CALLABLE>
	LL_INLINE void post(CALLABLE&& callable)
	{
		mQueue.push(std::move(callable));
	}

	// Posts work, unless the queue is closed before we can post.
	template <typename CALLABLE>
	LL_INLINE bool postIfOpen(CALLABLE&& callable)
	{
		return mQueue.pushIfOpen(std::move(callable));
	}

	// Posts work to be run to another LLWorkQueue, which may or may not still
	// exist and be open. Returns true if we were able to post.
	template <typename CALLABLE>
	static bool postMaybe(weak_t target, CALLABLE&& callable)
	{
		// We are being asked to post to the LLWorkQueue at 'target', which is
		// a weak_ptr: have to lock it to check it.
		auto tptr = target.lock();
		if (tptr)
		{
			try
			{
				tptr->post(std::forward<CALLABLE>(callable));
				return true;	// We were able to post()
			}
			catch (const Closed&)
			{
				// The LLWorkQueue still exists, but is Closed
			}
		}
		// Either target no longer exists, or its LLWorkQueue is Closed.
		return false;
	}

	template <typename CALLABLE>
	LL_INLINE bool tryPost(CALLABLE&& callable)
	{
		return mQueue.tryPush(std::move(callable));
	}

	//-------------------------- Handshaking API ----------------------------//

	// Posts work to another LLWorkQueue to be run at a specified time,
	// requesting a specific callback to be run on this LLWorkQueue on
	// completion. Returns true if we were able to post, false if the other
	// LLWorkQueue is inaccessible.
	template <typename CALLABLE, typename FOLLOWUP>
	bool postTo(weak_t target, CALLABLE&& callable, FOLLOWUP&& callback)
	{
		// We are being asked to post to the LLWorkQueue at 'target', which is
		// a weak_ptr: have to lock it to check it.
		auto tptr = target.lock();
		if (!tptr)
		{
			// Cannot post() if the target LLWorkQueue has been destroyed
			return false;
		}

		// Here we believe target LLWorkQueue still exists. Post to it a lambda
		// that packages our callable, our callback and a weak_ptr to this
		// originating LLWorkQueue.
		tptr->post([reply = super::getWeak(), callable = std::move(callable),
					callback = std::move(callback)]() mutable
				   {
						// Use postMaybe() below in case this originating
						// LLWorkQueue has been closed or destroyed. Remember,
						// the outer lambda is now running on a thread
						// servicing the target LLWorkQueue, and real time has
						// elapsed since postTo()'s tptr->post() call.
						try
						{
							 // Make a reply lambda to repost to THIS
							// LLWorkQueue. Delegate to makeReplyLambda() so we
							// can partially specialize on void return.
							postMaybe(reply,
									  makeReplyLambda(std::move(callable),
													  std::move(callback)));
						}
						catch (...)
						{
							// Either variant of makeReplyLambda() is
							// responsible for calling the caller's callable.
							// If that throws, return the exception to the
							// originating thread.
							postMaybe(reply,
									  // Bind current exception to transport
									  // back to the originating LLWorkQueue.
									  // Once there, rethrow it.
									  [exc = std::current_exception()]()
									  { std::rethrow_exception(exc); });
						}
				   });

		// It looks like we were able to post()...
		return true;
	}

	// Not currently in use: define LL_WAIT_FOR_RESULT to non-zero at compile
	// time if at all needed. HB
#if LL_WAIT_FOR_RESULT
	// Posts work to another LLWorkQueue, blocking the calling coroutine until
	// then, returning the result to caller on completion.
	// In general, we assume that each thread's default coroutine is busy
	// servicing its LLWorkQueue or whatever. To try to prevent mistakes, we
	// forbid calling waitForResult() from a thread's default coroutine.
	template <typename CALLABLE>
	LL_INLINE auto waitForResult(CALLABLE&& callable)
	{
		checkCoroutine("waitForResult()");
		return WaitForResult<CALLABLE,
							 decltype(std::forward<CALLABLE>(callable)())>()
			   (this, std::forward<CALLABLE>(callable));
	}
#endif

	//----------------------------- Worker API ------------------------------//

	// Pulls work items off this LLWorkQueue until the queue is closed, at
	// which point it returns. This would be the typical entry point for a
	// simple worker thread.
	void runUntilClose();

	// Runs all work items that are ready to run. Returns true if the queue
	// remains open, false if the queue has been closed. This could be used by
	// a thread whose primary purpose is to serve the queue, but also wants to
	// do other things with its idle time.
	bool runPending();

	// Runs at most one ready work item (zero if none are ready). Returns
	// true if the queue remains open, false if it has been closed.
	bool runOne();

	// Runs a subset of ready work items, until the timeslice has been
	// exceeded. Returns true if the queue remains open, false if the queue has
	// been closed. This could be used by a busy main thread to lend a bounded
	// few CPU cycles to this LLWorkQueue without risking it blowing out the
	// length of any one frame.
	// Modified to take an optional 'work_remaining' pointer, that, when non
	// NULL, allows to return the size of the queue. HB
	template <typename Rep, typename Period>
	LL_INLINE bool runFor(const std::chrono::duration<Rep, Period>& timeslice,
						  size_t* work_remaining = NULL)
	{
		return runUntil(TimePoint::clock::now() + timeslice, work_remaining);
	}

	// Just like runFor(), only with a specific end time instead of a timeslice
	// duration.
	// Modified to take an optional 'work_remaining' pointer, that, when non
	// NULL, allows to return the size of the queue. HB
	bool runUntil(const TimePoint& until, size_t* work_remaining = NULL);

private:
	// General case: arbitrary C++ return type
	template <typename CALLABLE, typename FOLLOWUP, typename RETURNTYPE>
	struct MakeReplyLambda;

	// Specialize for CALLABLE returning void
	template <typename CALLABLE, typename FOLLOWUP>
	struct MakeReplyLambda<CALLABLE, FOLLOWUP, void>;

#if LL_WAIT_FOR_RESULT
	// General case: arbitrary C++ return type
	template <typename CALLABLE, typename RETURNTYPE>
	struct WaitForResult;

	// Specialize for CALLABLE returning void
	template <typename CALLABLE>
	struct WaitForResult<CALLABLE, void>;

	static void checkCoroutine(const std::string& method);
#endif

	template <typename CALLABLE, typename FOLLOWUP>
	LL_INLINE static auto makeReplyLambda(CALLABLE&& callable,
										  FOLLOWUP&& callback)
	{
		return MakeReplyLambda<CALLABLE, FOLLOWUP,
							  decltype(std::forward<CALLABLE>(callable)())>()
			   (std::move(callable), std::move(callback));
	}

	void callWork(const Work& work);

	static std::string makeName(const std::string& name);
	static void error(const std::string& msg);

private:
	Queue mQueue;
};

// General case: arbitrary C++ return type
template <typename CALLABLE, typename FOLLOWUP, typename RETURNTYPE>
struct LLWorkQueue::MakeReplyLambda
{
	auto operator()(CALLABLE&& callable, FOLLOWUP&& callback)
	{
		// Call the callable in any case, but to minimize copying the result,
		// immediately bind it into the reply lambda. The reply lambda also
		// binds the original callback, so that when we, the originating
		// LLWorkQueue, finally receive and process the reply lambda, we will
		// call the bound callback with the bound result, on the same thread
		// that originally called postTo().
		return [result = std::forward<CALLABLE>(callable)(),
				callback = std::move(callback)]() mutable
			   { callback(std::move(result)); };
	}
};

// Specialize for CALLABLE returning void
template <typename CALLABLE, typename FOLLOWUP>
struct LLWorkQueue::MakeReplyLambda<CALLABLE, FOLLOWUP, void>
{
	auto operator()(CALLABLE&& callable, FOLLOWUP&& callback)
	{
		// Call the callable, which produces no result.
		std::forward<CALLABLE>(callable)();
		// Our completion callback is simply the caller's callback.
		return std::move(callback);
	}
};

#if LL_WAIT_FOR_RESULT
// General case: arbitrary C++ return type
template <typename CALLABLE, typename RETURNTYPE>
struct LLWorkQueue::WaitForResult
{
	auto operator()(LLWorkQueue* self, CALLABLE&& callable)
	{
		LLCoros::Promise<RETURNTYPE> promise;
		self->post(// We dare to bind a reference to Promise because it is
				   // specifically designed for cross-thread communication.
				   [&promise, callable = std::move(callable)]() mutable
				   {
						try
						{
							// Call the caller's callable and trigger promise
							// with result
							promise.set_value(callable());
						}
						catch (...)
						{
							promise.set_exception(std::current_exception());
						}
				   });
		auto future{ LLCoros::getFuture(promise) };
		// Now, on the calling thread, wait for that result
		return future.get();
	}
};

// Specialize for CALLABLE returning void
template <typename CALLABLE>
struct LLWorkQueue::WaitForResult<CALLABLE, void>
{
	auto operator()(LLWorkQueue* self, CALLABLE&& callable)
	{
		LLCoros::Promise<void> promise;
		self->post(// We dare to bind a reference to Promise because it is
				   // specifically designed for cross-thread communication.
				   [&promise, callable = std::move(callable)]() mutable
				   {
						try
						{
							callable();
							promise.set_value();
						}
						catch (...)
						{
							promise.set_exception(std::current_exception());
						}
				   });
		auto future{ LLCoros::getFuture(promise) };
		// Now, on the calling thread, wait for that result
		return future.get();
	}
};
#endif	// LL_WAIT_FOR_RESULT

#endif	// LL_WORKQUEUE_H
