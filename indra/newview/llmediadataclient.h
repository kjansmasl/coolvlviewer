/**
 * @file llmediadataclient.h
 * @brief class for queueing up requests to the media service
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLMEDIADATACLIENT_H
#define LL_LLMEDIADATACLIENT_H

#include <list>
#include <memory>
#include <set>

#include "llcorehttputil.h"
#include "lleventtimer.h"
#include "llhttpsdhandler.h"
#include "llpointer.h"
#include "llrefcount.h"

// Link seam for LLVOVolume
class LLMediaDataClientObject : public LLRefCount
{
public:
	// Get the number of media data items
	virtual U8 getMediaDataCount() const = 0;
	// Get the media data at index, as an LLSD
	virtual LLSD getMediaDataLLSD(U8 index) const = 0;
	// Return true if the current URL for the face in the media data matches
	// the specified URL.
	virtual bool isCurrentMediaUrl(U8 index, const std::string& url) const = 0;
	// Get this object's UUID
	virtual LLUUID getID() const = 0;
	// Navigate back to previous URL
	virtual void mediaNavigateBounceBack(U8 index) = 0;
	// Does this object have media?
	virtual bool hasMedia() const = 0;
	// Update the object's media data to the given array
	virtual void updateObjectMediaData(LLSD const& media_data_array,
									   const std::string& version_string) = 0;
	// Return the total "interest" of the media (on-screen area)
	virtual F64 getMediaInterest() const = 0;
	// Return the given cap url
	virtual const std::string& getCapabilityUrl(const char* name) const = 0;
	// Return whether the object has been marked dead
	virtual bool isDead() const = 0;
	// Returns a media version number for the object
	virtual U32 getMediaVersion() const = 0;
	// Returns whether the object is "interesting enough" to fetch
	virtual bool isInterestingEnough() const = 0;
	// Returns whether we've seen this object yet or not
	virtual bool isNew() const = 0;

	// Smart pointer type
	typedef LLPointer<LLMediaDataClientObject> ptr_t;
};

// This class creates a priority queue for requests and abstracts the cap URL,
// the request, and the handler.
class LLMediaDataClient : public LLRefCount
{
	friend class PredicateMatchRequest;

protected:
	LOG_CLASS(LLMediaDataClient);

public:
	static constexpr F32 QUEUE_TIMER_DELAY = 1.f;				// Seconds
	static constexpr F32 UNAVAILABLE_RETRY_TIMER_DELAY = 5.f;	// Seconds
	static constexpr U32 MAX_RETRIES = 4;
	static constexpr U32 MAX_SORTED_QUEUE_SIZE = 10000;
	static constexpr U32 MAX_ROUND_ROBIN_QUEUE_SIZE = 10000;

	LLMediaDataClient(F32 queue_timer_delay = QUEUE_TIMER_DELAY,
					  F32 retry_timer_delay = UNAVAILABLE_RETRY_TIMER_DELAY,
					  U32 max_retries = MAX_RETRIES,
					  U32 max_sorted_queue_size = MAX_SORTED_QUEUE_SIZE,
					  U32 max_round_robin_queue_size = MAX_ROUND_ROBIN_QUEUE_SIZE);

	LL_INLINE F32 getRetryTimerDelay() const		{ return mRetryTimerDelay; }

	// Returns true if the queue is empty
	virtual bool isEmpty() const;

	// Returns true if the given object is in the queue
	virtual bool isInQueue(const LLMediaDataClientObject::ptr_t& object);

	// Removes the given object from the queue. Returns true if the given
	// object is removed.
	virtual void removeFromQueue(const LLMediaDataClientObject::ptr_t& object);

	// Called only by the Queue timer and tests (potentially)
	virtual bool processQueueTimer();

protected:
	~LLMediaDataClient() override;	// Use unref

	// Request (pure virtual base class for requests in the queue)
	class Request : public std::enable_shared_from_this<Request>
	{
	public:
		typedef std::shared_ptr<Request> ptr_t;

		// Subclasses must implement this to build a payload for their request
		// type...
		virtual LLSD getPayload() const = 0;
		// ... and must create the correct type of handler.
		virtual LLCore::HttpHandler::ptr_t createHandler() = 0;

		LL_INLINE virtual std::string getURL()			{ return ""; }

		enum Type { GET, UPDATE, NAVIGATE, ANY };

		virtual ~Request()								{}

	protected:
		// The only way to create one of these is through a subclass.
		Request(Type in_type, LLMediaDataClientObject* obj,
				LLMediaDataClient* mdc, S32 face = -1);

	public:
		LL_INLINE LLMediaDataClientObject* getObject() const
		{
			return mObject;
		}

		LL_INLINE U32 getNum() const				{ return mNum; }
		LL_INLINE U32 getRetryCount() const			{ return mRetryCount; }
		LL_INLINE void incRetryCount()				{ ++mRetryCount; }
		LL_INLINE Type getType() const				{ return mType; }
		LL_INLINE F64 getScore() const				{ return mScore; }

		// Note: may return an empty string !
		const std::string& getCapability() const;
		const char* getCapName() const;
		const char* getTypeAsString() const;

		// Re-enqueue thyself
		void reEnqueue();

		F32 getRetryTimerDelay() const;
		U32 getMaxNumRetries() const;

		LL_INLINE bool isObjectValid() const
		{
			return mObject.notNull() && !mObject->isDead();
		}

		LL_INLINE bool isNew() const
		{
			return isObjectValid() && mObject->isNew();
		}

		void updateScore();

		void markDead();
		bool isDead();

		void startTracking();
		void stopTracking();

		friend std::ostream& operator<<(std::ostream& s, const Request& q);

		LL_INLINE const LLUUID& getID() const		{ return mObjectID; }
		LL_INLINE S32 getFace() const				{ return mFace; }

		LL_INLINE bool isMatch(const Request::ptr_t& other,
							   Type match_type = ANY) const
		{
			return (match_type == ANY || mType == other->mType) &&
				   mFace == other->mFace && mObjectID == other->mObjectID;
		}

	protected:
		LLMediaDataClientObject::ptr_t	mObject;

	private:
		// Back pointer to the MDC...not a ref !
		LLMediaDataClient*				mMDC;

		LLUUID							mObjectID;

		Type							mType;

		// Simple tracking
		U32								mNum;
		U32								mRetryCount;
		F64								mScore;

		S32								mFace;

		static U32						sNum;
	};

	class Handler : public LLHttpSDHandler
	{
	protected:
		LOG_CLASS(LLMediaDataClient::Handler);

		virtual void onSuccess(LLCore::HttpResponse* response,
							   const LLSD& content);
		virtual void onFailure(LLCore::HttpResponse* response,
							   LLCore::HttpStatus status);

	public:
		Handler(const Request::ptr_t& request);
		LL_INLINE Request::ptr_t getRequest()		{ return mRequest; }

	private:
		Request::ptr_t mRequest;
	};

	class RetryTimer : public LLEventTimer
	{
	public:
		RetryTimer(F32 time, Request::ptr_t);
		virtual bool tick();

	private:
		// Back-pointer
		Request::ptr_t mRequest;
	};

protected:
	typedef std::list<Request::ptr_t> request_queue_t;
	typedef std::set<Request::ptr_t> request_set_t;

	// Subclasses must override to return a cap name
	virtual const char* getCapabilityName() const = 0;

	// Puts the request into a queue, appropriately handling duplicates, etc.
	virtual void enqueue(Request::ptr_t) = 0;

	virtual void serviceQueue();

	LL_INLINE virtual void serviceHttp()			{ mHttpRequest->update(0); }

	LL_INLINE virtual request_queue_t* getQueue()	{ return &mQueue; }

	// Gets the next request, removing it from the queue
	virtual Request::ptr_t dequeue();

	LL_INLINE virtual bool canServiceRequest(Request::ptr_t)
	{
		return true;
	}

	// Returns a request to the head of the queue (should only be used for
	// requests that came from dequeue
	virtual void pushBack(Request::ptr_t request);

	void trackRequest(Request::ptr_t request);
	void stopTrackingRequest(Request::ptr_t request);

	LL_INLINE bool isDoneProcessing() const
	{
		return isEmpty() && mUnQueuedRequests.empty();
	}

	void startQueueTimer();
	LL_INLINE void stopQueueTimer()					{ mQueueTimerIsRunning = false; }

private:
	static F64 getObjectScore(const LLMediaDataClientObject::ptr_t& obj);

	friend std::ostream& operator<<(std::ostream& s, const Request& q);
	friend std::ostream& operator<<(std::ostream& s, const request_queue_t& q);

	class QueueTimer : public LLEventTimer
	{
	public:
		QueueTimer(F32 time, LLMediaDataClient* mdc);
		virtual bool tick();

	private:
		// back-pointer
		LLPointer<LLMediaDataClient> mMDC;
	};

	LL_INLINE void setIsRunning(bool val)			{ mQueueTimerIsRunning = val; }

protected:
	request_queue_t					mQueue;

	// Set for keeping track of requests that are not in either queue. This
	// includes:
	//  - Requests that have been sent and are awaiting a response (pointer
	//    held by the handler)
	//  - Requests that are waiting for their retry timers to fire (pointer
	//    held by the retry timer)
	request_set_t					mUnQueuedRequests;

	LLCore::HttpRequest::ptr_t		mHttpRequest;
	LLCore::HttpHeaders::ptr_t		mHttpHeaders;
	LLCore::HttpOptions::ptr_t		mHttpOpts;
	LLCore::HttpRequest::policy_t	mHttpPolicy;

	const F32						mQueueTimerDelay;
	const F32						mRetryTimerDelay;
	const U32						mMaxNumRetries;
	const U32						mMaxSortedQueueSize;
	const U32						mMaxRoundRobinQueueSize;

private:
	bool mQueueTimerIsRunning;
};

// MediaDataClient specific for the ObjectMedia cap
class LLObjectMediaDataClient final : public LLMediaDataClient
{
protected:
	LOG_CLASS(LLObjectMediaDataClient);

public:
	LLObjectMediaDataClient(F32 queue_timer_delay = QUEUE_TIMER_DELAY,
							F32 retry_timer_delay = UNAVAILABLE_RETRY_TIMER_DELAY,
							U32 max_retries = MAX_RETRIES,
							U32 max_sorted_queue_size = MAX_SORTED_QUEUE_SIZE,
							U32 max_round_robin_queue_size = MAX_ROUND_ROBIN_QUEUE_SIZE)
	:	LLMediaDataClient(queue_timer_delay, retry_timer_delay, max_retries),
		mCurrentQueueIsTheSortedQueue(true)
	{
	}

	void fetchMedia(LLMediaDataClientObject* object);
	void updateMedia(LLMediaDataClientObject* object);

	class RequestGet final : public Request
	{
	public:
		RequestGet(LLMediaDataClientObject* obj, LLMediaDataClient* mdc);
		LLSD getPayload() const override;
		LLCore::HttpHandler::ptr_t createHandler() override;
	};

	class RequestUpdate final : public Request
	{
	public:
		RequestUpdate(LLMediaDataClientObject* obj, LLMediaDataClient* mdc);
		LLSD getPayload() const override;
		LLCore::HttpHandler::ptr_t createHandler() override;
	};

	// Returns true if the queue is empty
	virtual bool isEmpty() const;

	// Returns true if the given object is in the queue
	virtual bool isInQueue(const LLMediaDataClientObject::ptr_t& object);

	// Removes the given object from the queue. Returns true if the given
	// object is removed.
	virtual void removeFromQueue(const LLMediaDataClientObject::ptr_t& object);

	virtual bool processQueueTimer();

	virtual bool canServiceRequest(Request::ptr_t request);

protected:
	// Subclasses must override to return a cap name
	virtual const char* getCapabilityName() const;

	virtual request_queue_t* getQueue();

	// Puts the request into the appropriate queue
	virtual void enqueue(Request::ptr_t);

	class Handler : public LLMediaDataClient::Handler
	{
	protected:
       LOG_CLASS(LLObjectMediaDataClient::Handler);

		virtual void onSuccess(LLCore::HttpResponse* response,
							   const LLSD& content);

	public:
		Handler(const Request::ptr_t& request)
		:	LLMediaDataClient::Handler(request)
		{
		}
	};

private:
	// The Get/Update data client needs a second queue to avoid object updates
	// starving load-ins.
	void swapCurrentQueue();

	request_queue_t mRoundRobinQueue;
	bool mCurrentQueueIsTheSortedQueue;

	// Comparator for sorting
	static bool compareRequestScores(const Request::ptr_t& o1,
									 const Request::ptr_t& o2);
	void sortQueue();
};

// MediaDataClient specific for the ObjectMediaNavigate cap
class LLObjectMediaNavigateClient final : public LLMediaDataClient
{
protected:
	LOG_CLASS(LLObjectMediaNavigateClient);

public:
	// NOTE: from llmediaservice.h
	static const int ERROR_PERMISSION_DENIED_CODE = 8002;

	LLObjectMediaNavigateClient(F32 queue_timer_delay = QUEUE_TIMER_DELAY,
								F32 retry_timer_delay = UNAVAILABLE_RETRY_TIMER_DELAY,
								U32 max_retries = MAX_RETRIES,
								U32 max_sorted_queue_size = MAX_SORTED_QUEUE_SIZE,
								U32 max_round_robin_queue_size = MAX_ROUND_ROBIN_QUEUE_SIZE)
	:	LLMediaDataClient(queue_timer_delay, retry_timer_delay, max_retries)
	{
	}

	void navigate(LLMediaDataClientObject* object, U8 texture_index,
				  const std::string& url);

	// Puts the request into the appropriate queue
	void enqueue(Request::ptr_t req) override;

	class RequestNavigate final : public Request
	{
	public:
		RequestNavigate(LLMediaDataClientObject* obj, LLMediaDataClient* mdc,
						U8 texture_index, const std::string& url);
		LLSD getPayload() const override;
		LLCore::HttpHandler::ptr_t createHandler() override;
		LL_INLINE std::string getURL() override		{ return mURL; }

	private:
		std::string mURL;
	};

protected:
	// Subclasses must override to return a cap name
	const char* getCapabilityName() const override;

	class Handler final : public LLMediaDataClient::Handler
	{
	protected:
        LOG_CLASS(LLObjectMediaNavigateClient::Handler);

		void onSuccess(LLCore::HttpResponse* response,
					   const LLSD& content) override;
		void onFailure(LLCore::HttpResponse* response,
					   LLCore::HttpStatus status) override;

	public:
		Handler(const Request::ptr_t& request)
		:	LLMediaDataClient::Handler(request)
		{
		}

	private:
		void mediaNavigateBounceBack();
	};
};

#endif	// LL_LLMEDIADATACLIENT_H
