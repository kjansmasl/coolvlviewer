/**
 * @file llqueuedthread.cpp
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llqueuedthread.h"

#include "llstl.h"
#include "lltimer.h"		// For ms_sleep()

///////////////////////////////////////////////////////////////////////////////
// LLQueuedThread class
///////////////////////////////////////////////////////////////////////////////

// Main thread
LLQueuedThread::LLQueuedThread(const std::string& name)
:	LLThread(name),
	mNextHandle(0),
	mIdleThread(true)
{
	// Always set the LLThread to the paused state before starting it, so that
	// the calling thread may fully initialize before actually starting the
	// LLQueuedThread processing, which shall be done either with an explicit
	// call to unpause() or via update(). HB
	pause();
	// Launch the LLThread
	start();
}

// Main thread
LLQueuedThread::~LLQueuedThread()
{
	shutdown();
	// ~LLThread() will be called here
}

// Main thread
void LLQueuedThread::shutdown()
{
	llinfos << "Shutting down: " << mName << llendl;

	setQuitting();
	llinfos << mName << " has been set quitting." << llendl;

	unpause(); // Main thread

	llinfos << "Waiting for "<< mName << " to stop..." << llendl;
	S32 timeout = 1000;
	for ( ; timeout > 0; --timeout)
	{
		if (isStopped())
		{
			break;
		}
		ms_sleep(10);
	}
	if (timeout == 0)
	{
		llwarns << mName << " timed out !" << llendl;
	}
	else
	{
		llinfos << mName << " stopped." << llendl;
	}

	LLMutexTrylock locker(mDataLock, 100);	// Try for 1 second. HB
	if (!locker.isLocked())
	{
		llwarns << "Data lock busy for: " << mName << llendl;
	}
	// Continue nonetheless... The thread is stopped at this point, so...  HB
	S32 active_count = 0;
	request_map_t::iterator it = mRequestMap.begin();
	while (it != mRequestMap.end())
	{
		QueuedRequest* req = it->second;
		S32 status = req->getStatus();
		if (status == STATUS_QUEUED || status == STATUS_INPROGRESS)
		{
			++active_count;
			req->setStatus(STATUS_ABORTED);
		}
		it = mRequestMap.erase(it);
		req->deleteRequest();
	}
	if (active_count)
	{
		llwarns << "Called with " << active_count << " active requests for "
				<< mName << llendl;
	}
}

// Main thread
//virtual
size_t LLQueuedThread::update()
{
	size_t pending = getPending();
	if (pending)
	{
		unpause();
	}
	return pending;
}

// May be called from any thread
//virtual
size_t LLQueuedThread::getPending()
{
	size_t res;
	lockData();
	res = mRequestQueue.size();
	unlockData();
	return res;
}

// Main thread
void LLQueuedThread::waitOnPending()
{
	while (true)
	{
		update();

		if (mIdleThread)
		{
			break;
		}
		yield();
	}
}

// Main thread
void LLQueuedThread::printQueueStats()
{
	lockData();
	if (!mRequestQueue.empty())
	{
		QueuedRequest* req = *mRequestQueue.begin();
		llinfos << llformat("Pending requests:%d Current status:%d",
							mRequestQueue.size(), req->getStatus()) << llendl;
	}
	else
	{
		llinfos << "Queued thread idle" << llendl;
	}
	unlockData();
}

// Main thread
LLQueuedThread::handle_t LLQueuedThread::generateHandle()
{
	lockData();
	handle_t res = ++mNextHandle;
	unlockData();
	return res;
}

// Main thread
bool LLQueuedThread::addRequest(QueuedRequest* req)
{
	if (mStatus == QUITTING)
	{
		return false;
	}

	lockData();
	req->setStatus(STATUS_QUEUED);
	mRequestQueue.insert(req);
	mRequestMap.emplace(req->mHandle, req);
	unlockData();

	// Something has been added to the queue
	if (!isPaused())
	{
		wake(); // Wake the thread up if necessary.
	}

	return true;
}

// Main thread
LLQueuedThread::QueuedRequest* LLQueuedThread::getRequest(handle_t handle)
{
	QueuedRequest* res = NULL;
	lockData();
	request_map_t::iterator it = mRequestMap.find(handle);
	if (it != mRequestMap.end())
	{
		res = it->second;
	}
	unlockData();
	return res;
}

LLQueuedThread::status_t LLQueuedThread::getRequestStatus(handle_t handle)
{
	status_t res = STATUS_EXPIRED;
	lockData();
	request_map_t::iterator it = mRequestMap.find(handle);
	if (it != mRequestMap.end())
	{
		res = it->second->getStatus();
	}
	unlockData();
	return res;
}

void LLQueuedThread::abortRequest(handle_t handle, bool autocomplete)
{
	lockData();
	request_map_t::iterator it = mRequestMap.find(handle);
	if (it != mRequestMap.end())
	{
		it->second->setFlags(FLAG_ABORT |
							 (autocomplete ? FLAG_AUTO_COMPLETE : 0));
	}
	unlockData();
}

// Main thread
void LLQueuedThread::setFlags(handle_t handle, U32 flags)
{
	lockData();
	request_map_t::iterator it = mRequestMap.find(handle);
	if (it != mRequestMap.end())
	{
		it->second->setFlags(flags);
	}
	unlockData();
}

void LLQueuedThread::setPriority(handle_t handle, U32 priority)
{
	lockData();
	request_map_t::iterator it = mRequestMap.find(handle);
	if (it != mRequestMap.end())
	{
		QueuedRequest* req = it->second;
		if (req->getPriority() != priority)
		{
			if (req->getStatus() == STATUS_INPROGRESS)
			{
				// Not in list
				req->setPriority(priority);
			}
			else if (req->getStatus() == STATUS_QUEUED)
			{
				// Remove from list then re-insert
				if (mRequestQueue.erase(req) != 1)
				{
					llwarns << "Request " << mName
							<< " was not in the requests queue !" << llendl;
					llassert(false);
				}
				req->setPriority(priority);
				mRequestQueue.insert(req);
			}
		}
	}
	unlockData();
}

bool LLQueuedThread::completeRequest(handle_t handle)
{
	bool res = false;
	lockData();
	request_map_t::iterator it = mRequestMap.find(handle);
	if (it != mRequestMap.end())
	{
		QueuedRequest* req = it->second;
		S32 status = req->getStatus();
		llassert_always(status != STATUS_QUEUED &&
						status != STATUS_INPROGRESS);
		mRequestMap.erase(it);
		req->deleteRequest();
		res = true;
	}
	unlockData();
	return res;
}

///////////////////////////////////////////////////////////////////////////////
// LLQueuedThread class: runs on its *own* thread
///////////////////////////////////////////////////////////////////////////////

size_t LLQueuedThread::processNextRequest()
{
	QueuedRequest* req;
	// Get next request from pool
	lockData();
	while (true)
	{
		req = NULL;
		if (mRequestQueue.empty())
		{
			break;
		}
		req = *mRequestQueue.begin();
		mRequestQueue.erase(mRequestQueue.begin());
		if (!req) continue;

		if (mStatus == QUITTING || (req->getFlags() & FLAG_ABORT))
		{
			LL_DEBUGS("QueuedThread") << mName << ": aborting request "
									  << std::hex << (uintptr_t)req << std::dec
									  << LL_ENDL;
			req->setStatus(STATUS_ABORTED);
			req->finishRequest(false);
			if (req->getFlags() & FLAG_AUTO_COMPLETE)
			{
				LL_DEBUGS("QueuedThread") << mName
										  << ": deleting auto-complete request "
										  << std::hex << (uintptr_t)req << std::dec
										  << LL_ENDL;
				mRequestMap.erase(req->mHandle);
				req->deleteRequest();
			}
			continue;
		}
		llassert_always(req->getStatus() == STATUS_QUEUED);
		break;
	}
	U32 start_priority = 0;
	if (req)
	{
		LL_DEBUGS("QueuedThread") << mName << ": flagging request "
								  << std::hex << (uintptr_t)req << std::dec
								  << " as being in progress" << LL_ENDL;
		req->setStatus(STATUS_INPROGRESS);
		start_priority = req->getPriority();
	}
	unlockData();

	if (req)
	{
		// Process request
		bool ok = req->processRequest();
		setRequestResult(req, ok);
		if (!ok && start_priority < PRIORITY_NORMAL)
		{
			yield();
		}
	}

	return getPending();
}

void LLQueuedThread::setRequestResult(QueuedRequest* req, bool result)
{
	if (result)
	{
		lockData();
		LL_DEBUGS("QueuedThread") << mName << ": flagging request "
								  << std::hex << (uintptr_t)req << std::dec
								  << " as complete" << LL_ENDL;
		req->setStatus(STATUS_COMPLETE);
		req->finishRequest(true);
		if (req->getFlags() & FLAG_AUTO_COMPLETE)
		{
			LL_DEBUGS("QueuedThread") << mName
									  << ": deleting auto-complete request "
									  << std::hex << (uintptr_t)req << std::dec
									  << LL_ENDL;
			mRequestMap.erase(req->mHandle);
			req->deleteRequest();
		}
		unlockData();
	}
	else
	{
		lockData();
		LL_DEBUGS("QueuedThread") << mName << ": decreasing request "
								  << std::hex << (uintptr_t)req << std::dec
								  << " priority" << LL_ENDL;
		req->setStatus(STATUS_QUEUED);
		mRequestQueue.insert(req);
		unlockData();
	}
}

//virtual
bool LLQueuedThread::runCondition()
{
	// mRunCondition must be locked here
	return !mIdleThread || !mRequestQueue.empty();
}

//virtual
void LLQueuedThread::run()
{
	// Call checkPause() immediately so we do not try to do anything before the
	// class is fully constructed
	checkPause();
	startThread();

	while (!isQuitting())
	{
		mIdleThread = false;
		threadedUpdate();

		if (!processNextRequest())
		{
			mIdleThread = true;
			yield();
		}

		// This will block on the condition until runCondition() returns true,
		// the thread is unpaused, or the thread leaves the RUNNING state.
		checkPause();
	}

	endThread();
	llinfos << "Queued thread " << mName << " exiting." << llendl;
}

///////////////////////////////////////////////////////////////////////////////
// LLQueuedThread::QueuedRequest sub-class
///////////////////////////////////////////////////////////////////////////////

//virtual
LLQueuedThread::QueuedRequest::~QueuedRequest()
{
	llassert_always(mStatus == STATUS_DELETE);
}

//virtual
void LLQueuedThread::QueuedRequest::deleteRequest()
{
	if (mStatus == STATUS_INPROGRESS)
	{
		llwarns << "Deleting a request in progress !" << llendl;
	}
	setStatus(STATUS_DELETE);
	delete this;
}
