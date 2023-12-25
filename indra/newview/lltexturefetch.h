/**
 * @file lltexturefetch.h
 * @brief Object for managing texture fetches.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2013, Linden Research, Inc.
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

#ifndef LL_LLTEXTUREFETCH_H
#define LL_LLTEXTUREFETCH_H

#include "llatomic.h"
#include "llcorehttprequest.h"
#include "lldir.h"
#include "hbfastmap.h"
#include "llimage.h"
#include "llworkerthread.h"

#include "llviewertexture.h"

class LLHost;
class LLTextureFetchWorker;

// Interface class
class LLTextureFetch final : public LLWorkerThread
{
protected:
	LOG_CLASS(LLTextureFetch);

	friend class LLTextureFetchWorker;

public:
	LLTextureFetch();
	~LLTextureFetch() override;

	size_t update() override;

	bool createRequest(FTType f_type, const std::string& url, const LLUUID& id,
					   const LLHost& host, F32 priority, S32 w, S32 h, S32 c,
					   S32 discard, bool needs_aux, bool can_use_http);
	bool deleteRequest(const LLUUID& id, bool force = true);
	uuid_list_t deleteAllRequests();
	bool getRequestFinished(const LLUUID& id, S32& discard_level,
							LLPointer<LLImageRaw>& raw,
							LLPointer<LLImageRaw>& aux,
							LLCore::HttpStatus& last_http_get_status);
	bool updateRequestPriority(const LLUUID& id, F32 priority);

	bool receiveImageHeader(const LLHost& host, const LLUUID& id, U8 codec,
							U16 packets, U32 totalbytes, U16 data_size,
							U8* data);
	bool receiveImagePacket(const LLHost& host, const LLUUID& id,
							U16 packet_num, U16 data_size, U8* data);

	LL_INLINE void setTextureBandwidth(F32 bandwidth)		{ mTextureBandwidth = bandwidth; }
	LL_INLINE F32 getTextureBandwidth()						{ return mTextureBandwidth; }

	// Debug
	bool isFromLocalCache(const LLUUID& id);
	S32 getFetchState(const LLUUID& id, F32& decode_progress_p,
					  F32& requested_priority_p, U32& fetch_priority_p,
					  F32& fetch_dtime_p, F32& request_dtime_p,
					  bool& can_use_http);

	U32 getNumRequests();
	// Like getNumRequests() but without locking the queue and thus only an
	// approximative number (used for stats and soft limits). HB
	LL_INLINE U32 getApproxNumRequests()					{ return mApproxNumRequests; }
	// NOTE: mHTTPTextureQueue is only accessed by addToHTTPQueue() and
	// removeFromHTTPQueue() and not used beyond the simple counting of HTTP
	// requests among the total requests. mNumHTTPRequests is updated each time
	// (in both addToHTTPQueue() and removeFromHTTPQueue()) to reflect the
	// number of active requests, so that we can get that number without the
	// need to lock mNetworkQueueMutex... HB
	LL_INLINE U32 getNumHTTPRequests()						{ return mNumHTTPRequests; }

	// LLQueuedThread override
	size_t getPending() override;

	LLTextureFetchWorker* getWorker(const LLUUID& id);
	LLTextureFetchWorker* getWorkerAfterLock(const LLUUID& id);

	LL_INLINE LLCore::HttpRequest& getHttpRequest()			{ return *mHttpRequest; }

	LL_INLINE LLCore::HttpRequest::policy_t getPolicyClass() const
	{
		return mHttpPolicyClass;
	}

	// HTTP resource waiting methods
	void addHttpWaiter(const LLUUID& tid);
	void removeHttpWaiter(const LLUUID& tid);
	bool isHttpWaiter(const LLUUID& tid);

	// Requests from resource wait state (WAIT_HTTP_RESOURCE) to active
	// (SEND_HTTP_REQ).
	//
	// Because this will modify state of many workers, you may not hold any Mw
	// lock while calling. This makes it a little inconvenient to use but that
	// is the rule.
	void releaseHttpWaiters();

	LL_INLINE static S32 getMaxRequestsInQueue()			{ return sMaxRequestsInQueue; }

protected:
	void addToNetworkQueue(LLTextureFetchWorker* worker);
	void removeFromNetworkQueue(LLTextureFetchWorker* worker, bool cancel);
	void addToHTTPQueue(const LLUUID& id);
	void removeFromHTTPQueue(const LLUUID& id, S32 received_size = 0);

private:
	void sendRequestListToSimulators();
	// LLQueuedThread override
	void threadedUpdate() override;

public:
	LLAtomicBool			mDebugPause;

private:
	// To protect mRequestMap only:
	LLMutex					mQueueMutex;
	// To protect mNetworkQueue, mHTTPTextureQueue, mHTTPTextureQueue and
	// mCancelQueue:
	LLMutex					mNetworkQueueMutex;

	// Map of all requests by UUID
	typedef fast_hmap<LLUUID, LLTextureFetchWorker*> map_t;
	map_t 					mRequestMap;

	// Set of requests that require network data
	uuid_list_t				mNetworkQueue;
	uuid_list_t				mHTTPTextureQueue;

	typedef std::map<LLHost, uuid_list_t> cancel_queue_t;
	cancel_queue_t			mCancelQueue;

	LLAtomicU32				mApproxNumRequests;
	LLAtomicU32				mNumHTTPRequests;

	F32						mTextureBandwidth;
	U32						mHTTPTextureBits;

	// Interfaces and objects into the core http library used to make our HTTP
	// requests.
	LLCore::HttpRequest*			mHttpRequest;
	LLCore::HttpOptions::ptr_t		mHttpOptions;
	LLCore::HttpOptions::ptr_t		mHttpOptionsWithHeaders;
	LLCore::HttpHeaders::ptr_t		mHttpHeaders;
	LLCore::HttpRequest::policy_t	mHttpPolicyClass;

	uuid_list_t				mHttpWaitResource;

	// We use a resource semaphore to keep HTTP requests in WAIT_HTTP_RESOURCE2
	// if there are not sufficient slots in the transport. This keeps them near
	// where they can be cheaply reprioritized rather than dumping them all
	// across a thread where it's more expensive to get at them. Requests in
	// either SEND_HTTP_REQ or WAIT_HTTP_REQ charge against the semaphore and
	// tracking state transitions is critical to liveness.
	S32						mHttpLowWater;
	S32						mHttpHighWater;
	S32						mHttpSemaphore;
	static S32				sMinRequestsInQueue;
	static S32				sMaxRequestsInQueue;
};

// Global, initialized in llappviewer.cpp and used in newview/. Moved here so
// that LLTextureFetch consumers do not need to include llappviewer.h to  use
// it. HB
extern LLTextureFetch* gTextureFetchp;

#endif // LL_LLTEXTUREFETCH_H
