/**
 * @file lliosocket.h
 * @author Phoenix
 * @date 2005-07-31
 * @brief Declaration of files used for handling sockets and associated pipes
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

#ifndef LL_LLIOSOCKET_H
#define LL_LLIOSOCKET_H

// The socket interface provided here is a simple wraper around apr sockets,
// with a pipe source and sink to read and write off of the socket. Every
// socket only performs non-blocking operations except the server socket which
// only performs blocking operations when an OS poll indicates it will not
// block.

#include "apr_pools.h"
#include "apr_network_io.h"

#include "lliopipe.h"

class LLHost;

// LLSocket
// Implementation of a wrapper around a socket.
// An instance of this class represents a single socket over it's entire life -
// from uninitialized, to connected, to a listening socket depending on it's
// purpose. This class simplifies our access into the socket interface by only
// providing stream/tcp and datagram/udp sockets - the only types we are
// interested in, since those are the only properly supported by all of our
// platforms.
class LLSocket
{
protected:
	LOG_CLASS(LLSocket);

public:
	// Reference counted shared pointers to sockets.
	typedef std::shared_ptr<LLSocket> ptr_t;

	// Type of socket to create.
	enum EType
	{
		STREAM_TCP,
		DATAGRAM_UDP,
	};

	// Anonymous enumeration to help identify ports
	enum
	{
		PORT_INVALID = (U16)-1,
		PORT_EPHEMERAL = 0,
	};

	// Creates a socket.
	// This is the call you would use if you intend to create a listen socket.
	// If you intend the socket to be known to external clients without prior
	// port notification, do not use PORT_EPHEMERAL.
	// - pool: the apr pool to use. A child pool will be created and associated
	//   with the socket.
	// - type: the type of socket to create
	// - port: the port for the socket
	// Returns a valid socket shared pointer if the call worked.
	static ptr_t create(apr_pool_t* pool, EType type,
						U16 port = PORT_EPHEMERAL);

	// Creates a LLSocket when you already have an apr socket.
	// This method assumes an ephemeral port. This is typically used by calls
	// which spawn a socket such as a call to <code>accept()</code> as in the
	// server socket. This call should not fail if you have a valid apr socket.
	// Because of the nature of how accept() works, you are expected to create
	// a new pool for the socket, use that pool for the accept, and pass it in
	// here where it will be bound with the socket and destroyed at the same
	// time.
	// - socket: the apr socket to use
	// - pool: the pool used to create the socket. *NOTE: The pool passed
	//   in will be DESTROYED.
	// Returns a valid socket shared pointer if the call worked.
	static ptr_t create(apr_socket_t* socket, apr_pool_t* pool);

	// Performs a blocking connect to a host. Do not use in production.
	// - host: the host to connect this socket to.
	// Returns true if the connect was successful.
	bool blockingConnect(const LLHost& host);

	// Gets the port. This will return PORT_EPHEMERAL if bind was never called.
	// Else, returns the port associated with this socket.
	LL_INLINE U16 getPort() const				{ return mPort; }

	// Gets the apr socket implementation. Returns the raw apr socket.
	LL_INLINE apr_socket_t* getSocket() const	{ return mSocket; }

	// Sets default socket options, with SO_NONBLOCK = 0 and a timeout in µs.
	// - timeout Number of microseconds to wait on this socket. Any
	// negative number means block-forever. TIMEOUT OF 0 IS NON-PORTABLE.
	void setBlocking(S32 timeout);

	// Sets default socket options, with SO_NONBLOCK = 1 and timeout = 0.
	void setNonBlocking();

protected:
	// Protected constructor since should only make sockets with one of the two
	// <code>create()</code> calls.
	LLSocket(apr_socket_t* socket, apr_pool_t* pool);

public:
	// Do not call this directly.
	~LLSocket();

protected:
	// The apr socket.
	apr_socket_t*	mSocket;

	// Our memory pool.
	apr_pool_t*		mPool;

	// The port if we know it.
	U16				mPort;
};

// LLIOSocketReader
// An LLIOPipe implementation which reads from a socket.
// An instance of a socket reader wraps around an LLSocket and performs
// non-blocking reads and passes it to the next pipe in the chain.
class LLIOSocketReader final : public LLIOPipe
{
protected:
	LOG_CLASS(LLIOSocketReader);

public:
	LLIOSocketReader(LLSocket::ptr_t socket);

protected:
	// Processes the data coming in the socket.
	// Since the socket and next pipe must exist for process to make any sense,
	// this method will return STATUS_PRECONDITION_NOT_MET unless if they are
	// not known. If a STATUS_STOP returned by the next link in the chain, this
	// reader will turn of the socket polling.
	//  - buffer: pointer to a buffer which needs processing. Probably NULL.
	//  - context: a data structure to pass structured data
	//  - eos: true if this function is the last. Almost always false.
	//  - pump: the pump which is calling process. May be NULL.
	// Returns STATUS_OK unless the preconditions are not met.
	EStatus process_impl(const LLChannelDescriptors& channels,
						 buffer_ptr_t& buffer, bool& eos, LLSD& context,
						 LLPumpIO* pump) override;

protected:
	LLSocket::ptr_t	mSource;
	std::vector<U8>	mBuffer;
	bool			mInitialized;
};

// LLIOSocketWriter
// An LLIOPipe implementation which writes to a socket
// An instance of a socket writer wraps around an LLSocket and performs
// non-blocking writes of the data passed in.
class LLIOSocketWriter final : public LLIOPipe
{
protected:
	LOG_CLASS(LLIOSocketWriter);

public:
	LLIOSocketWriter(LLSocket::ptr_t socket);

protected:
	// Writeq the data in buffer to the socket.
	// Since the socket and next pipe must exist for process to make any sense,
	// this method will return STATUS_PRECONDITION_NOT_MET unless if they are
	// not known.
	//  - buffer: pointer to a buffer which needs processing.
	//  - context: a data structure to pass structured data
	//  - eos: true if this function is the last.
	//  - pump: the pump which is calling process. May be NULL.
	// Returns STATUS_OK unless the preconditions are not met.
	EStatus process_impl(const LLChannelDescriptors& channels,
						 buffer_ptr_t& buffer, bool& eos,
						 LLSD& context, LLPumpIO* pump) override;

protected:
	LLSocket::ptr_t	mDestination;
	U8*				mLastWritten;
	bool			mInitialized;
};

#endif // LL_LLIOSOCKET_H
