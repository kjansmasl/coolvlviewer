/**
 * @file llworkqueue.cpp
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

#include "linden_common.h"

#include "llworkqueue.h"

#include "llatomic.h"

void LLWorkQueue::runUntilClose()
{
	try
	{
		while (true)
		{
			callWork(mQueue.pop());
			if (mQueue.empty())
			{
				LLThread::yield();
			}
		}
	}
	catch (const Closed&)
	{
	}
}

bool LLWorkQueue::runPending()
{
	try
	{
		for (Work work; mQueue.tryPop(work); )
		{
			callWork(work);
		}
	}
	catch (const Closed&)
	{
	}
	return !mQueue.done();
}

bool LLWorkQueue::runOne()
{
	try
	{
		Work work;
		if (mQueue.tryPop(work))
		{
			callWork(work);
		}
	}
	catch (const Closed&)
	{
	}
	return !mQueue.done();
}

bool LLWorkQueue::runUntil(const TimePoint& until, size_t* work_remaining)
{
	try
	{
		// Should we subtract some slop to allow for typical Work execution
		// time and how much slop ?
		for (Work work; TimePoint::clock::now() < until && mQueue.tryPop(work); )
		{
			callWork(work);
		}
	}
	catch (const Closed&)
	{
	}
	return !mQueue.done(work_remaining);
}

void LLWorkQueue::callWork(const Work& work)
{
	try
	{
		work();
	}
	catch (...)
	{
		// No matter what goes wrong with any individual work item, the worker
		// thread must go on !... Log our own instance name with the exception.
		llwarns << "Work failed for: " << getKey() << llendl;
	}
}

//static
std::string LLWorkQueue::makeName(const std::string& name)
{
	if (!name.empty())
	{
		return name;
	}

	// We use an atomic static variable to avoid bothering with mutex and
	// locks. HB
	static LLAtomicU32 discriminator(0);

	U32 num = discriminator++;
	return llformat("WorkQueue%d", num);
}

//static
void LLWorkQueue::error(const std::string& msg)
{
	llerrs << msg << llendl;
}

#if LL_WAIT_FOR_RESULT
//static
void LLWorkQueue::checkCoroutine(const std::string& method)
{
	// By convention, the default coroutine on each thread has an empty name
	// string.
	if (LLCoros::getName().empty())
	{
		throw(Error("Do not call " + method +
					" from a thread's default coroutine"));
	}
}
#endif
