/**
 * @file llxfer.cpp
 * @brief implementation of LLXfer class for a single xfer.
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

#include "llxfer.h"

#include "llextendedstatus.h"
#include "llmath.h"

// Number of bytes sent in each message
constexpr U32 LL_XFER_CHUNK_SIZE = 1000;

LLXfer::LLXfer(S32 chunk_size)
{
	init(chunk_size);
}

LLXfer::~LLXfer()
{
	cleanup();
}

void LLXfer::init(S32 chunk_size)
{
	mID = 0;

	mPacketNum = -1;	// There is a pre-increment before sending packet 0

	mXferSize = 0;

	mStatus = e_LL_XFER_UNINITIALIZED;
	mWaitingForACK = false;

	mCallback = NULL;
	mCallbackDataHandle = NULL;
	mCallbackResult = 0;

	mBufferContainsEOF = false;
	mBuffer = NULL;
	mBufferLength = 0;
	mBufferStartOffset = 0;

	mRetries = 0;

	if (chunk_size < 1)
	{
		chunk_size = LL_XFER_CHUNK_SIZE;
	}
	mChunkSize = chunk_size;
}

void LLXfer::cleanup()
{
	if (mBuffer)
	{
		delete[] mBuffer;
		mBuffer = NULL;
	}
}

S32 LLXfer::startSend(U64 xfer_id, const LLHost& remote_host)
{
	llwarns << "Default, no-operation version called for " << getFileName()
			<< llendl;
	return -1;
}

void LLXfer::closeFileHandle()
{
	llwarns << "Default, no-operation version called for " << getFileName()
			<< llendl;
}

S32 LLXfer::reopenFileHandle()
{
	llwarns << "Default, no-operation version called for " << getFileName()
			<< llendl;
	return -1;
}

void LLXfer::setXferSize(S32 xfer_size)
{
	mXferSize = xfer_size;
}

S32 LLXfer::startDownload()
{
	llwarns << "Default, no-operation version called for " << getFileName()
			<< llendl;
	return -1;
}

S32 LLXfer::receiveData(char* datap, S32 data_size)
{
	S32 retval = 0;

	if ((S32)mBufferLength + data_size > getMaxBufferSize())
	{
		// Write existing data to disk if it is larger than the buffer size
		retval = flush();
	}

	if (!retval)
	{
		if (datap != NULL)
		{
			// Append new data to mBuffer
			memcpy(&mBuffer[mBufferLength], datap, data_size);
			mBufferLength += data_size;
		}
		else
		{
			llerrs << "NULL data passed in receiveData" << llendl;
		}
	}

	return (retval);
}

S32 LLXfer::flush()
{
	// Only files have somewhere to flush to if we get called with a flush it
	// means we've blown past our allocated buffer size
	return -1;
}

S32 LLXfer::suck(S32 start_position)
{
	llwarns << "Attempted to send a packet outside the buffer bounds"
			<< llendl;
	return -1;
}

void LLXfer::sendPacket(S32 packet_num)
{
	char fdata_buf[LL_XFER_LARGE_PAYLOAD + 4];
	S32 fdata_size = mChunkSize;
	bool last_packet = false;
	S32 num_copy = 0;

	// If the desired packet is not in our current buffered excerpt from the
	// file
	if ((U32)packet_num * fdata_size < mBufferStartOffset ||
		llmin((U32)mXferSize, (U32)((packet_num + 1) * fdata_size)) >
			mBufferStartOffset + mBufferLength)

	{
		if (suck(packet_num * fdata_size))  // returns non-zero on failure
		{
			abort(LL_ERR_EOF);
			return;
		}
	}

	S32 desired_read_position = packet_num * fdata_size - mBufferStartOffset;

	fdata_size = llmin((S32)mBufferLength - desired_read_position, mChunkSize);
	if (fdata_size < 0)
	{
		llwarns << "Negative data size in transfer send: aborting." << llendl;
		abort(LL_ERR_EOF);
		return;
	}

	if (mBufferContainsEOF &&
		(U32)(desired_read_position + fdata_size) >= (U32)mBufferLength)
	{
		last_packet = true;
	}

	if (packet_num)
	{
		num_copy = llmin(fdata_size, (S32)sizeof(fdata_buf));
		num_copy = llmin(num_copy,
						 (S32)(mBufferLength - desired_read_position));
		if (num_copy > 0)
		{
			memcpy(fdata_buf, &mBuffer[desired_read_position], num_copy);
		}
	}
	else
	{
		// If we are the first packet, encode size as an additional S32 at
		// start of data.
		num_copy = llmin(fdata_size, (S32)(sizeof(fdata_buf) - sizeof(S32)));
		num_copy = llmin(num_copy,
						 (S32)(mBufferLength - desired_read_position));
		if (num_copy > 0)
		{
			memcpy(fdata_buf + sizeof(S32), &mBuffer[desired_read_position],
				   num_copy);
		}
		fdata_size += sizeof(S32);
		htonmemcpy(fdata_buf, &mXferSize, MVT_S32, sizeof(S32));
	}

	S32 encoded_packetnum = encodePacketNum(packet_num, last_packet);

	if (fdata_size)
	{
		// Send the packet
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_SendXferPacket);
		msg->nextBlockFast(_PREHASH_XferID);

		msg->addU64Fast(_PREHASH_ID, mID);
		msg->addU32Fast(_PREHASH_Packet, encoded_packetnum);

		msg->nextBlockFast(_PREHASH_DataPacket);
		msg->addBinaryDataFast(_PREHASH_Data, &fdata_buf, fdata_size);

		S32 bytes_sent = msg->sendMessage(mRemoteHost);
		if (!bytes_sent)
		{
			abort(LL_ERR_CIRCUIT_GONE);
			return;
		}

		ACKTimer.reset();
		mWaitingForACK = true;
	}

	if (last_packet)
	{
		mStatus = e_LL_XFER_COMPLETE;
	}
	else
	{
		mStatus = e_LL_XFER_IN_PROGRESS;
	}
}

void LLXfer::sendNextPacket()
{
	mRetries = 0;
	sendPacket(++mPacketNum);
}

void LLXfer::resendLastPacket()
{
	++mRetries;
	sendPacket(mPacketNum);
}

S32 LLXfer::processEOF()
{
	S32 retval = 0;

	mStatus = e_LL_XFER_COMPLETE;

	if (mCallbackResult == LL_ERR_NOERR)
	{
		llinfos << "Transfer from " << mRemoteHost << " complete: "
				<< getFileName() << llendl;
	}
	else
	{
		llinfos << "Transfer from " << mRemoteHost
				<< " failed or aborted with error code " << mCallbackResult
				<< ": " << getFileName() << llendl;
	}

	if (mCallback)
	{
		mCallback(mCallbackDataHandle, mCallbackResult, LLExtStat::NONE);
	}

	return retval;
}

S32 LLXfer::encodePacketNum(S32 packet_num, bool is_eof)
{
	return is_eof ? (packet_num | 0x80000000) : packet_num;
}

void LLXfer::abort (S32 result_code)
{
	mCallbackResult = result_code;

	llinfos << "Aborting transfer from: " << mRemoteHost << " - named: "
			<< getFileName() << " - error: " << result_code << llendl;

	if (result_code != LL_ERR_CIRCUIT_GONE)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_AbortXfer);
		msg->nextBlockFast(_PREHASH_XferID);
		msg->addU64Fast(_PREHASH_ID, mID);
		msg->addS32Fast(_PREHASH_Result, result_code);
		msg->sendMessage(mRemoteHost);
	}

	mStatus = e_LL_XFER_ABORTED;
}

std::string LLXfer::getFileName()
{
	return U64_to_str(mID);
}

U32 LLXfer::getXferTypeTag()
{
	return 0;
}

S32 LLXfer::getMaxBufferSize()
{
	return mXferSize;
}

std::ostream& operator<<(std::ostream& os, LLXfer& hh)
{
	os << hh.getFileName();
	return os;
}
