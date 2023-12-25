/**
 * @file llhttpretrypolicy.h
 * @brief Declarations for http retry policy class.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
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

#ifndef LL_RETRYPOLICY_H
#define LL_RETRYPOLICY_H

#include "llcorehttpheaders.h"
#include "llcorehttpresponse.h"
#include "llerror.h"
#include "llthread.h"
#include "lltimer.h"

// This is intended for use with HTTP Clients/Responders, but is not
// specifically coupled with those classes.
class LLHTTPRetryPolicy : public LLThreadSafeRefCount
{
public:
	LLHTTPRetryPolicy()				{}

	~LLHTTPRetryPolicy() override	{}

	// Call after a sucess to reset retry state.
	virtual void onSuccess() = 0;

	// Call once after an HTTP failure to update state.
	virtual void onFailure(S32 status, const LLSD& headers) = 0;
	virtual void onFailure(const LLCore::HttpResponse* response) = 0;

	virtual bool shouldRetry(F32& seconds_to_wait) const = 0;

	virtual void reset() = 0;
};

// Very general policy with geometric back-off after failures, up to a maximum
// delay, and maximum number of retries.
class LLAdaptiveRetryPolicy : public LLHTTPRetryPolicy
{
protected:
	LOG_CLASS(LLAdaptiveRetryPolicy);

public:
	LLAdaptiveRetryPolicy(F32 min_delay, F32 max_delay, F32 backoff_factor,
						  U32 max_retries, bool retry_on_4xx = false);

	void onSuccess() override;

	void onFailure(S32 status, const LLSD& headers) override;
	void onFailure(const LLCore::HttpResponse* response) override;

	bool shouldRetry(F32& seconds_to_wait) const override;

	void reset() override;

	static bool getSecondsUntilRetryAfter(const std::string& retry_after,
										  F32& seconds_to_wait);

protected:
	void init();

	bool getRetryAfter(const LLSD& headers, F32& retry_header_time);
	bool getRetryAfter(const LLCore::HttpHeaders::ptr_t& headers,
					   F32& retry_header_time);

	void onFailureCommon(S32 status, bool has_retry_header_time,
						 F32 retry_header_time);

private:
	const F32	mMinDelay;		// Delay never less than this value
	const F32	mMaxDelay;		// Delay never exceeds this value

	// Delay increases by this factor after each retry, up to mMaxDelay.
	const F32	mBackoffFactor;

	// Maximum number of times shouldRetry will return true.
	const U32	mMaxRetries;

	F32			mDelay;			// Current default delay.
	U32			mRetryCount;	// Number of times shouldRetry has been called.
	LLTimer		mRetryTimer;	// Time until next retry.

	// Becomes false after too many retries, or the wrong sort of status
	// received etc.
	bool		mShouldRetry;

	bool		mRetryOn4xx;	// Normally only retry on 5xx server errors.
};

#endif
