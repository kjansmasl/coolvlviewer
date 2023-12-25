/**
 * @file lliosocket.cpp
 * @author Phoenix
 * @date 2005-07-31
 * @brief Sockets declarations for use with the io pipes
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "lliosocket.h"

#include "llapr.h"

#include "llbuffer.h"
#include "llfasttimer.h"
#include "llhost.h"
#include "llhttpconstants.h"
#include "llpumpio.h"

// Constants
constexpr S32 LL_DEFAULT_LISTEN_BACKLOG = 10;
constexpr S32 LL_SEND_BUFFER_SIZE = 40000;
constexpr S32 LL_RECV_BUFFER_SIZE = 40000;

///////////////////////////////////////////////////////////////////////////////
// LLSocket class
///////////////////////////////////////////////////////////////////////////////

//static
LLSocket::ptr_t LLSocket::create(apr_pool_t* pool, EType type, U16 port)
{
	LLSocket::ptr_t rv;
	apr_socket_t* socket = NULL;
	apr_pool_t* new_pool = NULL;
	apr_status_t status = APR_EGENERAL;

	// create a pool for the socket
	status = apr_pool_create(&new_pool, pool);
	if (ll_apr_warn_status(status))
	{
		llwarns << "Socket creation failure (step 1)" << llendl;
		if (new_pool)
		{
			apr_pool_destroy(new_pool);
		}
		return rv;
	}

	if (STREAM_TCP == type)
	{
		status = apr_socket_create(&socket, APR_INET, SOCK_STREAM,
								   APR_PROTO_TCP, new_pool);
	}
	else if (DATAGRAM_UDP == type)
	{
		status = apr_socket_create(&socket, APR_INET, SOCK_DGRAM,
								   APR_PROTO_UDP, new_pool);
	}
	else
	{
		llwarns << "Socket creation aborted. Bad stream type: " << type
				<< llendl;
		if (new_pool)
		{
			apr_pool_destroy(new_pool);
		}
		return rv;
	}
	if (ll_apr_warn_status(status))
	{
		llwarns << "Socket creation failure (step 2)" << llendl;
		if (new_pool)
		{
			apr_pool_destroy(new_pool);
		}
		return rv;
	}
	// NOTE: cannot use std::make_shared here because LLSocket() constructor
	// is protected...
	rv = ptr_t(new LLSocket(socket, new_pool));
	if (port > 0)
	{
		apr_sockaddr_t* sa = NULL;
		status = apr_sockaddr_info_get(&sa, APR_ANYADDR, APR_UNSPEC, port, 0,
									   new_pool);
		if (ll_apr_warn_status(status))
		{
			llwarns << "Socket creation failure (step 3)" << llendl;
			rv.reset();
			return rv;
		}
		// This allows us to reuse the address on quick down/up. This is
		// unlikely to create problems.
		ll_apr_warn_status(apr_socket_opt_set(socket, APR_SO_REUSEADDR, 1));
		status = apr_socket_bind(socket, sa);
		if (ll_apr_warn_status(status))
		{
			llwarns << "Socket creation failure (step 4)" << llendl;
			rv.reset();
			return rv;
		}
		LL_DEBUGS("IOSocket") << "Bound "
							  << (DATAGRAM_UDP == type ? "udp" : "tcp")
							  << " socket to port: " << sa->port << LL_ENDL;
		if (STREAM_TCP == type)
		{
			// If it is a stream based socket, we need to tell the OS to keep a
			// queue of incoming connections for ACCEPT.
			LL_DEBUGS("IOSocket") << "Setting listen state for socket."
								  << LL_ENDL;
			status = apr_socket_listen(socket, LL_DEFAULT_LISTEN_BACKLOG);
			if (ll_apr_warn_status(status))
			{
				llwarns << "Socket creation failure (step 5)" << llendl;
				rv.reset();
				return rv;
			}
		}
	}
	else
	{
		// We need to indicate that we have an ephemeral port if the previous
		// calls were successful. It will
		port = PORT_EPHEMERAL;
	}
	rv->mPort = port;
	rv->setNonBlocking();
	return rv;
}

//static
LLSocket::ptr_t LLSocket::create(apr_socket_t* socket, apr_pool_t* pool)
{
	LLSocket::ptr_t rv;
	if (!socket)
	{
		return rv;
	}
	// NOTE: cannot use std::make_shared here because LLSocket() constructor
	// is protected...
	rv = ptr_t(new LLSocket(socket, pool));
	rv->mPort = PORT_EPHEMERAL;
	rv->setNonBlocking();
	return rv;
}

bool LLSocket::blockingConnect(const LLHost& host)
{
	if (!mSocket || !host.isOk()) return false;
	apr_sockaddr_t* sa = NULL;
	std::string ip_address;
	ip_address = host.getIPString();
	if (ll_apr_warn_status(apr_sockaddr_info_get(&sa, ip_address.c_str(),
												 APR_UNSPEC, host.getPort(),
												 0, mPool)))
	{
		return false;
	}

	setBlocking(1000);
	LL_DEBUGS("IOSocket") << "Blocking connect " << std::hex
						  << (intptr_t)mSocket << std::dec << LL_ENDL;
	if (ll_apr_warn_status(apr_socket_connect(mSocket, sa))) return false;
	setNonBlocking();
	return true;
}

LLSocket::LLSocket(apr_socket_t* socket, apr_pool_t* pool)
:	mSocket(socket),
	mPool(pool),
	mPort(PORT_INVALID)
{
	LL_DEBUGS("IOSocket") << "Constructing wholely formed socket " << std::hex
						  << (intptr_t)mSocket << std::dec << LL_ENDL;
}

LLSocket::~LLSocket()
{
	// *FIX: clean up memory we are holding.
	if (mSocket)
	{
		LL_DEBUGS("IOSocket") << "Destroying socket " << std::hex
							  << (intptr_t)mSocket << std::dec << LL_ENDL;
		apr_socket_close(mSocket);
		mSocket = NULL;
	}
	if (mPool)
	{
		apr_pool_destroy(mPool);
	}
}

// See http://dev.ariel-networks.com/apr/apr-tutorial/html/apr-tutorial-13.html#ss13.4
// for an explanation of how to get non-blocking sockets and timeouts with
// consistent behavior across platforms.

void LLSocket::setBlocking(S32 timeout)
{
	// set up the socket options
	ll_apr_warn_status(apr_socket_timeout_set(mSocket, timeout));
	ll_apr_warn_status(apr_socket_opt_set(mSocket, APR_SO_NONBLOCK, 0));
	ll_apr_warn_status(apr_socket_opt_set(mSocket, APR_SO_SNDBUF,
					   LL_SEND_BUFFER_SIZE));
	ll_apr_warn_status(apr_socket_opt_set(mSocket, APR_SO_RCVBUF,
					   LL_RECV_BUFFER_SIZE));
}

void LLSocket::setNonBlocking()
{
	// set up the socket options
	ll_apr_warn_status(apr_socket_timeout_set(mSocket, 0));
	ll_apr_warn_status(apr_socket_opt_set(mSocket, APR_SO_NONBLOCK, 1));
	ll_apr_warn_status(apr_socket_opt_set(mSocket, APR_SO_SNDBUF,
					   LL_SEND_BUFFER_SIZE));
	ll_apr_warn_status(apr_socket_opt_set(mSocket, APR_SO_RCVBUF,
					   LL_RECV_BUFFER_SIZE));
}

///////////////////////////////////////////////////////////////////////////////
// LLIOSocketReader class
///////////////////////////////////////////////////////////////////////////////

LLIOSocketReader::LLIOSocketReader(LLSocket::ptr_t socket)
:	mSource(socket),
	mInitialized(false)
{
}

//virtual
LLIOPipe::EStatus LLIOSocketReader::process_impl(const LLChannelDescriptors& channels,
												 buffer_ptr_t& buffer,
												 bool& eos,
												 LLSD& context,
												 LLPumpIO* pump)
{
	LL_FAST_TIMER(FTM_PROCESS_SOCKET_READER);

	PUMP_DEBUG;

	if (!mSource) return STATUS_PRECONDITION_NOT_MET;

	if (!mInitialized)
	{
		PUMP_DEBUG;
		// Since the read will not block, it's ok to initialize and
		// attempt to read off the descriptor immediately.
		mInitialized = true;
		if (pump)
		{
			PUMP_DEBUG;
			LL_DEBUGS("IOSocket") << "Initializing poll descriptor for LLIOSocketReader."
								  << LL_ENDL;
			apr_pollfd_t poll_fd;
			poll_fd.p = NULL;
			poll_fd.desc_type = APR_POLL_SOCKET;
			poll_fd.reqevents = APR_POLLIN;
			poll_fd.rtnevents = 0x0;
			poll_fd.desc.s = mSource->getSocket();
			poll_fd.client_data = NULL;
			pump->setConditional(this, &poll_fd);
		}
	}

#if 0
	if (!buffer)
	{
		buffer = new LLBufferArray;
	}
#endif

	PUMP_DEBUG;

	const apr_size_t READ_BUFFER_SIZE = 1024;
	char read_buf[READ_BUFFER_SIZE];
	apr_size_t len;
	apr_status_t status = APR_SUCCESS;
	do
	{
		PUMP_DEBUG;
		len = READ_BUFFER_SIZE;
		status = apr_socket_recv(mSource->getSocket(), read_buf, &len);
		buffer->append(channels.out(), (U8*)read_buf, len);
	}
	while (APR_SUCCESS == status && READ_BUFFER_SIZE == len);

	LL_DEBUGS("IOSocket") << "socket read status: " << status << LL_ENDL;
	LLIOPipe::EStatus rv = STATUS_OK;

	PUMP_DEBUG;

	// *FIX: Also need to check for broken pipe
	if (APR_STATUS_IS_EOF(status))
	{
		// *FIX: Should we shut down the socket read ?
		if (pump)
		{
			pump->setConditional(this, NULL);
		}
		rv = STATUS_DONE;
		eos = true;
	}
	else if (APR_STATUS_IS_EAGAIN(status))
	{
#if 0	// Disabled by Aura 9-9-8 for DEV-19961.
		// Everything is fine, but we can terminate this process pump.
		rv = STATUS_BREAK;
#endif
	}
	else
	{
		if (ll_apr_warn_status(status))
		{
			rv = STATUS_ERROR;
		}
	}

	PUMP_DEBUG;

	return rv;
}

//
// LLIOSocketWriter
//

LLIOSocketWriter::LLIOSocketWriter(LLSocket::ptr_t socket)
:	mDestination(socket),
	mLastWritten(NULL),
	mInitialized(false)
{
}

//virtual
LLIOPipe::EStatus LLIOSocketWriter::process_impl(const LLChannelDescriptors& channels,
												 buffer_ptr_t& buffer,
												 bool& eos,
												 LLSD& context,
												 LLPumpIO* pump)
{
	LL_FAST_TIMER(FTM_PROCESS_SOCKET_WRITER);
	PUMP_DEBUG;
	if (!mDestination) return STATUS_PRECONDITION_NOT_MET;
	if (!mInitialized)
	{
		PUMP_DEBUG;
		// Since the write will not block, it's ok to initialize and
		// attempt to write immediately.
		mInitialized = true;
		if (pump)
		{
			PUMP_DEBUG;
			LL_DEBUGS("IOSocket") << "Initializing poll descriptor for LLIOSocketWriter."
								  << LL_ENDL;
			apr_pollfd_t poll_fd;
			poll_fd.p = NULL;
			poll_fd.desc_type = APR_POLL_SOCKET;
			poll_fd.reqevents = APR_POLLOUT;
			poll_fd.rtnevents = 0x0;
			poll_fd.desc.s = mDestination->getSocket();
			poll_fd.client_data = NULL;
			pump->setConditional(this, &poll_fd);
		}
	}

	PUMP_DEBUG;
	// *FIX: Some sort of writev implementation would be much more efficient -
	// not only because writev() is better, but also because we won't have to
	// do as much work to find the start address.
	buffer->lock();
	LLBufferArray::segment_iterator_t it;
	LLBufferArray::segment_iterator_t end = buffer->endSegment();
	LLSegment segment;
	it = buffer->constructSegmentAfter(mLastWritten, segment);

#if 0
	if (mLastWritten == NULL)
	{
		it = buffer->beginSegment();
		segment = (*it);
	}
	else
	{
		it = buffer->getSegment(mLastWritten);
		segment = (*it);
		S32 size = segment.size();
		U8* data = segment.data();
		if (mLastWritten == data + size)
		{
			segment = *++it;
		}
		else
		{
			// *FIX: check the math on this one
			segment = LLSegment(it->getChannelMask(), mLastWritten + 1,
								size - (mLastWritten - data));
		}
	}
#endif

	PUMP_DEBUG;
	apr_size_t len;
	bool done = false;
	apr_status_t status = APR_SUCCESS;
	while (it != end)
	{

		PUMP_DEBUG;
		if (it->isOnChannel(channels.in()))
		{
			PUMP_DEBUG;
			len = (apr_size_t)segment.size();
			status = apr_socket_send(
				mDestination->getSocket(),
				(const char*)segment.data(),
				&len);
			// We sometimes get a 'non-blocking socket operation could not be
			// completed immediately' error from apr_socket_send.  In this
			// case we break and the data will be sent the next time the chain
			// is pumped.
			if (APR_STATUS_IS_EAGAIN(status))
			{
				ll_apr_warn_status(status);
				break;
			}

			mLastWritten = segment.data() + len - 1;

			PUMP_DEBUG;
			if (len < (apr_size_t)segment.size())
			{
				break;
			}

		}

		if (++it != end)
		{
			segment = *it;
		}
		else
		{
			done = true;
		}
	}
	buffer->unlock();

	PUMP_DEBUG;

	if (done && eos)
	{
		return STATUS_DONE;
	}
	return STATUS_OK;
}
