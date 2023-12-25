/**
 * @file lltransfermanager.cpp
 * @brief Improved transfer mechanism for moving data through the
 * message system.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "lltransfermanager.h"

#include "lldatapacker.h"
#include "lltransfersourcefile.h"
#include "lltransfersourceasset.h"
#include "lltransfertargetfile.h"
#include "lltransfertargetvfile.h"
#include "llmessage.h"

constexpr S32 MAX_PACKET_DATA_SIZE = 2048;
constexpr S32 MAX_PARAMS_SIZE = 1024;

LLTransferManager gTransferManager;
LLTransferSource::stype_scfunc_map LLTransferSource::sSourceCreateMap;

//
// LLTransferManager implementation
//

LLTransferManager::LLTransferManager()
:	mValid(false)
{
	for (S32 i = 0; i < LLTTT_NUM_TYPES; ++i)
	{
		mTransferBitsIn[i] = 0;
		mTransferBitsOut[i] = 0;
	}
}

LLTransferManager::~LLTransferManager()
{
	if (mValid)
	{
		llwarns << "LLTransferManager should have been cleaned up by message system shutdown process"
				<< llendl;
		cleanup();
	}
}

void LLTransferManager::init()
{
	if (mValid)
	{
		llerrs << "Double initializing LLTransferManager !" << llendl;
	}
	mValid = true;

	// Register message system handlers
	LLMessageSystem* msg = gMessageSystemp;
	msg->setHandlerFunc("TransferRequest", processTransferRequest, NULL);
	msg->setHandlerFunc("TransferInfo", processTransferInfo, NULL);
	msg->setHandlerFunc("TransferPacket", processTransferPacket, NULL);
	msg->setHandlerFunc("TransferAbort", processTransferAbort, NULL);
}

void LLTransferManager::cleanup()
{
	mValid = false;

	for (host_tc_map::iterator iter = mTransferConnections.begin(),
							   end = mTransferConnections.end();
		 iter != end; ++iter)
	{
		delete iter->second;
	}
	mTransferConnections.clear();
}

void LLTransferManager::updateTransfers()
{
	host_tc_map::iterator iter = mTransferConnections.begin();
	while (iter != mTransferConnections.end())
	{
		host_tc_map::iterator cur = iter++;
		cur->second->updateTransfers();
	}
}

void LLTransferManager::cleanupConnection(const LLHost& host)
{
	host_tc_map::iterator iter = mTransferConnections.find(host);
	if (iter == mTransferConnections.end())
	{
		// This can happen legitimately if we've never done a transfer, and
		// we're cleaning up a circuit.
		return;
	}
	LLTransferConnection* connp = iter->second;
	delete connp;
	mTransferConnections.erase(iter);
}

LLTransferConnection* LLTransferManager::getTransferConnection(const LLHost& host)
{
	host_tc_map::iterator iter;
	iter = mTransferConnections.find(host);
	if (iter == mTransferConnections.end())
	{
		mTransferConnections[host] = new LLTransferConnection(host);
		return mTransferConnections[host];
	}

	return iter->second;
}

LLTransferSourceChannel* LLTransferManager::getSourceChannel(const LLHost& host,
															 LLTransferChannelType type)
{
	LLTransferConnection* tcp = getTransferConnection(host);
	return tcp ? tcp->getSourceChannel(type) : NULL;
}

LLTransferTargetChannel* LLTransferManager::getTargetChannel(const LLHost& host,
															 LLTransferChannelType type)
{
	LLTransferConnection* tcp = getTransferConnection(host);
	return tcp ? tcp->getTargetChannel(type) : NULL;
}

LLTransferSource* LLTransferManager::findTransferSource(const LLUUID& transfer_id)
{
	// This linear traversal could screw us later if we do lots of searches for
	// sources. However, this ONLY happens right now in asset transfer
	// callbacks, so this should be relatively quick.
	LLTransferConnection* tcp;
	LLTransferSourceChannel* scp;
	LLTransferSource* sourcep;
	for (host_tc_map::iterator iter = mTransferConnections.begin(),
							   end = mTransferConnections.end();
		 iter != end; ++iter)
	{
		tcp = iter->second;
		for (LLTransferConnection::tsc_iter
				sc_iter = tcp->mTransferSourceChannels.begin(),
				sc_end = tcp->mTransferSourceChannels.end();
			 sc_iter != sc_end; ++sc_iter)
		{
			scp = *sc_iter;
			if (scp)
			{
				sourcep = scp->findTransferSource(transfer_id);
				if (sourcep)
				{
					return sourcep;
				}
			}
		}
	}

	return NULL;
}

//
// Message handlers
//

//static
void LLTransferManager::processTransferRequest(LLMessageSystem* msgp, void**)
{
	LLUUID transfer_id;
	LLTransferSourceType source_type;
	LLTransferChannelType channel_type;
	F32 priority;

	msgp->getUUID("TransferInfo", "TransferID", transfer_id);
	msgp->getS32("TransferInfo", "SourceType", (S32&)source_type);
	msgp->getS32("TransferInfo", "ChannelType", (S32&)channel_type);
	msgp->getF32("TransferInfo", "Priority", priority);

	LLTransferSourceChannel* tscp = gTransferManager.getSourceChannel(msgp->getSender(),
																	  channel_type);
	if (!tscp)
	{
		llwarns << "Source channel not found" << llendl;
		return;
	}

	if (tscp->findTransferSource(transfer_id))
	{
		llwarns << "Duplicate request for transfer " << transfer_id
				<< ", aborting!" << llendl;
		return;
	}

	S32 size = msgp->getSize("TransferInfo", "Params");
	if (size < 0)
	{
		llwarns << "Bad TransferInfo block. Aborted." << llendl;
		return;
	}
	if (size > MAX_PARAMS_SIZE)
	{
		llwarns << "Params too big. Aborted." << llendl;
		return;
	}

	LL_DEBUGS("Messaging") << "Initiating transfer. Id: " << transfer_id
						   << " - Source type: " << source_type
						   << " - Channel type:" << channel_type
						   << " - Priority:" << priority << LL_ENDL;
	LLTransferSource* tsp = LLTransferSource::createSource(source_type,
														   transfer_id,
														   priority);
	if (!tsp)
	{
		llwarns << "Couldn't create transfer source !" << llendl;
		return;
	}
	U8 tmp[MAX_PARAMS_SIZE];
	msgp->getBinaryData("TransferInfo", "Params", tmp, size);

	LLDataPackerBinaryBuffer dpb(tmp, MAX_PARAMS_SIZE);
	bool unpack_ok = tsp->unpackParams(dpb);
	if (!unpack_ok)
	{
		// This should only happen if the data is corrupt or incorrectly
		// packed.
		// *NOTE: We may want to call abortTransfer().
		llwarns << "Bad parameters !" << llendl;
		delete tsp;
		return;
	}

	tscp->addTransferSource(tsp);
	tsp->initTransfer();
}

//static
void LLTransferManager::processTransferInfo(LLMessageSystem* msgp, void**)
{
	//llinfos << "LLTransferManager::processTransferInfo" << llendl;

	LLUUID transfer_id;
	LLTransferTargetType target_type;
	LLTransferChannelType channel_type;
	LLTSCode status;
	S32 size;

	msgp->getUUID("TransferInfo", "TransferID", transfer_id);
	msgp->getS32("TransferInfo", "TargetType", (S32&)target_type);
	msgp->getS32("TransferInfo", "ChannelType", (S32&)channel_type);
	msgp->getS32("TransferInfo", "Status", (S32&)status);
	msgp->getS32("TransferInfo", "Size", size);

	LL_DEBUGS("Messaging") << "Processing info for transfer. Id: "
						   << transfer_id << " - Target type:" << target_type
						   << " - Channel type:" << channel_type << LL_ENDL;
	LLTransferTargetChannel* ttcp = gTransferManager.getTargetChannel(msgp->getSender(),
																	  channel_type);
	if (!ttcp)
	{
		llwarns << "Target channel not found" << llendl;
		// Should send a message to abort the transfer.
		return;
	}

	LLTransferTarget* ttp = ttcp->findTransferTarget(transfer_id);
	if (!ttp)
	{
		llwarns << "TransferInfo for unknown transfer !  Not able to handle this yet !"
				<< llendl;
		// This could happen if we're doing a push transfer, although to avoid
		// confusion, maybe it should be a different message.
		return;
	}

	if (status != LLTS_OK)
	{
		llwarns << transfer_id << ": Non-ok status, cleaning up" << llendl;
		ttp->completionCallback(status);
		// Clean up the transfer.
		ttcp->deleteTransfer(ttp);
		return;
	}

	// Unpack the params
	S32 params_size = msgp->getSize("TransferInfo", "Params");
	if (params_size < 0)
	{
		llwarns << "Bad TransferInfo/Params. Aborted." << llendl;
		return;
	}
	if (params_size > MAX_PARAMS_SIZE)
	{
		llwarns << "TransferInfo/Params too big. Aborted." << llendl;
		return;
	}
	else if (params_size > 0)
	{
		U8 tmp[MAX_PARAMS_SIZE];
		msgp->getBinaryData("TransferInfo", "Params", tmp, params_size);
		LLDataPackerBinaryBuffer dpb(tmp, MAX_PARAMS_SIZE);
		if (!ttp->unpackParams(dpb))
		{
			// This should only happen if the data is corrupt or
			// incorrectly packed.
			llwarns << "Bad params." << llendl;
			ttp->abortTransfer();
			ttcp->deleteTransfer(ttp);
			return;
		}
	}

	LL_DEBUGS("Messaging") << "Receiving " << transfer_id << ", size " << size
						   << " bytes" << LL_ENDL;
	ttp->setSize(size);
	ttp->setGotInfo(true);

	// OK, at this point we to handle any delayed transfer packets (which could
	// happen if this packet was lost)

	// This is a lame cut and paste of code down below.  If we change the logic
	// down there, we HAVE to change the logic up here.

	while (true)
	{
		S32 packet_id = 0;
		U8 tmp_data[MAX_PACKET_DATA_SIZE];
		// See if we've got any delayed packets
		packet_id = ttp->getNextPacketID();
		if (ttp->mDelayedPacketMap.find(packet_id) != ttp->mDelayedPacketMap.end())
		{
			// Perhaps this stuff should be inside a method in LLTransferPacket?
			// I'm too lazy to do it now, though.
			LL_DEBUGS("Messaging") << "Playing back delayed packet " << packet_id
								   << LL_ENDL;
			LLTransferPacket* packetp = ttp->mDelayedPacketMap[packet_id];

			// This is somewhat inefficient, but avoids us having to duplicate
			// code between the off-the-wire and delayed paths.
			packet_id = packetp->mPacketID;
			size = packetp->mSize;
			if (size)
			{
				if (packetp->mDatap && size < (S32)sizeof(tmp_data))
				{
					memcpy(tmp_data, packetp->mDatap, size);
				}
			}
			status = packetp->mStatus;
			ttp->mDelayedPacketMap.erase(packet_id);
			delete packetp;
		}
		else
		{
			// No matching delayed packet, we're done.
			break;
		}

		LLTSCode ret_code = ttp->dataCallback(packet_id, tmp_data, size);
		if (ret_code == LLTS_OK)
		{
			ttp->setLastPacketID(packet_id);
		}

		if (status != LLTS_OK)
		{
			if (status != LLTS_DONE)
			{
				llwarns << "Error in playback !" << llendl;
			}
			else
			{
				llinfos << "Replay finished for " << transfer_id << llendl;
			}
			// This transfer is done, either via error or not.
			ttp->completionCallback(status);
			ttcp->deleteTransfer(ttp);
			return;
		}
	}
}

//static
void LLTransferManager::processTransferPacket(LLMessageSystem* msgp, void**)
{
	LLUUID transfer_id;
	LLTransferChannelType channel_type;
	S32 packet_id;
	LLTSCode status;
	S32 size;
	msgp->getUUID("TransferData", "TransferID", transfer_id);
	msgp->getS32("TransferData", "ChannelType", (S32&)channel_type);
	msgp->getS32("TransferData", "Packet", packet_id);
	msgp->getS32("TransferData", "Status", (S32&)status);

	// Find the transfer associated with this packet.
	//llinfos << transfer_id << ":" << channel_type << llendl;
	LLTransferTargetChannel* ttcp =
		gTransferManager.getTargetChannel(msgp->getSender(), channel_type);
	if (!ttcp)
	{
		llwarns << "Target channel not found" << llendl;
		return;
	}

	LLTransferTarget* ttp = ttcp->findTransferTarget(transfer_id);
	if (!ttp)
	{
		llwarns_once << "Did not find matching transfer for " << transfer_id
					 << " processing packet from " << msgp->getSender()
					 << llendl;
		return;
	}

	size = msgp->getSize("TransferData", "Data");

	S32 msg_bytes = 0;
	if (msgp->getReceiveCompressedSize())
	{
		msg_bytes = msgp->getReceiveCompressedSize();
	}
	else
	{
		msg_bytes = msgp->getReceiveSize();
	}
	gTransferManager.addTransferBitsIn(ttcp->mChannelType, msg_bytes * 8);

	if (size < 0 || size > MAX_PACKET_DATA_SIZE)
	{
		llwarns << "Invalid transfer packet size " << size << llendl;
		return;
	}

	U8 tmp_data[MAX_PACKET_DATA_SIZE];
	if (size > 0)
	{
		// Only pull the data out if the size is > 0
		msgp->getBinaryData("TransferData", "Data", tmp_data, size);
	}

	if (!ttp->gotInfo() || ttp->getNextPacketID() != packet_id)
	{
		// Put this on a list of packets to be delivered later.
		if (!ttp->addDelayedPacket(packet_id, status, tmp_data, size))
		{
			// Whoops - failed to add a delayed packet for some reason.
			llwarns << "Too many delayed packets processing transfer "
					<< transfer_id << " from " << msgp->getSender() << llendl;
			ttp->abortTransfer();
			ttcp->deleteTransfer(ttp);
			return;
		}
#if LL_DEBUG	// Spammy!
		constexpr S32 LL_TRANSFER_WARN_GAP = 10;
		if (!ttp->gotInfo())
		{
			llwarns << "Got data packet before information in transfer "
					<< transfer_id << " from " << msgp->getSender()
					<< ", got " << packet_id << llendl;
		}
		else if (packet_id - ttp->getNextPacketID() > LL_TRANSFER_WARN_GAP)
		{
			llwarns << "Out of order packet in transfer " << transfer_id
					<< " from " << msgp->getSender() << ", got " << packet_id
					<< " expecting " << ttp->getNextPacketID() << llendl;
		}
#endif
		return;
	}

	// Loop through this until we're done with all delayed packets

	// NOTE: THERE IS A CUT AND PASTE OF THIS CODE IN THE TRANSFERINFO HANDLER
	// SO WE CAN PLAY BACK DELAYED PACKETS THERE !

	bool done = false;
	while (!done)
	{
		LLTSCode ret_code = ttp->dataCallback(packet_id, tmp_data, size);
		if (ret_code == LLTS_OK)
		{
			ttp->setLastPacketID(packet_id);
		}

		if (status != LLTS_OK)
		{
			if (status != LLTS_DONE)
			{
				llwarns << "Error in transfer!" << llendl;
			}
			else
			{
				LL_DEBUGS("Messaging") << "Transfer done for " << transfer_id
									   << LL_ENDL;
			}
			// This transfer is done, either via error or not.
			ttp->completionCallback(status);
			ttcp->deleteTransfer(ttp);
			return;
		}

		// See if we've got any delayed packets
		packet_id = ttp->getNextPacketID();
		if (ttp->mDelayedPacketMap.find(packet_id) != ttp->mDelayedPacketMap.end())
		{
			// Perhaps this stuff should be inside a method in LLTransferPacket?
			// I'm too lazy to do it now, though.
			LL_DEBUGS("Messaging") << "Playing back delayed packet "
								   << packet_id << LL_ENDL;
			LLTransferPacket* packetp = ttp->mDelayedPacketMap[packet_id];

			// This is somewhat inefficient, but avoids us having to duplicate
			// code between the off-the-wire and delayed paths.
			packet_id = packetp->mPacketID;
			size = packetp->mSize;
			if (size)
			{
				if (packetp->mDatap && size < (S32)sizeof(tmp_data))
				{
					memcpy(tmp_data, packetp->mDatap, size);
				}
			}
			status = packetp->mStatus;
			ttp->mDelayedPacketMap.erase(packet_id);
			delete packetp;
		}
		else
		{
			// No matching delayed packet, abort it.
			done = true;
		}
	}
}

//static
void LLTransferManager::processTransferAbort(LLMessageSystem* msgp, void**)
{
	LLUUID transfer_id;
	msgp->getUUID("TransferInfo", "TransferID", transfer_id);
	LLTransferChannelType channel_type;
	msgp->getS32("TransferInfo", "ChannelType", (S32&)channel_type);

	// See if it is a target that we're trying to abort. Find the transfer
	// associated with this packet.
	LLTransferTargetChannel* ttcp =
		gTransferManager.getTargetChannel(msgp->getSender(), channel_type);
	if (ttcp)
	{
		LLTransferTarget* ttp = ttcp->findTransferTarget(transfer_id);
		if (ttp)
		{
			ttp->abortTransfer();
			ttcp->deleteTransfer(ttp);
			return;
		}
	}

	// Hmm, not a target. Maybe it is a source.
	LLTransferSourceChannel* tscp =
		gTransferManager.getSourceChannel(msgp->getSender(), channel_type);
	if (tscp)
	{
		LLTransferSource* tsp = tscp->findTransferSource(transfer_id);
		if (tsp)
		{
			tsp->abortTransfer();
			tscp->deleteTransfer(tsp);
			return;
		}
	}

	llwarns << "Couldn't find transfer " << transfer_id << " to abort !"
			<< llendl;
}

//static
void LLTransferManager::reliablePacketCallback(void** user_data, S32 result)
{
	LLUUID* transfer_idp = (LLUUID*)user_data;
	if (result && transfer_idp)
	{
		LLTransferSource* tsp =
			gTransferManager.findTransferSource(*transfer_idp);
		if (tsp)
		{
			llwarns << "Aborting reliable transfer " << *transfer_idp
					<< " due to failed reliable resends !" << llendl;
			LLTransferSourceChannel* tscp = tsp->mChannelp;
			tsp->abortTransfer();
			tscp->deleteTransfer(tsp);
		}
		else
		{
			llwarns << "Aborting reliable transfer " << *transfer_idp
					<< " but can't find the LLTransferSource object" << llendl;
		}
	}
	delete transfer_idp;
}

//
// LLTransferConnection implementation
//

LLTransferConnection::LLTransferConnection(const LLHost& host)
{
	mHost = host;
}

LLTransferConnection::~LLTransferConnection()
{
	tsc_iter itersc;
	for (tsc_iter itersc = mTransferSourceChannels.begin(),
				  endsc = mTransferSourceChannels.end();
		 itersc != endsc; ++itersc)
	{
		delete *itersc;
	}
	mTransferSourceChannels.clear();

	for (ttc_iter itertc = mTransferTargetChannels.begin(),
				  endtc = mTransferTargetChannels.end();
		 itertc != endtc; ++itertc)
	{
		delete *itertc;
	}
	mTransferTargetChannels.clear();
}

void LLTransferConnection::updateTransfers()
{
	// Do stuff for source transfers (basically, send data out).
	tsc_iter iter, cur;
	iter = mTransferSourceChannels.begin();

	while (iter != mTransferSourceChannels.end())
	{
		cur = iter++;
		(*cur)->updateTransfers();
	}

	// Do stuff for target transfers. Primarily, we should be aborting
	// transfers that are irredeemably broken (large packet gaps that do not
	// appear to be getting filled in, most likely). Probably should NOT be
	// doing timeouts for other things, as new priority scheme means that a
	// high priority transfer COULD block a transfer for a long time.
}

LLTransferSourceChannel* LLTransferConnection::getSourceChannel(LLTransferChannelType channel_type)
{
	for (tsc_iter iter = mTransferSourceChannels.begin(),
				  end = mTransferSourceChannels.end();
		 iter != end; ++iter)
	{
		if ((*iter)->getChannelType() == channel_type)
		{
			return *iter;
		}
	}

	LLTransferSourceChannel* tscp = new LLTransferSourceChannel(channel_type,
																mHost);
	mTransferSourceChannels.push_back(tscp);
	return tscp;
}

LLTransferTargetChannel* LLTransferConnection::getTargetChannel(LLTransferChannelType channel_type)
{
	for (ttc_iter iter = mTransferTargetChannels.begin(),
				  end = mTransferTargetChannels.end();
		 iter != end; ++iter)
	{
		if ((*iter)->getChannelType() == channel_type)
		{
			return *iter;
		}
	}

	LLTransferTargetChannel* ttcp = new LLTransferTargetChannel(channel_type,
																mHost);
	mTransferTargetChannels.push_back(ttcp);
	return ttcp;
}

//
// LLTransferSourceChannel implementation
//

constexpr S32 DEFAULT_PACKET_SIZE = 1000;

LLTransferSourceChannel::LLTransferSourceChannel(LLTransferChannelType channel_type,
												 const LLHost& host)
:	mChannelType(channel_type),
	mHost(host),
	mTransferSources(LLTransferSource::sSetPriority, LLTransferSource::sGetPriority),
	mThrottleID(TC_ASSET)
{
}

LLTransferSourceChannel::~LLTransferSourceChannel()
{
	for (LLPriQueueMap<LLTransferSource*>::pqm_iter
			iter = mTransferSources.mMap.begin(),
			end = mTransferSources.mMap.end(); iter != end; ++iter)
	{
		// Just kill off all of the transfers
		if (iter->second)
		{
			iter->second->abortTransfer();
			delete iter->second;
		}
	}
	mTransferSources.mMap.clear();
}

void LLTransferSourceChannel::updatePriority(LLTransferSource* tsp,
											 F32 priority)
{
	mTransferSources.reprioritize(priority, tsp);
}

void LLTransferSourceChannel::updateTransfers()
{
	// Actually, this should do the following:
	// - Decide if we can actually send data.
	// - If so, update priorities so we know who gets to send it.
	// - Send data from the sources, while updating until we've sent our
	//   throttle allocation.

	LLMessageSystem* msg = gMessageSystemp;
	LLCircuitData* cdp = msg->mCircuitInfo.findCircuit(getHost());
	if (!cdp)
	{
		return;
	}

	if (cdp->isBlocked())
	{
		// *NOTE: We need to make sure that the throttle bits available gets
		// reset.
		// We DO NOT want to send any packets if they're blocked, they'll just
		// end up piling up on the other end.
		LL_DEBUGS("Messaging") << "Blocking transfers due to blocked circuit for "
							   << getHost() << LL_ENDL;
		return;
	}

	const S32 throttle_id = mThrottleID;

	LLThrottleGroup& tg = cdp->getThrottleGroup();

	if (tg.checkOverflow(throttle_id, 0.f))
	{
		return;
	}

	LLPriQueueMap<LLTransferSource*>::pqm_iter iter, next;

	bool done = false;
	for (iter = mTransferSources.mMap.begin();
		 iter != mTransferSources.mMap.end() && !done; )
	{
		next = iter;
		++next;

		LLTransferSource* tsp = iter->second;
		U8* datap = NULL;
		S32 data_size = 0;
		bool delete_data = false;
		S32 packet_id = 0;
		S32 sent_bytes = 0;
		LLTSCode status = LLTS_OK;

		// Get the packetID for the next packet that we're transferring.
		packet_id = tsp->getNextPacketID();
		status = tsp->dataCallback(packet_id, DEFAULT_PACKET_SIZE, &datap,
								   data_size, delete_data);

		if (status == LLTS_SKIP)
		{
			// We do not have any data, but we're not done, just go on. This
			// will presumably be used for streaming or async transfers that
			// are stalled waiting for data from another source.
			iter = next;
			continue;
		}

		LLUUID* cb_uuid = new LLUUID(tsp->getID());
		LLUUID transaction_id = tsp->getID();

		// Send the data now, even if it's an error.
		// The status code will tell the other end what to do.
		msg->newMessage("TransferPacket");
		msg->nextBlock("TransferData");
		msg->addUUID("TransferID", tsp->getID());
		msg->addS32("ChannelType", getChannelType());
		msg->addS32("Packet", packet_id);	// *HACK: need a REAL packet id
		msg->addS32("Status", status);
		msg->addBinaryData("Data", datap, data_size);
		sent_bytes = msg->getCurrentSendTotal();
		msg->sendReliable(getHost(), LL_DEFAULT_RELIABLE_RETRIES, true, 0.f,
						  LLTransferManager::reliablePacketCallback,
						  (void**)cb_uuid);

		// Do bookkeeping for the throttle
		done = tg.throttleOverflow(throttle_id, sent_bytes * 8.f);
		gTransferManager.addTransferBitsOut(mChannelType, sent_bytes * 8);

		// Clean up our temporary data.
		if (delete_data)
		{
			delete[] datap;
			datap = NULL;
		}

		if (findTransferSource(transaction_id) == NULL)
		{
			// Warning !  In the case of an aborted transfer, the sendReliable
			// call above calls AbortTransfer which in turn calls
			// deleteTransfer which means that somewhere way down the chain our
			// current iter can get invalidated resulting in an infrequent sim
			// crash. This check gets us to a valid transfer source in this
			// event.
			iter = next;
			continue;
		}

		// Update the packet counter
		tsp->setLastPacketID(packet_id);

		switch (status)
		{
			case LLTS_OK:
				// We're OK, don't need to do anything.  Keep sending data.
				break;
			case LLTS_ERROR:
				llwarns << "Error in transfer dataCallback!" << llendl;
				// fall through
			case LLTS_DONE:
				// We need to clean up this transfer source.
				LL_DEBUGS("Messaging") << tsp->getID() << " done" << LL_ENDL;
				tsp->completionCallback(status);
				delete tsp;

				mTransferSources.mMap.erase(iter);
				iter = next;
				break;
			default:
				llerrs << "Unknown transfer error code!" << llendl;
		}

		// At this point, we should do priority adjustment (since some
		// transfers like streaming transfers will adjust priority based on how
		// much they've sent and time, but I'm not going to bother yet. - djs.
	}
}

void LLTransferSourceChannel::addTransferSource(LLTransferSource* sourcep)
{
	sourcep->mChannelp = this;
	mTransferSources.push(sourcep->getPriority(), sourcep);
}

LLTransferSource* LLTransferSourceChannel::findTransferSource(const LLUUID& transfer_id)
{
	for (LLPriQueueMap<LLTransferSource*>::pqm_iter
			iter = mTransferSources.mMap.begin(),
			end = mTransferSources.mMap.end(); iter != end; ++iter)
	{
		LLTransferSource* tsp = iter->second;
		if (tsp->getID() == transfer_id)
		{
			return tsp;
		}
	}
	return NULL;
}

void LLTransferSourceChannel::deleteTransfer(LLTransferSource* tsp)
{
	if (tsp)
	{
		for (LLPriQueueMap<LLTransferSource*>::pqm_iter
				iter = mTransferSources.mMap.begin(),
				end = mTransferSources.mMap.end(); iter != end; ++iter)
		{
			if (iter->second == tsp)
			{
				delete tsp;
				mTransferSources.mMap.erase(iter);
				return;
			}
		}

		llwarns << "Unable to find transfer source ID " << tsp->getID()
				<< " to delete !" << llendl;
	}
}

//
// LLTransferTargetChannel implementation
//

LLTransferTargetChannel::LLTransferTargetChannel(LLTransferChannelType type,
												 const LLHost& host)
:	mChannelType(type),
	mHost(host)
{
}

LLTransferTargetChannel::~LLTransferTargetChannel()
{
	for (tt_iter iter = mTransferTargets.begin(), end = mTransferTargets.end();
		 iter != end; ++iter)
	{
		// Abort all of the current transfers
		(*iter)->abortTransfer();
		delete *iter;
	}
	mTransferTargets.clear();
}

void LLTransferTargetChannel::requestTransfer(const LLTransferSourceParams& source_params,
											  const LLTransferTargetParams& target_params,
											  F32 priority)
{
	LLUUID id;
	id.generate();
	LLTransferTarget* ttp = LLTransferTarget::createTarget(
		target_params.getType(),
		id,
		source_params.getType());
	if (!ttp)
	{
		llwarns << "Aborting due to target creation failure !" << llendl;
		return;
	}

	ttp->applyParams(target_params);
	addTransferTarget(ttp);

	sendTransferRequest(ttp, source_params, priority);
}

void LLTransferTargetChannel::sendTransferRequest(LLTransferTarget* targetp,
												  const LLTransferSourceParams& params,
												  F32 priority)
{
	//
	// Pack the message with data which explains how to get the source, and
	// send it off to the source for this channel.
	//
	llassert(targetp);
	llassert(targetp->getChannel() == this);

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("TransferRequest");
	msg->nextBlock("TransferInfo");
	msg->addUUID("TransferID", targetp->getID());
	msg->addS32("SourceType", params.getType());
	msg->addS32("ChannelType", getChannelType());
	msg->addF32("Priority", priority);

	U8 tmp[MAX_PARAMS_SIZE];
	LLDataPackerBinaryBuffer dp(tmp, MAX_PARAMS_SIZE);
	params.packParams(dp);
	S32 len = dp.getCurrentSize();
	msg->addBinaryData("Params", tmp, len);

	msg->sendReliable(mHost);
}

void LLTransferTargetChannel::addTransferTarget(LLTransferTarget* targetp)
{
	targetp->mChannelp = this;
	mTransferTargets.push_back(targetp);
}

LLTransferTarget* LLTransferTargetChannel::findTransferTarget(const LLUUID& transfer_id)
{
	for (tt_iter iter = mTransferTargets.begin(),
				 end = mTransferTargets.end();
		 iter != end; ++iter)
	{
		LLTransferTarget* ttp = *iter;
		if (ttp && ttp->getID() == transfer_id)
		{
			return ttp;
		}
	}
	return NULL;
}

void LLTransferTargetChannel::deleteTransfer(LLTransferTarget* ttp)
{
	if (ttp)
	{
		for (tt_iter iter = mTransferTargets.begin(),
					 end = mTransferTargets.end();
			 iter != end; ++iter)
		{
			if (*iter == ttp)
			{
				delete ttp;
				mTransferTargets.erase(iter);
				return;
			}
		}

		llwarns << "Unable to find transfer target ID " << ttp->getID()
				<< " to delete !" << llendl;
	}
}

//
// LLTransferSource implementation
//

LLTransferSource::LLTransferSource(LLTransferSourceType type,
								   const LLUUID& transfer_id,
								   F32 priority) :
	mType(type),
	mID(transfer_id),
	mChannelp(NULL),
	mPriority(priority),
	mSize(0),
	mLastPacketID(-1)
{
	setPriority(priority);
}

void LLTransferSource::sendTransferStatus(LLTSCode status)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("TransferInfo");
	msg->nextBlock("TransferInfo");
	msg->addUUID("TransferID", getID());
	msg->addS32("TargetType", LLTTT_UNKNOWN);
	msg->addS32("ChannelType", mChannelp->getChannelType());
	msg->addS32("Status", status);
	msg->addS32("Size", mSize);
	U8 tmp[MAX_PARAMS_SIZE];
	LLDataPackerBinaryBuffer dp(tmp, MAX_PARAMS_SIZE);
	packParams(dp);
	S32 len = dp.getCurrentSize();
	msg->addBinaryData("Params", tmp, len);
	msg->sendReliable(mChannelp->getHost());

	// Abort if there was as asset system issue.
	if (status != LLTS_OK)
	{
		completionCallback(status);
		mChannelp->deleteTransfer(this);
	}
}

// This should never be called directly, the transfer manager is responsible
// for aborting the transfer from the channel.  I might want to rethink this in
// the future, though.
void LLTransferSource::abortTransfer()
{
	// Send a message down, call the completion callback
	llinfos << "Aborting transfer " << getID() << " to "
			<< mChannelp->getHost() << llendl;
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("TransferAbort");
	msg->nextBlock("TransferInfo");
	msg->addUUID("TransferID", getID());
	msg->addS32("ChannelType", mChannelp->getChannelType());
	msg->sendReliable(mChannelp->getHost());

	completionCallback(LLTS_ABORT);
}

//static
void LLTransferSource::registerSourceType(LLTransferSourceType stype,
										  LLTransferSourceCreateFunc func)
{
	if (sSourceCreateMap.count(stype))
	{
		// Disallow changing what class handles a source type. Unclear when you
		// would want to do this and whether it would work.
		llwarns << "Reregistering source type " << stype << llendl;
		llassert(false);
	}
	else
	{
		sSourceCreateMap[stype] = func;
	}
}

//static
LLTransferSource* LLTransferSource::createSource(LLTransferSourceType stype,
												 const LLUUID& id,
												 F32 priority)
{
	switch (stype)
	{
#if 0	// *NOTE: The source file transfer mechanism is highly insecure and
		// could lead to easy exploitation of a server process. I have removed
		// all uses of it from the codebase. Phoenix.
		case LLTST_FILE:
			return new LLTransferSourceFile(id, priority);
#endif

		case LLTST_ASSET:
			return new LLTransferSourceAsset(id, priority);

		default:
		{
			if (!sSourceCreateMap.count(stype))
			{
				// Use the callback to create the source type if it's not
				// there.
				llwarns << "Unknown transfer source type: " << stype
						<< llendl;
				return NULL;
			}
			return (sSourceCreateMap[stype])(id, priority);
		}
	}
}

// static
void LLTransferSource::sSetPriority(LLTransferSource*& tsp, F32 priority)
{
	tsp->setPriority(priority);
}

// static
F32 LLTransferSource::sGetPriority(LLTransferSource*& tsp)
{
	return tsp->getPriority();
}

//
// LLTransferPacket implementation
//

LLTransferPacket::LLTransferPacket(S32 packet_id, LLTSCode status,
								   const U8* datap, S32 size)
:	mPacketID(packet_id),
	mStatus(status),
	mDatap(NULL),
	mSize(size)
{
	if (size == 0)
	{
		return;
	}

	mDatap = new U8[size];
	if (mDatap != NULL)
	{
		memcpy(mDatap, datap, size);
	}
}

LLTransferPacket::~LLTransferPacket()
{
	if (mDatap)
	{
		delete[] mDatap;
		mDatap = NULL;
	}
}

//
// LLTransferTarget implementation
//

LLTransferTarget::LLTransferTarget(LLTransferTargetType type,
								   const LLUUID& transfer_id,
								   LLTransferSourceType source_type)
:	mType(type),
	mSourceType(source_type),
	mID(transfer_id),
	mChannelp(NULL),
	mGotInfo(false),
	mSize(0),
	mLastPacketID(-1)
{
}

LLTransferTarget::~LLTransferTarget()
{
	// No actual cleanup of the transfer is done here, this is purely for
	// memory cleanup. The completionCallback is guaranteed to get called
	// before this happens.
	for (tpm_iter iter = mDelayedPacketMap.begin(),
				  end = mDelayedPacketMap.end();
		 iter != end; ++iter)
	{
		delete iter->second;
	}
	mDelayedPacketMap.clear();
}

// This should never be called directly, the transfer manager is responsible for
// aborting the transfer from the channel.  I might want to rethink this in the
// future, though.
void LLTransferTarget::abortTransfer()
{
	// Send a message up, call the completion callback
	llinfos << "Aborting transfer " << getID() << " from "
			<< mChannelp->getHost() << llendl;
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("TransferAbort");
	msg->nextBlock("TransferInfo");
	msg->addUUID("TransferID", getID());
	msg->addS32("ChannelType", mChannelp->getChannelType());
	msg->sendReliable(mChannelp->getHost());

	completionCallback(LLTS_ABORT);
}

bool LLTransferTarget::addDelayedPacket(S32 packet_id, LLTSCode status,
										U8* datap, S32 size)
{
	constexpr transfer_packet_map::size_type LL_MAX_DELAYED_PACKETS = 100;
	if (mDelayedPacketMap.size() > LL_MAX_DELAYED_PACKETS)
	{
		// too many delayed packets
		return false;
	}

	if (mDelayedPacketMap.find(packet_id) != mDelayedPacketMap.end())
	{
		llwarns << "Packet Id " << packet_id
				<< " ALREADY in delayed packet map !" << llendl;
		llassert(false);
		return true;
	}

	LLTransferPacket* tpp = new LLTransferPacket(packet_id, status, datap,
												 size);
	mDelayedPacketMap[packet_id] = tpp;
	return true;
}

LLTransferTarget* LLTransferTarget::createTarget(LLTransferTargetType type,
												 const LLUUID& id,
												 LLTransferSourceType source_type)
{
	switch (type)
	{
		case LLTTT_FILE:
			return new LLTransferTargetFile(id, source_type);

		case LLTTT_VFILE:
			return new LLTransferTargetVFile(id, source_type);

		default:
			llwarns << "Unknown transfer target type: " << type << llendl;
			return NULL;
	}
}

LLTransferSourceParamsInvItem::LLTransferSourceParamsInvItem()
:	LLTransferSourceParams(LLTST_SIM_INV_ITEM),
	mAssetType(LLAssetType::AT_NONE)
{
}

void LLTransferSourceParamsInvItem::setAgentSession(const LLUUID& agent_id,
													const LLUUID& session_id)
{
	mAgentID = agent_id;
	mSessionID = session_id;
}

void LLTransferSourceParamsInvItem::setInvItem(const LLUUID& owner_id,
											   const LLUUID& task_id,
											   const LLUUID& item_id)
{
	mOwnerID = owner_id;
	mTaskID = task_id;
	mItemID = item_id;
}

void LLTransferSourceParamsInvItem::setAsset(const LLUUID& asset_id,
											 LLAssetType::EType asset_type)
{
	mAssetID = asset_id;
	mAssetType = asset_type;
}

void LLTransferSourceParamsInvItem::packParams(LLDataPacker& dp) const
{
	LL_DEBUGS("Messaging") << "LLTransferSourceParamsInvItem::packParams()"
						   << LL_ENDL;
	dp.packUUID(mAgentID, "AgentID");
	dp.packUUID(mSessionID, "SessionID");
	dp.packUUID(mOwnerID, "OwnerID");
	dp.packUUID(mTaskID, "TaskID");
	dp.packUUID(mItemID, "ItemID");
	dp.packUUID(mAssetID, "AssetID");
	dp.packS32(mAssetType, "AssetType");
}

bool LLTransferSourceParamsInvItem::unpackParams(LLDataPacker& dp)
{
	S32 tmp_at;

	dp.unpackUUID(mAgentID, "AgentID");
	dp.unpackUUID(mSessionID, "SessionID");
	dp.unpackUUID(mOwnerID, "OwnerID");
	dp.unpackUUID(mTaskID, "TaskID");
	dp.unpackUUID(mItemID, "ItemID");
	dp.unpackUUID(mAssetID, "AssetID");
	dp.unpackS32(tmp_at, "AssetType");

	mAssetType = (LLAssetType::EType)tmp_at;

	return true;
}

LLTransferSourceParamsEstate::LLTransferSourceParamsEstate()
:	LLTransferSourceParams(LLTST_SIM_ESTATE),
	mEstateAssetType(ET_NONE),
	mAssetType(LLAssetType::AT_NONE)
{
}

void LLTransferSourceParamsEstate::setAgentSession(const LLUUID& agent_id,
												   const LLUUID& session_id)
{
	mAgentID = agent_id;
	mSessionID = session_id;
}

void LLTransferSourceParamsEstate::setEstateAssetType(EstateAssetType etype)
{
	mEstateAssetType = etype;
}

void LLTransferSourceParamsEstate::setAsset(const LLUUID& asset_id,
											LLAssetType::EType asset_type)
{
	mAssetID = asset_id;
	mAssetType = asset_type;
}

void LLTransferSourceParamsEstate::packParams(LLDataPacker& dp) const
{
	dp.packUUID(mAgentID, "AgentID");
	// *NOTE: We do not want to pass the session id from the server to the
	// client, but I am not sure if anyone expects this value to be set on the
	// client.
	dp.packUUID(mSessionID, "SessionID");
	dp.packS32(mEstateAssetType, "EstateAssetType");
}

bool LLTransferSourceParamsEstate::unpackParams(LLDataPacker& dp)
{
	S32 tmp_et;

	dp.unpackUUID(mAgentID, "AgentID");
	dp.unpackUUID(mSessionID, "SessionID");
	dp.unpackS32(tmp_et, "EstateAssetType");

	mEstateAssetType = (EstateAssetType)tmp_et;

	return true;
}
