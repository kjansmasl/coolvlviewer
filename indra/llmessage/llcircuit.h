/**
 * @file llcircuit.h
 * @brief Provides a method for tracking network circuit information
 * for the UDP message system
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

#ifndef LL_LLCIRCUIT_H
#define LL_LLCIRCUIT_H

#include <map>
#include <vector>

#include "llerror.h"
#include "llhost.h"
#include "llpacketack.h"
#include "llpreprocessor.h"
#include "llthrottle.h"
#include "lltimer.h"
#include "lluuid.h"

class LLMessageSystem;
class LLEncodedDatagramService;
class LLSD;

 // Relaxation constant on ping running average
constexpr F32 LL_AVERAGED_PING_ALPHA = 0.2f;
constexpr F32 LL_AVERAGED_PING_MAX = 2000;    // msec
// IW: increased to avoid retransmits when a process is slow
constexpr F32 LL_AVERAGED_PING_MIN =  100;    // msec

// Initial value for the ping delay, or for ping delay for an unknown circuit
constexpr U32 INITIAL_PING_VALUE_MSEC = 1000;

constexpr TPACKETID LL_MAX_OUT_PACKET_ID	= 0x01000000;
constexpr int LL_ERR_CIRCUIT_GONE			= -23017;
constexpr int LL_ERR_TCP_TIMEOUT			= -23016;

// 0 - flags
// [1,4] - packetid
// 5 - data offset (after message name)
constexpr U8 LL_PACKET_ID_SIZE = 6;

constexpr S32 LL_MAX_RESENT_PACKETS_PER_FRAME = 100;
constexpr S32 LL_MAX_ACKED_PACKETS_PER_FRAME = 200;
constexpr F32 LL_COLLECT_ACK_TIME_MAX = 2.f;

class LLCircuitData
{
	friend class LLCircuit;
	friend class LLMessageSystem;
	friend class LLEncodedDatagramService;
	// *HACK: so it has access to setAlive() so it can send a final shutdown
	// message:
	friend void crash_on_spaceserver_timeout(const LLHost& host, void*);

protected:
	LOG_CLASS(LLCircuitData);

public:
	LLCircuitData(const LLHost& host, TPACKETID in_id, F32 heartbeat_interval,
				  F32 circuit_timeout);
	~LLCircuitData();

	S32		resendUnackedPackets(F64 now);
	void	clearDuplicateList(TPACKETID oldest_id);

	// Used for tracking how many resends are being done on a circuit.
	void	dumpResendCountAndReset();

	// Public because stupid message system callbacks uses it.
	void	pingTimerStart();
	void	pingTimerStop(U8 ping_id);
	void	ackReliablePacket(TPACKETID packet_num);

	// remote computer information
	LL_INLINE const LLUUID& getRemoteID() const		{ return mRemoteID; }

	LL_INLINE const LLUUID& getRemoteSessionID() const
	{
		return mRemoteSessionID;
	}

	LL_INLINE void setRemoteID(const LLUUID& id)	{ mRemoteID = id; }

	LL_INLINE void setRemoteSessionID(const LLUUID& id)
	{
		mRemoteSessionID = id;
	}

	LL_INLINE void setTrusted(bool b)				{ mTrusted = b; }

	// The local end point ID is used when establishing a trusted circuit.
	// no matching set function for getLocalEndPointID()
	// mLocalEndPointID should only ever be setup in the LLCircuitData constructor
	const LLUUID& getLocalEndPointID() const		{ return mLocalEndPointID; }

	LL_INLINE U32 getPingDelay() const				{ return mPingDelay; }
	LL_INLINE S32 getPingsInTransit() const			{ return mPingsInTransit; }

	// ACCESSORS
	LL_INLINE bool isAlive() const					{ return mAlive; }
	LL_INLINE bool isBlocked() const				{ return mBlocked; }
	LL_INLINE bool getAllowTimeout() const			{ return mAllowTimeout; }
	F32 getPingDelayAveraged();
	F32 getPingInTransitTime();
	LL_INLINE U32 getPacketsIn() const				{ return mPacketsIn; }
	LL_INLINE S32 getBytesIn() const				{ return mBytesIn; }
	LL_INLINE S32 getBytesOut() const				{ return mBytesOut; }
	LL_INLINE U32 getPacketsOut() const				{ return mPacketsOut; }
	LL_INLINE U32 getPacketsLost() const			{ return mPacketsLost; }
	LL_INLINE TPACKETID getPacketOutID() const		{ return mPacketsOutID; }
	LL_INLINE bool getTrusted() const				{ return mTrusted; }
	F32 getAgeInSeconds() const;
	LL_INLINE S32 getUnackedPacketCount() const		{ return mUnackedPacketCount; }
	LL_INLINE S32 getUnackedPacketBytes() const		{ return mUnackedPacketBytes; }
	LL_INLINE F64 getNextPingSendTime() const		{ return mNextPingSendTime; }
	LL_INLINE U32 getLastPacketGap() const			{ return mLastPacketGap; }
    LL_INLINE LLHost getHost() const				{ return mHost; }
	LL_INLINE F64 getLastPacketInTime() const		{ return mLastPacketInTime;	}

	LL_INLINE LLThrottleGroup& getThrottleGroup()	{ return mThrottles; }

	class less
	{
	public:
		bool operator()(const LLCircuitData* lhs, const LLCircuitData* rhs) const
		{
			if (lhs->getNextPingSendTime() < rhs->getNextPingSendTime())
			{
				return true;
			}
			else if (lhs->getNextPingSendTime() > rhs->getNextPingSendTime())
			{
				return false;
			}
			else return lhs > rhs;
		}
	};

	// Debugging stuff (not necessary for operation)

	// Resets per-period counters if necessary.
	void checkPeriodTime();
	friend std::ostream& operator<<(std::ostream& s, LLCircuitData &circuit);
	void getInfo(LLSD& info) const;

protected:
	TPACKETID nextPacketOutID();
	void setPacketInID(TPACKETID id);
	void checkPacketInID(TPACKETID id, bool receive_resent);
	void setPingDelay(U32 ping);
	// Returns false if the circuit is dead and should be cleaned up
	bool checkCircuitTimeout();

	void addBytesIn(S32 bytes);
	void addBytesOut(S32 bytes);

	LL_INLINE U8 nextPingID()						{ return ++mLastPingID; }

	// Returns false if the circuit is dead and should be cleaned up
	bool updateWatchDogTimers(LLMessageSystem* msgsys);

	void addReliablePacket(S32 mSocket, U8* buf_ptr, S32 buf_len,
						   LLReliablePacketParams* params);
	bool isDuplicateResend(TPACKETID packetnum);

	// Call this method when a reliable message comes in - this will correctly
	// place the packet in the correct list to be acked later.
	// RAack = requested ack
	void collectRAck(TPACKETID packet_num);


	void setTimeoutCallback(void (*callback_func)(const LLHost&, void*),
							void* user_data);

	void setAlive(bool b_alive);
	void setAllowTimeout(bool allow);

protected:
	// Identification for this circuit.
	LLHost			mHost;
	LLUUID			mRemoteID;
	LLUUID			mRemoteSessionID;

	LLThrottleGroup	mThrottles;

	TPACKETID		mWrapID;

	// Current packet IDs of incoming/outgoing packets
	// Used for packet sequencing/packet loss detection.
	TPACKETID		mPacketsOutID;
	TPACKETID		mPacketsInID;
	TPACKETID		mHighestPacketID;

	// Callback and data to run in the case of a circuit timeout.
	// Used primarily to try and reconnect to servers if they crash/die.
	void			(*mTimeoutCallback)(const LLHost&, void*);
	void*			mTimeoutUserData;

	// Is this circuit trusted ?
	bool			mTrusted;
	// Machines can "pause" circuits, forcing them not to be dropped
	bool			mAllowTimeout;
	// Indicates whether a circuit is "alive", i.e. responded to pings
	bool			mAlive;
	// Blocked is true if the circuit is hosed, i.e. far behind on pings
	bool			mBlocked;

	// Not sure what the difference between this and mLastPingSendTime is
	F64				mPingTime;			// Time at which a ping was sent.

	F64				mLastPingSendTime;		// Time we last sent a ping
	F64				mLastPingReceivedTime;	// Time we last received a ping
	F64				mNextPingSendTime;		// Time to try & send the next ping
	S32				mPingsInTransit;		// Number of pings in transit
	U8				mLastPingID;			// ID of the last ping we sent out


	// Used for determining the resend time for reliable resends.
	// Raw ping delay
	U32				mPingDelay;
     // Averaged ping delay (fast attack/slow decay)
	F32				mPingDelayAveraged;

	typedef std::map<TPACKETID, U64> packet_time_map;

	packet_time_map	mPotentialLostPackets;
	packet_time_map	mRecentlyReceivedReliablePackets;
	typedef std::vector<TPACKETID> acks_vec_t;
	acks_vec_t		mAcks;

	// First ack creation time
	F32				mAckCreationTime;

	typedef std::map<TPACKETID, LLReliablePacket*> reliable_map;
	typedef reliable_map::iterator reliable_iter;

	reliable_map	mUnackedPackets;
	reliable_map	mFinalRetryPackets;

	S32				mUnackedPacketCount;
	S32				mUnackedPacketBytes;

	F64				mLastPacketInTime;		// Time of last packet arrival

	LLUUID			mLocalEndPointID;

	// These variables are being used for statistical and debugging purpose
	// ONLY, as far as I can tell.

	U32				mPacketsOut;
	U32				mPacketsIn;
	S32				mPacketsLost;
	S32				mBytesIn;
	S32				mBytesOut;

	F32				mLastPeriodLength;		// seconds
	S32				mBytesInLastPeriod;
	S32				mBytesOutLastPeriod;
	S32				mBytesInThisPeriod;
	S32				mBytesOutThisPeriod;
	F32				mPeakBPSIn;				// bits/s, max of all period bps
	F32				mPeakBPSOut;			// bits/s, max of all period bps
	F64				mPeriodTime;
    // Initialized when circuit created, used to track bandwidth numbers
	LLTimer			mExistenceTimer;

	// Number of resent packets since last spam
	S32				mCurrentResendCount;
	// Gap in sequence number of last packet.
    U32				mLastPacketGap;

	const F32 mHeartbeatInterval;
	const F32 mHeartbeatTimeout;
};

// Actually a singleton class -- the global messagesystem has a single
// LLCircuit member.
class LLCircuit
{
protected:
	LOG_CLASS(LLCircuit);

public:
	LLCircuit(F32 circuit_heartbeat_interval, F32 circuit_timeout);
	~LLCircuit();

	LLCircuitData* findCircuit(const LLHost& host) const;
	bool isCircuitAlive(const LLHost& host) const;

	LLCircuitData* addCircuitData(const LLHost& host, TPACKETID in_id);
	void removeCircuitData(const LLHost& host);

	void updateWatchDogTimers(LLMessageSystem *msgsys);
	void resendUnackedPackets(S32& unacked_list_len, S32& unacked_list_size);

	// This method is called during the message system processAcks() to send
	// out any acks that did not get sent already.
	void sendAcks(F32 collect_time);

	friend std::ostream& operator<<(std::ostream& s, LLCircuit &circuit);
	void getInfo(LLSD& info) const;

	void dumpResends();

	typedef std::map<LLHost, LLCircuitData*> circ_data_map_t;

	/**
	 * @brief This method gets an iterator range starting after key in
	 * the circuit data map.
	 *
	 * @param key The the host before first.
	 * @param first[out] The first matching value after key. This
	 * value will equal end if there are no entries.
	 * @param end[out] The end of the iteration sequence.
	 */
	void getCircuitRange(const LLHost& key, circ_data_map_t::iterator& first,
						 circ_data_map_t::iterator& end);

public:
	// Lists that optimize how many circuits we need to traverse a frame
	// *HACK: this should become protected eventually, but stupid !@$@# message
	// system/circuit classes are jumbling things up.
	circ_data_map_t			mUnackedCircuitMap; // Circuits with unacked data
	circ_data_map_t			mSendAckMap;		// Circuits needing to send ack

protected:
	circ_data_map_t			mCircuitData;

	// Circuits sorted by next ping time
	typedef std::set<LLCircuitData*, LLCircuitData::less> ping_set_t;
	ping_set_t				mPingSet;

	// This variable points to the last circuit data we found to optimize the
	// many, many times we call findCircuit. This may be set in otherwise const
	// methods, so it is declared mutable.
	mutable LLCircuitData*	mLastCircuit;

private:
	const F32				mHeartbeatInterval;
	const F32				mHeartbeatTimeout;
};
#endif
