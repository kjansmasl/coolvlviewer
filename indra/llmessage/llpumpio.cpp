/**
 * @file llpumpio.cpp
 * @author Phoenix
 * @date 2004-11-21
 * @brief Implementation of the i/o pump and related functions.
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

#include "apr_poll.h"

#include "llpumpio.h"

#include "llapr.h"
#include "llfasttimer.h"
#include "llstl.h"

// These should not be enabled in production, but they can be intensely useful
// during development for finding certain kinds of bugs.
#if LL_LINUX
# define LL_DEBUG_PIPE_TYPE_IN_PUMP 0
# define LL_DEBUG_POLL_FILE_DESCRIPTORS 0
# if LL_DEBUG_POLL_FILE_DESCRIPTORS
#  include "apr_portable.h"
# endif
#endif

#if LL_DEBUG_PIPE_TYPE_IN_PUMP
#include <typeinfo>
#endif

// Constants for poll timeout.
constexpr S32 DEFAULT_POLL_TIMEOUT = 0;
constexpr F32 DEFAULT_CHAIN_EXPIRY_SECS = 30.f;

// Sorta spammy debug modes.
#define LL_DEBUG_SPEW_BUFFER_CHANNEL_IN_ON_ERROR 0
#define LL_DEBUG_PROCESS_LINK 0
#define LL_DEBUG_PROCESS_RETURN_VALUE 0
// Super spammy debug mode.
#define LL_DEBUG_SPEW_BUFFER_CHANNEL_IN 0
#define LL_DEBUG_SPEW_BUFFER_CHANNEL_OUT 0

// Helper function
void ll_debug_poll_fd(const char* msg, const apr_pollfd_t* pollp)
{
#if LL_DEBUG_POLL_FILE_DESCRIPTORS
	if (!pollp)
	{
		LL_DEBUGS("PumpIO") << "Poll -- " << (msg ? msg : "") << ": no pollfd."
							<< LL_ENDL;
		return;
	}
	if (pollp->desc.s)
	{
		apr_os_sock_t os_sock;
		if (APR_SUCCESS == apr_os_sock_get(&os_sock, pollp->desc.s))
		{
			LL_DEBUGS("PumpIO") << "Poll -- " << (msg?msg:"") << " on fd "
								<< os_sock << " at " << pollp->desc.s
								<< LL_ENDL;
		}
		else
		{
			LL_DEBUGS("PumpIO") << "Poll -- " << (msg?msg:"") << " no fd "
								<< " at " << pollp->desc.s << LL_ENDL;
		}
	}
	else if (pollp->desc.f)
	{
		apr_os_file_t os_file;
		if (APR_SUCCESS == apr_os_file_get(&os_file, pollp->desc.f))
		{
			LL_DEBUGS("PumpIO") << "Poll -- " << (msg?msg:"") << " on fd "
								<< os_file << " at " << pollp->desc.f << LL_ENDL;
		}
		else
		{
			LL_DEBUGS("PumpIO") << "Poll -- " << (msg?msg:"") << " no fd "
								<< " at " << pollp->desc.f << LL_ENDL;
		}
	}
	else
	{
		LL_DEBUGS("PumpIO") << "Poll -- " << (msg?msg:"") << ": no descriptor."
							<< LL_ENDL;
	}
#endif
}

/**
 * @struct ll_delete_apr_pollset_fd_client_data
 * @brief This is a simple helper class to clean up our client data.
 */
struct ll_delete_apr_pollset_fd_client_data
{
	typedef std::pair<LLIOPipe::ptr_t, apr_pollfd_t> pipe_conditional_t;
	void operator()(const pipe_conditional_t& conditional)
	{
		S32* client_id = (S32*)conditional.second.client_data;
		delete client_id;
	}
};

///////////////////////////////////////////////////////////////////////////////
// LLPumpIO class
///////////////////////////////////////////////////////////////////////////////

LLPumpIO::LLPumpIO()
:	mRebuildPollset(false),
	mPollset(NULL),
	mPollsetClientID(0),
	mCurrentPool(NULL),
	mCurrentPoolReallocCount(0),
	mCurrentChain(mRunningChains.end())
{
	mCurrentChain = mRunningChains.end();
}

LLPumpIO::~LLPumpIO()
{
	if (mPollset)
	{
		apr_pollset_destroy(mPollset);
		mPollset = NULL;
	}
	if (mCurrentPool)
	{
		apr_pool_destroy(mCurrentPool);
		mCurrentPool = NULL;
	}
}

bool LLPumpIO::addChain(const chain_t& chain, F32 timeout)
{
	if (chain.empty()) return false;

	LLChainInfo info;
	info.setTimeoutSeconds(timeout);
	info.mData = std::make_shared<LLBufferArray>();
	info.mData->setThreaded(false);
	LLLinkInfo link;
#if LL_DEBUG_PIPE_TYPE_IN_PUMP
	LL_DEBUGS("PumpIO") << chain[0] << "Add chain '"
						<< typeid(*(chain[0])).name() << "'" << LL_ENDL;
#else
	LL_DEBUGS("PumpIO") << "Add chain: " << chain[0] << LL_ENDL;
#endif
	chain_t::const_iterator it = chain.begin();
	chain_t::const_iterator end = chain.end();
	for ( ; it != end; ++it)
	{
		link.mPipe = *it;
		link.mChannels = info.mData->nextChannel();
		info.mChainLinks.push_back(link);
	}
	mPendingChains.push_back(info);
	return true;
}

bool LLPumpIO::addChain(const LLPumpIO::links_t& links,
						LLIOPipe::buffer_ptr_t datap, LLSD context,
						F32 timeout)
{
	// Remember that if the caller is providing a full link description, we
	// need to have that description matched to a particular buffer.
	if (!datap || links.empty())
	{
		return false;
	}

#if LL_DEBUG_PIPE_TYPE_IN_PUMP
	LL_DEBUGS("PumpIO") << "Add chain: " << links[0].mPipe << " '"
						<< typeid(*(links[0].mPipe)).name() << "'" << LL_ENDL;
#else
	LL_DEBUGS("PumpIO") << "Add chain: " << links[0].mPipe << LL_ENDL;
#endif
	LLChainInfo info;
	info.setTimeoutSeconds(timeout);
	info.mChainLinks = links;
	info.mData = datap;
	info.mContext = context;
	mPendingChains.push_back(info);
	return true;
}

bool LLPumpIO::setTimeoutSeconds(F32 timeout)
{
	// If no chain is running, return failure.
	if (mRunningChains.end() == mCurrentChain)
	{
		return false;
	}
	mCurrentChain->setTimeoutSeconds(timeout);
	return true;
}

void LLPumpIO::adjustTimeoutSeconds(F32 delta)
{
	// Ensure a chain is running
	if (mRunningChains.end() != mCurrentChain)
	{
		mCurrentChain->adjustTimeoutSeconds(delta);
	}
}

static std::string events_2_string(apr_int16_t events)
{
	std::ostringstream ostr;
	if (events & APR_POLLIN)
	{
		ostr << "read,";
	}
	if (events & APR_POLLPRI)
	{
		ostr << "priority,";
	}
	if (events & APR_POLLOUT)
	{
		ostr << "write,";
	}
	if (events & APR_POLLERR)
	{
		ostr << "error,";
	}
	if (events & APR_POLLHUP)
	{
		ostr << "hangup,";
	}
	if (events & APR_POLLNVAL)
	{
		ostr << "invalid,";
	}
	return chop_tail_copy(ostr.str(), 1);
}

bool LLPumpIO::setConditional(LLIOPipe* pipep, const apr_pollfd_t* pollp)
{
	if (!pipep)
	{
		return false;
	}

	ll_debug_poll_fd("Set conditional", pollp);
	LL_DEBUGS("PumpIO") << "Setting conditionals ("
						<< (pollp ? events_2_string(pollp->reqevents) : "NULL")
						<< ") "
#if LL_DEBUG_PIPE_TYPE_IN_PUMP
						<< "on pipe " << typeid(*pipep).name()
#endif
						<< " at " << std::hex << pipep << std::dec << LL_ENDL;

	// Remove any matching poll file descriptors for this pipe.
	LLIOPipe::ptr_t pipe_ptr(pipep);
	LLChainInfo::conditionals_t::iterator it =
		mCurrentChain->mDescriptors.begin();
	while (it != mCurrentChain->mDescriptors.end())
	{
		LLChainInfo::pipe_conditional_t& value = (*it);
		if (pipe_ptr == value.first)
		{
			ll_delete_apr_pollset_fd_client_data()(value);
			it = mCurrentChain->mDescriptors.erase(it);
			mRebuildPollset = true;
		}
		else
		{
			++it;
		}
	}

	if (!pollp)
	{
		mRebuildPollset = true;
		return true;
	}
	LLChainInfo::pipe_conditional_t value;
	value.first = pipe_ptr;
	value.second = *pollp;
	value.second.rtnevents = 0;
	if (!pollp->p)
	{
		// Each fd needs a pool to work with, so if one was not specified, use
		// this pool. *FIXME: should it always be this pool ?
		value.second.p = gAPRPoolp;
	}
	value.second.client_data = new S32(++mPollsetClientID);
	mCurrentChain->mDescriptors.push_back(value);
	mRebuildPollset = true;
	return true;
}

void LLPumpIO::pump()
{
	pump(DEFAULT_POLL_TIMEOUT);
#if LL_PUMPIO_RESPOND
	callback();
#endif
}

LLPumpIO::current_chain_t LLPumpIO::removeRunningChain(LLPumpIO::current_chain_t& run_chain)
{
	std::for_each(run_chain->mDescriptors.begin(),
				  run_chain->mDescriptors.end(),
				  ll_delete_apr_pollset_fd_client_data());
	return mRunningChains.erase(run_chain);
}

// Timeout is in microseconds
void LLPumpIO::pump(S32 poll_timeout)
{
	LL_FAST_TIMER(FTM_PUMP_IO);

	// We need to move all of the pending heads over to the running chains.
	PUMP_DEBUG;

	// Move the pending chains over to the running chaings
	if (!mPendingChains.empty())
	{
		PUMP_DEBUG;
		std::copy(mPendingChains.begin(), mPendingChains.end(),
				  std::back_insert_iterator<running_chains_t>(mRunningChains));
		mPendingChains.clear();
		PUMP_DEBUG;
	}

	PUMP_DEBUG;
	// Rebuild the pollset if necessary
	if (mRebuildPollset)
	{
		PUMP_DEBUG;
		rebuildPollset();
		mRebuildPollset = false;
	}

	// Poll based on the last known pollset
	// *TODO: may want to pass in a poll timeout so it works correctly in
	// single and multi threaded processes.
	PUMP_DEBUG;
	typedef std::map<S32, S32> signal_client_t;
	signal_client_t signalled_client;
	const apr_pollfd_t* poll_fd = NULL;
	if (mPollset)
	{
		PUMP_DEBUG;
		S32 count = 0;
		S32 client_id = 0;
        {
			LL_TRACY_TIMER(TRC_PUMP_POLL);
            apr_pollset_poll(mPollset, poll_timeout, &count, &poll_fd);
        }
		PUMP_DEBUG;
		for (S32 ii = 0; ii < count; ++ii)
		{
			ll_debug_poll_fd("Signalled pipe", &poll_fd[ii]);
			client_id = *((S32*)poll_fd[ii].client_data);
			signalled_client[client_id] = ii;
		}
		PUMP_DEBUG;
	}

	PUMP_DEBUG;
	// set up for a check to see if each one was signalled
	signal_client_t::iterator not_signalled = signalled_client.end();

	// Process everything as appropriate
	running_chains_t::iterator run_chain = mRunningChains.begin();
	bool process_this_chain = false;
	while (run_chain != mRunningChains.end())
	{
		PUMP_DEBUG;
		if (run_chain->mInit && run_chain->mTimer.getStarted() &&
			run_chain->mTimer.hasExpired())
		{
			PUMP_DEBUG;
			if (handleChainError(*run_chain, LLIOPipe::STATUS_EXPIRED))
			{
				// the pipe probably handled the error. If the handler forgot
				// to reset the expiration then we need to do that here.
				if (run_chain->mTimer.getStarted() &&
					run_chain->mTimer.hasExpired())
				{
					PUMP_DEBUG;
					llinfos << "Error handler forgot to reset timeout. "
							<< "Resetting to " << DEFAULT_CHAIN_EXPIRY_SECS
							<< " seconds." << llendl;
					run_chain->setTimeoutSeconds(DEFAULT_CHAIN_EXPIRY_SECS);
				}
			}
			else
			{
				PUMP_DEBUG;
				// it timed out and no one handled it, so we need to
				// retire the chain
#if LL_DEBUG_PIPE_TYPE_IN_PUMP
				LL_DEBUGS("PumpIO") << "Removing chain "
									<< run_chain->mChainLinks[0].mPipe
									<< " '"
									<< typeid(*(run_chain->mChainLinks[0].mPipe)).name()
									<< "' because it timed out." << LL_ENDL;
#endif
				run_chain = removeRunningChain(run_chain);
				continue;
			}
		}

		mCurrentChain = run_chain;

		if (run_chain->mDescriptors.empty())
		{
			// if there are no conditionals, just process this chain.
			process_this_chain = true;
		}
		else
		{
			PUMP_DEBUG;
			// Check if this run chain was signalled. If any file descriptor is
			// ready for something, then go ahead and process this chain.
			process_this_chain = false;
			if (!signalled_client.empty())
			{
				PUMP_DEBUG;
				LLChainInfo::conditionals_t::iterator it;
				it = run_chain->mDescriptors.begin();
				LLChainInfo::conditionals_t::iterator end;
				end = run_chain->mDescriptors.end();
				S32 client_id = 0;
				signal_client_t::iterator signal;
				for (; it != end; ++it)
				{
					PUMP_DEBUG;
					client_id = *((S32*)(it->second.client_data));
					signal = signalled_client.find(client_id);
					if (signal == not_signalled) continue;
					constexpr apr_int16_t POLL_CHAIN_ERROR =
						APR_POLLHUP | APR_POLLNVAL | APR_POLLERR;
					const apr_pollfd_t* pollp = &(poll_fd[signal->second]);
					if (pollp->rtnevents & POLL_CHAIN_ERROR)
					{
						// Potential eror condition has been returned. If HUP
						// was one of them, we pass that as the error even
						// though there may be more. If there are in fact more
						// errors, we'll just wait for that detection until the
						// next pump() cycle to catch it so that the logic here
						// gets no more strained than it already is.
						LLIOPipe::EStatus error_status;
						if (pollp->rtnevents & APR_POLLHUP)
						{
							error_status = LLIOPipe::STATUS_LOST_CONNECTION;
						}
						else
						{
							error_status = LLIOPipe::STATUS_ERROR;
						}
						if (handleChainError(*run_chain, error_status)) break;
						ll_debug_poll_fd("Removing pipe", pollp);
						llwarns << "Removing pipe "
								<< run_chain->mChainLinks[0].mPipe << " '"
#if LL_DEBUG_PIPE_TYPE_IN_PUMP
								<< typeid(*(run_chain->mChainLinks[0].mPipe)).name()
#endif
								<< "' because: "
								<< events_2_string(pollp->rtnevents) << llendl;
						run_chain->mHead = run_chain->mChainLinks.end();
						break;
					}

					// At least 1 fd got signalled, and there were no errors.
					// That means we process this chain.
					process_this_chain = true;
					break;
				}
			}
		}
		if (process_this_chain)
		{
			PUMP_DEBUG;
			if (!run_chain->mInit)
			{
				run_chain->mHead = run_chain->mChainLinks.begin();
				run_chain->mInit = true;
			}
			PUMP_DEBUG;
			processChain(*run_chain);
		}

		PUMP_DEBUG;
		if (run_chain->mHead == run_chain->mChainLinks.end())
		{
#if LL_DEBUG_PIPE_TYPE_IN_PUMP
			LL_DEBUGS("PumpIO") << "Removing chain "
								<< run_chain->mChainLinks[0].mPipe
								<< " '"
								<< typeid(*(run_chain->mChainLinks[0].mPipe)).name()
								<< "' because we reached the end." << LL_ENDL;
#else
			LL_DEBUGS("PumpIO") << "Removing chain "
								<< run_chain->mChainLinks[0].mPipe
								<< " because we reached the end." << LL_ENDL;
#endif

			PUMP_DEBUG;
			// This chain is done. Clean up any allocated memory and erase the
			// chain info.
			run_chain = removeRunningChain(run_chain);

			// *NOTE: may not always need to rebuild the pollset.
			mRebuildPollset = true;
		}
		else
		{
			PUMP_DEBUG;
			// This chain needs more processing: just go to the next chain.
			++run_chain;
		}
	}

	PUMP_DEBUG;
	// null out the chain
	mCurrentChain = mRunningChains.end();
	END_PUMP_DEBUG;
}

#if LL_PUMPIO_RESPOND
bool LLPumpIO::respond(LLIOPipe* pipep)
{
	if (!pipep)
	{
		return false;
	}
	LLChainInfo info;
	LLLinkInfo link;
	link.mPipe = pipep;
	info.mChainLinks.push_back(link);
	mPendingCallbacks.push_back(info);
	return true;
}

bool LLPumpIO::respond(const links_t& links, LLIOPipe::buffer_ptr_t datap,
					   LLSD context)
{
	// If the caller is providing a full link description, we need to have that
	// description matched to a particular buffer.
	if (!datap || links.empty())
	{
		return false;
	}
	// Add the callback response
	LLChainInfo info;
	info.mChainLinks = links;
	info.mData = datap;
	info.mContext = context;
	mPendingCallbacks.push_back(info);
	return true;
}

void LLPumpIO::callback()
{
	{
		std::copy(mPendingCallbacks.begin(), mPendingCallbacks.end(),
				  std::back_insert_iterator<callbacks_t>(mCallbacks));
		mPendingCallbacks.clear();
	}

	if (!mCallbacks.empty())
	{
		LL_FAST_TIMER(FTM_PUMP_CALLBACK_CHAIN);
		for (callbacks_t::iterator it = mCallbacks.begin(),
								   end = mCallbacks.end();
			 it != end; ++it)
		{
			// It is always the first and last time for respone chains
			it->mHead = it->mChainLinks.begin();
			it->mInit = true;
			it->mEOS = true;
			processChain(*it);
		}
		mCallbacks.clear();
	}
}
#endif

void LLPumpIO::rebuildPollset()
{
	if (mPollset)
	{
		apr_pollset_destroy(mPollset);
		mPollset = NULL;
	}
	U32 size = 0;
	running_chains_t::iterator run_it = mRunningChains.begin();
	running_chains_t::iterator run_end = mRunningChains.end();
	for ( ; run_it != run_end; ++run_it)
	{
		size += run_it->mDescriptors.size();
	}

	if (size)
	{
		// Recycle the memory pool
		constexpr S32 POLLSET_POOL_RECYCLE_COUNT = 100;
		if (mCurrentPool &&
			(++mCurrentPoolReallocCount % POLLSET_POOL_RECYCLE_COUNT) == 0)
		{
			apr_pool_destroy(mCurrentPool);
			mCurrentPool = NULL;
			mCurrentPoolReallocCount = 0;
		}
		if (!mCurrentPool)
		{
			apr_status_t status = apr_pool_create(&mCurrentPool, gAPRPoolp);
			(void)ll_apr_warn_status(status);
		}

		// Add all of the file descriptors
		run_it = mRunningChains.begin();
		LLChainInfo::conditionals_t::iterator fd_it;
		LLChainInfo::conditionals_t::iterator fd_end;
		apr_pollset_create(&mPollset, size, mCurrentPool, 0);
		for ( ; run_it != run_end; ++run_it)
		{
			fd_it = run_it->mDescriptors.begin();
			fd_end = run_it->mDescriptors.end();
			for ( ; fd_it != fd_end; ++fd_it)
			{
				apr_pollset_add(mPollset, &(fd_it->second));
			}
		}
	}
}

void LLPumpIO::processChain(LLChainInfo& chain)
{
	PUMP_DEBUG;
	LLIOPipe::EStatus status = LLIOPipe::STATUS_OK;
	links_t::iterator it = chain.mHead;
	links_t::iterator end = chain.mChainLinks.end();
	if (it == end) return;

	bool need_process_signaled = false;
	bool keep_going = true;
	do
	{
#if LL_DEBUG_PROCESS_LINK
# if LL_DEBUG_PIPE_TYPE_IN_PUMP
		llinfos << "Processing " << typeid(*(it->mPipe)).name() << "."
				<< llendl;
# else
		llinfos << "Processing link " << it->mPipe << "." << llendl;
# endif
#endif
#if LL_DEBUG_SPEW_BUFFER_CHANNEL_IN
		if (chain.mData)
		{
			char* buf = NULL;
			S32 bytes = chain.mData->countAfter(it->mChannels.in(), NULL);
			if (bytes)
			{
				buf = new char[bytes + 1];
				chain.mData->readAfter(it->mChannels.in(), NULL, (U8*)buf,
									   bytes);
				buf[bytes] = '\0';
				llinfos << "CHANNEL IN(" << it->mChannels.in() << "): "
						<< buf << llendl;
				delete[] buf;
				buf = NULL;
			}
			else
			{
				llinfos << "CHANNEL IN(" << it->mChannels.in()<< "): (null)"
						<< llendl;
			}
		}
#endif
		PUMP_DEBUG;
		status = it->mPipe->process(it->mChannels, chain.mData, chain.mEOS,
									chain.mContext, this);
#if LL_DEBUG_SPEW_BUFFER_CHANNEL_OUT
		if (chain.mData)
		{
			char* buf = NULL;
			S32 bytes = chain.mData->countAfter(it->mChannels.out(), NULL);
			if (bytes)
			{
				buf = new char[bytes + 1];
				chain.mData->readAfter(it->mChannels.out(), NULL, (U8*)buf,
									   bytes);
				buf[bytes] = '\0';
				llinfos << "CHANNEL OUT(" << it->mChannels.out()<< "): "
						<< buf << llendl;
				delete[] buf;
				buf = NULL;
			}
			else
			{
				llinfos << "CHANNEL OUT(" << it->mChannels.out()<< "): (null)"
						<< llendl;
			}
		}
#endif

#if LL_DEBUG_PROCESS_RETURN_VALUE
		// Only bother with the success codes - error codes are logged
		// below.
		if (LLIOPipe::isSuccess(status))
		{
			llinfos << "Pipe returned: '"
# if LL_DEBUG_PIPE_TYPE_IN_PUMP
					<< typeid(*(it->mPipe)).name() << "':'"
# endif
					<< LLIOPipe::lookupStatusString(status) << "'" << llendl;
		}
#endif

		PUMP_DEBUG;
		switch (status)
		{
			case LLIOPipe::STATUS_OK:
				// no-op
				break;

			case LLIOPipe::STATUS_STOP:
				PUMP_DEBUG;
				status = LLIOPipe::STATUS_OK;
				chain.mHead = end;
				keep_going = false;
				break;

			case LLIOPipe::STATUS_DONE:
				PUMP_DEBUG;
				status = LLIOPipe::STATUS_OK;
				chain.mHead = (it + 1);
				chain.mEOS = true;
				break;

			case LLIOPipe::STATUS_BREAK:
				PUMP_DEBUG;
				status = LLIOPipe::STATUS_OK;
				keep_going = false;
				break;

			case LLIOPipe::STATUS_NEED_PROCESS:
				PUMP_DEBUG;
				status = LLIOPipe::STATUS_OK;
				if (!need_process_signaled)
				{
					need_process_signaled = true;
					chain.mHead = it;
				}
				break;

			default:
			{
				PUMP_DEBUG;
				if (LLIOPipe::isError(status))
				{
					llinfos << "Pump generated pipe err: '"
#if LL_DEBUG_PIPE_TYPE_IN_PUMP
							<< typeid(*(it->mPipe)).name() << "':'"
#endif
							<< LLIOPipe::lookupStatusString(status) << "'"
							<< llendl;
#if LL_DEBUG_SPEW_BUFFER_CHANNEL_IN_ON_ERROR
					if (chain.mData)
					{
						char* buf = NULL;
						S32 bytes = chain.mData->countAfter(it->mChannels.in(),
															NULL);
						if (bytes)
						{
							buf = new char[bytes + 1];
							chain.mData->readAfter(it->mChannels.in(), NULL,
												   (U8*)buf, bytes);
							buf[bytes] = '\0';
							llinfos << "Input After Error: " << buf << llendl;
							delete[] buf;
							buf = NULL;
						}
						else
						{
							llinfos << "Input After Error: (null)" << llendl;
						}
					}
					else
					{
						llinfos << "Input After Error: (null)" << llendl;
					}
#endif
					keep_going = false;
					chain.mHead  = it;
					if (!handleChainError(chain, status))
					{
						chain.mHead = end;
					}
				}
				else
				{
					llinfos << "Unhandled status code: " << status << ":"
							<< LLIOPipe::lookupStatusString(status) << llendl;
				}
				break;
			}
		}
		PUMP_DEBUG;
	}
	while (keep_going && ++it != end);

	PUMP_DEBUG;
}

bool LLPumpIO::handleChainError(LLChainInfo& chain,
								LLIOPipe::EStatus error)
{
	links_t::reverse_iterator rit;
	if (chain.mHead == chain.mChainLinks.end())
	{
		rit = links_t::reverse_iterator(chain.mHead);
	}
	else
	{
		rit = links_t::reverse_iterator(chain.mHead + 1);
	}

	links_t::reverse_iterator rend = chain.mChainLinks.rend();
	bool handled = false;
	bool keep_going = true;
	do
	{
#if LL_DEBUG_PIPE_TYPE_IN_PUMP
		LL_DEBUGS("PumpIO") << "Passing error to "
							<< typeid(*(rit->mPipe)).name() << "."
							<< LL_ENDL;
#endif
		error = rit->mPipe->handleError(error, this);
		switch (error)
		{
			case LLIOPipe::STATUS_OK:
				handled = true;
				chain.mHead = rit.base();
				break;

			case LLIOPipe::STATUS_STOP:
			case LLIOPipe::STATUS_DONE:
			case LLIOPipe::STATUS_BREAK:
			case LLIOPipe::STATUS_NEED_PROCESS:
#if LL_DEBUG_PIPE_TYPE_IN_PUMP
				LL_DEBUGS("PumpIO") << "Pipe "
									<< typeid(*(rit->mPipe)).name()
									<< " returned code to stop error handler."
									<< LL_ENDL;
#endif
				keep_going = false;
				break;

			case LLIOPipe::STATUS_EXPIRED:
				keep_going = false;
				break;

			default:
				if (LLIOPipe::isSuccess(error))
				{
					llinfos << "Unhandled status code: " << error << ":"
							<< LLIOPipe::lookupStatusString(error) << llendl;
					error = LLIOPipe::STATUS_ERROR;
					keep_going = false;
				}
				break;
		}
	}
	while (keep_going && !handled && ++rit != rend);

	return handled;
}

///////////////////////////////////////////////////////////////////////////////
// LLPumpIO::LLChainInfo structure
///////////////////////////////////////////////////////////////////////////////

LLPumpIO::LLChainInfo::LLChainInfo()
:	mInit(false),
	mEOS(false)
{
	mTimer.setTimerExpirySec(DEFAULT_CHAIN_EXPIRY_SECS);
}

void LLPumpIO::LLChainInfo::setTimeoutSeconds(F32 timeout)
{
	if (timeout > 0.f)
	{
		mTimer.start();
		mTimer.reset();
		mTimer.setTimerExpirySec(timeout);
	}
	else
	{
		mTimer.stop();
	}
}

void LLPumpIO::LLChainInfo::adjustTimeoutSeconds(F32 delta)
{
	if (mTimer.getStarted())
	{
		mTimer.setExpiryAt(mTimer.expiresAt() + (F64)delta);
	}
}
