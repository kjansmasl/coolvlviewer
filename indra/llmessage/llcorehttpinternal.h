/**
 * @file llcorehttpinternal.h
 * @brief Implementation constants and magic numbers
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef	_LLCORE_HTTP_INTERNAL_H_
#define	_LLCORE_HTTP_INTERNAL_H_

// If you find this included in a public interface header, something wrong is
// probably happening.

// --------------------------------------------------------------------
// General library to-do list
//
// - Implement policy classes. Structure is mostly there just did not need it
//   for the first consumer (classes are there but more advanced features, like
//   borrowing, are not there yet).
// - Set/get for global policy and policy classes is clumsy. Rework it heading
//   in a direction that allows for more dynamic behavior (mostly fixed).
// - Move HttpOpRequest::prepareRequest() to HttpLibcurl for the pedantic.
// - Update downloader and other long-duration services are going to need a
//   progress notification. Initial idea is to introduce a 'repeating request'
//   which can piggyback on another request and persist until cancelled or
//   carrier completes. Current queue structures allow an HttpOperation object
//   to be enqueued repeatedly, so...
// - Investigate making c-ares' re-implementation of a resolver library more
//   resilient or more intelligent on Mac. Part of the DNS failure lies in
//   here. The mechanism also looks a little less dynamic than needed in an
//   environments where networking is changing.
// - Global optimizations: 'borrowing' connections from other classes.
// - Dynamic/control system stuff: detect problems and self-adjust. This would
//   not help in the face of the router problems we have looked at, however.
//   Detect starvation due to UDP activity and provide feedback to it.
// - Change the transfer timeout scheme. We are less interested in absolute
//   time, in most cases, than in continuous progress.
// - Many of the policy class settings are currently applied to the entire
//   class. Some, like connection limits, would be better applied to each
//   destination target making multiple targets independent.
// --------------------------------------------------------------------

namespace LLCore
{

// Maxium number of policy classes that can be defined.
// *TODO:  Currently limited to the default class + 1, extend.
// TSN: should this be more dynamically sized ?  Is there a reason to hard
// limit the number of policies ?
constexpr int HTTP_POLICY_CLASS_LIMIT = 32;

// Debug/informational tracing. Used both as a global option and in per-request
// traces.
constexpr long HTTP_TRACE_OFF = 0;
constexpr long HTTP_TRACE_LOW = 1;
constexpr long HTTP_TRACE_CURL_HEADERS = 2;
constexpr long HTTP_TRACE_CURL_BODIES = 3;

constexpr long HTTP_TRACE_MIN = HTTP_TRACE_OFF;
constexpr long HTTP_TRACE_MAX = HTTP_TRACE_CURL_BODIES;

// Request retry limits
// At a minimum, retries need to extend past any throttling window we're
// expecting from central services. In the case of Linden services running
// through the caps routers, there's a five-second or so window for
// throttling with some spillover. We want to span a few windows to allow
// transport to slow after onset of the throttles and then recover without a
// final failure. Other systems may need other constants.
constexpr int HTTP_RETRY_COUNT_DEFAULT = 8;
constexpr int HTTP_RETRY_COUNT_MIN = 0;
constexpr int HTTP_RETRY_COUNT_MAX = 100;

constexpr int HTTP_REDIRECTS_DEFAULT = 10;

// Timeout value used for both connect and protocol exchange. Retries and
// time-on-queue are not included and are not accounted for.
constexpr long HTTP_REQUEST_TIMEOUT_DEFAULT = 30L;
constexpr long HTTP_REQUEST_XFER_TIMEOUT_DEFAULT = 0L;
constexpr long HTTP_REQUEST_TIMEOUT_MIN = 0L;
constexpr long HTTP_REQUEST_TIMEOUT_MAX = 3600L;

// Limits on connection counts
constexpr int HTTP_CONNECTION_LIMIT_DEFAULT = 8;
constexpr int HTTP_CONNECTION_LIMIT_MIN = 1;
constexpr int HTTP_CONNECTION_LIMIT_MAX = 256;

// Pipelining limits
constexpr long HTTP_PIPELINING_DEFAULT = 0L;
constexpr long HTTP_PIPELINING_MAX = 20L;

// Miscellaneous defaults
constexpr bool HTTP_USE_RETRY_AFTER_DEFAULT = true;
constexpr long HTTP_THROTTLE_RATE_DEFAULT = 0L;

// Tuning parameters

// Time worker thread sleeps after a pass through the request, ready and active
// queues.
constexpr int HTTP_SERVICE_LOOP_SLEEP_NORMAL_MS = 2;
}

#endif	// _LLCORE_HTTP_INTERNAL_H_
