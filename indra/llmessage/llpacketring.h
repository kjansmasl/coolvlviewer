/**
 * @file llpacketring.h
 * @brief definition of LLPacketRing class for implementing a resend,
 * drop, or delay in packet transmissions
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

#ifndef LL_LLPACKETRING_H
#define LL_LLPACKETRING_H

#include <queue>

#include "llhost.h"
#include "llthrottle.h"

class LLPacketBuffer;

class LLPacketRing
{
protected:
	LOG_CLASS(LLPacketRing);

public:
	LLPacketRing();
    ~LLPacketRing();

	void cleanup();

	LL_INLINE void setUseInThrottle(bool b)			{ mUseInThrottle = b; }
	LL_INLINE void setUseOutThrottle(bool b)		{ mUseOutThrottle = b; }
	LL_INLINE void setInBandwidth(F32 bps)			{ mInThrottle.setRate(bps); }
	LL_INLINE void setOutBandwidth(F32 bps)			{ mOutThrottle.setRate(bps); }

	S32 receivePacket(S32 socket, char* datap);
	S32 receiveFromRing(S32 socket, char* datap);

	bool sendPacket(int h_socket, char* send_buffer, S32 buf_size, LLHost host);

	LL_INLINE LLHost getLastSender()				{ return mLastSender; }
	LL_INLINE LLHost getLastReceivingInterface()	{ return mLastReceivingIF; }

	LL_INLINE S32 getAndResetActualInBits()
	{
		S32 bits = mActualBitsIn;
		mActualBitsIn = 0;
		return bits;
	}

	LL_INLINE S32 getAndResetActualOutBits()
	{
		S32 bits = mActualBitsOut;
		mActualBitsOut = 0;
		return bits;
	}

private:
	bool sendPacketImpl(int h_socket, const char* send_buffer, S32 buf_size,
						LLHost host);

protected:
	typedef std::queue<LLPacketBuffer*> packet_queue_t;
	packet_queue_t	mReceiveQueue;
	packet_queue_t	mSendQueue;

	LLHost			mLastSender;
	LLHost			mLastReceivingIF;

	// For simulating a lower-bandwidth connection - BPS
	LLThrottle		mInThrottle;
	LLThrottle		mOutThrottle;

	S32				mActualBitsIn;
	S32				mActualBitsOut;
	// How much data can we queue up before dropping data.
	S32				mMaxBufferLength;
	S32				mInBufferLength;	// Current incoming buffer length
	S32				mOutBufferLength;	// Current outgoing buffer length

	bool			mUseInThrottle;
	bool			mUseOutThrottle;
};

#endif
