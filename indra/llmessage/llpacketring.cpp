/**
 * @file llpacketring.cpp
 * @brief implementation of LLPacketRing class for a packet.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llpacketring.h"

#if LL_WINDOWS
# include <winsock2.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
#endif

#include "llmessage.h"
#include "llnet.h"
#include "llproxy.h"
#include "llrand.h"
#include "lltimer.h"

///////////////////////////////////////////////////////////////////////////////
// LLPacketBuffer class. Used to be in its own llpacketbuffer.h/cpp module, but
// is only used by LLPacketRing, so I moved it here. HB
///////////////////////////////////////////////////////////////////////////////

class LLPacketBuffer
{
public:
	LLPacketBuffer(const LLHost& host, const char* datap, S32 size);
	LL_INLINE LLPacketBuffer(S32 socket)			{ init(socket); }

	void init(S32 socket);

	LL_INLINE S32 getSize() const					{ return mSize; }
	LL_INLINE const char* getData() const			{ return mData; }
	LL_INLINE LLHost getHost() const				{ return mHost; }
	LL_INLINE LLHost getReceivingInterface() const	{ return mReceivingIF; }

protected:
	LLHost	mHost;					// Source/dest IP and port
	LLHost	mReceivingIF;			// Source/dest IP and port
	char	mData[NET_BUFFER_SIZE];	// Packet data
	S32		mSize;					// Size of buffer in bytes
};

LLPacketBuffer::LLPacketBuffer(const LLHost& host, const char* datap, S32 size)
:	mHost(host)
{
	mSize = 0;
	mData[0] = '!';

	if (size > NET_BUFFER_SIZE)
	{
		llerrs << "Sending packet > " << NET_BUFFER_SIZE << " of size " << size
			   << llendl;
	}
	if (datap)
	{
		memcpy(mData, datap, size);
		mSize = size;
	}
}

void LLPacketBuffer::init(S32 socket)
{
	mSize = receive_packet(socket, mData);
	mHost = get_sender();
	mReceivingIF = get_receiving_interface();
}

///////////////////////////////////////////////////////////////////////////////
// LLPacketRing class
///////////////////////////////////////////////////////////////////////////////

LLPacketRing::LLPacketRing()
:	mUseInThrottle(false),
	mUseOutThrottle(false),
	mInThrottle(256000.f),
	mOutThrottle(64000.f),
	mActualBitsIn(0),
	mActualBitsOut(0),
	mMaxBufferLength(64000),
	mInBufferLength(0),
	mOutBufferLength(0)
{
}

LLPacketRing::~LLPacketRing()
{
	cleanup();
}

void LLPacketRing::cleanup()
{
	while (!mReceiveQueue.empty())
	{
		LLPacketBuffer* packetp = mReceiveQueue.front();
		delete packetp;
		mReceiveQueue.pop();
	}

	while (!mSendQueue.empty())
	{
		LLPacketBuffer* packetp = mSendQueue.front();
		delete packetp;
		mSendQueue.pop();
	}
}

S32 LLPacketRing::receiveFromRing(S32 socket, char* datap)
{
	if (mInThrottle.checkOverflow(0))
	{
		// We do not have enough bandwidth, do not give them a packet.
		return 0;
	}

	LLPacketBuffer* packetp = NULL;
	if (mReceiveQueue.empty())
	{
		// No packets on the queue, do not give them any.
		return 0;
	}

	S32 packet_size = 0;
	packetp = mReceiveQueue.front();
	mReceiveQueue.pop();
	packet_size = packetp->getSize();
	if (packetp->getData() != NULL)
	{
		memcpy(datap, packetp->getData(), packet_size);
	}
	// need to set sender IP/port!!
	mLastSender = packetp->getHost();
	mLastReceivingIF = packetp->getReceivingInterface();
	delete packetp;

	this->mInBufferLength -= packet_size;

	// Adjust the throttle
	mInThrottle.throttleOverflow(packet_size * 8.f);
	return packet_size;
}

S32 LLPacketRing::receivePacket(S32 socket, char* datap)
{
	S32 packet_size = 0;

	// If using the throttle, simulate a limited size input buffer.
	if (mUseInThrottle)
	{
		bool done = false;

		// Push any current net packet (if any) onto delay ring
		while (!done)
		{
			LLPacketBuffer* packetp = new LLPacketBuffer(socket);
			if (packetp && packetp->getSize())
			{
				mActualBitsIn += packetp->getSize() * 8;
			}

			// If we faked packet loss, then we do not have a packet to use for
			// buffer overflow testing (no packetp == faked packet loss).
			if (packetp)
			{
				if (mInBufferLength + packetp->getSize() > mMaxBufferLength)
				{
					// Toss it.
					llwarns << "Throwing away packet, overflowing buffer"
							<< llendl;
					delete packetp;
					packetp = NULL;
				}
				else if (packetp->getSize())
				{
					mReceiveQueue.push(packetp);
					mInBufferLength += packetp->getSize();
				}
				else
				{
					delete packetp;
					packetp = NULL;
					done = true;
				}
			}
		}

		// Now, grab data off of the receive queue according to our throttled
		// bandwidth settings.
		packet_size = receiveFromRing(socket, datap);
	}
	else
	{
		// No delay, pull straight from net
		if (LLProxy::isSOCKSProxyEnabled())
		{
			U8 buffer[NET_BUFFER_SIZE + SOCKS_HEADER_SIZE];
			packet_size = receive_packet(socket, (char*)((void*)buffer));
			if (packet_size > SOCKS_HEADER_SIZE)
			{
				// *FIX: we are assuming ATYP is 0x01 (IPv4), not 0x03
				// (hostname) or 0x04 (IPv6)
				memcpy(datap, buffer + SOCKS_HEADER_SIZE,
					   packet_size - SOCKS_HEADER_SIZE);
				proxywrap_t* header = (proxywrap_t*)((void*)buffer);
				mLastSender.setAddress(header->addr);
				mLastSender.setPort(ntohs(header->port));

				packet_size -= SOCKS_HEADER_SIZE; // The unwrapped packet size
			}
			else
			{
				packet_size = 0;
			}
		}
		else
		{
			packet_size = receive_packet(socket, datap);
			mLastSender = get_sender();
		}

		mLastReceivingIF = get_receiving_interface();
	}

	return packet_size;
}

bool LLPacketRing::sendPacket(int h_socket, char* send_buffer, S32 buf_size,
							  LLHost host)
{
	bool status = true;
	if (!mUseOutThrottle)
	{
		return sendPacketImpl(h_socket, send_buffer, buf_size, host);
	}
	else
	{
		mActualBitsOut += buf_size * 8;
		LLPacketBuffer* packetp = NULL;
		// See if we have got enough throttle to send a packet.
		while (!mOutThrottle.checkOverflow(0.f))
		{
			// While we have enough bandwidth, send a packet from the queue or
			// the current packet

			S32 packet_size = 0;
			if (!mSendQueue.empty())
			{
				// Send a packet off of the queue
				LLPacketBuffer* packetp = mSendQueue.front();
				mSendQueue.pop();

				mOutBufferLength -= packetp->getSize();
				packet_size = packetp->getSize();

				status = sendPacketImpl(h_socket, packetp->getData(),
										packet_size, packetp->getHost());

				delete packetp;
				// Update the throttle
				mOutThrottle.throttleOverflow(packet_size * 8.f);
			}
			else
			{
				// If the queue is empty, we can just send this packet right
				// away.
				status = sendPacketImpl(h_socket, send_buffer, buf_size, host);
				packet_size = buf_size;

				// Update the throttle
				mOutThrottle.throttleOverflow(packet_size * 8.f);

				// This was the packet we are sending now, there are no other
				// packets that we need to send
				return status;
			}
		}

		// We have not sent the incoming packet, add it to the queue
		if (mOutBufferLength + buf_size > mMaxBufferLength)
		{
			// Nuke this packet, we overflowed the buffer. Toss it.
			llwarns << "Throwing away outbound packet, overflowing buffer"
					<< llendl;
		}
		else
		{
			static LLTimer queue_timer;
			if (mOutBufferLength > 4192 &&
				queue_timer.getElapsedTimeF32() > 1.f)
			{
				// Add it to the queue
				llinfos << "Outbound packet queue " << mOutBufferLength
						<< " bytes" << llendl;
				queue_timer.reset();
			}
			packetp = new LLPacketBuffer(host, send_buffer, buf_size);

			mOutBufferLength += packetp->getSize();
			mSendQueue.push(packetp);
		}
	}

	return status;
}

bool LLPacketRing::sendPacketImpl(int h_socket, const char* send_buffer,
								  S32 buf_size, LLHost host)
{
	if (!LLProxy::isSOCKSProxyEnabled())
	{
		return send_packet(h_socket, send_buffer, buf_size, host.getAddress(),
						   host.getPort());
	}

	char headered_send_buffer[NET_BUFFER_SIZE + SOCKS_HEADER_SIZE];

	proxywrap_t* socks_header = (proxywrap_t*)((void*)&headered_send_buffer);
	socks_header->rsv = 0;
	socks_header->addr = host.getAddress();
	socks_header->port = htons(host.getPort());
	socks_header->atype = ADDRESS_IPV4;
	socks_header->frag = 0;

	memcpy(headered_send_buffer + SOCKS_HEADER_SIZE, send_buffer, buf_size);

	LLHost proxyhost = LLProxy::getInstance()->getUDPProxy();
	return send_packet(h_socket, headered_send_buffer,
					   buf_size + SOCKS_HEADER_SIZE,
					   proxyhost.getAddress(), proxyhost.getPort());
}
