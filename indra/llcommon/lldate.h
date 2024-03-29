/**
 * @file lldate.h
 * @author Phoenix
 * @date 2006-02-05
 * @brief Declaration of a simple date class.
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

#ifndef LL_LLDATE_H
#define LL_LLDATE_H

#include <iosfwd>
#include <string>

#include "llerror.h"
#include "stdtypes.h"

// class represents a particular point in time in UTC. The date class
// represents a point in time after epoch (1970-01-01 00:00:00 UTC).
class LLDate
{
protected:
	LOG_CLASS(LLDate);

public:
	// Constructs a date equal to the UTC epoch start date.
	LLDate();

	// Constructs a date equal to the source date.
	LLDate(const LLDate& date);

	// Constructs a date from a number of seconds since the UTC epoch value.
	LLDate(F64 seconds_since_epoch);

	// Constructs a date from a string representation
	// The date is constructed in the fromString() method. See that method for
	// details of supported formats. If that method fails to parse the date,
	// the date is set to epoch.
	LLDate(const std::string& iso8601_date);

	// Returns the date as in ISO-8601 string.
	std::string asString() const;

	// A more "human readable" timestamp format: same as ISO-8601, but with the
	// "T" between date and time replaced with a space and the "Z" replaced
	// with " UTC"
	std::string asTimeStamp(bool with_utc = true) const;

	void toStream(std::ostream&) const;
	bool split(S32* year, S32* month = NULL, S32* day = NULL,
			   S32* hour = NULL, S32* min = NULL, S32* sec = NULL) const;
	std::string toHTTPDateString(const std::string& fmt) const;
	static std::string toHTTPDateString(tm* gmt, const char* fmt);

	// These two methods set the date from an ISO-8601 string. The parser only
	// supports strings conforming to YYYYF-MM-DDTHH:MM:SS.FFZ where Y is year,
	// M is month, D is day, H is hour, M is minute, S is second, F is sub-
	// second, and all other characters are literal. If these method fail to
	// parse the date, the previous date is retained. Reurn true if the string
	// was successfully parsed.
	bool fromString(const std::string& iso8601_date);
	bool fromStream(std::istream&);

	bool fromYMDHMS(S32 year, S32 month = 1, S32 day = 0, S32 hour = 0,
					S32 min = 0, S32 sec = 0);

	// Returns the date in seconds since epoch.
	LL_INLINE F64 secondsSinceEpoch() const				{ return mSecondsSinceEpoch; }

	// Sets the date in seconds since epoch.
	LL_INLINE void secondsSinceEpoch(F64 seconds)		{ mSecondsSinceEpoch = seconds; }

	// Creates a LLDate object set to the current time.
	static LLDate now();

	// Compares dates using operator< so we can order them using STL.
	bool operator<(const LLDate& rhs) const;

	// Remaining comparison operators in terms of operator<
	// This conforms to the expectation of STL.
	LL_INLINE bool operator>(const LLDate& rhs) const	{ return rhs < *this; }
	LL_INLINE bool operator<=(const LLDate& rhs) const	{ return !(rhs < *this); }
	LL_INLINE bool operator>=(const LLDate& rhs) const	{ return !(*this < rhs); }
	LL_INLINE bool operator!=(const LLDate& rhs) const	{ return *this < rhs || rhs < *this; }
	LL_INLINE bool operator==(const LLDate& rhs) const	{ return !(*this != rhs); }

	// Compare to epoch UTC.
	LL_INLINE bool isNull() const						{ return mSecondsSinceEpoch == 0.0; }
	LL_INLINE bool notNull() const						{ return mSecondsSinceEpoch != 0.0; }

private:
	F64 mSecondsSinceEpoch;
};

// Helper function to stream out a date
std::ostream& operator<<(std::ostream& s, const LLDate& date);

// Helper function to stream in a date
std::istream& operator>>(std::istream& s, LLDate& date);

#endif // LL_LLDATE_H
