/**
 * @file llmediadataclient.cpp
 * @brief class for queuing up requests for media data
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

#include "llviewerprecompiledheaders.h"

#include "boost/lexical_cast.hpp"

#include "llmediadataclient.h"

#include "llhttpconstants.h"
#include "llmediaentry.h"
#include "llsdutil.h"
#include "lltextureentry.h"

#include "llviewercontrol.h"
#include "llviewerregion.h"

// When making a request:
// - Obtain the "overall interest score" of the object. This would be the sum
//   of the impls' interest scores.
// - Put the request onto a queue sorted by this score (highest score at the
//   front of the queue).
// - On a timer, once a second, pull off the head of the queue and send the
//   request.
// - Any request that gets a 503 still goes through the retry logic
//
// What's up with this queuing code ?
//
// First, a bit of background:
//
// Media on a prim was added into the system in the viewer 2.0 timeframe. In
// order to avoid changing the network format of objects, an unused field in
// the object (the "MediaURL" string) was repurposed to indicate that the
// object had media data, and also hold a sequence number and the UUID of the
// agent who last updated the data. The actual media data for objects is
// accessed via the "ObjectMedia" capability.
// Due to concerns about sim performance, requests to this capability are
// rate-limited to 5 requests every 5 seconds per agent.

// The initial implementation of LLMediaDataClient used a single queue to
// manage requests to the "ObjectMedia" cap. Requests to the cap were queued
// so that objects closer to the avatar were loaded in first, since they were
// most likely to be the ones the media performance manager would load.

// This worked in some cases, but we found that it was possible for a scripted
// object that constantly updated its media data to starve other objects,
// since the same queue contained both requests to load previously unseen media
// data and requests to fetch media data in response to object updates.

// The solution for this we came up with was to have two queues. The sorted
// queue contains requests to fetch media data for objects that don't have it
// yet, and the round-robin queue contains requests to update media data for
// objects that have already completed their initial load.  When both queues
// are non-empty, the code ping-pongs between them so that updates cannot
// completely block initial load-in.

// << operators
std::ostream& operator<<(std::ostream& s,
						 const LLMediaDataClient::request_queue_t& q);
std::ostream& operator<<(std::ostream& s,
						 const LLMediaDataClient::Request& q);

///////////////////////////////////////////////////////////////////////////////
// PredicateMatchRequest class
// Unary predicate for matching requests in collections by either the request
// or by UUID
///////////////////////////////////////////////////////////////////////////////

class PredicateMatchRequest
{
public:
	PredicateMatchRequest(const LLMediaDataClient::Request::ptr_t& request,
						  LLMediaDataClient::Request::Type match_type =
							LLMediaDataClient::Request::ANY)
	:	mRequest(request),
		mMatchType(match_type)
	{
	}

	PredicateMatchRequest(const LLUUID& id,
						  LLMediaDataClient::Request::Type match_type =
							LLMediaDataClient::Request::ANY)
	:	mId(id),
		mMatchType(match_type)
	{
	}

	LL_INLINE PredicateMatchRequest(const PredicateMatchRequest& other)
	{
		mRequest = other.mRequest;
		mMatchType = other.mMatchType;
		mId = other.mId;
	}

	LL_INLINE bool operator()(const LLMediaDataClient::Request::ptr_t& test) const
	{
		if (mRequest)
		{
			return mRequest->isMatch(test, mMatchType);
		}
		if (mId.notNull())
		{
			return mId == test->getID() &&
				   (mMatchType == LLMediaDataClient::Request::ANY ||
					mMatchType == test->getType());
		}
		return false;
	}

private:
	LLMediaDataClient::Request::ptr_t	mRequest;
	LLMediaDataClient::Request::Type	mMatchType;
	LLUUID								mId;
};

template <typename T>
static void mark_dead_and_remove_if(T& c, const PredicateMatchRequest& match)
{
	for (typename T::iterator it = c.begin(); it != c.end(); )
	{
		if (match(*it))
		{
			(*it)->markDead();
			it = c.erase(it);
		}
		else
		{
			++it;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLMediaDataClient class
///////////////////////////////////////////////////////////////////////////////

LLMediaDataClient::LLMediaDataClient(F32 queue_timer_delay,
									 F32 retry_timer_delay,
									 U32 max_retries,
									 U32 max_sorted_queue_size,
									 U32 max_round_robin_queue_size)
:	mQueueTimerDelay(queue_timer_delay),
	mRetryTimerDelay(retry_timer_delay),
	mMaxNumRetries(max_retries),
	mMaxSortedQueueSize(max_sorted_queue_size),
	mMaxRoundRobinQueueSize(max_round_robin_queue_size),
	mQueueTimerIsRunning(false),
	mHttpRequest(new LLCore::HttpRequest()),
	mHttpHeaders(new LLCore::HttpHeaders()),
	mHttpOpts(new LLCore::HttpOptions()),
	mHttpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID)
{
}

//virtual
LLMediaDataClient::~LLMediaDataClient()
{
	stopQueueTimer();
}

bool LLMediaDataClient::isEmpty() const
{
	return mQueue.empty();
}

bool LLMediaDataClient::isInQueue(const LLMediaDataClientObject::ptr_t& object)
{
	PredicateMatchRequest upred(object->getID());
	return std::find_if(mQueue.begin(), mQueue.end(), upred) != mQueue.end() ||
		   std::find_if(mUnQueuedRequests.begin(), mUnQueuedRequests.end(),
						upred) != mUnQueuedRequests.end();
}

void LLMediaDataClient::removeFromQueue(const LLMediaDataClientObject::ptr_t& object)
{
	LL_DEBUGS("MediaDataClient") << "removing requests matching ID "
								 << object->getID() << LL_ENDL;
	PredicateMatchRequest upred(object->getID());
	mark_dead_and_remove_if(mQueue, upred);
	mark_dead_and_remove_if(mUnQueuedRequests, upred);
}

void LLMediaDataClient::startQueueTimer()
{
	if (!mQueueTimerIsRunning)
	{
		LL_DEBUGS("MediaDataClient") << "starting queue timer (delay="
									 << mQueueTimerDelay << " seconds)"
									 << LL_ENDL;
		// LLEventTimer automagically takes care of the lifetime of this object
		new QueueTimer(mQueueTimerDelay, this);
	}
	else
	{
		LL_DEBUGS("MediaDataClient") << "Queue timer is already running"
									 << LL_ENDL;
	}
}

bool LLMediaDataClient::processQueueTimer()
{
	if (isDoneProcessing())
	{
		return true;
	}

	LL_DEBUGS("MediaDataClient") << "QueueTimer::tick() started, queue size is: "
								 << mQueue.size() << LL_ENDL;
	serviceQueue();
	serviceHttp();
	LL_DEBUGS("MediaDataClient") << "QueueTimer::tick() finished, queue size is: "
								 << mQueue.size() << LL_ENDL;

	return isDoneProcessing();
}

LLMediaDataClient::Request::ptr_t LLMediaDataClient::dequeue()
{
	Request::ptr_t request;
	request_queue_t* queue_p = getQueue();

	if (queue_p->empty())
	{
		LL_DEBUGS("MediaDataClient") << "Queue empty: " << *queue_p
									 << LL_ENDL;
	}
	else
	{
		request = queue_p->front();

		if (canServiceRequest(request))
		{
			// We will be returning this request, so remove it from the queue.
			queue_p->pop_front();
		}
		else
		{
			// Do not return this request: it is not ready to be serviced.
			request.reset();
		}
	}

	return request;
}

void LLMediaDataClient::pushBack(Request::ptr_t request)
{
	request_queue_t* queue_p = getQueue();
	queue_p->push_front(request);
}

void LLMediaDataClient::trackRequest(Request::ptr_t request)
{
	request_set_t::iterator iter = mUnQueuedRequests.find(request);

	if (iter != mUnQueuedRequests.end())
	{
		llwarns << "Tracking already tracked request: " << *request << llendl;
	}
	else
	{
		mUnQueuedRequests.insert(request);
	}
}

void LLMediaDataClient::stopTrackingRequest(Request::ptr_t request)
{
	request_set_t::iterator iter = mUnQueuedRequests.find(request);

	if (iter != mUnQueuedRequests.end())
	{
		mUnQueuedRequests.erase(iter);
	}
	else
	{
		llwarns << "Removing an untracked request: " << *request << llendl;
	}
}

// Peels one off of the items from the queue and executes it
void LLMediaDataClient::serviceQueue()
{
	Request::ptr_t request;
	bool dead_request;
	do
	{
		request = dequeue();
		if (!request)
		{
			// Queue is empty.
			return;
		}

		dead_request = request->isDead();
		if (dead_request)
		{
			llinfos << "Skipping dead request " << *request << llendl;
		}
	}
	while (dead_request);

	// Try to send the HTTP message to the cap url
	const std::string& url = request->getCapability();
	if (!url.empty())
	{
		const LLSD& sd_payload = request->getPayload();
		llinfos << "Sending request for " << *request << llendl;

		// Add this request to the non-queued tracking list
		trackRequest(request);

		// And make the post
		LLCore::HttpHandler::ptr_t handler = request->createHandler();
		LLCore::HttpHandle handle =
			LLCoreHttpUtil::requestPostWithLLSD(mHttpRequest, mHttpPolicy, url,
												sd_payload, mHttpOpts,
												mHttpHeaders, handler);
		if (handle == LLCORE_HTTP_HANDLE_INVALID)
		{
			LLCore::HttpStatus status = mHttpRequest->getStatus();
			llwarns << "Failed POST request to: " << url << " - Reason: "
					<< status.toString() << llendl;
		}
	}
	// Cap url does not exist.
	else if (request->getRetryCount() < mMaxNumRetries)
	{
		llwarns << "Could not send request " << *request
				<< " (empty cap url), will retry." << llendl;
		// Put this request back at the head of its queue, and retry next time
		// the queue timer fires.
		request->incRetryCount();
		pushBack(request);
	}
	else
	{
		// This request has exceeded its maximum retry count. It will be
		// dropped.
		llwarns << "Could not send request " << *request << " for "
				<< mMaxNumRetries << " tries, dropping request." << llendl;
	}
}

// Dumps the queue
std::ostream& operator<<(std::ostream& s,
						 const LLMediaDataClient::request_queue_t& q)
{
	S32 i = 0;
	LLMediaDataClient::request_queue_t::const_iterator iter = q.begin();
	LLMediaDataClient::request_queue_t::const_iterator end = q.end();
	while (iter != end)
	{
		s << "\t" << i++ << "]: " << (*iter)->getID().asString() << "("
		  << (*iter++)->getObject()->getMediaInterest() << ")";
	}
	return s;
}

///////////////////////////////////////////////////////////////////////////////
// LLMediaDataClient::QueueTimer sub-class
// Queue of LLMediaDataClientObject smart pointers to request media for.
///////////////////////////////////////////////////////////////////////////////

LLMediaDataClient::QueueTimer::QueueTimer(F32 time, LLMediaDataClient* mdc)
:	LLEventTimer(time),
	mMDC(mdc)
{
	mMDC->setIsRunning(true);
}

//virtual
bool LLMediaDataClient::QueueTimer::tick()
{
	bool result = true;

	if (mMDC.notNull())
	{
		result = mMDC->processQueueTimer();
		if (result)
		{
			// This timer will not fire again.
			mMDC->setIsRunning(false);
			mMDC = NULL;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
// LLMediaDataClient::RetryTimer::RetryTimer sub-class
///////////////////////////////////////////////////////////////////////////////

LLMediaDataClient::RetryTimer::RetryTimer(F32 time, Request::ptr_t request)
:	LLEventTimer(time),
	mRequest(request)
{
	mRequest->startTracking();
}

//virtual
bool LLMediaDataClient::RetryTimer::tick()
{
	mRequest->stopTracking();

	if (mRequest->isDead())
	{
		llinfos << "RetryTimer fired for dead request: " << *mRequest
				<< ", aborting." << llendl;
	}
	else
	{
		llinfos << "RetryTimer fired for: " << *mRequest << ", retrying."
				<< llendl;
		mRequest->reEnqueue();
	}

	// Release the ref to the request.
	mRequest.reset();

	// Do not fire again
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// LLMediaDataClient::Request sub-class
///////////////////////////////////////////////////////////////////////////////

//static
U32 LLMediaDataClient::Request::sNum = 0;

LLMediaDataClient::Request::Request(Type in_type, LLMediaDataClientObject* obj,
									LLMediaDataClient* mdc, S32 face)
:	mType(in_type),
	mObject(obj),
	mNum(++sNum),
	mRetryCount(0),
	mMDC(mdc),
	mScore(0.0),
	mFace(face)
{
	mObjectID = mObject->getID();
}

const char* LLMediaDataClient::Request::getCapName() const
{
	return mMDC ? mMDC->getCapabilityName() : "";
}

const std::string& LLMediaDataClient::Request::getCapability() const
{
	return mMDC ? getObject()->getCapabilityUrl(getCapName())
				: LLStringUtil::null;
}

const char* LLMediaDataClient::Request::getTypeAsString() const
{
	Type t = getType();
	switch (t)
	{
		case GET:
			return "GET";

		case UPDATE:
			return "UPDATE";

		case NAVIGATE:
			return "NAVIGATE";

		case ANY:
			return "ANY";
	}
	return "";
}

void LLMediaDataClient::Request::reEnqueue()
{
	if (mMDC)
	{
		mMDC->enqueue(shared_from_this());
	}
}

F32 LLMediaDataClient::Request::getRetryTimerDelay() const
{
	return mMDC ? mMDC->mRetryTimerDelay : 0.f;
}

U32 LLMediaDataClient::Request::getMaxNumRetries() const
{
	return mMDC ? mMDC->mMaxNumRetries : 0U;
}

void LLMediaDataClient::Request::updateScore()
{
	F64 tmp = mObject->getMediaInterest();
	if (tmp != mScore)
	{
		LL_DEBUGS("MediaDataClient") << "Score for " << mObject->getID()
									 << " changed from " << mScore << " to "
									 << tmp << LL_ENDL;
		mScore = tmp;
	}
}

void LLMediaDataClient::Request::markDead()
{
	mMDC = NULL;
}

bool LLMediaDataClient::Request::isDead()
{
	return !mMDC || mObject->isDead();
}

void LLMediaDataClient::Request::startTracking()
{
	if (mMDC)
	{
		mMDC->trackRequest(shared_from_this());
	}
}

void LLMediaDataClient::Request::stopTracking()
{
	if (mMDC)
	{
		mMDC->stopTrackingRequest(shared_from_this());
	}
}

std::ostream& operator<<(std::ostream& s, const LLMediaDataClient::Request& r)
{
	s << "request: num=" << r.getNum() << " type=" << r.getTypeAsString()
	  << " ID=" << r.getID() << " face=" << r.getFace() << " #retries="
	  << r.getRetryCount();
	return s;
}

///////////////////////////////////////////////////////////////////////////////
// LLMediaDataClient::Handler
///////////////////////////////////////////////////////////////////////////////

LLMediaDataClient::Handler::Handler(const Request::ptr_t& request)
:	mRequest(request)
{
}

//virtual
void LLMediaDataClient::Handler::onSuccess(LLCore::HttpResponse* response,
										   const LLSD& content)
{
	mRequest->stopTracking();

	if (mRequest->isDead())
	{
		llwarns << "dead request " << *mRequest << llendl;
	}
	else
	{
		LL_DEBUGS("MediaDataClient") << *mRequest << " - Result: " << content
									 << LL_ENDL;
	}
}

//virtual
void LLMediaDataClient::Handler::onFailure(LLCore::HttpResponse* response,
										   LLCore::HttpStatus status)
{
	mRequest->stopTracking();

	if (status == gStatusUnavailable)
	{
		F32 retry_timeout = mRequest->getRetryTimerDelay();

		mRequest->incRetryCount();

		if (mRequest->getRetryCount() < mRequest->getMaxNumRetries())
		{
			llinfos << *mRequest << " got SERVICE_UNAVAILABLE... Retrying in "
					<< retry_timeout << " seconds" << llendl;

			// Start timer (instances are automagically tracked by
			// InstanceTracker<> and LLEventTimer)
			new RetryTimer(F32(retry_timeout), mRequest);
		}
		else
		{
			llinfos << *mRequest << " got SERVICE_UNAVAILABLE... Retry count "
					<< mRequest->getRetryCount() << " exceeds "
					<< mRequest->getMaxNumRetries() << ", not retrying"
					<< llendl;
		}
	}
	else
	{
		llwarns << *mRequest << " - HTTP error: " << status.toString()
				<< llendl;
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLObjectMediaDataClient sub-class
// Sub-class of LLMediaDataClient for the ObjectMedia cap
///////////////////////////////////////////////////////////////////////////////

void LLObjectMediaDataClient::fetchMedia(LLMediaDataClientObject* object)
{
	// Create a get request and put it in the queue.
	enqueue(Request::ptr_t(new RequestGet(object, this)));
}

const char* LLObjectMediaDataClient::getCapabilityName() const
{
	return "ObjectMedia";
}

LLObjectMediaDataClient::request_queue_t* LLObjectMediaDataClient::getQueue()
{
	return mCurrentQueueIsTheSortedQueue ? &mQueue : &mRoundRobinQueue;
}

void LLObjectMediaDataClient::sortQueue()
{
	if (!mQueue.empty())
	{
		// Score all elements in the sorted queue.
		for (request_queue_t::iterator iter = mQueue.begin(),
									   end = mQueue.end();
			 iter != end; ++iter)
		{
			(*iter)->updateScore();
		}

		// Re-sort the list...
		mQueue.sort(compareRequestScores);

		// ...then cull items over the max
		U32 size = mQueue.size();
		if (size > mMaxSortedQueueSize)
		{
			U32 num_to_cull = size - mMaxSortedQueueSize;
			llwarns_once << "Sorted queue maxed out, culling " << num_to_cull
						 << " items" << LL_ENDL;
			while (num_to_cull-- > 0)
			{
				mQueue.back()->markDead();
				mQueue.pop_back();
			}
		}
	}
}

//static
bool LLObjectMediaDataClient::compareRequestScores(const Request::ptr_t& o1,
												   const Request::ptr_t& o2)
{
	if (!o2)
	{
		return true;
	}
	if (!o1)
	{
		return false;
	}
	return o1->getScore() > o2->getScore();
}

void LLObjectMediaDataClient::enqueue(Request::ptr_t request)
{
	if (!request) return;

	static LLCachedControl<bool> media_enabled(gSavedSettings,
											   "EnableStreamingMedia");
	if (!media_enabled)
	{
		LL_DEBUGS("MediaDataClient") << "Media disabled: ignoring request "
									 << *request << LL_ENDL;
		return;
	}

	if (request->isDead())
	{
		LL_DEBUGS("MediaDataClient") << "Not queuing dead request "
									 << *request << LL_ENDL;
		return;
	}

	// Invariants: new requests always go into the sorted queue.
	bool is_new = request->isNew();

	if (!is_new && request->getType() == Request::GET)
	{
		// For GET requests that are not new, if a matching request is already
		// in the round robin queue, in flight, or being retried, leave it at
		// its current position.
		PredicateMatchRequest upred(request->getID(), Request::GET);
		request_queue_t::iterator iter = std::find_if(mRoundRobinQueue.begin(),
													  mRoundRobinQueue.end(),
													  upred);
		request_set_t::iterator iter2 = std::find_if(mUnQueuedRequests.begin(),
													 mUnQueuedRequests.end(),
													 upred);

		if (iter != mRoundRobinQueue.end() || iter2 != mUnQueuedRequests.end())
		{
			LL_DEBUGS("MediaDataClient") << "ALREADY THERE: NOT Queuing request for "
										 << *request << LL_ENDL;
			return;
		}
	}

	// *TODO: should an UPDATE cause pending GET requests for the same object
	// to be removed from the queue ?  If the update will cause an object
	// update message to be sent out at some point in the future, then yes.

	// Remove any existing requests of this type for this object
	PredicateMatchRequest upred(request->getID(), request->getType());
	mark_dead_and_remove_if(mQueue, upred);
	mark_dead_and_remove_if(mRoundRobinQueue, upred);
	mark_dead_and_remove_if(mUnQueuedRequests, upred);

	if (is_new)
	{
		LL_DEBUGS("MediaDataClient") << "Queuing SORTED request for "
									 << *request << LL_ENDL;

		mQueue.push_back(request);
	}
	else
	{
		if (mRoundRobinQueue.size() > mMaxRoundRobinQueueSize)
		{
			llwarns_sparse << "Round Robin queue maxed out !" << llendl;
			//LL_DEBUGS("MediaDataClient") << "Not queuing " << *request << LL_ENDL;
			return;
		}

		LL_DEBUGS("MediaDataClient") << "Queuing RR request for " << *request
									 << LL_ENDL;
		// Push the request on the pending queue
		mRoundRobinQueue.push_back(request);
	}

	// Start the timer if not already running
	startQueueTimer();
}

bool LLObjectMediaDataClient::canServiceRequest(Request::ptr_t request)
{
	if (mCurrentQueueIsTheSortedQueue &&
		!request->getObject()->isInterestingEnough())
	{
		LL_DEBUGS("MediaDataClient") << "Not fetching " << *request
									 << ": not interesting enough."
									 << LL_ENDL;
		return false;
	}
	return true;
}

void LLObjectMediaDataClient::swapCurrentQueue()
{
	// Swap
	mCurrentQueueIsTheSortedQueue = !mCurrentQueueIsTheSortedQueue;
	// If it is empty, swap back
	if (getQueue()->empty())
	{
		mCurrentQueueIsTheSortedQueue = !mCurrentQueueIsTheSortedQueue;
	}
}

bool LLObjectMediaDataClient::isEmpty() const
{
	return mQueue.empty() && mRoundRobinQueue.empty();
}

bool LLObjectMediaDataClient::isInQueue(const LLMediaDataClientObject::ptr_t& object)
{
	// First, call parent impl.
	if (LLMediaDataClient::isInQueue(object))
	{
		return true;
	}

	return std::find_if(mRoundRobinQueue.begin(), mRoundRobinQueue.end(),
						PredicateMatchRequest(object->getID())) !=
							mRoundRobinQueue.end();
}

void LLObjectMediaDataClient::removeFromQueue(const LLMediaDataClientObject::ptr_t& object)
{
	// First, call parent impl.
	LLMediaDataClient::removeFromQueue(object);
	mark_dead_and_remove_if(mRoundRobinQueue,
							PredicateMatchRequest(object->getID()));
}

bool LLObjectMediaDataClient::processQueueTimer()
{
	if (isDoneProcessing())
	{
		return true;
	}

	LL_DEBUGS("MediaDataClient") << "Started, SORTED queue size is: "
								 << mQueue.size() << ", RR queue size is: "
								 << mRoundRobinQueue.size() << LL_ENDL;
#if 0
	purgeDeadRequests();
#endif
	sortQueue();
	serviceQueue();
	serviceHttp();
	swapCurrentQueue();

	LL_DEBUGS("MediaDataClient") << "finished, SORTED queue size is: "
								 << mQueue.size() << ", RR queue size is: "
								 << mRoundRobinQueue.size() << LL_ENDL;

	return isDoneProcessing();
}

LLObjectMediaDataClient::RequestGet::RequestGet(LLMediaDataClientObject* obj,
												LLMediaDataClient* mdc)
:	LLMediaDataClient::Request(LLMediaDataClient::Request::GET, obj, mdc)
{
}

LLSD LLObjectMediaDataClient::RequestGet::getPayload() const
{
	LLSD result;
	result["verb"] = "GET";
	result[LLTextureEntry::OBJECT_ID_KEY] = mObject->getID();
	return result;
}

LLCore::HttpHandler::ptr_t LLObjectMediaDataClient::RequestGet::createHandler()
{
	return LLCore::HttpHandler::ptr_t(new LLObjectMediaDataClient::Handler(shared_from_this()));
}

void LLObjectMediaDataClient::updateMedia(LLMediaDataClientObject* object)
{
	// Create an update request and put it in the queue.
	enqueue(Request::ptr_t(new RequestUpdate(object, this)));
}

LLObjectMediaDataClient::RequestUpdate::RequestUpdate(LLMediaDataClientObject* obj,
													  LLMediaDataClient* mdc)
:	LLMediaDataClient::Request(LLMediaDataClient::Request::UPDATE, obj, mdc)
{
}

LLSD LLObjectMediaDataClient::RequestUpdate::getPayload() const
{
	LLSD result;
	result["verb"] = "UPDATE";
	result[LLTextureEntry::OBJECT_ID_KEY] = mObject->getID();

	LLSD object_media_data;
	for (S32 i = 0, count = mObject->getMediaDataCount(); i < count ; ++i)
	{
		object_media_data.append(mObject->getMediaDataLLSD(i));
	}

	result[LLTextureEntry::OBJECT_MEDIA_DATA_KEY] = object_media_data;

	return result;
}

LLCore::HttpHandler::ptr_t LLObjectMediaDataClient::RequestUpdate::createHandler()
{
	// This just uses the base class' handler.
	return LLCore::HttpHandler::ptr_t(new LLMediaDataClient::Handler(shared_from_this()));
}

//virtual
void LLObjectMediaDataClient::Handler::onSuccess(LLCore::HttpResponse* response,
												 const LLSD& content)
{
	LLMediaDataClient::Handler::onSuccess(response, content);

	if (getRequest()->isDead())
	{
		// warning emitted from base method.
		return;
	}

	if (!content.isMap())
	{
		onFailure(response,
				  LLCore::HttpStatus(HTTP_INTERNAL_ERROR,
									 "Malformed response contents"));
		return;
	}

	// This handler is only used for GET requests, not UPDATE.
	LL_DEBUGS("MediaDataClient") << *(getRequest()) << " GET returned: "
								 << content << LL_ENDL;

	// Look for an error
	if (content.has("error"))
	{
		const LLSD& error = content["error"];
		llwarns << *(getRequest())
				<< " Error getting media data for object: code = "
				<< error["code"].asString() << ": "
				<< error["message"].asString() << llendl;
		// *TODO: Warn user ?
		return;
	}

	// Check the data
	const LLUUID& object_id = content[LLTextureEntry::OBJECT_ID_KEY];
	if (object_id != getRequest()->getObject()->getID())
	{
		// NOT good, wrong object id !
		llwarns << *(getRequest()) << " DROPPING response with wrong object id ("
				<< object_id << ")" << llendl;
		return;
	}

	// Otherwise, update with object media data
	getRequest()->getObject()->updateObjectMediaData(content[LLTextureEntry::OBJECT_MEDIA_DATA_KEY],
													 content[LLTextureEntry::MEDIA_VERSION_KEY]);
}

///////////////////////////////////////////////////////////////////////////////
// LLObjectMediaNavigateClient class
// Subclass of LLMediaDataClient for the ObjectMediaNavigate cap
///////////////////////////////////////////////////////////////////////////////

const char* LLObjectMediaNavigateClient::getCapabilityName() const
{
	return "ObjectMediaNavigate";
}

void LLObjectMediaNavigateClient::enqueue(Request::ptr_t request)
{
	if (!request) return;

	static LLCachedControl<bool> media_enabled(gSavedSettings,
											   "EnableStreamingMedia");
	if (!media_enabled)
	{
		LL_DEBUGS("MediaDataClient") << "Media disabled: ignoring request "
									 << *request << LL_ENDL;
		return;
	}

	if (request->isDead())
	{
		LL_DEBUGS("MediaDataClient") << "Not queuing dead request "
									 << *request << LL_ENDL;
		return;
	}

	PredicateMatchRequest upred(request);

	// If there is already a matching request in the queue, remove it.
	request_queue_t::iterator iter = std::find_if(mQueue.begin(), mQueue.end(),
												  upred);
	if (iter != mQueue.end())
	{
		LL_DEBUGS("MediaDataClient") << "Removing matching queued request "
									 << **iter << LL_ENDL;
		mQueue.erase(iter);
	}
	else
	{
		request_set_t::iterator set_iter = std::find_if(mUnQueuedRequests.begin(),
														mUnQueuedRequests.end(),
														upred);
		if (set_iter != mUnQueuedRequests.end())
		{
			LL_DEBUGS("MediaDataClient") << "Removing matching unqueued request "
										 << **set_iter << LL_ENDL;
			mUnQueuedRequests.erase(set_iter);
		}
	}

#if 0
	// Sadly, this does not work. It ends up creating a race condition when the
	// user navigates and then hits the "back" button where the navigate-back
	// appears to be spurious and does not get broadcast.
	if (request->getObject()->isCurrentMediaUrl(request->getFace(),
												request->getURL()))
	{
		// This navigate request is trying to send the face to the current URL.
		// Drop it.
		LL_DEBUGS("MediaDataClient") << "Dropping spurious request "
									 << *request << LL_ENDL;
	}
	else
#endif
	{
		LL_DEBUGS("MediaDataClient") << "queuing new request " << (*request)
									 << LL_ENDL;
		mQueue.push_back(request);

		// Start the timer if not already running
		startQueueTimer();
	}
}

void LLObjectMediaNavigateClient::navigate(LLMediaDataClientObject* object,
										   U8 texture_index,
										   const std::string& url)
{
	// Create a get request and put it in the queue.
	enqueue(Request::ptr_t(new RequestNavigate(object, this, texture_index,
											   url)));
}

LLObjectMediaNavigateClient::RequestNavigate::RequestNavigate(LLMediaDataClientObject* obj,
															  LLMediaDataClient* mdc,
															  U8 texture_index,
															  const std::string& url)
:	LLMediaDataClient::Request(LLMediaDataClient::Request::NAVIGATE, obj, mdc,
							  (S32)texture_index),
	mURL(url)
{
}

LLSD LLObjectMediaNavigateClient::RequestNavigate::getPayload() const
{
	LLSD result;
	result[LLTextureEntry::OBJECT_ID_KEY] = getID();
	result[LLMediaEntry::CURRENT_URL_KEY] = mURL;
	result[LLTextureEntry::TEXTURE_INDEX_KEY] = (LLSD::Integer)getFace();

	return result;
}

LLCore::HttpHandler::ptr_t LLObjectMediaNavigateClient::RequestNavigate::createHandler()
{
	return LLCore::HttpHandler::ptr_t(new LLObjectMediaNavigateClient::Handler(shared_from_this()));
}

//virtual
void LLObjectMediaNavigateClient::Handler::onSuccess(LLCore::HttpResponse* response,
													 const LLSD& content)
{
	LLMediaDataClient::Handler::onSuccess(response, content);

	if (getRequest()->isDead())
	{
		// Warning emitted from base method.
		return;
	}

	llinfos << *(getRequest()) << " - NAVIGATE returned: " << content
			<< llendl;

	if (content.has("error"))
	{
		const LLSD& error = content["error"];
		S32 error_code = error["code"];

		if (error_code == ERROR_PERMISSION_DENIED_CODE)
		{
			mediaNavigateBounceBack();
		}
		else
		{
			llwarns << *(getRequest()) << " Error navigating: code = "
					<< error["code"].asString() << ": "
					<< error["message"].asString() << llendl;
		}
		// *TODO: Warn user ?
	}
	else
	{
		// No action required.
		LL_DEBUGS("MediaDataClient") << *(getRequest()) << " - " << content
									 << LL_ENDL;
	}
}

//virtual
void LLObjectMediaNavigateClient::Handler::onFailure(LLCore::HttpResponse* response,
													 LLCore::HttpStatus status)
{
	LLMediaDataClient::Handler::onFailure(response, status);

	if (getRequest()->isDead())
	{
		// Warning emitted from base method.
		return;
	}

	if (status != gStatusUnavailable)
	{
		mediaNavigateBounceBack();
	}
}

void LLObjectMediaNavigateClient::Handler::mediaNavigateBounceBack()
{
	llwarns << *(getRequest()) << " Navigation denied: bounce back" << llendl;
	const LLSD& payload = getRequest()->getPayload();
	// Bounce the face back
	getRequest()->getObject()->mediaNavigateBounceBack((LLSD::Integer)payload[LLTextureEntry::TEXTURE_INDEX_KEY]);
}
