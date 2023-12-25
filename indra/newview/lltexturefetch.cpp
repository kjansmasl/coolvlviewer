/**
 * @file lltexturefetch.cpp
 * @brief Object which fetches textures from the cache and/or network
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

#include "llviewerprecompiledheaders.h"

#include <iostream>

#include "lltexturefetch.h"

#include "llcorebufferarray.h"
#include "llcorebufferstream.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "llhttpconstants.h"
#include "llhttpretrypolicy.h"
#include "llimage.h"
#include "llimagedecodethread.h"
#include "llimagej2c.h"
#include "llsdutil.h"
#include "llworkerthread.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"				// For gFrameTimeSeconds
#include "llgridmanager.h"
#include "llstartup.h"
#include "lltexturecache.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewerregion.h"
#include "llworld.h"

// Global variable.
LLTextureFetch* gTextureFetchp = NULL;

// Introduction
//
// This is an attempt to document what's going on in here after-the-fact. It is
// a sincere attempt to be accurate but there will be mistakes.
//
//
// Purpose
//
// What is this module trying to do?  It accepts requests to load textures at a
// given priority and discard level and notifies the caller when done
// (successfully or not). Additional constraints are:
//
// * Support a local texture cache. Do not hit network when possible to avoid
//   it.
// * Use UDP or HTTP as directed or as fallback. HTTP is tried when not
//   disabled and a URL is available. UDP when a URL is not available or HTTP
//   attempts fail.
// * Asynchronous (using threads). Main thread is not to be blocked or
//   burdened.
// * High concurrency. Many requests need to be in-flight and at various stages
//   of completion.
// * Tolerate frequent re-prioritizations of requests. Priority is a reflection
//   of a camera's viewpoint and as that viewpoint changes, objects and
//   textures become more and less relevant and that is expressed at this level
//   by priority changes and request cancelations.
//
// The caller interfaces that fall out of the above and shape the
// implementation are:
// * createRequest - Load j2c image via UDP or HTTP at given discard level and
//                   priority
// * deleteRequest - Request removal of prior request
// * getRequestFinished - Test if request is finished returning data to caller
// * updateRequestPriority - Change priority of existing request
// * getFetchState - Retrieve progress on existing request
//
// Everything else in here is mostly plumbing, metrics and debug.
//
//
// The Work Queue
//
// The two central classes are LLTextureFetch and LLTextureFetchWorker.
// LLTextureFetch combines threading with a priority queue of work requests.
// The priority queue is sorted by a U32 priority derived from the F32 priority
// in the APIs. The *only* work request that receives service time by this
// thread is the highest priority request. All others wait until it is complete
// or a dynamic priority change has re-ordered work.
//
// LLTextureFetchWorker implements the work request and is 1:1 with texture
// fetch requests. Embedded in each is a state machine that walks it through
// the cache, HTTP, UDP, image decode and retry steps of texture acquisition.
//
//
// Threads
//
// Several threads are actively invoking code in this module. They include:
//
// 1.  Tmain    Main thread of execution
// 2.  Ttf      LLTextureFetch's worker thread provided by LLQueuedThread
// 3.  Ttc      LLTextureCache's worker thread
// 4.  Tid      Image decoder's worker thread
// 5.  Thl      HTTP library's worker thread
//
//
// Mutexes/Condition Variables
//
// 1.  Mt       Mutex defined for LLThread's condition variable (base class of
//              LLTextureFetch)
// 2.  Ct       Condition variable for LLThread and used by lock/unlockData().
// 3.  Mwtd     Special LLWorkerThread mutex used for request deletion
//              operations (base class of LLTextureFetch)
// 4.  Mfq      LLTextureFetch's mutex covering request and command queue
//              data.
// 5.  Mfnq     LLTextureFetch's mutex covering udp and http request
//              queue data.
// 6.  Mwc      Mutex covering LLWorkerClass's members (base class of
//              LLTextureFetchWorker). One per request.
// 7.  Mw       LLTextureFetchWorker's mutex. One per request.
//
//
// Lock Ordering Rules
//
// Not an exhaustive list but shows the order of lock acquisition needed to
// prevent deadlocks. 'A < B' means acquire 'A' before acquiring 'B'.
//
// 1.    Mw < Mfnq
// (there are many more...)
//
//
// Method and Member Definitions
//
// With the above, we will try to document what threads can call what methods
// (using T* for any), what locks must be held on entry and are taken out
// during execution and what data is covered by which lock (if any). This
// latter category will be especially prone to error so be skeptical.
//
// A line like: "// Locks: M<xxx>" indicates a method that must be invoked by
// a caller holding the 'M<xxx>' lock. Similarly, "// Threads: T<xxx>" means
// that a caller should be running in the indicated thread.
//
// For data members, a trailing comment like "// M<xxx>" means that the data
// member is covered by the specified lock. Absence of a comment can mean the
// member is unlocked or that I did not bother to do the archaeology. In the
// case of LLTextureFetchWorker, most data members added by the leaf class are
// actually covered by the Mw lock. You may also see "// T<xxx>" which means
// that the member's usage is restricted to one thread (except for perhaps
// construction and destruction) and so explicit locking is not used.
//
// In code, a trailing comment like "// [-+]M<xxx>" indicates a lock acquision
// or release point.
//
//
// Worker Lifecycle
//
// The threading and responder model makes it very likely that other components
// are holding on to a pointer to a worker request. So, uncoordinated deletions
// of requests is a guarantee of memory corruption in a short time. So
// destroying a request involves invocations's of LLQueuedThread/LLWorkerThread
// abort/stop logic that removes workers and puts them on a delete queue for
// 2-phase destruction. That second phase is deferrable by calls to deleteOK()
// which only allow final destruction (via dtor) once deleteOK has determined
// that the request is in a safe state.
//
//
// Worker State Machine
//
// (ASCII art needed)
//
//
// Priority Scheme
//
// [PRIORITY_LOW, PRIORITY_NORMAL)   - for WAIT_HTTP_RESOURCE state
//									   and other wait states
// [PRIORITY_HIGH, PRIORITY_URGENT)  - External event delivered,
//                                     rapidly transitioning through states,
//                                     no waiting allowed
//
// By itself, the above work queue model would fail the concurrency and
// liveness requirements of the interface. A high priority request could find
// itself on the head and stalled for external reasons (see VWR-28996). So a
// few additional constraints are required to keep things running:
// * Anything that can make forward progress must be kept at a higher priority
// than anything that cannot.
// * On completion of external events, the associated request needs to be
//   elevated beyond the normal range to handle any data delivery and release
//   any external resource.
//
// This effort is made to keep higher-priority entities moving forward in their
// state machines at every possible step of processing. It is not entirely
// proven that this produces the experiencial benefits promised.

namespace
{
	// The NoOpDeletor is used when passing certain objects (generally the
	// LLTextureFetchWorker) in a smart pointer below for passage into the
	// LLCore::Http libararies. When the smart pointer is destroyed, no action
	// will be taken since we do not in these cases want the object to be
	// destroyed at the end of the call.
    LL_INLINE void NoOpDeletor(LLCore::HttpHandler*)
	{
	}
}

static const char* e_state_name[] =
{
	"INVALID",
	"INIT",
	"LOAD_FROM_TEXTURE_CACHE",
	"CACHE_POST",
	"LOAD_FROM_NETWORK",
	"LOAD_FROM_SIMULATOR",
	"WAIT_HTTP_RESOURCE",
	"WAIT_HTTP_RESOURCE2",
	"SEND_HTTP_REQ",
	"WAIT_HTTP_REQ",
	"DECODE_IMAGE",
	"DECODE_IMAGE_UPDATE",
	"WRITE_TO_CACHE",
	"WAIT_ON_WRITE",
	"DONE"
};

class LLTextureFetchWorker final : public LLWorkerClass,
								   public LLCore::HttpHandler

{
	friend class LLTextureFetch;

protected:
	LOG_CLASS(LLTextureFetchWorker);

private:
	class CacheReadResponder final : public LLTextureCache::ReadResponder
	{
	protected:
		LOG_CLASS(LLTextureFetchWorker::CacheReadResponder);

	public:
		// Threads: Ttf
		LL_INLINE CacheReadResponder(const LLUUID& id, LLImageFormatted* image)
		:	mID(id),
			mStartTime(0.f)
		{
			setImage(image);
		}

		LL_INLINE void started() override	{ mStartTime = gFrameTimeSeconds; }

		// Threads: Ttc
		void completed(bool success) override
		{
			if (gTextureFetchp)
			{
				LLTextureFetchWorker* worker = gTextureFetchp->getWorker(mID);
				if (worker)
				{
 					worker->callbackCacheRead(success, mFormattedImage,
											  mImageSize, mImageLocal);
				}
			}
		}

		LL_INLINE bool expired() const
		{
			constexpr F32 read_timeout = 3.f;	// In seconds
			return mStartTime > 0.f &&
				   gFrameTimeSeconds - mStartTime > read_timeout;
		}

	private:
		LLUUID	mID;
		F32		mStartTime;
	};

	class CacheWriteResponder final : public LLTextureCache::WriteResponder
	{
	protected:
		LOG_CLASS(LLTextureFetchWorker::CacheWriteResponder);

	public:
		// Threads: Ttf
		LL_INLINE CacheWriteResponder(const LLUUID& id)
		:	mID(id),
			mStartTime(0.f)
		{
		}

		LL_INLINE void started() override	{ mStartTime = gFrameTimeSeconds; }

		// Threads: Ttc
		void completed(bool success) override
		{
			if (gTextureFetchp)
			{
				LLTextureFetchWorker* worker = gTextureFetchp->getWorker(mID);
				if (worker)
				{
					worker->callbackCacheWrite(success);
				}
			}
		}

		LL_INLINE bool expired() const
		{
			constexpr F32 write_timeout = 3.f;	// In seconds
			return mStartTime > 0.f &&
				   gFrameTimeSeconds - mStartTime > write_timeout;
		}

	private:
		LLUUID	mID;
		F32		mStartTime;
	};

	class DecodeResponder final : public LLImageDecodeThread::Responder
	{
	protected:
		LOG_CLASS(LLTextureFetchWorker::DecodeResponder);

	public:
		// Threads: Ttf
		DecodeResponder(const LLUUID& id)
		:	mID(id)
		{
		}

		// Threads: Tid
		void completed(bool success, LLImageRaw* raw, LLImageRaw* aux) override
		{
			if (gTextureFetchp)
			{
				LLTextureFetchWorker* worker = gTextureFetchp->getWorker(mID);
				if (worker)
				{
					worker->callbackDecoded(success, raw, aux);
				}
			}
		}

	private:
		LLUUID mID;
	};

	struct Compare
	{
		// lhs < rhs
		LL_INLINE bool operator()(const LLTextureFetchWorker* lhs,
								  const LLTextureFetchWorker* rhs) const
		{
			// Greater priority is "less"
			return lhs->mImagePriority > rhs->mImagePriority;
		}
	};

public:
	// Called from LLWorkerThread::processRequest()
	// Threads: Ttf
	bool doWork(S32 param) override;

	// Called from finishRequest() (WORK THREAD)
	// Threads: Ttf
	void finishWork(S32, bool) override;

	// Called from update()
	// Threads: Tmain
	bool deleteOK() override;

	~LLTextureFetchWorker();

	// Threads: Ttf
	// Locks: Mw
	S32 callbackHttpGet(LLCore::HttpResponse* response, bool partial,
						bool success);

	// Threads: Ttc
	void callbackCacheRead(bool success, LLImageFormatted* image,
						   S32 imagesize, bool islocal);

	// Threads: Ttc
	void callbackCacheWrite(bool success);

	// Threads: Tid
	void callbackDecoded(bool success, LLImageRaw* raw, LLImageRaw* aux);

	// Threads: T*
	const std::string& setGetStatus(LLCore::HttpStatus status)
	{
		LLMutexLock lock(mWorkMutex);
		mGetStatus = status;
		mGetReason = status.toString();
		return mGetReason;
	}

	LL_INLINE void setUrl(const std::string& url)
	{
		mUrl = url;
	}

	LL_INLINE void setCanUseHTTP(bool b)	{ mCanUseHTTP = b; }
	LL_INLINE bool getCanUseHTTP() const	{ return mCanUseHTTP; }

	// Inherited from LLCore::HttpHandler
	// Threads: Ttf
	void onCompleted(LLCore::HttpHandle h, LLCore::HttpResponse* r) override;

protected:
	LLTextureFetchWorker(FTType f_type, const std::string& url,
						 const LLUUID& id, const LLHost& host, F32 priority,
						 S32 discard, S32 size);

private:
	// called from addWork() (MAIN THREAD)
	// Threads: Tmain
	void startWork(S32 param) override;

	// called from doWork() (MAIN THREAD)
	// Threads: Tmain
	void endWork(S32 param, bool aborted) override;

	// Locks: Mw
	void resetFormattedData();

	// Locks: Mw
	void setImagePriority(F32 priority);

	// Locks: Mw (ctor invokes without lock)
	void setDesiredDiscard(S32 discard, S32 size);

    // Threads: T*
	// Locks: Mw
	bool insertPacket(S32 index, U8* data, S32 size);

	// Locks: Mw
	void clearPackets();

	// Locks: Mw
	void setupPacketData();

	// Locks: Mw
	void removeFromCache();

	// Threads: Ttf
	// Locks: Mw
	bool processSimulatorPackets();

	LL_INLINE void lockWorkMutex()			{ mWorkMutex.lock(); }
	LL_INLINE void unlockWorkMutex()		{ mWorkMutex.unlock(); }

	// Locks: Mw
	bool acquireHttpSemaphore()
	{
		llassert(!mHttpHasResource);
		if (!gTextureFetchp ||
			gTextureFetchp->mHttpSemaphore >= gTextureFetchp->mHttpHighWater)
		{
			return false;
		}
		mHttpHasResource = true;
		++gTextureFetchp->mHttpSemaphore;
		return true;
	}

	// Locks: Mw
	void releaseHttpSemaphore()
	{
		llassert(mHttpHasResource);
		mHttpHasResource = false;
		if (gTextureFetchp)
		{
			--gTextureFetchp->mHttpSemaphore;
		}
	}

	void calcWorkPriority();

	LL_INLINE U32 getStartingPriority()
	{
		return mWorkPriority | LLQueuedThread::PRIORITY_HIGH;
	}

	LL_INLINE void setLowPriority()
	{
		setPriority(mWorkPriority | LLQueuedThread::PRIORITY_LOW);
	}

	LL_INLINE void setHighPriority()
	{
		setPriority(mWorkPriority | LLQueuedThread::PRIORITY_HIGH);
	}

private:
	enum e_state // mState
	{
		// *NOTE: do not change the order/value of state variables, some code
		// depends upon specific ordering/adjacency.

		// NOTE: Affects LLTextureBar::draw in lltextureview.cpp (debug hack)
		INVALID = 0,
		INIT,
		LOAD_FROM_TEXTURE_CACHE,
		CACHE_POST,
		LOAD_FROM_NETWORK,
		LOAD_FROM_SIMULATOR,
		WAIT_HTTP_RESOURCE,				// Waiting for HTTP resources
		WAIT_HTTP_RESOURCE2,			// Waiting for HTTP resources
		SEND_HTTP_REQ,					// Commit to sending as HTTP
		WAIT_HTTP_REQ,					// Request sent, wait for completion
		DECODE_IMAGE,
		DECODE_IMAGE_UPDATE,
		WRITE_TO_CACHE,
		WAIT_ON_WRITE,
		DONE
	};

	enum e_request_state // mSentRequest
	{
		UNSENT = 0,
		QUEUED = 1,
		SENT_SIM = 2
	};

	enum e_write_to_cache_state //mWriteToCacheState
	{
		NOT_WRITE = 0,
		CAN_WRITE = 1,
		SHOULD_WRITE = 2
	};

	e_state							mState;
	e_write_to_cache_state			mWriteToCacheState;
	LLPointer<LLImageFormatted>		mFormattedImage;
	LLPointer<LLImageRaw>			mRawImage;
	LLPointer<LLImageRaw>			mAuxImage;
	LLPointer<CacheReadResponder>	mReadResponder;
	LLPointer<CacheWriteResponder>	mWriteResponder;
	FTType							mFTType;
	LLUUID							mID;
	LLHost							mHost;
	std::string						mUrl;
	U8								mType;
	F32								mImagePriority;
	F32								mRequestedPriority;
	U32								mWorkPriority;
	S32								mDesiredDiscard;
	S32								mSimRequestedDiscard;
	S32								mRequestedDiscard;
	S32								mLoadedDiscard;
	S32								mDecodedDiscard;
	LLFrameTimer					mRequestedTimer;
	LLFrameTimer					mFetchTimer;
	S32								mRequestedSize;
	S32								mRequestedOffset;
	S32								mDesiredSize;
	S32								mFileSize;
	S32								mCachedSize;
	e_request_state					mSentRequest;
	bool							mDecoding;
	bool							mLoaded;
	bool							mDecoded;
	bool							mWritten;
	bool							mNeedsAux;
	bool							mHaveAllData;
	bool							mInLocalCache;
	bool							mCanUseHTTP;
	// Set to true when we get the texture via UDP from sim server
	bool							mCanUseNET;
	S32								mRetryAttempt;
	S32								mActiveCount;
	LLCore::HttpStatus				mGetStatus;
	std::string						mGetReason;
	LLAdaptiveRetryPolicy			mFetchRetryPolicy;

	// Work Data
	LLMutex							mWorkMutex;
	struct PacketData
	{
		PacketData(U8* data, S32 size)
		:	mData(data),
			mSize(size)
		{
		}

		~PacketData()						{ clearData(); }
		void clearData()					{ delete[] mData; mData = NULL; }

		U8* mData;
		U32 mSize;
	};

	std::vector<PacketData*>		mPackets;
	S32								mFirstPacket;
	S32								mLastPacket;
	U16								mTotalPackets;
	U8								mImageCodec;

	// Handle of any active request
	LLCore::HttpHandle				mHttpHandle;
	// Refcounted pointer to response data
	LLCore::BufferArray*			mHttpBufferArray;
	S32								mHttpPolicyClass;
	// Actual received data size
	U32								mHttpReplySize;
	// Actual received data offset
	U32								mHttpReplyOffset;
	// Active request to HTTP library
	bool							mHttpActive;
	// Counts against Fetcher's mHttpSemaphore
	bool							mHttpHasResource;
};

LLTextureFetchWorker::LLTextureFetchWorker(FTType f_type,			// Fetched image type
										   const std::string& url,	// Optional URL
										   const LLUUID& id,		// Image UUID
										   const LLHost& host,		// Simulator host
										   F32 priority,			// Priority
										   S32 discard,				// Desired discard
										   S32 size)				// Desired size
:	LLWorkerClass(gTextureFetchp, "TextureFetch"),
	LLCore::HttpHandler(),
	mState(INIT),
	mWriteToCacheState(NOT_WRITE),
	mFTType(f_type),
	mID(id),
	mHost(host),
	mUrl(url),
	mImagePriority(priority),
	mRequestedPriority(0.f),
	mWorkPriority(0),
	mDesiredDiscard(-1),
	mSimRequestedDiscard(-1),
	mRequestedDiscard(-1),
	mLoadedDiscard(-1),
	mDecodedDiscard(-1),
	mRequestedSize(0),
	mRequestedOffset(0),
	mDesiredSize(TEXTURE_CACHE_ENTRY_SIZE),
	mFileSize(0),
	mCachedSize(0),
	mSentRequest(UNSENT),
	mDecoding(false),
	mLoaded(false),
	mDecoded(false),
	mWritten(false),
	mNeedsAux(false),
	mHaveAllData(false),
	mInLocalCache(false),
	mCanUseHTTP(true),
	mRetryAttempt(0),
	mActiveCount(0),
	mFirstPacket(0),
	mLastPacket(-1),
	mTotalPackets(0),
	mImageCodec(IMG_CODEC_INVALID),
	mHttpHandle(LLCORE_HTTP_HANDLE_INVALID),
	mHttpBufferArray(NULL),
	mHttpPolicyClass(gTextureFetchp->mHttpPolicyClass),
	mHttpActive(false),
	mHttpReplySize(0U),
	mHttpReplyOffset(0U),
	mHttpHasResource(false),
	mFetchRetryPolicy(10.0, 3600.0, 2.0, 10)
{
	mCanUseNET = !gIsInSecondLife && mUrl.empty();
	mType = host.isOk() ? LLImageBase::TYPE_AVATAR_BAKE
						: LLImageBase::TYPE_NORMAL;
	calcWorkPriority();
	if (!gTextureFetchp->mDebugPause)
	{
		addWork(0, getStartingPriority());
	}
	setDesiredDiscard(discard, size);
}

LLTextureFetchWorker::~LLTextureFetchWorker()
{
	llassert_always(!haveWork());

	mWorkMutex.lock();
	if (mHttpHasResource)
	{
		releaseHttpSemaphore();
	}
	if (gTextureFetchp && mHttpActive)
	{
		// Issue a cancel on a live request...
		gTextureFetchp->getHttpRequest().requestCancel(
			mHttpHandle, LLCore::HttpHandler::ptr_t());
	}

	mFormattedImage = NULL;
	clearPackets();
	if (mHttpBufferArray)
	{
		mHttpBufferArray->release();
		mHttpBufferArray = NULL;
	}
	mWorkMutex.unlock();

	if (gTextureFetchp)
	{
		gTextureFetchp->removeFromHTTPQueue(mID);
		gTextureFetchp->removeHttpWaiter(mID);
	}
}

// Locks: Mw
void LLTextureFetchWorker::clearPackets()
{
	for_each(mPackets.begin(), mPackets.end(), DeletePointer());
	mPackets.clear();
	mTotalPackets = 0;
	mLastPacket = -1;
	mFirstPacket = 0;
}

// Locks: Mw
void LLTextureFetchWorker::setupPacketData()
{
	S32 data_size = 0;
	if (mFormattedImage.notNull())
	{
		data_size = mFormattedImage->getDataSize();
	}
	if (data_size <= 0)
	{
		return;
	}
	// Only used for simulator requests
	mFirstPacket = (data_size - FIRST_PACKET_SIZE) / MAX_IMG_PACKET_SIZE + 1;
	if (FIRST_PACKET_SIZE +
		(mFirstPacket - 1) * MAX_IMG_PACKET_SIZE != data_size)
	{
		LL_DEBUGS("TextureFetch") << "Bad cached texture size (texture probably cached after an UDP fetch fallback): "
								  << data_size << " removing " << mID
								  << LL_ENDL;
		removeFromCache();
		resetFormattedData();
		clearPackets();
	}
	else if (mFileSize > 0)
	{
		mLastPacket = mFirstPacket - 1;
		mTotalPackets = (mFileSize - FIRST_PACKET_SIZE +
						 MAX_IMG_PACKET_SIZE - 1) / MAX_IMG_PACKET_SIZE + 1;
	}
	else
	{
		// This file was cached using HTTP so we have to refetch the first
		// packet
		resetFormattedData();
		clearPackets();
	}
}

// Locks: Mw (ctor invokes without lock)
void LLTextureFetchWorker::calcWorkPriority()
{
	static const F32 PRIORITY_SCALE =
		(F32)LLQueuedThread::PRIORITY_LOWBITS /
		LLViewerFetchedTexture::maxDecodePriority();
	mWorkPriority = llmin((U32)LLQueuedThread::PRIORITY_LOWBITS,
						  (U32)(mImagePriority * PRIORITY_SCALE));
}

// Locks: Mw (ctor invokes without lock)
void LLTextureFetchWorker::setDesiredDiscard(S32 discard, S32 size)
{
	if (!gTextureFetchp) return;

	bool prioritize = false;
	if (mDesiredDiscard != discard)
	{
		if (!haveWork())
		{
			calcWorkPriority();
			if (!gTextureFetchp->mDebugPause)
			{
				addWork(0, getStartingPriority());
			}
		}
		else if (mDesiredDiscard < discard)
		{
			prioritize = true;
		}
		mDesiredDiscard = discard;
		mDesiredSize = size;
	}
	else if (size > mDesiredSize)
	{
		mDesiredSize = size;
		prioritize = true;
	}
	mDesiredSize = llmax(mDesiredSize, TEXTURE_CACHE_ENTRY_SIZE);
	if (mState == DONE || (prioritize && mState == INIT))
	{
		mState = INIT;
		setHighPriority();
	}
}

// Locks: Mw
void LLTextureFetchWorker::setImagePriority(F32 priority)
{
	mImagePriority = priority;

	if (mState == DONE ||
		fabsf(priority - mImagePriority) > mImagePriority * .05f)
	{
		calcWorkPriority();
		U32 work_priority = mWorkPriority |
							(getPriority() &
							 LLQueuedThread::PRIORITY_HIGHBITS);
		setPriority(work_priority);
	}
}

// Locks: Mw
void LLTextureFetchWorker::resetFormattedData()
{
	if (mHttpBufferArray)
	{
		mHttpBufferArray->release();
		mHttpBufferArray = NULL;
	}
	if (mFormattedImage.notNull())
	{
		mFormattedImage->deleteData();
	}
	mHttpReplySize = 0;
	mHttpReplyOffset = 0;
	mHaveAllData = false;
}

// Threads: Tmain
void LLTextureFetchWorker::startWork(S32 param)
{
	llassert(mFormattedImage.isNull());
}

// Threads: Ttf
bool LLTextureFetchWorker::doWork(S32 param)
{
	if (!gTextureFetchp) return true;

	LLMutexLock lock(mWorkMutex);

	if (mState < DECODE_IMAGE &&
		(gTextureFetchp->isQuitting() ||
		 getFlags(LLWorkerClass::WCF_DELETE_REQUESTED)))
	{
		return true;	// Aborted fetch
	}
	if (mImagePriority < F_ALMOST_ZERO &&
		(mState == INIT || mState == LOAD_FROM_NETWORK ||
		 mState == LOAD_FROM_SIMULATOR))
	{
		return true;	// Zero priority, abort
	}
	if (mState > CACHE_POST && !mCanUseHTTP &&
		// NOTE: in SL mCanUseNET is always false, but local textures still
		// need to be fetched on pre-caching...
		!(gIsInSecondLife || mCanUseNET))
	{
		return true;	// Nowhere to get data, abort.
	}
	if (gTextureFetchp->mDebugPause)
	{
		return false;	// Debug: pause all work and keep spinning
	}
	if (mState != DONE)
	{
		mFetchTimer.reset();
	}

	if (mState == INIT)
	{
		mRawImage = NULL ;
		mRequestedDiscard = mLoadedDiscard = mDecodedDiscard = -1;
		mRequestedSize = mRequestedOffset = mFileSize = mCachedSize = 0;
		mLoaded = mDecoded = mWritten = mHaveAllData = false;
		mReadResponder = NULL;
		mWriteResponder = NULL;
		mSentRequest = UNSENT;
		if (mHttpBufferArray)
		{
			mHttpBufferArray->release();
			mHttpBufferArray = NULL;
		}
		mHttpReplySize = mHttpReplyOffset = 0;
		clearPackets(); // *TODO: Should not be necessary
		mState = LOAD_FROM_TEXTURE_CACHE;
		// Minimum desired size is TEXTURE_CACHE_ENTRY_SIZE
		mDesiredSize = llmax(mDesiredSize, TEXTURE_CACHE_ENTRY_SIZE);
		LL_DEBUGS("TextureFetch") << mID << ": Priority: "
								  << llformat("%8.0f", mImagePriority)
								  << " Desired Discard: " << mDesiredDiscard
								  << " Desired Size: " << mDesiredSize
								  << LL_ENDL;
		// Fall through
	}

	if (mState == LOAD_FROM_TEXTURE_CACHE)
	{
		if (!gTextureCachep)
		{
			mState = DONE;	// We are likely shutting down at this point...
			return false;
		}

		if (mReadResponder.notNull())	// Still waiting for the cache...
		{
			if (mReadResponder->expired())
			{
				mLoaded = false;
				mReadResponder = NULL;
				removeFromCache();

				if (mUrl.compare(0, 7, "file://") == 0)
				{
					llwarns << "Texture " << mID
							<< " corresponds to an unreadable disk file: "
							<< mUrl << llendl;
					mState = DONE;	// Cannot retry a missing file...
					return true;
				}
				LL_DEBUGS("TextureFetch") << "Texture " << mID
										  << ": cache read timeout; fetching from network."
										  << LL_ENDL;
				mState = LOAD_FROM_NETWORK;
				setHighPriority();
			}
			else
			{
				// Wait for the cache reply
				setLowPriority();
			}
			return false;
		}

		// Ask the texture from the cache.
		S32 offset = mFormattedImage.isNull() ? 0
											  : mFormattedImage->getDataSize();
		S32 size = mDesiredSize - offset;
		if (size > 0)
		{
			mFileSize = 0;
			mLoaded = false;

			// Set priority first since Responder may change it
			setLowPriority();

			bool reading;
			mReadResponder = new CacheReadResponder(mID, mFormattedImage);
			if (mUrl.compare(0, 7, "file://") == 0)
			{
				// Read file from local disk
				std::string filename = mUrl.substr(7);
				reading = gTextureCachep->readFromFile(filename, mID, offset,
													   size, mReadResponder);
			}
			else
			{
				reading = gTextureCachep->readFromCache(mID, offset, size,
														mReadResponder);
			}
			if (reading)
			{
				// Wait for the cache reply
				setLowPriority();
				return false;
			}
			// Failed to post a read to the cache thread queue
			mReadResponder = NULL;
		}

		mState = CACHE_POST;
		// Fall through
	}

	if (mState == CACHE_POST)
	{
		mCachedSize =
			mFormattedImage.isNull() ? 0 : mFormattedImage->getDataSize();
		// Successfully loaded
		if (mCachedSize >= mDesiredSize || mHaveAllData)
		{
			// We have enough data, decode it
			llassert_always(mFormattedImage->getDataSize() > 0);
			mLoadedDiscard = mDesiredDiscard;
			if (mLoadedDiscard < 0)
			{
				llwarns << "Texture " << mID << " mLoadedDiscard is "
						<< mLoadedDiscard << ", should be >= 0" << llendl;
			}
			mState = DECODE_IMAGE;
			mWriteToCacheState = NOT_WRITE;
			LL_DEBUGS("TextureFetch") << mID << ": Cached. Bytes: "
									  << mFormattedImage->getDataSize()
									  << ". Size: "
									  << llformat("%dx%d",
												  mFormattedImage->getWidth(),
												  mFormattedImage->getHeight())
									  << ". Desired discard: "
									  << mDesiredDiscard
									  << ". Desired size: " << mDesiredSize
									  << LL_ENDL;
			// Fall through
		}
		else if (mUrl.compare(0, 7, "file://") == 0)
		{
			// Failed to load local file, we are done.
			llwarns << "Texture " << mID
					<< " corresponds to an unreadable disk file: " << mUrl
					<< llendl;
			mState = DONE;
			return true;
		}
		else
		{
			// Need more data
			LL_DEBUGS("TextureFetch") << "Texture " << mID << ": not in cache"
									  << LL_ENDL;
			mState = LOAD_FROM_NETWORK;
			// Fall through
		}
	}

	if (mState == LOAD_FROM_NETWORK)
	{
		static LLCachedControl<bool> use_http(gSavedSettings,
											  "ImagePipelineUseHTTP");
		if ((use_http || gIsInSecondLife) && mCanUseHTTP && mUrl.empty())
		{
			LLViewerRegion* region = NULL;
			if (mHost.isInvalid())
			{
				region = gAgent.getRegion();
			}
			else
			{
				region = gWorld.getRegion(mHost);
			}
			if (!region)
			{
				// This will happen if not logged in
				LL_DEBUGS("TextureFetch") << "Texture " << mID
										  << ". Region not found for host: "
										  << mHost << LL_ENDL;
				mCanUseHTTP = false;
			}
			else if (!region->capabilitiesReceived())
			{
				// Bail till we have received the capabilities
				return false;
			}
			else
			{
				const std::string& http_url = region->getTextureUrl();
				if (http_url.empty())
				{
					mCanUseHTTP = false;
				}
				else
				{
					mUrl = http_url + "?texture_id=" + mID.asString();
					// Because this texture has a fixed texture id:
					mWriteToCacheState = CAN_WRITE;
				}
			}
		}
		// Check for retries to previous server failures.
		F32 wait_seconds;
		if (mFetchRetryPolicy.shouldRetry(wait_seconds))
		{
			if (wait_seconds <= 0.f)
			{
				llinfos <<"Retrying fecth now for texture: " << mID << llendl;
			}
			else
			{
				LL_DEBUGS("TextureFetch") << "Texture " << mID
										  << " waiting to retry for "
										  << wait_seconds << " seconds"
										  << LL_ENDL;
				return false;
			}
		}
		if (mCanUseHTTP && !mUrl.empty())
		{
			mState = WAIT_HTTP_RESOURCE;
			setHighPriority();
			if (mWriteToCacheState != NOT_WRITE)
			{
				mWriteToCacheState = CAN_WRITE;
			}
			// Do not return, fall through to next state
		}
		// NOTE: in SL mCanUseNET is always false, but local textures still
		// need to be fetched on pre-caching...
		else if (mSentRequest == UNSENT && (mCanUseNET || gIsInSecondLife))
		{
			// Add this to the network queue and sit here.
			// LLTextureFetch::sendRequestListToSimulators() will send off a
			// request which, when replied to by the simulator, will cause our
			// state to change to LOAD_FROM_SIMULATOR  via
			// LLTextureFetch::receiveImageHeader().
			mWriteToCacheState = CAN_WRITE;
			mRequestedSize = mDesiredSize;
			mRequestedDiscard = mDesiredDiscard;
			mSentRequest = QUEUED;
			gTextureFetchp->addToNetworkQueue(this);
			setLowPriority();
			return false;
		}
		else
		{
			return false;
		}
	}

	if (mState == LOAD_FROM_SIMULATOR)
	{
		if (mFormattedImage.isNull())
		{
			mFormattedImage = new LLImageJ2C;
		}
		if (processSimulatorPackets())
		{
			LL_DEBUGS("TextureFetch") << mID << ": loaded from sim. Bytes: "
									  << mFormattedImage->getDataSize()
									  << LL_ENDL;
			gTextureFetchp->removeFromNetworkQueue(this, false);
			if (mFormattedImage.isNull() || !mFormattedImage->getDataSize())
			{
				LL_DEBUGS("TextureFetch") << "processSimulatorPackets() failed to load buffer"
										  << LL_ENDL;
				return true;	// Failed
			}
			setHighPriority();
			if (mLoadedDiscard < 0)
			{
				llwarns << "Texture " << mID << " mLoadedDiscard is "
						<< mLoadedDiscard << ", should be >= 0" << llendl;
			}
			mState = DECODE_IMAGE;
			mWriteToCacheState = SHOULD_WRITE;
			// Fall through
		}
		else
		{
			gTextureFetchp->addToNetworkQueue(this); // Fail-safe
			setLowPriority();
			return false;
		}
	}

	if (mState == WAIT_HTTP_RESOURCE)
	{
		// NOTE: control the number of the http requests issued to:
		// 1.- avoid opening too many file descriptors at the same time;
		// 2.- control the traffic of http so udp gets bandwidth.
		//
		// If it looks like we are busy, keep this request here. Otherwise,
		// advance into the HTTP states.
		if (!acquireHttpSemaphore())
		{
			mState = WAIT_HTTP_RESOURCE2;
			setLowPriority();
			gTextureFetchp->addHttpWaiter(mID);
			return false;
		}
		mState = SEND_HTTP_REQ;

		// *NOTE:  You must invoke releaseHttpSemaphore() if you transition
		// to a state other than SEND_HTTP_REQ or WAIT_HTTP_REQ or abort
		// the request.
	}

	if (mState == WAIT_HTTP_RESOURCE2)
	{
		// Just idle it if we make it to the head...
		return false;
	}

	if (mState == SEND_HTTP_REQ)
	{
		static LLCachedControl<bool> disable_range_req(gSavedSettings,
													   "HttpRangeRequestsDisable");
		if (!mCanUseHTTP)
		{
			releaseHttpSemaphore();
			llwarns << "Texture " << mID
					<< " got to SEND_HTTP_REQ state but cannot use HTTP; aborting."
					<< llendl;
			return true; // Abort
		}

		gTextureFetchp->removeFromNetworkQueue(this, false);

		S32 cur_size = 0;
		if (mFormattedImage.notNull())
		{
			// Amount of data we already have:
			cur_size = mFormattedImage->getDataSize();
			if (mFormattedImage->getDiscardLevel() == 0)
			{
				if (cur_size > 0)
				{
					// We already have all the data, just decode it
					mLoadedDiscard = mFormattedImage->getDiscardLevel();
					setHighPriority();
					if (mLoadedDiscard < 0)
					{
						llwarns << "Texture " << mID << " mLoadedDiscard is "
								<< mLoadedDiscard << ", should be >= 0"
								<< llendl;
					}
					mState = DECODE_IMAGE;
					releaseHttpSemaphore();
					goto decode_image; // Fall through
				}
				else
				{
					releaseHttpSemaphore();
					llwarns << "Texture " << mID
							<< " SEND_HTTP_REQ aborted due to negative or null size: "
							<< cur_size << llendl;
					return true; // Abort.
				}
			}
		}
		mRequestedSize = mDesiredSize;
		mRequestedDiscard = mDesiredDiscard;
		mRequestedSize -= cur_size;
		mRequestedOffset = cur_size;
		if (mRequestedOffset)
		{
			// Texture fetching often issues 'speculative' loads that start
			// beyond the end of the actual asset. Some cache/web systems, e.g.
			// Varnish, will respond to this not with a 416 but with a 200 and
			// the entire asset in the response body. By ensuring that we
			// always have a partially satisfiable Range request, we avoid that
			// hit to the network. We just have to deal with the overlapping
			// data which is made somewhat harder by the fact that grid
			// services do not necessarily return the Content-Range header on
			// 206 responses. *Sigh*
			mRequestedOffset -= 1;
			mRequestedSize += 1;
		}

		mHttpHandle = LLCORE_HTTP_HANDLE_INVALID;
		if (!mUrl.empty())
		{
			mRequestedTimer.reset();
			mLoaded = false;
			mGetStatus = LLCore::HttpStatus();
			mGetReason.clear();
			LL_DEBUGS("TextureFetch") << "HTTP GET: " << mID << ". Offset: "
									  << mRequestedOffset << ". Bytes: "
									  << mRequestedSize << LL_ENDL;

			// For now, in SL, only server bake images use the returned headers
			// to specify a retry-after field. This said, it does not really
			// hurt to check for such a field in all replies (it involves
			// LLCore copying the header field for all replies, but it is a
			// very small time and memory penalty when compared to the rest of
			// the HTTP texture fetching code). RC server channels may soon be
			// using retry-after fields for all textures in SL...
			// *TODO: after the retry-after field will be generalized in SL,
			// make this an option (off by default) for OpenSim grids.
			static LLCachedControl<bool> check_all(gSavedSettings,
												   "TextureRetryDelayFromHeader");
			bool with_headers = check_all ||
								(gIsInSecondLife &&
								 mFTType == FTT_SERVER_BAKE);
			LLCore::HttpOptions::ptr_t options;
			options = with_headers ? gTextureFetchp->mHttpOptionsWithHeaders
								   : gTextureFetchp->mHttpOptions;

			if (disable_range_req)
			{
				// 'Range:' requests may be disabled in which case all HTTP
				// texture fetches result in full fetches. This can be used by
				// people with questionable ISPs or networking gear that do not
				// handle these well.
				mHttpHandle =
					gTextureFetchp->mHttpRequest->requestGet(
						mHttpPolicyClass, mUrl, options,
						gTextureFetchp->mHttpHeaders,
						LLCore::HttpHandler::ptr_t(this, &NoOpDeletor));
			}
			else
			{
				// *NOTE: This is an empirical value. Texture fetches have a
				// habit of using a value of 32MB to indicate 'get the rest of
				// the image'. Certain ISPs and network equipments get confused
				// when they see this in a Range: header. So, if the request
				// end is beyond this value, we issue an open-ended
				// "Range: request" (e.g. 'Range: <start>-') which seems to fix
				// the problem.
				constexpr S32 HTTP_REQUESTS_RANGE_END_MAX = 20000000;
				S32 req_size = mRequestedOffset + mRequestedSize >
								HTTP_REQUESTS_RANGE_END_MAX ? 0
															: mRequestedSize;
				// Will call callbackHttpGet when curl request completes
				mHttpHandle =
					gTextureFetchp->mHttpRequest->requestGetByteRange(
						mHttpPolicyClass, mUrl, mRequestedOffset, req_size,
						options, gTextureFetchp->mHttpHeaders,
						LLCore::HttpHandler::ptr_t(this, &NoOpDeletor));
			}
		}
		if (mHttpHandle == LLCORE_HTTP_HANDLE_INVALID)
		{
			LLCore::HttpStatus status =
				gTextureFetchp->mHttpRequest->getStatus();
			llwarns << "HTTP GET request failed for " << mID << ", status: "
					<< status.toTerseString() << " - reason: "
					<< status.toString() << llendl;
			resetFormattedData();
			// Fallback and try UDP
			if (mCanUseNET)
			{
				llinfos << "Falling back to UDP sim fetch for texture: "
						<< mID << llendl;
				mState = INIT;
				mCanUseHTTP = false;
				mUrl.clear();
				setHighPriority();
			}
			releaseHttpSemaphore();
			return !mCanUseNET;
		}

		mHttpActive = true;
		gTextureFetchp->addToHTTPQueue(mID);
		setLowPriority();
		mState = WAIT_HTTP_REQ;
		// Fall through
	}

	if (mState == WAIT_HTTP_REQ)
	{
		// *NOTE: As stated above, all transitions out of this state should
		// call releaseHttpSemaphore().
		if (!mLoaded)
		{
			// *HISTORY: there was a texture timeout test here originally that
			// would cancel a request that was over 120 seconds old. This is
			// probably not a good idea. Particularly rich regions can take an
			// enormous amount of time to load textures. We will revisit the
			// various possible timeout components (total request time,
			// connection time, I/O time, with and without retries, etc) in the
			// future.
			setLowPriority();
			return false;
		}

		S32 cur_size =
			mFormattedImage.notNull() ? mFormattedImage->getDataSize() : 0;
		if (mRequestedSize < 0)
		{
			if (mGetStatus == gStatusNotFound)
			{
				if (mWriteToCacheState == NOT_WRITE)
				{
					// Map tiles or server bakes. For map tiles, failed means
					// empty region, which is normal and expected.
					mState = DONE;
					releaseHttpSemaphore();
					if (mFTType != FTT_MAP_TILE)
					{
						llwarns << "Texture missing from server (404): "
								<< mUrl << llendl;
					}
					return true;
				}

				// Fallback and try UDP
				if (mCanUseNET)
				{
					llinfos << "Falling back to UDP sim fetch for texture: "
							<< mID << llendl;
					mState = INIT;
					mCanUseHTTP = false;
					mUrl.clear();
					setHighPriority();
					releaseHttpSemaphore();
					return false;
				}
			}
			else if (mGetStatus == gStatusUnavailable)
			{
				llinfos_once << "Texture server busy (503): " << mUrl
							 << llendl;
			}
			else if (mGetStatus == gStatusNotSatisfiable)
			{
				// Allowed, we will accept whatever data we have as complete.
				mHaveAllData = true;
			}
			else
			{
				llinfos << "HTTP GET failed for: " << mUrl << " - Status: "
						<< mGetStatus.toTerseString() << " - Reason: "
						<< mGetReason << llendl;
			}

			// Fallback and try UDP
			if (mCanUseNET && mFTType != FTT_LOCAL_FILE)
			{
				llinfos << "Falling back to UDP sim fetch for texture: " << mID
						<< llendl;
				mState = INIT;
				mCanUseHTTP = false;
				mUrl.clear();
				setHighPriority();
				releaseHttpSemaphore();
				return false;
			}

#if 0		// This causes issues with failures to retry some textures (e.g.
			// for land patches). HB
			if (mFTType != FTT_SERVER_BAKE && mFTType != FTT_MAP_TILE)
#endif
			{
				mUrl.clear();
			}

			if (cur_size > 0)
			{
				// Use available data
				mLoadedDiscard = mFormattedImage->getDiscardLevel();
				setHighPriority();
				if (mLoadedDiscard < 0)
				{
					llwarns << "Texture " << mID << " mLoadedDiscard is "
							<< mLoadedDiscard << ", should be >= 0" << llendl;
				}
				mState = DECODE_IMAGE;
				releaseHttpSemaphore();
				goto decode_image; // Fall through
			}

			// Fail harder
			resetFormattedData();
			mState = DONE;
			releaseHttpSemaphore();
			llwarns << "Texture " << mID << ": failed harder" << llendl;
			return true;	// Failed
		}

		if (mWriteToCacheState != NOT_WRITE)
		{
			// Clear the url since we are done with the fetch. Note: mUrl is
			// used to check is fetching is required so failure to clear it
			// will force an http fetch next time the texture is requested,
			// even if the data have already been fetched.
			mUrl.clear();
		}

		if (!mHttpBufferArray || !mHttpBufferArray->size())
		{
			// No data received.
			if (mHttpBufferArray)
			{
				mHttpBufferArray->release();
				mHttpBufferArray = NULL;
			}

			// Abort.
			mState = DONE;
			llwarns << "Texture " << mID << ": no data received" << llendl;
			releaseHttpSemaphore();
			return true;
		}

		S32 append_size = mHttpBufferArray->size();
		S32 total_size = cur_size + append_size;
		S32 src_offset = 0;
		llassert_always(append_size == mRequestedSize);
		if (mHttpReplyOffset && (S32)mHttpReplyOffset != cur_size)
		{
			// In case of a partial response, our offset may not be trivially
			// contiguous with the data we have. Get back into alignment.
			if ((S32)mHttpReplyOffset > cur_size ||
				cur_size > (S32)mHttpReplyOffset + append_size)
			{
				llwarns << "Partial HTTP response produces break in image data for texture "
						<< mID << ". Retrying load." << llendl;
#if LL_CURL_BUG	// *HACK: HTTP pipelining is buggy in libcurl versions after
				// v7.47 and is causing this kind of issue, so let's turn it
				// off, the time for the pipelined connection to get closed, so
				// that we can restart with fresh ones later... HB
				if (gAppViewerp->getAppCoreHttp().isPipeliningOn())
				{
					gAppViewerp->getAppCoreHttp().setPipelinedTempOff();
				}
#endif
				removeFromCache();
				resetFormattedData();
				if (mCanUseNET)
				{
					// Fallback and try UDP
					llinfos << "Falling back to UDP sim fetch for texture: "
							<< mID << llendl;
					mCanUseHTTP = false;
					mUrl.clear();
				}
				mState = INIT;
				setHighPriority();
				releaseHttpSemaphore();
				return false;
			}
			src_offset = cur_size - mHttpReplyOffset;
			append_size -= src_offset;
			total_size -= src_offset;
			// Make requested values reflect useful part:
			mRequestedSize -= src_offset;
			mRequestedOffset += src_offset;
		}

		if (mFormattedImage.isNull())
		{
			// For now, create formatted image based on extension
			std::string extension = gDirUtilp->getExtension(mUrl);
			mFormattedImage =
				LLImageFormatted::createFromType(LLImageBase::getCodecFromExtension(extension));
			if (mFormattedImage.isNull())
			{
				mFormattedImage = new LLImageJ2C; // Default
			}
		}

		if (mHaveAllData)	// The image file is fully loaded.
		{
			mFileSize = total_size;
		}
		else				// The file size is unknown.
		{
			// Flag the file is not fully loaded.
			mFileSize = total_size + 1;
		}

		U8* buffer = (U8*)allocate_texture_mem(total_size);
		if (!buffer)
		{
			// Fail because of out of memory error
			resetFormattedData();
			mState = DONE;
			llwarns << "Out of memory: could not complete texture fetch for "
					<< mID << llendl;
			releaseHttpSemaphore();
			return true;	// Failed
		}
		if (cur_size > 0)
		{
			memcpy(buffer, mFormattedImage->getData(), cur_size);
		}
		mHttpBufferArray->read(src_offset, (char*)buffer + cur_size,
							   append_size);

		// NOTE: setData releases current data and owns new data (buffer)
		mFormattedImage->setData(buffer, total_size);

		// Done with buffer array
		mHttpBufferArray->release();
		mHttpBufferArray = NULL;
		mHttpReplySize = 0;
		mHttpReplyOffset = 0;

		mLoadedDiscard = mRequestedDiscard;
		if (mLoadedDiscard < 0)
		{
			llwarns << "Texture " << mID << " mLoadedDiscard is "
					<< mLoadedDiscard << ", should be >= 0" << llendl;
		}
		mState = DECODE_IMAGE;
		if (mWriteToCacheState != NOT_WRITE)
		{
			mWriteToCacheState = SHOULD_WRITE;
		}
		setHighPriority();
		releaseHttpSemaphore();
		// Fall through
	}

decode_image:
	if (mState == DECODE_IMAGE)
	{
		// Set priority first since Responder may change it
		setLowPriority();

		if (mDesiredDiscard < 0 || mFormattedImage.isNull() ||
			mFormattedImage->getDataSize() <= 0 ||
			mLoadedDiscard < 0 || !gImageDecodeThreadp)
		{
			// We aborted, or decode entered with invalid mFormattedImage,
			// or decode entered with invalid mLoadedDiscard: do not decode.
			mState = DONE;
			goto fetch_done; // Fall through
		}

		mRawImage = NULL;
		mAuxImage = NULL;
		S32 discard = mHaveAllData ? 0 : mLoadedDiscard;
		mDecoded  = false;
		mState = DECODE_IMAGE_UPDATE;
		LL_DEBUGS("TextureFetch") << "Decoding " << mID << ". Bytes: "
								  << mFormattedImage->getDataSize()
								  << ". Discard: " << discard << ". All data: "
								  << mHaveAllData << LL_ENDL;
		mDecoding =
			gImageDecodeThreadp->decodeImage(mFormattedImage, discard,
											 mNeedsAux,
											 new DecodeResponder(mID));
		// Fall through
	}

	if (mState == DECODE_IMAGE_UPDATE)
	{
		if (!mDecoded)
		{
			return false;
		}

		if (mDecodedDiscard < 0)
		{
			LL_DEBUGS("TextureFetch") << "Failed to decode " << mID << LL_ENDL;
			if (mCachedSize > 0 && !mInLocalCache && mRetryAttempt == 0)
			{
				// Cache file should be deleted, try again
				LL_DEBUGS("TextureFetch") << "Texture" << mID
										  << ": decode of cached file failed (removed), retrying."
										  << LL_ENDL;
				llassert_always(!mDecoding);
				mFormattedImage = NULL;
				++mRetryAttempt;
				setHighPriority();
				mState = INIT;
				return false;
			}
			LL_DEBUGS("TextureFetch") << "Unable to load texture " << mID
									  << " after " << mRetryAttempt
									  << " retries." << LL_ENDL;
			mState = DONE; // Failed
			// Fall through
		}
		else
		{
			llassert_always(mRawImage.notNull());
			LL_DEBUGS("TextureFetch") << mID << " decoded. Discard: "
									  << mDecodedDiscard << ". Raw image: "
									  << llformat("%dx%d",
												  mRawImage->getWidth(),
												  mRawImage->getHeight())
									  << LL_ENDL;
			setHighPriority();
			mState = WRITE_TO_CACHE;
		}
		// Fall through
	}

	if (mState == WRITE_TO_CACHE)
	{
		if (!gTextureCachep || mWriteToCacheState != SHOULD_WRITE ||
			 mFormattedImage.isNull())
		{
			// If the cache is destroyed, or we are a local texture or we did
			// not actually receive any new data, or we failed to load
			// anything, skip.
			mState = DONE;
			goto fetch_done; // Fall through
		}
		S32 datasize = mFormattedImage->getDataSize();
		if (datasize <= 0)
		{
			// This should not happen... But has been seen happening once by
			// one user, who then hit the llassert_always(datasize) that used
			// to be there... Use proper fallback code (skip) instead. HB
			mState = DONE;
			goto fetch_done; // Fall through
		}

		// Set priority first since Responder may change it:
		setLowPriority();

		if (mFileSize < datasize)
		{
			// This could happen when http fetching and sim fetching mixed.
			if (mHaveAllData)
			{
				mFileSize = datasize;
			}
			else
			{
				mFileSize = datasize + 1; // flag not fully loaded.
			}
		}
		mWritten = false;
		mState = WAIT_ON_WRITE;
		mWriteResponder = new CacheWriteResponder(mID);
		if (!gTextureCachep->writeToCache(mID, mFormattedImage->getData(),
										  datasize, mFileSize, mRawImage,
										  mDecodedDiscard, mWriteResponder))
		{
			// Failed to post to the cache write queue, or read-only cache
			mWriteResponder = NULL;
			mState = DONE;
		}
		// Fall through
	}

	if (mState == WAIT_ON_WRITE)
	{
		if (mWritten)
		{
			mState = DONE;
			// Fall through
		}
		else if (!gTextureCachep || mWriteResponder.isNull() ||
				 mWriteResponder->expired())
		{
			llwarns << "Failed to cache texture " << mID << llendl;
			mWriteResponder = NULL;
			mState = DONE;
			// Fall through
		}
		else
		{
			// We are waiting for this write to complete before we can receive
			// more data (we cannot touch mFormattedImage until the write
			// completes).
			return false;
		}
	}

fetch_done:
	if (mState == DONE)
	{
		if (mDecodedDiscard > 0 && mDesiredDiscard < mDecodedDiscard)
		{
			// More data was requested, return to INIT
			mState = INIT;
			setHighPriority();
			return false;
		}

		setLowPriority();
		return true;
	}

	return false;
}

// Threads: Ttf
//virtual
void LLTextureFetchWorker::onCompleted(LLCore::HttpHandle handle,
									   LLCore::HttpResponse* response)
{
	mWorkMutex.lock();

	mHttpActive = false;

	bool success = true;
	bool partial = false;

	LLCore::HttpStatus status(response->getStatus());
	if (!status && mFTType == FTT_SERVER_BAKE)
	{
		llinfos << mID << " state " << e_state_name[mState] << llendl;
		mFetchRetryPolicy.onFailure(response);
		F32 retry_after;
		if (gTextureFetchp && mFetchRetryPolicy.shouldRetry(retry_after))
		{
			llinfos << "Texture: " << mID << " - State: "
					<< e_state_name[mState] << ". Will retry after "
					<< retry_after
					<< " seconds, resetting state to LOAD_FROM_NETWORK"
					<< llendl;
			gTextureFetchp->removeFromHTTPQueue(mID, 0);
			setGetStatus(status);
			releaseHttpSemaphore();
			mState = LOAD_FROM_NETWORK;
			mWorkMutex.unlock();
			return;
		}
		else
		{
			llwarns << "Texture: " << mID << " - State: "
					<< e_state_name[mState] << ". Will not retry" << llendl;
		}
	}
	else
	{
		mFetchRetryPolicy.onSuccess();
	}

	if (!status)
	{
		success = false;
		// Missing map tiles and local files are normal, do not complain about
		// them.
		if (mFTType != FTT_MAP_TILE && mFTType != FTT_LOCAL_FILE)
		{
			llwarns << "Texture: " << mID << " CURL GET FAILED, status: "
					<< status.toTerseString() << " - reason: "
					<< setGetStatus(status) << llendl;
		}
	}
	else
	{
		LL_DEBUGS("TextureFetch") << "HTTP complete: " << mID
								  << " status: " << status.toTerseString()
								  << " '" << setGetStatus(status) << "'"
								  << LL_ENDL;
		// A warning about partial (HTTP 206) data. Some grid services do *not*
		// return a 'Content-Range' header in the response to Range requests
		// with a 206 status. We are forced to assume we get what we asked for
		// in these cases until we can fix the services.
		partial = status == gStatusPartialContent;
	}

	S32 data_size = callbackHttpGet(response, partial, success);

	if (gTextureFetchp)
	{
		gTextureFetchp->removeFromHTTPQueue(mID, data_size);
	}

	mWorkMutex.unlock();
}

// Threads: Tmain
void LLTextureFetchWorker::endWork(S32 param, bool aborted)
{
	mFormattedImage = NULL;
}

// Threads: Ttf

//virtual
void LLTextureFetchWorker::finishWork(S32, bool)
{
	mDecoding = false;
	mReadResponder = NULL;
	mWriteResponder = NULL;
}

// LLQueuedThread's update() method is asking if it is okay to delete this
// worker. You will notice we are not locking in here which is a slight
// concern. Caller is expected to have made this request 'quiet' by whatever
// means...
// Threads: Tmain
//virtual
bool LLTextureFetchWorker::deleteOK()
{
	if (!gTextureFetchp)
	{
		return true;
	}

	if (mHttpActive || mDecoding || mReadResponder.notNull() ||
		mWriteResponder.notNull())
	{
		// HTTP library has a pointer to this worker and will dereference it to
		// do notification. Also, the image decoder thread and texture cache
		// pools cannot cancel a queued decode request.
		return false;
	}

	if (mState == WAIT_HTTP_RESOURCE2 && gTextureFetchp->isHttpWaiter(mID))
	{
		// Do not delete the worker out from under the releaseHttpWaiters()
		// method. Keep the pointers valid, clean up after that method has
		// recognized the cancelation and removed the UUID from the waiter
		// list.
		return false;
	}

	if (haveWork() &&
		// Not ok to delete from these states
		(mState >= WRITE_TO_CACHE && mState <= WAIT_ON_WRITE))
	{
		return false;
	}

	return true;
}

// Threads: Ttf
void LLTextureFetchWorker::removeFromCache()
{
	if (!mInLocalCache && gTextureCachep)
	{
		gTextureCachep->removeFromCache(mID);
	}
}

// Threads: Ttf
// Locks: Mw
bool LLTextureFetchWorker::processSimulatorPackets()
{
	if (mFormattedImage.isNull() || mRequestedSize < 0)
	{
		// Not sure how we got here, but not a valid state, abort !
		llassert_always(!mDecoding);
		mFormattedImage = NULL;
		return true;
	}

	if (mLastPacket >= mFirstPacket)
	{
		S32 buffer_size = mFormattedImage->getDataSize();
		for (S32 i = mFirstPacket; i <= mLastPacket; ++i)
		{
			llassert_always(mPackets[i]);
			buffer_size += mPackets[i]->mSize;
		}
		bool have_all_data = mLastPacket >= mTotalPackets - 1;
		if (mRequestedSize <= 0)
		{
			// We received a packed but did not issue a request yet (edge
			// case). Return true (we are "done") since we did not request
			// anything.
			return true;
		}
		if (buffer_size >= mRequestedSize || have_all_data)
		{
			/// We have enough (or all) data
			if (have_all_data)
			{
				mHaveAllData = true;
			}
			S32 cur_size = mFormattedImage->getDataSize();
			if (buffer_size > cur_size)
			{
				/// We have new data
				U8* buffer = (U8*)allocate_texture_mem(buffer_size);
				if (!buffer)
				{
					// Out of memory: abort
					mHaveAllData = false;
					mFormattedImage = NULL;
					llwarns << "Out of memory: could not complete texture fetch for "
							<< mID << llendl;
					return true;
				}
				S32 offset = 0;
				if (cur_size > 0 && mFirstPacket > 0)
				{
					memcpy(buffer, mFormattedImage->getData(), cur_size);
					offset = cur_size;
				}
				for (S32 i = mFirstPacket; i <= mLastPacket; ++i)
				{
					memcpy(buffer + offset, mPackets[i]->mData,
						   mPackets[i]->mSize);
					offset += mPackets[i]->mSize;
				}
				// NOTE: setData releases current data
				mFormattedImage->setData(buffer, buffer_size);
			}
			mLoadedDiscard = mRequestedDiscard;
			return true;
		}
	}
	return false;
}

bool LLTextureFetchWorker::insertPacket(S32 index, U8* data, S32 size)
{
	mRequestedTimer.reset();
	if (index >= mTotalPackets)
	{
		LL_DEBUGS("TextureFetch") << "Received image packet " << index
								  << " > max: " << mTotalPackets
								  << " for image: " << mID << LL_ENDL;
		return false;
	}
	if (index > 0 && index < mTotalPackets - 1 && size != MAX_IMG_PACKET_SIZE)
	{
 		LL_DEBUGS("TextureFetch") << "Received bad sized packet: " << index
								  << ", " << size << " != "
								  << MAX_IMG_PACKET_SIZE << " for image: "
								  << mID << LL_ENDL;
		return false;
	}

	if (index >= (S32)mPackets.size())
	{
		// Initialize v to NULL pointers
		mPackets.resize(index + 1, NULL);
	}
	else if (mPackets[index])
	{
		LL_DEBUGS("TextureFetch") << "Received duplicate packet: " << index
								  << " for image: " << mID << LL_ENDL;
		return false;
	}

	mPackets[index] = new PacketData(data, size);
	while (mLastPacket + 1 < (S32)mPackets.size() && mPackets[mLastPacket + 1])
	{
		++mLastPacket;
	}
	return true;
}

// Threads: Ttf
// Locks: Mw
S32 LLTextureFetchWorker::callbackHttpGet(LLCore::HttpResponse* response,
										  bool partial, bool success)
{
	S32 data_size = 0;

	if (mState != WAIT_HTTP_REQ)
	{
		llwarns << "Called for an unrequested fetch worker: " << mID
				<< " - req = " << mSentRequest << " - state = " << mState
				<< llendl;
		return data_size;
	}
	if (mLoaded)
	{
		llwarns << "Ignoring duplicate callback for " << mID << llendl;
		return data_size;
	}
	if (success)
	{
		// Get length of stream:
		LLCore::BufferArray * body(response->getBody());
		data_size = body ? body->size() : 0;

		LL_DEBUGS("TextureFetch") << "HTTP received " << mID << ": "
								  << data_size << " bytes." << LL_ENDL;
		if (data_size > 0)
		{
			// *TODO: set the formatted image data here directly to avoid the
			// copy
			// Hold on to body for later copy
			llassert_always(!mHttpBufferArray);
			body->addRef();
			mHttpBufferArray = body;

			if (partial)
			{
				unsigned int offset(0), length(0), full_length(0);
				response->getRange(&offset, &length, &full_length);
				if (!offset && !length)
				{
					// This is the case where we receive a 206 status but there
					// was not a useful Content-Range header in the response.
					// This could be because it was badly formatted but is more
					// likely due to capabilities services which scrub headers
					// from responses. Assume we got what we asked for...
					mHttpReplySize = data_size;
					mHttpReplyOffset = mRequestedOffset;
				}
				else
				{
					mHttpReplySize = length;
					mHttpReplyOffset = offset;
				}
			}

			if (!partial)
			{
				// Response indicates this is the entire asset regardless of
				// our asking for a byte range. Mark it so and drop any partial
				// data we might have so that the current response body becomes
				// the entire dataset.
				if (data_size <= mRequestedOffset)
				{
					llwarns << "Fetched entire texture " << mID
							<< " when it was expected to be marked complete. mImageSize: "
							<< mFileSize << " - datasize: "
							<< mFormattedImage->getDataSize() << llendl;
				}
				mHaveAllData = true;
				llassert_always(!mDecoding);
				mFormattedImage = NULL; // Discard any previous data we had
			}
			else if (data_size < mRequestedSize)
			{
				mHaveAllData = true;
			}
			else if (data_size > mRequestedSize)
			{
				// *TODO: This should not be happening any more (REALLY do not
				// expect this anymore)
				llwarns << "data_size = " << data_size << " > requested: "
						<< mRequestedSize << llendl;
				mHaveAllData = true;
				llassert_always(!mDecoding);
				mFormattedImage = NULL; // Discard any previous data we had
			}
		}
		else
		{
			// We requested data but received none (and no error), so
			// presumably we have all of it.
			mHaveAllData = true;
		}
		mRequestedSize = data_size;
	}
	else
	{
		mRequestedSize = -1; // Error
	}
	mLoaded = true;
	setHighPriority();

	return data_size;
}

// Threads: Ttc
void LLTextureFetchWorker::callbackCacheRead(bool success,
											 LLImageFormatted* image,
											 S32 imagesize, bool islocal)
{
	mWorkMutex.lock();

	if (mState != LOAD_FROM_TEXTURE_CACHE)
	{
		LL_DEBUGS("TextureFetch") << "Unexpected read callback for " << mID
								  << " with state = " << mState << LL_ENDL;
		mReadResponder = NULL;
		mWorkMutex.unlock();
		return;
	}

	if (success)
	{
		llassert_always(imagesize >= 0);
		mFileSize = imagesize;
		mFormattedImage = image;
		mImageCodec = image->getCodec();
		mInLocalCache = islocal;
		mLoaded = true;
		if (mFileSize != 0 && mFormattedImage->getDataSize() >= mFileSize)
		{
			mHaveAllData = true;
		}
	}

	mReadResponder = NULL;
	mState = CACHE_POST;
	setHighPriority();

	mWorkMutex.unlock();
}

// Threads: Ttc
void LLTextureFetchWorker::callbackCacheWrite(bool success)
{
	mWorkMutex.lock();

	if (mState != WAIT_ON_WRITE)
	{
 		LL_DEBUGS("TextureFetch") << "Unexpected write callback for " << mID
								  << " with state = " << mState << LL_ENDL;
		mWriteResponder = NULL;
		mWorkMutex.unlock();
		return;
	}

	mWriteResponder = NULL;
	mWritten = true;
	setHighPriority();

	mWorkMutex.unlock();
}

// Threads: Tid
void LLTextureFetchWorker::callbackDecoded(bool success, LLImageRaw* raw,
										   LLImageRaw* aux)
{
	mWorkMutex.lock();

	if (!mDecoding)
	{
		LL_DEBUGS("TextureFetch") << "Aborted decode (null handle) for " << mID
								  << LL_ENDL;
		mWorkMutex.unlock();
		return;	// Aborted, ignore
	}
	mDecoding = false;
	if (mState != DECODE_IMAGE_UPDATE)
	{
		LL_DEBUGS("TextureFetch") << "Unexpected decode callback for " << mID
								  << " with state = " << mState << LL_ENDL;
		mWorkMutex.unlock();
		return;
	}
	llassert_always(mFormattedImage.notNull());

	if (success)
	{
		llassert_always(raw);
		mRawImage = raw;
		mAuxImage = aux;
		mDecodedDiscard = mFormattedImage->getDiscardLevel();
 		LL_DEBUGS("TextureFetch") << "Decode finished for " << mID
								  << ". Discard: " << mDecodedDiscard
								  << ". Raw image: "
								  << llformat("%dx%d", mRawImage->getWidth(),
											  mRawImage->getHeight())
								  << LL_ENDL;
	}
	else
	{
		llwarns << "Decode failed: " << mID << " Discard: "
				<< (S32)mFormattedImage->getDiscardLevel() << llendl;
		removeFromCache();
		mDecodedDiscard = -1; // Redundant, here for clarity and paranoia
	}
	mDecoded = true;
	setHighPriority();

	mWorkMutex.unlock();
}

// Tuning/Parameterization Constants

// Maximum requests to have active in HTTP:
S32 LLTextureFetch::sMaxRequestsInQueue = 64;

// Active level at which to refill:
S32 LLTextureFetch::sMinRequestsInQueue = 32;

LLTextureFetch::LLTextureFetch()
:	LLWorkerThread("Texture fetch"),
	mDebugPause(false),
	mApproxNumRequests(0),
	mNumHTTPRequests(0),
	mTextureBandwidth(0),
	mHTTPTextureBits(0),
	mHttpSemaphore(0),
	mHttpLowWater(sMinRequestsInQueue),
	mHttpHighWater(sMaxRequestsInQueue)
{
	mHttpRequest = new LLCore::HttpRequest;
	mHttpOptions = DEFAULT_HTTP_OPTIONS;
	mHttpOptionsWithHeaders = DEFAULT_HTTP_OPTIONS;
	mHttpOptionsWithHeaders->setWantHeaders(true);
	mHttpHeaders = DEFAULT_HTTP_HEADERS;
	mHttpHeaders->append(HTTP_OUT_HEADER_ACCEPT,
						 HTTP_CONTENT_IMAGE_X_J2C);
	LLAppCoreHttp& app_core_http = gAppViewerp->getAppCoreHttp();
	mHttpPolicyClass = app_core_http.getPolicy(LLAppCoreHttp::AP_TEXTURE);
}

LLTextureFetch::~LLTextureFetch()
{
	clearDeleteList();

	mHttpWaitResource.clear();

	delete mHttpRequest;
	mHttpRequest = NULL;
}

bool LLTextureFetch::createRequest(FTType f_type, const std::string& url,
								   const LLUUID& id, const LLHost& host,
								   F32 priority, S32 w, S32 h, S32 c,
								   S32 desired_discard, bool needs_aux,
								   bool can_use_http)
{
	if (mDebugPause)
	{
		return false;
	}

	if (id.isNull())
	{
		LL_DEBUGS("TextureFetch") << "Null ID texture fetch request. Ignored."
								  << LL_ENDL;
		return false;
	}

	if (f_type == FTT_SERVER_BAKE)
	{
		LL_DEBUGS("Avatar") << "Requesting " << id << " " << w << "x" << h
							<< " discard " << desired_discard << LL_ENDL;
	}

	LLTextureFetchWorker* worker = getWorker(id);
	if (worker && worker->mHost != host)
	{
		llwarns << "Request creation for " << id
				<< " called with multiple hosts: " << host << " != "
				<< worker->mHost << llendl;
		deleteRequest(id);
		worker = NULL;
		return false;
	}

	S32 desired_size;
	std::string exten = gDirUtilp->getExtension(url);
	if (!url.empty() && !exten.empty() &&
		LLImageBase::getCodecFromExtension(exten) != IMG_CODEC_J2C)
	{
		// Only do partial requests for J2C at the moment
		desired_size = MAX_IMAGE_DATA_SIZE;
		desired_discard = 0;
	}
	else if (desired_discard == 0)
	{
		// If we want the entire image, and we know its size, then get it all
		// (calcDataSizeJ2C() below makes assumptions about how the image was
		// compressed - this code ensures that when we request the entire
		// image, we really do get it).
		desired_size = MAX_IMAGE_DATA_SIZE;
	}
	else if (w * h * c > 0)
	{
		// If the requester knows the dimensions of the image, this will
		// calculate how much data we need without having to parse the header.
		desired_size = LLImageJ2C::calcDataSizeJ2C(w, h, c, desired_discard);
	}
	else
	{
		// If the requester knows nothing about the file, we fetch the smallest
		// amount of data at the lowest resolution (highest discard level)
		// possible.
		desired_size = TEXTURE_CACHE_ENTRY_SIZE;
		desired_discard = MAX_DISCARD_LEVEL;
	}

	if (worker)
	{
		if (worker->wasAborted())
		{
			// Need to wait for previous aborted request to complete
			return false;
		}
		worker->lockWorkMutex();
#if 0	// This causes issues with failures to retry some textures (e.g. for
		// land patches). HB
		if (worker->mState == LLTextureFetchWorker::DONE &&
			worker->mDesiredDiscard == desired_discard &&
			worker->mDesiredSize == llmax(desired_size,
										  TEXTURE_CACHE_ENTRY_SIZE))
		{
			// Similar request has failed or is in a transitional state
			worker->unlockWorkMutex();
			return false;
		}
#endif
		++worker->mActiveCount;
		worker->mNeedsAux = needs_aux;
		worker->setImagePriority(priority);
		worker->setDesiredDiscard(desired_discard, desired_size);
		worker->setCanUseHTTP(can_use_http);
		if (can_use_http && !url.empty())
		{
			worker->setUrl(url);
		}
		if (worker->haveWork())
		{
			worker->unlockWorkMutex();
		}
		else
		{
			worker->mState = LLTextureFetchWorker::INIT;
			worker->unlockWorkMutex();
			worker->addWork(0, worker->getStartingPriority());
		}
	}
	else
	{
		worker = new LLTextureFetchWorker(f_type, url, id, host, priority,
										  desired_discard, desired_size);
		mQueueMutex.lock();
		mRequestMap[id] = worker;
		mApproxNumRequests = (U32)mRequestMap.size();
		mQueueMutex.unlock();

		worker->lockWorkMutex();
		++worker->mActiveCount;
		worker->mNeedsAux = needs_aux;
		worker->setCanUseHTTP(can_use_http);
		worker->unlockWorkMutex();
	}

 	LL_DEBUGS("TextureFetch") << "Requested: " << id << ". f_type: "
							  << fttype_to_string(f_type) << ". Discard: "
							  << desired_discard << ". Size: " << desired_size
							  << LL_ENDL;
	return true;
}

void LLTextureFetch::addToNetworkQueue(LLTextureFetchWorker* worker)
{
	mQueueMutex.lock();
	bool in_request_map = mRequestMap.count(worker->mID) != 0;
	mQueueMutex.unlock();

	mNetworkQueueMutex.lock();
	if (in_request_map)
	{
		// Only add to the queue if in the request map, i.e. a delete has not
		// been requested.
		mNetworkQueue.emplace(worker->mID);
	}
	for (cancel_queue_t::iterator iter1 = mCancelQueue.begin(),
								  end = mCancelQueue.end();
		 iter1 != end; ++iter1)
	{
		iter1->second.erase(worker->mID);
	}
	mNetworkQueueMutex.unlock();
}

void LLTextureFetch::removeFromNetworkQueue(LLTextureFetchWorker* worker,
											bool cancel)
{
	mNetworkQueueMutex.lock();
	size_t erased = mNetworkQueue.erase(worker->mID);
	if (cancel && erased > 0)
	{
		mCancelQueue[worker->mHost].emplace(worker->mID);
	}
	mNetworkQueueMutex.unlock();
}

// Threads: T*
//
// protected
void LLTextureFetch::addToHTTPQueue(const LLUUID& id)
{
	mNetworkQueueMutex.lock();
	mHTTPTextureQueue.emplace(id);	// May be insert (if not already there)
	mNumHTTPRequests = mHTTPTextureQueue.size();
	mNetworkQueueMutex.unlock();
}

// Threads: T*
void LLTextureFetch::removeFromHTTPQueue(const LLUUID& id, S32 received_size)
{
	mNetworkQueueMutex.lock();
	mHTTPTextureQueue.erase(id);	// May be remove (if actually there)
	mNumHTTPRequests = mHTTPTextureQueue.size();
	// Approximate - does not include header bits:
	mHTTPTextureBits += received_size * 8;
	mNetworkQueueMutex.unlock();
}

bool LLTextureFetch::deleteRequest(const LLUUID& id, bool force)
{
	mQueueMutex.lock();

	LLTextureFetchWorker* worker = getWorkerAfterLock(id);
	if (worker && (force || worker->deleteOK()))
	{
		mRequestMap.erase(worker->mID);
		mApproxNumRequests = (U32)mRequestMap.size();
		mQueueMutex.unlock();

		removeFromNetworkQueue(worker, true);
		llassert_always(!(worker->getFlags(LLWorkerClass::WCF_DELETE_REQUESTED)));
		worker->scheduleDelete();

		return true;
	}

	mQueueMutex.unlock();
	return false;
}

uuid_list_t LLTextureFetch::deleteAllRequests()
{
	llinfos << "Deleting all requests..." << llendl;

	// Pause the fetcher to avoid race conditions between locking and unlocking
	// of the queue.
	mDebugPause = true;

	// First create a vector of all texture UUIDs associated with a worker
	uuid_vec_t fetching_ids;
	fetching_ids.resize(mRequestMap.size());
	mQueueMutex.lock();
	for (map_t::const_iterator it = mRequestMap.begin(),
							   end = mRequestMap.end();
		 it != end; ++it)
	{
		LLTextureFetchWorker* worker = it->second;
		if (worker)	// Paranoia
		{
			fetching_ids.emplace_back(it->first);
		}
	}
	mQueueMutex.unlock();

	uuid_list_t deleted_ids;
	fetching_ids.resize(fetching_ids.size());

	// Then, delete all workers that are still around and are in a state
	// where they can actually be deleted...
	for (U32 i = 0, count = fetching_ids.size(); i < count; ++i)
	{
		const LLUUID& tex_id = fetching_ids[i];
		LLViewerFetchedTexture* texp = gTextureList.findImage(tex_id);
		if (texp &&
			(texp->getDontDiscard() ||
			 texp->getBoostLevel() >= LLGLTexture::BOOST_SUPER_HIGH))
		{
			// Do not interrupt the fetching of important images ! HB
			continue;
		}

		mQueueMutex.lock();
		map_t::iterator iter = mRequestMap.find(tex_id);
		bool can_delete = iter != mRequestMap.end();
		mQueueMutex.unlock();
		if (!can_delete)
		{
			// Request worker is already gone
			continue;
		}

		if (deleteRequest(tex_id, false))	// false = do not force
		{
			LL_DEBUGS("TextureFetch") << "Deleted the request for texture: "
									  << tex_id << LL_ENDL;
			deleted_ids.emplace(tex_id);
		}
		else
		{
			LL_DEBUGS("TextureFetch") << "Request for texture " << tex_id
									  << " cannot be deleted now." << LL_ENDL;
		}
	}

	mDebugPause = false;	// Un-pause
	llinfos << "All requests deleted." << llendl;

	return deleted_ids;
}

U32 LLTextureFetch::getNumRequests()
{
	mQueueMutex.lock();
	U32 size = (U32)mRequestMap.size();
	mQueueMutex.unlock();
	return size;
}

// Call mQueueMutex.lock() first !
// Threads: T*
// Locks: Mfq
LLTextureFetchWorker* LLTextureFetch::getWorkerAfterLock(const LLUUID& id)
{
	map_t::iterator iter = mRequestMap.find(id);
	return iter == mRequestMap.end() ? NULL : iter->second;
}

// Threads: T*
LLTextureFetchWorker* LLTextureFetch::getWorker(const LLUUID& id)
{
	mQueueMutex.lock();
	LLTextureFetchWorker* worker = getWorkerAfterLock(id);
	mQueueMutex.unlock();
	return worker;
}

bool LLTextureFetch::getRequestFinished(const LLUUID& id, S32& discard_level,
										LLPointer<LLImageRaw>& raw,
										LLPointer<LLImageRaw>& aux,
										LLCore::HttpStatus& last_http_get_status)
{
	LLTextureFetchWorker* worker = getWorker(id);
	if (!worker || worker->wasAborted())
	{
		return true;
	}

	if (!worker->haveWork())
	{
		// Should only happen if we set mDebugPause...
		if (!mDebugPause)
		{
			LL_DEBUGS("TextureFetch") << "Adding work for inactive worker: "
									  << id << LL_ENDL;
			worker->addWork(0, worker->getStartingPriority());
		}
		return false;
	}

	if (worker->checkWork())
	{
		worker->lockWorkMutex();
		last_http_get_status = worker->mGetStatus;
		discard_level = worker->mDecodedDiscard;
		raw = worker->mRawImage;
		aux = worker->mAuxImage;
		LL_DEBUGS("TextureFetch") << id << ": request finished. State: "
								  << worker->mState << ". Discard: "
								  << discard_level << LL_ENDL;
		worker->unlockWorkMutex();
		return true;
	}

	worker->lockWorkMutex();
	if (worker->mDecodedDiscard >= 0 &&
		(worker->mDecodedDiscard < discard_level || discard_level < 0) &&
		worker->mState >= LLTextureFetchWorker::WAIT_ON_WRITE)
	{
		// Not finished, but data is ready
		discard_level = worker->mDecodedDiscard;
		raw = worker->mRawImage;
		aux = worker->mAuxImage;
	}
	worker->unlockWorkMutex();
	return false;
}

bool LLTextureFetch::updateRequestPriority(const LLUUID& id, F32 priority)
{
	LLTextureFetchWorker* worker = getWorker(id);
	if (worker)
	{
		worker->lockWorkMutex();
		worker->setImagePriority(priority);
		worker->unlockWorkMutex();
		return true;
	}
	return false;
}

// Overridden since we also need to lock mQueueMutex for this operation.
// Threads: T*
//virtual
size_t LLTextureFetch::getPending()
{
	mQueueMutex.lock();
	size_t res = LLQueuedThread::getPending();
	mQueueMutex.unlock();
	return res;
}

// WORKER THREAD
//virtual
void LLTextureFetch::threadedUpdate()
{
	llassert_always(mHttpRequest);

	LLAppCoreHttp& app_core_http = gAppViewerp->getAppCoreHttp();
	if (app_core_http.isPipelined(LLAppCoreHttp::AP_TEXTURE))
	{
		mHttpHighWater = 4 * sMaxRequestsInQueue;
		mHttpLowWater = 4 * sMinRequestsInQueue;
	}
	else
	{
		mHttpHighWater = sMaxRequestsInQueue;
		mHttpLowWater = sMinRequestsInQueue;
	}

	// Release waiters
	releaseHttpWaiters();

	// Deliver all completion notifications
	LLCore::HttpStatus status = mHttpRequest->update(0);
	if (!status)
	{
		llinfos_once << "Problem during HTTP servicing. Reason: "
					 << status.toString() << llendl;
	}
}

// MAIN THREAD
//virtual
size_t LLTextureFetch::update()
{
	mNetworkQueueMutex.lock();
	gTextureList.sTextureBits += mHTTPTextureBits;
	mHTTPTextureBits = 0;
	mNetworkQueueMutex.unlock();

	size_t res = LLWorkerThread::update();

	if (!mDebugPause && LLStartUp::getStartupState() > STATE_AGENT_SEND)
	{
		// STATE_AGENT_SEND is the startup state when
		// send_complete_agent_movement() message is sent. Before this, the
		// RequestImages message sent by sendRequestListToSimulators would not
		// work, so do not bother trying.
		sendRequestListToSimulators();
	}

	return res;
}

// Threads: Tmain
void LLTextureFetch::sendRequestListToSimulators()
{
	// All requests
	constexpr F32 REQUEST_DELTA_TIME = 0.10f; // 10 fps

	// Sim requests
	constexpr S32 IMAGES_PER_REQUEST = 50;
	constexpr F32 SIM_LAZY_FLUSH_TIMEOUT = 10.f; // temp
	constexpr F32 MIN_REQUEST_TIME = 1.f;
	constexpr F32 MIN_DELTA_PRIORITY = 1000.f;

	// Periodically, gather the list of textures that need data from the network
	// And send the requests out to the simulators
	static LLFrameTimer timer;
	if (timer.getElapsedTimeF32() < REQUEST_DELTA_TIME)
	{
		return;
	}
	timer.reset();

	// Send requests
	typedef std::set<LLTextureFetchWorker*,
					 LLTextureFetchWorker::Compare> request_list_t;
	typedef std::map<LLHost, request_list_t> work_request_map_t;
	work_request_map_t requests;

	mNetworkQueueMutex.lock();
	for (uuid_list_t::iterator iter = mNetworkQueue.begin();
		 iter != mNetworkQueue.end(); )
	{
		uuid_list_t::iterator curiter = iter++;
		LLTextureFetchWorker* req = getWorker(*curiter);
		if (!req)
		{
			mNetworkQueue.erase(curiter);
			continue; // paranoia
		}
		if (req->mState != LLTextureFetchWorker::LOAD_FROM_NETWORK &&
			req->mState != LLTextureFetchWorker::LOAD_FROM_SIMULATOR)
		{
			// We already received our URL, remove from the queue
			llwarns << "Worker: " << req->mID
					<< " in mNetworkQueue but in wrong state: " << req->mState
					<< llendl;
			mNetworkQueue.erase(curiter);
			continue;
		}
		if (req->mSentRequest == LLTextureFetchWorker::SENT_SIM &&
			req->mTotalPackets > 0 &&
			req->mLastPacket >= req->mTotalPackets - 1)
		{
			// We have all the packets.
			continue;
		}
		F32 elapsed = req->mRequestedTimer.getElapsedTimeF32();
		F32 delta_priority = fabsf(req->mRequestedPriority -
								   req->mImagePriority);
		if (req->mSimRequestedDiscard != req->mDesiredDiscard ||
			elapsed >= SIM_LAZY_FLUSH_TIMEOUT ||
			(elapsed >= MIN_REQUEST_TIME &&
			 delta_priority > MIN_DELTA_PRIORITY))
		{
			requests[req->mHost].insert(req);
		}
	}
	mNetworkQueueMutex.unlock();

	LLMessageSystem* msg = gMessageSystemp;

	for (work_request_map_t::iterator iter1 = requests.begin(),
									  end1 = requests.end();
		 iter1 != end1; ++iter1)
	{
		LLHost host = iter1->first;
		// Invalid host = use agent host
		if (host.isInvalid())
		{
			host = gAgent.getRegionHost();
		}

		S32 sim_request_count = 0;

		for (request_list_t::iterator iter2 = iter1->second.begin(),
									  end2 = iter1->second.end();
			 iter2 != end2; ++iter2)
		{
			LLTextureFetchWorker* req = *iter2;
			if (msg)
			{
				if (req->mSentRequest != LLTextureFetchWorker::SENT_SIM)
				{
					// Initialize packet data based on data read from cache
					req->lockWorkMutex();
					req->setupPacketData();
					req->unlockWorkMutex();
				}
				if (0 == sim_request_count)
				{
					msg->newMessageFast(_PREHASH_RequestImage);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
					msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				}
				S32 packet = req->mLastPacket + 1;
				msg->nextBlockFast(_PREHASH_RequestImage);
				msg->addUUIDFast(_PREHASH_Image, req->mID);
				msg->addS8Fast(_PREHASH_DiscardLevel,
							   (S8)req->mDesiredDiscard);
				msg->addF32Fast(_PREHASH_DownloadPriority,
								req->mImagePriority);
				msg->addU32Fast(_PREHASH_Packet, packet);
				msg->addU8Fast(_PREHASH_Type, req->mType);

				req->lockWorkMutex();
				req->mSentRequest = LLTextureFetchWorker::SENT_SIM;
				req->mSimRequestedDiscard = req->mDesiredDiscard;
				req->mRequestedPriority = req->mImagePriority;
				req->mRequestedTimer.reset();
				req->unlockWorkMutex();

				if (++sim_request_count >= IMAGES_PER_REQUEST)
				{
					msg->sendSemiReliable(host, NULL, NULL);
					sim_request_count = 0;
				}
			}
		}
		if (msg && sim_request_count > 0 &&
			sim_request_count < IMAGES_PER_REQUEST)
		{
			msg->sendSemiReliable(host, NULL, NULL);
			sim_request_count = 0;
		}
	}

	// Send cancelations
	mNetworkQueueMutex.lock();
	if (msg && !mCancelQueue.empty())
	{
		for (cancel_queue_t::iterator iter1 = mCancelQueue.begin(),
									  end1 = mCancelQueue.end();
			 iter1 != end1; ++iter1)
		{
			LLHost host = iter1->first;
			if (host.isInvalid())
			{
				host = gAgent.getRegionHost();
			}
			S32 request_count = 0;
			for (uuid_list_t::iterator iter2 = iter1->second.begin(),
									   end2 = iter1->second.end();
				 iter2 != end2; ++iter2)
			{
				if (request_count == 0)
				{
					msg->newMessageFast(_PREHASH_RequestImage);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
					msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				}
				msg->nextBlockFast(_PREHASH_RequestImage);
				msg->addUUIDFast(_PREHASH_Image, *iter2);
				msg->addS8Fast(_PREHASH_DiscardLevel, -1);
				msg->addF32Fast(_PREHASH_DownloadPriority, 0);
				msg->addU32Fast(_PREHASH_Packet, 0);
				msg->addU8Fast(_PREHASH_Type, 0);

				if (++request_count >= IMAGES_PER_REQUEST)
				{
					msg->sendSemiReliable(host, NULL, NULL);
					request_count = 0;
				}
			}
			if (request_count > 0 && request_count < IMAGES_PER_REQUEST)
			{
				msg->sendSemiReliable(host, NULL, NULL);
			}
		}
		mCancelQueue.clear();
	}
	mNetworkQueueMutex.unlock();
}

bool LLTextureFetch::receiveImageHeader(const LLHost& host, const LLUUID& id,
										U8 codec, U16 packets, U32 totalbytes,
										U16 data_size, U8* data)
{
	bool res = true;

	LLTextureFetchWorker* worker = getWorker(id);
	if (!worker)
	{
		LL_DEBUGS("TextureFetch") << "Received header for non active worker: "
								  << id << LL_ENDL;
		res = false;
	}
	else if (worker->mState != LLTextureFetchWorker::LOAD_FROM_NETWORK ||
			 worker->mSentRequest != LLTextureFetchWorker::SENT_SIM)
	{
		LL_DEBUGS("TextureFetch") << "Worker: " << id << ". State: "
								  << e_state_name[worker->mState]
								  << ". Sent: " << worker->mSentRequest
								  << LL_ENDL;
		res = false;
	}
	else if (worker->mLastPacket != -1)
	{
		// check to see if we have gotten this packet before
		LL_DEBUGS("TextureFetch") << "Received duplicate header for: " << id
								  << LL_ENDL;
		res = false;
	}
	else if (!data_size)
	{
		LL_DEBUGS("TextureFetch") << "Empty image header for " << id
								  << LL_ENDL;
		res = false;
	}

	if (!res)
	{
		mNetworkQueueMutex.lock();
		mCancelQueue[host].emplace(id);
		mNetworkQueueMutex.unlock();
		return false;
	}

	worker->lockWorkMutex();

	// Copy header data into image object
	worker->mImageCodec = codec;
	worker->mTotalPackets = packets;
	worker->mFileSize = (S32)totalbytes;
	llassert_always(totalbytes > 0);
	llassert_always(data_size == FIRST_PACKET_SIZE ||
					data_size == worker->mFileSize);
	res = worker->insertPacket(0, data, data_size);
	worker->setHighPriority();
	worker->mState = LLTextureFetchWorker::LOAD_FROM_SIMULATOR;
	worker->unlockWorkMutex();

	return res;
}

bool LLTextureFetch::receiveImagePacket(const LLHost& host, const LLUUID& id,
										U16 packet_num, U16 data_size,
										U8* data)
{
	bool res = true;

	LLTextureFetchWorker* worker = getWorker(id);
	if (!worker)
	{
		LL_DEBUGS("TextureFetch") << "Received packet " << packet_num
								  << " for non active worker: " << id
								  << LL_ENDL;
		res = false;
	}
	else if (worker->mLastPacket == -1)
	{
		LL_DEBUGS("TextureFetch") << "Received packet " << packet_num
								  << " before header for: " << id << LL_ENDL;
		res = false;
	}
	else if (!data_size)
	{
		LL_DEBUGS("TextureFetch") << "Empty image header for " << id
								  << LL_ENDL;
		res = false;
	}

	if (!res)
	{
		mNetworkQueueMutex.lock();
		mCancelQueue[host].emplace(id);
		mNetworkQueueMutex.unlock();
		return false;
	}

	worker->lockWorkMutex();

	res = worker->insertPacket(packet_num, data, data_size);

	if (worker->mState == LLTextureFetchWorker::LOAD_FROM_SIMULATOR ||
		worker->mState == LLTextureFetchWorker::LOAD_FROM_NETWORK)
	{
		worker->setHighPriority();
		worker->mState = LLTextureFetchWorker::LOAD_FROM_SIMULATOR;
	}
	else
	{
		LL_DEBUGS("TextureFetch") << "Packet " << packet_num << "/"
								  << worker->mLastPacket << " for worker "
								  << id << " in state "
								  << e_state_name[worker->mState] << LL_ENDL;
		removeFromNetworkQueue(worker, true); // failsafe
	}

	worker->unlockWorkMutex();

	return res;
}

bool LLTextureFetch::isFromLocalCache(const LLUUID& id)
{
	bool from_cache = false;

	LLTextureFetchWorker* worker = getWorker(id);
	if (worker)
	{
		worker->lockWorkMutex();
		from_cache = worker->mInLocalCache;
		worker->unlockWorkMutex();
	}

	return from_cache;
}

S32 LLTextureFetch::getFetchState(const LLUUID& id,
								  F32& data_progress_p,
								  F32& requested_priority_p,
								  U32& fetch_priority_p,
								  F32& fetch_dtime_p,
								  F32& request_dtime_p,
								  bool& can_use_http)
{
	S32 state = LLTextureFetchWorker::INVALID;
	F32 data_progress = 0.0f;
	F32 requested_priority = 0.0f;
	F32 fetch_dtime = 999999.f;
	F32 request_dtime = 999999.f;
	U32 fetch_priority = 0;

	LLTextureFetchWorker* worker = getWorker(id);
	if (worker && worker->haveWork())
	{
		worker->lockWorkMutex();
		state = worker->mState;
		fetch_dtime = worker->mFetchTimer.getElapsedTimeF32();
		request_dtime = worker->mRequestedTimer.getElapsedTimeF32();
		if (worker->mFileSize > 0)
		{
			if (state == LLTextureFetchWorker::LOAD_FROM_SIMULATOR)
			{
				S32 data_size = FIRST_PACKET_SIZE +
								(worker->mLastPacket - 1) *
								MAX_IMG_PACKET_SIZE;
				data_size = llmax(data_size, 0);
				data_progress = (F32)data_size / (F32)worker->mFileSize;
			}
			else if (worker->mFormattedImage.notNull())
			{
				data_progress = (F32)worker->mFormattedImage->getDataSize() /
								(F32)worker->mFileSize;
			}
		}
		if (state >= LLTextureFetchWorker::LOAD_FROM_NETWORK &&
			state <= LLTextureFetchWorker::WAIT_HTTP_REQ)
		{
			requested_priority = worker->mRequestedPriority;
		}
		else
		{
			requested_priority = worker->mImagePriority;
		}
		fetch_priority = worker->getPriority();
		can_use_http = worker->getCanUseHTTP();
		worker->unlockWorkMutex();
	}

	data_progress_p = data_progress;
	requested_priority_p = requested_priority;
	fetch_priority_p = fetch_priority;
	fetch_dtime_p = fetch_dtime;
	request_dtime_p = request_dtime;

	return state;
}

// Threads: Ttf
void LLTextureFetch::addHttpWaiter(const LLUUID& tid)
{
	mNetworkQueueMutex.lock();
	mHttpWaitResource.emplace(tid);
	mNetworkQueueMutex.unlock();
}

// Threads: Ttf
void LLTextureFetch::removeHttpWaiter(const LLUUID& tid)
{
	mNetworkQueueMutex.lock();
	uuid_list_t::iterator iter = mHttpWaitResource.find(tid);
	if (iter != mHttpWaitResource.end())
	{
		mHttpWaitResource.erase(iter);
	}
	mNetworkQueueMutex.unlock();
}

bool LLTextureFetch::isHttpWaiter(const LLUUID& tid)
{
	mNetworkQueueMutex.lock();
	uuid_list_t::iterator iter = mHttpWaitResource.find(tid);
	bool ret = mHttpWaitResource.end() != iter;
	mNetworkQueueMutex.unlock();
	return ret;
}

// Release as many requests as permitted from the WAIT_HTTP_RESOURCE2 state to
// the SEND_HTTP_REQ state based on their current priority.
//
// This data structures and code associated with this looks a bit indirect and
// naive but it is done in the name of safety. An ordered container may become
// invalid from time to time due to priority changes caused by actions in other
// threads. State itself could also suffer the same fate with cancelled
// operations. Even done this way, I am not fully trusting we are truly safe.
// This module is due for a major refactoring and we will deal with it then.
//
// Threads: Ttf
// Locks: -Mw (must not hold any worker when called)
void LLTextureFetch::releaseHttpWaiters()
{
	if (mHttpSemaphore >= mHttpHighWater)
	{
		return;
	}

	mNetworkQueueMutex.lock();
	
	uuid_list_t::iterator iter;
	uuid_list_t::iterator end = mHttpWaitResource.end();
	for (iter = mHttpWaitResource.begin(); iter != end; ++iter)
	{
		LLTextureFetchWorker* worker(getWorker(*iter));
		if (worker)
		{
			if (!worker->acquireHttpSemaphore())
			{
				break;
			}
			worker->lockWorkMutex();
			worker->mState = LLTextureFetchWorker::SEND_HTTP_REQ;
			worker->setHighPriority();
			worker->unlockWorkMutex();
		}
	}
	mHttpWaitResource.erase(mHttpWaitResource.begin(), iter);

	mNetworkQueueMutex.unlock();
}
