/**
 * @file llcallbacklist.cpp
 * @brief A simple list of callback functions to call.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llcallbacklist.h"

#include "lleventtimer.h"

LLCallbackList gIdleCallbacks;

void LLCallbackList::addFunction(callback_t func, void* data)
{
	if (!func)
	{
		llwarns << "Function is NULL" << llendl;
		llassert(false);
		return;
	}

	// only add one callback per func/data pair
	if (!containsFunction(func, data))
	{
		callback_pair_t t(func, data);
		mCallbackList.push_back(t);
	}
}

bool LLCallbackList::deleteFunction(callback_t func, void* data)
{
	callback_list_t::iterator iter = find(func, data);
	if (iter != mCallbackList.end())
	{
		mCallbackList.erase(iter);
		return true;
	}
	else
	{
		return false;
	}
}

void LLCallbackList::deleteAllFunctions()
{
	mCallbackList.clear();
}

void LLCallbackList::callFunctions()
{
	for (callback_list_t::iterator iter = mCallbackList.begin();
		 iter != mCallbackList.end(); )
	{
		callback_list_t::iterator curiter = iter++;
		curiter->first(curiter->second);
	}
}

// Shim class to allow arbitrary boost::bind expressions to be run as one-time
// idle callbacks.
class OnIdleCallbackOneTime
{
public:
	OnIdleCallbackOneTime(nullary_func_t callable)
	:	mCallable(callable)
	{
	}

	static void onIdle(void* data)
	{
		gIdleCallbacks.deleteFunction(onIdle, data);
		OnIdleCallbackOneTime* self = reinterpret_cast<OnIdleCallbackOneTime*>(data);
		self->call();
		delete self;
	}

	LL_INLINE void call()
	{
		mCallable();
	}

private:
	nullary_func_t mCallable;
};

void doOnIdleOneTime(nullary_func_t callable)
{
	OnIdleCallbackOneTime* cb_functor = new OnIdleCallbackOneTime(callable);
	gIdleCallbacks.addFunction(&OnIdleCallbackOneTime::onIdle, cb_functor);
}

// Shim class to allow generic boost functions to be run as recurring idle
// callbacks. Callable should return true when done, false to continue getting
// called.
class OnIdleCallbackRepeating
{
public:
	OnIdleCallbackRepeating(bool_func_t callable)
	:	mCallable(callable)
	{
	}

	// Will keep getting called until the callable returns true.
	static void onIdle(void* data)
	{
		OnIdleCallbackRepeating* self = reinterpret_cast<OnIdleCallbackRepeating*>(data);
		bool done = self->call();
		if (done)
		{
			gIdleCallbacks.deleteFunction(onIdle, data);
			delete self;
		}
	}

	LL_INLINE bool call()
	{
		return mCallable();
	}

private:
	bool_func_t mCallable;
};

void doOnIdleRepeating(bool_func_t callable)
{
	OnIdleCallbackRepeating* cb_functor = new OnIdleCallbackRepeating(callable);
	gIdleCallbacks.addFunction(&OnIdleCallbackRepeating::onIdle, cb_functor);
}

class NullaryFuncEventTimer : public LLEventTimer
{
public:
	NullaryFuncEventTimer(nullary_func_t callable, F32 seconds)
	:	LLEventTimer(seconds),
		mCallable(callable)
	{
	}

private:
	LL_INLINE bool tick()
	{
		mCallable();
		return true;
	}

private:
	nullary_func_t mCallable;
};

// Call a given callable once after specified interval.
void doAfterInterval(nullary_func_t callable, F32 seconds)
{
	new NullaryFuncEventTimer(callable, seconds);
}

class BoolFuncEventTimer : public LLEventTimer
{
public:
	BoolFuncEventTimer(bool_func_t callable, F32 seconds)
	:	LLEventTimer(seconds),
		mCallable(callable)
	{
	}

private:
	LL_INLINE bool tick()
	{
		return mCallable();
	}

private:
	bool_func_t mCallable;
};

// Call a given callable every specified number of seconds, until it returns true.
void doPeriodically(bool_func_t callable, F32 seconds)
{
	new BoolFuncEventTimer(callable, seconds);
}
