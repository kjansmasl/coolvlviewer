/**
 * @file lltimer.h
 * @brief Cross-platform objects for doing timing
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_TIMER_H
#define LL_TIMER_H

#if LL_LINUX || LL_DARWIN
#include <sys/time.h>
#endif
#include <limits.h>
#include <list>
#include <string>

#include "llcommonmath.h"
#include "llerror.h"

// Time units conversions
#ifndef USEC_PER_SEC
constexpr U32 USEC_PER_SEC			= 1000000;
#endif
constexpr U32 SEC_PER_MIN			= 60;
constexpr U32 MIN_PER_HOUR			= 60;
constexpr U32 USEC_PER_MIN			= USEC_PER_SEC * SEC_PER_MIN;
constexpr U32 SEC_PER_DAY			= 86400;
constexpr U32 USEC_PER_HOUR			= USEC_PER_MIN * MIN_PER_HOUR;
constexpr U32 SEC_PER_HOUR			= SEC_PER_MIN * MIN_PER_HOUR;
constexpr F64 SEC_PER_USEC 			= 1.0 / (F64)USEC_PER_SEC;
constexpr F32 SEC_TO_MICROSEC		= 1000000.f;
constexpr U64 SEC_TO_MICROSEC_U64	= 1000000;

class LLTimer
{
protected:
	LOG_CLASS(LLTimer);

public:
	LLTimer();

	static void initClass();
	static void cleanupClass();

	// Returns the number of seconds elapsed since UNIX epoch with a milli-
	// second resolution.
	static F64 getEpochSeconds();

	static U64 getCurrentClockCount();		// Returns the raw clockticks

	// Returns a high precision micro-seconds time (usually since computer boot
	// up time).
	static U64 totalTime();

	// Returns a high precision seconds time (usually since computer boot up
	// time).
	LL_INLINE static F64 getTotalSeconds()
	{
		constexpr F64 USEC_TO_SEC_F64 = 0.000001;
		return U64_to_F64(totalTime()) * USEC_TO_SEC_F64;
	}

	// Returns a high precision number of seconds since the start of this
	// application instance.
	LL_INLINE static F64 getElapsedSeconds()
	{
		return sTimer->getElapsedTimeF64();
	}

	LL_INLINE void start()						{ reset(); mStarted = true; }
	LL_INLINE void stop()						{ mStarted = false; }

	LL_INLINE void reset()
	{
		mLastClockCount = getCurrentClockCount();
		mExpirationTicks = 0;
	}

	// Sets the timer so that the next elapsed call will be relative to this
	// time:
	LL_INLINE void setLastClockCount(U64 current_count)
	{
		mLastClockCount = current_count;
	}

	void setTimerExpirySec(F32 expiration);
	bool checkExpirationAndReset(F32 expiration);
	LL_INLINE bool hasExpired() const			{ return getCurrentClockCount() >= mExpirationTicks; }
	LL_INLINE bool getStarted() const			{ return mStarted; }

	// These methods return the elapsed time in seconds
	F64 getElapsedTimeF64() const;
	LL_INLINE F32 getElapsedTimeF32() const		{ return (F32)getElapsedTimeF64(); }

	// These methods return the remaining time in seconds
	F64 getRemainingTimeF64() const;
	LL_INLINE F32 getRemainingTimeF32() const	{ return (F32)getRemainingTimeF64(); }

	// These methods return the elapsed time in seconds and reset the timer
	F64 getElapsedTimeAndResetF64();
	LL_INLINE F32 getElapsedTimeAndResetF32()	{ return (F32)getElapsedTimeAndResetF64(); }

private:
	static U64 getElapsedTimeAndUpdate(U64& last_clock_count);

public:
	static LLTimer*	sTimer;					// Global timer

protected:
	U64				mLastClockCount;
	U64				mExpirationTicks;
	bool			mStarted;
};

//
// Various functions for initializing/accessing clock and timing stuff. Do not
// use these without REALLY knowing how they work.
//

void update_clock_frequencies();

// Sleep for milliseconds
void ms_sleep(U32 ms);

// Returns the correct UTC time in seconds, like time(NULL).
// Useful on the viewer, which may have its local clock set wrong.
time_t time_corrected();

// Returns the computer (local) time in seconds, like time(NULL).
time_t computer_time();

static LL_INLINE time_t time_min()
{
	if (sizeof(time_t) == 4)
	{
		return (time_t) INT_MIN;
	}
	else
	{
#ifdef LLONG_MIN
		return (time_t) LLONG_MIN;
#else
		return (time_t) LONG_MIN;
#endif
	}
}

static LL_INLINE time_t time_max()
{
	if (sizeof(time_t) == 4)
	{
		return (time_t) INT_MAX;
	}
	else
	{
#ifdef LLONG_MAX
		return (time_t) LLONG_MAX;
#else
		return (time_t) LONG_MAX;
#endif
	}
}

// Correction factor used by time_corrected() above.
extern S32 gUTCOffset;

// Converts internal "struct tm" time buffer to UTC
// Usage:
// S32 utc_time;
// utc_time = time_corrected();
// struct tm* internal_time = utc_time_to_tm(utc_time);
struct tm* utc_time_to_tm(time_t utc_time);

// Converts internal "struct tm" time buffer to local time
// Usage:
// S32 local_time;
// local_time = computer_time();
// struct tm* internal_time = local_time_to_tm(local_time);
struct tm* local_time_to_tm(time_t local_time);

// Converts internal "struct tm" time buffer to Pacific Standard/Daylight Time
// Usage:
// S32 utc_time;
// utc_time = time_corrected();
// struct tm* internal_time = utc_to_pacific_time(utc_time, gDaylight);
struct tm* utc_to_pacific_time(time_t utc_time, bool pacific_daylight_time);

void microsecondsToTimecodeString(U64 current_time, std::string& tcstring);
void secondsToTimecodeString(F32 current_time, std::string& tcstring);
void timeToFormattedString(time_t time, const char* format,
						   std::string& timestr);
void timeStructToFormattedString(struct tm* time, const std::string& format,
								 std::string& timestr);

#endif
