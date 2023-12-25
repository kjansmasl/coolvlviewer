/**
 * @file llmessage.cpp
 * @brief LLMessageSystem class implementation
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

#include <algorithm>

#include "llmessage.h"

#if !LL_WINDOWS
// Following header files required for inet_addr()
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif
#include <iomanip>
#include <iterator>
#include <sstream>

#include "apr_portable.h"
#include "apr_network_io.h"
#include "apr_poll.h"

#include "indra_constants.h"
#include "llapr.h"
#include "llcorehttplibcurl.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llmessageconfig.h"
#include "llmessagetemplate.h"
#include "llmessagetemplateparser.h"
#include "llnet.h"						// For start_net(), end_net()
#include "llpumpio.h"
#include "llsd.h"
#include "llsdmessagebuilder.h"
#include "llsdmessagereader.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "lltemplatemessagebuilder.h"
#include "lltemplatemessagereader.h"
#include "lltimer.h"
#include "lltransfermanager.h"
#include "lltransfertargetvfile.h"
#include "llquaternion.h"
#include "llvector3.h"
#include "llvector3d.h"
#include "llvector4.h"
#include "llxfermanager.h"

constexpr F32 CIRCUIT_DUMP_TIMEOUT = 30.f;

void swizzle_size_error(size_t n, size_t s)
{
	llerrs << "Size argument passed (" << n
		   << ")to htonmemcpy does not match swizzle type size: "
		   << s << llendl;
}

class LLMessagePollInfo
{
public:
	apr_socket_t*	mAPRSocketp;
	apr_pollfd_t	mPollFD;
};

class LLMessageHandlerBridge : public LLHTTPNode
{
	bool validate(const std::string& name, LLSD& context) const override
	{
		return true;
	}

	void post(LLHTTPNode::ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override;
};

//virtual
void LLMessageHandlerBridge::post(LLHTTPNode::ResponsePtr response,
								  const LLSD& context, const LLSD& input) const
{
	std::string name =
		context[CONTEXT_REQUEST][CONTEXT_WILDCARD]["message-name"];
	char* namep = gMessageStringTable.getString(name.c_str());

	LLMessageSystem* msg = gMessageSystemp;
	msg->mLastSender = LLHost(input["sender"].asString());
	msg->mPacketsIn += 1;
	msg->mLLSDMessageReader->setMessage(namep, input["body"]);
#if LL_USE_FIBER_AWARE_MUTEX
	LockMessageReader rdr(msg->mMessageReader, msg->mLLSDMessageReader);
#else
	msg->mMessageReader = msg->mLLSDMessageReader;
#endif

	if (msg->callHandler(namep))
	{
		response->result(LLSD());
	}
	else
	{
		response->notFound();
	}
}

LLHTTPRegistration<LLMessageHandlerBridge>
	gHTTPRegistrationMessageWildcard("/message/<message-name>");

#if LL_USE_FIBER_AWARE_MUTEX
// For the lifespan of this LockMessageChecker instance, we use
// LLTemplateMessageReader as msgsystem's mMessageReader
LockMessageChecker::LockMessageChecker(LLMessageSystem* msgsystem)
:	LockMessageReader(msgsystem->mMessageReader,
					  msgsystem->mTemplateMessageReader),
	mMessageSystem(msgsystem)
{
}
#endif

static const char* nullToEmpty(const char* s)
{
	static const char empty_string[] = "";
	return s ? s : empty_string;
}

void LLMessageSystem::init()
{
	// Initialize member variables
	mVerboseLog = false;

	mError = false;
	mErrorCode = 0;
	mSendReliable = false;

	mUnackedListDepth = 0;
	mUnackedListSize = 0;
	mDSMaxListDepth = 0;

	mNumberHighFreqMessages = 0;
	mNumberMediumFreqMessages = 0;
	mNumberLowFreqMessages = 0;
	mPacketsIn = mPacketsOut = 0;
	mBytesIn = mBytesOut = 0;
	mCompressedPacketsIn = mCompressedPacketsOut = 0;
	mReliablePacketsIn = mReliablePacketsOut = 0;

	mCompressedBytesIn = mCompressedBytesOut = 0;
	mUncompressedBytesIn = mUncompressedBytesOut = 0;
	mTotalBytesIn = mTotalBytesOut = 0;

    mDroppedPackets = 0;            // Total dropped packets in
    mResentPackets = 0;             // Total resent packets out
    mFailedResendPackets = 0;       // Total resend failure packets out
    mOffCircuitPackets = 0;         // Total # of off-circuit packets rejected
    mInvalidOnCircuitPackets = 0;   // Total # of on-circuit packets rejected

	mOurCircuitCode = 0;

	mIncomingCompressedSize = 0;
	mCurrentRecvPacketID = 0;

	mMessageFileVersionNumber = 0.f;

	mTimingCallback = NULL;
	mTimingCallbackData = NULL;

	mMessageBuilder = NULL;
#if LL_USE_FIBER_AWARE_MUTEX
	LockMessageReader(mMessageReader, NULL);
#else
	mMessageReader = NULL;
#endif

	if (strcmp(_PREHASH_AgentID, "AgentID"))
	{
		llerrs << "Message prehash table not properly initialized !" << llendl;
	}
}

void LLMessageSystem::setHttpOptionsWithTimeout(U32 timeout)
{
	mHttpOptions->setRetries(0);
	mHttpOptions->setTimeout(timeout);
	mHttpOptions->setTransferTimeout(timeout);
}

// Read file and build message templates
LLMessageSystem::LLMessageSystem(const std::string& filename, U32 port,
								 S32 version_major, S32 version_minor,
								 S32 version_patch, F32 heartbeat_interval,
								 F32 circuit_timeout)
:	mCircuitInfo(heartbeat_interval, circuit_timeout),
	mPort(port),
	mSocket(0),
	mSystemVersionMajor(version_major),
	mSystemVersionMinor(version_minor),
	mSystemVersionPatch(version_patch),
	mSystemVersionServer(0),
	mVersionFlags(0x0),
	mMessageBuilder(NULL),
#if !LL_USE_FIBER_AWARE_MUTEX
	mMessageReader(NULL),
#endif
	mSendSize(0),
	mHttpOptions(new LLCore::HttpOptions),
	// Default to not accepting packets from not alive circuits
	mProtected(true),
	mCircuitPrintFreq(60.f),	// In seconds
	// Constants for dumping output based on message processing time/count
	mNumMessageCounts(0),
	mMaxMessageCounts(200), 	// >= 0 means dump warnings
	mMaxMessageTime(1.f),
	// Statistics
	mTrueReceiveSize(0),
	mReceiveTime(0.f),
	mSendPacketFailureCount(0)
{
	init();

	loadTemplateFile(filename);

	mTemplateMessageBuilder = new LLTemplateMessageBuilder(mMessageTemplates);
	mLLSDMessageBuilder = new LLSDMessageBuilder();
	mTemplateMessageReader = new LLTemplateMessageReader(mMessageNumbers);
	mLLSDMessageReader = new LLSDMessageReader();

	// Initialize various bits of net info
	S32 error = start_net(mSocket, mPort);
	if (error != 0)
	{
		mError = true;
		mErrorCode = error;
	}

	//
	// Create the data structure that we can poll on
	//
	if (!gAPRPoolp)
	{
		llerrs << "No APR pool before message system initialization !"
			   << llendl;
	}
	apr_socket_t* apr_socketp = NULL;
	apr_os_sock_put(&apr_socketp, (apr_os_sock_t*)&mSocket, gAPRPoolp);

	mPollInfop = new LLMessagePollInfo;
	mPollInfop->mAPRSocketp = apr_socketp;
	mPollInfop->mPollFD.p = gAPRPoolp;
	mPollInfop->mPollFD.desc_type = APR_POLL_SOCKET;
	mPollInfop->mPollFD.reqevents = APR_POLLIN;
	mPollInfop->mPollFD.rtnevents = 0;
	mPollInfop->mPollFD.desc.s = apr_socketp;
	mPollInfop->mPollFD.client_data = NULL;

	F64 mt_sec = getMessageTimeSeconds();
	mResendDumpTime = mt_sec;
	mMessageCountTime = mt_sec;
	mCircuitPrintTime = mt_sec;
	mCurrentMessageTimeSeconds = mt_sec;
}

// Read file and build message templates
void LLMessageSystem::loadTemplateFile(const std::string& filename)
{
	if (filename.empty())
	{
		llerrs << "No template filename specified" << llendl;
#if 0	// Enable if we ever change the above for llwarns
		mError = true;
		return;
#endif
	}

	std::string template_body;
	if (!_read_file_into_string(template_body, filename))
	{
		llerrs << "Failed to open template: " << filename << llendl;
#if 0	// Enable if we ever change the above for llwarns
		mError = true;
		return;
#endif
	}

	LLTemplateTokenizer tokens(template_body);
	LLTemplateParser parsed(tokens);
	mMessageFileVersionNumber = parsed.getVersion();
	for (LLTemplateParser::message_iterator iter = parsed.getMessagesBegin();
		iter != parsed.getMessagesEnd(); iter++)
	{
		addTemplate(*iter);
	}
}

LLMessageSystem::~LLMessageSystem()
{
	mMessageTemplates.clear(); // Do not delete templates.

	for (template_number_map_t::iterator it = mMessageNumbers.begin(),
										 end = mMessageNumbers.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mMessageNumbers.clear();

	if (!mError)
	{
		end_net(mSocket);
	}
	mSocket = 0;

	delete mTemplateMessageReader;
	mTemplateMessageReader = NULL;

#if !LL_USE_FIBER_AWARE_MUTEX
	mMessageReader = NULL;
#endif

	delete mTemplateMessageBuilder;
	mTemplateMessageBuilder = NULL;
	mMessageBuilder = NULL;

	delete mLLSDMessageReader;
	mLLSDMessageReader = NULL;

	delete mLLSDMessageBuilder;
	mLLSDMessageBuilder = NULL;

	delete mPollInfop;
	mPollInfop = NULL;

	mIncomingCompressedSize = 0;
	mCurrentRecvPacketID = 0;
}

void LLMessageSystem::clearReceiveState()
{
	mCurrentRecvPacketID = 0;
	mIncomingCompressedSize = 0;
	mLastSender.invalidate();
	mLastReceivingIF.invalidate();
	mMessageReader->clearMessage();
}

bool LLMessageSystem::poll(F32 seconds)
{
	S32 num_socks;
	apr_status_t status = apr_poll(&(mPollInfop->mPollFD), 1, &num_socks,
								   (U64)(seconds * 1000000.f));
	if (status != APR_TIMEUP)
	{
		ll_apr_warn_status(status);
	}
	return num_socks != 0;
}

LLCircuitData* LLMessageSystem::findCircuit(const LLHost& host,
											bool reset_packet_id)
{
	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (!cdp)	// if this packet comes from a circuit we do not know about
	{
		// Are we rejecting off-circuit packets ?
		if (!mProtected)
		{
			// Nope, open the new circuit
			cdp = mCircuitInfo.addCircuitData(host, mCurrentRecvPacketID);
			if (reset_packet_id)
			{
				// I added this - I think it's correct - DJS
				// reset packet in Id
				cdp->setPacketInID(mCurrentRecvPacketID);
			}
			// And claim the packet is on the circuit we just added.
		}
	}
	// This is an old circuit... Is it still alive ?
	else if (!cdp->isAlive())
	{
		// Nope. Do not accept if weare protected
		if (mProtected)
		{
			// Do not accept packets from unexpected sources
			cdp = NULL;
		}
		else
		{
			// Wake up the circuit
			cdp->setAlive(true);
			if (reset_packet_id)
			{
				// Reset packet in Id
				cdp->setPacketInID(mCurrentRecvPacketID);
			}
		}
	}
	return cdp;
}

// Returns true if a valid, on-circuit message has been received.
#if LL_USE_FIBER_AWARE_MUTEX
bool LLMessageSystem::checkMessages(LockMessageChecker&, S64 frame_count)
#else
bool LLMessageSystem::checkMessages(S64 frame_count)
#endif
{
	// Pump
	bool valid_packet = false;

#if !LL_USE_FIBER_AWARE_MUTEX
	mMessageReader = mTemplateMessageReader;
#endif

	LLTransferTargetVFile::updateQueue();

	if (!mNumMessageCounts)
	{
		// This is the first message being handled after a resetReceiveCounts,
		// we must be starting the message processing loop.  Reset the timers.
		mCurrentMessageTimeSeconds = LLTimer::totalTime() * SEC_PER_USEC;
		mMessageCountTime = getMessageTimeSeconds();
	}

	// Loop until either no packets or a valid packet, i.e. burn through
	// packets from unregistered circuits
	S32 receive_size = 0;
	do
	{
		clearReceiveState();

		bool recv_reliable = false;
		bool recv_resent = false;
		S32 acks = 0;
		S32 true_rcv_size = 0;

		U8* buffer = mTrueReceiveBuffer;

		mTrueReceiveSize =
			mPacketRing.receivePacket(mSocket, (char*)mTrueReceiveBuffer);

#if 0	// Enable this if you want to dump all received packets into the log
		dumpPacketToLog();
#endif

		receive_size = mTrueReceiveSize;
		mLastSender = mPacketRing.getLastSender();
		mLastReceivingIF = mPacketRing.getLastReceivingInterface();

		if (receive_size < (S32)LL_MINIMUM_VALID_PACKET_SIZE)
		{
			// A receive size of zero is OK, that means that there are no more
			// packets available. Ones that are non-zero but below the minimum
			// packet size are worrisome.
			if (receive_size > 0)
			{
				llwarns << "Invalid (too short) packet discarded "
						<< receive_size << llendl;
				callExceptionFunc(MX_PACKET_TOO_SHORT);
			}
			// No data in packet receive buffer
			valid_packet = false;
		}
		else
		{
			LLHost host;
			LLCircuitData* cdp;

			// Note if packet acks are appended.
			if (buffer[0] & LL_ACK_FLAG)
			{
				acks += buffer[--receive_size];
				true_rcv_size = receive_size;
				if (receive_size >= ((S32)(acks * sizeof(TPACKETID) +
										   LL_MINIMUM_VALID_PACKET_SIZE)))
				{
					receive_size -= acks * sizeof(TPACKETID);
				}
				else
				{
					// Malformed packet; ignore it and continue with next one.
					llwarns << "Malformed packet received. Packet size "
							<< receive_size << " with invalid no. of acks "
							<< acks << llendl;
					valid_packet = false;
					continue;
				}
			}

			// Process the message as normal
			mIncomingCompressedSize = zeroCodeExpand(&buffer, &receive_size);
			mCurrentRecvPacketID = ntohl(*((U32*)(&buffer[1])));
			host = getSender();

			constexpr bool reset_packet_id = true;
			cdp = findCircuit(host, reset_packet_id);

			// At this point, cdp is now a pointer to the circuit that this
			// message came in on if it's valid, and NULL if the circuit was
			// bogus.

			if (cdp && acks > 0 &&
				acks * (S32)sizeof(TPACKETID) < true_rcv_size)
			{
				TPACKETID packet_id;
				U32 mem_id = 0;
				for (S32 i = 0; i < acks; ++i)
				{
					true_rcv_size -= sizeof(TPACKETID);
					memcpy(&mem_id, &mTrueReceiveBuffer[true_rcv_size],
						   sizeof(TPACKETID));
					packet_id = ntohl(mem_id);
					cdp->ackReliablePacket(packet_id);
				}
				if (!cdp->getUnackedPacketCount())
				{
					// Remove this circuit from the list of circuits with
					// unacked packets
					mCircuitInfo.mUnackedCircuitMap.erase(cdp->mHost);
				}
			}

			if (buffer[0] & LL_RELIABLE_FLAG)
			{
				recv_reliable = true;
			}
			if (buffer[0] & LL_RESENT_FLAG)
			{
				recv_resent = true;
				if (cdp && cdp->isDuplicateResend(mCurrentRecvPacketID))
				{
					// We need to ACK here to suppress further resends of
					// packets we have already seen.
					if (recv_reliable)
					{
						cdp->collectRAck(mCurrentRecvPacketID);
					}

					LL_DEBUGS("Messaging") << "Discarding duplicate resend from "
										   << host << LL_ENDL;
					if (mVerboseLog)
					{
						std::ostringstream str;
						str << "MSG: <- " << host;
						std::string tbuf =
							llformat("\t%6d\t%6d\t%6d ", receive_size,
									 (mIncomingCompressedSize ? mIncomingCompressedSize
															  : receive_size),
									 mCurrentRecvPacketID);
						str << tbuf << "(unknown)"
							<< (recv_reliable ? " reliable" : "")
							<< " resent "
							<< ((acks > 0) ? "acks" : "")
							<< " DISCARD DUPLICATE";
						llinfos << str.str() << llendl;
					}
					++mPacketsIn;
					valid_packet = false;
					continue;
				}
			}

			// UseCircuitCode can be a valid, off-circuit packet. But we do not
			// want to acknowledge UseCircuitCode until the circuit is
			// available, which is why the acknowledgement test is done above.
			// JC
			bool trusted = cdp && cdp->getTrusted();
			valid_packet =
				mTemplateMessageReader->validateMessage(buffer, receive_size,
														host, trusted);
			if (!valid_packet)
			{
				clearReceiveState();
			}

			// UseCircuitCode is allowed in even from an invalid circuit, so
			// that we can toss circuits around.
			if (valid_packet && !cdp &&
				mTemplateMessageReader->getMessageName() != _PREHASH_UseCircuitCode)
			{
				logMsgFromInvalidCircuit(host, recv_reliable);
				clearReceiveState();
				valid_packet = false;
			}

			if (valid_packet && cdp && !cdp->getTrusted() &&
				mTemplateMessageReader->isTrusted())
			{
				logTrustedMsgFromUntrustedCircuit(host);
				clearReceiveState();

				sendDenyTrustedCircuit(host);
				valid_packet = false;
			}

			if (valid_packet)
			{
				logValidMsg(cdp, host, recv_reliable, recv_resent, acks > 0);
				valid_packet = mTemplateMessageReader->readMessage(buffer,
																   host);
			}

			// It is possible that the circuit went away, because ANY message
			// can disable the circuit (for example, UseCircuit, CloseCircuit,
			// DisableSimulator). Find it again.
			cdp = mCircuitInfo.findCircuit(host);

			if (valid_packet)
			{
				++mPacketsIn;
				mBytesIn += mTrueReceiveSize;

				// ACK here for	valid packets that we have seen for the first
				// time
				if (cdp && recv_reliable)
				{
					// Add to the recently received list for duplicate
					// suppression
					cdp->mRecentlyReceivedReliablePackets[mCurrentRecvPacketID] =
						getMessageTimeUsecs();

					// Put it onto the list of packets to be acked
					cdp->collectRAck(mCurrentRecvPacketID);
					++mReliablePacketsIn;
				}
			}
			else if (mProtected  && !cdp)
			{
				llwarns << "Invalid Packet from invalid circuit " << host
						<< llendl;
				++mOffCircuitPackets;
			}
			else
			{
				++mInvalidOnCircuitPackets;
			}
		}
	}
	while (!valid_packet && receive_size > 0);

	F64 mt_sec = getMessageTimeSeconds();
	// Check to see if we need to print debug info
	if (mt_sec - mCircuitPrintTime > mCircuitPrintFreq)
	{
		LL_DEBUGS("CircuitInfo") << mCircuitInfo << LL_ENDL;
		mCircuitPrintTime = mt_sec;
	}

	if (!valid_packet)
	{
		clearReceiveState();
	}

	return valid_packet;
}

S32	LLMessageSystem::getReceiveBytes() const
{
	if (mIncomingCompressedSize)
	{
		return mIncomingCompressedSize * 8;
	}
	return getReceiveSize() * 8;
}

#if LL_USE_FIBER_AWARE_MUTEX
void LLMessageSystem::processAcks(LockMessageChecker&, F32 collect_time)
#else
void LLMessageSystem::processAcks(F32 collect_time)
#endif
{
	F64 mt_sec = getMessageTimeSeconds();
	{
		gTransferManager.updateTransfers();

		if (gXferManagerp)
		{
			gXferManagerp->retransmitUnackedPackets();
		}

		if (gAssetStoragep)
		{
			gAssetStoragep->checkForTimeouts();
		}
	}

	bool dump = false;

	// Check the status of circuits
	mCircuitInfo.updateWatchDogTimers(this);

	// Resend any necessary packets
	mCircuitInfo.resendUnackedPackets(mUnackedListDepth, mUnackedListSize);

	// Cycle through ack list for each host we need to send acks to
	mCircuitInfo.sendAcks(collect_time);

	if (!mDenyTrustedCircuitSet.empty())
	{
		llinfos << "Sending queued DenyTrustedCircuit messages." << llendl;
		for (host_set_t::iterator hostit = mDenyTrustedCircuitSet.begin();
			 hostit != mDenyTrustedCircuitSet.end(); ++hostit)
		{
			reallySendDenyTrustedCircuit(*hostit);
		}
		mDenyTrustedCircuitSet.clear();
	}

	if (mMaxMessageCounts >= 0 && mNumMessageCounts >= mMaxMessageCounts)
	{
		dump = true;
	}

	if (mMaxMessageTime >= 0.f)
	{
		// This is one of the only places where we are required to get REAL
		// message system time.
		mReceiveTime = (F32)(getMessageTimeSeconds(true) - mMessageCountTime);
		if (mReceiveTime > mMaxMessageTime)
		{
			dump = true;
		}
	}

	if (dump)
	{
		dumpReceiveCounts();
	}
	resetReceiveCounts();

	if (mt_sec - mResendDumpTime > CIRCUIT_DUMP_TIMEOUT)
	{
		mResendDumpTime = mt_sec;
		mCircuitInfo.dumpResends();
	}
}

void LLMessageSystem::copyMessageReceivedToSend()
{
	// NOTE: babbage: switch builder to match reader to avoid converting
	// message format
	if (mMessageReader == mTemplateMessageReader)
	{
		mMessageBuilder = mTemplateMessageBuilder;
	}
	else
	{
		mMessageBuilder = mLLSDMessageBuilder;
	}
	mSendReliable = false;
	mMessageBuilder->newMessage(mMessageReader->getMessageName());
	mMessageReader->copyToBuilder(*mMessageBuilder);
}

LLSD LLMessageSystem::getReceivedMessageLLSD() const
{
	LLSDMessageBuilder builder;
	mMessageReader->copyToBuilder(builder);
	return builder.getMessage();
}

LLSD LLMessageSystem::getBuiltMessageLLSD() const
{
	if (mLLSDMessageBuilder != mMessageBuilder)
	{
		// *TODO: implement as below ?
		llerrs << "Message not built as LLSD." << llendl;
	}
	return mLLSDMessageBuilder->getMessage();
}

LLSD LLMessageSystem::wrapReceivedTemplateData() const
{
	if (mMessageReader != mTemplateMessageReader)
	{
		return getReceivedMessageLLSD();
	}

	LLTemplateMessageBuilder builder(mMessageTemplates);
	builder.newMessage(mMessageReader->getMessageName());
	mMessageReader->copyToBuilder(builder);
	U8 buffer[MAX_BUFFER_SIZE];
	constexpr U8 offset_to_data = 0;
	U32 size = builder.buildMessage(buffer, MAX_BUFFER_SIZE,
									offset_to_data);
	LLSD::Binary binary_data(buffer, buffer + size);
	LLSD wrapped_data = LLSD::emptyMap();
	wrapped_data["binary-template-data"] = binary_data;
	return wrapped_data;
}

LLSD LLMessageSystem::wrapBuiltTemplateData() const
{
	if (mLLSDMessageBuilder == mMessageBuilder)
	{
		return getBuiltMessageLLSD();
	}

	U8 buffer[MAX_BUFFER_SIZE];
	constexpr U8 offset_to_data = 0;
	U32 size = mTemplateMessageBuilder->buildMessage(buffer, MAX_BUFFER_SIZE,
													 offset_to_data);
	LLSD::Binary binary_data(buffer, buffer + size);
	LLSD wrapped_data = LLSD::emptyMap();
	wrapped_data["binary-template-data"] = binary_data;
	return wrapped_data;
}

void LLMessageSystem::clearMessage()
{
	mSendReliable = false;
	mMessageBuilder->clearMessage();
}

// Sets block to add data to within current message
void LLMessageSystem::nextBlockFast(const char* blockname)
{
	mMessageBuilder->nextBlock(blockname);
}

void LLMessageSystem::nextBlock(const char* blockname)
{
	nextBlockFast(gMessageStringTable.getString(blockname));
}

bool LLMessageSystem::isSendFull(const char* blockname)
{
	char* str_table_name = NULL;
	if (blockname)
	{
		str_table_name = gMessageStringTable.getString(blockname);
	}
	return isSendFullFast(str_table_name);
}

bool LLMessageSystem::isSendFullFast(const char* blockname)
{
	return mMessageBuilder->isMessageFull(blockname);
}

// Blows away the last block of a message, returns false if that leaves no
// blocks or there was not a block to remove.
// *TODO: Babbage: Remove this horror.
bool LLMessageSystem::removeLastBlock()
{
	return mMessageBuilder->removeLastBlock();
}

S32 LLMessageSystem::sendReliable(const LLHost& host, U32 retries_factor)
{
	return sendReliable(host, LL_DEFAULT_RELIABLE_RETRIES * retries_factor,
						true, LL_PING_BASED_TIMEOUT_DUMMY, NULL, NULL);
}

S32 LLMessageSystem::sendSemiReliable(const LLHost& host,
									  void (*callback)(void**,S32),
									  void** callback_data)
{
	F32 timeout;
	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (cdp)
	{
		timeout = llmax(LL_MINIMUM_SEMIRELIABLE_TIMEOUT_SECONDS,
						LL_SEMIRELIABLE_TIMEOUT_FACTOR *
						cdp->getPingDelayAveraged());
	}
	else
	{
		timeout = LL_SEMIRELIABLE_TIMEOUT_FACTOR * LL_AVERAGED_PING_MAX;
	}

	// 0 retry and not ping-based timeout
	return sendReliable(host, 0, false, timeout, callback, callback_data);
}

// Sends the message via an UDP packet
S32 LLMessageSystem::sendReliable(const LLHost& host, S32 retries,
								  bool ping_based_timeout, F32 timeout,
								  void (*callback)(void**, S32),
								  void** callback_data)
{
	if (ping_based_timeout)
	{
	    LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	    if (cdp)
	    {
		    timeout = llmax(LL_MINIMUM_RELIABLE_TIMEOUT_SECONDS,
							LL_RELIABLE_TIMEOUT_FACTOR *
							cdp->getPingDelayAveraged());
	    }
	    else
	    {
		    timeout = llmax(LL_MINIMUM_RELIABLE_TIMEOUT_SECONDS,
							LL_RELIABLE_TIMEOUT_FACTOR * LL_AVERAGED_PING_MAX);
	    }
	}

	mSendReliable = true;
	mReliablePacketParams.set(host, retries, ping_based_timeout, timeout,
							  callback, callback_data,
							  const_cast<char*>(mMessageBuilder->getMessageName()));
	return sendMessage(host);
}

S32 LLMessageSystem::sendMessage(const LLHost& host)
{
	if (!mMessageBuilder->isBuilt())
	{
		mSendSize = mMessageBuilder->buildMessage(mSendBuffer, MAX_BUFFER_SIZE,
												  0);
	}

	// If port and ip are zero, do not bother trying to send the message
	if (!host.isOk())
	{
		return 0;
	}

	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (!cdp)
	{
		// This is a new circuit !  Are we protected ?
		if (mProtected)
		{
			// Yup !  Do not send packets to an unknown circuit
			if (mVerboseLog)
			{
				llinfos_once << "MSG: -> " << host << "\tUNKNOWN CIRCUIT:\t"
							 << mMessageBuilder->getMessageName() << llendl;
			}
			llwarns_once << "Trying to send "
						 << mMessageBuilder->getMessageName()
						 << " on unknown circuit " << host << llendl;
			return 0;
		}
		else
		{
			// Nope, open the new circuit
			cdp = mCircuitInfo.addCircuitData(host, 0);
		}
	}
	else if (!cdp->isAlive()) // This is an old circuit... Is it still alive ?
	{
		// Nope. Do not send to dead circuits
		if (mVerboseLog)
		{
			llinfos << "MSG: -> " << host << "\tDEAD CIRCUIT\t\t"
					<< mMessageBuilder->getMessageName() << llendl;
		}
		llwarns << "Trying to send message "
				<< mMessageBuilder->getMessageName() << " to dead circuit "
				<< host << llendl;
		return 0;
	}

	// NOTE: babbage: LLSD message -> HTTP, template message -> UDP
	if (mMessageBuilder == mLLSDMessageBuilder)
	{
		UntrustedCallback_t cb = NULL;
		if (mSendReliable && mReliablePacketParams.mCallback)
		{
			cb = boost::bind(mReliablePacketParams.mCallback,
							 mReliablePacketParams.mCallbackData, _1);
		}
		gCoros.launch("LLMessageSystem::sendUntrustedSimulatorMessageCoro",
					  boost::bind(&LLMessageSystem::sendUntrustedSimulatorMessageCoro,
								  this, host,
								  mLLSDMessageBuilder->getMessageName(),
								  mLLSDMessageBuilder->getMessage(), cb));
		mSendReliable = false;
		mReliablePacketParams.clear();
		return 1;
	}

	const char* msg_name = mMessageBuilder->getMessageName();
	if (msg_name != _PREHASH_PacketAck)
	{
		LL_DEBUGS("Messaging") << "Sending " << msg_name
							   << " to host " << host.getIPandPort()
							   << LL_ENDL;
	}

	// Zero out the flags and packetid. Subtract 1 here so that we do not
	// overwrite the offset if it was set set in buildMessage().
	memset(mSendBuffer, 0, LL_PACKET_ID_SIZE - 1);

	// Add the send id to the front of the message
	cdp->nextPacketOutID();

	// Packet ID size is always 4
	*((S32*)&mSendBuffer[PHL_PACKET_ID]) = htonl(cdp->getPacketOutID());

	// Compress the message, which will usually reduce its size.
	U8* buf_ptr = (U8*)mSendBuffer;
	U32 buffer_length = mSendSize;
	mMessageBuilder->compressMessage(buf_ptr, buffer_length);

	if (buffer_length > 1500)
	{
		if (msg_name != _PREHASH_ChildAgentUpdate &&
			msg_name != _PREHASH_SendXferPacket)
		{
			llwarns << "Trying to send "
					<< (buffer_length > 4000 ? "EXTRA " : "")
					<< "BIG message " << msg_name
					<< " - " << buffer_length << llendl;
		}
	}
	if (mSendReliable)
	{
		buf_ptr[0] |= LL_RELIABLE_FLAG;

		if (!cdp->getUnackedPacketCount())
		{
			// We are adding the first packed onto the unacked packet list(s)
			// Add this circuit to the list of circuits with unacked packets
			mCircuitInfo.mUnackedCircuitMap[cdp->mHost] = cdp;
		}

		cdp->addReliablePacket(mSocket, buf_ptr, buffer_length,
							   &mReliablePacketParams);
		++mReliablePacketsOut;
	}

	// Tack packet acks onto the end of this message

	// Space left for packet ids:
	S32 space_left = (MTUBYTES - buffer_length) / sizeof(TPACKETID);

	S32 ack_count = (S32)cdp->mAcks.size();
	bool is_ack_appended = false;
	std::vector<TPACKETID> acks;

	if (space_left > 0 && ack_count > 0 && msg_name != _PREHASH_PacketAck)
	{
		buf_ptr[0] |= LL_ACK_FLAG;
		S32 append_ack_count = llmin(space_left, ack_count);
		constexpr S32 MAX_ACKS = 250;
		append_ack_count = llmin(append_ack_count, MAX_ACKS);
		std::vector<TPACKETID>::iterator iter = cdp->mAcks.begin();
		std::vector<TPACKETID>::iterator last = cdp->mAcks.begin();
		last += append_ack_count;
		TPACKETID packet_id;
		for ( ; iter != last ; ++iter)
		{
			// Grab the next packet id.
			packet_id = (*iter);
			if (mVerboseLog)
			{
				acks.push_back(packet_id);
			}

			// Put it on the end of the buffer
			packet_id = htonl(packet_id);

			if ((S32)(buffer_length + sizeof(TPACKETID)) < MAX_BUFFER_SIZE)
			{
			    memcpy(&buf_ptr[buffer_length], &packet_id, sizeof(TPACKETID));
			    // Do the accounting
			    buffer_length += sizeof(TPACKETID);
			}
			else
			{
			    // Just reporting error is likely not enough. Need to check how
				// to abort or error out gracefully from this function. XXXTBD
				// *NOTE: actually, hitting this error would indicate that the
				// calculation above for space_left, ack_count,
				// append_acout_count is incorrect or that MAX_BUFFER_SIZE has
				// fallen below MTU which is bad and probably programmer error.
			    llerrs << "Buffer packing failed due to size." << llendl;
			}
		}

		// Clean up the source
		cdp->mAcks.erase(cdp->mAcks.begin(), last);

		// Tack the count in the final byte
		U8 count = (U8)append_ack_count;
		buf_ptr[buffer_length++] = count;
		is_ack_appended = true;
	}

	if (mPacketRing.sendPacket(mSocket, (char*)buf_ptr, buffer_length, host))
	{
		// mCircuitInfo already points to the correct circuit data
		cdp->addBytesOut(buffer_length);
	}
	else
	{
		++mSendPacketFailureCount;
	}

	if (mVerboseLog)
	{
		std::ostringstream str;
		str << "MSG: -> " << host;
		std::string buffer;
		buffer = llformat("\t%6d\t%6d\t%6d ", mSendSize, buffer_length,
						  cdp->getPacketOutID());
		str << buffer << msg_name << (mSendReliable ? " reliable " : "");
		if (is_ack_appended)
		{
			str << "\tACKS:\t";
			std::ostream_iterator<TPACKETID> append(str, " ");
			std::copy(acks.begin(), acks.end(), append);
		}
		llinfos << str.str() << llendl;
	}

	++mPacketsOut;
	mTotalBytesOut += buffer_length;

	mSendReliable = false;
	mReliablePacketParams.clear();
	return buffer_length;
}

void LLMessageSystem::logMsgFromInvalidCircuit(const LLHost& host,
											   bool recv_reliable)
{
	if (mVerboseLog)
	{
		std::ostringstream str;
		str << "MSG: <- " << host;
		std::string buffer = llformat("\t%6d\t%6d\t%6d ",
									  mMessageReader->getMessageSize(),
									  mIncomingCompressedSize ?
										mIncomingCompressedSize :
										mMessageReader->getMessageSize(),
									  mCurrentRecvPacketID);
		str << buffer << nullToEmpty(mMessageReader->getMessageName())
			<< (recv_reliable ? " reliable" : "") << " REJECTED";
		llinfos << str.str() << llendl;
	}

	// Keep track of rejected messages as well
	if (mNumMessageCounts >= MAX_MESSAGE_COUNT_NUM)
	{
		llwarns << "Got more than " << MAX_MESSAGE_COUNT_NUM
				<< " packets without clearing counts" << llendl;
	}
	else
	{
#if 0	// *TODO: babbage: work out if we need these
		mMessageCountList[mNumMessageCounts].mMessageNum =
			mCurrentRMessageTemplate->mMessageNumber;
#endif
		mMessageCountList[mNumMessageCounts].mMessageBytes =
			mMessageReader->getMessageSize();
		mMessageCountList[mNumMessageCounts++].mInvalid = true;
	}
}

S32 LLMessageSystem::sendMessage(const LLHost& host, const char* name,
								 const LLSD& message)
{
	if (!host.isOk())
	{
		llwarns << "trying to send message to invalid host"	<< llendl;
		return 0;
	}

	UntrustedCallback_t cb = NULL;
	if (mSendReliable && mReliablePacketParams.mCallback)
	{
		cb = boost::bind(mReliablePacketParams.mCallback,
						 mReliablePacketParams.mCallbackData, _1);
	}

	gCoros.launch("LLMessageSystem::sendUntrustedSimulatorMessageCoro",
				  boost::bind(&LLMessageSystem::sendUntrustedSimulatorMessageCoro,
							  this, host, name, message, cb));
	return 1;
}

void LLMessageSystem::logTrustedMsgFromUntrustedCircuit(const LLHost& host)
{
	// RequestTrustedCircuit is how we establish trust, so do not spam if it is
	// received on a trusted circuit. JC
	if (strcmp(mMessageReader->getMessageName(),
			   _PREHASH_RequestTrustedCircuit))
	{
		llwarns << "Received trusted message on untrusted circuit. "
				<< "Will reply with deny. "
				<< "Message: " << nullToEmpty(mMessageReader->getMessageName())
				<< " Host: " << host << llendl;
	}

	if (mNumMessageCounts >= MAX_MESSAGE_COUNT_NUM)
	{
		llwarns << "got more than " << MAX_MESSAGE_COUNT_NUM
				<< " packets without clearing counts" << llendl;
	}
	else
	{
#if 0	// *TODO: Babbage: work out if we need these
		mMessageCountList[mNumMessageCounts].mMessageNum =
			mCurrentRMessageTemplate->mMessageNumber;
#endif
		mMessageCountList[mNumMessageCounts].mMessageBytes =
			mMessageReader->getMessageSize();
		mMessageCountList[mNumMessageCounts++].mInvalid = true;
	}
}

void LLMessageSystem::logValidMsg(LLCircuitData* cdp, const LLHost& host,
								  bool recv_reliable, bool recv_resent,
								  bool recv_acks)
{
	if (mNumMessageCounts >= MAX_MESSAGE_COUNT_NUM)
	{
		llwarns << "Got more than " << MAX_MESSAGE_COUNT_NUM
				<< " packets without clearing counts" << llendl;
	}
	else
	{
#if 0	// *TODO: Babbage: work out if we need these
		mMessageCountList[mNumMessageCounts].mMessageNum =
			mCurrentRMessageTemplate->mMessageNumber;
#endif
		mMessageCountList[mNumMessageCounts].mMessageBytes =
			mMessageReader->getMessageSize();
		mMessageCountList[mNumMessageCounts++].mInvalid = false;
	}

	if (cdp)
	{
		// Update circuit packet ID tracking (missing/out of order packets)
		cdp->checkPacketInID(mCurrentRecvPacketID, recv_resent);
		cdp->addBytesIn(mTrueReceiveSize);
	}

	if (mVerboseLog)
	{
		std::ostringstream str;
		str << "MSG: <- " << host;
		std::string buffer = llformat("\t%6d\t%6d\t%6d ",
									  mMessageReader->getMessageSize(),
									  mIncomingCompressedSize ?
										mIncomingCompressedSize :
										mMessageReader->getMessageSize(),
									  mCurrentRecvPacketID);
		str << buffer << nullToEmpty(mMessageReader->getMessageName())
			<< (recv_reliable ? " reliable" : "")
			<< (recv_resent ? " resent" : "") << (recv_acks ? " acks" : "");
		llinfos << str.str() << llendl;
	}
}

void LLMessageSystem::getCircuitInfo(LLSD& info) const
{
	mCircuitInfo.getInfo(info);
}

// Activates a circuit, and set its trust level (true if trusted, false if not)
void LLMessageSystem::enableCircuit(const LLHost& host, bool trusted)
{
	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (!cdp)
	{
		cdp = mCircuitInfo.addCircuitData(host, 0);
	}
	else
	{
		cdp->setAlive(true);
	}
	cdp->setTrusted(trusted);
}

void LLMessageSystem::disableCircuit(const LLHost& host)
{
	llinfos << "Disabling " << host << llendl;
	LLMessageSystem* msg = gMessageSystemp;
	U32 code = msg->findCircuitCode(host);

	// Do not clean up 0 circuit code entries because many hosts (neighbor
	// sims, etc) can have the 0 circuit
	if (code)
	{
		code_session_map_t::iterator it = mCircuitCodes.find(code);
		if (it != mCircuitCodes.end())
		{
			llinfos << "Circuit " << code << " removed from list" << llendl;
			mCircuitCodes.hmap_erase(it);
		}

		U64 ip_port = 0;
		fast_hmap<U32, U64>::iterator iter =
			msg->mCircuitCodeToIPPort.find(code);
		if (iter != msg->mCircuitCodeToIPPort.end())
		{
			ip_port = iter->second;

			msg->mCircuitCodeToIPPort.erase(iter);

			U32 old_port = (U32)(ip_port & (U64)0xFFFFFFFF);
			U32 old_ip = (U32)(ip_port >> 32);

			llinfos << "Host " << LLHost(old_ip, old_port) << " circuit "
					<< code << " removed from lookup table" << llendl;
			msg->mIPPortToCircuitCode.erase(ip_port);
		}
		mCircuitInfo.removeCircuitData(host);
	}
	else
	{
		// Since we can open circuits which do not have circuit codes, it is
		// possible for this to happen...
		llinfos << "Could not find circuit code for " << host
			    << ", ignoring..." << llendl;
	}
}

void LLMessageSystem::setCircuitAllowTimeout(const LLHost& host, bool allow)
{
	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (cdp)
	{
		cdp->setAllowTimeout(allow);
	}
}

void LLMessageSystem::setCircuitTimeoutCallback(const LLHost& host,
												void (*callback_func)(const LLHost&,
																	  void*),
												void* user_data)
{
	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (cdp)
	{
		cdp->setTimeoutCallback(callback_func, user_data);
	}
}

bool LLMessageSystem::checkCircuitBlocked(U32 circuit)
{
	LLHost host = findHost(circuit);
	if (!host.isOk())
	{
		LL_DEBUGS("Messaging") << "Unknown circuit: " << circuit << LL_ENDL;
		return true;
	}

	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (cdp)
	{
		return cdp->isBlocked();
	}

	llinfos << "Unknown host: " << host << llendl;
	return false;
}

bool LLMessageSystem::checkCircuitAlive(U32 circuit)
{
	LLHost host = findHost(circuit);
	if (!host.isOk())
	{
		LL_DEBUGS("Messaging") << "Unknown circuit: " << circuit << LL_ENDL;
		return false;
	}

	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (cdp)
	{
		return cdp->isAlive();
	}

	llinfos << "Unknown host: " << host << llendl;
	return false;
}

bool LLMessageSystem::checkCircuitAlive(const LLHost& host)
{
	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (cdp)
	{
		return cdp->isAlive();
	}

	LL_DEBUGS("Messaging") << "Unknown host: " << host << LL_ENDL;
	return false;
}

U32 LLMessageSystem::findCircuitCode(const LLHost& host)
{
	U64 ip64 = (U64)host.getAddress();
	U64 port64 = (U64)host.getPort();
	U64 ip_port = (ip64 << 32) | port64;
	return get_if_there(mIPPortToCircuitCode, ip_port, U32(0));
}

LLHost LLMessageSystem::findHost(U32 circuit_code)
{
	if (!mCircuitCodeToIPPort.count(circuit_code))
	{
		return LLHost();
	}
	return LLHost(mCircuitCodeToIPPort[circuit_code]);
}

std::ostream& operator<<(std::ostream& s, LLMessageSystem& msg)
{
	U32 i;
	if (msg.mError)
	{
		s << "Message system not correctly initialized";
	}
	else
	{
		s << "Message system open on port " << msg.mPort << " and socket "
		  << msg.mSocket << "\n";
#if 0
		s << "Message template file " << msg.mName << " loaded\n";
#endif

		s << "\nHigh frequency messages:\n";

		for (i = 1; i < 255 && msg.mMessageNumbers[i]; ++i)
		{
			s << *(msg.mMessageNumbers[i]);
		}

		s << "\nMedium frequency messages:\n";

		for (i = (255 << 8) + 1;
			 i < (255 << 8) + 255 && msg.mMessageNumbers[i]; ++i)
		{
			s << *msg.mMessageNumbers[i];
		}

		s << "\nLow frequency messages:\n";

		for (i = 0xFFFF0001;
			 i < 0xFFFFFFFF && msg.mMessageNumbers[i]; ++i)
		{
			s << *msg.mMessageNumbers[i];
		}
	}
	return s;
}

LLMessageSystem* gMessageSystemp = NULL;

// Update appropriate ping info
void process_complete_ping_check(LLMessageSystem* msgsystem, void**)
{
	U8 ping_id;
	msgsystem->getU8Fast(_PREHASH_PingID, _PREHASH_PingID, ping_id);

	LLCircuitData* cdp;
	cdp = msgsystem->mCircuitInfo.findCircuit(msgsystem->getSender());
	if (cdp)
	{
		// Stop the appropriate timer
		cdp->pingTimerStop(ping_id);
	}
}

void process_start_ping_check(LLMessageSystem* msgsystem, void**)
{
	U8 ping_id;
	msgsystem->getU8Fast(_PREHASH_PingID, _PREHASH_PingID, ping_id);

	LLCircuitData* cdp;
	cdp = msgsystem->mCircuitInfo.findCircuit(msgsystem->getSender());
	if (cdp)
	{
		// Grab the packet id of the oldest unacked packet
		U32 packet_id;
		msgsystem->getU32Fast(_PREHASH_PingID, _PREHASH_OldestUnacked,
							  packet_id);
		cdp->clearDuplicateList(packet_id);
	}

	// Send off the response
	msgsystem->newMessageFast(_PREHASH_CompletePingCheck);
	msgsystem->nextBlockFast(_PREHASH_PingID);
	msgsystem->addU8(_PREHASH_PingID, ping_id);
	msgsystem->sendMessage(msgsystem->getSender());
}

// Note: this is currently unused. --mark
void open_circuit(LLMessageSystem* msgsystem, void**)
{
	U32 ip;
	msgsystem->getIPAddrFast(_PREHASH_CircuitInfo, _PREHASH_IP, ip);
	U16 port;
	msgsystem->getIPPortFast(_PREHASH_CircuitInfo, _PREHASH_Port, port);

	// By default, OpenCircuit's are untrusted
	msgsystem->enableCircuit(LLHost(ip, port), false);
}

void close_circuit(LLMessageSystem* msgsystem, void**)
{
	msgsystem->disableCircuit(msgsystem->getSender());
}

// static
void LLMessageSystem::processAddCircuitCode(LLMessageSystem* msg, void**)
{
	U32 code;
	msg->getU32Fast(_PREHASH_CircuitCode, _PREHASH_Code, code);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_CircuitCode, _PREHASH_SessionID, session_id);
	(void)msg->addCircuitCode(code, session_id);
}

bool LLMessageSystem::addCircuitCode(U32 code, const LLUUID& session_id)
{
	if (!code)
	{
		llwarns << "Zero circuit code" << llendl;
		return false;
	}
	code_session_map_t::iterator it = mCircuitCodes.find(code);
	if (it == mCircuitCodes.end())
	{
		llinfos << "New circuit code " << code << " added" << llendl;
		mCircuitCodes[code] = session_id;
	}
	else
	{
		llinfos << "Duplicate circuit code " << code << " added" << llendl;
	}
	return true;
}

// static
void LLMessageSystem::processUseCircuitCode(LLMessageSystem* msg,
											void** user)
{
	U32 circuit_code_in;
	msg->getU32Fast(_PREHASH_CircuitCode, _PREHASH_Code, circuit_code_in);

	U32 ip = msg->getSenderIP();
	U32 port = msg->getSenderPort();

	U64 ip64 = ip;
	U64 port64 = port;
	U64 ip_port_in = (ip64 << 32) | port64;

	if (!circuit_code_in)
	{
		llwarns << "Got zero circuit code in use_circuit_code" << llendl;
		return;
	}

	code_session_map_t::iterator it;
	it = msg->mCircuitCodes.find(circuit_code_in);
	if (it == msg->mCircuitCodes.end())
	{
		// Whoah, abort !  We do not know anything about this circuit code.
		llwarns << "UseCircuitCode for " << circuit_code_in
				<< " received without AddCircuitCode message. Aborting."
				<< llendl;
		return;
	}

	LLUUID id;
	msg->getUUIDFast(_PREHASH_CircuitCode, _PREHASH_ID, id);

	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_CircuitCode, _PREHASH_SessionID, session_id);
	if (session_id != it->second)
	{
		llwarns << "UseCircuitCode unmatched session id. Got " << session_id
				<< " but expected " << it->second << llendl;
		return;
	}

	// Clean up previous references to this ip/port or circuit
	U64 ip_port_old = get_if_there(msg->mCircuitCodeToIPPort,
								   circuit_code_in, U64(0));
	U32 circuit_code_old = get_if_there(msg->mIPPortToCircuitCode,
										ip_port_in, U32(0));
	if (ip_port_old)
	{
		if (ip_port_old == ip_port_in && circuit_code_old == circuit_code_in)
		{
			// Current information is the same as incoming info, ignore
			llinfos << "Got duplicate UseCircuitCode for circuit "
					<< circuit_code_in << " to " << msg->getSender() << llendl;
			return;
		}

		// Hmm, got a different IP and port for the same circuit code.
		U32 circut_code_old_ip_port = get_if_there(msg->mIPPortToCircuitCode,
												   ip_port_old, U32(0));
		msg->mCircuitCodeToIPPort.erase(circut_code_old_ip_port);
		msg->mIPPortToCircuitCode.erase(ip_port_old);
		U32 old_port = (U32)(ip_port_old & (U64)0xFFFFFFFF);
		U32 old_ip = (U32)(ip_port_old >> 32);
		llinfos << "Removing derelict lookup entry for circuit "
				<< circuit_code_old << " to " << LLHost(old_ip, old_port)
				<< llendl;
	}

	if (circuit_code_old)
	{
		LLHost cur_host(ip, port);

		llwarns << "Disabling existing circuit for " << cur_host << llendl;
		msg->disableCircuit(cur_host);
		if (circuit_code_old == circuit_code_in)
		{
			llwarns << "Asymmetrical circuit to IP/port lookup !  Multiple circuit codes for "
					<< cur_host
					<< ", probably... Permanently disabling circuit."
					<< llendl;
			return;
		}
		llwarns << "Circuit code changed for " << msg->getSender() << " from "
				<< circuit_code_old << " to " << circuit_code_in << llendl;
	}

	// Since this comes from the viewer, it is untrusted, but it passed the
	// circuit code and session id check, so we will go ahead and persist the
	// Id associated.
	LLCircuitData* cdp = msg->mCircuitInfo.findCircuit(msg->getSender());
	bool had_circuit_already = cdp != NULL;

	msg->enableCircuit(msg->getSender(), false);
	cdp = msg->mCircuitInfo.findCircuit(msg->getSender());
	if (cdp)
	{
		cdp->setRemoteID(id);
		cdp->setRemoteSessionID(session_id);
	}

	if (!had_circuit_already)
	{
		// *HACK: this would NORMALLY happen inside logValidMsg, but at the
		// point that this happens inside logValidMsg, there's no circuit for
		// this message yet. So the awful thing that we do here is do it inside
		// this message handler immediately AFTER the message is handled. We
		// COULD not do this, but then what happens is that some of the circuit
		// bookkeeping gets broken, especially the packets in count. That
		// causes some later packets to flush the RecentlyReceivedReliable
		// list, resulting in an error in which UseCircuitCode does not get
		// properly duplicate suppressed. Not a BIG deal, but it's somewhat
		// confusing (and bad from a state point of view). DJS 9/23/04

		// Since this is the first message on the circuit, by definition it is
		// not resent.
		cdp->checkPacketInID(msg->mCurrentRecvPacketID, false);
	}

	msg->mIPPortToCircuitCode[ip_port_in] = circuit_code_in;
	msg->mCircuitCodeToIPPort[circuit_code_in] = ip_port_in;

	llinfos << "Circuit code " << circuit_code_in << " from "
			<< msg->getSender() << " for agent " << id << " in session "
			<< session_id << llendl;

	const LLUseCircuitCodeResponder* responder =
		(const LLUseCircuitCodeResponder*)user;
	if (responder)
	{
		responder->complete(msg->getSender(), id);
	}
}

// static
void LLMessageSystem::processError(LLMessageSystem* msg, void**)
{
	S32 error_code = 0;
	msg->getS32("Data", "Code", error_code);
	std::string error_token;
	msg->getString("Data", "Token", error_token);

	LLUUID error_id;
	msg->getUUID("Data", "ID", error_id);
	std::string error_system;
	msg->getString("Data", "System", error_system);

	std::string error_message;
	msg->getString("Data", "Message", error_message);

	llwarns << "Message error from " << msg->getSender() << " - "
			<< error_code << " " << error_token << " " << error_id << " \""
			<< error_system << "\" \"" << error_message << "\"" << llendl;
}

static LLHTTPNode& messageRootNode()
{
	static LLHTTPNode root_node;
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;
		LLHTTPRegistrar::buildAllServices(root_node);
	}
	return root_node;
}

//static
void LLMessageSystem::dispatch(const std::string& msg_name,
							   const LLSD& message)
{
	LLPointer<LLSimpleResponse>	responsep =	LLSimpleResponse::create();
	dispatch(msg_name, message, responsep);
}

//static
void LLMessageSystem::dispatch(const std::string& msg_name,
							   const LLSD& message,
							   LLHTTPNode::ResponsePtr responsep)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg->mMessageTemplates.find(gMessageStringTable.getString(msg_name.c_str())) ==
			msg->mMessageTemplates.end() &&
		!LLMessageConfig::isValidMessage(msg_name))
	{
		llwarns << "Ignoring unknown message " << msg_name << llendl;
		responsep->notFound("Invalid message name");
		return;
	}

	std::string	path = "/message/" + msg_name;
	LLSD context;
	const LLHTTPNode* handler =	messageRootNode().traverse(path, context);
	if (!handler)
	{
		llwarns	<< "No handler for " << path << llendl;
		return;
	}

	LL_DEBUGS("Messaging") << "Received: " << msg_name << " from "
						   << message["sender"].asString() << LL_ENDL;

	handler->post(responsep, context, message);
}

static void check_for_unknown_msg(const char* type, const LLSD& map,
								  LLMessageSystem::template_name_map_t& templates)
{
	for (LLSD::map_const_iterator iter = map.beginMap(), end = map.endMap();
		 iter != end; ++iter)
	{
		const char* name = gMessageStringTable.getString(iter->first.c_str());

		if (templates.find(name) == templates.end())
		{
			llinfos << "Ban list type " << type
					<< " contains unrecognized message " << name << llendl;
		}
	}
}

void LLMessageSystem::setMessageBans(const LLSD& trusted,
									 const LLSD& untrusted)
{
	LL_DEBUGS("AppInit") << "Setting message bans" << LL_ENDL;
	bool any_set = false;

	for (template_name_map_t::iterator iter = mMessageTemplates.begin(),
									   end = mMessageTemplates.end();
		 iter != end; ++iter)
	{
		LLMessageTemplate* mt = iter->second;

		std::string name(mt->mName);
		bool ban_from_trusted = trusted.has(name) &&
								trusted.get(name).asBoolean();
		bool ban_from_untrusted = untrusted.has(name) &&
								  untrusted.get(name).asBoolean();

		mt->mBanFromTrusted = ban_from_trusted;
		mt->mBanFromUntrusted = ban_from_untrusted;

		if (ban_from_trusted  ||  ban_from_untrusted)
		{
			llinfos << name << " banned from "
				<< (ban_from_trusted ? "TRUSTED " : " ")
				<< (ban_from_untrusted ? "UNTRUSTED " : " ") << llendl;
			any_set = true;
		}
	}

	if (!any_set)
	{
		LL_DEBUGS("AppInit") << "No messages banned" << LL_ENDL;
	}

	check_for_unknown_msg("trusted", trusted, mMessageTemplates);
	check_for_unknown_msg("untrusted", untrusted, mMessageTemplates);
}

S32 LLMessageSystem::sendError(const LLHost& host, const LLUUID& agent_id,
							   S32 code, const std::string& token,
							   const LLUUID& id, const std::string& system,
							   const std::string& message, const LLSD& data)
{
	newMessage("Error");
	nextBlockFast(_PREHASH_AgentData);
	addUUIDFast(_PREHASH_AgentID, agent_id);
	nextBlockFast(_PREHASH_Data);
	addS32("Code", code);
	addString("Token", token);
	addUUID("ID", id);
	addString("System", system);
	std::string temp;
	temp = message;
	if (temp.size() > (size_t)MTUBYTES)
	{
		temp.resize((size_t)MTUBYTES);
	}
	addString("Message", message);
	LLPointer<LLSDBinaryFormatter> formatter = new LLSDBinaryFormatter;
	std::ostringstream ostr;
	formatter->format(data, ostr);
	temp = ostr.str();
	bool pack_data = true;
	static const std::string ERROR_MESSAGE_NAME("Error");
	if (LLMessageConfig::getMessageFlavor(ERROR_MESSAGE_NAME) ==
		LLMessageConfig::TEMPLATE_FLAVOR)
	{
		S32 msg_size = temp.size() + mMessageBuilder->getMessageSize();
		if (msg_size >= ETHERNET_MTU_BYTES)
		{
			pack_data = false;
		}
	}
	if (pack_data)
	{
		addBinaryData("Data", (void*)temp.c_str(), temp.size());
	}
	else
	{
		llwarns << "Data and message were too large; data removed." << llendl;
		addBinaryData("Data", NULL, 0);
	}
	return sendReliable(host);
}

void process_packet_ack(LLMessageSystem* msgsystem, void**)
{
	LLHost host = msgsystem->getSender();
	LLCircuitData* cdp = msgsystem->mCircuitInfo.findCircuit(host);
	if (!cdp)
	{
		return;
	}

	TPACKETID packet_id;
	S32 ack_count = msgsystem->getNumberOfBlocksFast(_PREHASH_Packets);
	for (S32 i = 0; i < ack_count; i++)
	{
		msgsystem->getU32Fast(_PREHASH_Packets, _PREHASH_ID, packet_id, i);
		cdp->ackReliablePacket(packet_id);
	}
	if (!cdp->getUnackedPacketCount())
	{
		// Remove this circuit from the list of circuits with unacked packets
		msgsystem->mCircuitInfo.mUnackedCircuitMap.erase(host);
	}
}

bool start_messaging_system(const std::string& template_name, U32 port,
							S32 major, S32 minor, S32 patch,
							const LLUseCircuitCodeResponder* responder,
							F32 heartbeat_interval, F32 timeout)
{
	gMessageSystemp = new LLMessageSystem(template_name, port, major, minor,
										  patch, heartbeat_interval, timeout);
	if (!gMessageSystemp)
	{
		llerrs << "Messaging system initialization failed." << llendl;
	}

	// Bail if system encountered an error.
	if (!gMessageSystemp->isOK())
	{
		return false;
	}

	if (gMessageSystemp->mMessageFileVersionNumber != gPrehashVersionNumber)
	{
		llinfos << "Message template version does not match prehash version number. Run simulator with -prehash command line option to rebuild prehash data"
				<< llendl;
	}
	else
	{
		LL_DEBUGS("AppInit") << "Message template version matches prehash version number"
							 << LL_ENDL;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->setHandlerFuncFast(_PREHASH_StartPingCheck,
							process_start_ping_check, NULL);
	msg->setHandlerFuncFast(_PREHASH_CompletePingCheck,
							process_complete_ping_check, NULL);
	msg->setHandlerFuncFast(_PREHASH_OpenCircuit, open_circuit, NULL);
	msg->setHandlerFuncFast(_PREHASH_CloseCircuit, close_circuit, NULL);

	msg->setHandlerFuncFast(_PREHASH_AddCircuitCode,
							LLMessageSystem::processAddCircuitCode);
	msg->setHandlerFuncFast(_PREHASH_UseCircuitCode,
							LLMessageSystem::processUseCircuitCode,
							(void**)responder);
	msg->setHandlerFuncFast(_PREHASH_PacketAck, process_packet_ack, NULL);

	// These two are only relevant to SL servers and rely on a "shared secret"
	// (a MD5 sum) that we always passed as a empty string in the viewer. So do
	// not bother with them and replace the server-specific callbacks (which I
	// removed from this module) with a null callback. HB
	msg->setHandlerFuncFast(_PREHASH_CreateTrustedCircuit,
							null_message_callback, NULL);
	msg->setHandlerFuncFast(_PREHASH_DenyTrustedCircuit,
							null_message_callback, NULL);

	msg->setHandlerFunc("Error", LLMessageSystem::processError);

	// We can hand this to the null_message_callback since it is a trusted
	// message, so it will automatically be denied if it is not trusted and
	// ignored if it is: exactly what we want.
	msg->setHandlerFunc(_PREHASH_RequestTrustedCircuit, null_message_callback, NULL);

	// Initialize the transfer manager
	gTransferManager.init();

	return true;
}

void LLMessageSystem::startLogging()
{
	mVerboseLog = true;
	std::ostringstream str;
	str << "START MESSAGE LOG" << std::endl;
	str << "Legend:" << std::endl;
	str << "\t<-\tincoming message" <<std::endl;
	str << "\t->\toutgoing message" << std::endl;
	str << "     <>        host           size    zero      id name";
	llinfos << str.str() << llendl;
}

void LLMessageSystem::stopLogging()
{
	if (mVerboseLog)
	{
		mVerboseLog = false;
		llinfos << "END MESSAGE LOG" << llendl;
	}
}

void LLMessageSystem::summarizeLogs(std::ostream& str)
{
	F32 run_time = mMessageSystemTimer.getElapsedTimeF32();
	str << "START MESSAGE LOG SUMMARY" << std::endl;
	std::string buffer = llformat("Run time: %12.3f seconds", run_time);

	F32 kbps = 0.008f / run_time;
	F32 packets_in = (F32)llmax(mPacketsIn, 1U);
	F32 packets_out = (F32)llmax(mPacketsOut, 1U);

	// Incoming
	str << buffer << std::endl << "Incoming (sim traffic):" << std::endl;
	std::string tmp_str = U64_to_str(mTotalBytesIn);
	buffer = llformat("Total bytes received:      %20s (%5.2f kbits per second)",
					  tmp_str.c_str(), (F32)mTotalBytesIn * kbps);
	str << buffer << std::endl;
	tmp_str = U64_to_str(mPacketsIn);
	buffer = llformat("Total packets received:    %20s (%5.2f packets per second)",
					  tmp_str.c_str(), (F32)mPacketsIn / run_time);
	str << buffer << std::endl;
	buffer = llformat("Average packet size:       %20.0f bytes",
					  (F32)mTotalBytesIn / packets_in);
	str << buffer << std::endl;
	tmp_str = U64_to_str(mReliablePacketsIn);
	buffer = llformat("Total reliable packets:    %20s (%5.2f%%)",
					  tmp_str.c_str(),
					  100.f * (F32)mReliablePacketsIn / packets_in);
	str << buffer << std::endl;
	tmp_str = U64_to_str(mCompressedPacketsIn);
	buffer = llformat("Total compressed packets:  %20s (%5.2f%%)",
					  tmp_str.c_str(),
					  100.f * (F32)mCompressedPacketsIn / packets_in);
	str << buffer << std::endl;
	S64 savings = mUncompressedBytesIn - mCompressedBytesIn;
	tmp_str = U64_to_str(savings);
	buffer = llformat("Total compression savings: %20s bytes",
					  tmp_str.c_str());
	str << buffer << std::endl;
	tmp_str = U64_to_str(savings / (mCompressedPacketsIn + 1));
	buffer = llformat("Avg comp packet savings:   %20s (%5.2f : 1)",
					  tmp_str.c_str(),
					  (F32)mUncompressedBytesIn /
					  (F32)(mCompressedBytesIn + 1));
	str << buffer << std::endl;
	tmp_str = U64_to_str(savings / (mPacketsIn + 1));
	buffer = llformat("Avg overall comp savings:  %20s (%5.2f : 1)",
					  tmp_str.c_str(),
					  ((F32)mTotalBytesIn +
					   (F32)savings) / (F32)(mTotalBytesIn + 1));

	// Outgoing
	str << buffer << std::endl << std::endl << "Outgoing (sim traffic):"
		<< std::endl;
	tmp_str = U64_to_str(mTotalBytesOut);
	buffer = llformat("Total bytes sent:          %20s (%5.2f kbits per second)",
					  tmp_str.c_str(), (F32)mTotalBytesOut * kbps);
	str << buffer << std::endl;
	tmp_str = U64_to_str(mPacketsOut);
	buffer = llformat("Total packets sent:        %20s (%5.2f packets per second)",
					  tmp_str.c_str(), (F32)mPacketsOut / run_time);
	str << buffer << std::endl;
	buffer = llformat("Average packet size:       %20.0f bytes",
					  (F32)mTotalBytesOut / packets_out);
	str << buffer << std::endl;
	tmp_str = U64_to_str(mReliablePacketsOut);
	buffer = llformat("Total reliable packets:    %20s (%5.2f%%)",
					  tmp_str.c_str(),
					  100.f * (F32)mReliablePacketsOut / packets_out);
	str << buffer << std::endl;
	tmp_str = U64_to_str(mCompressedPacketsOut);
	buffer = llformat("Total compressed packets:  %20s (%5.2f%%)",
					  tmp_str.c_str(),
					  100.f * (F32)mCompressedPacketsOut / packets_out);
	str << buffer << std::endl;
	savings = mUncompressedBytesOut - mCompressedBytesOut;
	tmp_str = U64_to_str(savings);
	buffer = llformat("Total compression savings: %20s bytes",
					  tmp_str.c_str());
	str << buffer << std::endl;
	tmp_str = U64_to_str(savings / (mCompressedPacketsOut + 1));
	buffer = llformat("Avg comp packet savings:   %20s (%5.2f : 1)",
					  tmp_str.c_str(),
					  (F32)mUncompressedBytesOut /
					  (F32)(mCompressedBytesOut + 1));
	str << buffer << std::endl;
	tmp_str = U64_to_str(savings/(mPacketsOut+1));
	buffer = llformat("Avg overall comp savings:  %20s (%5.2f : 1)",
					  tmp_str.c_str(),
					  ((F32)mTotalBytesOut +
					   (F32)savings) / (F32)(mTotalBytesOut + 1));
	str << buffer << std::endl << std::endl;
	buffer = llformat("SendPacket failures:       %20d",
					  mSendPacketFailureCount);
	str << buffer << std::endl;
	buffer = llformat("Dropped packets:           %20d", mDroppedPackets);
	str << buffer << std::endl;
	buffer = llformat("Resent packets:            %20d", mResentPackets);
	str << buffer << std::endl;
	buffer = llformat("Failed reliable resends:   %20d", mFailedResendPackets);
	str << buffer << std::endl;
	buffer = llformat("Off-circuit rejected packets: %17d",
					  mOffCircuitPackets);
	str << buffer << std::endl;
	buffer = llformat("On-circuit invalid packets:   %17d",
					  mInvalidOnCircuitPackets);
	str << buffer << std::endl << std::endl;

	if (LLMessageReader::getTimeDecodes())
	{
		str << "Decoding: " << std::endl;
		buffer = llformat("%35s%10s%10s%10s%10s",
						  "Message", "Count", "Time", "Max", "Avg");
		str << buffer << std:: endl;
		for (template_name_map_t::const_iterator
				iter = mMessageTemplates.begin(), end = mMessageTemplates.end();
			 iter != end; ++iter)
		{
			const LLMessageTemplate* mt = iter->second;
			if (mt->mTotalDecoded > 0.f)
			{
				F32 avg = mt->mTotalDecodeTime / (F32)mt->mTotalDecoded;
				buffer = llformat("%35s%10u%10f%10f%10f", mt->mName,
								  mt->mTotalDecoded, mt->mTotalDecodeTime,
								  mt->mMaxDecodeTimePerMsg, avg);
				str << buffer << std::endl;
			}
		}
		str << std::endl;
	}

	str << "Incoming (curl HTTP traffic):" << std::endl;
	U64 bytes = LLCore::HttpLibcurl::getDownloadedBytes();
	tmp_str = U64_to_str(bytes);
	buffer = llformat("Total bytes received:      %20s (%5.2f kbits per second)",
					  tmp_str.c_str(), (F32)bytes * kbps);
	str << buffer << std::endl << std::endl << "Outgoing (curl HTTP traffic):"
		<< std::endl;
	bytes = LLCore::HttpLibcurl::getUploadedBytes();
	tmp_str = U64_to_str(bytes);
	buffer = llformat("Total bytes sent:          %20s (%5.2f kbits per second)",
					  tmp_str.c_str(), (F32)bytes * kbps);
	str << buffer << std::endl;

	str << "END MESSAGE LOG SUMMARY" << std::endl;
}

void end_messaging_system(bool print_summary)
{
	gTransferManager.cleanup();
	LLTransferTargetVFile::updateQueue(true); // Shutdown LLTransferTargetVFile
	if (gMessageSystemp)
	{
		gMessageSystemp->stopLogging();

		if (print_summary)
		{
			std::ostringstream str;
			gMessageSystemp->summarizeLogs(str);
			llinfos << str.str().c_str() << llendl;
		}

		delete gMessageSystemp;
		gMessageSystemp = NULL;
	}
}

void LLMessageSystem::resetReceiveCounts()
{
	mNumMessageCounts = 0;

	for (template_name_map_t::iterator iter = mMessageTemplates.begin(),
									   end = mMessageTemplates.end();
		 iter != end; ++iter)
	{
		LLMessageTemplate* mt = iter->second;
		mt->mDecodeTimeThisFrame = 0.f;
	}
}

void LLMessageSystem::dumpReceiveCounts()
{
	LLMessageTemplate* mt;

	for (template_name_map_t::iterator iter = mMessageTemplates.begin(),
									   end = mMessageTemplates.end();
		 iter != end; ++iter)
	{
		LLMessageTemplate* mt = iter->second;
		mt->mReceiveCount = 0;
		mt->mReceiveBytes = 0;
		mt->mReceiveInvalid = 0;
	}

	S32 i;
	for (i = 0; i < mNumMessageCounts; ++i)
	{
		mt = get_ptr_in_map(mMessageNumbers,mMessageCountList[i].mMessageNum);
		if (mt)
		{
			++mt->mReceiveCount;
			mt->mReceiveBytes += mMessageCountList[i].mMessageBytes;
			if (mMessageCountList[i].mInvalid)
			{
				++mt->mReceiveInvalid;
			}
		}
	}

	if (mNumMessageCounts > 0)
	{
		LL_DEBUGS("Messaging") << "Dump: " << mNumMessageCounts
							   << " messages processed in " << mReceiveTime
							   << " seconds" << LL_ENDL;
		for (template_name_map_t::const_iterator
				iter = mMessageTemplates.begin(),
				end = mMessageTemplates.end();
			 iter != end; ++iter)
		{
			const LLMessageTemplate* mt = iter->second;
			if (mt->mReceiveCount > 0)
			{
				llinfos << "Num: " << std::setw(3) << mt->mReceiveCount
						<< " Bytes: " << std::setw(6) << mt->mReceiveBytes
						<< " Invalid: " << std::setw(3) << mt->mReceiveInvalid
						<< " " << mt->mName << " "
						<< ll_round(100 * mt->mDecodeTimeThisFrame / mReceiveTime)
						<< "%" << llendl;
			}
		}
	}
}

S32 LLMessageSystem::flush(const LLHost& host)
{
	if (mMessageBuilder->getMessageSize())
	{
		S32 sentbytes = sendMessage(host);
		clearMessage();
		return sentbytes;
	}
	return 0;
}

S32 LLMessageSystem::zeroCodeExpand(U8** data, S32* data_size)
{
	if (*data_size < LL_MINIMUM_VALID_PACKET_SIZE)
	{
		llwarns << "Call done with invalid data size: " << *data_size
				<< llendl;
	}

	mTotalBytesIn += *data_size;

	// If we are not zero-coded, simply return.
	if (!(*data[0] & LL_ZERO_CODE_FLAG))
	{
		return 0;
	}

	S32 in_size = *data_size;
	++mCompressedPacketsIn;
	mCompressedBytesIn += *data_size;

	*data[0] &= ~LL_ZERO_CODE_FLAG;

	S32 count = *data_size;

	U8* inptr = (U8*)*data;
	U8* outptr = (U8*)mEncodedRecvBuffer;

	// Skip the packet id field
	for (U32 ii = 0; ii < LL_PACKET_ID_SIZE; ++ii)
	{
		--count;
		*outptr++ = *inptr++;
	}

	// Reconstruct encoded packet, keeping track of net size gain. Sequential
	// zero bytes are encoded as 0 [U8 count] with 0 0 [count] representing
	// wrap (>256 zeros)

	while (count--)
	{
		if (outptr > (&mEncodedRecvBuffer[MAX_BUFFER_SIZE - 1]))
		{
			llwarns << "attempt to write past reasonable encoded buffer size 1"
					<< llendl;
			callExceptionFunc(MX_WROTE_PAST_BUFFER_SIZE);
			outptr = mEncodedRecvBuffer;
			break;
		}
		if (!((*outptr++ = *inptr++)))
		{
			while (((count--)) && (!(*inptr)))
			{
				*outptr++ = *inptr++;
  				if (outptr > (&mEncodedRecvBuffer[MAX_BUFFER_SIZE - 256]))
  				{
  					llwarns << "attempt to write past reasonable encoded buffer size 2"
							<< llendl;
					callExceptionFunc(MX_WROTE_PAST_BUFFER_SIZE);
					outptr = mEncodedRecvBuffer;
					count = -1;
					break;
  				}
				memset(outptr, 0, 255);
				outptr += 255;
			}

			if (count < 0)
			{
				break;
			}

			else
			{
  				if (outptr > (&mEncodedRecvBuffer[MAX_BUFFER_SIZE - (*inptr)]))
				{
  					llwarns << "attempt to write past reasonable encoded buffer size 3"
							<< llendl;
					callExceptionFunc(MX_WROTE_PAST_BUFFER_SIZE);
					outptr = mEncodedRecvBuffer;
				}
				memset(outptr, 0, *inptr - 1);
				outptr += (*inptr++) - 1;
			}
		}
	}

	*data = mEncodedRecvBuffer;
	*data_size = (S32)(outptr - mEncodedRecvBuffer);
	mUncompressedBytesIn += *data_size;

	return in_size;
}

void LLMessageSystem::addTemplate(LLMessageTemplate* templatep)
{
	if (mMessageTemplates.count(templatep->mName) > 0)
	{
		llerrs << templatep->mName << " already used as a template name !"
			   << llendl;
	}
	mMessageTemplates[templatep->mName] = templatep;
	mMessageNumbers[templatep->mMessageNumber] = templatep;
}

void LLMessageSystem::setHandlerFuncFast(const char* name,
										 void (*handler_func)(LLMessageSystem*,
															  void**),
										 void** user_data)
{
	LLMessageTemplate* msgtemplate = get_ptr_in_map(mMessageTemplates, name);
	if (!msgtemplate)
	{
		llerrs << name << " is not a known message name !" << llendl;
	}
	msgtemplate->setHandlerFunc(handler_func, user_data);
}

bool LLMessageSystem::callHandler(const char* name, bool trusted_source)
{
	name = gMessageStringTable.getString(name);
	template_name_map_t::const_iterator iter = mMessageTemplates.find(name);
	if (iter == mMessageTemplates.end())
	{
		llwarns << "Unknown message " << name << llendl;
		return false;
	}

	const LLMessageTemplate* msg_template = iter->second;
	if (msg_template->isBanned(trusted_source))
	{
		llwarns << "Banned message " << name << " from "
				<< (trusted_source ? "trusted " : "untrusted ") << "source"
				<< llendl;
		return false;
	}

	return msg_template->callHandlerFunc(this);
}

void LLMessageSystem::setExceptionFunc(EMessageException e,
									   msg_exception_callback func,
									   void* data)
{
	callbacks_t::iterator it = mExceptionCallbacks.find(e);
	if (it != mExceptionCallbacks.end())
	{
		mExceptionCallbacks.erase(it);
	}
	if (func)
	{
		mExceptionCallbacks.emplace(e, exception_t(func, data));
	}
}

bool LLMessageSystem::callExceptionFunc(EMessageException exception)
{
	callbacks_t::iterator it = mExceptionCallbacks.find(exception);
	if (it == mExceptionCallbacks.end())
	{
		return false;
	}

	exception_t& ex = it->second;
	msg_exception_callback ex_cb = ex.first;
	if (!ex_cb)
	{
		llwarns << "NULL message exception callback !" << llendl;
		return false;
	}

	(ex_cb)(this, ex.second, exception);

	return true;
}

void LLMessageSystem::setTimingFunc(msg_timing_callback func, void* data)
{
	mTimingCallback = func;
	mTimingCallbackData = data;
}

bool LLMessageSystem::isMessageFast(const char* msg)
{
	return msg == mMessageReader->getMessageName();
}

char* LLMessageSystem::getMessageName()
{
	return const_cast<char*>(mMessageReader->getMessageName());
}

const LLUUID& LLMessageSystem::getSenderID() const
{
	LLCircuitData* cdp = mCircuitInfo.findCircuit(mLastSender);
	return cdp ? cdp->mRemoteID : LLUUID::null;
}

const LLUUID& LLMessageSystem::getSenderSessionID() const
{
	LLCircuitData* cdp = mCircuitInfo.findCircuit(mLastSender);
	return cdp ? cdp->mRemoteSessionID : LLUUID::null;
}

void LLMessageSystem::sendDenyTrustedCircuit(const LLHost& host)
{
	mDenyTrustedCircuitSet.emplace(host);
}

void LLMessageSystem::reallySendDenyTrustedCircuit(const LLHost& host)
{
	LLCircuitData* cdp = mCircuitInfo.findCircuit(host);
	if (!cdp)
	{
		llwarns << "Not sending DenyTrustedCircuit to host without a circuit."
				<< llendl;
		return;
	}
	llinfos << "Sending DenyTrustedCircuit to " << host << llendl;
	newMessageFast(_PREHASH_DenyTrustedCircuit);
	nextBlockFast(_PREHASH_DataBlock);
	addUUIDFast(_PREHASH_EndPointID, cdp->getLocalEndPointID());
	sendMessage(host);
}

void null_message_callback(LLMessageSystem*, void**)
{
	// Nothing should ever go here, but we use this to register messages that
	// we are expecting to see (and spinning on) at startup.
}

void LLMessageSystem::dumpPacketToLog()
{
	llwarns << "Packet Dump from:" << mPacketRing.getLastSender() << llendl;
	llwarns << "Packet Size:" << mTrueReceiveSize << llendl;
	char line_buffer[256];
	S32 i;
	S32 cur_line_pos = 0;
	S32 cur_line = 0;

	for (i = 0; i < mTrueReceiveSize; ++i)
	{
		S32 offset = cur_line_pos * 3;
		snprintf(line_buffer + offset, sizeof(line_buffer) - offset,
				 "%02x ", mTrueReceiveBuffer[i]);
		if (++cur_line_pos >= 16)
		{
			cur_line_pos = 0;
			llwarns << "PD:" << cur_line << "PD:" << line_buffer << llendl;
			++cur_line;
		}
	}
	if (cur_line_pos)
	{
		llwarns << "PD:" << cur_line << "PD:" << line_buffer << llendl;
	}
}

//static
U64 LLMessageSystem::getMessageTimeUsecs(bool update)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg)
	{
		if (update)
		{
			msg->mCurrentMessageTimeSeconds = LLTimer::totalTime() *
											  SEC_PER_USEC;
		}
		return (U64)(msg->mCurrentMessageTimeSeconds * USEC_PER_SEC);
	}

	return LLTimer::totalTime();
}

//static
F64 LLMessageSystem::getMessageTimeSeconds(bool update)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg)
	{
		if (update)
		{
			msg->mCurrentMessageTimeSeconds = LLTimer::totalTime() *
											  SEC_PER_USEC;
		}
		return msg->mCurrentMessageTimeSeconds;
	}

	return LLTimer::totalTime() * SEC_PER_USEC;
}

typedef std::map<const char*, LLMessageBuilder*> BuilderMap;

void LLMessageSystem::newMessageFast(const char* name)
{
	LLMessageConfig::Flavor message_flavor =
		LLMessageConfig::getMessageFlavor(name);
	LLMessageConfig::Flavor server_flavor =
		LLMessageConfig::getServerDefaultFlavor();

	if (message_flavor == LLMessageConfig::TEMPLATE_FLAVOR)
	{
		mMessageBuilder = mTemplateMessageBuilder;
	}
	else if (message_flavor == LLMessageConfig::LLSD_FLAVOR)
	{
		mMessageBuilder = mLLSDMessageBuilder;
	}
	// NO_FLAVOR
	else if (server_flavor == LLMessageConfig::LLSD_FLAVOR)
	{
		mMessageBuilder = mLLSDMessageBuilder;
	}
	// TEMPLATE_FLAVOR or NO_FLAVOR
	else
	{
		mMessageBuilder = mTemplateMessageBuilder;
	}
	mSendReliable = false;
	mMessageBuilder->newMessage(name);
}

void LLMessageSystem::newMessage(const char* name)
{
	newMessageFast(gMessageStringTable.getString(name));
}

void LLMessageSystem::addBinaryDataFast(const char* varname, const void* data,
										S32 size)
{
	mMessageBuilder->addBinaryData(varname, data, size);
}

void LLMessageSystem::addBinaryData(const char* varname, const void* data,
									S32 size)
{
	mMessageBuilder->addBinaryData(gMessageStringTable.getString(varname),
								   data, size);
}

void LLMessageSystem::addS8Fast(const char* varname, S8 v)
{
	mMessageBuilder->addS8(varname, v);
}

void LLMessageSystem::addS8(const char* varname, S8 v)
{
	mMessageBuilder->addS8(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addU8Fast(const char* varname, U8 v)
{
	mMessageBuilder->addU8(varname, v);
}

void LLMessageSystem::addU8(const char* varname, U8 v)
{
	mMessageBuilder->addU8(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addS16Fast(const char* varname, S16 v)
{
	mMessageBuilder->addS16(varname, v);
}

void LLMessageSystem::addS16(const char* varname, S16 v)
{
	mMessageBuilder->addS16(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addU16Fast(const char* varname, U16 v)
{
	mMessageBuilder->addU16(varname, v);
}

void LLMessageSystem::addU16(const char* varname, U16 v)
{
	mMessageBuilder->addU16(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addF32Fast(const char* varname, F32 v)
{
	mMessageBuilder->addF32(varname, v);
}

void LLMessageSystem::addF32(const char* varname, F32 v)
{
	mMessageBuilder->addF32(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addS32Fast(const char* varname, S32 v)
{
	mMessageBuilder->addS32(varname, v);
}

void LLMessageSystem::addS32(const char* varname, S32 v)
{
	mMessageBuilder->addS32(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addU32Fast(const char* varname, U32 v)
{
	mMessageBuilder->addU32(varname, v);
}

void LLMessageSystem::addU32(const char* varname, U32 v)
{
	mMessageBuilder->addU32(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addU64Fast(const char* varname, U64 v)
{
	mMessageBuilder->addU64(varname, v);
}

void LLMessageSystem::addU64(const char* varname, U64 v)
{
	mMessageBuilder->addU64(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addF64Fast(const char* varname, F64 v)
{
	mMessageBuilder->addF64(varname, v);
}

void LLMessageSystem::addF64(const char* varname, F64 v)
{
	mMessageBuilder->addF64(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addIPAddrFast(const char* varname, U32 v)
{
	mMessageBuilder->addIPAddr(varname, v);
}

void LLMessageSystem::addIPAddr(const char* varname, U32 v)
{
	mMessageBuilder->addIPAddr(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addIPPortFast(const char* varname, U16 v)
{
	mMessageBuilder->addIPPort(varname, v);
}

void LLMessageSystem::addIPPort(const char* varname, U16 v)
{
	mMessageBuilder->addIPPort(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addBoolFast(const char* varname, bool v)
{
	mMessageBuilder->addBool(varname, v);
}

void LLMessageSystem::addBool(const char* varname, bool v)
{
	mMessageBuilder->addBool(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addStringFast(const char* varname, const char* v)
{
	mMessageBuilder->addString(varname, v);
}

void LLMessageSystem::addString(const char* varname, const char* v)
{
	mMessageBuilder->addString(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addStringFast(const char* varname, const std::string& v)
{
	mMessageBuilder->addString(varname, v);
}

void LLMessageSystem::addString(const char* varname, const std::string& v)
{
	mMessageBuilder->addString(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addVector3Fast(const char* varname, const LLVector3& v)
{
	mMessageBuilder->addVector3(varname, v);
}

void LLMessageSystem::addVector3(const char* varname, const LLVector3& v)
{
	mMessageBuilder->addVector3(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addVector4Fast(const char* varname, const LLVector4& v)
{
	mMessageBuilder->addVector4(varname, v);
}

void LLMessageSystem::addVector4(const char* varname, const LLVector4& v)
{
	mMessageBuilder->addVector4(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addVector3dFast(const char* varname, const LLVector3d& v)
{
	mMessageBuilder->addVector3d(varname, v);
}

void LLMessageSystem::addVector3d(const char* varname, const LLVector3d& v)
{
	mMessageBuilder->addVector3d(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addQuatFast(const char* varname, const LLQuaternion& v)
{
	mMessageBuilder->addQuat(varname, v);
}

void LLMessageSystem::addQuat(const char* varname, const LLQuaternion& v)
{
	mMessageBuilder->addQuat(gMessageStringTable.getString(varname), v);
}

void LLMessageSystem::addUUIDFast(const char* varname, const LLUUID& v)
{
	mMessageBuilder->addUUID(varname, v);
}

void LLMessageSystem::addUUID(const char* varname, const LLUUID& v)
{
	mMessageBuilder->addUUID(gMessageStringTable.getString(varname), v);
}

S32 LLMessageSystem::getCurrentSendTotal() const
{
	return mMessageBuilder->getMessageSize();
}

void LLMessageSystem::getS8Fast(const char* block, const char* var, S8& u,
								S32 blocknum)
{
	mMessageReader->getS8(block, var, u, blocknum);
}

void LLMessageSystem::getS8(const char* block, const char* var, S8& u,
							S32 blocknum)
{
	getS8Fast(gMessageStringTable.getString(block),
			  gMessageStringTable.getString(var), u, blocknum);
}

void LLMessageSystem::getU8Fast(const char* block, const char* var, U8& u,
								S32 blocknum)
{
	mMessageReader->getU8(block, var, u, blocknum);
}

void LLMessageSystem::getU8(const char* block, const char* var, U8& u,
							S32 blocknum)
{
	getU8Fast(gMessageStringTable.getString(block),
			  gMessageStringTable.getString(var), u, blocknum);
}

void LLMessageSystem::getBoolFast(const char* block, const char* var, bool& b,
								  S32 blocknum)
{
	mMessageReader->getBool(block, var, b, blocknum);
}

void LLMessageSystem::getBool(const char* block, const char* var, bool& b,
							  S32 blocknum)
{
	getBoolFast(gMessageStringTable.getString(block),
				gMessageStringTable.getString(var), b, blocknum);
}

void LLMessageSystem::getS16Fast(const char* block, const char* var, S16& d,
								 S32 blocknum)
{
	mMessageReader->getS16(block, var, d, blocknum);
}

void LLMessageSystem::getS16(const char* block, const char* var, S16& d,
							 S32 blocknum)
{
	getS16Fast(gMessageStringTable.getString(block),
			   gMessageStringTable.getString(var), d, blocknum);
}

void LLMessageSystem::getU16Fast(const char* block, const char* var, U16& d,
								 S32 blocknum)
{
	mMessageReader->getU16(block, var, d, blocknum);
}

void LLMessageSystem::getU16(const char* block, const char* var, U16& d,
							 S32 blocknum)
{
	getU16Fast(gMessageStringTable.getString(block),
			   gMessageStringTable.getString(var), d, blocknum);
}

void LLMessageSystem::getS32Fast(const char* block, const char* var, S32& d,
								 S32 blocknum)
{
	mMessageReader->getS32(block, var, d, blocknum);
}

void LLMessageSystem::getS32(const char* block, const char* var, S32& d,
							 S32 blocknum)
{
	getS32Fast(gMessageStringTable.getString(block),
			   gMessageStringTable.getString(var), d, blocknum);
}

void LLMessageSystem::getU32Fast(const char* block, const char* var, U32& d,
								 S32 blocknum)
{
	mMessageReader->getU32(block, var, d, blocknum);
}

void LLMessageSystem::getU32(const char* block, const char* var, U32& d,
							 S32 blocknum)
{
	getU32Fast(gMessageStringTable.getString(block),
			   gMessageStringTable.getString(var), d, blocknum);
}

void LLMessageSystem::getU64Fast(const char* block, const char* var, U64& d,
								 S32 blocknum)
{
	mMessageReader->getU64(block, var, d, blocknum);
}

void LLMessageSystem::getU64(const char* block, const char* var, U64& d,
							 S32 blocknum)
{

	getU64Fast(gMessageStringTable.getString(block),
			   gMessageStringTable.getString(var), d, blocknum);
}

void LLMessageSystem::getBinaryDataFast(const char* blockname,
										const char* varname,
										void* datap, S32 size,
										S32 blocknum, S32 max_size)
{
	mMessageReader->getBinaryData(blockname, varname, datap, size, blocknum,
								  max_size);
}

void LLMessageSystem::getBinaryData(const char* blockname,
									const char* varname,
									void* datap, S32 size,
									S32 blocknum, S32 max_size)
{
	getBinaryDataFast(gMessageStringTable.getString(blockname),
					  gMessageStringTable.getString(varname),
					  datap, size, blocknum, max_size);
}

void LLMessageSystem::getF32Fast(const char* block, const char* var, F32& d,
								 S32 blocknum)
{
	mMessageReader->getF32(block, var, d, blocknum);
}

void LLMessageSystem::getF32(const char* block, const char* var, F32& d,
							 S32 blocknum)
{
	getF32Fast(gMessageStringTable.getString(block),
			   gMessageStringTable.getString(var), d, blocknum);
}

void LLMessageSystem::getF64Fast(const char* block, const char* var, F64& d,
								 S32 blocknum)
{
	mMessageReader->getF64(block, var, d, blocknum);
}

void LLMessageSystem::getF64(const char* block, const char* var, F64& d,
							 S32 blocknum)
{
	getF64Fast(gMessageStringTable.getString(block),
				gMessageStringTable.getString(var), d, blocknum);
}

void LLMessageSystem::getVector3Fast(const char* block, const char* var,
									 LLVector3& v, S32 blocknum)
{
	mMessageReader->getVector3(block, var, v, blocknum);
}

void LLMessageSystem::getVector3(const char* block, const char* var,
								 LLVector3& v, S32 blocknum)
{
	getVector3Fast(gMessageStringTable.getString(block),
				   gMessageStringTable.getString(var), v, blocknum);
}

void LLMessageSystem::getVector4Fast(const char* block, const char* var,
									 LLVector4& v, S32 blocknum)
{
	mMessageReader->getVector4(block, var, v, blocknum);
}

void LLMessageSystem::getVector4(const char* block, const char* var,
								 LLVector4& v, S32 blocknum)
{
	getVector4Fast(gMessageStringTable.getString(block),
				   gMessageStringTable.getString(var), v, blocknum);
}

void LLMessageSystem::getVector3dFast(const char* block, const char* var,
									  LLVector3d& v, S32 blocknum)
{
	mMessageReader->getVector3d(block, var, v, blocknum);
}

void LLMessageSystem::getVector3d(const char* block, const char* var,
								  LLVector3d& v, S32 blocknum)
{
	getVector3dFast(gMessageStringTable.getString(block),
				gMessageStringTable.getString(var), v, blocknum);
}

void LLMessageSystem::getQuatFast(const char* block, const char* var,
								  LLQuaternion& q, S32 blocknum)
{
	mMessageReader->getQuat(block, var, q, blocknum);
}

void LLMessageSystem::getQuat(const char* block, const char* var,
							  LLQuaternion& q, S32 blocknum)
{
	getQuatFast(gMessageStringTable.getString(block),
				gMessageStringTable.getString(var), q, blocknum);
}

void LLMessageSystem::getUUIDFast(const char* block, const char* var,
								  LLUUID& u, S32 blocknum)
{
	mMessageReader->getUUID(block, var, u, blocknum);
}

void LLMessageSystem::getUUID(const char* block, const char* var, LLUUID& u,
							  S32 blocknum)
{
	getUUIDFast(gMessageStringTable.getString(block),
				gMessageStringTable.getString(var), u, blocknum);
}

void LLMessageSystem::getIPAddrFast(const char* block, const char* var,
									U32& u, S32 blocknum)
{
	mMessageReader->getIPAddr(block, var, u, blocknum);
}

void LLMessageSystem::getIPAddr(const char* block, const char* var, U32& u,
								S32 blocknum)
{
	getIPAddrFast(gMessageStringTable.getString(block),
				  gMessageStringTable.getString(var), u, blocknum);
}

void LLMessageSystem::getIPPortFast(const char* block, const char* var,
									U16& u, S32 blocknum)
{
	mMessageReader->getIPPort(block, var, u, blocknum);
}

void LLMessageSystem::getIPPort(const char* block, const char* var, U16& u,
								S32 blocknum)
{
	getIPPortFast(gMessageStringTable.getString(block),
				  gMessageStringTable.getString(var), u,
				  blocknum);
}

void LLMessageSystem::getStringFast(const char* block, const char* var,
									S32 buffer_size, char* s, S32 blocknum)
{
	if (buffer_size <= 0)
	{
		llwarns << "buffer_size <= 0" << llendl;
	}
	mMessageReader->getString(block, var, buffer_size, s, blocknum);
}

void LLMessageSystem::getString(const char* block, const char* var,
								S32 buffer_size, char* s, S32 blocknum)
{
	getStringFast(gMessageStringTable.getString(block),
				  gMessageStringTable.getString(var), buffer_size, s,
				  blocknum);
}

void LLMessageSystem::getStringFast(const char* block, const char* var,
									std::string& outstr, S32 blocknum)
{
	mMessageReader->getString(block, var, outstr, blocknum);
}

void LLMessageSystem::getString(const char* block, const char* var,
								std::string& outstr, S32 blocknum)
{
	getStringFast(gMessageStringTable.getString(block),
				  gMessageStringTable.getString(var), outstr,
				  blocknum);
}

bool LLMessageSystem::has(const char* blockname) const
{
	return getNumberOfBlocks(blockname) > 0;
}

S32	LLMessageSystem::getNumberOfBlocksFast(const char* blockname) const
{
	return mMessageReader->getNumberOfBlocks(blockname);
}

S32	LLMessageSystem::getNumberOfBlocks(const char* blockname) const
{
	return getNumberOfBlocksFast(gMessageStringTable.getString(blockname));
}

S32	LLMessageSystem::getSizeFast(const char* blockname,
								 const char* varname) const
{
	return mMessageReader->getSize(blockname, varname);
}

S32	LLMessageSystem::getSize(const char* blockname, const char* varname) const
{
	return getSizeFast(gMessageStringTable.getString(blockname),
					   gMessageStringTable.getString(varname));
}

// size in bytes of variable length data
S32	LLMessageSystem::getSizeFast(const char* blockname, S32 blocknum,
								 const char* varname) const
{
	return mMessageReader->getSize(blockname, blocknum, varname);
}

S32	LLMessageSystem::getSize(const char* blockname, S32 blocknum,
							 const char* varname) const
{
	return getSizeFast(gMessageStringTable.getString(blockname), blocknum,
					   gMessageStringTable.getString(varname));
}

S32 LLMessageSystem::getReceiveSize() const
{
	return mMessageReader->getMessageSize();
}

//static
void LLMessageSystem::setTimeDecodes(bool b)
{
	LLMessageReader::setTimeDecodes(b);
}

//static
void LLMessageSystem::setTimeDecodesSpamThreshold(F32 seconds)
{
	LLMessageReader::setTimeDecodesSpamThreshold(seconds);
}

// *HACK: babbage: return true if message rxed via either UDP or HTTP
// *TODO: babbage: move gServicePump in to LLMessageSystem?
#if LL_USE_FIBER_AWARE_MUTEX
bool LLMessageSystem::checkAllMessages(LockMessageChecker& lmc,
									   S64 frame_count, LLPumpIO* pumpp)
#else
bool LLMessageSystem::checkAllMessages(S64 frame_count, LLPumpIO* pumpp)
#endif
{
	if (!pumpp)
	{
		return false;
	}
#if LL_USE_FIBER_AWARE_MUTEX
	if (lmc.checkMessages(frame_count))
#else
	if (checkMessages(frame_count))
#endif
	{
		return true;
	}
	U32 packetsIn = mPacketsIn;
	pumpp->pump();
	return mPacketsIn - packetsIn > 0;
}

void LLMessageSystem::banUdpMessage(const std::string& name)
{
	template_name_map_t::iterator it =
		mMessageTemplates.find(gMessageStringTable.getString(name.c_str()));
	if (it != mMessageTemplates.end())
	{
		it->second->banUdp();
	}
	else
	{
		llwarns << "Attempted to ban an unknown message: " << name << llendl;
	}
}
const LLHost& LLMessageSystem::getSender() const
{
	return mLastSender;
}

void LLMessageSystem::sendUntrustedSimulatorMessageCoro(const LLHost& host,
														const char* msg_name,
														const LLSD& body,
														UntrustedCallback_t cb)
{
	const std::string& url = host.getUntrustedSimulatorCap();
	if (url.empty())
	{
		llwarns << "Empty capability !" << llendl;
		return;
	}

	LL_DEBUGS("Messaging") << "Sending " << msg_name
						   << " to host " << host.getIPandPort()
						   << " via capability: " << url << LL_ENDL;

	LLSD postdata;
	postdata["message"] = msg_name;
	postdata["body"] = body;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("untrustedSimMessage");
	LLSD result = adapter.postAndSuspend(url, postdata, mHttpOptions);

	if (cb && !cb.empty())
	{
		LLSD results =
			result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		LLCore::HttpStatus status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(results);

		cb(status ? LL_ERR_NOERR : LL_ERR_TCP_TIMEOUT);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLMessageStringTable class (formerly held in message_string_table.cpp)
///////////////////////////////////////////////////////////////////////////////

LL_INLINE U32 message_hash_my_string(const char* str)
{
	U32 retval = 0;
	while (*str++)
	{
		retval += *str;
		retval <<= 1;
	}
	return retval % MESSAGE_NUMBER_OF_HASH_BUCKETS;
}

LLMessageStringTable::LLMessageStringTable()
:	mUsed(0)
{
	for (U32 i = 0; i < MESSAGE_NUMBER_OF_HASH_BUCKETS; ++i)
	{
		mEmpty[i] = true;
		mString[i][0] = 0;
	}
}

char* LLMessageStringTable::getString(const char* str)
{
	U32 hash_value = message_hash_my_string(str);
	while (!mEmpty[hash_value])
	{
		if (!strncmp(str, mString[hash_value], MESSAGE_MAX_STRINGS_LENGTH))
		{
			return mString[hash_value];
		}
		else
		{
			++hash_value;
			hash_value %= MESSAGE_NUMBER_OF_HASH_BUCKETS;
		}
	}
	// Not found, so add it !
	strncpy(mString[hash_value], str, MESSAGE_MAX_STRINGS_LENGTH);
	mString[hash_value][MESSAGE_MAX_STRINGS_LENGTH - 1] = 0;
	mEmpty[hash_value] = false;
	if (++mUsed >= MESSAGE_NUMBER_OF_HASH_BUCKETS - 1)
	{
		llinfos << "Dumping string table before crashing on HashTable full !"
				<< llendl;
		for (U32 i = 0; i < MESSAGE_NUMBER_OF_HASH_BUCKETS; ++i)
		{
			llinfos << "Entry #" << i << ": " << mString[i] << llendl;
		}
	}
	return mString[hash_value];
}

///////////////////////////////////////////////////////////////////////////////
// LLGenericStreamingMessage helper class for Generic Streaming messages
///////////////////////////////////////////////////////////////////////////////

void LLGenericStreamingMessage::unpack(LLMessageSystem* msg)
{
	U16* m = (U16*)&mMethod;	// Squirrely pass enum as U16 by reference
	msg->getU16Fast(_PREHASH_MethodData, _PREHASH_Method, *m);
	constexpr S32 MAX_SIZE = 7 * 1024;
	static char buffer[MAX_SIZE];
	// Note: do not use getStringFast() to avoid 1200 bytes truncation.
	S32 size = msg->getSizeFast(_PREHASH_DataBlock, _PREHASH_Data);
	msg->getBinaryDataFast(_PREHASH_DataBlock, _PREHASH_Data, buffer, size, 0,
						   MAX_SIZE);
	mData.assign(buffer, size);
}
