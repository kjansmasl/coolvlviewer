/**
 * @file llworkerthread.cpp
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

#include "llworkerthread.h"

#include "llstl.h"

//============================================================================
// Runs on MAIN thread

LLWorkerThread::LLWorkerThread(const std::string& name)
:	LLQueuedThread(name)
{
}

LLWorkerThread::~LLWorkerThread()
{
	// Delete any workers in the delete queue (should be safe, had better be !)
	if (!mDeleteList.empty())
	{
		llwarns << mName << " destroyed with " << mDeleteList.size()
				<< " entries in delete list." << llendl;
	}
	// ~LLQueuedThread() will be called here
}

// Called only in destructor.
void LLWorkerThread::clearDeleteList()
{
	// Delete any workers in the delete queue (should be safe; had better be)
	if (!mDeleteList.empty())
	{
		llwarns <<  mName << " destroyed with " << mDeleteList.size()
				<< " entries in delete list." << llendl;

		mDeleteMutex.lock();
		for (delete_list_t::iterator iter = mDeleteList.begin(),
									 end = mDeleteList.end();
			 iter != end; ++iter)
		{
			LLWorkerClass* worker = *iter;
			if (worker)
			{
				worker->mRequestHandle = LLWorkerThread::nullHandle();
				worker->clearFlags(LLWorkerClass::WCF_HAVE_WORK |
								   LLWorkerClass::WCF_WORKING);
				delete worker;
			}
		}
		mDeleteList.clear();
		mDeleteMutex.unlock();
	}
}

//virtual
size_t LLWorkerThread::update()
{
	size_t res = LLQueuedThread::update();
	// Delete scheduled workers
	std::vector<LLWorkerClass*> delete_list, abort_list;
	mDeleteMutex.lock();
	for (delete_list_t::iterator iter = mDeleteList.begin();
		 iter != mDeleteList.end(); )
	{
		delete_list_t::iterator curiter = iter++;
		LLWorkerClass* worker = *curiter;
		if (worker && worker->deleteOK())
		{
			if (worker->getFlags(LLWorkerClass::WCF_WORK_FINISHED))
			{
				worker->setFlags(LLWorkerClass::WCF_DELETE_REQUESTED);
				delete_list.push_back(worker);
				mDeleteList.erase(curiter);
			}
			else if (!worker->getFlags(LLWorkerClass::WCF_ABORT_REQUESTED))
			{
				abort_list.push_back(worker);
			}
		}
	}
	mDeleteMutex.unlock();

	// Abort and delete after releasing mutex
	for (std::vector<LLWorkerClass*>::iterator iter = abort_list.begin();
		 iter != abort_list.end(); ++iter)
	{
		(*iter)->abortWork(false);
	}

	for (std::vector<LLWorkerClass*>::iterator iter = delete_list.begin();
		 iter != delete_list.end(); ++iter)
	{
		LLWorkerClass* worker = *iter;
		if (worker && worker->mRequestHandle)
		{
			// Finished but not completed
			completeRequest(worker->mRequestHandle);
			worker->mRequestHandle = LLWorkerThread::nullHandle();
			worker->clearFlags(LLWorkerClass::WCF_HAVE_WORK |
							   LLWorkerClass::WCF_WORKING);
		}
		delete *iter;
	}

	// Deleted and aborted entries mean there is still work to do
	return res + delete_list.size() + abort_list.size();
}

LLWorkerThread::handle_t LLWorkerThread::addWorkRequest(LLWorkerClass* workerclass,
														S32 param,
														U32 priority)
{
	handle_t handle = generateHandle();
	WorkRequest* req = new WorkRequest(handle, priority, workerclass, param);
	if (req)
	{
		if (!addRequest(req))
		{
			llwarns << "Failed to add request for " << mName << llendl;
			llassert(false);
			req->deleteRequest();
			handle = nullHandle();
		}
	}
	else
	{
		llwarns << "Failed to create a new request for " << mName << llendl;
		llassert(false);
		handle = nullHandle();
	}

	return handle;
}

void LLWorkerThread::deleteWorker(LLWorkerClass* workerclass)
{
	mDeleteMutex.lock();
	mDeleteList.push_back(workerclass);
	mDeleteMutex.unlock();
}

//============================================================================
// Runs on its OWN thread

LLWorkerThread::WorkRequest::WorkRequest(handle_t handle, U32 priority,
										 LLWorkerClass* workerclass, S32 param)
:	LLQueuedThread::QueuedRequest(handle, priority),
	mWorkerClass(workerclass),
	mParam(param)
{
}

//virtual (required for access by LLWorkerThread)
void LLWorkerThread::WorkRequest::deleteRequest()
{
	LLQueuedThread::QueuedRequest::deleteRequest();
}

//virtual
bool LLWorkerThread::WorkRequest::processRequest()
{
	LLWorkerClass* workerclass = getWorkerClass();
	if (workerclass)
	{
		workerclass->setWorking(true);
		bool complete = workerclass->doWork(getParam());
		workerclass->setWorking(false);
		return complete;
	}

	llwarns << "NULL worker class !" << llendl;
	llassert(false);
	return true;
}

//virtual
void LLWorkerThread::WorkRequest::finishRequest(bool completed)
{
	LLWorkerClass* workerclass = getWorkerClass();
	if (workerclass)
	{
		workerclass->finishWork(getParam(), completed);
		U32 flags = LLWorkerClass::WCF_WORK_FINISHED |
					(completed ? 0 : LLWorkerClass::WCF_WORK_ABORTED);
		workerclass->setFlags(flags);
	}
	else
	{
		llwarns << "NULL worker class !" << llendl;
		llassert(false);
	}
}

//============================================================================
// LLWorkerClass: operates in main thread

LLWorkerClass::LLWorkerClass(LLWorkerThread* workerthread,
							 const std::string& name)
:	mWorkerThread(workerthread),
	mWorkerClassName(name),
	mRequestHandle(LLWorkerThread::nullHandle()),
	mRequestPriority(LLWorkerThread::PRIORITY_NORMAL),
	mWorkFlags(0)
{
	if (!mWorkerThread)
	{
		llerrs << "LLWorkerClass " << name
			   << " created with NULL worker thread !" << llendl;
	}
}

LLWorkerClass::~LLWorkerClass()
{
	llassert_always(!(mWorkFlags & WCF_WORKING));
	llassert_always(mWorkFlags & WCF_DELETE_REQUESTED);
	llassert_always(!mMutex.isLocked());
	if (mRequestHandle != LLWorkerThread::nullHandle())
	{
		LLWorkerThread::WorkRequest* workreq =
			(LLWorkerThread::WorkRequest*)mWorkerThread->getRequest(mRequestHandle);
		if (!workreq)
		{
			llwarns << mWorkerClassName << " destroyed with stale work handle"
					<< llendl;
			llassert(false);
		}
		else if (workreq->getStatus() != LLWorkerThread::STATUS_ABORTED &&
				 workreq->getStatus() != LLWorkerThread::STATUS_COMPLETE)
		{
			llwarns << mWorkerClassName
					<< " destroyed with active worker !  Worker Status: "
					<< workreq->getStatus() << llendl;
			llassert(false);
		}
	}
}

// Called from worker thread
void LLWorkerClass::setWorking(bool working)
{
	mMutex.lock();
	if (working)
	{
		llassert_always(!(mWorkFlags & WCF_WORKING));
		setFlags(WCF_WORKING);
	}
	else
	{
		llassert_always((mWorkFlags & WCF_WORKING));
		clearFlags(WCF_WORKING);
	}
	mMutex.unlock();
}

bool LLWorkerClass::yield()
{
	LLThread::yield();
	mWorkerThread->checkPause();
	bool res;
	mMutex.lock();
	res = (getFlags() & WCF_ABORT_REQUESTED) != 0;
	mMutex.unlock();
	return res;
}

// Calls startWork, adds doWork() to queue
void LLWorkerClass::addWork(S32 param, U32 priority)
{
	mMutex.lock();
	llassert_always(!(mWorkFlags & (WCF_WORKING | WCF_HAVE_WORK)));
	if (mRequestHandle != LLWorkerThread::nullHandle())
	{
		llerrs << "Worker class " << mWorkerClassName
			   << ": attempt to add work with active worker !" << llendl;
	}

	startWork(param);
	clearFlags(WCF_WORK_FINISHED | WCF_WORK_ABORTED);
	setFlags(WCF_HAVE_WORK);
	mRequestHandle = mWorkerThread->addWorkRequest(this, param, priority);
	mMutex.unlock();
}

void LLWorkerClass::abortWork(bool autocomplete)
{
	mMutex.lock();
	if (mRequestHandle != LLWorkerThread::nullHandle())
	{
		mWorkerThread->abortRequest(mRequestHandle, autocomplete);
		mWorkerThread->setPriority(mRequestHandle,
								   LLQueuedThread::PRIORITY_IMMEDIATE);
		setFlags(WCF_ABORT_REQUESTED);
	}
	mMutex.unlock();
}

// If doWork is complete or aborted, call endWork() and return true
bool LLWorkerClass::checkWork(bool aborting)
{
	mMutex.lock();

	bool complete = false;
	bool abort = false;
	if (mRequestHandle != LLWorkerThread::nullHandle())
	{
		LLWorkerThread::WorkRequest* workreq =
			(LLWorkerThread::WorkRequest*)mWorkerThread->getRequest(mRequestHandle);
		if (!workreq)
		{
			if (mWorkerThread->isQuitting() || mWorkerThread->isStopped())
			{
				// The mWorkerThread is not running
				mRequestHandle = LLWorkerThread::nullHandle();
				clearFlags(WCF_HAVE_WORK | WCF_WORKING);
			}
			else
			{
				llassert_always(workreq);
			}
			mMutex.unlock();
			return true;
		}

		LLQueuedThread::status_t status = workreq->getStatus();
		if (status == LLWorkerThread::STATUS_ABORTED)
		{
			complete = true;
			abort = true;
		}
		else if (status == LLWorkerThread::STATUS_COMPLETE)
		{
			complete = true;
		}
		else
		{
			llassert_always(!aborting ||
							(workreq->getFlags() &
							 LLQueuedThread::FLAG_ABORT));
		}
		if (complete)
		{
			llassert_always(!getFlags(WCF_WORKING));
			endWork(workreq->getParam(), abort);
			mWorkerThread->completeRequest(mRequestHandle);
			mRequestHandle = LLWorkerThread::nullHandle();
			clearFlags(WCF_HAVE_WORK);
		}
	}
	else
	{
		complete = true;
	}

	mMutex.unlock();

	return complete;
}

void LLWorkerClass::scheduleDelete()
{
	bool do_delete = false;
	mMutex.lock();
	if (!getFlags(WCF_DELETE_REQUESTED))
	{
		setFlags(WCF_DELETE_REQUESTED);
		do_delete = true;
	}
	mMutex.unlock();
	if (do_delete)
	{
		mWorkerThread->deleteWorker(this);
	}
}

void LLWorkerClass::setPriority(U32 priority)
{
	mMutex.lock();
	if (mRequestHandle != LLWorkerThread::nullHandle() &&
		mRequestPriority != priority)
	{
		mRequestPriority = priority;
		mWorkerThread->setPriority(mRequestHandle, priority);
	}
	mMutex.unlock();
}
