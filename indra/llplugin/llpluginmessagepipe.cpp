/**
 * @file llpluginmessagepipe.cpp
 * @brief Classes that implement connections from the plugin system to pipes/pumps.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include "llpluginmessagepipe.h"

#include "llapr.h"
#include "llbufferstream.h"
#include "lltimer.h"				// For ms_sleep()

static const char MESSAGE_DELIMITER = '\0';

LLPluginMessagePipeOwner::LLPluginMessagePipeOwner()
:	mMessagePipe(NULL),
	mSocketError(APR_SUCCESS)
{
}

//virtual
LLPluginMessagePipeOwner::~LLPluginMessagePipeOwner()
{
	killMessagePipe();
}

//virtual
apr_status_t LLPluginMessagePipeOwner::socketError(apr_status_t error)
{
	mSocketError = error;
	return error;
}

//virtual
void LLPluginMessagePipeOwner::setMessagePipe(LLPluginMessagePipe* read_pipe)
{
	// Save a reference to this pipe
	mMessagePipe = read_pipe;
}

bool LLPluginMessagePipeOwner::writeMessageRaw(const std::string& message)
{
	if (mMessagePipe)
	{
		return mMessagePipe->addMessage(message);
	}

	llwarns << "Dropping message: " << message << llendl;
	return false;
}

void LLPluginMessagePipeOwner::killMessagePipe()
{
	if (mMessagePipe)
	{
		delete mMessagePipe;
		mMessagePipe = NULL;
	}
}

LLPluginMessagePipe::LLPluginMessagePipe(LLPluginMessagePipeOwner* owner,
										 LLSocket::ptr_t socket)
:	mOutputStartIndex(0),
	mOwner(owner),
	mSocket(socket)
{
	mOwner->setMessagePipe(this);
}

LLPluginMessagePipe::~LLPluginMessagePipe()
{
	if (mOwner)
	{
		mOwner->setMessagePipe(NULL);
	}
}

// Queues the message for later output
bool LLPluginMessagePipe::addMessage(const std::string& message)
{
	mOutputMutex.lock();

	// If we are starting to use up too much memory, clear
	if (mOutputStartIndex > 1024 * 1024)
	{
		mOutput = mOutput.substr(mOutputStartIndex);
		mOutputStartIndex = 0;
	}

	mOutput += message;
	mOutput += MESSAGE_DELIMITER;	// message separator

	mOutputMutex.unlock();

	return true;
}

void LLPluginMessagePipe::setSocketTimeout(apr_interval_time_t timeout_usec)
{
	// We never want to sleep forever, so force negative timeouts to become
	// non-blocking. According to this page:
	// http://dev.ariel-networks.com/apr/apr-tutorial/html/apr-tutorial-13.html
	// blocking/non-blocking with apr sockets is somewhat non-portable.
	apr_socket_opt_set(mSocket->getSocket(), APR_SO_NONBLOCK, 1);
	if (timeout_usec <= 0)
	{
		// Make the socket non-blocking
		apr_socket_timeout_set(mSocket->getSocket(), 0);
	}
	else
	{
		// Make the socket blocking-with-timeout
		apr_socket_timeout_set(mSocket->getSocket(), timeout_usec);
	}
}

bool LLPluginMessagePipe::pump(F64 timeout)
{
	bool result = pumpOutput();
	if (result)
	{
		result = pumpInput(timeout);
	}
	return result;
}

bool LLPluginMessagePipe::pumpOutput()
{
	bool result = true;

	if (mSocket)
	{
		mOutputMutex.lock();

		const char* output_data = &(mOutput.data()[mOutputStartIndex]);
		if (*output_data != '\0')
		{
			// Write any outgoing messages
			apr_size_t in_size = (apr_size_t)(mOutput.size() -
											  mOutputStartIndex);
			apr_size_t out_size = in_size;

			setSocketTimeout(0);

			apr_status_t status = apr_socket_send(mSocket->getSocket(),
												  output_data, &out_size);

			if (status == APR_SUCCESS || APR_STATUS_IS_EAGAIN(status))
			{
				// Success or Socket buffer is full...

				// If we have pumped the entire string, clear it
				if (out_size == in_size)
				{
					mOutputStartIndex = 0;
					mOutput.clear();
				}
				else
				{
					llassert(in_size > out_size);

					// Remove the written part from the buffer and try again
					// later.
					mOutputStartIndex += out_size;
				}
			}
			else if (APR_STATUS_IS_EOF(status))
			{
				// This is what we normally expect when a plugin exits.
				llinfos << "Got EOF from plugin socket. " << llendl;

				if (mOwner)
				{
					mOwner->socketError(status);
				}
				result = false;
			}
			else
			{
				// Some other error. Treat this as fatal.
				ll_apr_warn_status(status);

				if (mOwner)
				{
					mOwner->socketError(status);
				}
				result = false;
			}
		}

		mOutputMutex.unlock();
	}

	return result;
}

bool LLPluginMessagePipe::pumpInput(F64 timeout)
{
	bool result = true;

	if (mSocket)
	{
		// FIXME: For some reason, the apr timeout stuff is not working
		// properly on windows. Until such time as we figure out why, do not
		// try to use the socket timeout; just sleep here instead.
#if LL_WINDOWS
		if (timeout != 0.f)
		{
			ms_sleep((int)(timeout * 1000.f));
			timeout = 0.f;
		}
#endif

		char input_buf[1024];
		apr_size_t request_size;

		if (timeout == 0.f)
		{
			// If we have no timeout, start out with a full read.
			request_size = sizeof(input_buf);
		}
		else
		{
			// Start out by reading one byte, so that any data received will
			// wake us up.
			request_size = 1;
		}

		// Use the timeout so we will sleep if no data is available.
		setSocketTimeout((apr_interval_time_t)(timeout * 1000000));

		while (true)
		{
			apr_size_t size = request_size;
			apr_status_t status = apr_socket_recv(mSocket->getSocket(),
												  input_buf, &size);
			if (size > 0)
			{
				mInputMutex.lock();
				mInput.append(input_buf, size);
				mInputMutex.unlock();
			}

			if (status == APR_SUCCESS)
			{
				LL_DEBUGS("PluginSocket") << "success, read " << size
										  << LL_ENDL;
				if (size != request_size)
				{
					// This was a short read, so we are done.
					break;
				}
			}
			else if (APR_STATUS_IS_TIMEUP(status))
			{
				LL_DEBUGS("PluginSocket") << "TIMEUP, read " << size
										  << LL_ENDL;
				// Timeout was hit. Since the initial read is 1 byte, this
				// should never be a partial read.
				break;
			}
			else if (APR_STATUS_IS_EAGAIN(status))
			{
				LL_DEBUGS("PluginSocket") << "EAGAIN, read " << size
										  << LL_ENDL;
				// Non-blocking read returned immediately.
				break;
			}
			else if (APR_STATUS_IS_EOF(status))
			{
				// This is what we normally expect when a plugin exits.
				llinfos << "Got EOF from plugin socket. " << llendl;
				if (mOwner)
				{
					mOwner->socketError(status);
				}
				result = false;
				break;
			}
			else
			{
				// Some other error; treat this as fatal.
				ll_apr_warn_status(status);
				if (mOwner)
				{
					mOwner->socketError(status);
				}
				result = false;
				break;
			}

			if (timeout != 0.f)
			{
				// Second and subsequent reads should not use the timeout...
				setSocketTimeout(0);
				// ... and should try to fill the input buffer
				request_size = sizeof(input_buf);
			}
		}

		processInput();
	}

	return result;
}

void LLPluginMessagePipe::processInput()
{
	// Look for input delimiter(s) in the input buffer.
	std::string message;
	size_t delim;
	mInputMutex.lock();
	while ((delim = mInput.find(MESSAGE_DELIMITER)) != std::string::npos)
	{
		// Let the owner process this message
		if (mOwner)
		{
			// Pull the message out of the input buffer before calling
			// receiveMessageRaw. It is now possible for this function to get
			// called recursively (in the case where the plugin makes a
			// blocking request) and this guarantees that the messages will get
			// dequeued correctly.
			message.assign(mInput, 0, delim);
			mInput.erase(0, delim + 1);
			mInputMutex.unlock();
			mOwner->receiveMessageRaw(message);
			mInputMutex.lock();
		}
		else
		{
			llwarns << "NULL owner" << llendl;
		}
	}
	mInputMutex.unlock();
}
