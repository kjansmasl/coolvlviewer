/**
 * @file lleventpoll.cpp
 * @brief Implementation of the LLEventPoll class.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2018, Linden Research, Inc.
 * Copyright (c) 2019-2023, Henri Beauchamp.
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

#include "lleventpoll.h"

#include "llcorehttputil.h"
#include "hbfastmap.h"
#include "llhost.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llgridmanager.h"		// For gIsInSecondLife
#include "llstatusbar.h"
#include "llviewercontrol.h"

// This struture is used to store even poll replies until we can safely process
// them in the main coroutine of the main thread. HB

struct LLEventPollReplies
{
	LL_INLINE LLEventPollReplies(const std::string& poll_name,
								 const std::string& msg_name,
								 const LLSD& message)
	:	mPollName(poll_name),
		mMessageName(msg_name),
		mMessage(message)
	{
	}

	std::string	mPollName;
	std::string	mMessageName;
	LLSD		mMessage;
};

static std::vector<LLEventPollReplies> sReplies;

///////////////////////////////////////////////////////////////////////////////
// LLEventPollImpl class
///////////////////////////////////////////////////////////////////////////////

// We will wait RETRY_SECONDS + (error_count * RETRY_SECONDS_INC) before
// retrying after an error. This means we attempt to recover relatively quickly
// but back off giving more time to recover until we finally give up after
// MAX_EVENT_POLL_HTTP_ERRORS attempts.

// Half of a normal timeout.
constexpr F32 EVENT_POLL_ERROR_RETRY_SECONDS = 15.f;
constexpr F32 EVENT_POLL_ERROR_RETRY_SECONDS_INC = 5.f;
// 5 minutes, by the above rules.
constexpr S32 MAX_EVENT_POLL_HTTP_ERRORS = 10;

class LLEventPollImpl : public LLRefCount
{
protected:
	LOG_CLASS(LLEventPollImpl);

public:
	LLEventPollImpl(U64 handle, const LLHost& sender);

	void start(const std::string& url);
	void stop();

	void setRegionName(const std::string& region_name);

	LL_INLINE bool isPollInFlight() const
	{
		return !mRequestTimer.hasExpired() &&
				// Note: take into account the frame rate, so that we would not
				// end up never being able to TP because the events rate would
				// be as high as our frame rate. HB
			    mRequestTimer.getElapsedTimeF32() >= mMinDelay - gFrameDT;
	}

	LL_INLINE F32 getPollAge() const
	{
		return mRequestTimer.getElapsedTimeF32();
	}

private:
	~LLEventPollImpl();

	void handleMessage(const LLSD& content);

	static void eventPollCoro(std::string url,
							  LLPointer<LLEventPollImpl> impl);

private:
	LLCore::HttpRequest::policy_t					mHttpPolicy;
	LLCore::HttpOptions::ptr_t						mHttpOptions;
	LLCore::HttpHeaders::ptr_t						mHttpHeaders;
	LLCoreHttpUtil::HttpCoroutineAdapter::wptr_t	mAdapter;
	U64												mHandle;
	U32												mPollId;
	U32												mRequestTimeout;
	// This is the delay needed between the launch of a request and the moment
	// it can reliably receive server messages; messages arriving within this
	// delay could potentially get lost. HB
	F32												mMinDelay;
	std::string										mSenderIP;
	std::string										mPollURL;
	std::string										mPollName;
	LLTimer											mRequestTimer;
	bool											mDone;

	static fast_hmap<U64, LLSD>						sLastAck;
	static U32										sNextID;
};

fast_hmap<U64, LLSD> LLEventPollImpl::sLastAck;
U32 LLEventPollImpl::sNextID = 1;

LLEventPollImpl::LLEventPollImpl(U64 handle, const LLHost& sender)
:	mDone(false),
	mMinDelay(LLEventPoll::getMargin()),
	mPollId(sNextID++),
	mHandle(handle),
	mSenderIP(sender.getIPandPort()),
	// NOTE: by using these instead of omitting the corresponding
	// postAndSuspend() parameters, we avoid seeing such classes constructed
	// and destroyed at each loop... HB
	mHttpOptions(new LLCore::HttpOptions),
	mHttpHeaders(new LLCore::HttpHeaders)
{
	LLAppCoreHttp& app_core_http = gAppViewerp->getAppCoreHttp();
	mHttpPolicy = app_core_http.getPolicy(LLAppCoreHttp::AP_LONG_POLL);

	// The region name is unknown when the event poll instance is created: it
	// is filled up later via calls to LLEventPoll::setRegionName() done by
	// LLViewerRegion. HB
	mPollName = llformat("Event poll <%d> - Sender IP: %s - ", mPollId,
						 mSenderIP.c_str());
	llinfos << mPollName << "Initialized." << llendl;
#if LL_WINDOWS
	static const bool under_wine = gAppViewerp->isRunningUnderWine();
	// When running under Wine, touching the retries and timeouts causes HTTP
	// failures (another Wine bug, obviously), so do not do it then... HB
	if (under_wine)
	{
		llwarns_once << "Running under Wine: cannot set event polls retries and timeout."
					 << llendl;
		return;
	}
#endif
	// Do not retry requests at libcurl level: we want to see the requests
	// timing out here, when they do.
	mHttpOptions->setRetries(0);

	// In SL, we prefer to timeout viewer-side (in libcurl) before the server
	// would send us a bogus HTTP error (502 error report HTML page disguised
	// with a 499 or 500 error code in the header) on its own timeout (set to
	// 30s in SL servers). For OpenSim, we let the server time out on us by
	// default (a 502 error will be then received). The user may however decide
	// to change the default timeout via the corresponding debug setting. HB
	static const char* sl = "EventPollTimeoutForSL";
	static const char* os = "EventPollTimeoutForOS";
	mRequestTimeout = llclamp(gSavedSettings.getU32(gIsInSecondLife ? sl : os),
							  15, 180);
	mHttpOptions->setTimeout(mRequestTimeout);
	mHttpOptions->setTransferTimeout(mRequestTimeout);
}

LLEventPollImpl::~LLEventPollImpl()
{
	mHttpOptions.reset();
	mHttpHeaders.reset();
	LL_DEBUGS("EventPoll") << mPollName << "Destroyed." << LL_ENDL;
}

void LLEventPollImpl::setRegionName(const std::string& region_name)
{
	if (mPollName.find(region_name) == std::string::npos)	// Do not spam.
	{
		llinfos	<< mPollName << "Got region name: " << region_name << llendl;
		mPollName = llformat("Event poll <%d> - Region: %s - ", mPollId,
							 region_name.c_str());
	}
}

void LLEventPollImpl::start(const std::string& url)
{
	mPollURL = url;
	if (url.empty())
	{
		return;
	}
	llinfos	<< "Starting event poll <" << mPollId << "> - Sender IP: "
			<< mSenderIP << " - URL: " << mPollURL << llendl;
	std::string coroname =
		gCoros.launch("LLEventPollImpl::eventPollCoro",
					  boost::bind(&LLEventPollImpl::eventPollCoro, url, this));
	LL_DEBUGS("EventPoll") << mPollName << "Coroutine name: " << coroname
						   << LL_ENDL;
}

void LLEventPollImpl::stop()
{
	mDone = true;

	LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapterp = mAdapter.lock();
	if (adapterp)
	{
		llinfos << mPollName << "Cancelling..." << llendl;
		// Cancel the yielding operation if any.
		adapterp->cancelSuspendedOperation();
	}
	else
	{
		LL_DEBUGS("EventPoll") << mPollName
							   << "Already stopped, no action taken."
							   << LL_ENDL;
	}
}

void LLEventPollImpl::handleMessage(const LLSD& content)
{
	std::string	msg_name = content["message"];
	LLSD message;
	message["sender"] = mSenderIP;
	if (content.has("body"))
	{
		message["body"] = content["body"];
		LL_DEBUGS("EventPoll") << mPollName << "Queuing message: " << msg_name
							   << LL_ENDL;
	}
	else
	{
		llwarns << mPollName << "Message '" << msg_name << "' without a body."
				<< llendl;
	}
	// Note: coroutines calling handleMessage() all belong to the main thread,
	// so we do not need a mutex before touching sReplies; should this ever
	// change, a mutex lock would be needed here. HB
	sReplies.emplace_back(mPollName, msg_name, message);
}

//static
void LLEventPollImpl::eventPollCoro(std::string url,
									LLPointer<LLEventPollImpl> impl)
{
	// Hold a LLPointer of our impl on the coroutine stack, so to avoid the
	// impl destruction before the exit of the coroutine. HB
	LLPointer<LLEventPollImpl> self = impl;

	LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
		adapter(new LLCoreHttpUtil::HttpCoroutineAdapter("EventPoller",
														 self->mHttpPolicy));
	self->mAdapter = adapter;

	LL_DEBUGS("EventPoll") << self->mPollName << "Entering coroutine."
						   << LL_ENDL;

	// This delay determines a window for TP requests to be sent to the server:
	// we avoid sending one when the current poll request is about to expire,
	// so to avoid a race condition between servers (sim server, Apache server)
	// and viewer, where the TeleportFinish message could get lost during the
	// HTTP requests tear-down and restart. HB
	const F32 expiry = F32(self->mRequestTimeout) - LLEventPoll::getMargin();

	LLSD acknowledge;
	// Get the last "ack" we used in previous LLEventPollImpl instances for
	// this region, if any. HB
	fast_hmap<U64, LLSD>::const_iterator it = sLastAck.find(self->mHandle);
	if (it != sLastAck.end())
	{
		acknowledge = it->second;
	}

	// Continually poll for a server update until we have been terminated
	S32 error_count = 0;
	while (!self->mDone && !gDisconnected)
	{
		LLSD request;
		request["ack"] = acknowledge;
		request["done"] = false;

		LL_DEBUGS("EventPoll") << self->mPollName << "Posting and yielding."
							   << LL_ENDL;
		self->mRequestTimer.reset();
		self->mRequestTimer.setTimerExpirySec(expiry);
		LLSD result = adapter->postAndSuspend(url, request, self->mHttpOptions,
											  self->mHttpHeaders);
		// Note: resetting the timer flags it as "expired", which we want to
		// ensure so that isPollInFlight() returns false at this point. HB
		F32 request_time = self->mRequestTimer.getElapsedTimeAndResetF32();
		// If this request is fastest than our preset "min delay for an
		// established connection", then the latter is obviously too large, and
		// needs to be reduced. HB
		if (request_time < self->mMinDelay)
		{
			self->mMinDelay = request_time;
			LL_DEBUGS("EventPoll") << self->mPollName
								   << "Minimum delay for established connection reduced to: "
								   << request_time << LL_ENDL;
		}

		if (gDisconnected)
		{
			llinfos << self->mPollName
					<< "Viewer disconnected. Dropping stale event message."
					<< llendl;
			break;
		}

		bool is_agent_region = gAgent.getRegionHandle() == self->mHandle;

		LLCore::HttpStatus status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
		if (!status)
		{
			if (status == gStatusTimeout)
			{
				// A standard timeout response: we get this when there are no
				// events.
				LL_DEBUGS("EventPoll") << self->mPollName
									   << "Request timed out viewer-side after: "
									   << request_time << "s." << LL_ENDL;
				error_count = 0;
				continue;
			}

			// Log details when debugging for all other types of errors. HB
			LL_DEBUGS("EventPoll") << self->mPollName
								   << "Error received after: "
								   << request_time << "s."
								   << " - Error " << status.toTerseString()
								   << ": " << status.getMessage();
			const LLSD& http_results =
				result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
			if (http_results.has("error_body"))
			{
				std::string body = http_results["error_body"].asString();
				LL_CONT << " - Returned body:\n" << body;
			}
			LL_CONT << LL_ENDL;

			// When the server times out (because there was no event to
			// report), error 502 is seen on OpenSim grids, and should be seen
			// in SL, but are somehow "mutated" (their header is changed, but
			// not their "502 error" body) into 499 or 500 errors. Treat as
			// timeout and restart. HB
			if (status == gStatusBadGateway ||
				(gIsInSecondLife &&
				 (status == gStatusInternalError ||
				  status == gStatusServerInternalError)))
			{
				LL_DEBUGS("EventPoll") << "Error ignored and treated as server-side timeout."
									   << LL_ENDL;
				error_count = 0;
				continue;
			}
			if (status == gStatusCancelled)
			{
				// Event polling for this server has been cancelled.
				llinfos << self->mPollName << "Cancelled." << llendl;
				break;
			}
			if (status == gStatusNotFound)
			{
				// Do not give up on 404 if this is the agent region !  HB
				if (!is_agent_region)
				{
					// In some cases the server gets ahead of the viewer and
					// will return a 404 error (not found) before the cancel
					// event comes back in the queue.
					llinfos << self->mPollName << "Cancelled on 404."
							<< llendl;
					break;
				}
			}
			else if (!status.isHttpStatus())
			{
				// Some libcurl error (other than gStatusTimeout) or LLCore
				// error (other than gStatusCancelled) was returned. This is
				// unlikely to be recoverable...
				llwarns << self->mPollName
						<< "Critical error returned from libraries. Cancelling coroutine."
						<< llendl;
				break;
			}

			S32 max_retries = MAX_EVENT_POLL_HTTP_ERRORS;
			if (is_agent_region)
			{
				// Increase the number of allowed retries for the agent region:
				// there may be a temporary network issue, and we do not want
				// the viewer to give up too soon on the agent's region, since
				// it would cause a disconnection from the grid (see below). HB
				max_retries *= 2;
				llwarns << self->mPollName
						<< "Agent's region poll request error: "
						<< status.toTerseString() << ": "
						<< status.getMessage() << llendl;
				if (gStatusBarp)
				{
					gStatusBarp->incFailedEventPolls();
				}
			}
			if (error_count < max_retries)
			{
				// An unanticipated error has been received from our poll 
				// request. Calculate a timeout and wait for it to expire
				// (sleep) before trying again. The sleep time is increased by
				// EVENT_POLL_ERROR_RETRY_SECONDS_INC seconds for each
				// consecutive error until MAX_EVENT_POLL_HTTP_ERRORS is
				// reached.
				F32 wait = EVENT_POLL_ERROR_RETRY_SECONDS +
						   error_count * EVENT_POLL_ERROR_RETRY_SECONDS_INC;
				llwarns << self->mPollName << "Retrying in " << wait
						<< " seconds; error count is now " << ++error_count
						<< llendl;

				llcoro::suspendUntilTimeout(wait);

				LL_DEBUGS("EventPoll") << self->mPollName
									   << "About to retry request." << LL_ENDL;
				continue;
			}

			// At this point we have given up and the viewer will not receive
			// HTTP messages from the simulator. IMs, teleports, about land,
			// selecting land, region crossing and more will all fail. They are
			// essentially disconnected from the region even though some things
			// may still work. Since things would not get better until they
			// relog we force a disconnect now.
			if (is_agent_region)
			{
				llwarns << self->mPollName
						<< "Forcing disconnect due to stalled agent region event poll."
						<< llendl;
				gAppViewerp->forceDisconnect(LLTrans::getString("AgentLostConnection"));
			}
			else
			{
				llwarns << self->mPollName
						<< "Stalled region event poll. Giving up." << llendl;
			}
			self->mDone = true;
			break;
		}
		else if (is_agent_region && gStatusBarp)
		{
			gStatusBarp->resetFailedEventPolls();
		}

		error_count = 0;

		if (!result.isMap() || !result.has("events") ||
			!result["events"].isArray() || !result.has("id"))
		{
			llwarns << self->mPollName
					<< "Received reply without event or 'id' key: "
					<< LLSDXMLStreamer(result) << llendl;
			continue;
		}

		acknowledge = result["id"];
		if (acknowledge.isUndefined())
		{
			LL_DEBUGS("EventPoll") << self->mPollName
								   << "Got reply with undefined 'id' key."
								   << LL_ENDL;
			sLastAck.erase(self->mHandle);
		}
		else
		{
			sLastAck.emplace(self->mHandle, acknowledge);
		}

		const LLSD& events = result["events"];
		LL_DEBUGS("EventPoll") << self->mPollName << "Got "
							   << events.size() << " event(s):\n"
							   << LLSDXMLStreamer(acknowledge) << LL_ENDL;
		for (LLSD::array_const_iterator it = events.beginArray(),
										end = events.endArray();
			 it != end; ++it)
		{
			if (it->has("message"))
			{
				self->handleMessage(*it);
			}
		}
	}

	LL_DEBUGS("EventPoll") << self->mPollName << "Leaving coroutine."
						   << LL_ENDL;
}

///////////////////////////////////////////////////////////////////////////////
// LLEventPoll class proper
///////////////////////////////////////////////////////////////////////////////

LLEventPoll::LLEventPoll(U64 handle, const LLHost& sender,
						 const std::string& poll_url)
:	mImpl(new LLEventPollImpl(handle, sender))
{
	mImpl->start(poll_url);
}

LLEventPoll::~LLEventPoll()
{
	mImpl->stop();
	// Note: LLEventPollImpl instance will get deleted on coroutine exit since
	// the coroutine keeps a LLPointer to its instance on its own stack. HB
	mImpl = NULL;
}

void LLEventPoll::setRegionName(const std::string& region_name)
{
	if (mImpl.notNull())
	{
		mImpl->setRegionName(region_name);
	}
}

bool LLEventPoll::isPollInFlight() const
{
	return mImpl.notNull() && mImpl->isPollInFlight();
}

F32 LLEventPoll::getPollAge() const
{
	return mImpl.notNull() ? mImpl->getPollAge() : -1.f;
}

//static
F32 LLEventPoll::getMargin()
{
	static LLCachedControl<U32> margin(gSavedSettings,
									   "EventPollAgeWindowMargin");
	return llclamp((F32)margin, 200.f, 2000.f) * 0.001f;
}

//static
void LLEventPoll::dispatchMessages()
{
	// Note: coroutines calling handleMessage() all belong to the main thread,
	// so we do not need a mutex before touching sReplies; should this ever
	// change, a mutex lock would be needed here. HB
	for (U32 i = 0, count = sReplies.size(); i < count; ++i)
	{
		LLEventPollReplies& reply = sReplies[i];
		LL_DEBUGS("EventPoll") << reply.mPollName << "Processing message: "
							   << reply.mMessageName << LL_ENDL;
		LLMessageSystem::dispatch(reply.mMessageName, reply.mMessage);
	}
	sReplies.clear();
}
