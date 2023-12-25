/**
 * @file llthreadsafequeue.h
 * @brief Queue protected with mutexes for cross-thread use.
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLTHREADSAFEQUEUE_H
#define LL_LLTHREADSAFEQUEUE_H

#include <chrono>
#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "llatomic.h"
#include "hbfastmap.h"
#include "llthread.h"		// Also includes llmutex.h

// Keep this after inclusion of llmutex.h, so that LL_USE_FIBER_AWARE_MUTEX is
// properly set. HB
#if LL_USE_FIBER_AWARE_MUTEX
# include "boost/fiber/timed_mutex.hpp"
# define LL_TIMED_MUTEX_TYPE boost::fibers::timed_mutex
# define LL_CV_STATUS_TYPE boost::fibers::cv_status
#else
# include <condition_variable>
# define LL_TIMED_MUTEX_TYPE std::timed_mutex
# define LL_CV_STATUS_TYPE std::cv_status
#endif

///////////////////////////////////////////////////////////////////////////////
// LLThreadSafeQueueInterrupt class template
///////////////////////////////////////////////////////////////////////////////

class LLThreadSafeQueueInterrupt : public std::runtime_error
{
public:
	LLThreadSafeQueueInterrupt()
	:	std::runtime_error("queue operation interrupted")
	{
	}
};

// Implements a thread-safe FIFO. Let the default std::queue default to
// underlying std::deque. Override if desired.
template<typename ElementT, typename QueueT = std::queue<ElementT> >
class LLThreadSafeQueue
{
public:
	typedef ElementT value_type;
	typedef std::unique_lock<LL_TIMED_MUTEX_TYPE> lock_t;

	// Limiting the number of pending items prevents unbounded growth of the
	// underlying queue.
	LLThreadSafeQueue(size_t capacity = 1024)
	:	mCapacity(capacity),
		mEmpty(false),
		mClosed(false)
	{
	}

	virtual ~LLThreadSafeQueue() = default;

	// Add an element to the front of queue (will block if the queue has
	// reached capacity).
	// This call will raise an interrupt error if the queue is deleted while
	// the caller is blocked.
	template <typename T> void push(T&& element)
	{
		if (!pushIfOpen(std::forward<T>(element)))
		{
			throw(LLThreadSafeQueueInterrupt());
		}
	}

	// Adds an element to the queue (will block if the queue has reached its
	// maximum capacity). Returns false if the queue is closed before push is
	// possible.
	template <typename T> bool pushIfOpen(T&& element)
	{
		lock_t lock1(mLock);
		while (true)
		{
			// On the producer side, it does not matter whether the queue has
			// been drained or not: the moment either end calls close(),
			// further push() operations will fail.
			if (mClosed)
			{
				return false;
			}

			if (push_(lock1, std::forward<T>(element)))
			{
				return true;
			}

			// Storage is full. Wait for signal.
			mCapacityCond.wait(lock1);
		}
	}

	// Tries to add an element to the front of queue without blocking. Returns
	// true only if the element was actually added.
	template <typename T> bool tryPush(T&& element)
	{
		return tryLock([this, element = std::move(element)](lock_t& lock)
					   {
							if (mClosed)
							{
								return false;
							}
							return push_(lock, std::move(element));
					   });
	}

	// Tries to add an element to the queue, blocking if full but with timeout
	// after specified duration. Returns true if the element was added. There
	// are potentially two different timeouts involved: how long to try to lock
	// the mutex, versus how long to wait for the queue to stop being full.
	// Careful settings for each timeout might be orders of magnitude apart.
	// However, this method conflates them.
	template <typename Rep, typename Period, typename T>
	bool tryPushFor(const std::chrono::duration<Rep, Period>& timeout,
					T&& element)
	{
		// Convert duration to time_point; passing the same timeout duration to
		// each of multiple calls is wrong.
		return tryPushUntil(std::chrono::steady_clock::now() + timeout,
							std::forward<T>(element));
	}

	// Tries to add an element to the queue, blocking if full but with timeout
	// at specified time_point. Returns true if the element was added.
	template <typename Clock, typename Duration, typename T>
	bool tryPushUntil(const std::chrono::time_point<Clock, Duration>& until,
					  T&& element)
	{
		return tryLockUntil(until,
							[this, until,
							 element = std::move(element)](lock_t& lock)
							{
								while (!mClosed)
								{
									if (push_(lock, std::move(element)))
									{
										return true;
									}
									if (LL_CV_STATUS_TYPE::timeout ==
											mCapacityCond.wait_until(lock,
																	 until))
									{
										// Timed out; formally we might re-
										// check both conditions above.
										return false;
									}
									// If we did not time out, we were notified
									// for some reason. Loop back to check.
								}
								return false;
							});
	}

	// Pops the element at the end of the queue (will block if the queue is
	// empty). This call will raise an interrupt error if the queue is deleted
	// while the caller is blocked.
	ElementT pop()
	{
		lock_t lock1(mLock);
		ElementT value;
		while (true)
		{
			// On the consumer side, we always try to pop before checking
			// mClosed so we can finish draining the queue.
			pop_result popped = pop_(lock1, value);
			if (popped == POPPED)
			{
				return value;
			}
			// Once the queue is DONE, there will never be any more coming.
			if (popped == DONE)
			{
				throw(LLThreadSafeQueueInterrupt());
			}

			// If we did not pop because WAITING, i.e. canPop() returned false,
			// then even if the producer end has been closed, there is still at
			// least one item to drain: wait for it. Or we might be EMPTY, with
			// the queue still open. Either way, wait for signal.
			mEmptyCond.wait(lock1);
		}
	}

	// Pops an element from the end of the queue if there is one available.
	// Returns true only if an element was popped.
	bool tryPop(ElementT& element)
	{
		return tryLock([this, &element](lock_t& lock)
					   {
							// Conflate EMPTY, DONE, WAITING: tryPop() behavior
							// when the closed queue is implemented by simple
							// inability to push any new elements.
							return pop_(lock, element) == POPPED;
					   });
	}

	// Pops an element from the end of the queue, blocking if empty, with
	// timeout after specified duration. Returns true if an element was popped.
	template <typename Rep, typename Period>
	bool tryPopFor(const std::chrono::duration<Rep, Period>& timeout,
				   ElementT& element)
	{
		// Convert duration to time_point; passing the same timeout duration to
		// each of multiple calls is wrong.
		return tryPopUntil(std::chrono::steady_clock::now() + timeout,
						   element);
	}

	// Pops the element at the head of the queue, blocking if empty, with
	// timeout after specified duration. Returns true if an element was popped.
	template <typename Clock, typename Duration>
	bool tryPopUntil(const std::chrono::time_point<Clock, Duration>& until,
					 ElementT& element)
	{
		return tryLockUntil(until,
							[this, until, &element](lock_t& lock)
							{
								// Conflate EMPTY, DONE, WAITING
								return tryPopUntil_(lock, until,
													element) == POPPED;
							});
	}

	// Returns the size of the queue.
	size_t size()
	{
		lock_t lock(mLock);
		return mStorage.size();
	}

	// Returns true when the storage is empty (lock-less and yet thread-safe
	// since mEmpty is atomic). HB
	LL_INLINE bool empty()								{ return mEmpty; }

	// Returns the capacity of the queue.
	LL_INLINE size_t capacity()							{ return mCapacity; }

	// Closes the queue:
	//  - Every subsequent push() call will throw LLThreadSafeQueueInterrupt.
	//  - Every subsequent tryPush() call will return false.
	//  - pop() calls will return normally until the queue is drained, then
	//    every subsequent pop() will throw LLThreadSafeQueueInterrupt.
	//  - tryPop() calls will return normally until the queue is drained,
	//    then every subsequent tryPop() call will return false.
	void close()
	{
		mClosed = true;
		// Wake up any blocked pop() calls
		mEmptyCond.notify_all();
		// Wake up any blocked push() calls
		mCapacityCond.notify_all();
	}

	// Producer's end: are we prevented from pushing any additional items ?
	// Now lock-less and yet thread-safe since I made mClosed atomic. HB
	LL_INLINE bool isClosed()							{ return mClosed; }

	// Consumer's end: are we done, is the queue entirely drained ?
	// Modified to take an optional 'work_remaining' pointer, that, when non
	// NULL, allows to return the size of the queue (saves a lock(), when
	// compared to using a separated size() call, and provides the actual size
	// as seen by done(), not the size micro seconds later, which could be
	// different already). HB
	bool done(size_t* work_remaining = NULL)
	{
		lock_t lock(mLock);
		size_t size = mStorage.size();
		if (work_remaining)
		{
			*work_remaining = size;
		}
		return !size && mClosed;
	}

	// Return the number of elements popped from the queue by the thread
	// calling this method now. HB
	U32 getCalls()
	{
		lock_t lock(mLock);
		threads_stats_t::const_iterator it =
			mStats.find(LLThread::thisThreadIdHash());
		return it != mStats.end() ? it->second : 0;
	}

protected:
	enum pop_result { EMPTY, DONE, WAITING, POPPED };

	// Implementation logic, suitable for passing to tryLockUntil()
	template <typename Clock, typename Duration>
	pop_result tryPopUntil_(lock_t& lock,
							const std::chrono::time_point<Clock, Duration>& until,
							ElementT& element)
	{
		while (true)
		{
			pop_result popped = pop_(lock, element);
			if (popped == POPPED || popped == DONE)
			{
				// If we succeeded, great ! If we have drained the last item,
				// so be it. Either way, break the loop and tell caller.
				return popped;
			}

			// EMPTY or WAITING: wait for signal.
			if (LL_CV_STATUS_TYPE::timeout ==
					mEmptyCond.wait_until(lock, until))
			{
				// Timed out: formally we might re-check as it is, break loop.
				return popped;
			}

			// If we did not time out, we were notified for some reason. Loop
			// back to check.
		}
	}

	// I we are able to lock immediately, does so and runs the passed callable,
	// which must accept lock_t& and return bool.
	template <typename CALLABLE>
	bool tryLock(CALLABLE&& callable)
	{
		lock_t lock1(mLock, std::defer_lock);
		if (!lock1.try_lock())
		{
			return false;
		}
		return std::forward<CALLABLE>(callable)(lock1);
	}

	// I we are able to lock before the passed time_point, does so and runs the
	// passed callable, which must accept lock_t& and return bool.
	template <typename Clock, typename Duration, typename CALLABLE>
	bool tryLockUntil(const std::chrono::time_point<Clock, Duration>& until,
					  CALLABLE&& callable)
	{
		lock_t lock1(mLock, std::defer_lock);
		if (!lock1.try_lock_until(until))
		{
			return false;
		}
		return std::forward<CALLABLE>(callable)(lock1);
	}

	// While lock is locked, really pushes the passed element, if possible.
	template <typename T>
	bool push_(lock_t& lock, T&& element)
	{
		if (mStorage.size() >= mCapacity)
		{
			return false;
		}

		mStorage.push(std::forward<T>(element));
		mEmpty = false;
		lock.unlock();
		// Now that we have pushed, if somebody has been waiting to pop, signal
		// them.
		mEmptyCond.notify_one();
		return true;
	}

	// While lock is locked, really pops the head element, if possible.
	pop_result pop_(lock_t& lock, ElementT& element)
	{
		if (mStorage.empty())
		{
			mEmpty = true;
			// If mStorage is empty, there is no head element.
			return mClosed ? DONE : EMPTY;
		}
		if (!canPop(mStorage.front()))
		{
			return WAITING;
		}

		// QueueT::front() is the element about to pop()
		element = mStorage.front();
		mStorage.pop();
		mEmpty = mStorage.empty();
		// Add one to the number of popped elements, for this thread stats. HB
		++mStats[LLThread::thisThreadIdHash()];
		lock.unlock();
		// Now that we have popped, if somebody has been waiting to push,
		// signal them.
		mCapacityCond.notify_one();
		return POPPED;
	}

	// Is the current head element ready to pop ?  Yes by default and the sub-
	// class can override as needed.
	virtual bool canPop(const ElementT& head) const		{ return true; }

protected:
	typedef QueueT queue_type;
	QueueT						mStorage;
	// A map to keep track of the elements popping statistics per thread. HB
	typedef flat_hmap<U64, U32> threads_stats_t;
	threads_stats_t				mStats;
	LL_TIMED_MUTEX_TYPE			mLock;
	LL_COND_ANY_TYPE			mCapacityCond;
	LL_COND_ANY_TYPE			mEmptyCond;
	LLAtomicBool				mClosed;
	LLAtomicBool				mEmpty;
	size_t						mCapacity;
};

#if 0	// Not used for now

///////////////////////////////////////////////////////////////////////////////
// LLPriorityQueueAdapter class template
///////////////////////////////////////////////////////////////////////////////

// std::priority_queue's API is almost like std::queue, intentionally of
// course, but you must access the element about to pop() as top() rather than
// than as front(). Make an adapter for use with LLThreadSafeQueue.

template <typename T, typename Container = std::vector<T>,
		  typename Compare = std::less<typename Container::value_type> >
class LLPriorityQueueAdapter
{
public:
	// Publish all the same types
	typedef std::priority_queue<T, Container, Compare> queue_type;
	typedef typename queue_type::container_type container_type;
	typedef typename queue_type::value_type value_type;
	typedef typename queue_type::size_type size_type;
	typedef typename queue_type::reference reference;
	typedef typename queue_type::const_reference const_reference;

	// Although std::queue defines both const and non-const front() methods,
	// std::priority_queue defines only const top().
	LL_INLINE const_reference front() const			{ return mQ.top(); }

	// All the rest of these merely forward to the corresponding queue_type
	// methods.
	LL_INLINE bool empty() const					{ return mQ.empty(); }
	LL_INLINE size_type size() const				{ return mQ.size(); }
	LL_INLINE void push(const value_type& value)	{ mQ.push(value); }
	LL_INLINE void push(value_type&& value)			{ mQ.push(std::move(value)); }
	LL_INLINE void pop()							{ mQ.pop(); }

	template <typename... Args>
	LL_INLINE void emplace(Args&&... args)
	{
		mQ.emplace(std::forward<Args>(args)...);
	}

private:
	queue_type mQ;
};

#endif

#endif	// LL_LLTHREADSAFEQUEUE_H
