/**
 * @file lldate.cpp
 * @author Phoenix
 * @date 2006-02-05
 * @brief Implementation of the date class
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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
#include "lldate.h"

#include "apr_time.h"

#include <time.h>
#include <locale>
#include <iomanip>
#include <sstream>

#include "lltimer.h"
#include "llstring.h"

constexpr F64 DATE_EPOCH = 0.0;

// Should be APR_USEC_PER_SEC, but that relies on INT64_C which is not defined
// in glib under our build set up for some reason
constexpr F64 LL_APR_USEC_PER_SEC = 1000000.0;

LLDate::LLDate()
:	mSecondsSinceEpoch(DATE_EPOCH)
{
}

LLDate::LLDate(const LLDate& date)
:	mSecondsSinceEpoch(date.mSecondsSinceEpoch)
{
}

LLDate::LLDate(F64 seconds_since_epoch)
:	mSecondsSinceEpoch(seconds_since_epoch)
{
}

LLDate::LLDate(const std::string& iso8601_date)
{
	if (!fromString(iso8601_date))
	{
		llwarns << "date " << iso8601_date
				<< " failed to parse,  ZEROING IT OUT" << llendl;
		mSecondsSinceEpoch = DATE_EPOCH;
	}
}

std::string LLDate::asString() const
{
	std::ostringstream stream;
	toStream(stream);
	return stream.str();
}

// A more "human readable" timestamp format: same as ISO-8601, but with the "T"
// between date and time replaced with a space and the "Z" replaced with " UTC"
std::string LLDate::asTimeStamp(bool with_utc) const
{
	std::string timestr;
	if (with_utc)
	{
		timeToFormattedString((time_t)mSecondsSinceEpoch,
							   "%Y-%m-%d %H:%M:%S UTC", timestr);
	}
	else
	{
		timeToFormattedString((time_t)mSecondsSinceEpoch,
							   "%Y-%m-%d %H:%M:%S", timestr);
	}
	return timestr;
}

std::string LLDate::toHTTPDateString(const std::string& fmt) const
{
	time_t loc_seconds = (time_t)mSecondsSinceEpoch;
	return toHTTPDateString(gmtime(&loc_seconds), fmt.c_str());
}

std::string LLDate::toHTTPDateString(tm* gmt, const char* fmt)
{
	// Avoid calling setlocale() unnecessarily - it is expensive.
	static std::string prev_locale = "";
	std::string this_locale = LLStringUtil::getLocale();
	if (this_locale != prev_locale)
	{
		setlocale(LC_TIME, this_locale.c_str());
		prev_locale = this_locale;
	}

	// Use strftime() as it appears to be faster than std::time_put
	char buffer[128];
	strftime(buffer, 128, fmt, gmt);
	std::string res(buffer);
#if LL_WINDOWS
	// Convert from locale-dependant charset to UTF-8 (EXT-8524).
	res = ll_convert_string_to_utf8_string(res);
#endif
	return res;
}

void LLDate::toStream(std::ostream& s) const
{
	apr_time_t time = (apr_time_t)(mSecondsSinceEpoch * LL_APR_USEC_PER_SEC);

	apr_time_exp_t exp_time;
	if (apr_time_exp_gmt(&exp_time, time) != APR_SUCCESS)
	{
		s << "1970-01-01T00:00:00Z";
		return;
	}

	s << std::dec << std::setfill('0');
#if ( LL_WINDOWS || __GNUC__ > 2)
	s << std::right;
#else
	s.setf(ios::right);
#endif
	s		 << std::setw(4) << (exp_time.tm_year + 1900)
	  << '-' << std::setw(2) << (exp_time.tm_mon + 1)
	  << '-' << std::setw(2) << (exp_time.tm_mday)
	  << 'T' << std::setw(2) << (exp_time.tm_hour)
	  << ':' << std::setw(2) << (exp_time.tm_min)
	  << ':' << std::setw(2) << (exp_time.tm_sec);
	if (exp_time.tm_usec > 0)
	{
		s << '.' << std::setw(2)
		  << (int)(exp_time.tm_usec / (LL_APR_USEC_PER_SEC / 100));
	}
	s << 'Z' << std::setfill(' ');
}

bool LLDate::split(S32* year, S32* month, S32* day,
				   S32* hour, S32* min, S32* sec) const
{
	apr_time_t time = (apr_time_t)(mSecondsSinceEpoch * LL_APR_USEC_PER_SEC);

	apr_time_exp_t exp_time;
	if (apr_time_exp_gmt(&exp_time, time) != APR_SUCCESS)
	{
		return false;
	}

	if (year)
	{
		*year = exp_time.tm_year + 1900;
	}

	if (month)
	{
		*month = exp_time.tm_mon + 1;
	}

	if (day)
	{
		*day = exp_time.tm_mday;
	}

	if (hour)
	{
		*hour = exp_time.tm_hour;
	}

	if (min)
	{
		*min = exp_time.tm_min;
	}

	if (sec)
	{
		*sec = exp_time.tm_sec;
	}

	return true;
}

bool LLDate::fromString(const std::string& iso8601_date)
{
	std::istringstream stream(iso8601_date);
	return fromStream(stream);
}

bool LLDate::fromStream(std::istream& s)
{
	struct apr_time_exp_t exp_time;
	apr_int32_t tm_part;
	int c;

	s >> tm_part;
	exp_time.tm_year = tm_part - 1900;
	c = s.get(); // skip the hypen
	if (c != '-')
	{
		return false;
	}

	s >> tm_part;
	exp_time.tm_mon = tm_part - 1;
	c = s.get(); // skip the hypen
	if (c != '-')
	{
		return false;
	}

	s >> tm_part;
	exp_time.tm_mday = tm_part;

	c = s.get(); // skip the T
	if (c != 'T')
	{
		return false;
	}

	s >> tm_part;
	exp_time.tm_hour = tm_part;
	c = s.get(); // skip the :
	if (c != ':')
	{
		return false;
	}

	s >> tm_part;
	exp_time.tm_min = tm_part;
	c = s.get(); // skip the :
	if (c != ':')
	{
		return false;
	}

	s >> tm_part;
	exp_time.tm_sec = tm_part;

	// Zero out the unused fields
	exp_time.tm_usec = 0;
	exp_time.tm_wday = 0;
	exp_time.tm_yday = 0;
	exp_time.tm_isdst = 0;
	exp_time.tm_gmtoff = 0;

	// Generate a time_t from that
	apr_time_t time;
	if (apr_time_exp_gmt_get(&time, &exp_time) != APR_SUCCESS)
	{
		return false;
	}

	F64 seconds_since_epoch = time / LL_APR_USEC_PER_SEC;

	// Check for fractional
	c = s.peek();
	if (c == '.')
	{
		F64 fractional = 0.0;
		s >> fractional;
		seconds_since_epoch += fractional;
	}

	c = s.peek(); // Check for offset
	if (c == '+' || c == '-')
	{
		S32 offset_sign = (c == '+') ? 1 : -1;
		S32 offset_hours = 0;
		S32 offset_minutes = 0;
		S32 offset_in_seconds = 0;

		s >> offset_hours;

		c = s.get(); // Skip the colon a get the minutes if there are any
		if (c == ':')
		{
			s >> offset_minutes;
		}

		offset_in_seconds = (offset_hours * 60 +
							 offset_sign * offset_minutes) * 60;
		seconds_since_epoch -= offset_in_seconds;
	}
	else if (c != 'Z')
	{
		// Skip the Z
		return false;
	}

	mSecondsSinceEpoch = seconds_since_epoch;
	return true;
}

bool LLDate::fromYMDHMS(S32 year, S32 month, S32 day,
						S32 hour, S32 min, S32 sec)
{
	struct apr_time_exp_t exp_time;

	exp_time.tm_year = year - 1900;
	exp_time.tm_mon = month - 1;
	exp_time.tm_mday = day;
	exp_time.tm_hour = hour;
	exp_time.tm_min = min;
	exp_time.tm_sec = sec;

	// Zero out the unused fields
	exp_time.tm_usec = 0;
	exp_time.tm_wday = 0;
	exp_time.tm_yday = 0;
	exp_time.tm_isdst = 0;
	exp_time.tm_gmtoff = 0;

	// Generate a time_t from that
	apr_time_t time;
	if (apr_time_exp_gmt_get(&time, &exp_time) != APR_SUCCESS)
	{
		return false;
	}

	mSecondsSinceEpoch = time / LL_APR_USEC_PER_SEC;

	return true;
}

//static
LLDate LLDate::now()
{
	return LLDate(LLTimer::getEpochSeconds());
}

bool LLDate::operator<(const LLDate& rhs) const
{
    return mSecondsSinceEpoch < rhs.mSecondsSinceEpoch;
}

std::ostream& operator<<(std::ostream& s, const LLDate& date)
{
	date.toStream(s);
	return s;
}

std::istream& operator>>(std::istream& s, LLDate& date)
{
	date.fromStream(s);
	return s;
}
