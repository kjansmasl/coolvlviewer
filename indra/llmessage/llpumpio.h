/**
 * @file llpumpio.h
 * @author Phoenix
 * @date 2004-11-19
 * @brief Declaration of pump class which manages io chains.
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

#ifndef LL_LLPUMPIO_H
#define LL_LLPUMPIO_H

#include <list>
#include <vector>

#if LL_LINUX
# include <sys/param.h>		// For PATH_MAX in APR.
#endif
#include "apr_pools.h"

#include "llbuffer.h"
#include "llframetimer.h"
#include "lliopipe.h"

// The respond() feature is not used in the viewer. HB   
#define LL_PUMPIO_RESPOND 0

// LLPumpIO is a class to manage sets of I/O chains.
// The pump class provides a thread abstraction for doing I/O based
// communication between two threads in a structured way and optimized for
// processor time. The primary usage is to create a pump, and call pump() on a
// thread used for I/O and call respond() on a thread that is expected to do
// higher level processing. You can call almost any other method from any
// thread (see notes for each method for details). The LLPumpIO instance (there
// is only one in the viewer) uses gAPRPoolp as the APR pool for locking.
// A pump instance manages much of the state for the pipe, including the list
// of pipes in the chain, the channel for each element in the chain, the
// buffer, and if any pipe has marked the stream or process as done. Pipes can
// also set file descriptor based conditional statements so that calls to
// process do not happen until data is ready to be read or written. Pipes
// control execution of calls to process by returning a status code such as
// STATUS_OK or STATUS_BREAK. One way to conceptualize the way IO will work is
// that a pump combines the unit processing of pipes to behave like file pipes
// on the UNIX command line.
class LLPumpIO
{
protected:
	LOG_CLASS(LLPumpIO);

public:
	LLPumpIO();
	~LLPumpIO();

	// Typedef for having a chain of pipes.
	typedef std::vector<LLIOPipe::ptr_t> chain_t;

	// Adds a chain to this pump and process in the next cycle. This method
	// will automatically generate a buffer and assign each link in the chain
	// as if it were the consumer to the previous. 'chain' is the pipes for the
	// chain and 'timeout' the number of seconds in the future to expire (0.f
	// to never expire). Returns true if anything was added to the pump.
	bool addChain(const chain_t& chain, F32 timeout);

	// Structure to associate a pipe with it's buffer io indexes.
	struct LLLinkInfo
	{
		LLIOPipe::ptr_t			mPipe;
		LLChannelDescriptors	mChannels;
	};
	// Typedef for having a chain of LLLinkInfo instances.
	typedef std::vector<LLLinkInfo> links_t;

	// Adds a chain to this pump and process in the next cycle. This method
	// provides a slightly more sophisticated method for adding a chain where
	// the caller can specify which link elements are on what channels. It will
	// fail if no buffer is provided since any call to generate new channels
	// for the buffers will cause unpredictable interleaving of data. 'links'
	// are the pipes and io indexes for the chain, 'datap' is a shared pointer
	// to the data buffer, 'context' (potentially undefined) context meta-data
	// for the chain and 'timeout' is the number of seconds in the future to
	// expire (0.f to never expire). Returns true if anything was added to the
	// pump.
	bool addChain(const links_t& links, LLIOPipe::buffer_ptr_t datap,
				  LLSD context, F32 timeout);

	// Sets or clears a timeout for the running chain with 'timeout' the number
	// of seconds in the future to expire (0.f to never expire). Returns true
	// if the timer was set.
	bool setTimeoutSeconds(F32 timeout);

	// Adjusts the timeout of the running chain. This has no effect if there is
	// no timeout on the chain. 'delta' is the number of seconds to add to or
	// remove from the timeout.
	void adjustTimeoutSeconds(F32 delta);

	// Sets up file descriptors for for the running chain (also see
	// rebuildPollset()). There is currently a limit of one conditional per
	// pipe. 'pipep' is the pipe which is setting a conditional and 'pollp'
	// the entire socket and read/write condition (NULL to remove). Returns
	// true if the poll state was set.
	// NOTE: the internal mechanism for building a pollset based on pipe/
	// pollfd/chain generates an epoll error on Linux (and probably behaves
	// similarly on other platforms) because the pollset rebuilder will add
	// each apr_pollfd_t serially. This does not matter for pipes on the same
	// chain, since any signalled pipe will eventually invoke a call to
	// process(), but is a problem if the same apr_pollfd_t is on different
	// chains. Once we have more than just network i/o on the pump, this might
	// matter.
	// FIXME: Given the structure of the pump and pipe relationship, this
	// should probably go through a different mechanism than the pump. It might
	// be best if the pipe had some kind of controller which was passed into
	// process() rather than the pump which exposed thisinterface.
	bool setConditional(LLIOPipe* pipep, const apr_pollfd_t* pollp);

	// Invoke this method to call process on all running chains; it iterates
	// through the running chains, and if all pipe on a chain are
	// unconditionally ready or if any pipe has any conditional processiong
	// condition then process will be called on every chain which has requested
	// processing. That chain has a file descriptor ready, process() will be
	// called for all pipes which have requested it.
	void pump(S32 poll_timeout);

	// Calls the above method with the default timeout, then calls callback(),
	// when LL_PUMPIO_RESPOND is non zero. HB
	void pump();

#if LL_PUMPIO_RESPOND
	// Adds pipe to a special queue which will be called during the next call
	// to callback() and then dropped from the queue. This call will add a
	// single pipe, with no buffer, context, or channel information to the
	// callback queue. It will be called once, and then dropped. 'pipe p is a
	// single I/O pipe which will be called. Returns true if anything was added
	// to the pump.
	bool respond(LLIOPipe* pipep);

	// Adds a chain to a special queue which will be called during the next
	// call to callback() and then dropped from the queue. It is important to
	// remember that you should not add a data buffer or context which may
	// still be in another chain (that will almost certainly lead to problems).
	// Ensure that you are done reading and writing to those parameters, have
	// new generated, or empty pointers. 'links' are the pipes and I/O indexes
	// for the chain, 'datap' is a shared pointer to the data buffer, and
	// 'context' (potentially undefined) is the context meta-data for chain.
	// Returns true if anything was added to the pump.
	bool respond(const links_t& links, LLIOPipe::buffer_ptr_t datap,
				 LLSD context);

	// Runs through the callback queue and calls process(). This will process
	// all prending responses and call process on each, and will then drop all
	// processed callback requests (which may lead to deleting the referenced
	// objects).
	void callback();
#endif

	// This structure is the stuff we track while running chains.
	struct LLChainInfo
	{
		LLChainInfo();
		void setTimeoutSeconds(F32 timeout);
		void adjustTimeoutSeconds(F32 delta);

		// Basic member data
		links_t::iterator		mHead;
		links_t					mChainLinks;
		LLIOPipe::buffer_ptr_t	mData;
		LLFrameTimer			mTimer;
		LLSD					mContext;
		bool					mInit;
		bool					mEOS;

		// Tracking inside the pump
		typedef std::pair<LLIOPipe::ptr_t, apr_pollfd_t> pipe_conditional_t;
		typedef std::vector<pipe_conditional_t> conditionals_t;
		conditionals_t			mDescriptors;
	};

	typedef std::list<LLChainInfo> running_chains_t;
	typedef running_chains_t::iterator current_chain_t;
	current_chain_t removeRunningChain(current_chain_t& chain);

	// Given the internal state of the chains, rebuilds the pollset. Also see
	//  setConditional().
	void rebuildPollset();

	// Processes the chain passed in. This will potentially modify the
	// internals of the chain. On end, the chain.mHead will equal
	// chain.mChainLinks.end().
	void processChain(LLChainInfo& chain);

	// Rewinds through the chain to try to recover from an error. This will
	// potentially modify the internals of the chain. Retuns true if someone
	// handled the error.
	bool handleChainError(LLChainInfo& chain, LLIOPipe::EStatus error);

protected:
	apr_pollset_t*		mPollset;

	// Memory allocator for pollsets & mutexes.
	apr_pool_t*			mCurrentPool;
	S32					mCurrentPoolReallocCount;

	// All the running chains & info
 	typedef std::vector<LLChainInfo> pending_chains_t;
	pending_chains_t	mPendingChains;
	running_chains_t	mRunningChains;
	current_chain_t		mCurrentChain;

#if LL_PUMPIO_RESPOND
	// Structures necessary for doing callbacks. Since the callbacks only get
	// one chance to run, we do not have to maintain a list.
	typedef std::vector<LLChainInfo> callbacks_t;
	callbacks_t			mPendingCallbacks;
	callbacks_t			mCallbacks;
#endif

	S32					mPollsetClientID;
	bool				mRebuildPollset;
};

#endif // LL_LLPUMPIO_H
