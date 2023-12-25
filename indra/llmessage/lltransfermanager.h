/** 
 * @file lltransfermanager.h
 * @brief Improved transfer mechanism for moving data through the
 * message system.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLTRANSFERMANAGER_H
#define LL_LLTRANSFERMANAGER_H

#include <map>
#include <list>

#include "llhost.h"
#include "lluuid.h"
#include "llthrottle.h"
#include "llpreprocessor.h"
#include "llpriqueuemap.h"
#include "llassettype.h"

//
// Definition of the manager class for the new LLXfer replacement.
// Provides prioritized, bandwidth-throttled transport of arbitrary
// binary data between host/circuit combos
//

typedef enum e_transfer_channel_type
{
	LLTCT_UNKNOWN = 0,
	LLTCT_MISC,
	LLTCT_ASSET,
	LLTCT_NUM_TYPES
} LLTransferChannelType;

typedef enum e_transfer_source_type
{
	LLTST_UNKNOWN = 0,
	LLTST_FILE,
	LLTST_ASSET,
	LLTST_SIM_INV_ITEM,	// Simulator specific, may not be handled
	LLTST_SIM_ESTATE,	// Simulator specific, may not be handled
	LLTST_NUM_TYPES
} LLTransferSourceType;

typedef enum e_transfer_target_type
{
	LLTTT_UNKNOWN = 0,
	LLTTT_FILE,
	LLTTT_VFILE,
	LLTTT_NUM_TYPES
} LLTransferTargetType;

// Errors are negative, expected values are positive.
typedef enum e_status_codes
{
	LLTS_OK = 0,
	LLTS_DONE = 1,
	LLTS_SKIP = 2,
	LLTS_ABORT = 3,
	LLTS_ERROR = -1,
	LLTS_UNKNOWN_SOURCE = -2, // Equivalent of a 404
	LLTS_INSUFFICIENT_PERMISSIONS = -3	// Not enough permissions
} LLTSCode;

// Types of requests for estate wide information
typedef enum e_estate_type
{
	ET_Covenant = 0,
	ET_NONE = -1
} EstateAssetType;

class LLDataPacker;
class LLMessageSystem;
class LLTransferConnection;
class LLTransferSourceChannel;
class LLTransferTargetChannel;
class LLTransferSourceParams;
class LLTransferTargetParams;
class LLTransferSource;
class LLTransferTarget;

class LLTransferManager
{
protected:
	LOG_CLASS(LLTransferManager);

public:
	LLTransferManager();
	virtual ~LLTransferManager();

	void init();
	void cleanup();

	// Called per frame to push packets out on the various different channels.
	void updateTransfers();

	void cleanupConnection(const LLHost& host);

	LLTransferSourceChannel* getSourceChannel(const LLHost& host,
											  LLTransferChannelType stype);
	LLTransferTargetChannel* getTargetChannel(const LLHost& host,
											  LLTransferChannelType stype);

	LLTransferSource* findTransferSource(const LLUUID& transfer_id);

	LL_INLINE bool isValid() const							{ return mValid; }

	static void processTransferRequest(LLMessageSystem* mesgsys, void**);
	static void processTransferInfo(LLMessageSystem* mesgsys, void**);
	static void processTransferPacket(LLMessageSystem* mesgsys, void**);
	static void processTransferAbort(LLMessageSystem* mesgsys, void**);

	static void reliablePacketCallback(void**, S32 result);

	LL_INLINE S32 getTransferBitsIn(LLTransferChannelType tctype) const
	{
		return mTransferBitsIn[tctype];
	}

	LL_INLINE S32 getTransferBitsOut(LLTransferChannelType tctype) const
	{
		return mTransferBitsOut[tctype];
	}

	LL_INLINE void resetTransferBitsIn(LLTransferChannelType tctype)
	{
		mTransferBitsIn[tctype] = 0;
	}

	LL_INLINE void resetTransferBitsOut(LLTransferChannelType tctype)
	{
		mTransferBitsOut[tctype] = 0;
	}

	LL_INLINE void addTransferBitsIn(LLTransferChannelType tctype, S32 bits)
	{
		mTransferBitsIn[tctype] += bits;
	}

	LL_INLINE void addTransferBitsOut(LLTransferChannelType tctype, S32 bits)
	{
		mTransferBitsOut[tctype] += bits;
	}

protected:
	LLTransferConnection* getTransferConnection(const LLHost& host);

protected:
	// Convenient typedefs
	typedef std::map<LLHost, LLTransferConnection*> host_tc_map;
	// We keep a map between each host and LLTransferConnection.
	host_tc_map	mTransferConnections;

	LLHost		mHost;

	S32			mTransferBitsIn[LLTTT_NUM_TYPES];
	S32			mTransferBitsOut[LLTTT_NUM_TYPES];

	bool		mValid;
};

//
// Keeps tracks of all channels to/from a particular host.
//
class LLTransferConnection
{
	friend class LLTransferManager;

protected:
	LOG_CLASS(LLTransferConnection);

public:
	LLTransferConnection(const LLHost& host);
	virtual ~LLTransferConnection();

	void updateTransfers();

	LLTransferSourceChannel* getSourceChannel(LLTransferChannelType type);
	LLTransferTargetChannel* getTargetChannel(LLTransferChannelType type);

	// Convenient typedefs
	typedef std::list<LLTransferSourceChannel*>::iterator tsc_iter;
	typedef std::list<LLTransferTargetChannel*>::iterator ttc_iter;

protected:
	LLHost								mHost;
	std::list<LLTransferSourceChannel*>	mTransferSourceChannels;
	std::list<LLTransferTargetChannel*>	mTransferTargetChannels;
};

//
// A channel which is pushing data out.
//

class LLTransferSourceChannel
{
protected:
	LOG_CLASS(LLTransferSourceChannel);

public:
	LLTransferSourceChannel(LLTransferChannelType type, const LLHost& host);
	virtual ~LLTransferSourceChannel();

	void updateTransfers();

	void updatePriority(LLTransferSource* tsp, F32 priority);

	void addTransferSource(LLTransferSource* sourcep);
	LLTransferSource* findTransferSource(const LLUUID& transfer_id);
	void deleteTransfer(LLTransferSource* tsp);

	LL_INLINE void setThrottleID(S32 throttle_id)			{ mThrottleID = throttle_id; }

	LL_INLINE LLTransferChannelType getChannelType() const	{ return mChannelType; }
	LL_INLINE LLHost getHost() const						{ return mHost; }

protected:
	typedef std::list<LLTransferSource*>::iterator ts_iter;

	LLTransferChannelType				mChannelType;
	LLHost								mHost;
	LLPriQueueMap<LLTransferSource*>	mTransferSources;

	// The throttle that this source channel should use
	S32									mThrottleID;
};

//
// A channel receiving data from a source.
//
class LLTransferTargetChannel
{
	friend class LLTransferTarget;
	friend class LLTransferManager;

protected:
	LOG_CLASS(LLTransferTargetChannel);

public:
	LLTransferTargetChannel(LLTransferChannelType type, const LLHost& host);
	virtual ~LLTransferTargetChannel();

	void requestTransfer(const LLTransferSourceParams& source_params,
						 const LLTransferTargetParams& target_params,
						 F32 priority);

	LLTransferTarget* findTransferTarget(const LLUUID& transfer_id);
	void deleteTransfer(LLTransferTarget* ttp);

	LL_INLINE LLTransferChannelType	getChannelType() const	{ return mChannelType; }
	LL_INLINE LLHost getHost() const						{ return mHost; }

protected:
	void sendTransferRequest(LLTransferTarget* targetp,
							 const LLTransferSourceParams& params,
							 F32 priority);

	void addTransferTarget(LLTransferTarget* targetp);

protected:
	typedef std::list<LLTransferTarget*>::iterator tt_iter;

	LLTransferChannelType			mChannelType;
	LLHost							mHost;
	std::list<LLTransferTarget*>	mTransferTargets;
};

class LLTransferSourceParams
{
protected:
	LOG_CLASS(LLTransferSourceParams);

public:
	LLTransferSourceParams(LLTransferSourceType type)
	:	mType(type)
	{
	}

	virtual ~LLTransferSourceParams() = default;

	virtual void packParams(LLDataPacker& dp) const	= 0;
	virtual bool unpackParams(LLDataPacker& dp) = 0;

	LL_INLINE LLTransferSourceType getType() const			{ return mType; }
	
protected:
	LLTransferSourceType mType;
};

//
// LLTransferSource is an interface, all transfer sources should be derived
// from it.
//
typedef LLTransferSource* (*LLTransferSourceCreateFunc)(const LLUUID& id,
														F32 priority);

class LLTransferSource
{
protected:
	LOG_CLASS(LLTransferSource);

public:

	LL_INLINE LLUUID getID()								{ return mID; }

	friend class LLTransferManager;
	friend class LLTransferSourceChannel;

protected:
	LLTransferSource(LLTransferSourceType source_type,
					 const LLUUID& request_id, F32 priority);
	virtual ~LLTransferSource() = default;

	// When you have figured out your transfer status, do this
	void sendTransferStatus(LLTSCode status);

	virtual void initTransfer() = 0;
	virtual F32 updatePriority() = 0;
	virtual LLTSCode dataCallback(S32 packet_id, S32 max_bytes,
								  U8** datap, S32& returned_bytes,
								  bool& delete_returned) = 0;

	// The completionCallback is GUARANTEED to be called before the destructor.
	virtual void completionCallback(LLTSCode status) = 0;

	virtual void packParams(LLDataPacker& dp) const = 0;
	virtual bool unpackParams(LLDataPacker& dp) = 0;

	LL_INLINE virtual S32	getNextPacketID()				{ return mLastPacketID + 1; }
	LL_INLINE virtual void	setLastPacketID(S32 id)			{ mLastPacketID = id; }

	// For now, no self-induced priority changes
	LL_INLINE F32	getPriority()							{ return mPriority; }
	LL_INLINE void	setPriority(F32 pri)					{ mPriority = pri; }

	// DON'T USE THIS ONE, used internally by LLTransferManager
	virtual void abortTransfer();

	static LLTransferSource* createSource(LLTransferSourceType stype,
										  const LLUUID& request_id,
										  F32 priority);

	static void registerSourceType(LLTransferSourceType stype,
								   LLTransferSourceCreateFunc);

	static void sSetPriority(LLTransferSource*& tsp, F32 priority);
	static F32	sGetPriority(LLTransferSource*& tsp);

protected:
	typedef std::map<LLTransferSourceType,
					 LLTransferSourceCreateFunc> stype_scfunc_map;
	static stype_scfunc_map		sSourceCreateMap;

	LLTransferSourceType		mType;
	LLUUID						mID;
	LLTransferSourceChannel*	mChannelp;
	F32							mPriority;
	S32							mSize;
	S32							mLastPacketID;
};

class LLTransferTargetParams
{
protected:
	LOG_CLASS(LLTransferTargetParams);

public:
	LLTransferTargetParams(LLTransferTargetType type)
	:	mType(type)
	{
	}

	LL_INLINE LLTransferTargetType getType() const			{ return mType; }

protected:
	LLTransferTargetType mType;
};

class LLTransferPacket
{
	// Used for storing a packet that's being delivered later because it's out
	// of order. ONLY should be accessed by the following two classes, for now.
	friend class LLTransferTarget;
	friend class LLTransferManager;

protected:
	LLTransferPacket(S32 packet_id, LLTSCode status, const U8* data, S32 size);
	virtual ~LLTransferPacket();

protected:
	S32			mPacketID;
	LLTSCode	mStatus;
	U8*			mDatap;
	S32			mSize;
};

class LLTransferTarget
{
	friend class LLTransferManager;
	friend class LLTransferTargetChannel;

protected:
	LOG_CLASS(LLTransferTarget);

public:
	LLTransferTarget(LLTransferTargetType target_type,
					 const LLUUID& transfer_id,
					 LLTransferSourceType source_type);
	virtual ~LLTransferTarget();

	// Accessors
	LL_INLINE LLUUID getID() const							{ return mID; }
	LL_INLINE LLTransferTargetType getType() const			{ return mType; }
	LL_INLINE LLTransferTargetChannel* getChannel() const	{ return mChannelp; }
	LL_INLINE LLTransferSourceType getSourceType() const	{ return mSourceType; }

	// Static functionality
	static LLTransferTarget* createTarget(LLTransferTargetType target_type,
										  const LLUUID& request_id,
										  LLTransferSourceType source_type);

protected:
	// Implementation
	virtual bool unpackParams(LLDataPacker& dp) = 0;
	virtual void applyParams(const LLTransferTargetParams& params) = 0;
	virtual LLTSCode dataCallback(S32 packet_id, U8* in_datap,
								  S32 in_size) = 0;

	// The completionCallback is GUARANTEED to be called before the destructor,
	// so all handling of errors/aborts should be done here.
	virtual void completionCallback(LLTSCode status) = 0;

	void abortTransfer();

	LL_INLINE virtual S32 getNextPacketID()					{ return mLastPacketID + 1; }
	LL_INLINE virtual void setLastPacketID(S32 id)			{ mLastPacketID =id; }
	LL_INLINE void setSize(S32 size)						{ mSize = size; }
	LL_INLINE void setGotInfo(bool got_info)				{ mGotInfo = got_info; }
	LL_INLINE bool gotInfo() const							{ return mGotInfo; }

	bool addDelayedPacket(S32 packet_id, LLTSCode status, U8* datap, S32 size);

protected:
	typedef std::map<S32, LLTransferPacket*> transfer_packet_map;
	typedef std::map<S32, LLTransferPacket*>::iterator tpm_iter;

	LLTransferTargetType		mType;
	LLTransferSourceType		mSourceType;
	LLUUID						mID;
	LLTransferTargetChannel*	mChannelp;
	S32							mSize;
	S32							mLastPacketID;
	bool						mGotInfo;

	// Packets that are waiting because of missing/out of order issues
	transfer_packet_map		mDelayedPacketMap;
};

// Hack, here so it's publicly available even though LLTransferSourceInvItem is
// only available on the simulator
class LLTransferSourceParamsInvItem: public LLTransferSourceParams
{
protected:
	LOG_CLASS(LLTransferSourceParamsInvItem);

public:
	LLTransferSourceParamsInvItem();

	void packParams(LLDataPacker& dp) const override;
	bool unpackParams(LLDataPacker& dp) override;

	void setAgentSession(const LLUUID& agent_id, const LLUUID& session_id);
	void setInvItem(const LLUUID& owner_id, const LLUUID& task_id,
					const LLUUID& item_id);
	void setAsset(const LLUUID& asset_id, LLAssetType::EType at);

	LL_INLINE LLUUID getAgentID() const						{ return mAgentID; }
	LL_INLINE LLUUID getSessionID() const					{ return mSessionID; }
	LL_INLINE LLUUID getOwnerID() const						{ return mOwnerID; }
	LL_INLINE LLUUID getTaskID() const						{ return mTaskID; }
	LL_INLINE LLUUID getItemID() const						{ return mItemID; }
	LL_INLINE LLUUID getAssetID() const						{ return mAssetID; }
	LL_INLINE LLAssetType::EType getAssetType() const		{ return mAssetType; }

protected:
	LLUUID				mAgentID;
	LLUUID				mSessionID;
	LLUUID				mOwnerID;
	LLUUID				mTaskID;
	LLUUID				mItemID;
	LLUUID				mAssetID;
	LLAssetType::EType	mAssetType;
};

// Hack, here so it's publicly available even though LLTransferSourceEstate is
// only available on the simulator
class LLTransferSourceParamsEstate: public LLTransferSourceParams
{
protected:
	LOG_CLASS(LLTransferSourceParamsEstate);

public:
	LLTransferSourceParamsEstate();

	void packParams(LLDataPacker& dp) const override;
	bool unpackParams(LLDataPacker& dp) override;

	void setAgentSession(const LLUUID& agent_id, const LLUUID& session_id);
	void setEstateAssetType(EstateAssetType etype);
	void setAsset(const LLUUID& asset_id, LLAssetType::EType at);

	LL_INLINE LLUUID getAgentID() const						{ return mAgentID; }
	LL_INLINE LLUUID getSessionID() const					{ return mSessionID; }
	LL_INLINE EstateAssetType getEstateAssetType() const	{ return mEstateAssetType; }
	LL_INLINE LLUUID getAssetID() const						{ return mAssetID; }
	LL_INLINE LLAssetType::EType getAssetType() const		{ return mAssetType; }

protected:
	LLUUID				mAgentID;
	LLUUID				mSessionID;
	EstateAssetType		mEstateAssetType;
	// these are set on the sim based on estateinfotype
	LLUUID				mAssetID;
	LLAssetType::EType	mAssetType;
};

extern LLTransferManager gTransferManager;

#endif//LL_LLTRANSFERMANAGER_H
