/**
 * @file lltimer.cpp
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

#include "linden_common.h"

#if !LL_WINDOWS
# include <errno.h>
# include <time.h>
# include <unistd.h>
# if LL_DARWIN
#  include <sys/time.h>
# endif
#endif

#include "lltimer.h"

//---------------------------------------------------------------------------
// Globals and static variables
//---------------------------------------------------------------------------

// Viewer's offset from server UTC, in seconds
S32 gUTCOffset = 0;

static F64 sClockFrequency = 0.0;
static F64 sClockFrequencyInv = 0.0;
static F64 sClocksToMicroseconds = 0.0;
static U64 sTotalTimeClockCount = 0;
static U64 sLastTotalTimeClockCount = 0;
static U64 sLastClockDelta = 0;

LLTimer* LLTimer::sTimer = NULL;

// This is the amount of time (one bisextile year in micro seconds) we allow
// for the system clock to be set backwards while the viewer is ruunning.
constexpr U64 ONE_YEAR_MSEC = U64(366 * 24 * 3600) * SEC_TO_MICROSEC_U64;

// Helper function

void update_clock_frequencies()
{
#if LL_WINDOWS
	__int64 freq;
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
	sClockFrequency = (F64)freq;
#else
	// Both Linux and Mac use gettimeofday() for accurate time
	sClockFrequency = 1000000.0; // microseconds, so 1 MHz.
#endif
	sClockFrequencyInv = 1.0 / sClockFrequency;
	sClocksToMicroseconds = sClockFrequencyInv * SEC_TO_MICROSEC;
}

//---------------------------------------------------------------------------
// LLTimer implementation
//---------------------------------------------------------------------------

//static
void LLTimer::initClass()
{
	if (!sTimer)
	{
		sTimer = new LLTimer;
	}
}

//static
void LLTimer::cleanupClass()
{
	if (sTimer)
	{
		delete sTimer;
		sTimer = NULL;
	}
}

LLTimer::LLTimer()
{
	if (!sClockFrequency)
	{
		update_clock_frequencies();
	}

	mStarted = true;
	reset();
}

// Returns a seconds count since UNIX epoch, with a milli-second resolution.
// This method is slower (under Linux or Windows, at least) than the other
// methods below (such as getCurrentClockCount()), but the latter may return
// (under Linux or Windows) a "random" time, which is usually the time ellapsed
// since the computer booted up.
// This method is used by LLDate::now() and the environment "time of day"
// implementations. It is NOT suitable for high accurracy or high resolution
// applications.
//static
F64 LLTimer::getEpochSeconds()
{
#if LL_WINDOWS
	static F64 offset = 0.0;
	static F64 last_update = 0.0;
	LARGE_INTEGER clock;

	if (last_update == 0.0)
	{
		// We need to update the offset from Epoch for performance counter...
		update_clock_frequencies();	// Make sure sClockFrequencyInv is set
		// Get the number of *100ns* ticks since January 1st, *1601* UTC.
		// Using GetSystemTimeAsFileTime() is more complex but way more
		// accurate than using time() (which is a 1 second resolution timer).
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);
		// Get the current performance counter value
		QueryPerformanceCounter(&clock);
		// Compute the current time in seconds since Epoch
		ULARGE_INTEGER ft_value;
		ft_value.LowPart = ft.dwLowDateTime;
		ft_value.HighPart = ft.dwHighDateTime;
		constexpr F64 SEC_PER_100NS = 1.0 / 10000000.0;
		constexpr U64 EPOCH_DELTA = 116444736000000000UL;
		last_update = F64(ft_value.QuadPart - EPOCH_DELTA) * SEC_PER_100NS;
		// And here is our offset...
		offset = last_update - F64(clock.QuadPart) * sClockFrequencyInv;
	}
	else
	{
		// Get the current performance counter value
		QueryPerformanceCounter(&clock);
	}

	// Compute time from performance counter value and Epoch offset, in seconds
	F64 now = F64(clock.QuadPart) * sClockFrequencyInv + offset;

	// Resync every 3 minutes or so, in case the computer clock would be
	// changed (manually or automatically, e.g. via NTP).
	if (now - last_update > 360.0)
	{
		last_update = 0.0;
	}

	return now;
#else	// LL_LINUX || LL_DARWIN
	// UNIX/BSD clocks are in microseconds
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return F64(tv.tv_sec) + F64(tv.tv_usec) * SEC_PER_USEC;
#endif
}

//static
U64 LLTimer::getCurrentClockCount()
{
#if LL_LINUX
	struct timespec tv;
# ifdef CLOCK_MONOTONIC_RAW	// if MONOTONIC_RAW supported at build-time
	// Try and use a clock that is unaffected by ntp and user-triggered system
	// time changes... If MONOTONIC_RAW is supported at runtime (i.e. when
	// running on a kernel from Linux 2.6.28 onwards).
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &tv) == -1)
# endif
	{
		// If MONOTONIC_RAW is not supported, then use REALTIME
	    clock_gettime(CLOCK_REALTIME, &tv);
	}
	return tv.tv_sec * SEC_TO_MICROSEC_U64 + tv.tv_nsec / 1000;
#elif LL_WINDOWS
	// Ensure that callers to this method never have to deal with wrap !
	static bool first_time = true;
	static U64 offset;
	// QueryPerformanceCounter implementation
	LARGE_INTEGER clock_count;
	QueryPerformanceCounter(&clock_count);
	if (first_time)
	{
		offset = clock_count.QuadPart;
		first_time = false;
	}
	return clock_count.QuadPart - offset;
#else	// LL_DARWIN or any other UNIX/BSD-like OS
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * SEC_TO_MICROSEC_U64 + tv.tv_usec;
#endif
}

// Returns a high precision usec since computer boot up time
//static
U64 LLTimer::totalTime()
{
	U64 cur_clock_count = getCurrentClockCount();
	if (!sTotalTimeClockCount)	// First call ?
	{
		update_clock_frequencies();
		sLastClockDelta = cur_clock_count;
	}
	// Time not going backward or counter wrapping, we are all okay.
	else if (cur_clock_count >= sLastTotalTimeClockCount)
	{
		sLastClockDelta = cur_clock_count - sLastTotalTimeClockCount;
	}
	// Allow setting the system time backwards by one year; an actual wrapping
	// would yield a much larger delta anyway...
	else if (cur_clock_count + ONE_YEAR_MSEC > sLastTotalTimeClockCount)
	{
		// It is a pretty common occurrence that we get 1 or 2 ticks backwards
		// on some systems, so do not spam the log with this.
		LL_DEBUGS("Timer") << "Clock count went backwards. Last clock count = "
						   << sLastTotalTimeClockCount
						   << " - New clock count = " << cur_clock_count
						   << " - Using last clock delta as an estimation of ellapsed time: "
						   << sLastClockDelta << LL_ENDL;
		// Use previous clock delta as an estimation...
	}
	// We must have wrapped. Compensate accordingly.
	else
	{
		llwarns << "Clock count wrapping detected. Last clock count = "
				<< sLastTotalTimeClockCount << " - New clock count = "
				<< cur_clock_count << llendl;
		sLastClockDelta = (0xFFFFFFFFFFFFFFFFULL - sLastTotalTimeClockCount) +
						  cur_clock_count;;
	}
	sTotalTimeClockCount += sLastClockDelta;

	// Update the last clock count
	sLastTotalTimeClockCount = cur_clock_count;

	// Return the total clock tick count in microseconds.
	return (U64)(sTotalTimeClockCount * sClocksToMicroseconds);
}

F64 LLTimer::getElapsedTimeF64() const
{
	U64 last = mLastClockCount;
	return (F64)getElapsedTimeAndUpdate(last) * sClockFrequencyInv;
}

//static
U64 LLTimer::getElapsedTimeAndUpdate(U64& last_clock_count)
{
	U64 cur_clock_count = getCurrentClockCount();
	U64 result;

	if (cur_clock_count >= last_clock_count)
	{
		result = cur_clock_count - last_clock_count;
	}
	else
	{
		// Time has gone backward
		result = 0;
	}

	last_clock_count = cur_clock_count;

	return result;
}

F64 LLTimer::getElapsedTimeAndResetF64()
{
	return (F64)getElapsedTimeAndUpdate(mLastClockCount) * sClockFrequencyInv;
}

void LLTimer::setTimerExpirySec(F32 expiration)
{
	mExpirationTicks = getCurrentClockCount() +
					   (U64)((F32)(expiration * sClockFrequency));
}

F64 LLTimer::getRemainingTimeF64() const
{
	U64 cur_ticks = getCurrentClockCount();
	if (cur_ticks > mExpirationTicks)
	{
		return 0.0;
	}
	return F64(mExpirationTicks - cur_ticks) * sClockFrequencyInv;
}

bool LLTimer::checkExpirationAndReset(F32 expiration)
{
	U64 cur_ticks = getCurrentClockCount();
	if (cur_ticks < mExpirationTicks)
	{
		return false;
	}

	mExpirationTicks = cur_ticks + (U64)((F32)(expiration * sClockFrequency));
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// NON-MEMBER FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

void ms_sleep(U32 ms)
{
#if LL_WINDOWS
	// The Sleep() function is way too inaccurate, and already sleeps for
	// longer than a ms... So let's not make things worst and just "relinquish
	// the remainder of our time slice" (as documented by Microsoft) when
	// requesting less than a 3ms sleep time. HB
	if (ms < 3)
	{
		ms = 0;
	}
	Sleep(ms);
#else
	usleep(1000 * ms);
#endif
}

time_t time_corrected()
{
	return time(NULL) + gUTCOffset;
}

time_t computer_time()
{
	return time(NULL);
}

struct tm* utc_time_to_tm(time_t utc_time)
{
	struct tm* internal_time = gmtime(&utc_time);
	return internal_time;
}

struct tm* local_time_to_tm(time_t local_time)
{
	struct tm* internal_time = localtime(&local_time);
	return internal_time;
}

struct tm* utc_to_pacific_time(time_t utc_time, bool pacific_daylight_time)
{
	S32 pacific_offset_hours;
	if (pacific_daylight_time)
	{
		pacific_offset_hours = 7;
	}
	else
	{
		pacific_offset_hours = 8;
	}

	// We subtract off the PST/PDT offset _before_ getting "UTC" time, because
	// this will handle wrapping around for 5 AM UTC -> 10 PM PDT of the
	// previous day.
	utc_time -= pacific_offset_hours * MIN_PER_HOUR * SEC_PER_MIN;

	// Internal buffer to PST/PDT (see above)
	struct tm* internal_time = gmtime(&utc_time);

#if 0	// Do not do this, this would not correctly tell you if daylight
		// savings is active in CA or not.
	if (pacific_daylight_time)
	{
		internal_time->tm_isdst = 1;
	}
#endif

	return internal_time;
}

void microsecondsToTimecodeString(U64 current_time, std::string& tcstring)
{
	U64 hours = current_time / (U64)3600000000ul;
	U64 minutes = current_time / (U64)60000000;
	minutes %= 60;
	U64 seconds = current_time / (U64)1000000;
	seconds %= 60;
	U64 frames = current_time / (U64)41667;
	frames %= 24;
	U64 subframes = current_time / (U64)42;
	subframes %= 100;

	tcstring = llformat("%3.3d:%2.2d:%2.2d:%2.2d.%2.2d",
						(int)hours, (int)minutes, (int)seconds,
						(int)frames, (int)subframes);
}

void secondsToTimecodeString(F32 current_time, std::string& tcstring)
{
	microsecondsToTimecodeString((U64)((F64)(SEC_TO_MICROSEC * current_time)),
								 tcstring);
}

void timeToFormattedString(time_t time, const char* format,
						   std::string& timestr)
{
	char buffer[256];
	struct tm* t = localtime(&time);
	strftime(buffer, 255, format, t);
	timestr = (const char*)buffer;
}

void timeStructToFormattedString(struct tm* time, const std::string& format,
								 std::string& timestr)
{
	char buffer[256];
	strftime(buffer, 255, format.c_str(), time);
	timestr = (const char*)buffer;
}
