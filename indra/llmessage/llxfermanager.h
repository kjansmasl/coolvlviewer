/**
 * @file llxfermanager.h
 * @brief definition of LLXferManager class for a keeping track of
 * multiple xfers
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

#ifndef LL_LLXFERMANAGER_H
#define LL_LLXFERMANAGER_H

/**
 * this manager keeps both a send list and a receive list; anything with a
 * LLXferManager can send and receive files via messages
 */

#include <deque>

#include "llassetstorage.h"
#include "lldir.h"				// For ELLPath
#include "llthrottle.h"
#include "llxfer.h"

class LLHostStatus
{
public:
	LLHostStatus()
	:	mNumActive(0),
		mNumPending(0)
	{
	}

	virtual ~LLHostStatus()							{}

public:
	LLHost mHost;
	S32    mNumActive;
	S32    mNumPending;
};

// Class stores ack information, to be put on list so we can throttle xfer
// rate.
class LLXferAckInfo
{
public:
	LLXferAckInfo(U32 dummy = 0)
	{
		mID = 0;
		mPacketNum = -1;
	}

	U64 mID;
	S32 mPacketNum;
	LLHost mRemoteHost;
};

class LLXferManager
{
protected:
	LOG_CLASS(LLXferManager);

public:
	// This enumeration is useful in the requestFile() to specify if an xfer
	// must happen asap.
	enum
	{
		LOW_PRIORITY	= 0,
		HIGH_PRIORITY	= 1,
	};

	LLXferManager();
	~LLXferManager();

	LL_INLINE void setHardLimitOutgoingXfersPerCircuit(S32 max)
	{
		mHardLimitOutgoingXfersPerCircuit = max;
	}

	LL_INLINE void setUseAckThrottling(bool use)
	{
		mUseAckThrottling = use;
	}

	void setAckThrottleBPS(F32 bps);

	// List management routines
	typedef std::deque<LLXfer*> xfer_list_t;
	LLXfer* findXferByID(U64 id, xfer_list_t& list);
	void removeXfer(LLXfer* delp, xfer_list_t& list);
	LLHostStatus* findHostStatus(const LLHost& host);
	S32 numActiveXfers(const LLHost& host);
	S32 numPendingXfers(const LLHost& host);
	void changeNumActiveXfers(const LLHost& host, S32 delta);

	LL_INLINE void setMaxOutgoingXfersPerCircuit(S32 max_num)
	{
		mMaxOutgoingXfersPerCircuit = max_num;
	}

	LL_INLINE void setMaxIncomingXfers(S32 max_num)
	{
		mMaxIncomingXfers = max_num;
	}

	void updateHostStatus();
	void printHostStatus();

	// General utility routines
	void registerCallbacks(LLMessageSystem* mesgsys);
	U64 getNextID();

#if 0	// Not used
	LL_INLINE S32 encodePacketNum(S32 packet_num, bool is_eof)
	{
		return is_eof ? (packet_num | 0x80000000) : packet_num;
	}
#endif

	LL_INLINE S32 decodePacketNum(S32 packet_num)
	{
		return packet_num & 0x0FFFFFFF;
	}

	LL_INLINE bool isLastPacket(S32 packet_num)
	{
		return (packet_num & 0x80000000) != 0;
	}

	// File requesting routine
	U64 requestFile(const std::string& local_filename,
					const std::string& remote_filename,
					ELLPath remote_path, const LLHost& remote_host,
					bool delete_remote_on_completion,
					void (*callback)(void**, S32, LLExtStat), void** user_data,
					bool is_priority = false, bool use_big_packets = false);

	// VFile requesting
	void requestVFile(const LLUUID& local_id, const LLUUID& remote_id,
					  LLAssetType::EType type, const LLHost& remote_host,
					  void (*callback)(void**, S32, LLExtStat),
					  void** user_data, bool is_priority = false);
	// When arbitrary files are requested to be transfered (by giving a dir of
	// LL_PATH_NONE) they must be "expected", but having something pre-
	// authorize them. This pair of functions maintains a pre-authorized list.
	// The first function adds something to the list, the second checks if is
	// authorized, removing it if so.  In this way, a file is only authorized
	// for a single use.
	void expectFileForTransfer(const std::string& filename);
	bool validateFileForTransfer(const std::string& filename);

	// Same idea, but for the viewer about to call InitiateDownload to track
	// what it requested.
	void expectFileForRequest(const std::string& filename);
	bool validateFileForRequest(const std::string& filename);

	void processReceiveData(LLMessageSystem* mesgsys, void** user_data);
	void sendConfirmPacket(LLMessageSystem* mesgsys, U64 id, S32 packetnum,
						   const LLHost& remote_host);

	// File sending routines
	void processFileRequest(LLMessageSystem* mesgsys, void** user_data);
	void processConfirmation(LLMessageSystem* mesgsys, void** user_data);
	void retransmitUnackedPackets();

	// Error handling
	void abortRequestById(U64 xfer_id, S32 result_code);
	void processAbort(LLMessageSystem* mesgsys, void** user_data);
	bool isHostFlooded(const LLHost& host);

protected:
	// Implementation methods
	void startPendingDownloads();
	void addToList(LLXfer* xferp, xfer_list_t& xfer_list, bool is_priority);

public:
	xfer_list_t						mSendList;
	xfer_list_t						mReceiveList;

	typedef std::list<LLHostStatus*> status_list_t;
	status_list_t					mOutgoingHosts;

protected:
	S32								mMaxIncomingXfers;
	S32								mMaxOutgoingXfersPerCircuit;
	// At this limit, kill off the connection
	S32								mHardLimitOutgoingXfersPerCircuit;

	std::deque<LLXferAckInfo>		mXferAckQueue;
	LLThrottle						mAckThrottle;

	// Files that are authorized to transfer out
	std::multiset<std::string>		mExpectedTransfers;
	// Files that are authorized to be downloaded on top of
	std::multiset<std::string>		mExpectedRequests;

	 // Use ack throttling to cap file xfer bandwidth
	bool							mUseAckThrottling;
};

extern LLXferManager* gXferManagerp;

// Message system callbacks
void process_confirm_packet(LLMessageSystem* mesgsys, void** user_data);
void process_request_xfer(LLMessageSystem* mesgsys, void** user_data);
void continue_file_receive(LLMessageSystem* mesgsys, void** user_data);
void process_abort_xfer(LLMessageSystem* mesgsys, void** user_data);

#endif
