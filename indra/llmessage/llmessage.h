/**
 * @file llmessage.h
 * @brief LLMessageSystem class header file
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

#ifndef LL_MESSAGE_H
#define LL_MESSAGE_H

#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "boost/function.hpp"

#if LL_LINUX
# include <endian.h>
# include <netinet/in.h>
#endif

#if LL_WINDOWS
# include "winsock2.h"				// For htons etc.
#endif

#include "llcircuit.h"
#include "llerror.h"
#include "hbfastmap.h"
#include "llhost.h"
#include "llhttpnode.h"
#include "llcorehttpoptions.h"
#include "llsd.h"
#include "llmessagebuilder.h"
#include "llmutex.h"				// For LL_USE_FIBER_AWARE_MUTEX
#include "llpacketack.h"
#include "llpacketring.h"
#include "llstl.h"
#include "llstringtable.h"
#include "lltimer.h"
#include "message_prehash.h"

constexpr U32 MESSAGE_MAX_STRINGS_LENGTH = 64;
constexpr U32 MESSAGE_NUMBER_OF_HASH_BUCKETS = 8192;

constexpr S32 MESSAGE_MAX_PER_FRAME = 400;

class LLMessageStringTable
{
protected:
	LOG_CLASS(LLMessageStringTable);

public:
	LLMessageStringTable();

	char* getString(const char* str);

public:
	U32	 mUsed;
	bool mEmpty[MESSAGE_NUMBER_OF_HASH_BUCKETS];
	char mString[MESSAGE_NUMBER_OF_HASH_BUCKETS][MESSAGE_MAX_STRINGS_LENGTH];
};

extern LLMessageStringTable gMessageStringTable;

// Individual Messages are described with the following format
// Note that to ease parsing, keywords are used
//
//	// Comment 					(Comment like a C++ single line comment)
//  							Comments can only be placed between Messages
// {
// MessageName					(same naming restrictions as C variable)
// Frequency					("High", "Medium", or "Low" - determines whether message ID is 8, 16, or 32-bits --
//								 there can 254 messages in the first 2 groups, 32K in the last group)
//								(A message can be made up only of the Name if it is only a signal)
// Trust						("Trusted", "NotTrusted" - determines if a message will be accepted
//								 on a circuit.  "Trusted" messages are not accepted from NotTrusted circuits
//								 while NotTrusted messages are accepted on any circuit.  An example of a
//								 NotTrusted circuit is any circuit from the viewer.)
// Encoding						("Zerocoded", "Unencoded" - zerocoded messages attempt to compress sequences of
//								 zeros, but if there is no space win, it discards the compression and goes unencoded)
//		{
//		Block Name				(same naming restrictions as C variable)
//		Block Type				("Single", "Multiple", or "Variable" - determines if the block is coded once,
//								 a known number of times, or has a 8 bit argument encoded to tell the decoder
//								 how many times the group is repeated)
//		Block Repeat Number		(Optional - used only with the "Multiple" type - tells how many times the field is repeated
//			{
//			Variable 1 Name		(same naming restrictions as C variable)
//			Variable Type		("Fixed" or "Variable" - determines if the variable is of fixed size or needs to
//								 encode an argument describing the size in bytes)
//			Variable Size		(In bytes, either of the "Fixed" variable itself or of the size argument)
//
//			repeat variables
//
//			}
//
//			Repeat for number of variables in block
//		}
//
//		Repeat for number of blocks in message
// }
// Repeat for number of messages in file
//

// Constants
constexpr S32 MAX_MESSAGE_INTERNAL_NAME_SIZE = 255;
constexpr S32 MAX_BUFFER_SIZE = NET_BUFFER_SIZE;
constexpr S32 MAX_BLOCKS = 255;

constexpr U8 LL_ZERO_CODE_FLAG = 0x80;
constexpr U8 LL_RELIABLE_FLAG = 0x40;
constexpr U8 LL_RESENT_FLAG = 0x20;
constexpr U8 LL_ACK_FLAG = 0x10;

// 1 byte flags, 4 bytes sequence, 1 byte offset + 1 byte message name (high)
constexpr S32 LL_MINIMUM_VALID_PACKET_SIZE = LL_PACKET_ID_SIZE + 1;
enum EPacketHeaderLayout
{
	PHL_FLAGS = 0,
	PHL_PACKET_ID = 1,
	PHL_OFFSET = 5,
	PHL_NAME = 6
};

constexpr U32 LL_DEFAULT_RELIABLE_RETRIES = 3;
constexpr F32 LL_MINIMUM_RELIABLE_TIMEOUT_SECONDS = 1.f;
constexpr F32 LL_MINIMUM_SEMIRELIABLE_TIMEOUT_SECONDS = 1.f;
constexpr F32 LL_PING_BASED_TIMEOUT_DUMMY = 0.f;

// *NOTE: Maybe these factors shouldn't include the msec to sec conversion
// implicitly. However, all units should be MKS.
// factor * averaged ping
constexpr F32 LL_SEMIRELIABLE_TIMEOUT_FACTOR = 5.f / 1000.f;
// factor * averaged ping
constexpr F32 LL_RELIABLE_TIMEOUT_FACTOR = 5.f / 1000.f;
// factor * averaged ping
constexpr F32 LL_FILE_XFER_TIMEOUT_FACTOR = 5.f / 1000.f;
// factor * averaged ping for marking packets "Lost"
constexpr F32 LL_LOST_TIMEOUT_FACTOR = 16.f / 1000.f;
// Maximum amount of time before considering something "lost"
constexpr F32 LL_MAX_LOST_TIMEOUT = 5.f;

constexpr S32 MAX_MESSAGE_COUNT_NUM = 1024;

// Forward declarations
class LLCircuit;
class LLVector3;
class LLVector4;
class LLVector3d;
class LLQuaternion;
class LLUUID;
class LLMessageSystem;
class LLPumpIO;

// Message system exceptional condition handlers.
enum EMessageException
{
	MX_UNREGISTERED_MESSAGE,	// Message number not part of template
	MX_PACKET_TOO_SHORT,		// Invalid, shorter than minimum packet size
	MX_RAN_OFF_END_OF_PACKET,	// Ran off the end of the packet during decode
	MX_WROTE_PAST_BUFFER_SIZE	// Wrote past buffer size in zero code expand
};
typedef void (*msg_exception_callback)(LLMessageSystem*, void*,
									   EMessageException);

// Message data pieces are used to collect the data called for by the message
// template
class LLMsgData;
class LLMsgBlkData;
class LLMessageTemplate;

class LLMessagePollInfo;
class LLMessageBuilder;
class LLTemplateMessageBuilder;
class LLSDMessageBuilder;
class LLMessageReader;
class LLTemplateMessageReader;
class LLSDMessageReader;

class LLUseCircuitCodeResponder
{
protected:
	LOG_CLASS(LLUseCircuitCodeResponder);

public:
	virtual ~LLUseCircuitCodeResponder() = default;
	virtual void complete(const LLHost& host, const LLUUID& agent) const = 0;
};

#if LL_USE_FIBER_AWARE_MUTEX
// SL-12204: We have observed crashes when consumer code sets
// LLMessageSystem::mMessageReader, assuming that all subsequent processing of
// the current message will use the same mMessageReader value, only to have a
// different fiber sneak in and replace mMessageReader before completion. This
// is a limitation of sharing a stateful global resource for message parsing;
// instead code receiving a new message should instantiate a (trivially
// constructed) local message parser and use that.
//
// Until then, when one fiber sets a particular LLMessageReader subclass as the
// current message reader, ensure that no other fiber can replace it until the
// first fiber has finished with its message.
//
// This is achieved with two helper classes. LLMessageSystem::mMessageReader is
// now an LLMessageReaderPointer instance, which can efficiently compare or
// dereference its contained LLMessageReader* but which cannot be directly
// assigned. To change the value of LLMessageReaderPointer, you must
// instantiate LockMessageReader with the LLMessageReader* you wish to make
// current. mMessageReader will have that value for the lifetime of the
// LockMessageReader instance, then revert to NULL. Moreover, as its name
// implies, LockMessageReader locks the mutex in LLMessageReaderPointer so that
// any other fiber instantiating LockMessageReader will block until the first
// fiber has destroyed its instance.

class LLMessageReaderPointer
{
	// Only LockMessageReader can set mPtr.
	friend class LockMessageReader;

public:
	LLMessageReaderPointer()
	:	mPtr(NULL)
	{
	}

	// It is essential that comparison and dereferencing must be fast, which
	// is why we do not check for NULL when dereferencing.
	LL_INLINE LLMessageReader* operator->() const		{ return mPtr; }

	LL_INLINE bool operator==(const LLMessageReader* other) const
	{
		return mPtr == other;
	}

	LL_INLINE bool operator!=(const LLMessageReader* other) const
	{
		return mPtr != other;
	}

private:
	LLMessageReader*	mPtr;
	LL_MUTEX_TYPE		mMutex;
};

// To set mMessageReader to NULL use an anonymous instance that is destroyed
// immediately
// LockMessageReader(gMessageSystem->mMessageReader, NULL);
// Why do we still require going through LockMessageReader at all ? Because it
// would be bad if any fiber set mMessageReader to NULL while another
// fiber was still parsing a message.

class LockMessageReader
{
public:
	LL_INLINE LockMessageReader(LLMessageReaderPointer& var,
								LLMessageReader* instance)
	:	mVar(var.mPtr),
		mLock(var.mMutex)
	{
		mVar = instance;
	}

	LL_INLINE ~LockMessageReader()
	{
		mVar = NULL;
	}

private:
	// Capture a reference to LLMessageReaderPointer::mPtr...
	decltype(LLMessageReaderPointer::mPtr)&	mVar;
	// ...while holding a lock on LLMessageReaderPointer::mMutex
	LL_UNIQ_LOCK_TYPE						mLock;
};

// LockMessageReader is great as long as you only need mMessageReader locked
// during a single LLMessageSystem function call. However, empirically the
// sequence from checkAllMessages() through processAcks() need mMessageReader
// locked to LLTemplateMessageReader. Enforce that by making them require an
// instance of LockMessageChecker.
class LockMessageChecker;
#endif	// LL_USE_FIBER_AWARE_MUTEX

class LLMessageSystem
{
	friend class LLMessageHandlerBridge;
	friend class LockMessageChecker;

protected:
	LOG_CLASS(LLMessageSystem);

public:
	// Read file and build message templates
	LLMessageSystem(const std::string& filename, U32 port, S32 version_major,
					S32 version_minor, S32 version_patch,
					F32 heartbeat_interval, F32 circuit_timeout);

	~LLMessageSystem();

	LL_INLINE bool isOK() const						{ return !mError; }
	LL_INLINE S32 getErrorCode() const				{ return mErrorCode; }

	// Read file and build message templates filename must point to a
	// valid string which specifies the path of a valid linden
	// template.
	void loadTemplateFile(const std::string& filename);


	// Lethods for building, sending, receiving, and handling messages
	void setHandlerFuncFast(const char* name,
							void (*handler_func)(LLMessageSystem*, void**),
							void** user_data = NULL);
	LL_INLINE void setHandlerFunc(const char* name,
								  void (*handler_func)(LLMessageSystem*,
													   void**),
								  void** user_data = NULL)
	{
		setHandlerFuncFast(gMessageStringTable.getString(name),
						   handler_func, user_data);
	}

	// Set a callback function for a message system exception.
	void setExceptionFunc(EMessageException exception,
						  msg_exception_callback func, void* data = NULL);

	// Calls the specified exception func, and returns true if a function was
	// found and called. Otherwise returns false.
	bool callExceptionFunc(EMessageException exception);

	// Set a function that will be called once per packet processed with the
	// hashed message name and the time spent in the processing handler function
	// measured in seconds.  JC
	typedef void (*msg_timing_callback)(const char* hashed_name, F32 time, void* data);
	void setTimingFunc(msg_timing_callback func, void* data = NULL);

	LL_INLINE msg_timing_callback getTimingCallback()
	{
		return mTimingCallback;
	}

	LL_INLINE void* getTimingCallbackData()
	{
		return mTimingCallbackData;
	}

	// This method returns true if the code is in the circuit codes map.
	LL_INLINE bool isCircuitCodeKnown(U32 code) const
	{
		return mCircuitCodes.count(code) != 0;
	}

	// Usually called in response to an AddCircuitCode message, but may also be
	// called by the login process.
	bool addCircuitCode(U32 code, const LLUUID& session_id);

	// Number of seconds that we want to block waiting for data, returns
	// true if data was received:
	bool poll(F32 seconds);

#if LL_USE_FIBER_AWARE_MUTEX
	// Returns true if a valid, on-circuit message has been received.
	bool checkMessages(LockMessageChecker&, S64 frame_count = 0);

	void processAcks(LockMessageChecker&, F32 collect_time = 0.f);
#else
	// Returns true if a valid, on-circuit message has been received.
	bool checkMessages(S64 frame_count = 0);

	void processAcks(F32 collect_time = 0.f);
#endif

	bool isMessageFast(const char* msg);

	LL_INLINE bool isMessage(const char* msg)
	{
		return isMessageFast(gMessageStringTable.getString(msg));
	}

	void dumpPacketToLog();

	char* getMessageName();

	const LLHost& getSender() const;
	// getSender() is preferred
	LL_INLINE U32 getSenderIP() const				{ return mLastSender.getAddress(); }
	// getSender() is preferred
	LL_INLINE U32 getSenderPort() const				{ return mLastSender.getPort(); }

	LL_INLINE const LLHost& getReceivingInterface() const
	{
		return mLastReceivingIF;
	}

	// This method returns the uuid associated with the sender. The
	// UUID will be null if it is not yet known or is a server
	// circuit.
	const LLUUID& getSenderID() const;

	// This method returns the session id associated with the last
	// sender.
	const LLUUID& getSenderSessionID() const;

#if 0	// Unused
	// Set & get the session id
	LL_INLINE void setMySessionID(const LLUUID& id)	{ mSessionID = id; }
	LL_INLINE const LLUUID& getMySessionID()		{ return mSessionID; }
#endif

	void newMessageFast(const char* name);
	void newMessage(const char* name);

	void copyMessageReceivedToSend();
	void clearMessage();

	void nextBlockFast(const char* blockname);
	void nextBlock(const char* blockname);

	void addBinaryDataFast(const char* varname, const void* data, S32 size);
	void addBinaryData(const char* varname, const void* data, S32 size);

	// Typed, checks storage space:
	void addBoolFast(const char* varname, bool b);
	void addBool(const char* varname, bool b);
	void addS8Fast(const char* varname, S8 s);
	void addS8(const char* varname, S8 s);
	void addU8Fast(const char* varname, U8 u);
	void addU8(const char* varname, U8 u);
	void addS16Fast(const char* varname, S16 i);
	void addS16(const char* varname, S16 i);
	void addU16Fast(const char* varname, U16 i);
	void addU16(const char* varname, U16 i);
	void addF32Fast(const char* varname, F32 f);
	void addF32(const char* varname, F32 f);
	void addS32Fast(const char* varname, S32 s);
	void addS32(const char* varname, S32 s);
	void addU32Fast(const char* varname, U32 u);
	void addU32(const char* varname, U32 u);
	void addU64Fast(const char* varname, U64 lu);
	void addU64(const char* varname, U64 lu);
	void addF64Fast(const char* varname, F64 d);
	void addF64(const char* varname, F64 d);
	void addVector3Fast(const char* varname, const LLVector3& vec);
	void addVector3(const char* varname, const LLVector3& vec);
	void addVector4Fast(const char* varname, const LLVector4& vec);
	void addVector4(const char* varname, const LLVector4& vec);
	void addVector3dFast(const char* varname, const LLVector3d& vec);
	void addVector3d(const char* varname, const LLVector3d& vec);
	void addQuatFast(const char* varname, const LLQuaternion& quat);
	void addQuat(const char* varname, const LLQuaternion& quat);
	void addUUIDFast(const char* varname, const LLUUID& uuid);
	void addUUID(const char* varname, const LLUUID& uuid);
	void addIPAddrFast(const char* varname, U32 ip);
	void addIPAddr(const char* varname, U32 ip);
	void addIPPortFast(const char* varname, U16 port);
	void addIPPort(const char* varname, U16 port);
	void addStringFast(const char* varname, const char* s);
	void addString(const char* varname, const char* s);
	void addStringFast(const char* varname, const std::string& s);
	void addString(const char* varname, const std::string& s);

	S32 getCurrentSendTotal() const;
	LL_INLINE TPACKETID getCurrentRecvPacketID()	{ return mCurrentRecvPacketID; }

	// This method checks for current send total and returns true if
	// you need to go to the next block type or need to start a new
	// message. Specify the current blockname to check block counts,
	// otherwise the method only checks against MTU.
	bool isSendFull(const char* blockname = NULL);
	bool isSendFullFast(const char* blockname = NULL);

	// *TODO: Babbage: Remove this horror.
	bool removeLastBlock();

	S32 zeroCode(U8** data, S32* data_size);
	S32 zeroCodeExpand(U8** data, S32* data_size);

	// Uses ping-based retry.
	S32 sendReliable(const LLHost& host, U32 retries_factor = 1);

	// Uses ping-based retry
	LL_INLINE S32 sendReliable(U32 circuit)
	{
		return sendReliable(findHost(circuit));
	}

	// Use this one if you DON'T want automatic ping-based retry.
	S32	sendReliable(const LLHost& host, S32 retries,
					 bool ping_based_retries, F32 timeout,
					 void (*callback)(void**, S32), void** callback_data);

	S32 sendSemiReliable(const LLHost& host, void (*callback)(void**, S32),
						 void** callback_data);

	S32 sendMessage(const LLHost& host);
	// Transmission alias
	LL_INLINE S32 sendMessage(U32 circuit)
	{
		return sendMessage(findHost(circuit));
	}

	/**
	gets binary data from the current message.

	@param blockname the name of the block in the message (from the message
		   template)

	@param varname

	@param datap

	@param size expected size - set to zero to get any amount of data up to
	            max_size. Make sure max_size is set in that case !

	@param blocknum

	@param max_size the max number of bytes to read
	*/
	void getBinaryDataFast(const char* blockname, const char* varname,
						   void* datap, S32 size, S32 blocknum = 0,
						   S32 max_size = S32_MAX);
	void getBinaryData(const char* blockname, const char* varname, void* datap,
					   S32 size, S32 blocknum = 0, S32 max_size = S32_MAX);
	void getBoolFast(const char* block, const char* var, bool& data, S32 blocknum = 0);
	void getBool(const char* block, const char* var, bool& data, S32 blocknum = 0);
	void getS8Fast(const char* block, const char* var, S8& data, S32 blocknum = 0);
	void getS8(const char* block, const char* var, S8& data, S32 blocknum = 0);
	void getU8Fast(const char* block, const char* var, U8& data, S32 blocknum = 0);
	void getU8(const char* block, const char* var, U8& data, S32 blocknum = 0);
	void getS16Fast(const char* block, const char* var, S16& data, S32 blocknum = 0);
	void getS16(const char* block, const char* var, S16& data, S32 blocknum = 0);
	void getU16Fast(const char* block, const char* var, U16& data, S32 blocknum = 0);
	void getU16(const char* block, const char* var, U16& data, S32 blocknum = 0);
	void getS32Fast(const char* block, const char* var, S32& data, S32 blocknum = 0);
	void getS32(const char* block, const char* var, S32& data, S32 blocknum = 0);
	void getF32Fast(const char* block, const char* var, F32& data, S32 blocknum = 0);
	void getF32(const char* block, const char* var, F32& data, S32 blocknum = 0);
	void getU32Fast(const char* block, const char* var, U32& data, S32 blocknum = 0);
	void getU32(const char* block, const char* var, U32& data, S32 blocknum = 0);
	void getU64Fast(const char* block, const char* var, U64& data, S32 blocknum = 0);
	void getU64(const char* block, const char* var, U64& data, S32 blocknum = 0);
	void getF64Fast(const char* block, const char* var, F64& data, S32 blocknum = 0);
	void getF64(const char* block, const char* var, F64& data, S32 blocknum = 0);
	void getVector3Fast(const char* block, const char* var, LLVector3& vec, S32 blocknum = 0);
	void getVector3(const char* block, const char* var, LLVector3& vec, S32 blocknum = 0);
	void getVector4Fast(const char* block, const char* var, LLVector4& vec, S32 blocknum = 0);
	void getVector4(const char* block, const char* var, LLVector4& vec, S32 blocknum = 0);
	void getVector3dFast(const char* block, const char* var, LLVector3d& vec, S32 blocknum = 0);
	void getVector3d(const char* block, const char* var, LLVector3d& vec, S32 blocknum = 0);
	void getQuatFast(const char* block, const char* var, LLQuaternion& q, S32 blocknum = 0);
	void getQuat(const char* block, const char* var, LLQuaternion& q, S32 blocknum = 0);
	void getUUIDFast(const char* block, const char* var, LLUUID& uuid, S32 blocknum = 0);
	void getUUID(const char* block, const char* var, LLUUID& uuid, S32 blocknum = 0);
	void getIPAddrFast(const char* block, const char* var, U32& ip, S32 blocknum = 0);
	void getIPAddr(const char* block, const char* var, U32& ip, S32 blocknum = 0);
	void getIPPortFast(const char* block, const char* var, U16& port, S32 blocknum = 0);
	void getIPPort(const char* block, const char* var, U16 &port, S32 blocknum = 0);
	void getStringFast(const char* block, const char* var, S32 buffer_size, char* buffer, S32 blocknum = 0);
	void getString(const char* block, const char* var, S32 buffer_size, char* buffer, S32 blocknum = 0);
	void getStringFast(const char* block, const char* var, std::string& outstr, S32 blocknum = 0);
	void getString(const char* block, const char* var, std::string& outstr, S32 blocknum = 0);

	void getCircuitInfo(LLSD& info) const;

	LL_INLINE U32 getOurCircuitCode() const			{ return mOurCircuitCode; }

	void enableCircuit(const LLHost& host, bool trusted);
	void disableCircuit(const LLHost& host);

	// Use this to inform a peer that they aren't currently trusted... This now
	// enqueues the request so that we can ensure that we only send one deny
	// per circuit per message loop so that this doesn't become a DoS. The
	// actual sending is done by reallySendDenyTrustedCircuit().
	void sendDenyTrustedCircuit(const LLHost& host);

	// Change this message to be UDP black listed.
	void banUdpMessage(const std::string& name);

	void setCircuitAllowTimeout(const LLHost& host, bool allow);
	void setCircuitTimeoutCallback(const LLHost& host,
								   void (*callback_func)(const LLHost&, void*),
								   void* user_data);

	bool checkCircuitBlocked(U32 circuit);
	bool checkCircuitAlive(U32 circuit);
	bool checkCircuitAlive(const LLHost& host);
	LL_INLINE void setCircuitProtection(bool b)		{ mProtected = b; }
	U32 findCircuitCode(const LLHost& host);
	LLHost findHost(U32 circuit_code);

	bool has(const char* blockname) const;
	S32 getNumberOfBlocksFast(const char* blockname) const;
	S32 getNumberOfBlocks(const char* blockname) const;
	S32 getSizeFast(const char* blockname, const char* varname) const;
	S32 getSize(const char* blockname, const char* varname) const;
	S32 getSizeFast(const char* blockname, S32 blocknum,
					const char* varname) const; // size in bytes of data
	S32 getSize(const char* blockname, S32 blocknum,
				const char* varname) const;

	// Resets receive counts for all message types to 0
	void resetReceiveCounts();
	// Dumps receive count for each message type to llinfos
	void dumpReceiveCounts();

	LL_INLINE bool isClear() const					{ return mMessageBuilder->isClear(); }

	S32 flush(const LLHost& host);

	LL_INLINE U32 getListenPort() const				{ return mPort; }

	void startLogging();                     // starts verbose logging
	void stopLogging();                      // flushes and closes file
	void summarizeLogs(std::ostream& str);   // logs statistics

	S32 getReceiveSize() const;
	LL_INLINE S32 getReceiveCompressedSize() const	{ return mIncomingCompressedSize; }
	LL_INLINE S32 getReceiveBytes() const;

	LL_INLINE S32 getUnackedListSize() const		{ return mUnackedListSize; }
#if 0
	const char* getCurrentSMessageName() const		{ return mCurrentSMessageName; }
	const char* getCurrentSBlockName() const		{ return mCurrentSBlockName; }
#endif

	// Friends
	friend std::ostream& operator<<(std::ostream& s, LLMessageSystem &msg);

	// Max time to process messages before warning and dumping (neg to disable)
	LL_INLINE void setMaxMessageTime(F32 secs)		{ mMaxMessageTime = secs; }
	// Max number of messages before dumping (neg to disable)
	LL_INLINE void setMaxMessageCounts(S32 num)		{ mMaxMessageCounts = num; }

	// Get the current message system time in microseconds
	static U64 getMessageTimeUsecs(bool update = false);
	// Get the current message system time in seconds
	static F64 getMessageTimeSeconds(bool update = false);

	static void setTimeDecodes(bool b);
	static void setTimeDecodesSpamThreshold(F32 seconds);

	// Message handlers internal to the message systesm
	static void processAddCircuitCode(LLMessageSystem* msg, void**);
	static void processUseCircuitCode(LLMessageSystem* msg, void**);
	static void processError(LLMessageSystem* msg, void**);

	// Dispatch llsd message to http node tree
	static void dispatch(const std::string& msg_name, const LLSD& message);
	static void dispatch(const std::string& msg_name, const LLSD& message,
						 LLHTTPNode::ResponsePtr responsep);

	void setMessageBans(const LLSD& trusted, const LLSD& untrusted);

	/**
	 * @brief send an error message to the host. This is a helper method.
	 *
	 * @param host Destination host.
	 * @param agent_id Destination agent id (may be null)
	 * @param code An HTTP status compatible error code.
	 * @param token A specific short string based message
	 * @param id The transactionid/uniqueid/sessionid whatever.
	 * @param system The hierarchical path to the system (255 bytes)
	 * @param message Human readable message (1200 bytes)
	 * @param data Extra info.
	 * @return Returns value returned from sendReliable().
	 */
	S32 sendError(const LLHost& host, const LLUUID& agent_id, S32 code,
				  const std::string& token, const LLUUID& id,
				  const std::string& system, const std::string& message,
				  const LLSD& data);

	// Check UDP messages and pump http_pump to receive HTTP messages.
#if LL_USE_FIBER_AWARE_MUTEX
	bool checkAllMessages(LockMessageChecker&, S64 frame_count,
						  LLPumpIO* pumpp);
#else
	bool checkAllMessages(S64 frame_count, LLPumpIO* pumpp);
#endif

	void setHttpOptionsWithTimeout(U32 timeout);

private:
	LLSD getReceivedMessageLLSD() const;
	LLSD getBuiltMessageLLSD() const;

	// NOTE: Babbage: only use to support legacy misuse of the LLMessageSystem
	// API where values are dangerously written as one type and read as
	// another. LLSD does not support dangerous conversions and so converting
	// the message to an LLSD would result in the reads failing. All code which
	// misuses the message system in this way should be made safe but while the
	// unsafe code is run in old processes, this method should be used to
	// forward unsafe messages.
	LLSD wrapReceivedTemplateData() const;
	LLSD wrapBuiltTemplateData() const;

	void clearReceiveState();

	S32 sendMessage(const LLHost& host, const char* name, const LLSD& message);

	// Really sends the DenyTrustedCircuit message to a given host
	// related to sendDenyTrustedCircuit()
	void reallySendDenyTrustedCircuit(const LLHost& host);

	typedef boost::function<void(S32)> UntrustedCallback_t;
	void sendUntrustedSimulatorMessageCoro(const LLHost& dest_host,
										   const char* msg_name,
										   const LLSD& body,
										   UntrustedCallback_t callback);

	void addTemplate(LLMessageTemplate* templatep);

	void logMsgFromInvalidCircuit(const LLHost& sender, bool recv_reliable);
	void logTrustedMsgFromUntrustedCircuit(const LLHost& sender);
	void logValidMsg(LLCircuitData* cdp, const LLHost& sender,
					 bool recv_reliable, bool recv_resent, bool recv_acks);
	void logRanOffEndOfPacket(const LLHost& sender);

	bool callHandler(const char* name, bool trusted_source = false);

	void init();	// Constuctor shared initialisation.

	// Finds, creates or revives circuit for host as needed
	LLCircuitData* findCircuit(const LLHost& host, bool reset_packet_id);

private:
	U8								mSendBuffer[MAX_BUFFER_SIZE];
	S32								mSendSize;

public:
	// Set this flag to true when you want *very* verbose logs.
	bool							mVerboseLog;

	bool							mProtected;
	// Does the outgoing message require a pos ack ?
	bool							mSendReliable;

	LLPacketRing					mPacketRing;
	LLReliablePacketParams			mReliablePacketParams;

	F32								mMessageFileVersionNumber;
	S32								mSystemVersionMajor;
	S32								mSystemVersionMinor;
	S32								mSystemVersionPatch;
	S32								mSystemVersionServer;
	U32								mVersionFlags;

	U32								mNumberHighFreqMessages;
	U32								mNumberMediumFreqMessages;
	U32								mNumberLowFreqMessages;
	S32								mPort;
	S32								mSocket;

   	// Total packets in, including compressed and uncompressed
	U32								mPacketsIn;
    // Total packets out, including compressed and uncompressed
	U32								mPacketsOut;

   	// Total bytes in, including compressed and uncompressed
	U64								mBytesIn;
   	// Total bytes out, including compressed and uncompressed
	U64								mBytesOut;

	// Total compressed packets in
	U32								mCompressedPacketsIn;
	// Total compressed packets out
	U32								mCompressedPacketsOut;

	// Total reliable packets in
	U32								mReliablePacketsIn;
	// Total reliable packets out
	U32								mReliablePacketsOut;

	// Total dropped packets in
	U32								mDroppedPackets;
	// Total resent packets out
	U32								mResentPackets;
	// Total resend failure packets out
	U32								mFailedResendPackets;
	// Total # of off-circuit packets rejected
	U32								mOffCircuitPackets;
	// Total # of on-circuit but invalid packets rejected
	U32								mInvalidOnCircuitPackets;

	// Total uncompressed size of compressed packets in
	S64								mUncompressedBytesIn;
	// Total uncompressed size of compressed packets out
	S64								mUncompressedBytesOut;
	// Total compressed size of compressed packets in
	S64								mCompressedBytesIn;
	// Total compressed size of compressed packets out
	S64								mCompressedBytesOut;
	// Total size of all uncompressed packets in
	S64								mTotalBytesIn;
	// Total size of all uncompressed packets out
	S64								mTotalBytesOut;

	LLCircuit						mCircuitInfo;

    // Used to print circuit debug info every couple minutes
	F64								mCircuitPrintTime;
	F32								mCircuitPrintFreq;	// In seconds

	U32								mOurCircuitCode;
	S32								mSendPacketFailureCount;
	S32								mUnackedListDepth;
	S32								mUnackedListSize;
	S32								mDSMaxListDepth;

	fast_hmap<U64, U32>				mIPPortToCircuitCode;
	fast_hmap<U32, U64>				mCircuitCodeToIPPort;

	typedef std::map<const char*, LLMessageTemplate*> template_name_map_t;
	typedef fast_hmap<U32, LLMessageTemplate*> template_number_map_t;

private:
	template_name_map_t				mMessageTemplates;
	template_number_map_t			mMessageNumbers;

	LLCore::HttpOptions::ptr_t		mHttpOptions;

	// The mCircuitCodes is a map from circuit codes to session Ids. This
	// allows us to verify sessions on connect.
	typedef fast_hmap<U32, LLUUID> code_session_map_t;
	code_session_map_t				mCircuitCodes;

	// Viewers need to track a process session in order to make sure that no
	// one gives them a bad circuit code.
	LLUUID							mSessionID;

	class LLMessageCountInfo
	{
	public:
		U32 mMessageNum;
		U32 mMessageBytes;
		bool mInvalid;
	};

	LLMessagePollInfo*				mPollInfop;

	U8								mEncodedRecvBuffer[MAX_BUFFER_SIZE];
	U8								mTrueReceiveBuffer[MAX_BUFFER_SIZE];
	S32								mTrueReceiveSize;

	// Must be valid during decode

	bool							mError;
	S32								mErrorCode;
	// The last time we dumped resends
	F64								mResendDumpTime;

	LLMessageCountInfo				mMessageCountList[MAX_MESSAGE_COUNT_NUM];
	S32								mNumMessageCounts;
	F32								mReceiveTime;
	// Max number of seconds for processing messages
	F32								mMaxMessageTime;
	// Max number of messages to process before dumping
	S32								mMaxMessageCounts;
	F64								mMessageCountTime;

	// The current "message system time" (updated the first call to
	// checkMessages after a resetReceiveCount
	F64								mCurrentMessageTimeSeconds;

	// Message system exceptions
	typedef std::pair<msg_exception_callback, void*> exception_t;
	typedef std::map<EMessageException, exception_t> callbacks_t;
	callbacks_t						mExceptionCallbacks;

	// Stuff for logging
	LLTimer							mMessageSystemTimer;

	msg_timing_callback				mTimingCallback;
	void*							mTimingCallbackData;

	LLHost							mLastSender;
	LLHost							mLastReceivingIF;

	// Original size of compressed msg (0 if uncomp)
	S32								mIncomingCompressedSize;
	// Packet ID of current receive packet (for reporting)
	TPACKETID						mCurrentRecvPacketID;

	LLMessageBuilder*				mMessageBuilder;
	LLTemplateMessageBuilder*		mTemplateMessageBuilder;
	LLSDMessageBuilder*				mLLSDMessageBuilder;
#if LL_USE_FIBER_AWARE_MUTEX
	LLMessageReaderPointer			mMessageReader;
#else
	LLMessageReader*				mMessageReader;
#endif
	LLTemplateMessageReader*		mTemplateMessageReader;
	LLSDMessageReader*				mLLSDMessageReader;

	// A list of the circuits that need to be sent DenyTrustedCircuit messages.
	typedef std::set<LLHost> host_set_t;
	host_set_t						mDenyTrustedCircuitSet;
};

#if LL_USE_FIBER_AWARE_MUTEX
// Implementation of LockMessageChecker depends on definition of
// LLMessageSystem, hence must follow it.

class LockMessageChecker: public LockMessageReader
{
public:
	LockMessageChecker(LLMessageSystem* msgsystem);

	// For convenience, provide forwarding wrappers so you can call, e.g.,
	// checkAllMessages() on your LockMessageChecker instance instead of
	// perfect forwarding to avoid having to maintain these wrappers in sync
	// with the target methods.
	template <typename... ARGS>
	LL_INLINE bool checkAllMessages(ARGS&&... args)
	{
		return mMessageSystem->checkAllMessages(*this,
												std::forward<ARGS>(args)...);
	}

	template <typename... ARGS>
	LL_INLINE bool checkMessages(ARGS&&... args)
	{
		return mMessageSystem->checkMessages(*this,
											 std::forward<ARGS>(args)...);
	}

	template <typename... ARGS>
	LL_INLINE void processAcks(ARGS&&... args)
	{
		return mMessageSystem->processAcks(*this, std::forward<ARGS>(args)...);
	}

private:
	LLMessageSystem* mMessageSystem;
};
#endif	// LL_USE_FIBER_AWARE_MUTEX

// External hook into messaging system
extern LLMessageSystem* gMessageSystemp;

// Must specific overall system version, which is used to determine
// if a patch is available in the message template checksum verification.
// Returns true if able to initialize system.
bool start_messaging_system(const std::string& template_name, U32 port,
							S32 ver_major, S32 ver_minor, S32 ver_patch,
							const LLUseCircuitCodeResponder* responder,
							F32 heartbeat_interval, F32 circuit_timeout);

void end_messaging_system(bool print_summary = true);

void null_message_callback(LLMessageSystem* msg, void**data);

///////////////////////////////////////////////////////////////////////////////
// LLGenericStreamingMessage helper class for Generic Streaming messages
///////////////////////////////////////////////////////////////////////////////

class LLGenericStreamingMessage
{
public:
	enum Method : U16
	{
		METHOD_GLTF_MATERIAL_OVERRIDE = 0x4175,
		METHOD_UNKNOWN = 0xFFFF,
	};

	LL_INLINE LLGenericStreamingMessage()
	:	mMethod(METHOD_UNKNOWN)
	{
	}

	void unpack(LLMessageSystem* msg);

public:
	std::string		mData;
	Method			mMethod;
};

//
// Inlines
//

#ifndef LL_BIG_ENDIAN
# error Unknown endianness. Did you omit to include llpreprocessor.h ?
#endif

// Saves from inlining llerrs messages... HB
LL_NO_INLINE void swizzle_size_error(size_t n, size_t s);

static LL_INLINE void* htonmemcpy(void* vs, const void* vct,
								  EMsgVariableType type, size_t n)
{
	char* s = (char*)vs;
	const char* ct = (const char*)vct;
#if LL_BIG_ENDIAN
	S32 i, length;
#endif
	switch (type)
	{
	case MVT_FIXED:
	case MVT_VARIABLE:
	case MVT_U8:
	case MVT_S8:
	case MVT_BOOL:
	case MVT_LLUUID:
	case MVT_IP_ADDR:	// these two are swizzled in the getters and setters
	case MVT_IP_PORT:	// these two are swizzled in the getters and setters
		return(memcpy(s, ct, n));

	case MVT_U16:
	case MVT_S16:
		if (n != 2)
		{
			swizzle_size_error(n, 2);
		}
#if LL_BIG_ENDIAN
		*(s + 1) = *(ct);
		*(s) = *(ct + 1);
		return vs;
#else
		return memcpy(s, ct, n);
#endif

	case MVT_U32:
	case MVT_S32:
	case MVT_F32:
		if (n != 4)
		{
			swizzle_size_error(n, 4);
		}
#if LL_BIG_ENDIAN
		*(s + 3) = *(ct);
		*(s + 2) = *(ct + 1);
		*(s + 1) = *(ct + 2);
		*(s) = *(ct + 3);
		return vs;
#else
		return memcpy(s, ct, n);
#endif

	case MVT_U64:
	case MVT_S64:
	case MVT_F64:
		if (n != 8)
		{
			swizzle_size_error(n, 8);
		}
#if LL_BIG_ENDIAN
		*(s + 7) = *(ct);
		*(s + 6) = *(ct + 1);
		*(s + 5) = *(ct + 2);
		*(s + 4) = *(ct + 3);
		*(s + 3) = *(ct + 4);
		*(s + 2) = *(ct + 5);
		*(s + 1) = *(ct + 6);
		*s = *(ct + 7);
		return vs;
#else
		return memcpy(s, ct, n);
#endif

	case MVT_LLVector3:
	case MVT_LLQuaternion:  // We only send x, y, z and infer w (we set x, y, z to ensure that w >= 0)
		if (n != 12)
		{
			swizzle_size_error(n, 12);
		}
#if LL_BIG_ENDIAN
		htonmemcpy(s + 8, ct + 8, MVT_F32, 4);
		htonmemcpy(s + 4, ct + 4, MVT_F32, 4);
		return htonmemcpy(s, ct, MVT_F32, 4);
#else
		return memcpy(s, ct, n);
#endif

	case MVT_LLVector3d:
		if (n != 24)
		{
			swizzle_size_error(n, 24);
		}
#if LL_BIG_ENDIAN
		htonmemcpy(s + 16, ct + 16, MVT_F64, 8);
		htonmemcpy(s + 8, ct + 8, MVT_F64, 8);
		return htonmemcpy(s, ct, MVT_F64, 8);
#else
		return memcpy(s, ct, n);
#endif

	case MVT_LLVector4:
		if (n != 16)
		{
			swizzle_size_error(n, 16);
		}
#if LL_BIG_ENDIAN
		htonmemcpy(s + 12, ct + 12, MVT_F32, 4);
		htonmemcpy(s + 8, ct + 8, MVT_F32, 4);
		htonmemcpy(s + 4, ct + 4, MVT_F32, 4);
		return htonmemcpy(s, ct, MVT_F32, 4);
#else
		return memcpy(s, ct, n);
#endif

	case MVT_U16Vec3:
		if (n != 6)
		{
			swizzle_size_error(n, 6);
		}
#if LL_BIG_ENDIAN
		htonmemcpy(s + 4, ct + 4, MVT_U16, 2);
		htonmemcpy(s + 2, ct + 2, MVT_U16, 2);
		return htonmemcpy(s, ct, MVT_U16, 2);
#else
		return memcpy(s, ct, n);
#endif

	case MVT_U16Quat:
		if (n != 8)
		{
			swizzle_size_error(n, 8);
		}
#if LL_BIG_ENDIAN
		htonmemcpy(s + 6, ct + 6, MVT_U16, 2);
		htonmemcpy(s + 4, ct + 4, MVT_U16, 2);
		htonmemcpy(s + 2, ct + 2, MVT_U16, 2);
		return htonmemcpy(s, ct, MVT_U16, 2);
#else
		return memcpy(s, ct, n);
#endif

	case MVT_S16Array:
		if (n % 2)
		{
			swizzle_size_error(n, n + 1);
		}
#if LL_BIG_ENDIAN
		length = n % 2;
		for (i = 1; i < length; ++i)
		{
			htonmemcpy(s + i * 2, ct + i * 2, MVT_S16, 2);
		}
		return htonmemcpy(s, ct, MVT_S16, 2);
#else
		return memcpy(s, ct, n);
#endif

	default:
		return memcpy(s, ct, n);
	}
}

LL_INLINE void* ntohmemcpy(void* s, const void* ct, EMsgVariableType type,
						   size_t n)
{
	return htonmemcpy(s, ct, type, n);
}

#endif
