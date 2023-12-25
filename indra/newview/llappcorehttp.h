/**
 * @file llappcorehttp.h
 * @brief Singleton initialization/shutdown class for llcorehttp library
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef	_LL_APP_COREHTTP_H_
#define	_LL_APP_COREHTTP_H_

#include "boost/bind.hpp"
#include "boost/signals2.hpp"

#include "llcorehttphandler.h"
#include "llcorehttprequest.h"
#include "llcorehttpresponse.h"

// This class manages the lifecyle of the core http library. Slightly different
// style than traditional code but reflects the use of handler classes and
// light-weight interface object instances of the new libraries. To be used as
// a singleton and static construction is fine.
class LLAppCoreHttp final : public LLCore::HttpHandler
{
protected:
	LOG_CLASS(LLAppCoreHttp);

public:
	static constexpr long PIPELINING_DEPTH = 8L;

	typedef LLCore::HttpRequest::policy_t policy_t;

	enum EAppPolicy
	{
		// Catchall policy class. Not used yet but will have a generous
		// concurrency limit. Deep queueing possible by having a chatty HTTP
		// user.
		//
		// Destination:     anywhere
		// Protocol:        http: or https:
		// Transfer size:   KB-MB
		// Long poll:       no
		// Concurrency:     high
		// Request rate:    unknown
		// Pipelined:       no
		AP_DEFAULT,

		// Texture fetching policy class. Used to download textures via
		// capability or SSB service. Deep queueing of requests. Do not share.
		// GET requests only.
		//
		// Destination:     simhost:12046 & {bake-texture,cdn}:80
		// Protocol:        http:
		// Transfer size:   KB-MB
		// Long poll:       no
		// Concurrency:     high
		// Request rate:    high
		// Pipelined:       yes
		AP_TEXTURE,

		// Legacy mesh fetching policy class. Used to download textures via the
		// "GetMesh"' capability. Do not share.
		//
		// Destination:     simhost:12046
		// Protocol:        http:
		// Transfer size:   KB-MB
		// Long poll:       no
		// Concurrency:     dangerously high
		// Request rate:    high
		// Pipelined:       no
		AP_MESH1,

		// New mesh fetching policy class. Used to download textures via the
		// "GetMesh2" capability. Used when fetch request (typically one LOD)
		// is 'small', currently defined as 2MB. Very deeply queued. Do not
		// share. GET requests only.
		//
		// Destination:     simhost:12046 & cdn:80
		// Protocol:        http:
		// Transfer size:   KB-MB
		// Long poll:       no
		// Concurrency:     high
		// Request rate:    high
		// Pipelined:       yes
		AP_MESH2,

		// Large mesh fetching policy class. Used to download textures via
		// "GetMesh" or "GetMesh2" capabilities. Used when fetch request is not
		// small to avoid head-of-line problem when large requests block a
		// sequence of small, fast requests. Can be shared with similar traffic
		// that can wait for longish stalls (default timeout 600s).
		//
		// Destination:     simhost:12046 & cdn:80
		// Protocol:        http:
		// Transfer size:   MB
		// Long poll:       no
		// Concurrency:     low
		// Request rate:    low
		// Pipelined:       no
		AP_LARGE_MESH,

		// Asset download policy class. Used to fetch assets.
		//
		// Destination:     cdn:80
		// Protocol:        https:
		// Transfer size:   KB-MB
		// Long poll:       no
		// Concurrency:     high
		// Request rate:    high
		// Pipelined:       yes
		AP_ASSETS,

		// Asset upload policy class. Used to store assets (mesh only at the
		// moment) via changeable URL. Responses may take some time (default
		// timeout 240s).
		//
		// Destination:     simhost:12043
		// Protocol:        https:
		// Transfer size:   KB-MB
		// Long poll:       no
		// Concurrency:     low
		// Request rate:    low
		// Pipelined:       no
		AP_UPLOADS,

		// Long-poll-type HTTP requests. Not bound by a connection limit.
		// Requests will typically hang around for a long time (~30s). Only
		// shareable with other long-poll requests.
		//
		// Destination:     simhost:12043
		// Protocol:        https:
		// Transfer size:   KB
		// Long poll:       yes
		// Concurrency:     unlimited but low in practice
		// Request rate:    low
		// Pipelined:       no
		AP_LONG_POLL,

		// Inventory operations (really Capabilities-related operations). Mix
		// of high-priority and low-priority operations.
		//
		// Destination:     simhost:12043
		// Protocol:        https:
		// Transfer size:   KB-MB
		// Long poll:       no
		// Concurrency:     high
		// Request rate:    high
		// Pipelined:       no
		AP_INVENTORY,
		AP_REPORTING = AP_INVENTORY,	// Piggy-back on inventory

		// Material resource requests and puts.
		//
		// Destination:     simhost:12043
		// Protocol:        https:
		// Transfer size:   KB
		// Long poll:       no
		// Concurrency:     low
		// Request rate:    low
		// Pipelined:       no
		AP_MATERIALS,

		// Appearance resource requests and puts.
		//
		// Destination:     simhost:12043
		// Protocol:        https:
		// Transfer size:   KB
		// Long poll:       no
		// Concurrency:     mid
		// Request rate:    low
		// Pipelined:       yes
		AP_AGENT,

		AP_COUNT						// Must be last
	};

	LLAppCoreHttp();
	~LLAppCoreHttp();

	// Initialize the LLCore::HTTP library creating service classes
	// and starting the servicing thread.  Caller is expected to do
	// other initializations (SSL mutex, thread hash function) appropriate
	// for the application.
	void init();

	// Request that the servicing thread stop servicing requests,
	// release resource references and stop.  Request is asynchronous
	// and @see cleanup() will perform a limited wait loop for this
	// request to stop the thread.
	void requestStop();

	// Terminate LLCore::HTTP library services.  Caller is expected
	// to have made a best-effort to shutdown the servicing thread
	// by issuing a requestThreadStop() and waiting for completion
	// notification that the stop has completed.
	void cleanup();

	// Notification when the stop request is complete.
	LL_INLINE void onCompleted(LLCore::HttpHandle handle,
							   LLCore::HttpResponse* response) override
	{
		mStopped = true;
	}

	// Retrieve a policy class identifier for desired application function.
	LL_INLINE policy_t getPolicy(EAppPolicy policy) const
	{
		return mHttpClasses[policy].mPolicy;
	}

	LL_INLINE bool isPipelined(EAppPolicy policy) const
	{
		return mHttpClasses[policy].mPipelined;
	}

	bool isPipeliningOn();

	// Apply initial or new settings from the environment.
	void refreshSettings(bool initial = false);

#if LL_CURL_BUG
	// HACK: to work around libcurl bugs that sometimes cause the HTTP pipeline
	// to return corrupted data... The idea of that hack is to temporarily turn
	// off pipelining when we detect an issue, and automatically turn it back
	// on a minute later, with fresh pipelined connections, once the old ones
	// have been closed.
	void setPipelinedTempOff(bool turn_off = true);
	void checkPipelinedTempOff();
#endif

private:
	static LLCore::HttpStatus sslVerify(const std::string& uri,
										const LLCore::HttpHandler::ptr_t& handler,
										void* userdata);

private:
	// PODish container for per-class settings and state.
	struct HttpClass
	{
	public:
		HttpClass();

	public:
		// Policy class id for the class:
		policy_t					mPolicy;
		U32							mConnLimit;
		bool						mPipelined;
		// Signal to global setting that affect this class (if any):
		boost::signals2::connection mSettingsSignal;
	};
	HttpClass						mHttpClasses[AP_COUNT];

	// Request queue to issue shutdowns:
	LLCore::HttpRequest*			mRequest;
	LLCore::HttpHandle				mStopHandle;
	F64								mStopRequested;

	// Signals to global settings that affect us:
	boost::signals2::connection		mPipelinedSignal;
	boost::signals2::connection		mOSPipelinedSignal;

#if LL_CURL_BUG
	// When to restart HTTP pipelining after it got temporarily turned off
	F32								mRestartPipelined;

	bool							mPipelinedTempOff;
#endif

	bool							mPipelined;			// Global setting
	bool							mStopped;
};

#endif	// _LL_APP_COREHTTP_H_
