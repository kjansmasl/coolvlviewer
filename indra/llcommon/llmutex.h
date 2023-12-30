/**
 * @file llmutex.h
 * @brief Base classes for mutex and condition handling.
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

#ifndef LL_LLMUTEX_H
#define LL_LLMUTEX_H

// For now disable the fibers-aware mutexes unconditionnaly since they cause a
// weird issue with "Illegal deletion of LLDrawable" (and even though I plugged
// the cases where it could cause crashes, it is an abnormal condition, hinting
// for an issue in how mutexes may fail when taken across fibers). HB
#define LL_USE_FIBER_AWARE_MUTEX 0

#include <mutex>
#if LL_USE_FIBER_AWARE_MUTEX
# include "boost/fiber/mutex.hpp"
# include "boost/fiber/recursive_mutex.hpp"
# include "boost/fiber/condition_variable.hpp"
# define LL_MUTEX_TYPE boost::fibers::mutex
# define LL_REC_MUTEX_TYPE boost::fibers::recursive_mutex
# define LL_UNIQ_LOCK_TYPE std::unique_lock<boost::fibers::mutex>
# define LL_UNIQ_LOCK_REC_TYPE std::unique_lock<boost::fibers::recursive_mutex>
# define LL_COND_TYPE boost::fibers::condition_variable
# define LL_COND_ANY_TYPE boost::fibers::condition_variable_any
#else
# include <condition_variable>
# define LL_MUTEX_TYPE std::mutex
# define LL_REC_MUTEX_TYPE std::recursive_mutex
# define LL_UNIQ_LOCK_TYPE std::unique_lock<std::mutex>
# define LL_UNIQ_LOCK_REC_TYPE std::unique_lock<std::recursive_mutex>
# define LL_COND_TYPE std::condition_variable
# define LL_COND_ANY_TYPE std::condition_variable_any
#endif

#include "llerror.h"

class LLMutex
{
protected:
	LOG_CLASS(LLMutex);

public:
	LLMutex() = default;
	virtual ~LLMutex() = default;

	LL_INLINE void lock()		{ mMutex.lock(); }
	LL_INLINE void unlock()		{ mMutex.unlock(); }
	LL_INLINE bool trylock()	{ return mMutex.try_lock(); }

	bool isLocked();

protected:
	LL_REC_MUTEX_TYPE mMutex;
};

// Actually a condition/mutex pair (since each condition needs to be associated
// with a mutex).
class LLCondition : public LLMutex
{
public:
	LLCondition() = default;
	~LLCondition() override = default;

	// This method blocks
	LL_INLINE void wait()
	{
		LL_UNIQ_LOCK_REC_TYPE lock(mMutex);
		mCond.wait(lock);
	}

	LL_INLINE void signal()
	{
		mCond.notify_one();
	}

	LL_INLINE void broadcast()
	{
		mCond.notify_all();
	}

protected:
	LL_COND_ANY_TYPE mCond;
};

// Scoped locking class
class LLMutexLock
{
public:
	LL_INLINE LLMutexLock(LLMutex* mutexp)
	:	mMutex(mutexp)
	{
		if (mMutex)
		{
			mMutex->lock();
		}
	}

	LL_INLINE LLMutexLock(LLMutex& mutex)
	:	mMutex(&mutex)
	{
		mMutex->lock();
	}

	LL_INLINE ~LLMutexLock()
	{
		if (mMutex)
		{
			mMutex->unlock();
		}
	}

private:
	LLMutex* mMutex;
};

// Scoped locking class similar in function to LLMutexLock but uses the
// trylock() method to conditionally acquire lock without blocking. Caller
// resolves the resulting condition by calling the isLocked() method and either
// punts or continues as indicated.
//
// Mostly of interest to callers needing to avoid stalls and that can guarantee
// another attempt at a later time.
class LLMutexTrylock
{
public:
	LL_INLINE LLMutexTrylock(LLMutex* mutex)
	:	mMutex(mutex)
	{
		mLocked = mMutex && mMutex->trylock();
	}

	LL_INLINE LLMutexTrylock(LLMutex& mutex)
	:	mMutex(&mutex)
	{
		mLocked = mMutex->trylock();
	}

	// Tries locking 'attempts' times, with 10ms sleep delays between each try.
	LLMutexTrylock(LLMutex* mutex, U32 attempts);

	LL_INLINE ~LLMutexTrylock()
	{
		if (mLocked)
		{
			mMutex->unlock();
		}
	}

	LL_INLINE void unlock()
	{
		if (mLocked)
		{
			mLocked = false;
			mMutex->unlock();
		}
	}

	LL_INLINE bool isLocked() const				{ return mLocked; }

private:
	LLMutex*	mMutex;
	// 'true' when the mutex is actually locked by this class
	bool		mLocked;
};

#endif // LL_LLMUTEX_H
