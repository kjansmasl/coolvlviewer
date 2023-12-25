/**
 * @file llhttpretrypolicy.cpp
 * @brief Implementation of the http retry policy class.
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
 *
 * Copyright (c) 2013, Linden Research, Inc.
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

// for curl_getdate() (apparently parsing RFC 1123 dates is hard)
#include "curl/curl.h"

#include "llhttpretrypolicy.h"

#include "llhttpconstants.h"

LLAdaptiveRetryPolicy::LLAdaptiveRetryPolicy(F32 min_delay,
											 F32 max_delay,
											 F32 backoff_factor,
											 U32 max_retries,
											 bool retry_on_4xx)
:	mMinDelay(min_delay),
	mMaxDelay(max_delay),
	mBackoffFactor(backoff_factor),
	mMaxRetries(max_retries),
	mRetryOn4xx(retry_on_4xx)
{
	init();
}

void LLAdaptiveRetryPolicy::init()
{
	mDelay = mMinDelay;
	mRetryCount = 0;
	mShouldRetry = true;
}

void LLAdaptiveRetryPolicy::reset()
{
	init();
}

bool LLAdaptiveRetryPolicy::getRetryAfter(const LLSD& headers,
										  F32& retry_header_time)
{
	return headers.has(HTTP_IN_HEADER_RETRY_AFTER) &&
		   getSecondsUntilRetryAfter(headers[HTTP_IN_HEADER_RETRY_AFTER].asStringRef(),
									 retry_header_time);
}

bool LLAdaptiveRetryPolicy::getRetryAfter(const LLCore::HttpHeaders::ptr_t& headers,
										  F32& retry_header_time)
{
	if (headers)
	{
		const std::string* retry_value =
			headers->find(HTTP_IN_HEADER_RETRY_AFTER.c_str());
		if (retry_value &&
			getSecondsUntilRetryAfter(*retry_value, retry_header_time))
		{
			return true;
		}
	}
	return false;
}

void LLAdaptiveRetryPolicy::onSuccess()
{
	init();
}

void LLAdaptiveRetryPolicy::onFailure(S32 status, const LLSD& headers)
{
	F32 retry_header_time = 0.f;
	bool has_retry_header_time = getRetryAfter(headers, retry_header_time);
	onFailureCommon(status, has_retry_header_time, retry_header_time);
}

void LLAdaptiveRetryPolicy::onFailure(const LLCore::HttpResponse* response)
{
	if (!response) return;
	F32 retry_header_time = 0.f;
	const LLCore::HttpHeaders::ptr_t headers = response->getHeaders();
	bool has_retry_header_time = getRetryAfter(headers, retry_header_time);
	onFailureCommon(response->getStatus().getType(), has_retry_header_time,
					retry_header_time);
}

void LLAdaptiveRetryPolicy::onFailureCommon(S32 status,
											bool has_retry_header_time,
											F32 retry_header_time)
{
	if (!mShouldRetry)
	{
		llinfos << "Keep on failing..." << llendl;
		return;
	}
	if (mRetryCount > 0)
	{
		mDelay = llclamp(mDelay * mBackoffFactor, mMinDelay, mMaxDelay);
	}
	// Honor server Retry-After header. Status 503 may ask us to wait for a
	// certain amount of time before retrying.
	F32 wait_time = mDelay;
	if (has_retry_header_time)
	{
		wait_time = retry_header_time;
	}

	if (mRetryCount >= mMaxRetries)
	{
		llwarns << "Too many retries " << mRetryCount << ", aborting."
				<< llendl;
		mShouldRetry = false;
	}
	if (!mRetryOn4xx && !isHttpServerErrorStatus(status))
	{
		llwarns << "Non-server error " << status << ", aborting." << llendl;
		mShouldRetry = false;
	}
	if (mShouldRetry)
	{
		llinfos << "Retry count: " << mRetryCount << ". Will retry after "
				<< wait_time << "s." << llendl;
		mRetryTimer.reset();
		mRetryTimer.setTimerExpirySec(wait_time);
	}
	++mRetryCount;
}

bool LLAdaptiveRetryPolicy::shouldRetry(F32& seconds_to_wait) const
{
	if (mRetryCount == 0)
	{
		// Called shouldRetry before any failure.
		seconds_to_wait = F32_MAX;
		return false;
	}
	seconds_to_wait = mShouldRetry ? mRetryTimer.getRemainingTimeF32()
								   : F32_MAX;
	return mShouldRetry;
}

// Parses 'Retry-After' header contents and returns seconds until retry should
// occur.
//static
bool LLAdaptiveRetryPolicy::getSecondsUntilRetryAfter(const std::string& retry_after,
													  F32& seconds_to_wait)
{
	// *TODO: This needs testing !  Not in use yet. Examples of Retry-After
	// headers: Retry-After: Fri, 31 Dec 1999 23:59:59 GMT
	//          Retry-After: 120

	// Check for number of seconds version, first:
	char* end = 0;
	// Parse as double
	double seconds = std::strtod(retry_after.c_str(), &end);
	if (end != 0 && *end == 0)
	{
		// Successful parse
		seconds_to_wait = (F32)seconds;
		return true;
	}

	// Parse rfc1123 date.
	time_t date = curl_getdate(retry_after.c_str(), NULL);
	if (date == -1)
	{
		return false;
	}

	seconds_to_wait = (F64)date - LLTimer::getTotalSeconds();

	return true;
}
