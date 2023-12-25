/**
 * @file llxfermanager.cpp
 * @brief implementation of LLXferManager class for a collection of xfers
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

#include "llxfermanager.h"

#include "llstring.h"
#include "llxfer.h"
#include "llxfer_file.h"
#include "llxfer_mem.h"
#include "llxfer_vfile.h"

// Timeout if a registered transfer has not been requested in 60 seconds:
constexpr F32 LL_XFER_REGISTRATION_TIMEOUT = 60.f;
constexpr F32 LL_PACKET_TIMEOUT = 3.f;		// Packet timeout at 3s
constexpr S32 LL_PACKET_RETRY_LIMIT = 10;	// Packet retransmission limit
constexpr S32 LL_DEFAULT_MAX_SIMULTANEOUS_XFERS = 10;
constexpr S32 LL_DEFAULT_MAX_REQUEST_FIFO_XFERS = 1000;
// Kills the connection if a viewer download queue hits this many requests
// backed up:
constexpr S32 LL_DEFAULT_MAX_HARD_LIMIT_SIMULTANEOUS_XFERS = 500;

LLXferManager* gXferManagerp = NULL;

LLXferManager::LLXferManager()
{
	setMaxOutgoingXfersPerCircuit(LL_DEFAULT_MAX_SIMULTANEOUS_XFERS);
	setHardLimitOutgoingXfersPerCircuit(LL_DEFAULT_MAX_HARD_LIMIT_SIMULTANEOUS_XFERS);
	setMaxIncomingXfers(LL_DEFAULT_MAX_REQUEST_FIFO_XFERS);

	// Turn on or off ack throttling
	mUseAckThrottling = false;
	setAckThrottleBPS(100000);
}

LLXferManager::~LLXferManager()
{
	for_each(mOutgoingHosts.begin(), mOutgoingHosts.end(), DeletePointer());
	mOutgoingHosts.clear();

	for_each(mSendList.begin(), mSendList.end(), DeletePointer());
	mSendList.clear();

	for_each(mReceiveList.begin(), mReceiveList.end(), DeletePointer());
	mReceiveList.clear();
}

void LLXferManager::setAckThrottleBPS(F32 bps)
{
	// Let's figure out the min we can set based on the ack retry rate and
	// number of simultaneous.

	// Assuming we're running as slow as possible, this is the lowest ack
	// rate we can use.
	F32 min_bps = 8000.f * mMaxIncomingXfers / LL_PACKET_TIMEOUT;

	// Set
	F32 actual_rate = llmax(min_bps * 1.1f, bps);
	LL_DEBUGS("AppInit") << "LLXferManager ack throttle min rate: "
						 << min_bps << " - actual rate: " << actual_rate
						 << LL_ENDL;
	mAckThrottle.setRate(actual_rate);
}

void LLXferManager::updateHostStatus()
{
	// Clear the outgoing host list
	for_each(mOutgoingHosts.begin(), mOutgoingHosts.end(), DeletePointer());
	mOutgoingHosts.clear();

	// Loop through all outgoing xfers and re-build mOutgoingHosts
	for (xfer_list_t::iterator send_iter = mSendList.begin(),
							   send_end = mSendList.end();
		 send_iter != send_end; ++send_iter)
	{
		const LLHost& remote_host = (*send_iter)->mRemoteHost;
		LLHostStatus* host_statusp = NULL;
		for (status_list_t::iterator iter = mOutgoingHosts.begin(),
									 end = mOutgoingHosts.end();
			 iter != end; ++iter)
		{
			if (*iter && (*iter)->mHost == remote_host)
			{
				host_statusp = *iter;
				break;
			}
		}
		if (!host_statusp)
		{
			// We do not have this host, so add it
			host_statusp = new LLHostStatus();
			if (host_statusp)
			{
				host_statusp->mHost = remote_host;
				mOutgoingHosts.push_front(host_statusp);
			}
		}
		if (host_statusp)
		{
			// Do the accounting
			if ((*send_iter)->mStatus == e_LL_XFER_PENDING)
			{
				++host_statusp->mNumPending;
			}
			else if ((*send_iter)->mStatus == e_LL_XFER_IN_PROGRESS)
			{
				++host_statusp->mNumActive;
			}
		}
	}
}

void LLXferManager::printHostStatus()
{
	LLHostStatus* host_statusp = NULL;
	if (!mOutgoingHosts.empty())
	{
		llinfos << "Outgoing Xfers:" << llendl;

		for (status_list_t::iterator iter = mOutgoingHosts.begin(),
									 end = mOutgoingHosts.end();
			 iter != end; ++iter)
		{
			host_statusp = *iter;
			llinfos << "    " << host_statusp->mHost << " - active: "
					<< host_statusp->mNumActive << " - pending: "
					<< host_statusp->mNumPending << llendl;
		}
	}
}

LLXfer* LLXferManager::findXferByID(U64 id, xfer_list_t& xfer_list)
{
	for (xfer_list_t::iterator iter = xfer_list.begin(), end = xfer_list.end();
		 iter != end; ++iter)
	{
		if ((*iter)->mID == id)
		{
			return *iter;
		}
	}
	return NULL;
}

// This method assumes that delp will only occur in the list zero or one time.
void LLXferManager::removeXfer(LLXfer* delp, xfer_list_t& xfer_list)
{
	if (!delp)
	{
		return;
	}

	bool receiving = &xfer_list == &mReceiveList;
	for (xfer_list_t::iterator iter = xfer_list.begin(), end = xfer_list.end();
		 iter != end; ++iter)
	{
		if (*iter == delp)
		{
			LL_DEBUGS("FileTransfer") << "Deleting xfer to host "
									  << (*iter)->mRemoteHost
									  << " of " << (*iter)->mXferSize
									  << " bytes, status "
									  << (S32)(*iter)->mStatus << " from the "
									  << (receiving ? "receive" : "send")
									  << " list" << LL_ENDL;
			xfer_list.erase(iter);
			delete delp;
			break;
		}
	}
}

LLHostStatus* LLXferManager::findHostStatus(const LLHost& host)
{
	for (status_list_t::iterator iter = mOutgoingHosts.begin(),
								 end = mOutgoingHosts.end();
		 iter != end; ++iter)
	{
		LLHostStatus* host_statusp = *iter;
		if (host_statusp && host_statusp->mHost == host)
		{
			return host_statusp;
		}
	}
	return NULL;
}

S32 LLXferManager::numPendingXfers(const LLHost& host)
{
	LLHostStatus* host_statusp = findHostStatus(host);
	return host_statusp ? host_statusp->mNumPending : 0;
}

S32 LLXferManager::numActiveXfers(const LLHost& host)
{
	LLHostStatus* host_statusp = findHostStatus(host);
	return host_statusp ? host_statusp->mNumActive : 0;
}

void LLXferManager::changeNumActiveXfers(const LLHost& host, S32 delta)
{
	for (status_list_t::iterator iter = mOutgoingHosts.begin(),
								 end = mOutgoingHosts.end();
		 iter != end; ++iter)
	{
		LLHostStatus* host_statusp = *iter;
		if (host_statusp && host_statusp->mHost == host)
		{
			host_statusp->mNumActive += delta;
		}
	}
}

void LLXferManager::registerCallbacks(LLMessageSystem* msg)
{
	msg->setHandlerFuncFast(_PREHASH_ConfirmXferPacket, process_confirm_packet,
							NULL);
	msg->setHandlerFuncFast(_PREHASH_RequestXfer, process_request_xfer, NULL);
	msg->setHandlerFuncFast(_PREHASH_SendXferPacket, continue_file_receive,
							NULL);
	msg->setHandlerFuncFast(_PREHASH_AbortXfer, process_abort_xfer, NULL);
}

U64 LLXferManager::getNextID()
{
	LLUUID a_guid;
	a_guid.generate();
	return *((U64*)a_guid.mData);
}

U64 LLXferManager::requestFile(const std::string& local_filename,
							   const std::string& remote_filename,
							   ELLPath remote_path, const LLHost& remote_host,
							   bool delete_remote_on_completion,
							   void (*callback)(void**, S32, LLExtStat),
							   void** user_data, bool is_priority,
							   bool use_big_packets)
{
	LLXfer_File* file_xferp = NULL;

	// First check to see if it's already requested
	for (xfer_list_t::iterator iter = mReceiveList.begin(),
							   end = mReceiveList.end();
		 iter != end; ++iter)
	{
		if (!*iter || (*iter)->getXferTypeTag() != LLXfer::XFER_FILE)
		{
			continue;
		}
		file_xferp = (LLXfer_File*)*iter;
		if (file_xferp->matchesLocalFilename(local_filename) &&
			file_xferp->matchesRemoteFilename(remote_filename, remote_path) &&
			remote_host == file_xferp->mRemoteHost &&
			callback == file_xferp->mCallback &&
			user_data == file_xferp->mCallbackDataHandle)
		{
			LL_DEBUGS("FileTransfer") << "Requested a xfer already in progress"
									  << LL_ENDL;
			return file_xferp->mID;
		}
	}

	U64 xfer_id = 0;

	S32 chunk_size = use_big_packets ? LL_XFER_LARGE_PAYLOAD : -1;
	file_xferp = new LLXfer_File(chunk_size);
	if (file_xferp)
	{
		addToList(file_xferp, mReceiveList, is_priority);

		// Remove any file by the same name that happens to be lying around.
		// Note: according to AaronB, this is here to deal with locks on files
		// that were in transit during a crash.
		if (delete_remote_on_completion &&
			remote_filename.substr(remote_filename.length() - 4) == ".tmp" &&
			LLFile::exists(local_filename))
		{
			LLFile::remove(local_filename);
		}
		xfer_id = getNextID();
		file_xferp->initializeRequest(xfer_id, local_filename, remote_filename,
									  remote_path, remote_host,
									  delete_remote_on_completion, callback,
									  user_data);
		startPendingDownloads();
	}
	else
	{
		llwarns << "LLXfer allocation error: out of memory ?" << llendl;
		llassert(false);
	}

	return xfer_id;
}

void LLXferManager::requestVFile(const LLUUID& local_id,
								 const LLUUID& remote_id,
								 LLAssetType::EType type,
								 const LLHost& remote_host,
								 void (*callback)(void**, S32, LLExtStat),
								 void** user_data,
								 bool is_priority)
{
	LLXfer_VFile* xferp = NULL;
	for (xfer_list_t::iterator iter = mReceiveList.begin(),
							   end = mReceiveList.end();
		 iter != end; ++iter)
	{
		if (!*iter || (*iter)->getXferTypeTag() != LLXfer::XFER_VFILE)
		{
			continue;
		}
		xferp = (LLXfer_VFile*)*iter;
		// Find any matching existing requests
		if (xferp->matchesLocalFile(local_id, type) &&
			xferp->matchesRemoteFile(remote_id, type) &&
			remote_host == xferp->mRemoteHost &&
			callback == xferp->mCallback &&
			user_data == xferp->mCallbackDataHandle)
		{
			// We have match, so do not add a duplicate
			LL_DEBUGS("FileTransfer") << "Requested a xfer already in progress"
									  << LL_ENDL;
			return;
		}
	}

	xferp = new LLXfer_VFile();
	if (xferp)
	{
		addToList(xferp, mReceiveList, is_priority);
		xferp->initializeRequest(getNextID(), local_id, remote_id, type,
								 remote_host, callback, user_data);
		startPendingDownloads();
	}
	else
	{
		llwarns << "Xfer allocation error: out of memory ?" << llendl;
		llassert(false);
	}
}

void LLXferManager::processReceiveData(LLMessageSystem* msg, void**)
{
	U64 id;
	msg->getU64Fast(_PREHASH_XferID, _PREHASH_ID, id);
	S32 packetnum;
	msg->getS32Fast(_PREHASH_XferID, _PREHASH_Packet, packetnum);

	// There is sometimes an extra 4 bytes added to an xfer payload
	constexpr S32 BUF_SIZE = LL_XFER_LARGE_PAYLOAD + 4;

	S32 fdata_size = msg->getSizeFast(_PREHASH_DataPacket, _PREHASH_Data);
	if (fdata_size < 0 || fdata_size > BUF_SIZE)
	{
		char U64_BUF[MAX_STRING];
		llwarns << "Received invalid xfer data size of " << fdata_size
				<< " in packet number " << packetnum << " from "
				<< msg->getSender() << " for xfer Id: "
				<< U64_to_str(id, U64_BUF, sizeof(U64_BUF)) << llendl;
		return;
	}

	char fdata_buf[BUF_SIZE];
	msg->getBinaryDataFast(_PREHASH_DataPacket, _PREHASH_Data, fdata_buf, 0, 0,
						   BUF_SIZE);

	LLXfer* xferp = findXferByID(id, mReceiveList);
	if (!xferp)
	{
		char U64_BUF[MAX_STRING];
		llwarns << "received xfer data from " << msg->getSender()
			<< " for non-existent xfer id: "
			<< U64_to_str(id, U64_BUF, sizeof(U64_BUF)) << llendl;
		return;
	}

	// Is the packet different from what we were expecting ?
	if (decodePacketNum(packetnum) != xferp->mPacketNum)
	{
		// Confirm it if it was a resend of the last one, since the
		// confirmation might have gotten dropped
		if (decodePacketNum(packetnum) == xferp->mPacketNum - 1)
		{
			llinfos << "Reconfirming xfer " << xferp->mRemoteHost << ":"
					<< xferp->getFileName() << " packet " << packetnum
					<< llendl;
			sendConfirmPacket(msg, id, decodePacketNum(packetnum),
							  msg->getSender());
		}
		else
		{
			llinfos << "Ignoring xfer " << xferp->mRemoteHost << ":"
					<< xferp->getFileName() << " received packet " << packetnum
					<< "; expecting " << xferp->mPacketNum << llendl;
		}
		return;
	}

	S32 result = 0;

	// First packet has size encoded as additional S32 at beginning of data
	if (xferp->mPacketNum == 0)
	{
		S32 xfer_size;
		ntohmemcpy(&xfer_size, fdata_buf, MVT_S32, sizeof(S32));

		// Do any necessary things on first packet ie. allocate memory
		xferp->setXferSize(xfer_size);

		// Adjust buffer start and size
		result = xferp->receiveData(&fdata_buf[sizeof(S32)],
									fdata_size - sizeof(S32));
	}
	else
	{
		result = xferp->receiveData(fdata_buf, fdata_size);
	}

	if (result == LL_ERR_CANNOT_OPEN_FILE)
	{
		xferp->abort(LL_ERR_CANNOT_OPEN_FILE);
		removeXfer(xferp, mReceiveList);
		startPendingDownloads();
		return;
	}

	++xferp->mPacketNum;  // Expect next packet

	if (!mUseAckThrottling)
	{
		// No throttling, confirm right away
		sendConfirmPacket(msg, id, decodePacketNum(packetnum),
						  msg->getSender());
	}
	else
	{
		// Throttling, put on queue to be confirmed later.
		LLXferAckInfo ack_info;
		ack_info.mID = id;
		ack_info.mPacketNum = decodePacketNum(packetnum);
		ack_info.mRemoteHost = msg->getSender();
		mXferAckQueue.push_back(ack_info);
	}

	if (isLastPacket(packetnum))
	{
		xferp->processEOF();
		removeXfer(xferp, mReceiveList);
		startPendingDownloads();
	}
}

void LLXferManager::sendConfirmPacket(LLMessageSystem* msg, U64 id,
									  S32 packetnum, const LLHost& remote_host)
{
	msg->newMessageFast(_PREHASH_ConfirmXferPacket);
	msg->nextBlockFast(_PREHASH_XferID);
	msg->addU64Fast(_PREHASH_ID, id);
	msg->addU32Fast(_PREHASH_Packet, packetnum);

	// Ignore a circuit failure here; we will catch it with another message.
	msg->sendMessage(remote_host);
}

static bool find_and_remove(std::multiset<std::string>& files,
							const std::string& filename)
{
	std::multiset<std::string>::iterator ptr;
	if ((ptr = files.find(filename)) != files.end())
	{
		// erase(filename) erases *all* entries with that key
		files.erase(ptr);
		return true;
	}
	return false;
}

void LLXferManager::expectFileForRequest(const std::string& filename)
{
	mExpectedRequests.emplace(filename);
}

bool LLXferManager::validateFileForRequest(const std::string& filename)
{
	return find_and_remove(mExpectedRequests, filename);
}

void LLXferManager::expectFileForTransfer(const std::string& filename)
{
	mExpectedTransfers.emplace(filename);
}

bool LLXferManager::validateFileForTransfer(const std::string& filename)
{
	return find_and_remove(mExpectedTransfers, filename);
}

static bool remove_prefix(std::string& filename, const std::string& prefix)
{
	if (std::equal(prefix.begin(), prefix.end(), filename.begin()))
	{
		filename = filename.substr(prefix.length());
		return true;
	}
	return false;
}

// NOTE: This function is only used to check file names that our own code
// places in the cache directory. As such, it can be limited to this very
// restrictive file name pattern. It does not need to handle other characters.
static bool verify_cache_filename(const std::string& filename)
{
	size_t len = filename.size();
	if (len < 1 || len > 50)
	{
		return false;
	}
	for (U32 i = 0; i < len; ++i)
	{
		char c = filename[i];
		bool ok = isalnum(c);
		if (!ok && i > 0)
		{
			ok = '_' == c || '-' == c || '.' == c;
		}
		if (!ok)
		{
			return false;
		}
	}
	return true;
}

void LLXferManager::processFileRequest(LLMessageSystem* msg, void**)
{
	S32 result = LL_ERR_NOERR;

	bool use_big_pkts;
	msg->getBool("XferID", "UseBigPackets", use_big_pkts);

	U64 id;
	msg->getU64Fast(_PREHASH_XferID, _PREHASH_ID, id);
	char U64_BUF[MAX_STRING];
	llinfos << "xfer request id: " << U64_to_str(id, U64_BUF, sizeof(U64_BUF))
			<< " to " << msg->getSender() << llendl;

	std::string local_filename;
	msg->getStringFast(_PREHASH_XferID, _PREHASH_Filename, local_filename);

	U8 local_path_u8;
	msg->getU8("XferID", "FilePath", local_path_u8);
	ELLPath local_path = (ELLPath)local_path_u8;

	LLUUID	uuid;
	msg->getUUIDFast(_PREHASH_XferID, _PREHASH_VFileID, uuid);
	S16 type_s16;
	msg->getS16Fast(_PREHASH_XferID, _PREHASH_VFileType, type_s16);
	LLAssetType::EType type = (LLAssetType::EType)type_s16;

	LLXfer* xferp;

	if (uuid.notNull())
	{
		// Request for an asset: use a cache file
		if (!LLAssetType::lookup(type))
		{
			llwarns << "Invalid type for xfer request: " << uuid << ":"
					<< type_s16 << " to " << msg->getSender() << llendl;
			return;
		}

		llinfos << "starting vfile transfer: " << uuid << ","
				<< LLAssetType::lookup(type) << " to " << msg->getSender()
				<< llendl;

		xferp = (LLXfer*)new LLXfer_VFile(uuid, type);
		if (xferp)
		{
			mSendList.push_front(xferp);
			result = xferp->startSend(id, msg->getSender());
		}
		else
		{
			llwarns << "Xfer allocation error: out of memory ?" << llendl;
			llassert(false);
		}
	}
	else if (!local_filename.empty())
	{
		// Was given a file name to send. See DEV-21775 for detailed security
		// issues
		if (local_path == LL_PATH_NONE)
		{
			// This handles legacy simulators that are passing objects by
			// giving a filename that explicitly names the cache directory
			static const std::string legacy_cache_prefix = "data/";
			if (remove_prefix(local_filename, legacy_cache_prefix))
			{
				local_path = LL_PATH_CACHE;
			}
		}

		switch (local_path)
		{
			case LL_PATH_NONE:
				if (!validateFileForTransfer(local_filename))
				{
					llwarns << "SECURITY: Unapproved filename '"
							<< local_filename << llendl;
					return;
				}
				break;

			case LL_PATH_CACHE:
				if (!verify_cache_filename(local_filename))
				{
					llwarns << "SECURITY: Illegal cache filename '"
							<< local_filename << llendl;
					return;
				}
				break;

			default:
				llwarns << "SECURITY: Restricted file dir enum: "
						<< (U32)local_path << llendl;
				return;
		}


		// If we want to use a special path (e.g. LL_PATH_CACHE), we want to
		// make sure we create the proper expanded filename.
		std::string expanded_filename;
		if (local_path != LL_PATH_NONE)
		{
			expanded_filename = gDirUtilp->getExpandedFilename(local_path,
															   local_filename);
		}
		else
		{
			expanded_filename = local_filename;
		}
		llinfos << "starting file transfer: " <<  expanded_filename << " to "
				<< msg->getSender() << llendl;

		bool delete_local_on_completion = false;
		msg->getBool("XferID", "DeleteOnCompletion",
					 delete_local_on_completion);

		// -1 chunk_size causes it to use the default
		xferp = (LLXfer*)new LLXfer_File(expanded_filename,
										 delete_local_on_completion,
										 use_big_pkts ? LL_XFER_LARGE_PAYLOAD
													  : -1);
		if (xferp)
		{
			mSendList.push_front(xferp);
			result = xferp->startSend(id, msg->getSender());
		}
		else
		{
			llwarns << "Xfer allocation error: out of memory ?" << llendl;
			llassert(false);
		}
	}
	else
	{
		// No UUID or filename; use the Id sent.
		char U64_BUF[MAX_STRING];
		llinfos << "Starting memory transfer: "
				<< U64_to_str(id, U64_BUF, sizeof(U64_BUF)) << " to "
				<< msg->getSender() << llendl;

		xferp = findXferByID(id, mSendList);
		if (xferp)
		{
			result = xferp->startSend(id, msg->getSender());
		}
		else
		{
			llwarns << U64_BUF << " not found." << llendl;
			result = LL_ERR_FILE_NOT_FOUND;
		}
	}

	if (result)
	{
		if (xferp)
		{
			xferp->abort(result);
			removeXfer(xferp, mSendList);
		}
		else	// Can happen with a memory transfer not found
		{
			llinfos << "Aborting xfer to " << msg->getSender()
					<< " with error: " << result << llendl;

			msg->newMessageFast(_PREHASH_AbortXfer);
			msg->nextBlockFast(_PREHASH_XferID);
			msg->addU64Fast(_PREHASH_ID, id);
			msg->addS32Fast(_PREHASH_Result, result);
			msg->sendMessage(msg->getSender());
		}
	}
	else if (xferp)
	{
		// Figure out how many transfers the host has requested
		LLHostStatus* host_statusp = findHostStatus(xferp->mRemoteHost);
		if (host_statusp)
		{
			if (host_statusp->mNumActive < mMaxOutgoingXfersPerCircuit)
			{
				// Not many transfers in progress already, so start immediately
				xferp->sendNextPacket();
				changeNumActiveXfers(xferp->mRemoteHost, 1);
				LL_DEBUGS("FileTransfer")  << "Starting xfer immediately"
										   << LL_ENDL;
			}
			else if (!mHardLimitOutgoingXfersPerCircuit ||
					 host_statusp->mNumActive + host_statusp->mNumPending <
						mHardLimitOutgoingXfersPerCircuit)
			{
				// Must close the file handle and wait for earlier ones to
				// complete
				llinfos << "Queueing xfer request Id " << U64_to_str(id)
						<< ", " << host_statusp->mNumActive << " active and "
						<<  host_statusp->mNumPending
						<< " pending ahead of this one" << llendl;
				xferp->closeFileHandle();
			}
			else if (mHardLimitOutgoingXfersPerCircuit > 0)
			{
				// Way too many requested ... It is time to stop being nice and
				// kill the circuit.
				xferp->closeFileHandle();
				LLCircuitData* cdp =
					msg->mCircuitInfo.findCircuit(xferp->mRemoteHost);
				if (cdp && cdp->getTrusted())
				{
					// Trusted internal circuit: do not kill it
					llwarns << "Trusted circuit to " << xferp->mRemoteHost
							<< " has too many xfer requests in the queue: "
							<< host_statusp->mNumActive << " active and "
							<<  host_statusp->mNumPending
							<< " pending ahead of this one" << llendl;
				}
				else
				{
					llwarns << "Killing " << (cdp ? "active" : "missing (!)")
							<< " circuit to " << xferp->mRemoteHost
							<< " for having too many xfer requests queued: "
							<< host_statusp->mNumActive << " active and "
							<<  host_statusp->mNumPending
							<< " pending ahead of this one" << llendl;
					msg->disableCircuit(xferp->mRemoteHost);
				}
			}
		}
		else
		{
			llwarns << "No LLHostStatus found for Id " << U64_to_str(id)
					<<  " and host " << xferp->mRemoteHost << llendl;
		}
	}
	else
	{
		llwarns << "No xfer found for Id " << U64_to_str(id) << llendl;
	}
}

// Returns true if host is in a transfer-flood sitation. Same check for both
// internal and external hosts.
bool LLXferManager::isHostFlooded(const LLHost& host)
{
	LLHostStatus* host_statusp = findHostStatus(host);
	return host_statusp && mHardLimitOutgoingXfersPerCircuit > 0 &&
		   host_statusp->mNumActive + host_statusp->mNumPending >=
				80 * mHardLimitOutgoingXfersPerCircuit / 100;
}

void LLXferManager::processConfirmation(LLMessageSystem* msg, void**)
{
	U64 id = 0;
	S32 packetNum = 0;

	msg->getU64Fast(_PREHASH_XferID, _PREHASH_ID, id);
	msg->getS32Fast(_PREHASH_XferID, _PREHASH_Packet, packetNum);

	LLXfer* xferp = findXferByID(id, mSendList);
	if (xferp)
	{
		xferp->mWaitingForACK = false;
		if (xferp->mStatus == e_LL_XFER_IN_PROGRESS)
		{
			xferp->sendNextPacket();
		}
		else
		{
			removeXfer(xferp, mSendList);
		}
	}
}

void LLXferManager::retransmitUnackedPackets()
{
	LLXfer* xferp;
	xfer_list_t::iterator iter = mReceiveList.begin();
	while (iter != mReceiveList.end())
	{
		xferp = *iter;
		if (!xferp)	// Paranoia
		{
			iter = mReceiveList.erase(iter);
			continue;
		}
		if (xferp->mStatus == e_LL_XFER_IN_PROGRESS &&
			// If the circuit dies, abort
			!gMessageSystemp->mCircuitInfo.isCircuitAlive(xferp->mRemoteHost))
		{
			llwarns << "Xfer found in progress on dead circuit, aborting"
					<< llendl;
			xferp->mCallbackResult = LL_ERR_CIRCUIT_GONE;
			xferp->processEOF();
			iter = mReceiveList.erase(iter);
			delete xferp;
			continue;
		}
		++iter;
	}

	updateHostStatus();

	iter = mSendList.begin();
	while (iter != mSendList.end())
	{
		xferp = *iter;
		if (!xferp)	// Paranoia
		{
			iter = mSendList.erase(iter);
			continue;
		}
		if (xferp->mWaitingForACK &&
			xferp->ACKTimer.getElapsedTimeF32() > LL_PACKET_TIMEOUT)
		{
			if (xferp->mRetries > LL_PACKET_RETRY_LIMIT)
			{
				llinfos << "Dropping xfer " << xferp->mRemoteHost << ":"
						<< xferp->getFileName()
						<< " packet retransmit limit exceeded, xfer dropped"
						<< llendl;
				xferp->abort(LL_ERR_TCP_TIMEOUT);
				iter = mSendList.erase(iter);
				delete xferp;
				continue;
 			}

			llinfos << "Resending xfer " << xferp->mRemoteHost << ":"
					<< xferp->getFileName()
					<< " packet unconfirmed after " << LL_PACKET_TIMEOUT
					<< " seconds, packet: " << xferp->mPacketNum << llendl;
			xferp->resendLastPacket();
		}
		else if (xferp->mStatus == e_LL_XFER_REGISTERED &&
				 xferp->ACKTimer.getElapsedTimeF32() > LL_XFER_REGISTRATION_TIMEOUT)
		{
			llinfos << "Registered xfer never requested, xfer dropped"
					<< llendl;
			xferp->abort(LL_ERR_TCP_TIMEOUT);
			iter = mSendList.erase(iter);
			delete xferp;
			continue;
		}
		else if (xferp->mStatus == e_LL_XFER_ABORTED)
		{
			llwarns << "Removing aborted xfer " << xferp->mRemoteHost << ":"
					<< xferp->getFileName() << llendl;
			iter = mSendList.erase(iter);
			delete xferp;
			continue;
		}
		else if (xferp->mStatus == e_LL_XFER_PENDING)
		{
			LL_DEBUGS("FileTransfer") << "numActiveXfers = "
									  << numActiveXfers(xferp->mRemoteHost)
									  << " - mMaxOutgoingXfersPerCircuit = "
									  << mMaxOutgoingXfersPerCircuit
									  << LL_ENDL;
			if (numActiveXfers(xferp->mRemoteHost) <
					mMaxOutgoingXfersPerCircuit)
			{
				if (xferp->reopenFileHandle())
				{
					llwarns << "Removing failed xfer to " << xferp->mRemoteHost
							<< " for Id " << U64_to_str(xferp->mID) << llendl;
					xferp->abort(LL_ERR_CANNOT_OPEN_FILE);
					iter = mSendList.erase(iter);
					delete xferp;
					continue;
				}
				LL_DEBUGS("FileTransfer") << "Moving pending xfer ID "
									  <<  U64_to_str(xferp->mID)
									  << " to active" << LL_ENDL;
				xferp->sendNextPacket();
				changeNumActiveXfers(xferp->mRemoteHost, 1);
			}
		}
		++iter;
	}

	// *HACK: if we are using xfer confirm throttling, throttle our xfer
	// confirms here so we don't blow through bandwidth.
	while (mXferAckQueue.size())
	{
		if (mAckThrottle.checkOverflow(8000.0f))
		{
			break;
		}
		LL_DEBUGS("FileTransfer") << "Confirm packet queue length:"
								  << mXferAckQueue.size() << LL_ENDL;

		LLXferAckInfo ack_info = mXferAckQueue.front();
		mXferAckQueue.pop_front();

		sendConfirmPacket(gMessageSystemp, ack_info.mID, ack_info.mPacketNum,
						  ack_info.mRemoteHost);
		mAckThrottle.throttleOverflow(8000.f); // Assume 1000 bytes/packet
	}
}

void LLXferManager::abortRequestById(U64 xfer_id, S32 result_code)
{
	LLXfer* xferp = findXferByID(xfer_id, mReceiveList);
	if (xferp)
	{
		if (xferp->mStatus == e_LL_XFER_IN_PROGRESS)
		{
			// Causes processAbort();
			xferp->abort(result_code);
		}
		else
		{
			xferp->mCallbackResult = result_code;
			xferp->processEOF(); // Should notify the requester
			removeXfer(xferp, mReceiveList);
		}
		// Since already removed or marked as aborted no need to wait for
		// processAbort() to start new download
		startPendingDownloads();
	}
}

void LLXferManager::processAbort(LLMessageSystem* msg, void**)
{
	U64 id = 0;
	msg->getU64Fast(_PREHASH_XferID, _PREHASH_ID, id);
	S32 result_code = 0;
	msg->getS32Fast(_PREHASH_XferID, _PREHASH_Result, result_code);

	LLXfer* xferp = findXferByID(id, mReceiveList);
	if (xferp)
	{
		xferp->mCallbackResult = result_code;
		xferp->processEOF();
		removeXfer(xferp, mReceiveList);
		startPendingDownloads();
	}
}

// This method goes through the list, and starts pending operations until
// active downloads == mMaxIncomingXfers.
void LLXferManager::startPendingDownloads()
{
	// We copy the pending xfers into a temporary data structure because the
	// xfers are stored as an intrusive linked list where older requests get
	// pushed toward the back. Thus, if we did not do a stateful iteration, it
	// would be possible for old requests to never start.
	std::list<LLXfer*> pending_downloads;
	S32 download_count = 0;
	S32 pending_count = 0;
	LLXfer* xferp;
	for (xfer_list_t::iterator iter = mReceiveList.begin(),
							   end = mReceiveList.end();
		 iter != end; ++iter)
	{
		xferp = *iter;
		if (!xferp) continue;	// Paranoia

		if (xferp->mStatus == e_LL_XFER_PENDING)
		{
			++pending_count;
			pending_downloads.push_front(xferp);
		}
		else if (xferp->mStatus == e_LL_XFER_IN_PROGRESS)
		{
			++download_count;
		}
	}

	S32 start_count = mMaxIncomingXfers - download_count;

	LL_DEBUGS("FileTransfer") << "Xfer in progress: " << download_count
							  << " - xfer pending: " << pending_count
							  << " - Starting: "
							  << llmin(start_count, pending_count)
							  << LL_ENDL;

	if (start_count > 0 && pending_count > 0)
	{
		S32 result;
		for (std::list<LLXfer*>::iterator iter = pending_downloads.begin(),
										  end = pending_downloads.end();
			 iter != end; ++iter)
		{
			xferp = *iter;
			if (start_count-- <= 0)
			{
				break;
			}
			result = xferp->startDownload();
			if (result)
			{
				xferp->abort(result);
				++start_count;
			}
		}
	}
}

void LLXferManager::addToList(LLXfer* xferp, xfer_list_t& xfer_list,
							  bool is_priority)
{
	if (xferp)
	{
		if (is_priority)
		{
			xfer_list.push_back(xferp);
		}
		else
		{
			xfer_list.push_front(xferp);
		}
	}
	else
	{
		llwarns << "Tried to add a NULL LLXfer !" << llendl;
		llassert(false);
	}
}

///////////////////////////////////////////////////////////
// C routines
///////////////////////////////////////////////////////////

void process_confirm_packet(LLMessageSystem* msg, void** user_data)
{
	if (gXferManagerp)
	{
		gXferManagerp->processConfirmation(msg, user_data);
	}
}

void process_request_xfer(LLMessageSystem* msg, void** user_data)
{
	if (gXferManagerp)
	{
		gXferManagerp->processFileRequest(msg, user_data);
	}
}

void continue_file_receive(LLMessageSystem* msg, void** user_data)
{
	if (gXferManagerp)
	{
		gXferManagerp->processReceiveData(msg, user_data);
	}
}

void process_abort_xfer(LLMessageSystem* msg, void** user_data)
{
	if (gXferManagerp)
	{
		gXferManagerp->processAbort(msg, user_data);
	}
}
