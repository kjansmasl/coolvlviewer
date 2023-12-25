/**
 * @file llworkerthread.h
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

#ifndef LL_LLWORKERTHREAD_H
#define LL_LLWORKERTHREAD_H

#include <list>

#include "llqueuedthread.h"

class LLWorkerClass;

// Note: ~LLWorkerThread is O(N) N=# of worker threads, assumed to be small
// It is assumed that LLWorkerThreads are rarely created/destroyed.

class LLWorkerThread : public LLQueuedThread
{
	friend class LLWorkerClass;

protected:
	LOG_CLASS(LLWorkerThread);

public:
	class WorkRequest : public LLQueuedThread::QueuedRequest
	{
	protected:
		LOG_CLASS(LLWorkerThread::WorkRequest);

	protected:
		~WorkRequest() override = default;; // use deleteRequest()

	public:
		WorkRequest(handle_t handle, U32 priority, LLWorkerClass* workerclass,
					S32 param);

		LL_INLINE S32 getParam()					{ return mParam; }
		LL_INLINE LLWorkerClass* getWorkerClass()	{ return mWorkerClass; }

		bool processRequest() override;
		void finishRequest(bool completed) override;
		void deleteRequest() override;

	private:
		LLWorkerClass*	mWorkerClass;
		S32				mParam;
	};

	LLWorkerThread(const std::string& name);
	~LLWorkerThread() override;

	size_t update() override;

	handle_t addWorkRequest(LLWorkerClass* workerclass, S32 param,
							U32 priority = PRIORITY_NORMAL);
	// Debug
	LL_INLINE S32 getNumDeletes()					{ return (S32)mDeleteList.size(); }

protected:
	void clearDeleteList();

private:
	void deleteWorker(LLWorkerClass* workerclass); // schedule for deletion

private:
	typedef std::list<LLWorkerClass*> delete_list_t;
	delete_list_t	mDeleteList;
	LLMutex			mDeleteMutex;
};

// This is a base class which any class with worker functions should derive
// from. Example usage:
//  LLMyWorkerClass* foo = new LLMyWorkerClass();
//  foo->fetchData(); // calls addWork()
//  while (true) // main loop
//  {
//     if (foo->hasData()) // calls checkWork()
//     {
//        foo->processData();
//     }
//  }
//
// WorkerClasses only have one set of work functions. If they need to do
// multiple background tasks, use 'param' to switch amnong them. Only one
// background task can be active at a time (per instance). I.e. don't call
// addWork() if haveWork() returns true.

class LLWorkerClass
{
	friend class LLWorkerThread;
	friend class LLWorkerThread::WorkRequest;

protected:
	LOG_CLASS(LLWorkerClass);

public:
	typedef LLWorkerThread::handle_t handle_t;
	enum FLAGS
	{
		WCF_HAVE_WORK = 0x01,
		WCF_WORKING = 0x02,
		WCF_WORK_FINISHED = 0x10,
		WCF_WORK_ABORTED = 0x20,
		WCF_DELETE_REQUESTED = 0x40,
		WCF_ABORT_REQUESTED = 0x80
	};

public:
	LLWorkerClass(LLWorkerThread* workerthread, const std::string& name);
	virtual ~LLWorkerClass();

	// Called from WorkRequest::processRequest() (WORKER THREAD), returns true
	// if done.
	virtual bool doWork(S32 param) = 0;

	// Called from finishRequest() (WORK THREAD) after completed or aborted
	LL_INLINE virtual void finishWork(S32 param, bool completed)
	{
	}

	// Returns true if safe to delete the worker, called from update() (WORK
	// THREAD), defaults to always OK
	LL_INLINE virtual bool deleteOK()				{ return true; }

	// Schedules deletion once aborted or completed
	void scheduleDelete();

	// haveWork() may still be true if aborted
	LL_INLINE bool haveWork()						{ return getFlags(WCF_HAVE_WORK); }
	LL_INLINE bool isWorking()						{ return getFlags(WCF_WORKING); }
	LL_INLINE bool wasAborted()						{ return getFlags(WCF_ABORT_REQUESTED); }

	// setPriority(): changes the priority of a request
	void setPriority(U32 priority);
	LL_INLINE U32 getPriority()						{ return mRequestPriority; }

	LL_INLINE const std::string& getName() const	{ return mWorkerClassName; }

protected:
	// called from WORKER THREAD
	void setWorking(bool working);

	// Call from doWork only to avoid eating up cpu time. Returns true if work
	// has been aborted yields the current thread and calls
	// mWorkerThread->checkPause()
	bool yield();

	// Calls startWork, adds doWork() to queue
	void addWork(S32 param, U32 priority = LLWorkerThread::PRIORITY_NORMAL);

	// Requests that work be aborted
	void abortWork(bool autocomplete);

	// If doWork is complete or aborted, call endWork() and return true
	bool checkWork(bool aborting = false);

private:
	LL_INLINE void setFlags(U32 flags)				{ mWorkFlags = mWorkFlags | flags; }
	LL_INLINE void clearFlags(U32 flags)			{ mWorkFlags = mWorkFlags & ~flags; }
	LL_INLINE U32 getFlags()						{ return mWorkFlags; }

	// Called from addWork() (MAIN THREAD)
	virtual void startWork(S32 param) = 0;
	// Called from doWork() (MAIN THREAD)
	virtual void endWork(S32 param, bool aborted) = 0;

public:
	LL_INLINE bool getFlags(U32 flags)				{ return (mWorkFlags & flags) != 0; }

protected:
	LLWorkerThread*		mWorkerThread;
	std::string			mWorkerClassName;
	handle_t			mRequestHandle;
	U32					mRequestPriority;	// Last priority set

private:
	LLMutex				mMutex;
	LLAtomicU32			mWorkFlags;
};

#endif // LL_LLWORKERTHREAD_H
